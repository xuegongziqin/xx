/* deleter/parts/watch.c —— 被 deleter.c 直接 include
   内容：目录变化监听（ReadDirectoryChangesW）+ 监听目录的运行时发现。 */

/* ---------- 监听目录：运行时发现，绝不写死盘符 ----------
   真实比赛路径不确定，所以目录来源只有四种运行期信息：
     ① GetTempPathW()                 —— 双方共同的落盘点
     ② GetWindowsDirectoryW()+Temp    —— 系统临时目录
     ③ 本模块所在目录                  —— 我们自己的注入副本/备份
     ④ 活着的对手进程的映像路径所在目录 —— 对手装在哪只有它自己知道（进程自报）
   每个目录一个监听线程；10 秒重扫一次，出现新目录就再起一个。 */
#define WATCH_MAX_DIRS 8
static WCHAR g_watchDirs[WATCH_MAX_DIRS][MAX_PATH];
static int   g_watchDirCount = 0;

static void WatchAddDir(const WCHAR *dir)
{
    int i;
    if (!dir || !dir[0]) return;
    for (i = 0; i < g_watchDirCount; i++)
        if (_wcsicmp(g_watchDirs[i], dir) == 0) return;
    if (g_watchDirCount >= WATCH_MAX_DIRS) return;
    wcsncpy_s(g_watchDirs[g_watchDirCount], MAX_PATH, dir, _TRUNCATE);
    g_watchDirCount++;
}

static void WatchRefreshDirs(void)
{
    WCHAR buf[MAX_PATH];

    if (GetTempPathW(MAX_PATH, buf)) WatchAddDir(buf);
    if (GetWindowsDirectoryW(buf, MAX_PATH)) {
        if (wcslen(buf) + 5 < MAX_PATH) wcscat_s(buf, MAX_PATH, L"\\Temp");
        WatchAddDir(buf);
    }
    if (GetModuleFileNameW(g_hSelfModule, buf, MAX_PATH)) {
        WCHAR *p = wcsrchr(buf, L'\\');
        if (p) { *(p + 1) = L'\0'; WatchAddDir(buf); }
    }

    /* 对手进程自报的目录（路径不确定，靠进程告诉我们） */
    {
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe;
            memset(&pe, 0, sizeof(pe));
            pe.dwSize = sizeof(pe);
            if (Process32FirstW(hSnap, &pe)) {
                do {
                    WCHAR img[MAX_PATH] = L"";
                    DWORD cch = MAX_PATH;
                    HANDLE hProc;
                    if (g_watchDirCount >= WATCH_MAX_DIRS) break;
                    if (!IsTargetLeaf(pe.szExeFile)) continue;
                    hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
                    if (!hProc) continue;
                    if (QueryFullProcessImageNameW(hProc, 0, img, &cch) && img[0]) {
                        WCHAR *p = wcsrchr(img, L'\\');
                        if (p) { *(p + 1) = L'\0'; WatchAddDir(img); }
                    }
                    CloseHandle(hProc);
                } while (Process32NextW(hSnap, &pe));
            }
            CloseHandle(hSnap);
        }
    }
}
/* 单目录监听线程：lp 是堆上的目录路径（线程自己释放） */
static DWORD WINAPI DirWatchOne(LPVOID lp)
{
    WCHAR dir[MAX_PATH] = L"";
    HANDLE hDir, hEvent;
    OVERLAPPED ov;
    BYTE *buf;
    DWORD filter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                   FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE |
                   FILE_NOTIFY_CHANGE_CREATION;

    if (!lp) return 0;
    wcsncpy_s(dir, MAX_PATH, (const WCHAR *)lp, _TRUNCATE);
    free(lp);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

    hDir = CreateFileW(dir, FILE_LIST_DIRECTORY,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, OPEN_EXISTING,
                       FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
    if (hDir == INVALID_HANDLE_VALUE) {
        DelLog(L"监听 %ls 失败 err=%lu（该目录跳过）", dir, GetLastError());
        return 0;
    }
    buf = (BYTE *)malloc(WATCH_BUF_SIZE);
    hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!buf || !hEvent) {
        if (buf) free(buf);
        if (hEvent) CloseHandle(hEvent);
        CloseHandle(hDir);
        return 0;
    }
    memset(&ov, 0, sizeof(ov));
    ov.hEvent = hEvent;
    DelLog(L"目录监听已启动: %ls（毫秒级，落地即删）", dir);

    while (!g_bExit) {
        DWORD transferred = 0, offset = 0;

        if (!ReadDirectoryChangesW(hDir, buf, WATCH_BUF_SIZE, TRUE, filter, NULL, &ov, NULL)) {
            DelLog(L"ReadDirectoryChangesW 失败 %ls err=%lu，2 秒后重来", dir, GetLastError());
            Sleep(2000);
            continue;
        }
        if (WaitForSingleObject(hEvent, 1000) != WAIT_OBJECT_0) {
            CancelIo(hDir);
            ResetEvent(hEvent);
            continue;
        }
        if (!GetOverlappedResult(hDir, &ov, &transferred, FALSE)) {
            ResetEvent(hEvent);
            continue;
        }
        if (transferred == 0) {
            DelLog(L"监听缓冲区溢出 %ls（本轮事件丢弃，轮询会补上）", dir);
            ResetEvent(hEvent);
            continue;
        }

        for (;;) {
            FILE_NOTIFY_INFORMATION *fni = (FILE_NOTIFY_INFORMATION *)(buf + offset);
            if (fni->Action == FILE_ACTION_ADDED || fni->Action == FILE_ACTION_RENAMED_NEW_NAME) {
                int n = (int)(fni->FileNameLength / sizeof(WCHAR));
                if (n > 0 && n < 256) {
                    WCHAR full[MAX_PATH];
                    WCHAR leaf[256];
                    const WCHAR *p;
                    memcpy(leaf, fni->FileName, (size_t)n * sizeof(WCHAR));
                    leaf[n] = L'\0';
                    _snwprintf(full, MAX_PATH - 1, L"%ls%ls", dir, leaf);
                    full[MAX_PATH - 1] = L'\0';
                    p = wcsrchr(leaf, L'\\');
                    p = p ? p + 1 : leaf;

                    if (IsTargetLeaf(p)) {
                        DelLog(L"[WATCH] 落地即删: %ls", full);
                        if (_wcsicmp(p, L"yy.exe") == 0) KillProcessesOfImage(full);
                        ForceDeleteFile(full);
                    } else if (WatchInterestingExt(p)) {
                        FuzzyHuntFile(full, p);
                    }
                }
            }
            if (fni->NextEntryOffset == 0) break;
            offset += fni->NextEntryOffset;
            if (offset >= transferred) break;
        }
        ResetEvent(hEvent);
    }

    CancelIo(hDir);
    CloseHandle(hEvent);
    free(buf);
    CloseHandle(hDir);
    return 0;
}

/* 管理线程：10 秒刷新一次目录集合，新目录各起一个监听线程 */
static DWORD WINAPI WatchMgrThread(LPVOID lp)
{
    int started = 0;
    UNREFERENCED_PARAMETER(lp);
    Sleep(300);
    for (;;) {
        int i;
        if (g_bExit) break;
        WatchRefreshDirs();
        for (i = started; i < g_watchDirCount; i++) {
            WCHAR *copy = (WCHAR *)malloc(MAX_PATH * sizeof(WCHAR));
            if (!copy) break;
            wcsncpy_s(copy, MAX_PATH, g_watchDirs[i], _TRUNCATE);
            if (!CreateThread(NULL, 0, DirWatchOne, copy, 0, NULL)) free(copy);
        }
        if (g_watchDirCount > started) started = g_watchDirCount;
        Sleep(10000);
    }
    return 0;
}

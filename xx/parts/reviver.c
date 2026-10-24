/* xx/parts/reviver.c —— 被 xx.c 直接 include：复活器（参数解析 / 监视 / 拉起） */

static void ParseReviveArgs(void)
{
    const WCHAR *cmd = GetCommandLineW();
    const WCHAR *p;

    if (!cmd) return;

    p = wcsstr(cmd, L"--revive");
    if (p) {
        p += 8;
        while (*p == L' ' || *p == L'\t' || *p == L'=') p++;
        if (*p >= L'0' && *p <= L'9') {
            g_dwRevivePid = (DWORD)wcstoul(p, NULL, 10);
            g_bReviveMode = TRUE;
        }
    }

    p = wcsstr(cmd, L"--exe");
    if (p) {
        p += 5;
        while (*p == L' ' || *p == L'\t' || *p == L'=') p++;
        if (*p == L'"') {
            const WCHAR *q = p + 1;
            const WCHAR *e = wcschr(q, L'"');
            if (e && (e - q) < (int)(_countof(g_szReviveExe) - 1)) {
                wcsncpy_s(g_szReviveExe, _countof(g_szReviveExe), q, (size_t)(e - q));
            }
        } else {
            const WCHAR *e = p;
            while (*e && *e != L' ') e++;
            if ((e - p) < (int)(_countof(g_szReviveExe) - 1))
                wcsncpy_s(g_szReviveExe, _countof(g_szReviveExe), p, (size_t)(e - p));
        }
    }

    p = wcsstr(cmd, L"--token");
    if (p) {
        ULONGLONG tok;
        p += 7;
        while (*p == L' ' || *p == L'\t' || *p == L'=') p++;
        tok = wcstoull(p, NULL, 16);
        if (tok) g_dwInheritedToken = tok | 1ULL;
    }

    if (g_bReviveMode)
        WriteLog(L"复活器参数: boss=%lu exe=%ls token=%ls", g_dwRevivePid,
                 g_szReviveExe[0] ? g_szReviveExe : L"(未给)",
                 g_dwInheritedToken ? L"已继承" : L"无(将跑全新实例)");
    else if (g_dwInheritedToken)
        WriteLog(L"本实例继承了上一轮的会话 token（接着上一轮继续工作）");
}

/* 进程表里有没有活着的 xx.exe（排除复活器自己） */
static BOOL AnyXxExeRunning(void)
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe = { sizeof(pe) };
    BOOL found = FALSE;

    if (hSnap == INVALID_HANDLE_VALUE) return FALSE;
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (pe.th32ProcessID == GetCurrentProcessId()) continue;
            if (_wcsicmp(pe.szExeFile, L"xx.exe") == 0) { found = TRUE; break; }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return found;
}

/* 共享内存里记着的 xx 路径（复活器在 %TEMP% 跑，靠它找真身；没有就用命令行给的） */
static void GetReviveTargetPath(WCHAR *out, SIZE_T cch)
{
    out[0] = L'\0';
    if (g_szReviveExe[0]) { wcsncpy_s(out, cch, g_szReviveExe, _TRUNCATE); return; }
    if (!g_pSharedView) {
        HANDLE h = XXOpenShared();
        if (h) {
            g_pSharedView = MapViewOfFile(h, FILE_MAP_READ, 0, 0, SHARED_MEM_SIZE);
            CloseHandle(h);        /* 视图独立有效；只读够用 */
        }
    }
    if (g_pSharedView) {
        const WCHAR *p = XX_XXPATH(g_pSharedView);
        if (p[0] && XXPathLooksLikeOurXx(p)) wcsncpy_s(out, cch, p, _TRUNCATE);
    }
}

/* ================== 结束后删除（300ms × 10 重试） ==================
   进程刚死时，系统不一定立刻释放它镜像文件的句柄（映像段/内存管理器延迟回收），
   所以"结束后立即删除"经常第一次就失败。这里统一按用户要求：
   **每 300ms 重试一次，总共 10 次**（约 3 秒），每次把结果写日志。 */
static BOOL DeleteFileWithRetry(const WCHAR *path, const WCHAR *tag)
{
    int i;
    if (!path || !path[0]) return FALSE;
    for (i = 0; i < 10; i++) {
        if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) {
            WriteLog(L"%ls: %ls 已不存在（无需删除）", tag, path);
            return TRUE;
        }
        SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);
        if (DeleteFileW(path)) {
            WriteLog(L"%ls: 删除成功（第 %d 次）: %ls", tag, i + 1, path);
            return TRUE;
        }
        WriteLog(L"%ls: 删除失败 err=%lu（第 %d/10 次），300ms 后重试: %ls",
                 tag, GetLastError(), i + 1, path);
        Sleep(300);
    }
    WriteLog(L"%ls: 10 次都没删掉（映像仍被占用？）: %ls", tag, path);
    return FALSE;
}

/* 删除线程版：不阻塞调用方（用于清理 %TEMP% 里的旧副本） */
static DWORD WINAPI DeleteThreadProc(LPVOID lp)
{
    WCHAR *path = (WCHAR *)lp;
    DeleteFileWithRetry(path, L"清理旧副本");
    free(path);
    return 0;
}

static void DeleteInThread(const WCHAR *path)
{
    WCHAR *copy;
    SIZE_T cb;
    if (!path || !path[0]) return;
    cb = (wcslen(path) + 1) * sizeof(WCHAR);
    copy = (WCHAR *)malloc(cb);
    if (!copy) return;
    wcscpy_s(copy, cb / sizeof(WCHAR), path);
    if (!CreateThread(NULL, 0, DeleteThreadProc, copy, 0, NULL)) free(copy);
}
/* 复活器主循环：等本体结束/心跳停摆 → 拉起新实例（带 token 继续，或全新） → 自己退出 */
static void RunReviver(void)
{
    HANDLE hBoss = NULL;
    WCHAR exePath[MAX_PATH];
    WCHAR cmdLine[MAX_PATH * 2];
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD waited = 0;

    if (g_dwRevivePid)
        hBoss = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, g_dwRevivePid);

    WriteLog(L"复活器：开始监视 xx.exe(pid=%lu)%ls", g_dwRevivePid,
             hBoss ? L"" : L"（打不开句柄，改为只看进程表/心跳）");

    for (;;) {
        BOOL bossGone = FALSE;
        if (hBoss) {
            if (WaitForSingleObject(hBoss, 200) == WAIT_OBJECT_0) { bossGone = TRUE; }
        } else {
            Sleep(200);
            bossGone = (g_dwRevivePid && !AnyXxExeRunning()) ? TRUE : FALSE;
        }
        if (!bossGone && !AnyXxExeRunning()) bossGone = TRUE;
        if (bossGone) { WriteLog(L"复活器：xx.exe 已结束"); break; }

        /* 本体可能活着但心跳停摆（被挂起/弹窗被卸）：也复活一次 */
        if (g_pSharedView && XX_LAYOUT(g_pSharedView) == XX_LAYOUT_VERSION) {
            static LONG s_last = -1000000;
            LONG cur = XX_HB_TICK(g_pSharedView);
            if (cur == s_last) {
                waited += 200;
                if (waited >= HEARTBEAT_STALE_MS + 2000) {
                    WriteLog(L"复活器：心跳停摆 %lu ms，判定本体已僵死", (unsigned long)waited);
                    break;
                }
            } else { s_last = cur; waited = 0; }
        }
        Sleep(0);
    }
    if (hBoss) CloseHandle(hBoss);

    GetReviveTargetPath(exePath, MAX_PATH);
    if (!exePath[0]) {
        WriteLog(L"复活器：拿不到 xx.exe 路径 -> 无法复活（本次放弃）");
        ExitProcess(0);
    }

    /* **不删旧映像，直接用现成的**：
       程序目录里的 xx.exe 已被 defense 的"只读 + deny-DELETE"装甲护住 ——
       那既挡住了对手的 DeleteFileW，也会挡住我们自己；再去做"删了再还原"纯属自找麻烦
       （还会多打十几行失败日志、把复活推迟 3 秒）。
       所以复活器直接拉起现成的 xx.exe；只有当它真的**不在了**（被对手删掉、被我们清过）
       才用复活器自己的镜像补一份回来。 */
    if (GetFileAttributesW(exePath) == INVALID_FILE_ATTRIBUTES) {
        WCHAR self[MAX_PATH] = L"";
        WriteLog(L"复活器：%ls 不在了，用自身副本补一份", exePath);
        if (GetModuleFileNameW(NULL, self, MAX_PATH) && CopyFileW(self, exePath, FALSE))
            WriteLog(L"复活器：已从自身副本还原 %ls", exePath);
        else
            WriteLog(L"复活器：还原 %ls 失败 err=%lu", exePath, GetLastError());
    }
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNORMAL;   /* SW_HIDE 会让新实例的第一次 ShowWindow 失效 */
    memset(&pi, 0, sizeof(pi));

    if (g_dwInheritedToken) {
        swprintf_s(cmdLine, _countof(cmdLine), L"\"%ls\" --token %016llX", exePath,
                   (unsigned long long)g_dwInheritedToken);
        WriteLog(L"复活器：带上上一轮 token 复活 -> %ls", exePath);
    } else {
        swprintf_s(cmdLine, _countof(cmdLine), L"\"%ls\"", exePath);
        WriteLog(L"复活器：没有可用 token -> 拉起**全新**实例 -> %ls", exePath);
    }

    {
        DWORD newPid = 0;
        BOOL ok = FALSE;

        /* 带 token 时必须用 lpCommandLine（XXLaunchInConsoleSession 只接收 exePath，不能带参数） */
        if (g_dwInheritedToken) {
            if (CreateProcessW(exePath, cmdLine, NULL, NULL, FALSE,
                               CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
                ok = TRUE;
                newPid = pi.dwProcessId;
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            } else {
                WriteLog(L"复活器：带 token 拉起失败 err=%lu -> 退回不带 token（会变成全新会话）",
                         GetLastError());
            }
        }
        if (!ok) ok = XXLaunchInConsoleSession(exePath, FALSE, &newPid);
        if (ok) WriteLog(L"复活器：已拉起新实例 pid=%lu（本复活器退出）", newPid);
        else    WriteLog(L"复活器：两种拉起方式都失败 err=%lu", GetLastError());
    }
    ExitProcess(0);
}

/* 由本体调用：把自己复制到 %TEMP% 改名，并以 --revive 启动（**只复活，不接管**） */
static void SpawnReviver(void)
{
    WCHAR self[MAX_PATH], tmpDir[MAX_PATH], target[MAX_PATH], cmd[MAX_PATH * 3];
    const WCHAR *leaf;
    ULONGLONG rnd = 0;
    DWORD nNames, pick, tries;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    BOOL bOk = FALSE;

    if (g_bReviveMode) return;
    if (!GetModuleFileNameW(NULL, self, MAX_PATH)) return;
    leaf = wcsrchr(self, L'\\');
    leaf = leaf ? leaf + 1 : self;
    if (_wcsicmp(leaf, L"xx.exe") != 0) return;   /* 只有本体负责布置复活器 */
    if (!GetTempPathW(MAX_PATH, tmpDir)) return;

    nNames = (DWORD)(_countof(g_reviverNames));
    pick = GenRandom64(&rnd) ? (DWORD)(rnd % nNames) : (GetTickCount() % nNames);

    for (tries = 0; tries < nNames && !bOk; tries++) {
        swprintf_s(target, MAX_PATH, L"%ls%ls", tmpDir, g_reviverNames[(pick + tries) % nNames]);
        if (CopyFileW(self, target, FALSE)) { bOk = TRUE; break; }
        DeleteFileW(target);
        if (CopyFileW(self, target, FALSE)) { bOk = TRUE; break; }
    }
    if (!bOk) { WriteLog(L"布置复活器失败：三个候选名都复制不了"); return; }
    /* 顺手清理 %TEMP% 里**其它**旧副本（内存映像句柄可能还没释放，交给重试线程做） */
    {
        int k;
        WCHAR old[MAX_PATH];
        for (k = 0; k < (int)(_countof(g_reviverNames)); k++) {
            swprintf_s(old, MAX_PATH, L"%ls%ls", tmpDir, g_reviverNames[k]);
            if (_wcsicmp(old, target) == 0) continue;          /* 别删刚布置的这份 */
            if (GetFileAttributesW(old) != INVALID_FILE_ATTRIBUTES) DeleteInThread(old);
        }
    }
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNORMAL;   /* SW_HIDE 会让新实例的第一次 ShowWindow 失效 */
    memset(&pi, 0, sizeof(pi));

    swprintf_s(cmd, _countof(cmd), L"\"%ls\" --revive %lu --exe \"%ls\" --token %016llX",
               target, (unsigned long)GetCurrentProcessId(), self,
               (unsigned long long)NameTok());
    if (CreateProcessW(target, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WriteLog(L"已布置复活器: %ls (pid=%lu)", target, (unsigned long)pi.dwProcessId);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        WriteLog(L"布置复活器失败(启动): %ls err=%lu", target, GetLastError());
    }
}

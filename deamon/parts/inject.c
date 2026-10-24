/* deamon/parts/inject.c —— 被 deamon.c 直接 include：导出 RVA / 远程注入 / 引导注入 */

static BOOL __attribute__((unused)) GetRemoteExportRva(LPCWSTR localDllPath, const char *funcName, DWORD *pRva)
{
    HMODULE hMap;
    FARPROC pfn;
    DWORD rva;

    hMap = LoadLibraryExW(localDllPath, NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (!hMap) return FALSE;
    pfn = GetProcAddress(hMap, funcName);
    if (!pfn) { FreeLibrary(hMap); return FALSE; }
    rva = (DWORD)((ULONG_PTR)pfn - (ULONG_PTR)hMap);
    FreeLibrary(hMap);
    *pRva = rva;
    return TRUE;
}

/* **不再被调用**（保留代码备查）：
   它用 remote LoadLibraryW 的返回值当模块基址，而 GetExitCodeThread 只有 32 位，
   64 位基址被截断 → 加上导出 RVA 就跳到 0x????5B50 这种无效地址（(exec) AV）。
   现在各模块自己从共享内存读 XX_NAME_TOKEN64，命名照样一致，不需要远程推送。 */
static BOOL __attribute__((unused)) PushNamesRemote(HANDLE hProc, HMODULE hRemoteMod,
                                                    LPCWSTR localDllPath, DWORD pid)
{
    DWORD rva = 0;
    XX_NAME_BLOCK block, zero;
    LPVOID pRemote;
    HANDLE hThread;
    BOOL ok = FALSE;

    UNREFERENCED_PARAMETER(hRemoteMod);
    if (!hProc || !localDllPath || !localDllPath[0]) return FALSE;
    if (!GetRemoteExportRva(localDllPath, "XxSetNames", &rva)) return FALSE;

    XXNameBlockInit(&block, NameTok(), 0);
    pRemote = VirtualAllocEx(hProc, NULL, sizeof(block), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemote) return FALSE;
    if (!WriteProcessMemory(hProc, pRemote, &block, sizeof(block), NULL)) {
        VirtualFreeEx(hProc, pRemote, 0, MEM_RELEASE);
        return FALSE;
    }
    hThread = CreateRemoteThread(hProc, NULL, 0,
                                 (LPTHREAD_START_ROUTINE)((BYTE *)hRemoteMod + rva),
                                 pRemote, 0, NULL);
    if (hThread) { WaitForSingleObject(hThread, 2000); CloseHandle(hThread); ok = TRUE; }
    memset(&zero, 0, sizeof(zero));
    WriteProcessMemory(hProc, pRemote, &zero, sizeof(zero), NULL);
    VirtualFreeEx(hProc, pRemote, 0, MEM_RELEASE);
    if (!ok) WriteDeamonLog(L"送会话名失败 PID=%lu dll=%ls\n", pid, localDllPath);
    return ok;
}

static BOOL InjectDll(HANDLE hProc, LPCWSTR dllPath, DWORD pid) {
    SIZE_T size = (wcslen(dllPath) + 1) * sizeof(WCHAR);
    LPVOID pMem = VirtualAllocEx(hProc, NULL, size, MEM_COMMIT, PAGE_READWRITE);
    if (!pMem) {
        WriteDeamonLog(L"注入失败 VirtualAllocEx PID=%lu dll=%ls\n", pid, dllPath);
        return FALSE;
    }
    WriteProcessMemory(hProc, pMem, dllPath, size, NULL);
    {
        LPTHREAD_START_ROUTINE pLoadLib = (LPTHREAD_START_ROUTINE)
            GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
        HANDLE hThread = CreateRemoteThread(hProc, NULL, 0, pLoadLib, pMem, 0, NULL);
        BOOL ok = FALSE;
        if (hThread) {
            DWORD code = 0;
            WaitForSingleObject(hThread, 5000);
            /* 远端线程退出码 = 宿主里 LoadLibraryW 的返回值（只有低 32 位，仅用来判断成败） */
            if (GetExitCodeThread(hThread, &code)) ok = (code != 0);
            else ok = TRUE;
            if (!ok)
                WriteDeamonLog(L"注入被目标拒绝: %ls -> PID=%lu（远端 LoadLibrary 返回 0）\n",
                               dllPath, pid);
            CloseHandle(hThread);
        } else {
            WriteDeamonLog(L"注入 DLL 失败 PID=%lu dll=%ls err=%lu\n", pid, dllPath, GetLastError());
        }
        VirtualFreeEx(hProc, pMem, 0, MEM_RELEASE);
        return ok;
    }
}

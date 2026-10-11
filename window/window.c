/*
 * window.dll - 弹窗与日志显示（极速接管 + 自动复活 xx.exe）
 * 编译器：GNU GCC (MinGW-w64)
 * 编译选项：gcc -shared -o window.dll window.c -lgdi32 -luser32 -ladvapi32 -static -s -O2
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>

/* 共享内存布局与命名常量：唯一来源见 xxipc.h */
#include "xxipc.h"

/* 弹窗尺寸：450x325 = 原来的 900x650 的 1/4 面积 */
#define WND_WIDTH           450
#define WND_HEIGHT          325

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

/* 内部函数 */
static BOOL OpenSharedMemory(void);
static BOOL InitHeartbeatEvent(void);
static void SetHeartbeatSignal(void);
static void BumpHeartbeatTick(void);
static BOOL AnyOwnWindowOnDesktop(void);
static void ReadLog(WCHAR *buffer, SIZE_T size);
static DWORD WINAPI WindowMainThread(LPVOID lpParam);
__declspec(dllexport) DWORD WINAPI StartWindow(LPVOID lp);   /* 供 svcguard 远程拉起 */
static void ServeWindow(const WCHAR *szClassName, HINSTANCE hInst);
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void GetXxExePath(WCHAR *path, SIZE_T size);
static BOOL IsXxRunning(void);
static void EnsureXxRunning(void);

/* ================== xx.exe 路径与存活检查 ================== */

/* 从 window.dll 自身路径推导出 xx.exe 的完整路径 */
static void GetXxExePath(WCHAR *path, SIZE_T size)
{
    WCHAR dllPath[MAX_PATH];
    GetModuleFileNameW(GetModuleHandleW(L"window.dll"), dllPath, MAX_PATH);
    WCHAR *pSlash = wcsrchr(dllPath, L'\\');
    if (pSlash) pSlash[1] = L'\0';
    else wcscat_s(dllPath, MAX_PATH, L"\\");

    swprintf_s(path, size, L"%sxx.exe", dllPath);
}

/* 检查 xx.exe 是否已有进程在运行 */
static BOOL IsXxRunning(void)
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return FALSE;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    BOOL bFound = FALSE;
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"xx.exe") == 0) {
                bFound = TRUE;
                break;
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return bFound;
}

/* 如果没有 xx.exe 在运行，则启动它 */
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

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    CreateProcessW(xxPath, NULL, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                   NULL, NULL, &si, &pi);
    if (pi.hProcess) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

/* ================== DllMain ================== */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    UNREFERENCED_PARAMETER(hinstDLL);
    UNREFERENCED_PARAMETER(lpvReserved);
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
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

    if (!g_pSharedView) return FALSE;
    pSuffix = (WCHAR *)((BYTE *)g_pSharedView + OFFSET_HB_SUFFIX);
    if (pSuffix[0] == L'\0') return FALSE;
    if (g_hHeartbeatEvent) return TRUE;

    _snwprintf(szEventName, _countof(szEventName), L"%s%s", HEARTBEAT_PREFIX, pSuffix);
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

/* EnumWindows 只看调用线程所在桌面，因此跨会话的弹窗不会被误判为本会话已有弹窗 */
static BOOL CALLBACK EnumOwnWindowProc(HWND hWnd, LPARAM lp)
{
    WCHAR cls[128] = { 0 };
    UNREFERENCED_PARAMETER(lp);
    if (!IsWindowVisible(hWnd)) return TRUE;
    GetClassNameW(hWnd, cls, _countof(cls));
    if (wcsstr(cls, L"XX_ALIVE_WND_")) {
        InterlockedExchange(&g_bFoundOwnWindow, 1);
        return FALSE;
    }
    return TRUE;
}

static BOOL AnyOwnWindowOnDesktop(void)
{
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
        return TRUE;
    default:
        return FALSE;
    }
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        SetTimer(hWnd, 1, 500, NULL);
        g_bWindowAlive = 1;
        return 0;
    case WM_TIMER:
        if (wParam == 1) {
            DWORD sig;
            SetHeartbeatSignal();
            /* 控制台式滚动输出：只有日志确实变了才重画，不白刷 */
            sig = LogSignature();
            if (sig != g_dwLogSig) {
                g_dwLogSig = sig;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            /* 对手可能用 ShowWindow(SW_HIDE) 让弹窗"消失"，这里每 500ms 自检并拉回 */
            if (g_hWnd && !IsWindowVisible(g_hWnd)) ShowWindow(g_hWnd, SW_SHOW);
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        HFONT hOldFont;
        TEXTMETRICW tm;
        WCHAR *starts[LOG_VIEW_MAXLINES];
        WCHAR logBuffer[2048] = { 0 };
        WCHAR work[2048];
        WCHAR *w, *p;
        int lineH, top, body, shown, nStored = 0, head = 1, nDraw, y;

        GetClientRect(hWnd, &rc);

        {
            HBRUSH hBrush = CreateSolidBrush(RGB(30, 30, 30));
            FillRect(hdc, &rc, hBrush);
            DeleteObject(hBrush);
        }

        hOldFont = (HFONT)SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
        GetTextMetricsW(hdc, &tm);
        lineH = (int)tm.tmHeight + (int)tm.tmExternalLeading + 2;
        if (lineH < 8) lineH = 8;
        SetBkMode(hdc, TRANSPARENT);

        /* 标题行 */
        {
            RECT rcTitle = rc;
            rcTitle.bottom = rcTitle.top + lineH;
            SetTextColor(hdc, RGB(0, 255, 0));
            DrawTextW(hdc, L"XX PROGRAM - ALIVE", -1, &rcTitle,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        /* 日志区：按实际行高算出这个小窗口能放几行 */
        top = rc.top + lineH + 2;
        body = rc.bottom - top;
        shown = (body > 0) ? (body / lineH) : 0;
        if (shown > LOG_VIEW_MAXLINES) shown = LOG_VIEW_MAXLINES;

        ReadLog(logBuffer, _countof(logBuffer));

        /* \r\n → \n */
        wcsncpy_s(work, _countof(work), logBuffer, _TRUNCATE);
        w = work;
        for (p = work; *p; p++) {
            if (*p == L'\r') { *w++ = L'\n'; if (p[1] == L'\n') p++; }
            else *w++ = *p;
        }
        *w = L'\0';

        /* 行首用环形缓冲保存，只留"最后 LOG_VIEW_MAXLINES 行"。
           旧实现记的是**最前面** 128 行的行首，日志一长，画面就永远停在头 128 行
           里，只有共享缓冲写满、最旧字符被丢弃时才跳一次 —— 看起来就是
           "写满了才刷新"。改为从尾部取，新行到来立刻可见。 */
        if (work[0] != L'\0') {
            starts[0] = work;
            nStored = 1;
            for (p = work; *p; p++) {
                if (*p == L'\n') {
                    *p = L'\0';
                    if (p[1] != L'\0') {
                        starts[head] = p + 1;
                        head = (head + 1) % LOG_VIEW_MAXLINES;
                        if (nStored < LOG_VIEW_MAXLINES) nStored++;
                    }
                }
            }
        }

        /* 最新一行贴着底部，像终端一样往上滚 */
        nDraw = (nStored < shown) ? nStored : shown;
        y = rc.bottom - nDraw * lineH;
        if (y < top) y = top;

        SetTextColor(hdc, RGB(200, 200, 200));
        if (nDraw <= 0) {
            RECT r = rc;
            r.top = top;
            DrawTextW(hdc, L"Awaiting log...", -1, &r,
                      DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        } else {
            int i;
            for (i = 0; i < nDraw; i++) {
                int idx = ((head - nDraw + i) % LOG_VIEW_MAXLINES + LOG_VIEW_MAXLINES) % LOG_VIEW_MAXLINES;
                WCHAR *line = starts[idx];
                if (line && line[0]) {
                    RECT r = { rc.left + 4, y, rc.right - 4, y + lineH };
                    DrawTextW(hdc, line, -1, &r,
                              DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
                }
                y += lineH;
            }
        }

        SelectObject(hdc, hOldFont);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_CLOSE:
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_CLOSE)
            return 0;
        return DefWindowProc(hWnd, msg, wParam, lParam);
    /* 外部 PostMessage(WM_DESTROY) 只是一条普通消息，并不会真的销毁窗口；
       真销毁时 DestroyWindow 随后必定补发 WM_NCDESTROY。所以这里一律拒绝，
       收尾动作全部搬到 WM_NCDESTROY —— 堵住"一条消息打掉弹窗"这个洞。 */
    case WM_DESTROY:
        return 0;
    case WM_NCDESTROY:
        /* 外部 PostMessage(WM_NCDESTROY) 同样只是一条普通消息：无条件收尾会把
           活着的窗口当成"已销毁"，外层热备于是又建一个新窗，老窗口就永久泄漏在
           屏幕上。只有我们自己发起的销毁（g_bDestroying），或句柄确实已失效，
           才做收尾。 */
        if (InterlockedCompareExchange(&g_bDestroying, 0, 0) == 0 && IsWindow(hWnd))
            return 0;
        KillTimer(hWnd, 1);
        g_bWindowAlive = 0;
        g_hWnd = NULL;
        PostQuitMessage(0);
        return 0;
    }
    /* 不在白名单里的一律拒绝 */
    return PassThroughMsg(msg) ? DefWindowProc(hWnd, msg, wParam, lParam) : 0;
}

/* 建窗 + 跑消息循环 + 收尾。持有互斥锁与否都能调用（兜底路径不持锁）。 */
static void ServeWindow(const WCHAR *szClassName, HINSTANCE hInst)
{
    MSG msg;

    g_hWnd = CreateWindowExW(0, szClassName, L"XX Alive",
                             WS_OVERLAPPEDWINDOW & ~WS_SYSMENU,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             WND_WIDTH, WND_HEIGHT, NULL, NULL, hInst, NULL);
    if (!g_hWnd) return;

    ShowWindow(g_hWnd, SW_SHOW);
    UpdateWindow(g_hWnd);

    /* 消息循环：拒绝外部投递的 WM_QUIT
       （对手 PostMessage(WM_QUIT) 会让 GetMessage 返回 0，
         循环结束 → DestroyWindow → 弹窗消失 8 秒判负）。
       自身退出时 g_bExitThread 已被 DLL_PROCESS_DETACH 置 1，正常放行。 */
    for (;;) {
        BOOL gm = GetMessage(&msg, NULL, 0, 0);
        if (gm == -1) break;                        /* GetMessage 出错 */
        if (gm == 0) {                              /* 收到 WM_QUIT */
            /* 自身退出，或弹窗确实已被销毁（WM_NCDESTROY 的 PostQuitMessage）
               → 退出循环，交回外层热备循环重建弹窗 */
            if (g_bExitThread || g_hWnd == NULL) break;
            continue;                               /* 外部 WM_QUIT → 拒绝并继续 */
        }
        if (g_bExitThread) break;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (g_hWnd) {
        HWND hw = g_hWnd;
        InterlockedExchange(&g_bDestroying, 1);
        DestroyWindow(hw);            /* 真销毁：内部会补发 WM_NCDESTROY 完成收尾 */
        InterlockedExchange(&g_bDestroying, 0);
    }
    g_hWnd = NULL;
    g_bWindowAlive = 0;
}

static DWORD WINAPI WindowMainThread(LPVOID lpParam)
{
    UNREFERENCED_PARAMETER(lpParam);

    WCHAR *pWndSuffix = NULL;
    WCHAR szClassName[128];

    /* 共享内存与后缀可能比本实例晚就绪（xx.exe 刚被复活、正在重建字段），
       这里一直等下去：模块已经注入到宿主里，退出就再也不会被重新注入。 */
    for (;;) {
        if (g_bExitThread) return 0;
        if (!g_pSharedView) OpenSharedMemory();
        if (g_pSharedView) {
            pWndSuffix = (WCHAR *)((BYTE *)g_pSharedView + OFFSET_WND_SUFFIX);
            if (pWndSuffix[0] != L'\0') break;
        }
        Sleep(200);
    }

    /* 事件对象仅为兼容旧观察方式，失败不影响弹窗 */
    InitHeartbeatEvent();
    BumpHeartbeatTick();

    _snwprintf(szClassName, _countof(szClassName), L"XX_ALIVE_WND_%s", pWndSuffix);
    szClassName[_countof(szClassName) - 1] = L'\0';

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(L"window.dll");
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = szClassName;
    RegisterClassW(&wc);

    /* Local\ 命名空间按登录会话隔离：不需要 SeCreateGlobalPrivilege，
       session 0 里的实例也抢不到交互会话的锁（反之亦然）。 */
    g_hGuardMutex = CreateMutexW(NULL, FALSE, WINDOW_GUARD_MUTEX);
    g_hMakeGuard  = CreateMutexW(NULL, FALSE, WINDOW_MAKE_GUARD_MUTEX);

    // 极速热备循环：几乎无延迟接管
    while (!g_bExitThread) {
        DWORD dwWait;

        if (!g_hGuardMutex) {
            g_hGuardMutex = CreateMutexW(NULL, FALSE, WINDOW_GUARD_MUTEX);
            if (!g_hGuardMutex) Sleep(200);   /* 创建失败：稍后重试，期间走兜底 */
        }

        dwWait = g_hGuardMutex ? WaitForSingleObject(g_hGuardMutex, 0) : WAIT_TIMEOUT;

        if (dwWait == WAIT_OBJECT_0 || dwWait == WAIT_ABANDONED) {
            /* 获得弹窗权（ABANDONED = 前任持有者已死，锁归我们）：确保 xx.exe 存活 */
            EnsureXxRunning();
            ServeWindow(szClassName, wc.hInstance);
            g_dwNoWindowSince = 0;
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
                    g_dwNoWindowSince = 0;
                    /* 本实例没持有窗口（g_bWindowAlive=0），所以直接递增计数器：
                       只要弹窗还在屏幕上，即使持有者被挂起/冻结也不能误判丢失 */
                    BumpHeartbeatTick();
                } else if (g_dwNoWindowSince == 0) {
                    g_dwNoWindowSince = now ? now : 1;
                } else if (now - g_dwNoWindowSince >= WINDOW_FALLBACK_MS) {
                    /* 兜底建窗必须串行化：多个热备实例可能在同一 tick 越过阈值，
                       都看到"没有窗口"就各建一个。抢到建窗锁后再复查一次。 */
                    DWORD wMake = g_hMakeGuard ? WaitForSingleObject(g_hMakeGuard, 2000)
                                               : WAIT_OBJECT_0;
                    BOOL bOwnMake = (wMake == WAIT_OBJECT_0 || wMake == WAIT_ABANDONED);
                    if (bOwnMake || !g_hMakeGuard) {
                        if (!AnyOwnWindowOnDesktop()) {
                            EnsureXxRunning();
                            ServeWindow(szClassName, wc.hInstance);
                        }
                        if (bOwnMake) ReleaseMutex(g_hMakeGuard);
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
#define WINDOW_THREAD_MAX 4

static DWORD WINAPI WindowThreadEntry(LPVOID lp)
{
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

/*
 * window.dll - 弹窗与日志显示（极速接管 + 自动复活 xx.exe）
 * 编译器：GNU GCC (MinGW-w64)
 * 编译选项：gcc -shared -o window.dll window.c -lgdi32 -luser32 -ladvapi32 -static -s -O2
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>

/* 共享内存布局（与 xx.c 严格一致） */
#define SHARED_MEM_NAME     L"Global\\xx_shared_mem"
#define SHARED_MEM_SIZE     4096
#define HEARTBEAT_PREFIX    L"Global\\xx_heartbeat_"
#define WINDOW_GUARD_MUTEX  L"Global\\xx_window_guard"

#define OFFSET_HB_SUFFIX    0
#define OFFSET_WND_SUFFIX   64
#define OFFSET_LOG          256
#define OFFSET_COUNTER      4064

#define SUFFIX_MAX          64
#define LOG_SIZE            3808
#define MAX_SHOWN_LINES     20

/* 全局变量 */
static HANDLE g_hSharedMem = NULL;
static PVOID  g_pSharedView = NULL;
static HANDLE g_hHeartbeatEvent = NULL;
static HANDLE g_hGuardMutex = NULL;
static HWND   g_hWnd = NULL;
static volatile LONG g_bWindowAlive = 0;
static volatile LONG g_bExitThread = 0;
static HANDLE g_hWindowThread = NULL;

/* 内部函数 */
static BOOL OpenSharedMemory(void);
static BOOL InitHeartbeatEvent(void);
static void SetHeartbeatSignal(void);
static void ReadLog(WCHAR *buffer, SIZE_T size);
static DWORD WINAPI WindowMainThread(LPVOID lpParam);
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
        g_hWindowThread = CreateThread(NULL, 0, WindowMainThread, NULL, 0, NULL);
    } else if (fdwReason == DLL_PROCESS_DETACH) {
        g_bExitThread = 1;
        if (g_hWnd) PostMessage(g_hWnd, WM_QUIT, 0, 0);
        if (g_hWindowThread) {
            WaitForSingleObject(g_hWindowThread, 3000);
            CloseHandle(g_hWindowThread);
        }
        if (g_hHeartbeatEvent) CloseHandle(g_hHeartbeatEvent);
        if (g_hGuardMutex) CloseHandle(g_hGuardMutex);
        if (g_pSharedView) UnmapViewOfFile(g_pSharedView);
        if (g_hSharedMem) CloseHandle(g_hSharedMem);
    }
    return TRUE;
}

static BOOL OpenSharedMemory(void)
{
    g_hSharedMem = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, SHARED_MEM_NAME);
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
    WCHAR *pSuffix = (WCHAR *)((BYTE *)g_pSharedView + OFFSET_HB_SUFFIX);
    if (pSuffix[0] == L'\0') return FALSE;

    WCHAR szEventName[128];
    _snwprintf(szEventName, _countof(szEventName), L"%s%s", HEARTBEAT_PREFIX, pSuffix);
    g_hHeartbeatEvent = CreateEventW(NULL, TRUE, FALSE, szEventName);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(g_hHeartbeatEvent);
        g_hHeartbeatEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, szEventName);
    }
    return (g_hHeartbeatEvent != NULL);
}

static void SetHeartbeatSignal(void)
{
    if (g_bWindowAlive && g_hHeartbeatEvent)
        SetEvent(g_hHeartbeatEvent);
}

static void ReadLog(WCHAR *buffer, SIZE_T size)
{
    if (!g_pSharedView) return;
    WCHAR *pLog = (WCHAR *)((BYTE *)g_pSharedView + OFFSET_LOG);
    wcsncpy_s(buffer, size, pLog, _TRUNCATE);
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
            SetHeartbeatSignal();
            InvalidateRect(hWnd, NULL, FALSE);
            /* 对手可能用 ShowWindow(SW_HIDE) 让弹窗"消失"，这里每 500ms 自检并拉回 */
            if (g_hWnd && !IsWindowVisible(g_hWnd)) ShowWindow(g_hWnd, SW_SHOW);
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);

        HBRUSH hBrush = CreateSolidBrush(RGB(30, 30, 30));
        FillRect(hdc, &rc, hBrush);
        DeleteObject(hBrush);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(0, 255, 0));
        SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
        DrawTextW(hdc, L"XX PROGRAM - ALIVE", -1, &rc, DT_CENTER | DT_TOP | DT_SINGLELINE);

        rc.top += 30;
        SetTextColor(hdc, RGB(200, 200, 200));
        WCHAR logBuffer[2048] = { 0 };
        ReadLog(logBuffer, _countof(logBuffer));

        /* 拆行，只显示最近 MAX_SHOWN_LINES 条（超限时只丢最上面的） */
        WCHAR work[2048];
        wcscpy_s(work, _countof(work), logBuffer);
        WCHAR *w = work;
        for (WCHAR *r = work; *r; r++) {
            if (*r == L'\r') { *w++ = L'\n'; if (r[1] == L'\n') r++; }
            else *w++ = *r;
        }
        *w = L'\0';

        WCHAR *starts[128];
        int nLines = 0;
        starts[nLines++] = work;
        for (WCHAR *p = work; *p; p++) {
            if (*p == L'\n') {
                *p = L'\0';
                if (p[1] != L'\0' && nLines < 128)
                    starts[nLines++] = p + 1;
            }
        }

        WCHAR display[2048];
        display[0] = L'\0';
        int startIdx = (nLines > MAX_SHOWN_LINES) ? (nLines - MAX_SHOWN_LINES) : 0;
        for (int i = startIdx; i < nLines; i++) {
            if (starts[i][0] == L'\0') continue;
            wcscat_s(display, _countof(display), starts[i]);
            wcscat_s(display, _countof(display), L"\r\n");
        }

        if (display[0] == L'\0')
            wcscpy_s(display, _countof(display), L"Awaiting log...");
        DrawTextW(hdc, display, -1, &rc, DT_LEFT | DT_TOP | DT_WORDBREAK);

        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_CLOSE:
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_CLOSE)
            return 0;
        return DefWindowProc(hWnd, msg, wParam, lParam);
    case WM_DESTROY:
        KillTimer(hWnd, 1);
        g_bWindowAlive = 0;
        g_hWnd = NULL;
        PostQuitMessage(0);
        return 0;
    }
    /* 不在白名单里的一律拒绝 */
    return PassThroughMsg(msg) ? DefWindowProc(hWnd, msg, wParam, lParam) : 0;
}

static DWORD WINAPI WindowMainThread(LPVOID lpParam)
{
    UNREFERENCED_PARAMETER(lpParam);

    // 等待共享内存可用
    for (int retry = 0; retry < 30; retry++) {
        if (OpenSharedMemory()) break;
        Sleep(50);
    }
    if (!g_pSharedView) return 0;
    if (!InitHeartbeatEvent()) return 0;

    // 构建窗口类名（使用共享内存中的后缀）
    WCHAR *pWndSuffix = (WCHAR *)((BYTE *)g_pSharedView + OFFSET_WND_SUFFIX);
    WCHAR szClassName[128];
    _snwprintf(szClassName, _countof(szClassName), L"XX_ALIVE_WND_%s", pWndSuffix);

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(L"window.dll");
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = szClassName;
    RegisterClassW(&wc);

    g_hGuardMutex = CreateMutexW(NULL, FALSE, WINDOW_GUARD_MUTEX);
    if (!g_hGuardMutex) return 0;

    // 极速热备循环：几乎无延迟接管
    while (!g_bExitThread) {
        DWORD dwWait = WaitForSingleObject(g_hGuardMutex, 0);
        if (dwWait == WAIT_OBJECT_0) {
            // 获得弹窗权：确保 xx.exe 存活
            EnsureXxRunning();

            g_hWnd = CreateWindowExW(0, szClassName, L"XX Alive",
                                     WS_OVERLAPPEDWINDOW & ~WS_SYSMENU,
                                     CW_USEDEFAULT, CW_USEDEFAULT,
                                     900, 650, NULL, NULL, wc.hInstance, NULL);
            if (g_hWnd) {
                ShowWindow(g_hWnd, SW_SHOW);
                UpdateWindow(g_hWnd);
                MSG msg;
                /* 消息循环：拒绝外部投递的 WM_QUIT
                   （对手 PostMessage(WM_QUIT) 会让 GetMessage 返回 0，
                     循环结束 → DestroyWindow → 弹窗消失 8 秒判负）。
                   自身退出时 g_bExitThread 已被 DLL_PROCESS_DETACH 置 1，正常放行。 */
                for (;;) {
                    BOOL gm = GetMessage(&msg, NULL, 0, 0);
                    if (gm == -1) break;                        /* GetMessage 出错 */
                    if (gm == 0) {                              /* 收到 WM_QUIT */
                        /* 自身退出，或弹窗已被销毁（WM_DESTROY 的 PostQuitMessage）→
                           退出本循环，交由外层热备循环重建弹窗 */
                        if (g_bExitThread || g_hWnd == NULL) break;
                        continue;                               /* 外部 WM_QUIT → 拒绝并继续 */
                    }
                    if (g_bExitThread) break;
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
                if (g_hWnd) DestroyWindow(g_hWnd);
                g_hWnd = NULL;
                g_bWindowAlive = 0;
                ReleaseMutex(g_hGuardMutex);
            } else {
                ReleaseMutex(g_hGuardMutex);
                Sleep(5);
            }
        } else if (dwWait == WAIT_ABANDONED) {
            ReleaseMutex(g_hGuardMutex);
        } else {
            Sleep(5);
        }
    }
    return 0;
}

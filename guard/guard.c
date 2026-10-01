/*
 * guard.dll - 进程守护模块（C语言版，基于 deamon_dll.cpp）
 * 功能：读取同目录下的 xpt.xpt，监控列表中的进程，终止则重启，存活则恢复挂起线程
 *       启动前会检查目标程序是否已有进程存在，防止重复启动。
 * 编译器：GNU GCC (MinGW-w64)
 * 编译：gcc -shared -o guard.dll guard.c -ladvapi32 -static -s -O2
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define SHARED_MEM_NAME  L"Global\\xx_shared_mem"
#define SHARED_MEM_SIZE  4096
#define OFFSET_LOG       256
#define LOG_SIZE         3808

static CRITICAL_SECTION g_csLog;
static BOOL g_csLogInit = FALSE;
static HANDLE g_hSharedMap = NULL;
static PVOID  g_pSharedView = NULL;
static DWORD  g_dwLastLogTick = 0;
#define GUARD_LOG_INTERVAL_MS 10000

static void EnsureLogInit(void) {
    if (g_csLogInit) return;
    InitializeCriticalSection(&g_csLog);
    g_csLogInit = TRUE;
    g_hSharedMap = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, SHARED_MEM_NAME);
    if (g_hSharedMap)
        g_pSharedView = MapViewOfFile(g_hSharedMap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, SHARED_MEM_SIZE);
}

static void WriteGuardLog(const WCHAR *fmt, ...) {
    EnsureLogInit();
    if (!g_csLogInit || !g_pSharedView) return;

    /* 节流：每 10 秒最多一条，避免刷屏 */
    DWORD now = GetTickCount();
    if (g_dwLastLogTick != 0 && (now - g_dwLastLogTick) < GUARD_LOG_INTERVAL_MS)
        return;
    g_dwLastLogTick = now;

    SYSTEMTIME st;
    GetLocalTime(&st);
    WCHAR szLine[512];
    va_list args;
    va_start(args, fmt);
    _vsnwprintf(szLine, _countof(szLine) - 1, fmt, args);
    va_end(args);
    szLine[_countof(szLine) - 1] = L'\0';

    WCHAR entry[600];
    _snwprintf(entry, _countof(entry) - 1, L"[%02d:%02d:%02d.%03d] [guard] %s\r\n",
               st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, szLine);
    entry[_countof(entry) - 1] = L'\0';

    EnterCriticalSection(&g_csLog);
    WCHAR *pLog = (WCHAR *)((BYTE *)g_pSharedView + OFFSET_LOG);
    size_t len = wcslen(pLog);
    size_t maxLen = (LOG_SIZE / sizeof(WCHAR)) - 1;
    size_t newLen = wcslen(entry);
    if (len + newLen < maxLen) {
        wcscat_s(pLog, maxLen - len, entry);
    } else {
        memmove(pLog, pLog + newLen, (maxLen - newLen) * sizeof(WCHAR));
        wcscpy_s(pLog + (maxLen - newLen), newLen, entry);
    }
    LeaveCriticalSection(&g_csLog);
}

/* 受保护进程条目 */
typedef struct {
    DWORD pid;
    char  path[MAX_PATH];
} ProtectedProcess;

/* 动态列表 */
typedef struct {
    ProtectedProcess *data;
    size_t count;
    size_t capacity;
} ProcessList;

static void ListInit(ProcessList *list) {
    list->data     = NULL;
    list->count    = 0;
    list->capacity = 0;
}

static void ListAdd(ProcessList *list, DWORD pid, const char *path) {
    if (list->count >= list->capacity) {
        size_t newcap = list->capacity ? list->capacity * 2 : 16;
        ProtectedProcess *tmp = (ProtectedProcess *)realloc(list->data,
                                     newcap * sizeof(ProtectedProcess));
        if (!tmp) return;
        list->data     = tmp;
        list->capacity = newcap;
    }
    list->data[list->count].pid = pid;
    strncpy(list->data[list->count].path, path, MAX_PATH - 1);
    list->data[list->count].path[MAX_PATH - 1] = '\0';
    list->count++;
}

static void ListFree(ProcessList *list) {
    free(list->data);
    ListInit(list);
}

/* 获取 guard.dll 所在目录，并拼出 xpt.xpt 的完整路径 */
static void GetXptPath(char *buf, size_t size) {
    char dllPath[MAX_PATH];
    GetModuleFileNameA(GetModuleHandleA("guard.dll"), dllPath, MAX_PATH);
    char *pSlash = strrchr(dllPath, '\\');
    if (pSlash) pSlash[1] = '\0';
    snprintf(buf, size, "%sxpt.xpt", dllPath);
}

/* 从 xpt.xpt 读取所有受保护进程（与 C++ 版逻辑一致） */
static ProcessList ReadProtectedList(void) {
    ProcessList list;
    ListInit(&list);

    char xptPath[MAX_PATH];
    GetXptPath(xptPath, sizeof(xptPath));
    FILE *fp = fopen(xptPath, "r");
    if (!fp) return list;

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        /* 去除尾部空白（换行、回车、空格） */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r' || line[len-1] == ' '))
            line[--len] = '\0';
        if (len == 0) continue;

        /* 第一行：PID */
        DWORD pid = (DWORD)strtoul(line, NULL, 10);
        if (pid == 0) continue;

        /* 第二行：路径 */
        if (!fgets(line, sizeof(line), fp)) break;
        len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r' || line[len-1] == ' '))
            line[--len] = '\0';

        if (len > 0) {
            ListAdd(&list, pid, line);
        }
    }
    fclose(fp);
    return list;
}

/* 检查某个可执行文件是否已有实例在运行（通过枚举进程名称） */
static BOOL IsProcessAlreadyRunning(const char *exePath) {
    BOOL found = FALSE;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return FALSE;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);
    if (Process32First(hSnap, &pe)) {
        do {
            /* 比较进程名：需与目标路径中的文件名相同 */
            char procName[MAX_PATH];
            strncpy(procName, pe.szExeFile, MAX_PATH - 1);
            procName[MAX_PATH - 1] = '\0';

            /* 从完整路径提取文件名 */
            const char *pSlash = strrchr(exePath, '\\');
            const char *targetName = pSlash ? pSlash + 1 : exePath;

            if (_stricmp(procName, targetName) == 0) {
                found = TRUE;
                break;
            }
        } while (Process32Next(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return found;
}

/* 恢复指定进程的所有线程 */
static void ResumeProcessThreads(DWORD pid) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;

    int resumed = 0;
    THREADENTRY32 te32;
    te32.dwSize = sizeof(THREADENTRY32);
    if (Thread32First(hSnapshot, &te32)) {
        do {
            if (te32.th32OwnerProcessID == pid) {
                HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te32.th32ThreadID);
                if (hThread) {
                    while (ResumeThread(hThread) > 0) resumed++;
                    CloseHandle(hThread);
                }
            }
        } while (Thread32Next(hSnapshot, &te32));
    }
    CloseHandle(hSnapshot);
    if (resumed > 0)
        WriteGuardLog(L"恢复挂起线程 PID=%lu 恢复数=%d", pid, resumed);
}

/* 守护线程函数（与 C++ 版本行为一致，但增加了防止重复启动的检查） */
static DWORD WINAPI DaemonThread(LPVOID lpParam) {
    (void)lpParam;

    /* 冷却时间：5 秒内不对同一路径进行重启，防止多个守护实例同时创建进程 */
    DWORD dwLastRespawnTime = 0;
    const DWORD COOLDOWN_MS = 5000;

    while (1) {
        ProcessList list = ReadProtectedList();
        size_t i;

        for (i = 0; i < list.count; i++) {
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE,
                                          FALSE, list.data[i].pid);
            if (hProcess == NULL) {
                /* 进程已终止 */
                if (list.data[i].path[0] != '\0') {
                    DWORD tick = GetTickCount();
                    if (tick - dwLastRespawnTime < COOLDOWN_MS) {
                        /* 冷却中，暂不重启，等待下次扫描 */
                        continue;
                    }

                    /* 检查目标程序是否已有实例在运行，若有则跳过 */
                    if (!IsProcessAlreadyRunning(list.data[i].path)) {
                        STARTUPINFOA si = { sizeof(si) };
                        PROCESS_INFORMATION pi = { 0 };
                        if (CreateProcessA(list.data[i].path, NULL, NULL, NULL,
                                           FALSE, 0, NULL, NULL, &si, &pi)) {
                            WriteGuardLog(L"复活进程成功: %hs 新PID=%lu", list.data[i].path, pi.dwProcessId);
                            CloseHandle(pi.hProcess);
                            CloseHandle(pi.hThread);
                            dwLastRespawnTime = GetTickCount();  /* 更新冷却时间 */
                        } else {
                            WriteGuardLog(L"复活进程失败: %hs err=%lu", list.data[i].path, GetLastError());
                        }
                    } else {
                        WriteGuardLog(L"跳过复活（已有实例）: %hs", list.data[i].path);
                    }
                }
            } else {
                /* 进程存活，恢复其所有线程 */
                ResumeProcessThreads(list.data[i].pid);
                CloseHandle(hProcess);
            }
        }
        ListFree(&list);
        Sleep(200);
    }
    return 0;
}

/* DllMain - 与 C++ 版完全一致 */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)lpvReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        HANDLE hThread = CreateThread(NULL, 0, DaemonThread, NULL, 0, NULL);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}

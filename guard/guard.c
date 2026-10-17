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

/* 共享内存布局与命名常量：唯一来源见 xxipc.h */
#include "xxipc.h"

static CRITICAL_SECTION g_csLog;
static BOOL g_csLogInit = FALSE;
static HANDLE g_hSharedMap = NULL;
static PVOID  g_pSharedView = NULL;
static DWORD  g_dwLastLogTick = 0;
#define GUARD_LOG_INTERVAL_MS 10000

static HANDLE g_hXx = NULL;   /* xx 交接过来的进程句柄（只在被复制进的本进程内有效） */
static HANDLE g_hSingleton = NULL;    /* 进程内单例（持有到进程结束） */
static HMODULE g_hSelfModule = NULL;  /* 自身模块句柄（摘 PEB 链前缓存） */
static ULONGLONG g_dwNameToken = 0;       /* 会话随机名 token：只由 XxSetNames 送进来 */

static ULONGLONG NameTok(void) {
    if (g_dwNameToken) return g_dwNameToken;
    if (g_pSharedView) {
        ULONGLONG s = XX_NAME_TOKEN64(g_pSharedView);
        if (s) return s;
    }
    return XX_NAME_TOKEN_FIXED;
}


/* 远程导出：接收会话随机名（方案 A） */
__declspec(dllexport) BOOL WINAPI XxSetNames(const XX_NAME_BLOCK *pBlock) {
    ULONGLONG tok = 0;
    if (!XXNameBlockTake(pBlock, &tok, NULL)) return FALSE;
    if (tok) g_dwNameToken = tok;
    return TRUE;
}

/* 前置声明（判活退化路径要用） */
static BOOL IsProcessAlreadyRunning(const char *exePath);

static void EnsureLogInit(void) {
    if (g_csLogInit) return;
    InitializeCriticalSection(&g_csLog);
    g_csLogInit = TRUE;
    if (!g_pSharedView) {
        g_hSharedMap = XXOpenShared();
        if (g_hSharedMap)
            g_pSharedView = MapViewOfFile(g_hSharedMap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, SHARED_MEM_SIZE);
    }
}

/* ================== xx 句柄交接 ==================
   xx.exe 的进程 DACL 拒绝 Everyone（连自己和 SYSTEM 一起拒），OpenProcess 打不开它。
   所以跑在 xx 里的 svcguard 会把 xx 自己的进程句柄复制进本宿主进程，并按本进程
   PID 登记到共享内存句柄表；这里直接取出来用。句柄值只在被复制进来的进程内有效，
   对手即使读到也用不了；权限只有 查询+同步，拿到也无法终止 xx。 */

static BOOL OpenSharedForHandle(void) {
    if (g_pSharedView) return TRUE;
    if (!g_hSharedMap) {
        g_hSharedMap = XXOpenShared();
        if (!g_hSharedMap) return FALSE;
    }
    g_pSharedView = MapViewOfFile(g_hSharedMap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, SHARED_MEM_SIZE);
    return (g_pSharedView != NULL);
}

static HANDLE FindXxHandle(DWORD *pXxPid) {
    ULONGLONG tag;
    int i;

    if (!OpenSharedForHandle()) return NULL;
    if (XX_LAYOUT(g_pSharedView) != XX_LAYOUT_VERSION) return NULL;

    tag = (ULONGLONG)XX_TAG(g_pSharedView);
    for (i = 0; i < HDL_TABLE_MAX; i++) {
        HANDLE h;
        DWORD  expect, owner;

        if (XX_HDL_PID(g_pSharedView, i) != GetCurrentProcessId()) continue;
        if (XX_HDL_TAG(g_pSharedView, i) != tag) continue;        /* 上一次启动的陈旧条目 */
        if (XX_HDL_HANDLE(g_pSharedView, i) == 0) continue;

        /* 校验：必须是有效句柄，且确实指向记录里的那个进程 ——
           槽位复用/撕裂读都可能留下一个"看起来像句柄"的数字，直接拿去
           WaitForSingleObject 会炸在宿主进程里。 */
        h = (HANDLE)(ULONG_PTR)XX_HDL_HANDLE(g_pSharedView, i);
        expect = XX_HDL_RESERVED(g_pSharedView, i);
        owner = GetProcessId(h);
        if (owner == 0) continue;
        if (expect != 0 && owner != expect) continue;

        if (pXxPid) *pXxPid = owner;
        return h;
    }
    return NULL;
}

/* 判活：优先用交接句柄（进程一结束该句柄即受信），失效才退回 OpenProcess */
static BOOL XxAlive(DWORD pid, const char *path) {
    DWORD w;
    HANDLE h;

    if (g_hXx) {
        w = WaitForSingleObject(g_hXx, 0);
        if (w == WAIT_TIMEOUT) return TRUE;
        g_hXx = NULL;                 /* 已结束或句柄失效：清掉，下一轮重新查表 */
        if (w == WAIT_OBJECT_0) return FALSE;
    }
    h = OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, pid);
    if (h) {
        w = WaitForSingleObject(h, 0);
        CloseHandle(h);
        return (w != WAIT_OBJECT_0);
    }
    /* 被 DACL 拒绝：至少按进程名确认它还活着 —— 否则"判活手段失效"会被当成
       "xx 已死"，既反复走复活分支，又永远不再恢复它被挂起的线程。 */
    if (path && path[0]) return IsProcessAlreadyRunning(path);
    return FALSE;
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
    WCHAR fline[640];
    va_list args;
    va_start(args, fmt);
    _vsnwprintf(szLine, _countof(szLine) - 1, fmt, args);
    va_end(args);
    szLine[_countof(szLine) - 1] = L'\0';

    /* 没有弹窗时共享日志看不到，同时落一份文件 */
    _snwprintf(fline, _countof(fline) - 1, L"[guard][pid=%lu] %s\r\n",
               GetCurrentProcessId(), szLine);
    fline[_countof(fline) - 1] = L'\0';
    XXFileLog(fline);

    if (!g_pSharedView) return;

    WCHAR entry[600];
    _snwprintf(entry, _countof(entry) - 1, L"[%02d:%02d:%02d.%03d] [guard] %s\r\n",
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

/* 获取 xpt.xpt 的完整路径。
   注入副本是从 %TEMP% 的改名文件加载的，按自身模块路径找 xpt.xpt 会找到 %TEMP%
   里去 —— 所以优先用共享内存里 xx.exe 记下的路径推目录。 */
static void GetXptPath(char *buf, size_t size) {
    char dllPath[MAX_PATH];
    WCHAR wdir[MAX_PATH];

    wdir[0] = L'\0';
    if (g_pSharedView || OpenSharedForHandle()) {
        XXDirNameOf(XXPathBase(g_pSharedView, NULL), wdir, MAX_PATH);
    }
    if (wdir[0] != L'\0') {
        if (WideCharToMultiByte(CP_ACP, 0, wdir, -1, dllPath, MAX_PATH, NULL, NULL) == 0)
            dllPath[0] = '\0';
    } else {
        GetModuleFileNameA(g_hSelfModule ? g_hSelfModule : GetModuleHandleA("guard.dll"),
                           dllPath, MAX_PATH);
        char *pSlash = strrchr(dllPath, '\\');
        if (pSlash) pSlash[1] = '\0';
    }

    if (strlen(dllPath) + strlen(XPT_FILE_NAME) + 1 > size) {   /* 放不下就别截断写 */
        if (size) buf[0] = '\0';
        return;
    }
    snprintf(buf, size, "%s" XPT_FILE_NAME, dllPath);
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

/* 守护线程：读 xpt.xpt 里的受保护进程，用交接句柄判活；死了就重启，活着就恢复其线程 */
static DWORD WINAPI DaemonThread(LPVOID lpParam) {
    DWORD dwLastRespawnTime = 0;
    const DWORD COOLDOWN_MS = 5000;

    (void)lpParam;

    /* 共享内存可能晚于本实例就绪；拿不到就退化成 OpenProcess 判活 */
    OpenSharedForHandle();

    while (1) {
        ProcessList list = ReadProtectedList();
        size_t i;

        if (!g_hXx) g_hXx = FindXxHandle(NULL);

        for (i = 0; i < list.count; i++) {
            if (!XxAlive(list.data[i].pid, list.data[i].path)) {
                /* 进程已终止（或判活手段不可用） */
                if (list.data[i].path[0] != '\0') {
                    DWORD tick = GetTickCount();
                    if (tick - dwLastRespawnTime < COOLDOWN_MS)
                        continue;                       /* 冷却中，暂不重启 */

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
                            g_hXx = NULL;               /* 等新实例重新交接句柄 */
                        } else {
                            WriteGuardLog(L"复活进程失败: %hs err=%lu", list.data[i].path, GetLastError());
                        }
                    } else {
                        WriteGuardLog(L"跳过复活（已有实例）: %hs", list.data[i].path);
                    }
                }
            } else {
                /* 进程存活：恢复它被挂起的线程（yy 会先 SuspendThread 再杀） */
                ResumeProcessThreads(list.data[i].pid);
            }
        }
        ListFree(&list);
        Sleep(200);
    }
    return 0;
}

/* DllMain */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)lpvReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);

        /* 摘 PEB 链之后自身模块就查不到了，先把句柄缓存下来 */
        g_hSelfModule = hinstDLL;

        /* 进程内单例：模块被隐藏后注入方无法判断宿主里有没有我们，会反复注入；
           已有实例时直接让加载失败，避免同进程叠出多套看门狗线程。 */
        /* 等注入方把会话 token 送进来，让单例名和注入方的探针名一致 */
        XXWaitToken(&g_dwNameToken, 300);
        g_hSingleton = XXModuleSingleton(L"guard", NameTok());
        if (!g_hSingleton) return FALSE;

        XXHideSelfFromPeb(hinstDLL);          /* 从 PEB 三条模块链表里摘除自己 */

        {
            HANDLE hThread = CreateThread(NULL, 0, DaemonThread, NULL, 0, NULL);
            if (hThread) CloseHandle(hThread);
        }
    }
    return TRUE;
}

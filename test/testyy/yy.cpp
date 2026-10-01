#include <windows.h>
#include <winternl.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>

// ---- 全局配置区域 ----
const wchar_t* GOAL_NAME = L"xx.exe";              
const wchar_t* SHOW_TITLE = L"YY_Defensive_GUI";  
const wchar_t* GUARD_DLL = L"guard.dll";          
const wchar_t* HONEYPOT_EXE_NAME = L"lenovo_fix.exe"; 

// 敌方核心军火库 DLL 拦截名单
const wchar_t* BAD_MODULES[] = {
    L"DeamonWindows.dll", L"deamon.dll", L"Killer.dll", L"HookCore.dll", 
    L"MinHook.dll", L"defenser.dll", L"killtools.dll", L"killer.dll"
};
const int BAD_COUNT = 8;

// 我方分布式看门狗常驻进程：只注 explorer。
// 不再注 lsass/svchost —— 对系统关键进程用 CreateRemoteThread+LoadLibrary 有
// loader-lock 死锁风险，且 lsass 一崩就是整机崩（就是之前"资源管理器崩溃"的来源）。
const wchar_t* HOOD_LIST[] = {
    L"explorer.exe"
};
const int HOOD_COUNT = 1;

// 漏洞驱动设备通信链接屏蔽名单
const wchar_t* BYOVD_DEVICES[] = {
    L"\\\\.\\BootRepair", L"\\\\.\\kdmapper", L"\\\\.\\kdu", L"\\\\.\\GIO"
};
const int BYOVD_COUNT = 4;

// ---- 全局控制句柄池 ----
HANDLE g_hDeviceLocks[BYOVD_COUNT] = { INVALID_HANDLE_VALUE };
HANDLE g_hMaliciousFileLocks[BAD_COUNT] = { INVALID_HANDLE_VALUE }; 

HWND g_hLogBox = NULL;         
HWND g_hMainWnd = NULL;        
CRITICAL_SECTION g_LockBox;    

void* g_pMainTextBase = nullptr;         
SIZE_T g_dwTextSize = 0;                 
BYTE* g_pOriginalImageBackup = nullptr;  

DWORD g_HoneypotTriggeredPID = 0; 

typedef LONG(NTAPI* fnSuspend)(HANDLE hProc);
fnSuspend pfnSuspend = nullptr;

typedef LONG(NTAPI* fnSuspendThread)(HANDLE hThread, PULONG pPreviousSuspendCount);
fnSuspendThread pfnSuspendThread = nullptr;

void LogAction(const std::string& text) {
    EnterCriticalSection(&g_LockBox);
    SYSTEMTIME t; GetLocalTime(&t); std::stringstream ss;
    ss << "[" << std::setw(2) << std::setfill('0') << t.wDay << "-" 
       << std::setw(2) << std::setfill('0') << t.wMonth << "-" 
       << std::setw(2) << std::setfill('0') << t.wSecond << "] " << text << "\r\n";
    std::string s = ss.str(); std::cout << s; 
    if (g_hLogBox && IsWindow(g_hLogBox)) {
        int v = GetWindowTextLengthA(g_hLogBox);
        SendMessageA(g_hLogBox, EM_SETSEL, v, v);
        SendMessageA(g_hLogBox, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(s.c_str()));
    }
    LeaveCriticalSection(&g_LockBox);
}

// 占坑死锁控制函数：高健壮性重构，防止多线程死锁
void LockDownMaliciousFileNodes() {
    EnterCriticalSection(&g_LockBox); // 引入同步锁，防止 Hunter 线程和 main 线程撞车
    for (int i = 0; i < BAD_COUNT; i++) {
        if (g_hMaliciousFileLocks[i] != INVALID_HANDLE_VALUE) continue;
        g_hMaliciousFileLocks[i] = CreateFileW(
            BAD_MODULES[i],
            GENERIC_READ | GENERIC_WRITE,
            0, // 0 共享代表物理独占
            NULL,
            CREATE_ALWAYS, 
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
    }
    LeaveCriticalSection(&g_LockBox);
}

void ReleaseFileLocks() {
    EnterCriticalSection(&g_LockBox);
    for (int i = 0; i < BAD_COUNT; i++) {
        if (g_hMaliciousFileLocks[i] != INVALID_HANDLE_VALUE) {
            CloseHandle(g_hMaliciousFileLocks[i]);
            g_hMaliciousFileLocks[i] = INVALID_HANDLE_VALUE;
        }
    }
    LeaveCriticalSection(&g_LockBox);
}

void LockDownBbyovdChannels() {
    for (int i = 0; i < BYOVD_COUNT; i++) {
        g_hDeviceLocks[i] = CreateFileW(BYOVD_DEVICES[i], GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }
}

// ---- 防御核心 B：LdrLoadDll 反注入锁 ----
typedef NTSTATUS(NTAPI* fnLdrLoadDll)(PWSTR PathToFile, PULONG Flags, PUNICODE_STRING ModuleFileName, PHANDLE ModuleHandle);
fnLdrLoadDll pfnLdrLoadDll = nullptr;

NTSTATUS NTAPI SpyLdrLoadDll(PWSTR PathToFile, PULONG Flags, PUNICODE_STRING ModuleFileName, PHANDLE ModuleHandle) {
    if (ModuleFileName && ModuleFileName->Buffer) {
        for (int i = 0; i < BAD_COUNT; i++) {
            if (wcsstr(ModuleFileName->Buffer, BAD_MODULES[i]) != nullptr) {
                LogAction("DLL Shield: Denied enemy DLL injection workflow.");
                return 0xC0000022; 
            }
        }
    }
    return pfnLdrLoadDll(PathToFile, Flags, ModuleFileName, ModuleHandle);
}

// ---- 判定函数前导完整指令长度（>=14 才塞得下绝对跳转）；识别不了返回 -1 ----
static int PrologueLen(void* func) {
    unsigned char* p = (unsigned char*)func;
    if (p[0] == 0x4C && p[1] == 0x8B && p[2] == 0xDC) return 18;                 // LdrLoadDll (Win8+)
    if (p[0] == 0x48 && p[1] == 0x89 && p[2] == 0x5C && p[3] == 0x24) return 15; // LdrLoadDll (Win7)
    if (p[0] == 0x4C && p[1] == 0x8B && p[2] == 0xD1 && p[3] == 0xB8) {          // syscall stub
        if (p[8] == 0xF6 && p[9] == 0x04 && p[10] == 0x25) return 16;            // Win8+
        if (p[8] == 0x0F && p[9] == 0x05) return 16;                             // Win7
    }
    return -1;
}

// ---- trampoline：保存原文前 n 字节 + 跳回 func+n，避免调用原函数时递归回 hook ----
static void* MakeTrampoline(void* func, int n) {
    unsigned char* t = (unsigned char*)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!t) return nullptr;
    memcpy(t, func, n);
    t[n + 0] = 0xFF; t[n + 1] = 0x25;  // jmp qword ptr [rip+0]
    t[n + 2] = 0x00; t[n + 3] = 0x00; t[n + 4] = 0x00; t[n + 5] = 0x00;
    *(void**)(t + n + 6) = (unsigned char*)func + n;
    return t;
}

// ---- 安装 14 字节绝对跳转 hook，并返回可调用原函数的 trampoline ----
static bool InstallHook(void* pTarget, void* pSpy, void** outTramp) {
    int n = PrologueLen(pTarget);
    if (n < 14) return false;
    void* tramp = MakeTrampoline(pTarget, n);
    if (!tramp) return false;
    unsigned char stub[32];
    for (int i = 0; i < n; i++) stub[i] = 0x90;
    stub[0] = 0xFF; stub[1] = 0x25;    // jmp qword ptr [rip+0]
    stub[2] = 0x00; stub[3] = 0x00; stub[4] = 0x00; stub[5] = 0x00;
    *(void**)(stub + 6) = pSpy;
    DWORD op;
    if (!VirtualProtect(pTarget, n, PAGE_EXECUTE_READWRITE, &op)) return false;
    memcpy(pTarget, stub, n);
    VirtualProtect(pTarget, n, op, &op);
    *outTramp = tramp;
    return true;
}

void ForceLockDllInjection() {
    HMODULE hMod = GetModuleHandleA("ntdll.dll"); if (!hMod) return;
    void* pTarget = reinterpret_cast<void*>(GetProcAddress(hMod, "LdrLoadDll"));
    if (!pTarget) return;
    void* tramp = nullptr;
    if (!InstallHook(pTarget, reinterpret_cast<void*>(SpyLdrLoadDll), &tramp)) {
        LogAction("DLL Shield: LdrLoadDll hook skipped (unknown prologue).");
        return;
    }
    pfnLdrLoadDll = reinterpret_cast<fnLdrLoadDll>(tramp);
    LogAction("DLL Shield: Active hook installed at ntdll!LdrLoadDll.");
}

// ---- 防御核心 C：VEH 异常冷镜像自愈盾 ----
LONG WINAPI QuickVeh(PEXCEPTION_POINTERS pEx) {
    if (pEx->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT) {
        void* addr = pEx->ExceptionRecord->ExceptionAddress;
        if (addr >= g_pMainTextBase && addr < reinterpret_cast<BYTE*>(g_pMainTextBase) + g_dwTextSize) {
            SIZE_T offset = reinterpret_cast<BYTE*>(addr) - reinterpret_cast<BYTE*>(g_pMainTextBase);
            BYTE originalByte = g_pOriginalImageBackup[offset];
            DWORD op;
            if (VirtualProtect(addr, 1, PAGE_EXECUTE_READWRITE, &op)) {
                *static_cast<unsigned char*>(addr) = originalByte; 
                VirtualProtect(addr, 1, op, &op);
                LogAction("Matrix VEH Shield: Restored corrupted memory bits back to origin.");
                return EXCEPTION_CONTINUE_EXECUTION; 
            }
        }
    }
    return EXCEPTION_CONTINUE_SEARCH; 
}

// ---- 防御核心 D：远程线程隔离保护锁 ----
typedef NTSTATUS(NTAPI* fnCreateThread)(PHANDLE th, ACCESS_MASK ac, PVOID obj, HANDLE pr, PVOID st, PVOID arg, ULONG fl, ULONG_PTR z, SIZE_T sk, SIZE_T mx, PVOID at);
typedef NTSTATUS(NTAPI* fnQueueApc)(HANDLE th, PVOID ap, PVOID ctx, PVOID st, ULONG rs);
fnCreateThread pfnCreateThread = nullptr;
fnQueueApc pfnQueueApc = nullptr;

NTSTATUS NTAPI SpyCreateThread(PHANDLE th, ACCESS_MASK ac, PVOID obj, HANDLE pr, PVOID st, PVOID arg, ULONG fl, ULONG_PTR z, SIZE_T sk, SIZE_T mx, PVOID at) {
    // 本地 hook 无法拦截"别人往我这儿建线程"，原判定只会拦住自己向外注入
    // (把 guard.dll 铺到 explorer 等)。这里直接放行，恢复自身扩散能力。
    return pfnCreateThread(th, ac, obj, pr, st, arg, fl, z, sk, mx, at);
}
NTSTATUS NTAPI SpyQueueApc(HANDLE th, PVOID ap, PVOID ctx, PVOID st, ULONG rs) {
    LogAction("Shield Notice: Neutralized incoming APC payload.");
    return 0; 
}

void ForceLockRemoteCall() {
    HMODULE hMod = GetModuleHandleA("ntdll.dll"); if (!hMod) return;
    void* pEx = reinterpret_cast<void*>(GetProcAddress(hMod, "NtCreateThreadEx"));
    if (pEx) {
        void* t = nullptr;
        if (InstallHook(pEx, reinterpret_cast<void*>(SpyCreateThread), &t))
            pfnCreateThread = reinterpret_cast<fnCreateThread>(t);
    }
    void* pApc = reinterpret_cast<void*>(GetProcAddress(hMod, "NtQueueApcThread"));
    if (pApc) {
        void* t = nullptr;
        if (InstallHook(pApc, reinterpret_cast<void*>(SpyQueueApc), &t))
            pfnQueueApc = reinterpret_cast<fnQueueApc>(t);
    }
}

DWORD WINAPI ShieldWorker(LPVOID lpParamToken) {
    HMODULE hMod = GetModuleHandleA("kernel32.dll"); if (!hMod) return 0;
    void* pTarget = reinterpret_cast<void*>(GetProcAddress(hMod, "TerminateProcess")); if (!pTarget) return 0;
    BYTE cache[5] = { 0 }; memcpy(cache, pTarget, 5);
    while (true) {
        DWORD op; if (VirtualProtect(pTarget, 5, PAGE_EXECUTE_READWRITE, &op)) { memcpy(pTarget, cache, 5); VirtualProtect(pTarget, 5, op, &op); }
        Sleep(30); 
    }
    return 0;
}

bool EnableDebugPrivilege() {
    HANDLE hToken; LUID luid; TOKEN_PRIVILEGES tp;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
            tp.PrivilegeCount = 1; tp.Privileges[0].Luid = luid; tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
        }
        CloseHandle(hToken); return true;
    }
    return false;
}

bool CheckNameMatch(const wchar_t* a, const wchar_t* b) {
    if (!a || !b) return false;
    while (*a && *b) { if (towupper(*a) != towupper(*b)) return false; a++; b++; }
    return (*a == L'\0' && *b == L'\0');
}

bool IsModuleLoaded(DWORD pid, const wchar_t* dllBaseName) {
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (h == INVALID_HANDLE_VALUE) return false;
    MODULEENTRY32W me = { sizeof(MODULEENTRY32W) };
    bool found = false;
    if (Module32FirstW(h, &me)) {
        do { if (CheckNameMatch(me.szModule, dllBaseName)) { found = true; break; } } while (Module32NextW(h, &me));
    }
    CloseHandle(h);
    return found;
}

/* xx killtools 的 DLL 注入（KillByKillerDll 原样搬过来） */
bool ActionInject(const wchar_t* pName, const wchar_t* path) {
    const wchar_t* base = wcsrchr(path, L'\\'); base = base ? base + 1 : path;
    bool ok = false; HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); if (hSnap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe = { sizeof(PROCESSENTRY32W) };
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (CheckNameMatch(pe.szExeFile, pName)) {
                if (IsModuleLoaded(pe.th32ProcessID, base)) { ok = true; continue; }

                HANDLE hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE,
                                           FALSE, pe.th32ProcessID);
                if (!hProc) continue;

                SIZE_T pathSize = (lstrlenW(path) + 1) * sizeof(wchar_t);
                LPVOID pRemote = VirtualAllocEx(hProc, NULL, pathSize, MEM_COMMIT, PAGE_READWRITE);
                if (!pRemote) { CloseHandle(hProc); continue; }

                if (!WriteProcessMemory(hProc, pRemote, path, pathSize, NULL)) {
                    VirtualFreeEx(hProc, pRemote, 0, MEM_RELEASE);
                    CloseHandle(hProc);
                    continue;
                }

                LPTHREAD_START_ROUTINE pLoadLib = (LPTHREAD_START_ROUTINE)
                    GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
                if (!pLoadLib) {
                    VirtualFreeEx(hProc, pRemote, 0, MEM_RELEASE);
                    CloseHandle(hProc);
                    continue;
                }

                HANDLE hRemote = CreateRemoteThread(hProc, NULL, 0, pLoadLib, pRemote, 0, NULL);
                if (hRemote) {
                    WaitForSingleObject(hRemote, 100);
                    CloseHandle(hRemote);
                    ok = true;
                }
                VirtualFreeEx(hProc, pRemote, 0, MEM_RELEASE);
                CloseHandle(hProc);
            }
} while (Process32NextW(hSnap, &pe));
}
CloseHandle(hSnap); return ok;
}
bool FlagFileExists(const wchar_t* name) {
    wchar_t self[MAX_PATH];
    if (!GetModuleFileNameW(NULL, self, MAX_PATH)) return false;
    wchar_t* s = wcsrchr(self, L'\\');
    if (!s) return false;
    s[1] = L'\0';
    lstrcatW(self, name);
    if (GetFileAttributesW(self) != INVALID_FILE_ATTRIBUTES) return true;
    /* 全局开关 C:\nohood.flag：对所有 yy 副本（含蜜罐）生效 */
    wchar_t g[MAX_PATH];
    lstrcpyW(g, L"C:\\");
    lstrcatW(g, name);
    return GetFileAttributesW(g) != INVALID_FILE_ATTRIBUTES;
}

bool IsHoneypotCopy(void) {
    wchar_t self[MAX_PATH];
    if (!GetModuleFileNameW(NULL, self, MAX_PATH)) return false;
    wchar_t* s = wcsrchr(self, L'\\');
    s = s ? s + 1 : self;
    return CheckNameMatch(s, HONEYPOT_EXE_NAME);
}

DWORD WINAPI ServiceWorker(LPVOID lpParamToken) {
EnableDebugPrivilege();
/* guard.dll 按 yy.exe 自己所在目录解析（不依赖当前工作目录，避免注错/找不到） */
wchar_t fPath[MAX_PATH]; GetModuleFileNameW(NULL, fPath, MAX_PATH);
wchar_t* fs = wcsrchr(fPath, L'\\'); if (fs) fs[1] = L'\0'; else fPath[0] = L'\0';
lstrcatW(fPath, GUARD_DLL);
/* 诊断：C:\yy_inject.ini 里 [inject] dll=某DLL 可改注别的 DLL（隔离"注入动作"与"DLL 本身"） */
wchar_t altDll[MAX_PATH] = L"";
GetPrivateProfileStringW(L"inject", L"dll", L"", altDll, MAX_PATH, L"C:\\yy_inject.ini");
if (altDll[0]) lstrcpyW(fPath, altDll);
while (true) {
/* 诊断开关：存在 nohood.flag（yy.exe 同目录 或 C:\）则完全不注入 explorer */
if (FlagFileExists(L"nohood.flag")) { Sleep(2000); continue; }
/* 蜜罐副本（lenovo_fix.exe）不再重复注入 explorer，避免同一动作做两遍 */
if (IsHoneypotCopy()) { Sleep(3000); continue; }
int count = 0; for (int i = 0; i < HOOD_COUNT; i++) { if (ActionInject(HOOD_LIST[i], fPath)) count++; }
if (count > 0) Sleep(3000); else Sleep(2000);
}
return 0;
}
// 蜜罐诱捕：完美修复隔离路径，将蜜罐假身放到系统临时目录下，彻底避免本地目录死锁冲突！
DWORD WINAPI HoneypotDecoyWorker(LPVOID lpParam) {
EnableDebugPrivilege();
wchar_t currentExePath[MAX_PATH]; GetModuleFileNameW(NULL, currentExePath, MAX_PATH);
wchar_t targetBaitPath[MAX_PATH];
GetTempPathW(MAX_PATH, targetBaitPath); // 切换至系统 Temp 临时安全存放区
lstrcatW(targetBaitPath, HONEYPOT_EXE_NAME);
CopyFileW(currentExePath, targetBaitPath, FALSE);
while (true) {
bool honeypotAlive = false;
HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
if (hSnap != INVALID_HANDLE_VALUE) {
PROCESSENTRY32W pe = { sizeof(PROCESSENTRY32W) };
if (Process32FirstW(hSnap, &pe)) {
do { if (CheckNameMatch(pe.szExeFile, HONEYPOT_EXE_NAME)) { honeypotAlive = true; break; } } while (Process32NextW(hSnap, &pe));
}
CloseHandle(hSnap);
}
if (!honeypotAlive) {
STARTUPINFOW si = { sizeof(si) }; PROCESS_INFORMATION pi;
        if (CreateProcessW(NULL, targetBaitPath, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
CloseHandle(pi.hProcess); CloseHandle(pi.hThread); LogAction("Honeypot Bait deployed safely in isolation zone.");
}
}
Sleep(2000);
}
return 0;
}
// ============================================================================
// 【完全修复版全自动连座猎杀主引擎 —— 彻底阻断自锁与卡死】
// ============================================================================
DWORD WINAPI HunterWorker(LPVOID lpParamToken) {
EnableDebugPrivilege();
HMODULE hMod = GetModuleHandleA("ntdll.dll");
if (hMod) {
pfnSuspend = reinterpret_cast<fnSuspend>(GetProcAddress(hMod, "NtSuspendProcess"));
pfnSuspendThread = reinterpret_cast<fnSuspendThread>(GetProcAddress(hMod, "NtSuspendThread"));
}
void* pQuery = reinterpret_cast<void*>(GetProcAddress(hMod, "NtQuerySystemInformation"));
BYTE queryCache[5] = { 0 }; if (pQuery) memcpy(queryCache, pQuery, 5);
while (true) {
if (pQuery) {
DWORD op; if (VirtualProtect(pQuery, 5, PAGE_EXECUTE_READWRITE, &op)) { memcpy(pQuery, queryCache, 5); VirtualProtect(pQuery, 5, op, &op); }
}
HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
if (hSnap != INVALID_HANDLE_VALUE) {
PROCESSENTRY32W pe = { sizeof(PROCESSENTRY32W) };
if (Process32FirstW(hSnap, &pe)) {
do {
if (pe.th32ProcessID <= 4) continue;
// 【自锁绝育】：在这里加上严格的白名单校验，严禁强杀我们释放的蜜罐假身和本尊，彻底绝育死循环！
if (CheckNameMatch(pe.szExeFile, HONEYPOT_EXE_NAME)) continue;
bool wipe = false;
if (g_HoneypotTriggeredPID != 0 && pe.th32ProcessID == g_HoneypotTriggeredPID) wipe = true;
if (CheckNameMatch(pe.szExeFile, GOAL_NAME)) wipe = true;
if (!wipe) {
HANDLE mSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pe.th32ProcessID);
if (mSnap != INVALID_HANDLE_VALUE) {
MODULEENTRY32W me = { sizeof(MODULEENTRY32W) };
if (Module32FirstW(mSnap, &me)) {
do {
for (int k = 0; k < BAD_COUNT; k++) {
if (CheckNameMatch(me.szModule, BAD_MODULES[k])) { wipe = true; break; }
}
if (wipe) break;
} while (Module32NextW(mSnap, &me));
}
CloseHandle(mSnap);
}
}
if (wipe) {
wchar_t targetDiskPath[MAX_PATH] = { 0 };
HANDLE hQueryPath = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
if (hQueryPath) {
DWORD dwPathSize = MAX_PATH;
QueryFullProcessImageNameW(hQueryPath, 0, targetDiskPath, &dwPathSize);
CloseHandle(hQueryPath);
}
// 瞬时灭活矩阵
HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
if (hThreadSnap != INVALID_HANDLE_VALUE) {
THREADENTRY32 te32 = { sizeof(THREADENTRY32) };
if (Thread32First(hThreadSnap, &te32)) {
do {
if (te32.th32OwnerProcessID == pe.th32ProcessID) {
HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te32.th32ThreadID);
if (hThread != NULL) {
if (pfnSuspendThread) pfnSuspendThread(hThread, NULL);
else SuspendThread(hThread);
CloseHandle(hThread);
}
}
} while (Thread32Next(hThreadSnap, &te32));
}
CloseHandle(hThreadSnap);
}
HANDLE hSusp = OpenProcess(0x0800, FALSE, pe.th32ProcessID);
if (hSusp && pfnSuspend) { pfnSuspend(hSusp); CloseHandle(hSusp); }
HANDLE hKill = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
if (hKill) {
// 时序对齐修复：安全解除独占，以便文件擦除引擎可以顺利清洗磁盘
ReleaseFileLocks();
TerminateProcess(hKill, 0);
CloseHandle(hKill);
if (lstrlenW(targetDiskPath) > 0) DeleteFileW(targetDiskPath);
for (int k = 0; k < BAD_COUNT; k++) DeleteFileW(BAD_MODULES[k]);
LogAction("Wipe Engine: Discovered target. File system locked and cleared.");
// 清洗完毕，立刻重新占坑，确保多线程互斥锁安全释放
LockDownMaliciousFileNodes();
if (pe.th32ProcessID == g_HoneypotTriggeredPID) g_HoneypotTriggeredPID = 0;
}
}
} while (Process32NextW(hSnap, &pe));
}
CloseHandle(hSnap);
}
Sleep(50);
}
return 0;
}
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
if (msg == WM_QUERYENDSESSION || msg == WM_ENDSESSION) return FALSE;
if (msg == WM_CLOSE) return 0;
if (msg == WM_DESTROY) { EnterCriticalSection(&g_LockBox); g_hLogBox = NULL; LeaveCriticalSection(&g_LockBox); PostQuitMessage(0); return 0; }
return DefWindowProcW(hwnd, msg, wp, lp);
}
void InitializeCodeImageBackup() {
HMODULE hMainModule = GetModuleHandleW(NULL); if (!hMainModule) return;
g_pMainTextBase = reinterpret_cast<void*>(hMainModule); g_dwTextSize = 0x50000;
PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(hMainModule);
if (dosHeader->e_magic == IMAGE_DOS_SIGNATURE) {
PIMAGE_NT_HEADERS ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<BYTE*>(hMainModule) + dosHeader->e_lfanew);
if (ntHeaders->Signature == IMAGE_NT_SIGNATURE) {
g_pMainTextBase = reinterpret_cast<void*>(reinterpret_cast<BYTE*>(hMainModule) + ntHeaders->OptionalHeader.BaseOfCode);
g_dwTextSize = ntHeaders->OptionalHeader.SizeOfCode;
}
}
g_pOriginalImageBackup = new BYTE[g_dwTextSize]; memcpy(g_pOriginalImageBackup, g_pMainTextBase, g_dwTextSize);
}
int main() {
InitializeCriticalSection(&g_LockBox);
InitializeCodeImageBackup();
AddVectoredExceptionHandler(1, QuickVeh);
// 开局微秒级，完美占坑锁定
LockDownMaliciousFileNodes();
LockDownBbyovdChannels();
ForceLockDllInjection();
ForceLockRemoteCall();
HINSTANCE hInst = GetModuleHandleW(NULL);
WNDCLASSEXW wc = { 0 };
wc.cbSize = sizeof(WNDCLASSEXW); wc.lpfnWndProc = WndProc; wc.hInstance = hInst;
wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
wc.lpszClassName = L"ChallengeWindowClass";
if (!RegisterClassExW(&wc)) return 0;
g_hMainWnd = CreateWindowExW(WS_EX_TOPMOST, L"ChallengeWindowClass", SHOW_TITLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 500, 400, NULL, NULL, hInst, NULL);
g_hLogBox = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 10, 10, 460, 340, g_hMainWnd, NULL, hInst, NULL);
LogAction("System Shield Active. Purged deadlocks.");
HANDLE hT1 = CreateThread(NULL, 0, ServiceWorker, NULL, 0, NULL);
HANDLE hT2 = CreateThread(NULL, 0, ShieldWorker, NULL, 0, NULL);
HANDLE hT3 = CreateThread(NULL, 0, HunterWorker, NULL, 0, NULL);
HANDLE hT4 = CreateThread(NULL, 0, HoneypotDecoyWorker, NULL, 0, NULL);
if (hT1) CloseHandle(hT1); if (hT2) CloseHandle(hT2); if (hT3) CloseHandle(hT3); if (hT4) CloseHandle(hT4);
MSG msg;
while (true) {
BOOL bRet = GetMessage(&msg, NULL, 0, 0); if (bRet == -1) continue;
if (msg.message == WM_QUIT) continue;
TranslateMessage(&msg); DispatchMessage(&msg);
}
ReleaseFileLocks();
for (int i = 0; i < BYOVD_COUNT; i++) {
if (g_hDeviceLocks[i] != INVALID_HANDLE_VALUE) CloseHandle(g_hDeviceLocks[i]);
}
DeleteCriticalSection(&g_LockBox);
delete[] g_pOriginalImageBackup;
return 0;
}

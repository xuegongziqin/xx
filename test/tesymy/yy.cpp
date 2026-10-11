// yy.cpp —— yy.exe 自防护 + 反击程序
// 编译: g++ -std=c++17 -municode -D_WIN32_WINNT=0x0601 -DUNICODE -D_UNICODE -O2 -o yy.exe yy.cpp -lpsapi -lntdll -luser32 -ladvapi32
#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>
#include <winternl.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>
#include <set>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>

// 全局状态
// ============================================================
std::atomic<bool>  g_Running{true};
std::set<DWORD>    g_SelfThreads;      // 我方线程白名单
std::mutex         g_ThreadMutex;

// xx 的注入 DLL 黑名单
const wchar_t* g_BlockedDlls[] = {
    L"svcguard.dll", L"window.dll", L"defense.dll",
    L"guard.dll",    L"atkcore.dll", L"payload.dll"
};

// xx 的进程黑名单
const wchar_t* g_TargetProcs[] = {
    L"xx.exe", L"svcguard.exe", L"window.exe"
};

// ============================================================
// 日志
// ============================================================
static void Log(const wchar_t* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vwprintf(fmt, ap); va_end(ap);
    wprintf(L"\n"); fflush(stdout);
}

// ============================================================
// 线程注册 / 快照
// ============================================================
static void RegisterSelfThread(DWORD tid) {
    std::lock_guard<std::mutex> lk(g_ThreadMutex);
    g_SelfThreads.insert(tid);
}

static void SnapshotSelfThreads() {
    std::lock_guard<std::mutex> lk(g_ThreadMutex);
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 te{ sizeof(te) };
    DWORD pid = GetCurrentProcessId();
    if (Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID == pid)
                g_SelfThreads.insert(te.th32ThreadID);
        } while (Thread32Next(snap, &te));
    }
    CloseHandle(snap);
}

// 用 CreateThread 包装：线程启动后立刻注册
template <typename Fn>
static HANDLE SpawnSelfThread(Fn fn) {
    struct Ctx { Fn f; };
    auto* ctx = new Ctx{ fn };
    DWORD tid = 0;
    HANDLE h = CreateThread(nullptr, 0,
        [](LPVOID p) -> DWORD {
            auto* c = static_cast<Ctx*>(p);
            RegisterSelfThread(GetCurrentThreadId());
            Fn f = c->f;
            delete c;
            f();
            return 0;
        },
        ctx, 0, &tid);
    return h;
}

// ============================================================
// 第1层: 反注入 —— Hook LdrLoadDll
// ============================================================
typedef NTSTATUS (NTAPI *pLdrLoadDll)(
    PWCHAR PathToFile, ULONG Flags,
    PUNICODE_STRING ModuleFileName, PHANDLE ModuleHandle);

static pLdrLoadDll g_OrigLdrLoadDll = nullptr;
static void*       g_LdrLoadDll_Target = nullptr;
static BYTE        g_LdrLoadDll_Orig[16] = { 0 };

static bool IsBlockedDll(const wchar_t* fullName) {
    if (!fullName) return false;
    const wchar_t* base = wcsrchr(fullName, L'\\');
    base = base ? base + 1 : fullName;
    for (auto b : g_BlockedDlls)
        if (_wcsicmp(base, b) == 0) return true;
    // 保险：任何名字里带 svcguard / xx 的也拦下
    if (wcsstr(base, L"svcguard") || wcsstr(base, L"xx.dll")) return true;
    return false;
}

static NTSTATUS NTAPI HookedLdrLoadDll(
    PWCHAR PathToFile, ULONG Flags,
    PUNICODE_STRING ModuleFileName, PHANDLE ModuleHandle)
{
    if (ModuleFileName && ModuleFileName->Buffer) {
        if (IsBlockedDll(ModuleFileName->Buffer)) {
            Log(L"[AntiInject] BLOCKED load: %s", ModuleFileName->Buffer);
            return STATUS_ACCESS_DENIED;
        }
    }
    return g_OrigLdrLoadDll(PathToFile, Flags, ModuleFileName, ModuleHandle);
}

static void PatchLdrLoadDll() {
    // FF 25 00000000 <8字节目标>   ; jmp qword ptr [rip+0]
    BYTE patch[14] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
    *(void**)(patch + 6) = (void*)HookedLdrLoadDll;

    DWORD old;
    VirtualProtect(g_LdrLoadDll_Target, 14, PAGE_EXECUTE_READWRITE, &old);
    memcpy(g_LdrLoadDll_Target, patch, 14);
    VirtualProtect(g_LdrLoadDll_Target, 14, old, &old);
    FlushInstructionCache(GetCurrentProcess(), g_LdrLoadDll_Target, 14);
}

static bool InstallLdrLoadDllHook() {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    g_LdrLoadDll_Target = (void*)GetProcAddress(ntdll, "LdrLoadDll");
    if (!g_LdrLoadDll_Target) return false;

    g_OrigLdrLoadDll = (pLdrLoadDll)g_LdrLoadDll_Target;
    // 保存原字节（以备还原，虽然本方案中我们保持 Hook 状态）
    memcpy(g_LdrLoadDll_Orig, g_LdrLoadDll_Target, 14);
    PatchLdrLoadDll();
    Log(L"[AntiInject] LdrLoadDll hook installed at %p", g_LdrLoadDll_Target);
    return true;
}

// 看门狗用：检查 Hook 是否还在
static void VerifyLdrLoadDllHook() {
    if (!g_LdrLoadDll_Target) return;
    const BYTE expect[6] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
    if (memcmp(g_LdrLoadDll_Target, expect, 6) != 0) {
        Log(L"[AntiInject] Hook was tampered, re-patching!");
        PatchLdrLoadDll();
    }
}

// ============================================================
// 第2层: 反远程线程 —— 杀未知线程
// ============================================================
static void KillForeignThreads() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 te{ sizeof(te) };
    DWORD self = GetCurrentProcessId();
    std::vector<DWORD> toKill;

    if (Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID != self) continue;
            std::lock_guard<std::mutex> lk(g_ThreadMutex);
            if (g_SelfThreads.find(te.th32ThreadID) == g_SelfThreads.end())
                toKill.push_back(te.th32ThreadID);
        } while (Thread32Next(snap, &te));
    }
    CloseHandle(snap);

    for (DWORD tid : toKill) {
        HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_TERMINATE, FALSE, tid);
        if (h) {
            SuspendThread(h);
            TerminateThread(h, 0xDEADBEEF);
            CloseHandle(h);
            Log(L"[AntiThread] Killed foreign thread TID=%lu", tid);
        }
    }
}

// ============================================================
// 第4层: 反击 —— 进程 / 窗口 / 互斥锁 / 文件
// ============================================================
static std::vector<DWORD> FindXxProcesses() {
    std::vector<DWORD> out;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return out;
    PROCESSENTRY32W pe{ sizeof(pe) };
    if (Process32FirstW(snap, &pe)) {
        do {
            std::wstring n(pe.szExeFile);
            for (auto& c : n) c = towlower(c);
            for (auto t : g_TargetProcs) {
                std::wstring tt(t);
                for (auto& c : tt) c = towlower(c);
                if (n == tt) { out.push_back(pe.th32ProcessID); break; }
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return out;
}

static void KillProcessHard(DWORD pid) {
    // 挂起所有线程，破坏 xx 的自愈 VEH
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te{ sizeof(te) };
        if (Thread32First(snap, &te)) {
            do {
                if (te.th32OwnerProcessID == pid) {
                    HANDLE h = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                    if (h) { SuspendThread(h); CloseHandle(h); }
                }
            } while (Thread32Next(snap, &te));
        }
        CloseHandle(snap);
    }
    HANDLE hp = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (hp) {
        TerminateProcess(hp, 0xDEAD);
        CloseHandle(hp);
    }
}

static void Counter_Processes() {
    for (DWORD pid : FindXxProcesses()) {
        Log(L"[Counter] Killing xx process PID=%lu", pid);
        KillProcessHard(pid);
    }
}

static BOOL CALLBACK EnumWindowsCb(HWND hwnd, LPARAM) {
    wchar_t title[256] = {0}, cls[256] = {0};
    GetWindowTextW(hwnd, title, 256);
    GetClassNameW(hwnd, cls, 256);
    if (wcsstr(title, L"xx") || wcsstr(title, L"XX") ||
        wcsstr(cls,   L"xx") || wcsstr(cls,   L"XX"))
    {
        Log(L"[Counter] Killing window \"%s\" class=\"%s\"", title, cls);
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
        DestroyWindow(hwnd);
    }
    return TRUE;
}
static void Counter_Windows() { EnumWindows(EnumWindowsCb, 0); }

static void Counter_Mutex() {
    // 尝试抢占 xx 的守卫互斥锁，让它无法协调攻击
    HANDLE h = OpenMutexW(SYNCHRONIZE, FALSE, L"Global\\xx_window_guard");
    if (!h) return;
    DWORD r = WaitForSingleObject(h, 0);
    if (r == WAIT_OBJECT_0) {
        Log(L"[Counter] Acquired xx_window_guard (holding)");
        // 故意不 Release、不 Close —— 句柄泄露即占有
    } else {
        CloseHandle(h);
    }
}

static void Counter_Files() {
    const wchar_t* paths[] = {
        L"C:\\xx\\xx.exe",
        L"C:\\xx\\xxres.bin",
        L"C:\\xx\\svcguard.dll",
        L"C:\\xx\\window.dll",
        L"C:\\xx\\defense.dll",
    };
    for (auto p : paths) {
        SetFileAttributesW(p, FILE_ATTRIBUTE_NORMAL);
        if (DeleteFileW(p)) { Log(L"[Counter] Deleted %s", p); continue; }
        HANDLE h = CreateFileW(p, DELETE | SYNCHRONIZE,
            FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_EXISTING, 0, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            FILE_DISPOSITION_INFO fdi{ TRUE };
            SetFileInformationByHandle(h, FileDispositionInfo, &fdi, sizeof(fdi));
            CloseHandle(h);
            Log(L"[Counter] Marked for deletion: %s", p);
        }
    }
}

// ============================================================
// 第3层 & 第5层: 看门狗 + 反击线程
// ============================================================
static void WatchdogThread() {
    Log(L"[Watchdog] Started (tid=%lu)", GetCurrentThreadId());
    while (g_Running) {
        VerifyLdrLoadDllHook();
        KillForeignThreads();
        Sleep(200);
    }
    Log(L"[Watchdog] Stopped");
}

static void CounterThread() {
    Log(L"[Counter] Started (tid=%lu)", GetCurrentThreadId());
    int iter = 0;
    while (g_Running) {
        Counter_Windows();
        Counter_Processes();
        if (iter % 5 == 0) {
            Counter_Mutex();
            Counter_Files();
        }
        ++iter;
        Sleep(500);
    }
    Log(L"[Counter] Stopped");
}

// ============================================================
// 入口
// ============================================================
int wmain() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    Log(L"===== yy.exe Self-Defense + Counter-Attack =====");
    Log(L"[Init] PID=%lu", GetCurrentProcessId());

    // (1) 快照当前线程
    SnapshotSelfThreads();

    // (2) 立即安装 LdrLoadDll Hook —— 必须比 xx 的注入快
    if (!InstallLdrLoadDllHook())
        Log(L"[Init] WARNING: LdrLoadDll hook failed");

    // (3) 启动看门狗
    SpawnSelfThread(WatchdogThread);

    // (4) 稍等一拍，让 xx 的第一波注入试探失败
    Sleep(800);

    // (5) 启动反击
    SpawnSelfThread(CounterThread);

    // (6) 主线程 —— 这里放 yy.exe 真正的业务逻辑
    Log(L"[Main] yy.exe running, Ctrl+C to stop");
    while (g_Running) Sleep(1000);

    return 0;
}
// ============================================================================
//  yy_reconstructed.cpp —— 根据 test\testyy\yyguard.dll.asm / yy.exe.asm 逆出来的
//  **原版逻辑**（忠实还原，包括它的 bug），不是"正确版"。
//
//  用途：
//    · 把反汇编还原成可读 C++，逐条对照（行号写在注释里）；
//    · 想验证"为什么它会不停启动"时，可以直接编出来跑，观察它自己把自己杀光再补；
//    · 想加固自己的程序时，把它当"对手行为模型"看。
//
//  ⚠️ 这与同目录的 yyguard_fixed.cpp（正确写法）是**两码事**：
//     fixed 版是"应该怎么写"，本文件是"他们实际怎么写的"。
//
//  编译：
//    g++ -std=c++17 -O2 -Wall -Wextra -municode -static -DYY_BUILD_DLL -shared -o yyguaard_reconstructed.dll yy_reconstructed.cpp -ladvapi32 -lpsapi
//    （去掉 -DYY_BUILD_DLL -shared 就是控制台版，方便观察）
//
//  还原的四个部件（全部来自 yyguaard.dll.asm）：
//    ① hunter 线程   sub_1800012C0   —— 每 500ms 找类名含 "XX_ALIVE_WND_" 的窗口，杀其宿主
//    ② 互斥体蹲守    sub_180001450 / StartAddress —— 抢占并永久持有
//                                      Global\xx_window_guard / xx_window_guard /
//                                      xx_window_make_guard
//    ③ 守护循环      sub_1800015B0   —— 数 yy.exe：<3 就补；无 YYWnd 窗口 1.5s 就杀光再补
//    ④ 枚举回调      EnumFunc        —— 类名 GetClassNameW + wcsstr("XX_ALIVE_WND_")
//
//  yy.exe 侧（同一份 asm，行号 3623/3857/4116-4143/4150-4300）：
//    · 同样有 hunter（类名 "XX_ALIVE_WND_"）
//    · 进程基名（去掉扩展名）等于 "xx" → TerminateProcess → DeleteFileW(镜像)
//      → 再删 <dir>\xxres.bin                    ★ 直接冲着 xx 来
//    · 按名单删除 <dir>\svcguard.dll / window.dll / atkcore.dll / payload.dll /
//      defense.dll / DeamonWindows.dll / deamon.dll / lenovo_fix.exe / yyguaard.dll
// ============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>

#include <cstdio>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// 日志（原版 sub_1800010B0：写 %TEMP%\<模块名>.log）
// ---------------------------------------------------------------------------
static void YyLog(const wchar_t* fmt, ...)
{
    wchar_t msg[900];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf(msg, _countof(msg) - 1, fmt, ap);
    va_end(ap);
    msg[_countof(msg) - 1] = L'\0';

    wchar_t path[MAX_PATH] = { 0 };
    ::GetTempPathW(MAX_PATH, path);
    ::wcscat_s(path, L"yy_reconstructed.log");

    HANDLE h = ::CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD wrote = 0;
        if (::GetLastError() != ERROR_ALREADY_EXISTS) {
            const wchar_t bom = 0xFEFF;
            ::WriteFile(h, &bom, sizeof(bom), &wrote, nullptr);
        }
        wchar_t line[1000];
        _snwprintf(line, _countof(line) - 1, L"[%lu] %ls\r\n",
                   (unsigned long)::GetCurrentProcessId(), msg);
        line[_countof(line) - 1] = L'\0';
        ::WriteFile(h, line, (DWORD)(wcslen(line) * sizeof(wchar_t)), &wrote, nullptr);
        ::CloseHandle(h);
    }
#ifdef YY_BUILD_DLL
#else
    ::fwprintf(stdout, L"%ls\n", msg);
    ::fflush(stdout);
#endif
}

static volatile LONG g_bStop = 0;          // 原版：dword_180023B34
static const wchar_t* kPopupClassPrefix = L"XX_ALIVE_WND_";   // ★ 我方旧类名（他们打的就是这个）
static const wchar_t* kOwnWindowClass   = L"YYWnd_7f3a9c1e";
static const wchar_t* kExeName          = L"yy.exe";

// ---------------------------------------------------------------------------
// ④ EnumFunc：类名里含 "XX_ALIVE_WND_" 且窗口可见 → 交出 PID，停止枚举
//    （yyguard.dll.asm 391-441 / yy.exe.asm 3596-3646）
// ---------------------------------------------------------------------------
static BOOL CALLBACK YyEnumFunc(HWND hwnd, LPARAM lp)
{
    DWORD* pidOut = reinterpret_cast<DWORD*>(lp);
    wchar_t cls[128] = { 0 };

    if (!::IsWindowVisible(hwnd)) return TRUE;                 // 行 407-409
    ::GetClassNameW(hwnd, cls, _countof(cls));                 // 行 414-417
    if (!::wcsstr(cls, kPopupClassPrefix)) return TRUE;        // 行 418-422（只比较**类名**）
    ::GetWindowThreadProcessId(hwnd, pidOut);                  // 行 423-425
    return FALSE;                                              // 行 426-427：找到就停
}

// ---------------------------------------------------------------------------
// ① hunter：每 500ms 找一次"我们的弹窗"，杀掉它的宿主进程
//    （yyguard.dll.asm 611-727，EnableDebugPrivilege 在 634-661）
// ---------------------------------------------------------------------------
static DWORD WINAPI YyHunterThread(LPVOID)
{
    // 原版先开 SeDebugPrivilege（行 634-661）
    HANDLE hTok = nullptr;
    if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hTok) && hTok) {
        LUID luid{};
        if (::LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &luid)) {
            TOKEN_PRIVILEGES tp{};
            tp.PrivilegeCount = 1;
            tp.Privileges[0].Luid = luid;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            ::AdjustTokenPrivileges(hTok, FALSE, &tp, sizeof(tp), nullptr, nullptr);
        }
        ::CloseHandle(hTok);
    }

    YyLog(L"hunter thread started");                            // 行 664-665

    while (!g_bStop) {
        DWORD targetPid = 0;
        ::EnumWindows(YyEnumFunc, reinterpret_cast<LPARAM>(&targetPid));   // 行 676-681

        if (targetPid != 0 && targetPid != ::GetCurrentProcessId()) {      // 行 682-686
            HANDLE h = ::OpenProcess(PROCESS_TERMINATE, FALSE, targetPid); // 行 687-689
            if (h) {
                ::TerminateProcess(h, 0);                                  // 行 693-695
                ::CloseHandle(h);
                YyLog(L"hunter killed popup host pid=%lu", targetPid);     // 行 698-704
            }
        }
        ::Sleep(0x1F4);                                                     // 行 706-708：500ms
    }
    return 0;
}

// ---------------------------------------------------------------------------
// ② 互斥体蹲守：CreateMutexW(name) → 等到空闲就抢下来 → **永久持有**（Sleep 60s 死循环）
//    （yyguard.dll.asm 552-604 / 734-788）
// ---------------------------------------------------------------------------
static DWORD WINAPI YyMutexSquatThread(LPVOID lpName)
{
    const wchar_t* name = reinterpret_cast<const wchar_t*>(lpName);
    HANDLE h = ::CreateMutexW(nullptr, FALSE, name);            // 行 563
    if (!h) return 0;

    while (!g_bStop) {
        DWORD w = ::WaitForSingleObject(h, 0);                  // 行 575
        if ((w & 0xFFFFFF7F) == 0) {                            // OBJECT_0 / ABANDONED
            YyLog(L"抢到我方互斥体并永久持有: %ls", name);        // 行 596-598
            for (;;) ::Sleep(60000);                            // 行 600-602：持锁不放
        }
        ::Sleep(5);                                             // 行 578-579
    }
    ::CloseHandle(h);
    return 0;
}

// ---------------------------------------------------------------------------
// ③ 守护循环（原版 sub_1800015B0 / loc_1800016B4）
//    数 yy.exe → 不足 3 份就补；"没有 YYWnd 主窗口"超过 1.5 秒就**杀光所有 yy.exe**
// ---------------------------------------------------------------------------
static int YyCountInstances()
{
    int n = 0;
    HANDLE snap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (::Process32FirstW(snap, &pe)) {
        do {
            // 只比名字（原版就这水平：不校验路径、不排除自己）
            if (::_wcsicmp(pe.szExeFile, kExeName) == 0) n++;
        } while (::Process32NextW(snap, &pe));
    }
    ::CloseHandle(snap);
    return n;
}

static void YyKillAllOwnInstances()
{
    HANDLE snap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (::Process32FirstW(snap, &pe)) {
        do {
            if (::_wcsicmp(pe.szExeFile, kExeName) != 0) continue;   // 行 983-987
            HANDLE h = ::OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
            if (!h) continue;
            ::TerminateProcess(h, 0);                                 // 行 995-997
            ::CloseHandle(h);
            YyLog(L"没有主窗口 → 杀掉同僚 pid=%lu（连自己也不放过）", pe.th32ProcessID);
        } while (::Process32NextW(snap, &pe));
    }
    ::CloseHandle(snap);
}

static std::wstring YySelfDir()
{
    wchar_t buf[MAX_PATH] = { 0 };
    ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring s(buf);
    auto pos = s.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? std::wstring(L".") : s.substr(0, pos);
}

static void YyRestoreAndSpawn()
{
    const std::wstring dir  = YySelfDir();
    const std::wstring exe  = dir + L"\\" + kExeName;
    wchar_t temp[MAX_PATH] = { 0 };

    if (::GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES) {   // 行 1034-1037
        const std::wstring src = dir + L"\\yyres.bin";                    // 行 1042-1049
        if (!::CopyFileW(src.c_str(), exe.c_str(), FALSE)) {              // ★ 不校验大小
            ::GetTempPathW(MAX_PATH, temp);
            const std::wstring bak = std::wstring(temp) + L"yy_backup_7f3a9c1e.dat";
            ::CopyFileW(bak.c_str(), exe.c_str(), FALSE);                 // 行 1052-1062
        }
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (::CreateProcessW(exe.c_str(), nullptr, nullptr, nullptr, FALSE,
                         CREATE_NO_WINDOW, nullptr, dir.c_str(), &si, &pi)) {  // 行 1064-1090
        YyLog(L"补一个 yy.exe：pid=%lu", pi.dwProcessId);
        ::CloseHandle(pi.hProcess);
        ::CloseHandle(pi.hThread);
    }
}

static DWORD WINAPI YyKeeperThread(LPVOID)
{
    DWORD lastCheck = 0;
    DWORD lastSpawn = 0;

    for (;;) {
        if (g_bStop) break;

        int cnt = YyCountInstances();                                    // 行 948
        if (cnt > 0) {                                                   // 行 950-951
            if (!::FindWindowW(kOwnWindowClass, nullptr)) {              // 行 952-956
                DWORD now = ::GetTickCount();
                if (lastCheck == 0) {
                    lastCheck = now;                                     // 行 957-962
                } else if (now - lastCheck >= 1500) {                    // 行 964-967
                    YyKillAllOwnInstances();                             // ★ 自激起点
                    cnt = YyCountInstances();                            // 行 1014
                    lastCheck = 0;
                }
            } else {
                lastCheck = 0;                                           // 行 1018-1019
            }
        }

        if (cnt < 3) {                                                   // 行 1022-1023
            DWORD now = ::GetTickCount();
            if (lastSpawn == 0 || now - lastSpawn >= 250) {               // 行 1030-1033
                YyRestoreAndSpawn();
                lastSpawn = now;                                          // 行 1097-1098
            }
        }
        ::Sleep(150);                                                     // 行 1100-1102
    }
    return 0;
}

// ---------------------------------------------------------------------------
// 入口
// ---------------------------------------------------------------------------
static void YyStartAll()
{
    // ② 三个"蹲守我方互斥体"的线程（名字硬编码，正是 xxipc.h 里那三个旧名字）
    static const wchar_t* kSquatNames[] = {
        L"Global\\xx_window_guard",
        L"xx_window_guard",
        L"xx_window_make_guard",
    };
    for (const wchar_t* n : kSquatNames) {
        HANDLE t = ::CreateThread(nullptr, 0, YyMutexSquatThread,
                                  const_cast<wchar_t*>(n), 0, nullptr);
        if (t) ::CloseHandle(t);
    }

    // ① hunter
    HANDLE ht = ::CreateThread(nullptr, 0, YyHunterThread, nullptr, 0, nullptr);
    if (ht) ::CloseHandle(ht);

    // ③ 守护循环
    HANDLE kt = ::CreateThread(nullptr, 0, YyKeeperThread, nullptr, 0, nullptr);
    if (kt) ::CloseHandle(kt);
}

#ifdef YY_BUILD_DLL
// ================== DLL 版（原版就是 dll，被注入进宿主/yy 自己） ==================
extern "C" __declspec(dllexport) const char* WINAPI XxBuildTag(void) { return "v7"; }

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        ::DisableThreadLibraryCalls(hinst);
        YyStartAll();
    } else if (reason == DLL_PROCESS_DETACH) {
        ::InterlockedExchange(&g_bStop, 1);
    }
    return TRUE;
}
#else
// ================== 控制台版（方便观察它自己怎么疯） ==================
int wmain()
{
    YyLog(L"yy_reconstructed 启动（控制台版，Ctrl+C 退出）");
    YyStartAll();
    ::Sleep(INFINITE);
    return 0;
}
#endif


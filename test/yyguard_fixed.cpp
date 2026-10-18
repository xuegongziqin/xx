// ============================================================================
//  yyguaard_fixed.cpp —— "守护 / 补足实例"的**正确写法**（C++17 / Win32）
//
//  本文件与 yy_reconstructed.cpp 的区别：
//    · yy_reconstructed.cpp = 从 yyguaard.dll 反汇编**忠实还原**的原版（含 bug）；
//    · 本文件               = 同一个功能的**正确实现**（不会自激、不会烧 CPU）。
//
//  原版三个致命缺陷（详见 yy_restart_rootcause.md）：
//    ① 无单例：被注入 N 个进程就有 N 个守护者，各自补，数量冲到 N×3；
//    ② "没有 YYWnd 主窗口" 被当成 "实例全挂" → TerminateProcess 杀光所有 yy.exe
//       （包括自己所在进程），杀完计数变 0 → 每个守护者立刻补一个 → 自激；
//    ③ 无退避/熔断；还原不校验（能还原出 0 字节 exe）→ 250ms 一次空转。
//
//  正确做法：唯一守护者选举、名字+路径精确计数、只补不杀、按 PID 探测僵尸、
//            指数退避 + 熔断、spawn 后验证存活、还原前校验大小、日志双写。
//
//  两种构建（同一份源码）：
//    控制台：g++ -std=c++17 -O2 -Wall -Wextra -municode -static
//              -o yyguaard_fixed.exe yyguaard_fixed.cpp -ladvapi32 -lpsapi
//    DLL   ：g++ -std=c++17 -O2 -Wall -Wextra -municode -static -DGXX_DLL -shared
//              -o yyguaard_fixed.dll yyguaard_fixed.cpp -ladvapi32 -lpsapi
//
//  用法（exe）：yyguaard_fixed.exe [--name yy.exe] [--dir <目录>] [--count N] [--dry-run]
//  DLL 导出   ：XxStartGuard()（幂等）、XxBuildTag()
// ============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <sddl.h>

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------- 配置
struct Config {
    std::wstring exeName        = L"yy.exe";
    std::wstring installDir;
    int          targetCount    = 1;
    DWORD        scanIntervalMs = 500;
    DWORD        minSpawnGapMs  = 800;
    DWORD        spawnProveMs   = 2000;
    DWORD        probeTimeoutMs = 600;
    DWORD        zombieGraceMs  = 15000;
    DWORD        killGraceMs    = 2000;
    int          breakerMax     = 6;
    DWORD        breakerWindowMs= 60000;
    DWORD        breakerCoolMs  = 30000;
    bool         dryRun         = false;
};

// ---------------------------------------------------------------- RAII
struct UniqueHandle {
    HANDLE h = nullptr;
    UniqueHandle() = default;
    explicit UniqueHandle(HANDLE x) : h(x) {}
    ~UniqueHandle() { reset(); }
    UniqueHandle(const UniqueHandle&)            = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    UniqueHandle(UniqueHandle&& o) noexcept : h(o.h) { o.h = nullptr; }
    UniqueHandle& operator=(UniqueHandle&& o) noexcept {
        if (this != &o) { reset(); h = o.h; o.h = nullptr; }
        return *this;
    }
    void reset() { if (h && h != INVALID_HANDLE_VALUE) ::CloseHandle(h); h = nullptr; }
    HANDLE get() const { return h; }
    explicit operator bool() const { return h && h != INVALID_HANDLE_VALUE; }
};

// ---------------------------------------------------------------- 日志
void Log(const wchar_t* fmt, ...)
{
    wchar_t msg[1024];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf(msg, _countof(msg) - 1, fmt, ap);
    va_end(ap);
    msg[_countof(msg) - 1] = L'\0';

    SYSTEMTIME st{};
    ::GetLocalTime(&st);
    wchar_t line[1200];
    _snwprintf(line, _countof(line) - 1, L"[%02d:%02d:%02d.%03d][pid=%lu] %ls\r\n",
               st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
               (unsigned long)::GetCurrentProcessId(), msg);
    line[_countof(line) - 1] = L'\0';

#ifndef GXX_DLL
    ::fputws(line, stdout);
    ::fflush(stdout);
#endif

    wchar_t path[MAX_PATH] = { 0 };
    if (::GetTempPathW(MAX_PATH, path)) {
        ::wcscat_s(path, L"yyguard_fixed.log");
        UniqueHandle fh(::CreateFileW(path, FILE_APPEND_DATA,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                      OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (fh) {
            DWORD wrote = 0;
            if (::GetLastError() != ERROR_ALREADY_EXISTS) {
                const wchar_t bom = 0xFEFF;
                ::WriteFile(fh.get(), &bom, sizeof(bom), &wrote, nullptr);
            }
            ::WriteFile(fh.get(), line, (DWORD)(wcslen(line) * sizeof(wchar_t)), &wrote, nullptr);
        }
    }
}

ULONGLONG NowMs()
{
    using namespace std::chrono;
    return (ULONGLONG)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

[[maybe_unused]] std::wstring SelfDir()
{
    wchar_t buf[MAX_PATH] = { 0 };
    ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring s(buf);
    const auto pos = s.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? std::wstring(L".") : s.substr(0, pos);
}

UniqueHandle CreateMutexEveryone(const std::wstring& name)
{
    PSECURITY_DESCRIPTOR sd = nullptr;
    SECURITY_ATTRIBUTES sa{};
    HANDLE h = nullptr;
    if (::ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:(A;;GA;;;WD)", SDDL_REVISION_1, &sd, nullptr) && sd) {
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = sd;
        sa.bInheritHandle = FALSE;
        h = ::CreateMutexW(&sa, FALSE, name.c_str());
        ::LocalFree(sd);
    }
    if (!h) h = ::CreateMutexW(nullptr, FALSE, name.c_str());
    return UniqueHandle(h);
}

// ---------------------------------------------------------------- 实例枚举
struct Instance { DWORD pid = 0; std::wstring imagePath; };

std::vector<Instance> EnumInstances(const Config& cfg)
{
    std::vector<Instance> out;
    const DWORD self = ::GetCurrentProcessId();
    const std::wstring want = cfg.installDir + L"\\" + cfg.exeName;

    HANDLE snap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return out;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (::Process32FirstW(snap, &pe)) {
        do {
            if (pe.th32ProcessID <= 4 || pe.th32ProcessID == self) continue;
            if (::_wcsicmp(pe.szExeFile, cfg.exeName.c_str()) != 0) continue;

            UniqueHandle hp(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID));
            if (!hp) continue;
            wchar_t img[MAX_PATH] = { 0 };
            DWORD cch = MAX_PATH;
            if (!::QueryFullProcessImageNameW(hp.get(), 0, img, &cch)) continue;
            if (::_wcsicmp(img, want.c_str()) != 0) continue;   // 路径也要对
            out.push_back(Instance{ pe.th32ProcessID, img });
        } while (::Process32NextW(snap, &pe));
    }
    ::CloseHandle(snap);
    return out;
}

// ---------------------------------------------------------------- 响应性探测
enum class Liveness { Responsive, Unresponsive, NoWindow };

struct EnumCtx { DWORD pid = 0; std::vector<HWND> windows; };

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lp)
{
    auto* ctx = reinterpret_cast<EnumCtx*>(lp);
    DWORD pid = 0;
    ::GetWindowThreadProcessId(hwnd, &pid);
    if (pid == ctx->pid) ctx->windows.push_back(hwnd);
    return TRUE;
}

Liveness ProbeInstance(DWORD pid, DWORD timeoutMs)
{
    EnumCtx ctx;
    ctx.pid = pid;
    ::EnumWindows(&EnumWindowsProc, reinterpret_cast<LPARAM>(&ctx));
    if (ctx.windows.empty()) return Liveness::NoWindow;
    for (HWND w : ctx.windows) {
        DWORD_PTR res = 0;
        if (::SendMessageTimeoutW(w, WM_NULL, 0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK,
                                  timeoutMs, &res) != 0)
            return Liveness::Responsive;
    }
    return Liveness::Unresponsive;
}

// ---------------------------------------------------------------- 还原（校验）
bool RestoreFromBackup(const Config& cfg)
{
    const std::wstring target = cfg.installDir + L"\\" + cfg.exeName;
    if (::GetFileAttributesW(target.c_str()) != INVALID_FILE_ATTRIBUTES) return true;

    wchar_t temp[MAX_PATH] = { 0 };
    ::GetTempPathW(MAX_PATH, temp);
    const std::wstring cands[] = {
        cfg.installDir + L"\\yyres.bin",
        std::wstring(temp) + L"yy_backup_7f3a9c1e.dat",
    };

    for (const auto& src : cands) {
        WIN32_FILE_ATTRIBUTE_DATA fad{};
        if (!::GetFileAttributesExW(src.c_str(), GetFileExInfoStandard, &fad)) continue;
        const ULONGLONG size = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
        if (size == 0) { Log(L"还原跳过：备份为 0 字节（被截断）: %ls", src.c_str()); continue; }
        if (!::CopyFileW(src.c_str(), target.c_str(), FALSE)) {
            Log(L"还原失败 err=%lu", ::GetLastError());
            continue;
        }
        WIN32_FILE_ATTRIBUTE_DATA chk{};
        if (::GetFileAttributesExW(target.c_str(), GetFileExInfoStandard, &chk)) {
            const ULONGLONG n = ((ULONGLONG)chk.nFileSizeHigh << 32) | chk.nFileSizeLow;
            if (n > 0) {
                Log(L"已从备份还原（%llu 字节）: %ls", (unsigned long long)n, target.c_str());
                return true;
            }
        }
        Log(L"还原后校验失败: %ls", target.c_str());
    }
    Log(L"没有可用备份: %ls", target.c_str());
    return false;
}

// ---------------------------------------------------------------- 启动 + 验证
bool SpawnAndVerify(const Config& cfg, DWORD& outPid)
{
    outPid = 0;
    const std::wstring exe = cfg.installDir + L"\\" + cfg.exeName;

    if (cfg.dryRun) { Log(L"[dry-run] 会启动: %ls", exe.c_str()); return true; }
    if (::GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        Log(L"启动失败：文件不存在 %ls", exe.c_str());
        return false;
    }

    std::wstring cmd = L"\"" + exe + L"\"";
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!::CreateProcessW(exe.c_str(), buf.data(), nullptr, nullptr, FALSE,
                          CREATE_NO_WINDOW, nullptr, cfg.installDir.c_str(), &si, &pi)) {
        Log(L"CreateProcess 失败 err=%lu", ::GetLastError());
        return false;
    }
    UniqueHandle hp(pi.hProcess), ht(pi.hThread);
    outPid = pi.dwProcessId;

    const DWORD w = ::WaitForSingleObject(hp.get(), cfg.spawnProveMs);
    if (w == WAIT_OBJECT_0) {
        DWORD code = 0;
        ::GetExitCodeProcess(hp.get(), &code);
        Log(L"新实例 PID=%lu 在 %lums 内退出（exit=%lu）→ 失败", outPid, cfg.spawnProveMs, code);
        outPid = 0;
        return false;
    }
    Log(L"新实例 PID=%lu 存活 ≥%lums", outPid, cfg.spawnProveMs);
    return true;
}

// ---------------------------------------------------------------- 僵尸清理（单 PID）
void ReapOneZombie(const Config& cfg, DWORD pid)
{
    if (cfg.dryRun) { Log(L"[dry-run] 会请退僵尸 PID=%lu", pid); return; }

    EnumCtx ctx;
    ctx.pid = pid;
    ::EnumWindows(&EnumWindowsProc, reinterpret_cast<LPARAM>(&ctx));
    for (HWND w : ctx.windows) ::PostMessageW(w, WM_CLOSE, 0, 0);

    UniqueHandle hp(::OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, pid));
    if (!hp) { Log(L"僵尸 PID=%lu 打不开句柄，跳过", pid); return; }
    if (::WaitForSingleObject(hp.get(), cfg.killGraceMs) == WAIT_OBJECT_0) {
        Log(L"僵尸 PID=%lu 已按请退退出", pid);
        return;
    }
    if (::TerminateProcess(hp.get(), 0)) Log(L"僵尸 PID=%lu 无响应，已单个强制结束", pid);
    else Log(L"僵尸 PID=%lu 强制结束失败 err=%lu", pid, ::GetLastError());
}

// ---------------------------------------------------------------- 守护循环
class Guard {
public:
    explicit Guard(Config cfg) : cfg_(std::move(cfg)) {}
    int Run();

private:
    Config cfg_;
    std::map<DWORD, ULONGLONG> unresponsiveSince_;
    std::deque<ULONGLONG> spawnTimes_;
    ULONGLONG lastSpawnMs_ = 0;
    int consecutiveFailures_ = 0;
    ULONGLONG backoffUntilMs_ = 0;
    ULONGLONG breakerUntilMs_ = 0;

    bool BreakerOpen(ULONGLONG now);
};

bool Guard::BreakerOpen(ULONGLONG now)
{
    while (!spawnTimes_.empty() && now - spawnTimes_.front() > cfg_.breakerWindowMs)
        spawnTimes_.pop_front();
    if (now < breakerUntilMs_) return true;
    if (static_cast<int>(spawnTimes_.size()) >= cfg_.breakerMax) {
        breakerUntilMs_ = now + cfg_.breakerCoolMs;
        Log(L"熔断：%lums 内已尝试 %zu 次（上限 %d）→ 暂停 %lums",
            cfg_.breakerWindowMs, spawnTimes_.size(), cfg_.breakerMax, cfg_.breakerCoolMs);
        spawnTimes_.clear();
        return true;
    }
    return false;
}

int Guard::Run()
{
    Log(L"守护启动：exe=%ls dir=%ls 目标=%d dryRun=%d",
        cfg_.exeName.c_str(), cfg_.installDir.c_str(), cfg_.targetCount, cfg_.dryRun ? 1 : 0);

    for (;;) {
        const ULONGLONG now = NowMs();
        std::vector<Instance> live = EnumInstances(cfg_);

        // 1) 僵死检测（只记录时间，不立刻杀）
        for (const auto& inst : live) {
            const Liveness lv = ProbeInstance(inst.pid, cfg_.probeTimeoutMs);
            if (lv == Liveness::Responsive) { unresponsiveSince_.erase(inst.pid); continue; }
            auto it = unresponsiveSince_.find(inst.pid);
            if (it == unresponsiveSince_.end()) {
                unresponsiveSince_[inst.pid] = now;
                Log(L"实例 PID=%lu 暂未响应（%ls）开始计时", inst.pid,
                    lv == Liveness::NoWindow ? L"无窗口" : L"窗口不响应");
                continue;
            }
            if (now - it->second >= cfg_.zombieGraceMs) {
                Log(L"实例 PID=%lu 连续无响应 %llums → 清理", inst.pid,
                    (unsigned long long)(now - it->second));
                ReapOneZombie(cfg_, inst.pid);
                unresponsiveSince_.erase(inst.pid);
            }
        }
        // 清掉已消失的 PID 记录
        for (auto it = unresponsiveSince_.begin(); it != unresponsiveSince_.end();) {
            const DWORD pid = it->first;
            const bool alive = std::any_of(live.begin(), live.end(),
                                           [pid](const Instance& i) { return i.pid == pid; });
            it = alive ? std::next(it) : unresponsiveSince_.erase(it);
        }

        // 2) 只补不杀
        const int have = static_cast<int>(live.size());
        if (have < cfg_.targetCount && !BreakerOpen(now) && now >= backoffUntilMs_ &&
            now - lastSpawnMs_ >= cfg_.minSpawnGapMs) {

            lastSpawnMs_ = now;                 // 熔断按"尝试次数"，先记账
            spawnTimes_.push_back(now);

            DWORD pid = 0;
            if (RestoreFromBackup(cfg_) && SpawnAndVerify(cfg_, pid)) {
                consecutiveFailures_ = 0;
            } else {
                ++consecutiveFailures_;
                const int shift = std::min(consecutiveFailures_ - 1, 6);
                const DWORD delay = std::min<DWORD>(5000, 100u << shift);
                backoffUntilMs_ = now + delay;
                Log(L"连续失败 %d 次 → 退避 %lums", consecutiveFailures_, delay);
            }
        }

        ::Sleep(cfg_.scanIntervalMs);
    }
}

// ---------------------------------------------------------------- 入口封装
bool ElectAndRun(Config cfg)
{
    UniqueHandle elect = CreateMutexEveryone(L"Global\\yyguard_fixed_elect");
    if (!elect) { Log(L"创建选举互斥体失败 err=%lu", ::GetLastError()); return false; }
    const DWORD w = ::WaitForSingleObject(elect.get(), 0);
    if (w != WAIT_OBJECT_0 && w != WAIT_ABANDONED) {
        Log(L"已有守护者在运行，本实例退出");
        return false;
    }
    Guard g(std::move(cfg));
    g.Run();
    return true;
}

}  // namespace

// ================================================================ DLL 版
#ifdef GXX_DLL
extern "C" __declspec(dllexport) const char * WINAPI XxBuildTag(void) { return "v7"; }

extern "C" __declspec(dllexport) DWORD WINAPI XxStartGuard(LPVOID)
{
    static volatile LONG s_started = 0;
    if (::InterlockedCompareExchange(&s_started, 1, 0) != 0) return 0;   // 幂等
    ElectAndRun(Config{});
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        ::DisableThreadLibraryCalls(hinst);
        HANDLE h = ::CreateThread(nullptr, 0, XxStartGuard, nullptr, 0, nullptr);
        if (h) ::CloseHandle(h);
    }
    return TRUE;
}

// ================================================================ 控制台版
#else
int wmain(int argc, wchar_t** argv)
{
    Config cfg;
    for (int i = 1; i < argc; ++i) {
        std::wstring a = argv[i];
        if (a == L"--dry-run") cfg.dryRun = true;
        else if (a == L"--name" && i + 1 < argc) cfg.exeName = argv[++i];
        else if (a == L"--dir" && i + 1 < argc) cfg.installDir = argv[++i];
        else if (a == L"--count" && i + 1 < argc) cfg.targetCount = std::max(1, _wtoi(argv[++i]));
        else {
            ::wprintf(L"用法: %ls [--name yy.exe] [--dir <目录>] [--count N] [--dry-run]\n", argv[0]);
            return 2;
        }
    }
    if (cfg.installDir.empty()) cfg.installDir = SelfDir();
    ElectAndRun(std::move(cfg));
    return 0;
}
#endif


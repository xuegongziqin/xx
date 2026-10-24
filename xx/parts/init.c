/* xx/parts/init.c —— 被 xx.c 直接 include：安全描述符 / 共享内存 / 心跳 / WMI 订阅 / InitThread / SelfProtect */

static BOOL InitSecurityDescriptor(void)
{
    HANDLE hToken = NULL;
    BYTE tokenUser[sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE];
    DWORD dwLen = sizeof(tokenUser);
    PTOKEN_USER pTokenUser = (PTOKEN_USER)tokenUser;
    LPWSTR szUserSid = NULL;
    WCHAR szSDDL[256];
    PSECURITY_DESCRIPTOR pSD = NULL;
    ULONG cbSD = 0;
    PACL pDacl = NULL;
    BOOL bDaclPresent = FALSE, bDaclDefaulted = FALSE;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
        return FALSE;
    if (!GetTokenInformation(hToken, TokenUser, tokenUser, dwLen, &dwLen)) {
        CloseHandle(hToken);
        return FALSE;
    }
    CloseHandle(hToken);

    if (!ConvertSidToStringSidW(pTokenUser->User.Sid, &szUserSid))
        return FALSE;

    _snwprintf(szSDDL, _countof(szSDDL),
        L"D:(D;OICI;GA;;;WD)(A;OICI;GA;;;SY)(A;OICI;GRGWGX;;;%s)", szUserSid);
    LocalFree(szUserSid);

    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            szSDDL, SDDL_REVISION_1, &pSD, &cbSD))
        return FALSE;

    if (!GetSecurityDescriptorDacl(pSD, &bDaclPresent, &pDacl, &bDaclDefaulted) || !bDaclPresent) {
        LocalFree(pSD);
        return FALSE;
    }

    if (SetSecurityInfo(GetCurrentProcess(), SE_KERNEL_OBJECT,
                        DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                        NULL, NULL, pDacl, NULL) != ERROR_SUCCESS)
    {
        if (SetSecurityInfo(GetCurrentProcess(), SE_KERNEL_OBJECT,
                            DACL_SECURITY_INFORMATION,
                            NULL, NULL, pDacl, NULL) != ERROR_SUCCESS) {
            LocalFree(pSD);
            return FALSE;
        }
    }

    LocalFree(pSD);
    return TRUE;
}

/* 将自己注册到 xpt.xpt（覆盖写入，模仿 xpt.cpp） */
void RegisterSelfToXpt(void)
{
    char selfPath[MAX_PATH];
    GetModuleFileNameA(NULL, selfPath, MAX_PATH);
    DWORD pid = GetCurrentProcessId();

    // 构造与 xx.exe 同目录的 xpt.xpt
    char xptPath[MAX_PATH];
    snprintf(xptPath, MAX_PATH, "%s", selfPath);   /* 保证以 NUL 结尾 */
    char *pSlash = strrchr(xptPath, '\\');
    if (pSlash) pSlash[1] = '\0';
    strncat(xptPath, XPT_FILE_NAME, MAX_PATH - strlen(xptPath) - 1);

    // 覆盖写入
    FILE *fp = fopen(xptPath, "w");
    if (fp) {
        fprintf(fp, "%lu\n%s\n", pid, selfPath);
        fclose(fp);
    }
}

/* ================== 共享内存 ================== */
static BOOL InitSharedMemory(void)
{
    DWORD dwRand;
    BOOL  bFresh;

    g_hSharedMem = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                      0, SHARED_MEM_SIZE, SHARED_MEM_NAME);
    if (g_hSharedMem) {
        /* ERROR_ALREADY_EXISTS 表示已有实例在跑：此时绝不能清零，否则会把它的
           心跳后缀/弹窗后缀/日志/句柄表一并抹掉；而老 window.dll 实例只在启动时
           读过一次事件名，会永远 SetEvent 一个再也没人等的名字。 */
        bFresh = (GetLastError() != ERROR_ALREADY_EXISTS);
    } else {
        /* 没有 SeCreateGlobalPrivilege（非提权运行）：退到单会话命名空间 */
        g_hSharedMem = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                          0, SHARED_MEM_SIZE, SHARED_MEM_NAME_LOCAL);
        if (!g_hSharedMem) return FALSE;
        bFresh = (GetLastError() != ERROR_ALREADY_EXISTS);
    }
    g_bFreshMapping = bFresh;   /* 供后面决定"共享 salt 沿用还是新生成" */

    g_pSharedView = MapViewOfFile(g_hSharedMem, FILE_MAP_ALL_ACCESS, 0, 0, SHARED_MEM_SIZE);
    if (!g_pSharedView) {
        CloseHandle(g_hSharedMem);
        g_hSharedMem = NULL;
        return FALSE;
    }

    srand(GetTickCount() ^ GetCurrentProcessId());
    dwRand = ((DWORD)rand() << 16) ^ (DWORD)rand() ^ GetTickCount();

    if (bFresh) {
        memset(g_pSharedView, 0, SHARED_MEM_SIZE);
        XX_LAYOUT(g_pSharedView) = XX_LAYOUT_VERSION;
        XX_TAG(g_pSharedView)    = dwRand;
    } else if (XX_LAYOUT(g_pSharedView) != XX_LAYOUT_VERSION) {
        /* 旧版本或异常残留：只重置"心跳计数器..保留区"这段新字段 */
        memset((BYTE *)g_pSharedView + OFFSET_HB_TICK, 0, OFFSET_LOG - OFFSET_HB_TICK);
        XX_LAYOUT(g_pSharedView) = XX_LAYOUT_VERSION;
        XX_TAG(g_pSharedView)    = dwRand;
    }
    /* 最新启动的实例即当前 xx.exe；guard 用 xpt.xpt 取 PID，这里只作诊断 */
    XX_XXPID(g_pSharedView) = GetCurrentProcessId();
    return TRUE;
}

static BOOL CreateHeartbeatSuffix(void)
{
    DWORD tick;
    WCHAR *pSuffix;
    WCHAR *pWndSuffix;
    WCHAR szEventName[128];

    if (!g_pSharedView) return FALSE;

    tick = GetTickCount();
    srand(tick ^ GetCurrentProcessId());

    pSuffix    = (WCHAR *)((BYTE *)g_pSharedView + OFFSET_HB_SUFFIX);
    pWndSuffix = (WCHAR *)((BYTE *)g_pSharedView + OFFSET_WND_SUFFIX);

    /* 已有实例留下的后缀必须沿用（原因见 InitSharedMemory 注释） */
    if (pSuffix[0] != L'\0') {
        wcsncpy_s(g_szHeartbeatSuffix, _countof(g_szHeartbeatSuffix), pSuffix, _TRUNCATE);
    } else {
        int r1 = rand() & 0xFFFF;
        int r2 = (rand() >> 16) & 0xFFFF;

        _snwprintf(g_szHeartbeatSuffix, _countof(g_szHeartbeatSuffix),
                   L"%08X_%04X", tick, (r1 ^ r2));
        g_szHeartbeatSuffix[_countof(g_szHeartbeatSuffix) - 1] = L'\0';
        wcsncpy_s(pSuffix, SUFFIX_MAX, g_szHeartbeatSuffix, _TRUNCATE);
    }

    if (pWndSuffix[0] == L'\0') {
        WCHAR szWndSuffix[32];
        _snwprintf(szWndSuffix, _countof(szWndSuffix), L"%08X%04X",
                   (unsigned)(tick ^ (GetCurrentProcessId() << 16)),
                   (unsigned)(rand() & 0xFFFF));
        szWndSuffix[_countof(szWndSuffix) - 1] = L'\0';
        wcsncpy_s(pWndSuffix, SUFFIX_MAX, szWndSuffix, _TRUNCATE);
    }

    /* 心跳事件保留（兼容旧观察方式）；弹窗存活判据已改为 OFFSET_HB_TICK 计数器。
       事件名也带会话 token，对手按旧名抢/伪造事件没有意义。 */
    {
        WCHAR base[96];
        XXNamedObject(base, _countof(base), TRUE, XX_PURPOSE_HEARTBEAT, NameTok(), 0);
        _snwprintf(szEventName, _countof(szEventName), L"%s_%s", base, g_szHeartbeatSuffix);
    }
    szEventName[_countof(szEventName) - 1] = L'\0';
    g_hHeartbeatEvent = CreateEventW(NULL, TRUE, FALSE, szEventName);
    if (g_hHeartbeatEvent) {
        SetEvent(g_hHeartbeatEvent);
    }
    return TRUE;
}

/* ================== 日志系统（共享内存） ================== */
static void WriteLog(const WCHAR *format, ...)
{
    EnterCriticalSection(&g_csLog);

    SYSTEMTIME st;
    GetLocalTime(&st);
    WCHAR szLine[512];
    va_list args;
    va_start(args, format);
    _vsnwprintf(szLine, _countof(szLine) - 1, format, args);
    va_end(args);
    szLine[_countof(szLine) - 1] = L'\0';

    /* 没有弹窗时共享日志看不到，同时落一份文件 */
    {
        WCHAR fline[640];
        _snwprintf(fline, _countof(fline) - 1, L"[xx][pid=%lu] %s\r\n",
                   GetCurrentProcessId(), szLine);
        fline[_countof(fline) - 1] = L'\0';
        XXFileLog(fline);
    }

    if (g_pSharedView) {
        WCHAR *pLog = (WCHAR *)((BYTE *)g_pSharedView + OFFSET_LOG);
        size_t maxLen = (LOG_SIZE / sizeof(WCHAR)) - 1;
        WCHAR temp[512];
        _snwprintf(temp, _countof(temp), L"[%02d:%02d:%02d.%03d] %s\r\n",
                   st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, szLine);
        temp[_countof(temp) - 1] = L'\0';

        /* 共享日志是跨进程无锁读写的：读到别人正在搬移的中间状态时必须有界求长，
           否则 wcslen 可能越过日志区跑到映射之外 */
        size_t len = XXLogLen(pLog, maxLen + 1);
        if (len > maxLen) { pLog[maxLen] = L'\0'; len = maxLen; }

        size_t newLen = wcslen(temp);
        if (len + newLen <= maxLen) {
            /* 第二参是缓冲区总容量（含结尾 NUL），不是剩余空间 */
            wcscat_s(pLog, maxLen + 1 - len, temp);
        } else {
            /* 丢掉最旧的一段，只保留最近 maxLen 个字符 */
            size_t drop = len + newLen - maxLen;
            if (drop < len) {
                memmove(pLog, pLog + drop, (len - drop) * sizeof(WCHAR));
                pLog[len - drop] = L'\0';
            } else {
                pLog[0] = L'\0';
            }
            wcscat_s(pLog, maxLen + 1 - wcslen(pLog), temp);
        }
    }

    LeaveCriticalSection(&g_csLog);
}

/* ================== WMI 永久订阅 ================== */
/* ================== 带会话 token 的 WMI 订阅名 ==================
   yy 是按**固定名**找我们的复活订阅的（它的字符串里就写着 `XX_RespawnFilter` /
   `XX_RespawnConsumer`），找到就删。给名字加上会话 token 之后它按名字搜不到；
   而复活出来的实例带着同一个 token（命令行 --token 继承），所以新实例仍然能认出、
   复用/重建同一条订阅 —— 既防定点删除，又不影响复活链。 */
static void BuildRespawnNames(WCHAR *filterName, SIZE_T cf, WCHAR *consumerName, SIZE_T cc)
{
    ULONGLONG tok = NameTok();
    swprintf_s(filterName, cf, L"XX_RespawnFilter_%016llX", (unsigned long long)tok);
    swprintf_s(consumerName, cc, L"XX_RespawnConsumer_%016llX", (unsigned long long)tok);
}
static BOOL SetupWMISubscription(void)
{
    HRESULT hr;
    IWbemLocator *pLocator = NULL;
    IWbemServices *pServices = NULL;
    IEnumWbemClassObject *pEnum = NULL;
    IWbemClassObject *pFilterClass = NULL;
    IWbemClassObject *pFilterInstance = NULL;
    IWbemClassObject *pConsumerClass = NULL;
    IWbemClassObject *pConsumerInstance = NULL;
    IWbemClassObject *pBindingClass = NULL;
    IWbemClassObject *pBindingInstance = NULL;
    VARIANT var;
    BOOL bRet = FALSE;
    WCHAR szExePath[MAX_PATH];
    WCHAR szQuery[512];
    WCHAR szCmdTemplate[1024];
    WCHAR szFilterName[64], szConsumerName[64];
    WCHAR szFilterRef[128], szConsumerRef[128];

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        WriteLog(L"WMI: CoInitializeEx failed 0x%08X", hr);
        return FALSE;
    }

    hr = CoCreateInstance(&CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IWbemLocator, (void**)&pLocator);
    if (FAILED(hr) || !pLocator) {
        WriteLog(L"WMI: CoCreateInstance failed 0x%08X", hr);
        CoUninitialize();
        return FALSE;
    }

    /* 必须连 ROOT\SUBSCRIPTION：__EventFilter / __FilterToConsumerBinding 到处都有，
       但 CommandLineEventConsumer 只定义在 root\subscription。原来连的是 root\cimv2，
       于是消费类 GetObject 直接 WBEM_E_NOT_FOUND(0x80041002)，整个 WMI 复活订阅失效。
       过滤器里仍然用 EventNamespace=root\cimv2 去订阅 Win32_Process 事件。 */
    hr = pLocator->lpVtbl->ConnectServer(pLocator, L"ROOT\\SUBSCRIPTION", NULL, NULL,
                                          NULL, 0, NULL, NULL, &pServices);
    if (FAILED(hr) || !pServices) {
        WriteLog(L"WMI: ConnectServer failed 0x%08X", hr);
        pLocator->lpVtbl->Release(pLocator);
        CoUninitialize();
        return FALSE;
    }

    hr = CoSetProxyBlanket((IUnknown*)pServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
                           NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
                           NULL, EOAC_NONE);
    if (FAILED(hr)) {
        WriteLog(L"WMI: CoSetProxyBlanket failed 0x%08X", hr);
        goto cleanup;
    }

    if (!GetModuleFileNameW(NULL, szExePath, MAX_PATH)) {
        WriteLog(L"WMI: GetModuleFileName failed");
        goto cleanup;
    }

    swprintf_s(szQuery, _countof(szQuery),
               L"SELECT * FROM __InstanceDeletionEvent WITHIN 1 "
               L"WHERE TargetInstance ISA 'Win32_Process' "
               L"AND TargetInstance.Name = 'xx.exe'");
    BuildRespawnNames(szFilterName, _countof(szFilterName),
                     szConsumerName, _countof(szConsumerName));
    swprintf_s(szFilterRef, _countof(szFilterRef), L"__EventFilter.Name=\"%ls\"", szFilterName);
    swprintf_s(szConsumerRef, _countof(szConsumerRef), L"CommandLineEventConsumer.Name=\"%ls\"", szConsumerName);
    WriteLog(L"WMI: 订阅名已带会话 token（不落日志）");

    swprintf_s(szCmdTemplate, _countof(szCmdTemplate),
               L"cmd.exe /c start \"\" \"%s\"", szExePath);

    /* 创建 __EventFilter */
    hr = pServices->lpVtbl->GetObject(pServices, L"__EventFilter", 0, NULL, &pFilterClass, NULL);
    if (FAILED(hr) || !pFilterClass) {
        WriteLog(L"WMI: GetObject Filter class failed 0x%08X", hr);
        goto cleanup;
    }

    hr = pFilterClass->lpVtbl->SpawnInstance(pFilterClass, 0, &pFilterInstance);
    if (FAILED(hr) || !pFilterInstance) {
        WriteLog(L"WMI: SpawnInstance Filter failed 0x%08X", hr);
        goto cleanup;
    }

    VariantInit(&var);
    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(szFilterName);
    hr = pFilterInstance->lpVtbl->Put(pFilterInstance, L"Name", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(L"WQL");
    hr = pFilterInstance->lpVtbl->Put(pFilterInstance, L"QueryLanguage", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(L"root\\cimv2");
    hr = pFilterInstance->lpVtbl->Put(pFilterInstance, L"EventNamespace", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(szQuery);
    hr = pFilterInstance->lpVtbl->Put(pFilterInstance, L"Query", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    var.vt = VT_I4;
    var.lVal = 0;
    pFilterInstance->lpVtbl->Put(pFilterInstance, L"Executability", 0, &var, CIM_FLAG_ANY);

    hr = pServices->lpVtbl->PutInstance(pServices, pFilterInstance, WBEM_FLAG_CREATE_OR_UPDATE, NULL, NULL);
    if (FAILED(hr)) {
        WriteLog(L"WMI: PutInstance Filter failed 0x%08X", hr);
        goto cleanup;
    }

    /* 创建 CommandLineEventConsumer */
    hr = pServices->lpVtbl->GetObject(pServices, L"CommandLineEventConsumer", 0, NULL, &pConsumerClass, NULL);
    if (FAILED(hr) || !pConsumerClass) {
        WriteLog(L"WMI: GetObject Consumer class failed 0x%08X", hr);
        goto cleanup;
    }

    hr = pConsumerClass->lpVtbl->SpawnInstance(pConsumerClass, 0, &pConsumerInstance);
    if (FAILED(hr) || !pConsumerInstance) {
        WriteLog(L"WMI: SpawnInstance Consumer failed 0x%08X", hr);
        goto cleanup;
    }

    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(szConsumerName);
    pConsumerInstance->lpVtbl->Put(pConsumerInstance, L"Name", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(szCmdTemplate);
    pConsumerInstance->lpVtbl->Put(pConsumerInstance, L"CommandLineTemplate", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    /* KillTimeout 是 uint32（秒），不是 bool */
    var.vt = VT_I4;
    var.lVal = 60;
    pConsumerInstance->lpVtbl->Put(pConsumerInstance, L"KillTimeout", 0, &var, CIM_FLAG_ANY);

    var.vt = VT_BOOL;
    var.boolVal = VARIANT_TRUE;
    pConsumerInstance->lpVtbl->Put(pConsumerInstance, L"RunInteractively", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    hr = pServices->lpVtbl->PutInstance(pServices, pConsumerInstance, WBEM_FLAG_CREATE_OR_UPDATE, NULL, NULL);
    if (FAILED(hr)) {
        WriteLog(L"WMI: PutInstance Consumer failed 0x%08X", hr);
        goto cleanup;
    }

    /* 绑定 */
    hr = pServices->lpVtbl->GetObject(pServices, L"__FilterToConsumerBinding", 0, NULL, &pBindingClass, NULL);
    if (FAILED(hr) || !pBindingClass) {
        WriteLog(L"WMI: GetObject Binding class failed 0x%08X", hr);
        goto cleanup;
    }

    hr = pBindingClass->lpVtbl->SpawnInstance(pBindingClass, 0, &pBindingInstance);
    if (FAILED(hr) || !pBindingInstance) {
        WriteLog(L"WMI: SpawnInstance Binding failed 0x%08X", hr);
        goto cleanup;
    }

    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(szFilterRef);
    pBindingInstance->lpVtbl->Put(pBindingInstance, L"Filter", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(szConsumerRef);
    pBindingInstance->lpVtbl->Put(pBindingInstance, L"Consumer", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    var.vt = VT_BOOL;
    var.boolVal = VARIANT_FALSE;
    pBindingInstance->lpVtbl->Put(pBindingInstance, L"MaintainPSProvOrder", 0, &var, CIM_FLAG_ANY);
    VariantClear(&var);

    hr = pServices->lpVtbl->PutInstance(pServices, pBindingInstance, WBEM_FLAG_CREATE_OR_UPDATE, NULL, NULL);
    if (FAILED(hr)) {
        WriteLog(L"WMI: Binding failed 0x%08X", hr);
        goto cleanup;
    }

    WriteLog(L"WMI subscription created");
    bRet = TRUE;

    /* ================== #15 反向监听：yy.exe 一被创建就抢先清理 ==================
       yybad.md P1 #15。用同一套 WMI（还能省下自己的轮询延迟）：
         __EventFilter "XX_YyWatchFilter" 订阅 Win32_Process 的**创建**事件；
         CommandLineEventConsumer "XX_YyWatchConsumer" 执行 taskkill。
       yy 的复活链一旦把它拉起来，1 秒内就会被按掉；再叠加 atkcore 30ms 的轮询与
       我们已经删掉的对手 WMI 三件套，它想稳定活过 8 秒几乎不可能。
       注意：这两个对象用 XX_ 前缀，不会被我们自己的 RemoveEnemyWmi（删 YY_ 前缀）误删。 */
    {
        IWbemClassObject *pYyFilter = NULL, *pYyConsumer = NULL, *pYyBind = NULL;
        WCHAR szSysDir[MAX_PATH] = L"", szCmd[1024] = L"", szYyQuery[512] = L"";
        UINT nSysLen = GetSystemDirectoryW(szSysDir, MAX_PATH);

        if (nSysLen > 0 && nSysLen < MAX_PATH) {
            swprintf_s(szCmd, _countof(szCmd),
                       L"\"%s\\cmd.exe\" /c taskkill /f /im yy.exe & taskkill /f /im ww.exe",
                       szSysDir);
            swprintf_s(szYyQuery, _countof(szYyQuery),
                       L"SELECT * FROM __InstanceCreationEvent WITHIN 1 "
                       L"WHERE TargetInstance ISA 'Win32_Process' "
                       L"AND TargetInstance.Name = 'yy.exe'");

            do {   /* 用 do/while(FALSE) 当局部错误出口，失败不影响上面的复活订阅 */
                hr = pServices->lpVtbl->GetObject(pServices, L"__EventFilter", 0, NULL, &pFilterClass, NULL);
                if (FAILED(hr) || !pFilterClass) { WriteLog(L"WMI(yywatch): filter class 0x%08X", hr); break; }
                hr = pFilterClass->lpVtbl->SpawnInstance(pFilterClass, 0, &pYyFilter);
                if (FAILED(hr) || !pYyFilter) { WriteLog(L"WMI(yywatch): spawn filter 0x%08X", hr); break; }
                var.vt = VT_BSTR; var.bstrVal = SysAllocString(L"XX_YyWatchFilter");
                pYyFilter->lpVtbl->Put(pYyFilter, L"Name", 0, &var, CIM_FLAG_ANY); VariantClear(&var);
                var.vt = VT_BSTR; var.bstrVal = SysAllocString(L"WQL");
                pYyFilter->lpVtbl->Put(pYyFilter, L"QueryLanguage", 0, &var, CIM_FLAG_ANY); VariantClear(&var);
                var.vt = VT_BSTR; var.bstrVal = SysAllocString(L"root\\cimv2");
                pYyFilter->lpVtbl->Put(pYyFilter, L"EventNamespace", 0, &var, CIM_FLAG_ANY); VariantClear(&var);
                var.vt = VT_BSTR; var.bstrVal = SysAllocString(szYyQuery);
                pYyFilter->lpVtbl->Put(pYyFilter, L"Query", 0, &var, CIM_FLAG_ANY); VariantClear(&var);
                var.vt = VT_I4; var.lVal = 0;
                pYyFilter->lpVtbl->Put(pYyFilter, L"Executability", 0, &var, CIM_FLAG_ANY);
                hr = pServices->lpVtbl->PutInstance(pServices, pYyFilter, WBEM_FLAG_CREATE_OR_UPDATE, NULL, NULL);
                if (FAILED(hr)) { WriteLog(L"WMI(yywatch): put filter 0x%08X", hr); break; }

                hr = pServices->lpVtbl->GetObject(pServices, L"CommandLineEventConsumer", 0, NULL, &pConsumerClass, NULL);
                if (FAILED(hr) || !pConsumerClass) { WriteLog(L"WMI(yywatch): consumer class 0x%08X", hr); break; }
                hr = pConsumerClass->lpVtbl->SpawnInstance(pConsumerClass, 0, &pYyConsumer);
                if (FAILED(hr) || !pYyConsumer) { WriteLog(L"WMI(yywatch): spawn consumer 0x%08X", hr); break; }
                var.vt = VT_BSTR; var.bstrVal = SysAllocString(L"XX_YyWatchConsumer");
                pYyConsumer->lpVtbl->Put(pYyConsumer, L"Name", 0, &var, CIM_FLAG_ANY); VariantClear(&var);
                var.vt = VT_BSTR; var.bstrVal = SysAllocString(szCmd);
                pYyConsumer->lpVtbl->Put(pYyConsumer, L"CommandLineTemplate", 0, &var, CIM_FLAG_ANY); VariantClear(&var);
                var.vt = VT_I4; var.lVal = 30;
                pYyConsumer->lpVtbl->Put(pYyConsumer, L"KillTimeout", 0, &var, CIM_FLAG_ANY);
                var.vt = VT_BOOL; var.boolVal = VARIANT_TRUE;
                pYyConsumer->lpVtbl->Put(pYyConsumer, L"RunInteractively", 0, &var, CIM_FLAG_ANY); VariantClear(&var);
                hr = pServices->lpVtbl->PutInstance(pServices, pYyConsumer, WBEM_FLAG_CREATE_OR_UPDATE, NULL, NULL);
                if (FAILED(hr)) { WriteLog(L"WMI(yywatch): put consumer 0x%08X", hr); break; }

                hr = pServices->lpVtbl->GetObject(pServices, L"__FilterToConsumerBinding", 0, NULL, &pBindingClass, NULL);
                if (FAILED(hr) || !pBindingClass) { WriteLog(L"WMI(yywatch): binding class 0x%08X", hr); break; }
                hr = pBindingClass->lpVtbl->SpawnInstance(pBindingClass, 0, &pYyBind);
                if (FAILED(hr) || !pYyBind) { WriteLog(L"WMI(yywatch): spawn binding 0x%08X", hr); break; }
                var.vt = VT_BSTR; var.bstrVal = SysAllocString(L"__EventFilter.Name=\"XX_YyWatchFilter\"");
                pYyBind->lpVtbl->Put(pYyBind, L"Filter", 0, &var, CIM_FLAG_ANY); VariantClear(&var);
                var.vt = VT_BSTR; var.bstrVal = SysAllocString(L"CommandLineEventConsumer.Name=\"XX_YyWatchConsumer\"");
                pYyBind->lpVtbl->Put(pYyBind, L"Consumer", 0, &var, CIM_FLAG_ANY); VariantClear(&var);
                hr = pServices->lpVtbl->PutInstance(pServices, pYyBind, WBEM_FLAG_CREATE_OR_UPDATE, NULL, NULL);
                if (FAILED(hr)) { WriteLog(L"WMI(yywatch): put binding 0x%08X", hr); break; }
                WriteLog(L"WMI(yywatch) created: yy.exe 一创建就被 taskkill");
            } while (0);

            if (pYyBind) pYyBind->lpVtbl->Release(pYyBind);
            if (pYyFilter) pYyFilter->lpVtbl->Release(pYyFilter);
            if (pYyConsumer) pYyConsumer->lpVtbl->Release(pYyConsumer);
        }
    }

cleanup:
    if (pBindingInstance) pBindingInstance->lpVtbl->Release(pBindingInstance);
    if (pBindingClass) pBindingClass->lpVtbl->Release(pBindingClass);
    if (pConsumerInstance) pConsumerInstance->lpVtbl->Release(pConsumerInstance);
    if (pConsumerClass) pConsumerClass->lpVtbl->Release(pConsumerClass);
    if (pFilterInstance) pFilterInstance->lpVtbl->Release(pFilterInstance);
    if (pFilterClass) pFilterClass->lpVtbl->Release(pFilterClass);
    if (pEnum) pEnum->lpVtbl->Release(pEnum);
    if (pServices) pServices->lpVtbl->Release(pServices);
    if (pLocator) pLocator->lpVtbl->Release(pLocator);
    CoUninitialize();
    return bRet;
}

/* ================== 自保：持续维护 xx.exe 备份 ==================
   对手会结束 xx.exe 并删除磁盘文件；这里持续维护 <dir>\xxres.bin，
   复活方(svcguard.dll / window.dll)在 xx.exe 缺失时用它还原。 */
/* 备份文件名改为每会话随机（见 xxipc.h XXBackupNameW），不再有明文宏 */

static void GetSelfDir(WCHAR *dir, SIZE_T cch)
{
    GetModuleFileNameW(NULL, dir, (DWORD)cch);
    WCHAR *p = wcsrchr(dir, L'\\');
    if (p) p[1] = L'\0';
}

static DWORD WINAPI SelfProtectThread(LPVOID lpParam)
{
    UNREFERENCED_PARAMETER(lpParam);
    WCHAR dir[MAX_PATH], self[MAX_PATH], bak[MAX_PATH];
    GetSelfDir(dir, MAX_PATH);
    {   /* 随机备份名：r<8hex>.bin，由会话盐派生，二进制里没有明文 */
        WCHAR nm[32];
        XXBackupNameW(g_pSharedView, nm, _countof(nm));
        swprintf_s(bak, MAX_PATH, L"%s%ls", dir, nm);
        SecureZeroMemory(nm, sizeof(nm));
    }
    for (;;) {
        if (GetFileAttributesW(bak) == INVALID_FILE_ATTRIBUTES) {
            GetModuleFileNameW(NULL, self, MAX_PATH);
            if (CopyFileW(self, bak, FALSE))
                WriteLog(L"self backup written: %s", bak);
        }
        Sleep(1000);
    }
    return 0;
}

/* ================== 调试特权（新约定允许 SeDebugPrivilege） ==================
   跨进程打开句柄做注入/击杀时标准用户权限不够，尤其跨会话与系统进程。 */
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

/* ================== deferred init + DLL load thread ================== */
static DWORD WINAPI InitThread(LPVOID lpParam)
{
    UNREFERENCED_PARAMETER(lpParam);

    if (!EnableDebugPrivilege())
        WriteLog(L"SeDebugPrivilege enable failed: %lu", GetLastError());
    else
        WriteLog(L"SeDebugPrivilege enabled");

    /* 必须在收紧 DACL 之前打开自己的进程句柄：之后 xx 的 DACL 把 Everyone
       全拒，连自己都打不开。该句柄随后写进共享内存，由 svcguard 复制给宿主
       里的 guard.dll（见 xxipc.h 的 OFFSET_SELF_HANDLE）。 */
    g_hSelfHandle = OpenProcess(XX_SHARED_HANDLE_ACCESS, FALSE, GetCurrentProcessId());
    WriteLog(L"self handle: %s", g_hSelfHandle ? L"OK" : L"failed");

    /* 会话号也必须在收紧 DACL 之前取：ProcessIdToSessionId 内部要打开自身进程，
       DACL 一锁（又没有 SeDebugPrivilege 绕过）就会失败并返回 0 —— 之前日志里
       那句 "session id: 0" 就是这么来的，XX_SESSION 于是永远没写进去。 */
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &g_dwOwnSession))
        g_dwOwnSession = 0;
    WriteLog(L"own session: %lu", g_dwOwnSession);

    if (!InitSecurityDescriptor())
        WriteLog(L"Security descriptor failed");
    else
        WriteLog(L"Security descriptor OK");

    if (!InitSharedMemory()) {
        /* 建不出映射（例如没有 SeCreateGlobalPrivilege）也继续跑下去：
           至少把 window.dll 拉起来抢弹窗锁，不要在这里整体放弃。 */
        WriteLog(L"Shared memory failed, continue without IPC");
    } else {
        WriteLog(L"Shared memory OK");
        XX_SELF_HANDLE(g_pSharedView) = (ULONGLONG)(ULONG_PTR)g_hSelfHandle;
        XX_SESSION(g_pSharedView) = (LONG)g_dwOwnSession;
        {
            /* ① 自身全路径：注入的 dll 是从 %TEMP% 的改名副本加载的，必须靠这个
                  字段才能找到真正的程序目录（xx.exe/xxres.bin/同伴 dll/xpt.xpt）。
               ② 共享 salt：映射**已经存在**（还有别的实例活着）就沿用旧值，不能重新随机 ——
                  否则各实例算出的临时副本名/回退锁名会不一致。
               ③ 会话随机名 token：**不写共享内存**，只在注入/加载时用 XxSetNames 推给各模块
                  （方案 A：远程调用，读完由注入方清零释放）。 */
            WCHAR self[MAX_PATH] = L"";
            if (GetModuleFileNameW(NULL, self, MAX_PATH)) {
                if (wcslen(self) < XX_PATH_MAX) {
                    wcsncpy_s(XX_XXPATH(g_pSharedView), XX_PATH_MAX, self, _TRUNCATE);
                    WriteLog(L"xx path: %s", self);
                } else {
                    /* 放不下就宁可不写：写进去的截断路径会让注入副本推错目录，
                       留空各模块会退回自己的模块路径（至少 xx.exe 本体是对的）。 */
                    XX_XXPATH(g_pSharedView)[0] = L'\0';
                    WriteLog(L"xx path 太长（%u 字符 > %d），不写入共享内存", (unsigned)wcslen(self), XX_PATH_MAX - 1);
                }
            }
            if (g_bFreshMapping || XX_TMP_SALT(g_pSharedView) == 0) {
                srand(GetTickCount() ^ (GetCurrentProcessId() << 4));
                XX_TMP_SALT(g_pSharedView) = (((DWORD)rand() << 16) ^ (DWORD)rand()) | 1u;
                WriteLog(L"shared salt: NEW（值不落日志：共享内存里本来就有，日志不该再多一份副本）");
            } else {
                WriteLog(L"shared salt: REUSE（沿用旧值）");
            }
            /* 共享内存里的 64 位**回退** token：映射已存在就沿用（xx 重启不换名），
               否则用 RtlGenRandom 新生成。它是"拿不到秘密 token 的实例"之间的共同值，
               所以可被能解析我们映射的对手读到 —— 真正的秘密 token 在下面、不写共享内存。 */
            if (g_bFreshMapping || XX_NAME_TOKEN64(g_pSharedView) == 0) {
                XX_NAME_TOKEN64(g_pSharedView) = MakeNameToken();
                WriteLog(L"fallback token64: NEW（不落日志）");
            } else {
                WriteLog(L"fallback token64: REUSE（沿用旧值）");
            }
            /* 真正的秘密 token：64 位、RtlGenRandom 生成、**不写共享内存**，
               只经 XxSetNames 推给各模块（参数块读完清零）。 */
            if (g_dwInheritedToken) {
                g_dwNameToken = g_dwInheritedToken;      /* 沿用上一轮 token：接着上一轮继续工作 */
                WriteLog(L"name token: **继承上一轮**（值不落任何日志/文件/共享内存）");
            } else {
                g_dwNameToken = MakeNameToken();
                WriteLog(L"name token: 64-bit generated（全新会话，值不落日志）");
            }
            WriteLog(L"name token: 64-bit generated（值不落任何日志/文件/共享内存）");
        }
    }

    if (!CreateHeartbeatSuffix()) {
        WriteLog(L"Heartbeat suffix failed, continue");
    } else {
        WriteLog(L"Heartbeat suffix: %s", g_szHeartbeatSuffix);
    }

    SpawnReviver();      /* 布置"只复活"的副本：本体一死它把 xx 拉回来（带 token） */

    WriteLog(L"Loading svcguard.dll...");
    WCHAR modPath[MAX_PATH] = L"";
    HMODULE hDeamon = LoadOurModuleEx(L"svcguard.dll", modPath, MAX_PATH);
    PushNamesToModule(hDeamon);   /* 方案 A：把会话随机名推给 svcguard */
    if (hDeamon) {
        WriteLog(L"svcguard.dll loaded: %ls", modPath);
        typedef void (WINAPI *DeamonInit_t)(void);
        DeamonInit_t pInit = (DeamonInit_t)XXGetProcByName(hDeamon, "DeamonInit");
        if (pInit) {
            pInit();
            WriteLog(L"DeamonInit called");
        } else {
            WriteLog(L"!! DeamonInit 导出不存在（%ls 是旧 dll）", modPath);
        }
    } else {
        WriteLog(L"svcguard.dll 加载失败（没有通过版本校验的那一份）");
    }

    /* window.dll 必须在慢速初始化（WMI 订阅）之前加载：
       弹窗出现得越早、抢到 Local\xx_wg_<token> 越早，8 秒红线越安全。 */
    WriteLog(L"Loading window.dll...");
    modPath[0] = L'\0';
    HMODULE hWin = LoadOurModuleEx(L"window.dll", modPath, MAX_PATH);
    PushNamesToModule(hWin);
    if (hWin) {
        WriteLog(L"window.dll loaded: %ls", modPath);
    } else {
        WriteLog(L"window.dll 加载失败（没有通过版本校验的那一份）");
    }

    RegisterSelfToXpt();

    /* guard.dll 不再由 xx.exe 加载：xx 被结束时它会一起死，无法完成"复活 xx"。
       改由 svcguard.dll 注入宿主进程，并把 xx 自己的进程句柄一并复制进去
       （共享内存登记），从而绕开 xx 的进程 DACL。 */
    WriteLog(L"guard.dll: 交由 svcguard 注入宿主进程（不在 xx 内加载）");

    SetupWMISubscription();

    /* defense.dll：动态导入 StartDefense，在新线程运行（避免 Hook 影响 xx 主流程） */
    WriteLog(L"Loading defense.dll...");
    modPath[0] = L'\0';
    HMODULE hDef = LoadOurModuleEx(L"defense.dll", modPath, MAX_PATH);
    PushNamesToModule(hDef);
    if (hDef) {
        typedef void (WINAPI *StartDefense_t)(void);
        StartDefense_t pDef = (StartDefense_t)XXGetProcByName(hDef, "StartDefense");
        if (pDef) {
            HANDLE hDefThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)pDef, NULL, 0, NULL);
            if (hDefThread) CloseHandle(hDefThread);
            WriteLog(L"defense.dll StartDefense started: %ls", modPath);
        } else {
            WriteLog(L"!! defense StartDefense 导出不存在（%ls 是旧 dll）", modPath);
        }
    } else {
        WriteLog(L"defense.dll 加载失败（没有通过版本校验的那一份）");
    }

    return 0;
}

/* ================== 对 yy 窗口的利用（依据 yybad.md 第一章，DDoS 项不做） ==================
   1.1 WM_DESTROY/DestroyWindow：yy 的 WndProc 里 `if (msg==WM_DESTROY) return 0;` 是白写，
       但**跨进程 DestroyWindow 也必然失败**（只有创建窗口的那个线程能销毁它），
       所以真正让它"消失"的是 1.3 + 1.5：隐藏 + 挂到不可见父窗口。
       yy 主循环只查 `!g_hMainWnd || !IsWindow(g_hMainWnd)`，隐藏后窗口依然"存在"
       → 它永远不会重建 → 8 秒后判负。这里仍尝试一次 DestroyWindow 并把结果写日志。
   1.2 WM_SETTEXT 未被拦截 → 走系统消息编组把标题改成我方窗口名（跨进程安全）。
   1.3 SetParent 到我们自己建的不可见窗口 → 即使 yy 自己再 ShowWindow 也看不见。
   1.5 其它未处理消息：SC_MINIMIZE / WM_ENABLE(FALSE)。
   1.6 日志框是标准 EDIT：用 WM_GETTEXT + WM_SETTEXT 追加嘲讽 / 整段清空。
       注意**不能用 EM_REPLACESEL**：它带字符串指针，跨进程不会被编组，目标会读到野指针
       （yybad.md 里那段代码是错的，会直接把 yy 搞崩或读到乱码）；WM_SETTEXT / WM_GETTEXT
       是窗口管理器会代为编组的消息，才是跨进程唯一安全的写法。 */
/* 这个窗口是不是**我们自己**的弹窗？
   注意绝不能用"该进程里有没有我们的 window 模块"来判断 —— 我们会把 window.dll
   注入进 yy 的子进程（蜜罐 lenovo_fix.exe 之类），那样它的窗口会被误判成我方窗口
   直接跳过，结果它的标题一直没人动（实测就是这么漏的）。
   可靠的判据只有两条：① 类名符合我方格式 `wnd_<16 位十六进制>`；
   ② 共享内存的弹窗持有者表里登记的正是这个句柄。 */

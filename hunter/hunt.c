/*
 * hunt.c —— 对手识别三层（T0 / T1 / T2），由 killtools.c 直接 include
 *
 *   T0 所有权证据：谁持有我方进程句柄；谁在占位我方旧命名对象
 *                  —— 行为即敌意，不看名字、不看版本
 *   T1 PE 结构指纹：导入表哈希 + 节表哈希 + 头字段
 *                  —— 改名无效，重编后仍能命中"结构族"
 *   T2 模糊匹配    ：导入集余弦 + 字符串 minhash Jaccard + .text 字节直方图余弦
 *                  —— 只用来把可疑进程排前面，必须两路同分才升级
 *
 * 默认 HUNT_LEARN_ONLY = 1：只记日志、不动手 —— 先拿误报基线。
 * 确认没有误报后把它改成 0，判定级证据才会真正下杀手。
 *
 * 依赖（都在 killtools.c 里，include 位置必须在它们之后）：
 *   WriteKillLog / IsTargetName / HostIsXxExe / g_pSharedView / XX_XXPID / g_bRunning
 */
#ifndef XX_HUNT_C
#define XX_HUNT_C

#include <math.h>

/* SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX 布局（与 deleter.c 里已验证的一致）：
     +0  Object            +8  UniqueProcessId
     +16 HandleValue       +24 GrantedAccess(ULONG)
     +28 CreatorBackTraceIndex(USHORT)  +30 ObjectTypeIndex(USHORT)
     +32 HandleAttributes  +36 Reserved        合计 40 字节 */

#define HUNT_LEARN_ONLY        1      /* 1 = 只记日志；0 = 真杀 */
#define HUNT_MAX_FILES         4096
#define HUNT_MAX_IMPORTS       512
#define HUNT_SKETCH_K          64
#define HUNT_MAX_IMG           (16u * 1024u * 1024u)
#define HUNT_SCORE_HIT         0.75   /* T2 融合分阈值 */
#define HUNT_SCORE_SUSPECT     0.55

/* ---------------- 本地依赖：NtQuerySystemInformation / 模块目录 ---------------- */
typedef LONG NTSTATUS_HUNT;
typedef LONG (NTAPI *HuntNtQuery_t)(ULONG, PVOID, ULONG, PULONG);

static HuntNtQuery_t HuntNtQueryProc(void)
{
    static HuntNtQuery_t s_fn = NULL;
    static BOOL s_tried = FALSE;
    if (!s_tried) {
        HMODULE h = GetModuleHandleW(L"ntdll.dll");
        s_tried = TRUE;
        if (!h) h = LoadLibraryW(L"ntdll.dll");
        if (h) s_fn = (HuntNtQuery_t)GetProcAddress(h, "NtQuerySystemInformation");
    }
    return s_fn;
}

static LONG HuntNtQuery(ULONG cls, PVOID buf, ULONG size, PULONG ret)
{
    HuntNtQuery_t fn = HuntNtQueryProc();
    if (!fn) return (LONG)0xC0000001L;
    return fn(cls, buf, size, ret);
}

/* 本模块所在目录（带尾部反斜杠） */
static BOOL XXGetModuleDirW(WCHAR *out, SIZE_T cch)
{
    WCHAR path[MAX_PATH] = L"";
    if (!out || !cch) return FALSE;
    out[0] = L'\0';
    if (!g_hSelfModule && !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                              (LPCWSTR)&XXGetModuleDirW, &g_hSelfModule))
        return FALSE;
    if (!GetModuleFileNameW(g_hSelfModule, path, MAX_PATH)) return FALSE;
    {
        WCHAR *p = wcsrchr(path, L'\\');
        if (!p) return FALSE;
        *(p + 1) = L'\0';
    }
    wcsncpy_s(out, cch, path, _TRUNCATE);
    return out[0] != L'\0';
}
/* ---------------- 通用小工具 ---------------- */
static ULONGLONG HuntFnv64(const void *data, size_t len, ULONGLONG seed)
{
    const unsigned char *p = (const unsigned char *)data;
    ULONGLONG h = seed ? seed : 1469598103934665603ull;
    size_t i;
    for (i = 0; i < len; i++) { h ^= (ULONGLONG)p[i]; h *= 1099511628211ull; }
    return h;
}

static ULONGLONG HuntFnvStr(const char *s, ULONGLONG seed)
{
    return HuntFnv64(s, strlen(s), seed);
}

static BOOL HuntReadWholeFile(LPCWSTR path, unsigned char **out, DWORD *outSize)
{
    HANDLE h;
    DWORD size, read = 0;
    unsigned char *buf;

    *out = NULL; *outSize = 0;
    h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    size = GetFileSize(h, NULL);
    if (size == INVALID_FILE_SIZE || size < 0x200 || size > HUNT_MAX_IMG) { CloseHandle(h); return FALSE; }
    buf = (unsigned char *)malloc(size);
    if (!buf) { CloseHandle(h); return FALSE; }
    if (!ReadFile(h, buf, size, &read, NULL) || read != size) {
        free(buf); CloseHandle(h); return FALSE;
    }
    CloseHandle(h);
    *out = buf; *outSize = size;
    return TRUE;
}

/* ---------------- T1/T2 共用的 PE 特征 ---------------- */
typedef struct {
    /* 头字段 */
    DWORD timeStamp;
    DWORD sizeOfImage;
    DWORD entryPoint;
    DWORD checksum;
    WORD  sections;
    WORD  machine;

    /* 结构哈希 */
    ULONGLONG hashImports;      /* 排序后的 "dll!func" 集合 */
    ULONGLONG hashSections;     /* 节表：名字+VirtualSize+Characteristics */
    ULONGLONG hashStruct;       /* 上面两个 + 关键头字段（不含时间戳，跨重编稳定） */

    /* T2 用 */
    int       nImports;
    ULONGLONG imports[HUNT_MAX_IMPORTS];      /* 每个导入项一个 64 位 token 哈希 */
    int       nStrings;
    ULONGLONG strings[HUNT_SKETCH_K];         /* min-hash 采样（升序） */
    int       nBytes;
    DWORD     byteHist[256];                  /* .text 字节分布 */
} HuntPe;

static int __cdecl HuntCmpU64(const void *a, const void *b)
{
    ULONGLONG x = *(const ULONGLONG *)a, y = *(const ULONGLONG *)b;
    return (x < y) ? -1 : (x > y) ? 1 : 0;
}

/* 按需把 RVA 映射到文件偏移 */
static DWORD HuntRvaToOff(const unsigned char *buf, DWORD size, const IMAGE_NT_HEADERS *nt, DWORD rva)
{
    IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    WORD i;
    if (rva < nt->OptionalHeader.SizeOfHeaders) return (rva < size) ? rva : 0;
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        DWORD va = sec[i].VirtualAddress;
        DWORD vs = sec[i].Misc.VirtualSize ? sec[i].Misc.VirtualSize : sec[i].SizeOfRawData;
        if (rva >= va && rva < va + vs) {
            DWORD off = sec[i].PointerToRawData + (rva - va);
            return (off < size) ? off : 0;
        }
    }
    return 0;
}

/* 收集导入项：每个 "dll!func" 一个 token 哈希 */
static void HuntCollectImports(const unsigned char *buf, DWORD size, const IMAGE_NT_HEADERS *nt, HuntPe *pe)
{
    DWORD impRva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    DWORD impSize = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;
    DWORD off;
    int guard = 0;

    if (!impRva || !impSize) return;
    off = HuntRvaToOff(buf, size, nt, impRva);
    if (!off) return;

    while (guard++ < 256) {
        const IMAGE_IMPORT_DESCRIPTOR *desc = (const IMAGE_IMPORT_DESCRIPTOR *)(buf + off);
        DWORD nameOff;
        const char *dll;
        size_t dllLen;
        DWORD thunkOff;
        if (desc->Name == 0 && desc->FirstThunk == 0) break;
        nameOff = HuntRvaToOff(buf, size, nt, desc->Name);
        if (!nameOff) break;
        dll = (const char *)(buf + nameOff);
        dllLen = strnlen(dll, 64);
        thunkOff = HuntRvaToOff(buf, size, nt, desc->OriginalFirstThunk ? desc->OriginalFirstThunk : desc->FirstThunk);
        if (thunkOff) {
            int k = 0;
            while (k++ < 512) {
                const IMAGE_THUNK_DATA *th = (const IMAGE_THUNK_DATA *)(buf + thunkOff);
                ULONGLONG t;
                char item[96];
                if (th->u1.AddressOfData == 0) break;
                if (th->u1.Ordinal & IMAGE_ORDINAL_FLAG) {
                    _snprintf(item, sizeof(item) - 1, "%.*s!#%llu", (int)dllLen, dll,
                              (unsigned long long)(th->u1.Ordinal & 0xFFFF));
                } else {
                    DWORD hintOff = HuntRvaToOff(buf, size, nt, (DWORD)th->u1.AddressOfData);
                    const char *fn = hintOff ? (const char *)(buf + hintOff + 2) : "?";
                    _snprintf(item, sizeof(item) - 1, "%.*s!%.48s", (int)dllLen, dll, fn);
                }
                item[sizeof(item) - 1] = '\0';
                t = HuntFnvStr(item, 0);
                if (pe->nImports < HUNT_MAX_IMPORTS) pe->imports[pe->nImports++] = t;
                thunkOff += sizeof(IMAGE_THUNK_DATA);
                if (thunkOff >= size) break;
            }
        }
        off += sizeof(IMAGE_IMPORT_DESCRIPTOR);
        if (off >= size) break;
    }
}

/* 抽取字符串（ASCII ≥5 / UTF-16LE ≥4）→ min-hash 采样（保留最小的 K 个） */
static void HuntCollectStrings(const unsigned char *buf, DWORD size, HuntPe *pe)
{
    ULONGLONG best[HUNT_SKETCH_K];
    int n = 0, i;
    DWORD p = 0;

    memset(best, 0, sizeof(best));
    while (p < size) {
        DWORD q = p;
        while (q < size && (buf[q] >= 0x20 && buf[q] < 0x7F)) q++;
        if (q - p >= 5) {
            ULONGLONG h = HuntFnv64(buf + p, q - p, 0);
            if (n < HUNT_SKETCH_K) { best[n++] = h; }
            else {
                int worst = 0;
                for (i = 1; i < HUNT_SKETCH_K; i++) if (best[i] > best[worst]) worst = i;
                if (h < best[worst]) best[worst] = h;
            }
        }
        p = (q > p) ? q + 1 : p + 1;
    }
    qsort(best, (size_t)n, sizeof(best[0]), HuntCmpU64);
    memcpy(pe->strings, best, sizeof(best[0]) * (size_t)n);
    pe->nStrings = n;
}

static BOOL HuntFingerprint(LPCWSTR path, HuntPe *pe)
{
    unsigned char *buf = NULL;
    DWORD size = 0;
    const IMAGE_DOS_HEADER *dos;
    const IMAGE_NT_HEADERS *nt;
    IMAGE_SECTION_HEADER *sec;
    WORD i;

    memset(pe, 0, sizeof(*pe));
    if (!HuntReadWholeFile(path, &buf, &size)) return FALSE;

    dos = (const IMAGE_DOS_HEADER *)buf;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || (DWORD)dos->e_lfanew + sizeof(*nt) > size) {
        free(buf);
        return FALSE;
    }
    nt = (const IMAGE_NT_HEADERS *)(buf + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) { free(buf); return FALSE; }

    pe->timeStamp  = nt->FileHeader.TimeDateStamp;
    pe->sections   = nt->FileHeader.NumberOfSections;
    pe->machine    = nt->FileHeader.Machine;
    pe->sizeOfImage = nt->OptionalHeader.SizeOfImage;
    pe->entryPoint = nt->OptionalHeader.AddressOfEntryPoint;
    pe->checksum   = nt->OptionalHeader.CheckSum;

    /* 节表哈希 */
    sec = IMAGE_FIRST_SECTION(nt);
    {
        ULONGLONG h = 0;
        for (i = 0; i < pe->sections && i < 32; i++) {
            char nm[9];
            memcpy(nm, sec[i].Name, 8);
            nm[8] = '\0';
            h = HuntFnv64(nm, strnlen(nm, 8), h);
            h = HuntFnv64(&sec[i].Misc.VirtualSize, sizeof(DWORD), h);
            h = HuntFnv64(&sec[i].Characteristics, sizeof(DWORD), h);
        }
        pe->hashSections = h;
    }

    HuntCollectImports(buf, size, nt, pe);
    if (pe->nImports > 1) {
        int k;
        ULONGLONG h = 0;
        qsort(pe->imports, (size_t)pe->nImports, sizeof(pe->imports[0]), HuntCmpU64);
        for (k = 0; k < pe->nImports; k++) h = HuntFnv64(&pe->imports[k], sizeof(pe->imports[k]), h);
        pe->hashImports = h;
    }

    HuntCollectStrings(buf, size, pe);

    /* .text 字节直方图（弱信号，用来和"同源不同版本"对齐） */
    {
        static const char kText[] = { '.', 't', 'e', 'x', 't', 0 };
        for (i = 0; i < pe->sections; i++) {
            if (memcmp(sec[i].Name, kText, 5) != 0) continue;
            {
                DWORD off = sec[i].PointerToRawData;
                DWORD len = sec[i].SizeOfRawData;
                DWORD j, lim = 0;
                if (off >= size) break;
                if (len > size - off) len = size - off;
                if (len > 512u * 1024u) len = 512u * 1024u;
                for (j = 0; j < len; j++) pe->byteHist[buf[off + j]]++;
                pe->nBytes = (int)lim;   /* 0：只用直方图，不用长度 */
            }
            break;
        }
    }

    pe->hashStruct = HuntFnv64(&pe->hashImports, sizeof(ULONGLONG), 0);
    pe->hashStruct = HuntFnv64(&pe->hashSections, sizeof(ULONGLONG), pe->hashStruct);
    pe->hashStruct = HuntFnv64(&pe->sizeOfImage, sizeof(DWORD), pe->hashStruct);
    pe->hashStruct = HuntFnv64(&pe->sections, sizeof(WORD), pe->hashStruct);

    free(buf);
    return TRUE;
}

/* ---------------- T2：三路相似度 ---------------- */
static double HuntCosDwords(const DWORD *a, const DWORD *b, int n)
{
    double dot = 0, na = 0, nb = 0;
    int i;
    for (i = 0; i < n; i++) {
        dot += (double)a[i] * (double)b[i];
        na += (double)a[i] * (double)a[i];
        nb += (double)b[i] * (double)b[i];
    }
    if (na <= 0 || nb <= 0) return 0;
    return dot / (sqrt(na) * sqrt(nb));
}

static double HuntImportCos(const HuntPe *a, const HuntPe *b)
{
    /* 用"共同 token 数"近似：两边各自已排序 */
    int i = 0, j = 0, common = 0;
    int na = a->nImports, nb = b->nImports;
    if (!na || !nb) return 0;
    while (i < na && j < nb) {
        if (a->imports[i] == b->imports[j]) { common++; i++; j++; }
        else if (a->imports[i] < b->imports[j]) i++;
        else j++;
    }
    return (double)common / (double)(na + nb - common);   /* Jaccard，等价强度足够 */
}

static double HuntStringJaccard(const HuntPe *a, const HuntPe *b)
{
    int i = 0, j = 0, common = 0;
    int na = a->nStrings, nb = b->nStrings;
    if (!na || !nb) return 0;
    while (i < na && j < nb) {
        if (a->strings[i] == b->strings[j]) { common++; i++; j++; }
        else if (a->strings[i] < b->strings[j]) i++;
        else j++;
    }
    return (double)common / (double)(na + nb - common);
}

static double HuntCodeCos(const HuntPe *a, const HuntPe *b)
{
    return HuntCosDwords(a->byteHist, b->byteHist, 256);
}

/* 融合：两路以上达标才算"疑似" */
static double HuntScore(const HuntPe *cand, const HuntPe *base, int *signals)
{
    double c1 = HuntImportCos(cand, base);
    double c2 = HuntStringJaccard(cand, base);
    double c3 = HuntCodeCos(cand, base);
    int n = 0;
    if (c1 >= 0.85) n++;
    if (c2 >= 0.55) n++;
    if (c3 >= 0.90) n++;
    if (signals) *signals = n;
    return 0.5 * c1 + 0.3 * c2 + 0.2 * c3;
}

/* ---------------- 基线：我们自己造的 yy 样本 ---------------- */
static HuntPe g_huntBase[4];
static int    g_huntBaseCount = 0;
static BOOL   g_huntBaseReady = FALSE;

static void HuntBuildBaseline(void)
{
    static const WCHAR *kNames[] = { L"yy.exe", L"yyguard.dll", L"lenovo_fix.exe", NULL };
    WCHAR dirs[4][MAX_PATH];
    int nDir = 0, i, k;

    if (g_huntBaseReady) return;
    g_huntBaseReady = TRUE;

    /* 候选目录：程序目录、共享内存里记的 xx 目录、%TEMP% */
    {   /* 本模块所在目录当第一个候选 */
        WCHAR self[MAX_PATH] = L"";
        if (XXGetModuleDirW(self, MAX_PATH) && self[0]) wcscpy_s(dirs[nDir++], MAX_PATH, self);
    }
    if (g_pSharedView && XX_XXPATH(g_pSharedView)[0]) {
        WCHAR t[MAX_PATH];
        wcscpy_s(t, MAX_PATH, XX_XXPATH(g_pSharedView));
        { WCHAR *p = wcsrchr(t, L'\\'); if (p) *(p + 1) = L'\0'; }
        if (t[0]) wcscpy_s(dirs[nDir++], MAX_PATH, t);
    }
    if (GetTempPathW(MAX_PATH, dirs[nDir])) nDir++;

    for (i = 0; i < nDir && g_huntBaseCount < 4; i++) {
        for (k = 0; kNames[k] && g_huntBaseCount < 4; k++) {
            WCHAR path[MAX_PATH];
            _snwprintf(path, MAX_PATH - 1, L"%ls%ls", dirs[i], kNames[k]);
            path[MAX_PATH - 1] = L'\0';
            if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) continue;
            if (HuntFingerprint(path, &g_huntBase[g_huntBaseCount])) {
                WriteKillLog(L"[HUNT] 基线样本: %ls imports=%d strings=%d struct=0x%IX",
                             path, g_huntBase[g_huntBaseCount].nImports,
                             g_huntBase[g_huntBaseCount].nStrings,
                             g_huntBase[g_huntBaseCount].hashStruct);
                g_huntBaseCount++;
            }
        }
    }
    if (!g_huntBaseCount)
        WriteKillLog(L"[HUNT] 没有找到基线样本（yy.exe/yyguard.dll 不在候选目录），T1/T2 只做自比对");
}

/* ---------------- T0：谁持有我方进程句柄 ---------------- */
typedef struct {
    DWORD ownerPid;
    DWORD targetPid;
    DWORD access;
} HuntHandleHit;

static USHORT HuntProcessTypeIndex(void)
{
    static USHORT s_idx = 0;
    HANDLE hSelf;
    ULONG size = 1 << 20, ret = 0;
    unsigned char *buf;
    LONG st;

    if (s_idx) return s_idx;
    hSelf = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    if (!hSelf) return 0;

    for (;;) {
        buf = (unsigned char *)malloc(size);
        if (!buf) { CloseHandle(hSelf); return 0; }
        st = HuntNtQuery(64 /*SystemExtendedHandleInformation*/, buf, size, &ret);
        if (st == 0) break;
        free(buf);
        if (ret <= size || size >= (64u << 20)) { CloseHandle(hSelf); return 0; }
        size = ret + (64 * 1024);
    }

    {
        ULONG_PTR count = *(ULONG_PTR *)buf;
        unsigned char *p = buf + 2 * sizeof(ULONG_PTR);
        ULONG_PTR i;
        for (i = 0; i < count; i++) {
            /* 结构布局见 xxipc.h 的 XX_HANDLE_ENTRY 备注：这里按 40 字节/项走 */
            DWORD pid = *(DWORD *)(p + 8);
            ULONG_PTR handle = *(ULONG_PTR *)(p + 16);
            USHORT typeIdx = *(USHORT *)(p + 30);   /* +30 ObjectTypeIndex */
            if (pid == GetCurrentProcessId() && handle == (ULONG_PTR)hSelf) {
                s_idx = typeIdx;
                break;
            }
            p += 40;
        }
    }
    free(buf);
    CloseHandle(hSelf);
    return s_idx;
}

static BOOL HuntIsOurPid(DWORD pid)
{
    WCHAR img[MAX_PATH] = L"";
    DWORD cch = MAX_PATH;
    HANDLE h;
    const WCHAR *leaf;

    if (!pid) return FALSE;
    if (g_pSharedView && pid == XX_XXPID(g_pSharedView)) return TRUE;
    h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return FALSE;
    if (!QueryFullProcessImageNameW(h, 0, img, &cch)) { CloseHandle(h); return FALSE; }
    CloseHandle(h);
    leaf = wcsrchr(img, L'\\');
    leaf = leaf ? leaf + 1 : img;
    return (_wcsicmp(leaf, L"xx.exe") == 0);
}

/* 返回命中条数 */
static int HuntT0_HandleOwners(void)
{
    USHORT idx = HuntProcessTypeIndex();
    ULONG size = 1 << 20, ret = 0;
    unsigned char *buf;
    LONG st;
    int hits = 0;

    if (!idx) return 0;
    for (;;) {
        buf = (unsigned char *)malloc(size);
        if (!buf) return 0;
        st = HuntNtQuery(64, buf, size, &ret);
        if (st == 0) break;
        free(buf);
        if (ret <= size || size >= (64u << 20)) return 0;
        size = ret + (64 * 1024);
    }

    {
        ULONG_PTR count = *(ULONG_PTR *)buf;
        unsigned char *p = buf + 2 * sizeof(ULONG_PTR);
        ULONG_PTR i;
        for (i = 0; i < count; i++) {
            DWORD ownerPid = *(DWORD *)(p + 8);
            ULONG_PTR handleValue = *(ULONG_PTR *)(p + 16);
            USHORT typeIdx = *(USHORT *)(p + 30);   /* +30 ObjectTypeIndex */
            DWORD access = *(DWORD *)(p + 24);        /* +24 GrantedAccess */

            if (typeIdx == idx && ownerPid != GetCurrentProcessId() && ownerPid > 4) {
                HANDLE hOwner = OpenProcess(PROCESS_DUP_HANDLE | PROCESS_QUERY_LIMITED_INFORMATION,
                                            FALSE, ownerPid);
                if (hOwner) {
                    HANDLE hDup = NULL;
                    if (DuplicateHandle(hOwner, (HANDLE)handleValue, GetCurrentProcess(), &hDup,
                                        PROCESS_QUERY_LIMITED_INFORMATION, FALSE, 0) && hDup) {
                        DWORD targetPid = GetProcessId(hDup);
                        if (targetPid && HuntIsOurPid(targetPid)) {
                            WCHAR ownerImg[MAX_PATH] = L"", *leaf;
                            DWORD cch = MAX_PATH;
                            if (QueryFullProcessImageNameW(hOwner, 0, ownerImg, &cch)) {
                                leaf = wcsrchr(ownerImg, L'\\');
                                leaf = leaf ? leaf + 1 : ownerImg;
                            } else leaf = L"?";
                            hits++;
                            WriteKillLog(L"[HUNT][T0] PID=%lu(%ls) 持有我方进程 PID=%lu 的句柄 access=0x%lX",
                                         ownerPid, leaf, targetPid, access);
#if !HUNT_LEARN_ONLY
                            if (TerminateProcess(hOwner, 0))
                                WriteKillLog(L"[HUNT][T0] 已结束 PID=%lu（持有我方进程句柄）", ownerPid);
#endif
                        }
                        CloseHandle(hDup);
                    }
                    CloseHandle(hOwner);
                }
            }
            p += 40;
        }
    }
    free(buf);
    return hits;
}

/* T0-B：我方旧命名对象被占位 —— 我们早已不创建这些名字，存在即有人冒充 */
static void HuntT0_SquatProbe(void)
{
    static const WCHAR *kSquat[] = {
        L"Global\\xx_window_guard", L"Local\\xx_window_guard",
        L"Global\\xx_window_make_guard", NULL
    };
    int i;
    for (i = 0; kSquat[i]; i++) {
        HANDLE h = OpenMutexW(SYNCHRONIZE, FALSE, kSquat[i]);
        if (h) {
            CloseHandle(h);
            WriteKillLog(L"[HUNT][T0] 发现被占位的旧命名对象（第 %d 个）：有人冒充我方互斥体", i);
        }
    }
}

/* ---------------- 一层扫描入口 ---------------- */
static void HuntPass(void)
{
    HANDLE hSnap;
    PROCESSENTRY32W pe;
    int seen = 0, t0 = 0, t1 = 0, t2 = 0, suspects = 0;

    if (!HostIsXxExe()) return;      /* 只在交付本体里跑，避免 N 个宿主重复全盘扫描 */
    if (HUNT_LEARN_ONLY)
        ;                            /* 学习模式：只记日志 */

    HuntBuildBaseline();
    t0 += HuntT0_HandleOwners();
    HuntT0_SquatProbe();

    hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return;
    memset(&pe, 0, sizeof(pe));
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe)) {
        do {
            WCHAR img[MAX_PATH] = L"";
            DWORD cch = MAX_PATH;
            HANDLE hProc;
            HuntPe cand;

            if (pe.th32ProcessID == GetCurrentProcessId() || pe.th32ProcessID <= 4) continue;
            seen++;
            hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
            if (!hProc) continue;
            if (!QueryFullProcessImageNameW(hProc, 0, img, &cch)) { CloseHandle(hProc); continue; }
            CloseHandle(hProc);

            if (!HuntFingerprint(img, &cand)) continue;

            /* T1：结构哈希命中基线 */
            {
                int k;
                for (k = 0; k < g_huntBaseCount; k++) {
                    if (cand.hashStruct == g_huntBase[k].hashStruct) {
                        t1++;
                        WriteKillLog(L"[HUNT][T1] PID=%lu %ls 结构指纹命中基线（imports=0x%IX sect=0x%IX）",
                                     pe.th32ProcessID, pe.szExeFile,
                                     cand.hashImports, cand.hashSections);
#if !HUNT_LEARN_ONLY
                        {
                            HANDLE hK = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                            if (hK) { TerminateProcess(hK, 0); CloseHandle(hK); }
                        }
#endif
                        goto next_proc;
                    }
                }
            }

            /* T2：模糊打分（只要疑似就记，绝不单独动手） */
            {
                int k;
                for (k = 0; k < g_huntBaseCount; k++) {
                    int sig = 0;
                    double sc = HuntScore(&cand, &g_huntBase[k], &sig);
                    if (sc >= HUNT_SCORE_SUSPECT) {
                        suspects++;
                        if (sc >= HUNT_SCORE_HIT && sig >= 2) t2++;
                        WriteKillLog(L"[HUNT][T2] PID=%lu %ls score=%.2f 同源信号=%d imports=0x%IX",
                                     pe.th32ProcessID, pe.szExeFile, sc, sig, cand.hashImports);
                        break;
                    }
                }
            }
next_proc:
            ;
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);

    if (t0 || t1 || t2 || suspects)
        WriteKillLog(L"[HUNT] 本轮: 扫描=%d T0=%d T1=%d T2命中=%d 疑似=%d%s",
                     seen, t0, t1, t2, suspects,
                     HUNT_LEARN_ONLY ? L"（学习模式：只记录，未动手）" : L"");
}

#endif /* XX_HUNT_C */





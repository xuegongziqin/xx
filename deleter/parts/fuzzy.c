/* deleter/parts/fuzzy.c —— 被 deleter.c 直接 include
   内容：名字熵 + 内容余弦/shingle 的模糊识别与基线自举。 */

/* ================== 模糊识别：名字熵 + 内容余弦 ==================
   对手的落盘痕迹有三类固定名字抓不到：
     · yy_backup_7f3a9c1e.dat（yy.exe 里就是这条 UTF-16 字面量，紧跟在 "yy.exe" 后面）
     · yyres.bin 的副本、yysvc.exe 的副本
     · 随机命名（熵高）的备份/载荷
   所以这里补两条判据：
     ① 名字熵：去掉扩展名和下划线后做 Shannon 熵，≥3.2 且长度 ≥10 视为"随机名"；
     ② 内容余弦：字节直方图余弦 + 4 字节 shingle 的 minhash Jaccard，
        与已知 yy 家族样本（yy.exe/yyguard.dll/yysvc.exe/yyres.bin）比对。
   两个条件**同时**满足才删（名字随机 + 内容像 yy），避免误删普通临时文件。 */
#define FUZZY_MAX_BYTES   (8u * 1024u * 1024u)
#define FUZZY_SKETCH_K    48
#define FUZZY_COS_HIT     0.80
#define FUZZY_JAC_HIT     0.55
#define FUZZY_ENTROPY_HIT 3.20

typedef struct {
    DWORD     hist[256];
    ULONGLONG sketch[FUZZY_SKETCH_K];
    int       nSketch;
    DWORD     size;
} FuzzyFp;

static double FuzzyNameEntropy(const WCHAR *leaf)
{
    WCHAR base[128];
    int i, n = 0, counts[64];
    double ent = 0, total = 0;
    const WCHAR *dot;

    if (!leaf || !leaf[0]) return 0;
    wcsncpy_s(base, _countof(base), leaf, _TRUNCATE);
    dot = wcsrchr(base, L'.');
    if (dot && dot != base) base[(size_t)(dot - base)] = L'\0';

    memset(counts, 0, sizeof(counts));
    for (i = 0; base[i] && n < (int)_countof(base) - 1; i++) {
        WCHAR ch = base[i];
        if (ch == L'_' || ch == L'-') continue;      /* 分隔符不算随机性 */
        if (ch >= L'a' && ch <= L'z') ch = (WCHAR)(ch - L'a' + L'A');
        if (ch < 32 || ch > 126) continue;
        counts[ch % 64]++;
        n++;
    }
    if (n < 10) return 0;
    for (i = 0; i < 64; i++) {
        if (!counts[i]) continue;
        {
            double p = (double)counts[i] / (double)n;
            total += p;
            ent -= p * (log(p) / log(2.0));
        }
    }
    (void)total;
    return ent;
}

static BOOL FuzzyReadFp(const WCHAR *path, FuzzyFp *fp)
{
    HANDLE h;
    DWORD size, got = 0;
    BYTE *buf;
    DWORD i;

    memset(fp, 0, sizeof(*fp));
    h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    size = GetFileSize(h, NULL);
    if (size < 1024 || size > FUZZY_MAX_BYTES) { CloseHandle(h); return FALSE; }
    buf = (BYTE *)malloc(size);
    if (!buf) { CloseHandle(h); return FALSE; }
    if (!ReadFile(h, buf, size, &got, NULL) || got != size) {
        free(buf); CloseHandle(h); return FALSE;
    }
    CloseHandle(h);

    fp->size = size;
    for (i = 0; i < size; i++) fp->hist[buf[i]]++;

    /* 4 字节 shingle → bottom-K minhash（K=48） */
    {
        ULONGLONG best[FUZZY_SKETCH_K];
        int n = 0, k;
        memset(best, 0, sizeof(best));
        for (i = 0; i + 4 <= size; i += 4) {
            ULONGLONG h2 = 1469598103934665603ull;
            int j;
            for (j = 0; j < 4; j++) { h2 ^= buf[i + j]; h2 *= 1099511628211ull; }
            if (n < FUZZY_SKETCH_K) best[n++] = h2;
            else {
                int worst = 0;
                for (k = 1; k < FUZZY_SKETCH_K; k++) if (best[k] > best[worst]) worst = k;
                if (h2 < best[worst]) best[worst] = h2;
            }
        }
        for (k = 1; k < n; k++) {            /* 插入排序，K 很小 */
            ULONGLONG v = best[k];
            int m = k - 1;
            while (m >= 0 && best[m] > v) { best[m + 1] = best[m]; m--; }
            best[m + 1] = v;
        }
        memcpy(fp->sketch, best, sizeof(ULONGLONG) * (size_t)n);
        fp->nSketch = n;
    }
    free(buf);
    return TRUE;
}

static double FuzzyHistCos(const FuzzyFp *a, const FuzzyFp *b)
{
    double dot = 0, na = 0, nb = 0;
    int i;
    for (i = 0; i < 256; i++) {
        dot += (double)a->hist[i] * (double)b->hist[i];
        na  += (double)a->hist[i] * (double)a->hist[i];
        nb  += (double)b->hist[i] * (double)b->hist[i];
    }
    if (na <= 0 || nb <= 0) return 0;
    return dot / (sqrt(na) * sqrt(nb));
}

static double FuzzySketchJac(const FuzzyFp *a, const FuzzyFp *b)
{
    int i = 0, j = 0, common = 0;
    if (!a->nSketch || !b->nSketch) return 0;
    while (i < a->nSketch && j < b->nSketch) {
        if (a->sketch[i] == b->sketch[j]) { common++; i++; j++; }
        else if (a->sketch[i] < b->sketch[j]) i++;
        else j++;
    }
    return (double)common / (double)(a->nSketch + b->nSketch - common);
}

/* 基线样本：%TEMP% 与扫描目录里能找到的 yy 家族文件 */
static FuzzyFp g_fuzzyBase[4];
static int     g_fuzzyBaseN = 0;
static BOOL    g_fuzzyBaseTried = FALSE;

static void FuzzyLoadBaseline(void)
{
    static const WCHAR *kNames[] = { L"yy.exe", L"yyguard.dll", L"yysvc.exe", L"yyres.bin", NULL };
    WCHAR dirs[3][MAX_PATH];
    int nDir = 0, i, k;

    if (g_fuzzyBaseTried) return;
    g_fuzzyBaseTried = TRUE;

    if (GetTempPathW(MAX_PATH, dirs[nDir])) nDir++;
    {
        WCHAR self[MAX_PATH] = L"";
        if (GetModuleFileNameW(g_hSelfModule, self, MAX_PATH)) {
            WCHAR *p = wcsrchr(self, L'\\');
            if (p) { *(p + 1) = L'\0'; if (self[0]) wcscpy_s(dirs[nDir++], MAX_PATH, self); }
        }
    }

    for (i = 0; i < nDir && g_fuzzyBaseN < 4; i++) {
        for (k = 0; kNames[k] && g_fuzzyBaseN < 4; k++) {
            WCHAR path[MAX_PATH];
            _snwprintf(path, MAX_PATH - 1, L"%ls%ls", dirs[i], kNames[k]);
            path[MAX_PATH - 1] = L'\0';
            if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) continue;
            if (FuzzyReadFp(path, &g_fuzzyBase[g_fuzzyBaseN])) {
                DelLog(L"[FUZZY] 基线样本: %ls size=%lu", path, g_fuzzyBase[g_fuzzyBaseN].size);
                g_fuzzyBaseN++;
            }
        }
    }

    /* 最重要的来源：**活着**的对手进程的映像路径 —— 比赛路径不确定，
       只有进程自己知道它在哪。用 QueryFullProcessImageNameW 现取，绝不写死盘符。 */
    if (g_fuzzyBaseN < 4) {
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe;
            memset(&pe, 0, sizeof(pe));
            pe.dwSize = sizeof(pe);
            if (Process32FirstW(hSnap, &pe)) {
                do {
                    WCHAR img[MAX_PATH] = L"";
                    DWORD cch = MAX_PATH;
                    HANDLE hProc;
                    if (g_fuzzyBaseN >= 4) break;
                    if (!IsTargetLeaf(pe.szExeFile)) continue;
                    hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
                    if (!hProc) continue;
                    if (QueryFullProcessImageNameW(hProc, 0, img, &cch) && img[0]) {
                        if (FuzzyReadFp(img, &g_fuzzyBase[g_fuzzyBaseN])) {
                            DelLog(L"[FUZZY] 基线样本(运行中进程): %ls size=%lu",
                                   img, g_fuzzyBase[g_fuzzyBaseN].size);
                            g_fuzzyBaseN++;
                        }
                    }
                    CloseHandle(hProc);
                } while (Process32NextW(hSnap, &pe));
            }
            CloseHandle(hSnap);
        }
    }
}

/* 我们的产物与临时副本：不参与模糊删除 */
static BOOL FuzzyIsOurFile(const WCHAR *leaf)
{
    static const WCHAR *kOurs[] = {
        L"xx.exe", L"xxres.bin", L"svcguard.dll", L"window.dll", L"atkcore.dll",
        L"dedery.dll", L"guard.dll", L"defense.dll", L"payload.dll", L"payload.bin",
        L"xpt.xpt", L"xx_debug.log", NULL
    };
    int i;
    for (i = 0; kOurs[i]; i++) if (_wcsicmp(leaf, kOurs[i]) == 0) return TRUE;
    /* 我们改名的备份（rXXXXXXXX.tmp）与注入副本（系统 dll 名） */
    if (leaf[0] == L'r' && wcslen(leaf) == 12 && _wcsicmp(leaf + 9, L".tmp") == 0) return TRUE;
    if (wcsstr(leaf, L"xx_") == leaf) return TRUE;
    return FALSE;
}

/* 判定并删除：名字随机 + 内容像 yy 家族 */
static BOOL FuzzyHuntFile(const WCHAR *fullPath, const WCHAR *leaf)
{
    double ent, best = 0, bestJac = 0;
    FuzzyFp fp;
    int i;

    if (FuzzyIsOurFile(leaf)) return FALSE;
    ent = FuzzyNameEntropy(leaf);
    if (ent < FUZZY_ENTROPY_HIT) return FALSE;          /* 名字不像随机 → 不碰 */
    if (!FuzzyReadFp(fullPath, &fp)) return FALSE;

    FuzzyLoadBaseline();
    for (i = 0; i < g_fuzzyBaseN; i++) {
        double c = FuzzyHistCos(&fp, &g_fuzzyBase[i]);
        double j = FuzzySketchJac(&fp, &g_fuzzyBase[i]);
        if (c > best) { best = c; bestJac = j; }
    }
    if (!g_fuzzyBaseN) return FALSE;
    DelLog(L"[FUZZY] 候选 %ls 熵=%.2f 余弦=%.2f shingle=%.2f", fullPath, ent, best, bestJac);
    if (best >= FUZZY_COS_HIT && bestJac >= FUZZY_JAC_HIT) {
        DelLog(L"[FUZZY] 判定为 yy 家族副本（名字随机 + 内容相似）: %ls", fullPath);
        ForceDeleteFile(fullPath);
        return TRUE;
    }
    return FALSE;
}

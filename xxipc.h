/*
 * xxipc.h - 各模块共用的共享内存布局与命名常量（唯一来源）
 *
 * 任何模块都不得再自行定义这些偏移/大小：布局一旦漂移，
 * 日志、心跳计数器、句柄表就会互相踩内存。
 *
 * 共享内存 4096 字节布局（v2）：
 *   0    .. 127   心跳后缀   (64 WCHAR)
 *   128  .. 255   窗口后缀   (64 WCHAR)
 *   256  .. 259   心跳计数器 (LONG，弹窗存活时每 500ms +1)
 *   260  .. 263   布局版本   (LONG，必须等于 XX_LAYOUT_VERSION)
 *   264  .. 267   xx.exe PID (DWORD)
 *   268  .. 271   本次启动随机 tag (DWORD)，句柄表条目必须与之匹配
 *   272  .. 655   句柄表 16 条 x 24 字节
 *   656  .. 1023  保留
 *   1024 .. 4023  全局日志  (3000 字节 = 1500 WCHAR)
 *   4024 .. 4047  保留
 *   4048 .. 4051  svcguard 实例计数 (LONG)
 *
 * 句柄表条目（24 字节）：
 *   +0  DWORD     pid     —— 持有该句柄的宿主进程
 *   +8  ULONGLONG handle  —— 在 pid 进程内有效的 xx.exe 进程句柄
 *   +16 ULONGLONG tag     —— 必须等于 OFFSET_TAG，否则视为陈旧条目
 *
 * 句柄权限只有 PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE：
 * 句柄值只在"被复制进的那个进程"里有效，所以对手读到也用不了；
 * 即使同一宿主内的对手 DLL 拿到了它，也只能等/查，无法终止 xx。
 */

#ifndef XXIPC_H
#define XXIPC_H

/* ---- 共享内存 ---- */
/* 优先 Global\；没有 SeCreateGlobalPrivilege（非提权运行）时退到 Local\。
   同一台机器上同权限级别的各模块会做出同样的选择，因此仍然互相对得上。 */
#define SHARED_MEM_NAME         L"Global\\xx_shared_mem"
#define SHARED_MEM_NAME_LOCAL   L"Local\\xx_shared_mem"
#define SHARED_MEM_SIZE         4096

/* ---- 弹窗互斥锁：Local\ = 单会话，跨会话互不抢占 ---- */
#define WINDOW_GUARD_MUTEX      L"Local\\xx_window_guard"

/* 抢不到锁时的兜底建窗锁：多个热备实例可能在同一 tick 越过阈值，必须串行化 */
#define WINDOW_MAKE_GUARD_MUTEX L"Local\\xx_window_make_guard"

/* 抢不到锁时的兜底：本桌面连续看不到任何 XX 弹窗超过该时长就强建 */
#define WINDOW_FALLBACK_MS      1500

/* ---- 心跳：保留原事件（兼容），判据改用共享内存计数器 ---- */
#define HEARTBEAT_PREFIX        L"Global\\xx_heartbeat_"
#define HEARTBEAT_TIMEOUT       2000    /* 单次等待上限，仅用于事件兼容逻辑 */
#define HEARTBEAT_TICK_MS       500     /* 弹窗心跳节拍 */
#define HEARTBEAT_STALE_MS      2500    /* 计数器多久不涨判定弹窗丢失 */
#define HEARTBEAT_POLL_MS       500     /* 监视方轮询间隔 */
#define HEARTBEAT_RECOVER_MS    10000   /* 仍在停摆时的重试间隔（避免无脑重试） */

/* ---- 布局版本 ---- */
#define XX_LAYOUT_VERSION       2

/* ---- 偏移 ---- */
#define OFFSET_HB_SUFFIX        0
#define OFFSET_WND_SUFFIX       128
#define OFFSET_HB_TICK          256
#define OFFSET_LAYOUT           260
#define OFFSET_XX_PID           264
#define OFFSET_TAG              268
#define OFFSET_HDL_TABLE        272
#define OFFSET_SELF_HANDLE      656     /* xx.exe 启动早期（DACL 收紧前）打开的自身进程句柄 */
#define OFFSET_COUNTER          4048

#define SUFFIX_MAX              64
#define MAX_WND_SUFFIX          64

/* ---- 日志 ---- */
#define OFFSET_LOG              1024
#define LOG_SIZE                3000

/* ---- 句柄表 ---- */
#define HDL_TABLE_MAX           16
#define HDL_ENTRY_SIZE          24

/* 只读+同步：够 guard 判活（WaitForSingleObject）与查询路径，不足以终止进程 */
#define XX_SHARED_HANDLE_ACCESS (PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE)

/* ---- 通用名字 ---- */
#define XX_EXE_NAME_W           L"xx.exe"
#define XX_BACKUP_NAME_W        L"xxres.bin"
#define XPT_FILE_NAME           "xpt.xpt"

/* ---- 句柄表访问宏 ---- */
#define XX_HDL_ENTRY(p, i)      ((BYTE *)(p) + OFFSET_HDL_TABLE + (DWORD)(i) * HDL_ENTRY_SIZE)
#define XX_HDL_PID(p, i)        (*(DWORD     *)XX_HDL_ENTRY((p), (i)))
#define XX_HDL_RESERVED(p, i)   (*(DWORD     *)(XX_HDL_ENTRY((p), (i)) + 4))
#define XX_HDL_HANDLE(p, i)     (*(ULONGLONG *)(XX_HDL_ENTRY((p), (i)) + 8))
#define XX_HDL_TAG(p, i)        (*(ULONGLONG *)(XX_HDL_ENTRY((p), (i)) + 16))

/* ---- 心跳计数器读写 ---- */
#define XX_HB_TICK(p)           (*(volatile LONG *)((BYTE *)(p) + OFFSET_HB_TICK))
#define XX_LAYOUT(p)            (*(LONG    *)((BYTE *)(p) + OFFSET_LAYOUT))
#define XX_XXPID(p)             (*(DWORD   *)((BYTE *)(p) + OFFSET_XX_PID))
#define XX_TAG(p)               (*(DWORD   *)((BYTE *)(p) + OFFSET_TAG))
#define XX_SELF_HANDLE(p)       (*(ULONGLONG *)((BYTE *)(p) + OFFSET_SELF_HANDLE))
#define XX_COUNTER(p)           (*(volatile LONG *)((BYTE *)(p) + OFFSET_COUNTER))

/* ---- 跨进程共享内存的创建 / 打开（Global\ 拿不到就退 Local\） ---- */
static __inline HANDLE XXOpenShared(void) {
    HANDLE h = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, SHARED_MEM_NAME);
    if (!h) h = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, SHARED_MEM_NAME_LOCAL);
    return h;
}

/* ---- 日志是无锁共享的：读到别人正在搬移的中间状态时必须有界求长度，
       绝不能让 wcslen 越过日志区、跑出 4096 的映射之外 ---- */
static __inline size_t XXLogLen(const WCHAR *p, size_t maxChars) {
    size_t i;
    for (i = 0; i < maxChars; i++)
        if (p[i] == L'\0') return i;
    return maxChars;
}

/* ---- 布局自检：任何一处改坏，直接编不过，而不是运行时互相踩内存 ---- */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(OFFSET_HB_SUFFIX + SUFFIX_MAX * 2 <= OFFSET_WND_SUFFIX,
               "hb suffix overlaps window suffix");
_Static_assert(OFFSET_WND_SUFFIX + SUFFIX_MAX * 2 <= OFFSET_HB_TICK,
               "window suffix overlaps heartbeat tick");
_Static_assert(OFFSET_HB_TICK + 4 <= OFFSET_LAYOUT, "hb tick overlaps layout version");
_Static_assert(OFFSET_LAYOUT + 4 <= OFFSET_XX_PID, "layout version overlaps xx pid");
_Static_assert(OFFSET_XX_PID + 4 <= OFFSET_TAG, "xx pid overlaps tag");
_Static_assert(OFFSET_TAG + 4 <= OFFSET_HDL_TABLE, "tag overlaps handle table");
_Static_assert(OFFSET_HDL_TABLE + HDL_TABLE_MAX * HDL_ENTRY_SIZE <= OFFSET_SELF_HANDLE,
               "handle table overlaps self-handle slot");
_Static_assert(OFFSET_SELF_HANDLE + 8 <= OFFSET_LOG, "self-handle overlaps log");
_Static_assert(OFFSET_LOG + LOG_SIZE <= OFFSET_COUNTER, "log overlaps counter");
_Static_assert(OFFSET_COUNTER + 4 <= SHARED_MEM_SIZE, "counter outside shared memory");
_Static_assert(OFFSET_HDL_TABLE % 8 == 0 && HDL_ENTRY_SIZE % 8 == 0 && OFFSET_SELF_HANDLE % 8 == 0,
               "handle table entries must stay 8-byte aligned");
#endif

#endif /* XXIPC_H */

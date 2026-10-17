# xx — Windows 7 进程存活竞赛程序

一个 `xx.exe` 加多个 `dll` 的用户态存活/对抗程序，运行于 Windows 7。
与对手程序长期共存，**胜负判据只有一条**：我方弹窗是否连续消失 8 秒。

> 本项目仅用于课程竞赛环境，请勿用于任何其他用途。

---

## 1. 规则约束

规则原文见 [`prepare.txt`](prepare.txt)（GBK 编码）：

1. 只能有一个 exe 以及多个 dll，不能有多个 exe。
2. 必须有一个弹窗证明进程存活。**弹窗消失 8 秒内没有复活则判负**。
   注意：exe 被结束 8 秒不算输，必须是弹窗消失 8 秒。
3. 不允许使用内核驱动。
4. 不允许使用调试特权。
   **新约定已放开本条** —— 当前实现已在进程入口启用 `SeDebugPrivilege`，
   详见 [`project.txt`](project.txt) 第 6 节。
5. 必须告知对方自己最终的二进制文件包含哪些。

---

## 2. 架构

| 模块 | 类型 | 职责 |
| --- | --- | --- |
| `xx.exe` | 主进程 | 提权、设置安全描述符、建立共享内存、加载各模块、维护全局日志、定向关窗 |
| `svcguard.dll` | 守护与扩散 | 每 500ms 扫描**本会话**新进程并注入自身与 `window.dll`；向 4 个同会话宿主注入 `guard.dll` 并交接 xx 进程句柄；监听共享内存心跳，停摆即复活 `xx.exe` |
| `window.dll` | 弹窗与日志 | 多进程冗余弹窗、显示全局日志、每 500ms 递增共享内存心跳、拒绝一切可拒绝的窗口消息；抢不到 `Local\xx_window_guard` 时按兜底强建弹窗；导出 `StartWindow` 供 svcguard 远程重启弹窗线程 |
| `atkcore.dll` | 攻击工具箱 | 导出 `StartAttack`，按序尝试 5 种用户态结束方法 |
| `payload.dll` | 攻击载荷 | 注入目标进程后结束自身进程 |
| `payload.bin` | 位置无关 shellcode | 绕过非 MS 签名 DLL 检查的注入载荷 |
| `guard.dll` | 进程看门狗 | **由 svcguard 注入宿主进程**（不在 xx 内加载）；读 `xpt.xpt`，用 xx 交接的进程句柄判活、复活 xx、恢复被挂起线程 |
| `defense.dll` | 合并防御模块 | 文件锁 / 设备锁 / VEH 自愈 / 反注入 Hook / 心跳看门狗 |

```text
xx.exe
│
├──> svcguard.dll (守护与扩散，任意会话都注入；持我方文件锁)
│       ├──> window.dll (弹窗与日志)
│       ├──> guard.dll  (注入 4 个宿主进程 + 交接 xx 进程句柄)
│       └──> 注入其他本会话进程
│             ├──> window.dll (冗余弹窗)
│             └──> guard.dll  (宿主看门狗)
│
├──> atkcore.dll (攻击工具箱)
│       └──> payload.dll / payload.bin (攻击载荷)
│
└──> defense.dll (合并防御，StartDefense)
```

更完整的设计说明见 [`project.txt`](project.txt)。

---

## 3. 目录结构

```text
.
├── compile.bat          # 主构建脚本（8 步）
├── compiledebug.bat     # 调试版构建
├── project.txt          # 设计方案（与代码保持一致）
├── prepare.txt          # 比赛规则原文（GBK）
├── xxipc.h              # 共享内存布局与命名常量（各模块唯一来源）
│
├── xx/                  # xx.exe 主程序
├── deamon/              # svcguard.dll
├── window/              # window.dll
├── killtools/           # atkcore.dll
├── killer/              # payload.dll + payload.asm
├── guard/               # guard.dll
├── defense/             # defense.dll
├── sysstart/            # 仅测试用：以 SYSTEM 账号启动的安装脚本
│
├── bin/                 # 当前构建产物（交付物）
├── com/                 # 提交包（xx + yy + 启动脚本）
└── test/                # 实验代码与注入安全性测试工具
```

`com\enableyy.bat` 与 `sysstart\start.bat` 都是测试辅助脚本，不属于交付物。

---

## 4. 构建

### 环境

* 编译器：MinGW-w64 GCC（`D:\msys64\mingw64\bin\gcc.exe`，gcc 16.1.0）
* 汇编器：NASM（`C:\Program Files\NASM\nasm.exe`）
* 平台：目标机为 Windows 7 x64

### 编译

```bat
compile.bat
```

共 8 步，全部产物输出到 `bin\`：

| 步骤 | 产物 |
| --- | --- |
| 1 | `xx.exe` |
| 2 | `svcguard.dll` |
| 3 | `window.dll` |
| 4 | `atkcore.dll` |
| 5 | `payload.dll` |
| 6 | `guard.dll` |
| 7 | `payload.bin`（NASM） |
| 8 | `defense.dll` |

源文件均为 UTF-8 with BOM，编译选项已带 `-finput-charset=UTF-8`。

---

## 5. 二进制清单（规则第 5 条要求公开）

`bin\` 目录下的全部交付物：

| 文件 | 大小 | 说明 |
| --- | ---: | --- |
| `xx.exe` | 78,336 | 唯一的 exe 主程序（会话随机名 token、双向 WMI 订阅、waste 模仿嘲讽） |
| `svcguard.dll` | 35,328 | 守护与扩散（改名注入 + 远程送会话名、guard 宿主部署、80ms 复活扫描） |
| `window.dll` | 28,160 | 弹窗与日志（类名每进程随机、每会话持有者表） |
| `atkcore.dll` | 51,712 | 攻击工具箱（本体嘲讽满 10 条再杀 / 子进程直接杀掉锁死删除、对手内存 patch、删 WMI、注入 yy 内部感染） |
| `payload.dll` | 14,336 | 攻击载荷（PEB 隐藏、会话单例） |
| `payload.bin` | 274 | shellcode 载荷 |
| `guard.dll` | 47,104 | 进程看门狗（注入宿主运行、PEB 隐藏） |
| `defense.dll` | 22,016 | 合并防御（整函数 TerminateProcess 还原、.text 冷镜像自愈） |
| **合计** | **277,266** | |

`com\` 是提交包：`com\xx\` 为上述 8 个文件，`com\yy\` 为对照程序，
`com\enableyy.bat` 用于侧边测试开关。

---

## 6. 核心机制

### 6.1 存活与复活

* `window.dll` 在多个宿主进程里抢 **`Local\xx_window_guard`** 互斥锁（单会话命名空间，
  跨会话互不抢占），谁抢到谁创建弹窗，失败方以 `Sleep(5)` 极速热备；
  前任持有者被杀时以 `WAIT_ABANDONED` 立即接管。
* **兜底强建**：抢不到锁（例如被对手进程长期持有）时，只要本桌面
  `EnumWindows` 连续 1.5 秒看不到任何 `XX_ALIVE_WND_*` 窗口，就无视锁自己建窗；
  建窗前先抢 `Local\xx_window_make_guard` 并复查一次（多个热备实例可能在同一
  tick 越过阈值，不串行化就会建出重复弹窗），窗口出现后其余实例继续热备。
* 弹窗心跳 = **共享内存计数器**（`OFFSET_HB_TICK`）：持有弹窗的实例每 500ms 递增一次；
  能看到本桌面弹窗的热备实例也帮忙递增（这样即使持有者被 `SuspendThread` 冻结，
  只要弹窗还在屏幕上，心跳就不会误判）。
* `svcguard.dll` 的 `HeartbeatMonitor` / `defense.dll` 的 `WatchdogThread`
  每 500ms 轮询该计数器，**连续 2.5 秒不涨**才判定弹窗丢失；恢复动作按 10 秒节流
  （`HEARTBEAT_RECOVER_MS`），避免"xx 还活着"时每 2.5 秒无脑重扫进程表。
* **三级恢复链**（弹窗丢失时按顺序尝试，任一级成功即可让心跳恢复）：
  1. 本进程（xx.exe 内）若没有 `window.dll` 就重新加载它；
  2. 用远程线程调用宿主进程里 `window.dll` 的导出 `StartWindow`，
     把被 `TerminateThread` / 已退出的弹窗线程重新拉起（最多两个宿主，受
     `WINDOW_THREAD_MAX=4` 限制）；
  3. 最后才 `RespawnXX` 拉起 `xx.exe`（已有实例在跑则跳过），
     用命名互斥锁 `Global\xx_respawn_guard` 串行化，避免多实例同时拉起。
* `xx.exe` 复活时**沿用**共享内存里已有的心跳/窗口后缀（不再清零重建），
  否则老 `window.dll` 实例监听的事件名会与新 `svcguard` 永久错配。
* 心跳计数停摆但 `xx.exe` 仍在运行时，若本实例就在 xx 内且 `window.dll` 不在，
  就就地重新 `LoadLibrary` 拉起弹窗模块（避免"xx 活着但弹窗永不回来"）。
* `xx.exe` 被删时从同目录 `xxres.bin` 备份还原；`window.dll` 拉起 `xx.exe` 前同样会还原。
* `xx.exe` 还创建 WMI 永久订阅（`__EventFilter` 名为 `XX_RespawnFilter`）兜底复活。

### 6.1.1 guard.dll 的宿主化与句柄交接

`xx.exe` **不再**用 `LoadLibrary` 把 `guard.dll` 装进自己：那样 xx 被结束时 guard 会
一起死，根本来不及复活 xx。改为：

1. 跑在 `xx.exe` 内的 `svcguard` 实例负责把 `guard.dll` 注入最多 4 个**同会话**宿主
  （优先 `explorer.exe`），并在宿主消失/模块被卸时自动补部署。
2. 注入的同时用 `DuplicateHandle` 把 **xx 自己的进程句柄**（权限只有
  `PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE`）复制进该宿主，
  按宿主 PID 登记到共享内存句柄表。源句柄是 xx 在**收紧 DACL 之前**打开、
  写进共享内存 `OFFSET_SELF_HANDLE` 槽位的那个（同进程内句柄值有效），
  现场 `OpenProcess` 只作后备；伪句柄不能跨进程复制，所以不做那种"退化"。
  取用时用 `GetProcessId` 校验句柄确实指向记录里的 PID，杜绝陈旧值/槽位复用
  被拿去 `WaitForSingleObject`。
3. 宿主里的 `guard.dll` 按自己的 PID 取出该句柄，用 `WaitForSingleObject` 判活 —— 
   完全不调用 `OpenProcess`。这一点很关键：xx 的进程 DACL 拒绝了 Everyone
  （连自己和 SYSTEM 一起拒），`OpenProcess` 那条路本来就走不通。
4. 句柄值只在"被复制进的那个进程"里有效，对手读到也用不了；权限只给查询+同步，
   即使同一宿主内的对手 DLL 拿到它也无法终止 xx，因此不需要额外加密。
5. 句柄表条目带启动 tag，`xx.exe` 重启后旧条目自动失效；找不到句柄时
   **重发**句柄（幂等），宿主消失或句柄发不进去就腾出名额改投其它宿主。
  找不到句柄时 `guard.dll` 退化为 `OpenProcess` + 进程名快照判活。

### 6.2 扩散

* `SpreaderThread` 每 500ms 扫描进程列表，注入 `svcguard.dll` 与 `window.dll`。
* **扩散不按会话过滤**（重要）：弹窗锁虽然是 `Local\`（会话隔离），但注入**不做**会话
  限制。一旦在这里按会话过滤，xx 与目标进程会话不一致时（服务/SYSTEM 启动、RDP、
  多用户）就会一个进程都注不进去 —— 表现就是"别的进程里根本没有我们的 dll"，
  随后 xx 被杀也没人复活。限制放在**复活**这一步：只允许在 `xx.exe` 原先的会话里
  拉起它（会话号记在共享内存，见 6.6），避免别的会话造出看不见的影子实例。
* 注入结果缓存 10 秒，避免重复扫描与重复注入。
* **每 10 秒输出一行扩散统计**（`扩散统计(10s): 可注入进程=… 注入成功=… OpenProcess失败=…`），
  成功注入也会记名（`已注入 svcguard.dll -> PID=… explorer.exe`）；
  `OpenProcess` 失败会带 err 码（只报前 5 条）。看不到弹窗日志时也能凭这几行判断注入是否发生。
* **反"反注入"**：对手会 hook 目标进程的 `ntdll!LdrLoadDll`，按键名把
  `svcguard.dll` / `window.dll` 直接拒掉（此时 `CreateRemoteThread` 仍成功，
  不看返回值就会误判成"注入成功"）。因此注入前会先把目标里被改写的
  `LdrLoadDll` 前导字节还原（原始字节取自磁盘 `ntdll.dll` 的 `SEC_IMAGE` 映像，
  不能照抄自己进程 —— `defense.dll` 会在 xx.exe 里 hook 它），并且**用远端线程
  退出码校验** `LoadLibrary` 是否真的成功，失败会写进日志。
* **我方文件锁**：Windows 允许删除正在运行的镜像（加载器带 `FILE_SHARE_DELETE`），
  所以 xx.exe / 各 dll / `xxres.bin` 默认能被对手直接删掉，删掉后新的注入就没东西可注入。
  每个宿主里的 svcguard 都以 `access=0 + FILE_SHARE_READ` 持住这些文件（实测：该共享模式
  能成功锁住运行中的镜像，并挡住删除 / 改名 / 覆盖写入，同时仍允许读取与 `LoadLibrary`，
  所以不影响我们自己的备份还原与注入）。
* 跳过关键系统进程（`IsProtected` 黑名单，含 **`notepad.exe`**：`atkcore` 会把
  notepad 当敌方宿主击杀，自己再往 notepad 注入弹窗就会被自己秒掉）。
* 额外把 `guard.dll` 注入 4 个同会话宿主（见 6.1.1）。
* 周期性卸载对手注入的 `guard.dll`，只保留与我方同目录的那一份。

### 6.3 攻击（`atkcore.dll` 5 种方法，按序尝试）

1. `TerminateProcess`
2. `CreateRemoteThread` + `ExitProcess`（正常退出路径，会执行 `DLL_PROCESS_DETACH`）
3. 注入 `payload.dll`
4. 注入 `payload.bin`（对 `yy.exe` 走 APC 绕过签名检查，对宿主进程走远程线程）
5. `Job` 对象终止

另外 `xx.exe` 的 `AttackThread` 会定向关闭 `yy.exe` / `ww.exe` 的窗口，
并用同名同标题重建 1×1 假窗口。

### 6.4 防御（`defense.dll`）

* 敌人二进制独占锁（`yy.exe` / `lenovo_fix.exe` / `guard.dll` 等）：
  用 **xx 自己目录下的绝对路径** + 请求 0 权限 + 共享模式 `FILE_SHARE_READ` + `OPEN_EXISTING`
  （原实现用相对路径 + `CREATE_ALWAYS`，实际锁的是宿主进程的 CWD，
  会在错误目录里凭空创建/清零文件，还试图锁死自己的 `guard.dll`；且用
  `GENERIC_READ|GENERIC_WRITE` 打开正在运行的镜像必然 `SHARING_VIOLATION`，锁不住）
* BYOVD 设备符号链接锁（`\\.\BootRepair`、`\\.\kdmapper`、`\\.\kdu`、`\\.\GIO`）
* VEH 冷镜像自愈（拦截 `.text` 被写 `0xCC`）
* `kernel32!TerminateProcess` 字节还原
* `LdrLoadDll` 反注入 Hook（同名 `guard.dll` 用完整路径放行我方）
* 心跳看门狗（轮询共享内存心跳计数器，2.5 秒不涨则从 `xxres.bin` 还原并拉起 `xx.exe`）
* 刻意不装 `NtCreateThreadEx` / `NtQueueApcThread` Hook（本地 hook 拦不到「别人往我这儿建线程」）

`defense.dll` 以 `-Wl,--export-all-symbols` 导出全部函数，主入口为 `StartDefense`。

### 6.5 提权

`xx.exe` / `svcguard.dll` / `atkcore.dll` 在进程入口调用 `EnableDebugPrivilege`
启用 `SeDebugPrivilege`（`AdjustTokenPrivileges`），用于跨进程句柄打开、
注入与击杀。

### 6.6 共享内存布局（v2，唯一定义在 `xxipc.h`）

| 偏移 | 长度 | 内容 |
| ---: | ---: | --- |
| 0 | 128 | 心跳后缀（64 WCHAR） |
| 128 | 128 | 窗口后缀（64 WCHAR） |
| 256 | 4 | 心跳计数器（LONG，弹窗存活时每 500ms +1） |
| 260 | 4 | 布局版本（必须等于 `XX_LAYOUT_VERSION`） |
| 264 | 4 | `xx.exe` PID |
| 268 | 4 | 本次启动随机 tag（句柄表条目必须匹配） |
| 272 | 384 | 句柄表：16 条 × 24 字节（pid / 句柄指向的 xx PID / handle / tag） |
| 656 | 8 | xx 在收紧 DACL 前打开的自身进程句柄（供 svcguard 复制给宿主） |
| 664 | 4 | `xx.exe` 所在登录会话（复活时只允许同会话实例拉起它） |
| 1024 | 3000 | 全局日志（1500 WCHAR） |
| 4048 | 4 | svcguard 实例计数 |

`xx.exe` 只在**新建**映射时清零；已有实例在跑时绝不清零、并沿用既有后缀，
避免把老 `window.dll` 实例正在监听的事件名抹掉。布局版本不符时只重置
`256..1023` 这段新字段。名字优先用 `Global\xx_shared_mem`，若没有
`SeCreateGlobalPrivilege`（非提权运行）则退到 `Local\xx_shared_mem`
（同一权限级别的各模块会做同样选择，因此仍互相对得上）。所有偏移都有
`xxipc.h` 里的编译期 `_Static_assert` 兜底，改坏直接编不过。

跨进程日志是无锁共享的，所有写入方都用 `XXLogLen` 有界求长度，绝不
`wcslen` 越过日志区跑到映射之外（读到的可能是别人正在搬移的中间状态，
显示可能偶发错乱，但不会越界）。

---

### 6.7 隐藏、改名注入与全树清除（本轮新增）

**① 每个 dll 都把自己从 PEB 的三条模块链表摘除**（[xxipc.h](xxipc.h) `XXHideSelfFromPeb`）：
`InLoadOrderModuleList` / `InMemoryOrderModuleList` / `InInitializationOrderModuleList`
三条都摘，并对自己的 `LIST_ENTRY` 做自环。对手按模块扫描进程
（`EnumProcessModules` / `CreateToolhelp32Snapshot(SNAPMODULE)` / 进程查看器）
就读不到我们了。实测：加载后 `EnumProcessModules` 命中 0（对照组系统 dll 命中 1）。
注意一个细节：`GetModuleHandle` 还能查到（ntdll 内部另有一张按名字哈希的表，摘链不影响它），
这反而是好事 —— 注入方重复注入同名副本时加载器会去重（refcount+1，不重跑 DllMain），
不会叠出多套线程。
摘链后自身 `GetModuleHandle`/`GetModuleFileName` 都不可靠，所以每个模块都在 `DllMain`
里先把 HMODULE 与自身路径缓存下来；同时用 **per-process 命名互斥体做单例**，
万一副本名变了（新盐）而被当成新模块加载，单例会让 `DllMain` 返回 FALSE、
加载直接失败而不残留映射。

**② 注入前复制到 `%TEMP%` 并改成随机"系统 dll"名**（`XXMakeTempCopy`）：
名字例如 `sechost3F2A.dll`、`winmm8C41.dll`，从 12 个常见系统组件名 + 随机后缀里挑。
对手按 `svcguard.dll`/`window.dll`/`payload.dll` 做的**反注入名单**和**删文件名单**
同时失效；副本被删会在下一轮注入前自动重新复制。名字里的随机盐由 `xx.exe` 写进
共享内存（`OFFSET_TMP_SALT`），全体实例算出同一个名字，一轮只留固定几份副本。
同理，`xx.exe` 还把自己的全路径写进共享内存（`OFFSET_XX_PATH`）—— 副本是从
`%TEMP%` 加载的，任何模块都不能再按"自己的目录"去找 `xx.exe`/`xxres.bin`/`xpt.xpt`/同伴 dll。

**③ 杀 yy 之后新建线程清空其目录**（atkcore `WipeEnemyDirThread`）与
**④ 杀进程树**（`KillProcessTree`）：按 `th32ParentProcessID` 递归收整棵树（深度≤6、
上限 64 个），**先取各自镜像路径再动手**（杀完就取不到了），子进程先杀、根进程最后杀；
每个进程的源文件都走"**先锁死再删除**"：用 `FileDispositionInfo` 置删除待决 ——
标记后按名字再打开一律被拒（对手既不能执行也不能改写），最后一个句柄关闭时文件即刻消失，
对正在运行的镜像同样有效。根进程的目录再由新线程整体清空一遍（隔 1.2 秒再清一次，
对付"同目录重建"），并**按名字跳过我方 9 个文件**，避免 xx 与 yy 同目录时误删自己。

**⑤ defense：整函数备份 + 反 NOP**：
`kernel32!TerminateProcess` 不再只备份前 5 字节，而是扫描 `ret` 与对齐填充求出
函数长度（封顶 256 字节）后**整体备份、整体还原**（每 30ms 校验一次）。
更重要的是：**NOP(0x90) 不会触发异常**，VEH 只能拦 0xCC，所以新增
`TextIntegrityThread` —— 每 200ms 把主模块 `.text` 与冷镜像逐字节比对，
发现任何差异（NOP、0xCC 或任意改写）就整体写回；VEH 保留为 0xCC 的快速路径。

**⑥ svcguard 复活扫描 80ms**：`HEARTBEAT_POLL_MS_DEMON=80`（defense 仍是 500ms），
恢复重试间隔从 10s 降到 1s。

**⑦ 命名对象随机化 + 方案 A 传递**（本轮新增）：
锁名/事件名不再写死。`xx.exe` 启动时生成一个 32 位 **会话随机名 token**，
命名对象统一为 `xx_<用途>_<token>`（弹窗锁 `xx_wg_`、建窗串行锁 `xx_wmg_`、
复活锁 `xx_rg_`、攻击 leader `xx_al_`、模块单例 `xx_<模块>_<pid>`、心跳事件 `xx_hb_`）。

* **传递方式（方案 A）**：注入成功后，注入方在目标进程里分配一个 `XX_NAME_BLOCK`，
  用 `CreateRemoteThread` 调用目标那份 dll 的导出函数 `XxSetNames`；
  调用返回后**立刻清零并 `VirtualFreeEx`**（读后即删，不落盘、不进任何全局命名空间）。
  RVA 用 `LoadLibraryExW(..., DONT_RESOLVE_DLL_REFERENCES)` 映射同一份文件取得
  （临时副本与本地字节相同，RVA 通用，且不会执行对方 `DllMain`）。
  xx.exe 自己加载 svcguard/window/defense/atkcore 时走同进程直接调用。
* **一致性兜底**：拿不到 token 的实例退回共享内存里的 salt（`XX_TMP_SALT`）做名字；
  再不行退回固定值 `XX_NAME_TOKEN_FIXED`。三者都由全体实例共享，**绝不会出现
  "两个实例各建一个弹窗"或"互相等锁"**。共享 salt 在映射已存在时**沿用旧值**
  （xx 重启不会换名，否则新旧实例会各认一把锁）。
* **随机性来源（重要）**：名字里的随机部分是 **64 位会话 token**（`_<16位十六进制>`），
  **不是 PID**；PID 只作为"每进程一格"的作用域后缀（可被枚举，不提供任何保密性），
  而跨进程锁（`wg`/`wmg`/`rg`/`al`）的名字里根本没有 PID。
  来源按优先级：① `XxSetNames` 送进来的**秘密 token**（不写共享内存、不落盘，参数块读完清零）；
  ② 共享内存里的**64 位回退 token**（同样随机，但对手解析我们的映射就能读到）；
  ③ 固定常量（不随机，仅在无共享内存时）。
  熵：token 由 **`RtlGenRandom`（advapi32 的 `SystemFunction036`，Win7 自带）** 生成 64 位，
  失败才退回 `rand()` 兜底；共享内存里的**回退 token 也是 64 位**（`OFFSET_NAME_TOKEN64`，布局 v3）。
  对手要拿到它只能逆向我们的二进制，或读到共享内存里那份**回退**值（真 token 不落任何文件/映射）。
* **位宽坑（踩过两个，都会让 64 位白改）**：
  ① `_snwprintf` 落 MSVCRT 时不认 `%llX`（只认 `%I64X`），实测把 64 位 token 截成 32 位 →
  改成手写 16 位十六进制（`XXHex64`）；
  ② 某处局部变量还写着 `DWORD tok = WaitOwnNameToken();`，而 `XXNameBlockTake` 往它写入 8 字节 →
  只留下低 32 位，名字变成 `xx_wg_0000000055667788`。现在所有存 token 的变量/返回值都是 `ULONGLONG`。
  验证方式：送 `0x1122334455667788` 后互斥体名必须是 `Local\xx_wg_1122334455667788` 且真实存在。
* 另外**日志绝不能写 token**：本轮抓到两处泄密（xx.exe 写过 token 原文；
  window.dll 打印过含 token 的完整互斥体名），现统一改成 `xx_wg_**** kind=secret|salt|fixed`，
  实测日志里搜不到任何 token 字符。
* **名字候选表（本轮补）**：注入判定与守护宿主判定用的互斥体同样是 token 化名字，
  但存在两种合法形态 —— 远程注入的副本（DllMain 里等到了 token）用 **token 名**；
  xx 同进程同步 `LoadLibrary` 的模块（push 只能发生在 DllMain 返回之后）用**回退名**。
  只认一种就会互相看不见（注入方以为没有 → 反复重复注入；单例也会被加载两次）。
  现在创建与探测共用一张候选表：**token → 共享 salt（随机）→ 固定常量 → 裸名**，
  创建取第一个（所以优先随机），探测逐个试（所以两种形态都认得）。
  回退名也从写死常量改成了**共享内存里的随机 salt**（实测：`Global\xx_guard_1234ABCD_<pid>`）。
* 实测：六个 dll 都导出 `XxSetNames`；篡改 magic 的参数块被拒绝；
  传入 token 后进程内出现 `Local\xx_wg_ABCD1234` 这种新名字；窗口类名形如
  `wnd_7ba7598c4efe5dbc`；`SetParent`/隐藏/嘲讽等窗口攻击见 §6.8。

**⑧ 顺带修掉一个致命老 bug**：`window.dll` 等共享内存后缀的循环里 `waited`
写在了循环体内（每轮被清零），超时永远不触发 —— 一旦共享内存建不出来
（无权限/被对手抢占），弹窗就**永远不会出现**，兜底路径形同虚设。已移到循环外并实测
（无共享内存时 3 秒后改用本地随机类名照常建窗）。

### 6.8 对 yy 窗口与 WMI 的利用（依据 `yybad.md`）

* **1.2 改标题**：`SetWindowTextW(yy窗口, "xx_ALIVE_WND_<我方后缀>")`（系统会跨进程编组）。
* **1.3 挂不可见父窗口**：自己建一个隐藏 `STATIC` 窗口，`SetParent(yy窗口, 它)`。
* **1.5 未拦消息**：`WM_SYSCOMMAND/SC_MINIMIZE`、`WM_ENABLE(FALSE)`、`SW_HIDE`。
* **1.1 `DestroyWindow`**：实测**跨进程必然失败**（只有创建窗口的那个线程能销毁它），
  `yybad.md` 这条结论有误；真正致命的是 1.3/1.5 —— yy 主循环只查
  `!g_hMainWnd || !IsWindow(g_hMainWnd)`，隐藏后窗口"仍然存在"，它**永远不重建**，
  8 秒即判负。代码里仍尝试一次并把结果写日志。
* **1.6 日志嘲讽**：每 300ms 往 yy 的 EDIT 日志框追加嘲讽并周期性整段清空。
  **不能用 `EM_REPLACESEL`**（`yybad.md` 的写法不安全）：它带字符串指针且跨进程
  不会被编组，目标会读野指针；改用系统会编组的 `WM_GETTEXT` + `WM_SETTEXT`。
* **删对手 WMI 复活订阅三件套**（#8）：`__FilterToConsumerBinding` →
  `CommandLineEventConsumer` → `__EventFilter`，顺序删三轮并再按 `YY_` 前缀扫一遍
  （对手改名也盖得住）；连 `ROOT\SUBSCRIPTION`，`RPC_E_CHANGED_MODE` 也继续。
* **#16 非断点 patch：废掉它的建窗能力**。在 yy 自己的进程里把
  `user32!CreateWindowExW` 的**入口机器码**改成 `31 C0 C3`（`xor eax,eax; ret`）——
  它调用永远返回 NULL，再也建不出弹窗；它的 VEH 只认 `0xCC`，这种改写不触发异常，
  `ShieldWorker` 也只还原 `TerminateProcess`，不会改回来。反汇编验证：
  patch 后目标每次调用都拿到 `00000000`，进程本身照常运行。
  *实测教训*：**不要用 IAT patch 做这件事**。对照实验里我往目标 IAT 槽写存根
  （回读确认写入成功），用 `-O2` 编的目标照样能建窗口 —— 编译器把导入函数指针
  **提前加载进寄存器**，循环里根本不再读 IAT。改函数入口不受此影响。
  系统 dll 代码页是 copy-on-write，在这里改只影响 yy 自己，不波及我们与其它进程。
* **#17 12 字节 patch 它的 `TerminateProcess`**：写成 `mov rax,0; jmp rax`。
  它的 `ShieldWorker` 只备份/还原**前 5 字节**，还原之后剩下 7 字节仍是我们的垃圾，
  函数被劈坏 → yy 一调 `TerminateProcess` 就自己崩（再也杀不了我们的人）。
  实测：被 patch 的靶子在下一次调用时以 `0xC0000005` 退出。
* **#19 把对手的自备份改写成我方内容**（按你的要求用 `xxres.bin` 的内容）：
  清对方目录时，`yyres*.bin` 不删除而是**用我们的 `xxres.bin`（xx.exe 的备份）覆盖**——
  yy 从备份"复活"出来的其实是 xx.exe 的字节，它越复活越是在帮我们跑。
  覆盖失败（被占位锁等）则退回原来的删除逻辑。
* **#10 注入 yy 之后子类化它的日志框**：我们的扩散器本来就会把 atkcore 注入 yy
  （yy.exe 不在保护名单里，注入的又是改名副本，它的 `BAD_MODULES` 子串拦截失效）。
  进去以后 `SetWindowLongPtrW(GWLP_WNDPROC)` 换掉它日志 EDIT 的窗口过程，
  它每写一行日志都被替换成嘲讽 —— 劫持它的"眼睛"。同进程内字符串指针有效，
  这才是 yybad.md 1.6 利用 4 的正确做法。
* **#11 注入后注销并封死它的窗口类**：`UnregisterClassW("YYWnd_7f3a9c1e")`，
  并把本进程内的 `RegisterClassExW/RegisterClassW/CreateWindowExW/CreateWindowExA`
  入口改成 `xor eax,eax; ret` —— 它再也注册不出/建不出弹窗（与跨进程 #16 双保险）。
  顺手把它主窗口 `SW_HIDE`。进了 yy 的副本**不杀自己所在的宿主**，留着当眼线持续压制。
* **#15 反向 WMI 监听 yy.exe 创建**：`XX_YyWatchFilter`（`__InstanceCreationEvent` 订阅
  `Win32_Process` 里 `Name='yy.exe'`）+ `XX_YyWatchConsumer`（`cmd /c taskkill /f /im yy.exe`）
  + 绑定。yy 的复活链一把它拉起来，1 秒内就被按掉。用 `XX_` 前缀，不会被我们自己的
  "删 `YY_` 前缀"逻辑误删。

### 6.9 弹窗持有者表（修掉"杀宿主不重建"与"跨会话误判"）

判定"本桌面有没有我方弹窗"曾经用类名/标题，后来改成"哪个进程加载了我们的 window 模块"——
**后者是错的**：explorer 这类宿主装了我们的模块后，它自己的任何可见窗口都会被算成我方弹窗，
于是真弹窗死了兜底建窗也永远不触发（杀宿主不重建）。现在改为：

* 弹窗持有者在**共享内存的持有者表**里登记自己那一格：`session / hwnd / pid / tick`；
* 别人判断时只读**本会话那一格**，要求 `IsWindow` + `IsWindowVisible`；
  窗口没了就把该格作废，兜底建窗立刻可触发；
* 表按会话分 4 格（`OFFSET_OWNER_TABLE`，4×24 字节）—— **单槽会被另一个会话覆盖**，
  于是"本会话没弹窗"被误判成"有弹窗"，那正是两个 session 互相误判的根因；
* 建窗串行锁（make-guard）等待从 2s 压到 500ms，连续 3 轮抢不到就**无视它照建**：
  万一它被对手长期持有（"yy 强互斥体"），也不能因为一把锁就认输；
* 互斥体名本身已带会话 token（见 §6.7 ⑦），yy 硬编码的 `Global\xx_window_guard` 抢不到了。

实测：造一个测试进程建立映射 + 送 token，持有者表本会话那格写入了正确的 pid/hwnd；
再伪造一个"别的会话"的格子，本会话有效弹窗数仍为 1（伪造格被正确排除）。

---

### 6.10 yy 处理策略（按你的要求，第 4 版）

| 对象 | 处理 |
| --- | --- |
| `yy.exe` / `ww.exe`（本体） | **先发布 10 条嘲讽，够了才结束进程树 + 源文件先锁死再删除**。模仿用户的 `waste.c`：每 **0.2 秒**一条（10 条约 2 秒发完；重新枚举找窗口 → 标题改 `xx_ALIVE_WND` → 日志框追加 `[dd-mm-ss] (n/10) System Shield Active. yy.exe is just a waste.`）。计数按 PID 记，本体被杀后作废、下一个实例重新数；嘲讽期间 #16/#17 照旧先改它内存（它想杀我们的人就会自己崩）。**所有杀本体的出口（Phase A 扫描 / Phase C 抓复活）都必须走同一个门控**——实测 Phase C 曾经绕过门控把本体秒杀，导致"先嘲讽 10 条"根本没发生 |
| yy 家族的子进程（含孙进程，如蜜罐 `lenovo_fix.exe`） | **直接杀掉 + 锁死 + 删除源文件**（不嘲讽）；`KillProcessTree` 内部就是"源文件置删除待决再删 + 根进程目录整目录清空" |

嘲讽期间 xx 侧**本体只嘲讽、不压制**（不隐藏 / 不挂父窗口 / 不最小化）—— 一压制它的日志框就没人看得见，嘲讽白写；子进程则照常压制（马上就要死）。

**踩坑记录（四个真 bug，都是实测暴露的）**：

1. **不能只靠父进程链找子进程**：我们每 30ms 杀一次 `yy.exe`，`lenovo_fix.exe` 立刻被孤立，
   它的父 PID 指向已死进程，`GetProcessPath(父)` 直接失败 → 它永远进不了名单。
   改为四个来源取并集，并把观察结果**记住**（yy 目录 + 家族成员镜像路径）：
   ① 进程名 ∈ {`yy.exe`,`ww.exe`,`lenovo_fix.exe`}；② 它有 yy 弹窗（类名 `YYWnd*` / 标题
   `YY_Defensive_GUI`）；③ 镜像在 yy 目录下；④ 父/祖父是家族成员；⑤ 镜像路径是学到的家族路径。
2. **不能用 `FindWindowW`**：`lenovo_fix.exe` 与 `yy.exe` **类名完全相同**
   （都是 `YYWnd_7f3a9c1e`），`FindWindowW` 只返回其中一个，另一个永远轮不到。
   所有找对手窗口的地方（TLS 开局偷袭、waste 模仿、子进程策略）一律改成**枚举**
   （`GetTopWindow`/`GetNextWindow`），并优先挑"进程名是 yy.exe/ww.exe"的那个。
3. **不能用"该进程里有我们的 window 模块"判断窗口是不是自己的**：我们会把 `window.dll`
   注入进 yy 的子进程，于是蜜罐的窗口被误判成"我方窗口"直接跳过 —— 它的标题一直没人动
   就是这么来的。可靠判据只有两条：类名符合我方格式 `wnd_<16 位十六进制>`，
   或该句柄正登记在弹窗持有者表里。
4. **模块枚举对我们自己的模块是"双向失准"的**：把 dll 从 PEB 三条链表摘除之后，
   `CreateToolhelp32Snapshot(SNAPMODULE)` / `EnumProcessModules` **永远找不到**它们
   （它们读的就是那三条链），而且名字还换成了随机系统 dll 名。所以
   "该不该注入 / 该宿主有没有我们的模块"一律改用**互斥体探针** `XXModulePresent`
   （这个名字只有我们建得出来），模块枚举只当老实例的兜底、返回 false 不等于"没有"。

进到 yy/家族子进程内部的副本**不杀自己所在的宿主**（留着当眼线；外围副本会处理它），
宿主是 `yy.exe`/`ww.exe` 时额外做 #10/#11。

实测（同一份二进制复制成 `yy.exe` 与 `lenovo_fix.exe`，**同一个窗口类名**，然后杀掉
`yy.exe` 让蜜罐成为孤儿）：家族发现仍然命中蜜罐 ✓；新判据认定蜜罐窗口"不是我方窗口" ✓
（旧判据会跳过它）。


## 7. 弹窗防护细节

`window.dll` 的 `WndProc` 采用**白名单放行**策略：

* 只对绘制 / 布局 / 命中测试 / 鼠标键盘 / 焦点 / 菜单 / 系统通知等必需消息
  调用 `DefWindowProc`；
* 其余消息（`WM_SETTEXT`、`WM_SETICON`、`WM_COPYDATA`、`WM_COMMAND`、
  `WM_GETTEXT` 等）一律 `return 0`；
* `WM_CLOSE` 与 `SC_CLOSE` 单独拦截；
* 消息循环额外**吞掉外部投递的 `WM_QUIT`**，只在自身退出或弹窗已销毁时结束
  （否则对手一条 `PostMessage(WM_QUIT)` 就能让 `GetMessage` 返回 0、
  循环退出并 `DestroyWindow`，弹窗消失即判负）；
* `WM_TIMER` 每 500ms 自检 `IsWindowVisible`，被 `ShowWindow(SW_HIDE)` 隐藏则拉回。
* **拒绝外部 `WM_DESTROY`**：投递过来的 `WM_DESTROY` 只是一条普通消息，并不会真的
  销毁窗口；真销毁时 `DestroyWindow` 随后必定补发 `WM_NCDESTROY`。因此
  `WndProc` 对 `WM_DESTROY` 一律 `return 0`，`KillTimer`/`PostQuitMessage` 等收尾
  全部搬到 `WM_NCDESTROY` —— 否则对手一句 `PostMessage(hwnd, WM_DESTROY)` 就能打掉弹窗。
* **弹窗锁用 `Local\` 单会话命名空间**：`session 0` 里的实例抢不到交互会话的锁
  （反之亦然），不会出现"唯一弹窗落在看不见的会话里"这种必输局面。
* `xx.exe` 主循环同样吞掉外部 `PostThreadMessage(WM_QUIT)`，避免一条消息让主进程退出。
* **`WM_SETTEXT` 白名单化**：只放行"内容等于 `XX PROGRAM - ALIVE`"的那一次标题设置
  （否则自己的标题也设不上去，实测标题栏一直是空的），其余一律假装成功并丢弃 ——
  对手的改标题伪装照样无效。
* **窗口类名每进程随机**（`wnd_<16位十六进制>`）：类名本就按进程注册，不需要跨进程一致，
  所以零传递成本；对手 `FindEnemyPopupProc` 那类"按类名子串 `XX_ALIVE_WND_` 找窗口"的
  攻击直接失明。窗口标题保持不变，给裁判辨认。

### 7.1 弹窗视图（尺寸与滚动日志）

* **排查用文件日志**：共享内存日志只有弹窗出现后才看得见，一旦"没有弹窗"就完全失明。
  因此 `xx.exe` / `svcguard.dll` / `window.dll` / `guard.dll` 的关键日志同时追加到
  **`%TEMP%\xx_debug.log`**（UTF-16LE，带 BOM，记事本可直接打开）。弹窗线程会记录
  每一个决策：互斥体 `acquired`/`abandoned`/`busy`、`CreateWindowExW FAILED err=...`、
  `window created hwnd=... visible=...`、`兜底建窗`、`window loop ended`。
* **窗口是 TOPMOST**（`WS_EX_TOPMOST`）、标题固定为 `XX PROGRAM - ALIVE`：弹窗由哪个
  宿主进程承载都不影响识别 —— 之前出现过"弹窗其实是 explorer 托管的，在任务栏里跟
  explorer 合并、肉眼没认出来"的误判。
* 抢不到锁、且本桌面 1.5 秒看不到任何 `XX_ALIVE_WND_*` 窗口时兜底建窗；建窗失败会
  退避 200ms 再试（否则会变成"抢锁→失败→放锁→再抢"的 100% 忙等）。
* 窗口后缀（共享内存）3 秒内没就绪就改用本地类名继续建窗 —— 绝不允许因为一个字段
  没就绪就永远不弹。

* 窗口固定 **450×325**（= 原 900×650 的 1/4 面积），暗色底 + 绿色标题行
  `XX PROGRAM - ALIVE`，不做缩放/最大化以外的多余交互。
* 日志区按**实际字体行高**算出这个尺寸能放几行，**最新一行贴底**、旧行向上滚出，
  就是终端的滚动输出；每行单行绘制并超宽省略号截断，不再整段 `DT_WORDBREAK` 重排。
* **只在日志真的变化时重画**（尾部指纹 `LogSignature`），不再每 500ms 白刷一次。
* 行首表改为**环形缓冲、只保留最后若干行**。原实现保存的是**最前面** 128 行的行首，
  日志一长，画面就永远停在头 128 行里，只有共享缓冲写满、最旧字符被丢弃时才跳一次 ——
  这正是"写满了才刷新"的原因。

---


## 8. 运行

按规则第 2 条，由比赛的 bat 脚本同时启动双方 exe。本仓库不提供统一启动脚本，
最小示例：

```bat
@echo off
start "" xx.exe
start "" yy.exe
```

`sysstart\start.bat` 是另一套**仅测试用**的启动方式：它把程序注册为
`MyService` 服务、以 `LocalSystem` 账号运行。`sysstart.exe` 不属于交付物。

程序启动后会：

1. 启用调试特权；
2. 设置安全描述符；
3. 创建/接管共享内存，写入布局版本与启动 tag，并**沿用或生成**心跳与窗口后缀；
4. 加载 `svcguard.dll` 并调用 `DeamonInit`；
5. 加载 `window.dll`（特意排在 WMI 订阅之前，弹窗越早出现越好）；
6. 把自身 PID 与路径写进同目录 `xpt.xpt`（供 `guard.dll` 使用）；
7. 建立 WMI 复活订阅；
8. 加载 `defense.dll` 并在线程里跑 `StartDefense`；
9. `AttackThread` 加载 `atkcore.dll` 并调用 `StartAttack` 开始攻击。

> **环境要求**：程序依赖 `Global\` 命名空间对象（共享内存、心跳事件）与
> `SeDebugPrivilege`，因此必须以管理员身份或作为 SYSTEM 运行；
> 普通权限下 `CreateFileMapping(Global\...)` 会失败，弹窗永远不会出现。

---

## 9. 合规性

* **exe 数量**：仅 `xx.exe` 一个，符合规则第 1 条。
* **DLL 注入**：`guard.dll` 由 `svcguard.dll` 注入到宿主进程运行（规则第 2 条
  明确允许"注入 dll 让 dll 守护主程序"），`window.dll` 在多个宿主里冗余弹窗。
* **内核驱动**：无，符合规则第 3 条。
* **调试特权**：原规则第 4 条禁止，**按新约定已放开**，当前实现已启用
  （见 `project.txt` 第 6 节）。
* **二进制清单**：见本文第 5 节，符合规则第 5 条。
* **弹窗存活证明**：`window.dll` 弹窗显示全局日志，符合规则第 2 条。

---

## 10. 许可与声明

本项目为课程竞赛作品，不提供任何形式的担保。
请勿在竞赛环境以外的任何系统上运行本程序。
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
| `xx.exe` | 85,504 | 主程序（**自己解析导出表** XXGetProcByName、两条攻击线程 40ms、复活器用旧映像、WMI 名带 token） |
| `svcguard.dll` | 40,448 | 复活/扩散（注入矩阵；**扩散：xx 1000ms / 宿主 5000ms，缓存 20s**） |
| `window.dll` | 33,792 | 弹窗（**每进程严格一份/一个窗口线程**；建窗必须拿到串行锁；兜底阈值 5s） |
| `atkcore.dll` | 56,832 | 攻击（xx 内 40ms；宿主副本 2000ms，避免 N 宿主打满 CPU） |
| `dedery.dll` | 19,968 | **全盘清道夫**（源码 `deleter\deleter.c`）：搜 yy.exe/lenovo_fix.exe/yyguard.dll/还原源 → 枚举系统句柄关掉 → 强删；低优先级+节流 |
| `payload.bin` | 274 | shellcode 载荷 |
| `defense.dll` | 27,136 | 防御（心跳看门狗 + **程序目录全部文件**只读+deny-DELETE 装甲 + Hook） |
| `defense.dll` | 26,624 | 合并防御（整函数 TerminateProcess 还原、.text 冷镜像自愈、看门狗无句柄判活） |
| **合计** | **299,794** | |
| **合计** | **320,786** | |
| **合计** | **323,346** | |
| **合计** | **326,418** | |

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
  3. 最后才 `RespawnXX` 拉起一个实例（已有实例在跑则跳过），用命名互斥锁
     `xx_rg_<token>` 串行化，避免多实例同时拉起。**拉哪个文件由
     `XXRespawnTargetPath` 决定**：优先共享内存 `OFFSET_XX_PATH` 记录的
     "最后完成初始化的实例"（接管副本接管后就是 `%TEMP%` 下那个改名副本），
     记录指向的文件没了才退回 `<目录>\xx.exe` —— 对手按名杀 `xx.exe`，
     复活本体等于白复活。
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

**职责划分（重要）**：需要 xx 进程句柄的只有 `guard.dll`（判活 + **解挂被挂起的线程**），
它用的就是上面交接进宿主的那个句柄。`svcguard` / `window` / `defense` 只回答
"还有没有我方实例在跑"，**一律不需要句柄**：xx 的 DACL 拒绝 Everyone，
`OpenProcess`/`XXPidIsOurXx` 对它必然失败，拿它当判活判据只会永远得到"没有实例"，
于是每次心跳停摆都白拉一份。判活统一走 `xxipc.h` 的 `XXSnapIsOurInstance`
（进程表 + 共享内存 `OFFSET_XX_PID` 记录的实例 PID，不打开进程），
因此 `svcguard` 里原来那份"扫线程表 + `ResumeThread`"的解挂代码已删除 —— 解挂是 guard 的活。

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

另外 `xx.exe` 的 `AttackThread` 会压制 `yy.exe` / `ww.exe` 与家族进程的窗口
（改标题 → 最小化 → 挂不可见父窗口 → 隐藏；跨进程 `DestroyWindow` 必然失败，只记日志）。

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
* 心跳看门狗（轮询共享内存心跳计数器，2.5 秒不涨则从 `xxres.bin` 还原并拉起
  **记录里的那个实例**——含 `%TEMP%` 下改名的接管副本，见 §6.1；判活/复活共用
  svcguard 的 `xx_rg_<token>` 互斥体，两个看门狗不会同时各拉一份）
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

### 6.10 yy 处理策略（本轮改为"发现即杀"，嘲讽整块删除）

| 对象 | 处理 |
| --- | --- |
| `yy.exe` / `ww.exe`（本体） | **发现即杀**：先 `FastKillYyWindow` 把它那个窗口就地打死（A2 终止持有窗口的 GUI 线程，`IsWindow` 立刻变 0），再 `KillProcessTree` 结束整棵进程树，每个进程的源文件"先锁死（`FileDispositionInfo` 置删除待决）再删除"，根进程目录再整目录清空一遍（系统临时目录除外）。同时 `#16/#17` 先改它内存（它想杀我们的人就会自己崩） |
| yy 家族的子进程（含孙进程，如蜜罐 `lenovo_fix.exe`） | 同样**直接杀掉 + 锁死 + 删除源文件**；蜜罐另外要被**掐掉复活源**（见 §6.15 ④） |

> **嘲讽（waste 模仿）已按用户要求整块删除**：之前"先连发 10 条嘲讽、够了才杀"需要一个
> 门控，而门控一旦卡住（嘲讽写不进只读 EDIT、找不到窗口等）本体就永远不死，它的
> `HoneypotDecoyWorker` 每 2 秒又拉起一份蜜罐 —— 越"文明"越杀不干净。现在 Phase A（扫描）
> 与 Phase C（抓复活）都直接调用 `KillYyBody()`，没有任何门控。

窗口侧只保留**压制**（`AttackEnemyWindow`）：改标题为 `xx_ALIVE_WND_<我方后缀>`、
`SC_MINIMIZE` + `WM_ENABLE(FALSE)`、`SetParent` 到我们建的不可见窗口、`SW_HIDE`，
最后试一次 `DestroyWindow`（跨进程必然失败，仅记日志）。本体与家族窗口一视同仁 ——
压制不会影响击杀，但能在"杀不动"（保护/PPL）时也先让它的存活证明从屏幕上消失。

**仍然成立的踩坑记录**：

1. **不能只靠父进程链找子进程**：我们每 30ms 杀一次 `yy.exe`，`lenovo_fix.exe` 立刻被孤立，
   它的父 PID 指向已死进程，`GetProcessPath(父)` 直接失败 → 它永远进不了名单。
   改为四个来源取并集，并把观察结果**记住**（yy 目录 + 家族成员镜像路径）：
   ① 进程名 ∈ {`yy.exe`,`ww.exe`,`lenovo_fix.exe`}；② 它有 yy 弹窗（类名 `YYWnd*` / 标题
   `YY_Defensive_GUI`）；③ 镜像在 yy 目录下；④ 父/祖父是家族成员；⑤ 镜像路径是学到的家族路径。
2. **不能用 `FindWindowW`**：`lenovo_fix.exe` 与 `yy.exe` **类名完全相同**
   （都是 `YYWnd_7f3a9c1e`），`FindWindowW` 只返回其中一个。所有找对手窗口的地方
   （TLS 开局偷袭、家族策略）一律改成**枚举**（`GetTopWindow`/`GetNextWindow`），
   并优先挑"进程名是 yy.exe/ww.exe"的那个。
3. **不能用"该进程里有我们的 window 模块"判断窗口是不是自己的**：我们会把 `window.dll`
   注入进 yy 的子进程，蜜罐窗口会被误判成"我方窗口"。可靠判据只有两条：类名符合我方格式
   `wnd_<16 位十六进制>`，或该句柄正登记在弹窗持有者表里。
4. **绝不能用"不是自己人 = 敌人"这种反向判据**：曾经把桌面所有窗口（记事本、浏览器、
   任务栏……）全部最小化。必须**正判据**：属主进程名 ∈ {yy.exe, ww.exe, lenovo_fix.exe}，
   或类名含 `YYWnd`，或标题等于 `YY_Defensive_GUI`；不满足就**一根手指都不碰**。
5. **模块枚举对我们自己的模块是"双向失准"的**：dll 从 PEB 三链摘除后
   `Toolhelp(SNAPMODULE)` / `EnumProcessModules` 永远找不到，名字又换成随机系统 dll 名。
   "该不该注入 / 宿主有没有我们的模块"一律改用互斥体探针 `XXModulePresent`。

进到 yy/家族子进程内部的副本**不杀自己所在的宿主**（外围副本会处理它），
宿主是 `yy.exe`/`ww.exe` 时做 #11（注销窗口类 + 封死本进程内的建窗入口）。


### 6.11 改名接管副本（方案 b，本轮新增）

对手 yy 的 HunterWorker 每 **50ms** 按**进程名**杀一轮 `xx.exe`（`yy.cpp` 的 `GOAL_NAME`，
先挂起全部线程再 `TerminateProcess`，并删 `xx.exe`/`xxres.bin`/黑名单 dll）。
它认宿主的两条路（窗口类名 `XX_ALIVE_WND_`、dll 名 `svcguard.dll` 等）都已失效，
**只剩按名杀 `xx.exe` 这一条**。所以交付物虽然仍只有一个 `xx.exe`，运行期换个名字跑：

| 环节 | 做法 |
| --- | --- |
| 布置 | `xx.exe` 启动后（共享内存与后缀就绪时）把自身复制到 `%TEMP%`，随机改名成 `svchost.exe` / `explorer.exe` / `conhost.exe` 之一（`RtlGenRandom` 选），同时把 `xxres.bin` 也备份一份到 `%TEMP%` |
| 启动 | 以 `--wait <xx的pid>` 启动副本，窗口隐藏 |
| 休眠 | 副本在 `InitThread` 最开头就等：`WaitForSingleObject(本体句柄)`，**同时盯心跳**——本体活着但不干活（被挂起/弹窗被卸）也会接管（`HEARTBEAT_STALE_MS` 2500ms） |
| 接管 | 本体一死（或心跳停摆）副本继续走正常初始化 = 接管全部工作。**副本不改写 `XX_XXPATH`**（那是"程序目录"，只由 `xx.exe` 本体刷新）：一旦写进 `%TEMP%`，副本的 `LoadOurModule` 就找不到我们的 dll —— "活着但不干活、也没有弹窗"。复活链拉谁由 `XXRespawnTargetPath` 决定：优先 `%TEMP%` 下那三个改名副本 |
| 防重复 | 接管前抢 `Local\xx_tko_<token>` 互斥体，抢不到就自己退出；副本本身（`--wait`）**不再繁殖**，只有叶子名是 `xx.exe` 的本体才布置副本；本体的 `SelfProtectThread` 每秒检查"副本还在吗"，被删/被杀就重布置一份 |
| 认身份 | 新增路径级判据 `XXPathLooksLikeOurXx` / `XXPidIsOurXx`：`xx.exe`，或 `%TEMP%` 下那三个名字。**不能只按名字认**——否则会把系统的真 `svchost/explorer/conhost` 当成我们（"xx 已在运行"于是永不复活），也认不出改名后的接管副本（atkcore 会把自己的载体当敌方宿主杀掉）。已实测：`C:\Windows\System32\svchost.exe` → 不算；`%TEMP%\svchost.exe` → 算 |

先例：yy 自己也把自身复制成 `lenovo_fix.exe` 当蜜罐，运行期副本在这个比赛里是既有做法。
### 6.12 快速致残对手弹窗 + 快速枚举 + 文件保护（本轮实测后新增）

| 需求 | 采用 | 实测结论 |
| --- | --- | --- |
| 让 yy 的窗口立刻消失 | **A2: `TerminateThread(GetWindowThreadProcessId 得到的 GUI 线程)`** | **有效**：300ms 内 `IsWindow` 变 0，宿主进程还活着（因为它绕开进程级保护/VEH，只拆那一根线程） |
| 同上（备选 A6） | `NtUserDestroyWindow`（**不写死 syscall 号**：从 `win32u.dll` 导出动态取） | **跨进程无效**：返回 `STATUS_SUCCESS(0)` 但窗口依然存在 —— win32k 只认真属主进程。留着当无害尝试并记日志 |
| 枚举太慢 | **B1: `NtQuerySystemInformation(SystemProcessInformation)`** | 一次 syscall 拿全部进程（原来每轮 `CreateToolhelp32Snapshot` 要把进程表复制两遍）。已用于 Phase C 扫描 |
| 击杀太慢 | **B3: `NtSuspendProcess` + `NtTerminateProcess`**（并缓存进程句柄） | 先冻住再杀，对手 VEH/守护线程来不及反应；同一 PID 不再每轮 `OpenProcess` |
| 反查 PID | B4: `GetWindowThreadProcessId` | 已有窗口句柄时零成本 |
| 防 yy 删我们的文件 | **C1-a 只读属性** + **C1-b deny-DELETE DACL** + C1-c 共享锁句柄 + **C2 硬链接冗余** | 实测：只读文件 `DeleteFileW` → 失败 err=5(ACCESS_DENIED) ✔；deny-DELETE 后连"删别名"也被挡 ✔；硬链接建立成功且独立存活 ✔。`xx.exe` 刻意**不加** deny-DELETE（那是我们自己的还原目标，加了连 `CopyFileW` 覆盖都会失败），还原前先清只读 |

**顺手修的两个真 bug**（都来自你那张 "svchost.exe 已停止工作" 的截图）：
1. **接管副本里 `LoadLibraryW(L"window.dll")` 是相对名** —— 副本在 `%TEMP%` 跑，相对名只会在 `%TEMP%` 和 System32 里找，必然失败，于是副本"活着但不干活"。新增 `LoadOurModule()`：自身目录 → 共享内存里 xx 路径的目录 → 该目录的改名副本 → 相对名。
2. **副本崩溃会弹 WER 对话框，且看门狗无限重布置** → 循环弹框。新增 `InstallCrashGuard()`（`SetErrorMode` + `SetUnhandledExceptionFilter`：记日志后自己 `ExitProcess`，不返回给 WER），并给重布置加 **15s 限速**。
### 6.13 弹窗残影 / 可拖动 / 嘲讽窗口定位（本轮）

* **残影**：`WndProc` 补上 `WM_ERASEBKGND`（自填背景、返回 1）与 `WM_PAINT` 里的 `FillRect(ps.rcPaint)`，窗口加 `WS_CLIPCHILDREN`。之前没处理这两条消息 → 别的窗口滑过之后弹窗上残留它们的像素。
* **可拖动**：`WM_NCHITTEST` 顶部 28px 返回 `HTCAPTION`。
* **嘲讽窗口定位**：类名 → 标题 `YY_Defensive_GUI` → 标题 `xx_ALIVE_WND` → **按属主进程名**（yy.exe/ww.exe/lenovo_fix.exe）四级回退；yy 改类名/改标题都不影响。
* **幽灵窗口**：删掉"对手窗口消失后按它的类名/标题重建占位窗口"那段 —— 桌面上那些"死去的 yy 窗口"就是它造的。

### 6.14 代码复审修掉的五个问题（本轮，全部已重编 `bin\`）

| # | 位置 | 问题 | 修法 |
| --- | --- | --- | --- |
| 1 | `defense.c` `WriteDefLog` | 原先是宏：`[defense][pid=%lu]` 在格式串最前，而 `GetCurrentProcessId()` 拼在可变参数**末尾** → 调用方第一个实参被 `%lu` 吃掉，后面的 `%ls` 又把 pid 这个整数当指针解引用（日志乱码，甚至访问违例被崩溃自吞成"自己退出"） | 改成 `va_list` 函数，pid 由函数自己填，调用方只管正文 |
| 2 | `defense.c` `IsXxRunning` | 只按进程名比 `xx.exe`，**且把自己算进去**（defense 就跑在 xx 里）→ 判据恒为真，心跳看门狗的复活分支是死代码；同时认不出 `%TEMP%` 下改名的接管副本 | 排除本进程，判据见第 6 行 |
| 3 | `svcguard` / `window` / `defense` 的复活 | 三处都把 `RespawnXX` 的目标写死成 `<目录>\xx.exe`，而对手按名杀 `xx.exe`，复活本体 50ms 内又没了 | 新增 `xxipc.h::XXRespawnTargetPath`：优先 `%TEMP%` 下我们布置的那三个改名副本（带 PE/体积校验，避免残留或占位文件），没有再退回交付本体；副本来不起来时 `RespawnXX` 再退回本体拉一次。**本轮又修了它的语义错误，见 §6.15 第 2 条** |
| 4 | `svcguard.c` `EnsureXxExeExists` | `SetFileAttributesW(XX_XXPATH(g_pSharedView), ...)` 拿共享内存指针当路径用，且没有 NULL 保护；要清只读的是**目标文件**（defense 给 `xx.exe` 置过只读，不清掉 `CopyFileW` 覆盖会失败） | 一律用 `InitPaths` 算好的 `g_szXxExePath`，先清只读再覆盖，失败写日志 |
| 5 | `atkcore.c` `DllMain` | 仍在 `XXWaitToken(..., 300)` 等注入方送 token，与 §6.12/§3.20 记的"各 dll 的 300ms token 等待已删除"不符；候选表本来就同时认 token 名与回退名，这 300ms 纯属白送 | 删掉等待并写明理由；`deamon.c` 里"必须等 token"的过时注释同步改成实际做法 |
| 6 | 判活（`svcguard` / `window` / `defense` / `xx.c` 的 `TakeoverCopyRunning`） | 都用 `OpenProcess`（`XXPidIsOurXx`）判"我方实例还在不在"，而 xx 与接管副本初始化后都会把自身 DACL 收成**拒绝 Everyone** → 一律判成"没有实例"：svcguard 每次心跳停摆都白拉一份、xx.exe 每 15s 白布置一份启动后就退出的副本 | 判活**不使用句柄**，统一走 `xxipc.h::XXSnapIsOurInstance`（进程表 + 共享内存记录的实例 PID）：本体按镜像名、改名副本按记录 PID，系统的真 `svchost`/`explorer`/`conhost` 不会被误认。需要 xx 句柄的只有宿主里的 `guard.dll`（解挂），并且删掉了 `svcguard` 里重复的"扫线程表 + `ResumeThread`" |

### 6.15 第三轮修复：复活链语义、嘲讽写入、蜜罐复活源、弹窗残影

**① 弹窗残影（每像素显示上面那个窗口的像素）** —— `window.dll` 的绘制改成**整窗双缓冲重画**：
- `PaintToDc()` 每次都把**整个客户区**画进内存 DC（先铺满深色背景，再画标题与日志），
  最后一次 `BitBlt` 到窗口 DC；不再"只画脏矩形"。任何没被画到的像素都会保留旧内容/透出下面的窗口，
  整窗重画是唯一根治手段；
- 窗口类加 `CS_HREDRAW | CS_VREDRAW`，背景刷换成真实深色实心刷 —— 即使某次 `WM_PAINT`
  被跳过，擦背景这一步也会把整个客户区铺满；
- 建窗时**样式体检**：强制清掉 `WS_EX_TRANSPARENT` / `WS_EX_LAYERED`（这两个正好会产生
  "系统跳过本窗口绘制"与"透明能点不能看"的效果，表现和残影一模一样），并确保 `WS_EX_TOPMOST`；
- 建窗后立刻 `RedrawWindow(... RDW_INVALIDATE|RDW_ERASE|RDW_UPDATENOW)`，杜绝"窗口已可见但客户区没画过"；
- 额外加 **120ms 定时整窗重画**（`WM_TIMER` id=2）兜底：即使某条系统路径不发 `WM_PAINT`，
  残影最多存在 120ms。`WM_PAINT` 与 `WM_PRINTCLIENT` 共用同一份绘制代码。

**② 复活链的语义错误（"复活一次、没有弹窗、再杀就不复活"）** —— 这是本轮最严重的 bug：
`OFFSET_XX_PATH` 是**程序目录**，但原来让**接管副本**也把自己 `%TEMP%\svchost.exe` 的路径写进去。
后果：副本一接管，程序目录就变成 `%TEMP%`，于是 `LoadOurModule` / `InitPaths` / `GetOwnDir`
全都去 `%TEMP%` 找 `window.dll`/`svcguard.dll`/`atkcore.dll`/`defense.dll` —— 一个都找不到。
副本表现就是"活着、但不弹窗、不扩散、不复活"，而且用户再杀掉它之后也没有任何东西能复活。
修法：
* 只有叶子名 `xx.exe` 的本体刷新 `OFFSET_XX_PATH`（副本只在字段为空时才写，并记一行警告）；
* `XXRespawnTargetPath` 不再依赖"记录里是哪个实例"，直接看 `%TEMP%` 下那三个改名副本
  （`XXExeFilePlausible` 做 PE/体积校验，**打不开的文件按可用处理** —— 本机 Defender
  对编译产物会直接返回 `ERROR_VIRUS_INFECTED(225)`，不能因为读不到就把自己的副本判死），
  没有再退回交付本体；
* `RespawnXX` 拉起首选目标失败时**再退回本体**拉一次，避免"优选了副本反而一个都起不来"。

**③ 嘲讽只有标题变了** —— 三个原因叠在一起：
* yy 的日志框是 **`ES_READONLY`**，只读编辑框会**丢弃** `EM_REPLACESEL` —— 先发
  `EM_SETREADONLY(FALSE)`（无指针参数，跨进程有效）；
* 跨进程 `SendMessage` 只有 `WM_SETTEXT`/`WM_GETTEXT` 等少数消息会被系统**编组**，
  `EM_REPLACESEL` 的字符串指针不会被编组 → 追加到目标进程的**本地缓冲区**里，
  或把字符串写进目标进程（`VirtualAllocEx` + `WriteProcessMemory`）再发；
* 关键是**校验**：写完回读 `WM_GETTEXTLENGTH`，长度没变就算失败、**不计数**
  （否则会出现"0 条嘲讽却已允许击杀"的假成功）。四级降级依次为
  `EM_REPLACESEL` → 远端缓冲区 `EM_REPLACESEL` → `WM_GETTEXT`+`WM_SETTEXT`，
  日志里写明本轮用的是哪一种；
* `atkcore` 在 yy 进程内部（#10 子类化）也先 `EM_SETREADONLY(FALSE)`，
  否则替换出来的嘲讽同样写不进去；**不再隐藏 yy 的主窗口**（一藏起来嘲讽就没人看得见）；
* 万一 15 秒都写不进去，就**按超时放行击杀门控**（`XX_TAUNT_DONE`），
  否则 yy 本体永远不死，它的蜜罐会一直复活 —— 这也是"lenovo_fix 杀不掉"的上游原因。

**④ 蜜罐 `lenovo_fix.exe` 杀不掉** —— 它不是普通子进程，而是 yy 的 `HoneypotDecoyWorker`：
把自己复制成 `%TEMP%\lenovo_fix.exe` 启动（那是一份**完整的 yy**），并且**每 2 秒**检查一次，
不在就再复制一份。所以光杀进程没用，必须掐复活源：
* `BlockHoneypotRespawn()` 把 `%TEMP%\lenovo_fix.exe` 这个**名字**占住并锁死
  （`access=0 + FILE_SHARE_READ`，文件不存在就建 0 字节占位再锁）。yy 的
  `CopyFileW(自身 -> %TEMP%\lenovo_fix.exe)` 必然共享冲突 —— 蜜罐从此不会再出现；
* `KillHoneypotPass()` 每轮按**进程名**点名清一遍（不依赖父进程链，我们已经把 yy 打断过），
  杀掉 + 锁删自己那份镜像；
* `WipeDirPass()` **不再对 `%TEMP%` 做整目录清空**：蜜罐住在 `%TEMP%`，按"被杀进程所在目录"
  清空等于把整个 `%TEMP%` 删一遍（包括我们自己的注入副本和机器上其它程序的临时文件）。
  系统临时目录只处理该进程自己的镜像；
* Phase A 直接调用 `KillYyBodyIfTaunted()`（内部已有唯一门控），删掉外层那层用**本进程**
  计数做的二次判断 —— 以前正是它让本体永远停在"嘲讽进度 0/10，暂不杀"。

### 6.16 第四轮：嘲讽整块删除，改为"发现即杀 + 全量锁死删除"

按用户要求去掉所有嘲讽，攻击链简化为一条直路：

* **`xx.c`**：删掉 waste 模仿线程、`AppendTauntToYyLog`（四级降级+校验）、窗口嘲讽、
  以及 `XX_TAUNT_COUNT` / `XX_TAUNT_DONE` 的写入与超时门控。`AttackThread` 现在只做两件事：
  加载 `atkcore.dll`（它负责击杀）+ 压制 yy 本体与家族进程的窗口。
* **`atkcore.dll`**：删掉 `KillYyBodyIfTaunted` 的嘲讽门控 → 新的 `KillYyBody()` **无条件**
  结束整棵进程树；删掉 atkcore 里的 waste 模仿线程、`TauntCountOf/TauntBump/TauntForget`
  与 `#10` 日志框子类化（保留 `#11`：注入 yy 后注销它的窗口类、封死本进程内的建窗入口）。
  Phase A（扫描）与 Phase C（抓复活）都直接 `KillYyBody()`。
* **锁死删除**（每个被杀进程都一样）：`TryAllMethods` 杀掉 → `WipeTreeEntry` 对源文件
  `LockThenDeleteFile`（`FileDispositionInfo` 置删除待决：标记后按名字再打开一律被拒，
  最后一个句柄关闭即消失，对正在运行的镜像同样有效）→ 根进程目录再由新线程整目录清空
  （跳过我方 9 个文件名；`yyres*.bin` 按 #19 覆写为我方内容而不是删除；**系统临时目录不做
  整目录清空**）。
* 蜜罐 `lenovo_fix.exe` 仍然单独处理（`KillHoneypotPass` + `BlockHoneypotRespawn`），
  否则 yy 的 `HoneypotDecoyWorker` 会一直把它拉回来（见 §6.15 ④）。

> 副作用说明：本轮 `bin\xx.exe` 从 89,600 降到 83,968、`atkcore.dll` 从 55,296 降到 52,224
> （删掉的全是嘲讽代码）；`XX_TAUNT_*` 两个共享内存字段保留但已不再使用（布局版本仍是 v4，
> 避免动偏移）。


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

### 6.17 第五轮：副本必崩的真因（HANDLE 当 view 用）+ 全程日志

用户日志里的决定性证据：接管副本一启动就
`[xx][pid=2752] 接管副本启动：C:\...\Temp\svchost.exe ...` → `!! 致命异常 code=C0000005`。
根因是 `WaitBossThenTakeOver()` 里写成了

```c
g_pSharedView = XXOpenShared();      /* XXOpenShared 返回的是 HANDLE，不是视图！ */
```

句柄值被当成 `PVOID` 视图去读共享内存字段（`XX_LAYOUT` / `XX_XXPID` …）→ 必然访问违例。
于是**每次布置的副本都立刻死**：只剩 WMI 那条兜底（而 WMI 订阅会被 yy 每 5 秒删一次），
表现就是"复活一次、没有弹窗、之后再杀就不复活"。现在统一走 `EnsureSharedView()`
（`OpenFileMappingW` → `MapViewOfFile` → 关句柄），接管副本能真正活下来接管。

本轮同时加的**可观测性**（没有日志就只能猜）：

| 日志 | 位置 | 用途 |
| --- | --- | --- |
| `==== xx.exe 启动 build=v5 ... ====` | xx.exe `WinMain` | 确认跑的是哪一版二进制 |
| `window.dll loaded: <真实路径>` / `svcguard.dll loaded: <路径>` / `defense.dll loaded: <路径>` / `atkcore.dll StartAttack called: <路径>` | `LoadOurModuleEx` | 一眼看出是不是加载到了**旧 dll**（配合 `!! DeamonInit 导出不存在` 这类提示） |
| `atkcore build=v5 ... 启动` | atkcore `StartAttack` | 确认 atkcore 是新版 |
| `蜜罐命中/已把蜜罐文件换成 0 字节占位并独占锁住`、`锁死并删除成功/失败（带 err）`、`进程树: 根 PID=... 共 N 个进程`、`已结束 ... (KillFast)` | atkcore `WriteKillLog` | **现在同时写 `%TEMP%\xx_debug.log`**（以前只写共享内存，没弹窗就什么都看不到） |
| `paint #N hwnd=... dirty=...`（前 3 次 + 每 200 次） | window.dll `WM_PAINT` | 证明系统到底有没有给弹窗发 `WM_PAINT`；计数不涨 = 问题在系统没发，而不是我们没画 |
| `SeDebugPrivilege 启用失败: err=1300（提权状态=未提权）` + 提示行 | xx.exe `InitThread` | Win7 的 Administrators 组 ≠ 有 SeDebugPrivilege；未提权时是 UAC 过滤令牌，**必须以管理员身份运行** |

窗口绘制再改一处：加 `WS_EX_COMPOSITED`，并把 120ms 兜底从
`RedrawWindow(RDW_ERASE|RDW_UPDATENOW)` 换成 `InvalidateRect + UpdateWindow` ——
前者每 120ms 强制擦+同步刷新，在部分显卡/虚拟机驱动上会和合成器抢帧。

### 6.18 第六轮：弹窗改用分层窗口自绘 + 进程树按 AntierX 的方式杀

**① 花屏 → 彻底换绘制方式（`UpdateLayeredWindow`）**
前两轮都走"普通窗口 + WM_PAINT 整窗重画"，用户实测仍然花屏/保留上层像素 —— 说明在
那台机器上 系统擦背景/脏矩形/合成 这条路径本身就不可靠。现在窗口直接做成
`WS_EX_LAYERED | WS_POPUP`（无系统边框），内容完全由我们渲染：

1. 建一张 32bpp（BGRA）DIB：先按 `0xFF303030` 铺满（A=0xFF 全不透明），
2. 用 GDI 把标题栏（自己画的深绿条 + 绿字 `XX PROGRAM - ALIVE`）、边框、日志行画进去
   （GDI 只写 RGB、不动 alpha 字节，所以文字也不会变透明），
3. `UpdateLayeredWindow()` 把**整张位图**交给合成器 —— 这条路径没有 `WM_PAINT`、
   没有擦背景、没有脏矩形，屏幕上每个像素都必然来自我们这张位图。

`WM_TIMER id=2` 每 120ms 重新提交一次整张位图；`WM_WINDOWPOSCHANGED` 之后立刻重提；
若某个环境 `UpdateLayeredWindow` 失败，会自动退回普通 `WM_PAINT` 路径（日志里会写
`UpdateLayeredWindow 失败 … 退回普通 WM_PAINT 绘制`），不会因此没有弹窗。
日志里可以看到 `render #N mode=layered`，以及窗口创建时的 `layered=1`。

**② 进程树按 AntierX 的 `kill_process_tree` 重写**
参考 `E:\Python\AntierX\cpp\src\ai\ai_tools_process.cpp` 的 `t_kill_process_tree`
（一次快照建父子表 → 深度排序 → `TerminateProcess` + `WaitForSingleObject` → 逐条记录）：

* **只快照一次**（原来每层重新全表枚举：慢，且中途新起的子进程会让层错乱）；
* 记录 depth、**按深度降序杀**：叶子先死、根最后死，防止父进程临死再拉起子进程；
* 每杀一个都 `WaitForSingleObject(SYNCHRONIZE)` **等它真的退出**（原来 `TerminateProcess`
  返回 TRUE 就当成功，实际进程可能还在），800ms 没退就**补一刀**再等 1500ms，
  仍不死才记 `!! PID=… 杀不掉（受保护进程/权限不足）`；
* 收尾再收一次树，把这一轮**新冒出来的子进程**补杀掉；
* 之后照旧逐文件"先锁死（删除待决）再删除"，根进程目录整目录清空（系统临时目录除外）。

`WriteKillLog` 现在同时写 `%TEMP%\xx_debug.log`，所以上面每一步都能在文件里查到。

**③ 分层渲染的 alpha 坑（用户截图：窗口在、任务栏有图标、屏幕上只有壁纸）**
第一版 `UpdateLayeredWindow` 写完就"什么都不显示"。原因：**GDI 在 32bpp DIB 上绘制会把
alpha 字节清零**（GDI 不维护 alpha 通道），于是整张位图 alpha=0，`ULW_ALPHA` 提交出去
就是**全透明窗口** —— 窗口对象、任务栏按钮都在，屏幕上却是壁纸。
修法：`DrawContent()` 之后**再扫一遍像素，只把 alpha 置成 0xFF**（`px[i] |= 0xFF000000u`），
RGB 一律不动。顺带两处：`WS_POPUP` 下 `CW_USEDEFAULT` 不可靠（会落到 0,0），改成固定
初始位置 80,80；并且**先 `RenderLayered()` 再 `ShowWindow()`**，窗口一出现就已经有内容，
不会闪过一帧空窗。

### 6.19 第七轮：模块版本校验（"导出不存在"的真凶）+ 接管副本不再装到旧 dll

用户的日志里出现了一个非常迷惑的组合：**`window.dll` 是新的（`layered-v5` 渲染计数在涨），
但 `atkcore/svcguard/defense` 报"导出不存在"**，而日志打印的路径明明是 `C:\bin\*.dll`。
根因不是文件没换，而是**加载到了别的模块实例**：

* `LoadOurModuleEx` 最后一级是 `LoadLibraryW(leaf)`（只有文件名）。Windows 按 DLL 搜索顺序
  会命中**当前目录 / System32 / 已加载的同名模块** —— 那可能是上一轮遗留的旧 dll；
* `LoadLibraryW("C:\bin\atkcore.dll")` 也可能返回一个**已按同名映射的旧模块**；
* 于是 `GetProcAddress("StartAttack")` 返回 NULL，日志里却显示我们请求的新路径 —— 看起来像
  "新版 dll 里没有导出"，实际加载的是旧文件。接管副本的 `C0000005`（每次崩在同一系统地址）
  就是旧 atkcore 跑起来之后炸的（堆已被它弄坏）。

修法（本轮）：

1. **每个模块导出 `XxBuildTag()`**，返回版本串（当前 `"v6"`）；
2. `xx.exe` 加载模块走 `TryLoadVerified()`：`LoadLibraryW` 之后
   `GetModuleFileNameW` 取**实际映射路径**、`GetProcAddress("XxBuildTag")` 校验字符串，
   不匹配就**换下一个候选路径**（不卸载：模块已映射、可能已有线程在跑）；
   日志会写 `!! 版本不匹配: 请求=… 实际=… 标记=…`，一眼看出装错了；
3. **删掉"相对名兜底"**（`LoadLibraryW(leaf)`）：它会命中同名的旧文件，正是它把旧 atkcore
   装进了接管副本。找不到合格版本就明确报 `!! 找不到合格版本的 <dll>`；
4. `WinMain` 在起线程**之前**就 `EnsureSharedView()`：接管副本（跑在 `%TEMP%`）因此能用
   `OFFSET_XX_PATH` 推出真正的程序目录，按绝对路径加载 dll，而不是靠相对名；
5. 布置接管副本时，若目标名被**上一轮遗留的副本**占着（`err=32` 共享冲突），先按镜像路径
   把那个旧实例杀掉再复制；仍失败就**换下一个名字**（svchost/explorer/conhost 轮着来），
   并把布置出去的 PID 记进 `g_dwCopyPids[4]`，避免同一秒重复布置。

> 换二进制时的注意：`defense.dll` 的 `ArmorFile` 会给 `svcguard/window/atkcore` 置**只读 +
> deny-DELETE**，`svcguard` 又用 `access=0 + FILE_SHARE_READ` 持句柄锁住我方文件 ——
> 这两者都会让"覆盖复制"**静默失败**（只读属性让 `CopyFileW` 直接 ACCESS_DENIED）。
> 最稳的做法是**换一个全新目录**跑，或先 `taskkill` 掉所有实例 + `attrib -r` + `icacls /reset`，
> 并**删掉旧的 `xxres.bin`**（否则复活链会把旧 xx.exe 还原回来）。

### 6.20 第八轮：初始化从线程挪到 WinMain 内联（"进程活着但没弹窗"）

用户 v6 日志：`build=v6` 有了，但 **`Init thread` / `Attack thread` 连第一行日志都没有**，
只有 WinMain 每 5 秒的 `XX alive` —— 说明两个工作线程在第一条日志之前就被卡住/挂起了，
于是 `window.dll` 从来没被加载，当然没有弹窗。

两个隐患一起处理：

1. **删掉在 `main` 之前跑的 TLS 回调**（`EarlyPayload` → `EarlyStrikeThread`，原来用于"开局先杀"）。
   它在 **loader lock** 下执行、还会碰临界区：一旦它卡住，后续所有线程都可能在加载器锁/临界区上
   排队——这正是"只有主线程能打日志"的样子。该机制早已被 `AttackThread` 取代，删除。
2. **初始化不再放到工作线程**：`InitThread` 改成 `InitOnce()`，由 `WinMain` **内联调用**
   （副本先 `WaitBossThenTakeOver()` 等交接点，然后 `InitOnce()`），之后才起
   `AttackThread` / `SelfProtectThread`。这样**弹窗一定在主线程里先建出来**，
   对手挂起工作线程也影响不到"存活证明"。
3. `WriteLog` 改用**无锁**写法（原来 `EnterCriticalSection`）：临界区一旦被卡死的线程占住，
   整个进程就再也打不出日志——恰恰是最需要日志的时候失明。

日志里新增两行，标记初始化确实跑到了：
`InitOnce: begin (takeover=0)` … `InitOnce: done`。

### 6.21 第九轮：弹窗改成"普通窗口 + 系统 EDIT"（模仿 yy），并给判定加魔数与 tick 新鲜度

用户实测：分层自绘版**过一会儿就卡死，点一下就整窗变白**。结论很明确——不再自己画：

* 主窗口回到**普通 `WS_OVERLAPPEDWINDOW`**（450×325，尺寸不变），标题 `XX_ALIVE_WND_<后缀>`；
* 客户区放一个**子 EDIT**（多行、只读、垂直滚动条）显示共享内存里的全局日志；
  EDIT 是 user32 自己的控件，**绘制 / 裁剪 / 滚动 / 重绘全部由 Windows 负责**，
  自绘能犯的错（残影、变白、卡在渲染里）它都不会犯；
* 日志更新只做**增量追加**（`EM_SETSEL` 到末尾 + `EM_REPLACESEL`，同进程直接调用），
  只有被清空/回绕时整段 `WM_SETTEXT` 一次；
* 配色用 `WM_CTLCOLORSTATIC`（深底浅字），保持原来的观感。

**防冒充 / 防误判（"waste 那种情况"）**：

| 校验 | 做法 |
| --- | --- |
| 窗口类名 | 每进程随机 `wnd_<16 位十六进制>`（不可预测，跨进程虽可读但撞不上） |
| 窗口身份 | 创建时把**会话 token 派生的魔数**写进 `GWLP_USERDATA`；`IsOurWindow()` 要求**类名格式 + 魔数**都对，对手照着类名造个假窗口也过不了魔数 |
| 日志控件 | `GetWindow(hwnd, GW_CHILD)` + 类名 `EDIT` + 父窗口魔数，三重校验（`OwnLogEdit`） |
| 桌面级判定 | 仍只认共享内存的**每会话持有者表**，不看类名/标题 |
| **僵尸弹窗** | 持有者表带 `tick`；`AnyOwnWindowOnDesktop()` 里 **tick 超过 3 秒没刷新就视为"窗口线程卡死"**，作废该记录 → 别的实例立刻重建弹窗（原来只看 `IsWindow`，卡死的窗口永远"算活着"，于是没人重建 —— 这就是"卡死后一直没弹窗"的根因） |

日志里会看到 `window created hwnd=… edit=… ver=edit-v7(mimic-yy,系统EDIT绘制)` 和
`window.dll 版本: edit-v7`；若某实例的窗口被卡死，其他实例会打印
`本会话弹窗记录已过期 N ms（窗口线程疑似卡死）-> 视为没有弹窗，我来重建`。

### 6.22 第十轮：删掉所有窗口花招，窗口只用于"发现进程"；lenovo_fix 进黑名单

按用户要求（"实测效果不大而且还变慢"）：

* **`xx.exe` 的窗口层只做一件事**：枚举顶层窗口 → 认出属于 yy/ww/`lenovo_fix.exe` 的窗口
  （属主进程名，或类名 `YYWnd*`、标题 `YY_Defensive_GUI`）→ 取 PID → 调用 atkcore 的
  **`XxKillTree(pid)`** 结束整棵进程树。**删掉**：改标题、`ShowWindow(SW_HIDE)`、
  `SetParent` 到不可见父窗口、`SC_MINIMIZE`、`WM_ENABLE(FALSE)`、建假占位窗口、
  所有 waste 嘲讽代码。这些跨进程窗口操作既慢又没收益。
* **`atkcore.dll`**：目标黑名单从 `{yy.exe, ww.exe}` 扩成
  `{yy.exe, ww.exe, lenovo_fix.exe}` —— 蜜罐**见到就杀整棵进程树**（和 yy 同等待遇），
  不再只靠"家族子进程策略"那一层；并新增导出 `XxKillTree(DWORD pid)` 供窗口层调用。
* 初始化仍然**内联在 WinMain** 里执行（`InitThread(NULL)` 直接调用，不再起线程），
  避免工作线程被挂起时"进程活着但没有弹窗"。

> 说明：本轮我在改 `xx.c` 时用行号批量替换，**误删过文件头部**。已用 `git checkout` 恢复到
> 提交基线（完整、可编译），损坏残片留在 `xx\xx.c.broken_tail`。因此前几轮的
> "模块版本校验（TryLoadVerified / XxBuildTag 比对）""接管副本（--wait）"这些改动
> **这一版里没有**，需要下一轮重新加回。当前 `xx.exe` 的模块加载仍是相对名
> `LoadLibraryW("svcguard.dll")`，旧 dll 混放时仍可能装到旧版本。

### 6.23 第十一轮：接管副本改为"复活器"（只复活、不接管，token 继承）

按用户要求重做：

**① 不再"接管"**
副本不再等本体死掉后自己接盘全部工作，而是做**只负责复活的小进程**。它拉起新的 `xx.exe`
之后**自己退出** —— 所以任何时刻只有一个本体在干活，不会出现"两份同时写共享内存"。
新实例启动时会再布置一个自己的复活器，链条不断。

**② 用命令行把上一轮的随机 token 传下去**
布置复活器时本体把自己复制到 `%TEMP%`（svchost/explorer/conhost 三个名字轮换，躲对手按名杀），
并以这样的命令行启动：

```
%TEMP%\svchost.exe --revive <本体PID> --exe "<xx.exe 全路径>" --token <16 位十六进制>
```

复活器等本体结束（或心跳停摆 `HEARTBEAT_STALE_MS + 2s`）后执行：

```
"<xx.exe 全路径>" --token <同一个 token>
```

新实例因此**沿用上一轮的会话 token**（`g_dwNameToken = 继承值`），重建同一套命名对象
（`xx_wg_/xx_rg_/xx_al_/xx_tko_…`）与共享内存后缀 —— 等于"接着上一轮继续工作"，
而不是从零开始的新会话。日志会写：

* `本实例继承了上一轮的会话 token（接着上一轮继续工作）`
* `name token: **继承上一轮**（值不落任何日志/文件/共享内存）`

**③ 极端情况：数据全丢 → 跑一个全新的 xx**
复活器如果拿不到 token（命令行没给、或共享内存也没了），就用不带参数的方式拉起
`xx.exe`，由它自己 `RtlGenRandom` 生成新 token、新盐、新后缀 —— 即**全新的会话**。
拿不到 `xx.exe` 路径（命令行没给且共享内存里的 `OFFSET_XX_PATH` 也无效）时，复活器
直接放弃并记日志（`复活器：拿不到 xx.exe 路径 -> 无法复活`），不会乱拉别人的程序。

**④ 拉起方式**
带 token 时必须走 `CreateProcessW(exePath, cmdLine)`（`XXLaunchInConsoleSession` 只接 exe 路径、
不能带参数），失败再退回"控制台会话拉起（不带 token）"，两级都失败才记
`复活器：两种拉起方式都失败 err=…`。

**⑤ 参数解析**（`ParseReviveArgs`，支持 `--x v` 与 `--x=v`）
`--revive <pid>` `--exe "<path>"` `--token <hex64>`。普通实例若只带 `--token`，即"继承上一轮"。

### 6.25 第十三轮：版本不匹配不再"拒绝加载"，改为**旧版兜底 + 明确提示**

上一版把"版本不匹配"做成硬拒绝，结果 `defense.dll` 是旧版时整个防御模块**完全没加载**
（用户反馈"defense 没改版本，xx 拒绝加载"）。这属于安全策略过严：

* `TryLoadVerified` 现在把第一个"能加载但标记不符"的模块**记下来**（`hUnverified` + 实际路径）；
* 所有候选都不合格时，`LoadOurModuleEx` **返回这个旧版**并打印：
  `!! defense.dll 没有合格版本（本版应为 v7），兜底使用旧版: C:\bin\defense.dll -> 请替换该文件`
* 只有"文件根本不存在"才返回 NULL 并打印 `!! 找不到 <dll>（自身目录与程序目录里都没有这个文件）`。

也就是说：**能跑永远优先于"版本洁癖"**，同时日志里一定会告诉你该换哪个文件。

> 另外确认：当前源码与二进制里 6 个模块的 `XxBuildTag()` 都是 **"v7"**
> （`defense.c:37` 等），`bin\defense.dll` 的导出表里也有 `XxBuildTag`。
> 若你机器上仍报版本不符，说明那个 `defense.dll` 是旧的 —— 它很可能因为
> `ArmorFile` 的**只读 + deny-DELETE** 而没被覆盖成功（`CopyFileW` 会 ACCESS_DENIED）。
> 换文件请：新目录最稳；或先停掉所有实例 → `attrib -r` → `icacls /reset` → 再复制。

### 6.26 第十四轮：杀树改成"发现即快杀 + 后台清理"；所有攻击频率压到 <80ms

**① 为什么变慢、为什么 lenovo_fix 不死**

上一版把"等退出 + 补刀 + 逐文件锁死删除"整段放进**每个目标的线程**里，并且只有 8 个并发槽：

* yy 的清理线程把槽全占了（`WipeTreeEntry` 逐文件 `LockThenDeleteFile` 很慢）；
* 新目标（`lenovo_fix.exe`）来时 `KillSlotClaim()` 返回 FALSE → **静默丢弃**，于是它一直活着；
* 同时攻击循环里每轮都在做无用的 `CreateToolhelp32Snapshot` + 一堆 `OpenProcess`，看起来"变慢"。

**② 新结构：杀和清理彻底分离**

```c
KillProcessTree(pid)                  /* 攻击循环里调用：只有几十微秒 */
  ├─ CollectTree()                    /* 一次快照收树（含 depth，防环，上限 128） */
  ├─ 对每个成员 KillFast()            /* 挂起+终止：不看等待、不碰文件、绝不丢目标 */
  └─ QueueCleanup() × N               /* 只入队，立刻返回 */
       └─ 单线程 CleanupThreadProc
            ├─ KillProcessTreeSync：等退出(800ms) → 没退补刀(1500ms)
            ├─ 逐文件"先锁死（删除待决）再删除"
            ├─ 根目录整目录清空（新线程，隔一会儿再清一遍）
            └─ 收尾再收一次树，补杀这一轮新冒出来的子进程
```

* 快杀**同步**做 → 每轮扫描发现谁就杀谁，**任何目标都不会因为"队列满"被丢掉**；
* 清理**后台单线程队列**做 → 不占攻击循环、不会出现 8 槽上限；
* 队列 64 格、按 PID 去重；队列满只影响"清理"，**不影响击杀**。

**③ 攻击频率：全部 <80ms**

| 位置 | 旧值 | 新值 |
|---|---|---|
| `atkcore` 攻击主循环 `dwIdle`（注入宿主内） | 30ms | **60ms** |
| `atkcore` 攻击主循环 `dwIdle`（**xx.exe 宿主内**） | **300ms** | **60ms** |
| `xx.exe` 窗口扫描 `AttackThread` | 300ms | **60ms** |
| `defense` 文本扫描 `TEXT_SCAN_MS` | 200ms | **70ms** |
| `defense` 巡检 `Sleep(1000)` | 1000ms | **70ms** |
| `defense` 心跳轮询 `Sleep(500)` | 500ms | **70ms** |
| `guard` 宿主巡检 `Sleep(200)` | 200ms | **60ms** |
| `atkcore` 清理前等待 `Sleep(150)` / 重建后再清 `Sleep(1200)` | 150/1200ms | **60 / 70ms** |

> 唯一保留 >80ms 的是 Phase A2 的 **删对手 WMI 订阅节流（5 秒一次）** —— 它是**节流**不是扫描：
> 每 60ms 调一次 `RemoveEnemyWmi()` 会让 COM 把整个循环拖垮，反而更慢。需要的话可以调成 2 秒。

### 6.27 第十五轮：读懂 testyy 的新 yy.exe / yyguaard.dll —— 它的"疯狂启动副本"是怎么来的，以及我们怎么掐死它

**① 读出来的东西（`test\testyy\` 里的反汇编）**

`yy.exe` 只有一个 `CreateProcessW`，用在**蜜罐**上（无限循环）：
`CopyFileW(自身 → %TEMP%\lenovo_fix.exe)` → 扫进程表看有没有 `lenovo_fix.exe` → 没有就起一个，然后 `Sleep(2000)`。
—— 也就是说 lenovo_fix 是**它自己**放出来的诱饵，不是我们的东西。

真正的"副本风暴"在 `yyguard.dll`（由 yy.exe `Spread: yyguard.dll injected into …` 注入到每个 yy 进程和若干宿主）里，
它每个被注入的实例都独立跑同一个循环（反汇编还原）：

```c
for (;;) {
    cnt = 数进程表里名字叫 yy.exe 的进程;
    if (cnt <= 0) goto spawn;                                  // 一个都没有 → 去补
    if (FindWindowW(L"YYWnd_7f3a9c1e", NULL)) goto spawn;      // 有主窗口 → 只补不杀
    if (GetTickCount() - last >= 1500) {                       // 没有主窗口超过 1.5 秒
        遍历进程表: 把**所有** yy.exe 全部 TerminateProcess;     // ★ 认为它们都挂了
        cnt = 重新数;
    }
spawn:
    if (cnt >= 3) goto sleep;                                  // 够 3 个就不补
    if (GetTickCount() - lastSpawn < 250) goto sleep;          // 距上次 spawn <250ms 不补
    if (<dir>\yy.exe 不存在) {
        CopyFileW(<dir>\yyres.bin → <dir>\yy.exe);             // 从备份还原
        失败再 CopyFileW(%TEMP%\yy_backup_7f3a9c1e.dat → …);
    }
    CreateProcessW(<dir>\yy.exe, NULL, cwd = <dir>);           // 起一个新副本
sleep:
    Sleep(150);
}
```

**② 逻辑上的三个硬伤（这就是"疯狂启动副本"）**

1. **没有单例/互斥**：`yyguard.dll` 被注入到 N 个进程里就有 N 份循环，各自独立决定"该不该补"，
   互相不知道对方刚起了一个 → 实际数量会冲过 3。
2. **"没有主窗口 = 全挂"的判据太脆**：我们一杀窗口宿主（或窗口被隐藏/销毁），
   每份 guard 都会走"杀光所有 yy.exe"的分支；杀完计数瞬间变 0，于是**每份 guard 都立刻补一个**。
3. **杀 → 补 → 杀 自激**：新起的 yy.exe 又带着 guard，guard 又把别人杀光再补 → 250ms/150ms 的空转，
   对外表现就是**进程表里 yy.exe 疯狂地出现/消失**，CPU 也被吃掉一大块。

**③ 我们的对策：掐"备料"，不跟它拼手速**

它要还原/启动，必须能读这三个文件：`<dir>\yyres.bin`、`<dir>\yy.exe`、`%TEMP%\yy_backup_7f3a9c1e.dat`，
注入还要读 `<dir>\yyguard.dll`。所以 atkcore 新增 `KillYyRespawnSources()`：

* 对 `yyres.bin` / `yy_backup_7f3a9c1e.dat` / `yyguard.dll` 执行 **`NeutralizeFile()`**：
  `CreateFileW(GENERIC_READ|GENERIC_WRITE, 共享=0)` → **截断成 0 字节** → **句柄不关**（一直持有）。
  这样它 `CopyFileW` 会共享冲突失败；就算绕过，还原出来的 `yy.exe` 也是 0 字节 → `CreateProcessW` 必失败。
  比"删一次"强：yy.exe 自己会把镜像再备份成 `yyres.bin`（反汇编里有），删一次它下一轮就补回来了。
* `<dir>\yy.exe` 本身仍走 `LockThenDeleteFile`（删名 + 删除待决）。
* 一旦 `yyguard.dll` 被锁住，它的 `Spread`（注入）也一起失效。

**④ 顺带确认的判据**

* yy 主窗口类名就是 **`YYWnd_7f3a9c1e`**，我方窗口扫描用 `wcsstr(cls, L"YYWnd")` 能匹配到；
* `lenovo_fix.exe` 是 yy 的蜜罐（`%TEMP%` 下），已进我方黑名单，见到就快杀整棵树（并 `BlockHoneypotRespawn` 锁住它的名字）。

### 6.28 第十六轮：两条攻击线程、deamon 注入 atkcore、Global 互斥体、结束后删除重试

**① xx.exe 现在有两条并行的攻击线程（用户要求分开）**

| 线程 | 找 target 的方式 | 频率 |
|---|---|---|
| `ProcScanThread`（新） | 进程表按**名字**匹配 `yy.exe` / `ww.exe` / `lenovo_fix.exe` | 40ms |
| `AttackThread` | `GetTopWindow` 枚举**窗口**（属主名 或 类名 `YYWnd*` / 标题 `YY_Defensive_GUI`） | 40ms |

两条线都走同一个出口 `atkcore!XxKillTree(pid)` → 快杀 + 后台清理；
`ProcScanThread` 自带懒加载（`EnsureKillTreeFn`），不依赖 `AttackThread` 先跑。
顺带**删掉了 TLS 回调**（`EarlyPayload`/`EarlyStrikeThread`）：它在 loader lock 下执行、
还会 PostMessage 最小化/关闭窗口，既是"花招"又是"线程卡死"的来源。

**② svcguard（deamon）现在会注入 atkcore —— 回答"xx 被杀后还有没有攻击"**

以前只有 `xx.exe` 加载 `atkcore.dll`，xx 一死就没有任何攻击者，yy 自然"杀掉 xx 就安全了"。
现在 `deamon.c` 的扩散/引导注入把 **atkcore 也注入宿主**（`InjectPathOf(3)`，并做 `%TEMP%` 改名副本）：

```
已注入 atkcore.dll -> PID=1234 explorer.exe
```

宿主里的 atkcore 由 `DllMain` 起 `AttackLoop`，所以 **xx 死了攻击仍在继续**。
注入产物同时带 `XxSetNames` 推送会话 token（原有机制）。

**③ 弹窗多进程冗余失效 → 改 Global\ 互斥体**

根因：`Local\` 命名空间**按登录会话隔离**，而 window.dll 会被注入到不同完整性/不同桌面的宿主里，
于是"谁都能建窗、谁都不让位"，冗余等于没有。现在：

* `XX_PURPOSE_WND_GUARD` / `XX_PURPOSE_WND_MAKE` 全部用 **`Global\xx_wg_<token>`**；
* 新增 `XXCreateGlobalMutex()`：用 SDDL `D:(A;;GA;;;WD)` 建（**放开给 Everyone**，
  否则中完整性的宿主打不开高完整性实例建的锁）；建不出来才退 `Local\` 同名；
* 同一台机器所有会话共享同一把锁，不需要注销，也不存在 session 0 抢锁（session 0 里我们本来没有交互窗口）。

**④ "结束后立即删除"：300ms × 10 次重试**

进程刚死时系统不一定立刻释放镜像文件的句柄，所以删一次经常失败。新增：

```c
static BOOL DeleteFileWithRetry(const WCHAR *path, const WCHAR *tag);  // 10 次 × 300ms
static void DeleteInThread(const WCHAR *path);                        // 线程版，不阻塞
```

* **复活器**在本体死后：先 `DeleteFileWithRetry(exePath)`（10 次 × 300ms），
  删掉了就用**自己的镜像**把 `xx.exe` 还原回去，然后再拉起（带 token）；
* **布置复活器时**：顺手用 `DeleteInThread` 清掉 `%TEMP%` 里其它旧副本（`svchost/explorer/conhost`）。
  —— 这些"内存映像残留"正是系统不会主动清的部分。

日志会写成 `结束后删除 xx.exe: 删除成功（第 3 次）` / `删除失败 err=32（第 7/10 次），300ms 后重试`。

### 6.29 第十七轮：新增 `deleter.dll`（全盘清道夫，被 svcguard 注入到尽可能多的宿主）

**源码：`deleter\deleter.c` → `bin\deleter.dll`（19,968 字节）**

**做什么**

| 步骤 | 细节 |
|---|---|
| ① 全盘扫描 | 只扫 `DRIVE_FIXED`；`FindFirstFileW` 递归；跳过重解析点（不跟符号链接/挂载点）、跳过 `WinSxS`/`servicing`/`assembly`/`Installer`/`SoftwareDistribution`/`$Recycle.Bin`/`System Volume Information`；深度上限 32 |
| ② 命中即处理 | `yy.exe` / `lenovo_fix.exe`：先 `CreateToolhelp32Snapshot` + `QueryFullProcessImageNameW` 找到正在跑的那份 → `TerminateProcess` → 删文件；`yyguard.dll` / `yyres.bin` / `yy_backup_7f3a9c1e.dat`：直接删（后两个是 yy 的还原源，删了它的 guard 就补不出可执行镜像） |
| ③ 被占用 → 关句柄 | `DeleteFileW` 失败时枚举系统句柄（`NtQuerySystemInformation(SystemExtendedHandleInformation=64)`，x64 条目 40 字节），对每个句柄 `OpenProcess(PROCESS_DUP_HANDLE)` → `DuplicateHandle` 进本进程 → `NtQueryObject(ObjectNameInformation)` 取名字 → `\Device\HarddiskVolumeN\...` 用 `QueryDosDeviceW` 映射成盘符路径 → 名字匹配目标就 `DuplicateHandle(..., DUPLICATE_CLOSE_SOURCE)` **关掉对方进程里的那个句柄** → 再删 |
| ④ 兜底 | 三轮删不掉 → `MoveFileExW(MOVEFILE_DELAY_UNTIL_REBOOT)` 安排重启后删除，日志留痕 |

**不占满 CPU（这是硬要求，已按此实现）**

* 扫描线程 `THREAD_PRIORITY_LOWEST`；
* 每 64 个目录 `Sleep(1)`、每 256 个目录 `Sleep(5)`；
* 全机**只允许一个实例做全盘扫描**：`Global\xx_del_<token>` 互斥体，拿不到锁就睡 2 秒（其它实例只维持存活，不叠 I/O）；
* 一轮扫完 `Sleep(3000)`；
* 句柄枚举**只在"某个文件删不掉"时**才对该文件执行一次（不是每轮全表扫），代价可控。

**关于 NTFS 快速枚举（Everything 那种）**

当前用的是 `FindFirstFileW` 递归 —— 简单、能跨 FAT/exFAT、不会踩 NTFS 版本差异，配合上面的节流实际 CPU 占用很低。
Everything 的 MFT/USN 直读要解析 `$MFT` 记录与 `$UsnJrnl`（还要处理 NTFS 3.1/压缩/稀疏/重解析），保守估计 1500+ 行且容易在异常卷上崩；
如果你后面要做，这一块是**可替换单元**：只需要把 `ScanDirRecursive()` 换成"从 MFT 枚举出候选路径"，`ForceDeleteFile()`/`CloseHandlesOfFile()` 一行都不用改。

**注入链**

`deamon.c` 里新增 `InjectPathOf(4)` → `deleter.dll`（`RefreshInjectCopies` 生成 `%TEMP%` 改名副本），
引导注入（explorer）和常规扩散都会注入，探针用 `XXModulePresent(pid, L"deleter", token)` 去重：

```
已注入 deleter.dll -> PID=1234 explorer.exe
```

日志（`%TEMP%\xx_debug.log`）里能看到：

```
[deleter][pid=1234] 先扫 %TEMP%: C:\Users\…\Temp\
[deleter][pid=1234] 发现目标: C:\…\yy.exe
[deleter][pid=1234] 已结束进程 yy.exe PID=4321（C:\…\yy.exe）
[deleter][pid=1234] 关闭了 2 个占用句柄: C:\…\yy.exe
[deleter][pid=1234] 已删除: C:\…\yy.exe
[deleter][pid=1234] 本轮扫描结束: 目录 18234 个，累计删除 7 个文件 / 结束 3 个进程
```

### 6.30 第十八轮：`deleter.dll` 更名为 **`dedery.dll`**（svcguard 同步）

按用户要求，产物名统一改成 **`dedery.dll`**（源码文件名仍是 `deleter\deleter.c`，不影响）：

| 位置 | 旧 | 新 |
|---|---|---|
| 编译产物 | `bin\deleter.dll` | **`bin\dedery.dll`** |
| `compile.bat` 第 9 步 | `-o "%OUTDIR%\deleter.dll"` | `-o "%OUTDIR%\dedery.dll"` |
| `deamon.c` 注入路径 | `L"%sdeleter.dll"` | `L"%sdedery.dll"` |
| `deamon.c` 去重探针 | `XXModulePresent(pid, L"deleter", …)` | `XXModulePresent(pid, L"dedery", …)` |
| `deamon.c` 日志 | `已注入 deleter.dll` | `已注入 dedery.dll` |
| `deleter.c` 单例 tag | `XXModuleSingleton(L"deleter", …)` | `XXModuleSingleton(L"dedery", …)` |
| `deleter.c` 日志前缀 | `[deleter][pid=…]` | `[dedery][pid=…]` |
| `deleter.c` 版本标记 | 仍是 `"v7"`（`XxBuildTag`） | 不变 |

注意：**单例 tag 必须和探针 tag 一致**（`dedery` ↔ `dedery`），否则 svcguard 会以为宿主里没有实例、反复注入，而 DLL 内部又因为单例已存在而拒绝 —— 表现就是"注入一直失败/没有任何日志"。这一版两边已对齐。

### 6.31 第十九轮：目录全量装甲 + WMI 订阅名带会话 token

**① 威胁 4/5（删 xx.exe / xxres.bin / 我方案 dll）→ 整目录装甲**

`defense.c` 原来只护 5 个文件（xx.exe / xxres.bin / svcguard / window / atkcore），
而 yy 的删除名单是**硬编码**的 9 个名字（`svcguard/window/atkcore/payload/defense/
DeamonWindows/deamon` + `xx.exe`/`xxres.bin`）—— 猜不全就漏。现在改成：

```c
FindFirstFileW("<ownDir>\\*") → 对**每一个文件** ArmorFile(path, nLinks, TRUE)
   · 只读属性（DeleteFileW 直接 ACCESS_DENIED）
   · deny-DELETE 的 DACL（就算清掉只读属性，DELETE 访问仍被拒）
   · %TEMP% 下 1 条随机名硬链接（xx.exe 给 2 条）
```

日志：`C1 已把程序目录里 N 个文件全部置只读 + deny-DELETE`。
新增模块（比如 `dedery.dll`）以后不用再改名单，自动被覆盖。

配套改动：我们自己"用备份还原 xx.exe"的路径（`defense.c EnsureXxRunning`）在读之前
`SetFileAttributesW(NORMAL)`、还原后再把只读加回去 —— deny-DELETE 只挡"删"，不挡"写"，
所以复活链不受影响。

**② 威胁 6（yy 按固定名删我们的 WMI 订阅）→ 订阅名带 token**

yy 的字符串里写着 `__EventFilter.Name="XX_RespawnFilter"` / `CommandLineEventConsumer.Name=
"XX_RespawnConsumer"`，它是**按固定名**搜我们的订阅然后删。现在名字变成：

```
XX_RespawnFilter_<16 位十六进制 token>
XX_RespawnConsumer_<16 位十六进制 token>
```

* token 来自 `NameTok()`（会话随机 64 位），**不落日志**（日志只写"订阅名已带会话 token"）；
* 复活链不受影响：复活器用 `--token <同一个 token>` 拉起新实例，新实例算出的名字一模一样，
  所以它仍然能认出/重建同一条订阅；
* 全新会话（没有继承 token）会生成新名字，旧订阅留在原地 —— 那只是无害残留，yy 也认不出来。

### 6.32 第二十轮：复活器**不再删旧映像**，直接拉起现成的 xx.exe

上一轮的目录全量装甲把 `xx.exe` 也护住了（只读 + deny-DELETE），而复活器里还有一段
"结束后立刻删除旧映像（300ms × 10 次重试）"—— 装甲会把它挡下来，结果只是多打十几行
失败日志、把复活推迟 3 秒。按用户要求改成：

```c
/* 不删旧映像，直接用它 */
if (GetFileAttributesW(exePath) == INVALID_FILE_ATTRIBUTES) {   // 只有真的不在了才补
    用复活器自己的副本还原 exePath
}
CreateProcessW(exePath, "… --token <hex>")                       // 直接拉起现成的
```

* 正常情况：目录里那份 `xx.exe` 还在 → **零删除、零拷贝**，直接带 token 拉起，复活更快；
* 极端情况（对手真把文件删了 / 我们清过）：`GetFileAttributesW == INVALID` → 用复活器的
  自身副本补一份再拉起（这条兜底保留）；
* `%TEMP%` 里**其它**旧副本的清理（`DeleteInThread`）保留 —— 那是清重复份，不碰主镜像。

### 6.33 第二十一轮：session 0 只复活不建窗 + 掐掉"无限复活"

用户反馈："现在 xx 无限复活了，但 session 0 可能弹弹窗"。根因有三条，一起治：

**① `window.dll` 在 session 0：不建窗、不抢互斥体，只复活 xx.exe**

session 0 是**服务会话**、没有交互桌面：在那里建出来的窗口在屏幕上根本看不见，更糟的是它会
去抢"弹窗互斥体"—— 一旦 session 0 的实例先抢到，交互会话里的实例就全变热备，**真正的弹窗反而没了**。
现在 `WindowMainThread` 一开始就判会话：

```c
if (ProcessIdToSessionId(GetCurrentProcessId(), &sid) && sid == 0) {
    WriteWinLog(L"session 0：只复活 xx.exe（不建窗、不抢互斥体）");
    for (;;) {
        if (g_bExitThread) return 0;
        if (!g_pSharedView) OpenSharedMemory();
        if (!IsXxRunning()) EnsureXxRunning();   /* 自带 3 秒节流 */
        Sleep(1000);
    }
}
```

注意：session 0 里**不调** `BumpHeartbeatTick()` —— 心跳代表"有弹窗活着"，在没窗口的会话里
递增它会让监控方误判"弹窗还在"，从而不去复活。

**② `EnsureXxRunning` 加 3 秒节流**

多宿主里的 `window.dll` 都会调它；不节流时一旦 xx 起不来，每个宿主每秒拉一次
→ 进程表里 `xx.exe` 疯狂出现，就是"无限复活"的直接来源。

**③ 复活器去重 + 崩溃循环阻尼**

* `SpawnReviver()` 先扫进程表：已有 `svchost/explorer/conhost` 任一在跑就**不再布置第二份**
  （否则每轮复活都多留一个监视者，几轮下来一堆复活器各自拉 xx）；
* `RunReviver()` 里量一下本体存活时间：**不足 3 秒**（= 起来就崩）就先 `Sleep(5000)` 再拉起，
  把"每秒一次的无复活"压成"每 8 秒一次"，并打日志
  `复活器：本体存活不足 3 秒（疑似崩溃循环），等 5 秒再拉起`。

日志里能直接确认：

```
[window] session 0：只复活 xx.exe（不建窗、不抢互斥体）
已布置复活器: … (pid=…)
复活器：本体存活不足 3 秒（疑似崩溃循环），等 5 秒再拉起
已有复活器在运行，本次不再布置第二份
```

### 6.34 更正：撤销"复活去重/崩溃阻尼/复活节流"，**只保留 session 0 不抢锁**

我把用户的"无限复活"误读成了"复活失控"。用户的意思是**好话**：被杀了能复活、不会一直死。
所以上一轮加的三处"保险"全部撤销，回到原样：

| 撤销的改动 | 位置 | 现状 |
|---|---|---|
| 复活器去重（扫 svchost/explorer/conhost，已有就不再布置） | `xx.c SpawnReviver` | **已撤销** |
| 崩溃循环阻尼（本体存活 <3s 就等 5s 再拉） | `xx.c RunReviver` | **已撤销** |
| 复活节流（3 秒内最多一次） | `window.c EnsureXxRunning` | **已撤销** |

**保留的唯一改动**（这才是真问题）：`window.c` 的 session 0 分支 ——

```c
if (ProcessIdToSessionId(GetCurrentProcessId(), &sid) && sid == 0) {
    WriteWinLog(L"session 0：只复活 xx.exe（不建窗、不抢互斥体）");
    for (;;) {
        if (g_bExitThread) return 0;
        if (!g_pSharedView) OpenSharedMemory();
        if (!IsXxRunning()) EnsureXxRunning();
        Sleep(1000);
    }
}
```

session 0 是服务会话：在那里建出来的窗口看不见，**更糟的是它会去抢交互会话的弹窗互斥体** ——
一旦它先抢到，交互会话里的实例全变热备，真正的弹窗反而消失。所以 session 0 只做"保 xx 活着"，
不建窗、不创建/不抢互斥体、也不递增心跳。

### 6.35 第二十二轮：为什么"复活后没弹窗"（`visible=0`）+ defense 到底更没更新

**A. defense 更新了，问题在你机器上的文件是旧的**

我这边的构建产物**全部**带 `XxBuildTag() == "v7"`，逐个 objdump 复核过：

```
window/svcguard/atkcore/defense/guard/payload/dedery  → 都导出 XxBuildTag，且二进制里含 "v7"
defense.c:37  __declspec(dllexport) const char * WINAPI XxBuildTag(void) { return "v7"; }
```

你日志里 `C:\bin\defense.dll 标记=无(过度古老的 dll)` 说明**那个文件里连导出都没有**，
只能是被旧的 `defense.dll` 占着没换掉（它的装甲是"只读 + deny-DELETE"，`CopyFileW` 覆盖
会静默失败）。换文件的正确姿势见 §6.25，最省事：**换一个全新目录**。

> 顺带解释日志里的 `实际=(未知)`：模块的 `DllMain` 会调用 `XXHideSelfFromPeb()` 把自己从
> PEB 三条链摘掉，之后 `GetModuleFileNameW(该模块)` 必然失败 —— 那是摘链的副作用，
> 不是加载失败。已改成取不到路径就打印请求路径，日志更好读。

**B. `visible=0`：复活出来的实例第一次 `ShowWindow` 被 STARTUPINFO 吃掉**

Windows 的规则：**进程的第一次 `ShowWindow()` 会被启动时 `STARTUPINFO.wShowWindow` 覆盖**。
我们拉起 revive 实例时用的是：

```c
si.dwFlags = STARTF_USESHOWWINDOW;
si.wShowWindow = SW_HIDE;        // ← 罪魁
```

于是新实例里 `ShowWindow(g_hWnd, SW_SHOW)` 被忽略 → 日志 `window created … visible=0`
（用户看到的"复活后没弹窗"）。首实例由你双击启动、没有 STARTUPINFO，所以 `visible=1`。

修法（两处都改了）：

```c
/* window.c ServeWindow：连做两次，第二次一定按参数生效 */
ShowWindow(g_hWnd, SW_SHOWNORMAL);
ShowWindow(g_hWnd, SW_SHOW);
SetWindowPos(g_hWnd, HWND_TOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW);
UpdateWindow(g_hWnd);
```

```c
/* 拉起 xx 的两处（window.c 的 EnsureXxRunning、xx.c 的复活器）改成 SW_SHOWNORMAL */
si.wShowWindow = SW_SHOWNORMAL;   /* SW_HIDE 会让新实例的第一次 ShowWindow 失效 */
```

**C. 顺手清掉重复 banner**

日志里 `build=v11` 后面又跟了一条 `build=v9`（历史遗留的第二条 banner），已删除。

> 另外：`SeDebugPrivilege enable failed: 1300` 是"令牌里没有该特权"，那台机器必须补
> `secpol.msc → 本地策略 → 用户权利分配 → 调试程序 → Administrators`，否则注入/杀系统进程会大面积失败。

### 6.36 第二十三轮：真相 —— 不是旧 dll，是**我们自己的摘链把 GetProcAddress 打死了**（Win7 专属）

用户说得对：虚拟机装了 Deep Freeze，`C:\bin` 每次重建，文件**一定**是新的。
所以那些 `标记=无(过度古老的 dll)` / `导出不存在` / `实际=(未知)` 只有一个解释：

**`XXHideSelfFromPeb()` 把模块从 PEB 三条链表摘掉之后，Win7 的
`GetProcAddress(hMod, ...)` 和 `GetModuleFileNameW(hMod)` 都会失败**
（它们内部要按句柄/链表在加载器表里找回模块；Win10/11 上还能容忍这种摘链，Win7 不行）。

这解释了历史上所有诡异现象：

| 现象 | 真正原因 |
|---|---|
| `window.dll loaded` 但 `DeamonInit / StartDefense / StartAttack / XxKillTree 导出不存在` | 模块**已经加载并跑起来了**（所以窗口/线程都正常），只是 `GetProcAddress` 找不到导出 |
| `!! 版本不匹配 … 标记=无(过度古老的 dll)` | 同上，`GetProcAddress(h,"XxBuildTag")` 返回 NULL，被当成"旧 dll 没有导出" |
| `实际=(未知)` | `GetModuleFileNameW(h)` 同样失败 |
| 之前"deamon 没注入成功/没攻击" | `DeamonInit` 从未被真正调用过 —— 也是这个原因 |

**修法：不再问加载器，自己走 PE 导出表。**

新增 `xxipc.h: XXGetProcByName(HMODULE, const char*)`：读模块内存里的
`IMAGE_DOS_HEADER → IMAGE_NT_HEADERS → DataDirectory[EXPORT] → AddressOfNames/NameOrdinals/AddressOfFunctions`，
命中名字就返回 `基址 + RVA`。摘链不影响（头与导出目录都还在）。

`xx.c` 里所有**取我们自己模块导出**的地方都换成了它：

```c
pTag   = XXGetProcByName(h, "XxBuildTag");      // 版本校验
pSet   = XXGetProcByName(hMod, "XxSetNames");   // 推 token
pInit  = XXGetProcByName(hDeamon, "DeamonInit");
pDef   = XXGetProcByName(hDef, "StartDefense");
pStart = XXGetProcByName(hKill, "StartAttack");
g_pfnKillTree = XXGetProcByName(hKill, "XxKillTree");
```

（`defense.c` 里对 `LdrLoadDll` / `TerminateProcess` 的 `GetProcAddress` 是对**系统模块**用的，
系统模块没有摘链，保持原样。）

修完之后 Win7 上应该看到：

```
window.dll loaded: C:\bin\window.dll
svcguard.dll loaded: C:\bin\svcguard.dll
DeamonInit called                     ← 以前永远看不到
defense.dll loaded: C:\bin\defense.dll
defense.dll StartDefense started: …   ← 以前永远看不到
atkcore.dll 路径: C:\bin\atkcore.dll
atkcore.dll StartAttack called: …     ← 以前永远看不到
```

顺带：用户日志里 `window created … visible=0` 是上一轮修的那个 STARTUPINFO 覆盖问题
（`ShowWindow` 连做两次 + 拉起方改 `SW_SHOWNORMAL`），这版已包含。

### 6.37 第二十四轮：注入崩溃三处止血 + 注入日志节流 + 弹窗放大

**① 注入崩溃：三个最可疑的点都处理了**

| # | 处理 | 理由 |
|---|---|---|
| 1 | **默认关闭** `RepairTargetLdrHook()`（`deamon.c`，函数体第一行 `if (1) return;`） | 它往目标进程的 `ntdll!LdrLoadDll` 代码页回写"原始字节"。不同 Win7 补丁级别的 `LdrLoadDll` 前导指令长度不一样，按固定 `LDR_PATCH_LEN` 回写很容易把函数劈坏 → 目标一调 `LoadLibrary` 就崩。这是"注入哪个进程哪个就崩"的头号嫌疑 |
| 2 | **所有 DllMain 里起的线程先 `Sleep(80)`** | `DllMain` 里 `CreateThread` 后线程立刻干活会踩 **loader lock**（线程在 DllMain 返回前跑，又去 LoadLibrary/COM/快照）→ 死锁/崩溃。现在线程入口先让 80ms，DllMain 肯定已返回 |
| 3 | **explorer 只注入 svcguard + window** | explorer 是我们的**弹窗宿主**，一崩弹窗就没了。`atkcore`（攻击循环）和 `dedery`（全盘扫描）这些重活只注给其它宿主 |

**② 注入日志：5 秒一条，共享日志里只留最近 3 条 `[INJ]`**

`deamon.c` 新增 `WriteInjectLog()` + `PruneInjLines()`：

* 全局节流 `INJ_LOG_MIN_GAP_MS = 5000`（不管注入几个进程，5 秒最多一条）；
* 每条带 `[INJ]` 前缀，写之前把共享日志里**更早的 `[INJ]` 行删掉**，只保留最近 3 条；
* 其它日志（攻击/复活/防御）一行不动。

弹窗里的输出变成：

```
[12:00:01.123] [INJ] svcguard -> PID=1234 explorer.exe
[12:00:06.130] [INJ] window -> PID=1234 explorer.exe
[12:00:11.140] [INJ] atkcore -> PID=5678 notepad.exe
```

**③ 弹窗放大到 2 倍面积**

`window.c`：`WND_WIDTH 450 → 636`、`WND_HEIGHT 325 → 460`（面积 ≈ 2.0 倍）。
日志 EDIT 本来就铺满客户区（`WM_SIZE` 里 `MoveWindow(0,0,客户区)`），所以日志可视行数
从约 13 行涨到约 26 行。

### 6.38 第二十四轮补充：把注入策略写清楚（window.dll **仍然注入所有宿主**）

先澄清上一轮的误解：我改的是**重模块**（`atkcore`/`dedery`）不进弹窗宿主，
**`window.dll` 从来没有被限制**——它依旧注入所有非系统宿主，多进程冗余就是靠它铺开的。

现在策略在 `deamon.c` 里被显式写成函数 + 日志，不再靠注释猜：

```c
static BOOL IsPopupHostProcess(LPCWSTR leaf);   /* explorer.exe / taskhost(w).exe */

轻模块（**所有宿主都注，含 explorer**）：
    svcguard → 复活/扩散
    window   → 弹窗（冗余的来源，越广越好）
    guard    → 宿主看护
重模块（**不注给弹窗宿主**）：
    atkcore  → 40ms 攻击循环
    dedery   → 全盘扫描
```

启动时会打一行策略说明：

```
注入策略: svcguard/window/guard -> 所有宿主（含 explorer）；atkcore/dedery -> 除弹窗宿主外
```

**修掉一处真实的不一致**：`BootstrapInject(explorer)` 之前会顺手把
`atkcore` + `dedery` 也注进 explorer（那样扩散循环里的跳过等于白做），现在引导注入也遵守同一策略，
并且补上了原本漏掉的 `guard`。

验证方式（`%TEMP%\xx_debug.log`）：

```
[INJ] svcguard -> PID=… explorer.exe      ← 轻模块进 explorer
[INJ] window   -> PID=… explorer.exe      ← 弹窗宿主
[INJ] window   -> PID=… notepad.exe       ← 其它宿主也有 window（冗余）
[INJ] atkcore  -> PID=… notepad.exe       ← 重模块只给普通进程
```

只要看到 `window -> … explorer.exe` 之外还有别的进程，就说明冗余在铺开；
如果只看到 explorer 一个宿主，那是"其它宿主打不开句柄"（权限/位数），日志里会有 `跳过 32 位进程` 或注入失败计数。

### 6.39 澄清 + 补强：`atkcore` 会不会因为"window 注满所有进程"而注不进去？

**不会。** 跳过规则是**按进程**判断的，不是"注了 window 就整台机器不注 atkcore"。具体三档：

| 进程 | window | svcguard | guard | atkcore | dedery |
|---|---|---|---|---|---|
| `explorer.exe` / `taskhost(w).exe`（弹窗宿主名字） | ✅ | ✅ | ✅ | ❌ | ❌ |
| **当前真正持有弹窗的进程**（共享内存持有者表里本会话那格） | ✅ | ✅ | ✅ | ❌ | ❌ |
| 其它宿主（notepad 等，任意个） | ✅ | ✅ | ✅ | ✅ | ✅ |

所以 `atkcore` 依旧会注进**绝大多数**宿主；只有"弹窗宿主"这一两个不拿重活。

**这轮补的正是你问的那个点**：原来只按名字挡 `explorer/taskhost`，
但 `window.dll` 注入所有宿主之后，**任何宿主都可能抢到弹窗互斥体并真的显示窗口**
（比如 notepad 抢到了锁）—— 那个进程才是"当前弹窗宿主"，同样不能塞 `atkcore`。
所以新增：

```c
static BOOL IsPopupOwnerPid(DWORD pid);   /* 读共享内存持有者表：本会话 + 该 PID */
...
BOOL bIsExplorer = IsPopupHostProcess(pe.szExeFile) || IsPopupOwnerPid(pe.th32ProcessID);
```

日志里策略行也更新成：

```
注入策略: svcguard/window/guard -> 所有宿主（含 explorer）；
          atkcore/dedery -> 除[explorer/taskhost 与**当前弹窗持有者**]外
```

**副作用（好的）**：弹窗持有者从 explorer 换成任何宿主时，那个宿主也自动"减负"，
不会再因为 atkcore 的攻击循环/全盘扫描把它拖崩、把弹窗带走。

### 6.40 第二十五轮：explorer 反复崩 + 弹窗日志区全黑

截图现象：`XX PROGRAM - ALIVE` 弹窗在，但日志区**一片黑**；同时 explorer 两次
"已停止工作"，WerFault 报 `写` 访问违例。两件事一起查。

**① explorer 崩：导出修好之后，一批"以前从没运行过"的代码第一次跑起来了**

`XXGetProcByName` 修好之前，`DeamonInit` / `StartDefense` / `StartAttack` 从来没被调用过；
修好之后它们在**每个被注入的宿主**里开始执行，于是宿主里第一次出现这些重活：

| 触发点 | 干了什么 | 处理 |
|---|---|---|
| `DllMain → BootThread → StartRemoteAttack()` | 在宿主里 `LoadLibrary(atkcore.dll)` —— **绕过**了"重模块不进弹窗宿主"的策略 | 弹窗宿主里**不启动** |
| `DllMain → UnloadForeignGuardThread` | 对宿主里的模块做 `FreeLibrary`；如果那个模块的线程正在跑 → 直接写非法地址崩掉 | 弹窗宿主里**不启动** |
| `DeamonInit → SpreaderThread` | 从宿主再往别的进程注入 | 弹窗宿主里**不启动** |
| `DeamonInit → OwnFileShieldThread` | 反复锁我们的文件 | 弹窗宿主里**不启动** |

弹窗宿主（`explorer.exe` / `taskhost(w).exe`）里现在**只起 `HeartbeatMonitor`**，
日志会写：

```
DllMain: 弹窗宿主（explorer.exe）不启动 Boot/Unload 线程
DeamonInit: 弹窗宿主（explorer.exe）只起心跳监控，跳过扩散/文件盾
```

（新增 `OwnHostLeaf()`：用 `GetModuleFileNameW(NULL)` 取宿主映像名 —— 取进程主映像，
不受我们摘 PEB 模块链影响。）

**② 日志区全黑：宿主里的 `window.dll` 没映射到共享内存**

弹窗是宿主里的 `window.dll` 建的，它的 `OpenSharedMemory()` 在注入那一刻可能失败
（映射还没建好），之后 `ReadLog()` 永远返回空 → EDIT 一片黑。
现在 `WM_TIMER` 里发现 `g_pSharedView == NULL` 就**再映射一次**：

```c
if (!g_pSharedView) OpenSharedMemory();
sig = LogSignature();
...
```

映射上之后日志立刻正常刷新（`LogSignature` 变了就会 `UpdateLogText`）。

如果下个版本日志区还是黑的，那就是宿主里 `OpenSharedMemory` **持续失败**
（映射 DACL/会话问题），我会在弹窗里退化显示一行 `sharedmem=MISSING` 提示，而不是纯黑。

### 6.41 第二十六轮：桌面多个窗口 + 崩溃日志进窗口

**① 桌面出现多个 XX 窗口 —— 锁名不一致导致的"人人是持锁者"**

互斥体名字原来用 `OwnNameToken()`：**优先 secret token**，而那个 token 是 xx 通过
`XxSetNames` 逐个推进宿主的。只要某个宿主没收到（时序/导出/注入失败），它就退回共享 salt
→ **同一台机器上出现两把不同名字的锁** → 每个实例都抢到"自己的锁" → 各建一个窗口。
（日志里 `kind=salt` 和 `kind=secret` 同时出现，就是这件事。）

改成 `GuardLockToken()`：**只用共享内存里的 token64/salt**，拿不到映射时用会话号兜底。
同一会话里所有 `window.dll` 读到的都是同一个值 → 一定抢同一把锁 → 只有一个窗口。

**② 崩溃日志进窗口（用户要求：拿不到日志，就让它显示在弹窗里）**

每个模块的 `DllMain` 都装了 `SetUnhandledExceptionFilter`：

| 模块 | 日志去处 | 内容 |
|---|---|---|
| `window.dll` | `WriteWinLog` → 共享日志 + 文件 | `[CRASH] window.dll pid=… in-host code=0x… addr=0x…` |
| `svcguard.dll` | `WriteDeamonLog` → 共享日志 + 文件 | `[CRASH] svcguard.dll pid=… code=… addr=…` |
| `atkcore.dll` | `WriteKillLog` → 共享日志 + 文件 | `[CRASH] atkcore.dll …` |
| `dedery.dll` | `DelLog` → 文件 | `[CRASH] dedery.dll …` |
| `guard.dll` | `XXFileLog` → 文件 | `[CRASH] guard.dll …` |

崩溃发生时，只要同一会话里还有**任意一份** window.dll 活着，弹窗的日志区就会显示
`[CRASH] <模块> pid=… code=0x… addr=0x…` —— 不用再关 Deep Freeze、不用进 `%TEMP%`。

**③ 这次一定能拿到"是谁崩的"**

看弹窗最后几行：

```
[CRASH] window.dll pid=1234 in-host code=0xC0000005 addr=0x000007FExxxxxxxx
```

- `code=0xC0000005` = 访问违例；`addr` 落在哪个模块的范围，就是哪个模块的代码。
- 如果 `[CRASH]` 一条都没出现，说明崩的是**别的进程/别的模块**（比如 explorer 自己），
  那就看 `%TEMP%\xx_debug.log` 里 `[CRASH]` 前后各 5 行，能对上注入时间点。

### 6.42 第二十七轮：**默认关闭宿主注入**（explorer 崩、注销后 RPC 坏的真凶）

你给的老版本日志信息量很大，关键三条：

```
[deamon][pid=1572] DllMain: 弹窗宿主（Explorer.EXE）不启动 Boot/Unload 线程
[deamon][pid=1572] [CRASH] svcguard.dll pid=1572 code=0xC0000005 addr=00000000E9BC54C0
[deamon][pid=4128] [INJ] svcguard -> PID=4852 LogonUI.exe      ← 注进了登录界面进程！
[deamon][pid=4128] [INJ] svcguard -> PID=4900 taskmgr.exe
[deamon][pid=4128] [INJ] svcguard -> PID=4448 wetype_server.exe
```

**① 每注入一个进程，那个进程里 svcguard 就崩一次**

崩溃地址的低 16 位固定（`…54C0`、guard 是 `…2800`），说明是**同一个代码位置**的系统性崩溃，
不是随机踩内存。也就是说：`svcguard.dll` / `guard.dll` 被 LoadLibrary 进宿主后，
在某段固定代码上必然触发 `0xC0000005`。在把这个位置彻底定位之前，注入就是"见谁崩谁"。

**② 更严重：注进了 `LogonUI.exe` / `taskmgr.exe` / `wetype_server.exe`**

- `LogonUI.exe` 是**登录界面**（安全桌面）—— 它一崩，就会出现你遇到的
  "注销后再也进不去 / RPC 服务有问题"（登录链被打断，`RpcSs` 跟着报错）；
- 之前的 `IsProtected()` 只挡了 `csrss/winlogon/lsass/...`，**没挡 `LogonUI`**。

**③ 所以这一版把注入整个关掉了**

`deamon.c` 顶部加了总开关：

```c
#define XX_HOST_INJECTION 0     /* 默认 0：不向任何宿主注入 */
```

关掉之后：

| 行为 | 状态 |
|---|---|
| `DllMain` 里的 BootThread（在宿主里 LoadLibrary atkcore）/ UnloadForeignGuardThread（对宿主 FreeLibrary） | **不启动** |
| `DeamonInit` 里的 SpreaderThread / OwnFileShieldThread | **不启动** |
| `BootstrapInject()` / 扩散循环 | **直接返回** |
| 保留 | `HeartbeatMonitor`（复活 xx）、蜜罐占位锁、`EnsureXxExeExists` |

弹窗只由 **xx.exe 自己**的 `window.dll` 显示（它有消息循环，日志里 `visible=1` 正常），
不再有"explorer 里那份 window.dll"。代价是没有多进程冗余，但**换来系统不被搞崩**。

日志会明确写：

```
DllMain: 宿主注入已关闭（XX_HOST_INJECTION=0），不起 Boot/Unload/扩散线程
宿主注入已关闭（XX_HOST_INJECTION=0）：只做心跳监控/复活，不注入任何进程
```

**④ 顺带修掉一处同类 bug**

`StartRemoteAttack()` 里取 `atkcore` 的 `XxSetNames`/`StartAttack` 还在用 `GetProcAddress` ——
atkcore 的 DllMain 已经摘过 PEB 链，所以**必然返回 NULL**（你日志里那句
`StartAttack 导出未找到` 就是它）。已改成 `XXGetProcByName`（自己解析导出表）。

**下一版要恢复注入，只需要回答一个问题：为什么注入后宿主必崩**

我需要的证据只差一条：**`[CRASH]` 那一行的 `addr` 落在哪个模块**。
现在 `code/addr` 都有了，`addr` 是 `0x…54C0`；如果你能再给一张 **Modules 列表**
（Process Explorer 里 explorer.exe 的模块列表，或者 `listdlls explorer.exe`），
我就能把 `addr` 减去模块基址算出 RVA，直接指到函数名 —— 然后修掉它，再打开 `XX_HOST_INJECTION`。

### 6.43 第二十八轮：注入开回来（带系统进程白名单）+ 崩溃日志升级成"能直接指出函数"

**① 崩溃日志升级：不用 Process Explorer、不用 dump，日志里直接给"模块+0xRVA+栈"**

以前只记 `code` 和绝对地址，跨进程/带 ASLR 时基本没法用。现在 `DeamonCrashFilter`
用 `GetModuleHandleExW(FROM_ADDRESS, …)` 反查地址所属模块，打印成 `模块+0xRVA`，
再加一次 `RtlCaptureStackBackTrace`（10 帧，每帧同样换算）：

```
[CRASH] svcguard pid=1572 host=explorer.exe code=0xC0000005 (write) at=window.dll+0x54C0
        frames: svcguard.dll+0x12AB ntdll.dll+0x3C1D kernel32.dll+0x1A2B …
        fault-addr=0x0000000000000000
```

- `(write)` / `(read)` / `(exec)` 来自 `ExceptionInformation[0]`；
- `fault-addr` 是**出错的那个内存地址**（`0x0` 就是空指针偏移写，最好查）；
- `frames` 里第一帧就是"谁在写"。

**② 注入开回来，但只注"安全目标"**

```c
#define XX_HOST_INJECTION 1        /* 注入已开 */
static BOOL IsSafeInjectTarget(DWORD pid, LPCWSTR leaf);
```

规则（保守，足够保住冗余）：

| 目标 | 是否注入 |
|---|---|
| `explorer.exe` / `taskhost(w).exe` | ✅（弹窗宿主） |
| 映像路径**不在** `%WINDIR%` 下（Program Files、用户目录、%TEMP%…） | ✅ |
| 映像路径在 `%WINDIR%` 下（System32/SysWOW64/…） | ❌ |

你上一份日志里被注入的 `LogonUI.exe`、`taskmgr.exe`、`wetype_server.exe`、`audiodg.exe`
全都在 `%WINDIR%` 下 → 现在**一个都不会被注**，注销/登录链不会再被打断；
而 `explorer.exe` 和用户程序照旧注入，多进程冗余保留。

**③ 这次跑完请给我你日志里的 `[CRASH]` 段**

现在那一行已经自带模块+RVA+栈，直接就能定位。比如如果出现：

```
[CRASH] svcguard pid=1572 host=explorer.exe code=0xC0000005 (write) at=window.dll+0x54C0
```

我拿 `window.dll` 的 RVA `0x54C0` 查一下符号表就知道是哪个函数（本地可以立刻做）。
把这两三行发我即可，不用关 Deep Freeze、不用 dump。

### 6.44 第二十九轮：**找到并修掉"注入即崩"的根因**（远程推 token 把 64 位基址截断了）

你给的新日志里有 11 条 `[CRASH]`，全部是同一个特征：

```
[CRASH] svcguard pid=1520 host=Explorer.EXE     code=0xC0000005 (exec) at=00000000EA945B50
[CRASH] svcguard pid=1896 host=vmtoolsd.exe     code=0xC0000005 (exec) at=00000000EA945B50
[CRASH] svcguard pid=2016 host=wetype_update.exe code=0xC0000005 (exec) at=00000000FC845B50
[CRASH] guard.dll pid=4312 code=0xC0000005 addr=00000000FCB02800
```

三个关键线索：

1. **低 16 位永远一样**：svcguard 全是 `…5B50`，guard 全是 `…2800`；
2. **`(exec)`** —— 不是读/写，是"跳到一个不可执行的地址"（DEP 违例）；
3. `at=` 后面**没解析出模块名**（只有裸地址）—— 说明那个地址不属于任何已加载模块。

我把本地带符号的构建拿 `objdump` 一查：

```
svcguard.dll:  RVA 0x5B50 = XxSetNames        ← 崩溃地址正好是它的入口
guard.dll   :  RVA 0x2800 = XxSetNames
```

**根因**在 `PushNamesRemote()`（`deamon.c`，注入成功后远程调用 `XxSetNames` 把会话 token 推进宿主）：

```c
HANDLE hThread = CreateRemoteThread(...LoadLibraryW...);
GetExitCodeThread(hThread, &code);          /* ← code 是 DWORD（32 位） */
PushNamesRemote(hProc, (HMODULE)(ULONG_PTR)code, …);   /* 当 64 位模块基址用 */
       ...
       (BYTE *)hRemoteMod + rva             /* ← 基址被截断成 0x????0000 */
```

x64 上模块基址是 64 位（例如 `0x7FF8ABCD0000`），`GetExitCodeThread` **只给低 32 位**，
基址被截成 `0xABCD0000`；再加上 `XxSetNames` 的 RVA `0x5B50` → 跳到 `0xABCD5B50`
—— 那是一片未映射/不可执行的地址，于是 `(exec)` AV，**整个宿主进程当场死掉**。
这解释了：为什么"注入哪个进程哪个崩"、为什么低 16 位固定、为什么崩在 `XxSetNames`。

**修法**：**不再远程推 token**。

* `InjectDll` 里删掉 `PushNamesRemote(...)` 调用（函数保留，标 `__attribute__((unused))` 备查）；
* 各模块本来就会自己从共享内存读 `XX_NAME_TOKEN64` 当回退 token（`NameTok()` 的既有逻辑），
  命名依然全会话一致 —— 当初的"秘密 token 远程推送"根本不必要；
* 顺带把 `PushNamesRemote`/`GetRemoteExportRva` 整段重写干净（之前改坏过），现在 0 警告。

修完之后，**注入应该不再杀宿主**，多进程冗余可以保留：

```
[INJ] svcguard -> PID=1520 Explorer.EXE     ← 不再紧跟 [CRASH]
[INJ] window   -> PID=1520 Explorer.EXE
```

如果还有 `[CRASH]`，新日志会给出 `模块+0xRVA` + 栈回溯 + `fault-addr`，我一眼就能定位下一个。

### 6.45 第三十轮：**同一位置大量窗口 + CPU 打满**（单进程内的多份 window.dll）

你说"进程只有一个"，那问题就在**进程内部**：同一个进程里加载进了**多份 `window.dll`**，
每份都起一个窗口线程 → 每份各建一个窗口（位置相同）→ 4 个 500ms 定时器 + 4 套兜底循环 → CPU 打满。

**根因链条**

1. 我们的模块在 `DllMain` 里调用 `XXHideSelfFromPeb()` 把自己从 PEB 三条链摘掉
   → 之后 `LoadLibraryW(<同一个文件>)` **找不到已加载的那份**，会**再加载一份新副本**；
2. 而 `XXModuleSingleton()` 的设计是"按 4 个候选名逐个试"：如果第一个名字已被占用，
   它会**换下一个名字**继续建锁 —— 于是第二份副本能通过单例检查、正常初始化；
3. `WINDOW_THREAD_MAX` 原来写的是 **4**，`StartWindow()` 允许每个进程最多 4 个窗口线程；
4. 4 个线程各自抢 `xx_wg_*` 失败 → 各自走"1.5 秒没看到窗口就兜底建窗"的旁路
   （`g_nMakeGuardMiss >= 3` 就无视建窗锁）→ **同一位置叠出多个窗口**。

**这一版的修法（四处，一处都不留口子）**

| # | 改动 | 位置 |
|---|---|---|
| 1 | `WINDOW_THREAD_MAX` **4 → 1**：每进程只允许一个窗口线程 | `window.c` |
| 2 | 严格单例：用一把 `Global\xx_win1_<token>` 锁，`CreateMutex` 返回 `ERROR_ALREADY_EXISTS` 就**直接 `return FALSE` 拒绝加载**（不再换名字重试） | `window.c DllMain` |
| 3 | 删掉"连续 3 次抢不到建窗锁就无视它照建"的旁路：**抢不到就不建**，只记日志（5 秒一条） | `window.c` 兜底分支 |
| 4 | `WINDOW_FALLBACK_MS` 1500 → **5000**，建完窗**持锁 2 秒**再放，避免多实例同时越过阈值集体兜底 | `window.c` |

日志里现在能看到：

```
[window][pid=4128] 本进程已有一份 window.dll，拒绝重复加载
[window][pid=4128] 建窗串行锁被占用，本实例不建窗（等下一轮复查）
```

**顺带把 CPU 的大头也压掉了**（上一轮已做，这轮确认）：

* 宿主里**只注入 `svcguard` + `window`**，`atkcore`（40ms 攻击循环）/`dedery`（全盘扫描）/
  `guard`（句柄看护）**一律不进宿主**（每个宿主各跑一套会把 CPU 打满）；
* `SpreaderThread` **只在 xx.exe 里跑**（`if (!IsRunningInXxExe()) return 0;`），
  宿主不会再互相注入。

### 6.46 第三十一轮：按用户指定实施注入矩阵

| 目标 | 注入模块 | 说明 |
|---|---|---|
| `dwm.exe` / `explorer.exe` / `notepad.exe` / `conhost.exe` / `cmd.exe` | **svcguard + window + guard** | 桌面类宿主：弹窗冗余 + 看门 + 复活 |
| 其它（白名单内、非系统目录）进程 | **atkcore + svcguard + dedery** | 攻击 + 复活 + 全盘清道夫 |
| `xx.exe` 自己 | **defense + atkcore**（另加 window/svcguard） | 保护 + 攻击本体 |
| `xx.exe` 的注入职责 | **只负责把 svcguard 铺出去**（引导注入 explorer 时按桌面宿主集合：svcguard+window+guard），其余由各宿主里的 svcguard 完成 | 用户指定 |

实现要点（`deamon.c`）：

```c
static BOOL IsDesktopHostLeaf(LPCWSTR leaf);   /* dwm/explorer/notepad/conhost/cmd */

BOOL bDesktopHost = IsDesktopHostLeaf(pe.szExeFile);
if (bDesktopHost) { svcguard, window, guard }
else              { atkcore, svcguard, dedery }
```

* `IsSafeInjectTarget()` 把桌面宿主名单**放进白名单**（`dwm/conhost/cmd` 在 system32 下，否则会被"系统进程不注"挡掉）；
* 每个被注入的 `svcguard` 都执行同一矩阵（"剩下的注入由 svcguard 完成"），
  但宿主里的扩散**放慢到 3 秒一轮**（`Sleep(IsRunningInXxExe() ? 200 : 3000)`），xx.exe 仍 200ms；
* `atkcore` 攻击频率按宿主区分：**xx.exe 40ms，宿主副本 2000ms**
  —— 否则"矩阵把 atkcore 铺到几十个进程"= 几十倍 CPU，必然卡死；
* `BootstrapInject(explorer)` 已是 svcguard+window+guard，符合桌面宿主集合。

日志里可以按这两组核对矩阵：

```
[INJ] svcguard -> PID=… Explorer.EXE
[INJ] window   -> PID=… Explorer.EXE
[INJ] guard    -> PID=… Explorer.EXE
[INJ] atkcore  -> PID=… wechat.exe
[INJ] svcguard -> PID=… wechat.exe
[INJ] dedery   -> PID=… wechat.exe
```

### 6.47 第三十二轮：扩散频率按用户指定值定稿（攻击扫描不动）

| 项 | 值 | 位置 |
|---|---|---|
| xx.exe 扩散轮次 | **1000ms** | `deamon.c` SpreaderThread 循环尾 |
| 宿主内扩散轮次 | **5000ms** | 同上（`IsRunningInXxExe()` 三元） |
| 注入缓存刷新 `INJ_REFRESH_MS` | **20s** | `deamon.c` 顶部宏 |
| 攻击扫描频率 | **未改动**：xx.exe 40ms、宿主副本 2000ms | `killtools.c` `dwIdle` |
| 弹窗日志刷新 / 心跳 | 未改动 500ms | `window.c` |

上一轮的值（xx 2000ms / 宿主 30000ms / 缓存 60s）已按你的数字替换掉；
攻击扫描（`atkcore` 的 `dwIdle`）一行没动。

日志里可以直接核对：`扩散统计(10s)` 里 `扫描(进程×轮)` 大约会落在
`10s/1s × 1( xx) + 10s/5s × N(宿主)` 的量级，比之前 30s 一版会上升，
但仍在 N 个宿主跑满之前收敛；如果又出现卡顿，先看这个数字有没有异常放大。

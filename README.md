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
| `xx.exe` | 70,144 | 唯一的 exe 主程序 |
| `svcguard.dll` | 30,720 | 守护与扩散（含 guard.dll 宿主部署、句柄交接、我方文件锁、反反注入） |
| `window.dll` | 21,504 | 弹窗与日志（控制台式滚动输出、含远程自愈入口与决策日志） |
| `atkcore.dll` | 22,528 | 攻击工具箱 |
| `payload.dll` | 12,800 | 攻击载荷 |
| `payload.bin` | 274 | shellcode 载荷 |
| `guard.dll` | 44,032 | 进程看门狗（注入宿主进程运行） |
| `defense.dll` | 17,408 | 合并防御模块 |
| **合计** | **219,410** | |

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

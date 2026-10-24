# xx — Windows 7 进程存活竞赛程序

一个 `xx.exe` 加多个 `dll` 的用户态存活/对抗程序，运行于 Windows 7 x64。
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
   **新约定已放开本条**：当前实现已在进程入口启用 `SeDebugPrivilege`。
5. 必须告知对方自己最终的二进制文件包含哪些（见第 5 节）。

---

## 2. 架构

| 模块 | 类型 | 职责 |
| --- | --- | --- |
| `xx.exe` | 主进程 | 提权、安全描述符、共享内存、加载各模块、全局日志、按窗口发现并结束对手进程树、布置复活器 |
| `svcguard.dll` | 守护与扩散 | 按目标类型注入模块；监听共享内存心跳，停摆即复活 `xx.exe`；持我方文件锁 |
| `window.dll` | 弹窗与日志 | 多进程冗余弹窗、显示全局日志、递增共享内存心跳；每进程严格一份、建窗串行化 |
| `atkcore.dll` | 攻击工具箱 + **T1/T2 识别** | 导出 `StartAttack`，按序尝试多种用户态结束方法；每 30s 跑一轮 PE 结构指纹与模糊匹配 |
| `dedery.dll` | 全盘清道夫 + **T0 句柄扫描** | 两个线程：① 全盘搜对手程序/守护 dll 及还原源并强删；② 枚举系统句柄，找出持有我方进程句柄的进程并结束（改名/换版本无效） |
| `payload.dll` | 攻击载荷 | 注入目标进程后结束自身进程 |
| `payload.bin` | 位置无关 shellcode | 绕过非 MS 签名 DLL 检查的注入载荷 |
| `guard.dll` | 进程看门狗 | 由 `svcguard` 注入宿主进程，用交接来的进程句柄判活、复活 `xx.exe`、恢复被挂起线程 |
| `defense.dll` | 防御 | 程序目录全量文件只读 + deny-DELETE 装甲、反注入 Hook、心跳看门狗 |

```text
xx.exe
├──> defense.dll                 (保护：文件装甲 / Hook / 看门狗)
├──> atkcore.dll                 (攻击：进程树清理)
│       └──> payload.dll / payload.bin
├──> svcguard.dll                (守护与扩散)
│       ├──> window.dll          (弹窗冗余)
│       ├──> guard.dll           (宿主看门狗)
│       └──> atkcore / dedery    (注给其它宿主)
└──> window.dll                  (主弹窗)
```

更完整的设计说明见 [`project.txt`](project.txt)。

---

## 3. 目录结构

```text
.
├── compile.bat          # 主构建脚本（9 步）
├── compiledebug.bat     # 调试版构建
├── project.txt          # 设计方案与变更记录
├── prepare.txt          # 比赛规则原文（GBK）
├── xxipc.h              # 共享内存布局与命名常量（各模块唯一来源）
│
├── xx/                  # xx.exe 主程序
├── deamon/              # svcguard.dll
├── window/              # window.dll
├── killtools/           # atkcore.dll（parts/ 里是宿主化+WMI 片段）
├── dedery/              # 守护注入的额外模块
├── deleter/             # dedery.dll（parts/ 里是模糊识别、目录监听片段）
├── killer/              # payload.dll + payload.asm
├── guard/               # guard.dll
├── defense/             # defense.dll
├── sysstart/            # 仅测试用：以 SYSTEM 账号启动的安装脚本
│
├── bin/                 # 当前构建产物（交付物）
└── test/                # 实验代码、对手程序还原与注入安全性测试
```

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

共 9 步，全部产物输出到 `bin\`：

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
| 9 | `dedery.dll` |

源文件均为 UTF-8 with BOM，编译选项已带 `-finput-charset=UTF-8`。

---

## 5. 二进制清单（规则第 5 条要求公开）

`bin\` 目录下的全部交付物（合计 **362,258** 字节）：

| 文件 | 大小 | 说明 |
| --- | ---: | --- |
| `xx.exe` | 89,600 | 主程序（**服务双入口**：`--svc install/run/uninstall`，可拿 SYSTEM） |
| `svcguard.dll` | 40,960 | 守护与扩散 |
| `window.dll` | 34,304 | 弹窗与日志 |
| `atkcore.dll` | 72,192 | 攻击工具箱（含宿主化与自证；细节不公开） |
| `dedery.dll` | 35,328 | 全盘清道夫 + T0 句柄扫描 + 目录变化监听（三线程；监听目录**运行时发现**，无硬编码路径） |
| `guard.dll` | 47,616 | 宿主看门狗 |
| `defense.dll` | 27,648 | 防御 |
| `payload.dll` | 14,336 | 攻击载荷 |
| `payload.bin` | 274 | shellcode 载荷 |

---

## 6. 关键机制（概览）

* **存活判定**：由弹窗决定。`window.dll` 在任意宿主进程里建窗即为"存活"；
  心跳写入共享内存，`svcguard` 停摆超过阈值即复活 `xx.exe`。
* **复活链**：`xx.exe` 启动时在 `%TEMP%` 布置一个复活器副本（继承会话随机 token），
  本体被结束后由它把旧映像重新拉起；`svcguard` / `guard` 提供第二、三道看门。
* **注入矩阵**（按目标类型）：

  | 目标 | 注入 |
  | --- | --- |
  | `dwm` / `explorer` / `notepad` / `conhost` / `cmd` | `svcguard` + `window` + `guard` |
  | 其它白名单进程（映像不在 `%WINDIR%` 下） | `atkcore` + `svcguard` + `dedery` |
  | `xx.exe` 自身 | `defense` + `atkcore`（另加 `window` / `svcguard`） |

* **命名与共享状态**：对象名、互斥体名、日志都带会话随机 token / 共享盐，
  同一会话内一致，跨会话不可预测；布局唯一来源是 `xxipc.h`。
* **弹窗单实例**：每进程只允许一份 `window.dll`、一个窗口线程；建窗必须拿到
  串行锁；锁名用共享盐，保证一个会话只有一个窗口。
* **反内存扫描**：对手按固定串扫描进程私有内存来定位我们，因此做了四层处理：
  1. 弹窗标题每会话随机（`XX-PROG ALIVE <8hex>`），传给 API 后立刻 `SecureZeroMemory`；
  2. 备份文件名由会话盐派生（`r<8hex>.bin`），二进制与内存里都没有旧名字；
  3. 九个产物里不含任何特征串明文（按 ASCII 与 UTF-16LE 双形态实测 0 命中）；
  4. 反向利用：`atkcore` 把这三个特征串写进对手进程的私有内存，让它自己扫到自己。
* **对手识别（三层）**：`atkcore` 每 30s 跑一轮 `HuntPass()`，默认 **学习模式**（只记日志不杀）：
  1. **T0 所有权证据**（在 `dedery.dll` 的第二个线程里）：枚举系统句柄，找出"持有我方进程句柄"的进程（行为即敌意，改名无效）；
     每 10s 一轮，全机只允许一份实例动手，系统目录下的进程只记日志不动手；
  2. **T1 PE 结构指纹**：导入表哈希 + 节表哈希 + 头字段，命中基线即判定（改名/换版本仍能命中结构族）；
  3. **T2 模糊匹配**：导入集 Jaccard + 字符串 minhash Jaccard + `.text` 字节直方图余弦，
     融合分 ≥0.75 且**两路以上信号**才算疑似，永远不单独决定杀。
  基线样本取 `yy.exe` / `yyguard.dll` / `lenovo_fix.exe`（程序目录、xx 目录、`%TEMP%`）；
  确认无误报后把 `hunter/hunt.c` 的 `HUNT_LEARN_ONLY` 改成 0 才会真正下杀手。
* **宿主化与自证（实现细节不公开）**：`atkcore` 具备把对手映像当作我方运行载体的能力，
  并对"自称本程序但缺少构建标记"的进程做拒绝处理。具体判据、阈值与流程不在文档中说明。
* **主要间隔**：扩散 `xx` 1000ms / 宿主 5000ms、注入缓存 20s；
  攻击扫描 `xx` 40ms / 宿主副本 2000ms；心跳与日志刷新 500ms。

---

## 7. 运行与合规

* 直接运行 `bin\xx.exe`；会话 0（服务/登录界面）下只做复活，不建窗、不抢弹窗互斥体。
* 不加载内核驱动，全部为 Ring 3 用户态手段；涉及系统进程权限时依赖
  `SeDebugPrivilege`（需在本地安全策略中授予 Administrators）。
* 注入目标带白名单：`%WINDIR%` 下的系统/安全桌面进程一律不注入。

---

## 8. 变更记录

逐轮的实现细节、实测日志与根因分析**不再写在 README 里**，以免正文随轮次膨胀。

* 提交历史：`git log`（每轮一条，含根因与验收证据）
* 设计变更：`project.txt`（与代码保持同步）
* 对手程序还原与攻击面分析：`test/yy_restart_rootcause.md`

---

## 9. 许可与声明

仅用于课程竞赛与技术研究。所有代码与文档按"现状"提供，不对任何使用后果负责。

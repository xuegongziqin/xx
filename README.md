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
| `xx.exe` | 主进程 | 提权、设置安全描述符、加载各模块、维护全局日志、定向关窗 |
| `svcguard.dll` | 守护与扩散 | 每 500ms 扫描新进程并注入自身与 `window.dll`；心跳丢失即复活 `xx.exe` |
| `window.dll` | 弹窗与日志 | 多进程冗余弹窗、显示全局日志、500ms 心跳、拒绝一切可拒绝的窗口消息 |
| `atkcore.dll` | 攻击工具箱 | 导出 `StartAttack`，按序尝试 5 种用户态结束方法 |
| `payload.dll` | 攻击载荷 | 注入目标进程后结束自身进程 |
| `payload.bin` | 位置无关 shellcode | 绕过非 MS 签名 DLL 检查的注入载荷 |
| `guard.dll` | 进程看门狗 | 读取 `xpt.xpt`，复活被保护进程、恢复被挂起线程 |
| `defense.dll` | 合并防御模块 | 文件锁 / 设备锁 / VEH 自愈 / 反注入 Hook / 心跳看门狗 |

```text
xx.exe
│
├──> svcguard.dll (守护与扩散)
│       ├──> window.dll (弹窗与日志)
│       └──> 注入其他进程
│             └──> window.dll (冗余弹窗)
│
├──> atkcore.dll (攻击工具箱)
│       └──> payload.dll / payload.bin (攻击载荷)
│
├──> guard.dll (xpt.xpt 看门狗)
├──> defense.dll (合并防御，StartDefense)
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
| `xx.exe` | 68,096 | 唯一的 exe 主程序 |
| `svcguard.dll` | 19,968 | 守护与扩散 |
| `window.dll` | 16,896 | 弹窗与日志 |
| `atkcore.dll` | 21,504 | 攻击工具箱 |
| `payload.dll` | 12,800 | 攻击载荷 |
| `payload.bin` | 274 | shellcode 载荷 |
| `guard.dll` | 43,008 | 进程看门狗 |
| `defense.dll` | 17,408 | 合并防御模块 |
| **合计** | **199,954** | |

`com\` 是提交包：`com\xx\` 为上述 8 个文件，`com\yy\` 为对照程序，
`com\enableyy.bat` 用于侧边测试开关。

---

## 6. 核心机制

### 6.1 存活与复活

* `window.dll` 在多个宿主进程里抢 `Global\xx_window_guard` 互斥锁，
  谁抢到谁创建弹窗，失败方以 `Sleep(5)` 极速热备。
* 弹窗每 500ms 对 `Global\xx_heartbeat_{后缀}` 事件 `SetEvent`。
* `svcguard.dll` 的 `HeartbeatMonitor` 用 `WaitForSingleObject(event, 500)`
  监听，超时即 `RespawnXX`（已有实例在跑则跳过，避免双重复活），
  随后 `FreeLibraryAndExitThread` 释放本实例。
* `xx.exe` 被删时从同目录 `xxres.bin` 备份还原；`window.dll` 拉起 `xx.exe` 前同样会还原。
* `xx.exe` 还创建 WMI 永久订阅（`__EventFilter` 名为 `XX_RespawnFilter`）兜底复活。

### 6.2 扩散

* `SpreaderThread` 每 500ms 扫描进程列表，注入 `svcguard.dll` 与 `window.dll`。
* 注入结果缓存 10 秒，避免重复扫描与重复注入。
* 跳过关键系统进程（`IsProtected` 黑名单）。
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

* 敌人二进制独占锁（`yy.exe` / `lenovo_fix.exe` / `guard.dll` 等）
* BYOVD 设备符号链接锁（`\\.\BootRepair`、`\\.\kdmapper`、`\\.\kdu`、`\\.\GIO`）
* VEH 冷镜像自愈（拦截 `.text` 被写 `0xCC`）
* `kernel32!TerminateProcess` 字节还原
* `LdrLoadDll` 反注入 Hook（同名 `guard.dll` 用完整路径放行我方）
* 心跳看门狗（掉线则从 `xxres.bin` 还原并拉起 `xx.exe`）
* 刻意不装 `NtCreateThreadEx` / `NtQueueApcThread` Hook（本地 hook 拦不到「别人往我这儿建线程」）

`defense.dll` 以 `-Wl,--export-all-symbols` 导出全部函数，主入口为 `StartDefense`。

### 6.5 提权

`xx.exe` / `svcguard.dll` / `atkcore.dll` 在进程入口调用 `EnableDebugPrivilege`
启用 `SeDebugPrivilege`（`AdjustTokenPrivileges`），用于跨进程句柄打开、
跨会话注入与击杀。

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
3. 创建共享内存与心跳后缀；
4. 建立 WMI 复活订阅；
5. 依次加载 `svcguard.dll`、`guard.dll`、`window.dll`、`defense.dll`；
6. `AttackThread` 加载 `atkcore.dll` 并调用 `StartAttack` 开始攻击。

---

## 9. 合规性

* **exe 数量**：仅 `xx.exe` 一个，符合规则第 1 条。
* **内核驱动**：无，符合规则第 3 条。
* **调试特权**：原规则第 4 条禁止，**按新约定已放开**，当前实现已启用
  （见 `project.txt` 第 6 节）。
* **二进制清单**：见本文第 5 节，符合规则第 5 条。
* **弹窗存活证明**：`window.dll` 弹窗显示全局日志，符合规则第 2 条。

---

## 10. 许可与声明

本项目为课程竞赛作品，不提供任何形式的担保。
请勿在竞赛环境以外的任何系统上运行本程序。

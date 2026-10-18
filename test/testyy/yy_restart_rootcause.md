# yy 不停启动：根本原因分析（基于 `test\testyy\yy.exe.asm` / `yyguard.dll.asm`）

> 结论先说：**这不是"防复活"没做好，而是它的守护逻辑本身写成了自激振荡（livelock）**。
> 三份 guard 互相把对方杀掉、再各自补一个，补出来的又带 guard —— 形成 150ms 一轮的
> 「杀光 → 各补一个 → 再杀光」循环。对外表现就是进程表里 `yy.exe` 疯狂出现/消失。

---

## 1. 反汇编还原出来的真实逻辑

### 1.1 `yy.exe` 只有一个 `CreateProcessW`：蜜罐

`yy.exe.asm` 里唯一的 `CreateProcessW`（行 3524）位于 `sub_1400034A0`（线程入口），逻辑是：

```c
for (;;) {                                          // 行 3568-3571：Sleep(2000) 后跳回
    GetModuleFileNameW(NULL, self);                 // 行 3405
    GetTempPathW(tmp); lstrcatW(tmp, L"lenovo_fix.exe");   // 行 3412-3418
    CopyFileW(self, tmp, FALSE);                    // 行 3422：把自己复制成蜜罐
    if (!进程表里存在 lenovo_fix.exe)               // 行 3426-3493：名字比较
        CreateProcessW(NULL, "\"%s\" <隐藏参数>", …);  // 行 3508-3524
    Sleep(2000);                                    // 行 3570
}
```

→ `lenovo_fix.exe` 是**它自己**放的诱饵。这个循环本身节流良好（2 秒一次），
**不是"不停启动"的来源**（除非它的镜像文件被删了 —— 见 1.3）。

### 1.2 `yyguard.dll`：真正的原因

`yyguard.dll.asm` 里 `sub_180001000` = `CountYyInstances()`：

```c
n = 0;
snapshot(TH32CS_SNAPPROCESS);
for (每个 pe) {
    if (_wcsicmp(pe.szExeFile, L"yy.exe") == 0) n++;   // 行 426-432（cmp eax,1 / adc ebx,0）
}
return n;                                              // 只按**文件名**数
```

主循环（`sub_1800015B0` 起，循环体在 `loc_1800016B4`）：

```c
for (;;) {                                                  // 行 947 起
    cnt = CountYyInstances();                               // 行 948
    if (cnt <= 0) goto spawn;                               // 行 950-951
    if (FindWindowW(L"YYWnd_7f3a9c1e", NULL)) goto spawn;    // 行 952-956（有主窗口 → 不杀）
    if (now - lastCheck >= 1500) {                          // 行 957-967
        snapshot;
        for (每个 pe) {
            if (_wcsicmp(pe.szExeFile, L"yy.exe") == 0)     // 行 983-987
                OpenProcess(PROCESS_TERMINATE, …)           // 行 988-991
                TerminateProcess(h, 0);                     // 行 995-997  ★ 杀光所有 yy.exe
        }
        cnt = CountYyInstances();                           // 行 1014
    }
spawn:
    if (cnt >= 3) goto sleep;                               // 行 1022-1023  ★ 目标 3 份
    if (now - lastSpawn < 250) goto sleep;                  // 行 1030-1033
    if (GetFileAttributesW(<dir>\yy.exe) == INVALID) {       // 行 1034-1037
        CopyFileW(<dir>\yyres.bin  → <dir>\yy.exe);          // 行 1042-1049  从备份还原
        else CopyFileW(%TEMP%\yy_backup_7f3a9c1e.dat → …);   // 行 1052-1062
    }
    CreateProcessW(<dir>\yy.exe, NULL, NULL, NULL, FALSE, …); // 行 1064-1090
    if (ok) lastSpawn = now;                                // 行 1097-1098
sleep:
    Sleep(150);                                             // 行 1100-1102
    if (stop) return;                                       // 行 1103-1105
}
```

### 1.3 `yy.exe` 自己也会备份/还原

`yy.exe.asm` 行 2017 / 2408-2418 有两处 `CopyFileW`：
`<dir>\yyres.bin` ⇄ `<dir>\yy.exe`、`%TEMP%\yy_backup_7f3a9c1e.dat` ⇄ 自身。
→ **镜像文件被删掉也能自己长回来**（这正是"删了又出现"的第二条来源）。

---

## 2. 三个根本原因

### 根因 A：guard 没有单例 / 没有互斥

`yyguard.dll` 被 `Spread: yyguard.dll injected into …`（行 2983-2990、3151-3152）注入到
**每个 yy 进程 + 若干宿主进程**。每一份注入都独立跑上面那个 `for(;;)`：

* N 份 guard，各自读 `cnt`、各自判断 `cnt < 3`、各自 `CreateProcessW`；
* 谁也不知道别人刚补了一个 → 实际数量直接冲到 N×3，下一轮又全部被杀；
* 没有 `Global\` 互斥体、没有 elector、没有共享计数。

### 根因 B：「没有主窗口」被当成「实例全挂了」

判据是 `FindWindowW(L"YYWnd_7f3a9c1e", NULL)`（行 954）。这个判据有两个致命问题：

1. 主窗口没建出来（启动瞬间、被对手隐藏/销毁、桌面切换、session 0）→ 判据返回 NULL；
2. 一旦为 NULL 且超过 1.5 秒，guard 就认定「所有 yy.exe 都是僵尸」，**TerminateProcess 全杀**
   （行 983-997）—— 包括**自己所在的那个进程**（代码里没有任何 `pid != GetCurrentProcessId()` 过滤）。

杀完计数立刻变 0，于是每份 guard 都走 `spawn` 分支：补一个。
补出来的 yy.exe 载入 guard，guard 又杀光所有 yy.exe —— 自激。

### 根因 C：没有崩溃/风暴保护

* 只有 `Sleep(150)` + 「距上次 spawn ≥ 250ms」两道闸；
* 没有「连续失败退避」，没有「一分钟内最多 spawn 几次」的熔断；
* `CopyFileW` 还原时**不校验还原结果**：如果 `yyres.bin` 被我们截断成 0 字节
  （这是我方 `NeutralizeFile()` 干的事），它会还原出一个 **0 字节的 `yy.exe`**，
  `CreateProcessW` 必然失败 → 下一轮再来一次 → 250ms 一次的空转 + 磁盘写。
* `cnt` 是**先数后杀、杀完再数**，两次快照之间进程可能正好在创建/退出，计数天然抖动；
  抖动一次就多补一次。

---

## 3. 正确的写法应该是什么样

一句话：**把「守护」做成"选举出的唯一守护者 + 幂等收敛"，而不是"每个被注入体都当守护者"。**

| 缺陷 | 正确做法 |
|---|---|
| N 份 guard 抢着补 | 用 `Global\` 命名互斥体 + Everyone SD 选举**唯一** guard；落选者只做旁观（不消费 CPU、不碰进程） |
| 「没有主窗口」= 全死 | 删掉这个判据。改用**实例自己的存活信号**（命名事件 / 心跳 tick / `SendMessageTimeout(WM_NULL)` 探测），只对**确认僵死的那一个 PID** 动手，绝不批量 `TerminateProcess` |
| 杀光再补 | **只补不杀**（补足到 K 份即可）；真要清理僵尸，先请它退出（`WM_CLOSE`/`ExitProcess` 请愿），超时才 `TerminateProcess` 单个 PID |
| 250ms 一把梭 | 指数退避（100ms→200→400…封顶 5s）+ **熔断**（60 秒内 spawn 超过 N 次就停 30 秒并记日志） |
| 只按文件名计数 | 名字 + **完整路径**双匹配（`QueryFullProcessImageNameW`），避免把别人的 `yy.exe`/我们自己的同名文件算进去；同时排除自己 |
| 还原不校验 | 还原后必须校验：文件存在、大小 > 0、能被 `CreateProcessW` 起来并且**存活 ≥ 1.5s**；否则视为失败并退避 |
| 日志缺失 | 每次 spawn/失败/退避/熔断都写一行（含原因），否则只能靠猜 |

---

## 4. 参考实现

见同目录 **`yyguard_fixed.cpp`**（C++17，Win32，MinGW g++ 可直接编）：

```
g++ -std=c++17 -O2 -Wall -Wextra -municode -static -o yyguard_fixed.exe yyguaard_fixed.cpp -ladvapi32 -lpsapi
```

它实现了：
选举单例、精确计数（名字+路径）、只补不杀、僵死探测与单 PID 清理、
指数退避 + 熔断、还原校验、结构化日志。把它和上面 1.2 的反汇编逐条对照，
就能看清「原版哪里错、正确版为什么不会疯转」。


---

## 5. 验证（用参考实现跑出来的真实日志）

编译（GCC 16.1.0 MinGW-w64，零警告）：

```
g++ -std=c++17 -O2 -Wall -Wextra -municode -static -o yyguard_fixed.exe yyguard_fixed.cpp -ladvapi32 -lpsapi
```

**A. 目标进程不存在、也没有可用备份时**（原版会在这种状态下 250ms 一次空转，
本实现按指数退避 100→200→400→800→1600→3200→5000ms 收敛）：

```
[10:03:40.673] 守护启动：exe=yy.exe dir=E:\C++\xx\test 目标数=1 dryRun=1
[10:03:40.686] 没有任何可用备份，无法还原 E:\C++\xx\test\yy.exe
[10:03:40.687] 连续失败 1 次 → 退避 100ms
[10:03:41.211] 连续失败 2 次 → 退避 200ms
[10:03:41.726] 连续失败 3 次 → 退避 400ms
[10:03:42.239] 连续失败 4 次 → 退避 800ms
[10:03:43.278] 连续失败 5 次 → 退避 1600ms
[10:03:45.337] 连续失败 6 次 → 退避 3200ms
[10:03:48.957] 连续失败 7 次 → 退避 5000ms
```

**B. 熔断（把窗口调成 8 秒 / 上限 3 次做验证）**：

```
[10:04:56.869] 连续失败 1 次 → 退避 100ms
[10:04:57.902] 连续失败 2 次 → 退避 200ms
[10:04:58.932] 连续失败 3 次 → 退避 400ms
[10:04:59.454] 熔断：8000ms 内已启动 3 次（上限 3）→ 暂停 8000ms 再试
[10:05:07.672] ……冷却结束，恢复尝试
```

**C. 数量够时**（原版此时会去检查窗口、然后杀光；本实现什么都不做）：

```
[10:03:29.320] 守护启动：exe=notepad.exe dir=C:\Windows\System32 目标数=2 dryRun=1
[10:03:29.330] 目标数不足 → 会启动: "C:\Windows\System32\notepad.exe"
[10:03:30.373]（仍不足）→ 再次启动（间隔 ≥ minSpawnGapMs = 800ms）
```

对照 1.2 的原版逻辑可以看到：原版在同样的"进程不存在"状态下是
`Sleep(150)` + `250ms 间隔` + `CopyFileW(0 字节备份)` → **每秒 4 次无效 CreateProcess + 磁盘写**，
这就是"不停启动"的机械原因。

## 6. 文件

| 文件 | 说明 |
|---|---|
| `test\yy_restart_rootcause.md` | 本文：根本原因分析（含反汇编行号证据） |
| `test\yyguard_fixed.cpp` | C++17 参考实现（本文件，~600 行） |
| `test\yyguard_fixed.exe` | 上面编出来的可执行（930 KB，静态链接） |

> 说明：这是**参考实现**，不在 `xx.exe` 的构建链里，也不会被 svcguard 注入；
> 它的用途是把原版正确写法的样子摆出来，便于逐条对照原版反汇编，
> 同时可以拿它做实验（`--dry-run` 只看决策、不真启动）。

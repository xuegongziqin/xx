# yy 不停启动：根本原因分析 + 他对 xx 的攻击面（基于 testyy 反汇编）

> 一句话结论：**"不停启动"是它的守护逻辑自激**（N 份 guard 互相杀光再各补一个）；
> **对 xx 的攻击全部基于固定名字**（`xx` 基名、`XX_ALIVE_WND_` 类名、`xx_window_guard` 互斥体、
> 我们的 dll 文件名、我们 WMI 订阅名）—— 我们现在的随机命名 + DACL + 装甲恰好每条都错开。

---

## 1. 反汇编还原的真实逻辑

### 1.1 `yy.exe` 只有一个 `CreateProcessW`：蜜罐（每 2 秒）

`sub_1400034A0`（yy.exe.asm 3369-3593）：
`GetModuleFileNameW` → `GetTempPathW` + `lenovo_fix.exe` → `CopyFileW(自身→蜜罐)` →
进程表里没有 `lenovo_fix.exe` 就 `CreateProcessW` → `Sleep(2000)` 循环。
**这不是"不停启动"的来源**（它节流良好）。

### 1.2 `yyguard.dll` 的主循环（sub_1800015B0，947-1105 行）

```c
for (;;) {
    cnt = CountYyInstances();                              // sub_180001000：只按**文件名**数
    if (cnt > 0) {
        if (!FindWindowW(L"YYWnd_7f3a9c1e", NULL)) {        // 没有主窗口
            if (now - lastCheck >= 1500) {
                遍历进程表：TerminateProcess(所有叫 yy.exe 的进程);   // ★ 连自己也不放过
                cnt = CountYyInstances();
            }
        } else lastCheck = 0;
    }
    if (cnt < 3 && now - lastSpawn >= 250) {                 // ★ 目标 3 份
        if (!存在 <dir>\yy.exe) {
            CopyFileW(<dir>\yyres.bin → <dir>\yy.exe);        // 从备份还原（不校验大小）
            else CopyFileW(%TEMP%\yy_backup_7f3a9c1e.dat → …);
        }
        CreateProcessW(<dir>\yy.exe, NULL, cwd=<dir>);
        lastSpawn = now;
    }
    Sleep(150);
}
```

### 1.3 三个致命缺陷

| # | 缺陷 | 后果 |
|---|---|---|
| A | **无单例**：被注入 N 个进程就有 N 个守护者，各补各的 | 数量冲到 N×3 |
| B | **"没有 YYWnd 窗口" = "实例全挂"** → 杀光所有 yy.exe（无 `pid != self` 过滤） | 杀完计数=0 → 每个守护者立刻补一个 → 自激 |
| C | **无退避/熔断**；还原不校验（0 字节备份也能还原） | 250ms 一次空转：每秒 4 次无效 CreateProcess + 磁盘写 |

---

## 2. 它对 xx 的攻击面（yy.exe.asm / yyguard.dll.asm 中的证据）

见第 7 节完整表。核心三条：

1. **hunter**（yy.exe 3596-3646、yyguard.dll 611-727）：每 500ms `EnumWindows`，
   类名含 `XX_ALIVE_WND_` → `TerminateProcess(宿主 PID)`。
2. **杀 `xx.exe`**（yy.exe 3837-3891、4116-4143）：进程基名（去扩展名）== `xx` →
   结束进程 → `DeleteFileW(镜像)` → `DeleteFileW(<dir>\xxres.bin)`。
3. **删我方 dll**（yy.exe 4150-4300）：`svcguard.dll / window.dll / atkcore.dll / payload.dll /
   defense.dll / DeamonWindows.dll / deamon.dll / lenovo_fix.exe / yyguaard.dll` 逐个删。
   另有两处蹲守：`Global\xx_window_guard`、`xx_window_guard`、`xx_window_make_guard`；
   以及针对我们 WMI 订阅名 `XX_RespawnFilter` / `XX_RespawnConsumer` 的清理。

---

## 3. 正确写法应该是什么

| 缺陷 | 正确做法 |
|---|---|
| N 份 guard 抢着补 | `Global\` 互斥体 + Everyone SD 选举**唯一**守护者，落选者退出 |
| 无窗口 = 全死 | 删掉该判据；改为 `SendMessageTimeoutW(WM_NULL, 600ms)` 逐 PID 探测，只清理确认僵死的那一个 |
| 杀光再补 | **只补不杀**（补足到 K 份） |
| 250ms 一把梭 | 指数退避 100→200→400→…→5000ms + 熔断（60s 内超 6 次 → 停 30s） |
| 只按名字数 | 名字 + 完整镜像路径双匹配，排除自己 |
| 还原不校验 | 备份必须 >0 字节；spawn 后必须存活 ≥2s 才算成功 |

---

## 4. 参考实现（两个文件，别搞混）

| 文件 | 是什么 | 产物 |
|---|---|---|
| `yy_reconstructed.cpp` | 从 asm **忠实还原**的原版（含 bug，跑起来会自激） | `yyguard_reconstructed.dll`、`yy_reconstructed.exe` |
| `yyguard_fixed.cpp` | 同一功能的**正确实现**（选举/只补不杀/退避/熔断/验证） | `yyguard_fixed.exe`、**`yyguard_fixed.dll`** |

编译命令：

```
# 还原版
g++ -std=c++17 -O2 -Wall -Wextra -municode -DYY_BUILD_DLL -shared -static \
    -o yyguaard_reconstructed.dll yy_reconstructed.cpp -ladvapi32 -lpsapi
g++ -std=c++17 -O2 -Wall -Wextra -municode -static -o yy_reconstructed.exe yy_reconstructed.cpp -ladvapi32 -lpsapi

# 正确版
g++ -std=c++17 -O2 -Wall -Wextra -municode -static -o yyguaard_fixed.exe yyguaard_fixed.cpp -ladvapi32 -lpsapi
g++ -std=c++17 -O2 -Wall -Wextra -municode -static -DGXX_DLL -shared -o yyguaard_fixed.dll yyguaard_fixed.cpp -ladvapi32 -lpsapi
```

`yyguard_fixed.dll` 导出 `XxStartGuard()`（幂等）与 `XxBuildTag()`；`DllMain` 里起线程，
被注入宿主后与 `yyguaard.dll` 的加载方式完全一致。

---

## 5. 验证（正确版的真实日志）

```
[10:15:32.794] 守护启动：exe=yy.exe dir=E:\C++\xx\test 目标=1 dryRun=1
[10:15:32.807] 没有可用备份: E:\C++\xx\test\yy.exe
[10:15:32.807] 连续失败 1 次 → 退避 100ms
[10:15:33.849] 没有可用备份: E:\C++\xx\test\yy.exe
[10:15:33.851] 连续失败 2 次 → 退避 200ms
...
[10:04:59.454] 熔断：8000ms 内已尝试 3 次（上限 3）→ 暂停 8000ms
```

对照原版：同样状态下是 150ms 循环 + 250ms 间隔 + 0 字节还原 → **每秒 4 次无效启动**。
---

## 7. 新逆向（2026-10-18 版 asm）：他们对 xx 的攻击清单与有效性

从 `yy.exe.asm` / `yyguard.dll.asm` 里逆出来的、**明确针对我们**的东西：

| # | 攻击 | 反汇编证据 | 判据 | 对当前版本**是否有效** |
|---|---|---|---|---|
| 1 | hunter：杀"弹窗宿主"进程 | `yyguard.dll.asm` 611-727（`EnumWindows`+`TerminateProcess`，500ms 一轮）；`yy.exe.asm` 3596-3646 同款 | `EnumFunc` 里 `GetClassNameW` + `wcsstr(cls, L"XX_ALIVE_WND_")`（只比**类名**） | **无效**。我们的类名是 `wnd_<16hex>`，`XX_ALIVE_WND_` 只是标题。若改回旧类名，它会每 500ms 杀一次宿主（explorer），非常致命 |
| 2 | 蹲守互斥体 | `yyguard.dll.asm` 552-604 / 734-788；`yy.exe.asm` 1714-1741 | `CreateMutexW("Global\\xx_window_guard")`、`"xx_window_guard"`、`"xx_window_make_guard"`，抢到就 `Sleep(60000)` 死循环**永久持有** | **无效**。我们现在用 `Global\xx_wg_<16hex token>` + Everyone SD，名字完全不同。但那三个旧名字还在 `xxipc.h` 里（`WINDOW_GUARD_MUTEX` 等），建议删掉，免得将来误用 |
| 3 | 杀 `xx.exe` | `yy.exe.asm` 3837-3891（取基名去扩展名、与 `L"xx"` 比大小写不敏感）、4116 `TerminateProcess` | 进程基名 == `xx` | **被 DACL 挡住**。我们给自己进程设的 DACL 只给用户 SID `GRGWGX`（读/写/执行），**不含 PROCESS_TERMINATE**，`OpenProcess(PROCESS_TERMINATE)` 会拒绝；只有它跑成 SYSTEM（SY:GA）才可能成功 |
| 4 | 删 `xx.exe` + `xxres.bin` | `yy.exe.asm` 4126 `DeleteFileW(ExeName)`；4127-4143 拼 `<dir>\xxres.bin` 再 `DeleteFileW` | 杀完之后删镜像和我们的复活备份 | **多半被只读装甲挡住**（`DeleteFileW` 对只读文件 ACCESS_DENIED）。注意 defense 里 `xxres.bin` 的装甲是 `ArmorFile(path, 1, FALSE)` —— 只只读、不 deny-delete；建议改成 `TRUE` |
| 5 | 删我方模块文件 | `yy.exe.asm` 4150-4300 与指针表 9611-9627 | 名单：`svcguard.dll`、`window.dll`、`atkcore.dll`、`payload.dll`、`defense.dll`、`DeamonWindows.dll`、`deamon.dll`、`lenovo_fix.exe`、`yyguard.dll`，逐个 `<dir>\<name>` → `DeleteFileW` | **部分有效**。`svcguard/window/atkcore` 有 deny-delete 装甲；**`payload.dll` / `defense.dll` / `guard.dll` 没有**（`guard.dll` 不在它名单里，但 `payload.dll`/`defense.dll` 在）。已加载的文件删不掉，未加载的会被删。**建议**：把 `payload.dll`、`defense.dll`、`guard.dll` 也加进 `ArmorFile` |
| 6 | 删我们的 WMI 复活订阅 | `yy.exe` 字符串 `__EventFilter.Name="XX_RespawnFilter"`、`CommandLineEventConsumer.Name="XX_RespawnConsumer"` | 按名字定位我们的 Filter/Consumer 再删 | **有效**（削弱）。它不是致命：我们还有 svcguard + 复活器两条复活链 |
| 7 | 自清理 | `yy.exe.asm` 4150-4300 | 删自己目录下的 `lenovo_fix.exe` / `yyguard.dll` | 与我们无关 |

**它没有攻击的**（对我们有利）：

* `dedery.dll`（我们的全盘清道夫）**不在它的名单里**；
* 不碰共享内存（`Global\xx_shared_mem`）、不碰 `xx_wg_<token>` 互斥体；
* 我们的复活器叫 `svchost.exe` / `explorer.exe` / `conhost.exe`，它只按 `xx` 基名和窗口类名判断 → 不会误杀。

**结论**：新 yy 对 xx 的攻击是"**点名 xx + 点名我们的 dll + 点名我们的窗口类名/互斥体名**"，
全部基于**固定名字**。我们现在这套"随机 token 命名 + 类名 `wnd_<hex>` + DACL + 只读/deny-delete 装甲"
恰好每一条都错开了；**真正要补的只有两处**：

1. `xxres.bin` 的装甲从"只只读"改成 `bDenyDelete = TRUE`；
2. 把 `payload.dll` / `defense.dll` / `guard.dll` 加进 `ArmorFile` 名单（否则对目录下未加载的那份会被直接删掉）。

## 8. 两个工程文件的关系（为什么之前是 exe）

| 文件 | 是什么 | 产物 |
|---|---|---|
| `yy_reconstructed.cpp` | 从 asm **忠实还原**的原版逻辑（含 bug，跑起来会自激） | `yyguard_reconstructed.dll`（`-DYY_BUILD_DLL`）、`yy_reconstructed.exe`（控制台观察） |
| `yyguard_fixed.cpp` | 同一功能的**正确实现**（选举/只补不杀/退避/熔断/验证） | `yyguard_fixed.exe`（控制台实验，`--dry-run`）、**`yyguard_fixed.dll`**（`-DGXX_DLL`，导出 `XxStartGuard`/`XxBuildTag`，可被注入） |

原版 `yyguard.dll` 是 DLL，所以正确版也提供了 DLL 构建（`-DGXX_DLL`）：
`DllMain` 里起线程调 `XxStartGuard()`，导出函数幂等，重复注入不会叠出第二份守护循环。


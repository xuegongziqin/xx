# yy (11.cpp) 弱点全景分析

> 对手项目：11.cpp —— 一个带 ntdll hook、WMI 复活、进程互杀、DLL 注入的防御型程序
> 本文档按攻击面分层归纳其全部可利用弱点

---

## 目录

1. 窗口层弱点
2. 跨进程保护的自锁缺陷
3. ntdll Hook 的根本性误用
4. WMI 复活链路漏洞
5. 代码级崩溃/可触发缺陷
6. 竞速与调度缺陷
7. 硬编码常量清单
8. 跨进程检测绕过矩阵
9. 攻击优先级速查

---

## 一、窗口层弱点

### 1.1【致命】WndProc 对 `WM_DESTROY` 的处理完全无效

```cpp
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_QUERYENDSESSION || msg == WM_ENDSESSION) return FALSE;
    if (msg == WM_CLOSE) return 0;
    if (msg == WM_DESTROY) return 0;   // ← 白写，返回值被忽略
    return DefWindowProcW(hwnd, msg, wp, lp);
}
```

**原因**：`DestroyWindow(hwnd)` 直接进入销毁流程，发 `WM_DESTROY` 时完全忽略 WndProc 返回值。返回 0 阻止不了。

**利用**：

```cpp
HWND h = FindWindowW(L"YYWnd_7f3a9c1e", NULL);
DestroyWindow(h);   // 直接销毁
```

---

### 1.2【高】`WM_SETTEXT` 未拦截 —— 可跨进程改标题

WndProc 只处理 4 条消息，`WM_SETTEXT` 落到 `DefWindowProcW`，默认放行。

**利用（规则降维打击）**：

```cpp
HWND yyWnd = FindWindowW(L"YYWnd_7f3a9c1e", NULL);
SetWindowTextW(yyWnd, L"xx_ALIVE_WND");   // 直接改标题
```

yy 主循环 20ms 重建窗口会恢复标题，5ms 间隔循环改即可压制。

---

### 1.3【高】`SetParent` 挂到隐藏窗口 —— 窗口永久不可见

yy 主循环只检查 `IsWindow(g_hMainWnd)`：

```cpp
if (!g_hMainWnd || !IsWindow(g_hMainWnd)) {
    CreateMainWindow(hInst);
}
```

**利用**：

```cpp
HWND hidden = CreateWindowW(L"STATIC", NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL);
SetParent(yyWnd, hidden);   // IsWindow 仍返回 TRUE，不会重建
// 但窗口因父窗口不可见而不可见 → GUI 永久消失
```

---

### 1.4【高】主循环无消息洪水防御 —— 可 100% 卡死

```cpp
while (true) {
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { ... }
    if (!g_hMainWnd || !IsWindow(g_hMainWnd)) CreateMainWindow(hInst);
    Sleep(20);   // ← 只有队列空了才执行
}
```

**利用**：

```cpp
while (true) PostMessageW(yyWnd, WM_NULL, 0, 0);
```

消息队列永不为空，`Sleep(20)` 永不执行，yy CPU 100%。叠加多线程可彻底冻死所有后台 worker。

---

### 1.5【中】未处理的窗口消息清单

WndProc 只处理 4 条，其余全部落到 `DefWindowProcW`：

| 消息 | 利用 |
|---|---|
| `WM_SYSCOMMAND` + `SC_MINIMIZE` | 最小化 yy 窗口 |
| `WM_WINDOWPOSCHANGING` | hook 后拦截位置变化，移出屏幕 |
| `WM_ENABLE` | 禁用窗口（灰化） |
| `WM_SETICON` | 改图标伪装成其他进程 |
| `WM_SHOWWINDOW` | 强制显示/隐藏 |
| `WM_NCACTIVATE` | 伪造激活状态 |

---

### 1.6【中】日志框是标准 EDIT 控件 —— 可跨进程注入嘲讽

```cpp
g_hLogBox = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ... | ES_READONLY, ...);
```

`ES_READONLY` 只拦键盘输入，不拦 `EM_REPLACESEL`。

**利用 1 — 追加嘲讽**：

```cpp
HWND logBox = FindWindowExW(yyWnd, NULL, L"EDIT", NULL);
int len = GetWindowTextLengthA(logBox);
SendMessageA(logBox, EM_SETSEL, len, len);
SendMessageA(logBox, EM_REPLACESEL, FALSE, (LPARAM)"yy.exe is just a waste\r\n");
```

**利用 2 — 清空日志**：

```cpp
SendMessageW(logBox, WM_SETTEXT, 0, (LPARAM)L"[00-00-00] yy.exe is just a waste\r\n");
```

**利用 3 — 刷屏拖慢 LogAction**：

```cpp
while (true) {
    SendMessageA(logBox, EM_REPLACESEL, FALSE, (LPARAM)"yy.exe is just a waste\r\n");
    Sleep(10);
}
```

日志越长，`GetWindowTextLengthA` + `EM_SETSEL` 定位越慢，拖慢 yy 的 LogAction。

**利用 4（最狠，需注入）— 子类化 WndProc 替换日志输出**：

```cpp
// 在注入到 yy 的 DLL 内
WNDPROC g_oldLogProc = NULL;
LRESULT CALLBACK EvilLogProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == EM_REPLACESEL) {
        return CallWindowProcA(g_oldLogProc, h, m, FALSE,
            (LPARAM)"[00-00-00] yy.exe is just a waste\r\n");
    }
    return CallWindowProcA(g_oldLogProc, h, m, w, l);
}
g_oldLogProc = (WNDPROC)SetWindowLongPtrW(logBox, GWLP_WNDPROC, (LONG_PTR)EvilLogProc);
```

yy 打印任何内容都变成嘲讽。

---

### 1.7【中】类名硬编码 `YYWnd_7f3a9c1e`

二进制明文可见，静态扫描即可提取。任何进程 `FindWindowW(L"YYWnd_7f3a9c1e", NULL)` 直接拿句柄。

---

## 二、跨进程保护的自锁缺陷

### 2.1【致命】yy 的注入系统把自己锁死了

```cpp
static NTSTATUS NTAPI SpyCreateThread(..., HANDLE pr, ...) {
    if (!IsHoodInjectTarget(pr)) {
        return 0;
    }
    return g_TrampolineCreateThread(...);
}
```

`IsHoodInjectTarget` 只放行 4 个进程：

```cpp
const wchar_t* HOOD_LIST[] = {
    L"explorer.exe", L"svchost.exe", L"lsass.exe", L"cmd.exe"
};
```

**后果**：
- `ServiceWorker` 遍历所有进程注入 `yyguard.dll`
- 但 `CreateRemoteThread` 触发 yy 进程自己的 `NtCreateThreadEx` hook
- `SpyCreateThread` 检查目标 → 只有上述 4 个放行
- 其他所有进程的注入静默失败

**结论**：yy 只能向 4 个进程注入成功。你的 xx.exe 永远不会被 yy 注入（除非你伪装成这 4 个名字之一——千万别）。

---

### 2.2【高】`SpyCreateThread` 返回 `0` 而非错误码

```cpp
return 0;   // STATUS_SUCCESS
```

`NtCreateThreadEx` 返回 `STATUS_SUCCESS` 表示线程创建成功，`*th` 应被赋值。但 `*th` 从未赋值 → 调用者拿到未初始化句柄。

正确应返回 `0xC0000022`（STATUS_ACCESS_DENIED）。

---

### 2.3【高】hook 只在进程内生效 —— 从外部注入 yy 完全绕过

`ForceLockDllInjection` / `ForceLockRemoteCall` 通过 `VirtualProtect` 修改 yy 进程自己的 ntdll 代码页。写时复制机制下，其他进程的 ntdll 是独立副本。

**利用**：

```cpp
// 从 xx 进程直接注入 yy，yy 完全拦不住
HANDLE hYY = OpenProcess(PROCESS_ALL_ACCESS, FALSE, yyPid);
LPVOID p = VirtualAllocEx(hYY, NULL, sz, MEM_COMMIT, PAGE_READWRITE);
WriteProcessMemory(hYY, p, dllPath, sz, NULL);
HANDLE h = CreateRemoteThread(hYY, NULL, 0, LoadLibraryW, p, 0, NULL);
```

yy 的所有 ntdll hook 对此零防御。

---

## 三、ntdll Hook 的根本性误用

### 3.1【致命】保护机制与拦截机制混为一谈

yy 花大力气 hook 自己进程内的 ntdll，防的是"别人往我进程里注入"。但：

1. 这些 hook 只在 yy 进程内生效 —— 对跨进程攻击（从 xx 注入 yy）零防御
2. 这些 hook 反而锁死了 yy 自己的注入能力 —— `SpyCreateThread` 拦截了 ServiceWorker 的注入
3. 真正能跨进程保护自己的方式（内核回调、PPL、签名）它一个都没用

---

### 3.2【中】VEH 只检测 `0xCC` 断点

```cpp
static LONG WINAPI QuickVeh(PEXCEPTION_POINTERS pEx) {
    if (pEx->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT) {
        // 只处理断点
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
```

**非断点 patch 完全绕过 VEH**：

```cpp
DWORD old;
VirtualProtect(yyCodeAddr, N, PAGE_EXECUTE_READWRITE, &old);
memset(yyCodeAddr, 0x90, N);   // NOP 掉关键指令
VirtualProtect(yyCodeAddr, N, old, &old);
// VEH 无感知
```

---

### 3.3【中】`ShieldWorker` 只恢复 5 字节 —— 可让 yy 自杀

```cpp
unsigned char localCache[5];
memcpy(localCache, pTarget, 5);
// ...
memcpy(pTarget, localCache, 5);   // 只恢复前 5 字节
```

12 字节绝对跳转是 `48 B8 xx xx xx xx xx xx xx xx FF E0`。恢复前 5 字节 = `48 B8 xx xx xx`，后面 7 字节仍是 patch 状态。

**结果**：`TerminateProcess` 变成半损坏状态，调用它会崩溃 yy 进程。攻击者可借此让 yy 自杀。

---

### 3.4【中】未 hook `NtProtectVirtualMemory`

yy 自己用 `VirtualProtect` patch 别人，却没 hook 自己的 `NtProtectVirtualMemory`。攻击者可以随便改 yy 的代码页。

---

## 四、WMI 复活链路漏洞

### 4.1【高】删除订阅时没删 Binding

```cpp
wchar_t fp[] = L"__EventFilter.Name=\"XX_RespawnFilter\"";
wchar_t cp[] = L"CommandLineEventConsumer.Name=\"XX_RespawnConsumer\"";
pSvc->DeleteInstance(fp, 0, NULL, NULL);   // 只删 Filter
pSvc->DeleteInstance(cp, 0, NULL, NULL);   // 只删 Consumer
```

WMI 三件套是 Filter + Consumer + Binding。缺少删除 Binding：
- 残留的 `__FilterToConsumerBinding` 引用被删对象
- WMI 仓库 `OBJECTS.DATA` 可能自动重建部分对象

**正确删除顺序**：Binding → Consumer → Filter，循环校验至干净。

---

### 4.2【中】订阅名硬编码

```cpp
L"YY_RespawnFilter"
L"YY_RespawnConsumer"
```

公开、可预测、二进制明文。任何人扫描即可主动删除。

---

### 4.3【中】复活命令依赖 `cmd.exe`

```cpp
swprintf_s(cmd, L"cmd.exe /c if not exist \"%s\" copy /y ...", ...);
```

`cmd.exe` 被替换/禁用/hook → 复活失败。

---

### 4.4【中】备份文件 `yyres.bin` 路径固定

```cpp
if (bsl) { bsl[1] = L'\0'; wcscat_s(bakPath, L"yyres.bin"); }
```

**利用**：
- 占位锁（`CreateFileW` with `dwShareMode=0`）让 yy 复制失败
- 替换成恶意内容，WMI 复活时执行

---

### 4.5【低】`CoInitializeEx` 失败即 return

```cpp
HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
if (FAILED(hr)) return;
```

若线程已用 `COINIT_APARTMENTTHREADED` 初始化，返回 `RPC_E_CHANGED_MODE`（非错误）。代码直接 return，WMI 操作被跳过。多线程下其他 worker 可能撞上此坑。

---

## 五、代码级崩溃/可触发缺陷

### 5.1【中】`YyLog` 每次从头写文件 —— 日志永远只有最后一条

```cpp
HANDLE h = CreateFileW(path, GENERIC_WRITE, ..., OPEN_ALWAYS, ...);
WriteFile(h, line, ...);
```

- `GENERIC_WRITE` 不是 `FILE_APPEND_DATA`
- `OPEN_ALWAYS` 打开后文件指针在 0
- 每次写覆盖开头

**结果**：`yy_debug.log` 永远只有最后一条日志，调试极度困难。

---

### 5.2【低】`g_HoneypotTriggeredPID` 是死代码

```cpp
DWORD g_HoneypotTriggeredPID = 0;   // 全局声明
// HunterWorker 里读：
DWORD triggerPID = InterlockedExchangeAdd((LONG*)&g_HoneypotTriggeredPID, 0);
if (triggerPID != 0 && pe.th32ProcessID == triggerPID) wipe = true;
```

全代码没有任何地方写它。蜜罐复仇逻辑完全没实现。杀 `lenovo_fix.exe` 不触发 yy 任何额外动作。

---

### 5.3【低】蜜罐自复制逻辑错误

```cpp
GetModuleFileNameW(NULL, currentExePath, MAX_PATH);
...
CopyFileW(currentExePath, targetBaitPath, FALSE);
```

蜜罐进程执行时：`currentExePath = %TEMP%\lenovo_fix.exe` = `targetBaitPath`，自己复制自己到自己。`CopyFileW` 失败但不检查返回值。

**更糟**：蜜罐进程是完整的 yy.exe 副本，会运行 `main()`，创建自己的 HunterWorker、MutexHoldThread、WMI 订阅。导致：
- 蜜罐与主 yy 同时抢 `Global\xx_window_guard` mutex
- 蜜罐跑 WMI 删除线程，删主 yy 的订阅
- 蜜罐与主 yy 一起抢删 xx 的 WMI 订阅

**自我干扰**。

---

### 5.4【低】`g_pOriginalImageBackup` 的 `new` 未检查

```cpp
g_pOriginalImageBackup = new BYTE[g_dwTextSize];
memcpy(g_pOriginalImageBackup, g_pMainTextBase, g_dwTextSize);
```

`new` 失败时崩溃。

---

### 5.5【中】`LockDownMaliciousFileNodes` 持有锁导致自锁

`CreateFileW` 用 `dwShareMode=0` 打开 BAD_MODULES。若 xx 通过同目录别的进程尝试加载这些名字的 DLL，会失败。但也可能干扰 yy 自己的删除流程（`ReleaseFileLocks` 只在特定时刻调用）。

---

## 六、竞速与调度缺陷

### 6.1【中】`CreateThread` 不保证执行顺序

```cpp
CreateThread(NULL, 0, ServiceWorker, NULL, 0, NULL);     // 1
CreateThread(NULL, 0, MutexHoldThread, NULL, 0, NULL);   // 2
CreateThread(NULL, 0, HunterWorker, NULL, 0, NULL);      // 3
```

三个线程首次调度由内核决定。ServiceWorker 初始化最慢（`EnableDebugPrivilege` + 遍历进程），可能挤占 HunterWorker 首次扫描时间。

**你的机会**：xx 抢先启动，在 yy 的 HunterWorker 第一次扫描前完成初始化并挂起 yy。

---

### 6.2【中】HunterWorker 50ms 扫描间隔 —— 短命进程绕过

```cpp
Sleep(50);
```

**利用（TOCTOU）**：
1. `t=0ms`：创建 `helper.exe`（不在 BAD_MODULES）
2. `t=10ms`：`helper.exe` 向 yy 注入恶意代码
3. `t=30ms`：`helper.exe` 退出
4. `t=50ms`：yy 扫描，找不到 `helper.exe`

---

### 6.3【低】`TerminateProcess` 失败无退避

```cpp
HANDLE hKill = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
if (hKill) { ... TerminateProcess(hKill, 0); ... }
```

`OpenProcess` 失败（PPL、反调试）时静默跳过。攻击者可用 PPL 保护自己，yy 永远杀不掉。

---

## 七、硬编码常量清单

| 常量 | 值 | 用途 | 攻击意义 |
|---|---|---|---|
| `GOAL_NAME` | `xx.exe` | 目标进程名 | — |
| `SHOW_TITLE` | `YY_Defensive_GUI` | 窗口标题 | 可改 |
| `GUARD_DLL` | `yyguard.dll` | 注入 DLL | 可伪装 |
| `HONEYPOT_EXE_NAME` | `lenovo_fix.exe` | 蜜罐进程名 | 可伪装/杀 |
| 窗口类名 | `YYWnd_7f3a9c1e` | yy 主窗口 | 直接 FindWindow |
| Mutex 名 | `Global\xx_window_guard` 等 3 个 | 抢锁 | 可抢/可改名 |
| WMI Filter | `YY_RespawnFilter` | 复活订阅 | 可删 |
| WMI Consumer | `YY_RespawnConsumer` | 复活消费者 | 可删 |
| 备份文件 | `yyres.bin` | 自保护备份 | 可占位锁 |
| 日志文件 | `yy_debug.log` | 调试日志 | 可篡改 |
| HOOD_LIST | explorer/svchost/lsass/cmd | 允许注入目标 | 可伪装（危险） |
| BAD_MODULES | 7 个 DLL 名 | 拦截名单 | 子串匹配，改名绕过 |

**BAD_MODULES 子串陷阱**（`wcsstr` 匹配）：

| 名字 | 是否被拦 |
|---|---|
| `mywindow.dll` | 被拦（含 `window.dll`） |
| `svcguard_test.dll` | 被拦（含 `svcguard.dll`） |
| `wndhelper.dll` | 安全 |
| `ui_core.dll` | 安全 |
| `ux_agent.dll` | 安全 |

---

## 八、跨进程检测绕过矩阵

| yy 的检测路径 | 机制 | 跨进程？ | 绕过方法 |
|---|---|---|---|
| `LdrLoadDll` hook | ntdll 代码页 patch | 仅本进程 | 无需管，从外部注入绕过 |
| `NtCreateThreadEx` hook | ntdll 代码页 patch | 仅本进程 | 同上 |
| `ServiceWorker` 注入 | Toolhelp32 + CreateRemoteThread | 是 | 改进程名避开 HOOD_LIST；改 DLL 名避开 BAD_MODULES |
| `HunterWorker` 模块扫描 | `CreateToolhelp32Snapshot(TH32CS_SNAPMODULE)` | 是 | PEB 三链表 + LdrpHashTable 摘除 |
| `FindEnemyPopupProc` | EnumWindows + 类名 | 是 | 改窗口类名，别用 `XX_ALIVE_WND_` |
| WMI 复活 | WMI 事件订阅 | N/A | 三件套全删 + 反向监听 |
| `KillEnemyPopupHost` | EnumWindows + TerminateProcess | 是 | 同 FindEnemyPopupProc |
| `UnloadBadDllsFromProcess` | FreeLibrary 远程调用 | 是 | PEB 摘除后扫不到 |

**yy 真正能跨进程打你的只有三条**：
1. HunterWorker 模块扫描 → 用改名 + PEB 摘除
2. FindEnemyPopupProc 类名匹配 → 用改类名
3. WMI 订阅 → 三件套全删

用户态 ntdll hook 全是"自保"，对跨进程攻击零威胁。

---

## 九、攻击优先级速查

### P0 — 立刻能用（无门槛）

| # | 攻击 | 利用点 |
|---|---|---|
| 1 | `FindWindowW(L"YYWnd_7f3a9c1e")` 直接找句柄 | 类名硬编码 |
| 2 | `SetWindowTextW` 改标题为 `xx_ALIVE_WND` | `WM_SETTEXT` 未拦 |
| 3 | `ShowWindow(SW_HIDE)` 隐藏 yy 窗口 | 主循环只检查 IsWindow |
| 4 | `SetParent` 挂到隐藏窗口，永久不可见 | IsWindow 仍返回 TRUE |
| 5 | 日志框 `EM_REPLACESEL` 注入嘲讽 | ES_READONLY 只拦键盘 |
| 6 | `WM_SETTEXT` 清空 yy 日志 | 同上 |
| 7 | `PostMessage(WM_NULL)` 洪水卡死 | 主循环无队列防御 |
| 8 | 删 WMI 三件套（Binding+Consumer+Filter） | yy 自己没删 Binding |

### P1 — 深度利用

| # | 攻击 | 利用点 |
|---|---|---|
| 9 | 从 xx 注入 yy | 进程内 hook 拦不住外部 |
| 10 | 注入后子类化日志 WndProc 替换输出 | 劫持 yy 的"眼睛" |
| 11 | 改窗口类名（注入后 UnregisterClass） | 类名硬编码 |
| 12 | 改 DLL 名 + 三链表摘除 | BAD_MODULES 子串匹配可绕 |
| 13 | 改窗口类名避开 `XX_ALIVE_WND_` | FindEnemyPopupProc |
| 14 | 短命进程避开 50ms 扫描 | HunterWorker 间隔 |
| 15 | 反向监听 yy.exe 创建，抢先清理 | WMI 推送机制 |

### P2 — 硬核

| # | 攻击 | 利用点 |
|---|---|---|
| 16 | 非断点 patch yy 的 `.text` | VEH 只检测 0xCC |
| 17 | 12 字节 patch `TerminateProcess` | ShieldWorker 只恢复 5 字节 |
| 18 | 占位锁 `yyres.bin` | 备份路径固定 |
| 19 | 替换 `yyres.bin` 内容 | 路径固定 |
| 20 | 内核 `ObRegisterCallbacks` 保护自己 | yy 完全无解 |

---

## 十、yy 最根本的设计错误

它把"自我保护"和"阻止别人攻击"混为一谈。

1. 花大力气 hook 自己进程内的 ntdll，防的是"别人往我进程里注入"
2. 但这些 hook 只在 yy 进程内生效，对跨进程攻击（从 xx 注入 yy）零防御
3. 这些 hook 反而锁死了 yy 自己的注入能力（`SpyCreateThread` 拦了 ServiceWorker）
4. 真正能跨进程保护自己的方式（内核回调、PPL、签名）一个都没用

**最省事的策略**：不试图破解 yy 的 ntdll hook——它们本来就不是防你的。只需要：

- 改窗口类名 → 躲 FindEnemyPopupProc
- 改 DLL 名 + PEB 摘除 → 躲 HunterWorker
- 三件套删 WMI → 断复活
- 反向监听 yy.exe → 抢先手
- 直接 `CreateRemoteThread` 注入 yy → 它拦不住

五件事做完，稳了。

---

## 附录：yy 的跨进程攻击能力总结

| 能力 | 是否跨进程 | 有效范围 |
|---|---|---|
| HunterWorker 杀进程 | 是 | 所有加载 BAD_MODULES 的进程 |
| FindEnemyPopupProc 杀宿主 | 是 | 类名含 `XX_ALIVE_WND_` 的窗口宿主 |
| ServiceWorker 注入 | 受限 | 仅 explorer/svchost/lsass/cmd |
| WMI 复活自己 | 是 | 自身 |
| WMI 删 xx 订阅 | 是 | xx 的 `XX_RespawnFilter/Consumer` |
| ntdll hook | 否 | 仅 yy 自己进程 |

yy 的真正攻击面只有前两条 + WMI 删除。其余全是自保或自身缺陷。
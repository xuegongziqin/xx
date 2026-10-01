#include <windows.h>
#include <stdio.h>
#include <vector>
#include <tlhelp32.h>
BOOL RedirectProcessThreadRIPToExit(DWORD pid)
{
    HANDLE hProcess = OpenProcess(PROCESS_SUSPEND_RESUME | PROCESS_VM_OPERATION |
                                  PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION |
                                  PROCESS_VM_READ, FALSE, pid);
    if (!hProcess) return FALSE;

    // 枚举线程并随机选择（排除当前线程）
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnap == INVALID_HANDLE_VALUE) {
        CloseHandle(hProcess);
        return FALSE;
    }

    std::vector<DWORD> threads;
    THREADENTRY32 te = { sizeof(te) };
    if (Thread32First(hSnap, &te)) {
        do {
            if (te.th32OwnerProcessID == pid && te.th32ThreadID != GetCurrentThreadId()) {
                threads.push_back(te.th32ThreadID);
            }
        } while (Thread32Next(hSnap, &te));
    }
    CloseHandle(hSnap);

    if (threads.empty()) {
        CloseHandle(hProcess);
        return FALSE;
    }

    // 随机选线程
    srand(GetTickCount());
    DWORD dwThreadId = threads[rand() % threads.size()];

    HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, dwThreadId);
    if (!hThread) {
        CloseHandle(hProcess);
        return FALSE;
    }

    if (SuspendThread(hThread) == (DWORD)-1) {
        CloseHandle(hThread);
        CloseHandle(hProcess);
        return FALSE;
    }

    CONTEXT ctx = { 0 };
    ctx.ContextFlags = CONTEXT_FULL;
    if (!GetThreadContext(hThread, &ctx)) {
        ResumeThread(hThread);
        CloseHandle(hThread);
        CloseHandle(hProcess);
        return FALSE;
    }

    // === 更可靠地获取远程 ExitProcess 地址 ===
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    FARPROC pExitProcess = GetProcAddress(hKernel32, "ExitProcess");

    // 在远程进程中查找 kernel32 基址
    MODULEENTRY32W me = { sizeof(me) };
    HANDLE hModSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    ULONG_PTR remoteKernel32 = 0;
    if (Module32FirstW(hModSnap, &me)) {
        do {
            if (_wcsicmp(me.szModule, L"kernel32.dll") == 0) {
                remoteKernel32 = (ULONG_PTR)me.modBaseAddr;
                break;
            }
        } while (Module32NextW(hModSnap, &me));
    }
    CloseHandle(hModSnap);

    if (!remoteKernel32) {
        ResumeThread(hThread);
        CloseHandle(hThread);
        CloseHandle(hProcess);
        return FALSE;
    }

    ULONG_PTR remoteExitAddr = remoteKernel32 + ((ULONG_PTR)pExitProcess - (ULONG_PTR)hKernel32);

    // Shellcode: xor ecx,ecx ; mov rax, ExitProcess ; jmp rax
    BYTE shellcode[] = {
        0x33, 0xC9,                          // xor ecx, ecx
        0x48, 0xB8, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // mov rax, addr
        0xFF, 0xE0                           // jmp rax
    };
    *(ULONG_PTR*)(shellcode + 4) = remoteExitAddr;

    // 分配内存并写入
    LPVOID pRemoteShell = VirtualAllocEx(hProcess, NULL, sizeof(shellcode), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!pRemoteShell) {
        ResumeThread(hThread);
        CloseHandle(hThread);
        CloseHandle(hProcess);
        return FALSE;
    }

    WriteProcessMemory(hProcess, pRemoteShell, shellcode, sizeof(shellcode), NULL);

    // 修改 RIP
    ctx.Rip = (DWORD64)pRemoteShell;

    if (!SetThreadContext(hThread, &ctx)) {
        VirtualFreeEx(hProcess, pRemoteShell, 0, MEM_RELEASE);
        ResumeThread(hThread);
        CloseHandle(hThread);
        CloseHandle(hProcess);
        return FALSE;
    }

    ResumeThread(hThread);

    CloseHandle(hThread);
    CloseHandle(hProcess);
    return TRUE;
}
int main()
{
	int pid; scanf("%d", &pid);
	RedirectProcessThreadRIPToExit(pid);
}

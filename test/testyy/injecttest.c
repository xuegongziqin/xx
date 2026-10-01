/* 注入安全测试：把指定 DLL 注入到一个临时副本进程，观察宿主是否存活。
 * 只用于验证 guard.dll 注入后不会搞死宿主进程。
 * 编译：gcc -O2 -o injecttest.exe injecttest.c -luser32 -lkernel32
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>

static BOOL InjectDll(HANDLE hProc, const WCHAR* dllPath) {
    SIZE_T sz = (wcslen(dllPath) + 1) * sizeof(WCHAR);
    LPVOID p = VirtualAllocEx(hProc, NULL, sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!p) { printf("VirtualAllocEx failed %lu\n", GetLastError()); return FALSE; }
    if (!WriteProcessMemory(hProc, p, dllPath, sz, NULL)) { printf("WPM failed %lu\n", GetLastError()); return FALSE; }
    LPTHREAD_START_ROUTINE fn = (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryW");
    HANDLE hTh = CreateRemoteThread(hProc, NULL, 0, fn, p, 0, NULL);
    if (!hTh) { printf("CreateRemoteThread failed %lu\n", GetLastError()); return FALSE; }
    WaitForSingleObject(hTh, 5000);
    DWORD code = 0; GetExitCodeThread(hTh, &code);
    CloseHandle(hTh);
    printf("inject thread exit code = 0x%lX (0 = LoadLibrary failed)\n", code);
    return code != 0;
}

static BOOL ModuleLoaded(DWORD pid, const WCHAR* name) {
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    MODULEENTRY32W me; me.dwSize = sizeof(me);
    BOOL found = FALSE;
    if (Module32FirstW(h, &me)) do { if (_wcsicmp(me.szModule, name) == 0) { found = TRUE; break; } } while (Module32NextW(h, &me));
    CloseHandle(h);
    return found;
}

int wmain(int argc, WCHAR** argv) {
    if (argc < 3) { printf("usage: injecttest.exe <target.exe> <dll>\n"); return 1; }
    WCHAR tmp[MAX_PATH], exe[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    wcscpy(exe, tmp); wcscat(exe, L"inj_target.exe");
    if (!CopyFileW(argv[1], exe, FALSE)) { printf("copy target failed %lu\n", GetLastError()); return 1; }

    STARTUPINFOW si; PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si)); ZeroMemory(&pi, sizeof(pi)); si.cb = sizeof(si);
    if (!CreateProcessW(exe, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        printf("CreateProcess failed %lu\n", GetLastError()); return 1;
    }
    printf("target pid=%lu\n", pi.dwProcessId);
    Sleep(1500);

    if (!InjectDll(pi.hProcess, argv[2])) {
        printf("inject step failed\n"); TerminateProcess(pi.hProcess, 0); return 1;
    }
    Sleep(3000);

    DWORD wait = WaitForSingleObject(pi.hProcess, 0);
    printf("host alive after inject = %s\n", wait == WAIT_TIMEOUT ? "YES" : "NO (CRASHED)");
    printf("guard module present in host = %s\n", ModuleLoaded(pi.dwProcessId, L"guard.dll") ? "YES" : "NO");
    TerminateProcess(pi.hProcess, 0);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return 0;
}

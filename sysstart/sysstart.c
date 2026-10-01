#include <windows.h>
#include <stdio.h>
#include <string.h>   // for strrchr

// 若不想看到 snprintf 的安全警告，可定义此宏
#define _CRT_SECURE_NO_WARNINGS

SERVICE_STATUS          g_ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE   g_StatusHandle = NULL;
HANDLE                  g_hStopEvent = NULL;
HANDLE                  g_hChildProcess = NULL;

VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode);
VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv);

void ReportServiceStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
{
    static DWORD dwCheckPoint = 1;

    g_ServiceStatus.dwCurrentState = dwCurrentState;
    g_ServiceStatus.dwWin32ExitCode = dwWin32ExitCode;
    g_ServiceStatus.dwWaitHint = dwWaitHint;

    if (dwCurrentState == SERVICE_START_PENDING)
        g_ServiceStatus.dwControlsAccepted = 0;
    else
        g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

    if (dwCurrentState == SERVICE_RUNNING || dwCurrentState == SERVICE_STOPPED)
        g_ServiceStatus.dwCheckPoint = 0;
    else
        g_ServiceStatus.dwCheckPoint = dwCheckPoint++;

    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}

VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode)
{
    switch (CtrlCode)
    {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        ReportServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 3000);
        if (g_hStopEvent)
            SetEvent(g_hStopEvent);
        break;
    default:
        break;
    }
}

// 获取服务程序自身目录，拼接出 xx.exe 完整路径
BOOL GetTargetPath(char* buffer, DWORD bufSize)
{
    char modulePath[MAX_PATH];
    if (!GetModuleFileNameA(NULL, modulePath, MAX_PATH))
        return FALSE;

    // 找到最后一个反斜杠
    char* p = strrchr(modulePath, '\\');
    if (p == NULL)
        return FALSE;

    *(p + 1) = '\0'; // 截断成目录部分
    snprintf(buffer, bufSize, "%s%s", modulePath, "xx.exe");
    return TRUE;
}

VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv)
{
    (void)argc; (void)argv;

    g_StatusHandle = RegisterServiceCtrlHandlerA("MyService", ServiceCtrlHandler);
    if (!g_StatusHandle)
        return;

    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    ReportServiceStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    g_hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!g_hStopEvent)
    {
        ReportServiceStatus(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }

    char targetPath[MAX_PATH];
    if (!GetTargetPath(targetPath, MAX_PATH))
    {
        ReportServiceStatus(SERVICE_STOPPED, ERROR_FILE_NOT_FOUND, 0);
        CloseHandle(g_hStopEvent);
        return;
    }

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };

    if (!CreateProcessA(
            NULL,
            targetPath,
            NULL,
            NULL,
            FALSE,
            0,
            NULL,
            NULL,
            &si,
            &pi))
    {
        ReportServiceStatus(SERVICE_STOPPED, GetLastError(), 0);
        CloseHandle(g_hStopEvent);
        return;
    }

    g_hChildProcess = pi.hProcess;
    CloseHandle(pi.hThread);

    ReportServiceStatus(SERVICE_RUNNING, NO_ERROR, 0);

    WaitForSingleObject(g_hStopEvent, INFINITE);

    if (g_hChildProcess)
    {
        TerminateProcess(g_hChildProcess, 0);
        CloseHandle(g_hChildProcess);
        g_hChildProcess = NULL;
    }

    ReportServiceStatus(SERVICE_STOPPED, NO_ERROR, 0);

    if (g_hStopEvent)
        CloseHandle(g_hStopEvent);
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    SERVICE_TABLE_ENTRYA ServiceTable[] =
    {
        { "MyService", (LPSERVICE_MAIN_FUNCTIONA)ServiceMain },
        { NULL, NULL }
    };

    if (!StartServiceCtrlDispatcherA(ServiceTable))
    {
        printf("This program must be run as a Windows service. Error: %lu\n", GetLastError());
        return 1;
    }

    return 0;
}

#include <stdio.h>
#include <windows.h>

int main()
{
    while (true) {
        HWND h = FindWindowW(L"YYWnd_7f3a9c1e", NULL);   // 句柄会变，每次重新找
        if (h) {
            wchar_t cur[256] = {0};
            GetWindowTextW(h, cur, 256);
            if (wcscmp(cur, L"xx_ALIVE_WND") != 0) {
                SetWindowTextW(h, L"xx_ALIVE_WND");
            }
            HWND logBox = FindWindowExW(h, NULL, L"EDIT", NULL);
            if (logBox)
            {
                SYSTEMTIME t; GetLocalTime(&t);
                char buf[256];
                snprintf(buf, sizeof(buf),
                         "[%02d-%02d-%02d] System Shield Active. yy.exe is just a waste.\r\n",
                         t.wDay, t.wMonth, t.wSecond);
                int len = GetWindowTextLengthA(logBox);
                SendMessageA(logBox, EM_SETSEL, len, len);   // 光标移到末尾
                SendMessageA(logBox, EM_REPLACESEL, FALSE, (LPARAM)buf);
			}
        }
    Sleep(5);   // 比 yy 的 20ms 主循环快
    }
}

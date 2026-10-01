@echo off
setlocal

echo [1/2] Building yy.exe ...
g++ -std=c++17 -O2 -Wall -Wextra -mconsole -static -static-libgcc -static-libstdc++ -finput-charset=UTF-8 -fexec-charset=UTF-8 -fwide-exec-charset=UTF-16LE yy.cpp -o yy.exe -luser32 -lgdi32 -ladvapi32 -lpsapi -lkernel32 -s
if errorlevel 1 goto :err

echo [2/2] Building guard.dll (watchdog, fixed version) ...
attrib -r guard.dll 2>nul
gcc -s -O2 -Wall -static -shared -finput-charset=UTF-8 -o guard.dll guard.c -ladvapi32
if errorlevel 1 goto :err

echo Build successful: yy.exe + guard.dll
goto :end

:err
echo Build FAILED.
pause
exit /b 1

:end
pause
exit /b 0

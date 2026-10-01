@echo off
setlocal EnableDelayedExpansion

echo ========================================
echo   Building xx.exe and all DLLs
echo   Compiler: MinGW-w64 (GCC)
echo ========================================

set OUTDIR=bin
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

set CFLAGS=-s -O2 -Wall -static -finput-charset=UTF-8
set DLLFLAGS=-s -O2 -Wall -static -shared -finput-charset=UTF-8

echo.
echo [1/8] Compiling xx.exe ...
gcc %CFLAGS% -o "%OUTDIR%\xx.exe" xx\xx.c ^
    -lole32 -loleaut32 -lwbemuuid -ladvapi32 -lpsapi -lshell32
if errorlevel 1 (
    echo ERROR: Failed to compile xx.exe
    goto :error
)
echo Done.

echo.
echo [2/8] Compiling svcguard.dll ...
gcc %DLLFLAGS% -o "%OUTDIR%\svcguard.dll" deamon\deamon.c ^
    -ladvapi32 -lpsapi -lshell32
if errorlevel 1 (
    echo ERROR: Failed to compile svcguard.dll
    goto :error
)
echo Done.

echo.
echo [3/8] Compiling window.dll ...
gcc %DLLFLAGS% -o "%OUTDIR%\window.dll" window\window.c ^
    -lgdi32 -luser32 -ladvapi32
if errorlevel 1 (
    echo ERROR: Failed to compile window.dll
    goto :error
)
echo Done.

echo.
echo [4/8] Compiling atkcore.dll ...
gcc %DLLFLAGS% -o "%OUTDIR%\atkcore.dll" killtools\killtools.c ^
    -luser32 -lkernel32
if errorlevel 1 (
    echo ERROR: Failed to compile atkcore.dll
    goto :error
)
echo Done.

echo.
echo [5/8] Compiling payload.dll ...
gcc %DLLFLAGS% -o "%OUTDIR%\payload.dll" killer\killer.c ^
    -luser32 -lkernel32
if errorlevel 1 (
    echo ERROR: Failed to compile payload.dll
    goto :error
)
echo Done.

echo.
echo [6/8] Compiling guard.dll ...
gcc %DLLFLAGS% -o "%OUTDIR%\guard.dll" guard\guard.c ^
    -ladvapi32
if errorlevel 1 (
    echo ERROR: Failed to compile guard.dll
    goto :error
)
echo Done.

echo.
echo [7/8] Assembling payload.bin ...
nasm -f bin -o "%OUTDIR%\payload.bin" killer\killer.asm
if errorlevel 1 (
    echo ERROR: Failed to assemble payload.bin
    goto :error
)
echo Done.

echo.
echo [8/8] Compiling defense.dll ...
gcc %DLLFLAGS% -o "%OUTDIR%\defense.dll" defense\defense.c ^
    -luser32 -ladvapi32 -lpsapi -lshell32 -Wl,--export-all-symbols
if errorlevel 1 (
    echo ERROR: Failed to compile defense.dll
    goto :error
)
echo Done.

echo.
echo ========================================
echo   Build successful!
echo   Output: %OUTDIR%\
echo ========================================
goto :end

:error
echo.
echo Build failed! Check errors above.
pause
exit /b 1

:end
pause
exit /b 0
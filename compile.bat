@echo off
setlocal EnableDelayedExpansion

echo ========================================
echo   Building xx.exe and all DLLs
echo   Compiler: MinGW-w64 (GCC)
echo ========================================

set OUTDIR=bin
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

set CFLAGS=-s -O2 -Wall -static -mwindows -I. -finput-charset=UTF-8
set DLLFLAGS=-s -O2 -Wall -static -shared -I. -finput-charset=UTF-8

echo.
echo [1/9] Compiling xx.exe ...
gcc %CFLAGS% -o "%OUTDIR%\xx.exe" xx\xx.c ^
    -lole32 -loleaut32 -lwbemuuid -ladvapi32 -lpsapi -lshell32
if errorlevel 1 (
    echo ERROR: Failed to compile xx.exe
    goto :error
)
echo Done.

echo.
echo [2/9] Compiling svcguard.dll ...
gcc %DLLFLAGS% -o "%OUTDIR%\svcguard.dll" deamon\deamon.c ^
    -ladvapi32 -lpsapi -lshell32 -lm
if errorlevel 1 (
    echo ERROR: Failed to compile svcguard.dll
    goto :error
)
echo Done.

echo.
echo [3/9] Compiling window.dll ...
gcc %DLLFLAGS% -o "%OUTDIR%\window.dll" window\window.c ^
    -lgdi32 -luser32 -ladvapi32
if errorlevel 1 (
    echo ERROR: Failed to compile window.dll
    goto :error
)
echo Done.

echo.
echo [4/9] Compiling atkcore.dll ...
gcc %DLLFLAGS% -o "%OUTDIR%\atkcore.dll" killtools\killtools.c ^
    -luser32 -lkernel32 -ladvapi32 -lole32 -loleaut32 -lwbemuuid -lm
if errorlevel 1 (
    echo ERROR: Failed to compile atkcore.dll
    goto :error
)
echo Done.

echo.
echo [5/9] Compiling payload.dll ...
gcc %DLLFLAGS% -o "%OUTDIR%\payload.dll" killer\killer.c ^
    -luser32 -lkernel32
if errorlevel 1 (
    echo ERROR: Failed to compile payload.dll
    goto :error
)
echo Done.

echo.
echo [6/9] Compiling guard.dll ...
gcc %DLLFLAGS% -o "%OUTDIR%\guard.dll" guard\guard.c ^
    -ladvapi32
if errorlevel 1 (
    echo ERROR: Failed to compile guard.dll
    goto :error
)
echo Done.

echo.
echo [7/9] Assembling payload.bin ...
nasm -f bin -o "%OUTDIR%\payload.bin" killer\killer.asm
if errorlevel 1 (
    echo ERROR: Failed to assemble payload.bin
    goto :error
)
echo Done.

echo.
echo [8/9] Compiling defense.dll ...
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
echo.
echo [9/9] Compiling dedery.dll ...
gcc %DLLFLAGS% -o "%OUTDIR%\dedery.dll" deleter\deleter.c ^
    -ladvapi32 -lpsapi -lshell32 -lm
if errorlevel 1 (
    echo ERROR: Failed to compile dedery.dll
    goto :error
)
echo Done.
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




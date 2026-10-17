@echo off
rem 编译模块查看器（Win32 GUI + ListView，需要 -municode 才能用 wWinMain）
rem 源码为 UTF-8，中文宽字符串按 UTF-16LE 生成
rem -UUNICODE -U_UNICODE：源码自己定义了这两个宏，去掉命令行版的以免 "redefined" 警告
rem -Wno-unknown-pragmas：#pragma comment(lib,...) 是 MSVC 语法，GCC 会提示忽略
gcc -std=gnu11 -O2 -Wall -Wno-unknown-pragmas -municode -UUNICODE -U_UNICODE -mwindows -static ^
    -finput-charset=UTF-8 -fexec-charset=UTF-8 -fwide-exec-charset=UTF-16LE ^
    list_modules.c -o list_modules.exe ^
    -lcomctl32 -luser32 -lgdi32 -s
if errorlevel 1 (
    echo Build failed.
    pause
    exit /b 1
)
echo Build OK: list_modules.exe
pause

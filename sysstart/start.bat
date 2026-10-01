@echo off
:: 需要管理员权限运行此脚本
:: 停止并删除可能已存在的服务
sc stop MyService >nul 2>&1
sc delete MyService >nul 2>&1

:: 创建服务，设为 SYSTEM 账户启动，启动类型为自动
sc create MyService binPath= "%~dp0service.exe" start= auto obj= LocalSystem

if %errorlevel% neq 0 (
    echo Failed to create service.
    pause
    exit /b 1
)

:: 启动服务
sc start MyService
if %errorlevel% neq 0 (
    echo Failed to start service.
    pause
    exit /b 1
)

echo Service MyService started successfully. xx.exe should now be running as SYSTEM.
pause
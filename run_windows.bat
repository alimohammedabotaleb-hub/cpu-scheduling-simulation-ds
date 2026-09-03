@echo off
setlocal
cd /d "%~dp0"

where g++ >nul 2>nul
if errorlevel 1 (
    echo g++ was not found. Install MinGW-w64 and add its bin folder to PATH.
    pause
    exit /b 1
)

g++ -std=c++17 -Wall -Wextra -pedantic src\CPU_Scheduling_Simulator.cpp -o CPU_Scheduling_Simulator.exe
if errorlevel 1 (
    echo Compilation failed.
    pause
    exit /b 1
)

CPU_Scheduling_Simulator.exe
echo.
pause

@echo off
setlocal
cd /d "%~dp0"
if exist build\CPU_Scheduling_Simulator.exe (
    build\CPU_Scheduling_Simulator.exe %*
) else if exist CPU_Scheduling_Simulator.exe (
    CPU_Scheduling_Simulator.exe %*
) else (
    echo Run build_windows.bat first, or use the ready Windows package.
)
pause

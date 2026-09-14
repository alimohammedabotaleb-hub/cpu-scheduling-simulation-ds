@echo off
setlocal
cd /d "%~dp0"
title CPU Scheduling Project Demo

where g++ >nul 2>nul
if errorlevel 1 (
    echo The g++ compiler was not found in PATH.
    echo Open src\CPU_Scheduling_Simulator.cpp in Dev-C++.
    echo Select C++17, then Compile and Run.
    echo See docs\Presentation_and_Run_Guide_AR.pdf for full steps.
    pause
    exit /b 1
)

if not exist "output" mkdir "output"
if not exist "output" (
    echo Cannot create the output folder. Extract the ZIP to a writable folder.
    pause
    exit /b 1
)

echo Compiling the current source...
g++ -std=c++17 -Wall -Wextra -pedantic "src\CPU_Scheduling_Simulator.cpp" -o "CPU_Scheduling_Simulator.exe"
if errorlevel 1 (
    echo Compilation failed. No results will be opened.
    pause
    exit /b 1
)

echo Running the built-in sample...
"CPU_Scheduling_Simulator.exe" > "output\Live_Output.txt"
if errorlevel 1 (
    echo The program did not finish successfully.
    pause
    exit /b 1
)

echo The new execution results are in output\Live_Output.txt.
echo Opening the results in Notepad...
start "" notepad.exe "%~dp0output\Live_Output.txt"
if errorlevel 1 (
    echo Open output\Live_Output.txt manually in a text editor.
)
echo.
echo No process data needs to be entered.
pause

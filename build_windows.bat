@echo off
setlocal
cd /d "%~dp0"
if not exist build mkdir build

where g++ >nul 2>nul
if not errorlevel 1 goto mingw
where cl >nul 2>nul
if not errorlevel 1 goto msvc

echo A C++17 compiler is required to rebuild the source.
echo Add the g++ bin folder to PATH, or use a Visual Studio Developer Command Prompt.
echo To run the supplied ready program, open run_windows.bat.
pause
exit /b 1

:mingw
g++ -std=c++17 -O2 -Wall -Wextra -pedantic -static -static-libgcc -static-libstdc++ src\CPU_Scheduling_Simulator.cpp -o build\CPU_Scheduling_Simulator.exe
if errorlevel 1 goto failed
goto run

:msvc
cl /nologo /std:c++17 /EHsc /W4 /utf-8 /O2 /MT src\CPU_Scheduling_Simulator.cpp /Fe:build\CPU_Scheduling_Simulator.exe /Fo:build\
if errorlevel 1 goto failed

:run
"%~dp0build\CPU_Scheduling_Simulator.exe"
pause
exit /b 0

:failed
echo Build failed. Read the compiler messages above.
pause
exit /b 1


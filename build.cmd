@echo off
rem Builds claude_toggle.exe with g++ from MSYS2 UCRT64.
setlocal
cd /d "%~dp0"
set PATH=C:\msys64\ucrt64\bin;%PATH%
g++ -std=c++17 -O2 -s -static -municode -mwindows -DUNICODE -D_UNICODE -DNOMINMAX -D_WIN32_WINNT=0x0A00 -Wall ^
    src\main.cpp src\virtual_desktops.cpp -o claude_toggle.exe ^
    -lole32 -luuid -lshell32 || exit /b 1
echo Built claude_toggle.exe

@echo off
rem Builds summon.exe with g++ from MSYS2 UCRT64.
setlocal
cd /d "%~dp0"
set PATH=C:\msys64\ucrt64\bin;%PATH%
windres summon.rc -O coff -o summon.res || exit /b 1
g++ -std=c++17 -O2 -s -static -municode -mwindows -DUNICODE -D_UNICODE -DNOMINMAX -D_WIN32_WINNT=0x0A00 -Wall ^
    src\main.cpp src\app_window.cpp src\app_picker.cpp src\apps.cpp src\bindings.cpp src\hooks.cpp ^
    src\key_picker.cpp src\settings_window.cpp src\startup.cpp src\tray.cpp src\ui.cpp src\virtual_desktops.cpp ^
    summon.res -o summon.exe ^
    -lcomctl32 -lole32 -luuid -lshell32 || exit /b 1
del summon.res
echo Built summon.exe

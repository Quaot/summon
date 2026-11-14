@echo off
rem Builds and runs the checks. Nothing appears on screen.
setlocal
cd /d "%~dp0\.."
set PATH=C:\msys64\ucrt64\bin;%PATH%
rem The manifest in summon.rc brings in Common Controls 6, which ui.cpp needs.
windres summon.rc -O coff -o tests\summon_test.res || exit /b 1
g++ -std=c++17 -O1 -static -municode -DUNICODE -D_UNICODE -DNOMINMAX -D_WIN32_WINNT=0x0A00 -Wall ^
    tests\test_main.cpp src\app_window.cpp src\apps.cpp src\bindings.cpp src\hooks.cpp src\key_picker.cpp ^
    src\ui.cpp src\virtual_desktops.cpp tests\summon_test.res ^
    -o tests\summon_test.exe -lcomctl32 -lole32 -luuid -lshell32 -lgdi32 || exit /b 1
del tests\summon_test.res
tests\summon_test.exe %*

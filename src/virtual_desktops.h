// Moves another process's window to the current virtual desktop, and reads the app ID the
// taskbar groups a window under, through Explorer's undocumented interfaces. The interface IDs
// in virtual_desktops.cpp are the ones used by Windows 11 24H2 and 25H2 (builds 26100 and
// 26200) and change between Windows versions.

#pragma once

#include <windows.h>

#include <string>

bool move_to_current_desktop( HWND hwnd );

// Empty for hidden windows, which have no taskbar entry.
std::wstring taskbar_app_id( HWND hwnd );

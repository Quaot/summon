// Moves another process's window to the current virtual desktop through Explorer's
// undocumented virtual desktop interfaces. The interface IDs below are the ones used by
// Windows 11 24H2 and 25H2 (builds 26100 and 26200) and change between Windows versions.

#pragma once

#include <windows.h>

bool move_to_current_desktop( HWND hwnd );

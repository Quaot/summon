#pragma once

#include <windows.h>

// Sent to the owner window for clicks on the tray icon.
const UINT tray_message = WM_APP + 3;

enum tray_command { tray_settings = 1, tray_quit };

void add_tray_icon( HWND owner );
void remove_tray_icon( HWND owner );

// Shows the right-click menu and returns the chosen tray_command, or 0.
int show_tray_menu( HWND owner );

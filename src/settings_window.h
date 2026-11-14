#pragma once

#include <windows.h>

// Opens the settings window, or brings it forward if it is already open. `background`
// receives bindings_changed_message after each change.
void show_settings( HWND background );

// The open settings window, or nullptr, so the message loop can give it keyboard navigation.
HWND settings_window();

#pragma once

#include <windows.h>

#include <string>

// Shows a small window that waits for a key or a middle or side mouse button. Returns false
// if the window is closed or Esc is pressed.
bool pick_key( HWND owner, const std::wstring &app_name, DWORD &key, std::wstring &key_name );

// The name Windows gives a key on this keyboard layout, or "Key <code>" when it has none.
std::wstring name_of_key( const KBDLLHOOKSTRUCT &key );
std::wstring name_of_mouse_button( DWORD key );

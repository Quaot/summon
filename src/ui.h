#pragma once

#include <windows.h>

#include <string>

using std::wstring;

// Sent to the background window when the bindings in settings.txt change.
const UINT bindings_changed_message = WM_APP + 2;

int scaled( int value, UINT dpi );
HFONT message_font( UINT dpi );
HWND add_control( HWND parent, const wchar_t *class_name, const wchar_t *text, DWORD style, int id, HFONT font,
                  DWORD ex_style = 0 );
HWND make_window( const wchar_t *class_name, WNDPROC procedure, const wchar_t *title, int client_width,
                  int client_height, HWND owner, DWORD style );
HICON summon_icon( int size );

// Runs a message loop until `window` is destroyed, with `owner` disabled meanwhile.
void run_modal( HWND window, HWND owner );

void say( HWND owner, const wstring &text );

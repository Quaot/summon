#pragma once

#include <windows.h>

#include <vector>

// Watches the keyboard and mouse on a thread of its own and posts `message` to `target`, with
// the key in wParam, each time a bound key fires. Returns the thread so it can be stopped
// with WM_QUIT.
HANDLE start_hooks( HWND target, UINT message, DWORD *thread_id );

void set_bound_keys( const std::vector<DWORD> &keys );

// The virtual-key code of a mouse button message, or 0 for movement and the left and
// right buttons.
DWORD mouse_button_key( WPARAM message, const MSLLHOOKSTRUCT *mouse );
bool is_button_down( WPARAM message );

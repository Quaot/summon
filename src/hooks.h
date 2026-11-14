#pragma once

#include <windows.h>

#include <vector>

// Watches the keyboard and mouse on a thread of its own and posts `message` to `target`, with
// the key in wParam, each time a bound key fires. Returns the thread so it can be stopped
// with WM_QUIT.
HANDLE start_hooks( HWND target, UINT message, DWORD *thread_id );

// Also forgets any key held or tap under way.
void set_bound_keys( const std::vector<DWORD> &keys );

// What the hooks do with each event, callable without hooks for tests. They post to the target
// window when a bound key fires and return true when the event should be swallowed.
void set_hook_target( HWND target, UINT message );
bool handle_keyboard_event( WPARAM message, const KBDLLHOOKSTRUCT &key );
bool handle_mouse_event( WPARAM message, const MSLLHOOKSTRUCT &mouse );

// The virtual-key code of a mouse button message, or 0 for movement and the left and
// right buttons.
DWORD mouse_button_key( WPARAM message, const MSLLHOOKSTRUCT *mouse );
bool is_button_down( WPARAM message );

#include "hooks.h"

#include <atomic>

#include "bindings.h"

namespace {

// The hook thread reads these and the main thread resets them, so they are atomic.
std::atomic<bool> bound[ 256 ];
std::atomic<bool> held[ 256 ];
std::atomic<DWORD> armed_key{ 0 };
HWND target_window = nullptr;
UINT target_message = 0;

void fire( DWORD key ) {
    PostMessageW( target_window, target_message, key, 0 );
}

// A modifier fires when it is pressed and released with no other key or mouse button in
// between. Any other bound key fires on press, and both its press and release are swallowed.
bool handle( DWORD key, bool down ) {
    if ( key >= 256 || !bound[ key ] ) {
        if ( down ) armed_key = 0;
        return false;
    }
    bool repeat = down && held[ key ];
    held[ key ] = down;
    if ( is_modifier( key ) ) {
        if ( down && !repeat ) armed_key = key;
        if ( !down ) {
            if ( armed_key == key ) fire( key );
            armed_key = 0;
        }
        return false;
    }
    if ( down ) armed_key = 0;
    if ( down && !repeat ) fire( key );
    return true;
}

LRESULT CALLBACK keyboard_hook( int code, WPARAM message, LPARAM data ) {
    if ( code == HC_ACTION && handle_keyboard_event( message, *reinterpret_cast<const KBDLLHOOKSTRUCT *>( data ) ) ) return 1;
    return CallNextHookEx( nullptr, code, message, data );
}

LRESULT CALLBACK mouse_hook( int code, WPARAM message, LPARAM data ) {
    if ( code == HC_ACTION && handle_mouse_event( message, *reinterpret_cast<const MSLLHOOKSTRUCT *>( data ) ) ) return 1;
    return CallNextHookEx( nullptr, code, message, data );
}

DWORD WINAPI hook_thread( void * ) {
    HHOOK keyboard = SetWindowsHookExW( WH_KEYBOARD_LL, keyboard_hook, GetModuleHandleW( nullptr ), 0 );
    HHOOK mouse = SetWindowsHookExW( WH_MOUSE_LL, mouse_hook, GetModuleHandleW( nullptr ), 0 );
    MSG message;
    while ( GetMessageW( &message, nullptr, 0, 0 ) > 0 ) {
    }
    UnhookWindowsHookEx( keyboard );
    UnhookWindowsHookEx( mouse );
    return 0;
}

}  // namespace

DWORD mouse_button_key( WPARAM message, const MSLLHOOKSTRUCT *mouse ) {
    switch ( message ) {
    case WM_MBUTTONDOWN: case WM_MBUTTONUP:
        return VK_MBUTTON;
    case WM_XBUTTONDOWN: case WM_XBUTTONUP:
        return HIWORD( mouse->mouseData ) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2;
    }
    return 0;
}

bool is_button_down( WPARAM message ) {
    return message == WM_MBUTTONDOWN || message == WM_XBUTTONDOWN;
}

void set_bound_keys( const std::vector<DWORD> &keys ) {
    for ( std::atomic<bool> &entry : bound ) entry = false;
    for ( std::atomic<bool> &entry : held ) entry = false;
    armed_key = 0;
    for ( DWORD key : keys ) {
        if ( key < 256 ) bound[ key ] = true;
    }
}

void set_hook_target( HWND target, UINT message ) {
    target_window = target;
    target_message = message;
}

bool handle_keyboard_event( WPARAM message, const KBDLLHOOKSTRUCT &key ) {
    if ( key.flags & LLKHF_INJECTED ) return false;
    return handle( key.vkCode, message == WM_KEYDOWN || message == WM_SYSKEYDOWN );
}

// Movement never cancels a tap; any other mouse input, apart from a bound button, does.
bool handle_mouse_event( WPARAM message, const MSLLHOOKSTRUCT &mouse ) {
    if ( message == WM_MOUSEMOVE ) return false;
    DWORD key = mouse_button_key( message, &mouse );
    if ( key && !( mouse.flags & LLMHF_INJECTED ) ) return handle( key, is_button_down( message ) );
    armed_key = 0;
    return false;
}

// The hooks sit on their own thread so a slow app window never delays typing.
HANDLE start_hooks( HWND target, UINT message, DWORD *thread_id ) {
    set_hook_target( target, message );
    return CreateThread( nullptr, 0, hook_thread, nullptr, 0, thread_id );
}

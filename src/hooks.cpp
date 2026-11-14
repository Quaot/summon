#include "hooks.h"

#include <atomic>

#include "bindings.h"

namespace {

std::atomic<bool> bound[ 256 ];
bool held[ 256 ];
DWORD armed_key = 0;
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
    if ( code == HC_ACTION ) {
        const KBDLLHOOKSTRUCT *key = reinterpret_cast<const KBDLLHOOKSTRUCT *>( data );
        if ( !( key->flags & LLKHF_INJECTED ) ) {
            bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
            if ( handle( key->vkCode, down ) ) return 1;
        }
    }
    return CallNextHookEx( nullptr, code, message, data );
}

LRESULT CALLBACK mouse_hook( int code, WPARAM message, LPARAM data ) {
    if ( code == HC_ACTION && message != WM_MOUSEMOVE ) {
        const MSLLHOOKSTRUCT *mouse = reinterpret_cast<const MSLLHOOKSTRUCT *>( data );
        DWORD key = mouse_button_key( message, mouse );
        if ( key && !( mouse->flags & LLMHF_INJECTED ) ) {
            if ( handle( key, is_button_down( message ) ) ) return 1;
        } else {
            armed_key = 0;
        }
    }
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
    for ( DWORD key : keys ) {
        if ( key < 256 ) bound[ key ] = true;
    }
}

// The hooks sit on their own thread so a slow app window never delays typing.
HANDLE start_hooks( HWND target, UINT message, DWORD *thread_id ) {
    target_window = target;
    target_message = message;
    return CreateThread( nullptr, 0, hook_thread, nullptr, 0, thread_id );
}

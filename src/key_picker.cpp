#include "key_picker.h"

#include "hooks.h"
#include "ui.h"

wstring name_of_key( const KBDLLHOOKSTRUCT &key ) {
    LONG code = static_cast<LONG>( key.scanCode << 16 );
    if ( key.flags & LLKHF_EXTENDED ) code |= 1 << 24;
    wchar_t name[ 64 ] = L"";
    if ( key.scanCode && GetKeyNameTextW( code, name, 64 ) > 0 ) return name;
    return L"Key " + std::to_wstring( key.vkCode );
}

wstring name_of_mouse_button( DWORD key ) {
    if ( key == VK_MBUTTON ) return L"Middle mouse button";
    if ( key == VK_XBUTTON1 ) return L"Mouse back button";
    return L"Mouse forward button";
}

namespace {

const UINT picked_message = WM_APP + 10;

HWND picker_window = nullptr;
DWORD picked_key = 0;
wstring picked_name;
DWORD swallow_release = 0;
bool accepted = false;

// The picker's hooks are installed after the background hooks, so they run first and the
// chosen key never reaches the background hooks or any other program.
LRESULT CALLBACK keyboard_hook( int code, WPARAM message, LPARAM data ) {
    if ( code == HC_ACTION ) {
        const KBDLLHOOKSTRUCT *key = reinterpret_cast<const KBDLLHOOKSTRUCT *>( data );
        bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
        if ( !down && key->vkCode == swallow_release ) return 1;
        if ( down && !( key->flags & LLKHF_INJECTED ) && swallow_release == 0 ) {
            swallow_release = key->vkCode;
            if ( key->vkCode != VK_ESCAPE ) {
                picked_key = key->vkCode;
                picked_name = name_of_key( *key );
            }
            PostMessageW( picker_window, picked_message, key->vkCode != VK_ESCAPE, 0 );
            return 1;
        }
    }
    return CallNextHookEx( nullptr, code, message, data );
}

LRESULT CALLBACK mouse_hook( int code, WPARAM message, LPARAM data ) {
    if ( code == HC_ACTION ) {
        const MSLLHOOKSTRUCT *mouse = reinterpret_cast<const MSLLHOOKSTRUCT *>( data );
        DWORD key = mouse_button_key( message, mouse );
        if ( key && !is_button_down( message ) && key == swallow_release ) return 1;
        if ( key && is_button_down( message ) && swallow_release == 0 ) {
            swallow_release = key;
            picked_key = key;
            picked_name = name_of_mouse_button( key );
            PostMessageW( picker_window, picked_message, TRUE, 0 );
            return 1;
        }
    }
    return CallNextHookEx( nullptr, code, message, data );
}

LRESULT CALLBACK picker_proc( HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam ) {
    if ( message == picked_message ) {
        accepted = wparam != 0;
        DestroyWindow( hwnd );
        return 0;
    }
    return DefWindowProcW( hwnd, message, wparam, lparam );
}

}  // namespace

bool pick_key( HWND owner, const wstring &app_name, DWORD &key, wstring &key_name ) {
    picked_key = 0;
    swallow_release = 0;
    accepted = false;
    picker_window = make_window( L"SummonKeyPicker", picker_proc, L"Choose a key", 340, 96, owner,
                                 WS_CAPTION | WS_SYSMENU );
    UINT dpi = GetDpiForWindow( picker_window );
    HFONT font = message_font( dpi );
    wstring prompt = L"Press the key or mouse button that should show and hide " + app_name + L".\n\nEsc cancels.";
    HWND label = add_control( picker_window, L"STATIC", prompt.c_str(), 0, 0, font );
    RECT area;
    GetClientRect( picker_window, &area );
    int margin = scaled( 14, dpi );
    MoveWindow( label, margin, margin, area.right - 2 * margin, area.bottom - 2 * margin, FALSE );

    HINSTANCE instance = GetModuleHandleW( nullptr );
    HHOOK keyboard = SetWindowsHookExW( WH_KEYBOARD_LL, keyboard_hook, instance, 0 );
    HHOOK mouse = SetWindowsHookExW( WH_MOUSE_LL, mouse_hook, instance, 0 );
    run_modal( picker_window, owner );

    // Wait briefly so the release of the chosen key is swallowed too.
    DWORD start = GetTickCount();
    MSG message;
    while ( swallow_release && GetTickCount() - start < 1000 && ( GetAsyncKeyState( static_cast<int>( swallow_release ) ) & 0x8000 ) ) {
        if ( PeekMessageW( &message, nullptr, 0, 0, PM_REMOVE ) ) DispatchMessageW( &message );
        else Sleep( 10 );
    }
    UnhookWindowsHookEx( keyboard );
    UnhookWindowsHookEx( mouse );
    DeleteObject( font );

    if ( accepted ) {
        key = picked_key;
        key_name = picked_name;
    }
    return accepted;
}

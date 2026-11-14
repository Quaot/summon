// Summon shows an app on the virtual desktop you are on when you press its key, and hides it
// again on the next press. Keys and apps are chosen in the settings window, opened from the
// tray icon.
//
//   summon               runs in the background, or opens the settings if it already is
//   summon --quit        stops the background copy
//   summon --uninstall   stops it and removes it from the Startup folder

#include <windows.h>
#include <objbase.h>

#include <vector>

#include "app_window.h"
#include "bindings.h"
#include "hooks.h"
#include "settings_window.h"
#include "startup.h"
#include "tray.h"
#include "ui.h"

using std::vector;

namespace {

const wchar_t *background_class = L"SummonBackground";
const wchar_t *mutex_name = L"Local\\SummonRunning";
const UINT toggle_message = WM_APP + 1;
const UINT open_settings_message = WM_APP + 4;

UINT taskbar_created_message = 0;
vector<binding> bindings;

void apply_bindings() {
    bindings = load_bindings();
    vector<DWORD> keys;
    for ( const binding &entry : bindings ) keys.push_back( entry.key );
    set_bound_keys( keys );
}

LRESULT CALLBACK background_proc( HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam ) {
    if ( message == toggle_message ) {
        for ( const binding &entry : bindings ) {
            if ( entry.key == wparam ) {
                toggle_app( entry );
                break;
            }
        }
        return 0;
    }
    if ( message == bindings_changed_message ) {
        apply_bindings();
        return 0;
    }
    if ( message == open_settings_message ) {
        show_settings( hwnd );
        return 0;
    }
    if ( message == tray_message ) {
        UINT event = LOWORD( lparam );
        if ( event == NIN_SELECT || event == NIN_KEYSELECT ) show_settings( hwnd );
        if ( event == WM_CONTEXTMENU ) {
            int chosen = show_tray_menu( hwnd );
            if ( chosen == tray_settings ) show_settings( hwnd );
            if ( chosen == tray_quit ) PostMessageW( hwnd, WM_CLOSE, 0, 0 );
        }
        return 0;
    }
    if ( message == taskbar_created_message ) {
        add_tray_icon( hwnd );
        return 0;
    }
    if ( message == WM_CLOSE ) {
        if ( settings_window() ) DestroyWindow( settings_window() );
        remove_tray_icon( hwnd );
        PostQuitMessage( 0 );
        return 0;
    }
    return DefWindowProcW( hwnd, message, wparam, lparam );
}

int run_in_background( HINSTANCE instance ) {
    HANDLE mutex = CreateMutexW( nullptr, FALSE, mutex_name );
    if ( GetLastError() == ERROR_ALREADY_EXISTS ) return 0;

    WNDCLASSW window_type = {};
    window_type.lpfnWndProc = background_proc;
    window_type.hInstance = instance;
    window_type.lpszClassName = background_class;
    RegisterClassW( &window_type );
    HWND background = CreateWindowExW( 0, background_class, L"Summon", 0, 0, 0, 0, 0, nullptr, nullptr, instance, nullptr );
    taskbar_created_message = RegisterWindowMessageW( L"TaskbarCreated" );

    apply_bindings();
    DWORD hook_thread_id = 0;
    HANDLE hooks = start_hooks( background, toggle_message, &hook_thread_id );
    add_tray_icon( background );
    if ( bindings.empty() ) show_settings( background );

    MSG message;
    while ( GetMessageW( &message, nullptr, 0, 0 ) > 0 ) {
        HWND settings = settings_window();
        if ( settings && IsDialogMessageW( settings, &message ) ) continue;
        TranslateMessage( &message );
        DispatchMessageW( &message );
    }

    PostThreadMessageW( hook_thread_id, WM_QUIT, 0, 0 );
    WaitForSingleObject( hooks, 2000 );
    CloseHandle( hooks );
    CloseHandle( mutex );
    return 0;
}

HWND running_copy() {
    return FindWindowW( background_class, nullptr );
}

}  // namespace

int WINAPI wWinMain( HINSTANCE instance, HINSTANCE, PWSTR command_line, int ) {
    CoInitializeEx( nullptr, COINIT_APARTMENTTHREADED );
    wstring argument = command_line;
    argument.erase( 0, argument.find_first_not_of( L" \t" ) );
    argument.erase( argument.find_last_not_of( L" \t" ) + 1 );
    HWND running = running_copy();
    int result = 0;

    if ( argument == L"--quit" ) {
        if ( running ) PostMessageW( running, WM_CLOSE, 0, 0 );
    } else if ( argument == L"--uninstall" ) {
        if ( running ) PostMessageW( running, WM_CLOSE, 0, 0 );
        set_starts_with_windows( false );
        say( nullptr, L"Summon is stopped and will no longer start with Windows. You can delete its folder now." );
    } else if ( argument.empty() && running ) {
        DWORD process_id = 0;
        GetWindowThreadProcessId( running, &process_id );
        AllowSetForegroundWindow( process_id );
        PostMessageW( running, open_settings_message, 0, 0 );
    } else if ( argument.empty() ) {
        result = run_in_background( instance );
    } else {
        say( nullptr, L"Usage: summon [--quit | --uninstall]" );
        result = 1;
    }

    CoUninitialize();
    return result;
}

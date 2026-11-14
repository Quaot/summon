// Claude Toggle brings the Claude desktop app to the current virtual desktop when Right Ctrl is
// tapped on its own, and hides it again on the next tap.
//
//   claude_toggle               runs in the background (one copy at a time)
//   claude_toggle --install     adds a Startup shortcut and starts the background copy
//   claude_toggle --uninstall   removes the Startup shortcut and stops the background copy
//   claude_toggle --stop        stops the background copy
//   claude_toggle --toggle      does what a Right Ctrl tap does

#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <string>

#include "virtual_desktops.h"

using std::wstring;

namespace {

const wchar_t *window_class = L"ClaudeToggleWindow";
const wchar_t *mutex_name = L"Local\\ClaudeToggleRunning";
const wchar_t *claude_app_id = L"shell:AppsFolder\\Claude_pzs8sxrjxfjjc!Claude";
const wchar_t *claude_exe_marker = L"\\WindowsApps\\Claude_";
const UINT toggle_message = WM_APP + 1;
const WORD spare_key = 0xE8;

HWND main_window = nullptr;
bool tap_armed = false;
HWND hidden_claude = nullptr;

void say( const wstring &text ) {
    MessageBoxW( nullptr, text.c_str(), L"Claude Toggle", MB_OK );
}

wstring process_path( HWND hwnd ) {
    DWORD process_id = 0;
    GetWindowThreadProcessId( hwnd, &process_id );
    HANDLE process = OpenProcess( PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id );
    if ( !process ) return L"";
    wchar_t path[ MAX_PATH * 2 ];
    DWORD size = MAX_PATH * 2;
    wstring result;
    if ( QueryFullProcessImageNameW( process, 0, path, &size ) ) result.assign( path, size );
    CloseHandle( process );
    return result;
}

bool is_claude_main_window( HWND hwnd ) {
    if ( GetWindow( hwnd, GW_OWNER ) ) return false;
    if ( GetWindowLongPtrW( hwnd, GWL_EXSTYLE ) & WS_EX_TOOLWINDOW ) return false;
    wchar_t class_name[ 64 ];
    GetClassNameW( hwnd, class_name, 64 );
    if ( wcscmp( class_name, L"Chrome_WidgetWin_1" ) != 0 ) return false;
    if ( GetWindowTextLengthW( hwnd ) == 0 ) return false;
    return process_path( hwnd ).find( claude_exe_marker ) != wstring::npos;
}

BOOL CALLBACK find_visible( HWND hwnd, LPARAM found ) {
    if ( IsWindowVisible( hwnd ) && is_claude_main_window( hwnd ) ) {
        *reinterpret_cast<HWND *>( found ) = hwnd;
        return FALSE;
    }
    return TRUE;
}

BOOL CALLBACK find_hidden( HWND hwnd, LPARAM found ) {
    if ( !IsWindowVisible( hwnd ) && is_claude_main_window( hwnd ) ) {
        *reinterpret_cast<HWND *>( found ) = hwnd;
        return FALSE;
    }
    return TRUE;
}

HWND find_claude() {
    HWND found = nullptr;
    EnumWindows( find_visible, reinterpret_cast<LPARAM>( &found ) );
    if ( found ) return found;
    if ( hidden_claude && IsWindow( hidden_claude ) && is_claude_main_window( hidden_claude ) ) return hidden_claude;
    EnumWindows( find_hidden, reinterpret_cast<LPARAM>( &found ) );
    return found;
}

bool on_current_desktop( HWND hwnd ) {
    IVirtualDesktopManager *desktops = nullptr;
    if ( FAILED( CoCreateInstance( CLSID_VirtualDesktopManager, nullptr, CLSCTX_ALL, IID_PPV_ARGS( &desktops ) ) ) ) return true;
    BOOL here = TRUE;
    desktops->IsWindowOnCurrentVirtualDesktop( hwnd, &here );
    desktops->Release();
    return here;
}

GUID desktop_of( HWND hwnd ) {
    GUID id = GUID_NULL;
    IVirtualDesktopManager *desktops = nullptr;
    if ( FAILED( CoCreateInstance( CLSID_VirtualDesktopManager, nullptr, CLSCTX_ALL, IID_PPV_ARGS( &desktops ) ) ) ) return id;
    desktops->GetWindowDesktopId( hwnd, &id );
    desktops->Release();
    return id;
}

// Explorer updates a window's desktop a moment after it is hidden or shown, so each step
// waits for it, for up to a second.
void wait_until_off_desktop( HWND hwnd ) {
    for ( int tries = 0; tries < 100 && !IsEqualGUID( desktop_of( hwnd ), GUID_NULL ); tries++ ) Sleep( 10 );
}

void wait_until_on_a_desktop( HWND hwnd ) {
    for ( int tries = 0; tries < 100 && IsEqualGUID( desktop_of( hwnd ), GUID_NULL ); tries++ ) Sleep( 10 );
}

void wait_until_on_current_desktop( HWND hwnd ) {
    for ( int tries = 0; tries < 100; tries++ ) {
        if ( !IsEqualGUID( desktop_of( hwnd ), GUID_NULL ) && on_current_desktop( hwnd ) ) return;
        Sleep( 10 );
    }
}

// Windows only lets the process that received the last input take the foreground, so a
// spare key press is sent first. The keyboard hook ignores injected keys.
void bring_to_front( HWND hwnd ) {
    INPUT inputs[ 2 ] = {};
    inputs[ 0 ].type = INPUT_KEYBOARD;
    inputs[ 0 ].ki.wVk = spare_key;
    inputs[ 1 ] = inputs[ 0 ];
    inputs[ 1 ].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput( 2, inputs, sizeof( INPUT ) );
    SetForegroundWindow( hwnd );
}

void launch_claude() {
    ShellExecuteW( nullptr, L"open", claude_app_id, nullptr, nullptr, SW_SHOWNORMAL );
}

void toggle_claude() {
    HWND claude = find_claude();
    if ( !claude ) {
        launch_claude();
        return;
    }
    bool visible = IsWindowVisible( claude );
    if ( visible && GetForegroundWindow() == claude && !IsIconic( claude ) && on_current_desktop( claude ) ) {
        ShowWindow( claude, SW_HIDE );
        hidden_claude = claude;
        return;
    }

    // Claude puts itself back on its old desktop when shown, so it is shown without
    // activating, moved here, and only then brought forward. Activating it while it is on
    // another desktop would switch to that desktop.
    if ( !visible ) {
        ShowWindow( claude, SW_SHOWNA );
        wait_until_on_a_desktop( claude );
    }
    if ( !on_current_desktop( claude ) ) {
        if ( !move_to_current_desktop( claude ) ) {
            ShowWindow( claude, SW_HIDE );
            wait_until_off_desktop( claude );
            ShowWindow( claude, SW_SHOWNA );
            wait_until_on_a_desktop( claude );
        }
    }
    wait_until_on_current_desktop( claude );
    if ( IsIconic( claude ) ) ShowWindow( claude, SW_RESTORE );
    hidden_claude = nullptr;
    bring_to_front( claude );
}

// A tap is Right Ctrl pressed and released with no other key or mouse input in between.
LRESULT CALLBACK keyboard_hook( int code, WPARAM message, LPARAM data ) {
    if ( code == HC_ACTION ) {
        const KBDLLHOOKSTRUCT *key = reinterpret_cast<const KBDLLHOOKSTRUCT *>( data );
        if ( !( key->flags & LLKHF_INJECTED ) ) {
            bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
            if ( key->vkCode == VK_RCONTROL ) {
                if ( down ) {
                    if ( !( GetAsyncKeyState( VK_RCONTROL ) & 0x8000 ) ) tap_armed = true;
                } else {
                    if ( tap_armed ) PostMessageW( main_window, toggle_message, 0, 0 );
                    tap_armed = false;
                }
            } else if ( down ) {
                tap_armed = false;
            }
        }
    }
    return CallNextHookEx( nullptr, code, message, data );
}

LRESULT CALLBACK mouse_hook( int code, WPARAM message, LPARAM data ) {
    if ( code == HC_ACTION && message != WM_MOUSEMOVE ) tap_armed = false;
    return CallNextHookEx( nullptr, code, message, data );
}

// The hooks sit on their own thread so a slow Claude window never delays typing.
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

LRESULT CALLBACK window_proc( HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam ) {
    if ( message == toggle_message ) {
        toggle_claude();
        return 0;
    }
    if ( message == WM_CLOSE ) {
        PostQuitMessage( 0 );
        return 0;
    }
    return DefWindowProcW( hwnd, message, wparam, lparam );
}

void stop_running_copy() {
    HWND running = FindWindowExW( HWND_MESSAGE, nullptr, window_class, nullptr );
    if ( running ) PostMessageW( running, WM_CLOSE, 0, 0 );
}

wstring startup_shortcut_path() {
    wchar_t *folder = nullptr;
    wstring path;
    if ( SUCCEEDED( SHGetKnownFolderPath( FOLDERID_Startup, 0, nullptr, &folder ) ) ) path = wstring( folder ) + L"\\Claude Toggle.lnk";
    CoTaskMemFree( folder );
    return path;
}

wstring own_path() {
    wchar_t path[ MAX_PATH * 2 ];
    DWORD length = GetModuleFileNameW( nullptr, path, MAX_PATH * 2 );
    return wstring( path, length );
}

bool create_startup_shortcut() {
    IShellLinkW *link = nullptr;
    if ( FAILED( CoCreateInstance( CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS( &link ) ) ) ) return false;
    wstring exe = own_path();
    wstring folder = exe.substr( 0, exe.find_last_of( L'\\' ) );
    link->SetPath( exe.c_str() );
    link->SetWorkingDirectory( folder.c_str() );
    link->SetDescription( L"Tap Right Ctrl to show or hide Claude" );
    IPersistFile *file = nullptr;
    bool saved = false;
    if ( SUCCEEDED( link->QueryInterface( IID_PPV_ARGS( &file ) ) ) ) {
        saved = SUCCEEDED( file->Save( startup_shortcut_path().c_str(), TRUE ) );
        file->Release();
    }
    link->Release();
    return saved;
}

int run_in_background( HINSTANCE instance ) {
    HANDLE mutex = CreateMutexW( nullptr, FALSE, mutex_name );
    if ( GetLastError() == ERROR_ALREADY_EXISTS ) return 0;

    WNDCLASSW window_type = {};
    window_type.lpfnWndProc = window_proc;
    window_type.hInstance = instance;
    window_type.lpszClassName = window_class;
    RegisterClassW( &window_type );
    main_window = CreateWindowExW( 0, window_class, L"Claude Toggle", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, nullptr );

    DWORD hook_thread_id = 0;
    HANDLE hooks = CreateThread( nullptr, 0, hook_thread, nullptr, 0, &hook_thread_id );

    MSG message;
    while ( GetMessageW( &message, nullptr, 0, 0 ) > 0 ) DispatchMessageW( &message );

    PostThreadMessageW( hook_thread_id, WM_QUIT, 0, 0 );
    WaitForSingleObject( hooks, 2000 );
    CloseHandle( hooks );
    CloseHandle( mutex );
    return 0;
}

}  // namespace

int WINAPI wWinMain( HINSTANCE instance, HINSTANCE, PWSTR command_line, int ) {
    CoInitializeEx( nullptr, COINIT_APARTMENTTHREADED );
    wstring argument = command_line;
    argument.erase( 0, argument.find_first_not_of( L" \t" ) );
    argument.erase( argument.find_last_not_of( L" \t" ) + 1 );
    int result = 0;

    if ( argument == L"--stop" ) {
        stop_running_copy();
    } else if ( argument == L"--toggle" ) {
        HWND running = FindWindowExW( HWND_MESSAGE, nullptr, window_class, nullptr );
        if ( running ) PostMessageW( running, toggle_message, 0, 0 );
    } else if ( argument == L"--install" ) {
        if ( create_startup_shortcut() ) {
            ShellExecuteW( nullptr, L"open", own_path().c_str(), nullptr, nullptr, SW_SHOWNORMAL );
            say( L"Claude Toggle will start with Windows and is running now. Tap Right Ctrl to show or hide Claude." );
        } else {
            say( L"Could not create the shortcut in the Startup folder." );
            result = 1;
        }
    } else if ( argument == L"--uninstall" ) {
        stop_running_copy();
        DeleteFileW( startup_shortcut_path().c_str() );
        say( L"Claude Toggle is stopped and will no longer start with Windows." );
    } else if ( argument.empty() ) {
        result = run_in_background( instance );
    } else {
        say( L"Usage: claude_toggle [--install | --uninstall | --stop | --toggle]" );
        result = 1;
    }

    CoUninitialize();
    return result;
}

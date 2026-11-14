#include "app_window.h"

#include <appmodel.h>
#include <objbase.h>
#include <propkey.h>
#include <shellapi.h>
#include <shobjidl.h>

#include <map>

#include "virtual_desktops.h"

namespace {

const WORD spare_key = 0xE8;

// The window each app had when it was last hidden, so the same one comes back. A hidden window
// has no taskbar entry to identify it by, so its process is kept to check it is still the same.
struct hidden_window {
    HWND hwnd;
    DWORD process_id;
};
std::map<wstring, hidden_window> hidden_windows;

DWORD process_of( HWND hwnd ) {
    DWORD process_id = 0;
    GetWindowThreadProcessId( hwnd, &process_id );
    return process_id;
}

bool same_text( const wstring &a, const wstring &b ) {
    return !a.empty() && _wcsicmp( a.c_str(), b.c_str() ) == 0;
}

wstring window_app_id( HWND hwnd ) {
    IPropertyStore *store = nullptr;
    wstring id;
    if ( FAILED( SHGetPropertyStoreForWindow( hwnd, IID_PPV_ARGS( &store ) ) ) ) return id;
    PROPVARIANT value;
    PropVariantInit( &value );
    if ( SUCCEEDED( store->GetValue( PKEY_AppUserModel_ID, &value ) ) && value.vt == VT_LPWSTR ) id = value.pwszVal;
    PropVariantClear( &value );
    store->Release();
    return id;
}

bool process_matches( HWND hwnd, const binding &app ) {
    HANDLE process = OpenProcess( PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_of( hwnd ) );
    if ( !process ) return false;
    wchar_t text[ MAX_PATH * 2 ];
    UINT32 id_length = MAX_PATH * 2;
    bool matches = GetApplicationUserModelId( process, &id_length, text ) == ERROR_SUCCESS && same_text( text, app.app_id );
    DWORD path_length = MAX_PATH * 2;
    if ( !matches && QueryFullProcessImageNameW( process, 0, text, &path_length ) ) matches = same_text( text, app.exe_path );
    CloseHandle( process );
    return matches;
}

bool is_main_window( HWND hwnd ) {
    if ( GetWindow( hwnd, GW_OWNER ) ) return false;
    if ( GetWindowLongPtrW( hwnd, GWL_EXSTYLE ) & WS_EX_TOOLWINDOW ) return false;
    return GetWindowTextLengthW( hwnd ) > 0;
}

// The taskbar's app ID covers most apps, including those that set it on their process, such as
// Discord. The window's own ID and the process checks catch the rest.
bool belongs_to( HWND hwnd, const binding &app ) {
    if ( !is_main_window( hwnd ) ) return false;
    wstring taskbar_id = taskbar_app_id( hwnd );
    return same_text( taskbar_id, app.app_id ) || same_text( taskbar_id, app.exe_path ) ||
           same_text( window_app_id( hwnd ), app.app_id ) || process_matches( hwnd, app );
}

struct search {
    const binding *app;
    HWND found;
};

// EnumWindows goes from the top of the z-order, so the first match is the app's most recently
// used window.
BOOL CALLBACK find_visible( HWND hwnd, LPARAM data ) {
    search *wanted = reinterpret_cast<search *>( data );
    if ( IsWindowVisible( hwnd ) && belongs_to( hwnd, *wanted->app ) ) {
        wanted->found = hwnd;
        return FALSE;
    }
    return TRUE;
}

HWND find_window( const binding &app ) {
    search wanted = { &app, nullptr };
    EnumWindows( find_visible, reinterpret_cast<LPARAM>( &wanted ) );
    if ( wanted.found ) return wanted.found;
    auto hidden = hidden_windows.find( app.app_id );
    if ( hidden == hidden_windows.end() ) return nullptr;
    HWND hwnd = hidden->second.hwnd;
    if ( IsWindow( hwnd ) && !IsWindowVisible( hwnd ) && process_of( hwnd ) == hidden->second.process_id ) return hwnd;
    return nullptr;
}

GUID desktop_of( HWND hwnd ) {
    GUID id = GUID_NULL;
    IVirtualDesktopManager *desktops = nullptr;
    if ( FAILED( CoCreateInstance( CLSID_VirtualDesktopManager, nullptr, CLSCTX_ALL, IID_PPV_ARGS( &desktops ) ) ) ) return id;
    desktops->GetWindowDesktopId( hwnd, &id );
    desktops->Release();
    return id;
}

bool on_current_desktop( HWND hwnd ) {
    IVirtualDesktopManager *desktops = nullptr;
    if ( FAILED( CoCreateInstance( CLSID_VirtualDesktopManager, nullptr, CLSCTX_ALL, IID_PPV_ARGS( &desktops ) ) ) ) return true;
    BOOL here = TRUE;
    desktops->IsWindowOnCurrentVirtualDesktop( hwnd, &here );
    desktops->Release();
    return here;
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

// Programs chosen with Browse have a file path as their app ID and open directly.
void launch( const binding &app ) {
    bool is_file = app.app_id.find( L":\\" ) == 1;
    wstring target = is_file ? app.app_id : L"shell:AppsFolder\\" + app.app_id;
    ShellExecuteW( nullptr, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL );
}

}  // namespace

void toggle_app( const binding &app ) {
    HWND window = find_window( app );
    if ( !window ) {
        launch( app );
        return;
    }
    bool visible = IsWindowVisible( window );
    if ( visible && GetForegroundWindow() == window && !IsIconic( window ) && on_current_desktop( window ) ) {
        ShowWindow( window, SW_HIDE );
        hidden_windows[ app.app_id ] = { window, process_of( window ) };
        return;
    }

    // Some apps, Claude among them, put themselves back on their old desktop when shown. So the
    // window is shown without activating, moved here, and only then brought forward;
    // activating it while it is on another desktop would switch to that desktop.
    if ( !visible ) {
        ShowWindow( window, SW_SHOWNA );
        wait_until_on_a_desktop( window );
    }
    if ( !on_current_desktop( window ) && !move_to_current_desktop( window ) ) {
        ShowWindow( window, SW_HIDE );
        wait_until_off_desktop( window );
        ShowWindow( window, SW_SHOWNA );
        wait_until_on_a_desktop( window );
    }
    wait_until_on_current_desktop( window );
    if ( IsIconic( window ) ) ShowWindow( window, SW_RESTORE );
    hidden_windows.erase( app.app_id );
    bring_to_front( window );
}

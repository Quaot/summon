#include "settings_window.h"

#include <commctrl.h>

#include "app_picker.h"
#include "bindings.h"
#include "key_picker.h"
#include "startup.h"
#include "ui.h"

using std::vector;

namespace {

enum control_id { binding_list = 101, add_button, change_button, remove_button, startup_box };

HWND window = nullptr;
HWND background_window = nullptr;
HWND list = nullptr;
HWND change_control = nullptr;
HWND remove_control = nullptr;
HFONT font = nullptr;
vector<binding> bindings;

int selected_row() {
    return static_cast<int>( SendMessageW( list, LVM_GETNEXTITEM, static_cast<WPARAM>( -1 ), LVNI_SELECTED ) );
}

void update_buttons() {
    bool any = selected_row() >= 0;
    EnableWindow( change_control, any );
    EnableWindow( remove_control, any );
}

void fill_list( int select ) {
    SendMessageW( list, LVM_DELETEALLITEMS, 0, 0 );
    for ( size_t index = 0; index < bindings.size(); index++ ) {
        LVITEMW item = {};
        item.mask = LVIF_TEXT;
        item.iItem = static_cast<int>( index );
        item.pszText = const_cast<wchar_t *>( bindings[ index ].key_name.c_str() );
        SendMessageW( list, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>( &item ) );
        item.iSubItem = 1;
        item.pszText = const_cast<wchar_t *>( bindings[ index ].app_name.c_str() );
        SendMessageW( list, LVM_SETITEMTEXTW, index, reinterpret_cast<LPARAM>( &item ) );
    }
    if ( select >= 0 && select < static_cast<int>( bindings.size() ) ) {
        LVITEMW state = {};
        state.stateMask = state.state = LVIS_SELECTED | LVIS_FOCUSED;
        SendMessageW( list, LVM_SETITEMSTATE, select, reinterpret_cast<LPARAM>( &state ) );
    }
    update_buttons();
}

void save_and_apply( int select ) {
    if ( !save_bindings( bindings ) ) say( window, L"Could not write settings.txt next to summon.exe." );
    PostMessageW( background_window, bindings_changed_message, 0, 0 );
    fill_list( select );
}

const int key_kept = -2;
const int key_was_free = -1;

// A key can open only one app. If another row has the key, the user chooses whether to move
// it; the result is key_kept, key_was_free, or the row the key was taken from.
int free_key( DWORD key, const wstring &key_name, const wstring &app_name, int keep ) {
    int row = row_using_key( bindings, key, keep );
    if ( row < 0 ) return key_was_free;
    wstring question = key_name + L" already opens " + bindings[ row ].app_name + L". Use it for " + app_name + L" instead?";
    if ( MessageBoxW( window, question.c_str(), L"Summon", MB_YESNO | MB_ICONQUESTION ) != IDYES ) return key_kept;
    bindings.erase( bindings.begin() + row );
    return row;
}

void add_binding() {
    installed_app app;
    if ( !pick_app( window, app ) ) return;
    binding entry;
    if ( !pick_key( window, app.name, entry.key, entry.key_name ) ) return;
    if ( free_key( entry.key, entry.key_name, app.name, -1 ) == key_kept ) return;
    entry.app_name = app.name;
    entry.app_id = app.app_id;
    entry.exe_path = app.exe_path;
    bindings.push_back( entry );
    save_and_apply( static_cast<int>( bindings.size() ) - 1 );
}

void change_key() {
    int row = selected_row();
    if ( row < 0 ) return;
    DWORD key = 0;
    wstring key_name;
    if ( !pick_key( window, bindings[ row ].app_name, key, key_name ) ) return;
    int taken_from = free_key( key, key_name, bindings[ row ].app_name, row );
    if ( taken_from == key_kept ) return;
    if ( taken_from >= 0 && taken_from < row ) row--;
    bindings[ row ].key = key;
    bindings[ row ].key_name = key_name;
    save_and_apply( row );
}

void remove_binding() {
    int row = selected_row();
    if ( row < 0 ) return;
    bindings.erase( bindings.begin() + row );
    save_and_apply( row < static_cast<int>( bindings.size() ) ? row : row - 1 );
}

void lay_out() {
    UINT dpi = GetDpiForWindow( window );
    auto px = [ dpi ]( int value ) { return scaled( value, dpi ); };
    RECT area;
    GetClientRect( window, &area );
    int width = area.right - px( 24 );
    MoveWindow( GetDlgItem( window, 100 ), px( 12 ), px( 12 ), width, px( 36 ), FALSE );
    MoveWindow( list, px( 12 ), px( 52 ), width, px( 180 ), FALSE );
    MoveWindow( GetDlgItem( window, add_button ), px( 12 ), px( 244 ), px( 100 ), px( 28 ), FALSE );
    MoveWindow( change_control, px( 120 ), px( 244 ), px( 100 ), px( 28 ), FALSE );
    MoveWindow( remove_control, px( 228 ), px( 244 ), px( 100 ), px( 28 ), FALSE );
    MoveWindow( GetDlgItem( window, startup_box ), px( 12 ), px( 288 ), px( 220 ), px( 24 ), FALSE );
    LVCOLUMNW column = {};
    column.mask = LVCF_WIDTH;
    column.cx = px( 160 );
    SendMessageW( list, LVM_SETCOLUMNW, 0, reinterpret_cast<LPARAM>( &column ) );
    column.cx = width - px( 160 ) - GetSystemMetricsForDpi( SM_CXVSCROLL, dpi ) - px( 4 );
    SendMessageW( list, LVM_SETCOLUMNW, 1, reinterpret_cast<LPARAM>( &column ) );
}

LRESULT CALLBACK settings_proc( HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam ) {
    switch ( message ) {
    case WM_COMMAND:
        switch ( LOWORD( wparam ) ) {
        case add_button: add_binding(); break;
        case change_button: change_key(); break;
        case remove_button: remove_binding(); break;
        case startup_box: {
            bool wanted = SendMessageW( GetDlgItem( hwnd, startup_box ), BM_GETCHECK, 0, 0 ) == BST_CHECKED;
            if ( !set_starts_with_windows( wanted ) ) say( hwnd, L"Could not change the shortcut in the Startup folder." );
            SendMessageW( GetDlgItem( hwnd, startup_box ), BM_SETCHECK, starts_with_windows() ? BST_CHECKED : BST_UNCHECKED, 0 );
            break;
        }
        case IDCANCEL: DestroyWindow( hwnd ); break;
        }
        return 0;
    case WM_NOTIFY: {
        const NMHDR *header = reinterpret_cast<const NMHDR *>( lparam );
        if ( header->idFrom == binding_list && header->code == LVN_ITEMCHANGED ) update_buttons();
        if ( header->idFrom == binding_list && header->code == NM_DBLCLK ) change_key();
        if ( header->idFrom == binding_list && header->code == LVN_KEYDOWN &&
             reinterpret_cast<const NMLVKEYDOWN *>( lparam )->wVKey == VK_DELETE )
            remove_binding();
        return 0;
    }
    case WM_DESTROY:
        DeleteObject( font );
        window = nullptr;
        return 0;
    }
    return DefWindowProcW( hwnd, message, wparam, lparam );
}

}  // namespace

HWND settings_window() {
    return window;
}

void show_settings( HWND background ) {
    if ( window ) {
        ShowWindow( window, SW_RESTORE );
        SetForegroundWindow( window );
        return;
    }
    background_window = background;
    bindings = load_bindings();
    window = make_window( L"SummonSettings", settings_proc, L"Summon", 460, 326, nullptr,
                          WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX );
    font = message_font( GetDpiForWindow( window ) );

    add_control( window, L"STATIC", L"Each key shows its app on the desktop you are on, or hides the app when it is "
                                    L"already in front. Ctrl, Shift, Alt and Win work as lone taps.",
                 0, 100, font );
    list = add_control( window, WC_LISTVIEWW, L"", WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                        binding_list, font, WS_EX_CLIENTEDGE );
    SendMessageW( list, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER );
    LVCOLUMNW column = {};
    column.mask = LVCF_TEXT;
    column.pszText = const_cast<wchar_t *>( L"Key" );
    SendMessageW( list, LVM_INSERTCOLUMNW, 0, reinterpret_cast<LPARAM>( &column ) );
    column.pszText = const_cast<wchar_t *>( L"App" );
    SendMessageW( list, LVM_INSERTCOLUMNW, 1, reinterpret_cast<LPARAM>( &column ) );

    add_control( window, L"BUTTON", L"Add...", WS_TABSTOP, add_button, font );
    change_control = add_control( window, L"BUTTON", L"Change key...", WS_TABSTOP, change_button, font );
    remove_control = add_control( window, L"BUTTON", L"Remove", WS_TABSTOP, remove_button, font );
    HWND startup = add_control( window, L"BUTTON", L"Start with Windows", WS_TABSTOP | BS_AUTOCHECKBOX, startup_box, font );
    SendMessageW( startup, BM_SETCHECK, starts_with_windows() ? BST_CHECKED : BST_UNCHECKED, 0 );

    lay_out();
    fill_list( bindings.empty() ? -1 : 0 );
    // The first window a process shows can take the show state it was launched with, which
    // may be minimised.
    ShowWindow( window, SW_SHOWNORMAL );
    if ( IsIconic( window ) ) ShowWindow( window, SW_RESTORE );
    SetForegroundWindow( window );
    SetFocus( list );
}

#include "app_picker.h"

#include <commctrl.h>

#include "ui.h"

using std::vector;

namespace {

enum control_id { search_box = 101, app_list, ok_button, cancel_button, browse_button };

const int chose_browsed = -2;

vector<installed_app> apps;
installed_app browsed;
HWND search = nullptr;
HWND list = nullptr;
int chosen_index = -1;

wstring lowered( wstring text ) {
    CharLowerBuffW( &text[ 0 ], static_cast<DWORD>( text.size() ) );
    return text;
}

void fill_list() {
    wchar_t typed[ 256 ] = L"";
    GetWindowTextW( search, typed, 256 );
    wstring wanted = lowered( typed );
    SendMessageW( list, WM_SETREDRAW, FALSE, 0 );
    SendMessageW( list, LB_RESETCONTENT, 0, 0 );
    for ( size_t index = 0; index < apps.size(); index++ ) {
        if ( !wanted.empty() && lowered( apps[ index ].name ).find( wanted ) == wstring::npos ) continue;
        LRESULT row = SendMessageW( list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>( apps[ index ].name.c_str() ) );
        SendMessageW( list, LB_SETITEMDATA, row, index );
    }
    SendMessageW( list, LB_SETCURSEL, 0, 0 );
    SendMessageW( list, WM_SETREDRAW, TRUE, 0 );
    InvalidateRect( list, nullptr, TRUE );
}

void accept( HWND window ) {
    LRESULT row = SendMessageW( list, LB_GETCURSEL, 0, 0 );
    if ( row == LB_ERR ) return;
    chosen_index = static_cast<int>( SendMessageW( list, LB_GETITEMDATA, row, 0 ) );
    DestroyWindow( window );
}

// Up and down in the search box move through the list, so the mouse is never needed.
LRESULT CALLBACK search_proc( HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR ) {
    if ( message == WM_KEYDOWN && ( wparam == VK_UP || wparam == VK_DOWN || wparam == VK_PRIOR || wparam == VK_NEXT ) ) {
        SendMessageW( list, message, wparam, lparam );
        return 0;
    }
    return DefSubclassProc( hwnd, message, wparam, lparam );
}

LRESULT CALLBACK picker_proc( HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam ) {
    if ( message == WM_COMMAND ) {
        int id = LOWORD( wparam );
        int code = HIWORD( wparam );
        if ( id == search_box && code == EN_CHANGE ) fill_list();
        if ( id == app_list && code == LBN_DBLCLK ) accept( hwnd );
        if ( id == ok_button || id == IDOK ) accept( hwnd );
        if ( id == cancel_button || id == IDCANCEL ) DestroyWindow( hwnd );
        if ( id == browse_button && browse_for_program( hwnd, browsed ) ) {
            chosen_index = chose_browsed;
            DestroyWindow( hwnd );
        }
        return 0;
    }
    if ( message == WM_CLOSE ) {
        DestroyWindow( hwnd );
        return 0;
    }
    return DefWindowProcW( hwnd, message, wparam, lparam );
}

}  // namespace

bool pick_app( HWND owner, installed_app &chosen ) {
    HCURSOR old_cursor = SetCursor( LoadCursorW( nullptr, IDC_WAIT ) );
    apps = list_installed_apps();
    SetCursor( old_cursor );
    chosen_index = -1;

    const int width = 340, height = 400, margin = 12, button_width = 88, button_height = 28;
    HWND window = make_window( L"SummonAppPicker", picker_proc, L"Choose an app", width, height, owner,
                               WS_CAPTION | WS_SYSMENU );
    UINT dpi = GetDpiForWindow( window );
    HFONT font = message_font( dpi );
    auto px = [ dpi ]( int value ) { return scaled( value, dpi ); };

    search = add_control( window, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, search_box, font, WS_EX_CLIENTEDGE );
    SendMessageW( search, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>( L"Search apps" ) );
    SetWindowSubclass( search, search_proc, 1, 0 );
    list = add_control( window, L"LISTBOX", L"", WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, app_list,
                        font, WS_EX_CLIENTEDGE );
    HWND ok = add_control( window, L"BUTTON", L"Choose", WS_TABSTOP | BS_DEFPUSHBUTTON, ok_button, font );
    HWND cancel = add_control( window, L"BUTTON", L"Cancel", WS_TABSTOP, cancel_button, font );
    HWND browse = add_control( window, L"BUTTON", L"Browse...", WS_TABSTOP, browse_button, font );

    int search_height = px( 24 );
    MoveWindow( search, px( margin ), px( margin ), px( width - 2 * margin ), search_height, FALSE );
    int list_top = px( margin ) + search_height + px( 8 );
    int buttons_top = px( height - margin - button_height );
    MoveWindow( list, px( margin ), list_top, px( width - 2 * margin ), buttons_top - px( 10 ) - list_top, FALSE );
    MoveWindow( ok, px( width - margin - 2 * button_width - 8 ), buttons_top, px( button_width ), px( button_height ), FALSE );
    MoveWindow( cancel, px( width - margin - button_width ), buttons_top, px( button_width ), px( button_height ), FALSE );
    MoveWindow( browse, px( margin ), buttons_top, px( button_width ), px( button_height ), FALSE );

    fill_list();
    SetFocus( search );
    run_modal( window, owner );
    DeleteObject( font );

    if ( chosen_index == chose_browsed ) {
        chosen = browsed;
        return true;
    }
    if ( chosen_index < 0 ) return false;
    chosen = apps[ chosen_index ];
    return true;
}

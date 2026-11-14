#include "tray.h"

#include <shellapi.h>

#include "ui.h"

namespace {

NOTIFYICONDATAW icon_data( HWND owner ) {
    NOTIFYICONDATAW data = {};
    data.cbSize = sizeof( data );
    data.hWnd = owner;
    data.uID = 1;
    return data;
}

}  // namespace

void add_tray_icon( HWND owner ) {
    NOTIFYICONDATAW data = icon_data( owner );
    data.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP;
    data.uCallbackMessage = tray_message;
    data.hIcon = summon_icon( GetSystemMetrics( SM_CXSMICON ) );
    lstrcpynW( data.szTip, L"Summon", ARRAYSIZE( data.szTip ) );
    Shell_NotifyIconW( NIM_ADD, &data );
    data.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW( NIM_SETVERSION, &data );
}

void remove_tray_icon( HWND owner ) {
    NOTIFYICONDATAW data = icon_data( owner );
    Shell_NotifyIconW( NIM_DELETE, &data );
}

int show_tray_menu( HWND owner ) {
    HMENU menu = CreatePopupMenu();
    AppendMenuW( menu, MF_STRING, tray_settings, L"Settings" );
    AppendMenuW( menu, MF_SEPARATOR, 0, nullptr );
    AppendMenuW( menu, MF_STRING, tray_quit, L"Quit Summon" );
    SetMenuDefaultItem( menu, tray_settings, FALSE );
    POINT cursor;
    GetCursorPos( &cursor );
    // The owner must be in front for the menu to close when you click elsewhere.
    SetForegroundWindow( owner );
    int chosen = TrackPopupMenu( menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, cursor.x, cursor.y, 0, owner, nullptr );
    PostMessageW( owner, WM_NULL, 0, 0 );
    DestroyMenu( menu );
    return chosen;
}

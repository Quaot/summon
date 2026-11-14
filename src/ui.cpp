#include "ui.h"

#include <commctrl.h>

int scaled( int value, UINT dpi ) {
    return MulDiv( value, static_cast<int>( dpi ), 96 );
}

HFONT message_font( UINT dpi ) {
    NONCLIENTMETRICSW metrics = {};
    metrics.cbSize = sizeof( metrics );
    SystemParametersInfoForDpi( SPI_GETNONCLIENTMETRICS, sizeof( metrics ), &metrics, 0, dpi );
    return CreateFontIndirectW( &metrics.lfMessageFont );
}

HWND add_control( HWND parent, const wchar_t *class_name, const wchar_t *text, DWORD style, int id, HFONT font,
                  DWORD ex_style ) {
    HWND control = CreateWindowExW( ex_style, class_name, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, parent,
                                    reinterpret_cast<HMENU>( static_cast<INT_PTR>( id ) ), GetModuleHandleW( nullptr ),
                                    nullptr );
    SendMessageW( control, WM_SETFONT, reinterpret_cast<WPARAM>( font ), TRUE );
    return control;
}

HWND make_window( const wchar_t *class_name, WNDPROC procedure, const wchar_t *title, int client_width,
                  int client_height, HWND owner, DWORD style ) {
    HINSTANCE instance = GetModuleHandleW( nullptr );
    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof( window_class );
    window_class.lpfnWndProc = procedure;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW( nullptr, IDC_ARROW );
    window_class.hbrBackground = reinterpret_cast<HBRUSH>( COLOR_BTNFACE + 1 );
    window_class.hIcon = summon_icon( GetSystemMetrics( SM_CXICON ) );
    window_class.hIconSm = summon_icon( GetSystemMetrics( SM_CXSMICON ) );
    window_class.lpszClassName = class_name;
    RegisterClassExW( &window_class );

    UINT dpi = GetDpiForSystem();
    RECT frame = { 0, 0, scaled( client_width, dpi ), scaled( client_height, dpi ) };
    AdjustWindowRectExForDpi( &frame, style, FALSE, 0, dpi );
    int width = frame.right - frame.left;
    int height = frame.bottom - frame.top;
    RECT screen;
    SystemParametersInfoW( SPI_GETWORKAREA, 0, &screen, 0 );
    int left = screen.left + ( screen.right - screen.left - width ) / 2;
    int top = screen.top + ( screen.bottom - screen.top - height ) / 2;
    return CreateWindowExW( 0, class_name, title, style, left, top, width, height, owner, nullptr, instance, nullptr );
}

// The bell from summon.ico (drawn by art/make_icon.py), scaled from the nearest larger frame.
HICON summon_icon( int size ) {
    HICON icon = nullptr;
    LoadIconWithScaleDown( GetModuleHandleW( nullptr ), MAKEINTRESOURCEW( 1 ), size, size, &icon );
    return icon;
}

void run_modal( HWND window, HWND owner ) {
    if ( owner ) EnableWindow( owner, FALSE );
    ShowWindow( window, SW_SHOW );
    SetForegroundWindow( window );
    HWND first = GetNextDlgTabItem( window, nullptr, FALSE );
    if ( first ) SetFocus( first );
    MSG message;
    while ( IsWindow( window ) ) {
        if ( GetMessageW( &message, nullptr, 0, 0 ) <= 0 ) {
            PostQuitMessage( 0 );
            break;
        }
        if ( IsDialogMessageW( window, &message ) ) continue;
        TranslateMessage( &message );
        DispatchMessageW( &message );
    }
    if ( owner ) {
        EnableWindow( owner, TRUE );
        SetForegroundWindow( owner );
    }
}

void say( HWND owner, const wstring &text ) {
    MessageBoxW( owner, text.c_str(), L"Summon", MB_OK );
}

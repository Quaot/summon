#include "ui.h"

#include <cmath>
#include <vector>

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

// A rounded teal square with a white S, drawn at the size asked for.
HICON summon_icon( int size ) {
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof( info.bmiHeader );
    info.bmiHeader.biWidth = size;
    info.bmiHeader.biHeight = -size;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;

    HDC screen = GetDC( nullptr );
    HDC canvas = CreateCompatibleDC( screen );
    void *letter_bits = nullptr;
    HBITMAP letter = CreateDIBSection( screen, &info, DIB_RGB_COLORS, &letter_bits, nullptr, 0 );
    HGDIOBJ old_bitmap = SelectObject( canvas, letter );
    HFONT font = CreateFontW( -size * 3 / 4, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0,
                              ANTIALIASED_QUALITY, 0, L"Segoe UI" );
    HGDIOBJ old_font = SelectObject( canvas, font );
    SetBkColor( canvas, RGB( 0, 0, 0 ) );
    SetTextColor( canvas, RGB( 255, 255, 255 ) );
    RECT area = { 0, -size / 16, size, size };
    DrawTextW( canvas, L"S", 1, &area, DT_CENTER | DT_VCENTER | DT_SINGLELINE );
    GdiFlush();

    std::vector<DWORD> pixels( size * size );
    const DWORD *coverage = static_cast<const DWORD *>( letter_bits );
    double radius = size * 0.22;
    for ( int y = 0; y < size; y++ ) {
        for ( int x = 0; x < size; x++ ) {
            double px = x + 0.5, py = y + 0.5;
            double dx = std::fmax( std::fmax( radius - px, px - ( size - radius ) ), 0.0 );
            double dy = std::fmax( std::fmax( radius - py, py - ( size - radius ) ), 0.0 );
            double inside = std::fmin( std::fmax( radius - std::sqrt( dx * dx + dy * dy ) + 0.5, 0.0 ), 1.0 );
            double white = ( coverage[ y * size + x ] & 0xFF ) / 255.0;
            int red = static_cast<int>( 0x1F + ( 255 - 0x1F ) * white );
            int green = static_cast<int>( 0x7A + ( 255 - 0x7A ) * white );
            int blue = static_cast<int>( 0x8C + ( 255 - 0x8C ) * white );
            int alpha = static_cast<int>( inside * 255 );
            pixels[ y * size + x ] = ( alpha << 24 ) | ( red << 16 ) | ( green << 8 ) | blue;
        }
    }

    void *colour_bits = nullptr;
    HBITMAP colour = CreateDIBSection( screen, &info, DIB_RGB_COLORS, &colour_bits, nullptr, 0 );
    memcpy( colour_bits, pixels.data(), pixels.size() * sizeof( DWORD ) );
    HBITMAP mask = CreateBitmap( size, size, 1, 1, nullptr );
    ICONINFO icon_info = { TRUE, 0, 0, mask, colour };
    HICON icon = CreateIconIndirect( &icon_info );

    SelectObject( canvas, old_font );
    SelectObject( canvas, old_bitmap );
    DeleteObject( font );
    DeleteObject( letter );
    DeleteObject( colour );
    DeleteObject( mask );
    DeleteDC( canvas );
    ReleaseDC( nullptr, screen );
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

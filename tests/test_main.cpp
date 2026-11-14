#include "../src/bindings.h"
#include "../src/hooks.h"
#include "../src/app_window.h"
#include "../src/apps.h"
#include "../src/key_picker.h"
#include "../src/virtual_desktops.h"

#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <propsys.h>
#include <cstdio>
#include <string>
#include <vector>
#include <utility>

using std::wstring;
using std::string;
using std::vector;

static bool any_failed = false;
static int check_count = 0;

static void check( bool passed, const char *what )
{
    check_count++;
    if ( !passed ) any_failed = true;
    printf( "%s  %s\n", passed ? "pass" : "FAIL", what );
}

// Fired keys

static const UINT fire_message = WM_APP + 7;
static HWND target = nullptr;

static vector<std::pair<WPARAM, LPARAM>> fired()
{
    vector<std::pair<WPARAM, LPARAM>> keys;
    MSG msg;
    while ( PeekMessageW( &msg, target, fire_message, fire_message, PM_REMOVE ) )
        keys.push_back( { msg.wParam, msg.lParam } );
    return keys;
}

static bool fired_once( DWORD key )
{
    auto keys = fired();
    return keys.size() == 1 && keys[0].first == key && keys[0].second == 0;
}

static bool fired_nothing()
{
    return fired().empty();
}

static bool key( WPARAM message, DWORD vk, bool injected = false )
{
    KBDLLHOOKSTRUCT event = {};
    event.vkCode = vk;
    event.scanCode = MapVirtualKeyW( vk, MAPVK_VK_TO_VSC );
    event.flags = injected ? LLKHF_INJECTED : 0;
    if ( message == WM_KEYUP || message == WM_SYSKEYUP ) event.flags |= LLKHF_UP;
    event.time = GetTickCount();
    return handle_keyboard_event( message, event );
}

static bool mouse( WPARAM message, DWORD data = 0, bool injected = false )
{
    MSLLHOOKSTRUCT event = {};
    event.mouseData = data;
    event.flags = injected ? LLMHF_INJECTED : 0;
    event.time = GetTickCount();
    return handle_mouse_event( message, event );
}

static void bind( vector<DWORD> keys )
{
    set_bound_keys( keys );
    fired();
}

// Files in %TEMP%

static wstring temp_file( const wchar_t *name )
{
    wchar_t folder[MAX_PATH];
    GetTempPathW( MAX_PATH, folder );
    return wstring( folder ) + name;
}

static void write_bytes( const wstring &path, const string &bytes )
{
    HANDLE file = CreateFileW( path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr );
    DWORD written = 0;
    WriteFile( file, bytes.data(), (DWORD) bytes.size(), &written, nullptr );
    CloseHandle( file );
}

static vector<binding> load_text( const string &bytes )
{
    wstring path = temp_file( L"summon_test_settings.txt" );
    write_bytes( path, bytes );
    vector<binding> bindings = load_bindings_from( path );
    DeleteFileW( path.c_str() );
    return bindings;
}

static bool same( const binding &a, const binding &b )
{
    return a.key == b.key && a.key_name == b.key_name && a.app_name == b.app_name
        && a.app_id == b.app_id && a.exe_path == b.exe_path;
}

static bool is_binding( const binding &b, DWORD key_code, const wstring &key_name, const wstring &app_name,
                        const wstring &app_id, const wstring &exe_path )
{
    return same( b, binding{ key_code, key_name, app_name, app_id, exe_path } );
}

// Hidden windows

static const wchar_t *window_class = L"summon_test_window";

static HWND hidden_window( const wchar_t *title, DWORD ex_style = 0, HWND owner = nullptr )
{
    return CreateWindowExW( ex_style, window_class, title, WS_OVERLAPPED, 0, 0, 200, 100,
                            owner, nullptr, GetModuleHandleW( nullptr ), nullptr );
}

static const PROPERTYKEY app_id_key = { { 0x9F4C2855, 0x9F79, 0x4B39, { 0xA8, 0xD0, 0xE1, 0xD4, 0x2D, 0xE1, 0xD5, 0xF3 } }, 5 };

static bool set_window_app_id( HWND hwnd, const wchar_t *app_id )
{
    IPropertyStore *store = nullptr;
    if ( FAILED( SHGetPropertyStoreForWindow( hwnd, IID_IPropertyStore, (void **) &store ) ) ) return false;
    PROPVARIANT value;
    PropVariantInit( &value );
    if ( app_id ) {
        size_t bytes = ( wcslen( app_id ) + 1 ) * sizeof( wchar_t );
        value.vt = VT_LPWSTR;
        value.pwszVal = (LPWSTR) CoTaskMemAlloc( bytes );
        memcpy( value.pwszVal, app_id, bytes );
    }
    bool done = SUCCEEDED( store->SetValue( app_id_key, value ) );
    PropVariantClear( &value );
    store->Release();
    return done;
}

static wstring own_exe_path()
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW( nullptr, path, MAX_PATH );
    return path;
}

static wstring upper( wstring text )
{
    for ( auto &c : text ) c = towupper( c );
    return text;
}

// Checks

static void check_keys()
{
    check( is_modifier( VK_LSHIFT ), "Left Shift is a modifier." );
    check( is_modifier( VK_RSHIFT ), "Right Shift is a modifier." );
    check( is_modifier( VK_LCONTROL ), "Left Ctrl is a modifier." );
    check( is_modifier( VK_RCONTROL ), "Right Ctrl is a modifier." );
    check( is_modifier( VK_LMENU ), "Left Alt is a modifier." );
    check( is_modifier( VK_RMENU ), "Right Alt is a modifier." );
    check( is_modifier( VK_LWIN ), "Left Windows key is a modifier." );
    check( is_modifier( VK_RWIN ), "Right Windows key is a modifier." );
    check( !is_modifier( VK_SHIFT ), "The side-neutral VK_SHIFT is not a modifier." );
    check( !is_modifier( VK_CONTROL ), "The side-neutral VK_CONTROL is not a modifier." );
    check( !is_modifier( VK_MENU ), "The side-neutral VK_MENU is not a modifier." );
    check( !is_modifier( 'A' ) && !is_modifier( VK_F13 ) && !is_modifier( VK_CAPITAL ),
           "Letters, function keys and Caps Lock are not modifiers." );
    check( !is_modifier( VK_MBUTTON ) && !is_modifier( VK_XBUTTON1 ), "Mouse buttons are not modifiers." );
}

static void check_mouse_buttons()
{
    MSLLHOOKSTRUCT event = {};
    check( mouse_button_key( WM_MBUTTONDOWN, &event ) == VK_MBUTTON, "A middle button press gives VK_MBUTTON." );
    check( mouse_button_key( WM_MBUTTONUP, &event ) == VK_MBUTTON, "A middle button release gives VK_MBUTTON." );
    event.mouseData = MAKELONG( 0, XBUTTON1 );
    check( mouse_button_key( WM_XBUTTONDOWN, &event ) == VK_XBUTTON1, "A back button press gives VK_XBUTTON1." );
    check( mouse_button_key( WM_XBUTTONUP, &event ) == VK_XBUTTON1, "A back button release gives VK_XBUTTON1." );
    event.mouseData = MAKELONG( 0, XBUTTON2 );
    check( mouse_button_key( WM_XBUTTONDOWN, &event ) == VK_XBUTTON2, "A forward button press gives VK_XBUTTON2." );
    check( mouse_button_key( WM_XBUTTONUP, &event ) == VK_XBUTTON2, "A forward button release gives VK_XBUTTON2." );
    event.mouseData = 0;
    check( mouse_button_key( WM_LBUTTONDOWN, &event ) == 0, "A left button press gives 0." );
    check( mouse_button_key( WM_RBUTTONDOWN, &event ) == 0, "A right button press gives 0." );
    event.mouseData = MAKELONG( 0, WHEEL_DELTA );
    check( mouse_button_key( WM_MOUSEWHEEL, &event ) == 0, "A wheel turn gives 0." );
    event.mouseData = 0;
    check( mouse_button_key( WM_MOUSEMOVE, &event ) == 0, "Mouse movement gives 0." );

    check( is_button_down( WM_MBUTTONDOWN ), "A middle button press counts as a button press." );
    check( is_button_down( WM_XBUTTONDOWN ), "A side button press counts as a button press." );
    check( !is_button_down( WM_MBUTTONUP ), "A middle button release does not count as a button press." );
    check( !is_button_down( WM_XBUTTONUP ), "A side button release does not count as a button press." );
    check( !is_button_down( WM_LBUTTONDOWN ) && !is_button_down( WM_RBUTTONDOWN ),
           "Left and right button presses do not count as button presses." );
    check( !is_button_down( WM_LBUTTONUP ) && !is_button_down( WM_RBUTTONUP ),
           "Left and right button releases do not count as button presses." );
    check( !is_button_down( WM_MOUSEWHEEL ) && !is_button_down( WM_MOUSEHWHEEL ), "Wheel turns do not count as button presses." );
    check( !is_button_down( WM_MOUSEMOVE ), "Mouse movement does not count as a button press." );
}

static void check_bound_modifier()
{
    bind( { VK_RCONTROL } );
    bool press_swallowed = key( WM_KEYDOWN, VK_RCONTROL );
    bool nothing_on_press = fired_nothing();
    bool release_swallowed = key( WM_KEYUP, VK_RCONTROL );
    check( !press_swallowed, "The press of a bound modifier is not swallowed." );
    check( nothing_on_press, "A bound modifier does not fire on its press." );
    check( !release_swallowed, "The release of a bound modifier is not swallowed." );
    check( fired_once( VK_RCONTROL ), "Tapping a bound modifier fires it once on release, with the key as wParam and 0 as lParam." );

    key( WM_KEYDOWN, VK_RCONTROL );
    key( WM_KEYDOWN, VK_RCONTROL );
    key( WM_KEYDOWN, VK_RCONTROL );
    key( WM_KEYUP, VK_RCONTROL );
    check( fired_once( VK_RCONTROL ), "Holding a bound modifier through auto-repeat and letting go fires it once." );

    key( WM_KEYDOWN, VK_RCONTROL );
    mouse( WM_MOUSEMOVE );
    key( WM_KEYUP, VK_RCONTROL );
    check( fired_once( VK_RCONTROL ), "Mouse movement during a modifier tap does not cancel it." );

    key( WM_KEYDOWN, VK_RCONTROL );
    key( WM_KEYUP, 'A' );
    key( WM_KEYUP, VK_RCONTROL );
    check( fired_once( VK_RCONTROL ), "Releasing another key during a modifier tap does not cancel it." );

    key( WM_KEYDOWN, VK_RCONTROL );
    bool c_down_swallowed = key( WM_KEYDOWN, 'C' );
    bool c_up_swallowed = key( WM_KEYUP, 'C' );
    key( WM_KEYUP, VK_RCONTROL );
    check( fired_nothing(), "Pressing an unbound key while a bound modifier is held cancels the tap." );
    check( !c_down_swallowed && !c_up_swallowed, "An unbound key pressed with a bound modifier, as in Right Ctrl+C, is not swallowed." );

    key( WM_KEYDOWN, VK_RCONTROL );
    key( WM_KEYDOWN, VK_LSHIFT );
    key( WM_KEYUP, VK_LSHIFT );
    key( WM_KEYUP, VK_RCONTROL );
    check( fired_nothing(), "Pressing an unbound modifier during a bound modifier's tap cancels the tap." );

    key( WM_KEYDOWN, VK_RCONTROL );
    mouse( WM_LBUTTONDOWN );
    key( WM_KEYUP, VK_RCONTROL );
    check( fired_nothing(), "A left button press during a modifier tap cancels it." );

    key( WM_KEYDOWN, VK_RCONTROL );
    mouse( WM_RBUTTONDOWN );
    key( WM_KEYUP, VK_RCONTROL );
    check( fired_nothing(), "A right button press during a modifier tap cancels it." );

    key( WM_KEYDOWN, VK_RCONTROL );
    mouse( WM_MBUTTONDOWN );
    key( WM_KEYUP, VK_RCONTROL );
    check( fired_nothing(), "An unbound middle button press during a modifier tap cancels it." );

    key( WM_KEYDOWN, VK_RCONTROL );
    mouse( WM_MOUSEWHEEL, MAKELONG( 0, WHEEL_DELTA ) );
    key( WM_KEYUP, VK_RCONTROL );
    check( fired_nothing(), "A wheel turn during a modifier tap cancels it." );

    key( WM_KEYDOWN, VK_RCONTROL );
    mouse( WM_MOUSEHWHEEL, MAKELONG( 0, WHEEL_DELTA ) );
    key( WM_KEYUP, VK_RCONTROL );
    check( fired_nothing(), "A sideways wheel turn during a modifier tap cancels it." );

    key( WM_KEYDOWN, VK_RCONTROL );
    mouse( WM_LBUTTONUP );
    key( WM_KEYUP, VK_RCONTROL );
    check( fired_once( VK_RCONTROL ), "A left button release during a modifier tap does not cancel it." );

    key( WM_KEYDOWN, VK_RCONTROL );
    mouse( WM_RBUTTONUP );
    key( WM_KEYUP, VK_RCONTROL );
    check( fired_once( VK_RCONTROL ), "A right button release during a modifier tap does not cancel it." );

    key( WM_KEYDOWN, VK_RCONTROL );
    mouse( WM_MBUTTONUP );
    key( WM_KEYUP, VK_RCONTROL );
    check( fired_once( VK_RCONTROL ), "A middle button release during a modifier tap does not cancel it." );

    key( WM_KEYDOWN, VK_RCONTROL );
    mouse( WM_XBUTTONUP, MAKELONG( 0, XBUTTON1 ) );
    mouse( WM_XBUTTONUP, MAKELONG( 0, XBUTTON2 ) );
    key( WM_KEYUP, VK_RCONTROL );
    check( fired_once( VK_RCONTROL ), "Side button releases during a modifier tap do not cancel it." );

    key( WM_KEYDOWN, VK_RCONTROL );
    mouse( WM_XBUTTONDOWN, MAKELONG( 0, XBUTTON2 ) );
    key( WM_KEYUP, VK_RCONTROL );
    check( fired_nothing(), "An unbound side button press during a modifier tap cancels it." );

    key( WM_KEYDOWN, VK_RCONTROL );
    key( WM_KEYUP, VK_RCONTROL );
    key( WM_KEYDOWN, VK_RCONTROL );
    key( WM_KEYUP, VK_RCONTROL );
    check( fired().size() == 2, "Two taps of a bound modifier fire twice." );

    bind( { VK_RMENU } );
    bool sys_down_swallowed = key( WM_SYSKEYDOWN, VK_RMENU );
    bool sys_up_swallowed = key( WM_SYSKEYUP, VK_RMENU );
    check( fired_once( VK_RMENU ), "A bound modifier tapped through WM_SYSKEYDOWN and WM_SYSKEYUP fires once." );
    check( !sys_down_swallowed && !sys_up_swallowed, "A bound modifier's system key press and release are not swallowed." );

    key( WM_SYSKEYDOWN, VK_RMENU );
    key( WM_SYSKEYDOWN, VK_TAB );
    key( WM_SYSKEYUP, VK_RMENU );
    check( fired_nothing(), "A WM_SYSKEYDOWN of another key counts as a press and cancels the tap." );

    bind( { VK_RCONTROL, VK_RSHIFT } );
    key( WM_KEYDOWN, VK_RCONTROL );
    key( WM_KEYDOWN, VK_RSHIFT );
    key( WM_KEYUP, VK_RCONTROL );
    key( WM_KEYUP, VK_RSHIFT );
    check( fired_once( VK_RSHIFT ), "With two bound modifiers overlapping, only the one pressed last fires." );
}

static void check_bound_ordinary_key()
{
    bind( { VK_F13 } );
    bool down_swallowed = key( WM_KEYDOWN, VK_F13 );
    check( down_swallowed, "The press of a bound ordinary key is swallowed." );
    check( fired_once( VK_F13 ), "A bound ordinary key fires once on its first press, with the key as wParam and 0 as lParam." );
    bool repeat_swallowed = key( WM_KEYDOWN, VK_F13 );
    check( repeat_swallowed, "Auto-repeat presses of a bound ordinary key are swallowed." );
    check( fired_nothing(), "Auto-repeat presses of a bound ordinary key do not fire again." );
    bool up_swallowed = key( WM_KEYUP, VK_F13 );
    check( up_swallowed, "The release of a bound ordinary key is swallowed." );
    check( fired_nothing(), "A bound ordinary key does not fire on its release." );

    key( WM_KEYDOWN, VK_F13 );
    key( WM_KEYUP, VK_F13 );
    check( fired_once( VK_F13 ), "Pressing a bound ordinary key again after releasing it fires again." );

    bool sys_down_swallowed = key( WM_SYSKEYDOWN, VK_F13 );
    check( sys_down_swallowed && fired_once( VK_F13 ), "A WM_SYSKEYDOWN of a bound ordinary key fires and is swallowed." );
    check( key( WM_SYSKEYUP, VK_F13 ), "A WM_SYSKEYUP of a bound ordinary key is swallowed." );
    fired();

    bind( { 'Q' } );
    bool unbound_swallowed = key( WM_KEYDOWN, 'W' ) || key( WM_KEYUP, 'W' );
    check( !unbound_swallowed, "An unbound key's press and release are not swallowed." );
    check( fired_nothing(), "An unbound key never fires." );

    bind( { VK_SHIFT } );
    bool shift_swallowed = key( WM_KEYDOWN, VK_SHIFT );
    check( shift_swallowed && fired_once( VK_SHIFT ), "A bound VK_SHIFT acts as an ordinary key, firing and swallowed on its press." );
    key( WM_KEYUP, VK_SHIFT );

    bind( { VK_RCONTROL, VK_F13 } );
    key( WM_KEYDOWN, VK_RCONTROL );
    bool held_swallowed = key( WM_KEYDOWN, VK_F13 );
    key( WM_KEYUP, VK_F13 );
    key( WM_KEYUP, VK_RCONTROL );
    check( held_swallowed && fired_once( VK_F13 ), "A bound ordinary key pressed while a bound modifier is held fires, and the modifier does not." );
}

static void check_bound_mouse_buttons()
{
    bind( { VK_XBUTTON1, VK_MBUTTON } );
    bool x1_down = mouse( WM_XBUTTONDOWN, MAKELONG( 0, XBUTTON1 ) );
    check( x1_down && fired_once( VK_XBUTTON1 ), "A bound back button fires on its press and the press is swallowed." );
    bool x1_up = mouse( WM_XBUTTONUP, MAKELONG( 0, XBUTTON1 ) );
    check( x1_up && fired_nothing(), "A bound back button's release is swallowed and does not fire." );

    bool x2_down = mouse( WM_XBUTTONDOWN, MAKELONG( 0, XBUTTON2 ) );
    bool x2_up = mouse( WM_XBUTTONUP, MAKELONG( 0, XBUTTON2 ) );
    check( !x2_down && !x2_up && fired_nothing(), "An unbound forward button never fires and is never swallowed." );

    bool m_down = mouse( WM_MBUTTONDOWN );
    check( m_down && fired_once( VK_MBUTTON ), "A bound middle button fires on its press and the press is swallowed." );
    bool m_up = mouse( WM_MBUTTONUP );
    check( m_up && fired_nothing(), "A bound middle button's release is swallowed and does not fire." );

    bool move = mouse( WM_MOUSEMOVE );
    check( !move && fired_nothing(), "Mouse movement is never swallowed and never fires." );

    bind( { VK_LBUTTON, VK_RBUTTON } );
    bool left = mouse( WM_LBUTTONDOWN ) || mouse( WM_LBUTTONUP );
    bool right = mouse( WM_RBUTTONDOWN ) || mouse( WM_RBUTTONUP );
    bool wheel = mouse( WM_MOUSEWHEEL, MAKELONG( 0, WHEEL_DELTA ) );
    check( !left && !right && !wheel && fired_nothing(), "The left and right buttons and the wheel never fire or get swallowed, even when given as bound keys." );

    bind( { VK_RCONTROL, VK_XBUTTON1 } );
    key( WM_KEYDOWN, VK_RCONTROL );
    mouse( WM_XBUTTONDOWN, MAKELONG( 0, XBUTTON1 ) );
    mouse( WM_XBUTTONUP, MAKELONG( 0, XBUTTON1 ) );
    key( WM_KEYUP, VK_RCONTROL );
    check( fired_once( VK_XBUTTON1 ), "A bound mouse button pressed while a bound modifier is held fires, and the modifier does not." );
}

static void check_injected_input()
{
    bind( { VK_F13, VK_RCONTROL, VK_XBUTTON1 } );
    bool down = key( WM_KEYDOWN, VK_F13, true );
    bool up = key( WM_KEYUP, VK_F13, true );
    check( !down && !up && fired_nothing(), "An injected bound ordinary key never fires and is never swallowed." );

    bool mod_down = key( WM_KEYDOWN, VK_RCONTROL, true );
    bool mod_up = key( WM_KEYUP, VK_RCONTROL, true );
    check( !mod_down && !mod_up && fired_nothing(), "An injected tap of a bound modifier never fires and is never swallowed." );

    bool x_down = mouse( WM_XBUTTONDOWN, MAKELONG( 0, XBUTTON1 ), true );
    bool x_up = mouse( WM_XBUTTONUP, MAKELONG( 0, XBUTTON1 ), true );
    check( !x_down && !x_up && fired_nothing(), "An injected bound mouse button never fires and is never swallowed." );

    key( WM_KEYDOWN, VK_RCONTROL );
    key( WM_KEYDOWN, 'C', true );
    key( WM_KEYUP, 'C', true );
    key( WM_KEYUP, VK_RCONTROL );
    check( fired_once( VK_RCONTROL ), "An injected key press during a modifier tap does not cancel it." );

    key( WM_KEYDOWN, VK_RCONTROL );
    key( WM_KEYDOWN, VK_F13, true );
    key( WM_KEYUP, VK_RCONTROL );
    check( fired_once( VK_RCONTROL ), "An injected press of a bound ordinary key during a modifier tap does not cancel it." );

    key( WM_KEYDOWN, VK_RCONTROL );
    mouse( WM_LBUTTONDOWN, 0, true );
    key( WM_KEYUP, VK_RCONTROL );
    check( fired_once( VK_RCONTROL ), "An injected left button press during a modifier tap does not cancel it." );

    key( WM_KEYDOWN, VK_RCONTROL );
    mouse( WM_XBUTTONDOWN, MAKELONG( 0, XBUTTON1 ), true );
    key( WM_KEYUP, VK_RCONTROL );
    check( fired_once( VK_RCONTROL ), "An injected press of a bound side button during a modifier tap does not cancel it." );

    key( WM_KEYDOWN, VK_RCONTROL );
    mouse( WM_MOUSEWHEEL, MAKELONG( 0, WHEEL_DELTA ), true );
    key( WM_KEYUP, VK_RCONTROL );
    check( fired_once( VK_RCONTROL ), "An injected wheel turn during a modifier tap does not cancel it." );
}

static void check_set_bound_keys()
{
    bind( { VK_RCONTROL } );
    key( WM_KEYDOWN, VK_RCONTROL );
    set_bound_keys( { VK_RCONTROL } );
    key( WM_KEYUP, VK_RCONTROL );
    check( fired_nothing(), "A modifier tap under way when the bound keys are set again does not fire." );

    bind( { VK_RCONTROL } );
    key( WM_KEYDOWN, VK_RCONTROL );
    set_bound_keys( { VK_F13 } );
    key( WM_KEYUP, VK_RCONTROL );
    check( fired_nothing(), "A modifier tap under way does not fire after the modifier is unbound." );

    bind( { VK_F13 } );
    set_bound_keys( { VK_F14 } );
    bool old_swallowed = key( WM_KEYDOWN, VK_F13 ) || key( WM_KEYUP, VK_F13 );
    check( !old_swallowed && fired_nothing(), "A key dropped from the bound set no longer fires or gets swallowed." );
    bool new_swallowed = key( WM_KEYDOWN, VK_F14 );
    key( WM_KEYUP, VK_F14 );
    check( new_swallowed && fired_once( VK_F14 ), "A key in the new bound set fires and is swallowed." );

    bind( { VK_F13 } );
    key( WM_KEYDOWN, VK_F13 );
    fired();
    set_bound_keys( { VK_F13 } );
    bool held_swallowed = key( WM_KEYDOWN, VK_F13 );
    key( WM_KEYUP, VK_F13 );
    check( held_swallowed && fired_once( VK_F13 ), "A bound ordinary key held while the bound keys are set fires on its next press." );

    bind( {} );
}

static void check_key_names()
{
    check( name_of_mouse_button( VK_MBUTTON ) == L"Middle mouse button", "The middle button is named \"Middle mouse button\"." );
    check( name_of_mouse_button( VK_XBUTTON1 ) == L"Mouse back button", "The back button is named \"Mouse back button\"." );
    check( name_of_mouse_button( VK_XBUTTON2 ) == L"Mouse forward button", "The forward button is named \"Mouse forward button\"." );

    KBDLLHOOKSTRUCT a_key = {};
    a_key.vkCode = 'A';
    a_key.scanCode = MapVirtualKeyW( 'A', MAPVK_VK_TO_VSC );
    wchar_t expected[64] = {};
    GetKeyNameTextW( a_key.scanCode << 16, expected, 64 );
    check( name_of_key( a_key ) == expected, "The A key gets the name Windows gives its scan code." );

    KBDLLHOOKSTRUCT left_ctrl = {};
    left_ctrl.vkCode = VK_LCONTROL;
    left_ctrl.scanCode = 0x1D;
    KBDLLHOOKSTRUCT right_ctrl = left_ctrl;
    right_ctrl.vkCode = VK_RCONTROL;
    right_ctrl.flags = LLKHF_EXTENDED;
    wchar_t right_expected[64] = {};
    GetKeyNameTextW( ( 0x1D << 16 ) | ( 1 << 24 ), right_expected, 64 );
    check( name_of_key( right_ctrl ) == right_expected, "Right Ctrl gets the name Windows gives the extended scan code." );
    check( name_of_key( left_ctrl ) != name_of_key( right_ctrl ), "Right Ctrl and Left Ctrl have different names." );

    KBDLLHOOKSTRUCT no_scan = {};
    no_scan.vkCode = 232;
    check( name_of_key( no_scan ) == L"Key 232", "A key with scan code 0 is named \"Key\" and its code in decimal." );

    DWORD nameless_scan = 0;
    for ( DWORD scan = 1; scan < 0x80 && !nameless_scan; scan++ ) {
        wchar_t name[64] = {};
        if ( GetKeyNameTextW( scan << 16, name, 64 ) == 0 ) nameless_scan = scan;
    }
    if ( nameless_scan ) {
        KBDLLHOOKSTRUCT nameless = {};
        nameless.vkCode = 233;
        nameless.scanCode = nameless_scan;
        check( name_of_key( nameless ) == L"Key 233", "A key whose scan code Windows has no name for is named \"Key\" and its code in decimal." );
    }
}

static void check_settings_file()
{
    auto full = load_text( "65 | A | Notepad | Microsoft.WindowsNotepad_8wekyb3d8bbwe!App | C:\\Tools\\thing.exe\n" );
    check( full.size() == 1 && is_binding( full[0], 65, L"A", L"Notepad", L"Microsoft.WindowsNotepad_8wekyb3d8bbwe!App", L"C:\\Tools\\thing.exe" ),
           "A full line reads as one binding with all five fields." );

    auto skipped = load_text( "# a comment\n\n   \n66|B|App B|id.b|\n# 67 | C | App C | id.c |\n" );
    check( skipped.size() == 1 && skipped[0].key == 66, "Comment lines and blank lines are skipped." );

    auto bom = load_text( "\xEF\xBB\xBF" "68 | D | App D | id.d | \n" );
    check( bom.size() == 1 && bom[0].key == 68 && bom[0].key_name == L"D", "A UTF-8 byte-order mark at the start is ignored." );

    auto trimmed = load_text( "   69   |   Right Ctrl   |  My App  |  id.e  |  C:\\My Apps\\e.exe  \n" );
    check( trimmed.size() == 1 && is_binding( trimmed[0], 69, L"Right Ctrl", L"My App", L"id.e", L"C:\\My Apps\\e.exe" ),
           "Spaces around each field are trimmed and spaces inside fields are kept." );

    auto hex = load_text( "0xA3 | Right Ctrl | App | id.f | \n" );
    check( hex.size() == 1 && hex[0].key == 0xA3, "A key code written in hex with 0x is read." );

    auto limits = load_text( "0 | Zero | App | id.g | \n256 | Big | App | id.h | \n0x100 | Big | App | id.i | \n255 | Top | App | id.j | \n1 | Low | App | id.k | \n" );
    check( limits.size() == 2 && limits[0].key == 255 && limits[1].key == 1,
           "Lines with key 0 or above 255 are skipped, while keys 1 and 255 are kept." );

    auto no_id = load_text( "70 | F | App | | C:\\x.exe\n71 | G | App |   | \n72 | H | App | id.l | \n" );
    check( no_id.size() == 1 && no_id[0].key == 72, "A line whose app ID is empty is skipped." );

    auto shortened = load_text( "73 | I | App I | id.m\n" );
    check( shortened.size() == 1 && is_binding( shortened[0], 73, L"I", L"App I", L"id.m", L"" ),
           "A line that stops after the app ID reads with an empty program path." );

    auto too_short = load_text( "74 | J\n" );
    check( too_short.empty(), "A line that stops before the app ID gives no binding, since its app ID is empty." );

    auto no_bar = load_text( "75\nhello there\n76 | K | App K | id.n | \n" );
    check( no_bar.size() == 1 && no_bar[0].key == 76, "A line with no '|' at all is not a binding." );

    auto order = load_text( "80 | P | App P | id.p | \n78 | N | App N | id.q | \n79 | O | App O | id.r | \n" );
    check( order.size() == 3 && order[0].key == 80 && order[1].key == 78 && order[2].key == 79,
           "Bindings are read in the order of the file." );

    auto accents = load_text( "77 | \xC3\x89 | Caf\xC3\xA9 | id.\xC3\xBC | C:\\Programmes\\Caf\xC3\xA9\\caf\xC3\xA9.exe\n" );
    check( accents.size() == 1 && is_binding( accents[0], 77, L"\u00C9", L"Caf\u00E9", L"id.\u00FC", L"C:\\Programmes\\Caf\u00E9\\caf\u00E9.exe" ),
           "Accented letters in a UTF-8 file are read correctly." );

    wstring missing = temp_file( L"summon_test_no_such_file.txt" );
    DeleteFileW( missing.c_str() );
    check( load_bindings_from( missing ).empty(), "A missing file gives no bindings." );

    check( load_text( "" ).empty(), "An empty file gives no bindings." );

    auto crlf_full = load_text( "65 | A | Notepad | id.a | C:\\Tools\\thing.exe\r\n66 | B | App B | id.b | C:\\Tools\\b.exe\r\n" );
    check( crlf_full.size() == 2 && is_binding( crlf_full[0], 65, L"A", L"Notepad", L"id.a", L"C:\\Tools\\thing.exe" )
           && is_binding( crlf_full[1], 66, L"B", L"App B", L"id.b", L"C:\\Tools\\b.exe" ),
           "Lines ending in CRLF read with no carriage return left in the program path." );

    auto crlf_skipped = load_text( "# a comment\r\n\r\n   \r\n67 | C | App C | id.c | \r\n" );
    check( crlf_skipped.size() == 1 && crlf_skipped[0].key == 67, "Comment lines and blank lines ending in CRLF are skipped." );

    auto crlf_bom = load_text( "\xEF\xBB\xBF" "68 | D | App D | id.d | \r\n" );
    check( crlf_bom.size() == 1 && crlf_bom[0].key == 68, "A byte-order mark is ignored in a file with CRLF line endings." );

    auto crlf_short = load_text( "73 | I | App I | id.m\r\n" );
    check( crlf_short.size() == 1 && is_binding( crlf_short[0], 73, L"I", L"App I", L"id.m", L"" ),
           "A CRLF line that stops after the app ID reads with an app ID free of a carriage return and an empty program path." );

    auto crlf_no_id = load_text( "74 | J | App J | \r\n" );
    check( crlf_no_id.empty(), "A CRLF line whose app ID is empty is skipped." );

    auto crlf_hex = load_text( "0xA3 | Right Ctrl | App | id.f\r\n" );
    check( crlf_hex.size() == 1 && crlf_hex[0].key == 0xA3, "A hex key code is read on a line ending in CRLF." );

    auto mixed_endings = load_text( "80 | P | App P | id.p | \r\n81 | Q | App Q | id.q | \n82 | R | App R | id.r | " );
    check( mixed_endings.size() == 3 && mixed_endings[2].key == 82 && mixed_endings[2].app_id == L"id.r",
           "A file mixing CRLF and LF lines, with no newline at the end, gives every binding." );
}

static void check_old_settings_file()
{
    auto old = load_text( "key = 163\nname = Right Ctrl\n" );
    check( old.size() == 1 && is_binding( old[0], 163, L"Right Ctrl", L"Claude", L"Claude_pzs8sxrjxfjjc!Claude", L"" ),
           "An old Claude Toggle file gives one binding for Claude with its key and name." );

    auto old_comment = load_text( "# Claude Toggle\nkey = 165\nname = Right Alt\n" );
    check( old_comment.size() == 1 && old_comment[0].key == 165 && old_comment[0].key_name == L"Right Alt",
           "An old file with a comment line still gives its binding." );

    auto old_zero = load_text( "key = 0\nname = Nothing\n" );
    check( old_zero.empty(), "An old file whose key is 0 gives no binding." );

    auto old_big = load_text( "key = 300\nname = Nothing\n" );
    check( old_big.empty(), "An old file whose key is above 255 gives no binding." );

    auto mixed = load_text( "key = 163\nname = Right Ctrl\n90 | Z | App Z | id.z | \n" );
    check( mixed.size() == 1 && is_binding( mixed[0], 90, L"Z", L"App Z", L"id.z", L"" ),
           "When a file has new-form bindings, the old key and name lines are ignored." );

    auto name_only = load_text( "name = Right Ctrl\n" );
    check( name_only.empty(), "An old file with a name line and no key line gives no binding." );

    auto tight = load_text( "key=163\nname=Right Ctrl\n" );
    check( tight.size() == 1 && tight[0].key == 163 && tight[0].key_name == L"Right Ctrl",
           "Old key and name lines with no spaces around = are read." );

    auto wide = load_text( "key    =    163\nname   =   Right Ctrl\n" );
    check( wide.size() == 1 && wide[0].key == 163 && wide[0].key_name == L"Right Ctrl",
           "Old key and name lines with several spaces around = are read." );

    auto lopsided = load_text( "key= 163\nname =Right Ctrl\n" );
    check( lopsided.size() == 1 && lopsided[0].key == 163 && lopsided[0].key_name == L"Right Ctrl",
           "Old key and name lines with a space on only one side of = are read." );

    auto old_hex = load_text( "key = 0xA5\nname = Right Alt\n" );
    check( old_hex.size() == 1 && old_hex[0].key == 0xA5 && old_hex[0].key_name == L"Right Alt",
           "An old key line written in hex with 0x is read." );

    auto old_hex_big = load_text( "key = 0x100\nname = Nothing\n" );
    check( old_hex_big.empty(), "An old key line in hex above 255 gives no binding." );

    auto old_crlf = load_text( "key = 163\r\nname = Right Ctrl\r\n" );
    check( old_crlf.size() == 1 && is_binding( old_crlf[0], 163, L"Right Ctrl", L"Claude", L"Claude_pzs8sxrjxfjjc!Claude", L"" ),
           "An old file with CRLF line endings gives its binding with no carriage return in the key name." );

    auto old_mixed_crlf = load_text( "key = 163\r\nname = Right Ctrl\r\n90 | Z | App Z | id.z | \r\n" );
    check( old_mixed_crlf.size() == 1 && old_mixed_crlf[0].key == 90,
           "In a CRLF file with new-form bindings, the old lines are ignored." );
}

static void check_saving()
{
    vector<binding> bindings = {
        { VK_RCONTROL, L"Right Ctrl", L"Claude", L"Claude_pzs8sxrjxfjjc!Claude", L"" },
        { VK_XBUTTON1, L"Mouse back button", L"Caf\u00E9 \u00C9diteur", L"Some.App_abc!Main", L"C:\\Program Files\\Caf\u00E9 App\\\u00E9dit.exe" },
        { 0x41, L"A", L"Notepad++", L"{6D809377-6AF0-444B-8957-A3773F02200E}\\Notepad++\\notepad++.exe", L"C:\\Program Files\\Notepad++\\notepad++.exe" },
        { 255, L"Key 255", L"\u65E5\u672C\u8A9E App", L"id.with spaces", L"D:\\a b\\c d.exe" },
    };
    wstring path = temp_file( L"summon_test_saved.txt" );
    bool saved = save_bindings_to( path, bindings );
    check( saved, "Saving bindings to a file in %TEMP% succeeds." );
    auto loaded = load_bindings_from( path );
    bool all_same = loaded.size() == bindings.size();
    for ( size_t i = 0; all_same && i < bindings.size(); i++ ) all_same = same( loaded[i], bindings[i] );
    check( all_same, "Saved bindings read back the same, in order, including accented letters and spaces in names and paths." );

    save_bindings_to( path, {} );
    check( load_bindings_from( path ).empty(), "A saved empty list reads back as empty." );
    DeleteFileW( path.c_str() );
}

static void check_row_using_key()
{
    vector<binding> bindings = {
        { 65, L"A", L"App A", L"id.a", L"" },
        { 66, L"B", L"App B", L"id.b", L"" },
        { 67, L"C", L"App C", L"id.c", L"" },
    };
    check( row_using_key( bindings, 66, -1 ) == 1, "row_using_key finds the row that uses a key." );
    check( row_using_key( bindings, 66, 0 ) == 1, "row_using_key finds a row using the key when a different row is being changed." );
    check( row_using_key( bindings, 66, 1 ) == -1, "row_using_key leaves out the row being changed." );
    check( row_using_key( bindings, 70, -1 ) == -1, "row_using_key gives -1 when no row uses the key." );
    check( row_using_key( {}, 65, -1 ) == -1, "row_using_key gives -1 for an empty list." );

    bindings.push_back( { 65, L"A", L"App D", L"id.d", L"" } );
    check( row_using_key( bindings, 65, 0 ) == 3, "row_using_key finds another row using the same key as the row being changed." );
}

static void check_choose_action()
{
    check( choose_action( { false, false, false, false, false } ) == toggle_action::launch, "With no window found, the app is started." );
    check( choose_action( { false, true, true, false, true } ) == toggle_action::launch, "With no window found, the app is started whatever the other fields say." );
    check( choose_action( { true, true, true, false, true } ) == toggle_action::hide, "A visible, front, restored window on this desktop is hidden." );
    check( choose_action( { true, false, true, false, true } ) == toggle_action::bring_here, "A hidden window is brought here." );
    check( choose_action( { true, true, false, false, true } ) == toggle_action::bring_here, "A window behind other windows is brought here." );
    check( choose_action( { true, true, true, true, true } ) == toggle_action::bring_here, "A minimised window is brought here." );
    check( choose_action( { true, true, true, false, false } ) == toggle_action::bring_here, "A window on another desktop is brought here." );
    check( choose_action( { true, false, false, true, false } ) == toggle_action::bring_here, "A hidden, minimised window behind others on another desktop is brought here." );
}

static void check_window_matching()
{
    wstring exe = own_exe_path();
    binding by_path = { 0x41, L"A", L"Test", L"", exe };
    binding by_path_upper = { 0x41, L"A", L"Test", L"", upper( exe ) };
    binding other_path = { 0x41, L"A", L"Test", L"", L"C:\\Windows\\System32\\notepad.exe" };
    binding empty_app = { 0x41, L"A", L"Test", L"", L"" };

    HWND plain = hidden_window( L"Summon test window" );
    check( plain != nullptr && !IsWindowVisible( plain ), "The test window is created hidden." );
    check( window_belongs_to( plain, by_path ), "A titled, unowned window belongs to the app whose program path is the program that made it." );
    check( window_belongs_to( plain, by_path_upper ), "Program paths are compared ignoring letter case." );
    check( !window_belongs_to( plain, other_path ), "A window does not belong to an app with a different program path." );
    check( !window_belongs_to( plain, empty_app ), "An app with an empty app ID and program path matches no window." );

    HWND untitled = hidden_window( L"" );
    check( !window_belongs_to( untitled, by_path ), "A window with no title belongs to no app." );

    HWND tool = hidden_window( L"Summon test tool window", WS_EX_TOOLWINDOW );
    check( !window_belongs_to( tool, by_path ), "A tool window belongs to no app." );

    HWND owned = hidden_window( L"Summon test owned window", 0, plain );
    check( GetWindow( owned, GW_OWNER ) == plain, "The owned test window has an owner." );
    check( !window_belongs_to( owned, by_path ), "A window with an owner belongs to no app." );

    HWND tagged = hidden_window( L"Summon test tagged window" );
    bool tag_set = set_window_app_id( tagged, L"Summon.Test.TaggedApp" );
    check( tag_set, "An app ID can be set on a hidden test window." );
    binding by_id = { 0x41, L"A", L"Test", L"Summon.Test.TaggedApp", L"" };
    binding by_id_lower = { 0x41, L"A", L"Test", L"summon.test.taggedapp", L"" };
    binding other_id = { 0x41, L"A", L"Test", L"Summon.Test.OtherApp", L"" };
    check( window_belongs_to( tagged, by_id ), "A window belongs to the app whose ID is set on the window." );
    check( window_belongs_to( tagged, by_id_lower ), "The app ID set on a window is compared ignoring letter case." );
    check( !window_belongs_to( tagged, other_id ), "A window does not belong to an app whose ID differs from the one set on it." );
    check( !window_belongs_to( tagged, empty_app ), "An empty app ID does not match a window with an app ID set on it." );
    check( window_belongs_to( tagged, by_path ), "A window with an app ID set on it still belongs to the app with its program path." );
    set_window_app_id( tagged, nullptr );

    DestroyWindow( tagged );
    DestroyWindow( owned );
    DestroyWindow( tool );
    DestroyWindow( untitled );
    DestroyWindow( plain );
}

static wstring known_folder( const wchar_t *guid_text )
{
    GUID guid;
    if ( FAILED( CLSIDFromString( guid_text, &guid ) ) ) return L"";
    PWSTR path = nullptr;
    wstring result;
    if ( SUCCEEDED( SHGetKnownFolderPath( guid, 0, nullptr, &path ) ) ) result = path;
    CoTaskMemFree( path );
    return result;
}

static void check_app_ids()
{
    check( path_from_app_id( L"C:\\Tools\\thing.exe" ) == L"C:\\Tools\\thing.exe", "An app ID that is a full path to a program stays as it is." );

    wstring program_files = known_folder( L"{6D809377-6AF0-444B-8957-A3773F02200E}" );
    check( !program_files.empty()
           && path_from_app_id( L"{6D809377-6AF0-444B-8957-A3773F02200E}\\Notepad++\\notepad++.exe" ) == program_files + L"\\Notepad++\\notepad++.exe",
           "An app ID made of the Program Files GUID and a path becomes Program Files joined to that path." );

    wstring system = known_folder( L"{1AC14E77-02E7-4E5D-B744-2EB1AE5198B7}" );
    check( !system.empty()
           && path_from_app_id( L"{1AC14E77-02E7-4E5D-B744-2EB1AE5198B7}\\notepad.exe" ) == system + L"\\notepad.exe",
           "An app ID made of the System GUID and a file name becomes the System folder joined to that name." );

    check( path_from_app_id( L"Chrome" ).empty(), "A plain name such as Chrome gives an empty path." );
    check( path_from_app_id( L"Microsoft.WindowsNotepad_8wekyb3d8bbwe!App" ).empty(), "A packaged app ID gives an empty path." );
    check( path_from_app_id( L"C:\\Tools\\notes.txt" ).empty(), "A path to a file that is not .exe gives an empty path." );
    check( path_from_app_id( L"{00000000-0000-0000-0000-000000000000}\\Tools\\thing.exe" ).empty(),
           "An ID whose braces hold a GUID that is not a known folder gives an empty path." );
    check( path_from_app_id( L"{not-a-guid}\\Tools\\thing.exe" ).empty(), "An ID whose braces hold something other than a GUID gives an empty path." );
    check( path_from_app_id( L"" ).empty(), "An empty app ID gives an empty path." );
}

static void check_app_list()
{
    auto apps = list_installed_apps();
    check( !apps.empty(), "The app list is not empty." );

    bool all_named = true;
    for ( auto &app : apps ) if ( app.name.empty() || app.app_id.empty() ) all_named = false;
    check( all_named, "Every app in the list has a display name and an app ID." );

    bool sorted = true;
    for ( size_t i = 1; i < apps.size(); i++ )
        if ( _wcsicmp( apps[i - 1].name.c_str(), apps[i].name.c_str() ) > 0 ) sorted = false;
    check( sorted, "The app list is sorted by name ignoring case." );

    bool has_claude = false;
    for ( auto &app : apps ) if ( app.name == L"Claude" && app.app_id == L"Claude_pzs8sxrjxfjjc!Claude" ) has_claude = true;
    check( has_claude, "The app list includes Claude with app ID Claude_pzs8sxrjxfjjc!Claude." );
}

static void check_explorer()
{
    check( desktop_interfaces_work(), "Explorer answers to the virtual desktop interface IDs Summon uses." );

    HWND hidden = hidden_window( L"Summon test desktop window" );
    check( taskbar_app_id( hidden ).empty(), "A hidden window has an empty taskbar app ID." );
    LONG_PTR style_before = GetWindowLongPtrW( hidden, GWL_STYLE );
    bool moved = move_to_current_desktop( hidden );
    check( !moved, "Moving a hidden window to the current desktop returns false." );
    check( !IsWindowVisible( hidden ) && GetWindowLongPtrW( hidden, GWL_STYLE ) == style_before,
           "Moving a hidden window to the current desktop leaves it hidden and unchanged." );
    DestroyWindow( hidden );
}

int wmain()
{
    CoInitializeEx( nullptr, COINIT_APARTMENTTHREADED );

    WNDCLASSW wc = {};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW( nullptr );
    wc.lpszClassName = window_class;
    RegisterClassW( &wc );

    target = CreateWindowExW( 0, window_class, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr );
    set_hook_target( target, fire_message );

    check_keys();
    check_mouse_buttons();
    check_bound_modifier();
    check_bound_ordinary_key();
    check_bound_mouse_buttons();
    check_injected_input();
    check_set_bound_keys();
    check_key_names();
    check_settings_file();
    check_old_settings_file();
    check_saving();
    check_row_using_key();
    check_choose_action();
    check_window_matching();
    check_app_ids();
    check_app_list();
    check_explorer();

    DestroyWindow( target );
    CoUninitialize();

    printf( any_failed ? "Some checks failed.\n" : "All checks passed.\n" );
    return any_failed ? 1 : 0;
}

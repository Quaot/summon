// Checks Summon's logic without touching the screen, keyboard or mouse. Key presses are fake
// hook events handed straight to the hook handlers, and windows are hidden ones made here.
// Prints one line per check and returns 1 if any failed.
//
//   tests\test.cmd     builds tests\summon_test.exe and runs the checks

#include <windows.h>
#include <knownfolders.h>
#include <objbase.h>
#include <propkey.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <cstdio>
#include <vector>

#include "../src/app_window.h"
#include "../src/apps.h"
#include "../src/bindings.h"
#include "../src/hooks.h"
#include "../src/key_picker.h"
#include "../src/virtual_desktops.h"

using std::vector;

namespace {

int failures = 0;

void check( bool passed, const char *what ) {
    std::printf( "%s  %s\n", passed ? "pass" : "FAIL", what );
    if ( !passed ) ++failures;
}

wstring temp_file( const wchar_t *name ) {
    wchar_t folder[ MAX_PATH ];
    GetTempPathW( MAX_PATH, folder );
    return wstring( folder ) + name;
}

void write_file( const wstring &path, const char *bytes ) {
    FILE *file = _wfopen( path.c_str(), L"wb" );
    std::fputs( bytes, file );
    std::fclose( file );
}

bool same_binding( const binding &a, const binding &b ) {
    return a.key == b.key && a.key_name == b.key_name && a.app_name == b.app_name && a.app_id == b.app_id &&
           a.exe_path == b.exe_path;
}

// The settings file

void check_bindings_file() {
    wstring path = temp_file( L"summon_test_settings.txt" );

    vector<binding> saved( 2 );
    saved[ 0 ] = { VK_RCONTROL, L"Right Ctrl", L"Claude", L"Claude_pzs8sxrjxfjjc!Claude", L"" };
    saved[ 1 ] = { VK_XBUTTON1, L"Mouse back button", L"Caf\u00e9 Notes", L"C:\\My Tools\\Desktop\\Caf\u00e9.lnk",
                   L"C:\\Program Files\\Caf\u00e9\\cafe.exe" };
    check( save_bindings_to( path, saved ), "bindings save to a file" );
    vector<binding> loaded = load_bindings_from( path );
    check( loaded.size() == 2, "both saved bindings load back" );
    check( loaded.size() == 2 && same_binding( loaded[ 0 ], saved[ 0 ] ), "a binding with an empty program path loads back unchanged" );
    check( loaded.size() == 2 && same_binding( loaded[ 1 ], saved[ 1 ] ), "accents and spaces in names and paths survive a save and load" );

    write_file( path, "# a comment\r\n\r\n   \r\n# key code | key name | app name | app id | exe path\r\n"
                      "163 | Right Ctrl | Claude | Claude_pzs8sxrjxfjjc!Claude |\r\n" );
    loaded = load_bindings_from( path );
    check( loaded.size() == 1, "comment lines and blank lines are skipped" );

    write_file( path, "\xEF\xBB\xBF" "120 | F9 | Notepad | Microsoft.WindowsNotepad_8wekyb3d8bbwe!App |\r\n" );
    loaded = load_bindings_from( path );
    check( loaded.size() == 1 && loaded[ 0 ].key == 120, "a binding on the first line after a byte-order mark loads" );

    write_file( path, "  0xA3   |   Right Ctrl   |  Claude  |  Claude_pzs8sxrjxfjjc!Claude  |  \r\n" );
    loaded = load_bindings_from( path );
    check( loaded.size() == 1 && loaded[ 0 ].key == VK_RCONTROL, "a key code written in hex loads" );
    check( loaded.size() == 1 && loaded[ 0 ].app_name == L"Claude" && loaded[ 0 ].app_id == L"Claude_pzs8sxrjxfjjc!Claude",
           "spaces around each field are trimmed" );

    write_file( path, "119 | F8 | Notepad | Microsoft.WindowsNotepad_8wekyb3d8bbwe!App\r\n" );
    loaded = load_bindings_from( path );
    check( loaded.size() == 1 && loaded[ 0 ].exe_path.empty(), "a line without the program path field still loads" );

    write_file( path, "0 | None | Claude | Claude_pzs8sxrjxfjjc!Claude |\r\n"
                      "300 | Too big | Claude | Claude_pzs8sxrjxfjjc!Claude |\r\n"
                      "119 | F8 | Notepad | |\r\n"
                      "nonsense without bars\r\n"
                      "120 | F9 | Notepad | Microsoft.WindowsNotepad_8wekyb3d8bbwe!App |\r\n" );
    loaded = load_bindings_from( path );
    check( loaded.size() == 1 && loaded[ 0 ].key == 120, "lines with a zero key, a key over 255, no app ID or no bars are skipped" );

    write_file( path, "# Change the key with: claude_toggle --set-key\r\nkey = 163\r\nname = Right Ctrl\r\n" );
    loaded = load_bindings_from( path );
    check( loaded.size() == 1 && loaded[ 0 ].key == VK_RCONTROL && loaded[ 0 ].key_name == L"Right Ctrl" &&
               loaded[ 0 ].app_name == L"Claude" && loaded[ 0 ].app_id == L"Claude_pzs8sxrjxfjjc!Claude",
           "an old Claude Toggle settings file becomes a Claude binding" );

    write_file( path, "key = 5\r\n120 | F9 | Notepad | Microsoft.WindowsNotepad_8wekyb3d8bbwe!App |\r\n" );
    loaded = load_bindings_from( path );
    check( loaded.size() == 1 && loaded[ 0 ].key == 120, "an old key line is ignored when the file also has new bindings" );

    DeleteFileW( path.c_str() );
    check( load_bindings_from( path ).empty(), "a missing settings file gives no bindings" );

    check( save_bindings_to( path, {} ) && load_bindings_from( path ).empty(), "an empty list saves and loads as empty" );
    DeleteFileW( path.c_str() );
}

void check_modifiers() {
    bool all_modifiers = true;
    for ( DWORD key : { VK_LSHIFT, VK_RSHIFT, VK_LCONTROL, VK_RCONTROL, VK_LMENU, VK_RMENU, VK_LWIN, VK_RWIN } )
        all_modifiers = all_modifiers && is_modifier( key );
    check( all_modifiers, "both Shifts, Ctrls, Alts and Win keys count as modifiers" );
    check( !is_modifier( VK_SNAPSHOT ) && !is_modifier( 'A' ) && !is_modifier( VK_F8 ), "PrtSc, letters and F keys are not modifiers" );
    check( !is_modifier( VK_MBUTTON ) && !is_modifier( VK_XBUTTON1 ) && !is_modifier( VK_XBUTTON2 ), "mouse buttons are not modifiers" );
}

void check_key_owner() {
    vector<binding> list( 3 );
    list[ 0 ] = { VK_RCONTROL, L"Right Ctrl", L"Claude", L"a", L"" };
    list[ 1 ] = { VK_XBUTTON1, L"Mouse back button", L"Discord", L"b", L"" };
    list[ 2 ] = { VK_F8, L"F8", L"Notepad", L"c", L"" };
    check( row_using_key( list, VK_XBUTTON1, -1 ) == 1, "the row already using a key is found" );
    check( row_using_key( list, VK_XBUTTON1, 1 ) == -1, "the row being changed does not count as using its own key" );
    check( row_using_key( list, VK_F9, -1 ) == -1, "an unused key belongs to no row" );
}

// The hooks

const UINT fired_message = WM_APP + 77;
HWND target = nullptr;

vector<DWORD> fired_keys() {
    vector<DWORD> keys;
    MSG message;
    while ( PeekMessageW( &message, target, fired_message, fired_message, PM_REMOVE ) ) keys.push_back( static_cast<DWORD>( message.wParam ) );
    return keys;
}

bool key( DWORD code, bool down, bool injected = false, bool system = false ) {
    KBDLLHOOKSTRUCT event = {};
    event.vkCode = code;
    event.flags = injected ? LLKHF_INJECTED : 0;
    if ( !down ) event.flags |= LLKHF_UP;
    WPARAM message = down ? ( system ? WM_SYSKEYDOWN : WM_KEYDOWN ) : ( system ? WM_SYSKEYUP : WM_KEYUP );
    return handle_keyboard_event( message, event );
}

bool mouse( WPARAM message, WORD x_button = 0, bool injected = false ) {
    MSLLHOOKSTRUCT event = {};
    event.mouseData = static_cast<DWORD>( x_button ) << 16;
    event.flags = injected ? LLMHF_INJECTED : 0;
    return handle_mouse_event( message, event );
}

void check_hooks() {
    target = CreateWindowExW( 0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandleW( nullptr ), nullptr );
    set_hook_target( target, fired_message );

    set_bound_keys( { VK_RCONTROL } );
    bool swallowed = key( VK_RCONTROL, true );
    check( fired_keys().empty() && !swallowed, "pressing a bound modifier fires nothing yet and lets the press through" );
    swallowed = key( VK_RCONTROL, false );
    vector<DWORD> fired = fired_keys();
    check( fired.size() == 1 && fired[ 0 ] == VK_RCONTROL && !swallowed, "releasing it alone fires once and lets the release through" );

    key( VK_RCONTROL, true );
    swallowed = key( 'C', true );
    key( 'C', false );
    key( VK_RCONTROL, false );
    check( fired_keys().empty() && !swallowed, "Right Ctrl+C fires nothing and C still reaches other programs" );

    key( VK_RCONTROL, true );
    key( VK_RCONTROL, true );
    key( VK_RCONTROL, true );
    key( VK_RCONTROL, false );
    check( fired_keys().size() == 1, "holding a modifier until it repeats still fires once on release" );

    key( VK_RCONTROL, true );
    key( VK_RCONTROL, false );
    key( VK_RCONTROL, true );
    key( VK_RCONTROL, false );
    check( fired_keys().size() == 2, "two taps fire twice" );

    key( VK_RCONTROL, true );
    swallowed = mouse( WM_LBUTTONDOWN );
    mouse( WM_LBUTTONUP );
    key( VK_RCONTROL, false );
    check( fired_keys().empty() && !swallowed, "a click while the modifier is held cancels the tap and the click goes through" );

    key( VK_RCONTROL, true );
    mouse( WM_MOUSEWHEEL );
    key( VK_RCONTROL, false );
    check( fired_keys().empty(), "scrolling while the modifier is held cancels the tap" );

    key( VK_RCONTROL, true );
    mouse( WM_MOUSEMOVE );
    key( VK_RCONTROL, false );
    check( fired_keys().size() == 1, "moving the mouse while the modifier is held does not cancel the tap" );

    key( VK_RCONTROL, true );
    key( 'F', true, false, true );
    key( VK_RCONTROL, false );
    check( fired_keys().empty(), "a system key press in between cancels the tap" );

    key( VK_RCONTROL, true, true );
    key( VK_RCONTROL, false, true );
    check( fired_keys().empty(), "injected key presses, such as Summon's own, are ignored" );

    key( VK_LCONTROL, true );
    swallowed = key( VK_LCONTROL, false );
    check( fired_keys().empty() && !swallowed, "an unbound modifier fires nothing" );

    set_bound_keys( { VK_F8 } );
    swallowed = key( VK_F8, true );
    fired = fired_keys();
    check( fired.size() == 1 && fired[ 0 ] == VK_F8 && swallowed, "a bound ordinary key fires on press and the press is swallowed" );
    bool repeat_swallowed = key( VK_F8, true );
    check( fired_keys().empty() && repeat_swallowed, "its auto-repeat is swallowed without firing again" );
    swallowed = key( VK_F8, false );
    check( fired_keys().empty() && swallowed, "its release is swallowed and fires nothing" );

    swallowed = key( VK_F9, true );
    key( VK_F9, false );
    check( fired_keys().empty() && !swallowed, "an unbound ordinary key fires nothing and goes through" );

    swallowed = key( VK_F8, true, true );
    key( VK_F8, false, true );
    check( fired_keys().empty() && !swallowed, "an injected press of a bound key is ignored and goes through" );

    set_bound_keys( { VK_RCONTROL, VK_F8 } );
    key( VK_RCONTROL, true );
    key( VK_F8, true );
    key( VK_F8, false );
    key( VK_RCONTROL, false );
    fired = fired_keys();
    check( fired.size() == 1 && fired[ 0 ] == VK_F8, "a bound key pressed while a bound modifier is held fires, and the modifier does not" );

    set_bound_keys( { VK_XBUTTON1, VK_MBUTTON } );
    swallowed = mouse( WM_XBUTTONDOWN, XBUTTON1 );
    fired = fired_keys();
    check( fired.size() == 1 && fired[ 0 ] == VK_XBUTTON1 && swallowed, "a bound back button fires on press and the press is swallowed" );
    swallowed = mouse( WM_XBUTTONUP, XBUTTON1 );
    check( fired_keys().empty() && swallowed, "its release is swallowed too" );
    swallowed = mouse( WM_XBUTTONDOWN, XBUTTON2 );
    mouse( WM_XBUTTONUP, XBUTTON2 );
    check( fired_keys().empty() && !swallowed, "the unbound forward button goes through" );
    swallowed = mouse( WM_MBUTTONDOWN );
    mouse( WM_MBUTTONUP );
    fired = fired_keys();
    check( fired.size() == 1 && fired[ 0 ] == VK_MBUTTON && swallowed, "a bound middle button fires" );
    swallowed = mouse( WM_XBUTTONDOWN, XBUTTON1, true );
    check( fired_keys().empty() && !swallowed, "an injected mouse button is ignored" );
    mouse( WM_XBUTTONUP, XBUTTON1, true );

    set_bound_keys( {} );
    swallowed = key( VK_F8, true );
    key( VK_F8, false );
    swallowed = mouse( WM_XBUTTONDOWN, XBUTTON1 ) || swallowed;
    mouse( WM_XBUTTONUP, XBUTTON1 );
    check( fired_keys().empty() && !swallowed, "after every binding is removed, nothing fires or is swallowed" );

    MSLLHOOKSTRUCT event = {};
    check( mouse_button_key( WM_MBUTTONDOWN, &event ) == VK_MBUTTON && mouse_button_key( WM_MBUTTONUP, &event ) == VK_MBUTTON,
           "middle button messages map to the middle button" );
    event.mouseData = XBUTTON1 << 16;
    check( mouse_button_key( WM_XBUTTONDOWN, &event ) == VK_XBUTTON1, "the first side button is the back button" );
    event.mouseData = XBUTTON2 << 16;
    check( mouse_button_key( WM_XBUTTONUP, &event ) == VK_XBUTTON2, "the second side button is the forward button" );
    check( mouse_button_key( WM_LBUTTONDOWN, &event ) == 0 && mouse_button_key( WM_RBUTTONDOWN, &event ) == 0 &&
               mouse_button_key( WM_MOUSEWHEEL, &event ) == 0,
           "left, right and wheel are never treated as bindable buttons" );
    check( is_button_down( WM_MBUTTONDOWN ) && is_button_down( WM_XBUTTONDOWN ) && !is_button_down( WM_XBUTTONUP ),
           "button presses and releases are told apart" );

    DestroyWindow( target );
}

// Showing and hiding

void check_choices() {
    window_state none = {};
    check( choose_action( none ) == toggle_action::launch, "with no window found, the app is started" );

    window_state in_front = { true, true, true, false, true };
    check( choose_action( in_front ) == toggle_action::hide, "a window in front on this desktop is hidden" );

    window_state minimised = in_front;
    minimised.minimised = true;
    check( choose_action( minimised ) == toggle_action::bring_here, "a minimised window is brought back" );

    window_state elsewhere = in_front;
    elsewhere.on_current_desktop = false;
    check( choose_action( elsewhere ) == toggle_action::bring_here, "a window on another desktop is brought here" );

    window_state behind = in_front;
    behind.in_front = false;
    check( choose_action( behind ) == toggle_action::bring_here, "a window behind others on this desktop is brought forward" );

    window_state hidden = { true, false, false, false, true };
    check( choose_action( hidden ) == toggle_action::bring_here, "a hidden window is shown again" );
}

HWND hidden_window( const wchar_t *title, DWORD ex_style = 0, HWND owner = nullptr ) {
    return CreateWindowExW( ex_style, L"STATIC", title, WS_OVERLAPPED, 0, 0, 100, 100, owner, nullptr,
                            GetModuleHandleW( nullptr ), nullptr );
}

void set_window_app_id( HWND hwnd, const wchar_t *id ) {
    IPropertyStore *store = nullptr;
    if ( FAILED( SHGetPropertyStoreForWindow( hwnd, IID_PPV_ARGS( &store ) ) ) ) return;
    PROPVARIANT value = {};
    value.vt = VT_LPWSTR;
    value.pwszVal = const_cast<wchar_t *>( id );
    store->SetValue( PKEY_AppUserModel_ID, value );
    store->Commit();
    store->Release();
}

void check_window_matching() {
    wchar_t own_path[ MAX_PATH * 2 ];
    GetModuleFileNameW( nullptr, own_path, MAX_PATH * 2 );
    wstring upper = own_path;
    CharUpperBuffW( &upper[ 0 ], static_cast<DWORD>( upper.size() ) );

    binding by_path = { VK_F8, L"F8", L"Test", L"Summon.Test.NoSuchApp", own_path };
    binding by_upper_path = { VK_F8, L"F8", L"Test", L"Summon.Test.NoSuchApp", upper };
    binding other = { VK_F8, L"F8", L"Other", L"Summon.Test.Other", L"C:\\Nowhere\\other.exe" };
    binding empty = { VK_F8, L"F8", L"Empty", L"", L"" };

    HWND plain = hidden_window( L"Summon test window" );
    check( window_belongs_to( plain, by_path ), "a window matches the app whose program made it" );
    check( window_belongs_to( plain, by_upper_path ), "program paths match regardless of letter case" );
    check( !window_belongs_to( plain, other ), "a window does not match another app" );
    check( !window_belongs_to( plain, empty ), "an empty app ID and path match nothing" );

    HWND untitled = hidden_window( L"" );
    check( !window_belongs_to( untitled, by_path ), "a window without a title is never an app's main window" );
    HWND tool = hidden_window( L"Summon tool window", WS_EX_TOOLWINDOW );
    check( !window_belongs_to( tool, by_path ), "a tool window is never an app's main window" );
    HWND owned = hidden_window( L"Summon owned window", 0, plain );
    check( !window_belongs_to( owned, by_path ), "a window owned by another, such as a dialog, is never an app's main window" );

    HWND labelled = hidden_window( L"Summon labelled window" );
    set_window_app_id( labelled, L"Summon.Test.Labelled" );
    binding by_label = { VK_F8, L"F8", L"Labelled", L"Summon.Test.Labelled", L"C:\\Nowhere\\other.exe" };
    check( window_belongs_to( labelled, by_label ), "a window carrying an app ID matches that app" );
    check( !window_belongs_to( labelled, other ), "a window carrying an app ID does not match a different one" );

    for ( HWND hwnd : { owned, plain, untitled, tool, labelled } ) DestroyWindow( hwnd );
}

// Apps, key names and Explorer

void check_app_paths() {
    wchar_t *program_files = nullptr;
    SHGetKnownFolderPath( FOLDERID_ProgramFiles, 0, nullptr, &program_files );
    wstring expected = wstring( program_files ) + L"\\Notepad++\\notepad++.exe";
    CoTaskMemFree( program_files );
    check( path_from_app_id( L"{6D809377-6AF0-444B-8957-A3773F02200E}\\Notepad++\\notepad++.exe" ) == expected,
           "an app ID that starts with a known folder becomes a program path" );
    check( path_from_app_id( L"C:\\Tools\\thing.exe" ) == L"C:\\Tools\\thing.exe", "an app ID that is already a path stays as it is" );
    check( path_from_app_id( L"Chrome" ).empty(), "a plain app name is not a path" );
    check( path_from_app_id( L"Microsoft.WindowsNotepad_8wekyb3d8bbwe!App" ).empty(), "a packaged app ID is not a path" );
    check( path_from_app_id( L"{6D809377-6AF0-444B-8957-A3773F02200E}\\readme.txt" ).empty(), "a known-folder ID for a non-program is not a path" );
    check( path_from_app_id( L"{not-a-guid}\\thing.exe" ).empty(), "a broken folder ID is not a path" );
}

void check_installed_apps() {
    vector<installed_app> apps = list_installed_apps();
    check( apps.size() > 20, "the Start menu's app list is read" );
    bool sorted = true, complete = true, has_claude = false;
    for ( size_t index = 0; index < apps.size(); index++ ) {
        if ( index > 0 && _wcsicmp( apps[ index - 1 ].name.c_str(), apps[ index ].name.c_str() ) > 0 ) sorted = false;
        if ( apps[ index ].name.empty() || apps[ index ].app_id.empty() ) complete = false;
        if ( apps[ index ].app_id == L"Claude_pzs8sxrjxfjjc!Claude" ) has_claude = true;
    }
    check( sorted, "the app list is sorted by name, ignoring case" );
    check( complete, "every app in the list has a name and an app ID" );
    check( has_claude, "Claude is in the list with its package app ID (owner's machine)" );
}

void check_key_names() {
    check( name_of_mouse_button( VK_MBUTTON ) == L"Middle mouse button", "the middle button has a plain name" );
    check( name_of_mouse_button( VK_XBUTTON1 ) == L"Mouse back button", "the back button has a plain name" );
    check( name_of_mouse_button( VK_XBUTTON2 ) == L"Mouse forward button", "the forward button has a plain name" );

    KBDLLHOOKSTRUCT a_key = {};
    a_key.vkCode = 'A';
    a_key.scanCode = 0x1E;
    check( name_of_key( a_key ) == L"A", "a letter key is named by its letter" );
    KBDLLHOOKSTRUCT right_ctrl = {};
    right_ctrl.vkCode = VK_RCONTROL;
    right_ctrl.scanCode = 0x1D;
    right_ctrl.flags = LLKHF_EXTENDED;
    KBDLLHOOKSTRUCT left_ctrl = right_ctrl;
    left_ctrl.vkCode = VK_LCONTROL;
    left_ctrl.flags = 0;
    check( name_of_key( right_ctrl ) != name_of_key( left_ctrl ), "Right Ctrl and Left Ctrl get different names" );
    KBDLLHOOKSTRUCT nameless = {};
    nameless.vkCode = 0xE8;
    check( name_of_key( nameless ) == L"Key 232", "a key with no name is called by its number" );
}

void check_explorer() {
    check( desktop_interfaces_work(), "Explorer's virtual desktop interfaces answer on this Windows build" );
    HWND hidden = hidden_window( L"Summon hidden window" );
    check( taskbar_app_id( hidden ).empty(), "a hidden window has no taskbar app ID" );
    check( !move_to_current_desktop( hidden ), "a hidden window cannot be moved between desktops" );
    DestroyWindow( hidden );
}

}  // namespace

int wmain() {
    CoInitializeEx( nullptr, COINIT_APARTMENTTHREADED );

    check_bindings_file();
    check_modifiers();
    check_key_owner();
    check_hooks();
    check_choices();
    check_window_matching();
    check_app_paths();
    check_installed_apps();
    check_key_names();
    check_explorer();

    std::printf( "%s\n", failures ? "Some checks failed." : "All checks passed." );
    CoUninitialize();
    return failures ? 1 : 0;
}

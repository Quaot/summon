#pragma once

#include <windows.h>

#include <string>
#include <vector>

using std::wstring;

// One key and the app it shows and hides. Keys are virtual-key codes; mouse buttons use
// VK_MBUTTON, VK_XBUTTON1 and VK_XBUTTON2. app_id is the app's entry in the Start menu's
// app list, and exe_path is its program file when Windows reports one.
struct binding {
    DWORD key = 0;
    wstring key_name;
    wstring app_name;
    wstring app_id;
    wstring exe_path;
};

// load_bindings and save_bindings use settings.txt next to summon.exe.
std::vector<binding> load_bindings();
bool save_bindings( const std::vector<binding> &bindings );
std::vector<binding> load_bindings_from( const wstring &path );
bool save_bindings_to( const wstring &path, const std::vector<binding> &bindings );

// The row whose binding uses `key`, other than row `keep`, or -1 if there is none.
int row_using_key( const std::vector<binding> &bindings, DWORD key, int keep );

// Modifier keys fire on a lone tap and still work in shortcuts. Every other key fires on
// press and is kept from reaching other programs.
bool is_modifier( DWORD key );

wstring exe_path();
wstring exe_folder();

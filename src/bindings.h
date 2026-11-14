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

std::vector<binding> load_bindings();
bool save_bindings( const std::vector<binding> &bindings );

// Modifier keys fire on a lone tap and still work in shortcuts. Every other key fires on
// press and is kept from reaching other programs.
bool is_modifier( DWORD key );

wstring exe_path();
wstring exe_folder();

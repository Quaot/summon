#pragma once

#include <windows.h>

#include <string>
#include <vector>

using std::wstring;

// An entry in the Start menu's list of apps (the shell:AppsFolder folder).
struct installed_app {
    wstring name;
    wstring app_id;
    wstring exe_path;
};

// Every app, sorted by name.
std::vector<installed_app> list_installed_apps();

// Asks for a program or shortcut that is missing from the Start menu. A shortcut becomes the
// app ID, so it opens with its own folder and arguments; the program it points to identifies
// the app's windows.
bool browse_for_program( HWND owner, installed_app &chosen );

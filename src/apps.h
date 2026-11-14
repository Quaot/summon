#pragma once

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

#include "bindings.h"

#include <cstdio>
#include <cwchar>

using std::vector;

namespace {

wstring settings_path() {
    return exe_folder() + L"\\settings.txt";
}

wstring trimmed( const wstring &text ) {
    size_t first = text.find_first_not_of( L" \t\r\n\xFEFF" );
    if ( first == wstring::npos ) return L"";
    size_t last = text.find_last_not_of( L" \t\r\n" );
    return text.substr( first, last - first + 1 );
}

vector<wstring> split( const wstring &line ) {
    vector<wstring> fields;
    size_t start = 0;
    while ( true ) {
        size_t bar = line.find( L'|', start );
        fields.push_back( trimmed( line.substr( start, bar - start ) ) );
        if ( bar == wstring::npos ) break;
        start = bar + 1;
    }
    return fields;
}

}  // namespace

wstring exe_path() {
    wchar_t path[ MAX_PATH * 2 ];
    DWORD length = GetModuleFileNameW( nullptr, path, MAX_PATH * 2 );
    return wstring( path, length );
}

wstring exe_folder() {
    wstring path = exe_path();
    return path.substr( 0, path.find_last_of( L'\\' ) );
}

// settings.txt has one binding per line: key code | key name | app name | app id | exe path.
// Older files from Claude Toggle held a single "key = 163" line for Claude.
vector<binding> load_bindings() {
    vector<binding> bindings;
    FILE *file = _wfopen( settings_path().c_str(), L"r, ccs=UTF-8" );
    if ( !file ) return bindings;
    wchar_t buffer[ 2048 ];
    binding old_claude;
    while ( fgetws( buffer, 2048, file ) ) {
        wstring line = trimmed( buffer );
        if ( line.empty() || line[ 0 ] == L'#' ) continue;
        if ( line.find( L'|' ) != wstring::npos ) {
            vector<wstring> fields = split( line );
            fields.resize( 5 );
            binding entry;
            entry.key = wcstoul( fields[ 0 ].c_str(), nullptr, 0 );
            entry.key_name = fields[ 1 ];
            entry.app_name = fields[ 2 ];
            entry.app_id = fields[ 3 ];
            entry.exe_path = fields[ 4 ];
            if ( entry.key > 0 && entry.key < 256 && !entry.app_id.empty() ) bindings.push_back( entry );
        } else if ( line.rfind( L"key", 0 ) == 0 && line.find( L'=' ) != wstring::npos ) {
            old_claude.key = wcstoul( trimmed( line.substr( line.find( L'=' ) + 1 ) ).c_str(), nullptr, 0 );
        } else if ( line.rfind( L"name", 0 ) == 0 && line.find( L'=' ) != wstring::npos ) {
            old_claude.key_name = trimmed( line.substr( line.find( L'=' ) + 1 ) );
        }
    }
    fclose( file );
    if ( bindings.empty() && old_claude.key > 0 && old_claude.key < 256 ) {
        old_claude.app_name = L"Claude";
        old_claude.app_id = L"Claude_pzs8sxrjxfjjc!Claude";
        bindings.push_back( old_claude );
    }
    return bindings;
}

bool save_bindings( const vector<binding> &bindings ) {
    FILE *file = _wfopen( settings_path().c_str(), L"w, ccs=UTF-8" );
    if ( !file ) return false;
    fwprintf( file, L"# Summon settings, written by the settings window.\n" );
    fwprintf( file, L"# key code | key name | app name | app id | exe path\n" );
    for ( const binding &entry : bindings ) {
        fwprintf( file, L"%lu | %ls | %ls | %ls | %ls\n", entry.key, entry.key_name.c_str(), entry.app_name.c_str(),
                  entry.app_id.c_str(), entry.exe_path.c_str() );
    }
    fclose( file );
    return true;
}

bool is_modifier( DWORD key ) {
    switch ( key ) {
    case VK_LSHIFT: case VK_RSHIFT: case VK_LCONTROL: case VK_RCONTROL:
    case VK_LMENU: case VK_RMENU: case VK_LWIN: case VK_RWIN:
        return true;
    }
    return false;
}

#include "apps.h"

#include <windows.h>
#include <knownfolders.h>
#include <propkey.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <algorithm>

using std::vector;

namespace {

// Desktop apps without a property for their program file have an app ID such as
// "{6D809377-6AF0-444B-8957-A3773F02200E}\Notepad++\notepad++.exe", where the GUID names a
// known folder such as Program Files.
wstring path_from_app_id( const wstring &app_id ) {
    if ( app_id.size() < 5 || app_id.compare( app_id.size() - 4, 4, L".exe" ) != 0 ) return L"";
    if ( app_id.find( L":\\" ) == 1 ) return app_id;
    if ( app_id[ 0 ] != L'{' ) return L"";
    size_t close = app_id.find( L'}' );
    if ( close == wstring::npos ) return L"";
    GUID folder_id;
    if ( FAILED( CLSIDFromString( app_id.substr( 0, close + 1 ).c_str(), &folder_id ) ) ) return L"";
    wchar_t *folder = nullptr;
    wstring path;
    if ( SUCCEEDED( SHGetKnownFolderPath( folder_id, 0, nullptr, &folder ) ) ) path = folder + app_id.substr( close + 1 );
    CoTaskMemFree( folder );
    return path;
}

wstring take( wchar_t *text ) {
    wstring result = text ? text : L"";
    CoTaskMemFree( text );
    return result;
}

}  // namespace

vector<installed_app> list_installed_apps() {
    vector<installed_app> apps;
    IShellItem *folder = nullptr;
    if ( FAILED( SHGetKnownFolderItem( FOLDERID_AppsFolder, KF_FLAG_DEFAULT, nullptr, IID_PPV_ARGS( &folder ) ) ) ) return apps;
    IEnumShellItems *items = nullptr;
    if ( SUCCEEDED( folder->BindToHandler( nullptr, BHID_EnumItems, IID_PPV_ARGS( &items ) ) ) ) {
        IShellItem *item = nullptr;
        while ( items->Next( 1, &item, nullptr ) == S_OK ) {
            installed_app app;
            wchar_t *text = nullptr;
            if ( SUCCEEDED( item->GetDisplayName( SIGDN_NORMALDISPLAY, &text ) ) ) app.name = take( text );
            if ( SUCCEEDED( item->GetDisplayName( SIGDN_PARENTRELATIVEPARSING, &text ) ) ) app.app_id = take( text );
            IShellItem2 *details = nullptr;
            if ( SUCCEEDED( item->QueryInterface( IID_PPV_ARGS( &details ) ) ) ) {
                if ( SUCCEEDED( details->GetString( PKEY_Link_TargetParsingPath, &text ) ) ) app.exe_path = take( text );
                details->Release();
            }
            if ( app.exe_path.empty() ) app.exe_path = path_from_app_id( app.app_id );
            if ( !app.name.empty() && !app.app_id.empty() ) apps.push_back( app );
            item->Release();
        }
        items->Release();
    }
    folder->Release();
    std::sort( apps.begin(), apps.end(), []( const installed_app &a, const installed_app &b ) {
        return _wcsicmp( a.name.c_str(), b.name.c_str() ) < 0;
    } );
    return apps;
}

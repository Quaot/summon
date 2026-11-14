#include "startup.h"

#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>

#include "bindings.h"

namespace {

wstring shortcut_path() {
    wchar_t *folder = nullptr;
    wstring path;
    if ( SUCCEEDED( SHGetKnownFolderPath( FOLDERID_Startup, 0, nullptr, &folder ) ) ) path = wstring( folder ) + L"\\Summon.lnk";
    CoTaskMemFree( folder );
    return path;
}

}  // namespace

bool starts_with_windows() {
    return GetFileAttributesW( shortcut_path().c_str() ) != INVALID_FILE_ATTRIBUTES;
}

bool set_starts_with_windows( bool wanted ) {
    if ( !wanted ) return DeleteFileW( shortcut_path().c_str() ) || !starts_with_windows();
    IShellLinkW *link = nullptr;
    if ( FAILED( CoCreateInstance( CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS( &link ) ) ) ) return false;
    link->SetPath( exe_path().c_str() );
    link->SetWorkingDirectory( exe_folder().c_str() );
    link->SetDescription( L"Shows and hides apps on the current virtual desktop" );
    IPersistFile *file = nullptr;
    bool saved = false;
    if ( SUCCEEDED( link->QueryInterface( IID_PPV_ARGS( &file ) ) ) ) {
        saved = SUCCEEDED( file->Save( shortcut_path().c_str(), TRUE ) );
        file->Release();
    }
    link->Release();
    return saved;
}

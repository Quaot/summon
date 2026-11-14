#include "virtual_desktops.h"

#include <objbase.h>
#include <servprov.h>

namespace {

const CLSID clsid_immersive_shell = { 0xC2F03A33, 0x21F5, 0x47FA, { 0xB4, 0xBB, 0x15, 0x63, 0x62, 0xA2, 0xF2, 0x39 } };
const GUID sid_desktop_manager_internal = { 0xC5E0CDCA, 0x7B6E, 0x41B2, { 0x9F, 0xC4, 0xD9, 0x39, 0x75, 0xCC, 0x46, 0x7B } };
const IID iid_desktop_manager_internal = { 0x53F5CA0B, 0x158F, 0x4124, { 0x90, 0x0C, 0x05, 0x71, 0x58, 0x06, 0x0B, 0x27 } };
const IID iid_view_collection = { 0x1841C6D7, 0x4F9D, 0x42C0, { 0xAF, 0x41, 0x87, 0x47, 0x53, 0x8F, 0x10, 0xE5 } };

}  // namespace

// Only the methods used here are declared, in their vtable slots. These sit outside the
// anonymous namespace: GCC at -O2 drops calls through pure virtual types it can see are never
// implemented in the program.
struct application_view : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE get_iids( ULONG *, IID ** ) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_runtime_class_name( void ** ) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_trust_level( int * ) = 0;
    virtual HRESULT STDMETHODCALLTYPE set_focus() = 0;
    virtual HRESULT STDMETHODCALLTYPE switch_to() = 0;
    virtual HRESULT STDMETHODCALLTYPE try_invoke_back( void * ) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_thumbnail_window( HWND * ) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_monitor( void ** ) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_visibility( int * ) = 0;
    virtual HRESULT STDMETHODCALLTYPE set_cloak( int, int ) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_position( REFIID, void ** ) = 0;
    virtual HRESULT STDMETHODCALLTYPE set_position( void * ) = 0;
    virtual HRESULT STDMETHODCALLTYPE insert_after_window( HWND ) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_extended_frame_position( RECT * ) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_app_user_model_id( PWSTR * ) = 0;
};

struct virtual_desktop : IUnknown {};

struct view_collection : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE get_views( void ** ) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_views_by_z_order( void ** ) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_views_by_app_user_model_id( const wchar_t *, void ** ) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_view_for_hwnd( HWND, application_view ** ) = 0;
};

struct desktop_manager_internal : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE get_count( int * ) = 0;
    virtual HRESULT STDMETHODCALLTYPE move_view_to_desktop( application_view *, virtual_desktop * ) = 0;
    virtual HRESULT STDMETHODCALLTYPE can_view_move_desktops( application_view *, BOOL * ) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_current_desktop( virtual_desktop ** ) = 0;
};

bool desktop_interfaces_work() {
    IServiceProvider *shell = nullptr;
    if ( FAILED( CoCreateInstance( clsid_immersive_shell, nullptr, CLSCTX_LOCAL_SERVER, IID_IServiceProvider,
                                   reinterpret_cast<void **>( &shell ) ) ) )
        return false;
    desktop_manager_internal *manager = nullptr;
    view_collection *views = nullptr;
    virtual_desktop *current = nullptr;
    bool works = SUCCEEDED( shell->QueryService( sid_desktop_manager_internal, iid_desktop_manager_internal, reinterpret_cast<void **>( &manager ) ) ) &&
                 SUCCEEDED( shell->QueryService( iid_view_collection, iid_view_collection, reinterpret_cast<void **>( &views ) ) ) &&
                 SUCCEEDED( manager->get_current_desktop( &current ) ) && current;
    if ( current ) current->Release();
    if ( views ) views->Release();
    if ( manager ) manager->Release();
    shell->Release();
    return works;
}

std::wstring taskbar_app_id( HWND hwnd ) {
    std::wstring id;
    IServiceProvider *shell = nullptr;
    if ( FAILED( CoCreateInstance( clsid_immersive_shell, nullptr, CLSCTX_LOCAL_SERVER, IID_IServiceProvider,
                                   reinterpret_cast<void **>( &shell ) ) ) )
        return id;
    view_collection *views = nullptr;
    application_view *view = nullptr;
    PWSTR text = nullptr;
    if ( SUCCEEDED( shell->QueryService( iid_view_collection, iid_view_collection, reinterpret_cast<void **>( &views ) ) ) &&
         SUCCEEDED( views->get_view_for_hwnd( hwnd, &view ) ) && view &&
         SUCCEEDED( view->get_app_user_model_id( &text ) ) && text ) {
        id = text;
    }
    CoTaskMemFree( text );
    if ( view ) view->Release();
    if ( views ) views->Release();
    shell->Release();
    return id;
}

bool move_to_current_desktop( HWND hwnd ) {
    IServiceProvider *shell = nullptr;
    if ( FAILED( CoCreateInstance( clsid_immersive_shell, nullptr, CLSCTX_LOCAL_SERVER, IID_IServiceProvider,
                                   reinterpret_cast<void **>( &shell ) ) ) )
        return false;

    desktop_manager_internal *manager = nullptr;
    view_collection *views = nullptr;
    application_view *view = nullptr;
    virtual_desktop *current = nullptr;
    bool moved = false;

    if ( SUCCEEDED( shell->QueryService( sid_desktop_manager_internal, iid_desktop_manager_internal, reinterpret_cast<void **>( &manager ) ) ) &&
         SUCCEEDED( shell->QueryService( iid_view_collection, iid_view_collection, reinterpret_cast<void **>( &views ) ) ) &&
         SUCCEEDED( views->get_view_for_hwnd( hwnd, &view ) ) && view &&
         SUCCEEDED( manager->get_current_desktop( &current ) ) && current ) {
        moved = SUCCEEDED( manager->move_view_to_desktop( view, current ) );
    }

    if ( current ) current->Release();
    if ( view ) view->Release();
    if ( views ) views->Release();
    if ( manager ) manager->Release();
    shell->Release();
    return moved;
}

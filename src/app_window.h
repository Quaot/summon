#pragma once

#include "bindings.h"

// Brings the app's main window to the front on the current virtual desktop, starting the app
// if it has no window, or hides the window when it is already in front.
void toggle_app( const binding &app );

// What toggle_app does, decided from the window's state alone so it can be tested.
enum class toggle_action { launch, hide, bring_here };

struct window_state {
    bool found;
    bool visible;
    bool in_front;
    bool minimised;
    bool on_current_desktop;
};

toggle_action choose_action( const window_state &state );

// Whether hwnd is the app's main window: titled, unowned, not a tool window, and carrying the
// app's ID or made by its program.
bool window_belongs_to( HWND hwnd, const binding &app );

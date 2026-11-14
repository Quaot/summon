#pragma once

#include "bindings.h"

// Brings the app's main window to the front on the current virtual desktop, starting the app
// if it has no window, or hides the window when it is already in front.
void toggle_app( const binding &app );

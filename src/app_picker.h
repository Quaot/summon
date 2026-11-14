#pragma once

#include <windows.h>

#include "apps.h"

// Shows the Start menu's apps with a search box. Returns false if nothing is chosen.
bool pick_app( HWND owner, installed_app &chosen );

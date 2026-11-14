# What Summon should do

This is the reference for Summon's behaviour. The checks in `tests\test_main.cpp` are written from this file alone, by someone who has not read the code. When a check fails, the code is wrong unless the check contradicts this file.

## Purpose

Summon runs in the background with a tray icon. You give an app a key; pressing the key brings that app's window onto the virtual desktop you are on and to the front, or starts the app if it has no window. Pressing the key again while the window is in front hides it.

## Keys

A key is a Windows virtual-key code from 1 to 255. Mouse buttons are keys too: the middle button is `VK_MBUTTON`, the back button `VK_XBUTTON1` and the forward button `VK_XBUTTON2`. The left and right mouse buttons and the wheel can never be keys.

Modifiers are exactly these eight codes: `VK_LSHIFT`, `VK_RSHIFT`, `VK_LCONTROL`, `VK_RCONTROL`, `VK_LMENU`, `VK_RMENU`, `VK_LWIN`, `VK_RWIN`. Nothing else is a modifier, including the side-neutral `VK_SHIFT`, `VK_CONTROL` and `VK_MENU`.

### How a bound key fires

Summon watches every key and mouse event through low-level hooks. Each event goes through `handle_keyboard_event` or `handle_mouse_event`, which return true when the event should be swallowed (kept from every other program). When a key fires, Summon posts the target message to the target window with the key code as `wParam` and 0 as `lParam`.

- **Bound modifier:** fires when it is pressed and then released with no other key press, mouse button press or wheel event in between. It fires on the release. Its presses and releases are never swallowed, so shortcuts such as Right Ctrl+C keep working. Auto-repeat presses while it is held do not restart or cancel the tap, so holding it and letting go still fires once. Mouse movement does not cancel the tap. Releases never cancel a tap, whether of another key or of a mouse button; only presses and wheel events do. So with two bound modifiers, pressing the second cancels the first one's tap, and the second still fires on its own release even if the first is released in between.
- **Bound ordinary key or mouse button:** fires on the first press and never on the release. Its press, its auto-repeat presses and its release are all swallowed. Auto-repeat presses do not fire again.
- **Unbound key or button:** never fires and is never swallowed. Pressing it cancels a tap that is under way.
- A bound ordinary key pressed while a bound modifier is held fires, and the modifier's tap is cancelled.
- **Injected input** (events flagged `LLKHF_INJECTED` or `LLMHF_INJECTED`, such as the key Summon itself sends) is ignored entirely: it never fires, is never swallowed, and does not cancel a tap under way, whatever key or button it is.
- `WM_KEYDOWN` and `WM_SYSKEYDOWN` are presses; `WM_KEYUP` and `WM_SYSKEYUP` are releases.
- `set_bound_keys` replaces the whole set of bound keys and forgets any key held or tap under way, so nothing left over from before can fire. A bound ordinary key that was held when the set changed counts as freshly pressed on its next press, so that press fires.

### Naming keys

- `name_of_mouse_button`: `VK_MBUTTON` is "Middle mouse button", `VK_XBUTTON1` is "Mouse back button", `VK_XBUTTON2` is "Mouse forward button".
- `name_of_key` gives the name Windows has for the key's scan code on the current keyboard layout (with the extended flag taken into account, so Right Ctrl and Left Ctrl differ). When the scan code is 0 or Windows has no name for it, the name is "Key " followed by the virtual-key code in decimal, such as "Key 232".

## Bindings and settings.txt

A binding holds a key, the key's name, the app's display name, the app ID, and the app's program path (which may be empty).

Bindings are saved in a UTF-8 text file. Each binding is one line:

```
key code | key name | app name | app ID | program path
```

Reading a file:

- Lines may end with CRLF or LF. Lines starting with `#`, and blank lines, are skipped. A UTF-8 byte-order mark at the start of the file is ignored.
- Spaces around each field are trimmed.
- The key code may be decimal or hex with `0x`. A line whose key is 0 or above 255, or whose app ID is empty, is skipped.
- A line may stop early; missing fields are empty. A line with no `|` at all is not a binding.
- A missing file gives no bindings.
- Older files from Summon's earlier version ("Claude Toggle") hold lines `key = <code>` and `name = <key name>` instead, with any spacing around `=`; the code is read like a new-form key code, so decimal or hex with `0x`. If a file has no bindings in the new form but has such a key line with a valid code, it gives one binding: that key and name, app name "Claude", app ID `Claude_pzs8sxrjxfjjc!Claude`, and an empty program path. If the file also has new-form bindings, the old lines are ignored.

Writing a file saves every binding in order so that reading it back gives the same bindings exactly, including accented letters and spaces in names and paths. A saved empty list reads back as empty.

A key can open only one app. `row_using_key` finds the row whose binding already uses a key, leaving out the row being changed, and gives -1 when no other row uses it.

## Choosing what a key press does

From the state of the app's window:

- No window found: start the app.
- Window visible, in front, not minimised, and on the current desktop: hide it.
- Anything else (hidden, behind other windows, minimised, or on another desktop): bring it here, meaning onto the current desktop, restored and in front.

## Which windows belong to an app

A window can be an app's main window only if it has a title, has no owner window, and is not a tool window. Such a window belongs to an app when any of these match the app, ignoring letter case:

- the app ID the taskbar groups the window under, compared with the app's ID or its program path;
- the app ID set on the window itself (the `System.AppUserModel.ID` property), compared with the app's ID;
- the package app ID of the process that made the window, compared with the app's ID;
- the program file of that process, compared with the app's program path.

An empty app ID or program path never matches anything.

## Apps

The apps you can choose are those in the Start menu's app list (`shell:AppsFolder`). Each has a display name and an app ID, and the list is sorted by name ignoring case. On the owner's machine the list includes Claude with app ID `Claude_pzs8sxrjxfjjc!Claude`.

Some app IDs name a program file. `path_from_app_id` turns such an ID into a path:

- An ID that is already a full path to a program, such as `C:\Tools\thing.exe`, stays as it is.
- An ID made of a known-folder GUID in braces followed by a path, such as `{6D809377-6AF0-444B-8957-A3773F02200E}\Notepad++\notepad++.exe`, becomes that known folder's path joined to the rest (here Program Files, so `C:\Program Files\Notepad++\notepad++.exe` on most machines).
- Anything else gives an empty string: plain names such as `Chrome`, packaged IDs such as `Microsoft.WindowsNotepad_8wekyb3d8bbwe!App`, paths to files that are not `.exe`, and IDs whose braces do not hold a valid known-folder GUID.

## Explorer

Moving windows between virtual desktops uses Explorer's undocumented interfaces, whose IDs change between Windows versions.

- `desktop_interfaces_work` is true when Explorer answers to the IDs Summon uses; on Windows 11 build 26200 it is true.
- `taskbar_app_id` is empty for a window that has no taskbar entry, such as a hidden window.
- `move_to_current_desktop` returns false for a window that has no taskbar entry, such as a hidden window, and does nothing to it.

## What the checks may call

These declarations come from Summon's headers in `src\`. Include the headers by those names (`#include "../src/bindings.h"` and so on).

```cpp
// bindings.h
struct binding {
    DWORD key = 0;
    wstring key_name;
    wstring app_name;
    wstring app_id;
    wstring exe_path;
};
std::vector<binding> load_bindings_from( const wstring &path );
bool save_bindings_to( const wstring &path, const std::vector<binding> &bindings );
int row_using_key( const std::vector<binding> &bindings, DWORD key, int keep );
bool is_modifier( DWORD key );

// hooks.h
void set_bound_keys( const std::vector<DWORD> &keys );
void set_hook_target( HWND target, UINT message );
bool handle_keyboard_event( WPARAM message, const KBDLLHOOKSTRUCT &key );
bool handle_mouse_event( WPARAM message, const MSLLHOOKSTRUCT &mouse );
DWORD mouse_button_key( WPARAM message, const MSLLHOOKSTRUCT *mouse );   // 0 for anything but middle, back and forward
bool is_button_down( WPARAM message );                                   // true for middle and side button presses only

// app_window.h
enum class toggle_action { launch, hide, bring_here };
struct window_state {
    bool found;
    bool visible;
    bool in_front;
    bool minimised;
    bool on_current_desktop;
};
toggle_action choose_action( const window_state &state );
bool window_belongs_to( HWND hwnd, const binding &app );

// apps.h
struct installed_app {
    wstring name;
    wstring app_id;
    wstring exe_path;
};
wstring path_from_app_id( const wstring &app_id );
std::vector<installed_app> list_installed_apps();

// key_picker.h
std::wstring name_of_key( const KBDLLHOOKSTRUCT &key );
std::wstring name_of_mouse_button( DWORD key );

// virtual_desktops.h
bool move_to_current_desktop( HWND hwnd );
std::wstring taskbar_app_id( HWND hwnd );
bool desktop_interfaces_work();
```

`wstring` is `std::wstring`; the headers bring in `<windows.h>`. In a mouse event, the side button number is in the high word of `mouseData` (`XBUTTON1` or `XBUTTON2`).

## Rules for the checks

- Never show a window, send real input (`SendInput`, `keybd_event`, `mouse_event`), take a screenshot, or move the mouse. Windows the checks make stay hidden, and the target window for fired keys can be a message-only window (`HWND_MESSAGE`); read fired keys from it with `PeekMessage`.
- Never start or switch apps, and never touch the real `settings.txt` or the Startup folder. Use files in `%TEMP%` and delete them afterwards.
- The test program is `tests\test_main.cpp`. It prints one line per check, `pass  <plain sentence>` or `FAIL  <plain sentence>`, then "All checks passed." or "Some checks failed.", and returns 1 if any failed. `tests\test.cmd` builds it with the Summon sources and runs it.

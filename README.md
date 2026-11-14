# Summon

Summon gives an app a key. Press the key and the app comes up on the virtual desktop you are on, moving its window over from another desktop or starting the app if it is closed. Press it again while the app is in front and the window hides, taskbar button and all. The app keeps running, so nothing in it is lost.

A key can be anything on the keyboard, or any mouse button other than left and right. Ctrl, Shift, Alt and Win count only when tapped on their own, so shortcuts such as Ctrl+C still work. Any other key you bind stops doing its usual job; binding PrtSc, for example, stops it from opening the Snipping Tool.

## Build

Needs MSYS2 with the UCRT64 g++ in `C:\msys64\ucrt64\bin`.

```
build.cmd
```

## Use

Run `summon.exe`. A teal S appears among the tray icons by the clock; Windows may tuck it under the ^ arrow, and you can drag it out to keep it in view. Click it to open the settings.

- **Add** asks for an app, chosen from the same list as the Start menu, then for its key.
- **Change key** picks a new key for the selected app. Double-clicking a row does the same.
- **Remove** deletes the selected row, as does the Delete key.
- **Start with Windows** puts a shortcut in your Startup folder.

Running `summon.exe` again while it is running also opens the settings. Right-click the tray icon to quit.

The settings are saved in `settings.txt` next to the exe, one binding per line.

## Remove

```
summon.exe --uninstall
```

This stops Summon and deletes its Startup shortcut. After that the folder can be deleted.

## How it works

Low-level keyboard and mouse hooks watch for the bound keys. Windows has no public way to move another program's window to a different virtual desktop, so Summon uses Explorer's undocumented `IVirtualDesktopManagerInternal` interface (see `src/virtual_desktops.cpp`). It moves the window first and brings it forward only once it has arrived, since bringing forward a window that sits on another desktop switches you to that desktop.

To tell an app's windows apart from the rest, Summon asks Explorer which app the taskbar groups each window under, through the same undocumented interfaces. That catches apps such as Discord that name themselves on the process rather than the window. The program path and package name are checked as a fallback.

The interface IDs come from the [winvd](https://docs.rs/crate/winvd/latest/source/src/interfaces.rs) project and match Windows 11 24H2 and 25H2 (builds 26100 and 26200). Summon was tested on build 26200. Microsoft changes these IDs between Windows versions. If an update breaks them, windows stay on their old desktop until the IDs in `virtual_desktops.cpp` are updated from winvd.

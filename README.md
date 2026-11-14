# Claude Toggle

Tapping Right Ctrl on its own brings the Claude desktop app up on whichever virtual desktop you are on. If Claude is on another desktop, its window moves here; if Claude is not running, it starts. Tapping again while Claude is in front hides it, taskbar button and all, and Claude keeps running so Claude Code sessions carry on.

A tap means pressing and releasing Right Ctrl with no other key or mouse input in between, so shortcuts such as Right Ctrl+C still work as usual.

## Build

Needs MSYS2 with the UCRT64 g++ in `C:\msys64\ucrt64\bin`.

```
build.cmd
```

## Install

```
claude_toggle.exe --install
```

This puts a `Claude Toggle` shortcut in your Startup folder (`shell:startup`) and starts the background copy. It runs with no window; only one copy runs at a time.

## Remove

```
claude_toggle.exe --uninstall
```

This stops the background copy and deletes the Startup shortcut. After that the folder can be deleted.

## Other commands

```
claude_toggle.exe --stop      stops the background copy until the next sign-in
claude_toggle.exe --toggle    does what a Right Ctrl tap does
```

## How it works

A low-level keyboard hook watches for the tap. Windows gives programs no public way to move another program's window to a different virtual desktop, so the tool uses Explorer's undocumented `IVirtualDesktopManagerInternal` interface to move Claude to the current desktop (see `src/virtual_desktops.cpp`), and brings Claude to the front only once it has arrived. Bringing it forward while it still sits on another desktop would switch you to that desktop.

The interface IDs come from the [winvd](https://docs.rs/crate/winvd/latest/source/src/interfaces.rs) project and match Windows 11 24H2 and 25H2 (builds 26100 and 26200); this was tested on build 26200. Microsoft changes these IDs between Windows versions. If an update breaks them, the tool falls back to hiding Claude and showing it again, which leaves Claude on its old desktop, and the IDs in `virtual_desktops.cpp` need updating from winvd.

The tool finds Claude's main window by looking for a titled `Chrome_WidgetWin_1` window owned by the Claude app in `WindowsApps`, and starts Claude through its app ID `Claude_pzs8sxrjxfjjc!Claude`.

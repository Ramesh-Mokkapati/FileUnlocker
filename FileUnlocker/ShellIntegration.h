// ShellIntegration.h - Windows Explorer integration for File Unlocker.
//
// Adds an "Unlock with File Unlocker" entry to Explorer's right-click menu
// for files and folders, so the file manager itself can hand a locked item
// straight to this utility, which then lists the processes holding it and
// can terminate them.
//
// The verb is registered per-user under HKCU\Software\Classes (no
// administrator rights, nothing machine-wide): Explorer runs
//     "<path to FileUnlocker.exe>" "<selected item>"
// and CFileUnlockerApp opens with that item already checked.
//
// The same register/unregister calls back the /register and /unregister
// command-line switches, so an installer can set it up silently.
#pragma once
#include <string>

class ShellIntegration
{
public:
    // True when the verb is registered AND points at this executable (a copy
    // registered from another folder counts as not installed, so re-adding it
    // repoints the menu here).
    static bool IsRegistered();

    // Adds (or repoints) the context-menu verb for files and folders.
    static bool Register(std::wstring& errorOut);

    // Removes the verb. Succeeds when it was never registered.
    static bool Unregister(std::wstring& errorOut);

    // Full path of the running executable.
    static std::wstring ExecutablePath();

    // Registers the menu entry unless the user has turned it off (opt-out
    // remembered in the app's settings) or this process is elevated.
    // Silent: failures just leave the menu as it was.
    static void EnsureRegistered();

    // Remembers whether the user turned the menu entry off, so
    // EnsureRegistered() doesn't add it back on the next launch.
    static void SetOptedOut(bool optedOut);
    static bool IsOptedOut();
    static bool IsProcessElevated();
};

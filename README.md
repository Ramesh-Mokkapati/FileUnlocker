# File Unlocker

A small Windows MFC utility that finds which process(es) have a file (or anything inside a folder) open,
and can close them to release the lock. Right-click a locked file in Explorer and choose
**Unlock with File Unlocker**.

Split out of DiskForge, where it used to be one of the built-in tools.

## Features

- **Explorer integration** - the first time File Unlocker runs it adds **"Unlock with File Unlocker"** to
  Explorer's right-click menu for every file and folder (on Windows 11 it is under *Show more options*).
  Right-clicking a locked item opens File Unlocker with it already checked and its locking processes listed,
  ready for **Kill Selected** / **Kill All**.
  - Untick *Show "Unlock with File Unlocker" in Explorer's right-click menu* (or run
    `FileUnlocker.exe /unregister`) to remove it. That choice is remembered; `/register` adds it back, for
    example from an installer
  - If the exe moves, the next launch points the menu at the new location
  - The entry is written per user under `HKCU\Software\Classes\*\shell` and `...\Directory\shell`, so it needs
    no administrator rights. An elevated instance never writes it (it may belong to a different account)
- **Files and folders** - for a folder, every file inside (up to 5,000, junctions and symbolic links not
  followed) is checked in one Restart Manager session, so "the folder is in use" can be traced back to the
  processes behind it
- `FileUnlocker.exe <path>` opens with that file or folder checked; items can also be dropped on the window,
  typed in (Enter re-checks) or picked with **Browse...**
- The list shows each locking process's name, PID and full executable path
- **Runs without elevation**; **Run as Administrator** relaunches it elevated (keeping the path) when a locking
  process belongs to another user or runs elevated
- Works at the process/file-handle level via the Windows Restart Manager API (`RmStartSession` /
  `RmRegisterResources` / `RmGetList` - the mechanism behind Explorer's own "This file is open in another
  program" dialog)
- Its only "write" is terminating another process, which the dialog always confirms first - listing exactly
  which process(es) and PIDs are about to be closed and warning that unsaved work in them will be lost
- **Refuses to terminate core Windows processes** (`csrss.exe`, `wininit.exe`, `winlogon.exe`, `services.exe`,
  `lsass.exe`, `smss.exe`, `System`, `Registry`) even if asked, since force-killing those can crash Windows.
  The check lives in the engine (`FileUnlocker::IsCriticalSystemProcess`), not just the UI. It also never
  terminates its own process. This is a safety floor, not a full "important processes" list - killing any
  process can lose unsaved work, which is why every kill is confirmed

## Command line

| Command | Effect |
|---------|--------|
| `FileUnlocker.exe` | Opens the window (and adds the Explorer menu entry unless it was turned off) |
| `FileUnlocker.exe "<file or folder>"` | Opens with that item checked - this is what the Explorer menu runs |
| `FileUnlocker.exe /register` | Adds the Explorer menu entry and exits |
| `FileUnlocker.exe /unregister` | Removes the Explorer menu entry, remembers that, and exits |

## Requirements

- **OS:** Windows 10 / Windows 11 x64
- **Runtime:** Microsoft Visual C++ Redistributable 2022 (x64) - https://aka.ms/vs/17/release/vc_redist.x64.exe
- **Build:** Visual Studio 2022 with the "Desktop development with C++" workload and MFC

## How to Build

Open `FileUnlocker.sln` in Visual Studio 2022, select **Release | x64** and build, or:

```powershell
msbuild FileUnlocker.sln /p:Configuration=Release /p:Platform=x64
```

The exe is written to `x64\Release\FileUnlocker.exe`.

## Continuous Integration

`.github/workflows/build.yml` builds Debug and Release x64 on every push and pull request and uploads
`FileUnlocker.exe` as an artifact. Every successful push to the default branch is released automatically as
`v1.0.<run number>` (pushing a `v*.*.*` tag releases exactly that tag) with `FileUnlocker-<tag>-x64.zip`
attached, after checking the exe only depends on Windows and VC++ 2015-2022 runtime DLLs.

## Project Structure

| File | Description |
|------|-------------|
| `FileUnlocker.sln` | Visual Studio 2022 solution |
| `FileUnlocker/FileUnlocker.vcxproj` | MSBuild project (x64, v143, MFC as shared DLL, `asInvoker`) |
| `FileUnlocker/FileUnlockerApp.cpp/h` | Application entry point, command-line switches, automatic menu registration |
| `FileUnlocker/FileUnlocker.cpp/h` | Engine: finds locking processes via Restart Manager, terminates them with the safety checks |
| `FileUnlocker/FileUnlockerDialog.cpp/h` | Main window |
| `FileUnlocker/ShellIntegration.cpp/h` | Explorer right-click menu: register, unregister, opt-out, elevation check |
| `FileUnlocker/FileUnlocker.rc`, `resource.h` | Dialog, icon, strings, version info |
| `FileUnlocker/FileUnlocker.manifest` | DPI awareness, Windows 10/11 compatibility, Common Controls 6 |
| `FileUnlocker/FileUnlocker.ico` | Application icon (also used by the Explorer menu entry) |

// FileUnlockerApp.h - Stand-alone File Unlocker utility, split out of
// DiskForge. Finds the processes that hold a file or folder open (Restart
// Manager) and can terminate them; integrates with Explorer's right-click
// menu so the file manager can hand it a locked item directly.
//
// Command line:
//   FileUnlocker.exe [path]      open with [path] already checked
//   FileUnlocker.exe /register   add the Explorer menu entry, then exit
//   FileUnlocker.exe /unregister remove the Explorer menu entry, then exit
#pragma once

class CFileUnlockerApp : public CWinApp
{
public:
    virtual BOOL InitInstance();
};

extern CFileUnlockerApp theApp;

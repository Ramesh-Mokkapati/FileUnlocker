// FileUnlockerApp.cpp - Application entry point.
#include "pch.h"
#include "FileUnlockerApp.h"
#include "FileUnlockerDialog.h"
#include "ShellIntegration.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CFileUnlockerApp theApp;

BOOL CFileUnlockerApp::InitInstance()
{
    INITCOMMONCONTROLSEX InitCtrls;
    InitCtrls.dwSize = sizeof(InitCtrls);
    InitCtrls.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&InitCtrls);

    CWinApp::InitInstance();
    SetRegistryKey(_T("DiskForge"));

    // The first argument is either a switch or the item to check; quotes
    // around a path (as Explorer passes "%1") are already stripped by __wargv.
    CString path;
    if (__argc > 1)
    {
        CString arg = __wargv[1];
        bool reg = arg.CompareNoCase(_T("/register")) == 0;
        bool unreg = arg.CompareNoCase(_T("/unregister")) == 0;
        if (reg || unreg)
        {
            std::wstring err;
            bool ok = reg ? ShellIntegration::Register(err) : ShellIntegration::Unregister(err);
            if (!ok)
                AfxMessageBox(CString(err.c_str()), MB_ICONERROR);
            else
                ShellIntegration::SetOptedOut(unreg);
            return FALSE;
        }
        path = arg;
    }

    // Put "Unlock with File Unlocker" on Explorer's right-click menu the
    // first time the tool runs (and repoint it if the exe has moved), unless
    // the user switched it off. Skipped when elevated: an elevated process
    // may belong to a different account, whose HKCU isn't the user's menu.
    ShellIntegration::EnsureRegistered();

    CFileUnlockerDialog dlg(path);
    m_pMainWnd = &dlg;
    dlg.DoModal();
    return FALSE; // dialog closed - exit instead of starting the message pump
}

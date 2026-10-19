// FileUnlockerDialog.cpp
#include "pch.h"
#include "FileUnlockerDialog.h"
#include "ShellIntegration.h"
#include <shellapi.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CFileUnlockerDialog, CDialogEx)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_EN_CHANGE(IDC_FU_EDIT_FILE_PATH, &CFileUnlockerDialog::OnEnChangeFilePath)
    ON_BN_CLICKED(IDC_FU_BTN_BROWSE, &CFileUnlockerDialog::OnBtnBrowse)
    ON_BN_CLICKED(IDC_FU_BTN_REFRESH, &CFileUnlockerDialog::OnBtnRefresh)
    ON_BN_CLICKED(IDC_FU_BTN_KILL_SELECTED, &CFileUnlockerDialog::OnBtnKillSelected)
    ON_BN_CLICKED(IDC_FU_BTN_KILL_ALL, &CFileUnlockerDialog::OnBtnKillAll)
    ON_BN_CLICKED(IDC_FU_BTN_CLOSE, &CFileUnlockerDialog::OnBtnClose)
    ON_BN_CLICKED(IDC_FU_CHK_EXPLORER, &CFileUnlockerDialog::OnChkExplorer)
    ON_BN_CLICKED(IDC_FU_BTN_ELEVATE, &CFileUnlockerDialog::OnBtnElevate)
    ON_WM_DROPFILES()
END_MESSAGE_MAP()

CFileUnlockerDialog::CFileUnlockerDialog(const CString& initialPath, CWnd* pParent)
    : CDialogEx(IDD_FILEUNLOCKER, pParent)
    , m_initialPath(initialPath)
{
}

bool CFileUnlockerDialog::IsProcessElevated()
{
    bool elevated = false;
    HANDLE hToken = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
    {
        TOKEN_ELEVATION te = {};
        DWORD cb = 0;
        if (GetTokenInformation(hToken, TokenElevation, &te, sizeof(te), &cb))
            elevated = te.TokenIsElevated != 0;
        CloseHandle(hToken);
    }
    return elevated;
}

void CFileUnlockerDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_FU_EDIT_FILE_PATH,   m_editFilePath);
    DDX_Control(pDX, IDC_FU_BTN_BROWSE,       m_btnBrowse);
    DDX_Control(pDX, IDC_FU_LIST_LOCKERS,     m_listLockers);
    DDX_Control(pDX, IDC_FU_STATIC_STATUS,    m_staticStatus);
    DDX_Control(pDX, IDC_FU_BTN_REFRESH,      m_btnRefresh);
    DDX_Control(pDX, IDC_FU_BTN_KILL_SELECTED,m_btnKillSelected);
    DDX_Control(pDX, IDC_FU_BTN_KILL_ALL,     m_btnKillAll);
    DDX_Control(pDX, IDC_FU_BTN_CLOSE,        m_btnClose);
    DDX_Control(pDX, IDC_FU_CHK_EXPLORER,     m_chkExplorer);
    DDX_Control(pDX, IDC_FU_BTN_ELEVATE,      m_btnElevate);
}

BOOL CFileUnlockerDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    m_elevated = IsProcessElevated();
    SetWindowText(m_elevated
        ? _T("File Unlocker (Administrator) - find and release processes locking a file")
        : _T("File Unlocker - find and release processes locking a file"));
    CenterWindow(GetParent());

    HICON hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
    SetIcon(hIcon, TRUE);
    SetIcon(hIcon, FALSE);

    m_listLockers.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_LABELTIP);
    m_listLockers.InsertColumn(0, _T("Process Name"), LVCFMT_LEFT, 200);
    m_listLockers.InsertColumn(1, _T("PID"),          LVCFMT_RIGHT, 70);
    m_listLockers.InsertColumn(2, _T("Executable"),   LVCFMT_LEFT, 360);

    m_btnRefresh.EnableWindow(FALSE);
    m_btnKillSelected.EnableWindow(FALSE);
    m_btnKillAll.EnableWindow(FALSE);

    // Processes running as another user or elevated can only be closed
    // from an elevated instance - offer the relaunch only when it would help.
    m_btnElevate.ShowWindow(m_elevated ? SW_HIDE : SW_SHOW);
    m_chkExplorer.SetCheck(ShellIntegration::IsRegistered() ? BST_CHECKED : BST_UNCHECKED);

    DragAcceptFiles(TRUE);

    if (!m_initialPath.IsEmpty())
        SetTargetPath(m_initialPath);
    else
        m_staticStatus.SetWindowText(_T("Choose (or drop) a file or folder to check whether anything has it open."));

    return TRUE;
}

void CFileUnlockerDialog::OnEnChangeFilePath()
{
    CString pathText;
    m_editFilePath.GetWindowText(pathText);
    m_targetPath = pathText.GetString();
    m_btnRefresh.EnableWindow(!pathText.IsEmpty());
}

void CFileUnlockerDialog::SetTargetPath(const CString& path)
{
    m_editFilePath.SetWindowText(path);
    OnEnChangeFilePath();
    RefreshLockerList();
}

void CFileUnlockerDialog::OnDropFiles(HDROP hDropInfo)
{
    UINT len = DragQueryFile(hDropInfo, 0, nullptr, 0);
    if (len > 0)
    {
        CString path;
        DragQueryFile(hDropInfo, 0, path.GetBuffer(len + 1), len + 1);
        path.ReleaseBuffer();
        SetTargetPath(path);
    }
    DragFinish(hDropInfo);
}

void CFileUnlockerDialog::OnChkExplorer()
{
    std::wstring err;
    bool wantIt = (m_chkExplorer.GetCheck() == BST_CHECKED);
    bool ok = wantIt ? ShellIntegration::Register(err) : ShellIntegration::Unregister(err);
    if (!ok)
    {
        AfxMessageBox(CString(err.c_str()), MB_ICONERROR);
        m_chkExplorer.SetCheck(ShellIntegration::IsRegistered() ? BST_CHECKED : BST_UNCHECKED);
        return;
    }
    ShellIntegration::SetOptedOut(!wantIt);
    m_staticStatus.SetWindowText(wantIt
        ? _T("Added \"Unlock with File Unlocker\" to Explorer's right-click menu for files and folders.")
        : _T("Removed File Unlocker from Explorer's right-click menu."));
}

void CFileUnlockerDialog::OnBtnElevate()
{
    CString params;
    if (!m_targetPath.empty())
        params.Format(_T("\"%s\""), m_targetPath.c_str());

    std::wstring exe = ShellIntegration::ExecutablePath();
    HINSTANCE h = ShellExecute(nullptr, _T("runas"), exe.c_str(), params.IsEmpty() ? nullptr : params.GetString(),
                               nullptr, SW_SHOWNORMAL);
    if ((INT_PTR)h > 32)
        EndDialog(IDCANCEL); // the elevated copy takes over
    // else: the UAC prompt was declined - keep running unelevated
}

void CFileUnlockerDialog::OnBtnBrowse()
{
    CFileDialog fileDlg(TRUE, nullptr, nullptr, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
        _T("All Files (*.*)|*.*||"), this);

    if (fileDlg.DoModal() == IDOK)
    {
        m_editFilePath.SetWindowText(fileDlg.GetPathName());
        OnEnChangeFilePath();
        RefreshLockerList();
    }
}

void CFileUnlockerDialog::RefreshLockerList()
{
    m_listLockers.DeleteAllItems();
    m_btnKillSelected.EnableWindow(FALSE);
    m_btnKillAll.EnableWindow(FALSE);

    if (m_targetPath.empty())
    {
        m_staticStatus.SetWindowText(_T("Choose (or drop) a file or folder to check whether anything has it open."));
        return;
    }

    if (GetFileAttributes(m_targetPath.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        m_currentLockers.clear();
        m_staticStatus.SetWindowText(_T("That file or folder does not exist."));
        return;
    }

    CWaitCursor wait; // a big folder means checking thousands of files
    m_currentLockers = FileUnlocker::FindLockingProcesses(m_targetPath);

    if (m_currentLockers.empty())
    {
        m_staticStatus.SetWindowText(_T("No active locks detected on this item."));
        return;
    }

    for (size_t i = 0; i < m_currentLockers.size(); ++i)
    {
        CString procName(m_currentLockers[i].processName.c_str());
        CString pidStr;
        pidStr.Format(_T("%u"), m_currentLockers[i].processId);

        int itemIndex = m_listLockers.InsertItem(static_cast<int>(i), procName);
        m_listLockers.SetItemText(itemIndex, 1, pidStr);
        m_listLockers.SetItemText(itemIndex, 2, CString(m_currentLockers[i].imagePath.c_str()));
        m_listLockers.SetItemData(itemIndex, static_cast<DWORD_PTR>(i));
    }

    CString status;
    status.Format(_T("%d process(es) have this item open."), (int)m_currentLockers.size());
    m_staticStatus.SetWindowText(status);

    m_btnKillSelected.EnableWindow(TRUE);
    m_btnKillAll.EnableWindow(TRUE);
}

void CFileUnlockerDialog::OnBtnRefresh()
{
    RefreshLockerList();
}

void CFileUnlockerDialog::KillProcesses(const std::vector<LockingProcess>& targets)
{
    if (targets.empty())
        return;

    CString listDesc;
    for (const auto& p : targets)
    {
        CString line;
        line.Format(_T("%s (PID %u)\r\n"), CString(p.processName.c_str()).GetString(), p.processId);
        listDesc += line;
    }

    CString msg =
        _T("This will forcibly close the following process(es):\r\n\r\n") + listDesc +
        _T("\r\nAny unsaved work in them will be lost. This cannot be undone.\r\n\r\nContinue?");

    if (AfxMessageBox(msg, MB_YESNO | MB_ICONWARNING) != IDYES)
        return;

    int killed = 0, skipped = 0, failed = 0;
    CString failures;

    for (const auto& p : targets)
    {
        std::wstring err;
        if (FileUnlocker::TerminateLockingProcess(p.processId, p.processName, err))
        {
            ++killed;
        }
        else if (FileUnlocker::IsCriticalSystemProcess(p.processName))
        {
            ++skipped;
        }
        else
        {
            ++failed;
            failures += CString(p.processName.c_str()) + _T(": ") + CString(err.c_str()) + _T("\r\n");
        }
    }

    CString result;
    result.Format(_T("%d terminated, %d skipped (critical system process), %d failed."), killed, skipped, failed);
    if (!failures.IsEmpty())
        result += _T("\r\n\r\n") + failures;
    if (failed > 0 && !m_elevated)
        result += _T("\r\nProcesses that belong to another user or run elevated can only be closed after ")
                  _T("clicking \"Run as Administrator\".");

    AfxMessageBox(result, (failed > 0) ? MB_ICONWARNING : MB_ICONINFORMATION);

    ::Sleep(300); // Give the OS a moment to fully release the handle(s) before re-checking
    RefreshLockerList();
}

void CFileUnlockerDialog::OnBtnKillSelected()
{
    std::vector<LockingProcess> targets;
    POSITION pos = m_listLockers.GetFirstSelectedItemPosition();
    while (pos)
    {
        int selectedIdx = m_listLockers.GetNextSelectedItem(pos);
        DWORD_PTR dataIdx = m_listLockers.GetItemData(selectedIdx);
        if (dataIdx < m_currentLockers.size())
            targets.push_back(m_currentLockers[dataIdx]);
    }

    if (targets.empty())
    {
        AfxMessageBox(_T("Select one or more processes in the list first."), MB_ICONWARNING);
        return;
    }

    KillProcesses(targets);
}

void CFileUnlockerDialog::OnBtnKillAll()
{
    KillProcesses(m_currentLockers);
}

void CFileUnlockerDialog::OnBtnClose()
{
    EndDialog(IDCANCEL);
}

void CFileUnlockerDialog::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    if (!m_listLockers.GetSafeHwnd() || cx <= 0 || cy <= 0)
        return;

    const int L = 12, R = cx - 12;
    m_editFilePath.MoveWindow(L, 12, (R - 90) - L, 22, TRUE);
    m_btnBrowse.MoveWindow(R - 80, 12, 80, 22, TRUE);
    m_listLockers.MoveWindow(L, 42, R - L, cy - 42 - 96, TRUE);
    m_staticStatus.MoveWindow(L, cy - 90, R - L, 22, TRUE);
    m_btnRefresh.MoveWindow(L, cy - 60, 110, 26, TRUE);
    m_btnKillSelected.MoveWindow(L + 120, cy - 60, 130, 26, TRUE);
    m_btnKillAll.MoveWindow(L + 260, cy - 60, 140, 26, TRUE);
    m_btnElevate.MoveWindow(R - 170, cy - 60, 170, 26, TRUE);
    m_chkExplorer.MoveWindow(L, cy - 30, R - L - 100, 22, TRUE);
    m_btnClose.MoveWindow(R - 90, cy - 34, 90, 26, TRUE);
}

void CFileUnlockerDialog::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    lpMMI->ptMinTrackSize.x = MIN_WIDTH;
    lpMMI->ptMinTrackSize.y = MIN_HEIGHT;
    CDialogEx::OnGetMinMaxInfo(lpMMI);
}

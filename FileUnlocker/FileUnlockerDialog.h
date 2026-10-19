// FileUnlockerDialog.h - UI for finding and (optionally) terminating
// processes that have a chosen file (or folder) locked open. It is the main
// window of FileUnlocker.exe; Explorer's "Unlock with File Unlocker" menu
// entry (see ShellIntegration.h) opens it with the item already filled in.
#pragma once
#include "resource.h"
#include "FileUnlocker.h"


class CFileUnlockerDialog : public CDialogEx
{
public:
    explicit CFileUnlockerDialog(const CString& initialPath = CString(), CWnd* pParent = nullptr);

    enum { IDD = IDD_FILEUNLOCKER };

protected:
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual void OnOK() { RefreshLockerList(); } // Enter in the path box re-checks

    afx_msg void OnEnChangeFilePath();
    afx_msg void OnBtnBrowse();
    afx_msg void OnBtnRefresh();
    afx_msg void OnBtnKillSelected();
    afx_msg void OnBtnKillAll();
    afx_msg void OnBtnClose();
    afx_msg void OnChkExplorer();
    afx_msg void OnBtnElevate();
    afx_msg void OnDropFiles(HDROP hDropInfo);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);

    DECLARE_MESSAGE_MAP()

private:
    CEdit       m_editFilePath;
    CButton     m_btnBrowse;
    CListCtrl   m_listLockers;
    CButton     m_btnRefresh;
    CButton     m_btnKillSelected;
    CButton     m_btnKillAll;
    CStatic     m_staticStatus;
    CButton     m_btnClose;
    CButton     m_chkExplorer;
    CButton     m_btnElevate;

    std::vector<LockingProcess> m_currentLockers;
    std::wstring                 m_targetPath;
    CString                      m_initialPath;
    bool                         m_elevated = false;

    static const int MIN_WIDTH  = 620;
    static const int MIN_HEIGHT = 420;

    void RefreshLockerList();
    void SetTargetPath(const CString& path);
    static bool IsProcessElevated();
    void KillProcesses(const std::vector<LockingProcess>& targets);
};

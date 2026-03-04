#pragma once

#include "Resource.h"
#include <afxwin.h>
#include <afxcmn.h>

class CConfigEncryptDlg : public CDialogEx
{
public:
    CConfigEncryptDlg(CWnd* pParent = nullptr);
    enum { IDD = IDD_CONFIG_ENCRYPT_DIALOG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();

    afx_msg void OnBnClickedLoadJson();
    afx_msg void OnBnClickedEncrypt();
    DECLARE_MESSAGE_MAP()

    CString m_strJsonPath;
    CString m_strStatus;
};


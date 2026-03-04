#include "pch.h"
#include "framework.h"
#include "ConfigEncryptDlg.h"
#include "ConfigCrypto.h"
#include "afxdialogex.h"
#include <fstream>
#include <sstream>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CConfigEncryptDlg::CConfigEncryptDlg(CWnd* pParent) : CDialogEx(IDD_CONFIG_ENCRYPT_DIALOG, pParent) {}

void CConfigEncryptDlg::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_EDIT_JSON_PATH, m_strJsonPath);
    DDX_Text(pDX, IDC_STATIC_STATUS, m_strStatus);
}

BEGIN_MESSAGE_MAP(CConfigEncryptDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_LOAD_JSON, &CConfigEncryptDlg::OnBnClickedLoadJson)
    ON_BN_CLICKED(IDC_BTN_ENCRYPT, &CConfigEncryptDlg::OnBnClickedEncrypt)
END_MESSAGE_MAP()

BOOL CConfigEncryptDlg::OnInitDialog() {
    CDialogEx::OnInitDialog();
    m_strStatus = _T("Load a config.json file, then encrypt to config.dat.");
    SetDlgItemText(IDC_STATIC_STATUS, m_strStatus);
    return TRUE;
}

void CConfigEncryptDlg::OnBnClickedLoadJson() {
    try {
        CFileDialog dlg(TRUE, _T("json"), _T("config.json"), OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
            _T("JSON files (*.json)|*.json|All files (*.*)|*.*||"), this);
        if (dlg.DoModal() != IDOK) return;
        m_strJsonPath = dlg.GetPathName();
        m_strStatus = _T("Ready. Click 'Encrypt to config.dat' to save.");
        UpdateData(FALSE);
        SetDlgItemText(IDC_STATIC_STATUS, m_strStatus);
    } catch (const std::exception& e) {
        CString msg;
        msg.Format(_T("Exception: %S"), e.what());
        AfxMessageBox(msg, MB_ICONERROR);
    } catch (...) {
        AfxMessageBox(_T("Unknown exception loading file."), MB_ICONERROR);
    }
}

void CConfigEncryptDlg::OnBnClickedEncrypt() {
    UpdateData(TRUE);
    if (m_strJsonPath.IsEmpty()) {
        AfxMessageBox(_T("Please load a config.json file first."), MB_ICONWARNING);
        return;
    }
    try {
        CFile f;
        if (!f.Open(m_strJsonPath, CFile::modeRead | CFile::shareDenyWrite | CFile::typeBinary)) {
            AfxMessageBox(_T("Cannot open the selected file."), MB_ICONERROR);
            return;
        }
        ULONGLONG len = f.GetLength();
        if (len == 0 || len > 1024 * 1024) {
            f.Close();
            AfxMessageBox(_T("File is empty or too large."), MB_ICONERROR);
            return;
        }
        std::string plain((size_t)len, '\0');
        f.Read(&plain[0], (UINT)len);
        f.Close();

        std::vector<unsigned char> cipher;
        if (!ConfigCrypto::EncryptToDat(plain, cipher)) {
            AfxMessageBox(_T("AES encryption failed."), MB_ICONERROR);
            return;
        }

        CString outPath = m_strJsonPath;
        int n = outPath.ReverseFind(_T('\\'));
        if (n >= 0) outPath = outPath.Left(n + 1);
        outPath += _T("config.dat");

        CFile out;
        if (!out.Open(outPath, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary)) {
            AfxMessageBox(_T("Cannot create config.dat."), MB_ICONERROR);
            return;
        }
        out.Write(cipher.data(), (UINT)cipher.size());
        out.Close();

        m_strStatus.Format(_T("Saved: %s"), (LPCWSTR)outPath);
        SetDlgItemText(IDC_STATIC_STATUS, m_strStatus);
        AfxMessageBox(_T("config.dat created successfully."), MB_ICONINFORMATION);
    } catch (const std::exception& e) {
        CString msg;
        msg.Format(_T("Exception: %S"), e.what());
        AfxMessageBox(msg, MB_ICONERROR);
    } catch (...) {
        AfxMessageBox(_T("Unknown exception during encryption."), MB_ICONERROR);
    }
}

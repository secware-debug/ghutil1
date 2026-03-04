// ViewerDlg.cpp : implementation file
//

#include "pch.h"
#include "framework.h"
#include "Viewer.h"
#include "ViewerDlg.h"
#include "ConfigEncryptDlg.h"
#include "afxdialogex.h"
#include <shellapi.h>
#include <sstream>
#include <memory>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace {
bool ParseJsonTypeAndMessage(const std::string& json, std::string& type, std::string& message) {
    type.clear();
    message.clear();
    auto pos = json.find("\"type\"");
    if (pos == std::string::npos) return false;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return false;
    pos = json.find('"', pos);
    if (pos == std::string::npos) return false;
    size_t end = json.find('"', pos + 1);
    if (end == std::string::npos) return false;
    type = json.substr(pos + 1, end - pos - 1);
    pos = json.find("\"message\"");
    if (pos != std::string::npos) {
        pos = json.find(':', pos);
        if (pos != std::string::npos) {
            pos = json.find('"', pos);
            if (pos != std::string::npos) {
                end = json.find('"', pos + 1);
                if (end != std::string::npos)
                    message = json.substr(pos + 1, end - pos - 1);
            }
        }
    }
    return true;
}
} // namespace


// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CViewerDlg dialog



CViewerDlg::CViewerDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_VIEWER_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CViewerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CViewerDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_MESSAGE(WM_WEBSOCKET_MSG, &CViewerDlg::OnWebSocketMsg)
	ON_WM_DESTROY()
	ON_MESSAGE(WM_TRAYICON, &CViewerDlg::OnTrayIcon)
	ON_MESSAGE(WM_CLIENT_DISCONNECTED, &CViewerDlg::OnClientDisconnected)
	ON_BN_CLICKED(IDC_BTN_ENCRYPT_CONFIG, &CViewerDlg::OnBnClickedEncryptConfig)
	ON_BN_CLICKED(IDC_BTN_CLEAR_ALL, &CViewerDlg::OnBnClickedClearAll)
	ON_MESSAGE(WM_SERVER_ERROR, &CViewerDlg::OnServerError)
END_MESSAGE_MAP()


// CViewerDlg message handlers

BOOL CViewerDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	try {

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// List control columns: UID, Last Ping, Status, Events Today, Received Logs, IP
	m_listClients.SubclassDlgItem(IDC_LIST_CLIENTS, this);
	m_listClients.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_listClients.InsertColumn(0, _T("UID"), LVCFMT_LEFT, 80);
	m_listClients.InsertColumn(1, _T("Last Ping"), LVCFMT_LEFT, 120);
	m_listClients.InsertColumn(2, _T("Status"), LVCFMT_LEFT, 50);
	m_listClients.InsertColumn(3, _T("Events Today"), LVCFMT_LEFT, 90);
	m_listClients.InsertColumn(4, _T("Received Logs"), LVCFMT_LEFT, 100);
	m_listClients.InsertColumn(5, _T("IP Address"), LVCFMT_LEFT, 120);

	m_server.SetMessageCallback([this](const std::string& text, const std::string& clientIp) {
		auto* p = new WebSocketMsgPayload{ text, clientIp };
		PostMessage(WM_WEBSOCKET_MSG, (WPARAM)p, 0);
	});
	m_server.SetDisconnectCallback([this](const std::string& clientIp) {
		auto* p = new std::string(clientIp);
		PostMessage(WM_CLIENT_DISCONNECTED, (WPARAM)p, 0);
	});
	m_server.SetErrorCallback([this](const std::string& error) {
		auto* p = new std::string(error);
		PostMessage(WM_SERVER_ERROR, (WPARAM)p, 0);
	});
	if (!m_server.Start(8765)) {
		AfxMessageBox(_T("WebSocket server failed to start on port 8765."));
	} else {
		SetWindowText(_T("Server - 0.0.0.0:8765"));
		AddTrayIcon();
	}
	} catch (const std::exception& e) {
		CString str;
		str.Format(_T("Exception on init: %S"), e.what());
		AfxMessageBox(str, MB_ICONERROR);
	} catch (...) {
		AfxMessageBox(_T("Unknown exception on init."), MB_ICONERROR);
	}
	return TRUE;
}

void CViewerDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CViewerDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

HCURSOR CViewerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CViewerDlg::OnDestroy()
{
	RemoveTrayIcon();
	CDialogEx::OnDestroy();
	m_server.Stop();
}

LRESULT CViewerDlg::OnWebSocketMsg(WPARAM wParam, LPARAM lParam)
{
	std::unique_ptr<WebSocketMsgPayload> p((WebSocketMsgPayload*)wParam);
	if (!p) return 0;
	try {
		std::string type, message;
		if (ParseJsonTypeAndMessage(p->text, type, message)) {
			if (type == "end") {
				SetClientOff(p->clientIp);
				return 0;
			}
			bool isTargetHit = (type == "target_hit");
			UpdateOrAddClient(p->clientIp, isTargetHit);
			if (isTargetHit) {
				CString msg = message.empty() ? _T("target_hit received") : CString(CStringA(message.c_str()));
				ShowNotification(_T("Target Hit"), msg);
			}
		}
	} catch (const std::exception& e) {
		CString str;
		str.Format(_T("Exception in WebSocket message: %S"), e.what());
		AfxMessageBox(str, MB_ICONERROR);
	} catch (...) {
		AfxMessageBox(_T("Unknown exception in WebSocket message."), MB_ICONERROR);
	}
	return 0;
}

void CViewerDlg::UpdateOrAddClient(const std::string& clientIp, bool eventTargetHit)
{
	CTime now = CTime::GetCurrentTime();
	CString timeStr = now.Format(_T("%Y-%m-%d %H:%M:%S"));
	auto it = m_clientsByIp.find(clientIp);
	if (it == m_clientsByIp.end()) {
		ClientRow row;
		row.uid.Format(_T("C%d"), ++m_nextClientIndex);
		row.lastPing = timeStr;
		row.statusOn = true;
		row.eventsToday = eventTargetHit ? 1 : 0;
		row.receivedLogs = 1;
		row.ip = CString(CStringA(clientIp.c_str()));
		row.listIndex = m_listClients.GetItemCount();
		m_listClients.InsertItem(row.listIndex, row.uid);
		m_listClients.SetItemText(row.listIndex, 1, row.lastPing);
		m_listClients.SetItemText(row.listIndex, 2, _T("On"));
		CString tmp;
		tmp.Format(_T("%d"), row.eventsToday);
		m_listClients.SetItemText(row.listIndex, 3, tmp);
		tmp.Format(_T("%d"), row.receivedLogs);
		m_listClients.SetItemText(row.listIndex, 4, tmp);
		m_listClients.SetItemText(row.listIndex, 5, row.ip);
		m_clientsByIp[clientIp] = row;
	} else {
		ClientRow& row = it->second;
		row.lastPing = timeStr;
		row.receivedLogs++;
		if (eventTargetHit) row.eventsToday++;
		if (row.listIndex >= 0 && row.listIndex < m_listClients.GetItemCount()) {
			m_listClients.SetItemText(row.listIndex, 1, row.lastPing);
			CString tmp;
			tmp.Format(_T("%d"), row.eventsToday);
			m_listClients.SetItemText(row.listIndex, 3, tmp);
			tmp.Format(_T("%d"), row.receivedLogs);
			m_listClients.SetItemText(row.listIndex, 4, tmp);
		}
	}
}

bool CViewerDlg::AddTrayIcon()
{
	NOTIFYICONDATAW nid = {};
	nid.cbSize = sizeof(nid);
	nid.hWnd = m_hWnd;
	nid.uID = ID_TRAYICON;
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.uCallbackMessage = WM_TRAYICON;
	nid.hIcon = m_hIcon;
	wcscpy_s(nid.szTip, L"Server");
	return Shell_NotifyIconW(NIM_ADD, &nid) != FALSE;
}

void CViewerDlg::RemoveTrayIcon()
{
	NOTIFYICONDATAW nid = {};
	nid.cbSize = sizeof(nid);
	nid.hWnd = m_hWnd;
	nid.uID = ID_TRAYICON;
	Shell_NotifyIconW(NIM_DELETE, &nid);
}

LRESULT CViewerDlg::OnTrayIcon(WPARAM wParam, LPARAM lParam)
{
	if (LOWORD(lParam) == NIN_BALLOONUSERCLICK)
		SetForegroundWindow();
	return 0;
}

LRESULT CViewerDlg::OnClientDisconnected(WPARAM wParam, LPARAM lParam)
{
	std::unique_ptr<std::string> p((std::string*)wParam);
	if (p) SetClientOff(*p);
	return 0;
}

LRESULT CViewerDlg::OnServerError(WPARAM wParam, LPARAM lParam)
{
	std::unique_ptr<std::string> p((std::string*)wParam);
	if (p) {
		CString str;
		str.Format(_T("Server error: %S"), p->c_str());
		AfxMessageBox(str, MB_ICONERROR);
	}
	return 0;
}

void CViewerDlg::OnBnClickedEncryptConfig()
{
	try {
		CConfigEncryptDlg dlg(this);
		dlg.DoModal();
	} catch (const std::exception& e) {
		CString msg;
		msg.Format(_T("Exception: %S"), e.what());
		AfxMessageBox(msg, MB_ICONERROR);
	} catch (...) {
		AfxMessageBox(_T("Unknown exception."), MB_ICONERROR);
	}
}

void CViewerDlg::OnBnClickedClearAll()
{
	try {
		m_listClients.DeleteAllItems();
		m_clientsByIp.clear();
		m_nextClientIndex = 0;
	} catch (const std::exception& e) {
		CString msg;
		msg.Format(_T("Exception: %S"), e.what());
		AfxMessageBox(msg, MB_ICONERROR);
	} catch (...) {
		AfxMessageBox(_T("Unknown exception."), MB_ICONERROR);
	}
}

void CViewerDlg::SetClientOff(const std::string& clientIp)
{
	auto it = m_clientsByIp.find(clientIp);
	if (it == m_clientsByIp.end()) return;
	ClientRow& row = it->second;
	row.statusOn = false;
	if (row.listIndex >= 0 && row.listIndex < m_listClients.GetItemCount())
		m_listClients.SetItemText(row.listIndex, 2, _T("Off"));
}

void CViewerDlg::ShowNotification(const CString& title, const CString& message)
{
	NOTIFYICONDATAW nid = {};
	nid.cbSize = sizeof(nid);
	nid.hWnd = m_hWnd;
	nid.uID = ID_TRAYICON;
	nid.uFlags = NIF_INFO;
	nid.dwInfoFlags = NIIF_INFO;
	nid.uTimeout = 5000;
	wcsncpy_s(nid.szInfoTitle, (LPCWSTR)title, _TRUNCATE);
	wcsncpy_s(nid.szInfo, (LPCWSTR)message, _TRUNCATE);
	if (Shell_NotifyIconW(NIM_MODIFY, &nid))
		return;
	AfxMessageBox(message, MB_ICONINFORMATION | MB_OK);
}


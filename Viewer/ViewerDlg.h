// ViewerDlg.h : header file
//

#pragma once

#include "WebSocketServer.h"
#include <string>
#include <map>

#define WM_WEBSOCKET_MSG (WM_USER + 100)
#define WM_TRAYICON (WM_USER + 101)
#define WM_CLIENT_DISCONNECTED (WM_USER + 102)
#define WM_SERVER_ERROR (WM_USER + 103)
#define ID_TRAYICON 1

struct WebSocketMsgPayload {
	std::string text;
	std::string clientIp;
};

// CViewerDlg dialog
class CViewerDlg : public CDialogEx
{
// Construction
public:
	CViewerDlg(CWnd* pParent = nullptr);

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_VIEWER_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);

// Implementation
protected:
	HICON m_hIcon;
	CListCtrl m_listClients;
	WebSocketServer m_server;

	struct ClientRow {
		CString uid;
		CString lastPing;
		bool statusOn = false;
		int eventsToday = 0;
		int receivedLogs = 0;
		CString ip;
		int listIndex = -1;
	};
	std::map<std::string, ClientRow> m_clientsByIp;
	int m_nextClientIndex = 0;

	void UpdateOrAddClient(const std::string& clientIp, bool eventTargetHit);
	void SetClientOff(const std::string& clientIp);
	void ShowNotification(const CString& title, const CString& message);
	bool AddTrayIcon();
	void RemoveTrayIcon();

	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg LRESULT OnWebSocketMsg(WPARAM wParam, LPARAM lParam);
	afx_msg void OnDestroy();
	afx_msg LRESULT OnTrayIcon(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnClientDisconnected(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnServerError(WPARAM wParam, LPARAM lParam);
	afx_msg void OnBnClickedEncryptConfig();
	afx_msg void OnBnClickedClearAll();
	DECLARE_MESSAGE_MAP()
};


// IpcUIDlg.h : header file
//

#pragma once

class CIpcUIDlgAutoProxy;

// 워커 쓰레드 -> UI 쓰레드 로그 전달. lParam 은 malloc 한 wchar_t*, 받는 쪽에서 free.
#define WM_IPC_LOG		(WM_APP + 100)

#define IPC_UI_TIMER_ID		1
#define IPC_UI_LOG_MAX		2000


// CIpcUIDlg dialog
class CIpcUIDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CIpcUIDlg);
	friend class CIpcUIDlgAutoProxy;

// Construction
public:
	CIpcUIDlg(CWnd* pParent = nullptr);	// standard constructor
	virtual ~CIpcUIDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_IPCUI_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	CIpcUIDlgAutoProxy* m_pAutoProxy;
	HICON m_hIcon;

	CListBox	m_listLog;
	CString		m_strQueueName;
	CString		m_strIpAddr;
	UINT		m_nPortNum;
	BOOL		m_bNetworkByteOrder;
	int			m_nLogExtent;

	BOOL CanExit();

	void AddLog(LPCWSTR lpszText);
	void UpdateButtons();
	void RunPeer(LPCTSTR lpszRole);

	static void __cdecl LogCallback(void* pCtx, int nChannel, const char* pszUtf8);

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnClose();
	afx_msg void OnDestroy();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg LRESULT OnIpcLog(WPARAM wParam, LPARAM lParam);

	afx_msg void OnBnClickedThreadStart();
	afx_msg void OnBnClickedThreadStop();
	afx_msg void OnBnClickedMqRecv();
	afx_msg void OnBnClickedMqSend();
	afx_msg void OnBnClickedMqStop();
	afx_msg void OnBnClickedMqPeer();
	afx_msg void OnBnClickedTcpRecv();
	afx_msg void OnBnClickedTcpSend();
	afx_msg void OnBnClickedTcpStop();
	afx_msg void OnBnClickedTcpPeer();
	afx_msg void OnBnClickedLogClear();

	virtual void OnOK();
	virtual void OnCancel();
	DECLARE_MESSAGE_MAP()
};

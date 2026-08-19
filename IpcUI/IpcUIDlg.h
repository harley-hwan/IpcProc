
// IpcUIDlg.h : header file
//

#pragma once

class CIpcUIDlgAutoProxy;


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

	BOOL CanExit();

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnClose();
	virtual void OnOK();
	virtual void OnCancel();
	DECLARE_MESSAGE_MAP()
};


// IpcUIDlg.cpp : implementation file
//

#include "pch.h"
#include "framework.h"
#include "IpcUI.h"
#include "IpcUIDlg.h"
#include "afxdialogex.h"
#include "IpcCore.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

static volatile HWND	g_hLogWnd = nullptr;

// 코어는 UTF-8 문자열을 쓴다
static CStringA ToUtf8(const CString& str)
{
	CStringA strOut;

	int nLen = ::WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, nullptr, nullptr);
	if (nLen > 0)
	{
		::WideCharToMultiByte(CP_UTF8, 0, str, -1, strOut.GetBuffer(nLen), nLen, nullptr, nullptr);
		strOut.ReleaseBuffer();
	}

	return strOut;
}


// CIpcUIDlg dialog


IMPLEMENT_DYNAMIC(CIpcUIDlg, CDialogEx);

CIpcUIDlg::CIpcUIDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_IPCUI_DIALOG, pParent)
	, m_strQueueName(_T("IpcDemoQ"))
	, m_strIpAddr(_T("127.0.0.1"))
	, m_nPortNum(51000)
	, m_bNetworkByteOrder(FALSE)
	, m_nLogExtent(0)
{
	EnableActiveAccessibility();
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

CIpcUIDlg::~CIpcUIDlg()
{
}

void CIpcUIDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_LOG, m_listLog);
	DDX_Text(pDX, IDC_EDIT_MQNAME, m_strQueueName);
	DDV_MaxChars(pDX, m_strQueueName, IPC_MSGQ_NAME_MAX - 1);
	DDX_Text(pDX, IDC_EDIT_IP, m_strIpAddr);
	DDX_Text(pDX, IDC_EDIT_PORT, m_nPortNum);
	DDV_MinMaxUInt(pDX, m_nPortNum, 1, 65535);
	DDX_Check(pDX, IDC_CHK_NBO, m_bNetworkByteOrder);
}

BEGIN_MESSAGE_MAP(CIpcUIDlg, CDialogEx)
	ON_WM_CLOSE()
	ON_WM_DESTROY()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_TIMER()
	ON_MESSAGE(WM_IPC_LOG, &CIpcUIDlg::OnIpcLog)
	ON_BN_CLICKED(IDC_BTN_TH_START,   &CIpcUIDlg::OnBnClickedThreadStart)
	ON_BN_CLICKED(IDC_BTN_TH_STOP,    &CIpcUIDlg::OnBnClickedThreadStop)
	ON_BN_CLICKED(IDC_BTN_MQ_RECV,    &CIpcUIDlg::OnBnClickedMqRecv)
	ON_BN_CLICKED(IDC_BTN_MQ_SEND,    &CIpcUIDlg::OnBnClickedMqSend)
	ON_BN_CLICKED(IDC_BTN_MQ_STOP,    &CIpcUIDlg::OnBnClickedMqStop)
	ON_BN_CLICKED(IDC_BTN_TCP_RECV,   &CIpcUIDlg::OnBnClickedTcpRecv)
	ON_BN_CLICKED(IDC_BTN_TCP_SEND,   &CIpcUIDlg::OnBnClickedTcpSend)
	ON_BN_CLICKED(IDC_BTN_TCP_STOP,   &CIpcUIDlg::OnBnClickedTcpStop)
	ON_BN_CLICKED(IDC_BTN_NEW_PROC,   &CIpcUIDlg::OnBnClickedNewProcess)
	ON_BN_CLICKED(IDC_BTN_LOG_CLEAR,  &CIpcUIDlg::OnBnClickedLogClear)
END_MESSAGE_MAP()


// CIpcUIDlg message handlers

void __cdecl CIpcUIDlg::LogCallback(void* pCtx, int nChannel, const char* pszUtf8)
{
	UNREFERENCED_PARAMETER(pCtx);

	HWND hWnd = g_hLogWnd;
	if ((hWnd == nullptr) || (pszUtf8 == nullptr))
		return;

	int nLen = ::MultiByteToWideChar(CP_UTF8, 0, pszUtf8, -1, nullptr, 0);
	if (nLen <= 0)
		return;

	wchar_t* pszWide = static_cast<wchar_t*>(malloc(static_cast<size_t>(nLen) * sizeof(wchar_t)));
	if (pszWide == nullptr)
		return;

	if (::MultiByteToWideChar(CP_UTF8, 0, pszUtf8, -1, pszWide, nLen) <= 0)
	{
		free(pszWide);
		return;
	}

	if (!::PostMessage(hWnd, WM_IPC_LOG, static_cast<WPARAM>(nChannel), reinterpret_cast<LPARAM>(pszWide)))
		free(pszWide);
}

LRESULT CIpcUIDlg::OnIpcLog(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);

	wchar_t* pszWide = reinterpret_cast<wchar_t*>(lParam);
	if (pszWide != nullptr)
	{
		AddLog(pszWide);
		free(pszWide);
	}

	return 0;
}

void CIpcUIDlg::AddLog(LPCWSTR lpszText)
{
	if ((lpszText == nullptr) || (m_listLog.GetSafeHwnd() == nullptr))
		return;

	while (m_listLog.GetCount() >= IPC_UI_LOG_MAX)
		m_listLog.DeleteString(0);

	CTime	tmNow = CTime::GetCurrentTime();
	CString	strLine;
	strLine.Format(_T("%02d:%02d:%02d  %s"), tmNow.GetHour(), tmNow.GetMinute(), tmNow.GetSecond(), lpszText);

	int nIndex = m_listLog.AddString(strLine);
	if (nIndex < 0)
		return;

	m_listLog.SetTopIndex(nIndex);

	CDC* pDC = m_listLog.GetDC();
	if (pDC != nullptr)
	{
		CFont* pOldFont = pDC->SelectObject(m_listLog.GetFont());
		int nWidth = pDC->GetTextExtent(strLine).cx + 8;
		pDC->SelectObject(pOldFont);
		m_listLog.ReleaseDC(pDC);

		if (nWidth > m_nLogExtent)
		{
			m_nLogExtent = nWidth;
			m_listLog.SetHorizontalExtent(m_nLogExtent);
		}
	}
}

BOOL CIpcUIDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	g_hLogWnd = GetSafeHwnd();
	f_IpcSetLogHandler(&CIpcUIDlg::LogCallback, nullptr);

	if (f_IpcCoreInit() != IPC_PASS)
		AfxMessageBox(_T("IpcCore init failed"), MB_ICONERROR);

	UpdateData(FALSE);
	UpdateButtons();
	SetTimer(IPC_UI_TIMER_ID, 300, nullptr);

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CIpcUIDlg::OnDestroy()
{
	KillTimer(IPC_UI_TIMER_ID);

	CWaitCursor waitCursor;

	f_IpcCoreDeinit();
	g_hLogWnd = nullptr;
	f_IpcSetLogHandler(nullptr, nullptr);

	// 아직 큐에 남아 있는 로그 버퍼 반납
	MSG msg;
	while (::PeekMessage(&msg, GetSafeHwnd(), WM_IPC_LOG, WM_IPC_LOG, PM_REMOVE))
	{
		if (msg.lParam != 0)
			free(reinterpret_cast<void*>(msg.lParam));
	}

	CDialogEx::OnDestroy();
}

void CIpcUIDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == IPC_UI_TIMER_ID)
		UpdateButtons();

	CDialogEx::OnTimer(nIDEvent);
}

void CIpcUIDlg::UpdateButtons()
{
	const BOOL bThread = (f_IpcThreadDemoIsRunning() != 0);
	const BOOL bMsgQ   = (f_IpcMsgQDemoIsRunning()   != 0);
	const BOOL bTcp    = (f_IpcTcpDemoIsRunning()    != 0);

	GetDlgItem(IDC_BTN_TH_START)->EnableWindow(!bThread);
	GetDlgItem(IDC_BTN_TH_STOP)->EnableWindow(bThread);

	GetDlgItem(IDC_BTN_MQ_RECV)->EnableWindow(!bMsgQ);
	GetDlgItem(IDC_BTN_MQ_SEND)->EnableWindow(!bMsgQ);
	GetDlgItem(IDC_BTN_MQ_STOP)->EnableWindow(bMsgQ);
	GetDlgItem(IDC_EDIT_MQNAME)->EnableWindow(!bMsgQ);

	GetDlgItem(IDC_BTN_TCP_RECV)->EnableWindow(!bTcp);
	GetDlgItem(IDC_BTN_TCP_SEND)->EnableWindow(!bTcp);
	GetDlgItem(IDC_BTN_TCP_STOP)->EnableWindow(bTcp);
	GetDlgItem(IDC_EDIT_IP)->EnableWindow(!bTcp);
	GetDlgItem(IDC_EDIT_PORT)->EnableWindow(!bTcp);
	GetDlgItem(IDC_CHK_NBO)->EnableWindow(!bTcp);
}

void CIpcUIDlg::OnBnClickedThreadStart()
{
	f_IpcThreadDemoStart();
	UpdateButtons();
}

void CIpcUIDlg::OnBnClickedThreadStop()
{
	f_IpcThreadDemoStop();
	UpdateButtons();
}

void CIpcUIDlg::OnBnClickedMqRecv()
{
	if (!UpdateData(TRUE))
		return;

	f_IpcMsgQDemoStart(ToUtf8(m_strQueueName), 0);
	UpdateButtons();
}

void CIpcUIDlg::OnBnClickedMqSend()
{
	if (!UpdateData(TRUE))
		return;

	f_IpcMsgQDemoStart(ToUtf8(m_strQueueName), 1);
	UpdateButtons();
}

void CIpcUIDlg::OnBnClickedMqStop()
{
	f_IpcMsgQDemoStop();
	UpdateButtons();
}

void CIpcUIDlg::OnBnClickedTcpRecv()
{
	if (!UpdateData(TRUE))
		return;

	CStringA strIpA = ToUtf8(m_strIpAddr);
	f_IpcTcpDemoStart(0, reinterpret_cast<const SOCKET_CHAR8*>(static_cast<LPCSTR>(strIpA)),
		static_cast<SOCKET_UINT16>(m_nPortNum), m_bNetworkByteOrder ? 1 : 0);
	UpdateButtons();
}

void CIpcUIDlg::OnBnClickedTcpSend()
{
	if (!UpdateData(TRUE))
		return;

	CStringA strIpA = ToUtf8(m_strIpAddr);
	f_IpcTcpDemoStart(1, reinterpret_cast<const SOCKET_CHAR8*>(static_cast<LPCSTR>(strIpA)),
		static_cast<SOCKET_UINT16>(m_nPortNum), m_bNetworkByteOrder ? 1 : 0);
	UpdateButtons();
}

void CIpcUIDlg::OnBnClickedTcpStop()
{
	f_IpcTcpDemoStop();
	UpdateButtons();
}

void CIpcUIDlg::OnBnClickedLogClear()
{
	m_listLog.ResetContent();
	m_nLogExtent = 0;
	m_listLog.SetHorizontalExtent(0);
}

// 같은 exe 를 인자 없이 한 번 더 띄운다. 새 창은 처음 실행한 것과 똑같은 상태로 뜬다.
void CIpcUIDlg::OnBnClickedNewProcess()
{
	TCHAR szExePath[MAX_PATH] = { 0 };
	if (::GetModuleFileName(nullptr, szExePath, MAX_PATH) == 0)
	{
		AddLog(_T("[UI] GetModuleFileName failed"));
		return;
	}

	STARTUPINFO			si = { 0 };
	PROCESS_INFORMATION	pi = { 0 };
	si.cb = sizeof(si);

	if (::CreateProcess(szExePath, nullptr, nullptr, nullptr, FALSE,
		0, nullptr, nullptr, &si, &pi))
	{
		::CloseHandle(pi.hThread);
		::CloseHandle(pi.hProcess);
		AddLog(_T("[UI] new process"));
	}
	else
	{
		AddLog(_T("[UI] CreateProcess failed"));
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CIpcUIDlg::OnPaint()
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

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CIpcUIDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CIpcUIDlg::OnClose()
{
	CDialogEx::OnClose();
}

void CIpcUIDlg::OnOK()
{
	// Enter 키로 닫히지 않게 비워둔다
}

void CIpcUIDlg::OnCancel()
{
	CDialogEx::OnCancel();
}

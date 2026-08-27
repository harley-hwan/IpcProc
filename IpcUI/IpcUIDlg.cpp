//
// @file	IpcUIDlg.cpp
// @brief	메인 대화상자. 버튼으로 IpcCore 의 세 가지 IPC 데모를 켜고 끄고,
//			워커 쓰레드 로그를 리스트박스에 출력함
// @author	hwan
// @date	2026.08.19.
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

static volatile HWND	s_hLogWnd = nullptr;

// 코어는 UTF-8 문자열을 씀
static CStringA f_ToUtf8(const CString& str)
{
	CStringA strOut;

	INT32 nLen = ::WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, nullptr, nullptr);
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

// 워커 쓰레드에서 불림. UI 접근 없이 PostMessage 로 넘기고 바로 리턴함
VOID __cdecl CIpcUIDlg::LogCallback(VOID* vpUserCtx, INT32 iChannel, const CHAR* cpUtf8)
{
	UNREFERENCED_PARAMETER(vpUserCtx);

	HWND hWnd = s_hLogWnd;
	if ((hWnd == nullptr) || (cpUtf8 == nullptr))
		return;

	INT32 nLen = ::MultiByteToWideChar(CP_UTF8, 0, cpUtf8, -1, nullptr, 0);
	if (nLen <= 0)
		return;

	wchar_t* wcpText = static_cast<wchar_t*>(malloc(static_cast<size_t>(nLen) * sizeof(wchar_t)));
	if (wcpText == nullptr)
		return;

	if (::MultiByteToWideChar(CP_UTF8, 0, cpUtf8, -1, wcpText, nLen) <= 0)
	{
		free(wcpText);
		return;
	}

	if (!::PostMessage(hWnd, WM_IPC_LOG, static_cast<WPARAM>(iChannel), reinterpret_cast<LPARAM>(wcpText)))
		free(wcpText);
}

// UI 쓰레드. lParam 의 문자열을 출력하고 free 함
LRESULT CIpcUIDlg::OnIpcLog(WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);

	wchar_t* wcpText = reinterpret_cast<wchar_t*>(lParam);
	if (wcpText != nullptr)
	{
		AddLog(wcpText);
		free(wcpText);
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

	INT32 nIndex = m_listLog.AddString(strLine);
	if (nIndex < 0)
		return;

	m_listLog.SetTopIndex(nIndex);

	// 가로 스크롤 폭을 가장 긴 줄에 맞춰 늘림
	CDC* pDC = m_listLog.GetDC();
	if (pDC != nullptr)
	{
		CFont* pOldFont = pDC->SelectObject(m_listLog.GetFont());
		INT32 nWidth = pDC->GetTextExtent(strLine).cx + 8;
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

	SetIcon(m_hIcon, TRUE);
	SetIcon(m_hIcon, FALSE);

	s_hLogWnd = GetSafeHwnd();
	f_IpcSetLogHandler(&CIpcUIDlg::LogCallback, nullptr);

	if (f_IpcCoreInit() != IPC_PASS)
		AfxMessageBox(_T("IpcCore init failed"), MB_ICONERROR);

	UpdateData(FALSE);
	UpdateButtons();
	SetTimer(IPC_UI_TIMER_ID, 300, nullptr);

	return TRUE;
}

void CIpcUIDlg::OnDestroy()
{
	KillTimer(IPC_UI_TIMER_ID);

	CWaitCursor waitCursor;

	f_IpcCoreDeinit();
	s_hLogWnd = nullptr;
	f_IpcSetLogHandler(nullptr, nullptr);

	// 아직 큐에 남아 있는 로그 버퍼 반납함
	MSG st_Msg;
	while (::PeekMessage(&st_Msg, GetSafeHwnd(), WM_IPC_LOG, WM_IPC_LOG, PM_REMOVE))
	{
		if (st_Msg.lParam != 0)
			free(reinterpret_cast<void*>(st_Msg.lParam));
	}

	CDialogEx::OnDestroy();
}

void CIpcUIDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == IPC_UI_TIMER_ID)
		UpdateButtons();

	CDialogEx::OnTimer(nIDEvent);
}

// 데모 동작 상태에 맞춰 버튼/입력칸을 켜고 끔
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

	f_IpcMsgQDemoStart(f_ToUtf8(m_strQueueName), 0);
	UpdateButtons();
}

void CIpcUIDlg::OnBnClickedMqSend()
{
	if (!UpdateData(TRUE))
		return;

	f_IpcMsgQDemoStart(f_ToUtf8(m_strQueueName), 1);
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

	CStringA strIpA = f_ToUtf8(m_strIpAddr);
	f_IpcTcpDemoStart(0, reinterpret_cast<const INT8*>(static_cast<LPCSTR>(strIpA)),
		static_cast<UINT16>(m_nPortNum), m_bNetworkByteOrder ? 1 : 0);
	UpdateButtons();
}

void CIpcUIDlg::OnBnClickedTcpSend()
{
	if (!UpdateData(TRUE))
		return;

	CStringA strIpA = f_ToUtf8(m_strIpAddr);
	f_IpcTcpDemoStart(1, reinterpret_cast<const INT8*>(static_cast<LPCSTR>(strIpA)),
		static_cast<UINT16>(m_nPortNum), m_bNetworkByteOrder ? 1 : 0);
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

// 같은 exe 를 인자 없이 하나 더 띄움. 새 창은 처음 실행한 것과 똑같은 상태로 뜸
void CIpcUIDlg::OnBnClickedNewProcess()
{
	TCHAR szExePath[MAX_PATH] = { 0 };
	if (::GetModuleFileName(nullptr, szExePath, MAX_PATH) == 0)
	{
		AddLog(_T("[UI] GetModuleFileName failed"));
		return;
	}

	STARTUPINFO			st_StartupInfo = { 0 };
	PROCESS_INFORMATION	st_ProcInfo    = { 0 };
	st_StartupInfo.cb = sizeof(st_StartupInfo);

	if (::CreateProcess(szExePath, nullptr, nullptr, nullptr, FALSE,
		0, nullptr, nullptr, &st_StartupInfo, &st_ProcInfo))
	{
		::CloseHandle(st_ProcInfo.hThread);
		::CloseHandle(st_ProcInfo.hProcess);
		AddLog(_T("[UI] new process"));
	}
	else
	{
		AddLog(_T("[UI] CreateProcess failed"));
	}
}

// 최소화 상태면 아이콘을 직접 그림 (마법사 생성 코드)
void CIpcUIDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this);

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

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
	// Enter 키로 닫히지 않게 비워둠
}

void CIpcUIDlg::OnCancel()
{
	CDialogEx::OnCancel();
}

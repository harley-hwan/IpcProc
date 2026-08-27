
// IpcUI.cpp : Defines the class behaviors for the application.
//

#include "pch.h"
#include "framework.h"
#include "IpcUI.h"
#include "IpcUIDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CIpcUIApp

BEGIN_MESSAGE_MAP(CIpcUIApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CIpcUIApp construction

CIpcUIApp::CIpcUIApp()
{
	// support Restart Manager
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

	// Place all significant initialization in InitInstance
}


// The one and only CIpcUIApp object

CIpcUIApp theApp;

const GUID CDECL BASED_CODE _tlid =
		{0x81f8dbda,0x31e0,0x402d,{0xbe,0x98,0x11,0xee,0x58,0xa4,0xdd,0xa9}};
const WORD _wVerMajor = 1;
const WORD _wVerMinor = 0;


// CIpcUIApp initialization

BOOL CIpcUIApp::InitInstance()
{
	// InitCommonControlsEx() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// Set this to include all the common control classes you want to use
	// in your application.
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();

	if (!AfxSocketInit())
	{
		AfxMessageBox(IDP_SOCKETS_INIT_FAILED);
		return FALSE;
	}


	// Initialize OLE libraries
	if (!AfxOleInit())
	{
		AfxMessageBox(IDP_OLE_INIT_FAILED);
		return FALSE;
	}

	// Activate "Windows Native" visual manager for enabling themes in MFC controls
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	// Change the registry key under which our settings are stored
	SetRegistryKey(_T("IpcProc"));

	// Parse command line for automation or reg/unreg switches.
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	// App was launched with /Embedding or /Automation switch.
	// Run app as automation server.
	if (cmdInfo.m_bRunEmbedded || cmdInfo.m_bRunAutomated)
	{
		// Register class factories via CoRegisterClassObject().
		COleTemplateServer::RegisterAll();
	}
	// App was launched with /Unregserver or /Unregister switch.  Remove
	// entries from the registry.
	else if (cmdInfo.m_nShellCommand == CCommandLineInfo::AppUnregister)
	{
		COleObjectFactory::UpdateRegistryAll(FALSE);
		AfxOleUnregisterTypeLib(_tlid, _wVerMajor, _wVerMinor);
		return FALSE;
	}
	// /Register, /Regserver 로 "명시적으로" 실행했을 때만 레지스트리에 기록한다.
	//
	// 마법사가 만든 원래 코드는 그냥 실행할 때마다 여기를 타면서
	// HKEY_CLASSES_ROOT 에 쓰려고 하는데, HKCR 쓰기는 관리자 권한이 필요하므로
	// 일반 사용자로 실행하면 매번 조용히 실패한다.
	// 이 프로그램의 IPC 데모는 COM 자동화를 쓰지 않으므로 등록 자체가 필요 없다.
	// (자동화 서버로 쓰려면 관리자 권한 명령 프롬프트에서 IpcUI.exe /Regserver 를 한 번 실행)
	else if (cmdInfo.m_nShellCommand == CCommandLineInfo::AppRegister)
	{
		COleObjectFactory::UpdateRegistryAll();
		AfxOleRegisterTypeLib(AfxGetInstanceHandle(), _tlid);
		return FALSE;
	}

	// Create the shell manager, in case the dialog contains
	// any shell tree view or shell list view controls.
	// (등록 전용 실행 경로에서 새지 않도록 위의 조기 return 뒤에서 생성한다)
	CShellManager *pShellManager = new CShellManager;

	CIpcUIDlg dlg;
	m_pMainWnd = &dlg;
	INT_PTR nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// 대화상자가 OK 로 닫힌 경우
	}
	else if (nResponse == IDCANCEL)
	{
		// 대화상자가 Cancel 로 닫힌 경우
	}
	else if (nResponse == -1)
	{
		TRACE(traceAppMsg, 0, "Warning: dialog creation failed, so application is terminating unexpectedly.\n");
		TRACE(traceAppMsg, 0, "Warning: if you are using MFC controls on the dialog, you cannot #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS.\n");
	}

	// 지역 객체를 가리키던 포인터를 남겨두지 않는다
	m_pMainWnd = nullptr;

	// Delete the shell manager created above.
	if (pShellManager != nullptr)
	{
		delete pShellManager;
	}

#if !defined(_AFXDLL) && !defined(_AFX_NO_MFC_CONTROLS_IN_DIALOGS)
	ControlBarCleanUp();
#endif

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}

int CIpcUIApp::ExitInstance()
{
	AfxOleTerm(FALSE);

	return CWinApp::ExitInstance();
}

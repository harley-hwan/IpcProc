//
// @file	IpcUI.cpp
// @brief	앱 초기화 (MFC 마법사 코드 기반)
// @author	hwan
// @date	2026.08.19.
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


CIpcUIApp::CIpcUIApp()
{
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;
}


CIpcUIApp theApp;


BOOL CIpcUIApp::InitInstance()
{
	INITCOMMONCONTROLSEX st_InitCtrls;
	st_InitCtrls.dwSize = sizeof(st_InitCtrls);
	st_InitCtrls.dwICC  = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&st_InitCtrls);

	CWinApp::InitInstance();

	if (!AfxSocketInit())
	{
		AfxMessageBox(IDP_SOCKETS_INIT_FAILED);
		return FALSE;
	}

	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	SetRegistryKey(_T("IpcProc"));

	CShellManager *pShellManager = new CShellManager;

	CIpcUIDlg dlg;
	m_pMainWnd = &dlg;
	INT_PTR nResponse = dlg.DoModal();
	if (nResponse == -1)
	{
		TRACE(traceAppMsg, 0, "Warning: dialog creation failed, so application is terminating unexpectedly.\n");
		TRACE(traceAppMsg, 0, "Warning: if you are using MFC controls on the dialog, you cannot #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS.\n");
	}

	m_pMainWnd = nullptr;

	if (pShellManager != nullptr)
	{
		delete pShellManager;
	}

#if !defined(_AFXDLL) && !defined(_AFX_NO_MFC_CONTROLS_IN_DIALOGS)
	ControlBarCleanUp();
#endif

	return FALSE;
}

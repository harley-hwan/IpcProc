//
// @file	CoordUI.cpp
// @brief	앱 초기화 (MFC 마법사 코드 기반). 소켓은 안 쓰니 AfxSocketInit 은 없음
// @author	hwan
// @date	2026.09.02.
//
#include "pch.h"
#include "framework.h"
#include "CoordUI.h"
#include "CoordUIDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CCoordUIApp

BEGIN_MESSAGE_MAP(CCoordUIApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


CCoordUIApp::CCoordUIApp()
{
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;
}


CCoordUIApp theApp;


BOOL CCoordUIApp::InitInstance()
{
	INITCOMMONCONTROLSEX st_InitCtrls;
	st_InitCtrls.dwSize = sizeof(st_InitCtrls);
	st_InitCtrls.dwICC  = ICC_WIN95_CLASSES;	// 슬라이더, 리스트뷰가 여기 들어 있음
	InitCommonControlsEx(&st_InitCtrls);

	CWinApp::InitInstance();

	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	SetRegistryKey(_T("CoordFrames"));

	CShellManager *pShellManager = new CShellManager;

	CCoordUIDlg dlg;
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

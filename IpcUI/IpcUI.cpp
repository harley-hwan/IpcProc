//
// @file	IpcUI.cpp
// @brief	응용 초기화/종료 (MFC 마법사 생성 코드 기반)
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
	// Restart Manager 지원
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;
}


// 유일한 CIpcUIApp 객체
CIpcUIApp theApp;


BOOL CIpcUIApp::InitInstance()
{
	// 공용 컨트롤 초기화 (visual style 매니페스트 사용 시 필수임)
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

	// MFC 컨트롤 테마 적용
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	// 설정 저장용 레지스트리 키
	SetRegistryKey(_T("IpcProc"));

	// COM 자동화 미사용이라 마법사가 넣은 자동화 서버 등록 코드(idl/reg/typelib)는 제거함
	CShellManager *pShellManager = new CShellManager;

	CIpcUIDlg dlg;
	m_pMainWnd = &dlg;
	INT_PTR nResponse = dlg.DoModal();
	if (nResponse == -1)
	{
		TRACE(traceAppMsg, 0, "Warning: dialog creation failed, so application is terminating unexpectedly.\n");
		TRACE(traceAppMsg, 0, "Warning: if you are using MFC controls on the dialog, you cannot #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS.\n");
	}

	// 지역 객체를 가리키던 포인터를 남겨두지 않음
	m_pMainWnd = nullptr;

	if (pShellManager != nullptr)
	{
		delete pShellManager;
	}

#if !defined(_AFXDLL) && !defined(_AFX_NO_MFC_CONTROLS_IN_DIALOGS)
	ControlBarCleanUp();
#endif

	// 대화상자 기반이므로 메시지 펌프를 돌리지 않고 종료함
	return FALSE;
}

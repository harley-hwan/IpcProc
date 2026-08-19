
// IpcUI.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'pch.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols


// CIpcUIApp:
// See IpcUI.cpp for the implementation of this class
//

class CIpcUIApp : public CWinApp
{
public:
	CIpcUIApp();

// 명령행으로 전달받은 자동 시작 역할
//   IpcUI.exe /peer:<msgq-tx|msgq-rx|tcp-tx|tcp-rx> [/name:<큐이름>] [/ip:<주소>] [/port:<번호>] [/nbo]
// [상대 프로세스 실행] 버튼이 이 형식으로 자기 자신을 다시 띄운다.
public:
	CString	m_strAutoRole;
	CString	m_strAutoQueue;
	CString	m_strAutoIp;
	UINT	m_nAutoPort;
	BOOL	m_bAutoNbo;

	void ParseIpcArguments();

// Overrides
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern CIpcUIApp theApp;


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

// Overrides
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern CIpcUIApp theApp;

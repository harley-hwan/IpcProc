//
// @file	IpcUI.h
// @brief	IpcUI 응용 클래스 선언
// @author	hwan
// @date	2026.08.19.
//
#pragma once

#ifndef __AFXWIN_H__
	#error "include 'pch.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols


// CIpcUIApp
class CIpcUIApp : public CWinApp
{
public:
	CIpcUIApp();

// Overrides
public:
	virtual BOOL InitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern CIpcUIApp theApp;

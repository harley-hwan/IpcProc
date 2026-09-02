//
// @file	CoordUI.h
// @brief	CoordUI 앱 클래스
// @author	hwan
// @date	2026.09.02.
//
#pragma once

#ifndef __AFXWIN_H__
	#error "include 'pch.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols


// CCoordUIApp
class CCoordUIApp : public CWinApp
{
public:
	CCoordUIApp();

// Overrides
public:
	virtual BOOL InitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern CCoordUIApp theApp;

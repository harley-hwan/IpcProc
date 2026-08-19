
// DlgProxy.h: header file
//

#pragma once

class CIpcUIDlg;


// CIpcUIDlgAutoProxy command target

class CIpcUIDlgAutoProxy : public CCmdTarget
{
	DECLARE_DYNCREATE(CIpcUIDlgAutoProxy)

	CIpcUIDlgAutoProxy();           // protected constructor used by dynamic creation

// Attributes
public:
	CIpcUIDlg* m_pDialog;

// Operations
public:

// Overrides
	public:
	virtual void OnFinalRelease();

// Implementation
protected:
	virtual ~CIpcUIDlgAutoProxy();

	// Generated message map functions

	DECLARE_MESSAGE_MAP()
	DECLARE_OLECREATE(CIpcUIDlgAutoProxy)

	// Generated OLE dispatch map functions

	DECLARE_DISPATCH_MAP()
	DECLARE_INTERFACE_MAP()
};


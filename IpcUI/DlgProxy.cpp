
// DlgProxy.cpp : implementation file
//

#include "pch.h"
#include "framework.h"
#include "IpcUI.h"
#include "DlgProxy.h"
#include "IpcUIDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CIpcUIDlgAutoProxy

IMPLEMENT_DYNCREATE(CIpcUIDlgAutoProxy, CCmdTarget)

CIpcUIDlgAutoProxy::CIpcUIDlgAutoProxy()
	: m_pDialog(nullptr)		// 아래 조건이 모두 실패하면 소멸자가 미초기화 포인터를 만지게 되므로 반드시 초기화
{
	EnableAutomation();

	// To keep the application running as long as an automation
	//	object is active, the constructor calls AfxOleLockApp.
	AfxOleLockApp();

	// Get access to the dialog through the application's
	//  main window pointer.  Set the proxy's internal pointer
	//  to point to the dialog, and set the dialog's back pointer to
	//  this proxy.
	ASSERT_VALID(AfxGetApp()->m_pMainWnd);
	if (AfxGetApp()->m_pMainWnd)
	{
		ASSERT_KINDOF(CIpcUIDlg, AfxGetApp()->m_pMainWnd);
		if (AfxGetApp()->m_pMainWnd->IsKindOf(RUNTIME_CLASS(CIpcUIDlg)))
		{
			m_pDialog = reinterpret_cast<CIpcUIDlg*>(AfxGetApp()->m_pMainWnd);
			m_pDialog->m_pAutoProxy = this;
		}
	}
}

CIpcUIDlgAutoProxy::~CIpcUIDlgAutoProxy()
{
	// To terminate the application when all objects created with
	// 	with automation, the destructor calls AfxOleUnlockApp.
	//  Among other things, this will destroy the main dialog
	if (m_pDialog != nullptr)
		m_pDialog->m_pAutoProxy = nullptr;
	AfxOleUnlockApp();
}

void CIpcUIDlgAutoProxy::OnFinalRelease()
{
	// When the last reference for an automation object is released
	// OnFinalRelease is called.  The base class will automatically
	// deletes the object.  Add additional cleanup required for your
	// object before calling the base class.

	CCmdTarget::OnFinalRelease();
}

BEGIN_MESSAGE_MAP(CIpcUIDlgAutoProxy, CCmdTarget)
END_MESSAGE_MAP()

BEGIN_DISPATCH_MAP(CIpcUIDlgAutoProxy, CCmdTarget)
END_DISPATCH_MAP()

// Note: we add support for IID_IIpcUI to support typesafe binding
//  from VBA.  This IID must match the GUID that is attached to the
//  dispinterface in the .IDL file.

// {f692bc46-a7b5-4bdb-add5-df7c685a77f1}
static const IID IID_IIpcUI =
{0xf692bc46,0xa7b5,0x4bdb,{0xad,0xd5,0xdf,0x7c,0x68,0x5a,0x77,0xf1}};

BEGIN_INTERFACE_MAP(CIpcUIDlgAutoProxy, CCmdTarget)
	INTERFACE_PART(CIpcUIDlgAutoProxy, IID_IIpcUI, Dispatch)
END_INTERFACE_MAP()

// The IMPLEMENT_OLECREATE2 macro is defined in pch.h of this project
// {cddd0ec2-cb6c-47ea-bb51-44dca6eaae8c}
IMPLEMENT_OLECREATE2(CIpcUIDlgAutoProxy, "IpcUI.Application", 0xcddd0ec2,0xcb6c,0x47ea,0xbb,0x51,0x44,0xdc,0xa6,0xea,0xae,0x8c)


// CIpcUIDlgAutoProxy message handlers

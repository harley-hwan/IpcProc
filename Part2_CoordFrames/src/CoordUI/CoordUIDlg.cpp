//
// @file	CoordUIDlg.cpp
// @brief	메인 대화상자. 입력은 도 단위로 받아 라디안으로 바꿔 CoordCore 에 넘기고,
//			입력칸이나 슬라이더가 바뀌면 바로 다시 계산해서 단계별 결과를 표에 찍음.
//			변환 로직은 여기 없고 f_* 함수만 부름
// @author	hwan
// @date	2026.09.02.
//
#include "pch.h"
#include "framework.h"
#include <cmath>
#include "CoordUI.h"
#include "CoordUIDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// 슬라이더는 정수만 되니까 0.1 도 단위로 씀
#define COORD_UI_SLIDER_SCALE	10

// 표 행 번호
enum {
	enum_Row_FwdAnt = 0,
	enum_Row_FwdBody,
	enum_Row_FwdNed,
	enum_Row_FwdEcef,
	enum_Row_ShipEcef,
	enum_Row_FwdLla,
	enum_Row_InvEcef,
	enum_Row_InvNed,
	enum_Row_InvBody,
	enum_Row_InvAnt,
	enum_Row_InvPolar,
	enum_Row_Count
};


// CCoordUIDlg dialog

IMPLEMENT_DYNAMIC(CCoordUIDlg, CDialogEx);

CCoordUIDlg::CCoordUIDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_COORDUI_DIALOG, pParent)
	, m_bReady(FALSE)
{
	EnableActiveAccessibility();
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

CCoordUIDlg::~CCoordUIDlg()
{
}

void CCoordUIDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_STEPS,   m_listSteps);
	DDX_Control(pDX, IDC_SLIDER_ROLL,  m_sliderRoll);
	DDX_Control(pDX, IDC_SLIDER_PITCH, m_sliderPitch);
	DDX_Control(pDX, IDC_SLIDER_YAW,   m_sliderYaw);
}

BEGIN_MESSAGE_MAP(CCoordUIDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_HSCROLL()
	ON_CONTROL_RANGE(EN_CHANGE, IDC_EDIT_RANGE, IDC_EDIT_OFF_Z, &CCoordUIDlg::OnEnChangeInput)
	ON_BN_CLICKED(IDC_BTN_RESET, &CCoordUIDlg::OnBnClickedReset)
END_MESSAGE_MAP()


// CCoordUIDlg message handlers

BOOL CCoordUIDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	SetIcon(m_hIcon, TRUE);
	SetIcon(m_hIcon, FALSE);

	// 단계별 결과 표. 정변환 5행 + 함선 위치 + 역변환 5행
	m_listSteps.SetExtendedStyle(m_listSteps.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_listSteps.InsertColumn(0, _T("단계"),      LVCFMT_LEFT,  200);
	m_listSteps.InsertColumn(1, _T("x / N / X"), LVCFMT_RIGHT, 118);
	m_listSteps.InsertColumn(2, _T("y / E / Y"), LVCFMT_RIGHT, 118);
	m_listSteps.InsertColumn(3, _T("z / D / Z"), LVCFMT_RIGHT, 118);
	for (INT32 i = 0; i < enum_Row_Count; i++)
	{
		m_listSteps.InsertItem(i, _T(""));
	}

	// 자세 슬라이더. 0.1 도 단위, 눈금은 roll/pitch 10 도, yaw 45 도마다
	m_sliderRoll.SetRange(-45 * COORD_UI_SLIDER_SCALE, 45 * COORD_UI_SLIDER_SCALE);
	m_sliderRoll.SetTicFreq(10 * COORD_UI_SLIDER_SCALE);
	m_sliderPitch.SetRange(-30 * COORD_UI_SLIDER_SCALE, 30 * COORD_UI_SLIDER_SCALE);
	m_sliderPitch.SetTicFreq(10 * COORD_UI_SLIDER_SCALE);
	m_sliderYaw.SetRange(0, 360 * COORD_UI_SLIDER_SCALE);
	m_sliderYaw.SetTicFreq(45 * COORD_UI_SLIDER_SCALE);

	m_bReady = TRUE;
	OnBnClickedReset();		// 과제 예제값 채우고 한 번 계산

	return TRUE;
}

FLOAT64 CCoordUIDlg::GetEditDouble(INT32 nId) const
{
	CString str;

	GetDlgItemText(nId, str);
	return _tcstod(str, nullptr);
}

void CCoordUIDlg::SetEditDouble(INT32 nId, FLOAT64 dValue, INT32 nDigits)
{
	CString str;

	str.Format(_T("%.*f"), nDigits, dValue);
	SetDlgItemText(nId, str);
}

// 구조체는 라디안이고 화면은 도. 바꾸는 건 여기서 한 번만
void CCoordUIDlg::SetInputs(const ST_Polar& stMeas, const ST_Lla& stShip, const ST_Attitude& stAtt, const ST_Mount& stMount)
{
	const BOOL bReady = m_bReady;

	m_bReady = FALSE;		// 칸 하나 채울 때마다 계산하지 않게 잠시 끔
	SetEditDouble(IDC_EDIT_RANGE,      stMeas.dRange, 1);
	SetEditDouble(IDC_EDIT_AZ,         RAD2DEG(stMeas.dAz), 3);
	SetEditDouble(IDC_EDIT_EL,         RAD2DEG(stMeas.dEl), 3);
	SetEditDouble(IDC_EDIT_LAT,        RAD2DEG(stShip.dLat), 6);
	SetEditDouble(IDC_EDIT_LON,        RAD2DEG(stShip.dLon), 6);
	SetEditDouble(IDC_EDIT_ALT,        stShip.dAlt, 1);
	SetEditDouble(IDC_EDIT_ROLL,       RAD2DEG(stAtt.dRoll), 1);
	SetEditDouble(IDC_EDIT_PITCH,      RAD2DEG(stAtt.dPitch), 1);
	SetEditDouble(IDC_EDIT_YAW,        RAD2DEG(stAtt.dYaw), 1);
	SetEditDouble(IDC_EDIT_MOUNT_AZ,   RAD2DEG(stMount.dAz), 3);
	SetEditDouble(IDC_EDIT_MOUNT_TILT, RAD2DEG(stMount.dTilt), 3);
	SetEditDouble(IDC_EDIT_OFF_X,      stMount.stOffset.dX, 1);
	SetEditDouble(IDC_EDIT_OFF_Y,      stMount.stOffset.dY, 1);
	SetEditDouble(IDC_EDIT_OFF_Z,      stMount.stOffset.dZ, 1);
	m_bReady = bReady;

	SyncSliders();
}

void CCoordUIDlg::ReadInputs(ST_Polar& stMeas, ST_Lla& stShip, ST_Attitude& stAtt, ST_Mount& stMount) const
{
	stMeas.dRange       = GetEditDouble(IDC_EDIT_RANGE);
	stMeas.dAz          = DEG2RAD(GetEditDouble(IDC_EDIT_AZ));
	stMeas.dEl          = DEG2RAD(GetEditDouble(IDC_EDIT_EL));
	stShip.dLat         = DEG2RAD(GetEditDouble(IDC_EDIT_LAT));
	stShip.dLon         = DEG2RAD(GetEditDouble(IDC_EDIT_LON));
	stShip.dAlt         = GetEditDouble(IDC_EDIT_ALT);
	stAtt.dRoll         = DEG2RAD(GetEditDouble(IDC_EDIT_ROLL));
	stAtt.dPitch        = DEG2RAD(GetEditDouble(IDC_EDIT_PITCH));
	stAtt.dYaw          = DEG2RAD(GetEditDouble(IDC_EDIT_YAW));
	stMount.dAz         = DEG2RAD(GetEditDouble(IDC_EDIT_MOUNT_AZ));
	stMount.dTilt       = DEG2RAD(GetEditDouble(IDC_EDIT_MOUNT_TILT));
	stMount.stOffset.dX = GetEditDouble(IDC_EDIT_OFF_X);
	stMount.stOffset.dY = GetEditDouble(IDC_EDIT_OFF_Y);
	stMount.stOffset.dZ = GetEditDouble(IDC_EDIT_OFF_Z);
}

// 편집칸 값을 슬라이더 위치로. SetPos 는 WM_HSCROLL 을 안 보내서 되돌아오지 않음
void CCoordUIDlg::SyncSliders()
{
	m_sliderRoll.SetPos(static_cast<INT32>(std::floor(GetEditDouble(IDC_EDIT_ROLL)  * COORD_UI_SLIDER_SCALE + 0.5)));
	m_sliderPitch.SetPos(static_cast<INT32>(std::floor(GetEditDouble(IDC_EDIT_PITCH) * COORD_UI_SLIDER_SCALE + 0.5)));
	m_sliderYaw.SetPos(static_cast<INT32>(std::floor(GetEditDouble(IDC_EDIT_YAW)   * COORD_UI_SLIDER_SCALE + 0.5)));
}

void CCoordUIDlg::SetRow(INT32 nRow, LPCTSTR lpszLabel, FLOAT64 dA, FLOAT64 dB, FLOAT64 dC, INT32 nDigits)
{
	CString str;

	m_listSteps.SetItemText(nRow, 0, lpszLabel);
	str.Format(_T("%.*f"), nDigits, dA);
	m_listSteps.SetItemText(nRow, 1, str);
	str.Format(_T("%.*f"), nDigits, dB);
	m_listSteps.SetItemText(nRow, 2, str);
	str.Format(_T("%.*f"), nDigits, dC);
	m_listSteps.SetItemText(nRow, 3, str);
}

// main.c 와 같은 순서. 정변환 5단계 -> 역변환 5단계 -> 최종 출력 두 가지
void CCoordUIDlg::Calculate()
{
	ST_Polar    stMeas;
	ST_Lla      stShip;
	ST_Attitude stAtt;
	ST_Mount    stMount;
	ST_Vec3     stAnt;
	ST_Vec3     stBody;
	ST_Vec3     stNed;
	ST_Vec3     stEcef;
	ST_Vec3     stShipEcef;
	ST_Lla      stTarget;
	ST_Vec3     stEcefBack;
	ST_Vec3     stNedBack;
	ST_Vec3     stBodyBack;
	ST_Vec3     stAntBack;
	ST_Polar    stBack;
	CString     str;

	ReadInputs(stMeas, stShip, stAtt, stMount);
	if (stMeas.dRange <= 0.0)
	{
		SetDlgItemText(IDC_STATIC_ERR, _T("거리 R 은 0 보다 커야 함"));
		return;
	}

	// 정변환. 안테나에서 출발해서 한 단계씩 밖으로
	stAnt      = f_PolarToXyz(stMeas);
	stBody     = f_AntToBody(stMount, stAnt);
	stNed      = f_BodyToNed(stAtt, stBody);
	stEcef     = f_NedToEcef(stShip, stNed);
	stShipEcef = f_LlaToEcef(stShip);
	stTarget   = f_EcefToLla(stEcef);

	// 역변환. 표적 LLA 에서 다시 안테나까지. 입력값이 그대로 나와야 정상
	stEcefBack = f_LlaToEcef(stTarget);
	stNedBack  = f_EcefToNed(stShip, stEcefBack);
	stBodyBack = f_NedToBody(stAtt, stNedBack);
	stAntBack  = f_BodyToAnt(stMount, stBodyBack);
	stBack     = f_XyzToPolar(stAntBack);

	SetRow(enum_Row_FwdAnt,   _T("정변환 1) 극좌표 -> 안테나 직교 (x, y, z)"), stAnt.dX, stAnt.dY, stAnt.dZ, 4);
	SetRow(enum_Row_FwdBody,  _T("정변환 2) 안테나 -> 동체 (x, y, z)"),        stBody.dX, stBody.dY, stBody.dZ, 4);
	SetRow(enum_Row_FwdNed,   _T("정변환 3) 동체 -> NED (N, E, D)"),          stNed.dX, stNed.dY, stNed.dZ, 4);
	SetRow(enum_Row_FwdEcef,  _T("정변환 4) NED -> ECEF (X, Y, Z)"),          stEcef.dX, stEcef.dY, stEcef.dZ, 4);
	SetRow(enum_Row_ShipEcef, _T("        함선 ECEF (X, Y, Z)"),               stShipEcef.dX, stShipEcef.dY, stShipEcef.dZ, 4);
	SetRow(enum_Row_FwdLla,   _T("정변환 5) ECEF -> LLA (lat, lon, alt)"),    RAD2DEG(stTarget.dLat), RAD2DEG(stTarget.dLon), stTarget.dAlt, 6);
	SetRow(enum_Row_InvEcef,  _T("역변환 1) LLA -> ECEF (X, Y, Z)"),          stEcefBack.dX, stEcefBack.dY, stEcefBack.dZ, 4);
	SetRow(enum_Row_InvNed,   _T("역변환 2) ECEF -> NED (N, E, D)"),          stNedBack.dX, stNedBack.dY, stNedBack.dZ, 4);
	SetRow(enum_Row_InvBody,  _T("역변환 3) NED -> 동체 (x, y, z)"),          stBodyBack.dX, stBodyBack.dY, stBodyBack.dZ, 4);
	SetRow(enum_Row_InvAnt,   _T("역변환 4) 동체 -> 안테나 직교 (x, y, z)"),  stAntBack.dX, stAntBack.dY, stAntBack.dZ, 4);
	SetRow(enum_Row_InvPolar, _T("역변환 5) 안테나 직교 -> 극좌표 (R, Az, El)"), stBack.dRange, RAD2DEG(stBack.dAz), RAD2DEG(stBack.dEl), 6);

	// 최종 출력. 조건 6 이 요구하는 두 가지
	str.Format(_T("lat = %.9f deg,  lon = %.9f deg,  alt = %.4f m"),
		RAD2DEG(stTarget.dLat), RAD2DEG(stTarget.dLon), stTarget.dAlt);
	SetDlgItemText(IDC_STATIC_LLA, str);

	str.Format(_T("R = %.4f m,  Az = %.6f deg,  El = %.6f deg"),
		stBack.dRange, RAD2DEG(stBack.dAz), RAD2DEG(stBack.dEl));
	SetDlgItemText(IDC_STATIC_POLAR, str);

	str.Format(_T("dR = %.1e m,  dAz = %.1e deg,  dEl = %.1e deg"),
		std::fabs(stBack.dRange - stMeas.dRange),
		std::fabs(RAD2DEG(stBack.dAz - stMeas.dAz)),
		std::fabs(RAD2DEG(stBack.dEl - stMeas.dEl)));
	SetDlgItemText(IDC_STATIC_ERR, str);
}

// 슬라이더를 움직이면 편집칸에 값을 써 넣고, 그 EN_CHANGE 가 계산까지 이어짐
void CCoordUIDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	if (pScrollBar != nullptr)
	{
		switch (pScrollBar->GetDlgCtrlID())
		{
		case IDC_SLIDER_ROLL:
			SetEditDouble(IDC_EDIT_ROLL,  static_cast<FLOAT64>(m_sliderRoll.GetPos())  / COORD_UI_SLIDER_SCALE, 1);
			break;
		case IDC_SLIDER_PITCH:
			SetEditDouble(IDC_EDIT_PITCH, static_cast<FLOAT64>(m_sliderPitch.GetPos()) / COORD_UI_SLIDER_SCALE, 1);
			break;
		case IDC_SLIDER_YAW:
			SetEditDouble(IDC_EDIT_YAW,   static_cast<FLOAT64>(m_sliderYaw.GetPos())   / COORD_UI_SLIDER_SCALE, 1);
			break;
		default:
			break;
		}
	}

	CDialogEx::OnHScroll(nSBCode, nPos, pScrollBar);
}

// 입력칸이 바뀌면 바로 다시 계산. 자세 칸이면 슬라이더도 맞춰 줌
void CCoordUIDlg::OnEnChangeInput(UINT nId)
{
	if (!m_bReady)
	{
		return;
	}

	if ((nId == IDC_EDIT_ROLL) || (nId == IDC_EDIT_PITCH) || (nId == IDC_EDIT_YAW))
	{
		SyncSliders();
	}

	Calculate();
}

// 과제 조건 값. 안테나 위치는 동체 기준이라 30 m 뒤 = x -30, 10 m 위 = z -10
void CCoordUIDlg::OnBnClickedReset()
{
	ST_Polar    stMeas  = { 20000.0, DEG2RAD(30.0), DEG2RAD(0.0) };
	ST_Lla      stShip  = { DEG2RAD(36.408), DEG2RAD(127.307), 0.0 };
	ST_Attitude stAtt   = { DEG2RAD(0.0), DEG2RAD(0.0), DEG2RAD(45.0) };		// roll, pitch, yaw
	ST_Mount    stMount = { DEG2RAD(90.0), DEG2RAD(0.0), { -30.0, 0.0, -10.0 } };

	SetInputs(stMeas, stShip, stAtt, stMount);
	Calculate();
}

void CCoordUIDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this);

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

HCURSOR CCoordUIDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

// Enter 는 창을 닫지 않고 계산
void CCoordUIDlg::OnOK()
{
	Calculate();
}

void CCoordUIDlg::OnCancel()
{
	CDialogEx::OnCancel();
}

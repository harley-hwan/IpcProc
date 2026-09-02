//
// @file	CoordUIDlg.h
// @brief	메인 대화상자
// @author	hwan
// @date	2026.09.02.
//
#pragma once

#include "coord_frames.h"


// CCoordUIDlg dialog
class CCoordUIDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CCoordUIDlg);

// Construction
public:
	CCoordUIDlg(CWnd* pParent = nullptr);
	virtual ~CCoordUIDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_COORDUI_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON		m_hIcon;

	CListCtrl	m_listSteps;
	CSliderCtrl	m_sliderRoll;
	CSliderCtrl	m_sliderPitch;
	CSliderCtrl	m_sliderYaw;
	BOOL		m_bReady;		// 입력칸 채우는 중에는 EN_CHANGE 마다 계산하지 않게

	FLOAT64	GetEditDouble(INT32 nId) const;
	void	SetEditDouble(INT32 nId, FLOAT64 dValue, INT32 nDigits);
	void	SetInputs(const ST_Polar& stMeas, const ST_Lla& stShip, const ST_Attitude& stAtt, const ST_Mount& stMount);
	void	ReadInputs(ST_Polar& stMeas, ST_Lla& stShip, ST_Attitude& stAtt, ST_Mount& stMount) const;
	void	SyncSliders();
	void	SetRow(INT32 nRow, LPCTSTR lpszLabel, FLOAT64 dA, FLOAT64 dB, FLOAT64 dC, INT32 nDigits);
	void	Calculate();

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnEnChangeInput(UINT nId);

	afx_msg void OnBnClickedReset();

	virtual void OnOK();
	virtual void OnCancel();
	DECLARE_MESSAGE_MAP()
};

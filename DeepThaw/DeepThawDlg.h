
// DeepThawDlg.h: 头文件
//
#pragma once

#include "framework.h"
#include "InterFace_DeepFrz.h"

//启用视觉样式6
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// CDeepThawDlg 对话框
class CDeepThawDlg : public CDialogEx
{
// 构造
public:
	CDeepThawDlg(CWnd* pParent = nullptr);	// 标准构造函数

private:
	HICON m_hIcon;

	CButton* m_pButtonForceMode = nullptr;
	CButton * m_pButtonSetting = nullptr,
		* m_pButtonDelete = nullptr;

	CEdit * m_pEdit = nullptr;

	std::wstring m_Edit_Content;

	bool m_DeepFrzStatus = false;

protected:

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();

	afx_msg void OnButtonSetting();		
	afx_msg void OnButtonDelete();
	afx_msg void OnForceModeCheck();
	afx_msg BOOL OnQueryEndSession();

	afx_msg void OnPaint();
	DECLARE_MESSAGE_MAP()
};

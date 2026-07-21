
// DeepThawDlg.cpp: 实现文件
//

#include "framework.h"
#include "DeepThaw.h"
#include "DeepThawDlg.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CDeepThawDlg 对话框

CDeepThawDlg::CDeepThawDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DEEPTHAW_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

BEGIN_MESSAGE_MAP(CDeepThawDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BUTTON_SETTING, OnButtonSetting)
	ON_BN_CLICKED(IDC_BUTTON_DELETE , OnButtonDelete)
	ON_WM_PAINT()
END_MESSAGE_MAP()


// CDeepThawDlg 消息处理程序

BOOL CDeepThawDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();


	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	m_pButtonDelete		= (CButton*)	GetDlgItem(IDC_BUTTON_DELETE);
	m_pButtonSetting	= (CButton*)	GetDlgItem(IDC_BUTTON_SETTING);
	m_pEdit				= (CEdit*)		GetDlgItem(IDC_EDIT);
	
	m_pButtonDelete->SetWindowText(L"Delete all reboot restore software");

	std::wstring Version;
	if (Interface_DeepFrz::getDeepFrzVersion(Version) == false)
	{
		m_Edit_Content += L"Connot find Deep Freeze on this computer\r\n";
		m_pEdit->SetWindowText(m_Edit_Content.c_str());

		return true;
	}
	
	m_Edit_Content += L"Find Deep Freeze on this computer\r\n";
	m_Edit_Content += L"Its Version is " + Version + L"\r\n";

	if (Interface_DeepFrz::getDeepFrzStatus())
	{
		m_Edit_Content += L"Status: Running\r\n";
		m_DeepFrzStatus = true;

		m_pButtonSetting->SetWindowText(L"Disable all reboot-restore software");
	
	}
	else
	{
		m_Edit_Content += L"Status: Stopped\r\n";
		m_DeepFrzStatus = false;					//也多写一句吧

		m_pButtonSetting->SetWindowText(L"Enable all reboot-restore software");
	}

	m_Edit_Content += L"\r\n";

	m_Edit_Content += L"CAUTION:\r\n"
		L"Disable reboot-restore software before deletion.\r\n";

	m_pButtonSetting->EnableWindow();
	m_pButtonDelete ->EnableWindow();

	m_pEdit->SetWindowText(m_Edit_Content.c_str());
	

	return FALSE;		//焦点不要在控件上  
}



void CDeepThawDlg::OnButtonDelete()
{
	if (m_DeepFrzStatus)
	{
		MessageBox(L"You must disable reboot-restore software before deletion.", L"DeepThaw", MB_ICONERROR | MB_OK);
	}
	else
	{
		auto ButtonID=
			MessageBoxW(L"This operation will only delete the files left by Deep Freeze on this computer; it will not remove the registry entries it has left.\n"
				L"After using this operation to delete them, Deep Freeze can never be installed on this computer again. Are you sure you want to continue?\n\n"
				L"You may also use the official uninstaller to remove it, which will allow this computer to have Deep Freeze reinstalled afterward.",
				L"Something you definitely need to know...", MB_ICONINFORMATION | MB_YESNO);
		if (ButtonID == IDNO)
			return;

		LPSTR pathWillBeRemoved[] =
		{
			"del /f /s /q %ProgramFiles(x86)%\\Faronics",		//这里要做x86适配
			"del /f /s /q %SystemRoot%\\System32\\drivers\\DeepFrz.sys ",
			"del /f /s /q %SystemRoot%\\System32\\drivers\\DFRegMon.sys",
			"del /f /s /q %SystemRoot%\\System32\\drivers\\DfDiskLo.sys",
			"del /f /s /q %SystemRoot%\\System32\\drivers\\FarDisk.sys",
			"del /f /s /q %SystemRoot%\\System32\\drivers\\FarSpace.sys"
		};

		for (auto& path : pathWillBeRemoved)
			system(path);						//不需要再做返回检查了

		std::wstring Version;
		if (Interface_DeepFrz::getDeepFrzVersion(Version) == false)
		{
			MessageBox(L"DeepFreeze has been removed successfully",L"DeepThaw", MB_ICONINFORMATION | MB_OK);
			m_Edit_Content += L"Connot find Deep Freeze on this computer\r\n";
			m_pEdit->SetWindowText(m_Edit_Content.c_str());

			m_pButtonDelete->EnableWindow(0);
			m_pButtonSetting->EnableWindow(0);
		}
		else
			MessageBox(L"Failed to remove DeepFreeze", L"DeepThaw", MB_ICONERROR | MB_OK);
		
	}

}

#include "InterFace_DeepFrz.h"
//NtShutdownSystem 函数 定义
typedef enum _SHUTDOWN_ACTION {
	ShutdownNoReboot,  // 关机
	ShutdownReboot,    // 重启
	ShutdownPowerOff   // 断电
} SHUTDOWN_ACTION, * PSHUTDOWN_ACTION;

typedef LONG NTSTATUS;

typedef 
NTSTATUS(__stdcall * type_NtShutdownSystem)(SHUTDOWN_ACTION Action);

void CDeepThawDlg::OnButtonSetting()
{
	auto ButtonID=
	MessageBox(L"After the operation, your computer will automatically restart, regardless of whether it was successful. \n"
		L"Do you want to continue?", L"Something you definitely need to know...", MB_YESNO | MB_ICONINFORMATION);

	if (ButtonID == IDNO)
		return;

	Interface_DeepFrz::setDeepFrzStatus(!m_DeepFrzStatus);

	type_NtShutdownSystem NtShutdownSystem = nullptr;
	NtShutdownSystem = (type_NtShutdownSystem)GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtShutdownSystem");
	
	NtShutdownSystem(ShutdownReboot);
	
	ExitProcess(0);

}

void CDeepThawDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

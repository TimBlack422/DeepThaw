// DeepThaw.cpp: 定义应用程序的类行为。
//
#include "framework.h"
#include "DeepThaw.h"
#include "DeepThawDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

SC_HANDLE hService_DeepThawKernel = nullptr;

// CDeepThawApp
BEGIN_MESSAGE_MAP(CDeepThawApp, CWinApp)
END_MESSAGE_MAP()


// CDeepThawApp 构造			显然是没啥用
CDeepThawApp::CDeepThawApp()
{
}


// 唯一的 CDeepThawApp 对象
CDeepThawApp theApp;

void LoadDriver();
void UnloadDriver();

// CDeepThawApp 初始化

BOOL CDeepThawApp::InitInstance()
{
	CWinApp::InitInstance();

	CShellManager *pShellManager = new CShellManager;
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	//操作系统版本排除
	//if (!IsWindows10OrGreater())
	//{
	//	AfxMessageBox(L"Please use Windows 10 or a later version of the operating system to run this tool.\n", MB_OK | MB_ICONERROR);
	//	return FALSE;
	//}

	//加载驱动
	LoadDriver();

	//获取SE_SHUTDOWN_NAME特权
	HANDLE hToken{ 0 };
	OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &hToken);	//这里就不检查返回值了，后面有人帮我们检查的

	TOKEN_PRIVILEGES privilegesWantToEnable{ 0 };
	privilegesWantToEnable.PrivilegeCount = 1;
	privilegesWantToEnable.Privileges->Attributes = SE_PRIVILEGE_ENABLED;

	LookupPrivilegeValue(nullptr, SE_SHUTDOWN_NAME, &privilegesWantToEnable.Privileges[0].Luid);

	if (!AdjustTokenPrivileges(hToken, false, &privilegesWantToEnable, 0, 0, 0))
	{
		AfxMessageBox(L"Failed to adjust token privileges!\n"
			L"Please restart your computer.", MB_OK | MB_ICONERROR);
	}

	CloseHandle(hToken);

	CDeepThawDlg dlg;
	m_pMainWnd = &dlg;
	INT_PTR nResponse = dlg.DoModal();

	//卸载驱动
	UnloadDriver();

	return FALSE;
}

void LoadDriver()
{
	SC_HANDLE hSCManager =
		OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
	if(hSCManager == nullptr)
	{
		AfxMessageBox(L"Failed to load the Driver!\nRestart your computer and then try opening this tool again.\n\nIf it still doesn't work, please disable the Driver Signature Enforcement feature on your computer.", MB_ICONERROR | MB_OK);
		ExitProcess(0);
	}
	hService_DeepThawKernel = OpenService(hSCManager, L"DeepThaw_Kernel", SERVICE_ALL_ACCESS);
	if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST)
	{
createService:					//这是个不好的习惯，大家请不要学习我
		std::wstring driverPath;
		WCHAR currentDirectory[MAX_PATH]{ 0 };  
		if (!GetCurrentDirectory(MAX_PATH, currentDirectory))
		{
			AfxMessageBox(L"Failed to load the Driver!\nRestart your computer and then try opening this tool again.\n\nIf it still doesn't work, please disable the Driver Signature Enforcement feature on your computer.", MB_ICONERROR | MB_OK);
			ExitProcess(0);
		}

		driverPath += L"\\??\\";
		driverPath += currentDirectory;
		driverPath += L"\\DeepThaw_Kernel.sys";

		hService_DeepThawKernel =
			CreateService(hSCManager,
				L"DeepThaw_Kernel",
				L"DeepThaw_Kernel",
				SERVICE_ALL_ACCESS,
				SERVICE_KERNEL_DRIVER,
				SERVICE_DEMAND_START,
				SERVICE_ERROR_NORMAL,
				driverPath.c_str(),
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr
				);
		if(!hService_DeepThawKernel)
		{
			AfxMessageBox(L"Failed to load the Driver!\nRestart your computer and then try opening this tool again.\n\nIf it still doesn't work, please disable the Driver Signature Enforcement feature on your computer.", MB_ICONERROR | MB_OK);
			ExitProcess(0);
		}

		goto startService;			//大家不要学习我
	}
	if (hService_DeepThawKernel)
	{
		DeleteService(hService_DeepThawKernel);
		CloseServiceHandle(hService_DeepThawKernel);

		goto createService;
	}

startService:
	if(!StartService(hService_DeepThawKernel, 0, 0))
	{
		if (!StartService(hService_DeepThawKernel, 0, 0)) {			//这里要写两遍
			AfxMessageBox(L"Failed to load the Driver!\nRestart your computer and then try opening this tool again.\n\nIf it still doesn't work, please disable the Driver Signature Enforcement feature on your computer.", MB_ICONERROR | MB_OK);
			ExitProcess(0);
		}
	}

	CloseServiceHandle(hSCManager);
}

void UnloadDriver()
{
	SERVICE_STATUS ss{ 0 };		//不想管这个了
	ControlService(hService_DeepThawKernel, SERVICE_CONTROL_STOP, &ss);

	DeleteService(hService_DeepThawKernel);
	CloseServiceHandle(hService_DeepThawKernel);
}

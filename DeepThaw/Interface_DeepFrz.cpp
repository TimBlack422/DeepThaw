#include "InterFace_DeepFrz.h"
#include <Windows.h>
#include "..\DeepThaw_Kernel\DriverControlCode.h"

#pragma comment(lib,"Version.lib")

bool Interface_DeepFrz::getDeepFrzVersion(std::wstring& output_version) 
{
	//简单地获取系统所在卷的盘符
	WCHAR windowsDirectory[MAX_PATH]{ 0 };
	if(!GetWindowsDirectory(windowsDirectory, MAX_PATH))
		return false;


	std::wstring DeepFrzDriverPath = windowsDirectory;
	
	DeepFrzDriverPath += L"\\System32\\drivers\\DeepFrz.sys";

	DWORD dwHandle = 0;			//这个是给GetFileVersionInfoSize设置成0用的
	DWORD FileVersionInfoSize =
		GetFileVersionInfoSize(DeepFrzDriverPath.c_str(), &dwHandle);
	if (!FileVersionInfoSize)
		return false;

	BYTE * VersionData = new BYTE[FileVersionInfoSize];
	if (!VersionData)
		return false;

	if(!GetFileVersionInfo(DeepFrzDriverPath.c_str(), 0, FileVersionInfoSize, VersionData))
	{
		delete[] VersionData;
		return false;
	}

	VS_FIXEDFILEINFO* FileVersion = nullptr;
	UINT puLen = 0;
	VerQueryValue(VersionData, L"\\", (LPVOID*)&FileVersion,
		&puLen);
	if(FileVersion == nullptr)
	{
		delete[] VersionData;
		return false;
	}

	output_version.clear();
	output_version += std::to_wstring(HIWORD(FileVersion->dwFileVersionMS));
	output_version += L".";
	output_version += std::to_wstring(LOWORD(FileVersion->dwFileVersionLS));

	return true;
}

bool Interface_DeepFrz::getDeepFrzStatus()
{
	SC_HANDLE hSCM =
		OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
	if (hSCM == nullptr)
		return false;

	SC_HANDLE hDeepFrz =
		OpenService(hSCM, L"DeepFrz", SERVICE_ALL_ACCESS);
	if (hDeepFrz == nullptr)
	{
		CloseHandle(hSCM);
		return false;
	}
	
	SERVICE_STATUS DeepFrz_Service_Status{ 0 };
	//这里的返回值直接忽略
	QueryServiceStatus(hDeepFrz, &DeepFrz_Service_Status);
	if (DeepFrz_Service_Status.dwCurrentState == SERVICE_STOPPED)
		return false;

	

	CloseServiceHandle(hDeepFrz);
	CloseServiceHandle(hSCM);

	return true;
}


bool Interface_DeepFrz::setDeepFrzStatus(bool Enable = 0)
{
	if (Enable) 
	{
		//将DeepFrz启用回来

		LPWSTR pathWillBeWritten_Classes[] = 
		{
		L"System\\CurrentControlSet\\Control\\Class\\{71a27cdd-812a-11d0-bec7-08002be2092f}" ,
		L"System\\CurrentControlSet\\Control\\Class\\{4D36E967-E325-11CE-BFC1-08002BE10318}",
		L"System\\CurrentControlSet\\Control\\Class\\{4D36E96B-E325-11CE-BFC1-08002BE10318}",
		L"System\\CurrentControlSet\\Control\\Class\\{4d36e96f-e325-11ce-bfc1-08002be10318}"
		};

		//其UpperFilters要被修改为的值
		LPWSTR Values[] =
		{
			L"DeepFrz\0volsnap\0FarSpace\0",
			L"DeepFrz\0partmgr\0",			//这个还要修改LowerFilters为DfDiskLo\0\0EhStorClass\0
			L"DeepFrz\0kbdclass\0",
			L"DeepFrz\0mouclass\0"
		};

		DWORD size_of_Values[] =
		{
			26 * sizeof(WCHAR),
			17 * sizeof(WCHAR),
			18 * sizeof(WCHAR),
			18 * sizeof(WCHAR)
		};

		LPWSTR pathWillBeWritten_Service[] =
		{
		L"System\\CurrentControlSet\\Services\\DeepFrz",
		L"System\\CurrentControlSet\\Services\\FarDisk",
		L"System\\CurrentControlSet\\Services\\FarSpace",
		L"System\\CurrentControlSet\\Services\\DFRegMon",
		L"System\\CurrentControlSet\\Services\\DfDiskLo",
		L"System\\CurrentControlSet\\Services\\DFServ"
		};

		for (auto path : pathWillBeWritten_Classes)
		{
			static int i = 0;

			HKEY hKey = nullptr;
			RegOpenKeyEx(HKEY_LOCAL_MACHINE, path, 0, KEY_ALL_ACCESS, &hKey);
			if (hKey == nullptr)
				continue;

			RegSetValueEx(hKey, L"UpperFilters", 0, REG_MULTI_SZ, (const BYTE*)Values[i], size_of_Values[i]);

			if (i == 1)
				RegSetValueEx(hKey, L"LowerFilters", 0, REG_MULTI_SZ, (const BYTE*)L"DfDiskLo\0EhStorClass\0", 22 * sizeof(WCHAR));

			RegCloseKey(hKey);

			i++;
		}

		for (auto path : pathWillBeWritten_Service)
		{
			static int i = 0;

			DWORD Value_Start = 0;				//驱动全部填0啦，无所谓的

			if (i == 5)
				Value_Start = 2;

			HKEY hKey = nullptr;
			RegOpenKeyEx(HKEY_LOCAL_MACHINE, path, 0, KEY_ALL_ACCESS, &hKey);
			if (hKey == nullptr)
				continue;

			RegSetValueEx(hKey, L"Start", 0, REG_DWORD, (const BYTE*)&Value_Start, sizeof(Value_Start));
			RegCloseKey(hKey);
			i++;
		}

	}
	else
	{
		HANDLE hDeepThaw_Kernel =
			CreateFile(L"\\\\.\\DeepThaw_Kernel", GENERIC_READ | GENERIC_WRITE,
				0, NULL, OPEN_EXISTING,
				0, nullptr);
		if (hDeepThaw_Kernel == INVALID_HANDLE_VALUE)
			return false;

		auto status =
			DeviceIoControl(hDeepThaw_Kernel, IOCTL_DISABLE_DEEPFRZ, 0, 0, 0, 0, 0, nullptr);
		if (!status)
			return false;

		CloseHandle(hDeepThaw_Kernel);
		return true;
	}

}
#include"Interface_RebootRestoreRx.h"
#include <TlHelp32.h>
#include <string.h>

static bool IsPasswordVerificationDisabled = false;

enum class TypeOfVersion
{
	RebootRestoreRxStandard,
	RebootRestoreRxEnterprise,
	RollBackRxPro,
	RollBackRxServer
};

typedef struct
{
	unsigned long Version_1;
	unsigned long Version_2;
	unsigned long Version_3;
	unsigned long Version_4;

	TypeOfVersion typeOfVersion;			//标示产品版本类型
} FILE_VERSION_INTERNAL;		//这个是Interface的内部结构

bool GetVersion(FILE_VERSION_INTERNAL& File_Version_Internal);

bool Interface_RebootRestoreRx::IsPasswordVerificationDisabled()
{
	return ::IsPasswordVerificationDisabled;
}

std::wstring Interface_RebootRestoreRx::GetConsoleContent()
{
	//
	//RebootRestoreRx或Rollback Rx
	//Version: 
	//Status: 
	//Probably xxx Edition
	std::wstring content;


	FILE_VERSION_INTERNAL RrrxVersion{ 0 };
	if(!GetVersion(RrrxVersion))
	{
		if (RrrxVersion.Version_1 == 0)
			content += L"Could not find Reboot Restore Rx or Rollback Rx on the computer\r\n";
		return content;
	}

	content += L"Reboot Restore Rx or Rollback Rx\r\n";

	content += L"Version: ";
	content += std::to_wstring(RrrxVersion.Version_1) += L".";
	content += std::to_wstring(RrrxVersion.Version_2) += L".";
	content += std::to_wstring(RrrxVersion.Version_3) += L".";
	content += std::to_wstring(RrrxVersion.Version_4) += L"\r\n";

	content += L"Status: ";
	if (GetStatus())
		content += L"Running \r\n";
	else
		content += L"Stopped \r\n";

	
	switch (RrrxVersion.typeOfVersion)
	{
	case TypeOfVersion::RebootRestoreRxStandard:
		content += L"Probably Reboot Restore Rx Standard";
		break;
	case TypeOfVersion::RebootRestoreRxEnterprise:
		content += L"Probably Reboot Restore Rx Enterprise";
		break;
	case TypeOfVersion::RollBackRxPro:
		content += L"Probably RollBack Rx Pro";
		break;
	case TypeOfVersion::RollBackRxServer:
		content += L"Probably RollBack Rx Server";
		break;
	}
	
	return content;
}

bool Interface_RebootRestoreRx::DisableRebootRestoreRxPasswordVerification() 
{	
	//先把ShdServ的句柄抓出来先
	HANDLE hProcessSnapshot =
		CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hProcessSnapshot == INVALID_HANDLE_VALUE)
		return false;

	PROCESSENTRY32 pe32{ sizeof(PROCESSENTRY32)};
	if(!Process32First(hProcessSnapshot, &pe32))
	{
		CloseHandle(hProcessSnapshot);
		return false;
	}
	
	DWORD ShdServProcessID = 0;
	do
	{
		if(wcscmp(pe32.szExeFile,L"ShdServ.exe")==0)
		{
			ShdServProcessID = pe32.th32ProcessID;
			break;
		}

	} while (Process32Next(hProcessSnapshot, &pe32) && GetLastError() != ERROR_NO_MORE_FILES);

	CloseHandle(hProcessSnapshot);
	if (ShdServProcessID == 0)
		return false;
	
	HANDLE hProcessShdSrv
		= OpenProcess(PROCESS_ALL_ACCESS, FALSE, ShdServProcessID);
	if (hProcessShdSrv == NULL)
		return false;

	//计算dll的位置
	std::wstring path;
	WCHAR currentDirectory[MAX_PATH]{ 0 };

	GetCurrentDirectory(MAX_PATH, currentDirectory);

	path = currentDirectory;
	path += L"\\DeepThaw_ForInjection.dll";
	
	//把dll的路径写进去
	void* addrOfDllPathInShdServ =
		VirtualAllocEx(hProcessShdSrv, 0, path.size() * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if(addrOfDllPathInShdServ == nullptr)
	{
		CloseHandle(hProcessShdSrv);
		return false;
	}
	
	if (!WriteProcessMemory(hProcessShdSrv, addrOfDllPathInShdServ, path.c_str(), path.size() * 2, nullptr))
	{
		//VirtualFreeEx(hProcessShdSrv, addrOfDllPathInShdServ, path.size() * 2, MEM_RELEASE);
		CloseHandle(hProcessShdSrv);
		return false;
	}

	//接下来把loadlibrary的地址揪出来
	//具体到为  loadlibraryw
	
	//先把相对于kernel32.dll的offest算出来
	HANDLE hModuleSnapshot =
		CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
	if (hModuleSnapshot == INVALID_HANDLE_VALUE)
	{
		//VirtualFreeEx(hProcessShdSrv, addrOfDllPathInShdServ, path.size() * 2, MEM_RELEASE);
		CloseHandle(hProcessShdSrv);
		return false;
	}

	MODULEENTRY32 me32{ sizeof(MODULEENTRY32) };
	if (!Module32First(hModuleSnapshot, &me32))
	{
		//VirtualFreeEx(hProcessShdSrv, addrOfDllPathInShdServ, path.size() * 2, MEM_RELEASE);
		CloseHandle(hProcessShdSrv);
		CloseHandle(hModuleSnapshot);
		return false;
	}

	void * relatedOffestOfLoadLibraryW = nullptr;
	do
	{
		_wcsupr_s(me32.szModule, MAX_MODULE_NAME32 + 1);
		if(wcscmp(me32.szModule,L"KERNEL32.DLL")==0)
		{
			relatedOffestOfLoadLibraryW = reinterpret_cast<void*>((ULONGLONG)LoadLibraryW - (ULONGLONG)me32.modBaseAddr);
			break;
		}

	} while (Module32Next(hModuleSnapshot, &me32) && GetLastError() != ERROR_NO_MORE_FILES);

	if(relatedOffestOfLoadLibraryW == nullptr)
	{
		//VirtualFreeEx(hProcessShdSrv, addrOfDllPathInShdServ, path.size() * 2, MEM_RELEASE);
		CloseHandle(hProcessShdSrv);
		CloseHandle(hModuleSnapshot);
		return false;
	}

	CloseHandle(hModuleSnapshot);
	hModuleSnapshot = nullptr;
	
	//算出在shdserv中loadlibraryw的偏移
	hModuleSnapshot =
		CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, ShdServProcessID);
	if(hModuleSnapshot == INVALID_HANDLE_VALUE)
	{
		//VirtualFreeEx(hProcessShdSrv, addrOfDllPathInShdServ, path.size() * 2, MEM_RELEASE);
		CloseHandle(hProcessShdSrv);
		return false;
	}

	MODULEENTRY32 me322{ sizeof(MODULEENTRY32) };
	if (!Module32First(hModuleSnapshot, &me322))
	{
		//VirtualFreeEx(hProcessShdSrv, addrOfDllPathInShdServ, path.size() * 2, MEM_RELEASE);
		CloseHandle(hProcessShdSrv);
		CloseHandle(hModuleSnapshot);
		return false;
	}

	void* relatedOffestOfKernel32dll = nullptr;
	do
	{
		_wcsupr_s(me322.szModule, MAX_MODULE_NAME32 + 1);
		if (wcscmp(me322.szModule, L"KERNEL32.DLL") == 0)
		{
			relatedOffestOfKernel32dll = (void* )me32.modBaseAddr;
			break;
		}

	} while (Module32Next(hModuleSnapshot, &me322) && GetLastError() != ERROR_NO_MORE_FILES);
	
	if(relatedOffestOfKernel32dll == nullptr)
	{
		//VirtualFreeEx(hProcessShdSrv, addrOfDllPathInShdServ, path.size() * 2, MEM_RELEASE);
		CloseHandle(hProcessShdSrv);
		CloseHandle(hModuleSnapshot);
		return false;
	}

	//把loadlibraryw的地址inshdserv给他算出来
	void* LoadLibraryWInShdServ = reinterpret_cast<void*>(
	 (ULONGLONG)relatedOffestOfKernel32dll + (ULONGLONG)relatedOffestOfLoadLibraryW);

	//接下来注入远程线程就ok了
	if (!CreateRemoteThread(hProcessShdSrv, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibraryWInShdServ,
		addrOfDllPathInShdServ, 0, 0))
	{
		//VirtualFreeEx(hProcessShdSrv, addrOfDllPathInShdServ, path.size() * 2, MEM_RELEASE);
		CloseHandle(hProcessShdSrv);
		CloseHandle(hModuleSnapshot);
		return false;
	}

	::IsPasswordVerificationDisabled = true;

	CloseHandle(hProcessShdSrv);
	CloseHandle(hModuleSnapshot);
	return true;
}

bool GetVersion(FILE_VERSION_INTERNAL & File_Version_Internal)
{
	SC_HANDLE hSCM =
		OpenSCManager(nullptr, NULL, SC_MANAGER_ALL_ACCESS);
	if (hSCM == nullptr)
		return false;

	SC_HANDLE hShdService =
		OpenService(hSCM, L"ShdServ", SERVICE_ALL_ACCESS);
	if(hShdService == nullptr)
	{
		CloseServiceHandle(hSCM);
		return false;
	}

	DWORD BytesNeeded = 0;
	if (!QueryServiceConfig(hShdService, 0, 0, &BytesNeeded) && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
	{
		CloseServiceHandle(hShdService);
		CloseServiceHandle(hSCM);
		return false;
	}
	
	QUERY_SERVICE_CONFIG* ServiceConfig =
		reinterpret_cast<QUERY_SERVICE_CONFIG*>(new BYTE[BytesNeeded]);
	if (ServiceConfig == nullptr)
	{
		CloseServiceHandle(hShdService);
		CloseServiceHandle(hSCM);
		return false;
	}

	RtlZeroMemory(ServiceConfig, BytesNeeded);
	if (!QueryServiceConfig(hShdService, ServiceConfig, BytesNeeded, &BytesNeeded))
	{
		CloseServiceHandle(hShdService);
		CloseServiceHandle(hSCM);
		return false;
	}

	//给返回的字符恰头去尾
	for(auto i =ServiceConfig->lpBinaryPathName;*i!='\0';i++)
	{
		*i = *(i + 1);
		if (*i == '\"')
			*i = '\0';
	}
	DWORD dwHandle = 0;			//这个是给GetFileVersionInfoSize设置成0用的
	DWORD FileVersionInfoSize =
		GetFileVersionInfoSize(ServiceConfig->lpBinaryPathName, &dwHandle);
	if (!FileVersionInfoSize)
	{
		delete[] ServiceConfig;
		CloseServiceHandle(hShdService);
		CloseServiceHandle(hSCM);
		return false;
	}

	BYTE* VersionData = new BYTE[FileVersionInfoSize];
	if (!VersionData)
	{
		delete[] ServiceConfig;
		CloseServiceHandle(hShdService);
		CloseServiceHandle(hSCM);
		return false;
	}

	if (!GetFileVersionInfo(ServiceConfig->lpBinaryPathName, 0, FileVersionInfoSize, VersionData))
	{
		delete[] VersionData;
		delete[] ServiceConfig;
		CloseServiceHandle(hShdService);
		CloseServiceHandle(hSCM);
		return false;
	}

	VS_FIXEDFILEINFO* FileVersion = nullptr;
	UINT puLen = 0;
	VerQueryValue(VersionData, L"\\", (LPVOID*)&FileVersion,
		&puLen);
	if (FileVersion == nullptr)
	{
		delete[] VersionData;
		return false;
	}

	File_Version_Internal.Version_1 = HIWORD(FileVersion->dwFileVersionMS);
	File_Version_Internal.Version_2 = LOWORD(FileVersion->dwFileVersionMS);
	File_Version_Internal.Version_3 = HIWORD(FileVersion->dwFileVersionLS);
	File_Version_Internal.Version_4 = LOWORD(FileVersion->dwFileVersionLS);

	delete[] VersionData;
	delete[] ServiceConfig;
	CloseServiceHandle(hShdService);
	CloseServiceHandle(hSCM);

	HKEY hKeyShield = nullptr;
	auto status =
		RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Shield",
			0,
			KEY_ALL_ACCESS,
			&hKeyShield);
	if(status!=ERROR_SUCCESS)
	{
		return true;
	}

	WCHAR DisplayName[MAX_PATH]{ 0 };
	DWORD cbData = sizeof(DisplayName);
	status= RegQueryValueEx(hKeyShield, L"DisplayName", NULL, NULL, (LPBYTE)DisplayName, &cbData);
	if(status != ERROR_SUCCESS)
	{
		RegCloseKey(hKeyShield);
		return true;
	}

	if (wcscmp(DisplayName, L"Reboot Restore Standard") == 0)		//13.0
	{
		File_Version_Internal.typeOfVersion = TypeOfVersion::RebootRestoreRxStandard;
	}
	else if(wcscmp(DisplayName,L"Reboot Restore Enterprise") == 0)	//13.0
	{
		File_Version_Internal.typeOfVersion = TypeOfVersion::RebootRestoreRxEnterprise;
	}
	else if(wcscmp(DisplayName,L"RollBack Rx Professional") == 0)	//12.9
	{
		File_Version_Internal.typeOfVersion = TypeOfVersion::RollBackRxPro;
	}
	else if(wcscmp(DisplayName,L"RollBack Rx Server") == 0)			//12.7
	{
		File_Version_Internal.typeOfVersion = TypeOfVersion::RollBackRxServer;
	}

	return true;
}

bool Interface_RebootRestoreRx::GetStatus()
{
	SC_HANDLE hSCM =
		OpenSCManager(nullptr, NULL, SC_MANAGER_ALL_ACCESS);
	if (hSCM == nullptr)
		return false;

	SC_HANDLE hShdService =
		OpenService(hSCM, L"ShdServ", SERVICE_ALL_ACCESS);
	if (hShdService == nullptr)
	{
		CloseServiceHandle(hSCM);
		return false;
	}

	SERVICE_STATUS ServiceStatus{ 0 };
	if(!QueryServiceStatus(hShdService,&ServiceStatus))
	{
		CloseServiceHandle(hShdService);
		CloseServiceHandle(hSCM);
		return false;
	}

	if (ServiceStatus.dwCurrentState == SERVICE_RUNNING)
	{
		return true;
	}
	else
	{
		return false;
	}
}

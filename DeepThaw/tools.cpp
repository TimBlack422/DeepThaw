#include "tools.h"

#include <TlHelp32.h>
#include <string>

//简单地检测进程是否存在就好了 这里是Secure System
bool IsVBSEnabled()
{
	HANDLE hProcessSnapshot =
		CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hProcessSnapshot == INVALID_HANDLE_VALUE)
		 return false;

	PROCESSENTRY32 pe32{ sizeof(PROCESSENTRY32) };
	Process32First(hProcessSnapshot, &pe32);

	do
	{
		_wcsupr_s(pe32.szExeFile, MAX_PATH);
		if (wcscmp(pe32.szExeFile, L"SECURE SYSTEM") == 0)
		{
			return true;
		}

	} while (Process32Next(hProcessSnapshot, &pe32) && GetLastError() != ERROR_NO_MORE_FILES);


	CloseHandle(hProcessSnapshot);
	return false;
}
#pragma once

#ifndef IN_ACHIEVEMENT
#include <ntddk.h>
#endif 


namespace fsd_hook
{
	NTSTATUS InitializeFSD_Hook(PDRIVER_OBJECT pDrvObj, bool Enable);		//Enable = 1初始化，否则即卸载

	void StartIoBlock();
	void StopIoBlock();
}
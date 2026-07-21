#pragma once
#include <ntddk.h>

namespace Kernel_Interface_DeepFrz
{
	
	//pubilc
	bool DisableDeepFrzOnSystemShutdown();

	//private
	bool DisableDeepFrz(IRP * Irp);		//目前那个Irp的参数保留 没有用

}
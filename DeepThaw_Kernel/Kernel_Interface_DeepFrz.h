#pragma once
#include <ntifs.h>
#include <ntddk.h>

namespace Kernel_Interface_DeepFrz
{
	
	//abandoned
	bool DisableDeepFrzOnSystemShutdown();

	//private
	bool DisableDeepFrz(IRP * Irp);		//目前那个Irp的参数保留 没有用

	//Normal模式
	//bool getDeepFrzStatusNormal();	//内核代码遵循少写的原则
	bool DisableDeepFrzStatusNormal(IRP * Irp);
}
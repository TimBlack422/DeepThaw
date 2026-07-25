#include "Kernel_Interface_DeepFrz.h"
#include "Tools.h"

extern PDEVICE_OBJECT pMainDevobj;

extern KEVENT		  event_BlockIO[2];
extern "C"
PEPROCESS			  pProceess_UnBlockIO;

///////////////////////////////////////////////////////////////////////////
// ///////////////////////////////////////////////////////////////////////
// 辅助工具们
//用来挂钩DeepFrz驱动的dispatcher	至于为什么不写一起呢，因为这样写简单

PDRIVER_DISPATCH OriginalDeepFrzDispatchers[IRP_MJ_MAXIMUM_FUNCTION]{ 0 };
PDRIVER_DISPATCH OriginalFarSpaceDispatchers[IRP_MJ_MAXIMUM_FUNCTION]{ 0 };

LowerDeviceList * DeepFrzFarSpace_LowerDeviceList = nullptr;

NTSTATUS MyHookDispatcher_DeepFrz(DEVICE_OBJECT* pDeviceObject, IRP* Irp);
NTSTATUS MyHookDispatcher_FarSpace(DEVICE_OBJECT* pDeviceObject, IRP* Irp);

/////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

// 接口实现//////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

//这个函数，替换dispatchersOfDeepFrz然后改注册表
bool Kernel_Interface_DeepFrz::DisableDeepFrz(IRP* Irp)
{
	PDRIVER_OBJECT pDeepFrzDrvObj = nullptr, pFarSpaceDrvObj = nullptr;

	auto status =
		ReferenceDriverObjectByName(L"\\Driver\\DeepFrz", &pDeepFrzDrvObj);
	if (NT_ERROR(status))
		return status;

	bool isFarSpaceExisted = false;
	status =
		ReferenceDriverObjectByName(L"\\Driver\\FarSpace", &pFarSpaceDrvObj);
	if (!NT_ERROR(status))				//todo：对没有FarSpace的老DeepFreeze的兼容
		isFarSpaceExisted = true;

	//可以直接卸载的驱动

	LPWSTR UnloadDriver_Name[] =
	{ 
	  L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\DFRegMon",
	  L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\FarDisk" 
	};
	for (int i = 0; i < 2; i++)
	{
		UNICODE_STRING driverServiceName{ 0 };

		RtlInitUnicodeString(&driverServiceName, UnloadDriver_Name[i]);
		ZwUnloadDriver(&driverServiceName);
	}
	
	
	// 驱动DeepFrz挂钩的几个设备classes DiskDrive {4D36E967-E325-11CE-BFC1-08002BE10318}
	// Volume {71a27cdd-812a-11d0-bec7-08002be2092f}
	// Keyboard {4D36E96B-E325-11CE-BFC1-08002BE10318}
	// Mouse	{4d36e96f-e325-11ce-bfc1-08002be10318}
	LPWSTR targetPDO[] =
	{ 
	L"\\Driver\\Disk",
	L"\\Driver\\volmgr"//,
	//L"\\Driver\\i8042prt"//,
//	L"\\Driver\\mouhid"
	};
	
	for (auto DriverName : targetPDO)
	{
		PDRIVER_OBJECT pDrvobj = nullptr;
		ReferenceDriverObjectByName(DriverName, &pDrvobj);
		if (!pDrvobj)
			continue;

		KdPrint(("For DeepFrz \n"));
		FindLowerDevobjByTargetDrvObjAndPDO(pDeepFrzDrvObj, pDrvobj, &DeepFrzFarSpace_LowerDeviceList);

		//FarSpace的搜索
		//DfDiskLo就不必了，我逆向完发现一般都是直接下传的
		if (isFarSpaceExisted)
		{
			KdPrint(("For FarSpace \n"));
			FindLowerDevobjByTargetDrvObjAndPDO(pFarSpaceDrvObj, pDrvobj, &DeepFrzFarSpace_LowerDeviceList);
		}

		for (auto pdo = pDrvobj->DeviceObject; pdo;
			pdo = pdo->NextDevice)
		{
			//顺便为每个PDO创建DevObj来实现Io的封锁		返回值就直接忽略了
			PDEVICE_OBJECT pAttachedDeviceObject = nullptr;
			IoCreateDevice(pMainDevobj->DriverObject, 0, nullptr, pdo->DeviceType,
				0, FALSE, &pAttachedDeviceObject);
			if (!pAttachedDeviceObject)
				KeBugCheck(SYSTEM_THREAD_EXCEPTION_NOT_HANDLED);

			pAttachedDeviceObject->DeviceExtension = IoAttachDeviceToDeviceStack(pAttachedDeviceObject, pdo);
			//这些指针直接丢，不影响的///////////////////
		}

		ObDereferenceObject(pDrvobj);
	}

	//IO封锁开始
	KeClearEvent(&event_BlockIO[0]);
	//针对进程的IO封锁开始
	pProceess_UnBlockIO = PsGetCurrentProcess();
	KeClearEvent(&event_BlockIO[1]);
	

	for (auto i = IRP_MJ_CREATE; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
	{
		OriginalDeepFrzDispatchers[i]=(PDRIVER_DISPATCH)
		InterlockedExchange64((LONG64*) & pDeepFrzDrvObj->MajorFunction[i],
			(LONG64)MyHookDispatcher_DeepFrz);
	}
	//懒得两个合并循环了
	//这个是FarSpace
	if (isFarSpaceExisted) 
	{
		for (auto i = IRP_MJ_CREATE; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
		{
			OriginalFarSpaceDispatchers[i] = (PDRIVER_DISPATCH)
				InterlockedExchange64((LONG64*)&pFarSpaceDrvObj->MajorFunction[i],
					(LONG64)MyHookDispatcher_FarSpace);
		}
	}
	//Io封锁解除。
	KeSetEvent(&event_BlockIO[0], 1, FALSE);

	//注册表修改
	LPWSTR registerPath_Classes[] = 
	{ 
		L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Class\\{71a27cdd-812a-11d0-bec7-08002be2092f}" ,
		L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Class\\{4D36E967-E325-11CE-BFC1-08002BE10318}",
		L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Class\\{4D36E96B-E325-11CE-BFC1-08002BE10318}",
		L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Class\\{4d36e96f-e325-11ce-bfc1-08002be10318}"
	};

	//其UpperFilters要被修改为的值
	LPWSTR Values[] =
	{
		L"volsnap\0",
		L"partmgr\0",			//这个还要修改LowerFilters为EhStorClass
		L"kbdclass\0",
		L"mouclass\0"
	};

	LPWSTR registerPath_Services[] = 
	{
	L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\DeepFrz",		
	L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\FarDisk",		
	L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\FarSpace",		
	L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\DFRegMon",		
	L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\DfDiskLo",		
	L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\DFServ"			
	};


	for (int i = 0; i < 4; i++)
	{
		RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE, registerPath_Classes[i],
			L"UpperFilters", REG_MULTI_SZ, Values[i], sizeof(WCHAR) * wcslen(Values[i]) + sizeof(WCHAR) * 2);

		if (i == 1)
		RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE, registerPath_Classes[i],
			L"LowerFilters", REG_MULTI_SZ, L"EhStorClass\0", sizeof(WCHAR) * wcslen(L"EhStorClass") + sizeof(WCHAR) * 2);
	}

	for (auto Path : registerPath_Services)
	{
		DWORD32 ValueDate = 3;
		RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE, Path,
			L"Start", REG_DWORD, &ValueDate, sizeof(ValueDate));
	}
	
	return true;		//	其实完全可以写void，因为是在正在关机时候执行
}

bool Kernel_Interface_DeepFrz::DisableDeepFrzOnSystemShutdown()
{
	//阻止驱动卸载
	pMainDevobj->DriverObject->DriverUnload = nullptr;

	if (NT_SUCCESS(IoRegisterShutdownNotification(pMainDevobj)))
		return true;
	else
		return false;

}
///////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

NTSTATUS MyHookDispatcher_DeepFrz(DEVICE_OBJECT* pDeviceObject, IRP* Irp)
{
	auto irpStack =
		IoGetCurrentIrpStackLocation(Irp);
	
	auto MajorFuncCode = irpStack->MajorFunction;

	PDEVICE_OBJECT LowerDevObj = nullptr;
	switch(MajorFuncCode)
	{
	case IRP_MJ_READ: //对于IRP_MJ_READ直接传给DeepFrz or FarSpace
		return OriginalDeepFrzDispatchers[irpStack->MajorFunction](pDeviceObject, Irp);
	default:
		LookupLowerDevobjByLowerDeviceList(pDeviceObject, DeepFrzFarSpace_LowerDeviceList, &LowerDevObj);
		if (LowerDevObj == nullptr)
			return OriginalDeepFrzDispatchers[irpStack->MajorFunction](pDeviceObject, Irp);
		
		if (MajorFuncCode == IRP_MJ_WRITE)
			KdPrint(("IRP_MJ_WRITE "));

		KdPrint(("Filter OK! 1 LowerDevobj: %p\n",LowerDevObj));

		if (MajorFuncCode == IRP_MJ_FLUSH_BUFFERS || MajorFuncCode == IRP_MJ_WRITE)
		{
			IoSkipCurrentIrpStackLocation(Irp);
			return IoCallDriver(LowerDevObj, Irp);
		}
		else
		{	
			Irp->IoStatus.Status = STATUS_SUCCESS;
			Irp->IoStatus.Information = 0;

			IoCompleteRequest(Irp, IO_NO_INCREMENT);
			return STATUS_SUCCESS;
		}
	}
	
}

//这两个函数几乎一样的

NTSTATUS MyHookDispatcher_FarSpace(DEVICE_OBJECT* pDeviceObject, IRP* Irp)
{
	auto irpStack =
		IoGetCurrentIrpStackLocation(Irp);

	auto MajorFuncCode = irpStack->MajorFunction;

	PDEVICE_OBJECT LowerDevObj = nullptr;
	switch (MajorFuncCode)
	{
	case IRP_MJ_READ: //对于IRP_MJ_READ直接传给DeepFrz or FarSpace
		return OriginalFarSpaceDispatchers[irpStack->MajorFunction](pDeviceObject, Irp);
	default:
		LookupLowerDevobjByLowerDeviceList(pDeviceObject, DeepFrzFarSpace_LowerDeviceList, &LowerDevObj);
		if (LowerDevObj == nullptr)
			return OriginalFarSpaceDispatchers[irpStack->MajorFunction](pDeviceObject, Irp);
		
		KdPrint(("Filter OK! 2 LowerDevobj: %p\n", LowerDevObj));
		IoSkipCurrentIrpStackLocation(Irp);
		return IoCallDriver(LowerDevObj, Irp);
	}

}
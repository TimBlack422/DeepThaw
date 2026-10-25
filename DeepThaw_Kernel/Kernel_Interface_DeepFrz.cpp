#define WINDOWS_IGNORE_PACKING_MISMATCH

#include "Kernel_Interface_DeepFrz.h"
#include "Tools.h"
#include "FSD_Hook.h"
#include <intrin.h>

extern PDEVICE_OBJECT pMainDevobj;

extern KEVENT		  event_BlockIO[2];
extern "C"
PEPROCESS			  pProceess_UnBlockIO;

KEVENT				  event_BlockIrp{ 0 };			//这个是用来拦截并筛选Irp的

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

void* pDeepFrzNonVBSArea = nullptr;
void* pDeepFrzDeviceIoDispatcherNonVBSArea = nullptr;
//这个函数，替换dispatchersOfDeepFrz到不受vbs保护的内存区域
bool Kernel_Interface_DeepFrz::DisableDeepFrzForce(IRP* Irp)
{
	PDRIVER_OBJECT pDeepFrzDrvObj = nullptr;

	auto status =
		ReferenceDriverObjectByName(L"\\Driver\\DeepFrz", &pDeepFrzDrvObj);
	if (NT_ERROR(status))
	{
		Irp->IoStatus.Status = status;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return false;
	}

	PHYSICAL_ADDRESS lowAddress{ 0 };
	PHYSICAL_ADDRESS highAddress{ 0 };
	PHYSICAL_ADDRESS skipBytes{ 0 };
	lowAddress.QuadPart = 0;
	highAddress.QuadPart = -1;
	skipBytes.QuadPart = 0;

	PMDL mdlDeepFrzNonVBSArea =
		MmAllocatePagesForMdlEx(lowAddress, highAddress, skipBytes, pDeepFrzDrvObj->DriverSize, MmNonCached, MM_ALLOCATE_FULLY_REQUIRED | MM_ALLOCATE_REQUIRE_CONTIGUOUS_CHUNKS);
	if(mdlDeepFrzNonVBSArea == nullptr)
	{
		Irp->IoStatus.Status = STATUS_MEMORY_NOT_ALLOCATED;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return false;
	}
	

	pDeepFrzNonVBSArea =
		MmGetSystemAddressForMdl(mdlDeepFrzNonVBSArea);		//反正会蓝屏的 就不错误检查了

	status = MmProtectMdlSystemAddress(mdlDeepFrzNonVBSArea, PAGE_EXECUTE_READWRITE);
	if(NT_ERROR(status))
	{
		Irp->IoStatus.Status = status;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return false;
	}

	auto pDeepFrzDriSize = pDeepFrzDrvObj->DriverSize;	//这个东西要自己探测
	for (auto i = (ULONGLONG)pDeepFrzDrvObj->DriverStart+ pDeepFrzDrvObj->DriverSize; ;i-=PAGE_SIZE)
	{
		if (MmIsAddressValid((PVOID)i))
		{
			pDeepFrzDriSize = i  - (ULONGLONG)pDeepFrzDrvObj->DriverStart;
			break;
		}
	}

	pDeepFrzDriSize += PAGE_SIZE - 1;
	RtlCopyMemory(pDeepFrzNonVBSArea, pDeepFrzDrvObj->DriverStart, pDeepFrzDriSize);
	
	//todo:把所有的表重定位

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


		for (auto pdo = pDrvobj->DeviceObject; pdo;
			pdo = pdo->NextDevice)
		{
			////顺便为每个PDO创建DevObj来实现Io的封锁		返回值就直接忽略了
			//PDEVICE_OBJECT pAttachedDeviceObject = nullptr;
			//IoCreateDevice(pMainDevobj->DriverObject, 0, nullptr, pdo->DeviceType,
			//	0, FALSE, &pAttachedDeviceObject);
			//if (!pAttachedDeviceObject)
			//	KeBugCheck(SYSTEM_THREAD_EXCEPTION_NOT_HANDLED);

			//pAttachedDeviceObject->DeviceExtension = IoAttachDeviceToDeviceStack(pAttachedDeviceObject, pdo);
			////这些指针直接丢，不影响的///////////////////
		}

		ObDereferenceObject(pDrvobj);
	}

	//IO封锁开始
	//KeClearEvent(&event_BlockIO[0]);
	////针对进程的IO封锁开始
	pProceess_UnBlockIO = PsGetCurrentProcess();
	KeClearEvent(&event_BlockIO[1]);
	fsd_hook::StartIoBlock();				//针对文件的io封锁

	KIRQL oldIrql{ 0 };
	KeRaiseIrql(APC_LEVEL, &oldIrql);

	//hook deepfrz的dispatcher  为不受vbs保护的区域  这里只挂钩DeviceIoControl
	//for (auto i = IRP_MJ_CREATE; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
	//{
	//	if (pDeepFrzDrvObj->MajorFunction[i]<pDeepFrzDrvObj->DriverStart || 
	//		(ULONG64)pDeepFrzDrvObj->MajorFunction[i]>((ULONG64)pDeepFrzDrvObj->DriverStart + (ULONG64)pDeepFrzDrvObj->DriverSize))
	//	{
	//		continue;
	//	}							//猛然想到某些地址是完全不会被调用的
	pDeepFrzDeviceIoDispatcherNonVBSArea = (void*)((ULONGLONG)pDeepFrzNonVBSArea + (ULONGLONG)pDeepFrzDrvObj->MajorFunction[IRP_MJ_DEVICE_CONTROL] - (ULONGLONG)pDeepFrzDrvObj->DriverStart);

		OriginalDeepFrzDispatchers[IRP_MJ_DEVICE_CONTROL]=(PDRIVER_DISPATCH)
		InterlockedExchange64((LONG64*) & pDeepFrzDrvObj->MajorFunction[IRP_MJ_DEVICE_CONTROL],
			(ULONGLONG)MyHookDispatcher_DeepFrz);	//还是得路由一下
	//}
	
	pDeepFrzDrvObj->DriverStart = pDeepFrzNonVBSArea;

	KeLowerIrql(oldIrql);

	//Io进程封锁解除
	fsd_hook::StopIoBlock();
	KeSetEvent(&event_BlockIO[0], 0, FALSE);
	KeSetEvent(&event_BlockIO[1], 0, FALSE);

	KeInitializeEvent(&event_BlockIrp, NotificationEvent, TRUE);
	ObDereferenceObject(pDeepFrzDrvObj);
	
	
	return Kernel_Interface_DeepFrz::DisableDeepFrzStatusNormal(Irp);		//交给你了
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

//这个是用来筛选Irp的
NTSTATUS IoCompletionRoutineForIrp(PDEVICE_OBJECT DevObj, PIRP Irp, PVOID Context);

//这个函数会筛选Irp DeviceIoControl包的类型 来防止蓝屏
NTSTATUS MyHookDispatcher_DeepFrz(DEVICE_OBJECT* pDeviceObject, IRP* Irp)
{
	auto irpStack =
		IoGetCurrentIrpStackLocation(Irp);
	
	auto MajorFuncCode = irpStack->MajorFunction;
	NTSTATUS status = 0;

	PDEVICE_OBJECT LowerDevObj = nullptr;
	switch(MajorFuncCode)
	{
	case IRP_MJ_READ: //对于IRP_MJ_READ直接传给DeepFrz or FarSpace
		return OriginalDeepFrzDispatchers[irpStack->MajorFunction](pDeviceObject, Irp);
	case IRP_MJ_DEVICE_CONTROL:			//现在的状况来看只有这里能执行
		//注册Io完成例程
		//IoSetCompletionRoutine(Irp, IoCompletionRoutineForIrp, 0, FALSE, TRUE, FALSE);
		if(irpStack->Parameters.DeviceIoControl.IoControlCode == 0x72070 || 
		   irpStack->Parameters.DeviceIoControl.IoControlCode == 0x72094)
			return ((PDRIVER_DISPATCH)pDeepFrzDeviceIoDispatcherNonVBSArea)(pDeviceObject, Irp);
		else
			return OriginalDeepFrzDispatchers[irpStack->MajorFunction](pDeviceObject, Irp);

		//KeResetEvent(&event_BlockIrp);
		//KeWaitForSingleObject(&event_BlockIrp, Executive, KernelMode, FALSE, NULL);

	default:
		LookupLowerDevobjByLowerDeviceList(pDeviceObject, DeepFrzFarSpace_LowerDeviceList, &LowerDevObj);
		if (LowerDevObj == nullptr)
			return OriginalDeepFrzDispatchers[irpStack->MajorFunction](pDeviceObject, Irp);
		
		if (MajorFuncCode == IRP_MJ_WRITE)
			KdPrint(("IRP_MJ_WRITE "));

		KdPrint(("Filter OK! 1 LowerDevobj: %p\n",LowerDevObj));

		if ((MajorFuncCode == IRP_MJ_FLUSH_BUFFERS || MajorFuncCode == IRP_MJ_WRITE) && (ExGetPreviousMode() == KernelMode))
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

//NTSTATUS IoCompletionRoutineForIrp(PDEVICE_OBJECT DevObj, PIRP Irp, PVOID Context) 
//{
//	if (Irp->IoStatus.Status == STATUS_ACCESS_DENIED)
//	{
//		return STATUS_MORE_PROCESSING_REQUIRED;
//	}
//	return STATUS_CONTINUE_COMPLETION;
//}

//用来挂钩的函数
static void Hook_ExSystemTimeToLocalTime(PLARGE_INTEGER, PLARGE_INTEGER);

bool Kernel_Interface_DeepFrz::DisableDeepFrzStatusNormal(IRP* Irp)
{
	auto IrpStack = IoGetCurrentIrpStackLocation(Irp);
	
	DWORD32 RVA_IAT = *reinterpret_cast<DWORD32*>
	(Irp->AssociatedIrp.SystemBuffer);

	UNICODE_STRING Name_ExSystemTimeToLocalTime{ 0 };
	RtlInitUnicodeString(&Name_ExSystemTimeToLocalTime, L"ExSystemTimeToLocalTime");
	auto pExSystemTimeToLocalTime = MmGetSystemRoutineAddress(&Name_ExSystemTimeToLocalTime);
	if(pExSystemTimeToLocalTime == nullptr)
	{
		Irp->IoStatus.Status = STATUS_INTERNAL_ERROR;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return false;
	}

	PDRIVER_OBJECT pDeepFrzObj = nullptr;
	ReferenceDriverObjectByName(L"\\Driver\\DeepFrz", &pDeepFrzObj);
	if(pDeepFrzObj == nullptr)
	{
		Irp->IoStatus.Status = STATUS_NOT_FOUND;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return false;
	}

	auto DriverStart = pDeepFrzObj->DriverStart;
	//先把iat对应的槽的地址算出来
	PIMAGE_THUNK_DATA pThunk = PIMAGE_THUNK_DATA
		(((ULONGLONG)DriverStart) + RVA_IAT);
	for (;pThunk->u1.Function != (ULONGLONG)nullptr; pThunk++)			//200%成功的我就不做错误处理了,iat也没有个数标记限制...
	{
		if (pThunk->u1.Function == (ULONGLONG)pExSystemTimeToLocalTime)
			break;
	}

	//pThunk->u1.Function = (ULONGLONG)nullptr;
	PIMAGE_THUNK_DATA addrCanBeWritten = (PIMAGE_THUNK_DATA)
		MmMapIoSpace(MmGetPhysicalAddress(pThunk), sizeof(pThunk->u1.Function), MmNonCached);
	if(addrCanBeWritten == nullptr)
	{
		Irp->IoStatus.Status = STATUS_INTERNAL_ERROR;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return false;
	}
	addrCanBeWritten->u1.Function = (ULONGLONG)Hook_ExSystemTimeToLocalTime;		//todo:这个要变成我们获取函数的函数

	ObDereferenceObject(pDeepFrzObj);
	MmUnmapIoSpace(addrCanBeWritten, sizeof(pThunk->u1.Function));

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return true;

}

static void Hook_ExSystemTimeToLocalTime(PLARGE_INTEGER, PLARGE_INTEGER) 
{
	auto AddressOfReturnAddress =
		_AddressOfReturnAddress();

	//执行次数		绝大多数deepfrz 一般次数为1
	static int Times = 0;
	Times++;

	USHORT FrameNum = 0;

	PVOID* Frames[5]{};
	FrameNum = RtlCaptureStackBackTrace(1, 5, (PVOID*)Frames, 0);

	//panduan次数后开始修改指令
	if (Times == 2 || ( (Times > 2) ? (Times% 2 ==0 || Times %3 ==0) : 0))
	{ 
		BYTE* originalAddr = (BYTE*)Frames[3] -5;

		BYTE* AddrCanBeWritten = (BYTE*)
			MmMapIoSpace(MmGetPhysicalAddress(originalAddr), 5, MmNonCached);
		BYTE Bin[] = {0xB0,0x01,0x90,0x90,0x90 };	//mov al,1
		for (int i = 0; i < 5; i++)
			AddrCanBeWritten[i] = Bin[i];			//可能会蓝屏，如果mmmapiospace返回0的话。不过那不是我的问题了..
		
		MmUnmapIoSpace(AddrCanBeWritten, 5);

	}

	return;
};
#define WINDOWS_IGNORE_PACKING_MISMATCH

#include <intrin.h>

#include "Kernel_Interface_DeepFrz.h"
#include "Tools.h"
#include "FSD_Hook.h"

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

bool ForceModeEnabled = nullptr;

//这个函数，替换dispatchersOfDeepFrz到不受vbs保护的内存区域
bool Kernel_Interface_DeepFrz::DisableDeepFrzForce(IRP* Irp)
{
	ForceModeEnabled = true;
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
		if (irpStack->Parameters.DeviceIoControl.IoControlCode == 0x72070 ||
			irpStack->Parameters.DeviceIoControl.IoControlCode == 0x72094)
			;//return ((PDRIVER_DISPATCH)pDeepFrzDeviceIoDispatcherNonVBSArea)(pDeviceObject, Irp);
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
	addrCanBeWritten->u1.Function = (ULONGLONG)Hook_ExSystemTimeToLocalTime;		//这个变成我们获取函数的函数

	ObDereferenceObject(pDeepFrzObj);
	MmUnmapIoSpace(addrCanBeWritten, sizeof(pThunk->u1.Function));

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return true;

}

extern "C"
NTSYSAPI VOID __cdecl RtlRestoreContext(PCONTEXT ContextRecord, _EXCEPTION_RECORD* ExceptionRecode);		//这个函数默认没有声明，但是被导出了。只能这样用了

void ModifyRaxTo1();						//把rax改到1，仅此而已
void* originalReturnAddress = nullptr;

CONTEXT context{ 0 };
static void Hook_ExSystemTimeToLocalTime(PLARGE_INTEGER, PLARGE_INTEGER localTime) 
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

		if (ForceModeEnabled == false) {
			BYTE* AddrCanBeWritten = (BYTE*)
				MmMapIoSpace(MmGetPhysicalAddress(originalAddr), 5, MmNonCached);
			BYTE Bin[] = { 0xB0,0x01,0x90,0x90,0x90 };	//mov al,1
			for (int i = 0; i < 5; i++)
				AddrCanBeWritten[i] = Bin[i];			//可能会蓝屏，如果mmmapiospace返回0的话。不过那不是我的问题了..

			MmUnmapIoSpace(AddrCanBeWritten, 5);
		}
		else
		{
			//直接栈回溯
			//RtlCaptureContext(&context);

			//BYTE* rsp = (BYTE*)context.Rsp;
			//for (; *(ULONGLONG*)rsp != (ULONGLONG)(Frames[3]); rsp++)
			//	;

			//originalReturnAddress =(void*) *rsp;
			DbgBreakPoint();
			//*rsp = (ULONGLONG)ModifyRaxTo1;
			localTime->QuadPart = 0x19DB1DED53E8000 + 0x989680LL * 0x7FFFFFFF;
			
		}

	}

	return;
};

void ModifyRaxTo1()
{
	RtlZeroMemory(&context, sizeof(CONTEXT));
	RtlCaptureContext(&context);

	context.Rax = 1;
	context.Rip = (ULONGLONG)originalReturnAddress;

	RtlRestoreContext(&context, NULL);


}
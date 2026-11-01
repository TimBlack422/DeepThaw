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

bool ForceModeEnabled = false;
void* BaseAddr_DeepFrzDriver = nullptr;
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

//这个函数，打开强制模式开关
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

//这个函数会注册一个SEH用来处理异常
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
			;// return ((PDRIVER_DISPATCH)pDeepFrzDeviceIoDispatcherNonVBSArea)(pDeviceObject, Irp);
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

	BaseAddr_DeepFrzDriver = pDeepFrzObj->DriverStart;

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


CONTEXT context{ 0 };
UNWIND_HISTORY_TABLE HistoryTable{ 0 };
PRUNTIME_FUNCTION pRuntime_Function = nullptr;
static void Hook_ExSystemTimeToLocalTime(PLARGE_INTEGER, PLARGE_INTEGER) 
{
	//执行次数		绝大多数deepfrz 一般次数为1
	static int Times = 0;
	Times++;

	USHORT FrameNum = 0;

	RtlZeroMemory(&context, sizeof(context));

	PVOID* Frames[5]{};
	FrameNum = RtlCaptureStackBackTrace(1, 5, (PVOID*)Frames, 0);

	//panduan次数后开始修改指令
	if (Times == 2 || ( (Times > 2) ? (Times% 2 ==0 || Times %3 ==0) : 0))
	{ 
		if (ForceModeEnabled == false) {
			BYTE* originalAddr = (BYTE*)Frames[3] - 5;

			BYTE* AddrCanBeWritten = (BYTE*)
				MmMapIoSpace(MmGetPhysicalAddress(originalAddr), 5, MmNonCached);
			BYTE Bin[] = { 0xB0,0x01,0x90,0x90,0x90 };	//mov al,1
			for (int i = 0; i < 5; i++)
				AddrCanBeWritten[i] = Bin[i];			//可能会蓝屏，如果mmmapiospace返回0的话。不过那不是我的问题了..

			MmUnmapIoSpace(AddrCanBeWritten, 5);
		}
		else											//为强制模式
		{
			DWORD64   EstablisherFrame=0;
			DWORD64  ImageBase = 0;
			
			//栈回溯	回溯5下
			RtlCaptureContext(&context);

			for (int count = 0; count < 4; count++)
			{
				RtlZeroMemory(&HistoryTable, sizeof(HistoryTable));
				pRuntime_Function = nullptr;
				pRuntime_Function =
					RtlLookupFunctionEntry(context.Rip, &ImageBase, &HistoryTable);

				RtlVirtualUnwind(0x0,				//UNW_FLAG_NHANDLE
					(DWORD64)HistoryTable.Entry[0].ImageBase,
					context.Rip,
					pRuntime_Function,
					&context,
					0,
					&EstablisherFrame,
					nullptr);
			}

			context.Rax = 1;
			RtlRestoreContext(&context,NULL);				//这里就没有返回一说了

		}

	}

	return;
};

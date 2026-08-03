#define IN_ACHIEVEMENT

#include <Fltkernel.h>
#include "FSD_Hook.h"
#include "Tools.h"

FLT_PREOP_CALLBACK_STATUS On_IrpMJWrite_IrpMJFlushBuffers_PreOperationCallback(PFLT_CALLBACK_DATA, PCFLT_RELATED_OBJECTS, PVOID*);
NTSTATUS FilterUnloadCallback(FLT_FILTER_UNLOAD_FLAGS Flags);

const FLT_OPERATION_REGISTRATION Flt_Opra_Rrgstrtn[] =
{ {IRP_MJ_WRITE,0,On_IrpMJWrite_IrpMJFlushBuffers_PreOperationCallback},
  {IRP_MJ_FLUSH_BUFFERS,0,On_IrpMJWrite_IrpMJFlushBuffers_PreOperationCallback},
  {IRP_MJ_READ,0,On_IrpMJWrite_IrpMJFlushBuffers_PreOperationCallback},		//兼顾哈兼顾
  {IRP_MJ_OPERATION_END} };

const FLT_REGISTRATION Flt_Registration = 
{
	sizeof(FLT_REGISTRATION),
	FLT_REGISTRATION_VERSION,
	NULL,
	nullptr,
	Flt_Opra_Rrgstrtn,				//OperationRegistration
	FilterUnloadCallback			//unloadCallback
};

PFLT_FILTER pFlt_Instance = nullptr;

NTSTATUS fsd_hook::InitializeFSD_Hook(PDRIVER_OBJECT pDrvObj, bool Enable)
{
	if ((!Enable) && (!pFlt_Instance))
	{
		fsd_hook::StopIoBlock();
		return STATUS_SUCCESS;
	}

	auto status=
	FltRegisterFilter(pDrvObj, &Flt_Registration, &pFlt_Instance);
	if (NT_ERROR(status)) {
		if(pFlt_Instance)
		FltUnregisterFilter(pFlt_Instance);
		return status;
	}

	return status;
}


bool IoBlock_Mark = false;

void fsd_hook::StartIoBlock()
{
	if (!IoBlock_Mark)
		IoBlock_Mark = true;
	if (pFlt_Instance)
		FltStartFiltering(pFlt_Instance);
}


void fsd_hook::StopIoBlock()
{
	if (IoBlock_Mark)
		IoBlock_Mark = false;
}

LPWSTR gBlockList[] =							//封锁文件的目录
{
	L"\\Windows\\System32\\config\\SYSTEM",
	L"\\pagefile.sys"							//这个暂时不用,不然会打架
};

extern "C" PEPROCESS	 pProceess_UnBlockIO;	//			
FLT_PREOP_CALLBACK_STATUS On_IrpMJWrite_IrpMJFlushBuffers_PreOperationCallback(PFLT_CALLBACK_DATA CallbackData, PCFLT_RELATED_OBJECTS, PVOID*)
{
	if (IoBlock_Mark == false)
		return FLT_PREOP_SUCCESS_NO_CALLBACK;

	if (CallbackData->Iopb->IrpFlags & IRP_PAGING_IO)		//只筛选分页操作，一般注册表都是分页的
	{
		if (CallbackData->RequestorMode == KernelMode)
			return FLT_PREOP_SUCCESS_NO_CALLBACK;
		else
		{
			CallbackData->IoStatus.Status = STATUS_SUCCESS;
			CallbackData->IoStatus.Information = 0;
			return FLT_PREOP_COMPLETE;
		}
	}

	PFLT_FILE_NAME_INFORMATION name_Info = nullptr;
	auto status =
		FltGetFileNameInformation(CallbackData, FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_ALWAYS_ALLOW_CACHE_LOOKUP, &name_Info);
	if (NT_SUCCESS(status))
	{
		if (wcsstr(name_Info->Name.Buffer, gBlockList[0]))
		{
			FltReleaseFileNameInformation(name_Info);
			return FLT_PREOP_SUCCESS_NO_CALLBACK;
		}
	}

	FltReleaseFileNameInformation(name_Info);

	CallbackData->IoStatus.Status = STATUS_SUCCESS;
	CallbackData->IoStatus.Information = 0;
	return FLT_PREOP_COMPLETE;

}

NTSTATUS FilterUnloadCallback(FLT_FILTER_UNLOAD_FLAGS Flags)
{
	FltUnregisterFilter(pFlt_Instance);
	pFlt_Instance = nullptr;

	return STATUS_SUCCESS;
}
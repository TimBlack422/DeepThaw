#include "Tools.h"

NTSTATUS ReferenceDriverObjectByName(LPCWSTR ObjectPath, PDRIVER_OBJECT* pDrvObj)
{
	UNICODE_STRING name{ 0 };
	RtlInitUnicodeString(&name, ObjectPath);
	
	auto status =
		ObReferenceObjectByName(&name, OBJ_CASE_INSENSITIVE, nullptr, 0, *IoDriverObjectType,
			KernelMode, nullptr, (PVOID*)pDrvObj);

	if (NT_ERROR(status))
		return status;

	return STATUS_SUCCESS;
}

NTSTATUS FindLowerDevobjByTargetDrvObjAndPDO(PDRIVER_OBJECT TargetDrvobj, PDRIVER_OBJECT driver_pdo, LowerDeviceList** pList_LowerDevice)
{
	LowerDeviceList* list = nullptr;
	if (!*pList_LowerDevice) {
		list = reinterpret_cast<LowerDeviceList*>(ExAllocatePool(NonPagedPool, sizeof(LowerDeviceList)));
		if (!list)
			return STATUS_INTERNAL_ERROR;
		RtlZeroMemory(list, sizeof(LowerDeviceList));
		*pList_LowerDevice = list;
	}
	else
	{
		list = *pList_LowerDevice;
		for (auto i = list; list; list = list->NextItem)
			if (!list->NextItem)
				break;
	}

	KdPrint(("FindLowerDevobjByTargetDrvObjAndPDO: Start List: %p \n", *pList_LowerDevice));

	for (PDEVICE_OBJECT pdo = driver_pdo->DeviceObject; pdo; pdo = pdo->NextDevice)
	{
		for (PDEVICE_OBJECT currectDevobj = pdo, lowerDevobj = nullptr
			; currectDevobj;
			lowerDevobj = currectDevobj,
			currectDevobj = currectDevobj->AttachedDevice)
		{
			if (currectDevobj->DriverObject == TargetDrvobj)
			{
				KdPrint(("FindLowerDevobjByTargetDrvObjAndPDO: lowerDevobj: %p currect: %p \n", lowerDevobj, currectDevobj));
				list->LowerDevObj  = lowerDevobj;
				list->TargetDevObj = currectDevobj;

				list->NextItem = nullptr;
				if (currectDevobj->AttachedDevice) {
					list->NextItem = reinterpret_cast<LowerDeviceList*>(ExAllocatePool(NonPagedPool, sizeof(LowerDeviceList)));
					list = list->NextItem;
					if (!list)
						return STATUS_INTERNAL_ERROR;

					RtlZeroMemory(list, sizeof(LowerDeviceList));
				}
				//return STATUS_SUCCESS;
			}
		}
	}
	return STATUS_NOT_FOUND;
}

NTSTATUS LookupLowerDevobjByLowerDeviceList(PDEVICE_OBJECT TargetDevObj, LowerDeviceList* DeviceList, PDEVICE_OBJECT* output_LowerDevObj)
{
	for (auto i = DeviceList; i; i = i->NextItem)
	{
		//KdPrint(("LookupLowerDevobjByLowerDeviceList List: %p item:%p TargetDevobj:%p i->TargetDevobj:%p LowerDevobj:%p \n", DeviceList,i,TargetDevObj, i->TargetDevObj, i->LowerDevObj));
		if (i->TargetDevObj == TargetDevObj)
		{
			//KdPrint(("Found! LookupLowerDevobjByLowerDeviceList TargetDevobj:%p i->TargetDevobj:%p LowerDevobj:%p \n",TargetDevObj,i->TargetDevObj,i->LowerDevObj));
			*output_LowerDevObj = i->LowerDevObj;
			return STATUS_SUCCESS;
		}
	}

	*output_LowerDevObj = nullptr;
	return STATUS_NOT_FOUND;
}

static VOID __stdcall Thread_NtShutdownSystem (PVOID StartContext)
{
	if(ExGetPreviousMode() == UserMode)
		NtShutdownSystem(ShutdownReboot);

	PsTerminateSystemThread(0);
}

void __stdcall  NtMyRebootSystem()
{
	HANDLE hThreadHandle{ 0 };
	OBJECT_ATTRIBUTES ObjectAttributes = { 0 };
	
	KIRQL OldIrql = -1;
	if (KeGetCurrentIrql() != PASSIVE_LEVEL)		//降IQRL
		KeRaiseIrql(PASSIVE_LEVEL,&OldIrql);

	InitializeObjectAttributes(&ObjectAttributes, nullptr, OBJ_KERNEL_HANDLE, nullptr, nullptr);
	PsCreateSystemThread(&hThreadHandle, THREAD_ALL_ACCESS, &ObjectAttributes, nullptr, nullptr
		, Thread_NtShutdownSystem, nullptr);

	if (OldIrql != -1)
		KeLowerIrql(OldIrql); 

	KeWaitForSingleObject(hThreadHandle, Executive, KernelMode, false, nullptr);
}
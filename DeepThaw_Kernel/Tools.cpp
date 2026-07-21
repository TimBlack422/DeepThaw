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
		list = (LowerDeviceList*)ExAllocatePool(NonPagedPool, sizeof(LowerDeviceList));
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
					list->NextItem = (LowerDeviceList*)ExAllocatePool(NonPagedPool, sizeof(LowerDeviceList));
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
		KdPrint(("LookupLowerDevobjByLowerDeviceList List: %p item:%p TargetDevobj:%p i->TargetDevobj:%p LowerDevobj:%p \n", DeviceList,i,TargetDevObj, i->TargetDevObj, i->LowerDevObj));
		if (i->TargetDevObj == TargetDevObj)
		{
			KdPrint(("Found! LookupLowerDevobjByLowerDeviceList TargetDevobj:%p i->TargetDevobj:%p LowerDevobj:%p \n",TargetDevObj,i->TargetDevObj,i->LowerDevObj));
			*output_LowerDevObj = i->LowerDevObj;
			return STATUS_SUCCESS;
		}
	}

	*output_LowerDevObj = nullptr;
	return STATUS_NOT_FOUND;
}
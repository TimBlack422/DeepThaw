#pragma once
#include <ntddk.h>

extern "C"
NTSTATUS
ObReferenceObjectByName(
	IN PUNICODE_STRING ObjectName,
	IN ULONG Attributes,
	IN PACCESS_STATE PassedAccessState OPTIONAL,
	IN ACCESS_MASK DesiredAccess OPTIONAL,
	IN POBJECT_TYPE ObjectType,
	IN KPROCESSOR_MODE AccessMode,
	IN OUT PVOID ParseContext OPTIONAL,
	OUT PVOID* Object
);

extern "C" POBJECT_TYPE* IoDriverObjectType;

extern "C" POBJECT_TYPE ObGetObjectType(IN PVOID Object);


NTSTATUS ReferenceDriverObjectByName(LPCWSTR ObjectPath, PDRIVER_OBJECT* pDrvObj);

struct LowerDeviceList
{
	PDEVICE_OBJECT TargetDevObj;
	PDEVICE_OBJECT LowerDevObj;

	LowerDeviceList * NextItem;
};

NTSTATUS FindLowerDevobjByTargetDrvObjAndPDO(PDRIVER_OBJECT TargetDrvobj, PDRIVER_OBJECT drvobj_pdo, LowerDeviceList** item);

NTSTATUS LookupLowerDevobjByLowerDeviceList (PDEVICE_OBJECT TargetDevObj, LowerDeviceList* item,PDEVICE_OBJECT* output_LowerDevObj);

NTSTATUS GetSystemRootAsDosName();
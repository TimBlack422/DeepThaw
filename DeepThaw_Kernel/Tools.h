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

//NtShutdownSystem 函数 定义
typedef enum _SHUTDOWN_ACTION {
	ShutdownNoReboot,  // 关机
	ShutdownReboot,    // 重启
	ShutdownPowerOff   // 断电
} SHUTDOWN_ACTION, * PSHUTDOWN_ACTION;

extern "C"
NTSTATUS __stdcall NtShutdownSystem(SHUTDOWN_ACTION Action);

void	(__stdcall  NtMyRebootSystem)();

//from winnt.h
#ifdef _WIN64
typedef struct _IMAGE_THUNK_DATA64 {
	union {
		ULONGLONG ForwarderString;  // PBYTE 
		ULONGLONG Function;         // PDWORD
		ULONGLONG Ordinal;
		ULONGLONG AddressOfData;    // PIMAGE_IMPORT_BY_NAME
	} u1;
} IMAGE_THUNK_DATA64;
typedef IMAGE_THUNK_DATA64* PIMAGE_THUNK_DATA64;

typedef IMAGE_THUNK_DATA64              IMAGE_THUNK_DATA;
typedef PIMAGE_THUNK_DATA64             PIMAGE_THUNK_DATA;

#else
//@[comment("MVI_tracked")]
typedef struct _IMAGE_THUNK_DATA32 {
	union {
		DWORD32 ForwarderString;      // PBYTE 
		DWORD32 Function;             // PDWORD
		DWORD32 Ordinal;
		DWORD32 AddressOfData;        // PIMAGE_IMPORT_BY_NAME
	} u1;
} IMAGE_THUNK_DATA32;
typedef IMAGE_THUNK_DATA32* PIMAGE_THUNK_DATA32;

typedef IMAGE_THUNK_DATA32              IMAGE_THUNK_DATA;
typedef PIMAGE_THUNK_DATA32             PIMAGE_THUNK_DATA;
#endif

typedef CHAR BYTE, * PBYTE;
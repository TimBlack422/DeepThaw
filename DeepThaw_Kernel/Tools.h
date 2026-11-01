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

//这些wdk自带的没有定义，但是内核有导出
typedef struct _IMAGE_RUNTIME_FUNCTION_ENTRY {
	DWORD32 BeginAddress;
	DWORD32 EndAddress;
	union {
		DWORD32 UnwindInfoAddress;
		DWORD32 UnwindData;
	} DUMMYUNIONNAME;
} RUNTIME_FUNCTION, * PRUNTIME_FUNCTION, _IMAGE_RUNTIME_FUNCTION_ENTRY, * _PIMAGE_RUNTIME_FUNCTION_ENTRY;

typedef struct _KNONVOLATILE_CONTEXT_POINTERS {
    ULONG64* Rax;
    ULONG64* Rbx;
    ULONG64* Rcx;
    ULONG64* Rdx;
    ULONG64* Rsi;
    ULONG64* Rdi;
    ULONG64* Rbp;
    ULONG64* Rsp;
    ULONG64* R8;
    ULONG64* R9;
    ULONG64* R10;
    ULONG64* R11;
    ULONG64* R12;
    ULONG64* R13;
    ULONG64* R14;
    ULONG64* R15;
    M128A* Xmm0;
    M128A* Xmm1;
    M128A* Xmm2;
    M128A* Xmm3;
    M128A* Xmm4;
    M128A* Xmm5;
    M128A* Xmm6;
    M128A* Xmm7;
    M128A* Xmm8;
    M128A* Xmm9;
    M128A* Xmm10;
    M128A* Xmm11;
    M128A* Xmm12;
    M128A* Xmm13;
    M128A* Xmm14;
    M128A* Xmm15;
} KNONVOLATILE_CONTEXT_POINTERS, * PKNONVOLATILE_CONTEXT_POINTERS;

#define UNWIND_HISTORY_TABLE_SIZE 12

typedef struct _UNWIND_HISTORY_TABLE_ENTRY {
    ULONG_PTR ImageBase;
    PRUNTIME_FUNCTION FunctionEntry;
} UNWIND_HISTORY_TABLE_ENTRY, * PUNWIND_HISTORY_TABLE_ENTRY;

typedef struct _UNWIND_HISTORY_TABLE {
    DWORD32 Count;
    BYTE  LocalHint;
    BYTE  GlobalHint;
    BYTE  Search;
    BYTE  Once;
    ULONG_PTR LowAddress;
    ULONG_PTR HighAddress;
    UNWIND_HISTORY_TABLE_ENTRY Entry[UNWIND_HISTORY_TABLE_SIZE];
} UNWIND_HISTORY_TABLE, * PUNWIND_HISTORY_TABLE;

extern "C"
NTSYSAPI VOID __cdecl RtlRestoreContext(PCONTEXT ContextRecord, _EXCEPTION_RECORD* ExceptionRecord);

extern "C"
NTSYSAPI PEXCEPTION_ROUTINE RtlVirtualUnwind(
    _In_ DWORD32 HandlerType,
    _In_ ULONG_PTR ImageBase,
    _In_ ULONG_PTR ControlPc,
    _In_ PRUNTIME_FUNCTION FunctionEntry,
    _Inout_ PCONTEXT ContextRecord,
    _Out_ PVOID* HandlerData,
    _Out_ PULONG_PTR EstablisherFrame,
    _Inout_opt_ PKNONVOLATILE_CONTEXT_POINTERS ContextPointers);

extern "C"
NTSYSAPI VOID __cdecl RtlRestoreContext(PCONTEXT,_EXCEPTION_RECORD*);

extern "C"
NTSYSAPI PRUNTIME_FUNCTION RtlLookupFunctionEntry(
     DWORD64               ControlPc,
     PDWORD64              ImageBase,
     PUNWIND_HISTORY_TABLE HistoryTable
);

/**
 * A pointer to a user-defined function that serves as the starting routine for a new thread.
 *
 * \param ThreadParameter A pointer to a variable that is passed to the thread.
 * \return NTSTATUS Successful or errant status.
 */
typedef _Function_class_(USER_THREAD_START_ROUTINE)
NTSTATUS NTAPI USER_THREAD_START_ROUTINE(
    _In_ PVOID ThreadParameter
);

typedef USER_THREAD_START_ROUTINE* PUSER_THREAD_START_ROUTINE;

/**
 * The RtlCreateUserThread routine creates a thread in the specified process.
 *
 * \param ProcessHandle Handle to the process in which the thread is to be created.
 * \param ThreadSecurityDescriptor Optional pointer to a security descriptor for the new thread. If NULL, the
 * thread gets a default security descriptor.
 * \param CreateSuspended If TRUE, the thread is created in a suspended state and must be resumed explicitly.
 * If FALSE, the thread starts running immediately.
 * \param ZeroBits Optional number of high-order address bits that must be zero in the stack's base address.
 * Usually set to 0.
 * \param MaximumStackSize Optional maximum size, in bytes, of the stack for the new thread. If 0, the default
 * size is used.
 * \param CommittedStackSize Optional initial size, in bytes, of committed stack for the new thread. If 0, the
 * default size is used.
 * \param StartAddress Pointer to the application-defined function to be executed by the thread.
 * \param Parameter Optional pointer to a variable to be passed to the thread function.
 * \param ThreadHandle Optional pointer to a variable that receives the handle of the new thread.
 * \param ClientId Optional pointer to a CLIENT_ID structure that receives the thread and process identifiers.
 * \return NTSTATUS Successful or errant status.
 * \remarks These alter execution lifecycle rather than just creating/freeing a data structure. Thread exit
 * can become process exit for the last thread; runtime loader notifications and synchronization matter.
 */
NTSYSAPI
NTSTATUS
NTAPI
RtlCreateUserThread(
    _In_ HANDLE ProcessHandle,
    _In_opt_ PSECURITY_DESCRIPTOR ThreadSecurityDescriptor,
    _In_ BOOLEAN CreateSuspended,
    _In_opt_ ULONG ZeroBits,
    _In_opt_ SIZE_T MaximumStackSize,
    _In_opt_ SIZE_T CommittedStackSize,
    _In_ PUSER_THREAD_START_ROUTINE StartAddress,
    _In_opt_ PVOID Parameter,
    _Out_opt_ PHANDLE ThreadHandle,
    _Out_opt_ PCLIENT_ID ClientId
);

NTSTATUS CreateRemoteUserThread(HANDLE hHandle, USER_THREAD_START_ROUTINE StartAddress, PVOID Parameter);
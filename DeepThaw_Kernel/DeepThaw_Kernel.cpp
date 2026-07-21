#include <ntddk.h>

//Interface的头文件以后还会拓展的
#include "Kernel_Interface_DeepFrz.h"


#include "DriverControlCode.h"

//用来通信的devobj
PDEVICE_OBJECT pMainDevobj = nullptr;
//标识是否需要BlockIO
KEVENT event_BlockIO{ 0 };

void DriverUnload(PDRIVER_OBJECT pDrvObj);

//////////////////////////////////////////////
NTSTATUS		CreateCloseDispatch				(DEVICE_OBJECT * pDeviceObject, IRP * Irp);
NTSTATUS		DeviceIoControlDispatch			(DEVICE_OBJECT * pDeviceObject, IRP * Irp);
NTSTATUS		ShutdownNotificationDispatch	(DEVICE_OBJECT * pDeviceObject, IRP * Irp);

NTSTATUS		IoBlock_Dispatch				(DEVICE_OBJECT* pDeviceObject, IRP* Irp);	//用来封锁IO
///////////////////////////////////////////////

extern "C"
NTSTATUS __stdcall DriverEntry
(
	PDRIVER_OBJECT	pDriver_Object,
	PUNICODE_STRING	pRegister_String
)
{
	UNREFERENCED_PARAMETER(pRegister_String);

	
	//填写dispatcher函数
	pDriver_Object->DriverUnload = DriverUnload;

	for (int i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
		pDriver_Object->MajorFunction[i] = IoBlock_Dispatch;

	
	pDriver_Object->MajorFunction[IRP_MJ_CREATE] = CreateCloseDispatch;
	pDriver_Object->MajorFunction[IRP_MJ_CLOSE]  = CreateCloseDispatch;

	pDriver_Object->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceIoControlDispatch;
	pDriver_Object->MajorFunction[IRP_MJ_SHUTDOWN] = ShutdownNotificationDispatch;


	UNICODE_STRING deviceName{ 0 };
	RtlInitUnicodeString(&deviceName,L"\\Device\\DeepThaw_Kernel");

	auto status =
		IoCreateDevice(pDriver_Object, 0, &deviceName, FILE_DEVICE_UNKNOWN
			, FILE_DEVICE_SECURE_OPEN, false, &pMainDevobj);
	if (NT_ERROR(status))
		return status;

	UNICODE_STRING symbolicLink_Name{ 0 };
	RtlInitUnicodeString(&symbolicLink_Name, L"\\DosDevices\\DeepThaw_Kernel");
	status = IoCreateSymbolicLink(&symbolicLink_Name, &deviceName);
	if(NT_ERROR(status))
	{
		IoDeleteDevice(pMainDevobj);
		return status;
	}

	//初始化一个Event对象为io封锁做好准备
	KeInitializeEvent(&event_BlockIO, NotificationEvent, TRUE);

	return STATUS_SUCCESS;
}


void DriverUnload(PDRIVER_OBJECT pDrvObj)
{
	UNREFERENCED_PARAMETER(pDrvObj);

	UNICODE_STRING name_SymbolicLink{ 0 };
	RtlInitUnicodeString(&name_SymbolicLink, L"\\DosDevices\\DeepThaw_Kernel");
	IoDeleteSymbolicLink(&name_SymbolicLink);

	IoDeleteDevice(pMainDevobj);
	pMainDevobj = nullptr;

}

NTSTATUS CreateCloseDispatch(DEVICE_OBJECT* pDeviceObject, IRP* Irp)
{
	if (pDeviceObject != pMainDevobj)
		return IoBlock_Dispatch(pDeviceObject, Irp);

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}


NTSTATUS DeviceIoControlDispatch(DEVICE_OBJECT* pDeviceObject, IRP* Irp)
{
	if (pDeviceObject != pMainDevobj)
		return IoBlock_Dispatch(pDeviceObject, Irp);
	
	auto irpStack = IoGetCurrentIrpStackLocation(Irp);

	bool status = false;
	switch(irpStack->Parameters.DeviceIoControl.IoControlCode)
	{
	case IOCTL_DISABLE_DEEPFRZ:
		status = Kernel_Interface_DeepFrz::DisableDeepFrz(Irp);
		break;
	default:
		Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest(Irp, IO_NO_INCREMENT);

		return STATUS_INVALID_PARAMETER;
	}

	Irp->IoStatus.Status =  STATUS_SUCCESS ;
	Irp->IoStatus.Information = 0;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS ;
}

NTSTATUS ShutdownNotificationDispatch(DEVICE_OBJECT* pDeviceObject, IRP* Irp)
{
	if (pDeviceObject != pMainDevobj)
		return IoBlock_Dispatch(pDeviceObject, Irp);

	Kernel_Interface_DeepFrz::DisableDeepFrz(Irp);

	return STATUS_SUCCESS;
}

NTSTATUS IoBlock_Dispatch(DEVICE_OBJECT* pDeviceObject, IRP* Irp)
{
	if (pDeviceObject == pMainDevobj)		//这个一定要写，不然会bsod
	{
		Irp->IoStatus.Status = STATUS_SUCCESS;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);

		return STATUS_SUCCESS;
	}
		
	KeWaitForSingleObject(&event_BlockIO, Executive, KernelMode, FALSE, nullptr);

	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver((PDEVICE_OBJECT)pDeviceObject->DeviceExtension, Irp);
}
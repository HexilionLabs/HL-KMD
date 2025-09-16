#include "common.h"

BOOLEAN g_AuthManagerInitialized = FALSE;
KSPIN_LOCK g_AuthListLock;
ULONG g_AuthListNextSlot = 0;
AUTH_ENTRY g_AuthList[100];
ULONG g_SpecialProcessId = 0;


NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    g_DriverObject = DriverObject;
    KeInitializeSpinLock(&g_AuthListLock);
    g_AuthManagerInitialized = TRUE;

    LogTrace(1, "Main.c", 0, "DriverEntry", "ZAM Driver loading...");

    return DriverEntryMain(DriverObject, RegistryPath);
}

VOID DriverUnload(PDRIVER_OBJECT DriverObject)
{
    LogTrace(4, "Main.c", 0x1b2, "DriverUnload", "ZAM Driver Unloaded!");

    if (g_IsTraceInitialized) {
        KeWaitForSingleObject(g_TraceFlusherThread, Executive, KernelMode, FALSE, NULL);
        ObfDereferenceObject(g_TraceFlusherThread);
        if (g_TraceLogFilePath->Buffer) {
            ExFreePoolWithTag(g_TraceLogFilePath, ZAM_POOL_TAG);
        }
    }

    if (g_DriverLoadContext == 1) { // Default
        // Things
    }
    else if (g_DriverLoadContext == 3) { // Guard
        // ObUnRegisterCallbacks(...)
    }

    UNICODE_STRING symbolicLinkName;
    if (g_DriverLoadContext == 1) {
        RtlInitUnicodeString(&symbolicLinkName, L"\\DosDevices\\amsdk"); // NIGGA \(-_-)/
    }
    else if (g_DriverLoadContext == 3) {
        RtlInitUnicodeString(&symbolicLinkName, L"\\DosDevices\\B5A6B7C9-1E31-4E62-91CB-6078ED1E9A4F");
    }
    IoDeleteSymbolicLink(&symbolicLinkName);

    if (g_ControlDeviceObject) {
        IoDeleteDevice(g_ControlDeviceObject);
    }
}

NTSTATUS DriverEntryMain(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    g_DriverObject = DriverObject;

    UNICODE_STRING earlyBootPattern, guardPattern;
    RtlInitUnicodeString(&earlyBootPattern, L"*.ZAM_EarlyBoot");
    RtlInitUnicodeString(&guardPattern, L"*ZAM_Guard");

    if (HlpMatchUnicodeString(&earlyBootPattern, RegistryPath)) {
        g_DriverLoadContext = 2; // EarlyBoot
    }
    else if (HlpMatchUnicodeString(&guardPattern, RegistryPath)) {
        g_DriverLoadContext = 3; // Guard
        InitializeTracing(L"ZAM_Guard.krnl.trace");
    }
    else {
        g_DriverLoadContext = 1; // Default
        InitializeTracing(L"ZAM.krnl.trace");
    }

    if (g_DriverLoadContext != 2) {
        return DriverInit(DriverObject);
    }

    return STATUS_SUCCESS;
}



NTSTATUS DriverInit(PDRIVER_OBJECT DriverObject) {
    NTSTATUS status = STATUS_SUCCESS;
    UNICODE_STRING deviceName, symbolicLinkName;
    BOOLEAN symbolicLinkCreated = FALSE;

    g_IsDriverVerifierActive = MmIsDriverVerifying(DriverObject);

    if (g_DriverLoadContext == 1) { // Default
        RtlInitUnicodeString(&deviceName, L"\\Device\\amsdk");
        RtlInitUnicodeString(&symbolicLinkName, L"\\DosDevices\\amsdk");
    }
    else if (g_DriverLoadContext == 3) { // Guard
        RtlInitUnicodeString(&deviceName, L"\\Device\\B5A6B7C9-1E31-4E62-91CB-6078ED1E9A4F");
        RtlInitUnicodeString(&symbolicLinkName, L"\\DosDevices\\B5A6B7C9-1E31-4E62-91CB-6078ED1E9A4F");
    }
    else
        return STATUS_INVALID_PARAMETER;

    // Create the device object.
    status = IoCreateDevice(
        DriverObject,
        sizeof(DEVICE_EXTENSION),
        &deviceName,
        FILE_DEVICE_UNKNOWN, // 0x22
        FILE_DEVICE_SECURE_OPEN, // 0x100
        FALSE,
        &g_ControlDeviceObject
    );

    if (!NT_SUCCESS(status)) {
        LogTrace(7, "Main.c", 0x5D, "DriverInit", "Unable to create DeviceObject %wZ", &deviceName);
        return status;
    }

    DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverDispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverDispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DriverDispatchDeviceControl;
    DriverObject->DriverUnload = DriverUnload;

    PDEVICE_EXTENSION devExt = (PDEVICE_EXTENSION)g_ControlDeviceObject->DeviceExtension;
    devExt->DeviceObject = g_ControlDeviceObject;
    devExt->DriverObject = DriverObject;

    g_ControlDeviceObject->Flags |= DO_BUFFERED_IO;
    g_ControlDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    status = IoCreateDevice(...);
    if (!NT_SUCCESS(status)) {
        LogTrace(...);
        goto cleanup;
    }

    status = IoCreateSymbolicLink(&symbolicLinkName, &deviceName);
    if (!NT_SUCCESS(status)) {
        LogTrace(...);
        goto cleanup;
    }
    symbolicLinkCreated = TRUE;

    if (g_DriverLoadContext == 1) { // Default
        // Things
        LogTrace(1, "Main.c", 0x95, "DriverInit", "ZAM Driver initialized :) PID %d", PsGetCurrentProcessId());
    }
    else if (g_DriverLoadContext == 3) { // Guard
        // Things
        LogTrace(1, "Main.c", 0xA3, "DriverInit", "ZAM Guard initialized :) PID %d", PsGetCurrentProcessId());
    }

cleanup:
    if (!NT_SUCCESS(status)) {
        // This block runs only on failure.
        if (symbolicLinkCreated) {
            IoDeleteSymbolicLink(&symbolicLinkName);
        }
        if (g_ControlDeviceObject) {
            IoDeleteDevice(g_ControlDeviceObject);
        }
    }
    return status;
}

NTSTATUS DriverDispatchCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IofCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS DriverDispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    PIO_STACK_LOCATION ioStackLocation;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG_PTR information = 0;

    ioStackLocation = IoGetCurrentIrpStackLocation(Irp);

    if (!ioStackLocation) {
        status = STATUS_UNSUCCESSFUL;
    }
    else {
        ULONG ioctlCode = ioStackLocation->Parameters.DeviceIoControl.IoControlCode;
        
        ULONG inBufferSize = ioStackLoPVOID ioBuffer = Irp->AssociatedIrp.MasterIrp;cation->Parameters.DeviceIoControl.InputBufferLength;
        ULONG outBufferSize = ioStackLocation->Parameters.DeviceIoControl.OutputBufferLength;

        BOOLEAN needsAuthorization = (ioctlCode != IOCTL_REGISTER_PROCESS);

        if (needsAuthorization) {
            HANDLE currentPid = PsGetCurrentProcessId();
            if (!IsProcessAuthorized((ULONG)(ULONG_PTR)currentPid, TRUE)) {
                status = STATUS_ACCESS_DENIED;
                LogTrace(7, "Main.c", 0x1e2, "DeviceIoControlHandler", "ProcessID %d is not authorized", (ULONG)(ULONG_PTR)currentPid);

                Irp->IoStatus.Status = status;
                Irp->IoStatus.Information = 0;
                IofCompleteRequest(Irp, IO_NO_INCREMENT);
                return status;
            }
        }

        switch (ioctlCode)
        {
        case IOCTL_REGISTER_PROCESS:
            LogTrace(1, "Main.c", 0x20f, "DeviceIoControlHandler", "IOCTL_REGISTER_PROCESS");
            if (inBufferSize >= sizeof(ULONG)) {
                ULONG pid = *(PULONG)ioBuffer;
                status = AddProcessToAuthorizedList(pid);
            }
            else {
                status = STATUS_INVALID_PARAMETER;
            }
            break;

        case IOCTL_TERMINATE_PROCESS:
            LogTrace(1, "Main.c", 0x263, "DeviceIoControlHandler", "IOCTL_TERMINATE_PROCESS");
            // status = TerminateTargetProcess(ioBuffer);
            status = STATUS_SUCCESS;
            break;

            // things

        default:
            LogTrace(7, "Main.c", 0x2ea, "DeviceIoControlHandler", "Unknown IOCTL code provided 0x%X", ioctlCode);
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
        }
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = information;
    IofCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}
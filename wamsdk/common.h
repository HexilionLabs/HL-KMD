#pragma once
#include <ntifs.h>
#include <fltKernel.h>

#define ZAM_POOL_TAG 'ANMZ'

#define AUTH_ENTRY_IMAGENAME_SIZE 2480
typedef struct _AUTH_ENTRY {
    ULONG SessionId;
    ULONG ProcessId;
    CHAR Reserved[4]; // Padding
    CHAR ImageFileName[AUTH_ENTRY_IMAGENAME_SIZE];
} AUTH_ENTRY, * PAUTH_ENTRY;


typedef struct _DEVICE_EXTENSION {
    PDEVICE_OBJECT DeviceObject;
    PDRIVER_OBJECT DriverObject;
    // ... other members
} DEVICE_EXTENSION, * PDEVICE_EXTENSION;

// main.cpp
extern PDRIVER_OBJECT g_DriverObject;
extern PDEVICE_OBJECT g_ControlDeviceObject;
extern ULONG g_DriverLoadContext; // 1 = Default, 2 = EarlyBoot, 3 = Guard
extern UNICODE_STRING g_SystemRootPath;
extern BOOLEAN g_IsDriverVerifierActive;
extern BOOLEAN g_IsDebuggerAttached;

// tracing.cpp
extern BOOLEAN g_IsTraceInitialized;
extern BOOLEAN g_TraceThreadExitSignal;
extern KSPIN_LOCK g_TraceBufferLock;
extern KSEMAPHORE g_TraceFlushSemaphore;
extern PETHREAD g_TraceFlusherThread;
extern PUNICODE_STRING g_TraceLogFilePath;
extern CHAR g_TraceBuffer[0x10000]; // In memory log buffer
extern ULONG g_TraceBufferOffset;
extern BOOLEAN g_TraceThreadExitSignal;

// auth.cpp
extern BOOLEAN g_AuthManagerInitialized;
extern KSPIN_LOCK g_AuthListLock;
extern ULONG g_AuthListNextSlot; // Used for round-robin replacement
extern AUTH_ENTRY g_AuthList[100]; // The global list of authorized processes (0x64 entries)
extern ULONG g_SpecialProcessId;

// IOCTL Codes
#define IOCTL_REGISTER_PROCESS              0x80002010
#define IOCTL_TERMINATE_PROCESS             0x80002048

// main.cpp
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath);
VOID DriverUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS DriverInit(PDRIVER_OBJECT DriverObject);
NTSTATUS DriverDispatchCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DriverDispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);

// helpers.cpp
PUNICODE_STRING HlpAllocateUnicodeString(PCWSTR Buffer, USHORT ExtraLength, POOL_TYPE PoolType);
BOOLEAN HlpMatchUnicodeString(PUNICODE_STRING Pattern, PUNICODE_STRING Name);

// tracing.cpp
VOID InitializeTracing(PCWSTR LogFileName);
VOID LogTrace(ULONG Level, PCSTR SourceFile, ULONG LineNumber, PCSTR FunctionName, PCSTR Format, ...);
VOID TraceFlusherThread(PVOID StartContext);

// auth.cpp
NTSTATUS AddProcessToAuthorizedList(_In_ ULONG ProcessId);
BOOLEAN IsProcessAuthorized(_In_ ULONG ProcessId, _In_ BOOLEAN AcquireLock);
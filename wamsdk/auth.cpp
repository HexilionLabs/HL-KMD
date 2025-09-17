#include "auth.h"

BOOLEAN IsProcessAuthorized(ULONG ProcessId, BOOLEAN AcquireLock)
{
    BOOLEAN isFound = FALSE;
    KIRQL oldIrql = 0;

    if (!g_AuthManagerInitialized) {
        LogTrace(5, "AuthenticationManager.c", 0x1c8, "ZmnAuthIsRegisteredProcessId", "Authentication Manager is not initialized");
        return FALSE;
    }

    if (ProcessId == 0) {
        return FALSE;
    }

    if (g_SpecialProcessId != 0 && g_SpecialProcessId == ProcessId) {
        return TRUE;
    }

    if (AcquireLock) {
        KeAcquireSpinLock(&g_AuthListLock, &oldIrql);
    }

    for (int i = 0; i < 100; i++) {
        if (g_AuthList[i].ProcessId == ProcessId) {
            isFound = TRUE;
            break;
        }
    }

    if (AcquireLock) {
        KeReleaseSpinLock(&g_AuthListLock, oldIrql);
    }

    return isFound;
}


NTSTATUS AddProcessToAuthorizedList(ULONG ProcessId) 
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    PEPROCESS pProcess = NULL;

    if (!g_AuthManagerInitialized) {
        LogTrace(5, "AuthenticationManager.c", 0x10a, "ZmnAuthRegisterProcess", "Authentication Manager is not initialized");
        return STATUS_UNSUCCESSFUL;
    }

    if (IsProcessAuthorized(ProcessId, TRUE)) {
        LogTrace(1, "AuthenticationManager.c", 0x114, "ZmnAuthRegisterProcess", "%d is already registered", ProcessId);
        return STATUS_SUCCESS;
    }

    status = PsLookupProcessByProcessId((HANDLE)ProcessId, &pProcess);
    if (!NT_SUCCESS(status)) {
        LogTrace(7, "AuthenticationManager.c", 0x122, "ZmnAuthRegisterProcess", "Can not lookup process by id %d", ProcessId);
        return status;
    }

    PCHAR imageName = PsGetProcessImageFileName(pProcess);
    if (!imageName) {
        LogTrace(7, "AuthenticationManager.c", 0x12e, "ZmnAuthRegisterProcess", "Can not get process name for pid %d", ProcessId);
        ObfDereferenceObject(pProcess);
        return STATUS_UNSUCCESSFUL;
    }

    KIRQL oldIrql;
    KeAcquireSpinLock(&g_AuthListLock, &oldIrql);

    LONG slotIndex = -1;
    for (int i = 0; i < 100; i++) {
        if (g_AuthList[i].ProcessId == 0) {
            slotIndex = i;
            break;
        }
    }

    if (slotIndex == -1) {
        slotIndex = g_AuthListNextSlot;
        g_AuthListNextSlot = (g_AuthListNextSlot + 1) % 100;
    }

    PAUTH_ENTRY entry = &g_AuthList[slotIndex];
    entry->ProcessId = ProcessId; // NIGGA \(-__-)/
    entry->SessionId = PsGetProcessSessionId(pProcess);
    RtlCopyMemory(entry->ImageFileName, imageName, 255);

    KeReleaseSpinLock(&g_AuthListLock, oldIrql);

    LogTrace(1, "AuthenticationManager.c", 0x167, "ZmnAuthRegisterProcess",
        "ProcessID %d ProcessName %s registered to #%d slot / total %d",
        ProcessId, entry->ImageFileName, slotIndex, g_AuthListNextSlot);

    ObfDereferenceObject(pProcess);

    return STATUS_SUCCESS;
}

#include "common.h"
#include <stdarg.h>
#include <ntstrsafe.h>

static void SafeVsnPrintf(char** pBuffer, size_t* pRemainingSize, const char* Format, ...)
{
    va_list args;
    va_start(args, Format);

    int written = _vsnprintf(*pBuffer, *pRemainingSize, Format, args);
    if (written > 0) {
        *pBuffer += written;
        *pRemainingSize -= written;
    }

    va_end(args);
}

VOID InitializeTracing(PCWSTR LogFileName)
{
    HANDLE hThread;

    if (g_IsTraceInitialized) return;

    RtlZeroMemory(&g_TraceBuffer, sizeof(g_TraceBuffer));
    KeInitializeSpinLock(&g_TraceBufferLock);
    KeInitializeSemaphore(&g_TraceFlushSemaphore, 0, MAXLONG);

    g_TraceLogFilePath = HlpAllocateUnicodeString(L"\\SystemRoot\\", 0x100, PagedPool);
    if (!g_TraceLogFilePath) {
        DbgPrint("System low on resources\n");
        return;
    }

    RtlAppendUnicodeToString(g_TraceLogFilePath, LogFileName);

    NTSTATUS status = PsCreateSystemThread(
        &hThread,
        THREAD_ALL_ACCESS,
        NULL,
        NULL,
        NULL,
        TraceFlusherThread,
        NULL
    );

    if (!NT_SUCCESS(status)) {
        DbgPrint("Can not create flusher thread\n");
        return;
    }

    status = ObReferenceObjectByHandle(hThread, THREAD_ALL_ACCESS, *PsThreadType, KernelMode, (PVOID*)&g_TraceFlusherThread, NULL);
    ZwClose(hThread);

    if (NT_SUCCESS(status)) {
        DbgPrint("Log file initialized %wZ\n", g_TraceLogFilePath);
        g_IsTraceInitialized = TRUE;
    }
    else {
        DbgPrint("Can not reference flusher thread\n");
    }
}

VOID TraceFlusherThread(PVOID StartContext)
{
    UNREFERENCED_PARAMETER(StartContext);

    LARGE_INTEGER timeout;
    timeout.QuadPart = -50000000; // 5 sec timeout

    PVOID localBuffer = ExAllocatePoolWithTag(PagedPool, 0x10000, ZAM_POOL_TAG);
    if (!localBuffer) {
        DbgPrint("ZAM TraceThread: Failed to allocate local flush buffer.\n");
        PsTerminateSystemThread(STATUS_INSUFFICIENT_RESOURCES);
        return;
    }

    // delete any pre existing log file on startup
    OBJECT_ATTRIBUTES objAttr;
    InitializeObjectAttributes(&objAttr, g_TraceLogFilePath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    ZwDeleteFile(&objAttr);

    while (g_TraceThreadExitSignal == FALSE)
    {
        KeWaitForSingleObject(&g_TraceFlushSemaphore, Executive, KernelMode, FALSE, &timeout);

        if (g_TraceThreadExitSignal == TRUE) {
            break;
        }

        if (g_TraceBufferOffset > 0)
        {
            KIRQL oldIrql;
            ULONG dataLength;

            KeAcquireSpinLock(&g_TraceBufferLock, &oldIrql);
            dataLength = g_TraceBufferOffset;
            if (dataLength > 0) {
                RtlCopyMemory(localBuffer, g_TraceBuffer, dataLength);
                g_TraceBufferOffset = 0;
            }
            KeReleaseSpinLock(&g_TraceBufferLock, oldIrql);

            if (dataLength > 0)
            {
                HANDLE hFile = NULL;
                IO_STATUS_BLOCK ioStatusBlock;

                NTSTATUS status = ZwCreateFile(
                    &hFile,
                    FILE_APPEND_DATA | SYNCHRONIZE,
                    &objAttr,
                    &ioStatusBlock,
                    NULL,
                    FILE_ATTRIBUTE_NORMAL,
                    FILE_SHARE_READ,
                    FILE_OPEN_IF, // Open if exists, create if not
                    FILE_SYNCHRONOUS_IO_NONALERT,
                    NULL, 0
                );

                if (NT_SUCCESS(status)) {
                    ZwWriteFile(hFile, NULL, NULL, NULL, &ioStatusBlock, localBuffer, dataLength, NULL, NULL);
                    ZwClose(hFile);
                }
            }
        }
    }

    ExFreePoolWithTag(localBuffer, ZAM_POOL_TAG);
    PsTerminateSystemThread(STATUS_SUCCESS);
}

VOID LogTrace(ULONG Level, PCSTR SourceFile, ULONG LineNumber, PCSTR FunctionName, PCSTR Format, ...)
{
    CHAR logMessage[512];
    CHAR tempFormat[256];
    va_list args;

    const char* fileNameOnly = strrchr(SourceFile, '\\');
    if (fileNameOnly) {
        fileNameOnly++;
    }
    else {
        fileNameOnly = SourceFile;
    }

    RtlStringCbPrintfA(
        tempFormat, sizeof(tempFormat),
        "[ZAM] %s:%lu (%s): %s\n",
        fileNameOnly, LineNumber, FunctionName, Format
    );

    va_start(args, Format);
    RtlStringCbVPrintfA(logMessage, sizeof(logMessage), tempFormat, args);
    va_end(args);

    DbgPrint(logMessage);

    if (g_IsTraceInitialized)
    {
        KIRQL oldIrql;
        size_t messageLen = strlen(logMessage);

        KeAcquireSpinLock(&g_TraceBufferLock, &oldIrql);

        if ((g_TraceBufferOffset + messageLen) < sizeof(g_TraceBuffer))
        {
            RtlCopyMemory(&g_TraceBuffer[g_TraceBufferOffset], logMessage, messageLen);
            g_TraceBufferOffset += (ULONG)messageLen;
        }

        KeReleaseSpinLock(&g_TraceBufferLock, oldIrql);

        // signal flusher thread there is data to write
        KeReleaseSemaphore(&g_TraceFlushSemaphore, 0, 1, FALSE);
    }
}
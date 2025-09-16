#include "controller.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void PrintHeader(void) {
    printf("::----------------------------------------::\n");
    printf("::       Process Control Utility (C)      ::\n");
    printf("::    (Hexilion Labs - Malware Research)  ::\n");
    printf("::----------------------------------------::\n\n");
}

void ReadInput(char* buffer, int size) {
    if (fgets(buffer, size, stdin)) {
        buffer[strcspn(buffer, "\n")] = 0;
    }
}

int main(void) {
    PrintHeader();

    Controller* controller = Controller_Create();
    if (controller == NULL) {
        fprintf(stderr, "[CRITICAL] Initialization failed. Last Error: %lu\n", GetLastError());
        printf("Press Enter to exit.\n");
        getchar();
        return 1;
    }

    printf("[+] Driver connection established and process authorized.\n");
    printf("------------------------------------------\n\n");

    char inputBuffer[256];
    while (1) {
        printf("[?] Enter the PID to terminate (or 'q' to quit): ");
        ReadInput(inputBuffer, sizeof(inputBuffer));

        if (strcmp(inputBuffer, "q") == 0) {
            break;
        }

        char* endPtr;
        DWORD targetPid = strtoul(inputBuffer, &endPtr, 10);
        if (*endPtr != '\0') {
            printf("[!] Invalid input. Should be a number.\n\n");
            continue;
        }

        printf("[?] Wait for process to exit completely? (y/n): ");
        ReadInput(inputBuffer, sizeof(inputBuffer));
        bool shouldWait = (tolower((unsigned char)inputBuffer[0]) == 'y');

        printf("\n[*] Terminating Process %lu...\n", targetPid);
        if (Controller_TerminateProcess(controller, targetPid, shouldWait)) {
            printf("[+] Terminated successfully.\n");
        }
        else {
            printf("[-] Failed to send command. (Error: %lu)\n", GetLastError());
        }
        printf("------------------------------------------\n\n");
    }

    printf("[~] Cleaning up and exiting...\n");
    Controller_Destroy(controller);

    return 0;
}
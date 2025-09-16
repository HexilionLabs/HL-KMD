#pragma once
#pragma once
#include <windows.h>
#include <stdbool.h>

typedef struct Controller Controller;

Controller* Controller_Create(void);

void Controller_Destroy(Controller* controller);

BOOL Controller_TerminateProcess(Controller* controller, DWORD processId, bool waitForExit);
#pragma once
#include "common.h"

NTSTATUS AddProcessToAuthorizedList(_In_ ULONG ProcessId);
BOOLEAN IsProcessAuthorized(_In_ ULONG ProcessId, _In_ BOOLEAN AcquireLock);
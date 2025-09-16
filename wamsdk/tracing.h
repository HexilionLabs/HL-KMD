#pragma once
#pragma once
#include "common.h"

VOID InitializeTracing(
    _In_ PCWSTR LogFileName
);

VOID LogTrace(
    _In_ ULONG Level,
    _In_ PCSTR SourceFile,
    _In_ ULONG LineNumber,
    _In_ PCSTR FunctionName,
    _In_ PCSTR Format,
    _In_ ...
);

VOID TraceFlusherThread(_In_ PVOID StartContext);
#pragma once
#include "common.h"

PUNICODE_STRING HlpAllocateUnicodeString(
    _In_ PCWSTR Buffer,
    _In_ USHORT ExtraLength,
    _In_ POOL_TYPE PoolType
);

BOOLEAN HlpMatchUnicodeString(
    _In_ PUNICODE_STRING Pattern,
    _In_ PUNICODE_STRING Name
);
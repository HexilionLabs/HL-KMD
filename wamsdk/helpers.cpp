#include "helpers.h"

PUNICODE_STRING HlpAllocateUnicodeString(PCWSTR Buffer, USHORT ExtraLength, POOL_TYPE PoolType)
{
    if (!Buffer) {
        return NULL;
    }

    size_t bufferCb = wcslen(Buffer) * sizeof(WCHAR);
    size_t extraCb = ExtraLength * sizeof(WCHAR);
    size_t totalCb = sizeof(UNICODE_STRING) + bufferCb + extraCb + sizeof(UNICODE_NULL);

    // allocate single block of mem for struct and string buffer.
    // flag 0x400 is POOL_RAISE_IF_ALLOCATION_FAILURE.
    PUNICODE_STRING pUnicodeString = (PUNICODE_STRING)ExAllocatePoolWithTag(
        (POOL_TYPE)(PoolType | POOL_RAISE_IF_ALLOCATION_FAILURE),
        totalCb,
        ZAM_POOL_TAG
    );

    if (!pUnicodeString) {
        return NULL;
    }

    // Zero all the allocation if debugger is not attached.
    if (!g_IsDebuggerAttached) {
        RtlZeroMemory(pUnicodeString, totalCb);
    }

    pUnicodeString->Buffer = (PWCH)((PCHAR)pUnicodeString + sizeof(UNICODE_STRING));
    pUnicodeString->Length = (USHORT)bufferCb;
    pUnicodeString->MaximumLength = (USHORT)(bufferCb + extraCb);

    RtlCopyMemory(pUnicodeString->Buffer, Buffer, bufferCb);

    return pUnicodeString;
}


BOOLEAN HlpMatchUnicodeString(PUNICODE_STRING Pattern, PUNICODE_STRING Name)
{
    BOOLEAN isMatch = FALSE;
    UNICODE_STRING upcasedPattern;
    upcasedPattern.Buffer = NULL;

    upcasedPattern.MaximumLength = Pattern->MaximumLength;
    upcasedPattern.Buffer = (PWCH)ExAllocatePoolWithTag(
        PagedPool,
        Pattern->MaximumLength,
        ZAM_POOL_TAG
    );

    if (!upcasedPattern.Buffer) {
        LogTrace(7, "HelperFunctions.c", 0x62, "HlpMatchUnicodeString", "Can not allocate unicode string %wZ", Pattern);
        return FALSE;
    }

    NTSTATUS status = RtlUpcaseUnicodeString(&upcasedPattern, Pattern, FALSE); // FALSE = Do not allocate destination
    if (!NT_SUCCESS(status)) {
        LogTrace(7, "HelperFunctions.c", 0x76, "HlpMatchUnicodeString", "Can not upcase %wZ", Pattern);
        ExFreePoolWithTag(upcasedPattern.Buffer, ZAM_POOL_TAG);
        return FALSE;
    }

    // wildcard expression match
    isMatch = FsRtlIsNameInExpression(&upcasedPattern, Name, TRUE, NULL); // TRUE = IgnoreCase

    ExFreePoolWithTag(upcasedPattern.Buffer, ZAM_POOL_TAG);

    return isMatch;
}
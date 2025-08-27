//
// callbacks_singlethreaded.c
// Contains symcrypt call back functions for single threaded applications.
//
// Copyright (c) Microsoft Corporation. Licensed under the MIT license.
//

#include "precomp.h"

PVOID
SYMCRYPT_CALL
SymCryptCallbackAlloc( SIZE_T nBytes )
{
    SIZE_T cbAllocation = (nBytes + (SYMCRYPT_ASYM_ALIGN_VALUE - 1)) & ~(SYMCRYPT_ASYM_ALIGN_VALUE - 1);
    size_t alignment = SYMCRYPT_ASYM_ALIGN_VALUE;
    if (alignment < sizeof(void*)) {
	    alignment = sizeof(void*);
    }

    void *raw = malloc(cbAllocation + sizeof(void*));
    uintptr_t raw_addr = (uintptr_t)raw + sizeof(void*);
    uintptr_t aligned_addr = (raw_addr + alignment - 1) & ~(uintptr_t)(alignment - 1);

    ((void**)aligned_addr)[-1] = raw;

    return (void*)aligned_addr;
}

VOID
SYMCRYPT_CALL
SymCryptCallbackFree( VOID * pMem )
{
    free( ((void**)pMem)[-1] );
}

SYMCRYPT_ERROR
SYMCRYPT_CALL
SymCryptCallbackRandom( PBYTE pbBuffer, SIZE_T cbBuffer )
{
    SymCryptRandom( pbBuffer, cbBuffer );
    return SYMCRYPT_NO_ERROR;
}

PVOID
SYMCRYPT_CALL
SymCryptCallbackAllocateMutexFastInproc(void)
{
    static const BYTE byte = 0;

    // we want to return a valid non-NULL address so caller can check for NULL
    return (PVOID)&byte;
}

VOID
SYMCRYPT_CALL
SymCryptCallbackFreeMutexFastInproc( PVOID pMutex ) {}

VOID
SYMCRYPT_CALL
SymCryptCallbackAcquireMutexFastInproc( PVOID pMutex ) {}

VOID
SYMCRYPT_CALL
SymCryptCallbackReleaseMutexFastInproc( PVOID pMutex ) {}


// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_probe.c: the probe on Windows: the rule that a fault names, the launch queue that every process on the host
// shares, and the lock that one start of a probe holds. The start itself is the platform's (platform.h)
#include "../../../src/cu/transpiler/lstar/interface/interface_internal.h"
#include "../platform.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

// the rule that a Windows fault names: an unhandled exception ends its process with the exception's NTSTATUS as its
// code
static InterfaceFault interface_fault_of(unsigned long long code)
{
    switch (code)
    {
    // an integer division by zero, one that overflows, and the floating-point traps from denormal operand to
    // underflow
    case 0xC0000094ULL:
    case 0xC0000095ULL:
    case 0xC000008DULL:
    case 0xC000008EULL:
    case 0xC000008FULL:
    case 0xC0000090ULL:
    case 0xC0000091ULL:
    case 0xC0000092ULL:
    case 0xC0000093ULL:
        return INTERFACE_FAULT_ARITHMETIC;
    // an access violation, and a page the system could not bring in
    case 0xC0000005ULL:
    case 0xC0000006ULL:
        return INTERFACE_FAULT_ADDRESS;
    // an illegal instruction, and a privileged one
    case 0xC000001DULL:
    case 0xC0000096ULL:
        return INTERFACE_FAULT_INSTRUCTION;
    case 0xC00000FDULL:
        return INTERFACE_FAULT_STACK;
    case 0x80000003ULL:
        return INTERFACE_FAULT_TRAP;
    // the fail-fast that a runtime ends itself with, abort among its callers
    case 0xC0000409ULL:
        return INTERFACE_FAULT_ABORT;
    default:
        return INTERFACE_FAULT_OTHER;
    }
}

void interface_ending_read(const PlatformEnding *ending, InterfaceAnswer *answer)
{
    // an exit code with its top bit set is the NTSTATUS of the fault that ended the probe
    const int faulted = (ending->code & 0x80000000ULL) != 0ULL;
    answer->code = ending->code;
    answer->ending = faulted ? INTERFACE_ENDING_FAULTED : INTERFACE_ENDING_EXITED;
    answer->fault = faulted ? interface_fault_of(ending->code) : INTERFACE_FAULT_NONE;
}

// The lock of one start of a probe (interface_internal.h)
static SRWLOCK s_interface_starting = SRWLOCK_INIT;

void interface_starting_held(void)
{
    AcquireSRWLockExclusive(&s_interface_starting);
}

void interface_starting_let_go(void)
{
    ReleaseSRWLockExclusive(&s_interface_starting);
}

// The launch queue (interface.h): two semaphores that every process on the host opens by name. One counts the places
// for the children that run at once. The other is the start under way, which its taker holds for the spacing after the
// start. A process that the system gives no named semaphore queues its own starts alone
static HANDLE s_launches = NULL;
static HANDLE s_launching = NULL;
static INIT_ONCE s_queue_opened = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK interface_queue_open(PINIT_ONCE once, PVOID parameter, PVOID *context)
{
    (void)once;
    (void)parameter;
    (void)context;
    // the counts fit a LONG
    s_launches = CreateSemaphoreW(NULL, (LONG)INTERFACE_LAUNCHES_AT_ONCE, (LONG)INTERFACE_LAUNCHES_AT_ONCE,
                                  L"Local\\orior_launches");
    s_launching = CreateSemaphoreW(NULL, 1L, 1L, L"Local\\orior_launching");
    if (s_launches == NULL)
    {
        s_launches = CreateSemaphoreW(NULL, (LONG)INTERFACE_LAUNCHES_AT_ONCE, (LONG)INTERFACE_LAUNCHES_AT_ONCE, NULL);
    }
    if (s_launching == NULL)
    {
        s_launching = CreateSemaphoreW(NULL, 1L, 1L, NULL);
    }
    return TRUE;
}

void interface_launch_taken(void)
{
    InitOnceExecuteOnce(&s_queue_opened, interface_queue_open, NULL, NULL);
    if (s_launches != NULL)
    {
        WaitForSingleObject(s_launches, INFINITE);
    }
    if (s_launching != NULL)
    {
        WaitForSingleObject(s_launching, INFINITE);
    }
}

void interface_launch_started(void)
{
    platform_paused(INTERFACE_LAUNCH_SPACING_MILLISECONDS);
    if (s_launching != NULL)
    {
        ReleaseSemaphore(s_launching, 1L, NULL);
    }
}

void interface_launch_given(void)
{
    if (s_launches != NULL)
    {
        ReleaseSemaphore(s_launches, 1L, NULL);
    }
}
#endif

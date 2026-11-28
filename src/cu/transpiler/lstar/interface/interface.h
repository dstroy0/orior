// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef INTERFACE_H
#define INTERFACE_H

// The interface, a probe runner (engine_table.md item 11(f) 3). A probe is a small program that asks the target one
// question. It runs in a probe process that the interface can lose, and the interface records how it ended. That is the
// exit status, or the signal or fault that ended it, with the output that it wrote and the time that it took. Its death
// answers a probe that kills its process, and the next question goes from a fresh process

#include "../../../engine/engine_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // how a probe ended
    typedef enum
    {
        // the program could not start, and nothing ran
        INTERFACE_ENDING_NOT_STARTED = 0,
        // it returned from main or called exit: the code is its exit status
        INTERFACE_ENDING_EXITED = 1,
        // a signal ended it (POSIX): the code is the signal's number
        INTERFACE_ENDING_SIGNALED = 2,
        // an exception that nothing handled ended it (Windows), and the code is its NTSTATUS. The interface reads an
        // exit status with its top bit on, an NTSTATUS of warning or error severity, as the fault that ended the
        // process. A program that exits with one by choice reads as that fault too
        INTERFACE_ENDING_FAULTED = 3,
        // its time ran out and the interface ended it and every process it started
        INTERFACE_ENDING_OUT_OF_TIME = 4
    } InterfaceEnding;

    // the rule a signal or a fault names, the same on every host; the code preserves what the host distinguished
    typedef enum
    {
        INTERFACE_FAULT_NONE = 0,
        // an integer division by zero or one that overflows (`INT_MIN / -1`), or a floating-point trap. That is
        // SIGFPE, which does not tell them apart, or on Windows 0xC0000094, 0xC0000095 and 0xC000008D to 0xC0000093,
        // which do
        INTERFACE_FAULT_ARITHMETIC = 1,
        // an address the process may not read or write: SIGSEGV or SIGBUS, or 0xC0000005 and 0xC0000006
        INTERFACE_FAULT_ADDRESS = 2,
        // an instruction the part lacks or the process may not run: SIGILL, or 0xC000001D and 0xC0000096
        INTERFACE_FAULT_INSTRUCTION = 3,
        // the stack ran past its end: 0xC00000FD. POSIX delivers it as SIGSEGV, read as an address
        INTERFACE_FAULT_STACK = 4,
        // a trap or breakpoint: SIGTRAP, or 0x80000003
        INTERFACE_FAULT_TRAP = 5,
        // the program ended itself as failed: SIGABRT, or the fail-fast 0xC0000409
        INTERFACE_FAULT_ABORT = 6,
        // any other signal or fault
        INTERFACE_FAULT_OTHER = 7
    } InterfaceFault;

    // a probe:
    //
    // - its command, ended by NULL, with the program first, found along PATH where it names no folder
    // - the file that its output and its errors go to, together, as the probe writes them. The interface makes it new.
    //   Where it is NULL, they come back through a pipe straight into the answer. The interface reads them as the
    //   probe writes them, so that the probe never waits on them, and makes no file.
    // - the max time that it gets, 0 for no limit
    //
    // Its input is empty
    typedef struct
    {
        char *const *command;
        const char *output_path;
        unsigned long long limit_microseconds;
    } InterfaceProbe;

    // what the interface saw: how the probe ended and its code, the rule that this names, and the bytes of output that
    // it wrote. The interface reads as many of them as `output_capacity` allows into `output`, ended by a zero byte
    // (the capacity counts that byte). Then its wall time. `output` can be NULL with a capacity of 0
    typedef struct
    {
        InterfaceEnding ending;
        unsigned long long code;
        InterfaceFault fault;
        unsigned long long output_bytes;
        char *output;
        unsigned long long output_capacity;
        unsigned long long microseconds;
    } InterfaceAnswer;

    // runs one probe to its end and fills the answer. The return is 0 after the probe ended, however it ended. It is -1
    // where the interface itself failed, with the error raised in module ENGINE_MODULE_INTERFACE. Examples are an
    // output file that the interface could not make or read, and a started probe that it could not wait on
    long interface_probe_run(const InterfaceProbe *probe, InterfaceAnswer *answer, EngineError *error);

    // The launch queue, shared by every process on the host that starts a child through the interface. At max
    // INTERFACE_LAUNCHES_AT_ONCE children run at once, and INTERFACE_LAUNCH_SPACING_MILLISECONDS pass between one start
    // and the next. The call interface_probe_run takes a place before it starts a probe. After the start, it signals
    // it, which delays the next start for the spacing. It gives the place back when the probe ended. A program that
    // starts a child of its own, such as the compilers that the query crate starts, paces itself by the same two
    // numbers
#define INTERFACE_LAUNCHES_AT_ONCE 4u
#define INTERFACE_LAUNCH_SPACING_MILLISECONDS 50u

    // a place in the queue taken, and the start under way: waits until both are free
    void interface_launch_taken(void);

    // the start made: the spacing passes, and the next start may go
    void interface_launch_started(void);

    // the place given back, after the child ended or where it never started
    void interface_launch_given(void);

    // the ending's name, and the fault's
    const char *interface_ending_name(InterfaceEnding ending);

    const char *interface_fault_name(InterfaceFault fault);

#ifdef __cplusplus
}
#endif

#endif

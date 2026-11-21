// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// The interface's PTX test (engine_table.md item 11(f) 4). The interface runs interface_ptx_probe, one question a
// process: the membership queries, every arithmetic, test and conversion form of ptx.krs checked against the host's
// integers; then the illegal operations, each queried from a fresh process, since a fault on the device leaves its
// context unusable; and after each, a fresh process whose device answers. The first argument is interface_ptx_probe's
// path, the second a folder for the probes' output
#include "../../../lstar/interface/interface.h"
#include "../../../../engine/engine_note.h"
#include "memoria_operor/memoria_linea.h"
#include "cellularum_laboro/cellularum_linea.h"

#include <stdio.h>
#include <string.h>

#define INTERFACE_PTX_CAPACITY 65536u
// a question's limit: the membership queries assemble and run a kernel for each question
#define INTERFACE_PTX_LIMIT 600000000ULL

typedef struct
{
    const char *probe;
    const char *folder;
    unsigned int checks;
    unsigned int failed;
} InterfacePtxTest;

static char s_interface_ptx_output[INTERFACE_PTX_CAPACITY];

static void interface_ptx_check(InterfacePtxTest *test, int passed, const char *what)
{
    test->checks += 1u;
    test->failed += passed ? 0u : 1u;
    printf("  %s %s\n", passed ? "ok  " : "FAIL", what);
}

// one question queried of the probe in a process of its own, its answer and its output printed whole; 0 where the
// interface itself failed
static int interface_ptx_probe(InterfacePtxTest *test, const char *question, InterfaceAnswer *answer)
{
    char path[1024];
    {
        VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put(&buffer_line, test->folder);
        mmgr_line_put(&buffer_line, "/");
        mmgr_line_put(&buffer_line, question);
        mmgr_line_put(&buffer_line, ".out");
        (void)mmgr_line_finish(&buffer_line);
    }
    char *const command[] = {(char *)test->probe, (char *)question, NULL};
    const InterfaceProbe probe = {command, path, INTERFACE_PTX_LIMIT};
    mmgr_bytes_set(answer, 0, sizeof(*answer));
    answer->output = s_interface_ptx_output;
    answer->output_capacity = sizeof(s_interface_ptx_output);
    EngineError error;
    mmgr_bytes_set(&error, 0, sizeof(error));
    if (interface_probe_run(&probe, answer, &error) != 0L)
    {
        printf("  %s: the interface failed, module %d site %u status %d\n", question, (int)error.module, error.site,
               error.status);
        return 0;
    }
    printf("  %s: %s, code %llu (0x%llx), fault %s, %llu bytes of output, %llu.%06llu s\n", question,
           interface_ending_name(answer->ending), answer->code, answer->code, interface_fault_name(answer->fault),
           answer->output_bytes, answer->microseconds / 1000000ULL, answer->microseconds % 1000000ULL);
    printf("%s", s_interface_ptx_output);
    return 1;
}

// a fresh process whose device answers 6 + 7
static void interface_ptx_alive(InterfacePtxTest *test, const char *after)
{
    InterfaceAnswer answer;
    char what[256];
    const int result = interface_ptx_probe(test, "alive", &answer);
    {
        VerbaLinea buffer_line = mmgr_line_over(what, sizeof(what));
        mmgr_line_put(&buffer_line, "a fresh probe after ");
        mmgr_line_put(&buffer_line, after);
        mmgr_line_put(&buffer_line, ": the device answers 6 + 7 = 0x0000000d");
        (void)mmgr_line_finish(&buffer_line);
    }
    interface_ptx_check(
        test,
        result && (answer.ending == INTERFACE_ENDING_EXITED) && (answer.code == 0ULL) &&
            (mmgr_text_find(s_interface_ptx_output, "answered 0000000d", PLATFORM_TEXT_TO_TERMINATOR) != NULL),
        what);
}

// an illegal operation the device errors: the probe exits 3 having printed the CUDA error, which `error` names where
// it is given
static void interface_ptx_error(InterfacePtxTest *test, const char *question, const char *error, const char *what)
{
    InterfaceAnswer answer;
    const int result = interface_ptx_probe(test, question, &answer);
    const int named = (error == NULL)
                          ? (mmgr_text_find(s_interface_ptx_output, "error ", PLATFORM_TEXT_TO_TERMINATOR) != NULL)
                          : (mmgr_text_find(s_interface_ptx_output, error, PLATFORM_TEXT_TO_TERMINATOR) != NULL);
    interface_ptx_check(test, result && (answer.ending == INTERFACE_ENDING_EXITED) && (answer.code == 3ULL) && named,
                        what);
}

int program_main(int count, char **arguments)
{
    if (count < 3)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  interface_ptx_test: <interface_ptx_probe> <output folder>");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    InterfacePtxTest test = {arguments[1], arguments[2], 0u, 0u};
    InterfaceAnswer answer;

    interface_ptx_alive(&test, "nothing");

    int result = interface_ptx_probe(&test, "membership", &answer);
    interface_ptx_check(&test,
                        result && (answer.ending == INTERFACE_ENDING_EXITED) && (answer.code == 0ULL) &&
                            (mmgr_text_find(s_interface_ptx_output, " questions, 0 with a case that differs",
                                            PLATFORM_TEXT_TO_TERMINATOR) != NULL),
                        "membership: every defined answer of every form agrees with the host's integers");

    interface_ptx_error(&test, "address", "error 700 cudaErrorIllegalAddress",
                        "a load from address 16 errors as an illegal address (700)");
    interface_ptx_alive(&test, "an illegal address");
    interface_ptx_error(&test, "misaligned", "error 716 cudaErrorMisalignedAddress",
                        "a 32-bit load one byte in errors as a misaligned address (716)");
    interface_ptx_alive(&test, "a misaligned address");
    interface_ptx_error(&test, "trap", NULL, "trap errors with a CUDA error");
    interface_ptx_alive(&test, "a trap");

    result = interface_ptx_probe(&test, "lacking", &answer);
    interface_ptx_check(&test,
                        result && (answer.ending == INTERFACE_ENDING_EXITED) && (answer.code == 4ULL) &&
                            (mmgr_text_find(s_interface_ptx_output, "errored", PLATFORM_TEXT_TO_TERMINATOR) != NULL),
                        "elect.sync, which PTX gives sm_90 and later, errors in the toolchain for this device");
    interface_ptx_alive(&test, "an instruction the part lacks");

    printf("  interface ptx test: %u checks, %u failed\n", test.checks, test.failed);
    return (test.failed == 0u) ? 0 : 1;
}

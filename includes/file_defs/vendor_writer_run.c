// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// vendor_writer_run.c: the vendor's writer run through the interface by a format's writer, its output printed and
// passed to the run
#include "vendor_writer_run.h"

#include "../../src/cu/transpiler/lstar/interface/interface.h"
#include "qry/writer/qry_member.h"

#include <stdio.h>

int vendor_writer_run(char *const *command, const char *machine_name, const char *program, const char *mode)
{
    static char s_output[65536];
    const InterfaceProbe probe = {command, NULL, 0ull};
    InterfaceAnswer answer = {0};
    answer.output = s_output;
    answer.output_capacity = sizeof(s_output);
    EngineError error = {0};
    // what this process passed moved into the run before the writer reads it, and what the writer passed once it ends
    qry_member_settled();
    const long ran = interface_probe_run(&probe, &answer, &error);
    qry_member_rings_settled();
    if (answer.output_bytes != 0ull)
    {
        printf("%s", s_output);
        const unsigned long long output_len =
            (answer.output_bytes < (sizeof(s_output) - 1u)) ? answer.output_bytes : (sizeof(s_output) - 1u);
        qry_member_append(machine_name, s_output, output_len, QRY_ORDINARY);
    }
    if ((ran != 0L) || (answer.ending != INTERFACE_ENDING_EXITED) || (answer.code != 0ull))
    {
        printf("  %s: the vendor's writer%s%s did not end clean (%s, code %llx)\n", program, (mode != NULL) ? " " : "",
               (mode != NULL) ? mode : "", interface_ending_name(answer.ending), answer.code);
        return 0;
    }
    return 1;
}

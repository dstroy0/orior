// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// head.c: the lines of a SASS ruleset's head, each the part's answer in its .ksc or left out
#include "head.h"

#include "../../../../types/file_defs/ksc/ksc.h"

#include <stdio.h>
#include <string.h>

unsigned int krs_head_written(QryText *head, const char *ksc_path, char *missing, size_t room)
{
    static KscFile s_file;
    static KscRow s_row;
    int returned = 0;
    int least = 0;
    ksc_read(&s_file, ksc_path);
    for (unsigned int at = 0u; at < s_file.lines; at += 1u)
    {
        if (!ksc_row_read(s_file.line[at], "run", &s_row) || (strcmp(s_row.answer, "answers") != 0))
        {
            continue;
        }
        returned = returned || (strcmp(s_row.question, "container returned") == 0);
        least = least || (strncmp(s_row.question, "opening least", 13u) == 0);
    }
    ksc_free(&s_file);
    unsigned int written = 0u;
    if (returned)
    {
        qry_text_printed(head, "toolchain cubin_write\n");
        written += 1u;
    }
    if (least)
    {
        qry_text_printed(head, "header container\n");
        written += 1u;
    }
    snprintf(missing, room, "%s%s%s", returned ? "" : "toolchain", (!returned && !least) ? ", " : "",
             least ? "" : "header");
    return written;
}

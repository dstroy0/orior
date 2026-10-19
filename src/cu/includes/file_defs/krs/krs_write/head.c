// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// head.c: the lines of a SASS ruleset's head, each the part's answer in its .ksc or left out
#include "head.h"

#include "../../ksc/ksc.h"
#include "verba_scribo/verba_scribo.h"

#include <stdio.h>
#include <string.h>

unsigned int krs_head_written(char *out, size_t cap, size_t *at, const char *ksc_path, char *missing, size_t room)
{
    static KscFile s_file;
    static KscRow s_row;
    int returned = 0;
    int least = 0;
    ksc_read(&s_file, ksc_path);
    for (unsigned int line = 0u; line < s_file.lines; line += 1u)
    {
        if (!ksc_row_read(s_file.line[line], "run", &s_row) || (strcmp(s_row.answer, "answers") != 0))
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
        *at = verba_textus.put(&(VerbaTextusCfg){.out = out, .cap = cap, .at = *at, .text = "toolchain cubin_write\n"});
        written += 1u;
    }
    if (least)
    {
        *at = verba_textus.put(&(VerbaTextusCfg){.out = out, .cap = cap, .at = *at, .text = "header container\n"});
        written += 1u;
    }
    snprintf(missing, room, "%s%s%s", returned ? "" : "toolchain", (!returned && !least) ? ", " : "",
             least ? "" : "header");
    return written;
}

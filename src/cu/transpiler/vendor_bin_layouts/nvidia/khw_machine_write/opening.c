// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// opening.c: the place the least opening begins at, read from the part's .ksc
#include "opening.h"

#include "../../../../types/file_defs/ksc/ksc.h"

#include <string.h>

unsigned int khw_opening_read(const char *ksc_path)
{
    static KscFile s_file;
    static KscRow s_row;
    unsigned int from = 0u;
    ksc_read(&s_file, ksc_path);
    for (unsigned int at = 0u; at < s_file.lines; at += 1u)
    {
        if (ksc_row_read(s_file.line[at], "run", &s_row) && (strcmp(s_row.answer, "answers") == 0) &&
            (strncmp(s_row.question, "opening least", 13u) == 0))
        {
            from = (unsigned int)s_row.value;
        }
    }
    ksc_free(&s_file);
    return from;
}

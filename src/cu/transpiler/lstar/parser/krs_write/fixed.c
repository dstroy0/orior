// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// fixed.c: the fixed lines of a SASS ruleset's head, each the part's answer in its .ksc or left out
#include "fixed.h"

#include "../../../../types/file_defs/ksc/ksc.h"

#include <stdio.h>
#include <string.h>

// the four fixed registers the kernel the system accepted leaves at its slot, in the order their lines are written
static const char *const s_fixed_names[] = {"zero", "lane_number", "record", "index"};

#define KRS_FIXED_NAMES ((unsigned int)(sizeof(s_fixed_names) / sizeof(s_fixed_names[0])))

unsigned int krs_fixed_written(QryText *head, const char *ksc_path, char *missing, size_t room)
{
    static KscFile s_file;
    static KscRow s_row;
    int given[KRS_FIXED_NAMES] = {0};
    unsigned long long number[KRS_FIXED_NAMES] = {0ull};
    unsigned long long top = 0ull;
    ksc_read(&s_file, ksc_path);
    for (unsigned int at = 0u; at < s_file.lines; at += 1u)
    {
        unsigned int first = 0u;
        unsigned int bits = 0u;
        if (sscanf(s_file.line[at], "field rd %u %u", &first, &bits) == 2)
        {
            top = (bits >= 64u) ? 0xffffffffffffffffull : ((1ull << bits) - 1ull);
            continue;
        }
        if (!ksc_row_read(s_file.line[at], "run", &s_row) || (strcmp(s_row.answer, "answers") != 0) ||
            (strncmp(s_row.question, "fixed ", 6u) != 0))
        {
            continue;
        }
        for (unsigned int name = 0u; name < KRS_FIXED_NAMES; name += 1u)
        {
            if (strcmp(s_row.question + 6, s_fixed_names[name]) == 0)
            {
                given[name] = 1;
                number[name] = s_row.value;
            }
        }
    }
    ksc_free(&s_file);
    unsigned int written = 0u;
    size_t used = 0u;
    missing[0] = '\0';
    for (unsigned int name = 0u; name < KRS_FIXED_NAMES; name += 1u)
    {
        if (given[name] != 0)
        {
            if ((top != 0ull) && (number[name] == top))
            {
                qry_text_printed(head, "fixed %s = RZ\n", s_fixed_names[name]);
            }
            else
            {
                qry_text_printed(head, "fixed %s = R%llu\n", s_fixed_names[name], number[name]);
            }
            written += 1u;
            continue;
        }
        const int added = snprintf(missing + used, room - used, "%s%s", (used == 0u) ? "" : ", ", s_fixed_names[name]);
        // snprintf counts what it would write, a count no wider than the room; what fits is what is kept
        used += (added > 0) ? (size_t)added : 0u;
        used = (used < room) ? used : (room - 1u);
    }
    return written;
}

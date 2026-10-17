// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// opening.c: the place the least opening begins at and the slot's place, each read from the part's .ksc
#include "opening.h"

#include "../../../../types/file_defs/ksc/ksc.h"

#include <stdio.h>
#include <string.h>

// the word of the row `run answers <k> <question>` of the .ksc at `ksc_path` whose question begins with `question`, or
// `none` where no such row stands or the file is not there
static unsigned int opening_row_read(const char *ksc_path, const char *question, unsigned int none)
{
    static KscFile s_file;
    static KscRow s_row;
    unsigned int word = none;
    ksc_read(&s_file, ksc_path);
    for (unsigned int at = 0u; at < s_file.lines; at += 1u)
    {
        if (ksc_row_read(s_file.line[at], "run", &s_row) && (strcmp(s_row.answer, "answers") == 0) &&
            (strncmp(s_row.question, question, strlen(question)) == 0))
        {
            word = (unsigned int)s_row.value;
        }
    }
    ksc_free(&s_file);
    return word;
}

unsigned int khw_opening_read(const char *ksc_path)
{
    return opening_row_read(ksc_path, "opening least", 0u);
}

unsigned int khw_slot_read(const char *ksc_path)
{
    return opening_row_read(ksc_path, "slot place", KHW_SLOT_NONE);
}

int khw_guard_field_read(const char *ksc_path, unsigned int *first, unsigned int *bits)
{
    static KscFile s_file;
    int found = 0;
    ksc_read(&s_file, ksc_path);
    for (unsigned int at = 0u; at < s_file.lines; at += 1u)
    {
        char name[64];
        unsigned int from = 0u;
        unsigned int width = 0u;
        if ((sscanf(s_file.line[at], "field %63s %u %u", name, &from, &width) == 3) && (strcmp(name, "guard") == 0))
        {
            *first = from;
            *bits = width;
            found = 1;
        }
    }
    ksc_free(&s_file);
    return found;
}

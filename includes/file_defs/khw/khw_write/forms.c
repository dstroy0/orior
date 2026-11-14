// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// forms.c: the forms: a form of the machine file's text listed, the vendor's run, and the forms read
#include "khw_write.h"

// the opening of a form line at `text`, `<operation> <types> 0x<low> 0x<high>`, into its parts, each into its room: 1,
// or 0 where `text` does not open so
static int khw_form_line_read(const char *text, char *operation, size_t operation_room, char *types, size_t types_room,
                              unsigned long long *low, unsigned long long *high)
{
    const char *at = text;
    return line_word(&at, operation, operation_room) && line_word(&at, types, types_room) && line_number(&at, 1, low) &&
           line_number(&at, 1, high);
}

// 1 where the machine file's text, `size` bytes at `bytes`, names the encoding `low` and `high` as one of its forms, a
// line `form <operation> <types> 0x<low> 0x<high> ...`: the forms a word of the web is written by and ranked among
int khw_form_listed(const unsigned char *bytes, unsigned long long size, unsigned long long low,
                    unsigned long long high)
{
    unsigned long long at = 0ull;
    char line[512];
    while (line_read(bytes, size, &at, line, sizeof(line)))
    {
        char operation[64];
        char types[256];
        unsigned long long listed_low = 0ull;
        unsigned long long listed_high = 0ull;
        if ((strncmp(line, "form ", 5u) == 0) &&
            khw_form_line_read(line + 5, operation, sizeof(operation), types, sizeof(types), &listed_low,
                               &listed_high) &&
            (listed_low == low) && (listed_high == high))
        {
            return 1;
        }
    }
    return 0;
}

// The vendor's writer run through the interface in `mode`, passed the part, the file to write, the container layout,
// the vendor's table and the answers file (vendor_writer_run.h), its output passed to the run as KHW_MACHINE_NAME. 1
// where it ended clean
int khw_vendor_run(const char *mode, const char *answers)
{
    char *const command[] = {(char *)s_machine_writer, (char *)s_part,  (char *)s_path, (char *)s_layout,
                             (char *)s_mnemonics,      (char *)answers, (char *)mode,   NULL};
    return vendor_writer_run(command, KHW_MACHINE_NAME, "khw_write", mode);
}

// The forms read from the latest blob of `name` in the run into s_named: the working set the split passes
// retain, a line to a form with its phase and the order its words were read ahead of the relation, or the forms the
// part answered for, a line without those, every form of it classified and read in order. Each form's encoding and the
// runs its operands sit in, the relation and the signedness read past. The count read, 0 where the run contains none
unsigned int khw_forms_read(const char *name)
{
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    if (!qry_member_blob(name, &bytes, &size, NULL))
    {
        return 0u;
    }
    char line[2048];
    unsigned int read = 0u;
    unsigned long long next = 0ull;
    while ((read < KHW_FORMS) && line_read(bytes, size, &next, line, sizeof(line)))
    {
        if ((line[0] == '#') || (line[0] == '\n') || (line[0] == '\0'))
        {
            continue;
        }
        KhwAnswered answered;
        memset(&answered, 0, sizeof(answered));
        char relation[256];
        const char *runs_at = line;
        // a working-set line leads with the phase and the read order; a vendor answers line leads with the relation,
        // and every form it contains is classified and read in the order the host put the words
        if (!line_unsigned(&runs_at, &answered.phase) || !line_signed(&runs_at, &answered.swapped) ||
            !line_word(&runs_at, relation, sizeof(relation)) || !line_signed(&runs_at, &answered.signed_read) ||
            !line_signed(&runs_at, &answered.signedness_queried) || !line_number(&runs_at, 1, &answered.low) ||
            !line_number(&runs_at, 1, &answered.high) || !line_unsigned(&runs_at, &answered.runs))
        {
            answered.phase = KHW_PHASE_CLASSIFIED;
            answered.swapped = 0;
            runs_at = line;
            if (!line_word(&runs_at, relation, sizeof(relation)) || !line_signed(&runs_at, &answered.signed_read) ||
                !line_signed(&runs_at, &answered.signedness_queried) || !line_number(&runs_at, 1, &answered.low) ||
                !line_number(&runs_at, 1, &answered.high) || !line_unsigned(&runs_at, &answered.runs))
            {
                continue;
            }
        }
        // the relation named back to its anchor or its frame relation, for a form read in to write the same relation
        // out: a frame's with its reading after a dot
        answered.anchor = (unsigned int)LADDER_ANCHOR_COUNT;
        answered.frame = (unsigned int)FRAME_RELATION_COUNT;
        const size_t ladder_prefix = strlen(KHW_READS_LADDER);
        const size_t frame_prefix = strlen(FRAME_READS);
        if (strncmp(relation, FRAME_READS, frame_prefix) == 0)
        {
            char *const dot = strchr(relation + frame_prefix, '.');
            answered.reading = (dot != NULL) ? strtoull(dot + 1, NULL, 16) : 0ull;
            if (dot != NULL)
            {
                *dot = '\0';
            }
            for (unsigned int each = 0u; each < (unsigned int)FRAME_RELATION_COUNT; each += 1u)
            {
                answered.frame = (strcmp(s_frame_text[each], relation + frame_prefix) == 0) ? each : answered.frame;
            }
        }
        else
        {
            const char *const name =
                (strncmp(relation, KHW_READS_LADDER, ladder_prefix) == 0) ? (relation + ladder_prefix) : relation;
            for (unsigned int anchor = 0u; anchor < (unsigned int)LADDER_ANCHOR_COUNT; anchor += 1u)
            {
                if (strcmp(s_anchor_text[anchor], name) == 0)
                {
                    answered.anchor = anchor;
                    break;
                }
            }
        }
        if ((answered.runs > KHW_RUNS) || !khw_runs_read(&answered, runs_at))
        {
            continue;
        }
        s_named[read] = answered;
        read += 1u;
    }
    return read;
}

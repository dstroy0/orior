// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// opening.c: the place the least opening begins at and the probe place, each read from the part's .ksc
#include "opening.h"
#include "../../../../src/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"

#include "../../ksc/ksc.h"

// the order of the texts `left` and `right`, each read to its terminator or to `cap`: below 0 where `left`
// comes first, 0 where they are the same, above 0 where `right` does
static inline int opening_text_order(const char *left, const char *right, size_t cap)
{
    const size_t left_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = left, .cap = cap);
    const size_t right_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = right, .cap = cap);
    const size_t shorter = (left_length < right_length) ? left_length : right_length;
    const size_t reach = (shorter < cap) ? (shorter + 1u) : cap;
    const size_t differs =
        EMBED_CALL(cellul.diff, CatenaFinitaCfg, .src = left, .cap = reach, .other = right, .ci = EMBED_FALSE);
    if (differs >= reach)
    {
        return 0;
    }
    // a byte of a text read as unsigned, and its order as a signed count
    return (int)(unsigned char)left[differs] - (int)(unsigned char)right[differs];
}

// the word of the row `run answers <k> <question>` of the .ksc at `ksc_path` whose question begins with `question`, or
// `none` where no such row is or the file is not there
static unsigned int opening_row_read(const char *ksc_path, const char *question, unsigned int none)
{
    static KscFile s_file;
    static KscRow s_row;
    unsigned int word = none;
    ksc_read(&s_file, ksc_path);
    for (unsigned int at = 0u; at < s_file.lines; at += 1u)
    {
        if (ksc_row_read(s_file.line[at], "run", &s_row) &&
            (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = s_row.answer, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = "answers", .ci = EMBED_FALSE)) &&
            (opening_text_order(s_row.question, question,
                                (size_t)(EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = question,
                                                    .cap = PLATFORM_TEXT_TO_TERMINATOR))) == 0))
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

unsigned int khw_probe_place_read(const char *ksc_path)
{
    return opening_row_read(ksc_path, "probe place", KHW_PROBE_PLACE_NONE);
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
        if (ksc_field_read(s_file.line[at], name, sizeof(name), &from, &width) &&
            (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "guard",
                        .ci = EMBED_FALSE)))
        {
            *first = from;
            *bits = width;
            found = 1;
        }
    }
    ksc_free(&s_file);
    return found;
}

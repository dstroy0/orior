// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// fixed.c: the fixed lines of a SASS ruleset's head, each the part's answer in its .ksc, the line the ruleset gave
// before, or left out
#include "fixed.h"

#include "../../../parsers/line_read.h"
#include "../../krf/krf.h"
#include "../../ksc/ksc.h"
#include "verba_scribo/verba_scribo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the four fixed registers the kernel the system accepted leaves at its slot, in the order their lines are written
static const char *const s_fixed_names[] = {"zero", "lane_number", "record", "index"};

#define KRS_FIXED_NAMES ((unsigned int)(sizeof(s_fixed_names) / sizeof(s_fixed_names[0])))

// the most fixed lines a ruleset gives, and the longest
#define KRS_FIXED_GIVEN_MOST 64u

#define KRS_FIXED_LINE 256u

// a fixed line the ruleset gave before this run: its register's name and the line whole
typedef struct
{
    char name[64];
    char line[KRS_FIXED_LINE];
} KrsFixedGiven;

// every `fixed <name> = ...` line the ruleset at `krs_path` gave, into `given`, at most KRS_FIXED_GIVEN_MOST; the
// count, 0 where there is no path or no file
static unsigned int fixed_given_read(const char *krs_path, KrsFixedGiven *given)
{
    const unsigned char *file_bytes = NULL;
    unsigned long long file_size = 0ull;
    unsigned long long file_at = 0ull;
    unsigned int count = 0u;
    if ((krs_path == NULL) || !qry_member_file_blob(krs_path, &file_bytes, &file_size))
    {
        return 0u;
    }
    char line[KRS_FIXED_LINE];
    while (line_read(file_bytes, file_size, &file_at, line, sizeof(line)) && (count < KRS_FIXED_GIVEN_MOST))
    {
        char kind[64];
        char name[64];
        const char *at = line;
        if (!line_word(&at, kind, sizeof(kind)) || !line_word(&at, name, sizeof(name)) || (strcmp(kind, "fixed") != 0))
        {
            continue;
        }
        line[strcspn(line, "\r\n")] = '\0';
        snprintf(given[count].name, sizeof(given[count].name), "%s", name);
        snprintf(given[count].line, sizeof(given[count].line), "%s", line);
        count += 1u;
    }
    return count;
}

// the line the ruleset gave `name` before this run, of the `count` in `given`, or NULL where it gave none
static const char *fixed_given_of(const KrsFixedGiven *given, unsigned int count, const char *name)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (strcmp(given[at].name, name) == 0)
        {
            return given[at].line;
        }
    }
    return NULL;
}

// 1 where `name` is one of the four the kernel's slot answers for
static int fixed_asked(const char *name)
{
    for (unsigned int at = 0u; at < KRS_FIXED_NAMES; at += 1u)
    {
        if (strcmp(s_fixed_names[at], name) == 0)
        {
            return 1;
        }
    }
    return 0;
}

unsigned int krs_fixed_written(char *out, size_t cap, size_t *at, const char *ksc_path, const char *krs_path,
                               char *missing, size_t room)
{
    static KscFile s_file;
    static KscRow s_row;
    static KrsFixedGiven s_given[KRS_FIXED_GIVEN_MOST];
    int answered[KRS_FIXED_NAMES] = {0};
    unsigned long long number[KRS_FIXED_NAMES] = {0ull};
    unsigned long long top = 0ull;
    ksc_read(&s_file, ksc_path);
    for (unsigned int line = 0u; line < s_file.lines; line += 1u)
    {
        unsigned long long first = 0ull;
        unsigned long long bits = 0ull;
        const char *read = s_file.line[line];
        if (line_letters(&read, "field") && line_letters(&read, "rd ") && line_number(&read, 0, &first) &&
            line_number(&read, 0, &bits))
        {
            top = (bits >= 64u) ? 0xffffffffffffffffull : ((1ull << bits) - 1ull);
            continue;
        }
        if (!ksc_row_read(s_file.line[line], "run", &s_row) || (strcmp(s_row.answer, "answers") != 0) ||
            (strncmp(s_row.question, "fixed ", 6u) != 0))
        {
            continue;
        }
        for (unsigned int name = 0u; name < KRS_FIXED_NAMES; name += 1u)
        {
            if (strcmp(s_row.question + 6, s_fixed_names[name]) == 0)
            {
                answered[name] = 1;
                number[name] = s_row.value;
            }
        }
    }
    ksc_free(&s_file);
    const unsigned int givens = fixed_given_read(krs_path, s_given);
    unsigned int written = 0u;
    size_t used = 0u;
    missing[0] = '\0';
    // the given lines a row replaces with another text, kept in the .krf beside the .ksc
    const char *removed[KRS_FIXED_NAMES];
    unsigned int removals = 0u;
    for (unsigned int name = 0u; name < KRS_FIXED_NAMES; name += 1u)
    {
        if (answered[name] != 0)
        {
            char line[KRS_FIXED_LINE];
            if ((top != 0ull) && (number[name] == top))
            {
                snprintf(line, sizeof(line), "fixed %s = RZ", s_fixed_names[name]);
            }
            else
            {
                snprintf(line, sizeof(line), "fixed %s = R%llu", s_fixed_names[name], number[name]);
            }
            *at = verba_textus.put(&(VerbaTextusCfg){.out = out, .cap = cap, .at = *at, .text = line});
            *at = verba_littera.ch(&(VerbaLitteraCfg){.out = out, .cap = cap, .at = *at, .ch = '\n'});
            written += 1u;
            const char *const before = fixed_given_of(s_given, givens, s_fixed_names[name]);
            if ((before != NULL) && (strcmp(before, line) != 0))
            {
                removed[removals] = before;
                removals += 1u;
            }
            continue;
        }
        const char *const kept = fixed_given_of(s_given, givens, s_fixed_names[name]);
        if (kept != NULL)
        {
            *at = verba_textus.put(&(VerbaTextusCfg){.out = out, .cap = cap, .at = *at, .text = kept});
            *at = verba_littera.ch(&(VerbaLitteraCfg){.out = out, .cap = cap, .at = *at, .ch = '\n'});
            written += 1u;
            continue;
        }
        const int added = snprintf(missing + used, room - used, "%s%s", (used == 0u) ? "" : ", ", s_fixed_names[name]);
        // snprintf counts what it would write, a count no wider than the room; what fits is what is kept
        used += (added > 0) ? (size_t)added : 0u;
        used = (used < room) ? used : (room - 1u);
    }
    for (unsigned int given = 0u; given < givens; given += 1u)
    {
        if (fixed_asked(s_given[given].name))
        {
            continue;
        }
        *at = verba_textus.put(&(VerbaTextusCfg){.out = out, .cap = cap, .at = *at, .text = s_given[given].line});
        *at = verba_littera.ch(&(VerbaLitteraCfg){.out = out, .cap = cap, .at = *at, .ch = '\n'});
        written += 1u;
    }
    if (removals != 0u)
    {
        char krf[1200];
        krf_path_beside(ksc_path, krf, sizeof(krf));
        if (!krf_removed(krf, "krs", "replaced", removed, removals))
        {
            printf("  orior_krs_write: %u fixed lines a row replaced could not be kept in %s\n", removals, krf);
        }
    }
    return written;
}

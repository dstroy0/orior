// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_hits.c: the lines of one file that hold a search's query, the first place on each line it stands, at most
// CLI_HITS_MOST of them, and the text of a found line as a hit keeps it
#include "../text/json_tree.h"
#include "../text/regexp.h"
#include "../text/sift.h"
#include "files_internal.h"

unsigned long long cli_hit_cut(const unsigned char *text, unsigned long long size)
{
    // where the last letter that is not whitespace ends, and where the last letter kept ends
    unsigned long long trimmed = 0ull;
    unsigned long long most = size;
    unsigned int letters = 0u;
    unsigned long long at = 0ull;
    while (at < size)
    {
        unsigned int letter = 0u;
        at += cli_lossy_letter(text, size, at, &letter);
        letters += (letters < CLI_HIT_TEXT) ? 1u : 0u;
        most = ((letters == CLI_HIT_TEXT) && (most == size)) ? at : most;
        trimmed = cli_fuzzy_white(letter) ? trimmed : at;
    }
    return (trimmed < most) ? trimmed : most;
}

int cli_hit_wordy(unsigned int letter)
{
    return (letter == '_') || cli_regexp_property(CLI_REGEXP_LETTER, letter) ||
           cli_regexp_property(CLI_REGEXP_NUMBER, letter);
}

// The sift's places in one file as they are read: the file and what is looked for, where the hits go, the line the
// last place stood on, the byte its newlines were counted to, the line of the last hit, and the hits so far
typedef struct
{
    const unsigned char *bytes;
    unsigned long long size;
    const CliSought *sought;
    CliHitPut put;
    void *context;
    unsigned long long line;
    unsigned long long counted;
    unsigned long long last_line;
    unsigned int found;
} CliHitting;

// a place the sift found the query: a hit where it is the first on its line, a whole word where one is asked for
static void cli_hit_at(void *context, unsigned long long at)
{
    CliHitting *const hitting = (CliHitting *)context;
    const unsigned char *const bytes = hitting->bytes;
    if (hitting->found >= CLI_HITS_MOST)
    {
        return;
    }
    for (unsigned long long place = hitting->counted; place < at; place += 1ull)
    {
        hitting->line += (bytes[place] == '\n') ? 1ull : 0ull;
    }
    hitting->counted = at;
    if (hitting->line == hitting->last_line)
    {
        return;
    }
    unsigned long long start = at;
    while ((start > 0ull) && (bytes[start - 1ull] != '\n'))
    {
        start -= 1ull;
    }
    const unsigned char *const newline =
        memor.chr(&(MemoriaCfg){.src = bytes + at, .val = '\n', .bytes = hitting->size - at});
    // a pointer found in the bytes is at or past their start
    const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - bytes) : hitting->size;
    // the letters before the query on its line, and the last of them
    unsigned long long before = 0ull;
    unsigned int last = 0u;
    for (unsigned long long place = start; place < at; before += 1ull)
    {
        place += cli_lossy_letter(bytes, at, place, &last);
    }
    if (hitting->sought->how.word)
    {
        const unsigned long long after = at + hitting->sought->length;
        unsigned int next = 0u;
        const int has_next = after < end;
        if (has_next)
        {
            cli_lossy_letter(bytes, end, after, &next);
        }
        if (((before != 0ull) && cli_hit_wordy(last)) || (has_next && cli_hit_wordy(next)))
        {
            return;
        }
    }
    hitting->last_line = hitting->line;
    hitting->put(hitting->context, hitting->line, before + 1ull, bytes + start,
                 cli_hit_cut(bytes + start, end - start));
    hitting->found += 1u;
}

// whether the letters of `line`, `size` bytes, from `at`, read in lower case, are the query's, the byte after them
// into `after`
static int cli_lower_at(const unsigned char *line, unsigned long long size, unsigned long long at,
                        const CliSought *sought, unsigned long long *after)
{
    for (unsigned int index = 0u; index < sought->lower_count; index += 1u)
    {
        if (at >= size)
        {
            return 0;
        }
        unsigned int letter = 0u;
        at += cli_lossy_letter(line, size, at, &letter);
        if (cli_regexp_lower(letter) != sought->lower[index])
        {
            return 0;
        }
    }
    *after = at;
    return 1;
}

// A line's letters as they are stepped over: the byte, the letter's place, and the letter before, read in lower case
typedef struct
{
    unsigned long long at;
    unsigned long long letter_at;
    unsigned int last;
} CliStep;

static void cli_stepped(CliStep *step, const unsigned char *line, unsigned long long size)
{
    unsigned int letter = 0u;
    step->at += cli_lossy_letter(line, size, step->at, &letter);
    step->letter_at += 1ull;
    step->last = cli_regexp_lower(letter);
}

// the first place on the line the query stands in its lower case, a whole word where one is asked for: 1 with its
// letter's place into `col`, or 0
static int cli_line_holds(const unsigned char *line, unsigned long long size, const CliSought *sought,
                          unsigned long long *col)
{
    CliStep step = {0ull, 0ull, 0u};
    while (step.at < size)
    {
        unsigned long long after = 0ull;
        if (!cli_lower_at(line, size, step.at, sought, &after))
        {
            cli_stepped(&step, line, size);
            continue;
        }
        unsigned int next = 0u;
        const int has_next = after < size;
        if (has_next)
        {
            cli_lossy_letter(line, size, after, &next);
        }
        const int joined = ((step.letter_at != 0ull) && cli_hit_wordy(step.last)) ||
                           (has_next && cli_hit_wordy(cli_regexp_lower(next)));
        if (!sought->how.word || !joined)
        {
            *col = step.letter_at + 1ull;
            return 1;
        }
        // a match that is no whole word is passed over whole
        for (unsigned int index = 0u; index < sought->lower_count; index += 1u)
        {
            cli_stepped(&step, line, size);
        }
    }
    return 0;
}

void cli_files_hits_in(const unsigned char *bytes, unsigned long long size, const CliSought *sought, CliHitPut put,
                       void *context)
{
    if (sought->how.match_case || sought->ascii)
    {
        CliHitting hitting = {bytes, size, sought, put, context, 1ull, 0ull, 0ull, 0u};
        cli_sift_find_all(bytes, size, (const unsigned char *)sought->query, sought->length, !sought->how.match_case,
                          cli_hit_at, &hitting);
        return;
    }
    // each line, less a carriage return before its newline
    unsigned long long start = 0ull;
    unsigned long long index = 0ull;
    unsigned int found = 0u;
    while ((start < size) && (found < CLI_HITS_MOST))
    {
        const unsigned char *const newline =
            memor.chr(&(MemoriaCfg){.src = bytes + start, .val = '\n', .bytes = size - start});
        // a pointer found in the bytes is at or past their start
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - bytes) : size;
        unsigned long long length = end - start;
        if ((newline != NULL) && (length > 0ull) && (bytes[end - 1ull] == '\r'))
        {
            length -= 1ull;
        }
        index += 1ull;
        unsigned long long col = 0ull;
        if (cli_line_holds(bytes + start, length, sought, &col))
        {
            put(context, index, col, bytes + start, cli_hit_cut(bytes + start, length));
            found += 1u;
        }
        start = end + 1ull;
    }
}

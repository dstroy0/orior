// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_fuzzy.c: the match of a query's letters in a text, in order anywhere in it, case aside, in no more pieces than
// half its letters or three, a letter scoring more at the start of a word or past where the text's name starts. The
// palette's own matcher, fuzzy.js, scores the same
#include "../text/regexp.h"
#include "files_internal.h"

// whether a letter is whitespace
int cli_fuzzy_white(unsigned int letter)
{
    return (letter == ' ') || ((letter >= '\t') && (letter <= '\r')) || (letter == 0x85u) || (letter == 0xA0u) ||
           (letter == 0x1680u) || ((letter >= 0x2000u) && (letter <= 0x200Au)) || (letter == 0x2028u) ||
           (letter == 0x2029u) || (letter == 0x202Fu) || (letter == 0x205Fu) || (letter == 0x3000u);
}

// whether a letter splits a text into words: whitespace, or of the bytes of a path all ASCII a space or a tab, and a
// slash, a separator in a name, a dot, a colon or an at; and whether it is upper case
static int cli_splits(unsigned int letter, int bytes)
{
    const int white = bytes ? ((letter == ' ') || (letter == '\t')) : cli_fuzzy_white(letter);
    return white || (letter == '/') || (letter == '\\') || (letter == '_') || (letter == '-') || (letter == '.') ||
           (letter == ':') || (letter == '@');
}

static int cli_upper(unsigned int letter, int bytes)
{
    return bytes ? ((letter >= 'A') && (letter <= 'Z')) : cli_regexp_property(CLI_REGEXP_UPPER, letter);
}

int cli_fuzzy_starts_word(const unsigned int *text, unsigned int at, int bytes)
{
    return (at == 0u) || cli_splits(text[at - 1u], bytes) ||
           (!cli_upper(text[at - 1u], bytes) && cli_upper(text[at], bytes));
}

int cli_fuzzy_letters(const unsigned int *wanted, unsigned int wanted_count, const unsigned int *shown,
                      const unsigned int *lower, unsigned int count, unsigned int from, int bytes, unsigned int *latest,
                      double *score, unsigned int *hits, unsigned int *hit_count)
{
    *score = 0.0;
    *hit_count = 0u;
    if (wanted_count == 0u)
    {
        return 1;
    }
    if (wanted_count > CLI_FUZZY_LETTERS)
    {
        return 0;
    }
    // the latest place each letter can stand with the letters after it still fitting
    unsigned int at = count;
    for (unsigned int index = wanted_count; index > 0u; index -= 1u)
    {
        while ((at > 0u) && (lower[at - 1u] != wanted[index - 1u]))
        {
            at -= 1u;
        }
        if (at == 0u)
        {
            return 0;
        }
        at -= 1u;
        latest[index - 1u] = at;
    }
    int has_last = 0;
    unsigned int last = 0u;
    for (unsigned int index = 0u; index < wanted_count; index += 1u)
    {
        const unsigned int letter = wanted[index];
        const unsigned int after = has_last ? (last + 1u) : 0u;
        // the letter right after the one before it, else the first that starts a word, else the first
        int has_found = has_last && (after < count) && (lower[after] == letter);
        unsigned int found = after;
        int has_first = 0;
        unsigned int first = 0u;
        for (unsigned int place = after; !has_found && (place <= latest[index]); place += 1u)
        {
            if (lower[place] == letter)
            {
                first = has_first ? first : place;
                has_first = 1;
                if (cli_fuzzy_starts_word(shown, place, bytes))
                {
                    found = place;
                    has_found = 1;
                }
            }
        }
        if (!has_found && !has_first)
        {
            return 0;
        }
        found = has_found ? found : first;
        *score += 1.0;
        *score += (has_last && (found == (last + 1u))) ? 5.0 : 0.0;
        *score += cli_fuzzy_starts_word(shown, found, bytes) ? 8.0 : 0.0;
        *score += (found >= from) ? 3.0 : 0.0;
        *score += (shown[found] == letter) ? 1.0 : 0.0;
        hits[index] = found;
        last = found;
        has_last = 1;
    }
    // a query torn into more pieces than half its letters reads as no match
    unsigned int runs = 0u;
    for (unsigned int index = 0u; index < wanted_count; index += 1u)
    {
        runs += ((index == 0u) || (hits[index] != (hits[index - 1u] + 1u))) ? 1u : 0u;
    }
    const unsigned int half = (wanted_count + 1u) / 2u;
    if (runs > ((half > 3u) ? half : 3u))
    {
        return 0;
    }
    *score -= ((double)(hits[wanted_count - 1u] - hits[0]) * 0.1) + ((double)count * 0.02);
    *hit_count = wanted_count;
    return 1;
}

unsigned int cli_files_wanted(const char *query, unsigned int *letters, unsigned int room)
{
    static unsigned int s_read[CLI_FUZZY_LETTERS];
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    const unsigned long long count = cli_regexp_letters(
        query, cellul.len(&(CatenaFinitaCfg){.src = query, .cap = CLI_CELLS_POOL}), s_read, CLI_FUZZY_LETTERS);
    unsigned int kept = 0u;
    for (unsigned long long at = 0ull; (at < count) && (kept < room); at += 1ull)
    {
        if (!cli_fuzzy_white(s_read[at]))
        {
            letters[kept] = cli_regexp_lower(s_read[at]);
            kept += 1u;
        }
    }
    cli_lock_give(&s_lock);
    return kept;
}

unsigned int cli_files_letters(const char *text, unsigned int *shown, unsigned int *lower, unsigned int room)
{
    // a letter count is below `room`, which an unsigned int holds
    const unsigned int count = (unsigned int)cli_regexp_letters(
        text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), shown, room);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        lower[at] = cli_regexp_lower(shown[at]);
    }
    return count;
}

int cli_files_fuzzy_held(const unsigned int *wanted, unsigned int wanted_count, const unsigned int *shown,
                         const unsigned int *lower, unsigned int count, unsigned int *latest, double *score,
                         unsigned int *hits, unsigned int *hit_count)
{
    return cli_fuzzy_letters(wanted, wanted_count, shown, lower, count, 0u, 0, latest, score, hits, hit_count);
}

int cli_files_fuzzy(const char *query, const char *text, unsigned long long from, double *score, unsigned int *hits,
                    unsigned int *count)
{
    static unsigned int s_wanted[CLI_FUZZY_LETTERS];
    static unsigned int s_shown[CLI_FUZZY_LETTERS];
    static unsigned int s_lower[CLI_FUZZY_LETTERS];
    static unsigned int s_latest[CLI_FUZZY_LETTERS];
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    const unsigned int wanted = cli_files_wanted(query, s_wanted, CLI_FUZZY_LETTERS);
    const unsigned int letters = cli_files_letters(text, s_shown, s_lower, CLI_FUZZY_LETTERS);
    // a place in letters is below CLI_FUZZY_LETTERS, which an unsigned int holds
    const unsigned int start = (from < CLI_FUZZY_LETTERS) ? (unsigned int)from : CLI_FUZZY_LETTERS;
    const int matched =
        cli_fuzzy_letters(s_wanted, wanted, s_shown, s_lower, letters, start, 0, s_latest, score, hits, count);
    cli_lock_give(&s_lock);
    return matched;
}

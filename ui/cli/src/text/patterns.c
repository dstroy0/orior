// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// patterns.c: the patterns of a part read from their lines, held against a path, written as git's pathspecs, and kept
// for each part of the window
#include "patterns.h"
#include "regexp.h"

// the patterns of each part, 1 in `s_made` where they were set, and the lock that is held while one is read or set
static CliPatterns s_set[CLI_PARTS];
static int s_made[CLI_PARTS];
static CliLock s_lock = CLI_LOCK_INIT;

// A letter as a pattern compares it: whatever its case on Windows, as it is elsewhere
static unsigned int cli_fold(unsigned int letter)
{
#if defined(_WIN32)
    return cli_regexp_lower(letter);
#else
    return letter;
#endif
}

// whether a byte is one of ASCII's spaces
static int cli_is_space(char letter)
{
    return (letter == ' ') || ((letter >= '\t') && (letter <= '\r'));
}

// whether the glob names the path from its first letter to its last
static int cli_glob(const unsigned int *pattern, unsigned long long pattern_length, const unsigned int *path,
                    unsigned long long length)
{
    if (pattern_length == 0ull)
    {
        return length == 0ull;
    }
    if ((pattern_length >= 3ull) && (pattern[0] == '*') && (pattern[1] == '*') && (pattern[2] == '/'))
    {
        if (cli_glob(pattern + 3u, pattern_length - 3ull, path, length))
        {
            return 1;
        }
        for (unsigned long long at = 0ull; at < length; at += 1ull)
        {
            if ((path[at] == '/') && cli_glob(pattern + 3u, pattern_length - 3ull, path + at + 1u, length - at - 1ull))
            {
                return 1;
            }
        }
        return 0;
    }
    if ((pattern_length >= 2ull) && (pattern[0] == '*') && (pattern[1] == '*'))
    {
        for (unsigned long long at = 0ull; at <= length; at += 1ull)
        {
            if (cli_glob(pattern + 2u, pattern_length - 2ull, path + at, length - at))
            {
                return 1;
            }
        }
        return 0;
    }
    if (pattern[0] == '*')
    {
        unsigned long long name = 0ull;
        while ((name < length) && (path[name] != '/'))
        {
            name += 1ull;
        }
        for (unsigned long long at = 0ull; at <= name; at += 1ull)
        {
            if (cli_glob(pattern + 1u, pattern_length - 1ull, path + at, length - at))
            {
                return 1;
            }
        }
        return 0;
    }
    if (pattern[0] == '?')
    {
        return (length != 0ull) && (path[0] != '/') &&
               cli_glob(pattern + 1u, pattern_length - 1ull, path + 1u, length - 1ull);
    }
    return (length != 0ull) && (path[0] == pattern[0]) &&
           cli_glob(pattern + 1u, pattern_length - 1ull, path + 1u, length - 1ull);
}

// whether the pattern names the path, or a folder it is in; the path's letters folded as a pattern compares them
static int cli_hits(const CliPatterns *patterns, const CliPattern *pattern, const unsigned int *path,
                    unsigned long long length, int dir)
{
    const unsigned int *const glob = patterns->letters + pattern->first;
    for (unsigned long long end = 0ull; end < length; end += 1ull)
    {
        if ((path[end] == '/') && cli_glob(glob, pattern->length, path, end))
        {
            return 1;
        }
    }
    return (dir || !pattern->folders) && cli_glob(glob, pattern->length, path, length);
}

// One pattern's line read into the hiding or the keeping ones: trimmed, `./` before it left off, a trailing `/`
// naming folders only, and `**/` put before one that has no `/` of its own. An empty one names nothing
static void cli_pattern_read(CliPatterns *patterns, const char *from, const char *to, int keep)
{
    while ((from < to) && cli_is_space(from[0]))
    {
        from += 1;
    }
    while ((to > from) && cli_is_space(to[-1]))
    {
        to -= 1;
    }
    from += (((to - from) >= 2) && (from[0] == '.') && (from[1] == '/')) ? 2 : 0;
    const int folders = (to > from) && (to[-1] == '/');
    while ((to > from) && (to[-1] == '/'))
    {
        to -= 1;
    }
    if (from == to)
    {
        return;
    }
    int slashed = 0;
    for (const char *at = from; at < to; at += 1)
    {
        slashed = slashed || (at[0] == '/');
    }
    while (slashed && (from < to) && (from[0] == '/'))
    {
        from += 1;
    }
    unsigned int *const count = keep ? &patterns->keeps : &patterns->hides;
    CliPattern *const list = keep ? patterns->keep : patterns->hide;
    const unsigned int first = patterns->used;
    const unsigned long long room = CLI_PATTERN_LETTERS - first;
    unsigned long long length = 0ull;
    static const unsigned int s_anywhere[3] = {'*', '*', '/'};
    if (!slashed && (room >= 3ull))
    {
        memor.cpy(&(MemoriaCfg){.dst = patterns->letters + first, .src = s_anywhere, .bytes = sizeof(s_anywhere)});
        length = 3ull;
    }
    length +=
        cli_regexp_letters(from, (unsigned long long)(to - from), patterns->letters + first + length, room - length);
    if ((*count >= CLI_PATTERNS_MOST) || (room < 3ull))
    {
        return;
    }
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        patterns->letters[first + at] = cli_fold(patterns->letters[first + at]);
    }
    // the length is below CLI_PATTERN_LETTERS, which an unsigned int counts
    const CliPattern pattern = {first, (unsigned int)length, folders};
    list[*count] = pattern;
    *count += 1u;
    patterns->used = first + (unsigned int)length;
}

// one line of the patterns read: trimmed, nothing where it is empty or starts with `#`, a keeping pattern where it
// starts with `!`
static void cli_line_read(CliPatterns *patterns, const char *line, unsigned long long size)
{
    const char *from = line;
    const char *to = line + size;
    while ((from < to) && cli_is_space(from[0]))
    {
        from += 1;
    }
    while ((to > from) && cli_is_space(to[-1]))
    {
        to -= 1;
    }
    if ((from == to) || (from[0] == '#'))
    {
        return;
    }
    const int keep = from[0] == '!';
    cli_pattern_read(patterns, from + (keep ? 1 : 0), to, keep);
}

void cli_patterns_read(const char *const *lines, unsigned int count, CliPatterns *patterns)
{
    patterns->hides = 0u;
    patterns->keeps = 0u;
    patterns->used = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_read(patterns, lines[at], cellul.len(&(CatenaFinitaCfg){.src = lines[at], .cap = CLI_CELLS_POOL}));
    }
}

void cli_patterns_read_set(const char *const *lines, unsigned int count, CliPatterns *patterns)
{
    patterns->hides = 0u;
    patterns->keeps = 0u;
    patterns->used = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *from = lines[at];
        const char *to = from + cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL});
        while ((from < to) && cli_is_space(from[0]))
        {
            from += 1;
        }
        while ((to > from) && cli_is_space(to[-1]))
        {
            to -= 1;
        }
        if ((from == to) || (from[0] == '#'))
        {
            continue;
        }
        // each line turned about: one that takes out is a pattern that hides, and one that names keeps
        char flipped[CLI_PATH_LONGEST];
        CliLine line = {flipped, sizeof(flipped), 0ull};
        if (from[0] != '!')
        {
            cli_line_character(&line, '!');
        }
        cli_line_counted(&line, from + ((from[0] == '!') ? 1 : 0),
                         (unsigned long long)(to - from) - ((from[0] == '!') ? 1ull : 0ull));
        const unsigned long long length = cli_line_fits(&line) ? line.at : 0ull;
        cli_line_closed(&line);
        cli_line_read(patterns, flipped, length);
    }
}

int cli_patterns_empty(const CliPatterns *patterns)
{
    return (patterns->hides == 0u) && (patterns->keeps == 0u);
}

int cli_patterns_keeps_only(const CliPatterns *patterns)
{
    return patterns->keeps != 0u;
}

// the path's letters, each folded as a pattern compares it, into `letters`, which holds CLI_PATH_LONGEST: their count
static unsigned long long cli_path_letters(const char *path, unsigned int *letters)
{
    const unsigned long long length = cli_regexp_letters(
        path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), letters, CLI_PATH_LONGEST);
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        letters[at] = cli_fold(letters[at]);
    }
    return length;
}

int cli_patterns_hides(const CliPatterns *patterns, const char *path, int dir)
{
    unsigned int letters[CLI_PATH_LONGEST];
    const unsigned long long length = cli_path_letters(path, letters);
    for (unsigned int at = 0u; at < patterns->hides; at += 1u)
    {
        if (cli_hits(patterns, &patterns->hide[at], letters, length, dir))
        {
            return 1;
        }
    }
    if (dir || (patterns->keeps == 0u))
    {
        return 0;
    }
    for (unsigned int at = 0u; at < patterns->keeps; at += 1u)
    {
        if (cli_hits(patterns, &patterns->keep[at], letters, length, 0))
        {
            return 0;
        }
    }
    return 1;
}

int cli_patterns_keeps_folder(const CliPatterns *patterns, const char *path)
{
    unsigned int letters[CLI_PATH_LONGEST];
    const unsigned long long length = cli_path_letters(path, letters);
    for (unsigned int at = 0u; at < patterns->keeps; at += 1u)
    {
        if (cli_hits(patterns, &patterns->keep[at], letters, length, 1))
        {
            return 1;
        }
    }
    return 0;
}

// one pattern as git's pathspecs: the path itself and everything in it, under `magic`
static void cli_pattern_specs(const CliPatterns *patterns, const CliPattern *pattern, const char *magic,
                              CliWords *words)
{
#if defined(_WIN32)
    const char *const letter_case = ",icase";
#else
    const char *const letter_case = "";
#endif
    char spec[CLI_PATH_LONGEST];
    for (int inside = pattern->folders ? 1 : 0; inside < 2; inside += 1)
    {
        CliLine line = {spec, sizeof(spec), 0ull};
        cli_line_text(&line, ":(");
        cli_line_text(&line, magic);
        cli_line_text(&line, "glob");
        cli_line_text(&line, letter_case);
        cli_line_character(&line, ')');
        for (unsigned int at = 0u; at < pattern->length; at += 1u)
        {
            cli_utf8_put(&line, patterns->letters[pattern->first + at]);
        }
        if (inside)
        {
            cli_line_text(&line, "/**");
        }
        words->full = words->full || !cli_line_fits(&line);
        cli_line_closed(&line);
        cli_words_put(words, spec);
    }
}

void cli_patterns_kept_specs(const CliPatterns *patterns, CliWords *words)
{
    for (unsigned int at = 0u; at < patterns->keeps; at += 1u)
    {
        cli_pattern_specs(patterns, &patterns->keep[at], "", words);
    }
}

void cli_patterns_hidden_specs(const CliPatterns *patterns, CliWords *words)
{
    for (unsigned int at = 0u; at < patterns->hides; at += 1u)
    {
        cli_pattern_specs(patterns, &patterns->hide[at], "exclude,", words);
    }
}

void cli_patterns_pathspecs(const CliPatterns *patterns, CliWords *words)
{
    if (patterns->keeps == 0u)
    {
        cli_words_put(words, ".");
    }
    cli_patterns_kept_specs(patterns, words);
    cli_patterns_hidden_specs(patterns, words);
}

// whether two lists of patterns name the same, pattern by pattern
static int cli_same_list(const CliPatterns *one, const CliPattern *list, const CliPatterns *other,
                         const CliPattern *other_list, unsigned int count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if ((list[at].folders != other_list[at].folders) || (list[at].length != other_list[at].length) ||
            (memor.cmp(&(MemoriaCfg){.src = one->letters + list[at].first,
                                     .other = other->letters + other_list[at].first,
                                     .bytes = list[at].length * sizeof(unsigned int)}) != 0))
        {
            return 0;
        }
    }
    return 1;
}

int cli_patterns_set(unsigned int part, const char *const *lines, unsigned int count)
{
    static CliPatterns s_read;
    cli_lock_take(&s_lock);
    cli_patterns_read(lines, count, &s_read);
    CliPatterns *const kept = &s_set[part];
    const int same = s_made[part] && (kept->hides == s_read.hides) && (kept->keeps == s_read.keeps) &&
                     cli_same_list(kept, kept->hide, &s_read, s_read.hide, s_read.hides) &&
                     cli_same_list(kept, kept->keep, &s_read, s_read.keep, s_read.keeps);
    memor.cpy(&(MemoriaCfg){.dst = kept, .src = &s_read, .bytes = sizeof(s_read)});
    s_made[part] = 1;
    cli_lock_give(&s_lock);
    return !same;
}

void cli_patterns_of(unsigned int part, CliPatterns *patterns)
{
    cli_lock_take(&s_lock);
    if (s_made[part])
    {
        memor.cpy(&(MemoriaCfg){.dst = patterns, .src = &s_set[part], .bytes = sizeof(*patterns)});
    }
    else
    {
        patterns->hides = 0u;
        patterns->keeps = 0u;
        patterns->used = 0u;
    }
    cli_lock_give(&s_lock);
}

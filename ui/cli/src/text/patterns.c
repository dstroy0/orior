// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// patterns.c: the patterns of a part read from their lines, matched against a path, written as git's pathspecs, and
// stored for each part of the window
#include "patterns.h"
#include "regexp.h"
#include "text.h"

// the patterns of each part, 1 in `s_made` where they were set, and the lock that is acquired while one is read or set
static CliPatterns s_set[CLI_PARTS];
static int s_made[CLI_PARTS];

typedef struct
{
    unsigned int letter;
    char letter_letter;
    const unsigned int *pattern_u32s;
    unsigned long long pattern_length;
    const unsigned int *path;
    unsigned long long length;
    const CliPatterns *patterns;
    const CliPattern *pattern;
    int dir;
    CliPatterns *patterns_out;
    const char *from;
    const char *to;
    int include;
    const char *line;
    unsigned long long size;
    const char *path_text;
    unsigned int *letters;
    const char *magic;
    CliWords *words;
    const CliPatterns *one;
    const CliPattern *list;
    const CliPatterns *other;
    const CliPattern *other_list;
    unsigned int count;
} PatternsCtx;
// A letter as a pattern compares it: whatever its case on Windows, as it is elsewhere
EMBED_INLINE unsigned int cli_fold(const PatternsCtx *args)
{
    unsigned int letter = args->letter;
#if defined(_WIN32)
    return EMBED_CALL(cli_regexp_letter.lower, CliRegexpLetterCfg, .letter = letter);
#else
    return letter;
#endif
}

// whether a byte is one of ASCII's spaces
EMBED_INLINE int cli_is_space(const PatternsCtx *args)
{
    return (args->letter_letter == ' ') || ((args->letter_letter >= '\t') && (args->letter_letter <= '\r'));
}

// whether the glob names the path from its first letter to its last
static int cli_glob(const PatternsCtx *args)
{
    if (args->pattern_length == 0ull)
    {
        return args->length == 0ull;
    }
    if ((args->pattern_length >= 3ull) && (args->pattern_u32s[0] == '*') && (args->pattern_u32s[1] == '*') &&
        (args->pattern_u32s[2] == '/'))
    {
        if (EMBED_CALL(cli_glob, PatternsCtx, .pattern_u32s = args->pattern_u32s + 3u,
                       .pattern_length = args->pattern_length - 3ull, .path = args->path, .length = args->length))
        {
            return 1;
        }
        for (unsigned long long at = 0ull; at < args->length; at += 1ull)
        {
            if ((args->path[at] == '/') && EMBED_CALL(cli_glob, PatternsCtx, .pattern_u32s = args->pattern_u32s + 3u,
                                                      .pattern_length = args->pattern_length - 3ull,
                                                      .path = args->path + at + 1u, .length = args->length - at - 1ull))
            {
                return 1;
            }
        }
        return 0;
    }
    if ((args->pattern_length >= 2ull) && (args->pattern_u32s[0] == '*') && (args->pattern_u32s[1] == '*'))
    {
        for (unsigned long long at = 0ull; at <= args->length; at += 1ull)
        {
            if (EMBED_CALL(cli_glob, PatternsCtx, .pattern_u32s = args->pattern_u32s + 2u,
                           .pattern_length = args->pattern_length - 2ull, .path = args->path + at,
                           .length = args->length - at))
            {
                return 1;
            }
        }
        return 0;
    }
    if (args->pattern_u32s[0] == '*')
    {
        unsigned long long name = 0ull;
        while ((name < args->length) && (args->path[name] != '/'))
        {
            name += 1ull;
        }
        for (unsigned long long at = 0ull; at <= name; at += 1ull)
        {
            if (EMBED_CALL(cli_glob, PatternsCtx, .pattern_u32s = args->pattern_u32s + 1u,
                           .pattern_length = args->pattern_length - 1ull, .path = args->path + at,
                           .length = args->length - at))
            {
                return 1;
            }
        }
        return 0;
    }
    if (args->pattern_u32s[0] == '?')
    {
        return (args->length != 0ull) && (args->path[0] != '/') &&
               EMBED_CALL(cli_glob, PatternsCtx, .pattern_u32s = args->pattern_u32s + 1u,
                          .pattern_length = args->pattern_length - 1ull, .path = args->path + 1u,
                          .length = args->length - 1ull);
    }
    return (args->length != 0ull) && (args->path[0] == args->pattern_u32s[0]) &&
           EMBED_CALL(cli_glob, PatternsCtx, .pattern_u32s = args->pattern_u32s + 1u,
                      .pattern_length = args->pattern_length - 1ull, .path = args->path + 1u,
                      .length = args->length - 1ull);
}

// whether the pattern names the path, or a folder it is in; the path's letters folded as a pattern compares them
EMBED_INLINE int cli_hits(const PatternsCtx *args)
{
    const unsigned int *const glob = args->patterns->letters + args->pattern->first;
    for (unsigned long long end = 0ull; end < args->length; end += 1ull)
    {
        if ((args->path[end] == '/') &&
            EMBED_CALL(cli_glob, PatternsCtx, .pattern_u32s = glob, .pattern_length = args->pattern->length,
                       .path = args->path, .length = end))
        {
            return 1;
        }
    }
    return (args->dir || !args->pattern->folders) &&
           EMBED_CALL(cli_glob, PatternsCtx, .pattern_u32s = glob, .pattern_length = args->pattern->length,
                      .path = args->path, .length = args->length);
}

// One pattern's line read into the hiding or the including ones: trimmed, `./` before it left off, a trailing `/`
// naming folders only, and `**/` put before one that has no `/` of its own. An empty one names nothing
EMBED_INLINE void cli_pattern_read(const PatternsCtx *args)
{
    const char *from = args->from;
    const char *to = args->to;
    while ((from < to) && EMBED_CALL(cli_is_space, PatternsCtx, .letter_letter = from[0]))
    {
        from += 1;
    }
    while ((to > from) && EMBED_CALL(cli_is_space, PatternsCtx, .letter_letter = to[-1]))
    {
        to -= 1;
    }
    from += (((to - from) >= 2) && (from[0] == '.') && (from[1] == '/')) ? 2 : 0;
    const int folders = (to > from) && (to[-1] == '/');
    to = from + EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg, .text = from,
                           .at = (unsigned long long)(to - from), .set = "/");
    if (from == to)
    {
        return;
    }
    int slashed = 0;
    slashed = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = from, .at = 0ull,
                         .size = (unsigned long long)(to - from), .set = "/") < (unsigned long long)(to - from);
    from += slashed ? EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = from, .at = 0ull,
                                 .size = (unsigned long long)(to - from), .set = "/")
                    : 0ull;
    unsigned int *const count = args->include ? &args->patterns_out->includes : &args->patterns_out->hides;
    CliPattern *const list = args->include ? args->patterns_out->include : args->patterns_out->hide;
    const unsigned int first = args->patterns_out->used;
    const unsigned long long room = CLI_PATTERN_LETTERS - first;
    unsigned long long length = 0ull;
    static const unsigned int s_anywhere[3] = {'*', '*', '/'};
    if (!slashed && (room >= 3ull))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = args->patterns_out->letters + first, .src = s_anywhere, .bytes = sizeof(s_anywhere)});
        length = 3ull;
    }
    length +=
        EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = from, .size = (unsigned long long)(to - from),
                   .letters = args->patterns_out->letters + first + length, .room_u64 = room - length);
    if ((*count >= CLI_PATTERNS_MOST) || (room < 3ull))
    {
        return;
    }
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        args->patterns_out->letters[first + at] =
            EMBED_CALL(cli_fold, PatternsCtx, .letter = args->patterns_out->letters[first + at]);
    }
    // the length is below CLI_PATTERN_LETTERS, which an unsigned int counts
    const CliPattern pattern = {first, (unsigned int)length, folders};
    list[*count] = pattern;
    *count += 1u;
    args->patterns_out->used = first + (unsigned int)length;
}

// one line of the patterns read: trimmed, nothing where it is empty or starts with `#`, a including pattern where it
// starts with `!`
EMBED_INLINE void cli_line_read(const PatternsCtx *args)
{
    const char *from = args->line;
    const char *to = args->line + args->size;
    while ((from < to) && EMBED_CALL(cli_is_space, PatternsCtx, .letter_letter = from[0]))
    {
        from += 1;
    }
    while ((to > from) && EMBED_CALL(cli_is_space, PatternsCtx, .letter_letter = to[-1]))
    {
        to -= 1;
    }
    if ((from == to) || (from[0] == '#'))
    {
        return;
    }
    const int include = from[0] == '!';
    EMBED_CALL(cli_pattern_read, PatternsCtx, .patterns_out = args->patterns_out, .from = from + (include ? 1 : 0),
               .to = to, .include = include);
}

void cli_patterns_read(const CliPatternsLinesCfg *args)
{
    args->patterns_out->hides = 0u;
    args->patterns_out->includes = 0u;
    args->patterns_out->used = 0u;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        EMBED_CALL(cli_line_read, PatternsCtx, .patterns_out = args->patterns_out, .line = args->lines[at],
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->lines[at], .cap = CLI_CELLS_POOL}));
    }
}

void cli_patterns_read_set(const CliPatternsLinesCfg *args)
{
    args->patterns_out->hides = 0u;
    args->patterns_out->includes = 0u;
    args->patterns_out->used = 0u;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const char *from = args->lines[at];
        const char *to = from + cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL});
        while ((from < to) && EMBED_CALL(cli_is_space, PatternsCtx, .letter_letter = from[0]))
        {
            from += 1;
        }
        while ((to > from) && EMBED_CALL(cli_is_space, PatternsCtx, .letter_letter = to[-1]))
        {
            to -= 1;
        }
        if ((from == to) || (from[0] == '#'))
        {
            continue;
        }
        // each line turned about: one that takes out is a pattern that hides, and one that names includes
        char flipped[CLI_PATH_MAX_LEN];
        VerbaLinea line = {flipped, sizeof(flipped), 0ull};
        if (from[0] != '!')
        {
            mmgr_line_ch(&line, '!');
        }
        mmgr_line_put_n(&line, from + ((from[0] == '!') ? 1 : 0),
                        (unsigned long long)(to - from) - ((from[0] == '!') ? 1ull : 0ull));
        const unsigned long long length = mmgr_line_ok(&line) ? line.at : 0ull;
        mmgr_line_finish(&line);
        EMBED_CALL(cli_line_read, PatternsCtx, .patterns_out = args->patterns_out, .line = flipped, .size = length);
    }
}

int cli_patterns_empty(const CliPatternsCfg *args)
{
    return (args->patterns->hides == 0u) && (args->patterns->includes == 0u);
}

int cli_patterns_include_only(const CliPatternsCfg *args)
{
    return args->patterns->includes != 0u;
}

// the path's letters, each folded as a pattern compares it, into `letters`, sized for CLI_PATH_MAX_LEN: their count
EMBED_INLINE unsigned long long cli_path_letters(const PatternsCtx *args)
{
    unsigned int *letters = args->letters;
    const unsigned long long length =
        EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = args->path_text,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->path_text, .cap = CLI_CELLS_POOL}),
                   .letters = letters, .room_u64 = CLI_PATH_MAX_LEN);
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        letters[at] = EMBED_CALL(cli_fold, PatternsCtx, .letter = letters[at]);
    }
    return length;
}

int cli_patterns_hides(const CliPatternsPathCfg *args)
{
    unsigned int letters[CLI_PATH_MAX_LEN];
    const unsigned long long length =
        EMBED_CALL(cli_path_letters, PatternsCtx, .path_text = args->path, .letters = letters);
    for (unsigned int at = 0u; at < args->patterns->hides; at += 1u)
    {
        if (EMBED_CALL(cli_hits, PatternsCtx, .patterns = args->patterns, .pattern = &args->patterns->hide[at],
                       .path = letters, .length = length, .dir = args->dir))
        {
            return 1;
        }
    }
    if (args->dir || (args->patterns->includes == 0u))
    {
        return 0;
    }
    for (unsigned int at = 0u; at < args->patterns->includes; at += 1u)
    {
        if (EMBED_CALL(cli_hits, PatternsCtx, .patterns = args->patterns, .pattern = &args->patterns->include[at],
                       .path = letters, .length = length, .dir = 0))
        {
            return 0;
        }
    }
    return 1;
}

int cli_patterns_include_folder(const CliPatternsPathCfg *args)
{
    unsigned int letters[CLI_PATH_MAX_LEN];
    const unsigned long long length =
        EMBED_CALL(cli_path_letters, PatternsCtx, .path_text = args->path, .letters = letters);
    for (unsigned int at = 0u; at < args->patterns->includes; at += 1u)
    {
        if (EMBED_CALL(cli_hits, PatternsCtx, .patterns = args->patterns, .pattern = &args->patterns->include[at],
                       .path = letters, .length = length, .dir = 1))
        {
            return 1;
        }
    }
    return 0;
}

// one pattern as git's pathspecs: the path itself and everything in it, under `magic`
EMBED_INLINE void cli_pattern_specs(const PatternsCtx *args)
{
    CliWords *words = args->words;
#if defined(_WIN32)
    const char *const letter_case = ",icase";
#else
    const char *const letter_case = "";
#endif
    char spec[CLI_PATH_MAX_LEN];
    for (int inside = args->pattern->folders ? 1 : 0; inside < 2; inside += 1)
    {
        VerbaLinea line = {spec, sizeof(spec), 0ull};
        mmgr_line_put(&line, ":(");
        mmgr_line_put(&line, args->magic);
        mmgr_line_put(&line, "glob");
        mmgr_line_put(&line, letter_case);
        mmgr_line_ch(&line, ')');
        for (unsigned int at = 0u; at < args->pattern->length; at += 1u)
        {
            EMBED_CALL(cli_line_letter_internal.utf8_put, CliLineLetterCtx, .line = &line,
                       .letter = args->patterns->letters[args->pattern->first + at]);
        }
        if (inside)
        {
            mmgr_line_put(&line, "/**");
        }
        words->full = words->full || !mmgr_line_ok(&line);
        mmgr_line_finish(&line);
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = words, .word = spec);
    }
}

void cli_patterns_include_specs(const CliPatternsCfg *args)
{
    for (unsigned int at = 0u; at < args->patterns->includes; at += 1u)
    {
        EMBED_CALL(cli_pattern_specs, PatternsCtx, .patterns = args->patterns, .pattern = &args->patterns->include[at],
                   .magic = "", .words = args->words);
    }
}

void cli_patterns_hidden_specs(const CliPatternsCfg *args)
{
    for (unsigned int at = 0u; at < args->patterns->hides; at += 1u)
    {
        EMBED_CALL(cli_pattern_specs, PatternsCtx, .patterns = args->patterns, .pattern = &args->patterns->hide[at],
                   .magic = "exclude,", .words = args->words);
    }
}

void cli_patterns_pathspecs(const CliPatternsCfg *args)
{
    if (args->patterns->includes == 0u)
    {
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word = ".");
    }
    cli_patterns.include_specs(args);
    cli_patterns.hidden_specs(args);
}

// whether two lists of patterns name the same, pattern by pattern
EMBED_INLINE int cli_same_list(const PatternsCtx *args)
{
    const CliPatterns *other = args->other;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if ((args->list[at].folders != args->other_list[at].folders) ||
            (args->list[at].length != args->other_list[at].length) ||
            (memor.cmp(&(MemoriaCfg){.src = args->one->letters + args->list[at].first,
                                     .other = other->letters + args->other_list[at].first,
                                     .bytes = args->list[at].length * sizeof(unsigned int)}) != 0))
        {
            return 0;
        }
    }
    return 1;
}

int cli_patterns_set(const CliPatternsLinesCountCfg *args)
{
    static CliPatterns s_read;
    EMBED_CALL(cli_patterns_lines.read, CliPatternsLinesCfg, .lines = args->lines, .count = args->count,
               .patterns_out = &s_read);
    CliPatterns *const set_at = &s_set[args->part];
    const int same = s_made[args->part] && (set_at->hides == s_read.hides) && (set_at->includes == s_read.includes) &&
                     EMBED_CALL(cli_same_list, PatternsCtx, .one = set_at, .list = set_at->hide, .other = &s_read,
                                .other_list = s_read.hide, .count = s_read.hides) &&
                     EMBED_CALL(cli_same_list, PatternsCtx, .one = set_at, .list = set_at->include, .other = &s_read,
                                .other_list = s_read.include, .count = s_read.includes);
    memor.cpy(&(MemoriaCfg){.dst = set_at, .src = &s_read, .bytes = sizeof(s_read)});
    s_made[args->part] = 1;
    return !same;
}

void cli_patterns_get(const CliPatternsPartCfg *args)
{
    if (s_made[args->part])
    {
        memor.cpy(
            &(MemoriaCfg){.dst = args->patterns_out, .src = &s_set[args->part], .bytes = sizeof(*args->patterns_out)});
    }
    else
    {
        args->patterns_out->hides = 0u;
        args->patterns_out->includes = 0u;
        args->patterns_out->used = 0u;
    }
}

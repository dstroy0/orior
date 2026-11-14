// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_fuzzy.c: the match of a query's letters in a text, in order anywhere in it, case aside, in no more pieces than
// half its letters or three, a letter scoring more at the start of a word or past where the text's name starts. The
// palette's own matcher, fuzzy.js, scores the same
#include "../text/regexp.h"
#include "files_internal.h"

unsigned int cli_top_put(const CliFilesTopCtx *args)
{
    CliTopPlace *const top = args->top;
    unsigned int low = 0u;
    unsigned int high = args->count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (top[middle].score >= args->score)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    if (low >= args->most)
    {
        return args->count;
    }
    const unsigned int count = (args->count < args->most) ? (args->count + 1u) : args->most;
    for (unsigned int at = count - 1u; at > low; at -= 1u)
    {
        top[at] = top[at - 1u];
    }
    top[low] = (CliTopPlace){args->score, args->at};
    return count;
}

// whether a letter is whitespace
int cli_fuzzy_white(const CliFilesLetterCtx *args)
{
    return (args->letter == ' ') || ((args->letter >= '\t') && (args->letter <= '\r')) || (args->letter == 0x85u) ||
           (args->letter == 0xA0u) || (args->letter == 0x1680u) ||
           ((args->letter >= 0x2000u) && (args->letter <= 0x200Au)) || (args->letter == 0x2028u) ||
           (args->letter == 0x2029u) || (args->letter == 0x202Fu) || (args->letter == 0x205Fu) ||
           (args->letter == 0x3000u);
}

typedef struct
{
    unsigned int letter;
    int bytes;
    const CliFilesBytesRequestedCtx *request;
    unsigned int index;
    unsigned int after;
    int has_last;
    unsigned int *found;
} FilesFuzzyCtx;
// whether a letter splits a text into words: whitespace, or of the bytes of a path all ASCII a space or a tab, and a
// slash, a separator in a name, a dot, a colon or an at; and whether it is upper case
EMBED_INLINE int cli_splits(const FilesFuzzyCtx *args)
{
    unsigned int letter = args->letter;
    const int white = args->bytes
                          ? ((letter == ' ') || (letter == '\t'))
                          : EMBED_CALL(cli_files_letter_internal.fuzzy_white, CliFilesLetterCtx, .letter = letter);
    return white || (letter == '/') || (letter == '\\') || (letter == '_') || (letter == '-') || (letter == '.') ||
           (letter == ':') || (letter == '@');
}

EMBED_INLINE int cli_upper(const FilesFuzzyCtx *args)
{
    unsigned int letter = args->letter;
    return args->bytes ? ((letter >= 'A') && (letter <= 'Z'))
                       : EMBED_CALL(cli_regexp_letter.property, CliRegexpLetterCfg, .property = CLI_REGEXP_UPPER,
                                    .letter = letter);
}

int cli_fuzzy_starts_word(const CliFilesTextU32sCtx *args)
{
    return (args->at == 0u) ||
           EMBED_CALL(cli_splits, FilesFuzzyCtx, .letter = args->text_u32s[args->at - 1u], .bytes = args->bytes_i32) ||
           (!EMBED_CALL(cli_upper, FilesFuzzyCtx, .letter = args->text_u32s[args->at - 1u], .bytes = args->bytes_i32) &&
            EMBED_CALL(cli_upper, FilesFuzzyCtx, .letter = args->text_u32s[args->at], .bytes = args->bytes_i32));
}

// the latest place in the text each requested letter can stand with the letters after it still fitting, into
// `latest`: 1, or 0 where they do not all fit
EMBED_INLINE int fuzzy_latest_found(const CliFilesBytesRequestedCtx *args)
{
    unsigned int at = args->count_u32;
    for (unsigned int index = args->requested_count; index > 0u; index -= 1u)
    {
        while ((at > 0u) && (args->lower[at - 1u] != args->required[index - 1u]))
        {
            at -= 1u;
        }
        if (at == 0u)
        {
            return 0;
        }
        at -= 1u;
        args->latest[index - 1u] = at;
    }
    return 1;
}

// the place of the `request`'s letter at `index`, from `after` on and no later than its latest, into `found`: right at
// `after` where it `has_last` letter before it, else the first that starts a word, else the first; 1, or 0 where the
// letter stands at none
EMBED_INLINE int fuzzy_place_found(const FilesFuzzyCtx *args)
{
    const CliFilesBytesRequestedCtx *const request = args->request;
    const unsigned int letter = request->required[args->index];
    const unsigned int after = args->after;
    if (args->has_last && (after < request->count_u32) && (request->lower[after] == letter))
    {
        *args->found = after;
        return 1;
    }
    int has_first = 0;
    unsigned int first = 0u;
    for (unsigned int place = after; place <= request->latest[args->index]; place += 1u)
    {
        if (request->lower[place] != letter)
        {
            continue;
        }
        if (EMBED_CALL(cli_files_text_u_32s_internal.fuzzy_starts_word, CliFilesTextU32sCtx,
                       .text_u32s = request->displayed, .at = place, .bytes_i32 = request->bytes_i32))
        {
            *args->found = place;
            return 1;
        }
        first = has_first ? first : place;
        has_first = 1;
    }
    *args->found = first;
    return has_first;
}

// how many pieces the requested letters' hits stand in, each piece of places one after another
EMBED_INLINE unsigned int fuzzy_runs(const CliFilesBytesRequestedCtx *args)
{
    unsigned int runs = 0u;
    for (unsigned int index = 0u; index < args->requested_count; index += 1u)
    {
        runs += ((index == 0u) || (args->hits[index] != (args->hits[index - 1u] + 1u))) ? 1u : 0u;
    }
    return runs;
}

int cli_fuzzy_letters(const CliFilesBytesRequestedCtx *args)
{
    double *score = args->score;
    unsigned int *hit_count = args->hit_count;
    *score = 0.0;
    *hit_count = 0u;
    if (args->requested_count == 0u)
    {
        return 1;
    }
    if ((args->requested_count > CLI_FUZZY_LETTERS) || !fuzzy_latest_found(args))
    {
        return 0;
    }
    int has_last = 0;
    unsigned int last = 0u;
    for (unsigned int index = 0u; index < args->requested_count; index += 1u)
    {
        const unsigned int letter = args->required[index];
        unsigned int found = 0u;
        if (!EMBED_CALL(fuzzy_place_found, FilesFuzzyCtx, .request = args, .index = index,
                        .after = has_last ? (last + 1u) : 0u, .has_last = has_last, .found = &found))
        {
            return 0;
        }
        *score += 1.0;
        *score += (has_last && (found == (last + 1u))) ? 5.0 : 0.0;
        *score += EMBED_CALL(cli_files_text_u_32s_internal.fuzzy_starts_word, CliFilesTextU32sCtx,
                             .text_u32s = args->displayed, .at = found, .bytes_i32 = args->bytes_i32)
                      ? 8.0
                      : 0.0;
        *score += (found >= args->from_u32) ? 3.0 : 0.0;
        *score += (args->displayed[found] == letter) ? 1.0 : 0.0;
        args->hits[index] = found;
        last = found;
        has_last = 1;
    }
    // a query torn into more pieces than half its letters reads as no match
    const unsigned int half = (args->requested_count + 1u) / 2u;
    if (fuzzy_runs(args) > ((half > 3u) ? half : 3u))
    {
        return 0;
    }
    *score -=
        ((double)(args->hits[args->requested_count - 1u] - args->hits[0]) * 0.1) + ((double)args->count_u32 * 0.02);
    *hit_count = args->requested_count;
    return 1;
}

unsigned int cli_files_want(const CliFilesQueryLettersCfg *args)
{
    static unsigned int s_read[CLI_FUZZY_LETTERS];
    const unsigned long long count =
        EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = args->query,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->query, .cap = CLI_POOL_BYTES}), .letters = s_read,
                   .capacity_u64 = CLI_FUZZY_LETTERS);
    unsigned int requested_count = 0u;
    for (unsigned long long at = 0ULL; (at < count) && (requested_count < args->capacity); at += 1ULL)
    {
        if (!EMBED_CALL(cli_files_letter_internal.fuzzy_white, CliFilesLetterCtx, .letter = s_read[at]))
        {
            args->letters[requested_count] =
                EMBED_CALL(cli_regexp_letter.lower, CliRegexpLetterCfg, .letter = s_read[at]);
            requested_count += 1u;
        }
    }
    return requested_count;
}

unsigned int cli_files_letters(const CliFilesTextCfg *args)
{
    // a letter count is below `capacity`, which an unsigned int stores
    const unsigned int count =
        (unsigned int)EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = args->text,
                                 .size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES}),
                                 .letters = args->displayed, .capacity_u64 = args->capacity);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        args->lower[at] = EMBED_CALL(cli_regexp_letter.lower, CliRegexpLetterCfg, .letter = args->displayed[at]);
    }
    return count;
}

int cli_files_fuzzy_matches(const CliFilesCountCfg *args)
{
    return EMBED_CALL(cli_files_bytes_requested_internal.fuzzy_letters, CliFilesBytesRequestedCtx,
                      .required = args->required, .requested_count = args->requested_count,
                      .displayed = args->displayed_read, .lower = args->lower_read, .count_u32 = args->count,
                      .from_u32 = 0u, .bytes_i32 = 0, .latest = args->latest, .score = args->score, .hits = args->hits,
                      .hit_count = args->hit_count);
}

int cli_files_fuzzy(const CliFilesQueryCfg *args)
{
    static unsigned int s_requested[CLI_FUZZY_LETTERS];
    static unsigned int s_shown[CLI_FUZZY_LETTERS];
    static unsigned int s_lower[CLI_FUZZY_LETTERS];
    static unsigned int s_latest[CLI_FUZZY_LETTERS];
    const unsigned int required =
        EMBED_CALL(cli_files_query_letters.want, CliFilesQueryLettersCfg, .query = args->query, .letters = s_requested,
                   .capacity = CLI_FUZZY_LETTERS);
    const unsigned int letters = EMBED_CALL(cli_files_text.letters, CliFilesTextCfg, .text = args->text,
                                            .displayed = s_shown, .lower = s_lower, .capacity = CLI_FUZZY_LETTERS);
    // a place in letters is below CLI_FUZZY_LETTERS, which an unsigned int stores
    const unsigned int start = (args->from_u64 < CLI_FUZZY_LETTERS) ? (unsigned int)args->from_u64 : CLI_FUZZY_LETTERS;
    const int matched = EMBED_CALL(
        cli_files_bytes_requested_internal.fuzzy_letters, CliFilesBytesRequestedCtx, .required = s_requested,
        .requested_count = required, .displayed = s_shown, .lower = s_lower, .count_u32 = letters, .from_u32 = start,
        .bytes_i32 = 0, .latest = s_latest, .score = args->score, .hits = args->hits, .hit_count = args->count_pointer);
    return matched;
}

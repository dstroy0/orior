// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_fuzzy.c: the match of a query's characters in a text, in order anywhere in it, case aside, in no more pieces than
// half its characters or three, a character scoring more at the start of a word or past where the text's name starts. The
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
    if (low >= args->max)
    {
        return args->count;
    }
    const unsigned int count = (args->count < args->max) ? (args->count + 1u) : args->max;
    for (unsigned int at = count - 1u; at > low; at -= 1u)
    {
        top[at] = top[at - 1u];
    }
    top[low] = (CliTopPlace){args->score, args->at};
    return count;
}

// whether a character is whitespace
int cli_fuzzy_white(const CliFilesLetterCtx *args)
{
    return (args->character == ' ') || ((args->character >= '\t') && (args->character <= '\r')) || (args->character == 0x85u) ||
           (args->character == 0xA0u) || (args->character == 0x1680u) ||
           ((args->character >= 0x2000u) && (args->character <= 0x200Au)) || (args->character == 0x2028u) ||
           (args->character == 0x2029u) || (args->character == 0x202Fu) || (args->character == 0x205Fu) ||
           (args->character == 0x3000u);
}

typedef struct
{
    unsigned int character;
    int bytes;
    const CliFilesBytesRequestedCtx *request;
    unsigned int index;
    unsigned int after;
    int has_last;
    unsigned int *found;
} FilesFuzzyCtx;
// whether a character splits a text into words: whitespace, or of the bytes of a path all ASCII a space or a tab, and a
// slash, a separator in a name, a dot, a colon or an at; and whether it is upper case
EMBED_INLINE int cli_splits(const FilesFuzzyCtx *args)
{
    unsigned int character = args->character;
    const int white = args->bytes
                          ? ((character == ' ') || (character == '\t'))
                          : EMBED_CALL(cli_files_character_internal.fuzzy_white, CliFilesLetterCtx, .character = character);
    return white || (character == '/') || (character == '\\') || (character == '_') || (character == '-') || (character == '.') ||
           (character == ':') || (character == '@');
}

EMBED_INLINE int cli_upper(const FilesFuzzyCtx *args)
{
    unsigned int character = args->character;
    return args->bytes ? ((character >= 'A') && (character <= 'Z'))
                       : EMBED_CALL(cli_regexp_character.property, CliRegexpLetterCfg, .property = CLI_REGEXP_UPPER,
                                    .character = character);
}

int cli_fuzzy_starts_word(const CliFilesTextU32sCtx *args)
{
    return (args->at == 0u) ||
           EMBED_CALL(cli_splits, FilesFuzzyCtx, .character = args->text_u32s[args->at - 1u], .bytes = args->bytes_i32) ||
           (!EMBED_CALL(cli_upper, FilesFuzzyCtx, .character = args->text_u32s[args->at - 1u], .bytes = args->bytes_i32) &&
            EMBED_CALL(cli_upper, FilesFuzzyCtx, .character = args->text_u32s[args->at], .bytes = args->bytes_i32));
}

// the latest place in the text each requested character can are with the characters after it still fitting, into
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

// the place of the `request`'s character at `index`, from `after` on and no later than its latest, into `found`: right at
// `after` where it `has_last` character before it, else the first that starts a word, else the first; 1, or 0 where the
// character is at none
EMBED_INLINE int fuzzy_place_found(const FilesFuzzyCtx *args)
{
    const CliFilesBytesRequestedCtx *const request = args->request;
    const unsigned int character = request->required[args->index];
    const unsigned int after = args->after;
    if (args->has_last && (after < request->count_u32) && (request->lower[after] == character))
    {
        *args->found = after;
        return 1;
    }
    int has_first = 0;
    unsigned int first = 0u;
    for (unsigned int place = after; place <= request->latest[args->index]; place += 1u)
    {
        if (request->lower[place] != character)
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

// how many pieces the requested characters' hits are in, each piece of places one after another
EMBED_INLINE unsigned int fuzzy_runs(const CliFilesBytesRequestedCtx *args)
{
    unsigned int runs = 0u;
    for (unsigned int index = 0u; index < args->requested_count; index += 1u)
    {
        runs += ((index == 0u) || (args->hits[index] != (args->hits[index - 1u] + 1u))) ? 1u : 0u;
    }
    return runs;
}

int cli_fuzzy_characters(const CliFilesBytesRequestedCtx *args)
{
    double *score = args->score;
    unsigned int *hit_count = args->hit_count;
    *score = 0.0;
    *hit_count = 0u;
    if (args->requested_count == 0u)
    {
        return 1;
    }
    if ((args->requested_count > CLI_FUZZY_CHARACTERS) || !fuzzy_latest_found(args))
    {
        return 0;
    }
    int has_last = 0;
    unsigned int last = 0u;
    for (unsigned int index = 0u; index < args->requested_count; index += 1u)
    {
        const unsigned int character = args->required[index];
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
        *score += (args->displayed[found] == character) ? 1.0 : 0.0;
        args->hits[index] = found;
        last = found;
        has_last = 1;
    }
    // a query torn into more pieces than half its characters reads as no match
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
    static unsigned int s_read[CLI_FUZZY_CHARACTERS];
    const unsigned long long count =
        EMBED_CALL(cli_regexp_text.characters, CliRegexpTextCfg, .text = args->query,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->query, .cap = CLI_POOL_BYTES}), .characters = s_read,
                   .capacity_u64 = CLI_FUZZY_CHARACTERS);
    unsigned int requested_count = 0u;
    for (unsigned long long at = 0ULL; (at < count) && (requested_count < args->capacity); at += 1ULL)
    {
        if (!EMBED_CALL(cli_files_character_internal.fuzzy_white, CliFilesLetterCtx, .character = s_read[at]))
        {
            args->characters[requested_count] =
                EMBED_CALL(cli_regexp_character.lower, CliRegexpLetterCfg, .character = s_read[at]);
            requested_count += 1u;
        }
    }
    return requested_count;
}

unsigned int cli_files_characters(const CliFilesTextCfg *args)
{
    // a character count is below `capacity`, which an unsigned int stores
    const unsigned int count =
        (unsigned int)EMBED_CALL(cli_regexp_text.characters, CliRegexpTextCfg, .text = args->text,
                                 .size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES}),
                                 .characters = args->displayed, .capacity_u64 = args->capacity);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        args->lower[at] = EMBED_CALL(cli_regexp_character.lower, CliRegexpLetterCfg, .character = args->displayed[at]);
    }
    return count;
}

int cli_files_fuzzy_matches(const CliFilesCountCfg *args)
{
    return EMBED_CALL(cli_files_bytes_requested_internal.fuzzy_characters, CliFilesBytesRequestedCtx,
                      .required = args->required, .requested_count = args->requested_count,
                      .displayed = args->displayed_read, .lower = args->lower_read, .count_u32 = args->count,
                      .from_u32 = 0u, .bytes_i32 = 0, .latest = args->latest, .score = args->score, .hits = args->hits,
                      .hit_count = args->hit_count);
}

int cli_files_fuzzy(const CliFilesQueryCfg *args)
{
    static unsigned int s_requested[CLI_FUZZY_CHARACTERS];
    static unsigned int s_displayed[CLI_FUZZY_CHARACTERS];
    static unsigned int s_lower[CLI_FUZZY_CHARACTERS];
    static unsigned int s_latest[CLI_FUZZY_CHARACTERS];
    const unsigned int required =
        EMBED_CALL(cli_files_query_characters.want, CliFilesQueryLettersCfg, .query = args->query, .characters = s_requested,
                   .capacity = CLI_FUZZY_CHARACTERS);
    const unsigned int characters = EMBED_CALL(cli_files_text.characters, CliFilesTextCfg, .text = args->text,
                                            .displayed = s_displayed, .lower = s_lower, .capacity = CLI_FUZZY_CHARACTERS);
    // a place in characters is below CLI_FUZZY_CHARACTERS, which an unsigned int stores
    const unsigned int start = (args->from_u64 < CLI_FUZZY_CHARACTERS) ? (unsigned int)args->from_u64 : CLI_FUZZY_CHARACTERS;
    const int matched = EMBED_CALL(
        cli_files_bytes_requested_internal.fuzzy_characters, CliFilesBytesRequestedCtx, .required = s_requested,
        .requested_count = required, .displayed = s_displayed, .lower = s_lower, .count_u32 = characters, .from_u32 = start,
        .bytes_i32 = 0, .latest = s_latest, .score = args->score, .hits = args->hits, .hit_count = args->count_pointer);
    return matched;
}

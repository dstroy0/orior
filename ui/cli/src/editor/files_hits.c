// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_hits.c: the lines of one file that contain a search's query, the first place on each line it is, at max
// CLI_HITS_MAX of them, and the text of a found line as a hit retains it
#include "../command_line/callback_cfg.h"
#include "../text/json_tree.h"
#include "../text/regexp.h"
#include "../text/sift.h"
#include "../text/text.h"
#include "files_internal.h"

// A line's characters as they are stepped over: the byte, the character's place, and the character before, read in lower case
typedef struct
{
    unsigned long long at;
    unsigned long long character_at;
    unsigned int last;
} CliStep;
unsigned long long cli_hit_cut(const CliFilesPathCtx *args)
{
    // where the last character that is not whitespace ends, and where the last character retained ends
    unsigned long long trimmed = 0ULL;
    unsigned long long max = args->size;
    unsigned int characters = 0u;
    unsigned long long at = 0ULL;
    while (at < args->size)
    {
        unsigned int character = 0u;
        at += EMBED_CALL(cli_json_tree_length.lossy_character, CliJsonTreeLengthCfg, .text_bytes = args->text,
                         .length = args->size, .at = at, .character = &character);
        characters += (characters < CLI_HIT_TEXT) ? 1u : 0u;
        max = ((characters == CLI_HIT_TEXT) && (max == args->size)) ? at : max;
        trimmed = EMBED_CALL(cli_files_character_internal.fuzzy_white, CliFilesLetterCtx, .character = character) ? trimmed : at;
    }
    return (trimmed < max) ? trimmed : max;
}

int cli_hit_wordy(const CliFilesLetterCtx *args)
{
    return (args->character == '_') ||
           EMBED_CALL(cli_regexp_character.property, CliRegexpLetterCfg, .property = CLI_REGEXP_CHARACTER,
                      .character = args->character) ||
           EMBED_CALL(cli_regexp_character.property, CliRegexpLetterCfg, .property = CLI_REGEXP_NUMBER,
                      .character = args->character);
}

// The sift's places in one file as they are read: the file and what is looked for, where the hits go, the line the
// last place was on, the byte its newlines were counted to, the line of the last hit, and the hits so far
typedef struct
{
    const unsigned char *bytes;
    unsigned long long size;
    const CliNeedle *needle;
    CliHitAppend put;
    void *context;
    unsigned long long line;
    unsigned long long counted;
    unsigned long long last_line;
    unsigned int found;
} CliHitting;

// a place the sift found the query: a hit where it is the first on its line, a whole word where one is requested
static void cli_hit_at(const CliEachAtCfg *args)
{
    unsigned long long at = args->at;
    CliHitting *const hitting = (CliHitting *)args->context;
    const unsigned char *const bytes = hitting->bytes;
    if (hitting->found >= CLI_HITS_MAX)
    {
        return;
    }
    for (unsigned long long place = hitting->counted; place < at; place += 1ULL)
    {
        hitting->line += (bytes[place] == '\n') ? 1ULL : 0ULL;
    }
    hitting->counted = at;
    if (hitting->line == hitting->last_line)
    {
        return;
    }
    unsigned long long start = at;
    start = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = (const char *)bytes,
                       .at = start, .set = "\n");
    const unsigned long long end =
        EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = (const char *)bytes, .at = at, .size = hitting->size)
            .stop;
    // the characters before the query on its line, and the last of them
    unsigned long long before = 0ULL;
    unsigned int last = 0u;
    {
        unsigned long long place = start;
        while (place < at)
        {
            place += EMBED_CALL(cli_json_tree_length.lossy_character, CliJsonTreeLengthCfg, .text_bytes = bytes,
                                .length = at, .at = place, .character = &last);
            before += 1ULL;
        }
    }
    if (hitting->needle->how.word_text)
    {
        const unsigned long long after = at + hitting->needle->length;
        unsigned int next = 0u;
        const int has_next = after < end;
        if (has_next)
        {
            EMBED_CALL(cli_json_tree_length.lossy_character, CliJsonTreeLengthCfg, .text_bytes = bytes, .length = end,
                       .at = after, .character = &next);
        }
        if (((before != 0ULL) && EMBED_CALL(cli_files_character_internal.hit_wordy, CliFilesLetterCtx, .character = last)) ||
            (has_next && EMBED_CALL(cli_files_character_internal.hit_wordy, CliFilesLetterCtx, .character = next)))
        {
            return;
        }
    }
    hitting->last_line = hitting->line;
    EMBED_CALL(hitting->put, CliHitAppendCfg, .context = hitting->context, .line = hitting->line, .col = before + 1ULL,
               .text = bytes + start,
               .size = EMBED_CALL(cli_files_path_internal.hit_cut, CliFilesPathCtx, .text = bytes + start,
                                  .size = end - start));
    hitting->found += 1u;
}

typedef struct
{
    const unsigned char *line;
    unsigned long long size;
    unsigned long long at;
    const CliNeedle *needle;
    unsigned long long *after;
    CliStep *step;
    unsigned long long *col;
} FilesHitsCtx;
// whether the characters of `line`, `size` bytes, from `at`, read in lower case, are the query's, the byte after them
// into `after`
EMBED_INLINE int cli_lower_at(const FilesHitsCtx *args)
{
    unsigned long long at = args->at;
    unsigned long long *after = args->after;
    for (unsigned int index = 0u; index < args->needle->lower_count; index += 1u)
    {
        if (at >= args->size)
        {
            return 0;
        }
        unsigned int character = 0u;
        at += EMBED_CALL(cli_json_tree_length.lossy_character, CliJsonTreeLengthCfg, .text_bytes = args->line,
                         .length = args->size, .at = at, .character = &character);
        if (EMBED_CALL(cli_regexp_character.lower, CliRegexpLetterCfg, .character = character) != args->needle->lower[index])
        {
            return 0;
        }
    }
    *after = at;
    return 1;
}

EMBED_INLINE void cli_on_step(const FilesHitsCtx *args)
{
    unsigned int character = 0u;
    args->step->at += EMBED_CALL(cli_json_tree_length.lossy_character, CliJsonTreeLengthCfg, .text_bytes = args->line,
                                 .length = args->size, .at = args->step->at, .character = &character);
    args->step->character_at += 1ULL;
    args->step->last = EMBED_CALL(cli_regexp_character.lower, CliRegexpLetterCfg, .character = character);
}

// the first place on the line the query is in its lower case, a whole word where one is requested: 1 with its
// character's place into `col`, or 0
EMBED_INLINE int cli_line_match(const FilesHitsCtx *args)
{
    unsigned long long *col = args->col;
    CliStep step = {0ULL, 0ULL, 0u};
    while (step.at < args->size)
    {
        unsigned long long after = 0ULL;
        if (!EMBED_CALL(cli_lower_at, FilesHitsCtx, .line = args->line, .size = args->size, .at = step.at,
                        .needle = args->needle, .after = &after))
        {
            EMBED_CALL(cli_on_step, FilesHitsCtx, .step = &step, .line = args->line, .size = args->size);
            continue;
        }
        unsigned int next = 0u;
        const int has_next = after < args->size;
        if (has_next)
        {
            EMBED_CALL(cli_json_tree_length.lossy_character, CliJsonTreeLengthCfg, .text_bytes = args->line,
                       .length = args->size, .at = after, .character = &next);
        }
        const int joined =
            ((step.character_at != 0ULL) &&
             EMBED_CALL(cli_files_character_internal.hit_wordy, CliFilesLetterCtx, .character = step.last)) ||
            (has_next && EMBED_CALL(cli_files_character_internal.hit_wordy, CliFilesLetterCtx,
                                    .character = EMBED_CALL(cli_regexp_character.lower, CliRegexpLetterCfg, .character = next)));
        if (!args->needle->how.word_text || !joined)
        {
            *col = step.character_at + 1ULL;
            return 1;
        }
        // a match that is no whole word is passed over whole
        for (unsigned int index = 0u; index < args->needle->lower_count; index += 1u)
        {
            EMBED_CALL(cli_on_step, FilesHitsCtx, .step = &step, .line = args->line, .size = args->size);
        }
    }
    return 0;
}

void cli_files_hits_in(const CliFilesContextCtx *args)
{
    if (args->needle->how.match_case || args->needle->ascii)
    {
        CliHitting hitting = {
            args->bytes_bytes, args->size, args->needle, args->put, args->context, 1ULL, 0ULL, 0ULL, 0u};
        EMBED_CALL(cli_sift_length.find_all, CliSiftLengthCfg, .text = args->bytes_bytes, .length = args->size,
                   .needle = (const unsigned char *)args->needle->query, .needle_length = args->needle->length,
                   .fold = !args->needle->how.match_case, .found = cli_hit_at, .context = &hitting);
        return;
    }
    // each line, less a carriage return before its newline
    unsigned long long start = 0ULL;
    unsigned long long index = 0ULL;
    unsigned int found = 0u;
    while ((start < args->size) && (found < CLI_HITS_MAX))
    {
        const CliTextLine bounds = EMBED_CALL(cli_text_at.line_at, CliTextAtCfg,
                                              .text = (const char *)args->bytes_bytes, .at = start, .size = args->size);
        const unsigned long long end = bounds.stop;
        unsigned long long length = end - start;
        length -= bounds.stop - bounds.cut;
        index += 1ULL;
        unsigned long long col = 0ULL;
        if (EMBED_CALL(cli_line_match, FilesHitsCtx, .line = args->bytes_bytes + start, .size = length,
                       .needle = args->needle, .col = &col))
        {
            EMBED_CALL(args->put, CliHitAppendCfg, .context = args->context, .line = index, .col = col,
                       .text = args->bytes_bytes + start,
                       .size = EMBED_CALL(cli_files_path_internal.hit_cut, CliFilesPathCtx,
                                          .text = args->bytes_bytes + start, .size = length));
            found += 1u;
        }
        start = end + 1ULL;
    }
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// purpose_lines.c: the spans of a text a comment is read from, trimmed and matched, a license's comment known, and a
// comment's lines with their marks taken off
#include "purpose_lines.h"
#include "../editor/files_internal.h"
#include "../text/json_tree.h"
#include "../text/text.h"

CliSpan cli_purpose_trimmed_start(const CliPurposeLinesSpanCfg *args)
{
    CliSpan span = args->span;
    while (span.length > 0ull)
    {
        unsigned int letter = 0u;
        const unsigned long long step = EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                                                   .text_bytes = (const unsigned char *)span.at, .length = span.length,
                                                   .at = 0ull, .letter = &letter);
        if (!EMBED_CALL(cli_files_letter_internal.fuzzy_white, CliFilesLetterCtx, .letter = letter))
        {
            break;
        }
        span = (CliSpan){span.at + step, span.length - step};
    }
    return span;
}

CliSpan cli_purpose_trim(const CliPurposeLinesSpanCfg *args)
{
    CliSpan span = args->span;
    span = EMBED_CALL(cli_purpose_lines_span.purpose_trimmed_start, CliPurposeLinesSpanCfg, .span = span);
    unsigned long long end = 0ull;
    for (unsigned long long at = 0ull; at < span.length;)
    {
        unsigned int letter = 0u;
        const unsigned long long step = EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                                                   .text_bytes = (const unsigned char *)span.at, .length = span.length,
                                                   .at = at, .letter = &letter);
        at += step;
        end = EMBED_CALL(cli_files_letter_internal.fuzzy_white, CliFilesLetterCtx, .letter = letter) ? end : at;
    }
    return (CliSpan){span.at, end};
}

int cli_purpose_starts(const CliPurposeLinesSpanCfg *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL});
    return (args->span.length >= length) &&
           (memor.cmp(&(MemoriaCfg){.src = args->span.at, .other = args->text, .bytes = length}) == 0);
}

int cli_purpose_license(const CliPurposeLinesCfg *args)
{
    static const char *const s_words[2] = {"copyright", "license"};
    for (unsigned int word = 0u; word < 2u; word += 1u)
    {
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = s_words[word], .cap = CLI_CELLS_POOL});
        for (unsigned long long at = 0ull; (at + size) <= args->length; at += 1ull)
        {
            if (cellul.starts(&(CatenaFinitaCfg){.src = args->text + at, .other = s_words[word], .cap = size, .ci = 1}))
            {
                return 1;
            }
        }
    }
    return 0;
}

void cli_purpose_uncommented(const CliPurposeLinesCountCfg *args)
{
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        CliSpan rest =
            EMBED_CALL(cli_purpose_lines_span.purpose_trimmed_start, CliPurposeLinesSpanCfg, .span = args->lines[at]);
        if (EMBED_CALL(cli_purpose_lines_span.purpose_starts, CliPurposeLinesSpanCfg, .span = rest, .text = args->mark))
        {
            const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->mark, .cap = CLI_CELLS_POOL});
            rest = (CliSpan){rest.at + size, rest.length - size};
        }
        const unsigned long long marks = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg,
                                                    .text = rest.at, .at = 0ull, .size = rest.length, .set = "!/*#");
        rest = (CliSpan){rest.at + marks, rest.length - marks};
        rest = EMBED_CALL(cli_purpose_lines_span.purpose_trim, CliPurposeLinesSpanCfg, .span = rest);
        if (at != 0u)
        {
            mmgr_line_ch(args->out, '\n');
        }
        mmgr_line_put_n(args->out, rest.at, rest.length);
    }
}

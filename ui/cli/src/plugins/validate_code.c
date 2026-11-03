// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// validate_code.c: a file's code as the validation reads it: its comments and strings blanked, its words with their
// lines and columns, the theorems it declares, and the flagged words it uses
#include "../text/text.h"
#include "validate_internal.h"

// the most namespaces and sections open at once
#define CLI_VALIDATE_SPACES 256u

int cli_validate_code_get(const CliValidateTextCtx *args)
{
    char *const out = g_cli_validate->code;
    unsigned long long used = 0ULL;
    unsigned long long at = 0ULL;
    unsigned long long depth = 0ULL;
    int in_string = 0;
    int in_line = 0;
    // each letter is retained whole or blanked as one byte, so the code is never longer than the text
    if (args->size > CLI_VALIDATE_CODE)
    {
        return 0;
    }
    while (at < args->size)
    {
        unsigned int letter = 0u;
        const unsigned long long reserved = EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                                                       .text_bytes = (const unsigned char *)args->text,
                                                       .length = args->size, .at = at, .letter = &letter);
        const char c = args->text[at];
        const char next = (at + reserved < args->size) ? args->text[at + reserved] : '\0';
        const char blank = (c == '\n') ? '\n' : ' ';
        if (in_line)
        {
            in_line = (c != '\n');
            out[used] = blank;
            used += 1ULL;
        }
        else if (depth > 0ULL)
        {
            if (((c == '/') && (next == '-')) || ((c == '-') && (next == '/')))
            {
                depth = (c == '/') ? depth + 1ULL : depth - 1ULL;
                memor.cpy(&(MemoriaCfg){.dst = out + used, .src = "  ", .bytes = 2u});
                used += 2ULL;
                at += 2ULL;
                continue;
            }
            out[used] = blank;
            used += 1ULL;
        }
        else if (in_string && (c == '\\'))
        {
            out[used] = ' ';
            used += 1ULL;
            const int escapes = at + 1ULL < args->size;
            unsigned int escaped = 0u;
            const unsigned long long skipped =
                escapes ? EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                                     .text_bytes = (const unsigned char *)args->text, .length = args->size,
                                     .at = at + 1ULL, .letter = &escaped)
                        : 0ULL;
            if (escapes)
            {
                out[used] = (next == '\n') ? '\n' : ' ';
                used += 1ULL;
            }
            at += 1ULL + skipped;
            continue;
        }
        else if (in_string)
        {
            in_string = (c != '"');
            out[used] = blank;
            used += 1ULL;
        }
        else if ((c == '-') && (next == '-'))
        {
            in_line = 1;
            out[used] = ' ';
            used += 1ULL;
        }
        else if ((c == '/') && (next == '-'))
        {
            depth = 1ULL;
            memor.cpy(&(MemoriaCfg){.dst = out + used, .src = "  ", .bytes = 2u});
            used += 2ULL;
            at += 2ULL;
            continue;
        }
        else if (c == '"')
        {
            in_string = 1;
            out[used] = ' ';
            used += 1ULL;
        }
        else
        {
            memor.cpy(&(MemoriaCfg){.dst = out + used, .src = args->text + at, .bytes = reserved});
            used += reserved;
        }
        at += reserved;
    }
    g_cli_validate->code_size = used;
    return 1;
}

typedef struct
{
    unsigned int letter;
    const CliCodeWord *word_text;
    const char *with;
    const char *bare;
    unsigned long long size;
    const char *one;
    unsigned long long length;
} ValidateCodeCtx;
// whether a letter is whitespace, as Unicode has it
EMBED_INLINE int code_white(const ValidateCodeCtx *args)
{
    return ((args->letter >= 0x09u) && (args->letter <= 0x0du)) || (args->letter == 0x20u) || (args->letter == 0x85u) ||
           (args->letter == 0xa0u) || (args->letter == 0x1680u) ||
           ((args->letter >= 0x2000u) && (args->letter <= 0x200au)) || (args->letter == 0x2028u) ||
           (args->letter == 0x2029u) || (args->letter == 0x202fu) || (args->letter == 0x205fu) ||
           (args->letter == 0x3000u);
}

// whether a letter is one a word contains: an ASCII letter or digit, one of `_.'#!?`, or a letter past ASCII that is no
// whitespace and no angle bracket
EMBED_INLINE int code_word_letter(const ValidateCodeCtx *args)
{
    if (args->letter < 0x80u)
    {
        return ((args->letter >= 'a') && (args->letter <= 'z')) || ((args->letter >= 'A') && (args->letter <= 'Z')) ||
               ((args->letter >= '0') && (args->letter <= '9')) ||
               ((args->letter != 0u) && (args->letter < 0x80u) &&
                (cellul.chr(&(CatenaFinitaCfg){
                     .src = "_.'#!?", .cap = CLI_CELLS_POOL, .byte = (unsigned char)args->letter}) != NULL));
    }
    return !code_white(args) && (args->letter != 0x27e8u) && (args->letter != 0x27e9u);
}

void cli_validate_words_get(void)
{
    const char *const code = g_cli_validate->code;
    const unsigned long long size = g_cli_validate->code_size;
    g_cli_validate->word_count = 0u;
    unsigned int line = 0u;
    unsigned int col = 0u;
    unsigned long long at = 0ULL;
    while (at < size)
    {
        if (code[at] == '\n')
        {
            line += 1u;
            col = 0u;
            at += 1ULL;
            continue;
        }
        unsigned int letter = 0u;
        unsigned long long reserved =
            EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                       .text_bytes = (const unsigned char *)code, .length = size, .at = at, .letter = &letter);
        if (!EMBED_CALL(code_word_letter, ValidateCodeCtx, .letter = letter))
        {
            col += 1u;
            at += reserved;
            continue;
        }
        const unsigned long long start = at;
        const unsigned int start_col = col;
        while ((at < size) && EMBED_CALL(code_word_letter, ValidateCodeCtx, .letter = letter))
        {
            at += reserved;
            col += 1u;
            reserved = (at < size) ? EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                                                .text_bytes = (const unsigned char *)code, .length = size, .at = at,
                                                .letter = &letter)
                                   : 0ULL;
        }
        if (g_cli_validate->word_count < CLI_VALIDATE_WORDS)
        {
            g_cli_validate->words[g_cli_validate->word_count] =
                (CliCodeWord){start, (unsigned int)(at - start), line, start_col, col - start_col};
            g_cli_validate->word_count += 1u;
        }
    }
}

// whether a word is the text `with`
EMBED_INLINE int code_word_is(const ValidateCodeCtx *args)
{
    return (cellul.len(&(CatenaFinitaCfg){.src = args->with, .cap = CLI_CELLS_POOL}) == args->word_text->size) &&
           (memor.cmp(&(MemoriaCfg){.src = g_cli_validate->code + args->word_text->start,
                                    .other = args->with,
                                    .bytes = args->word_text->size}) == 0);
}

void cli_validate_theorems_in(void)
{
    const CliTool *const tool = &g_cli_validate->tool;
    const CliCodeWord *const words = g_cli_validate->words;
    const unsigned int count = g_cli_validate->word_count;
    // the open namespaces and sections, each a word of the code, or none for a section
    unsigned int spaces[CLI_VALIDATE_SPACES];
    unsigned int open = 0u;
    g_cli_validate->theorem_count = 0u;
    g_cli_validate->name_used = 0ULL;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CliCodeWord *const word_text = &words[at];
        const CliCodeWord *const next = (at + 1u < count) ? &words[at + 1u] : NULL;
        if (EMBED_CALL(code_word_is, ValidateCodeCtx, .word_text = word_text, .with = "namespace"))
        {
            if ((next != NULL) && (open < CLI_VALIDATE_SPACES))
            {
                spaces[open] = at + 1u;
                open += 1u;
            }
            continue;
        }
        if (EMBED_CALL(code_word_is, ValidateCodeCtx, .word_text = word_text, .with = "section") ||
            (EMBED_CALL(code_word_is, ValidateCodeCtx, .word_text = word_text, .with = "noncomputable") &&
             (next != NULL) && EMBED_CALL(code_word_is, ValidateCodeCtx, .word_text = next, .with = "section")))
        {
            if (EMBED_CALL(code_word_is, ValidateCodeCtx, .word_text = word_text, .with = "section") &&
                (open < CLI_VALIDATE_SPACES))
            {
                spaces[open] = CLI_JSON_NONE;
                open += 1u;
            }
            continue;
        }
        if (EMBED_CALL(code_word_is, ValidateCodeCtx, .word_text = word_text, .with = "end"))
        {
            open -= (open > 0u) ? 1u : 0u;
            continue;
        }
        if ((next == NULL) || !EMBED_CALL(cli_validate_json_internal.in_array, CliValidateJsonCtx,
                                          .json = &g_cli_validate->json, .array = tool->declarations,
                                          .text = g_cli_validate->code + word_text->start, .length = word_text->size))
        {
            continue;
        }
        // a name from the root is whole as it is written
        const char *name = g_cli_validate->code + next->start;
        unsigned long long name_size = next->size;
        const int rooted =
            (name_size >= 7ULL) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "_root_.", .bytes = 7u}) == 0);
        VerbaLinea full = {g_cli_validate->names + g_cli_validate->name_used,
                           CLI_VALIDATE_NAMES - g_cli_validate->name_used, 0ULL};
        for (unsigned int space = 0u; !rooted && (space < open); space += 1u)
        {
            if (spaces[space] != CLI_JSON_NONE)
            {
                const CliCodeWord *const named = &words[spaces[space]];
                mmgr_line_put_n(&full, g_cli_validate->code + named->start, named->size);
                mmgr_line_ch(&full, '.');
            }
        }
        mmgr_line_put_n(&full, rooted ? name + 7 : name, rooted ? name_size - 7ULL : name_size);
        if ((mmgr_line_finish(&full) == 0ULL) || (g_cli_validate->theorem_count == CLI_VALIDATE_THEOREMS))
        {
            continue;
        }
        g_cli_validate->theorems[g_cli_validate->theorem_count] = g_cli_validate->name_used;
        g_cli_validate->theorem_count += 1u;
        g_cli_validate->name_used += full.at + 1ULL;
    }
}

// whether a flagged word `one` is the word `bare`, or its last part after a dot
EMBED_INLINE int code_flag_hit(const ValidateCodeCtx *args)
{
    return ((args->size == args->length) &&
            (memor.cmp(&(MemoriaCfg){.src = args->bare, .other = args->one, .bytes = args->length}) == 0)) ||
           ((args->size > args->length) && (args->bare[args->size - args->length - 1ULL] == '.') &&
            (memor.cmp(&(MemoriaCfg){
                 .src = args->bare + args->size - args->length, .other = args->one, .bytes = args->length}) == 0));
}

void cli_validate_flags_in(void)
{
    const CliTool *const tool = &g_cli_validate->tool;
    const CliJson *const json = &g_cli_validate->json;
    for (unsigned int at = 0u; at < g_cli_validate->word_count; at += 1u)
    {
        const CliCodeWord *const word_text = &g_cli_validate->words[at];
        const char *bare = g_cli_validate->code + word_text->start;
        unsigned long long size = word_text->size;
        {
            const unsigned long long lead = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg,
                                                       .text = bare, .at = 0ULL, .size = size, .set = "@");
            bare += lead;
            size -= lead;
        }
        for (unsigned int flag = 0u; flag < tool->flag_count; flag += 1u)
        {
            const CliFlag *const flagged = &tool->flags[flag];
            int hit = 0;
            for (unsigned int index = 0u;
                 !hit && (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json,
                                     .array = flagged->words, .index = index) != CLI_JSON_NONE);
                 index += 1u)
            {
                unsigned long long length = 0ULL;
                const char *const one =
                    EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                               .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                  .json_read = json, .array = flagged->words, .index = index),
                               .length_pointer = &length);
                hit = (one != NULL) && EMBED_CALL(code_flag_hit, ValidateCodeCtx, .bare = bare, .size = size,
                                                  .one = one, .length = length);
            }
            if (!hit)
            {
                continue;
            }
            char message[4096];
            VerbaLinea line = {message, sizeof(message), 0ULL};
            mmgr_line_put_n(&line, bare, size);
            mmgr_line_put(&line, ": ");
            mmgr_line_put(&line, flagged->phrases);
            const CliFinding finding = {word_text->line,
                                        word_text->col,
                                        word_text->line,
                                        word_text->col + word_text->letters,
                                        flagged->severity,
                                        "flag",
                                        -1,
                                        0ULL,
                                        0ULL};
            EMBED_CALL(cli_validate_length_internal.finding_append, CliValidateLengthCtx, .finding = &finding,
                       .message_text = message, .length = line.at, .lifted = NULL);
            break;
        }
    }
}

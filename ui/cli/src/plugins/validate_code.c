// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// validate_code.c: a file's code as the validation reads it: its comments and strings blanked, its words with their
// lines and columns, the theorems it declares, and the flagged words it uses
#include "validate_internal.h"

// the most namespaces and sections open at once
#define CLI_VALIDATE_SPACES 256u

int cli_validate_code_of(const char *text, unsigned long long size)
{
    char *const out = g_cli_validate->code;
    unsigned long long used = 0ull;
    unsigned long long at = 0ull;
    unsigned long long depth = 0ull;
    int in_string = 0;
    int in_line = 0;
    // each letter is kept whole or blanked as one byte, so the code is never longer than the text
    if (size > CLI_VALIDATE_CODE)
    {
        return 0;
    }
    while (at < size)
    {
        unsigned int letter = 0u;
        const unsigned long long taken = cli_lossy_letter((const unsigned char *)text, size, at, &letter);
        const char c = text[at];
        const char next = (at + taken < size) ? text[at + taken] : '\0';
        const char blank = (c == '\n') ? '\n' : ' ';
        if (in_line)
        {
            in_line = (c != '\n');
            out[used] = blank;
            used += 1ull;
        }
        else if (depth > 0ull)
        {
            if (((c == '/') && (next == '-')) || ((c == '-') && (next == '/')))
            {
                depth = (c == '/') ? depth + 1ull : depth - 1ull;
                memor.cpy(&(MemoriaCfg){.dst = out + used, .src = "  ", .bytes = 2u});
                used += 2ull;
                at += 2ull;
                continue;
            }
            out[used] = blank;
            used += 1ull;
        }
        else if (in_string)
        {
            if (c == '\\')
            {
                out[used] = ' ';
                used += 1ull;
                unsigned long long skipped = 1ull;
                if (at + 1ull < size)
                {
                    unsigned int escaped = 0u;
                    skipped += cli_lossy_letter((const unsigned char *)text, size, at + 1ull, &escaped);
                    out[used] = (next == '\n') ? '\n' : ' ';
                    used += 1ull;
                }
                at += skipped;
                continue;
            }
            in_string = (c != '"');
            out[used] = blank;
            used += 1ull;
        }
        else if ((c == '-') && (next == '-'))
        {
            in_line = 1;
            out[used] = ' ';
            used += 1ull;
        }
        else if ((c == '/') && (next == '-'))
        {
            depth = 1ull;
            memor.cpy(&(MemoriaCfg){.dst = out + used, .src = "  ", .bytes = 2u});
            used += 2ull;
            at += 2ull;
            continue;
        }
        else if (c == '"')
        {
            in_string = 1;
            out[used] = ' ';
            used += 1ull;
        }
        else
        {
            memor.cpy(&(MemoriaCfg){.dst = out + used, .src = text + at, .bytes = taken});
            used += taken;
        }
        at += taken;
    }
    g_cli_validate->code_size = used;
    return 1;
}

// whether a letter is whitespace, as Unicode has it
static int code_white(unsigned int letter)
{
    return ((letter >= 0x09u) && (letter <= 0x0du)) || (letter == 0x20u) || (letter == 0x85u) || (letter == 0xa0u) ||
           (letter == 0x1680u) || ((letter >= 0x2000u) && (letter <= 0x200au)) || (letter == 0x2028u) ||
           (letter == 0x2029u) || (letter == 0x202fu) || (letter == 0x205fu) || (letter == 0x3000u);
}

// whether a letter is one a word holds: an ASCII letter or digit, one of `_.'#!?`, or a letter past ASCII that is no
// whitespace and no angle bracket
static int code_word_letter(unsigned int letter)
{
    if (letter < 0x80u)
    {
        return ((letter >= 'a') && (letter <= 'z')) || ((letter >= 'A') && (letter <= 'Z')) ||
               ((letter >= '0') && (letter <= '9')) ||
               (cellul.chr(&(CatenaFinitaCfg){.src = "_.'#!?", .cap = CLI_CELLS_POOL, .byte = (int)letter}) != NULL &&
                letter != 0u);
    }
    return !code_white(letter) && (letter != 0x27e8u) && (letter != 0x27e9u);
}

void cli_validate_words_of(void)
{
    const char *const code = g_cli_validate->code;
    const unsigned long long size = g_cli_validate->code_size;
    g_cli_validate->word_count = 0u;
    unsigned int line = 0u;
    unsigned int col = 0u;
    unsigned long long at = 0ull;
    while (at < size)
    {
        if (code[at] == '\n')
        {
            line += 1u;
            col = 0u;
            at += 1ull;
            continue;
        }
        unsigned int letter = 0u;
        unsigned long long taken = cli_lossy_letter((const unsigned char *)code, size, at, &letter);
        if (!code_word_letter(letter))
        {
            col += 1u;
            at += taken;
            continue;
        }
        const unsigned long long start = at;
        const unsigned int start_col = col;
        while ((at < size) && code_word_letter(letter))
        {
            at += taken;
            col += 1u;
            taken = (at < size) ? cli_lossy_letter((const unsigned char *)code, size, at, &letter) : 0ull;
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
static int code_word_is(const CliCodeWord *word, const char *with)
{
    return (cellul.len(&(CatenaFinitaCfg){.src = with, .cap = CLI_CELLS_POOL}) == word->size) &&
           (memor.cmp(&(MemoriaCfg){.src = g_cli_validate->code + word->start, .other = with, .bytes = word->size}) ==
            0);
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
    g_cli_validate->name_used = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CliCodeWord *const word = &words[at];
        const CliCodeWord *const next = (at + 1u < count) ? &words[at + 1u] : NULL;
        if (code_word_is(word, "namespace"))
        {
            if ((next != NULL) && (open < CLI_VALIDATE_SPACES))
            {
                spaces[open] = at + 1u;
                open += 1u;
            }
            continue;
        }
        if (code_word_is(word, "section") ||
            (code_word_is(word, "noncomputable") && (next != NULL) && code_word_is(next, "section")))
        {
            if (code_word_is(word, "section") && (open < CLI_VALIDATE_SPACES))
            {
                spaces[open] = CLI_JSON_NONE;
                open += 1u;
            }
            continue;
        }
        if (code_word_is(word, "end"))
        {
            open -= (open > 0u) ? 1u : 0u;
            continue;
        }
        if ((next == NULL) || !cli_validate_holds(&g_cli_validate->json, tool->declarations,
                                                  g_cli_validate->code + word->start, word->size))
        {
            continue;
        }
        // a name from the root is whole as it is written
        const char *name = g_cli_validate->code + next->start;
        unsigned long long name_size = next->size;
        const int rooted =
            (name_size >= 7ull) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "_root_.", .bytes = 7u}) == 0);
        CliLine full = {g_cli_validate->names + g_cli_validate->name_used,
                        CLI_VALIDATE_NAMES - g_cli_validate->name_used, 0ull};
        for (unsigned int space = 0u; !rooted && (space < open); space += 1u)
        {
            if (spaces[space] != CLI_JSON_NONE)
            {
                const CliCodeWord *const named = &words[spaces[space]];
                cli_line_counted(&full, g_cli_validate->code + named->start, named->size);
                cli_line_character(&full, '.');
            }
        }
        cli_line_counted(&full, rooted ? name + 7 : name, rooted ? name_size - 7ull : name_size);
        if ((cli_line_closed(&full) == 0ull) || (g_cli_validate->theorem_count == CLI_VALIDATE_THEOREMS))
        {
            continue;
        }
        g_cli_validate->theorems[g_cli_validate->theorem_count] = g_cli_validate->name_used;
        g_cli_validate->theorem_count += 1u;
        g_cli_validate->name_used += full.at + 1ull;
    }
}

// whether a flagged word `one` is the word `bare`, or its last part after a dot
static int code_flag_hit(const char *bare, unsigned long long size, const char *one, unsigned long long length)
{
    return ((size == length) && (memor.cmp(&(MemoriaCfg){.src = bare, .other = one, .bytes = length}) == 0)) ||
           ((size > length) && (bare[size - length - 1ull] == '.') &&
            (memor.cmp(&(MemoriaCfg){.src = bare + size - length, .other = one, .bytes = length}) == 0));
}

void cli_validate_flags_in(void)
{
    const CliTool *const tool = &g_cli_validate->tool;
    const CliJson *const json = &g_cli_validate->json;
    for (unsigned int at = 0u; at < g_cli_validate->word_count; at += 1u)
    {
        const CliCodeWord *const word = &g_cli_validate->words[at];
        const char *bare = g_cli_validate->code + word->start;
        unsigned long long size = word->size;
        while ((size > 0ull) && (bare[0] == '@'))
        {
            bare += 1;
            size -= 1ull;
        }
        for (unsigned int flag = 0u; flag < tool->flag_count; flag += 1u)
        {
            const CliFlag *const flagged = &tool->flags[flag];
            int hit = 0;
            for (unsigned int index = 0u; !hit && (cli_json_item(json, flagged->words, index) != CLI_JSON_NONE);
                 index += 1u)
            {
                unsigned long long length = 0ull;
                const char *const one = cli_json_string(json, cli_json_item(json, flagged->words, index), &length);
                hit = (one != NULL) && code_flag_hit(bare, size, one, length);
            }
            if (!hit)
            {
                continue;
            }
            char message[4096];
            CliLine line = {message, sizeof(message), 0ull};
            cli_line_counted(&line, bare, size);
            cli_line_text(&line, ": ");
            cli_line_text(&line, flagged->says);
            const CliFinding finding = {
                word->line, word->col, word->line, word->col + word->letters, flagged->severity, "flag",
                -1,         0ull,      0ull};
            cli_validate_finding_put(&finding, message, line.at, NULL);
            break;
        }
    }
}

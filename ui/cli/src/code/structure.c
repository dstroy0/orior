// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// structure.c: a text's tokens line by line, the shortest edit between two sequences, and each token and line numbered
// by its text
#include "../text/regexp.h"
#include "structure_internal.h"

typedef struct
{
    const char *at;
    const char *end;
    unsigned int *bytes;
    unsigned int letter;
    const unsigned int *a;
    int n;
    const unsigned int *b;
    int m;
    CliStep *steps;
    unsigned int capacity;
    const CliSide *one;
    unsigned int at_u32;
    const CliSide *other;
    unsigned int other_at;
    unsigned int line;
    unsigned int other_line;
    CliSide *side;
    unsigned int *names;
    unsigned int *line_names;
} StructureCtx;

// the letter of UTF-8 at `at` before `end`, its bytes into `bytes`: a byte that starts no letter is U+FFFD, one byte
EMBED_INLINE unsigned int cli_letter(const StructureCtx *args)
{
    unsigned int *bytes = args->bytes;
    const unsigned char lead = (unsigned char)args->at[0];
    unsigned int length;
    if (lead < 0x80u)
    {
        length = 1u;
    }
    else if ((lead >> 5u) == 6u)
    {
        length = 2u;
    }
    else if ((lead >> 4u) == 14u)
    {
        length = 3u;
    }
    else if ((lead >> 3u) == 30u)
    {
        length = 4u;
    }
    else
    {
        length = 0u;
    }
    if ((length == 0u) || ((unsigned long long)(args->end - args->at) < length))
    {
        *bytes = 1u;
        return (length == 1u) ? lead : 0xfffdu;
    }
    static const unsigned int s_prefix_bits[5] = {0x00u, 0x7fu, 0x1fu, 0x0fu, 0x07u};
    unsigned int letter = lead & s_prefix_bits[length];
    for (unsigned int step = 1u; step < length; step += 1u)
    {
        const unsigned char next = (unsigned char)args->at[step];
        if ((next >> 6u) != 2u)
        {
            *bytes = 1u;
            return 0xfffdu;
        }
        letter = (letter << 6u) | (next & 0x3fu);
    }
    *bytes = length;
    return letter;
}

// how many UTF-16 units a letter takes
EMBED_INLINE unsigned int cli_units(const StructureCtx *args)
{
    return (args->letter >= 0x10000u) ? 2u : 1u;
}

EMBED_INLINE int cli_is_whitespace(const StructureCtx *args)
{
    return ((args->letter >= 0x09u) && (args->letter <= 0x0du)) || (args->letter == 0x20u) || (args->letter == 0x85u) ||
           (args->letter == 0xa0u) || (args->letter == 0x1680u) ||
           ((args->letter >= 0x2000u) && (args->letter <= 0x200au)) || (args->letter == 0x2028u) ||
           (args->letter == 0x2029u) || (args->letter == 0x202fu) || (args->letter == 0x205fu) ||
           (args->letter == 0x3000u);
}

EMBED_INLINE int cli_starts_name_letter(const StructureCtx *args)
{
    unsigned int letter = args->letter;
    return EMBED_CALL(cli_regexp_letter.property, CliRegexpLetterCfg, .property = CLI_REGEXP_LETTER,
                      .letter = letter) ||
           (letter == '_') || (letter == '$');
}

EMBED_INLINE int cli_alphanumeric(const StructureCtx *args)
{
    unsigned int letter = args->letter;
    return EMBED_CALL(cli_regexp_letter.property, CliRegexpLetterCfg, .property = CLI_REGEXP_LETTER,
                      .letter = letter) ||
           EMBED_CALL(cli_regexp_letter.property, CliRegexpLetterCfg, .property = CLI_REGEXP_NUMBER, .letter = letter);
}

// what a token continues taking after its first letter: a name's letters, a number's, or a string's to its closing
// quote
enum
{
    CLI_ALLOC_NAME,
    CLI_ALLOC_NUMBER,
    CLI_ALLOC_STRING,
    CLI_ALLOC_SIGN
};

int cli_structure_lex(const CliStructureTextCtx *args)
{
    const char *text = args->text;
    const char *const end = text + args->size;
    const char *at = text;
    unsigned int line = 0u;
    unsigned int column = 0u;
    args->side->text = text;
    args->side->tokens = 0u;
    args->side->lines = 1u;
    args->side->line_first[0] = 0u;
    while (at < end)
    {
        unsigned int bytes = 0u;
        const unsigned int letter = EMBED_CALL(cli_letter, StructureCtx, .at = at, .end = end, .bytes = &bytes);
        if (letter == '\n')
        {
            line += 1u;
            column = 0u;
            if (args->side->lines >= CLI_STRUCTURE_LINES)
            {
                return 0;
            }
            args->side->line_first[args->side->lines] = args->side->tokens;
            args->side->lines += 1u;
            at += bytes;
            continue;
        }
        if (EMBED_CALL(cli_is_whitespace, StructureCtx, .letter = letter))
        {
            column += EMBED_CALL(cli_units, StructureCtx, .letter = letter);
            at += bytes;
            continue;
        }
        if (args->side->tokens >= CLI_STRUCTURE_TOKENS)
        {
            return 0;
        }
        const unsigned int from = column;
        // a place in the text below what a side contains, which an unsigned int counts
        const unsigned int start = (unsigned int)(at - text);
        column += EMBED_CALL(cli_units, StructureCtx, .letter = letter);
        at += bytes;
        unsigned int take;
        if (EMBED_CALL(cli_starts_name_letter, StructureCtx, .letter = letter))
        {
            take = CLI_ALLOC_NAME;
        }
        else if ((letter >= '0') && (letter <= '9'))
        {
            take = CLI_ALLOC_NUMBER;
        }
        else if ((letter == '"') || (letter == '\'') || (letter == '`'))
        {
            take = CLI_ALLOC_STRING;
        }
        else
        {
            take = CLI_ALLOC_SIGN;
        }
        int escaped = 0;
        int closed = 0;
        while ((take != CLI_ALLOC_SIGN) && (at < end))
        {
            unsigned int next_bytes = 0u;
            const unsigned int next = EMBED_CALL(cli_letter, StructureCtx, .at = at, .end = end, .bytes = &next_bytes);
            int continues = next != '\n';
            if (continues && (take == CLI_ALLOC_NAME))
            {
                continues =
                    EMBED_CALL(cli_alphanumeric, StructureCtx, .letter = next) || (next == '_') || (next == '$');
            }
            else if (continues && (take == CLI_ALLOC_NUMBER))
            {
                continues =
                    EMBED_CALL(cli_alphanumeric, StructureCtx, .letter = next) || (next == '.') || (next == '_');
            }
            else if (continues)
            {
                continues = !closed;
                closed = closed || (!escaped && (next == letter));
                escaped = !escaped && (next == '\\');
            }
            if (!continues)
            {
                break;
            }
            column += EMBED_CALL(cli_units, StructureCtx, .letter = next);
            at += next_bytes;
        }
        // as above, a place in the text a side contains
        const CliToken token = {line, from, column, start, (unsigned int)(at - text)};
        args->side->token[args->side->tokens] = token;
        args->side->tokens += 1u;
    }
    args->side->line_first[args->side->lines] = args->side->tokens;
    return 1;
}

// Myers' shortest edit. It stores one band of its frontier for each edit, and what it stores grows with the edits and
// not with the sequences, in the compare's capacity

EMBED_INLINE unsigned int cli_myers(const StructureCtx *args)
{
    const int most = ((args->n + args->m) < (int)CLI_MAX_EDITS) ? (args->n + args->m) : (int)CLI_MAX_EDITS;
    const int middle = most + 1;
    memor.set(&(MemoriaCfg){.dst = g_cli_structure->frontier,
                            .val = 0,
                            .bytes = sizeof(g_cli_structure->frontier[0]) * (size_t)((2 * most) + 3)});
    int found = -1;
    {
        int d = 0;
        while ((found < 0) && (d <= most))
        {
            // the frontier before this edit stored, d squared on, the band from middle - d to middle + d
            memor.cpy(&(MemoriaCfg){.dst = g_cli_structure->trace + (d * d),
                                    .src = g_cli_structure->frontier + (middle - d),
                                    .bytes = sizeof(int) * (size_t)((2 * d) + 1)});
            int k = -d;
            while ((found < 0) && (k <= d))
            {
                const int at = middle + k;
                int x =
                    ((k == -d) || ((k != d) && (g_cli_structure->frontier[at - 1] < g_cli_structure->frontier[at + 1])))
                        ? g_cli_structure->frontier[at + 1]
                        : (g_cli_structure->frontier[at - 1] + 1);
                int y = x - k;
                while ((x < args->n) && (y < args->m) && (args->a[x] == args->b[y]))
                {
                    x += 1;
                    y += 1;
                }
                g_cli_structure->frontier[at] = x;
                if ((x >= args->n) && (y >= args->m))
                {
                    found = d;
                }
                k += 2;
            }
            d += 1;
        }
    }
    if (found < 0)
    {
        return CLI_STRUCTURE_NONE;
    }
    // the steps taken backward from the end, then turned about
    unsigned int count = 0u;
    int x = args->n;
    int y = args->m;
    for (int d = found; d >= 1; d -= 1)
    {
        const int *const before = g_cli_structure->trace + (d * d);
        const int k = x - y;
        const int down = (k == -d) || ((k != d) && (before[k - 1 + d] < before[k + 1 + d]));
        const int back_k = down ? (k + 1) : (k - 1);
        const int back_x = before[back_k + d];
        const int back_y = back_x - back_k;
        while ((x > back_x) && (y > back_y) && (count < args->capacity))
        {
            x -= 1;
            y -= 1;
            // the places are inside the sequences, which an unsigned int counts
            const CliStep step = {CLI_STEP_EQUAL, (unsigned int)x, (unsigned int)y};
            args->steps[count] = step;
            count += 1u;
        }
        if (count >= args->capacity)
        {
            return CLI_STRUCTURE_NONE;
        }
        if (down)
        {
            y -= 1;
            const CliStep step = {CLI_STEP_IN, 0u, (unsigned int)y};
            args->steps[count] = step;
        }
        else
        {
            x -= 1;
            const CliStep step = {CLI_STEP_OUT, (unsigned int)x, 0u};
            args->steps[count] = step;
        }
        count += 1u;
    }
    while ((x > 0) && (y > 0) && (count < args->capacity))
    {
        x -= 1;
        y -= 1;
        const CliStep step = {CLI_STEP_EQUAL, (unsigned int)x, (unsigned int)y};
        args->steps[count] = step;
        count += 1u;
    }
    for (unsigned int low = 0u; (count != 0u) && (low < (count - 1u - low)); low += 1u)
    {
        const CliStep pivot = args->steps[low];
        args->steps[low] = args->steps[count - 1u - low];
        args->steps[count - 1u - low] = pivot;
    }
    return count;
}

unsigned int cli_structure_shortest(const CliStructureACtx *args)
{
    unsigned int head = 0u;
    while ((head < args->n) && (head < args->m) && (args->a[head] == args->b[head]))
    {
        head += 1u;
    }
    unsigned int tail = 0u;
    while (((head + tail) < args->n) && ((head + tail) < args->m) &&
           (args->a[args->n - 1u - tail] == args->b[args->m - 1u - tail]))
    {
        tail += 1u;
    }
    if ((args->n + args->m) > args->capacity)
    {
        return CLI_STRUCTURE_NONE;
    }
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < head; at += 1u)
    {
        const CliStep step = {CLI_STEP_EQUAL, at, at};
        args->steps[count] = step;
        count += 1u;
    }
    // the sequences are below what a side contains, which an int counts
    const unsigned int core =
        EMBED_CALL(cli_myers, StructureCtx, .a = args->a + head, .n = (int)(args->n - head - tail), .b = args->b + head,
                   .m = (int)(args->m - head - tail), .steps = args->steps + count, .capacity = args->capacity - count);
    if (core == CLI_STRUCTURE_NONE)
    {
        return core;
    }
    for (unsigned int at = count; at < (count + core); at += 1u)
    {
        args->steps[at].x += head;
        args->steps[at].y += head;
    }
    count += core;
    for (unsigned int back = 0u; back < tail; back += 1u)
    {
        const CliStep step = {CLI_STEP_EQUAL, args->n - tail + back, args->m - tail + back};
        args->steps[count] = step;
        count += 1u;
    }
    return count;
}

// whether two tokens' texts are the same
EMBED_INLINE int cli_token_same(const StructureCtx *args)
{
    const CliSide *other = args->other;
    const CliToken *const left = &args->one->token[args->at_u32];
    const CliToken *const right = &other->token[args->other_at];
    return ((left->end - left->start) == (right->end - right->start)) &&
           (memor.cmp(&(MemoriaCfg){.src = args->one->text + left->start,
                                    .other = other->text + right->start,
                                    .bytes = left->end - left->start}) == 0);
}

// whether two lines contain tokens of the same texts, one for one
EMBED_INLINE int cli_line_same(const StructureCtx *args)
{
    const unsigned int count = args->one->line_first[args->line + 1u] - args->one->line_first[args->line];
    if ((args->other->line_first[args->other_line + 1u] - args->other->line_first[args->other_line]) != count)
    {
        return 0;
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (args->one->token_name[args->one->line_first[args->line] + at] !=
            args->other->token_name[args->other->line_first[args->other_line] + at])
        {
            return 0;
        }
    }
    return 1;
}

// a side's tokens, then its lines, each given the number of its text, a new one where the text is new
static int cli_side_by_name(const StructureCtx *args)
{
    unsigned int *names = args->names;
    unsigned int *line_names = args->line_names;
    for (unsigned int at = 0u; at < args->side->tokens; at += 1u)
    {
        const CliToken *const token = &args->side->token[at];
        unsigned long long hash = 0xcbf29ce484222325ULL;
        for (unsigned int byte = token->start; byte < token->end; byte += 1u)
        {
            hash = (hash ^ (unsigned char)args->side->text[byte]) * 0x100000001b3ULL;
        }
        unsigned long long bucket = hash & (CLI_STRUCTURE_NAMES - 1u);
        while ((g_cli_structure->token_names[bucket].name != 0u) &&
               !EMBED_CALL(cli_token_same, StructureCtx, .one = g_cli_structure->token_names[bucket].side,
                           .at_u32 = g_cli_structure->token_names[bucket].at, .other = args->side, .other_at = at))
        {
            bucket = (bucket + 1u) & (CLI_STRUCTURE_NAMES - 1u);
        }
        if (g_cli_structure->token_names[bucket].name == 0u)
        {
            if (*names >= (CLI_STRUCTURE_NAMES / 2u))
            {
                return 0;
            }
            *names += 1u;
            const CliNameBucket made = {args->side, at, *names};
            g_cli_structure->token_names[bucket] = made;
        }
        args->side->token_name[at] = g_cli_structure->token_names[bucket].name;
    }
    for (unsigned int line = 0u; line < args->side->lines; line += 1u)
    {
        unsigned long long hash = 0xcbf29ce484222325ULL;
        for (unsigned int at = args->side->line_first[line]; at < args->side->line_first[line + 1u]; at += 1u)
        {
            hash = (hash ^ args->side->token_name[at]) * 0x100000001b3ULL;
        }
        unsigned long long bucket = hash & (CLI_STRUCTURE_NAMES - 1u);
        while ((g_cli_structure->line_names[bucket].name != 0u) &&
               !EMBED_CALL(cli_line_same, StructureCtx, .one = g_cli_structure->line_names[bucket].side,
                           .line = g_cli_structure->line_names[bucket].at, .other = args->side, .other_line = line))
        {
            bucket = (bucket + 1u) & (CLI_STRUCTURE_NAMES - 1u);
        }
        if (g_cli_structure->line_names[bucket].name == 0u)
        {
            if (*line_names >= (CLI_STRUCTURE_NAMES / 2u))
            {
                return 0;
            }
            *line_names += 1u;
            const CliNameBucket made = {args->side, line, *line_names};
            g_cli_structure->line_names[bucket] = made;
        }
        args->side->line_name[line] = g_cli_structure->line_names[bucket].name;
    }
    return 1;
}

int cli_structure_by_name(const CliStructureOldCtx *args)
{
    memor.set(
        &(MemoriaCfg){.dst = g_cli_structure->token_names, .val = 0, .bytes = sizeof(g_cli_structure->token_names)});
    memor.set(
        &(MemoriaCfg){.dst = g_cli_structure->line_names, .val = 0, .bytes = sizeof(g_cli_structure->line_names)});
    unsigned int names = 0u;
    unsigned int line_names = 0u;
    return EMBED_CALL(cli_side_by_name, StructureCtx, .side = args->old, .names = &names, .line_names = &line_names) &&
           EMBED_CALL(cli_side_by_name, StructureCtx, .side = args->now, .names = &names, .line_names = &line_names);
}

int cli_structure_starts_name(const CliStructureSideCtx *args)
{
    const char *const at = args->side_read->text + args->side_read->token[args->token].start;
    unsigned int bytes = 0u;
    return EMBED_CALL(cli_starts_name_letter, StructureCtx,
                      .letter = EMBED_CALL(cli_letter, StructureCtx, .at = at,
                                           .end = args->side_read->text + args->side_read->token[args->token].end,
                                           .bytes = &bytes));
}

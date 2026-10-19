// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// structure.c: a text's tokens line by line, the shortest edit between two sequences, and each token and line numbered
// by its text
#include "../text/regexp.h"
#include "structure_internal.h"

// the letter of UTF-8 at `at` before `end`, its bytes into `bytes`: a byte that starts no letter is U+FFFD, one byte
static unsigned int cli_letter(const char *at, const char *end, unsigned int *bytes)
{
    const unsigned char lead = (unsigned char)at[0];
    const unsigned int length = (lead < 0x80u)          ? 1u
                                : ((lead >> 5u) == 6u)  ? 2u
                                : ((lead >> 4u) == 14u) ? 3u
                                : ((lead >> 3u) == 30u) ? 4u
                                                        : 0u;
    if ((length == 0u) || ((unsigned long long)(end - at) < length))
    {
        *bytes = 1u;
        return (length == 1u) ? lead : 0xfffdu;
    }
    static const unsigned int s_lead_bits[5] = {0x00u, 0x7fu, 0x1fu, 0x0fu, 0x07u};
    unsigned int letter = lead & s_lead_bits[length];
    for (unsigned int step = 1u; step < length; step += 1u)
    {
        const unsigned char next = (unsigned char)at[step];
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
static unsigned int cli_units(unsigned int letter)
{
    return (letter >= 0x10000u) ? 2u : 1u;
}

static int cli_is_whitespace(unsigned int letter)
{
    return ((letter >= 0x09u) && (letter <= 0x0du)) || (letter == 0x20u) || (letter == 0x85u) || (letter == 0xa0u) ||
           (letter == 0x1680u) || ((letter >= 0x2000u) && (letter <= 0x200au)) || (letter == 0x2028u) ||
           (letter == 0x2029u) || (letter == 0x202fu) || (letter == 0x205fu) || (letter == 0x3000u);
}

static int cli_starts_name_letter(unsigned int letter)
{
    return cli_regexp_property(CLI_REGEXP_LETTER, letter) || (letter == '_') || (letter == '$');
}

static int cli_alphanumeric(unsigned int letter)
{
    return cli_regexp_property(CLI_REGEXP_LETTER, letter) || cli_regexp_property(CLI_REGEXP_NUMBER, letter);
}

// what a token keeps taking after its first letter: a name's letters, a number's, or a string's to its closing quote
enum
{
    CLI_TAKE_NAME,
    CLI_TAKE_NUMBER,
    CLI_TAKE_STRING,
    CLI_TAKE_SIGN
};

int cli_structure_lex(const char *text, unsigned long long size, CliSide *side)
{
    const char *const end = text + size;
    const char *at = text;
    unsigned int line = 0u;
    unsigned int column = 0u;
    side->text = text;
    side->tokens = 0u;
    side->lines = 1u;
    side->line_first[0] = 0u;
    while (at < end)
    {
        unsigned int bytes = 0u;
        const unsigned int letter = cli_letter(at, end, &bytes);
        if (letter == '\n')
        {
            line += 1u;
            column = 0u;
            if (side->lines >= CLI_STRUCTURE_LINES)
            {
                return 0;
            }
            side->line_first[side->lines] = side->tokens;
            side->lines += 1u;
            at += bytes;
            continue;
        }
        if (cli_is_whitespace(letter))
        {
            column += cli_units(letter);
            at += bytes;
            continue;
        }
        if (side->tokens >= CLI_STRUCTURE_TOKENS)
        {
            return 0;
        }
        const unsigned int from = column;
        // a place in the text below what a side holds, which an unsigned int counts
        const unsigned int start = (unsigned int)(at - text);
        column += cli_units(letter);
        at += bytes;
        const unsigned int take = cli_starts_name_letter(letter)                             ? CLI_TAKE_NAME
                                  : ((letter >= '0') && (letter <= '9'))                     ? CLI_TAKE_NUMBER
                                  : ((letter == '"') || (letter == '\'') || (letter == '`')) ? CLI_TAKE_STRING
                                                                                             : CLI_TAKE_SIGN;
        int escaped = 0;
        int closed = 0;
        while ((take != CLI_TAKE_SIGN) && (at < end))
        {
            unsigned int next_bytes = 0u;
            const unsigned int next = cli_letter(at, end, &next_bytes);
            int keep = next != '\n';
            if (keep && (take == CLI_TAKE_NAME))
            {
                keep = cli_alphanumeric(next) || (next == '_') || (next == '$');
            }
            else if (keep && (take == CLI_TAKE_NUMBER))
            {
                keep = cli_alphanumeric(next) || (next == '.') || (next == '_');
            }
            else if (keep)
            {
                keep = !closed;
                closed = closed || (!escaped && (next == letter));
                escaped = !escaped && (next == '\\');
            }
            if (!keep)
            {
                break;
            }
            column += cli_units(next);
            at += next_bytes;
        }
        // as above, a place in the text a side holds
        const CliToken token = {line, from, column, start, (unsigned int)(at - text)};
        side->token[side->tokens] = token;
        side->tokens += 1u;
    }
    side->line_first[side->lines] = side->tokens;
    return 1;
}

// Myers' shortest edit. It keeps one band of its frontier for each edit, and what it keeps grows with the edits and not
// with the sequences, in the compare's room

static unsigned int cli_myers(const unsigned int *a, int n, const unsigned int *b, int m, CliStep *steps,
                              unsigned int room)
{
    const int most = ((n + m) < (int)CLI_MOST_EDITS) ? (n + m) : (int)CLI_MOST_EDITS;
    const int middle = most + 1;
    memor.set(&(MemoriaCfg){.dst = g_cli_structure->frontier,
                            .val = 0,
                            .bytes = sizeof(g_cli_structure->frontier[0]) * (size_t)((2 * most) + 3)});
    int found = -1;
    for (int d = 0; (found < 0) && (d <= most); d += 1)
    {
        // the frontier before this edit kept, d squared on, the band from middle - d to middle + d
        memor.cpy(&(MemoriaCfg){.dst = g_cli_structure->trace + (d * d),
                                .src = g_cli_structure->frontier + (middle - d),
                                .bytes = sizeof(int) * (size_t)((2 * d) + 1)});
        for (int k = -d; (found < 0) && (k <= d); k += 2)
        {
            const int at = middle + k;
            int x = ((k == -d) || ((k != d) && (g_cli_structure->frontier[at - 1] < g_cli_structure->frontier[at + 1])))
                        ? g_cli_structure->frontier[at + 1]
                        : (g_cli_structure->frontier[at - 1] + 1);
            int y = x - k;
            while ((x < n) && (y < m) && (a[x] == b[y]))
            {
                x += 1;
                y += 1;
            }
            g_cli_structure->frontier[at] = x;
            if ((x >= n) && (y >= m))
            {
                found = d;
            }
        }
    }
    if (found < 0)
    {
        return CLI_STRUCTURE_NONE;
    }
    // the steps taken backward from the end, then turned about
    unsigned int count = 0u;
    int x = n;
    int y = m;
    for (int d = found; d >= 1; d -= 1)
    {
        const int *const before = g_cli_structure->trace + (d * d);
        const int k = x - y;
        const int down = (k == -d) || ((k != d) && (before[k - 1 + d] < before[k + 1 + d]));
        const int back_k = down ? (k + 1) : (k - 1);
        const int back_x = before[back_k + d];
        const int back_y = back_x - back_k;
        while ((x > back_x) && (y > back_y) && (count < room))
        {
            x -= 1;
            y -= 1;
            // the places are inside the sequences, which an unsigned int counts
            const CliStep step = {CLI_STEP_KEEP, (unsigned int)x, (unsigned int)y};
            steps[count] = step;
            count += 1u;
        }
        if (count >= room)
        {
            return CLI_STRUCTURE_NONE;
        }
        if (down)
        {
            y -= 1;
            const CliStep step = {CLI_STEP_IN, 0u, (unsigned int)y};
            steps[count] = step;
        }
        else
        {
            x -= 1;
            const CliStep step = {CLI_STEP_OUT, (unsigned int)x, 0u};
            steps[count] = step;
        }
        count += 1u;
    }
    while ((x > 0) && (y > 0) && (count < room))
    {
        x -= 1;
        y -= 1;
        const CliStep step = {CLI_STEP_KEEP, (unsigned int)x, (unsigned int)y};
        steps[count] = step;
        count += 1u;
    }
    for (unsigned int low = 0u; (count != 0u) && (low < (count - 1u - low)); low += 1u)
    {
        const CliStep held = steps[low];
        steps[low] = steps[count - 1u - low];
        steps[count - 1u - low] = held;
    }
    return count;
}

unsigned int cli_structure_shortest(const unsigned int *a, unsigned int n, const unsigned int *b, unsigned int m,
                                    CliStep *steps, unsigned int room)
{
    unsigned int head = 0u;
    while ((head < n) && (head < m) && (a[head] == b[head]))
    {
        head += 1u;
    }
    unsigned int tail = 0u;
    while (((head + tail) < n) && ((head + tail) < m) && (a[n - 1u - tail] == b[m - 1u - tail]))
    {
        tail += 1u;
    }
    if ((n + m) > room)
    {
        return CLI_STRUCTURE_NONE;
    }
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < head; at += 1u)
    {
        const CliStep step = {CLI_STEP_KEEP, at, at};
        steps[count] = step;
        count += 1u;
    }
    // the sequences are below what a side holds, which an int counts
    const unsigned int core =
        cli_myers(a + head, (int)(n - head - tail), b + head, (int)(m - head - tail), steps + count, room - count);
    if (core == CLI_STRUCTURE_NONE)
    {
        return core;
    }
    for (unsigned int at = count; at < (count + core); at += 1u)
    {
        steps[at].x += head;
        steps[at].y += head;
    }
    count += core;
    for (unsigned int back = 0u; back < tail; back += 1u)
    {
        const CliStep step = {CLI_STEP_KEEP, n - tail + back, m - tail + back};
        steps[count] = step;
        count += 1u;
    }
    return count;
}

// whether two tokens' texts are the same
static int cli_token_same(const CliSide *one, unsigned int at, const CliSide *other, unsigned int other_at)
{
    const CliToken *const left = &one->token[at];
    const CliToken *const right = &other->token[other_at];
    return ((left->end - left->start) == (right->end - right->start)) &&
           (memor.cmp(&(MemoriaCfg){.src = one->text + left->start,
                                    .other = other->text + right->start,
                                    .bytes = left->end - left->start}) == 0);
}

// whether two lines hold tokens of the same texts, one for one
static int cli_line_same(const CliSide *one, unsigned int line, const CliSide *other, unsigned int other_line)
{
    const unsigned int count = one->line_first[line + 1u] - one->line_first[line];
    if ((other->line_first[other_line + 1u] - other->line_first[other_line]) != count)
    {
        return 0;
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (one->token_name[one->line_first[line] + at] != other->token_name[other->line_first[other_line] + at])
        {
            return 0;
        }
    }
    return 1;
}

// a side's tokens, then its lines, each given the number of its text, a new one where the text is new
static int cli_side_named(CliSide *side, unsigned int *names, unsigned int *line_names)
{
    for (unsigned int at = 0u; at < side->tokens; at += 1u)
    {
        const CliToken *const token = &side->token[at];
        unsigned long long hash = 0xcbf29ce484222325ull;
        for (unsigned int byte = token->start; byte < token->end; byte += 1u)
        {
            hash = (hash ^ (unsigned char)side->text[byte]) * 0x100000001b3ull;
        }
        unsigned long long bucket = hash & (CLI_STRUCTURE_NAMES - 1u);
        while ((g_cli_structure->token_names[bucket].name != 0u) &&
               !cli_token_same(g_cli_structure->token_names[bucket].side, g_cli_structure->token_names[bucket].at, side,
                               at))
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
            const CliNameBucket made = {side, at, *names};
            g_cli_structure->token_names[bucket] = made;
        }
        side->token_name[at] = g_cli_structure->token_names[bucket].name;
    }
    for (unsigned int line = 0u; line < side->lines; line += 1u)
    {
        unsigned long long hash = 0xcbf29ce484222325ull;
        for (unsigned int at = side->line_first[line]; at < side->line_first[line + 1u]; at += 1u)
        {
            hash = (hash ^ side->token_name[at]) * 0x100000001b3ull;
        }
        unsigned long long bucket = hash & (CLI_STRUCTURE_NAMES - 1u);
        while ((g_cli_structure->line_names[bucket].name != 0u) &&
               !cli_line_same(g_cli_structure->line_names[bucket].side, g_cli_structure->line_names[bucket].at, side,
                              line))
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
            const CliNameBucket made = {side, line, *line_names};
            g_cli_structure->line_names[bucket] = made;
        }
        side->line_name[line] = g_cli_structure->line_names[bucket].name;
    }
    return 1;
}

int cli_structure_named(CliSide *old, CliSide *now)
{
    memor.set(
        &(MemoriaCfg){.dst = g_cli_structure->token_names, .val = 0, .bytes = sizeof(g_cli_structure->token_names)});
    memor.set(
        &(MemoriaCfg){.dst = g_cli_structure->line_names, .val = 0, .bytes = sizeof(g_cli_structure->line_names)});
    unsigned int names = 0u;
    unsigned int line_names = 0u;
    return cli_side_named(old, &names, &line_names) && cli_side_named(now, &names, &line_names);
}

int cli_structure_starts_name(const CliSide *side, unsigned int token)
{
    const char *const at = side->text + side->token[token].start;
    unsigned int bytes = 0u;
    return cli_starts_name_letter(cli_letter(at, side->text + side->token[token].end, &bytes));
}

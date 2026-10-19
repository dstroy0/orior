// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_scan.c: a text read by its letters, its line and its column counted in UTF-16 units as the editor and the
// protocol count columns; the tokens and the comments a reader keeps; and a set of names
#include "../text/json_tree.h"
#include "../text/regexp.h"
#include "inspect_internal.h"

CliLock g_cli_inspect_lock = CLI_LOCK_INIT;
CliInspectTok *g_cli_inspect_tok;
unsigned int g_cli_inspect_toks;
CliInspectSpan *g_cli_inspect_comment;
unsigned int g_cli_inspect_comments;
int g_cli_inspect_over;

void cli_inspect_scan_laid(CliInspectCarve *carve)
{
    g_cli_inspect_tok = cli_inspect_carved(carve, (unsigned long long)CLI_INSPECT_TOKENS * sizeof(CliInspectTok));
    g_cli_inspect_comment =
        cli_inspect_carved(carve, (unsigned long long)CLI_INSPECT_COMMENTS * sizeof(CliInspectSpan));
}

void cli_inspect_scan_begun(CliInspectScan *scan, const char *src, unsigned long long size)
{
    *scan = (CliInspectScan){src, size, 0ull, 0u, 0u};
}

// the letter at `pos` of the scan's text, its bytes into `taken`
static unsigned int scan_letter(const CliInspectScan *scan, unsigned long long pos, unsigned long long *taken)
{
    if (pos >= scan->size)
    {
        *taken = 0ull;
        return CLI_INSPECT_END;
    }
    unsigned int letter = 0u;
    *taken = cli_lossy_letter((const unsigned char *)scan->src, scan->size, pos, &letter);
    return letter;
}

unsigned int cli_inspect_peek(const CliInspectScan *scan)
{
    unsigned long long taken = 0ull;
    return scan_letter(scan, scan->pos, &taken);
}

unsigned int cli_inspect_peek_at(const CliInspectScan *scan, unsigned int ahead)
{
    unsigned long long pos = scan->pos;
    unsigned long long taken = 0ull;
    for (unsigned int at = 0u; at < ahead; at += 1u)
    {
        if (scan_letter(scan, pos, &taken) == CLI_INSPECT_END)
        {
            return CLI_INSPECT_END;
        }
        pos += taken;
    }
    return scan_letter(scan, pos, &taken);
}

unsigned int cli_inspect_bump(CliInspectScan *scan)
{
    unsigned long long taken = 0ull;
    const unsigned int letter = scan_letter(scan, scan->pos, &taken);
    if (letter == CLI_INSPECT_END)
    {
        return letter;
    }
    scan->pos += taken;
    if (letter == '\n')
    {
        scan->line += 1u;
        scan->col = 0u;
    }
    else
    {
        scan->col += (letter >= 0x10000u) ? 2u : 1u;
    }
    return letter;
}

CliInspectPlace cli_inspect_place(const CliInspectScan *scan)
{
    return (CliInspectPlace){scan->line, scan->col};
}

void cli_inspect_skip_line(CliInspectScan *scan)
{
    const CliInspectPlace from = cli_inspect_place(scan);
    unsigned int letter = cli_inspect_peek(scan);
    while ((letter != CLI_INSPECT_END) && (letter != '\n') && (letter != '\r'))
    {
        (void)cli_inspect_bump(scan);
        letter = cli_inspect_peek(scan);
    }
    cli_inspect_comment_put(from, cli_inspect_place(scan));
}

void cli_inspect_tok_put(const CliInspectScan *scan, unsigned char kind, unsigned long long start, CliInspectPlace from,
                         int field)
{
    if (g_cli_inspect_toks == CLI_INSPECT_TOKENS)
    {
        g_cli_inspect_over = 1;
        return;
    }
    // a text read is at most 4 GiB, which a token's bytes count
    g_cli_inspect_tok[g_cli_inspect_toks] = (CliInspectTok){
        kind, (unsigned char)(field != 0), (unsigned int)start, (unsigned int)scan->pos, from, cli_inspect_place(scan)};
    g_cli_inspect_toks += 1u;
}

void cli_inspect_comment_put(CliInspectPlace from, CliInspectPlace to)
{
    if (g_cli_inspect_comments == CLI_INSPECT_COMMENTS)
    {
        g_cli_inspect_over = 1;
        return;
    }
    g_cli_inspect_comment[g_cli_inspect_comments] = (CliInspectSpan){from, to};
    g_cli_inspect_comments += 1u;
}

int cli_inspect_alphabetic(unsigned int letter)
{
    if (letter < 0x80u)
    {
        return ((letter >= 'a') && (letter <= 'z')) || ((letter >= 'A') && (letter <= 'Z'));
    }
    return (letter != CLI_INSPECT_END) && cli_regexp_property(CLI_REGEXP_LETTER, letter);
}

int cli_inspect_alphanumeric(unsigned int letter)
{
    if (letter < 0x80u)
    {
        return cli_inspect_alphabetic(letter) || ((letter >= '0') && (letter <= '9'));
    }
    return (letter != CLI_INSPECT_END) &&
           (cli_regexp_property(CLI_REGEXP_LETTER, letter) || cli_regexp_property(CLI_REGEXP_NUMBER, letter));
}

int cli_inspect_white(unsigned int letter)
{
    return ((letter >= 0x09u) && (letter <= 0x0du)) || (letter == 0x20u) || (letter == 0x85u) || (letter == 0xa0u) ||
           (letter == 0x1680u) || ((letter >= 0x2000u) && (letter <= 0x200au)) || (letter == 0x2028u) ||
           (letter == 0x2029u) || (letter == 0x202fu) || (letter == 0x205fu) || (letter == 0x3000u);
}

// the slot of a set a name hashes to, FNV-1a over its bytes
static unsigned int set_slot(const CliInspectSet *set, const char *at, unsigned int size)
{
    unsigned int hash = 2166136261u;
    for (unsigned int index = 0u; index < size; index += 1u)
    {
        hash = (hash ^ (unsigned char)at[index]) * 16777619u;
    }
    return hash & (set->buckets - 1u);
}

void cli_inspect_set_laid(CliInspectSet *set, CliInspectCarve *carve)
{
    unsigned int buckets = 16u;
    while ((buckets / 2u) < g_cli_inspect_most.names)
    {
        buckets <<= 1u;
    }
    set->buckets = buckets;
    set->count = 0u;
    set->at = cli_inspect_carved(carve, (unsigned long long)buckets * sizeof(const char *));
    set->size = cli_inspect_carved(carve, (unsigned long long)buckets * sizeof(unsigned int));
    set->taken = cli_inspect_carved(carve, (unsigned long long)(buckets / 2u) * sizeof(unsigned int));
    if (set->at != NULL)
    {
        memor.set(&(MemoriaCfg){.dst = (void *)set->at, .val = 0, .bytes = (size_t)buckets * sizeof(const char *)});
    }
}

void cli_inspect_set_cleared(CliInspectSet *set)
{
    for (unsigned int index = 0u; index < set->count; index += 1u)
    {
        set->at[set->taken[index]] = NULL;
    }
    set->count = 0u;
}

void cli_inspect_set_put(CliInspectSet *set, const char *at, unsigned int size)
{
    unsigned int slot = set_slot(set, at, size);
    while (set->at[slot] != NULL)
    {
        if ((set->size[slot] == size) &&
            (memor.cmp(&(MemoriaCfg){.src = set->at[slot], .other = at, .bytes = size}) == 0))
        {
            return;
        }
        slot = (slot + 1u) & (set->buckets - 1u);
    }
    if (set->count == (set->buckets / 2u))
    {
        return;
    }
    set->at[slot] = at;
    set->size[slot] = size;
    set->taken[set->count] = slot;
    set->count += 1u;
}

int cli_inspect_set_has(const CliInspectSet *set, const char *at, unsigned int size)
{
    unsigned int slot = set_slot(set, at, size);
    while (set->at[slot] != NULL)
    {
        if ((set->size[slot] == size) &&
            (memor.cmp(&(MemoriaCfg){.src = set->at[slot], .other = at, .bytes = size}) == 0))
        {
            return 1;
        }
        slot = (slot + 1u) & (set->buckets - 1u);
    }
    return 0;
}

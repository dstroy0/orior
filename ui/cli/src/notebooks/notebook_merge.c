// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// notebook_merge.c: a notebook's cells as a diff compares them, and the cells two sides of a merge make of their base
#include "notebook_internal.h"

// the most lines of one cell's source two cells are compared by
#define CLI_NOTEBOOK_LINES 4096u

// the cells of each side of a merge, the table of the longest runs of the same cells, and where each base cell stands
// on each side, CLI_JSON_NONE where it stands on none; held under the lock while one merge runs
static unsigned int s_base[CLI_NOTEBOOK_CELLS];
static unsigned int s_ours[CLI_NOTEBOOK_CELLS];
static unsigned int s_theirs[CLI_NOTEBOOK_CELLS];
static unsigned int s_table[CLI_NOTEBOOK_CELLS + 1u][CLI_NOTEBOOK_CELLS + 1u];
static unsigned int s_ours_at[CLI_NOTEBOOK_CELLS];
static unsigned int s_theirs_at[CLI_NOTEBOOK_CELLS];
static CliLock s_lock = CLI_LOCK_INIT;

int cli_notebook_for_diff(CliJson *json, const char *text, unsigned long long size, CfgJsonToken *tokens,
                          unsigned int tokens_most, CliLine *out, CliLine *said)
{
    const unsigned int book = cli_notebook_read(json, "x.ipynb", text, size, tokens, tokens_most, said);
    if (book == CLI_JSON_NONE)
    {
        return 0;
    }
    const unsigned long long begun = out->at;
    const unsigned int cells = cli_json_member(json, book, "cells");
    unsigned int index = 0u;
    for (unsigned int cell = cli_json_is(json, cells, CFG_JSON_ARRAY) ? json->node[cells].first : CLI_JSON_NONE;
         cell != CLI_JSON_NONE; cell = json->node[cell].next)
    {
        index += 1u;
        const char *const named = cli_json_string(json, cli_json_member(json, cell, "cell_type"), NULL);
        cli_line_text(out, "# cell ");
        cli_line_decimal(out, index, 1u);
        cli_line_text(out, " (");
        cli_line_text(out, (named != NULL) ? named : "code");
        cli_line_text(out, ")\n");
        unsigned long long length = 0ull;
        const char *const source = cli_notebook_source(json, cell, &length);
        cli_line_counted(out, source, length);
        if (cli_line_fits(out) && !cli_notebook_ends(out->out + begun, out->at - begun, "\n"))
        {
            cli_line_character(out, '\n');
        }
        const unsigned int outputs = cli_json_member(json, cell, "outputs");
        for (unsigned int output = cli_json_is(json, outputs, CFG_JSON_ARRAY) ? json->node[outputs].first
                                                                              : CLI_JSON_NONE;
             output != CLI_JSON_NONE; output = json->node[output].next)
        {
            const unsigned int data = cli_json_member(json, output, "data");
            const char *shown = cli_json_string(json, cli_json_member(json, output, "text"), NULL);
            shown = (shown != NULL) ? shown : cli_json_string(json, cli_json_member(json, data, "text/plain"), NULL);
            const char *const evalue = cli_json_string(json, cli_json_member(json, output, "evalue"), NULL);
            const char *const ename = cli_json_string(json, cli_json_member(json, output, "ename"), NULL);
            const char *const typed = cli_json_string(json, cli_json_member(json, output, "output_type"), NULL);
            cli_line_text(out, "## ");
            cli_line_text(out, (typed != NULL) ? typed : "output");
            if ((shown != NULL) || (evalue != NULL))
            {
                cli_line_character(out, '\n');
                if (shown != NULL)
                {
                    cli_line_text(out, shown);
                }
                else
                {
                    cli_line_text(out, (ename != NULL) ? ename : "error");
                    cli_line_text(out, ": ");
                    cli_line_text(out, evalue);
                }
                if (cli_line_fits(out) && !cli_notebook_ends(out->out + begun, out->at - begun, "\n"))
                {
                    cli_line_character(out, '\n');
                }
                continue;
            }
            cli_line_text(out, ": ");
            int first = 1;
            for (unsigned int kind = cli_json_is(json, data, CFG_JSON_OBJECT) ? json->node[data].first : CLI_JSON_NONE;
                 kind != CLI_JSON_NONE; kind = json->node[kind].next)
            {
                cli_line_text(out, first ? "" : ", ");
                cli_line_text(out, json->text + json->node[kind].key);
                first = 0;
            }
            cli_line_character(out, '\n');
        }
    }
    return 1;
}

// the lines of a text, each its start and its end, into `lines`, which holds CLI_NOTEBOOK_LINES: their count
static unsigned int cli_lines_split(const char *text, unsigned long long length, const char *(*lines)[2])
{
    unsigned int count = 0u;
    const char *at = text;
    while ((at < (text + length)) && (count < CLI_NOTEBOOK_LINES))
    {
        const char *stop = NULL;
        const char *const next = cli_notebook_line(at, text + length, &stop);
        lines[count][0] = at;
        lines[count][1] = stop;
        count += 1u;
        at = next;
    }
    return count;
}

// whether two cells' sources are alike enough to be one cell: at least half their lines held in common, each line of
// one matched to the first line of the other like it not matched yet
static int cli_alike(const CliJson *json, unsigned int one, unsigned int other)
{
    static const char *s_left[CLI_NOTEBOOK_LINES][2];
    static const char *s_right[CLI_NOTEBOOK_LINES][2];
    static unsigned char s_used[CLI_NOTEBOOK_LINES];
    unsigned long long left_length = 0ull;
    unsigned long long right_length = 0ull;
    const char *const left_text = cli_notebook_source(json, one, &left_length);
    const char *const right_text = cli_notebook_source(json, other, &right_length);
    if ((left_length == right_length) &&
        (memor.cmp(&(MemoriaCfg){.src = left_text, .other = right_text, .bytes = left_length}) == 0))
    {
        return 1;
    }
    const unsigned int left = cli_lines_split(left_text, left_length, s_left);
    const unsigned int right = cli_lines_split(right_text, right_length, s_right);
    if ((left == 0u) || (right == 0u))
    {
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = s_used, .val = 0, .bytes = right});
    unsigned int shared = 0u;
    for (unsigned int line = 0u; line < left; line += 1u)
    {
        const unsigned long long size = (unsigned long long)(s_left[line][1] - s_left[line][0]);
        for (unsigned int at = 0u; at < right; at += 1u)
        {
            if (!s_used[at] && ((unsigned long long)(s_right[at][1] - s_right[at][0]) == size) &&
                (memor.cmp(&(MemoriaCfg){.src = s_left[line][0], .other = s_right[at][0], .bytes = size}) == 0))
            {
                s_used[at] = 1u;
                shared += 1u;
                break;
            }
        }
    }
    // twice the lines in common over all the lines of both at least a half
    return (4ull * shared) >= ((unsigned long long)left + right);
}

// Whether two cells are the same cell: by their ids where both carry one, and else of one kind and alike
static int cli_same_cell(const CliJson *json, unsigned int one, unsigned int other)
{
    const char *const left = cli_json_string(json, cli_json_member(json, one, "id"), NULL);
    const char *const right = cli_json_string(json, cli_json_member(json, other, "id"), NULL);
    if ((left != NULL) && (right != NULL))
    {
        return cellul.eq(&(CatenaFinitaCfg){.src = left, .other = right, .cap = CLI_CELLS_POOL});
    }
    return cli_json_same(json, cli_json_member(json, one, "cell_type"), json,
                         cli_json_member(json, other, "cell_type")) &&
           cli_alike(json, one, other);
}

// For each cell of `base`, where the same cell stands in `other`, by the longest run of matches in order, as a diff
// aligns lines
static void cli_aligned(const CliJson *json, const unsigned int *base, unsigned int n, const unsigned int *other,
                        unsigned int m, unsigned int *found)
{
    for (unsigned int j = 0u; j <= m; j += 1u)
    {
        s_table[n][j] = 0u;
    }
    for (unsigned int i = n; i > 0u; i -= 1u)
    {
        s_table[i - 1u][m] = 0u;
        for (unsigned int j = m; j > 0u; j -= 1u)
        {
            const unsigned int down = s_table[i][j - 1u];
            const unsigned int across = s_table[i - 1u][j];
            s_table[i - 1u][j - 1u] = cli_same_cell(json, base[i - 1u], other[j - 1u]) ? (s_table[i][j] + 1u)
                                      : (down > across)                                ? down
                                                                                       : across;
        }
    }
    unsigned int i = 0u;
    unsigned int j = 0u;
    for (unsigned int at = 0u; at < n; at += 1u)
    {
        found[at] = CLI_JSON_NONE;
    }
    while ((i < n) && (j < m))
    {
        if (cli_same_cell(json, base[i], other[j]) && (s_table[i][j] == (s_table[i + 1u][j + 1u] + 1u)))
        {
            found[i] = j;
            i += 1u;
            j += 1u;
        }
        else if (s_table[i + 1u][j] >= s_table[i][j + 1u])
        {
            i += 1u;
        }
        else
        {
            j += 1u;
        }
    }
}

// the cells of a book's `cells`, at most CLI_NOTEBOOK_CELLS: their count
static unsigned int cli_cells(const CliJson *json, unsigned int book, unsigned int *cells)
{
    const unsigned int list = cli_json_member(json, book, "cells");
    unsigned int count = 0u;
    for (unsigned int cell = cli_json_is(json, list, CFG_JSON_ARRAY) ? json->node[list].first : CLI_JSON_NONE;
         (cell != CLI_JSON_NONE) && (count < CLI_NOTEBOOK_CELLS); cell = json->node[cell].next)
    {
        cells[count] = cell;
        count += 1u;
    }
    return count;
}

// the base cell a cell one side added follows there: the nearest cell before it that stands for a base cell, by that
// base cell's place, or CLI_JSON_NONE for the head
static unsigned int cli_follows(const unsigned int *sides, unsigned int bases, unsigned int index)
{
    for (unsigned int before = index; before > 0u; before -= 1u)
    {
        for (unsigned int base = 0u; base < bases; base += 1u)
        {
            if (sides[base] == (before - 1u))
            {
                return base;
            }
        }
    }
    return CLI_JSON_NONE;
}

// whether a side's cell `index` stands for a base cell
static int cli_taken(const unsigned int *sides, unsigned int bases, unsigned int index)
{
    for (unsigned int base = 0u; base < bases; base += 1u)
    {
        if (sides[base] == index)
        {
            return 1;
        }
    }
    return 0;
}

// `{"key": cell}` put after the entries, the cell copied
static void cli_entry(CliJson *json, unsigned int entries, const char *key, unsigned int cell)
{
    const unsigned int entry = cli_json_made(json, CFG_JSON_OBJECT);
    cli_json_set(json, entry, key, cli_json_copy(json, cell));
    cli_json_append(json, entries, entry);
}

// whether a side's cell changed from its base cell: its source or its kind
static int cli_changed(const CliJson *json, unsigned int cell, unsigned int base)
{
    unsigned long long length = 0ull;
    unsigned long long base_length = 0ull;
    const char *const source = cli_notebook_source(json, cell, &length);
    const char *const base_source = cli_notebook_source(json, base, &base_length);
    return (length != base_length) ||
           (memor.cmp(&(MemoriaCfg){.src = source, .other = base_source, .bytes = length}) != 0) ||
           !cli_json_same(json, cli_json_member(json, cell, "cell_type"), json,
                          cli_json_member(json, base, "cell_type"));
}

// whether two cells hold the same source
static int cli_same_source(const CliJson *json, unsigned int one, unsigned int other)
{
    unsigned long long length = 0ull;
    unsigned long long other_length = 0ull;
    const char *const source = cli_notebook_source(json, one, &length);
    const char *const other_source = cli_notebook_source(json, other, &other_length);
    return (length == other_length) &&
           (memor.cmp(&(MemoriaCfg){.src = source, .other = other_source, .bytes = length}) == 0);
}

// the cells each side added after the base cell `after`, CLI_JSON_NONE for the head, put as entries: ours, then each
// of theirs no cell of ours added there holds the source of
static void cli_added_put(CliJson *json, unsigned int entries, unsigned int after, unsigned int bases,
                          unsigned int ours, unsigned int theirs)
{
    for (unsigned int index = 0u; index < ours; index += 1u)
    {
        if (!cli_taken(s_ours_at, bases, index) && (cli_follows(s_ours_at, bases, index) == after))
        {
            cli_entry(json, entries, "take", s_ours[index]);
        }
    }
    for (unsigned int index = 0u; index < theirs; index += 1u)
    {
        if (cli_taken(s_theirs_at, bases, index) || (cli_follows(s_theirs_at, bases, index) != after))
        {
            continue;
        }
        int held = 0;
        for (unsigned int mine = 0u; !held && (mine < ours); mine += 1u)
        {
            held = !cli_taken(s_ours_at, bases, mine) && (cli_follows(s_ours_at, bases, mine) == after) &&
                   cli_same_source(json, s_ours[mine], s_theirs[index]);
        }
        if (!held)
        {
            cli_entry(json, entries, "take", s_theirs[index]);
        }
    }
}

// `{"conflict": {"base", "ours", "theirs"}}` put after the entries, a side that deleted the cell null
static void cli_conflict(CliJson *json, unsigned int entries, unsigned int base, unsigned int mine, unsigned int yours)
{
    const unsigned int conflict = cli_json_made(json, CFG_JSON_OBJECT);
    cli_json_set(json, conflict, "base", cli_json_copy(json, base));
    cli_json_set(json, conflict, "ours",
                 (mine != CLI_JSON_NONE) ? cli_json_copy(json, mine) : cli_json_made(json, CFG_JSON_NULL));
    cli_json_set(json, conflict, "theirs",
                 (yours != CLI_JSON_NONE) ? cli_json_copy(json, yours) : cli_json_made(json, CFG_JSON_NULL));
    const unsigned int entry = cli_json_made(json, CFG_JSON_OBJECT);
    cli_json_set(json, entry, "conflict", conflict);
    cli_json_append(json, entries, entry);
}

// the entry for one base cell, as each side holds it
static void cli_base_entry(CliJson *json, unsigned int entries, unsigned int base, unsigned int mine,
                           unsigned int yours)
{
    if ((mine != CLI_JSON_NONE) && (yours != CLI_JSON_NONE))
    {
        const int ours_changed = cli_changed(json, mine, base);
        const int theirs_changed = cli_changed(json, yours, base);
        if (!theirs_changed || (ours_changed && cli_same_source(json, mine, yours) &&
                                cli_json_same(json, cli_json_member(json, mine, "cell_type"), json,
                                              cli_json_member(json, yours, "cell_type"))))
        {
            cli_entry(json, entries, "take", mine);
        }
        else if (!ours_changed)
        {
            cli_entry(json, entries, "take", yours);
        }
        else
        {
            cli_conflict(json, entries, base, mine, yours);
        }
    }
    else if ((mine != CLI_JSON_NONE) && cli_changed(json, mine, base))
    {
        cli_conflict(json, entries, base, mine, CLI_JSON_NONE);
    }
    else if ((yours != CLI_JSON_NONE) && cli_changed(json, yours, base))
    {
        cli_conflict(json, entries, base, CLI_JSON_NONE, yours);
    }
}

unsigned int cli_notebook_merge(CliJson *json, unsigned int base, unsigned int ours, unsigned int theirs)
{
    cli_lock_take(&s_lock);
    const unsigned int bases = cli_cells(json, base, s_base);
    const unsigned int our_count = cli_cells(json, ours, s_ours);
    const unsigned int their_count = cli_cells(json, theirs, s_theirs);
    cli_aligned(json, s_base, bases, s_ours, our_count, s_ours_at);
    cli_aligned(json, s_base, bases, s_theirs, their_count, s_theirs_at);
    const unsigned int entries = cli_json_made(json, CFG_JSON_ARRAY);
    cli_added_put(json, entries, CLI_JSON_NONE, bases, our_count, their_count);
    for (unsigned int index = 0u; index < bases; index += 1u)
    {
        const unsigned int mine = (s_ours_at[index] != CLI_JSON_NONE) ? s_ours[s_ours_at[index]] : CLI_JSON_NONE;
        const unsigned int yours = (s_theirs_at[index] != CLI_JSON_NONE) ? s_theirs[s_theirs_at[index]] : CLI_JSON_NONE;
        cli_base_entry(json, entries, s_base[index], mine, yours);
        cli_added_put(json, entries, index, bases, our_count, their_count);
    }
    const unsigned int book = cli_json_copy(json, ours);
    cli_json_set(json, book, "cells", cli_json_made(json, CFG_JSON_ARRAY));
    const unsigned int merged = cli_json_made(json, CFG_JSON_OBJECT);
    cli_json_set(json, merged, "book", book);
    cli_json_set(json, merged, "entries", entries);
    cli_lock_give(&s_lock);
    return json->full ? CLI_JSON_NONE : merged;
}

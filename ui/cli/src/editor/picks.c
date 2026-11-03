// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// picks.c: the window's lists stored by number, their rows matched against a query, a long list over the cores
#include "picks.h"
#include "../command_line/cells.h"
#include "../machine/cores.h"
#include "../text/json_tree.h"
#include "files.h"

// One part of a list as one thread matches it: its rows from `from` to `to`, and the letters of the row being matched,
// as shown and in lower case, the match's own capacity and the places that matched
typedef struct
{
    unsigned int from;
    unsigned int to;
    unsigned int displayed[CLI_FUZZY_LETTERS];
    unsigned int lower[CLI_FUZZY_LETTERS];
    unsigned int latest[CLI_FUZZY_LETTERS];
    unsigned int hits[CLI_FUZZY_LETTERS];
    unsigned int hit_count;
    CliThread thread;
} CliPickPart;
// the rows past which a list is matched over the cores, the rest on one; and the most threads it is matched on
#define CLI_SPREAD_FROM 4096u
#define CLI_PICKS_PARTS 256u

// A list stored: the number it is stored under, its rows' count, and its cell, each row's start in the texts and then
// the texts one after another, each ended by a 0
typedef struct
{
    unsigned long long id;
    unsigned int count;
    unsigned int *row;
    char *text;
    void *cell;
} CliPickList;

// The lists stored, in the order they were stored, and a match's capacity, one cell sized to the most rows of a list
// stored and to the parts the cores take, taken again as a longer list is stored and given back as the last list is
// forgotten: the query's letters, each row's score and whether it answered, the rows that answered in order, the parts
// and how many have ended
typedef struct
{
    CliPickList list[CLI_PICKS_LISTS];
    unsigned int lists;
    unsigned int required[CLI_FUZZY_LETTERS];
    unsigned int requested_count;
    const CliPickList *matching;
    void *capacity;
    unsigned int capacity_rows;
    unsigned int parts;
    double *score;
    unsigned char *matched;
    unsigned int *order;
    unsigned int *reserve;
    CliPickPart *part;
    unsigned int ended;
} CliPicks;

// the lists stored; the next number; and the wake the parts end on
static CliPicks s_picks;
static unsigned long long s_next = 1ULL;

typedef struct
{
    unsigned char **at;
    unsigned long long bytes;
    unsigned int rows;
    CliPickPart *part;
    unsigned int index;
    double *score;
    unsigned int count;
} PicksCtx;
// the next part of a cell, `bytes` long, taken from `*at`, its start aligned to a word's boundary: its start
EMBED_INLINE void *picks_part(const PicksCtx *args)
{
    unsigned char **at = args->at;
    void *const part = *at;
    *at += (args->bytes + 7ULL) & ~7ULL;
    return part;
}

// the match's capacity taken for `rows` and the parts the cores take, the capacity before given back: 1, or 0 where no
// cell is left, the capacity before retained
EMBED_INLINE int picks_work_state_alloc(const PicksCtx *args)
{
    const unsigned int cores = cli_cores_for_work();
    unsigned int parts;
    if (cores < 1u)
    {
        parts = 1u;
    }
    else if (cores > CLI_PICKS_PARTS)
    {
        parts = CLI_PICKS_PARTS;
    }
    else
    {
        parts = cores;
    }
    const unsigned long long bytes =
        ((unsigned long long)args->rows * (sizeof(double) + 1ULL + (2ULL * sizeof(unsigned int)))) +
        ((unsigned long long)parts * sizeof(CliPickPart)) + (5ULL * 8ULL);
    void *const capacity = g_cli_cells->persistent_buf_alloc(bytes, &g_cli_editor_cells);
    if (capacity == NULL)
    {
        return 0;
    }
    g_cli_cells->persistent_buf_release(s_picks.capacity, &g_cli_editor_cells);
    s_picks.capacity = capacity;
    s_picks.capacity_rows = args->rows;
    s_picks.parts = parts;
    unsigned char *at = capacity;
    s_picks.score =
        EMBED_CALL(picks_part, PicksCtx, .at = &at, .bytes = (unsigned long long)args->rows * sizeof(double));
    s_picks.matched = EMBED_CALL(picks_part, PicksCtx, .at = &at, .bytes = args->rows);
    s_picks.order =
        EMBED_CALL(picks_part, PicksCtx, .at = &at, .bytes = (unsigned long long)args->rows * sizeof(unsigned int));
    s_picks.reserve =
        EMBED_CALL(picks_part, PicksCtx, .at = &at, .bytes = (unsigned long long)args->rows * sizeof(unsigned int));
    s_picks.part =
        EMBED_CALL(picks_part, PicksCtx, .at = &at, .bytes = (unsigned long long)parts * sizeof(CliPickPart));
    return 1;
}

unsigned long long cli_picks_store(const CliPicksCfg *args)
{
    unsigned long long rows = (args->length == 0ULL) ? 0ULL : 1ULL;
    for (unsigned long long at = 0ULL; at < args->length; at += 1ULL)
    {
        rows += (args->texts[at] == '\n') ? 1ULL : 0ULL;
    }
    // a row's start is an unsigned int's, and a text past one's reach is not stored
    if ((s_picks.lists >= CLI_PICKS_LISTS) || (args->length >= 0xffffffffULL))
    {
        return 0ULL;
    }
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(
        (rows * sizeof(unsigned int)) + args->length + 1ULL + 8ULL, &g_cli_editor_cells);
    if (cell == NULL)
    {
        return 0ULL;
    }
    if (((s_picks.capacity == NULL) || (rows > s_picks.capacity_rows)) &&
        !EMBED_CALL(picks_work_state_alloc, PicksCtx, .rows = (unsigned int)rows))
    {
        g_cli_cells->persistent_buf_release(cell, &g_cli_editor_cells);
        return 0ULL;
    }
    CliPickList *const list = &s_picks.list[s_picks.lists];
    unsigned char *at = cell;
    list->row = EMBED_CALL(picks_part, PicksCtx, .at = &at, .bytes = rows * sizeof(unsigned int));
    list->text = (char *)at;
    list->cell = cell;
    list->id = s_next;
    list->count = (unsigned int)rows;
    unsigned int row = 0u;
    for (unsigned long long at_text = 0ULL; at_text < args->length; at_text += 1ULL)
    {
        const int starts = (at_text == 0ULL) || (args->texts[at_text - 1ULL] == '\n');
        if (starts)
        {
            list->row[row] = (unsigned int)at_text;
            row += 1u;
        }
        list->text[at_text] = (args->texts[at_text] == '\n') ? '\0' : args->texts[at_text];
    }
    if ((args->length > 0ULL) && (args->texts[args->length - 1ULL] == '\n'))
    {
        // a text that ends with a newline has an empty row after it
        list->row[row] = (unsigned int)args->length;
    }
    list->text[args->length] = '\0';
    s_picks.lists += 1u;
    const unsigned long long id = s_next;
    s_next += 1ULL;
    return id;
}

void cli_picks_forget(const CliPicksIdCfg *args)
{
    unsigned int at = 0u;
    while ((at < s_picks.lists) && (s_picks.list[at].id != args->id))
    {
        at += 1u;
    }
    if (at == s_picks.lists)
    {
        return;
    }
    g_cli_cells->persistent_buf_release(s_picks.list[at].cell, &g_cli_editor_cells);
    for (unsigned int list = at + 1u; list < s_picks.lists; list += 1u)
    {
        s_picks.list[list - 1u] = s_picks.list[list];
    }
    s_picks.lists -= 1u;
    if (s_picks.lists == 0u)
    {
        g_cli_cells->persistent_buf_release(s_picks.capacity, &g_cli_editor_cells);
        s_picks.capacity = NULL;
        s_picks.capacity_rows = 0u;
    }
}

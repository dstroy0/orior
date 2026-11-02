// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// picks.c: the window's lists stored by number, their rows matched against a query, a long list over the cores
#include "picks.h"
#include "../command_line/cells.h"
#include "../machine/cores.h"
#include "../text/json_tree.h"
#include "files.h"

// One part of a list as one thread matches it: its rows from `from` to `to`, and the letters of the row being matched,
// as shown and in lower case, the match's own room and the places that matched
typedef struct
{
    unsigned int from;
    unsigned int to;
    unsigned int shown[CLI_FUZZY_LETTERS];
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

// The lists stored, in the order they were stored, and a match's room, one cell sized to the most rows of a list stored
// and to the parts the cores take, taken again as a longer list is stored and given back as the last list is forgotten:
// the query's letters, each row's score and whether it answered, the rows that answered in order, the parts and how
// many have ended
typedef struct
{
    CliPickList list[CLI_PICKS_LISTS];
    unsigned int lists;
    unsigned int wanted[CLI_FUZZY_LETTERS];
    unsigned int wanted_count;
    const CliPickList *matching;
    void *room;
    unsigned int room_rows;
    unsigned int parts;
    double *score;
    unsigned char *matched;
    unsigned int *order;
    unsigned int *spare;
    CliPickPart *part;
    unsigned int ended;
} CliPicks;

// the lists stored; the next number; and the wake the parts end on
static CliPicks s_picks;
static unsigned long long s_next = 1ull;
static CliWake s_wake = CLI_WAKE_INIT;

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
    *at += (args->bytes + 7ull) & ~7ull;
    return part;
}

// the match's room taken for `rows` and the parts the cores take, the room before given back: 1, or 0 where no cell is
// left, the room before retained
EMBED_INLINE int picks_room_take(const PicksCtx *args)
{
    const unsigned int cores = cli_cores_for_work();
    const unsigned int parts = (cores < 1u) ? 1u : ((cores > CLI_PICKS_PARTS) ? CLI_PICKS_PARTS : cores);
    const unsigned long long bytes =
        ((unsigned long long)args->rows * (sizeof(double) + 1ull + (2ull * sizeof(unsigned int)))) +
        ((unsigned long long)parts * sizeof(CliPickPart)) + (5ull * 8ull);
    void *const room = g_cli_cells->persistent_buf_alloc(bytes, &g_cli_editor_cells);
    if (room == NULL)
    {
        return 0;
    }
    g_cli_cells->persistent_buf_release(s_picks.room, &g_cli_editor_cells);
    s_picks.room = room;
    s_picks.room_rows = args->rows;
    s_picks.parts = parts;
    unsigned char *at = room;
    s_picks.score =
        EMBED_CALL(picks_part, PicksCtx, .at = &at, .bytes = (unsigned long long)args->rows * sizeof(double));
    s_picks.matched = EMBED_CALL(picks_part, PicksCtx, .at = &at, .bytes = args->rows);
    s_picks.order =
        EMBED_CALL(picks_part, PicksCtx, .at = &at, .bytes = (unsigned long long)args->rows * sizeof(unsigned int));
    s_picks.spare =
        EMBED_CALL(picks_part, PicksCtx, .at = &at, .bytes = (unsigned long long)args->rows * sizeof(unsigned int));
    s_picks.part =
        EMBED_CALL(picks_part, PicksCtx, .at = &at, .bytes = (unsigned long long)parts * sizeof(CliPickPart));
    return 1;
}

unsigned long long cli_picks_store(const CliPicksCfg *args)
{
    unsigned long long rows = (args->length == 0ull) ? 0ull : 1ull;
    for (unsigned long long at = 0ull; at < args->length; at += 1ull)
    {
        rows += (args->texts[at] == '\n') ? 1ull : 0ull;
    }
    // a row's start is an unsigned int's, and a text past one's reach is not stored
    if ((s_picks.lists >= CLI_PICKS_LISTS) || (args->length >= 0xffffffffull))
    {
        return 0ull;
    }
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(
        (rows * sizeof(unsigned int)) + args->length + 1ull + 8ull, &g_cli_editor_cells);
    if (cell == NULL)
    {
        return 0ull;
    }
    if (((s_picks.room == NULL) || (rows > s_picks.room_rows)) &&
        !EMBED_CALL(picks_room_take, PicksCtx, .rows = (unsigned int)rows))
    {
        g_cli_cells->persistent_buf_release(cell, &g_cli_editor_cells);
        return 0ull;
    }
    CliPickList *const list = &s_picks.list[s_picks.lists];
    unsigned char *at = cell;
    list->row = EMBED_CALL(picks_part, PicksCtx, .at = &at, .bytes = rows * sizeof(unsigned int));
    list->text = (char *)at;
    list->cell = cell;
    list->id = s_next;
    list->count = (unsigned int)rows;
    unsigned int row = 0u;
    for (unsigned long long at_text = 0ull; at_text < args->length; at_text += 1ull)
    {
        const int starts = (at_text == 0ull) || (args->texts[at_text - 1ull] == '\n');
        if (starts)
        {
            list->row[row] = (unsigned int)at_text;
            row += 1u;
        }
        list->text[at_text] = (args->texts[at_text] == '\n') ? '\0' : args->texts[at_text];
    }
    if ((args->length > 0ull) && (args->texts[args->length - 1ull] == '\n'))
    {
        // a text that ends with a newline has an empty row after it
        list->row[row] = (unsigned int)args->length;
    }
    list->text[args->length] = '\0';
    s_picks.lists += 1u;
    const unsigned long long id = s_next;
    s_next += 1ull;
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
        g_cli_cells->persistent_buf_release(s_picks.room, &g_cli_editor_cells);
        s_picks.room = NULL;
        s_picks.room_rows = 0u;
    }
}

// the row `index` of the list being matched matched against the query by the part: 1 with its score, or 0
EMBED_INLINE int cli_pick_match(const PicksCtx *args)
{
    double *score = args->score;
    const char *const text = s_picks.matching->text + s_picks.matching->row[args->index];
    const unsigned int count =
        EMBED_CALL(cli_files_text.letters, CliFilesTextCfg, .text = text, .shown = args->part->shown,
                   .lower = args->part->lower, .room = CLI_FUZZY_LETTERS);
    return EMBED_CALL(cli_files_count.fuzzy_matches, CliFilesCountCfg, .wanted = s_picks.wanted,
                      .wanted_count = s_picks.wanted_count, .shown_read = args->part->shown,
                      .lower_read = args->part->lower, .count = count, .latest = args->part->latest, .score = score,
                      .hits = args->part->hits, .hit_count = &args->part->hit_count);
}

// a part's rows matched, each row's score recorded by its place
EMBED_INLINE void cli_part_match(const PicksCtx *args)
{
    for (unsigned int index = args->part->from; index < args->part->to; index += 1u)
    {
        double score = 0.0;
        s_picks.matched[index] =
            (unsigned char)EMBED_CALL(cli_pick_match, PicksCtx, .part = args->part, .index = index, .score = &score);
        s_picks.score[index] = score;
    }
}

// a part matched on a thread of its own, its end reported
static void cli_part_thread(void *context)
{
    EMBED_CALL(cli_part_match, PicksCtx, .part = (CliPickPart *)context);
    s_picks.ended += 1u;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_wake);
}

// the list's rows matched, over the cores where it is long
EMBED_INLINE void cli_picks_match(const PicksCtx *args)
{
    const unsigned int parts = (args->count >= CLI_SPREAD_FROM) ? s_picks.parts : 1u;
    const unsigned int share = (args->count + parts - 1u) / parts;
    for (unsigned int at = 0u; at < parts; at += 1u)
    {
        const unsigned long long from = (unsigned long long)at * share;
        const unsigned long long to = (unsigned long long)(at + 1u) * share;
        // each end is at most the count, which an unsigned int stores
        s_picks.part[at].from = (unsigned int)((from < args->count) ? from : args->count);
        s_picks.part[at].to = (unsigned int)((to < args->count) ? to : args->count);
    }
    s_picks.ended = 0u;
    unsigned int started = 0u;
    for (unsigned int at = 1u; at < parts; at += 1u)
    {
        CliPickPart *const part = &s_picks.part[at];
        part->thread = (CliThread){cli_part_thread, part, 0u};
        if (EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &part->thread))
        {
            started += 1u;
        }
        else
        {
            EMBED_CALL(cli_part_match, PicksCtx, .part = part);
        }
    }
    EMBED_CALL(cli_part_match, PicksCtx, .part = &s_picks.part[0]);
    while (s_picks.ended < started)
    {
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_wake);
    }
}

// the rows that answered sorted by score, the highest first, those that score alike in the list's order: their count
EMBED_INLINE unsigned int cli_picks_sort(const PicksCtx *args)
{
    unsigned int found = 0u;
    for (unsigned int index = 0u; index < args->count; index += 1u)
    {
        s_picks.order[found] = index;
        found += s_picks.matched[index] ? 1u : 0u;
    }
    unsigned int *from = s_picks.order;
    unsigned int *into = s_picks.spare;
    for (unsigned int width = 1u; width < found; width *= 2u)
    {
        for (unsigned int start = 0u; start < found; start += 2u * width)
        {
            const unsigned int middle = ((start + width) < found) ? (start + width) : found;
            const unsigned int end = ((middle + width) < found) ? (middle + width) : found;
            unsigned int left = start;
            unsigned int right = middle;
            for (unsigned int at = start; at < end; at += 1u)
            {
                const int take_left =
                    (left < middle) && ((right >= end) || (s_picks.score[from[left]] >= s_picks.score[from[right]]));
                into[at] = take_left ? from[left] : from[right];
                left += take_left ? 1u : 0u;
                right += take_left ? 0u : 1u;
            }
        }
        unsigned int *const turned = from;
        from = into;
        into = turned;
    }
    if (from != s_picks.order)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_picks.order, .src = from, .bytes = found * sizeof(unsigned int)});
    }
    return found;
}

int cli_picks_find(const CliPicksIdCfg *args)
{
    const char *query = args->query;
    unsigned int at = 0u;
    while ((at < s_picks.lists) && (s_picks.list[at].id != args->id))
    {
        at += 1u;
    }
    if (at == s_picks.lists)
    {
        mmgr_line_put(args->err_msg, "no list is stored under ");
        mmgr_line_decimal(args->err_msg, args->id, 0u);
        return 0;
    }
    s_picks.matching = &s_picks.list[at];
    const unsigned int count = s_picks.matching->count;
    s_picks.wanted_count = EMBED_CALL(cli_files_query_letters.want, CliFilesQueryLettersCfg, .query = query,
                                      .letters = s_picks.wanted, .room = CLI_FUZZY_LETTERS);
    unsigned int found = count;
    if (s_picks.wanted_count == 0u)
    {
        for (unsigned int index = 0u; index < count; index += 1u)
        {
            s_picks.order[index] = index;
            s_picks.score[index] = 0.0;
        }
    }
    else
    {
        EMBED_CALL(cli_picks_match, PicksCtx, .count = count);
        found = EMBED_CALL(cli_picks_sort, PicksCtx, .count = count);
    }
    // the shown rows matched again on this thread for the places that matched
    CliPickPart *const part = &s_picks.part[0];
    const unsigned long long shown = (found < args->most) ? found : args->most;
    mmgr_line_ch(args->out, '[');
    for (unsigned long long place = 0ull; place < shown; place += 1ull)
    {
        const unsigned int index = s_picks.order[place];
        double score = 0.0;
        part->hit_count = 0u;
        if (s_picks.wanted_count != 0u)
        {
            EMBED_CALL(cli_pick_match, PicksCtx, .part = part, .index = index, .score = &score);
        }
        mmgr_line_put(args->out, (place != 0ull) ? ",{\"at\":" : "{\"at\":");
        mmgr_line_decimal(args->out, index, 0u);
        mmgr_line_put(args->out, ",\"score\":");
        EMBED_CALL(cli_json_tree_out_value.json_double, CliJsonTreeOutValueCfg, .out = args->out,
                   .value_f64 = s_picks.score[index]);
        mmgr_line_put(args->out, ",\"hits\":[");
        for (unsigned int hit = 0u; hit < part->hit_count; hit += 1u)
        {
            mmgr_line_put(args->out, (hit != 0u) ? "," : "");
            mmgr_line_decimal(args->out, part->hits[hit], 0u);
        }
        mmgr_line_put(args->out, "]}");
    }
    mmgr_line_ch(args->out, ']');
    return 1;
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// picks.c: the window's lists held by number, their rows matched against a query, a long list over the cores
#include "picks.h"
#include "../command_line/cells.h"
#include "../machine/cores.h"
#include "../text/json_tree.h"
#include "files.h"

// the rows past which a list is matched over the cores, the rest on one; and the most threads it is matched on
#define CLI_SPREAD_FROM 4096u
#define CLI_PICKS_PARTS 256u

// A list held: the number it is held by, and its rows, `count` of them from `first`
typedef struct
{
    unsigned long long id;
    unsigned int first;
    unsigned int count;
} CliPickList;

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

// The lists held, in the order they were held, their rows' texts one after another, each ended by a 0; and a match:
// the query's letters, each row's score and whether it answered, the rows that answered in order, the parts and how
// many have ended
typedef struct
{
    CliPickList list[CLI_PICKS_LISTS];
    unsigned int lists;
    unsigned int row[CLI_PICKS_ROWS];
    unsigned int rows;
    char text[CLI_PICKS_TEXT];
    unsigned int used;
    unsigned int wanted[CLI_FUZZY_LETTERS];
    unsigned int wanted_count;
    const CliPickList *matching;
    double score[CLI_PICKS_ROWS];
    unsigned char matched[CLI_PICKS_ROWS];
    unsigned int order[CLI_PICKS_ROWS];
    unsigned int spare[CLI_PICKS_ROWS];
    CliPickPart part[CLI_PICKS_PARTS];
    unsigned int ended;
} CliPicks;

// the lists held, a cell taken as the first is held and given back as the last is forgotten; the next number; and the
// wake the parts end on
static CliPicks *s_picks;
static unsigned long long s_next = 1ull;
static CliWake s_wake = CLI_WAKE_INIT;
static CliLock s_lock = CLI_LOCK_INIT;
static CliLock s_ending = CLI_LOCK_INIT;

unsigned long long cli_picks_hold(const char *texts, unsigned long long length)
{
    unsigned long long rows = (length == 0ull) ? 0ull : 1ull;
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        rows += (texts[at] == '\n') ? 1ull : 0ull;
    }
    cli_lock_take(&s_lock);
    if (s_picks == NULL)
    {
        s_picks = cli_cell_taken(sizeof(*s_picks), "the lists held");
        if (s_picks == NULL)
        {
            cli_lock_give(&s_lock);
            return 0ull;
        }
        s_picks->lists = 0u;
        s_picks->rows = 0u;
        s_picks->used = 0u;
    }
    const int fits = (s_picks->lists < CLI_PICKS_LISTS) && ((s_picks->rows + rows) <= CLI_PICKS_ROWS) &&
                     ((s_picks->used + length + 1ull) <= CLI_PICKS_TEXT);
    if (!fits)
    {
        cli_lock_give(&s_lock);
        return 0ull;
    }
    // the rows and bytes fit the bounds, which an unsigned int holds
    s_picks->list[s_picks->lists] = (CliPickList){s_next, s_picks->rows, (unsigned int)rows};
    s_picks->lists += 1u;
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        const int starts = (at == 0ull) || (texts[at - 1ull] == '\n');
        if (starts)
        {
            s_picks->row[s_picks->rows] = s_picks->used + (unsigned int)at;
            s_picks->rows += 1u;
        }
        s_picks->text[s_picks->used + at] = (texts[at] == '\n') ? '\0' : texts[at];
    }
    if ((length > 0ull) && (texts[length - 1ull] == '\n'))
    {
        // a text that ends with a newline holds an empty row after it
        s_picks->row[s_picks->rows] = s_picks->used + (unsigned int)length;
        s_picks->rows += 1u;
    }
    if (length > 0ull)
    {
        s_picks->text[s_picks->used + length] = '\0';
        s_picks->used += (unsigned int)length + 1u;
    }
    const unsigned long long id = s_next;
    s_next += 1ull;
    cli_lock_give(&s_lock);
    return id;
}

void cli_picks_forget(unsigned long long id)
{
    cli_lock_take(&s_lock);
    unsigned int at = 0u;
    while ((s_picks != NULL) && (at < s_picks->lists) && (s_picks->list[at].id != id))
    {
        at += 1u;
    }
    if ((s_picks == NULL) || (at == s_picks->lists))
    {
        cli_lock_give(&s_lock);
        return;
    }
    const CliPickList gone = s_picks->list[at];
    // the bytes the list's texts take, from its first row to the next list's or the end
    const unsigned int start = (gone.count != 0u) ? s_picks->row[gone.first] : 0u;
    const unsigned int after = gone.first + gone.count;
    const unsigned int end = (after < s_picks->rows) ? s_picks->row[after] : s_picks->used;
    const unsigned int span = (gone.count != 0u) ? (end - start) : 0u;
    if (span != 0u)
    {
        memor.move_down(
            &(MemoriaCfg){.dst = s_picks->text + start, .src = s_picks->text + end, .bytes = s_picks->used - end});
    }
    s_picks->used -= span;
    for (unsigned int row = after; row < s_picks->rows; row += 1u)
    {
        s_picks->row[row - gone.count] = s_picks->row[row] - span;
    }
    s_picks->rows -= gone.count;
    for (unsigned int list = at + 1u; list < s_picks->lists; list += 1u)
    {
        s_picks->list[list - 1u] = s_picks->list[list];
        s_picks->list[list - 1u].first -= gone.count;
    }
    s_picks->lists -= 1u;
    if (s_picks->lists == 0u)
    {
        cli_cell_given(s_picks);
        s_picks = NULL;
    }
    cli_lock_give(&s_lock);
}

// the row `index` of the list being matched held against the query by the part: 1 with its score, or 0
static int cli_pick_matched(CliPickPart *part, unsigned int index, double *score)
{
    const char *const text = s_picks->text + s_picks->row[s_picks->matching->first + index];
    const unsigned int count = cli_files_letters(text, part->shown, part->lower, CLI_FUZZY_LETTERS);
    return cli_files_fuzzy_held(s_picks->wanted, s_picks->wanted_count, part->shown, part->lower, count, part->latest,
                                score, part->hits, &part->hit_count);
}

// a part's rows matched, each row's score kept by its place
static void cli_part_matched(CliPickPart *part)
{
    for (unsigned int index = part->from; index < part->to; index += 1u)
    {
        double score = 0.0;
        s_picks->matched[index] = (unsigned char)cli_pick_matched(part, index, &score);
        s_picks->score[index] = score;
    }
}

// a part matched on a thread of its own, its end told
static void cli_part_thread(void *context)
{
    cli_part_matched((CliPickPart *)context);
    cli_lock_take(&s_ending);
    s_picks->ended += 1u;
    cli_wake_all(&s_wake);
    cli_lock_give(&s_ending);
}

// the list's rows matched, over the cores where it is long
static void cli_picks_matched(unsigned int count)
{
    unsigned int parts = 1u;
    if (count >= CLI_SPREAD_FROM)
    {
        const unsigned int cores = cli_cores_for_work();
        parts = (cores < 1u) ? 1u : ((cores > CLI_PICKS_PARTS) ? CLI_PICKS_PARTS : cores);
    }
    const unsigned int share = (count + parts - 1u) / parts;
    for (unsigned int at = 0u; at < parts; at += 1u)
    {
        const unsigned long long from = (unsigned long long)at * share;
        const unsigned long long to = (unsigned long long)(at + 1u) * share;
        // each end is at most the count, which an unsigned int holds
        s_picks->part[at].from = (unsigned int)((from < count) ? from : count);
        s_picks->part[at].to = (unsigned int)((to < count) ? to : count);
    }
    s_picks->ended = 0u;
    unsigned int started = 0u;
    for (unsigned int at = 1u; at < parts; at += 1u)
    {
        CliPickPart *const part = &s_picks->part[at];
        part->thread = (CliThread){cli_part_thread, part};
        if (cli_thread_started(&part->thread))
        {
            started += 1u;
        }
        else
        {
            cli_part_matched(part);
        }
    }
    cli_part_matched(&s_picks->part[0]);
    cli_lock_take(&s_ending);
    while (s_picks->ended < started)
    {
        cli_wake_wait(&s_wake, &s_ending);
    }
    cli_lock_give(&s_ending);
}

// the rows that answered sorted by score, the highest first, those that score alike in the list's order: their count
static unsigned int cli_picks_sorted(unsigned int count)
{
    unsigned int found = 0u;
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        s_picks->order[found] = index;
        found += s_picks->matched[index] ? 1u : 0u;
    }
    unsigned int *from = s_picks->order;
    unsigned int *into = s_picks->spare;
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
                    (left < middle) && ((right >= end) || (s_picks->score[from[left]] >= s_picks->score[from[right]]));
                into[at] = take_left ? from[left] : from[right];
                left += take_left ? 1u : 0u;
                right += take_left ? 0u : 1u;
            }
        }
        unsigned int *const turned = from;
        from = into;
        into = turned;
    }
    if (from != s_picks->order)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_picks->order, .src = from, .bytes = found * sizeof(unsigned int)});
    }
    return found;
}

int cli_picks_find(unsigned long long id, const char *query, unsigned long long most, CliLine *out, CliLine *said)
{
    cli_lock_take(&s_lock);
    unsigned int at = 0u;
    while ((s_picks != NULL) && (at < s_picks->lists) && (s_picks->list[at].id != id))
    {
        at += 1u;
    }
    if ((s_picks == NULL) || (at == s_picks->lists))
    {
        cli_lock_give(&s_lock);
        cli_line_text(said, "no list is held by ");
        cli_line_decimal(said, id, 0u);
        return 0;
    }
    s_picks->matching = &s_picks->list[at];
    const unsigned int count = s_picks->matching->count;
    s_picks->wanted_count = cli_files_wanted(query, s_picks->wanted, CLI_FUZZY_LETTERS);
    unsigned int found = count;
    if (s_picks->wanted_count == 0u)
    {
        for (unsigned int index = 0u; index < count; index += 1u)
        {
            s_picks->order[index] = index;
            s_picks->score[index] = 0.0;
        }
    }
    else
    {
        cli_picks_matched(count);
        found = cli_picks_sorted(count);
    }
    // the shown rows matched again on this thread for the places that matched
    CliPickPart *const part = &s_picks->part[0];
    const unsigned long long shown = (found < most) ? found : most;
    cli_line_character(out, '[');
    for (unsigned long long place = 0ull; place < shown; place += 1ull)
    {
        const unsigned int index = s_picks->order[place];
        double score = 0.0;
        part->hit_count = 0u;
        if (s_picks->wanted_count != 0u)
        {
            cli_pick_matched(part, index, &score);
        }
        cli_line_text(out, (place != 0ull) ? ",{\"at\":" : "{\"at\":");
        cli_line_decimal(out, index, 0u);
        cli_line_text(out, ",\"score\":");
        cli_json_double(out, s_picks->score[index]);
        cli_line_text(out, ",\"hits\":[");
        for (unsigned int hit = 0u; hit < part->hit_count; hit += 1u)
        {
            cli_line_text(out, (hit != 0u) ? "," : "");
            cli_line_decimal(out, part->hits[hit], 0u);
        }
        cli_line_text(out, "]}");
    }
    cli_line_character(out, ']');
    cli_lock_give(&s_lock);
    return 1;
}

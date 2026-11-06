// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// hunks.c: the changes of the view shown against the last commit's file. A read counts the store's changes, takes a
// cell sized to them where the count moved, and writes their four numbers into it; the file as the last commit has it
// is read from git once for each key and stored beside the text, the store answering every later read from it. The
// change shown, its sentence and when it was shown stand beside the cell; a revert's text stands in a cell sized to it
#include "hunks.h"

#include "../code/git.h"
#include "../command_line/cli_internal.h"
#include "../command_line/pool.h"

// the most bytes of the file as the last commit has it a read takes, and the four numbers a change stands as
#define HUNKS_HEAD_MAX (1ULL << 20u)
#define HUNKS_NUMBERS 4u

// The changes: four numbers each in one cell, the newer text's first line and the line after its last, then the
// older's, how many and how many the cell fits; the key they were read for; the change shown, CLI_HUNKS_NONE for none,
// and the millisecond it was shown at; the sentence over it and its bytes; and the text of the last revert
typedef struct
{
    unsigned int *hunk;
    unsigned int count;
    unsigned int capacity;
    char key[CLI_NAME_MAX_LEN];
    unsigned int shown;
    unsigned long long shown_ms;
    char title[CLI_HUNKS_TITLE];
    unsigned long long title_length;
    char *revert;
} Hunks;

static Hunks s_hunks = {NULL, 0u, 0u, {0}, CLI_HUNKS_NONE, 0ULL, {0}, 0ULL, NULL};

// A pass over the store's changes: the cell they are written into, how many it fits, and how many were handed, those
// past the cell counted and not written
typedef struct
{
    unsigned int *hunk;
    unsigned int capacity;
    unsigned int count;
} HunksFill;

typedef struct
{
    const char *key;
    const char *root;
    const char *path;
    const char *text;
    unsigned long long size;
    unsigned int at;
    unsigned long long line;
    HunksFill *fill;
} HunksCtx;

// a change handed by the store counted, and written after those written so far where the cell fits it
static void hunks_filled(const CliEachHunkCfg *args)
{
    HunksFill *const fill = args->context;
    if (fill->count < fill->capacity)
    {
        unsigned int *const hunk = fill->hunk + (HUNKS_NUMBERS * fill->count);
        hunk[0] = args->now_from;
        hunk[1] = args->now_to;
        hunk[2] = args->then_from;
        hunk[3] = args->then_to;
    }
    fill->count += 1u;
}

// the first line the gutter marks for the change `at`: its first newer line, or for lines taken out alone the line
// above them, the first where none stands above
EMBED_INLINE unsigned int hunks_mark_from(const HunksCtx *args)
{
    const unsigned int *const hunk = s_hunks.hunk + (HUNKS_NUMBERS * args->at);
    if (hunk[1] > hunk[0])
    {
        return hunk[0];
    }
    return (hunk[0] > 0u) ? hunk[0] - 1u : 0u;
}

// the line after the last the gutter marks for the change `at`
EMBED_INLINE unsigned int hunks_mark_to(const HunksCtx *args)
{
    const unsigned int *const hunk = s_hunks.hunk + (HUNKS_NUMBERS * args->at);
    return (hunk[1] > hunk[0]) ? hunk[1] : EMBED_CALL(hunks_mark_from, HunksCtx, .at = args->at) + 1u;
}

// how many changes the gutter marks from a line at most `line`, by halving over the changes' first marked lines, which
// stand in order
EMBED_INLINE unsigned int hunks_before(const HunksCtx *args)
{
    unsigned int low = 0u;
    unsigned int high = s_hunks.count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if ((unsigned long long)EMBED_CALL(hunks_mark_from, HunksCtx, .at = middle) <= args->line)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    return low;
}

// the mark of the change `at`
EMBED_INLINE unsigned int hunks_mark_of(const HunksCtx *args)
{
    const unsigned int *const hunk = s_hunks.hunk + (HUNKS_NUMBERS * args->at);
    if (hunk[1] == hunk[0])
    {
        return CLI_HUNKS_MARK_REMOVED;
    }
    return (hunk[3] == hunk[2]) ? CLI_HUNKS_MARK_ADDED : CLI_HUNKS_MARK_CHANGED;
}

// the sentence over the change shown written again
EMBED_INLINE void hunks_titled(const HunksCtx *args)
{
    (void)args;
    VerbaLinea line = {s_hunks.title, sizeof(s_hunks.title), 0ULL};
    if (s_hunks.shown != CLI_HUNKS_NONE)
    {
        mmgr_line_put(&line, "Change ");
        mmgr_line_decimal(&line, s_hunks.shown + 1u, 1u);
        mmgr_line_put(&line, " of ");
        mmgr_line_decimal(&line, s_hunks.count, 1u);
    }
    s_hunks.title_length = mmgr_line_finish(&line);
}

// the change `at` shown from now on
EMBED_INLINE void hunks_shown(const HunksCtx *args)
{
    s_hunks.shown = args->at;
    s_hunks.shown_ms = cli_clock_milliseconds();
    EMBED_CALL(hunks_titled, HunksCtx, .at = args->at);
}

// the file at `path` of the tree at `root` as the last commit has it stored beside the text under `key`, none where
// git has no such file, through a cell of this read alone, the changes against it handed to `fill`: 1, or 0 where
// none could be read
EMBED_INLINE int hunks_head_stored(const HunksCtx *args)
{
    char *const scratch = ((args->root != NULL) && (args->path != NULL))
                              ? g_cli_guard->temporary_buf_alloc(HUNKS_HEAD_MAX, g_cli_widgets_owner)
                              : NULL;
    VerbaLinea out = {scratch, HUNKS_HEAD_MAX, 0ULL};
    const int tracked = (scratch != NULL) &&
                        EMBED_CALL(cli_git_root_file_stage.head_text, CliGitRootFileStageCfg, .root = args->root,
                                   .file = args->path, .out = &out) &&
                        mmgr_line_ok(&out);
    const int found = EMBED_CALL(cli_document_out_key_head_input.changes, CliDocumentOutKeyHeadInputCfg,
                                 .key = args->key, .head_given = 1, .head = tracked ? scratch : NULL,
                                 .head_size = tracked ? out.at : 0ULL, .each = hunks_filled, .context = args->fill);
    g_cli_guard->persistent_buf_release(scratch, g_cli_widgets_owner);
    return found;
}

void cli_hunks_read(const CliHunksCfg *args)
{
    const int same = (args->key != NULL) &&
                     cellul.eq(&(CatenaFinitaCfg){.src = s_hunks.key, .other = args->key, .cap = CLI_NAME_MAX_LEN});
    if (!same)
    {
        s_hunks.shown = CLI_HUNKS_NONE;
        s_hunks.key[0] = '\0';
    }
    s_hunks.count = 0u;
    if (args->key == NULL)
    {
        g_cli_guard->persistent_buf_release(s_hunks.hunk, g_cli_widgets_owner);
        s_hunks.hunk = NULL;
        s_hunks.capacity = 0u;
        EMBED_CALL(hunks_titled, HunksCtx, .at = 0u);
        return;
    }
    // one pass over the store's changes counts them and writes those the cell fits: against the text handed, against
    // the file read from git for a key new to the changes, else against the file the store has beside the text
    HunksFill fill = {s_hunks.hunk, s_hunks.capacity, 0u};
    int found = 0;
    if (args->text != NULL)
    {
        found = EMBED_CALL(cli_document_out_key_head_input.changes, CliDocumentOutKeyHeadInputCfg, .key = args->key,
                           .head_given = 1, .head = args->text, .head_size = args->size, .each = hunks_filled,
                           .context = &fill);
    }
    else if (!same)
    {
        found = EMBED_CALL(hunks_head_stored, HunksCtx, .key = args->key, .root = args->root, .path = args->path,
                           .fill = &fill);
    }
    else
    {
        found = EMBED_CALL(cli_document_out_key_head_input.changes, CliDocumentOutKeyHeadInputCfg, .key = args->key,
                           .each = hunks_filled, .context = &fill);
    }
    if (!same)
    {
        const unsigned long long length =
            cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_NAME_MAX_LEN - 1u});
        memor.cpy(&(MemoriaCfg){.dst = s_hunks.key, .src = args->key, .bytes = length});
        s_hunks.key[length] = '\0';
    }
    // the cell sized to the changes where their count moved: those written moved into it where they fit the old one,
    // else written again into it
    const unsigned int total = found ? fill.count : 0u;
    if (total != s_hunks.capacity)
    {
        unsigned int *const taken =
            (total != 0u) ? g_cli_guard->persistent_buf_alloc(
                                (unsigned long long)total * HUNKS_NUMBERS * sizeof(unsigned int), g_cli_widgets_owner)
                          : NULL;
        if ((taken != NULL) && (total <= s_hunks.capacity))
        {
            memor.cpy(&(MemoriaCfg){.dst = taken,
                                    .src = s_hunks.hunk,
                                    .bytes = (unsigned long long)total * HUNKS_NUMBERS * sizeof(unsigned int)});
        }
        else if (taken != NULL)
        {
            fill = (HunksFill){taken, total, 0u};
            (void)EMBED_CALL(cli_document_out_key_head_input.changes, CliDocumentOutKeyHeadInputCfg, .key = args->key,
                             .each = hunks_filled, .context = &fill);
        }
        g_cli_guard->persistent_buf_release(s_hunks.hunk, g_cli_widgets_owner);
        s_hunks.hunk = taken;
        s_hunks.capacity = (taken != NULL) ? total : 0u;
    }
    s_hunks.count = (s_hunks.hunk != NULL) ? total : 0u;
    if ((s_hunks.shown != CLI_HUNKS_NONE) && (s_hunks.shown >= s_hunks.count))
    {
        s_hunks.shown = CLI_HUNKS_NONE;
    }
    EMBED_CALL(hunks_titled, HunksCtx, .at = 0u);
}

unsigned int cli_hunks_count(const CliHunksCfg *args)
{
    (void)args;
    return s_hunks.count;
}

unsigned int cli_hunks_at_line(const CliHunksCfg *args)
{
    if (args->mark_out != NULL)
    {
        *args->mark_out = CLI_HUNKS_MARK_NONE;
    }
    // the last change whose first marked line is at most the line; the one before it is read too, a change taking
    // lines out marking the line above them, which the change before may end on
    const unsigned int low = EMBED_CALL(hunks_before, HunksCtx, .line = args->line);
    for (unsigned int back = 0u; (back < 2u) && (low > back); back += 1u)
    {
        const unsigned int at = low - 1u - back;
        if (args->line < (unsigned long long)EMBED_CALL(hunks_mark_to, HunksCtx, .at = at))
        {
            if (args->mark_out != NULL)
            {
                *args->mark_out = EMBED_CALL(hunks_mark_of, HunksCtx, .at = at);
            }
            return at;
        }
    }
    return CLI_HUNKS_NONE;
}

int cli_hunks_show(const CliHunksCfg *args)
{
    const unsigned int at = EMBED_CALL(cli_hunks.at_line, CliHunksCfg, .line = args->line);
    if (at == CLI_HUNKS_NONE)
    {
        return 0;
    }
    EMBED_CALL(hunks_shown, HunksCtx, .at = at);
    return 1;
}

int cli_hunks_step(const CliHunksCfg *args)
{
    const unsigned int count = s_hunks.count;
    if (count == 0u)
    {
        return 0;
    }
    unsigned int at = 0u;
    if (s_hunks.shown != CLI_HUNKS_NONE)
    {
        at = (args->by < 0) ? ((s_hunks.shown + count - 1u) % count) : ((s_hunks.shown + 1u) % count);
    }
    else if (args->by < 0)
    {
        // the last change that starts above the line, the last where none does
        const unsigned int above =
            (args->line > 0ULL) ? EMBED_CALL(hunks_before, HunksCtx, .line = args->line - 1ULL) : 0u;
        at = (above > 0u) ? above - 1u : count - 1u;
    }
    else
    {
        // the first change that starts below the line, the first where none does
        const unsigned int below = EMBED_CALL(hunks_before, HunksCtx, .line = args->line);
        at = (below < count) ? below : 0u;
    }
    EMBED_CALL(hunks_shown, HunksCtx, .at = at);
    if (args->line_out != NULL)
    {
        *args->line_out = EMBED_CALL(hunks_mark_from, HunksCtx, .at = at);
    }
    return 1;
}

void cli_hunks_close(const CliHunksCfg *args)
{
    (void)args;
    s_hunks.shown = CLI_HUNKS_NONE;
    EMBED_CALL(hunks_titled, HunksCtx, .at = 0u);
}

int cli_hunks_revert(const CliHunksCfg *args)
{
    if (s_hunks.shown == CLI_HUNKS_NONE)
    {
        mmgr_line_put(args->err_msg, "no change is shown");
        return 0;
    }
    CliShape shape = {0ULL, 0ULL};
    (void)EMBED_CALL(cli_document_text.shape, CliDocumentTextCfg, .key = s_hunks.key, .shape = &shape);
    const unsigned int *const hunk = s_hunks.hunk + (HUNKS_NUMBERS * s_hunks.shown);
    const unsigned long long lines = shape.lines;
    // the older lines' bytes at the most: three a unit and a line end each, with the line end before them at the text's
    // end and the end after them
    unsigned long long bytes = 2ULL;
    for (unsigned int line = hunk[2]; line < hunk[3]; line += 1u)
    {
        unsigned long long count = 0ULL;
        (void)EMBED_CALL(cli_document_line.head_line_units, CliDocumentLineUnitsCfg, .key = s_hunks.key, .line = line,
                         .count = &count);
        bytes += (3ULL * count) + 1ULL;
    }
    char *const text = g_cli_guard->persistent_buf_alloc(bytes, g_cli_widgets_owner);
    if (text == NULL)
    {
        mmgr_line_put(args->err_msg, "the revert's cell could not be taken");
        return 0;
    }
    g_cli_guard->persistent_buf_release(s_hunks.revert, g_cli_widgets_owner);
    s_hunks.revert = text;
    // the newer lines from their first line's start to the next line's start replaced by the older lines, each ended
    // by a line end; where the change reaches the text's last line, from the end of the line above it to the text's
    // end, the line end before the older lines in place of after the last
    const int at_end = (unsigned long long)hunk[1] >= lines;
    const int above = at_end && (hunk[0] > 0u);
    VerbaLinea out = {text, bytes, 0ULL};
    if (above)
    {
        mmgr_line_put(&out, "\n");
    }
    for (unsigned int line = hunk[2]; line < hunk[3]; line += 1u)
    {
        unsigned long long count = 0ULL;
        const unsigned short *const units = EMBED_CALL(cli_document_line.head_line_units, CliDocumentLineUnitsCfg,
                                                       .key = s_hunks.key, .line = line, .count = &count);
        if ((line != hunk[2]) && at_end)
        {
            mmgr_line_put(&out, "\n");
        }
        EMBED_CALL(cli_document_units_count.units_utf8, CliDocumentUnitsCountCfg, .out = &out, .units = units,
                   .count = count);
        if (!at_end)
        {
            mmgr_line_put(&out, "\n");
        }
    }
    const unsigned long long length = mmgr_line_finish(&out);
    CliPosition from = {hunk[0], 0ULL};
    CliPosition to = {hunk[1], 0ULL};
    if (at_end && (lines != 0ULL))
    {
        unsigned long long last = 0ULL;
        (void)EMBED_CALL(cli_document_line.line_units, CliDocumentLineUnitsCfg, .key = s_hunks.key,
                         .line = lines - 1ULL, .count = &last);
        to = (CliPosition){lines - 1ULL, last};
        if (above)
        {
            unsigned long long before = 0ULL;
            (void)EMBED_CALL(cli_document_line.line_units, CliDocumentLineUnitsCfg, .key = s_hunks.key,
                             .line = hunk[0] - 1u, .count = &before);
            from = (CliPosition){hunk[0] - 1u, before};
        }
    }
    *args->edit_out = (CliEdit){from, to, text, length};
    *args->edit_count = 1u;
    return 1;
}

int cli_hunks_key(const CliHunksCfg *args)
{
    if ((s_hunks.shown == CLI_HUNKS_NONE) ||
        !cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Escape", .cap = 16ULL}))
    {
        return 0;
    }
    EMBED_CALL(cli_hunks.close, CliHunksCfg, .at = 0u);
    return 1;
}

const char *cli_hunks_title(const CliHunksCfg *args)
{
    *args->length_out = s_hunks.title_length;
    return s_hunks.title;
}

int cli_hunks_settled(const CliHunksCfg *args)
{
    (void)args;
    return (s_hunks.shown == CLI_HUNKS_NONE) || (cli_clock_milliseconds() >= s_hunks.shown_ms + CLI_HUNKS_SETTLE_MS);
}

unsigned int cli_hunks_row_count(const CliHunksCfg *args)
{
    if (s_hunks.shown == CLI_HUNKS_NONE)
    {
        return 0u;
    }
    const unsigned int *const hunk = s_hunks.hunk + (HUNKS_NUMBERS * s_hunks.shown);
    if (args->line_out != NULL)
    {
        *args->line_out = (hunk[1] > hunk[0])
                              ? (unsigned long long)hunk[1] - 1ULL
                              : (unsigned long long)EMBED_CALL(hunks_mark_from, HunksCtx, .at = s_hunks.shown);
    }
    return (hunk[3] - hunk[2]) + (hunk[1] - hunk[0]);
}

int cli_hunks_row(const CliHunksCfg *args)
{
    if (args->at >= EMBED_CALL(cli_hunks.row_count, CliHunksCfg, .line_out = NULL))
    {
        return 0;
    }
    const unsigned int *const hunk = s_hunks.hunk + (HUNKS_NUMBERS * s_hunks.shown);
    const unsigned int removed = hunk[3] - hunk[2];
    CliHunkRow *const row = args->row;
    row->removed = args->at < removed;
    row->count = 0ULL;
    row->units = row->removed ? EMBED_CALL(cli_document_line.head_line_units, CliDocumentLineUnitsCfg,
                                           .key = s_hunks.key, .line = hunk[2] + args->at, .count = &row->count)
                              : EMBED_CALL(cli_document_line.line_units, CliDocumentLineUnitsCfg, .key = s_hunks.key,
                                           .line = hunk[0] + (args->at - removed), .count = &row->count);
    return 1;
}

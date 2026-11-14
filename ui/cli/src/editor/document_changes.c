// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// document_changes.c: a text's changes against its last commit's file answered off one pass of the store's diff each
// time they are asked for; nothing is kept between asks. A pass hands each change to a reader that keeps the one it
// seeks, or marks the lines of a run, and counts them all
#include "document_changes.h"

#include "../command_line/pool.h"

// A reader of one pass: the line sought and whether a line is sought, the place sought, the change found and its
// place, how many changes have passed, the last change passed, and the run of lines marked
typedef struct
{
    unsigned long long line;
    int by_line;
    unsigned int at;
    int found;
    CliChange change;
    unsigned int found_at;
    unsigned int count;
    int has_last;
    CliChange last;
    unsigned char *marks;
    unsigned long long marks_from;
    unsigned int marks_count;
} ChangesPass;

typedef struct
{
    const CliChange *change;
    unsigned long long line;
} ChangesCtx;

// the first line the gutter marks a change on: its first newer line, or for lines taken out alone the line above them,
// the first where none stands above
EMBED_INLINE unsigned int changes_marked_from(const ChangesCtx *args)
{
    if (args->change->now_to > args->change->now_from)
    {
        return args->change->now_from;
    }
    return (args->change->now_from > 0u) ? args->change->now_from - 1u : 0u;
}

// the line after the last the gutter marks a change on
EMBED_INLINE unsigned int changes_marked_to(const ChangesCtx *args)
{
    return (args->change->now_to > args->change->now_from) ? args->change->now_to : changes_marked_from(args) + 1u;
}

EMBED_INLINE unsigned int changes_mark(const ChangesCtx *args)
{
    if (args->change->now_to == args->change->now_from)
    {
        return CLI_CHANGE_MARK_REMOVED;
    }
    return (args->change->then_to == args->change->then_from) ? CLI_CHANGE_MARK_ADDED : CLI_CHANGE_MARK_CHANGED;
}

// whether the gutter marks `change` on `line`
EMBED_INLINE int changes_holds(const ChangesCtx *args)
{
    return (args->line >= (unsigned long long)changes_marked_from(args)) &&
           (args->line < (unsigned long long)changes_marked_to(args));
}

// a change handed by the store: counted, kept where it is the one sought, and its marks written over the run
static void changes_passed(const CliEachHunkCfg *args)
{
    ChangesPass *const pass = args->context;
    const CliChange change = {args->now_from, args->now_to, args->then_from, args->then_to};
    const int sought = pass->by_line ? EMBED_CALL(changes_holds, ChangesCtx, .change = &change, .line = pass->line)
                                     : (pass->count == pass->at);
    if (sought && !pass->found)
    {
        pass->found = 1;
        pass->change = change;
        pass->found_at = pass->count;
    }
    if (pass->marks != NULL)
    {
        const unsigned long long from = EMBED_CALL(changes_marked_from, ChangesCtx, .change = &change);
        const unsigned long long to = EMBED_CALL(changes_marked_to, ChangesCtx, .change = &change);
        const unsigned long long end = pass->marks_from + pass->marks_count;
        const unsigned long long first = (from > pass->marks_from) ? from : pass->marks_from;
        const unsigned long long last = (to < end) ? to : end;
        const unsigned char mark = (unsigned char)EMBED_CALL(changes_mark, ChangesCtx, .change = &change);
        for (unsigned long long line = first; line < last; line += 1ULL)
        {
            pass->marks[line - pass->marks_from] = mark;
        }
    }
    pass->has_last = 1;
    pass->last = change;
    pass->count += 1u;
}

// one pass over the changes of the text under `key` with `pass` as the reader: 1 where the store has the changes
static int changes_pass(const char *key, ChangesPass *pass)
{
    return EMBED_CALL(cli_document_out_key_head_input.changes, CliDocumentOutKeyHeadInputCfg, .key = key,
                      .each = changes_passed, .context = pass);
}

int cli_document_change_find(const CliChangeCfg *args)
{
    ChangesPass pass = {
        args->line, args->at == CLI_CHANGE_NONE, args->at, 0, {0u, 0u, 0u, 0u}, 0u, 0u, 0, {0u, 0u, 0u, 0u}, NULL, 0ULL,
        0u};
    const int has = changes_pass(args->key, &pass);
    if (args->count_out != NULL)
    {
        *args->count_out = has ? pass.count : 0u;
    }
    if (!has || !pass.found)
    {
        return 0;
    }
    if (args->change_out != NULL)
    {
        *args->change_out = pass.change;
    }
    if (args->at_out != NULL)
    {
        *args->at_out = pass.found_at;
    }
    return 1;
}

// A reader of the change that starts nearest a line: the first that starts at or below it and the last that starts
// above it, each with its place, and how many changes passed
typedef struct
{
    unsigned long long line;
    int has_below;
    CliChange below;
    unsigned int below_at;
    int has_above;
    CliChange above;
    unsigned int above_at;
    unsigned int count;
} ChangesNearest;

static void changes_nearest_passed(const CliEachHunkCfg *args)
{
    ChangesNearest *const nearest = args->context;
    const CliChange change = {args->now_from, args->now_to, args->then_from, args->then_to};
    const unsigned long long from = EMBED_CALL(changes_marked_from, ChangesCtx, .change = &change);
    if ((from >= nearest->line) && !nearest->has_below)
    {
        nearest->has_below = 1;
        nearest->below = change;
        nearest->below_at = nearest->count;
    }
    if (from < nearest->line)
    {
        nearest->has_above = 1;
        nearest->above = change;
        nearest->above_at = nearest->count;
    }
    nearest->count += 1u;
}

int cli_document_change_step(const CliChangeCfg *args)
{
    if (args->at != CLI_CHANGE_NONE)
    {
        unsigned int count = 0u;
        (void)EMBED_CALL(cli_document_change.find, CliChangeCfg, .key = args->key, .at = args->at, .count_out = &count);
        if (count == 0u)
        {
            return 0;
        }
        const unsigned int at = (args->by < 0) ? ((args->at + count - 1u) % count) : ((args->at + 1u) % count);
        return EMBED_CALL(cli_document_change.find, CliChangeCfg, .key = args->key, .at = at,
                          .change_out = args->change_out, .at_out = args->at_out);
    }
    // from a line: going down the first change that starts at or below it, the first of all where none does; going up
    // the last that starts above it, the last of all where none does
    ChangesNearest nearest = {
        (args->by < 0) ? args->line : args->line, 0, {0u, 0u, 0u, 0u}, 0u, 0, {0u, 0u, 0u, 0u}, 0u, 0u};
    if (!EMBED_CALL(cli_document_out_key_head_input.changes, CliDocumentOutKeyHeadInputCfg, .key = args->key,
                    .each = changes_nearest_passed, .context = &nearest) ||
        (nearest.count == 0u))
    {
        return 0;
    }
    unsigned int at = 0u;
    if ((args->by < 0) && nearest.has_above)
    {
        at = nearest.above_at;
    }
    else if (args->by < 0)
    {
        at = nearest.count - 1u;
    }
    else if (nearest.has_below)
    {
        at = nearest.below_at;
    }
    return EMBED_CALL(cli_document_change.find, CliChangeCfg, .key = args->key, .at = at,
                      .change_out = args->change_out, .at_out = args->at_out);
}

void cli_document_change_marks(const CliChangeCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->marks_out, .val = CLI_CHANGE_MARK_NONE, .bytes = args->count});
    ChangesPass pass = {
        0ULL,       0,          CLI_CHANGE_NONE, 0, {0u, 0u, 0u, 0u}, 0u, 0u, 0, {0u, 0u, 0u, 0u}, args->marks_out,
        args->line, args->count};
    (void)changes_pass(args->key, &pass);
}

unsigned int cli_document_change_mark(const CliChangeCfg *args)
{
    return EMBED_CALL(changes_mark, ChangesCtx, .change = args->change);
}

unsigned int cli_document_change_marked_from(const CliChangeCfg *args)
{
    return EMBED_CALL(changes_marked_from, ChangesCtx, .change = args->change);
}

int cli_document_change_revert(const CliChangeCfg *args)
{
    const CliChange change = *args->change;
    CliShape shape = {0ULL, 0ULL};
    (void)EMBED_CALL(cli_document_text.shape, CliDocumentTextCfg, .key = args->key, .shape = &shape);
    const unsigned long long lines = shape.lines;
    // the older lines' bytes at the most: three a unit and a line end each, with the line end before them at the text's
    // end and the end after them
    unsigned long long bytes = 2ULL;
    for (unsigned int line = change.then_from; line < change.then_to; line += 1u)
    {
        unsigned long long count = 0ULL;
        (void)EMBED_CALL(cli_document_line.head_line_units, CliDocumentLineUnitsCfg, .key = args->key, .line = line,
                         .count = &count);
        bytes += (3ULL * count) + 1ULL;
    }
    char *const text = g_cli_guard->temporary_buf_alloc(bytes, g_cli_editor_owner);
    if (text == NULL)
    {
        mmgr_line_put(args->err_msg, "the revert's cell could not be taken");
        return 0;
    }
    // the newer lines from their first line's start to the next line's start replaced by the older lines, each ended
    // by a line end; where the change reaches the text's last line, from the end of the line above it to the text's
    // end, the line end before the older lines in place of after the last
    const int at_end = (unsigned long long)change.now_to >= lines;
    const int above = at_end && (change.now_from > 0u);
    VerbaLinea out = {text, bytes, 0ULL};
    if (above)
    {
        mmgr_line_put(&out, "\n");
    }
    for (unsigned int line = change.then_from; line < change.then_to; line += 1u)
    {
        unsigned long long count = 0ULL;
        (void)EMBED_CALL(cli_document_line.head_line_units, CliDocumentLineUnitsCfg, .key = args->key, .line = line,
                         .count = &count);
        // the older line's units copied out into a cell of the temporary tier
        unsigned short *const units =
            (count != 0ULL) ? g_cli_guard->temporary_buf_alloc(count * sizeof(*units), g_cli_editor_owner) : NULL;
        if (units != NULL)
        {
            (void)EMBED_CALL(cli_document_line.head_line_units, CliDocumentLineUnitsCfg, .key = args->key, .line = line,
                             .count = &count, .dst = units, .capacity = count);
        }
        if ((line != change.then_from) && at_end)
        {
            mmgr_line_put(&out, "\n");
        }
        EMBED_CALL(cli_document_units_count.units_utf8, CliDocumentUnitsCountCfg, .out = &out, .units = units,
                   .count = (units != NULL) ? count : 0ULL);
        g_cli_guard->persistent_buf_release(units, g_cli_editor_owner);
        if (!at_end)
        {
            mmgr_line_put(&out, "\n");
        }
    }
    const unsigned long long length = mmgr_line_finish(&out);
    CliPosition from = {change.now_from, 0ULL};
    CliPosition to = {change.now_to, 0ULL};
    if (at_end && (lines != 0ULL))
    {
        unsigned long long last = 0ULL;
        (void)EMBED_CALL(cli_document_line.line_units, CliDocumentLineUnitsCfg, .key = args->key, .line = lines - 1ULL,
                         .count = &last);
        to = (CliPosition){lines - 1ULL, last};
        if (above)
        {
            unsigned long long before = 0ULL;
            (void)EMBED_CALL(cli_document_line.line_units, CliDocumentLineUnitsCfg, .key = args->key,
                             .line = change.now_from - 1u, .count = &before);
            from = (CliPosition){change.now_from - 1u, before};
        }
    }
    *args->edit_out = (CliEdit){from, to, text, length};
    return 1;
}

void cli_document_change_revert_end(const CliChangeCfg *args)
{
    // the edit's text is this module's temporary take, handed out read-only
    g_cli_guard->persistent_buf_release((void *)(embed_word)args->edit_out->text, g_cli_editor_owner);
}

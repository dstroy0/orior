// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// controller_notices.c: the notices, as statusbar.js shows them. A notice that answers what the reader did, a press or
// a key that gives a command within CLI_NOTICES_ANSWERS_MS, shows at once; any other needs nothing done and waits for a
// break in the work: a save, or the keys at rest CLI_NOTICES_REST_MS, where every one waiting but the last goes to the
// list and the last shows. Focus holds every notice until it ends. A notice's kind is its first line with each run of
// digits read as any count; a kind put away shows no more, the list still taking it. Every notice shown or set aside
// stands in the list the bell opens, the newest last, CLI_NOTICES_LISTED at the most
#include "controller_internal.h"

#include "../command_line/cells.h"
#include "../machine/state.h"
#include "../text/text.h"

// the notices the list holds at the most, and as many wait at the most; and the key of orior's folder the kinds put
// away stand under, a line each
#define CLI_NOTICES_LISTED 200u
#define CLI_NOTICES_AWAY_KEY "notices-away"

// A notice: its text, then its kind, in a cell of its own; whether it is a failure's; and the clock's reading as it
// came
typedef struct
{
    char *text;
    char *kind;
    int failed;
    unsigned long long at_ms;
} CliModelNotice;

// The notices: the list, the newest last, in a cell, how many and how many came since the bell was opened; those
// waiting for a break, in a cell, and how many; when the notice shown goes, its kind in its cell after its first line;
// when the reader last commanded and last gave any input; and the kinds put away, a line each, in a cell, NULL for none
typedef struct
{
    CliModelNotice *list;
    unsigned int listed;
    unsigned int unseen;
    CliModelNotice *waiting;
    unsigned int waiting_count;
    unsigned long long shown_until_ms;
    unsigned long long commanded_ms;
    unsigned long long input_ms;
    char *away;
} ControllerNotices;

static ControllerNotices s_notices;

typedef struct
{
    const char *text;
    const char *kind;
    unsigned long long length;
    int failed;
    int seen;
    CliModelNotice *notice;
    const CliModelNotice *notice_read;
} ControllerNoticesCtx;

// the bytes of `text`'s first line
EMBED_INLINE unsigned long long notices_first_line(const ControllerNoticesCtx *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL});
    return EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->text, .at = 0ULL,
                      .size = size, .set = "\n");
}

// `text` as a notice in a cell of its own, its kind after it, its first line each run of digits written as '#'; 1, or
// 0 where the cells cannot meet it
EMBED_INLINE int notices_made(const ControllerNoticesCtx *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL});
    const unsigned long long first = EMBED_CALL(notices_first_line, ControllerNoticesCtx, .text = args->text);
    char *const cell = g_cli_cells->persistent_buf_alloc(size + first + 2ULL, &g_cli_controller_cells);
    if (cell == NULL)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = cell, .src = args->text, .bytes = size + 1ULL});
    VerbaLinea kind = {cell + size + 1ULL, first + 1ULL, 0ULL};
    for (unsigned long long at = 0ULL; at < first;)
    {
        const unsigned long long digit =
            EMBED_CALL(cli_text_at_size_classes.to_class, CliTextAtSizeClassesCfg, .text = args->text, .at = at,
                       .size = first, .classes = CLI_TEXT_DIGITS, .set = "");
        mmgr_line_put_n(&kind, args->text + at, digit - at);
        at = EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = args->text, .at = digit,
                        .size = first, .classes = CLI_TEXT_DIGITS, .set = "");
        mmgr_line_put(&kind, (digit < first) ? "#" : "");
    }
    (void)mmgr_line_finish(&kind);
    *args->notice = (CliModelNotice){cell, cell + size + 1ULL, args->failed, cli_clock_milliseconds()};
    return 1;
}

// the notice put last in the list, its oldest let go where the list is full, counted unseen where it is not `seen`
EMBED_INLINE void notices_listed(const ControllerNoticesCtx *args)
{
    if (s_notices.list == NULL)
    {
        s_notices.list =
            g_cli_cells->persistent_buf_alloc(CLI_NOTICES_LISTED * sizeof(*s_notices.list), &g_cli_controller_cells);
    }
    if (s_notices.list == NULL)
    {
        g_cli_cells->persistent_buf_release(args->notice_read->text, &g_cli_controller_cells);
        return;
    }
    if (s_notices.listed == CLI_NOTICES_LISTED)
    {
        g_cli_cells->persistent_buf_release(s_notices.list[0].text, &g_cli_controller_cells);
        memor.move_down(&(MemoriaCfg){.dst = &s_notices.list[0],
                                      .src = &s_notices.list[1],
                                      .bytes = (size_t)(CLI_NOTICES_LISTED - 1u) * sizeof(*s_notices.list)});
        s_notices.listed -= 1u;
    }
    s_notices.list[s_notices.listed] = *args->notice_read;
    s_notices.listed += 1u;
    s_notices.unseen += args->seen ? 0u : 1u;
}

// the notice shown on the status bar: its first line, then its kind, in a cell of the model's, standing until its time
// is out; the notice itself put in the list
EMBED_INLINE void notices_shown(const ControllerNoticesCtx *args)
{
    const CliModelNotice notice = *args->notice_read;
    const unsigned long long first = EMBED_CALL(notices_first_line, ControllerNoticesCtx, .text = notice.text);
    const unsigned long long kind = cellul.len(&(CatenaFinitaCfg){.src = notice.kind, .cap = CLI_CELLS_POOL});
    char *const shown = g_cli_cells->persistent_buf_alloc(first + kind + 2ULL, &g_cli_controller_cells);
    if (shown != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = shown, .src = notice.text, .bytes = first});
        shown[first] = '\0';
        memor.cpy(&(MemoriaCfg){.dst = shown + first + 1ULL, .src = notice.kind, .bytes = kind + 1ULL});
        g_cli_cells->persistent_buf_release(g_cli_model.notice, &g_cli_controller_cells);
        g_cli_model.notice = shown;
        g_cli_model.notice_failed = notice.failed;
        s_notices.shown_until_ms =
            cli_clock_milliseconds() + (notice.failed ? CLI_NOTICES_FAILED_MS : CLI_NOTICES_SHOWN_MS);
        g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_STATUS);
    }
    EMBED_CALL(notices_listed, ControllerNoticesCtx, .notice_read = &notice, .seen = 0);
}

// the notice shown taken off the status bar
EMBED_INLINE void notices_hidden(const ControllerNoticesCtx *args)
{
    (void)args;
    if (g_cli_model.notice != NULL)
    {
        g_cli_cells->persistent_buf_release(g_cli_model.notice, &g_cli_controller_cells);
        g_cli_model.notice = NULL;
        g_cli_model.notice_failed = 0;
        g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_STATUS);
    }
}

// whether the kind `kind` is put away
EMBED_INLINE int notices_away(const ControllerNoticesCtx *args)
{
    const char *line = s_notices.away;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->kind, .cap = CLI_CELLS_POOL});
    while ((line != NULL) && (*line != '\0'))
    {
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL});
        const unsigned long long end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                  .text = line, .at = 0ULL, .size = size, .set = "\n");
        if ((end == length) && (memor.cmp(&(MemoriaCfg){.src = line, .other = args->kind, .bytes = length}) == 0))
        {
            return 1;
        }
        line += (end < size) ? (end + 1ULL) : end;
    }
    return 0;
}

// the kinds put away written as `away`, `length` bytes, and saved in orior's folder; the cells' refusal leaves them
EMBED_INLINE void notices_away_set(const ControllerNoticesCtx *args)
{
    char *const away = g_cli_cells->persistent_buf_alloc(args->length + 1ULL, &g_cli_controller_cells);
    if (away == NULL)
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = away, .src = args->text, .bytes = args->length});
    away[args->length] = '\0';
    g_cli_cells->persistent_buf_release(s_notices.away, &g_cli_controller_cells);
    s_notices.away = away;
    EMBED_CALL(cli_model.value_save, CliModelCtx, .path = CLI_NOTICES_AWAY_KEY, .value = away);
}

// the notices waiting for a break taken: every one but the last into the list, the last shown; none while Focus holds
EMBED_INLINE void notices_break(const ControllerNoticesCtx *args)
{
    (void)args;
    if (g_cli_model.focus || (s_notices.waiting_count == 0u))
    {
        return;
    }
    for (unsigned int at = 0u; (at + 1u) < s_notices.waiting_count; at += 1u)
    {
        EMBED_CALL(notices_listed, ControllerNoticesCtx, .notice_read = &s_notices.waiting[at], .seen = 0);
    }
    const CliModelNotice last = s_notices.waiting[s_notices.waiting_count - 1u];
    s_notices.waiting_count = 0u;
    g_cli_model.focus_waiting = 0u;
    EMBED_CALL(notices_shown, ControllerNoticesCtx, .notice_read = &last);
}

void cli_model_notice_post(const CliModelCtx *args)
{
    CliModelNotice notice;
    if (!EMBED_CALL(notices_made, ControllerNoticesCtx, .text = args->path, .failed = args->on, .notice = &notice))
    {
        return;
    }
    if (EMBED_CALL(notices_away, ControllerNoticesCtx, .kind = notice.kind))
    {
        EMBED_CALL(notices_listed, ControllerNoticesCtx, .notice_read = &notice, .seen = 1);
        return;
    }
    const unsigned long long now = cli_clock_milliseconds();
    if (!g_cli_model.focus && ((now - s_notices.commanded_ms) <= CLI_NOTICES_ANSWERS_MS))
    {
        EMBED_CALL(notices_shown, ControllerNoticesCtx, .notice_read = &notice);
        return;
    }
    if (s_notices.waiting == NULL)
    {
        s_notices.waiting =
            g_cli_cells->persistent_buf_alloc(CLI_NOTICES_LISTED * sizeof(*s_notices.waiting), &g_cli_controller_cells);
    }
    if (s_notices.waiting == NULL)
    {
        EMBED_CALL(notices_listed, ControllerNoticesCtx, .notice_read = &notice, .seen = 0);
        return;
    }
    // the waiting full, the oldest of them goes to the list
    if (s_notices.waiting_count == CLI_NOTICES_LISTED)
    {
        EMBED_CALL(notices_listed, ControllerNoticesCtx, .notice_read = &s_notices.waiting[0], .seen = 0);
        memor.move_down(&(MemoriaCfg){.dst = &s_notices.waiting[0],
                                      .src = &s_notices.waiting[1],
                                      .bytes = (size_t)(CLI_NOTICES_LISTED - 1u) * sizeof(*s_notices.waiting)});
        s_notices.waiting_count -= 1u;
    }
    s_notices.waiting[s_notices.waiting_count] = notice;
    s_notices.waiting_count += 1u;
    if (g_cli_model.focus)
    {
        g_cli_model.focus_waiting = s_notices.waiting_count;
        g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_STATUS);
    }
}

void cli_model_notices_input(const CliModelCtx *args)
{
    const CliInput *const input = (const CliInput *)args->input;
    const unsigned long long now = cli_clock_milliseconds();
    s_notices.input_ms = now;
    if (input->kind == CLI_INPUT_POINTER_DOWN)
    {
        s_notices.commanded_ms = now;
        return;
    }
    if (input->kind != CLI_INPUT_KEY)
    {
        return;
    }
    // a key with a modifier, a function key, or any key the editor does not take, gives a command; the rest type
    const char *const name = (args->path != NULL) ? args->path : "";
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_LABEL_MAX_LEN});
    const int modified = cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "Ctrl+", .cap = CLI_LABEL_MAX_LEN}) ||
                         cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "Alt+", .cap = CLI_LABEL_MAX_LEN}) ||
                         cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "Meta+", .cap = CLI_LABEL_MAX_LEN}) ||
                         cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "Mod+", .cap = CLI_LABEL_MAX_LEN});
    const int function = (length >= 2ULL) && (name[0] == 'F') &&
                         (EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = name,
                                     .at = 1ULL, .size = length, .classes = CLI_TEXT_DIGITS, .set = "") == length);
    if (modified || function || !g_cli_model.editing)
    {
        s_notices.commanded_ms = now;
    }
}

void cli_model_notices_break(const CliModelCtx *args)
{
    (void)args;
    EMBED_CALL(notices_break, ControllerNoticesCtx, .seen = 0);
}

void cli_model_notices_press(const CliModelCtx *args)
{
    if (args->at == CLI_HIT_STATUS_FOCUS)
    {
        EMBED_CALL(cli_model_notices.focus, CliModelCtx, .on = 0);
        return;
    }
    if ((args->at == CLI_HIT_STATUS_SILENCE) && (g_cli_model.notice != NULL))
    {
        // the kind stands after the first line in the notice's cell
        const char *const kind = g_cli_model.notice +
                                 cellul.len(&(CatenaFinitaCfg){.src = g_cli_model.notice, .cap = CLI_CELLS_POOL}) +
                                 1ULL;
        const unsigned long long had =
            (s_notices.away != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = s_notices.away, .cap = CLI_CELLS_POOL})
                                     : 0ULL;
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = kind, .cap = CLI_CELLS_POOL});
        char *const joined = g_cli_cells->persistent_buf_alloc(had + length + 2ULL, &g_cli_controller_cells);
        if ((joined != NULL) && !EMBED_CALL(notices_away, ControllerNoticesCtx, .kind = kind))
        {
            VerbaLinea line = {joined, had + length + 2ULL, 0ULL};
            mmgr_line_put(&line, (s_notices.away != NULL) ? s_notices.away : "");
            mmgr_line_put(&line, (had != 0ULL) ? "\n" : "");
            mmgr_line_put(&line, kind);
            EMBED_CALL(notices_away_set, ControllerNoticesCtx, .text = joined, .length = mmgr_line_finish(&line));
        }
        g_cli_cells->persistent_buf_release(joined, &g_cli_controller_cells);
    }
    if ((args->at == CLI_HIT_STATUS_NOTICE) || (args->at == CLI_HIT_STATUS_SILENCE))
    {
        EMBED_CALL(notices_hidden, ControllerNoticesCtx, .seen = 0);
    }
}

void cli_model_notices_focus(const CliModelCtx *args)
{
    g_cli_model.focus = args->on;
    g_cli_model.focus_waiting = g_cli_model.focus ? s_notices.waiting_count : 0u;
    g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_STATUS) | CLI_MODEL_DIRTY(CLI_SNAPSHOT_MENUS);
    if (!g_cli_model.focus)
    {
        EMBED_CALL(notices_break, ControllerNoticesCtx, .seen = 0);
    }
}

void cli_model_notices_show_again(const CliModelCtx *args)
{
    if ((s_notices.away == NULL) || !EMBED_CALL(notices_away, ControllerNoticesCtx, .kind = args->path))
    {
        return;
    }
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = s_notices.away, .cap = CLI_CELLS_POOL});
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL});
    char *const left = g_cli_cells->persistent_buf_alloc(size + 1ULL, &g_cli_controller_cells);
    if (left == NULL)
    {
        return;
    }
    VerbaLinea line = {left, size + 1ULL, 0ULL};
    for (unsigned long long at = 0ULL; at < size;)
    {
        const unsigned long long end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                  .text = s_notices.away, .at = at, .size = size, .set = "\n");
        const int same =
            ((end - at) == length) &&
            (memor.cmp(&(MemoriaCfg){.src = s_notices.away + at, .other = args->path, .bytes = length}) == 0);
        if (!same)
        {
            mmgr_line_put(&line, (line.at != 0ULL) ? "\n" : "");
            mmgr_line_put_n(&line, s_notices.away + at, end - at);
        }
        at = end + 1ULL;
    }
    EMBED_CALL(notices_away_set, ControllerNoticesCtx, .text = left, .length = mmgr_line_finish(&line));
    g_cli_cells->persistent_buf_release(left, &g_cli_controller_cells);
}

int cli_model_notices_step(const CliModelCtx *args)
{
    (void)args;
    const unsigned long long now = cli_clock_milliseconds();
    int did = 0;
    if ((g_cli_model.notice != NULL) && (now >= s_notices.shown_until_ms))
    {
        EMBED_CALL(notices_hidden, ControllerNoticesCtx, .seen = 0);
        did = 1;
    }
    if (!g_cli_model.focus && (s_notices.waiting_count != 0u) && ((now - s_notices.input_ms) >= CLI_NOTICES_REST_MS))
    {
        EMBED_CALL(notices_break, ControllerNoticesCtx, .seen = 0);
        did = 1;
    }
    return did;
}

unsigned long long cli_model_notices_due_ms(const CliModelCtx *args)
{
    (void)args;
    const unsigned long long shown = (g_cli_model.notice != NULL) ? s_notices.shown_until_ms : 0ULL;
    const unsigned long long rest =
        (!g_cli_model.focus && (s_notices.waiting_count != 0u)) ? (s_notices.input_ms + CLI_NOTICES_REST_MS) : 0ULL;
    if ((shown == 0ULL) || ((rest != 0ULL) && (rest < shown)))
    {
        return rest;
    }
    return shown;
}

void cli_model_notices_restore(const CliModelCtx *args)
{
    const char *const away = cli_state_entry_value((const CliStateEntries *)args->input, CLI_NOTICES_AWAY_KEY);
    if (away == NULL)
    {
        return;
    }
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = away, .cap = CLI_CELLS_POOL});
    char *const copy = g_cli_cells->persistent_buf_alloc(size + 1ULL, &g_cli_controller_cells);
    if (copy != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = copy, .src = away, .bytes = size + 1ULL});
        g_cli_cells->persistent_buf_release(s_notices.away, &g_cli_controller_cells);
        s_notices.away = copy;
    }
}

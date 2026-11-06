// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// notices.c: the notices, as statusbar.js shows them. A notice that answers what the reader did, a press or a key that
// gives a command within CLI_NOTICES_ANSWERS_MS, shows at once; any other needs nothing done and waits for a break in
// the work: a save, or the keys at rest CLI_NOTICES_REST_MS, where every one waiting but the last goes to the list and
// the last shows. Focus stops every notice until it ends. A notice's kind is its first line with each run of digits
// read as any count; a kind put away shows no more, the list still taking it. Every notice shown or set aside stands
// in the list the bell opens, the newest last, CLI_NOTICES_LISTED at the most
#include "command_line.h"

#include "../machine/state.h"
#include "../text/text.h"
#include "commands.h"
#include "pool.h"

// the notices the list takes at the most, and as many wait at the most; and the key of orior's folder the kinds put
// away stand under, a line each
#define CLI_NOTICES_LISTED 200u
#define CLI_NOTICES_AWAY_KEY "notices-away"

// A notice in one cell: its first line and a NUL, the lines after its first and a NUL, then its kind and a NUL; where
// the lines after the first stand and where its kind stands; whether it is a failure's; and the clock's reading as it
// came
typedef struct
{
    char *cell;
    const char *rest;
    const char *kind;
    int failed;
    unsigned long long at_ms;
} CliNotice;

// The notices: the list, a ring in a cell, the oldest's place, how many and how many came since the bell was opened;
// those waiting for a break, a ring in a cell, the oldest's place and how many; the notice shown, read in place in its
// cell in the list, NULL for none, its kind, and whether it is a failure's; when it goes; whether Focus is on and how
// many wait on it; when the reader last commanded and last gave any input; and the kinds put away, a line each, NULL
// for none, in a cell of the notices' own where `away_owned` is 1 and else read in place where orior's folder was read
typedef struct
{
    CliNotice *list;
    unsigned int list_first;
    unsigned int listed;
    unsigned int unseen;
    CliNotice *waiting;
    unsigned int waiting_first;
    unsigned int waiting_count;
    const char *shown;
    const char *shown_kind;
    int shown_failed;
    unsigned long long shown_until_ms;
    int focus;
    unsigned int focus_waiting;
    unsigned long long commanded_ms;
    unsigned long long input_ms;
    char *away;
    int away_owned;
} CliNotices;

static CliNotices s_notices;

typedef struct
{
    const char *text;
    char *owned;
    const char *kind;
    int failed;
    int seen;
    CliNotice *notice;
    const CliNotice *notice_read;
} CliNoticesCtx;

// the bytes of `text`'s first line
EMBED_INLINE unsigned long long notices_first_line(const CliNoticesCtx *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES});
    return EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->text, .at = 0ULL,
                      .size = size, .set = "\n");
}

// `text`, the caller's and gone once it returns, written once as a notice in a cell of its own: its first line and a
// NUL, the lines after it and a NUL, then its kind, its first line each run of digits written as '#'; 1, or 0 where the
// cells cannot meet it
EMBED_INLINE int notices_built(const CliNoticesCtx *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES});
    const unsigned long long first = EMBED_CALL(notices_first_line, CliNoticesCtx, .text = args->text);
    char *const cell = g_cli_guard->persistent_buf_alloc(size + first + 2ULL, g_cli_command_line_owner);
    if (cell == NULL)
    {
        return 0;
    }
    // the newline that ends the first line becomes its NUL, the lines after it standing where they stood
    memor.cpy(&(MemoriaCfg){.dst = cell, .src = args->text, .bytes = size + 1ULL});
    cell[first] = '\0';
    const char *const rest = (first < size) ? (cell + first + 1ULL) : (cell + first);
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
    *args->notice = (CliNotice){cell, rest, cell + size + 1ULL, args->failed, cli_clock_milliseconds()};
    return 1;
}

// the notice shown taken off the status bar, its cell standing in the list
EMBED_INLINE void notices_hidden(const CliNoticesCtx *args)
{
    (void)args;
    s_notices.shown = NULL;
    s_notices.shown_kind = NULL;
    s_notices.shown_failed = 0;
}

// a notice's cell let go, the notice shown taken off the status bar first where it is that one
EMBED_INLINE void notices_released(const CliNoticesCtx *args)
{
    if (args->notice_read->cell == s_notices.shown)
    {
        EMBED_CALL(notices_hidden, CliNoticesCtx, .seen = 0);
    }
    g_cli_guard->persistent_buf_release(args->notice_read->cell, g_cli_command_line_owner);
}

// the notice put last in the list, the oldest let go where the list is full, counted unseen where it is not `seen`: 1,
// or 0 where no list could be taken and the notice is let go
EMBED_INLINE int notices_listed(const CliNoticesCtx *args)
{
    if (s_notices.list == NULL)
    {
        s_notices.list =
            g_cli_guard->persistent_buf_alloc(CLI_NOTICES_LISTED * sizeof(*s_notices.list), g_cli_command_line_owner);
    }
    if (s_notices.list == NULL)
    {
        EMBED_CALL(notices_released, CliNoticesCtx, .notice_read = args->notice_read);
        return 0;
    }
    if (s_notices.listed == CLI_NOTICES_LISTED)
    {
        EMBED_CALL(notices_released, CliNoticesCtx, .notice_read = &s_notices.list[s_notices.list_first]);
        s_notices.list_first = (s_notices.list_first + 1u) % CLI_NOTICES_LISTED;
        s_notices.listed -= 1u;
    }
    s_notices.list[(s_notices.list_first + s_notices.listed) % CLI_NOTICES_LISTED] = *args->notice_read;
    s_notices.listed += 1u;
    s_notices.unseen += args->seen ? 0u : 1u;
    return 1;
}

// the notice put in the list and shown on the status bar, its first line read in place in its cell, standing until its
// time is out
EMBED_INLINE void notices_shown(const CliNoticesCtx *args)
{
    const CliNotice notice = *args->notice_read;
    if (!EMBED_CALL(notices_listed, CliNoticesCtx, .notice_read = &notice, .seen = 0))
    {
        return;
    }
    s_notices.shown = notice.cell;
    s_notices.shown_kind = notice.kind;
    s_notices.shown_failed = notice.failed;
    s_notices.shown_until_ms =
        cli_clock_milliseconds() + (notice.failed ? CLI_NOTICES_FAILED_MS : CLI_NOTICES_SHOWN_MS);
}

// whether the kind `kind` is put away
EMBED_INLINE int notices_away(const CliNoticesCtx *args)
{
    const char *line = s_notices.away;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->kind, .cap = CLI_POOL_BYTES});
    while ((line != NULL) && (*line != '\0'))
    {
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_POOL_BYTES});
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

// the kinds put away, the cell `owned` they were written in taken as they stand, and saved in orior's folder
EMBED_INLINE void notices_away_set(const CliNoticesCtx *args)
{
    char *const away = args->owned;
    if (s_notices.away_owned)
    {
        g_cli_guard->persistent_buf_release(s_notices.away, g_cli_command_line_owner);
    }
    s_notices.away = away;
    s_notices.away_owned = 1;
    char folder[CLI_PATH_MAX_LEN];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder))
    {
        (void)cli_state_value_save(
            &(CliStateValueCfg){.folder = folder, .key = CLI_NOTICES_AWAY_KEY, .value = away, .err_msg = &err_msg});
    }
}

// the notices waiting for a break taken: every one but the last into the list, the last shown; none while Focus is on
EMBED_INLINE void notices_break(const CliNoticesCtx *args)
{
    (void)args;
    if (s_notices.focus || (s_notices.waiting_count == 0u))
    {
        return;
    }
    for (unsigned int at = 0u; (at + 1u) < s_notices.waiting_count; at += 1u)
    {
        (void)EMBED_CALL(notices_listed, CliNoticesCtx,
                         .notice_read = &s_notices.waiting[(s_notices.waiting_first + at) % CLI_NOTICES_LISTED],
                         .seen = 0);
    }
    const CliNotice last =
        s_notices.waiting[(s_notices.waiting_first + s_notices.waiting_count - 1u) % CLI_NOTICES_LISTED];
    s_notices.waiting_first = 0u;
    s_notices.waiting_count = 0u;
    s_notices.focus_waiting = 0u;
    EMBED_CALL(notices_shown, CliNoticesCtx, .notice_read = &last);
}

void cli_notice_post(const char *text, int failed)
{
    CliNotice notice;
    if (!EMBED_CALL(notices_built, CliNoticesCtx, .text = text, .failed = failed, .notice = &notice))
    {
        return;
    }
    if (EMBED_CALL(notices_away, CliNoticesCtx, .kind = notice.kind))
    {
        (void)EMBED_CALL(notices_listed, CliNoticesCtx, .notice_read = &notice, .seen = 1);
        return;
    }
    const unsigned long long now = cli_clock_milliseconds();
    if (!s_notices.focus && ((now - s_notices.commanded_ms) <= CLI_NOTICES_ANSWERS_MS))
    {
        EMBED_CALL(notices_shown, CliNoticesCtx, .notice_read = &notice);
        return;
    }
    if (s_notices.waiting == NULL)
    {
        s_notices.waiting = g_cli_guard->persistent_buf_alloc(CLI_NOTICES_LISTED * sizeof(*s_notices.waiting),
                                                              g_cli_command_line_owner);
    }
    if (s_notices.waiting == NULL)
    {
        (void)EMBED_CALL(notices_listed, CliNoticesCtx, .notice_read = &notice, .seen = 0);
        return;
    }
    // the waiting full, the oldest of them goes to the list
    if (s_notices.waiting_count == CLI_NOTICES_LISTED)
    {
        (void)EMBED_CALL(notices_listed, CliNoticesCtx, .notice_read = &s_notices.waiting[s_notices.waiting_first],
                         .seen = 0);
        s_notices.waiting_first = (s_notices.waiting_first + 1u) % CLI_NOTICES_LISTED;
        s_notices.waiting_count -= 1u;
    }
    s_notices.waiting[(s_notices.waiting_first + s_notices.waiting_count) % CLI_NOTICES_LISTED] = notice;
    s_notices.waiting_count += 1u;
    if (s_notices.focus)
    {
        s_notices.focus_waiting = s_notices.waiting_count;
    }
}

void cli_notices_input(void)
{
    s_notices.input_ms = cli_clock_milliseconds();
}

void cli_notices_pointer(void)
{
    const unsigned long long now = cli_clock_milliseconds();
    s_notices.input_ms = now;
    s_notices.commanded_ms = now;
}

void cli_notices_key(const char *name, int editing)
{
    const unsigned long long now = cli_clock_milliseconds();
    s_notices.input_ms = now;
    // a key with a modifier, a function key, or any key the editor does not take, gives a command; the rest type
    const char *const key = (name != NULL) ? name : "";
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_LABEL_MAX_LEN});
    const int modified = cellul.starts(&(CatenaFinitaCfg){.src = key, .other = "Ctrl+", .cap = CLI_LABEL_MAX_LEN}) ||
                         cellul.starts(&(CatenaFinitaCfg){.src = key, .other = "Alt+", .cap = CLI_LABEL_MAX_LEN}) ||
                         cellul.starts(&(CatenaFinitaCfg){.src = key, .other = "Meta+", .cap = CLI_LABEL_MAX_LEN}) ||
                         cellul.starts(&(CatenaFinitaCfg){.src = key, .other = "Mod+", .cap = CLI_LABEL_MAX_LEN});
    const int function = (length >= 2ULL) && (key[0] == 'F') &&
                         (EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = key,
                                     .at = 1ULL, .size = length, .classes = CLI_TEXT_DIGITS, .set = "") == length);
    if (modified || function || !editing)
    {
        s_notices.commanded_ms = now;
    }
}

void cli_notices_break(void)
{
    EMBED_CALL(notices_break, CliNoticesCtx, .seen = 0);
}

void cli_notices_press(unsigned int part)
{
    if (part == CLI_NOTICES_PRESS_FOCUS)
    {
        cli_notices_focus(0);
        return;
    }
    const char *const kind = s_notices.shown_kind;
    if ((part == CLI_NOTICES_PRESS_SILENCE) && (kind != NULL) && !EMBED_CALL(notices_away, CliNoticesCtx, .kind = kind))
    {
        const unsigned long long had =
            (s_notices.away != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = s_notices.away, .cap = CLI_POOL_BYTES})
                                     : 0ULL;
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = kind, .cap = CLI_POOL_BYTES});
        char *const joined = g_cli_guard->persistent_buf_alloc(had + length + 2ULL, g_cli_command_line_owner);
        if (joined != NULL)
        {
            VerbaLinea line = {joined, had + length + 2ULL, 0ULL};
            mmgr_line_put(&line, (s_notices.away != NULL) ? s_notices.away : "");
            mmgr_line_put(&line, (had != 0ULL) ? "\n" : "");
            mmgr_line_put(&line, kind);
            (void)mmgr_line_finish(&line);
            EMBED_CALL(notices_away_set, CliNoticesCtx, .owned = joined);
        }
    }
    if ((part == CLI_NOTICES_PRESS_NOTICE) || (part == CLI_NOTICES_PRESS_SILENCE))
    {
        EMBED_CALL(notices_hidden, CliNoticesCtx, .seen = 0);
    }
}

void cli_notices_focus(int on)
{
    s_notices.focus = on;
    s_notices.focus_waiting = on ? s_notices.waiting_count : 0u;
    if (!on)
    {
        EMBED_CALL(notices_break, CliNoticesCtx, .seen = 0);
    }
}

void cli_notices_show_again(const char *kind)
{
    if ((s_notices.away == NULL) || !EMBED_CALL(notices_away, CliNoticesCtx, .kind = kind))
    {
        return;
    }
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = s_notices.away, .cap = CLI_POOL_BYTES});
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = kind, .cap = CLI_POOL_BYTES});
    char *const left = g_cli_guard->persistent_buf_alloc(size + 1ULL, g_cli_command_line_owner);
    if (left == NULL)
    {
        return;
    }
    VerbaLinea line = {left, size + 1ULL, 0ULL};
    for (unsigned long long at = 0ULL; at < size;)
    {
        const unsigned long long end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                  .text = s_notices.away, .at = at, .size = size, .set = "\n");
        const int same = ((end - at) == length) &&
                         (memor.cmp(&(MemoriaCfg){.src = s_notices.away + at, .other = kind, .bytes = length}) == 0);
        if (!same)
        {
            mmgr_line_put(&line, (line.at != 0ULL) ? "\n" : "");
            mmgr_line_put_n(&line, s_notices.away + at, end - at);
        }
        at = end + 1ULL;
    }
    (void)mmgr_line_finish(&line);
    EMBED_CALL(notices_away_set, CliNoticesCtx, .owned = left);
}

int cli_notices_step(void)
{
    const unsigned long long now = cli_clock_milliseconds();
    int did = 0;
    if ((s_notices.shown != NULL) && (now >= s_notices.shown_until_ms))
    {
        EMBED_CALL(notices_hidden, CliNoticesCtx, .seen = 0);
        did = 1;
    }
    if (!s_notices.focus && (s_notices.waiting_count != 0u) && ((now - s_notices.input_ms) >= CLI_NOTICES_REST_MS))
    {
        EMBED_CALL(notices_break, CliNoticesCtx, .seen = 0);
        did = 1;
    }
    return did;
}

unsigned long long cli_notices_due_ms(void)
{
    const unsigned long long shown = (s_notices.shown != NULL) ? s_notices.shown_until_ms : 0ULL;
    const unsigned long long rest =
        (!s_notices.focus && (s_notices.waiting_count != 0u)) ? (s_notices.input_ms + CLI_NOTICES_REST_MS) : 0ULL;
    if ((shown == 0ULL) || ((rest != 0ULL) && (rest < shown)))
    {
        return rest;
    }
    return shown;
}

void cli_notices_restore(const CliStateEntries *entries)
{
    const char *const away = (entries != NULL) ? cli_state_entry_value(entries, CLI_NOTICES_AWAY_KEY) : NULL;
    if (away == NULL)
    {
        return;
    }
    if (s_notices.away_owned)
    {
        g_cli_guard->persistent_buf_release(s_notices.away, g_cli_command_line_owner);
    }
    // read in place in the entries, which stand as long as the window does; a kind put away later writes a cell of
    // the notices' own
    s_notices.away = (char *)away;
    s_notices.away_owned = 0;
}

void cli_notices_end(void)
{
    for (unsigned int at = 0u; at < s_notices.listed; at += 1u)
    {
        g_cli_guard->persistent_buf_release(s_notices.list[(s_notices.list_first + at) % CLI_NOTICES_LISTED].cell,
                                            g_cli_command_line_owner);
    }
    for (unsigned int at = 0u; at < s_notices.waiting_count; at += 1u)
    {
        g_cli_guard->persistent_buf_release(s_notices.waiting[(s_notices.waiting_first + at) % CLI_NOTICES_LISTED].cell,
                                            g_cli_command_line_owner);
    }
    g_cli_guard->persistent_buf_release(s_notices.list, g_cli_command_line_owner);
    g_cli_guard->persistent_buf_release(s_notices.waiting, g_cli_command_line_owner);
    if (s_notices.away_owned)
    {
        g_cli_guard->persistent_buf_release(s_notices.away, g_cli_command_line_owner);
    }
    s_notices = (CliNotices){0};
}

const char *cli_notice_shown(void)
{
    return s_notices.shown;
}

int cli_notice_failed(void)
{
    return s_notices.shown_failed;
}

int cli_notices_focused(void)
{
    return s_notices.focus;
}

unsigned int cli_notices_waiting(void)
{
    return s_notices.focus_waiting;
}

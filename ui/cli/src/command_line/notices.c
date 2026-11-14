// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// notices.c: the notices, as statusbar.js shows them. A notice that answers what the reader did, a press or a key that
// gives a command within CLI_NOTICES_ANSWERS_MS, shows at once; any other needs nothing done and waits for a break in
// the work: a save, or the keys at rest CLI_NOTICES_REST_MS, where every one waiting but the last goes to the list and
// the last shows. Focus stops every notice until it ends. A notice's type is its first line with each run of digits
// read as any count; a type put away shows no more, the list still taking it. Every notice shown or set aside stands
// in the list the bell opens, the newest last, CLI_NOTICES_LISTED at the most. A notice's text and the types put away
// are movable cells, each read by a copy into a temporary cell; the two rings are MMgr's, each in one fixed cell
#include "command_line.h"

#include "../machine/state.h"
#include "../text/text.h"
#include "commands.h"
#include "memoria_anularis/memoria_anularis.h"
#include "notices_internal.h"
#include "pool.h"
#include "rings.h"

// the key of orior's folder the types put away stand under, a line each
#define CLI_NOTICES_AWAY_KEY "notices-away"

// A ring of notices, MMgr's: the ring over its segments, a notice each, the notices the rings block's
typedef struct
{
    mmgr_ring ring;
    CliNotice *notice;
} CliNoticesRing;

// The notices: the list, a ring, NULL until the first notice, and how many came since the bell was opened; those
// waiting for a break, a ring, NULL until the first; the notice shown, its cell in the list, NULL for none, its first
// line and its type copied out, and whether it is a failure's; when it goes; whether Focus is on and how many wait on
// it; when the reader last commanded and last gave any input; and the types put away, a line each: in a movable cell
// of the notices' own with its bytes where `away_owned` is 1, else read in place where orior's folder was read, NULL
// for none
typedef struct
{
    CliNoticesRing *list;
    unsigned int unseen;
    CliNoticesRing *waiting;
    const CarcerMovableEntry *shown;
    char shown_text[CLI_PATH_MAX_LEN];
    char shown_type[CLI_PATH_MAX_LEN];
    int shown_failed;
    unsigned long long shown_until_ms;
    int focus;
    unsigned int focus_waiting;
    unsigned long long commanded_ms;
    unsigned long long input_ms;
    const CarcerMovableEntry *away;
    unsigned long long away_size;
    const char *away_read;
    int away_owned;
} CliNotices;

static CliNotices s_notices;

typedef struct
{
    const char *text;
    const char *owned;
    const char *type;
    int failed;
    int seen;
    CliNotice *notice;
    const CliNotice *notice_read;
    CliNoticesRing *ring;
    unsigned int at;
} CliNoticesCtx;

// the ring laid where none stands: the ring, its notices the rings block's part, so it punches no hole in the
// persistent tier
EMBED_INLINE CliNoticesRing *notices_ring_taken(const CliNoticesCtx *args)
{
    static CliNoticesRing s_ring;
    if (args->ring != NULL)
    {
        return args->ring;
    }
    s_ring.notice = (CliNotice *)(void *)EMBED_CALL(cli_rings.part, CliRingsPartCfg, .part = CLI_RINGS_NOTICES);
    (void)anularis.init(&(AnularisCfg){.ring = &s_ring.ring,
                                       .buf = (embed_u8 *)(void *)s_ring.notice,
                                       .capacity = CLI_NOTICES_RING_BYTES,
                                       .segment_count = CLI_NOTICES_RING,
                                       .segment_bytes = sizeof(CliNotice)});
    return &s_ring;
}

// the notices a ring holds, 0 for a ring not yet taken
EMBED_INLINE unsigned int notices_count(const CliNoticesCtx *args)
{
    // the segments in flight are at most CLI_NOTICES_LISTED, inside an unsigned int
    return (args->ring != NULL) ? (unsigned int)anularis.seg_inflight(&(AnularisCfg){.ring = &args->ring->ring}) : 0u;
}

// the notice `at` past a ring's oldest
EMBED_INLINE CliNotice *notices_at(const CliNoticesCtx *args)
{
    size_t front = 0u;
    (void)anularis.seg_front(&(AnularisCfg){.ring = &args->ring->ring, .out_index = &front});
    return &args->ring->notice[MMGR_RING_WRAP(front + args->at, CLI_NOTICES_RING)];
}

// the notice put after a ring's newest
EMBED_INLINE void notices_put(const CliNoticesCtx *args)
{
    size_t segment = 0u;
    (void)anularis.seg_next(&(AnularisCfg){.ring = &args->ring->ring, .out_index = &segment});
    args->ring->notice[segment] = *args->notice_read;
    anularis.seg_publish(&(AnularisCfg){.ring = &args->ring->ring});
}

// a notice's text copied into a temporary cell: its first line at the start, the lines after it at `rest_at` and its
// type at `type_at`; NULL where the cell could not be taken; the caller gives the copy back
EMBED_INLINE char *notices_text_copy(const CliNoticesCtx *args)
{
    char *const copy = g_cli_guard->temporary_buf_alloc(args->notice_read->size, g_cli_command_line_owner);
    if (copy != NULL)
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = args->notice_read->cell, .dst = copy, .bytes = args->notice_read->size});
    }
    return copy;
}

// the types put away copied into a temporary cell with their NUL, NULL where none are or the cell could not be taken;
// the caller gives the copy back
EMBED_INLINE char *notices_away_copy(const CliNoticesCtx *args)
{
    (void)args;
    if (s_notices.away_owned)
    {
        char *const copy = g_cli_guard->temporary_buf_alloc(s_notices.away_size + 1ULL, g_cli_command_line_owner);
        if (copy != NULL)
        {
            (void)g_cli_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = s_notices.away, .dst = copy, .bytes = s_notices.away_size});
            copy[s_notices.away_size] = '\0';
        }
        return copy;
    }
    if (s_notices.away_read == NULL)
    {
        return NULL;
    }
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = s_notices.away_read, .cap = CLI_POOL_BYTES});
    char *const copy = g_cli_guard->temporary_buf_alloc(size + 1ULL, g_cli_command_line_owner);
    if (copy != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = copy, .src = s_notices.away_read, .bytes = size + 1ULL});
    }
    return copy;
}

// the bytes of `text`'s first line
EMBED_INLINE unsigned long long notices_first_line(const CliNoticesCtx *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES});
    return EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->text, .at = 0ULL,
                      .size = size, .set = "\n");
}

// `text`, the caller's and gone once it returns, written once as a notice in a movable cell of its own: its first line
// and a NUL, the lines after it and a NUL, then its type, its first line each run of digits written as '#'; 1, or 0
// where the cells cannot meet it
EMBED_INLINE int notices_built(const CliNoticesCtx *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES});
    const unsigned long long first = EMBED_CALL(notices_first_line, CliNoticesCtx, .text = args->text);
    const unsigned long long bytes = size + first + 2ULL;
    char *const laid = g_cli_guard->temporary_buf_alloc(bytes, g_cli_command_line_owner);
    const CarcerMovableEntry *const cell =
        (laid != NULL) ? g_cli_guard->movable_buf_alloc(bytes, g_cli_command_line_owner) : NULL;
    if (cell == NULL)
    {
        g_cli_guard->persistent_buf_release(laid, g_cli_command_line_owner);
        return 0;
    }
    // the newline that ends the first line becomes its NUL, the lines after it standing where they stood
    memor.cpy(&(MemoriaCfg){.dst = laid, .src = args->text, .bytes = size + 1ULL});
    laid[first] = '\0';
    const unsigned long long rest_at = (first < size) ? first + 1ULL : first;
    VerbaLinea type = {laid + size + 1ULL, first + 1ULL, 0ULL};
    for (unsigned long long at = 0ULL; at < first;)
    {
        const unsigned long long digit =
            EMBED_CALL(cli_text_at_size_classes.to_class, CliTextAtSizeClassesCfg, .text = args->text, .at = at,
                       .size = first, .classes = CLI_TEXT_DIGITS, .set = "");
        mmgr_line_put_n(&type, args->text + at, digit - at);
        at = EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = args->text, .at = digit,
                        .size = first, .classes = CLI_TEXT_DIGITS, .set = "");
        mmgr_line_put(&type, (digit < first) ? "#" : "");
    }
    (void)mmgr_line_finish(&type);
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = cell, .src = laid, .bytes = bytes});
    g_cli_guard->persistent_buf_release(laid, g_cli_command_line_owner);
    *args->notice = (CliNotice){cell, bytes, rest_at, size + 1ULL, args->failed, cli_clock_milliseconds()};
    return 1;
}

// the notice shown taken off the status bar, its cell standing in the list
EMBED_INLINE void notices_hidden(const CliNoticesCtx *args)
{
    (void)args;
    s_notices.shown = NULL;
    s_notices.shown_text[0] = '\0';
    s_notices.shown_type[0] = '\0';
    s_notices.shown_failed = 0;
}

// a notice's cell let go, the notice shown taken off the status bar first where it is that one
EMBED_INLINE void notices_released(const CliNoticesCtx *args)
{
    if (args->notice_read->cell == s_notices.shown)
    {
        EMBED_CALL(notices_hidden, CliNoticesCtx, .seen = 0);
    }
    (void)g_cli_guard->movable_buf_release(args->notice_read->cell, g_cli_command_line_owner);
}

// the notice put last in the list, the oldest let go where the list is full, counted unseen where it is not `seen`: 1,
// or 0 where no list could be taken and the notice is let go
EMBED_INLINE int notices_listed(const CliNoticesCtx *args)
{
    s_notices.list = EMBED_CALL(notices_ring_taken, CliNoticesCtx, .ring = s_notices.list);
    if (s_notices.list == NULL)
    {
        EMBED_CALL(notices_released, CliNoticesCtx, .notice_read = args->notice_read);
        return 0;
    }
    if (EMBED_CALL(notices_count, CliNoticesCtx, .ring = s_notices.list) == CLI_NOTICES_LISTED)
    {
        const CliNotice oldest = *EMBED_CALL(notices_at, CliNoticesCtx, .ring = s_notices.list, .at = 0u);
        EMBED_CALL(notices_released, CliNoticesCtx, .notice_read = &oldest);
        anularis.seg_release(&(AnularisCfg){.ring = &s_notices.list->ring});
    }
    EMBED_CALL(notices_put, CliNoticesCtx, .ring = s_notices.list, .notice_read = args->notice_read);
    s_notices.unseen += args->seen ? 0u : 1u;
    return 1;
}

// the notice put in the list and shown on the status bar, its first line and its type copied out of its cell, standing
// until its time is out
EMBED_INLINE void notices_shown(const CliNoticesCtx *args)
{
    const CliNotice notice = *args->notice_read;
    if (!EMBED_CALL(notices_listed, CliNoticesCtx, .notice_read = &notice, .seen = 0))
    {
        return;
    }
    char *const text = EMBED_CALL(notices_text_copy, CliNoticesCtx, .notice_read = &notice);
    if (text == NULL)
    {
        return;
    }
    VerbaLinea line = {s_notices.shown_text, sizeof(s_notices.shown_text), 0ULL};
    mmgr_line_put(&line, text);
    (void)mmgr_line_finish(&line);
    VerbaLinea type = {s_notices.shown_type, sizeof(s_notices.shown_type), 0ULL};
    mmgr_line_put(&type, text + notice.type_at);
    (void)mmgr_line_finish(&type);
    g_cli_guard->persistent_buf_release(text, g_cli_command_line_owner);
    s_notices.shown = notice.cell;
    s_notices.shown_failed = notice.failed;
    s_notices.shown_until_ms =
        cli_clock_milliseconds() + (notice.failed ? CLI_NOTICES_FAILED_MS : CLI_NOTICES_SHOWN_MS);
}

// whether the type `type` is put away
EMBED_INLINE int notices_away(const CliNoticesCtx *args)
{
    char *const away = notices_away_copy(args);
    const char *line = away;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->type, .cap = CLI_POOL_BYTES});
    int found = 0;
    while (!found && (line != NULL) && (*line != '\0'))
    {
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_POOL_BYTES});
        const unsigned long long end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                  .text = line, .at = 0ULL, .size = size, .set = "\n");
        found = (end == length) && (memor.cmp(&(MemoriaCfg){.src = line, .other = args->type, .bytes = length}) == 0);
        line += (end < size) ? (end + 1ULL) : end;
    }
    g_cli_guard->persistent_buf_release(away, g_cli_command_line_owner);
    return found;
}

// the types put away, the text `owned` written into a movable cell of the notices' own, and saved in orior's folder
EMBED_INLINE void notices_away_set(const CliNoticesCtx *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->owned, .cap = CLI_POOL_BYTES});
    const CarcerMovableEntry *const away = g_cli_guard->movable_buf_alloc(size + 1ULL, g_cli_command_line_owner);
    if (away == NULL)
    {
        return;
    }
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = away, .src = args->owned, .bytes = size + 1ULL});
    if (s_notices.away_owned)
    {
        (void)g_cli_guard->movable_buf_release(s_notices.away, g_cli_command_line_owner);
    }
    s_notices.away = away;
    s_notices.away_size = size;
    s_notices.away_read = NULL;
    s_notices.away_owned = 1;
    char folder[CLI_PATH_MAX_LEN];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder))
    {
        (void)cli_state_value_save(&(CliStateValueCfg){
            .folder = folder, .key = CLI_NOTICES_AWAY_KEY, .value = args->owned, .err_msg = &err_msg});
    }
}

// the notices waiting for a break taken: every one but the last into the list, the last shown; none while Focus is on
EMBED_INLINE void notices_break(const CliNoticesCtx *args)
{
    (void)args;
    const unsigned int count = EMBED_CALL(notices_count, CliNoticesCtx, .ring = s_notices.waiting);
    if (s_notices.focus || (count == 0u))
    {
        return;
    }
    for (unsigned int at = 0u; (at + 1u) < count; at += 1u)
    {
        const CliNotice waiting = *EMBED_CALL(notices_at, CliNoticesCtx, .ring = s_notices.waiting, .at = at);
        (void)EMBED_CALL(notices_listed, CliNoticesCtx, .notice_read = &waiting, .seen = 0);
    }
    const CliNotice last = *EMBED_CALL(notices_at, CliNoticesCtx, .ring = s_notices.waiting, .at = count - 1u);
    // the waiting ring emptied, each segment released
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        anularis.seg_release(&(AnularisCfg){.ring = &s_notices.waiting->ring});
    }
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
    char *const copy = EMBED_CALL(notices_text_copy, CliNoticesCtx, .notice_read = &notice);
    const int away = (copy != NULL) && EMBED_CALL(notices_away, CliNoticesCtx, .type = copy + notice.type_at);
    g_cli_guard->persistent_buf_release(copy, g_cli_command_line_owner);
    if (away)
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
    s_notices.waiting = EMBED_CALL(notices_ring_taken, CliNoticesCtx, .ring = s_notices.waiting);
    if (s_notices.waiting == NULL)
    {
        (void)EMBED_CALL(notices_listed, CliNoticesCtx, .notice_read = &notice, .seen = 0);
        return;
    }
    // the waiting full, the oldest of them goes to the list
    if (EMBED_CALL(notices_count, CliNoticesCtx, .ring = s_notices.waiting) == CLI_NOTICES_LISTED)
    {
        const CliNotice oldest = *EMBED_CALL(notices_at, CliNoticesCtx, .ring = s_notices.waiting, .at = 0u);
        (void)EMBED_CALL(notices_listed, CliNoticesCtx, .notice_read = &oldest, .seen = 0);
        anularis.seg_release(&(AnularisCfg){.ring = &s_notices.waiting->ring});
    }
    EMBED_CALL(notices_put, CliNoticesCtx, .ring = s_notices.waiting, .notice_read = &notice);
    if (s_notices.focus)
    {
        s_notices.focus_waiting = EMBED_CALL(notices_count, CliNoticesCtx, .ring = s_notices.waiting);
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
    const char *const type = (s_notices.shown != NULL) ? s_notices.shown_type : NULL;
    if ((part == CLI_NOTICES_PRESS_SILENCE) && (type != NULL) && !EMBED_CALL(notices_away, CliNoticesCtx, .type = type))
    {
        char *const away = EMBED_CALL(notices_away_copy, CliNoticesCtx, .seen = 0);
        const unsigned long long had =
            (away != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = away, .cap = CLI_POOL_BYTES}) : 0ULL;
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = type, .cap = CLI_POOL_BYTES});
        char *const joined = g_cli_guard->temporary_buf_alloc(had + length + 2ULL, g_cli_command_line_owner);
        if (joined != NULL)
        {
            VerbaLinea line = {joined, had + length + 2ULL, 0ULL};
            mmgr_line_put(&line, (away != NULL) ? away : "");
            mmgr_line_put(&line, (had != 0ULL) ? "\n" : "");
            mmgr_line_put(&line, type);
            (void)mmgr_line_finish(&line);
            EMBED_CALL(notices_away_set, CliNoticesCtx, .owned = joined);
        }
        g_cli_guard->persistent_buf_release(joined, g_cli_command_line_owner);
        g_cli_guard->persistent_buf_release(away, g_cli_command_line_owner);
    }
    if ((part == CLI_NOTICES_PRESS_NOTICE) || (part == CLI_NOTICES_PRESS_SILENCE))
    {
        EMBED_CALL(notices_hidden, CliNoticesCtx, .seen = 0);
    }
}

void cli_notices_focus(int on)
{
    s_notices.focus = on;
    s_notices.focus_waiting = on ? EMBED_CALL(notices_count, CliNoticesCtx, .ring = s_notices.waiting) : 0u;
    if (!on)
    {
        EMBED_CALL(notices_break, CliNoticesCtx, .seen = 0);
    }
}

char *cli_notices_away_alloc(void)
{
    return EMBED_CALL(notices_away_copy, CliNoticesCtx, .seen = 0);
}

void cli_notices_away_end(char *away)
{
    g_cli_guard->persistent_buf_release(away, g_cli_command_line_owner);
}

void cli_notices_show_again(const char *type)
{
    char *const away = EMBED_CALL(notices_away_copy, CliNoticesCtx, .seen = 0);
    if ((away == NULL) || !EMBED_CALL(notices_away, CliNoticesCtx, .type = type))
    {
        g_cli_guard->persistent_buf_release(away, g_cli_command_line_owner);
        return;
    }
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = away, .cap = CLI_POOL_BYTES});
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = type, .cap = CLI_POOL_BYTES});
    char *const left = g_cli_guard->temporary_buf_alloc(size + 1ULL, g_cli_command_line_owner);
    if (left == NULL)
    {
        g_cli_guard->persistent_buf_release(away, g_cli_command_line_owner);
        return;
    }
    VerbaLinea line = {left, size + 1ULL, 0ULL};
    for (unsigned long long at = 0ULL; at < size;)
    {
        const unsigned long long end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                  .text = away, .at = at, .size = size, .set = "\n");
        const int same =
            ((end - at) == length) && (memor.cmp(&(MemoriaCfg){.src = away + at, .other = type, .bytes = length}) == 0);
        if (!same)
        {
            mmgr_line_put(&line, (line.at != 0ULL) ? "\n" : "");
            mmgr_line_put_n(&line, away + at, end - at);
        }
        at = end + 1ULL;
    }
    (void)mmgr_line_finish(&line);
    EMBED_CALL(notices_away_set, CliNoticesCtx, .owned = left);
    g_cli_guard->persistent_buf_release(left, g_cli_command_line_owner);
    g_cli_guard->persistent_buf_release(away, g_cli_command_line_owner);
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
    if (!s_notices.focus && (EMBED_CALL(notices_count, CliNoticesCtx, .ring = s_notices.waiting) != 0u) &&
        ((now - s_notices.input_ms) >= CLI_NOTICES_REST_MS))
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
        (!s_notices.focus && (EMBED_CALL(notices_count, CliNoticesCtx, .ring = s_notices.waiting) != 0u))
            ? (s_notices.input_ms + CLI_NOTICES_REST_MS)
            : 0ULL;
    if ((shown == 0ULL) || ((rest != 0ULL) && (rest < shown)))
    {
        return rest;
    }
    return shown;
}

void cli_notices_restore(const CliStateEntries *entries)
{
    // the types put away read out of the entries' cell into a cell of this call, then into a cell of the notices' own
    const unsigned long long size = cli_state_entry_size(entries, CLI_NOTICES_AWAY_KEY);
    if (size == CLI_STATE_NONE)
    {
        return;
    }
    char *const away_text = g_cli_guard->temporary_buf_alloc(size + 1ULL, g_cli_command_line_owner);
    const CarcerMovableEntry *const away = g_cli_guard->movable_buf_alloc(size + 1ULL, g_cli_command_line_owner);
    if ((away_text == NULL) || (away == NULL) ||
        (cli_state_entry_value(entries, CLI_NOTICES_AWAY_KEY, away_text, size + 1ULL) == NULL))
    {
        g_cli_guard->persistent_buf_release(away_text, g_cli_command_line_owner);
        if (away != NULL)
        {
            (void)g_cli_guard->movable_buf_release(away, g_cli_command_line_owner);
        }
        return;
    }
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = away, .src = away_text, .bytes = size + 1ULL});
    g_cli_guard->persistent_buf_release(away_text, g_cli_command_line_owner);
    if (s_notices.away_owned)
    {
        (void)g_cli_guard->movable_buf_release(s_notices.away, g_cli_command_line_owner);
    }
    s_notices.away = away;
    s_notices.away_size = size;
    s_notices.away_read = NULL;
    s_notices.away_owned = 1;
}

void cli_notices_end(void)
{
    CliNoticesRing *const rings[2] = {s_notices.list, s_notices.waiting};
    for (unsigned int ring = 0u; ring < 2u; ring += 1u)
    {
        const unsigned int count = EMBED_CALL(notices_count, CliNoticesCtx, .ring = rings[ring]);
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            (void)g_cli_guard->movable_buf_release(
                EMBED_CALL(notices_at, CliNoticesCtx, .ring = rings[ring], .at = at)->cell, g_cli_command_line_owner);
        }
    }
    if (s_notices.away_owned)
    {
        (void)g_cli_guard->movable_buf_release(s_notices.away, g_cli_command_line_owner);
    }
    s_notices = (CliNotices){0};
}

const char *cli_notice_shown(void)
{
    return (s_notices.shown != NULL) ? s_notices.shown_text : NULL;
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

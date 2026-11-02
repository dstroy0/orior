// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_run.c: git started at a tree with the words trust gives, its errors read on a thread of their own and its input
// written on another while its output is read, so none of the three waits on a pipe the others fill; what it gave
// stored in a room of the run's own, a cell taken for it
#include "../command_line/cells.h"
#include "../machine/trust.h"
#include "git_internal.h"

#include "../text/text.h"
#include <stddef.h>

// the most words git is started with
#define CLI_GIT_WORDS 8192u
// A room a run of git takes: what it gave and the bytes it points into, the process, the threads that read its errors
// and write its input and the count of them ended, and the words it is started with; its output stands after it in
// its cell
typedef struct
{
    CliLock lock;
    CliWake wake;
    unsigned int ended;
    char err[CLI_GIT_ERR];
    CliSpawned spawned;
    CliThread reader;
    CliThread writer;
    const char *input;
    unsigned long long length;
    const char *words[CLI_GIT_WORDS];
    CliGitGave gave;
} CliGitRoom;

// the empty gave handed out where the cells cannot meet a run, which every run of git refuses
static char s_nothing[1];
static CliGitGave s_refused = {s_nothing, 0ull, 1, s_nothing, 0ull, 0};

void cli_git_text_begin(const CliGitTextCfg *args)
{
    args->text->bytes = g_cli_cells->temporary_buf_alloc(CLI_GIT_TEXT, &g_cli_git_cells);
    args->text->room = 0ull;
    args->text->used = 0ull;
    args->text->full = 0;
    if (args->text->bytes != NULL)
    {
        args->text->room = CLI_GIT_TEXT;
        args->text->used = 1ull;
        args->text->bytes[0] = '\0';
    }
}

unsigned int cli_git_text_put(const CliGitTextCfg *args)
{
    const unsigned long long most = args->text->used + args->length + 1ull;
    if ((most > CLI_GIT_TEXT) || (args->text->bytes == NULL))
    {
        args->text->full = 1;
        return 0u;
    }
    // a place is below CLI_GIT_TEXT, which an unsigned int stores
    const unsigned int at = (unsigned int)args->text->used;
    memor.cpy(&(MemoriaCfg){.dst = args->text->bytes + at, .src = args->from, .bytes = args->length});
    args->text->bytes[at + args->length] = '\0';
    args->text->used += args->length + 1ull;
    return at;
}

const char *cli_git_text_get(const CliGitTextAtCfg *args)
{
    return (args->text_read->bytes != NULL) ? (args->text_read->bytes + args->at) : "";
}

void cli_git_text_free(const CliGitTextCfg *args)
{
    g_cli_cells->persistent_buf_release(args->text->bytes, &g_cli_git_cells);
    args->text->bytes = NULL;
    args->text->used = 0ull;
    args->text->room = 0ull;
}

CliGitGave *cli_git_take(void)
{
    CliGitRoom *const room = g_cli_cells->persistent_buf_alloc(sizeof(*room) + CLI_GIT_OUT, &g_cli_git_cells);
    if (room == NULL)
    {
        return &s_refused;
    }
    char *const out = (char *)(void *)(room + 1);
    const CliLock lock = CLI_LOCK_INIT;
    const CliWake wake = CLI_WAKE_INIT;
    room->lock = lock;
    room->wake = wake;
    out[0] = '\0';
    room->err[0] = '\0';
    room->gave = (CliGitGave){out, 0ull, 1, room->err, 0ull, 0};
    return &room->gave;
}

typedef struct
{
    CliGitGave *gave;
    CliGitRoom *room;
} GitRunCtx;
// the room that contains `gave`, one cli_git_take took from the cells
EMBED_INLINE CliGitRoom *cli_room_get(const GitRunCtx *args)
{
    return (CliGitRoom *)(void *)((char *)args->gave - offsetof(CliGitRoom, gave));
}

void cli_git_free(const CliGitGaveCtx *args)
{
    if (args->gave != &s_refused)
    {
        g_cli_cells->persistent_buf_release(EMBED_CALL(cli_room_get, GitRunCtx, .gave = args->gave), &g_cli_git_cells);
    }
}

// a thread of a run ended, reported to the run
EMBED_INLINE void cli_end(const GitRunCtx *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &args->room->lock);
    args->room->ended += 1u;
    EMBED_CALL(cli_lock_internal.wake_all, CliLockCtx, .wake = &args->room->wake);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &args->room->lock);
}

// what git reports went wrong read until it ends, the bytes past the room read and let go
static void cli_errors_read(void *context)
{
    CliGitRoom *const room = (CliGitRoom *)context;
    CliGitGave *const gave = &room->gave;
    for (;;)
    {
        char spare[4096];
        const unsigned long long room_left = CLI_GIT_ERR - 1ull - gave->err_size;
        char *const into = (room_left != 0ull) ? (gave->err + gave->err_size) : spare;
        const unsigned long long got =
            EMBED_CALL(cli_room_pipe_handle_internal.spawn_read, CliRoomPipeHandleCtx, .pipe_handle = room->spawned.err,
                       .piece = into, .room = (room_left != 0ull) ? room_left : sizeof(spare));
        if (got == 0ull)
        {
            break;
        }
        gave->err_size += (room_left != 0ull) ? got : 0ull;
    }
    gave->err[gave->err_size] = '\0';
    EMBED_CALL(cli_end, GitRunCtx, .room = room);
}

// the run's input written whole and its pipe closed, which git reads as its input's end
static void cli_input_write(void *context)
{
    CliGitRoom *const room = (CliGitRoom *)context;
    EMBED_CALL(cli_pipe_handle_internal.spawn_write, CliPipeHandleCtx, .pipe_handle = room->spawned.in,
               .bytes = room->input, .size = room->length);
    EMBED_CALL(cli_spawned_internal.spawn_input_close, CliSpawnedCtx, .spawned = &room->spawned);
    EMBED_CALL(cli_end, GitRunCtx, .room = room);
}

int cli_git_gave(const CliGitGaveCtx *args)
{
    const char *input = args->input;
    unsigned long long length = args->length;
    if (args->gave == &s_refused)
    {
        mmgr_line_put(args->err_msg, "git: the cells cannot meet a run of git");
        return 0;
    }
    CliGitRoom *const room = EMBED_CALL(cli_room_get, GitRunCtx, .gave = args->gave);
    args->gave->out_size = 0ull;
    args->gave->out_whole = 1;
    args->gave->err_size = 0ull;
    args->gave->out[0] = '\0';
    args->gave->err[0] = '\0';
    args->gave->ok = 0;
    if ((args->count + 4u) > CLI_GIT_WORDS)
    {
        mmgr_line_put(args->err_msg, "git: too many words to start it with");
        return 0;
    }
    unsigned int words = 0u;
    room->words[words] = "git";
    words += 1u;
    words += EMBED_CALL(cli_trust_tree_words.git_words, CliTrustTreeWordsCfg, .tree = args->root,
                        .words = room->words + words);
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        room->words[words] = args->request[at];
        words += 1u;
    }
    room->words[words] = NULL;
    CliSetting settings[3];
    unsigned int set = 0u;
    if ((args->how & CLI_GIT_LOCKS_OFF) != 0u)
    {
        settings[set] = (CliSetting){"GIT_OPTIONAL_LOCKS", "0"};
        set += 1u;
    }
    if ((args->how & CLI_GIT_PROMPT_OFF) != 0u)
    {
        settings[set] = (CliSetting){"GIT_TERMINAL_PROMPT", "0"};
        set += 1u;
    }
    if (args->index != NULL)
    {
        settings[set] = (CliSetting){"GIT_INDEX_FILE", args->index};
        set += 1u;
    }
    const int started = (input != NULL)
                            ? EMBED_CALL(cli_words_err_msg_internal.spawn_feed, CliWordsErrMsgCtx,
                                         .spawned = &room->spawned, .words = room->words, .folder = args->root,
                                         .settings = settings, .count = set, .detached = 1, .err_msg = args->err_msg)
                            : EMBED_CALL(cli_words_err_msg_internal.spawn, CliWordsErrMsgCtx, .spawned = &room->spawned,
                                         .words = room->words, .folder = args->root, .settings = settings, .count = set,
                                         .detached = 1, .err_msg = args->err_msg);
    if (!started)
    {
        return 0;
    }
    room->ended = 0u;
    room->input = input;
    room->length = length;
    room->reader = (CliThread){cli_errors_read, room};
    room->writer = (CliThread){cli_input_write, room};
    // the reader and the writer each report their end, on a thread of their own or, where none starts, here
    const unsigned int ending = (input != NULL) ? 2u : 1u;
    const int reading = EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &room->reader);
    if ((input != NULL) && !EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &room->writer))
    {
        cli_input_write(room);
    }
    for (;;)
    {
        char spare[CLI_GIT_READ];
        const unsigned long long room_left = CLI_GIT_OUT - 1ull - args->gave->out_size;
        char *const into = (room_left != 0ull) ? (args->gave->out + args->gave->out_size) : spare;
        const unsigned long long got =
            EMBED_CALL(cli_room_pipe_handle_internal.spawn_read, CliRoomPipeHandleCtx, .pipe_handle = room->spawned.out,
                       .piece = into, .room = (room_left != 0ull) ? room_left : sizeof(spare));
        if (got == 0ull)
        {
            break;
        }
        args->gave->out_size += (room_left != 0ull) ? got : 0ull;
        args->gave->out_whole = args->gave->out_whole && (room_left != 0ull);
    }
    args->gave->out[args->gave->out_size] = '\0';
    if (!reading)
    {
        cli_errors_read(room);
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &room->lock);
    while (room->ended < ending)
    {
        EMBED_CALL(cli_lock_internal.wake_wait, CliLockCtx, .wake = &room->wake, .lock = &room->lock);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &room->lock);
    args->gave->ok = EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &room->spawned) == 0ll;
    EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &room->spawned);
    return 1;
}

int cli_git_query(const CliGitGaveCtx *args)
{
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    return EMBED_CALL(cli_git_gave_internal.gave, CliGitGaveCtx, .root = args->root, .request = args->request,
                      .count = args->count, .how = CLI_GIT_LOCKS_OFF, .index = NULL, .input = NULL, .length = 0ull,
                      .gave = args->gave, .err_msg = &err_msg) &&
           args->gave->ok && args->gave->out_whole;
}

void cli_git_trim(const CliGitFromCtx *args)
{
    const char **from = args->from;
    const char **to = args->to;
    *from += EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = *from, .at = 0ull,
                        .size = (unsigned long long)(*to - *from), .set = CLI_TEXT_WHITE);
    *to = *from + EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg, .text = *from,
                             .at = (unsigned long long)(*to - *from), .set = CLI_TEXT_WHITE);
}

int cli_git_on_run(const CliGitRootCtx *args)
{
    CliGitGave *const gave = cli_git_take();
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea started = {why, sizeof(why), 0ull};
    if (!EMBED_CALL(cli_git_gave_internal.gave, CliGitGaveCtx, .root = args->root, .request = args->request,
                    .count = args->count, .how = CLI_GIT_PROMPT_OFF, .index = args->index, .input = args->input,
                    .length = args->length, .gave = gave, .err_msg = &started))
    {
        mmgr_line_put(args->err_msg, "git: ");
        mmgr_line_put_n(args->err_msg, why, started.at);
        EMBED_CALL(cli_git_gave_internal.free, CliGitGaveCtx, .gave = gave);
        return 0;
    }
    // what it wrote and reported joined, in a cell of their size
    const unsigned long long joined_room = gave->out_size + gave->err_size + 1ull;
    char *const joined_cell = g_cli_cells->temporary_buf_alloc(joined_room, &g_cli_git_cells);
    VerbaLinea joined = mmgr_line_over(joined_cell, joined_room);
    if (joined.out == NULL)
    {
        mmgr_line_put(args->err_msg, "git: what it wrote and reported could not be joined: the cells cannot meet it");
        EMBED_CALL(cli_git_gave_internal.free, CliGitGaveCtx, .gave = gave);
        return 0;
    }
    mmgr_line_put_n(&joined, gave->out, gave->out_size);
    mmgr_line_put_n(&joined, gave->err, gave->err_size);
    const char *from = joined.out;
    const char *to = joined.out + joined.at;
    EMBED_CALL(cli_git_from_internal.trim, CliGitFromCtx, .from = &from, .to = &to);
    const int ok = gave->ok;
    if (ok)
    {
        mmgr_line_put_n(args->out, from, (unsigned long long)(to - from));
    }
    else if (from == to)
    {
        mmgr_line_put(args->err_msg, "git ");
        mmgr_line_put(args->err_msg, (args->count != 0u) ? args->request[0] : "");
        mmgr_line_put(args->err_msg, " failed");
    }
    else
    {
        mmgr_line_put_n(args->err_msg, from, (unsigned long long)(to - from));
    }
    g_cli_cells->persistent_buf_release(joined.out, &g_cli_git_cells);
    EMBED_CALL(cli_git_gave_internal.free, CliGitGaveCtx, .gave = gave);
    return ok;
}

int cli_git_run(const CliGitRootOutCountErrMsgCfg *args)
{
    return EMBED_CALL(cli_git_root_internal.run, CliGitRootCtx, .root = args->root, .request = args->request,
                      .count = args->count, .index = NULL, .input = NULL, .length = 0ull, .out = args->out,
                      .err_msg = args->err_msg);
}

int cli_git_is_id(const CliGitCtx *args)
{
    if (args->id[0] == '\0')
    {
        return 0;
    }
    if (EMBED_CALL(cli_text_at_size_classes.past_class, CliTextAtSizeClassesCfg, .text = args->id, .at = 0ull,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->id, .cap = CLI_CELLS_POOL}),
                   .classes = CLI_TEXT_DIGITS,
                   .set = "abcdefABCDEF") != cellul.len(&(CatenaFinitaCfg){.src = args->id, .cap = CLI_CELLS_POOL}))
    {
        return 0;
    }
    return 1;
}

int cli_git_beside(const CliGitRootErrMsgCtx *args)
{
    char full[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = args->root, .relative = args->file, .joined = full,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    if (!EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = full, .out = args->folder))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = args->folder,
                          .src = args->root,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_CELLS_POOL}) + 1u});
    }
    const unsigned long long root_length = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_CELLS_POOL});
    while (!EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = args->folder) &&
           EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->folder, .other_pointer = args->root,
                      .size = root_length) &&
           (!cellul.eq(&(CatenaFinitaCfg){.src = args->folder, .other = args->root, .cap = CLI_CELLS_POOL})))
    {
        char above[CLI_PATH_MAX_LEN];
        if (!EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = args->folder, .out = above))
        {
            memor.cpy(
                &(MemoriaCfg){.dst = above,
                              .src = args->root,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_CELLS_POOL}) + 1u});
        }
        memor.cpy(&(MemoriaCfg){.dst = args->folder,
                                .src = above,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = above, .cap = CLI_CELLS_POOL}) + 1u});
    }
    char relative[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = args->folder, .path = full,
               .relative_out = relative);
    VerbaLinea line = {args->shown, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, "./");
    mmgr_line_put(&line, relative);
    if (mmgr_line_finish(&line) == 0ull)
    {
        mmgr_line_put(args->err_msg, args->file);
        mmgr_line_put(args->err_msg, ": its path is too long");
        return 0;
    }
    return 1;
}

const char *cli_git_field(const CliGitAtCtx *args)
{
    const char **at = args->at;
    unsigned long long *length = args->length_pointer;
    if (*at > args->end)
    {
        return NULL;
    }
    const char *const start = *at;
    const char *place = start;
    while ((place < args->end) && (place[0] != args->stop))
    {
        place += 1;
    }
    *length = (unsigned long long)(place - start);
    *at = place + 1;
    return start;
}

int cli_git_seconds(const CliGitLengthTextCtx *args)
{
    long long *value = args->value;
    long long sign = 1ll;
    unsigned long long at = 0ull;
    if ((args->length > 0ull) && ((args->text[0] == '-') || (args->text[0] == '+')))
    {
        sign = (args->text[0] == '-') ? -1ll : 1ll;
        at = 1ull;
    }
    if (at == args->length)
    {
        return 0;
    }
    long long number = 0ll;
    for (; at < args->length; at += 1ull)
    {
        if ((args->text[at] < '0') || (args->text[at] > '9'))
        {
            return 0;
        }
        number = (number * 10ll) + (long long)(args->text[at] - '0');
    }
    *value = sign * number;
    return 1;
}

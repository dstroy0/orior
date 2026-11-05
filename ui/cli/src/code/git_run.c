// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_run.c: git started at a tree with the words trust gives, its errors read on a thread of their own and its input
// written on another while its output is read, so none of the three waits on a pipe the others fill; what it gave
// stored in a work state of the run's own, a cell taken for it
#include "../command_line/cells.h"
#include "../machine/trust.h"
#include "git_internal.h"

#include "../text/text.h"
#include <stddef.h>

// the most words git is started with
#define CLI_GIT_WORDS 8192u
// A work state a run of git takes: what it gave and the bytes it points into, the process, the threads that read its
// errors and write its input and the count of them ended, and the words it is started with; its output stands after it
// in its cell
typedef struct
{
    CliWake wake;
    unsigned int ended;
    char err[CLI_GIT_ERR];
    CliSpawned spawned;
    CliThread reader;
    CliThread writer;
    const char *input;
    unsigned long long length;
    const char *words[CLI_GIT_WORDS];
    CliGitResult result;
} CliGitWorkState;

// the empty gave handed out where the cells cannot meet a run, which every run of git refuses
static char s_nothing[1];
static CliGitResult s_refused = {s_nothing, 0ULL, 1, s_nothing, 0ULL, 0};

void cli_git_text_begin(const CliGitTextCfg *args)
{
    args->text->bytes = args->text->standing ? g_cli_cells->persistent_buf_alloc(CLI_GIT_TEXT, &g_cli_git_cells)
                                             : g_cli_cells->temporary_buf_alloc(CLI_GIT_TEXT, &g_cli_git_cells);
    args->text->capacity = 0ULL;
    args->text->used = 0ULL;
    args->text->full = 0;
    if (args->text->bytes != NULL)
    {
        args->text->capacity = CLI_GIT_TEXT;
        args->text->used = 1ULL;
        args->text->bytes[0] = '\0';
    }
}

unsigned int cli_git_text_append(const CliGitTextCfg *args)
{
    const unsigned long long most = args->text->used + args->length + 1ULL;
    if ((most > CLI_GIT_TEXT) || (args->text->bytes == NULL))
    {
        args->text->full = 1;
        return 0u;
    }
    // a place is below CLI_GIT_TEXT, which an unsigned int stores
    const unsigned int at = (unsigned int)args->text->used;
    memor.cpy(&(MemoriaCfg){.dst = args->text->bytes + at, .src = args->from, .bytes = args->length});
    args->text->bytes[at + args->length] = '\0';
    args->text->used += args->length + 1ULL;
    return at;
}

const char *cli_git_text_get(const CliGitTextAtCfg *args)
{
    return (args->text_read->bytes != NULL) ? (args->text_read->bytes + args->at) : "";
}

CliGitResult *cli_git_alloc(void)
{
    CliGitWorkState *const work_state =
        g_cli_cells->persistent_buf_alloc(sizeof(*work_state) + CLI_GIT_OUT, &g_cli_git_cells);
    if (work_state == NULL)
    {
        return &s_refused;
    }
    char *const out = (char *)(void *)(work_state + 1);
    const CliWake wake = CLI_WAKE_INIT;
    work_state->wake = wake;
    out[0] = '\0';
    work_state->err[0] = '\0';
    work_state->result = (CliGitResult){out, 0ULL, 1, work_state->err, 0ULL, 0};
    return &work_state->result;
}

typedef struct
{
    CliGitResult *result;
    CliGitWorkState *work_state;
} GitRunCtx;
// the work state that contains `result`, one cli_git_alloc took from the cells
EMBED_INLINE CliGitWorkState *cli_capacity_get(const GitRunCtx *args)
{
    return (CliGitWorkState *)(void *)((char *)args->result - offsetof(CliGitWorkState, result));
}

void cli_git_free(const CliGitResultCtx *args)
{
    if (args->result != &s_refused)
    {
        g_cli_cells->persistent_buf_release(EMBED_CALL(cli_capacity_get, GitRunCtx, .result = args->result),
                                            &g_cli_git_cells);
    }
}

// a thread of a run ended, reported to the run
EMBED_INLINE void cli_end(const GitRunCtx *args)
{
    args->work_state->ended += 1u;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &args->work_state->wake);
}

// what git reports went wrong read until it ends, the bytes past the work state read and let go
static void cli_errors_read(void *context)
{
    CliGitWorkState *const work_state = (CliGitWorkState *)context;
    CliGitResult *const result = &work_state->result;
    for (;;)
    {
        char reserve[4096];
        const unsigned long long capacity_remaining = CLI_GIT_ERR - 1ULL - result->err_size;
        char *const into = (capacity_remaining != 0ULL) ? (result->err + result->err_size) : reserve;
        const unsigned long long got =
            EMBED_CALL(cli_capacity_pipe_handle_internal.spawn_read, CliCapacityPipeHandleCtx,
                       .pipe_handle = work_state->spawned.err, .piece = into,
                       .capacity = (capacity_remaining != 0ULL) ? capacity_remaining : sizeof(reserve));
        if (got == 0ULL)
        {
            break;
        }
        result->err_size += (capacity_remaining != 0ULL) ? got : 0ULL;
    }
    result->err[result->err_size] = '\0';
    EMBED_CALL(cli_end, GitRunCtx, .work_state = work_state);
}

// the run's input written whole and its pipe closed, which git reads as its input's end
static void cli_input_write(void *context)
{
    CliGitWorkState *const work_state = (CliGitWorkState *)context;
    EMBED_CALL(cli_pipe_handle_internal.spawn_write, CliPipeHandleCtx, .pipe_handle = work_state->spawned.in,
               .bytes = work_state->input, .size = work_state->length);
    EMBED_CALL(cli_spawned_internal.spawn_input_close, CliSpawnedCtx, .spawned = &work_state->spawned);
    EMBED_CALL(cli_end, GitRunCtx, .work_state = work_state);
}

int cli_git_result(const CliGitResultCtx *args)
{
    const char *input = args->input;
    unsigned long long length = args->length;
    if (args->result == &s_refused)
    {
        mmgr_line_put(args->err_msg, "git: the cells cannot meet a run of git");
        return 0;
    }
    CliGitWorkState *const work_state = EMBED_CALL(cli_capacity_get, GitRunCtx, .result = args->result);
    args->result->out_size = 0ULL;
    args->result->out_untruncated = 1;
    args->result->err_size = 0ULL;
    args->result->out[0] = '\0';
    args->result->err[0] = '\0';
    args->result->ok = 0;
    if ((args->count + 4u) > CLI_GIT_WORDS)
    {
        mmgr_line_put(args->err_msg, "git: too many words to start it with");
        return 0;
    }
    unsigned int words = 0u;
    work_state->words[words] = "git";
    words += 1u;
    words += EMBED_CALL(cli_trust_tree_words.git_words, CliTrustTreeWordsCfg, .tree = args->root,
                        .words = work_state->words + words);
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        work_state->words[words] = args->request[at];
        words += 1u;
    }
    work_state->words[words] = NULL;
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
    const int started =
        (input != NULL) ? EMBED_CALL(cli_words_err_msg_internal.spawn_feed, CliWordsErrMsgCtx,
                                     .spawned = &work_state->spawned, .words = work_state->words, .folder = args->root,
                                     .settings = settings, .count = set, .detached = 1, .err_msg = args->err_msg)
                        : EMBED_CALL(cli_words_err_msg_internal.spawn, CliWordsErrMsgCtx,
                                     .spawned = &work_state->spawned, .words = work_state->words, .folder = args->root,
                                     .settings = settings, .count = set, .detached = 1, .err_msg = args->err_msg);
    if (!started)
    {
        return 0;
    }
    work_state->ended = 0u;
    work_state->input = input;
    work_state->length = length;
    work_state->reader = (CliThread){cli_errors_read, work_state};
    work_state->writer = (CliThread){cli_input_write, work_state};
    // the reader and the writer each report their end, on a thread of their own or, where none starts, here
    const unsigned int ending = (input != NULL) ? 2u : 1u;
    const int read_state = EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &work_state->reader);
    if ((input != NULL) && !EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &work_state->writer))
    {
        cli_input_write(work_state);
    }
    for (;;)
    {
        char reserve[CLI_GIT_READ];
        const unsigned long long capacity_remaining = CLI_GIT_OUT - 1ULL - args->result->out_size;
        char *const into = (capacity_remaining != 0ULL) ? (args->result->out + args->result->out_size) : reserve;
        const unsigned long long got =
            EMBED_CALL(cli_capacity_pipe_handle_internal.spawn_read, CliCapacityPipeHandleCtx,
                       .pipe_handle = work_state->spawned.out, .piece = into,
                       .capacity = (capacity_remaining != 0ULL) ? capacity_remaining : sizeof(reserve));
        if (got == 0ULL)
        {
            break;
        }
        args->result->out_size += (capacity_remaining != 0ULL) ? got : 0ULL;
        args->result->out_untruncated = args->result->out_untruncated && (capacity_remaining != 0ULL);
    }
    args->result->out[args->result->out_size] = '\0';
    if (!read_state)
    {
        cli_errors_read(work_state);
    }
    while (work_state->ended < ending)
    {
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &work_state->wake);
    }
    args->result->ok =
        EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &work_state->spawned) == 0LL;
    EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &work_state->spawned);
    return 1;
}

int cli_git_query(const CliGitResultCtx *args)
{
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    return EMBED_CALL(cli_git_result_internal.result, CliGitResultCtx, .root = args->root, .request = args->request,
                      .count = args->count, .how = CLI_GIT_LOCKS_OFF, .index = NULL, .input = NULL, .length = 0ULL,
                      .result = args->result, .err_msg = &err_msg) &&
           args->result->ok && args->result->out_untruncated;
}

void cli_git_trim(const CliGitFromCtx *args)
{
    const char **from = args->from;
    const char **to = args->to;
    *from += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = *from, .at = 0ULL,
                        .size = (unsigned long long)(*to - *from), .set = CLI_TEXT_WHITE);
    *to = *from + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = *from,
                             .at = (unsigned long long)(*to - *from), .set = CLI_TEXT_WHITE);
}

int cli_git_on_run(const CliGitRootCtx *args)
{
    CliGitResult *const result = cli_git_alloc();
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea started = {why, sizeof(why), 0ULL};
    if (!EMBED_CALL(cli_git_result_internal.result, CliGitResultCtx, .root = args->root, .request = args->request,
                    .count = args->count, .how = CLI_GIT_PROMPT_OFF, .index = args->index, .input = args->input,
                    .length = args->length, .result = result, .err_msg = &started))
    {
        mmgr_line_put(args->err_msg, "git: ");
        mmgr_line_put_n(args->err_msg, why, started.at);
        EMBED_CALL(cli_git_result_internal.free, CliGitResultCtx, .result = result);
        return 0;
    }
    // what it wrote and reported joined, in a cell of their size
    const unsigned long long joined_capacity = result->out_size + result->err_size + 1ULL;
    char *const joined_cell = g_cli_cells->temporary_buf_alloc(joined_capacity, &g_cli_git_cells);
    VerbaLinea joined = mmgr_line_over(joined_cell, joined_capacity);
    if (joined.out == NULL)
    {
        mmgr_line_put(args->err_msg, "git: what it wrote and reported could not be joined: the cells cannot meet it");
        EMBED_CALL(cli_git_result_internal.free, CliGitResultCtx, .result = result);
        return 0;
    }
    mmgr_line_put_n(&joined, result->out, result->out_size);
    mmgr_line_put_n(&joined, result->err, result->err_size);
    const char *from = joined.out;
    const char *to = joined.out + joined.at;
    EMBED_CALL(cli_git_from_internal.trim, CliGitFromCtx, .from = &from, .to = &to);
    const int ok = result->ok;
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
    EMBED_CALL(cli_git_result_internal.free, CliGitResultCtx, .result = result);
    return ok;
}

int cli_git_run(const CliGitRootOutCountErrMsgCfg *args)
{
    return EMBED_CALL(cli_git_root_internal.run, CliGitRootCtx, .root = args->root, .request = args->request,
                      .count = args->count, .index = NULL, .input = NULL, .length = 0ULL, .out = args->out,
                      .err_msg = args->err_msg);
}

int cli_git_is_id(const CliGitCtx *args)
{
    if (args->id[0] == '\0')
    {
        return 0;
    }
    if (EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = args->id, .at = 0ULL,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->id, .cap = CLI_CELLS_POOL}),
                   .classes = CLI_TEXT_DIGITS,
                   .set = "abcdefABCDEF") != cellul.len(&(CatenaFinitaCfg){.src = args->id, .cap = CLI_CELLS_POOL}))
    {
        return 0;
    }
    return 1;
}

int cli_git_adjacent(const CliGitRootErrMsgCtx *args)
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
    VerbaLinea line = {args->displayed, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, "./");
    mmgr_line_put(&line, relative);
    if (mmgr_line_finish(&line) == 0ULL)
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
    long long sign = 1LL;
    unsigned long long at = 0ULL;
    if ((args->length > 0ULL) && ((args->text[0] == '-') || (args->text[0] == '+')))
    {
        sign = (args->text[0] == '-') ? -1LL : 1LL;
        at = 1ULL;
    }
    if (at == args->length)
    {
        return 0;
    }
    long long number = 0LL;
    for (; at < args->length; at += 1ULL)
    {
        if ((args->text[at] < '0') || (args->text[at] > '9'))
        {
            return 0;
        }
        number = (number * 10LL) + (long long)(args->text[at] - '0');
    }
    *value = sign * number;
    return 1;
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_bring.c: files from outside the window copied into a folder of the tree on a thread of their own, each
// reporting how far along it is and the copy reporting where each went once done; the tree on another machine, the
// files read here and written there over the link a part at a time
#include "../command_line/callback_cfg.h"
#include "../comms/link.h"
#include "../editor/files.h"
#include "serve_internal.h"

// the most files one bring copies, the most bytes of their paths, and the bytes what it reports is written in
#define CLI_SERVE_BRINGS 4096u
#define CLI_SERVE_BRING_PATHS (1u << 20u)
#define CLI_SERVE_BRING_MORE (64ull << 10u)

// A bring of files on a thread of its own: the tree, whether it is on another machine over the link, the folder they go
// into, the files and the texts of their paths, one cell of their count and their bytes the bring gives back as it ends
typedef struct
{
    char root[CLI_PATH_MAX_LEN];
    int linked;
    char into[CLI_PATH_MAX_LEN];
    const char **from;
    unsigned int count;
    CliThread thread;
    int busy;
} CliServeBring;

static CliServeBring s_bring;
static CliLock s_bring_lock = CLI_LOCK_INIT;
static CliWake s_bring_wake = CLI_WAKE_INIT;

// how far a file is copied reported as bring-progress
static void bring_progress(const CliProgressCfg *args)
{
    unsigned long long size = args->size;
    char body[CLI_PATH_MAX_LEN + 128u];
    VerbaLinea line = {body, sizeof(body), 0ull};
    mmgr_line_put(&line, "{\"path\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &line,
               .text_bytes = (const unsigned char *)args->path,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&line, ",\"done\":");
    mmgr_line_decimal(&line, args->done, 1u);
    mmgr_line_put(&line, ",\"size\":");
    mmgr_line_decimal(&line, size, 1u);
    mmgr_line_ch(&line, '}');
    if (mmgr_line_ok(&line))
    {
        EMBED_CALL(cli_serve_event_internal.emit, CliServeEventCtx, .event = "bring-progress", .body = body,
                   .size = line.at);
    }
}

// a call of the tree's side on another machine, over the link
static int bring_linked_call(const CliCallCfg *args)
{
    const char *name = args->name;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    return EMBED_CALL(cli_link_size_out.call, CliLinkSizeOutCfg, .name = name, .request = args->arguments,
                      .size = cellul.len(&(CatenaFinitaCfg){.src = args->arguments, .cap = CLI_CELLS_POOL}), .out = out,
                      .err_msg = err_msg);
}

// the files of the bring copied, or read here and written over the link where the tree is on another machine, and
// where each went reported as bring-done once done
static void bring_run(void *context)
{
    CliServeBring *const bring = context;
    char why[1024];
    const unsigned long long brought_room =
        ((unsigned long long)bring->count * ((CLI_PATH_MAX_LEN * 6ull) + 16ull)) + CLI_SERVE_BRING_MORE;
    char *const brought_cell = g_cli_cells->temporary_buf_alloc(brought_room, &g_cli_serve_strings_cells);
    VerbaLinea brought = mmgr_line_over(brought_cell, brought_room);
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const int done =
        bring->linked
            ? EMBED_CALL(cli_files_context.bring_over, CliFilesContextCfg, .call = bring_linked_call,
                         .call_context = NULL, .into = bring->into, .from = bring->from, .count = bring->count,
                         .progress = bring_progress, .context = NULL, .out = &brought, .err_msg = &err_msg)
            : EMBED_CALL(cli_files_root.bring, CliFilesRootCfg, .root = bring->root, .into = bring->into,
                         .from = bring->from, .count = bring->count, .progress = bring_progress, .context = NULL,
                         .out = &brought, .err_msg = &err_msg);
    const unsigned long long line_room = brought.at + CLI_SERVE_BRING_MORE;
    char *const line_cell = g_cli_cells->temporary_buf_alloc(line_room, &g_cli_serve_strings_cells);
    VerbaLinea line = mmgr_line_over(line_cell, line_room);
    mmgr_line_put(&line, "{\"into\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &line,
               .text_bytes = (const unsigned char *)bring->into,
               .length = cellul.len(&(CatenaFinitaCfg){.src = bring->into, .cap = CLI_CELLS_POOL}));
    if (done)
    {
        mmgr_line_put(&line, ",\"brought\":");
        mmgr_line_put_n(&line, brought.out, mmgr_line_ok(&brought) ? brought.at : 0ull);
    }
    else
    {
        mmgr_line_put(&line, ",\"error\":");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &line,
                   .text_bytes = (const unsigned char *)why, .length = mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
    }
    mmgr_line_ch(&line, '}');
    EMBED_CALL(cli_serve_event_internal.emit, CliServeEventCtx, .event = "bring-done", .body = line.out,
               .size = mmgr_line_ok(&line) ? line.at : 0ull);
    g_cli_cells->persistent_buf_release(line.out, &g_cli_serve_strings_cells);
    g_cli_cells->persistent_buf_release(brought.out, &g_cli_serve_strings_cells);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_bring_lock);
    g_cli_cells->persistent_buf_release(bring->from, &g_cli_serve_strings_cells);
    bring->from = NULL;
    bring->count = 0u;
    bring->busy = 0;
    EMBED_CALL(cli_lock_internal.wake_all, CliLockCtx, .wake = &s_bring_wake);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_bring_lock);
}

typedef struct
{
    const CliServeArgs *request;
    int linked;
    VerbaLinea *out;
    VerbaLinea *err_msg;
} ServeCallBringCtx;
// the bring the call's arguments name started, into the tree open here or, where `linked`, into the tree on another
// machine the link is open to: null into `out`, 1, or 0 with why given
static int bring_start(const ServeCallBringCtx *args)
{
    const CliServeArgs *arguments = args->request;
    int linked = args->linked;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    root[0] = '\0';
    unsigned int count = 0u;
    const char **given = NULL;
    const char *const into = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "into", .err_msg = err_msg);
    if ((into == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_internal.strings_take, CliServeErrMsgCtx, .request = arguments, .key = "paths",
                    .most = CLI_SERVE_BRINGS, .items_pointer = &given, .count = &count, .err_msg = err_msg) ||
        (!linked && !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx,
                                .root = root, .err_msg = err_msg)))
    {
        EMBED_CALL(cli_serve_items_internal.strings_free, CliServeItemsCtx, .items = given);
        return 0;
    }
    // the paths retained past the call, CLI_SERVE_BRING_PATHS of them at the most, in a cell of their pointers and
    // bytes
    unsigned long long bytes = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        bytes += cellul.len(&(CatenaFinitaCfg){.src = given[at], .cap = CLI_CELLS_POOL}) + 1ull;
    }
    bytes = (bytes < CLI_SERVE_BRING_PATHS) ? bytes : CLI_SERVE_BRING_PATHS;
    const char **const from =
        g_cli_cells->persistent_buf_alloc(((count + 1ull) * sizeof(*from)) + bytes + 1ull, &g_cli_serve_strings_cells);
    if (from == NULL)
    {
        EMBED_CALL(cli_serve_items_internal.strings_free, CliServeItemsCtx, .items = given);
        mmgr_line_put(err_msg, "the files a bring copies could not be given a cell");
        return 0;
    }
    char *const paths = (char *)(void *)(from + count + 1u);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_bring_lock);
    while (s_bring.busy)
    {
        EMBED_CALL(cli_lock_internal.wake_wait, CliLockCtx, .wake = &s_bring_wake, .lock = &s_bring_lock);
    }
    s_bring.busy = 1;
    s_bring.linked = linked;
    memor.cpy(&(MemoriaCfg){.dst = s_bring.root, .src = root, .bytes = sizeof(root)});
    VerbaLinea line = {s_bring.into, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, into);
    (void)mmgr_line_finish(&line);
    unsigned long long used = 0ull;
    s_bring.from = from;
    s_bring.count = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const size_t size = cellul.len(&(CatenaFinitaCfg){.src = given[at], .cap = CLI_CELLS_POOL}) + 1u;
        if (used + size <= bytes)
        {
            memor.cpy(&(MemoriaCfg){.dst = paths + used, .src = given[at], .bytes = size});
            s_bring.from[s_bring.count] = paths + used;
            s_bring.count += 1u;
            used += size;
        }
    }
    EMBED_CALL(cli_serve_items_internal.strings_free, CliServeItemsCtx, .items = given);
    s_bring.thread = (CliThread){bring_run, &s_bring};
    const int started = EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_bring.thread);
    s_bring.busy = started;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_bring_lock);
    if (!started)
    {
        bring_run(&s_bring);
    }
    mmgr_line_put(args->out, "null");
    return 1;
}

int cli_serve_files_bring(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    return EMBED_CALL(bring_start, ServeCallBringCtx, .request = arguments, .linked = 0, .out = out,
                      .err_msg = err_msg);
}

int cli_serve_files_bring_over(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    return EMBED_CALL(bring_start, ServeCallBringCtx, .request = arguments, .linked = 1, .out = out,
                      .err_msg = err_msg);
}

int cli_serve_bring_name(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    const char *const into = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "into", .err_msg = err_msg);
    const char *const name = (into != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                         .request = arguments, .key = "name", .err_msg = err_msg)
                                            : NULL;
    if ((name == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg) ||
        !EMBED_CALL(cli_files_root_err_msg.free_in, CliFilesRootErrMsgCfg, .root = root, .into = into, .name = name,
                    .path_out = path, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)path,
               .length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    return 1;
}

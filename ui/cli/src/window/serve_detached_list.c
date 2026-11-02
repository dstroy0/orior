// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_detached_list.c: the detached runs of the tree open listed, the oldest first, each with its job, and a run
// retained a week let go
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../runs/runner.h"
#include "../text/text.h"
#include "serve_internal.h"

#include <stdlib.h>

// how long a detached run is retained after it starts, in milliseconds, and the most runs detached that are listed
#define CLI_SERVE_DETACHED_TTL (7ull * 24ull * 3600ull * 1000ull)
#define CLI_SERVE_DETACHED_RUNS 4096u

// The detached runs of the tree open as they are listed: the runs folder, the tree, the time now, and each run found
typedef struct
{
    const char *detached_dir;
    const char *root;
    unsigned long long now;
    unsigned long long run[CLI_SERVE_DETACHED_RUNS];
    unsigned int count;
} CliServeDetachedList;

// A listing's room, a cell taken with the listing's lock and given back with it: the runs found, and each run's job
// read into a tree in a cell of its own, sized to the job
typedef struct
{
    CliServeDetachedList list;
    CliJson json;
} CliServeDetachedListRoom;

static CliServeDetachedListRoom *s_list_room;
static CliLock s_list_lock = CLI_LOCK_INIT;

// an entry of the runs folder taken where it is a detached run's job file: let go where the run is older than a week,
// retained where the run is of the tree open
static void detached_entry(const CliEachNameIsFolderCfg *args)
{
    CliServeDetachedList *const list = args->context;
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL});
    if (args->is_folder || (length <= 4u) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = args->name + length - 4u, .other = ".job", .cap = CLI_CELLS_POOL})) ||
        (list->count == CLI_SERVE_DETACHED_RUNS))
    {
        return;
    }
    unsigned long long run = 0ull;
    for (size_t at = 0u; at < length - 4u; at += 1u)
    {
        if ((args->name[at] < '0') || (args->name[at] > '9') || (run > (~0ull - 9ull) / 10ull))
        {
            return;
        }
        run = (run * 10ull) + (unsigned long long)(args->name[at] - '0');
    }
    char number[32];
    VerbaLinea named = {number, sizeof(number), 0ull};
    mmgr_line_decimal(&named, run, 1u);
    (void)mmgr_line_finish(&named);
    char base[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = list->detached_dir, .relative = number, .joined = base);
    if ((list->now > run) && (list->now - run > CLI_SERVE_DETACHED_TTL))
    {
        static const char *const s_kinds[4] = {"job", "output", "pid", "stop"};
        for (unsigned int kind = 0u; kind < 4u; kind += 1u)
        {
            char path[CLI_PATH_MAX_LEN];
            EMBED_CALL(cli_serve_base_internal.detached_file, CliServeBaseCtx, .base = base, .kind = s_kinds[kind],
                       .path_out = path);
            (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = path);
        }
        return;
    }
    list->run[list->count] = run;
    list->count += 1u;
}

// two runs by their numbers
static int detached_order(const void *one, const void *other)
{
    const unsigned long long left = *(const unsigned long long *)one;
    const unsigned long long right = *(const unsigned long long *)other;
    return (left < right) ? -1 : (left > right) ? 1 : 0;
}

int cli_serve_runs_detach(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char detached_dir[CLI_PATH_MAX_LEN];
    char root[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_serve.lock);
    const int has_detached_dir = g_cli_serve.has_detached_dir && g_cli_serve.has_root;
    memor.cpy(&(MemoriaCfg){.dst = detached_dir, .src = g_cli_serve.detached_dir, .bytes = sizeof(detached_dir)});
    memor.cpy(&(MemoriaCfg){.dst = root, .src = g_cli_serve.root, .bytes = sizeof(root)});
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_serve.lock);
    if (!has_detached_dir)
    {
        mmgr_line_put(out, "[]");
        return 1;
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_list_lock);
    s_list_room = EMBED_CALL(cli_serve_size_err_msg_internal.cell, CliServeSizeErrMsgCtx, .size = sizeof(*s_list_room),
                             .what = "the detached runs' listing", .err_msg = err_msg);
    if (s_list_room == NULL)
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_list_lock);
        return 0;
    }
    mmgr_line_ch(out, '[');
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &s_list_room->json,
               .node_json_node_pointer = NULL, .nodes_most = 0u, .text_out = NULL, .text_most = 0ull);
    s_list_room->list.detached_dir = detached_dir;
    s_list_room->list.root = root;
    s_list_room->list.now = cli_wall_milliseconds();
    s_list_room->list.count = 0u;
    (void)EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = detached_dir,
                     .each = detached_entry, .context_bytess = &s_list_room->list);
    qsort(s_list_room->list.run, s_list_room->list.count, sizeof(s_list_room->list.run[0]), detached_order);
    int written = 0;
    for (unsigned int at = 0u; at < s_list_room->list.count; at += 1u)
    {
        char number[32];
        VerbaLinea named = {number, sizeof(number), 0ull};
        mmgr_line_decimal(&named, s_list_room->list.run[at], 1u);
        (void)mmgr_line_finish(&named);
        char base[CLI_PATH_MAX_LEN];
        char path[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = detached_dir, .relative = number, .joined = base);
        EMBED_CALL(cli_serve_base_internal.detached_file, CliServeBaseCtx, .base = base, .kind = "job",
                   .path_out = path);
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &s_list_room->json);
        char quiet[256];
        VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
        const unsigned int spec = EMBED_CALL(cli_json_tree_err_msg.json_file_take, CliJsonTreeErrMsgCfg,
                                             .json = &s_list_room->json, .path = path, .nodes_more = 0u,
                                             .text_more = 0ull, .err_msg = &ignored, .owner = &g_cli_window_cells);
        const char *const tree =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_list_room->json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = &s_list_room->json, .object = spec, .name = "root"),
                       .length_pointer = NULL);
        if ((tree == NULL) || (!cellul.eq(&(CatenaFinitaCfg){.src = tree, .other = root, .cap = CLI_CELLS_POOL})))
        {
            continue;
        }
        EMBED_CALL(cli_serve_base_internal.detached_file, CliServeBaseCtx, .base = base, .kind = "output",
                   .path_out = path);
        unsigned long long lines_size = 0ull;
        const unsigned char *const mapped = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = path,
                                                       .size_pointer = &lines_size, .owner = &g_cli_window_cells);
        unsigned long long end = lines_size;
        end = EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg, .text = (const char *)mapped,
                         .at = end, .set = "\n\r");
        unsigned long long start = end;
        start = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = (const char *)mapped,
                           .at = start, .set = "\n");
        const int ended = (mapped != NULL) && (end - start >= 6ull) &&
                          (memor.cmp(&(MemoriaCfg){.src = mapped + start, .other = "{\"end\"", .bytes = 6u}) == 0);
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = mapped,
                   .owner = &g_cli_window_cells);
        mmgr_line_put(out, written ? ",{\"run\":" : "{\"run\":");
        mmgr_line_decimal(out, s_list_room->list.run[at], 1u);
        mmgr_line_put(out, ",\"job\":");
        const unsigned int job = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                            .json_read = &s_list_room->json, .object = spec, .name = "job");
        if (job != CLI_JSON_NONE)
        {
            EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &s_list_room->json,
                       .node = job, .out = out, .indent = 0u, .sorted = 0);
        }
        else
        {
            mmgr_line_put(out, "null");
        }
        mmgr_line_put(out, ended ? ",\"ended\":true}" : ",\"ended\":false}");
        written = 1;
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &s_list_room->json);
    g_cli_cells->persistent_buf_release(s_list_room, &g_cli_window_cells);
    s_list_room = NULL;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_list_lock);
    mmgr_line_ch(out, ']');
    return 1;
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_detached_list.c: the detached runs of the tree open listed, the oldest first, each with its job, and a run
// retained a week let go
#include "../runs/runner.h"
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
static void detached_entry(void *context, const char *name, int is_folder)
{
    CliServeDetachedList *const list = context;
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    if (is_folder || (length <= 4u) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = name + length - 4u, .other = ".job", .cap = CLI_CELLS_POOL})) ||
        (list->count == CLI_SERVE_DETACHED_RUNS))
    {
        return;
    }
    unsigned long long run = 0ull;
    for (size_t at = 0u; at < length - 4u; at += 1u)
    {
        if ((name[at] < '0') || (name[at] > '9') || (run > (~0ull - 9ull) / 10ull))
        {
            return;
        }
        run = (run * 10ull) + (unsigned long long)(name[at] - '0');
    }
    char number[32];
    VerbaLinea named = {number, sizeof(number), 0ull};
    mmgr_line_decimal(&named, run, 1u);
    (void)mmgr_line_finish(&named);
    char base[CLI_PATH_MAX_LEN];
    cli_full(list->detached_dir, number, base);
    if ((list->now > run) && (list->now - run > CLI_SERVE_DETACHED_TTL))
    {
        static const char *const s_kinds[4] = {"job", "output", "pid", "stop"};
        for (unsigned int kind = 0u; kind < 4u; kind += 1u)
        {
            char path[CLI_PATH_MAX_LEN];
            cli_serve_detached_file(base, s_kinds[kind], path);
            (void)query_removed(path);
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

int cli_serve_runs_detached(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    char detached_dir[CLI_PATH_MAX_LEN];
    char root[CLI_PATH_MAX_LEN];
    cli_lock_take(&g_cli_serve.lock);
    const int has_detached_dir = g_cli_serve.has_detached_dir && g_cli_serve.has_root;
    memor.cpy(&(MemoriaCfg){.dst = detached_dir, .src = g_cli_serve.detached_dir, .bytes = sizeof(detached_dir)});
    memor.cpy(&(MemoriaCfg){.dst = root, .src = g_cli_serve.root, .bytes = sizeof(root)});
    cli_lock_give(&g_cli_serve.lock);
    if (!has_detached_dir)
    {
        mmgr_line_put(out, "[]");
        return 1;
    }
    cli_lock_take(&s_list_lock);
    s_list_room = cli_serve_cell(sizeof(*s_list_room), "the detached runs' listing", err_msg);
    if (s_list_room == NULL)
    {
        cli_lock_give(&s_list_lock);
        return 0;
    }
    mmgr_line_ch(out, '[');
    cli_json_begun(&s_list_room->json, NULL, 0u, NULL, 0ull);
    s_list_room->list.detached_dir = detached_dir;
    s_list_room->list.root = root;
    s_list_room->list.now = cli_wall_milliseconds();
    s_list_room->list.count = 0u;
    (void)query_folder_each(detached_dir, detached_entry, &s_list_room->list);
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
        cli_full(detached_dir, number, base);
        cli_serve_detached_file(base, "job", path);
        cli_json_given(&s_list_room->json);
        char quiet[256];
        VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
        const unsigned int spec = cli_json_file_taken(&s_list_room->json, path, 0u, 0ull, &ignored);
        const char *const tree =
            cli_json_string(&s_list_room->json, cli_json_member(&s_list_room->json, spec, "root"), NULL);
        if ((tree == NULL) || (!cellul.eq(&(CatenaFinitaCfg){.src = tree, .other = root, .cap = CLI_CELLS_POOL})))
        {
            continue;
        }
        cli_serve_detached_file(base, "output", path);
        unsigned long long lines_size = 0ull;
        const unsigned char *const mapped = cli_file_read(path, &lines_size);
        unsigned long long end = lines_size;
        while ((end > 0ull) && ((mapped[end - 1ull] == '\n') || (mapped[end - 1ull] == '\r')))
        {
            end -= 1ull;
        }
        unsigned long long start = end;
        while ((start > 0ull) && (mapped[start - 1ull] != '\n'))
        {
            start -= 1ull;
        }
        const int ended = (mapped != NULL) && (end - start >= 6ull) &&
                          (memor.cmp(&(MemoriaCfg){.src = mapped + start, .other = "{\"end\"", .bytes = 6u}) == 0);
        if (mapped != NULL)
        {
            cli_file_let_go(mapped, lines_size);
        }
        mmgr_line_put(out, written ? ",{\"run\":" : "{\"run\":");
        mmgr_line_decimal(out, s_list_room->list.run[at], 1u);
        mmgr_line_put(out, ",\"job\":");
        const unsigned int job = cli_json_member(&s_list_room->json, spec, "job");
        if (job != CLI_JSON_NONE)
        {
            cli_json_write(&s_list_room->json, job, out, 0u, 0);
        }
        else
        {
            mmgr_line_put(out, "null");
        }
        mmgr_line_put(out, ended ? ",\"ended\":true}" : ",\"ended\":false}");
        written = 1;
    }
    cli_json_given(&s_list_room->json);
    g_cli_cells->persistent_buf_release(s_list_room, NULL);
    s_list_room = NULL;
    cli_lock_give(&s_list_lock);
    mmgr_line_ch(out, ']');
    return 1;
}

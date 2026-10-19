// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_kept_list.c: the kept runs of the tree open listed, the oldest first, each with its job, and a run kept a week
// let go
#include "../runs/runner.h"
#include "serve_internal.h"

#include <stdlib.h>

// how long a kept run is kept after it starts, in milliseconds, and the most runs kept that are listed
#define CLI_SERVE_KEPT_FOR (7ull * 24ull * 3600ull * 1000ull)
#define CLI_SERVE_KEPT_RUNS 4096u

// The kept runs of the tree open as they are listed: the keeping folder, the tree, the time now, and each run found
typedef struct
{
    const char *keep;
    const char *root;
    unsigned long long now;
    unsigned long long run[CLI_SERVE_KEPT_RUNS];
    unsigned int count;
} CliServeKeptList;

// A listing's room, a cell taken with the listing's lock and given back with it: the runs found, and each run's job
// read into a tree in a cell of its own, sized to the job
typedef struct
{
    CliServeKeptList list;
    CliJson json;
} CliServeKeptListRoom;

static CliServeKeptListRoom *s_kept;
static CliLock s_list_lock = CLI_LOCK_INIT;

// an entry of the keeping folder taken where it is a kept run's job file: let go where the run is older than a week,
// kept where the run is of the tree open
static void kept_entry(void *context, const char *name, int is_folder)
{
    CliServeKeptList *const list = context;
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    if (is_folder || (length <= 4u) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = name + length - 4u, .other = ".job", .cap = CLI_CELLS_POOL})) ||
        (list->count == CLI_SERVE_KEPT_RUNS))
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
    CliLine named = {number, sizeof(number), 0ull};
    cli_line_decimal(&named, run, 1u);
    (void)cli_line_closed(&named);
    char base[CLI_PATH_LONGEST];
    cli_full(list->keep, number, base);
    if ((list->now > run) && (list->now - run > CLI_SERVE_KEPT_FOR))
    {
        static const char *const s_kinds[4] = {"job", "said", "pid", "stop"};
        for (unsigned int kind = 0u; kind < 4u; kind += 1u)
        {
            char path[CLI_PATH_LONGEST];
            cli_serve_kept_file(base, s_kinds[kind], path);
            (void)query_removed(path);
        }
        return;
    }
    list->run[list->count] = run;
    list->count += 1u;
}

// two runs by their numbers
static int kept_order(const void *one, const void *other)
{
    const unsigned long long left = *(const unsigned long long *)one;
    const unsigned long long right = *(const unsigned long long *)other;
    return (left < right) ? -1 : (left > right) ? 1 : 0;
}

int cli_serve_runs_kept(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    char keep[CLI_PATH_LONGEST];
    char root[CLI_PATH_LONGEST];
    cli_lock_take(&g_cli_serve.lock);
    const int keeping = g_cli_serve.has_keeping && g_cli_serve.has_root;
    memor.cpy(&(MemoriaCfg){.dst = keep, .src = g_cli_serve.keeping, .bytes = sizeof(keep)});
    memor.cpy(&(MemoriaCfg){.dst = root, .src = g_cli_serve.root, .bytes = sizeof(root)});
    cli_lock_give(&g_cli_serve.lock);
    if (!keeping)
    {
        cli_line_text(out, "[]");
        return 1;
    }
    cli_lock_take(&s_list_lock);
    s_kept = cli_serve_cell(sizeof(*s_kept), "the kept runs' listing", said);
    if (s_kept == NULL)
    {
        cli_lock_give(&s_list_lock);
        return 0;
    }
    cli_line_character(out, '[');
    cli_json_begun(&s_kept->json, NULL, 0u, NULL, 0ull);
    s_kept->list.keep = keep;
    s_kept->list.root = root;
    s_kept->list.now = cli_wall_milliseconds();
    s_kept->list.count = 0u;
    (void)query_folder_each(keep, kept_entry, &s_kept->list);
    qsort(s_kept->list.run, s_kept->list.count, sizeof(s_kept->list.run[0]), kept_order);
    int written = 0;
    for (unsigned int at = 0u; at < s_kept->list.count; at += 1u)
    {
        char number[32];
        CliLine named = {number, sizeof(number), 0ull};
        cli_line_decimal(&named, s_kept->list.run[at], 1u);
        (void)cli_line_closed(&named);
        char base[CLI_PATH_LONGEST];
        char path[CLI_PATH_LONGEST];
        cli_full(keep, number, base);
        cli_serve_kept_file(base, "job", path);
        cli_json_given(&s_kept->json);
        char quiet[256];
        CliLine ignored = {quiet, sizeof(quiet), 0ull};
        const unsigned int spec = cli_json_file_taken(&s_kept->json, path, 0u, 0ull, &ignored);
        const char *const tree = cli_json_string(&s_kept->json, cli_json_member(&s_kept->json, spec, "root"), NULL);
        if ((tree == NULL) || (!cellul.eq(&(CatenaFinitaCfg){.src = tree, .other = root, .cap = CLI_CELLS_POOL})))
        {
            continue;
        }
        cli_serve_kept_file(base, "said", path);
        unsigned long long said_size = 0ull;
        const unsigned char *const mapped = qry_member_file_mapped(path, &said_size);
        unsigned long long end = said_size;
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
            qry_member_file_unmapped(mapped, said_size);
        }
        cli_line_text(out, written ? ",{\"run\":" : "{\"run\":");
        cli_line_decimal(out, s_kept->list.run[at], 1u);
        cli_line_text(out, ",\"job\":");
        const unsigned int job = cli_json_member(&s_kept->json, spec, "job");
        if (job != CLI_JSON_NONE)
        {
            cli_json_write(&s_kept->json, job, out, 0u, 0);
        }
        else
        {
            cli_line_text(out, "null");
        }
        cli_line_text(out, ended ? ",\"ended\":true}" : ",\"ended\":false}");
        written = 1;
    }
    cli_json_given(&s_kept->json);
    cli_cell_given(s_kept);
    s_kept = NULL;
    cli_lock_give(&s_list_lock);
    cli_line_character(out, ']');
    return 1;
}

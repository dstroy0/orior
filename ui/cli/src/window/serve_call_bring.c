// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_bring.c: files from outside the window copied into a folder of the tree on a thread of their own, each
// reporting how far along it is and the copy reporting where each went once done; the tree on another machine, the
// files read here and written there over the link a part at a time
#include "../comms/link.h"
#include "../editor/files.h"
#include "serve_internal.h"

// the most files one bring copies, the most bytes of their paths, and the bytes what it reports is begun to be written
// in, doubling as it is
#define CLI_SERVE_BRINGS 4096u
#define CLI_SERVE_BRING_PATHS (1u << 20u)
#define CLI_SERVE_BRING_PROGRESS_INIT_CAP 4096u

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
static void bring_progress(void *context, const char *path, unsigned long long done, unsigned long long size)
{
    (void)context;
    char body[CLI_PATH_MAX_LEN + 128u];
    VerbaLinea line = {body, sizeof(body), 0ull};
    mmgr_line_put(&line, "{\"path\":");
    cli_json_quoted_lossy(&line, (const unsigned char *)path,
                          cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&line, ",\"done\":");
    mmgr_line_decimal(&line, done, 1u);
    mmgr_line_put(&line, ",\"size\":");
    mmgr_line_decimal(&line, size, 1u);
    mmgr_line_ch(&line, '}');
    if (mmgr_line_ok(&line))
    {
        cli_serve_emitted("bring-progress", body, line.at);
    }
}

// a call of the tree's side on another machine, over the link
static int bring_linked_call(void *context, const char *name, const char *arguments, VerbaLinea *out,
                             VerbaLinea *err_msg)
{
    (void)context;
    return cli_link_call(name, arguments, cellul.len(&(CatenaFinitaCfg){.src = arguments, .cap = CLI_CELLS_POOL}), out,
                         err_msg);
}

// the files of the bring copied, or read here and written over the link where the tree is on another machine, and
// where each went reported as bring-done once done
static void bring_run(void *context)
{
    CliServeBring *const bring = context;
    char why[1024];
    VerbaLinea brought;
    (void)cli_growing_line_made(&brought, CLI_SERVE_BRING_PROGRESS_INIT_CAP);
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const int done = bring->linked ? cli_files_bring_over(bring_linked_call, NULL, bring->into, bring->from,
                                                          bring->count, bring_progress, NULL, &brought, &err_msg)
                                   : cli_files_bring(bring->root, bring->into, bring->from, bring->count,
                                                     bring_progress, NULL, &brought, &err_msg);
    VerbaLinea line;
    (void)cli_growing_line_made(&line, brought.at + CLI_SERVE_BRING_PROGRESS_INIT_CAP);
    mmgr_line_put(&line, "{\"into\":");
    cli_json_quoted_lossy(&line, (const unsigned char *)bring->into,
                          cellul.len(&(CatenaFinitaCfg){.src = bring->into, .cap = CLI_CELLS_POOL}));
    if (done)
    {
        mmgr_line_put(&line, ",\"brought\":");
        mmgr_line_put_n(&line, brought.out, mmgr_line_ok(&brought) ? brought.at : 0ull);
    }
    else
    {
        mmgr_line_put(&line, ",\"error\":");
        cli_json_quoted_lossy(&line, (const unsigned char *)why, mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
    }
    mmgr_line_ch(&line, '}');
    cli_serve_emitted("bring-done", line.out, mmgr_line_ok(&line) ? line.at : 0ull);
    g_cli_cells->persistent_buf_release(line.out, NULL);
    g_cli_cells->persistent_buf_release(brought.out, NULL);
    cli_lock_take(&s_bring_lock);
    g_cli_cells->persistent_buf_release(bring->from, &g_cli_serve_strings_cells);
    bring->from = NULL;
    bring->count = 0u;
    bring->busy = 0;
    cli_wake_all(&s_bring_wake);
    cli_lock_give(&s_bring_lock);
}

// the bring the call's arguments name started, into the tree open here or, where `linked`, into the tree on another
// machine the link is open to: null into `out`, 1, or 0 with why given
static int bring_started(const CliServeArgs *args, int linked, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    root[0] = '\0';
    unsigned int count = 0u;
    const char **given = NULL;
    const char *const into = cli_serve_string(args, "into", err_msg);
    if ((into == NULL) || !cli_serve_strings_taken(args, "paths", CLI_SERVE_BRINGS, &given, &count, err_msg) ||
        (!linked && !cli_serve_tree(root, err_msg)))
    {
        cli_serve_strings_given(given);
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
        cli_serve_strings_given(given);
        mmgr_line_put(err_msg, "the files a bring copies could not be given a cell");
        return 0;
    }
    char *const paths = (char *)(void *)(from + count + 1u);
    cli_lock_take(&s_bring_lock);
    while (s_bring.busy)
    {
        cli_wake_wait(&s_bring_wake, &s_bring_lock);
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
    cli_serve_strings_given(given);
    s_bring.thread = (CliThread){bring_run, &s_bring};
    const int started = cli_thread_started(&s_bring.thread);
    s_bring.busy = started;
    cli_lock_give(&s_bring_lock);
    if (!started)
    {
        bring_run(&s_bring);
    }
    mmgr_line_put(out, "null");
    return 1;
}

int cli_serve_files_bring(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    return bring_started(args, 0, out, err_msg);
}

int cli_serve_files_bring_over(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    return bring_started(args, 1, out, err_msg);
}

int cli_serve_bring_name(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    const char *const into = cli_serve_string(args, "into", err_msg);
    const char *const name = (into != NULL) ? cli_serve_string(args, "name", err_msg) : NULL;
    if ((name == NULL) || !cli_serve_tree(root, err_msg) || !cli_files_free_in(root, into, name, path, err_msg))
    {
        return 0;
    }
    cli_json_quoted_lossy(out, (const unsigned char *)path,
                          cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    return 1;
}

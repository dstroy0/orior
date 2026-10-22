// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_bring.c: files from outside the window copied into a folder of the tree on a thread of their own, each
// telling how far along it is and the copy telling where each went once done
#include "../editor/files.h"
#include "serve_internal.h"

// the most files one bring copies, the most bytes of their paths, and the bytes what it tells is begun to be written
// in, doubling as it is
#define CLI_SERVE_BRINGS 4096u
#define CLI_SERVE_BRING_PATHS (1u << 20u)
#define CLI_SERVE_BRING_TOLD_FIRST 4096u

// A bring of files on a thread of its own: the tree, the folder they go into, the files and the texts of their paths,
// one cell of their count and their bytes the bring gives back as it ends
typedef struct
{
    char root[CLI_PATH_LONGEST];
    char into[CLI_PATH_LONGEST];
    const char **from;
    unsigned int count;
    CliThread thread;
    int busy;
} CliServeBring;

static CliServeBring s_bring;
static CliLock s_bring_lock = CLI_LOCK_INIT;
static CliWake s_bring_wake = CLI_WAKE_INIT;

// how far a file is copied told as bring-progress
static void bring_told(void *context, const char *path, unsigned long long done, unsigned long long size)
{
    (void)context;
    char body[CLI_PATH_LONGEST + 128u];
    CliLine line = {body, sizeof(body), 0ull};
    cli_line_text(&line, "{\"path\":");
    cli_json_quoted_lossy(&line, (const unsigned char *)path,
                          cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    cli_line_text(&line, ",\"done\":");
    cli_line_decimal(&line, done, 1u);
    cli_line_text(&line, ",\"size\":");
    cli_line_decimal(&line, size, 1u);
    cli_line_character(&line, '}');
    if (cli_line_fits(&line))
    {
        cli_serve_emitted("bring-progress", body, line.at);
    }
}

// the files of the bring copied, and where each went told as bring-done once done
static void bring_run(void *context)
{
    CliServeBring *const bring = context;
    char why[1024];
    CliLine brought;
    (void)cli_line_growing(&brought, CLI_SERVE_BRING_TOLD_FIRST, "where the files brought went");
    CliLine said = {why, sizeof(why), 0ull};
    const int done =
        cli_files_bring(bring->root, bring->into, bring->from, bring->count, bring_told, NULL, &brought, &said);
    CliLine line;
    (void)cli_line_growing(&line, brought.at + CLI_SERVE_BRING_TOLD_FIRST, "a bring told done");
    cli_line_text(&line, "{\"into\":");
    cli_json_quoted_lossy(&line, (const unsigned char *)bring->into,
                          cellul.len(&(CatenaFinitaCfg){.src = bring->into, .cap = CLI_CELLS_POOL}));
    if (done)
    {
        cli_line_text(&line, ",\"brought\":");
        cli_line_counted(&line, brought.out, cli_line_fits(&brought) ? brought.at : 0ull);
    }
    else
    {
        cli_line_text(&line, ",\"error\":");
        cli_json_quoted_lossy(&line, (const unsigned char *)why, cli_line_fits(&said) ? said.at : 0ull);
    }
    cli_line_character(&line, '}');
    cli_serve_emitted("bring-done", line.out, cli_line_fits(&line) ? line.at : 0ull);
    cli_cell_given(line.out);
    cli_cell_given(brought.out);
    cli_lock_take(&s_bring_lock);
    cli_cell_given_by(&g_cli_serve_strings_cells, bring->from);
    bring->from = NULL;
    bring->count = 0u;
    bring->busy = 0;
    cli_wake_all(&s_bring_wake);
    cli_lock_give(&s_bring_lock);
}

int cli_serve_files_bring(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    unsigned int count = 0u;
    const char **given = NULL;
    const char *const into = cli_serve_string(args, "into", said);
    if ((into == NULL) || !cli_serve_strings_taken(args, "paths", CLI_SERVE_BRINGS, &given, &count, said) ||
        !cli_serve_tree(root, said))
    {
        cli_serve_strings_given(given);
        return 0;
    }
    // the paths kept past the call, CLI_SERVE_BRING_PATHS of them at the most, in a cell of their pointers and bytes
    unsigned long long bytes = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        bytes += cellul.len(&(CatenaFinitaCfg){.src = given[at], .cap = CLI_CELLS_POOL}) + 1ull;
    }
    bytes = (bytes < CLI_SERVE_BRING_PATHS) ? bytes : CLI_SERVE_BRING_PATHS;
    const char **const from = cli_cell_taken_by(
        &g_cli_serve_strings_cells, ((count + 1ull) * sizeof(*from)) + bytes + 1ull, "the files a bring copies");
    if (from == NULL)
    {
        cli_serve_strings_given(given);
        cli_line_text(said, "the files a bring copies could not be given a cell");
        return 0;
    }
    char *const paths = (char *)(void *)(from + count + 1u);
    cli_lock_take(&s_bring_lock);
    while (s_bring.busy)
    {
        cli_wake_wait(&s_bring_wake, &s_bring_lock);
    }
    s_bring.busy = 1;
    memor.cpy(&(MemoriaCfg){.dst = s_bring.root, .src = root, .bytes = sizeof(root)});
    CliLine line = {s_bring.into, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, into);
    (void)cli_line_closed(&line);
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
    cli_line_text(out, "null");
    return 1;
}

int cli_serve_bring_name(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    const char *const into = cli_serve_string(args, "into", said);
    const char *const name = (into != NULL) ? cli_serve_string(args, "name", said) : NULL;
    if ((name == NULL) || !cli_serve_tree(root, said) || !cli_files_free_in(root, into, name, path, said))
    {
        return 0;
    }
    cli_json_quoted_lossy(out, (const unsigned char *)path,
                          cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    return 1;
}

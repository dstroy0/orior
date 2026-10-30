// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_programs.c: the command line's words that are programs of their own: `serve` answers a window on
// another machine over its input and output; `write-raised` copies a file orior_cli staged over the file the reader
// gave the rights to write; `detached-run` runs a job the server detaches, a process of its own; `deploy` previews,
// sends or rolls back a deployment the tree names; `libraries` reports what a program loads and from where; and `sea`
// builds a script into Node's single executable
#include "../deploy/deploy.h"
#include "../machine/raised.h"
#include "../programs/binary.h"
#include "../programs/executables.h"
#include "../programs/sea.h"
#include "../toolchains/toolchains.h"
#include "../window/serve.h"
#include "command_line_internal.h"

// What a program's libraries are read with, a cell taken as they are read: the report of what it loads, and the PATH
// orior's jobs have, whose folders they are looked for in
typedef struct
{
    CliBinaryReport report;
    char path[CLI_PATH_TEXT];
} CliProgramsReading;

static char s_err_buf[1u << 14u];
static char s_line[1u << 16u];

// a line printed written to the output
static void programs_out(void *context, const char *line, unsigned long long size)
{
    (void)context;
    cli_out_line(line, size);
}

// a line printed written to the errors
static void programs_err(void *context, const char *line, unsigned long long size)
{
    (void)context;
    cli_err_line(line, size);
}

// why given written to the errors, and `code`
static int programs_failed(const VerbaLinea *err_msg, int code)
{
    cli_err_line(err_msg->out, mmgr_line_ok(err_msg) ? err_msg->at : 0ull);
    return code;
}

// `orior_cli serve`: the window on another machine answered, then every run, shell and server still in the container
// ended with it, on the machine the window reached as on the window's own
static int programs_serve(const char *named)
{
    static char s_root[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ull};
    if ((named != NULL) && !cli_command_line_tree(named, s_root, &err_msg))
    {
        return programs_failed(&err_msg, CLI_NO_CODE);
    }
    cli_serve((named != NULL) ? s_root : NULL);
    cli_container_members_ended();
    return 0;
}

// `orior_cli deploy <name> <do> <changed> <gone>`, the tree the one --root names or the folder it runs in
static int programs_deploy(const char *named, const char *const *words, unsigned int count)
{
    const int act = (count > 2u) ? cli_deploy_act_of(words[2]) : -1;
    if (act < 0)
    {
        cli_err("deploy takes a deployment's name and what to do, preview, send or roll back, and then what to do of "
                "files changed there and of files gone from the tree");
        return CLI_WRONG;
    }
    static char s_root[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ull};
    if ((named != NULL) ? !cli_command_line_tree(named, s_root, &err_msg) : !cli_folder_current(s_root, sizeof(s_root)))
    {
        if (named == NULL)
        {
            mmgr_line_put(&err_msg, "orior_cli finds no folder to run in");
        }
        return programs_failed(&err_msg, CLI_NO_CODE);
    }
    const int replace =
        (count > 3u) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = words[3], .other = "replace them", .cap = CLI_CELLS_POOL}));
    const int remove =
        (count > 4u) && (cellul.eq(&(CatenaFinitaCfg){.src = words[4], .other = "delete them", .cap = CLI_CELLS_POOL}));
    if (!cli_deploy_run(s_root, words[1], act, replace, remove, programs_out, NULL, &err_msg))
    {
        return programs_failed(&err_msg, CLI_WRONG);
    }
    return 0;
}

// `orior_cli libraries <program>`: each library the program loads, where it is found and the file found, then what the
// report reports about the program
static int programs_libraries(const char *const *words, unsigned int count)
{
    static const char *const s_found[] = {"beside",      "system",     "redistributable", "debug_runtime",
                                          "system_path", "orior_path", "build_path",      "missing"};
    if (count < 2u)
    {
        cli_err("libraries takes the program to read");
        return CLI_WRONG;
    }
    CliProgramsReading *const reading = cli_lane_alloc(sizeof(*reading), &g_cli_command_line_cells);
    if (reading == NULL)
    {
        cli_err("the cells cannot meet what a program's libraries are read with");
        return CLI_WRONG;
    }
    cli_toolchains_run_path(reading->path, sizeof(reading->path));
#if defined(_WIN32)
    const char between = ';';
#else
    const char between = ':';
#endif
    // the PATH's folders in a cell of the most it names, one for each mark between them and one more
    unsigned int most = 1u;
    for (const char *at = reading->path; at[0] != '\0'; at += 1)
    {
        most += (at[0] == between) ? 1u : 0u;
    }
    char (*const folder)[CLI_PATH_MAX_LEN] =
        g_cli_cells->persistent_buf_alloc((unsigned long long)most * CLI_PATH_MAX_LEN, &g_cli_command_line_cells);
    unsigned int folders = 0u;
    for (const char *part = reading->path; (folder != NULL) && (part != NULL) && (folders < most);)
    {
        const char *const end = cellul.chr(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL, .byte = between});
        const size_t size =
            (end != NULL) ? (size_t)(end - part) : cellul.len(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL});
        if ((size > 0u) && (size < CLI_PATH_MAX_LEN))
        {
            memor.cpy(&(MemoriaCfg){.dst = folder[folders], .src = part, .bytes = size});
            folder[folders][size] = '\0';
            folders += 1u;
        }
        part = (end != NULL) ? end + 1 : NULL;
    }
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ull};
    const int read =
        cli_binary_read(words[1], (const char (*)[CLI_PATH_MAX_LEN])folder, folders, &reading->report, &err_msg);
    cli_cell_release(folder, &g_cli_command_line_cells);
    if (!read)
    {
        cli_cell_release(reading, &g_cli_command_line_cells);
        return programs_failed(&err_msg, CLI_WRONG);
    }
    const CliBinaryReport *const report = &reading->report;
    for (unsigned int at = 0u; at < report->libraries; at += 1u)
    {
        const CliLibrary *const one = &report->library[at];
        VerbaLinea line = {s_line, sizeof(s_line), 0ull};
        mmgr_line_put(&line, one->name);
        mmgr_line_put(&line, "  ");
        mmgr_line_put(&line, (one->found < (sizeof(s_found) / sizeof(s_found[0]))) ? s_found[one->found] : "");
        mmgr_line_put(&line, "  ");
        mmgr_line_put(&line, one->has_at ? one->at : "");
        cli_out_line(s_line, mmgr_line_ok(&line) ? line.at : 0ull);
    }
    const QuerySink sink = {programs_err, NULL};
    cli_binary_report_print(report, sink);
    cli_cell_release(reading, &g_cli_command_line_cells);
    return 0;
}

// `orior_cli sea <script> <program> [module]`: the script built into Node's single executable
static int programs_sea(const char *const *words, unsigned int count)
{
    if (count < 3u)
    {
        cli_err("sea takes the script and the program to make of it, and module after them for an ES module");
        return CLI_WRONG;
    }
    const int module =
        (count > 3u) && (cellul.eq(&(CatenaFinitaCfg){.src = words[3], .other = "module", .cap = CLI_CELLS_POOL}));
    char node[CLI_PATH_MAX_LEN];
    char made[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ull};
    const QuerySink sink = {programs_out, NULL};
    if (!cli_executables_program("node", "node", node, &err_msg) ||
        !cli_sea_build(node, words[1], words[2], module, sink, made, &err_msg))
    {
        return programs_failed(&err_msg, CLI_WRONG);
    }
    VerbaLinea line = {s_line, sizeof(s_line), 0ull};
    mmgr_line_put(&line, "made ");
    mmgr_line_put(&line, made);
    cli_out_line(s_line, mmgr_line_ok(&line) ? line.at : 0ull);
    return 0;
}

int cli_line_program(const char *first, const char *named, const char *const *words, unsigned int count, int *code)
{
    if (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "serve", .cap = CLI_CELLS_POOL}))
    {
        *code = programs_serve(named);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "write-raised", .cap = CLI_CELLS_POOL}))
    {
        if (count < 3u)
        {
            cli_err("write-raised takes the file staged and the file to write");
            *code = CLI_WRONG;
            return 1;
        }
        VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ull};
        *code = cli_raised_copy_over(words[1], words[2], &err_msg) ? 0 : programs_failed(&err_msg, 1);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "detached-run", .cap = CLI_CELLS_POOL}))
    {
        *code = (count > 1u) ? cli_serve_detached_run(words[1]) : CLI_WRONG;
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "deploy", .cap = CLI_CELLS_POOL}))
    {
        *code = programs_deploy(named, words, count);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "libraries", .cap = CLI_CELLS_POOL}))
    {
        *code = programs_libraries(words, count);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "sea", .cap = CLI_CELLS_POOL}))
    {
        *code = programs_sea(words, count);
        return 1;
    }
    return 0;
}

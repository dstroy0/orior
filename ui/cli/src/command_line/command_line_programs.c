// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_programs.c: the command line's words that are programs of their own: `serve` answers a window on
// another machine over its input and output; `write-raised` copies a file orior staged over the file the reader gave
// the rights to write; `keep-run` runs a job the server keeps, a process of its own; `deploy` previews, sends or rolls
// back a deployment the tree names; `libraries` says what a program loads and from where; and `sea` builds a script
// into Node's single executable
#include "../deploy/deploy.h"
#include "../machine/raised.h"
#include "../programs/binary.h"
#include "../programs/executables.h"
#include "../programs/sea.h"
#include "../toolchains/toolchains.h"
#include "../window/serve.h"
#include "command_line_internal.h"

// the most folders of the PATH orior's jobs have that a program's libraries are looked for in
#define CLI_PROGRAMS_PATH 512u

static char s_said[1u << 14u];
static char s_line[1u << 16u];

// a line said written to the output
static void programs_out(void *context, const char *line, unsigned long long size)
{
    (void)context;
    cli_out_line(line, size);
}

// a line said written to the errors
static void programs_err(void *context, const char *line, unsigned long long size)
{
    (void)context;
    cli_err_line(line, size);
}

// why said written to the errors, and `code`
static int programs_failed(const CliLine *said, int code)
{
    cli_err_line(said->out, cli_line_fits(said) ? said->at : 0ull);
    return code;
}

// `orior serve`: the window on another machine answered, then every run, shell and server still in the container
// ended with it, on the machine the window reached as on the window's own
static int programs_serve(const char *named)
{
    static char s_root[CLI_PATH_LONGEST];
    CliLine said = {s_said, sizeof(s_said), 0ull};
    if ((named != NULL) && !cli_command_line_tree(named, s_root, &said))
    {
        return programs_failed(&said, CLI_NO_CODE);
    }
    cli_serve((named != NULL) ? s_root : NULL);
    cli_container_members_ended();
    return 0;
}

// `orior deploy <name> <do> <changed> <gone>`, the tree the one --root names or the folder it runs in
static int programs_deploy(const char *named, const char *const *words, unsigned int count)
{
    const int act = (count > 2u) ? cli_deploy_act_of(words[2]) : -1;
    if (act < 0)
    {
        cli_err("deploy takes a deployment's name and what to do, preview, send or roll back, and then what to do of "
                "files changed there and of files gone from the tree");
        return CLI_WRONG;
    }
    static char s_root[CLI_PATH_LONGEST];
    CliLine said = {s_said, sizeof(s_said), 0ull};
    if ((named != NULL) ? !cli_command_line_tree(named, s_root, &said) : !cli_folder_current(s_root, sizeof(s_root)))
    {
        if (named == NULL)
        {
            cli_line_text(&said, "orior finds no folder to run in");
        }
        return programs_failed(&said, CLI_NO_CODE);
    }
    const int replace =
        (count > 3u) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = words[3], .other = "replace them", .cap = CLI_CELLS_POOL}));
    const int remove =
        (count > 4u) && (cellul.eq(&(CatenaFinitaCfg){.src = words[4], .other = "delete them", .cap = CLI_CELLS_POOL}));
    if (!cli_deploy_run(s_root, words[1], act, replace, remove, programs_out, NULL, &said))
    {
        return programs_failed(&said, CLI_WRONG);
    }
    return 0;
}

// `orior libraries <program>`: each library the program loads, where it is found and the file found, then what the
// report tells of the program
static int programs_libraries(const char *const *words, unsigned int count)
{
    static const char *const s_found[] = {"beside",      "system",     "redistributable", "debug_runtime",
                                          "system_path", "orior_path", "build_path",      "missing"};
    if (count < 2u)
    {
        cli_err("libraries takes the program to read");
        return CLI_WRONG;
    }
    static char s_path[CLI_PATH_TEXT];
    static char s_folders[CLI_PROGRAMS_PATH][CLI_PATH_LONGEST];
    cli_toolchains_run_path(s_path, sizeof(s_path));
#if defined(_WIN32)
    const char between = ';';
#else
    const char between = ':';
#endif
    unsigned int folders = 0u;
    for (const char *part = s_path; (part != NULL) && (folders < CLI_PROGRAMS_PATH);)
    {
        const char *const end = cellul.chr(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL, .byte = between});
        const size_t size =
            (end != NULL) ? (size_t)(end - part) : cellul.len(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL});
        if ((size > 0u) && (size < CLI_PATH_LONGEST))
        {
            memor.cpy(&(MemoriaCfg){.dst = s_folders[folders], .src = part, .bytes = size});
            s_folders[folders][size] = '\0';
            folders += 1u;
        }
        part = (end != NULL) ? end + 1 : NULL;
    }
    static CliBinaryReport s_report;
    CliLine said = {s_said, sizeof(s_said), 0ull};
    if (!cli_binary_read(words[1], (const char (*)[CLI_PATH_LONGEST])s_folders, folders, &s_report, &said))
    {
        return programs_failed(&said, CLI_WRONG);
    }
    for (unsigned int at = 0u; at < s_report.libraries; at += 1u)
    {
        const CliLibrary *const one = &s_report.library[at];
        CliLine line = {s_line, sizeof(s_line), 0ull};
        cli_line_text(&line, one->name);
        cli_line_text(&line, "  ");
        cli_line_text(&line, (one->found < (sizeof(s_found) / sizeof(s_found[0]))) ? s_found[one->found] : "");
        cli_line_text(&line, "  ");
        cli_line_text(&line, one->has_at ? one->at : "");
        cli_out_line(s_line, cli_line_fits(&line) ? line.at : 0ull);
    }
    const QuerySink say = {programs_err, NULL};
    cli_binary_report_said(&s_report, say);
    return 0;
}

// `orior sea <script> <program> [module]`: the script built into Node's single executable
static int programs_sea(const char *const *words, unsigned int count)
{
    if (count < 3u)
    {
        cli_err("sea takes the script and the program to make of it, and module after them for an ES module");
        return CLI_WRONG;
    }
    const int module =
        (count > 3u) && (cellul.eq(&(CatenaFinitaCfg){.src = words[3], .other = "module", .cap = CLI_CELLS_POOL}));
    char node[CLI_PATH_LONGEST];
    char made[CLI_PATH_LONGEST];
    CliLine said = {s_said, sizeof(s_said), 0ull};
    const QuerySink say = {programs_out, NULL};
    if (!cli_executables_program("node", "node", node, &said) ||
        !cli_sea_build(node, words[1], words[2], module, say, made, &said))
    {
        return programs_failed(&said, CLI_WRONG);
    }
    CliLine line = {s_line, sizeof(s_line), 0ull};
    cli_line_text(&line, "made ");
    cli_line_text(&line, made);
    cli_out_line(s_line, cli_line_fits(&line) ? line.at : 0ull);
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
        CliLine said = {s_said, sizeof(s_said), 0ull};
        *code = cli_raised_copy_over(words[1], words[2], &said) ? 0 : programs_failed(&said, 1);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "keep-run", .cap = CLI_CELLS_POOL}))
    {
        *code = (count > 1u) ? cli_serve_keep_run(words[1]) : CLI_WRONG;
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

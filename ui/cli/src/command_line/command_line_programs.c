// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_programs.c: the command line's words that are programs of their own: `write-raised` copies a file
// orior_cli staged over the file the reader gave the rights to write; `deploy` previews, sends or rolls back a
// deployment the tree names; `libraries` reports what a program loads and from where; and `sea` builds a script into
// Node's single executable
#include "../deploy/deploy.h"
#include "../machine/raised.h"
#include "../programs/binary.h"
#include "../programs/executables.h"
#include "../programs/sea.h"
#include "../text/text.h"
#include "../toolchains/toolchains.h"
#include "callback_cfg.h"
#include "command_line_internal.h"

// What a program's libraries are read with, a cell taken as they are read: the report of what it loads, and the PATH
// orior's jobs have, whose folders they are looked for in
typedef struct
{
    CliBinaryReport report;
    char path[CLI_PATH_TEXT];
} CliProgramsReadState;

static char s_err_buf[1u << 14u];
static char s_line[1u << 16u];

// a line printed written to the output
static void programs_out(const CliEachLineSizeCfg *args)
{
    unsigned long long size = args->size;
    EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = args->line,
               .size = size);
}

// a line printed written to the errors
static void programs_err(const CliEachLineSizeCfg *args)
{
    unsigned long long size = args->size;
    EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = args->line,
               .size = size);
}

typedef struct
{
    const VerbaLinea *err_msg;
    int code;
    const char *named;
    const char *const *words;
    unsigned int count;
} CommandLineProgramsCtx;
// why given written to the errors, and `code`
EMBED_INLINE int programs_fail(const CommandLineProgramsCtx *args)
{
    EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = args->err_msg->out,
               .size = mmgr_line_ok(args->err_msg) ? args->err_msg->at : 0ULL);
    return args->code;
}

// `orior_cli deploy <name> <do> <changed> <gone>`, the tree the one --root names or the folder it runs in
EMBED_INLINE int programs_deploy(const CommandLineProgramsCtx *args)
{
    const char *named = args->named;
    const int act =
        (args->count > 2u) ? EMBED_CALL(cli_deploy_table.act_get, CliDeployCfg, .word_text = args->words[2]) : -1;
    if (act < 0)
    {
        EMBED_CALL(
            cli_command_line_internal.err, CliCommandLineCtx,
            .text =
                "deploy takes a deployment's name and what to do, preview, send or roll back, and then what to do of "
                "files changed there and of files gone from the tree");
        return CLI_WRONG;
    }
    static char s_root[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ULL};
    if ((named != NULL) ? !EMBED_CALL(cli_command_line_named.tree, CliCommandLineNamedCfg, .named = named,
                                      .root = s_root, .err_msg = &err_msg)
                        : !EMBED_CALL(cli_path_capacity_internal.folder_current, CliPathCapacityCtx, .path_out = s_root,
                                      .capacity_bytes = sizeof(s_root)))
    {
        if (named == NULL)
        {
            mmgr_line_put(&err_msg, "orior_cli finds no folder to run in");
        }
        return EMBED_CALL(programs_fail, CommandLineProgramsCtx, .err_msg = &err_msg, .code = CLI_NO_CODE);
    }
    const int replace =
        (args->count > 3u) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = args->words[3], .other = "replace them", .cap = CLI_POOL_BYTES}));
    const int remove =
        (args->count > 4u) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = args->words[4], .other = "delete them", .cap = CLI_POOL_BYTES}));
    if (!EMBED_CALL(cli_deploy_root.run, CliDeployRootCfg, .root = s_root, .name = args->words[1], .act = act,
                    .replace = replace, .remove = remove, .print = programs_out, .context = NULL, .err_msg = &err_msg))
    {
        return EMBED_CALL(programs_fail, CommandLineProgramsCtx, .err_msg = &err_msg, .code = CLI_WRONG);
    }
    return 0;
}

// `orior_cli libraries <program>`: each library the program loads, where it is found and the file found, then what the
// report reports about the program
EMBED_INLINE int programs_libraries(const CommandLineProgramsCtx *args)
{
    static const char *const s_found[] = {"beside",      "system",     "redistributable", "debug_runtime",
                                          "system_path", "orior_path", "build_path",      "missing"};
    if (args->count < 2u)
    {
        EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx, .text = "libraries takes the program to read");
        return CLI_WRONG;
    }
    CliProgramsReadState *const read_state =
        g_cli_guard->temporary_buf_alloc(sizeof(*read_state), g_cli_command_line_owner);
    if (read_state == NULL)
    {
        EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx,
                   .text = "the cells cannot meet what a program's libraries are read with");
        return CLI_WRONG;
    }
    EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = read_state->path,
               .size = sizeof(read_state->path));
#if defined(_WIN32)
    const char between = ';';
#else
    const char between = ':';
#endif
    // the PATH's folders in a cell of the most it names, one for each mark between them and one more
    unsigned int most = 1u;
    most += (unsigned int)EMBED_CALL(
        cli_text_size_byte.count, CliTextSizeByteCfg, .text = read_state->path,
        .size = cellul.len(&(CatenaFinitaCfg){.src = read_state->path, .cap = CLI_POOL_BYTES}), .byte = between);
    char (*const folder)[CLI_PATH_MAX_LEN] =
        g_cli_guard->persistent_buf_alloc((unsigned long long)most * CLI_PATH_MAX_LEN, g_cli_command_line_owner);
    unsigned int folders = 0u;
    for (const char *part = read_state->path; (folder != NULL) && (part != NULL) && (folders < most);)
    {
        const char *const end = cellul.chr(&(CatenaFinitaCfg){.src = part, .cap = CLI_POOL_BYTES, .byte = between});
        const size_t size =
            (end != NULL) ? (size_t)(end - part) : cellul.len(&(CatenaFinitaCfg){.src = part, .cap = CLI_POOL_BYTES});
        if ((size > 0u) && (size < CLI_PATH_MAX_LEN))
        {
            memor.cpy(&(MemoriaCfg){.dst = folder[folders], .src = part, .bytes = size});
            folder[folders][size] = '\0';
            folders += 1u;
        }
        part = (end != NULL) ? end + 1 : NULL;
    }
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ULL};
    const int read = EMBED_CALL(cli_binary_path_orior_path.read, CliBinaryPathOriorPathCfg, .path = args->words[1],
                                .orior_path = (const char (*)[CLI_PATH_MAX_LEN])folder, .count = folders,
                                .report = &read_state->report, .err_msg = &err_msg);
    g_cli_guard->persistent_buf_release(folder, g_cli_command_line_owner);
    if (!read)
    {
        g_cli_guard->persistent_buf_release(read_state, g_cli_command_line_owner);
        return EMBED_CALL(programs_fail, CommandLineProgramsCtx, .err_msg = &err_msg, .code = CLI_WRONG);
    }
    const CliBinaryReport *const report = &read_state->report;
    for (unsigned int at = 0u; at < report->libraries; at += 1u)
    {
        const CliLibrary *const one = &report->library[at];
        VerbaLinea line = {s_line, sizeof(s_line), 0ULL};
        mmgr_line_put(&line, one->name);
        mmgr_line_put(&line, "  ");
        mmgr_line_put(&line, (one->found < (sizeof(s_found) / sizeof(s_found[0]))) ? s_found[one->found] : "");
        mmgr_line_put(&line, "  ");
        mmgr_line_put(&line, one->has_at ? one->at : "");
        EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = s_line,
                   .size = mmgr_line_ok(&line) ? line.at : 0ULL);
    }
    const QuerySink sink = {programs_err, NULL};
    EMBED_CALL(cli_binary_report.report_print, CliBinaryReportCfg, .report_read = report, .sink = sink);
    g_cli_guard->persistent_buf_release(read_state, g_cli_command_line_owner);
    return 0;
}

// `orior_cli sea <script> <program> [module]`: the script built into Node's single executable
EMBED_INLINE int programs_sea(const CommandLineProgramsCtx *args)
{
    if (args->count < 3u)
    {
        EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx,
                   .text =
                       "sea takes the script and the program to make of it, and module after them for an ES module");
        return CLI_WRONG;
    }
    const int module = (args->count > 3u) &&
                       (cellul.eq(&(CatenaFinitaCfg){.src = args->words[3], .other = "module", .cap = CLI_POOL_BYTES}));
    char node[CLI_PATH_MAX_LEN];
    char made[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ULL};
    const QuerySink sink = {programs_out, NULL};
    if (!EMBED_CALL(cli_executables_tool.program, CliExecutablesToolCfg, .tool = "node", .name = "node", .path = node,
                    .err_msg = &err_msg) ||
        !EMBED_CALL(cli_sea_node.build, CliSeaNodeCfg, .node = node, .main = args->words[1], .output = args->words[2],
                    .module = module, .sink = sink, .made = made, .err_msg = &err_msg))
    {
        return EMBED_CALL(programs_fail, CommandLineProgramsCtx, .err_msg = &err_msg, .code = CLI_WRONG);
    }
    VerbaLinea line = {s_line, sizeof(s_line), 0ULL};
    mmgr_line_put(&line, "made ");
    mmgr_line_put(&line, made);
    EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = s_line,
               .size = mmgr_line_ok(&line) ? line.at : 0ULL);
    return 0;
}

int cli_line_program(const CliCommandLineWordsCtx *args)
{
    int *code = args->code;
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->first, .other = "write-raised", .cap = CLI_POOL_BYTES}))
    {
        if (args->count < 3u)
        {
            EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx,
                       .text = "write-raised takes the file staged and the file to write");
            *code = CLI_WRONG;
            return 1;
        }
        VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ULL};
        *code = EMBED_CALL(cli_raised_from.copy_over, CliRaisedFromCfg, .from = args->words[1], .to = args->words[2],
                           .err_msg = &err_msg)
                    ? 0
                    : EMBED_CALL(programs_fail, CommandLineProgramsCtx, .err_msg = &err_msg, .code = 1);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->first, .other = "deploy", .cap = CLI_POOL_BYTES}))
    {
        *code = EMBED_CALL(programs_deploy, CommandLineProgramsCtx, .named = args->named, .words = args->words,
                           .count = args->count);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->first, .other = "libraries", .cap = CLI_POOL_BYTES}))
    {
        *code = EMBED_CALL(programs_libraries, CommandLineProgramsCtx, .words = args->words, .count = args->count);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->first, .other = "sea", .cap = CLI_POOL_BYTES}))
    {
        *code = EMBED_CALL(programs_sea, CommandLineProgramsCtx, .words = args->words, .count = args->count);
        return 1;
    }
    return 0;
}

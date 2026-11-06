// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_jobs.c: the query's programs found through the toolchains, every command of the query a job of the protocol
// group whose one step is orior_cli itself with the command's words, and `orior_cli protocol` run here
#include "../../../query/src/query_loops.h"
#include "../command_line/catalog.h"
#include "../programs/executables.h"
#include "../toolchains/toolchains.h"
#include "protocol.h"

// the PATH every program the query starts runs with
static char s_run_path[1u << 16u];

typedef struct
{
    const char *tool;
    const char *program;
    char *path;
    CliCatalog *catalog;
    const QueryWord *word_text;
} QueryJobsCtx;
// `program` in the folder of the toolchain `tool`'s chosen program, where one is chosen, into `path`: 1, or 0 where
// none is
EMBED_INLINE int jobs_choose(const QueryJobsCtx *args)
{
    char *path = args->path;
    char chosen[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    const char *const names[1] = {args->program};
    return EMBED_CALL(cli_toolchains_id_path.chosen_program, CliToolchainsIdPathCfg, .id = args->tool,
                      .path = chosen) &&
           EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = chosen, .out = folder) &&
           EMBED_CALL(cli_toolchains_dir.program_in, CliToolchainsDirCfg, .dir = folder, .names = names, .count = 1u,
                      .path = path);
}

// the program of `role` as the toolchains give it, else as the PATH does, into `path`, sized for `capacity`: 1, or 0
// with why it is not found given
static int jobs_program(const QueryEachRolePathCapacityErrMsgCfg *args)
{
    QueryRole role = args->role;
    char *path = args->path;
    VerbaLinea *err_msg = args->err_msg;
    char found[CLI_PATH_MAX_LEN];
    int chosen = 0;
    switch (role)
    {
    case QUERY_ROLE_C:
        chosen = EMBED_CALL(jobs_choose, QueryJobsCtx, .tool = "gcc", .program = "gcc", .path = found) ||
                 EMBED_CALL(jobs_choose, QueryJobsCtx, .tool = "llvm", .program = "clang", .path = found);
        break;
    case QUERY_ROLE_CXX:
        chosen = EMBED_CALL(jobs_choose, QueryJobsCtx, .tool = "gcc", .program = "g++", .path = found) ||
                 EMBED_CALL(jobs_choose, QueryJobsCtx, .tool = "llvm", .program = "clang++", .path = found);
        break;
    case QUERY_ROLE_NVCC:
        chosen = EMBED_CALL(jobs_choose, QueryJobsCtx, .tool = "cuda", .program = "nvcc", .path = found);
        break;
    case QUERY_ROLE_NVDISASM:
        chosen = EMBED_CALL(jobs_choose, QueryJobsCtx, .tool = "cuda", .program = "nvdisasm", .path = found);
        break;
    case QUERY_ROLE_CUOBJDUMP:
        chosen = EMBED_CALL(jobs_choose, QueryJobsCtx, .tool = "cuda", .program = "cuobjdump", .path = found);
        break;
    case QUERY_ROLE_CL:
        chosen = EMBED_CALL(jobs_choose, QueryJobsCtx, .tool = "msvc", .program = "cl", .path = found);
        break;
    case QUERY_ROLE_PYTHON:
        chosen = EMBED_CALL(jobs_choose, QueryJobsCtx, .tool = "python", .program = "python3", .path = found) ||
                 EMBED_CALL(jobs_choose, QueryJobsCtx, .tool = "python", .program = "python", .path = found);
        break;
    default:
        break;
    }
    if (chosen && (cellul.len(&(CatenaFinitaCfg){.src = found, .cap = CLI_POOL_BYTES}) < args->capacity))
    {
        memor.cpy(&(MemoriaCfg){.dst = path,
                                .src = found,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = found, .cap = CLI_POOL_BYTES}) + 1u});
        return 1;
    }
    const QueryPrograms on_path = query_on_path();
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea none = {why, sizeof(why), 0ULL};
    if (EMBED_CALL(on_path.program, QueryEachRolePathCapacityErrMsgCfg, .context = on_path.context, .role = role,
                   .path = path, .capacity = args->capacity, .err_msg = &none))
    {
        return 1;
    }
    static const char *const tools[QUERY_ROLE_COUNT] = {"gcc",    "gcc",  "cuda",   "cuda",  "cuda", "msvc",
                                                        "python", "cuda", "docker", "shell", "shell"};
    char program[CLI_PATH_MAX_LEN];
    const unsigned long long from = err_msg->at;
    if (EMBED_CALL(cli_executables_tool.program, CliExecutablesToolCfg, .tool = tools[role],
                   .name = EMBED_CALL(query_table.role_name, QueryCfg, .role = role), .path = program,
                   .err_msg = err_msg))
    {
        err_msg->at = from;
        mmgr_line_put(err_msg, EMBED_CALL(query_table.role_name, QueryCfg, .role = role));
        mmgr_line_put(err_msg, " is not found: File, Toolchains opens its install page or takes its folder");
    }
    return 0;
}

// the PATH the toolchains give every program they start
static const char *jobs_run_path(void *context)
{
    (void)context;
    EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = s_run_path,
               .size = sizeof(s_run_path));
    return s_run_path;
}

QueryPrograms cli_protocol_programs(void)
{
    return (QueryPrograms){jobs_program, jobs_run_path, NULL};
}

// a command's word put on the last step as the argument that makes it
EMBED_INLINE void jobs_word(const QueryJobsCtx *args)
{
    CliCatalog *catalog = args->catalog;
    const QueryWord *word_text = args->word_text;
    switch (word_text->kind)
    {
    case QUERY_WORD_LITERAL:
        EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_LIT,
                   .a = word_text->first, .b = NULL, .c = NULL, .d = NULL, .separated = 0);
        break;
    case QUERY_WORD_VALUE:
        EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_VALUE,
                   .a = word_text->first, .b = NULL, .c = NULL, .d = NULL, .separated = 0);
        break;
    case QUERY_WORD_FLAG:
        EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_FLAG,
                   .a = word_text->first, .b = word_text->second, .c = NULL, .d = NULL, .separated = 0);
        break;
    case QUERY_WORD_EACH:
        EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_EACH,
                   .a = word_text->first, .b = word_text->second, .c = NULL, .d = NULL, .separated = 0);
        break;
    case QUERY_WORD_WORDS:
        EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_WORDS,
                   .a = word_text->first, .b = NULL, .c = NULL, .d = NULL, .separated = 0);
        break;
    default:
        EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_WHEN,
                   .a = word_text->first, .b = word_text->second, .c = NULL, .d = NULL, .separated = 0);
        EMBED_CALL(cli_catalog_word_table.word_text, CliCatalogWordCfg, .catalog = catalog,
                   .word_text = word_text->third);
        break;
    }
}

void cli_query_jobs(const CliCatalogRootCfg *args)
{
    const QueryCommand *commands = NULL;
    const unsigned int count = EMBED_CALL(query_table.commands, QueryCfg, .root = args->root, .commands = &commands);
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        const QueryCommand *const command = &commands[index];
        char id[CLI_NAME_MAX_LEN];
        VerbaLinea id_line = {id, sizeof(id), 0ULL};
        mmgr_line_put(&id_line, "protocol/");
        mmgr_line_put(&id_line, command->id);
        if ((mmgr_line_finish(&id_line) == 0ULL) ||
            (EMBED_CALL(cli_catalog_id_group.job, CliCatalogIdGroupCfg, .catalog = args->catalog, .id = id,
                        .group = "protocol", .title = command->title, .file = QUERY_TABLE, .about = command->about,
                        .opens = "") == CLI_CATALOG_NONE))
        {
            continue;
        }
        for (unsigned int at = 0u; at < command->param_count; at += 1u)
        {
            const QueryParam *const param = &command->params[at];
            EMBED_CALL(cli_catalog_key_kind.param, CliCatalogKeyKindCfg, .catalog = args->catalog, .key = param->key,
                       .kind = param->kind, .required = param->required);
            for (unsigned int choice = 0u; choice < param->choice_count; choice += 1u)
            {
                EMBED_CALL(cli_catalog_choice_table.choice, CliCatalogChoiceCfg, .catalog = args->catalog,
                           .choice = param->choices[choice]);
            }
            // a command's param has no default: the window prompts for one where it is required
            if (args->catalog->params > 0u)
            {
                args->catalog->param[args->catalog->params - 1u].fallback = CLI_CATALOG_NONE;
            }
        }
        EMBED_CALL(cli_catalog_text_program.step, CliCatalogTextProgramCfg, .catalog = args->catalog,
                   .program = CLI_PROGRAM_ORIOR, .text = NULL, .tool_program = NULL, .system = 0);
        EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = args->catalog, .kind_u32 = CLI_ARG_LIT,
                   .a = "protocol", .b = NULL, .c = NULL, .d = NULL, .separated = 0);
        for (unsigned int at = 0u; at < command->word_count; at += 1u)
        {
            EMBED_CALL(jobs_word, QueryJobsCtx, .catalog = args->catalog, .word_text = &command->words[at]);
        }
    }
}

int cli_protocol_run(const CliProtocolCfg *args)
{
    EMBED_CALL(cli_toolchains_root.set_tree, CliToolchainsRootCfg, .root = args->root);
    const QueryPrograms programs = cli_protocol_programs();
    return EMBED_CALL(query_table.run, QueryCfg, .root = args->root, .programs = &programs, .words = args->words,
                      .count = args->count, .sink = args->sink);
}

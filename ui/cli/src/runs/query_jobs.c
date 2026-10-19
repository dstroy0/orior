// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_jobs.c: the query's programs found through the toolchains, every command of the query a job of the protocol
// group whose one step is orior itself with the command's words, and `orior protocol` run here
#include "../../../query/src/query_loops.h"
#include "../command_line/catalog.h"
#include "../programs/executables.h"
#include "../toolchains/toolchains.h"
#include "protocol.h"

// the PATH every program the query starts runs with
static char s_run_path[1u << 16u];
static CliLock s_run_path_lock = CLI_LOCK_INIT;

// `program` in the folder of the toolchain `tool`'s chosen program, where one is chosen, into `path`: 1, or 0 where
// none is
static int jobs_chosen(const char *tool, const char *program, char *path)
{
    char chosen[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    const char *const names[1] = {program};
    return cli_toolchains_chosen_program(tool, chosen) && cli_path_parent(chosen, folder) &&
           cli_toolchains_program_in(folder, names, 1u, path);
}

// the program of `role` as the toolchains give it, else as the PATH does, into `path`, which holds `room`: 1, or 0
// with why it is not found said
static int jobs_program(void *context, QueryRole role, char *path, size_t room, CliLine *said)
{
    (void)context;
    char found[CLI_PATH_LONGEST];
    int chosen = 0;
    switch (role)
    {
    case QUERY_ROLE_C:
        chosen = jobs_chosen("gcc", "gcc", found) || jobs_chosen("llvm", "clang", found);
        break;
    case QUERY_ROLE_CXX:
        chosen = jobs_chosen("gcc", "g++", found) || jobs_chosen("llvm", "clang++", found);
        break;
    case QUERY_ROLE_NVCC:
        chosen = jobs_chosen("cuda", "nvcc", found);
        break;
    case QUERY_ROLE_NVDISASM:
        chosen = jobs_chosen("cuda", "nvdisasm", found);
        break;
    case QUERY_ROLE_CUOBJDUMP:
        chosen = jobs_chosen("cuda", "cuobjdump", found);
        break;
    case QUERY_ROLE_CL:
        chosen = jobs_chosen("msvc", "cl", found);
        break;
    case QUERY_ROLE_PYTHON:
        chosen = jobs_chosen("python", "python3", found) || jobs_chosen("python", "python", found);
        break;
    default:
        break;
    }
    if (chosen && (cellul.len(&(CatenaFinitaCfg){.src = found, .cap = CLI_CELLS_POOL}) < room))
    {
        memor.cpy(&(MemoriaCfg){.dst = path,
                                .src = found,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = found, .cap = CLI_CELLS_POOL}) + 1u});
        return 1;
    }
    const QueryPrograms on_path = query_on_path();
    char why[CLI_SAID_LONGEST];
    CliLine none = {why, sizeof(why), 0ull};
    if (on_path.program(on_path.context, role, path, room, &none))
    {
        return 1;
    }
    static const char *const tools[QUERY_ROLE_COUNT] = {"gcc",    "gcc",  "cuda",   "cuda",  "cuda", "msvc",
                                                        "python", "cuda", "docker", "shell", "shell"};
    char program[CLI_PATH_LONGEST];
    const unsigned long long from = said->at;
    if (cli_executables_program(tools[role], query_role_name(role), program, said))
    {
        said->at = from;
        cli_line_text(said, query_role_name(role));
        cli_line_text(said, " is not found: File, Toolchains opens its install page or takes its folder");
    }
    return 0;
}

// the PATH the toolchains give every program they start
static const char *jobs_run_path(void *context)
{
    (void)context;
    cli_lock_take(&s_run_path_lock);
    cli_toolchains_run_path(s_run_path, sizeof(s_run_path));
    cli_lock_give(&s_run_path_lock);
    return s_run_path;
}

QueryPrograms cli_protocol_programs(void)
{
    return (QueryPrograms){jobs_program, jobs_run_path, NULL};
}

// a command's word put on the last step as the argument that makes it
static void jobs_word(CliCatalog *catalog, const QueryWord *word)
{
    switch (word->kind)
    {
    case QUERY_WORD_LITERAL:
        cli_catalog_arg(catalog, CLI_ARG_LIT, word->first, NULL, NULL, NULL, 0);
        break;
    case QUERY_WORD_VALUE:
        cli_catalog_arg(catalog, CLI_ARG_VALUE, word->first, NULL, NULL, NULL, 0);
        break;
    case QUERY_WORD_FLAG:
        cli_catalog_arg(catalog, CLI_ARG_FLAG, word->first, word->second, NULL, NULL, 0);
        break;
    case QUERY_WORD_EACH:
        cli_catalog_arg(catalog, CLI_ARG_EACH, word->first, word->second, NULL, NULL, 0);
        break;
    case QUERY_WORD_WORDS:
        cli_catalog_arg(catalog, CLI_ARG_WORDS, word->first, NULL, NULL, NULL, 0);
        break;
    default:
        cli_catalog_arg(catalog, CLI_ARG_WHEN, word->first, word->second, NULL, NULL, 0);
        cli_catalog_word(catalog, word->third);
        break;
    }
}

void cli_query_jobs(const char *root, CliCatalog *catalog)
{
    const QueryCommand *commands = NULL;
    const unsigned int count = query_commands(root, &commands);
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        const QueryCommand *const command = &commands[index];
        char id[CLI_NAME_LONGEST];
        CliLine id_line = {id, sizeof(id), 0ull};
        cli_line_text(&id_line, "protocol/");
        cli_line_text(&id_line, command->id);
        if ((cli_line_closed(&id_line) == 0ull) ||
            (cli_catalog_job(catalog, id, "protocol", command->title, QUERY_TABLE, command->about, "") ==
             CLI_CATALOG_NONE))
        {
            continue;
        }
        for (unsigned int at = 0u; at < command->param_count; at += 1u)
        {
            const QueryParam *const param = &command->params[at];
            cli_catalog_param(catalog, param->key, param->kind, param->required);
            for (unsigned int choice = 0u; choice < param->choice_count; choice += 1u)
            {
                cli_catalog_choice(catalog, param->choices[choice]);
            }
            // a command's param has no default: the window asks for one where it is required
            if (catalog->params > 0u)
            {
                catalog->param[catalog->params - 1u].fallback = CLI_CATALOG_NONE;
            }
        }
        cli_catalog_step(catalog, CLI_PROGRAM_ORIOR, NULL, NULL, 0);
        cli_catalog_arg(catalog, CLI_ARG_LIT, "protocol", NULL, NULL, NULL, 0);
        for (unsigned int at = 0u; at < command->word_count; at += 1u)
        {
            jobs_word(catalog, &command->words[at]);
        }
    }
}

int cli_protocol_run(const char *root, const char *const *words, unsigned int count, QuerySink sink)
{
    cli_toolchains_set_tree(root);
    const QueryPrograms programs = cli_protocol_programs();
    return query_run(root, &programs, words, count, sink);
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call.c: a call answered by name: its arguments read into cells taken for the call and given back once it has
// answered, and the call found among the tables of each source's calls. Arguments that are no JSON object are read as
// none
#include "../command_line/cells.h"
#include "serve_internal.h"

// One table of calls and its count
typedef struct
{
    const CliServeCall *call;
    const unsigned int *count;
} CliServeTable;

// the answer to the call `name` among the tables: it, or NULL where none answers it
static CliServeAnswer call_answer_of(const char *name)
{
    static const CliServeTable s_tables[] = {
        {g_cli_serve_tree_calls, &g_cli_serve_tree_count},
        {g_cli_serve_files_calls, &g_cli_serve_files_count},
        {g_cli_serve_git_calls, &g_cli_serve_git_count},
        {g_cli_serve_run_calls, &g_cli_serve_run_count},
        {g_cli_serve_lsp_calls, &g_cli_serve_lsp_count},
        {g_cli_serve_debug_calls, &g_cli_serve_debug_count},
        {g_cli_serve_db_calls, &g_cli_serve_db_count},
        {g_cli_serve_kernel_calls, &g_cli_serve_kernel_count},
        {g_cli_serve_tool_calls, &g_cli_serve_tool_count},
        {g_cli_serve_page_calls, &g_cli_serve_page_count},
        {g_cli_serve_notebook_calls, &g_cli_serve_notebook_count},
        {g_cli_serve_build_calls, &g_cli_serve_build_count},
        {g_cli_serve_shape_calls, &g_cli_serve_shape_count},
        {g_cli_serve_git_acts_calls, &g_cli_serve_git_acts_count},
    };
    for (unsigned int table = 0u; table < sizeof(s_tables) / sizeof(s_tables[0]); table += 1u)
    {
        for (unsigned int at = 0u; at < *s_tables[table].count; at += 1u)
        {
            if (cellul.eq(
                    &(CatenaFinitaCfg){.src = s_tables[table].call[at].name, .other = name, .cap = CLI_CELLS_POOL}))
            {
                return s_tables[table].call[at].answer;
            }
        }
    }
    return NULL;
}

int cli_serve_call(const char *name, const char *args, unsigned long long size, CliLine *out, CliLine *said)
{
    const CliServeAnswer answer = call_answer_of(name);
    if (answer == NULL)
    {
        cli_line_text(said, name);
        cli_line_text(said, " is not a command of the tree's");
        return 0;
    }
    return cli_serve_answered(answer, args, size, out, said);
}

int cli_serve_answered(CliServeAnswer answer, const char *args, unsigned long long size, CliLine *out, CliLine *said)
{
    CliJson json;
    char quiet[256];
    CliLine ignored = {quiet, sizeof(quiet), 0ull};
    unsigned int object = cli_json_taken(&json, args, size, 1u, 0ull, &ignored);
    object = cli_json_is(&json, object, CFG_JSON_OBJECT) ? object : cli_json_made(&json, CFG_JSON_OBJECT);
    int answered = 0;
    if (object == CLI_JSON_NONE)
    {
        cli_line_text(said, "the call's arguments could not be given cells");
    }
    else
    {
        const CliServeArgs read = {&json, object};
        answered = answer(&read, out, said);
    }
    cli_json_given(&json);
    return answered;
}

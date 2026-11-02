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

typedef struct
{
    const char *name;
} ServeCallCtx;
// the answer to the call `name` among the tables: it, or NULL where none answers it
EMBED_INLINE CliServeAnswer call_answer_get(const ServeCallCtx *args)
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
            if (cellul.eq(&(CatenaFinitaCfg){
                    .src = s_tables[table].call[at].name, .other = args->name, .cap = CLI_CELLS_POOL}))
            {
                return s_tables[table].call[at].answer;
            }
        }
    }
    return NULL;
}

int cli_serve_call(const CliServeErrMsgCfg *args)
{
    const CliServeAnswer answer = EMBED_CALL(call_answer_get, ServeCallCtx, .name = args->name);
    if (answer == NULL)
    {
        mmgr_line_put(args->err_msg, args->name);
        mmgr_line_put(args->err_msg, " is not a command of the tree's");
        return 0;
    }
    return EMBED_CALL(cli_serve_size_internal.answer, CliServeSizeCtx, .answer = answer, .args_text = args->request,
                      .size = args->size, .out = args->out, .err_msg = args->err_msg);
}

int cli_serve_on_answer(const CliServeSizeCtx *args)
{
    unsigned long long size = args->size;
    VerbaLinea *err_msg = args->err_msg;
    CliJson json;
    char quiet[256];
    VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
    unsigned int object = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take_temporary, CliJsonTreeTextSizeErrMsgCfg,
                                     .json = &json, .text = args->args_text, .size = size, .nodes_more = 1u,
                                     .text_more = 0ull, .err_msg = &ignored, .owner = &g_cli_window_cells);
    object = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json, .node = object,
                        .kind = CFG_JSON_OBJECT)
                 ? object
                 : EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = &json, .kind = CFG_JSON_OBJECT);
    int answered = 0;
    if (object == CLI_JSON_NONE)
    {
        mmgr_line_put(err_msg, "the call's arguments could not be given cells");
    }
    else
    {
        const CliServeArgs read = {&json, object};
        answered = EMBED_CALL(args->answer, CliServeAnswerCfg, .request = &read, .out = args->out, .err_msg = err_msg);
        // the pool leaves a cell as it is, and a password a call carries lies in its arguments' cell
        EMBED_CALL(cli_serve_err_msg_internal.secret_wiped, CliServeErrMsgCtx, .request = &read, .key = "password");
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    return answered;
}

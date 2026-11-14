// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_recv.c: what a server sends unprompted, and the answer where it requests: a file's diagnostics stored and
// passed on, a sliced file's moved to its own lines first, unless they come empty for a file the tree's check lets go,
// are of a slice it was given before its last, or are a window's of a file the check gathers them for; an edit it
// requests passed on and marked to be
// made; each setting it requests left to its own default but Python's; and its hints and its tokens requested again.
//
// Python's settings name the tree's environment's Python, where the tree has one, and, as its analysis, the folders
// the tree's scripts put on `sys.path` as they run, for their imports to be found. Pyright requests the analysis
// within `python`, and other servers for `python.analysis` alone. A setting the analysis leaves out is taken as off:
// the ones on by default are given
#include "../code/python_paths.h"
#include "../command_line/callback_cfg.h"
#include "../toolchains/toolchains.h"
#include "servers_internal.h"

// the most folders the analysis is given
#define CLI_SERVERS_EXTRA_PATHS 256u

// what is received read, under the reading's lock
static CliServersReadState s_read_state = {0};

typedef struct
{
    const char *text;
    unsigned long long length;
    const CliServersSink *sink;
    const char *params;
    unsigned long long size;
    const CliJson *json;
    unsigned int item;
    VerbaLinea *answer;
} ServersRecvCtx;
// whether `length` bytes of JSON contain an array with no items
EMBED_INLINE int recv_empty_array(const ServersRecvCtx *args)
{
    const char *text = args->text;
    unsigned long long at =
        EMBED_CALL(cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg, .text = text, .size = args->length);
    if ((at == args->length) || (text[at] != '['))
    {
        return 0;
    }
    at += 1ULL;
    at +=
        EMBED_CALL(cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg, .text = text + at, .size = args->length - at);
    return (at < args->length) && (text[at] == ']');
}

// a file's diagnostics stored, by its key with its URI, and passed on whole
EMBED_INLINE void recv_diagnostics(const ServersRecvCtx *args)
{
    const CliServersSink *sink = args->sink;
    const char *params = args->params;
    unsigned long long size = args->size;
    unsigned long long uri_at = 0ULL;
    unsigned long long uri_length = 0ULL;
    unsigned long long all_at = 0ULL;
    unsigned long long all_length = 0ULL;
    char uri[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = params, .size_value = size, .name = "uri",
                    .start = &uri_at, .length_pointer = &uri_length) ||
        !EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = params, .size_value = size, .name = "diagnostics",
                    .start = &all_at, .length_pointer = &all_length) ||
        !EMBED_CALL(cli_lsp_capacity.string, CliLspCapacityCfg, .text = params + uri_at, .length = uri_length,
                    .out_capacity = uri, .capacity = sizeof(uri)))
    {
        return;
    }
    const char *all = params + all_at;
    const unsigned long long lead =
        EMBED_CALL(cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg, .text = all, .size = all_length);
    if ((lead == all_length) || (all[lead] != '['))
    {
        return;
    }
    // a sliced file's diagnostics are of its slice, and stand on the file's own lines once moved there
    const unsigned long long uri_size = cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_POOL_BYTES});
    char path[CLI_PATH_MAX_LEN];
    const int sliced =
        EMBED_CALL(cli_lsp_length_uri.path_get, CliLspLengthUriCfg, .uri = uri, .length = uri_size, .path_out = path) &&
        EMBED_CALL(cli_servers_path_lsp_internal.slice, CliServersPathLspCtx, .path = path);
    char *const mapped = sliced
                             ? EMBED_CALL(cli_servers_path_size_internal.slice_diagnostics, CliServersPathSizeCtx,
                                          .path = path, .params_text = params, .params_size = size, .all = all,
                                          .size = all_length, .mapped_size = &all_length, .owner = g_cli_servers_owner)
                             : NULL;
    if (sliced && (mapped == NULL))
    {
        return;
    }
    all = sliced ? mapped : all;
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_lsp_length.key_get, CliLspLengthCfg, .uri = uri, .length = uri_size, .key = key);
    if ((EMBED_CALL(recv_empty_array, ServersRecvCtx, .text = all, .length = all_length) &&
         EMBED_CALL(cli_servers_key_checker_internal.letting_go, CliServersKeyCheckerCtx, .key = key)) ||
        EMBED_CALL(cli_servers_size_key_internal.gather, CliServersSizeKeyCtx, .key = key, .all = all,
                   .size = all_length))
    {
        g_cli_guard->persistent_buf_release(mapped, g_cli_servers_owner);
        return;
    }
    EMBED_CALL(cli_servers_key_checker_internal.wake, CliServersKeyCheckerCtx, .key = key);
    (void)EMBED_CALL(cli_keyed_key_adjacent.put, CliKeyedKeyAdjacentCfg, .keyed_out = &g_cli_servers_published,
                     .key = key, .adjacent_value = uri, .text = all, .size_value = all_length);
    g_cli_guard->persistent_buf_release(mapped, g_cli_servers_owner);
    EMBED_CALL(cli_servers_sink_key_internal.emit_file, CliServersSinkKeyCtx, .key = key, .uri = uri, .sink = sink);
}

// an edit the server requests passed on as each file's edits
EMBED_INLINE void recv_edit(const ServersRecvCtx *args)
{
    unsigned long long size = args->size;
    EMBED_CALL(cli_servers_read_state_answer_internal.read_state_lock, CliServersReadStateAnswerCtx,
               .read_state = &s_read_state);
    const unsigned int root = EMBED_CALL(cli_servers_read_state_internal.read, CliServersReadStateCtx,
                                         .read_state = &s_read_state, .text = args->params, .size = size);
    CliGrownLine edits;
    cli_grown_begin(&edits, CLI_SERVERS_ANSWER, g_cli_servers_owner);
    EMBED_CALL(cli_servers_json_tree_out_internal.workspace_edit_get, CliServersJsonTreeOutCtx,
               .json = &s_read_state.json,
               .edit = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                  .json_read = &s_read_state.json, .object = root, .name = "edit"),
               .tree = args->sink->tree, .out = &edits.line);
    if (mmgr_line_ok(&edits.line) && (args->sink->emit != NULL))
    {
        EMBED_CALL(args->sink->emit, CliEachEventJsonSizeCfg, .context = args->sink->context, .event = "lsp-edits",
                   .json = edits.cell, .size = cli_grown_size(&edits));
    }
    cli_grown_end(&edits);
    g_cli_guard->persistent_buf_release(s_read_state.json.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), s_read_state.json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_read_state.json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
}

// One folder of the analysis written: where it is written and whether one is
typedef struct
{
    VerbaLinea *answer;
    int written;
} CliRecvPaths;

// a folder the tree's scripts put on `sys.path` written as a JSON string after those written
static void recv_path(const CliEachPathCfg *args)
{
    CliRecvPaths *const paths = args->context;
    mmgr_line_put(paths->answer, paths->written ? "," : "");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = paths->answer,
               .text_bytes = (const unsigned char *)args->path,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}));
    paths->written = 1;
}

// Python's analysis for the item `item` requests of: the folders the scripts of its tree put on `sys.path`
EMBED_INLINE void recv_analysis(const ServersRecvCtx *args)
{
    const CliJson *json = args->json;
    unsigned long long length = 0ULL;
    const char *const scope =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->item, .name = "scopeUri"),
                   .length_pointer = &length);
    char root[CLI_PATH_MAX_LEN];
    mmgr_line_put(args->answer, "{\"autoImportCompletions\":true,\"autoSearchPaths\":true,\"extraPaths\":[");
    CliRecvPaths paths = {args->answer, 0};
    if ((scope != NULL) &&
        EMBED_CALL(cli_lsp_length_uri.path_get, CliLspLengthUriCfg, .uri = scope, .length = length, .path_out = root))
    {
        (void)EMBED_CALL(cli_python_paths_table.python_paths, CliPythonPathsCfg, .root = root,
                         .most = CLI_SERVERS_EXTRA_PATHS, .each = recv_path, .context = &paths);
    }
    mmgr_line_put(args->answer, "],\"useLibraryCodeForTypes\":true}");
}

// each setting the server requests: Python's and its analysis given, every other left to its own default
EMBED_INLINE void recv_configuration(const ServersRecvCtx *args)
{
    unsigned long long size = args->size;
    EMBED_CALL(cli_servers_read_state_answer_internal.read_state_lock, CliServersReadStateAnswerCtx,
               .read_state = &s_read_state);
    const CliJson *const json = &s_read_state.json;
    const unsigned int items =
        EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                   .object = EMBED_CALL(cli_servers_read_state_internal.read, CliServersReadStateCtx,
                                        .read_state = &s_read_state, .text = args->params, .size = size),
                   .name = "items");
    static CliTreeEnvironment s_environment;
    const int python =
        EMBED_CALL(cli_toolchains_table.environment, CliToolchainsCfg, .environment_out = &s_environment) &&
        s_environment.has_python;
    mmgr_line_ch(args->answer, '[');
    int first = 1;
    for (unsigned int item = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json,
                                        .node = items, .type = CFG_JSON_ARRAY)
                                 ? json->node[items].first
                                 : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        mmgr_line_put(args->answer, first ? "" : ",");
        first = 0;
        unsigned long long length = 0ULL;
        const char *const section =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = item, .name = "section"),
                       .length_pointer = &length);
        if ((section != NULL) && (length == 6ULL) &&
            (memor.cmp(&(MemoriaCfg){.src = section, .other = "python", .bytes = 6u}) == 0))
        {
            mmgr_line_put(args->answer, "{\"analysis\":");
            EMBED_CALL(recv_analysis, ServersRecvCtx, .json = json, .item = item, .answer = args->answer);
            if (python)
            {
                mmgr_line_put(args->answer, ",\"defaultInterpreterPath\":");
                EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->answer,
                           .text_bytes = (const unsigned char *)s_environment.python,
                           .length =
                               cellul.len(&(CatenaFinitaCfg){.src = s_environment.python, .cap = CLI_POOL_BYTES}));
                mmgr_line_put(args->answer, ",\"pythonPath\":");
                EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->answer,
                           .text_bytes = (const unsigned char *)s_environment.python,
                           .length =
                               cellul.len(&(CatenaFinitaCfg){.src = s_environment.python, .cap = CLI_POOL_BYTES}));
            }
            mmgr_line_ch(args->answer, '}');
        }
        else if ((section != NULL) && (length == 15ULL) &&
                 (memor.cmp(&(MemoriaCfg){.src = section, .other = "python.analysis", .bytes = 15u}) == 0))
        {
            EMBED_CALL(recv_analysis, ServersRecvCtx, .json = json, .item = item, .answer = args->answer);
        }
        else
        {
            mmgr_line_put(args->answer, "null");
        }
    }
    mmgr_line_ch(args->answer, ']');
    g_cli_guard->persistent_buf_release(s_read_state.json.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), s_read_state.json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_read_state.json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
}

void cli_servers_recv(const CliLspRecvCfg *args)
{
    const char *params = args->params;
    unsigned long long size = args->size;
    VerbaLinea *answer = args->answer;
    const CliServersSink *const sink = args->context;
    if (cellul.eq(
            &(CatenaFinitaCfg){.src = args->method, .other = "textDocument/publishDiagnostics", .cap = CLI_POOL_BYTES}))
    {
        EMBED_CALL(recv_diagnostics, ServersRecvCtx, .sink = sink, .params = params, .size = size);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->method, .other = "workspace/applyEdit", .cap = CLI_POOL_BYTES}))
    {
        EMBED_CALL(recv_edit, ServersRecvCtx, .sink = sink, .params = params, .size = size);
        mmgr_line_put(answer, "{\"applied\":true}");
    }
    else if (cellul.eq(
                 &(CatenaFinitaCfg){.src = args->method, .other = "workspace/configuration", .cap = CLI_POOL_BYTES}))
    {
        EMBED_CALL(recv_configuration, ServersRecvCtx, .params = params, .size = size, .answer = answer);
    }
    else if ((cellul.eq(&(CatenaFinitaCfg){
                 .src = args->method, .other = "workspace/inlayHint/refresh", .cap = CLI_POOL_BYTES})) &&
             (sink->emit != NULL))
    {
        EMBED_CALL(sink->emit, CliEachEventJsonSizeCfg, .context = sink->context, .event = "lsp-hints", .json = "null",
                   .size = 4ULL);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){
                 .src = args->method, .other = "workspace/semanticTokens/refresh", .cap = CLI_POOL_BYTES}))
    {
        cli_servers_forget_tokens();
        if (sink->emit != NULL)
        {
            EMBED_CALL(sink->emit, CliEachEventJsonSizeCfg, .context = sink->context, .event = "lsp-parse",
                       .json = "null", .size = 4ULL);
        }
    }
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_request.c: what the server of a file answers about a place in it: a hover, the definitions, the completions,
// orior's own reading's of a build file or JSON first, every usage of a symbol with its line's text read from its file
// once, whether the symbol can be renamed and the edits that rename it, and the signature of the call the cursor is
// in. An answer requested while no server has the file open is none, but a usage, a rename and what it would change,
// which report there is no server to query. A file given its server as slices is asked of the tree alone: its
// definition, usages and renames are the tree's (servers_usages.c), its completions the names the tree has in effect
// at the place (symbols_complete.c), and what the tree does not answer is none
#include "../builds/builds.h"
#include "../code/schema.h"
#include "../code/symbols.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "servers_internal.h"

#include "../text/text.h"

// One usage: its path in the paths' pool, its span, and its place among the usages
typedef struct
{
    unsigned int path;
    CliTextPosition from;
    CliTextPosition to;
    unsigned int order;
} CliServersUsage;

// what a server answers quickly and what a search of the tree answers, each read under its own lock
static CliServersReadState s_quick = {0};
// the completions, under the quick reading's lock; and the usages and their paths, each in a cell of the answer's
// size, under the search's
static CliCompletions s_items;

typedef struct
{
    const char *path;
    VerbaLinea *err_msg;
    CliLsp *server;
    CliServersReadState *read_state;
    const char *method;
    unsigned int line;
    unsigned int col;
    const char *added;
    unsigned long long wait;
    int *requested;
    VerbaLinea *out;
    unsigned int one;
    char *path_out;
    CliTextPosition *from;
    CliTextPosition *to;
    unsigned int all;
    const char *file;
    unsigned long long size;
    unsigned int *at;
    unsigned long long *start;
    const CellOwner *owner;
} ServersRequestCtx;
// `method` sent to `server` about `line`, `col` of `path`, `added` after the place, the answer read into `read_state`:
// its root, or CLI_JSON_NONE with why given, where `requested` is 0
EMBED_INLINE unsigned int request_read(const ServersRequestCtx *args)
{
    CliLsp *server = args->server;
    CliServersReadState *read_state = args->read_state;
    const char *method = args->method;
    const char *path = args->path;
    unsigned int line = args->line;
    unsigned int col = args->col;
    const char *added = args->added;
    unsigned long long wait = args->wait;
    int *requested = args->requested;
    VerbaLinea *err_msg = args->err_msg;
    CliLspAnswer answer;
    *requested = EMBED_CALL(cli_servers_path_internal.request_at, CliServersPathCtx, .server = server, .method = method,
                            .path = path, .line = line, .col = col, .added = added, .wait = wait, .answer = &answer,
                            .err_msg = err_msg);
    EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
    return *requested ? EMBED_CALL(cli_servers_read_state_answer_internal.answer_read, CliServersReadStateAnswerCtx,
                                   .read_state = read_state, .answer = &answer)
                      : CLI_JSON_NONE;
}

int cli_servers_hover(const CliServersPathOutCfg *args)
{
    CliLsp *const server =
        EMBED_CALL(cli_servers_path_lsp_internal.owner_answering, CliServersPathLspCtx, .path = args->path);
    if (server == NULL)
    {
        return 1;
    }
    EMBED_CALL(cli_servers_read_state_answer_internal.read_state_lock, CliServersReadStateAnswerCtx,
               .read_state = &s_quick);
    int requested = 0;
    const unsigned int root = EMBED_CALL(request_read, ServersRequestCtx, .server = server, .read_state = &s_quick,
                                         .method = "textDocument/hover", .path = args->path, .line = args->line,
                                         .col = args->col, .added = NULL, .wait = CLI_SERVERS_REQUEST_TIMEOUT_MS,
                                         .requested = &requested, .err_msg = args->err_msg);
    if (requested)
    {
        const unsigned long long capacity =
            EMBED_CALL(cli_json_tree_node_out.json_write_capacity, CliJsonTreeNodeOutCfg, .json_read = &s_quick.json);
        char *const displayed = g_cli_guard->temporary_buf_alloc(capacity, g_cli_servers_owner);
        VerbaLinea markdown = mmgr_line_over(displayed, capacity);
        EMBED_CALL(cli_servers_json_out_internal.markdown_get, CliServersJsonOutCtx, .json = &s_quick.json,
                   .contents = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = &s_quick.json, .object = root, .name = "contents"),
                   .out = &markdown);
        const unsigned long long size = mmgr_line_ok(&markdown) ? markdown.at : 0ULL;
        unsigned long long from = 0ULL;
        unsigned long long to = 0ULL;
        EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = (size > 0ULL) ? displayed : "",
                   .size = size, .from_u64s = &from, .to_u64s = &to);
        if (to > from)
        {
            mmgr_line_put_n(args->out, displayed, size);
        }
        g_cli_guard->persistent_buf_release(displayed, g_cli_servers_owner);
    }
    g_cli_guard->persistent_buf_release(s_quick.json.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), s_quick.json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_quick.json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return requested;
}

// the completions read into one cell of `owner`, each text ended by a 0: the cell, or NULL where the cells cannot meet
// it
EMBED_INLINE CliServerItems *request_items_alloc(const ServersRequestCtx *args)
{
    unsigned long long bytes = 0ULL;
    for (unsigned int at = 0u; at < s_items.count; at += 1u)
    {
        const CliCompletion *const one = &s_items.item[at];
        bytes += (unsigned long long)one->label_size + one->detail_size + one->insert_size + 3ULL +
                 cellul.len(&(CatenaFinitaCfg){.src = one->type, .cap = CLI_POOL_BYTES}) + 1ULL;
    }
    CliServerItems *const items =
        g_cli_guard->temporary_buf_alloc(sizeof(*items) + (s_items.count * sizeof(CliServerItem)) + bytes, args->owner);
    if (items == NULL)
    {
        return NULL;
    }
    items->count = s_items.count;
    items->item = (CliServerItem *)(void *)(items + 1);
    char *text = (char *)(void *)(items->item + s_items.count);
    for (unsigned int at = 0u; at < s_items.count; at += 1u)
    {
        const CliCompletion *const one = &s_items.item[at];
        CliServerItem *const item = &items->item[at];
        item->snippet = one->snippet;
        item->label = text;
        memor.cpy(&(MemoriaCfg){.dst = text, .src = s_items.text + one->label_at, .bytes = one->label_size});
        text[one->label_size] = '\0';
        text += one->label_size + 1ULL;
        item->detail = text;
        memor.cpy(&(MemoriaCfg){.dst = text, .src = s_items.text + one->detail_at, .bytes = one->detail_size});
        text[one->detail_size] = '\0';
        text += one->detail_size + 1ULL;
        item->insert = text;
        memor.cpy(&(MemoriaCfg){.dst = text, .src = s_items.text + one->insert_at, .bytes = one->insert_size});
        text[one->insert_size] = '\0';
        text += one->insert_size + 1ULL;
        const unsigned long long type = cellul.len(&(CatenaFinitaCfg){.src = one->type, .cap = CLI_POOL_BYTES}) + 1ULL;
        item->type = text;
        memor.cpy(&(MemoriaCfg){.dst = text, .src = one->type, .bytes = type});
        text += type;
    }
    return items;
}

// a name the tree completes a word with put among the completions as what it is, a type the tree names saved by the
// page's own word for it, `word` for one it does not name, as an item saves its type by pointer
static void request_tree_item(const CliDeclaredCfg *args)
{
    const char *type = args->type;
    static const char *const s_types[8] = {"function", "constant", "class",     "method",
                                           "module",   "variable", "parameter", "local"};
    const char *named = "word";
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        named = cellul.eq(&(CatenaFinitaCfg){.src = type, .other = s_types[at], .cap = CLI_POOL_BYTES}) ? s_types[at]
                                                                                                        : named;
    }
    (void)EMBED_CALL(cli_edits_items.completion_append, CliEditsItemsCfg, .items = &s_items, .label = args->name,
                     .label_size = args->length, .type = named, .detail = "", .detail_size = 0ULL, .insert = args->name,
                     .insert_size = args->length, .snippet = 0);
}

// the names the tree completes the word at `line`, `col` of the file at `path` with, the editor's text of it read,
// put among the completions
EMBED_INLINE void request_tree_complete(const ServersRequestCtx *args)
{
    const char *path = args->path;
    unsigned int line = args->line;
    unsigned int col = args->col;
    char tree[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = tree, .src = g_cli_servers.checked_root, .bytes = sizeof(tree)});
    char own[CLI_PATH_MAX_LEN];
    VerbaLinea own_line = {own, sizeof(own), 0ULL};
    EMBED_CALL(cli_servers_path_err_msg_internal.tree_path, CliServersPathErrMsgCtx, .tree = tree, .path = path,
               .out = &own_line);
    unsigned long long size = 0ULL;
    char *const text = (mmgr_line_finish(&own_line) != 0ULL)
                           ? EMBED_CALL(cli_servers_path_size_type.text_alloc, CliServersPathSizeTypeCfg, .path = path,
                                        .size_pointer = &size)
                           : NULL;
    char quiet[256];
    VerbaLinea err_msg = {quiet, sizeof(quiet), 0ULL};
    if (text != NULL)
    {
        (void)EMBED_CALL(cli_symbols_path_text_size.complete, CliSymbolsPathTextSizeCfg, .path = own, .text = text,
                         .size = size, .line = line, .col = col, .each_declared = request_tree_item, .context = NULL,
                         .err_msg = &err_msg);
    }
    g_cli_guard->persistent_buf_release(text, g_cli_servers_owner);
}

CliServerItems *cli_servers_items_alloc(const CliServersAskCfg *args)
{
    const char *path = args->path;
    unsigned int line = args->line;
    unsigned int col = args->col;
    EMBED_CALL(cli_servers_read_state_answer_internal.read_state_lock, CliServersReadStateAnswerCtx,
               .read_state = &s_quick);
    EMBED_CALL(cli_edits_items.completions_begin, CliEditsItemsCfg, .items = &s_items);
    // a file given its server as a slice is completed from the tree
    if (EMBED_CALL(cli_servers_path_lsp_internal.slice, CliServersPathLspCtx, .path = path))
    {
        EMBED_CALL(request_tree_complete, ServersRequestCtx, .path = path, .line = line, .col = col);
    }
    if (EMBED_CALL(cli_servers_path_size_root_sink_internal.own_reads, CliServersPathSizeRootSinkCtx, .path = path))
    {
        char root[CLI_PATH_MAX_LEN];
        memor.cpy(&(MemoriaCfg){.dst = root, .src = g_cli_servers.checked_root, .bytes = sizeof(root)});
        unsigned long long size = 0ULL;
        char *const text = (root[0] != '\0')
                               ? EMBED_CALL(cli_servers_path_size_type.text_alloc, CliServersPathSizeTypeCfg,
                                            .path = path, .size_pointer = &size)
                               : NULL;
        if ((text != NULL) && (EMBED_CALL(cli_builds_path.build_type_get, CliBuildsPathCfg, .path = path) != 0u))
        {
            EMBED_CALL(cli_builds_path.build_complete, CliBuildsPathCfg, .root = root, .path = path, .text = text,
                       .size = size, .line = line, .col = col, .items = &s_items);
        }
        else if (text != NULL)
        {
            EMBED_CALL(cli_schema_line.complete, CliSchemaLineCfg, .path = path, .text = text, .size = size,
                       .line = line, .col = col, .items = &s_items);
        }
        g_cli_guard->persistent_buf_release(text, g_cli_servers_owner);
    }
    CliLsp *const server =
        EMBED_CALL(cli_servers_path_lsp_internal.owner_answering, CliServersPathLspCtx, .path = path);
    int requested = 1;
    if (server != NULL)
    {
        const unsigned long long before = args->err_msg->at;
        const unsigned int root =
            EMBED_CALL(request_read, ServersRequestCtx, .server = server, .read_state = &s_quick,
                       .method = "textDocument/completion", .path = path, .line = line, .col = col,
                       .added = "\"context\":{\"triggerKind\":1}", .wait = CLI_SERVERS_REQUEST_TIMEOUT_MS,
                       .requested = &requested, .err_msg = args->err_msg);
        if (!requested && (s_items.count > 0u))
        {
            args->err_msg->at = before;
            requested = 1;
        }
        const CliJson *const json = &s_quick.json;
        const unsigned int items = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg,
                                              .json_read = json, .node = root, .type = CFG_JSON_ARRAY)
                                       ? root
                                       : EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                    .json_read = json, .object = root, .name = "items");
        for (unsigned int one = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json,
                                           .node = items, .type = CFG_JSON_ARRAY)
                                    ? json->node[items].first
                                    : CLI_JSON_NONE;
             one != CLI_JSON_NONE; one = json->node[one].next)
        {
            EMBED_CALL(cli_servers_json_one_internal.item_get, CliServersJsonOneCtx, .json = json, .one = one,
                       .items = &s_items);
        }
    }
    CliServerItems *const answered =
        requested ? EMBED_CALL(request_items_alloc, ServersRequestCtx, .owner = args->owner) : NULL;
    g_cli_guard->persistent_buf_release(s_quick.json.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), s_quick.json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_quick.json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return answered;
}

int cli_servers_signature_read(const CliServersAskCfg *args)
{
    CliLsp *const server =
        EMBED_CALL(cli_servers_path_lsp_internal.owner_answering, CliServersPathLspCtx, .path = args->path);
    if (server == NULL)
    {
        return 0;
    }
    EMBED_CALL(cli_servers_read_state_answer_internal.read_state_lock, CliServersReadStateAnswerCtx,
               .read_state = &s_quick);
    int requested = 0;
    const unsigned int root = EMBED_CALL(request_read, ServersRequestCtx, .server = server, .read_state = &s_quick,
                                         .method = "textDocument/signatureHelp", .path = args->path, .line = args->line,
                                         .col = args->col, .added = NULL, .wait = CLI_SERVERS_REQUEST_TIMEOUT_MS,
                                         .requested = &requested, .err_msg = args->err_msg);
    const int written =
        requested && EMBED_CALL(cli_servers_json_out_help_internal.signature_get, CliServersJsonOutHelpCtx,
                                .json = &s_quick.json, .help = root, .store = args->store);
    g_cli_guard->persistent_buf_release(s_quick.json.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), s_quick.json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_quick.json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return written;
}

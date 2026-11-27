// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// scripts_calls.c: the calls of `orior.*` that a host sends as `orior/call` {name, args} (scripts_internal.h). Each
// call has its entry in a table. An entry can need a right: the path or the host of its first argument must be in the
// right's list of plugin.json, and the reader must allow the right. A call of a right that the reader did not answer
// waits. The window asks the reader, and the answer goes back to the host when the reader answers
#include "../command_line/pool.h"
#include "../editor/files.h"
#include "../servers/lsp.h"
#include "../servers/servers.h"
#include "../text/json_tree.h"
#include "scripts_internal.h"

// The most words that a server's start puts before the words of its plugin.json. Then how long a request to a
// plugin's server waits for its answer, in milliseconds
#define CLI_SCRIPTS_SERVER_WORDS 32u
#define CLI_SCRIPTS_SERVER_WAIT_MS 30000ULL

// The most calls that wait on the reader at once, and the most bytes of the params that one wait keeps. Then the most
// bytes of a file that a call reads, and the right of an entry that needs no right
#define CLI_SCRIPTS_HELD 32u
#define CLI_SCRIPTS_HELD_PARAMS (1ULL << 20u)
#define CLI_SCRIPTS_FILE_MAX (CLI_LSP_ANSWERED / 8ULL)
#define CLI_SCRIPTS_NO_RIGHT CLI_SCRIPTS_RIGHTS

// the bytes of the copy that a read of the keychain reads one secret into, its 0 with them
#define CLI_SCRIPTS_SECRET_MAX 4096u

// One call that waits on the reader: whether it holds its place, its plugin, and the right that it needs. Then the
// server that waits on its answer, the request's id as JSON, and its params in a cell of the call's own
typedef struct
{
    int used;
    char id[CLI_NAME_MAX_LEN];
    unsigned int right;
    CliLsp *lsp;
    char request[64];
    char *params;
    unsigned long long size;
} ScriptsHeld;

// the tree; the calls that wait, under their gate; and the question that shows now, its plugin and its right
static char s_root[CLI_PATH_MAX_LEN];
static int s_rooted;
static ScriptsHeld s_held[CLI_SCRIPTS_HELD];
static CliGate s_held_gate = CLI_GATE_INIT;
static int s_asking;
static char s_asking_id[CLI_NAME_MAX_LEN];
static unsigned int s_asking_right;

typedef struct
{
    const char *id;
    CliLsp *lsp;
    const char *params;
    unsigned long long size;
    VerbaLinea *answer;
    const CliJson *json;
    unsigned int root;
    unsigned int args;
    unsigned int index;
    unsigned int right;
    const char *name;
    const char *why;
    char *path_out;
    int may_wait;
    int asks_answer;
} ScriptsCallsCtx;

// an answer of the error `why`, as the host rejects a call's promise with it
EMBED_INLINE void calls_refused(const ScriptsCallsCtx *args)
{
    mmgr_line_put(args->answer, "{\"oriorError\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->answer,
               .text_bytes = (const unsigned char *)args->why,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->why, .cap = CLI_ERR_MSG_MAX_LEN}));
    mmgr_line_put(args->answer, "}");
}

// the string at the place `index` of the call's args, NULL where it is no string
EMBED_INLINE const char *calls_string(const ScriptsCallsCtx *args)
{
    const unsigned int node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                         .json_read = args->json, .array = args->args, .index = args->index);
    return EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                      .node = node, .length_pointer = NULL);
}

// the full path of the tree's file `name` into `path_out`, sized for CLI_PATH_MAX_LEN: 1, or 0 where no tree is open
EMBED_INLINE int calls_full(const ScriptsCallsCtx *args)
{
    if (!s_rooted)
    {
        return 0;
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = args->name, .joined = args->path_out);
    return 1;
}

// files.read(path): the text of the tree's file, as a JSON string
static void calls_files_read(const ScriptsCallsCtx *args)
{
    const char *const name =
        EMBED_CALL(calls_string, ScriptsCallsCtx, .json = args->json, .args = args->args, .index = 0u);
    char path[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(calls_full, ScriptsCallsCtx, .name = name, .path_out = path))
    {
        EMBED_CALL(calls_refused, ScriptsCallsCtx, .answer = args->answer, .why = "no tree is open");
        return;
    }
    unsigned long long size = 0ULL;
    const unsigned char *const text = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                 .size_pointer = &size, .owner = g_cli_scripts_owner);
    if ((text == NULL) || (size > CLI_SCRIPTS_FILE_MAX))
    {
        EMBED_CALL(calls_refused, ScriptsCallsCtx, .answer = args->answer,
                   .why = (text == NULL) ? "the file could not be read" : "the file is too large to read");
    }
    else
    {
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->answer,
                   .text_bytes = text, .length = size);
    }
    g_platform_guard->persistent_buf_release((void *)(embed_word)text, g_cli_scripts_owner);
}

// files.write(path, text): the text written as the tree's file, null after
static void calls_files_write(const ScriptsCallsCtx *args)
{
    const char *const name =
        EMBED_CALL(calls_string, ScriptsCallsCtx, .json = args->json, .args = args->args, .index = 0u);
    const unsigned int node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                         .json_read = args->json, .array = args->args, .index = 1u);
    unsigned long long length = 0ULL;
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                        .json_read = args->json, .node = node, .length_pointer = &length);
    char path[CLI_PATH_MAX_LEN];
    if (text == NULL)
    {
        EMBED_CALL(calls_refused, ScriptsCallsCtx, .answer = args->answer, .why = "the text to write is no string");
    }
    else if (!EMBED_CALL(calls_full, ScriptsCallsCtx, .name = name, .path_out = path))
    {
        EMBED_CALL(calls_refused, ScriptsCallsCtx, .answer = args->answer, .why = "no tree is open");
    }
    else if (!EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = text,
                         .size = length))
    {
        EMBED_CALL(calls_refused, ScriptsCallsCtx, .answer = args->answer, .why = "the file could not be written");
    }
    else
    {
        mmgr_line_put(args->answer, "null");
    }
}

// One listing of the tree's files into an answer: the answer, and the plugin whose reads hold the list. Then whether a
// path went into the answer before
typedef struct
{
    VerbaLinea *answer;
    const char *id;
    int listed;
} ScriptsListing;

// One file of the tree that the plugin's reads names, written into the answer as a JSON string. A comma comes before
// each file but the first
static void calls_tree_listed(const CliEachPathLengthCfg *args)
{
    ScriptsListing *const listing = (ScriptsListing *)args->context;
    if (!EMBED_CALL(cli_scripts_rights.named, CliScriptsRightsCfg, .id = listing->id, .right = CLI_SCRIPTS_READS,
                    .name = args->path))
    {
        return;
    }
    if (listing->listed)
    {
        mmgr_line_ch(listing->answer, ',');
    }
    listing->listed = 1;
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = listing->answer,
               .text_bytes = (const unsigned char *)args->path, .length = args->length);
}

// tree.files(): the tree's files that the plugin's reads names, as a JSON array, each its path from the top with `/`
// between its folders. Git gives the list where git reads the tree. The call refuses a list that does not fit in one
// answer, and never cuts it
static void calls_tree_files(const ScriptsCallsCtx *args)
{
    if (!s_rooted)
    {
        EMBED_CALL(calls_refused, ScriptsCallsCtx, .answer = args->answer, .why = "no tree is open");
        return;
    }
    const unsigned long long before = args->answer->at;
    ScriptsListing listing = {args->answer, args->id, 0};
    mmgr_line_ch(args->answer, '[');
    int git = 0;
    const int whole = EMBED_CALL(cli_files_root_each_context.list, CliFilesRootEachContextCfg, .root = s_root,
                                 .each = calls_tree_listed, .context = &listing, .git_pointer = &git);
    mmgr_line_ch(args->answer, ']');
    if (!whole || !mmgr_line_ok(args->answer))
    {
        args->answer->at = before;
        EMBED_CALL(calls_refused, ScriptsCallsCtx, .answer = args->answer,
                   .why = whole ? "the tree's files do not fit in one answer"
                                : "the tree has more files than one list holds");
    }
}

// tree.relative(path): the path from the tree's top of a whole path, with `/` between its folders, as a JSON string. A
// file of a mount is under the mount's name. null where the path is outside the tree and its mounts
static void calls_tree_relative(const ScriptsCallsCtx *args)
{
    const char *const path =
        EMBED_CALL(calls_string, ScriptsCallsCtx, .json = args->json, .args = args->args, .index = 0u);
    if (path == NULL)
    {
        EMBED_CALL(calls_refused, ScriptsCallsCtx, .answer = args->answer, .why = "the path is no string");
        return;
    }
    if (!s_rooted)
    {
        EMBED_CALL(calls_refused, ScriptsCallsCtx, .answer = args->answer, .why = "no tree is open");
        return;
    }
    char relative[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .path = path, .root = s_root, .relative_out = relative);
    // a path outside the tree and its mounts comes back whole, with a top of its own
    if (EMBED_CALL(cli_path_internal.top, CliPathCtx, .path = relative) != 0ULL)
    {
        mmgr_line_put(args->answer, "null");
        return;
    }
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->answer,
               .text_bytes = (const unsigned char *)relative,
               .length = cellul.len(&(CatenaFinitaCfg){.src = relative, .cap = CLI_PATH_MAX_LEN}));
}

// the keychain's service of the plugin `id` into `path_out`, sized for CLI_NAME_MAX_LEN * 2: "plugin " and the id
EMBED_INLINE void calls_secret_service(const ScriptsCallsCtx *args)
{
    VerbaLinea service = {args->path_out, CLI_NAME_MAX_LEN * 2u, 0ULL};
    mmgr_line_put(&service, "plugin ");
    mmgr_line_put(&service, args->id);
    (void)mmgr_line_finish(&service);
}

// secrets.read(key): the secret that the plugin stored under `key` in the system's keychain, as a JSON string, null
// where it stored none
static void calls_secrets_read(const ScriptsCallsCtx *args)
{
    const char *const key =
        EMBED_CALL(calls_string, ScriptsCallsCtx, .json = args->json, .args = args->args, .index = 0u);
    if (key == NULL)
    {
        EMBED_CALL(calls_refused, ScriptsCallsCtx, .answer = args->answer, .why = "the key is no string");
        return;
    }
    char service[CLI_NAME_MAX_LEN * 2u];
    EMBED_CALL(calls_secret_service, ScriptsCallsCtx, .id = args->id, .path_out = service);
    char secret[CLI_SCRIPTS_SECRET_MAX];
    if (!EMBED_CALL(cli_service_internal.keychain_read, CliServiceCtx, .service = service, .account = key,
                    .out = secret, .capacity = sizeof(secret)))
    {
        mmgr_line_put(args->answer, "null");
        return;
    }
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->answer,
               .text_bytes = (const unsigned char *)secret,
               .length = cellul.len(&(CatenaFinitaCfg){.src = secret, .cap = sizeof(secret)}));
}

// secrets.write(key, secret): the secret stored under `key` in the system's keychain, or forgotten where it is null,
// null after. The keychain refuses a secret past what it keeps, with the system's reason
static void calls_secrets_write(const ScriptsCallsCtx *args)
{
    const char *const key =
        EMBED_CALL(calls_string, ScriptsCallsCtx, .json = args->json, .args = args->args, .index = 0u);
    const unsigned int node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                         .json_read = args->json, .array = args->args, .index = 1u);
    const int forgets = (node == CLI_JSON_NONE) || (args->json->node[node].type == CFG_JSON_NULL);
    const char *const secret = forgets ? NULL
                                       : EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                                    .json_read = args->json, .node = node, .length_pointer = NULL);
    if (key == NULL)
    {
        EMBED_CALL(calls_refused, ScriptsCallsCtx, .answer = args->answer, .why = "the key is no string");
        return;
    }
    if (!forgets && (secret == NULL))
    {
        EMBED_CALL(calls_refused, ScriptsCallsCtx, .answer = args->answer, .why = "the secret is no string or null");
        return;
    }
    char service[CLI_NAME_MAX_LEN * 2u];
    EMBED_CALL(calls_secret_service, ScriptsCallsCtx, .id = args->id, .path_out = service);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const int done = forgets ? EMBED_CALL(cli_err_msg_service_internal.keychain_forget, CliErrMsgServiceCtx,
                                          .service = service, .account = key, .err_msg = &err_msg)
                             : EMBED_CALL(cli_err_msg_service_internal.keychain_store, CliErrMsgServiceCtx,
                                          .service = service, .account = key, .secret = secret, .err_msg = &err_msg);
    (void)mmgr_line_finish(&err_msg);
    if (!done)
    {
        EMBED_CALL(calls_refused, ScriptsCallsCtx, .answer = args->answer, .why = why);
        return;
    }
    mmgr_line_put(args->answer, "null");
}

// the JSON of the place `index` of the call's args written on `out`, null where the call has no such place
EMBED_INLINE void calls_json(const ScriptsCallsCtx *args)
{
    const unsigned int node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                         .json_read = args->json, .array = args->args, .index = args->index);
    if (node == CLI_JSON_NONE)
    {
        mmgr_line_put(args->answer, "null");
        return;
    }
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = args->json, .node = node,
               .out = args->answer, .indent = 0u, .sorted = 0);
}

// A JSON argument of the call written into a cell of the call's own, with its bytes into `size`. The cell takes twice
// the `size` bytes of the tree's texts, for the escapes that the writing adds. The cell, or NULL where no cell is free
// or the writing does not fit. The caller gives the cell back
static char *calls_json_alloc(const ScriptsCallsCtx *args, unsigned long long *size)
{
    const unsigned long long capacity = (args->size * 2ULL) + 64ULL;
    char *const text = g_platform_guard->temporary_buf_alloc(capacity, g_cli_scripts_owner);
    VerbaLinea written = mmgr_line_over(text, (text != NULL) ? capacity : 0ULL);
    EMBED_CALL(calls_json, ScriptsCallsCtx, .json = args->json, .args = args->args, .index = args->index,
               .answer = &written);
    *size = mmgr_line_ok(&written) ? written.at : 0ULL;
    if ((text != NULL) && !mmgr_line_ok(&written))
    {
        g_platform_guard->persistent_buf_release(text, g_cli_scripts_owner);
        return NULL;
    }
    return text;
}

// server.start(options, words): the plugin's server started at the tree's top, with the JSON `options` as its first
// options. The strings of `words` come before the words of its plugin.json. Its messages go to the plugin's host
static void calls_server_start(const ScriptsCallsCtx *args)
{
    unsigned long long length = 0ULL;
    char *const options = calls_json_alloc(
        &(ScriptsCallsCtx){.json = args->json, .args = args->args, .index = 0u, .size = args->json->used}, &length);
    const unsigned int words = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = args->json, .array = args->args, .index = 1u);
    const char *first[CLI_SCRIPTS_SERVER_WORDS];
    unsigned int firsts = 0u;
    while ((firsts < CLI_SCRIPTS_SERVER_WORDS) &&
           ((first[firsts] =
                 EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                            .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                               .json_read = args->json, .array = words, .index = firsts),
                            .length_pointer = NULL)) != NULL))
    {
        firsts += 1u;
    }
    CliServersSink sink = cli_servers_problems_sink(&(CliServersProblemsCfg){.root = s_rooted ? s_root : NULL});
    sink.emit = cli_scripts_server_emit;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const int started = s_rooted && (options != NULL) &&
                        EMBED_CALL(cli_servers_plugin_start_table.plugin_start, CliServersRootSinkCfg, .root = s_root,
                                   .sink = &sink, .err_msg = &err_msg, .id = args->id, .options = options,
                                   .length = length, .first_characters = first, .firsts = firsts);
    g_platform_guard->persistent_buf_release(options, g_cli_scripts_owner);
    (void)mmgr_line_finish(&err_msg);
    if (started)
    {
        mmgr_line_put(args->answer, "null");
        return;
    }
    EMBED_CALL(calls_refused, ScriptsCallsCtx, .answer = args->answer, .why = !s_rooted ? "no tree is open" : why);
}

// server.stop(): the plugin's server stopped, where it runs
static void calls_server_stop(const ScriptsCallsCtx *args)
{
    EMBED_CALL(cli_servers_root_sink.plugin_stop, CliServersRootSinkCfg, .id = args->id);
    mmgr_line_put(args->answer, "null");
}

// server.request(method, params) and server.notify(method, params): a request to the plugin's server, its result the
// call's answer, or a notification, null after
EMBED_INLINE void calls_server_sent(const ScriptsCallsCtx *args)
{
    const char *const method =
        EMBED_CALL(calls_string, ScriptsCallsCtx, .json = args->json, .args = args->args, .index = 0u);
    unsigned long long length = 0ULL;
    char *const params = calls_json_alloc(
        &(ScriptsCallsCtx){.json = args->json, .args = args->args, .index = 1u, .size = args->json->used}, &length);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const unsigned long long before = args->answer->at;
    const int sent =
        (method != NULL) && (params != NULL) &&
        (args->asks_answer ? EMBED_CALL(cli_servers_err_msg.plugin_request, CliServersErrMsgCfg, .err_msg = &err_msg,
                                        .id = args->id, .length = length, .method = method, .params = params,
                                        .wait = CLI_SCRIPTS_SERVER_WAIT_MS, .result = args->answer)
                           : EMBED_CALL(cli_servers_err_msg.plugin_notify, CliServersErrMsgCfg, .err_msg = &err_msg,
                                        .id = args->id, .length = length, .method = method, .params = params));
    g_platform_guard->persistent_buf_release(params, g_cli_scripts_owner);
    (void)mmgr_line_finish(&err_msg);
    if (sent && args->asks_answer && (args->answer->at == before))
    {
        mmgr_line_put(args->answer, "null");
    }
    else if (sent && !args->asks_answer)
    {
        mmgr_line_put(args->answer, "null");
    }
    else if (!sent)
    {
        args->answer->at = before;
        EMBED_CALL(calls_refused, ScriptsCallsCtx, .answer = args->answer,
                   .why = (method == NULL) ? "the call names no method" : why);
    }
}

static void calls_server_request(const ScriptsCallsCtx *args)
{
    EMBED_CALL(calls_server_sent, ScriptsCallsCtx, .id = args->id, .json = args->json, .args = args->args,
               .answer = args->answer, .asks_answer = 1);
}

static void calls_server_notify(const ScriptsCallsCtx *args)
{
    EMBED_CALL(calls_server_sent, ScriptsCallsCtx, .id = args->id, .json = args->json, .args = args->args,
               .answer = args->answer, .asks_answer = 0);
}

// One entry of the calls: its name, the right that it needs, and what it does. An entry that waits on a server runs
// on a thread of its own, and its answer goes to the host later. Last, whether the first argument is what the right's
// list must name. An entry of a right with no such argument holds itself to the list
typedef struct
{
    const char *name;
    unsigned int right;
    void (*run)(const ScriptsCallsCtx *args);
    int later;
    int targeted;
} ScriptsCallEntry;

static const ScriptsCallEntry s_calls[] = {
    {"files.read", CLI_SCRIPTS_READS, calls_files_read, 0, 1},
    {"files.write", CLI_SCRIPTS_WRITES, calls_files_write, 0, 1},
    {"server.start", CLI_SCRIPTS_NO_RIGHT, calls_server_start, 1, 0},
    {"server.stop", CLI_SCRIPTS_NO_RIGHT, calls_server_stop, 0, 0},
    {"server.request", CLI_SCRIPTS_NO_RIGHT, calls_server_request, 1, 0},
    {"server.notify", CLI_SCRIPTS_NO_RIGHT, calls_server_notify, 0, 0},
    {"tree.files", CLI_SCRIPTS_READS, calls_tree_files, 1, 0},
    {"tree.relative", CLI_SCRIPTS_NO_RIGHT, calls_tree_relative, 0, 0},
    {"secrets.read", CLI_SCRIPTS_SECRETS, calls_secrets_read, 0, 1},
    {"secrets.write", CLI_SCRIPTS_SECRETS, calls_secrets_write, 0, 1},
};

// the call kept until the reader answers the right `right`: its params copied, the server kept, and the request left
// unanswered. 1, or 0 where no place or cell is free
EMBED_INLINE int calls_held(const ScriptsCallsCtx *args)
{
    const unsigned int right = args->right;
    char request[64];
    if ((args->size > CLI_SCRIPTS_HELD_PARAMS) || (args->lsp == NULL) ||
        !EMBED_CALL(cli_lsp.answer_later, CliLspCfg, .id = request, .capacity = sizeof(request)))
    {
        return 0;
    }
    char *const params = g_platform_guard->persistent_buf_alloc(args->size + 1ULL, g_cli_scripts_owner);
    if (params == NULL)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = params, .src = args->params, .bytes = (embed_word)args->size});
    params[args->size] = '\0';
    cli_gate_shut(&s_held_gate);
    unsigned int at = 0u;
    while ((at < CLI_SCRIPTS_HELD) && s_held[at].used)
    {
        at += 1u;
    }
    if (at < CLI_SCRIPTS_HELD)
    {
        s_held[at].used = 1;
        (void)cellul.copy(&(CatenaFinitaCfg){.dst = s_held[at].id, .src = args->id, .cap = CLI_NAME_MAX_LEN});
        s_held[at].right = right;
        s_held[at].lsp = args->lsp;
        (void)cellul.copy(&(CatenaFinitaCfg){.dst = s_held[at].request, .src = request, .cap = sizeof(request)});
        s_held[at].params = params;
        s_held[at].size = args->size;
        EMBED_CALL(cli_lsp_context.ref, CliLspContextCfg, .lsp = args->lsp);
    }
    cli_gate_opened(&s_held_gate);
    if (at == CLI_SCRIPTS_HELD)
    {
        g_platform_guard->persistent_buf_release(params, g_cli_scripts_owner);
        return 0;
    }
    return 1;
}

// One call that runs on a thread of its own: the thread, the call's plugin, and the server that waits on its answer.
// Then the request's id as JSON, and the call's params in the cell of the record
typedef struct
{
    CliThread thread;
    char id[CLI_NAME_MAX_LEN];
    CliLsp *lsp;
    char request[64];
    unsigned long long size;
    char params[];
} ScriptsLater;

static void calls_later_run(void *context);

// the call run on a thread of its own: its params copied, the server kept, and the request left unanswered. 1, or 0
// where the call cannot wait, and the caller runs it at once
EMBED_INLINE int calls_later(const ScriptsCallsCtx *args)
{
    char request[64];
    if ((args->lsp == NULL) || !EMBED_CALL(cli_lsp.answer_later, CliLspCfg, .id = request, .capacity = sizeof(request)))
    {
        return 0;
    }
    static const char s_no_room[] = "{\"oriorError\":\"no room for the call\"}";
    static const char s_no_thread[] = "{\"oriorError\":\"no thread for the call\"}";
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    ScriptsLater *const later = (ScriptsLater *)g_platform_guard->persistent_buf_alloc(
        sizeof(ScriptsLater) + args->size + 1ULL, g_cli_scripts_owner);
    if (later == NULL)
    {
        (void)EMBED_CALL(cli_lsp_answered_table.answered, CliLspAnsweredCfg, .lsp = args->lsp, .id = request,
                         .result = s_no_room, .size = sizeof(s_no_room) - 1u, .err_msg = &err_msg);
        return 1;
    }
    (void)cellul.copy(&(CatenaFinitaCfg){.dst = later->id, .src = args->id, .cap = CLI_NAME_MAX_LEN});
    (void)cellul.copy(&(CatenaFinitaCfg){.dst = later->request, .src = request, .cap = sizeof(later->request)});
    later->lsp = args->lsp;
    later->size = args->size;
    memor.cpy(&(MemoriaCfg){.dst = later->params, .src = args->params, .bytes = (embed_word)args->size});
    later->params[args->size] = '\0';
    later->thread = (CliThread){calls_later_run, later, 0u};
    EMBED_CALL(cli_lsp_context.ref, CliLspContextCfg, .lsp = args->lsp);
    if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &later->thread))
    {
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = args->lsp);
        g_platform_guard->persistent_buf_release(later, g_cli_scripts_owner);
        (void)EMBED_CALL(cli_lsp_answered_table.answered, CliLspAnsweredCfg, .lsp = args->lsp, .id = request,
                         .result = s_no_thread, .size = sizeof(s_no_thread) - 1u, .err_msg = &err_msg);
    }
    return 1;
}

// The call run, its JSON already read with its root at `root`: its entry found, its right checked, and the entry run.
// 1 where the call's answer is in `answer`, and 0 where the call waits on the reader
EMBED_INLINE int calls_run(const ScriptsCallsCtx *args)
{
    const unsigned int root = args->root;
    const char *const name =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = root, .name = "name"),
                   .length_pointer = NULL);
    const unsigned int call_args = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = args->json, .object = root, .name = "args");
    unsigned int at = 0u;
    const unsigned int count = (unsigned int)(sizeof(s_calls) / sizeof(s_calls[0]));
    while ((name != NULL) && (at < count) &&
           !cellul.eq(&(CatenaFinitaCfg){.src = name, .other = s_calls[at].name, .cap = CLI_NAME_MAX_LEN}))
    {
        at += 1u;
    }
    if ((name == NULL) || (at == count))
    {
        EMBED_CALL(calls_refused, ScriptsCallsCtx, .answer = args->answer, .why = "the window answers no such call");
        return 1;
    }
    const unsigned int right = s_calls[at].right;
    // the target of a right is the path, the host or the key of the call's first argument
    const char *const target =
        ((right == CLI_SCRIPTS_NO_RIGHT) || !s_calls[at].targeted)
            ? NULL
            : EMBED_CALL(calls_string, ScriptsCallsCtx, .json = args->json, .args = call_args, .index = 0u);
    if (right != CLI_SCRIPTS_NO_RIGHT)
    {
        const int named =
            !s_calls[at].targeted || ((target != NULL) && EMBED_CALL(cli_scripts_rights.named, CliScriptsRightsCfg,
                                                                     .id = args->id, .right = right, .name = target));
        const unsigned int answer =
            named ? EMBED_CALL(cli_scripts_rights.answer, CliScriptsRightsCfg, .id = args->id, .right = right)
                  : CLI_SCRIPTS_REFUSED;
        if (!named)
        {
            EMBED_CALL(calls_refused, ScriptsCallsCtx, .answer = args->answer,
                       .why = "the plugin's plugin.json does not list this file, host or key");
            return 1;
        }
        if (answer == CLI_SCRIPTS_REFUSED)
        {
            EMBED_CALL(calls_refused, ScriptsCallsCtx, .answer = args->answer, .why = "the reader refused this right");
            return 1;
        }
        if ((answer == CLI_SCRIPTS_UNASKED) && args->may_wait &&
            EMBED_CALL(calls_held, ScriptsCallsCtx, .id = args->id, .lsp = args->lsp, .params = args->params,
                       .size = args->size, .right = right))
        {
            return 0;
        }
        if (answer == CLI_SCRIPTS_UNASKED)
        {
            EMBED_CALL(calls_refused, ScriptsCallsCtx, .answer = args->answer,
                       .why = "the reader did not answer for this right");
            return 1;
        }
    }
    if (s_calls[at].later && args->may_wait &&
        EMBED_CALL(calls_later, ScriptsCallsCtx, .id = args->id, .lsp = args->lsp, .params = args->params,
                   .size = args->size))
    {
        return 0;
    }
    EMBED_CALL(s_calls[at].run, ScriptsCallsCtx, .id = args->id, .json = args->json, .args = call_args,
               .answer = args->answer);
    return 1;
}

// the call's params read into a tree and the call run: 1 where its answer is in `answer`, 0 where it waits
EMBED_INLINE int calls_read_run(const ScriptsCallsCtx *args)
{
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    const unsigned int root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg,
                                         .json = &json, .text = args->params, .size = args->size, .nodes_more = 0u,
                                         .text_more = 0ULL, .err_msg = &err_msg, .owner = g_cli_scripts_owner);
    if (root == CLI_JSON_NONE)
    {
        EMBED_CALL(calls_refused, ScriptsCallsCtx, .answer = args->answer, .why = "the call is no JSON");
        return 1;
    }
    const int answered =
        EMBED_CALL(calls_run, ScriptsCallsCtx, .id = args->id, .lsp = args->lsp, .params = args->params,
                   .size = args->size, .answer = args->answer, .json = &json, .root = root, .may_wait = args->may_wait);
    g_platform_guard->persistent_buf_release(json.node, mmgr_owner_of_buf(platform_pool_series(), json.node));
    return answered;
}

// the thread of one call: the call run, its answer sent to the server that waits on it, and the record given back
static void calls_later_run(void *context)
{
    ScriptsLater *const later = (ScriptsLater *)context;
    char *const answer_text = g_platform_guard->temporary_buf_alloc(CLI_LSP_ANSWERED, g_cli_scripts_owner);
    VerbaLinea answer = mmgr_line_over(answer_text, (answer_text != NULL) ? CLI_LSP_ANSWERED : 0ULL);
    (void)EMBED_CALL(calls_read_run, ScriptsCallsCtx, .id = later->id, .lsp = later->lsp, .params = later->params,
                     .size = later->size, .answer = &answer, .may_wait = 0);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    (void)EMBED_CALL(cli_lsp_answered_table.answered, CliLspAnsweredCfg, .lsp = later->lsp, .id = later->request,
                     .result = answer_text, .size = mmgr_line_ok(&answer) ? answer.at : 0ULL, .err_msg = &err_msg);
    EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = later->lsp);
    g_platform_guard->persistent_buf_release(answer_text, g_cli_scripts_owner);
    g_platform_guard->persistent_buf_release(later, g_cli_scripts_owner);
}

void cli_scripts_call(const CliScriptsCallCtx *args)
{
    (void)EMBED_CALL(calls_read_run, ScriptsCallsCtx, .id = args->id, .lsp = args->lsp, .params = args->params,
                     .size = args->size, .answer = args->answer, .may_wait = 1);
}

void cli_scripts_set_tree(const CliScriptsTreeCfg *args)
{
    s_rooted =
        (args->root != NULL) &&
        (cellul.copy(&(CatenaFinitaCfg){.dst = s_root, .src = args->root, .cap = CLI_PATH_MAX_LEN}) < CLI_PATH_MAX_LEN);
}

int cli_scripts_question(const CliScriptsQuestionCfg *args)
{
    cli_gate_shut(&s_held_gate);
    unsigned int at = 0u;
    while (!s_asking && (at < CLI_SCRIPTS_HELD) && !s_held[at].used)
    {
        at += 1u;
    }
    const int asked = !s_asking && (at < CLI_SCRIPTS_HELD);
    if (asked)
    {
        s_asking = 1;
        (void)cellul.copy(&(CatenaFinitaCfg){.dst = s_asking_id, .src = s_held[at].id, .cap = CLI_NAME_MAX_LEN});
        s_asking_right = s_held[at].right;
    }
    cli_gate_opened(&s_held_gate);
    if (!asked)
    {
        return 0;
    }
    static const char *const s_titles[CLI_SCRIPTS_RIGHTS] = {" reads files of the tree", " writes files of the tree",
                                                             " reaches hosts on the network",
                                                             " keeps secrets in the system's keychain"};
    static const char *const s_bodies[CLI_SCRIPTS_RIGHTS] = {
        "The plugin reads the tree's files that its plugin.json names in reads.",
        "The plugin writes the tree's files that its plugin.json names in writes.",
        "The plugin sends requests to the hosts that its plugin.json names in reaches.",
        "The plugin keeps the secrets that its plugin.json names in secrets, in the system's keychain."};
    mmgr_line_put(args->title, s_asking_id);
    mmgr_line_put(args->title, s_titles[s_asking_right]);
    mmgr_line_put(args->body, s_bodies[s_asking_right]);
    mmgr_line_put(args->body, " Until you allow it, each such call fails. Preferences can change the answer later.");
    return 1;
}

void cli_scripts_question_answered(const CliScriptsQuestionCfg *args)
{
    if (!s_asking)
    {
        return;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (args->answer != CLI_SCRIPTS_UNASKED)
    {
        (void)EMBED_CALL(cli_scripts_rights.answer_set, CliScriptsRightsCfg, .id = s_asking_id, .right = s_asking_right,
                         .answer = args->answer, .err_msg = &err_msg);
    }
    char *const answer_text = g_platform_guard->temporary_buf_alloc(CLI_LSP_ANSWERED, g_cli_scripts_owner);
    unsigned int at = 0u;
    while (at < CLI_SCRIPTS_HELD)
    {
        cli_gate_shut(&s_held_gate);
        const int ours =
            s_held[at].used && (s_held[at].right == s_asking_right) &&
            cellul.eq(&(CatenaFinitaCfg){.src = s_held[at].id, .other = s_asking_id, .cap = CLI_NAME_MAX_LEN});
        ScriptsHeld held = ours ? s_held[at] : (ScriptsHeld){0};
        s_held[at].used = ours ? 0 : s_held[at].used;
        cli_gate_opened(&s_held_gate);
        if (ours)
        {
            VerbaLinea answer = mmgr_line_over(answer_text, (answer_text != NULL) ? CLI_LSP_ANSWERED : 0ULL);
            (void)EMBED_CALL(calls_read_run, ScriptsCallsCtx, .id = held.id, .lsp = held.lsp, .params = held.params,
                             .size = held.size, .answer = &answer, .may_wait = 0);
            err_msg.at = 0ULL;
            (void)EMBED_CALL(cli_lsp_answered_table.answered, CliLspAnsweredCfg, .lsp = held.lsp, .id = held.request,
                             .result = answer_text, .size = mmgr_line_ok(&answer) ? answer.at : 0ULL,
                             .err_msg = &err_msg);
            EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = held.lsp);
            g_platform_guard->persistent_buf_release(held.params, g_cli_scripts_owner);
        }
        at += 1u;
    }
    g_platform_guard->persistent_buf_release(answer_text, g_cli_scripts_owner);
    s_asking = 0;
}

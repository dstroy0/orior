// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_start.c: a server found and started: the toolchain whose `server` serves a language; the server of a tree
// started where it is not running, at the tree's top, every server stopped where another tree is served; clangd started
// again, given back its files, where the build's database changed; one that ended started again and given back its
// files, unless it ended RESTARTS times of late, when it is left stopped and why is reported; and one that cannot be
// found or does not start not tried again until the tree changes. A server that checks its projects as a whole is given
// each project the tree contains; the options of a tree whose own code is disabled, or whose code runs, are added to
// its own
#include "../builds/compile_db.h"
#include "../command_line/callback_cfg.h"
#include "../toolchains/toolchains.h"
#include "servers_internal.h"

#include "../text/text.h"

CliServersState g_cli_servers = {0};

// the files a server that ended or is started again had open, to be given back to the one started after it
static CliKeyed s_reopen;

// the manifest read, under its own lock

typedef struct
{
    const CliJson *json;
    unsigned int object;
    const char *name;
    char *out;
    unsigned long long capacity;
    unsigned int tool_u32;
    unsigned int server;
    CliServerSpec *spec;
    const char *tool;
    const char *err_msg;
} ServersStartCtx;
// a string member of a manifest object copied into `out`, sized for `capacity`
EMBED_INLINE void start_string(const ServersStartCtx *args)
{
    const CliJson *json = args->json;
    unsigned int object = args->object;
    const char *name = args->name;
    unsigned long long size = 0ULL;
    const char *const text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = object, .name = name),
                   .length_pointer = &size);
    const unsigned long long copied_len = ((text != NULL) && (size < args->capacity)) ? size : 0ULL;
    memor.cpy(&(MemoriaCfg){.dst = args->out, .src = (text != NULL) ? text : "", .bytes = (size_t)copied_len});
    args->out[copied_len] = '\0';
}

// a JSON member of a manifest object written into `out`, sized for `capacity`, empty where it has none
EMBED_INLINE void start_json(const ServersStartCtx *args)
{
    const CliJson *json = args->json;
    unsigned int object = args->object;
    const char *name = args->name;
    char *out = args->out;
    const unsigned int member = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                           .object = object, .name = name);
    VerbaLinea line = {out, args->capacity, 0ULL};
    if (member != CLI_JSON_NONE)
    {
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = member,
                   .out = &line, .indent = 0u, .sorted = 0);
    }
    if (!mmgr_line_ok(&line))
    {
        line.at = 0ULL;
    }
    (void)mmgr_line_finish(&line);
}

// a server's spec read from the manifest's tool at `tool`
EMBED_INLINE void start_spec(const ServersStartCtx *args)
{
    const CliJson *json = args->json;
    memor.set(&(MemoriaCfg){.dst = args->spec, .val = 0, .bytes = sizeof(*args->spec)});
    EMBED_CALL(start_string, ServersStartCtx, .json = json, .object = args->tool_u32, .name = "id",
               .out = args->spec->tool, .capacity = sizeof(args->spec->tool));
    EMBED_CALL(start_string, ServersStartCtx, .json = json, .object = args->tool_u32, .name = "name",
               .out = args->spec->name, .capacity = sizeof(args->spec->name));
    EMBED_CALL(start_string, ServersStartCtx, .json = json, .object = args->server, .name = "program",
               .out = args->spec->program, .capacity = sizeof(args->spec->program));
    const unsigned int arguments = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = args->server, .name = "args");
    for (unsigned int at = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                      .node = arguments, .kind = CFG_JSON_ARRAY)
                               ? json->node[arguments].first
                               : CLI_JSON_NONE;
         (at != CLI_JSON_NONE) && (args->spec->args < CLI_SERVERS_ARGS); at = json->node[at].next)
    {
        unsigned long long size = 0ULL;
        const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = json, .node = at, .length_pointer = &size);
        if ((text != NULL) && (size < sizeof(args->spec->arg[0])))
        {
            memor.cpy(&(MemoriaCfg){.dst = args->spec->arg[args->spec->args], .src = text, .bytes = (size_t)size});
            args->spec->arg[args->spec->args][size] = '\0';
            args->spec->args += 1u;
        }
    }
    const unsigned int languages = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = args->server, .name = "languages");
    for (unsigned int at = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                      .node = languages, .kind = CFG_JSON_ARRAY)
                               ? json->node[languages].first
                               : CLI_JSON_NONE;
         (at != CLI_JSON_NONE) && (args->spec->languages < CLI_SERVERS_LANGUAGES); at = json->node[at].next)
    {
        unsigned long long size = 0ULL;
        const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = json, .node = at, .length_pointer = &size);
        if ((text != NULL) && (size < CLI_NAME_MAX_LEN))
        {
            memor.cpy(
                &(MemoriaCfg){.dst = args->spec->language[args->spec->languages], .src = text, .bytes = (size_t)size});
            args->spec->language[args->spec->languages][size] = '\0';
            args->spec->languages += 1u;
        }
    }
    const unsigned int ids = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                        .object = args->server, .name = "ids");
    for (unsigned int at = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                      .node = ids, .kind = CFG_JSON_OBJECT)
                               ? json->node[ids].first
                               : CLI_JSON_NONE;
         (at != CLI_JSON_NONE) && (args->spec->ids < CLI_SERVERS_LANGUAGES); at = json->node[at].next)
    {
        unsigned long long size = 0ULL;
        const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = json, .node = at, .length_pointer = &size);
        const char *const key = json->text + json->node[at].key;
        if ((text != NULL) && (size < CLI_NAME_MAX_LEN) &&
            (cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_POOL_BYTES}) < sizeof(args->spec->id_extension[0])))
        {
            memor.cpy(&(MemoriaCfg){.dst = args->spec->id_extension[args->spec->ids],
                                    .src = key,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_POOL_BYTES}) + 1u});
            memor.cpy(&(MemoriaCfg){.dst = args->spec->id_name[args->spec->ids], .src = text, .bytes = (size_t)size});
            args->spec->id_name[args->spec->ids][size] = '\0';
            args->spec->ids += 1u;
        }
    }
    EMBED_CALL(start_string, ServersStartCtx, .json = json, .object = args->server, .name = "setup",
               .out = args->spec->setup, .capacity = sizeof(args->spec->setup));
    EMBED_CALL(start_string, ServersStartCtx, .json = json, .object = args->server, .name = "projects",
               .out = args->spec->projects, .capacity = sizeof(args->spec->projects));
    EMBED_CALL(start_string, ServersStartCtx, .json = json, .object = args->server, .name = "checks",
               .out = args->spec->checks, .capacity = sizeof(args->spec->checks));
    EMBED_CALL(start_json, ServersStartCtx, .json = json, .object = args->server, .name = "blocked",
               .out = args->spec->blocked_args, .capacity = sizeof(args->spec->blocked_args));
    EMBED_CALL(start_json, ServersStartCtx, .json = json, .object = args->server, .name = "trusted",
               .out = args->spec->trusted, .capacity = sizeof(args->spec->trusted));
    args->spec->slices = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                    .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                       .json_read = json, .object = args->server, .name = "slices"),
                                    .kind = CFG_JSON_TRUE);
}

int cli_servers_spec_for(const CliServersLanguageCtx *args)
{
    CliJson manifest_json;
    const unsigned int manifest = EMBED_CALL(cli_toolchains_json_nodes_more.manifest, CliToolchainsJsonNodesMoreCfg,
                                             .json_out = &manifest_json, .nodes_more = 0u, .text_more = 0ULL);
    int found = 0;
    for (unsigned int tool = (manifest != CLI_JSON_NONE) ? manifest_json.node[manifest].first : CLI_JSON_NONE;
         !found && (tool != CLI_JSON_NONE); tool = manifest_json.node[tool].next)
    {
        const unsigned int server = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                               .json_read = &manifest_json, .object = tool, .name = "server");
        const unsigned int languages = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                  .json_read = &manifest_json, .object = server, .name = "languages");
        for (unsigned int at = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                          .json_read = &manifest_json, .node = languages, .kind = CFG_JSON_ARRAY)
                                   ? manifest_json.node[languages].first
                                   : CLI_JSON_NONE;
             !found && (at != CLI_JSON_NONE); at = manifest_json.node[at].next)
        {
            unsigned long long size = 0ULL;
            const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                                .json_read = &manifest_json, .node = at, .length_pointer = &size);
            found = (text != NULL) &&
                    (cellul.len(&(CatenaFinitaCfg){.src = args->language, .cap = CLI_POOL_BYTES}) == size) &&
                    (memor.cmp(&(MemoriaCfg){.src = text, .other = args->language, .bytes = (size_t)size}) == 0);
        }
        if (found)
        {
            EMBED_CALL(start_spec, ServersStartCtx, .json = &manifest_json, .tool_u32 = tool, .server = server,
                       .spec = args->spec);
        }
    }
    g_cli_guard->persistent_buf_release(manifest_json.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), manifest_json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&manifest_json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return found;
}

void cli_servers_id_get(const CliServersPathLanguageCtx *args)
{
    const char *const dot = EMBED_CALL(
        cli_text_byte.last, CliTextByteCfg, .text = args->path,
        .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}), .byte = '.');
    const char *const slash = EMBED_CALL(
        cli_text_byte.last, CliTextByteCfg, .text = args->path,
        .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}), .byte = '/');
    const char *const back = EMBED_CALL(
        cli_text_byte.last, CliTextByteCfg, .text = args->path,
        .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}), .byte = '\\');
    const int named = (dot != NULL) && ((slash == NULL) || (dot > slash)) && ((back == NULL) || (dot > back));
    char extension[32] = {0};
    if (named)
    {
        (void)EMBED_CALL(cli_text_out_capacity.lowered_into, CliTextOutCapacityCfg, .out_capacity = extension,
                         .capacity = sizeof(extension), .text = dot + 1);
    }
    VerbaLinea line = {args->id_out, CLI_NAME_MAX_LEN, 0ULL};
    for (unsigned int at = 0u; at < args->spec_read->ids; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){
                .src = args->spec_read->id_extension[at], .other = extension, .cap = CLI_POOL_BYTES}))
        {
            mmgr_line_put(&line, args->spec_read->id_name[at]);
            (void)mmgr_line_finish(&line);
            return;
        }
    }
    mmgr_line_put(&line, args->language);
    (void)mmgr_line_finish(&line);
}

CliLsp *cli_servers_owner_get(const CliServersPathLspCtx *args)
{
    const char *path = args->path;
    CliLsp *found = NULL;
    unsigned int at = 0u;
    while ((found == NULL) && (at < CLI_SERVERS_MAX))
    {
        CliServerRunning *const one = &g_cli_servers.running[at];
        if (one->used && EMBED_CALL(cli_lsp_path_out.has, CliLspPathOutCfg, .lsp = one->lsp, .path = path))
        {
            found = one->lsp;
            EMBED_CALL(cli_lsp_context.ref, CliLspContextCfg, .lsp = found);
        }
        at += 1u;
    }
    return found;
}

int cli_servers_slicing(const CliServersCtx *args)
{
    int slicing = 0;
    for (unsigned int at = 0u; at < CLI_SERVERS_MAX; at += 1u)
    {
        const CliServerRunning *const one = &g_cli_servers.running[at];
        slicing |= one->used && (one->lsp == args->server_read) && one->spec.slices;
    }
    return slicing;
}

// a file a server had open retained to be given back to the one started after it
static void start_reopen_store(const CliEachPathTextLengthCfg *args)
{
    const char *text = args->text;
    (void)EMBED_CALL(cli_keyed_key_adjacent.put, CliKeyedKeyAdjacentCfg, .keyed_out = &s_reopen, .key = args->path,
                     .adjacent_value = "", .text = text, .size_value = args->length);
}

// the running server of `tool`: its place, CLI_SERVERS_MAX where none runs
EMBED_INLINE unsigned int start_running(const ServersStartCtx *args)
{
    for (unsigned int at = 0u; at < CLI_SERVERS_MAX; at += 1u)
    {
        if (g_cli_servers.running[at].used &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = g_cli_servers.running[at].spec.tool, .other = args->tool, .cap = CLI_POOL_BYTES})))
        {
            return at;
        }
    }
    return CLI_SERVERS_MAX;
}

// why the toolchain `tool` failed, NULL where it did not
EMBED_INLINE const char *start_fail(const ServersStartCtx *args)
{
    for (unsigned int at = 0u; at < g_cli_servers.failed; at += 1u)
    {
        if (cellul.eq(
                &(CatenaFinitaCfg){.src = g_cli_servers.failed_tool[at], .other = args->tool, .cap = CLI_POOL_BYTES}))
        {
            return g_cli_servers.failed_msg[at];
        }
    }
    return NULL;
}

// why the toolchain `tool` failed recorded
EMBED_INLINE void start_failure(const ServersStartCtx *args)
{
    unsigned int at = 0u;
    while ((at < g_cli_servers.failed) &&
           (!cellul.eq(
               &(CatenaFinitaCfg){.src = g_cli_servers.failed_tool[at], .other = args->tool, .cap = CLI_POOL_BYTES})))
    {
        at += 1u;
    }
    if (at == CLI_SERVERS_FAILED)
    {
        return;
    }
    g_cli_servers.failed += (at == g_cli_servers.failed) ? 1u : 0u;
    VerbaLinea line = {g_cli_servers.failed_tool[at], CLI_NAME_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->tool);
    (void)mmgr_line_finish(&line);
    VerbaLinea why = {g_cli_servers.failed_msg[at], sizeof(g_cli_servers.failed_msg[at]), 0ULL};
    mmgr_line_put(&why, args->err_msg);
    (void)mmgr_line_finish(&why);
}

// whether the toolchain `tool`'s server, which ended, may be started again, its end counted
EMBED_INLINE int start_again(const ServersStartCtx *args)
{
    unsigned int at = 0u;
    while ((at < g_cli_servers.restarted) &&
           (!cellul.eq(
               &(CatenaFinitaCfg){.src = g_cli_servers.restart_tool[at], .other = args->tool, .cap = CLI_POOL_BYTES})))
    {
        at += 1u;
    }
    if (at == CLI_SERVERS_FAILED)
    {
        return 1;
    }
    if (at == g_cli_servers.restarted)
    {
        VerbaLinea line = {g_cli_servers.restart_tool[at], CLI_NAME_MAX_LEN, 0ULL};
        mmgr_line_put(&line, args->tool);
        (void)mmgr_line_finish(&line);
        EMBED_CALL(cli_container_restarts.restarts_init, CliContainerRestartsCfg,
                   .restarts = &g_cli_servers.restarts[at], .most = CLI_SERVERS_RESTARTS,
                   .within = CLI_SERVERS_RESTARTS_WITHIN);
        g_cli_servers.restarted += 1u;
    }
    return EMBED_CALL(cli_container_restarts_at.restarts_dy, CliContainerRestartsAtCfg,
                      .restarts = &g_cli_servers.restarts[at], .at = cli_clock_milliseconds());
}

CliLsp *cli_servers_server(const CliServersLanguageCtx *args)
{
    const char *root = args->root;
    CliServerSpec *spec = args->spec;
    VerbaLinea *err_msg = args->err_msg;
    if (!EMBED_CALL(cli_servers_language_internal.spec_for, CliServersLanguageCtx, .language = args->language,
                    .spec = spec))
    {
        return NULL;
    }
    if (!cellul.eq(&(CatenaFinitaCfg){.src = g_cli_servers.root, .other = root, .cap = CLI_POOL_BYTES}))
    {
        cli_servers_stop_all();
        VerbaLinea line = {g_cli_servers.root, CLI_PATH_MAX_LEN, 0ULL};
        mmgr_line_put(&line, root);
        (void)mmgr_line_finish(&line);
    }
    EMBED_CALL(cli_keyed_name.clear, CliKeyedNameCfg, .keyed_out = &s_reopen);
    EMBED_CALL(cli_keyed_name.begin, CliKeyedNameCfg, .keyed_out = &s_reopen, .name = "files to open again",
               .entries_max = 256u);
    const unsigned int at = EMBED_CALL(start_running, ServersStartCtx, .tool = spec->tool);
    static CliDatabase s_database;
    if ((at < CLI_SERVERS_MAX) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = spec->program, .other = "clangd", .cap = CLI_POOL_BYTES})) &&
        EMBED_CALL(cli_lsp_context.alive, CliLspContextCfg, .lsp = g_cli_servers.running[at].lsp) &&
        EMBED_CALL(cli_compile_db_root_database.refresh, CliCompileDbRootDatabaseCfg, .root = root,
                   .database = &s_database))
    {
        CliLsp *const old = g_cli_servers.running[at].lsp;
        g_cli_servers.running[at].used = 0;
        EMBED_CALL(cli_lsp_context.open_each, CliLspContextCfg, .lsp = old, .each = start_reopen_store,
                   .context = NULL);
        EMBED_CALL(cli_servers_path_lsp_internal.stop_later, CliServersPathLspCtx, .lsp = old);
    }
    else if (at < CLI_SERVERS_MAX)
    {
        CliLsp *const server = g_cli_servers.running[at].lsp;
        if (EMBED_CALL(cli_lsp_context.alive, CliLspContextCfg, .lsp = server))
        {
            EMBED_CALL(cli_lsp_context.ref, CliLspContextCfg, .lsp = server);
            return server;
        }
        g_cli_servers.running[at].used = 0;
        if (!EMBED_CALL(start_again, ServersStartCtx, .tool = spec->tool))
        {
            char why[CLI_LSP_COMPLAINT];
            VerbaLinea complaint = {why, sizeof(why), 0ULL};
            EMBED_CALL(cli_lsp_path_out.complaint, CliLspPathOutCfg, .lsp = server, .out = &complaint);
            mmgr_line_put(err_msg, spec->program);
            mmgr_line_put(err_msg, " ended ");
            mmgr_line_decimal(err_msg, CLI_SERVERS_RESTARTS + 1u, 1u);
            mmgr_line_put(err_msg, " times within ");
            mmgr_line_decimal(err_msg, CLI_SERVERS_RESTARTS_WITHIN / 1000u, 1u);
            mmgr_line_put(err_msg, " s and is left stopped");
            if (complaint.at > 0u)
            {
                mmgr_line_put(err_msg, ": ");
                mmgr_line_put_n(err_msg, why, complaint.at);
            }
            (void)mmgr_line_finish(err_msg);
            EMBED_CALL(start_failure, ServersStartCtx, .tool = spec->tool, .err_msg = err_msg->out);
            EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
            return NULL;
        }
        EMBED_CALL(cli_lsp_context.open_each, CliLspContextCfg, .lsp = server, .each = start_reopen_store,
                   .context = NULL);
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
    }
    const char *const failed = EMBED_CALL(start_fail, ServersStartCtx, .tool = spec->tool);
    if (failed != NULL)
    {
        mmgr_line_put(err_msg, failed);
        return NULL;
    }
    CliLsp *const server = EMBED_CALL(cli_servers_root_internal.launch, CliServersRootCtx, .root = root,
                                      .spec_read = spec, .sink = args->sink, .err_msg = err_msg);
    if (server == NULL)
    {
        (void)mmgr_line_finish(err_msg);
        EMBED_CALL(start_failure, ServersStartCtx, .tool = spec->tool, .err_msg = err_msg->out);
        return NULL;
    }
    for (unsigned int one = 0u; one < s_reopen.entries; one += 1u)
    {
        if (!EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = &s_reopen, .at = one)->used)
        {
            continue;
        }
        const char *const path = EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = &s_reopen, .at = one)->cell;
        const char *adjacent = NULL;
        unsigned long long size = 0ULL;
        const char *const text = EMBED_CALL(cli_keyed_key_adjacent.get, CliKeyedKeyAdjacentCfg, .keyed = &s_reopen,
                                            .key = path, .adjacent = &adjacent, .size = &size);
        char id[CLI_NAME_MAX_LEN];
        EMBED_CALL(cli_servers_path_language_internal.id_get, CliServersPathLanguageCtx, .spec_read = spec,
                   .path = path, .language = args->language, .id_out = id);
        char ignored[256];
        VerbaLinea quiet = {ignored, sizeof(ignored), 0ULL};
        (void)EMBED_CALL(cli_lsp_err_msg_text.open, CliLspErrMsgTextCfg, .lsp = server, .path = path, .language_id = id,
                         .text = text, .length = size, .err_msg = &quiet);
    }
    for (unsigned int index = 0u; index < CLI_SERVERS_MAX; index += 1u)
    {
        if (!g_cli_servers.running[index].used)
        {
            g_cli_servers.running[index].used = 1;
            g_cli_servers.running[index].lsp = server;
            g_cli_servers.running[index].spec = *spec;
            g_cli_servers.running[index].waits = 0u;
            g_cli_servers.running[index].pending_requests = 0ULL;
            g_cli_servers.running[index].quiet_from = cli_clock_milliseconds();
            break;
        }
    }
    EMBED_CALL(cli_lsp_context.ref, CliLspContextCfg, .lsp = server);
    return server;
}

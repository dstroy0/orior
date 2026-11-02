// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_actions.c: the quick fixes and refactorings for a span of a file: the inspections' and the checkers' fixes
// of the findings that touch it first, then what its server offers, then what each plugin server that has open it
// offers, each given the diagnostics it gave that touch it, a fix offered for a diagnostic and again for its note
// listed once, those that can be carried out first and the preferred fixes before the rest. An action of a plugin's
// server carries the plugin's id, so it is carried out by that server (servers_act.c). A server given the file as a
// slice is asked about the slice's lines, with the diagnostics on them
#include "../command_line/cells.h"
#include "servers_internal.h"

// what is read, under the reading's lock
static CliServersReading s_reading = {CLI_LOCK_INIT};

void cli_servers_member_write(const CliServersJsonNodeCtx *args)
{
    const unsigned int member = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                           .json_read = args->json, .object = args->node, .name = args->name);
    if (member == CLI_JSON_NONE)
    {
        mmgr_line_put(args->out, "null");
        return;
    }
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = args->json, .node = member,
               .out = args->out, .indent = 0u, .sorted = 0);
}

typedef struct
{
    CliKeyed *keyed;
    CliLock *lock;
    const char *key;
    unsigned int array;
    CliTextPlace from;
    CliTextPlace to;
    int *written;
    VerbaLinea *out;
    unsigned int all;
    const char *path;
    const char *uri;
    VerbaLinea *params;
    const CliJson *json;
    unsigned int one;
    const char *plugin;
    CliLsp *server;
    VerbaLinea *err_msg;
} ServersActionsCtx;
// the text stored under `key` in `keyed` read: its root, CLI_JSON_NONE where none is
EMBED_INLINE unsigned int actions_stored_read(const ServersActionsCtx *args)
{
    CliKeyed *keyed = args->keyed;
    CliLock *lock = args->lock;
    const char *key = args->key;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = lock);
    const char *beside = NULL;
    unsigned long long size = 0ull;
    const char *const text = EMBED_CALL(cli_keyed_key_beside.get, CliKeyedKeyBesideCfg, .keyed = keyed, .key = key,
                                        .beside = &beside, .size = &size);
    const unsigned int root = ((text != NULL) && (size > 0ull))
                                  ? EMBED_CALL(cli_servers_reading_internal.read, CliServersReadingCtx,
                                               .reading = &s_reading, .text = text, .size = size)
                                  : CLI_JSON_NONE;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = lock);
    return root;
}

// the fixes of each finding of `array` that touches `from` to `to`, written after those written
static void actions_ours(const ServersActionsCtx *args)
{
    CliTextPlace from = args->from;
    CliTextPlace to = args->to;
    int *written = args->written;
    VerbaLinea *out = args->out;
    const CliJson *const json = &s_reading.json;
    for (unsigned int one = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                       .node = args->array, .kind = CFG_JSON_ARRAY)
                                ? json->node[args->array].first
                                : CLI_JSON_NONE;
         one != CLI_JSON_NONE; one = json->node[one].next)
    {
        if (!EMBED_CALL(cli_servers_json_from_internal.overlaps, CliServersJsonFromCtx, .from = from, .to = to,
                        .json = json,
                        .range = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                            .object = one, .name = "range")))
        {
            continue;
        }
        const unsigned int fixes =
            EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                       .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                            .object = one, .name = "data"),
                       .name = "orior");
        for (unsigned int fix = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                           .node = fixes, .kind = CFG_JSON_ARRAY)
                                    ? json->node[fixes].first
                                    : CLI_JSON_NONE;
             fix != CLI_JSON_NONE; fix = json->node[fix].next)
        {
            unsigned long long size = 0ull;
            const char *const title =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = fix, .name = "title"),
                           .length_pointer = &size);
            mmgr_line_put(out, *written ? ",{\"title\":" : "{\"title\":");
            EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
                       .text = (title != NULL) ? title : "", .length = size);
            mmgr_line_put(out, ",\"kind\":\"quickfix\",\"preferred\":true,\"disabled\":null,\"raw\":");
            const unsigned int run = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                .json_read = json, .object = fix, .name = "run");
            if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = run,
                           .kind = CFG_JSON_OBJECT))
            {
                mmgr_line_put(out, "{\"orior_run\":");
                EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = run,
                           .out = out, .indent = 0u, .sorted = 0);
            }
            else
            {
                mmgr_line_put(out, "{\"orior\":");
                EMBED_CALL(cli_servers_json_node_internal.member_write, CliServersJsonNodeCtx, .json = json,
                           .node = fix, .name = "edits", .out = out);
            }
            mmgr_line_put(out, "}}");
            *written = 1;
        }
    }
}

// the params that request from the server of `uri` its actions on `from` to `to`, with its diagnostics that touch it,
// each on the slice's lines where the server was given the file at `path` as a slice, `path` empty where the server
// was given it whole, written into `params`: 1, or 0 where the slice holds no line of the span
EMBED_INLINE int actions_params(const ServersActionsCtx *args)
{
    const char *path = args->path;
    CliTextPlace from = args->from;
    CliTextPlace to = args->to;
    unsigned int first = from.line;
    unsigned int last = to.line;
    if (!EMBED_CALL(cli_servers_path_line_internal.slice_in, CliServersPathLineCtx, .path = path, .line = from.line,
                    .into = &first) ||
        !EMBED_CALL(cli_servers_path_line_internal.slice_in, CliServersPathLineCtx, .path = path, .line = to.line,
                    .into = &last))
    {
        return 0;
    }
    mmgr_line_put(args->params, "{\"textDocument\":{\"uri\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->params, .text = args->uri,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->uri, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(args->params, "},\"range\":{\"start\":{\"line\":");
    mmgr_line_decimal(args->params, first, 1u);
    mmgr_line_put(args->params, ",\"character\":");
    mmgr_line_decimal(args->params, from.col, 1u);
    mmgr_line_put(args->params, "},\"end\":{\"line\":");
    mmgr_line_decimal(args->params, last, 1u);
    mmgr_line_put(args->params, ",\"character\":");
    mmgr_line_decimal(args->params, to.col, 1u);
    mmgr_line_put(args->params, "}},\"context\":{\"diagnostics\":[");

    const CliJson *const json = &s_reading.json;
    int written = 0;
    for (unsigned int one = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                       .node = args->all, .kind = CFG_JSON_ARRAY)
                                ? json->node[args->all].first
                                : CLI_JSON_NONE;
         one != CLI_JSON_NONE; one = json->node[one].next)
    {
        // a diagnostic the slice holds neither line of is none the server gave
        const unsigned int range = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = one, .name = "range");
        unsigned int slice_line = 0u;
        if (EMBED_CALL(cli_servers_json_from_internal.overlaps, CliServersJsonFromCtx, .from = from, .to = to,
                       .json = json, .range = range) &&
            EMBED_CALL(cli_servers_path_line_internal.slice_in, CliServersPathLineCtx, .path = path,
                       .line = EMBED_CALL(cli_servers_json_node_otherwise_internal.place,
                                          CliServersJsonNodeOtherwiseCtx, .json = json,
                                          .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                             .json_read = json, .object = range, .name = "start"))
                                   .line,
                       .into = &slice_line) &&
            EMBED_CALL(cli_servers_path_line_internal.slice_in, CliServersPathLineCtx, .path = path,
                       .line = EMBED_CALL(cli_servers_json_node_otherwise_internal.place,
                                          CliServersJsonNodeOtherwiseCtx, .json = json,
                                          .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                             .json_read = json, .object = range, .name = "end"))
                                   .line,
                       .into = &slice_line))
        {
            mmgr_line_put(args->params, written ? "," : "");
            (void)EMBED_CALL(cli_servers_path_json_internal.slice_ranged_in, CliServersPathJsonCtx, .path = path,
                             .json = json, .one = one, .out = args->params);
            written = 1;
        }
    }
    mmgr_line_put(args->params, "],\"triggerKind\":1}}");
    return 1;
}

// the order an action goes in: those that can be carried out first, then the preferred, then the quick fixes
EMBED_INLINE unsigned int actions_rank(const ServersActionsCtx *args)
{
    const CliJson *json = args->json;
    unsigned long long size = 0ull;
    const char *const kind =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->one, .name = "kind"),
                   .length_pointer = &size);
    const int disabled =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                           .json_read = json, .object = args->one, .name = "disabled"),
                                      .name = "reason"),
                   .length_pointer = NULL) != NULL;
    const int preferred = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                     .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                        .json_read = json, .object = args->one, .name = "isPreferred"),
                                     .kind = CFG_JSON_TRUE);
    const int quickfix = (kind != NULL) && EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = kind,
                                                      .size = size, .prefix = "quickfix");
    return (disabled ? 4u : 0u) + (preferred ? 0u : 2u) + (quickfix ? 0u : 1u);
}

// the server's actions written after those written, each distinguished by its title and its edit once; the actions
// retained in a cell of one place for each the server offers, none written where it cannot be taken
EMBED_INLINE void actions_offer(const ServersActionsCtx *args)
{
    const char *plugin = args->plugin;
    int *written = args->written;
    VerbaLinea *out = args->out;
    const CliJson *const json = &s_reading.json;
    const unsigned int offered = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                            .node = args->all, .kind = CFG_JSON_ARRAY)
                                     ? json->node[args->all].count
                                     : 0u;
    unsigned int *const action =
        (offered > 0u)
            ? g_cli_cells->temporary_buf_alloc((unsigned long long)offered * sizeof(unsigned int), &g_cli_servers_cells)
            : NULL;
    unsigned int count = 0u;
    for (unsigned int one = (action != NULL) ? json->node[args->all].first : CLI_JSON_NONE; one != CLI_JSON_NONE;
         one = json->node[one].next)
    {
        const unsigned int title = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = one, .name = "title");
        if (EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = title, .length_pointer = NULL) == NULL)
        {
            continue;
        }
        const unsigned int edit = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                             .object = one, .name = "edit");
        int seen = 0;
        for (unsigned int before = 0u; !seen && (before < count); before += 1u)
        {
            seen = EMBED_CALL(cli_json_tree_one.json_same, CliJsonTreeOneCfg, .json_read = json, .one = title,
                              .other_json = json,
                              .other = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                  .json_read = json, .object = action[before], .name = "title")) &&
                   EMBED_CALL(cli_json_tree_one.json_same, CliJsonTreeOneCfg, .json_read = json, .one = edit,
                              .other_json = json,
                              .other = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                  .json_read = json, .object = action[before], .name = "edit"));
        }
        if (!seen)
        {
            action[count] = one;
            count += 1u;
        }
    }
    for (unsigned int rank = 0u; rank < 8u; rank += 1u)
    {
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            if (EMBED_CALL(actions_rank, ServersActionsCtx, .json = json, .one = action[at]) == rank)
            {
                mmgr_line_put(out, *written ? "," : "");
                (void)EMBED_CALL(cli_servers_json_internal.action_get, CliServersJsonCtx, .json = json,
                                 .one = action[at], .plugin = plugin, .out = out);
                *written = 1;
            }
        }
    }
    g_cli_cells->persistent_buf_release(action, &g_cli_servers_cells);
}

// `server`, the server of the plugin `plugin` where that is not NULL, queried for its actions on `from` to `to` of the
// file at `path`, of `uri`, with its diagnostics `all` that touch it, and what it offers written after those written,
// none where its own server was given the file as a slice that holds no line of the span: 1, or 0 with why given
EMBED_INLINE int actions_request(const ServersActionsCtx *args)
{
    const char *path = args->path;
    VerbaLinea *err_msg = args->err_msg;
    // a span the slice the server was given does not hold is one it offers nothing on
    unsigned int slice_line = 0u;
    if ((args->plugin == NULL) && (!EMBED_CALL(cli_servers_path_line_internal.slice_in, CliServersPathLineCtx,
                                               .path = path, .line = args->from.line, .into = &slice_line) ||
                                   !EMBED_CALL(cli_servers_path_line_internal.slice_in, CliServersPathLineCtx,
                                               .path = path, .line = args->to.line, .into = &slice_line)))
    {
        return 1;
    }
    // the diagnostics written with the span come in a server's answer, and so fit what an answer holds
    char *const params_text = g_cli_cells->temporary_buf_alloc(CLI_SERVERS_ANSWER, &g_cli_servers_cells);
    VerbaLinea params = mmgr_line_over(params_text, CLI_SERVERS_ANSWER);
    const int whole =
        EMBED_CALL(actions_params, ServersActionsCtx, .all = args->all, .path = (args->plugin == NULL) ? path : "",
                   .uri = args->uri, .from = args->from, .to = args->to, .params = &params) &&
        mmgr_line_ok(&params);
    if (!whole)
    {
        mmgr_line_put(err_msg, "the request for actions could not be written: its params do not fit");
    }
    CliLspAnswer answer = {NULL, NULL, 0ull};
    const int requested =
        whole && EMBED_CALL(cli_lsp_err_msg_length.request, CliLspErrMsgLengthCfg, .lsp = args->server,
                            .method = "textDocument/codeAction", .params = params.out, .length = params.at,
                            .wait = CLI_SERVERS_REQUEST_TIMEOUT_MS, .answer = &answer, .err_msg = err_msg);
    g_cli_cells->persistent_buf_release(params_text, &g_cli_servers_cells);
    if (requested)
    {
        EMBED_CALL(actions_offer, ServersActionsCtx,
                   .all = EMBED_CALL(cli_servers_reading_answer_internal.answer_read, CliServersReadingAnswerCtx,
                                     .reading = &s_reading, .answer = &answer),
                   .plugin = args->plugin, .written = args->written, .out = args->out);
    }
    return requested;
}

int cli_servers_actions(const CliServersPathOutErrMsgFromCfg *args)
{
    char uri[CLI_PATH_MAX_LEN];
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_servers_path_uri_internal.uri_get, CliServersPathUriCtx, .path = args->path, .uri_out = uri);
    EMBED_CALL(cli_lsp_length.key_get, CliLspLengthCfg, .uri = uri,
               .length = cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), .key = key);
    EMBED_CALL(cli_servers_reading_answer_internal.reading_lock, CliServersReadingAnswerCtx, .reading = &s_reading);
    const unsigned long long start = args->out->at;
    mmgr_line_ch(args->out, '[');
    int written = 0;
    EMBED_CALL(actions_ours, ServersActionsCtx,
               .array = EMBED_CALL(actions_stored_read, ServersActionsCtx, .keyed = &g_cli_servers_inspected,
                                   .lock = &g_cli_servers_checks_lock, .key = key),
               .from = args->from, .to = args->to, .written = &written, .out = args->out);
    const unsigned int by_checker = EMBED_CALL(actions_stored_read, ServersActionsCtx, .keyed = &g_cli_servers_checked,
                                               .lock = &g_cli_servers_checks_lock, .key = key);
    const CliJson *const json = &s_reading.json;
    for (unsigned int id = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                      .node = by_checker, .kind = CFG_JSON_OBJECT)
                               ? json->node[by_checker].first
                               : CLI_JSON_NONE;
         id != CLI_JSON_NONE; id = json->node[id].next)
    {
        EMBED_CALL(actions_ours, ServersActionsCtx, .array = id, .from = args->from, .to = args->to,
                   .written = &written, .out = args->out);
    }
    // the tree's own server queried first, its refusal reported only where no other offers anything
    CliLsp *const server =
        EMBED_CALL(cli_servers_path_lsp_internal.owner_start, CliServersPathLspCtx, .path = args->path);
    int requested = 1;
    char refused[256];
    VerbaLinea refusal = {refused, sizeof(refused), 0ull};
    if (server != NULL)
    {
        const unsigned int all = EMBED_CALL(actions_stored_read, ServersActionsCtx, .keyed = &g_cli_servers_published,
                                            .lock = &g_cli_servers_published_lock, .key = key);
        requested = EMBED_CALL(actions_request, ServersActionsCtx, .server = server, .plugin = NULL, .all = all,
                               .path = args->path, .uri = uri, .from = args->from, .to = args->to, .written = &written,
                               .out = args->out, .err_msg = &refusal);
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
    }
    // each plugin server that has the file open queried as well, given the diagnostics it gave, a server that does not
    // answer offering nothing
    int plugged = 0;
    char plugin[CLI_NAME_MAX_LEN];
    unsigned int place = 0u;
    for (CliLsp *server_at = EMBED_CALL(cli_servers_path_id_internal.plugin_owner_get, CliServersPathIdCtx,
                                        .at_pointer = &place, .path = args->path, .id_out = plugin);
         server_at != NULL; server_at = EMBED_CALL(cli_servers_path_id_internal.plugin_owner_get, CliServersPathIdCtx,
                                                   .at_pointer = &place, .path = args->path, .id_out = plugin))
    {
        char why[256];
        VerbaLinea quiet = {why, sizeof(why), 0ull};
        (void)EMBED_CALL(actions_request, ServersActionsCtx, .server = server_at, .plugin = plugin,
                         .all = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                           .object = by_checker, .name = plugin),
                         .path = args->path, .uri = uri, .from = args->from, .to = args->to, .written = &written,
                         .out = args->out, .err_msg = &quiet);
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server_at);
        plugged = 1;
    }
    if ((server == NULL) && !written && !plugged)
    {
        mmgr_line_put(args->err_msg, "no language server has this file open");
        requested = 0;
    }
    if (!requested && (written || plugged))
    {
        requested = 1;
    }
    else if (!requested)
    {
        mmgr_line_put_n(args->err_msg, refused, mmgr_line_ok(&refusal) ? refusal.at : sizeof(refused));
    }
    mmgr_line_ch(args->out, ']');
    args->out->at = requested ? args->out->at : start;
    EMBED_CALL(cli_servers_reading_answer_internal.reading_release, CliServersReadingAnswerCtx, .reading = &s_reading);
    return requested;
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_hints.c: what a server can do, as its capabilities name it; the spans Expand Selection steps through, the
// tree's, read off the editor's text by the roles its grammar names (symbols_spans.c); and the hints a server writes in
// the text and not into it, each its place, its text, its kind, 1 a type, 2 a parameter's name and 0 any other, and
// whether a space stands before it and after it. A file given its server as slices is asked of the tree alone: its
// hints are the names of the parameters its calls' arguments are given to (symbols_hints.c)
#include "../code/symbols.h"
#include "../command_line/callback_cfg.h"
#include "../editor/document.h"
#include "servers_internal.h"

// a server's capabilities read into a tree taken to their size, under their lock
static CliJson s_capable;
static CliLock s_capable_lock = CLI_LOCK_INIT;

// what the server answers read, under the reading's lock
static CliServersReading s_reading = {CLI_LOCK_INIT};

typedef struct
{
    CliLsp *server;
    const char *name;
    const char *uri;
    VerbaLinea *params;
    const CliJson *json;
    unsigned int label;
    VerbaLinea *out;
    const char *path;
    unsigned int from;
    unsigned int to;
    VerbaLinea *err_msg;
} ServersHintsCtx;
// the capability `name` of the server read, with the capabilities' lock taken: its node
EMBED_INLINE unsigned int hints_capability(const ServersHintsCtx *args)
{
    const char *name = args->name;
    unsigned long long length = 0ull;
    const char *const capabilities =
        EMBED_CALL(cli_lsp_size_kind.capabilities, CliLspSizeKindCfg, .lsp_read = args->server, .size = &length);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_capable_lock);
    char quiet[256];
    VerbaLinea err_msg = {quiet, sizeof(quiet), 0ull};
    const unsigned int root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg,
                                         .json = &s_capable, .text = capabilities, .size = length, .nodes_more = 0u,
                                         .text_more = 0ull, .err_msg = &err_msg, .owner = &g_cli_servers_cells);
    return EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_capable, .object = root,
                      .name = name);
}

// the capabilities' tree given back with their lock
static void hints_capability_free(void)
{
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &s_capable);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_capable_lock);
}

int cli_servers_capable(const CliServersServerCtx *args)
{
    const unsigned int member =
        EMBED_CALL(hints_capability, ServersHintsCtx, .server = args->server, .name = args->name);
    const int capable = (member != CLI_JSON_NONE) &&
                        !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_capable,
                                    .node = member, .kind = CFG_JSON_NULL) &&
                        !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_capable,
                                    .node = member, .kind = CFG_JSON_FALSE);
    hints_capability_free();
    return capable;
}

int cli_servers_capable_true(const CliServersServerCtx *args)
{
    const unsigned int member =
        EMBED_CALL(hints_capability, ServersHintsCtx, .server = args->server, .name = args->name);
    const int capable = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_capable,
                                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                      .json_read = &s_capable, .object = member, .name = args->inner),
                                   .kind = CFG_JSON_TRUE);
    hints_capability_free();
    return capable;
}

int cli_servers_capable_command(const CliServersServerCommandCtx *args)
{
    const unsigned int member =
        EMBED_CALL(hints_capability, ServersHintsCtx, .server = args->server, .name = "executeCommandProvider");
    const unsigned int commands = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                             .json_read = &s_capable, .object = member, .name = "commands");
    int listed = 0;
    for (unsigned int one = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_capable,
                                       .node = commands, .kind = CFG_JSON_ARRAY)
                                ? s_capable.node[commands].first
                                : CLI_JSON_NONE;
         !listed && (one != CLI_JSON_NONE); one = s_capable.node[one].next)
    {
        const char *const name = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = &s_capable, .node = one, .length_pointer = NULL);
        listed =
            (name != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = name, .other = args->command, .cap = CLI_CELLS_POOL});
    }
    hints_capability_free();
    return listed;
}

// the params of a request about the file `uri` begun, its object left open
EMBED_INLINE void hints_params_begin(const ServersHintsCtx *args)
{
    mmgr_line_put(args->params, "{\"textDocument\":{\"uri\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->params, .text = args->uri,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->uri, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(args->params, '}');
}

// The tree's spans being written: where, and whether one was
typedef struct
{
    VerbaLinea *out;
    int written;
} CliServersSpanned;

// a span the tree gives written as `[from, to]`, after the others
static void hints_span_write(const CliSpannedCfg *args)
{
    CliServersSpanned *const spanned = (CliServersSpanned *)args->context;
    mmgr_line_put(spanned->out, spanned->written ? ",[" : "[");
    EMBED_CALL(cli_edits_place.text_place_write, CliEditsPlaceCfg,
               .place = (CliTextPlace){args->from.line, args->from.col}, .out = spanned->out);
    mmgr_line_ch(spanned->out, ',');
    EMBED_CALL(cli_edits_place.text_place_write, CliEditsPlaceCfg, .place = (CliTextPlace){args->to.line, args->to.col},
               .out = spanned->out);
    mmgr_line_ch(spanned->out, ']');
    spanned->written = 1;
}

int cli_servers_selection(const CliServersPathErrMsgOutCtx *args)
{
    const char *path = args->path;
    unsigned long long size = 0ull;
    char *const branch = (args->key != NULL)
                             ? EMBED_CALL(cli_document_key.text_take, CliDocumentKeyCfg, .key = args->key,
                                          .size_pointer = &size, .owner = &g_cli_servers_cells)
                             : NULL;
    char *const text = (branch != NULL) ? branch
                                        : EMBED_CALL(cli_servers_path_size_kind.text_take, CliServersPathSizeKindCfg,
                                                     .path = path, .size_pointer = &size);
    CliServersSpanned spanned = {args->out, 0};
    mmgr_line_ch(args->out, '[');
    const int done = (text == NULL) ||
                     EMBED_CALL(cli_symbols_path_text_size_context_err_msg_from_symbol_place.spans,
                                CliSymbolsPathTextSizeContextErrMsgFromSymbolPlaceCfg, .path = path, .text = text,
                                .size = size, .from_symbol_place = (CliSymbolPlace){args->from.line, args->from.col},
                                .to_symbol_place = (CliSymbolPlace){args->to.line, args->to.col},
                                .each_spanned = hints_span_write, .context = &spanned, .err_msg = args->err_msg);
    mmgr_line_ch(args->out, ']');
    g_cli_cells->persistent_buf_release(text, &g_cli_servers_cells);
    return done;
}

// a hint's label written as a JSON string, trimmed: its text, or the values of its parts joined; 0 where it is empty
EMBED_INLINE int hints_label_write(const ServersHintsCtx *args)
{
    const CliJson *json = args->json;
    VerbaLinea *out = args->out;
    static char s_label[1u << 16u];
    VerbaLinea joined = {s_label, sizeof(s_label), 0ull};
    unsigned long long size = 0ull;
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                        .json_read = json, .node = args->label, .length_pointer = &size);
    if (text != NULL)
    {
        mmgr_line_put_n(&joined, text, size);
    }
    for (unsigned int part = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                        .node = args->label, .kind = CFG_JSON_ARRAY)
                                 ? json->node[args->label].first
                                 : CLI_JSON_NONE;
         part != CLI_JSON_NONE; part = json->node[part].next)
    {
        const char *const value =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = part, .name = "value"),
                       .length_pointer = &size);
        if (value != NULL)
        {
            mmgr_line_put_n(&joined, value, size);
        }
    }
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = s_label,
               .size = mmgr_line_ok(&joined) ? joined.at : 0ull, .from_u64s = &from, .to_u64s = &to);
    if (to == from)
    {
        return 0;
    }
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = s_label + from,
               .length = to - from);
    return 1;
}

// The tree's hints being written: where, and whether one was
typedef struct
{
    VerbaLinea *out;
    int written;
} CliServersHinted;

// a parameter's name the tree gives an argument written as a hint of a parameter's name, a space after it
static void hints_tree_write(const CliHintedCfg *args)
{
    CliServersHinted *const hinted = (CliServersHinted *)args->context;
    mmgr_line_put(hinted->out, hinted->written ? ",{\"line\":" : "{\"line\":");
    mmgr_line_decimal(hinted->out, args->line, 1u);
    mmgr_line_put(hinted->out, ",\"col\":");
    mmgr_line_decimal(hinted->out, args->col, 1u);
    mmgr_line_put(hinted->out, ",\"label\":\"");
    // a parameter's name is a name, plain letters a JSON string holds as they are
    mmgr_line_put_n(hinted->out, args->name, (size_t)args->length);
    mmgr_line_put(hinted->out, ":\",\"kind\":2,\"left\":false,\"right\":true}");
    hinted->written = 1;
}

// the hints on the lines `from` to `to` of a file given its server as a slice, from the tree: 1, or 0 with why given
EMBED_INLINE int hints_of_tree(const ServersHintsCtx *args)
{
    const char *path = args->path;
    unsigned int from = args->from;
    unsigned int to = args->to;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char tree[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_servers.lock);
    memor.cpy(&(MemoriaCfg){.dst = tree, .src = g_cli_servers.checked_root, .bytes = sizeof(tree)});
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_servers.lock);
    char own[CLI_PATH_MAX_LEN];
    VerbaLinea own_line = {own, sizeof(own), 0ull};
    EMBED_CALL(cli_servers_path_err_msg_internal.tree_path, CliServersPathErrMsgCtx, .tree = tree, .path = path,
               .out = &own_line);
    unsigned long long size = 0ull;
    char *const text = (mmgr_line_finish(&own_line) != 0ull)
                           ? EMBED_CALL(cli_servers_path_size_kind.text_take, CliServersPathSizeKindCfg, .path = path,
                                        .size_pointer = &size)
                           : NULL;
    CliServersHinted hinted = {out, 0};
    mmgr_line_ch(out, '[');
    const int done =
        (text == NULL) || EMBED_CALL(cli_symbols_path_text_size_context.hints, CliSymbolsPathTextSizeContextCfg,
                                     .path = own, .text = text, .size = size, .from = from, .to = to,
                                     .each_hinted = hints_tree_write, .context = &hinted, .err_msg = err_msg);
    mmgr_line_ch(out, ']');
    g_cli_cells->persistent_buf_release(text, &g_cli_servers_cells);
    return done;
}

int cli_servers_hints(const CliServersPathOutErrMsgFromToCfg *args)
{
    const char *path = args->path;
    VerbaLinea *out = args->out;
    if (EMBED_CALL(cli_servers_path_lsp_internal.slice, CliServersPathLspCtx, .path = path))
    {
        return EMBED_CALL(hints_of_tree, ServersHintsCtx, .path = path, .from = args->from_u32, .to = args->to_u32,
                          .out = out, .err_msg = args->err_msg);
    }
    CliLsp *const server =
        EMBED_CALL(cli_servers_path_lsp_internal.owner_answering, CliServersPathLspCtx, .path = path);
    if ((server == NULL) || !EMBED_CALL(cli_servers_server_internal.capable, CliServersServerCtx, .server = server,
                                        .name = "inlayHintProvider"))
    {
        if (server != NULL)
        {
            EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
        }
        mmgr_line_put(out, "[]");
        return 1;
    }
    EMBED_CALL(cli_servers_reading_answer_internal.reading_lock, CliServersReadingAnswerCtx, .reading = &s_reading);
    // the text the server has, in a cell of its size and the 0 after it
    unsigned long long text_size = 0ull;
    const int sized =
        EMBED_CALL(cli_lsp_size_path.text_size, CliLspSizePathCfg, .lsp = server, .path = path, .size = &text_size);
    char *const text = sized ? g_cli_cells->temporary_buf_alloc(text_size + 1ull, &g_cli_servers_cells) : NULL;
    VerbaLinea hints_line = mmgr_line_over(text, text_size + 1ull);
    const int has =
        (text != NULL) &&
        EMBED_CALL(cli_lsp_path_out.text, CliLspPathOutCfg, .lsp = server, .path = path, .out = &hints_line) &&
        mmgr_line_ok(&hints_line);
    const unsigned long long size = has ? hints_line.at : 0ull;
    unsigned long long lines = 1ull;
    unsigned long long last = 0ull;
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        if (text[at] == '\n')
        {
            lines += 1ull;
            last = at + 1ull;
        }
    }
    // the range ends at the start of the line after `to`, or at the end of the text where `to` is its last line
    char uri[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_servers_path_uri_internal.uri_get, CliServersPathUriCtx, .path = path, .uri_out = uri);
    char params_text[CLI_SERVERS_PARAMS];
    VerbaLinea params = {params_text, sizeof(params_text), 0ull};
    EMBED_CALL(hints_params_begin, ServersHintsCtx, .uri = uri, .params = &params);
    mmgr_line_put(&params, ",\"range\":{\"start\":{\"line\":");
    mmgr_line_decimal(&params, args->from_u32, 1u);
    mmgr_line_put(&params, ",\"character\":0},\"end\":{\"line\":");
    const int inside = ((unsigned long long)args->to_u32 + 1ull) < lines;
    mmgr_line_decimal(&params, inside ? (unsigned long long)args->to_u32 + 1ull : lines - 1ull, 1u);
    mmgr_line_put(&params, ",\"character\":");
    mmgr_line_decimal(&params,
                      inside ? 0ull
                             : EMBED_CALL(cli_text_size_byte.units, CliTextSizeByteCfg,
                                          .text = (size > 0ull) ? text + last : "", .size = size - last),
                      1u);
    mmgr_line_put(&params, "}}}");
    g_cli_cells->persistent_buf_release(text, &g_cli_servers_cells);
    CliLspAnswer answer;
    const int requested = EMBED_CALL(
        cli_servers_err_msg_internal.request, CliServersErrMsgCtx, .server = server, .method = "textDocument/inlayHint",
        .params = &params, .wait = CLI_SERVERS_REQUEST_TIMEOUT_MS, .answer = &answer, .err_msg = args->err_msg);
    EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
    if (requested)
    {
        const CliJson *const json = &s_reading.json;
        const unsigned int all = EMBED_CALL(cli_servers_reading_answer_internal.answer_read, CliServersReadingAnswerCtx,
                                            .reading = &s_reading, .answer = &answer);
        mmgr_line_ch(out, '[');
        int written = 0;
        for (unsigned int one = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                           .node = all, .kind = CFG_JSON_ARRAY)
                                    ? json->node[all].first
                                    : CLI_JSON_NONE;
             one != CLI_JSON_NONE; one = json->node[one].next)
        {
            const unsigned long long before = out->at;
            const CliTextPlace at =
                EMBED_CALL(cli_servers_json_node_otherwise_internal.place, CliServersJsonNodeOtherwiseCtx, .json = json,
                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = one, .name = "position"));
            mmgr_line_put(out, written ? ",{\"line\":" : "{\"line\":");
            mmgr_line_decimal(out, at.line, 1u);
            mmgr_line_put(out, ",\"col\":");
            mmgr_line_decimal(out, at.col, 1u);
            mmgr_line_put(out, ",\"label\":");
            if (!EMBED_CALL(hints_label_write, ServersHintsCtx, .json = json,
                            .label = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                .json_read = json, .object = one, .name = "label"),
                            .out = out))
            {
                out->at = before;
                continue;
            }
            mmgr_line_put(out, ",\"kind\":");
            // the kind is cut to its low byte, as the protocol's 1 and 2 always fit
            mmgr_line_decimal(out,
                              EMBED_CALL(cli_servers_json_node_otherwise_internal.whole, CliServersJsonNodeOtherwiseCtx,
                                         .json = json,
                                         .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                            .json_read = json, .object = one, .name = "kind"),
                                         .otherwise = 0ull) &
                                  0xffull,
                              1u);
            const int left = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                        .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                           .json_read = json, .object = one, .name = "paddingLeft"),
                                        .kind = CFG_JSON_TRUE);
            const int right = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                         .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                            .json_read = json, .object = one, .name = "paddingRight"),
                                         .kind = CFG_JSON_TRUE);
            mmgr_line_put(out, left ? ",\"left\":true" : ",\"left\":false");
            mmgr_line_put(out, right ? ",\"right\":true}" : ",\"right\":false}");
            written = 1;
        }
        mmgr_line_ch(out, ']');
    }
    EMBED_CALL(cli_servers_reading_answer_internal.reading_release, CliServersReadingAnswerCtx, .reading = &s_reading);
    return requested;
}

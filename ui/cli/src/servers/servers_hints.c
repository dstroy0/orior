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
    unsigned long long length = 0ULL;
    const char *const capabilities =
        EMBED_CALL(cli_lsp_size_kind.capabilities, CliLspSizeKindCfg, .lsp_read = args->server, .size = &length);
    char quiet[256];
    VerbaLinea err_msg = {quiet, sizeof(quiet), 0ULL};
    const unsigned int root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg,
                                         .json = &s_capable, .text = capabilities, .size = length, .nodes_more = 0u,
                                         .text_more = 0ULL, .err_msg = &err_msg, .owner = &g_cli_servers_cells);
    return EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_capable, .object = root,
                      .name = name);
}

// the capabilities' tree given back with their lock
static void hints_capability_free(void)
{
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &s_capable);
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
    int list_entries = 0;
    for (unsigned int one = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_capable,
                                       .node = commands, .kind = CFG_JSON_ARRAY)
                                ? s_capable.node[commands].first
                                : CLI_JSON_NONE;
         !list_entries && (one != CLI_JSON_NONE); one = s_capable.node[one].next)
    {
        const char *const name = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = &s_capable, .node = one, .length_pointer = NULL);
        list_entries =
            (name != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = name, .other = args->command, .cap = CLI_CELLS_POOL});
    }
    hints_capability_free();
    return list_entries;
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
    EMBED_CALL(cli_edits_position.text_position_write, CliEditsPositionCfg,
               .place = (CliTextPosition){args->from.line, args->from.col}, .out = spanned->out);
    mmgr_line_ch(spanned->out, ',');
    EMBED_CALL(cli_edits_position.text_position_write, CliEditsPositionCfg,
               .place = (CliTextPosition){args->to.line, args->to.col}, .out = spanned->out);
    mmgr_line_ch(spanned->out, ']');
    spanned->written = 1;
}

int cli_servers_selection(const CliServersPathErrMsgOutCtx *args)
{
    const char *path = args->path;
    unsigned long long size = 0ULL;
    char *const branch = (args->key != NULL)
                             ? EMBED_CALL(cli_document_key.text_alloc, CliDocumentKeyCfg, .key = args->key,
                                          .size_pointer = &size, .owner = &g_cli_servers_cells)
                             : NULL;
    char *const text = (branch != NULL) ? branch
                                        : EMBED_CALL(cli_servers_path_size_kind.text_alloc, CliServersPathSizeKindCfg,
                                                     .path = path, .size_pointer = &size);
    CliServersSpanned spanned = {args->out, 0};
    mmgr_line_ch(args->out, '[');
    const int done =
        (text == NULL) ||
        EMBED_CALL(cli_symbols_path_text_size_context_err_msg_from_symbol_position.spans,
                   CliSymbolsPathTextSizeContextErrMsgFromSymbolPositionCfg, .path = path, .text = text, .size = size,
                   .from_symbol_position = (CliSymbolPosition){args->from.line, args->from.col},
                   .to_symbol_position = (CliSymbolPosition){args->to.line, args->to.col},
                   .each_spanned = hints_span_write, .context = &spanned, .err_msg = args->err_msg);
    mmgr_line_ch(args->out, ']');
    g_cli_cells->persistent_buf_release(text, &g_cli_servers_cells);
    return done;
}

// The tree's hints being written: where, and whether one was
typedef struct
{
    VerbaLinea *out;
    int written;
} CliServersHinted;

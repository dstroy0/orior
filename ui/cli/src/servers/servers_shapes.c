// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_shapes.c: a server's answer read, and what it reports written in orior's own shapes: a place, a path, a hover
// as Markdown, a completion as the editor's item, a diagnostic and a location. No request about a place reaches a
// server given the tree's slices, so a place goes and comes back on the file's own lines
#include "../command_line/pool.h"
#include "../text/measured.h"
#include "../text/text.h"
#include "servers_internal.h"

void cli_servers_read_state_lock(const CliServersReadStateAnswerCtx *args)
{
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &args->read_state->json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
}

unsigned int cli_servers_read(const CliServersReadStateCtx *args)
{
    g_cli_guard->persistent_buf_release(args->read_state->json.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), args->read_state->json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&args->read_state->json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    char quiet[256];
    VerbaLinea err_msg = {quiet, sizeof(quiet), 0ULL};
    return EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc_temporary, CliJsonTreeTextSizeErrMsgCfg,
                      .json = &args->read_state->json, .text = args->text, .size = args->size, .nodes_more = 0u,
                      .text_more = 0ULL, .err_msg = &err_msg, .owner = g_cli_servers_owner);
}

unsigned int cli_servers_answer_read(const CliServersReadStateAnswerCtx *args)
{
    const unsigned int root =
        EMBED_CALL(cli_servers_read_state_internal.read, CliServersReadStateCtx, .read_state = args->read_state,
                   .text = args->answer->text, .size = args->answer->size);
    g_cli_guard->persistent_buf_release(args->answer->message, g_cli_servers_owner);
    args->answer->message = NULL;
    return root;
}

int cli_servers_request(const CliServersErrMsgCtx *args)
{
    const char *method = args->method;
    const VerbaLinea *params = args->params;
    unsigned long long wait = args->wait;
    CliLspAnswer *answer = args->answer;
    VerbaLinea *err_msg = args->err_msg;
    *answer = (CliLspAnswer){NULL, NULL, 0ULL};
    if (!mmgr_line_ok(params))
    {
        mmgr_line_put(err_msg, "the request could not be written: its params do not fit");
        return 0;
    }
    // a server given the tree's slices is waited on until it answers
    EMBED_CALL(cli_servers_internal.request_begin, CliServersCtx, .server_read = args->server);
    const int requested =
        EMBED_CALL(cli_lsp_err_msg_length.request, CliLspErrMsgLengthCfg, .lsp = args->server, .method = method,
                   .params = params->out, .length = params->at, .wait = wait, .answer = answer, .err_msg = err_msg);
    EMBED_CALL(cli_servers_internal.request_end, CliServersCtx, .server_read = args->server);
    return requested;
}

int cli_servers_request_at(const CliServersPathCtx *args)
{
    const char *path = args->path;
    unsigned int line = args->line;
    unsigned int col = args->col;
    const char *added = args->added;
    char text[CLI_SERVERS_PARAMS];
    VerbaLinea params = {text, sizeof(text), 0ULL};
    EMBED_CALL(cli_lsp_path.at, CliLspPathCfg, .path = path, .line = line, .col = col, .added = added,
               .params_verba_linea_pointer = &params);
    return EMBED_CALL(cli_servers_err_msg_internal.request, CliServersErrMsgCtx, .server = args->server,
                      .method = args->method, .params = &params, .wait = args->wait, .answer = args->answer,
                      .err_msg = args->err_msg);
}

unsigned long long cli_servers_json_integer(const CliServersJsonNodeOtherwiseCtx *args)
{
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json,
                    .node = args->node, .kind = CFG_JSON_NUMBER) ||
        (args->json->node[args->node].length == 0ULL))
    {
        return args->otherwise;
    }
    const char *const digits = args->json->text + args->json->node[args->node].text;
    unsigned long long value = 0ULL;
    for (unsigned long long at = 0ULL; at < args->json->node[args->node].length; at += 1ULL)
    {
        if ((digits[at] < '0') || (digits[at] > '9') || (value > (~0ULL - 9ULL) / 10ULL))
        {
            return args->otherwise;
        }
        value = (value * 10ULL) + (unsigned long long)(digits[at] - '0');
    }
    return value;
}

CliTextPosition cli_servers_position(const CliServersJsonNodeOtherwiseCtx *args)
{
    // a place past 2^32 units is cut to its low word, as the protocol's unsigned integer is read
    const CliTextPosition place = {
        (unsigned int)EMBED_CALL(cli_servers_json_node_otherwise_internal.integer, CliServersJsonNodeOtherwiseCtx,
                                 .json = args->json,
                                 .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                    .json_read = args->json, .object = args->node, .name = "line"),
                                 .otherwise = 0ULL),
        (unsigned int)EMBED_CALL(cli_servers_json_node_otherwise_internal.integer, CliServersJsonNodeOtherwiseCtx,
                                 .json = args->json,
                                 .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                    .json_read = args->json, .object = args->node, .name = "character"),
                                 .otherwise = 0ULL)};
    return place;
}

void cli_servers_path_write(const CliServersPathErrMsgCtx *args)
{
    char written[CLI_PATH_MAX_LEN];
    VerbaLinea displayed = {written, sizeof(written), 0ULL};
    EMBED_CALL(cli_servers_path_err_msg_internal.tree_path, CliServersPathErrMsgCtx, .tree = args->tree,
               .path = args->path, .out = &displayed);
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
               .text_bytes = (const unsigned char *)written, .length = mmgr_line_ok(&displayed) ? displayed.at : 0ULL);
}

void cli_servers_markdown_get(const CliServersJsonOutCtx *args)
{
    unsigned long long size = 0ULL;
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                        .json_read = args->json, .node = args->contents, .length_pointer = &size);
    if (text != NULL)
    {
        mmgr_line_put_n(args->out, text, size);
        return;
    }
    if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json,
                   .node = args->contents, .kind = CFG_JSON_ARRAY))
    {
        // a part that writes nothing is left out with the break before it: each is measured before it is written
        int written = 0;
        for (unsigned int part = args->json->node[args->contents].first; part != CLI_JSON_NONE;
             part = args->json->node[part].next)
        {
            char piece[64];
            VerbaLinea measure;
            EMBED_CALL(cli_measured_piece.measure_begin, CliMeasuredPieceCfg, .line = &measure, .piece = piece,
                       .piece_size = sizeof(piece));
            EMBED_CALL(cli_servers_json_out_internal.markdown_get, CliServersJsonOutCtx, .json = args->json,
                       .contents = part, .out = &measure);
            if (EMBED_CALL(cli_measured_table.measure, CliMeasuredCfg, .line_read = &measure) == 0ULL)
            {
                continue;
            }
            mmgr_line_put(args->out, written ? "\n\n" : "");
            EMBED_CALL(cli_servers_json_out_internal.markdown_get, CliServersJsonOutCtx, .json = args->json,
                       .contents = part, .out = args->out);
            written = 1;
        }
        return;
    }
    unsigned long long language_size = 0ULL;
    const char *const language =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = args->contents, .name = "language"),
                   .length_pointer = &language_size);
    unsigned long long value_size = 0ULL;
    const char *const value =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = args->contents, .name = "value"),
                   .length_pointer = &value_size);
    if (value == NULL)
    {
        return;
    }
    if (language != NULL)
    {
        mmgr_line_put(args->out, "```");
        mmgr_line_put_n(args->out, language, language_size);
        mmgr_line_ch(args->out, '\n');
        mmgr_line_put_n(args->out, value, value_size);
        mmgr_line_put(args->out, "\n```");
        return;
    }
    mmgr_line_put_n(args->out, value, value_size);
}

typedef struct
{
    unsigned long long kind;
    const CliJson *json;
    unsigned int one;
    const char *name;
    unsigned long long *size;
    VerbaLinea *out;
} ServersShapesCtx;
// the kind the editor shows for the protocol's kind of a completion
EMBED_INLINE const char *shapes_kind_get(const ServersShapesCtx *args)
{
    switch (args->kind)
    {
    case 2ULL:
    case 3ULL:
    case 4ULL:
        return "function";
    case 5ULL:
    case 10ULL:
        return "field";
    case 6ULL:
        return "variable";
    case 7ULL:
    case 8ULL:
    case 13ULL:
    case 22ULL:
    case 25ULL:
        return "type";
    case 9ULL:
        return "module";
    case 12ULL:
    case 20ULL:
    case 21ULL:
        return "constant";
    case 14ULL:
        return "keyword";
    case 15ULL:
        return "snippet";
    case 24ULL:
        return "operator";
    default:
        return "word";
    }
}

// a string member of `one`, NULL where it has none
EMBED_INLINE const char *shapes_member(const ServersShapesCtx *args)
{
    const CliJson *json = args->json;
    const char *name = args->name;
    return EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                      .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                         .object = args->one, .name = name),
                      .length_pointer = args->size);
}

void cli_servers_item_get(const CliServersJsonOneCtx *args)
{
    unsigned long long label_size = 0ULL;
    const char *label = EMBED_CALL(shapes_member, ServersShapesCtx, .json = args->json, .one = args->one,
                                   .name = "label", .size = &label_size);
    label = (label != NULL) ? label : "";
    unsigned long long from = 0ULL;
    unsigned long long to = 0ULL;
    EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = label, .size = label_size,
               .from_u64s = &from, .to_u64s = &to);
    while ((to - from >= 3ULL) &&
           (memor.cmp(&(MemoriaCfg){.src = label + from, .other = "\xe2\x80\xa2", .bytes = 3u}) == 0))
    {
        from += 3ULL;
    }
    unsigned long long inner_from = 0ULL;
    unsigned long long inner_to = 0ULL;
    EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = label + from, .size = to - from,
               .from_u64s = &inner_from, .to_u64s = &inner_to);
    const char *const displayed = label + from + inner_from;
    const unsigned long long displayed_size = inner_to - inner_from;
    unsigned long long name_size = 0ULL;
    const char *name = EMBED_CALL(shapes_member, ServersShapesCtx, .json = args->json, .one = args->one,
                                  .name = "filterText", .size = &name_size);
    if (name == NULL)
    {
        name = displayed;
        name_size = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = displayed,
                               .at = name_size, .size = displayed_size, .set = "( ");
    }
    unsigned long long insert_size = 0ULL;
    const char *insert = EMBED_CALL(
        cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
        .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                           .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                .json_read = args->json, .object = args->one, .name = "textEdit"),
                           .name = "newText"),
        .length_pointer = &insert_size);
    insert = (insert != NULL) ? insert
                              : EMBED_CALL(shapes_member, ServersShapesCtx, .json = args->json, .one = args->one,
                                           .name = "insertText", .size = &insert_size);
    if (insert == NULL)
    {
        insert = name;
        insert_size = name_size;
    }
    static char s_detail[CLI_COMPLETIONS_TEXT];
    VerbaLinea detail = {s_detail, sizeof(s_detail), 0ULL};
    unsigned long long detail_src_size = 0ULL;
    const char *const detail_src = EMBED_CALL(shapes_member, ServersShapesCtx, .json = args->json, .one = args->one,
                                              .name = "detail", .size = &detail_src_size);
    if (detail_src != NULL)
    {
        mmgr_line_put_n(&detail, detail_src, detail_src_size);
        mmgr_line_ch(&detail, ' ');
    }
    mmgr_line_put_n(&detail, displayed, displayed_size);
    const unsigned long long detail_size = mmgr_line_ok(&detail) ? detail.at : 0ULL;
    unsigned long long detail_from = 0ULL;
    unsigned long long detail_to = 0ULL;
    EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = s_detail, .size = detail_size,
               .from_u64s = &detail_from, .to_u64s = &detail_to);
    const char *const kind =
        EMBED_CALL(shapes_kind_get, ServersShapesCtx,
                   .kind = EMBED_CALL(cli_servers_json_node_otherwise_internal.integer, CliServersJsonNodeOtherwiseCtx,
                                      .json = args->json,
                                      .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                         .json_read = args->json, .object = args->one, .name = "kind"),
                                      .otherwise = 1ULL));
    (void)EMBED_CALL(cli_edits_items.completion_append, CliEditsItemsCfg, .items = args->items, .label = name,
                     .label_size = name_size, .kind = kind, .detail = s_detail + detail_from,
                     .detail_size = detail_to - detail_from, .insert = insert, .insert_size = insert_size,
                     .snippet = 0);
}

void cli_servers_diagnostic_get(const CliServersJsonCtx *args)
{
    const unsigned int range = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = args->json, .object = args->one, .name = "range");
    const CliTextPosition from =
        EMBED_CALL(cli_servers_json_node_otherwise_internal.place, CliServersJsonNodeOtherwiseCtx, .json = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = range, .name = "start"));
    const CliTextPosition to =
        EMBED_CALL(cli_servers_json_node_otherwise_internal.place, CliServersJsonNodeOtherwiseCtx, .json = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = range, .name = "end"));
    const unsigned int message = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                            .json_read = args->json, .object = args->one, .name = "message");
    unsigned long long size = 0ULL;
    const char *text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                  .json_read = args->json, .node = message, .length_pointer = &size);
    text = (text != NULL) ? text
                          : EMBED_CALL(shapes_member, ServersShapesCtx, .json = args->json, .one = message,
                                       .name = "value", .size = &size);
    // the severity is cut to its low byte, as the protocol's 1 to 4 always fits
    const unsigned int severity =
        (unsigned int)(EMBED_CALL(cli_servers_json_node_otherwise_internal.integer, CliServersJsonNodeOtherwiseCtx,
                                  .json = args->json,
                                  .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                     .json_read = args->json, .object = args->one, .name = "severity"),
                                  .otherwise = 1ULL) &
                       0xffULL);
    *args->diagnostic =
        (CliServerDiagnostic){from.line, from.col, to.line, to.col, severity, (text != NULL) ? text : "", size};
}

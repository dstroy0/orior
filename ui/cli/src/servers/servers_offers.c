// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_offers.c: what a server offers written in orior's own shapes: a call's signature, each parameter by its
// span in the signature's text in UTF-16 units, and a quick fix or a refactoring with the server's own form of it,
// which is given back to carry it out; and whether a span touches another
#include "servers_internal.h"

// the most parameters of a signature written
#define CLI_SERVERS_PARAMS 256u

typedef struct
{
    const CliJson *json;
    unsigned int value;
    VerbaLinea *out;
    unsigned int param;
    const char *label;
    unsigned long long size;
    unsigned long long *from;
    unsigned long long *to;
    CliTextPosition one;
    CliTextPosition other;
} ServersOffersCtx;
// what a server reports of a signature or a parameter, from either of the shapes the protocol allows, its letters as
// they are
EMBED_INLINE void offers_documentation_get(const ServersOffersCtx *args)
{
    const CliJson *json = args->json;
    unsigned long long size = 0ull;
    const char *text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                                  .node = args->value, .length_pointer = &size);
    text = (text != NULL)
               ? text
               : EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                            .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                               .json_read = json, .object = args->value, .name = "value"),
                            .length_pointer = &size);
    if (text != NULL)
    {
        mmgr_line_put_n(args->out, text, size);
    }
}

// the span of the parameter `param` in the signature's text `label`, `size` bytes, into `from` and `to`: 1, or 0
// where the parameter's text is not in it or its span is not whole
EMBED_INLINE int offers_param_span(const ServersOffersCtx *args)
{
    const CliJson *json = args->json;
    unsigned long long size = args->size;
    unsigned long long *from = args->from;
    unsigned long long *to = args->to;
    const unsigned int named = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->param, .name = "label");
    unsigned long long part_size = 0ull;
    const char *const part = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                        .json_read = json, .node = named, .length_pointer = &part_size);
    if (part != NULL)
    {
        unsigned long long at = 0ull;
        if (!EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = args->label, .length_value = size,
                        .required = part, .size = part_size, .at_pointer = &at))
        {
            return 0;
        }
        *from = EMBED_CALL(cli_text_size_byte.units, CliTextSizeByteCfg, .text = args->label, .size = at);
        *to = *from + EMBED_CALL(cli_text_size_byte.units, CliTextSizeByteCfg, .text = part, .size = part_size);
        return 1;
    }
    const unsigned int first = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = named, .index = 0u);
    const unsigned int second = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                           .json_read = json, .array = named, .index = 1u);
    const unsigned long long none = ~0ull;
    *from = EMBED_CALL(cli_servers_json_node_otherwise_internal.integer, CliServersJsonNodeOtherwiseCtx, .json = json,
                       .node = first, .otherwise = none);
    *to = EMBED_CALL(cli_servers_json_node_otherwise_internal.integer, CliServersJsonNodeOtherwiseCtx, .json = json,
                     .node = second, .otherwise = none);
    // each end is cut to its low word, as the protocol's unsigned integer is read
    *from &= 0xffffffffull;
    *to &= 0xffffffffull;
    return EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = named,
                      .kind = CFG_JSON_ARRAY) &&
           (EMBED_CALL(cli_servers_json_node_otherwise_internal.integer, CliServersJsonNodeOtherwiseCtx, .json = json,
                       .node = first, .otherwise = none) != none) &&
           (EMBED_CALL(cli_servers_json_node_otherwise_internal.integer, CliServersJsonNodeOtherwiseCtx, .json = json,
                       .node = second, .otherwise = none) != none);
}

CliSignaturePanel *cli_servers_signature_get(const CliServersJsonOutHelpCtx *args)
{
    const unsigned int all = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                        .object = args->help, .name = "signatures");
    const unsigned long long chosen =
        EMBED_CALL(cli_servers_json_node_otherwise_internal.integer, CliServersJsonNodeOtherwiseCtx, .json = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = args->help, .name = "activeSignature"),
                   .otherwise = 0ull);
    unsigned int one = (chosen < 0xffffffffull)
                           ? EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                        .json_read = args->json, .array = all, .index = (unsigned int)chosen)
                           : CLI_JSON_NONE;
    one = (one != CLI_JSON_NONE) ? one
                                 : EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                              .json_read = args->json, .array = all, .index = 0u);
    unsigned long long size = 0ull;
    const char *const label =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = one, .name = "label"),
                   .length_pointer = &size);
    if (label == NULL)
    {
        return NULL;
    }
    const unsigned int parameters = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                               .json_read = args->json, .object = one, .name = "parameters");
    const unsigned int first = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                          .json_read = args->json, .node = parameters, .kind = CFG_JSON_ARRAY)
                                   ? args->json->node[parameters].first
                                   : CLI_JSON_NONE;
    unsigned int most = 0u;
    for (unsigned int param = first; param != CLI_JSON_NONE; param = args->json->node[param].next)
    {
        most += 1u;
    }
    char doc_text[1u << 16u];
    VerbaLinea doc = {doc_text, sizeof(doc_text), 0ull};
    EMBED_CALL(offers_documentation_get, ServersOffersCtx, .json = args->json,
               .value = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                   .object = one, .name = "documentation"),
               .out = &doc);
    const unsigned long long doc_size = mmgr_line_ok(&doc) ? doc.at : 0ull;
    CliSignaturePanel *const panel = g_cli_cells->persistent_buf_alloc(
        sizeof(*panel) + (most * sizeof(CliSignatureParameter)) + size + doc_size + 2ull, args->owner);
    if (panel == NULL)
    {
        return NULL;
    }
    memor.set(&(MemoriaCfg){.dst = panel, .val = 0, .bytes = sizeof(*panel)});
    panel->parameter = (CliSignatureParameter *)(void *)(panel + 1);
    char *const text = (char *)(void *)(panel->parameter + most);
    memor.cpy(&(MemoriaCfg){.dst = text, .src = label, .bytes = size});
    text[size] = '\0';
    memor.cpy(&(MemoriaCfg){.dst = text + size + 1ull, .src = doc_text, .bytes = doc_size});
    text[size + 1ull + doc_size] = '\0';
    panel->label = text;
    panel->length = size;
    panel->doc = text + size + 1ull;
    panel->doc_length = doc_size;
    for (unsigned int param = first; param != CLI_JSON_NONE; param = args->json->node[param].next)
    {
        unsigned long long from = 0ull;
        unsigned long long to = 0ull;
        if (EMBED_CALL(offers_param_span, ServersOffersCtx, .json = args->json, .param = param, .label = label,
                       .size = size, .from = &from, .to = &to))
        {
            // a span within a label is far inside what an unsigned int represents
            panel->parameter[panel->parameters] = (CliSignatureParameter){(unsigned int)from, (unsigned int)to};
            panel->parameters += 1u;
        }
    }
    const unsigned long long none = ~0ull;
    unsigned long long active =
        EMBED_CALL(cli_servers_json_node_otherwise_internal.integer, CliServersJsonNodeOtherwiseCtx, .json = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = one, .name = "activeParameter"),
                   .otherwise = none);
    active =
        (active != none)
            ? active
            : EMBED_CALL(cli_servers_json_node_otherwise_internal.integer, CliServersJsonNodeOtherwiseCtx,
                         .json = args->json,
                         .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                            .json_read = args->json, .object = args->help, .name = "activeParameter"),
                         .otherwise = none);
    // a parameter's place is far inside what an int represents
    panel->active = (active < panel->parameters) ? (int)active : -1;
    return panel;
}

// whether the place `one` stands before `other`
EMBED_INLINE int offers_before(const ServersOffersCtx *args)
{
    return (args->one.line < args->other.line) ||
           ((args->one.line == args->other.line) && (args->one.col < args->other.col));
}

int cli_servers_overlaps(const CliServersJsonFromCtx *args)
{
    const CliTextPosition start =
        EMBED_CALL(cli_servers_json_node_otherwise_internal.place, CliServersJsonNodeOtherwiseCtx, .json = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = args->range, .name = "start"));
    const CliTextPosition end =
        EMBED_CALL(cli_servers_json_node_otherwise_internal.place, CliServersJsonNodeOtherwiseCtx, .json = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = args->range, .name = "end"));
    return !EMBED_CALL(offers_before, ServersOffersCtx, .one = end, .other = args->from) &&
           !EMBED_CALL(offers_before, ServersOffersCtx, .one = args->to, .other = start);
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// json_allocated.c: a JSON tree in a cell of its own, sized to the text it is read from: its tokens counted before the
// read, the nodes and texts one cell, the tokens a cell given back as the read ends
#include "json_tree.h"

#include "../command_line/pool.h"
#include "text.h"

typedef struct
{
    char letter;
    CliJson *json;
    unsigned int nodes_max;
    unsigned long long text_max;
    const char *what;
    const CellOwner *owner;
    int temporary;
    const char *text;
    unsigned long long size;
    unsigned int nodes_more;
    unsigned long long text_more;
    VerbaLinea *err_msg;
} JsonTakenCtx;

// whether a letter goes on a number or a literal: a digit, a letter, or a sign, a point or an exponent's sign
EMBED_INLINE int json_word_letter(const JsonTakenCtx *args)
{
    return ((args->letter >= '0') && (args->letter <= '9')) || ((args->letter >= 'a') && (args->letter <= 'z')) ||
           ((args->letter >= 'A') && (args->letter <= 'Z')) || (args->letter == '.') || (args->letter == '+') ||
           (args->letter == '-');
}

unsigned int cli_json_tokens_count(const CliJsonTreeTextCfg *args)
{
    unsigned int count = 0u;
    unsigned long long at = 0ULL;
    while (at < args->size)
    {
        const char letter = args->text[at];
        if (letter == '"')
        {
            at += 1ULL;
            at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->text, .at = at,
                            .size = args->size, .set = "\"\\");
            while ((at < args->size) && (args->text[at] == '\\'))
            {
                at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->text,
                                .at = at + 2ULL, .size = args->size, .set = "\"\\");
            }
            at += 1ULL;
            count += 1u;
        }
        else if ((letter == '{') || (letter == '['))
        {
            at += 1ULL;
            count += 1u;
        }
        else if (EMBED_CALL(json_word_letter, JsonTakenCtx, .letter = letter))
        {
            while ((at < args->size) && EMBED_CALL(json_word_letter, JsonTakenCtx, .letter = args->text[at]))
            {
                at += 1ULL;
            }
            count += 1u;
        }
        else
        {
            at += 1ULL;
        }
    }
    return count;
}

// the tree's capacity reserved as cli_json_reserve says, its cell taken for `owner` from the temporary tier where
// `temporary` is set, else from the persistent tier
EMBED_INLINE int json_reserved_in(const JsonTakenCtx *args)
{
    CliJson *json = args->json;
    unsigned int nodes_max = args->nodes_max;
    unsigned long long text_max = args->text_max;
    const char *what = args->what;
    const unsigned long long nodes_bytes = (unsigned long long)nodes_max * sizeof(CliJsonNode);
    const unsigned long long bytes = nodes_bytes + text_max + 1ULL;
    unsigned char *const cell = args->temporary ? g_cli_guard->temporary_buf_alloc(bytes, args->owner)
                                                : g_cli_guard->persistent_buf_alloc(bytes, args->owner);
    if (cell != NULL)
    {
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = json,
                   .node_json_node_pointer = (CliJsonNode *)cell, .nodes_max = nodes_max,
                   .text_out = (char *)(cell + nodes_bytes), .text_max = text_max + 1ULL);
    }
    else
    {
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = json,
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    }
    json->what = what;
    return cell != NULL;
}

int cli_json_reserve(const CliJsonTreeNodesMaxCfg *args)
{
    return EMBED_CALL(json_reserved_in, JsonTakenCtx, .json = args->json, .nodes_max = args->nodes_max,
                      .text_max = args->text_max, .what = args->what, .owner = args->owner, .temporary = 0);
}

int cli_json_reserved_temporary(const CliJsonTreeNodesMaxCfg *args)
{
    return EMBED_CALL(json_reserved_in, JsonTakenCtx, .json = args->json, .nodes_max = args->nodes_max,
                      .text_max = args->text_max, .what = args->what, .owner = args->owner, .temporary = 1);
}

int cli_json_capacity(const CliJsonTreeNodesCfg *args)
{
    const int fits = (((unsigned long long)args->json->nodes + args->nodes) <= args->json->nodes_max) &&
                     ((args->json->used + args->text_u64) <= args->json->text_max);
    args->json->full = args->json->full || !fits;
    return fits;
}

void cli_json_clear(const CliJsonTreeCfg *args)
{
    args->json->nodes = 0u;
    args->json->used = 0ULL;
    args->json->full = 0;
}

// the tree read as cli_json_alloc reads it, its own cell taken as json_reserved_in takes it
EMBED_INLINE unsigned int json_alloc_in(const JsonTakenCtx *args)
{
    CliJson *json = args->json;
    const char *text = args->text;
    unsigned long long size = args->size;
    VerbaLinea *err_msg = args->err_msg;
    // the tree begun empty before any take: a take refused leaves a tree with no cell for its holder to give back
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = json, .node_json_node_pointer = NULL,
               .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    const unsigned int tokens_max =
        EMBED_CALL(cli_json_tree_text.json_tokens_count, CliJsonTreeTextCfg, .text = text, .size = size) + 1u;
    CfgJsonToken *const tokens =
        g_cli_guard->temporary_buf_alloc((unsigned long long)tokens_max * sizeof(CfgJsonToken), args->owner);
    if ((tokens == NULL) ||
        !EMBED_CALL(json_reserved_in, JsonTakenCtx, .json = json, .nodes_max = tokens_max + args->nodes_more,
                    .text_max = size + args->text_more, .what = "a tree's nodes and texts", .owner = args->owner,
                    .temporary = args->temporary))
    {
        g_cli_guard->persistent_buf_release(tokens, args->owner);
        mmgr_line_put(err_msg, "the cells a tree is read into could not be taken");
        return CLI_JSON_NONE;
    }
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_tokens.json_read, CliJsonTreeTextSizeTokensCfg, .json = json, .text = text,
                   .size = size, .tokens = tokens, .tokens_max = tokens_max, .err_msg = err_msg);
    g_cli_guard->persistent_buf_release(tokens, args->owner);
    return root;
}

unsigned int cli_json_alloc(const CliJsonTreeTextSizeErrMsgCfg *args)
{
    return EMBED_CALL(json_alloc_in, JsonTakenCtx, .json = args->json, .text = args->text, .size = args->size,
                      .nodes_more = args->nodes_more, .text_more = args->text_more, .err_msg = args->err_msg,
                      .owner = args->owner, .temporary = 0);
}

unsigned int cli_json_alloc_temporary(const CliJsonTreeTextSizeErrMsgCfg *args)
{
    return EMBED_CALL(json_alloc_in, JsonTakenCtx, .json = args->json, .text = args->text, .size = args->size,
                      .nodes_more = args->nodes_more, .text_more = args->text_more, .err_msg = args->err_msg,
                      .owner = args->owner, .temporary = 1);
}

void cli_json_extent(const CliJsonTreeNodeNodesCfg *args)
{
    unsigned int *nodes = args->nodes_pointer;
    unsigned long long *text = args->text_u64s;
    if ((args->node == CLI_JSON_NONE) || (args->node >= args->json_read->nodes))
    {
        return;
    }
    const CliJsonNode *const one = &args->json_read->node[args->node];
    *nodes += 1u;
    *text += ((one->type == CFG_JSON_STRING) || (one->type == CFG_JSON_NUMBER)) ? (one->length + 1ULL) : 0ULL;
    for (unsigned int child = one->first; (child != CLI_JSON_NONE) && (child < args->json_read->nodes);
         child = args->json_read->node[child].next)
    {
        // a member's key is stored again where the member is copied
        *text += (one->type == CFG_JSON_OBJECT)
                     ? (cellul.len(&(CatenaFinitaCfg){.src = args->json_read->text + args->json_read->node[child].key,
                                                      .cap = CLI_POOL_BYTES}) +
                        1ULL)
                     : 0ULL;
        EMBED_CALL(cli_json_tree_node_nodes.json_extent, CliJsonTreeNodeNodesCfg, .json_read = args->json_read,
                   .node = child, .nodes_pointer = nodes, .text_u64s = text);
    }
}

unsigned int cli_json_file_alloc(const CliJsonTreeErrMsgCfg *args)
{
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = args->path,
                                                  .size_pointer = &size, .owner = g_cli_text_owner);
    if (bytes == NULL)
    {
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = args->json,
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, ": it could not be read");
        return CLI_JSON_NONE;
    }
    // the tree stores its texts of its own, and the file is let go as the read ends
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = args->json,
                   .text = (const char *)bytes, .size = size, .nodes_more = args->nodes_more,
                   .text_more = args->text_more, .err_msg = args->err_msg, .owner = args->owner);
    g_cli_guard->persistent_buf_release((void *)(uintptr_t)bytes, (g_cli_text_owner));
    return root;
}

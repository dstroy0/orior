// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// json_allocated.c: a JSON tree in a cell of its own, sized to the text it is read from: its tokens counted before the
// read, the nodes and texts one cell, the tokens a cell given back as the read ends
#include "json_tree.h"

#include "../command_line/pool.h"
#include "text.h"

typedef struct
{
    char character;
    CliJson *json;
    unsigned int nodes_max;
    unsigned long long text_max;
    const char *what;
    const CellOwner *owner;
    const char *text;
    unsigned long long size;
    unsigned int nodes_more;
    unsigned long long text_more;
    VerbaLinea *err_msg;
} JsonTakenCtx;

// whether a character goes on a number or a literal: a digit, a character, or a sign, a point or an exponent's sign
EMBED_INLINE int json_word_character(const JsonTakenCtx *args)
{
    return ((args->character >= '0') && (args->character <= '9')) || ((args->character >= 'a') && (args->character <= 'z')) ||
           ((args->character >= 'A') && (args->character <= 'Z')) || (args->character == '.') || (args->character == '+') ||
           (args->character == '-');
}

unsigned int cli_json_tokens_count(const CliJsonTreeTextCfg *args)
{
    unsigned int count = 0u;
    unsigned long long at = 0ULL;
    while (at < args->size)
    {
        const char character = args->text[at];
        if (character == '"')
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
        else if ((character == '{') || (character == '['))
        {
            at += 1ULL;
            count += 1u;
        }
        else if (EMBED_CALL(json_word_character, JsonTakenCtx, .character = character))
        {
            while ((at < args->size) && EMBED_CALL(json_word_character, JsonTakenCtx, .character = args->text[at]))
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

// the tree's capacity reserved as cli_json_reserve says, its cell taken for `owner` from the temporary tier
EMBED_INLINE int json_reserved_in(const JsonTakenCtx *args)
{
    CliJson *json = args->json;
    unsigned int nodes_max = args->nodes_max;
    unsigned long long text_max = args->text_max;
    const char *what = args->what;
    const unsigned long long nodes_bytes = (unsigned long long)nodes_max * sizeof(CliJsonNode);
    const unsigned long long bytes = nodes_bytes + text_max + 1ULL;
    unsigned char *const cell = g_cli_guard->temporary_buf_alloc(bytes, args->owner);
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
                      .text_max = args->text_max, .what = args->what, .owner = args->owner);
}

int cli_json_stored(const CliJsonTreeStoredCfg *args)
{
    const CliJson *const json = args->json_read;
    const unsigned long long nodes_bytes = (unsigned long long)json->nodes * sizeof(CliJsonNode);
    const CarcerMovableEntry *const cell = g_cli_guard->movable_buf_alloc(nodes_bytes + json->used + 1ULL, args->owner);
    if (cell == NULL)
    {
        return 0;
    }
    if (json->nodes > 0u)
    {
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = cell, .offset = 0ULL, .src = json->node, .bytes = nodes_bytes});
    }
    if (json->used > 0ULL)
    {
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = cell, .offset = nodes_bytes, .src = json->text, .bytes = json->used});
    }
    const char zero = '\0';
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = cell, .offset = nodes_bytes + json->used, .src = &zero, .bytes = 1ULL});
    cli_json_stored_release(args->stored);
    *args->stored = (CliJsonStored){cell, args->owner, json->nodes, json->used};
    return 1;
}

int cli_json_pinned(const CliJsonTreeStoredCfg *args)
{
    const CliJsonStored *const stored = args->stored_read;
    if ((stored->cell == NULL) ||
        !EMBED_CALL(json_reserved_in, JsonTakenCtx, .json = args->json, .nodes_max = stored->nodes,
                    .text_max = stored->used, .what = "a stored tree", .owner = stored->owner))
    {
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = args->json,
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        return 0;
    }
    const unsigned long long nodes_bytes = (unsigned long long)stored->nodes * sizeof(CliJsonNode);
    if (stored->nodes > 0u)
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = stored->cell, .offset = 0ULL, .dst = args->json->node, .bytes = nodes_bytes});
    }
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = stored->cell, .offset = nodes_bytes, .dst = args->json->text, .bytes = stored->used + 1ULL});
    args->json->nodes = stored->nodes;
    args->json->used = stored->used;
    return 1;
}

void cli_json_stored_release(CliJsonStored *stored)
{
    if (stored->cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(stored->cell, stored->owner);
    }
    stored->cell = NULL;
    stored->nodes = 0u;
    stored->used = 0ULL;
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
                    .text_max = size + args->text_more, .what = "a tree's nodes and texts", .owner = args->owner))
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
                      .owner = args->owner);
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
    g_cli_guard->persistent_buf_release((void *)(embed_word)bytes, (g_cli_text_owner));
    return root;
}

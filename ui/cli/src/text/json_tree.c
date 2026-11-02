// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// json_tree.c: a JSON tree read from its text, its nodes made, found, put, taken out, copied and compared
#include "json_tree.h"
#include "text.h"

// no key: a node that is no object's member
#define CLI_JSON_NO_KEY 0xffffffffffffffffull

void cli_json_begin(const CliJsonTreeNodeNodeCfg *args)
{
    CliJsonNode *node = args->node_json_node_pointer;
    unsigned int nodes_max = args->nodes_max;
    char *text = args->text_out;
    unsigned long long text_max = args->text_max;
    args->json->node = node;
    args->json->nodes = 0u;
    args->json->nodes_max = nodes_max;
    args->json->text = text;
    args->json->used = 0ull;
    args->json->text_max = text_max;
    args->json->full = 0;
    args->json->what = NULL;
}

typedef struct
{
    CliJson *json;
    const char *text;
    unsigned long long length;
    const CliJson *json_read;
    unsigned int parent;
    const char *name;
    unsigned int *before;
    unsigned int value;
    unsigned int object;
    unsigned long long key;
    const CfgJsonToken *tokens;
    unsigned int token;
} JsonTreeCtx;
// `length` bytes of `text` stored among the tree's texts, a NUL after them: where they start, or 0 where they do not
// fit
EMBED_INLINE unsigned long long cli_text_store(const JsonTreeCtx *args)
{
    CliJson *json = args->json;
    const char *text = args->text;
    if (!EMBED_CALL(cli_json_tree_nodes.json_capacity, CliJsonTreeNodesCfg, .json = json, .nodes = 0u,
                    .text_u64 = args->length + 1ull))
    {
        return 0ull;
    }
    const unsigned long long at = json->used;
    memor.cpy(&(MemoriaCfg){.dst = json->text + at, .src = text, .bytes = args->length});
    json->text[at + args->length] = '\0';
    json->used += args->length + 1ull;
    return at;
}

unsigned int cli_json_init(const CliJsonTreeKindCfg *args)
{
    if (!EMBED_CALL(cli_json_tree_nodes.json_capacity, CliJsonTreeNodesCfg, .json = args->json, .nodes = 1u,
                    .text_u64 = 0ull))
    {
        return CLI_JSON_NONE;
    }
    const unsigned int made = args->json->nodes;
    const CliJsonNode node = {args->kind, CLI_JSON_NO_KEY, 0ull, 0ull, CLI_JSON_NONE, CLI_JSON_NONE, CLI_JSON_NONE, 0u};
    args->json->node[made] = node;
    args->json->nodes += 1u;
    return made;
}

unsigned int cli_json_string_init(const CliJsonTreeTextLengthKindCfg *args)
{
    const char *text = args->text;
    unsigned long long length = args->length;
    const unsigned int made =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->json, .kind = CFG_JSON_STRING);
    if (made != CLI_JSON_NONE)
    {
        args->json->node[made].text =
            EMBED_CALL(cli_text_store, JsonTreeCtx, .json = args->json, .text = text, .length = length);
        args->json->node[made].length = args->json->full ? 0ull : length;
    }
    return made;
}

unsigned int cli_json_number_init(const CliJsonTreeTextLengthKindCfg *args)
{
    const char *text = args->text;
    const unsigned int made =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->json, .kind = CFG_JSON_NUMBER);
    if (made != CLI_JSON_NONE)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
        args->json->node[made].text =
            EMBED_CALL(cli_text_store, JsonTreeCtx, .json = args->json, .text = text, .length = length);
        args->json->node[made].length = args->json->full ? 0ull : length;
    }
    return made;
}

// the child of `parent` whose key is `name`, and the child before it into `before`, CLI_JSON_NONE where it is first
EMBED_INLINE unsigned int cli_on_key(const JsonTreeCtx *args)
{
    unsigned int *before = args->before;
    *before = CLI_JSON_NONE;
    if ((args->parent == CLI_JSON_NONE) || (args->json_read->node[args->parent].kind != CFG_JSON_OBJECT))
    {
        return CLI_JSON_NONE;
    }
    for (unsigned int child = args->json_read->node[args->parent].first; child != CLI_JSON_NONE;
         child = args->json_read->node[child].next)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->json_read->text + args->json_read->node[child].key,
                                         .other = args->name,
                                         .cap = CLI_CELLS_POOL}))
        {
            return child;
        }
        *before = child;
    }
    return CLI_JSON_NONE;
}

unsigned int cli_json_member(const CliJsonTreeObjectCfg *args)
{
    unsigned int before = CLI_JSON_NONE;
    return EMBED_CALL(cli_on_key, JsonTreeCtx, .json_read = args->json_read, .parent = args->object, .name = args->name,
                      .before = &before);
}

unsigned int cli_json_item(const CliJsonTreeArrayIndexCfg *args)
{
    if ((args->array == CLI_JSON_NONE) || (args->json_read->node[args->array].kind != CFG_JSON_ARRAY))
    {
        return CLI_JSON_NONE;
    }
    unsigned int child = args->json_read->node[args->array].first;
    for (unsigned int at = 0u; (child != CLI_JSON_NONE) && (at < args->index); at += 1u)
    {
        child = args->json_read->node[child].next;
    }
    return child;
}

const char *cli_json_string(const CliJsonTreeNodeLengthCfg *args)
{
    unsigned long long *length = args->length_pointer;
    if ((args->node == CLI_JSON_NONE) || (args->json_read->node[args->node].kind != CFG_JSON_STRING))
    {
        return NULL;
    }
    if (length != NULL)
    {
        *length = args->json_read->node[args->node].length;
    }
    return args->json_read->text + args->json_read->node[args->node].text;
}

int cli_json_is(const CliJsonTreeNodeKindCfg *args)
{
    return (args->node != CLI_JSON_NONE) && (args->json_read->node[args->node].kind == args->kind);
}

// `value` put after the last child of `parent`
EMBED_INLINE void cli_append_last(const JsonTreeCtx *args)
{
    CliJsonNode *const node = &args->json->node[args->parent];
    args->json->node[args->value].next = CLI_JSON_NONE;
    if (node->last == CLI_JSON_NONE)
    {
        node->first = args->value;
    }
    else
    {
        args->json->node[node->last].next = args->value;
    }
    node->last = args->value;
    node->count += 1u;
}

// `value` put as the member of `object` whose key starts at `key` in the tree's texts
EMBED_INLINE void cli_append_key(const JsonTreeCtx *args)
{
    unsigned long long key = args->key;
    if ((args->object == CLI_JSON_NONE) || (args->value == CLI_JSON_NONE) ||
        (args->json->node[args->object].kind != CFG_JSON_OBJECT))
    {
        return;
    }
    unsigned int before = CLI_JSON_NONE;
    const unsigned int had = EMBED_CALL(cli_on_key, JsonTreeCtx, .json_read = args->json, .parent = args->object,
                                        .name = args->json->text + key, .before = &before);
    args->json->node[args->value].key = key;
    if (had == CLI_JSON_NONE)
    {
        EMBED_CALL(cli_append_last, JsonTreeCtx, .json = args->json, .parent = args->object, .value = args->value);
        return;
    }
    args->json->node[args->value].next = args->json->node[had].next;
    if (before == CLI_JSON_NONE)
    {
        args->json->node[args->object].first = args->value;
    }
    else
    {
        args->json->node[before].next = args->value;
    }
    if (args->json->node[args->object].last == had)
    {
        args->json->node[args->object].last = args->value;
    }
}

void cli_json_set(const CliJsonTreeObjectNameCfg *args)
{
    unsigned int before = CLI_JSON_NONE;
    const unsigned int had = EMBED_CALL(cli_on_key, JsonTreeCtx, .json_read = args->json, .parent = args->object,
                                        .name = args->name, .before = &before);
    const unsigned long long key =
        (had != CLI_JSON_NONE)
            ? args->json->node[had].key
            : EMBED_CALL(cli_text_store, JsonTreeCtx, .json = args->json, .text = args->name,
                         .length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}));
    if (!args->json->full)
    {
        EMBED_CALL(cli_append_key, JsonTreeCtx, .json = args->json, .object = args->object, .key = key,
                   .value = args->value);
    }
}

void cli_json_remove(const CliJsonTreeObjectNameCfg *args)
{
    unsigned int before = CLI_JSON_NONE;
    const unsigned int had = EMBED_CALL(cli_on_key, JsonTreeCtx, .json_read = args->json, .parent = args->object,
                                        .name = args->name, .before = &before);
    if (had == CLI_JSON_NONE)
    {
        return;
    }
    if (before == CLI_JSON_NONE)
    {
        args->json->node[args->object].first = args->json->node[had].next;
    }
    else
    {
        args->json->node[before].next = args->json->node[had].next;
    }
    if (args->json->node[args->object].last == had)
    {
        args->json->node[args->object].last = before;
    }
    args->json->node[args->object].count -= 1u;
}

void cli_json_append(const CliJsonTreeArrayCfg *args)
{
    if ((args->array != CLI_JSON_NONE) && (args->value != CLI_JSON_NONE) &&
        (args->json->node[args->array].kind == CFG_JSON_ARRAY))
    {
        args->json->node[args->value].key = CLI_JSON_NO_KEY;
        EMBED_CALL(cli_append_last, JsonTreeCtx, .json = args->json, .parent = args->array, .value = args->value);
    }
}

void cli_json_sort(const CliJsonTreeObjectNameCfg *args)
{
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json,
                    .node = args->object, .kind = CFG_JSON_OBJECT))
    {
        return;
    }
    // each member taken off the list in turn and put back in its place by key, a list sorted in order
    unsigned int sorted = CLI_JSON_NONE;
    unsigned int child = args->json->node[args->object].first;
    while (child != CLI_JSON_NONE)
    {
        const unsigned int next = args->json->node[child].next;
        const char *const key = args->json->text + args->json->node[child].key;
        if ((sorted == CLI_JSON_NONE) ||
            (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = key,
                        .other_pointer = args->json->text + args->json->node[sorted].key) < 0))
        {
            args->json->node[child].next = sorted;
            sorted = child;
        }
        else
        {
            unsigned int before = sorted;
            while ((args->json->node[before].next != CLI_JSON_NONE) &&
                   (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg,
                               .one = args->json->text + args->json->node[args->json->node[before].next].key,
                               .other_pointer = key) <= 0))
            {
                before = args->json->node[before].next;
            }
            args->json->node[child].next = args->json->node[before].next;
            args->json->node[before].next = child;
        }
        child = next;
    }
    args->json->node[args->object].first = sorted;
    unsigned int last = sorted;
    while ((last != CLI_JSON_NONE) && (args->json->node[last].next != CLI_JSON_NONE))
    {
        last = args->json->node[last].next;
    }
    args->json->node[args->object].last = last;
}

unsigned int cli_json_copy(const CliJsonTreeCfg *args)
{
    if (args->node == CLI_JSON_NONE)
    {
        return CLI_JSON_NONE;
    }
    const unsigned int made = EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->json,
                                         .kind = args->json->node[args->node].kind);
    if (made == CLI_JSON_NONE)
    {
        return made;
    }
    // a text is never changed once stored, so a copy shares it
    args->json->node[made].text = args->json->node[args->node].text;
    args->json->node[made].length = args->json->node[args->node].length;
    for (unsigned int child = args->json->node[args->node].first; child != CLI_JSON_NONE;
         child = args->json->node[child].next)
    {
        const unsigned int copied =
            EMBED_CALL(cli_json_tree.json_copy, CliJsonTreeCfg, .json = args->json, .node = child);
        if (copied == CLI_JSON_NONE)
        {
            return CLI_JSON_NONE;
        }
        EMBED_CALL(cli_append_last, JsonTreeCtx, .json = args->json, .parent = made, .value = copied);
        args->json->node[copied].key = args->json->node[child].key;
    }
    return made;
}

unsigned int cli_json_copy_from(const CliJsonTreeNodeCfg *args)
{
    if (args->node == CLI_JSON_NONE)
    {
        return CLI_JSON_NONE;
    }
    const CliJsonNode *const src_node = &args->from->node[args->node];
    const unsigned int made =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->to, .kind = src_node->kind);
    if (made == CLI_JSON_NONE)
    {
        return made;
    }
    if ((src_node->kind == CFG_JSON_STRING) || (src_node->kind == CFG_JSON_NUMBER))
    {
        args->to->node[made].text = EMBED_CALL(cli_text_store, JsonTreeCtx, .json = args->to,
                                               .text = args->from->text + src_node->text, .length = src_node->length);
        args->to->node[made].length = args->to->full ? 0ull : src_node->length;
    }
    for (unsigned int child = src_node->first; child != CLI_JSON_NONE; child = args->from->node[child].next)
    {
        const unsigned int copied = EMBED_CALL(cli_json_tree_node.json_copy_from, CliJsonTreeNodeCfg, .to = args->to,
                                               .from = args->from, .node = child);
        if (copied == CLI_JSON_NONE)
        {
            return CLI_JSON_NONE;
        }
        if (src_node->kind == CFG_JSON_OBJECT)
        {
            const char *const key = args->from->text + args->from->node[child].key;
            const unsigned long long at =
                EMBED_CALL(cli_text_store, JsonTreeCtx, .json = args->to, .text = key,
                           .length = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}));
            if (args->to->full)
            {
                return CLI_JSON_NONE;
            }
            EMBED_CALL(cli_append_key, JsonTreeCtx, .json = args->to, .object = made, .key = at, .value = copied);
        }
        else
        {
            EMBED_CALL(cli_append_last, JsonTreeCtx, .json = args->to, .parent = made, .value = copied);
        }
    }
    return made;
}

int cli_json_same(const CliJsonTreeOneCfg *args)
{
    unsigned int other = args->other;
    if ((args->one == CLI_JSON_NONE) || (other == CLI_JSON_NONE))
    {
        return args->one == other;
    }
    const CliJsonNode *const left = &args->json_read->node[args->one];
    const CliJsonNode *const right = &args->other_json->node[other];
    if ((left->kind != right->kind) || (left->count != right->count))
    {
        return 0;
    }
    if ((left->kind == CFG_JSON_STRING) || (left->kind == CFG_JSON_NUMBER))
    {
        return (left->length == right->length) &&
               (memor.cmp(&(MemoriaCfg){.src = args->json_read->text + left->text,
                                        .other = args->other_json->text + right->text,
                                        .bytes = left->length}) == 0);
    }
    unsigned int pair = right->first;
    for (unsigned int child = left->first; child != CLI_JSON_NONE; child = args->json_read->node[child].next)
    {
        const unsigned int match =
            (left->kind == CFG_JSON_OBJECT)
                ? EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->other_json,
                             .object = other, .name = args->json_read->text + args->json_read->node[child].key)
                : pair;
        if (!EMBED_CALL(cli_json_tree_one.json_same, CliJsonTreeOneCfg, .json_read = args->json_read, .one = child,
                        .other_json = args->other_json, .other = match))
        {
            return 0;
        }
        pair = (pair != CLI_JSON_NONE) ? args->other_json->node[pair].next : pair;
    }
    return 1;
}

// the value at token `token` of `text` made a node, all under it with it
static unsigned int cli_convert(const JsonTreeCtx *args)
{
    CliJson *json = args->json;
    const char *text = args->text;
    const CfgJsonToken *const at = &args->tokens[args->token];
    const unsigned int made =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = at->kind);
    if (made == CLI_JSON_NONE)
    {
        return made;
    }
    if (at->kind == CFG_JSON_STRING)
    {
        // a string's letters as stored are at most its letters as written
        if (!EMBED_CALL(cli_json_tree_nodes.json_capacity, CliJsonTreeNodesCfg, .json = json, .nodes = 0u,
                        .text_u64 = (unsigned long long)(at->end - at->start) + 1ull))
        {
            return CLI_JSON_NONE;
        }
        const unsigned long long capacity = json->text_max - json->used;
        if (!cfg_json_string(text, at, json->text + json->used, (size_t)capacity))
        {
            json->full = 1;
            return CLI_JSON_NONE;
        }
        const unsigned long long length =
            cellul.len(&(CatenaFinitaCfg){.src = json->text + json->used, .cap = CLI_CELLS_POOL});
        json->node[made].text = json->used;
        json->node[made].length = length;
        json->used += length + 1ull;
    }
    else if (at->kind == CFG_JSON_NUMBER)
    {
        json->node[made].text = EMBED_CALL(cli_text_store, JsonTreeCtx, .json = json, .text = text + at->start,
                                           .length = at->end - at->start);
        json->node[made].length = at->end - at->start;
    }
    else if (at->kind == CFG_JSON_OBJECT)
    {
        unsigned int key = args->token + 1u;
        for (unsigned int member = 0u; member < at->count; member += 1u)
        {
            const unsigned int value = EMBED_CALL(cli_convert, JsonTreeCtx, .json = json, .text = text,
                                                  .tokens = args->tokens, .token = key + 1u);
            const unsigned int named =
                EMBED_CALL(cli_convert, JsonTreeCtx, .json = json, .text = text, .tokens = args->tokens, .token = key);
            if ((value == CLI_JSON_NONE) || (named == CLI_JSON_NONE))
            {
                return CLI_JSON_NONE;
            }
            // the key's node stands for its text alone, and is let go of
            EMBED_CALL(cli_append_key, JsonTreeCtx, .json = json, .object = made, .key = json->node[named].text,
                       .value = value);
            json->nodes -= (named == (json->nodes - 1u)) ? 1u : 0u;
            key = args->tokens[key + 1u].next;
        }
    }
    else if (at->kind == CFG_JSON_ARRAY)
    {
        unsigned int item = args->token + 1u;
        for (unsigned int each = 0u; each < at->count; each += 1u)
        {
            const unsigned int value =
                EMBED_CALL(cli_convert, JsonTreeCtx, .json = json, .text = text, .tokens = args->tokens, .token = item);
            if (value == CLI_JSON_NONE)
            {
                return CLI_JSON_NONE;
            }
            EMBED_CALL(cli_append_last, JsonTreeCtx, .json = json, .parent = made, .value = value);
            item = args->tokens[item].next;
        }
    }
    return made;
}

unsigned int cli_json_read(const CliJsonTreeTextSizeTokensCfg *args)
{
    CfgJsonParse parse = {0u, 0u, 0u, NULL};
    if (!cfg_json_parse_metadata(args->text, (size_t)args->size, args->tokens, args->tokens_max, &parse))
    {
        mmgr_line_put(args->err_msg, (parse.reason != NULL) ? parse.reason : "it is not JSON");
        mmgr_line_put(args->err_msg, " at line ");
        mmgr_line_decimal(args->err_msg, parse.line, 1u);
        mmgr_line_put(args->err_msg, " column ");
        mmgr_line_decimal(args->err_msg, parse.column, 1u);
        return CLI_JSON_NONE;
    }
    const unsigned int root = EMBED_CALL(cli_convert, JsonTreeCtx, .json = args->json, .text = args->text,
                                         .tokens = args->tokens, .token = 0u);
    if ((root == CLI_JSON_NONE) || args->json->full)
    {
        mmgr_line_put(args->err_msg, "it contains more than the tree it is read into fits");
        return CLI_JSON_NONE;
    }
    return root;
}

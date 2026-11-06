// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema_offer.c: what completes the word before the cursor in a JSON file, from the schemas that apply where it
// stands: at a key, each key they give the object that it does not contain yet, inside a string the rest of its name
// and outside one its name written with what its type opens; at a value, each value their `enum`, `const`, `default`
// and `examples` allow and the booleans and null their types take, inside a string only the strings
#include "../command_line/callback_cfg.h"
#include "schema_internal.h"

// the most keys and values offered, and the bytes they take
#define CLI_SCHEMA_OFFERED 4096u
#define CLI_SCHEMA_OFFERED_TEXT (1u << 20u)

// whether a letter goes in the word the cursor completes
static int offer_letter(const CliEachLetterCfg *args)
{
    unsigned int letter = args->letter;
    return EMBED_CALL(cli_text_table.alphanumeric, CliTextCfg, .letter = letter) || (letter == '_') ||
           (letter == '$') || (letter == '-') || (letter == '.') || (letter == '@') || (letter == '/');
}

typedef struct
{
    const char *text;
    const char *name;
    unsigned long long size;
    unsigned long long token;
    unsigned long long at;
    CliCompletions *items;
    const CliJson *json;
    unsigned int value;
    const char *detail;
    unsigned long long detail_size;
    unsigned int schema;
    const char *kind;
    const char *token_text;
    unsigned long long token_size;
} SchemaOfferCtx;
// whether the object the cursor is in contains the key `name` before it, by its raw text
EMBED_INLINE int offer_has_key(const SchemaOfferCtx *args)
{
    const CliSchemaSpot *const spot = &g_cli_schema_spot;
    for (unsigned int at = spot->keys_first; at < spot->keys_first + spot->keys; at += 1u)
    {
        if ((g_cli_schema_key_to[at] - g_cli_schema_key_from[at] == args->size) &&
            (memor.cmp(&(MemoriaCfg){.src = args->text + g_cli_schema_key_from[at],
                                     .other = args->name,
                                     .bytes = (size_t)args->size}) == 0))
        {
            return 1;
        }
    }
    return 0;
}

// the keys the schemas reached give the object that it does not contain yet, each once, put after the token
EMBED_INLINE void offer_keys(const SchemaOfferCtx *args)
{
    const char *text = args->text;
    CliCompletions *items = args->items;
    const CliJson *const json = &g_cli_schema_store->json;
    const unsigned long long base =
        EMBED_CALL(cli_builds_text_at.build_run_before, CliBuildsTextAtCfg, .text = text + args->token,
                   .at = args->at - args->token, .of = cli_build_word_letter) +
        args->token;
    static const char *s_offered[CLI_SCHEMA_OFFERED];
    unsigned int offered = 0u;
    for (unsigned int reached = 0u; reached < g_cli_schema_reached_count; reached += 1u)
    {
        const unsigned int properties = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json,
                                                   .map = g_cli_schema_reached[reached], .name = "properties");
        for (unsigned int one = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                           .node = properties, .kind = CFG_JSON_OBJECT)
                                    ? json->node[properties].first
                                    : CLI_JSON_NONE;
             one != CLI_JSON_NONE; one = json->node[one].next)
        {
            const char *const name = json->text + json->node[one].key;
            const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES});
            int seen = EMBED_CALL(offer_has_key, SchemaOfferCtx, .text = text, .name = name, .size = size);
            unsigned int earlier = 0u;
            while (!seen && (earlier < offered))
            {
                seen = (cellul.eq(&(CatenaFinitaCfg){.src = s_offered[earlier], .other = name, .cap = CLI_POOL_BYTES}));
                earlier += 1u;
            }
            if (seen || (offered == CLI_SCHEMA_OFFERED) || (size < base - args->token) ||
                (memor.cmp(&(MemoriaCfg){
                     .src = name, .other = text + args->token, .bytes = (size_t)(base - args->token)}) != 0))
            {
                continue;
            }
            s_offered[offered] = name;
            offered += 1u;
            static char s_detail[1u << 12u];
            VerbaLinea detail = {s_detail, sizeof(s_detail), 0ULL};
            EMBED_CALL(cli_schema_json_out_internal.detail, CliSchemaJsonOutCtx, .json = json, .schema = one,
                       .out = &detail);
            if (g_cli_schema_spot.in_string)
            {
                const unsigned long long typed = base - args->token;
                (void)EMBED_CALL(cli_edits_items.completion_append, CliEditsItemsCfg, .items = items,
                                 .label = name + typed, .label_size = size - typed, .kind = "field", .detail = s_detail,
                                 .detail_size = detail.at, .insert = name + typed, .insert_size = size - typed,
                                 .snippet = 0);
                continue;
            }
            unsigned long long kind_size = 0ULL;
            const char *const kind = EMBED_CALL(cli_schema_json_internal.string, CliSchemaJsonCtx, .json = json,
                                                .map = one, .name = "type", .size_pointer = &kind_size);
            const char *shape = "$0";
            if ((kind != NULL) && (kind_size == 6u) &&
                (memor.cmp(&(MemoriaCfg){.src = kind, .other = "string", .bytes = 6u}) == 0))
            {
                shape = "\"$0\"";
            }
            else if ((kind != NULL) && (kind_size == 6u) &&
                     (memor.cmp(&(MemoriaCfg){.src = kind, .other = "object", .bytes = 6u}) == 0))
            {
                shape = "{$0}";
            }
            else if ((kind != NULL) && (kind_size == 5u) &&
                     (memor.cmp(&(MemoriaCfg){.src = kind, .other = "array", .bytes = 5u}) == 0))
            {
                shape = "[$0]";
            }
            static char s_quoted[1u << 13u];
            VerbaLinea quoted = {s_quoted, sizeof(s_quoted), 0ULL};
            EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &quoted, .text = name,
                       .length = size);
            static char s_insert[1u << 14u];
            VerbaLinea insert = {s_insert, sizeof(s_insert), 0ULL};
            for (unsigned long long letter = 0ULL; letter < quoted.at; letter += 1u)
            {
                const int dollar = s_quoted[letter] == '$';
                mmgr_line_put_n(&insert, dollar ? "\\$" : (s_quoted + letter), dollar ? 2u : 1u);
            }
            mmgr_line_put(&insert, ": ");
            mmgr_line_put(&insert, shape);
            (void)EMBED_CALL(cli_edits_items.completion_append, CliEditsItemsCfg, .items = items, .label = name,
                             .label_size = size, .kind = "field", .detail = s_detail, .detail_size = detail.at,
                             .insert = s_insert, .insert_size = insert.at, .snippet = 1);
        }
    }
}

// The values offered: each its text and detail, listed once
typedef struct
{
    unsigned int at[CLI_SCHEMA_OFFERED];
    unsigned int size[CLI_SCHEMA_OFFERED];
    unsigned int detail_at[CLI_SCHEMA_OFFERED];
    unsigned int detail_size[CLI_SCHEMA_OFFERED];
    unsigned int count;
    char text[CLI_SCHEMA_OFFERED_TEXT];
    unsigned int used;
} CliSchemaOffered;

static CliSchemaOffered s_values;

// a value offered, a string's letters inside a string and its JSON outside one, with `detail`, where it is not offered
static void offer_value(const SchemaOfferCtx *args)
{
    const CliJson *json = args->json;
    static char s_text[1u << 14u];
    VerbaLinea line = {s_text, sizeof(s_text), 0ULL};
    if (g_cli_schema_spot.in_string)
    {
        unsigned long long size = 0ULL;
        const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = json, .node = args->value, .length_pointer = &size);
        if (text == NULL)
        {
            return;
        }
        mmgr_line_put_n(&line, text, size);
    }
    else
    {
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = args->value,
                   .out = &line, .indent = 0u, .sorted = 1);
    }
    if (!mmgr_line_ok(&line) || (s_values.count == CLI_SCHEMA_OFFERED) ||
        (line.at + args->detail_size > CLI_SCHEMA_OFFERED_TEXT - s_values.used))
    {
        return;
    }
    for (unsigned int at = 0u; at < s_values.count; at += 1u)
    {
        if ((s_values.size[at] == line.at) &&
            (memor.cmp(&(MemoriaCfg){.src = s_values.text + s_values.at[at], .other = s_text, .bytes = line.at}) == 0))
        {
            return;
        }
    }
    s_values.at[s_values.count] = s_values.used;
    s_values.size[s_values.count] = (unsigned int)line.at;
    memor.cpy(&(MemoriaCfg){.dst = s_values.text + s_values.used, .src = s_text, .bytes = line.at});
    s_values.used += (unsigned int)line.at;
    s_values.detail_at[s_values.count] = s_values.used;
    s_values.detail_size[s_values.count] = (unsigned int)args->detail_size;
    memor.cpy(
        &(MemoriaCfg){.dst = s_values.text + s_values.used, .src = args->detail, .bytes = (size_t)args->detail_size});
    s_values.used += (unsigned int)args->detail_size;
    s_values.count += 1u;
}

// whether a schema's `type` names `kind`
EMBED_INLINE int offer_type(const SchemaOfferCtx *args)
{
    const CliJson *json = args->json;
    const char *kind = args->kind;
    const unsigned int types =
        EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = args->schema, .name = "type");
    unsigned long long size = 0ULL;
    const char *const single = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                          .json_read = json, .node = types, .length_pointer = &size);
    if (single != NULL)
    {
        return (cellul.len(&(CatenaFinitaCfg){.src = kind, .cap = CLI_POOL_BYTES}) == size) &&
               (memor.cmp(&(MemoriaCfg){.src = single, .other = kind, .bytes = (size_t)size}) == 0);
    }
    for (unsigned int one = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                       .node = types, .kind = CFG_JSON_ARRAY)
                                ? json->node[types].first
                                : CLI_JSON_NONE;
         one != CLI_JSON_NONE; one = json->node[one].next)
    {
        const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = json, .node = one, .length_pointer = &size);
        if ((text != NULL) && (cellul.len(&(CatenaFinitaCfg){.src = kind, .cap = CLI_POOL_BYTES}) == size) &&
            (memor.cmp(&(MemoriaCfg){.src = text, .other = kind, .bytes = (size_t)size}) == 0))
        {
            return 1;
        }
    }
    return 0;
}

// the values the schemas reached allow, put after `token`
EMBED_INLINE void offer_values(const SchemaOfferCtx *args)
{
    const char *token = args->token_text;
    unsigned long long token_size = args->token_size;
    CliCompletions *items = args->items;
    CliJson *const json = &g_cli_schema_store->json;
    s_values.count = 0u;
    s_values.used = 0u;
    for (unsigned int reached = 0u; reached < g_cli_schema_reached_count; reached += 1u)
    {
        const unsigned int one = g_cli_schema_reached[reached];
        static char s_detail[1u << 12u];
        VerbaLinea detail = {s_detail, sizeof(s_detail), 0ULL};
        EMBED_CALL(cli_schema_json_out_internal.detail, CliSchemaJsonOutCtx, .json = json, .schema = one,
                   .out = &detail);
        const unsigned int choices =
            EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = one, .name = "enum");
        for (unsigned int at = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                          .node = choices, .kind = CFG_JSON_ARRAY)
                                   ? json->node[choices].first
                                   : CLI_JSON_NONE;
             at != CLI_JSON_NONE; at = json->node[at].next)
        {
            EMBED_CALL(offer_value, SchemaOfferCtx, .json = json, .value = at, .detail = s_detail,
                       .detail_size = detail.at);
        }
        static const char *const s_input[2] = {"const", "default"};
        for (unsigned int word_text = 0u; word_text < 2u; word_text += 1u)
        {
            const unsigned int supplied = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json,
                                                     .map = one, .name = s_input[word_text]);
            if (supplied != CLI_JSON_NONE)
            {
                EMBED_CALL(offer_value, SchemaOfferCtx, .json = json, .value = supplied, .detail = s_detail,
                           .detail_size = detail.at);
            }
        }
        const unsigned int examples =
            EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = one, .name = "examples");
        for (unsigned int at = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                          .node = examples, .kind = CFG_JSON_ARRAY)
                                   ? json->node[examples].first
                                   : CLI_JSON_NONE;
             at != CLI_JSON_NONE; at = json->node[at].next)
        {
            EMBED_CALL(offer_value, SchemaOfferCtx, .json = json, .value = at, .detail = s_detail,
                       .detail_size = detail.at);
        }
        static const CfgJsonKind s_kinds[3] = {CFG_JSON_TRUE, CFG_JSON_FALSE, CFG_JSON_NULL};
        for (unsigned int word_text = 0u; word_text < 3u; word_text += 1u)
        {
            if (!EMBED_CALL(offer_type, SchemaOfferCtx, .json = json, .schema = one,
                            .kind = (word_text < 2u) ? "boolean" : "null") ||
                g_cli_schema_spot.in_string)
            {
                continue;
            }
            const unsigned int made =
                EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = s_kinds[word_text]);
            if (made != CLI_JSON_NONE)
            {
                EMBED_CALL(offer_value, SchemaOfferCtx, .json = json, .value = made, .detail = s_detail,
                           .detail_size = detail.at);
            }
        }
    }
    for (unsigned int at = 0u; at < s_values.count; at += 1u)
    {
        EMBED_CALL(cli_builds_items.build_relative, CliBuildsItemsCfg, .items = items, .token = token,
                   .token_size = token_size, .value = s_values.text + s_values.at[at], .value_size = s_values.size[at],
                   .kind = "value", .detail = s_values.text + s_values.detail_at[at],
                   .detail_size = s_values.detail_size[at]);
    }
}

void cli_schema_complete(const CliSchemaLineCfg *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned int line = args->line;
    unsigned int col = args->col;
    const unsigned long long at = EMBED_CALL(cli_builds_path.build_byte_at, CliBuildsPathCfg, .text = text,
                                             .size = size, .line = line, .col = col);
    if (!cli_schema_lock())
    {
        return;
    }
    if (!EMBED_CALL(cli_schema_read_text_internal.spot, CliSchemaReadTextCtx, .text = text, .size = at))
    {
        if (g_cli_schema_read != NULL)
        {
            g_cli_guard->persistent_buf_release(g_cli_schema_read->node, g_cli_schema_owner);
            g_cli_schema_read->node = NULL;
            g_cli_schema_read->member = NULL;
            g_cli_schema_read->pool = NULL;
            g_cli_schema_read->nodes_max = 0u;
            g_cli_schema_read->members_max = 0u;
            g_cli_schema_read->pool_max = 0u;
        }
        if (g_cli_schema_store != NULL)
        {
            g_cli_guard->persistent_buf_release(g_cli_schema_store->json.node,
                                                mmgr_owner_of_buf(cli_pool_series_cellularum(), g_cli_schema_store->json.node));
            EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&g_cli_schema_store->json),
                       .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        }
        g_cli_guard->persistent_buf_release(g_cli_schema_store, g_cli_code_owner);
        g_cli_guard->persistent_buf_release(g_cli_schema_read, g_cli_code_owner);
        g_cli_schema_store = NULL;
        g_cli_schema_read = NULL;
        return;
    }
    EMBED_CALL(cli_schema_read_text_internal.read, CliSchemaReadTextCtx, .text = text, .size = size,
               .read_out = g_cli_schema_read);
    char key[CLI_PATH_MAX_LEN];
    int named = 0;
    unsigned long long from = 0ULL;
    unsigned long long to = 0ULL;
    if (EMBED_CALL(cli_schema_read_internal.get, CliSchemaReadCtx, .path = args->path, .read = g_cli_schema_read,
                   .key = key, .named = &named, .from = &from, .to = &to))
    {
        EMBED_CALL(cli_schema_store_internal.store_begin, CliSchemaStoreCtx, .store = g_cli_schema_store);
        const unsigned int schema =
            EMBED_CALL(cli_schema_store_internal.load, CliSchemaStoreCtx, .store = g_cli_schema_store, .key_read = key);
        if (schema != CLI_SCHEMA_NONE)
        {
            const unsigned long long token = EMBED_CALL(cli_builds_text_at.build_run_before, CliBuildsTextAtCfg,
                                                        .text = text, .at = at, .of = offer_letter);
            EMBED_CALL(cli_schema_text_base_internal.reached_get, CliSchemaTextBaseCtx, .text = text, .schema = schema,
                       .base = key);
            if (g_cli_schema_spot.at_key)
            {
                EMBED_CALL(offer_keys, SchemaOfferCtx, .text = text, .token = token, .at = at, .items = args->items);
            }
            else
            {
                EMBED_CALL(offer_values, SchemaOfferCtx, .token_text = text + token, .token_size = at - token,
                           .items = args->items);
            }
        }
    }
    if (g_cli_schema_read != NULL)
    {
        g_cli_guard->persistent_buf_release(g_cli_schema_read->node, g_cli_schema_owner);
        g_cli_schema_read->node = NULL;
        g_cli_schema_read->member = NULL;
        g_cli_schema_read->pool = NULL;
        g_cli_schema_read->nodes_max = 0u;
        g_cli_schema_read->members_max = 0u;
        g_cli_schema_read->pool_max = 0u;
    }
    if (g_cli_schema_store != NULL)
    {
        g_cli_guard->persistent_buf_release(g_cli_schema_store->json.node,
                                            mmgr_owner_of_buf(cli_pool_series_cellularum(), g_cli_schema_store->json.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&g_cli_schema_store->json),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    }
    g_cli_guard->persistent_buf_release(g_cli_schema_store, g_cli_code_owner);
    g_cli_guard->persistent_buf_release(g_cli_schema_read, g_cli_code_owner);
    g_cli_schema_store = NULL;
    g_cli_schema_read = NULL;
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema_complete.c: what completes the word before the cursor in a JSON file: where the cursor stands, the steps to
// the value it is in, whether it is at a key and inside a string; the schemas that apply there, `$ref`s followed and
// `allOf`, `anyOf`, `oneOf` and `if`'s branches taken together; and from them the keys the object does not contain yet,
// each written with what its type opens, or the values the schema allows. A text nested deeper than CLI_SCHEMA_FRAMES
// or containing more keys open than CLI_SCHEMA_OPEN_KEYS is read as far as they fit; the schemas past
// CLI_SCHEMA_REACHED are not offered
#include "../text/text.h"
#include "schema_internal.h"

// the deepest frames read
#define CLI_SCHEMA_FRAMES 512u

// What stands open: an object, its key, whether its colon was passed and its first key read, or an array and its index
typedef struct
{
    int object;
    int has_key;
    unsigned long long key_from;
    unsigned long long key_to;
    int colon;
    unsigned int keys_first;
    unsigned int index;
} CliSchemaFrame;

static CliSchemaFrame s_frame[CLI_SCHEMA_FRAMES];
unsigned long long g_cli_schema_key_from[CLI_SCHEMA_OPEN_KEYS];
unsigned long long g_cli_schema_key_to[CLI_SCHEMA_OPEN_KEYS];
static unsigned int s_keys;
CliSchemaSpot g_cli_schema_spot;

typedef struct
{
    int key;
    unsigned long long from;
    unsigned long long to;
    unsigned int index;
    const char *text;
    unsigned long long size;
    unsigned long long *at;
    unsigned int frames;
    unsigned int schema;
    const char *base;
    unsigned int at_u32;
    unsigned int depth;
} SchemaCompleteCtx;
// a step put on the spot's steps where it fits
EMBED_INLINE void complete_step(const SchemaCompleteCtx *args)
{
    if (g_cli_schema_spot.steps <= CLI_SCHEMA_FRAMES)
    {
        g_cli_schema_spot.step[g_cli_schema_spot.steps] = (CliSchemaStep){args->key, args->from, args->to, args->index};
        g_cli_schema_spot.steps += 1u;
    }
}

// a string passed from `at`, its raw text a key where the object's colon was not passed: past it, or 0 where it is not
// closed and the cursor stands in it
EMBED_INLINE int complete_string(const SchemaCompleteCtx *args)
{
    unsigned long long *at = args->at;
    unsigned long long end = *at + 1u;
    while (end < args->size)
    {
        if (args->text[end] == '\\')
        {
            end += 2u;
            continue;
        }
        if (args->text[end] == '"')
        {
            if ((args->frames > 0u) && s_frame[args->frames - 1u].object && !s_frame[args->frames - 1u].colon)
            {
                s_frame[args->frames - 1u].has_key = 1;
                s_frame[args->frames - 1u].key_from = *at + 1u;
                s_frame[args->frames - 1u].key_to = end;
            }
            *at = end + 1u;
            return 1;
        }
        end += 1u;
    }
    return 0;
}

int cli_schema_spot(const CliSchemaReadTextCtx *args)
{
    unsigned int frames = 0u;
    s_keys = 0u;
    g_cli_schema_spot.steps = 0u;
    g_cli_schema_spot.in_string = 0;
    for (unsigned long long at = 0ULL; at < args->size;)
    {
        unsigned long long end = 0ULL;
        if (EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = args->text + at,
                       .size = args->size - at, .prefix = "//"))
        {
            at = EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = args->text + at,
                            .length_value = args->size - at, .required = "\n", .size = 1u, .at_pointer = &end)
                     ? at + end
                     : args->size;
            continue;
        }
        if (EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = args->text + at,
                       .size = args->size - at, .prefix = "/*"))
        {
            at = EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = args->text + at + 2u,
                            .length_value = args->size - at - 2u, .required = "*/", .size = 2u, .at_pointer = &end)
                     ? at + 2u + end + 2u
                     : args->size;
            continue;
        }
        const char byte = args->text[at];
        CliSchemaFrame *const last = (frames > 0u) ? &s_frame[frames - 1u] : NULL;
        if (byte == '"')
        {
            if (!EMBED_CALL(complete_string, SchemaCompleteCtx, .text = args->text, .size = args->size, .at = &at,
                            .frames = frames))
            {
                g_cli_schema_spot.in_string = 1;
                break;
            }
            continue;
        }
        if ((byte == ':') && (last != NULL) && last->object)
        {
            last->colon = 1;
        }
        else if ((byte == ',') && (last != NULL) && last->object)
        {
            if (last->has_key && (s_keys < CLI_SCHEMA_OPEN_KEYS))
            {
                g_cli_schema_key_from[s_keys] = last->key_from;
                g_cli_schema_key_to[s_keys] = last->key_to;
                s_keys += 1u;
            }
            last->has_key = 0;
            last->colon = 0;
        }
        else if ((byte == ',') && (last != NULL))
        {
            last->index += 1u;
        }
        else if (((byte == '{') || (byte == '[')) && (frames < CLI_SCHEMA_FRAMES))
        {
            if ((last != NULL) && last->object && last->has_key && last->colon)
            {
                EMBED_CALL(complete_step, SchemaCompleteCtx, .key = 1, .from = last->key_from, .to = last->key_to,
                           .index = 0u);
            }
            else if ((last != NULL) && !last->object)
            {
                EMBED_CALL(complete_step, SchemaCompleteCtx, .key = 0, .from = 0ULL, .to = 0ULL, .index = last->index);
            }
            s_frame[frames] = (CliSchemaFrame){byte == '{', 0, 0ULL, 0ULL, 0, s_keys, 0u};
            frames += 1u;
        }
        else if (((byte == '}') || (byte == ']')) && (frames > 0u))
        {
            frames -= 1u;
            s_keys = s_frame[frames].keys_first;
            g_cli_schema_spot.steps -= ((frames > 0u) && (g_cli_schema_spot.steps > 0u)) ? 1u : 0u;
        }
        at += 1u;
    }
    if (frames == 0u)
    {
        return 0;
    }
    const CliSchemaFrame *const last = &s_frame[frames - 1u];
    if (last->object)
    {
        if (last->colon)
        {
            EMBED_CALL(complete_step, SchemaCompleteCtx, .key = 1, .from = last->has_key ? last->key_from : 0ULL,
                       .to = last->has_key ? last->key_to : 0ULL, .index = 0u);
        }
        g_cli_schema_spot.at_key = !last->colon;
        g_cli_schema_spot.keys_first = last->keys_first;
        g_cli_schema_spot.keys = s_keys - last->keys_first;
        return 1;
    }
    EMBED_CALL(complete_step, SchemaCompleteCtx, .key = 0, .from = 0ULL, .to = 0ULL, .index = last->index);
    g_cli_schema_spot.at_key = 0;
    g_cli_schema_spot.keys_first = 0u;
    g_cli_schema_spot.keys = 0u;
    return 1;
}

// the schemas reached
unsigned int g_cli_schema_reached[CLI_SCHEMA_REACHED];
unsigned int g_cli_schema_reached_count;

// the schemas that apply at the end of the steps from `at` of `schema`, read in `base`, put among those reached
static void complete_schemas_at(const SchemaCompleteCtx *args)
{
    const char *text = args->text;
    const char *base = args->base;
    CliSchemaStore *const store = g_cli_schema_store;
    const CliJson *const json = &store->json;
    if ((args->depth > CLI_SCHEMA_DEEPEST) || (args->schema == CLI_SCHEMA_NONE) ||
        !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = args->schema,
                    .kind = CFG_JSON_OBJECT))
    {
        return;
    }
    unsigned long long size = 0ULL;
    const char *const reference = EMBED_CALL(cli_schema_json_internal.string, CliSchemaJsonCtx, .json = json,
                                             .map = args->schema, .name = "$ref", .size_pointer = &size);
    // the base a reference is read in, a cell retained while the schemas under it are reached
    char *const doc =
        (reference != NULL) ? g_cli_cells->temporary_buf_alloc(CLI_PATH_MAX_LEN, &g_cli_schema_cells) : NULL;
    if (doc != NULL)
    {
        const unsigned int target = EMBED_CALL(cli_schema_size_internal.resolve, CliSchemaSizeCtx, .store = store,
                                               .base = base, .reference = reference, .size = size, .key = doc);
        if (target != CLI_SCHEMA_NONE)
        {
            EMBED_CALL(complete_schemas_at, SchemaCompleteCtx, .text = text, .schema = target, .base = doc,
                       .at_u32 = args->at_u32, .depth = args->depth + 1u);
        }
    }
    g_cli_cells->persistent_buf_release(doc, &g_cli_schema_cells);
    static const char *const s_words[5] = {"allOf", "anyOf", "oneOf", "then", "else"};
    for (unsigned int word_text = 0u; word_text < 5u; word_text += 1u)
    {
        const unsigned int all = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json,
                                            .map = args->schema, .name = s_words[word_text]);
        if ((word_text < 3u) && EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                           .node = all, .kind = CFG_JSON_ARRAY))
        {
            for (unsigned int one = json->node[all].first; one != CLI_JSON_NONE; one = json->node[one].next)
            {
                EMBED_CALL(complete_schemas_at, SchemaCompleteCtx, .text = text, .schema = one, .base = base,
                           .at_u32 = args->at_u32, .depth = args->depth + 1u);
            }
        }
        else if ((word_text >= 3u) && (all != CLI_JSON_NONE))
        {
            EMBED_CALL(complete_schemas_at, SchemaCompleteCtx, .text = text, .schema = all, .base = base,
                       .at_u32 = args->at_u32, .depth = args->depth + 1u);
        }
    }
    if (args->at_u32 == g_cli_schema_spot.steps)
    {
        if (g_cli_schema_reached_count < CLI_SCHEMA_REACHED)
        {
            g_cli_schema_reached[g_cli_schema_reached_count] = args->schema;
            g_cli_schema_reached_count += 1u;
        }
        return;
    }
    const CliSchemaStep *const step = &g_cli_schema_spot.step[args->at_u32];
    if (step->key)
    {
        char key[1u << 12u];
        const unsigned long long key_size =
            (step->to - step->from < sizeof(key)) ? step->to - step->from : sizeof(key) - 1u;
        memor.cpy(&(MemoriaCfg){.dst = key, .src = text + step->from, .bytes = (size_t)key_size});
        key[key_size] = '\0';
        int any = 0;
        const unsigned int named =
            EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json,
                       .map = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json,
                                         .map = args->schema, .name = "properties"),
                       .name = key);
        if (named != CLI_JSON_NONE)
        {
            any = 1;
            EMBED_CALL(complete_schemas_at, SchemaCompleteCtx, .text = text, .schema = named, .base = base,
                       .at_u32 = args->at_u32 + 1u, .depth = args->depth + 1u);
        }
        const unsigned int patterns = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json,
                                                 .map = args->schema, .name = "patternProperties");
        for (unsigned int one = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                           .node = patterns, .kind = CFG_JSON_OBJECT)
                                    ? json->node[patterns].first
                                    : CLI_JSON_NONE;
             one != CLI_JSON_NONE; one = json->node[one].next)
        {
            const char *const pattern = json->text + json->node[one].key;
            if (EMBED_CALL(cli_schema_text_internal.matches, CliSchemaTextCtx, .pattern = pattern,
                           .pattern_size = cellul.len(&(CatenaFinitaCfg){.src = pattern, .cap = CLI_CELLS_POOL}),
                           .text = key, .size = key_size) == 1)
            {
                any = 1;
                EMBED_CALL(complete_schemas_at, SchemaCompleteCtx, .text = text, .schema = one, .base = base,
                           .at_u32 = args->at_u32 + 1u, .depth = args->depth + 1u);
            }
        }
        const unsigned int additional = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json,
                                                   .map = args->schema, .name = "additionalProperties");
        if (!any && EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                               .node = additional, .kind = CFG_JSON_OBJECT))
        {
            EMBED_CALL(complete_schemas_at, SchemaCompleteCtx, .text = text, .schema = additional, .base = base,
                       .at_u32 = args->at_u32 + 1u, .depth = args->depth + 1u);
        }
        return;
    }
    unsigned int tuple = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = args->schema,
                                    .name = "prefixItems");
    const unsigned int items =
        EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = args->schema, .name = "items");
    tuple = (tuple != CLI_JSON_NONE) ? tuple
                                     : (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                   .json_read = json, .node = items, .kind = CFG_JSON_ARRAY)
                                            ? items
                                            : CLI_JSON_NONE);
    const unsigned int positioned = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                               .json_read = json, .array = tuple, .index = step->index);
    if (positioned != CLI_JSON_NONE)
    {
        EMBED_CALL(complete_schemas_at, SchemaCompleteCtx, .text = text, .schema = positioned, .base = base,
                   .at_u32 = args->at_u32 + 1u, .depth = args->depth + 1u);
        return;
    }
    const unsigned int rest = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                         .node = items, .kind = CFG_JSON_OBJECT)
                                  ? items
                                  : EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json,
                                               .map = args->schema, .name = "additionalItems");
    if (rest != CLI_JSON_NONE)
    {
        EMBED_CALL(complete_schemas_at, SchemaCompleteCtx, .text = text, .schema = rest, .base = base,
                   .at_u32 = args->at_u32 + 1u, .depth = args->depth + 1u);
    }
}

void cli_schema_detail(const CliSchemaJsonOutCtx *args)
{
    unsigned long long size = 0ULL;
    const char *description = EMBED_CALL(cli_schema_json_internal.string, CliSchemaJsonCtx, .json = args->json,
                                         .map = args->schema, .name = "description", .size_pointer = &size);
    description = (description != NULL)
                      ? description
                      : EMBED_CALL(cli_schema_json_internal.string, CliSchemaJsonCtx, .json = args->json,
                                   .map = args->schema, .name = "title", .size_pointer = &size);
    if (description != NULL)
    {
        unsigned long long line =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = description, .at = 0ULL, .size = size).stop;
        line -= ((line > 0u) && (description[line - 1u] == '\r')) ? 1u : 0u;
        mmgr_line_put_n(args->out, description, line);
        return;
    }
    const unsigned int kind = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = args->json,
                                         .map = args->schema, .name = "type");
    if (kind == CLI_JSON_NONE)
    {
        return;
    }
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                        .json_read = args->json, .node = kind, .length_pointer = &size);
    if (text != NULL)
    {
        mmgr_line_put_n(args->out, text, size);
        return;
    }
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = args->json, .node = kind,
               .out = args->out, .indent = 0u, .sorted = 1);
}

void cli_schema_reached_get(const CliSchemaTextBaseCtx *args)
{
    g_cli_schema_reached_count = 0u;
    EMBED_CALL(complete_schemas_at, SchemaCompleteCtx, .text = args->text, .schema = args->schema, .base = args->base,
               .at_u32 = 0u, .depth = 0u);
}

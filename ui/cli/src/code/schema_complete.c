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
    char byte;
    unsigned int *frames_out;
    CliSchemaFrame *frame;
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

// the place past a comment beginning at `from` of `text`, the line's end for a line comment and past its close for a
// block comment, the text's end where it is not closed; `from` itself where no comment begins there
EMBED_INLINE unsigned long long complete_comment_end(const SchemaCompleteCtx *args)
{
    const char *const text = args->text + args->from;
    const unsigned long long size = args->size - args->from;
    unsigned long long end = 0ULL;
    if (EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = text, .size = size, .prefix = "//"))
    {
        return EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = text, .length_value = size, .required = "\n",
                          .size = 1u, .at_pointer = &end)
                   ? args->from + end
                   : args->size;
    }
    if (EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = text, .size = size, .prefix = "/*"))
    {
        return EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = text + 2u, .length_value = size - 2u,
                          .required = "*/", .size = 2u, .at_pointer = &end)
                   ? args->from + 2u + end + 2u
                   : args->size;
    }
    return args->from;
}

// a key of the object `frame` closed by a comma: recorded among the keys open where it fits, and the frame set to read
// the next
EMBED_INLINE void complete_key_closed(const SchemaCompleteCtx *args)
{
    CliSchemaFrame *const last = args->frame;
    if (last->has_key && (s_keys < CLI_SCHEMA_OPEN_KEYS))
    {
        g_cli_schema_key_from[s_keys] = last->key_from;
        g_cli_schema_key_to[s_keys] = last->key_to;
        s_keys += 1u;
    }
    last->has_key = 0;
    last->colon = 0;
}

// the step into a frame opened inside the frame `frame`, NULL at the top, put on the spot's steps: the key whose value
// it is, or the index it stands at
EMBED_INLINE void complete_opened(const SchemaCompleteCtx *args)
{
    const CliSchemaFrame *const last = args->frame;
    if ((last != NULL) && last->object && last->has_key && last->colon)
    {
        EMBED_CALL(complete_step, SchemaCompleteCtx, .key = 1, .from = last->key_from, .to = last->key_to, .index = 0u);
    }
    else if ((last != NULL) && !last->object)
    {
        EMBED_CALL(complete_step, SchemaCompleteCtx, .key = 0, .from = 0ULL, .to = 0ULL, .index = last->index);
    }
}

// the frames changed by `byte` read outside a string, `frames_out` the count of them open: a colon passed, a key or an
// index closed by a comma, a frame opened by `{` or `[` where one fits, or one closed by `}` or `]`
EMBED_INLINE void complete_punctuation(const SchemaCompleteCtx *args)
{
    unsigned int *const frames = args->frames_out;
    const char byte = args->byte;
    CliSchemaFrame *const last = (*frames > 0u) ? &s_frame[*frames - 1u] : NULL;
    if ((byte == ':') && (last != NULL) && last->object)
    {
        last->colon = 1;
    }
    else if ((byte == ',') && (last != NULL) && last->object)
    {
        EMBED_CALL(complete_key_closed, SchemaCompleteCtx, .frame = last);
    }
    else if ((byte == ',') && (last != NULL))
    {
        last->index += 1u;
    }
    else if (((byte == '{') || (byte == '[')) && (*frames < CLI_SCHEMA_FRAMES))
    {
        EMBED_CALL(complete_opened, SchemaCompleteCtx, .frame = last);
        s_frame[*frames] = (CliSchemaFrame){byte == '{', 0, 0ULL, 0ULL, 0, s_keys, 0u};
        *frames += 1u;
    }
    else if (((byte == '}') || (byte == ']')) && (*frames > 0u))
    {
        *frames -= 1u;
        s_keys = s_frame[*frames].keys_first;
        g_cli_schema_spot.steps -= ((*frames > 0u) && (g_cli_schema_spot.steps > 0u)) ? 1u : 0u;
    }
}

int cli_schema_spot(const CliSchemaReadTextCtx *args)
{
    unsigned int frames = 0u;
    s_keys = 0u;
    g_cli_schema_spot.steps = 0u;
    g_cli_schema_spot.in_string = 0;
    for (unsigned long long at = 0ULL; at < args->size;)
    {
        const unsigned long long past =
            EMBED_CALL(complete_comment_end, SchemaCompleteCtx, .text = args->text, .size = args->size, .from = at);
        if (past != at)
        {
            at = past;
            continue;
        }
        const char byte = args->text[at];
        if (byte != '"')
        {
            EMBED_CALL(complete_punctuation, SchemaCompleteCtx, .byte = byte, .frames_out = &frames);
            at += 1u;
            continue;
        }
        if (!EMBED_CALL(complete_string, SchemaCompleteCtx, .text = args->text, .size = args->size, .at = &at,
                        .frames = frames))
        {
            g_cli_schema_spot.in_string = 1;
            break;
        }
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

static void complete_schemas_at(const SchemaCompleteCtx *args);

// the schemas reached through the `$ref` of `schema`, read in the base it names
EMBED_INLINE void complete_reference_reached(const SchemaCompleteCtx *args)
{
    CliSchemaStore *const store = g_cli_schema_store;
    unsigned long long size = 0ULL;
    const char *const reference = EMBED_CALL(cli_schema_json_internal.string, CliSchemaJsonCtx, .json = &store->json,
                                             .map = args->schema, .name = "$ref", .size_pointer = &size);
    // the base a reference is read in, a cell retained while the schemas under it are reached
    char *const doc =
        (reference != NULL) ? g_cli_guard->temporary_buf_alloc(CLI_PATH_MAX_LEN, g_cli_schema_owner) : NULL;
    if (doc != NULL)
    {
        const unsigned int target = EMBED_CALL(cli_schema_size_internal.resolve, CliSchemaSizeCtx, .store = store,
                                               .base = args->base, .reference = reference, .size = size, .key = doc);
        if (target != CLI_SCHEMA_NONE)
        {
            EMBED_CALL(complete_schemas_at, SchemaCompleteCtx, .text = args->text, .schema = target, .base = doc,
                       .at_u32 = args->at_u32, .depth = args->depth + 1u);
        }
    }
    g_cli_guard->persistent_buf_release(doc, g_cli_schema_owner);
}

// the schemas reached through the branches of `schema` taken together: each of its `allOf`, `anyOf` and `oneOf`, and
// its `then` and its `else`
EMBED_INLINE void complete_branches_reached(const SchemaCompleteCtx *args)
{
    const CliJson *const json = &g_cli_schema_store->json;
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
                EMBED_CALL(complete_schemas_at, SchemaCompleteCtx, .text = args->text, .schema = one,
                           .base = args->base, .at_u32 = args->at_u32, .depth = args->depth + 1u);
            }
        }
        else if ((word_text >= 3u) && (all != CLI_JSON_NONE))
        {
            EMBED_CALL(complete_schemas_at, SchemaCompleteCtx, .text = args->text, .schema = all, .base = args->base,
                       .at_u32 = args->at_u32, .depth = args->depth + 1u);
        }
    }
}

// the schemas reached past the key step `at_u32` of `schema`: its property of the key's name and each of its
// `patternProperties` the name matches, or its `additionalProperties` where none of them names it
EMBED_INLINE void complete_key_reached(const SchemaCompleteCtx *args)
{
    const CliJson *const json = &g_cli_schema_store->json;
    const CliSchemaStep *const step = &g_cli_schema_spot.step[args->at_u32];
    char key[1u << 12u];
    const unsigned long long key_size =
        (step->to - step->from < sizeof(key)) ? step->to - step->from : sizeof(key) - 1u;
    memor.cpy(&(MemoriaCfg){.dst = key, .src = args->text + step->from, .bytes = (size_t)key_size});
    key[key_size] = '\0';
    int any = 0;
    const unsigned int named = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json,
                                          .map = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx,
                                                            .json = json, .map = args->schema, .name = "properties"),
                                          .name = key);
    if (named != CLI_JSON_NONE)
    {
        any = 1;
        EMBED_CALL(complete_schemas_at, SchemaCompleteCtx, .text = args->text, .schema = named, .base = args->base,
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
                       .pattern_size = cellul.len(&(CatenaFinitaCfg){.src = pattern, .cap = CLI_POOL_BYTES}),
                       .text = key, .size = key_size) == 1)
        {
            any = 1;
            EMBED_CALL(complete_schemas_at, SchemaCompleteCtx, .text = args->text, .schema = one, .base = args->base,
                       .at_u32 = args->at_u32 + 1u, .depth = args->depth + 1u);
        }
    }
    const unsigned int additional = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json,
                                               .map = args->schema, .name = "additionalProperties");
    if (!any && EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                           .node = additional, .kind = CFG_JSON_OBJECT))
    {
        EMBED_CALL(complete_schemas_at, SchemaCompleteCtx, .text = args->text, .schema = additional, .base = args->base,
                   .at_u32 = args->at_u32 + 1u, .depth = args->depth + 1u);
    }
}

// the schemas reached past the index step `at_u32` of `schema`: the item of its `prefixItems`, or of its `items` given
// as a list, at the index, or else its `items` given as one schema, or else its `additionalItems`
EMBED_INLINE void complete_index_reached(const SchemaCompleteCtx *args)
{
    const CliJson *const json = &g_cli_schema_store->json;
    unsigned int tuple = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = args->schema,
                                    .name = "prefixItems");
    const unsigned int items =
        EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = args->schema, .name = "items");
    if ((tuple == CLI_JSON_NONE) && EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                               .json_read = json, .node = items, .kind = CFG_JSON_ARRAY))
    {
        tuple = items;
    }
    const unsigned int positioned =
        EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = tuple,
                   .index = g_cli_schema_spot.step[args->at_u32].index);
    const unsigned int rest = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                         .node = items, .kind = CFG_JSON_OBJECT)
                                  ? items
                                  : EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json,
                                               .map = args->schema, .name = "additionalItems");
    const unsigned int reached = (positioned != CLI_JSON_NONE) ? positioned : rest;
    if (reached != CLI_JSON_NONE)
    {
        EMBED_CALL(complete_schemas_at, SchemaCompleteCtx, .text = args->text, .schema = reached, .base = args->base,
                   .at_u32 = args->at_u32 + 1u, .depth = args->depth + 1u);
    }
}

// the schemas that apply at the end of the steps from `at` of `schema`, read in `base`, put among those reached
static void complete_schemas_at(const SchemaCompleteCtx *args)
{
    if ((args->depth > CLI_SCHEMA_DEEPEST) || (args->schema == CLI_SCHEMA_NONE) ||
        !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &g_cli_schema_store->json,
                    .node = args->schema, .kind = CFG_JSON_OBJECT))
    {
        return;
    }
    complete_reference_reached(args);
    complete_branches_reached(args);
    if (args->at_u32 < g_cli_schema_spot.steps)
    {
        if (g_cli_schema_spot.step[args->at_u32].key)
        {
            complete_key_reached(args);
        }
        else
        {
            complete_index_reached(args);
        }
    }
    else if (g_cli_schema_reached_count < CLI_SCHEMA_REACHED)
    {
        g_cli_schema_reached[g_cli_schema_reached_count] = args->schema;
        g_cli_schema_reached_count += 1u;
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

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema_complete.c: what completes the word before the cursor in a JSON file: where the cursor stands, the steps to
// the value it is in, whether it is at a key and inside a string; the schemas that hold there, `$ref`s followed and
// `allOf`, `anyOf`, `oneOf` and `if`'s branches taken together; and from them the keys the object does not hold yet,
// each written with what its type opens, or the values the schema allows. A text nested deeper than CLI_SCHEMA_FRAMES
// or holding more keys open than CLI_SCHEMA_OPEN_KEYS is read as far as they hold; the schemas past CLI_SCHEMA_REACHED
// are not offered
#include "schema_internal.h"

// the deepest frames read, and the bytes the bases of the schemas reached take
#define CLI_SCHEMA_FRAMES 512u
#define CLI_SCHEMA_REACHED_TEXT (1u << 20u)

// What stands open: an object, its key, whether its colon was passed and its first key held, or an array and its index
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

// a step put on the spot's steps where it fits
static void complete_step(int key, unsigned long long from, unsigned long long to, unsigned int index)
{
    if (g_cli_schema_spot.steps <= CLI_SCHEMA_FRAMES)
    {
        g_cli_schema_spot.step[g_cli_schema_spot.steps] = (CliSchemaStep){key, from, to, index};
        g_cli_schema_spot.steps += 1u;
    }
}

// a string passed from `at`, its raw text a key where the object's colon was not passed: past it, or 0 where it is not
// closed and the cursor stands in it
static int complete_string(const char *text, unsigned long long size, unsigned long long *at, unsigned int frames)
{
    unsigned long long end = *at + 1u;
    while (end < size)
    {
        if (text[end] == '\\')
        {
            end += 2u;
            continue;
        }
        if (text[end] == '"')
        {
            if ((frames > 0u) && s_frame[frames - 1u].object && !s_frame[frames - 1u].colon)
            {
                s_frame[frames - 1u].has_key = 1;
                s_frame[frames - 1u].key_from = *at + 1u;
                s_frame[frames - 1u].key_to = end;
            }
            *at = end + 1u;
            return 1;
        }
        end += 1u;
    }
    return 0;
}

int cli_schema_spot(const char *text, unsigned long long size)
{
    unsigned int frames = 0u;
    s_keys = 0u;
    g_cli_schema_spot.steps = 0u;
    g_cli_schema_spot.in_string = 0;
    for (unsigned long long at = 0ull; at < size;)
    {
        unsigned long long end = 0ull;
        if (cli_text_starts(text + at, size - at, "//"))
        {
            at = cli_text_find(text + at, size - at, "\n", 1u, &end) ? at + end : size;
            continue;
        }
        if (cli_text_starts(text + at, size - at, "/*"))
        {
            at = cli_text_find(text + at + 2u, size - at - 2u, "*/", 2u, &end) ? at + 2u + end + 2u : size;
            continue;
        }
        const char byte = text[at];
        CliSchemaFrame *const last = (frames > 0u) ? &s_frame[frames - 1u] : NULL;
        if (byte == '"')
        {
            if (!complete_string(text, size, &at, frames))
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
                complete_step(1, last->key_from, last->key_to, 0u);
            }
            else if ((last != NULL) && !last->object)
            {
                complete_step(0, 0ull, 0ull, last->index);
            }
            s_frame[frames] = (CliSchemaFrame){byte == '{', 0, 0ull, 0ull, 0, s_keys, 0u};
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
            complete_step(1, last->has_key ? last->key_from : 0ull, last->has_key ? last->key_to : 0ull, 0u);
        }
        g_cli_schema_spot.at_key = !last->colon;
        g_cli_schema_spot.keys_first = last->keys_first;
        g_cli_schema_spot.keys = s_keys - last->keys_first;
        return 1;
    }
    complete_step(0, 0ull, 0ull, last->index);
    g_cli_schema_spot.at_key = 0;
    g_cli_schema_spot.keys_first = 0u;
    g_cli_schema_spot.keys = 0u;
    return 1;
}

// the schemas reached, and the bases their references are read in
unsigned int g_cli_schema_reached[CLI_SCHEMA_REACHED];
unsigned int g_cli_schema_reached_count;
static char s_reached_text[CLI_SCHEMA_REACHED_TEXT];
static unsigned int s_reached_used;

// the schemas that hold at the end of the steps from `at` of `schema`, read in `base`, put among those reached
static void complete_schemas_at(const char *text, unsigned int schema, const char *base, unsigned int at,
                                unsigned int depth)
{
    CliSchemaStore *const store = g_cli_schema_store;
    const CliJson *const json = &store->json;
    if ((depth > CLI_SCHEMA_DEEPEST) || (schema == CLI_SCHEMA_NONE) || !cli_json_is(json, schema, CFG_JSON_OBJECT))
    {
        return;
    }
    unsigned long long size = 0ull;
    const char *const reference = cli_schema_string(json, schema, "$ref", &size);
    if ((reference != NULL) && (CLI_PATH_LONGEST <= CLI_SCHEMA_REACHED_TEXT - s_reached_used))
    {
        char *const doc = s_reached_text + s_reached_used;
        const unsigned int target = cli_schema_resolve(store, base, reference, size, doc);
        if (target != CLI_SCHEMA_NONE)
        {
            s_reached_used += (unsigned int)cellul.len(&(CatenaFinitaCfg){.src = doc, .cap = CLI_CELLS_POOL}) + 1u;
            complete_schemas_at(text, target, doc, at, depth + 1u);
        }
    }
    static const char *const s_words[5] = {"allOf", "anyOf", "oneOf", "then", "else"};
    for (unsigned int word = 0u; word < 5u; word += 1u)
    {
        const unsigned int all = cli_schema_get(json, schema, s_words[word]);
        if ((word < 3u) && cli_json_is(json, all, CFG_JSON_ARRAY))
        {
            for (unsigned int one = json->node[all].first; one != CLI_JSON_NONE; one = json->node[one].next)
            {
                complete_schemas_at(text, one, base, at, depth + 1u);
            }
        }
        else if ((word >= 3u) && (all != CLI_JSON_NONE))
        {
            complete_schemas_at(text, all, base, at, depth + 1u);
        }
    }
    if (at == g_cli_schema_spot.steps)
    {
        if (g_cli_schema_reached_count < CLI_SCHEMA_REACHED)
        {
            g_cli_schema_reached[g_cli_schema_reached_count] = schema;
            g_cli_schema_reached_count += 1u;
        }
        return;
    }
    const CliSchemaStep *const step = &g_cli_schema_spot.step[at];
    if (step->key)
    {
        char key[1u << 12u];
        const unsigned long long key_size =
            (step->to - step->from < sizeof(key)) ? step->to - step->from : sizeof(key) - 1u;
        memor.cpy(&(MemoriaCfg){.dst = key, .src = text + step->from, .bytes = (size_t)key_size});
        key[key_size] = '\0';
        int any = 0;
        const unsigned int named = cli_schema_get(json, cli_schema_get(json, schema, "properties"), key);
        if (named != CLI_JSON_NONE)
        {
            any = 1;
            complete_schemas_at(text, named, base, at + 1u, depth + 1u);
        }
        const unsigned int patterns = cli_schema_get(json, schema, "patternProperties");
        for (unsigned int one = cli_json_is(json, patterns, CFG_JSON_OBJECT) ? json->node[patterns].first
                                                                             : CLI_JSON_NONE;
             one != CLI_JSON_NONE; one = json->node[one].next)
        {
            const char *const pattern = json->text + json->node[one].key;
            if (cli_schema_matches(pattern, cellul.len(&(CatenaFinitaCfg){.src = pattern, .cap = CLI_CELLS_POOL}), key,
                                   key_size) == 1)
            {
                any = 1;
                complete_schemas_at(text, one, base, at + 1u, depth + 1u);
            }
        }
        const unsigned int additional = cli_schema_get(json, schema, "additionalProperties");
        if (!any && cli_json_is(json, additional, CFG_JSON_OBJECT))
        {
            complete_schemas_at(text, additional, base, at + 1u, depth + 1u);
        }
        return;
    }
    unsigned int tuple = cli_schema_get(json, schema, "prefixItems");
    const unsigned int items = cli_schema_get(json, schema, "items");
    tuple = (tuple != CLI_JSON_NONE) ? tuple : (cli_json_is(json, items, CFG_JSON_ARRAY) ? items : CLI_JSON_NONE);
    const unsigned int placed = cli_json_item(json, tuple, step->index);
    if (placed != CLI_JSON_NONE)
    {
        complete_schemas_at(text, placed, base, at + 1u, depth + 1u);
        return;
    }
    const unsigned int rest =
        cli_json_is(json, items, CFG_JSON_OBJECT) ? items : cli_schema_get(json, schema, "additionalItems");
    if (rest != CLI_JSON_NONE)
    {
        complete_schemas_at(text, rest, base, at + 1u, depth + 1u);
    }
}

void cli_schema_detail(const CliJson *json, unsigned int schema, CliLine *out)
{
    unsigned long long size = 0ull;
    const char *said = cli_schema_string(json, schema, "description", &size);
    said = (said != NULL) ? said : cli_schema_string(json, schema, "title", &size);
    if (said != NULL)
    {
        const char *const newline = memor.chr(&(MemoriaCfg){.src = said, .val = '\n', .bytes = (size_t)size});
        unsigned long long line = (newline != NULL) ? (unsigned long long)(newline - said) : size;
        line -= ((line > 0u) && (said[line - 1u] == '\r')) ? 1u : 0u;
        cli_line_counted(out, said, line);
        return;
    }
    const unsigned int kind = cli_schema_get(json, schema, "type");
    if (kind == CLI_JSON_NONE)
    {
        return;
    }
    const char *const text = cli_json_string(json, kind, &size);
    if (text != NULL)
    {
        cli_line_counted(out, text, size);
        return;
    }
    cli_json_write(json, kind, out, 0u, 1);
}

void cli_schema_reached_of(const char *text, unsigned int schema, const char *base)
{
    g_cli_schema_reached_count = 0u;
    s_reached_used = 0u;
    complete_schemas_at(text, schema, base, 0u, 0u);
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema_offer.c: what completes the word before the cursor in a JSON file, from the schemas that hold where it stands:
// at a key, each key they give the object that it does not hold yet, inside a string the rest of its name and outside
// one its name written with what its type opens; at a value, each value their `enum`, `const`, `default` and
// `examples` allow and the booleans and null their types take, inside a string only the strings
#include "schema_internal.h"

// the most keys and values offered, and the bytes they take
#define CLI_SCHEMA_OFFERED 4096u
#define CLI_SCHEMA_OFFERED_TEXT (1u << 20u)

// whether a letter goes in the word the cursor completes
static int offer_letter(unsigned int letter)
{
    return cli_text_alphanumeric(letter) || (letter == '_') || (letter == '$') || (letter == '-') || (letter == '.') ||
           (letter == '@') || (letter == '/');
}

// whether the object the cursor is in holds the key `name` before it, by its raw text
static int offer_held(const char *text, const char *name, unsigned long long size)
{
    const CliSchemaSpot *const spot = &g_cli_schema_spot;
    for (unsigned int at = spot->keys_first; at < spot->keys_first + spot->keys; at += 1u)
    {
        if ((g_cli_schema_key_to[at] - g_cli_schema_key_from[at] == size) &&
            (memor.cmp(&(MemoriaCfg){.src = text + g_cli_schema_key_from[at], .other = name, .bytes = (size_t)size}) ==
             0))
        {
            return 1;
        }
    }
    return 0;
}

// the keys the schemas reached give the object that it does not hold yet, each once, put after the token
static void offer_keys(const char *text, unsigned long long token, unsigned long long at, CliCompletions *items)
{
    const CliJson *const json = &g_cli_schema_store->json;
    const unsigned long long base = cli_build_run_before(text + token, at - token, cli_build_word_letter) + token;
    static const char *s_offered[CLI_SCHEMA_OFFERED];
    unsigned int offered = 0u;
    for (unsigned int reached = 0u; reached < g_cli_schema_reached_count; reached += 1u)
    {
        const unsigned int properties = cli_schema_get(json, g_cli_schema_reached[reached], "properties");
        for (unsigned int one = cli_json_is(json, properties, CFG_JSON_OBJECT) ? json->node[properties].first
                                                                               : CLI_JSON_NONE;
             one != CLI_JSON_NONE; one = json->node[one].next)
        {
            const char *const name = json->text + json->node[one].key;
            const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
            int seen = offer_held(text, name, size);
            for (unsigned int earlier = 0u; !seen && (earlier < offered); earlier += 1u)
            {
                seen = (cellul.eq(&(CatenaFinitaCfg){.src = s_offered[earlier], .other = name, .cap = CLI_CELLS_POOL}));
            }
            if (seen || (offered == CLI_SCHEMA_OFFERED) || (size < base - token) ||
                (memor.cmp(&(MemoriaCfg){.src = name, .other = text + token, .bytes = (size_t)(base - token)}) != 0))
            {
                continue;
            }
            s_offered[offered] = name;
            offered += 1u;
            static char s_detail[1u << 12u];
            CliLine detail = {s_detail, sizeof(s_detail), 0ull};
            cli_schema_detail(json, one, &detail);
            if (g_cli_schema_spot.in_string)
            {
                const unsigned long long typed = base - token;
                (void)cli_completion_put(items, name + typed, size - typed, "field", s_detail, detail.at, name + typed,
                                         size - typed, 0);
                continue;
            }
            unsigned long long kind_size = 0ull;
            const char *const kind = cli_schema_string(json, one, "type", &kind_size);
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
            CliLine quoted = {s_quoted, sizeof(s_quoted), 0ull};
            cli_json_quoted(&quoted, name, size);
            static char s_insert[1u << 14u];
            CliLine insert = {s_insert, sizeof(s_insert), 0ull};
            for (unsigned long long letter = 0ull; letter < quoted.at; letter += 1u)
            {
                cli_line_text(&insert, (s_quoted[letter] == '$') ? "\\$" : "");
                if (s_quoted[letter] != '$')
                {
                    cli_line_character(&insert, s_quoted[letter]);
                }
            }
            cli_line_text(&insert, ": ");
            cli_line_text(&insert, shape);
            (void)cli_completion_put(items, name, size, "field", s_detail, detail.at, s_insert, insert.at, 1);
        }
    }
}

// The values offered: each its text and detail, kept once
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
static void offer_value(const CliJson *json, unsigned int value, const char *detail, unsigned long long detail_size)
{
    static char s_text[1u << 14u];
    CliLine line = {s_text, sizeof(s_text), 0ull};
    if (g_cli_schema_spot.in_string)
    {
        unsigned long long size = 0ull;
        const char *const text = cli_json_string(json, value, &size);
        if (text == NULL)
        {
            return;
        }
        cli_line_counted(&line, text, size);
    }
    else
    {
        cli_json_write(json, value, &line, 0u, 1);
    }
    if (!cli_line_fits(&line) || (s_values.count == CLI_SCHEMA_OFFERED) ||
        (line.at + detail_size > CLI_SCHEMA_OFFERED_TEXT - s_values.used))
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
    memor.cpy(&(MemoriaCfg){.dst = s_values.text + s_values.used, .src = s_text, .bytes = (size_t)line.at});
    s_values.used += (unsigned int)line.at;
    s_values.detail_at[s_values.count] = s_values.used;
    s_values.detail_size[s_values.count] = (unsigned int)detail_size;
    memor.cpy(&(MemoriaCfg){.dst = s_values.text + s_values.used, .src = detail, .bytes = (size_t)detail_size});
    s_values.used += (unsigned int)detail_size;
    s_values.count += 1u;
}

// whether a schema's `type` names `kind`
static int offer_typed(const CliJson *json, unsigned int schema, const char *kind)
{
    const unsigned int types = cli_schema_get(json, schema, "type");
    unsigned long long size = 0ull;
    const char *const single = cli_json_string(json, types, &size);
    if (single != NULL)
    {
        return (cellul.len(&(CatenaFinitaCfg){.src = kind, .cap = CLI_CELLS_POOL}) == size) &&
               (memor.cmp(&(MemoriaCfg){.src = single, .other = kind, .bytes = (size_t)size}) == 0);
    }
    for (unsigned int one = cli_json_is(json, types, CFG_JSON_ARRAY) ? json->node[types].first : CLI_JSON_NONE;
         one != CLI_JSON_NONE; one = json->node[one].next)
    {
        const char *const text = cli_json_string(json, one, &size);
        if ((text != NULL) && (cellul.len(&(CatenaFinitaCfg){.src = kind, .cap = CLI_CELLS_POOL}) == size) &&
            (memor.cmp(&(MemoriaCfg){.src = text, .other = kind, .bytes = (size_t)size}) == 0))
        {
            return 1;
        }
    }
    return 0;
}

// the values the schemas reached allow, put after `token`
static void offer_values(const char *token, unsigned long long token_size, CliCompletions *items)
{
    CliJson *const json = &g_cli_schema_store->json;
    s_values.count = 0u;
    s_values.used = 0u;
    for (unsigned int reached = 0u; reached < g_cli_schema_reached_count; reached += 1u)
    {
        const unsigned int one = g_cli_schema_reached[reached];
        static char s_detail[1u << 12u];
        CliLine detail = {s_detail, sizeof(s_detail), 0ull};
        cli_schema_detail(json, one, &detail);
        const unsigned int choices = cli_schema_get(json, one, "enum");
        for (unsigned int at = cli_json_is(json, choices, CFG_JSON_ARRAY) ? json->node[choices].first : CLI_JSON_NONE;
             at != CLI_JSON_NONE; at = json->node[at].next)
        {
            offer_value(json, at, s_detail, detail.at);
        }
        static const char *const s_given[2] = {"const", "default"};
        for (unsigned int word = 0u; word < 2u; word += 1u)
        {
            const unsigned int given = cli_schema_get(json, one, s_given[word]);
            if (given != CLI_JSON_NONE)
            {
                offer_value(json, given, s_detail, detail.at);
            }
        }
        const unsigned int examples = cli_schema_get(json, one, "examples");
        for (unsigned int at = cli_json_is(json, examples, CFG_JSON_ARRAY) ? json->node[examples].first : CLI_JSON_NONE;
             at != CLI_JSON_NONE; at = json->node[at].next)
        {
            offer_value(json, at, s_detail, detail.at);
        }
        static const CfgJsonKind s_kinds[3] = {CFG_JSON_TRUE, CFG_JSON_FALSE, CFG_JSON_NULL};
        for (unsigned int word = 0u; word < 3u; word += 1u)
        {
            if (offer_typed(json, one, (word < 2u) ? "boolean" : "null") && !g_cli_schema_spot.in_string)
            {
                const unsigned int made = cli_json_made(json, s_kinds[word]);
                if (made != CLI_JSON_NONE)
                {
                    offer_value(json, made, s_detail, detail.at);
                }
            }
        }
    }
    for (unsigned int at = 0u; at < s_values.count; at += 1u)
    {
        cli_build_relative(items, token, token_size, s_values.text + s_values.at[at], s_values.size[at], "value",
                           s_values.text + s_values.detail_at[at], s_values.detail_size[at]);
    }
}

void cli_schema_complete(const char *path, const char *text, unsigned long long size, unsigned int line,
                         unsigned int col, CliCompletions *items)
{
    const unsigned long long at = cli_build_byte_at(text, size, line, col);
    if (!cli_schema_held())
    {
        return;
    }
    if (!cli_schema_spot(text, at))
    {
        cli_schema_released();
        return;
    }
    cli_schema_read(text, size, g_cli_schema_read);
    char key[CLI_PATH_LONGEST];
    int named = 0;
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    if (cli_schema_of(path, g_cli_schema_read, key, &named, &from, &to))
    {
        cli_schema_store_begun(g_cli_schema_store);
        const unsigned int schema = cli_schema_load(g_cli_schema_store, key);
        if (schema != CLI_SCHEMA_NONE)
        {
            const unsigned long long token = cli_build_run_before(text, at, offer_letter);
            cli_schema_reached_of(text, schema, key);
            if (g_cli_schema_spot.at_key)
            {
                offer_keys(text, token, at, items);
            }
            else
            {
                offer_values(text + token, at - token, items);
            }
        }
    }
    cli_schema_released();
}

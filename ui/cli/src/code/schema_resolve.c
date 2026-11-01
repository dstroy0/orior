// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema_resolve.c: where a schema's reference leads and which schema a file names: a `$ref` resolved by its document
// and its fragment, a JSON pointer with its `%XX` escapes read or an anchor, a reference read beside its base, a URL's
// or a file's; and a file's schema found by its `$schema` or by the name it is known by
#include "schema_internal.h"

#include "../text/text.h"
#include <stdlib.h>

// the schema files of these names follow where they name none, by the SchemaStore's URL
static const char *const s_known[8][2] = {
    {"package.json", "https://json.schemastore.org/package.json"},
    {"tsconfig.json", "https://json.schemastore.org/tsconfig.json"},
    {"jsconfig.json", "https://json.schemastore.org/jsconfig.json"},
    {".eslintrc.json", "https://json.schemastore.org/eslintrc.json"},
    {".prettierrc.json", "https://json.schemastore.org/prettierrc.json"},
    {"composer.json", "https://getcomposer.org/schema.json"},
    {".babelrc.json", "https://json.schemastore.org/babelrc.json"},
    {"devcontainer.json",
     "https://raw.githubusercontent.com/devcontainers/spec/main/schemas/devContainer.schema.json"}};

// a fragment of a reference with its `%XX` escapes read into `out`
static void resolve_percent_decoded(const char *text, unsigned long long size, VerbaLinea *out)
{
    for (unsigned long long at = 0ull; at < size;)
    {
        if ((text[at] == '%') && (at + 2u < size))
        {
            char hex[3] = {text[at + 1u], text[at + 2u], '\0'};
            char *end = NULL;
            const unsigned long value = strtoul(hex, &end, 16);
            const int spaced = (hex[0] == ' ') || ((hex[0] >= '\t') && (hex[0] <= '\r')) || (hex[0] == '-');
            if ((end == hex + 2) && !spaced)
            {
                mmgr_line_ch(out, (char)value);
                at += 3u;
                continue;
            }
        }
        mmgr_line_ch(out, text[at]);
        at += 1u;
    }
}

// the node at the JSON pointer `pointer` from `node`, CLI_JSON_NONE where none is there
static unsigned int resolve_pointer(const CliJson *json, unsigned int node, const char *pointer,
                                    unsigned long long size)
{
    if (size == 0u)
    {
        return node;
    }
    if (pointer[0] != '/')
    {
        return CLI_JSON_NONE;
    }
    unsigned int at = node;
    for (unsigned long long start = 1ull; (at != CLI_JSON_NONE) && (start <= size);)
    {
        unsigned long long end = start;
        end = cli_text_to_any(pointer, end, size, "/");
        char token[1u << 12u];
        VerbaLinea line = {token, sizeof(token), 0ull};
        for (unsigned long long one = start; one < end; one += 1u)
        {
            if ((pointer[one] == '~') && (one + 1u < end) && ((pointer[one + 1u] == '0') || (pointer[one + 1u] == '1')))
            {
                mmgr_line_ch(&line, (pointer[one + 1u] == '1') ? '/' : '~');
                one += 1u;
                continue;
            }
            mmgr_line_ch(&line, pointer[one]);
        }
        if (mmgr_line_finish(&line) == 0u && (end > start))
        {
            return CLI_JSON_NONE;
        }
        if (cli_json_is(json, at, CFG_JSON_OBJECT))
        {
            at = cli_json_member(json, at, token);
        }
        else if (cli_json_is(json, at, CFG_JSON_ARRAY))
        {
            char *stop = NULL;
            const unsigned long index = strtoul(token, &stop, 10);
            const int digits = (line.at > 0u) && (token[0] >= '0') && (token[0] <= '9') && (*stop == '\0') &&
                               !((line.at > 1u) && (token[0] == '0'));
            at = digits ? cli_json_item(json, at, (unsigned int)index) : CLI_JSON_NONE;
        }
        else
        {
            return CLI_JSON_NONE;
        }
        start = end + 1u;
    }
    return at;
}

// the schema in `node` whose `$anchor`, or whose `$id` or `id` of `#name`, is `name`, CLI_JSON_NONE where none is
static unsigned int resolve_anchored(const CliJson *json, unsigned int node, const char *name, unsigned long long size)
{
    if (cli_json_is(json, node, CFG_JSON_OBJECT))
    {
        unsigned long long length = 0ull;
        const char *const anchor = cli_schema_string(json, node, "$anchor", &length);
        if ((anchor != NULL) && (length == size) &&
            (memor.cmp(&(MemoriaCfg){.src = anchor, .other = name, .bytes = (size_t)size}) == 0))
        {
            return node;
        }
        const char *id = cli_schema_string(json, node, "$id", &length);
        id = (id != NULL) ? id : cli_schema_string(json, node, "id", &length);
        if ((id != NULL) && (length == size + 1u) && (id[0] == '#') &&
            (memor.cmp(&(MemoriaCfg){.src = id + 1, .other = name, .bytes = (size_t)size}) == 0))
        {
            return node;
        }
    }
    if (!cli_json_is(json, node, CFG_JSON_OBJECT) && !cli_json_is(json, node, CFG_JSON_ARRAY))
    {
        return CLI_JSON_NONE;
    }
    for (unsigned int child = json->node[node].first; child != CLI_JSON_NONE; child = json->node[child].next)
    {
        const unsigned int found = resolve_anchored(json, child, name, size);
        if (found != CLI_JSON_NONE)
        {
            return found;
        }
    }
    return CLI_JSON_NONE;
}

// `target` read from `base` into `out`, sized for CLI_PATH_MAX_LEN: a URL as it stands, and otherwise beside the
// base, a URL's or a file's, its `.` and `..` read
static void resolve_join(const char *base, const char *target, unsigned long long size, char *out)
{
    VerbaLinea line = {out, CLI_PATH_MAX_LEN, 0ull};
    unsigned long long found = 0ull;
    if (cli_text_find(target, size, "://", 3u, &found))
    {
        mmgr_line_put_n(&line, target, size);
        (void)mmgr_line_finish(&line);
        return;
    }
    const int url = (cellul.find(&(CatenaFinitaCfg){
                         .src = base, .cap = CLI_CELLS_POOL, .other = "://", .other_cap = CLI_CELLS_POOL}) != NULL);
    const char *const slash =
        cli_text_last(base, cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL}), '/');
    const char *const back =
        url ? NULL : cli_text_last(base, cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL}), '\\');
    const char *const cut = ((back != NULL) && ((slash == NULL) || (back > slash))) ? back : slash;
    const char *rest = target;
    unsigned long long rest_size = size;
    if (!url && cli_text_starts(target, size, "file://"))
    {
        rest += 7;
        rest_size -= 7u;
    }
    const int absolute =
        !url && (rest_size > 0u) && ((rest[0] == '/') || (rest[0] == '\\') || ((rest_size > 1u) && (rest[1] == ':')));
    if (absolute)
    {
        mmgr_line_put_n(&line, rest, rest_size);
        (void)mmgr_line_finish(&line);
        return;
    }
    mmgr_line_put_n(&line, base,
                    (cut != NULL) ? (unsigned long long)(cut - base)
                                  : (url ? cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL}) : 0u));
    unsigned int parts = 0u;
    for (unsigned long long at = 0ull; at < line.at; at += 1u)
    {
        parts += (out[at] == '/') ? 1u : 0u;
    }
    for (unsigned long long start = 0ull; start <= rest_size;)
    {
        unsigned long long end = start;
        end = cli_text_to_any(rest, start, rest_size, url ? "/" : "/\\");
        const unsigned long long part = end - start;
        if ((part == 2u) && (memor.cmp(&(MemoriaCfg){.src = rest + start, .other = "..", .bytes = 2u}) == 0))
        {
            if (!url || (parts + 1u > 3u))
            {
                line.at = cli_text_back_any(out, line.at, "/\\");
                line.at -= (line.at > 0u) ? 1u : 0u;
                parts -= (parts > 0u) ? 1u : 0u;
            }
        }
        else if (!((part == 1u) && (rest[start] == '.')) && (url || (part > 0u)))
        {
            mmgr_line_put(&line, (url || (line.at > 0u)) ? "/" : "");
            mmgr_line_put_n(&line, rest + start, part);
            parts += 1u;
        }
        start = end + 1u;
    }
    (void)mmgr_line_finish(&line);
}

unsigned int cli_schema_resolve(CliSchemaStore *store, const char *base, const char *reference, unsigned long long size,
                                char *key)
{
    unsigned long long hash = size;
    (void)cli_text_find(reference, size, "#", 1u, &hash);
    VerbaLinea line = {key, CLI_PATH_MAX_LEN, 0ull};
    if (hash == 0u)
    {
        mmgr_line_put(&line, base);
        (void)mmgr_line_finish(&line);
    }
    else
    {
        resolve_join(base, reference, hash, key);
    }
    const unsigned int doc = cli_schema_load(store, key);
    if (doc == CLI_SCHEMA_NONE)
    {
        return CLI_SCHEMA_NONE;
    }
    const char *const fragment = (hash < size) ? reference + hash + 1u : "";
    const unsigned long long fragment_size = (hash < size) ? size - hash - 1u : 0u;
    if (fragment_size == 0u)
    {
        return doc;
    }
    if (fragment[0] == '/')
    {
        static char s_decoded[1u << 14u];
        VerbaLinea decoded = {s_decoded, sizeof(s_decoded), 0ull};
        resolve_percent_decoded(fragment, fragment_size, &decoded);
        const unsigned int found = resolve_pointer(&store->json, doc, s_decoded, decoded.at);
        return (found != CLI_JSON_NONE) ? found : CLI_SCHEMA_NONE;
    }
    const unsigned int found = resolve_anchored(&store->json, doc, fragment, fragment_size);
    return (found != CLI_JSON_NONE) ? found : CLI_SCHEMA_NONE;
}

int cli_schema_of(const char *path, const CliSchemaRead *read, char *key, int *named, unsigned long long *from,
                  unsigned long long *to)
{
    *named = 0;
    const CliSchemaNode *const root = (read->root != CLI_SCHEMA_NONE) ? &read->node[read->root] : NULL;
    for (unsigned int at = ((root != NULL) && (root->kind == CLI_SCHEMA_OBJECT)) ? root->first : CLI_SCHEMA_NONE;
         at != CLI_SCHEMA_NONE; at = read->member[at].next)
    {
        const CliSchemaMember *const member = &read->member[at];
        if ((member->key_size != 7u) ||
            (memor.cmp(&(MemoriaCfg){.src = cli_schema_text(read, member->key_at), .other = "$schema", .bytes = 7u}) !=
             0))
        {
            continue;
        }
        const CliSchemaNode *const value = (member->value != CLI_SCHEMA_NONE) ? &read->node[member->value] : NULL;
        if ((value == NULL) || (value->kind != CLI_SCHEMA_TEXT))
        {
            break;
        }
        const char *const text = cli_schema_text(read, value->text_at);
        unsigned long long found = 0ull;
        if (cli_text_find(text, value->text_size, "://", 3u, &found) &&
            !cli_text_starts(text, value->text_size, "file://"))
        {
            VerbaLinea line = {key, CLI_PATH_MAX_LEN, 0ull};
            mmgr_line_put_n(&line, text, value->text_size);
            (void)mmgr_line_finish(&line);
        }
        else
        {
            resolve_join(path, text, value->text_size, key);
        }
        *named = 1;
        *from = value->from;
        *to = value->to;
        return 1;
    }
    const char *name = path;
    const unsigned long long name_past =
        cli_text_back_any(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), "/\\");
    name = (name_past != 0ull) ? (path + name_past) : name;
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        int same = (cellul.len(&(CatenaFinitaCfg){.src = s_known[at][0], .cap = CLI_CELLS_POOL}) ==
                    cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
        same =
            same && cellul.eq(&(CatenaFinitaCfg){.src = name, .other = s_known[at][0], .cap = CLI_CELLS_POOL, .ci = 1});
        if (same)
        {
            VerbaLinea line = {key, CLI_PATH_MAX_LEN, 0ull};
            mmgr_line_put(&line, s_known[at][1]);
            (void)mmgr_line_finish(&line);
            return 1;
        }
    }
    return 0;
}

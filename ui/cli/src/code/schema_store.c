// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema_store.c: the schemas a check reaches: each read once, by its URL among the meta-schemas orior holds or those
// kept from a fetch, or by its path, its comments taken out, into one tree; a `$ref` resolved by its document and its
// fragment, a JSON pointer with its `%XX` escapes read or an anchor; and a file's schema found by its `$schema` or by
// the name it is known by. A document larger than CLI_SCHEMA_DOCUMENT_TEXT, or whose values the tree cannot hold, is
// one that could not be read
#include "schema_internal.h"

#include "../text/text.h"
#include <stdlib.h>

// the most bytes of a schema read, and the most tokens it is read in
#define CLI_SCHEMA_DOCUMENT_TEXT (1u << 22u)
#define CLI_SCHEMA_TOKENS (1u << 18u)

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

static char s_raw[CLI_SCHEMA_DOCUMENT_TEXT];
static char s_stripped[CLI_SCHEMA_DOCUMENT_TEXT];
static CfgJsonToken s_tokens[CLI_SCHEMA_TOKENS];

void cli_schema_store_begun(CliSchemaStore *store)
{
    cli_json_begun(&store->json, store->node, CLI_SCHEMA_STORE_NODES, store->text, CLI_SCHEMA_STORE_TEXT);
    store->documents = 0u;
    store->missings = 0u;
}

// a URL as a key among those orior holds, no scheme and no `#` and no `/` at its end, into `out`
static void store_url_key(const char *url, CliLine *out)
{
    const char *const scheme =
        cellul.find(&(CatenaFinitaCfg){.src = url, .cap = CLI_CELLS_POOL, .other = "://", .other_cap = CLI_CELLS_POOL});
    const char *const rest = (scheme != NULL) ? scheme + 3 : url;
    unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL});
    while ((size > 0u) && ((rest[size - 1u] == '#') || (rest[size - 1u] == '/')))
    {
        size -= 1u;
    }
    cli_line_counted(out, rest, size);
}

int cli_schema_kept_at(const char *url, char *out)
{
    char folder[CLI_PATH_LONGEST];
    char schemas[CLI_PATH_LONGEST];
    char key[CLI_PATH_LONGEST];
    CliLine line = {key, sizeof(key), 0ull};
    store_url_key(url, &line);
    if (!cli_home_folder(folder) || !cli_line_fits(&line))
    {
        return 0;
    }
    char name[CLI_PATH_LONGEST];
    CliLine named = {name, sizeof(name), 0ull};
    for (unsigned long long at = 0ull; at < line.at;)
    {
        unsigned long long length = 0ull;
        const unsigned int letter = cli_text_letter(key, line.at, at, &length);
        const int kept = ((letter >= 'a') && (letter <= 'z')) || ((letter >= 'A') && (letter <= 'Z')) ||
                         ((letter >= '0') && (letter <= '9')) || (letter == '.') || (letter == '-');
        cli_line_character(&named, kept ? (char)letter : '_');
        at += length;
    }
    cli_line_text(&named, ".json");
    if (cli_line_closed(&named) == 0u)
    {
        return 0;
    }
    cli_full(folder, "schemas", schemas);
    cli_full(schemas, name, out);
    return 1;
}

// `size` bytes of JSON with their comments taken out, each by spaces and a block's lines kept, into `s_stripped`
static void store_stripped(const char *text, unsigned long long size)
{
    int quoted = 0;
    for (unsigned long long at = 0ull; at < size;)
    {
        if (quoted)
        {
            s_stripped[at] = text[at];
            if ((text[at] == '\\') && (at + 1u < size))
            {
                s_stripped[at + 1u] = text[at + 1u];
                at += 2u;
                continue;
            }
            quoted = (text[at] != '"');
            at += 1u;
            continue;
        }
        unsigned long long end = 0ull;
        if (cli_text_starts(text + at, size - at, "//"))
        {
            end = cli_text_find(text + at, size - at, "\n", 1u, &end) ? at + end : size;
            memor.set(&(MemoriaCfg){.dst = s_stripped + at, .val = ' ', .bytes = (size_t)(end - at)});
            at = end;
            continue;
        }
        if (cli_text_starts(text + at, size - at, "/*"))
        {
            end = cli_text_find(text + at + 2u, size - at - 2u, "*/", 2u, &end) ? at + 2u + end + 2u : size;
            for (unsigned long long one = at; one < end; one += 1u)
            {
                s_stripped[one] = (text[one] == '\n') ? '\n' : ' ';
            }
            at = end;
            continue;
        }
        quoted = (text[at] == '"');
        s_stripped[at] = text[at];
        at += 1u;
    }
}

// whether the store could not find `key` before
static int store_missing(const CliSchemaStore *store, const char *key)
{
    for (unsigned int at = 0u; at < store->missings; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = store->missing[at], .other = key, .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return 0;
}

// `key` kept among those the store could not find
static void store_missed(CliSchemaStore *store, const char *key)
{
    if (store->missings < CLI_SCHEMA_MISSING)
    {
        CliLine line = {store->missing[store->missings], CLI_PATH_LONGEST, 0ull};
        cli_line_text(&line, key);
        (void)cli_line_closed(&line);
        store->missings += 1u;
    }
}

// the text of the document at `key`, the bundled meta-schemas first for a URL: its bytes and size, NULL where none
static const char *store_text_of(const char *key, unsigned long long *size)
{
    if (cellul.find(
            &(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL, .other = "://", .other_cap = CLI_CELLS_POOL}) != NULL)
    {
        char wanted[CLI_PATH_LONGEST];
        CliLine line = {wanted, sizeof(wanted), 0ull};
        store_url_key(key, &line);
        for (unsigned int at = 0u; at < 5u; at += 1u)
        {
            if ((cellul.len(&(CatenaFinitaCfg){.src = g_cli_schema_bundled[at][0], .cap = CLI_CELLS_POOL}) ==
                 line.at) &&
                (memor.cmp(&(MemoriaCfg){
                     .src = g_cli_schema_bundled[at][0], .other = wanted, .bytes = (size_t)line.at}) == 0))
            {
                *size = cellul.len(&(CatenaFinitaCfg){.src = g_cli_schema_bundled[at][1], .cap = CLI_CELLS_POOL});
                return g_cli_schema_bundled[at][1];
            }
        }
        char kept[CLI_PATH_LONGEST];
        if (!cli_schema_kept_at(key, kept))
        {
            return NULL;
        }
        *size = qry_member_file_read(kept, (unsigned char *)s_raw, sizeof(s_raw));
        return (*size != QRY_MEMBER_UNREAD) ? s_raw : NULL;
    }
    *size = qry_member_file_read(key, (unsigned char *)s_raw, sizeof(s_raw));
    return (*size != QRY_MEMBER_UNREAD) ? s_raw : NULL;
}

unsigned int cli_schema_load(CliSchemaStore *store, const char *key)
{
    for (unsigned int at = 0u; at < store->documents; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = store->key[at], .other = key, .cap = CLI_CELLS_POOL}))
        {
            return store->root[at];
        }
    }
    if ((store->documents >= CLI_SCHEMA_DOCUMENTS) || store_missing(store, key))
    {
        return CLI_SCHEMA_NONE;
    }
    unsigned long long size = 0ull;
    const char *const text = store_text_of(key, &size);
    unsigned int root = CLI_JSON_NONE;
    if ((text != NULL) && cli_utf8_whole(text, size))
    {
        store_stripped(text, size);
        static char s_said[256];
        CliLine said = {s_said, sizeof(s_said), 0ull};
        root = cli_json_read(&store->json, s_stripped, size, s_tokens, CLI_SCHEMA_TOKENS, &said);
        root = store->json.full ? CLI_JSON_NONE : root;
    }
    if (root == CLI_JSON_NONE)
    {
        store_missed(store, key);
        return CLI_SCHEMA_NONE;
    }
    CliLine line = {store->key[store->documents], CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, key);
    (void)cli_line_closed(&line);
    store->root[store->documents] = root;
    store->documents += 1u;
    return root;
}

// a fragment of a reference with its `%XX` escapes read into `out`
static void store_percent_decoded(const char *text, unsigned long long size, CliLine *out)
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
                cli_line_character(out, (char)value);
                at += 3u;
                continue;
            }
        }
        cli_line_character(out, text[at]);
        at += 1u;
    }
}

// the node at the JSON pointer `pointer` from `node`, CLI_JSON_NONE where none is there
static unsigned int store_pointer(const CliJson *json, unsigned int node, const char *pointer, unsigned long long size)
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
        while ((end < size) && (pointer[end] != '/'))
        {
            end += 1u;
        }
        char token[1u << 12u];
        CliLine line = {token, sizeof(token), 0ull};
        for (unsigned long long one = start; one < end; one += 1u)
        {
            if ((pointer[one] == '~') && (one + 1u < end) && ((pointer[one + 1u] == '0') || (pointer[one + 1u] == '1')))
            {
                cli_line_character(&line, (pointer[one + 1u] == '1') ? '/' : '~');
                one += 1u;
                continue;
            }
            cli_line_character(&line, pointer[one]);
        }
        if (cli_line_closed(&line) == 0u && (end > start))
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
static unsigned int store_anchored(const CliJson *json, unsigned int node, const char *name, unsigned long long size)
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
        const unsigned int found = store_anchored(json, child, name, size);
        if (found != CLI_JSON_NONE)
        {
            return found;
        }
    }
    return CLI_JSON_NONE;
}

// `target` read from `base` into `out`, which holds CLI_PATH_LONGEST: a URL as it stands, and otherwise beside the
// base, a URL's or a file's, its `.` and `..` read
static void store_join(const char *base, const char *target, unsigned long long size, char *out)
{
    CliLine line = {out, CLI_PATH_LONGEST, 0ull};
    unsigned long long found = 0ull;
    if (cli_text_find(target, size, "://", 3u, &found))
    {
        cli_line_counted(&line, target, size);
        (void)cli_line_closed(&line);
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
        cli_line_counted(&line, rest, rest_size);
        (void)cli_line_closed(&line);
        return;
    }
    cli_line_counted(&line, base,
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
        while ((end < rest_size) && (rest[end] != '/') && (url || (rest[end] != '\\')))
        {
            end += 1u;
        }
        const unsigned long long part = end - start;
        if ((part == 2u) && (memor.cmp(&(MemoriaCfg){.src = rest + start, .other = "..", .bytes = 2u}) == 0))
        {
            if (!url || (parts + 1u > 3u))
            {
                while ((line.at > 0u) && (out[line.at - 1u] != '/') && (out[line.at - 1u] != '\\'))
                {
                    line.at -= 1u;
                }
                line.at -= (line.at > 0u) ? 1u : 0u;
                parts -= (parts > 0u) ? 1u : 0u;
            }
        }
        else if (!((part == 1u) && (rest[start] == '.')) && (url || (part > 0u)))
        {
            cli_line_text(&line, (url || (line.at > 0u)) ? "/" : "");
            cli_line_counted(&line, rest + start, part);
            parts += 1u;
        }
        start = end + 1u;
    }
    (void)cli_line_closed(&line);
}

unsigned int cli_schema_resolve(CliSchemaStore *store, const char *base, const char *reference, unsigned long long size,
                                char *key)
{
    unsigned long long hash = size;
    (void)cli_text_find(reference, size, "#", 1u, &hash);
    CliLine line = {key, CLI_PATH_LONGEST, 0ull};
    if (hash == 0u)
    {
        cli_line_text(&line, base);
        (void)cli_line_closed(&line);
    }
    else
    {
        store_join(base, reference, hash, key);
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
        CliLine decoded = {s_decoded, sizeof(s_decoded), 0ull};
        store_percent_decoded(fragment, fragment_size, &decoded);
        const unsigned int found = store_pointer(&store->json, doc, s_decoded, decoded.at);
        return (found != CLI_JSON_NONE) ? found : CLI_SCHEMA_NONE;
    }
    const unsigned int found = store_anchored(&store->json, doc, fragment, fragment_size);
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
            CliLine line = {key, CLI_PATH_LONGEST, 0ull};
            cli_line_counted(&line, text, value->text_size);
            (void)cli_line_closed(&line);
        }
        else
        {
            store_join(path, text, value->text_size, key);
        }
        *named = 1;
        *from = value->from;
        *to = value->to;
        return 1;
    }
    const char *name = path;
    for (const char *at = path; *at != '\0'; at += 1)
    {
        name = ((*at == '/') || (*at == '\\')) ? at + 1 : name;
    }
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        int same = (cellul.len(&(CatenaFinitaCfg){.src = s_known[at][0], .cap = CLI_CELLS_POOL}) ==
                    cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
        for (unsigned int one = 0u; same && (name[one] != '\0'); one += 1u)
        {
            const char lower = ((name[one] >= 'A') && (name[one] <= 'Z')) ? (char)(name[one] - 'A' + 'a') : name[one];
            same = (lower == s_known[at][0][one]);
        }
        if (same)
        {
            CliLine line = {key, CLI_PATH_LONGEST, 0ull};
            cli_line_text(&line, s_known[at][1]);
            (void)cli_line_closed(&line);
            return 1;
        }
    }
    return 0;
}

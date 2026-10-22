// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema_store.c: the schemas a check reaches: each read once, by its URL among the meta-schemas orior_cli holds or
// those kept from a fetch, or by its path, its file mapped where it lies, its comments taken out in a cell of its size
// and read in a cell of the tokens it holds, into one tree. A document larger than CLI_SCHEMA_DOCUMENT_TEXT, of more
// tokens than CLI_SCHEMA_TOKENS, or whose values the tree cannot hold, is one that could not be read
#include "schema_internal.h"

#include "../text/text.h"

// the most bytes of a schema read, and the most tokens it is read in
#define CLI_SCHEMA_DOCUMENT_TEXT (1u << 22u)
#define CLI_SCHEMA_TOKENS (1u << 18u)

CliCellsTally g_cli_schema_cells = CLI_CELLS_TALLY("cli_schemas");

void cli_schema_store_begun(CliSchemaStore *store)
{
    cli_json_emptied(&store->json);
    store->documents = 0u;
    store->missings = 0u;
}

// a URL as a key among those orior_cli holds, no scheme and no `#` and no `/` at its end, into `out`
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

// `size` bytes of JSON with their comments taken out, each by spaces and a block's lines kept, into `stripped`, which
// holds `size` bytes
static void store_stripped(const char *text, unsigned long long size, char *stripped)
{
    int quoted = 0;
    for (unsigned long long at = 0ull; at < size;)
    {
        if (quoted)
        {
            stripped[at] = text[at];
            if ((text[at] == '\\') && (at + 1u < size))
            {
                stripped[at + 1u] = text[at + 1u];
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
            memor.set(&(MemoriaCfg){.dst = stripped + at, .val = ' ', .bytes = (size_t)(end - at)});
            at = end;
            continue;
        }
        if (cli_text_starts(text + at, size - at, "/*"))
        {
            end = cli_text_find(text + at + 2u, size - at - 2u, "*/", 2u, &end) ? at + 2u + end + 2u : size;
            for (unsigned long long one = at; one < end; one += 1u)
            {
                stripped[one] = (text[one] == '\n') ? '\n' : ' ';
            }
            at = end;
            continue;
        }
        quoted = (text[at] == '"');
        stripped[at] = text[at];
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

// the text of the document at `key`, the bundled meta-schemas first for a URL, and otherwise its file mapped where it
// lies, with 1 in `mapped`: its bytes and size, NULL where none is or its file is larger than CLI_SCHEMA_DOCUMENT_TEXT
static const char *store_text_of(const char *key, unsigned long long *size, int *mapped)
{
    *mapped = 0;
    const char *path = key;
    char kept[CLI_PATH_LONGEST];
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
        if (!cli_schema_kept_at(key, kept))
        {
            return NULL;
        }
        path = kept;
    }
    const unsigned char *const bytes = cli_file_held(path, size);
    if ((bytes != NULL) && (*size > CLI_SCHEMA_DOCUMENT_TEXT))
    {
        cli_file_let_go(bytes, *size);
        return NULL;
    }
    *mapped = (bytes != NULL);
    return (const char *)bytes;
}

// `size` bytes of a schema's `text` read into the store's tree, its comments taken out in a cell of its size and its
// tokens in a cell of their count, both given back after: its root, CLI_JSON_NONE where it could not be read
static unsigned int store_read(CliSchemaStore *store, const char *text, unsigned long long size)
{
    char *const stripped = cli_cell_taken_by(&g_cli_schema_cells, size + 1ull, "a schema's text");
    if (stripped == NULL)
    {
        return CLI_JSON_NONE;
    }
    store_stripped(text, size, stripped);
    const unsigned int tokens_most = cli_json_tokens_counted(stripped, size) + 1u;
    CfgJsonToken *const tokens =
        (tokens_most <= CLI_SCHEMA_TOKENS)
            ? cli_cell_taken_by(&g_cli_schema_cells, (unsigned long long)tokens_most * sizeof(CfgJsonToken),
                                "a schema's tokens")
            : NULL;
    unsigned int root = CLI_JSON_NONE;
    if ((tokens != NULL) && cli_json_room(&store->json, tokens_most, size))
    {
        char why[256];
        CliLine said = {why, sizeof(why), 0ull};
        root = cli_json_read(&store->json, stripped, size, tokens, tokens_most, &said);
        root = store->json.full ? CLI_JSON_NONE : root;
    }
    cli_cell_given_by(&g_cli_schema_cells, tokens);
    cli_cell_given_by(&g_cli_schema_cells, stripped);
    return root;
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
    int mapped = 0;
    const char *const text = store_text_of(key, &size, &mapped);
    const unsigned int root =
        ((text != NULL) && cli_utf8_whole(text, size)) ? store_read(store, text, size) : CLI_JSON_NONE;
    if (mapped)
    {
        cli_file_let_go((const unsigned char *)text, size);
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

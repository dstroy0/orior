// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema_store.c: the schemas a check reaches: each read once, by its URL among the meta-schemas impero bundles or
// those stored from a fetch, or by its path, its file mapped where it lies, its comments taken out in a cell of its
// size and read in a cell of the tokens it contains, into one tree. A document larger than CLI_SCHEMA_DOCUMENT_TEXT, of
// more tokens than CLI_SCHEMA_TOKENS, or whose values the tree cannot represent, is one that could not be read
#include "../command_line/pool.h"
#include "schema_internal.h"

#include "../text/text.h"

// the most bytes of a schema read, and the most tokens it is read in
#define CLI_SCHEMA_DOCUMENT_TEXT (1u << 22u)
#define CLI_SCHEMA_TOKENS (1u << 18u)

void cli_schema_store_begin(const CliSchemaStoreCtx *args)
{
    EMBED_CALL(cli_json_tree.json_clear, CliJsonTreeCfg, .json = &args->store->json);
    args->store->documents = 0u;
    args->store->missings = 0u;
}

typedef struct
{
    const char *url;
    VerbaLinea *out;
    const char *text;
    unsigned long long size;
    char *stripped;
    const CliSchemaStore *store_read;
    const char *key;
    CliSchemaStore *store;
    unsigned long long *size_pointer;
    int *mapped;
} SchemaStoreCtx;
// a URL as a key among those impero bundles, no scheme and no `#` and no `/` at its end, into `out`
EMBED_INLINE void store_url_key(const SchemaStoreCtx *args)
{
    const char *const scheme = cellul.find(
        &(CatenaFinitaCfg){.src = args->url, .cap = CLI_POOL_BYTES, .other = "://", .other_cap = CLI_POOL_BYTES});
    const char *const rest = (scheme != NULL) ? scheme + 3 : args->url;
    unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = rest, .cap = CLI_POOL_BYTES});
    size =
        EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = rest, .at = size, .set = "#/");
    mmgr_line_put_n(args->out, rest, size);
}

int cli_schema_cache_path(const CliSchemaUrlCtx *args)
{
    char folder[CLI_PATH_MAX_LEN];
    char schemas[CLI_PATH_MAX_LEN];
    char key[CLI_PATH_MAX_LEN];
    VerbaLinea line = {key, sizeof(key), 0ULL};
    EMBED_CALL(store_url_key, SchemaStoreCtx, .url = args->url, .out = &line);
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder) || !mmgr_line_ok(&line))
    {
        return 0;
    }
    char name[CLI_PATH_MAX_LEN];
    VerbaLinea named = {name, sizeof(name), 0ULL};
    for (unsigned long long at = 0ULL; at < line.at;)
    {
        unsigned long long length = 0ULL;
        const unsigned int letter =
            EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = key, .size = line.at, .at = at, .length = &length);
        const int allowed = ((letter >= 'a') && (letter <= 'z')) || ((letter >= 'A') && (letter <= 'Z')) ||
                            ((letter >= '0') && (letter <= '9')) || (letter == '.') || (letter == '-');
        mmgr_line_ch(&named, allowed ? (char)letter : '_');
        at += length;
    }
    mmgr_line_put(&named, ".json");
    if (mmgr_line_finish(&named) == 0u)
    {
        return 0;
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "schemas", .joined = schemas);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = schemas, .relative = name, .joined = args->out_capacity);
    return 1;
}

// `size` bytes of JSON with their comments taken out, each by spaces and a block's lines preserved, into `stripped`,
// which is sized for `size` bytes
EMBED_INLINE void store_strip(const SchemaStoreCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    int quoted = 0;
    for (unsigned long long at = 0ULL; at < size;)
    {
        if (quoted)
        {
            args->stripped[at] = text[at];
            if ((text[at] == '\\') && (at + 1u < size))
            {
                args->stripped[at + 1u] = text[at + 1u];
                at += 2u;
                continue;
            }
            quoted = (text[at] != '"');
            at += 1u;
            continue;
        }
        unsigned long long end = 0ULL;
        if (EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = text + at, .size = size - at,
                       .prefix = "//"))
        {
            end = EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = text + at, .length_value = size - at,
                             .required = "\n", .size = 1u, .at_pointer = &end)
                      ? at + end
                      : size;
            memor.set(&(MemoriaCfg){.dst = args->stripped + at, .val = ' ', .bytes = (embed_word)(end - at)});
            at = end;
            continue;
        }
        if (EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = text + at, .size = size - at,
                       .prefix = "/*"))
        {
            end = EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = text + at + 2u,
                             .length_value = size - at - 2u, .required = "*/", .size = 2u, .at_pointer = &end)
                      ? at + 2u + end + 2u
                      : size;
            for (unsigned long long one = at; one < end; one += 1u)
            {
                args->stripped[one] = (text[one] == '\n') ? '\n' : ' ';
            }
            at = end;
            continue;
        }
        quoted = (text[at] == '"');
        args->stripped[at] = text[at];
        at += 1u;
    }
}

// whether the store could not find `key` before
EMBED_INLINE int store_missing(const SchemaStoreCtx *args)
{
    for (unsigned int at = 0u; at < args->store_read->missings; at += 1u)
    {
        if (cellul.eq(
                &(CatenaFinitaCfg){.src = args->store_read->missing[at], .other = args->key, .cap = CLI_POOL_BYTES}))
        {
            return 1;
        }
    }
    return 0;
}

// `key` recorded among those the store could not find
EMBED_INLINE void store_missed(const SchemaStoreCtx *args)
{
    if (args->store->missings < CLI_SCHEMA_MISSING)
    {
        VerbaLinea line = {args->store->missing[args->store->missings], CLI_PATH_MAX_LEN, 0ULL};
        mmgr_line_put(&line, args->key);
        (void)mmgr_line_finish(&line);
        args->store->missings += 1u;
    }
}

// the text of the document at `key`, the bundled meta-schemas first for a URL, and otherwise its file mapped where it
// lies, with 1 in `mapped`: its bytes and size, NULL where none is or its file is larger than CLI_SCHEMA_DOCUMENT_TEXT
EMBED_INLINE const char *store_text_get(const SchemaStoreCtx *args)
{
    unsigned long long *size = args->size_pointer;
    int *mapped = args->mapped;
    *mapped = 0;
    const char *path = args->key;
    char path_buf[CLI_PATH_MAX_LEN];
    if (cellul.find(&(CatenaFinitaCfg){
            .src = args->key, .cap = CLI_POOL_BYTES, .other = "://", .other_cap = CLI_POOL_BYTES}) != NULL)
    {
        char required[CLI_PATH_MAX_LEN];
        VerbaLinea line = {required, sizeof(required), 0ULL};
        EMBED_CALL(store_url_key, SchemaStoreCtx, .url = args->key, .out = &line);
        for (unsigned int at = 0u; at < 5u; at += 1u)
        {
            if ((cellul.len(&(CatenaFinitaCfg){.src = g_cli_schema_bundled[at][0], .cap = CLI_POOL_BYTES}) ==
                 line.at) &&
                (memor.cmp(&(MemoriaCfg){.src = g_cli_schema_bundled[at][0], .other = required, .bytes = line.at}) ==
                 0))
            {
                *size = cellul.len(&(CatenaFinitaCfg){.src = g_cli_schema_bundled[at][1], .cap = CLI_POOL_BYTES});
                return g_cli_schema_bundled[at][1];
            }
        }
        if (!EMBED_CALL(cli_schema_url_internal.cache_path, CliSchemaUrlCtx, .url = args->key,
                        .out_capacity = path_buf))
        {
            return NULL;
        }
        path = path_buf;
    }
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                  .size_pointer = size, .owner = g_cli_schema_owner);
    if ((bytes != NULL) && (*size > CLI_SCHEMA_DOCUMENT_TEXT))
    {
        g_cli_guard->persistent_buf_release((void *)(embed_word)bytes, (g_cli_schema_owner));
        return NULL;
    }
    *mapped = (bytes != NULL);
    return (const char *)bytes;
}

// `size` bytes of a schema's `text` read into the store's tree, its comments taken out in a cell of its size and its
// tokens in a cell of their count, both given back after: its root, CLI_JSON_NONE where it could not be read
EMBED_INLINE unsigned int store_read(const SchemaStoreCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    char *const stripped = g_cli_guard->temporary_buf_alloc(size + 1ULL, g_cli_schema_owner);
    if (stripped == NULL)
    {
        return CLI_JSON_NONE;
    }
    EMBED_CALL(store_strip, SchemaStoreCtx, .text = text, .size = size, .stripped = stripped);
    const unsigned int tokens_max =
        EMBED_CALL(cli_json_tree_text.json_tokens_count, CliJsonTreeTextCfg, .text = stripped, .size = size) + 1u;
    CfgJsonToken *const tokens = (tokens_max <= CLI_SCHEMA_TOKENS)
                                     ? g_cli_guard->temporary_buf_alloc(
                                           (unsigned long long)tokens_max * sizeof(CfgJsonToken), g_cli_schema_owner)
                                     : NULL;
    unsigned int root = CLI_JSON_NONE;
    if ((tokens != NULL) && EMBED_CALL(cli_json_tree_nodes.json_capacity, CliJsonTreeNodesCfg,
                                       .json = &args->store->json, .nodes = tokens_max, .text_u64 = size))
    {
        char why[256];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        root = EMBED_CALL(cli_json_tree_text_size_tokens.json_read, CliJsonTreeTextSizeTokensCfg,
                          .json = &args->store->json, .text = stripped, .size = size, .tokens = tokens,
                          .tokens_max = tokens_max, .err_msg = &err_msg);
        root = args->store->json.full ? CLI_JSON_NONE : root;
    }
    g_cli_guard->persistent_buf_release(tokens, g_cli_schema_owner);
    g_cli_guard->persistent_buf_release(stripped, g_cli_schema_owner);
    return root;
}

unsigned int cli_schema_load(const CliSchemaStoreCtx *args)
{
    for (unsigned int at = 0u; at < args->store->documents; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->store->key[at], .other = args->key_read, .cap = CLI_POOL_BYTES}))
        {
            return args->store->root[at];
        }
    }
    if ((args->store->documents >= CLI_SCHEMA_DOCUMENTS) ||
        EMBED_CALL(store_missing, SchemaStoreCtx, .store_read = args->store, .key = args->key_read))
    {
        return CLI_SCHEMA_NONE;
    }
    unsigned long long size = 0ULL;
    int mapped = 0;
    const char *const text =
        EMBED_CALL(store_text_get, SchemaStoreCtx, .key = args->key_read, .size_pointer = &size, .mapped = &mapped);
    const unsigned int root =
        ((text != NULL) && EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg, .bytes = text, .size = size))
            ? EMBED_CALL(store_read, SchemaStoreCtx, .store = args->store, .text = text, .size = size)
            : CLI_JSON_NONE;
    if (mapped)
    {
        g_cli_guard->persistent_buf_release((void *)(embed_word)((const unsigned char *)text), (g_cli_schema_owner));
    }
    if (root == CLI_JSON_NONE)
    {
        EMBED_CALL(store_missed, SchemaStoreCtx, .store = args->store, .key = args->key_read);
        return CLI_SCHEMA_NONE;
    }
    VerbaLinea line = {args->store->key[args->store->documents], CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->key_read);
    (void)mmgr_line_finish(&line);
    args->store->root[args->store->documents] = root;
    args->store->documents += 1u;
    return root;
}

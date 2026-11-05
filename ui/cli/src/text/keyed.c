// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// keyed.c: texts stored by a key, every store's entries in the modules' shared cells: each entry its key, the text
// beside it and its text one after another in a cell of its own, each ended by a 0; an entry found by its key's hash
// and then its key
#include "keyed.h"
#include "../command_line/callback_cfg.h"

#include "../command_line/cells.h"

// the hash of a key, FNV-1a over its bytes
unsigned int cli_keyed_hash(const CliKeyedKeyAdjacentCfg *args)
{
    unsigned int hash = 2166136261u;
    for (const char *at = args->key; *at != '\0'; at += 1)
    {
        hash = (hash ^ (unsigned char)*at) * 16777619u;
    }
    return hash;
}

typedef struct
{
    const CliKeyed *keyed;
    const char *key;
    unsigned int hash;
    unsigned long long size;
    char *cell;
    const char *why;
    CliKeyed *keyed_out;
} KeyedCtx;
// the entry stored under `key`, whose hash is `hash`, `entries` where none is
EMBED_INLINE unsigned int keyed_find(const KeyedCtx *args)
{
    const CliKeyed *keyed = args->keyed;
    for (unsigned int at = 0u; at < keyed->entries; at += 1u)
    {
        const CliKeyedEntry *const one = EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = keyed, .at = at);
        if (one->used && (one->hash == args->hash) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = one->cell, .other = args->key, .cap = CLI_CELLS_POOL})))
        {
            return at;
        }
    }
    return keyed->entries;
}

// a cell of `size` bytes for `keyed`'s entry, or NULL; and a cell given back
EMBED_INLINE char *keyed_cell_alloc(const KeyedCtx *args)
{
    return g_cli_cells->persistent_buf_alloc(args->size, &g_cli_text_cells);
}

// that `key` could not be stored by `keyed`, and why, reported on the standard error
EMBED_INLINE void keyed_reject(const KeyedCtx *args)
{
    char msg_buf[CLI_PATH_MAX_LEN + 256u];
    VerbaLinea line = {msg_buf, sizeof(msg_buf), 0ULL};
    mmgr_line_put(&line, "orior: ");
    mmgr_line_put(&line, args->keyed->name);
    mmgr_line_put(&line, ": ");
    mmgr_line_put(&line, args->key);
    mmgr_line_put(&line, " is not stored: ");
    mmgr_line_put(&line, args->why);
    mmgr_line_ch(&line, '\n');
    EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 1, .text = msg_buf, .length = line.at);
}

void cli_keyed_begin(const CliKeyedNameCfg *args)
{
    const char *name = args->name;
    unsigned int entries_max = args->entries_max;
    memor.set(&(MemoriaCfg){.dst = args->keyed_out, .val = 0, .bytes = sizeof(*args->keyed_out)});
    args->keyed_out->name = name;
    args->keyed_out->entries_max =
        (entries_max < (CLI_KEYED_PAGE * CLI_KEYED_PAGES)) ? entries_max : (CLI_KEYED_PAGE * CLI_KEYED_PAGES);
}

CliKeyedEntry *cli_keyed_entry(const CliKeyedCfg *args)
{
    return &args->keyed->page[args->at / CLI_KEYED_PAGE][args->at % CLI_KEYED_PAGE];
}

// the entries that contain one moved down together, in the order they stand, the pages they leave given back
EMBED_INLINE void keyed_compact(const KeyedCtx *args)
{
    CliKeyed *keyed = args->keyed_out;
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < keyed->entries; at += 1u)
    {
        if (EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = keyed, .at = at)->used)
        {
            *EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = keyed, .at = count) =
                *EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = keyed, .at = at);
            count += 1u;
        }
    }
    keyed->entries = count;
    const unsigned int reached = (keyed->entries + CLI_KEYED_PAGE - 1u) / CLI_KEYED_PAGE;
    for (unsigned int page = reached; page < CLI_KEYED_PAGES; page += 1u)
    {
        g_cli_cells->persistent_buf_release(((char *)keyed->page[page]), &g_cli_text_cells);
        keyed->page[page] = NULL;
    }
}

int cli_keyed_append(const CliKeyedKeyAdjacentCfg *args)
{
    cli_keyed_key_adjacent.remove(args);
    const size_t key_size = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_CELLS_POOL});
    const size_t adjacent_size = cellul.len(&(CatenaFinitaCfg){.src = args->adjacent_value, .cap = CLI_CELLS_POOL});
    const unsigned long long needed = key_size + 1u + adjacent_size + 1u + args->size_value + 1u;
    if (args->keyed_out->entries == args->keyed_out->entries_max)
    {
        EMBED_CALL(keyed_compact, KeyedCtx, .keyed_out = args->keyed_out);
    }
    const unsigned int page = args->keyed_out->entries / CLI_KEYED_PAGE;
    if ((args->keyed_out->entries < args->keyed_out->entries_max) && (args->keyed_out->page[page] == NULL))
    {
        args->keyed_out->page[page] = (CliKeyedEntry *)(void *)EMBED_CALL(
            keyed_cell_alloc, KeyedCtx, .keyed = args->keyed_out, .size = CLI_KEYED_PAGE * sizeof(CliKeyedEntry));
    }
    if ((args->keyed_out->entries == args->keyed_out->entries_max) || (args->keyed_out->page[page] == NULL))
    {
        EMBED_CALL(keyed_reject, KeyedCtx, .keyed = args->keyed_out, .key = args->key, .why = "every entry is taken");
        return 0;
    }
    char *const place = EMBED_CALL(keyed_cell_alloc, KeyedCtx, .keyed = args->keyed_out, .size = needed);
    if (place == NULL)
    {
        EMBED_CALL(keyed_reject, KeyedCtx, .keyed = args->keyed_out, .key = args->key,
                   .why = "the cells have no capacity left for it");
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = place, .src = args->key, .bytes = key_size + 1u});
    memor.cpy(&(MemoriaCfg){.dst = place + key_size + 1u, .src = args->adjacent_value, .bytes = adjacent_size + 1u});
    // a size the cell allocated fits a size_t
    memor.cpy(&(MemoriaCfg){
        .dst = place + key_size + 1u + adjacent_size + 1u, .src = args->text, .bytes = (size_t)args->size_value});
    place[key_size + 1u + adjacent_size + 1u + args->size_value] = '\0';
    // a key's and a second text's sizes are a path's and a URI's, far inside what an unsigned int represents
    *EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = args->keyed_out, .at = args->keyed_out->entries) =
        (CliKeyedEntry){
            1, cli_keyed_key_adjacent.hash(args), place, needed, (unsigned int)key_size, (unsigned int)adjacent_size};
    args->keyed_out->entries += 1u;
    return 1;
}

const char *cli_keyed_get_hash(const CliKeyedKeyCfg *args)
{
    const char **adjacent = args->adjacent;
    unsigned long long *size = args->size;
    const unsigned int at =
        EMBED_CALL(keyed_find, KeyedCtx, .keyed = args->keyed, .key = args->key, .hash = args->hash);
    if (at == args->keyed->entries)
    {
        *adjacent = "";
        *size = 0ULL;
        return NULL;
    }
    const CliKeyedEntry *const one = EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = args->keyed, .at = at);
    *adjacent = one->cell + one->key_size + 1u;
    *size = one->size - one->key_size - one->adjacent_size - 3u;
    return one->cell + one->key_size + 1u + one->adjacent_size + 1u;
}

const char *cli_keyed_get(const CliKeyedKeyAdjacentCfg *args)
{
    return EMBED_CALL(cli_keyed_key.get_hash, CliKeyedKeyCfg, .keyed = args->keyed, .key = args->key,
                      .hash = cli_keyed_key_adjacent.hash(args), .adjacent = args->adjacent, .size = args->size);
}

void cli_keyed_remove(const CliKeyedKeyAdjacentCfg *args)
{
    const unsigned int at = EMBED_CALL(keyed_find, KeyedCtx, .keyed = args->keyed_out, .key = args->key,
                                       .hash = cli_keyed_key_adjacent.hash(args));
    if (at < args->keyed_out->entries)
    {
        g_cli_cells->persistent_buf_release(
            (EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = args->keyed_out, .at = at)->cell),
            &g_cli_text_cells);
        EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = args->keyed_out, .at = at)->used = 0;
        EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = args->keyed_out, .at = at)->cell = NULL;
    }
}

void cli_keyed_clear(const CliKeyedNameCfg *args)
{
    for (unsigned int at = 0u; at < args->keyed_out->entries; at += 1u)
    {
        if (EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = args->keyed_out, .at = at)->used)
        {
            g_cli_cells->persistent_buf_release(
                (EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = args->keyed_out, .at = at)->cell),
                &g_cli_text_cells);
        }
    }
    args->keyed_out->entries = 0u;
    const unsigned int reached = (args->keyed_out->entries + CLI_KEYED_PAGE - 1u) / CLI_KEYED_PAGE;
    for (unsigned int page = reached; page < CLI_KEYED_PAGES; page += 1u)
    {
        g_cli_cells->persistent_buf_release(((char *)args->keyed_out->page[page]), &g_cli_text_cells);
        args->keyed_out->page[page] = NULL;
    }
}

void cli_keyed_each(const CliKeyedEachCfg *args)
{
    const CliKeyed *keyed = args->keyed;
    for (unsigned int at = 0u; at < keyed->entries; at += 1u)
    {
        const CliKeyedEntry *const one = EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = keyed, .at = at);
        if (one->used)
        {
            const char *const key = one->cell;
            const char *const adjacent = key + one->key_size + 1u;
            EMBED_CALL(args->each, CliEachKeyAdjacentTextSizeCfg, .context = args->context, .key = key,
                       .adjacent = adjacent, .text = adjacent + one->adjacent_size + 1u,
                       .size = one->size - one->key_size - one->adjacent_size - 3u);
        }
    }
}

unsigned long long cli_keyed_listing_size(const CliKeyedCfg *args)
{
    unsigned long long size = 0ULL;
    for (unsigned int at = 0u; at < args->keyed->entries; at += 1u)
    {
        const CliKeyedEntry *const one = EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = args->keyed, .at = at);
        size += one->used ? ((unsigned long long)one->key_size + one->adjacent_size + 2ULL) : 0ULL;
    }
    return size;
}

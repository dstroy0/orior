// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// keyed.c: texts stored by a key, every store's entries in the modules' shared cells: each entry its key, the text
// beside it and its text one after another in a movable cell of its own, each ended by a 0; the entries themselves in
// pages, each a movable cell; an entry found by its key's hash and then its key
#include "keyed.h"
#include "../command_line/callback_cfg.h"

// the bytes of a text that crosses from one cell to another at a time
#define KEYED_WINDOW 1024u

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
    const char *why;
    CliKeyed *keyed_out;
    unsigned int at;
    const CliKeyedEntry *entry;
} KeyedCtx;

// the entry at `at` written into its page
EMBED_INLINE void keyed_entry_write(const KeyedCtx *args)
{
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = args->keyed_out->page[args->at / CLI_KEYED_PAGE],
                            .offset = (embed_word)(args->at % CLI_KEYED_PAGE) * sizeof(CliKeyedEntry),
                            .src = args->entry,
                            .bytes = sizeof(CliKeyedEntry)});
}

CliKeyedEntry cli_keyed_entry(const CliKeyedCfg *args)
{
    CliKeyedEntry entry = {0, 0u, NULL, 0ULL, 0u, 0u, 0ULL, 0ULL};
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = args->keyed->page[args->at / CLI_KEYED_PAGE],
                            .offset = (embed_word)(args->at % CLI_KEYED_PAGE) * sizeof(CliKeyedEntry),
                            .dst = &entry,
                            .bytes = sizeof(entry)});
    return entry;
}

// the entry stored under `key`, whose hash is `hash`, `entries` where none is: an entry's key is the same where its
// size and its bytes, 0 and all, are
EMBED_INLINE unsigned int keyed_find(const KeyedCtx *args)
{
    const CliKeyed *keyed = args->keyed;
    const embed_word key_size = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_POOL_BYTES});
    for (unsigned int at = 0u; at < keyed->entries; at += 1u)
    {
        const CliKeyedEntry one = EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = keyed, .at = at);
        if (one.used && (one.hash == args->hash) && (one.key_size == key_size) &&
            (g_cli_guard->movable_buf_compare(
                 &(CarcerMovableCfg){.cell = one.cell, .offset = 0u, .src = args->key, .bytes = key_size + 1u}) == 0))
        {
            return at;
        }
    }
    return keyed->entries;
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

// the pages from `at` on given back
EMBED_INLINE void keyed_pages_release(const KeyedCtx *args)
{
    for (unsigned int page = args->at; page < CLI_KEYED_PAGES; page += 1u)
    {
        if (args->keyed_out->page[page] != NULL)
        {
            (void)g_cli_guard->movable_buf_release(args->keyed_out->page[page], g_cli_text_owner);
        }
        args->keyed_out->page[page] = NULL;
    }
}

// the entries that contain one moved down together, in the order they stand, the pages they leave given back
EMBED_INLINE void keyed_compact(const KeyedCtx *args)
{
    CliKeyed *keyed = args->keyed_out;
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < keyed->entries; at += 1u)
    {
        const CliKeyedEntry one = EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = keyed, .at = at);
        if (one.used)
        {
            EMBED_CALL(keyed_entry_write, KeyedCtx, .keyed_out = keyed, .at = count, .entry = &one);
            count += 1u;
        }
    }
    keyed->entries = count;
    EMBED_CALL(keyed_pages_release, KeyedCtx, .keyed_out = keyed, .at = (count + CLI_KEYED_PAGE - 1u) / CLI_KEYED_PAGE);
}

int cli_keyed_append(const CliKeyedKeyAdjacentCfg *args)
{
    cli_keyed_key_adjacent.remove(args);
    const embed_word key_size = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_POOL_BYTES});
    const embed_word adjacent_size = cellul.len(&(CatenaFinitaCfg){.src = args->adjacent_value, .cap = CLI_POOL_BYTES});
    const unsigned long long text_at = key_size + 1u + adjacent_size + 1u;
    const unsigned long long needed = text_at + args->size_value + 1u;
    if (args->keyed_out->entries == args->keyed_out->entries_max)
    {
        EMBED_CALL(keyed_compact, KeyedCtx, .keyed_out = args->keyed_out);
    }
    const unsigned int page = args->keyed_out->entries / CLI_KEYED_PAGE;
    if ((args->keyed_out->entries < args->keyed_out->entries_max) && (args->keyed_out->page[page] == NULL))
    {
        args->keyed_out->page[page] =
            g_cli_guard->movable_buf_alloc(CLI_KEYED_PAGE * sizeof(CliKeyedEntry), g_cli_text_owner);
    }
    if ((args->keyed_out->entries == args->keyed_out->entries_max) || (args->keyed_out->page[page] == NULL))
    {
        EMBED_CALL(keyed_reject, KeyedCtx, .keyed = args->keyed_out, .key = args->key, .why = "every entry is taken");
        return 0;
    }
    const CarcerMovableEntry *const place = g_cli_guard->movable_buf_alloc(needed, g_cli_text_owner);
    if (place == NULL)
    {
        EMBED_CALL(keyed_reject, KeyedCtx, .keyed = args->keyed_out, .key = args->key,
                   .why = "the cells have no capacity left for it");
        return 0;
    }
    static const char s_zero = '\0';
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = place, .offset = 0u, .src = args->key, .bytes = key_size + 1u});
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
        .cell = place, .offset = key_size + 1u, .src = args->adjacent_value, .bytes = adjacent_size + 1u});
    // a size the cell allocated fits a embed_word; a text out of a cell crosses in windows
    if (args->text_cell == NULL)
    {
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = place, .offset = (embed_word)text_at, .src = args->text, .bytes = (embed_word)args->size_value});
    }
    for (unsigned long long at = 0ULL; (args->text_cell != NULL) && (at < args->size_value);)
    {
        char window[KEYED_WINDOW];
        const unsigned long long left = args->size_value - at;
        const unsigned long long bytes = (left < sizeof(window)) ? left : sizeof(window);
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = args->text_cell,
                                                                .offset = (embed_word)(args->text_at + at),
                                                                .dst = window,
                                                                .bytes = (embed_word)bytes});
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = place, .offset = (embed_word)(text_at + at), .src = window, .bytes = (embed_word)bytes});
        at += bytes;
    }
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = place, .offset = (embed_word)(needed - 1ULL), .src = &s_zero, .bytes = 1u});
    // a key's and a second text's sizes are a path's and a URI's, far inside what an unsigned int represents
    const CliKeyedEntry entry = {1,
                                 cli_keyed_key_adjacent.hash(args),
                                 place,
                                 needed,
                                 (unsigned int)key_size,
                                 (unsigned int)adjacent_size,
                                 text_at,
                                 args->size_value};
    EMBED_CALL(keyed_entry_write, KeyedCtx, .keyed_out = args->keyed_out, .at = args->keyed_out->entries,
               .entry = &entry);
    args->keyed_out->entries += 1u;
    return 1;
}

unsigned int cli_keyed_find_hash(const CliKeyedKeyCfg *args)
{
    const unsigned int at =
        EMBED_CALL(keyed_find, KeyedCtx, .keyed = args->keyed, .key = args->key, .hash = args->hash);
    return (at == args->keyed->entries) ? CLI_KEYED_NONE : at;
}

unsigned int cli_keyed_find(const CliKeyedKeyAdjacentCfg *args)
{
    return EMBED_CALL(cli_keyed_key.find_hash, CliKeyedKeyCfg, .keyed = args->keyed, .key = args->key,
                      .hash = cli_keyed_key_adjacent.hash(args));
}

void cli_keyed_remove_at(const CliKeyedCfg *args)
{
    CliKeyedEntry one = EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = args->keyed_out, .at = args->at);
    if (one.used && (one.cell != NULL))
    {
        (void)g_cli_guard->movable_buf_release(one.cell, g_cli_text_owner);
    }
    one.used = 0;
    one.cell = NULL;
    EMBED_CALL(keyed_entry_write, KeyedCtx, .keyed_out = args->keyed_out, .at = args->at, .entry = &one);
}

void cli_keyed_remove(const CliKeyedKeyAdjacentCfg *args)
{
    const unsigned int at = EMBED_CALL(keyed_find, KeyedCtx, .keyed = args->keyed_out, .key = args->key,
                                       .hash = cli_keyed_key_adjacent.hash(args));
    if (at < args->keyed_out->entries)
    {
        EMBED_CALL(cli_keyed_table.remove_at, CliKeyedCfg, .keyed_out = args->keyed_out, .at = at);
    }
}

void cli_keyed_clear(const CliKeyedNameCfg *args)
{
    for (unsigned int at = 0u; at < args->keyed_out->entries; at += 1u)
    {
        const CliKeyedEntry one = EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = args->keyed_out, .at = at);
        if (one.used && (one.cell != NULL))
        {
            (void)g_cli_guard->movable_buf_release(one.cell, g_cli_text_owner);
        }
    }
    args->keyed_out->entries = 0u;
    EMBED_CALL(keyed_pages_release, KeyedCtx, .keyed_out = args->keyed_out, .at = 0u);
}

void cli_keyed_each(const CliKeyedEachCfg *args)
{
    const CliKeyed *keyed = args->keyed;
    for (unsigned int at = 0u; at < keyed->entries; at += 1u)
    {
        const CliKeyedEntry one = EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = keyed, .at = at);
        if (one.used)
        {
            EMBED_CALL(args->each, CliEachKeyedEntryCfg, .context = args->context, .entry = one);
        }
    }
}

unsigned long long cli_keyed_listing_size(const CliKeyedCfg *args)
{
    unsigned long long size = 0ULL;
    for (unsigned int at = 0u; at < args->keyed->entries; at += 1u)
    {
        const CliKeyedEntry one = EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = args->keyed, .at = at);
        size += one.used ? ((unsigned long long)one.key_size + one.adjacent_size + 2ULL) : 0ULL;
    }
    return size;
}

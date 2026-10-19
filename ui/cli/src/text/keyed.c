// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// keyed.c: texts kept by a key, every keeper's entries in the modules' shared cells: each entry its key, the text
// beside it and its text one after another in a cell of its own, each ended by a 0; an entry found by its key's hash
// and then its key
#include "keyed.h"

#include "../command_line/cells.h"

// the hash of a key, FNV-1a over its bytes
static unsigned int keyed_hash(const char *key)
{
    unsigned int hash = 2166136261u;
    for (const char *at = key; *at != '\0'; at += 1)
    {
        hash = (hash ^ (unsigned char)*at) * 16777619u;
    }
    return hash;
}

// the entry kept under `key`, `entries` where none is
static unsigned int keyed_find(const CliKeyed *keyed, const char *key)
{
    const unsigned int hash = keyed_hash(key);
    for (unsigned int at = 0u; at < keyed->entries; at += 1u)
    {
        const CliKeyedEntry *const one = cli_keyed_entry(keyed, at);
        if (one->used && (one->hash == hash) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = one->cell, .other = key, .cap = CLI_CELLS_POOL})))
        {
            return at;
        }
    }
    return keyed->entries;
}

// a cell of `size` bytes for `keyed`'s entry, or NULL; and a cell given back
static char *keyed_cell_taken(const CliKeyed *keyed, unsigned long long size)
{
    return cli_cell_taken(size, keyed->name);
}

static void keyed_cell_given(char *cell)
{
    cli_cell_given(cell);
}

// that `key` could not be kept by `keyed`, and why, said on the standard error
static void keyed_refused(const CliKeyed *keyed, const char *key, const char *why)
{
    char told[CLI_PATH_LONGEST + 256u];
    CliLine line = {told, sizeof(told), 0ull};
    cli_line_text(&line, "orior: ");
    cli_line_text(&line, keyed->name);
    cli_line_text(&line, ": ");
    cli_line_text(&line, key);
    cli_line_text(&line, " is not kept: ");
    cli_line_text(&line, why);
    cli_line_character(&line, '\n');
    query_terminal_written(1, told, line.at);
}

void cli_keyed_begun(CliKeyed *keyed, const char *name, unsigned int entries_most)
{
    memor.set(&(MemoriaCfg){.dst = keyed, .val = 0, .bytes = sizeof(*keyed)});
    keyed->name = name;
    keyed->entries_most =
        (entries_most < (CLI_KEYED_PAGE * CLI_KEYED_PAGES)) ? entries_most : (CLI_KEYED_PAGE * CLI_KEYED_PAGES);
}

CliKeyedEntry *cli_keyed_entry(const CliKeyed *keyed, unsigned int at)
{
    return &keyed->page[at / CLI_KEYED_PAGE][at % CLI_KEYED_PAGE];
}

// the pages past those the entries ever taken reach given back
static void keyed_pages_given(CliKeyed *keyed)
{
    const unsigned int reached = (keyed->entries + CLI_KEYED_PAGE - 1u) / CLI_KEYED_PAGE;
    for (unsigned int page = reached; page < CLI_KEYED_PAGES; page += 1u)
    {
        keyed_cell_given((char *)keyed->page[page]);
        keyed->page[page] = NULL;
    }
}

// the entries that hold one moved down together, in the order they stand, the pages they leave given back
static void keyed_compacted(CliKeyed *keyed)
{
    unsigned int kept = 0u;
    for (unsigned int at = 0u; at < keyed->entries; at += 1u)
    {
        if (cli_keyed_entry(keyed, at)->used)
        {
            *cli_keyed_entry(keyed, kept) = *cli_keyed_entry(keyed, at);
            kept += 1u;
        }
    }
    keyed->entries = kept;
    keyed_pages_given(keyed);
}

int cli_keyed_put(CliKeyed *keyed, const char *key, const char *beside, const char *text, unsigned long long size)
{
    cli_keyed_remove(keyed, key);
    const size_t key_size = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL});
    const size_t beside_size = cellul.len(&(CatenaFinitaCfg){.src = beside, .cap = CLI_CELLS_POOL});
    const unsigned long long needed = key_size + 1u + beside_size + 1u + size + 1u;
    if (keyed->entries == keyed->entries_most)
    {
        keyed_compacted(keyed);
    }
    const unsigned int page = keyed->entries / CLI_KEYED_PAGE;
    if ((keyed->entries < keyed->entries_most) && (keyed->page[page] == NULL))
    {
        keyed->page[page] = (CliKeyedEntry *)(void *)keyed_cell_taken(keyed, CLI_KEYED_PAGE * sizeof(CliKeyedEntry));
    }
    if ((keyed->entries == keyed->entries_most) || (keyed->page[page] == NULL))
    {
        keyed_refused(keyed, key, "every entry is taken");
        return 0;
    }
    char *const place = keyed_cell_taken(keyed, needed);
    if (place == NULL)
    {
        keyed_refused(keyed, key, "the cells have no room left for it");
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = place, .src = key, .bytes = key_size + 1u});
    memor.cpy(&(MemoriaCfg){.dst = place + key_size + 1u, .src = beside, .bytes = beside_size + 1u});
    // a size the cell held fits a size_t
    memor.cpy(&(MemoriaCfg){.dst = place + key_size + 1u + beside_size + 1u, .src = text, .bytes = (size_t)size});
    place[key_size + 1u + beside_size + 1u + size] = '\0';
    // a key's and a second text's sizes are a path's and a URI's, far inside what an unsigned int holds
    *cli_keyed_entry(keyed, keyed->entries) =
        (CliKeyedEntry){1, keyed_hash(key), place, needed, (unsigned int)key_size, (unsigned int)beside_size};
    keyed->entries += 1u;
    return 1;
}

const char *cli_keyed_get(const CliKeyed *keyed, const char *key, const char **beside, unsigned long long *size)
{
    const unsigned int at = keyed_find(keyed, key);
    if (at == keyed->entries)
    {
        *beside = "";
        *size = 0ull;
        return NULL;
    }
    const CliKeyedEntry *const one = cli_keyed_entry(keyed, at);
    *beside = one->cell + one->key_size + 1u;
    *size = one->size - one->key_size - one->beside_size - 3u;
    return one->cell + one->key_size + 1u + one->beside_size + 1u;
}

void cli_keyed_remove(CliKeyed *keyed, const char *key)
{
    const unsigned int at = keyed_find(keyed, key);
    if (at < keyed->entries)
    {
        keyed_cell_given(cli_keyed_entry(keyed, at)->cell);
        cli_keyed_entry(keyed, at)->used = 0;
        cli_keyed_entry(keyed, at)->cell = NULL;
    }
}

void cli_keyed_clear(CliKeyed *keyed)
{
    for (unsigned int at = 0u; at < keyed->entries; at += 1u)
    {
        if (cli_keyed_entry(keyed, at)->used)
        {
            keyed_cell_given(cli_keyed_entry(keyed, at)->cell);
        }
    }
    keyed->entries = 0u;
    keyed_pages_given(keyed);
}

void cli_keyed_each(const CliKeyed *keyed,
                    void (*each)(void *context, const char *key, const char *beside, const char *text,
                                 unsigned long long size),
                    void *context)
{
    for (unsigned int at = 0u; at < keyed->entries; at += 1u)
    {
        const CliKeyedEntry *const one = cli_keyed_entry(keyed, at);
        if (one->used)
        {
            const char *const key = one->cell;
            const char *const beside = key + one->key_size + 1u;
            each(context, key, beside, beside + one->beside_size + 1u,
                 one->size - one->key_size - one->beside_size - 3u);
        }
    }
}

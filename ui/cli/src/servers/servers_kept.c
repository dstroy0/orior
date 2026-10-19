// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_kept.c: a checker's findings kept, by file, each file's findings by the checkers' ids in order, and the
// diagnostics of each file whose findings from that checker changed passed on whole
#include "../command_line/cells.h"
#include "servers_internal.h"

// the findings read, under the reading's lock
static CliServersReading s_found = {CLI_LOCK_INIT};

// the path of a pair of the findings, and its URI and key, each into what holds CLI_PATH_LONGEST: 1, or 0 where the
// pair names no path
static int kept_file_of(unsigned int pair, char *uri, char *key)
{
    unsigned long long size = 0ull;
    const char *const named = cli_json_string(&s_found.json, cli_json_item(&s_found.json, pair, 0u), &size);
    char path[CLI_PATH_LONGEST];
    if ((named == NULL) || (size >= sizeof(path)))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = path, .src = named, .bytes = (size_t)size});
    path[size] = '\0';
    cli_servers_uri_of(path, uri);
    cli_lsp_key_of(uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), key);
    return 1;
}

// the findings `items` of the checker `id` kept for the file of `key` and `uri`: 1 where they changed; under the
// checks' lock. The file's entry is read into a tree with room over for the findings copied in, their key and an
// object made where none was kept, and written through pieces into a cell of its size
static int kept_put(const char *key, const char *uri, const char *id, unsigned int items)
{
    const char *beside = NULL;
    unsigned long long size = 0ull;
    const char *const kept = cli_keyed_get(&g_cli_servers_checked, key, &beside, &size);
    unsigned int nodes_more = 1u;
    unsigned long long text_more = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}) + 1ull;
    cli_json_extent(&s_found.json, items, &nodes_more, &text_more);
    char quiet[256];
    CliLine said = {quiet, sizeof(quiet), 0ull};
    CliJson json;
    cli_json_begun(&json, NULL, 0u, NULL, 0ull);
    unsigned int entry = CLI_JSON_NONE;
    if (kept != NULL)
    {
        entry = cli_json_taken(&json, kept, size, nodes_more, text_more, &said);
    }
    else if (!cli_json_held(&json, nodes_more, text_more, "a file's findings"))
    {
        return 0;
    }
    entry = cli_json_is(&json, entry, CFG_JSON_OBJECT) ? entry : cli_json_made(&json, CFG_JSON_OBJECT);
    const unsigned int had = cli_json_member(&json, entry, id);
    if ((had != CLI_JSON_NONE) && cli_json_same(&json, had, &s_found.json, items))
    {
        cli_json_given(&json);
        return 0;
    }
    cli_json_set(&json, entry, id, cli_json_copy_from(&json, &s_found.json, items));
    cli_json_sorted(&json, entry);
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    CliLine written;
    cli_pieces_begun(&pieces, &written, piece, sizeof(piece));
    cli_json_write(&json, entry, &written, 0u, 1);
    unsigned long long written_size = 0ull;
    char *const entry_written = cli_pieces_joined(&pieces, &written, "a file's findings written", &written_size);
    if (!json.full && (entry_written != NULL))
    {
        (void)cli_keyed_put(&g_cli_servers_checked, key, uri, entry_written, written_size);
    }
    cli_cell_given(entry_written);
    cli_json_given(&json);
    return 1;
}

void cli_servers_keep_checked(const char *id, const char *found, unsigned long long size, int always,
                              const CliServersEmit *emit)
{
    cli_servers_reading_held(&s_found);
    const unsigned int root = cli_servers_read(&s_found, found, size);
    const CliJson *const json = &s_found.json;
    // the files whose findings changed, in a cell of one place for each file the findings name
    const unsigned int files = cli_json_is(json, root, CFG_JSON_ARRAY) ? json->node[root].count : 0u;
    unsigned int *const changed_files = (files > 0u) ? cli_cell_taken((unsigned long long)files * sizeof(unsigned int),
                                                                      "the files whose findings changed")
                                                     : NULL;
    unsigned int changed = 0u;
    char uri[CLI_PATH_LONGEST];
    char key[CLI_PATH_LONGEST];
    for (unsigned int pair = (changed_files != NULL) ? json->node[root].first : CLI_JSON_NONE; pair != CLI_JSON_NONE;
         pair = json->node[pair].next)
    {
        const unsigned int items = cli_json_item(json, pair, 1u);
        if (!cli_json_is(json, items, CFG_JSON_ARRAY) || !kept_file_of(pair, uri, key))
        {
            continue;
        }
        cli_lock_take(&g_cli_servers_checks_lock);
        const int put = kept_put(key, uri, id, items);
        cli_lock_give(&g_cli_servers_checks_lock);
        if (put || always)
        {
            changed_files[changed] = pair;
            changed += 1u;
        }
    }
    for (unsigned int at = 0u; at < changed; at += 1u)
    {
        if (kept_file_of(changed_files[at], uri, key))
        {
            cli_servers_emit_whole(key, uri, emit);
        }
    }
    cli_cell_given(changed_files);
    cli_servers_reading_released(&s_found);
}

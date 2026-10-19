// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_named.c: the checkers the reader names, by name or id: kept by their ids, a checker no longer named taking
// its findings with it, each file the editor holds checked again with them, and the tree where its check is on; the
// names no checker answers to given back
#include "../command_line/cells.h"
#include "../toolchains/checkers.h"
#include "servers_internal.h"

static CliLock s_lock = CLI_LOCK_INIT;

// a key and the text beside it put on the line that is `context`, each ended by a 0
static void named_kept(void *context, const char *key, const char *beside, const char *text, unsigned long long size)
{
    (void)text;
    (void)size;
    CliLine *const files = context;
    cli_line_counted(files, key, cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1ull);
    cli_line_counted(files, beside, cellul.len(&(CatenaFinitaCfg){.src = beside, .cap = CLI_CELLS_POOL}) + 1ull);
}

// the keys of `keyed` and the texts beside them, each ended by a 0, in a cell of their size, their size into `size`:
// the cell, or NULL where none are or the cell could not be taken; under the keeper's lock
static char *named_files(CliKeyed *keyed, unsigned long long *size)
{
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    CliLine files;
    cli_pieces_begun(&pieces, &files, piece, sizeof(piece));
    cli_keyed_each(keyed, named_kept, &files);
    return cli_pieces_joined(&pieces, &files, "the files a naming of checkers reaches", size);
}

// whether `id` is among the checkers named
static int named_among(const char *id)
{
    for (unsigned int at = 0u; at < g_cli_servers.checkers; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = g_cli_servers.checker[at], .other = id, .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return cellul.eq(&(CatenaFinitaCfg){.src = id, .other = CLI_SERVERS_OWN, .cap = CLI_CELLS_POOL});
}

// the findings of the checkers no longer named taken out of the file of `key`: 1 where it had some; under the
// servers' and the checks' locks. The file's entry is read into a tree of its size and written through pieces into a
// cell of the size it writes to
static int named_dropped(const char *key, const char *uri)
{
    const char *beside = NULL;
    unsigned long long size = 0ull;
    const char *const kept = cli_keyed_get(&g_cli_servers_checked, key, &beside, &size);
    if (kept == NULL)
    {
        return 0;
    }
    CliJson json;
    cli_json_begun(&json, NULL, 0u, NULL, 0ull);
    char quiet[256];
    CliLine said = {quiet, sizeof(quiet), 0ull};
    const unsigned int entry = cli_json_taken(&json, kept, size, 0u, 0ull, &said);
    int dropped = 0;
    unsigned int id = cli_json_is(&json, entry, CFG_JSON_OBJECT) ? json.node[entry].first : CLI_JSON_NONE;
    while (id != CLI_JSON_NONE)
    {
        const unsigned int next = json.node[id].next;
        const char *const name = json.text + json.node[id].key;
        if (!named_among(name))
        {
            cli_json_remove(&json, entry, name);
            dropped = 1;
        }
        id = next;
    }
    if (dropped)
    {
        char piece[CLI_SERVERS_PIECE];
        CliPieces pieces;
        CliLine written;
        cli_pieces_begun(&pieces, &written, piece, sizeof(piece));
        cli_json_write(&json, entry, &written, 0u, 1);
        unsigned long long written_size = 0ull;
        char *const entry_written = cli_pieces_joined(&pieces, &written, "a file's findings written", &written_size);
        if (entry_written != NULL)
        {
            (void)cli_keyed_put(&g_cli_servers_checked, key, uri, entry_written, written_size);
        }
        cli_cell_given(entry_written);
    }
    cli_json_given(&json);
    return dropped;
}

// the checkers no longer named take their findings with them, each file that had some passed on whole
static void named_findings_dropped(const CliServersEmit *emit)
{
    char dropped_piece[CLI_SERVERS_PIECE];
    CliPieces dropped_pieces;
    CliLine dropped;
    cli_pieces_begun(&dropped_pieces, &dropped, dropped_piece, sizeof(dropped_piece));
    cli_lock_take(&g_cli_servers_checks_lock);
    unsigned long long size = 0ull;
    char *const files = named_files(&g_cli_servers_checked, &size);
    for (unsigned long long at = 0ull; (files != NULL) && (at < size);)
    {
        const char *const key = files + at;
        const char *const uri = key + cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u;
        if (named_dropped(key, uri))
        {
            cli_line_counted(&dropped, key, cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1ull);
            cli_line_counted(&dropped, uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}) + 1ull);
        }
        at += cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u +
              cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}) + 1u;
    }
    cli_lock_give(&g_cli_servers_checks_lock);
    cli_cell_given(files);
    cli_lock_give(&g_cli_servers.lock);
    unsigned long long dropped_size = 0ull;
    char *const passed = cli_pieces_joined(&dropped_pieces, &dropped, "the files whose findings went", &dropped_size);
    for (unsigned long long at = 0ull; (passed != NULL) && (at < dropped_size);)
    {
        const char *const key = passed + at;
        const char *const uri = key + cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u;
        cli_servers_emit_whole(key, uri, emit);
        at += cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u +
              cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}) + 1u;
    }
    cli_cell_given(passed);
}

int cli_servers_set_checkers(const char *const *names, unsigned int count, const char *root, const CliServersEmit *emit,
                             CliLine *out, CliLine *said)
{
    (void)said;
    cli_lock_take(&s_lock);
    const unsigned long long start = out->at;
    cli_line_character(out, '[');
    cli_lock_take(&g_cli_servers.lock);
    g_cli_servers.checkers = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        unsigned long long from = 0ull;
        unsigned long long to = 0ull;
        cli_text_trimmed(names[at], cellul.len(&(CatenaFinitaCfg){.src = names[at], .cap = CLI_CELLS_POOL}), &from,
                         &to);
        char name[CLI_NAME_LONGEST];
        if ((to == from) || (to - from >= sizeof(name)))
        {
            continue;
        }
        memor.cpy(&(MemoriaCfg){.dst = name, .src = names[at] + from, .bytes = (size_t)(to - from)});
        name[to - from] = '\0';
        char id[CLI_NAME_LONGEST];
        if (cli_checkers_named(name, id) && (g_cli_servers.checkers < CLI_SERVERS_CHECKERS))
        {
            memor.cpy(
                &(MemoriaCfg){.dst = g_cli_servers.checker[g_cli_servers.checkers], .src = id, .bytes = sizeof(id)});
            g_cli_servers.checkers += 1u;
            continue;
        }
        cli_line_text(out, (out->at > start + 1u) ? "," : "");
        cli_json_quoted(out, name, to - from);
    }
    cli_line_character(out, ']');
    // a checker no longer named takes its findings with it; the servers' lock is given back there
    named_findings_dropped(emit);
    if (root == NULL)
    {
        cli_lock_give(&s_lock);
        return 1;
    }
    cli_lock_take(&g_cli_servers.lock);
    g_cli_servers.told = *emit;
    g_cli_servers.has_told = 1;
    CliLine checked = {g_cli_servers.checked_root, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&checked, root);
    (void)cli_line_closed(&checked);
    cli_lock_give(&g_cli_servers.lock);
    cli_lock_take(&g_cli_servers_texts_lock);
    unsigned long long size = 0ull;
    char *const held = named_files(&g_cli_servers_texts, &size);
    cli_lock_give(&g_cli_servers_texts_lock);
    for (unsigned long long at = 0ull; (held != NULL) && (at < size);)
    {
        const char *const key = held + at;
        const char *const path = key + cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u;
        unsigned long long text_size = 0ull;
        char *const text = cli_servers_text_taken(path, &text_size);
        if (text != NULL)
        {
            cli_servers_run_checkers(root, path, text, text_size, 1, emit);
        }
        cli_cell_given(text);
        at += cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u +
              cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u;
    }
    cli_cell_given(held);
    if (cli_servers_check_on())
    {
        cli_servers_check_tree_with_checkers(root, emit);
    }
    cli_lock_give(&s_lock);
    return 1;
}

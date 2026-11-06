// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_parse.c: the parse of a file the editor has open: orior's own where the file is Python or JavaScript, and
// its server's semantic tokens and folding ranges where its server gives them, none where neither reads it or the
// file is given its server as a slice, orior's highlighter coloring it and its server not asked. A parse is cached, by
// the file's key with its text's hash, until the file's text changes, and is read one at a time, so that a parse
// requested for again meanwhile waits for it and is answered from what was cached. Orior's own parse is counted, then
// read into storage of what it counted; a server's into storage of what its answers give
#include "../command_line/pool.h"
#include "../inspect/inspect.h"
#include "servers_internal.h"

// The head of a parse cached: its text's hash, whether it is orior's own, and its lines', runs', folds' and spans'
// counts
typedef struct
{
    unsigned long long hash;
    unsigned int integer;
    unsigned int lines;
    unsigned int runs;
    unsigned int folds;
    unsigned int ranges;
} CliServersParseHead;
// the most token types a server's legend names
#define CLI_SERVERS_LEGEND 256u

// The parse's storage, each part in a cell of its count, given back with the parsing lock: the runs, each line's first
// run and how many it has, the folds by their first and last lines, and the spans
typedef struct
{
    CliInspectRun *run;
    unsigned int *line_first;
    unsigned int *line_runs;
    unsigned int *fold_first;
    unsigned int *fold_last;
    CliInspectSpan *range;
} CliServersParseWorkState;

typedef struct
{
    unsigned int count;
    size_t size;
    const char *what;
    unsigned int runs;
    unsigned int lines;
    unsigned int folds;
    unsigned int ranges;
    const CliServersParseHead *head;
    void *into;
    const char *stored;
    unsigned long long *at;
    const char *key;
    unsigned long long hash;
    char *stored_out;
    const void *from;
    const char *path;
    const char *name;
    unsigned long long size_u64;
    CliLsp *server;
    const char *method;
    const char *uri;
    unsigned long long wait;
    CliLspAnswer *answer;
    VerbaLinea *err_msg;
    const unsigned char *legend;
    unsigned int legends;
    const CliInspectSpan *spans;
    VerbaLinea *out;
} ServersParseCtx;
unsigned long long cli_servers_hash_get(const CliServersReadStateCtx *args)
{
    unsigned long long hash = 14695981039346656037ULL;
    for (unsigned long long at = 0ULL; at < args->size; at += 1ULL)
    {
        hash = (hash ^ (unsigned char)args->text[at]) * 1099511628211ULL;
    }
    return hash;
}

void cli_servers_forget_tokens(void)
{
    for (unsigned int at = 0u; at < g_cli_servers_parses.entries; at += 1u)
    {
        CliKeyedEntry *const one =
            EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = &g_cli_servers_parses, .at = at);
        CliServersParseHead head = {0ULL, 1u, 0u, 0u, 0u, 0u};
        const unsigned long long text_at = (unsigned long long)one->key_size + 1u + one->adjacent_size + 1u;
        if (one->used && (one->size - text_at - 1u >= sizeof(head)))
        {
            memor.cpy(&(MemoriaCfg){.dst = &head, .src = one->cell + text_at, .bytes = sizeof(head)});
        }
        one->used = one->used && head.integer;
    }
}

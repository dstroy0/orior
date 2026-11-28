// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_parse.c: the parse of a file the editor has open: its server's semantic tokens and folding ranges where
// its server gives them, none where no server reads it or the file is given its server as a slice, orior's
// highlighter coloring it and its server not asked. A parse is cached, by the file's key with its text's hash, until
// the file's text changes, and is read one at a time, so that a parse requested for again meanwhile waits for it and
// is answered from what was cached, into storage of what its answers give
#include "../command_line/pool.h"
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
// the max token types a server's legend names
#define CLI_SERVERS_LEGEND 256u

typedef struct
{
    unsigned int count;
    embed_word size;
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
        CliKeyedEntry one = {0};
        (void)g_platform_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = g_cli_servers_parses.page[at / CLI_KEYED_PAGE],
                                .offset = (embed_word)(at % CLI_KEYED_PAGE) * sizeof(one),
                                .dst = &one,
                                .bytes = sizeof(one)});
        CliServersParseHead head = {0ULL, 1u, 0u, 0u, 0u, 0u};
        if (one.used && (one.text_size >= sizeof(head)))
        {
            (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
                .cell = one.cell, .offset = (embed_word)one.text_at, .dst = &head, .bytes = sizeof(head)});
        }
        if (one.used && !head.integer)
        {
            EMBED_CALL(cli_keyed_table.remove_at, CliKeyedCfg, .keyed_out = &g_cli_servers_parses, .at = at);
        }
    }
}

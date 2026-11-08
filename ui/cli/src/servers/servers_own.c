// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_own.c: the files orior_cli reads itself, its build files and JSON by its schema, checked by its own reading:
// a file as its text stands, on a thread of the servers', a later check of the same file standing in for an earlier one
// still going, the file receiving its findings, changed or the same, as a file the editor opens does; and each such
// file of the tree as the disk stores it, a file with no findings passed on only where it had some before, for those to
// clear. The names a build tool gives a file are recorded as its text is checked, for its server's reports of them as
// undefined to be passed over. Each job is made in a cell of its size and handed over whole, and what it finds is
// written through pieces into a cell of the size it writes to
#include "../builds/builds.h"
#include "../code/schema.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "servers_internal.h"

// A check of one file: its tree, its path, its key, its run, where what is reported goes, and its text's size, the text
// after it
typedef struct
{
    char root[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char key[CLI_PATH_MAX_LEN];
    unsigned long long run;
    CliServersSink sink;
    unsigned long long size;
} CliServersOwnJob;

// A check of the tree: its tree, where what is reported goes, and how many paths follow it, each ended by a 0
typedef struct
{
    char root[CLI_PATH_MAX_LEN];
    CliServersSink sink;
    unsigned int count;
} CliServersOwnTreeJob;

int cli_servers_own_reads(const CliServersPathSizeRootSinkCtx *args)
{
    const char *path = args->path;
    return (EMBED_CALL(cli_builds_path.build_type_get, CliBuildsPathCfg, .path = path) != 0u) ||
           EMBED_CALL(cli_schema.reads, CliSchemaCfg, .path = path);
}

typedef struct
{
    const char *root;
    const char *path;
    const char *text;
    unsigned long long size;
    VerbaLinea *out;
    const char *key;
    int *written;
    VerbaLinea *items;
    VerbaLinea *found;
} ServersOwnCtx;
// the diagnostics orior's own reading of the file at `path` finds, written as an array
EMBED_INLINE void own_check(const ServersOwnCtx *args)
{
    const char *root = args->root;
    const char *path = args->path;
    const char *text = args->text;
    unsigned long long size = args->size;
    VerbaLinea *out = args->out;
    if (EMBED_CALL(cli_builds_path.build_type_get, CliBuildsPathCfg, .path = path) != 0u)
    {
        EMBED_CALL(cli_builds_path.build_check, CliBuildsPathCfg, .root = root, .path = path, .text = text,
                   .size = size, .out = out);
        return;
    }
    EMBED_CALL(cli_schema.check, CliSchemaCfg, .root = root, .path = path, .text = text, .size = size, .out = out);
}

// a name a build tool gives a file put on its own line
static void own_name_append(const CliEachNameSizeCfg *args)
{
    VerbaLinea *const names = args->context;
    mmgr_line_put_n(names, args->name, args->size);
    mmgr_line_ch(names, '\n');
}

// one file checked as its text stands, its findings stored and passed on where its run is still the last
static void own_job(const CliServersJobCfg *args)
{
    unsigned long long size = args->size;
    const CliServersOwnJob *const own = args->job;
    const char *const text = (const char *)args->job + sizeof(*own);
    if (size < sizeof(*own) + own->size)
    {
        return;
    }
    char *const findings = g_cli_guard->temporary_buf_alloc(CLI_SERVERS_ANSWER, g_cli_servers_owner);
    VerbaLinea found = mmgr_line_over(findings, CLI_SERVERS_ANSWER);
    mmgr_line_put(&found, "[[");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &found,
               .text_bytes = (const unsigned char *)own->path,
               .length = cellul.len(&(CatenaFinitaCfg){.src = own->path, .cap = CLI_POOL_BYTES}));
    mmgr_line_ch(&found, ',');
    EMBED_CALL(own_check, ServersOwnCtx, .root = own->root, .path = own->path, .text = text, .size = own->size,
               .out = &found);
    mmgr_line_put(&found, "]]");
    if (mmgr_line_ok(&found) && EMBED_CALL(cli_servers_key_internal.run_last, CliServersKeyCtx, .key = own->key,
                                           .id = CLI_SERVERS_OWN, .run_u64 = own->run))
    {
        EMBED_CALL(cli_servers_size_internal.findings_store, CliServersSizeCtx, .id = CLI_SERVERS_OWN,
                   .found = found.out, .size = found.at, .always = 1, .sink = &own->sink);
    }
    g_cli_guard->persistent_buf_release(findings, g_cli_servers_owner);
}

void cli_servers_check_own(const CliServersPathSizeRootSinkCtx *args)
{
    unsigned long long size = args->size;
    const CliServersSink *sink = args->sink;
    if (!cli_servers_path_size_root_sink_internal.own_reads(args) || (size > CLI_SERVERS_LARGEST))
    {
        return;
    }
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_servers_path_key_internal.key_get, CliServersPathKeyCtx, .path = args->path, .key_out = key);
    // each name a part of the file's text, and a break after it
    const unsigned long long capacity = (size * 2ULL) + 1ULL;
    char *const supplied = g_cli_guard->temporary_buf_alloc(capacity, g_cli_servers_owner);
    VerbaLinea names = mmgr_line_over(supplied, capacity);
    EMBED_CALL(cli_builds_path_text.build_input_names, CliBuildsPathTextCfg, .path = args->path, .text = args->text,
               .size = size, .each = own_name_append, .context = &names);
    const int integer = mmgr_line_ok(&names);
    (void)EMBED_CALL(cli_keyed_key_adjacent.put, CliKeyedKeyAdjacentCfg, .keyed_out = &g_cli_servers_input, .key = key,
                     .adjacent_value = "", .text = integer ? names.out : "", .size_value = integer ? names.at : 0ULL);
    g_cli_guard->persistent_buf_release(supplied, g_cli_servers_owner);
    char *const job = g_cli_guard->persistent_buf_alloc(sizeof(CliServersOwnJob) + size, g_cli_servers_owner);
    if (job == NULL)
    {
        return;
    }
    CliServersOwnJob *const own = (CliServersOwnJob *)(void *)job;
    memor.set(&(MemoriaCfg){.dst = own, .val = 0, .bytes = sizeof(*own)});
    VerbaLinea line = {own->root, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->root);
    (void)mmgr_line_finish(&line);
    line = (VerbaLinea){own->path, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->path);
    (void)mmgr_line_finish(&line);
    memor.cpy(&(MemoriaCfg){.dst = own->key, .src = key, .bytes = sizeof(key)});
    own->run = EMBED_CALL(cli_servers_key_internal.run_count, CliServersKeyCtx, .key = key, .id = CLI_SERVERS_OWN);
    own->sink = *sink;
    own->size = size;
    // a size under CLI_SERVERS_LARGEST fits a size_t
    memor.cpy(&(MemoriaCfg){.dst = job + sizeof(*own), .src = args->text, .bytes = (size_t)size});
    (void)EMBED_CALL(cli_servers_run_size_internal.later_dispatch, CliServersRunSizeCtx, .run = own_job, .bytes = job,
                     .size = sizeof(*own) + size);
}

// whether the file of `key` had findings of orior's own before; under the checks' lock
EMBED_INLINE int own_had(const ServersOwnCtx *args)
{
    const char *key = args->key;
    const char *adjacent = NULL;
    unsigned long long size = 0ULL;
    const char *const stored =
        EMBED_CALL(cli_keyed_key_adjacent.get, CliKeyedKeyAdjacentCfg, .keyed = &g_cli_servers_checked, .key = key,
                   .adjacent = &adjacent, .size = &size);
    unsigned long long at = 0ULL;
    unsigned long long length = 0ULL;
    if ((stored == NULL) || !EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = stored, .size_value = size,
                                        .name = CLI_SERVERS_OWN, .start = &at, .length_pointer = &length))
    {
        return 0;
    }
    const unsigned long long lead =
        EMBED_CALL(cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg, .text = stored + at, .size = length);
    if ((lead == length) || (stored[at + lead] != '['))
    {
        return 0;
    }
    const unsigned long long inner = lead + 1ULL +
                                     EMBED_CALL(cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg,
                                                .text = stored + at + lead + 1ULL, .size = length - lead - 1ULL);
    return (inner < length) && (stored[at + inner] != ']');
}

// the findings of the file at `path` of the tree `root`, read where it lies on the disk and written on `items`, the
// one line every file of the tree's job uses, put after those `found` contains, `*written` stating whether any are:
// none where it is not read whole, or where it has none and had none before
EMBED_INLINE void own_tree_file(const ServersOwnCtx *args)
{
    const char *path = args->path;
    int *written = args->written;
    unsigned long long size = 0ULL;
    const unsigned char *const mapped = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                   .size_pointer = &size, .owner = g_cli_servers_owner);
    if ((mapped == NULL) ||
        !EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg, .bytes = (const char *)mapped, .size = size))
    {
        g_cli_guard->persistent_buf_release((void *)(uintptr_t)mapped, (g_cli_servers_owner));
        return;
    }
    args->items->at = 0ULL;
    EMBED_CALL(own_check, ServersOwnCtx, .root = args->root, .path = path, .text = (const char *)mapped, .size = size,
               .out = args->items);
    g_cli_guard->persistent_buf_release((void *)(uintptr_t)mapped, (g_cli_servers_owner));
    const int integer = mmgr_line_ok(args->items);
    const int empty = integer && (args->items->at == 2ULL) &&
                      (memor.cmp(&(MemoriaCfg){.src = args->items->out, .other = "[]", .bytes = 2u}) == 0);
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_servers_path_key_internal.key_get, CliServersPathKeyCtx, .path = path, .key_out = key);
    const int had = EMBED_CALL(own_had, ServersOwnCtx, .key = key);
    if (integer && (!empty || had))
    {
        mmgr_line_put(args->found, *written ? ",[" : "[");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->found,
                   .text_bytes = (const unsigned char *)path,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES}));
        mmgr_line_ch(args->found, ',');
        (void)mmgr_line_put_n(args->found, args->items->out, args->items->at);
        mmgr_line_ch(args->found, ']');
        *written = 1;
    }
}

// each file of the tree orior_cli reads itself checked as the disk stores it, its findings stored and passed on
static void own_tree_job(const CliServersJobCfg *args)
{
    unsigned long long size = args->size;
    const CliServersOwnTreeJob *const tree = args->job;
    char *const findings = g_cli_guard->temporary_buf_alloc(CLI_SERVERS_FINDINGS, g_cli_servers_owner);
    VerbaLinea found = mmgr_line_over(findings, CLI_SERVERS_FINDINGS);
    char *const item_text = g_cli_guard->temporary_buf_alloc(CLI_SERVERS_ANSWER, g_cli_servers_owner);
    VerbaLinea items = mmgr_line_over(item_text, CLI_SERVERS_ANSWER);
    mmgr_line_ch(&found, '[');
    int written = 0;
    unsigned long long at = sizeof(*tree);
    for (unsigned int file = 0u; (file < tree->count) && (at < size); file += 1u)
    {
        const char *const path = (const char *)args->job + at;
        at += cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES}) + 1u;
        EMBED_CALL(own_tree_file, ServersOwnCtx, .root = tree->root, .path = path, .written = &written, .items = &items,
                   .found = &found);
    }
    g_cli_guard->persistent_buf_release(item_text, g_cli_servers_owner);
    mmgr_line_ch(&found, ']');
    if (mmgr_line_ok(&found))
    {
        EMBED_CALL(cli_servers_size_internal.findings_store, CliServersSizeCtx, .id = CLI_SERVERS_OWN,
                   .found = found.out, .size = found.at, .always = 0, .sink = &tree->sink);
    }
    g_cli_guard->persistent_buf_release(findings, g_cli_servers_owner);
}

void cli_servers_check_tree_own(const CliServersFilesCtx *args)
{
    unsigned int count = args->count;
    const CliServersSink *sink = args->sink;
    // the paths counted, then the job made in a cell of their size
    unsigned long long used = sizeof(CliServersOwnTreeJob);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char path[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = args->files[at], .joined = path);
        used += EMBED_CALL(cli_servers_path_size_root_sink_internal.own_reads, CliServersPathSizeRootSinkCtx,
                           .path = args->files[at])
                    ? (cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES}) + 1u)
                    : 0u;
    }
    if (used == sizeof(CliServersOwnTreeJob))
    {
        return;
    }
    char *const job = g_cli_guard->persistent_buf_alloc(used, g_cli_servers_owner);
    if (job == NULL)
    {
        return;
    }
    CliServersOwnTreeJob *const tree = (CliServersOwnTreeJob *)(void *)job;
    memor.set(&(MemoriaCfg){.dst = tree, .val = 0, .bytes = sizeof(*tree)});
    VerbaLinea line = {tree->root, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->root);
    (void)mmgr_line_finish(&line);
    tree->sink = *sink;
    unsigned long long at_byte = sizeof(*tree);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (!EMBED_CALL(cli_servers_path_size_root_sink_internal.own_reads, CliServersPathSizeRootSinkCtx,
                        .path = args->files[at]))
        {
            continue;
        }
        char path[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = args->files[at], .joined = path);
        const size_t size = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES}) + 1u;
        memor.cpy(&(MemoriaCfg){.dst = job + at_byte, .src = path, .bytes = size});
        at_byte += size;
        tree->count += 1u;
    }
    (void)EMBED_CALL(cli_servers_run_size_internal.later_dispatch, CliServersRunSizeCtx, .run = own_tree_job,
                     .bytes = job, .size = used);
}

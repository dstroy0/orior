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
#include "../command_line/cells.h"
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

int cli_servers_own_reads(const char *path)
{
    return (cli_build_kind_of(path) != 0u) || cli_schema_reads(path);
}

// the diagnostics orior's own reading of the file at `path` finds, written as an array
static void own_check(const char *root, const char *path, const char *text, unsigned long long size, VerbaLinea *out)
{
    if (cli_build_kind_of(path) != 0u)
    {
        cli_build_check(root, path, text, size, out);
        return;
    }
    cli_schema_check(root, path, text, size, out);
}

// a name a build tool gives a file put on its own line
static void own_name_put(void *context, const char *name, unsigned long long size)
{
    VerbaLinea *const names = context;
    mmgr_line_put_n(names, name, size);
    mmgr_line_ch(names, '\n');
}

// a piece of what is written put on the line that is `context`
static void own_put(void *context, const char *bytes, unsigned long long size)
{
    mmgr_line_put_n((VerbaLinea *)context, bytes, size);
}

// one file checked as its text stands, its findings stored and passed on where its run is still the last
static void own_job(const void *job, unsigned long long size)
{
    const CliServersOwnJob *const own = job;
    const char *const text = (const char *)job + sizeof(*own);
    if (size < sizeof(*own) + own->size)
    {
        return;
    }
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    VerbaLinea found;
    cli_pieces_begun(&pieces, &found, piece, sizeof(piece));
    mmgr_line_put(&found, "[[");
    cli_json_quoted_lossy(&found, (const unsigned char *)own->path,
                          cellul.len(&(CatenaFinitaCfg){.src = own->path, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(&found, ',');
    own_check(own->root, own->path, text, own->size, &found);
    mmgr_line_put(&found, "]]");
    unsigned long long found_size = 0ull;
    char *const findings = cli_pieces_joined(&pieces, &found, "what orior's own check of a file finds", &found_size);
    if ((findings != NULL) && cli_servers_run_last(own->key, CLI_SERVERS_OWN, own->run))
    {
        cli_servers_findings_stored(CLI_SERVERS_OWN, findings, found_size, 1, &own->sink);
    }
    g_cli_cells->persistent_buf_release(findings, NULL);
}

void cli_servers_check_own(const char *root, const char *path, const char *text, unsigned long long size,
                           const CliServersSink *sink)
{
    if (!cli_servers_own_reads(path) || (size > CLI_SERVERS_LARGEST))
    {
        return;
    }
    char key[CLI_PATH_MAX_LEN];
    cli_servers_key_of(path, key);
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    VerbaLinea names;
    cli_pieces_begun(&pieces, &names, piece, sizeof(piece));
    cli_build_given_names(path, text, size, own_name_put, &names);
    unsigned long long names_size = 0ull;
    char *const given = cli_pieces_joined(&pieces, &names, "the names a build tool gives", &names_size);
    cli_lock_take(&g_cli_servers_checks_lock);
    (void)cli_keyed_put(&g_cli_servers_given, key, "", (given != NULL) ? given : "",
                        (given != NULL) ? names_size : 0ull);
    cli_lock_give(&g_cli_servers_checks_lock);
    g_cli_cells->persistent_buf_release(given, NULL);
    char *const job = g_cli_cells->persistent_buf_alloc(sizeof(CliServersOwnJob) + size, NULL);
    if (job == NULL)
    {
        return;
    }
    CliServersOwnJob *const own = (CliServersOwnJob *)(void *)job;
    memor.set(&(MemoriaCfg){.dst = own, .val = 0, .bytes = sizeof(*own)});
    VerbaLinea line = {own->root, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, root);
    (void)mmgr_line_finish(&line);
    line = (VerbaLinea){own->path, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, path);
    (void)mmgr_line_finish(&line);
    memor.cpy(&(MemoriaCfg){.dst = own->key, .src = key, .bytes = sizeof(key)});
    own->run = cli_servers_run_counted(key, CLI_SERVERS_OWN);
    own->sink = *sink;
    own->size = size;
    // a size under CLI_SERVERS_LARGEST fits a size_t
    memor.cpy(&(MemoriaCfg){.dst = job + sizeof(*own), .src = text, .bytes = (size_t)size});
    (void)cli_servers_later_handed(own_job, job, sizeof(*own) + size);
}

// whether the file of `key` had findings of orior's own before; under the checks' lock
static int own_had(const char *key)
{
    const char *beside = NULL;
    unsigned long long size = 0ull;
    const char *const stored = cli_keyed_get(&g_cli_servers_checked, key, &beside, &size);
    unsigned long long at = 0ull;
    unsigned long long length = 0ull;
    if ((stored == NULL) || !cli_lsp_member(stored, size, CLI_SERVERS_OWN, &at, &length))
    {
        return 0;
    }
    const unsigned long long lead = cli_text_lead(stored + at, length);
    if ((lead == length) || (stored[at + lead] != '['))
    {
        return 0;
    }
    const unsigned long long inner = lead + 1ull + cli_text_lead(stored + at + lead + 1ull, length - lead - 1ull);
    return (inner < length) && (stored[at + inner] != ']');
}

// the findings of the file at `path` of the tree `root`, read where it lies on the disk, put after those `found`
// contains, `*written` stating whether any are: none where it is not read whole, or where it has none and had none
// before
static void own_tree_file(const char *root, const char *path, int *written, VerbaLinea *found)
{
    unsigned long long size = 0ull;
    const unsigned char *const mapped = cli_file_read(path, &size);
    if ((mapped == NULL) || !cli_utf8_whole((const char *)mapped, size))
    {
        if (mapped != NULL)
        {
            cli_file_let_go(mapped, size);
        }
        return;
    }
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    VerbaLinea items;
    cli_pieces_begun(&pieces, &items, piece, sizeof(piece));
    own_check(root, path, (const char *)mapped, size, &items);
    cli_file_let_go(mapped, size);
    const int whole = cli_pieces_ended(&pieces, &items);
    const int empty = (pieces.size == 2ull) && (pieces.first != NULL) &&
                      (memor.cmp(&(MemoriaCfg){.src = pieces.first->bytes, .other = "[]", .bytes = 2u}) == 0);
    char key[CLI_PATH_MAX_LEN];
    cli_servers_key_of(path, key);
    cli_lock_take(&g_cli_servers_checks_lock);
    const int had = own_had(key);
    cli_lock_give(&g_cli_servers_checks_lock);
    if (whole && (!empty || had))
    {
        mmgr_line_put(found, *written ? ",[" : "[");
        cli_json_quoted_lossy(found, (const unsigned char *)path,
                              cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
        mmgr_line_ch(found, ',');
        cli_pieces_each(&pieces, own_put, found);
        mmgr_line_ch(found, ']');
        *written = 1;
    }
    cli_pieces_given(&pieces);
}

// each file of the tree orior_cli reads itself checked as the disk stores it, its findings stored and passed on
static void own_tree_job(const void *job, unsigned long long size)
{
    const CliServersOwnTreeJob *const tree = job;
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    VerbaLinea found;
    cli_pieces_begun(&pieces, &found, piece, sizeof(piece));
    mmgr_line_ch(&found, '[');
    int written = 0;
    unsigned long long at = sizeof(*tree);
    for (unsigned int file = 0u; (file < tree->count) && (at < size); file += 1u)
    {
        const char *const path = (const char *)job + at;
        at += cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u;
        own_tree_file(tree->root, path, &written, &found);
    }
    mmgr_line_ch(&found, ']');
    unsigned long long found_size = 0ull;
    char *const findings = cli_pieces_joined(&pieces, &found, "what orior's own check of the tree finds", &found_size);
    if (findings != NULL)
    {
        cli_servers_findings_stored(CLI_SERVERS_OWN, findings, found_size, 0, &tree->sink);
    }
    g_cli_cells->persistent_buf_release(findings, NULL);
}

void cli_servers_check_tree_own(const char *root, const char *const *files, unsigned int count,
                                const CliServersSink *sink)
{
    // the paths counted, then the job made in a cell of their size
    unsigned long long used = sizeof(CliServersOwnTreeJob);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char path[CLI_PATH_MAX_LEN];
        cli_full(root, files[at], path);
        used += cli_servers_own_reads(files[at])
                    ? (cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u)
                    : 0u;
    }
    if (used == sizeof(CliServersOwnTreeJob))
    {
        return;
    }
    char *const job = g_cli_cells->persistent_buf_alloc(used, NULL);
    if (job == NULL)
    {
        return;
    }
    CliServersOwnTreeJob *const tree = (CliServersOwnTreeJob *)(void *)job;
    memor.set(&(MemoriaCfg){.dst = tree, .val = 0, .bytes = sizeof(*tree)});
    VerbaLinea line = {tree->root, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, root);
    (void)mmgr_line_finish(&line);
    tree->sink = *sink;
    unsigned long long at_byte = sizeof(*tree);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (!cli_servers_own_reads(files[at]))
        {
            continue;
        }
        char path[CLI_PATH_MAX_LEN];
        cli_full(root, files[at], path);
        const size_t size = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u;
        memor.cpy(&(MemoriaCfg){.dst = job + at_byte, .src = path, .bytes = size});
        at_byte += size;
        tree->count += 1u;
    }
    (void)cli_servers_later_handed(own_tree_job, job, used);
}

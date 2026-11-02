// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_inspect.c: the inspections' tree: a file's text read into it, or the file taken out where it
// has none, and each file's findings that changed passed on. Where the tree contains another root or none, the root's
// Python and JavaScript are read first, on a thread of the servers', and a read for a tree let go meanwhile is set
// aside. What each Python class's family sets is stored by file, and where it moves the files whose server reports an
// attribute are passed on again, for a family that sets it to decide whether the report passes; a file the editor
// opens receives its diagnostics, changed or the same. The findings and the files whose findings changed are written
// through pieces into cells of their size
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../editor/files.h"
#include "../inspect/inspect_tree.h"
#include "servers_internal.h"

#include "../text/text.h"

// The findings of the tree as they are written: the line they go on, each file's path ended by a 0 then its findings
// as an array ended by a 0, whether a file is begun and whether it has a finding yet, and how many files are begun
typedef struct
{
    VerbaLinea *found;
    int file_begun;
    int has_finding;
    unsigned long long files;
} CliServersInspectFound;
// the most files with names supplied and findings seen
#define CLI_SERVERS_INSPECT_FILES (1u << 16u)

// A read of a file into the tree: its root, its path in the root, where what is reported goes, the trees let go before
// it, and whether its text follows, with its size
typedef struct
{
    char root[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    CliServersSink sink;
    unsigned long long born;
    int has_text;
    unsigned long long size;
} CliServersInspectJob;

// how many trees were let go, under its own lock; and the root the tree was read from and when, under the tree's
// lock, which a read acquires
static unsigned long long s_generation;
static CliLock s_generation_lock = CLI_LOCK_INIT;
static char s_tree_root[CLI_PATH_MAX_LEN];
static unsigned long long s_tree_generation;
static CliLock s_tree_lock = CLI_LOCK_INIT;
// what each family sets and the files seen, under the tree's lock
static CliKeyed s_supplied_new = {"names newly supplied", {NULL}, 0u, CLI_SERVERS_INSPECT_FILES};
static CliKeyed s_seen = {"findings seen", {NULL}, 0u, CLI_SERVERS_INSPECT_FILES};

// how many trees were let go
static unsigned long long inspect_generation(void)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_generation_lock);
    const unsigned long long generation = s_generation;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_generation_lock);
    return generation;
}

typedef struct
{
    const char *root;
    CliServersInspectFound *found;
    VerbaLinea *changed;
    const char *key;
    const char *uri;
    const char *found_text;
    unsigned long long size;
    unsigned long long found_files;
    const CliServersSink *sink;
    unsigned long long born;
} ServersInspectCtx;
// whether the tree was read from `root` since the last tree was let go; under the tree's lock
EMBED_INLINE int inspect_has_tree(const ServersInspectCtx *args)
{
    return (s_tree_root[0] != '\0') && (s_tree_generation == inspect_generation()) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = s_tree_root, .other = args->root, .cap = CLI_CELLS_POOL}));
}

int cli_servers_tree_read(const CliServersRootKindCtx *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_tree_lock);
    const int read = (s_tree_root[0] != '\0') && (s_tree_generation == inspect_generation());
    memor.cpy(&(MemoriaCfg){.dst = args->root_out, .src = s_tree_root, .bytes = CLI_PATH_MAX_LEN});
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_tree_lock);
    return read;
}

// the array of the file begun on the findings' line closed and ended by a 0
EMBED_INLINE void inspect_file_close(const ServersInspectCtx *args)
{
    if (args->found->file_begun)
    {
        mmgr_line_ch(args->found->found, ']');
        mmgr_line_ch(args->found->found, '\0');
    }
}

// a finding of a file of the tree put after its file's, a file's path with none first
static void inspect_find(const CliInspectFoundCfg *args)
{
    CliServersInspectFound *const found = args->context;
    if (args->finding == NULL)
    {
        EMBED_CALL(inspect_file_close, ServersInspectCtx, .found = found);
        mmgr_line_put_n(found->found, args->path,
                        cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}) + 1ull);
        mmgr_line_ch(found->found, '[');
        found->file_begun = 1;
        found->has_finding = 0;
        found->files += 1ull;
        return;
    }
    if (!found->file_begun)
    {
        return;
    }
    mmgr_line_put(found->found, found->has_finding ? "," : "");
    mmgr_line_put_n(found->found, args->finding, args->size);
    found->has_finding = 1;
}

// an attribute a class's family sets stored, by its file's key, as `class attribute` on a line after those stored
static void inspect_supplied(const CliInspectSuppliedCfg *args)
{
    const char *path = args->path;
    const char *const root = args->context;
    char full[CLI_PATH_MAX_LEN];
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = path, .joined = full);
    EMBED_CALL(cli_servers_path_key_internal.key_get, CliServersPathKeyCtx, .path = full, .key_out = key);
    const char *beside = NULL;
    unsigned long long size = 0ull;
    const char *const stored = EMBED_CALL(cli_keyed_key_beside.get, CliKeyedKeyBesideCfg, .keyed = &s_supplied_new,
                                          .key = key, .beside = &beside, .size = &size);
    const unsigned long long class_size =
        cellul.len(&(CatenaFinitaCfg){.src = args->class_name, .cap = CLI_CELLS_POOL});
    const unsigned long long attribute_size =
        cellul.len(&(CatenaFinitaCfg){.src = args->attribute, .cap = CLI_CELLS_POOL});
    const unsigned long long line_size = size + class_size + 1ull + attribute_size + 1ull;
    char *const line = g_cli_cells->temporary_buf_alloc(line_size, &g_cli_servers_cells);
    if (line == NULL)
    {
        return;
    }
    // each size is a path's, a name's or a line of them, far inside what a size_t counts
    memor.cpy(&(MemoriaCfg){.dst = line, .src = (stored != NULL) ? stored : "", .bytes = (size_t)size});
    memor.cpy(&(MemoriaCfg){.dst = line + size, .src = args->class_name, .bytes = (size_t)class_size});
    line[size + class_size] = ' ';
    memor.cpy(
        &(MemoriaCfg){.dst = line + size + class_size + 1ull, .src = args->attribute, .bytes = (size_t)attribute_size});
    line[line_size - 1ull] = '\n';
    (void)EMBED_CALL(cli_keyed_key_beside.put, CliKeyedKeyBesideCfg, .keyed_out = &s_supplied_new, .key = key,
                     .beside_value = "", .text = line, .size_value = line_size);
    g_cli_cells->persistent_buf_release(line, &g_cli_servers_cells);
}

// what each family sets put in place of what was stored: 1 where it moved; under the checks' lock
static int inspect_supplied_put(void)
{
    unsigned int count = 0u;
    unsigned int made = 0u;
    int moved = 0;
    for (unsigned int at = 0u; at < g_cli_servers_supplied.entries; at += 1u)
    {
        count +=
            EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = &g_cli_servers_supplied, .at = at)->used ? 1u : 0u;
    }
    for (unsigned int at = 0u; !moved && (at < s_supplied_new.entries); at += 1u)
    {
        const CliKeyedEntry *const one =
            EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = &s_supplied_new, .at = at);
        if (!one->used)
        {
            continue;
        }
        made += 1u;
        const char *beside = NULL;
        unsigned long long size = 0ull;
        unsigned long long other = 0ull;
        const char *const key = one->cell;
        const char *const text = EMBED_CALL(cli_keyed_key_beside.get, CliKeyedKeyBesideCfg, .keyed = &s_supplied_new,
                                            .key = key, .beside = &beside, .size = &size);
        const char *const was =
            EMBED_CALL(cli_keyed_key_beside.get, CliKeyedKeyBesideCfg, .keyed = &g_cli_servers_supplied, .key = key,
                       .beside = &beside, .size = &other);
        moved = (was == NULL) || (other != size) ||
                (memor.cmp(&(MemoriaCfg){.src = was, .other = text, .bytes = (size_t)size}) != 0);
    }
    if (!moved && (count == made))
    {
        return 0;
    }
    EMBED_CALL(cli_keyed_name.clear, CliKeyedNameCfg, .keyed_out = &g_cli_servers_supplied);
    for (unsigned int at = 0u; at < s_supplied_new.entries; at += 1u)
    {
        const CliKeyedEntry *const one =
            EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = &s_supplied_new, .at = at);
        if (!one->used)
        {
            continue;
        }
        const char *beside = NULL;
        unsigned long long size = 0ull;
        const char *const text = EMBED_CALL(cli_keyed_key_beside.get, CliKeyedKeyBesideCfg, .keyed = &s_supplied_new,
                                            .key = one->cell, .beside = &beside, .size = &size);
        if (text != NULL)
        {
            (void)EMBED_CALL(cli_keyed_key_beside.put, CliKeyedKeyBesideCfg, .keyed_out = &g_cli_servers_supplied,
                             .key = one->cell, .beside_value = "", .text = text, .size_value = size);
        }
    }
    return 1;
}

// a file whose findings changed put on the line that is `changed`, its key then its URI, each ended by a 0
EMBED_INLINE void inspect_change(const ServersInspectCtx *args)
{
    mmgr_line_put_n(args->changed, args->key,
                    cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_CELLS_POOL}) + 1ull);
    mmgr_line_put_n(args->changed, args->uri,
                    cellul.len(&(CatenaFinitaCfg){.src = args->uri, .cap = CLI_CELLS_POOL}) + 1ull);
}

// each file's findings of `size` bytes of `found`, `found_files` files, stored, by its key with its URI, and each file
// whose findings changed passed on
EMBED_INLINE void inspect_passed_on(const ServersInspectCtx *args)
{
    const char *root = args->root;
    unsigned long long size = args->size;
    const CliServersSink *sink = args->sink;
    EMBED_CALL(cli_keyed_name.clear, CliKeyedNameCfg, .keyed_out = &s_seen);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_servers_checks_lock);
    // a file found changes by its key and its URI, each at most a path's; one stored and no longer found by what it
    // stored
    const unsigned long long room =
        (args->found_files * CLI_PATH_MAX_LEN * 2ull) +
        EMBED_CALL(cli_keyed_table.listed_size, CliKeyedCfg, .keyed = &g_cli_servers_inspected) + 1ull;
    char *const files = g_cli_cells->temporary_buf_alloc(room, &g_cli_servers_cells);
    VerbaLinea changed = mmgr_line_over(files, room);
    for (unsigned long long at = 0ull; at < size;)
    {
        const char *const path = args->found_text + at;
        const char *const values = path + cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u;
        const unsigned long long values_size = cellul.len(&(CatenaFinitaCfg){.src = values, .cap = CLI_CELLS_POOL});
        at += cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u + values_size + 1u;
        char full[CLI_PATH_MAX_LEN];
        char uri[CLI_PATH_MAX_LEN];
        char key[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = path, .joined = full);
        EMBED_CALL(cli_servers_path_uri_internal.uri_get, CliServersPathUriCtx, .path = full, .uri_out = uri);
        EMBED_CALL(cli_lsp_length.key_get, CliLspLengthCfg, .uri = uri,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), .key = key);
        (void)EMBED_CALL(cli_keyed_key_beside.put, CliKeyedKeyBesideCfg, .keyed_out = &s_seen, .key = key,
                         .beside_value = "", .text = "", .size_value = 0ull);
        const char *beside = NULL;
        unsigned long long stored_size = 0ull;
        const char *const stored =
            EMBED_CALL(cli_keyed_key_beside.get, CliKeyedKeyBesideCfg, .keyed = &g_cli_servers_inspected, .key = key,
                       .beside = &beside, .size = &stored_size);
        const int same = (stored != NULL) && (stored_size == values_size) &&
                         (memor.cmp(&(MemoriaCfg){.src = stored, .other = values, .bytes = (size_t)values_size}) == 0);
        if (((stored != NULL) || (values_size > 2u)) && !same)
        {
            (void)EMBED_CALL(cli_keyed_key_beside.put, CliKeyedKeyBesideCfg, .keyed_out = &g_cli_servers_inspected,
                             .key = key, .beside_value = uri, .text = values, .size_value = values_size);
            EMBED_CALL(inspect_change, ServersInspectCtx, .changed = &changed, .key = key, .uri = uri);
        }
    }
    for (unsigned int at = 0u; at < g_cli_servers_inspected.entries; at += 1u)
    {
        CliKeyedEntry *const one =
            EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = &g_cli_servers_inspected, .at = at);
        const char *const key = one->cell;
        const char *beside = NULL;
        unsigned long long stored_size = 0ull;
        if (one->used && (EMBED_CALL(cli_keyed_key_beside.get, CliKeyedKeyBesideCfg, .keyed = &s_seen, .key = key,
                                     .beside = &beside, .size = &stored_size) == NULL))
        {
            (void)EMBED_CALL(cli_keyed_key_beside.get, CliKeyedKeyBesideCfg, .keyed = &g_cli_servers_inspected,
                             .key = key, .beside = &beside, .size = &stored_size);
            EMBED_CALL(inspect_change, ServersInspectCtx, .changed = &changed, .key = key, .uri = beside);
            one->used = 0;
        }
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_servers_checks_lock);
    const unsigned long long changed_size = mmgr_line_ok(&changed) ? changed.at : 0ull;
    for (unsigned long long at = 0ull; at < changed_size;)
    {
        const char *const key = files + at;
        const char *const uri = key + cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u;
        EMBED_CALL(cli_servers_sink_key_internal.emit_whole, CliServersSinkKeyCtx, .key = key, .uri = uri,
                   .sink = sink);
        at += cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u +
              cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}) + 1u;
    }
    g_cli_cells->persistent_buf_release(files, &g_cli_servers_cells);
}

// the tree read from `root`, each Python and JavaScript file of it read where it lies, the tree's files laid flat for
// the read: 1, or 0 where a tree was let go meanwhile or the list could not be taken
EMBED_INLINE int inspect_tree_read(const ServersInspectCtx *args)
{
    const char *root = args->root;
    const CliFiles *const files = EMBED_CALL(cli_files_root_paths.view, CliFilesRootPathsCfg, .root = root);
    if (files == NULL)
    {
        return 0;
    }
    cli_inspect_tree_clear();
    for (unsigned int at = 0u; (at < files->count) && (inspect_generation() == args->born); at += 1u)
    {
        const char *const one = EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = at);
        const char *const language = EMBED_CALL(cli_inspect_table.language_get, CliInspectCfg, .path = one);
        if (language == NULL)
        {
            continue;
        }
        char path[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = one, .joined = path);
        unsigned long long size = 0ull;
        const unsigned char *const mapped = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = path,
                                                       .size_pointer = &size, .owner = &g_cli_servers_cells);
        if ((mapped != NULL) &&
            EMBED_CALL(cli_json_tree_size.utf8_whole, CliJsonTreeSizeCfg, .bytes = (const char *)mapped, .size = size))
        {
            (void)EMBED_CALL(cli_inspect_tree_path_language.set, CliInspectTreePathLanguageCfg, .path = one,
                             .language = language, .text = (const char *)mapped, .size = size);
        }
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = mapped,
                   .owner = &g_cli_servers_cells);
    }
    EMBED_CALL(cli_files.let, CliFilesCfg, .files = files);
    if (inspect_generation() != args->born)
    {
        cli_inspect_tree_clear();
        s_tree_root[0] = '\0';
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = s_tree_root,
                            .src = root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    s_tree_generation = args->born;
    return 1;
}

// a file read into the tree, the root's read first where the tree contains another, and the findings passed on
static void inspect_job(const CliServersJobCfg *args)
{
    unsigned long long size = args->size;
    const CliServersInspectJob *const read = args->job;
    if (size < sizeof(*read) + read->size)
    {
        return;
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_tree_lock);
    if (!EMBED_CALL(inspect_has_tree, ServersInspectCtx, .root = read->root) &&
        !EMBED_CALL(inspect_tree_read, ServersInspectCtx, .root = read->root, .born = read->born))
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_tree_lock);
        return;
    }
    const char *const language = EMBED_CALL(cli_inspect_table.language_get, CliInspectCfg, .path = read->file);
    if (read->has_text && (language != NULL))
    {
        (void)EMBED_CALL(cli_inspect_tree_path_language.set, CliInspectTreePathLanguageCfg, .path = read->file,
                         .language = language, .text = (const char *)args->job + sizeof(*read), .size = read->size);
    }
    else
    {
        EMBED_CALL(cli_inspect_tree_path_line.remove, CliInspectTreePathLineCfg, .path = read->file);
    }
    char *const findings = g_cli_cells->temporary_buf_alloc(CLI_SERVERS_FINDINGS, &g_cli_servers_cells);
    VerbaLinea line = mmgr_line_over(findings, CLI_SERVERS_FINDINGS);
    CliServersInspectFound found = {&line, 0, 0, 0ull};
    EMBED_CALL(cli_inspect_tree.findings, CliInspectTreeCfg, .each_inspect_found = inspect_find, .context = &found);
    EMBED_CALL(inspect_file_close, ServersInspectCtx, .found = &found);
    EMBED_CALL(cli_keyed_name.clear, CliKeyedNameCfg, .keyed_out = &s_supplied_new);
    EMBED_CALL(cli_inspect_tree_context_each_supplied.supplied, CliInspectTreeContextEachSuppliedCfg,
               .each_inspect_supplied = inspect_supplied, .context = (void *)read->root);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_servers_checks_lock);
    const int moved = inspect_supplied_put();
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_servers_checks_lock);
    if (mmgr_line_ok(&line))
    {
        EMBED_CALL(inspect_passed_on, ServersInspectCtx, .root = read->root, .found_text = line.out, .size = line.at,
                   .found_files = found.files, .sink = &read->sink);
    }
    g_cli_cells->persistent_buf_release(findings, &g_cli_servers_cells);
    // what a family sets decides whether a server's reports of an attribute pass on
    if (moved)
    {
        EMBED_CALL(cli_servers_sink_key_internal.emit_reporting_attributes, CliServersSinkKeyCtx, .sink = &read->sink);
    }
    // a file the editor opens receives its diagnostics, changed or the same
    if (read->has_text)
    {
        char full[CLI_PATH_MAX_LEN];
        char uri[CLI_PATH_MAX_LEN];
        char key[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = read->root, .relative = read->file, .joined = full);
        EMBED_CALL(cli_servers_path_uri_internal.uri_get, CliServersPathUriCtx, .path = full, .uri_out = uri);
        EMBED_CALL(cli_lsp_length.key_get, CliLspLengthCfg, .uri = uri,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), .key = key);
        EMBED_CALL(cli_servers_sink_key_internal.emit_whole, CliServersSinkKeyCtx, .key = key, .uri = uri,
                   .sink = &read->sink);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_tree_lock);
}

void cli_servers_inspect(const CliServersPathSizeRootSinkCtx *args)
{
    const char *path = args->path;
    unsigned long long size = args->size;
    const CliServersSink *sink = args->sink;
    const size_t top = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_CELLS_POOL});
    if (!EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = path, .other_pointer = args->root, .size = top) ||
        ((path[top] != '/') && (path[top] != '\\')) ||
        (EMBED_CALL(cli_inspect_table.language_get, CliInspectCfg, .path = path) == NULL))
    {
        return;
    }
    // the read made in a cell of its size, the text after it where it is given
    const int has_text = (args->text != NULL) && (size <= CLI_SERVERS_LARGEST);
    const unsigned long long text_size = has_text ? size : 0ull;
    char *const job = g_cli_cells->temporary_buf_alloc(sizeof(CliServersInspectJob) + text_size, &g_cli_servers_cells);
    if (job == NULL)
    {
        return;
    }
    CliServersInspectJob *const read = (CliServersInspectJob *)(void *)job;
    memor.set(&(MemoriaCfg){.dst = read, .val = 0, .bytes = sizeof(*read)});
    memor.cpy(&(MemoriaCfg){.dst = read->root, .src = args->root, .bytes = top + 1u});
    VerbaLinea file = {read->file, CLI_PATH_MAX_LEN, 0ull};
    (void)EMBED_CALL(cli_text_from.put_swap, CliTextFromCfg, .out = &file, .text = path + top + 1u, .from = '\\',
                     .to = '/');
    (void)mmgr_line_finish(&file);
    read->sink = *sink;
    read->has_text = has_text;
    read->size = text_size;
    if (has_text)
    {
        // a size under CLI_SERVERS_LARGEST fits a size_t
        memor.cpy(&(MemoriaCfg){.dst = job + sizeof(*read), .src = args->text, .bytes = (size_t)text_size});
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_servers.lock);
    g_cli_servers.sink = *sink;
    g_cli_servers.has_sink = 1;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_servers.lock);
    read->born = inspect_generation();
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_tree_lock);
    const int ready = EMBED_CALL(inspect_has_tree, ServersInspectCtx, .root = args->root);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_tree_lock);
    if (ready)
    {
        EMBED_CALL(inspect_job, CliServersJobCfg, .job = job, .size = sizeof(*read) + text_size);
        g_cli_cells->persistent_buf_release(job, &g_cli_servers_cells);
        return;
    }
    (void)EMBED_CALL(cli_servers_run_size_internal.later_dispatch, CliServersRunSizeCtx, .run = inspect_job,
                     .bytes = job, .size = sizeof(*read) + text_size);
}

void cli_servers_inspect_again(const CliServersPathSizeLanguageCtx *args)
{
    char root[CLI_PATH_MAX_LEN];
    static CliServersSink s_sink;
    static CliLock s_sink_lock = CLI_LOCK_INIT;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_sink_lock);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_servers.lock);
    const int has_sink = g_cli_servers.has_sink;
    s_sink = g_cli_servers.sink;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_servers.lock);
    if (has_sink && EMBED_CALL(cli_servers_root_kind_internal.tree_read, CliServersRootKindCtx, .root_out = root))
    {
        EMBED_CALL(cli_servers_path_size_root_sink_internal.inspect, CliServersPathSizeRootSinkCtx, .root = root,
                   .path = args->path, .text = args->text, .size = args->size, .sink = &s_sink);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_sink_lock);
}

void cli_servers_inspect_let_go(void)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_generation_lock);
    s_generation += 1ull;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_generation_lock);
}

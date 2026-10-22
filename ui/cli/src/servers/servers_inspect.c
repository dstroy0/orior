// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_inspect.c: the inspections' index of the tree: a file's text read into it, or the file taken out where it
// has none, and each file's findings that changed passed on. Where the index holds another tree or none, the tree's
// Python and JavaScript are read first, on a thread of the servers', and a read for a tree let go meanwhile is set
// aside. What each Python class's family sets is kept by file, and where it moves the files whose server reports an
// attribute are passed on again, for a family that sets it to decide whether the report passes; a file the editor
// opens hears its diagnostics, changed or the same. The findings and the files whose findings changed are written
// through pieces into cells of their size
#include "../command_line/cells.h"
#include "../editor/files.h"
#include "../inspect/inspect_tree.h"
#include "servers_internal.h"

#include "../text/text.h"

// the most files with names supplied and findings seen
#define CLI_SERVERS_INSPECT_FILES (1u << 16u)

// A read of a file into the index: its tree, its path in the tree, where what is said goes, the trees let go before
// it, and whether its text follows, with its size
typedef struct
{
    char root[CLI_PATH_LONGEST];
    char file[CLI_PATH_LONGEST];
    CliServersEmit emit;
    unsigned long long born;
    int has_text;
    unsigned long long size;
} CliServersInspectJob;

// The findings of the index as they are written: the line they go on, each file's path ended by a 0 then its findings
// as an array ended by a 0, whether a file is begun and whether it has a finding yet
typedef struct
{
    CliLine *found;
    int file_begun;
    int has_finding;
} CliServersInspectFound;

// how many trees were let go, under its own lock; and the tree the index was read from and when, under the index's
// lock, which a read holds
static unsigned long long s_generation;
static CliLock s_generation_lock = CLI_LOCK_INIT;
static char s_indexed[CLI_PATH_LONGEST];
static unsigned long long s_indexed_generation;
static CliLock s_index_lock = CLI_LOCK_INIT;
// what each family sets and the files seen, under the index's lock
static CliKeyed s_supplied_new = {"names newly supplied", {NULL}, 0u, CLI_SERVERS_INSPECT_FILES};
static CliKeyed s_seen = {"findings seen", {NULL}, 0u, CLI_SERVERS_INSPECT_FILES};

// how many trees were let go
static unsigned long long inspect_generation(void)
{
    cli_lock_take(&s_generation_lock);
    const unsigned long long generation = s_generation;
    cli_lock_give(&s_generation_lock);
    return generation;
}

// whether the index holds the tree at `root`, read since the last tree was let go; under the index's lock
static int inspect_holds(const char *root)
{
    return (s_indexed[0] != '\0') && (s_indexed_generation == inspect_generation()) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = s_indexed, .other = root, .cap = CLI_CELLS_POOL}));
}

int cli_servers_indexed(char *root)
{
    cli_lock_take(&s_index_lock);
    const int indexed = (s_indexed[0] != '\0') && (s_indexed_generation == inspect_generation());
    memor.cpy(&(MemoriaCfg){.dst = root, .src = s_indexed, .bytes = CLI_PATH_LONGEST});
    cli_lock_give(&s_index_lock);
    return indexed;
}

// the array of the file begun on the findings' line closed and ended by a 0
static void inspect_file_closed(CliServersInspectFound *found)
{
    if (found->file_begun)
    {
        cli_line_character(found->found, ']');
        cli_line_character(found->found, '\0');
    }
}

// a finding of a file of the index put after its file's, a file's path with none first
static void inspect_found(void *context, const char *path, const char *finding, unsigned long long size)
{
    CliServersInspectFound *const found = context;
    if (finding == NULL)
    {
        inspect_file_closed(found);
        cli_line_counted(found->found, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1ull);
        cli_line_character(found->found, '[');
        found->file_begun = 1;
        found->has_finding = 0;
        return;
    }
    if (!found->file_begun)
    {
        return;
    }
    cli_line_text(found->found, found->has_finding ? "," : "");
    cli_line_counted(found->found, finding, size);
    found->has_finding = 1;
}

// an attribute a class's family sets kept, by its file's key, as `class attribute` on a line after those kept
static void inspect_supplied(void *context, const char *path, const char *class_name, const char *attribute)
{
    const char *const root = context;
    char full[CLI_PATH_LONGEST];
    char key[CLI_PATH_LONGEST];
    cli_full(root, path, full);
    cli_servers_key_of(full, key);
    const char *beside = NULL;
    unsigned long long size = 0ull;
    const char *const kept = cli_keyed_get(&s_supplied_new, key, &beside, &size);
    const unsigned long long class_size = cellul.len(&(CatenaFinitaCfg){.src = class_name, .cap = CLI_CELLS_POOL});
    const unsigned long long attribute_size = cellul.len(&(CatenaFinitaCfg){.src = attribute, .cap = CLI_CELLS_POOL});
    const unsigned long long line_size = size + class_size + 1ull + attribute_size + 1ull;
    char *const line = cli_cell_taken(line_size, "the names a family sets");
    if (line == NULL)
    {
        return;
    }
    // each size is a path's, a name's or a line of them, far inside what a size_t counts
    memor.cpy(&(MemoriaCfg){.dst = line, .src = (kept != NULL) ? kept : "", .bytes = (size_t)size});
    memor.cpy(&(MemoriaCfg){.dst = line + size, .src = class_name, .bytes = (size_t)class_size});
    line[size + class_size] = ' ';
    memor.cpy(&(MemoriaCfg){.dst = line + size + class_size + 1ull, .src = attribute, .bytes = (size_t)attribute_size});
    line[line_size - 1ull] = '\n';
    (void)cli_keyed_put(&s_supplied_new, key, "", line, line_size);
    cli_cell_given(line);
}

// what each family sets put in place of what was kept: 1 where it moved; under the checks' lock
static int inspect_supplied_put(void)
{
    unsigned int kept = 0u;
    unsigned int made = 0u;
    int moved = 0;
    for (unsigned int at = 0u; at < g_cli_servers_supplied.entries; at += 1u)
    {
        kept += cli_keyed_entry(&g_cli_servers_supplied, at)->used ? 1u : 0u;
    }
    for (unsigned int at = 0u; !moved && (at < s_supplied_new.entries); at += 1u)
    {
        const CliKeyedEntry *const one = cli_keyed_entry(&s_supplied_new, at);
        if (!one->used)
        {
            continue;
        }
        made += 1u;
        const char *beside = NULL;
        unsigned long long size = 0ull;
        unsigned long long other = 0ull;
        const char *const key = one->cell;
        const char *const text = cli_keyed_get(&s_supplied_new, key, &beside, &size);
        const char *const was = cli_keyed_get(&g_cli_servers_supplied, key, &beside, &other);
        moved = (was == NULL) || (other != size) ||
                (memor.cmp(&(MemoriaCfg){.src = was, .other = text, .bytes = (size_t)size}) != 0);
    }
    if (!moved && (kept == made))
    {
        return 0;
    }
    cli_keyed_clear(&g_cli_servers_supplied);
    for (unsigned int at = 0u; at < s_supplied_new.entries; at += 1u)
    {
        const CliKeyedEntry *const one = cli_keyed_entry(&s_supplied_new, at);
        if (!one->used)
        {
            continue;
        }
        const char *beside = NULL;
        unsigned long long size = 0ull;
        const char *const text = cli_keyed_get(&s_supplied_new, one->cell, &beside, &size);
        if (text != NULL)
        {
            (void)cli_keyed_put(&g_cli_servers_supplied, one->cell, "", text, size);
        }
    }
    return 1;
}

// a file whose findings changed put on the line that is `changed`, its key then its URI, each ended by a 0
static void inspect_changed(CliLine *changed, const char *key, const char *uri)
{
    cli_line_counted(changed, key, cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1ull);
    cli_line_counted(changed, uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}) + 1ull);
}

// each file's findings of `size` bytes of `found` kept, by its key with its URI, and each file whose findings changed
// passed on
static void inspect_passed_on(const char *root, const char *found, unsigned long long size, const CliServersEmit *emit)
{
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    CliLine changed;
    cli_pieces_begun(&pieces, &changed, piece, sizeof(piece));
    cli_keyed_clear(&s_seen);
    cli_lock_take(&g_cli_servers_checks_lock);
    for (unsigned long long at = 0ull; at < size;)
    {
        const char *const path = found + at;
        const char *const values = path + cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u;
        const unsigned long long values_size = cellul.len(&(CatenaFinitaCfg){.src = values, .cap = CLI_CELLS_POOL});
        at += cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u + values_size + 1u;
        char full[CLI_PATH_LONGEST];
        char uri[CLI_PATH_LONGEST];
        char key[CLI_PATH_LONGEST];
        cli_full(root, path, full);
        cli_servers_uri_of(full, uri);
        cli_lsp_key_of(uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), key);
        (void)cli_keyed_put(&s_seen, key, "", "", 0ull);
        const char *beside = NULL;
        unsigned long long kept_size = 0ull;
        const char *const kept = cli_keyed_get(&g_cli_servers_inspected, key, &beside, &kept_size);
        const int same = (kept != NULL) && (kept_size == values_size) &&
                         (memor.cmp(&(MemoriaCfg){.src = kept, .other = values, .bytes = (size_t)values_size}) == 0);
        if (((kept != NULL) || (values_size > 2u)) && !same)
        {
            (void)cli_keyed_put(&g_cli_servers_inspected, key, uri, values, values_size);
            inspect_changed(&changed, key, uri);
        }
    }
    for (unsigned int at = 0u; at < g_cli_servers_inspected.entries; at += 1u)
    {
        CliKeyedEntry *const one = cli_keyed_entry(&g_cli_servers_inspected, at);
        const char *const key = one->cell;
        const char *beside = NULL;
        unsigned long long kept_size = 0ull;
        if (one->used && (cli_keyed_get(&s_seen, key, &beside, &kept_size) == NULL))
        {
            (void)cli_keyed_get(&g_cli_servers_inspected, key, &beside, &kept_size);
            inspect_changed(&changed, key, beside);
            one->used = 0;
        }
    }
    cli_lock_give(&g_cli_servers_checks_lock);
    unsigned long long changed_size = 0ull;
    char *const files = cli_pieces_joined(&pieces, &changed, "the files whose findings changed", &changed_size);
    for (unsigned long long at = 0ull; (files != NULL) && (at < changed_size);)
    {
        const char *const key = files + at;
        const char *const uri = key + cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u;
        cli_servers_emit_whole(key, uri, emit);
        at += cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u +
              cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}) + 1u;
    }
    cli_cell_given(files);
}

// the index read from the tree at `root`, each Python and JavaScript file of it read where it lies, the view of the
// tree's files held in place for the read: 1, or 0 where a tree was let go meanwhile or the view could not be held
static int inspect_index_read(const char *root, unsigned long long born)
{
    const CliFiles *const files = cli_files_held(root);
    if (files == NULL)
    {
        return 0;
    }
    cli_inspect_tree_cleared();
    for (unsigned int at = 0u; (at < files->count) && (inspect_generation() == born); at += 1u)
    {
        const char *const one = cli_files_at(files, at);
        const char *const language = cli_inspect_language_of(one);
        if (language == NULL)
        {
            continue;
        }
        char path[CLI_PATH_LONGEST];
        cli_full(root, one, path);
        unsigned long long size = 0ull;
        const unsigned char *const mapped = cli_file_held(path, &size);
        if ((mapped != NULL) && cli_utf8_whole((const char *)mapped, size))
        {
            (void)cli_inspect_tree_set(one, language, (const char *)mapped, size);
        }
        if (mapped != NULL)
        {
            cli_file_let_go(mapped, size);
        }
    }
    cli_files_let(files);
    if (inspect_generation() != born)
    {
        cli_inspect_tree_cleared();
        s_indexed[0] = '\0';
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = s_indexed,
                            .src = root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    s_indexed_generation = born;
    return 1;
}

// a file read into the index, the tree's read first where the index holds another, and the findings passed on
static void inspect_job(const void *job, unsigned long long size)
{
    const CliServersInspectJob *const read = job;
    if (size < sizeof(*read) + read->size)
    {
        return;
    }
    cli_lock_take(&s_index_lock);
    if (!inspect_holds(read->root) && !inspect_index_read(read->root, read->born))
    {
        cli_lock_give(&s_index_lock);
        return;
    }
    const char *const language = cli_inspect_language_of(read->file);
    if (read->has_text && (language != NULL))
    {
        (void)cli_inspect_tree_set(read->file, language, (const char *)job + sizeof(*read), read->size);
    }
    else
    {
        cli_inspect_tree_remove(read->file);
    }
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    CliLine line;
    cli_pieces_begun(&pieces, &line, piece, sizeof(piece));
    CliServersInspectFound found = {&line, 0, 0};
    cli_inspect_tree_findings(inspect_found, &found);
    inspect_file_closed(&found);
    unsigned long long found_size = 0ull;
    char *const findings = cli_pieces_joined(&pieces, &line, "the index's findings", &found_size);
    cli_keyed_clear(&s_supplied_new);
    cli_inspect_tree_supplied(inspect_supplied, (void *)read->root);
    cli_lock_take(&g_cli_servers_checks_lock);
    const int moved = inspect_supplied_put();
    cli_lock_give(&g_cli_servers_checks_lock);
    if (findings != NULL)
    {
        inspect_passed_on(read->root, findings, found_size, &read->emit);
    }
    cli_cell_given(findings);
    // what a family sets decides whether a server's reports of an attribute pass on
    if (moved)
    {
        cli_servers_emit_reporting_attributes(&read->emit);
    }
    // a file the editor opens hears its diagnostics, changed or the same
    if (read->has_text)
    {
        char full[CLI_PATH_LONGEST];
        char uri[CLI_PATH_LONGEST];
        char key[CLI_PATH_LONGEST];
        cli_full(read->root, read->file, full);
        cli_servers_uri_of(full, uri);
        cli_lsp_key_of(uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), key);
        cli_servers_emit_whole(key, uri, &read->emit);
    }
    cli_lock_give(&s_index_lock);
}

void cli_servers_inspect(const char *root, const char *path, const char *text, unsigned long long size,
                         const CliServersEmit *emit)
{
    const size_t top = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL});
    if (!cli_text_same(path, root, top) || ((path[top] != '/') && (path[top] != '\\')) ||
        (cli_inspect_language_of(path) == NULL))
    {
        return;
    }
    // the read made in a cell of its size, the text after it where it is given
    const int has_text = (text != NULL) && (size <= CLI_SERVERS_LARGEST);
    const unsigned long long text_size = has_text ? size : 0ull;
    char *const job = cli_cell_taken(sizeof(CliServersInspectJob) + text_size, "a read into the inspections' index");
    if (job == NULL)
    {
        return;
    }
    CliServersInspectJob *const read = (CliServersInspectJob *)(void *)job;
    memor.set(&(MemoriaCfg){.dst = read, .val = 0, .bytes = sizeof(*read)});
    memor.cpy(&(MemoriaCfg){.dst = read->root, .src = root, .bytes = top + 1u});
    CliLine file = {read->file, CLI_PATH_LONGEST, 0ull};
    for (const char *at = path + top + 1u; *at != '\0'; at += 1)
    {
        cli_line_character(&file, (*at == '\\') ? '/' : *at);
    }
    (void)cli_line_closed(&file);
    read->emit = *emit;
    read->has_text = has_text;
    read->size = text_size;
    if (has_text)
    {
        // a size under CLI_SERVERS_LARGEST fits a size_t
        memor.cpy(&(MemoriaCfg){.dst = job + sizeof(*read), .src = text, .bytes = (size_t)text_size});
    }
    cli_lock_take(&g_cli_servers.lock);
    g_cli_servers.told = *emit;
    g_cli_servers.has_told = 1;
    cli_lock_give(&g_cli_servers.lock);
    read->born = inspect_generation();
    cli_lock_take(&s_index_lock);
    const int ready = inspect_holds(root);
    cli_lock_give(&s_index_lock);
    if (ready)
    {
        inspect_job(job, sizeof(*read) + text_size);
        cli_cell_given(job);
        return;
    }
    (void)cli_servers_later_handed(inspect_job, job, sizeof(*read) + text_size);
}

void cli_servers_inspect_again(const char *path, const char *text, unsigned long long size)
{
    char root[CLI_PATH_LONGEST];
    static CliServersEmit s_told;
    static CliLock s_told_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_told_lock);
    cli_lock_take(&g_cli_servers.lock);
    const int told = g_cli_servers.has_told;
    s_told = g_cli_servers.told;
    cli_lock_give(&g_cli_servers.lock);
    if (told && cli_servers_indexed(root))
    {
        cli_servers_inspect(root, path, text, size, &s_told);
    }
    cli_lock_give(&s_told_lock);
}

void cli_servers_inspect_let_go(void)
{
    cli_lock_take(&s_generation_lock);
    s_generation += 1ull;
    cli_lock_give(&s_generation_lock);
}

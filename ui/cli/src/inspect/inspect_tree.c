// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_tree.c: the tree stored: each file's path and facts copied whole into a cell of its own, its arrays
// laid one after another; a file found by its path through a table; and the function of a file that contains a line, or
// whose name starts on it
#include "../command_line/cells.h"
#include "inspect_tree_internal.h"

// the buckets of the table a path is found by: each a file's place and one, 0 where none is and CLI_TREE_NONE where
// one was taken out
#define CLI_TREE_BUCKETS (2u * CLI_TREE_FILES)

CliTreeFile *g_cli_tree_page[CLI_TREE_PAGES];
unsigned int g_cli_tree_files;
CliLock g_cli_tree_lock = CLI_LOCK_INIT;

static unsigned int s_bucket[CLI_TREE_BUCKETS];

// The storage a file's facts are worked out in before they are stored: one cell sized to the file's text, its lists one
// after another, taken for one file and given back once its facts are stored
typedef struct
{
    CliInspectImport *imports;
    CliInspectClass *classes;
    CliInspectFunction *functions;
    CliInspectFinding *findings;
    CliInspectFix *fixes;
    CliInspectEdit *edits;
    CliInspectName *names;
    char *text;
    CliFacts facts;
} CliTreeWork;

// the next part of a cell, `bytes` long, taken from `*at`, its start aligned to a word's boundary: its start
static void *tree_part(unsigned char **at, unsigned long long bytes)
{
    void *const part = *at;
    *at += (bytes + 7ull) & ~7ull;
    return part;
}

// the work of `size` bytes of `text` in `language` made over one cell sized to what the walk picks up: the facts are
// counted by a walk that stores nothing, the cell taken for the counts, and the walk after fills it. The cell, or NULL
// where the language is not read or the cell cannot be taken
static void *tree_work_taken(CliTreeWork *work, const char *language, const char *text, unsigned long long size)
{
    CliFacts counted;
    cli_facts_counting(&counted);
    if (!cli_inspect_facts(language, text, size, &counted))
    {
        return NULL;
    }
    const unsigned long long bytes =
        (counted.imports * sizeof(CliInspectImport)) + (counted.classes * sizeof(CliInspectClass)) +
        (counted.functions * sizeof(CliInspectFunction)) + (counted.findings * sizeof(CliInspectFinding)) +
        (counted.fixes * sizeof(CliInspectFix)) + (counted.edits * sizeof(CliInspectEdit)) +
        (counted.names * sizeof(CliInspectName)) + counted.used + (8ull * 8ull);
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(bytes, NULL);
    if (cell == NULL)
    {
        return NULL;
    }
    unsigned char *at = cell;
    work->imports = tree_part(&at, counted.imports * sizeof(CliInspectImport));
    work->classes = tree_part(&at, counted.classes * sizeof(CliInspectClass));
    work->functions = tree_part(&at, counted.functions * sizeof(CliInspectFunction));
    work->findings = tree_part(&at, counted.findings * sizeof(CliInspectFinding));
    work->fixes = tree_part(&at, counted.fixes * sizeof(CliInspectFix));
    work->edits = tree_part(&at, counted.edits * sizeof(CliInspectEdit));
    work->names = tree_part(&at, counted.names * sizeof(CliInspectName));
    work->text = tree_part(&at, counted.used);
    cli_facts_begun(&work->facts, work->imports, counted.imports, work->classes, counted.classes, work->functions,
                    counted.functions, work->findings, counted.findings, work->fixes, counted.fixes, work->edits,
                    counted.edits, work->names, counted.names, work->text, counted.used);
    return cell;
}

// the bucket a path hashes to, FNV-1a over its bytes
static unsigned int tree_hash(const char *path, unsigned long long size)
{
    unsigned int hash = 2166136261u;
    for (unsigned long long at = 0ull; at < size; at += 1u)
    {
        hash = (hash ^ (unsigned char)path[at]) * 16777619u;
    }
    return hash & (CLI_TREE_BUCKETS - 1u);
}

unsigned int cli_tree_find(const char *path, unsigned long long size)
{
    unsigned int bucket = tree_hash(path, size);
    for (unsigned int probe = 0u; (probe < CLI_TREE_BUCKETS) && (s_bucket[bucket] != 0u); probe += 1u)
    {
        const unsigned int place = s_bucket[bucket];
        if (place != CLI_TREE_NONE)
        {
            const CliTreeFile *const file = &CLI_TREE_FILE(place - 1u);
            if ((file->path_size == size) &&
                (memor.cmp(&(MemoriaCfg){.src = file->path, .other = path, .bytes = (size_t)size}) == 0))
            {
                return place - 1u;
            }
        }
        bucket = (bucket + 1u) & (CLI_TREE_BUCKETS - 1u);
    }
    return CLI_TREE_NONE;
}

unsigned int cli_tree_find_joined(const char *folder, unsigned long long folder_size, const char *name)
{
    char path[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea line = {path, sizeof(path), 0ull};
    mmgr_line_put_n(&line, folder, folder_size);
    mmgr_line_put(&line, (folder_size > 0u) ? "/" : "");
    mmgr_line_put(&line, name);
    return mmgr_line_ok(&line) ? cli_tree_find(path, line.at) : CLI_TREE_NONE;
}

unsigned long long cli_tree_parent(const char *path, unsigned long long size)
{
    unsigned long long end = size;
    while ((end > 0u) && (path[end - 1u] != '/'))
    {
        end -= 1u;
    }
    return (end > 0u) ? (end - 1u) : 0ull;
}

// a file's place put in the table
static void tree_bucket_put(unsigned int file)
{
    unsigned int bucket = tree_hash(CLI_TREE_FILE(file).path, CLI_TREE_FILE(file).path_size);
    while ((s_bucket[bucket] != 0u) && (s_bucket[bucket] != CLI_TREE_NONE))
    {
        bucket = (bucket + 1u) & (CLI_TREE_BUCKETS - 1u);
    }
    s_bucket[bucket] = file + 1u;
}

// a file's place taken out of the table
static void tree_bucket_taken(unsigned int file)
{
    unsigned int bucket = tree_hash(CLI_TREE_FILE(file).path, CLI_TREE_FILE(file).path_size);
    for (unsigned int probe = 0u; (probe < CLI_TREE_BUCKETS) && (s_bucket[bucket] != 0u); probe += 1u)
    {
        if (s_bucket[bucket] == file + 1u)
        {
            s_bucket[bucket] = CLI_TREE_NONE;
            return;
        }
        bucket = (bucket + 1u) & (CLI_TREE_BUCKETS - 1u);
    }
}

// `size` bytes of `bytes` copied into `cell` at `*at`, which moves past them, aligned to eight bytes: where they stand
static void *tree_copied(unsigned char *cell, unsigned long long *at, const void *bytes, unsigned long long size)
{
    void *const place = cell + *at;
    memor.cpy(&(MemoriaCfg){.dst = place, .src = bytes, .bytes = (size_t)size});
    *at += (size + 7u) & ~7ull;
    return place;
}

// the bytes a file of `path_size` bytes of path and the facts worked out take in its cell
static unsigned long long tree_size(const CliFacts *facts, unsigned long long path_size)
{
    unsigned long long size = (path_size + 1u + 7u) & ~7ull;
    size += ((facts->imports * sizeof(CliInspectImport)) + 7u) & ~7ull;
    size += ((facts->classes * sizeof(CliInspectClass)) + 7u) & ~7ull;
    size += ((facts->functions * sizeof(CliInspectFunction)) + 7u) & ~7ull;
    size += ((facts->findings * sizeof(CliInspectFinding)) + 7u) & ~7ull;
    size += ((facts->fixes * sizeof(CliInspectFix)) + 7u) & ~7ull;
    size += ((facts->edits * sizeof(CliInspectEdit)) + 7u) & ~7ull;
    size += ((facts->names * sizeof(CliInspectName)) + 7u) & ~7ull;
    size += (facts->used + 7u) & ~7ull;
    return size;
}

// the file at `file` taken out of the tree, its cell given back
static void tree_taken(unsigned int file)
{
    tree_bucket_taken(file);
    cli_cell_release(CLI_TREE_FILE(file).cell, NULL);
    CLI_TREE_FILE(file).cell = NULL;
    CLI_TREE_FILE(file).used = 0;
    cli_tree_families_given();
}

// a place among the files for a new one: its place, CLI_TREE_NONE where every place is taken
static unsigned int tree_place(void)
{
    for (unsigned int file = 0u; file < g_cli_tree_files; file += 1u)
    {
        if (!CLI_TREE_FILE(file).used)
        {
            return file;
        }
    }
    if (g_cli_tree_files == CLI_TREE_FILES)
    {
        return CLI_TREE_NONE;
    }
    // a page taken as its first place is
    const unsigned int page = g_cli_tree_files / CLI_TREE_PAGE;
    if (g_cli_tree_page[page] == NULL)
    {
        g_cli_tree_page[page] = g_cli_cells->persistent_buf_alloc(CLI_TREE_PAGE * sizeof(CliTreeFile), NULL);
        if (g_cli_tree_page[page] == NULL)
        {
            return CLI_TREE_NONE;
        }
        memor.set(&(MemoriaCfg){.dst = g_cli_tree_page[page], .val = 0, .bytes = CLI_TREE_PAGE * sizeof(CliTreeFile)});
    }
    g_cli_tree_files += 1u;
    return g_cli_tree_files - 1u;
}

int cli_inspect_tree_set(const char *path, const char *language, const char *text, unsigned long long size)
{
    // the facts are worked out in a cell of the call's own, sized to what they contain, before the lock is taken
    CliTreeWork work_copy;
    CliTreeWork *const work = &work_copy;
    void *const work_cell = tree_work_taken(work, language, text, size);
    if (work_cell == NULL)
    {
        return 0;
    }
    CliFacts *const facts = &work->facts;
    const int read = cli_inspect_facts(language, text, size, facts) && !facts->full;
    const unsigned long long path_size = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    const unsigned long long needed = read ? tree_size(facts, path_size) : 0ull;
    unsigned char *const cell = read ? g_cli_cells->persistent_buf_alloc(needed, NULL) : NULL;
    cli_lock_take(&g_cli_tree_lock);
    const unsigned int place = cli_tree_find(path, path_size);
    if (place != CLI_TREE_NONE)
    {
        tree_taken(place);
    }
    const unsigned int file = (cell != NULL) ? tree_place() : CLI_TREE_NONE;
    if (file == CLI_TREE_NONE)
    {
        cli_lock_give(&g_cli_tree_lock);
        cli_cell_release(cell, NULL);
        cli_cell_release(work_cell, NULL);
        return 0;
    }
    CliTreeFile *const tree_file = &CLI_TREE_FILE(file);
    unsigned long long at = 0ull;
    tree_file->cell = cell;
    tree_file->size = needed;
    char *const path_copy = tree_copied(cell, &at, path, path_size + 1u);
    tree_file->path = path_copy;
    tree_file->path_size = (unsigned int)path_size;
    tree_file->python = (cellul.eq(&(CatenaFinitaCfg){.src = language, .other = "python", .cap = CLI_CELLS_POOL}));
    tree_file->facts = *facts;
    tree_file->facts.import = tree_copied(cell, &at, work->imports, facts->imports * sizeof(CliInspectImport));
    tree_file->facts.class_of = tree_copied(cell, &at, work->classes, facts->classes * sizeof(CliInspectClass));
    tree_file->facts.function = tree_copied(cell, &at, work->functions, facts->functions * sizeof(CliInspectFunction));
    tree_file->facts.finding = tree_copied(cell, &at, work->findings, facts->findings * sizeof(CliInspectFinding));
    tree_file->facts.fix = tree_copied(cell, &at, work->fixes, facts->fixes * sizeof(CliInspectFix));
    tree_file->facts.edit = tree_copied(cell, &at, work->edits, facts->edits * sizeof(CliInspectEdit));
    tree_file->facts.name = tree_copied(cell, &at, work->names, facts->names * sizeof(CliInspectName));
    tree_file->facts.text = tree_copied(cell, &at, work->text, facts->used);
    tree_file->facts.imports_most = facts->imports;
    tree_file->facts.classes_most = facts->classes;
    tree_file->facts.functions_most = facts->functions;
    tree_file->facts.findings_most = facts->findings;
    tree_file->facts.fixes_most = facts->fixes;
    tree_file->facts.edits_most = facts->edits;
    tree_file->facts.names_most = facts->names;
    tree_file->facts.text_most = facts->used;
    tree_file->used = 1;
    tree_bucket_put(file);
    cli_tree_families_given();
    cli_lock_give(&g_cli_tree_lock);
    cli_cell_release(work_cell, NULL);
    return 1;
}

void cli_inspect_tree_remove(const char *path)
{
    cli_lock_take(&g_cli_tree_lock);
    const unsigned int place = cli_tree_find(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    if (place != CLI_TREE_NONE)
    {
        tree_taken(place);
    }
    cli_lock_give(&g_cli_tree_lock);
}

void cli_inspect_tree_cleared(void)
{
    cli_lock_take(&g_cli_tree_lock);
    for (unsigned int file = 0u; file < g_cli_tree_files; file += 1u)
    {
        cli_cell_release(CLI_TREE_FILE(file).cell, NULL);
        CLI_TREE_FILE(file).cell = NULL;
        CLI_TREE_FILE(file).used = 0;
    }
    for (unsigned int page = 0u; page < CLI_TREE_PAGES; page += 1u)
    {
        cli_cell_release(g_cli_tree_page[page], NULL);
        g_cli_tree_page[page] = NULL;
    }
    g_cli_tree_files = 0u;
    memor.set(&(MemoriaCfg){.dst = s_bucket, .val = 0, .bytes = sizeof(s_bucket)});
    cli_tree_families_given();
    cli_lock_give(&g_cli_tree_lock);
}

int cli_inspect_tree_contains(const char *path)
{
    cli_lock_take(&g_cli_tree_lock);
    const int found =
        (cli_tree_find(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL})) != CLI_TREE_NONE);
    cli_lock_give(&g_cli_tree_lock);
    return found;
}

void cli_inspect_tree_paths(void (*each)(void *context, const char *path), void *context)
{
    cli_lock_take(&g_cli_tree_lock);
    for (unsigned int file = 0u; file < g_cli_tree_files; file += 1u)
    {
        if (CLI_TREE_FILE(file).used)
        {
            each(context, CLI_TREE_FILE(file).path);
        }
    }
    cli_lock_give(&g_cli_tree_lock);
}

int cli_inspect_tree_function_at(const char *path, unsigned int line, unsigned int *at)
{
    cli_lock_take(&g_cli_tree_lock);
    const unsigned int file = cli_tree_find(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    int found = 0;
    unsigned int least = 0u;
    for (unsigned int index = 0u; (file != CLI_TREE_NONE) && (index < CLI_TREE_FILE(file).facts.functions); index += 1u)
    {
        const CliInspectFunction *const one = &CLI_TREE_FILE(file).facts.function[index];
        if ((one->first <= line) && (line <= one->last) && (!found || (one->last - one->first < least)))
        {
            found = 1;
            least = one->last - one->first;
            *at = index;
        }
    }
    cli_lock_give(&g_cli_tree_lock);
    return found;
}

int cli_inspect_tree_function(const char *path, unsigned int at, CliInspectReached each, void *context)
{
    cli_lock_take(&g_cli_tree_lock);
    const unsigned int file = cli_tree_find(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    const int found = (file != CLI_TREE_NONE) && (at < CLI_TREE_FILE(file).facts.functions);
    if (found)
    {
        each(context, CLI_TREE_FILE(file).path, &CLI_TREE_FILE(file).facts, at, NULL, 0u);
    }
    cli_lock_give(&g_cli_tree_lock);
    return found;
}

int cli_inspect_tree_function_on(const char *path, unsigned int line, unsigned int *at)
{
    cli_lock_take(&g_cli_tree_lock);
    const unsigned int file = cli_tree_find(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    int found = 0;
    for (unsigned int index = 0u; !found && (file != CLI_TREE_NONE) && (index < CLI_TREE_FILE(file).facts.functions);
         index += 1u)
    {
        if (CLI_TREE_FILE(file).facts.function[index].from.line == line)
        {
            found = 1;
            *at = index;
        }
    }
    cli_lock_give(&g_cli_tree_lock);
    return found;
}

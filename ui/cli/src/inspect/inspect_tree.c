// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_tree.c: the tree stored: each file's path and facts copied whole into a cell of its own, its arrays
// laid one after another; a file found by its path through a table; and the function of a file that contains a line, or
// whose name starts on it
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../text/text.h"
#include "inspect_tree_internal.h"

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
// the buckets of the table a path is found by: each a file's place and one, 0 where none is and CLI_TREE_NONE where
// one was taken out
#define CLI_TREE_BUCKETS (2u * CLI_TREE_FILES)

CliTreeFile *g_cli_tree_page[CLI_TREE_PAGES];
unsigned int g_cli_tree_files;
CliLock g_cli_tree_lock = CLI_LOCK_INIT;

static unsigned int s_bucket[CLI_TREE_BUCKETS];

typedef struct
{
    unsigned char **at;
    unsigned long long bytes;
    CliTreeWork *work;
    const char *language;
    const char *text;
    unsigned long long size;
    const char *path;
    unsigned int file;
    unsigned char *cell;
    unsigned long long *at_u64s;
    const void *bytes_bytess;
    const CliFacts *facts;
    unsigned long long path_size;
} InspectTreeCtx;
// the next part of a cell, `bytes` long, taken from `*at`, its start aligned to a word's boundary: its start
EMBED_INLINE void *tree_part(const InspectTreeCtx *args)
{
    unsigned char **at = args->at;
    void *const part = *at;
    *at += (args->bytes + 7ull) & ~7ull;
    return part;
}

// the work of `size` bytes of `text` in `language` made over one cell sized to what the walk picks up: the facts are
// counted by a walk that stores nothing, the cell taken for the counts, and the walk after fills it. The cell, or NULL
// where the language is not read or the cell cannot be taken
EMBED_INLINE void *tree_work_take(const InspectTreeCtx *args)
{
    const char *language = args->language;
    const char *text = args->text;
    unsigned long long size = args->size;
    CliFacts counted;
    EMBED_CALL(cli_inspect_facts_language.facts_counting, CliInspectFactsLanguageCfg, .facts = &counted);
    if (!EMBED_CALL(cli_inspect_facts_language.facts, CliInspectFactsLanguageCfg, .language = language, .text = text,
                    .size = size, .facts = &counted))
    {
        return NULL;
    }
    const unsigned long long bytes =
        (counted.imports * sizeof(CliInspectImport)) + (counted.classes * sizeof(CliInspectClass)) +
        (counted.functions * sizeof(CliInspectFunction)) + (counted.findings * sizeof(CliInspectFinding)) +
        (counted.fixes * sizeof(CliInspectFix)) + (counted.edits * sizeof(CliInspectEdit)) +
        (counted.names * sizeof(CliInspectName)) + counted.used + (8ull * 8ull);
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(bytes, &g_cli_inspect_cells);
    if (cell == NULL)
    {
        return NULL;
    }
    unsigned char *at = cell;
    args->work->imports =
        EMBED_CALL(tree_part, InspectTreeCtx, .at = &at, .bytes = counted.imports * sizeof(CliInspectImport));
    args->work->classes =
        EMBED_CALL(tree_part, InspectTreeCtx, .at = &at, .bytes = counted.classes * sizeof(CliInspectClass));
    args->work->functions =
        EMBED_CALL(tree_part, InspectTreeCtx, .at = &at, .bytes = counted.functions * sizeof(CliInspectFunction));
    args->work->findings =
        EMBED_CALL(tree_part, InspectTreeCtx, .at = &at, .bytes = counted.findings * sizeof(CliInspectFinding));
    args->work->fixes =
        EMBED_CALL(tree_part, InspectTreeCtx, .at = &at, .bytes = counted.fixes * sizeof(CliInspectFix));
    args->work->edits =
        EMBED_CALL(tree_part, InspectTreeCtx, .at = &at, .bytes = counted.edits * sizeof(CliInspectEdit));
    args->work->names =
        EMBED_CALL(tree_part, InspectTreeCtx, .at = &at, .bytes = counted.names * sizeof(CliInspectName));
    args->work->text = EMBED_CALL(tree_part, InspectTreeCtx, .at = &at, .bytes = counted.used);
    EMBED_CALL(cli_inspect_facts_table.facts_begin, CliInspectFactsCfg, .facts = &args->work->facts,
               .imports = args->work->imports, .imports_most = counted.imports, .classes = args->work->classes,
               .classes_most = counted.classes, .functions = args->work->functions, .functions_most = counted.functions,
               .findings = args->work->findings, .findings_most = counted.findings, .fixes = args->work->fixes,
               .fixes_most = counted.fixes, .edits = args->work->edits, .edits_most = counted.edits,
               .names = args->work->names, .names_most = counted.names, .text_out = args->work->text,
               .text_most = counted.used);
    return cell;
}

// the bucket a path hashes to, FNV-1a over its bytes
EMBED_INLINE unsigned int tree_hash(const InspectTreeCtx *args)
{
    unsigned int hash = 2166136261u;
    for (unsigned long long at = 0ull; at < args->size; at += 1u)
    {
        hash = (hash ^ (unsigned char)args->path[at]) * 16777619u;
    }
    return hash & (CLI_TREE_BUCKETS - 1u);
}

unsigned int cli_tree_find(const CliInspectTreePathCtx *args)
{
    unsigned int bucket = EMBED_CALL(tree_hash, InspectTreeCtx, .path = args->path, .size = args->size);
    for (unsigned int probe = 0u; (probe < CLI_TREE_BUCKETS) && (s_bucket[bucket] != 0u); probe += 1u)
    {
        const unsigned int place = s_bucket[bucket];
        if (place != CLI_TREE_NONE)
        {
            const CliTreeFile *const file = &CLI_TREE_FILE(place - 1u);
            if ((file->path_size == args->size) &&
                (memor.cmp(&(MemoriaCfg){.src = file->path, .other = args->path, .bytes = (size_t)args->size}) == 0))
            {
                return place - 1u;
            }
        }
        bucket = (bucket + 1u) & (CLI_TREE_BUCKETS - 1u);
    }
    return CLI_TREE_NONE;
}

unsigned int cli_tree_find_join(const CliInspectTreeFolderCtx *args)
{
    char path[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea line = {path, sizeof(path), 0ull};
    mmgr_line_put_n(&line, args->folder, args->folder_size);
    mmgr_line_put(&line, (args->folder_size > 0u) ? "/" : "");
    mmgr_line_put(&line, args->name);
    return mmgr_line_ok(&line) ? EMBED_CALL(cli_inspect_tree_path_internal.tree_find, CliInspectTreePathCtx,
                                            .path = path, .size = line.at)
                               : CLI_TREE_NONE;
}

unsigned long long cli_tree_parent(const CliInspectTreePathCtx *args)
{
    unsigned long long end = args->size;
    end = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path, .at = end,
                     .set = "/");
    return (end > 0u) ? (end - 1u) : 0ull;
}

// a file's place put in the table
EMBED_INLINE void tree_bucket_put(const InspectTreeCtx *args)
{
    unsigned int bucket = EMBED_CALL(tree_hash, InspectTreeCtx, .path = CLI_TREE_FILE(args->file).path,
                                     .size = CLI_TREE_FILE(args->file).path_size);
    while ((s_bucket[bucket] != 0u) && (s_bucket[bucket] != CLI_TREE_NONE))
    {
        bucket = (bucket + 1u) & (CLI_TREE_BUCKETS - 1u);
    }
    s_bucket[bucket] = args->file + 1u;
}

// a file's place taken out of the table
EMBED_INLINE void tree_bucket_take(const InspectTreeCtx *args)
{
    unsigned int bucket = EMBED_CALL(tree_hash, InspectTreeCtx, .path = CLI_TREE_FILE(args->file).path,
                                     .size = CLI_TREE_FILE(args->file).path_size);
    for (unsigned int probe = 0u; (probe < CLI_TREE_BUCKETS) && (s_bucket[bucket] != 0u); probe += 1u)
    {
        if (s_bucket[bucket] == args->file + 1u)
        {
            s_bucket[bucket] = CLI_TREE_NONE;
            return;
        }
        bucket = (bucket + 1u) & (CLI_TREE_BUCKETS - 1u);
    }
}

// `size` bytes of `bytes` copied into `cell` at `*at`, which moves past them, aligned to eight bytes: where they stand
EMBED_INLINE void *tree_copy(const InspectTreeCtx *args)
{
    unsigned long long *at = args->at_u64s;
    const void *bytes = args->bytes_bytess;
    void *const place = args->cell + *at;
    memor.cpy(&(MemoriaCfg){.dst = place, .src = bytes, .bytes = (size_t)args->size});
    *at += (args->size + 7u) & ~7ull;
    return place;
}

// the bytes a file of `path_size` bytes of path and the facts worked out take in its cell
EMBED_INLINE unsigned long long tree_size(const InspectTreeCtx *args)
{
    unsigned long long size = (args->path_size + 1u + 7u) & ~7ull;
    size += ((args->facts->imports * sizeof(CliInspectImport)) + 7u) & ~7ull;
    size += ((args->facts->classes * sizeof(CliInspectClass)) + 7u) & ~7ull;
    size += ((args->facts->functions * sizeof(CliInspectFunction)) + 7u) & ~7ull;
    size += ((args->facts->findings * sizeof(CliInspectFinding)) + 7u) & ~7ull;
    size += ((args->facts->fixes * sizeof(CliInspectFix)) + 7u) & ~7ull;
    size += ((args->facts->edits * sizeof(CliInspectEdit)) + 7u) & ~7ull;
    size += ((args->facts->names * sizeof(CliInspectName)) + 7u) & ~7ull;
    size += (args->facts->used + 7u) & ~7ull;
    return size;
}

// the file at `file` taken out of the tree, its cell given back
EMBED_INLINE void tree_take(const InspectTreeCtx *args)
{
    tree_bucket_take(args);
    g_cli_cells->persistent_buf_release(CLI_TREE_FILE(args->file).cell, &g_cli_inspect_cells);
    CLI_TREE_FILE(args->file).cell = NULL;
    CLI_TREE_FILE(args->file).used = 0;
    cli_tree_families_free();
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
        g_cli_tree_page[page] =
            g_cli_cells->persistent_buf_alloc(CLI_TREE_PAGE * sizeof(CliTreeFile), &g_cli_inspect_cells);
        if (g_cli_tree_page[page] == NULL)
        {
            return CLI_TREE_NONE;
        }
        memor.set(&(MemoriaCfg){.dst = g_cli_tree_page[page], .val = 0, .bytes = CLI_TREE_PAGE * sizeof(CliTreeFile)});
    }
    g_cli_tree_files += 1u;
    return g_cli_tree_files - 1u;
}

int cli_inspect_tree_set(const CliInspectTreePathLanguageCfg *args)
{
    const char *path = args->path;
    const char *language = args->language;
    const char *text = args->text;
    unsigned long long size = args->size;
    // the facts are worked out in a cell of the call's own, sized to what they contain, before the lock is taken
    CliTreeWork work_copy;
    CliTreeWork *const work = &work_copy;
    void *const work_cell =
        EMBED_CALL(tree_work_take, InspectTreeCtx, .work = work, .language = language, .text = text, .size = size);
    if (work_cell == NULL)
    {
        return 0;
    }
    CliFacts *const facts = &work->facts;
    const int read = EMBED_CALL(cli_inspect_facts_language.facts, CliInspectFactsLanguageCfg, .language = language,
                                .text = text, .size = size, .facts = facts) &&
                     !facts->full;
    const unsigned long long path_size = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    const unsigned long long needed =
        read ? EMBED_CALL(tree_size, InspectTreeCtx, .facts = facts, .path_size = path_size) : 0ull;
    unsigned char *const cell = read ? g_cli_cells->persistent_buf_alloc(needed, &g_cli_inspect_cells) : NULL;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_tree_lock);
    const unsigned int place =
        EMBED_CALL(cli_inspect_tree_path_internal.tree_find, CliInspectTreePathCtx, .path = path, .size = path_size);
    if (place != CLI_TREE_NONE)
    {
        EMBED_CALL(tree_take, InspectTreeCtx, .file = place);
    }
    const unsigned int file = (cell != NULL) ? tree_place() : CLI_TREE_NONE;
    if (file == CLI_TREE_NONE)
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_tree_lock);
        g_cli_cells->persistent_buf_release(cell, &g_cli_inspect_cells);
        g_cli_cells->persistent_buf_release(work_cell, &g_cli_inspect_cells);
        return 0;
    }
    CliTreeFile *const tree_file = &CLI_TREE_FILE(file);
    unsigned long long at = 0ull;
    tree_file->cell = cell;
    tree_file->size = needed;
    char *const path_copy = EMBED_CALL(tree_copy, InspectTreeCtx, .cell = cell, .at_u64s = &at, .bytes_bytess = path,
                                       .size = path_size + 1u);
    tree_file->path = path_copy;
    tree_file->path_size = (unsigned int)path_size;
    tree_file->python = (cellul.eq(&(CatenaFinitaCfg){.src = language, .other = "python", .cap = CLI_CELLS_POOL}));
    tree_file->facts = *facts;
    tree_file->facts.import =
        EMBED_CALL(tree_copy, InspectTreeCtx, .cell = cell, .at_u64s = &at, .bytes_bytess = work->imports,
                   .size = facts->imports * sizeof(CliInspectImport));
    tree_file->facts.class_of =
        EMBED_CALL(tree_copy, InspectTreeCtx, .cell = cell, .at_u64s = &at, .bytes_bytess = work->classes,
                   .size = facts->classes * sizeof(CliInspectClass));
    tree_file->facts.function =
        EMBED_CALL(tree_copy, InspectTreeCtx, .cell = cell, .at_u64s = &at, .bytes_bytess = work->functions,
                   .size = facts->functions * sizeof(CliInspectFunction));
    tree_file->facts.finding =
        EMBED_CALL(tree_copy, InspectTreeCtx, .cell = cell, .at_u64s = &at, .bytes_bytess = work->findings,
                   .size = facts->findings * sizeof(CliInspectFinding));
    tree_file->facts.fix = EMBED_CALL(tree_copy, InspectTreeCtx, .cell = cell, .at_u64s = &at,
                                      .bytes_bytess = work->fixes, .size = facts->fixes * sizeof(CliInspectFix));
    tree_file->facts.edit = EMBED_CALL(tree_copy, InspectTreeCtx, .cell = cell, .at_u64s = &at,
                                       .bytes_bytess = work->edits, .size = facts->edits * sizeof(CliInspectEdit));
    tree_file->facts.name = EMBED_CALL(tree_copy, InspectTreeCtx, .cell = cell, .at_u64s = &at,
                                       .bytes_bytess = work->names, .size = facts->names * sizeof(CliInspectName));
    tree_file->facts.text = EMBED_CALL(tree_copy, InspectTreeCtx, .cell = cell, .at_u64s = &at,
                                       .bytes_bytess = work->text, .size = facts->used);
    tree_file->facts.imports_most = facts->imports;
    tree_file->facts.classes_most = facts->classes;
    tree_file->facts.functions_most = facts->functions;
    tree_file->facts.findings_most = facts->findings;
    tree_file->facts.fixes_most = facts->fixes;
    tree_file->facts.edits_most = facts->edits;
    tree_file->facts.names_most = facts->names;
    tree_file->facts.text_most = facts->used;
    tree_file->used = 1;
    EMBED_CALL(tree_bucket_put, InspectTreeCtx, .file = file);
    cli_tree_families_free();
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_tree_lock);
    g_cli_cells->persistent_buf_release(work_cell, &g_cli_inspect_cells);
    return 1;
}

void cli_inspect_tree_remove(const CliInspectTreePathLineCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_tree_lock);
    const unsigned int place =
        EMBED_CALL(cli_inspect_tree_path_internal.tree_find, CliInspectTreePathCtx, .path = args->path,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}));
    if (place != CLI_TREE_NONE)
    {
        EMBED_CALL(tree_take, InspectTreeCtx, .file = place);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_tree_lock);
}

void cli_inspect_tree_clear(void)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_tree_lock);
    for (unsigned int file = 0u; file < g_cli_tree_files; file += 1u)
    {
        g_cli_cells->persistent_buf_release(CLI_TREE_FILE(file).cell, &g_cli_inspect_cells);
        CLI_TREE_FILE(file).cell = NULL;
        CLI_TREE_FILE(file).used = 0;
    }
    for (unsigned int page = 0u; page < CLI_TREE_PAGES; page += 1u)
    {
        g_cli_cells->persistent_buf_release(g_cli_tree_page[page], &g_cli_inspect_cells);
        g_cli_tree_page[page] = NULL;
    }
    g_cli_tree_files = 0u;
    memor.set(&(MemoriaCfg){.dst = s_bucket, .val = 0, .bytes = sizeof(s_bucket)});
    cli_tree_families_free();
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_tree_lock);
}

int cli_inspect_tree_contains(const CliInspectTreePathLineCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_tree_lock);
    const int found =
        (EMBED_CALL(cli_inspect_tree_path_internal.tree_find, CliInspectTreePathCtx, .path = args->path,
                    .size = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL})) != CLI_TREE_NONE);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_tree_lock);
    return found;
}

void cli_inspect_tree_paths(const CliInspectTreeEachCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_tree_lock);
    for (unsigned int file = 0u; file < g_cli_tree_files; file += 1u)
    {
        if (CLI_TREE_FILE(file).used)
        {
            EMBED_CALL(args->each_callback, CliEachPathCfg, .context = args->context, .path = CLI_TREE_FILE(file).path);
        }
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_tree_lock);
}

int cli_inspect_tree_function_at(const CliInspectTreePathLineCfg *args)
{
    unsigned int *at = args->at_pointer;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_tree_lock);
    const unsigned int file =
        EMBED_CALL(cli_inspect_tree_path_internal.tree_find, CliInspectTreePathCtx, .path = args->path,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}));
    int found = 0;
    unsigned int least = 0u;
    for (unsigned int index = 0u; (file != CLI_TREE_NONE) && (index < CLI_TREE_FILE(file).facts.functions); index += 1u)
    {
        const CliInspectFunction *const one = &CLI_TREE_FILE(file).facts.function[index];
        if ((one->first <= args->line) && (args->line <= one->last) && (!found || (one->last - one->first < least)))
        {
            found = 1;
            least = one->last - one->first;
            *at = index;
        }
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_tree_lock);
    return found;
}

int cli_inspect_tree_function(const CliInspectTreePathCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_tree_lock);
    const unsigned int file =
        EMBED_CALL(cli_inspect_tree_path_internal.tree_find, CliInspectTreePathCtx, .path = args->path,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}));
    const int found = (file != CLI_TREE_NONE) && (args->at < CLI_TREE_FILE(file).facts.functions);
    if (found)
    {
        EMBED_CALL(args->each, CliInspectReachedCfg, .context = args->context, .path = CLI_TREE_FILE(file).path,
                   .facts = &CLI_TREE_FILE(file).facts, .function = args->at, .sites = NULL, .count = 0u);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_tree_lock);
    return found;
}

int cli_inspect_tree_function_on(const CliInspectTreePathLineCfg *args)
{
    unsigned int *at = args->at_pointer;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_tree_lock);
    const unsigned int file =
        EMBED_CALL(cli_inspect_tree_path_internal.tree_find, CliInspectTreePathCtx, .path = args->path,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}));
    int found = 0;
    for (unsigned int index = 0u; !found && (file != CLI_TREE_NONE) && (index < CLI_TREE_FILE(file).facts.functions);
         index += 1u)
    {
        if (CLI_TREE_FILE(file).facts.function[index].from.line == args->line)
        {
            found = 1;
            *at = index;
        }
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_tree_lock);
    return found;
}

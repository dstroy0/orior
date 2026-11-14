// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_tree.c: the tree stored: each file's path and facts written into one movable cell of its own, its lists one
// after another, and the file's record in a page of records; a file found by its path through a table; and the
// function of a file that contains a line, or whose name starts on it
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
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

const CarcerMovableEntry *g_cli_tree_page[CLI_TREE_PAGES];
unsigned int g_cli_tree_files;

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
    unsigned int hash;
    const CarcerMovableEntry *cell;
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
    *at += (args->bytes + 7ULL) & ~7ULL;
    return part;
}

// the work of `size` bytes of `text` in `language` made over one cell sized to what the walk picks up: the facts are
// counted by a walk that stores nothing, the cell taken for the counts, and the walk after fills it. The cell, or NULL
// where the language is not read or the cell cannot be taken
EMBED_INLINE void *tree_work_alloc(const InspectTreeCtx *args)
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
        (counted.names * sizeof(CliInspectName)) + counted.used + (8ULL * 8ULL);
    unsigned char *const cell = g_cli_guard->temporary_buf_alloc(bytes, g_cli_inspect_owner);
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
               .imports = args->work->imports, .imports_max = counted.imports, .classes = args->work->classes,
               .classes_max = counted.classes, .functions = args->work->functions, .functions_max = counted.functions,
               .findings = args->work->findings, .findings_max = counted.findings, .fixes = args->work->fixes,
               .fixes_max = counted.fixes, .edits = args->work->edits, .edits_max = counted.edits,
               .names = args->work->names, .names_max = counted.names, .text_out = args->work->text,
               .text_max = counted.used);
    return cell;
}

// the bucket a path hashes to, FNV-1a over its bytes
EMBED_INLINE unsigned int tree_hash(const InspectTreeCtx *args)
{
    unsigned int hash = 2166136261u;
    for (unsigned long long at = 0ULL; at < args->size; at += 1u)
    {
        hash = (hash ^ (unsigned char)args->path[at]) * 16777619u;
    }
    return hash & (CLI_TREE_BUCKETS - 1u);
}

unsigned int cli_tree_find(const CliInspectTreePathCtx *args)
{
    unsigned int bucket = EMBED_CALL(tree_hash, InspectTreeCtx, .path = args->path, .size = args->size);
    unsigned int probe = 0u;
    while ((probe < CLI_TREE_BUCKETS) && (s_bucket[bucket] != 0u))
    {
        const unsigned int place = s_bucket[bucket];
        if (place != CLI_TREE_NONE)
        {
            CliTreeFile file;
            CLI_TREE_FILE_READ(place - 1u, &file);
            if ((file.path_size == args->size) &&
                (g_cli_guard->movable_buf_compare(&(CarcerMovableCfg){.cell = file.facts.cell,
                                                                      .offset = CLI_TREE_PATH_AT,
                                                                      .src = args->path,
                                                                      .bytes = (embed_word)args->size}) == 0))
            {
                return place - 1u;
            }
        }
        bucket = (bucket + 1u) & (CLI_TREE_BUCKETS - 1u);
        probe += 1u;
    }
    return CLI_TREE_NONE;
}

unsigned int cli_tree_find_join(const CliInspectTreeFolderCtx *args)
{
    char path[CLI_TREE_PATH_BYTES];
    VerbaLinea line = {path, sizeof(path), 0ULL};
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
    return (end > 0u) ? (end - 1u) : 0ULL;
}

// a file's place put in the table at the bucket its path hashes to
EMBED_INLINE void tree_bucket_append(const InspectTreeCtx *args)
{
    unsigned int bucket = args->hash;
    while ((s_bucket[bucket] != 0u) && (s_bucket[bucket] != CLI_TREE_NONE))
    {
        bucket = (bucket + 1u) & (CLI_TREE_BUCKETS - 1u);
    }
    s_bucket[bucket] = args->file + 1u;
}

// a file's place taken out of the table, from the bucket its path hashes to
EMBED_INLINE void tree_bucket_alloc(const InspectTreeCtx *args)
{
    unsigned int bucket = args->hash;
    unsigned int probe = 0u;
    while ((probe < CLI_TREE_BUCKETS) && (s_bucket[bucket] != 0u))
    {
        if (s_bucket[bucket] == args->file + 1u)
        {
            s_bucket[bucket] = CLI_TREE_NONE;
            return;
        }
        bucket = (bucket + 1u) & (CLI_TREE_BUCKETS - 1u);
        probe += 1u;
    }
}

// `size` bytes of `bytes` written into the movable cell `cell` at `*at`, which moves past them, aligned to eight
// bytes: where they are in the cell
EMBED_INLINE unsigned long long tree_write(const InspectTreeCtx *args)
{
    unsigned long long *at = args->at_u64s;
    const unsigned long long place = *at;
    if (args->size > 0u)
    {
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = args->cell, .offset = place, .src = args->bytes_bytess, .bytes = (embed_word)args->size});
    }
    *at += (args->size + 7u) & ~7ULL;
    return place;
}

// the bytes a file of `path_size` bytes of path and the facts worked out take in its cell
EMBED_INLINE unsigned long long tree_size(const InspectTreeCtx *args)
{
    unsigned long long size = (args->path_size + 1u + 7u) & ~7ULL;
    size += ((args->facts->imports * sizeof(CliInspectImport)) + 7u) & ~7ULL;
    size += ((args->facts->classes * sizeof(CliInspectClass)) + 7u) & ~7ULL;
    size += ((args->facts->functions * sizeof(CliInspectFunction)) + 7u) & ~7ULL;
    size += ((args->facts->findings * sizeof(CliInspectFinding)) + 7u) & ~7ULL;
    size += ((args->facts->fixes * sizeof(CliInspectFix)) + 7u) & ~7ULL;
    size += ((args->facts->edits * sizeof(CliInspectEdit)) + 7u) & ~7ULL;
    size += ((args->facts->names * sizeof(CliInspectName)) + 7u) & ~7ULL;
    size += (args->facts->used + 7u) & ~7ULL;
    return size;
}

// the file at `file` taken out of the tree, its facts' cell given back and its record cleared
EMBED_INLINE void tree_alloc(const InspectTreeCtx *args)
{
    CliTreeFile file;
    CLI_TREE_FILE_READ(args->file, &file);
    EMBED_CALL(tree_bucket_alloc, InspectTreeCtx, .file = args->file, .hash = file.hash);
    if (file.facts.cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(file.facts.cell, g_cli_inspect_owner);
    }
    memor.set(&(MemoriaCfg){.dst = &file, .val = 0, .bytes = sizeof(file)});
    CLI_TREE_FILE_WRITE(args->file, &file);
    cli_tree_families_free();
}

// a place among the files for a new one: its place, CLI_TREE_NONE where every place is taken
static unsigned int tree_position(void)
{
    for (unsigned int file = 0u; file < g_cli_tree_files; file += 1u)
    {
        CliTreeFile record;
        CLI_TREE_FILE_READ(file, &record);
        if (!record.used)
        {
            return file;
        }
    }
    if (g_cli_tree_files == CLI_TREE_FILES)
    {
        return CLI_TREE_NONE;
    }
    // a page taken as its first place is, each record in it cleared
    const unsigned int page = g_cli_tree_files / CLI_TREE_PAGE;
    if (g_cli_tree_page[page] == NULL)
    {
        g_cli_tree_page[page] =
            g_cli_guard->movable_buf_alloc(CLI_TREE_PAGE * sizeof(CliTreeFile), g_cli_inspect_owner);
        if (g_cli_tree_page[page] == NULL)
        {
            return CLI_TREE_NONE;
        }
        CliTreeFile cleared;
        memor.set(&(MemoriaCfg){.dst = &cleared, .val = 0, .bytes = sizeof(cleared)});
        for (unsigned int place = 0u; place < CLI_TREE_PAGE; place += 1u)
        {
            CLI_TREE_FILE_WRITE((page * CLI_TREE_PAGE) + place, &cleared);
        }
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
    const unsigned long long path_size = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES});
    if (path_size >= CLI_TREE_PATH_BYTES)
    {
        return 0;
    }
    // the facts are worked out in a cell of the call's own, sized to what they contain, before they are stored
    CliTreeWork work_copy;
    CliTreeWork *const work = &work_copy;
    void *const work_cell =
        EMBED_CALL(tree_work_alloc, InspectTreeCtx, .work = work, .language = language, .text = text, .size = size);
    if (work_cell == NULL)
    {
        return 0;
    }
    CliFacts *const facts = &work->facts;
    const int read = EMBED_CALL(cli_inspect_facts_language.facts, CliInspectFactsLanguageCfg, .language = language,
                                .text = text, .size = size, .facts = facts) &&
                     !facts->full;
    const unsigned long long needed =
        read ? EMBED_CALL(tree_size, InspectTreeCtx, .facts = facts, .path_size = path_size) : 0ULL;
    const CarcerMovableEntry *const cell = read ? g_cli_guard->movable_buf_alloc(needed, g_cli_inspect_owner) : NULL;
    const unsigned int place =
        EMBED_CALL(cli_inspect_tree_path_internal.tree_find, CliInspectTreePathCtx, .path = path, .size = path_size);
    if (place != CLI_TREE_NONE)
    {
        EMBED_CALL(tree_alloc, InspectTreeCtx, .file = place);
    }
    const unsigned int file = (cell != NULL) ? tree_position() : CLI_TREE_NONE;
    if (file == CLI_TREE_NONE)
    {
        if (cell != NULL)
        {
            (void)g_cli_guard->movable_buf_release(cell, g_cli_inspect_owner);
        }
        g_cli_guard->persistent_buf_release(work_cell, g_cli_inspect_owner);
        return 0;
    }
    CliTreeFile record;
    memor.set(&(MemoriaCfg){.dst = &record, .val = 0, .bytes = sizeof(record)});
    unsigned long long at = CLI_TREE_PATH_AT;
    record.facts.cell = cell;
    record.path_size = (unsigned int)path_size;
    record.hash = EMBED_CALL(tree_hash, InspectTreeCtx, .path = path, .size = path_size);
    record.python = (cellul.eq(&(CatenaFinitaCfg){.src = language, .other = "python", .cap = CLI_POOL_BYTES}));
    (void)EMBED_CALL(tree_write, InspectTreeCtx, .cell = cell, .at_u64s = &at, .bytes_bytess = path,
                     .size = path_size + 1u);
    record.facts.import_at =
        EMBED_CALL(tree_write, InspectTreeCtx, .cell = cell, .at_u64s = &at, .bytes_bytess = work->imports,
                   .size = facts->imports * sizeof(CliInspectImport));
    record.facts.class_at = EMBED_CALL(tree_write, InspectTreeCtx, .cell = cell, .at_u64s = &at,
                                       .bytes_bytess = work->classes, .size = facts->classes * sizeof(CliInspectClass));
    record.facts.function_at =
        EMBED_CALL(tree_write, InspectTreeCtx, .cell = cell, .at_u64s = &at, .bytes_bytess = work->functions,
                   .size = facts->functions * sizeof(CliInspectFunction));
    record.facts.finding_at =
        EMBED_CALL(tree_write, InspectTreeCtx, .cell = cell, .at_u64s = &at, .bytes_bytess = work->findings,
                   .size = facts->findings * sizeof(CliInspectFinding));
    record.facts.fix_at = EMBED_CALL(tree_write, InspectTreeCtx, .cell = cell, .at_u64s = &at,
                                     .bytes_bytess = work->fixes, .size = facts->fixes * sizeof(CliInspectFix));
    record.facts.edit_at = EMBED_CALL(tree_write, InspectTreeCtx, .cell = cell, .at_u64s = &at,
                                      .bytes_bytess = work->edits, .size = facts->edits * sizeof(CliInspectEdit));
    record.facts.name_at = EMBED_CALL(tree_write, InspectTreeCtx, .cell = cell, .at_u64s = &at,
                                      .bytes_bytess = work->names, .size = facts->names * sizeof(CliInspectName));
    record.facts.text_at = EMBED_CALL(tree_write, InspectTreeCtx, .cell = cell, .at_u64s = &at,
                                      .bytes_bytess = work->text, .size = facts->used);
    record.facts.imports = facts->imports;
    record.facts.classes = facts->classes;
    record.facts.functions = facts->functions;
    record.facts.findings = facts->findings;
    record.facts.fixes = facts->fixes;
    record.facts.edits = facts->edits;
    record.facts.names = facts->names;
    record.facts.used = facts->used;
    record.used = 1;
    CLI_TREE_FILE_WRITE(file, &record);
    EMBED_CALL(tree_bucket_append, InspectTreeCtx, .file = file, .hash = record.hash);
    cli_tree_families_free();
    g_cli_guard->persistent_buf_release(work_cell, g_cli_inspect_owner);
    return 1;
}

void cli_inspect_tree_remove(const CliInspectTreePathLineCfg *args)
{
    const unsigned int place =
        EMBED_CALL(cli_inspect_tree_path_internal.tree_find, CliInspectTreePathCtx, .path = args->path,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}));
    if (place != CLI_TREE_NONE)
    {
        EMBED_CALL(tree_alloc, InspectTreeCtx, .file = place);
    }
}

void cli_inspect_tree_clear(void)
{
    for (unsigned int file = 0u; file < g_cli_tree_files; file += 1u)
    {
        CliTreeFile record;
        CLI_TREE_FILE_READ(file, &record);
        if (record.facts.cell != NULL)
        {
            (void)g_cli_guard->movable_buf_release(record.facts.cell, g_cli_inspect_owner);
        }
    }
    for (unsigned int page = 0u; page < CLI_TREE_PAGES; page += 1u)
    {
        if (g_cli_tree_page[page] != NULL)
        {
            (void)g_cli_guard->movable_buf_release(g_cli_tree_page[page], g_cli_inspect_owner);
        }
        g_cli_tree_page[page] = NULL;
    }
    g_cli_tree_files = 0u;
    memor.set(&(MemoriaCfg){.dst = s_bucket, .val = 0, .bytes = sizeof(s_bucket)});
    cli_tree_families_free();
}

int cli_inspect_tree_contains(const CliInspectTreePathLineCfg *args)
{
    const int found =
        (EMBED_CALL(cli_inspect_tree_path_internal.tree_find, CliInspectTreePathCtx, .path = args->path,
                    .size = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES})) != CLI_TREE_NONE);
    return found;
}

void cli_inspect_tree_paths(const CliInspectTreeEachCfg *args)
{
    for (unsigned int file = 0u; file < g_cli_tree_files; file += 1u)
    {
        CliTreeFile record;
        CLI_TREE_FILE_READ(file, &record);
        if (record.used)
        {
            char path[CLI_TREE_PATH_BYTES];
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                .cell = record.facts.cell, .offset = CLI_TREE_PATH_AT, .dst = path, .bytes = record.path_size + 1ULL});
            EMBED_CALL(args->each_callback, CliEachPathCfg, .context = args->context, .path = path);
        }
    }
}

int cli_inspect_tree_function_at(const CliInspectTreePathLineCfg *args)
{
    unsigned int *at = args->at_pointer;
    const unsigned int file =
        EMBED_CALL(cli_inspect_tree_path_internal.tree_find, CliInspectTreePathCtx, .path = args->path,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}));
    if (file == CLI_TREE_NONE)
    {
        return 0;
    }
    CliTreeFile record;
    CLI_TREE_FILE_READ(file, &record);
    int found = 0;
    unsigned int min = 0u;
    for (unsigned int index = 0u; index < record.facts.functions; index += 1u)
    {
        CliInspectFunction one;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = record.facts.cell,
                                                                .offset = CLI_FACTS_FUNCTION_AT(&record.facts, index),
                                                                .dst = &one,
                                                                .bytes = sizeof(one)});
        if ((one.first <= args->line) && (args->line <= one.last) && (!found || (one.last - one.first < min)))
        {
            found = 1;
            min = one.last - one.first;
            *at = index;
        }
    }
    return found;
}

int cli_inspect_tree_function(const CliInspectTreePathCfg *args)
{
    const unsigned int file =
        EMBED_CALL(cli_inspect_tree_path_internal.tree_find, CliInspectTreePathCtx, .path = args->path,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}));
    if (file == CLI_TREE_NONE)
    {
        return 0;
    }
    CliTreeFile record;
    CLI_TREE_FILE_READ(file, &record);
    const int found = (args->at < record.facts.functions);
    if (found)
    {
        char path[CLI_TREE_PATH_BYTES];
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = record.facts.cell, .offset = CLI_TREE_PATH_AT, .dst = path, .bytes = record.path_size + 1ULL});
        EMBED_CALL(args->each, CliInspectReachedCfg, .context = args->context, .path = path, .facts = &record.facts,
                   .function = args->at, .sites = NULL, .count = 0u);
    }
    return found;
}

int cli_inspect_tree_function_on(const CliInspectTreePathLineCfg *args)
{
    unsigned int *at = args->at_pointer;
    const unsigned int file =
        EMBED_CALL(cli_inspect_tree_path_internal.tree_find, CliInspectTreePathCtx, .path = args->path,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}));
    if (file == CLI_TREE_NONE)
    {
        return 0;
    }
    CliTreeFile record;
    CLI_TREE_FILE_READ(file, &record);
    int found = 0;
    for (unsigned int index = 0u; !found && (index < record.facts.functions); index += 1u)
    {
        CliInspectFunction one;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = record.facts.cell,
                                                                .offset = CLI_FACTS_FUNCTION_AT(&record.facts, index),
                                                                .dst = &one,
                                                                .bytes = sizeof(one)});
        if (one.from.line == args->line)
        {
            found = 1;
            *at = index;
        }
    }
    return found;
}

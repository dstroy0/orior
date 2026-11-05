// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_search.c: every line in the tree's files that contains a query. The files are what git tracks or would track,
// or the whole tree where git cannot read it. Text is found by the sift: a worker on each core orior's work may take
// reads the next file into a cell of its size and searches it there; a regular expression is git's to search
#include "../code/symbols.h"
#include "../command_line/callback_cfg.h"
#include "../machine/cores.h"
#include "../text/regexp.h"
#include "../text/text.h"
#include "embed_atomic.h"
#include "files_search_hits.h"

// the bytes past which a search of a tree git cannot read passes a file over; the most workers a search starts; and
// the bytes a file's start is looked through for a 0 that marks it as no text
#define CLI_SEARCHED_BYTES (4ULL * 1024ULL * 1024ULL)
#define CLI_WORKERS_MAX 256u
#define CLI_BINARY_LOOK 8192ULL

// One search: the files searched, picked off the tree for the search's length, or standing after it with the lines,
// and the most bytes of one read, what is looked for and its letters in lower case; the next file to read and the
// workers searching, both atomic since every worker moves them; the lines each worker found, and how many stores are in
// use; each worker's thread; whether the lines stand after the search, for the window's rows to point into, and those
// lines in the order of the files, how many
typedef struct
{
    const char *root;
    CliPicked paths;
    unsigned long long most;
    CliNeedle needle;
    unsigned int lower[CLI_FUZZY_LETTERS];
    embed_atomic_u32 next;
    embed_atomic_u32 workers;
    CliWake wake;
    CliSearchHits hits[CLI_WORKERS_MAX];
    unsigned int stores;
    CliThread worker_thread[CLI_WORKERS_MAX];
    void (*each)(const CliEachHitCfg *args);
    void *context;
    int standing;
    const CliSearchHit **ordered;
    unsigned int found;
    unsigned int *file_first;
    unsigned int files;
} CliSearch;

static CliSearch s_search = {.wake = CLI_WAKE_INIT};

// A file's hits as a worker finds them: the file and the worker's store
typedef struct
{
    unsigned int file;
    CliSearchHits *hits;
} CliFileHits;

// a line found put last in the worker's store (files_search_hits.c)
static void cli_hit_append(const CliHitAppendCfg *args)
{
    const CliFileHits *const file = (const CliFileHits *)args->context;
    EMBED_CALL(cli_files_search_hits.hits_put, CliFilesSearchHitsCfg, .hits = file->hits, .file = file->file,
               .line = args->line, .col = args->col, .text = args->text, .size = args->size);
}

// a line git found put last in the first store, for the lines to stand after the search, its file found among the
// files picked by its path; a path the pick left out is passed over
static void cli_git_hit_store(const CliEachHitCfg *args)
{
    if (s_search.paths.files == NULL)
    {
        return;
    }
    const unsigned int file =
        EMBED_CALL(cli_files_path_kind.index, CliFilesPathKindCfg, .files = s_search.paths.files, .path = args->path);
    if (file < s_search.paths.count)
    {
        EMBED_CALL(cli_files_search_hits.hits_put, CliFilesSearchHitsCfg, .hits = &s_search.hits[0], .file = file,
                   .line = args->line, .col = args->col, .text = args->text, .size = args->size);
    }
}

typedef struct
{
    const unsigned char *bytes;
    unsigned long long size;
    unsigned int index;
    CliSearchHits *hits;
    const char *root;
    const CliPatterns *patterns;
    const CliPatterns *set;
    VerbaLinea *out;
} FilesSearchCtx;
// a file read searched, where its start contains no 0 that marks it as no text
EMBED_INLINE void cli_search(const FilesSearchCtx *args)
{
    const unsigned char *bytes = args->bytes;
    unsigned long long size = args->size;
    const unsigned long long look = (size < CLI_BINARY_LOOK) ? size : CLI_BINARY_LOOK;
    if ((look == 0ULL) || (memor.chr(&(MemoriaCfg){.src = bytes, .val = 0, .bytes = look}) == NULL))
    {
        CliFileHits file = {args->index, args->hits};
        EMBED_CALL(cli_files_context_internal.hits_in, CliFilesContextCtx, .bytes_bytes = bytes, .size = size,
                   .needle = &s_search.needle, .put = cli_hit_append, .context = &file);
    }
}

// a worker with `context` its store: each file left read into a cell of its size and searched, until none is left.
// Taking a file is one atomic step, so no two workers read one file and each takes its files in their order
static void cli_worker(void *context)
{
    CliSearchHits *const hits = (CliSearchHits *)context;
    for (;;)
    {
        const unsigned int index = EMBED_ATOMIC_ADD_32(&s_search.next, 1u);
        if (index >= s_search.paths.count)
        {
            break;
        }
        char full[CLI_PATH_MAX_LEN];
        VerbaLinea line = {full, sizeof(full), 0ULL};
        mmgr_line_put(&line, s_search.root);
        mmgr_line_ch(&line, '/');
        mmgr_line_put(&line, EMBED_CALL(cli_files_picked_index_internal.picked_at, CliFilesPickedIndexCtx,
                                        .picked_read = &s_search.paths, .index = index));
        unsigned long long size = 0ULL;
        if ((mmgr_line_finish(&line) != 0ULL) &&
            EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = full, .size_pointer = &size) &&
            (size <= s_search.most))
        {
            unsigned long long got = 0ULL;
            const unsigned char *const bytes =
                EMBED_CALL(cli_path_internal.file_piece_alloc, CliPathCtx, .path = full, .from = 0ULL,
                           .count_u64 = size, .size_pointer = &got, .owner = &g_cli_editor_cells);
            if (bytes != NULL)
            {
                EMBED_CALL(cli_search, FilesSearchCtx, .bytes = bytes, .size = got, .index = index, .hits = hits);
                g_cli_cells->persistent_buf_release((void *)(uintptr_t)bytes, &g_cli_editor_cells);
            }
        }
    }
    (void)EMBED_ATOMIC_ADD_32(&s_search.workers, (embed_u32)-1);
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_search.wake);
}

// The patterns a search's files are picked against: the search's own and the set it was given
typedef struct
{
    const CliPatterns *patterns;
    const CliPatterns *set;
} CliSearchHides;

// whether a search reads a file: neither the search's patterns nor the set hide it
static int cli_search_includes(const CliEachPathCfg *args)
{
    const char *path = args->path;
    const CliSearchHides *const hides = (const CliSearchHides *)args->context;
    return !EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = hides->patterns, .path = path,
                       .dir = 0) &&
           !EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = hides->set, .path = path, .dir = 0);
}

// the tree's files the search reads picked into the search for its length and sorted, less those the patterns hide;
// none where the cellblock cannot meet them. And the
// most bytes it reads of one, a tree git cannot read passing over a larger file
EMBED_INLINE void cli_search_paths(const FilesSearchCtx *args)
{
    CliSearchHides hides = {args->patterns, args->set};
    (void)EMBED_CALL(cli_files_picked_internal.pick, CliFilesPickedCtx, .picked = &s_search.paths, .root = args->root,
                     .filter = cli_search_includes, .context = &hides, .standing = s_search.standing);
    if (s_search.paths.files != NULL)
    {
        (void)EMBED_CALL(cli_files_bytes_internal.sort, CliFilesBytesCtx, .files = s_search.paths.files);
        s_search.paths.count = s_search.paths.files->count;
    }
    s_search.most = cli_tree_git() ? ~0ULL : CLI_SEARCHED_BYTES;
}

EMBED_INLINE void cli_search_ordered(const FilesSearchCtx *args);

// the files read and searched, a worker on each core the work may take, and the lines found written
EMBED_INLINE void cli_search_run(const FilesSearchCtx *args)
{
    VerbaLinea *out = args->out;
    const unsigned int budget = cli_cores_for_work();
    const unsigned int files = (s_search.paths.count > 1u) ? s_search.paths.count : 1u;
    unsigned int workers = (budget < 1u) ? 1u : budget;
    workers = (workers > files) ? files : workers;
    workers = (workers > CLI_WORKERS_MAX) ? CLI_WORKERS_MAX : workers;
    unsigned int started = 0u;
    for (unsigned int at = 0u; at < workers; at += 1u)
    {
        (void)EMBED_ATOMIC_ADD_32(&s_search.workers, 1u);
        s_search.hits[started].standing = s_search.standing;
        s_search.worker_thread[started] = (CliThread){cli_worker, &s_search.hits[started], CLI_PRIORITY_WORK};
        if (EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_search.worker_thread[started]))
        {
            started += 1u;
        }
        else
        {
            (void)EMBED_ATOMIC_ADD_32(&s_search.workers, (embed_u32)-1);
        }
    }
    if (started == 0u)
    {
        // with no worker started, the files are read and searched here, one at a time
        (void)EMBED_ATOMIC_ADD_32(&s_search.workers, 1u);
        s_search.hits[0].standing = s_search.standing;
        cli_worker(&s_search.hits[0]);
        started = 1u;
    }
    s_search.stores = started;
    while (EMBED_ATOMIC_LOAD(&s_search.workers) > 0u)
    {
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_search.wake);
    }
    EMBED_CALL(cli_search_ordered, FilesSearchCtx, .out = out);
}

// the stores' lines read back in the order of the files: each written, or handed on, where the lines do not stand
// after the search, and each stored in that order where they do, with each file's first line
EMBED_INLINE void cli_search_ordered(const FilesSearchCtx *args)
{
    VerbaLinea *out = args->out;
    s_search.found = 0u;
    s_search.files = 0u;
    if (s_search.standing)
    {
        // one cell for the lines standing after the search, each line's place and each file's first line, sized to
        // the lines the stores have
        unsigned int count = 0u;
        for (unsigned int store = 0u; store < s_search.stores; store += 1u)
        {
            count += s_search.hits[store].lives;
        }
        count = (count < CLI_HITS_MAX) ? count : CLI_HITS_MAX;
        s_search.ordered = g_cli_cells->persistent_buf_alloc(
            (unsigned long long)count * (sizeof(const CliSearchHit *) + sizeof(unsigned int)), &g_cli_search_cells);
        s_search.file_first = (s_search.ordered != NULL) ? (unsigned int *)(void *)(s_search.ordered + count) : NULL;
    }
    if ((out != NULL) && !s_search.standing)
    {
        mmgr_line_ch(out, '[');
    }
    unsigned int file = 0u;
    for (unsigned int at = 0u; at < CLI_HITS_MAX; at += 1u)
    {
        const CliSearchHit *const hit = EMBED_CALL(cli_files_search_hits.hits_next, CliFilesSearchHitsCfg,
                                                   .hits = s_search.hits, .stores = s_search.stores);
        if (hit == NULL)
        {
            break;
        }
        if (s_search.standing)
        {
            if (s_search.ordered != NULL)
            {
                if ((at == 0u) || (hit->file != file))
                {
                    s_search.file_first[s_search.files] = at;
                    s_search.files += 1u;
                    file = hit->file;
                }
                s_search.ordered[at] = hit;
                s_search.found = at + 1u;
            }
        }
        else
        {
            EMBED_CALL(cli_files_path_internal.hit_write, CliFilesPathCtx, .out = out, .first = at == 0u,
                       .path = EMBED_CALL(cli_files_picked_index_internal.picked_at, CliFilesPickedIndexCtx,
                                          .picked_read = &s_search.paths, .index = hit->file),
                       .line = hit->line, .col = hit->col, .text = hit->text, .size = hit->size, .each = s_search.each,
                       .context = s_search.context);
        }
    }
    if ((out != NULL) && !s_search.standing)
    {
        mmgr_line_ch(out, ']');
    }
}

unsigned int cli_search_standing_count(void)
{
    return s_search.standing ? s_search.found : 0u;
}

const CliSearchHit *cli_search_standing_hit(unsigned int at)
{
    return (s_search.standing && (at < s_search.found)) ? s_search.ordered[at] : NULL;
}

unsigned int cli_search_standing_files(void)
{
    return s_search.standing ? s_search.files : 0u;
}

unsigned int cli_search_standing_file_first(unsigned int file)
{
    return (s_search.standing && (file < s_search.files)) ? s_search.file_first[file] : s_search.found;
}

const char *cli_search_standing_path(const CliSearchHit *hit)
{
    return EMBED_CALL(cli_files_picked_index_internal.picked_at, CliFilesPickedIndexCtx, .picked_read = &s_search.paths,
                      .index = hit->file);
}

void cli_search_standing_free(void)
{
    for (unsigned int store = 0u; store < s_search.stores; store += 1u)
    {
        CliHitCell *cell = s_search.hits[store].cell_first;
        while (cell != NULL)
        {
            CliHitCell *const next = cell->next;
            g_cli_cells->persistent_buf_release(cell, &g_cli_search_cells);
            cell = next;
        }
        s_search.hits[store] = (CliSearchHits){NULL, NULL, 0u, NULL, NULL, 0};
    }
    EMBED_CALL(cli_files_picked_internal.picked_let, CliFilesPickedCtx, .picked = &s_search.paths);
    g_cli_cells->persistent_buf_release(s_search.ordered, &g_cli_search_cells);
    s_search.ordered = NULL;
    s_search.file_first = NULL;
    s_search.found = 0u;
    s_search.files = 0u;
    s_search.standing = 0;
}

int cli_files_search(const CliFilesRootOutErrMsgCfg *args)
{
    const char *root = args->root;
    static CliPatterns s_patterns;
    static CliWords s_specs;
    // the lines left standing by the last search go before the next
    cli_search_standing_free();
    s_search.each = args->each;
    s_search.context = args->context;
    s_search.standing = args->standing;
    if (args->query[0] == '\0')
    {
        if (args->out != NULL)
        {
            mmgr_line_put(args->out, "[]");
        }
        return 1;
    }
    EMBED_CALL(cli_patterns_part.get, CliPatternsPartCfg, .part = CLI_PART_SEARCH, .patterns_out = &s_patterns);
    s_specs.count = 0u;
    s_specs.used = 0u;
    s_specs.full = 0;
    EMBED_CALL(cli_patterns.include_specs, CliPatternsCfg, .patterns = args->set, .words = &s_specs);
    if (s_specs.count == 0u)
    {
        EMBED_CALL(cli_patterns.include_specs, CliPatternsCfg, .patterns = &s_patterns, .words = &s_specs);
    }
    if (s_specs.count == 0u)
    {
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_specs,
                   .word_text = ".");
    }
    EMBED_CALL(cli_patterns.hidden_specs, CliPatternsCfg, .patterns = &s_patterns, .words = &s_specs);
    EMBED_CALL(cli_patterns.hidden_specs, CliPatternsCfg, .patterns = args->set, .words = &s_specs);
    s_search.root = root;
    if (args->how.regex)
    {
        // the lines git finds stand after the search where asked: the files picked first, so each line finds its file
        // among them, and git's lines put in the first store and read back in the files' order
        if (s_search.standing)
        {
            EMBED_CALL(cli_search_paths, FilesSearchCtx, .root = root, .patterns = &s_patterns, .set = args->set);
            s_search.hits[0].standing = 1;
            s_search.stores = 1u;
        }
        const int received =
            EMBED_CALL(cli_files_root_internal.search_by_git, CliFilesRootCtx, .root = root, .query = args->query,
                       .how = args->how, .specs = &s_specs, .patterns = &s_patterns, .set = args->set,
                       .out = s_search.standing ? NULL : args->out, .err_msg = args->err_msg,
                       .each = s_search.standing ? cli_git_hit_store : args->each, .context = args->context);
        if (received != CLI_GIT_SEARCH_NONE)
        {
            if (s_search.standing)
            {
                EMBED_CALL(cli_search_ordered, FilesSearchCtx, .out = NULL);
            }
            return received == CLI_GIT_SEARCH_FOUND;
        }
        if (s_search.standing)
        {
            cli_search_standing_free();
            s_search.standing = args->standing;
        }
    }
    EMBED_CALL(cli_search_paths, FilesSearchCtx, .root = root, .patterns = &s_patterns, .set = args->set);
    // a letter count is below CLI_FUZZY_LETTERS, which an unsigned int stores
    const unsigned int letters =
        (unsigned int)EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = args->query,
                                 .size = cellul.len(&(CatenaFinitaCfg){.src = args->query, .cap = CLI_CELLS_POOL}),
                                 .letters = s_search.lower, .capacity_u64 = CLI_FUZZY_LETTERS);
    int ascii = 1;
    ascii = ascii && EMBED_CALL(cli_text_size_byte.ascii, CliTextSizeByteCfg, .text = args->query,
                                .size = cellul.len(&(CatenaFinitaCfg){.src = args->query, .cap = CLI_CELLS_POOL}));
    for (unsigned int at = 0u; at < letters; at += 1u)
    {
        s_search.lower[at] = EMBED_CALL(cli_regexp_letter.lower, CliRegexpLetterCfg, .letter = s_search.lower[at]);
    }
    s_search.needle =
        (CliNeedle){args->query, cellul.len(&(CatenaFinitaCfg){.src = args->query, .cap = CLI_CELLS_POOL}),
                    ascii,       s_search.lower,
                    letters,     args->how};
    EMBED_ATOMIC_STORE(&s_search.next, 0u);
    EMBED_ATOMIC_STORE(&s_search.workers, 0u);
    EMBED_CALL(cli_search_run, FilesSearchCtx, .out = args->out);
    if (!s_search.standing)
    {
        for (unsigned int store = 0u; store < s_search.stores; store += 1u)
        {
            CliHitCell *cell = s_search.hits[store].cell_first;
            while (cell != NULL)
            {
                CliHitCell *const next = cell->next;
                g_cli_cells->persistent_buf_release(cell, &g_cli_search_cells);
                cell = next;
            }
            s_search.hits[store] = (CliSearchHits){NULL, NULL, 0u, NULL, NULL, 0};
        }
        EMBED_CALL(cli_files_picked_internal.picked_let, CliFilesPickedCtx, .picked = &s_search.paths);
    }
    return 1;
}

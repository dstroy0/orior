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
#include "files_search_hits.h"

// the bytes past which a search of a tree git cannot read passes a file over; the most workers a search starts; and
// the bytes a file's start is looked through for a 0 that marks it as no text
#define CLI_SEARCHED_BYTES (4ULL * 1024ULL * 1024ULL)
#define CLI_WORKERS_MAX 256u
#define CLI_BINARY_LOOK 8192ULL

// One search: the files searched, picked off the tree for the search's length, and the most bytes of one
// read, what is looked for and its letters in lower case; the next file to read; the workers searching; the lines
// found; and each worker's thread
typedef struct
{
    const char *root;
    CliPicked paths;
    unsigned long long most;
    CliNeedle needle;
    unsigned int lower[CLI_FUZZY_LETTERS];
    unsigned int next;
    unsigned int workers;
    CliWake wake;
    CliSearchHits hits;
    CliThread worker_thread[CLI_WORKERS_MAX];
} CliSearch;

static CliSearch s_search = {.wake = CLI_WAKE_INIT};

// A file's hits as a worker finds them: the file and the hits it has given
typedef struct
{
    unsigned int file;
    unsigned int order;
} CliFileHits;

// a line found put among those stored (files_search_hits.c); where no capacity for it is given it is let go
static void cli_hit_append(const CliHitAppendCfg *args)
{
    CliFileHits *const file = (CliFileHits *)args->context;
    CliSearchHit *const hit =
        EMBED_CALL(cli_files_search_hits.hits_alloc, CliFilesSearchHitsCfg, .hits = &s_search.hits);
    if (hit != NULL)
    {
        // a hit's bytes are at most CLI_HIT_BYTES, which an unsigned int stores
        const unsigned int copied_len = (unsigned int)((args->size < CLI_HIT_BYTES) ? args->size : CLI_HIT_BYTES);
        *hit = (CliSearchHit){file->file, file->order, args->line, args->col, copied_len, {0}};
        memor.cpy(&(MemoriaCfg){.dst = hit->text, .src = args->text, .bytes = copied_len});
    }
    file->order += 1u;
}

typedef struct
{
    const unsigned char *bytes;
    unsigned long long size;
    unsigned int index;
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
        CliFileHits file = {args->index, 0u};
        EMBED_CALL(cli_files_context_internal.hits_in, CliFilesContextCtx, .bytes_bytes = bytes, .size = size,
                   .needle = &s_search.needle, .put = cli_hit_append, .context = &file);
    }
}

// a worker: each file left read into a cell of its size and searched, until none is left
static void cli_worker(void *context)
{
    (void)context;
    for (;;)
    {
        const unsigned int index = s_search.next;
        s_search.next += (index < s_search.paths.count) ? 1u : 0u;
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
                EMBED_CALL(cli_search, FilesSearchCtx, .bytes = bytes, .size = got, .index = index);
                EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes,
                           .owner = &g_cli_editor_cells);
            }
        }
    }
    s_search.workers -= 1u;
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
                     .filter = cli_search_includes, .context = &hides);
    if (s_search.paths.files != NULL)
    {
        (void)EMBED_CALL(cli_files_bytes_internal.sort, CliFilesBytesCtx, .files = s_search.paths.files);
        s_search.paths.count = s_search.paths.files->count;
    }
    s_search.most = cli_tree_git() ? ~0ULL : CLI_SEARCHED_BYTES;
}

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
        s_search.workers += 1u;
        s_search.worker_thread[started] = (CliThread){cli_worker, NULL, CLI_PRIORITY_WORK};
        const int one =
            EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_search.worker_thread[started]);
        started += one ? 1u : 0u;
        s_search.workers -= one ? 0u : 1u;
    }
    if (started == 0u)
    {
        // with no worker started, the files are read and searched here, one at a time
        s_search.workers = 1u;
        cli_worker(NULL);
    }
    while (s_search.workers > 0u)
    {
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_search.wake);
    }
    CliSearchHits *const hits = &s_search.hits;
    EMBED_CALL(cli_files_search_hits.hits_sort, CliFilesSearchHitsCfg, .hits = hits);
    const unsigned int displayed = (hits->lives < CLI_HITS_MAX) ? hits->lives : CLI_HITS_MAX;
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < displayed; at += 1u)
    {
        const CliSearchHit *const hit = &hits->pool[hits->live[at]];
        EMBED_CALL(cli_files_path_internal.hit_write, CliFilesPathCtx, .out = out, .first = at == 0u,
                   .path = EMBED_CALL(cli_files_picked_index_internal.picked_at, CliFilesPickedIndexCtx,
                                      .picked_read = &s_search.paths, .index = hit->file),
                   .line = hit->line, .col = hit->col, .text = hit->text, .size = hit->size);
    }
    mmgr_line_ch(out, ']');
}

int cli_files_search(const CliFilesRootOutErrMsgCfg *args)
{
    const char *root = args->root;
    static CliPatterns s_patterns;
    static CliWords s_specs;
    if (args->query[0] == '\0')
    {
        mmgr_line_put(args->out, "[]");
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
    if (args->how.regex)
    {
        const int received =
            EMBED_CALL(cli_files_root_internal.search_by_git, CliFilesRootCtx, .root = root, .query = args->query,
                       .how = args->how, .specs = &s_specs, .patterns = &s_patterns, .set = args->set, .out = args->out,
                       .err_msg = args->err_msg);
        if (received != CLI_GIT_SEARCH_NONE)
        {
            return received == CLI_GIT_SEARCH_FOUND;
        }
    }
    s_search.root = root;
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
    s_search.next = 0u;
    s_search.workers = 0u;
    EMBED_CALL(cli_files_search_hits.hits_init, CliFilesSearchHitsCfg, .hits = &s_search.hits);
    EMBED_CALL(cli_search_run, FilesSearchCtx, .out = args->out);
    EMBED_CALL(cli_files_search_hits.hits_free, CliFilesSearchHitsCfg, .hits = &s_search.hits);
    EMBED_CALL(cli_files_picked_internal.picked_let, CliFilesPickedCtx, .picked = &s_search.paths);
    return 1;
}

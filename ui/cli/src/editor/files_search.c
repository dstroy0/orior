// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_search.c: every line in the tree's files that holds a query. The files are what git tracks or would track, or
// the whole tree where git cannot read it. Text is found by the sift: a worker on each core orior's work may take
// reads the next file into a cell of its own, grown as a larger file comes, and searches it there, so a search takes
// one cell a worker and not two a file; a regular expression is git's to search
#include "../machine/cores.h"
#include "../text/regexp.h"
#include "files_search_hits.h"

// the bytes past which a search of a tree git cannot read passes a file over; the most workers a search starts; the
// bytes a worker's cell is first taken with, doubled as a larger file comes; and the bytes a file's start is looked
// through for a 0 that marks it as no text
#define CLI_SEARCHED_BYTES (4ull * 1024ull * 1024ull)
#define CLI_WORKERS_MOST 256u
#define CLI_WORKER_ROOM_FIRST (256ull * 1024ull)
#define CLI_BINARY_LOOK 8192ull

// One worker: the cell it reads each file into and that cell's bytes
typedef struct
{
    unsigned char *room;
    unsigned long long room_size;
} CliSearchWorker;

// One search: the files searched, picked from the view of the tree held through the search, and the most bytes of one
// read, what is looked for and its letters in lower case; the next file to read; the workers searching; the lines
// found; and each worker's thread and cell
typedef struct
{
    const char *root;
    CliPicked paths;
    unsigned long long most;
    CliSought sought;
    unsigned int lower[CLI_FUZZY_LETTERS];
    unsigned int next;
    unsigned int workers;
    CliWake wake;
    CliSearchHits hits;
    CliThread worker_thread[CLI_WORKERS_MOST];
    CliSearchWorker worker[CLI_WORKERS_MOST];
} CliSearch;

static CliSearch s_search = {.wake = CLI_WAKE_INIT};
static CliLock s_lock = CLI_LOCK_INIT;

// A file's hits as a worker finds them: the file and the hits it has given
typedef struct
{
    unsigned int file;
    unsigned int order;
} CliFileHits;

// a line found put among those held (files_search_hits.c); where no room for it is given it is let go
static void cli_hit_put(void *context, unsigned long long line, unsigned long long col, const unsigned char *text,
                        unsigned long long size)
{
    CliFileHits *const file = (CliFileHits *)context;
    cli_lock_take(&s_lock);
    CliHeldHit *const hit = cli_hits_slot(&s_search.hits);
    if (hit != NULL)
    {
        // a hit's bytes are at most CLI_HIT_BYTES, which an unsigned int holds
        const unsigned int kept = (unsigned int)((size < CLI_HIT_BYTES) ? size : CLI_HIT_BYTES);
        *hit = (CliHeldHit){file->file, file->order, line, col, kept, {0}};
        memor.cpy(&(MemoriaCfg){.dst = hit->text, .src = text, .bytes = kept});
    }
    file->order += 1u;
    cli_lock_give(&s_lock);
}

// a file read searched, where its start holds no 0 that marks it as no text
static void cli_searched(const unsigned char *bytes, unsigned long long size, unsigned int index)
{
    const unsigned long long look = (size < CLI_BINARY_LOOK) ? size : CLI_BINARY_LOOK;
    if ((look == 0ull) || (memor.chr(&(MemoriaCfg){.src = bytes, .val = 0, .bytes = look}) == NULL))
    {
        CliFileHits file = {index, 0u};
        cli_files_hits_in(bytes, size, &s_search.sought, cli_hit_put, &file);
    }
}

// room for a file of `size` bytes in a worker's cell: the cell as it is where it holds them, else one taken anew of
// twice the cell's bytes or the file's and one, the cell before given back; NULL, so the file is passed over, where it
// is larger than the search reads or no cell is given
static unsigned char *cli_worker_roomed(void *context, unsigned long long size)
{
    CliSearchWorker *const worker = context;
    if (size > s_search.most)
    {
        return NULL;
    }
    if ((size + 1ull) > worker->room_size)
    {
        cli_cell_given(worker->room);
        unsigned long long room = (worker->room_size * 2ull > size + 1ull) ? (worker->room_size * 2ull) : (size + 1ull);
        room = (room < CLI_WORKER_ROOM_FIRST) ? CLI_WORKER_ROOM_FIRST : room;
        worker->room = cli_cell_taken(room, "a search worker's room");
        worker->room_size = (worker->room != NULL) ? room : 0ull;
    }
    return worker->room;
}

// a worker: each file left read into its cell and searched, until none is left; its cell given back as it ends
static void cli_worker(void *context)
{
    CliSearchWorker *const worker = context;
    for (;;)
    {
        cli_lock_take(&s_lock);
        const unsigned int index = s_search.next;
        s_search.next += (index < s_search.paths.count) ? 1u : 0u;
        cli_lock_give(&s_lock);
        if (index >= s_search.paths.count)
        {
            break;
        }
        char full[CLI_PATH_LONGEST];
        CliLine line = {full, sizeof(full), 0ull};
        cli_line_text(&line, s_search.root);
        cli_line_character(&line, '/');
        cli_line_text(&line, cli_picked_at(&s_search.paths, index));
        unsigned long long size = 0ull;
        if ((cli_line_closed(&line) != 0ull) && cli_file_read_whole(full, cli_worker_roomed, worker, &size) &&
            (worker->room != NULL))
        {
            cli_searched(worker->room, size, index);
        }
    }
    cli_cell_given(worker->room);
    worker->room = NULL;
    worker->room_size = 0ull;
    cli_lock_take(&s_lock);
    s_search.workers -= 1u;
    cli_wake_all(&s_search.wake);
    cli_lock_give(&s_lock);
}

// The patterns a search's files are picked against: the search's own and the set it was given
typedef struct
{
    const CliPatterns *patterns;
    const CliPatterns *set;
} CliSearchHides;

// whether a search reads a file: neither the search's patterns nor the set hide it
static int cli_search_keeps(void *context, const char *path)
{
    const CliSearchHides *const hides = (const CliSearchHides *)context;
    return !cli_patterns_hides(hides->patterns, path, 0) && !cli_patterns_hides(hides->set, path, 0);
}

// the tree's files the search reads picked into the search, the view held until the search ends: git's view, or the
// tree's own files where git cannot read it, less those the patterns hide; none where no view could be held. And the
// most bytes it reads of one, a tree git cannot read passing over a larger file
static void cli_search_paths(const char *root, const CliPatterns *patterns, const CliPatterns *set)
{
    CliSearchHides hides = {patterns, set};
    (void)cli_files_picked(&s_search.paths, root, cli_search_keeps, &hides);
    s_search.most = ((s_search.paths.seen != NULL) && s_search.paths.seen->git) ? ~0ull : CLI_SEARCHED_BYTES;
}

// the files read and searched, a worker on each core the work may take, and the lines found written
static void cli_search_run(CliLine *out)
{
    const unsigned int budget = cli_cores_for_work();
    const unsigned int files = (s_search.paths.count > 1u) ? s_search.paths.count : 1u;
    unsigned int workers = (budget < 1u) ? 1u : budget;
    workers = (workers > files) ? files : workers;
    workers = (workers > CLI_WORKERS_MOST) ? CLI_WORKERS_MOST : workers;
    unsigned int started = 0u;
    for (unsigned int at = 0u; at < workers; at += 1u)
    {
        cli_lock_take(&s_lock);
        s_search.workers += 1u;
        cli_lock_give(&s_lock);
        s_search.worker_thread[started] = (CliThread){cli_worker, &s_search.worker[started], CLI_PRIORITY_WORK};
        const int one = cli_thread_started(&s_search.worker_thread[started]);
        started += one ? 1u : 0u;
        cli_lock_take(&s_lock);
        s_search.workers -= one ? 0u : 1u;
        cli_lock_give(&s_lock);
    }
    if (started == 0u)
    {
        // with no worker started, the files are read and searched here, one at a time
        s_search.workers = 1u;
        cli_worker(&s_search.worker[0]);
    }
    cli_lock_take(&s_lock);
    while (s_search.workers > 0u)
    {
        cli_wake_wait(&s_search.wake, &s_lock);
    }
    cli_lock_give(&s_lock);
    CliSearchHits *const hits = &s_search.hits;
    cli_hits_sorted(hits);
    const unsigned int shown = (hits->lives < CLI_HITS_MOST) ? hits->lives : CLI_HITS_MOST;
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < shown; at += 1u)
    {
        const CliHeldHit *const hit = &hits->pool[hits->live[at]];
        cli_hit_written(out, at == 0u, cli_picked_at(&s_search.paths, hit->file), hit->line, hit->col, hit->text,
                        hit->size);
    }
    cli_line_character(out, ']');
}

int cli_files_search(const char *root, const char *query, CliSearching how, const CliPatterns *set, CliLine *out,
                     CliLine *said)
{
    static CliPatterns s_patterns;
    static CliWords s_specs;
    static CliLock s_turn = CLI_LOCK_INIT;
    if (query[0] == '\0')
    {
        cli_line_text(out, "[]");
        return 1;
    }
    cli_lock_take(&s_turn);
    cli_patterns_of(CLI_PART_SEARCH, &s_patterns);
    s_specs.count = 0u;
    s_specs.used = 0u;
    s_specs.full = 0;
    cli_patterns_kept_specs(set, &s_specs);
    if (s_specs.count == 0u)
    {
        cli_patterns_kept_specs(&s_patterns, &s_specs);
    }
    if (s_specs.count == 0u)
    {
        cli_words_put(&s_specs, ".");
    }
    cli_patterns_hidden_specs(&s_patterns, &s_specs);
    cli_patterns_hidden_specs(set, &s_specs);
    if (how.regex)
    {
        const int came = cli_search_by_git(root, query, how, &s_specs, &s_patterns, set, out, said);
        if (came != CLI_GIT_SEARCH_NONE)
        {
            cli_lock_give(&s_turn);
            return came == CLI_GIT_SEARCH_FOUND;
        }
    }
    s_search.root = root;
    cli_search_paths(root, &s_patterns, set);
    // a letter count is below CLI_FUZZY_LETTERS, which an unsigned int holds
    const unsigned int letters = (unsigned int)cli_regexp_letters(
        query, cellul.len(&(CatenaFinitaCfg){.src = query, .cap = CLI_CELLS_POOL}), s_search.lower, CLI_FUZZY_LETTERS);
    int ascii = 1;
    for (const char *at = query; at[0] != '\0'; at += 1)
    {
        ascii = ascii && ((unsigned char)at[0] < 0x80u);
    }
    for (unsigned int at = 0u; at < letters; at += 1u)
    {
        s_search.lower[at] = cli_regexp_lower(s_search.lower[at]);
    }
    s_search.sought = (CliSought){query,   cellul.len(&(CatenaFinitaCfg){.src = query, .cap = CLI_CELLS_POOL}),
                                  ascii,   s_search.lower,
                                  letters, how};
    s_search.next = 0u;
    s_search.workers = 0u;
    cli_hits_given(&s_search.hits);
    cli_search_run(out);
    cli_hits_given(&s_search.hits);
    cli_picked_let(&s_search.paths);
    cli_lock_give(&s_turn);
    return 1;
}

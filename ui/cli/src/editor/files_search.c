// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_search.c: every line in the tree's files that holds a query. The files are what git tracks or would track, or
// the whole tree where git cannot read it. Text is found by the sift: one reader reads the files, with as many heads as
// read faster, and a worker on each other core orior's work may take searches the next file read; a regular
// expression is git's to search
#include "../machine/cores.h"
#include "../text/regexp.h"
#include "files_internal.h"

// the most heads a search's reader reads with; the files read between two looks at whether another head reads faster;
// how much faster the reads must come since the head started last for the reader to start another; the files a search's
// reader reads ahead of each worker; and the bytes past which a search of a tree git cannot read passes a file over
#define CLI_HEADS 8u
#define CLI_HEAD_SPAN 64ull
#define CLI_HEAD_GAIN 1.1
#define CLI_READ_AHEAD 2u
#define CLI_SEARCHED_BYTES (4ull * 1024ull * 1024ull)

// the most workers a search starts, the most hits held while it runs, and the bytes a file's start is looked through
// for a 0 that marks it as no text
#define CLI_WORKERS_MOST 256u
#define CLI_POOL_MOST (CLI_HITS_MOST * 4u)
#define CLI_BINARY_LOOK 8192ull

// A file read and handed to a worker: its place among the files and its bytes
typedef struct
{
    unsigned int index;
    const unsigned char *bytes;
    unsigned long long size;
} CliQueued;

// A line found, held until the search ends: its file's place, its place among its file's, the line and column, and its
// text's bytes
typedef struct
{
    unsigned int file;
    unsigned int order;
    unsigned long long line;
    unsigned long long col;
    unsigned int size;
    unsigned char text[CLI_HIT_BYTES];
} CliHeldHit;

// One search: the files searched, picked from the view of the tree held through the search, and the most bytes of one
// read, what is looked for and its letters in lower case;
// the next file to read and the files read; the files read and not yet taken, `queued` of them from `head`, at most
// `room`; the heads reading and whether the reader starts no more; the workers searching; the lines found, each slot
// of `live` in use, and the free slots
typedef struct
{
    const char *root;
    CliPicked paths;
    unsigned long long most;
    CliSought sought;
    unsigned int lower[CLI_FUZZY_LETTERS];
    unsigned int next;
    unsigned int read;
    CliQueued queue[CLI_WORKERS_MOST * CLI_READ_AHEAD];
    unsigned int head;
    unsigned int queued;
    unsigned int room;
    unsigned int heads;
    int heads_ended;
    unsigned int workers;
    CliWake wake;
    CliHeldHit pool[CLI_POOL_MOST];
    unsigned int live[CLI_POOL_MOST];
    unsigned int lives;
    unsigned int spare[CLI_POOL_MOST];
    unsigned int free[CLI_POOL_MOST];
    unsigned int frees;
    CliThread head_thread[CLI_HEADS];
    CliThread worker_thread[CLI_WORKERS_MOST];
} CliSearch;

static CliSearch s_search = {.wake = CLI_WAKE_INIT};
static CliLock s_lock = CLI_LOCK_INIT;

// whether the hit in slot `one` comes before the hit in slot `other`: by its file, then its place in it
static int cli_hit_before(unsigned int one, unsigned int other)
{
    const CliHeldHit *const left = &s_search.pool[one];
    const CliHeldHit *const right = &s_search.pool[other];
    return (left->file < right->file) || ((left->file == right->file) && (left->order < right->order));
}

// the live slots sorted by their hits' order, the lock held
static void cli_live_sorted(void)
{
    unsigned int *from = s_search.live;
    unsigned int *into = s_search.spare;
    const unsigned int count = s_search.lives;
    for (unsigned int width = 1u; width < count; width *= 2u)
    {
        for (unsigned int start = 0u; start < count; start += 2u * width)
        {
            const unsigned int middle = ((start + width) < count) ? (start + width) : count;
            const unsigned int end = ((middle + width) < count) ? (middle + width) : count;
            unsigned int left = start;
            unsigned int right = middle;
            for (unsigned int at = start; at < end; at += 1u)
            {
                const int take_left = (left < middle) && ((right >= end) || !cli_hit_before(from[right], from[left]));
                into[at] = take_left ? from[left] : from[right];
                left += take_left ? 1u : 0u;
                right += take_left ? 0u : 1u;
            }
        }
        unsigned int *const turned = from;
        from = into;
        into = turned;
    }
    if (from != s_search.live)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_search.live, .src = from, .bytes = count * sizeof(unsigned int)});
    }
}

// A file's hits as a worker finds them: the file and the hits it has given
typedef struct
{
    unsigned int file;
    unsigned int order;
} CliFileHits;

// a line found put among those held. Where they are full, those that come after the first CLI_HITS_MOST are let go:
// no line found later can bring one of them back among the first
static void cli_hit_put(void *context, unsigned long long line, unsigned long long col, const unsigned char *text,
                        unsigned long long size)
{
    CliFileHits *const file = (CliFileHits *)context;
    cli_lock_take(&s_lock);
    if (s_search.lives == CLI_POOL_MOST)
    {
        cli_live_sorted();
        for (unsigned int at = CLI_HITS_MOST; at < s_search.lives; at += 1u)
        {
            s_search.free[s_search.frees] = s_search.live[at];
            s_search.frees += 1u;
        }
        s_search.lives = CLI_HITS_MOST;
    }
    s_search.frees -= 1u;
    const unsigned int slot = s_search.free[s_search.frees];
    CliHeldHit *const hit = &s_search.pool[slot];
    // a hit's bytes are at most CLI_HIT_BYTES, which an unsigned int holds
    const unsigned int kept = (unsigned int)((size < CLI_HIT_BYTES) ? size : CLI_HIT_BYTES);
    *hit = (CliHeldHit){file->file, file->order, line, col, kept, {0}};
    memor.cpy(&(MemoriaCfg){.dst = hit->text, .src = text, .bytes = kept});
    s_search.live[s_search.lives] = slot;
    s_search.lives += 1u;
    file->order += 1u;
    cli_lock_give(&s_lock);
}

// one file searched, its bytes let go after
static void cli_searched(const CliQueued *queued)
{
    const unsigned long long look = (queued->size < CLI_BINARY_LOOK) ? queued->size : CLI_BINARY_LOOK;
    if ((look == 0ull) || (memor.chr(&(MemoriaCfg){.src = queued->bytes, .val = 0, .bytes = look}) == NULL))
    {
        CliFileHits file = {queued->index, 0u};
        cli_files_hits_in(queued->bytes, queued->size, &s_search.sought, cli_hit_put, &file);
    }
    qry_member_file_unmapped(queued->bytes, queued->size);
}

// the next file read from the disk: 1 with it into `queued`, or 0 where none is left. A file too large or that cannot
// be read is passed over
static int cli_file_read(CliQueued *queued)
{
    for (;;)
    {
        cli_lock_take(&s_lock);
        const unsigned int index = s_search.next;
        s_search.next += (index < s_search.paths.count) ? 1u : 0u;
        cli_lock_give(&s_lock);
        if (index >= s_search.paths.count)
        {
            return 0;
        }
        char full[CLI_PATH_LONGEST];
        CliLine line = {full, sizeof(full), 0ull};
        cli_line_text(&line, s_search.root);
        cli_line_character(&line, '/');
        cli_line_text(&line, cli_picked_at(&s_search.paths, index));
        unsigned long long size = 0ull;
        const unsigned char *const bytes =
            (cli_line_closed(&line) != 0ull) ? qry_member_file_mapped(full, &size) : NULL;
        cli_lock_take(&s_lock);
        s_search.read += 1u;
        cli_lock_give(&s_lock);
        if ((bytes != NULL) && (size > s_search.most))
        {
            qry_member_file_unmapped(bytes, size);
            continue;
        }
        if (bytes != NULL)
        {
            *queued = (CliQueued){index, bytes, size};
            return 1;
        }
    }
}

// a head: the files read and handed to the workers, waiting while the queue is full
static void cli_head(void *context)
{
    (void)context;
    CliQueued queued;
    while (cli_file_read(&queued))
    {
        cli_lock_take(&s_lock);
        while (s_search.queued == s_search.room)
        {
            cli_wake_wait(&s_search.wake, &s_lock);
        }
        s_search.queue[(s_search.head + s_search.queued) % s_search.room] = queued;
        s_search.queued += 1u;
        cli_wake_all(&s_search.wake);
        cli_lock_give(&s_lock);
    }
    cli_lock_take(&s_lock);
    s_search.heads -= 1u;
    cli_wake_all(&s_search.wake);
    cli_lock_give(&s_lock);
}

// a worker: each file handed it searched, until the heads have ended and none is left
static void cli_worker(void *context)
{
    (void)context;
    for (;;)
    {
        cli_lock_take(&s_lock);
        while ((s_search.queued == 0u) && !(s_search.heads_ended && (s_search.heads == 0u)))
        {
            cli_wake_wait(&s_search.wake, &s_lock);
        }
        if (s_search.queued == 0u)
        {
            s_search.workers -= 1u;
            cli_wake_all(&s_search.wake);
            cli_lock_give(&s_lock);
            return;
        }
        const CliQueued queued = s_search.queue[s_search.head];
        s_search.head = (s_search.head + 1u) % s_search.room;
        s_search.queued -= 1u;
        cli_wake_all(&s_search.wake);
        cli_lock_give(&s_lock);
        cli_searched(&queued);
    }
}

// a head started: 1, or 0 where it could not start
static int cli_head_started(unsigned int at)
{
    cli_lock_take(&s_lock);
    s_search.heads += 1u;
    cli_lock_give(&s_lock);
    s_search.head_thread[at] = (CliThread){cli_head, NULL};
    if (cli_thread_started(&s_search.head_thread[at]))
    {
        return 1;
    }
    cli_lock_take(&s_lock);
    s_search.heads -= 1u;
    cli_lock_give(&s_lock);
    return 0;
}

// the reader: one head, and another each span of files while the head started last raised the files read a
// millisecond, at most `most` of them; the heads read on until the files are read
static void cli_reader(unsigned int most)
{
    if (!cli_head_started(0u))
    {
        // with no head started, the files are read here, the reader starting no more
        cli_lock_take(&s_lock);
        s_search.heads += 1u;
        cli_lock_give(&s_lock);
        cli_head(NULL);
    }
    unsigned int heads = 1u;
    double rate = 0.0;
    unsigned long long since_at = cli_clock_milliseconds();
    unsigned long long since_done = 0ull;
    while (heads < most)
    {
        query_paused(1u);
        cli_lock_take(&s_lock);
        const unsigned long long done = s_search.read;
        cli_lock_give(&s_lock);
        if (done >= s_search.paths.count)
        {
            break;
        }
        if ((done - since_done) < CLI_HEAD_SPAN)
        {
            continue;
        }
        const double seconds = (double)(cli_clock_milliseconds() - since_at) / 1000.0;
        const double now = (double)(done - since_done) / ((seconds > 1e-9) ? seconds : 1e-9);
        if ((heads > 1u) && (now <= (rate * CLI_HEAD_GAIN)))
        {
            break;
        }
        rate = now;
        since_at = cli_clock_milliseconds();
        since_done = done;
        if (!cli_head_started(heads))
        {
            break;
        }
        heads += 1u;
    }
    cli_lock_take(&s_lock);
    s_search.heads_ended = 1;
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

// the files read and searched, a worker on each core the work may take less the heads', and the lines found written
static void cli_search_run(CliLine *out)
{
    const unsigned int budget = cli_cores_for_work();
    const unsigned int quarter = ((budget / 4u) > 1u) ? (budget / 4u) : 1u;
    const unsigned int heads_most = (quarter < CLI_HEADS) ? quarter : CLI_HEADS;
    const unsigned int files = (s_search.paths.count > 1u) ? s_search.paths.count : 1u;
    unsigned int workers = (budget > heads_most) ? (budget - heads_most) : 0u;
    workers = (workers < 1u) ? 1u : workers;
    workers = (workers > files) ? files : workers;
    workers = (workers > CLI_WORKERS_MOST) ? CLI_WORKERS_MOST : workers;
    s_search.room = workers * CLI_READ_AHEAD;
    unsigned int started = 0u;
    for (unsigned int at = 0u; at < workers; at += 1u)
    {
        cli_lock_take(&s_lock);
        s_search.workers += 1u;
        cli_lock_give(&s_lock);
        s_search.worker_thread[started] = (CliThread){cli_worker, NULL};
        const int one = cli_thread_started(&s_search.worker_thread[started]);
        started += one ? 1u : 0u;
        cli_lock_take(&s_lock);
        s_search.workers -= one ? 0u : 1u;
        cli_lock_give(&s_lock);
    }
    if (started == 0u)
    {
        // with no worker to hand them to, each file is read and searched here, one at a time
        CliQueued queued;
        while (cli_file_read(&queued))
        {
            cli_searched(&queued);
        }
    }
    else
    {
        cli_reader(heads_most);
        cli_lock_take(&s_lock);
        while (s_search.workers > 0u)
        {
            cli_wake_wait(&s_search.wake, &s_lock);
        }
        cli_lock_give(&s_lock);
    }
    cli_live_sorted();
    const unsigned int shown = (s_search.lives < CLI_HITS_MOST) ? s_search.lives : CLI_HITS_MOST;
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < shown; at += 1u)
    {
        const CliHeldHit *const hit = &s_search.pool[s_search.live[at]];
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
    s_search.read = 0u;
    s_search.head = 0u;
    s_search.queued = 0u;
    s_search.heads = 0u;
    s_search.heads_ended = 0;
    s_search.workers = 0u;
    s_search.lives = 0u;
    for (unsigned int at = 0u; at < CLI_POOL_MOST; at += 1u)
    {
        s_search.free[at] = (CLI_POOL_MOST - 1u) - at;
    }
    s_search.frees = CLI_POOL_MOST;
    cli_search_run(out);
    cli_picked_let(&s_search.paths);
    cli_lock_give(&s_turn);
    return 1;
}

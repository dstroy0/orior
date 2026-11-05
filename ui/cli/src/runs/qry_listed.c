// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_listed.c: the Runs pane's rows. The tree's folders of runs come from one walk of the tree's files, walked again
// only where the watch lost changes; the watch hands the folder of each run or overseer.txt it sees written through a
// ring the window's thread drains, and marks the listing stale. Every listing reads each folder's entries anew, a run
// that appears in one showing at once, and reads each folder's overseer.txt for its lessons. A run opened in the pane
// is read through the run's records, its panics' and conferences' texts copied into the pane's own cell, every other
// record listed by its name and size alone. All of it stands in one cell taken at the first listing
#include "qry.h"

#include "../command_line/cells.h"
#include "../command_line/command_line.h"
#include "../editor/files.h"
#include "../text/text.h"
#include "embed_atomic.h"

#include <stdlib.h>

// the most folders of runs, the bytes their paths take, the most runs listed, the most lessons, the bytes the runs'
// paths and the lessons' words take, the runs opened at once, the most records an opened run lists, the bytes of the
// texts of its panics and conferences, the most conferences of one run, the most bytes copied of one record's text,
// the runs whose first panic was posted, and the folders the watch's ring carries
#define RUNS_FOLDERS 256u
#define RUNS_FOLDER_TEXT (64u * 1024u)
#define RUNS_MAX 4096u
#define RUNS_LESSONS 1024u
#define RUNS_TEXT (1u << 20u)
#define RUNS_OPENED 4u
#define RUNS_BLOBS 4096u
#define RUNS_BLOB_TEXT (512u * 1024u)
#define RUNS_CONFERENCES 64u
#define RUNS_TEXT_MAX 65536ULL
#define RUNS_NOTICED 64u
#define RUNS_PENDING 32u
// a record whose text is not copied, a record of no conference, and the ending a run's file takes
#define RUNS_NO_TEXT RUNS_BLOB_TEXT
#define RUNS_NO_CONFERENCE RUNS_CONFERENCES
#define RUNS_ENDING ".qry"
#define RUNS_ENDING_LENGTH 4ULL
#define RUNS_OVERSEER "overseer.txt"
#define RUNS_LESSON "lesson "
#define RUNS_LESSON_LENGTH 7ULL

// A folder of runs: its path's place in the folders' text, its first run and how many, and its first lesson and how
// many
typedef struct
{
    unsigned int path;
    unsigned int first;
    unsigned int count;
    unsigned int lesson_first;
    unsigned int lessons;
} RunsFolder;

// A lesson: the places of its cause, its carrier and its stand-in in the text
typedef struct
{
    unsigned int cause;
    unsigned int carrier;
    unsigned int stand_in;
} RunsLesson;

// A run: its path's place in the text, its name's place, the bytes of the name without the ending, its bytes on the
// disk, whether its writer runs and the place plus 1 of its panic, and its folder
typedef struct
{
    unsigned int path;
    unsigned int name;
    unsigned long long name_length;
    unsigned long long size;
    int live;
    unsigned long long panicked;
    unsigned int folder;
} RunsRun;

// A record of an opened run: its name's place in the run's text and the name's bytes, its blob's bytes, its priority,
// its text's place and bytes, RUNS_NO_TEXT where none is copied, how many lines the text has as its rows show them,
// and the conference it is of, RUNS_NO_CONFERENCE for none
typedef struct
{
    unsigned int name;
    unsigned int name_length;
    unsigned long long size;
    unsigned int priority;
    unsigned int text;
    unsigned long long text_length;
    unsigned int lines;
    unsigned int conference;
} RunsBlob;

// A conference of an opened run: its records' name and the question's text, the name after `conference_` with each
// underscore a space, each a place in the run's text with its bytes
typedef struct
{
    unsigned int name;
    unsigned int name_length;
    unsigned int question;
    unsigned int question_length;
} RunsConference;

// A run opened in the pane: whether the entry is in use, the run's path from the top of the tree, whether its writer
// ran as last read, where the next read goes on from, the .qry's bytes after its last whole record, its records, its
// conferences, and the text each record's name and copied text stand in
typedef struct
{
    int used;
    char path[CLI_PATH_MAX_LEN];
    int live;
    unsigned long long next;
    unsigned long long rest;
    RunsBlob blob[RUNS_BLOBS];
    unsigned int blobs;
    RunsConference conference[RUNS_CONFERENCES];
    unsigned int conferences;
    char text[RUNS_BLOB_TEXT];
    unsigned int used_text;
} RunsOpened;

// A run whose first panic was posted: its path's hash and the panic posted
typedef struct
{
    unsigned long long hash;
    unsigned long long panicked;
} RunsNoticed;

// The pane's data: the tree listed, whether its files were walked for the folders of runs and whether a listing has
// gone, when the runs were listed and the opened runs read on the window's clock, the folders and their text, the runs,
// the lessons and the text their paths and words stand in, how many runs are live and how many of those panicked, the
// runs opened, and the panics posted
typedef struct
{
    char root[CLI_PATH_MAX_LEN];
    int walked;
    int listed;
    unsigned long long listed_ms;
    unsigned long long read_ms;
    RunsFolder folder[RUNS_FOLDERS];
    unsigned int folders;
    char folder_text[RUNS_FOLDER_TEXT];
    unsigned int folder_used;
    RunsRun run[RUNS_MAX];
    unsigned int runs;
    RunsLesson lesson[RUNS_LESSONS];
    unsigned int lessons;
    char text[RUNS_TEXT];
    unsigned int used;
    unsigned int live;
    unsigned int panicked;
    RunsOpened opened[RUNS_OPENED];
    RunsNoticed noticed[RUNS_NOTICED];
    unsigned int noticed_count;
} Runs;

static Runs *s_runs;
static void (*s_woken)(void);
// the watch's thread writes the ring at `put` and the window's thread reads it at `took`, each the other's count read
// through the atomics; a ring with no free place marks the changes lost
static char s_pending[RUNS_PENDING][CLI_PATH_MAX_LEN];
static embed_atomic_u32 s_pending_put;
static embed_atomic_u32 s_pending_took;
static embed_atomic_u32 s_stale;
static embed_atomic_u32 s_lost;

// What a row walk is after: the row asked for, the rows passed, where the row goes, NULL to count alone, and whether
// the row asked for was reached
typedef struct
{
    unsigned int want;
    unsigned int count;
    CliRunsRow *row;
    int found;
} RowsWalk;

typedef struct
{
    const char *root;
    const char *path;
    unsigned long long length;
    unsigned int folder;
    RunsOpened *opened;
    const QryRecord *record;
    unsigned long long sequence;
    const char *text;
    unsigned long long size;
    unsigned int at;
    RowsWalk *walk;
    const CliRunsRow *row;
    int whole;
    unsigned long long now_ms;
    char *pool;
    unsigned int *used;
    unsigned int capacity;
} QryListedCtx;

// `length` bytes of `text` copied into `pool`, sized `capacity` with `used` bytes of it taken, with a NUL after them:
// their place, or `capacity` where they do not fit
EMBED_INLINE unsigned int pool_append(const QryListedCtx *args)
{
    if (args->length + 1ULL > (unsigned long long)(args->capacity - *args->used))
    {
        return args->capacity;
    }
    const unsigned int at = *args->used;
    memor.cpy(&(MemoriaCfg){.dst = args->pool + at, .src = args->text, .bytes = args->length});
    args->pool[at + args->length] = '\0';
    // a text that fits the bytes is counted by an unsigned int
    *args->used = at + (unsigned int)args->length + 1u;
    return at;
}

// the place past the last `/` of `length` bytes of `path`, 0 where it has none: the folder is the bytes before it
EMBED_INLINE unsigned long long runs_folder_cut(const QryListedCtx *args)
{
    return EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path,
                      .at = args->length, .set = "/");
}

// whether `length` bytes of `path` end in a run's ending, or in a folder's overseer.txt
EMBED_INLINE int runs_path_is_run(const QryListedCtx *args)
{
    return (args->length > RUNS_ENDING_LENGTH) &&
           (memor.cmp(&(MemoriaCfg){.src = args->path + args->length - RUNS_ENDING_LENGTH,
                                    .other = RUNS_ENDING,
                                    .bytes = RUNS_ENDING_LENGTH}) == 0);
}

EMBED_INLINE int runs_path_is_overseer(const QryListedCtx *args)
{
    const unsigned long long cut =
        EMBED_CALL(runs_folder_cut, QryListedCtx, .path = args->path, .length = args->length);
    const unsigned long long name_length = args->length - cut;
    return (name_length == cellul.len(&(CatenaFinitaCfg){.src = RUNS_OVERSEER, .cap = 16ULL})) &&
           (memor.cmp(&(MemoriaCfg){.src = args->path + cut, .other = RUNS_OVERSEER, .bytes = name_length}) == 0);
}

// `length` bytes of `path`, a folder from the top of the tree, put in its place among the folders of runs where it is
// not one already; on the window's thread
EMBED_INLINE void runs_folder_append(const QryListedCtx *args)
{
    Runs *const runs = s_runs;
    for (unsigned int at = 0u; at < runs->folders; at += 1u)
    {
        const char *const folder = runs->folder_text + runs->folder[at].path;
        if ((cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = RUNS_FOLDER_TEXT}) == args->length) &&
            (memor.cmp(&(MemoriaCfg){.src = folder, .other = args->path, .bytes = args->length}) == 0))
        {
            return;
        }
    }
    if (runs->folders >= RUNS_FOLDERS)
    {
        return;
    }
    const unsigned int path =
        EMBED_CALL(pool_append, QryListedCtx, .pool = runs->folder_text, .used = &runs->folder_used,
                   .capacity = RUNS_FOLDER_TEXT, .text = args->path, .length = args->length);
    if (path == RUNS_FOLDER_TEXT)
    {
        return;
    }
    unsigned int place = runs->folders;
    while ((place > 0u) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg,
                                       .one = runs->folder_text + runs->folder[place - 1u].path,
                                       .other_pointer = runs->folder_text + path) > 0))
    {
        runs->folder[place] = runs->folder[place - 1u];
        place -= 1u;
    }
    runs->folder[place] = (RunsFolder){path, 0u, 0u, 0u, 0u};
    runs->folders += 1u;
}

// the folder of `length` bytes of `path`, a run's or an overseer.txt's, among the folders of runs
EMBED_INLINE void runs_folder_of_append(const QryListedCtx *args)
{
    const unsigned long long cut =
        EMBED_CALL(runs_folder_cut, QryListedCtx, .path = args->path, .length = args->length);
    EMBED_CALL(runs_folder_append, QryListedCtx, .path = args->path, .length = (cut > 0ULL) ? cut - 1ULL : 0ULL);
}

void cli_qry_run_write(const CliQryPathRootCfg *args)
{
    if ((args->length >= CLI_PATH_MAX_LEN) ||
        (!EMBED_CALL(runs_path_is_run, QryListedCtx, .path = args->path, .length = args->length) &&
         !EMBED_CALL(runs_path_is_overseer, QryListedCtx, .path = args->path, .length = args->length)))
    {
        return;
    }
    const unsigned int put = EMBED_ATOMIC_LOAD(&s_pending_put);
    const unsigned int took = EMBED_ATOMIC_LOAD(&s_pending_took);
    if (put - took < RUNS_PENDING)
    {
        const unsigned long long cut =
            EMBED_CALL(runs_folder_cut, QryListedCtx, .path = args->path, .length = args->length);
        const unsigned long long folder_length = (cut > 0ULL) ? cut - 1ULL : 0ULL;
        char *const pending = s_pending[put % RUNS_PENDING];
        memor.cpy(&(MemoriaCfg){.dst = pending, .src = args->path, .bytes = folder_length});
        pending[folder_length] = '\0';
        EMBED_ATOMIC_STORE(&s_pending_put, put + 1u);
    }
    else
    {
        EMBED_ATOMIC_STORE(&s_lost, 1u);
    }
    EMBED_ATOMIC_STORE(&s_stale, 1u);
    if (s_woken != NULL)
    {
        s_woken();
    }
}

void cli_qry_runs_lose(const CliQryOutRootCfg *args)
{
    (void)args;
    EMBED_ATOMIC_STORE(&s_lost, 1u);
    EMBED_ATOMIC_STORE(&s_stale, 1u);
    if (s_woken != NULL)
    {
        s_woken();
    }
}

// the folders the watch's ring carries taken into the folders of runs
EMBED_INLINE void runs_pending_take(const QryListedCtx *args)
{
    (void)args;
    const unsigned int put = EMBED_ATOMIC_LOAD(&s_pending_put);
    for (unsigned int took = EMBED_ATOMIC_LOAD(&s_pending_took); took != put; took += 1u)
    {
        const char *const folder = s_pending[took % RUNS_PENDING];
        EMBED_CALL(runs_folder_append, QryListedCtx, .path = folder,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_PATH_MAX_LEN}));
        EMBED_ATOMIC_STORE(&s_pending_took, took + 1u);
    }
}

// the tree's files walked for its folders of runs: the folder of every .qry and every overseer.txt
EMBED_INLINE void runs_walk(const QryListedCtx *args)
{
    Runs *const runs = s_runs;
    runs->folders = 0u;
    runs->folder_used = 0u;
    const CliFiles *const files = EMBED_CALL(cli_files_root_paths.view, CliFilesRootPathsCfg, .root = args->root);
    for (unsigned int at = 0u; (files != NULL) && (at < files->count); at += 1u)
    {
        const char *const path = EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = at);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN});
        if (EMBED_CALL(runs_path_is_run, QryListedCtx, .path = path, .length = length) ||
            EMBED_CALL(runs_path_is_overseer, QryListedCtx, .path = path, .length = length))
        {
            EMBED_CALL(runs_folder_of_append, QryListedCtx, .path = path, .length = length);
        }
    }
    if (files != NULL)
    {
        EMBED_CALL(cli_files.let, CliFilesCfg, .files = files);
    }
    runs->walked = 1;
}

// the full path of `path`, from the top of the tree at `root`, into `text`, sized CLI_PATH_MAX_LEN
EMBED_INLINE void runs_full(const QryListedCtx *args)
{
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = args->path,
               .joined = (char *)args->text);
}

// the lessons of the overseer.txt at `path` in the tree at `root`, each line `lesson <cause> <carrier> <stand-in>`,
// appended after the lessons so far: how many
EMBED_INLINE unsigned int runs_lessons_read(const QryListedCtx *args)
{
    Runs *const runs = s_runs;
    char full[CLI_PATH_MAX_LEN];
    EMBED_CALL(runs_full, QryListedCtx, .root = args->root, .path = args->path, .text = full);
    unsigned long long size = 0ULL;
    const char *const text = (const char *)EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = full,
                                                      .size_pointer = &size, .owner = &g_cli_qry_listing_cells);
    unsigned int count = 0u;
    for (unsigned long long at = 0ULL; (text != NULL) && (at < size) && (runs->lessons < RUNS_LESSONS);)
    {
        const unsigned long long end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                  .text = text, .at = at, .size = size, .set = "\n");
        const unsigned long long line_end = ((end > at) && (text[end - 1ULL] == '\r')) ? end - 1ULL : end;
        if ((line_end - at > RUNS_LESSON_LENGTH) &&
            (memor.cmp(&(MemoriaCfg){.src = text + at, .other = RUNS_LESSON, .bytes = RUNS_LESSON_LENGTH}) == 0))
        {
            unsigned int word[3] = {RUNS_TEXT, RUNS_TEXT, RUNS_TEXT};
            unsigned long long from = at + RUNS_LESSON_LENGTH;
            for (unsigned int each = 0u; each < 3u; each += 1u)
            {
                from = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = text, .at = from,
                                  .size = line_end, .set = " \t");
                const unsigned long long word_end =
                    EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text, .at = from,
                               .size = line_end, .set = " \t");
                if (word_end > from)
                {
                    word[each] = EMBED_CALL(pool_append, QryListedCtx, .pool = runs->text, .used = &runs->used,
                                            .capacity = RUNS_TEXT, .text = text + from, .length = word_end - from);
                }
                from = word_end;
            }
            if ((word[0] != RUNS_TEXT) && (word[1] != RUNS_TEXT) && (word[2] != RUNS_TEXT))
            {
                runs->lesson[runs->lessons] = (RunsLesson){word[0], word[1], word[2]};
                runs->lessons += 1u;
                count += 1u;
            }
        }
        at = end + 1ULL;
    }
    g_cli_cells->persistent_buf_release((void *)(uintptr_t)((const unsigned char *)text), (&g_cli_qry_listing_cells));
    return count;
}

// the hash of `length` bytes of `path`, FNV-1a over 64 bits
EMBED_INLINE unsigned long long runs_hash(const QryListedCtx *args)
{
    unsigned long long hash = 14695981039346656037ULL;
    for (unsigned long long at = 0ULL; at < args->length; at += 1ULL)
    {
        hash ^= (unsigned char)args->path[at];
        hash *= 1099511628211ULL;
    }
    return hash;
}

// the opened entry of the run at `path`, NULL where it is not opened
EMBED_INLINE RunsOpened *runs_opened_of(const QryListedCtx *args)
{
    Runs *const runs = s_runs;
    for (unsigned int at = 0u; at < RUNS_OPENED; at += 1u)
    {
        if (runs->opened[at].used &&
            cellul.eq(&(CatenaFinitaCfg){.src = runs->opened[at].path, .other = args->path, .cap = CLI_PATH_MAX_LEN}))
        {
            return &runs->opened[at];
        }
    }
    return NULL;
}

// an entry of the folder `folder` of runs read: a .qry a run with its size and whether its writer runs, listed under
// the folder; an overseer.txt the folder's lessons
static void runs_entry(const CliEachNameIsFolderCfg *args)
{
    Runs *const runs = s_runs;
    const QryListedCtx *const listing = (const QryListedCtx *)args->context;
    RunsFolder *const folder = &runs->folder[listing->folder];
    const char *const folder_path = runs->folder_text + folder->path;
    char relative[CLI_PATH_MAX_LEN];
    VerbaLinea line = {relative, sizeof(relative), 0ULL};
    if (folder_path[0] != '\0')
    {
        mmgr_line_put(&line, folder_path);
        mmgr_line_ch(&line, '/');
    }
    const unsigned long long name_at = line.at;
    mmgr_line_put(&line, args->name);
    if (args->is_folder || (mmgr_line_finish(&line) == 0ULL))
    {
        return;
    }
    const unsigned long long length = line.at;
    if (EMBED_CALL(runs_path_is_overseer, QryListedCtx, .path = relative, .length = length))
    {
        folder->lessons += EMBED_CALL(runs_lessons_read, QryListedCtx, .root = listing->root, .path = relative);
        return;
    }
    if (!EMBED_CALL(runs_path_is_run, QryListedCtx, .path = relative, .length = length) || (runs->runs >= RUNS_MAX))
    {
        return;
    }
    const unsigned int path = EMBED_CALL(pool_append, QryListedCtx, .pool = runs->text, .used = &runs->used,
                                         .capacity = RUNS_TEXT, .text = relative, .length = length);
    if (path == RUNS_TEXT)
    {
        return;
    }
    char full[CLI_PATH_MAX_LEN];
    EMBED_CALL(runs_full, QryListedCtx, .root = listing->root, .path = relative, .text = full);
    RunsRun *const run = &runs->run[runs->runs];
    *run = (RunsRun){path, 0u, 0ULL, 0ULL, 0, 0ULL, listing->folder};
    // the name starts inside the path, which an unsigned int counts
    run->name = path + (unsigned int)name_at;
    run->name_length = length - name_at - RUNS_ENDING_LENGTH;
    (void)EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = full, .size_pointer = &run->size);
    run->live =
        EMBED_CALL(cli_qry_path_panicked.existing, CliQryPathPanickedCfg, .path = full, .panicked = &run->panicked);
    runs->runs += 1u;
    folder->count += 1u;
}

// the order of two runs by their paths
static int runs_order(const void *one, const void *other)
{
    const Runs *const runs = s_runs;
    return EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = runs->text + ((const RunsRun *)one)->path,
                      .other_pointer = runs->text + ((const RunsRun *)other)->path);
}

// a live run's panic posted as a failed notice where it was not posted already: `<name> panicked at blob <N>`
EMBED_INLINE void runs_panic_notice(const QryListedCtx *args)
{
    Runs *const runs = s_runs;
    const RunsRun *const run = &runs->run[args->at];
    const char *const path = runs->text + run->path;
    const unsigned long long hash = EMBED_CALL(runs_hash, QryListedCtx, .path = path,
                                               .length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = RUNS_TEXT}));
    RunsNoticed *noticed = NULL;
    for (unsigned int at = 0u; at < runs->noticed_count; at += 1u)
    {
        noticed = (runs->noticed[at].hash == hash) ? &runs->noticed[at] : noticed;
    }
    if ((noticed != NULL) && (noticed->panicked == run->panicked))
    {
        return;
    }
    if (noticed == NULL)
    {
        // the table full, its oldest entry takes the new run
        const unsigned int at = (runs->noticed_count < RUNS_NOTICED) ? runs->noticed_count : 0u;
        runs->noticed_count += (runs->noticed_count < RUNS_NOTICED) ? 1u : 0u;
        noticed = &runs->noticed[at];
        noticed->hash = hash;
    }
    noticed->panicked = run->panicked;
    char text[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea line = {text, sizeof(text), 0ULL};
    mmgr_line_put_n(&line, runs->text + run->name, run->name_length);
    mmgr_line_put(&line, " panicked at blob ");
    mmgr_line_decimal(&line, run->panicked, 1u);
    if (mmgr_line_finish(&line) != 0ULL)
    {
        cli_notice_post(text, 1);
    }
}

// the runs listed: the folders walked for where none were or the watch lost changes, the watch's folders taken, each
// folder's entries read for its runs and its lessons, the runs of a folder in the order of their paths, a live run's
// first panic posted, and the live and panicked counted
EMBED_INLINE void runs_list(const QryListedCtx *args)
{
    Runs *const runs = s_runs;
    if (!runs->walked || (EMBED_ATOMIC_EXCHANGE_32(&s_lost, 0u) != 0u))
    {
        EMBED_CALL(runs_walk, QryListedCtx, .root = args->root);
    }
    EMBED_CALL(runs_pending_take, QryListedCtx, .root = args->root);
    runs->runs = 0u;
    runs->lessons = 0u;
    runs->used = 0u;
    for (unsigned int at = 0u; at < runs->folders; at += 1u)
    {
        RunsFolder *const folder = &runs->folder[at];
        folder->first = runs->runs;
        folder->count = 0u;
        folder->lesson_first = runs->lessons;
        folder->lessons = 0u;
        char full[CLI_PATH_MAX_LEN];
        EMBED_CALL(runs_full, QryListedCtx, .root = args->root, .path = runs->folder_text + folder->path, .text = full);
        QryListedCtx listing = {.root = args->root, .folder = at};
        (void)EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = full, .each = runs_entry,
                         .context_bytess = &listing);
        qsort(runs->run + folder->first, folder->count, sizeof(RunsRun), runs_order);
    }
    runs->live = 0u;
    runs->panicked = 0u;
    for (unsigned int at = 0u; at < runs->runs; at += 1u)
    {
        const RunsRun *const run = &runs->run[at];
        runs->live += run->live ? 1u : 0u;
        runs->panicked += (run->live && (run->panicked != 0ULL)) ? 1u : 0u;
        if (run->live && (run->panicked != 0ULL))
        {
            EMBED_CALL(runs_panic_notice, QryListedCtx, .at = at);
        }
    }
    runs->listed = 1;
    runs->listed_ms = args->now_ms;
}

// how many lines `size` bytes of `text` have as rows show them: every line where `whole`, a trailing newline closing
// the last and opening none; else the lines that carry more than blanks
EMBED_INLINE unsigned int runs_lines(const QryListedCtx *args)
{
    unsigned int lines = 0u;
    for (unsigned long long at = 0ULL; at < args->size;)
    {
        const unsigned long long end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                  .text = args->text, .at = at, .size = args->size, .set = "\n");
        const unsigned long long first = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg,
                                                    .text = args->text, .at = at, .size = end, .set = " \t\r");
        lines += (args->whole || (first < end)) ? 1u : 0u;
        at = end + 1ULL;
    }
    return lines;
}

// the line `at` of `size` bytes of `text`, counted as runs_lines counts them, trimmed of its return where `whole` and
// of its blanks at both ends where not: 1 with its start into `row->text` and its bytes into `row->text_length`
EMBED_INLINE int runs_line(const QryListedCtx *args)
{
    CliRunsRow *const row = (CliRunsRow *)args->row;
    unsigned int lines = 0u;
    for (unsigned long long from = 0ULL; from < args->size;)
    {
        const unsigned long long end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                  .text = args->text, .at = from, .size = args->size, .set = "\n");
        const unsigned long long first = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg,
                                                    .text = args->text, .at = from, .size = end, .set = " \t\r");
        if (args->whole || (first < end))
        {
            if (lines == args->at)
            {
                const unsigned long long start = args->whole ? from : first;
                unsigned long long last = end;
                while ((last > start) &&
                       ((args->text[last - 1ULL] == '\r') ||
                        (!args->whole && ((args->text[last - 1ULL] == ' ') || (args->text[last - 1ULL] == '\t')))))
                {
                    last -= 1ULL;
                }
                row->text = args->text + start;
                row->text_length = last - start;
                return 1;
            }
            lines += 1u;
        }
        from = end + 1ULL;
    }
    return 0;
}

// a record of an opened run read: its name copied, and for a panic or a conference its text, up to RUNS_TEXT_MAX
// bytes, and the conference's question the first time its name is read
static void opened_record(const CliEachRecordCfg *args)
{
    RunsOpened *const opened = (RunsOpened *)args->context;
    const QryRecord *const record = args->record;
    if (opened->blobs >= RUNS_BLOBS)
    {
        return;
    }
    const unsigned int name =
        EMBED_CALL(pool_append, QryListedCtx, .pool = opened->text, .used = &opened->used_text,
                   .capacity = RUNS_BLOB_TEXT, .text = QRY_RECORD_NAME(record), .length = record->name_length);
    RunsBlob *const blob = &opened->blob[opened->blobs];
    *blob = (RunsBlob){name, record->name_length, record->blob_length, record->priority, RUNS_NO_TEXT, 0ULL,
                       0u,   RUNS_NO_CONFERENCE};
    const unsigned long long prefix = cellul.len(&(CatenaFinitaCfg){.src = CLI_CONFERENCE, .cap = 32ULL});
    const int conference =
        (record->name_length > prefix) &&
        (memor.cmp(&(MemoriaCfg){.src = QRY_RECORD_NAME(record), .other = CLI_CONFERENCE, .bytes = prefix}) == 0);
    if ((record->priority == QRY_PANIC) || conference)
    {
        blob->text_length = (record->blob_length < RUNS_TEXT_MAX) ? record->blob_length : RUNS_TEXT_MAX;
        blob->text = EMBED_CALL(pool_append, QryListedCtx, .pool = opened->text, .used = &opened->used_text,
                                .capacity = RUNS_BLOB_TEXT, .text = (const char *)QRY_RECORD_BLOB(record),
                                .length = blob->text_length);
        blob->text_length = (blob->text != RUNS_NO_TEXT) ? blob->text_length : 0ULL;
        blob->lines = EMBED_CALL(runs_lines, QryListedCtx, .text = opened->text + blob->text, .size = blob->text_length,
                                 .whole = record->priority == QRY_PANIC);
    }
    if (conference && (name != RUNS_NO_TEXT))
    {
        for (unsigned int at = 0u; at < opened->conferences; at += 1u)
        {
            const RunsConference *const one = &opened->conference[at];
            if ((one->name_length == record->name_length) && (memor.cmp(&(MemoriaCfg){.src = opened->text + one->name,
                                                                                      .other = QRY_RECORD_NAME(record),
                                                                                      .bytes = one->name_length}) == 0))
            {
                blob->conference = at;
            }
        }
        if ((blob->conference == RUNS_NO_CONFERENCE) && (opened->conferences < RUNS_CONFERENCES))
        {
            const unsigned long long question_length = record->name_length - prefix;
            const unsigned int question = EMBED_CALL(
                pool_append, QryListedCtx, .pool = opened->text, .used = &opened->used_text, .capacity = RUNS_BLOB_TEXT,
                .text = QRY_RECORD_NAME(record) + prefix, .length = question_length);
            if (question != RUNS_NO_TEXT)
            {
                for (unsigned long long at = 0ULL; at < question_length; at += 1ULL)
                {
                    opened->text[question + at] =
                        (opened->text[question + at] == '_') ? ' ' : opened->text[question + at];
                }
                // a question's bytes are inside a record's name, which an unsigned int counts
                opened->conference[opened->conferences] =
                    (RunsConference){name, record->name_length, question, (unsigned int)question_length};
                blob->conference = opened->conferences;
                opened->conferences += 1u;
            }
        }
    }
    opened->blobs += 1u;
}

// an opened run read: whole where `whole`, else on from where the last read stopped; a run whose writer stopped since
// the last read is read whole again from its .qry
EMBED_INLINE void opened_read(const QryListedCtx *args)
{
    RunsOpened *const opened = args->opened;
    if (args->whole)
    {
        opened->blobs = 0u;
        opened->conferences = 0u;
        opened->used_text = 0u;
        opened->next = 0ULL;
        opened->rest = 0ULL;
    }
    char full[CLI_PATH_MAX_LEN];
    EMBED_CALL(runs_full, QryListedCtx, .root = args->root, .path = opened->path, .text = full);
    const int was_live = opened->live;
    opened->live = EMBED_CALL(cli_qry_path_from_each.records, CliQryPathFromEachCfg, .path = full, .from = opened->next,
                              .sequence = opened->blobs, .each = opened_record, .context = opened,
                              .next = &opened->next, .rest = &opened->rest);
    if (!args->whole && was_live && !opened->live)
    {
        EMBED_CALL(opened_read, QryListedCtx, .root = args->root, .opened = opened, .whole = 1);
    }
}

// whether any run listed is live, and whether any opened run was live as last read
EMBED_INLINE int runs_any_live(const QryListedCtx *args)
{
    (void)args;
    return s_runs->live != 0u;
}

EMBED_INLINE int runs_any_opened_live(const QryListedCtx *args)
{
    (void)args;
    for (unsigned int at = 0u; at < RUNS_OPENED; at += 1u)
    {
        if (s_runs->opened[at].used && s_runs->opened[at].live)
        {
            return 1;
        }
    }
    return 0;
}

// the cell taken where none is, and everything of a tree other than `root` let go: the cell, or NULL where none can be
// taken
EMBED_INLINE Runs *runs_of(const QryListedCtx *args)
{
    if (s_runs == NULL)
    {
        s_runs = g_cli_cells->persistent_buf_alloc(sizeof(Runs), &g_cli_qry_listing_cells);
        if (s_runs == NULL)
        {
            return NULL;
        }
        memor.set(&(MemoriaCfg){.dst = s_runs, .val = 0, .bytes = sizeof(Runs)});
    }
    Runs *const runs = s_runs;
    if (!cellul.eq(&(CatenaFinitaCfg){.src = runs->root, .other = args->root, .cap = CLI_PATH_MAX_LEN}))
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_PATH_MAX_LEN});
        runs->walked = 0;
        runs->listed = 0;
        runs->runs = 0u;
        runs->folders = 0u;
        runs->lessons = 0u;
        runs->live = 0u;
        runs->panicked = 0u;
        runs->noticed_count = 0u;
        for (unsigned int at = 0u; at < RUNS_OPENED; at += 1u)
        {
            runs->opened[at].used = 0;
        }
        memor.cpy(
            &(MemoriaCfg){.dst = runs->root, .src = args->root, .bytes = (length < CLI_PATH_MAX_LEN) ? length : 0ULL});
        runs->root[(length < CLI_PATH_MAX_LEN) ? length : 0ULL] = '\0';
    }
    return runs;
}

void cli_runs_pane_begin(const CliRunsPaneCfg *args)
{
    s_woken = args->woken;
}

void cli_runs_pane_relist(const CliRunsPaneCfg *args)
{
    if ((args->root != NULL) && (EMBED_CALL(runs_of, QryListedCtx, .root = args->root) != NULL))
    {
        EMBED_CALL(runs_list, QryListedCtx, .root = args->root, .now_ms = args->now_ms);
    }
}

int cli_runs_pane_step(const CliRunsPaneCfg *args)
{
    if (args->root == NULL)
    {
        // no tree: the rows of the last are let go
        const int listed = (s_runs != NULL) && s_runs->listed;
        if (listed)
        {
            s_runs->listed = 0;
            s_runs->runs = 0u;
            s_runs->live = 0u;
            s_runs->panicked = 0u;
            s_runs->root[0] = '\0';
        }
        return listed;
    }
    Runs *const runs = EMBED_CALL(runs_of, QryListedCtx, .root = args->root);
    if (runs == NULL)
    {
        return 0;
    }
    int changed = 0;
    const int stale = EMBED_ATOMIC_EXCHANGE_32(&s_stale, 0u) != 0u;
    if (!runs->listed || stale ||
        (EMBED_CALL(runs_any_live, QryListedCtx, .root = args->root) &&
         (args->now_ms - runs->listed_ms >= CLI_RUNS_LIST_LIVE_MS)))
    {
        EMBED_CALL(runs_list, QryListedCtx, .root = args->root, .now_ms = args->now_ms);
        changed = 1;
    }
    if (args->shown && EMBED_CALL(runs_any_opened_live, QryListedCtx, .root = args->root) &&
        (args->now_ms - runs->read_ms >= CLI_RUNS_READ_LIVE_MS))
    {
        for (unsigned int at = 0u; at < RUNS_OPENED; at += 1u)
        {
            if (runs->opened[at].used && runs->opened[at].live)
            {
                EMBED_CALL(opened_read, QryListedCtx, .root = args->root, .opened = &runs->opened[at], .whole = 0);
            }
        }
        runs->read_ms = args->now_ms;
        changed = 1;
    }
    return changed;
}

unsigned long long cli_runs_pane_deadline_ms(const CliRunsPaneCfg *args)
{
    const Runs *const runs = s_runs;
    if ((runs == NULL) || !runs->listed || (args->root == NULL))
    {
        return 0ULL;
    }
    unsigned long long deadline = 0ULL;
    if (EMBED_CALL(runs_any_live, QryListedCtx, .root = args->root))
    {
        deadline = runs->listed_ms + CLI_RUNS_LIST_LIVE_MS;
    }
    if (args->shown && EMBED_CALL(runs_any_opened_live, QryListedCtx, .root = args->root))
    {
        const unsigned long long read = runs->read_ms + CLI_RUNS_READ_LIVE_MS;
        deadline = ((deadline == 0ULL) || (read < deadline)) ? read : deadline;
    }
    return deadline;
}

// a row handed to the walk: the walk's row filled where it is the one asked for: 1 once the row asked for was reached
EMBED_INLINE int rows_put(const QryListedCtx *args)
{
    RowsWalk *const walk = args->walk;
    if ((walk->row != NULL) && (walk->count == walk->want))
    {
        *walk->row = *args->row;
        walk->found = 1;
    }
    walk->count += 1u;
    return walk->found;
}

// the rows of an opened run's detail handed to the walk: each panic's head and lines, each conference's question and
// accounts, the count, and the latest records: 1 once the row asked for was reached
EMBED_INLINE int rows_detail(const QryListedCtx *args)
{
    const RunsOpened *const opened = args->opened;
    for (unsigned int at = 0u; at < opened->blobs; at += 1u)
    {
        const RunsBlob *const blob = &opened->blob[at];
        if (blob->priority != QRY_PANIC)
        {
            continue;
        }
        if (EMBED_CALL(rows_put, QryListedCtx, .walk = args->walk,
                       .row = &(CliRunsRow){.kind = CLI_RUNS_ROW_PANIC, .number = at}))
        {
            return 1;
        }
        for (unsigned int line = 0u; line < blob->lines; line += 1u)
        {
            CliRunsRow row = {.kind = CLI_RUNS_ROW_PANIC_LINE, .number = at};
            if (args->walk->count == args->walk->want)
            {
                (void)EMBED_CALL(runs_line, QryListedCtx, .text = opened->text + blob->text, .size = blob->text_length,
                                 .at = line, .whole = 1, .row = &row);
            }
            if (EMBED_CALL(rows_put, QryListedCtx, .walk = args->walk, .row = &row))
            {
                return 1;
            }
        }
    }
    for (unsigned int conference = 0u; conference < opened->conferences; conference += 1u)
    {
        const RunsConference *const one = &opened->conference[conference];
        if (EMBED_CALL(rows_put, QryListedCtx, .walk = args->walk,
                       .row = &(CliRunsRow){.kind = CLI_RUNS_ROW_QUESTION,
                                            .text = opened->text + one->question,
                                            .text_length = one->question_length}))
        {
            return 1;
        }
        for (unsigned int at = 0u; at < opened->blobs; at += 1u)
        {
            const RunsBlob *const blob = &opened->blob[at];
            if (blob->conference != conference)
            {
                continue;
            }
            for (unsigned int line = 0u; line < blob->lines; line += 1u)
            {
                CliRunsRow row = {.kind = CLI_RUNS_ROW_ACCOUNT, .number = at};
                if (args->walk->count == args->walk->want)
                {
                    (void)EMBED_CALL(runs_line, QryListedCtx, .text = opened->text + blob->text,
                                     .size = blob->text_length, .at = line, .whole = 0, .row = &row);
                    // the expert before the first colon, the account after it, the ruling the overseer's
                    const unsigned long long cut =
                        EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = row.text,
                                   .at = 0ULL, .size = row.text_length, .set = ":");
                    if ((cut > 0ULL) && (cut < row.text_length))
                    {
                        row.second = row.text;
                        row.second_length = cut;
                        while ((row.second_length > 0ULL) && (row.second[row.second_length - 1ULL] == ' '))
                        {
                            row.second_length -= 1ULL;
                        }
                        const unsigned long long account_at =
                            EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = row.text,
                                       .at = cut + 1ULL, .size = row.text_length, .set = " \t");
                        row.text_length -= account_at;
                        row.text += account_at;
                        row.ruling =
                            (row.second_length == 8ULL) &&
                            (memor.cmp(&(MemoriaCfg){.src = row.second, .other = "overseer", .bytes = 8ULL}) == 0);
                    }
                }
                if (EMBED_CALL(rows_put, QryListedCtx, .walk = args->walk, .row = &row))
                {
                    return 1;
                }
            }
        }
    }
    if (EMBED_CALL(
            rows_put, QryListedCtx, .walk = args->walk,
            .row = &(CliRunsRow){
                .kind = CLI_RUNS_ROW_COUNT, .number = opened->blobs, .size = opened->rest, .live = opened->live}))
    {
        return 1;
    }
    const unsigned int first = (opened->blobs > CLI_RUNS_SHOWN_BLOBS) ? opened->blobs - CLI_RUNS_SHOWN_BLOBS : 0u;
    for (unsigned int at = first; at < opened->blobs; at += 1u)
    {
        const RunsBlob *const blob = &opened->blob[at];
        if (EMBED_CALL(rows_put, QryListedCtx, .walk = args->walk,
                       .row = &(CliRunsRow){.kind = CLI_RUNS_ROW_BLOB,
                                            .text = (blob->name != RUNS_NO_TEXT) ? opened->text + blob->name : "",
                                            .text_length = (blob->name != RUNS_NO_TEXT) ? blob->name_length : 0ULL,
                                            .number = at,
                                            .size = blob->size,
                                            .priority = blob->priority}))
        {
            return 1;
        }
    }
    return 0;
}

// every row handed to the walk in order: no run where none is listed; else each folder's head, its runs each with its
// detail where opened, and its lessons under a head: 1 once the row asked for was reached
EMBED_INLINE int rows_walk(const QryListedCtx *args)
{
    const Runs *const runs = s_runs;
    if ((runs == NULL) || !runs->listed)
    {
        return 0;
    }
    if (runs->runs == 0u)
    {
        return EMBED_CALL(rows_put, QryListedCtx, .walk = args->walk, .row = &(CliRunsRow){.kind = CLI_RUNS_ROW_EMPTY});
    }
    for (unsigned int at = 0u; at < runs->folders; at += 1u)
    {
        const RunsFolder *const folder = &runs->folder[at];
        if ((folder->count == 0u) && (folder->lessons == 0u))
        {
            continue;
        }
        const char *const folder_path = runs->folder_text + folder->path;
        if (EMBED_CALL(rows_put, QryListedCtx, .walk = args->walk,
                       .row = &(CliRunsRow){.kind = CLI_RUNS_ROW_FOLDER,
                                            .text = (folder_path[0] != '\0') ? folder_path : ".",
                                            .text_length = (folder_path[0] != '\0')
                                                               ? cellul.len(&(CatenaFinitaCfg){.src = folder_path,
                                                                                               .cap = RUNS_FOLDER_TEXT})
                                                               : 1ULL}))
        {
            return 1;
        }
        for (unsigned int each = folder->first; each < folder->first + folder->count; each += 1u)
        {
            const RunsRun *const run = &runs->run[each];
            RunsOpened *const opened = EMBED_CALL(runs_opened_of, QryListedCtx, .path = runs->text + run->path);
            if (EMBED_CALL(rows_put, QryListedCtx, .walk = args->walk,
                           .row = &(CliRunsRow){.kind = CLI_RUNS_ROW_RUN,
                                                .path = runs->text + run->path,
                                                .text = runs->text + run->name,
                                                .text_length = run->name_length,
                                                .size = run->size,
                                                .live = run->live,
                                                .panicked = run->panicked,
                                                .open = opened != NULL}))
            {
                return 1;
            }
            if ((opened != NULL) && EMBED_CALL(rows_detail, QryListedCtx, .walk = args->walk, .opened = opened))
            {
                return 1;
            }
        }
        if (folder->lessons == 0u)
        {
            continue;
        }
        if (EMBED_CALL(rows_put, QryListedCtx, .walk = args->walk, .row = &(CliRunsRow){.kind = CLI_RUNS_ROW_LESSONS}))
        {
            return 1;
        }
        for (unsigned int each = folder->lesson_first; each < folder->lesson_first + folder->lessons; each += 1u)
        {
            const RunsLesson *const lesson = &runs->lesson[each];
            if (EMBED_CALL(rows_put, QryListedCtx, .walk = args->walk,
                           .row = &(CliRunsRow){.kind = CLI_RUNS_ROW_LESSON,
                                                .text = runs->text + lesson->cause,
                                                .text_length = cellul.len(&(CatenaFinitaCfg){
                                                    .src = runs->text + lesson->cause, .cap = RUNS_TEXT}),
                                                .second = runs->text + lesson->carrier,
                                                .second_length = cellul.len(&(CatenaFinitaCfg){
                                                    .src = runs->text + lesson->carrier, .cap = RUNS_TEXT}),
                                                .third = runs->text + lesson->stand_in,
                                                .third_length = cellul.len(&(CatenaFinitaCfg){
                                                    .src = runs->text + lesson->stand_in, .cap = RUNS_TEXT})}))
            {
                return 1;
            }
        }
    }
    return 0;
}

unsigned int cli_runs_pane_rows(const CliRunsPaneCfg *args)
{
    (void)args;
    RowsWalk walk = {0u, 0u, NULL, 0};
    (void)EMBED_CALL(rows_walk, QryListedCtx, .walk = &walk);
    return walk.count;
}

int cli_runs_pane_row(const CliRunsPaneCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->row, .val = 0, .bytes = sizeof(*args->row)});
    RowsWalk walk = {args->at, 0u, args->row, 0};
    return EMBED_CALL(rows_walk, QryListedCtx, .walk = &walk);
}

void cli_runs_pane_toggle(const CliRunsPaneCfg *args)
{
    Runs *const runs = s_runs;
    CliRunsRow row;
    if ((runs == NULL) || (args->root == NULL) ||
        !EMBED_CALL(cli_runs_pane.row, CliRunsPaneCfg, .at = args->at, .row = &row) || (row.kind != CLI_RUNS_ROW_RUN))
    {
        return;
    }
    RunsOpened *const opened = EMBED_CALL(runs_opened_of, QryListedCtx, .path = row.path);
    if (opened != NULL)
    {
        opened->used = 0;
        return;
    }
    RunsOpened *free_entry = NULL;
    for (unsigned int at = 0u; at < RUNS_OPENED; at += 1u)
    {
        free_entry = (!runs->opened[at].used && (free_entry == NULL)) ? &runs->opened[at] : free_entry;
    }
    if (free_entry == NULL)
    {
        // every entry in use: the first entry is taken over
        free_entry = &runs->opened[0];
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = row.path, .cap = RUNS_TEXT});
    memor.cpy(&(MemoriaCfg){.dst = free_entry->path, .src = row.path, .bytes = length + 1ULL});
    free_entry->used = 1;
    free_entry->live = 0;
    EMBED_CALL(opened_read, QryListedCtx, .root = args->root, .opened = free_entry, .whole = 1);
}

void cli_runs_pane_marks(const CliRunsPaneCfg *args)
{
    const Runs *const runs = s_runs;
    *args->live_out = ((runs != NULL) && runs->listed) ? runs->live : 0u;
    *args->panicked_out = ((runs != NULL) && runs->listed) ? runs->panicked : 0u;
}

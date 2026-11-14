// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_log.c: the Git pane's rows. The query, the authors setting and the repository chosen stand here; a change marks
// the rest's start, and the step that finds the rest passed, or the watch's mark that the graph is stale, lists the
// tree's repositories and reads the chosen one's graph through git_graph.c into cells of the listing, then writes the
// repositories and the graph's commits and strokes into movable cells that stand until the next listing, every row
// read out of them by copy. The label `name: branch` and the commits' count as text are written once a listing into
// the pane's own bytes, and each commit's day into a movable cell sized to them, read into a row as it is asked for. A
// commit opened has the files it changed read once through git_lists.c into a movable cell of its own with their paths
// under the tree, read a record at a time, and is found again by its id as the graph is listed again; the opened
// commits' records stand in a movable cell of their count
#include "git.h"

#include "../command_line/commands.h"
#include "../command_line/pool.h"
#include "../text/text.h"
#include "../text/typed.h"
#include "embed_atomic.h"

// the key the authors setting is saved under in orior's folder, the branch row's text where the repository is on no
// branch, the notice under a commit that changed no file under the tree, and the words of the commits' count
#define GIT_LOG_AUTHORS_KEY "git.authors"
#define GIT_LOG_NO_BRANCH "no branch"
#define GIT_LOG_NONE "No file under the tree changed."
#define GIT_LOG_COMMITS " commits "
#define GIT_LOG_FOUND "found"
// the bytes the label `name: branch` and the commits' count as text take at the most, each with its end
#define GIT_LOG_LABEL ((2u * CLI_NAME_MAX_LEN) + 3u)
#define GIT_LOG_COUNT 64u

// A file a commit changed: the place of its path under the tree in the commit's text, the place of its name in that
// path, and its state letter
typedef struct
{
    unsigned int path;
    unsigned int name;
    char state;
} GitLogFile;

// A commit whose files were read: the first sixteen hex digits of its id as a number, which finds it again as the graph
// is listed again; its place in the graph, CLI_GIT_GRAPH where the graph no longer lists it; its first row; whether its
// files show; how many files; and its movable cell, a GitLogFile a file with their paths' text after them, NULL where
// it changed none
typedef struct
{
    unsigned long long id;
    unsigned int commit;
    unsigned int row;
    int shown;
    unsigned int count;
    const CarcerMovableEntry *cell;
} GitLogOpened;

// The pane's data: the query, whether a change waits and since when, the authors setting, whether rows stand; the
// repositories as git lists them in a movable cell, a CliRepository each, how many, their texts and the one chosen;
// the graph's commits as git_graph.c lays them in a movable cell, a CliDrawn each then every stroke of four words, how
// many of each, and their texts; the label `name: branch` and the commits' count as text with their bytes, the days in
// a movable cell, CLI_GIT_LOG_DAY bytes a commit, the rows counted, and the commits whose files were read, records in a
// movable cell of their count, in the graph's order
typedef struct
{
    CliTyped query;
    int waits;
    unsigned long long changed_ms;
    int authors;
    int listed;
    const CarcerMovableEntry *repositories_cell;
    unsigned int repositories;
    CliGitText repositories_text;
    unsigned int chosen;
    const CarcerMovableEntry *graph_cell;
    unsigned int commits;
    unsigned int strokes;
    CliGitText graph_text;
    char label[GIT_LOG_LABEL];
    unsigned long long label_length;
    char count[GIT_LOG_COUNT];
    unsigned long long count_length;
    const CarcerMovableEntry *days;
    unsigned int rows;
    const CarcerMovableEntry *opened;
    unsigned int opens;
} GitLog;

static GitLog s_log;
static void (*s_woken)(void);
static embed_atomic_u32 s_stale;

// the place of the opened commit `at`'s record in the records' cell, of the file `at`'s record in a commit's cell, of
// the repository `at`'s record in the repositories' cell, and of the commit `at`'s record and of the stroke `at` in
// the graph's cell, the strokes after every commit
#define GIT_LOG_OPENED_AT(at_) ((unsigned long long)(at_) * sizeof(GitLogOpened))
#define GIT_LOG_FILE_AT(at_) ((unsigned long long)(at_) * sizeof(GitLogFile))
#define GIT_LOG_REPOSITORY_AT(at_) ((unsigned long long)(at_) * sizeof(CliRepository))
#define GIT_LOG_DRAWN_AT(at_) ((unsigned long long)(at_) * sizeof(CliDrawn))
#define GIT_LOG_STROKE_AT(at_)                                                                                         \
    (GIT_LOG_DRAWN_AT(s_log.commits) + ((unsigned long long)(at_) * 4ULL * sizeof(unsigned int)))

typedef struct
{
    const char *text;
    unsigned int at;
    unsigned int which;
    unsigned int *under_out;
    GitLogOpened *record_out;
    int *opened_out;
    char *out;
    long long seconds;
    const char *root;
    unsigned long long now_ms;
    unsigned long long id;
    CliRepository *repository_out;
    CliDrawn *drawn_out;
} GitLogCtx;

// the repository `at`'s record read out of the repositories' cell into `repository_out`, and the commit `at`'s record
// read out of the graph's cell into `drawn_out`
EMBED_INLINE void repository_read(const GitLogCtx *args)
{
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_log.repositories_cell,
                                                            .offset = GIT_LOG_REPOSITORY_AT(args->at),
                                                            .dst = args->repository_out,
                                                            .bytes = sizeof(CliRepository)});
}

EMBED_INLINE void drawn_read(const GitLogCtx *args)
{
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_log.graph_cell,
                                                            .offset = GIT_LOG_DRAWN_AT(args->at),
                                                            .dst = args->drawn_out,
                                                            .bytes = sizeof(CliDrawn)});
}

// the first sixteen hexadecimal digits of the id `text` as a number
EMBED_INLINE unsigned long long id_number(const GitLogCtx *args)
{
    unsigned long long number = 0ULL;
    for (unsigned int at = 0u; (at < 16u) && (args->text[at] != '\0'); at += 1u)
    {
        const char digit = args->text[at];
        const unsigned int value = ((digit >= '0') && (digit <= '9'))   ? (unsigned int)(digit - '0')
                                   : ((digit >= 'a') && (digit <= 'f')) ? (unsigned int)(digit - 'a') + 10u
                                   : ((digit >= 'A') && (digit <= 'F')) ? (unsigned int)(digit - 'A') + 10u
                                                                        : 0u;
        number = (number << 4u) | value;
    }
    return number;
}

// `seconds` since 1970 written as YYYY-MM-DD on the machine's calendar into `out`, CLI_GIT_LOG_DAY bytes
EMBED_INLINE void day_write(const GitLogCtx *args)
{
    CliCivil civil;
    EMBED_CALL(cli_clock_civil, CliClockCivilCtx, .seconds = args->seconds, .civil_out = &civil);
    VerbaLinea line = {args->out, CLI_GIT_LOG_DAY, 0ULL};
    mmgr_line_decimal(&line, civil.year, 4u);
    mmgr_line_ch(&line, '-');
    mmgr_line_decimal(&line, civil.month, 2u);
    mmgr_line_ch(&line, '-');
    mmgr_line_decimal(&line, civil.day, 2u);
    (void)mmgr_line_finish(&line);
}

// the chosen repository's folder under the tree at `root`, the tree itself where the chosen one is its own, written
// into `out`, CLI_PATH_MAX_LEN bytes
EMBED_INLINE void root_write(const GitLogCtx *args)
{
    char copied[CLI_PATH_MAX_LEN];
    CliRepository chosen = {0};
    const int listed = (s_log.repositories_cell != NULL) && (s_log.chosen < s_log.repositories);
    if (listed)
    {
        EMBED_CALL(repository_read, GitLogCtx, .at = s_log.chosen, .repository_out = &chosen);
    }
    const char *const relative =
        listed ? EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_log.repositories_text,
                            .at = chosen.path, .dst = copied, .capacity = sizeof(copied))
               : "";
    if (relative[0] != '\0')
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = relative, .joined = args->out);
        return;
    }
    VerbaLinea line = {args->out, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->root);
    (void)mmgr_line_finish(&line);
}

// the id of the commit `at` of the graph copied into `out`, CLI_GIT_LOG_ID bytes
EMBED_INLINE const char *commit_id(const GitLogCtx *args)
{
    CliDrawn drawn;
    EMBED_CALL(drawn_read, GitLogCtx, .at = args->at, .drawn_out = &drawn);
    return EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_log.graph_text, .at = drawn.id,
                      .dst = args->out, .capacity = CLI_GIT_LOG_ID);
}

// the record of the opened commit `at` read out of the records' cell into `record_out`
EMBED_INLINE void opened_read(const GitLogCtx *args)
{
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_log.opened,
                                                            .offset = GIT_LOG_OPENED_AT(args->at),
                                                            .dst = args->record_out,
                                                            .bytes = sizeof(*args->record_out)});
}

// the place of the opened commit's record at the graph's place `which`: the pane's `opens` where none stands there
EMBED_INLINE unsigned int opened_at(const GitLogCtx *args)
{
    unsigned int low = 0u;
    unsigned int high = s_log.opens;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        GitLogOpened record;
        EMBED_CALL(opened_read, GitLogCtx, .at = middle, .record_out = &record);
        if (record.commit == args->which)
        {
            return middle;
        }
        low = (record.commit < args->which) ? middle + 1u : low;
        high = (record.commit < args->which) ? high : middle;
    }
    return s_log.opens;
}

// the records in the graph's order, each record's first row, and the rows counted: the branch's row, a row a commit,
// and under one whose files show a row each file, or the notice's row where it has none. The records are read whole
// into the temporary tier, ordered there and written back
EMBED_INLINE void rows_index(const GitLogCtx *args)
{
    (void)args;
    GitLog *const log = &s_log;
    const unsigned long long bytes = GIT_LOG_OPENED_AT(log->opens);
    GitLogOpened *const opened =
        (log->opens != 0u) ? g_cli_guard->temporary_buf_alloc(bytes, g_cli_git_log_owner) : NULL;
    if (opened != NULL)
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = log->opened, .offset = 0ULL, .dst = opened, .bytes = bytes});
        for (unsigned int at = 1u; at < log->opens; at += 1u)
        {
            const GitLogOpened record = opened[at];
            unsigned int put = at;
            while ((put > 0u) && (opened[put - 1u].commit > record.commit))
            {
                opened[put] = opened[put - 1u];
                put -= 1u;
            }
            opened[put] = record;
        }
    }
    // a record the graph does not list now has no row, one past every row
    unsigned int extra = 0u;
    for (unsigned int at = 0u; (opened != NULL) && (at < log->opens); at += 1u)
    {
        GitLogOpened *const record = &opened[at];
        const int listed = record->commit != CLI_GIT_GRAPH;
        record->row = listed ? 1u + record->commit + extra : 0xffffffffu;
        extra += (listed && record->shown) ? ((record->count != 0u) ? record->count : 1u) : 0u;
    }
    if (opened != NULL)
    {
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = log->opened, .offset = 0ULL, .src = opened, .bytes = bytes});
        g_cli_guard->persistent_buf_release(opened, g_cli_git_log_owner);
    }
    log->rows = log->listed ? 1u + log->commits + extra : 0u;
}

// the commit whose rows the row `at` stands in, the row's place under the commit's own row into `under_out` and, where
// its files were read, its record into `record_out` with `opened_out` 1, else 0: the commit, or CLI_GIT_GRAPH where
// the row is the branch's or stands past the commits
EMBED_INLINE unsigned int commit_of(const GitLogCtx *args)
{
    const GitLog *const log = &s_log;
    *args->under_out = 0u;
    *args->opened_out = 0;
    if ((args->at == 0u) || (args->at >= log->rows))
    {
        return CLI_GIT_GRAPH;
    }
    // the last opened commit whose first row is at or before the row
    unsigned int low = 0u;
    unsigned int high = log->opens;
    GitLogOpened record;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        EMBED_CALL(opened_read, GitLogCtx, .at = middle, .record_out = &record);
        low = (record.row <= args->at) ? middle + 1u : low;
        high = (record.row <= args->at) ? high : middle;
    }
    if (low == 0u)
    {
        return args->at - 1u;
    }
    EMBED_CALL(opened_read, GitLogCtx, .at = low - 1u, .record_out = &record);
    const unsigned int block = 1u + (record.shown ? ((record.count != 0u) ? record.count : 1u) : 0u);
    if (args->at < record.row + block)
    {
        *args->under_out = args->at - record.row;
        *args->record_out = record;
        *args->opened_out = 1;
        return record.commit;
    }
    return record.commit + 1u + (args->at - (record.row + block));
}

// the files the commit at the graph's place `which`, whose id is `text` and `id` as a number, changed, read from the
// chosen repository under the tree at `root` into a cell of their own, each path under the tree with the repository's
// folder before git's path, and the commit's record appended with its files showing
EMBED_INLINE void files_read(const GitLogCtx *args)
{
    GitLog *const log = &s_log;
    // the touched list, the repository's path and the error line for this read alone
    CliTouchedList *const touched = g_cli_guard->temporary_buf_alloc(
        sizeof(CliTouchedList) + CLI_PATH_MAX_LEN + CLI_ERR_MSG_MAX_LEN, g_cli_git_log_owner);
    if (touched == NULL)
    {
        return;
    }
    char *const root = (char *)(void *)(touched + 1);
    VerbaLinea err_msg = {root + CLI_PATH_MAX_LEN, CLI_ERR_MSG_MAX_LEN, 0ULL};
    touched->count = 0u;
    touched->text = (CliGitText){NULL, 0ULL, 0ULL, 0, 0};
    EMBED_CALL(root_write, GitLogCtx, .root = args->root, .out = root);
    if (!EMBED_CALL(cli_git_root_err_msg.touch, CliGitRootErrMsgCfg, .root = root, .err_msg = &err_msg,
                    .id = args->text, .found_touched_list_pointer = touched))
    {
        touched->count = 0u;
    }
    char folder[CLI_PATH_MAX_LEN];
    CliRepository chosen = {0};
    if (log->chosen < log->repositories)
    {
        EMBED_CALL(repository_read, GitLogCtx, .at = log->chosen, .repository_out = &chosen);
    }
    (void)EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &log->repositories_text,
                     .at = chosen.path, .dst = folder, .capacity = sizeof(folder));
    const unsigned long long folder_length = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_PATH_MAX_LEN});
    // the record's cell, laid in the temporary tier first: a GitLogFile a file, then each path under the tree with its
    // end; and the records' cell grown by one, the records before read into it
    const unsigned long long files_bytes = GIT_LOG_FILE_AT(touched->count);
    const unsigned long long text_bytes =
        touched->text.used + ((unsigned long long)touched->count * (folder_length + 1ULL));
    const unsigned long long before_bytes = GIT_LOG_OPENED_AT(log->opens);
    GitLogFile *const file = g_cli_guard->temporary_buf_alloc(files_bytes + text_bytes + 1ULL, g_cli_git_log_owner);
    GitLogOpened *const opened =
        g_cli_guard->temporary_buf_alloc(before_bytes + sizeof(GitLogOpened), g_cli_git_log_owner);
    const CarcerMovableEntry *const records =
        g_cli_guard->movable_buf_alloc(before_bytes + sizeof(GitLogOpened), g_cli_git_log_owner);
    const CarcerMovableEntry *const cell =
        (touched->count != 0u) ? g_cli_guard->movable_buf_alloc(files_bytes + text_bytes, g_cli_git_log_owner) : NULL;
    if ((file == NULL) || (opened == NULL) || (records == NULL) || ((touched->count != 0u) && (cell == NULL)))
    {
        if (cell != NULL)
        {
            (void)g_cli_guard->movable_buf_release(cell, g_cli_git_log_owner);
        }
        if (records != NULL)
        {
            (void)g_cli_guard->movable_buf_release(records, g_cli_git_log_owner);
        }
        g_cli_guard->persistent_buf_release(opened, g_cli_git_log_owner);
        g_cli_guard->persistent_buf_release(file, g_cli_git_log_owner);
        EMBED_CALL(cli_git_text.text_release, CliGitTextCfg, .text = &touched->text);
        g_cli_guard->persistent_buf_release(touched, g_cli_git_log_owner);
        return;
    }
    char *const text = (char *)(void *)(file + touched->count);
    unsigned long long used = 0ULL;
    for (unsigned int at = 0u; at < touched->count; at += 1u)
    {
        VerbaLinea line = {text + used, text_bytes - used, 0ULL};
        if (folder_length != 0ULL)
        {
            mmgr_line_put(&line, folder);
            mmgr_line_ch(&line, '/');
        }
        mmgr_line_put(&line, EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &touched->text,
                                        .at = touched->item[at].path));
        (void)mmgr_line_finish(&line);
        // the places are below the cell's bytes, which an unsigned int counts
        file[at] = (GitLogFile){(unsigned int)used,
                                (unsigned int)EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg,
                                                         .text = text + used, .at = line.at, .set = "/"),
                                touched->item[at].state};
        used += line.at + 1ULL;
    }
    if (cell != NULL)
    {
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = cell, .offset = 0ULL, .src = file, .bytes = files_bytes + text_bytes});
    }
    if (log->opens != 0u)
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = log->opened, .offset = 0ULL, .dst = opened, .bytes = before_bytes});
        (void)g_cli_guard->movable_buf_release(log->opened, g_cli_git_log_owner);
    }
    opened[log->opens] = (GitLogOpened){args->id, args->which, 0u, 1, touched->count, cell};
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
        .cell = records, .offset = 0ULL, .src = opened, .bytes = before_bytes + sizeof(GitLogOpened)});
    log->opened = records;
    log->opens += 1u;
    g_cli_guard->persistent_buf_release(opened, g_cli_git_log_owner);
    g_cli_guard->persistent_buf_release(file, g_cli_git_log_owner);
    EMBED_CALL(cli_git_text.text_release, CliGitTextCfg, .text = &touched->text);
    g_cli_guard->persistent_buf_release(touched, g_cli_git_log_owner);
}

// the opened commits found again in the graph by their ids; one the graph does not list now, a query leaving it out,
// stands at CLI_GIT_GRAPH with its files until a listing has it again
EMBED_INLINE void opened_found(const GitLogCtx *args)
{
    (void)args;
    GitLog *const log = &s_log;
    for (unsigned int record = 0u; record < log->opens; record += 1u)
    {
        GitLogOpened opened;
        EMBED_CALL(opened_read, GitLogCtx, .at = record, .record_out = &opened);
        opened.commit = CLI_GIT_GRAPH;
        for (unsigned int at = 0u; (opened.commit == CLI_GIT_GRAPH) && (at < log->commits); at += 1u)
        {
            char id[CLI_GIT_LOG_ID];
            (void)EMBED_CALL(commit_id, GitLogCtx, .at = at, .out = id);
            opened.commit = (EMBED_CALL(id_number, GitLogCtx, .text = id) == opened.id) ? at : CLI_GIT_GRAPH;
        }
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = log->opened, .offset = GIT_LOG_OPENED_AT(record), .src = &opened, .bytes = sizeof(opened)});
    }
}

// the days' cell given back, and the label and the count emptied
EMBED_INLINE void listing_released(const GitLogCtx *args)
{
    (void)args;
    GitLog *const log = &s_log;
    if (log->days != NULL)
    {
        (void)g_cli_guard->movable_buf_release(log->days, g_cli_git_log_owner);
    }
    log->days = NULL;
    log->label[0] = '\0';
    log->label_length = 0ULL;
    log->count[0] = '\0';
    log->count_length = 0ULL;
}

// the repositories' and the graph's cells given back, none listed
EMBED_INLINE void lists_released(const GitLogCtx *args)
{
    (void)args;
    GitLog *const log = &s_log;
    if (log->repositories_cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(log->repositories_cell, g_cli_git_log_owner);
    }
    if (log->graph_cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(log->graph_cell, g_cli_git_log_owner);
    }
    log->repositories_cell = NULL;
    log->repositories = 0u;
    log->graph_cell = NULL;
    log->commits = 0u;
    log->strokes = 0u;
}

// the repositories `text` and the graph `out` read by git for the listing written into movable cells sized to them,
// the repositories a record each, the graph its commits then its strokes: 1, or 0 where the cells cannot meet them
EMBED_INLINE int lists_stored(const GitLogCtx *args)
{
    GitLog *const log = &s_log;
    const CliRepositories *const repositories = (const CliRepositories *)(const void *)args->text;
    const CliGraph *const graph = (const CliGraph *)(const void *)args->out;
    const unsigned long long repositories_bytes = GIT_LOG_REPOSITORY_AT(repositories->count);
    const unsigned long long drawn_bytes = GIT_LOG_DRAWN_AT(graph->count);
    const unsigned long long strokes_bytes = (unsigned long long)graph->strokes * 4ULL * sizeof(unsigned int);
    log->repositories_cell =
        (repositories->count != 0u) ? g_cli_guard->movable_buf_alloc(repositories_bytes, g_cli_git_log_owner) : NULL;
    log->graph_cell =
        (graph->count != 0u) ? g_cli_guard->movable_buf_alloc(drawn_bytes + strokes_bytes, g_cli_git_log_owner) : NULL;
    if (((repositories->count != 0u) && (log->repositories_cell == NULL)) ||
        ((graph->count != 0u) && (log->graph_cell == NULL)))
    {
        EMBED_CALL(lists_released, GitLogCtx, .at = 0u);
        return 0;
    }
    if (log->repositories_cell != NULL)
    {
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = log->repositories_cell, .offset = 0ULL, .src = repositories->item, .bytes = repositories_bytes});
    }
    if (log->graph_cell != NULL)
    {
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = log->graph_cell, .offset = 0ULL, .src = graph->item, .bytes = drawn_bytes});
    }
    if ((log->graph_cell != NULL) && (strokes_bytes != 0ULL))
    {
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = log->graph_cell, .offset = drawn_bytes, .src = graph->stroke, .bytes = strokes_bytes});
    }
    log->repositories = repositories->count;
    log->commits = graph->count;
    log->strokes = graph->strokes;
    return 1;
}

// the listing read for the tree at `root`: the repositories and the graph read anew, the listing's texts, the label
// `name: branch` and the commits' count in the pane's bytes and each commit's day in a movable cell sized to them,
// every opened commit found again and the rows indexed: 1, or 0 where the cells cannot meet it
EMBED_INLINE int listing_read(const GitLogCtx *args)
{
    GitLog *const log = &s_log;
    EMBED_CALL(listing_released, GitLogCtx, .at = 0u);
    EMBED_CALL(lists_released, GitLogCtx, .at = 0u);
    // the lists' texts standing between the frames, released before each listing; the lists themselves read into
    // cells of the listing alone, then written into their movable cells
    EMBED_CALL(cli_git_text.text_release, CliGitTextCfg, .text = &log->repositories_text);
    EMBED_CALL(cli_git_text.text_release, CliGitTextCfg, .text = &log->graph_text);
    log->repositories_text.standing = 1;
    log->graph_text.standing = 1;
    CliRepositories *const repositories =
        g_cli_guard->temporary_buf_alloc(sizeof(CliRepositories), g_cli_git_log_owner);
    CliGraph *const graph = g_cli_guard->temporary_buf_alloc(sizeof(CliGraph), g_cli_git_log_owner);
    char *const root = g_cli_guard->temporary_buf_alloc(CLI_PATH_MAX_LEN, g_cli_git_log_owner);
    if ((repositories == NULL) || (graph == NULL) || (root == NULL))
    {
        g_cli_guard->persistent_buf_release(root, g_cli_git_log_owner);
        g_cli_guard->persistent_buf_release(graph, g_cli_git_log_owner);
        g_cli_guard->persistent_buf_release(repositories, g_cli_git_log_owner);
        return 0;
    }
    repositories->count = 0u;
    repositories->text = log->repositories_text;
    EMBED_CALL(cli_git_root_found_repositories.repositories, CliGitRootFoundRepositoriesCfg, .root = args->root,
               .found_repositories_pointer = repositories);
    log->repositories_text = repositories->text;
    log->chosen = (log->chosen < repositories->count) ? log->chosen : 0u;
    // the chosen repository's folder, written out of the repositories read for this listing
    char relative[CLI_PATH_MAX_LEN];
    const char *const folder =
        (repositories->count != 0u)
            ? EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &log->repositories_text,
                         .at = repositories->item[log->chosen].path, .dst = relative, .capacity = sizeof(relative))
            : "";
    if (folder[0] != '\0')
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = folder, .joined = root);
    }
    else
    {
        VerbaLinea root_line = {root, CLI_PATH_MAX_LEN, 0ULL};
        mmgr_line_put(&root_line, args->root);
        (void)mmgr_line_finish(&root_line);
    }
    graph->count = 0u;
    graph->strokes = 0u;
    graph->text = log->graph_text;
    // the query copied out of the typed text's cell for git
    char query[CLI_GIT_LOG_QUERY_MAX];
    (void)EMBED_CALL(cli_typed.copy, CliTypedCfg, .typed = &log->query, .dst = query, .capacity = sizeof(query));
    EMBED_CALL(cli_git_root_query.graph, CliGitRootQueryCfg, .root = root, .query = query, .authors = log->authors,
               .found_graph_pointer = graph);
    log->graph_text = graph->text;
    g_cli_guard->persistent_buf_release(root, g_cli_git_log_owner);
    // the listing's texts: the label `name: branch`, the commits' count, and each commit's day, sized to them
    const CliRepository *const chosen = (repositories->count != 0u) ? &repositories->item[log->chosen] : NULL;
    char name_copied[CLI_NAME_MAX_LEN];
    char branch_copied[CLI_NAME_MAX_LEN];
    const char *const name = (chosen != NULL) ? EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                                           .text_read = &log->repositories_text, .at = chosen->name,
                                                           .dst = name_copied, .capacity = sizeof(name_copied))
                                              : "";
    const char *const branch =
        ((chosen != NULL) && chosen->has_branch)
            ? EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &log->repositories_text,
                         .at = chosen->branch, .dst = branch_copied, .capacity = sizeof(branch_copied))
            : NULL;
    const unsigned int commits = graph->count;
    // the days laid in the temporary tier, then written into their movable cell whole
    const unsigned long long days_bytes = (unsigned long long)commits * CLI_GIT_LOG_DAY;
    char *const days = (commits != 0u) ? g_cli_guard->temporary_buf_alloc(days_bytes, g_cli_git_log_owner) : NULL;
    log->days = (commits != 0u) ? g_cli_guard->movable_buf_alloc(days_bytes, g_cli_git_log_owner) : NULL;
    const int stored = ((commits == 0u) || ((days != NULL) && (log->days != NULL))) &&
                       EMBED_CALL(lists_stored, GitLogCtx, .text = (const char *)(const void *)repositories,
                                  .out = (char *)(void *)graph);
    if (!stored)
    {
        g_cli_guard->persistent_buf_release(days, g_cli_git_log_owner);
        g_cli_guard->persistent_buf_release(graph, g_cli_git_log_owner);
        g_cli_guard->persistent_buf_release(repositories, g_cli_git_log_owner);
        EMBED_CALL(listing_released, GitLogCtx, .at = 0u);
        return 0;
    }
    VerbaLinea label = {log->label, sizeof(log->label), 0ULL};
    mmgr_line_put(&label, name);
    if (branch != NULL)
    {
        mmgr_line_put(&label, ": ");
        mmgr_line_put(&label, branch);
    }
    log->label_length = mmgr_line_finish(&label) ? label.at : 0ULL;
    VerbaLinea count = {log->count, sizeof(log->count), 0ULL};
    mmgr_line_decimal(&count, commits, 1u);
    mmgr_line_put(&count, (commits == 1u) ? " commit " : GIT_LOG_COMMITS);
    mmgr_line_put(&count, (log->query.length != 0ULL) ? GIT_LOG_FOUND : "shown");
    log->count_length = mmgr_line_finish(&count) ? count.at : 0ULL;
    for (unsigned int at = 0u; at < commits; at += 1u)
    {
        EMBED_CALL(day_write, GitLogCtx, .seconds = graph->item[at].when,
                   .out = days + ((unsigned long long)at * CLI_GIT_LOG_DAY));
    }
    if (commits != 0u)
    {
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = log->days, .offset = 0ULL, .src = days, .bytes = days_bytes});
        g_cli_guard->persistent_buf_release(days, g_cli_git_log_owner);
    }
    g_cli_guard->persistent_buf_release(graph, g_cli_git_log_owner);
    g_cli_guard->persistent_buf_release(repositories, g_cli_git_log_owner);
    log->listed = 1;
    EMBED_CALL(opened_found, GitLogCtx, .at = 0u);
    EMBED_CALL(rows_index, GitLogCtx, .at = 0u);
    return 1;
}

// the graph listed anew for the tree at `root`, the stale mark cleared and the rest ended; where there is no tree or
// the listing cannot be read, the listing's cells released, the texts, the repositories, the graph and every opened
// commit's files, no rows standing
EMBED_INLINE void list(const GitLogCtx *args)
{
    GitLog *const log = &s_log;
    EMBED_ATOMIC_STORE(&s_stale, 0u);
    log->waits = 0;
    log->listed = 0;
    log->rows = 0u;
    if ((args->root != NULL) && EMBED_CALL(listing_read, GitLogCtx, .root = args->root))
    {
        return;
    }
    EMBED_CALL(listing_released, GitLogCtx, .at = 0u);
    EMBED_CALL(lists_released, GitLogCtx, .at = 0u);
    EMBED_CALL(cli_git_text.text_release, CliGitTextCfg, .text = &log->repositories_text);
    EMBED_CALL(cli_git_text.text_release, CliGitTextCfg, .text = &log->graph_text);
    for (unsigned int at = 0u; at < log->opens; at += 1u)
    {
        GitLogOpened record;
        EMBED_CALL(opened_read, GitLogCtx, .at = at, .record_out = &record);
        if (record.cell != NULL)
        {
            (void)g_cli_guard->movable_buf_release(record.cell, g_cli_git_log_owner);
        }
    }
    if (log->opened != NULL)
    {
        (void)g_cli_guard->movable_buf_release(log->opened, g_cli_git_log_owner);
    }
    log->opened = NULL;
    log->opens = 0u;
    log->listed = 0;
    log->rows = 0u;
}

// a change to the query or the settings at `now_ms`: the rest begins
EMBED_INLINE void changed(const GitLogCtx *args)
{
    s_log.waits = 1;
    s_log.changed_ms = args->now_ms;
}

void cli_git_log_begin(const CliGitLogCfg *args)
{
    s_woken = args->woken;
    char saved_text[16];
    const char *const saved = cli_state_entry_value(args->entries, GIT_LOG_AUTHORS_KEY, saved_text, sizeof(saved_text));
    s_log.authors = (saved != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = saved, .other = "true", .cap = 8ULL});
}

void cli_git_log_type(const CliGitLogCfg *args)
{
    const unsigned long long before = s_log.query.length;
    EMBED_CALL(cli_typed.put, CliTypedCfg, .typed = &s_log.query, .text = args->text, .size = args->size,
               .most = CLI_GIT_LOG_QUERY_MAX, .owner = g_cli_git_log_owner);
    if (s_log.query.length != before)
    {
        EMBED_CALL(changed, GitLogCtx, .now_ms = args->now_ms);
    }
}

int cli_git_log_key(const CliGitLogCfg *args)
{
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Backspace", .cap = 16ULL}))
    {
        EMBED_CALL(cli_typed.erase, CliTypedCfg, .typed = &s_log.query, .owner = g_cli_git_log_owner);
        EMBED_CALL(changed, GitLogCtx, .now_ms = args->now_ms);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Enter", .cap = 16ULL}))
    {
        // the rest is over: the next step reads the graph
        EMBED_CALL(changed, GitLogCtx, .now_ms = args->now_ms - CLI_GIT_LOG_REST_MS);
        return 1;
    }
    return 0;
}

void cli_git_log_option(const CliGitLogCfg *args)
{
    if ((args->which != CLI_GIT_LOG_REPO) || (s_log.repositories < 2u))
    {
        return;
    }
    s_log.chosen = (s_log.chosen + 1u < s_log.repositories) ? s_log.chosen + 1u : 0u;
    EMBED_CALL(list, GitLogCtx, .root = args->root);
}

void cli_git_log_authors_set(const CliGitLogCfg *args)
{
    s_log.authors = args->on;
    // the folder it is saved in and the error line, for the save alone
    char *const folder = g_cli_guard->temporary_buf_alloc(CLI_PATH_MAX_LEN + CLI_ERR_MSG_MAX_LEN, g_cli_git_log_owner);
    if (folder != NULL)
    {
        VerbaLinea err_msg = {folder + CLI_PATH_MAX_LEN, CLI_ERR_MSG_MAX_LEN, 0ULL};
        if (EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder))
        {
            (void)cli_state_value_save(&(CliStateValueCfg){.folder = folder,
                                                           .key = GIT_LOG_AUTHORS_KEY,
                                                           .value = args->on ? "true" : "false",
                                                           .err_msg = &err_msg});
        }
        g_cli_guard->persistent_buf_release(folder, g_cli_git_log_owner);
    }
    EMBED_CALL(changed, GitLogCtx, .now_ms = args->now_ms - CLI_GIT_LOG_REST_MS);
}

int cli_git_log_authors(const CliGitLogCfg *args)
{
    (void)args;
    return s_log.authors;
}

unsigned long long cli_git_log_query(const CliGitLogCfg *args)
{
    return EMBED_CALL(cli_typed.copy, CliTypedCfg, .typed = &s_log.query, .dst = args->text_out,
                      .capacity = CLI_GIT_LOG_QUERY_MAX);
}

const char *cli_git_log_repo(const CliGitLogCfg *args)
{
    *args->count_out = s_log.listed ? s_log.repositories : 0u;
    return s_log.label;
}

void cli_git_log_relist(const CliGitLogCfg *args)
{
    EMBED_CALL(list, GitLogCtx, .root = args->root);
}

int cli_git_log_step(const CliGitLogCfg *args)
{
    if (!args->on)
    {
        return 0;
    }
    const int stale = (EMBED_ATOMIC_EXCHANGE_32(&s_stale, 0u) != 0u) && s_log.listed;
    if (stale || (s_log.waits && (args->now_ms - s_log.changed_ms >= CLI_GIT_LOG_REST_MS)))
    {
        EMBED_CALL(list, GitLogCtx, .root = args->root);
        return 1;
    }
    return 0;
}

unsigned long long cli_git_log_deadline_ms(const CliGitLogCfg *args)
{
    (void)args;
    return s_log.waits ? s_log.changed_ms + CLI_GIT_LOG_REST_MS : 0ULL;
}

void cli_git_log_stale(const CliGitLogCfg *args)
{
    (void)args;
    EMBED_ATOMIC_STORE(&s_stale, 1u);
    if (s_woken != NULL)
    {
        s_woken();
    }
}

unsigned int cli_git_log_rows(const CliGitLogCfg *args)
{
    (void)args;
    return s_log.rows;
}

int cli_git_log_row(const CliGitLogCfg *args)
{
    const GitLog *const log = &s_log;
    CliGitLogRow *const row = args->row;
    memor.set(&(MemoriaCfg){.dst = row, .val = 0, .bytes = sizeof(*row)});
    if (args->at >= log->rows)
    {
        return 0;
    }
    if (args->at == 0u)
    {
        CliRepository chosen = {0};
        const int listed = (log->repositories != 0u) && (log->chosen < log->repositories);
        if (listed)
        {
            EMBED_CALL(repository_read, GitLogCtx, .at = log->chosen, .repository_out = &chosen);
        }
        row->type = CLI_GIT_LOG_ROW_BRANCH;
        row->text =
            (listed && chosen.has_branch)
                ? EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &log->repositories_text,
                             .at = chosen.branch, .dst = row->text_copied, .capacity = CLI_GIT_LOG_TEXT)
                : GIT_LOG_NO_BRANCH;
        row->text_length = cellul.len(&(CatenaFinitaCfg){.src = row->text, .cap = CLI_NAME_MAX_LEN});
        memor.cpy(&(MemoriaCfg){.dst = row->second_copied, .src = log->count, .bytes = log->count_length + 1ULL});
        row->second = row->second_copied;
        row->second_length = log->count_length;
        return 1;
    }
    unsigned int under = 0u;
    GitLogOpened record;
    int opened = 0;
    const unsigned int commit = EMBED_CALL(commit_of, GitLogCtx, .at = args->at, .under_out = &under,
                                           .record_out = &record, .opened_out = &opened);
    CliDrawn drawn_read_out;
    EMBED_CALL(drawn_read, GitLogCtx, .at = commit, .drawn_out = &drawn_read_out);
    const CliDrawn *const drawn = &drawn_read_out;
    row->id = EMBED_CALL(commit_id, GitLogCtx, .at = commit, .out = row->id_copied);
    if (under == 0u)
    {
        row->type = CLI_GIT_LOG_ROW_COMMIT;
        row->text = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &log->graph_text,
                               .at = drawn->subject, .dst = row->text_copied, .capacity = CLI_GIT_LOG_TEXT);
        row->text_length = cellul.len(&(CatenaFinitaCfg){.src = row->text, .cap = CLI_GIT_LOG_TEXT});
        if (log->authors)
        {
            row->second = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &log->graph_text,
                                     .at = drawn->author, .dst = row->second_copied, .capacity = CLI_NAME_MAX_LEN);
            row->second_length = cellul.len(&(CatenaFinitaCfg){.src = row->second, .cap = CLI_NAME_MAX_LEN});
        }
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = log->days,
                                                                .offset = (unsigned long long)commit * CLI_GIT_LOG_DAY,
                                                                .dst = row->day_copied,
                                                                .bytes = CLI_GIT_LOG_DAY});
        row->day = row->day_copied;
        row->open = opened && record.shown;
        row->merge = drawn->parents > 1u;
        row->lane_at = drawn->lane_at;
        row->color = drawn->color;
        // the commit's strokes copied out of the graph's cell into the row's own, cut to CLI_GIT_LOG_ROW_STROKES
        row->strokes = (drawn->stroke_count < CLI_GIT_LOG_ROW_STROKES) ? drawn->stroke_count : CLI_GIT_LOG_ROW_STROKES;
        if (row->strokes != 0u)
        {
            (void)g_cli_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = log->graph_cell,
                                    .offset = GIT_LOG_STROKE_AT(drawn->first_stroke),
                                    .dst = row->stroke_copied,
                                    .bytes = (unsigned long long)row->strokes * 4ULL * sizeof(unsigned int)});
        }
        row->stroke = row->stroke_copied;
        row->refs = drawn->refs;
        row->across = drawn->across;
        return 1;
    }
    if (record.count == 0u)
    {
        row->type = CLI_GIT_LOG_ROW_NONE;
        row->text = GIT_LOG_NONE;
        row->text_length = sizeof(GIT_LOG_NONE) - 1ULL;
        return 1;
    }
    // the file's record, then its path copied out of the commit's cell into the row's text, its end measured first
    GitLogFile file;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = record.cell, .offset = GIT_LOG_FILE_AT(under - 1u), .dst = &file, .bytes = sizeof(file)});
    const unsigned long long path_at = GIT_LOG_FILE_AT(record.count) + file.path;
    const unsigned long long measured =
        g_cli_guard->movable_buf_measure(&(CarcerMovableCfg){.cell = record.cell, .offset = path_at});
    const unsigned long long path_length = (measured < CLI_PATH_MAX_LEN - 1ULL) ? measured : CLI_PATH_MAX_LEN - 1ULL;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = record.cell, .offset = path_at, .dst = row->text_copied, .bytes = path_length});
    row->text_copied[path_length] = '\0';
    row->type = CLI_GIT_LOG_ROW_FILE;
    row->path = row->text_copied;
    row->state = file.state;
    row->text = row->path + ((file.name < path_length) ? file.name : path_length);
    row->text_length = path_length - ((file.name < path_length) ? file.name : path_length);
    row->second = row->path;
    row->second_length = (file.name > 0u) ? file.name - 1u : 0u;
    return 1;
}

int cli_git_log_ref(const CliGitLogCfg *args)
{
    const GitLog *const log = &s_log;
    CliGitLogRef *const ref = args->ref;
    memor.set(&(MemoriaCfg){.dst = ref, .val = 0, .bytes = sizeof(*ref)});
    unsigned int under = 0u;
    GitLogOpened record;
    int opened = 0;
    const unsigned int commit = EMBED_CALL(commit_of, GitLogCtx, .at = args->at, .under_out = &under,
                                           .record_out = &record, .opened_out = &opened);
    CliDrawn drawn = {0};
    if (commit != CLI_GIT_GRAPH)
    {
        EMBED_CALL(drawn_read, GitLogCtx, .at = commit, .drawn_out = &drawn);
    }
    if ((commit == CLI_GIT_GRAPH) || (under != 0u) || (args->which >= drawn.refs))
    {
        return 0;
    }
    // a name as git writes it: `HEAD -> ` before the branch HEAD stands on, `HEAD` alone where it stands on none,
    // and `tag: ` before a tag
    const char *name = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &log->graph_text,
                                  .at = drawn.ref[args->which], .dst = ref->copied, .capacity = CLI_NAME_MAX_LEN);
    if (cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "HEAD -> ", .cap = CLI_NAME_MAX_LEN}))
    {
        ref->head = 1;
        name += 8;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "HEAD", .cap = CLI_NAME_MAX_LEN}))
    {
        ref->head = 1;
    }
    else if (cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "tag: ", .cap = CLI_NAME_MAX_LEN}))
    {
        ref->tag = 1;
        name += 5;
    }
    ref->name = name;
    ref->length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_NAME_MAX_LEN});
    return 1;
}

// the commit at the graph's place `which` opened or closed, its files read from the tree at `root` as it first opens,
// and the rows indexed again
EMBED_INLINE void commit_toggle(const GitLogCtx *args)
{
    const unsigned int at = EMBED_CALL(opened_at, GitLogCtx, .which = args->which);
    if (at < s_log.opens)
    {
        GitLogOpened record;
        EMBED_CALL(opened_read, GitLogCtx, .at = at, .record_out = &record);
        record.shown = !record.shown;
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = s_log.opened, .offset = GIT_LOG_OPENED_AT(at), .src = &record, .bytes = sizeof(record)});
    }
    else
    {
        char id[CLI_GIT_LOG_ID];
        (void)EMBED_CALL(commit_id, GitLogCtx, .at = args->which, .out = id);
        EMBED_CALL(files_read, GitLogCtx, .which = args->which, .text = id,
                   .id = EMBED_CALL(id_number, GitLogCtx, .text = id), .root = args->root);
    }
    EMBED_CALL(rows_index, GitLogCtx, .at = 0u);
}

void cli_git_log_toggle(const CliGitLogCfg *args)
{
    unsigned int under = 0u;
    GitLogOpened record;
    int opened = 0;
    const unsigned int commit = EMBED_CALL(commit_of, GitLogCtx, .at = args->at, .under_out = &under,
                                           .record_out = &record, .opened_out = &opened);
    if ((commit != CLI_GIT_GRAPH) && (under == 0u))
    {
        EMBED_CALL(commit_toggle, GitLogCtx, .which = commit, .root = args->root);
    }
}

int cli_git_log_toggle_id(const CliGitLogCfg *args)
{
    for (unsigned int at = 0u; s_log.listed && (at < s_log.commits); at += 1u)
    {
        char id[CLI_GIT_LOG_ID];
        (void)EMBED_CALL(commit_id, GitLogCtx, .at = at, .out = id);
        if (cellul.eq(&(CatenaFinitaCfg){.src = id, .other = args->name, .cap = CLI_NAME_MAX_LEN}))
        {
            EMBED_CALL(commit_toggle, GitLogCtx, .which = at, .root = args->root);
            return 1;
        }
    }
    return 0;
}

int cli_git_log_message(const CliGitLogCfg *args)
{
    if (!s_log.listed || (args->root == NULL))
    {
        return 0;
    }
    // the repository's path and the error line, for the read alone
    char *const root = g_cli_guard->temporary_buf_alloc(CLI_PATH_MAX_LEN + CLI_ERR_MSG_MAX_LEN, g_cli_git_log_owner);
    if (root == NULL)
    {
        return 0;
    }
    VerbaLinea err_msg = {root + CLI_PATH_MAX_LEN, CLI_ERR_MSG_MAX_LEN, 0ULL};
    EMBED_CALL(root_write, GitLogCtx, .root = args->root, .out = root);
    const char *const request[4] = {"log", "-1", "--format=%B", args->name};
    const int read = EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = root,
                                .out = args->out, .count = 4u, .err_msg = &err_msg, .request = request);
    g_cli_guard->persistent_buf_release(root, g_cli_git_log_owner);
    return read;
}

int cli_git_log_menu(const CliGitLogCfg *args)
{
    CliGitLogRow row;
    if (!EMBED_CALL(cli_git_log.row, CliGitLogCfg, .at = args->at, .row = &row) ||
        ((row.type != CLI_GIT_LOG_ROW_COMMIT) && (row.type != CLI_GIT_LOG_ROW_FILE)))
    {
        return 0;
    }
    // the rows run with the commit's id or the file's path, which stand in the cells while the menu is open
    cli_menus_own_begin();
    if (row.type == CLI_GIT_LOG_ROW_FILE)
    {
        cli_menus_own_row(&(CliItem){"Copy Path", "file-copy-path", "", row.path, "", "", 0, 0, 0, 0, 0u});
        return 1;
    }
    cli_menus_own_row(
        &(CliItem){row.open ? "Close Files" : "Open Files", "commit-files", "", row.id, "", "", 0, 0, 0, 0, 0u});
    cli_menus_own_line();
    cli_menus_own_row(&(CliItem){"Copy Commit ID", "commit-copy-id", "", row.id, "", "", 0, 0, 0, 0, 0u});
    cli_menus_own_row(&(CliItem){"Copy Commit Message", "commit-copy-message", "", row.id, "", "", 0, 0, 0, 0, 0u});
    return 1;
}

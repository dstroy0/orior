// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_log.c: the Git pane's rows. The query, the authors setting and the repository chosen stand here; a change marks
// the rest's start, and the step that finds the rest passed, or the watch's mark that the graph is stale, lists the
// tree's repositories and reads the chosen one's graph through git_graph.c into cells that stand until the next
// listing, every row pointing into them. The label `name: branch`, the commits' count as text and each commit's day
// are written once a listing into one cell sized to them. A commit opened has the files it changed read once through
// git_lists.c into a cell of its own with their paths under the tree, and is found again by its id as the graph is
// listed again
#include "git.h"

#include "../command_line/cells.h"
#include "../command_line/commands.h"
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
// files show; and its files, in a cell of its own with their paths' text after them
typedef struct
{
    unsigned long long id;
    unsigned int commit;
    unsigned int row;
    int shown;
    unsigned int count;
    GitLogFile *file;
    const char *text;
} GitLogOpened;

// The pane's data: the query, whether a change waits and since when, the authors setting, whether rows stand, the
// repositories as git lists them and the one chosen, the graph as git_graph.c lays it, the listing's cell with the
// label, the commits' count as text and each commit's day, the rows counted, and the commits whose files were read, in
// a cell sized to their count, in the graph's order
typedef struct
{
    CliTyped query;
    int waits;
    unsigned long long changed_ms;
    int authors;
    int listed;
    CliRepositories *repositories;
    unsigned int chosen;
    CliGraph *graph;
    char *listing;
    const char *label;
    unsigned long long label_length;
    const char *count;
    unsigned long long count_length;
    const char *day;
    unsigned int rows;
    GitLogOpened *opened;
    unsigned int opens;
} GitLog;

static GitLog s_log;
static void (*s_woken)(void);
static embed_atomic_u32 s_stale;

typedef struct
{
    const char *text;
    unsigned int at;
    unsigned int which;
    unsigned int *under_out;
    GitLogOpened **record_out;
    char *out;
    long long seconds;
    const char *root;
    unsigned long long now_ms;
    unsigned long long id;
} GitLogCtx;

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
    const CliRepositories *const repositories = s_log.repositories;
    const char *const relative =
        ((repositories != NULL) && (s_log.chosen < repositories->count))
            ? EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &repositories->text,
                         .at = repositories->item[s_log.chosen].path)
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

// the id of the commit `at` of the graph
EMBED_INLINE const char *commit_id(const GitLogCtx *args)
{
    return EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_log.graph->text,
                      .at = s_log.graph->item[args->at].id);
}

// the record of the opened commit at the graph's place `which`: NULL where none stands there
EMBED_INLINE GitLogOpened *opened_at(const GitLogCtx *args)
{
    unsigned int low = 0u;
    unsigned int high = s_log.opens;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (s_log.opened[middle].commit == args->which)
        {
            return &s_log.opened[middle];
        }
        low = (s_log.opened[middle].commit < args->which) ? middle + 1u : low;
        high = (s_log.opened[middle].commit < args->which) ? high : middle;
    }
    return NULL;
}

// the records in the graph's order, each record's first row, and the rows counted: the branch's row, a row a commit,
// and under one whose files show a row each file, or the notice's row where it has none
EMBED_INLINE void rows_index(const GitLogCtx *args)
{
    (void)args;
    GitLog *const log = &s_log;
    for (unsigned int at = 1u; at < log->opens; at += 1u)
    {
        const GitLogOpened record = log->opened[at];
        unsigned int put = at;
        while ((put > 0u) && (log->opened[put - 1u].commit > record.commit))
        {
            log->opened[put] = log->opened[put - 1u];
            put -= 1u;
        }
        log->opened[put] = record;
    }
    // a record the graph does not list now has no row, one past every row
    unsigned int extra = 0u;
    for (unsigned int at = 0u; at < log->opens; at += 1u)
    {
        GitLogOpened *const record = &log->opened[at];
        const int listed = record->commit != CLI_GIT_GRAPH;
        record->row = listed ? 1u + record->commit + extra : 0xffffffffu;
        extra += (listed && record->shown) ? ((record->count != 0u) ? record->count : 1u) : 0u;
    }
    log->rows = log->listed ? 1u + log->graph->count + extra : 0u;
}

// the commit whose rows the row `at` stands in, the row's place under the commit's own row into `under_out` and its
// record into `record_out` where its files were read, NULL otherwise: the commit, or CLI_GIT_GRAPH where the row is
// the branch's or stands past the commits
EMBED_INLINE unsigned int commit_of(const GitLogCtx *args)
{
    const GitLog *const log = &s_log;
    *args->under_out = 0u;
    *args->record_out = NULL;
    if ((args->at == 0u) || (args->at >= log->rows))
    {
        return CLI_GIT_GRAPH;
    }
    // the last opened commit whose first row is at or before the row
    unsigned int low = 0u;
    unsigned int high = log->opens;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        low = (log->opened[middle].row <= args->at) ? middle + 1u : low;
        high = (log->opened[middle].row <= args->at) ? high : middle;
    }
    if (low == 0u)
    {
        return args->at - 1u;
    }
    GitLogOpened *const record = &log->opened[low - 1u];
    const unsigned int block = 1u + (record->shown ? ((record->count != 0u) ? record->count : 1u) : 0u);
    if (args->at < record->row + block)
    {
        *args->under_out = args->at - record->row;
        *args->record_out = record;
        return record->commit;
    }
    return record->commit + 1u + (args->at - (record->row + block));
}

// the records' cells released, no commit's files read
EMBED_INLINE void opened_release(const GitLogCtx *args)
{
    (void)args;
    for (unsigned int at = 0u; at < s_log.opens; at += 1u)
    {
        g_cli_cells->persistent_buf_release(s_log.opened[at].file, &g_cli_git_log_cells);
    }
    g_cli_cells->persistent_buf_release(s_log.opened, &g_cli_git_log_cells);
    s_log.opened = NULL;
    s_log.opens = 0u;
}

// the files the commit at the graph's place `which`, whose id is `text` and `id` as a number, changed, read from the
// chosen repository under the tree at `root` into a cell of their own, each path under the tree with the repository's
// folder before git's path, and the commit's record appended with its files showing
EMBED_INLINE void files_read(const GitLogCtx *args)
{
    GitLog *const log = &s_log;
    // the touched list, the repository's path and the error line for this read alone
    CliTouchedList *const touched = g_cli_cells->temporary_buf_alloc(
        sizeof(CliTouchedList) + CLI_PATH_MAX_LEN + CLI_ERR_MSG_MAX_LEN, &g_cli_git_log_cells);
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
    const char *const folder =
        EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &log->repositories->text,
                   .at = log->repositories->item[log->chosen].path);
    const unsigned long long folder_length = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_PATH_MAX_LEN});
    // the record's cell: a GitLogFile a file, then each path under the tree with its end
    const unsigned long long files_bytes = (unsigned long long)touched->count * sizeof(GitLogFile);
    const unsigned long long text_bytes =
        touched->text.used + ((unsigned long long)touched->count * (folder_length + 1ULL));
    GitLogFile *const file = g_cli_cells->persistent_buf_alloc(files_bytes + text_bytes, &g_cli_git_log_cells);
    GitLogOpened *const opened = g_cli_cells->persistent_buf_alloc(
        ((unsigned long long)log->opens + 1ULL) * sizeof(GitLogOpened), &g_cli_git_log_cells);
    if ((file == NULL) || (opened == NULL))
    {
        g_cli_cells->persistent_buf_release(file, &g_cli_git_log_cells);
        g_cli_cells->persistent_buf_release(opened, &g_cli_git_log_cells);
        g_cli_cells->persistent_buf_release(touched->text.bytes, &g_cli_git_cells);
        g_cli_cells->persistent_buf_release(touched, &g_cli_git_log_cells);
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
    if (log->opens != 0u)
    {
        memor.cpy(&(MemoriaCfg){.dst = opened, .src = log->opened, .bytes = (size_t)log->opens * sizeof(GitLogOpened)});
    }
    g_cli_cells->persistent_buf_release(log->opened, &g_cli_git_log_cells);
    opened[log->opens] = (GitLogOpened){args->id, args->which, 0u, 1, touched->count, file, text};
    log->opened = opened;
    log->opens += 1u;
    g_cli_cells->persistent_buf_release(touched->text.bytes, &g_cli_git_cells);
    g_cli_cells->persistent_buf_release(touched, &g_cli_git_log_cells);
}

// the listing's cells released: the repositories, the graph, the listing's texts and every opened commit's files
EMBED_INLINE void listing_release(const GitLogCtx *args)
{
    (void)args;
    GitLog *const log = &s_log;
    g_cli_cells->persistent_buf_release(log->listing, &g_cli_git_log_cells);
    log->listing = NULL;
    log->label = NULL;
    log->label_length = 0ULL;
    log->count = NULL;
    log->count_length = 0ULL;
    log->day = NULL;
    if (log->repositories != NULL)
    {
        g_cli_cells->persistent_buf_release(log->repositories->text.bytes, &g_cli_git_cells);
        g_cli_cells->persistent_buf_release(log->repositories, &g_cli_git_log_cells);
        log->repositories = NULL;
    }
    if (log->graph != NULL)
    {
        g_cli_cells->persistent_buf_release(log->graph->text.bytes, &g_cli_git_cells);
        g_cli_cells->persistent_buf_release(log->graph, &g_cli_git_log_cells);
        log->graph = NULL;
    }
    EMBED_CALL(opened_release, GitLogCtx, .at = 0u);
    log->listed = 0;
    log->rows = 0u;
}

// the opened commits found again in the graph by their ids; one the graph does not list now, a query leaving it out,
// stands at CLI_GIT_GRAPH with its files until a listing has it again
EMBED_INLINE void opened_found(const GitLogCtx *args)
{
    (void)args;
    GitLog *const log = &s_log;
    for (unsigned int at = 0u; at < log->opens; at += 1u)
    {
        log->opened[at].commit = CLI_GIT_GRAPH;
    }
    for (unsigned int at = 0u; (log->opens != 0u) && (at < log->graph->count); at += 1u)
    {
        const unsigned long long number =
            EMBED_CALL(id_number, GitLogCtx, .text = EMBED_CALL(commit_id, GitLogCtx, .at = at));
        for (unsigned int record = 0u; record < log->opens; record += 1u)
        {
            log->opened[record].commit = (log->opened[record].id == number) ? at : log->opened[record].commit;
        }
    }
}

// the tree at `root` listed: its repositories, the chosen one's graph for the query, the listing's texts written, the
// opened commits found again, and the rows indexed; nothing listed where `root` is NULL
EMBED_INLINE void list(const GitLogCtx *args)
{
    GitLog *const log = &s_log;
    EMBED_ATOMIC_STORE(&s_stale, 0u);
    log->waits = 0;
    log->listed = 0;
    log->rows = 0u;
    if (args->root == NULL)
    {
        EMBED_CALL(listing_release, GitLogCtx, .at = 0u);
        return;
    }
    g_cli_cells->persistent_buf_release(log->listing, &g_cli_git_log_cells);
    log->listing = NULL;
    // the lists' cells, taken at the first listing, their texts' cells released before each listing after
    if (log->repositories != NULL)
    {
        g_cli_cells->persistent_buf_release(log->repositories->text.bytes, &g_cli_git_cells);
    }
    else
    {
        log->repositories = g_cli_cells->persistent_buf_alloc(sizeof(CliRepositories), &g_cli_git_log_cells);
    }
    if (log->graph != NULL)
    {
        g_cli_cells->persistent_buf_release(log->graph->text.bytes, &g_cli_git_cells);
    }
    else
    {
        log->graph = g_cli_cells->persistent_buf_alloc(sizeof(CliGraph), &g_cli_git_log_cells);
    }
    char *const root = g_cli_cells->temporary_buf_alloc(CLI_PATH_MAX_LEN, &g_cli_git_log_cells);
    if ((log->repositories == NULL) || (log->graph == NULL) || (root == NULL))
    {
        g_cli_cells->persistent_buf_release(root, &g_cli_git_log_cells);
        if (log->repositories != NULL)
        {
            log->repositories->text = (CliGitText){NULL, 0ULL, 0ULL, 0, 1};
        }
        if (log->graph != NULL)
        {
            log->graph->text = (CliGitText){NULL, 0ULL, 0ULL, 0, 1};
        }
        EMBED_CALL(listing_release, GitLogCtx, .at = 0u);
        return;
    }
    log->repositories->text = (CliGitText){NULL, 0ULL, 0ULL, 0, 1};
    log->repositories->count = 0u;
    EMBED_CALL(cli_git_root_found_repositories.repositories, CliGitRootFoundRepositoriesCfg, .root = args->root,
               .found_repositories_pointer = log->repositories);
    log->chosen = (log->chosen < log->repositories->count) ? log->chosen : 0u;
    EMBED_CALL(root_write, GitLogCtx, .root = args->root, .out = root);
    log->graph->text = (CliGitText){NULL, 0ULL, 0ULL, 0, 1};
    log->graph->count = 0u;
    log->graph->strokes = 0u;
    EMBED_CALL(cli_git_root_query.graph, CliGitRootQueryCfg, .root = root,
               .query = (log->query.bytes != NULL) ? log->query.bytes : "", .authors = log->authors,
               .found_graph_pointer = log->graph);
    g_cli_cells->persistent_buf_release(root, &g_cli_git_log_cells);
    // the listing's texts: the label `name: branch`, the commits' count, and each commit's day, sized to them
    const CliRepository *const chosen = (log->repositories->count != 0u) ? &log->repositories->item[log->chosen] : NULL;
    const char *const name = (chosen != NULL) ? EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                                           .text_read = &log->repositories->text, .at = chosen->name)
                                              : "";
    const char *const branch = ((chosen != NULL) && chosen->has_branch)
                                   ? EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                                .text_read = &log->repositories->text, .at = chosen->branch)
                                   : NULL;
    const unsigned long long name_length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_NAME_MAX_LEN});
    const unsigned long long branch_length =
        (branch != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = branch, .cap = CLI_NAME_MAX_LEN}) : 0ULL;
    const unsigned int commits = log->graph->count;
    const unsigned long long digits = 1ULL + (commits >= 10u) + (commits >= 100u) + (commits >= 1000u);
    const unsigned long long label_bytes = name_length + ((branch != NULL) ? 2ULL + branch_length : 0ULL) + 1ULL;
    const unsigned long long count_bytes = digits + sizeof(GIT_LOG_COMMITS) + sizeof(GIT_LOG_FOUND);
    log->listing = g_cli_cells->persistent_buf_alloc(
        label_bytes + count_bytes + ((unsigned long long)commits * CLI_GIT_LOG_DAY), &g_cli_git_log_cells);
    if (log->listing == NULL)
    {
        EMBED_CALL(listing_release, GitLogCtx, .at = 0u);
        return;
    }
    VerbaLinea label = {log->listing, label_bytes, 0ULL};
    mmgr_line_put(&label, name);
    if (branch != NULL)
    {
        mmgr_line_put(&label, ": ");
        mmgr_line_put(&label, branch);
    }
    log->label = log->listing;
    log->label_length = mmgr_line_finish(&label) ? label.at : 0ULL;
    VerbaLinea count = {log->listing + label_bytes, count_bytes, 0ULL};
    mmgr_line_decimal(&count, commits, 1u);
    mmgr_line_put(&count, (commits == 1u) ? " commit " : GIT_LOG_COMMITS);
    mmgr_line_put(&count, (log->query.length != 0ULL) ? GIT_LOG_FOUND : "shown");
    log->count = log->listing + label_bytes;
    log->count_length = mmgr_line_finish(&count) ? count.at : 0ULL;
    log->day = log->listing + label_bytes + count_bytes;
    for (unsigned int at = 0u; at < commits; at += 1u)
    {
        EMBED_CALL(day_write, GitLogCtx, .seconds = log->graph->item[at].when,
                   .out = log->listing + label_bytes + count_bytes + ((unsigned long long)at * CLI_GIT_LOG_DAY));
    }
    log->listed = 1;
    EMBED_CALL(opened_found, GitLogCtx, .at = 0u);
    EMBED_CALL(rows_index, GitLogCtx, .at = 0u);
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
    const char *const saved =
        (args->entries != NULL) ? cli_state_entry_value(args->entries, GIT_LOG_AUTHORS_KEY) : NULL;
    s_log.authors = (saved != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = saved, .other = "true", .cap = 8ULL});
}

void cli_git_log_type(const CliGitLogCfg *args)
{
    const unsigned long long before = s_log.query.length;
    EMBED_CALL(cli_typed.put, CliTypedCfg, .typed = &s_log.query, .text = args->text, .size = args->size,
               .most = CLI_GIT_LOG_QUERY_MAX, .owner = &g_cli_git_log_cells);
    if (s_log.query.length != before)
    {
        EMBED_CALL(changed, GitLogCtx, .now_ms = args->now_ms);
    }
}

int cli_git_log_key(const CliGitLogCfg *args)
{
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Backspace", .cap = 16ULL}))
    {
        EMBED_CALL(cli_typed.erase, CliTypedCfg, .typed = &s_log.query, .owner = &g_cli_git_log_cells);
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
    if ((args->which != CLI_GIT_LOG_REPO) || (s_log.repositories == NULL) || (s_log.repositories->count < 2u))
    {
        return;
    }
    s_log.chosen = (s_log.chosen + 1u < s_log.repositories->count) ? s_log.chosen + 1u : 0u;
    EMBED_CALL(list, GitLogCtx, .root = args->root);
}

void cli_git_log_authors_set(const CliGitLogCfg *args)
{
    s_log.authors = args->on;
    // the folder it is saved in and the error line, for the save alone
    char *const folder = g_cli_cells->temporary_buf_alloc(CLI_PATH_MAX_LEN + CLI_ERR_MSG_MAX_LEN, &g_cli_git_log_cells);
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
        g_cli_cells->persistent_buf_release(folder, &g_cli_git_log_cells);
    }
    EMBED_CALL(changed, GitLogCtx, .now_ms = args->now_ms - CLI_GIT_LOG_REST_MS);
}

int cli_git_log_authors(const CliGitLogCfg *args)
{
    (void)args;
    return s_log.authors;
}

const char *cli_git_log_query(const CliGitLogCfg *args)
{
    *args->length_out = s_log.query.length;
    return (s_log.query.bytes != NULL) ? s_log.query.bytes : "";
}

const char *cli_git_log_repo(const CliGitLogCfg *args)
{
    *args->count_out = s_log.listed ? s_log.repositories->count : 0u;
    return s_log.listed ? s_log.label : "";
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
        const CliRepository *const chosen =
            (log->repositories->count != 0u) ? &log->repositories->item[log->chosen] : NULL;
        row->kind = CLI_GIT_LOG_ROW_BRANCH;
        row->text = ((chosen != NULL) && chosen->has_branch)
                        ? EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                     .text_read = &log->repositories->text, .at = chosen->branch)
                        : GIT_LOG_NO_BRANCH;
        row->text_length = cellul.len(&(CatenaFinitaCfg){.src = row->text, .cap = CLI_NAME_MAX_LEN});
        row->second = log->count;
        row->second_length = log->count_length;
        return 1;
    }
    unsigned int under = 0u;
    GitLogOpened *record = NULL;
    const unsigned int commit =
        EMBED_CALL(commit_of, GitLogCtx, .at = args->at, .under_out = &under, .record_out = &record);
    const CliDrawn *const drawn = &log->graph->item[commit];
    row->id = EMBED_CALL(commit_id, GitLogCtx, .at = commit);
    if (under == 0u)
    {
        row->kind = CLI_GIT_LOG_ROW_COMMIT;
        row->text = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &log->graph->text,
                               .at = drawn->subject);
        row->text_length = cellul.len(&(CatenaFinitaCfg){.src = row->text, .cap = CLI_GIT_TEXT});
        if (log->authors)
        {
            row->second = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &log->graph->text,
                                     .at = drawn->author);
            row->second_length = cellul.len(&(CatenaFinitaCfg){.src = row->second, .cap = CLI_NAME_MAX_LEN});
        }
        row->day = log->day + ((unsigned long long)commit * CLI_GIT_LOG_DAY);
        row->open = (record != NULL) && record->shown;
        row->merge = drawn->parents > 1u;
        row->lane_at = drawn->lane_at;
        row->color = drawn->color;
        row->stroke = &log->graph->stroke[drawn->first_stroke];
        row->strokes = drawn->stroke_count;
        row->refs = drawn->refs;
        row->across = drawn->across;
        return 1;
    }
    if (record->count == 0u)
    {
        row->kind = CLI_GIT_LOG_ROW_NONE;
        row->text = GIT_LOG_NONE;
        row->text_length = sizeof(GIT_LOG_NONE) - 1ULL;
        return 1;
    }
    const GitLogFile *const file = &record->file[under - 1u];
    row->kind = CLI_GIT_LOG_ROW_FILE;
    row->path = record->text + file->path;
    row->state = file->state;
    row->text = row->path + file->name;
    row->text_length = cellul.len(&(CatenaFinitaCfg){.src = row->text, .cap = CLI_PATH_MAX_LEN});
    row->second = row->path;
    row->second_length = (file->name > 0u) ? file->name - 1u : 0u;
    return 1;
}

int cli_git_log_ref(const CliGitLogCfg *args)
{
    const GitLog *const log = &s_log;
    CliGitLogRef *const ref = args->ref;
    memor.set(&(MemoriaCfg){.dst = ref, .val = 0, .bytes = sizeof(*ref)});
    unsigned int under = 0u;
    GitLogOpened *record = NULL;
    const unsigned int commit =
        EMBED_CALL(commit_of, GitLogCtx, .at = args->at, .under_out = &under, .record_out = &record);
    if ((commit == CLI_GIT_GRAPH) || (under != 0u) || (args->which >= log->graph->item[commit].refs))
    {
        return 0;
    }
    // a name as git writes it: `HEAD -> ` before the branch HEAD stands on, `HEAD` alone where it stands on none,
    // and `tag: ` before a tag
    const char *name = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &log->graph->text,
                                  .at = log->graph->item[commit].ref[args->which]);
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
    GitLogOpened *const record = EMBED_CALL(opened_at, GitLogCtx, .which = args->which);
    if (record != NULL)
    {
        record->shown = !record->shown;
    }
    else
    {
        const char *const id = EMBED_CALL(commit_id, GitLogCtx, .at = args->which);
        EMBED_CALL(files_read, GitLogCtx, .which = args->which, .text = id,
                   .id = EMBED_CALL(id_number, GitLogCtx, .text = id), .root = args->root);
    }
    EMBED_CALL(rows_index, GitLogCtx, .at = 0u);
}

void cli_git_log_toggle(const CliGitLogCfg *args)
{
    unsigned int under = 0u;
    GitLogOpened *record = NULL;
    const unsigned int commit =
        EMBED_CALL(commit_of, GitLogCtx, .at = args->at, .under_out = &under, .record_out = &record);
    if ((commit != CLI_GIT_GRAPH) && (under == 0u))
    {
        EMBED_CALL(commit_toggle, GitLogCtx, .which = commit, .root = args->root);
    }
}

int cli_git_log_toggle_id(const CliGitLogCfg *args)
{
    for (unsigned int at = 0u; s_log.listed && (at < s_log.graph->count); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){
                .src = EMBED_CALL(commit_id, GitLogCtx, .at = at), .other = args->name, .cap = CLI_NAME_MAX_LEN}))
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
    char *const root = g_cli_cells->temporary_buf_alloc(CLI_PATH_MAX_LEN + CLI_ERR_MSG_MAX_LEN, &g_cli_git_log_cells);
    if (root == NULL)
    {
        return 0;
    }
    VerbaLinea err_msg = {root + CLI_PATH_MAX_LEN, CLI_ERR_MSG_MAX_LEN, 0ULL};
    EMBED_CALL(root_write, GitLogCtx, .root = args->root, .out = root);
    const char *const request[4] = {"log", "-1", "--format=%B", args->name};
    const int read = EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = root,
                                .out = args->out, .count = 4u, .err_msg = &err_msg, .request = request);
    g_cli_cells->persistent_buf_release(root, &g_cli_git_log_cells);
    return read;
}

int cli_git_log_menu(const CliGitLogCfg *args)
{
    CliGitLogRow row;
    if (!EMBED_CALL(cli_git_log.row, CliGitLogCfg, .at = args->at, .row = &row) ||
        ((row.kind != CLI_GIT_LOG_ROW_COMMIT) && (row.kind != CLI_GIT_LOG_ROW_FILE)))
    {
        return 0;
    }
    // the rows run with the commit's id or the file's path, which stand in the cells while the menu is open
    cli_menus_own_begin();
    if (row.kind == CLI_GIT_LOG_ROW_FILE)
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

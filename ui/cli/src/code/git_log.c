// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_log.c: the Git pane's rows. The query, the authors setting and the repository chosen stand here; a change marks
// the rest's start, and the step that finds the rest passed, or the watch's mark that the graph is stale, lists the
// tree's repositories and reads the chosen one's graph through git_graph.c, each commit's texts copied into the pane's
// own cell beside the graph's layout and its day read on the machine's calendar. A commit opened has the files it
// changed read once through git_lists.c and stored under its id until another tree is listed
#include "git.h"

#include "../command_line/cells.h"
#include "../command_line/commands.h"
#include "../text/text.h"
#include "embed_atomic.h"

// the bytes the graph's texts take, the bytes the files' paths take, the most commits whose files are read and the
// most files read, the bytes of the commits' count and of the repository's label, the key the authors setting is saved
// under in orior's folder, the branch row's text where the repository is on no branch, and the notice under a commit
// that changed no file under the tree
#define GIT_LOG_TEXT (1u << 20u)
#define GIT_LOG_FILE_TEXT (512u * 1024u)
#define GIT_LOG_OPENED 64u
#define GIT_LOG_FILES 8192u
#define GIT_LOG_COUNT 32u
#define GIT_LOG_LABEL (CLI_NAME_MAX_LEN * 2u)
#define GIT_LOG_AUTHORS_KEY "git.authors"
#define GIT_LOG_NO_BRANCH "no branch"
#define GIT_LOG_NONE "No file under the tree changed."

// A commit whose files were read: the first sixteen digits of its id as a number, its files' first place and how many,
// and whether its files show
typedef struct
{
    unsigned long long id;
    unsigned int first;
    unsigned int count;
    int shown;
} GitLogOpened;

// A file a commit changed: its path's place in the files' text and its state letter
typedef struct
{
    unsigned int path;
    char state;
} GitLogFile;

// A repository of the tree: the places of its path, its name and its branch in the text, and whether it is on a branch
typedef struct
{
    unsigned int path;
    unsigned int name;
    unsigned int branch;
    int has_branch;
} GitLogRepo;

// The pane's data: the query and its bytes, whether a change waits and since when, the authors setting, the tree
// listed and whether the rows stand for it, its repositories, the one chosen and its path, the label of the chosen one,
// its folder as the graph's root, its branch, the commits counted, the graph with each commit's texts pointing into
// the text, each commit's day, the first row of each commit and the rows counted, the texts, the commits whose files
// were read, their files and the files' texts
typedef struct
{
    char query[CLI_GIT_LOG_QUERY_MAX];
    unsigned long long query_length;
    int waits;
    unsigned long long changed_ms;
    int authors;
    char tree[CLI_PATH_MAX_LEN];
    int listed;
    GitLogRepo repo[CLI_GIT_REPOSITORIES];
    unsigned int repos;
    unsigned int chosen;
    char chosen_path[CLI_PATH_MAX_LEN];
    char label[GIT_LOG_LABEL];
    unsigned long long label_length;
    char root[CLI_PATH_MAX_LEN];
    unsigned int branch;
    int has_branch;
    char count[GIT_LOG_COUNT];
    unsigned long long count_length;
    CliGraph graph;
    char day[CLI_GIT_GRAPH][CLI_GIT_LOG_DAY];
    unsigned int first[CLI_GIT_GRAPH];
    unsigned int rows;
    char text[GIT_LOG_TEXT];
    unsigned int used;
    GitLogOpened opened[GIT_LOG_OPENED];
    unsigned int opens;
    GitLogFile file[GIT_LOG_FILES];
    unsigned int files;
    char file_text[GIT_LOG_FILE_TEXT];
    unsigned int file_used;
} GitLog;

static GitLog *s_log;
static void (*s_woken)(void);
static embed_atomic_u32 s_stale;
static CliRepositories s_repositories;
static CliTouchedList s_touched;

typedef struct
{
    const char *text;
    unsigned long long length;
    char *pool;
    unsigned int capacity;
    unsigned int *used;
    const CliGitText *text_read;
    unsigned int at;
    unsigned int which;
    unsigned int *under_out;
    char *out;
    long long seconds;
    const char *root;
    unsigned long long now_ms;
    unsigned long long id;
} GitLogCtx;

// the cell taken where none is: it, or NULL where none can be taken
EMBED_INLINE GitLog *log_of(const GitLogCtx *args)
{
    (void)args;
    if (s_log == NULL)
    {
        s_log = g_cli_cells->persistent_buf_alloc(sizeof(GitLog), &g_cli_git_log_cells);
        if (s_log != NULL)
        {
            memor.set(&(MemoriaCfg){.dst = s_log, .val = 0, .bytes = sizeof(GitLog)});
        }
    }
    return s_log;
}

// `length` bytes of `text` copied into `pool` of `capacity` after its `used` bytes with a NUL after them: their place,
// or 0, the empty text's place, where they do not fit
EMBED_INLINE unsigned int pool_append(const GitLogCtx *args)
{
    if (args->length + 1ULL > (unsigned long long)(args->capacity - *args->used))
    {
        return 0u;
    }
    const unsigned int at = *args->used;
    memor.cpy(&(MemoriaCfg){.dst = args->pool + at, .src = args->text, .bytes = args->length});
    args->pool[at + args->length] = '\0';
    // a text that fits the pool is counted by an unsigned int
    *args->used = at + (unsigned int)args->length + 1u;
    return at;
}

// the text at `at` of a list git wrote copied into the graph's texts: its place
EMBED_INLINE unsigned int git_text_copy(const GitLogCtx *args)
{
    const char *const text =
        EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = args->text_read, .at = args->at);
    return EMBED_CALL(pool_append, GitLogCtx, .pool = s_log->text, .capacity = GIT_LOG_TEXT, .used = &s_log->used,
                      .text = text, .length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_GIT_TEXT}));
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

// the place of the commit numbered `id` among those whose files were read, GIT_LOG_OPENED where it is not among them
EMBED_INLINE unsigned int opened_at(const GitLogCtx *args)
{
    for (unsigned int at = 0u; at < s_log->opens; at += 1u)
    {
        if (s_log->opened[at].id == args->id)
        {
            return at;
        }
    }
    return GIT_LOG_OPENED;
}

// `seconds` since 1970 written as YYYY-MM-DD on the machine's calendar into `out`, sized CLI_GIT_LOG_DAY
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

// the first row of each commit and the rows counted: the branch's row, then a row a commit and under one whose files
// show a row each file, or the notice's row where it has none
EMBED_INLINE void rows_index(const GitLogCtx *args)
{
    (void)args;
    GitLog *const log = s_log;
    unsigned int rows = 1u;
    for (unsigned int at = 0u; at < log->graph.count; at += 1u)
    {
        log->first[at] = rows;
        const unsigned int opened = EMBED_CALL(
            opened_at, GitLogCtx, .id = EMBED_CALL(id_number, GitLogCtx, .text = log->text + log->graph.item[at].id));
        rows += 1u;
        if ((opened != GIT_LOG_OPENED) && log->opened[opened].shown)
        {
            rows += (log->opened[opened].count != 0u) ? log->opened[opened].count : 1u;
        }
    }
    log->rows = rows;
}

// the commit whose rows the row `at` stands in, the row's place under the commit's own row into `under_out`: the
// commit, or CLI_GIT_GRAPH where the row is the branch's or stands past the commits
EMBED_INLINE unsigned int commit_of(const GitLogCtx *args)
{
    const GitLog *const log = s_log;
    if ((args->at == 0u) || (args->at >= log->rows) || (log->graph.count == 0u))
    {
        return CLI_GIT_GRAPH;
    }
    // the last commit whose first row is at or before the row
    unsigned int low = 0u;
    unsigned int high = log->graph.count;
    while (high - low > 1u)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (log->first[middle] <= args->at)
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }
    *args->under_out = args->at - log->first[low];
    return low;
}

// the commits whose files were read forgotten, with their files
EMBED_INLINE void opened_reset(const GitLogCtx *args)
{
    (void)args;
    s_log->opens = 0u;
    s_log->files = 0u;
    s_log->file_text[0] = '\0';
    s_log->file_used = 1u;
}

// the files the commit `text` is the id of changed, read from the chosen repository and stored under the commit's
// number: its place among the commits whose files were read
EMBED_INLINE unsigned int files_read(const GitLogCtx *args)
{
    GitLog *const log = s_log;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (!EMBED_CALL(cli_git_root_err_msg.touch, CliGitRootErrMsgCfg, .root = log->root, .err_msg = &err_msg,
                    .id = args->text, .found_touched_list_pointer = &s_touched))
    {
        s_touched.count = 0u;
    }
    if ((log->opens >= GIT_LOG_OPENED) || (log->files + s_touched.count > GIT_LOG_FILES))
    {
        EMBED_CALL(opened_reset, GitLogCtx, .at = 0u);
    }
    const char *const folder = (log->repos != 0u) ? log->text + log->repo[log->chosen].path : log->text;
    const unsigned long long folder_length = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_PATH_MAX_LEN});
    GitLogOpened *const opened = &log->opened[log->opens];
    *opened = (GitLogOpened){args->id, log->files, 0u, 1};
    for (unsigned int at = 0u; at < s_touched.count; at += 1u)
    {
        // the path under the tree: the repository's folder, where it is not the tree's own, then the path git wrote
        char path[CLI_PATH_MAX_LEN];
        VerbaLinea line = {path, sizeof(path), 0ULL};
        if (folder_length != 0ULL)
        {
            mmgr_line_put(&line, folder);
            mmgr_line_ch(&line, '/');
        }
        mmgr_line_put(&line, EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_touched.text,
                                        .at = s_touched.item[at].path));
        if (mmgr_line_finish(&line) == 0ULL)
        {
            continue;
        }
        const unsigned int place =
            EMBED_CALL(pool_append, GitLogCtx, .pool = log->file_text, .capacity = GIT_LOG_FILE_TEXT,
                       .used = &log->file_used, .text = path, .length = line.at);
        if (place == 0u)
        {
            break;
        }
        log->file[log->files] = (GitLogFile){place, s_touched.item[at].state};
        log->files += 1u;
        opened->count += 1u;
    }
    EMBED_CALL(cli_git_text.text_free, CliGitTextCfg, .text = &s_touched.text);
    log->opens += 1u;
    return log->opens - 1u;
}

// the tree at `root` listed: its repositories, the one chosen found again by its path or the tree's own, its label, its
// folder and its branch, its graph for the query with each commit's texts copied and its day written, and the commits
// counted; a tree other than the last listed forgets the commits whose files were read
EMBED_INLINE void list(const GitLogCtx *args)
{
    GitLog *const log = s_log;
    EMBED_ATOMIC_STORE(&s_stale, 0u);
    log->waits = 0;
    log->listed = 0;
    log->text[0] = '\0';
    log->used = 1u;
    log->repos = 0u;
    log->graph.count = 0u;
    log->graph.strokes = 0u;
    log->has_branch = 0;
    log->branch = 0u;
    log->label_length = 0ULL;
    log->label[0] = '\0';
    if (args->root == NULL)
    {
        log->tree[0] = '\0';
        EMBED_CALL(opened_reset, GitLogCtx, .at = 0u);
        EMBED_CALL(rows_index, GitLogCtx, .at = 0u);
        return;
    }
    if (!cellul.eq(&(CatenaFinitaCfg){.src = log->tree, .other = args->root, .cap = CLI_PATH_MAX_LEN}))
    {
        VerbaLinea tree = {log->tree, sizeof(log->tree), 0ULL};
        mmgr_line_put(&tree, args->root);
        (void)mmgr_line_finish(&tree);
        log->chosen_path[0] = '\0';
        EMBED_CALL(opened_reset, GitLogCtx, .at = 0u);
    }
    EMBED_CALL(cli_git_root_found_repositories.repositories, CliGitRootFoundRepositoriesCfg, .root = args->root,
               .found_repositories_pointer = &s_repositories);
    log->chosen = 0u;
    for (unsigned int at = 0u; at < s_repositories.count; at += 1u)
    {
        const CliRepository *const repository = &s_repositories.item[at];
        log->repo[at] = (GitLogRepo){
            EMBED_CALL(git_text_copy, GitLogCtx, .text_read = &s_repositories.text, .at = repository->path),
            EMBED_CALL(git_text_copy, GitLogCtx, .text_read = &s_repositories.text, .at = repository->name),
            EMBED_CALL(git_text_copy, GitLogCtx, .text_read = &s_repositories.text, .at = repository->branch),
            repository->has_branch};
        if (cellul.eq(&(CatenaFinitaCfg){
                .src = log->text + log->repo[at].path, .other = log->chosen_path, .cap = CLI_PATH_MAX_LEN}))
        {
            log->chosen = at;
        }
    }
    log->repos = s_repositories.count;
    EMBED_CALL(cli_git_text.text_free, CliGitTextCfg, .text = &s_repositories.text);
    VerbaLinea root = {log->root, sizeof(log->root), 0ULL};
    mmgr_line_put(&root, args->root);
    (void)mmgr_line_finish(&root);
    if (log->repos != 0u)
    {
        const GitLogRepo *const chosen = &log->repo[log->chosen];
        VerbaLinea chosen_path = {log->chosen_path, sizeof(log->chosen_path), 0ULL};
        mmgr_line_put(&chosen_path, log->text + chosen->path);
        (void)mmgr_line_finish(&chosen_path);
        if (log->chosen_path[0] != '\0')
        {
            EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = log->chosen_path,
                       .joined = log->root);
        }
        VerbaLinea label = {log->label, sizeof(log->label), 0ULL};
        mmgr_line_put(&label, log->text + chosen->name);
        if (chosen->has_branch)
        {
            mmgr_line_put(&label, ": ");
            mmgr_line_put(&label, log->text + chosen->branch);
        }
        log->label_length = mmgr_line_finish(&label) ? label.at : 0ULL;
        log->branch = chosen->branch;
        log->has_branch = chosen->has_branch;
    }
    EMBED_CALL(cli_git_root_query.graph, CliGitRootQueryCfg, .root = log->root, .query = log->query,
               .authors = log->authors, .found_graph_pointer = &log->graph);
    for (unsigned int at = 0u; at < log->graph.count; at += 1u)
    {
        CliDrawn *const drawn = &log->graph.item[at];
        drawn->id = EMBED_CALL(git_text_copy, GitLogCtx, .text_read = &log->graph.text, .at = drawn->id);
        drawn->author = EMBED_CALL(git_text_copy, GitLogCtx, .text_read = &log->graph.text, .at = drawn->author);
        drawn->subject = EMBED_CALL(git_text_copy, GitLogCtx, .text_read = &log->graph.text, .at = drawn->subject);
        for (unsigned int ref = 0u; ref < drawn->refs; ref += 1u)
        {
            drawn->ref[ref] =
                EMBED_CALL(git_text_copy, GitLogCtx, .text_read = &log->graph.text, .at = drawn->ref[ref]);
        }
        EMBED_CALL(day_write, GitLogCtx, .seconds = drawn->when, .out = log->day[at]);
    }
    EMBED_CALL(cli_git_text.text_free, CliGitTextCfg, .text = &log->graph.text);
    VerbaLinea count = {log->count, sizeof(log->count), 0ULL};
    mmgr_line_decimal(&count, log->graph.count, 1u);
    mmgr_line_put(&count, (log->graph.count == 1u) ? " commit " : " commits ");
    mmgr_line_put(&count, (log->query_length != 0ULL) ? "found" : "shown");
    log->count_length = mmgr_line_finish(&count) ? count.at : 0ULL;
    log->listed = 1;
    EMBED_CALL(rows_index, GitLogCtx, .at = 0u);
}

// a change to the query or the settings at `now_ms`: the rest begins
EMBED_INLINE void changed(const GitLogCtx *args)
{
    s_log->waits = 1;
    s_log->changed_ms = args->now_ms;
}

void cli_git_log_begin(const CliGitLogCfg *args)
{
    s_woken = args->woken;
    GitLog *const log = EMBED_CALL(log_of, GitLogCtx, .at = 0u);
    const char *const saved =
        (args->entries != NULL) ? cli_state_entry_value(args->entries, GIT_LOG_AUTHORS_KEY) : NULL;
    if (log != NULL)
    {
        log->authors = (saved != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = saved, .other = "true", .cap = 8ULL});
    }
}

void cli_git_log_type(const CliGitLogCfg *args)
{
    GitLog *const log = EMBED_CALL(log_of, GitLogCtx, .at = 0u);
    if ((log == NULL) || (args->size == 0ULL))
    {
        return;
    }
    unsigned long long size = args->size;
    for (unsigned long long at = 0ULL; at < size; at += 1ULL)
    {
        // a control character typed, a tab or a return, is no part of a query
        size = ((unsigned char)args->text[at] < 0x20u) ? at : size;
    }
    if ((size == 0ULL) || (log->query_length + size + 1ULL > CLI_GIT_LOG_QUERY_MAX))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = log->query + log->query_length, .src = args->text, .bytes = size});
    log->query_length += size;
    log->query[log->query_length] = '\0';
    EMBED_CALL(changed, GitLogCtx, .now_ms = args->now_ms);
}

int cli_git_log_key(const CliGitLogCfg *args)
{
    GitLog *const log = EMBED_CALL(log_of, GitLogCtx, .at = 0u);
    if (log == NULL)
    {
        return 0;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Backspace", .cap = 16ULL}))
    {
        // the last letter erased: its lead byte and the continuation bytes after it
        while (log->query_length > 0ULL)
        {
            log->query_length -= 1ULL;
            if (((unsigned char)log->query[log->query_length] & 0xC0u) != 0x80u)
            {
                break;
            }
        }
        log->query[log->query_length] = '\0';
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
    GitLog *const log = EMBED_CALL(log_of, GitLogCtx, .at = 0u);
    if ((log == NULL) || (args->which != CLI_GIT_LOG_REPO) || (log->repos < 2u))
    {
        return;
    }
    const unsigned int next = (log->chosen + 1u < log->repos) ? log->chosen + 1u : 0u;
    VerbaLinea chosen_path = {log->chosen_path, sizeof(log->chosen_path), 0ULL};
    mmgr_line_put(&chosen_path, log->text + log->repo[next].path);
    (void)mmgr_line_finish(&chosen_path);
    EMBED_CALL(list, GitLogCtx, .root = args->root);
}

void cli_git_log_authors_set(const CliGitLogCfg *args)
{
    GitLog *const log = EMBED_CALL(log_of, GitLogCtx, .at = 0u);
    if (log == NULL)
    {
        return;
    }
    log->authors = args->on;
    char folder[CLI_PATH_MAX_LEN];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder))
    {
        (void)cli_state_value_save(&(CliStateValueCfg){
            .folder = folder, .key = GIT_LOG_AUTHORS_KEY, .value = args->on ? "true" : "false", .err_msg = &err_msg});
    }
    EMBED_CALL(changed, GitLogCtx, .now_ms = args->now_ms - CLI_GIT_LOG_REST_MS);
}

int cli_git_log_authors(const CliGitLogCfg *args)
{
    (void)args;
    return (s_log != NULL) && s_log->authors;
}

const char *cli_git_log_query(const CliGitLogCfg *args)
{
    const GitLog *const log = s_log;
    *args->length_out = (log != NULL) ? log->query_length : 0ULL;
    return (log != NULL) ? log->query : "";
}

const char *cli_git_log_repo(const CliGitLogCfg *args)
{
    const GitLog *const log = s_log;
    *args->count_out = ((log != NULL) && log->listed) ? log->repos : 0u;
    return ((log != NULL) && log->listed) ? log->label : "";
}

void cli_git_log_relist(const CliGitLogCfg *args)
{
    if (EMBED_CALL(log_of, GitLogCtx, .at = 0u) != NULL)
    {
        EMBED_CALL(list, GitLogCtx, .root = args->root);
    }
}

int cli_git_log_step(const CliGitLogCfg *args)
{
    GitLog *const log = EMBED_CALL(log_of, GitLogCtx, .at = 0u);
    if ((log == NULL) || !args->on)
    {
        return 0;
    }
    const int stale = (EMBED_ATOMIC_EXCHANGE_32(&s_stale, 0u) != 0u) && log->listed;
    if (stale || (log->waits && (args->now_ms - log->changed_ms >= CLI_GIT_LOG_REST_MS)))
    {
        EMBED_CALL(list, GitLogCtx, .root = args->root);
        return 1;
    }
    return 0;
}

unsigned long long cli_git_log_deadline_ms(const CliGitLogCfg *args)
{
    (void)args;
    const GitLog *const log = s_log;
    return ((log != NULL) && log->waits) ? log->changed_ms + CLI_GIT_LOG_REST_MS : 0ULL;
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
    const GitLog *const log = s_log;
    return ((log != NULL) && log->listed) ? log->rows : 0u;
}

int cli_git_log_row(const CliGitLogCfg *args)
{
    const GitLog *const log = s_log;
    CliGitLogRow *const row = args->row;
    memor.set(&(MemoriaCfg){.dst = row, .val = 0, .bytes = sizeof(*row)});
    if ((log == NULL) || !log->listed || (args->at >= log->rows))
    {
        return 0;
    }
    if (args->at == 0u)
    {
        row->kind = CLI_GIT_LOG_ROW_BRANCH;
        row->text = log->has_branch ? log->text + log->branch : GIT_LOG_NO_BRANCH;
        row->text_length = cellul.len(&(CatenaFinitaCfg){.src = row->text, .cap = CLI_NAME_MAX_LEN});
        row->second = log->count;
        row->second_length = log->count_length;
        return 1;
    }
    unsigned int under = 0u;
    const unsigned int commit = EMBED_CALL(commit_of, GitLogCtx, .at = args->at, .under_out = &under);
    const CliDrawn *const drawn = &log->graph.item[commit];
    const char *const id = log->text + drawn->id;
    const unsigned int opened = EMBED_CALL(opened_at, GitLogCtx, .id = EMBED_CALL(id_number, GitLogCtx, .text = id));
    row->id = id;
    if (under == 0u)
    {
        row->kind = CLI_GIT_LOG_ROW_COMMIT;
        row->text = log->text + drawn->subject;
        row->text_length = cellul.len(&(CatenaFinitaCfg){.src = row->text, .cap = CLI_GIT_TEXT});
        if (log->authors)
        {
            row->second = log->text + drawn->author;
            row->second_length = cellul.len(&(CatenaFinitaCfg){.src = row->second, .cap = CLI_NAME_MAX_LEN});
        }
        row->day = log->day[commit];
        row->open = (opened != GIT_LOG_OPENED) && log->opened[opened].shown;
        row->merge = drawn->parents > 1u;
        row->lane_at = drawn->lane_at;
        row->color = drawn->color;
        row->stroke = &log->graph.stroke[drawn->first_stroke];
        row->strokes = drawn->stroke_count;
        row->refs = drawn->refs;
        // the columns the row crosses: the dot's and every stroke's ends
        unsigned int across = drawn->lane_at;
        for (unsigned int at = 0u; at < drawn->stroke_count; at += 1u)
        {
            const unsigned int *const stroke = log->graph.stroke[drawn->first_stroke + at];
            across = (stroke[0] > across) ? stroke[0] : across;
            across = (stroke[1] > across) ? stroke[1] : across;
        }
        row->across = across + 1u;
        return 1;
    }
    const GitLogOpened *const shown = &log->opened[opened];
    if (shown->count == 0u)
    {
        row->kind = CLI_GIT_LOG_ROW_NONE;
        row->text = GIT_LOG_NONE;
        row->text_length = cellul.len(&(CatenaFinitaCfg){.src = GIT_LOG_NONE, .cap = 64ULL});
        return 1;
    }
    const GitLogFile *const file = &log->file[shown->first + under - 1u];
    row->kind = CLI_GIT_LOG_ROW_FILE;
    row->path = log->file_text + file->path;
    row->state = file->state;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = row->path, .cap = CLI_PATH_MAX_LEN});
    const unsigned long long cut = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg,
                                              .text = row->path, .at = length, .set = "/");
    row->text = row->path + cut;
    row->text_length = length - cut;
    row->second = row->path;
    row->second_length = (cut > 0ULL) ? cut - 1ULL : 0ULL;
    return 1;
}

int cli_git_log_ref(const CliGitLogCfg *args)
{
    const GitLog *const log = s_log;
    CliGitLogRef *const ref = args->ref;
    memor.set(&(MemoriaCfg){.dst = ref, .val = 0, .bytes = sizeof(*ref)});
    unsigned int under = 0u;
    if ((log == NULL) || !log->listed || (args->at >= log->rows))
    {
        return 0;
    }
    const unsigned int commit = EMBED_CALL(commit_of, GitLogCtx, .at = args->at, .under_out = &under);
    if ((commit == CLI_GIT_GRAPH) || (under != 0u) || (args->which >= log->graph.item[commit].refs))
    {
        return 0;
    }
    // a name as git writes it: `HEAD -> ` before the branch HEAD stands on, `HEAD` alone where it stands on none,
    // and `tag: ` before a tag
    const char *name = log->text + log->graph.item[commit].ref[args->which];
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

// the commit `which` opened or closed, its files read as it first opens, and the rows indexed again
EMBED_INLINE void commit_toggle(const GitLogCtx *args)
{
    GitLog *const log = s_log;
    const char *const id = log->text + log->graph.item[args->which].id;
    const unsigned long long number = EMBED_CALL(id_number, GitLogCtx, .text = id);
    const unsigned int opened = EMBED_CALL(opened_at, GitLogCtx, .id = number);
    if (opened == GIT_LOG_OPENED)
    {
        (void)EMBED_CALL(files_read, GitLogCtx, .text = id, .id = number);
    }
    else
    {
        log->opened[opened].shown = !log->opened[opened].shown;
    }
    EMBED_CALL(rows_index, GitLogCtx, .at = 0u);
}

// the commit of the row `at` where the row is a commit's own: its place, or CLI_GIT_GRAPH where the row is another's
EMBED_INLINE unsigned int commit_row(const GitLogCtx *args)
{
    const GitLog *const log = s_log;
    unsigned int under = 0u;
    if ((log == NULL) || !log->listed || (args->at >= log->rows))
    {
        return CLI_GIT_GRAPH;
    }
    const unsigned int commit = EMBED_CALL(commit_of, GitLogCtx, .at = args->at, .under_out = &under);
    return (under == 0u) ? commit : CLI_GIT_GRAPH;
}

void cli_git_log_toggle(const CliGitLogCfg *args)
{
    const unsigned int commit = EMBED_CALL(commit_row, GitLogCtx, .at = args->at);
    if (commit != CLI_GIT_GRAPH)
    {
        EMBED_CALL(commit_toggle, GitLogCtx, .which = commit);
    }
}

int cli_git_log_toggle_id(const CliGitLogCfg *args)
{
    const GitLog *const log = s_log;
    if ((log == NULL) || !log->listed)
    {
        return 0;
    }
    for (unsigned int at = 0u; at < log->graph.count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){
                .src = log->text + log->graph.item[at].id, .other = args->name, .cap = CLI_NAME_MAX_LEN}))
        {
            EMBED_CALL(commit_toggle, GitLogCtx, .which = at);
            return 1;
        }
    }
    return 0;
}

int cli_git_log_message(const CliGitLogCfg *args)
{
    const GitLog *const log = s_log;
    if ((log == NULL) || !log->listed)
    {
        return 0;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const char *const request[4] = {"log", "-1", "--format=%B", args->name};
    return EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = log->root,
                      .out = args->out, .count = 4u, .err_msg = &err_msg, .request = request);
}

int cli_git_log_menu(const CliGitLogCfg *args)
{
    CliGitLogRow row;
    if (!EMBED_CALL(cli_git_log.row, CliGitLogCfg, .at = args->at, .row = &row) ||
        ((row.kind != CLI_GIT_LOG_ROW_COMMIT) && (row.kind != CLI_GIT_LOG_ROW_FILE)))
    {
        return 0;
    }
    char words[CLI_PATH_MAX_LEN + 2u];
    VerbaLinea line = {words, sizeof(words), 0ULL};
    mmgr_line_put(&line, (row.kind == CLI_GIT_LOG_ROW_COMMIT) ? row.id : row.path);
    mmgr_line_ch(&line, '\n');
    (void)mmgr_line_finish(&line);
    cli_menus_own_begin();
    if (row.kind == CLI_GIT_LOG_ROW_FILE)
    {
        cli_menus_own_row(&(CliItem){"Copy Path", "file-copy-path", "", words, "", "", 0, 0, 0, 0, 0u});
        return 1;
    }
    cli_menus_own_row(
        &(CliItem){row.open ? "Close Files" : "Open Files", "commit-files", "", words, "", "", 0, 0, 0, 0, 0u});
    cli_menus_own_line();
    cli_menus_own_row(&(CliItem){"Copy Commit ID", "commit-copy-id", "", words, "", "", 0, 0, 0, 0, 0u});
    cli_menus_own_row(&(CliItem){"Copy Commit Message", "commit-copy-message", "", words, "", "", 0, 0, 0, 0, 0u});
    return 1;
}

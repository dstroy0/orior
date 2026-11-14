// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git.h: what git reports about the tree: the branch it is on, the files that differ from the last commit and how, the
// commits that touched a file, every branch's commits laid out as a graph and the files each changed, the commit that
// last changed each line of a file, and a file's text as one of those commits left it or as the last did. In a tree
// git cannot read, each of these comes back empty. What git is requested to do, for the Commit window and the Git menu:
// commit chosen files, push, pull where nothing would merge, put a file back as the last commit left it, act on
// branches, put changes aside in a stash and bring them back, and apply a commit of another branch, each giving git's
// own words where it refuses. Here too is the clone of a repository into a folder of its own, for File, Clone
// Repository and `imp file clone` (git.c, git_run.c, git_commit.c, git_branches.c, git_lists.c, git_graph.c,
// git_lines.c, git_clone.c)
#ifndef GIT_H
#define GIT_H
#include "../command_line/callback_cfg.h"
#include "../command_line/cli_internal.h"
#include "../command_line/pool.h"
#include "../machine/state.h"
#include "embed_dispatch_layout.h"

typedef struct CliGitRootCfg CliGitRootCfg;
typedef struct CliGitRootOutCfg CliGitRootOutCfg;
typedef struct CliGitRootOutCountCfg CliGitRootOutCountCfg;
typedef struct CliGitLengthCfg CliGitLengthCfg;
typedef struct CliGitRootOutMessageCfg CliGitRootOutMessageCfg;
typedef struct CliGitRootReposCfg CliGitRootReposCfg;
typedef struct CliGitErrMsgCfg CliGitErrMsgCfg;
typedef struct CliGitRootOutCountErrMsgCfg CliGitRootOutCountErrMsgCfg;
typedef struct CliGitRootFileCfg CliGitRootFileCfg;
typedef struct CliGitErrMsgFolderCfg CliGitErrMsgFolderCfg;
typedef struct CliGitRootFileErrMsgCfg CliGitRootFileErrMsgCfg;
typedef struct CliGitRootQueryCfg CliGitRootQueryCfg;
typedef struct CliGitRootCommitCfg CliGitRootCommitCfg;
typedef struct CliGitRootIdCfg CliGitRootIdCfg;
typedef struct CliGitRootFileStageCfg CliGitRootFileStageCfg;
typedef struct CliGitRootErrMsgCfg CliGitRootErrMsgCfg;
typedef struct CliGitRootAheadCfg CliGitRootAheadCfg;
typedef struct CliGitOutCfg CliGitOutCfg;
typedef struct CliGitRootErrMsgPathCfg CliGitRootErrMsgPathCfg;
typedef struct CliGitTextCfg CliGitTextCfg;
typedef struct CliGitOutFoundTouchedListCfg CliGitOutFoundTouchedListCfg;
typedef struct CliGitCfg CliGitCfg;
typedef struct CliGitRootFoundBranchesCfg CliGitRootFoundBranchesCfg;
typedef struct CliGitOutFoundBranchesCfg CliGitOutFoundBranchesCfg;
typedef struct CliGitOutFoundChangedListCfg CliGitOutFoundChangedListCfg;
typedef struct CliGitOutFoundCfg CliGitOutFoundCfg;
typedef struct CliGitOutFoundGraphCfg CliGitOutFoundGraphCfg;
typedef struct CliGitOutFoundLineHistoryCfg CliGitOutFoundLineHistoryCfg;
typedef struct CliGitRootFoundRepositoriesCfg CliGitRootFoundRepositoriesCfg;
typedef struct CliGitOutFoundRepositoriesCfg CliGitOutFoundRepositoriesCfg;
typedef struct CliGitRootFoundStashesCfg CliGitRootFoundStashesCfg;
typedef struct CliGitOutFoundStashesCfg CliGitOutFoundStashesCfg;
typedef struct CliGitTextAtCfg CliGitTextAtCfg;

// the repository File, Clone Repository offers first
#define CLI_GIT_ORIOR "https://github.com/dstroy0/orior.git"

// the most bytes of the texts of one list read from git
#define CLI_GIT_TEXT (1u << 22u)

// The texts of a list: each one ended by a 0 at its place, the place 0 containing the empty text. As the list is read
// they are written into `bytes`, a work cell of the temporary tier of `capacity`, 1 in `full` where one did not fit and
// was left empty. A list that stands between the window's frames, 1 in `standing`, ends its read by moving them into
// `cell`, a movable cell of MMgr's sized to `used`, the work cell given back; a list read for one call keeps them in
// the work cell until it is released
typedef struct
{
    char *bytes;
    const CarcerMovableEntry *cell;
    unsigned long long used;
    unsigned long long capacity;
    int full;
    int standing;
} CliGitText;

// the texts begun empty, for a list containing none before, or one released; `length` bytes of `from` stored: their
// place; a standing list's texts moved into a movable cell sized to them as its read ends, nothing for another list;
// the text at a place: where the texts stand in the work cell, where they stand, else copied into `dst`, `capacity`
// bytes with its end, the empty text where the list has none or `dst` is NULL; and the texts' cells released, the list
// containing none
void cli_git_text_begin(const CliGitTextCfg *args);
unsigned int cli_git_text_append(const CliGitTextCfg *args);
void cli_git_text_end(const CliGitTextCfg *args);
const char *cli_git_text_get(const CliGitTextAtCfg *args);
void cli_git_text_release(const CliGitTextCfg *args);

// the most repositories, changed files, branches, stashes and commits a list contains
#define CLI_GIT_REPOSITORIES 256u

#define CLI_GIT_CHANGED 65536u

#define CLI_GIT_BRANCHES 4096u

#define CLI_GIT_STASHES 1024u

#define CLI_GIT_COMMITS 200u

// A repository of the tree: its folder's path in the tree, empty for the one the tree itself is in, its name, and the
// branch it is on
typedef struct
{
    unsigned int path;
    unsigned int name;
    unsigned int branch;
    int has_branch;
} CliRepository;

typedef struct
{
    CliRepository item[CLI_GIT_REPOSITORIES];
    unsigned int count;
    CliGitText text;
} CliRepositories;

// A file that differs from the last commit: its path from the tree's top, the place of its name in that path, and its
// state, one letter: M changed, A added, D gone, R renamed, U not yet tracked, C in conflict
typedef struct
{
    unsigned int path;
    unsigned int name;
    char state;
} CliChanged;

typedef struct
{
    CliChanged item[CLI_GIT_CHANGED];
    unsigned int count;
    CliGitText text;
} CliChangedList;

// One commit that touched a file: its id, when it was made in seconds since 1970, and its subject
typedef struct
{
    unsigned int id;
    long long when;
    unsigned int subject;
} CliCommit;

typedef struct
{
    CliCommit item[CLI_GIT_COMMITS];
    unsigned int count;
    CliGitText text;
} CliCommits;

// A branch, local or remote: its short name, whether the tree is on it, the branch it follows, and when its last
// commit was made and its subject
typedef struct
{
    unsigned int name;
    int remote;
    int current;
    unsigned int upstream;
    long long when;
    unsigned int subject;
} CliBranch;

typedef struct
{
    CliBranch item[CLI_GIT_BRANCHES];
    unsigned int count;
    CliGitText text;
} CliBranches;

// A stash, changes put aside: its name, `stash@{0}` the newest, when it was made in seconds since 1970, and git's line
// for it, the branch it was made on and its message
typedef struct
{
    unsigned int name;
    long long when;
    unsigned int subject;
} CliStash;

typedef struct
{
    CliStash item[CLI_GIT_STASHES];
    unsigned int count;
    CliGitText text;
} CliStashes;

// A file the next commit takes in part: its path and the text the commit takes for it, `length` bytes
typedef struct
{
    const char *path;
    const char *text;
    unsigned long long length;
} CliGitPart;

// The changes as the window reads them (git_changes.c): every file under the tree that differs from the last commit of
// the repository containing it, as rows in the order git lists them, the list standing until the next read, and under a
// file whose changes are open, a row each change and a row each line of it. A row is of one of three types: a file,
// a change or a line. Every row carries the file's path from the tree's top and the file's place among the files. A
// file's row carries its name, the last part of that path, the bytes of the folder before it, its state letter as
// cli_git_change reads it, whether the next commit takes it, whether it takes it in part, some lines left out,
// whether its changes can open under it, and whether they are open. A change's row carries the change's place among
// the file's changes, the newer text's first line of it and the line after its last, counted from 0, whether some of
// its lines are taken and whether some and not all are. A line's row carries its change's place, its first units copied
// out of the document store, CLI_CHANGED_ROW_UNITS at the most, and how many, whether it is taken out of the older
// text, else put in by the newer, and whether the commit takes it. The path is copied out of the list's texts into the
// row, and the name points into that copy
#define CLI_CHANGES_ROW_FILE 0u
#define CLI_CHANGES_ROW_HUNK 1u
#define CLI_CHANGES_ROW_LINE 2u
#define CLI_CHANGED_ROW_UNITS 256u
typedef struct
{
    unsigned int type;
    char path[CLI_PATH_MAX_LEN];
    unsigned int file;
    const char *name;
    unsigned long long folder_length;
    char state;
    int taken;
    int part;
    int opens;
    int open;
    unsigned int hunk;
    unsigned int from;
    unsigned int to;
    unsigned short units[CLI_CHANGED_ROW_UNITS];
    unsigned long long count;
    int gone;
} CliChangedRow;
typedef struct CliGitChangesCfg CliGitChangesCfg;
// the most bytes of the commit's message with its end, and of git's refusal with its end; the head's actions: the
// message's box, which takes the keys, Commit, Commit and Push, the tree read again, the files taken rolled back, the
// branch pulled and pushed, and every file taken or left out; and the milliseconds a roll back's second press has to
// come in
#define CLI_CHANGES_MESSAGE_MAX 4096u
#define CLI_CHANGES_REFUSAL_MAX 4096u
#define CLI_CHANGES_FIELD 1u
#define CLI_CHANGES_COMMIT 2u
#define CLI_CHANGES_COMMIT_PUSH 3u
#define CLI_CHANGES_READ 4u
#define CLI_CHANGES_ROLLBACK 5u
#define CLI_CHANGES_PULL 6u
#define CLI_CHANGES_PUSH 7u
#define CLI_CHANGES_ALL 8u
#define CLI_CHANGES_ROLLBACK_MS 4000ULL
// the changes begun: `woken` wakes the view's thread as the watch marks the changes stale; the changes marked stale
// from any thread; and while the pane is `on`, the changes read again where the watch marked them stale since they
// were last read: 1 where they were
void cli_changes_begin(const CliGitChangesCfg *args);
void cli_changes_stale(const CliGitChangesCfg *args);
int cli_changes_step(const CliGitChangesCfg *args);
// the changes read anew of the tree at `root`, every repository of it, each file taken by the next commit unless it was
// left out before, with how far the branch stands from the remote it follows; none where `root` is NULL
void cli_changes_read(const CliGitChangesCfg *args);
// how many rows the changes have, the files with the rows open under them; how many files differ from the last commit;
// the row `at` into `row`: 1, or 0 past the last; the row `at` toggled: a file's left
// out of the next commit or taken again, its lines' choices cleared; a change's lines every one taken where some is
// left out, else every one left out; a line's left out or taken; a file left out as its last line is, and taken again
// as a line of it is; the changes of the file of the row `at` opened under it, the document store's changes of its
// text, the open view's where one is open on it, against the file as the last commit has it, or closed where they are
// open: 1, or 0 where the file's changes cannot open, a file gone, not a text or too far from the last commit's; how
// many files the next commit takes; every file taken where `on`, else every file left out; and the menu of the row
// `at` built as the menus' own rows, Open File, which a file gone cannot take, then Copy Path: 1, or 0 past the last
unsigned int cli_changes_rows(const CliGitChangesCfg *args);
unsigned int cli_changes_files(const CliGitChangesCfg *args);
int cli_changes_row(const CliGitChangesCfg *args);
void cli_changes_toggle(const CliGitChangesCfg *args);
int cli_changes_open(const CliGitChangesCfg *args);
unsigned int cli_changes_taken(const CliGitChangesCfg *args);
void cli_changes_take_all(const CliGitChangesCfg *args);
int cli_changes_menu(const CliGitChangesCfg *args);
// `size` bytes of `text` typed at the message's end; the key `name` pressed in the box, Backspace erasing the last
// letter: 1 where the key was taken; the message copied with its end into `text_out`, sized for
// CLI_CHANGES_MESSAGE_MAX: its bytes; the files taken committed to the
// tree at `root` with the message, a file with lines left out committed with the text of its lines taken, each
// repository committed to pushed where `push`, the message cleared, the open changes closed and the changes read
// again where git takes it: 1, or 0 where git refuses or nothing is to commit; and git's refusal of the
// last commit, pull, push or roll back, or why nothing was to commit, copied with its end into `text_out` where that is
// not NULL, sized for CLI_CHANGES_REFUSAL_MAX: its bytes, 0 where the last was taken
void cli_changes_type(const CliGitChangesCfg *args);
int cli_changes_key(const CliGitChangesCfg *args);
unsigned long long cli_changes_message(const CliGitChangesCfg *args);
int cli_changes_commit(const CliGitChangesCfg *args);
unsigned long long cli_changes_refusal(const CliGitChangesCfg *args);
// how many commits the branch has that the remote it follows does not into `ahead`, and the other way into `behind`:
// 1, or 0 where it follows no remote, as the changes were last read; the branch pulled, and pushed, at `root`, the
// changes read again after: 1, or 0 with the refusal standing; and the files taken rolled back to the last commit at
// `now_ms`: 0 where the press arms the roll back, a second press within CLI_CHANGES_ROLLBACK_MS rolling, 1 where it
// rolled, a file the last commit does not contain refused, the list standing as it was until it is read again
int cli_changes_sync(const CliGitChangesCfg *args);
int cli_changes_pull(const CliGitChangesCfg *args);
int cli_changes_push(const CliGitChangesCfg *args);
int cli_changes_rollback(const CliGitChangesCfg *args);
struct CliGitChangesCfg
{
    const char *root;
    unsigned int at;
    CliChangedRow *row;
    const char *text;
    unsigned long long size;
    const char *name;
    unsigned long long *length_out;
    void (*woken)(void);
    int on;
    int push;
    unsigned long long now_ms;
    unsigned int *ahead;
    unsigned int *behind;
    char *text_out;
};

typedef struct
{
    void (*begin)(const CliGitChangesCfg *args);
    void (*stale)(const CliGitChangesCfg *args);
    int (*step)(const CliGitChangesCfg *args);
    void (*read)(const CliGitChangesCfg *args);
    unsigned int (*rows)(const CliGitChangesCfg *args);
    unsigned int (*files)(const CliGitChangesCfg *args);
    int (*row)(const CliGitChangesCfg *args);
    void (*toggle)(const CliGitChangesCfg *args);
    int (*open)(const CliGitChangesCfg *args);
    unsigned int (*taken)(const CliGitChangesCfg *args);
    void (*take_all)(const CliGitChangesCfg *args);
    int (*menu)(const CliGitChangesCfg *args);
    void (*type)(const CliGitChangesCfg *args);
    int (*key)(const CliGitChangesCfg *args);
    unsigned long long (*message)(const CliGitChangesCfg *args);
    int (*commit)(const CliGitChangesCfg *args);
    unsigned long long (*refusal)(const CliGitChangesCfg *args);
    int (*sync)(const CliGitChangesCfg *args);
    int (*pull)(const CliGitChangesCfg *args);
    int (*push)(const CliGitChangesCfg *args);
    int (*rollback)(const CliGitChangesCfg *args);
} CliChangesNs;
EMBED_TABLE_LAYOUT(CliChangesNs, begin, stale, step, read, rows, files, row, toggle, open, taken, take_all, menu, type,
                   key, message, commit, refusal, sync, pull, push, rollback);
EMBED_TABLE_STORAGE CliChangesNs cli_changes EMBED_UNUSED = {
    .begin = cli_changes_begin,
    .stale = cli_changes_stale,
    .step = cli_changes_step,
    .read = cli_changes_read,
    .rows = cli_changes_rows,
    .files = cli_changes_files,
    .row = cli_changes_row,
    .toggle = cli_changes_toggle,
    .open = cli_changes_open,
    .taken = cli_changes_taken,
    .take_all = cli_changes_take_all,
    .menu = cli_changes_menu,
    .type = cli_changes_type,
    .key = cli_changes_key,
    .message = cli_changes_message,
    .commit = cli_changes_commit,
    .refusal = cli_changes_refusal,
    .sync = cli_changes_sync,
    .pull = cli_changes_pull,
    .push = cli_changes_push,
    .rollback = cli_changes_rollback,
};

// The Git pane as the window reads it (git_log.c): the repository chosen among the tree's, the one the tree is in
// until another is chosen, the branch it is on, every branch's commits as a graph, or those the query typed over the
// graph finds in the whole history, and under a commit opened the files it changed. The rows: the branch's row with the
// commits counted, a row a commit, and under one opened a row each file it changed, or the notice that none under the
// tree did. The query rests CLI_GIT_LOG_REST_MS after a change before the graph is read again, the watch marks the
// graph stale from its thread, and the authors setting, saved under `git.authors`, has each commit's author shown and
// searched. The rows point into the lists read from git, the repositories, the graph and each opened commit's files,
// which stand until the next read
#define CLI_GIT_LOG_QUERY_MAX 256u
#define CLI_GIT_LOG_REST_MS 250ULL
#define CLI_GIT_LOG_DAY 11u
// the rows' types; and the head's actions: the field, which takes the keys, and the repository choice, which moves on
// to the next repository
#define CLI_GIT_LOG_ROW_BRANCH 0u
#define CLI_GIT_LOG_ROW_COMMIT 1u
#define CLI_GIT_LOG_ROW_FILE 2u
#define CLI_GIT_LOG_ROW_NONE 3u
#define CLI_GIT_LOG_FIELD 1u
#define CLI_GIT_LOG_REPO 2u
// A row: its type; its text, the branch's name, the commit's subject, the file's name or the notice, and its bytes; a
// second text, the commits counted, the commit's author where authors show or the file's folder, and its bytes; a
// file's path from the tree's top and its state letter; and a commit's id, its day as YYYY-MM-DD, whether its files
// show, whether it merges, the column its dot stands in and the number of its line, how many columns its strokes
// cross, its strokes, each [from, to, half, line] as the graph lays them, and how many branches and tags stand at it.
// The text, the second text, the id and the day read off the pane's cells are copied into the row's own `text_copied`,
// `second_copied`, `id_copied` and `day_copied`, which their pointers then name; a file's path stands in `text_copied`
// with its name after its folder; the strokes are copied into `stroke_copied`, CLI_GIT_LOG_ROW_STROKES at the most
#define CLI_GIT_LOG_TEXT (1u << 14u)
#define CLI_GIT_LOG_ID 48u
#define CLI_GIT_LOG_ROW_STROKES 64u
typedef struct
{
    unsigned int type;
    const char *text;
    unsigned long long text_length;
    const char *second;
    unsigned long long second_length;
    const char *path;
    char state;
    const char *id;
    const char *day;
    char text_copied[CLI_GIT_LOG_TEXT];
    char second_copied[CLI_NAME_MAX_LEN];
    char id_copied[CLI_GIT_LOG_ID];
    char day_copied[CLI_GIT_LOG_DAY];
    int open;
    int merge;
    unsigned int lane_at;
    unsigned int color;
    unsigned int across;
    const unsigned int (*stroke)[4];
    unsigned int strokes;
    unsigned int refs;
    unsigned int stroke_copied[CLI_GIT_LOG_ROW_STROKES][4];
} CliGitLogRow;
// A name at a commit: the branch's or the tag's text, which points into `copied`, the name as git writes it copied out
// of the graph's texts, and its bytes, whether HEAD stands at it, and whether it is a tag
typedef struct
{
    const char *name;
    unsigned long long length;
    int head;
    int tag;
    char copied[CLI_NAME_MAX_LEN];
} CliGitLogRef;
typedef struct CliGitLogCfg CliGitLogCfg;
// the pane begun: `woken` wakes the view's thread as the watch marks the graph stale, and `entries` restore the
// authors setting
void cli_git_log_begin(const CliGitLogCfg *args);
// `size` bytes of `text` typed into the query at `now_ms`; and the key `name` pressed: Backspace erases the query's
// last letter and Enter ends the rest, each 1, any other key 0
void cli_git_log_type(const CliGitLogCfg *args);
int cli_git_log_key(const CliGitLogCfg *args);
// the head's action `which` taken at `now_ms` for the tree at `root`: the repository choice moves on to the next
// repository and the graph is read again
void cli_git_log_option(const CliGitLogCfg *args);
// the authors setting turned `on` or off at `now_ms`, saved, the graph read again; and whether it is on
void cli_git_log_authors_set(const CliGitLogCfg *args);
int cli_git_log_authors(const CliGitLogCfg *args);
// the query copied with its end into `text_out`, sized for CLI_GIT_LOG_QUERY_MAX: its bytes; and the repository chosen
// as `name: branch`, how many the tree has into `count_out`
unsigned long long cli_git_log_query(const CliGitLogCfg *args);
const char *cli_git_log_repo(const CliGitLogCfg *args);
// the repositories, the branch and the graph of the tree at `root` read now, as the pane shows; the step at `now_ms`
// while the pane is `on`: the graph read again where the query's rest has passed or the watch marked it stale, 1 where
// it was; when the rest passes, 0 where no change waits; and the graph marked stale from any thread
void cli_git_log_relist(const CliGitLogCfg *args);
int cli_git_log_step(const CliGitLogCfg *args);
unsigned long long cli_git_log_deadline_ms(const CliGitLogCfg *args);
void cli_git_log_stale(const CliGitLogCfg *args);
// how many rows the pane has; the row `at` into `row`: 1, or 0 past the last; the branch or tag `which` at the commit
// of the row `at` into `ref`: 1, or 0 past its last; the commit of the row `at` opened or closed, its files read from
// the tree at `root` as it first opens, and the commit whose id is `name` the same: 1, or 0 where the graph has none;
// the message of the commit whose id is `name` into `out`, read from the tree at `root`: 1, or 0 where git has none;
// and the menu of the row `at`
// built as the menus' own rows, a commit's Open Files or Close Files, Copy Commit ID and Copy Commit Message, a file's
// Copy Path: 1, or 0 where the row has none
unsigned int cli_git_log_rows(const CliGitLogCfg *args);
int cli_git_log_row(const CliGitLogCfg *args);
int cli_git_log_ref(const CliGitLogCfg *args);
void cli_git_log_toggle(const CliGitLogCfg *args);
int cli_git_log_toggle_id(const CliGitLogCfg *args);
int cli_git_log_message(const CliGitLogCfg *args);
int cli_git_log_menu(const CliGitLogCfg *args);
struct CliGitLogCfg
{
    void (*woken)(void);
    const CliStateEntries *entries;
    const char *text;
    unsigned long long size;
    const char *name;
    unsigned int which;
    int on;
    const char *root;
    unsigned long long now_ms;
    unsigned int at;
    CliGitLogRow *row;
    CliGitLogRef *ref;
    unsigned long long *length_out;
    unsigned int *count_out;
    VerbaLinea *out;
    char *text_out;
};

typedef struct
{
    void (*begin)(const CliGitLogCfg *args);
    void (*type)(const CliGitLogCfg *args);
    int (*key)(const CliGitLogCfg *args);
    void (*option)(const CliGitLogCfg *args);
    void (*authors_set)(const CliGitLogCfg *args);
    int (*authors)(const CliGitLogCfg *args);
    unsigned long long (*query)(const CliGitLogCfg *args);
    const char *(*repo)(const CliGitLogCfg *args);
    void (*relist)(const CliGitLogCfg *args);
    int (*step)(const CliGitLogCfg *args);
    unsigned long long (*deadline_ms)(const CliGitLogCfg *args);
    void (*stale)(const CliGitLogCfg *args);
    unsigned int (*rows)(const CliGitLogCfg *args);
    int (*row)(const CliGitLogCfg *args);
    int (*ref)(const CliGitLogCfg *args);
    void (*toggle)(const CliGitLogCfg *args);
    int (*toggle_id)(const CliGitLogCfg *args);
    int (*message)(const CliGitLogCfg *args);
    int (*menu)(const CliGitLogCfg *args);
} CliGitLogNs;
EMBED_TABLE_LAYOUT(CliGitLogNs, begin, type, key, option, authors_set, authors, query, repo, relist, step, deadline_ms,
                   stale, rows, row, ref, toggle, toggle_id, message, menu);
EMBED_TABLE_STORAGE CliGitLogNs cli_git_log EMBED_UNUSED = {
    .begin = cli_git_log_begin,
    .type = cli_git_log_type,
    .key = cli_git_log_key,
    .option = cli_git_log_option,
    .authors_set = cli_git_log_authors_set,
    .authors = cli_git_log_authors,
    .query = cli_git_log_query,
    .repo = cli_git_log_repo,
    .relist = cli_git_log_relist,
    .step = cli_git_log_step,
    .deadline_ms = cli_git_log_deadline_ms,
    .stale = cli_git_log_stale,
    .rows = cli_git_log_rows,
    .row = cli_git_log_row,
    .ref = cli_git_log_ref,
    .toggle = cli_git_log_toggle,
    .toggle_id = cli_git_log_toggle_id,
    .message = cli_git_log_message,
    .menu = cli_git_log_menu,
};

// the commit the tree at `root` stands at into `commit`, sized for CLI_NAME_MAX_LEN, its time in seconds since 1970,
// and whether the tree has no change past it: 1, or 0 where the tree is in no repository or has no commit
int cli_git_head(const CliGitRootCommitCfg *args);
// the branch the tree is on, or the short id of the commit it stands at where it is on none, into `name`, sized for
// CLI_NAME_MAX_LEN: 1, or 0 where git cannot determine
int cli_git_branch(const CliGitCfg *args);
// the name of the repository at `root` into `name`, sized for CLI_NAME_MAX_LEN: the last part of its `origin`
// remote's address, without a `.git` ending, the folder's name where it has no `origin`
void cli_git_name(const CliGitCfg *args);
// the repositories of the tree: the one the tree is in, where it is in one, then each folder under it, four deep at
// most, that contains a repository of its own, a submodule among them, by path; hidden folders and those a build or a
// package manager fills are not searched
void cli_git_repositories(const CliGitRootFoundRepositoriesCfg *args);
// the text of a file a merge left in conflict, as one of its stages stores it: 1 the base, 2 the branch merged into, 3
// the branch merged, into `out`: 1, or 0 where git has none
int cli_git_stage_text(const CliGitRootFileStageCfg *args);
// every file under the tree that differs from the last commit of the repository that contains it, by its path in the
// tree, for the `count` repositories at `repos` where `named`, or every one of the tree where not
void cli_git_change(const CliGitRootReposCfg *args);
// the files at `paths` committed with `message`, each as it stands, in the repository that contains it, a commit to
// each repository that contains some, each repository committed to pushed where `push`; and the files of `whole` as
// each stands and the files of `parts` with the text each is to carry, the same way and pushed the same way: git's
// line for each commit into `out`, 1, or 0 with why in `err_msg`
int cli_git_commit(const CliGitRootOutCountCfg *args);
int cli_git_commit_parts(const CliGitRootCfg *args);
// git run at `root` with nothing to answer a prompt with, `args`, `count` of them: what it wrote into `out`, 1, or 0
// with what it reported went wrong
int cli_git_run(const CliGitRootOutCountErrMsgCfg *args);
// every local and remote branch, the one with the newest commit first, less each remote's HEAD
void cli_git_branches(const CliGitRootFoundBranchesCfg *args);
// what the branches list requests done to a branch: `create` makes one from the branch open and goes on it, `switch`
// goes on one, a remote one made a local branch that follows it, `rename` names it `to`, `delete` takes it out where
// its commits are merged and `delete-unmerged` where they are not, `merge` merges it into the branch open, and `rebase`
// sets the branch open's own commits on it: git's words into `out`, 1, or 0 with why given
int cli_git_branch_act(const CliGitRootOutCfg *args);
// every stash, the newest first
void cli_git_stashes(const CliGitRootFoundStashesCfg *args);
// what is requested done to the stashes: `push` puts every change aside, new files with them, under `message` where one
// is given; `apply` brings stash `name` back and retains it; `pop` brings it back and drops it where it comes back
// whole; `drop` throws it away: git's words into `out`, 1, or 0 with why given
int cli_git_stash_act(const CliGitRootOutMessageCfg *args);
// the commits of other branches, local and remote, that the branch the tree is on does not contain, merges aside, the
// newest first; a commit already applied to it under another id is not one
void cli_git_elsewhere(const CliGitRootFileErrMsgCfg *args);
// commit `id` applied to the branch the tree is on as a commit of its own: git's words into `out`, 1, or 0 with why
// given, where the change does not fit leaving the files in conflict for the merge window
int cli_git_cherry_pick(const CliGitRootFileCfg *args);
// a file a merge left in conflict marked resolved, as it now stands, by staging it: 1, or 0 with why given
int cli_git_resolve(const CliGitRootErrMsgPathCfg *args);
// the branch pushed to the remote it follows, or to origin under its own name where it follows none; and the commits of
// the remote branch it follows brought in, only where its own commits are all among them: git's words into `out`, 1,
// or 0 with why given
int cli_git_push(const CliGitRootOutCountErrMsgCfg *args);
int cli_git_pull(const CliGitRootOutCountErrMsgCfg *args);
// how many commits the branch has that its remote does not, and the other way: 1, or 0 where it follows no remote
int cli_git_ahead_behind(const CliGitRootAheadCfg *args);
// the file at `path` put back as the last commit left it, its changes in the tree and in what is staged taken back; a
// file the last commit does not contain is refused: 1, or 0 with why given
int cli_git_rollback(const CliGitRootErrMsgPathCfg *args);
// the commits that touched `file`, the newest first, following it through renames: 1, or 0 with why given
int cli_git_commits(const CliGitRootFileErrMsgCfg *args);
// the text of `file` as commit `id` left it into `out`: 1, or 0 with why given; as the first parent of `id` left it: 1,
// or 0 where the commit has no parent or the parent does not contain the file; as the last commit left it: 1, or 0
// where the tree is not git's or the last commit does not contain the file
int cli_git_text_at(const CliGitRootFileCfg *args);
int cli_git_text_before(const CliGitRootFileCfg *args);
int cli_git_head_text(const CliGitRootFileStageCfg *args);
// the file at `path` of the tree at `root` as the last commit has it read on a worker and stored beside the text under
// `key` in the document store, none where the last commit does not contain it; the call returns at once
void cli_git_head_later(const char *root, const char *path, const char *key);
// who made commit `id` into `name`, and its subject into `subject`: 1, or 0 where git cannot determine
int cli_git_new_by(const CliGitRootIdCfg *args);

// the most commits the graph draws, and the most a search over them finds; the most parents and names of a commit
// stored; and the most strokes of every row
#define CLI_GIT_GRAPH 2000u

#define CLI_GIT_PARENTS 16u

#define CLI_GIT_REFS 32u

#define CLI_GIT_STROKES (1u << 17u)

// One commit of the graph: its id, its parents, its author, when its author made it in seconds since 1970, the
// branches and tags that stand at it, and its subject. `lane` is the column its dot stands in and `color` the number of
// the line it is on. Its strokes are `stroke_count` from `first_stroke` of the graph's, each `[from, to, half, color]`:
// from column `from` at the row's top to column `to` at its middle where `half` is 0, from `from` at the middle to `to`
// at the bottom where it is 1. `across` is how many columns the row crosses, the dot's and every stroke's ends
typedef struct
{
    unsigned int id;
    unsigned int parent[CLI_GIT_PARENTS];
    unsigned int parents;
    unsigned int author;
    long long when;
    unsigned int ref[CLI_GIT_REFS];
    unsigned int refs;
    unsigned int subject;
    unsigned int lane_at;
    unsigned int color;
    unsigned int first_stroke;
    unsigned int stroke_count;
    unsigned int across;
} CliDrawn;

typedef struct
{
    CliDrawn item[CLI_GIT_GRAPH];
    unsigned int count;
    unsigned int stroke[CLI_GIT_STROKES][4];
    unsigned int strokes;
    CliGitText text;
} CliGraph;

// the commits of every branch, the newest first and none before its children, laid out as a graph; with a `query`, the
// commits whose subject, id or branches contain each of its words, in any case, and their author where `authors`, from
// the whole history, each on a line of its own (git_graph.c)
void cli_git_graph(const CliGitRootQueryCfg *args);

// the most files a commit is reported to have changed
#define CLI_GIT_TOUCHED 16384u

// A file a commit changed from its first parent, or from nothing where it has none: its path under the tree, its state
// letter (M changed, A added, D gone, R renamed), and the path it had before where it was renamed
typedef struct
{
    unsigned int path;
    char state;
    unsigned int was;
    int has_was;
} CliTouched;

typedef struct
{
    CliTouched item[CLI_GIT_TOUCHED];
    unsigned int count;
    CliGitText text;
} CliTouchedList;

// the files under the tree that commit `id` changed: 1, or 0 with why given (git_lists.c)
int cli_git_touch(const CliGitRootErrMsgCfg *args);

// the most lines of a file and the most commits its history contains
#define CLI_GIT_LINES (1u << 20u)

#define CLI_GIT_LINE_COMMITS 65536u

// A commit that last changed some line of a file: its id, author, when its author made it in seconds since 1970, and
// subject; the file's path under the tree as the commit left it, and the commit before it with the path the file had
// there, where there is one
typedef struct
{
    unsigned int id;
    unsigned int author;
    long long when;
    unsigned int subject;
    unsigned int path;
    int has_path;
    unsigned int parent;
    int has_parent;
    unsigned int was;
    int has_was;
} CliLineCommit;

// The commit that last changed each line of a file: `line` stores, for each line, its commit's place in `commit`, or -1
// for a line no commit contains yet
typedef struct
{
    CliLineCommit commit[CLI_GIT_LINE_COMMITS];
    unsigned int commits;
    int line[CLI_GIT_LINES];
    unsigned int lines;
    CliGitText text;
} CliLineHistory;

// the commit that last changed each line of `file` as `text`, `length` bytes, contains it, the file as the editor has
// it, edits and all, a line the last commit does not contain being no commit's: 1, or 0 with why given (git_lines.c)
int cli_git_line_history(const CliGitLengthCfg *args);

// the folder a clone goes in where none is named, the reader's home, and the folder File, Open, Repository clones
// into, one of orior's own, apart from the reader's, each into `folder`, sized for CLI_PATH_MAX_LEN: 1, or 0 where
// there is none; `folder` made a repository, as git init does, a folder that is one already left as it is; the folder
// a clone of `url` makes under `parent` into `folder`, the last part of the address without `.git`, as git itself
// names it; and `url` cloned into `target`, a folder that is not there yet or is empty, each line of git's progress
// handed to `progress_line` as it comes with whether the line is finished. Each 1, or 0 with why given (git_clone.c)
int cli_git_clone_parent(const CliGitErrMsgFolderCfg *args);
int cli_git_opened_parent(const CliGitErrMsgFolderCfg *args);
int cli_git_init(const CliGitOutCfg *args);
int cli_git_clone_folder(const CliGitErrMsgFolderCfg *args);
int cli_git_clone(const CliGitErrMsgCfg *args);

struct CliGitRootCfg
{
    const char *root;
    VerbaLinea *out;
    const char *message;
    VerbaLinea *err_msg;
    const char *const *whole;
    unsigned int full_files_count;
    const CliGitPart *parts;
    unsigned int part_count;
    int push;
};

struct CliGitRootOutCfg
{
    const char *root;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const char *act;
    const char *name_read;
    const char *to;
};

struct CliGitRootOutCountCfg
{
    const char *root;
    VerbaLinea *out;
    unsigned int count;
    const char *message;
    const char *const *paths;
    VerbaLinea *err_msg;
    int push;
};

struct CliGitLengthCfg
{
    unsigned long long length;
    const char *root;
    const char *file;
    VerbaLinea *err_msg;
    const char *text_text;
    CliLineHistory *found_line_history_pointer;
};

struct CliGitRootOutMessageCfg
{
    const char *root;
    VerbaLinea *out;
    const char *message;
    VerbaLinea *err_msg;
    const char *act;
    const char *name_read;
};

struct CliGitRootReposCfg
{
    const char *root;
    const char *const *repos;
    unsigned int count;
    int named;
    CliChangedList *found_changed_list_pointer;
};

struct CliGitErrMsgCfg
{
    VerbaLinea *err_msg;
    const char *url;
    const char *target;
    void (*progress_line)(const CliEachLineEndedCfg *args);
    void *context;
};

struct CliGitRootOutCountErrMsgCfg
{
    const char *root;
    VerbaLinea *out;
    unsigned int count;
    VerbaLinea *err_msg;
    const char *const *request;
};

struct CliGitRootFileCfg
{
    const char *root;
    const char *file;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const char *id;
};

struct CliGitErrMsgFolderCfg
{
    VerbaLinea *err_msg;
    char *folder;
    const char *url;
    const char *parent;
};

struct CliGitRootFileErrMsgCfg
{
    const char *root;
    const char *file;
    VerbaLinea *err_msg;
    CliCommits *found;
};

struct CliGitRootQueryCfg
{
    const char *root;
    const char *query;
    int authors;
    CliGraph *found_graph_pointer;
};

struct CliGitRootCommitCfg
{
    const char *root;
    char *commit;
    long long *when;
    int *clean;
};

struct CliGitRootIdCfg
{
    const char *root;
    const char *id;
    VerbaLinea *name_verba_linea_pointer;
    VerbaLinea *subject;
};

struct CliGitRootFileStageCfg
{
    const char *root;
    const char *file;
    unsigned int stage;
    VerbaLinea *out;
};

struct CliGitRootErrMsgCfg
{
    const char *root;
    VerbaLinea *err_msg;
    const char *id;
    CliTouchedList *found_touched_list_pointer;
};

struct CliGitRootAheadCfg
{
    const char *root;
    unsigned int *ahead;
    unsigned int *behind;
};

struct CliGitOutCfg
{
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const char *folder_read;
};

struct CliGitRootErrMsgPathCfg
{
    const char *root;
    VerbaLinea *err_msg;
    const char *path;
};

struct CliGitTextCfg
{
    CliGitText *text;
    const char *from;
    unsigned long long length;
};

struct CliGitOutFoundTouchedListCfg
{
    VerbaLinea *out;
    const CliTouchedList *found_touched_list_pointer_other;
    const char *repo;
};

struct CliGitCfg
{
    const char *root;
    char *name;
};

struct CliGitRootFoundBranchesCfg
{
    const char *root;
    CliBranches *found_branches_pointer;
};

struct CliGitOutFoundBranchesCfg
{
    VerbaLinea *out;
    const CliBranches *found_branches_pointer_other;
};

struct CliGitOutFoundChangedListCfg
{
    VerbaLinea *out;
    const CliChangedList *found_changed_list_pointer_other;
};

struct CliGitOutFoundCfg
{
    VerbaLinea *out;
    const CliCommits *found_read;
};

struct CliGitOutFoundGraphCfg
{
    VerbaLinea *out;
    const CliGraph *found_graph_pointer_other;
};

struct CliGitOutFoundLineHistoryCfg
{
    VerbaLinea *out;
    const CliLineHistory *found_line_history_pointer_other;
};

struct CliGitRootFoundRepositoriesCfg
{
    const char *root;
    CliRepositories *found_repositories_pointer;
};

struct CliGitOutFoundRepositoriesCfg
{
    VerbaLinea *out;
    const CliRepositories *found_repositories_pointer_other;
};

struct CliGitRootFoundStashesCfg
{
    const char *root;
    CliStashes *found_stashes_pointer;
};

struct CliGitOutFoundStashesCfg
{
    VerbaLinea *out;
    const CliStashes *found_stashes_pointer_other;
};

struct CliGitTextAtCfg
{
    const CliGitText *text_read;
    unsigned int at;
    char *dst;
    unsigned long long capacity;
};

typedef struct
{
    void (*text_begin)(const CliGitTextCfg *args);
    unsigned int (*text_append)(const CliGitTextCfg *args);
    void (*text_end)(const CliGitTextCfg *args);
    void (*text_release)(const CliGitTextCfg *args);
} CliGitTextNs;
EMBED_TABLE_LAYOUT(CliGitTextNs, text_begin, text_append, text_end, text_release);
EMBED_TABLE_STORAGE CliGitTextNs cli_git_text EMBED_UNUSED = {
    .text_begin = cli_git_text_begin,
    .text_append = cli_git_text_append,
    .text_end = cli_git_text_end,
    .text_release = cli_git_text_release,
};

typedef struct
{
    const char *(*text_get)(const CliGitTextAtCfg *args);
} CliGitTextAtNs;
EMBED_TABLE_LAYOUT(CliGitTextAtNs, text_get);
EMBED_TABLE_STORAGE CliGitTextAtNs cli_git_text_at_table EMBED_UNUSED = {
    .text_get = cli_git_text_get,
};

typedef struct
{
    int (*head)(const CliGitRootCommitCfg *args);
} CliGitRootCommitNs;
EMBED_TABLE_LAYOUT(CliGitRootCommitNs, head);
EMBED_TABLE_STORAGE CliGitRootCommitNs cli_git_root_commit EMBED_UNUSED = {
    .head = cli_git_head,
};

typedef struct
{
    int (*branch)(const CliGitCfg *args);
    void (*name)(const CliGitCfg *args);
} CliGitNs;
EMBED_TABLE_LAYOUT(CliGitNs, branch, name);
EMBED_TABLE_STORAGE CliGitNs cli_git_table EMBED_UNUSED = {
    .branch = cli_git_branch,
    .name = cli_git_name,
};

typedef struct
{
    void (*repositories)(const CliGitRootFoundRepositoriesCfg *args);
} CliGitRootFoundRepositoriesNs;
EMBED_TABLE_LAYOUT(CliGitRootFoundRepositoriesNs, repositories);
EMBED_TABLE_STORAGE CliGitRootFoundRepositoriesNs cli_git_root_found_repositories EMBED_UNUSED = {
    .repositories = cli_git_repositories,
};

typedef struct
{
    int (*stage_text)(const CliGitRootFileStageCfg *args);
    int (*head_text)(const CliGitRootFileStageCfg *args);
} CliGitRootFileStageNs;
EMBED_TABLE_LAYOUT(CliGitRootFileStageNs, stage_text, head_text);
EMBED_TABLE_STORAGE CliGitRootFileStageNs cli_git_root_file_stage EMBED_UNUSED = {
    .stage_text = cli_git_stage_text,
    .head_text = cli_git_head_text,
};

typedef struct
{
    void (*change)(const CliGitRootReposCfg *args);
} CliGitRootReposNs;
EMBED_TABLE_LAYOUT(CliGitRootReposNs, change);
EMBED_TABLE_STORAGE CliGitRootReposNs cli_git_root_repos EMBED_UNUSED = {
    .change = cli_git_change,
};

typedef struct
{
    int (*commit)(const CliGitRootOutCountCfg *args);
} CliGitRootOutCountNs;
EMBED_TABLE_LAYOUT(CliGitRootOutCountNs, commit);
EMBED_TABLE_STORAGE CliGitRootOutCountNs cli_git_root_out_count EMBED_UNUSED = {
    .commit = cli_git_commit,
};

typedef struct
{
    int (*commit_parts)(const CliGitRootCfg *args);
} CliGitRootNs;
EMBED_TABLE_LAYOUT(CliGitRootNs, commit_parts);
EMBED_TABLE_STORAGE CliGitRootNs cli_git_root EMBED_UNUSED = {
    .commit_parts = cli_git_commit_parts,
};

typedef struct
{
    int (*run)(const CliGitRootOutCountErrMsgCfg *args);
    int (*push)(const CliGitRootOutCountErrMsgCfg *args);
    int (*pull)(const CliGitRootOutCountErrMsgCfg *args);
} CliGitRootOutCountErrMsgNs;
EMBED_TABLE_LAYOUT(CliGitRootOutCountErrMsgNs, run, push, pull);
EMBED_TABLE_STORAGE CliGitRootOutCountErrMsgNs cli_git_root_out_count_err_msg EMBED_UNUSED = {
    .run = cli_git_run,
    .push = cli_git_push,
    .pull = cli_git_pull,
};

typedef struct
{
    void (*branches)(const CliGitRootFoundBranchesCfg *args);
} CliGitRootFoundBranchesNs;
EMBED_TABLE_LAYOUT(CliGitRootFoundBranchesNs, branches);
EMBED_TABLE_STORAGE CliGitRootFoundBranchesNs cli_git_root_found_branches EMBED_UNUSED = {
    .branches = cli_git_branches,
};

typedef struct
{
    int (*branch_act)(const CliGitRootOutCfg *args);
} CliGitRootOutNs;
EMBED_TABLE_LAYOUT(CliGitRootOutNs, branch_act);
EMBED_TABLE_STORAGE CliGitRootOutNs cli_git_root_out EMBED_UNUSED = {
    .branch_act = cli_git_branch_act,
};

typedef struct
{
    void (*stashes)(const CliGitRootFoundStashesCfg *args);
} CliGitRootFoundStashesNs;
EMBED_TABLE_LAYOUT(CliGitRootFoundStashesNs, stashes);
EMBED_TABLE_STORAGE CliGitRootFoundStashesNs cli_git_root_found_stashes EMBED_UNUSED = {
    .stashes = cli_git_stashes,
};

typedef struct
{
    int (*stash_act)(const CliGitRootOutMessageCfg *args);
} CliGitRootOutMessageNs;
EMBED_TABLE_LAYOUT(CliGitRootOutMessageNs, stash_act);
EMBED_TABLE_STORAGE CliGitRootOutMessageNs cli_git_root_out_message EMBED_UNUSED = {
    .stash_act = cli_git_stash_act,
};

typedef struct
{
    void (*elsewhere)(const CliGitRootFileErrMsgCfg *args);
    int (*commits)(const CliGitRootFileErrMsgCfg *args);
} CliGitRootFileErrMsgNs;
EMBED_TABLE_LAYOUT(CliGitRootFileErrMsgNs, elsewhere, commits);
EMBED_TABLE_STORAGE CliGitRootFileErrMsgNs cli_git_root_file_err_msg EMBED_UNUSED = {
    .elsewhere = cli_git_elsewhere,
    .commits = cli_git_commits,
};

typedef struct
{
    int (*cherry_pick)(const CliGitRootFileCfg *args);
    int (*text_at)(const CliGitRootFileCfg *args);
    int (*text_before)(const CliGitRootFileCfg *args);
} CliGitRootFileNs;
EMBED_TABLE_LAYOUT(CliGitRootFileNs, cherry_pick, text_at, text_before);
EMBED_TABLE_STORAGE CliGitRootFileNs cli_git_root_file EMBED_UNUSED = {
    .cherry_pick = cli_git_cherry_pick,
    .text_at = cli_git_text_at,
    .text_before = cli_git_text_before,
};

typedef struct
{
    int (*resolve)(const CliGitRootErrMsgPathCfg *args);
    int (*rollback)(const CliGitRootErrMsgPathCfg *args);
} CliGitRootErrMsgPathNs;
EMBED_TABLE_LAYOUT(CliGitRootErrMsgPathNs, resolve, rollback);
EMBED_TABLE_STORAGE CliGitRootErrMsgPathNs cli_git_root_err_msg_path EMBED_UNUSED = {
    .resolve = cli_git_resolve,
    .rollback = cli_git_rollback,
};

typedef struct
{
    int (*ahead_behind)(const CliGitRootAheadCfg *args);
} CliGitRootAheadNs;
EMBED_TABLE_LAYOUT(CliGitRootAheadNs, ahead_behind);
EMBED_TABLE_STORAGE CliGitRootAheadNs cli_git_root_ahead EMBED_UNUSED = {
    .ahead_behind = cli_git_ahead_behind,
};

typedef struct
{
    int (*new_by)(const CliGitRootIdCfg *args);
} CliGitRootIdNs;
EMBED_TABLE_LAYOUT(CliGitRootIdNs, new_by);
EMBED_TABLE_STORAGE CliGitRootIdNs cli_git_root_id EMBED_UNUSED = {
    .new_by = cli_git_new_by,
};

typedef struct
{
    void (*graph)(const CliGitRootQueryCfg *args);
} CliGitRootQueryNs;
EMBED_TABLE_LAYOUT(CliGitRootQueryNs, graph);
EMBED_TABLE_STORAGE CliGitRootQueryNs cli_git_root_query EMBED_UNUSED = {
    .graph = cli_git_graph,
};

typedef struct
{
    int (*touch)(const CliGitRootErrMsgCfg *args);
} CliGitRootErrMsgNs;
EMBED_TABLE_LAYOUT(CliGitRootErrMsgNs, touch);
EMBED_TABLE_STORAGE CliGitRootErrMsgNs cli_git_root_err_msg EMBED_UNUSED = {
    .touch = cli_git_touch,
};

typedef struct
{
    int (*line_history)(const CliGitLengthCfg *args);
} CliGitLengthNs;
EMBED_TABLE_LAYOUT(CliGitLengthNs, line_history);
EMBED_TABLE_STORAGE CliGitLengthNs cli_git_length EMBED_UNUSED = {
    .line_history = cli_git_line_history,
};

typedef struct
{
    int (*clone_parent)(const CliGitErrMsgFolderCfg *args);
    int (*opened_parent)(const CliGitErrMsgFolderCfg *args);
    int (*clone_folder)(const CliGitErrMsgFolderCfg *args);
} CliGitErrMsgFolderNs;
EMBED_TABLE_LAYOUT(CliGitErrMsgFolderNs, clone_parent, opened_parent, clone_folder);
EMBED_TABLE_STORAGE CliGitErrMsgFolderNs cli_git_err_msg_folder EMBED_UNUSED = {
    .clone_parent = cli_git_clone_parent,
    .opened_parent = cli_git_opened_parent,
    .clone_folder = cli_git_clone_folder,
};

typedef struct
{
    int (*init)(const CliGitOutCfg *args);
} CliGitOutNs;
EMBED_TABLE_LAYOUT(CliGitOutNs, init);
EMBED_TABLE_STORAGE CliGitOutNs cli_git_out EMBED_UNUSED = {
    .init = cli_git_init,
};

typedef struct
{
    int (*clone)(const CliGitErrMsgCfg *args);
} CliGitErrMsgNs;
EMBED_TABLE_LAYOUT(CliGitErrMsgNs, clone);
EMBED_TABLE_STORAGE CliGitErrMsgNs cli_git_err_msg EMBED_UNUSED = {
    .clone = cli_git_clone,
};

#endif

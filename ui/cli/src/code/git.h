// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git.h: what git says of the tree: the branch it is on, the files that differ from the last commit and how, the
// commits that touched a file, every branch's commits laid out as a graph and the files each changed, the commit that
// last changed each line of a file, and a file's text as one of those commits left it or as the last did. In a tree
// git cannot read, each of these comes back empty. What git is asked to do, for the Commit window and the Git menu:
// commit chosen files, push, pull where nothing would merge, put a file back as the last commit left it, act on
// branches, put changes aside in a stash and bring them back, and apply a commit of another branch, each giving git's
// own words where it refuses. Here too is the clone of a repository into a folder of its own, for File, Clone
// Repository and `orior file clone` (git.c, git_run.c, git_commit.c, git_branches.c, git_lists.c, git_graph.c,
// git_lines.c, git_clone.c)
#ifndef GIT_H
#define GIT_H

#include "../command_line/cli_internal.h"

// the repository File, Clone Repository offers first
#define CLI_GIT_ORIOR "https://github.com/dstroy0/orior.git"

// the most bytes of the texts of one list git gives
#define CLI_GIT_TEXT (1u << 22u)

// The texts of a list: each one ended by a 0 at its place, the place 0 holding the empty text, and 1 in `full` where
// one did not fit and was kept empty
typedef struct
{
    char bytes[CLI_GIT_TEXT];
    unsigned long long used;
    int full;
} CliGitText;

// the texts made empty; `length` bytes of `from` kept: their place; and the text at a place
void cli_git_text_begun(CliGitText *text);
unsigned int cli_git_text_put(CliGitText *text, const char *from, unsigned long long length);
const char *cli_git_text_of(const CliGitText *text, unsigned int at);

// the most repositories, changed files, branches, stashes and commits a list holds
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

// A file that differs from the last commit. Its state is one letter: M changed, A added, D gone, R renamed, U not yet
// tracked, C in conflict
typedef struct
{
    unsigned int path;
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

// A file the next commit takes in part: its path and the text the commit gives it, `length` bytes
typedef struct
{
    const char *path;
    const char *text;
    unsigned long long length;
} CliGitPart;

// the commit the tree at `root` stands at into `commit`, which holds CLI_NAME_LONGEST, its time in seconds since 1970,
// and whether the tree holds no change past it: 1, or 0 where the tree is in no repository or has no commit
int cli_git_head(const char *root, char *commit, long long *when, int *clean);
// the branch the tree is on, or the short id of the commit it stands at where it is on none, into `name`, which holds
// CLI_NAME_LONGEST: 1, or 0 where git cannot say
int cli_git_branch(const char *root, char *name);
// the name of the repository at `root` into `name`, which holds CLI_NAME_LONGEST: the last part of its `origin`
// remote's address, without a `.git` ending, the folder's name where it has no `origin`
void cli_git_name(const char *root, char *name);
// the repositories of the tree: the one the tree is in, where it is in one, then each folder under it, four deep at
// most, that holds a repository of its own, a submodule among them, by path; hidden folders and those a build or a
// package manager fills are not searched
void cli_git_repositories(const char *root, CliRepositories *found);
// the text of a file a merge left in conflict, as one of its stages holds it: 1 the base, 2 the branch merged into, 3
// the branch merged, into `out`: 1, or 0 where git has none
int cli_git_stage_text(const char *root, const char *file, unsigned int stage, CliLine *out);
// every file under the tree that differs from the last commit of the repository that holds it, by its path in the tree,
// for the `count` repositories at `repos` where `named`, or every one of the tree where not
void cli_git_changed(const char *root, const char *const *repos, unsigned int count, int named, CliChangedList *found);
// the files at `paths` committed with `message`, each as it stands, in the repository that holds it, a commit to each
// repository that holds some; and the files of `whole` as each stands and the files of `parts` with the text each is
// given, the same way: git's line for each commit into `out`, 1, or 0 with why said
int cli_git_commit(const char *root, const char *message, const char *const *paths, unsigned int count, CliLine *out,
                   CliLine *said);
int cli_git_commit_parts(const char *root, const char *message, const char *const *whole, unsigned int whole_count,
                         const CliGitPart *parts, unsigned int part_count, CliLine *out, CliLine *said);
// git run at `root` with nothing to answer a prompt with, `args`, `count` of them: what it wrote into `out`, 1, or 0
// with what it said went wrong
int cli_git_run(const char *root, const char *const *args, unsigned int count, CliLine *out, CliLine *said);
// every local and remote branch, the one with the newest commit first, less each remote's HEAD
void cli_git_branches(const char *root, CliBranches *found);
// what the branches list asks done to a branch: `create` makes one from the branch open and goes on it, `switch` goes
// on one, a remote one made a local branch that follows it, `rename` names it `to`, `delete` takes it out where its
// commits are merged and `delete-unmerged` where they are not, `merge` merges it into the branch open, and `rebase`
// sets the branch open's own commits on it: git's words into `out`, 1, or 0 with why said
int cli_git_branch_act(const char *root, const char *act, const char *name, const char *to, CliLine *out,
                       CliLine *said);
// every stash, the newest first
void cli_git_stashes(const char *root, CliStashes *found);
// what is asked done to the stashes: `push` puts every change aside, new files with them, under `message` where one is
// given; `apply` brings stash `name` back and keeps it; `pop` brings it back and drops it where it comes back whole;
// `drop` throws it away: git's words into `out`, 1, or 0 with why said
int cli_git_stash_act(const char *root, const char *act, const char *name, const char *message, CliLine *out,
                      CliLine *said);
// the commits of other branches, local and remote, that the branch the tree is on does not hold, merges aside, the
// newest first; a commit already applied to it under another id is not one
void cli_git_elsewhere(const char *root, CliCommits *found);
// commit `id` applied to the branch the tree is on as a commit of its own: git's words into `out`, 1, or 0 with why
// said, where the change does not fit leaving the files in conflict for the merge window
int cli_git_cherry_pick(const char *root, const char *id, CliLine *out, CliLine *said);
// a file a merge left in conflict marked resolved, as it now stands, by staging it: 1, or 0 with why said
int cli_git_resolve(const char *root, const char *path, CliLine *said);
// the branch pushed to the remote it follows, or to origin under its own name where it follows none; and the commits of
// the remote branch it follows brought in, only where its own commits are all among them: git's words into `out`, 1,
// or 0 with why said
int cli_git_push(const char *root, CliLine *out, CliLine *said);
int cli_git_pull(const char *root, CliLine *out, CliLine *said);
// how many commits the branch has that its remote does not, and the other way: 1, or 0 where it follows no remote
int cli_git_ahead_behind(const char *root, unsigned int *ahead, unsigned int *behind);
// the file at `path` put back as the last commit left it, its changes in the tree and in what is staged taken back; a
// file the last commit does not hold is refused: 1, or 0 with why said
int cli_git_rollback(const char *root, const char *path, CliLine *said);
// the commits that touched `file`, the newest first, following it through renames: 1, or 0 with why said
int cli_git_commits(const char *root, const char *file, CliCommits *found, CliLine *said);
// the text of `file` as commit `id` left it into `out`: 1, or 0 with why said; as the first parent of `id` left it: 1,
// or 0 where the commit has no parent or the parent does not hold the file; as the last commit left it: 1, or 0 where
// the tree is not git's or the last commit does not hold the file
int cli_git_text_at(const char *root, const char *file, const char *id, CliLine *out, CliLine *said);
int cli_git_text_before(const char *root, const char *file, const char *id, CliLine *out);
int cli_git_head_text(const char *root, const char *file, CliLine *out);
// who made commit `id` into `name`, and its subject into `subject`: 1, or 0 where git cannot say
int cli_git_made_by(const char *root, const char *id, CliLine *name, CliLine *subject);

// the most commits the graph draws, and the most a search over them finds; the most parents and names of a commit
// kept; and the most strokes of every row
#define CLI_GIT_GRAPH 2000u

#define CLI_GIT_PARENTS 16u

#define CLI_GIT_REFS 32u

#define CLI_GIT_STROKES (1u << 17u)

// One commit of the graph: its id, its parents, its author, when its author made it in seconds since 1970, the
// branches and tags that stand at it, and its subject. `lane` is the column its dot stands in and `color` the number of
// the line it is on. Its strokes are `stroke_count` from `first_stroke` of the graph's, each `[from, to, half, color]`:
// from column `from` at the row's top to column `to` at its middle where `half` is 0, from `from` at the middle to `to`
// at the bottom where it is 1
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
    unsigned int lane;
    unsigned int color;
    unsigned int first_stroke;
    unsigned int stroke_count;
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
// commits whose subject, id or branches hold each of its words, in any case, and their author where `authors`, from the
// whole history, each on a line of its own (git_graph.c)
void cli_git_graph(const char *root, const char *query, int authors, CliGraph *found);

// the most files a commit is told to have changed
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

// the files under the tree that commit `id` changed: 1, or 0 with why said (git_lists.c)
int cli_git_touched(const char *root, const char *id, CliTouchedList *found, CliLine *said);

// the most lines of a file and the most commits its history holds
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

// The commit that last changed each line of a file: `line` holds, for each line, its commit's place in `commit`, or -1
// for a line no commit holds yet
typedef struct
{
    CliLineCommit commit[CLI_GIT_LINE_COMMITS];
    unsigned int commits;
    int line[CLI_GIT_LINES];
    unsigned int lines;
    CliGitText text;
} CliLineHistory;

// the commit that last changed each line of `file` as `text`, `length` bytes, holds it, the file as the editor has it,
// edits and all, a line the last commit does not hold being no commit's: 1, or 0 with why said (git_lines.c)
int cli_git_line_history(const char *root, const char *file, const char *text, unsigned long long length,
                         CliLineHistory *found, CliLine *said);

// the folder a clone goes in where none is named, the reader's home, and the folder File, Open, Repository clones
// into, one of orior's own, apart from the reader's, each into `folder`, which holds CLI_PATH_LONGEST: 1, or 0 where
// there is none; `folder` made a repository, as git init does, a folder that is one already left as it is; the folder
// a clone of `url` makes under `parent` into `folder`, the last part of the address without `.git`, as git itself
// names it; and `url` cloned into `target`, a folder that is not there yet or is empty, each line of git's progress
// handed to `said_line` as it comes with whether the line is finished. Each 1, or 0 with why said (git_clone.c)
int cli_git_clone_parent(char *folder);
int cli_git_opened_parent(char *folder);
int cli_git_init(const char *folder, CliLine *out, CliLine *said);
int cli_git_clone_folder(const char *url, const char *parent, char *folder, CliLine *said);
int cli_git_clone(const char *url, const char *target, void (*said_line)(void *context, const char *line, int ended),
                  void *context, CliLine *said);

// each list written as the page reads it, a JSON array of objects whose members are the fields above by their names, a
// state a letter of its own, a path or a branch one has not `null`; a touched file's path and the path it had each
// written after `repo` and a `/` where `repo` is not empty; and a file's line history as `{"commits", "lines"}`
// (git_written.c)
void cli_git_repositories_written(const CliRepositories *found, CliLine *out);
void cli_git_changed_written(const CliChangedList *found, CliLine *out);
void cli_git_commits_written(const CliCommits *found, CliLine *out);
void cli_git_branches_written(const CliBranches *found, CliLine *out);
void cli_git_stashes_written(const CliStashes *found, CliLine *out);
void cli_git_graph_written(const CliGraph *found, CliLine *out);
void cli_git_touched_written(const CliTouchedList *found, const char *repo, CliLine *out);
void cli_git_line_history_written(const CliLineHistory *found, CliLine *out);

#endif

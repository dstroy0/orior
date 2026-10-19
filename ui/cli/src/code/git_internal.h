// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_internal.h: what the git sources share: git started and what it gave read (git_run.c); and what the git check
// reaches: a status's letter and the graph laid out
#ifndef GIT_INTERNAL_H
#define GIT_INTERNAL_H

#include "git.h"

// the most bytes of what git writes that are kept, and of what it says went wrong
#define CLI_GIT_OUT (1u << 24u)

#define CLI_GIT_ERR (1u << 16u)

// how git is started: with no optional lock taken, as a question that changes nothing asks it; with no terminal prompt
// to answer
#define CLI_GIT_LOCKS_OFF 1u

#define CLI_GIT_PROMPT_OFF 2u

// What git gave: what it wrote, `out_size` bytes of CLI_GIT_OUT, ended by a 0, and 0 in `out_whole` where more came
// than is kept; what it said went wrong, the same of CLI_GIT_ERR; and whether it ended with success
typedef struct
{
    char *out;
    unsigned long long out_size;
    int out_whole;
    char *err;
    unsigned long long err_size;
    int ok;
} CliGitGave;

// what one run of git gave, in a cell taken for the run, and given back; where the cells cannot meet a run, an empty
// one every run of git refuses
CliGitGave *cli_git_taken(void);
void cli_git_given(CliGitGave *gave);
// git at `root` started with the words trust gives, then `args`, `count` of them, as `how` says, with GIT_INDEX_FILE
// set to `index` where it is given, and `length` bytes of `input` on its standard input where it is given, else
// nothing: what it gave into `gave`, 1, or 0 with why it did not start said
int cli_git_gave(const char *root, const char *const *args, unsigned int count, unsigned int how, const char *index,
                 const char *input, unsigned long long length, CliGitGave *gave, CliLine *said);
// git at `root` asked a question that changes nothing, `args`, `count` of them, with no lock taken: what it wrote into
// `gave`, 1, or 0 where it failed or wrote more than is kept
int cli_git_asked(const char *root, const char *const *args, unsigned int count, CliGitGave *gave);
// git at `root` run with `args`, with nothing to answer a prompt with, an index of its own where `index` is given and
// `input` on its standard input where it is given: what it wrote and said, trimmed, into `out`, 1, or 0 with what it
// said went wrong, trimmed, or that it failed where it said nothing
int cli_git_ran(const char *root, const char *const *args, unsigned int count, const char *index, const char *input,
                unsigned long long length, CliLine *out, CliLine *said);
// the bytes from `*from` to `*to` with the whitespace at either end left off
void cli_git_trimmed(const char **from, const char **to);
// the repository of the `count` at `repos` that holds the first `length` bytes of `path`, both paths in the tree: the
// deepest whose folder holds it, or NULL where none does (git.c)
const char *cli_git_owner(const char *const *repos, unsigned int count, const char *path, unsigned long long length);
// the field from `*at` to the next `stop` or `end`: its start, its length into `length`, and `*at` moved past the
// stop; NULL where the fields have ended. And a whole number as written, a sign before it or none: 1 with its value
// into `value`, or 0 where it is no number
const char *cli_git_field(const char **at, const char *end, char stop, unsigned long long *length);
int cli_git_seconds(const char *text, unsigned long long length, long long *value);
// whether `id` is a commit's id as written: not empty and only hex digits
int cli_git_is_id(const char *id);
// the folder git is asked about a file from into `folder`, and the file's path from there, `./` before it, into
// `shown`, each holding CLI_PATH_LONGEST: the file's own folder, or the nearest one above it that is there where that
// one is gone. 1, or 0 with why said where the file is not inside the tree
int cli_git_beside(const char *root, const char *file, char *folder, char *shown, CliLine *said);
// the state letter for a porcelain status's two columns (git.c)
char cli_git_state_of(char index, char tree);
// each commit of the graph given its column, its color and the strokes of its row (git_graph.c)
void cli_git_laid_out(CliGraph *found);

#endif

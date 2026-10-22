// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// purpose.h: what a file is for, before it is opened: what its first comment says, and which files of the tree use it.
//
// The first comment is the first that stands before the file's code, read with the comment marks of the file's
// language as its plugin names them, past a first line of `#!`, and past a comment that states a copyright or a
// license; a Python module's docstring counts as a comment, and a Markdown file's first paragraph under its headings
// stands for one. Its lines are joined into sentences, and the first few of them are given.
//
// A file uses another where it names the other's file name, as an import of a script, an include of a header, a link
// or a stylesheet does, or where a line that imports, uses, includes or inputs names the other's name without its
// extension as a word, as Python's imports, Rust's modules and TeX's inputs do. The files read are the tree's text
// files, each up to 512 KiB (purpose.c, purpose_comment.c)
#ifndef PURPOSE_H
#define PURPOSE_H

#include "../command_line/cells.h"
#include "../command_line/cli_internal.h"

// the most bytes of a file read for its comment, and of each file read for its uses; the most letters of a comment
// given, and the most users named
#define CLI_PURPOSE_HEAD (64u * 1024u)
#define CLI_PURPOSE_READ (512u * 1024u)
#define CLI_PURPOSE_SAID 600u
#define CLI_PURPOSE_USERS 20u

// what the first comment of `size` bytes of `text`, the text of the file at `path`, says, past a license's comment,
// written into `out`; and the cells the room it is read in holds (purpose_comment.c)
void cli_purpose_first_comment(const char *path, const char *text, unsigned long long size, CliLine *out);
extern CliCellsTally g_cli_purpose_cells;

// whether `size` bytes of `text` use the file named `name`, whose name without its extension is `stem`; what the first
// comment of the file at `path` of the tree `root` says, written as a JSON string; the files of the tree that use it,
// the first CLI_PURPOSE_USERS of them by their paths, and how many there are in all, the tree's files read on the cores
// orior's work may take, written as `{"users", "all"}`; and both, written as `{"said", "users", "all"}`. Each of the
// last three 1, or 0 with why said (purpose.c)
int cli_purpose_uses(const char *text, unsigned long long size, const char *name, const char *stem);
int cli_purpose_said(const char *root, const char *path, CliLine *out, CliLine *said);
int cli_purpose_users(const char *root, const char *path, CliLine *out, CliLine *said);
int cli_purpose(const char *root, const char *path, CliLine *out, CliLine *said);

#endif

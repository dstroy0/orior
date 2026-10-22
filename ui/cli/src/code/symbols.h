// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols.h: what the files of a tree declare, for Go to Symbol in the tree and Search Everywhere: each line read by
// its own shape in its file's language, as the outline reads the open file (symbols_read.c). The index keeps each
// file's declarations with the time the file was last written and reads a file again only once that time moves. It is
// built on a thread of its own, and a search asked for while it is built answers from what is read so far and says so
// (symbols.c)
#ifndef SYMBOLS_H
#define SYMBOLS_H

#include "../command_line/cli_internal.h"

// one declaration found: its name, `length` bytes, what it is, and its line counted from 0
typedef void (*CliDeclared)(void *context, const char *name, unsigned long long length, const char *kind,
                            unsigned int line);

// whether the declarations of the file at `path` are read: whether its language is one read here
int cli_symbols_reads(const char *path);
// the declarations of `size` bytes of `text`, read in the language of the file at `path`, each handed to `each`
void cli_symbols_declared(const char *path, const char *text, unsigned long long size, CliDeclared each, void *context);
// How well `query` names `name`, higher the better, into `points`: the name itself first, then a name it begins, then
// one holding it, then one whose words it starts, as `gtf` starts `go_to_file` and `goToFile`, then one holding its
// letters in order. Case is set aside, and the shorter of two names that answer alike comes first. 1, or 0 where it
// does not name it
int cli_symbols_score(const char *query, const char *name, unsigned long long name_length, long long *points);
// every file forgotten, for a tree opened in place of the last
void cli_symbols_forget(void);
// the index brought up to date on a thread of its own, unless it is being brought up to date already or was within
// four seconds; `paths`, `count` of them, the tree's files as the file listing gives them
void cli_symbols_refresh(const char *root, const char *const *paths, unsigned int count);
// how far the first reading of the tree's files has gone, `done` of `total`: 1 while it goes, else 0
int cli_symbols_progress(unsigned long long *done, unsigned long long *total);
// the reading going ended, what it has read kept
void cli_symbols_stop(void);
// whether the tree's files are being read for the first time
int cli_symbols_unread(void);
// the declarations whose names answer `query`, at most `most` of them, the best first, written as `{"symbols":
// [{"name", "kind", "path", "line", "text"}...], "reading"}`, `text` the declaration's own line trimmed, read from
// its file as the answer is written
void cli_symbols_find(const char *query, unsigned int most, CliLine *out);

#endif

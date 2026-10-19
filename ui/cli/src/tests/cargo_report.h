// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cargo_report.h: what cargo test and the test harness write, read: a result's line, each failure's message and the
// place it panicked, a build's first error and the place it names, and the folder a package's paths are written from
// (cargo_report.c)
#ifndef CARGO_REPORT_H
#define CARGO_REPORT_H

#include "../command_line/cli_internal.h"

// a line of the harness's own, `test name ... ok`: the test's name as its start and size in the line, and `passed`,
// `failed` or `skipped`: 1, or 0 where the line is no result
int cli_cargo_result_line(const char *line, unsigned long long size, unsigned long long *name,
                          unsigned long long *name_size, const char **outcome);

// each failed test's message and the place it panicked, a path as the compiler wrote it, NULL for none, and a line
// counted from 1, from the harness's report of its failures, handed to `each`, a test failing twice told by its last
void cli_cargo_failures_each(const char *out, unsigned long long size,
                             void (*each)(void *context, const char *name, unsigned long long name_size,
                                          const char *message, unsigned long long message_size, const char *path,
                                          long long line),
                             void *context);

// a path and a line from `path:line:column` into `path`, which holds CLI_PATH_LONGEST, with forward slashes, and
// `line`: 1, or 0 where the text is none
int cli_cargo_place_of(const char *text, unsigned long long size, char *path, long long *line);

// the compiler's first error in what a build said into `error`, which holds `room`, its size into `length`, and the
// place it names into `path` and `line`, `path` empty where it names none: 1, or 0 where it said no error; and the last
// line of a text that is not blank, trimmed, or `none` where every line is, the same way
int cli_cargo_build_error(const char *said, unsigned long long size, char *error, unsigned long long room,
                          unsigned long long *length, char *path, long long *line);
void cli_cargo_last_line(const char *text, unsigned long long size, const char *none, char *out,
                         unsigned long long room, unsigned long long *length);

// the folder whose paths a package's panics and errors are written from into `workspace`, which holds
// CLI_PATH_LONGEST: the workspace's, where the package is one of a workspace in the tree, or the package's own
void cli_cargo_workspace_of(const char *root, const char *package, char *workspace);

#endif

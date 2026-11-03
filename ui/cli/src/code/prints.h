// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// prints.h: the print statements a change added: each line the tree's change against its last commit puts in, a file
// new to git whole, that prints in its file's language as a print for a task does, such as Python's print(),
// JavaScript's console.log(), Rust's dbg! and println!, C's printf, Go's fmt.Println, Java's System.out.println and
// Ruby's puts. Each is named by its file, its line in the file as it stands, and its text, for the reader to see before
// a commit and take out in one step (prints.c)
#ifndef PRINTS_H
#define PRINTS_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliPrintsOutCfg CliPrintsOutCfg;
typedef struct CliPrintsCfg CliPrintsCfg;
typedef struct CliPrintsPathCfg CliPrintsPathCfg;

// whether `length` bytes of `text`, a line of the file at `path`, print as a print for a task does; and the prints the
// change of the tree `root` against its last commit added, by path and then line, each once, written as
// `[{"path", "line", "text"}]`, the line counted from 0
int cli_prints_line(const CliPrintsPathCfg *args);
struct CliPrintsOutCfg
{
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const char *diff;
    unsigned long long size;
};

struct CliPrintsCfg
{
    const char *root;
    VerbaLinea *out;
    VerbaLinea *err_msg;
};

struct CliPrintsPathCfg
{
    const char *path;
    const char *text;
    unsigned long long length;
};

typedef struct
{
    int (*line)(const CliPrintsPathCfg *args);
} CliPrintsPathNs;
EMBED_TABLE_LAYOUT(CliPrintsPathNs, line);
EMBED_TABLE_STORAGE CliPrintsPathNs cli_prints_path EMBED_UNUSED = {
    .line = cli_prints_line,
};

#endif

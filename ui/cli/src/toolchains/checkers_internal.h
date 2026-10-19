// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// checkers_internal.h: what the checkers sources share: the findings read from what a checker wrote, by file
#ifndef CHECKERS_INTERNAL_H
#define CHECKERS_INTERNAL_H

#include "checkers.h"

// the most files and findings one check reads, and the bytes of their diagnostics
#define CLI_CHECK_FILES 1024u
#define CLI_CHECK_FINDINGS 65536u
#define CLI_CHECK_TEXT (1u << 24u)

// The findings of one check: the files they name, each by its path and its key, its path with forward slashes in
// lower case; and each finding's file and its diagnostic, written as JSON, in the texts
typedef struct
{
    char path[CLI_CHECK_FILES][CLI_PATH_LONGEST];
    char key[CLI_CHECK_FILES][CLI_PATH_LONGEST];
    unsigned int files;
    unsigned int file_of[CLI_CHECK_FINDINGS];
    unsigned int start[CLI_CHECK_FINDINGS];
    unsigned int length[CLI_CHECK_FINDINGS];
    unsigned int findings;
    char text[CLI_CHECK_TEXT];
    unsigned int used;
} CliFindings;

// the findings a checker named `name` that reads `reads` wrote, `out_size` bytes of output and `err_size` of errors,
// read into `findings` by file: `path`, where it is given, stands for a file named `-`, `<stdin>` or `stdin`, and a
// relative path is read from `root`; and the file at `path` among the findings: its place, put first where it is not
// there and `first` is set (checkers_read.c)
void cli_checkers_read(const char *name, const char *reads, const char *out, unsigned long long out_size,
                       const char *err, unsigned long long err_size, const char *root, const char *path,
                       CliFindings *findings);
unsigned int cli_checkers_file(CliFindings *findings, const char *path, int first);

// A finding as the protocol's diagnostic: where it starts, counted from one, where it ends where it is given, its
// severity, its message and code, each as bytes that may not be UTF-8, and its fixes as JSON
typedef struct
{
    unsigned long long line;
    unsigned long long col;
    int has_end;
    unsigned long long end_line;
    unsigned long long end_col;
    unsigned int severity;
    const char *message;
    unsigned long long message_length;
    const char *code;
    unsigned long long code_length;
    const char *fixes;
} CliFinding;

// a finding put among the findings of the file `file`; the file a finding names, `length` bytes of `name`: the one
// checked where it names `-`, `<stdin>` or `stdin` and one was, else the name read from the tree's top where it is
// relative, its place among the findings (checkers_read.c); and the findings `size` bytes of `text` write a line
// each, put among the findings (checkers_lines.c)
void cli_checkers_finding_put(unsigned int file, const CliFinding *finding);
unsigned int cli_checkers_file_of(const char *name, unsigned long long length);
void cli_checkers_lines_read(const char *text, unsigned long long size);

#endif

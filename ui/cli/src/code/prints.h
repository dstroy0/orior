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

// whether `length` bytes of `text`, a line of the file at `path`, print as a print for a task does; and the prints the
// change of the tree `root` against its last commit added, by path and then line, each once, written as
// `[{"path", "line", "text"}]`, the line counted from 0
int cli_prints_line(const char *path, const char *text, unsigned long long length);
int cli_prints_added(const char *root, CliLine *out, CliLine *said);
// the prints the lines `size` bytes of a unified diff with no lines of context put in, each by its path, its line in
// the file as it stands and its text, written as cli_prints_added writes them; each 1, or 0 with why said where no room
// for the prints is left
int cli_prints_put_in(const char *diff, unsigned long long size, CliLine *out, CliLine *said);

#endif

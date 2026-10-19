// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// checkers.h: checkers: the type checkers and linters the reader names, each a toolchain of the manifest with a
// `checker` that says how it runs and how its findings read. A checker is found as any toolchain is, and needs nothing
// set up past its name.
//
// A checker that reads a file's text on its input checks the text as the editor holds it, and one that reads files
// checks the file as the disk holds it. One that reads a tree whole is run over the tree from its top folder. Each
// finding is a diagnostic of the protocol's form, its source the checker's name, and a fix the checker gives, as ruff
// does, a fix the editor offers.
//
// Findings read in one of four forms: `lines`, a finding a line as `path:line:col: message`, the column, a severity
// word after it, and a code before the message or in brackets after it where the checker writes one, as flake8, mypy
// and pylint write them; and the JSON of ruff, ESLint and ShellCheck (checkers.c, checkers_read.c)
#ifndef CHECKERS_H
#define CHECKERS_H

#include "../command_line/cli_internal.h"

// the checker a name names, by its toolchain's id or its name in any case, its id into `id`, which holds
// CLI_NAME_LONGEST: 1, or 0 where none does; whether the checker `id` checks a tree whole, whether it reads a file's
// text on its input, and whether it checks `language`; and the languages of every checker, written as a JSON object of
// each checker's id and its languages
int cli_checkers_named(const char *name, char *id);
int cli_checkers_reads_tree(const char *id);
int cli_checkers_reads_input(const char *id);
int cli_checkers_checks(const char *id, const char *language);
void cli_checkers_languages(CliLine *out);

// the file at `path` of the tree at `root` checked by the checker `id`, as `length` bytes of `text` where the checker
// reads its input and `text` is given, and as the disk holds it otherwise; and the tree at `root` checked whole, where
// the checker reads a tree. Each writes the findings by file, the file's own first where it checks a file, as
// `[[path, [diagnostic]]]`: 1, or 0 with why said. A checker runs no tree whose own code is held back
int cli_checkers_check_file(const char *id, const char *root, const char *path, const char *text,
                            unsigned long long length, CliLine *out, CliLine *said);
int cli_checkers_check_tree(const char *id, const char *root, CliLine *out, CliLine *said);

#endif

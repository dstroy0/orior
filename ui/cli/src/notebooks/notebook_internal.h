// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// notebook_internal.h: what notebook.c and notebook_merge.c share
#ifndef NOTEBOOK_INTERNAL_H
#define NOTEBOOK_INTERNAL_H

#include "notebook.h"

// a cell's source or an output's text as one text, from a list of lines or a text: a string node made of it
unsigned int cli_notebook_joined(CliJson *json, unsigned int value);
// a cell's source, the empty text where it has none, its length into `length`
const char *cli_notebook_source(const CliJson *json, unsigned int cell, unsigned long long *length);
// whether `text` ends with `ending`
int cli_notebook_ends(const char *text, unsigned long long length, const char *ending);
// the comment a script's cells are marked with in the language of `path`, by its extension
const char *cli_notebook_comment(const char *path);
// the line of a text that starts at `at` before `end`: where it ends, its `\r` before `\n` left off, into `stop`;
// where the next starts
const char *cli_notebook_line(const char *at, const char *end, const char **stop);
// a text as Jupyter keeps it: an array node of its lines, each with its line end but the last
unsigned int cli_notebook_lines_of(CliJson *json, const char *text, unsigned long long length);

#endif

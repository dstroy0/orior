// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// review.h: a change reviewed by its structure: the declarations it touches, across its files, and not file by file. A
// change is the tree's against its last commit, or a commit's against its first parent, with who made the commit.
//
// Each declaration a change adds, takes out or changes is a piece of the review, its text as it was and as it is, a
// declaration's text running from its line to the line before the next declaration, its blank lines at the end left
// off. One taken out of a place and put in at another, in the same file or another, by its name and its kind and with
// the most of its lines kept, is one piece that moved. The pieces that call one another stand together, each caller
// before what it calls. A file whose language is not read here, or whose lines outside its declarations changed, is
// named apart (review.c, review_compare.c, review_text.c)
#ifndef REVIEW_H
#define REVIEW_H

#include "../command_line/cli_internal.h"

// the change of the tree `root` against its last commit, or of commit `commit` against its first parent where it is
// given, reviewed by its structure, written as `{"pieces": [{"path", "name", "kind", "how", "from", "line", "was",
// "now", "calls"}], "groups", "other", "by", "subject"}`: 1, or 0 with why said
int cli_review(const char *root, const char *commit, CliLine *out, CliLine *said);
// the change of `count` files given whole reviewed by its structure: each at `paths`, its text as it was and as it is,
// NULL where it has none, written as cli_review writes one, with no one who made it
void cli_review_files(const char *const *paths, const char *const *was, const char *const *now, unsigned int count,
                      CliLine *out);

#endif

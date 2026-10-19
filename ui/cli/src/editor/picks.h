// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// picks.h: the lists a quick open of the window's shows that are not the tree's files or symbols, as the tree's tests
// and the machine's processes: each held here by a number as it opens, and matched as each letter is typed, the best
// first, as files_fuzzy.c matches the tree's files. A list of tens of thousands answers a letter in a few
// milliseconds, spread over the cores (picks.c)
#ifndef PICKS_H
#define PICKS_H

#include "../command_line/cli_internal.h"

// the most lists held at once, the most rows of them all, and the most bytes of their texts
#define CLI_PICKS_LISTS 64u
#define CLI_PICKS_ROWS (1u << 20u)
#define CLI_PICKS_TEXT (1u << 25u)

// the rows of `length` bytes of `texts`, one to a line, held: the number they are held by, from 1, or 0 where they
// do not fit beside the lists held; the list held by `id` let go; and the rows of the list held by `id` that answer
// `query`, at most `most` of them, the best first and rows that score alike in the list's order, with no query its
// first `most` rows, written as `[{"at", "score", "hits"}]`: 1, or 0 with why said where no list is held by `id`
unsigned long long cli_picks_hold(const char *texts, unsigned long long length);
void cli_picks_forget(unsigned long long id);
int cli_picks_find(unsigned long long id, const char *query, unsigned long long most, CliLine *out, CliLine *said);

#endif

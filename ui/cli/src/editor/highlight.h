// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// highlight.h: the editor's colors, worked out here: a text a line at a time from a grammar, as the page's tokens.js
// reads one (highlight.c reads and stores a grammar, highlight_line.c colors lines by it). A grammar is the page's own,
// handed over as JSON with each pattern as its source and flags, and is stored under a key for the lines requested of
// it after.
//
// A grammar is `{ tokenizer: { root: rules, <state>: rules, ... }, <name>: [words], ... }`. Each rule is tried in
// order at each place in a line, and the first whose pattern matches there takes the match: `[pattern, token]`,
// `[pattern, token, next]` or `[pattern, { token, next }]`, after which the state changes; `[pattern, [token, ...]]`,
// each group of the match a token of its own; or `{ include: "@state" }`, the rules of another state. A token is a
// name, or `{ cases: { "@<name>": token, "<word>": token, "@default": token } }`, which picks by the matched text, and
// `@brackets` is a bracket's. A next of `@<state>` enters a state, `@pop` leaves the one entered last, and `@popall`
// returns to root. A grammar whose `perLine` is set starts every line in root; any other carries its states from the
// end of one line to the start of the next, written as their names joined by `/`
#ifndef HIGHLIGHT_H
#define HIGHLIGHT_H

#include "../text/json_tree.h"

// the grammar at `def` in `json` read and stored under `key`, in the place of one stored there before: 1, or 0 with
// what in it is not read here reported
int cli_highlight_store(const char *key, const CliJson *json, unsigned int def, VerbaLinea *err_msg);
// `count` lines colored with the grammar stored under `key`, the first starting in `state` and each after in the state
// the one before it ends in, written into `out` as `{"names": [every name a token can take, the empty name first],
// "runs": [[[column, name], ...] for each line], "states": [the state each line ends in]}`: 1, or 0 with why given
int cli_highlight_color(const char *key, const char *state, const char *const *lines, unsigned int count,
                        VerbaLinea *out, VerbaLinea *err_msg);
// One line's leaves as the colorer hands them over: the line, and its runs, each the column it starts at in UTF-16
// units and its token's name, running to the next run's column or the line's end; a run's name is `text` from
// `name_at[name[run]]`, the empty name where the grammar gives none
typedef struct
{
    const char *line;
    unsigned int runs;
    const unsigned int *col;
    const unsigned int *name;
    const unsigned int *name_at;
    const char *text;
} CliLeaves;

// what each line's leaves are handed to, in turn
typedef void (*CliLeavesEach)(void *context, const CliLeaves *leaves);

// `count` lines colored with the grammar stored under `key`, the first starting in `state` and each after in the state
// the one before it ends in, each line's leaves handed to `each`, and the state the last ends in written into `ended`:
// 1, or 0 with why given. `each` runs under the lock the grammars are stored under, and colors and stores nothing
int cli_highlight_leaves(const char *key, const char *state, const char *const *lines, unsigned int count,
                         CliLeavesEach each, void *context, VerbaLinea *ended, VerbaLinea *err_msg);
// the grammars' own report, `<pool's bytes> <their mark> 0`, the most the grammars and their rooms occupied of the
// cellblock at once, written into `out` (highlight_store.c)
void cli_highlight_cells_usage_written(VerbaLinea *out);

#endif

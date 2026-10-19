// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// highlight.h: the editor's colors, worked out here: a text a line at a time from a grammar, as the page's tokens.js
// reads one (highlight.c reads and keeps a grammar, highlight_line.c colors lines by it). A grammar is the page's own,
// handed over as JSON with each pattern as its source and flags, and is kept under a key for the lines asked of it
// after.
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

// the grammar at `def` in `json` read and kept under `key`, in the place of one kept there before: 1, or 0 with what
// in it is not read here said
int cli_highlight_keep(const char *key, const CliJson *json, unsigned int def, CliLine *said);
// `count` lines colored with the grammar kept under `key`, the first starting in `state` and each after in the state
// the one before it ends in, written into `out` as `{"names": [every name a token can take, the empty name first],
// "runs": [[[column, name], ...] for each line], "states": [the state each line ends in]}`: 1, or 0 with why said
int cli_highlight_color(const char *key, const char *state, const char *const *lines, unsigned int count, CliLine *out,
                        CliLine *said);
// the grammars' own report, `<pool's bytes> <their mark> 0`, the most the grammars and their rooms held of the
// cellblock at once, written into `out` (highlight_keep.c)
void cli_highlight_cells_said(CliLine *out);

#endif

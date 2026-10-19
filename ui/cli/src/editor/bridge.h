// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// bridge.h: the bridge between languages as the editor reads it (bridge.c): Lstar.klq, each language's .klm, and every
// ruleset, all in src/cu/transpiler/lstar/protocol/table.
//
// A language's name goes through its .klm to a key of the bridge, the key's pairs and their verdicts are in Lstar.klq,
// and each ruleset's entry of the key's name is the form it writes. The forms of each line are in
// src/cu/includes/file_defs/klq and klm.
//
// Only the line shapes are held to: `key`, `pair`, the verdict under a pair (`open` or `closed`), and a .klm's `key`,
// `kind` and `breaks`. Every other line under a pair is kept as a note, its first word and the rest, and a line the
// query adds later shows without a change here. A .klm's name and its key are read as two words even where they are
// equal
#ifndef BRIDGE_H
#define BRIDGE_H

#include "../text/json_tree.h"

#define CLI_BRIDGE_DIR "src/cu/transpiler/lstar/protocol/table"

// the bridge of the tree at `root` read into `json`, in a cell of its own sized to the bridge's files, which
// cli_json_given gives back, as the window and the command line read it: its node,
// `{"dir", "klq": {"path", "keys": {name: {"line", "pairs": [{"first", "second", "line", "verdict": note or null,
// "notes": [note...]}]}}, "identities": [note...]} or null, "maps": [{"path", "language", "names": {name: {"key",
// "line", "kind": note or null, "breaks": [note...]}}}], "rulesets": [{"path", "entries": {name: [{"kind",
// "line"}]}}]}`, a note `{"word", "rest", "line"}` and each map's names in order; CLI_JSON_NONE where the tree is full
unsigned int cli_bridge_read(CliJson *json, const char *root);
// a Lstar.klq's, a .klm's and a ruleset's text, at `path` in the tree, read into `json`: its node
unsigned int cli_bridge_klq(CliJson *json, const char *path, const char *text, unsigned long long size);
unsigned int cli_bridge_klm(CliJson *json, const char *path, const char *text, unsigned long long size);
unsigned int cli_bridge_ruleset(CliJson *json, const char *path, const char *text, unsigned long long size);

#endif

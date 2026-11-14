// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// bridge.h: the bridge between languages as the editor reads it (bridge.c): Lstar.klq, each language's .klm, and every
// ruleset, all in src/cu/transpiler/lstar/protocol/table.
//
// A language's name goes through its .klm to a key of the bridge, the key's pairs and their verdicts are in Lstar.klq,
// and each ruleset's entry of the key's name is the form it writes. The forms of each line are in
// includes/file_defs/klq and klm.
//
// Only the line shapes are checked: `key`, `pair`, the verdict under a pair (`open` or `closed`), and a .klm's `key`,
// `type` and `breaks`. Every other line under a pair is retained as a note, its first word and the rest, and a line the
// query adds later shows without a change here. A .klm's name and its key are read as two words even where they are
// equal
#ifndef BRIDGE_H
#define BRIDGE_H
#include "../text/json_tree.h"
#include "embed_dispatch_layout.h"

typedef struct CliBridgePathCfg CliBridgePathCfg;
typedef struct CliBridgeCfg CliBridgeCfg;

#define CLI_BRIDGE_DIR "src/cu/transpiler/lstar/protocol/table"

// the bridge of the tree at `root` read into `json`, in a cell of its own sized to the bridge's files, which
// persistent_buf_release releases, as the window and the command line read it: its node,
// `{"dir", "klq": {"path", "keys": {name: {"line", "pairs": [{"first", "second", "line", "verdict": note or null,
// "notes": [note...]}]}}, "identities": [note...]} or null, "maps": [{"path", "language", "names": {name: {"key",
// "line", "type": note or null, "breaks": [note...]}}}], "rulesets": [{"path", "entries": {name: [{"type",
// "line"}]}}]}`, a note `{"word", "rest", "line"}` and each map's names in order; CLI_JSON_NONE where the tree is full
unsigned int cli_bridge_read(const CliBridgeCfg *args);
// a Lstar.klq's, a .klm's and a ruleset's text, at `path` in the tree, read into `json`: its node
unsigned int cli_bridge_klq(const CliBridgePathCfg *args);
unsigned int cli_bridge_klm(const CliBridgePathCfg *args);
unsigned int cli_bridge_ruleset(const CliBridgePathCfg *args);

struct CliBridgePathCfg
{
    CliJson *json;
    const char *path;
    const char *text;
    unsigned long long size;
};

struct CliBridgeCfg
{
    CliJson *json;
    const char *root;
};

typedef struct
{
    unsigned int (*read)(const CliBridgeCfg *args);
} CliBridgeNs;
EMBED_TABLE_LAYOUT(CliBridgeNs, read);
EMBED_TABLE_STORAGE CliBridgeNs cli_bridge EMBED_UNUSED = {
    .read = cli_bridge_read,
};

typedef struct
{
    unsigned int (*klq)(const CliBridgePathCfg *args);
    unsigned int (*klm)(const CliBridgePathCfg *args);
    unsigned int (*ruleset)(const CliBridgePathCfg *args);
} CliBridgePathNs;
EMBED_TABLE_LAYOUT(CliBridgePathNs, klq, klm, ruleset);
EMBED_TABLE_STORAGE CliBridgePathNs cli_bridge_path EMBED_UNUSED = {
    .klq = cli_bridge_klq,
    .klm = cli_bridge_klm,
    .ruleset = cli_bridge_ruleset,
};

#endif

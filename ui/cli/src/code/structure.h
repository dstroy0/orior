// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// structure.h: compare by structure (structure.c, structure_compare.c): two texts set side by side by their tokens,
// the names, numbers, strings and signs of the code, and not by their characters. White space and line breaks between
// tokens are layout, and a line whose tokens did not change is reshaped and not changed.
//
// The lines are compared first, each as the tokens it contains. A line taken out in one place and put in at another
// with the same tokens, two of them at least, is moved. What is left of each change is compared token by token: a name
// changed to one other name everywhere it changed is renamed, and every other token is taken out or put in. Past three
// thousand edits, at either step, the two texts read as too far apart
#ifndef STRUCTURE_H
#define STRUCTURE_H
typedef struct CliStructureCfg CliStructureCfg;

#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

// What a line, or a token of it, is: the same in both texts, changed, moved, reshaped, or changed only by names renamed
enum
{
    CLI_SAME,
    CLI_CHANGED,
    CLI_MOVED,
    CLI_RESHAPED,
    CLI_RENAMED
};

// `then`, the older text, compared with `now` by structure, written into `out` as the window reads it:
// `{"rows": [[left line or null, right line or null]...], "starts": [the row each change starts at], "left": [each
// line's kind], "right": [...], "left_marks": [[line, from, to, kind]...] for each token that is not the same,
// "right_marks": [...], "renames": [[was, became, places]...], "moved": lines, "reshaped": lines}`, or `null` where
// they are too far apart
void cli_structure_compare(const CliStructureCfg *args);

struct CliStructureCfg
{
    const char *then;
    unsigned long long then_size;
    const char *now;
    unsigned long long now_size;
    VerbaLinea *out;
};

typedef struct
{
    void (*compare)(const CliStructureCfg *args);
} CliStructureNs;
EMBED_TABLE_LAYOUT(CliStructureNs, compare);
EMBED_TABLE_STORAGE CliStructureNs cli_structure EMBED_UNUSED = {
    .compare = cli_structure_compare,
};

#endif

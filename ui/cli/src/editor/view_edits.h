// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// view_edits.h: edits the CLI answers, a change's revert among them, written into a view as one step, which one undo
// takes back whole and one redo brings again (view.c)
#ifndef VIEW_EDITS_H
#define VIEW_EDITS_H
#include "view.h"

typedef struct CliViewEditsCfg CliViewEditsCfg;

// the `count` edits at `edit`, in the order of their places in the text and none overlapping, each placed against the
// text as it stood before any, written into the view under `key` from the last to the first as one step, the cursor put
// at the first edit's start: 1, or 0 with why written into `err_msg`, the edits written before the one refused standing
// in the step
int cli_view_edits_apply(const CliViewEditsCfg *args);
struct CliViewEditsCfg
{
    const char *key;
    const CliEdit *edit;
    unsigned int count;
    VerbaLinea *err_msg;
};

typedef struct
{
    int (*apply)(const CliViewEditsCfg *args);
} CliViewEditsNs;
EMBED_TABLE_LAYOUT(CliViewEditsNs, apply);
EMBED_TABLE_STORAGE CliViewEditsNs cli_view_edits EMBED_UNUSED = {
    .apply = cli_view_edits_apply,
};

#endif

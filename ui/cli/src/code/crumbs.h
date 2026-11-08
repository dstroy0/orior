// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// crumbs.h: the crumbs of the file shown, as the status bar lays them: each folder of its path from the tree's top, the
// file, and each declaration of the outline the cursor's line stands inside, outermost first. The rows stand as
// indices into one cell with their texts after them, read when the file shown is another and again when the cursor's
// line leaves the lines the innermost declaration's crumb answers for; the outline's rows stand in the order of their
// lines, so the declaration a line stands in is found by halving. What stands beside a crumb is the menus' own rows:
// a folder's entries, or the declarations at a declaration's depth under the one it stands in. The setting, on until
// set off, is saved under CLI_CRUMBS_KEY in orior's folder (crumbs.c)
#ifndef CRUMBS_H
#define CRUMBS_H
#include "../command_line/cli_internal.h"
#include "../editor/document.h"
#include "../machine/state.h"
#include "embed_dispatch_layout.h"

typedef struct CliCrumbsCfg CliCrumbsCfg;

// a crumb's type: a folder, the file, a declaration; the most crumbs a file shows; and the key the setting is saved
// under
#define CLI_CRUMB_FOLDER 0u
#define CLI_CRUMB_FILE 1u
#define CLI_CRUMB_SYMBOL 2u
#define CLI_CRUMBS_MOST 64u
#define CLI_CRUMBS_KEY "breadcrumbs"

// A crumb: its type, its text, `length` bytes, standing until the crumbs are read again, and how deep it stands, a
// folder's place in the path or a declaration's depth in the outline
typedef struct
{
    unsigned int type;
    const char *text;
    unsigned long long length;
    unsigned int depth;
} CliCrumbRow;

// the setting restored from `entries`, on where none is saved
void cli_crumbs_begin(const CliCrumbsCfg *args);
// the crumbs of the file at `path` shown under `key`, the cursor at the line `line`: the folders and the file always,
// and the declarations of the outline where `key` is not NULL; none where `path` is NULL or the setting is off
void cli_crumbs_read(const CliCrumbsCfg *args);
// how many crumbs stand; and the crumb `at` into `row`: 1, or 0 past the last
unsigned int cli_crumbs_rows(const CliCrumbsCfg *args);
int cli_crumbs_row(const CliCrumbsCfg *args);
// what stands beside the crumb `at` built as the menus' own rows, their texts in a cell of the crumbs' own standing
// until the next menu: a folder's entries and the file's folder's in the tree at `root`, each running file-open with
// its path, and the declarations beside a declaration, each running symbol-go with its line from 0: 1, or 0 where no
// row stands
int cli_crumbs_menu(const CliCrumbsCfg *args);
// the place symbol-go's word `text` reads as, a line, into `place_out`: 1, or 0 where the word is no line
int cli_crumbs_go(const CliCrumbsCfg *args);
// whether the crumbs show; and the setting put `on` and saved in orior's folder, the crumbs read again as it turns on
int cli_crumbs_shown(const CliCrumbsCfg *args);
void cli_crumbs_shown_set(const CliCrumbsCfg *args);
struct CliCrumbsCfg
{
    const char *key;
    const char *path;
    const char *root;
    unsigned long long line;
    unsigned int at;
    CliCrumbRow *row;
    int on;
    const char *text;
    CliPosition *place_out;
    const CliStateEntries *entries;
};

typedef struct
{
    void (*begin)(const CliCrumbsCfg *args);
    void (*read)(const CliCrumbsCfg *args);
    unsigned int (*rows)(const CliCrumbsCfg *args);
    int (*row)(const CliCrumbsCfg *args);
    int (*menu)(const CliCrumbsCfg *args);
    int (*go)(const CliCrumbsCfg *args);
    int (*shown)(const CliCrumbsCfg *args);
    void (*shown_set)(const CliCrumbsCfg *args);
} CliCrumbsNs;
EMBED_TABLE_LAYOUT(CliCrumbsNs, begin, read, rows, row, menu, go, shown, shown_set);
EMBED_TABLE_STORAGE CliCrumbsNs cli_crumbs EMBED_UNUSED = {
    .begin = cli_crumbs_begin,
    .read = cli_crumbs_read,
    .rows = cli_crumbs_rows,
    .row = cli_crumbs_row,
    .menu = cli_crumbs_menu,
    .go = cli_crumbs_go,
    .shown = cli_crumbs_shown,
    .shown_set = cli_crumbs_shown_set,
};

#endif

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// quick.h: the quick open along the top of the window, which reads what is typed by its first letter: a file of the
// tree by default, the files opened last first, and `path:line:column` going to that place; `>` for the menus'
// commands, the ones run last first; `:` for a line of the open file, as `:line:column`; `@` for what the open file
// declares; and `#` for what any file of the tree declares. Search Everywhere reads what is typed as all of these at
// once, CLI_QUICK_EACH of each under its heading: the files, what the tree declares, and the commands. The typed text
// with its caret, the rows, the row chosen and the rows scrolled off the top stand here, the rows in one cell sized to
// them, each a set of offsets into its text and its marks; the window draws them and hands the input on (quick.c)
#ifndef QUICK_H
#define QUICK_H
#include "../command_line/commands.h"
#include "document.h"
#include "embed_dispatch_layout.h"

typedef struct CliQuickCfg CliQuickCfg;

// what the quick open reads: files, commands, a line, what the open file declares, what the tree declares, and all at
// once for Search Everywhere
#define CLI_QUICK_FILES 0u
#define CLI_QUICK_COMMANDS 1u
#define CLI_QUICK_LINE 2u
#define CLI_QUICK_SYMBOLS 3u
#define CLI_QUICK_TREE 4u
#define CLI_QUICK_EVERYWHERE 5u

// the types of row: a file, a command, a line, a declaration, a heading the keys step past, a sentence that goes
// nowhere, and the last row while the tree is read, which has the rows asked again
#define CLI_QUICK_ROW_FILE 0u
#define CLI_QUICK_ROW_COMMAND 1u
#define CLI_QUICK_ROW_LINE 2u
#define CLI_QUICK_ROW_SYMBOL 3u
#define CLI_QUICK_ROW_HEADING 4u
#define CLI_QUICK_ROW_TEXT 5u
#define CLI_QUICK_ROW_READING 6u

// what a choice opens: nothing, a file at a place, or a command
#define CLI_QUICK_CHOSE_NONE 0u
#define CLI_QUICK_CHOSE_PATH 1u
#define CLI_QUICK_CHOSE_COMMAND 2u

// the most rows, the rows of each type Search Everywhere shows, the milliseconds between the rows asked again while
// the tree is read, the milliseconds two presses of Shift come within to open Search Everywhere, the most bytes typed,
// the most marks of one row's label and detail together, and the row `at` that stands for the row chosen
#define CLI_QUICK_MOST 200u
#define CLI_QUICK_EACH 8u
#define CLI_QUICK_AGAIN_MS 250ULL
#define CLI_QUICK_SHIFT_MS 400ULL
#define CLI_QUICK_TEXT_MAX 512u
#define CLI_QUICK_MARKS 32u
#define CLI_QUICK_CHOSEN 0xFFFFFFFFu

// A row as the window draws it, copied out of the rows' cell: its type; its label, its detail drawn quieter after it
// and the keys of a command at the right end, each ended, with its bytes; and the marks of the letters that matched,
// pairs of a byte offset and bytes, `label_mark_count` pairs into the label and `detail_mark_count` into the detail
typedef struct
{
    unsigned int type;
    char label[CLI_PATH_MAX_LEN];
    unsigned long long label_length;
    char detail[CLI_PATH_MAX_LEN];
    unsigned long long detail_length;
    char keys[CLI_LABEL_MAX_LEN];
    unsigned long long keys_length;
    unsigned int label_marks[2u * CLI_QUICK_MARKS];
    unsigned int label_mark_count;
    unsigned int detail_marks[2u * CLI_QUICK_MARKS];
    unsigned int detail_mark_count;
} CliQuickRow;

// What the box shows: whether it is open; what it reads; the typed text with its bytes and the caret's byte; the
// sentence the field shows while the text is empty, with its bytes; how many rows; the row chosen; and the rows
// scrolled off the top, the chosen in sight of the `shown` rows the box shows
typedef struct
{
    int open;
    unsigned int mode;
    const char *text;
    unsigned long long length;
    unsigned long long caret;
    const char *placeholder;
    unsigned long long placeholder_length;
    unsigned int rows;
    unsigned int chosen;
    unsigned int scroll;
} CliQuickView;

// What a choice opens: its type; a file's path from the tree's top and the place in it where `placed`; or a command's
// word and its keys; each text standing until the next choice
typedef struct
{
    unsigned int type;
    const char *path;
    CliPosition place;
    int placed;
    const char *command;
    const char *keys;
} CliQuickChoice;

// the quick open opened by the command `command`, palette, file, symbol, symbol-tree, line or search-everywhere, for
// the tree at `root` and the view shown under `key`, NULL for none, with `length` bytes of `text` typed after what the
// command reads, `shown` rows in the box, at `now_ms`: 1, or 0 where the command is none of these. `length` bytes of
// `text` typed at the caret, a control character and what follows it left out, at `now_ms`. The key `name`: while
// open Backspace, Delete, Left, Right, Home, End and Mod+V edit the text and move the caret, and while closed the keys
// of one of the commands above open it as that command does: 1 where the key was taken
int cli_quick_open(const CliQuickCfg *args);
void cli_quick_type(const CliQuickCfg *args);
int cli_quick_key(const CliQuickCfg *args);
// the chosen row moved `by` rows, past a heading the way it moves, back where nothing stands past it, and the rows
// scrolled `scroll_by` rows, the chosen standing in sight of the `shown` rows; the row `at` into `row_out`: 1, or 0
// past the rows; and what the box shows into `view_out`
void cli_quick_step(const CliQuickCfg *args);
int cli_quick_row(const CliQuickCfg *args);
void cli_quick_view(const CliQuickCfg *args);
// the row `at`, or the chosen where `at` is CLI_QUICK_CHOSEN, chosen: what it opens into `choice_out`, the file or the
// command put first among the recent, the box closed: 1, or 0 where the row opens nothing, the box open still
int cli_quick_choose(const CliQuickCfg *args);
// at `now_ms`, the rows asked again where the tree was read when they were asked and CLI_QUICK_AGAIN_MS passed, the
// chosen row standing chosen where it stands still: 1 where they were; and when they are next asked, 0 for never
int cli_quick_again(const CliQuickCfg *args);
unsigned long long cli_quick_deadline_ms(const CliQuickCfg *args);
// the key `name` at `now_ms` for the double Shift: Shift a second time within CLI_QUICK_SHIFT_MS of the first, no other
// key between, opens Search Everywhere for `root` and `key` with `shown` rows: 1 where it opened; and the box closed,
// its cells released
int cli_quick_shift(const CliQuickCfg *args);
void cli_quick_close(const CliQuickCfg *args);
struct CliQuickCfg
{
    const char *command;
    const char *root;
    const char *key;
    const char *text;
    unsigned long long length;
    const char *name;
    unsigned int at;
    int by;
    int scroll_by;
    unsigned int shown;
    unsigned long long now_ms;
    CliQuickRow *row_out;
    CliQuickView *view_out;
    CliQuickChoice *choice_out;
};

typedef struct
{
    int (*open)(const CliQuickCfg *args);
    void (*type)(const CliQuickCfg *args);
    int (*key)(const CliQuickCfg *args);
    void (*step)(const CliQuickCfg *args);
    int (*row)(const CliQuickCfg *args);
    void (*view)(const CliQuickCfg *args);
    int (*choose)(const CliQuickCfg *args);
    int (*again)(const CliQuickCfg *args);
    unsigned long long (*deadline_ms)(const CliQuickCfg *args);
    int (*shift)(const CliQuickCfg *args);
    void (*close)(const CliQuickCfg *args);
} CliQuickNs;
EMBED_TABLE_LAYOUT(CliQuickNs, open, type, key, step, row, view, choose, again, deadline_ms, shift, close);
EMBED_TABLE_STORAGE CliQuickNs cli_quick EMBED_UNUSED = {
    .open = cli_quick_open,
    .type = cli_quick_type,
    .key = cli_quick_key,
    .step = cli_quick_step,
    .row = cli_quick_row,
    .view = cli_quick_view,
    .choose = cli_quick_choose,
    .again = cli_quick_again,
    .deadline_ms = cli_quick_deadline_ms,
    .shift = cli_quick_shift,
    .close = cli_quick_close,
};

#endif

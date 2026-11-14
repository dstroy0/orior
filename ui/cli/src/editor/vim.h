// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// vim.h: Vim's keys for the editor's views, where the setting vim turns them on. The view asks this of each key and
// each text typed first, and what this does not take goes to the view's own keys: in insert mode every key but Escape
// and Ctrl+[, in the other modes every key pressed with Ctrl that Vim has no use for. Normal mode takes a count before
// a command, and "x before it for the register x; the register " takes every yank and delete, and 0 the last yank.
// The motions: h j k l and the arrows, w b e and W B E, 0 ^ $, gg and G, f F t T to a letter on the line and ; , to go
// to it again, % to the bracket that pairs with the next on the line, { } to the empty line before or after, n N to
// the next or the last place searched for, and Ctrl+D Ctrl+U by half the view's rows. The operators d c y > < act on a
// motion, on a text object (iw aw iW aW, and i or a before one of " ' ` ( ) b [ ] { } B < >) or, doubled, on whole
// lines. Then x X D C Y s S J ~ r, p P, u and Ctrl+R, i a I A o O, v V, / ? * #, and ., which does the last change
// again with the text typed in insert mode after it. Visual mode, from v, and visual line mode, from V, move the
// selection's end by the motions, take a text object, swap the ends with o, and act on what is chosen with d c y > <
// ~ J p, x and s. Every edit is one of the view's own steps, so undo and the save point stay the view's, and every
// search goes through the view's find. The registers, the keys typed and every sentence stand in one text cell (vim.c,
// vim_parse.c, vim_motions.c, vim_objects.c, vim_commands.c, vim_visual.c; the view's steps in view.c)
#ifndef VIM_H
#define VIM_H
#include "../machine/state.h"
#include "document.h"
#include "embed_dispatch_layout.h"

typedef struct CliVimCfg CliVimCfg;
typedef struct CliViewVimCfg CliViewVimCfg;

// the modes: normal, insert, visual and visual line
#define CLI_VIM_NORMAL 0u
#define CLI_VIM_INSERT 1u
#define CLI_VIM_VISUAL 2u
#define CLI_VIM_LINE 3u

// What the window draws for Vim on a view: whether Vim's keys are on; the mode; whether the caret is a block a cell
// wide, every mode but insert; the label, the mode in capitals and after a space the keys typed of the command so
// far, `label_length` bytes; and whether the search prompt is open, whether it searches back, from ?, and its text,
// `prompt_length` bytes. The label and the prompt stand in the module's text cell until its next input
typedef struct
{
    int on;
    unsigned int mode;
    int block;
    const char *label;
    unsigned long long label_length;
    int prompt_open;
    int prompt_back;
    const char *prompt;
    unsigned long long prompt_length;
} CliVimShape;

// Vim begun, the setting vim read among `entries`, "true" for on, which every view takes that `set` has not turned;
// Vim's keys turned on or off for the view under `key`, the mode normal and the prompt closed either way, or for every
// view, the setting itself, where `key` is NULL; and the view under `key` closed, its record and its texts let go
void cli_vim_begin(const CliVimCfg *args);
void cli_vim_set(const CliVimCfg *args);
void cli_vim_close(const CliVimCfg *args);
// the key `name`, as the window names a key, offered to Vim for the view under `key`: 1 where Vim took it, 0 where it
// is the view's; and `length` bytes of `text` typed offered to Vim: 1 where Vim took them, into the prompt or as the
// keys of a command, 0 in insert mode, where the view types them and the change being taken takes them for `.`
int cli_vim_key(const CliVimCfg *args);
int cli_vim_text(const CliVimCfg *args);
// what the window draws for the view under `key`, the shape off where Vim's keys are off for it: the module's shape,
// read anew at each call, with no cell taken
const CliVimShape *cli_vim_shape(const CliVimCfg *args);
struct CliVimCfg
{
    const char *key;
    const char *name;
    const char *text;
    unsigned long long length;
    int on;
    const CliStateEntries *entries;
    VerbaLinea *err_msg;
};

typedef struct
{
    void (*begin)(const CliVimCfg *args);
    void (*set)(const CliVimCfg *args);
    void (*close)(const CliVimCfg *args);
    int (*key)(const CliVimCfg *args);
    int (*text)(const CliVimCfg *args);
    const CliVimShape *(*shape)(const CliVimCfg *args);
} CliVimNs;
EMBED_TABLE_LAYOUT(CliVimNs, begin, set, close, key, text, shape);
EMBED_TABLE_STORAGE CliVimNs cli_vim_view EMBED_UNUSED = {
    .begin = cli_vim_begin,
    .set = cli_vim_set,
    .close = cli_vim_close,
    .key = cli_vim_key,
    .text = cli_vim_text,
    .shape = cli_vim_shape,
};

// The view's own steps Vim acts through, defined beside the views (view.c), each on the view under `key`: how many
// lines its text has; the units of its line `line` copied into a temporary cell, their count into `count`, NULL past
// the last or where the cell could not be taken, and that copy given back as `units`; how many rows it shows; the bytes
// of UTF-8 the span from `from` to `to` takes, and that span's UTF-8 written into `out`; the span from `from` to `to`
// replaced by `length` bytes of `text`, a step of its own, the cursor after the text: 1, or 0 with why written into
// `err_msg`; the lines from `from.line` to `to.line` a step of indentation deeper where `deeper`, else a step
// shallower, a step of its own: 1, or 0 with why; the cursor put at `cursor` and the anchor at `anchor`, the view's top
// following; the last step undone, or the next redone where `redo`: 1, or 0 with why; and the next place from `from`
// where the view's find finds `length` bytes of `text`, as a pattern and where that reads as none as the text itself,
// or with `word_text` as a whole word, before it going back where `back`, round the text's ends: 1 with the place into
// `place`, or 0 where it finds none. Each answers 0, or nothing, where no view is open under the key
unsigned long long cli_view_vim_lines(const CliViewVimCfg *args);
const unsigned short *cli_view_vim_line(const CliViewVimCfg *args);
void cli_view_vim_line_end(const CliViewVimCfg *args);
unsigned int cli_view_vim_rows(const CliViewVimCfg *args);
unsigned long long cli_view_vim_span_bytes(const CliViewVimCfg *args);
void cli_view_vim_span(const CliViewVimCfg *args);
int cli_view_vim_edit(const CliViewVimCfg *args);
int cli_view_vim_indent(const CliViewVimCfg *args);
void cli_view_vim_place(const CliViewVimCfg *args);
int cli_view_vim_step(const CliViewVimCfg *args);
int cli_view_vim_find(const CliViewVimCfg *args);
struct CliViewVimCfg
{
    const char *key;
    unsigned long long line;
    unsigned long long *count;
    CliPosition from;
    CliPosition to;
    const char *text;
    unsigned long long length;
    CliPosition cursor;
    CliPosition anchor;
    int deeper;
    int redo;
    int back;
    int word_text;
    CliPosition *place;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const unsigned short *units;
};

typedef struct
{
    unsigned long long (*lines)(const CliViewVimCfg *args);
    const unsigned short *(*line)(const CliViewVimCfg *args);
    void (*line_end)(const CliViewVimCfg *args);
    unsigned int (*rows)(const CliViewVimCfg *args);
    unsigned long long (*span_bytes)(const CliViewVimCfg *args);
    void (*span)(const CliViewVimCfg *args);
    int (*edit)(const CliViewVimCfg *args);
    int (*indent)(const CliViewVimCfg *args);
    void (*place)(const CliViewVimCfg *args);
    int (*step)(const CliViewVimCfg *args);
    int (*find)(const CliViewVimCfg *args);
} CliViewVimNs;
EMBED_TABLE_LAYOUT(CliViewVimNs, lines, line, line_end, rows, span_bytes, span, edit, indent, place, step, find);
EMBED_TABLE_STORAGE CliViewVimNs cli_view_vim EMBED_UNUSED = {
    .lines = cli_view_vim_lines,
    .line = cli_view_vim_line,
    .line_end = cli_view_vim_line_end,
    .rows = cli_view_vim_rows,
    .span_bytes = cli_view_vim_span_bytes,
    .span = cli_view_vim_span,
    .edit = cli_view_vim_edit,
    .indent = cli_view_vim_indent,
    .place = cli_view_vim_place,
    .step = cli_view_vim_step,
    .find = cli_view_vim_find,
};

#endif

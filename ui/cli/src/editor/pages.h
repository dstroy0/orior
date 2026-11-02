// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pages.h: paged texts: files too large for the page to store, stored here as the file on the disk and the lines
// written over it, the page storing only the lines near its view.
//
// A file opened paged is read for an index behind the page: the byte each 1024th line starts at, and how many lines
// there are, which grows as the reading goes and is whole once it ends. A line is read from the disk from the nearest
// line the index records before it. The lines are split as the page splits a file it reads: at each "\n", a "\r" before
// it taken off, and a file that ends in "\n" has an empty line last.
//
// The text is a list of pieces, each a run of the file's lines or of lines written here, and an edit the page makes
// takes the lines it reaches out of their pieces and puts its own in their place: the file on the disk is read and
// never written until the text is saved. A save writes every piece in turn to a file beside the file, moves it over the
// file, and reads the file afresh. The lines written since the file was read are the changes the gutter marks
// (pages.c, pages_edit.c)
#ifndef PAGES_H
#define PAGES_H
typedef struct CliPagesOutCfg CliPagesOutCfg;
typedef struct CliPagesOutErrMsgCfg CliPagesOutErrMsgCfg;
typedef struct CliPagesFromCfg CliPagesFromCfg;
typedef struct CliPagesOutErrMsgNeedleCfg CliPagesOutErrMsgNeedleCfg;
typedef struct CliPagesPathCfg CliPagesPathCfg;
typedef struct CliPagesCfg CliPagesCfg;
typedef struct CliPagesViewCfg CliPagesViewCfg;

#include "document.h"
#include "embed_dispatch_layout.h"

// the file at `path` opened as the paged text `key`, its index read on a thread of its own; its shape as it stands
// written as `{"lines", "indexed", "size"}`: 1, or 0 with why given. The shape of the text under `key`; its lines from
// `from` to before `to`, once the index has read as far as `to` or the whole file, as far as the text contains them,
// written as a JSON array of strings, each cut after its first `*cut` letters where `cut` is given and not 0, `*cut`
// left 0 where no line was longer, and where `lengths` is given each line's whole length in the page's units, UTF-16's,
// written into it as a JSON array of numbers; and the text let go
int cli_pages_open(const CliPagesPathCfg *args);
int cli_pages_shape(const CliPagesPathCfg *args);
void cli_pages_release(const CliPagesCfg *args);

// A view of a paged text as the page reports it, each frame it moves and once as it rests: the line at its top, the
// rows it shows, the letters across it reaches, the lines the minimap stands beside from `map_from` to before `map_to`,
// the lines the page holds from `cached_from` to before `cached_to`, cut after `cached_cut` letters, 0 where whole, and
// the letters a row holds where it wraps its lines, each line as many rows as those letters fill, 0 where it does not
typedef struct
{
    unsigned long long top;
    unsigned long long rows;
    unsigned long long columns;
    unsigned long long map_from;
    unsigned long long map_to;
    unsigned long long cached_from;
    unsigned long long cached_to;
    unsigned long long cached_cut;
    unsigned long long width;
} CliPagesView;

// The lines a view is answered: from `from` to before `to`, none where they are the same, cut after `cut` letters, 0
// where whole; and the lines the page saves, from `save_from` to before `save_to`, those it holds outside them let go
typedef struct
{
    unsigned long long from;
    unsigned long long to;
    unsigned long long cut;
    unsigned long long save_from;
    unsigned long long save_to;
} CliPagesPositioned;

// the view `view` of the paged text under `key` placed into `positioned`, the view saved as the last reported. A view
// reported where it was reported last, or more than CLI_PAGE_STILL milliseconds after the last report, rests: its rows
// whole where the page does not hold them whole, then the lines it saves whole, CLI_PAGE_ADJACENT about its rows and
// the minimap's, a step of CLI_PAGE_REMAINDER_BYTES at a time. Any other moves: its rows where it will stand a report
// later at the step it moved since the last, and where it stands where that step is no more than its rows,
// CLI_PAGE_EDGE about them, cut at its reach. Nothing where the page holds those already. A step of lines toward the
// ends of the page's is no more than CLI_PAGE_REMAINDER_BYTES of their bytes reach, one at the least; and a view that
// wraps its lines is placed the lines its rows take of those about them, each line as many rows as its letters fill at
// the view's width, a step as it moves no more than fill the rows placed. 1, or 0 where none is stored under the key
int cli_pages_on_view(const CliPagesViewCfg *args);
// how many lines the paged text under `key` contains as far as its index has read, into `lines`, and whether the index
// is whole, into `whole`: 1, or 0 where none is stored under the key; and its lines from `from` to before `to` as far
// as the index has read, as the walk of its colors reads them: each written as UTF-8 and ended by a NUL into a cell
// taken for `owner`, its start put into `line`, capacity for `to` - `from` of them, and how many were read into
// `count`: the cell, given back with persistent_buf_release, or NULL where none is stored or they could not be read
int cli_pages_count(const CliPagesCfg *args);
char *cli_pages_lines_alloc(const CliPagesFromCfg *args);

// the page's changes written, each as its document writes it, `edits[at]` edits of `changes[at]`, `count` changes,
// its edits placed against the text as it stood before any of them, written from the last to the first, the text's
// shape written after; the lines written since the file was read, each run written as `[from, to]`; the text's matches
// for `needle`, each `[line, from col, to col]`, read 16384 lines at a time until `most` are found; and the text saved
// to its file through a file beside it, the file read afresh, its index whole before the save answers, its shape
// written. Each 1, or 0 with why given (pages_edit.c)
int cli_pages_edit(const CliPagesOutErrMsgCfg *args);
int cli_pages_change(const CliPagesPathCfg *args);
int cli_pages_find(const CliPagesOutErrMsgNeedleCfg *args);
int cli_pages_save(const CliPagesPathCfg *args);

struct CliPagesOutCfg
{
    const char *key;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    unsigned long long from;
    unsigned long long to;
    unsigned long long *cut;
    VerbaLinea *lengths;
};

struct CliPagesOutErrMsgCfg
{
    const char *key;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const CliEdit *const *changes;
    const unsigned int *edits;
    unsigned int count_u32;
};

struct CliPagesFromCfg
{
    const char *key;
    unsigned long long from;
    unsigned long long to;
    const char **line;
    unsigned long long *count;
    CellOwner *owner;
};

struct CliPagesOutErrMsgNeedleCfg
{
    const char *key;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const CliDocNeedle *needle;
    unsigned long long most;
};

struct CliPagesPathCfg
{
    const char *key;
    const char *path;
    VerbaLinea *out;
    VerbaLinea *err_msg;
};

struct CliPagesCfg
{
    const char *key;
    unsigned long long *lines;
    int *whole;
};

struct CliPagesViewCfg
{
    const char *key;
    const CliPagesView *view;
    CliPagesPositioned *positioned;
};

typedef struct
{
    int (*open)(const CliPagesPathCfg *args);
    int (*shape)(const CliPagesPathCfg *args);
    int (*change)(const CliPagesPathCfg *args);
    int (*save)(const CliPagesPathCfg *args);
} CliPagesPathNs;
EMBED_TABLE_LAYOUT(CliPagesPathNs, open, shape, change, save);
EMBED_TABLE_STORAGE CliPagesPathNs cli_pages_path EMBED_UNUSED = {
    .open = cli_pages_open,
    .shape = cli_pages_shape,
    .change = cli_pages_change,
    .save = cli_pages_save,
};

typedef struct
{
    void (*release)(const CliPagesCfg *args);
    int (*count)(const CliPagesCfg *args);
} CliPagesNs;
EMBED_TABLE_LAYOUT(CliPagesNs, release, count);
EMBED_TABLE_STORAGE CliPagesNs cli_pages EMBED_UNUSED = {
    .release = cli_pages_release,
    .count = cli_pages_count,
};

typedef struct
{
    int (*view)(const CliPagesViewCfg *args);
} CliPagesViewNs;
EMBED_TABLE_LAYOUT(CliPagesViewNs, view);
EMBED_TABLE_STORAGE CliPagesViewNs cli_pages_view EMBED_UNUSED = {
    .view = cli_pages_on_view,
};

typedef struct
{
    char *(*lines_alloc)(const CliPagesFromCfg *args);
} CliPagesFromNs;
EMBED_TABLE_LAYOUT(CliPagesFromNs, lines_alloc);
EMBED_TABLE_STORAGE CliPagesFromNs cli_pages_from EMBED_UNUSED = {
    .lines_alloc = cli_pages_lines_alloc,
};

typedef struct
{
    int (*edit)(const CliPagesOutErrMsgCfg *args);
} CliPagesOutErrMsgNs;
EMBED_TABLE_LAYOUT(CliPagesOutErrMsgNs, edit);
EMBED_TABLE_STORAGE CliPagesOutErrMsgNs cli_pages_out_err_msg EMBED_UNUSED = {
    .edit = cli_pages_edit,
};

typedef struct
{
    int (*find)(const CliPagesOutErrMsgNeedleCfg *args);
} CliPagesOutErrMsgNeedleNs;
EMBED_TABLE_LAYOUT(CliPagesOutErrMsgNeedleNs, find);
EMBED_TABLE_STORAGE CliPagesOutErrMsgNeedleNs cli_pages_out_err_msg_needle EMBED_UNUSED = {
    .find = cli_pages_find,
};

#endif

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// preview.h: the live preview of the view displayed, as the window draws it beside the text or alone over it: the type of
// preview a file's path has, the mode chosen once for every file and saved, the view the preview is bound to, and the
// step that positions the view's text out again as it changes, at max once a look, a Markdown text as items (preview.c,
// preview_markdown.c). The items are in two movable cells sized to them, the items with the blocks after them and the
// text the items draw, read out by copy at each site, each item a row of the first cell sorted by its top: a run of
// text, a box, a rule, a line segment, or a block's rectangle, outlined while the cursor's line is in the block.
// The item row passed out is a copy, with its text copied beside it, that the next row read replaces. The preview's
// scroll top is here alone: the wheel moves it and leads the view's top along, the view's top moved by the editor
// moves it in turn and leads nothing back, and the source line at a top and the top of a source line answer each other
// between two blocks. A press on a link answers where it goes: a place in the preview, scrolled to, a file of the tree
// to open, or an address opened in the reader's browser here
#ifndef CLI_PREVIEW_H
#define CLI_PREVIEW_H
#include "../command_line/cli_internal.h"
#include "../command_line/pool.h"
#include "../machine/state.h"
#include "embed_dispatch_layout.h"

typedef struct CliPreviewCfg CliPreviewCfg;
typedef struct CliPreviewItemCfg CliPreviewItemCfg;
typedef struct CliPreviewScrollCfg CliPreviewScrollCfg;
typedef struct CliPreviewMarkCfg CliPreviewMarkCfg;

// the types of preview a file has by its path's ending: none, Markdown, HTML, CSS, SVG, Mermaid, LaTeX, and a file of
// requests
#define CLI_PREVIEW_NONE 0u
#define CLI_PREVIEW_MARKDOWN 1u
#define CLI_PREVIEW_HTML 2u
#define CLI_PREVIEW_CSS 3u
#define CLI_PREVIEW_SVG 4u
#define CLI_PREVIEW_MERMAID 5u
#define CLI_PREVIEW_LATEX 6u
#define CLI_PREVIEW_REQUESTS 7u

// the modes: no preview, the preview beside the text, and the preview alone over the editor's rectangle; the setting
// the mode is saved under in orior's folder; and how often the text is looked at for a change while the preview shows,
// in milliseconds
#define CLI_PREVIEW_OFF 0u
#define CLI_PREVIEW_BESIDE 1u
#define CLI_PREVIEW_ALONE 2u
#define CLI_PREVIEW_MODE_KEY "preview.mode"
#define CLI_PREVIEW_LOOK_MS 200ULL

// the types of item: a run of text, a box, a rule, a line segment from its corner across its width and height, and a
// block's rectangle, drawn only while marked
#define CLI_PREVIEW_ITEM_TEXT 0u
#define CLI_PREVIEW_ITEM_BOX 1u
#define CLI_PREVIEW_ITEM_RULE 2u
#define CLI_PREVIEW_ITEM_SEGMENT 3u
#define CLI_PREVIEW_ITEM_BLOCK 4u

// the faces a run is drawn in, the interface's sans and the code face; and the flags of an item: bold, italic, struck
// through, underlined, and marked by the cursor's line
#define CLI_PREVIEW_FACE_SANS 0u
#define CLI_PREVIEW_FACE_CODE 1u
#define CLI_PREVIEW_BOLD 1u
#define CLI_PREVIEW_ITALIC 2u
#define CLI_PREVIEW_STRIKE 4u
#define CLI_PREVIEW_UNDERLINE 8u
#define CLI_PREVIEW_HERE 16u

// the color roles an item is drawn in, which the window maps to its palette: the text, the text quieter and quieter
// still, the signal, the hairline, the code's ground, a link, the cursor's mark, none, and the first of the eight
// slices
#define CLI_PREVIEW_ROLE_TEXT 0u
#define CLI_PREVIEW_ROLE_QUIET 1u
#define CLI_PREVIEW_ROLE_FAINT 2u
#define CLI_PREVIEW_ROLE_SIGNAL 3u
#define CLI_PREVIEW_ROLE_LINE 4u
#define CLI_PREVIEW_ROLE_CODE_GROUND 5u
#define CLI_PREVIEW_ROLE_LINK 6u
#define CLI_PREVIEW_ROLE_HERE 7u
#define CLI_PREVIEW_ROLE_NONE 8u
#define CLI_PREVIEW_ROLE_SLICE 9u

// what a press answers: nothing, a file of the tree to open, a place in the preview already scrolled to, and an
// address opened in the reader's browser
#define CLI_PREVIEW_OPEN_NONE 0u
#define CLI_PREVIEW_OPEN_FILE 1u
#define CLI_PREVIEW_OPEN_PLACE 2u
#define CLI_PREVIEW_OPEN_OUT 3u

// One item, a row of the items cell: its type; its rectangle in the page's pixels from the preview's top left, a
// segment's run across and down; a run's text, `length` bytes from `text` in the text cell, its face and its size in
// the page's pixels; its color role, a box's stroke role and its corners' radius; its flags; the source line its block
// starts on; and the address a link goes to, `link_length` bytes from `link` in the text cell, none where 0
typedef struct
{
    unsigned int type;
    int x;
    int y;
    int width;
    int height;
    unsigned int text;
    unsigned int length;
    unsigned int face;
    unsigned int size;
    unsigned int role;
    unsigned int stroke;
    unsigned int radius;
    unsigned int flags;
    unsigned int line;
    unsigned int link;
    unsigned int link_length;
} CliPreviewItem;

// One block of the preview, a row after the items in the items cell, in the order of the source: the line it starts on,
// its top in the page's pixels, its block item's row, how deep in other blocks it is, and whether it is a heading
typedef struct
{
    unsigned int line;
    int top;
    unsigned int item;
    unsigned int depth;
    int heading;
} CliPreviewBlock;

// The items of a preview in their two movable cells: the items' cell and how many items, where its first block starts
// and how many blocks, the text's cell and its bytes, and the height every item fills in the page's pixels, counted as
// items are added; a cell is NULL where it carries nothing. The item `at_` starts CLI_PREVIEW_ITEM_AT(at_) bytes into
// the items' cell and the block `at_` CLI_PREVIEW_BLOCK_AT(items_, at_)
typedef struct
{
    const CarcerMovableEntry *cell;
    unsigned int items;
    unsigned long long block_at;
    unsigned int blocks;
    const CarcerMovableEntry *text_cell;
    unsigned int text_bytes;
    int height;
} CliPreviewItems;
#define CLI_PREVIEW_ITEM_AT(at_) ((unsigned long long)(at_) * sizeof(CliPreviewItem))
#define CLI_PREVIEW_BLOCK_AT(items_, at_) ((items_)->block_at + ((unsigned long long)(at_) * sizeof(CliPreviewBlock)))

// the max bytes of a run's text the item row passes out beside the row, the run's `length` cut to them
#define CLI_PREVIEW_ROW_TEXT 4096u

// Where a press goes: the file's path from the tree's top, the line to open it at where `placed`, the target, one of
// CLI_PREVIEW_OPEN_*, and the preview's top a place in it scrolled to
typedef struct
{
    char path[CLI_PATH_MAX_LEN];
    unsigned long long line;
    int placed;
    unsigned int target;
    int top_px;
} CliPreviewOpen;

// the preview begun with the mode saved among `entries`, off where none is saved; the type of preview the file at
// `path` has, CLI_PREVIEW_NONE for none; the mode; the mode set for every file and saved: the mode `word` reads,
// off, beside or alone, where it is not NULL, else `mode` where the preview is in another, and off where it is
// in `mode` already; the view under `key` bound, its path read from the views, the items let go and the top put at 0
// where the view, its path or its type changed: 1 where it did, NULL unbinding; and the step at `now_ms` for the view
// under `key` displayed in a rectangle `width_px` by `height_px` frame pixels at `scale`: the view bound, the text arranged
// again where it changed since the last look and a look is owed or the width moved, the top following the view's
// where the editor moved it, and the block of the cursor's line marked: 1 where the preview draws anew; and when the
// next look is owed, 0 for none
void cli_preview_begin(const CliPreviewCfg *args);
unsigned int cli_preview_type(const CliPreviewCfg *args);
unsigned int cli_preview_mode_get(const CliPreviewCfg *args);
void cli_preview_mode_set(const CliPreviewCfg *args);
int cli_preview_show(const CliPreviewCfg *args);
int cli_preview_step(const CliPreviewCfg *args);
unsigned long long cli_preview_deadline_ms(const CliPreviewCfg *args);
struct CliPreviewCfg
{
    const CliStateEntries *entries;
    const char *key;
    const char *path;
    unsigned int mode;
    const char *word;
    unsigned long long now_ms;
    int width_px;
    int height_px;
    int scale;
};

typedef struct
{
    void (*begin)(const CliPreviewCfg *args);
    unsigned int (*type)(const CliPreviewCfg *args);
    unsigned int (*mode_get)(const CliPreviewCfg *args);
    void (*mode_set)(const CliPreviewCfg *args);
    int (*show)(const CliPreviewCfg *args);
    int (*step)(const CliPreviewCfg *args);
    unsigned long long (*deadline_ms)(const CliPreviewCfg *args);
} CliPreviewNs;
EMBED_TABLE_LAYOUT(CliPreviewNs, begin, type, mode_get, mode_set, show, step, deadline_ms);
EMBED_TABLE_STORAGE CliPreviewNs cli_preview EMBED_UNUSED = {
    .begin = cli_preview_begin,
    .type = cli_preview_type,
    .mode_get = cli_preview_mode_get,
    .mode_set = cli_preview_mode_set,
    .show = cli_preview_show,
    .step = cli_preview_step,
    .deadline_ms = cli_preview_deadline_ms,
};

// how many items the preview has; the item `at` copied out of its cell, its text copied beside it and its first byte
// into `text_out` where it is not NULL, the copy located until the next row is read: the item, NULL past the last;
// the first item a draw from the top `top_px` starts at, the first item of the outermost
// block located at that top, found by a binary search over the blocks' tops; and the height every item fills, in the
// page's pixels
unsigned int cli_preview_item_count(const CliPreviewItemCfg *args);
const CliPreviewItem *cli_preview_item_row(const CliPreviewItemCfg *args);
unsigned int cli_preview_item_first(const CliPreviewItemCfg *args);
int cli_preview_item_extent(const CliPreviewItemCfg *args);
struct CliPreviewItemCfg
{
    unsigned int at;
    int top_px;
    const char **text_out;
};

typedef struct
{
    unsigned int (*count)(const CliPreviewItemCfg *args);
    const CliPreviewItem *(*item)(const CliPreviewItemCfg *args);
    unsigned int (*first)(const CliPreviewItemCfg *args);
    int (*extent)(const CliPreviewItemCfg *args);
} CliPreviewItemNs;
EMBED_TABLE_LAYOUT(CliPreviewItemNs, count, item, first, extent);
EMBED_TABLE_STORAGE CliPreviewItemNs cli_preview_item EMBED_UNUSED = {
    .count = cli_preview_item_count,
    .item = cli_preview_item_row,
    .first = cli_preview_item_first,
    .extent = cli_preview_item_extent,
};

// the top moved by `delta_px` page pixels, no further up than the first item and no further down than the last fills
// the rectangle, and in the beside mode the source line at the new top passed to the view as its scroll, the view's
// top read back so the follow leads nothing: 1 where the top moved; the top in the page's pixels; the source line at
// the top `top_px`, between the two blocks it is between, its fraction of a line in 65536ths into `fraction_out`
// where it is not NULL; and the top of the source line `line` and `line_fraction` 65536ths, between the two blocks it
// falls between, each a binary search over the blocks
int cli_preview_scroll_by(const CliPreviewScrollCfg *args);
int cli_preview_scroll_top(const CliPreviewScrollCfg *args);
unsigned long long cli_preview_scroll_line_at(const CliPreviewScrollCfg *args);
int cli_preview_scroll_top_of(const CliPreviewScrollCfg *args);
struct CliPreviewScrollCfg
{
    int delta_px;
    int top_px;
    unsigned long long line;
    unsigned int line_fraction;
    unsigned int *fraction_out;
};

typedef struct
{
    int (*by)(const CliPreviewScrollCfg *args);
    int (*top)(const CliPreviewScrollCfg *args);
    unsigned long long (*line_at)(const CliPreviewScrollCfg *args);
    int (*top_of)(const CliPreviewScrollCfg *args);
} CliPreviewScrollNs;
EMBED_TABLE_LAYOUT(CliPreviewScrollNs, by, top, line_at, top_of);
EMBED_TABLE_STORAGE CliPreviewScrollNs cli_preview_scroll EMBED_UNUSED = {
    .by = cli_preview_scroll_by,
    .top = cli_preview_scroll_top,
    .line_at = cli_preview_scroll_line_at,
    .top_of = cli_preview_scroll_top_of,
};

// the innermost block at or before the source line `line` marked, the block marked before let go: 1 where the mark
// moved; and a press at `x`, `y` page pixels from the preview's top left resolved into `open_out`: a link to a place of
// the preview scrolls there, one to a file of the tree answers the file, one to an address out of the tree is opened
// in the reader's browser: the target, CLI_PREVIEW_OPEN_NONE where the press is on no link
int cli_preview_mark_line(const CliPreviewMarkCfg *args);
unsigned int cli_preview_mark_press(const CliPreviewMarkCfg *args);
struct CliPreviewMarkCfg
{
    unsigned long long line;
    int x;
    int y;
    CliPreviewOpen *open_out;
};

typedef struct
{
    int (*mark)(const CliPreviewMarkCfg *args);
    unsigned int (*press)(const CliPreviewMarkCfg *args);
} CliPreviewMarkNs;
EMBED_TABLE_LAYOUT(CliPreviewMarkNs, mark, press);
EMBED_TABLE_STORAGE CliPreviewMarkNs cli_preview_mark EMBED_UNUSED = {
    .mark = cli_preview_mark_line,
    .press = cli_preview_mark_press,
};

#endif

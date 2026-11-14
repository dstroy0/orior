// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// rename.h: Rename Symbol's box over the name at the cursor: the name in it selected as it opens, text typed written
// over the selection and after it, Backspace erasing, Enter accepting the new name and Escape closing the box. Every
// key and all text go to the box first while it is open; a press off it closes it (rename.c)
#ifndef GUI_WINDOW_RENAME_H
#define GUI_WINDOW_RENAME_H
#include "../../../includes/window/glyphs.h"
#include "../../../includes/window/palette.h"
#include "../../cli/src/editor/document.h"
#include "embed_dispatch_layout.h"
typedef struct GuiRenameCfg GuiRenameCfg;

// the sizes in the page's pixels: the box's padding across and down, its least width, the caret's width and the text
// size
#define GUI_RENAME_PAD_X 6
#define GUI_RENAME_PAD_Y 3
#define GUI_RENAME_MIN_WIDTH 120
#define GUI_RENAME_CARET 2
#define GUI_RENAME_SIZE 13u
// what a key comes to: not the box's, taken, taken and the box closes, and taken with the new name accepted
#define GUI_RENAME_PASSED 0u
#define GUI_RENAME_TAKEN 1u
#define GUI_RENAME_CLOSES 2u
#define GUI_RENAME_ACCEPTS 3u

// The box: whether it is open; whether its text is selected whole, as it is until the first edit; the name's span in
// the view, its line and its units; the text with its bytes; and the box as it was drawn last
typedef struct
{
    int open;
    int selected;
    CliPosition from;
    CliPosition to;
    char text[CLI_NAME_MAX_LEN];
    unsigned long long length;
    GuiRect rect;
} GuiRename;

// the box closed
void gui_rename_init(const GuiRenameCfg *args);
// the box opened over the word at the cursor of the view `key`, the word its text, selected: 1, or 0 where no word is
// at the cursor or no view is open under the key
int gui_rename_open(const GuiRenameCfg *args);
void gui_rename_close(const GuiRenameCfg *args);
// the key `name` pressed while the box is open: one of the answers above; and `length` bytes of `text` typed, written
// over the selection where the text is selected: 1 where the box took it
unsigned int gui_rename_key(const GuiRenameCfg *args);
int gui_rename_text(const GuiRenameCfg *args);
// the box drawn over the name's cell `anchor`, inside `bounds`, at `scale`
void gui_rename_draw(const GuiRenameCfg *args);
// whether x, y is on the box as it was drawn last
int gui_rename_hit(const GuiRenameCfg *args);
struct GuiRenameCfg
{
    GuiRename *rename;
    const GuiRename *rename_read;
    const char *key;
    const char *name;
    const char *text;
    unsigned long long length;
    GuiRaster *raster;
    GuiGlyphs *glyphs;
    const GuiPalette *palette;
    GuiRect anchor;
    GuiRect bounds;
    int scale;
    int x;
    int y;
};

typedef struct
{
    void (*init)(const GuiRenameCfg *args);
    int (*open)(const GuiRenameCfg *args);
    void (*close)(const GuiRenameCfg *args);
    unsigned int (*key)(const GuiRenameCfg *args);
    int (*text)(const GuiRenameCfg *args);
    void (*draw)(const GuiRenameCfg *args);
    int (*hit)(const GuiRenameCfg *args);
} GuiRenameNs;
EMBED_TABLE_LAYOUT(GuiRenameNs, init, open, close, key, text, draw, hit);
EMBED_TABLE_STORAGE GuiRenameNs gui_rename EMBED_UNUSED = {
    .init = gui_rename_init,
    .open = gui_rename_open,
    .close = gui_rename_close,
    .key = gui_rename_key,
    .text = gui_rename_text,
    .draw = gui_rename_draw,
    .hit = gui_rename_hit,
};

#endif

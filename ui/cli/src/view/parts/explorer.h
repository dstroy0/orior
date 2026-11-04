// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// explorer.h: the explorer's kind of side pane: the pane of the tree's files headed by the tree's name, its rows the
// folders and files, a folder at a time as each is opened, read where the explorer's listing stands
// (editor/files.h); a row is a line down from each folder above it, the folder's arrow, the folder's icon or the
// file's mark, and the name; a folder's row pressed opens or closes it and a file's opens the file. The mark of a
// file's name, which the other panes draw before a file, is read here too (explorer.c)
#ifndef GUI_PARTS_EXPLORER_H
#define GUI_PARTS_EXPLORER_H
#include "../../editor/files.h"
#include "embed_dispatch_layout.h"
#include "pane.h"

typedef struct GuiExplorerMarkCfg GuiExplorerMarkCfg;

// the sizes in the page's pixels: the arrow's box, the icon's box and the gap after it, and a mark's text size; and
// the most bytes of a file's mark text with its end, and of the name it is read from
#define GUI_EXPLORER_TWISTY 16
#define GUI_EXPLORER_ICON 16
#define GUI_EXPLORER_ICON_GAP 8
#define GUI_EXPLORER_MARK_SIZE 11u
#define GUI_EXPLORER_MARK_MOST 64u

// the explorer's kind of pane
extern const GuiPaneKind gui_explorer_kind;

// the mark of the file `name`, `length` bytes, as the page's icons draw a file's type: its text into `text`, sized
// GUI_EXPLORER_MARK_MOST, and which of the palette's mark colors it takes, counted from 1, 0 for the quieter one
unsigned int gui_explorer_mark(const GuiExplorerMarkCfg *args);
struct GuiExplorerMarkCfg
{
    const char *name;
    unsigned long long length;
    char *text;
};

typedef struct
{
    unsigned int (*mark)(const GuiExplorerMarkCfg *args);
} GuiExplorerMarkNs;
EMBED_TABLE_LAYOUT(GuiExplorerMarkNs, mark);
EMBED_TABLE_STORAGE GuiExplorerMarkNs gui_explorer_mark_table EMBED_UNUSED = {
    .mark = gui_explorer_mark,
};

#endif

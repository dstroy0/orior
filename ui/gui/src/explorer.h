// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// explorer.h: the explorer's side pane table: the pane of the tree's files headed by the tree's name, its rows the
// folders and files, a folder at a time as each is opened, read where the explorer's listing stands
// (editor/files.h); a row is a line down from each folder above it, the folder's arrow, the folder's icon or the
// file's mark, and the name; a folder's row pressed opens or closes it and a file's opens the file. The mark of a
// file's name, which the other panes draw before a file, is read here too (explorer.c)
#ifndef GUI_WINDOW_EXPLORER_H
#define GUI_WINDOW_EXPLORER_H
#include "../../cli/src/editor/files.h"
#include "embed_dispatch_layout.h"
#include "pane.h"
#include "themes.h"

typedef struct GuiExplorerMarkCfg GuiExplorerMarkCfg;

// the sizes in the page's pixels: the arrow's box, the icon's box and the gap after it, a mark's text size, the gap
// before a file's folder and the folder's text size; and the most bytes of a file's mark text with its end, and of the
// name it is read from
#define GUI_EXPLORER_TWISTY 16
#define GUI_EXPLORER_ICON 16
#define GUI_EXPLORER_ICON_GAP 8
#define GUI_EXPLORER_MARK_SIZE 11u
#define GUI_EXPLORER_WHERE_GAP 8
#define GUI_EXPLORER_WHERE_SIZE 11u
#define GUI_EXPLORER_MARK_MAX 64u

typedef struct GuiExplorerFileCfg GuiExplorerFileCfg;

// a file drawn in the pane's row from x: its mark centered in the icon's box in the mark's color, its name after the
// icon's gap in `color`, and where `folder_length` is not 0 its folder after the where gap in the quieter color at the
// where size, each cut to `clip`: the x after what was drawn
float gui_explorer_file_draw(const GuiExplorerFileCfg *args);
struct GuiExplorerFileCfg
{
    const GuiPaneCfg *pane;
    int x;
    const char *name;
    unsigned long long length;
    const char *folder;
    unsigned long long folder_length;
    GuiColor color;
    GuiRect clip;
};

typedef struct
{
    float (*file_draw)(const GuiExplorerFileCfg *args);
} GuiExplorerFileNs;
EMBED_TABLE_LAYOUT(GuiExplorerFileNs, file_draw);
EMBED_TABLE_STORAGE GuiExplorerFileNs gui_explorer_file EMBED_UNUSED = {
    .file_draw = gui_explorer_file_draw,
};

// the explorer's pane table
extern const GuiPaneTable gui_explorer_table;

// the mark of the file `name`, `length` bytes, as the page's icons draw a file's type: its text into `text`, sized
// GUI_EXPLORER_MARK_MAX, and which of the palette's mark colors it takes, counted from 1, 0 for the quieter one, or
// GUI_EXPLORER_MARK_OWN with the mark's own color into `color_out` where the theme's icon names one, a color of
// `palette` by name or a hex value
#define GUI_EXPLORER_MARK_OWN (GUI_MARKS + 1u)
unsigned int gui_explorer_mark(const GuiExplorerMarkCfg *args);
struct GuiExplorerMarkCfg
{
    const char *name;
    unsigned long long length;
    char *text;
    const GuiPalette *palette;
    GuiColor *color_out;
};

// the theme whose icons the marks consult first, NULL for none; it stands until the next call
typedef struct
{
    const GuiTheme *theme_read;
} GuiExplorerThemeCfg;
void gui_explorer_theme_set(const GuiExplorerThemeCfg *args);

typedef struct
{
    void (*set)(const GuiExplorerThemeCfg *args);
} GuiExplorerThemeNs;
EMBED_TABLE_LAYOUT(GuiExplorerThemeNs, set);
EMBED_TABLE_STORAGE GuiExplorerThemeNs gui_explorer_theme EMBED_UNUSED = {
    .set = gui_explorer_theme_set,
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

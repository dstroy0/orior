// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// icons.h: the window's icons, the shapes the page's chrome is read by, a folder for the files, a branch for git, a
// bell for notifications, each a path on a square of sixteen units drawn in one line with round ends and joins, its
// filled parts dots; an icon drawn once at a pixel size and line width and kept in a cache of coverage masks (icons.c)
#ifndef GUI_ICONS_H
#define GUI_ICONS_H
#include "raster.h"

// the icons kept drawn at once; once every slot is used every one is let go and drawn again as it is asked for; and
// the units an icon's square is drawn on
#define GUI_ICON_SLOTS 64u
#define GUI_ICON_UNITS 16.0f

// An icon kept: whether its slot is used, the icon, pixel size and line width that key it, and its mask, the cover in
// a cell of its own
typedef struct
{
    int used;
    unsigned int icon;
    unsigned int size;
    float stroke;
    GuiMask mask;
} GuiKeptIcon;

// The icons: the slots the icons drawn are kept in, how many of them used
typedef struct
{
    GuiKeptIcon slot[GUI_ICON_SLOTS];
    unsigned int kept;
} GuiIcons;

// the icons holding none drawn; and every cell of theirs given back
void gui_icons_made(GuiIcons *icons);
void gui_icons_given(GuiIcons *icons);
// whether an icon is named `name`
int gui_icon_named(const char *name);
// the icon named `name` drawn on a square of `size` pixels a side, its line `stroke` pixels wide, and kept: its mask,
// laid with its top left at the square's, or NULL where no icon has the name or a cell could not be taken
const GuiMask *gui_icon(GuiIcons *icons, const char *name, unsigned int size, float stroke);
#endif

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// icons.h: the window's icons, the shapes the page's top menu is read by, a folder for the files, a branch for git, a
// bell for notifications, each a path on a square of sixteen units drawn in one line with round ends and joins, its
// filled parts dots, and the wordmark's eye on its own box with its iris filled; an icon drawn once at a pixel size
// and line width and saved in a cache of coverage masks (icons.c)
#ifndef GUI_ICONS_H
#define GUI_ICONS_H
#include "embed_dispatch_layout.h"
#include "../raster/raster.h"

typedef struct GuiIconsCoverCfg GuiIconsCoverCfg;
typedef struct GuiIconsCfg GuiIconsCfg;

// the cache's entries, one rasterized icon each; when every entry is in use the cache is emptied and icons are
// rasterized again on demand; the units of an icon's square; and the width of an icon's line in those units, as the
// page draws them
#define GUI_ICON_CACHE_ENTRIES 64u
#define GUI_ICON_UNITS 16.0f
#define GUI_ICON_STROKE 1.1f
// the eye's box in its own units, its longer side, and its line in those units as the page strokes it: a line's
// width in pixels at a size is the stroke by the size over the side, and no thinner than a pixel
#define GUI_LOGO_SIDE 255.2f
#define GUI_LOGO_STROKE 2.2f

// A cache entry: whether it is in use, its key (the icon, the pixel size and the line width), and the coverage mask,
// allocated in a cell of its own
typedef struct
{
    int used;
    unsigned int icon;
    unsigned int size;
    float stroke;
    GuiMask mask_bits;
} GuiIconCacheEntry;

// The icons: the cache's entries and how many of them are in use
typedef struct
{
    GuiIconCacheEntry entry[GUI_ICON_CACHE_ENTRIES];
    unsigned int count;
    unsigned long long covers_used;
} GuiIcons;

// the icons holding none drawn; and every cell of theirs given back
void gui_icons_init(const GuiIconsCfg *args);
// whether an icon is named `name`
int gui_icon_exists(const GuiIconsCfg *args);
// the icon named `name` drawn on a square of `size` pixels a side, its line `stroke` pixels wide, and saved: its mask,
// laid with its top left at the square's, or NULL where no icon has the name or a cell could not be taken
const GuiMask *gui_icon_get(const GuiIconsCfg *args);
// the shape of `corners` corners, each an x and a y in pixels, filled on `cover`, `width` by `height` floats from 0 to
// 1: each pixel of it takes its share inside the shape where that is more than it has, a sample inside where the edges
// round it wind about it, as SVG's nonzero fill has it
void gui_icons_polygon_fill(const GuiIconsCoverCfg *args);
struct GuiIconsCoverCfg
{
    float *cover;
    unsigned int width;
    unsigned int height;
    const float *corner;
    unsigned int corners;
};

struct GuiIconsCfg
{
    GuiIcons *icons;
    const char *name;
    unsigned int size;
    float stroke;
};

typedef struct
{
    void (*init)(const GuiIconsCfg *args);
    int (*icon_by_name)(const GuiIconsCfg *args);
    const GuiMask *(*icon)(const GuiIconsCfg *args);
} GuiIconsNs;
EMBED_TABLE_LAYOUT(GuiIconsNs, init, icon_by_name, icon);
EMBED_TABLE_STORAGE GuiIconsNs gui_icons EMBED_UNUSED = {
    .init = gui_icons_init,
    .icon_by_name = gui_icon_exists,
    .icon = gui_icon_get,
};

typedef struct
{
    void (*polygon_fill)(const GuiIconsCoverCfg *args);
} GuiIconsCoverNs;
EMBED_TABLE_LAYOUT(GuiIconsCoverNs, polygon_fill);
EMBED_TABLE_STORAGE GuiIconsCoverNs gui_icons_cover EMBED_UNUSED = {
    .polygon_fill = gui_icons_polygon_fill,
};

#endif

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// themes.h: the reader's color themes. A theme is a name, the scheme it is drawn over, the colors it sets in place of
// the scheme's by the palette's names, and the icons it gives files by patterns of their names. The themes are the
// state store's `themes` entry, read from its JSON and written back to it; one theme is shared as text and read back
// from such text (themes.c)
#ifndef GUI_THEMES_H
#define GUI_THEMES_H
#include "../../../includes/raster/raster.h"
#include "../../cli/src/command_line/pool.h"
#include "embed_dispatch_layout.h"

typedef struct GuiThemesOwnerCfg GuiThemesOwnerCfg;
typedef struct GuiThemesTextCfg GuiThemesTextCfg;
typedef struct GuiThemesOutCfg GuiThemesOutCfg;
typedef struct GuiThemesNameCfg GuiThemesNameCfg;
typedef struct GuiThemesAtCfg GuiThemesAtCfg;
typedef struct GuiThemesAtNameCfg GuiThemesAtNameCfg;
typedef struct GuiThemesAtColorCfg GuiThemesAtColorCfg;
typedef struct GuiThemesAtIconCfg GuiThemesAtIconCfg;
typedef struct GuiThemeOutCfg GuiThemeOutCfg;
typedef struct GuiThemeTextCfg GuiThemeTextCfg;
typedef struct GuiThemeNameCfg GuiThemeNameCfg;

// the most themes kept; the most bytes a theme's name takes with its end; the most colors a theme sets, the palette's
// named colors and a plugin's tokens; the most icons a theme gives; the most bytes of an icon's pattern, of its mark,
// three letters of four bytes each, and of its color's text; and the scheme's own name
#define GUI_THEMES_MAX 24u
#define GUI_THEME_NAME 64u
#define GUI_THEME_COLORS 192u
#define GUI_THEME_ICONS 32u
#define GUI_THEME_PATTERN 64u
#define GUI_THEME_MARK 13u
#define GUI_THEME_COLOR_TEXT 32u
#define GUI_THEME_BASE_NAME "Dark"

// One color a theme sets: the palette's name for it and the color
typedef struct
{
    char name[GUI_THEME_NAME];
    GuiColor color;
} GuiThemeColor;

// One icon a theme gives: the pattern of the file names it covers, `*` for any letters and `?` for one; the mark, a
// letter or a few; and its color, one of the palette's by name or a color's hex value, empty for the quieter mark
typedef struct
{
    char pattern[GUI_THEME_PATTERN];
    char mark[GUI_THEME_MARK];
    char color[GUI_THEME_COLOR_TEXT];
} GuiThemeIcon;

// A theme: its name, its colors and its icons
typedef struct
{
    char name[GUI_THEME_NAME];
    GuiThemeColor color[GUI_THEME_COLORS];
    unsigned int colors;
    GuiThemeIcon icon[GUI_THEME_ICONS];
    unsigned int icons;
} GuiTheme;

// The themes kept, in the order of the entry: `count` records of GuiTheme in one movable cell their holder owns, each
// at its place times the record's bytes, read into a record of the call that reads it
typedef struct
{
    const CarcerMovableEntry *cell;
    const CellOwner *owner;
    unsigned int count;
} GuiThemes;

// `themes` given a cell of `owner` for GUI_THEMES_MAX themes, none in it: 1, or 0 where the cell is refused; the cell
// given back; and the theme `at` read into `theme_out`: 1, or 0 where `at` names none
int gui_themes_init(const GuiThemesOwnerCfg *args);
void gui_themes_free(const GuiThemesOwnerCfg *args);
int gui_theme_get(const GuiThemesAtCfg *args);

// `themes` read from `text`, the entry's JSON array of themes, `size` bytes, the tree read in a cell of `owner`; a
// theme of a scheme other than the window's, a color whose value does not read and an icon without a pattern are left
// out; and `themes` written into `out` as that array, compact
void gui_themes_read(const GuiThemesTextCfg *args);
void gui_themes_text(const GuiThemesOutCfg *args);
// the theme named `name`: its place, or `count` where none is; and a name no theme has, from `name`, into `name_out`
// sized GUI_THEME_NAME: `name` itself, or its stem with a number after it where it is taken, the scheme's own name
// taken too
unsigned int gui_theme_find(const GuiThemesNameCfg *args);
void gui_theme_free_name(const GuiThemesNameCfg *args);
// a theme added named `name`, its colors and icons those of the theme `from`, none where `from` is `count`: its
// place, or `count` where the themes are full; the theme `at` taken out; and the theme `at` renamed `name`
unsigned int gui_theme_add(const GuiThemesAtNameCfg *args);
void gui_theme_remove(const GuiThemesAtCfg *args);
void gui_theme_rename(const GuiThemesAtNameCfg *args);
// the color `name` of the theme `at` set to `color` where `on` is 1, else put back to the scheme's; and whether the
// theme sets it, its color into `color_pointer`
void gui_theme_color_set(const GuiThemesAtColorCfg *args);
int gui_theme_color_get(const GuiThemesAtColorCfg *args);
// the icon of `pattern` in the theme `at` set with `mark` and `color`, in place of one of the pattern; and the icon of
// `pattern` taken off
void gui_theme_icon_set(const GuiThemesAtIconCfg *args);
void gui_theme_icon_remove(const GuiThemesAtIconCfg *args);
// the first icon of `theme` whose pattern covers the file name `name`, `length` bytes, letters compared without case:
// the icon, or NULL where none does
const GuiThemeIcon *gui_theme_icon_match(const GuiThemeNameCfg *args);
// `theme` written into `out` as text to share, one member a line; and a theme read from `size` bytes of `text` into
// `theme_out`, the tree read in a cell of `owner`: 1, or 0 where the text is no theme of the window's scheme
void gui_theme_text(const GuiThemeOutCfg *args);
int gui_theme_read(const GuiThemeTextCfg *args);

struct GuiThemesOwnerCfg
{
    GuiThemes *themes;
    const CellOwner *owner;
};

struct GuiThemesTextCfg
{
    GuiThemes *themes;
    const char *text;
    unsigned long long size;
    const CellOwner *owner;
};

struct GuiThemesOutCfg
{
    const GuiThemes *themes_read;
    VerbaLinea *out;
};

struct GuiThemesNameCfg
{
    const GuiThemes *themes_read;
    const char *name;
    char *name_out;
};

struct GuiThemesAtCfg
{
    GuiThemes *themes;
    unsigned int at;
    GuiTheme *theme_out;
};

struct GuiThemesAtNameCfg
{
    GuiThemes *themes;
    unsigned int at;
    unsigned int from;
    const char *name;
};

struct GuiThemesAtColorCfg
{
    GuiThemes *themes;
    unsigned int at;
    const char *name;
    GuiColor color;
    int on;
    GuiColor *color_pointer;
};

struct GuiThemesAtIconCfg
{
    GuiThemes *themes;
    unsigned int at;
    const char *pattern;
    const char *mark;
    const char *color;
};

struct GuiThemeOutCfg
{
    const GuiTheme *theme_read;
    VerbaLinea *out;
};

struct GuiThemeTextCfg
{
    const char *text;
    unsigned long long size;
    GuiTheme *theme_out;
    const CellOwner *owner;
};

struct GuiThemeNameCfg
{
    const GuiTheme *theme_read;
    const char *name;
    unsigned long long length;
};

typedef struct
{
    void (*read)(const GuiThemesTextCfg *args);
} GuiThemesTextNs;
EMBED_TABLE_LAYOUT(GuiThemesTextNs, read);
EMBED_TABLE_STORAGE GuiThemesTextNs gui_themes_text_table EMBED_UNUSED = {
    .read = gui_themes_read,
};

typedef struct
{
    void (*text)(const GuiThemesOutCfg *args);
} GuiThemesOutNs;
EMBED_TABLE_LAYOUT(GuiThemesOutNs, text);
EMBED_TABLE_STORAGE GuiThemesOutNs gui_themes_out EMBED_UNUSED = {
    .text = gui_themes_text,
};

typedef struct
{
    unsigned int (*find)(const GuiThemesNameCfg *args);
    void (*free_name)(const GuiThemesNameCfg *args);
} GuiThemesNameNs;
EMBED_TABLE_LAYOUT(GuiThemesNameNs, find, free_name);
EMBED_TABLE_STORAGE GuiThemesNameNs gui_themes_name EMBED_UNUSED = {
    .find = gui_theme_find,
    .free_name = gui_theme_free_name,
};

typedef struct
{
    void (*remove)(const GuiThemesAtCfg *args);
    int (*get)(const GuiThemesAtCfg *args);
} GuiThemesAtNs;
EMBED_TABLE_LAYOUT(GuiThemesAtNs, remove, get);
EMBED_TABLE_STORAGE GuiThemesAtNs gui_themes_at EMBED_UNUSED = {
    .remove = gui_theme_remove,
    .get = gui_theme_get,
};

typedef struct
{
    int (*init)(const GuiThemesOwnerCfg *args);
    void (*free)(const GuiThemesOwnerCfg *args);
} GuiThemesOwnerNs;
EMBED_TABLE_LAYOUT(GuiThemesOwnerNs, init, free);
EMBED_TABLE_STORAGE GuiThemesOwnerNs gui_themes_owner EMBED_UNUSED = {
    .init = gui_themes_init,
    .free = gui_themes_free,
};

typedef struct
{
    unsigned int (*add)(const GuiThemesAtNameCfg *args);
    void (*rename)(const GuiThemesAtNameCfg *args);
} GuiThemesAtNameNs;
EMBED_TABLE_LAYOUT(GuiThemesAtNameNs, add, rename);
EMBED_TABLE_STORAGE GuiThemesAtNameNs gui_themes_at_name EMBED_UNUSED = {
    .add = gui_theme_add,
    .rename = gui_theme_rename,
};

typedef struct
{
    void (*color_set)(const GuiThemesAtColorCfg *args);
    int (*color_get)(const GuiThemesAtColorCfg *args);
} GuiThemesAtColorNs;
EMBED_TABLE_LAYOUT(GuiThemesAtColorNs, color_set, color_get);
EMBED_TABLE_STORAGE GuiThemesAtColorNs gui_themes_at_color EMBED_UNUSED = {
    .color_set = gui_theme_color_set,
    .color_get = gui_theme_color_get,
};

typedef struct
{
    void (*icon_set)(const GuiThemesAtIconCfg *args);
    void (*icon_remove)(const GuiThemesAtIconCfg *args);
} GuiThemesAtIconNs;
EMBED_TABLE_LAYOUT(GuiThemesAtIconNs, icon_set, icon_remove);
EMBED_TABLE_STORAGE GuiThemesAtIconNs gui_themes_at_icon EMBED_UNUSED = {
    .icon_set = gui_theme_icon_set,
    .icon_remove = gui_theme_icon_remove,
};

typedef struct
{
    void (*text)(const GuiThemeOutCfg *args);
} GuiThemeOutNs;
EMBED_TABLE_LAYOUT(GuiThemeOutNs, text);
EMBED_TABLE_STORAGE GuiThemeOutNs gui_theme_out EMBED_UNUSED = {
    .text = gui_theme_text,
};

typedef struct
{
    int (*read)(const GuiThemeTextCfg *args);
} GuiThemeTextNs;
EMBED_TABLE_LAYOUT(GuiThemeTextNs, read);
EMBED_TABLE_STORAGE GuiThemeTextNs gui_theme_text_table EMBED_UNUSED = {
    .read = gui_theme_read,
};

typedef struct
{
    const GuiThemeIcon *(*icon_match)(const GuiThemeNameCfg *args);
} GuiThemeNameNs;
EMBED_TABLE_LAYOUT(GuiThemeNameNs, icon_match);
EMBED_TABLE_STORAGE GuiThemeNameNs gui_theme_name EMBED_UNUSED = {
    .icon_match = gui_theme_icon_match,
};

#endif

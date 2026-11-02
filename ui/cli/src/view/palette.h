// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// palette.h: the colors the window draws in, one palette a scheme, each the colors style.css names for it: the
// grounds, the texts, the lines and signals, the bar's and the status bar's, the editor's, and a token's color by the
// name its grammar gives it (palette.c)
#ifndef GUI_PALETTE_H
#define GUI_PALETTE_H
typedef struct GuiPaletteAtSizeCfg GuiPaletteAtSizeCfg;
typedef struct GuiPaletteAtCfg GuiPaletteAtCfg;
typedef struct GuiPaletteTextCfg GuiPaletteTextCfg;
typedef struct GuiPaletteForegroundCfg GuiPaletteForegroundCfg;
typedef struct GuiPaletteSizeCfg GuiPaletteSizeCfg;
typedef struct GuiPaletteOwnCfg GuiPaletteOwnCfg;
typedef struct GuiPaletteOwnBarPercentCfg GuiPaletteOwnBarPercentCfg;
typedef struct GuiPaletteLengthCfg GuiPaletteLengthCfg;
typedef struct GuiPaletteSizeColorCfg GuiPaletteSizeColorCfg;
typedef struct GuiPaletteCfg GuiPaletteCfg;
#include "embed_dispatch_layout.h"
#include "raster.h"

// the schemes: the dark one; and the colors a file's mark in the explorer is drawn in, --t1 to --t8
#define GUI_SCHEME_DARK 0u
#define GUI_SCHEMES 1u
#define GUI_MARKS 8u

// A token's color: the name its grammar gives it, and the color
typedef struct
{
    const char *name;
    GuiColor color;
} GuiTokenColor;

// A scheme's colors, named as style.css names them: the window's ground, surface, well and deep; its text at three
// strengths; its hairline; the signal, link, hover and fail colors; the ink the bar, the strip and the status bar are
// tinted with and the tint itself, the ink at its opacity; the bar's text, its titles' text, the ground of a title
// pointed or open and its text then, a path's text on the bars, and the status bar's text; the editor's ground at
// the editor's opacity, its text, line numbers, current
// line, selection, the selection's ground where the keys are elsewhere, caret, indent guide, the ground of a mark
// among the text, a fold's, the line of a widget over the text and the grounds of find's matches, the current one's
// stronger; the minimap's selection, its match, its slider and the slider dragged; the shown tab's ground and the
// line along it; the ground of the strip's tool whose window shows, the signal at a share; the side pane's ground,
// the ink at half; a diagnostic's error, warning and note colors; the large name's color and the eye's in the bar; the
// shade under a raised thing and the color of TABVLA's cuts; a fuse's ash, burn, fire and the white of its hottest
// core; the lattice's three colors, a point in place the first and a moved one passing through the second to the third;
// the eight colors a file's mark takes; and its tokens, `token_count` of them
typedef struct
{
    GuiColor background_fill;
    GuiColor surface;
    GuiColor well;
    GuiColor deep;
    GuiColor text;
    GuiColor text_2;
    GuiColor text_3;
    GuiColor line;
    GuiColor signal;
    GuiColor link;
    GuiColor hover;
    GuiColor fail;
    GuiColor text_color;
    GuiColor glass;
    GuiColor bar_text;
    GuiColor bar_title;
    GuiColor bar_title_hover;
    GuiColor bar_bright;
    GuiColor bar_path;
    GuiColor status_text;
    GuiColor editor_background;
    GuiColor editor_text;
    GuiColor number;
    GuiColor current;
    GuiColor selection;
    GuiColor selection_idle;
    GuiColor caret;
    GuiColor guide;
    GuiColor same;
    GuiColor widget_line;
    GuiColor match;
    GuiColor match_on;
    GuiColor map_selection;
    GuiColor map_match;
    GuiColor map_slider;
    GuiColor map_slider_on;
    GuiColor tab_background;
    GuiColor tab_line;
    GuiColor strip_on;
    GuiColor side_background;
    GuiColor error;
    GuiColor warning;
    GuiColor note;
    GuiColor open_text;
    GuiColor bar_mark;
    GuiColor lifted;
    GuiColor incised;
    GuiColor fuse_ash;
    GuiColor fuse_burn;
    GuiColor fuse_fire;
    GuiColor fuse_core;
    GuiColor lattice[3];
    GuiColor mark[GUI_MARKS];
    const GuiTokenColor *token;
    unsigned int token_count;
} GuiPalette;

// A palette the window can change: the colors, its tokens its own copies, the most a scheme holds
#define GUI_PALETTE_TOKENS 64u
typedef struct
{
    GuiPalette colors;
    GuiTokenColor token[GUI_PALETTE_TOKENS];
} GuiPaletteOwn;

// the palette of `scheme`, the dark one for any scheme not known
const GuiPalette *gui_palette(const GuiPaletteCfg *args);
// `own` made the palette of `scheme`; and the ink's tint and the editor's ground laid at the bar's and the editor's
// opacity
void gui_palette_own_init(const GuiPaletteCfg *args);
void gui_palette_opacity_layout(const GuiPaletteOwnBarPercentCfg *args);
// the palette's colors by name, its tokens after them: how many; the name of the one `at`, into `size` bytes;
// its color; and it set in `own`, with what depends on it
unsigned int gui_palette_color_count(const GuiPaletteLengthCfg *args);
void gui_palette_color_name(const GuiPaletteAtCfg *args);
GuiColor gui_palette_color_get(const GuiPaletteAtCfg *args);
void gui_palette_color_set(const GuiPaletteOwnCfg *args);
// a color's value in hex, `#rrggbb`, or `#rrggbbaa` where it is not opaque, into `size` bytes; and such a value read,
// with or without its `#`, six digits taking the alpha of `current`: 1, or 0 where it is no color
void gui_palette_value_text(const GuiPaletteSizeColorCfg *args);
int gui_palette_value_read(const GuiPaletteTextCfg *args);
// the least contrast a color of text has with the color under it, as WCAG 2's level AA asks of text, in tenths of a
// ratio
#define GUI_PALETTE_CONTRAST_MIN 45u
// the contrast of `foreground_color` over `back`, in tenths of a ratio from 10 to 210 as WCAG 2 measures it, rounded
// down; `back` laid over `background_fill` and `foreground_color` over that where they are translucent
unsigned int gui_palette_contrast(const GuiPaletteForegroundCfg *args);
// whether the named color `at` is text under the least contrast with a color it is drawn on: 1, with its contrast into
// `tenths` and that color's name into `under`, `size` bytes; 0 for one over the least and for a color that is no text
int gui_palette_color_low(const GuiPaletteAtSizeCfg *args);
// a contrast in tenths as its ratio, `2.8:1`, into `size` bytes
void gui_palette_contrast_text(const GuiPaletteSizeCfg *args);
// the color of the token named by `length` bytes of `name`, the editor's text color where no token has the name
GuiColor gui_palette_token(const GuiPaletteLengthCfg *args);
struct GuiPaletteAtSizeCfg
{
    const GuiPalette *palette;
    unsigned int at;
    unsigned long long size;
    unsigned int *tenths;
    char *under;
};

struct GuiPaletteAtCfg
{
    const GuiPalette *palette;
    unsigned int at;
    char *name;
    unsigned long long size;
};

struct GuiPaletteTextCfg
{
    const char *text_read;
    unsigned long long length;
    GuiColor current;
    GuiColor *color_pointer;
};

struct GuiPaletteForegroundCfg
{
    GuiColor foreground_color;
    GuiColor back;
    GuiColor background_fill;
};

struct GuiPaletteSizeCfg
{
    unsigned long long size;
    char *text;
    unsigned int tenths_value;
};

struct GuiPaletteOwnCfg
{
    GuiPaletteOwn *own;
    unsigned int at;
    GuiColor color;
};

struct GuiPaletteOwnBarPercentCfg
{
    GuiPaletteOwn *own;
    unsigned int bar_percent;
    unsigned int editor_percent;
};

struct GuiPaletteLengthCfg
{
    const GuiPalette *palette;
    unsigned long long length;
    const char *name_read;
};

struct GuiPaletteSizeColorCfg
{
    unsigned long long size;
    GuiColor color;
    char *text;
};

struct GuiPaletteCfg
{
    unsigned int scheme;
    GuiPaletteOwn *own;
};

typedef struct
{
    const GuiPalette *(*palette)(const GuiPaletteCfg *args);
    void (*own_init)(const GuiPaletteCfg *args);
} CliPaletteNs;
EMBED_TABLE_LAYOUT(CliPaletteNs, palette, own_init);
EMBED_TABLE_STORAGE CliPaletteNs cli_palette EMBED_UNUSED = {
    .palette = gui_palette,
    .own_init = gui_palette_own_init,
};

typedef struct
{
    void (*opacity_layout)(const GuiPaletteOwnBarPercentCfg *args);
} GuiPaletteOwnBarPercentNs;
EMBED_TABLE_LAYOUT(GuiPaletteOwnBarPercentNs, opacity_layout);
EMBED_TABLE_STORAGE GuiPaletteOwnBarPercentNs gui_palette_own_bar_percent EMBED_UNUSED = {
    .opacity_layout = gui_palette_opacity_layout,
};

typedef struct
{
    unsigned int (*name_count)(const GuiPaletteLengthCfg *args);
    GuiColor (*token)(const GuiPaletteLengthCfg *args);
} GuiPaletteLengthNs;
EMBED_TABLE_LAYOUT(GuiPaletteLengthNs, name_count, token);
EMBED_TABLE_STORAGE GuiPaletteLengthNs gui_palette_length EMBED_UNUSED = {
    .name_count = gui_palette_color_count,
    .token = gui_palette_token,
};

typedef struct
{
    void (*name_name)(const GuiPaletteAtCfg *args);
    GuiColor (*name_color)(const GuiPaletteAtCfg *args);
} GuiPaletteAtNs;
EMBED_TABLE_LAYOUT(GuiPaletteAtNs, name_name, name_color);
EMBED_TABLE_STORAGE GuiPaletteAtNs gui_palette_at EMBED_UNUSED = {
    .name_name = gui_palette_color_name,
    .name_color = gui_palette_color_get,
};

typedef struct
{
    void (*name_set)(const GuiPaletteOwnCfg *args);
} GuiPaletteOwnNs;
EMBED_TABLE_LAYOUT(GuiPaletteOwnNs, name_set);
EMBED_TABLE_STORAGE GuiPaletteOwnNs gui_palette_own EMBED_UNUSED = {
    .name_set = gui_palette_color_set,
};

typedef struct
{
    void (*value_text)(const GuiPaletteSizeColorCfg *args);
} GuiPaletteSizeColorNs;
EMBED_TABLE_LAYOUT(GuiPaletteSizeColorNs, value_text);
EMBED_TABLE_STORAGE GuiPaletteSizeColorNs gui_palette_size_color EMBED_UNUSED = {
    .value_text = gui_palette_value_text,
};

typedef struct
{
    int (*value_read)(const GuiPaletteTextCfg *args);
} GuiPaletteTextNs;
EMBED_TABLE_LAYOUT(GuiPaletteTextNs, value_read);
EMBED_TABLE_STORAGE GuiPaletteTextNs gui_palette_text EMBED_UNUSED = {
    .value_read = gui_palette_value_read,
};

typedef struct
{
    unsigned int (*contrast)(const GuiPaletteForegroundCfg *args);
} GuiPaletteForegroundNs;
EMBED_TABLE_LAYOUT(GuiPaletteForegroundNs, contrast);
EMBED_TABLE_STORAGE GuiPaletteForegroundNs gui_palette_foreground EMBED_UNUSED = {
    .contrast = gui_palette_contrast,
};

typedef struct
{
    int (*name_low)(const GuiPaletteAtSizeCfg *args);
} GuiPaletteAtSizeNs;
EMBED_TABLE_LAYOUT(GuiPaletteAtSizeNs, name_low);
EMBED_TABLE_STORAGE GuiPaletteAtSizeNs gui_palette_at_size EMBED_UNUSED = {
    .name_low = gui_palette_color_low,
};

typedef struct
{
    void (*contrast_text)(const GuiPaletteSizeCfg *args);
} GuiPaletteSizeNs;
EMBED_TABLE_LAYOUT(GuiPaletteSizeNs, contrast_text);
EMBED_TABLE_STORAGE GuiPaletteSizeNs gui_palette_size EMBED_UNUSED = {
    .contrast_text = gui_palette_contrast_text,
};

#endif

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// font.h: a TrueType font read and drawn by orior_cli itself (font.c reads it, font_outline.c outlines and draws a
// glyph): its tables read from a .ttf or the first font of a .ttc, a character found by its cmap, a glyph's outline
// read from glyf, simple or made of other glyphs, its quadratic curves cut into short lines, and the lines drawn as
// coverage, a pixel's share of the glyph, by the signed area each line leaves in the cells it crosses
#ifndef PRINT_FONT_H
#define PRINT_FONT_H
#include "embed_dispatch_layout.h"
#include "print.h"

typedef struct CliFontGlyphMatrixCfg CliFontGlyphMatrixCfg;
typedef struct CliFontGlyphCfg CliFontGlyphCfg;
typedef struct CliFontContoursCfg CliFontContoursCfg;
typedef struct CliFontDataCfg CliFontDataCfg;
typedef struct CliFontFamiliesCfg CliFontFamiliesCfg;
typedef struct CliFontCfg CliFontCfg;
typedef struct CliFontAtCfg CliFontAtCfg;

// the most bytes of a font file, tables of a font, character ranges of its cmap, points of a glyph's outline and
// contours of it
#define CLI_FONT_BYTES (1u << 23u)

#define CLI_FONT_TABLES 128u

#define CLI_FONT_RANGES (1u << 17u)

#define CLI_OUTLINE_POINTS (1u << 15u)

#define CLI_OUTLINE_CONTOURS (1u << 11u)

// A font's tables, as read: its bytes, each table's tag and place, its units, ascent and descent, the form of its
// loca, its count of glyphs and of metrics, and its character map as ranges, each its first and last character and the
// glyph the first is drawn by
typedef struct
{
    const unsigned char *data;
    unsigned long long size;
    unsigned char tag[CLI_FONT_TABLES][4];
    unsigned long long table[CLI_FONT_TABLES];
    unsigned int tables;
    float units;
    float ascent;
    float descent;
    int long_loca;
    unsigned int glyphs;
    unsigned int metrics;
    unsigned int (*range)[3];
    unsigned int ranges;
} CliFont;

// A glyph's outline: closed contours of points in pixels, each contour's last point's place after the one before's
typedef struct
{
    float x[CLI_OUTLINE_POINTS];
    float y[CLI_OUTLINE_POINTS];
    unsigned int points;
    unsigned int end[CLI_OUTLINE_CONTOURS];
    unsigned int contours;
} CliContours;

// A glyph drawn as coverage: its width and height in pixels, where its top left stands from the pen on the baseline,
// and each pixel's share of it, from 0 to 1
typedef struct
{
    unsigned long long width;
    unsigned long long height;
    int left;
    int top;
    float *cover;
} CliMask;

// the font in `size` bytes of `data`, a TrueType font or the first of a collection, its ranges stored in `ranges`,
// which sized for CLI_FONT_RANGES: 1, or 0 where it reads as none
int cli_font_read(const CliFontDataCfg *args);
// the glyph that draws `letter`, 0 where the font has none; how far a glyph moves the pen, in font units
unsigned int cli_font_glyph(const CliFontCfg *args);
float cli_font_advance(const CliFontGlyphCfg *args);
// where a glyph's outline stands in glyf, `from` and `to`: 1, or 0 where it has none
int cli_font_glyph_range(const CliFontGlyphCfg *args);
unsigned int cli_font_u16(const CliFontAtCfg *args);
int cli_font_i16(const CliFontAtCfg *args);
// the outline of a glyph put after `out`'s contours, moved by `matrix`: [a, b, c, d, e, f] for x' = a x + c y + e,
// y' = b x + d y + f (font_outline.c)
void cli_font_outline(const CliFontGlyphMatrixCfg *args);
// `contours`, in pixels with y going up from the baseline, drawn as coverage into `mask`, its cover in `cover`, which
// sized for `capacity`; a mask too large for the capacity drawn as none; and with `cover` NULL the mask measured alone,
// its size and place set and nothing drawn, so a cover of `width` by `height` and four more can be taken for it
void cli_font_rasterize(const CliFontContoursCfg *args);
// the first family of `families`, `count` of them, whose regular face the system's font folders hold, each family its
// files regular, bold, italic and bold italic, an empty name for a face it lacks: each face's path into `paths`, which
// is sized for four of CLI_PATH_MAX_LEN, empty where the family or the face is not found; and the monospace fonts
// orior_cli draws with, the first found of those the editor uses and those each system has
void cli_font_find(const CliFontFamiliesCfg *args);
void cli_font_monospace(const CliFontFamiliesCfg *args);

struct CliFontGlyphMatrixCfg
{
    const CliFont *font;
    unsigned int glyph;
    const float *matrix;
    unsigned int depth;
    CliContours *out;
};

struct CliFontGlyphCfg
{
    const CliFont *font;
    unsigned int glyph;
    unsigned long long *from;
    unsigned long long *to;
};

struct CliFontContoursCfg
{
    const CliContours *contours;
    CliMask *mask_bits;
    float *cover;
    unsigned long long capacity;
};

struct CliFontDataCfg
{
    CliFont *font_out;
    const unsigned char *data;
    unsigned long long size;
    unsigned int (*ranges)[3];
};

struct CliFontFamiliesCfg
{
    const char *const (*families)[4];
    unsigned int count;
    char (*paths)[CLI_PATH_MAX_LEN];
};

struct CliFontCfg
{
    const CliFont *font;
    unsigned int letter;
};

struct CliFontAtCfg
{
    const CliFont *font;
    unsigned long long at;
};

typedef struct
{
    int (*read)(const CliFontDataCfg *args);
} CliFontDataNs;
EMBED_TABLE_LAYOUT(CliFontDataNs, read);
EMBED_TABLE_STORAGE CliFontDataNs cli_font_data EMBED_UNUSED = {
    .read = cli_font_read,
};

typedef struct
{
    unsigned int (*glyph)(const CliFontCfg *args);
} CliFontNs;
EMBED_TABLE_LAYOUT(CliFontNs, glyph);
EMBED_TABLE_STORAGE CliFontNs cli_font EMBED_UNUSED = {
    .glyph = cli_font_glyph,
};

typedef struct
{
    float (*advance)(const CliFontGlyphCfg *args);
    int (*glyph_range)(const CliFontGlyphCfg *args);
} CliFontGlyphNs;
EMBED_TABLE_LAYOUT(CliFontGlyphNs, advance, glyph_range);
EMBED_TABLE_STORAGE CliFontGlyphNs cli_font_glyph_table EMBED_UNUSED = {
    .advance = cli_font_advance,
    .glyph_range = cli_font_glyph_range,
};

typedef struct
{
    unsigned int (*u16)(const CliFontAtCfg *args);
    int (*i16)(const CliFontAtCfg *args);
} CliFontAtNs;
EMBED_TABLE_LAYOUT(CliFontAtNs, u16, i16);
EMBED_TABLE_STORAGE CliFontAtNs cli_font_at EMBED_UNUSED = {
    .u16 = cli_font_u16,
    .i16 = cli_font_i16,
};

typedef struct
{
    void (*outline)(const CliFontGlyphMatrixCfg *args);
} CliFontGlyphMatrixNs;
EMBED_TABLE_LAYOUT(CliFontGlyphMatrixNs, outline);
EMBED_TABLE_STORAGE CliFontGlyphMatrixNs cli_font_glyph_matrix EMBED_UNUSED = {
    .outline = cli_font_outline,
};

typedef struct
{
    void (*rasterize)(const CliFontContoursCfg *args);
} CliFontContoursNs;
EMBED_TABLE_LAYOUT(CliFontContoursNs, rasterize);
EMBED_TABLE_STORAGE CliFontContoursNs cli_font_contours EMBED_UNUSED = {
    .rasterize = cli_font_rasterize,
};

typedef struct
{
    void (*find)(const CliFontFamiliesCfg *args);
    void (*monospace)(const CliFontFamiliesCfg *args);
} CliFontFamiliesNs;
EMBED_TABLE_LAYOUT(CliFontFamiliesNs, find, monospace);
EMBED_TABLE_STORAGE CliFontFamiliesNs cli_font_families EMBED_UNUSED = {
    .find = cli_font_find,
    .monospace = cli_font_monospace,
};

#endif

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// font.h: a TrueType font read and drawn (font.c reads it, font_outline.c outlines and draws a glyph): its tables
// read from a .ttf or the first font of a .ttc, a character found by its cmap, a glyph's outline read from glyf,
// simple or made of other glyphs, its quadratic curves cut into short lines, and the lines passed to the one
// rasterizer (../coverage/coverage.h) for each pixel's share of the glyph. The font's bytes and the ranges of its
// character map are the caller's: a font is read over the file the caller holds
#ifndef FONT_H
#define FONT_H
#include "embed_dispatch_layout.h"

typedef struct FontGlyphMatrixCfg FontGlyphMatrixCfg;
typedef struct FontGlyphCfg FontGlyphCfg;
typedef struct FontContoursCfg FontContoursCfg;
typedef struct FontDataCfg FontDataCfg;
typedef struct FontCfg FontCfg;
typedef struct FontAtCfg FontAtCfg;

// the max bytes of a font file, tables of a font, character ranges of its cmap, points of a glyph's outline and
// contours of it
#define FONT_BYTES (1u << 23u)

#define FONT_TABLES 128u

#define FONT_RANGES (1u << 17u)

#define FONT_OUTLINE_POINTS (1u << 15u)

#define FONT_OUTLINE_CONTOURS (1u << 11u)

// The tables of a font, as read: its bytes, the tag and place of each table, and its units, ascent and descent. Then
// the form of its loca, and its count of glyphs and of metrics.
// Then its character map as ranges. Each range holds its first and last character, and the glyph that draws the first
typedef struct
{
    const unsigned char *data;
    unsigned long long size;
    unsigned char tag[FONT_TABLES][4];
    unsigned long long table[FONT_TABLES];
    unsigned int tables;
    float units;
    float ascent;
    float descent;
    int long_loca;
    unsigned int glyphs;
    unsigned int metrics;
    unsigned int (*range)[3];
    unsigned int ranges;
} Font;

// A glyph's outline: closed contours of points in pixels, each contour's last point's place after the one before's
typedef struct
{
    float x[FONT_OUTLINE_POINTS];
    float y[FONT_OUTLINE_POINTS];
    unsigned int points;
    unsigned int end[FONT_OUTLINE_CONTOURS];
    unsigned int contours;
} FontContours;

// A glyph drawn as coverage: its width and height in pixels, and where its top left is from the pen on the baseline.
// Then the share of it in each pixel, from 0 to 1
typedef struct
{
    unsigned long long width;
    unsigned long long height;
    int left;
    int top;
    float *cover;
} FontMask;

// the font in `size` bytes of `data`, a TrueType font or the first of a collection. Its ranges go in `ranges`, sized
// for FONT_RANGES. The result is 1, or 0 where it reads as none
int font_read(const FontDataCfg *args);
// the glyph that draws `character`, 0 where the font has none; how far a glyph moves the pen, in font units
unsigned int font_glyph(const FontCfg *args);
float font_advance(const FontGlyphCfg *args);
// where a glyph's outline is in glyf, `from` and `to`: 1, or 0 where it has none
int font_glyph_range(const FontGlyphCfg *args);
unsigned int font_u16(const FontAtCfg *args);
int font_i16(const FontAtCfg *args);
// the outline of a glyph, put after the contours of `out`, and moved by `matrix`. The matrix is [a, b, c, d, e, f],
// for x' = a x + c y + e, y' = b x + d y + f (font_outline.c)
void font_outline(const FontGlyphMatrixCfg *args);
// `contours`, in pixels with y up from the baseline, drawn as coverage into `mask`. Its cover goes in `cover`, sized
// for `capacity`. Its edges go in `scratch` of `scratch_bytes`, as `font_rasterize_bytes` counts them for the contours.
// A mask too large for the capacity draws as none.
//
// With `cover` NULL, the call only measures the mask. It sets its size and place, and draws nothing. The caller can
// then take a cover of `width` by `height` and four more for it
void font_rasterize(const FontContoursCfg *args);
unsigned long long font_rasterize_bytes(const FontContoursCfg *args);

struct FontGlyphMatrixCfg
{
    const Font *font;
    unsigned int glyph;
    const float *matrix;
    unsigned int depth;
    FontContours *out;
};

struct FontGlyphCfg
{
    const Font *font;
    unsigned int glyph;
    unsigned long long *from;
    unsigned long long *to;
};

struct FontContoursCfg
{
    const FontContours *contours;
    FontMask *mask_bits;
    float *cover;
    unsigned long long capacity;
    void *scratch;
    unsigned long long scratch_bytes;
};

struct FontDataCfg
{
    Font *font_out;
    const unsigned char *data;
    unsigned long long size;
    unsigned int (*ranges)[3];
};

struct FontCfg
{
    const Font *font;
    unsigned int character;
};

struct FontAtCfg
{
    const Font *font;
    unsigned long long at;
};

typedef struct
{
    int (*read)(const FontDataCfg *args);
} FontDataNs;
EMBED_TABLE_LAYOUT(FontDataNs, read);
EMBED_TABLE_STORAGE FontDataNs font_data EMBED_UNUSED = {
    .read = font_read,
};

typedef struct
{
    unsigned int (*glyph)(const FontCfg *args);
} FontLetterNs;
EMBED_TABLE_LAYOUT(FontLetterNs, glyph);
EMBED_TABLE_STORAGE FontLetterNs font_character EMBED_UNUSED = {
    .glyph = font_glyph,
};

typedef struct
{
    float (*advance)(const FontGlyphCfg *args);
    int (*glyph_range)(const FontGlyphCfg *args);
} FontGlyphNs;
EMBED_TABLE_LAYOUT(FontGlyphNs, advance, glyph_range);
EMBED_TABLE_STORAGE FontGlyphNs font_glyph_table EMBED_UNUSED = {
    .advance = font_advance,
    .glyph_range = font_glyph_range,
};

typedef struct
{
    unsigned int (*u16)(const FontAtCfg *args);
    int (*i16)(const FontAtCfg *args);
} FontAtNs;
EMBED_TABLE_LAYOUT(FontAtNs, u16, i16);
EMBED_TABLE_STORAGE FontAtNs font_at EMBED_UNUSED = {
    .u16 = font_u16,
    .i16 = font_i16,
};

typedef struct
{
    void (*outline)(const FontGlyphMatrixCfg *args);
} FontGlyphMatrixNs;
EMBED_TABLE_LAYOUT(FontGlyphMatrixNs, outline);
EMBED_TABLE_STORAGE FontGlyphMatrixNs font_glyph_matrix EMBED_UNUSED = {
    .outline = font_outline,
};

typedef struct
{
    void (*rasterize)(const FontContoursCfg *args);
    unsigned long long (*bytes)(const FontContoursCfg *args);
} FontContoursNs;
EMBED_TABLE_LAYOUT(FontContoursNs, rasterize, bytes);
EMBED_TABLE_STORAGE FontContoursNs font_contours EMBED_UNUSED = {
    .rasterize = font_rasterize,
    .bytes = font_rasterize_bytes,
};

#endif

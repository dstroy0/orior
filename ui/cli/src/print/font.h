// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// font.h: a TrueType font read and drawn by orior_cli itself (font.c reads it, font_outline.c outlines and draws a glyph):
// its tables read from a .ttf or the first font of a .ttc, a character found by its cmap, a glyph's outline read from
// glyf, simple or made of other glyphs, its quadratic curves cut into short lines, and the lines drawn as coverage, a
// pixel's share of the glyph, by the signed area each line leaves in the cells it crosses
#ifndef PRINT_FONT_H
#define PRINT_FONT_H

#include "print.h"

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

// the font in `size` bytes of `data`, a TrueType font or the first of a collection, its ranges kept in `ranges`, which
// holds CLI_FONT_RANGES: 1, or 0 where it reads as none
int cli_font_read(CliFont *font, const unsigned char *data, unsigned long long size, unsigned int (*ranges)[3]);
// the glyph that draws `letter`, 0 where the font has none; how far a glyph moves the pen, in font units
unsigned int cli_font_glyph(const CliFont *font, unsigned int letter);
float cli_font_advance(const CliFont *font, unsigned int glyph);
// where a glyph's outline stands in glyf, `from` and `to`: 1, or 0 where it has none
int cli_font_glyph_range(const CliFont *font, unsigned int glyph, unsigned long long *from, unsigned long long *to);
unsigned int cli_font_u16(const CliFont *font, unsigned long long at);
int cli_font_i16(const CliFont *font, unsigned long long at);
// the outline of a glyph put after `out`'s contours, moved by `matrix`: [a, b, c, d, e, f] for x' = a x + c y + e,
// y' = b x + d y + f (font_outline.c)
void cli_font_outline(const CliFont *font, unsigned int glyph, const float *matrix, unsigned int depth,
                      CliContours *out);
// `contours`, in pixels with y going up from the baseline, drawn as coverage into `mask`, its cover in `cover`, which
// holds `room`; a mask too large for the room drawn as none
void cli_font_rasterize(const CliContours *contours, CliMask *mask, float *cover, unsigned long long room);
// the monospace fonts orior_cli draws with, regular, bold, italic and bold italic, the first found of those the editor
// uses and those each system has, each path into `paths`, each holding CLI_PATH_LONGEST, empty where it has none
void cli_font_monospace(char (*paths)[CLI_PATH_LONGEST]);

#endif

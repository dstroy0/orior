// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pdf.c: the pages laid out as a PDF of orior's own writing: PDF 1.4, its text in the Courier faces every PDF reader
// holds, regular, bold, oblique and bold oblique, encoded as WinAnsi, a letter that has no place there written as a
// question mark. Only the pages the layout names are written
#include "../text/regexp.h"
#include "print.h"

#include <math.h>

// the most bytes of one page's content, and the most objects a PDF holds
#define CLI_PDF_CONTENT (1u << 22u)

#define CLI_PDF_OBJECTS (7u + (2u * CLI_PRINT_PAGES))

// the color of the line numbers
static const unsigned char s_number_color[3] = {0x88u, 0x88u, 0x88u};

// The letters past Latin-1's printing ones WinAnsi writes from 0x80 to 0x9F, each its byte
static const unsigned int s_win_ansi[27][2] = {
    {0x20acu, 0x80u}, {0x201au, 0x82u}, {0x0192u, 0x83u}, {0x201eu, 0x84u}, {0x2026u, 0x85u}, {0x2020u, 0x86u},
    {0x2021u, 0x87u}, {0x02c6u, 0x88u}, {0x2030u, 0x89u}, {0x0160u, 0x8au}, {0x2039u, 0x8bu}, {0x0152u, 0x8cu},
    {0x017du, 0x8eu}, {0x2018u, 0x91u}, {0x2019u, 0x92u}, {0x201cu, 0x93u}, {0x201du, 0x94u}, {0x2022u, 0x95u},
    {0x2013u, 0x96u}, {0x2014u, 0x97u}, {0x02dcu, 0x98u}, {0x2122u, 0x99u}, {0x0161u, 0x9au}, {0x203au, 0x9bu},
    {0x0153u, 0x9cu}, {0x017eu, 0x9eu}, {0x0178u, 0x9fu}};

// A letter as WinAnsi writes it, '?' where it has no place there
static unsigned char cli_win_ansi(unsigned int letter)
{
    if (((letter >= 0x20u) && (letter <= 0x7eu)) || ((letter >= 0xa0u) && (letter <= 0xffu)))
    {
        // a letter of Latin-1 is its one byte
        return (unsigned char)letter;
    }
    for (unsigned int at = 0u; at < 27u; at += 1u)
    {
        if (s_win_ansi[at][0] == letter)
        {
            // each of the table's bytes is below 0x100
            return (unsigned char)s_win_ansi[at][1];
        }
    }
    return '?';
}

// `length` bytes of `text` as a PDF string of WinAnsi bytes, its brackets and backslashes escaped
static void cli_string(CliLine *out, const char *text, unsigned long long length)
{
    static unsigned int s_letters[CLI_PATH_LONGEST * 4u];
    cli_line_character(out, '(');
    unsigned long long done = 0ull;
    while (done < length)
    {
        // the text read a piece at a time, each piece cut where a letter starts
        unsigned long long piece = length - done;
        piece = (piece < CLI_PATH_LONGEST) ? piece : CLI_PATH_LONGEST;
        while (((done + piece) < length) && ((((unsigned char)text[done + piece]) & 0xc0u) == 0x80u))
        {
            piece -= 1ull;
        }
        const unsigned long long count = cli_regexp_letters(text + done, piece, s_letters, CLI_PATH_LONGEST * 4u);
        for (unsigned long long at = 0ull; at < count; at += 1ull)
        {
            const unsigned char byte = cli_win_ansi(s_letters[at]);
            if ((byte == '(') || (byte == ')') || (byte == '\\'))
            {
                cli_line_character(out, '\\');
            }
            // a byte of WinAnsi written as the char it is
            cli_line_character(out, (char)byte);
        }
        done += piece;
    }
    cli_line_character(out, ')');
}

// a hex digit in upper case
static char cli_upper_hex(unsigned int value)
{
    static const char s_digits[17] = "0123456789ABCDEF";
    return s_digits[value & 0x0fu];
}

// `text` as a PDF text string in UTF-16, for the document's title
static void cli_text_string(CliLine *out, const char *text)
{
    static unsigned int s_letters[CLI_PATH_LONGEST];
    cli_line_text(out, "<FEFF");
    const unsigned long long count = cli_regexp_letters(
        text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), s_letters, CLI_PATH_LONGEST);
    for (unsigned long long at = 0ull; at < count; at += 1ull)
    {
        const unsigned int letter = s_letters[at];
        unsigned int units[2] = {letter, 0u};
        unsigned int unit_count = 1u;
        if (letter >= 0x10000u)
        {
            units[0] = 0xd800u + ((letter - 0x10000u) >> 10u);
            units[1] = 0xdc00u + ((letter - 0x10000u) & 0x3ffu);
            unit_count = 2u;
        }
        for (unsigned int unit = 0u; unit < unit_count; unit += 1u)
        {
            for (unsigned int shift = 16u; shift > 0u; shift -= 4u)
            {
                cli_line_character(out, cli_upper_hex(units[unit] >> (shift - 4u)));
            }
        }
    }
    cli_line_character(out, '>');
}

// a number written with `places` digits after the point, rounded
static void cli_fixed(CliLine *out, float value, unsigned int places)
{
    const double scale = (places == 3u) ? 1000.0 : 100.0;
    const long long whole = llround((double)value * scale);
    // the value's size, never negative
    const unsigned long long size = (unsigned long long)((whole < 0ll) ? -whole : whole);
    // a scale of 100 or 1000, which an unsigned long long holds whole
    const unsigned long long divisor = (unsigned long long)scale;
    if (whole < 0ll)
    {
        cli_line_character(out, '-');
    }
    cli_line_decimal(out, size / divisor, 1u);
    cli_line_character(out, '.');
    cli_line_decimal(out, size % divisor, places);
}

static void cli_shade(CliLine *out, const unsigned char *rgb)
{
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        cli_fixed(out, (float)rgb[at] / 255.0f, 3u);
        cli_line_character(out, ' ');
    }
    cli_line_text(out, "rg");
}

// one row's number or piece written at `x` on its baseline
static void cli_shown(CliLine *out, const char *face, float x, float baseline, const unsigned char *rgb,
                      const char *text, unsigned long long length)
{
    // the face at the text's size, 9.5 points
    cli_line_character(out, '/');
    cli_line_text(out, face);
    cli_line_text(out, " 9.5 Tf 1 0 0 1 ");
    cli_fixed(out, x, 2u);
    cli_line_character(out, ' ');
    cli_fixed(out, baseline, 2u);
    cli_line_text(out, " Tm ");
    cli_shade(out, rgb);
    cli_line_character(out, ' ');
    cli_string(out, text, length);
    cli_line_text(out, " Tj\n");
}

// The content of one page: each row's number and pieces at their columns
static void cli_content(const CliPrintLaid *laid, unsigned int page, float height, int gray, CliLine *out)
{
    unsigned char number_tint[3];
    if (gray)
    {
        cli_print_color("#888888", 7u, 1, number_tint);
    }
    else
    {
        memor.cpy(&(MemoriaCfg){.dst = number_tint, .src = s_number_color, .bytes = 3u});
    }
    cli_line_text(out, "BT\n");
    const unsigned int first = page * laid->rows_per_page;
    const unsigned int last = ((first + laid->rows_per_page) < laid->rows) ? (first + laid->rows_per_page) : laid->rows;
    for (unsigned int index = first; index < last; index += 1u)
    {
        const CliPrintRow *const row = &laid->row[index];
        const float baseline =
            height - CLI_PRINT_MARGIN - ((float)(index - first) * CLI_PRINT_LEADING) - (CLI_PRINT_LEADING * 0.75f);
        if (row->numbered)
        {
            char number[32];
            CliLine line = {number, sizeof(number), 0ull};
            const unsigned long long width = (laid->number_columns > 2ull) ? (laid->number_columns - 2ull) : 0ull;
            cli_line_decimal_columns(&line, row->number, (unsigned int)width);
            const unsigned long long length = cli_line_closed(&line);
            cli_shown(out, "F1", CLI_PRINT_MARGIN, baseline, number_tint, number, length);
        }
        for (unsigned int at = 0u; at < row->pieces; at += 1u)
        {
            const CliPrintPiece *const piece = &laid->piece[row->first + at];
            static const char *const s_faces[4] = {"F1", "F2", "F3", "F4"};
            const float x = CLI_PRINT_MARGIN + ((float)(laid->number_columns + piece->column) * CLI_PRINT_ADVANCE);
            cli_shown(out, s_faces[(piece->bold ? 1u : 0u) | (piece->italic ? 2u : 0u)], x, baseline, piece->color,
                      laid->text + piece->text, piece->length);
        }
    }
    cli_line_text(out, "ET\n");
}

// the object `number` begun at the out's place, kept among the offsets
static void cli_object(CliLine *out, unsigned long long *offsets, unsigned int number)
{
    offsets[number - 1u] = out->at;
    cli_line_decimal(out, number, 1u);
    cli_line_text(out, " 0 obj\n");
}

void cli_print_pdf(const CliPrintListing *listing, const CliPrintLayout *layout, CliLine *out,
                   unsigned long long *pages)
{
    static unsigned int s_printed[CLI_PRINT_PAGES];
    static unsigned long long s_offsets[CLI_PDF_OBJECTS];
    static char s_content[CLI_PDF_CONTENT];
    static const char *const s_faces[4] = {"Courier", "Courier-Bold", "Courier-Oblique", "Courier-BoldOblique"};
    const CliPrintLaid *const laid = cli_print_lay_out(listing, layout);
    float width = 0.0f;
    float height = 0.0f;
    cli_print_size(layout, &width, &height);
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < laid->pages; at += 1u)
    {
        if (cli_print_prints(layout, at + 1ull) && (count < CLI_PRINT_PAGES))
        {
            s_printed[count] = at;
            count += 1u;
        }
    }
    // 1 the catalog, 2 the pages, 3 to 6 the faces, 7 the document's information; then each page and its contents,
    // two objects a page
    const unsigned int first_page = 8u;
    const unsigned int objects = 7u + (2u * count);
    cli_line_text(out, "%PDF-1.4\n%\xe2\xe3\xcf\xd3\n");
    cli_object(out, s_offsets, 1u);
    cli_line_text(out, "<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");
    cli_object(out, s_offsets, 2u);
    cli_line_text(out, "<< /Type /Pages /Kids [");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(out, (at != 0u) ? " " : "");
        cli_line_decimal(out, first_page + (at * 2u), 1u);
        cli_line_text(out, " 0 R");
    }
    cli_line_text(out, "] /Count ");
    cli_line_decimal(out, count, 1u);
    cli_line_text(out, " >>\nendobj\n");
    for (unsigned int face = 0u; face < 4u; face += 1u)
    {
        cli_object(out, s_offsets, 3u + face);
        cli_line_text(out, "<< /Type /Font /Subtype /Type1 /BaseFont /");
        cli_line_text(out, s_faces[face]);
        cli_line_text(out, " /Encoding /WinAnsiEncoding >>\nendobj\n");
    }
    cli_object(out, s_offsets, 7u);
    cli_line_text(out, "<< /Title ");
    cli_text_string(out, listing->title);
    cli_line_text(out, " /Producer (orior) >>\nendobj\n");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned int page = first_page + (at * 2u);
        cli_object(out, s_offsets, page);
        cli_line_text(out, "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ");
        cli_fixed(out, width, 2u);
        cli_line_character(out, ' ');
        cli_fixed(out, height, 2u);
        cli_line_text(out, "] /Resources << /Font << /F1 3 0 R /F2 4 0 R /F3 5 0 R /F4 6 0 R >> >> /Contents ");
        cli_line_decimal(out, page + 1u, 1u);
        cli_line_text(out, " 0 R >>\nendobj\n");
        CliLine content = {s_content, sizeof(s_content), 0ull};
        cli_content(laid, s_printed[at], height, layout->gray, &content);
        const unsigned long long length = cli_line_fits(&content) ? content.at : 0ull;
        cli_object(out, s_offsets, page + 1u);
        cli_line_text(out, "<< /Length ");
        cli_line_decimal(out, length, 1u);
        cli_line_text(out, " >>\nstream\n");
        cli_line_counted(out, s_content, length);
        cli_line_text(out, "endstream\nendobj\n");
    }
    const unsigned long long xref = out->at;
    cli_line_text(out, "xref\n0 ");
    cli_line_decimal(out, objects + 1u, 1u);
    cli_line_text(out, "\n0000000000 65535 f \n");
    for (unsigned int at = 0u; at < objects; at += 1u)
    {
        cli_line_decimal(out, s_offsets[at], 10u);
        cli_line_text(out, " 00000 n \n");
    }
    cli_line_text(out, "trailer\n<< /Size ");
    cli_line_decimal(out, objects + 1u, 1u);
    cli_line_text(out, " /Root 1 0 R /Info 7 0 R >>\nstartxref\n");
    cli_line_decimal(out, xref, 1u);
    cli_line_text(out, "\n%%EOF\n");
    *pages = count;
}

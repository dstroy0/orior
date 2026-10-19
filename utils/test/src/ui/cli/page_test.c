// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// page_test.c: the laying out's check: a long line goes on under itself and pages break by rows; and pages, ranges and
// gray read as written
#include "../../../../../ui/cli/src/print/print.h"
#include "cli_check.h"

// the listing of `count` lines, line `at` the text `lines(at)`, read into the one listing
static const CliPrintListing *page_listing(unsigned int count, const char *(*lines)(unsigned int at))
{
    static char s_text[1u << 16u];
    CliLine text = {s_text, sizeof(s_text), 0ull};
    cli_line_text(&text, "{\"title\":\"t\",\"lines\":[");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(&text, (at > 0u) ? ",{\"number\":" : "{\"number\":");
        cli_line_decimal(&text, at + 1u, 1u);
        cli_line_text(&text, ",\"runs\":[{\"text\":");
        cli_json_quoted(&text, lines(at), cellul.len(&(CatenaFinitaCfg){.src = lines(at), .cap = CLI_CELLS_POOL}));
        cli_line_text(&text, ",\"color\":\"#112233\"}]}");
    }
    cli_line_text(&text, "],\"tab\":4}");
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_text, text.at, &read);
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    CliPrintListing *listing = NULL;
    return cli_print_listing_read(json, read, &listing, &said) ? listing : NULL;
}

// the three lines of the first listing
static const char *page_three(unsigned int at)
{
    static char s_long[201];
    memor.set(&(MemoriaCfg){.dst = s_long, .val = 'x', .bytes = 200u});
    s_long[200] = '\0';
    return (at == 0u) ? "short" : ((at == 1u) ? s_long : "\tafter");
}

// the numbers of the second
static const char *page_numbers(unsigned int at)
{
    static char s_number[16];
    CliLine line = {s_number, sizeof(s_number), 0ull};
    cli_line_decimal(&line, at, 1u);
    (void)cli_line_closed(&line);
    return s_number;
}

// a layout on A4, upright or on its side, in color, every page
static CliPrintLayout page_layout(int landscape)
{
    CliPrintLayout layout;
    memor.set(&(MemoriaCfg){.dst = &layout, .val = 0, .bytes = sizeof(layout)});
    memor.cpy(&(MemoriaCfg){.dst = layout.paper,
                            .src = "a4",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "a4", .cap = CLI_CELLS_POOL}) + 1u});
    layout.landscape = landscape;
    layout.copies = 1u;
    return layout;
}

int main(void)
{
    cli_print_take();
    const CliPrintLayout upright = page_layout(0);
    const CliPrintLaid *laid = cli_print_lay_out(page_listing(3u, page_three), &upright);
    cli_check((laid->rows >= 4u) && laid->row[0].numbered && (laid->row[0].number == 1ull) && laid->row[1].numbered &&
                  (laid->row[1].number == 2ull) && !laid->row[2].numbered,
              "the long line's second row has no number");
    unsigned long long first_width = 0ull;
    for (unsigned int piece = 0u; piece < laid->row[1].pieces; piece += 1u)
    {
        first_width += laid->piece[laid->row[1].first + piece].length;
    }
    cli_check((first_width < 200ull) && (first_width > 60ull), "a long line's first row is cut to the page");
    const CliPrintRow *const last = &laid->row[laid->rows - 1u];
    const CliPrintPiece *const after = &laid->piece[last->first];
    cli_check((after->length == 9u) &&
                  (memor.cmp(&(MemoriaCfg){.src = laid->text + after->text, .other = "    after", .bytes = 9u}) == 0),
              "a tab is as wide as four spaces");
    laid = cli_print_lay_out(page_listing(200u, page_numbers), &upright);
    const unsigned int pages = laid->pages;
    cli_check(pages >= 3u, "two hundred lines take three pages or more");
    const CliPrintLayout side = page_layout(1);
    laid = cli_print_lay_out(page_listing(200u, page_numbers), &side);
    cli_check(laid->pages > pages, "on its side a page holds fewer rows");
    CliPrintLayout ranged = page_layout(0);
    memor.cpy(&(MemoriaCfg){.dst = ranged.pages,
                            .src = "1-3, 5",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "1-3, 5", .cap = CLI_CELLS_POOL}) + 1u});
    ranged.has_pages = 1;
    cli_check(cli_print_prints(&ranged, 2ull) && cli_print_prints(&ranged, 5ull) && !cli_print_prints(&ranged, 4ull),
              "the pages named print, and no other");
    int low[4];
    int high[4];
    const unsigned int ranges = cli_print_ranges(&ranged, low, high, 4u);
    cli_check((ranges == 2u) && (low[0] == 1) && (high[0] == 3) && (low[1] == 5) && (high[1] == 5),
              "the ranges read as written");
    unsigned char rgb[3];
    cli_print_color("#ff0000", 7ull, 1, rgb);
    cli_check((rgb[0] == 54u) && (rgb[1] == 54u) && (rgb[2] == 54u), "a color made gray by its lightness");
    cli_print_color("#0a0b0c", 7ull, 0, rgb);
    cli_check((rgb[0] == 10u) && (rgb[1] == 11u) && (rgb[2] == 12u), "a color read as written");
    cli_print_give();
    return cli_checked();
}

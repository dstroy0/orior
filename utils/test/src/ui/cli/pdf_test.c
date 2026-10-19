// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pdf_test.c: the PDF's check: a listing is a PDF whose cross-reference finds each object, and a layout that names
// one page prints one
#include "../../../../../ui/cli/src/print/print.h"
#include "cli_check.h"

#include "../../../../../ui/cli/src/text/text.h"

static char s_pdf[1u << 22u];

// the listing of 120 numbered lines, each a run of text, read; and the layout `layout` read: 1, or 0
static int pdf_read(const char *layout_text, const CliPrintListing **listing, CliPrintLayout *layout)
{
    static char s_listing[1u << 16u];
    CliLine text = {s_listing, sizeof(s_listing), 0ull};
    cli_line_text(&text, "{\"title\":\"main.py \xe2\x80\x94 orior\",\"lines\":[");
    for (unsigned int number = 1u; number <= 120u; number += 1u)
    {
        cli_line_text(&text, (number > 1u) ? ",{\"number\":" : "{\"number\":");
        cli_line_decimal(&text, number, 1u);
        cli_line_text(&text, ",\"runs\":[{\"text\":\"print(");
        cli_line_decimal(&text, number, 1u);
        cli_line_text(&text, ") # (\xc3\xa9) \\\\ \xe2\x98\x83\",\"color\":\"#7a5ee0\",\"bold\":");
        cli_line_text(&text, ((number % 2u) == 0u) ? "true" : "false");
        cli_line_text(&text, ",\"italic\":");
        cli_line_text(&text, ((number % 3u) == 0u) ? "true}]}" : "false}]}");
    }
    cli_line_text(&text, "],\"tab\":4}");
    unsigned int read = CLI_JSON_NONE;
    const CliJson *json = cli_check_parsed(s_listing, text.at, &read);
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    CliPrintListing *held = NULL;
    if (!cli_print_listing_read(json, read, &held, &said))
    {
        return 0;
    }
    *listing = held;
    json =
        cli_check_parsed(layout_text, cellul.len(&(CatenaFinitaCfg){.src = layout_text, .cap = CLI_CELLS_POOL}), &read);
    cli_print_layout_read(json, read, layout);
    return 1;
}

int main(void)
{
    static CliPrintLayout s_layout;
    const CliPrintListing *listing = NULL;
    cli_print_take();
    cli_check(pdf_read("{}", &listing, &s_layout), "the listing reads");
    CliLine pdf = {s_pdf, sizeof(s_pdf) - 1u, 0ull};
    unsigned long long pages = 0ull;
    cli_print_pdf(listing, &s_layout, &pdf, &pages);
    s_pdf[pdf.at] = '\0';
    cli_check(pages >= 2ull, "120 lines take two pages or more");
    cli_check(cellul.starts(&(CatenaFinitaCfg){.src = s_pdf, .other = "%PDF-1.4", .cap = CLI_CELLS_POOL}),
              "it starts as a PDF 1.4");
    cli_check((pdf.at >= 6u) && (cellul.eq(&(CatenaFinitaCfg){
                                    .src = s_pdf + pdf.at - 6u, .other = "%%EOF\n", .cap = CLI_CELLS_POOL})),
              "it ends with %%EOF");
    cli_check(
        (cellul.find(&(CatenaFinitaCfg){.src = s_pdf,
                                        .cap = CLI_CELLS_POOL,
                                        .other = "(print\\(12\\) # \\(\xe9\\) \\\\ ?) Tj",
                                        .other_cap = CLI_CELLS_POOL}) != NULL) ||
            (cellul.find(&(CatenaFinitaCfg){
                 .src = s_pdf, .cap = CLI_CELLS_POOL, .other = "print\\(12\\)", .other_cap = CLI_CELLS_POOL}) != NULL),
        "a line is shown as its text");
    char count[32];
    CliLine counted = {count, sizeof(count), 0ull};
    cli_line_text(&counted, "/Count ");
    cli_line_decimal(&counted, pages, 1u);
    (void)cli_line_closed(&counted);
    cli_check(cellul.find(&(CatenaFinitaCfg){
                  .src = s_pdf, .cap = CLI_CELLS_POOL, .other = count, .other_cap = CLI_CELLS_POOL}) != NULL,
              "the page tree counts the pages");
    const char *start = NULL;
    for (const char *at = cellul.find(&(CatenaFinitaCfg){
             .src = s_pdf, .cap = CLI_CELLS_POOL, .other = "startxref\n", .other_cap = CLI_CELLS_POOL});
         at != NULL; at = cellul.find(&(CatenaFinitaCfg){
                         .src = at + 1, .cap = CLI_CELLS_POOL, .other = "startxref\n", .other_cap = CLI_CELLS_POOL}))
    {
        start = at;
    }
    unsigned long long xref = 0ull;
    for (const char *digit = (start != NULL) ? start + 10 : ""; (*digit >= '0') && (*digit <= '9'); digit += 1)
    {
        xref = xref * 10ull + (unsigned long long)(*digit - '0');
    }
    int found = (start != NULL) && (xref < pdf.at);
    const char *line = found ? s_pdf + xref : "";
    for (unsigned int skip = 0u; found && (skip < 3u); skip += 1u)
    {
        line = cellul.chr(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL, .byte = '\n'});
        found = (line != NULL);
        line = found ? line + 1 : "";
    }
    unsigned int index = 1u;
    // an entry is ten digits of offset, a space, five of generation, then " n" and its two-byte end
    while (found && (cellul.starts(&(CatenaFinitaCfg){.src = line + 16, .other = " n", .cap = CLI_CELLS_POOL})))
    {
        unsigned long long offset = 0ull;
        for (unsigned int digit = 0u; digit < 10u; digit += 1u)
        {
            offset = offset * 10ull + (unsigned long long)(line[digit] - '0');
        }
        char object[32];
        CliLine named = {object, sizeof(object), 0ull};
        cli_line_decimal(&named, index, 1u);
        cli_line_text(&named, " 0 obj");
        (void)cli_line_closed(&named);
        found =
            (offset < pdf.at) &&
            cli_text_same(s_pdf + offset, object, cellul.len(&(CatenaFinitaCfg){.src = object, .cap = CLI_CELLS_POOL}));
        index += 1u;
        line = cellul.chr(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL, .byte = '\n'});
        line = (line != NULL) ? line + 1 : "";
    }
    cli_check(found && (index > 1u), "each offset the cross-reference gives starts its object");
    cli_check(pdf_read("{\"pages\":\"2\"}", &listing, &s_layout), "the listing reads again");
    pdf.at = 0ull;
    cli_print_pdf(listing, &s_layout, &pdf, &pages);
    cli_check(pages == 1ull, "a layout that names one page prints one");
    cli_print_give();
    return cli_checked();
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_page.c: print layout: a long line wraps under itself, pages break by rows, and page ranges and gray read as written
#include "ui/cli/src/print/print.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// the listing of `count` lines, line `at` the text `lines(at)`
static const CliPrintListing *listing_of(unsigned int count, const char *(*lines)(const CliAtCfg *args))
{
    static char s_text[1u << 16u];
    VerbaLinea text = {s_text, sizeof(s_text), 0ull};
    mmgr_line_put(&text, "{\"title\":\"t\",\"lines\":[");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(&text, (at > 0u) ? ",{\"number\":" : "{\"number\":");
        mmgr_line_decimal(&text, at + 1u, 1u);
        mmgr_line_put(&text, ",\"runs\":[{\"text\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &text, .text = EMBED_CALL(lines, CliAtCfg, .at = at), .length = CLI_UNITY_LENGTH(EMBED_CALL(lines, CliAtCfg, .at = at)));
        mmgr_line_put(&text, ",\"color\":\"#112233\"}]}");
    }
    mmgr_line_put(&text, "],\"tab\":4}");
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(s_text, text.at, &read);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    CliPrintListing *listing = NULL;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_print_json.listing_read, CliPrintJsonCfg, .json = json, .node = read, .listing_pointer = &listing, .err_msg = &err_msg));
    return listing;
}

// a short line, a line of two hundred bytes, and a line starting with a tab
static const char *three_lines(const CliAtCfg *args)
{
    static char s_long[201];
    memor.set(&(MemoriaCfg){.dst = s_long, .val = 'x', .bytes = 200u});
    s_long[200] = '\0';
    return (args->at == 0u) ? "short" : ((args->at == 1u) ? s_long : "\tafter");
}

// each line's text is its index
static const char *numbered_lines(const CliAtCfg *args)
{
    static char s_number[16];
    VerbaLinea line = {s_number, sizeof(s_number), 0ull};
    mmgr_line_decimal(&line, args->at, 1u);
    (void)mmgr_line_finish(&line);
    return s_number;
}

// a layout on A4, portrait or landscape, in color, every page
static CliPrintLayout a4_layout(int landscape)
{
    CliPrintLayout layout;
    memor.set(&(MemoriaCfg){.dst = &layout, .val = 0, .bytes = sizeof(layout)});
    memor.cpy(&(MemoriaCfg){.dst = layout.paper, .src = "a4", .bytes = 3u});
    layout.landscape = landscape;
    layout.copies = 1u;
    return layout;
}

// a portrait layout printing pages 1 to 3 and 5
static CliPrintLayout ranged_layout(void)
{
    CliPrintLayout layout = a4_layout(0);
    memor.cpy(&(MemoriaCfg){.dst = layout.pages, .src = "1-3, 5", .bytes = 7u});
    layout.has_pages = 1;
    return layout;
}

void setUp(void)
{
    cli_unity_begun();
    cli_print_alloc();
}

void tearDown(void)
{
    cli_print_release();
}

void test_a_long_lines_second_row_has_no_number(void)
{
    const CliPrintLayout layout = a4_layout(0);
    const CliPrintLaid *const positioned = EMBED_CALL(cli_print_layout_listing_out.position_out, CliPrintLayoutListingOutCfg, .listing = listing_of(3u, three_lines), .layout = &layout);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT(4u, positioned->rows);
    TEST_ASSERT_TRUE(positioned->row[0].numbered);
    TEST_ASSERT_EQUAL_UINT64(1ull, positioned->row[0].number);
    TEST_ASSERT_TRUE(positioned->row[1].numbered);
    TEST_ASSERT_EQUAL_UINT64(2ull, positioned->row[1].number);
    TEST_ASSERT_FALSE(positioned->row[2].numbered);
}

void test_a_long_lines_first_row_is_cut_to_the_page(void)
{
    const CliPrintLayout layout = a4_layout(0);
    const CliPrintLaid *const positioned = EMBED_CALL(cli_print_layout_listing_out.position_out, CliPrintLayoutListingOutCfg, .listing = listing_of(3u, three_lines), .layout = &layout);
    unsigned long long width = 0ull;
    for (unsigned int piece = 0u; piece < positioned->row[1].pieces; piece += 1u)
    {
        width += positioned->piece[positioned->row[1].first + piece].length;
    }
    TEST_ASSERT_LESS_THAN_UINT64(200ull, width);
    TEST_ASSERT_GREATER_THAN_UINT64(60ull, width);
}

void test_a_tab_is_four_spaces_wide(void)
{
    const CliPrintLayout layout = a4_layout(0);
    const CliPrintLaid *const positioned = EMBED_CALL(cli_print_layout_listing_out.position_out, CliPrintLayoutListingOutCfg, .listing = listing_of(3u, three_lines), .layout = &layout);
    const CliPrintPiece *const after = &positioned->piece[positioned->row[positioned->rows - 1u].first];
    CLI_TEST_ASSERT_TEXT("    after", positioned->text + after->text, after->length);
}

void test_two_hundred_lines_alloc_at_min_three_pages(void)
{
    const CliPrintLayout layout = a4_layout(0);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT(3u, EMBED_CALL(cli_print_layout_listing_out.position_out, CliPrintLayoutListingOutCfg, .listing = listing_of(200u, numbered_lines), .layout = &layout)->pages);
}

void test_a_landscape_page_fits_fewer_rows(void)
{
    const CliPrintLayout portrait = a4_layout(0);
    const CliPrintLayout landscape = a4_layout(1);
    const unsigned int portrait_pages = EMBED_CALL(cli_print_layout_listing_out.position_out, CliPrintLayoutListingOutCfg, .listing = listing_of(200u, numbered_lines), .layout = &portrait)->pages;
    TEST_ASSERT_GREATER_THAN_UINT(portrait_pages, EMBED_CALL(cli_print_layout_listing_out.position_out, CliPrintLayoutListingOutCfg, .listing = listing_of(200u, numbered_lines), .layout = &landscape)->pages);
}

void test_the_named_pages_print(void)
{
    const CliPrintLayout layout = ranged_layout();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_print_table.prints, CliPrintCfg, .layout = &layout, .page_u64 = 2ull));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_print_table.prints, CliPrintCfg, .layout = &layout, .page_u64 = 5ull));
}

void test_a_page_not_named_does_not_print(void)
{
    const CliPrintLayout layout = ranged_layout();
    TEST_ASSERT_FALSE(EMBED_CALL(cli_print_table.prints, CliPrintCfg, .layout = &layout, .page_u64 = 4ull));
}

void test_the_page_ranges_read_as_written(void)
{
    const CliPrintLayout layout = ranged_layout();
    int low[4];
    int high[4];
    TEST_ASSERT_EQUAL_UINT(2u, EMBED_CALL(cli_print_layout_low.ranges, CliPrintLayoutLowCfg, .layout = &layout, .low = low, .high = high, .capacity = 4u));
    TEST_ASSERT_EQUAL_INT(1, low[0]);
    TEST_ASSERT_EQUAL_INT(3, high[0]);
    TEST_ASSERT_EQUAL_INT(5, low[1]);
    TEST_ASSERT_EQUAL_INT(5, high[1]);
}

void test_a_color_turns_gray_by_its_lightness(void)
{
    unsigned char rgb[3];
    EMBED_CALL(cli_print_text.color, CliPrintTextCfg, .text = "#ff0000", .length = 7ull, .gray = 1, .rgb = rgb);
    TEST_ASSERT_EQUAL_UINT8(54u, rgb[0]);
    TEST_ASSERT_EQUAL_UINT8(54u, rgb[1]);
    TEST_ASSERT_EQUAL_UINT8(54u, rgb[2]);
}

void test_a_color_reads_as_written(void)
{
    unsigned char rgb[3];
    EMBED_CALL(cli_print_text.color, CliPrintTextCfg, .text = "#0a0b0c", .length = 7ull, .gray = 0, .rgb = rgb);
    TEST_ASSERT_EQUAL_UINT8(10u, rgb[0]);
    TEST_ASSERT_EQUAL_UINT8(11u, rgb[1]);
    TEST_ASSERT_EQUAL_UINT8(12u, rgb[2]);
}

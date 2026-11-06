// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_pdf.c: a listing is a PDF whose cross-reference finds each object, and a layout that names one page prints one
#include "ui/cli/src/print/print.h"
#include "ui/cli/src/text/text.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_pdf[1u << 22u];
static CliPrintLayout s_layout;
static unsigned long long s_pages;
static unsigned long long s_size;

// the PDF of 120 numbered lines under the layout `layout_text` written into s_pdf, NUL-terminated
static void pdf_written(const char *layout_text)
{
    static char s_listing[1u << 16u];
    VerbaLinea text = {s_listing, sizeof(s_listing), 0ull};
    mmgr_line_put(&text, "{\"title\":\"main.py \xe2\x80\x94 orior\",\"lines\":[");
    for (unsigned int number = 1u; number <= 120u; number += 1u)
    {
        mmgr_line_put(&text, (number > 1u) ? ",{\"number\":" : "{\"number\":");
        mmgr_line_decimal(&text, number, 1u);
        mmgr_line_put(&text, ",\"runs\":[{\"text\":\"print(");
        mmgr_line_decimal(&text, number, 1u);
        mmgr_line_put(&text, ") # (\xc3\xa9) \\\\ \xe2\x98\x83\",\"color\":\"#7a5ee0\",\"bold\":");
        mmgr_line_put(&text, ((number % 2u) == 0u) ? "true" : "false");
        mmgr_line_put(&text, ",\"italic\":");
        mmgr_line_put(&text, ((number % 3u) == 0u) ? "true}]}" : "false}]}");
    }
    mmgr_line_put(&text, "],\"tab\":4}");
    unsigned int read = CLI_JSON_NONE;
    const CliJson *json = cli_unity_parsed(s_listing, text.at, &read);
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    CliPrintListing *listing = NULL;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_print_json.listing_read, CliPrintJsonCfg, .json = json, .node = read, .listing_pointer = &listing, .err_msg = &err_msg));
    json = cli_unity_parsed(layout_text, CLI_UNITY_LENGTH(layout_text), &read);
    EMBED_CALL(cli_print_json_node.layout_read, CliPrintJsonNodeCfg, .json = json, .node = read, .layout_out = &s_layout);
    VerbaLinea pdf = {s_pdf, sizeof(s_pdf) - 1u, 0ull};
    EMBED_CALL(cli_print_layout_listing_out.pdf, CliPrintLayoutListingOutCfg, .listing = listing, .layout = &s_layout, .out = &pdf, .pages = &s_pages);
    s_size = pdf.at;
    s_pdf[s_size] = '\0';
}

// where the last `startxref` of the PDF points, the test failed where it has none
static unsigned long long cross_reference_offset(void)
{
    const char *start = NULL;
    for (const char *at = cellul.find(&(CatenaFinitaCfg){.src = s_pdf, .cap = CLI_POOL_BYTES, .other = "startxref\n", .other_cap = CLI_POOL_BYTES}); at != NULL;
         at = cellul.find(&(CatenaFinitaCfg){.src = at + 1, .cap = CLI_POOL_BYTES, .other = "startxref\n", .other_cap = CLI_POOL_BYTES}))
    {
        start = at;
    }
    TEST_ASSERT_NOT_NULL(start);
    unsigned long long offset = 0ull;
    for (const char *digit = start + 10; (*digit >= '0') && (*digit <= '9'); digit += 1)
    {
        offset = offset * 10ull + (unsigned long long)(*digit - '0');
    }
    TEST_ASSERT_LESS_THAN_UINT64(s_size, offset);
    return offset;
}

void setUp(void)
{
    cli_unity_begun();
    cli_print_alloc();
    pdf_written("{}");
}

void tearDown(void)
{
    cli_print_release();
}

void test_120_lines_alloc_at_min_two_pages(void)
{
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(2ull, s_pages);
}

void test_it_starts_as_a_pdf_1_4(void)
{
    TEST_ASSERT_EQUAL_MEMORY("%PDF-1.4", s_pdf, 8u);
}

void test_it_ends_with_the_end_of_file_mark(void)
{
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(6ull, s_size);
    TEST_ASSERT_EQUAL_STRING("%%EOF\n", s_pdf + s_size - 6u);
}

void test_a_line_is_displayed_as_its_text(void)
{
    CLI_TEST_ASSERT_CONTAINS("print\\(12\\)", s_pdf);
}

void test_the_page_tree_counts_the_pages(void)
{
    char count[32];
    VerbaLinea counted = {count, sizeof(count), 0ull};
    mmgr_line_put(&counted, "/Count ");
    mmgr_line_decimal(&counted, s_pages, 1u);
    (void)mmgr_line_finish(&counted);
    CLI_TEST_ASSERT_CONTAINS(count, s_pdf);
}

void test_each_cross_reference_offset_starts_its_object(void)
{
    const char *line = s_pdf + cross_reference_offset();
    for (unsigned int skip = 0u; skip < 3u; skip += 1u)
    {
        line = cellul.chr(&(CatenaFinitaCfg){.src = line, .cap = CLI_POOL_BYTES, .byte = '\n'});
        TEST_ASSERT_NOT_NULL(line);
        line += 1;
    }
    unsigned int index = 1u;
    // an entry is ten digits of offset, a space, five of generation, then " n" and its two-byte end
    while (cellul.starts(&(CatenaFinitaCfg){.src = line + 16, .other = " n", .cap = CLI_POOL_BYTES}))
    {
        unsigned long long offset = 0ull;
        for (unsigned int digit = 0u; digit < 10u; digit += 1u)
        {
            offset = offset * 10ull + (unsigned long long)(line[digit] - '0');
        }
        char object[32];
        VerbaLinea named = {object, sizeof(object), 0ull};
        mmgr_line_decimal(&named, index, 1u);
        mmgr_line_put(&named, " 0 obj");
        (void)mmgr_line_finish(&named);
        TEST_ASSERT_LESS_THAN_UINT64(s_size, offset);
        TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = s_pdf + offset, .other_pointer = object, .size = CLI_UNITY_LENGTH(object)), object);
        index += 1u;
        line = cellul.chr(&(CatenaFinitaCfg){.src = line, .cap = CLI_POOL_BYTES, .byte = '\n'});
        TEST_ASSERT_NOT_NULL(line);
        line += 1;
    }
    TEST_ASSERT_GREATER_THAN_UINT(1u, index);
}

void test_a_layout_that_names_one_page_prints_one(void)
{
    pdf_written("{\"pages\":\"2\"}");
    TEST_ASSERT_EQUAL_UINT64(1ull, s_pages);
}

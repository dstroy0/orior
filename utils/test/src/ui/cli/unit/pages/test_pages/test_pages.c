// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_pages.c: lines are read as the page splits them from anywhere in the file; a view is placed the lines it needs, a wrapping one by its rows and a step
// by the bytes it reads; and edits are written over the file's lines and saved through a file beside it
#include "ui/cli/src/editor/pages.h"
#include "ui/cli/src/text/text.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// A shape: its lines, and whether its index is whole
typedef struct
{
    unsigned long long lines;
    int indexed;
} PagesShape;

static char s_out[1u << 20u];
static char s_folder[CLI_PATH_MAX_LEN];
static char s_text[1u << 17u];
static char s_error[256];
static int s_folder_created;

// a file of long lines: 1000 bytes each, the first naming its number as "long N"
static char s_long[1u << 19u];
static int s_long_written;
// a file of short lines and then long ones, its lines' bytes on average far less than those of the long ones
static char s_mixed[1u << 21u];
// a file of many short lines, "row N", for a view moving thousands of them a report
static char s_rows[1u << 18u];

// the steps of a sequence of views: whether each did what its case names
static int s_viewed_run_result;
static int s_viewed[12];
static int s_wrapped_run_result;
static int s_wrapped[3];
static int s_mixed_run_result;
static int s_mixed_steps[3];

static int s_edit_run_result;
static int s_edit_opened;
static int s_edited;
static unsigned long long s_edit_lines;
static int s_shadow_row;
static int s_shadow_joined;
static int s_shadow_other;
static char s_edit_within[1024];
static char s_edit_join[1024];
static char s_edit_changed[256];
static char s_edit_removed[1024];
static char s_edit_found[256];
static int s_edit_saved;
static unsigned long long s_saved_lines;
static int s_saved_indexed;
static int s_saved_head;
static int s_saved_joined;
static int s_saved_end;
static char s_saved_changed[256];
static int s_shadow_after;

static int s_wide_run_result;
static int s_wide_opened;
static unsigned long long s_wide_lines;
static char s_wide_found[256];
static int s_wide_saved;
static int s_wide_sized;
static unsigned long long s_wide_size;
static unsigned long long s_wide_total;

// a whole number member of a node of the shared tree, 0 where it has none
static unsigned long long integer_member(const CliJson *json, unsigned int node, const char *name)
{
    const unsigned int number = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = node, .name = name);
    unsigned long long value = 0ull;
    for (unsigned long long at = 0ull; EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = number, .kind = CFG_JSON_NUMBER) && (at < json->node[number].length); at += 1ull)
    {
        // a digit's value is below 10
        value = value * 10ull + (unsigned long long)(json->text[json->node[number].text + at] - '0');
    }
    return value;
}

// a shape written as `{"lines", "indexed", "size"}` read
static PagesShape shape_of(unsigned long long size)
{
    unsigned int shape = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(s_out, size, &shape);
    const PagesShape read = {integer_member(json, shape, "lines"), EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = shape, .name = "indexed"), .kind = CFG_JSON_TRUE)};
    return read;
}

// the shape of `key` once its index is whole, waited for up to five seconds
static PagesShape shape_indexed(const char *key)
{
    const unsigned long long until = cli_clock_milliseconds() + 5000ull;
    for (;;)
    {
        char why[256];
        VerbaLinea err_msg = {why, sizeof(why), 0ull};
        VerbaLinea out = {s_out, sizeof(s_out), 0ull};
        (void)EMBED_CALL(cli_pages_path.shape, CliPagesPathCfg, .key = key, .out = &out, .err_msg = &err_msg);
        const PagesShape shape = shape_of(out.at);
        if (shape.indexed || (cli_clock_milliseconds() > until))
        {
            return shape;
        }
    }
}

// the file `name` of the folder written with `size` bytes of `text`, its path into `path`
static void file_written(const char *name, const char *text, unsigned long long size, char *path)
{
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_folder, .relative = name, .joined = path);
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = text, .size = size), name);
}

// a file of 5000 lines of "line N" opened as `key`, indexed
static void lines_file_opened(const char *key)
{
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea text = {s_text, sizeof(s_text), 0ull};
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    for (unsigned int number = 0u; number < 5000u; number += 1u)
    {
        mmgr_line_put(&text, "line ");
        mmgr_line_decimal(&text, number, 1u);
        mmgr_line_put(&text, "\r\n");
    }
    file_written("read.txt", s_text, text.at, path);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_pages_path.open, CliPagesPathCfg, .key = key, .path = path, .out = &out, .err_msg = &err_msg));
    (void)shape_indexed(key);
}

// the file of long lines written once for the process, its path in `path`
static void long_file_written(char *path)
{
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_folder, .relative = "long.txt", .joined = path);
    if (!s_long_written)
    {
        VerbaLinea text = {s_long, sizeof(s_long), 0ull};
        s_long_written = 1;
        for (unsigned int number = 0u; number < 400u; number += 1u)
        {
            const unsigned long long begun = text.at;
            mmgr_line_put(&text, "long ");
            mmgr_line_decimal(&text, number, 1u);
            while ((text.at - begun) < 1000ull)
            {
                mmgr_line_ch(&text, 'x');
            }
            mmgr_line_ch(&text, '\n');
        }
        TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = s_long, .size = text.at));
    }
}

// the file of long lines opened as `key`, indexed
static void long_file_opened(const char *key)
{
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    long_file_written(path);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_pages_path.open, CliPagesPathCfg, .key = key, .path = path, .out = &out, .err_msg = &err_msg));
    (void)shape_indexed(key);
}

// a view that wraps its lines at 100 letters reported through its movements, each step's result recorded, once for the process
static void wrapped_run_result(void)
{
    CliPagesView view = {100ull, 20ull, 0ull, 90ull, 130ull, 0ull, 0ull, 0ull, 100ull};
    CliPagesPositioned positioned = {0ull, 0ull, 0ull, 0ull, 0ull};
    if (s_wrapped_run_result)
    {
        return;
    }
    s_wrapped_run_result = 1;
    long_file_opened("wrapped");
    s_wrapped[0] = EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = "wrapped", .view = &view, .positioned = &positioned) && (positioned.from == 99ull) && (positioned.to == 103ull);
    view.cached_from = positioned.from;
    view.cached_to = positioned.to;
    s_wrapped[1] = EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = "wrapped", .view = &view, .positioned = &positioned) && (positioned.from == 103ull) && (positioned.to > positioned.from);
    view.top = 200ull;
    s_wrapped[2] = EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = "wrapped", .view = &view, .positioned = &positioned) && (positioned.from >= 290ull) && (positioned.from <= 299ull) && (positioned.to >= 302ull) && (positioned.to <= 310ull);
    EMBED_CALL(cli_pages.release, CliPagesCfg, .key = "wrapped");
}

// a view reported through a file of short lines and then long ones, each step's result recorded, once for the process
static void mixed_run_result(void)
{
    char path[CLI_PATH_MAX_LEN];
    CliPagesView view = {1980ull, 20ull, 0ull, 1960ull, 2050ull, 1972ull, 2008ull, 0ull};
    CliPagesPositioned positioned = {0ull, 0ull, 0ull, 0ull, 0ull};
    VerbaLinea text = {s_mixed, sizeof(s_mixed), 0ull};
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    if (s_mixed_run_result)
    {
        return;
    }
    s_mixed_run_result = 1;
    for (unsigned int number = 0u; number < 2000u; number += 1u)
    {
        mmgr_line_put(&text, "s ");
        mmgr_line_decimal(&text, number, 1u);
        mmgr_line_ch(&text, '\n');
    }
    for (unsigned int number = 0u; number < 60u; number += 1u)
    {
        for (unsigned int at = 0u; at < 20000u; at += 1u)
        {
            mmgr_line_ch(&text, 'x');
        }
        mmgr_line_ch(&text, '\n');
    }
    file_written("mixed.txt", s_mixed, text.at, path);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_pages_path.open, CliPagesPathCfg, .key = "mixed", .path = path, .out = &out, .err_msg = &err_msg));
    (void)shape_indexed("mixed");
    s_mixed_steps[0] = EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = "mixed", .view = &view, .positioned = &positioned) && (positioned.from == 2008ull) && ((positioned.to - positioned.from) >= 1ull) && ((positioned.to - positioned.from) <= 7ull);
    view.top = 2010ull;
    view.map_from = 2000ull;
    view.map_to = 2040ull;
    view.cached_from = 0ull;
    view.cached_to = 0ull;
    (void)EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = "mixed", .view = &view, .positioned = &positioned);
    s_mixed_steps[1] = EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = "mixed", .view = &view, .positioned = &positioned) && (positioned.from == 2002ull) && ((positioned.to - positioned.from) >= 1ull) && ((positioned.to - positioned.from) <= 7ull);
    view.cached_from = positioned.from;
    view.cached_to = positioned.to;
    const unsigned long long full_to = positioned.to;
    s_mixed_steps[2] = EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = "mixed", .view = &view, .positioned = &positioned) && (positioned.from == full_to) && (positioned.to > positioned.from);
    EMBED_CALL(cli_pages.release, CliPagesCfg, .key = "mixed");
}

// `text` copied into `out`, which is sized for `capacity`
static void text_stored(char *out, unsigned long long capacity, const char *text, unsigned long long size)
{
    VerbaLinea line = {out, capacity, 0ull};
    mmgr_line_put_n(&line, text, size);
    (void)mmgr_line_finish(&line);
}

// a file of wide lines past a reading's width opened, searched and saved, what each answered recorded, once for the process
static void wide_run_result(void)
{
    static char s_wide[32u * 8192u];
    static const char s_last[] = "the last line\n";
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    if (s_wide_run_result)
    {
        return;
    }
    s_wide_run_result = 1;
    memor.set(&(MemoriaCfg){.dst = s_wide, .val = 'w', .bytes = sizeof(s_wide)});
    for (unsigned int line = 1u; line <= 32u; line += 1u)
    {
        s_wide[(line * 8192u) - 1u] = '\n';
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_folder, .relative = "wide.txt", .joined = path);
    int written = EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = path, .bytes = "", .size = 0ull, .fresh = 1);
    for (unsigned int part = 0u; written && (part < 282u); part += 1u)
    {
        written = EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = path, .bytes = s_wide, .size = sizeof(s_wide), .fresh = 0);
    }
    written = written && EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = path, .bytes = s_last, .size = sizeof(s_last) - 1u, .fresh = 0);
    s_wide_total = (282ull * sizeof(s_wide)) + sizeof(s_last) - 1u;
    s_wide_opened = written && EMBED_CALL(cli_pages_path.open, CliPagesPathCfg, .key = "wide", .path = path, .out = &out, .err_msg = &err_msg);
    s_wide_lines = shape_indexed("wide").lines;
    const CliDocNeedle needle = {"the last", 0, 1, 0};
    out.at = 0ull;
    err_msg.at = 0ull;
    (void)EMBED_CALL(cli_pages_out_err_msg_needle.find, CliPagesOutErrMsgNeedleCfg, .key = "wide", .needle = &needle, .most = 10ull, .out = &out, .err_msg = &err_msg);
    text_stored(s_wide_found, sizeof(s_wide_found), s_out, out.at);
    out.at = 0ull;
    err_msg.at = 0ull;
    struct CliPaged *const read = EMBED_CALL(cli_pages_path.save_beside_alloc, CliPagesPathCfg, .key = "wide", .err_msg = &err_msg);
    s_wide_saved = (read != NULL) && EMBED_CALL(cli_pages_place.save_place, CliPagesPlaceCfg, .read = read, .whole = 1, .err_msg = &err_msg);
    s_wide_sized = EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = path, .size_pointer = &s_wide_size);
    EMBED_CALL(cli_pages.release, CliPagesCfg, .key = "wide");
}

void setUp(void)
{
    cli_unity_begun();
    if (!s_folder_created)
    {
        s_folder_created = 1;
        cli_unity_folder("pages", s_folder);
    }
}

void tearDown(void)
{
}

void test_a_file_ending_in_a_newline_has_an_empty_line_last(void)
{
    lines_file_opened("read");
    const unsigned long long lines = shape_indexed("read").lines;
    EMBED_CALL(cli_pages.release, CliPagesCfg, .key = "read");
    TEST_ASSERT_EQUAL_UINT64(5001ull, lines);
}

void test_find_reaches_the_first_line_of_a_block_of_letters_of_one_byte_each(void)
{
    const CliDocNeedle needle = {"line 0", 0, 1, 0};
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    lines_file_opened("read");
    (void)EMBED_CALL(cli_pages_out_err_msg_needle.find, CliPagesOutErrMsgNeedleCfg, .key = "read", .needle = &needle, .most = 10ull, .out = &out, .err_msg = &err_msg);
    EMBED_CALL(cli_pages.release, CliPagesCfg, .key = "read");
    CLI_TEST_ASSERT_TEXT("[[0,0,6]]", s_out, out.at);
}

void test_a_file_of_long_lines_has_its_lines_counted(void)
{
    long_file_opened("long");
    const unsigned long long lines = shape_indexed("long").lines;
    EMBED_CALL(cli_pages.release, CliPagesCfg, .key = "long");
    TEST_ASSERT_EQUAL_UINT64(401ull, lines);
}

void test_a_wrapping_view_at_remainder_is_positioned_the_lines_its_rows_need(void)
{
    wrapped_run_result();
    TEST_ASSERT_TRUE(s_wrapped[0]);
}

void test_a_wrapping_view_whose_lines_are_held_is_positioned_the_step_below_them(void)
{
    wrapped_run_result();
    TEST_ASSERT_TRUE(s_wrapped[1]);
}

void test_a_wrapping_view_that_moved_is_positioned_the_lines_it_will_reach_a_few_each_side(void)
{
    wrapped_run_result();
    TEST_ASSERT_TRUE(s_wrapped[2]);
}

void test_a_step_into_long_lines_is_as_many_as_its_bytes_reach(void)
{
    mixed_run_result();
    TEST_ASSERT_TRUE(s_mixed_steps[0]);
}

void test_a_view_at_remainder_among_long_lines_is_positioned_its_rows_as_far_as_its_bytes_reach(void)
{
    mixed_run_result();
    TEST_ASSERT_TRUE(s_mixed_steps[1]);
}

void test_a_view_whose_long_lines_are_complete_is_positioned_the_rows_after_them(void)
{
    mixed_run_result();
    TEST_ASSERT_TRUE(s_mixed_steps[2]);
}

void test_a_view_moving_far_through_short_lines_is_positioned_no_more_than_a_step_each_side(void)
{
    char path[CLI_PATH_MAX_LEN];
    CliPagesView view = {19000ull, 20ull, 64ull, 19000ull, 19020ull, 0ull, 0ull, 0ull};
    CliPagesPositioned positioned = {0ull, 0ull, 0ull, 0ull, 0ull};
    VerbaLinea text = {s_rows, sizeof(s_rows), 0ull};
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    for (unsigned int number = 0u; number < 20000u; number += 1u)
    {
        mmgr_line_put(&text, "row ");
        mmgr_line_decimal(&text, number, 1u);
        mmgr_line_ch(&text, '\n');
    }
    file_written("rows.txt", s_rows, text.at, path);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_pages_path.open, CliPagesPathCfg, .key = "rows", .path = path, .out = &out, .err_msg = &err_msg));
    (void)shape_indexed("rows");
    (void)EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = "rows", .view = &view, .positioned = &positioned);
    view.top = 14000ull;
    const int positioned_ok = EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = "rows", .view = &view, .positioned = &positioned);
    EMBED_CALL(cli_pages.release, CliPagesCfg, .key = "rows");
    TEST_ASSERT_TRUE(positioned_ok);
    TEST_ASSERT_LESS_OR_EQUAL_UINT64(7992ull, positioned.from);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(10028ull, positioned.to);
    TEST_ASSERT_LESS_OR_EQUAL_UINT64(2036ull, positioned.to - positioned.from);
}

void test_a_file_of_wide_lines_opens_and_is_indexed_complete(void)
{
    wide_run_result();
    TEST_ASSERT_TRUE(s_wide_opened);
    TEST_ASSERT_EQUAL_UINT64(9026ull, s_wide_lines);
}

void test_find_reaches_the_line_end_a_block_wider_than_a_read_state(void)
{
    wide_run_result();
    TEST_ASSERT_EQUAL_STRING("[[9024,0,8]]", s_wide_found);
}

void test_a_save_writes_every_byte_of_every_wide_line(void)
{
    wide_run_result();
    TEST_ASSERT_TRUE(s_wide_saved);
    TEST_ASSERT_TRUE(s_wide_sized);
    TEST_ASSERT_EQUAL_UINT64(s_wide_total, s_wide_size);
}

// the size of the file at `path`, ~0 where there is none
static unsigned long long size_of(const char *path)
{
    unsigned long long size = 0ull;
    return EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = path, .size_pointer = &size) ? size : ~0ull;
}

void test_a_save_put_back_leaves_the_file_as_it_was_and_one_put_in_place_replaces_it(void)
{
    char path[CLI_PATH_MAX_LEN];
    char beside[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_folder, .relative = "placed.txt", .joined = path);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_folder, .relative = ".placed.txt.saving", .joined = beside);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = path, .bytes = "one\ntwo\n", .size = 8ull, .fresh = 1));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_pages_path.open, CliPagesPathCfg, .key = "placed", .path = path, .out = &out, .err_msg = &err_msg));
    TEST_ASSERT_TRUE(shape_indexed("placed").indexed);
    const CliEdit edit = {{0ull, 0ull}, {0ull, 0ull}, "x", 1ull};
    const CliEdit *const changes[1] = {&edit};
    const unsigned int edits[1] = {1u};
    out.at = 0ull;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_pages_out_err_msg.edit, CliPagesOutErrMsgCfg, .key = "placed", .changes = changes, .edits = edits, .count_u32 = 1u, .out = &out, .err_msg = &err_msg));
    struct CliPaged *read = EMBED_CALL(cli_pages_path.save_beside_alloc, CliPagesPathCfg, .key = "placed", .err_msg = &err_msg);
    TEST_ASSERT_NOT_NULL(read);
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(9ull, size_of(beside), "the text with its edit written beside the file");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_pages_place.save_place, CliPagesPlaceCfg, .read = read, .whole = 0, .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(~0ull, size_of(beside), "nothing left beside the file");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(8ull, size_of(path), "the file as it was");
    read = EMBED_CALL(cli_pages_path.save_beside_alloc, CliPagesPathCfg, .key = "placed", .err_msg = &err_msg);
    TEST_ASSERT_NOT_NULL(read);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_pages_place.save_place, CliPagesPlaceCfg, .read = read, .whole = 1, .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_UINT64(~0ull, size_of(beside));
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(9ull, size_of(path), "the file replaced by the text with its edit");
    const PagesShape shape = shape_indexed("placed");
    TEST_ASSERT_TRUE_MESSAGE(shape.indexed, "the text stored in place reads the new file, its index whole");
    TEST_ASSERT_EQUAL_UINT64(3ull, shape.lines);
    EMBED_CALL(cli_pages.release, CliPagesCfg, .key = "placed");
}

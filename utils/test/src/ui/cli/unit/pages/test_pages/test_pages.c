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
static char s_why[256];
static int s_folder_made;

// a file of long lines: 1000 bytes each, the first naming its number as "long N"
static char s_long[1u << 19u];
static int s_long_written;
// a file of short lines and then long ones, its lines' bytes on average far less than those of the long ones
static char s_mixed[1u << 21u];
// a file of many short lines, "row N", for a view moving thousands of them a report
static char s_rows[1u << 18u];

// the steps of a sequence of views: whether each did what its case names
static int s_viewed_ran;
static int s_viewed[12];
static int s_wrapped_ran;
static int s_wrapped[3];
static int s_mixed_ran;
static int s_mixed_steps[3];

static int s_edit_ran;
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

static int s_wide_ran;
static int s_wide_opened;
static unsigned long long s_wide_lines;
static char s_wide_found[256];
static int s_wide_saved;
static int s_wide_sized;
static unsigned long long s_wide_size;
static unsigned long long s_wide_whole;

// a whole number member of a node of the shared tree, 0 where it has none
static unsigned long long whole_member(const CliJson *json, unsigned int node, const char *name)
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
    const PagesShape read = {whole_member(json, shape, "lines"), EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = shape, .name = "indexed"), .kind = CFG_JSON_TRUE)};
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

// the lines of `key` from `from` to before `to`, joined by bars, into `joined`, which is sized for 1024
static void lines_joined(const char *key, unsigned long long from, unsigned long long to, char *joined)
{
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    (void)EMBED_CALL(cli_pages_out.read, CliPagesOutCfg, .key = key, .from = from, .to = to, .cut = NULL, .out = &out, .lengths = NULL, .err_msg = &err_msg);
    unsigned int lines = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(s_out, out.at, &lines);
    VerbaLinea line = {joined, 1024u, 0ull};
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = lines, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = lines, .index = at), .length_pointer = NULL);
        mmgr_line_put(&line, (at > 0u) ? "|" : "");
        mmgr_line_put(&line, (text != NULL) ? text : "");
    }
    (void)mmgr_line_finish(&line);
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
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
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
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    long_file_written(path);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_pages_path.open, CliPagesPathCfg, .key = key, .path = path, .out = &out, .err_msg = &err_msg));
    (void)shape_indexed(key);
}

// the lines of `key` from `from` to before `to` read, asserted to be each the long line of its number, whole
static void assert_long_lines(const char *key, unsigned long long from, unsigned long long to)
{
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    (void)EMBED_CALL(cli_pages_out.read, CliPagesOutCfg, .key = key, .from = from, .to = to, .cut = NULL, .out = &out, .lengths = NULL, .err_msg = &err_msg);
    unsigned int lines = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(s_out, out.at, &lines);
    unsigned int count = 0u;
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = lines, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        unsigned long long length = 0ull;
        char named[32];
        VerbaLinea name = {named, sizeof(named), 0ull};
        const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = lines, .index = at), .length_pointer = &length);
        mmgr_line_put(&name, "long ");
        mmgr_line_decimal(&name, from + at, 1u);
        TEST_ASSERT_NOT_NULL(text);
        TEST_ASSERT_EQUAL_UINT64(1000ull, length);
        TEST_ASSERT_EQUAL_MEMORY(named, text, name.at);
        TEST_ASSERT_EQUAL_CHAR('x', text[name.at]);
        count += 1u;
    }
    TEST_ASSERT_EQUAL_UINT64(to - from, count);
}

// the lines a view of the long file is answered read: 1 where each is the long line of its number cut at the placed cut and its length in the page's units 1000
static int cut_lines_match(const char *key, const CliPagesPlaced *placed)
{
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out) / 2u, 0ull};
    VerbaLinea lengths = {s_out + (sizeof(s_out) / 2u), sizeof(s_out) / 2u, 0ull};
    unsigned long long made = placed->cut;
    (void)EMBED_CALL(cli_pages_out.read, CliPagesOutCfg, .key = key, .from = placed->from, .to = placed->to, .cut = &made, .out = &out, .lengths = &lengths, .err_msg = &err_msg);
    unsigned int lines = CLI_JSON_NONE;
    const CliJson *json = cli_unity_parsed(s_out, out.at, &lines);
    int cut = 1;
    unsigned int count = 0u;
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = lines, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        unsigned long long length = 0ull;
        char named[32];
        VerbaLinea name = {named, sizeof(named), 0ull};
        const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = lines, .index = at), .length_pointer = &length);
        mmgr_line_put(&name, "long ");
        mmgr_line_decimal(&name, placed->from + at, 1u);
        // the file ends in a newline, its last line, the 401st, empty
        const int last = (placed->from + at) == 400ull;
        cut = cut && (text != NULL) && (last ? (length == 0ull) : ((length == ((placed->cut != 0ull) ? placed->cut : 1000ull)) && (memor.cmp(&(MemoriaCfg){.src = text, .other = named, .bytes = name.at}) == 0)));
        count += 1u;
    }
    unsigned int counted = CLI_JSON_NONE;
    json = cli_unity_parsed(s_out + (sizeof(s_out) / 2u), lengths.at, &counted);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned int length = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = counted, .index = at);
        const int last = (placed->from + at) == 400ull;
        cut = cut && EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = length, .kind = CFG_JSON_NUMBER) && (last ? (json->text[json->node[length].text] == '0') : ((json->node[length].length == 4u) && (json->text[json->node[length].text] == '1')));
    }
    return cut && (made == placed->cut) && (count == (placed->to - placed->from));
}

// a view of the long file reported through its movements, each step's result recorded, once for the process
static void viewed_ran(void)
{
    CliPagesView view = {100ull, 20ull, 64ull, 90ull, 130ull, 0ull, 0ull, 0ull};
    CliPagesPlaced placed = {0ull, 0ull, 0ull, 0ull, 0ull};
    if (s_viewed_ran)
    {
        return;
    }
    s_viewed_ran = 1;
    long_file_opened("viewed");
    s_viewed[0] = EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = "viewed", .view = &view, .placed = &placed) && (placed.cut == 0ull) && (placed.from <= 100ull) && (placed.to >= 120ull) && (placed.to - placed.from < 60ull);
    s_viewed[1] = cut_lines_match("viewed", &placed);
    view.cached_from = placed.from;
    view.cached_to = placed.to;
    view.top = 110ull;
    const unsigned long long cached_end = view.cached_to;
    s_viewed[2] = EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = "viewed", .view = &view, .placed = &placed) && (placed.cut == 64ull) && (placed.from == cached_end) && (placed.to == 401ull) && (placed.save_from <= view.cached_from) && (placed.save_to >= placed.to);
    s_viewed[3] = cut_lines_match("viewed", &placed);
    view.cached_to = placed.to;
    view.cached_cut = placed.cut;
    view.top = 115ull;
    s_viewed[4] = EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = "viewed", .view = &view, .placed = &placed) && (placed.from == placed.to) && (placed.save_from <= view.cached_from) && (placed.save_to >= view.cached_to);
    view.cached_to = 200ull;
    view.top = 120ull;
    s_viewed[5] = EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = "viewed", .view = &view, .placed = &placed) && (placed.from == 200ull) && (placed.to == 401ull);
    view.cached_to = 148ull;
    view.top = 165ull;
    s_viewed[6] = EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = "viewed", .view = &view, .placed = &placed) && (placed.cut == 64ull) && (placed.from > 165ull) && (placed.from <= 191ull) && (placed.to >= 249ull) && (placed.to - placed.from < 60ull);
    view.cached_from = placed.from;
    view.cached_to = placed.to;
    s_viewed[7] = EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = "viewed", .view = &view, .placed = &placed) && (placed.cut == 0ull) && (placed.from <= 165ull) && (placed.to >= 185ull);
    view.cached_from = placed.from;
    view.cached_to = placed.to;
    view.cached_cut = 0ull;
    view.map_from = 60ull;
    view.map_to = 300ull;
    s_viewed[8] = EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = "viewed", .view = &view, .placed = &placed) && (placed.cut == 0ull) && (placed.from == view.cached_to) && (placed.to > placed.from) && (placed.save_from <= 60ull) && (placed.save_to >= 300ull);
    view.cached_from = 300ull;
    view.cached_to = 400ull;
    view.top = 350ull;
    (void)EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = "viewed", .view = &view, .placed = &placed);
    view.top = 230ull;
    s_viewed[9] = EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = "viewed", .view = &view, .placed = &placed) && (placed.from <= 72ull) && (placed.to >= 168ull);
    EMBED_CALL(cli_pages.release, CliPagesCfg, .key = "viewed");
}

// a view that wraps its lines at 100 letters reported through its movements, each step's result recorded, once for the process
static void wrapped_ran(void)
{
    CliPagesView view = {100ull, 20ull, 0ull, 90ull, 130ull, 0ull, 0ull, 0ull, 100ull};
    CliPagesPlaced placed = {0ull, 0ull, 0ull, 0ull, 0ull};
    if (s_wrapped_ran)
    {
        return;
    }
    s_wrapped_ran = 1;
    long_file_opened("wrapped");
    s_wrapped[0] = EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = "wrapped", .view = &view, .placed = &placed) && (placed.from == 99ull) && (placed.to == 103ull);
    view.cached_from = placed.from;
    view.cached_to = placed.to;
    s_wrapped[1] = EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = "wrapped", .view = &view, .placed = &placed) && (placed.from == 103ull) && (placed.to > placed.from);
    view.top = 200ull;
    s_wrapped[2] = EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = "wrapped", .view = &view, .placed = &placed) && (placed.from >= 290ull) && (placed.from <= 299ull) && (placed.to >= 302ull) && (placed.to <= 310ull);
    EMBED_CALL(cli_pages.release, CliPagesCfg, .key = "wrapped");
}

// a view reported through a file of short lines and then long ones, each step's result recorded, once for the process
static void mixed_ran(void)
{
    char path[CLI_PATH_MAX_LEN];
    CliPagesView view = {1980ull, 20ull, 0ull, 1960ull, 2050ull, 1972ull, 2008ull, 0ull};
    CliPagesPlaced placed = {0ull, 0ull, 0ull, 0ull, 0ull};
    VerbaLinea text = {s_mixed, sizeof(s_mixed), 0ull};
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    if (s_mixed_ran)
    {
        return;
    }
    s_mixed_ran = 1;
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
    s_mixed_steps[0] = EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = "mixed", .view = &view, .placed = &placed) && (placed.from == 2008ull) && ((placed.to - placed.from) >= 1ull) && ((placed.to - placed.from) <= 7ull);
    view.top = 2010ull;
    view.map_from = 2000ull;
    view.map_to = 2040ull;
    view.cached_from = 0ull;
    view.cached_to = 0ull;
    (void)EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = "mixed", .view = &view, .placed = &placed);
    s_mixed_steps[1] = EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = "mixed", .view = &view, .placed = &placed) && (placed.from == 2002ull) && ((placed.to - placed.from) >= 1ull) && ((placed.to - placed.from) <= 7ull);
    view.cached_from = placed.from;
    view.cached_to = placed.to;
    const unsigned long long whole_to = placed.to;
    s_mixed_steps[2] = EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = "mixed", .view = &view, .placed = &placed) && (placed.from == whole_to) && (placed.to > placed.from);
    EMBED_CALL(cli_pages.release, CliPagesCfg, .key = "mixed");
}

// `text` copied into `out`, which is sized for `room`
static void text_stored(char *out, unsigned long long room, const char *text, unsigned long long size)
{
    VerbaLinea line = {out, room, 0ull};
    mmgr_line_put_n(&line, text, size);
    (void)mmgr_line_finish(&line);
}

// a file of 3000 lines edited, read, and saved, what each answered recorded, once for the process
static void edit_ran(void)
{
    static const CliEdit s_edits[2] = {{{10ull, 0ull}, {10ull, 3ull}, "ROW", 3ull}, {{2000ull, 4ull}, {2001ull, 4ull}, "joined\nand ", 11ull}};
    static const CliEdit s_removed[1] = {{{5ull, 0ull}, {7ull, 0ull}, "", 0ull}};
    static const char s_head[] = "row 0\nrow 1\nrow 2\nrow 3\nrow 4\nrow 7\nrow 8\nrow 9\nROW 10\n";
    char path[CLI_PATH_MAX_LEN];
    char shadow[CLI_PATH_MAX_LEN];
    VerbaLinea text = {s_text, sizeof(s_text), 0ull};
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    if (s_edit_ran)
    {
        return;
    }
    s_edit_ran = 1;
    for (unsigned int number = 0u; number < 3000u; number += 1u)
    {
        mmgr_line_put(&text, "row ");
        mmgr_line_decimal(&text, number, 1u);
        mmgr_line_ch(&text, '\n');
    }
    file_written("edit.txt", s_text, text.at, path);
    s_edit_opened = EMBED_CALL(cli_pages_path.open, CliPagesPathCfg, .key = "edit", .path = path, .out = &out, .err_msg = &err_msg);
    (void)shape_indexed("edit");
    const CliEdit *const changes[1] = {s_edits};
    const unsigned int counts[1] = {2u};
    out.at = 0ull;
    s_edited = EMBED_CALL(cli_pages_out_err_msg.edit, CliPagesOutErrMsgCfg, .key = "edit", .changes = changes, .edits = counts, .count_u32 = 1u, .out = &out, .err_msg = &err_msg);
    s_edit_lines = shape_of(out.at).lines;
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_folder, .relative = ".edit.txt.~", .joined = shadow);
    const unsigned long long shadowed = qry_member_file_read(shadow, (unsigned char *)s_text, sizeof(s_text) - 1u);
    s_text[(shadowed == QRY_MEMBER_UNREAD) ? 0u : shadowed] = '\0';
    s_shadow_row = cellul.find(&(CatenaFinitaCfg){.src = s_text, .cap = CLI_CELLS_POOL, .other = "ROW 10\n", .other_cap = CLI_CELLS_POOL}) != NULL;
    s_shadow_joined = cellul.find(&(CatenaFinitaCfg){.src = s_text, .cap = CLI_CELLS_POOL, .other = "row joined\nand 2001\n", .other_cap = CLI_CELLS_POOL}) != NULL;
    s_shadow_other = cellul.find(&(CatenaFinitaCfg){.src = s_text, .cap = CLI_CELLS_POOL, .other = "row 9", .other_cap = CLI_CELLS_POOL}) != NULL;
    lines_joined("edit", 9ull, 12ull, s_edit_within);
    lines_joined("edit", 2000ull, 2002ull, s_edit_join);
    out.at = 0ull;
    (void)EMBED_CALL(cli_pages_path.change, CliPagesPathCfg, .key = "edit", .out = &out, .err_msg = &err_msg);
    text_stored(s_edit_changed, sizeof(s_edit_changed), s_out, out.at);
    const CliEdit *const removed[1] = {s_removed};
    const unsigned int removed_counts[1] = {1u};
    out.at = 0ull;
    (void)EMBED_CALL(cli_pages_out_err_msg.edit, CliPagesOutErrMsgCfg, .key = "edit", .changes = removed, .edits = removed_counts, .count_u32 = 1u, .out = &out, .err_msg = &err_msg);
    lines_joined("edit", 4ull, 7ull, s_edit_removed);
    const CliDocSought sought = {"ROW", 0, 1, 0};
    out.at = 0ull;
    (void)EMBED_CALL(cli_pages_out_err_msg_sought.find, CliPagesOutErrMsgSoughtCfg, .key = "edit", .sought = &sought, .most = 10ull, .out = &out, .err_msg = &err_msg);
    text_stored(s_edit_found, sizeof(s_edit_found), s_out, out.at);
    out.at = 0ull;
    s_edit_saved = EMBED_CALL(cli_pages_path.save, CliPagesPathCfg, .key = "edit", .out = &out, .err_msg = &err_msg);
    const PagesShape saved = shape_of(out.at);
    s_saved_lines = saved.lines;
    s_saved_indexed = saved.indexed;
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_text, sizeof(s_text) - 1u);
    s_text[(size == QRY_MEMBER_UNREAD) ? 0u : size] = '\0';
    s_saved_head = EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = s_text, .other_pointer = s_head, .size = sizeof(s_head) - 1u);
    s_saved_joined = cellul.find(&(CatenaFinitaCfg){.src = s_text, .cap = CLI_CELLS_POOL, .other = "row joined\nand 2001\n", .other_cap = CLI_CELLS_POOL}) != NULL;
    s_saved_end = (size != QRY_MEMBER_UNREAD) && (size >= 9u) && cellul.eq(&(CatenaFinitaCfg){.src = s_text + size - 9u, .other = "row 2999\n", .cap = CLI_CELLS_POOL});
    out.at = 0ull;
    (void)EMBED_CALL(cli_pages_path.change, CliPagesPathCfg, .key = "edit", .out = &out, .err_msg = &err_msg);
    text_stored(s_saved_changed, sizeof(s_saved_changed), s_out, out.at);
    EMBED_CALL(cli_pages.release, CliPagesCfg, .key = "edit");
    s_shadow_after = EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = shadow);
}

// a file of wide lines past a reading's width opened, searched and saved, what each answered recorded, once for the process
static void wide_ran(void)
{
    static char s_wide[32u * 8192u];
    static const char s_last[] = "the last line\n";
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    if (s_wide_ran)
    {
        return;
    }
    s_wide_ran = 1;
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
    s_wide_whole = (282ull * sizeof(s_wide)) + sizeof(s_last) - 1u;
    s_wide_opened = written && EMBED_CALL(cli_pages_path.open, CliPagesPathCfg, .key = "wide", .path = path, .out = &out, .err_msg = &err_msg);
    s_wide_lines = shape_indexed("wide").lines;
    const CliDocSought sought = {"the last", 0, 1, 0};
    out.at = 0ull;
    err_msg.at = 0ull;
    (void)EMBED_CALL(cli_pages_out_err_msg_sought.find, CliPagesOutErrMsgSoughtCfg, .key = "wide", .sought = &sought, .most = 10ull, .out = &out, .err_msg = &err_msg);
    text_stored(s_wide_found, sizeof(s_wide_found), s_out, out.at);
    out.at = 0ull;
    err_msg.at = 0ull;
    s_wide_saved = EMBED_CALL(cli_pages_path.save, CliPagesPathCfg, .key = "wide", .out = &out, .err_msg = &err_msg);
    s_wide_sized = EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = path, .size_pointer = &s_wide_size);
    EMBED_CALL(cli_pages.release, CliPagesCfg, .key = "wide");
}

void setUp(void)
{
    cli_unity_begun();
    if (!s_folder_made)
    {
        s_folder_made = 1;
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

void test_lines_read_from_anywhere_in_the_file(void)
{
    char joined[1024];
    lines_file_opened("read");
    lines_joined("read", 0ull, 2ull, joined);
    TEST_ASSERT_EQUAL_STRING("line 0|line 1", joined);
    lines_joined("read", 2047ull, 2050ull, joined);
    TEST_ASSERT_EQUAL_STRING("line 2047|line 2048|line 2049", joined);
    lines_joined("read", 4999ull, 5001ull, joined);
    TEST_ASSERT_EQUAL_STRING("line 4999|", joined);
    EMBED_CALL(cli_pages.release, CliPagesCfg, .key = "read");
}

void test_find_reaches_the_first_line_of_a_block_of_letters_of_one_byte_each(void)
{
    const CliDocSought sought = {"line 0", 0, 1, 0};
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    lines_file_opened("read");
    (void)EMBED_CALL(cli_pages_out_err_msg_sought.find, CliPagesOutErrMsgSoughtCfg, .key = "read", .sought = &sought, .most = 10ull, .out = &out, .err_msg = &err_msg);
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

void test_long_lines_read_whole_from_anywhere(void)
{
    long_file_opened("long");
    assert_long_lines("long", 0ull, 2ull);
    assert_long_lines("long", 64ull, 68ull);
    assert_long_lines("long", 200ull, 240ull);
    assert_long_lines("long", 390ull, 400ull);
    EMBED_CALL(cli_pages.release, CliPagesCfg, .key = "long");
}

void test_a_view_first_reported_is_placed_its_rows_whole_and_few_more(void)
{
    viewed_ran();
    TEST_ASSERT_TRUE(s_viewed[0]);
}

void test_the_rows_of_a_view_at_rest_read_whole(void)
{
    viewed_ran();
    TEST_ASSERT_TRUE(s_viewed[1]);
}

void test_a_view_that_moved_past_the_pages_lines_is_placed_a_step_after_them_cut_at_its_reach(void)
{
    viewed_ran();
    TEST_ASSERT_TRUE(s_viewed[2]);
}

void test_the_lines_of_a_moving_view_read_cut_their_lengths_whole(void)
{
    viewed_ran();
    TEST_ASSERT_TRUE(s_viewed[3]);
}

void test_a_view_far_from_the_end_of_the_pages_lines_is_placed_nothing(void)
{
    viewed_ran();
    TEST_ASSERT_TRUE(s_viewed[4]);
}

void test_a_view_moving_down_near_the_pages_last_line_is_placed_the_step_after_it(void)
{
    viewed_ran();
    TEST_ASSERT_TRUE(s_viewed[5]);
}

void test_a_view_that_moved_more_than_its_rows_is_placed_the_rows_it_will_reach(void)
{
    viewed_ran();
    TEST_ASSERT_TRUE(s_viewed[6]);
}

void test_a_view_reported_where_it_was_last_is_at_rest_and_placed_its_rows_whole(void)
{
    viewed_ran();
    TEST_ASSERT_TRUE(s_viewed[7]);
}

void test_a_view_at_rest_whose_lines_are_whole_is_placed_the_lines_below_it_stores(void)
{
    viewed_ran();
    TEST_ASSERT_TRUE(s_viewed[8]);
}

void test_a_view_that_moved_up_is_placed_the_rows_it_will_reach_a_step_holding_them(void)
{
    viewed_ran();
    TEST_ASSERT_TRUE(s_viewed[9]);
}

void test_a_wrapping_view_at_rest_is_placed_the_lines_its_rows_need(void)
{
    wrapped_ran();
    TEST_ASSERT_TRUE(s_wrapped[0]);
}

void test_a_wrapping_view_whose_lines_are_held_is_placed_the_step_below_them(void)
{
    wrapped_ran();
    TEST_ASSERT_TRUE(s_wrapped[1]);
}

void test_a_wrapping_view_that_moved_is_placed_the_lines_it_will_reach_a_few_each_side(void)
{
    wrapped_ran();
    TEST_ASSERT_TRUE(s_wrapped[2]);
}

void test_a_step_into_long_lines_is_as_many_as_its_bytes_reach(void)
{
    mixed_ran();
    TEST_ASSERT_TRUE(s_mixed_steps[0]);
}

void test_a_view_at_rest_among_long_lines_is_placed_its_rows_as_far_as_its_bytes_reach(void)
{
    mixed_ran();
    TEST_ASSERT_TRUE(s_mixed_steps[1]);
}

void test_a_view_whose_long_lines_are_whole_is_placed_the_rows_after_them(void)
{
    mixed_ran();
    TEST_ASSERT_TRUE(s_mixed_steps[2]);
}

void test_a_view_moving_far_through_short_lines_is_placed_no_more_than_a_step_each_side(void)
{
    char path[CLI_PATH_MAX_LEN];
    CliPagesView view = {19000ull, 20ull, 64ull, 19000ull, 19020ull, 0ull, 0ull, 0ull};
    CliPagesPlaced placed = {0ull, 0ull, 0ull, 0ull, 0ull};
    VerbaLinea text = {s_rows, sizeof(s_rows), 0ull};
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
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
    (void)EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = "rows", .view = &view, .placed = &placed);
    view.top = 14000ull;
    const int placed_ok = EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = "rows", .view = &view, .placed = &placed);
    EMBED_CALL(cli_pages.release, CliPagesCfg, .key = "rows");
    TEST_ASSERT_TRUE(placed_ok);
    TEST_ASSERT_LESS_OR_EQUAL_UINT64(7992ull, placed.from);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(10028ull, placed.to);
    TEST_ASSERT_LESS_OR_EQUAL_UINT64(2036ull, placed.to - placed.from);
}

void test_lines_no_longer_than_a_cut_read_whole_and_the_read_reports_none_was_cut(void)
{
    unsigned long long made = 64ull;
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    lines_file_opened("short");
    (void)EMBED_CALL(cli_pages_out.read, CliPagesOutCfg, .key = "short", .from = 10ull, .to = 13ull, .cut = &made, .out = &out, .lengths = NULL, .err_msg = &err_msg);
    EMBED_CALL(cli_pages.release, CliPagesCfg, .key = "short");
    CLI_TEST_ASSERT_TEXT("[\"line 10\",\"line 11\",\"line 12\"]", s_out, out.at);
    TEST_ASSERT_EQUAL_UINT64(0ull, made);
}

void test_edits_are_written_the_text_one_line_longer(void)
{
    edit_ran();
    TEST_ASSERT_TRUE(s_edit_opened);
    TEST_ASSERT_TRUE(s_edited);
    TEST_ASSERT_EQUAL_UINT64(3001ull, s_edit_lines);
}

void test_only_the_lines_written_are_stored_in_the_shadow_beside_the_file(void)
{
    edit_ran();
    TEST_ASSERT_TRUE(s_shadow_row);
    TEST_ASSERT_TRUE(s_shadow_joined);
    TEST_ASSERT_FALSE(s_shadow_other);
}

void test_an_edit_within_a_line_is_read(void)
{
    edit_ran();
    TEST_ASSERT_EQUAL_STRING("row 9|ROW 10|row 11", s_edit_within);
}

void test_an_edit_that_joins_lines_is_read(void)
{
    edit_ran();
    TEST_ASSERT_EQUAL_STRING("row joined|and 2001", s_edit_join);
}

void test_the_lines_written_since_the_file_was_read_are_marked(void)
{
    edit_ran();
    TEST_ASSERT_EQUAL_STRING("[[10,11],[2000,2002]]", s_edit_changed);
}

void test_lines_removed_move_the_rest_up(void)
{
    edit_ran();
    TEST_ASSERT_EQUAL_STRING("row 4|row 7|row 8", s_edit_removed);
}

void test_find_reads_the_text_as_it_stands(void)
{
    edit_ran();
    TEST_ASSERT_EQUAL_STRING("[[8,0,3]]", s_edit_found);
}

void test_the_saved_file_is_read_afresh(void)
{
    edit_ran();
    TEST_ASSERT_TRUE(s_edit_saved);
    TEST_ASSERT_EQUAL_UINT64(2999ull, s_saved_lines);
    TEST_ASSERT_TRUE(s_saved_indexed);
}

void test_the_saved_file_contains_the_edits_and_ends_as_it_did(void)
{
    edit_ran();
    TEST_ASSERT_TRUE(s_saved_head);
    TEST_ASSERT_TRUE(s_saved_joined);
    TEST_ASSERT_TRUE(s_saved_end);
}

void test_a_saved_text_has_no_lines_marked(void)
{
    edit_ran();
    TEST_ASSERT_EQUAL_STRING("[]", s_saved_changed);
}

void test_the_shadow_is_removed_once_the_text_is_released(void)
{
    edit_ran();
    TEST_ASSERT_FALSE(s_shadow_after);
}

void test_eight_texts_are_open_at_once_each_released_on_its_own(void)
{
    static const char *const s_keys[] = {"one", "two", "three", "four", "five", "six", "seven", "eight"};
    const unsigned int count = sizeof(s_keys) / sizeof(s_keys[0]);
    char joined[1024];
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char name[CLI_NAME_MAX_LEN];
        char path[CLI_PATH_MAX_LEN];
        VerbaLinea named = {name, sizeof(name), 0ull};
        VerbaLinea out = {s_out, sizeof(s_out), 0ull};
        mmgr_line_put(&named, s_keys[at]);
        mmgr_line_put(&named, ".txt");
        (void)mmgr_line_finish(&named);
        file_written(name, s_keys[at], CLI_UNITY_LENGTH(s_keys[at]), path);
        TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(cli_pages_path.open, CliPagesPathCfg, .key = s_keys[at], .path = path, .out = &out, .err_msg = &err_msg), s_keys[at]);
    }
    lines_joined("eight", 0ull, 1ull, joined);
    TEST_ASSERT_EQUAL_STRING("eight", joined);
    EMBED_CALL(cli_pages.release, CliPagesCfg, .key = "three");
    lines_joined("seven", 0ull, 1ull, joined);
    TEST_ASSERT_EQUAL_STRING("seven", joined);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        EMBED_CALL(cli_pages.release, CliPagesCfg, .key = s_keys[at]);
    }
}

void test_a_file_of_wide_lines_opens_and_is_indexed_whole(void)
{
    wide_ran();
    TEST_ASSERT_TRUE(s_wide_opened);
    TEST_ASSERT_EQUAL_UINT64(9026ull, s_wide_lines);
}

void test_find_reaches_the_line_past_a_block_wider_than_a_reading(void)
{
    wide_ran();
    TEST_ASSERT_EQUAL_STRING("[[9024,0,8]]", s_wide_found);
}

void test_a_save_writes_every_byte_of_every_wide_line(void)
{
    wide_ran();
    TEST_ASSERT_TRUE(s_wide_saved);
    TEST_ASSERT_TRUE(s_wide_sized);
    TEST_ASSERT_EQUAL_UINT64(s_wide_whole, s_wide_size);
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_latex.c: the LaTeX preview: the engine is the one the file names, else the one its packages need, a plain job name is stored and any
// other made preview, the pages share one set of glyphs and each id is the window's own, SyncTeX's boxes are read in points with the lines
// that set them, the log's errors are read with their files and lines, and a file is made into pages with its lines placed
#include "ui/cli/src/editor/latex.h"
#include "ui/cli/src/editor/latex_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_svg[] = "<?xml version=\"1.0\"?>\n<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"595.276\" height=\"841.89pt\" "
                            "viewBox=\"0 0 595.276 841.89\">\n<defs>\n<g id=\"glyph-0-0\"><path d=\"M 0 0\"/></g>\n</defs>\n<pageSet>\n"
                            "<page>\n<use xlink:href=\"#glyph-0-0\" x=\"1\" y=\"2\"/>\n</page>\n<page>\n<g clip-path=\"url(#clip-1)\"/>\n"
                            "</page>\n</pageSet>\n</svg>\n";
static const char s_document[] = "\\documentclass{article}\n\\begin{document}\nThe first page.\n\\newpage\nThe second "
                                 "page.\n\\input{parts/one}\n\\end{document}\n";

// What making a document showed, read once for the process
typedef struct
{
    int ran;
    int made;
    char why[512];
    unsigned int errors;
    unsigned int pages;
    int glyphs_shared;
    int first_line_placed;
    int second_line_placed;
    int part_placed;
    int tree_written;
    unsigned int failed_pages;
    char failed_file[64];
    long long failed_line;
} LatexMade;

static char s_out[1u << 20u];
static const CliJson *s_json;
static LatexMade s_made;

// the job `file` is made under is `wanted`
static void assert_job(const char *wanted, const char *file)
{
    char job[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_latex_file_internal.job_get, CliLatexFileCtx, .file_read = file, .job_out = job);
    TEST_ASSERT_EQUAL_STRING(wanted, job);
}

// the engine of `text` is `wanted`
static void assert_engine(const char *wanted, const char *text)
{
    TEST_ASSERT_EQUAL_STRING(wanted, EMBED_CALL(cli_latex.engine_get, CliLatexCfg, .text = text, .length = CLI_UNITY_LENGTH(text)));
}

// the svg's pages written and parsed: the root's node
static unsigned int pages_read(const char *svg, unsigned long long size)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    mmgr_line_ch(&out, '{');
    TEST_ASSERT_TRUE(cli_latex_lock());
    const int written = EMBED_CALL(cli_latex_size_internal.tex_pages_write, CliLatexSizeCtx, .svg = svg, .size = size, .out = &out);
    cli_latex_release();
    TEST_ASSERT_TRUE(written);
    mmgr_line_ch(&out, '}');
    unsigned int root = CLI_JSON_NONE;
    s_json = cli_unity_parsed(s_out, out.at, &root);
    return root;
}

// a whole number at a node of the shared tree
static long long whole_at(unsigned int node)
{
    long long value = 0ll;
    for (unsigned long long at = 0ull; EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = s_json, .node = node, .kind = CFG_JSON_NUMBER) && (at < s_json->node[node].length); at += 1ull)
    {
        value = value * 10ll + (long long)(s_json->text[s_json->node[node].text + at] - '0');
    }
    return value;
}

// a number member of a node of the shared tree
static double number_member(unsigned int node, const char *name)
{
    char text[64];
    VerbaLinea line = {text, sizeof(text), 0ull};
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = s_json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = node, .name = name), .out = &line, .indent = 0u, .sorted = 0);
    double value = 0.0;
    (void)EMBED_CALL(cli_latex_text_length_value_internal.tex_double, CliLatexTextLengthValueCtx, .text = text, .length = line.at, .value = &value);
    return value;
}

// SyncTeX's boxes of two pages read: the array's node
static unsigned int boxes_read(void)
{
    static const char s_sync[] = "SyncTeX "
                                 "Version:1\nInput:1:C:\\made\\paper.tex\nInput:2:C:\\tex\\article.cls\nInput:3:C:\\tree\\doc\\parts\\one.tex\n"
                                 "Output:pdf\nMagnification:1000\nUnit:1\nX Offset:0\nY "
                                 "Offset:0\nContent:\n!413\n{1\n[1,7:4736286,46220574:26673152,"
                                 "41484288,0\n(1,5:8799518,10300473:22609920,455111,127431\nx1,5:10179418,10300473\ng1,4:10397871,10300473\n"
                                 "x1,4:13314231,10300473\nx2,9:13532684,10300473\ng1,5:31409438,10300473\n)\n]\n}1\n{2\n(3,2:8799518,8865054:"
                                 "22609920,647495,0\nx3,1:9330359,8865054\n)\n}2\n";
    CliTexNames names;
    EMBED_CALL(cli_latex_names_root_internal.tex_names_init, CliLatexNamesRootCtx, .names_out = &names, .root = "C:/tree", .dir = "C:/made", .job = "paper", .path = "doc/paper.tex");
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    TEST_ASSERT_TRUE(cli_latex_lock());
    EMBED_CALL(cli_latex_names_internal.tex_boxes_write, CliLatexNamesCtx, .sync = s_sync, .size = sizeof(s_sync) - 1u, .names = &names, .out = &out);
    cli_latex_release();
    unsigned int boxes = CLI_JSON_NONE;
    s_json = cli_unity_parsed(s_out, out.at, &boxes);
    return boxes;
}

// a document with an input file made, and one that fails, once for the process, what they showed in s_made
static void document_made(void)
{
    static const char s_failing[] = "\\documentclass{article}\n\\begin{document}\n\\nosuchcommand\n\\end{document}\n";
    if (s_made.ran)
    {
        return;
    }
    s_made.ran = 1;
    char root[CLI_PATH_MAX_LEN];
    char store_dir[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    cli_unity_folder("latex_make", root);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = "doc/parts", .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = "store", .joined = store_dir);
    cli_unity_file(root, "doc/parts/one.tex", "Text of the part.\n");
    VerbaLinea err_msg = {s_made.why, sizeof(s_made.why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    s_made.made = EMBED_CALL(cli_latex.make_in, CliLatexCfg, .root = root, .path = "doc/paper.tex", .text = s_document, .length = sizeof(s_document) - 1u, .out_dir = store_dir, .out = &out, .err_msg = &err_msg);
    if (!s_made.made)
    {
        (void)mmgr_line_finish(&err_msg);
        return;
    }
    unsigned int made = CLI_JSON_NONE;
    s_json = cli_unity_parsed(s_out, out.at, &made);
    const unsigned int boxes = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = made, .name = "boxes");
    s_made.errors = s_json->node[EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = made, .name = "errors")].count;
    s_made.pages = s_json->node[EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = made, .name = "pages")].count;
    s_made.glyphs_shared = cellul.find(&(CatenaFinitaCfg){.src = cli_unity_member_text(made, "defs"), .cap = CLI_CELLS_POOL, .other = "tex-glyph", .other_cap = CLI_CELLS_POOL}) != NULL;
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = boxes, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int box = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = boxes, .index = at);
        const long long page = whole_at(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = box, .name = "page"));
        const int paper = cellul.eq(&(CatenaFinitaCfg){.src = cli_unity_member_text(box, "file"), .other = "doc/paper.tex", .cap = CLI_CELLS_POOL});
        const unsigned int lines = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = box, .name = "lines");
        for (unsigned int mark = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = lines, .index = mark) != CLI_JSON_NONE; mark += 1u)
        {
            const long long line = whole_at(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = lines, .index = mark), .index = 1u));
            s_made.first_line_placed |= paper && (page == 1ll) && (line == 3ll);
            s_made.second_line_placed |= paper && (page == 2ll) && (line == 5ll);
        }
        s_made.part_placed |= (page == 2ll) && cellul.eq(&(CatenaFinitaCfg){.src = cli_unity_member_text(box, "file"), .other = "doc/parts/one.tex", .cap = CLI_CELLS_POOL});
    }
    char aux[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = "doc/paper.pdf", .joined = path);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = "doc/paper.aux", .joined = aux);
    s_made.tree_written = EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path) || EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = aux);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = "doc/paper.tex", .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = s_document, .size = sizeof(s_document) - 1u));
    out.at = 0ull;
    (void)EMBED_CALL(cli_latex.make_in, CliLatexCfg, .root = root, .path = "doc/paper.tex", .text = s_failing, .length = sizeof(s_failing) - 1u, .out_dir = store_dir, .out = &out, .err_msg = &err_msg);
    s_json = cli_unity_parsed(s_out, out.at, &made);
    const unsigned int error = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = made, .name = "errors"), .index = 0u);
    s_made.failed_pages = s_json->node[EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = made, .name = "pages")].count;
    const char *const failed = cli_unity_member_text(error, "file");
    memor.cpy(&(MemoriaCfg){.dst = s_made.failed_file, .src = failed, .bytes = CLI_UNITY_LENGTH(failed) + 1ull});
    s_made.failed_line = whole_at(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = error, .name = "line"));
}

// the document made, the test ignored where the machine has no engine
static void engine_required(void)
{
    document_made();
    if (!s_made.made)
    {
        TEST_IGNORE_MESSAGE("the machine has no LaTeX engine");
    }
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_the_engine_the_file_names(void)
{
    assert_engine("xelatex", "% !TEX program = xelatex\n\\documentclass{article}");
}

void test_the_engine_the_file_names_in_the_other_form(void)
{
    assert_engine("lualatex", "%!TeX TS-program = LuaLaTeX\n");
}

void test_fontspec_needs_lualatex(void)
{
    assert_engine("lualatex", "\\documentclass{article}\n\\usepackage[no-math]{fontspec}");
}

void test_unicode_math_needs_lualatex(void)
{
    assert_engine("lualatex", "\\documentclass{article}\n\\usepackage{amsmath, unicode-math}");
}

void test_a_package_in_a_comment_is_not_used(void)
{
    assert_engine("pdflatex", "\\documentclass{article}\n% \\usepackage{fontspec}\n\\usepackage{amsmath}");
}

void test_a_line_after_the_first_command_names_no_engine(void)
{
    assert_engine("pdflatex", "\\documentclass{article}\n% !TEX program = xelatex");
}

void test_a_plain_job_name_is_stored(void)
{
    assert_job("paper_2-b", "a/paper_2-b.tex");
}

void test_a_name_with_a_space_is_made_preview(void)
{
    assert_job("preview", "a/my paper.tex");
}

void test_a_name_past_ascii_is_made_preview(void)
{
    assert_job("preview", "a/\xC3\xA9.tex");
}

void test_the_glyphs_are_shared(void)
{
    CLI_TEST_ASSERT_CONTAINS("id=\"tex-glyph-0-0\"", cli_unity_member_text(pages_read(s_svg, sizeof(s_svg) - 1u), "defs"));
}

void test_each_page_has_its_size(void)
{
    const unsigned int root = pages_read(s_svg, sizeof(s_svg) - 1u);
    const unsigned int pages = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = root, .name = "pages");
    const unsigned int first = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = pages, .index = 0u);
    char size[64];
    VerbaLinea line = {size, sizeof(size), 0ull};
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = s_json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = first, .name = "width"), .out = &line, .indent = 0u, .sorted = 0);
    mmgr_line_ch(&line, ' ');
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = s_json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = first, .name = "height"), .out = &line, .indent = 0u, .sorted = 0);
    (void)mmgr_line_finish(&line);
    TEST_ASSERT_EQUAL_UINT(2u, s_json->node[pages].count);
    TEST_ASSERT_EQUAL_STRING("595.276 841.89", size);
}

void test_a_pages_ids_are_the_windows_own(void)
{
    const unsigned int root = pages_read(s_svg, sizeof(s_svg) - 1u);
    const unsigned int pages = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = root, .name = "pages");
    CLI_TEST_ASSERT_CONTAINS("href=\"#tex-glyph-0-0\"", cli_unity_member_text(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = pages, .index = 0u), "svg"));
}

void test_every_reference_uses_the_windows_ids(void)
{
    const unsigned int root = pages_read(s_svg, sizeof(s_svg) - 1u);
    const unsigned int pages = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = root, .name = "pages");
    CLI_TEST_ASSERT_CONTAINS("url(#tex-clip-1)", cli_unity_member_text(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = pages, .index = 1u), "svg"));
}

void test_a_single_page_has_no_page_set(void)
{
    static const char s_single[] = "<svg width=\"10\" height=\"20\"><defs></defs><g/></svg>";
    const unsigned int root = pages_read(s_single, sizeof(s_single) - 1u);
    const unsigned int pages = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = root, .name = "pages");
    TEST_ASSERT_EQUAL_UINT(1u, s_json->node[pages].count);
    TEST_ASSERT_EQUAL_STRING("<g/>", cli_unity_member_text(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = pages, .index = 0u), "svg"));
}

void test_a_box_is_read_with_its_page_and_file(void)
{
    const unsigned int boxes = boxes_read();
    const unsigned int first = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = boxes, .index = 0u);
    TEST_ASSERT_EQUAL_UINT(2u, s_json->node[boxes].count);
    TEST_ASSERT_EQUAL_INT64(1ll, whole_at(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = first, .name = "page")));
    TEST_ASSERT_EQUAL_STRING("doc/paper.tex", cli_unity_member_text(first, "file"));
}

void test_a_box_has_one_mark_where_the_line_changes_and_none_from_the_class(void)
{
    static const long long s_lines[3] = {5ll, 4ll, 5ll};
    const unsigned int first = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = boxes_read(), .index = 0u);
    const unsigned int lines = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = first, .name = "lines");
    TEST_ASSERT_EQUAL_UINT(3u, s_json->node[lines].count);
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        TEST_ASSERT_EQUAL_INT64(s_lines[at], whole_at(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = lines, .index = at), .index = 1u)));
    }
}

void test_a_boxs_left_is_in_points(void)
{
    const unsigned int first = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = boxes_read(), .index = 0u);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 8799518.0 * (72.0 / 72.27 / 65536.0), number_member(first, "x"));
}

void test_a_boxs_top_is_the_baseline_less_the_height(void)
{
    const unsigned int first = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = boxes_read(), .index = 0u);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, (10300473.0 - 455111.0) * (72.0 / 72.27 / 65536.0), number_member(first, "y"));
}

void test_a_boxs_height_is_the_height_and_the_depth(void)
{
    const unsigned int first = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = boxes_read(), .index = 0u);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, (455111.0 + 127431.0) * (72.0 / 72.27 / 65536.0), number_member(first, "h"));
}

void test_a_file_the_text_inputs_is_the_trees(void)
{
    const unsigned int second = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = boxes_read(), .index = 1u);
    TEST_ASSERT_EQUAL_INT64(2ll, whole_at(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = second, .name = "page")));
    TEST_ASSERT_EQUAL_STRING("doc/parts/one.tex", cli_unity_member_text(second, "file"));
    TEST_ASSERT_EQUAL_UINT(1u, s_json->node[EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = second, .name = "lines")].count);
}

void test_the_logs_errors_are_read_with_their_files_and_lines(void)
{
    static const char s_log[] = "This is pdfTeX\n./paper.tex:12: Undefined control sequence.\nl.12 \\foo\n"
                                "C:/tree/doc/parts/one.tex:3: Missing $ inserted.\nC:/tex/x.sty:40: LaTeX Error: Option "
                                "clash.\npaper.tex:12:  ==> Fatal error occurred, no output PDF file produced!\n";
    CliTexNames names;
    EMBED_CALL(cli_latex_names_root_internal.tex_names_init, CliLatexNamesRootCtx, .names_out = &names, .root = "C:/tree", .dir = "C:/store/abc", .job = "paper", .path = "doc/paper.tex");
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    mmgr_line_ch(&out, '[');
    const unsigned int count = EMBED_CALL(cli_latex_names_log_internal.tex_errors_write, CliLatexNamesLogCtx, .log = s_log, .size = sizeof(s_log) - 1u, .names = &names, .out = &out);
    mmgr_line_ch(&out, ']');
    TEST_ASSERT_EQUAL_UINT(3u, count);
    CLI_TEST_ASSERT_TEXT("[{\"file\":\"doc/paper.tex\",\"line\":12,\"text\":\"Undefined control sequence.\"},"
                         "{\"file\":\"doc/parts/one.tex\",\"line\":3,\"text\":\"Missing $ inserted.\"},"
                         "{\"file\":\"C:/tex/x.sty\",\"line\":40,\"text\":\"LaTeX Error: Option clash.\"}]",
                         s_out, out.at);
}

void test_a_make_with_no_engine_reports_it(void)
{
    document_made();
    if (s_made.made)
    {
        TEST_IGNORE_MESSAGE("the machine has a LaTeX engine");
    }
    CLI_TEST_ASSERT_CONTAINS("is not found: File, Toolchains", s_made.why);
}

void test_a_make_has_no_errors_and_two_pages(void)
{
    engine_required();
    TEST_ASSERT_EQUAL_UINT(0u, s_made.errors);
    TEST_ASSERT_EQUAL_UINT(2u, s_made.pages);
}

void test_a_makes_pages_share_their_glyphs(void)
{
    engine_required();
    TEST_ASSERT_TRUE(s_made.glyphs_shared);
}

void test_each_line_is_placed_on_its_page(void)
{
    engine_required();
    TEST_ASSERT_TRUE(s_made.first_line_placed);
    TEST_ASSERT_TRUE(s_made.second_line_placed);
}

void test_an_input_part_beside_the_file_is_placed(void)
{
    engine_required();
    TEST_ASSERT_TRUE(s_made.part_placed);
}

void test_nothing_is_written_into_the_tree(void)
{
    engine_required();
    TEST_ASSERT_FALSE(s_made.tree_written);
}

void test_a_failed_make_gives_its_errors_and_no_pages(void)
{
    engine_required();
    TEST_ASSERT_EQUAL_UINT(0u, s_made.failed_pages);
    TEST_ASSERT_EQUAL_STRING("doc/paper.tex", s_made.failed_file);
    TEST_ASSERT_EQUAL_INT64(3ll, s_made.failed_line);
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// latex_test.c: the LaTeX preview's check: the engine is the one the file names, else the one its packages need; a
// plain name is kept and any other made as preview; the pages share one set of glyphs and each id is the window's own;
// SyncTeX's boxes are read in points with the lines that set them; the log's errors are read with their files and
// lines; and a file is made into pages with its lines placed, a failed one giving its errors
#include "../../../../../ui/cli/src/editor/latex.h"
#include "../../../../../ui/cli/src/editor/latex_internal.h"
#include "cli_check.h"

static char s_out[1u << 20u];

// the engine of `text`, checked against `wanted`
static void latex_engine(const char *text, const char *wanted, const char *what)
{
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = cli_latex_engine_of(text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL})),
                  .other = wanted,
                  .cap = CLI_CELLS_POOL}),
              what);
}

// the job `file` is made under, checked against `wanted`
static void latex_job(const char *file, const char *wanted, const char *what)
{
    char job[CLI_PATH_LONGEST];
    cli_latex_job_of(file, job);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = job, .other = wanted, .cap = CLI_CELLS_POOL}), what);
}

// the pages share one set of glyphs and each id is the window's own
static void latex_pages(void)
{
    static const char s_svg[] =
        "<?xml version=\"1.0\"?>\n<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"595.276\" height=\"841.89pt\" "
        "viewBox=\"0 0 595.276 841.89\">\n<defs>\n<g id=\"glyph-0-0\"><path d=\"M 0 0\"/></g>\n</defs>\n<pageSet>\n"
        "<page>\n<use xlink:href=\"#glyph-0-0\" x=\"1\" y=\"2\"/>\n</page>\n<page>\n<g clip-path=\"url(#clip-1)\"/>\n"
        "</page>\n</pageSet>\n</svg>\n";
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_line_character(&out, '{');
    cli_check(cli_latex_held(), "a make's room is taken");
    cli_check(cli_tex_pages_written(s_svg, sizeof(s_svg) - 1u, &out), "the pages are written");
    cli_line_character(&out, '}');
    unsigned int root = CLI_JSON_NONE;
    const CliJson *json = cli_check_parsed(s_out, out.at, &root);
    const unsigned int pages = cli_json_member(json, root, "pages");
    const unsigned int first = cli_json_item(json, pages, 0u);
    char size[64];
    CliLine line = {size, sizeof(size), 0ull};
    cli_json_write(json, cli_json_member(json, first, "width"), &line, 0u, 0);
    cli_line_character(&line, ' ');
    cli_json_write(json, cli_json_member(json, first, "height"), &line, 0u, 0);
    (void)cli_line_closed(&line);
    cli_check(cellul.find(&(CatenaFinitaCfg){.src = cli_check_member_text(root, "defs"),
                                             .cap = CLI_CELLS_POOL,
                                             .other = "id=\"tex-glyph-0-0\"",
                                             .other_cap = CLI_CELLS_POOL}) != NULL,
              "the glyphs are shared");
    cli_check((json->node[pages].count == 2u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = size, .other = "595.276 841.89", .cap = CLI_CELLS_POOL})),
              "each page has its size");
    cli_check(cellul.find(&(CatenaFinitaCfg){.src = cli_check_member_text(first, "svg"),
                                             .cap = CLI_CELLS_POOL,
                                             .other = "href=\"#tex-glyph-0-0\"",
                                             .other_cap = CLI_CELLS_POOL}) != NULL,
              "and its ids are the window's own");
    cli_check(cellul.find(&(CatenaFinitaCfg){.src = cli_check_member_text(cli_json_item(json, pages, 1u), "svg"),
                                             .cap = CLI_CELLS_POOL,
                                             .other = "url(#tex-clip-1)",
                                             .other_cap = CLI_CELLS_POOL}) != NULL,
              "in every reference");
    out.at = 0ull;
    cli_line_character(&out, '{');
    (void)cli_tex_pages_written("<svg width=\"10\" height=\"20\"><defs></defs><g/></svg>", 52ull, &out);
    cli_latex_released();
    cli_line_character(&out, '}');
    json = cli_check_parsed(s_out, out.at, &root);
    const unsigned int one = cli_json_member(json, root, "pages");
    cli_check((json->node[one].count == 1u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(cli_json_item(json, one, 0u), "svg"),
                                                .other = "<g/>",
                                                .cap = CLI_CELLS_POOL})),
              "a single page has no page set");
}

// a whole number member of a node of the shared tree
static long long latex_whole(const CliJson *json, unsigned int node)
{
    long long value = 0ll;
    for (unsigned long long at = 0ull; cli_json_is(json, node, CFG_JSON_NUMBER) && (at < json->node[node].length);
         at += 1ull)
    {
        value = value * 10ll + (long long)(json->text[json->node[node].text + at] - '0');
    }
    return value;
}

// a number member of a node of the shared tree, read
static double latex_number(const CliJson *json, unsigned int node, const char *name)
{
    char text[64];
    CliLine line = {text, sizeof(text), 0ull};
    cli_json_write(json, cli_json_member(json, node, name), &line, 0u, 0);
    double value = 0.0;
    (void)cli_tex_double(text, line.at, &value);
    return value;
}

// SyncTeX's boxes are read in points with the lines that set them
static void latex_boxes(void)
{
    static const char s_sync[] =
        "SyncTeX "
        "Version:1\nInput:1:C:\\made\\paper.tex\nInput:2:C:\\tex\\article.cls\nInput:3:C:\\tree\\doc\\parts\\one.tex\n"
        "Output:pdf\nMagnification:1000\nUnit:1\nX Offset:0\nY "
        "Offset:0\nContent:\n!413\n{1\n[1,7:4736286,46220574:26673152,"
        "41484288,0\n(1,5:8799518,10300473:22609920,455111,127431\nx1,5:10179418,10300473\ng1,4:10397871,10300473\n"
        "x1,4:13314231,10300473\nx2,9:13532684,10300473\ng1,5:31409438,10300473\n)\n]\n}1\n{2\n(3,2:8799518,8865054:"
        "22609920,647495,0\nx3,1:9330359,8865054\n)\n}2\n";
    CliTexNames names;
    cli_tex_names_made(&names, "C:/tree", "C:/made", "paper", "doc/paper.tex");
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_check(cli_latex_held(), "a make's room is taken for its boxes");
    cli_tex_boxes_written(s_sync, sizeof(s_sync) - 1u, &names, &out);
    cli_latex_released();
    unsigned int boxes = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_out, out.at, &boxes);
    const unsigned int first = cli_json_item(json, boxes, 0u);
    cli_check((json->node[boxes].count == 2u) && (latex_whole(json, cli_json_member(json, first, "page")) == 1ll) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = cli_check_member_text(first, "file"), .other = "doc/paper.tex", .cap = CLI_CELLS_POOL})),
              "a box is read with its page and its file");
    const unsigned int lines = cli_json_member(json, first, "lines");
    int marks = json->node[lines].count == 3u;
    static const long long s_lines[3] = {5ll, 4ll, 5ll};
    for (unsigned int at = 0u; marks && (at < 3u); at += 1u)
    {
        marks = latex_whole(json, cli_json_item(json, cli_json_item(json, lines, at), 1u)) == s_lines[at];
    }
    cli_check(marks, "one mark where the line changes, none from the class");
    const double point = 72.0 / 72.27 / 65536.0;
    const double x = latex_number(json, first, "x") - 8799518.0 * point;
    const double y = latex_number(json, first, "y") - (10300473.0 - 455111.0) * point;
    const double h = latex_number(json, first, "h") - (455111.0 + 127431.0) * point;
    cli_check((x < 0.01) && (x > -0.01), "a box's left in points");
    cli_check((y < 0.01) && (y > -0.01), "the top, the baseline less the height");
    cli_check((h < 0.01) && (h > -0.01), "the height and the depth");
    const unsigned int second = cli_json_item(json, boxes, 1u);
    cli_check(
        (latex_whole(json, cli_json_member(json, second, "page")) == 2ll) &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = cli_check_member_text(second, "file"), .other = "doc/parts/one.tex", .cap = CLI_CELLS_POOL})) &&
            (json->node[cli_json_member(json, second, "lines")].count == 1u),
        "a file the text inputs is the tree's");
}

// the log's errors are read with their files and lines
static void latex_errors(void)
{
    static const char s_log[] =
        "This is pdfTeX\n./paper.tex:12: Undefined control sequence.\nl.12 \\foo\n"
        "C:/tree/doc/parts/one.tex:3: Missing $ inserted.\nC:/tex/x.sty:40: LaTeX Error: Option "
        "clash.\npaper.tex:12:  ==> Fatal error occurred, no output PDF file produced!\n";
    CliTexNames names;
    cli_tex_names_made(&names, "C:/tree", "C:/kept/abc", "paper", "doc/paper.tex");
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_line_character(&out, '[');
    const unsigned int count = cli_tex_errors_written(s_log, sizeof(s_log) - 1u, &names, &out);
    cli_line_character(&out, ']');
    cli_check_text(s_out, out.at,
                   "[{\"file\":\"doc/paper.tex\",\"line\":12,\"text\":\"Undefined control sequence.\"},"
                   "{\"file\":\"doc/parts/one.tex\",\"line\":3,\"text\":\"Missing $ inserted.\"},"
                   "{\"file\":\"C:/tex/x.sty\",\"line\":40,\"text\":\"LaTeX Error: Option clash.\"}]",
                   "the log's errors are read with their files and lines");
    cli_check(count == 3u, "each once");
}

// a file is made into pages with its lines placed, and a failed one gives its errors
static void latex_make(void)
{
    static const char s_text[] = "\\documentclass{article}\n\\begin{document}\nThe first page.\n\\newpage\nThe second "
                                 "page.\n\\input{parts/one}\n\\end{document}\n";
    char root[CLI_PATH_LONGEST];
    char kept[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    cli_check_folder("latex_make", root);
    cli_full(root, "doc/parts", path);
    (void)query_folder_made(path);
    cli_full(root, "kept", kept);
    cli_full(root, "doc/parts/one.tex", path);
    (void)query_file_written(path, "Text of the part.\n", 18ull);
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine out = {s_out, sizeof(s_out), 0ull};
    if (!cli_latex_make_in(root, "doc/paper.tex", s_text, sizeof(s_text) - 1u, kept, &out, &said))
    {
        (void)cli_line_closed(&said);
        cli_check(cellul.find(&(CatenaFinitaCfg){.src = why,
                                                 .cap = CLI_CELLS_POOL,
                                                 .other = "is not found: File, Toolchains",
                                                 .other_cap = CLI_CELLS_POOL}) != NULL,
                  "a make with no engine says so");
        return;
    }
    unsigned int made = CLI_JSON_NONE;
    const CliJson *json = cli_check_parsed(s_out, out.at, &made);
    const unsigned int boxes = cli_json_member(json, made, "boxes");
    cli_check(json->node[cli_json_member(json, made, "errors")].count == 0u, "the make has no errors");
    cli_check(json->node[cli_json_member(json, made, "pages")].count == 2u, "and two pages");
    cli_check(cellul.find(&(CatenaFinitaCfg){.src = cli_check_member_text(made, "defs"),
                                             .cap = CLI_CELLS_POOL,
                                             .other = "tex-glyph",
                                             .other_cap = CLI_CELLS_POOL}) != NULL,
              "the pages share their glyphs");
    int line_three = 0;
    int line_five = 0;
    int part = 0;
    for (unsigned int at = 0u; cli_json_item(json, boxes, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int box = cli_json_item(json, boxes, at);
        const long long page = latex_whole(json, cli_json_member(json, box, "page"));
        const int paper = cellul.eq(&(CatenaFinitaCfg){
            .src = cli_check_member_text(box, "file"), .other = "doc/paper.tex", .cap = CLI_CELLS_POOL});
        const unsigned int lines = cli_json_member(json, box, "lines");
        for (unsigned int mark = 0u; cli_json_item(json, lines, mark) != CLI_JSON_NONE; mark += 1u)
        {
            const long long line = latex_whole(json, cli_json_item(json, cli_json_item(json, lines, mark), 1u));
            line_three |= paper && (page == 1ll) && (line == 3ll);
            line_five |= paper && (page == 2ll) && (line == 5ll);
        }
        part |= (page == 2ll) &&
                (cellul.eq(&(CatenaFinitaCfg){
                    .src = cli_check_member_text(box, "file"), .other = "doc/parts/one.tex", .cap = CLI_CELLS_POOL}));
    }
    cli_check(line_three && line_five, "each line is placed on its page");
    cli_check(part, "the part input from beside the file");
    cli_full(root, "doc/paper.pdf", path);
    char aux[CLI_PATH_LONGEST];
    cli_full(root, "doc/paper.aux", aux);
    cli_check(!query_is_file(path) && !query_is_file(aux), "nothing written into the tree");
    static const char s_failing[] = "\\documentclass{article}\n\\begin{document}\n\\nosuchcommand\n\\end{document}\n";
    cli_full(root, "doc/paper.tex", path);
    (void)query_file_written(path, s_text, sizeof(s_text) - 1u);
    out.at = 0ull;
    (void)cli_latex_make_in(root, "doc/paper.tex", s_failing, sizeof(s_failing) - 1u, kept, &out, &said);
    json = cli_check_parsed(s_out, out.at, &made);
    const unsigned int error = cli_json_item(json, cli_json_member(json, made, "errors"), 0u);
    cli_check((json->node[cli_json_member(json, made, "pages")].count == 0u) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = cli_check_member_text(error, "file"), .other = "doc/paper.tex", .cap = CLI_CELLS_POOL})) &&
                  (latex_whole(json, cli_json_member(json, error, "line")) == 3ll),
              "a failed make gives its errors and no pages");
}

int main(void)
{
    latex_engine("% !TEX program = xelatex\n\\documentclass{article}", "xelatex", "the engine the file names");
    latex_engine("%!TeX TS-program = LuaLaTeX\n", "lualatex", "in either way it is written");
    latex_engine("\\documentclass{article}\n\\usepackage[no-math]{fontspec}", "lualatex", "fontspec needs LuaLaTeX");
    latex_engine("\\documentclass{article}\n\\usepackage{amsmath, unicode-math}", "lualatex", "and unicode-math");
    latex_engine("\\documentclass{article}\n% \\usepackage{fontspec}\n\\usepackage{amsmath}", "pdflatex",
                 "a package in a comment is none");
    latex_engine("\\documentclass{article}\n% !TEX program = xelatex", "pdflatex",
                 "a line after the first command names nothing");
    latex_job("a/paper_2-b.tex", "paper_2-b", "a plain name is kept");
    latex_job("a/my paper.tex", "preview", "and any other made as preview");
    latex_job("a/\xC3\xA9.tex", "preview", "a name past ASCII too");
    latex_pages();
    latex_boxes();
    latex_errors();
    latex_make();
    return cli_checked();
}

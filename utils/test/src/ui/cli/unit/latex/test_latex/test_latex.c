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
    int run_result;
    int made;
    char why[512];
    unsigned int errors;
    unsigned int pages;
    int glyphs_shared;
    int first_line_positioned;
    int second_line_positioned;
    int part_positioned;
    int tree_written;
    unsigned int failed_pages;
    char failed_file[64];
    long long failed_line;
} LatexMade;

static char s_out[1u << 20u];
static const CliJson *s_json;
static LatexMade s_created;

// the job `file` is made under is `required`
static void assert_job(const char *required, const char *file)
{
    char job[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_latex_file_internal.job_get, CliLatexFileCtx, .file_read = file, .job_out = job);
    TEST_ASSERT_EQUAL_STRING(required, job);
}

// the engine of `text` is `required`
static void assert_engine(const char *required, const char *text)
{
    TEST_ASSERT_EQUAL_STRING(required, EMBED_CALL(cli_latex.engine_get, CliLatexCfg, .text = text, .length = CLI_UNITY_LENGTH(text)));
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

void test_a_name_with_a_space_is_created_preview(void)
{
    assert_job("preview", "a/my paper.tex");
}

void test_a_name_end_ascii_is_created_preview(void)
{
    assert_job("preview", "a/\xC3\xA9.tex");
}


// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_symbols.c: Python's, Rust's, scripts', C's and the other languages' declarations are read; scores rank the name itself first; a first reading reports
// how far it has gone and a stop stores what it read; the tree is read and a file read again once it changes; a call's signature is read from the tree; a
// name's definition in effect is found through includes, conditions and the compilation database's defines; a slice is cut for a view; and a name's usages,
// definition, completions, hints, spans, bracket pair and call hierarchy are read
#include "ui/cli/src/code/symbols.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// the tree a group of cases reads: the walk of a tree is one for the process, each group walking its own once on arriving
typedef enum
{
    TREE_NONE = 0,
    TREE_SIGNATURE,
    TREE_EFFECT,
    TREE_SLICE,
    TREE_SLICES,
    TREE_USAGES
} SymbolsTree;

static const char s_use_text[] = "#include \"shape.h\"\nint total(int n)\n{\n    int w = area(n) + SIDES;\n    return w + area(1);\n}\n";
static const char s_indent_text[] = "def total(n):\n    w = area(n) + 1\n    return w\n\nx = 2\n";
static const unsigned int s_slice_openers[2] = {6u, 10u};

static const char *const s_area_usages[5] = {"src/u/use.c:3:12-16", "src/u/use.c:4:15-19", "src/u/shape.h:2:4-8", "src/u/area.c:1:4-8", NULL};
static const char *const s_sides_usages[4] = {"src/u/use.c:3:22-27", "src/u/shape.h:3:8-13", "src/u/area.c:3:21-26", NULL};
static const char *const s_parameter_usages[2] = {"src/u/area.c:1:13-14", "src/u/area.c:3:17-18"};
static const char *const s_local_usages[2] = {"src/u/use.c:3:8-9", "src/u/use.c:4:11-12"};

static char s_out[1u << 20u];
static char s_error[256];
static SymbolsTree s_tree;

static int s_stop_run_result;
static int s_stop_seen;
static unsigned long long s_stop_total;
static unsigned long long s_stop_done;
static int s_stop_ended;
static int s_stop_unwalked;
static unsigned int s_stop_found;
static unsigned int s_stop_answered;

static int s_slices_run_result;
static int s_slices_cut;

// a declaration written after the others, `name kind line` joined by `|`
static void declaration_recorded(const CliDeclaredCfg *args)
{
    VerbaLinea *const out = args->context;
    mmgr_line_put(out, (out->at > 0ull) ? "|" : "");
    mmgr_line_put_n(out, args->name, args->length);
    mmgr_line_ch(out, ' ');
    mmgr_line_put(out, args->kind);
    mmgr_line_ch(out, ' ');
    mmgr_line_decimal(out, args->line, 1u);
}

// the declarations of `text` read as the file `path`, asserted against `required`
static void assert_declarations(const char *path, const char *text, const char *required)
{
    char out[1024];
    VerbaLinea line = {out, sizeof(out), 0ull};
    EMBED_CALL(cli_symbols_path_text_size.declare, CliSymbolsPathTextSizeCfg, .path = path, .text = text, .size = CLI_UNITY_LENGTH(text), .each_declared = declaration_recorded, .context = &line);
    CLI_TEST_ASSERT_TEXT(required, out, line.at);
}

// the score `query` assigns `name`, -1 where it does not name it
static long long score_of(const char *query, const char *name)
{
    long long points = 0ll;
    return EMBED_CALL(cli_symbols_query.score, CliSymbolsQueryCfg, .query = query, .name = name, .name_length = CLI_UNITY_LENGTH(name), .points = &points) ? points : -1ll;
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_pythons_declarations_are_read(void)
{
    assert_declarations("x.py",
                        "import os\nLIMIT = 3\nlimit = 4\nX: int = 5\nclass Page:\n    def read(self):\n        pass\nasync def "
                        "go():\n    if a == b:\n        pass\n",
                        "LIMIT constant 1|X constant 3|Page class 4|read function 5|go function 7");
}

void test_rusts_declarations_are_read(void)
{
    assert_declarations("x.rs",
                        "pub(crate) fn alpha() {}\npub struct Beta;\nconst GAMMA: u32 = 1;\nmacro_rules! delta {\n}\npub async "
                        "unsafe fn epsilon() {}\nmod zeta;\nimpl Beta {\nextern \"C\" fn eta() {}\nstatic mut THETA: u8 = 0;\n",
                        "alpha function 0|Beta class 1|GAMMA constant 2|delta function 3|epsilon function 5|zeta module 6|eta "
                        "function 8|THETA constant 9");
}

void test_a_scripts_declarations_are_read(void)
{
    assert_declarations("x.js",
                        "export async function load(path) {\nclass Editor {\n  paint() {\n    if (x) {\n  }\nconst STEP = "
                        "4;\nexport const go = (a) => a;\nlet run = async () => 1;\n",
                        "load function 0|Editor class 1|paint method 2|STEP constant 5|go function 6|run function 7");
}

void test_cs_declarations_are_read(void)
{
    assert_declarations("x.c",
                        "#define LIMIT 4\n#  define TWICE(x) ((x) * 2)\nstruct grid {\ntypedef enum shade\nstatic int "
                        "count_cells(const struct grid *g) {\nint scale(int a,\n  return helper(1);\nint value = make(2);\nvoid "
                        "declared(int);\nif (x) {\n",
                        "LIMIT constant 0|TWICE constant 1|grid class 2|shade class 3|count_cells function 4|scale function 5|declared "
                        "function 8");
}

void test_a_shells_declarations_are_read(void)
{
    assert_declarations("x.sh", "build() {\nfunction clean {\necho hi\n", "build function 0|clean function 1");
}

void test_powershells_declarations_are_read(void)
{
    assert_declarations("x.ps1", "Function Get-Thing {\n", "Get-Thing function 0");
}

void test_tomls_tables_are_read(void)
{
    assert_declarations("x.toml", "[package]\n[[bin]]\nname = 1\n", "package module 0|bin module 1");
}

void test_markdowns_headings_are_read_none_in_code(void)
{
    assert_declarations("x.md", "# Title\n```\n# not this\n```\n## Part two ##\n", "Title heading 0|Part two heading 4");
}

void test_texs_sections_are_read(void)
{
    assert_declarations("x.tex", "\\section*{Start}\n\\subsection[short]{Long}\n", "Start heading 0|Long heading 1");
}

void test_scores_rank_the_name_itself_first(void)
{
    const long long exact = score_of("paint", "paint");
    const long long begins = score_of("paint", "paintStrip");
    const long long contains = score_of("paint", "repaint");
    const long long initials = score_of("gtf", "go_to_file");
    const long long letters = score_of("pnt", "paint");
    TEST_ASSERT_GREATER_THAN_INT64(begins, exact);
    TEST_ASSERT_GREATER_THAN_INT64(contains, begins);
    TEST_ASSERT_GREATER_THAN_INT64(initials, contains);
    TEST_ASSERT_GREATER_THAN_INT64(letters, initials);
}

void test_a_words_start_ranks_over_letters(void)
{
    TEST_ASSERT_GREATER_THAN_INT64(score_of("pnt", "paint"), score_of("gtf", "goToFile"));
}

void test_a_query_that_names_none_does_not_answer(void)
{
    TEST_ASSERT_EQUAL_INT64(-1ll, score_of("xyz", "paint"));
}


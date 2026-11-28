// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_symbols.c: Python's, Rust's, scripts', C's and the other languages' declarations are read; scores rank the name
// itself first; a first reading reports how far it has gone and a stop stores what it read; the tree is read and a file
// read again once it changes; a call's signature is read from the tree; a name's definition in effect is found through
// includes, conditions and the compilation database's defines; a slice is cut for a view; and a name's usages,
// definition, completions, hints, spans, bracket pair and call hierarchy are read
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/code/symbols.h"
#include "ui/cli/src/code/symbols_tree.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// the tree the cases over a walked tree read: a header, a definition and a file that calls it, walked once for the
// process
static const char s_shape_text[] = "#ifndef SHAPE_H\n#define SHAPE_H\nint area(int side);\n#define SIDES 4\n#endif\n";
static const char s_area_text[] = "#include \"shape.h\"\nint area(int n)\n{\n    return n * n * SIDES;\n}\n";
static const char s_use_text[] =
    "#include \"shape.h\"\nint total(int n)\n{\n    int w = area(n) + SIDES;\n    return w + area(1);\n}\n";
static char s_tree[CLI_PATH_MAX_LEN];
static int s_begun;
static char s_reached[1024];
static VerbaLinea s_reached_line = {s_reached, sizeof(s_reached), 0ull};
static char s_sliced[2048];
static VerbaLinea s_sliced_line = {s_sliced, sizeof(s_sliced), 0ull};
static unsigned int s_windows;
static unsigned int s_first_window_first;

// the tree written and walked, waited for as the window waits for it
static void tree_begun(void)
{
    if (s_begun)
    {
        return;
    }
    s_begun = 1;
    cli_unity_folder("build/harness/symbols_tree", s_tree);
    const char *const init[2] = {"init", "-q"};
    cli_unity_git(s_tree, init, 2u);
    char src[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_tree, .relative = "src", .joined = src);
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = src);
    cli_unity_file(s_tree, "src/shape.h", s_shape_text);
    cli_unity_file(s_tree, "src/area.c", s_area_text);
    cli_unity_file(s_tree, "src/use.c", s_use_text);
    const char *const add[2] = {"add", "-A"};
    cli_unity_git(s_tree, add, 2u);
    EMBED_CALL(cli_symbols_context.tree_list, CliSymbolsContextCfg, .root = s_tree);
    EMBED_CALL(cli_symbols_root.tree_walk, CliSymbolsRootCfg, .root = s_tree);
    CliWake pause = CLI_WAKE_INIT;
    const unsigned long long last_ms = cli_clock_milliseconds() + 10000ULL;
    while (cli_tree_unwalked() && (cli_clock_milliseconds() < last_ms))
    {
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &pause, .milliseconds_u64 = 5ULL);
    }
    TEST_ASSERT_FALSE_MESSAGE(cli_tree_unwalked(), "the tree is walked");
}

// a function a call hierarchy reaches written as `path:line name sites`, `|` between them
static void reached_recorded(const CliCallReachedCfg *args)
{
    mmgr_line_put(&s_reached_line, (s_reached_line.at > 0ull) ? "|" : "");
    mmgr_line_put(&s_reached_line, args->path);
    mmgr_line_ch(&s_reached_line, ':');
    mmgr_line_decimal(&s_reached_line, args->line, 1u);
    mmgr_line_ch(&s_reached_line, ' ');
    mmgr_line_put_n(&s_reached_line, args->name, args->length);
    mmgr_line_ch(&s_reached_line, ' ');
    mmgr_line_decimal(&s_reached_line, args->count, 1u);
}

// the functions the call hierarchy entry `each` reaches from line `line` of `path`, NUL-terminated in s_reached
static const char *reached_of(int (*each)(const CliSymbolsPathContextLineCfg *args), const char *path,
                              unsigned int line)
{
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    s_reached_line.at = 0ull;
    TEST_ASSERT_EQUAL_INT(1, each(&(CliSymbolsPathContextLineCfg){
                                 .path = path, .line = line, .each = reached_recorded, .err_msg = &err_msg}));
    (void)mmgr_line_finish(&s_reached_line);
    return s_reached;
}

// a slice's line written after the others as `line:text`, a newline between them
static void slice_line_recorded(const CliSliceLineCfg *args)
{
    if (args->line == CLI_SLICE_NOWHERE)
    {
        mmgr_line_put(&s_sliced_line, "-:");
    }
    else
    {
        mmgr_line_decimal(&s_sliced_line, args->line, 1u);
        mmgr_line_ch(&s_sliced_line, ':');
    }
    mmgr_line_put_n(&s_sliced_line, args->text, args->length);
    mmgr_line_ch(&s_sliced_line, '\n');
}

// a window counted, the first one's first line kept; every window cut
static int window_counted(const CliSliceWindowCfg *args)
{
    if (s_windows == 0u)
    {
        s_first_window_first = args->first;
    }
    s_windows += 1u;
    return 1;
}

// a declaration written after the others, `name type line` joined by `|`
static void declaration_recorded(const CliDeclaredCfg *args)
{
    VerbaLinea *const out = args->context;
    mmgr_line_put(out, (out->at > 0ull) ? "|" : "");
    mmgr_line_put_n(out, args->name, args->length);
    mmgr_line_ch(out, ' ');
    mmgr_line_put(out, args->type);
    mmgr_line_ch(out, ' ');
    mmgr_line_decimal(out, args->line, 1u);
}

// the declarations of `text` read as the file `path`, asserted against `required`
static void assert_declarations(const char *path, const char *text, const char *required)
{
    char out[1024];
    VerbaLinea line = {out, sizeof(out), 0ull};
    EMBED_CALL(cli_symbols_path_text_size.declare, CliSymbolsPathTextSizeCfg, .path = path, .text = text,
               .size = CLI_UNITY_LENGTH(text), .each_declared = declaration_recorded, .context = &line);
    CLI_TEST_ASSERT_TEXT(required, out, line.at);
}

// the score `query` assigns `name`, -1 where it does not name it
static long long score_of(const char *query, const char *name)
{
    long long points = 0ll;
    return EMBED_CALL(cli_symbols_query.score, CliSymbolsQueryCfg, .query = query, .name = name,
                      .name_length = CLI_UNITY_LENGTH(name), .points = &points)
               ? points
               : -1ll;
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
    assert_declarations(
        "x.py",
        "import os\nLIMIT = 3\nlimit = 4\nX: int = 5\nclass Page:\n    def read(self):\n        pass\nasync def "
        "go():\n    if a == b:\n        pass\n",
        "LIMIT constant 1|X constant 3|Page class 4|read function 5|go function 7");
}

void test_rusts_declarations_are_read(void)
{
    assert_declarations(
        "x.rs",
        "pub(crate) fn alpha() {}\npub struct Beta;\nconst GAMMA: u32 = 1;\nmacro_rules! delta {\n}\npub async "
        "unsafe fn epsilon() {}\nmod zeta;\nimpl Beta {\nextern \"C\" fn eta() {}\nstatic mut THETA: u8 = 0;\n",
        "alpha function 0|Beta class 1|GAMMA constant 2|delta function 3|epsilon function 5|zeta module 6|eta "
        "function 8|THETA constant 9");
}

void test_a_scripts_declarations_are_read(void)
{
    assert_declarations(
        "x.js",
        "export async function load(path) {\nclass Editor {\n  paint() {\n    if (x) {\n  }\nconst STEP = "
        "4;\nexport const go = (a) => a;\nlet run = async () => 1;\n",
        "load function 0|Editor class 1|paint method 2|STEP constant 5|go function 6|run function 7");
}

void test_cs_declarations_are_read(void)
{
    assert_declarations(
        "x.c",
        "#define LIMIT 4\n#  define TWICE(x) ((x) * 2)\nstruct grid {\ntypedef enum shade\nstatic int "
        "count_cells(const struct grid *g) {\nint scale(int a,\n  return helper(1);\nint value = make(2);\nvoid "
        "declared(int);\nif (x) {\n",
        "LIMIT constant 0|TWICE constant 1|grid class 2|shade class 3|count_cells function 4|scale function 5|value "
        "variable 7|declared function 8");
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
    assert_declarations("x.md", "# Title\n```\n# not this\n```\n## Part two ##\n",
                        "Title heading 0|Part two heading 4");
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
    const long long characters = score_of("pnt", "paint");
    TEST_ASSERT_GREATER_THAN_INT64(begins, exact);
    TEST_ASSERT_GREATER_THAN_INT64(contains, begins);
    TEST_ASSERT_GREATER_THAN_INT64(initials, contains);
    TEST_ASSERT_GREATER_THAN_INT64(characters, initials);
}

void test_a_words_start_ranks_over_characters(void)
{
    TEST_ASSERT_GREATER_THAN_INT64(score_of("pnt", "paint"), score_of("gtf", "goToFile"));
}

void test_a_query_that_names_none_does_not_answer(void)
{
    TEST_ASSERT_EQUAL_INT64(-1ll, score_of("xyz", "paint"));
}

void test_the_tree_is_listed_and_its_first_reading_has_ended_with_every_file_done(void)
{
    tree_begun();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_symbols_root.tree_is_list, CliSymbolsRootCfg, .root = s_tree));
    TEST_ASSERT_FALSE(
        EMBED_CALL(cli_symbols_root.tree_is_list, CliSymbolsRootCfg, .root = "build/harness/no_such_tree"));
    unsigned long long done = 0ull;
    unsigned long long total = 0ull;
    TEST_ASSERT_FALSE(
        EMBED_CALL(cli_symbols_done.tree_walk_progress, CliSymbolsDoneCfg, .done = &done, .total = &total));
    TEST_ASSERT_EQUAL_UINT64(total, done);
}

void test_the_call_hierarchy_names_the_function_at_a_line_its_callers_and_its_callees(void)
{
    tree_begun();
    TEST_ASSERT_EQUAL_STRING("src/use.c:1 total 0", reached_of(cli_symbols_function_at, "src/use.c", 1u));
    TEST_ASSERT_EQUAL_STRING("src/use.c:1 total 2", reached_of(cli_symbols_callers, "src/area.c", 1u));
    TEST_ASSERT_EQUAL_STRING("src/shape.h:2 area 2", reached_of(cli_symbols_callees, "src/use.c", 1u));
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    TEST_ASSERT_EQUAL_INT(0,
                          EMBED_CALL(cli_symbols_path_context_line.function_at, CliSymbolsPathContextLineCfg,
                                     .path = "src/use.c", .line = 0u, .each = reached_recorded, .err_msg = &err_msg));
}

void test_a_slice_for_a_view_carries_its_openers_and_the_lines_in_view(void)
{
    tree_begun();
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const unsigned int opener[1] = {2u};
    s_sliced_line.at = 0ull;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_symbols_path.slice_each, CliSymbolsPathCfg, .path = "src/use.c", .text = s_use_text,
                                .size = sizeof(s_use_text) - 1u, .first = 3u, .last = 4u, .opener = opener,
                                .openers = 1u, .each_slice_line = slice_line_recorded, .err_msg = &err_msg));
    (void)mmgr_line_finish(&s_sliced_line);
    CLI_TEST_ASSERT_CONTAINS("2:{\n", s_sliced);
    CLI_TEST_ASSERT_CONTAINS("3:    int w = area(n) + SIDES;\n", s_sliced);
    CLI_TEST_ASSERT_CONTAINS("4:    return w + area(1);\n", s_sliced);
}

void test_a_file_is_cut_into_the_slices_of_its_windows_in_one_walk(void)
{
    tree_begun();
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    s_sliced_line.at = 0ull;
    s_windows = 0u;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_symbols_path_text_size_context_err_msg.slices_each,
                                CliSymbolsPathTextSizeContextErrMsgCfg, .path = "src/use.c", .text = s_use_text,
                                .size = sizeof(s_use_text) - 1u, .height = 3u, .each_slice_line = slice_line_recorded,
                                .window = window_counted, .err_msg = &err_msg));
    (void)mmgr_line_finish(&s_sliced_line);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT(1u, s_windows);
    TEST_ASSERT_EQUAL_UINT(0u, s_first_window_first);
    CLI_TEST_ASSERT_CONTAINS("int total(int n)", s_sliced);
}

void test_a_token_names_roles_come_off_its_segments(void)
{
    TEST_ASSERT_BITS_HIGH(
        CLI_ROLE_REGION_OPENS | CLI_ROLE_DEPTH_OPENS,
        EMBED_CALL(cli_symbols_query.name_roles, CliSymbolsQueryCfg, .name = "delimiter.bracket.region.depth.open"));
    TEST_ASSERT_BITS_HIGH(
        CLI_ROLE_REGION_CLOSES | CLI_ROLE_CLOSING,
        EMBED_CALL(cli_symbols_query.name_roles, CliSymbolsQueryCfg, .name = "delimiter.bracket.region.depth.close"));
    TEST_ASSERT_EQUAL_UINT(CLI_ROLE_QUOTED,
                           EMBED_CALL(cli_symbols_query.name_roles, CliSymbolsQueryCfg, .name = "string"));
    TEST_ASSERT_BITS_HIGH(CLI_ROLE_NAME | CLI_ROLE_WORD,
                          EMBED_CALL(cli_symbols_query.name_roles, CliSymbolsQueryCfg, .name = "identifier"));
}

// the declarations a query finds counted, the first one's name kept
typedef struct
{
    unsigned int count;
    char first[16];
} FoundRecorded;

static void found_recorded(const CliEachFoundCfg *args)
{
    FoundRecorded *const recorded = (FoundRecorded *)args->context;
    if ((recorded->count == 0u) && (args->name_length < sizeof(recorded->first)))
    {
        memor.cpy(&(MemoriaCfg){.dst = recorded->first, .src = args->name, .bytes = args->name_length + 1ull});
    }
    recorded->count += 1u;
}

void test_the_declarations_a_query_names_are_ranked_and_a_calls_signature_read(void)
{
    tree_begun();
    FoundRecorded recorded = {0u, ""};
    (void)EMBED_CALL(cli_symbols_query_max.tree_declarations_each, CliSymbolsQueryMaxCfg, .query = "area", .max = 8u,
                     .each = found_recorded, .context = &recorded);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT(1u, recorded.count);
    TEST_ASSERT_EQUAL_STRING("area", recorded.first);
    static CliSignatureStore s_store;
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    // the cursor inside `area(n)` on line 3 of the caller
    TEST_ASSERT_TRUE(EMBED_CALL(cli_symbols_path_text_size_out.signature_read, CliSymbolsPathTextSizeOutCfg,
                                .path = "src/use.c", .text = s_use_text, .size = sizeof(s_use_text) - 1u,
                                .state = "root", .line = 3u, .col = 18u, .owner = g_cli_text_owner, .err_msg = &err_msg,
                                .store = &s_store));
    CLI_TEST_ASSERT_CONTAINS("area", s_store.label);
}

static char s_stamps[1024];
static VerbaLinea s_stamp_line;

// a change the tree stamps written on a line of s_stamp_line
static void stamp_recorded(const CliEachPathTypeCfg *args)
{
    mmgr_line_put(&s_stamp_line, args->path);
    mmgr_line_ch(&s_stamp_line, '\n');
}

void test_a_branchs_copy_gives_its_path_and_declarations_and_a_moved_stamp_is_reported(void)
{
    tree_begun();
    const unsigned int file = EMBED_CALL(cli_symbols_tree_path.branch_get, CliSymbolsTreePathCfg, .path = "src/area.c");
    TEST_ASSERT_NOT_EQUAL(CLI_BRANCHES_MAX, file);
    CliTreeBranch branch;
    EMBED_CALL(cli_symbols_tree_at.branch_read, CliSymbolsTreeAtCfg, .file = file, .branch_out = &branch);
    TEST_ASSERT_NOT_NULL(branch.cell);
    EMBED_CALL(cli_symbols_tree.branch_cell_count, CliSymbolsTreeCfg, .bytes = branch.bytes);
    char *const scratch = cli_branch_copy_alloc();
    TEST_ASSERT_NOT_NULL(scratch);
    (void)g_platform_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = branch.cell, .offset = 0u, .dst = scratch, .bytes = (embed_word)branch.bytes});
    TEST_ASSERT_EQUAL_STRING("src/area.c",
                             EMBED_CALL(cli_symbols_tree_branch_scratch.branch_copy_path,
                                        CliSymbolsTreeBranchScratchCfg, .branch = &branch, .scratch_read = scratch));
    const CliDeclaration *const declarations =
        EMBED_CALL(cli_symbols_tree_branch_scratch.branch_copy_declarations, CliSymbolsTreeBranchScratchCfg,
                   .branch = &branch, .scratch_read = scratch);
    TEST_ASSERT_NOT_NULL(declarations);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT(1u, branch.count);
    int area_declared = 0;
    for (unsigned int at = 0u; at < branch.count; at += 1u)
    {
        const char *const name = scratch + declarations[at].name;
        area_declared |= (declarations[at].length == 4u) && (name[0] == 'a') && (name[1] == 'r') && (name[2] == 'e') &&
                         (name[3] == 'a');
    }
    TEST_ASSERT_TRUE_MESSAGE(area_declared, "area is declared in src/area.c");
    g_platform_guard->persistent_buf_release(scratch, mmgr_owner_of_buf(platform_pool_series(), scratch));
    // a file written anew moves its stamp, which the tree reports where nothing else reports its changes
    s_stamp_line = (VerbaLinea){s_stamps, sizeof(s_stamps), 0ull};
    cli_unity_file(s_tree, "src/use.c",
                   "#include \"shape.h\"\nint use(void)\n{\n    return area(1) + area(2) + SIDES;\n}\n");
    EMBED_CALL(cli_symbols_context.tree_stamp, CliSymbolsContextCfg, .root = s_tree, .emit = stamp_recorded,
               .context = NULL);
    (void)mmgr_line_finish(&s_stamp_line);
    CLI_TEST_ASSERT_CONTAINS("src/use.c", s_stamps);
}

// The path of branch `number` of the many, into `path`
static void many_path(unsigned int number, char *path, size_t room)
{
    VerbaLinea line = {path, room, 0ULL};
    mmgr_line_put(&line, "many/");
    mmgr_line_decimal(&line, number % 7u, 1u);
    mmgr_line_put(&line, "/file_");
    mmgr_line_decimal(&line, number, 1u);
    mmgr_line_put(&line, ".c");
    (void)mmgr_line_finish(&line);
}

// More branches than a run holds, joined over several merges. The test lets every other one go, and finds each one
// kept at its own place. It finds no one let go, and finds a branch joined again after them
void test_many_branches_are_found_through_the_merges_and_the_ones_let_go_are_not(void)
{
    tree_begun();
    enum
    {
        MANY = 2000u
    };
    static unsigned int s_place[MANY];
    char path[CLI_PATH_MAX_LEN];
    for (unsigned int at = 0u; at < MANY; at += 1u)
    {
        many_path(at, path, sizeof(path));
        s_place[at] = EMBED_CALL(cli_symbols_tree_path.branch_exists, CliSymbolsTreePathCfg, .path = path, .folder = 0);
        TEST_ASSERT_NOT_EQUAL(CLI_BRANCHES_MAX, s_place[at]);
    }
    for (unsigned int at = 0u; at < MANY; at += 2u)
    {
        EMBED_CALL(cli_symbols_tree_file.branch_leave, CliSymbolsTreeFileCfg, .file = s_place[at]);
    }
    for (unsigned int at = 0u; at < MANY; at += 1u)
    {
        many_path(at, path, sizeof(path));
        const unsigned int found = EMBED_CALL(cli_symbols_tree_path.branch_get, CliSymbolsTreePathCfg, .path = path);
        TEST_ASSERT_EQUAL_UINT(((at % 2u) == 0u) ? CLI_BRANCHES_MAX : s_place[at], found);
    }
    many_path(0u, path, sizeof(path));
    const unsigned int again =
        EMBED_CALL(cli_symbols_tree_path.branch_exists, CliSymbolsTreePathCfg, .path = path, .folder = 0);
    TEST_ASSERT_NOT_EQUAL(CLI_BRANCHES_MAX, again);
    TEST_ASSERT_EQUAL_UINT(again, EMBED_CALL(cli_symbols_tree_path.branch_get, CliSymbolsTreePathCfg, .path = path));
    // the many let go, so that the tree is as the tests after this one find it
    EMBED_CALL(cli_symbols_tree_file.branch_leave, CliSymbolsTreeFileCfg, .file = again);
    for (unsigned int at = 1u; at < MANY; at += 2u)
    {
        EMBED_CALL(cli_symbols_tree_file.branch_leave, CliSymbolsTreeFileCfg, .file = s_place[at]);
    }
    many_path(1u, path, sizeof(path));
    TEST_ASSERT_EQUAL_UINT(CLI_BRANCHES_MAX,
                           EMBED_CALL(cli_symbols_tree_path.branch_get, CliSymbolsTreePathCfg, .path = path));
}

void test_a_branch_is_joined_read_counted_and_let_go_and_the_tree_cleared_last(void)
{
    tree_begun();
    unsigned int before = 0u;
    unsigned long long bytes = 0ull;
    EMBED_CALL(cli_symbols_tree_count.branch_files_count, CliSymbolsTreeCountCfg, .count = &before,
               .bytes_u64s = &bytes);
    TEST_ASSERT_EQUAL_UINT(3u, before);
    const unsigned int file =
        EMBED_CALL(cli_symbols_tree_path.branch_exists, CliSymbolsTreePathCfg, .path = "src/new.c", .folder = 0);
    TEST_ASSERT_NOT_EQUAL(CLI_BRANCHES_MAX, file);
    TEST_ASSERT_EQUAL_UINT(file,
                           EMBED_CALL(cli_symbols_tree_path.branch_get, CliSymbolsTreePathCfg, .path = "src/new.c"));
    CliTreeBranch branch;
    EMBED_CALL(cli_symbols_tree_at.branch_read, CliSymbolsTreeAtCfg, .file = file, .branch_out = &branch);
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_symbols_tree_branch_path.branch_path_read, CliSymbolsTreeBranchPathCfg, .branch = &branch,
               .path_out = path);
    TEST_ASSERT_EQUAL_STRING("src/new.c", path);
    // a branch written at its place reads back the same
    branch.written = 12345ULL;
    EMBED_CALL(cli_symbols_tree_at.branch_write, CliSymbolsTreeAtCfg, .file = file, .branch = &branch);
    CliTreeBranch written;
    EMBED_CALL(cli_symbols_tree_at.branch_read, CliSymbolsTreeAtCfg, .file = file, .branch_out = &written);
    TEST_ASSERT_EQUAL_UINT64(12345ULL, written.written);
    TEST_ASSERT_EQUAL_PTR(branch.cell, written.cell);
    unsigned int after = 0u;
    EMBED_CALL(cli_symbols_tree_count.branch_files_count, CliSymbolsTreeCountCfg, .count = &after,
               .bytes_u64s = &bytes);
    TEST_ASSERT_EQUAL_UINT(before + 1u, after);
    EMBED_CALL(cli_symbols_tree_file.branch_leave, CliSymbolsTreeFileCfg, .file = file);
    EMBED_CALL(cli_symbols_tree_count.branch_files_count, CliSymbolsTreeCountCfg, .count = &after,
               .bytes_u64s = &bytes);
    TEST_ASSERT_EQUAL_UINT(before, after);
    TEST_ASSERT_EQUAL_UINT(CLI_BRANCHES_MAX,
                           EMBED_CALL(cli_symbols_tree_path.branch_get, CliSymbolsTreePathCfg, .path = "src/new.c"));
    cli_tree_walk_stop();
    cli_tree_clear();
    EMBED_CALL(cli_symbols_tree_count.branch_files_count, CliSymbolsTreeCountCfg, .count = &after,
               .bytes_u64s = &bytes);
    TEST_ASSERT_EQUAL_UINT(0u, after);
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_query_words.c: the words of `imp protocol`: the list names every loop once; leave is read off the words; a loop missing a program reports
// it and starts no run; an unknown command is refused with the list; the table's whole names are those with every type; the steps run the protocol in
// order; the base is the one word the cross accepts; the readings of a table are distinguished from a pull; the version and the architectures are read
// off nvcc; the query's words are read and a file not there is reported; and the writers read their words
#include "ui/query/src/learn/tables_internal.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/query/src/query_loops.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_root[QUERY_PATH_MAX_LEN];
static char s_printed[QUERY_LINE_MAX_LEN];
static unsigned long long s_printed_size;
static int s_ready;
static char s_error[1024];
static char s_query_root[QUERY_PATH_MAX_LEN];
static char s_out[1024];

// a line printed, recorded
static void line_recorded(const CliEachLineSizeCfg *args)
{
    VerbaLinea line = {s_printed, sizeof(s_printed), 0ull};
    mmgr_line_put_n(&line, args->line, args->size);
    s_printed_size = mmgr_line_finish(&line);
}

// nothing is found, each reported by its name
static int nothing_found(const QueryEachRolePathCapacityErrMsgCfg *args)
{
    QueryRole role = args->role;
    args->path[0] = '\0';
    mmgr_line_put(args->err_msg, EMBED_CALL(query_table.role_name, QueryCfg, .role = role));
    mmgr_line_put(args->err_msg, " is not found");
    return 0;
}

// no PATH beyond the process's own
static const char *empty_path(void *context)
{
    (void)context;
    return "";
}

// the files of the table at `root`, each empty
static void table_written(const char *root, const char *const *files, unsigned int count)
{
    char table[QUERY_PATH_MAX_LEN];
    char path[QUERY_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = QUERY_TABLE, .joined = table);
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = root);
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = table);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = table, .relative = files[at], .joined = path);
        (void)EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = "", .size = 0ull);
    }
}

// the whole names the table contains of `types`, joined by `,`
static const char *joined_names(const char *root, const char *const *types, unsigned int count)
{
    static char s_names[64][QUERY_NAME_MAX_LEN];
    static const char *s_named[64];
    static char s_joined[512];
    const unsigned int named = EMBED_CALL(query_loops_root_types.whole, QueryLoopsRootTypesCfg, .root = root, .types = types, .type_count = count, .names = s_names, .named = s_named);
    VerbaLinea line = {s_joined, sizeof(s_joined), 0ull};
    for (unsigned int at = 0u; at < named; at += 1u)
    {
        mmgr_line_put(&line, (at > 0u) ? "," : "");
        mmgr_line_put(&line, s_named[at]);
    }
    (void)mmgr_line_finish(&line);
    return s_joined;
}

// the reading `text`'s words request: 1 with its mode into `mode`, 0 for a pull, -1 refused
static int read_state_of(const char *text, const char **mode)
{
    static char s_text[256];
    static const char *s_words[16];
    char why[512];
    memor.cpy(&(MemoriaCfg){.dst = s_text, .src = text, .bytes = CLI_UNITY_LENGTH(text) + 1u});
    unsigned int count = 0u;
    for (char *at = s_text; (at[0] != '\0') && (count < 16u);)
    {
        while (at[0] == ' ')
        {
            at[0] = '\0';
            at += 1;
        }
        if (at[0] == '\0')
        {
            break;
        }
        s_words[count] = at;
        count += 1u;
        while ((at[0] != ' ') && (at[0] != '\0'))
        {
            at += 1;
        }
    }
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    *mode = NULL;
    return EMBED_CALL(query_loops_err_msg_words_count_mode.read_state, QueryLoopsErrMsgWordsCountModeCfg, .words = s_words, .count = count, .mode = mode, .err_msg = &err_msg);
}

// a table of whole languages and parts written at s_query_root
static void query_table_written(void)
{
    static const char *const s_coordinates[9] = {"Lstar.klq", "cu.klm", "cu.krs", "sass.klm", "sass.krs", "sm_86.khw", "sm_86.kqr", "sm_86.ksc", "c.krs"};
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "build/query_test/query_values", .joined = s_query_root);
    table_written(s_query_root, s_coordinates, 9u);
}

// the query's words read at s_query_root: 1 with its names joined by `,` and `dry` after a `|` in s_out, or 0 with why in s_out
static int query_words_read(const char *const *words, unsigned int count)
{
    QueryContext context;
    memor.set(&(MemoriaCfg){.dst = &context, .val = 0, .bytes = sizeof(context)});
    context.root = s_query_root;
    const char *names[3] = {NULL, NULL, NULL};
    unsigned int named = 0u;
    int dry = 0;
    VerbaLinea out_line = {s_out, sizeof(s_out), 0ull};
    const int read = EMBED_CALL(query_loops_err_msg_context_words.query_words, QueryLoopsErrMsgContextWordsCfg, .context_read = &context, .words = words, .count = count, .names_characters = names, .named_u32s = &named, .dry_pointer = &dry, .err_msg = &out_line);
    if (read)
    {
        out_line.at = 0ull;
        for (unsigned int at = 0u; at < named; at += 1u)
        {
            mmgr_line_put(&out_line, (at > 0u) ? "," : "");
            mmgr_line_put(&out_line, names[at]);
        }
        mmgr_line_put(&out_line, dry ? "|dry" : "|");
    }
    (void)mmgr_line_finish(&out_line);
    return read;
}

void setUp(void)
{
    char found[CLI_PATH_MAX_LEN];
    cli_unity_begun();
    if (s_ready)
    {
        return;
    }
    if (!EMBED_CALL(cli_tree_internal.tree_find, CliTreeCtx, .tree = found))
    {
        TEST_IGNORE_MESSAGE("no tree is found");
    }
    memor.cpy(&(MemoriaCfg){.dst = s_root, .src = found, .bytes = CLI_UNITY_LENGTH(found) + 1u});
    for (char *at = s_root; at[0] != '\0'; at += 1)
    {
        at[0] = (at[0] == '\\') ? '/' : at[0];
    }
    s_ready = 1;
}

void tearDown(void)
{
}

void test_the_list_names_every_loop_once(void)
{
    const QueryCommand *commands = NULL;
    char ids[256];
    VerbaLinea line = {ids, sizeof(ids), 0ull};
    const unsigned int count = EMBED_CALL(query_table.commands, QueryCfg, .root = ".", .commands = &commands);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(&line, (at > 0u) ? "," : "");
        mmgr_line_put(&line, commands[at].id);
    }
    CLI_TEST_ASSERT_TEXT("course,tables,decode,query,learn,cross,write,pairs,stick,retention,sweep", ids, line.at);
}

void test_leave_is_read_off_the_words(void)
{
    const char *words[5] = {"turns", "--leave", "none", "--base", "a.cu"};
    unsigned int left = 5u;
    int sent = 0;
    QueryLeave leave = QUERY_LEAVE_ALL;
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(query_loops_err_msg_words_count_input.leave_alloc, QueryLoopsErrMsgWordsCountInputCfg, .words_read = words, .count_pointer = &left, .supplied = &sent, .leave = &leave, .err_msg = &err_msg));
    TEST_ASSERT_TRUE(sent);
    TEST_ASSERT_EQUAL_INT(QUERY_LEAVE_NONE, leave);
    TEST_ASSERT_EQUAL_UINT(3u, left);
    TEST_ASSERT_EQUAL_STRING("turns", words[0]);
    TEST_ASSERT_EQUAL_STRING("--base", words[1]);
    TEST_ASSERT_EQUAL_STRING("a.cu", words[2]);
}

void test_an_unknown_leave_is_refused(void)
{
    const char *bad[2] = {"--leave", "maybe"};
    unsigned int left = 2u;
    int sent = 0;
    QueryLeave leave = QUERY_LEAVE_ALL;
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    TEST_ASSERT_FALSE(EMBED_CALL(query_loops_err_msg_words_count_input.leave_alloc, QueryLoopsErrMsgWordsCountInputCfg, .words_read = bad, .count_pointer = &left, .supplied = &sent, .leave = &leave, .err_msg = &err_msg));
}

void test_no_leave_sent_is_none(void)
{
    const char *none[1] = {"turns"};
    unsigned int left = 1u;
    int sent = 0;
    QueryLeave leave = QUERY_LEAVE_ALL;
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(query_loops_err_msg_words_count_input.leave_alloc, QueryLoopsErrMsgWordsCountInputCfg, .words_read = none, .count_pointer = &left, .supplied = &sent, .leave = &leave, .err_msg = &err_msg));
    TEST_ASSERT_FALSE(sent);
}

void test_a_loop_missing_a_program_reports_it_and_starts_no_run(void)
{
    static unsigned long long s_numbers[QUERY_RUNS_MAX];
    static char s_paths[QUERY_RUNS_MAX][QUERY_PATH_MAX_LEN];
    static const char *const s_learn[2] = {"learn", "turns"};
    char folder[QUERY_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = QUERY_LEARN_FOLDER, .joined = folder);
    const unsigned int before = EMBED_CALL(query_folder_number_internal.runs_each, QueryFolderNumberCtx, .folder = folder, .number = s_numbers, .path_callback = s_paths, .capacity_u32 = QUERY_RUNS_MAX);
    const QueryPrograms nothing = {nothing_found, empty_path, NULL};
    const QuerySink sink = {line_recorded, NULL};
    s_printed_size = 0ull;
    TEST_ASSERT_LESS_THAN_INT(0, EMBED_CALL(query_table.run, QueryCfg, .root = s_root, .programs = &nothing, .words = s_learn, .count = 2u, .sink = sink));
    const int not_found = cellul.find(&(CatenaFinitaCfg){.src = s_printed, .cap = CLI_POOL_BYTES, .other = "is not found", .other_cap = CLI_POOL_BYTES}) != NULL;
    const int no_disassembler = cellul.find(&(CatenaFinitaCfg){.src = s_printed, .cap = CLI_POOL_BYTES, .other = "no nvdisasm", .other_cap = CLI_POOL_BYTES}) != NULL;
    TEST_ASSERT_TRUE(not_found || no_disassembler);
    TEST_ASSERT_EQUAL_UINT(before, EMBED_CALL(query_folder_number_internal.runs_each, QueryFolderNumberCtx, .folder = folder, .number = s_numbers, .path_callback = s_paths, .capacity_u32 = QUERY_RUNS_MAX));
}

void test_an_unknown_command_is_refused_with_the_list(void)
{
    static const char *const s_dance[1] = {"dance"};
    const QueryPrograms on_path = query_on_path();
    const QuerySink sink = {line_recorded, NULL};
    s_printed_size = 0ull;
    TEST_ASSERT_LESS_THAN_INT(0, EMBED_CALL(query_table.run, QueryCfg, .root = ".", .programs = &on_path, .words = s_dance, .count = 1u, .sink = sink));
    CLI_TEST_ASSERT_CONTAINS("learn", s_printed);
    CLI_TEST_ASSERT_CONTAINS("sweep", s_printed);
}

void test_no_command_is_refused_with_the_list(void)
{
    const QueryPrograms on_path = query_on_path();
    const QuerySink sink = {line_recorded, NULL};
    s_printed_size = 0ull;
    TEST_ASSERT_LESS_THAN_INT(0, EMBED_CALL(query_table.run, QueryCfg, .root = ".", .programs = &on_path, .words = NULL, .count = 0u, .sink = sink));
    CLI_TEST_ASSERT_CONTAINS("learn", s_printed);
}

void test_the_tables_complete_names_are_those_with_every_type(void)
{
    static const char *const s_files[9] = {"cu.klm", "cu.krs", "sass.klm", "sass.krs", "c.krs", "sm_86.khw", "sm_86.kqr", "sm_86.ksc", "sm_89.khw"};
    static const char *const s_languages[2] = {"klm", "krs"};
    static const char *const s_parts[3] = {"khw", "kqr", "ksc"};
    char root[QUERY_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "build/query_test/whole", .joined = root);
    table_written(root, s_files, 9u);
    TEST_ASSERT_EQUAL_STRING("cu,sass", joined_names(root, s_languages, 2u));
    TEST_ASSERT_EQUAL_STRING("sm_86", joined_names(root, s_parts, 3u));
}

void test_the_steps_run_the_protocol_in_order(void)
{
    char names[512];
    VerbaLinea line = {names, sizeof(names), 0ull};
    EMBED_CALL(cli_loops.names_join, QueryLoopsCfg, .line = &line);
    CLI_TEST_ASSERT_TEXT("vendor tables, discover, turns, registers, write krs, stick engine, pairs stick, pairs past, costs, "
                         "write krs",
                         names, line.at);
}

void test_the_course_starts_at_the_first_step_where_none_is_named(void)
{
    unsigned int first = 9u;
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(query_loops_err_msg_words_count.course_from, QueryLoopsErrMsgWordsCountCfg, .words = NULL, .count = 0u, .first = &first, .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_UINT(0u, first);
}

void test_the_course_starts_at_a_step_by_its_name(void)
{
    static const char *const s_costs[2] = {"--from", "costs"};
    unsigned int first = 9u;
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(query_loops_err_msg_words_count.course_from, QueryLoopsErrMsgWordsCountCfg, .words = s_costs, .count = 2u, .first = &first, .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_UINT(8u, first);
}

void test_a_name_two_steps_share_names_the_first(void)
{
    static const char *const s_krs[2] = {"--from", "write krs"};
    unsigned int first = 9u;
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(query_loops_err_msg_words_count.course_from, QueryLoopsErrMsgWordsCountCfg, .words = s_krs, .count = 2u, .first = &first, .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_UINT(4u, first);
}

void test_a_from_flag_with_no_name_is_refused(void)
{
    static const char *const s_bare[1] = {"--from"};
    unsigned int first = 9u;
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    TEST_ASSERT_FALSE(EMBED_CALL(query_loops_err_msg_words_count.course_from, QueryLoopsErrMsgWordsCountCfg, .words = s_bare, .count = 1u, .first = &first, .err_msg = &err_msg));
}

void test_a_from_name_no_step_has_is_refused(void)
{
    static const char *const s_fly[2] = {"--from", "fly"};
    unsigned int first = 9u;
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    TEST_ASSERT_FALSE(EMBED_CALL(query_loops_err_msg_words_count.course_from, QueryLoopsErrMsgWordsCountCfg, .words = s_fly, .count = 2u, .first = &first, .err_msg = &err_msg));
    (void)mmgr_line_finish(&err_msg);
    CLI_TEST_ASSERT_CONTAINS("no step of the course is fly", s_error);
}

void test_the_cross_has_no_base_where_none_is_named(void)
{
    const char *base = "x";
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(query_loops_err_msg_words.base_get, QueryLoopsErrMsgWordsCfg, .words = NULL, .count = 0u, .base_pointer = &base, .err_msg = &err_msg));
    TEST_ASSERT_NULL(base);
}

void test_the_cross_reads_its_base(void)
{
    static const char *const s_base[2] = {"--base", "a.cu"};
    const char *base = "x";
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(query_loops_err_msg_words.base_get, QueryLoopsErrMsgWordsCfg, .words = s_base, .count = 2u, .base_pointer = &base, .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_STRING("a.cu", base);
}

void test_a_base_not_written_as_the_cross_reads_it_is_refused(void)
{
    static const char *const s_base[2] = {"--base", "a.cu"};
    static const char *const s_word[1] = {"a.cu"};
    const char *base = "x";
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    TEST_ASSERT_FALSE(EMBED_CALL(query_loops_err_msg_words.base_get, QueryLoopsErrMsgWordsCfg, .words = s_word, .count = 1u, .base_pointer = &base, .err_msg = &err_msg));
    TEST_ASSERT_FALSE(EMBED_CALL(query_loops_err_msg_words.base_get, QueryLoopsErrMsgWordsCfg, .words = s_base, .count = 1u, .base_pointer = &base, .err_msg = &err_msg));
}

void test_architectures_alone_or_nothing_request_a_pull(void)
{
    const char *mode = NULL;
    TEST_ASSERT_EQUAL_INT(0, read_state_of("sm_86 sm_90", &mode));
    TEST_ASSERT_EQUAL_INT(0, read_state_of("", &mode));
}

void test_the_fields_of_a_table_are_a_read_state_of_one_architecture(void)
{
    const char *mode = NULL;
    TEST_ASSERT_EQUAL_INT(1, read_state_of("fields sm_86", &mode));
    TEST_ASSERT_EQUAL_STRING("spans", mode);
    TEST_ASSERT_EQUAL_INT(-1, read_state_of("fields sm_86 extra", &mode));
    TEST_ASSERT_EQUAL_INT(-1, read_state_of("fields", &mode));
}

void test_a_read_state_of_encodings_is_of_two_words_and_a_banks_size(void)
{
    const char *mode = NULL;
    TEST_ASSERT_EQUAL_INT(1, read_state_of("read sm_86 --bank 0x174 0000460000047ab9 000fc80000000a00", &mode));
    TEST_ASSERT_EQUAL_STRING("class", mode);
    TEST_ASSERT_EQUAL_INT(-1, read_state_of("read sm_86 0000460000047ab9", &mode));
    TEST_ASSERT_EQUAL_INT(-1, read_state_of("read sm_86 --bank", &mode));
}

void test_the_version_is_read_off_nvcc(void)
{
    static const char s_version[] = "Cuda compilation tools, release 13.3, V13.3.73\nBuild cuda_13.3.r13.3/compiler.38244171_0";
    char version[64];
    TEST_ASSERT_TRUE(EMBED_CALL(query_tables_output_internal.version_get, QueryTablesOutputCtx, .output = (const unsigned char *)s_version, .size = sizeof(s_version) - 1u, .version = version, .capacity = sizeof(version)));
    TEST_ASSERT_EQUAL_STRING("13.3.73", version);
}

void test_no_version_is_read_off_nothing(void)
{
    char version[64];
    TEST_ASSERT_FALSE(EMBED_CALL(query_tables_output_internal.version_get, QueryTablesOutputCtx, .output = (const unsigned char *)"nothing", .size = 7ull, .version = version, .capacity = sizeof(version)));
}

void test_the_architectures_are_read_off_nvcc(void)
{
    static const char s_arches[] = "compute_75 compute_80 compute_120\n";
    char arches[8][16];
    TEST_ASSERT_EQUAL_UINT(3u, EMBED_CALL(query_tables_internal.architectures_get, QueryTablesCtx, .output = (const unsigned char *)s_arches, .size = sizeof(s_arches) - 1u, .arches = arches));
    TEST_ASSERT_EQUAL_STRING("sm_75", arches[0]);
    TEST_ASSERT_EQUAL_STRING("sm_80", arches[1]);
    TEST_ASSERT_EQUAL_STRING("sm_120", arches[2]);
}

void test_the_vendors_archive_is_named_by_version(void)
{
    char archive[128];
    EMBED_CALL(query_tables_capacity_internal.archive_by_name, QueryTablesCapacityCtx, .version_read = "13.3.73", .archive = archive, .capacity = sizeof(archive));
    TEST_ASSERT_EQUAL_STRING("cuda_nvdisasm-linux-x86_64-13.3.73-archive", archive);
}

void test_a_path_is_written_as_the_container_sees_it(void)
{
    char inside[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_tables_root_internal.in_container, QueryTablesRootCtx, .root = "D:\\tree", .path_read = "D:\\tree\\build\\vendor\\nvidia\\cubins\\sm_86.cubin", .inside = inside);
    TEST_ASSERT_EQUAL_STRING("/tree/build/vendor/nvidia/cubins/sm_86.cubin", inside);
}

void test_a_query_reads_two_languages(void)
{
    static const char *const s_two[2] = {"--cu", "--sass"};
    query_table_written();
    TEST_ASSERT_TRUE(query_words_read(s_two, 2u));
    TEST_ASSERT_EQUAL_STRING("cu,sass|", s_out);
}

void test_a_query_reads_two_languages_and_a_part(void)
{
    static const char *const s_three[3] = {"--cu", "--sass", "--sm_86"};
    query_table_written();
    TEST_ASSERT_TRUE(query_words_read(s_three, 3u));
    TEST_ASSERT_EQUAL_STRING("cu,sass,sm_86|", s_out);
}

void test_a_query_reads_the_windows_flags_and_dry(void)
{
    static const char *const s_flags[7] = {"--from", "cu", "--to", "sass", "--part", "sm_86", "--dry"};
    query_table_written();
    TEST_ASSERT_TRUE(query_words_read(s_flags, 7u));
    TEST_ASSERT_EQUAL_STRING("cu,sass,sm_86|dry", s_out);
}

void test_words_not_a_querys_are_refused(void)
{
    static const char *const s_one[1] = {"--cu"};
    static const char *const s_plain[2] = {"cu", "--sass"};
    static const char *const s_more[4] = {"--cu", "--sass", "--sm_86", "--more"};
    static const char *const s_path[2] = {"--../cu", "--sass"};
    query_table_written();
    TEST_ASSERT_FALSE(query_words_read(s_one, 1u));
    TEST_ASSERT_FALSE(query_words_read(s_plain, 2u));
    TEST_ASSERT_FALSE(query_words_read(s_more, 4u));
    TEST_ASSERT_FALSE(query_words_read(s_path, 2u));
}

void test_a_dry_query_needs_a_part_for_its_pairs(void)
{
    static const char *const s_dry[3] = {"--cu", "--sass", "--dry"};
    query_table_written();
    TEST_ASSERT_FALSE(query_words_read(s_dry, 3u));
}

void test_a_parts_files_not_there_are_reported(void)
{
    static const char *const s_short[3] = {"--cu", "--sass", "--sm_89"};
    query_table_written();
    TEST_ASSERT_FALSE(query_words_read(s_short, 3u));
    CLI_TEST_ASSERT_CONTAINS("sm_89.khw", s_out);
    CLI_TEST_ASSERT_CONTAINS("sm_89.kqr", s_out);
    CLI_TEST_ASSERT_CONTAINS("sm_89.ksc", s_out);
    CLI_TEST_ASSERT_NOT_CONTAINS("cu.klm", s_out);
}

void test_a_languages_files_not_there_are_reported(void)
{
    static const char *const s_host[3] = {"--c", "--rs", "--host"};
    query_table_written();
    TEST_ASSERT_FALSE(query_words_read(s_host, 3u));
    CLI_TEST_ASSERT_CONTAINS("c.klm", s_out);
    CLI_TEST_ASSERT_NOT_CONTAINS("c.krs", s_out);
    CLI_TEST_ASSERT_CONTAINS("rs.krs", s_out);
    CLI_TEST_ASSERT_CONTAINS("host.khw", s_out);
}

void test_the_bridges_writer_is_over_the_rulesets_named_or_all(void)
{
    static const char *const s_bridge[3] = {"bridge", "cu.krs", "c.krs"};
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    TEST_ASSERT_EQUAL_INT(QUERY_WRITE_BRIDGE, EMBED_CALL(query_loops_err_msg_words.write_words, QueryLoopsErrMsgWordsCfg, .words = s_bridge, .count = 1u, .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_INT(QUERY_WRITE_BRIDGE, EMBED_CALL(query_loops_err_msg_words.write_words, QueryLoopsErrMsgWordsCfg, .words = s_bridge, .count = 3u, .err_msg = &err_msg));
}

void test_the_rulesets_and_the_machine_files_writers_are_read(void)
{
    static const char *const s_krs[2] = {"krs", "more"};
    static const char *const s_gsm[1] = {"gsm"};
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    TEST_ASSERT_EQUAL_INT(QUERY_WRITE_KRS, EMBED_CALL(query_loops_err_msg_words.write_words, QueryLoopsErrMsgWordsCfg, .words = s_krs, .count = 1u, .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_INT(QUERY_WRITE_GSM, EMBED_CALL(query_loops_err_msg_words.write_words, QueryLoopsErrMsgWordsCfg, .words = s_gsm, .count = 1u, .err_msg = &err_msg));
}

void test_no_other_writer_is_read(void)
{
    static const char *const s_krs[2] = {"krs", "more"};
    static const char *const s_kdm[1] = {"kdm"};
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    TEST_ASSERT_LESS_THAN_INT(0, EMBED_CALL(query_loops_err_msg_words.write_words, QueryLoopsErrMsgWordsCfg, .words = s_krs, .count = 2u, .err_msg = &err_msg));
    TEST_ASSERT_LESS_THAN_INT(0, EMBED_CALL(query_loops_err_msg_words.write_words, QueryLoopsErrMsgWordsCfg, .words = s_kdm, .count = 1u, .err_msg = &err_msg));
    TEST_ASSERT_LESS_THAN_INT(0, EMBED_CALL(query_loops_err_msg_words.write_words, QueryLoopsErrMsgWordsCfg, .words = NULL, .count = 0u, .err_msg = &err_msg));
}

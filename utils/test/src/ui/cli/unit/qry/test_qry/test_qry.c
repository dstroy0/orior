// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_qry.c: a finished run is read whole and then from where a read stopped, a run's bytes open as its names each joined, a tree's
// runs are listed, and a lesson line is read and any other skipped
#include "src/cu/includes/file_defs/qry/qry.h"
#include "src/cu/includes/file_defs/qry/qry_read.h"
#include "ui/cli/src/runs/qry.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static unsigned char s_run[4096];
static char s_out[1u << 16u];
static char s_why[512];
static char s_path[CLI_PATH_MAX_LEN];
static unsigned long long s_first_size;
static const CliJson *s_json;
static unsigned int s_read;
static unsigned int s_one;
static unsigned int s_panic;

// a record of `name` containing `blob`, written at `time` with `priority`, placed after the `at` bytes of s_run: the bytes after it
static unsigned long long record_put(unsigned long long at, const char *name, const char *blob, unsigned long long time, unsigned int priority)
{
    const unsigned long long name_size = CLI_UNITY_LENGTH(name);
    const unsigned long long blob_size = CLI_UNITY_LENGTH(blob);
    const unsigned long long filled = 24ull + name_size + blob_size;
    const unsigned long long whole = ((filled + 7ull) & ~7ull) + 8ull;
    unsigned char *const bytes = s_run + at;
    memor.set(&(MemoriaCfg){.dst = bytes, .val = 0, .bytes = whole});
    const unsigned long long fields[4] = {time, blob_size, name_size, priority};
    const unsigned int widths[4] = {8u, 8u, 4u, 4u};
    unsigned int place = 0u;
    for (unsigned int field = 0u; field < 4u; field += 1u)
    {
        for (unsigned int each = 0u; each < widths[field]; each += 1u)
        {
            // a byte of the field, its low eight bits
            bytes[place + each] = (unsigned char)((fields[field] >> (8u * each)) & 0xffu);
        }
        place += widths[field];
    }
    memor.cpy(&(MemoriaCfg){.dst = bytes + 24u, .src = name, .bytes = name_size});
    memor.cpy(&(MemoriaCfg){.dst = bytes + 24u + name_size, .src = blob, .bytes = blob_size});
    const unsigned long long crc = qry_record_crc(bytes, whole - 8ull);
    for (unsigned int each = 0u; each < 8u; each += 1u)
    {
        // a byte of the CRC, its low eight bits
        bytes[whole - 8ull + each] = (unsigned char)((crc >> (8u * each)) & 0xffu);
    }
    return at + whole;
}

// a whole number member of a node of the shared tree, 0 where it has none
static unsigned long long whole_member(unsigned int node, const char *name)
{
    const unsigned int number = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = node, .name = name);
    unsigned long long value = 0ull;
    for (unsigned long long at = 0ull; EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = s_json, .node = number, .kind = CFG_JSON_NUMBER) && (at < s_json->node[number].length); at += 1ull)
    {
        value = value * 10ull + (unsigned long long)(s_json->text[s_json->node[number].text + at] - '0');
    }
    return value;
}

// the run's file read from `from`, parsed: s_read its node
static void file_read_from(unsigned long long from)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_qry_path_from.read_file_from, CliQryPathFromCfg, .path = s_path, .from = from, .out = &out, .err_msg = &err_msg));
    s_json = cli_unity_parsed(s_out, out.at, &s_read);
}

// a finished run of an ordinary record and a panic written and read: s_one and s_panic their nodes
static void finished_run_read(void)
{
    char dir[CLI_PATH_MAX_LEN];
    cli_unity_folder("qry_read", dir);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = dir, .relative = "query1-x.qry", .joined = s_path);
    unsigned long long size = record_put(0ull, "a.txt", "one\n", 10ull, QRY_ORDINARY);
    size = record_put(size, "panic", "why\n", 20ull, QRY_PANIC);
    s_first_size = size;
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = s_path, .bytes = s_run, .size = s_first_size));
    file_read_from(0ull);
    const unsigned int blobs = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = s_read, .name = "blobs");
    s_one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = blobs, .index = 0u);
    s_panic = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = blobs, .index = 1u);
}

// a third record appended to the finished run
static void record_appended(void)
{
    const unsigned long long size = record_put(s_first_size, "a.txt", "two\n", 30ull, QRY_ORDINARY);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = s_path, .bytes = s_run, .size = size));
}

// a run of three records, two names: its size
static unsigned long long two_names_written(void)
{
    unsigned long long size = record_put(0ull, "b.txt", "1", 1ull, QRY_ORDINARY);
    size = record_put(size, "a.txt", "2", 2ull, QRY_ORDINARY);
    return record_put(size, "b.txt", "3", 3ull, QRY_ORDINARY);
}

// a tree of one run in a folder with lessons, its listing NUL-terminated in s_out
static void tree_listed(const char *name)
{
    char root[CLI_PATH_MAX_LEN];
    char runs[CLI_PATH_MAX_LEN];
    cli_unity_folder(name, root);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = "runs", .joined = runs);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = runs));
    cli_unity_file(root, "runs/one.qry", "");
    cli_unity_file(root, "runs/overseer.txt", "lesson time * a stand in\n");
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_qry_out_root.list, CliQryOutRootCfg, .root = root, .out = &out);
    (void)mmgr_line_finish(&out);
}

// the listing of `root` contains `wanted`
static int listing_contains(const char *root, const char *wanted)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_qry_out_root.list, CliQryOutRootCfg, .root = root, .out = &out);
    (void)mmgr_line_finish(&out);
    return cellul.find(&(CatenaFinitaCfg){.src = s_out, .cap = CLI_CELLS_POOL, .other = wanted, .other_cap = 64u}) != NULL;
}

// an empty run at `name` in its folder `folder` of `root`
static void empty_run_written(const char *root, const char *folder, const char *name)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = folder, .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path));
    cli_unity_file(root, name, "");
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_finished_run_is_not_live(void)
{
    finished_run_read();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = s_json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = s_read, .name = "live"), .kind = CFG_JSON_FALSE));
}

void test_a_runs_records_read_in_order(void)
{
    finished_run_read();
    TEST_ASSERT_EQUAL_UINT(2u, s_json->node[EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = s_read, .name = "blobs")].count);
    TEST_ASSERT_EQUAL_STRING("a.txt", cli_unity_member_text(s_one, "name"));
    TEST_ASSERT_EQUAL_STRING("panic", cli_unity_member_text(s_panic, "name"));
}

void test_a_record_reads_its_contents(void)
{
    finished_run_read();
    TEST_ASSERT_EQUAL_STRING("one\n", cli_unity_member_text(s_one, "text"));
}

void test_a_record_reads_its_time(void)
{
    finished_run_read();
    TEST_ASSERT_EQUAL_UINT64(20ull, whole_member(s_panic, "nanoseconds"));
}

void test_a_runs_panic_is_the_panic_record(void)
{
    finished_run_read();
    TEST_ASSERT_EQUAL_UINT64(whole_member(s_panic, "sequence"), whole_member(s_read, "panicked"));
}

void test_the_next_read_starts_past_the_last_record(void)
{
    finished_run_read();
    TEST_ASSERT_EQUAL_UINT64(s_first_size, whole_member(s_read, "next"));
}

void test_a_read_continues_from_where_the_last_stopped(void)
{
    finished_run_read();
    const unsigned long long next = whole_member(s_read, "next");
    record_appended();
    file_read_from(next);
    TEST_ASSERT_EQUAL_UINT(1u, s_json->node[EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = s_read, .name = "blobs")].count);
}

void test_a_names_records_join_in_the_order_written(void)
{
    finished_run_read();
    record_appended();
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_qry_path_out.joined_bytes, CliQryPathOutCfg, .path = s_path, .name = "a.txt", .out = &out, .err_msg = &err_msg));
    CLI_TEST_ASSERT_TEXT("one\ntwo\n", s_out, out.at);
}

void test_a_runs_names_come_in_the_order_of_their_first_records(void)
{
    const unsigned long long size = two_names_written();
    char names[4][CLI_NAME_MAX_LEN];
    TEST_ASSERT_EQUAL_UINT(2u, EMBED_CALL(cli_qry_data.member_names, CliQryDataCfg, .data = s_run, .size = size, .names = names, .room = 4u));
    TEST_ASSERT_EQUAL_STRING("b.txt", names[0]);
    TEST_ASSERT_EQUAL_STRING("a.txt", names[1]);
}

void test_a_names_records_are_joined(void)
{
    const unsigned long long size = two_names_written();
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_qry_out.member_join, CliQryOutCfg, .data = s_run, .size = size, .name = "b.txt", .out = &out);
    CLI_TEST_ASSERT_TEXT("13", s_out, out.at);
}

void test_another_names_records_are_joined(void)
{
    const unsigned long long size = two_names_written();
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_qry_out.member_join, CliQryOutCfg, .data = s_run, .size = size, .name = "a.txt", .out = &out);
    CLI_TEST_ASSERT_TEXT("2", s_out, out.at);
}

void test_a_trees_run_is_listed_with_its_folder(void)
{
    tree_listed("qry_listed_folder");
    CLI_TEST_ASSERT_CONTAINS("{\"path\":\"runs/one.qry\",\"folder\":\"runs\"", s_out);
}

void test_a_trees_run_is_listed_with_its_folders_lessons(void)
{
    tree_listed("qry_listed_lessons");
    CLI_TEST_ASSERT_CONTAINS("\"lessons\":[{\"cause\":\"time\"", s_out);
}

void test_the_walk_stores_its_folders_in_a_cell(void)
{
    tree_listed("qry_listed_cell");
    TEST_ASSERT_NOT_EQUAL(0ull, embed_atomic_usize_load(&g_cli_qry_listed_cells.in_use));
    TEST_ASSERT_NOT_EQUAL(0ull, embed_atomic_usize_load(&g_cli_qry_listed_cells.hw));
}

void test_a_trees_first_listing_walks_it(void)
{
    char root[CLI_PATH_MAX_LEN];
    cli_unity_folder("qry_watched_first", root);
    empty_run_written(root, "first", "first/one.qry");
    TEST_ASSERT_TRUE(listing_contains(root, "\"first/one.qry\""));
}

void test_a_folder_new_since_the_first_listing_is_not_walked(void)
{
    char root[CLI_PATH_MAX_LEN];
    cli_unity_folder("qry_watched_new", root);
    empty_run_written(root, "first", "first/one.qry");
    (void)listing_contains(root, "\"first/one.qry\"");
    empty_run_written(root, "later", "later/two.qry");
    TEST_ASSERT_FALSE(listing_contains(root, "\"later/two.qry\""));
}

void test_a_run_the_watch_saw_written_is_listed(void)
{
    char root[CLI_PATH_MAX_LEN];
    cli_unity_folder("qry_watched_written", root);
    empty_run_written(root, "first", "first/one.qry");
    (void)listing_contains(root, "\"first/one.qry\"");
    empty_run_written(root, "later", "later/two.qry");
    EMBED_CALL(cli_qry_path_root.run_write, CliQryPathRootCfg, .root = root, .path = "later/two.qry", .length = 13ull);
    TEST_ASSERT_TRUE(listing_contains(root, "\"later/two.qry\""));
}

void test_a_tree_is_walked_again_after_the_watch_lost_changes(void)
{
    char root[CLI_PATH_MAX_LEN];
    cli_unity_folder("qry_watched_lost", root);
    empty_run_written(root, "first", "first/one.qry");
    (void)listing_contains(root, "\"first/one.qry\"");
    empty_run_written(root, "lost", "lost/three.qry");
    EMBED_CALL(cli_qry_out_root.runs_lose, CliQryOutRootCfg, .root = root);
    TEST_ASSERT_TRUE(listing_contains(root, "\"lost/three.qry\""));
}

void test_a_lesson_line_is_read_and_any_other_skipped(void)
{
    static const char s_overseer[] = "lesson time * a stand in\nnot a lesson\nlesson register 3\n";
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_qry_out_size.lessons_write, CliQryOutSizeCfg, .text = s_overseer, .size = sizeof(s_overseer) - 1u, .out = &out);
    CLI_TEST_ASSERT_TEXT("{\"cause\":\"time\",\"carrier\":\"*\",\"stand_in\":\"a stand in\"}", s_out, out.at);
}

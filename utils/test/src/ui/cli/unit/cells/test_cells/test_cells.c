// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_cells.c: the memory pool: a new pool is zero with no cell allocated, an allocation is counted to its owner and listed in the audit,
// an allocation past the pool is refused, a file of whole pages in a cell is measured and searched past its end, and a movable cell
// moves at a quiet time with its bytes. The cases run in source order: the mover's is last, the pool not being made anew once the option
// is assigned
#include "ui/cli/src/code/symbols.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/command_line/cells.h"
#include "ui/cli/src/editor/files.h"
#include "ui/cli/src/editor/files_internal.h"
#include "ui/cli/src/plugins/plugins.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

#define PAGE 4096ull

// the owner every cell here is counted to
static CellOwner s_checked = MMGR_CELL_OWNER("cells_check");

static unsigned long long s_pool;
static unsigned long long s_persistent;
static unsigned long long s_temporary;
static char s_list_result[1u << 14u];
static char s_bytes[2u * PAGE];
static char s_folder[CLI_PATH_MAX_LEN];

// a whole number member of a node, 0 where it has none
static unsigned long long integer_member(const CliJson *json, unsigned int node, const char *name)
{
    const unsigned int number = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = node, .name = name);
    unsigned long long value = 0ull;
    for (unsigned long long at = 0ull; EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = number, .kind = CFG_JSON_NUMBER) && (at < json->node[number].length); at += 1ull)
    {
        value = value * 10ull + (unsigned long long)(json->text[json->node[number].text + at] - '0');
    }
    return value;
}

// a found path counted
static void path_counted(const CliEachPathLengthCfg *args)
{
    void *context = args->context;
    *(unsigned int *)context += 1u;
}

// the pool made anew at its full size, which resets the peaks
static void pool_created_anew(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL));
}

// the tiers' peaks read into s_pool, s_persistent and s_temporary
static void peaks_read(void)
{
    EMBED_CALL(cli_cells_table.report, CliCellsCfg, .pool = &s_pool, .persistent = &s_persistent, .temporary = &s_temporary);
}

// a file of `pages` pages stored in a cell is whole, measured to its end, and searched without reading past it
static void assert_full_text_pages(const char *name, unsigned int pages)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_folder, .relative = name, .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = path, .bytes = s_bytes, .size = pages * PAGE, .fresh = 1));
    unsigned long long size = 0ull;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path, .size_pointer = &size, .owner = &g_checks_cells);
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_EQUAL_UINT64(pages * PAGE, size);
    TEST_ASSERT_EQUAL_UINT64(pages * PAGE, cellul.len(&(CatenaFinitaCfg){.src = (const char *)bytes, .cap = CLI_CELLS_POOL}));
    TEST_ASSERT_NULL(cellul.chr(&(CatenaFinitaCfg){.src = (const char *)bytes, .cap = CLI_CELLS_POOL, .byte = '!'}));
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_checks_cells);
}

void setUp(void)
{
    static int s_registered;
    cli_unity_begun();
    if (!s_registered)
    {
        (void)mmgr_owner_register(&s_checked);
        s_registered = 1;
    }
    cli_unity_folder("cells", s_folder);
    for (unsigned long long at = 0ull; at < sizeof(s_bytes); at += 1ull)
    {
        // a letter of the alphabet
        s_bytes[at] = (char)('a' + (at % 26ull));
    }
}

void tearDown(void)
{
    cli_unity_parsed_released();
}

void test_a_pool_created_for_a_request_is_zero_with_no_cell_allocated(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = 64ull << 20u));
    TEST_ASSERT_TRUE(cli_cells_zero());
}

void test_a_cell_is_allocated_from_a_pool_created_for_a_request(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = 64ull << 20u));
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(1000ull, &s_checked);
    TEST_ASSERT_NOT_NULL(cell);
    g_cli_cells->persistent_buf_release(cell, &s_checked);
}

void test_the_pool_is_not_created_anew_while_a_cell_is_allocated(void)
{
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(1000ull, &s_checked);
    const int made = EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = 32ull << 20u);
    g_cli_cells->persistent_buf_release(cell, &s_checked);
    TEST_ASSERT_FALSE(made);
}

void test_the_pool_is_created_anew_once_the_cell_is_released(void)
{
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(1000ull, &s_checked);
    g_cli_cells->persistent_buf_release(cell, &s_checked);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = 32ull << 20u));
}

void test_a_new_pool_has_reached_nothing(void)
{
    pool_created_anew();
    peaks_read();
    TEST_ASSERT_EQUAL_UINT64(0ull, s_persistent);
    TEST_ASSERT_EQUAL_UINT64(0ull, s_temporary);
}

void test_a_walk_finds_the_folders_files(void)
{
    unsigned int found = 0u;
    EMBED_CALL(cli_files_root_each_context.walk, CliFilesRootEachContextCfg, .root = ".", .each = path_counted, .context = &found);
    TEST_ASSERT_NOT_EQUAL(0u, found);
}

void test_a_walks_paths_stay_out_of_the_persistent_tier(void)
{
    unsigned int found = 0u;
    pool_created_anew();
    EMBED_CALL(cli_files_root_each_context.walk, CliFilesRootEachContextCfg, .root = ".", .each = path_counted, .context = &found);
    peaks_read();
    TEST_ASSERT_EQUAL_UINT64(0ull, s_persistent);
}

void test_a_walks_paths_are_in_the_temporary_tier(void)
{
    unsigned int found = 0u;
    pool_created_anew();
    EMBED_CALL(cli_files_root_each_context.walk, CliFilesRootEachContextCfg, .root = ".", .each = path_counted, .context = &found);
    peaks_read();
    TEST_ASSERT_NOT_EQUAL(0ull, s_temporary);
}

void test_a_list_is_in_the_persistent_tier(void)
{
    CliFiles files;
    pool_created_anew();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_bytes_internal.init, CliFilesBytesCtx, .files = &files, .count = 2ull, .bytes = 8ull));
    (void)EMBED_CALL(cli_files_path_length_internal.put, CliFilesPathLengthCtx, .files = &files, .path = "b", .length = 1ull);
    (void)EMBED_CALL(cli_files_path_length_internal.put, CliFilesPathLengthCtx, .files = &files, .path = "a", .length = 1ull);
    peaks_read();
    EMBED_CALL(cli_files_bytes_internal.clear, CliFilesBytesCtx, .files = &files);
    TEST_ASSERT_NOT_EQUAL(0ull, s_persistent);
}

void test_a_list_is_sorted_and_its_reserve_list_stays_out_of_the_persistent_tier(void)
{
    CliFiles files;
    pool_created_anew();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_bytes_internal.init, CliFilesBytesCtx, .files = &files, .count = 2ull, .bytes = 8ull));
    (void)EMBED_CALL(cli_files_path_length_internal.put, CliFilesPathLengthCtx, .files = &files, .path = "b", .length = 1ull);
    (void)EMBED_CALL(cli_files_path_length_internal.put, CliFilesPathLengthCtx, .files = &files, .path = "a", .length = 1ull);
    peaks_read();
    const unsigned long long list_entries = s_persistent;
    const int sorted = EMBED_CALL(cli_files_bytes_internal.sort, CliFilesBytesCtx, .files = &files);
    const unsigned long long first = files.start[0];
    peaks_read();
    EMBED_CALL(cli_files_bytes_internal.clear, CliFilesBytesCtx, .files = &files);
    TEST_ASSERT_TRUE(sorted);
    TEST_ASSERT_EQUAL_UINT64(2ull, first);
    TEST_ASSERT_EQUAL_UINT64(list_entries, s_persistent);
}

void test_the_languages_for_a_call_stay_out_of_the_persistent_tier(void)
{
    CliLanguages languages;
    pool_created_anew();
    EMBED_CALL(cli_plugins_table.languages_for_call, CliPluginsCfg, .languages = &languages);
    const int reserved = languages.pair != NULL;
    peaks_read();
    EMBED_CALL(cli_plugins_table.languages_free, CliPluginsCfg, .languages = &languages);
    TEST_ASSERT_TRUE(reserved);
    TEST_ASSERT_EQUAL_UINT64(0ull, s_persistent);
}

void test_the_languages_that_outlive_a_call_are_in_the_persistent_tier(void)
{
    CliLanguages languages;
    pool_created_anew();
    EMBED_CALL(cli_plugins_table.languages, CliPluginsCfg, .languages = &languages);
    const int reserved = languages.pair != NULL;
    peaks_read();
    EMBED_CALL(cli_plugins_table.languages_free, CliPluginsCfg, .languages = &languages);
    TEST_ASSERT_TRUE(reserved);
    TEST_ASSERT_GREATER_THAN_UINT64(0ull, s_persistent);
}

void test_the_languages_a_call_reads_stay_out_of_the_persistent_tier(void)
{
    pool_created_anew();
    CliSymbolsLanguages *const read = cli_symbols_languages_for_call();
    peaks_read();
    TEST_ASSERT_NOT_NULL(read);
    EMBED_CALL(cli_symbols.languages_free, CliSymbolsCfg, .read = read);
    TEST_ASSERT_EQUAL_UINT64(0ull, s_persistent);
}

void test_the_walks_and_the_lists_cells_are_released(void)
{
    unsigned int found = 0u;
    EMBED_CALL(cli_files_root_each_context.walk, CliFilesRootEachContextCfg, .root = ".", .each = path_counted, .context = &found);
    TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(&g_cli_files_views_cells));
}

void test_an_allocation_is_counted_to_its_owner(void)
{
    void *const counted = g_cli_cells->persistent_buf_alloc(1000ull, &s_checked);
    TEST_ASSERT_NOT_NULL(counted);
    const unsigned long long in_use = embed_atomic_usize_load(&s_checked.in_use);
    g_cli_cells->persistent_buf_release(counted, &s_checked);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(1000ull, in_use);
}

void test_the_audit_lists_the_owner_with_its_bytes(void)
{
    void *const counted = g_cli_cells->persistent_buf_alloc(1000ull, &s_checked);
    VerbaLinea listing = {s_list_result, sizeof(s_list_result), 0ull};
    EMBED_CALL(cli_cells_table.owners_write, CliCellsCfg, .out = &listing);
    TEST_ASSERT_TRUE(mmgr_line_ok(&listing));
    const unsigned long long in_use = embed_atomic_usize_load(&s_checked.in_use);
    unsigned int owners = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(s_list_result, listing.at, &owners);
    unsigned long long list_entries = 0ull;
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = owners, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = owners, .index = at);
        const char *const name = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = one, .name = "name"), .length_pointer = NULL);
        if ((name != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "cells_check", .cap = 64u}))
        {
            list_entries = integer_member(json, one, "in_use");
        }
    }
    g_cli_cells->persistent_buf_release(counted, &s_checked);
    TEST_ASSERT_EQUAL_UINT64(in_use, list_entries);
}

void test_a_released_cell_comes_off_its_owner(void)
{
    void *const counted = g_cli_cells->persistent_buf_alloc(1000ull, &s_checked);
    g_cli_cells->persistent_buf_release(counted, &s_checked);
    TEST_ASSERT_EQUAL_UINT64(0ull, embed_atomic_usize_load(&s_checked.in_use));
}

void test_an_allocation_end_the_pool_is_refused_and_counts_nothing(void)
{
    // the pool's size is a size_t's on the machines this runs on
    TEST_ASSERT_NULL(g_cli_cells->persistent_buf_alloc((size_t)CLI_CELLS_POOL, &s_checked));
    TEST_ASSERT_EQUAL_UINT64(0ull, embed_atomic_usize_load(&s_checked.in_use));
}

void test_a_file_of_one_page_ends_where_the_file_ends(void)
{
    assert_full_text_pages("one.txt", 1u);
}

void test_a_file_of_two_pages_ends_where_the_file_ends(void)
{
    assert_full_text_pages("two.txt", 2u);
}

void test_a_movable_cell_moves_at_a_quiet_time_with_its_bytes(void)
{
    enum
    {
        CELL_BYTES = 4096,
        HOLE_BYTES = 1 << 20,
        PATIENCE_MS = 20000
    };
    static unsigned char s_written[CELL_BYTES];
    static unsigned char s_read[CELL_BYTES];
    static CliWake s_wake = CLI_WAKE_INIT;
    cli_cells_movable_ready();
    void *const hole = g_cli_cells->persistent_buf_alloc(HOLE_BYTES, &s_checked);
    const CarcerMovableSlot *const cell = g_cli_cells->movable_buf_alloc(CELL_BYTES, &s_checked);
    TEST_ASSERT_NOT_NULL(hole);
    TEST_ASSERT_NOT_NULL(cell);
    for (unsigned int at = 0u; at < CELL_BYTES; at += 1u)
    {
        // a byte of the pattern, its low eight bits
        s_written[at] = (unsigned char)(at * 7u);
    }
    TEST_ASSERT_TRUE(g_cli_cells->movable_buf_write(&(CarcerMovableCfg){.cell = cell, .src = s_written, .bytes = CELL_BYTES}));
    TEST_ASSERT_FALSE_MESSAGE(g_cli_cells->movable_buf_write(&(CarcerMovableCfg){.cell = cell, .src = s_written, .bytes = CELL_BYTES + 1u}), "a write past its end");
    const size_t gap = g_cli_cells->buf_available();
    g_cli_cells->persistent_buf_release(hole, &s_checked);
    const unsigned long long began = cli_clock_milliseconds();
    while ((g_cli_cells->buf_available() <= gap) && ((cli_clock_milliseconds() - began) < PATIENCE_MS))
    {
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &s_wake, .milliseconds_u64 = 50ull);
    }
    TEST_ASSERT_GREATER_THAN_UINT64(gap, g_cli_cells->buf_available());
    TEST_ASSERT_TRUE(g_cli_cells->movable_buf_read(&(CarcerMovableCfg){.cell = cell, .dst = s_read, .bytes = CELL_BYTES}));
    TEST_ASSERT_EQUAL_MEMORY(s_written, s_read, CELL_BYTES);
    TEST_ASSERT_TRUE(g_cli_cells->movable_buf_release(cell, &s_checked));
    TEST_ASSERT_EQUAL_UINT64(0ull, embed_atomic_usize_load(&s_checked.in_use));
    TEST_ASSERT_FALSE_MESSAGE(EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL), "the pool made anew with the mover reading it");
}

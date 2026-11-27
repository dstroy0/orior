// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_pool.c: the memory pool: a pool emptied for a request is idle, a cell is counted to the owner named and
// listed in the audit, a take past the pool is refused, the owner table holds the owners declared, a file of whole
// pages in a cell is measured and searched past its end, and a movable cell moves at a quiet time with its bytes. The
// cases run in source order: the mover's is last, the pool not being made anew once the option is assigned
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/code/symbols.h"
#include "ui/cli/src/code/symbols_internal.h"
#include "ui/cli/src/command_line/pool.h"
#include "ui/cli/src/editor/files.h"
#include "ui/cli/src/editor/files_internal.h"
#include "ui/cli/src/plugins/plugins.h"
#include "ui/cli/src/text/text.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

#define PAGE 4096ull

static unsigned long long s_pool;
static unsigned long long s_persistent;
static unsigned long long s_temporary;
static char s_list_result[1u << 14u];
static char s_bytes[2u * PAGE];
static char s_folder[CLI_PATH_MAX_LEN];

// a found path counted
static void path_counted(const CliEachPathLengthCfg *args)
{
    void *context = args->context;
    *(unsigned int *)context += 1u;
}

// the pool made anew at its full size, which resets the peaks
static void pool_created_anew(void)
{
    cli_plugins_languages_end();
    TEST_ASSERT_TRUE(cli_pool_made(CLI_POOL_BYTES));
}

// the tiers' peaks read into s_pool, s_persistent and s_temporary
static void peaks_read(void)
{
    cli_pool_report(&s_pool, &s_persistent, &s_temporary);
}

// a file of `pages` pages stored in a cell is whole, measured to its end, and searched without reading past it
static void assert_full_text_pages(const char *name, unsigned int pages)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_folder, .relative = name, .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = path, .bytes = s_bytes,
                                .size = pages * PAGE, .fresh = 1));
    unsigned long long size = 0ull;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                  .size_pointer = &size, .owner = g_checks_owner);
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_EQUAL_UINT64(pages * PAGE, size);
    TEST_ASSERT_EQUAL_UINT64(pages * PAGE,
                             cellul.len(&(CatenaFinitaCfg){.src = (const char *)bytes, .cap = CLI_POOL_BYTES}));
    TEST_ASSERT_NULL(cellul.chr(&(CatenaFinitaCfg){.src = (const char *)bytes, .cap = CLI_POOL_BYTES, .byte = '!'}));
    g_platform_guard->persistent_buf_release((void *)(uintptr_t)bytes, g_checks_owner);
}

void setUp(void)
{
    cli_unity_begun();
    cli_unity_folder("pool", s_folder);
    for (unsigned long long at = 0ull; at < sizeof(s_bytes); at += 1ull)
    {
        // a character of the alphabet
        s_bytes[at] = (char)('a' + (at % 26ull));
    }
}

void tearDown(void)
{
    cli_unity_parsed_released();
}

void test_a_pool_emptied_for_a_request_is_idle_with_no_cell_allocated(void)
{
    TEST_ASSERT_TRUE(cli_pool_made(64ull << 20u));
    TEST_ASSERT_TRUE(mmgr_series_cellularum_idle(platform_pool_series()));
}

void test_a_request_past_the_pool_declared_is_refused(void)
{
    TEST_ASSERT_FALSE(cli_pool_made(CLI_POOL_BYTES + 1ull));
}

void test_a_cell_is_allocated_from_a_pool_emptied_for_a_request(void)
{
    TEST_ASSERT_TRUE(cli_pool_made(64ull << 20u));
    unsigned char *const cell = g_platform_guard->persistent_buf_alloc(1000ull, g_checks_owner);
    TEST_ASSERT_NOT_NULL(cell);
    g_platform_guard->persistent_buf_release(cell, g_checks_owner);
}

void test_the_pool_is_not_emptied_while_a_cell_is_allocated(void)
{
    unsigned char *const cell = g_platform_guard->persistent_buf_alloc(1000ull, g_checks_owner);
    const int made = cli_pool_made(32ull << 20u);
    g_platform_guard->persistent_buf_release(cell, g_checks_owner);
    TEST_ASSERT_FALSE(made);
}

void test_the_pool_is_emptied_once_the_cell_is_released(void)
{
    unsigned char *const cell = g_platform_guard->persistent_buf_alloc(1000ull, g_checks_owner);
    g_platform_guard->persistent_buf_release(cell, g_checks_owner);
    TEST_ASSERT_TRUE(cli_pool_made(32ull << 20u));
}

void test_a_new_pool_has_reached_nothing(void)
{
    pool_created_anew();
    peaks_read();
    TEST_ASSERT_EQUAL_UINT64(0ull, s_persistent);
    TEST_ASSERT_EQUAL_UINT64(0ull, s_temporary);
}

void test_the_owner_table_holds_every_owner_declared_at_its_place(void)
{
    TEST_ASSERT_TRUE(mmgr_owner_table_count > 0u);
    TEST_ASSERT_TRUE(mmgr_owner_listed(g_checks_owner));
    TEST_ASSERT_EQUAL_PTR(&mmgr_owner_table[g_checks_owner->place - 1u], g_checks_owner);
    TEST_ASSERT_EQUAL_STRING("checks", g_checks_owner->name);
    TEST_ASSERT_FALSE(mmgr_owner_listed(NULL));
}

void test_a_walk_finds_the_folders_files(void)
{
    unsigned int found = 0u;
    EMBED_CALL(cli_files_root_each_context.walk, CliFilesRootEachContextCfg, .root = ".", .each = path_counted,
               .context = &found);
    TEST_ASSERT_NOT_EQUAL(0u, found);
}

void test_a_walks_paths_stay_out_of_the_persistent_tier(void)
{
    unsigned int found = 0u;
    pool_created_anew();
    EMBED_CALL(cli_files_root_each_context.walk, CliFilesRootEachContextCfg, .root = ".", .each = path_counted,
               .context = &found);
    peaks_read();
    TEST_ASSERT_EQUAL_UINT64(0ull, s_persistent);
}

void test_a_walks_paths_are_in_the_temporary_tier(void)
{
    unsigned int found = 0u;
    pool_created_anew();
    EMBED_CALL(cli_files_root_each_context.walk, CliFilesRootEachContextCfg, .root = ".", .each = path_counted,
               .context = &found);
    peaks_read();
    TEST_ASSERT_NOT_EQUAL(0ull, s_temporary);
}

void test_a_list_is_sorted_and_its_reserve_list_stays_out_of_the_persistent_tier(void)
{
    CliFiles files;
    pool_created_anew();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_bytes_internal.work_init, CliFilesBytesCtx, .files = &files, .count = 2ull,
                                .bytes = 8ull));
    (void)EMBED_CALL(cli_files_path_length_internal.put, CliFilesPathLengthCtx, .files = &files, .path = "b",
                     .length = 1ull);
    (void)EMBED_CALL(cli_files_path_length_internal.put, CliFilesPathLengthCtx, .files = &files, .path = "a",
                     .length = 1ull);
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

void test_the_languages_are_read_in_the_temporary_tier_and_kept_in_one_cell_sized_to_them(void)
{
    pool_created_anew();
    char id[CLI_NAME_MAX_LEN];
    TEST_ASSERT_TRUE(EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg,
                                .path = "build.zig", .id_out = id));
    peaks_read();
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64((unsigned long long)CLI_LANGUAGE_PAIRS * 2ull * CLI_NAME_MAX_LEN, s_temporary);
    TEST_ASSERT_LESS_THAN_UINT64((unsigned long long)CLI_LANGUAGE_PAIRS * 2ull * CLI_NAME_MAX_LEN, s_persistent);
}

void test_a_grammar_a_call_reads_takes_no_persistent_cell(void)
{
    pool_created_anew();
    char id[CLI_NAME_MAX_LEN];
    TEST_ASSERT_TRUE(EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg,
                                .path = "build.zig", .id_out = id));
    peaks_read();
    const unsigned long long built = s_persistent;
    CliLanguageGrammar grammar = {0};
    const int read = EMBED_CALL(cli_plugins_languages_path.grammar_of, CliPluginsGrammarOfCfg, .path = "build.zig",
                                .id = NULL, .store = 0, .grammar_out = &grammar);
    peaks_read();
    cli_plugins_languages_end();
    TEST_ASSERT_TRUE(read);
    // the languages' cell stays in the pool through the read: a persistent take of the read raises the peak past it
    TEST_ASSERT_EQUAL_UINT64(built, s_persistent);
}

void test_the_walks_and_the_lists_cells_are_released(void)
{
    unsigned int found = 0u;
    EMBED_CALL(cli_files_root_each_context.walk, CliFilesRootEachContextCfg, .root = ".", .each = path_counted,
               .context = &found);
    TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(g_cli_files_views_owner));
}

void test_a_take_is_counted_to_its_owner(void)
{
    void *const counted = g_platform_guard->persistent_buf_alloc(1000ull, g_checks_owner);
    TEST_ASSERT_NOT_NULL(counted);
    const unsigned long long in_use = mmgr_owner_in_use(g_checks_owner);
    g_platform_guard->persistent_buf_release(counted, g_checks_owner);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(1000ull, in_use);
}

// the bytes in use the audit's line for the owner `name` gives, 0 where it has no line
static unsigned long long listed_in_use(const char *listing, const char *name)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = 64u});
    const char *line = listing;
    while (*line != '\0')
    {
        if ((memor.cmp(&(MemoriaCfg){.src = line, .other = name, .bytes = length}) == 0) && (line[length] == '\t'))
        {
            return cellul.to_ulong(&(TransfiguroCfg){.src = line + length + 1u});
        }
        line += cellul.span_not(&(CatenaFinitaCfg){.src = line, .cap = CLI_POOL_BYTES, .other = "\n"});
        line += (*line != '\0') ? 1u : 0u;
    }
    return 0ull;
}

void test_the_audit_lists_the_owner_with_its_bytes(void)
{
    void *const counted = g_platform_guard->persistent_buf_alloc(1000ull, g_checks_owner);
    VerbaLinea listing = {s_list_result, sizeof(s_list_result), 0ull};
    cli_pool_owners_write(&listing);
    TEST_ASSERT_TRUE(mmgr_line_ok(&listing));
    (void)mmgr_line_finish(&listing);
    const unsigned long long in_use = mmgr_owner_in_use(g_checks_owner);
    const unsigned long long list_entries = listed_in_use(s_list_result, "checks");
    g_platform_guard->persistent_buf_release(counted, g_checks_owner);
    TEST_ASSERT_EQUAL_UINT64(in_use, list_entries);
}

void test_a_released_cell_comes_off_its_owner(void)
{
    void *const counted = g_platform_guard->persistent_buf_alloc(1000ull, g_checks_owner);
    g_platform_guard->persistent_buf_release(counted, g_checks_owner);
    TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(g_checks_owner));
}

void test_a_cell_answers_the_owner_it_counts_to(void)
{
    void *const counted = g_platform_guard->persistent_buf_alloc(1000ull, g_checks_owner);
    TEST_ASSERT_EQUAL_PTR(g_checks_owner, mmgr_owner_of_buf(platform_pool_series(), counted));
    g_platform_guard->persistent_buf_release(counted, g_checks_owner);
    TEST_ASSERT_NULL(mmgr_owner_of_buf(platform_pool_series(), counted));
}

void test_a_take_past_the_pool_is_refused_and_counts_nothing(void)
{
    // the pool's size is a size_t's on the machines this runs on
    TEST_ASSERT_NULL(g_platform_guard->persistent_buf_alloc((size_t)CLI_POOL_BYTES, g_checks_owner));
    TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(g_checks_owner));
}

void test_a_file_of_one_page_ends_where_the_file_ends(void)
{
    assert_full_text_pages("one.txt", 1u);
}

void test_a_file_of_two_pages_ends_where_the_file_ends(void)
{
    assert_full_text_pages("two.txt", 2u);
}

void test_a_grown_line_holds_a_long_text_whole(void)
{
    enum
    {
        PIECE = 1000,
        PIECES = 300
    };
    static char s_piece[PIECE];
    for (unsigned int at = 0u; at < PIECE; at += 1u)
    {
        // a byte of the pattern, a character of the alphabet
        s_piece[at] = (char)('a' + (at % 26u));
    }
    PlatformText grown;
    platform_text_begun(&grown, ((unsigned long long)PIECE * PIECES) + 1ULL, platform_place_g_checks_owner, PLATFORM_TIER_TEMPORARY);
    for (unsigned int piece = 0u; piece < PIECES; piece += 1u)
    {
        mmgr_line_put_n(&grown.line, s_piece, PIECE);
    }
    TEST_ASSERT_TRUE(mmgr_line_ok(&grown.line));
    TEST_ASSERT_EQUAL_UINT64((unsigned long long)PIECE * PIECES, platform_text_size(&grown));
    TEST_ASSERT_EQUAL_CHAR('\0', grown.cell[platform_text_size(&grown)]);
    for (unsigned int piece = 0u; piece < PIECES; piece += 1u)
    {
        TEST_ASSERT_EQUAL_MEMORY(s_piece, grown.cell + ((unsigned long long)piece * PIECE), PIECE);
    }
    platform_text_ended(&grown);
    TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(g_checks_owner));
}

void test_a_grown_line_fills_at_the_max_it_is_given(void)
{
    enum
    {
        PIECE = 65536
    };
    static char s_piece[PIECE];
    PlatformText grown;
    platform_text_begun(&grown, 3ull * PIECE, platform_place_g_checks_owner, PLATFORM_TIER_TEMPORARY);
    for (unsigned int piece = 0u; piece < 4u; piece += 1u)
    {
        mmgr_line_put_n(&grown.line, s_piece, sizeof(s_piece));
    }
    TEST_ASSERT_FALSE(mmgr_line_ok(&grown.line));
    TEST_ASSERT_EQUAL_UINT64((3ull * PIECE) - 1ull, platform_text_size(&grown));
    TEST_ASSERT_EQUAL_CHAR('\0', grown.cell[platform_text_size(&grown)]);
    platform_text_ended(&grown);
    TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(g_checks_owner));
}

void test_a_grown_line_begun_over_again_keeps_its_cell_at_the_size_it_grew_to(void)
{
    PlatformText grown;
    platform_text_begun(&grown, 1ull << 20u, platform_place_g_checks_owner, PLATFORM_TIER_TEMPORARY);
    mmgr_line_put(&grown.line, "some text that grows the line past its first cell's worth of bytes");
    const unsigned long long capacity = grown.capacity;
    platform_text_reset(&grown);
    TEST_ASSERT_EQUAL_UINT64(0ull, platform_text_size(&grown));
    TEST_ASSERT_EQUAL_UINT64(capacity, grown.capacity);
    mmgr_line_put(&grown.line, "ab");
    TEST_ASSERT_EQUAL_UINT64(2ull, platform_text_size(&grown));
    TEST_ASSERT_EQUAL_MEMORY("ab", grown.cell, 2u);
    platform_text_ended(&grown);
    TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(g_checks_owner));
}

void test_an_owners_usage_is_the_pools_bytes_its_peak_and_a_zero_and_the_reports_run_outside_a_run(void)
{
    char text[96];
    char pool[32];
    VerbaLinea out = mmgr_line_over(text, sizeof(text));
    VerbaLinea pool_line = mmgr_line_over(pool, sizeof(pool));
    void *const counted = g_platform_guard->persistent_buf_alloc(1000ull, g_checks_owner);
    g_platform_guard->persistent_buf_release(counted, g_checks_owner);
    cli_pool_owner_usage_write(g_checks_owner, &out);
    (void)mmgr_line_finish(&out);
    mmgr_line_decimal(&pool_line, CLI_POOL_BYTES, 1u);
    mmgr_line_ch(&pool_line, ' ');
    (void)mmgr_line_finish(&pool_line);
    TEST_ASSERT_TRUE_MESSAGE(cellul.starts(&(CatenaFinitaCfg){.src = text, .other = pool, .cap = sizeof(text)}), text);
    TEST_ASSERT_TRUE_MESSAGE(
        EMBED_CALL(cli_text_size_suffix.ends, CliTextSizeSuffixCfg, .text = text, .size = out.at, .suffix = " 0"),
        text);
    TEST_ASSERT_EQUAL_UINT64(
        2ull, EMBED_CALL(cli_text_size_byte.count, CliTextSizeByteCfg, .text = text, .size = out.at, .byte = ' '));
    // a process in no run reports nothing, and the telemetry line goes to the error stream
    cli_pool_peaks_report();
    cli_pool_reported("checks", &out);
    cli_pool_telemetry_log();
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
    void *const hole = g_platform_guard->persistent_buf_alloc(HOLE_BYTES, g_checks_owner);
    const CarcerMovableEntry *const cell = g_platform_guard->movable_buf_alloc(CELL_BYTES, g_checks_owner);
    TEST_ASSERT_NOT_NULL(hole);
    TEST_ASSERT_NOT_NULL(cell);
    for (unsigned int at = 0u; at < CELL_BYTES; at += 1u)
    {
        // a byte of the pattern, its low eight bits
        s_written[at] = (unsigned char)(at * 7u);
    }
    TEST_ASSERT_TRUE(
        g_platform_guard->movable_buf_write(&(CarcerMovableCfg){.cell = cell, .src = s_written, .bytes = CELL_BYTES}));
    TEST_ASSERT_FALSE_MESSAGE(
        g_platform_guard->movable_buf_write(&(CarcerMovableCfg){.cell = cell, .src = s_written, .bytes = CELL_BYTES + 1u}),
        "a write past its end");
    const size_t gap = g_platform_guard->buf_available();
    g_platform_guard->persistent_buf_release(hole, g_checks_owner);
    const unsigned long long began = cli_clock_milliseconds();
    while ((g_platform_guard->buf_available() <= gap) && ((cli_clock_milliseconds() - began) < PATIENCE_MS))
    {
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &s_wake, .milliseconds_u64 = 50ull);
    }
    TEST_ASSERT_GREATER_THAN_UINT64(gap, g_platform_guard->buf_available());
    TEST_ASSERT_TRUE(
        g_platform_guard->movable_buf_read(&(CarcerMovableCfg){.cell = cell, .dst = s_read, .bytes = CELL_BYTES}));
    TEST_ASSERT_EQUAL_MEMORY(s_written, s_read, CELL_BYTES);
    TEST_ASSERT_TRUE(g_platform_guard->movable_buf_release(cell, g_checks_owner));
    TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(g_checks_owner));
    // a pool reset stops the mover first and then restarts it: a movable allocation succeeds after the reset as before
    TEST_ASSERT_TRUE_MESSAGE(cli_pool_made(CLI_POOL_BYTES), "the pool made anew with the mover running");
    const CarcerMovableEntry *const again = g_platform_guard->movable_buf_alloc(CELL_BYTES, g_checks_owner);
    TEST_ASSERT_NOT_NULL_MESSAGE(again, "a movable cell taken from the pool made anew");
    TEST_ASSERT_TRUE(g_platform_guard->movable_buf_release(again, g_checks_owner));
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_query_pairs.c: the pairs' modes: the part's name for the vendor's reader, the modes and their targets, the dry carrier's flags
// first as the carrier reads them, the kernel handed before the frame and the pairs, and the words read
#include "ui/cli/src/text/text.h"
#include "ui/query/src/learn/pairs_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static QueryWords s_words;
static char s_out[1024];
static QueryPairsMode s_mode;

// the words joined by spaces from `from` into `out`, `capacity` bytes
static const char *words_joined(unsigned int from, char *out, unsigned long long capacity)
{
    VerbaLinea line = {out, capacity, 0ull};
    for (unsigned int at = from; at < s_words.count; at += 1u)
    {
        mmgr_line_put(&line, (at > from) ? " " : "");
        mmgr_line_put(&line, s_words.word_text[at]);
    }
    (void)mmgr_line_finish(&line);
    return out;
}

// the targets of `mode` over part sm_86 in the tree T, with `words` given, joined by spaces
static const char *targets_of(const char *mode, const char *const *words, unsigned int count)
{
    QueryContext context;
    memor.set(&(MemoriaCfg){.dst = &context, .val = 0, .bytes = sizeof(context)});
    context.root = "T";
    QueryPairsMode read;
    memor.set(&(MemoriaCfg){.dst = &read, .val = 0, .bytes = sizeof(read)});
    read.mode = mode;
    read.part = "sm_86";
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        read.words[at] = words[at];
    }
    read.word_count = count;
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    EMBED_CALL(query_pairs_words_internal.targets, QueryPairsWordsCtx, .context_read = &context, .mode = &read, .words = &s_words);
    return words_joined(0u, s_out, sizeof(s_out));
}

// the words of the carrier, dry or live
static void transport_words(int dry)
{
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    EMBED_CALL(query_pairs_part_dry_internal.machine_transfer_words, QueryPairsPartDryCtx, .words = &s_words, .transport = "C", .cross = "X", .nvdisasm = "N", .machine = "M", .layout = "L", .part = "sm_86", .dry = dry);
}

// `words` read as a pairs' mode into s_mode: 1, or 0 where they are refused
static int mode_read(const char *const *words, unsigned int count)
{
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    return EMBED_CALL(query_pairs_err_msg_internal.read, QueryPairsErrMsgCtx, .words_letters = words, .count = count, .mode_out = &s_mode, .err_msg = &err_msg);
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_parts_name_for_the_vendors_reader(void)
{
    char name[64];
    EMBED_CALL(query_pairs_part_name_internal.part_read, QueryPairsPartNameCtx, .part = "sm_86", .name = name, .capacity = sizeof(name));
    TEST_ASSERT_EQUAL_STRING("SM86", name);
}

void test_a_parts_name_with_a_letter_after_it(void)
{
    char name[64];
    EMBED_CALL(query_pairs_part_name_internal.part_read, QueryPairsPartNameCtx, .part = "sm_120a", .name = name, .capacity = sizeof(name));
    TEST_ASSERT_EQUAL_STRING("SM120A", name);
}

void test_the_queues_target_is_the_stick(void)
{
    TEST_ASSERT_EQUAL_STRING("T/build/measuring_stick/measuring_stick.tsv", targets_of("queue", NULL, 0u));
}

void test_the_pairs_targets_are_the_bridge_and_the_machine_file(void)
{
    TEST_ASSERT_EQUAL_STRING("T/" QUERY_TABLE "/Lstar.klq T/" QUERY_TABLE "/sm_86.khw", targets_of("pair", NULL, 0u));
}

void test_targets_use_forward_slashes_on_every_platform(void)
{
    TEST_ASSERT_NULL(cellul.chr(&(CatenaFinitaCfg){.src = targets_of("pair", NULL, 0u), .cap = CLI_CELLS_POOL, .byte = '\\'}));
}

void test_the_stalls_target_is_the_machine_file(void)
{
    TEST_ASSERT_EQUAL_STRING("T/" QUERY_TABLE "/sm_86.khw", targets_of("stall", NULL, 0u));
}

void test_the_costs_targets(void)
{
    TEST_ASSERT_EQUAL_STRING("T/" QUERY_TABLE "/sm_86.kdm T/" QUERY_TABLE "/sm_86.khw", targets_of("cost", NULL, 0u));
}

void test_the_curves_targets_are_its_own_words(void)
{
    static const char *const s_curve[2] = {"1006", "1007"};
    TEST_ASSERT_EQUAL_STRING("1006 1007", targets_of("curve", s_curve, 2u));
}

void test_the_register_frame_and_fixed_modes_have_no_targets(void)
{
    TEST_ASSERT_EQUAL_STRING("", targets_of("register", NULL, 0u));
    TEST_ASSERT_EQUAL_STRING("", targets_of("frame", NULL, 0u));
    TEST_ASSERT_EQUAL_STRING("", targets_of("fixed", NULL, 0u));
}

void test_the_dry_transports_flags_come_first(void)
{
    transport_words(1);
    TEST_ASSERT_EQUAL_STRING("C --dry --full --gated gated.txt --lessons overseer.txt M L --pop C --dry --full --lessons overseer.txt M L --cross X N SM86 airlock.md gated.txt", words_joined(0u, s_out, sizeof(s_out)));
}

void test_a_live_transport_has_no_dry_flags(void)
{
    transport_words(0);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = words_joined(0u, s_out, sizeof(s_out)), .other_pointer = "C --gated gated.txt --lessons overseer.txt M L ", .size = 46u));
}

void test_a_live_transports_remaining_words_match_the_dry_ones(void)
{
    char rest[1024];
    transport_words(1);
    (void)words_joined(9u, rest, sizeof(rest));
    transport_words(0);
    TEST_ASSERT_EQUAL_STRING(rest, words_joined(7u, s_out, sizeof(s_out)));
}

void test_the_kernels_words(void)
{
    static QueryFiles s_files;
    EMBED_CALL(query_loops_root.files_get, QueryLoopsRootCfg, .root = "T", .part = "sm_86", .files = &s_files);
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    EMBED_CALL(query_pairs_part_internal.kernel_words, QueryPairsPartCtx, .words = &s_words, .part = "sm_86", .files = &s_files);
    TEST_ASSERT_EQUAL_UINT(6u, s_words.count);
    TEST_ASSERT_EQUAL_STRING("sm_86", s_words.word_text[0]);
    CLI_TEST_ASSERT_CONTAINS("/table/sm_86.khw", s_words.word_text[1]);
    CLI_TEST_ASSERT_CONTAINS("/nvidia/elf64_nvidia.tsv", s_words.word_text[2]);
    CLI_TEST_ASSERT_CONTAINS("/nvidia/mnemonic_nvidia.tsv", s_words.word_text[3]);
    TEST_ASSERT_EQUAL_STRING("kernel.bin", s_words.word_text[4]);
    TEST_ASSERT_EQUAL_STRING("kernel", s_words.word_text[5]);
}

void test_the_kernel_is_transferred_before_the_frame_the_fixed_and_the_pairs(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(query_pairs_internal.kernel_dispatch, QueryPairsCtx, .mode_text = "frame"));
    TEST_ASSERT_TRUE(EMBED_CALL(query_pairs_internal.kernel_dispatch, QueryPairsCtx, .mode_text = "fixed"));
    TEST_ASSERT_TRUE(EMBED_CALL(query_pairs_internal.kernel_dispatch, QueryPairsCtx, .mode_text = "pair"));
}

void test_the_kernel_is_not_transferred_before_the_register_mode(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(query_pairs_internal.kernel_dispatch, QueryPairsCtx, .mode_text = "register"));
}

void test_a_pairs_words_are_read(void)
{
    static const char *const s_pair[6] = {"pair", "--part", "sm_86", "--dry", "--seed", "2"};
    TEST_ASSERT_TRUE(mode_read(s_pair, 6u));
    TEST_ASSERT_EQUAL_STRING("pair", s_mode.mode);
    TEST_ASSERT_EQUAL_STRING("sm_86", s_mode.part);
    TEST_ASSERT_TRUE(s_mode.dry);
    TEST_ASSERT_EQUAL_STRING("2", s_mode.seed);
    TEST_ASSERT_EQUAL_STRING("sass", s_mode.to);
    TEST_ASSERT_EQUAL_UINT(0u, s_mode.word_count);
}

void test_a_curves_words_are_read_with_its_trace(void)
{
    static const char *const s_curve[4] = {"curve", "1006", "--trace", "build/x"};
    TEST_ASSERT_TRUE(mode_read(s_curve, 4u));
    TEST_ASSERT_EQUAL_UINT(1u, s_mode.word_count);
    TEST_ASSERT_EQUAL_STRING("1006", s_mode.words[0]);
    TEST_ASSERT_EQUAL_STRING("build/x", s_mode.trace);
}

void test_a_sticks_rulesets_are_read(void)
{
    static const char *const s_stick[3] = {"stick", "sass.krs", "c.krs"};
    TEST_ASSERT_TRUE(mode_read(s_stick, 3u));
    TEST_ASSERT_EQUAL_UINT(2u, s_mode.word_count);
    TEST_ASSERT_EQUAL_STRING("sass.krs", s_mode.words[0]);
    TEST_ASSERT_EQUAL_STRING("c.krs", s_mode.words[1]);
}

void test_a_frames_words_are_read(void)
{
    static const char *const s_frame[3] = {"frame", "--part", "sm_86"};
    TEST_ASSERT_TRUE(mode_read(s_frame, 3u));
    TEST_ASSERT_EQUAL_STRING("frame", s_mode.mode);
    TEST_ASSERT_EQUAL_UINT(0u, s_mode.word_count);
}

void test_a_dry_fixeds_words_are_read(void)
{
    static const char *const s_fixed[2] = {"fixed", "--dry"};
    TEST_ASSERT_TRUE(mode_read(s_fixed, 2u));
    TEST_ASSERT_EQUAL_STRING("fixed", s_mode.mode);
    TEST_ASSERT_TRUE(s_mode.dry);
    TEST_ASSERT_EQUAL_UINT(0u, s_mode.word_count);
}

void test_a_target_language_other_than_sass_is_refused(void)
{
    static const char *const s_ptx[3] = {"pair", "--to", "ptx"};
    TEST_ASSERT_FALSE(mode_read(s_ptx, 3u));
}

void test_an_unknown_mode_is_refused(void)
{
    static const char *const s_fly[1] = {"fly"};
    TEST_ASSERT_FALSE(mode_read(s_fly, 1u));
}

void test_a_flag_with_no_value_is_refused(void)
{
    static const char *const s_seed[2] = {"pair", "--seed"};
    TEST_ASSERT_FALSE(mode_read(s_seed, 2u));
}

void test_no_words_are_refused(void)
{
    TEST_ASSERT_FALSE(mode_read(NULL, 0u));
}

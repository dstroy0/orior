// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// marginal_paths_test.cu: the path of the marginals on the device, with the engine linked into the program. The test
// draws seeded questions of sources and options, and runs them through marginal_run and through marginal_run_host. The
// sums of the device must equal the sums of the host, and their signum must equal the signum of the device path
#include "../../../../../../../src/cu/engine/engine.h"
#include "../../../../../../../src/cu/engine/nbody/marginal/marginal.h"
#include "../../../../../../../src/cu/engine/runtime/obsignatio/obsignatio.h"
#include "../../../../../../../includes/platform/platform.h"

#include "memoria_operor/memoria_operor.h"

// the questions of the test, the most sources of a question, the most options of a source, and the targets
#define MARGINAL_TEST_QUESTIONS 48u
#define MARGINAL_TEST_SOURCES_EACH 6u
#define MARGINAL_TEST_OPTIONS_EACH 4u
#define MARGINAL_TEST_TARGETS 12u
#define MARGINAL_TEST_SOURCES (MARGINAL_TEST_QUESTIONS * MARGINAL_TEST_SOURCES_EACH)
#define MARGINAL_TEST_OPTIONS (MARGINAL_TEST_SOURCES * MARGINAL_TEST_OPTIONS_EACH)

// the pool of the test, for its questions and its sums
extern "C" const unsigned long long g_program_pool_bytes = 1ULL << 26u;

// the seeded words that the questions draw from
static unsigned int s_seed = 0x6A09E667u;

static unsigned int marginal_test_drawn(void)
{
    s_seed ^= s_seed << 13u;
    s_seed ^= s_seed >> 17u;
    s_seed ^= s_seed << 5u;
    return s_seed;
}

// the count of the checks, and one check, printed as it passes or fails
static unsigned int s_checks = 0u;

static unsigned int marginal_test_check(int held, const char *what)
{
    s_checks += 1u;
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, (held != 0) ? "  ok   " : "  FAIL ");
    mmgr_line_put(output_line, what);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return (held != 0) ? 0u : 1u;
}

// The questions, their sources and their options, and the sums of the device and of the host
typedef struct
{
    MarginalQuestion questions[MARGINAL_TEST_QUESTIONS];
    MarginalSource sources[MARGINAL_TEST_SOURCES];
    MarginalOption options[MARGINAL_TEST_OPTIONS];
    unsigned int source_count;
    unsigned int option_count;
    unsigned int question_sums[2u][MARGINAL_TEST_QUESTIONS * MARGINAL_SUMS * MARGINAL_LIMBS];
    unsigned int option_sums[2u][MARGINAL_TEST_OPTIONS * MARGINAL_LIMBS];
} MarginalTestCase;

// Each question has 1 to MARGINAL_TEST_SOURCES_EACH sources. Each source has 0 to MARGINAL_TEST_OPTIONS_EACH options
// on a target of MARGINAL_TEST_TARGETS, a weight of 1 to 1000 each, and an absent weight of 1 to 100
static void marginal_test_drawn_case(MarginalTestCase *one)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = one, .bytes = sizeof(*one), .val = 0);
    for (unsigned int question = 0u; question < MARGINAL_TEST_QUESTIONS; question++)
    {
        one->questions[question].source_first = one->source_count;
        one->questions[question].sources = 1u + (marginal_test_drawn() % MARGINAL_TEST_SOURCES_EACH);
        for (unsigned int source = 0u; source < one->questions[question].sources; source++)
        {
            MarginalSource *const placed = &one->sources[one->source_count];
            placed->option_first = one->option_count;
            placed->options = marginal_test_drawn() % (MARGINAL_TEST_OPTIONS_EACH + 1u);
            placed->absent = 1u + (marginal_test_drawn() % 100u);
            for (unsigned int option = 0u; option < placed->options; option++)
            {
                one->options[one->option_count].target = marginal_test_drawn() % MARGINAL_TEST_TARGETS;
                one->options[one->option_count].weight = 1u + (marginal_test_drawn() % 1000u);
                one->option_count += 1u;
            }
            one->source_count += 1u;
        }
    }
}

// the request of the case, its sums written to the sums of `path`
static void marginal_test_request(MarginalTestCase *one, unsigned int path, MarginalRequest *request)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = request, .bytes = sizeof(*request), .val = 0);
    request->questions = one->questions;
    request->question_count = MARGINAL_TEST_QUESTIONS;
    request->sources = one->sources;
    request->source_count = one->source_count;
    request->options = one->options;
    request->option_count = one->option_count;
    request->question_sums = one->question_sums[path];
    request->option_sums = one->option_sums[path];
}

// the first 8 bytes of the signum of `count` bytes as a word, little end first, or 0 where the signum errors
static unsigned long long marginal_test_signum(const void *bytes, unsigned long long count)
{
    unsigned char signum[OBSIGNATIO_SIGNUM_BYTES];
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    // Explicit cast reads the sums as the bytes that the signum reads
    const ObsignatioSignumRequest request = {(const unsigned char *)bytes, count, NULL, OBSIGNATIO_MODE_HASH, signum,
                                             OBSIGNATIO_SIGNUM_BYTES,      &error};
    if ((count == 0ULL) || (obsignatio_signum(&request) != 0L))
    {
        return 0ULL;
    }
    unsigned long long word = 0ULL;
    for (unsigned int at = 0u; at < 8u; at++)
    {
        word |= (unsigned long long)signum[at] << (8u * at);
    }
    return word;
}

int program_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    // the first word of the signum of the sums of the device, the answer of the device path
    static const unsigned long long SIGNUM = 0x70760423466edff5ULL;
    const unsigned int place = platform_place_g_program_owner;
    MarginalTestCase *const one = (MarginalTestCase *)platform_cell_taken(sizeof(MarginalTestCase), place, 1);
    if (one == NULL)
    {
        return 1;
    }
    marginal_test_drawn_case(one);
    MarginalRequest request;
    marginal_test_request(one, 0u, &request);
    const long device = marginal_run(&request);
    marginal_test_request(one, 1u, &request);
    const long host = marginal_run_host(&request);
    const size_t question_bytes =
        (size_t)MARGINAL_TEST_QUESTIONS * MARGINAL_SUMS * MARGINAL_LIMBS * sizeof(unsigned int);
    const size_t option_bytes = (size_t)one->option_count * MARGINAL_LIMBS * sizeof(unsigned int);
    unsigned int failed = marginal_test_check((device == (long)MARGINAL_TEST_QUESTIONS) && (host == device),
                                              "marginal_run and marginal_run_host answer each question");
    failed += marginal_test_check((EMBED_CALL(memor.cmp, MemoriaCfg, .src = one->question_sums[0],
                                              .other = one->question_sums[1], .bytes = question_bytes) == 0) &&
                                      (EMBED_CALL(memor.cmp, MemoriaCfg, .src = one->option_sums[0],
                                                  .other = one->option_sums[1], .bytes = option_bytes) == 0),
                                  "the sums of the device are the sums of the host");
    unsigned int marked = 0u;
    for (size_t word = 0u; word < (question_bytes / sizeof(unsigned int)); word++)
    {
        marked |= one->question_sums[0][word];
    }
    failed += marginal_test_check(marked != 0u, "the sums are not 0");
    const unsigned long long word =
        marginal_test_signum(one->question_sums[0],
                             sizeof(one->question_sums[0]) + sizeof(one->question_sums[1]) + sizeof(one->option_sums));
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_decimal(output_line, one->source_count, 1u);
        mmgr_line_put(output_line, " sources, ");
        mmgr_line_decimal(output_line, one->option_count, 1u);
        mmgr_line_put(output_line, " options: signum 0x");
        mmgr_line_hex(output_line, word, 16u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    failed += marginal_test_check((word != 0ULL) && (word == SIGNUM),
                                  "the sums of the device are the answer of the device path");
    platform_cell_released(one, place);
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  marginal paths test: ");
    mmgr_line_decimal(output_line, s_checks, 1u);
    mmgr_line_put(output_line, " checks, ");
    mmgr_line_decimal(output_line, failed, 1u);
    mmgr_line_put(output_line, " failed\n");
    platform_line_out_ended();
    return (failed == 0u) ? 0 : 1;
}

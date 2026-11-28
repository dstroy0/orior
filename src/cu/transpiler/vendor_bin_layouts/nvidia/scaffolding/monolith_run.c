// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// monolith_run.c: the cubin of the monolith (monolith.cu), loaded as the build made it and run on the part. This file
// compiles nothing: it passes the cubin to the driver, and queries the driver for its entries.
//
// The first entry answers every precept for each case, checked against precept_applied on the host. The program reads
// the clock of the part across it on every run. The second entry runs every costed precept as a chain of `turns`
// turns of MONOLITH_BLOCK steps, alone and then interleaved with every other. The program reads the mean of `cost
// runs` runs for each step. It checks the last word of every chain against the same chain run through precept_applied
// on the host, which confirms that every step ran.
//
//     monolith_run <cubin> [runs] [turns] [cost runs]
#include "../../../../engine/rmc/precept_value.h"
#include "../../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "../../../../engine/engine_note.h"
#include "../monolith.h"

#include <cuda.h>

// the words the first entry reads: the left word, the right word and the flag, which is 0 on every run
#define MONOLITH_IN_WORDS 3u
// the words it writes: every precept's answer, then the clock's two words
#define MONOLITH_OUT_WORDS (PRECEPT_COUNT + 2u)

// the words that every costed chain runs with. The low five bits of the right word, 25, are the count of a shift. Those
// of the left word, 15, are the count of the second chain of a pair
#define MONOLITH_COST_LEFT 0xdeadbeefu
#define MONOLITH_COST_RIGHT 0x9e3779b9u

// a pair costs about the dearer of its two alone, past this share of the cheaper, where the part runs them side by
// side. It costs about both together, past the share below it, where the part runs them one after the other
#define MONOLITH_SIDE_BY_SIDE 0.25
#define MONOLITH_ONE_AFTER 0.75

#define MONOLITH_NAMED(name_, text_, arity_) text_,

static const char *const s_names[PRECEPT_COUNT] = {PRECEPTS(MONOLITH_NAMED)};

static unsigned int s_checks = 0u;
static unsigned int s_failed = 0u;

static void check_that(int passed, const char *what, unsigned int precept, unsigned int left, unsigned int right)
{
    s_checks += 1u;
    if (passed == 0)
    {
        s_failed += 1u;
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  FAILED: ");
            mmgr_line_put(output_line, what);
            mmgr_line_put(output_line, ", precept ");
            mmgr_line_decimal(output_line, precept, 1u);
            mmgr_line_put(output_line, ", left ");
            mmgr_line_hex(output_line, left, 8u);
            mmgr_line_put(output_line, ", right ");
            mmgr_line_hex(output_line, right, 8u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
}

static unsigned char *monolith_file(const char *path)
{
    PlatformFile *const file = platform_file_open(path, "rb");
    if (file == NULL)
    {
        return NULL;
    }
    platform_file_seek(file, 0L, PLATFORM_FILE_FROM_END);
    const long size = platform_file_tell(file);
    platform_file_seek(file, 0L, PLATFORM_FILE_FROM_START);
    // a size the file reported is at least 0, and a byte past it ends the image
    unsigned char *const bytes =
        (size > 0L) ? (unsigned char *)platform_cell_taken((size_t)size + 1u, platform_place_g_program_owner, 0) : NULL;
    const int read = (bytes != NULL) && (platform_file_read_items(bytes, 1u, (size_t)size, file) == (size_t)size);
    platform_file_close(file);
    if (read == 0)
    {
        platform_cell_released(bytes, platform_place_g_program_owner);
        return NULL;
    }
    return bytes;
}

// the word that precept `precept` must answer for `left` and `right` with the flag 0: the precept_applied of the host.
// The one exception is JCC, which does not take its jump, and whose last store is the left word
static unsigned int monolith_expected(unsigned int precept, unsigned int left, unsigned int right)
{
    return (precept == PRECEPT_JCC) ? left : precept_applied((unsigned char)precept, left, right);
}

// `word` put through `steps` steps of `precept` with `right` the right word, on the host
static unsigned int monolith_chain(unsigned int precept, unsigned int word, unsigned int right,
                                   unsigned long long steps)
{
    unsigned int current = word;
    for (unsigned long long step = 0ULL; step < steps; step += 1ULL)
    {
        current = precept_applied((unsigned char)precept, current, right);
    }
    return current;
}

// a count of the words given, read as at least one
static unsigned int monolith_count_given(int count_of_words, char **words, int place, long otherwise)
{
    // Explicit cast narrows the signed word of the read to the long of a count, which a count of words fits
    const long given = (count_of_words > place)
                           ? (long)EMBED_CALL(cellul.to_long, TransfiguroCfg, .src = words[place], .end = NULL)
                           : otherwise;
    return (given > 0L) ? (unsigned int)given : 1u;
}

// every precept answered for each case and checked against the host, the clock across the program read
static int monolith_answers(CUfunction entry, CUdeviceptr in, CUdeviceptr out, unsigned int runs, const char *cubin)
{
    // small words, a shift of nothing, of one, of the width less one and of past the width, and the sign's end
    static const unsigned int cases[][2] = {
        {1u, 1u},           {0u, 0u},           {3u, 5u},          {0xdeadbeefu, 0u}, {0xdeadbeefu, 1u},
        {0xdeadbeefu, 31u}, {0xdeadbeefu, 33u}, {0x80000000u, 4u}, {0x7fffffffu, 1u}, {0xffffffffu, 0x12345678u},
    };
    const unsigned int case_count = (unsigned int)(sizeof(cases) / sizeof(cases[0]));
    unsigned long long least = ~0ULL;
    unsigned long long max = 0ULL;
    for (unsigned int at = 0u; at < case_count; at += 1u)
    {
        const unsigned int given[MONOLITH_IN_WORDS] = {cases[at][0], cases[at][1], 0u};
        unsigned int answered[MONOLITH_OUT_WORDS] = {0u};
        for (unsigned int run = 0u; run < runs; run += 1u)
        {
            void *arguments[2] = {&in, &out};
            const int ran =
                (cuMemcpyHtoD(in, given, sizeof(given)) == CUDA_SUCCESS) &&
                (cuLaunchKernel(entry, 1u, 1u, 1u, 1u, 1u, 1u, 0u, NULL, arguments, NULL) == CUDA_SUCCESS) &&
                (cuCtxSynchronize() == CUDA_SUCCESS) && (cuMemcpyDtoH(answered, out, sizeof(answered)) == CUDA_SUCCESS);
            if (ran == 0)
            {
                {
                    EngineNote engine_note;
                    engine_note_begun(&engine_note);
                    mmgr_line_put(&engine_note.line, "  monolith: a run of the answers did not complete on the part");
                    mmgr_line_ch(&engine_note.line, '\n');
                    engine_note_written(&engine_note);
                }
                return 0;
            }
            const unsigned long long spent =
                (unsigned long long)answered[PRECEPT_COUNT] | ((unsigned long long)answered[PRECEPT_COUNT + 1u] << 32u);
            least = (spent < least) ? spent : least;
            max = (spent > max) ? spent : max;
        }
        for (unsigned int precept = 0u; precept < PRECEPT_COUNT; precept += 1u)
        {
            check_that(answered[precept] == monolith_expected(precept, given[0], given[1]),
                       "the part's answer is the host's", precept, given[0], given[1]);
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  monolith ");
        mmgr_line_put(output_line, cubin);
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal(output_line, case_count, 1u);
        mmgr_line_put(output_line, " cases, ");
        mmgr_line_decimal(output_line, runs, 1u);
        mmgr_line_put(output_line, " runs a case, the program ");
        mmgr_line_decimal(output_line, least, 1u);
        mmgr_line_put(output_line, " cycles at least, ");
        mmgr_line_decimal(output_line, max, 1u);
        mmgr_line_put(output_line, " at max");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 1;
}

// every costed chain run alone and interleaved, `cost_runs` times, each its mean a step, and every last word checked
// against the host's
static int monolith_costs(CUfunction entry, unsigned int turns, unsigned int cost_runs)
{
    CUdeviceptr cycles = 0u;
    CUdeviceptr sink = 0u;
    if ((cuMemAlloc(&cycles, MONOLITH_SECTIONS * sizeof(unsigned long long)) != CUDA_SUCCESS) ||
        (cuMemAlloc(&sink, MONOLITH_SECTIONS * sizeof(unsigned int)) != CUDA_SUCCESS))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  monolith: the costs' words were not allocated on the part");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 0;
    }
    // each section's counts over the runs: their sum, the least and the max
    static double s_spent[MONOLITH_SECTIONS];
    static unsigned long long s_least[MONOLITH_SECTIONS];
    static unsigned long long s_max[MONOLITH_SECTIONS];
    for (unsigned int section = 0u; section < MONOLITH_SECTIONS; section += 1u)
    {
        s_least[section] = ~0ULL;
    }
    unsigned int last[MONOLITH_SECTIONS] = {0u};
    unsigned int left = MONOLITH_COST_LEFT;
    unsigned int right = MONOLITH_COST_RIGHT;
    unsigned int flag = 1u;
    for (unsigned int run = 0u; run < cost_runs; run += 1u)
    {
        unsigned long long counted[MONOLITH_SECTIONS] = {0ULL};
        void *arguments[6] = {&left, &right, &turns, &flag, &cycles, &sink};
        const int ran = (cuLaunchKernel(entry, 1u, 1u, 1u, 1u, 1u, 1u, 0u, NULL, arguments, NULL) == CUDA_SUCCESS) &&
                        (cuCtxSynchronize() == CUDA_SUCCESS) &&
                        (cuMemcpyDtoH(counted, cycles, sizeof(counted)) == CUDA_SUCCESS) &&
                        (cuMemcpyDtoH(last, sink, sizeof(last)) == CUDA_SUCCESS);
        if (ran == 0)
        {
            {
                EngineNote engine_note;
                engine_note_begun(&engine_note);
                mmgr_line_put(&engine_note.line, "  monolith: a run of the costs did not complete on the part");
                mmgr_line_ch(&engine_note.line, '\n');
                engine_note_written(&engine_note);
            }
            return 0;
        }
        for (unsigned int section = 0u; section < MONOLITH_SECTIONS; section += 1u)
        {
            s_spent[section] += (double)counted[section];
            s_least[section] = (counted[section] < s_least[section]) ? counted[section] : s_least[section];
            s_max[section] = (counted[section] > s_max[section]) ? counted[section] : s_max[section];
        }
    }
    cuMemFree(cycles);
    cuMemFree(sink);

    // every chain run on the host. First each costed precept from the left word on the right word, which a single and
    // the first chain of a pair run. Then from the right word on the left word, which the second chain of a pair runs
    const unsigned long long steps = (unsigned long long)turns * (unsigned long long)MONOLITH_BLOCK;
    unsigned int forward[MONOLITH_COSTED_COUNT];
    unsigned int backward[MONOLITH_COSTED_COUNT];
    for (unsigned int place = 0u; place < MONOLITH_COSTED_COUNT; place += 1u)
    {
        forward[place] = monolith_chain(MONOLITH_COSTED(place), left, right, steps);
        backward[place] = monolith_chain(MONOLITH_COSTED(place), right, left, steps);
        check_that(last[place] == forward[place], "a chain run alone ends on the host's word", MONOLITH_COSTED(place),
                   left, right);
    }
    for (unsigned int first = 0u; first < MONOLITH_COSTED_COUNT; first += 1u)
    {
        for (unsigned int second = first; second < MONOLITH_COSTED_COUNT; second += 1u)
        {
            check_that(last[MONOLITH_PAIR_SECTION(first, second)] == (forward[first] ^ backward[second]),
                       "two chains interleaved end on the host's words", MONOLITH_COSTED(first), left, right);
        }
    }

    // every section read for each step, past the turns of NOP read the same way. That is its mean, its least m, and its
    // spread s, the max less the least, over the runs (P6 in query_protocol_table.md)
    const double chain = (double)steps;
    const double bare_mean = (s_spent[0] / (double)cost_runs) / chain;
    const double bare_least = (double)s_least[0] / chain;
    static double s_mean[MONOLITH_SECTIONS];
    static double s_m[MONOLITH_SECTIONS];
    static double s_s[MONOLITH_SECTIONS];
    for (unsigned int section = 0u; section < MONOLITH_SECTIONS; section += 1u)
    {
        s_mean[section] = ((s_spent[section] / (double)cost_runs) / chain) - bare_mean;
        s_m[section] = ((double)s_least[section] / chain) - bare_least;
        s_s[section] = (double)(s_max[section] - s_least[section]) / chain;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  monolith costs: ");
        mmgr_line_decimal(output_line, turns, 1u);
        mmgr_line_put(output_line, " turns of ");
        mmgr_line_decimal(output_line, MONOLITH_BLOCK, 1u);
        mmgr_line_put(output_line, " steps a section, ");
        mmgr_line_decimal(output_line, steps, 1u);
        mmgr_line_put(output_line, " steps a chain, ");
        mmgr_line_decimal(output_line, cost_runs, 1u);
        mmgr_line_put(output_line, " runs");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  NOP's turns cost ");
        mmgr_line_fixed(output_line, (double)(bare_mean), 4u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " cycles a step with nothing in them, and every figure below is past that");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  alone, cycles a step     mean   least m  spread s");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    double spread_alone = 0.0;
    for (unsigned int place = 0u; place < MONOLITH_COSTED_COUNT; place += 1u)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    ");
            mmgr_line_put_columns(output_line, s_names[MONOLITH_COSTED(place)], 5u);
            mmgr_line_put(output_line, "           ");
            mmgr_line_fixed(output_line, (double)(s_mean[place]), 4u, 8u, EMBED_FALSE);
            mmgr_line_put(output_line, "  ");
            mmgr_line_fixed(output_line, (double)(s_m[place]), 4u, 8u, EMBED_FALSE);
            mmgr_line_put(output_line, "  ");
            mmgr_line_fixed(output_line, (double)(s_s[place]), 4u, 8u, EMBED_FALSE);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        spread_alone = (s_s[place] > spread_alone) ? s_s[place] : spread_alone;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  s alone, the largest spread of the singles: ");
        mmgr_line_fixed(output_line, (double)(spread_alone), 4u, 0u, EMBED_FALSE);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    // The singles ranked by their least, each delta to the next read as a state of gnascor's branch pair. Inside s the
    // two are one cost, a slice inside the floor, dual or gray (which of the two is gnascor's Open 4). Past s the row's
    // precept is the cheaper side and is valid: lead
    unsigned int order[MONOLITH_COSTED_COUNT];
    for (unsigned int place = 0u; place < MONOLITH_COSTED_COUNT; place += 1u)
    {
        unsigned int at = place;
        while ((at > 0u) && (s_m[order[at - 1u]] > s_m[place]))
        {
            order[at] = order[at - 1u];
            at -= 1u;
        }
        order[at] = place;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(
            output_line,
            "  alone, ranked by least m, each delta to the next read against s: dual/gray inside it, lead past it");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    for (unsigned int rank = 0u; rank < MONOLITH_COSTED_COUNT; rank += 1u)
    {
        const unsigned int place = order[rank];
        if ((rank + 1u) == MONOLITH_COSTED_COUNT)
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "    ");
                mmgr_line_put_columns(output_line, s_names[MONOLITH_COSTED(place)], 5u);
                mmgr_line_put(output_line, " ");
                mmgr_line_fixed(output_line, (double)(s_m[place]), 4u, 8u, EMBED_FALSE);
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            continue;
        }
        const unsigned int next = order[rank + 1u];
        const double delta = s_m[next] - s_m[place];
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    ");
            mmgr_line_put_columns(output_line, s_names[MONOLITH_COSTED(place)], 5u);
            mmgr_line_put(output_line, " ");
            mmgr_line_fixed(output_line, (double)(s_m[place]), 4u, 8u, EMBED_FALSE);
            mmgr_line_put(output_line, "  +");
            mmgr_line_fixed(output_line, (double)(delta), 4u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, " to ");
            mmgr_line_put_columns(output_line, s_names[MONOLITH_COSTED(next)], 5u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put(output_line, (delta > spread_alone) ? "lead" : "dual/gray");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }

    // the pairs by their least, against the singles' least
    const double *const alone = s_m;
    double spread_pairs = 0.0;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line,
                      "  interleaved, least m a step of each, the row's precept with the column's:\n       ");
        platform_line_out_ended();
    }
    for (unsigned int second = 0u; second < MONOLITH_COSTED_COUNT; second += 1u)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, s_names[MONOLITH_COSTED(second)], 6u);
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    unsigned int side_by_side = 0u;
    unsigned int one_after = 0u;
    unsigned int between = 0u;
    for (unsigned int first = 0u; first < MONOLITH_COSTED_COUNT; first += 1u)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    ");
            mmgr_line_put_columns(output_line, s_names[MONOLITH_COSTED(first)], 5u);
            platform_line_out_ended();
        }
        for (unsigned int second = 0u; second < MONOLITH_COSTED_COUNT; second += 1u)
        {
            if (second < first)
            {
                {
                    VerbaLinea *const output_line = platform_line_out();
                    mmgr_line_put(output_line, " ");
                    mmgr_line_put_right(output_line, "", 6u);
                    platform_line_out_ended();
                }
                continue;
            }
            const unsigned int section = MONOLITH_PAIR_SECTION(first, second);
            const double paired = s_m[section];
            spread_pairs = (s_s[section] > spread_pairs) ? s_s[section] : spread_pairs;
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, " ");
                mmgr_line_fixed(output_line, (double)(paired), 2u, 6u, EMBED_FALSE);
                platform_line_out_ended();
            }
            const double dearer = (alone[first] > alone[second]) ? alone[first] : alone[second];
            const double cheaper = (alone[first] > alone[second]) ? alone[second] : alone[first];
            // the program reads a pair only where both its precepts cost one cycle or more for each step alone
            if (cheaper >= 1.0)
            {
                const double share = (paired - dearer) / cheaper;
                side_by_side += (share < MONOLITH_SIDE_BY_SIDE) ? 1u : 0u;
                one_after += (share > MONOLITH_ONE_AFTER) ? 1u : 0u;
                between += ((share >= MONOLITH_SIDE_BY_SIDE) && (share <= MONOLITH_ONE_AFTER)) ? 1u : 0u;
            }
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  of the pairs whose precepts each cost a cycle a step or more: ");
        mmgr_line_decimal(output_line, side_by_side, 1u);
        mmgr_line_put(output_line, " run side by side, ");
        mmgr_line_decimal(output_line, one_after, 1u);
        mmgr_line_put(output_line, " one after the other, ");
        mmgr_line_decimal(output_line, between, 1u);
        mmgr_line_put(output_line, " between");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  s interleaved, the largest spread of the pairs: ");
        mmgr_line_fixed(output_line, (double)(spread_pairs), 4u, 0u, EMBED_FALSE);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 1;
}

int program_main(int count_of_words, char **words)
{
    if ((count_of_words < 2) || (count_of_words > 5))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "monolith_run <cubin> [runs] [turns] [cost runs]");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    const unsigned int runs = monolith_count_given(count_of_words, words, 2, 64L);
    const unsigned int turns = monolith_count_given(count_of_words, words, 3, 10000L);
    const unsigned int cost_runs = monolith_count_given(count_of_words, words, 4, 8L);
    unsigned char *const image = monolith_file(words[1]);
    CUdevice device = 0;
    CUcontext context = NULL;
    CUmodule module = NULL;
    CUfunction answers = NULL;
    CUfunction costs = NULL;
    CUdeviceptr in = 0u;
    CUdeviceptr out = 0u;
    const int opened = (image != NULL) && (cuInit(0u) == CUDA_SUCCESS) && (cuDeviceGet(&device, 0) == CUDA_SUCCESS) &&
                       (cuDevicePrimaryCtxRetain(&context, device) == CUDA_SUCCESS) &&
                       (cuCtxSetCurrent(context) == CUDA_SUCCESS) &&
                       (cuModuleLoadData(&module, image) == CUDA_SUCCESS) &&
                       (cuModuleGetFunction(&answers, module, "monolith") == CUDA_SUCCESS) &&
                       (cuModuleGetFunction(&costs, module, "monolith_cost") == CUDA_SUCCESS) &&
                       (cuMemAlloc(&in, MONOLITH_IN_WORDS * sizeof(unsigned int)) == CUDA_SUCCESS) &&
                       (cuMemAlloc(&out, MONOLITH_OUT_WORDS * sizeof(unsigned int)) == CUDA_SUCCESS);
    if (opened == 0)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  monolith: the cubin ");
            mmgr_line_put(&engine_note.line, words[1]);
            mmgr_line_put(&engine_note.line, " was not loaded on the part");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 1;
    }
    const int answered = monolith_answers(answers, in, out, runs, words[1]);
    const int costed = (answered != 0) && monolith_costs(costs, turns, cost_runs);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  monolith: ");
        mmgr_line_decimal(output_line, s_checks, 1u);
        mmgr_line_put(output_line, " checks, ");
        mmgr_line_decimal(output_line, s_failed, 1u);
        mmgr_line_put(output_line, " failed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    cuMemFree(in);
    cuMemFree(out);
    cuModuleUnload(module);
    cuDevicePrimaryCtxRelease(device);
    platform_cell_released(image, platform_place_g_program_owner);
    return ((costed != 0) && (s_failed == 0u)) ? 0 : 1;
}

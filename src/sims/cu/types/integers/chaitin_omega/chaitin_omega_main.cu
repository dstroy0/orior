// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// chaitin_omega_main.cu: the terms printed and main
#include "../../../../../platform/platform.h"
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "chaitin_omega_internal.h"
#include "memoria_operor/memoria_operor.h"

// The pool. Each of the host's workers holds the tables of its rewrite. They are at the room of the run's token budget,
// or at OMEGA_TERM_MAX where the run has none
extern "C" const unsigned long long g_program_pool_bytes = 1ULL << 31u;

// a term from its code into `term`, for the checks below
static void omega_term(const char *code, OmegaTokens *term)
{
    size_t at = 0u;
    while (code[at] != '\0')
    {
        if (code[at] == '0')
        {
            omega_put(term, (code[at + 1u] == '0') ? OMEGA_LAMBDA : OMEGA_APPLY);
            at += 2u;
        }
        else
        {
            const size_t ones = EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &code[at],
                                           .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "1");
            at += ones;
            // a run of ones within a code of the checks, far below 2^31. An int stores its count
            omega_put(term, (int)ones);
            at += 1u;
        }
    }
}

// the fate of the term of the code, its rewrite's tables on the temporary tier at the room of the budget
static OmegaFate omega_fate_of(const char *code, unsigned int steps, unsigned int tokens)
{
    OmegaRewrite rewrite;
    omega_rewrite_taken(&rewrite, omega_rewrite_room(tokens), 1);
    omega_term(code, &rewrite.term);
    unsigned int taken = 0u;
    const OmegaFate fate = omega_run(&rewrite, steps, tokens, &taken);
    omega_rewrite_given(&rewrite);
    return fate;
}

// What one worker of the check of the engine's terms reads and writes: its range of the terms and its tables. Also
// whether every term of it agreed
typedef struct
{
    const OmegaCounts *counts;
    const OmegaCrossed *crossed;
    size_t first;
    size_t last;
    OmegaRewrite rewrite;
    int same;
} OmegaCrossWorker;

static void omega_cross_worker(void *context)
{
    OmegaCrossWorker *const state = (OmegaCrossWorker *)context;
    state->same = 1;
    for (size_t at = state->first; at < state->last; at += 1u)
    {
        const OmegaSettled *const one = &state->crossed->settled[at];
        if (one->fate == OMEGA_GREW)
        {
            continue;
        }
        state->rewrite.term.count = 0u;
        omega_unrank(state->counts, one->length, 0u, one->index, &state->rewrite.term);
        unsigned int taken = 0u;
        const OmegaFate fate = omega_run(&state->rewrite, (unsigned int)one->steps + 1u, 0xFFFFFFFFu, &taken);
        state->same =
            state->same && ((int)fate == one->fate) &&
            ((fate != OMEGA_HALTS) || ((taken == one->steps) && (omega_code_bits(&state->rewrite.term) == one->bits)));
    }
}

// Every term the engine settled, run again by omega_run: 1 where each agrees. Fewer than 4096 terms run on this
// thread, and the host's workers share more. The workers' states are one cell of the temporary tier, which the call
// gives back after its wait
static int omega_cross(const OmegaCounts *counts, const OmegaCrossed *crossed, unsigned int workers)
{
    const unsigned int used = (crossed->count < 4096u) ? 1u : workers;
    OmegaCrossWorker *const states = (OmegaCrossWorker *)omega_taken(used, sizeof(OmegaCrossWorker), 1);
    static PlatformWorker s_threads[OMEGA_WORKERS_MAX];
    const size_t room = omega_rewrite_room(0xFFFFFFFFu);
    for (unsigned int worker = 0u; worker < used; worker += 1u)
    {
        states[worker].counts = counts;
        states[worker].crossed = crossed;
        states[worker].first = (crossed->count * worker) / used;
        states[worker].last = (crossed->count * (worker + 1u)) / used;
        omega_rewrite_taken(&states[worker].rewrite, room, 1);
    }
    if (used == 1u)
    {
        omega_cross_worker(&states[0]);
    }
    else
    {
        for (unsigned int worker = 0u; worker < used; worker += 1u)
        {
            if (platform_worker_started(&s_threads[worker], omega_cross_worker, &states[worker]) == 0)
            {
                omega_refused("the system refused a worker");
            }
        }
        for (unsigned int worker = 0u; worker < used; worker += 1u)
        {
            platform_worker_joined(&s_threads[worker]);
        }
    }
    int every = 1;
    for (unsigned int worker = used; worker > 0u; worker -= 1u)
    {
        every = every && states[worker - 1u].same;
        omega_rewrite_given(&states[worker - 1u].rewrite);
    }
    omega_given(states);
    return every;
}

// the bits of a mass scaled by 2^64
static void omega_bits(VerbaLinea *line, const OriorExactInteger *value, unsigned int bits)
{
    mmgr_line_put(line, "0.");
    for (unsigned int bit = 0u; bit < bits; bit += 1u)
    {
        const unsigned int place = 63u - bit;
        mmgr_line_ch(line, (((value->limb[place / 32u] >> (place % 32u)) & 1u) != 0u) ? '1' : '0');
    }
}

// the mass sum of count[n] 2^-n scaled by 2^64, exactly
static void omega_dyadic(OriorExactInteger *value, const unsigned long long *count, unsigned int length)
{
    orior_exact_zero(value);
    for (unsigned int bits = 2u; bits <= length; bits += 1u)
    {
        OriorExactInteger part;
        OriorExactInteger scale;
        sim_exact_unsigned(&part, count[bits]);
        sim_exact_unsigned(&scale, 1ULL << (64u - bits));
        OriorExactInteger product;
        (void)sim_exact_product(&part, &scale, &product);
        (void)sim_exact_sum(value, &product, value);
    }
}

int program_main(int argc, char **argv)
{
    platform_line_out_every_write();
    static char line_buffer[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, line_buffer);
    static OmegaCounts counts;
    // L alone runs on the engine, which nothing bounds. L, steps and tokens run the budgeted kernel on the device. With
    // "cpu" after them they run on the host, kept as the cross-check. "--ledger <path>" plans either run as jobs.
    counts.length = (argc > 1) ? (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = argv[1],
                                                          .end = NULL, .base = 10)
                               : OMEGA_LENGTH_DEFAULT;
    // Explicit cast reads the byte as its code point
    const int budgeted =
        (argc > 2) && EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_NUM, .byte = (uint8_t)argv[2][0]);
    counts.steps =
        budgeted ? (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = argv[2], .end = NULL, .base = 10)
                 : OMEGA_STEPS_DEFAULT;
    counts.tokens = (budgeted && (argc > 3)) ? (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg,
                                                                        .src = argv[3], .end = NULL, .base = 10)
                                             : OMEGA_TOKENS_DEFAULT;
    const int on_engine = !budgeted;
    int on_device = budgeted;
    const char *ledger = NULL;
    for (int at = 2; at < argc; at += 1)
    {
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = argv[at], .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "cpu",
                       .ci = EMBED_FALSE))
        {
            on_device = 0;
        }
        else if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = argv[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "--ledger", .ci = EMBED_FALSE)) &&
                 ((at + 1) < argc))
        {
            at += 1;
            ledger = argv[at];
        }
    }
    if ((counts.length < 4u) || (counts.length > OMEGA_LENGTH_MAX))
    {
        mmgr_line_put(&results.line, "  the length must be from 4 to 60 bits\n");
        sim_flush(&results);
        return 2;
    }
    if ((on_device != 0) && ((counts.tokens < 64u) || (counts.tokens > 16383u)))
    {
        // the device stores a term, its indices and its spine ends in 16 bits, in buffers twice the token budget
        mmgr_line_put(&results.line, "  on the device the token budget must be from 64 to 16383\n");
        sim_flush(&results);
        return 2;
    }
    omega_count_all(&counts);

    // the sim checks the enumeration against an independent parse of every code
    int counted = 1;
    const unsigned int parsed_max = (counts.length < OMEGA_PARSED_MAX) ? counts.length : OMEGA_PARSED_MAX;
    for (unsigned int length = 2u; length <= parsed_max; length += 1u)
    {
        unsigned long long closed = 0ULL;
        for (unsigned long long code = 0ULL; code < (1ULL << length); code += 1ULL)
        {
            unsigned int at = 0u;
            closed += ((omega_parse(code, length, &at, 0u) != 0) && (at == length)) ? 1ULL : 0ULL;
        }
        counted = counted && (closed == omega_count(&counts, length, 0u));
    }
    sim_check(&results, counted, "the closed term counts equal a parse of every code through 22 bits");
    int ranked = 1;
    // a term of a program, at max half as many tokens as OMEGA_LENGTH_MAX bits
    int term_tokens[OMEGA_LENGTH_MAX];
    OmegaTokens term = {term_tokens, 0u, OMEGA_LENGTH_MAX};
    for (unsigned int length = 2u; (length <= 16u) && (length <= counts.length); length += 1u)
    {
        for (unsigned long long index = 0ULL; index < omega_count(&counts, length, 0u); index += 1ULL)
        {
            term.count = 0u;
            omega_unrank(&counts, length, 0u, index, &term);
            unsigned long long bits = 0ULL;
            for (size_t at = 0u; at < term.count; at += 1u)
            {
                bits += (term.token[at] <= 0) ? 2ULL : ((unsigned long long)term.token[at] + 1ULL);
            }
            ranked = ranked && (bits == length) && (omega_end(term.token, 0u) == term.count);
        }
    }
    sim_check(&results, ranked, "every unranked term through 16 bits is one term of its length");
    // I, I I, omega omega, K I (omega omega), and (lambda x. x x x)(lambda x. x x x)
    sim_check(&results, omega_fate_of("0010", 64u, 256u) == OMEGA_HALTS, "the identity is a normal form");
    sim_check(&results, omega_fate_of("0100100010", 64u, 256u) == OMEGA_HALTS, "I I reduces to I");
    sim_check(&results, omega_fate_of("010001101000011010", 64u, 256u) == OMEGA_LOOPS,
              "omega omega is watched returning to itself");
    sim_check(&results, omega_fate_of("010100001100010010001101000011010", 64u, 256u) == OMEGA_HALTS,
              "K I (omega omega) halts, normal order dropping the loop");
    sim_check(&results, omega_fate_of("01000101101010000101101010", 1000000u, 4096u) == OMEGA_DIVERGES,
              "(x x x)(x x x) is proven to grow forever: it returns at its own head");

    // a run on the device is one tessera job. The kernel's run declares the count table it copies there, and the
    // engine's run its first round's pool. The daemon measures the buffers each grows to and records that peak under
    // the run's arguments
    const unsigned long long declared = (on_engine != 0) ? omega_engine_declared(&counts) : sizeof(counts.count);
    if (((on_engine != 0) || (on_device != 0)) && !sim_job_submit(&results, "chaitin_omega", argc, argv, declared))
    {
        return sim_close(&results, "chaitin_omega");
    }

    // every closed term of at max L bits, run on the device unless the request names the host
    const unsigned int workers = omega_workers();
    static OmegaResults fates;
    unsigned int device_threads = 0u;
    unsigned long long host_runs = 0ULL;
    unsigned long long jobs_run = 0ULL;
    unsigned long long jobs_retained = 0ULL;
    const unsigned long long start = platform_clock_ns();
    OmegaEngineReport engine_report;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &engine_report, .bytes = sizeof(engine_report), .val = 0);
    // the terms that the engine settles through OMEGA_ENGINE_CROSS_MAX bits, at most every closed term to that length
    OmegaCrossed crossed = {NULL, 0u, 0u, 0ULL};
    if (on_engine != 0)
    {
        for (unsigned int length = 2u; (length <= OMEGA_ENGINE_CROSS_MAX) && (length <= counts.length); length += 1u)
        {
            crossed.room += (size_t)omega_count(&counts, length, 0u);
        }
        crossed.settled = (OmegaSettled *)omega_taken(crossed.room, sizeof(OmegaSettled), 0);
    }
    if (on_engine != 0)
    {
        if (omega_engine(&results, &counts, ledger, &engine_report, &crossed, &fates) == 0)
        {
            return sim_close(&results, "chaitin_omega");
        }
    }
    else if (on_device != 0)
    {
        if (omega_device(&results, &counts, workers, ledger, &device_threads, &host_runs, &jobs_run, &jobs_retained,
                         &fates) == 0)
        {
            return sim_close(&results, "chaitin_omega");
        }
    }
    else
    {
        omega_host(&counts, counts.length, workers, &fates);
    }
    // the nanoseconds since the start, as seconds in a double
    const double seconds = (double)(platform_clock_ns() - start) / 1000000000.0;
    if (on_device != 0)
    {
        // the host's run of the same terms under the same budgets must give every fate and busy beaver alike
        const unsigned int cross = (counts.length < OMEGA_CROSS_MAX) ? counts.length : OMEGA_CROSS_MAX;
        static OmegaResults host;
        omega_host(&counts, cross, workers, &host);
        int alike = (host.contradictions == 0ULL);
        for (unsigned int length = 2u; length <= cross; length += 1u)
        {
            for (unsigned int fate = 0u; fate < 6u; fate += 1u)
            {
                alike = alike && (host.fate[fate][length] == fates.fate[fate][length]);
            }
            alike = alike && (host.max_steps[length] == fates.max_steps[length]) &&
                    (host.max_bits[length] == fates.max_bits[length]) &&
                    ((host.fate[OMEGA_HALTS][length] == 0ULL) ||
                     ((host.steps_champion[length] == fates.steps_champion[length]) &&
                      (host.bits_champion[length] == fates.bits_champion[length])));
        }
        sim_check(&results, alike,
                  "the device's fates and busy beavers, champions included, equal the host's through 30 bits");
    }
    if (on_engine != 0)
    {
        // Every term the engine settled through 24 bits, run again by omega_run with no token budget. The steps reach
        // the step that the engine settled it at. It must have the same fate, and for a halt the same step and the
        // same normal form. A loop shows its fate alone. The check does not run a term that grew again, whether by
        // the growth proof or as a parked term.
        //
        // Where the run takes on a term through 24 bits, at least one must settle. A run that takes each such job from
        // its ledger has no term to check
        const int every = omega_cross(&counts, &crossed, workers);
        sim_check(
            &results, (every != 0) && ((crossed.count != 0u) || (crossed.owed == 0ULL)),
            "every term the engine settled through 24 bits, those that grew aside: omega_run gives the same fate, "
            "and for a halt the same step and the same normal form");
    }
    omega_given(crossed.settled);

    // the mass past L, counted
    static OmegaMass s_all_down;
    static OmegaMass s_all_up;
    static OmegaMass s_closed_up;
    static OmegaMass s_normal_down;
    const unsigned long long *const all_down = s_all_down.value;
    const unsigned long long *const closed_up = s_closed_up.value;
    const unsigned long long *const normal_down = s_normal_down.value;
    omega_all_mass(&s_all_down, 0);
    omega_all_mass(&s_all_up, 1);
    omega_closed_mass(&s_all_up, &s_closed_up);
    unsigned long long parsed = 0ULL;
    for (unsigned int length = 2u; length <= OMEGA_COUNTED; length += 1u)
    {
        parsed += all_down[length];
    }
    unsigned long long remaining = OMEGA_ONE - parsed;
    for (unsigned int length = counts.length + 1u; length <= OMEGA_COUNTED; length += 1u)
    {
        remaining += closed_up[length];
    }
    sim_check(&results, parsed <= OMEGA_ONE, "the counted parse mass stays below 1");
    omega_normal_mass(&s_normal_down);
    unsigned long long normal_end = 0ULL;
    int normal_within = 1;
    for (unsigned int length = 2u; length <= OMEGA_COUNTED; length += 1u)
    {
        normal_within = normal_within && (normal_down[length] <= closed_up[length]);
        normal_end += (length > counts.length) ? normal_down[length] : 0ULL;
    }
    sim_check(&results, normal_within, "the normal form mass is within the closed mass at every length");

    OriorExactInteger lower;
    OriorExactInteger open;
    OriorExactInteger upper;
    OriorExactInteger tail;
    OriorExactInteger four;
    OriorExactInteger programs;
    // a halt is a normal form reached or a type found
    unsigned long long halted[OMEGA_LENGTH_MAX + 1u] = {0ULL};
    unsigned long long unsettled[OMEGA_LENGTH_MAX + 1u] = {0ULL};
    for (unsigned int length = 0u; length <= counts.length; length += 1u)
    {
        halted[length] = fates.fate[OMEGA_HALTS][length] + fates.fate[OMEGA_TYPED][length];
        unsettled[length] = fates.fate[OMEGA_OPEN][length] + fates.fate[OMEGA_GREW][length];
    }
    omega_dyadic(&lower, halted, counts.length);
    sim_check(&results, fates.contradictions == 0ULL,
              "no term proven to loop or to grow forever has a simple type (every typable term halts)");
    omega_dyadic(&open, unsettled, counts.length);
    unsigned long long closed_counts[OMEGA_LENGTH_MAX + 1u] = {0ULL};
    for (unsigned int length = 2u; length <= counts.length; length += 1u)
    {
        closed_counts[length] = omega_count(&counts, length, 0u);
    }
    omega_dyadic(&programs, closed_counts, counts.length);
    // the fixed point's 2^-62 scaled to 2^-64
    sim_exact_unsigned(&tail, remaining);
    sim_exact_unsigned(&four, 4ULL);
    (void)sim_exact_product(&tail, &four, &tail);
    (void)sim_exact_sum(&lower, &open, &upper);
    (void)sim_exact_sum(&upper, &tail, &upper);
    // the normal forms past L halt unrun
    OriorExactInteger normal;
    sim_exact_unsigned(&normal, normal_end);
    (void)sim_exact_product(&normal, &four, &normal);
    (void)sim_exact_sum(&lower, &normal, &lower);
    sim_check(&results, (lower.sign > 0) && (orior_exact_compare(&lower, &upper) < 0),
              "the bracket is proper: 0 < lower < upper");

    unsigned int shared = 0u;
    while (shared < 64u)
    {
        const unsigned int place = 63u - shared;
        const unsigned int low_bit = (lower.limb[place / 32u] >> (place % 32u)) & 1u;
        const unsigned int high_bit = (upper.limb[place / 32u] >> (place % 32u)) & 1u;
        if (low_bit != high_bit)
        {
            break;
        }
        shared += 1u;
    }

    unsigned long long totals[6] = {0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL};
    for (unsigned int fate = 0u; fate < 6u; fate += 1u)
    {
        for (unsigned int length = 0u; length <= counts.length; length += 1u)
        {
            totals[fate] += fates.fate[fate][length];
        }
    }
    mmgr_line_put(&results.line, "  Chaitin's Omega for the binary lambda calculus, every closed term through ");
    mmgr_line_decimal(&results.line, counts.length, 1u);
    mmgr_line_put(&results.line, " bits (");
    if (on_engine != 0)
    {
        mmgr_line_put(&results.line, "no step or token budget, ");
    }
    else
    {
        mmgr_line_decimal(&results.line, counts.steps, 1u);
        mmgr_line_put(&results.line, " steps, ");
        mmgr_line_decimal(&results.line, counts.tokens, 1u);
        mmgr_line_put(&results.line, " tokens, ");
    }
    if (on_device != 0)
    {
        mmgr_line_decimal(&results.line, device_threads, 1u);
        mmgr_line_put(&results.line, " device threads, ");
        mmgr_line_decimal(&results.line, host_runs, 1u);
        mmgr_line_put(&results.line, " runs past the device's budgets ran on the host, ");
        mmgr_line_decimal(&results.line, jobs_run, 1u);
        mmgr_line_put(&results.line, " jobs run, ");
        mmgr_line_decimal(&results.line, jobs_retained, 1u);
        mmgr_line_put(&results.line, " from the ledger, ");
    }
    else if (on_engine != 0)
    {
        mmgr_line_put(&results.line, "on the device's record machine, ");
        mmgr_line_decimal(&results.line, engine_report.rounds, 1u);
        mmgr_line_put(&results.line, " rounds, ");
        mmgr_line_decimal(&results.line, engine_report.records, 1u);
        mmgr_line_put(&results.line, " records swept, ");
        mmgr_line_decimal(&results.line, engine_report.parked, 1u);
        mmgr_line_put(&results.line, " parked as outgrown, ");
    }
    else
    {
        mmgr_line_decimal(&results.line, workers, 1u);
        mmgr_line_put(&results.line, " host threads, ");
    }
    mmgr_line_decimal(&results.line, (unsigned long long)(seconds * 1000.0), 1u);
    mmgr_line_put(&results.line,
                  " ms)\n  length    closed      halts  typed halts     loops  grows forever  out of steps"
                  "  outgrew\n");
    for (unsigned int length = 2u; length <= counts.length; length += 1u)
    {
        mmgr_line_decimal_columns(&results.line, length, 8u);
        mmgr_line_decimal_columns(&results.line, omega_count(&counts, length, 0u), 10u);
        mmgr_line_decimal_columns(&results.line, fates.fate[OMEGA_HALTS][length], 11u);
        mmgr_line_decimal_columns(&results.line, fates.fate[OMEGA_TYPED][length], 13u);
        mmgr_line_decimal_columns(&results.line, fates.fate[OMEGA_LOOPS][length], 10u);
        mmgr_line_decimal_columns(&results.line, fates.fate[OMEGA_DIVERGES][length], 15u);
        mmgr_line_decimal_columns(&results.line, fates.fate[OMEGA_OPEN][length], 14u);
        mmgr_line_decimal_columns(&results.line, fates.fate[OMEGA_GREW][length], 9u);
        mmgr_line_ch(&results.line, '\n');
    }
    mmgr_line_put(&results.line, "  terms run ");
    mmgr_line_decimal(&results.line, totals[0] + totals[1] + totals[2] + totals[3] + totals[4] + totals[5], 1u);
    mmgr_line_put(&results.line, ": halted ");
    mmgr_line_decimal(&results.line, totals[OMEGA_HALTS], 1u);
    mmgr_line_put(&results.line, ", proven to halt by a simple type ");
    mmgr_line_decimal(&results.line, totals[OMEGA_TYPED], 1u);
    mmgr_line_put(&results.line, ", proven to loop ");
    mmgr_line_decimal(&results.line, totals[OMEGA_LOOPS], 1u);
    mmgr_line_put(&results.line, ", proven to grow forever ");
    mmgr_line_decimal(&results.line, totals[OMEGA_DIVERGES], 1u);
    mmgr_line_put(&results.line, ", out of steps ");
    mmgr_line_decimal(&results.line, totals[OMEGA_OPEN], 1u);
    mmgr_line_put(&results.line, ", outgrew the space ");
    mmgr_line_decimal(&results.line, totals[OMEGA_GREW], 1u);
    mmgr_line_put(&results.line, "\n  program mass through L  ");
    omega_bits(&results.line, &programs, 64u);
    mmgr_line_put(&results.line, "\n  normal forms past L     ");
    omega_bits(&results.line, &normal, 64u);
    mmgr_line_put(&results.line, "\n  Omega lower             ");
    omega_bits(&results.line, &lower, 64u);
    mmgr_line_put(&results.line, "\n  open mass               ");
    omega_bits(&results.line, &open, 64u);
    mmgr_line_put(&results.line, "\n  closed mass past L      ");
    omega_bits(&results.line, &tail, 64u);
    mmgr_line_put(&results.line, "\n  Omega upper             ");
    omega_bits(&results.line, &upper, 64u);
    mmgr_line_put(&results.line, "\n  Omega = 0.");
    for (unsigned int bit = 0u; bit < shared; bit += 1u)
    {
        const unsigned int place = 63u - bit;
        mmgr_line_ch(&results.line, (((lower.limb[place / 32u] >> (place % 32u)) & 1u) != 0u) ? '1' : '0');
    }
    mmgr_line_put(&results.line, "...  (");
    mmgr_line_decimal(&results.line, shared, 1u);
    mmgr_line_put(&results.line, " bits proven)\n");
    sim_flush(&results);

    // the busy beaver table: a length with an open term reads "at least", since the open one may halt later
    // and larger
    mmgr_line_put(&results.line,
                  "\n  busy beavers of the halting terms (>= where a term of that length is still open)\n"
                  "  length  max steps  step champion                             BB lambda  normal form "
                  "champion\n");
    sim_flush(&results);
    for (unsigned int length = 2u; length <= counts.length; length += 1u)
    {
        if (fates.fate[OMEGA_HALTS][length] == 0ULL)
        {
            continue;
        }
        // a typed halt proves a normal form exists without writing it. Its size is still unknown
        const int settled = ((unsettled[length] + fates.fate[OMEGA_TYPED][length]) == 0ULL);
        mmgr_line_decimal_columns(&results.line, length, 8u);
        mmgr_line_put(&results.line, settled ? "    " : "  >=");
        mmgr_line_decimal_columns(&results.line, fates.max_steps[length], 8u);
        mmgr_line_put(&results.line, "  ");
        term.count = 0u;
        omega_unrank(&counts, length, 0u, fates.steps_champion[length], &term);
        omega_code_text(&results.line, &term);
        for (unsigned int pad = length; pad < 42u; pad += 1u)
        {
            mmgr_line_ch(&results.line, ' ');
        }
        mmgr_line_put(&results.line, settled ? "  " : ">=");
        mmgr_line_decimal_columns(&results.line, fates.max_bits[length], 8u);
        mmgr_line_put(&results.line, "  ");
        term.count = 0u;
        omega_unrank(&counts, length, 0u, fates.bits_champion[length], &term);
        omega_code_text(&results.line, &term);
        mmgr_line_ch(&results.line, '\n');
        sim_flush(&results);
    }
    // BusyBeaverWiki's BB lambda (OEIS A333479) from 4 through 33 bits; 0 where no closed term exists. The entries
    // equal the terms of OEIS's b-file for A333479 at 4 to 33. A row with a term still open gives only a lower bound,
    // and the published value is the true max. Where the two meet, the run reached the max. Past 33 the maxima outgrow
    // any space here (327686 bits at 34)
    static const unsigned long long published_max[34] = {
        0ULL,  0ULL,  0ULL,  0ULL,  4ULL,  0ULL,   6ULL,   7ULL,   8ULL,   9ULL,   10ULL, 11ULL,
        12ULL, 13ULL, 14ULL, 15ULL, 16ULL, 17ULL,  18ULL,  19ULL,  20ULL,  22ULL,  24ULL, 26ULL,
        30ULL, 42ULL, 52ULL, 44ULL, 58ULL, 223ULL, 160ULL, 267ULL, 298ULL, 1812ULL};
    int published = 1;
    int bounded = 1;
    for (unsigned int length = 4u; (length <= 33u) && (length <= counts.length); length += 1u)
    {
        const int settled = ((unsettled[length] + fates.fate[OMEGA_TYPED][length]) == 0ULL);
        published = published && (fates.max_bits[length] == published_max[length]);
        bounded = bounded && (fates.max_bits[length] <= published_max[length]) &&
                  ((settled == 0) || (fates.max_bits[length] == published_max[length]));
    }
    // under any budgets: never past the true max, and on it wherever every term of the length halted
    sim_check(&results, bounded,
              "BB lambda is at max BusyBeaverWiki's (OEIS A333479) through 33 bits, and equal to it "
              "at every settled length");
    if ((counts.steps >= OMEGA_STEPS_DEFAULT) && (counts.tokens >= OMEGA_TOKENS_DEFAULT))
    {
        // the default budgets reach every max through 33 bits, the 1812-bit normal form at 33 the largest
        sim_check(&results, published,
                  "BB lambda equals BusyBeaverWiki's (OEIS A333479) at every length through 33 bits");
    }
    return sim_close(&results, "chaitin_omega");
}

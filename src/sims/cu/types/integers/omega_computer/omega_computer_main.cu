// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// omega_computer_main.cu: direct runs, requests and main
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "omega_computer_internal.h"

// Tromp's universal machine, 190 bits: (\1 (\(\\(\\1 (\\2 (1 4) (5 1 (1 1 1)))) (\3 (\2 (3 (\\3 (\\2 3 (1 4))))
// (4 (\4 (\3 1 (2 1))))))) (1 1)) (\1 (2 2))) (\1 1)
static const char s_omega_computer_universal[] =
    "0100010110000100000100000110000001011100110111100101111110100101101010000111100001011100111100000011110000001011"
    "101110011011110011111000011111000010111101001110100110100001100111011000011010";

// Tromp's delimit, 326 bits: reads a Levenshtein code and returns the number it codes as bits
static const char s_omega_computer_delimit[] =
    "0101000110100000000110000101100111100000100101111101111000010101100000000110000111110000001011111101100101111110"
    "1100101111010011101011110001000000101110010101000110100000000001011000010101111110111110000001010111101111101111"
    "110000101100000010111111101011011100000011111100001011011110111001111010000001011000001101100010000010";

// Tromp's test for uni.lam: this after delimit, and the list it must give
static const char s_omega_computer_delimit_input[] = "1111000111001";

static const char s_omega_computer_delimit_output[] = "11010";

// The pool: the arenas, the stack, the work and the arguments of the lazy machine at the cap of its cells, 1792 MiB.
// Also the rewriting's tables at OMEGA_COMPUTER_TERM_MAX, 112 MiB, and room for the sim's own cells
extern "C" const unsigned long long g_program_pool_bytes = 1ULL << 31u;

// the bits of a text that ends in a terminator, without the terminator
#define OMEGA_COMPUTER_TEXT_BITS(text_) (sizeof(text_) - 1u)

// The direct run by the rewrite: the program applied to its input's list. `normal` names the last term, in the
// rewrite's tables until the next rewrite
static OmegaComputerFate omega_computer_direct(const OmegaComputerTerm *program, const char *input, size_t input_count,
                                               unsigned int steps, unsigned int tokens, OmegaComputerTerm *normal,
                                               unsigned long long *taken)
{
    static int s_start_tokens[OMEGA_COMPUTER_PROGRAM_MAX];
    OmegaComputerTerm start = {s_start_tokens, 0u, OMEGA_COMPUTER_PROGRAM_MAX};
    omega_computer_put(&start, OMEGA_COMPUTER_APPLY);
    omega_computer_appended(&start, program->token, program->count);
    omega_computer_list_tokens(input, input_count, &start);
    return omega_computer_rewrite(&start, steps, tokens, taken, normal);
}

static void omega_computer_binary(VerbaLinea *line, unsigned long long numerator, unsigned int bits)
{
    mmgr_line_put(line, "0.");
    for (unsigned int bit = bits; bit > 0u; bit -= 1u)
    {
        mmgr_line_ch(line, (((numerator >> (bit - 1u)) & 1ULL) != 0ULL) ? '1' : '0');
    }
}

// the leading bits a value strictly between low and high over 2^bits is proven to have
static unsigned int omega_computer_proven(unsigned long long low, unsigned long long high, unsigned int bits)
{
    unsigned int proven = 0u;
    for (unsigned int place = 1u; place <= bits; place += 1u)
    {
        const unsigned int shift = bits - place;
        const unsigned long long floor_low = low >> shift;
        // the value is below high. Its first `place` bits are at max ceil(high / 2^shift) - 1
        const unsigned long long top = ((high + ((1ULL << shift) - 1ULL)) >> shift) - 1ULL;
        if (floor_low != top)
        {
            break;
        }
        proven = place;
    }
    return proven;
}

static int omega_computer_request(const char *text, unsigned long long least, unsigned long long max,
                                  unsigned long long *value)
{
    unsigned long long read = 0ULL;
    if ((text == NULL) || (text[0] == '\0'))
    {
        return 0;
    }
    const size_t digits = EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                     .other = "0123456789");
    if (text[digits] != '\0')
    {
        return 0;
    }
    // each digit is a step of the value: the value times 10, and the digit added
    for (size_t at = 0u; at < digits; at += 1u)
    {
        if (read > max)
        {
            return 0;
        }
        // one decimal digit, 0 to 9
        read = (read * 10ULL) + (unsigned long long)(text[at] - '0');
    }
    if ((read < least) || (read > max))
    {
        return 0;
    }
    *value = read;
    return 1;
}

// the tables of the sim's terms, each at a fixed bound
static int s_universal_tokens[OMEGA_COMPUTER_PROGRAM_MAX];
static int s_delimit_tokens[OMEGA_COMPUTER_PROGRAM_MAX];
static int s_expected_tokens[OMEGA_COMPUTER_PROGRAM_MAX];
static int s_identity_tokens[OMEGA_COMPUTER_PROGRAM_MAX];
static int s_nested_tokens[OMEGA_COMPUTER_PROGRAM_MAX];
static int s_program_tokens[OMEGA_COMPUTER_PROGRAM_MAX];
static int s_normal_tokens[OMEGA_COMPUTER_DEPTH_MAX + 1u][OMEGA_COMPUTER_NORMAL_MAX];
static char s_code_bits[OMEGA_COMPUTER_BITS_MAX];
static OmegaComputerMachine s_machine;

int program_main(int count, char **arguments)
{
    char line_buffer[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, line_buffer);
    unsigned long long length = OMEGA_COMPUTER_LENGTH_DEFAULT;
    unsigned long long deepest = OMEGA_COMPUTER_DEPTH_DEFAULT;
    unsigned long long budget = OMEGA_COMPUTER_STEPS_DEFAULT;
    const int understood =
        ((count < 2) || omega_computer_request(arguments[1], 2ULL, OMEGA_COMPUTER_LENGTH_MAX, &length)) &&
        ((count < 3) || omega_computer_request(arguments[2], 1ULL, OMEGA_COMPUTER_DEPTH_MAX, &deepest)) &&
        ((count < 4) || omega_computer_request(arguments[3], 1ULL, 1ULL << 40u, &budget)) && (count <= 4);
    if (understood == 0)
    {
        mmgr_line_put(&results.line, "  usage: omega_computer [L, 2 to 40] [depth, 1 to 8] [steps a run]\n");
        sim_check(&results, 0, "the request names its length, its depth and its steps");
        return sim_close(&results, "omega computer");
    }
    // the request's values are at max 40 and 8
    const unsigned int max = (unsigned int)length;
    const unsigned int depths = (unsigned int)deepest;

    // 1. U, and Tromp's test at every depth
    const char *const universal_code = s_omega_computer_universal;
    const size_t universal_count = OMEGA_COMPUTER_TEXT_BITS(s_omega_computer_universal);
    OmegaComputerTerm universal = {s_universal_tokens, 0u, OMEGA_COMPUTER_PROGRAM_MAX};
    size_t universal_at = 0u;
    const int universal_closed = omega_computer_parse(universal_code, universal_count, &universal_at, 0, &universal) &&
                                 (universal_at == universal_count);
    OmegaComputerTerm delimit = {s_delimit_tokens, 0u, OMEGA_COMPUTER_PROGRAM_MAX};
    size_t delimit_at = 0u;
    const size_t delimit_count = OMEGA_COMPUTER_TEXT_BITS(s_omega_computer_delimit);
    const int delimit_closed =
        omega_computer_parse(s_omega_computer_delimit, delimit_count, &delimit_at, 0, &delimit) &&
        (delimit_at == delimit_count);
    const size_t input_count = OMEGA_COMPUTER_TEXT_BITS(s_omega_computer_delimit_input);
    mmgr_line_put(&results.line, "  U, Tromp's universal machine: ");
    mmgr_line_decimal(&results.line, universal_count, 1u);
    mmgr_line_put(&results.line, " bits, ");
    mmgr_line_decimal(&results.line, universal.count, 1u);
    mmgr_line_put(&results.line, " tokens; delimit: ");
    mmgr_line_decimal(&results.line, delimit_count, 1u);
    mmgr_line_put(&results.line, " bits\n");
    OmegaComputerBits code = {s_code_bits, 0u, sizeof(s_code_bits)};
    omega_computer_code(&universal, &code);
    const int reads_back =
        (code.count == universal_count) &&
        (EMBED_CALL(memor.cmp, MemoriaCfg, .src = code.bit, .other = universal_code, .bytes = universal_count) == 0);
    sim_check(&results, universal_closed && (universal_count == 190u) && reads_back,
              "U is one closed term of 190 bits, and its code reads back to itself");
    sim_flush(&results);
    omega_computer_rewrite_open();
    OmegaComputerMachine *const machine = &s_machine;
    omega_computer_machine_open(machine, &universal);
    machine->step_budget = budget;
    OmegaComputerTerm expected = {s_expected_tokens, 0u, OMEGA_COMPUTER_PROGRAM_MAX};
    omega_computer_list_tokens(s_omega_computer_delimit_output,
                               OMEGA_COMPUTER_TEXT_BITS(s_omega_computer_delimit_output), &expected);
    OmegaComputerTerm normals[OMEGA_COMPUTER_DEPTH_MAX + 1u];
    for (unsigned int depth = 0u; depth <= OMEGA_COMPUTER_DEPTH_MAX; depth += 1u)
    {
        normals[depth].token = s_normal_tokens[depth];
        normals[depth].count = 0u;
        normals[depth].room = OMEGA_COMPUTER_NORMAL_MAX;
    }
    OmegaComputerTerm *const normal = &normals[0];
    int tromp_ok = delimit_closed;
    mmgr_line_put(&results.line, "  Tromp's test, delimit then 1111000111001: depth, fate, beta steps, and the list\n");
    for (unsigned int depth = 0u; depth <= depths; depth += 1u)
    {
        const OmegaComputerFate fate = omega_computer_run(machine, &delimit, s_omega_computer_delimit_input,
                                                          input_count, depth, universal_code, universal_count, normal);
        const int ok = (fate == OMEGA_COMPUTER_HALTS) && omega_computer_same(normal, &expected);
        tromp_ok = tromp_ok && ok;
        mmgr_line_put(&results.line, "    ");
        mmgr_line_decimal(&results.line, depth, 1u);
        mmgr_line_put(&results.line, (fate == OMEGA_COMPUTER_HALTS) ? "  halts  " : "  does not halt  ");
        mmgr_line_decimal(&results.line, machine->steps, 1u);
        mmgr_line_put(&results.line, "  cells ");
        mmgr_line_decimal(&results.line, machine->thunk_count + machine->environment_count + machine->spine_count, 1u);
        mmgr_line_put(&results.line, (ok != 0) ? "  11010\n" : "  NOT 11010\n");
        sim_flush(&results);
    }
    OmegaComputerTerm rewritten;
    unsigned long long rewrite_taken = 0ULL;
    const OmegaComputerFate delimit_direct = omega_computer_direct(
        &delimit, s_omega_computer_delimit_input, input_count, 1u << 20u, 1u << 19u, &rewritten, &rewrite_taken);
    tromp_ok = tromp_ok && (delimit_direct == OMEGA_COMPUTER_HALTS) && omega_computer_same(&rewritten, &expected);
    mmgr_line_put(&results.line, "    the rewriting, directly: fate ");
    mmgr_line_decimal(&results.line, (unsigned long long)delimit_direct, 1u);
    mmgr_line_put(&results.line, ", ");
    mmgr_line_decimal(&results.line, rewrite_taken, 1u);
    mmgr_line_put(&results.line, " steps, ");
    mmgr_line_decimal(&results.line, rewritten.count, 1u);
    mmgr_line_put(&results.line, " tokens\n");
    sim_check(
        &results, tromp_ok,
        "U reading delimit and 1111000111001 gives the list 11010 at every depth, and the rewriting gives it directly");
    // diagnostic: the rewrite in normal order of U nested, its steps and its largest live term
    {
        OmegaComputerTerm identity = {s_identity_tokens, 0u, OMEGA_COMPUTER_PROGRAM_MAX};
        omega_computer_put(&identity, OMEGA_COMPUTER_LAMBDA);
        omega_computer_put(&identity, 1);
        const OmegaComputerTerm *const tried[2] = {&identity, &delimit};
        const char *const inputs[2] = {"", s_omega_computer_delimit_input};
        const size_t input_counts[2] = {0u, input_count};
        for (unsigned int which = 0u; which < 2u; which += 1u)
        {
            for (unsigned int depth = 1u; depth <= 2u; depth += 1u)
            {
                OmegaComputerBits bits = {s_code_bits, 0u, sizeof(s_code_bits)};
                for (unsigned int copy = 1u; copy < depth; copy += 1u)
                {
                    omega_computer_bits_put(&bits, universal_code, universal_count);
                }
                omega_computer_code(tried[which], &bits);
                omega_computer_bits_put(&bits, inputs[which], input_counts[which]);
                OmegaComputerTerm nested = {s_nested_tokens, 0u, OMEGA_COMPUTER_PROGRAM_MAX};
                omega_computer_put(&nested, OMEGA_COMPUTER_APPLY);
                omega_computer_appended(&nested, universal.token, universal.count);
                omega_computer_list_tokens(bits.bit, bits.count, &nested);
                const size_t start = nested.count;
                unsigned long long nested_taken = 0ULL;
                OmegaComputerTerm nested_last;
                const OmegaComputerFate nested_fate =
                    omega_computer_rewrite(&nested, 1u << 18u, 1u << 17u, &nested_taken, &nested_last);
                mmgr_line_put(&results.line, "    rewriting ");
                mmgr_line_put(&results.line, (which == 0u) ? "identity" : "delimit");
                mmgr_line_put(&results.line, " depth ");
                mmgr_line_decimal(&results.line, depth, 1u);
                mmgr_line_put(&results.line, ": fate ");
                mmgr_line_decimal(&results.line, (unsigned long long)nested_fate, 1u);
                mmgr_line_put(&results.line, ", steps ");
                mmgr_line_decimal(&results.line, nested_taken, 1u);
                mmgr_line_put(&results.line, ", start ");
                mmgr_line_decimal(&results.line, start, 1u);
                mmgr_line_put(&results.line, " tokens, peak ");
                mmgr_line_decimal(&results.line, g_omega_computer_peak, 1u);
                mmgr_line_put(&results.line, " tokens\n");
                sim_flush(&results);
            }
        }
    }

    // 2 to 4. every closed M through L bits, run on nil at every depth
    OmegaComputerCounts counts;
    omega_computer_count_all(&counts, max);
    unsigned long long halted[OMEGA_COMPUTER_DEPTH_MAX + 1u] = {0ULL};
    unsigned long long looped[OMEGA_COMPUTER_DEPTH_MAX + 1u] = {0ULL};
    unsigned long long open[OMEGA_COMPUTER_DEPTH_MAX + 1u] = {0ULL};
    unsigned long long grew[OMEGA_COMPUTER_DEPTH_MAX + 1u] = {0ULL};
    unsigned long long halted_mass[OMEGA_COMPUTER_DEPTH_MAX + 1u] = {0ULL};
    unsigned long long steps_total[OMEGA_COMPUTER_DEPTH_MAX + 1u] = {0ULL};
    unsigned long long proven_mass = 0ULL;
    unsigned long long programs = 0ULL;
    unsigned long long direct_halted = 0ULL;
    unsigned long long direct_proven = 0ULL;
    unsigned long long agree_direct = 0ULL;
    unsigned long long lazy_missed = 0ULL;
    unsigned long long settled_everywhere = 0ULL;
    unsigned long long contradictions = 0ULL;
    unsigned long long unnested = 0ULL;
    unsigned long long depth_steps[OMEGA_COMPUTER_DEPTH_MAX + 1u] = {0ULL};
    OmegaComputerFate fates[OMEGA_COMPUTER_DEPTH_MAX + 1u];
    for (unsigned int depth = 0u; depth <= OMEGA_COMPUTER_DEPTH_MAX; depth += 1u)
    {
        fates[depth] = OMEGA_COMPUTER_OPEN;
        normals[depth].count = 0u;
    }
    OmegaComputerTerm program = {s_program_tokens, 0u, OMEGA_COMPUTER_PROGRAM_MAX};
    for (unsigned int bits = 2u; bits <= max; bits += 1u)
    {
        const unsigned long long terms = omega_computer_count(&counts, bits, 0u);
        const unsigned long long mass = 1ULL << (max - bits);
        for (unsigned long long index = 0ULL; index < terms; index += 1ULL)
        {
            program.count = 0u;
            omega_computer_unrank(&counts, bits, 0u, index, &program);
            programs += 1ULL;
            unsigned long long taken = 0ULL;
            OmegaComputerTerm direct_normal;
            const OmegaComputerFate direct = omega_computer_direct(
                &program, "", 0u, OMEGA_COMPUTER_REWRITE_STEPS, OMEGA_COMPUTER_REWRITE_TOKENS, &direct_normal, &taken);
            const int proven_by_rewriting = (direct == OMEGA_COMPUTER_LOOPS) || (direct == OMEGA_COMPUTER_DIVERGES);
            machine->step_budget =
                (proven_by_rewriting && (budget > OMEGA_COMPUTER_NEVER_STEPS)) ? OMEGA_COMPUTER_NEVER_STEPS : budget;
            for (unsigned int depth = 0u; depth <= depths; depth += 1u)
            {
                fates[depth] = omega_computer_run(machine, &program, "", 0u, depth, universal_code, universal_count,
                                                  &normals[depth]);
                depth_steps[depth] = machine->steps;
            }
            // the direct run halts where the rewrite or the lazy machine at depth 0 halts, and never halts where
            // either proves it
            const int rewrite_halts = direct == OMEGA_COMPUTER_HALTS;
            const int rewrite_never = (direct == OMEGA_COMPUTER_LOOPS) || (direct == OMEGA_COMPUTER_DIVERGES);
            const OmegaComputerTerm *const reference = rewrite_halts ? &direct_normal : &normals[0];
            const int reference_halts = rewrite_halts || (fates[0] == OMEGA_COMPUTER_HALTS);
            int never = rewrite_never;
            for (unsigned int depth = 0u; depth <= depths; depth += 1u)
            {
                never = never || (fates[depth] == OMEGA_COMPUTER_LOOPS);
            }
            if (rewrite_halts && (fates[0] == OMEGA_COMPUTER_HALTS))
            {
                const int agree = omega_computer_same(&direct_normal, &normals[0]);
                agree_direct += (agree != 0) ? 1ULL : 0ULL;
                contradictions += (agree != 0) ? 0ULL : 1ULL;
            }
            lazy_missed += (rewrite_halts && (fates[0] != OMEGA_COMPUTER_HALTS)) ? 1ULL : 0ULL;
            direct_halted += (reference_halts != 0) ? 1ULL : 0ULL;
            direct_proven += (never != 0) ? 1ULL : 0ULL;
            contradictions += ((reference_halts != 0) && (never != 0)) ? 1ULL : 0ULL;
            proven_mass += (never != 0) ? mass : 0ULL;
            int everywhere = reference_halts;
            for (unsigned int depth = 0u; depth <= depths; depth += 1u)
            {
                const int halts = (depth == 0u) ? reference_halts : (fates[depth] == OMEGA_COMPUTER_HALTS);
                if (halts != 0)
                {
                    halted[depth] += 1ULL;
                    halted_mass[depth] += mass;
                    // a halt at any depth has the direct run's normal form, and no run proves it never halts
                    if ((depth > 0u) &&
                        (((reference_halts != 0) && (omega_computer_same(&normals[depth], reference) == 0)) ||
                         (never != 0)))
                    {
                        contradictions += 1ULL;
                    }
                    // every M the inner machine finishes, the outer machine finished
                    if (depth > 0u)
                    {
                        const int outer = (depth == 1u) ? reference_halts : (fates[depth - 1u] == OMEGA_COMPUTER_HALTS);
                        unnested += (outer != 0) ? 0ULL : 1ULL;
                    }
                }
                else
                {
                    everywhere = 0;
                    const OmegaComputerFate fate = (depth == 0u) ? (rewrite_never ? direct : fates[0]) : fates[depth];
                    looped[depth] +=
                        ((fate == OMEGA_COMPUTER_LOOPS) || (fate == OMEGA_COMPUTER_DIVERGES)) ? 1ULL : 0ULL;
                    open[depth] += (fate == OMEGA_COMPUTER_OPEN) ? 1ULL : 0ULL;
                    grew[depth] += (fate == OMEGA_COMPUTER_GREW) ? 1ULL : 0ULL;
                }
            }
            if (everywhere != 0)
            {
                settled_everywhere += 1ULL;
                for (unsigned int depth = 0u; depth <= depths; depth += 1u)
                {
                    steps_total[depth] += depth_steps[depth];
                }
            }
        }
    }
    omega_computer_machine_close(machine);
    omega_computer_rewrite_close();

    // the mass of every code through L bits, closed or not, over 2^L; what it leaves of 1 bounds every longer code
    unsigned long long all_codes[OMEGA_COMPUTER_LENGTH_MAX + 1u] = {0ULL};
    unsigned long long counted = 0ULL;
    unsigned long long closed_mass = 0ULL;
    for (unsigned int bits = 2u; bits <= max; bits += 1u)
    {
        unsigned long long codes = 1ULL + all_codes[bits - 2u];
        for (unsigned int left = 2u; (left + 4u) <= bits; left += 1u)
        {
            codes += all_codes[left] * all_codes[bits - 2u - left];
        }
        all_codes[bits] = codes;
        counted += codes << (max - bits);
        closed_mass += omega_computer_count(&counts, bits, 0u) << (max - bits);
    }
    const unsigned long long total = 1ULL << max;
    // the upper bound: everything but the proven non-halts and the codes through L with a free variable
    const unsigned long long upper = total - proven_mass - (counted - closed_mass);

    mmgr_line_put(&results.line, "  ");
    mmgr_line_decimal(&results.line, programs, 1u);
    mmgr_line_put(&results.line, " closed programs through ");
    mmgr_line_decimal(&results.line, max, 1u);
    mmgr_line_put(&results.line, " bits, run on nil; the direct run halts ");
    mmgr_line_decimal(&results.line, direct_halted, 1u);
    mmgr_line_put(&results.line, " and proves ");
    mmgr_line_decimal(&results.line, direct_proven, 1u);
    mmgr_line_put(&results.line, " never halt; the rewriting and the lazy machine agree on ");
    mmgr_line_decimal(&results.line, agree_direct, 1u);
    mmgr_line_put(&results.line, " normal forms\n  depth  halted  proven never  out of steps  outgrew  Omega_nil at "
                                 "least  proven bits  steps (settled everywhere)\n");
    unsigned long long previous_mass = 0ULL;
    int nested = 1;
    for (unsigned int depth = 0u; depth <= depths; depth += 1u)
    {
        nested = nested && ((depth == 0u) || (halted_mass[depth] <= previous_mass));
        previous_mass = halted_mass[depth];
        mmgr_line_put(&results.line, "    ");
        mmgr_line_decimal(&results.line, depth, 1u);
        mmgr_line_put(&results.line, "  ");
        mmgr_line_decimal(&results.line, halted[depth], 1u);
        mmgr_line_put(&results.line, "  ");
        mmgr_line_decimal(&results.line, looped[depth], 1u);
        mmgr_line_put(&results.line, "  ");
        mmgr_line_decimal(&results.line, open[depth], 1u);
        mmgr_line_put(&results.line, "  ");
        mmgr_line_decimal(&results.line, grew[depth], 1u);
        mmgr_line_put(&results.line, "  ");
        omega_computer_binary(&results.line, halted_mass[depth], max);
        mmgr_line_put(&results.line, "  ");
        mmgr_line_decimal(&results.line, omega_computer_proven(halted_mass[depth], upper, max), 1u);
        mmgr_line_put(&results.line, "  ");
        mmgr_line_decimal(&results.line, steps_total[depth], 1u);
        if (depth > 0u)
        {
            mmgr_line_put(&results.line, " (x");
            sim_fraction_print(&results.line, steps_total[depth],
                               (steps_total[depth - 1u] == 0ULL) ? 1ULL : steps_total[depth - 1u], 2u);
            mmgr_line_ch(&results.line, ')');
        }
        mmgr_line_ch(&results.line, '\n');
        sim_flush(&results);
    }
    mmgr_line_put(&results.line, "  Omega_nil at max ");
    omega_computer_binary(&results.line, upper, max);
    mmgr_line_put(&results.line, " at every depth; ");
    mmgr_line_decimal(&results.line, settled_everywhere, 1u);
    mmgr_line_put(&results.line, " programs halt at every depth\n");
    sim_check(&results, contradictions == 0ULL,
              "every normal form at every depth is the direct run's, token for token, and no run halts that any run "
              "proves never halts");
    sim_check(&results, (unnested == 0ULL) && nested,
              "each depth's bracket contains the one outside it: every program the inner machine finishes, the outer "
              "finished");
    sim_check(&results, (lazy_missed == 0ULL) && (agree_direct > 0ULL),
              "the lazy machine reaches every normal form the rewriting reaches, run directly");
    return sim_close(&results, "omega computer");
}

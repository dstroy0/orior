// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// matching_values.cu: Duraiswami's six matching functions valued, every term by its own exact series at each length,
// as sums and chains the record machine runs, one lane a prime (matching.h, term_value.h)
#include "run_cfg.h"

#include "report.h"

#include "term_value.h"
#include "decay_integral.h"
#include "matching.h"
#include "ode_series.h"
#include "record.h"

#include "cycle.h"
#include "key_schedule.h"
#include "keymath.h"

#include <cuda_runtime.h>

#include <string.h>

// The six functions at each eta are the exact forms of matching_functions. At each length every term is written by
// term_value.h as sums and chains over residues, and each form's value after them. Every residue is an exact integer
// held modulo each of the lanes' primes: a sum of rows is one sweep of the record machine, a lane a row and a prime,
// its rows summed on the device by cycle_record_sum; a chain is one lane taking its steps. One sweep's residues
// are the tables the next sweep's lanes read. The primes are as many as the widest value's bits need and two more.
// The program runs twice. Built at the default width, it writes the plan, runs every sweep and writes each value's
// residues, with the width that holds the values whole, beside the record. Built again at that width and given those
// residues, it reads each value whole by the Chinese remainder theorem, reports it and writes the record.
// Checks:
// 1. Every term the forms hold is one term_value.h writes in its independent numbers.
// 2. Every value is written as sums and chains of the record machine.
// 3. Every sweep's records on the device equal the same program's on the host over its first lanes, word for word.
// 4. Every value read back lies within its bound, over every prime.
// 5. At each z the cfg names, w and w' by their series about 1 and by their integral agree within the cfg's agreement
//    at the last length.
// 6. Every function's values at the last two lengths agree within the cfg's agreement.
// 7. Every exact value is held in the build's width.
// 8. Every value is written whole to the record the cfg names.
// The request: matching_values <cfg>, then matching_values <cfg> <residues>, as run.sh runs them.
//     bash examples/navier_stokes/run.sh matching_values examples/navier_stokes/cfg/matching_values.cfg

static const char *const s_matching_values_names[6] = {"torque", "force", "m_inf", "j_inf", "s_inf", "h_match"};

// the bits a lane's prime takes, the primes past the bound that show a residue that disagrees, the lanes a host check
// runs, and the most lanes one sweep holds
static const unsigned int s_matching_values_prime_bits = 31u;
static const unsigned int s_matching_values_spare = 2u;
static const unsigned long long s_matching_values_checked = 16ull;
static const unsigned long long s_matching_values_lanes_most = 1ull << 25u;

// one value the program reads back: its numerator and divisor residues
typedef struct
{
    std::string name;
    unsigned int numerator;
    unsigned int divisor;
    int zero;
} MatchingValuesOutput;

static unsigned int matching_values_bit_length(unsigned long long value)
{
    unsigned int bits = 0u;
    while (value != 0ull)
    {
        bits += 1u;
        value >>= 1u;
    }
    return bits;
}

// `count` primes below 2^31, from the largest down
static std::vector<unsigned long long> matching_values_primes(unsigned long long count)
{
    std::vector<unsigned long long> small;
    for (unsigned long long candidate = 3ull; candidate * candidate < (1ull << s_matching_values_prime_bits); candidate += 2ull)
    {
        int prime = 1;
        for (unsigned long long divisor : small)
        {
            if (divisor * divisor > candidate)
            {
                break;
            }
            if ((candidate % divisor) == 0ull)
            {
                prime = 0;
                break;
            }
        }
        if (prime)
        {
            small.push_back(candidate);
        }
    }
    std::vector<unsigned long long> primes;
    for (unsigned long long candidate = (1ull << s_matching_values_prime_bits) - 1ull; primes.size() < count; candidate -= 2ull)
    {
        int prime = 1;
        for (unsigned long long divisor : small)
        {
            if (divisor * divisor > candidate)
            {
                break;
            }
            if ((candidate % divisor) == 0ull)
            {
                prime = 0;
                break;
            }
        }
        if (prime)
        {
            primes.push_back(candidate);
        }
    }
    return primes;
}

// a program for one sweep: its steps, its tables with the limbs each holds, and the steps it writes out
typedef struct
{
    std::vector<EngineRecordStep> steps;
    std::vector<EngineRecordTable> tables;
    std::vector<std::vector<unsigned int>> held;
    std::vector<unsigned int> outputs;
    unsigned int zero;
    unsigned int prime;
    unsigned int place;
    unsigned int row;
} MatchingValuesProgram;

static unsigned int matching_values_step(MatchingValuesProgram *program, EngineRecordOperation operation, unsigned int left,
                                         unsigned int right)
{
    EngineRecordStep step = {operation, left, right, 0u};
    program->steps.push_back(step);
    return (unsigned int)program->steps.size() - 1u;
}

static unsigned int matching_values_constant(MatchingValuesProgram *program, unsigned long long value)
{
    return matching_values_step(program, ENGINE_RECORD_CONSTANT, (unsigned int)(value & 0xFFFFFFFFull), (unsigned int)(value >> 32u));
}

// a table holding `entries`, each row as many limbs as the widest entry needs; every row past them 0
static unsigned int matching_values_table(MatchingValuesProgram *program, const std::vector<std::vector<unsigned int>> &entries)
{
    unsigned int out_bits = 1u;
    for (const std::vector<unsigned int> &entry : entries)
    {
        for (size_t limb = entry.size(); limb > 0u; limb -= 1u)
        {
            if (entry[limb - 1u] != 0u)
            {
                const unsigned int bits = 32u * ((unsigned int)limb - 1u) + matching_values_bit_length(entry[limb - 1u]);
                out_bits = (bits > out_bits) ? bits : out_bits;
                break;
            }
        }
    }
    const unsigned int index_bits = (entries.size() <= 2u) ? 1u : matching_values_bit_length(entries.size() - 1u);
    const unsigned int limbs = (out_bits + 31u) / 32u;
    std::vector<unsigned int> held((size_t)limbs << index_bits, 0u);
    for (size_t row = 0u; row < entries.size(); row += 1u)
    {
        for (size_t limb = 0u; (limb < entries[row].size()) && (limb < limbs); limb += 1u)
        {
            held[row * limbs + limb] = entries[row][limb];
        }
    }
    EngineRecordTable table = {index_bits, out_bits, NULL};
    program->tables.push_back(table);
    program->held.push_back(held);
    return (unsigned int)program->tables.size() - 1u;
}

// a table of words, each below 2^64
static unsigned int matching_values_words(MatchingValuesProgram *program, const std::vector<unsigned long long> &words)
{
    std::vector<std::vector<unsigned int>> entries(words.size());
    for (size_t row = 0u; row < words.size(); row += 1u)
    {
        entries[row] = {(unsigned int)(words[row] & 0xFFFFFFFFull), (unsigned int)(words[row] >> 32u)};
    }
    return matching_values_table(program, entries);
}

static unsigned int matching_values_look(MatchingValuesProgram *program, unsigned int index, unsigned int table)
{
    return matching_values_step(program, ENGINE_RECORD_TABLE, index, table);
}

static unsigned int matching_values_reduce(MatchingValuesProgram *program, unsigned int value)
{
    return matching_values_step(program, ENGINE_RECORD_REMAINDER, value, program->prime);
}

static unsigned int matching_values_times(MatchingValuesProgram *program, unsigned int left, unsigned int right)
{
    return matching_values_reduce(program, matching_values_step(program, ENGINE_RECORD_PRODUCT, left, right));
}

// the lane read as its run, its row in the run and its prime: lane = (run K + place) group + row, the row of the sweep
// run group + row and the prime the place-th
static void matching_values_begin(MatchingValuesProgram *program, unsigned long long group, unsigned long long count,
                                   const std::vector<unsigned long long> &primes)
{
    program->zero = matching_values_step(program, ENGINE_RECORD_FIELD, 0u, 0u);
    const unsigned int lane = matching_values_step(program, ENGINE_RECORD_LANE, 0u, 0u);
    const unsigned int width = matching_values_constant(program, group);
    const unsigned int size = matching_values_constant(program, count);
    const unsigned int row = matching_values_step(program, ENGINE_RECORD_REMAINDER, lane, width);
    const unsigned int above = matching_values_step(program, ENGINE_RECORD_QUOTIENT, lane, width);
    program->place = matching_values_step(program, ENGINE_RECORD_REMAINDER, above, size);
    const unsigned int run = matching_values_step(program, ENGINE_RECORD_QUOTIENT, above, size);
    const unsigned int first = matching_values_step(program, ENGINE_RECORD_PRODUCT, run, width);
    program->row = matching_values_step(program, ENGINE_RECORD_SUM, matching_values_step(program, ENGINE_RECORD_SUM, first, row),
                                        program->zero);
    program->prime = matching_values_look(program, program->place, matching_values_words(program, primes));
}

// the residues a sweep reads, each given a place in the sweep's table of them
typedef struct
{
    std::map<unsigned int, unsigned int> place;
    std::vector<unsigned int> residues;
} MatchingValuesRead;

static unsigned int matching_values_read_place(MatchingValuesRead *read, unsigned int residue)
{
    const auto found = read->place.find(residue);
    if (found != read->place.end())
    {
        return found->second;
    }
    const unsigned int place = (unsigned int)read->residues.size();
    read->place[residue] = place;
    read->residues.push_back(residue);
    return place;
}

// the table of every residue the sweep reads: entry r K_pad + j the residue's integer modulo the j-th prime, as written
static unsigned int matching_values_residue_table(MatchingValuesProgram *program, const MatchingValuesRead &read,
                                                  const std::vector<std::vector<unsigned long long>> &store,
                                                  unsigned long long span)
{
    std::vector<unsigned long long> words(read.residues.size() * span, 0ull);
    for (size_t at = 0u; at < read.residues.size(); at += 1u)
    {
        const std::vector<unsigned long long> &held = store[read.residues[at]];
        for (size_t place = 0u; place < held.size(); place += 1u)
        {
            words[at * span + place] = held[place];
        }
    }
    return matching_values_words(program, words);
}

// the residue's value in this lane: entry place K_pad + the lane's place of the table
static unsigned int matching_values_take(MatchingValuesProgram *program, unsigned int place, unsigned int table,
                                          unsigned long long span)
{
    // the index plus 2^index_bits: the table reads the same low bits, and the register is as wide as the table's index
    const unsigned int start = matching_values_step(program, ENGINE_RECORD_PRODUCT, place, matching_values_constant(program, span));
    const unsigned int index = matching_values_step(program, ENGINE_RECORD_SUM, start, program->place);
    const unsigned long long above = 1ull << program->tables[table].index_bits;
    return matching_values_look(program, matching_values_step(program, ENGINE_RECORD_SUM, index, matching_values_constant(program, above)),
                                table);
}

// the entry at row `row` times `width` plus `at` of a table laid out a row of the sweep at a time
static unsigned int matching_values_entry(MatchingValuesProgram *program, unsigned int base, unsigned int at, unsigned int table)
{
    const unsigned int index = (at == 0u) ? base : matching_values_step(program, ENGINE_RECORD_SUM, base, matching_values_constant(program, at));
    return matching_values_look(program, index, table);
}

// p - value where `negative` is 1: value + negative (p - 2 value), then reduced
static unsigned int matching_values_signed(MatchingValuesProgram *program, unsigned int value, unsigned int negative)
{
    const unsigned int rest = matching_values_step(program, ENGINE_RECORD_DIFFERENCE,
                                                   matching_values_step(program, ENGINE_RECORD_DIFFERENCE, program->prime, value), value);
    return matching_values_reduce(program, matching_values_step(program, ENGINE_RECORD_SUM, value,
                                                                matching_values_step(program, ENGINE_RECORD_PRODUCT, negative, rest)));
}

static unsigned int matching_values_sign_bit(int negative)
{
    return negative ? 1u : 0u;
}

// a signed word below 2^62 as a table's entry: word + 2^62
static unsigned long long matching_values_lifted(long long word)
{
    return (unsigned long long)word + (1ull << 62u);
}

// one sweep's program laid out and run on the device: its records, and the host's over the first lanes checked
typedef struct
{
    EngineRecordKey key;
    EngineRecordLayout layout;
    unsigned int out_limbs;
} MatchingValuesLaid;

typedef struct
{
    std::vector<unsigned int> host_words;
    unsigned long long sweeps;
    unsigned long long checked;
    unsigned long long agreed;
    unsigned long long steps_most;
    unsigned long long lanes;
    int compiled;
    int ok;
} MatchingValuesRuns;

// the sweep run: every lane on the device, its first lanes on the host to check the device; `sum` the output summed over
// each run of `group` lanes into `sums`, or each output read lane by lane into `fields` where group is 1
static int matching_values_sweep(MatchingValuesProgram *program, unsigned long long lanes, unsigned long long group,
                                 MatchingValuesRuns *runs, std::vector<std::vector<long long>> *fields)
{
    for (size_t table = 0u; table < program->tables.size(); table += 1u)
    {
        program->tables[table].values = program->held[table].data();
    }
    EngineError error;
    memset(&error, 0, sizeof(error));
    MatchingValuesLaid laid;
    memset(&laid, 0, sizeof(laid));
    const unsigned int field_bits[1] = {1u};
    const unsigned int field_offset[1] = {0u};
    const KeymathRecordRequest encode = {program->steps.data(), (unsigned int)program->steps.size(), field_bits, 1u, 1u,
                                         program->outputs.data(), (unsigned int)program->outputs.size(),
                                         program->tables.data(), (unsigned int)program->tables.size(), &laid.key, &error};
    int ok = keymath_record_encode(&encode) != KEYMATH_ERROR;
    unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX] = {1u, 0u, 0u};
    const KeyScheduleRecordRequest lay = {&laid.key, field_offset, 1u, in_limbs, 1, &laid.layout, &error};
    const int encoded = ok;
    ok = ok && (key_schedule_record_layout(&lay) != KEY_SCHEDULE_ERROR);
    const int laid_out = ok;
    CycleRecord *record = NULL;
    ok = ok && (cycle_record_load(&laid.layout, &record, &error) != CYCLE_ERROR);
    if (!ok)
    {
        fprintf(stderr, "  matching_values: a sweep of %zu steps did not %s: module %d site %u status %d\n",
                program->steps.size(), !encoded ? "imprint" : (!laid_out ? "lay out" : "load"), (int)error.module, error.site,
                error.status);
        const EngineRecordStep *const first_step = program->steps.data();
        const EngineRecordTable *const first_table = program->tables.data();
        const EngineRecordStep *const at_step = (const EngineRecordStep *)error.evacaddr;
        const EngineRecordTable *const at_table = (const EngineRecordTable *)error.evacaddr;
        if ((at_step >= first_step) && (at_step < first_step + program->steps.size()))
        {
            fprintf(stderr, "    at step %lld, operation %d, left %u right %u\n", (long long)(at_step - first_step),
                    (int)at_step->operation, at_step->left, at_step->right);
        }
        else if ((at_table >= first_table) && (at_table < first_table + program->tables.size()))
        {
            fprintf(stderr, "    at table %lld, index bits %u, out bits %u\n", (long long)(at_table - first_table),
                    at_table->index_bits, at_table->out_bits);
        }
    }
    const unsigned int out_limbs = ok ? laid.layout.out_limbs : 0u;
    unsigned int *device_in = NULL;
    unsigned int *device_out = NULL;
    const unsigned int in_word = 0u;
    ok = ok && (cudaMalloc((void **)&device_in, sizeof(unsigned int)) == cudaSuccess) &&
         (cudaMalloc((void **)&device_out, (size_t)(lanes * out_limbs) * sizeof(unsigned int)) == cudaSuccess) &&
         (cudaMemcpy(device_in, &in_word, sizeof(unsigned int), cudaMemcpyHostToDevice) == cudaSuccess);
    const CycleRecordRunRequest run = {record, {device_in, NULL, NULL}, {1ull, 0ull, 0ull}, NULL, lanes, device_out, &error};
    ok = ok && (cycle_record_run(&run) == (long)lanes);
    // the host's records of the first lanes, from the exact integer library, against the device's
    const unsigned long long checked = (lanes < s_matching_values_checked) ? lanes : s_matching_values_checked;
    std::vector<unsigned int> host_out((size_t)(checked * out_limbs), 0u);
    std::vector<unsigned int> device_head((size_t)(checked * out_limbs), 0u);
    const CycleRecordHostRequest host = {&laid.layout, {&in_word, NULL, NULL}, {1ull, 0ull, 0ull}, NULL, checked,
                                         host_out.data(), &error};
    const int same = ok && (cycle_record_run_host(&host) == (long)checked) &&
                     (cudaMemcpy(device_head.data(), device_out, device_head.size() * sizeof(unsigned int),
                                 cudaMemcpyDeviceToHost) == cudaSuccess) &&
                     (memcmp(host_out.data(), device_head.data(), device_head.size() * sizeof(unsigned int)) == 0);
    runs->sweeps += 1ull;
    runs->checked += checked;
    runs->agreed += same ? checked : 0ull;
    runs->lanes += lanes;
    runs->steps_most = (program->steps.size() > runs->steps_most) ? program->steps.size() : runs->steps_most;
    const int compiled = (record != NULL) && cycle_record_compiled(record);
    runs->compiled = runs->compiled && compiled;
    printf("    sweep: %zu steps, %llu lanes in runs of %llu, %s, %llu of %llu host lanes agree\n", program->steps.size(), lanes,
           group, compiled ? "compiled" : "interpreted", same ? checked : 0ull, checked);
    fields->assign(program->outputs.size(), std::vector<long long>());
    if (ok && (group > 1ull))
    {
        // the first output summed over each run of `group` lanes
        const DeviceRecordStep *const step = &laid.layout.step_table[program->outputs[0]];
        std::vector<unsigned int> sums((size_t)(lanes / group) * 2u, 0u);
        const CycleRecordSumRequest sum = {device_out, lanes, group, out_limbs, step->out_offset, step->out_bits, 2u,
                                           sums.data(), &error};
        ok = cycle_record_sum(&sum) != CYCLE_ERROR;
        (*fields)[0].resize((size_t)(lanes / group));
        for (size_t at = 0u; ok && (at < (*fields)[0].size()); at += 1u)
        {
            (*fields)[0][at] = (long long)((unsigned long long)sums[2u * at] | ((unsigned long long)sums[2u * at + 1u] << 32u));
        }
    }
    else if (ok)
    {
        std::vector<unsigned int> words((size_t)(lanes * out_limbs), 0u);
        ok = cudaMemcpy(words.data(), device_out, words.size() * sizeof(unsigned int), cudaMemcpyDeviceToHost) == cudaSuccess;
        for (size_t output = 0u; ok && (output < program->outputs.size()); output += 1u)
        {
            const DeviceRecordStep *const step = &laid.layout.step_table[program->outputs[output]];
            (*fields)[output].resize((size_t)lanes);
            for (unsigned long long lane = 0ull; lane < lanes; lane += 1ull)
            {
                unsigned long long word = 0ull;
                for (unsigned int bit = 0u; bit < step->out_bits; bit += 1u)
                {
                    const unsigned int at = step->out_offset + bit;
                    word |= (unsigned long long)((words[lane * out_limbs + at / 32u] >> (at % 32u)) & 1u) << bit;
                }
                (*fields)[output][lane] = (long long)word;
            }
        }
    }
    if (!ok && encoded && laid_out)
    {
        fprintf(stderr, "  matching_values: a sweep of %llu lanes did not run: module %d site %u status %d\n", lanes,
                (int)error.module, error.site, error.status);
    }
    cudaFree(device_in);
    cudaFree(device_out);
    if (record != NULL)
    {
        cycle_record_release(record);
    }
    if (laid_out)
    {
        key_schedule_record_release(&laid.layout);
    }
    if (encoded)
    {
        keymath_record_release(&laid.key);
    }
    runs->ok = runs->ok && ok;
    return ok;
}

// the runs given, each `group` chains, swept: X times the tail summed over each run, or with Y and every step's X
// where the group is 1
static int matching_values_chains(const TermValues *plan, const std::vector<const TermValueRun *> &chosen, unsigned long long group,
                                  const std::vector<unsigned long long> &primes, unsigned long long span,
                                  std::vector<std::vector<unsigned long long>> *store, MatchingValuesRuns *runs)
{
    const unsigned long long count = primes.size();
    MatchingValuesProgram program;
    matching_values_begin(&program, group, count, primes);
    size_t steps = 0u;
    size_t tail_words = 1u;
    int keep = 0;
    for (const TermValueRun *run : chosen)
    {
        for (const TermValueChain &chain : run->chains)
        {
            steps = (chain.steps.size() > steps) ? chain.steps.size() : steps;
            tail_words = (term_value_words(chain.tail.primes).size() > tail_words) ? term_value_words(chain.tail.primes).size() : tail_words;
            keep = keep || !chain.history.empty();
        }
    }
    // every chain of the sweep at its row; a run short of `group` chains, and a chain short of the most steps, filled
    // with chains and steps that change nothing and whose tail is 0
    const unsigned long long rows = chosen.size() * group;
    MatchingValuesRead read;
    std::vector<unsigned long long> x_words(rows, matching_values_lifted(0ll));
    std::vector<unsigned long long> y_words(rows, matching_values_lifted(0ll));
    std::vector<unsigned long long> x_places(rows, 0ull);
    std::vector<unsigned long long> y_places(rows, 0ull);
    std::vector<unsigned long long> coefficients[4];
    for (unsigned int which = 0u; which < 4u; which += 1u)
    {
        coefficients[which].assign(rows * (steps == 0u ? 1u : steps), matching_values_lifted((which == 0u || which == 3u) ? 1ll : 0ll));
    }
    std::vector<unsigned long long> tails(rows * tail_words, 1ull);
    std::vector<unsigned long long> signs(rows, 0ull);
    matching_values_read_place(&read, 0u);
    for (size_t at = 0u; at < chosen.size(); at += 1u)
    {
        for (size_t member = 0u; member < group; member += 1u)
        {
            const size_t row = at * group + member;
            if (member >= chosen[at]->chains.size())
            {
                tails[row * tail_words] = 0ull;
                continue;
            }
            const TermValueChain &chain = chosen[at]->chains[member];
            x_words[row] = matching_values_lifted(chain.x_word);
            y_words[row] = matching_values_lifted(chain.y_word);
            x_places[row] = matching_values_read_place(&read, chain.x_residue);
            y_places[row] = matching_values_read_place(&read, chain.y_residue);
            const size_t start = steps - chain.steps.size();
            for (size_t step = 0u; step < chain.steps.size(); step += 1u)
            {
                const TermValueStep &held = chain.steps[step];
                coefficients[0][row * steps + start + step] = matching_values_lifted(held.a);
                coefficients[1][row * steps + start + step] = matching_values_lifted(held.b);
                coefficients[2][row * steps + start + step] = matching_values_lifted(held.c);
                coefficients[3][row * steps + start + step] = matching_values_lifted(held.d);
            }
            const std::vector<unsigned long long> words = term_value_words(chain.tail.primes);
            for (size_t word = 0u; word < words.size(); word += 1u)
            {
                tails[row * tail_words + word] = words[word];
            }
            signs[row] = matching_values_sign_bit(chain.tail.negative);
        }
    }
    // a coefficient 0 or 1 in every step of every chain is no table: 0 drops its term, 1 takes its side as it is
    int fixed[4] = {0, 0, 0, 0};
    long long fixed_value[4] = {0ll, 0ll, 0ll, 0ll};
    for (unsigned int which = 0u; which < 4u; which += 1u)
    {
        for (long long candidate = 0ll; candidate <= 1ll; candidate += 1ll)
        {
            int every = 1;
            for (unsigned long long entry : coefficients[which])
            {
                every = every && (entry == matching_values_lifted(candidate));
            }
            if (every)
            {
                fixed[which] = 1;
                fixed_value[which] = candidate;
            }
        }
    }
    const unsigned int residues = matching_values_residue_table(&program, read, *store, span);
    const unsigned int x_word_table = matching_values_words(&program, x_words);
    const unsigned int y_word_table = matching_values_words(&program, y_words);
    const unsigned int x_place_table = matching_values_words(&program, x_places);
    const unsigned int y_place_table = matching_values_words(&program, y_places);
    unsigned int coefficient_tables[4] = {0u, 0u, 0u, 0u};
    for (unsigned int which = 0u; which < 4u; which += 1u)
    {
        if (!fixed[which])
        {
            coefficient_tables[which] = matching_values_words(&program, coefficients[which]);
        }
    }
    const unsigned int tail_table = matching_values_words(&program, tails);
    const unsigned int sign_table = matching_values_words(&program, signs);
    // a signed entry e = c + 2^62 read as c modulo p: e + 2^62 - (2^63 mod p), never negative, reduced
    const unsigned int shift = matching_values_step(
        &program, ENGINE_RECORD_DIFFERENCE, matching_values_constant(&program, 1ull << 62u),
        matching_values_reduce(&program, matching_values_constant(&program, 1ull << 63u)));
    const unsigned int row = program.row;
    const unsigned int x_start = matching_values_times(
        &program,
        matching_values_reduce(&program, matching_values_step(&program, ENGINE_RECORD_SUM, matching_values_look(&program, row, x_word_table), shift)),
        matching_values_take(&program, matching_values_look(&program, row, x_place_table), residues, span));
    const unsigned int y_start = matching_values_times(
        &program,
        matching_values_reduce(&program, matching_values_step(&program, ENGINE_RECORD_SUM, matching_values_look(&program, row, y_word_table), shift)),
        matching_values_take(&program, matching_values_look(&program, row, y_place_table), residues, span));
    unsigned int x = x_start;
    unsigned int y = y_start;
    const unsigned int base = matching_values_step(&program, ENGINE_RECORD_PRODUCT, row, matching_values_constant(&program, steps));
    std::vector<unsigned int> history;
    for (size_t step = 0u; step < steps; step += 1u)
    {
        // each coefficient c read as e + 2^62 - (2^63 mod p), congruent to c and never negative; the step's sum is
        // reduced once
        const unsigned int index =
            (step == 0u) ? base : matching_values_step(&program, ENGINE_RECORD_SUM, base, matching_values_constant(&program, step));
        unsigned int coefficient[4] = {0u, 0u, 0u, 0u};
        for (unsigned int which = 0u; which < 4u; which += 1u)
        {
            if (!fixed[which])
            {
                coefficient[which] = matching_values_step(&program, ENGINE_RECORD_SUM,
                                                          matching_values_look(&program, index, coefficient_tables[which]), shift);
            }
        }
        unsigned int next[2] = {0u, 0u};
        for (unsigned int half = 0u; half < 2u; half += 1u)
        {
            unsigned int parts[2] = {0u, 0u};
            int held[2] = {0, 0};
            const unsigned int sides[2] = {x, y};
            for (unsigned int side = 0u; side < 2u; side += 1u)
            {
                const unsigned int which = 2u * half + side;
                if (fixed[which] && (fixed_value[which] == 0ll))
                {
                    continue;
                }
                held[side] = 1;
                parts[side] = fixed[which] ? sides[side]
                                           : matching_values_step(&program, ENGINE_RECORD_PRODUCT, coefficient[which], sides[side]);
            }
            if (held[0] && held[1])
            {
                next[half] = matching_values_reduce(&program, matching_values_step(&program, ENGINE_RECORD_SUM, parts[0], parts[1]));
            }
            else if (held[0] || held[1])
            {
                const unsigned int only = held[0] ? 0u : 1u;
                const unsigned int which = 2u * half + only;
                next[half] = fixed[which] ? parts[only] : matching_values_reduce(&program, parts[only]);
            }
            else
            {
                next[half] = matching_values_constant(&program, 0ull);
            }
        }
        x = next[0];
        y = next[1];
        history.push_back(x);
    }
    unsigned int ended = x;
    const unsigned int tail_base =
        matching_values_step(&program, ENGINE_RECORD_PRODUCT, row, matching_values_constant(&program, tail_words));
    for (size_t word = 0u; word < tail_words; word += 1u)
    {
        ended = matching_values_times(&program, ended, matching_values_entry(&program, tail_base, (unsigned int)word, tail_table));
    }
    ended = matching_values_signed(&program, ended, matching_values_look(&program, row, sign_table));
    // each output a step of its own: a step named twice is copied by a sum with the zero field
    std::map<unsigned int, int> named;
    std::vector<unsigned int> written = {ended};
    if (group == 1ull)
    {
        written.push_back(y);
        if (keep)
        {
            written.insert(written.end(), history.begin(), history.end());
        }
    }
    for (unsigned int step : written)
    {
        const unsigned int out = named[step] ? matching_values_step(&program, ENGINE_RECORD_SUM, step, program.zero) : step;
        named[step] = 1;
        named[out] = 1;
        program.outputs.push_back(out);
    }
    std::vector<std::vector<long long>> fields;
    const unsigned long long lanes = chosen.size() * count * group;
    if (!matching_values_sweep(&program, lanes, group, runs, &fields))
    {
        return 0;
    }
    for (size_t at = 0u; at < chosen.size(); at += 1u)
    {
        const TermValueRun *run = chosen[at];
        std::vector<unsigned long long> &x_held = (*store)[run->x_target];
        x_held.assign(count, 0ull);
        for (unsigned long long place = 0ull; place < count; place += 1ull)
        {
            x_held[place] = (unsigned long long)fields[0][at * count + place];
        }
        if (group != 1ull)
        {
            continue;
        }
        std::vector<unsigned long long> &y_held = (*store)[run->y_target];
        y_held.assign(count, 0ull);
        for (unsigned long long place = 0ull; place < count; place += 1ull)
        {
            y_held[place] = (unsigned long long)fields[1][at * count + place];
        }
        const std::vector<unsigned int> &kept = run->chains[0].history;
        for (size_t step = 0u; step < kept.size(); step += 1u)
        {
            std::vector<unsigned long long> &held = (*store)[kept[step]];
            held.assign(count, 0ull);
            const size_t output = 2u + (steps - kept.size()) + step;
            for (unsigned long long place = 0ull; place < count; place += 1ull)
            {
                held[place] = (unsigned long long)fields[output][at * count + place];
            }
        }
    }
    return 1;
}

// the sums given, each at most `group` rows, swept: each row a lane over each prime, the rows of a sum summed on the
// device
static int matching_values_sums(const TermValues *plan, const std::vector<const TermValueSum *> &chosen, unsigned long long group,
                                const std::vector<unsigned long long> &primes, unsigned long long span,
                                std::vector<std::vector<unsigned long long>> *store, MatchingValuesRuns *runs)
{
    (void)plan;
    const unsigned long long count = primes.size();
    MatchingValuesProgram program;
    matching_values_begin(&program, group, count, primes);
    size_t raised_most = 0u;
    size_t plain_most = 0u;
    size_t word_most = 1u;
    unsigned int exponent_most = 1u;
    int wide = 0;
    for (const TermValueSum *sum : chosen)
    {
        for (const TermValueRow &row : sum->rows)
        {
            size_t raised = 0u;
            size_t plain = 0u;
            for (const auto &entry : row.residues)
            {
                raised += (entry.second > 1u) ? 1u : 0u;
                plain += (entry.second == 1u) ? 1u : 0u;
                exponent_most = (entry.second > exponent_most) ? entry.second : exponent_most;
            }
            raised_most = (raised > raised_most) ? raised : raised_most;
            plain_most = (plain > plain_most) ? plain : plain_most;
            const size_t words = term_value_words(row.primes).size();
            word_most = (words > word_most) ? words : word_most;
            wide = wide || !row.wide.empty();
        }
    }
    const unsigned int exponent_bits = matching_values_bit_length(exponent_most);
    const unsigned long long rows = chosen.size() * group;
    MatchingValuesRead read;
    matching_values_read_place(&read, 0u);
    std::vector<unsigned long long> raised_places(rows * (raised_most == 0u ? 1u : raised_most), 0ull);
    std::vector<unsigned long long> raised_powers(rows * (raised_most == 0u ? 1u : raised_most), 0ull);
    std::vector<unsigned long long> plain_places(rows * (plain_most == 0u ? 1u : plain_most), 0ull);
    std::vector<unsigned long long> words(rows * word_most, 1ull);
    std::vector<std::vector<unsigned int>> wide_rows(wide ? rows : 0u, std::vector<unsigned int>(1u, 1u));
    std::vector<unsigned long long> signs(rows, 0ull);
    for (size_t at = 0u; at < chosen.size(); at += 1u)
    {
        for (size_t member = 0u; member < group; member += 1u)
        {
            const size_t row = at * group + member;
            if (member >= chosen[at]->rows.size())
            {
                words[row * word_most] = 0ull;
                continue;
            }
            const TermValueRow &held = chosen[at]->rows[member];
            size_t raised = 0u;
            size_t plain = 0u;
            for (const auto &entry : held.residues)
            {
                if (entry.second > 1u)
                {
                    raised_places[row * raised_most + raised] = matching_values_read_place(&read, entry.first);
                    raised_powers[row * raised_most + raised] = entry.second;
                    raised += 1u;
                }
                else
                {
                    plain_places[row * plain_most + plain] = matching_values_read_place(&read, entry.first);
                    plain += 1u;
                }
            }
            const std::vector<unsigned long long> factors = term_value_words(held.primes);
            for (size_t word = 0u; word < factors.size(); word += 1u)
            {
                words[row * word_most + word] = factors[word];
            }
            if (wide && !held.wide.empty())
            {
                wide_rows[row] = held.wide;
            }
            signs[row] = matching_values_sign_bit(held.negative);
        }
    }
    const unsigned int residues = matching_values_residue_table(&program, read, *store, span);
    const unsigned int row = program.row;
    unsigned int value = matching_values_constant(&program, 1ull);
    if (raised_most > 0u)
    {
        const unsigned int place_table = matching_values_words(&program, raised_places);
        const unsigned int power_table = matching_values_words(&program, raised_powers);
        const unsigned int base =
            matching_values_step(&program, ENGINE_RECORD_PRODUCT, row, matching_values_constant(&program, raised_most));
        const unsigned int one = matching_values_constant(&program, 1ull);
        const unsigned int two = matching_values_constant(&program, 2ull);
        for (size_t slot = 0u; slot < raised_most; slot += 1u)
        {
            const unsigned int factor = matching_values_reduce(
                &program, matching_values_take(&program, matching_values_entry(&program, base, (unsigned int)slot, place_table), residues, span));
            const unsigned int power = matching_values_entry(&program, base, (unsigned int)slot, power_table);
            const unsigned int less = matching_values_step(&program, ENGINE_RECORD_DIFFERENCE, factor, one);
            // factor^power by its bits from the top: square, then times factor where the bit is 1
            unsigned int raised = one;
            for (unsigned int bit = exponent_bits; bit > 0u; bit -= 1u)
            {
                if (bit != exponent_bits)
                {
                    raised = matching_values_times(&program, raised, raised);
                }
                const unsigned int shifted = matching_values_step(&program, ENGINE_RECORD_QUOTIENT, power,
                                                                  matching_values_constant(&program, 1ull << (bit - 1u)));
                const unsigned int held = matching_values_step(&program, ENGINE_RECORD_REMAINDER, shifted, two);
                const unsigned int chosen_factor =
                    matching_values_step(&program, ENGINE_RECORD_SUM, one, matching_values_step(&program, ENGINE_RECORD_PRODUCT, held, less));
                raised = matching_values_times(&program, raised, chosen_factor);
            }
            value = matching_values_times(&program, value, raised);
        }
    }
    if (plain_most > 0u)
    {
        const unsigned int place_table = matching_values_words(&program, plain_places);
        const unsigned int base =
            matching_values_step(&program, ENGINE_RECORD_PRODUCT, row, matching_values_constant(&program, plain_most));
        for (size_t slot = 0u; slot < plain_most; slot += 1u)
        {
            value = matching_values_times(
                &program, value,
                matching_values_take(&program, matching_values_entry(&program, base, (unsigned int)slot, place_table), residues, span));
        }
    }
    const unsigned int word_table = matching_values_words(&program, words);
    const unsigned int word_base = matching_values_step(&program, ENGINE_RECORD_PRODUCT, row, matching_values_constant(&program, word_most));
    for (size_t word = 0u; word < word_most; word += 1u)
    {
        value = matching_values_times(&program, value, matching_values_entry(&program, word_base, (unsigned int)word, word_table));
    }
    if (wide)
    {
        const unsigned int wide_table = matching_values_table(&program, wide_rows);
        value = matching_values_times(&program, value, matching_values_reduce(&program, matching_values_look(&program, row, wide_table)));
    }
    value = matching_values_signed(&program, value, matching_values_look(&program, row, matching_values_words(&program, signs)));
    program.outputs.push_back(value);
    std::vector<std::vector<long long>> fields;
    const unsigned long long lanes = chosen.size() * count * group;
    if (!matching_values_sweep(&program, lanes, group, runs, &fields))
    {
        return 0;
    }
    for (size_t at = 0u; at < chosen.size(); at += 1u)
    {
        std::vector<unsigned long long> &held = (*store)[chosen[at]->target];
        held.assign(count, 0ull);
        for (unsigned long long place = 0ull; place < count; place += 1ull)
        {
            held[place] = (unsigned long long)fields[0][at * count + place];
        }
    }
    return 1;
}

static unsigned long long matching_values_group(unsigned long long size)
{
    unsigned long long group = 1ull;
    while (group < size)
    {
        group <<= 1u;
    }
    return group;
}

// every level of the plan swept in order, each level's sums and runs by their group, each residue released once the
// last sweep that reads it has run
static int matching_values_device(const TermValues *plan, const std::vector<unsigned long long> &primes,
                                  const std::vector<int> &kept, std::vector<std::vector<unsigned long long>> *store,
                                  MatchingValuesRuns *runs)
{
    const unsigned long long count = primes.size();
    const unsigned long long span = matching_values_group(count);
    unsigned int levels = 0u;
    for (const TermValueResidue &residue : plan->residues)
    {
        levels = (residue.level > levels) ? residue.level : levels;
    }
    // the last level that reads each residue
    std::vector<unsigned int> last(plan->residues.size(), 0u);
    for (const TermValueSum &sum : plan->sums)
    {
        for (const TermValueRow &row : sum.rows)
        {
            for (const auto &entry : row.residues)
            {
                const unsigned int level = plan->residues[sum.target].level;
                last[entry.first] = (level > last[entry.first]) ? level : last[entry.first];
            }
        }
    }
    for (const TermValueRun &run : plan->runs)
    {
        const unsigned int level = plan->residues[run.x_target].level;
        for (const TermValueChain &chain : run.chains)
        {
            last[chain.x_residue] = (level > last[chain.x_residue]) ? level : last[chain.x_residue];
            last[chain.y_residue] = (level > last[chain.y_residue]) ? level : last[chain.y_residue];
        }
    }
    store->assign(plan->residues.size(), std::vector<unsigned long long>());
    (*store)[0].assign(count, 1ull);
    int ok = 1;
    for (unsigned int level = 1u; ok && (level <= levels); level += 1u)
    {
        std::map<std::pair<unsigned long long, int>, std::vector<const TermValueRun *>> run_groups;
        for (const TermValueRun &run : plan->runs)
        {
            if (plan->residues[run.x_target].level == level)
            {
                // runs swept together share their group, whether they keep each step, and which coefficients are 0 or
                // 1 in every step: 3 for each coefficient that is neither
                int shape = run.chains[0].history.empty() ? 0 : 1;
                for (unsigned int which = 0u; which < 4u; which += 1u)
                {
                    int seen = -1;
                    for (const TermValueChain &chain : run.chains)
                    {
                        for (const TermValueStep &step : chain.steps)
                        {
                            const long long held = (which == 0u) ? step.a : ((which == 1u) ? step.b : ((which == 2u) ? step.c : step.d));
                            const int code = (held == 0ll) ? 0 : ((held == 1ll) ? 1 : 2);
                            seen = (seen < 0) ? code : ((seen == code) ? seen : 2);
                        }
                    }
                    shape = shape * 4 + ((seen < 0) ? 3 : seen);
                }
                run_groups[std::make_pair(matching_values_group(run.chains.size()), shape)].push_back(&run);
            }
        }
        std::map<unsigned long long, std::vector<const TermValueSum *>> sum_groups;
        for (const TermValueSum &sum : plan->sums)
        {
            if (plan->residues[sum.target].level == level)
            {
                sum_groups[matching_values_group(sum.rows.size())].push_back(&sum);
            }
        }
        for (const auto &entry : run_groups)
        {
            const unsigned long long group = entry.first.first;
            const unsigned long long most = s_matching_values_lanes_most / (count * group);
            for (size_t start = 0u; ok && (start < entry.second.size()); start += (size_t)most)
            {
                const size_t stop = (start + most < entry.second.size()) ? start + (size_t)most : entry.second.size();
                const std::vector<const TermValueRun *> chosen(entry.second.begin() + (long long)start, entry.second.begin() + (long long)stop);
                ok = matching_values_chains(plan, chosen, group, primes, span, store, runs);
            }
        }
        for (const auto &entry : sum_groups)
        {
            const unsigned long long group = entry.first;
            const unsigned long long most = (s_matching_values_lanes_most / (count * group) > 0ull) ? s_matching_values_lanes_most / (count * group) : 1ull;
            for (size_t start = 0u; ok && (start < entry.second.size()); start += (size_t)most)
            {
                const size_t stop = (start + most < entry.second.size()) ? start + (size_t)most : entry.second.size();
                const std::vector<const TermValueSum *> chosen(entry.second.begin() + (long long)start, entry.second.begin() + (long long)stop);
                ok = matching_values_sums(plan, chosen, group, primes, span, store, runs);
            }
        }
        for (size_t residue = 1u; residue < plan->residues.size(); residue += 1u)
        {
            if ((last[residue] == level) && !kept[residue])
            {
                std::vector<unsigned long long>().swap((*store)[residue]);
            }
        }
        printf("  level %u of %u swept\n", level, levels);
        fflush(stdout);
    }
    return ok;
}

// the program's first run, at the default width: the plan, every sweep on the device, and the residues written
static int matching_values_write(int count, char **arguments, SimResults *results)
{
    static RunCfg cfg;
    MatchingRequest *const request = new MatchingRequest();
    CoreSeries *const series = new CoreSeries();
    std::vector<SimRational> etas;
    std::vector<unsigned long long> lengths;
    std::vector<SimRational> kummer_checks;
    SimRational agreement;
    unsigned long long places = 0ull;
    int read = run_cfg_open(arguments[1], &cfg, &results->line) && matching_read(&cfg, request, series) &&
               run_cfg_rationals(&cfg, "etas", &etas) && !etas.empty() && run_cfg_counts(&cfg, "lengths", &lengths) &&
               (lengths.size() >= 2u) && run_cfg_rationals(&cfg, "kummer_checks", &kummer_checks) &&
               run_cfg_rational(&cfg, "agreement", &agreement) && (sim_rational_sign(agreement) > 0) &&
               run_cfg_count(&cfg, "report.places", &places) && (places <= 18ull);
    for (size_t index = 0u; read && (index < etas.size()); index += 1u)
    {
        read = sim_rational_sign(sim_rational_difference(sim_rational(1ll, 1ll), sim_rational_product(etas[index], etas[index]))) > 0;
    }
    for (size_t index = 0u; read && (index < lengths.size()); index += 1u)
    {
        read = (lengths[index] >= 2ull) && (lengths[index] <= 256ull);
    }
    for (size_t index = 0u; read && (index < kummer_checks.size()); index += 1u)
    {
        const SimRational offset = sim_rational_absolute(sim_rational_difference(kummer_checks[index], sim_rational(1ll, 1ll)));
        read = sim_rational_sign(sim_rational_difference(offset, sim_rational(1ll, 1ll))) < 0;
    }
    if (!read)
    {
        run_cfg_missing(&results->line, "core anisotropy, axis swirl, axial and pressure, core order, join amplitude, inner "
                                        "and outer, content swirl and axial each as eta_modes and weights, blend cuts, terms "
                                        "above 2 and orders, etas inside (-1, 1), two or more lengths from 2 to 256, "
                                        "kummer_checks within 1 of 1, agreement above 0, report places");
        sim_flush(results);
        fprintf(stderr, "matching_values <cfg>\n");
        delete request;
        delete series;
        return 2;
    }
    const SimRational h = request->h;
    TermBook *const book = new TermBook();
    std::vector<MatchingFunctions> functions(etas.size());
    for (size_t index = 0u; index < etas.size(); index += 1u)
    {
        matching_at(request, etas[index], book, &functions[index]);
    }
    scriptura_text(&results->line, "  ");
    scriptura_decimal(&results->line, book->names.size(), 1u);
    scriptura_text(&results->line, " terms in the forms at ");
    scriptura_decimal(&results->line, etas.size(), 1u);
    scriptura_text(&results->line, " etas\n");
    sim_flush(results);

    // the plan: every length's terms, its functions and the numbers reported beside them
    TermValues *const plan = new TermValues();
    term_value_begin(plan);
    std::vector<MatchingValuesOutput> outputs;
    int named = 1;
    for (size_t step = 0u; step < lengths.size(); step += 1u)
    {
        term_value_open(plan, h, (unsigned int)lengths[step]);
        const std::string at = "length_" + std::to_string(lengths[step]);
        std::vector<unsigned int> numerators(book->names.size(), 0u);
        std::vector<unsigned int> divisors(book->names.size(), 0u);
        std::vector<int> zero(book->names.size(), 0);
        for (size_t term = 0u; term < book->names.size(); term += 1u)
        {
            TermValue value;
            if (!term_value_named(plan, book->names[term], &value))
            {
                named = 0;
                scriptura_text(&results->line, "  no value for the term ");
                scriptura_text(&results->line, book->names[term].c_str());
                scriptura_character(&results->line, '\n');
            }
            zero[term] = !term_value_parts(plan, value, &numerators[term], &divisors[term]);
            MatchingValuesOutput output = {at + "_term_" + std::to_string(term), numerators[term], divisors[term], zero[term]};
            outputs.push_back(output);
        }
        const TermValue reported[4] = {plan->e, plan->gamma_once, plan->value_at_one, plan->slope_at_one};
        static const char *const reported_names[4] = {"e", "gamma", "value", "slope"};
        for (size_t which = 0u; which < 4u; which += 1u)
        {
            MatchingValuesOutput output = {at + "_" + reported_names[which], 0u, 0u, 0};
            output.zero = !term_value_parts(plan, reported[which], &output.numerator, &output.divisor);
            outputs.push_back(output);
        }
        for (size_t index = 0u; index < etas.size(); index += 1u)
        {
            const TermForm *const six[6] = {&functions[index].torque, &functions[index].force, &functions[index].m_inf,
                                            &functions[index].j_inf, &functions[index].s_inf, &functions[index].h_match};
            for (size_t which = 0u; which < 6u; which += 1u)
            {
                const TermValue value = term_value_form(plan, *six[which], numerators, divisors, zero);
                MatchingValuesOutput output = {at + "_eta_" + term_book_rational(etas[index]) + "_" + s_matching_values_names[which],
                                               0u, 0u, 0};
                output.zero = !term_value_parts(plan, value, &output.numerator, &output.divisor);
                outputs.push_back(output);
            }
        }
        if (step + 1u == lengths.size())
        {
            for (const SimRational &z : kummer_checks)
            {
                TermValue value[4];
                term_value_kummer(plan, z, &value[0], &value[1]);
                term_value_integral(plan, z, &value[2], &value[3]);
                static const char *const route_names[4] = {"series_value", "series_slope", "integral_value", "integral_slope"};
                for (size_t which = 0u; which < 4u; which += 1u)
                {
                    MatchingValuesOutput output = {at + "_z_" + term_book_rational(z) + "_" + route_names[which], 0u, 0u, 0};
                    output.zero = !term_value_parts(plan, value[which], &output.numerator, &output.divisor);
                    outputs.push_back(output);
                }
            }
        }
    }
    delete request;
    delete series;
    unsigned long long widest = 0ull;
    std::vector<int> kept(plan->residues.size(), 0);
    for (const MatchingValuesOutput &output : outputs)
    {
        if (output.zero)
        {
            continue;
        }
        kept[output.numerator] = 1;
        kept[output.divisor] = 1;
        widest = (plan->residues[output.numerator].bits > widest) ? plan->residues[output.numerator].bits : widest;
        widest = (plan->residues[output.divisor].bits > widest) ? plan->residues[output.divisor].bits : widest;
    }
    unsigned long long rows = 0ull;
    unsigned long long chains = 0ull;
    for (const TermValueSum &sum : plan->sums)
    {
        rows += sum.rows.size();
    }
    for (const TermValueRun &run : plan->runs)
    {
        chains += run.chains.size();
    }
    // each prime above 2^30 adds 30 bits or more; the sign takes one more, and the spare primes stand past the bound
    const unsigned long long prime_count = (widest + 1ull + 29ull) / 30ull + s_matching_values_spare;
    const std::vector<unsigned long long> primes = matching_values_primes(prime_count);
    printf("  %zu residues, %zu sums of %llu rows, %zu runs of %llu chains; the widest value %llu bits, %llu primes\n",
           plan->residues.size(), plan->sums.size(), rows, plan->runs.size(), chains, widest, prime_count);
    fflush(stdout);
    sim_check(results, named, "every term valued");
    sim_check(results, !term_value_short(), "every value written as sums and chains");

    for (MatchingFunctions &held : functions)
    {
        held = MatchingFunctions();
    }
    // the device's part: the most lanes a sweep holds, each output record two limbs, and its tables
    const unsigned long long declared = s_matching_values_lanes_most * 2ull * sizeof(unsigned int) + (256ull << 20u);
    const int submitted = sim_job_submit(results, "matching_values", count, arguments, declared);
    sim_check(results, submitted, "the device job submitted");
    std::vector<std::vector<unsigned long long>> store;
    MatchingValuesRuns runs;
    runs.sweeps = 0ull;
    runs.checked = 0ull;
    runs.agreed = 0ull;
    runs.steps_most = 0ull;
    runs.lanes = 0ull;
    runs.compiled = 1;
    runs.ok = 1;
    const int swept = submitted && matching_values_device(plan, primes, kept, &store, &runs);
    printf("  %llu sweeps over %llu lanes, the longest program %llu steps, %s; the host checked %llu lanes, %llu agree\n",
           runs.sweeps, runs.lanes, runs.steps_most, runs.compiled ? "every program compiled" : "a program interpreted",
           runs.checked, runs.agreed);
    sim_check(results, swept, "every sweep run on the device");
    sim_check(results, swept && (runs.checked == runs.agreed), "the host's records equal the device's");

    // the residues beside the record, with the width that holds the primes' product and a sign, and at least the width
    // a product of two values in lowest terms takes
    unsigned long long limbs = 1024ull;
    while (limbs < prime_count + 4ull)
    {
        limbs <<= 1u;
    }
    const std::string path = std::string(arguments[2]);
    FILE *const out = swept ? fopen(path.c_str(), "w") : NULL;
    if (out != NULL)
    {
        fprintf(out, "limbs %llu\nprimes %zu\n", limbs, primes.size());
        for (unsigned long long prime : primes)
        {
            fprintf(out, "%llu\n", prime);
        }
        fprintf(out, "values %zu\n", outputs.size());
        for (const MatchingValuesOutput &output : outputs)
        {
            fprintf(out, "%s\n%d %llu %llu\n", output.name.c_str(), output.zero,
                    output.zero ? 0ull : plan->residues[output.numerator].bits, output.zero ? 0ull : plan->residues[output.divisor].bits);
            for (unsigned int part = 0u; !output.zero && (part < 2u); part += 1u)
            {
                const std::vector<unsigned long long> &held = store[(part == 0u) ? output.numerator : output.divisor];
                for (size_t place = 0u; place < primes.size(); place += 1u)
                {
                    fprintf(out, "%llu%c", held[place] % primes[place], (place + 1u == primes.size()) ? '\n' : ' ');
                }
            }
        }
    }
    const int written = (out != NULL) && (fclose(out) == 0);
    sim_check(results, written, "every value's residues written");
    delete plan;
    delete book;
    return -1;
}

// the inverse of `value` modulo the prime, value not a multiple of it
static unsigned long long matching_values_inverse(unsigned long long value, unsigned long long prime)
{
    long long old_r = (long long)(value % prime);
    long long r = (long long)prime;
    long long old_s = 1ll;
    long long s = 0ll;
    while (r != 0ll)
    {
        const long long quotient = old_r / r;
        const long long next_r = old_r - quotient * r;
        old_r = r;
        r = next_r;
        const long long next_s = old_s - quotient * s;
        old_s = s;
        s = next_s;
    }
    return (unsigned long long)((old_s % (long long)prime + (long long)prime) % (long long)prime);
}

// the integer whose residues over the primes are `residues`, read signed: X with |X| below half the primes' product
// `product`. Its digits over the primes in turn, X = a_0 + p_0 (a_1 + p_1 (a_2 + ...)), are found modulo each prime
// in turn, a_i = (r_i - (a_0 + a_1 p_0 + ... + a_(i-1) p_0 ... p_(i-2))) / (p_0 ... p_(i-1)) modulo p_i, each product of
// two below 2^62; then X is built from the top digit down.
static AnchorExactInteger matching_values_whole(const std::vector<unsigned long long> &residues,
                                                const std::vector<unsigned long long> &primes, const AnchorExactInteger &product)
{
    const size_t count = primes.size();
    std::vector<unsigned long long> digits(count, 0ull);
    for (size_t index = 0u; index < count; index += 1u)
    {
        const unsigned long long prime = primes[index];
        unsigned long long held = 0ull;
        unsigned long long scale = 1ull;
        for (size_t below = index; below > 0u; below -= 1u)
        {
            held = (held * (primes[below - 1u] % prime) + digits[below - 1u]) % prime;
        }
        for (size_t below = 0u; below < index; below += 1u)
        {
            scale = (scale * (primes[below] % prime)) % prime;
        }
        const unsigned long long gap = (residues[index] % prime + prime - held) % prime;
        digits[index] = (gap * matching_values_inverse(scale, prime)) % prime;
    }
    AnchorExactInteger whole;
    sim_exact_unsigned(&whole, 0ull);
    for (size_t index = count; index > 0u; index -= 1u)
    {
        AnchorExactInteger scaled;
        AnchorExactInteger digit;
        AnchorExactInteger sum;
        sim_exact_unsigned(&digit, digits[index - 1u]);
        if (((index < count) && !sim_exact_scaled(&whole, primes[index - 1u], &scaled)) ||
            !sim_exact_sum((index < count) ? &scaled : &whole, &digit, &sum))
        {
            s_sim_rational_wide = 1;
            return whole;
        }
        whole = sum;
    }
    // above half the product, the integer is negative
    AnchorExactInteger twice;
    if (!sim_exact_scaled(&whole, 2ull, &twice))
    {
        s_sim_rational_wide = 1;
        return whole;
    }
    if (anchor_exact_compare(&twice, &product) > 0)
    {
        AnchorExactInteger negative;
        sim_exact_less(&whole, &product, &negative);
        whole = negative;
    }
    return whole;
}

// 1 where |left - right| <= bound
static int matching_values_near(SimRational left, SimRational right, SimRational bound)
{
    return sim_rational_sign(sim_rational_difference(sim_rational_absolute(sim_rational_difference(left, right)), bound)) <= 0;
}

// a value read back: numerator and divisor whole, then in lowest terms with the divisor positive
typedef struct
{
    std::string name;
    int zero;
    int within;
    SimRational value;
} MatchingValuesValue;

// the program's second run, at the width the first wrote: every value read back whole, reported and recorded
static int matching_values_read(char **arguments, SimResults *results)
{
    static RunCfg cfg;
    std::vector<SimRational> etas;
    std::vector<unsigned long long> lengths;
    std::vector<SimRational> kummer_checks;
    SimRational agreement;
    unsigned long long places = 0ull;
    const int read = run_cfg_open(arguments[1], &cfg, &results->line) && run_cfg_rationals(&cfg, "etas", &etas) &&
                     run_cfg_counts(&cfg, "lengths", &lengths) && (lengths.size() >= 2u) &&
                     run_cfg_rationals(&cfg, "kummer_checks", &kummer_checks) && run_cfg_rational(&cfg, "agreement", &agreement) &&
                     run_cfg_count(&cfg, "report.places", &places);
    FILE *const in = read ? fopen(arguments[2], "r") : NULL;
    unsigned long long limbs = 0ull;
    size_t prime_count = 0u;
    int ok = (in != NULL) && (fscanf(in, " limbs %llu primes %zu", &limbs, &prime_count) == 2);
    std::vector<unsigned long long> primes(prime_count, 0ull);
    for (size_t index = 0u; ok && (index < prime_count); index += 1u)
    {
        ok = fscanf(in, " %llu", &primes[index]) == 1;
    }
    AnchorExactInteger product;
    sim_exact_unsigned(&product, 1ull);
    for (size_t index = 0u; ok && (index < prime_count); index += 1u)
    {
        AnchorExactInteger next;
        ok = sim_exact_scaled(&product, primes[index], &next);
        product = next;
    }
    size_t value_count = 0u;
    ok = ok && (fscanf(in, " values %zu", &value_count) == 1);
    std::vector<MatchingValuesValue> values(ok ? value_count : 0u);
    int within = 1;
    for (size_t index = 0u; ok && (index < value_count); index += 1u)
    {
        char name[512];
        unsigned long long bits[2] = {0ull, 0ull};
        ok = (fscanf(in, " %511s %d %llu %llu", name, &values[index].zero, &bits[0], &bits[1]) == 4);
        values[index].name = name;
        values[index].within = 1;
        values[index].value = sim_rational(0ll, 1ll);
        if (!ok || values[index].zero)
        {
            continue;
        }
        AnchorExactInteger parts[2];
        for (unsigned int part = 0u; ok && (part < 2u); part += 1u)
        {
            std::vector<unsigned long long> residues(prime_count, 0ull);
            for (size_t place = 0u; ok && (place < prime_count); place += 1u)
            {
                ok = fscanf(in, " %llu", &residues[place]) == 1;
            }
            parts[part] = matching_values_whole(residues, primes, product);
            values[index].within = values[index].within && (sim_exact_bits(&parts[part]) <= bits[part]);
        }
        within = within && values[index].within;
        // lowest terms, the divisor positive
        AnchorExactInteger common;
        AnchorExactInteger quotient;
        AnchorExactInteger rest;
        SimRational value;
        ok = ok && (anchor_exact_gcd(&parts[0], &parts[1], &common) == ANCHOR_EXACT_OK) &&
             (anchor_exact_divide(&parts[0], &common, &value.numerator, &rest) == ANCHOR_EXACT_OK) &&
             (anchor_exact_divide(&parts[1], &common, &value.denominator, &quotient) == ANCHOR_EXACT_OK);
        if (ok && (value.denominator.sign < 0))
        {
            value.denominator.sign = 1;
            value.numerator.sign = -value.numerator.sign;
        }
        values[index].value = value;
    }
    if (in != NULL)
    {
        fclose(in);
    }
    sim_check(results, ok, "every value's residues read");
    sim_check(results, within, "every value read back within its bound");
    std::map<std::string, SimRational> by_name;
    for (const MatchingValuesValue &value : values)
    {
        by_name[value.name] = value.value;
    }
    FILE *const record = ok ? record_open(arguments[1], &cfg, "report.record") : NULL;
    static TermBook book;
    for (size_t step = 0u; ok && (step < lengths.size()); step += 1u)
    {
        const std::string at = "length_" + std::to_string(lengths[step]);
        scriptura_text(&results->line, "  length ");
        scriptura_decimal(&results->line, lengths[step], 1u);
        scriptura_text(&results->line, ": e ");
        report_value(&results->line, by_name[at + "_e"], (unsigned int)places);
        scriptura_text(&results->line, ", Gamma(1 + h) ");
        report_value(&results->line, by_name[at + "_gamma"], (unsigned int)places);
        scriptura_text(&results->line, ", w(1) ");
        report_value(&results->line, by_name[at + "_value"], (unsigned int)places);
        scriptura_text(&results->line, ", w'(1) ");
        report_value(&results->line, by_name[at + "_slope"], (unsigned int)places);
        scriptura_character(&results->line, '\n');
        for (const MatchingValuesValue &value : values)
        {
            if ((record != NULL) && (value.name.compare(0u, at.size() + 6u, at + "_term_") == 0))
            {
                record_form(record, value.name.c_str(), term_form_rational(value.value), &book);
            }
        }
        for (size_t index = 0u; index < etas.size(); index += 1u)
        {
            scriptura_text(&results->line, "    eta = ");
            report_value(&results->line, etas[index], (unsigned int)places);
            scriptura_character(&results->line, ':');
            for (size_t which = 0u; which < 6u; which += 1u)
            {
                const std::string name = at + "_eta_" + term_book_rational(etas[index]) + "_" + s_matching_values_names[which];
                scriptura_character(&results->line, ' ');
                scriptura_text(&results->line, s_matching_values_names[which]);
                scriptura_character(&results->line, ' ');
                report_value(&results->line, by_name[name], (unsigned int)places);
                if (record != NULL)
                {
                    record_form(record, name.c_str(), term_form_rational(by_name[name]), &book);
                }
            }
            scriptura_character(&results->line, '\n');
        }
        sim_flush(results);
    }
    // the two routes to w at the last length
    int kummer = ok;
    const std::string last_at = "length_" + std::to_string(lengths.back());
    for (const SimRational &z : kummer_checks)
    {
        const std::string at = last_at + "_z_" + term_book_rational(z) + "_";
        const SimRational series_value = by_name[at + "series_value"];
        const SimRational series_slope = by_name[at + "series_slope"];
        const SimRational integral_value = by_name[at + "integral_value"];
        const SimRational integral_slope = by_name[at + "integral_slope"];
        kummer = kummer && matching_values_near(series_value, integral_value, agreement) &&
                 matching_values_near(series_slope, integral_slope, agreement);
        scriptura_text(&results->line, "    z = ");
        report_value(&results->line, z, (unsigned int)places);
        scriptura_text(&results->line, ": w by its series ");
        report_value(&results->line, series_value, (unsigned int)places);
        scriptura_text(&results->line, ", by its integral ");
        report_value(&results->line, integral_value, (unsigned int)places);
        scriptura_text(&results->line, "; w' ");
        report_value(&results->line, series_slope, (unsigned int)places);
        scriptura_text(&results->line, ", ");
        report_value(&results->line, integral_slope, (unsigned int)places);
        scriptura_character(&results->line, '\n');
    }
    scriptura_text(&results->line, kummer ? "  w and w' by their series about 1 and by their integral agree within the cfg's agreement\n"
                                          : "  w or w' by its series about 1 and by its integral differ past the cfg's agreement\n");
    sim_check(results, kummer, "two routes to w");
    // the last two lengths, each change set against the agreement alone
    int settled = ok;
    const std::string before_at = "length_" + std::to_string(lengths[lengths.size() - 2u]);
    for (size_t index = 0u; index < etas.size(); index += 1u)
    {
        scriptura_text(&results->line, "  the change between the last two lengths at eta = ");
        report_value(&results->line, etas[index], (unsigned int)places);
        scriptura_character(&results->line, ':');
        for (size_t which = 0u; which < 6u; which += 1u)
        {
            const std::string tail = "_eta_" + term_book_rational(etas[index]) + "_" + s_matching_values_names[which];
            const SimRational change = sim_rational_absolute(sim_rational_difference(by_name[last_at + tail], by_name[before_at + tail]));
            settled = settled && (sim_rational_sign(sim_rational_difference(change, agreement)) <= 0);
            scriptura_character(&results->line, ' ');
            scriptura_text(&results->line, s_matching_values_names[which]);
            scriptura_character(&results->line, ' ');
            report_value(&results->line, change, 2u);
        }
        scriptura_character(&results->line, '\n');
    }
    sim_check(results, settled, "the last two lengths agree");
    const int recorded = (record != NULL) && record_close(record);
    const int held = (s_sim_rational_wide == 0) && !report_short() && !record_short() && !run_cfg_short();
    scriptura_text(&results->line, held ? "  every exact value is held in the build's width\n"
                                        : "  a value outgrew the build's width\n");
    sim_check(results, held, "every exact value held");
    scriptura_text(&results->line, recorded ? "  every value is written whole to the cfg's record\n"
                                            : "  the record is not written: the cfg names none, or it does not open\n");
    sim_check(results, recorded, "record written");
    return -1;
}

int main(int count, char **arguments)
{
    char capacity[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, capacity);
    if ((count != 3) && (count != 4))
    {
        fprintf(stderr, "matching_values <cfg> <residues> [read]\n");
        return 2;
    }
    const int status = (count == 3) ? matching_values_write(count, arguments, &results) : matching_values_read(arguments, &results);
    if (status >= 0)
    {
        return status;
    }
    return sim_close(&results, "matching values");
}

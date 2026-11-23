// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// chaitin_omega_mass.cu: mass, simple types and the host's workers
#include "chaitin_omega_internal.h"
#include "embed_atomic.h"
#include "memoria_operor/memoria_operor.h"

// an independent parse of a code of exactly `length` bits: 1 where it is one closed term that takes every bit
int omega_parse(unsigned long long code, unsigned int length, unsigned int *at, unsigned int depth)
{
    if ((*at + 2u) > length)
    {
        return 0;
    }
    const unsigned int first = (unsigned int)((code >> (length - 1u - *at)) & 1ULL);
    const unsigned int second = (unsigned int)((code >> (length - 2u - *at)) & 1ULL);
    if (first == 0u)
    {
        *at += 2u;
        if (second == 0u)
        {
            return omega_parse(code, length, at, depth + 1u);
        }
        return omega_parse(code, length, at, depth) && omega_parse(code, length, at, depth);
    }
    unsigned int index = 0u;
    while ((*at < length) && (((code >> (length - 1u - *at)) & 1ULL) == 1ULL))
    {
        index += 1u;
        *at += 1u;
    }
    if (*at >= length)
    {
        return 0;
    }
    *at += 1u;
    return (index <= depth) ? 1 : 0;
}

// the high and low words of a 64 by 64 bit product, from 32 bit halves
static void omega_wide_product(unsigned long long left, unsigned long long right, unsigned long long *high,
                               unsigned long long *low)
{
    const unsigned long long left_low = left & 0xFFFFFFFFULL;
    const unsigned long long left_high = left >> 32u;
    const unsigned long long right_low = right & 0xFFFFFFFFULL;
    const unsigned long long right_high = right >> 32u;
    const unsigned long long low_low = left_low * right_low;
    const unsigned long long cross_one = left_high * right_low;
    const unsigned long long cross_two = left_low * right_high;
    const unsigned long long high_high = left_high * right_high;
    const unsigned long long middle = (low_low >> 32u) + (cross_one & 0xFFFFFFFFULL) + (cross_two & 0xFFFFFFFFULL);
    *low = (low_low & 0xFFFFFFFFULL) | (middle << 32u);
    *high = high_high + (cross_one >> 32u) + (cross_two >> 32u) + (middle >> 32u);
}

// a product of two fixed point masses, rounded down or up
static unsigned long long omega_mass_product(unsigned long long left, unsigned long long right, int up)
{
    unsigned long long high = 0ULL;
    unsigned long long low = 0ULL;
    omega_wide_product(left, right, &high, &low);
    const unsigned long long scaled = (high << (64u - OMEGA_POINT)) | (low >> OMEGA_POINT);
    const unsigned long long dropped = low & (OMEGA_ONE - 1ULL);
    return scaled + (((up != 0) && (dropped != 0ULL)) ? 1ULL : 0ULL);
}

// 2^-length in fixed point, rounded down or up
static unsigned long long omega_mass_of(unsigned int length, int up)
{
    if (length <= OMEGA_POINT)
    {
        return OMEGA_ONE >> length;
    }
    return (up != 0) ? 1ULL : 0ULL;
}

// a(n), the mass of every n-bit term code, closed or not: a variable, a lambda, or an application
void omega_all_mass(OmegaMass *all, int up)
{
    unsigned long long *const mass = all->value;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = mass, .bytes = sizeof(all->value), .val = 0u);
    for (unsigned int length = 2u; length <= OMEGA_COUNTED; length += 1u)
    {
        unsigned long long pairs = 0ULL;
        for (unsigned int left = 2u; (left + 4u) <= length; left += 1u)
        {
            pairs += omega_mass_product(mass[left], mass[length - 2u - left], up);
        }
        const unsigned long long quarter =
            (up != 0) ? ((mass[length - 2u] + pairs + 3ULL) / 4ULL) : ((mass[length - 2u] + pairs) / 4ULL);
        mass[length] = omega_mass_of(length, up) + quarter;
    }
}

// A table of masses by length and depth, the depths of each length below it: length n holds n + 1 depths from
// n (n + 1) / 2. It is one cell of the temporary tier at the count of every length's depths
typedef struct
{
    unsigned long long *value;
} OmegaUnder;

static size_t omega_under_at(unsigned int length, unsigned int depth)
{
    return (((size_t)length * ((size_t)length + 1u)) / 2u) + depth;
}

static void omega_under_taken(OmegaUnder *under)
{
    const size_t count = omega_under_at(OMEGA_COUNTED + 1u, 0u);
    under->value = (unsigned long long *)omega_taken(count, sizeof(unsigned long long), 1);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = under->value, .bytes = count * sizeof(unsigned long long), .val = 0u);
}

// the mass closed under `depth` at `length`; k >= n - 1 reads the mass of every code of the length
static unsigned long long omega_under_read(const OmegaUnder *under, const unsigned long long *all, unsigned int length,
                                           unsigned int depth)
{
    if (length < 2u)
    {
        return 0ULL;
    }
    return ((depth + 1u) >= length) ? all[length] : under->value[omega_under_at(length, depth)];
}

// c(n, 0) rounded up, the mass of the n-bit closed codes, from c(n, k) closed under k lambdas. Past k = n - 1 every
// variable fits, and c is a(n)
void omega_closed_mass(const OmegaMass *all_up, OmegaMass *closed)
{
    OmegaUnder under;
    omega_under_taken(&under);
    const unsigned long long *const all = all_up->value;
    for (unsigned int length = 2u; length <= OMEGA_COUNTED; length += 1u)
    {
        for (unsigned int depth = 0u; (depth + 1u) < length; depth += 1u)
        {
            unsigned long long pairs = 0ULL;
            for (unsigned int left = 2u; (left + 4u) <= length; left += 1u)
            {
                pairs += omega_mass_product(omega_under_read(&under, all, left, depth),
                                            omega_under_read(&under, all, length - 2u - left, depth), 1);
            }
            under.value[omega_under_at(length, depth)] =
                ((omega_under_read(&under, all, length - 2u, depth + 1u) + pairs + 3ULL) / 4ULL);
        }
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = closed->value, .bytes = sizeof(closed->value), .val = 0u);
    for (unsigned int length = 2u; length <= OMEGA_COUNTED; length += 1u)
    {
        closed->value[length] = omega_under_read(&under, all, length, 0u);
    }
    omega_given(under.value);
}

// the mass of the n-bit closed terms already in normal form, rounded down: such a term halts without a step.
// Every one past L adds to the halted mass uncounted by any run. A normal form is a lambda over a normal form, or a
// neutral term. A neutral term is a variable, or a neutral term applied to a normal form.
void omega_normal_mass(OmegaMass *normal_closed)
{
    // the unconstrained pair first: every variable fits once k reaches n - 1
    static OmegaMass s_normal_all;
    static OmegaMass s_neutral_all;
    unsigned long long *const normal_all = s_normal_all.value;
    unsigned long long *const neutral_all = s_neutral_all.value;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = normal_all, .bytes = sizeof(s_normal_all.value), .val = 0u);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = neutral_all, .bytes = sizeof(s_neutral_all.value), .val = 0u);
    for (unsigned int length = 2u; length <= OMEGA_COUNTED; length += 1u)
    {
        unsigned long long pairs = 0ULL;
        for (unsigned int left = 2u; (left + 4u) <= length; left += 1u)
        {
            pairs += omega_mass_product(neutral_all[left], normal_all[length - 2u - left], 0);
        }
        neutral_all[length] = omega_mass_of(length, 0) + (pairs / 4ULL);
        normal_all[length] = neutral_all[length] + (normal_all[length - 2u] / 4ULL);
    }
    OmegaUnder normal;
    OmegaUnder neutral;
    omega_under_taken(&normal);
    omega_under_taken(&neutral);
    for (unsigned int length = 2u; length <= OMEGA_COUNTED; length += 1u)
    {
        for (unsigned int depth = 0u; (depth + 1u) < length; depth += 1u)
        {
            unsigned long long pairs = 0ULL;
            for (unsigned int left = 2u; (left + 4u) <= length; left += 1u)
            {
                pairs += omega_mass_product(omega_under_read(&neutral, neutral_all, left, depth),
                                            omega_under_read(&normal, normal_all, length - 2u - left, depth), 0);
            }
            const size_t at = omega_under_at(length, depth);
            neutral.value[at] = pairs / 4ULL;
            normal.value[at] =
                neutral.value[at] + (omega_under_read(&normal, normal_all, length - 2u, depth + 1u) / 4ULL);
        }
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = normal_closed->value, .bytes = sizeof(normal_closed->value), .val = 0u);
    for (unsigned int length = 2u; length <= OMEGA_COUNTED; length += 1u)
    {
        normal_closed->value[length] = omega_under_read(&normal, normal_all, length, 0u);
    }
    omega_given(neutral.value);
    omega_given(normal.value);
}

static unsigned int omega_type_new(OmegaTypes *types, int arrow, unsigned int from, unsigned int to)
{
    if (types->count == OMEGA_TYPES_MAX)
    {
        omega_refused("the types of a program pass their table");
    }
    const unsigned int made = types->count;
    types->parent[made] = made;
    types->from[made] = from;
    types->to[made] = to;
    types->arrow[made] = (unsigned char)arrow;
    types->count += 1u;
    return made;
}

static unsigned int omega_type_find(OmegaTypes *types, unsigned int type)
{
    while (types->parent[type] != type)
    {
        types->parent[type] = types->parent[types->parent[type]];
        type = types->parent[type];
    }
    return type;
}

static int omega_type_occurs(OmegaTypes *types, unsigned int variable, unsigned int type)
{
    type = omega_type_find(types, type);
    if (type == variable)
    {
        return 1;
    }
    if (types->arrow[type] == 0u)
    {
        return 0;
    }
    return omega_type_occurs(types, variable, types->from[type]) || omega_type_occurs(types, variable, types->to[type]);
}

static int omega_type_unify(OmegaTypes *types, unsigned int one, unsigned int other)
{
    one = omega_type_find(types, one);
    other = omega_type_find(types, other);
    if (one == other)
    {
        return 1;
    }
    if (types->arrow[one] == 0u)
    {
        if (omega_type_occurs(types, one, other) != 0)
        {
            return 0;
        }
        types->parent[one] = other;
        return 1;
    }
    if (types->arrow[other] == 0u)
    {
        return omega_type_unify(types, other, one);
    }
    const unsigned int one_from = types->from[one];
    const unsigned int one_to = types->to[one];
    const unsigned int other_from = types->from[other];
    const unsigned int other_to = types->to[other];
    return omega_type_unify(types, one_from, other_from) && omega_type_unify(types, one_to, other_to);
}

// the type of the subterm at `at` into `type`, and one past it into `end`; 0 where no simple type exists
static int omega_type_infer(OmegaTypes *types, const OmegaTokens *term, size_t at, unsigned int *type, size_t *end)
{
    const int token = term->token[at];
    if (token == OMEGA_LAMBDA)
    {
        const unsigned int argument = omega_type_new(types, 0, 0u, 0u);
        if (types->bound_count == OMEGA_TYPES_MAX)
        {
            omega_refused("the bound variables of a program pass their table");
        }
        types->bound[types->bound_count] = argument;
        types->bound_count += 1u;
        unsigned int body = 0u;
        const int ok = omega_type_infer(types, term, at + 1u, &body, end);
        types->bound_count -= 1u;
        *type = omega_type_new(types, 1, argument, body);
        return ok;
    }
    if (token == OMEGA_APPLY)
    {
        unsigned int function = 0u;
        unsigned int argument = 0u;
        size_t middle = 0u;
        if ((omega_type_infer(types, term, at + 1u, &function, &middle) == 0) ||
            (omega_type_infer(types, term, middle, &argument, end) == 0))
        {
            return 0;
        }
        const unsigned int result = omega_type_new(types, 0, 0u, 0u);
        *type = result;
        return omega_type_unify(types, function, omega_type_new(types, 1, argument, result));
    }
    *type = types->bound[types->bound_count - (unsigned int)token];
    *end = at + 1u;
    return 1;
}

static int omega_simply_typed(OmegaTypes *types, const OmegaTokens *term)
{
    types->count = 0u;
    types->bound_count = 0u;
    unsigned int type = 0u;
    size_t end = 0u;
    return omega_type_infer(types, term, 0u, &type, &end);
}

void omega_results_open(OmegaResults *results)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = results, .bytes = sizeof(*results), .val = 0);
    for (unsigned int length = 0u; length <= OMEGA_LENGTH_MAX; length += 1u)
    {
        results->steps_champion[length] = ~0ULL;
        results->bits_champion[length] = ~0ULL;
    }
}

// a busy beaver candidate: runs finish out of rank order. A tie retains the lower rank
void omega_champion(unsigned long long *max, unsigned long long *champion, unsigned long long value,
                    unsigned long long index)
{
    if ((value > *max) || ((value == *max) && (index < *champion)))
    {
        *max = value;
        *champion = index;
    }
}

// One term's fate into the results. A run that halted leaves the normal form in the rewrite's term. Any other has the
// type certificate read off the program itself, as the run left the term reduced.
void omega_settle(const OmegaCounts *counts, unsigned int length, unsigned long long index, OmegaFate fate,
                  unsigned int taken, OmegaRewrite *rewrite, OmegaResults *results)
{
    OmegaTokens *const term = &rewrite->term;
    if (fate != OMEGA_HALTS)
    {
        term->count = 0u;
        omega_unrank(counts, length, 0u, index, term);
        const int typed = omega_simply_typed(&rewrite->types, term);
        // a typable term halts. A proven loop or unbounded growth that types is a broken proof
        results->contradictions += ((typed != 0) && ((fate == OMEGA_LOOPS) || (fate == OMEGA_DIVERGES))) ? 1ULL : 0ULL;
        fate = ((typed != 0) && ((fate == OMEGA_OPEN) || (fate == OMEGA_GREW))) ? OMEGA_TYPED : fate;
    }
    results->fate[fate][length] += 1ULL;
    if (fate == OMEGA_HALTS)
    {
        omega_champion(&results->max_steps[length], &results->steps_champion[length], taken, index);
        omega_champion(&results->max_bits[length], &results->bits_champion[length], omega_code_bits(term), index);
    }
}

void omega_merge(OmegaResults *into, const OmegaResults *one)
{
    into->contradictions += one->contradictions;
    for (unsigned int length = 0u; length <= OMEGA_LENGTH_MAX; length += 1u)
    {
        for (unsigned int fate = 0u; fate < 6u; fate += 1u)
        {
            into->fate[fate][length] += one->fate[fate][length];
        }
        if (one->steps_champion[length] != ~0ULL)
        {
            omega_champion(&into->max_steps[length], &into->steps_champion[length], one->max_steps[length],
                           one->steps_champion[length]);
            omega_champion(&into->max_bits[length], &into->bits_champion[length], one->max_bits[length],
                           one->bits_champion[length]);
        }
    }
}

// the workers that the host runs: a worker for each processor, at max OMEGA_WORKERS_MAX
unsigned int omega_workers(void)
{
    const unsigned int processors = platform_processors();
    return (processors < OMEGA_WORKERS_MAX) ? processors : OMEGA_WORKERS_MAX;
}

// the chunks of the terms that the host's workers take in turn: a length and the rank of its first term
typedef struct
{
    unsigned int length;
    unsigned long long from;
} OmegaChunk;

// what one of the host's workers reads and writes: the chunks, the next chunk that the workers share, its tables and
// its tally
typedef struct
{
    const OmegaCounts *counts;
    const OmegaChunk *chunks;
    unsigned long long chunk_count;
    embed_atomic_u64 *next_chunk;
    OmegaRewrite rewrite;
    OmegaResults results;
} OmegaWorker;

static void omega_worker(void *context)
{
    OmegaWorker *const worker = (OmegaWorker *)context;
    const OmegaCounts *const counts = worker->counts;
    omega_results_open(&worker->results);
    for (;;)
    {
        const unsigned long long chunk = (unsigned long long)EMBED_CALL(
            embed_atomic_u64_fetch_add, EmbedAtomicU64FetchAddCfg, .word = worker->next_chunk, .value = 1ULL);
        if (chunk >= worker->chunk_count)
        {
            return;
        }
        const unsigned int length = worker->chunks[chunk].length;
        const unsigned long long from = worker->chunks[chunk].from;
        const unsigned long long total = omega_count(counts, length, 0u);
        const unsigned long long to = ((from + OMEGA_CHUNK) < total) ? (from + OMEGA_CHUNK) : total;
        for (unsigned long long index = from; index < to; index += 1ULL)
        {
            worker->rewrite.term.count = 0u;
            omega_unrank(counts, length, 0u, index, &worker->rewrite.term);
            unsigned int taken = 0u;
            const OmegaFate fate = omega_run(&worker->rewrite, counts->steps, counts->tokens, &taken);
            omega_settle(counts, length, index, fate, taken, &worker->rewrite, &worker->results);
        }
    }
}

// Every closed term through `max` bits run on the host's threads. The call counts the chunks first, then takes one
// cell of the temporary tier at that count. The workers' states are one cell of the temporary tier, which the call
// gives back after its wait
void omega_host(const OmegaCounts *counts, unsigned int max, unsigned int workers, OmegaResults *fates)
{
    unsigned long long chunk_count = 0ULL;
    for (unsigned int length = 2u; length <= max; length += 1u)
    {
        const unsigned long long total = omega_count(counts, length, 0u);
        chunk_count += (total + OMEGA_CHUNK - 1ULL) / OMEGA_CHUNK;
    }
    OmegaChunk *const chunks = (OmegaChunk *)omega_taken(chunk_count, sizeof(OmegaChunk), 1);
    unsigned long long filled = 0ULL;
    for (unsigned int length = 2u; length <= max; length += 1u)
    {
        for (unsigned long long from = 0ULL; from < omega_count(counts, length, 0u); from += OMEGA_CHUNK)
        {
            chunks[filled].length = length;
            chunks[filled].from = from;
            filled += 1ULL;
        }
    }
    embed_atomic_u64 next_chunk;
    EMBED_CALL(embed_atomic_u64_store, EmbedAtomicU64StoreCfg, .word = &next_chunk, .value = 0ULL);
    OmegaWorker *const states = (OmegaWorker *)omega_taken(workers, sizeof(OmegaWorker), 1);
    static PlatformWorker s_threads[OMEGA_WORKERS_MAX];
    const size_t room = omega_rewrite_room(counts->tokens);
    for (unsigned int worker = 0u; worker < workers; worker += 1u)
    {
        states[worker].counts = counts;
        states[worker].chunks = chunks;
        states[worker].chunk_count = chunk_count;
        states[worker].next_chunk = &next_chunk;
        omega_rewrite_taken(&states[worker].rewrite, room, 1);
    }
    for (unsigned int worker = 0u; worker < workers; worker += 1u)
    {
        if (platform_worker_started(&s_threads[worker], omega_worker, &states[worker]) == 0)
        {
            omega_refused("the system refused a worker");
        }
    }
    for (unsigned int worker = 0u; worker < workers; worker += 1u)
    {
        platform_worker_joined(&s_threads[worker]);
    }
    omega_results_open(fates);
    for (unsigned int worker = 0u; worker < workers; worker += 1u)
    {
        omega_merge(fates, &states[worker].results);
    }
    for (unsigned int worker = workers; worker > 0u; worker -= 1u)
    {
        omega_rewrite_given(&states[worker - 1u].rewrite);
    }
    omega_given(states);
    omega_given(chunks);
}

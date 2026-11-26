// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "keymath.h"
#include "keymath_core.h"

#include "../../../../../includes/exact_types/integers/exact_integer.h"
#include "../../../../platform/platform.h"
#include "memoria_operor/memoria_operor.h"

#define KEYMATH_CHECK(condition_, evacaddr_, error_, type_)                                                            \
    engine_error_check((condition_), (type_), ENGINE_MODULE_KEYMATH, (unsigned int)__LINE__,                           \
                       (const void *)(evacaddr_), (error_))

// the owner that the cells of keymath count to
#define KEYMATH_PLACE ((unsigned int)platform_place_g_engine_owner)

// One term of the residual key while the steps build it. It holds its sign, its shift, and a row of weights on each
// axis, each weight an exact integer. The rows are ephemeral, and the encode gives them back before it returns
struct EncodeTerm
{
    unsigned int negative;
    unsigned long long shift;
    PlatformArray rows[ENGINE_AXES];
};

// the weight at `tap` of a row, or NULL past its taps
static OriorExactInteger *encode_weight(const PlatformArray *row, unsigned long long tap)
{
    // Explicit cast reads the element of the row as the exact integer that it holds
    return (OriorExactInteger *)platform_array_at(row, tap);
}

// the limbs of a weight up to its top limb that is not zero, at least one
static unsigned long long encode_limbs_used(const OriorExactInteger *weight)
{
    for (unsigned long long limb = ORIOR_EXACT_LIMBS; limb > 1ULL; limb -= 1ULL)
    {
        if (weight->limb[limb - 1ULL] != 0u)
        {
            return limb;
        }
    }
    return 1ULL;
}

// the bits of a weight up to its top bit that is one, 0 for zero
static unsigned long long encode_bit_length(const OriorExactInteger *weight)
{
    for (unsigned long long limb = ORIOR_EXACT_LIMBS; limb > 0ULL; limb -= 1ULL)
    {
        unsigned int top = weight->limb[limb - 1ULL];
        if (top != 0u)
        {
            unsigned long long bits = (limb - 1ULL) * 32ULL;
            while (top != 0u)
            {
                bits += 1ULL;
                top >>= 1u;
            }
            return bits;
        }
    }
    return 0ULL;
}

// a row of `taps` weights, each zero. 1, or 0 where the pool cannot meet it
static int encode_row_made(PlatformArray *row, unsigned long long taps)
{
    platform_array_begun(row, sizeof(OriorExactInteger), KEYMATH_PLACE, 1);
    return platform_array_counted(row, taps);
}

// the row `from` copied into `into`, a row of its own. 1, or 0 where the pool cannot meet it
static int encode_row_copied(PlatformArray *into, const PlatformArray *from)
{
    if (!encode_row_made(into, from->count))
    {
        return 0;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = into->cell, .src = from->cell,
               .bytes = (size_t)(from->count * from->element));
    return 1;
}

// the term `from` copied into `into`, with rows of its own. 1, or 0 where the pool cannot meet them
static int encode_term_copied(EncodeTerm *into, const EncodeTerm *from)
{
    into->negative = from->negative;
    into->shift = from->shift;
    int ok = 1;
    for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
    {
        platform_array_begun(&into->rows[axis], sizeof(OriorExactInteger), KEYMATH_PLACE, 1);
        ok = ok && encode_row_copied(&into->rows[axis], &from->rows[axis]);
    }
    return ok;
}

// The rows of the first `owned` terms of the terms given back, and then the cell of the terms. The terms past `owned`
// share their rows with other terms, which give them back
static void encode_terms_owned_ended(PlatformArray *terms, unsigned long long owned)
{
    for (unsigned long long at = 0ULL; (at < owned) && (at < terms->count); at += 1ULL)
    {
        // Explicit cast reads the element as the term that it holds
        EncodeTerm *const term = (EncodeTerm *)platform_array_at(terms, at);
        for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
        {
            platform_array_ended(&term->rows[axis]);
        }
    }
    platform_array_ended(terms);
}

// the rows of each term of the terms given back, and then the cell of the terms
static void encode_terms_ended(PlatformArray *terms)
{
    encode_terms_owned_ended(terms, terms->count);
}

// the terms of `from` copied into `into`, with a cell of its own. 1, or 0 where the pool cannot meet them
static int encode_terms_copied(PlatformArray *into, const PlatformArray *from)
{
    platform_array_begun(into, sizeof(EncodeTerm), KEYMATH_PLACE, 1);
    int ok = 1;
    for (unsigned long long at = 0ULL; ok && (at < from->count); at += 1ULL)
    {
        EncodeTerm copy;
        // Explicit cast reads the element as the term that it holds
        ok = encode_term_copied(&copy, (const EncodeTerm *)platform_array_at(from, at)) &&
             platform_array_appended(into, &copy);
    }
    return ok;
}

// The row replaced by `next`: the cell of the old row goes back, and the row takes the cell of `next`
static void encode_row_replaced(PlatformArray *row, PlatformArray *next)
{
    platform_array_ended(row);
    *row = *next;
}

// the row times 1 + z: each tap the sum of itself and the tap before. 1, or 0 where a weight passes the width
static int encode_unit_step(PlatformArray *row)
{
    PlatformArray stepped;
    if (!encode_row_made(&stepped, row->count + 1ULL))
    {
        return 0;
    }
    int ok = 1;
    for (unsigned long long tap = 0ULL; ok && (tap < stepped.count); tap += 1ULL)
    {
        OriorExactInteger *const into = encode_weight(&stepped, tap);
        const OriorExactInteger *const here = encode_weight(row, tap);
        const OriorExactInteger *const before = (tap > 0ULL) ? encode_weight(row, tap - 1ULL) : NULL;
        if (here != NULL)
        {
            ok = orior_exact_add(into, here, into) == ORIOR_EXACT_OK;
        }
        if (ok && (before != NULL))
        {
            ok = orior_exact_add(into, before, into) == ORIOR_EXACT_OK;
        }
    }
    encode_row_replaced(row, &stepped);
    return ok;
}

// the row times 1 + z + ... + z^(length - 1): each tap the sum of the `length` taps ending at it. 1, or 0 where a
// weight passes the width
static int encode_comb(PlatformArray *row, unsigned int length)
{
    PlatformArray summed;
    if (!encode_row_made(&summed, row->count + length - 1u))
    {
        return 0;
    }
    int ok = 1;
    for (unsigned long long tap = 0ULL; ok && (tap < summed.count); tap += 1ULL)
    {
        for (unsigned int back = 0u; ok && (back < length) && (back <= tap); back += 1u)
        {
            const OriorExactInteger *const weight = encode_weight(row, tap - back);
            if (weight != NULL)
            {
                ok =
                    orior_exact_add(encode_weight(&summed, tap), weight, encode_weight(&summed, tap)) == ORIOR_EXACT_OK;
            }
        }
    }
    encode_row_replaced(row, &summed);
    return ok;
}

// the row times 1 + 2 z^spacing + z^(2 spacing): a pair [1, 2, 1] whose taps are `spacing` apart, two taps of the row's
// own spacing and none between. 1, or 0 where a weight passes the width
static int encode_spaced_pair(PlatformArray *row, unsigned long long spacing)
{
    PlatformArray paired;
    if (!encode_row_made(&paired, row->count + (2ULL * spacing)))
    {
        return 0;
    }
    int ok = 1;
    for (unsigned long long tap = 0ULL; ok && (tap < row->count); tap += 1ULL)
    {
        const OriorExactInteger *const weight = encode_weight(row, tap);
        OriorExactInteger *const first = encode_weight(&paired, tap);
        OriorExactInteger *const middle = encode_weight(&paired, tap + spacing);
        OriorExactInteger *const last = encode_weight(&paired, tap + (2ULL * spacing));
        ok = (orior_exact_add(first, weight, first) == ORIOR_EXACT_OK) &&
             (orior_exact_add(middle, weight, middle) == ORIOR_EXACT_OK) &&
             (orior_exact_add(middle, weight, middle) == ORIOR_EXACT_OK) &&
             (orior_exact_add(last, weight, last) == ORIOR_EXACT_OK);
    }
    encode_row_replaced(row, &paired);
    return ok;
}

// the terms of `running` stepped by one smooth, comb or spaced pair. 1, or 0 where a weight passes the width or the
// pool cannot meet a row
static int encode_terms_stepped(PlatformArray *running, const EngineStep *doing)
{
    int ok = 1;
    for (unsigned int axis = 0u; ok && (axis < ENGINE_AXES); axis += 1u)
    {
        for (unsigned long long at = 0ULL; ok && (at < running->count); at += 1ULL)
        {
            // Explicit cast reads the element as the term that it holds
            EncodeTerm *const term = (EncodeTerm *)platform_array_at(running, at);
            if (doing->operation == ENGINE_SMOOTH)
            {
                for (unsigned int unit = 0u; ok && (unit < doing->orders[axis]); unit += 1u)
                {
                    ok = encode_unit_step(&term->rows[axis]);
                }
            }
            else if ((doing->operation == ENGINE_COMB) && (doing->orders[axis] >= 2u))
            {
                ok = encode_comb(&term->rows[axis], doing->orders[axis]);
            }
            else if (doing->operation == ENGINE_SPACED)
            {
                for (unsigned int pair = 0u; ok && (pair < doing->orders[axis]); pair += 1u)
                {
                    ok = encode_spaced_pair(&term->rows[axis], 1ULL << doing->shift);
                }
            }
        }
    }
    return ok;
}

// The key of the terms: each term's sign and shift, and on each axis its taps, its limbs and its growth. The limbs of
// each row go out limb by limb, each limb over every weight of the row. 1, or 0 where the pool cannot meet them
static int encode_key_placed(const PlatformArray *running, EngineKey *key, EngineError *error)
{
    unsigned long long limb_count = 0ULL;
    for (unsigned long long at = 0ULL; at < running->count; at += 1ULL)
    {
        // Explicit cast reads the element as the term that it holds
        const EncodeTerm *const term = (const EncodeTerm *)platform_array_at(running, at);
        for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
        {
            unsigned long long widest = 1ULL;
            for (unsigned long long tap = 0ULL; tap < term->rows[axis].count; tap += 1ULL)
            {
                const unsigned long long used = encode_limbs_used(encode_weight(&term->rows[axis], tap));
                widest = (used > widest) ? used : widest;
            }
            limb_count += widest * term->rows[axis].count;
        }
    }
    // the key is a resource that the caller gives back with keymath_key_release
    key->term = (EngineKeyTerm *)platform_cell_taken((running->count + 1ULL) * sizeof(EngineKeyTerm), KEYMATH_PLACE, 0);
    key->limbs = (unsigned int *)platform_cell_taken((limb_count + 1ULL) * sizeof(unsigned int), KEYMATH_PLACE, 0);
    if (!KEYMATH_CHECK((key->term != NULL) && (key->limbs != NULL), key, error, ENGINE_ERROR_RESOURCE))
    {
        return 0;
    }
    unsigned long long placed_limbs = 0ULL;
    OriorExactInteger one;
    orior_exact_zero(&one);
    one.limb[0] = 1u;
    one.sign = 1;
    for (unsigned long long at = 0ULL; at < running->count; at += 1ULL)
    {
        // Explicit cast reads the element as the term that it holds
        const EncodeTerm *const term = (const EncodeTerm *)platform_array_at(running, at);
        EngineKeyTerm *const out = &key->term[at];
        EMBED_CALL(memor.set, MemoriaCfg, .dst = out, .bytes = sizeof(*out), .val = 0u);
        out->negative = term->negative;
        out->shift = term->shift;
        for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
        {
            const PlatformArray *const row = &term->rows[axis];
            unsigned long long widest = 1ULL;
            OriorExactInteger sum;
            orior_exact_zero(&sum);
            for (unsigned long long tap = 0ULL; tap < row->count; tap += 1ULL)
            {
                const unsigned long long used = encode_limbs_used(encode_weight(row, tap));
                widest = (used > widest) ? used : widest;
                (void)orior_exact_add(&sum, encode_weight(row, tap), &sum);
            }
            // the growth is the bits of the sum less one, and the sum of a row of weights is at least one
            OriorExactInteger less_one;
            (void)orior_exact_subtract(&sum, &one, &less_one);
            EngineKeyRow *const placed = &out->row[axis];
            placed->taps = row->count;
            placed->limbs = widest;
            placed->first = placed_limbs;
            placed->growth_bits = encode_bit_length(&less_one);
            for (unsigned long long limb = 0ULL; limb < widest; limb += 1ULL)
            {
                for (unsigned long long tap = 0ULL; tap < row->count; tap += 1ULL)
                {
                    key->limbs[placed_limbs] = encode_weight(row, tap)->limb[limb];
                    placed_limbs += 1ULL;
                }
            }
        }
    }
    key->terms = (unsigned int)running->count;
    key->limb_count = placed_limbs;
    return 1;
}

extern "C" long keymath_encode(const KeymathEncodeRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return KEYMATH_ERROR;
    }
    EngineError *const error = request->error;
    if (!KEYMATH_CHECK((request->key != NULL) && (request->steps != NULL) && (request->count != 0u), request, error,
                       ENGINE_ERROR_REQUEST))
    {
        return KEYMATH_ERROR;
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = request->key, .bytes = sizeof(*request->key), .val = 0u);
    // the one term of the start: each axis a row of the one weight 1
    PlatformArray running;
    platform_array_begun(&running, sizeof(EncodeTerm), KEYMATH_PLACE, 1);
    EncodeTerm start;
    start.negative = 0u;
    start.shift = 0ULL;
    int ok = 1;
    for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
    {
        ok = encode_row_made(&start.rows[axis], 1ULL) && ok;
        if (ok)
        {
            OriorExactInteger *const weight = encode_weight(&start.rows[axis], 0ULL);
            weight->limb[0] = 1u;
            weight->sign = 1;
        }
    }
    ok = ok && platform_array_appended(&running, &start);
    PlatformArray saved;
    platform_array_begun(&saved, sizeof(EncodeTerm), KEYMATH_PLACE, 1);
    int has_saved = 0;
    // whether the centers of the running terms and of the retained terms sit half a voxel before the voxel on each
    // axis. The window of an order o starts floor((o + 1) / 2) before it, and the center moves half a voxel with each
    // odd order
    unsigned int running_half[ENGINE_AXES] = {0u, 0u, 0u};
    unsigned int saved_half[ENGINE_AXES] = {0u, 0u, 0u};
    for (unsigned int step = 0u; ok && (step < request->count); step += 1u)
    {
        const EngineStep *const doing = &request->steps[step];
        if (doing->operation == ENGINE_SMOOTH)
        {
            for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
            {
                running_half[axis] ^= doing->orders[axis] & 1u;
            }
            ok = KEYMATH_CHECK(encode_terms_stepped(&running, doing), doing, error, ENGINE_ERROR_REQUEST);
        }
        else if (doing->operation == ENGINE_COMB)
        {
            for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
            {
                // a comb of n widens the window by n - 1 taps, and moves the center as an order of n - 1 does
                running_half[axis] ^= (doing->orders[axis] >= 2u) ? ((doing->orders[axis] - 1u) & 1u) : 0u;
            }
            ok = KEYMATH_CHECK(encode_terms_stepped(&running, doing), doing, error, ENGINE_ERROR_REQUEST);
        }
        else if (doing->operation == ENGINE_SPACED)
        {
            // a spaced pair is symmetric about the voxel: it widens the window by 2 · 2^shift taps and moves no center
            ok = KEYMATH_CHECK(doing->shift < ENGINE_SPACINGS, doing, error, ENGINE_ERROR_REQUEST) &&
                 KEYMATH_CHECK(encode_terms_stepped(&running, doing), doing, error, ENGINE_ERROR_REQUEST);
        }
        else if (doing->operation == ENGINE_SAVE)
        {
            encode_terms_ended(&saved);
            ok = KEYMATH_CHECK(encode_terms_copied(&saved, &running), doing, error, ENGINE_ERROR_RESOURCE);
            has_saved = 1;
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = saved_half, .src = running_half, .bytes = sizeof(saved_half));
        }
        else if ((doing->operation == ENGINE_SCALE_SUBTRACT) && (has_saved != 0))
        {
            // terms subtracted half a voxel apart are no residual of one voxel
            PlatformArray next;
            platform_array_begun(&next, sizeof(EncodeTerm), KEYMATH_PLACE, 1);
            ok = KEYMATH_CHECK(EMBED_CALL(memor.cmp, MemoriaCfg, .src = saved_half, .other = running_half,
                                          .bytes = sizeof(saved_half)) == 0,
                               doing, error, ENGINE_ERROR_REQUEST) &&
                 KEYMATH_CHECK(encode_terms_copied(&next, &saved), doing, error, ENGINE_ERROR_RESOURCE);
            // the terms of next that the copy made, each with rows of its own
            const unsigned long long copied = next.count;
            for (unsigned long long at = 0ULL; ok && (at < next.count); at += 1ULL)
            {
                // Explicit cast reads the element as the term that it holds
                ((EncodeTerm *)platform_array_at(&next, at))->shift += (unsigned long long)doing->shift;
            }
            // the running terms move to the end of next, each of the opposite sign, and their rows move with them
            for (unsigned long long at = 0ULL; ok && (at < running.count); at += 1ULL)
            {
                // Explicit cast reads the element as the term that it holds
                EncodeTerm moved = *(EncodeTerm *)platform_array_at(&running, at);
                moved.negative ^= 1u;
                ok = KEYMATH_CHECK(platform_array_appended(&next, &moved), doing, error, ENGINE_ERROR_RESOURCE);
            }
            // Where the subtraction holds, next takes the place of the running terms and their rows. Else the running
            // terms keep their rows, and next gives back the rows of its copied terms and its cell
            if (ok != 0)
            {
                platform_array_ended(&running);
                running = next;
            }
            else
            {
                encode_terms_owned_ended(&next, copied);
            }
        }
        else
        {
            ok = KEYMATH_CHECK(0, doing, error, ENGINE_ERROR_REQUEST);
        }
    }
    ok = ok && encode_key_placed(&running, request->key, error);
    encode_terms_ended(&running);
    encode_terms_ended(&saved);
    if (!ok)
    {
        keymath_key_release(request->key);
        return KEYMATH_ERROR;
    }
    return (long)request->key->terms;
}

extern "C" void keymath_key_release(EngineKey *key)
{
    platform_cell_released(key->term, KEYMATH_PLACE);
    platform_cell_released(key->limbs, KEYMATH_PLACE);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = key, .bytes = sizeof(*key), .val = 0u);
}

// The record encoding. keymath_core_record_encode (keymath_core.h), which the device runs as well, arranges the term,
// the width and the form of each step. The key then comes from the terms, the outputs and the tables
extern "C" long keymath_record_encode(const KeymathRecordRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return KEYMATH_ERROR;
    }
    EngineError *const error = request->error;
    if (!KEYMATH_CHECK((request->key != NULL) && (request->steps != NULL) && (request->count != 0u) &&
                           (request->outputs != NULL) && (request->output_count != 0u) &&
                           (request->output_count <= request->count) && (request->members != 0u) &&
                           (request->members <= ENGINE_RECORD_MEMBERS_MAX),
                       request, error, ENGINE_ERROR_REQUEST))
    {
        return KEYMATH_ERROR;
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = request->key, .bytes = sizeof(*request->key), .val = 0u);
    const unsigned long long count = request->count;
    // the work of the encoding is ephemeral, and the key is a resource that the caller gives back
    EngineRecordTerm *const terms =
        (EngineRecordTerm *)platform_cell_taken(count * sizeof(EngineRecordTerm), KEYMATH_PLACE, 0);
    unsigned char *const never_negative = (unsigned char *)platform_cell_taken(count, KEYMATH_PLACE, 1);
    KeymathCoreAffine *const forms =
        (KeymathCoreAffine *)platform_cell_taken(count * sizeof(KeymathCoreAffine), KEYMATH_PLACE, 1);
    if (!KEYMATH_CHECK((terms != NULL) && (never_negative != NULL) && (forms != NULL), request, error,
                       ENGINE_ERROR_RESOURCE))
    {
        platform_cell_released(forms, KEYMATH_PLACE);
        platform_cell_released(never_negative, KEYMATH_PLACE);
        platform_cell_released(terms, KEYMATH_PLACE);
        return KEYMATH_ERROR;
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = terms, .bytes = (size_t)(count * sizeof(EngineRecordTerm)), .val = 0u);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = never_negative, .bytes = (size_t)count, .val = 0u);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = forms, .bytes = (size_t)(count * sizeof(KeymathCoreAffine)), .val = 0u);
    unsigned int bound[KEYMATH_BOUND_LIMBS];
    KeymathCoreEncode encoding{};
    encoding.steps = request->steps;
    encoding.count = request->count;
    encoding.field_bits = request->field_bits;
    encoding.fields = request->fields;
    encoding.members = request->members;
    encoding.outputs = request->outputs;
    encoding.output_count = request->output_count;
    encoding.tables = request->tables;
    encoding.table_count = request->table_count;
    encoding.terms = terms;
    encoding.never_negative = never_negative;
    encoding.forms = forms;
    encoding.bound = bound;
    // the arena of the encoding, which doubles where the encoding ends full
    PlatformArray arena;
    platform_array_begun(&arena, sizeof(KeymathCoreTerm), KEYMATH_PLACE, 1);
    unsigned long long capacity = count * KEYMATH_ARENA_PER_STEP;
    int ok = 0;
    int room = 1;
    do
    {
        room = platform_array_counted(&arena, capacity);
        encoding.arena.terms = (KeymathCoreTerm *)arena.cell;
        encoding.arena.capacity = capacity;
        ok = room && keymath_core_record_encode(&encoding);
        capacity *= 2ULL;
    } while (room && (ok == 0) && (encoding.end == KEYMATH_CORE_FULL));
    platform_array_ended(&arena);
    platform_cell_released(forms, KEYMATH_PLACE);
    platform_cell_released(never_negative, KEYMATH_PLACE);
    if (ok == 0)
    {
        platform_cell_released(terms, KEYMATH_PLACE);
        if (!room)
        {
            KEYMATH_CHECK(0, request, error, ENGINE_ERROR_RESOURCE);
            return KEYMATH_ERROR;
        }
        // the step, its table or the output the encoding ended at
        const void *const evacaddr =
            (encoding.end == KEYMATH_CORE_OUTPUT)  ? (const void *)&request->outputs[encoding.at]
            : (encoding.end == KEYMATH_CORE_TABLE) ? (const void *)&request->tables[request->steps[encoding.at].right]
                                                   : (const void *)&request->steps[encoding.at];
        KEYMATH_CHECK(0, evacaddr, error, ENGINE_ERROR_REQUEST);
        return KEYMATH_ERROR;
    }
    EngineRecordKey *const key = request->key;
    key->term = terms;
    key->output = (unsigned int *)platform_cell_taken((unsigned long long)request->output_count * sizeof(unsigned int),
                                                      KEYMATH_PLACE, 0);
    if (!KEYMATH_CHECK(key->output != NULL, key, error, ENGINE_ERROR_RESOURCE))
    {
        keymath_record_release(key);
        return KEYMATH_ERROR;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = key->output, .src = request->outputs,
               .bytes = (size_t)request->output_count * sizeof(unsigned int));
    key->steps = request->count;
    key->members = request->members;
    key->outputs = request->output_count;
    key->tables = request->table_count;
    if (request->table_count != 0u)
    {
        // the words of every table, one after the next
        unsigned long long words = 0ULL;
        for (unsigned int table = 0u; table < request->table_count; table += 1u)
        {
            // index_bits is at max 32, and the entry count at max 2^32, which an unsigned long long represents
            words += (1ULL << request->tables[table].index_bits) * ((request->tables[table].out_bits + 31u) / 32u);
        }
        key->table = (EngineRecordTable *)platform_cell_taken(
            (unsigned long long)request->table_count * sizeof(EngineRecordTable), KEYMATH_PLACE, 0);
        key->table_values =
            (unsigned int *)platform_cell_taken((words + 1ULL) * sizeof(unsigned int), KEYMATH_PLACE, 0);
        if (!KEYMATH_CHECK((key->table != NULL) && (key->table_values != NULL), key, error, ENGINE_ERROR_RESOURCE))
        {
            keymath_record_release(key);
            return KEYMATH_ERROR;
        }
        unsigned long long at = 0ULL;
        for (unsigned int table = 0u; table < request->table_count; table += 1u)
        {
            const EngineRecordTable *const source = &request->tables[table];
            const unsigned long long entries = 1ULL << source->index_bits;
            const unsigned int out_limbs = (source->out_bits + 31u) / 32u;
            key->table[table].index_bits = source->index_bits;
            key->table[table].out_bits = source->out_bits;
            key->table[table].values = NULL;
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &key->table_values[at], .src = source->values,
                       .bytes = (size_t)(entries * out_limbs * sizeof(unsigned int)));
            at += entries * out_limbs;
        }
        key->table_word_count = words;
    }
    return (long)key->steps;
}

extern "C" void keymath_record_release(EngineRecordKey *key)
{
    platform_cell_released(key->term, KEYMATH_PLACE);
    platform_cell_released(key->output, KEYMATH_PLACE);
    platform_cell_released(key->table, KEYMATH_PLACE);
    platform_cell_released(key->table_values, KEYMATH_PLACE);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = key, .bytes = sizeof(*key), .val = 0u);
}

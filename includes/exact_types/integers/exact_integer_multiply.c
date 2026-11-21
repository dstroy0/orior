
// exact_integer_multiply.c: the Fermat transform and exact multiplication
#include "exact_integer_internal.h"

static int fermat_multiply(uint32_t *result, const uint32_t *left, const uint32_t *right, size_t limbs);

// the cyclic transform of count = 2^depth ring elements in place, root 2^root_bits. First comes bit-reversed order,
// then butterflies (u, v) -> (u + w v, u - w v) with w a power of the root. Each product by w is a shift
static void fermat_fourier(uint32_t *elements, size_t count, unsigned int depth, size_t limbs, size_t root_bits,
                           uint32_t *scratch, uint32_t *turned)
{
    const size_t width = limbs + 1u;
    const size_t cycle = 2u * limbs * LIMB_BITS;
    for (size_t at = 0u; at < count; at++)
    {
        size_t reversed = 0u;
        for (unsigned int bit = 0u; bit < depth; bit++)
        {
            reversed |= ((at >> bit) & 1u) << (depth - 1u - bit);
        }
        if (reversed > at)
        {
            memor.cpy(&(MemoriaCfg){.dst = turned, .src = &elements[at * width], .bytes = width * sizeof(uint32_t)});
            memor.cpy(&(MemoriaCfg){
                .dst = &elements[at * width], .src = &elements[reversed * width], .bytes = width * sizeof(uint32_t)});
            memor.cpy(
                &(MemoriaCfg){.dst = &elements[reversed * width], .src = turned, .bytes = width * sizeof(uint32_t)});
        }
    }
    for (size_t span = 2u; span <= count; span <<= 1u)
    {
        const size_t half = span / 2u;
        const size_t stage_bits = (root_bits * (count / span)) % cycle;
        for (size_t start = 0u; start < count; start += span)
        {
            for (size_t step = 0u; step < half; step++)
            {
                uint32_t *const upper = &elements[(start + step) * width];
                uint32_t *const lower = &elements[(start + step + half) * width];
                fermat_shift(turned, lower, (step * stage_bits) % cycle, limbs, scratch);
                fermat_subtract(lower, upper, turned, limbs);
                fermat_add(upper, upper, turned, limbs);
            }
        }
    }
}

// the inner ring for pieces of piece_bits in 2^depth parts: at least 2 piece_bits + depth + 2 bits. It is a
// multiple of the part count and of a limb, and a signed coefficient of the negacyclic product fits. A ring past the
// base case also leaves room to split in turn
static size_t fermat_inner_bits(size_t piece_bits, unsigned int depth)
{
    const size_t least = (2u * piece_bits) + depth + 2u;
    const size_t parts = (size_t)1u << depth;
    const size_t tight_step = (parts > (size_t)LIMB_BITS) ? parts : (size_t)LIMB_BITS;
    const size_t tight = ((least + tight_step - 1u) / tight_step) * tight_step;
    if ((tight / LIMB_BITS) <= (size_t)TRANSFORM_BASE_LIMBS)
    {
        return tight;
    }
    size_t step = (size_t)LIMB_BITS << ((bits_ceiling_log(least) + 1u) / 2u);
    step = (step < parts) ? parts : step;
    return ((least + step - 1u) / step) * step;
}

// the modeled word operations of a product on the ladder below the transform: long multiplication's square, and
// Karatsuba's three half products and linear passes
static unsigned long long fermat_ladder_cost(size_t limbs)
{
    if (limbs < (size_t)ORIOR_EXACT_KARATSUBA_LIMBS)
    {
        return (unsigned long long)limbs * limbs;
    }
    return (3ULL * fermat_ladder_cost((limbs + 1u) / 2u)) + (8ULL * limbs);
}

static unsigned long long fermat_cost(size_t limbs, unsigned int *depth);

// the modeled word operations of the transform at a depth. That is three transforms of 2^depth depth / 2 butterflies
// over the words of the inner ring, a few passes each. Then the pointwise products, each at its own best. The code
// never takes a split whose inner ring is no smaller than the ring
static unsigned long long fermat_cost_at(size_t limbs, unsigned int depth)
{
    const size_t parts = (size_t)1u << depth;
    const size_t inner = fermat_inner_bits((limbs / parts) * LIMB_BITS, depth) / LIMB_BITS;
    if (inner >= limbs)
    {
        return ~0ULL;
    }
    unsigned int inner_depth = 0u;
    const unsigned long long point = fermat_cost(inner, &inner_depth);
    return (6ULL * parts * depth * (inner + 1u)) + (4ULL * parts * (inner + 1u)) + (parts * point);
}

// the cheapest way to multiply in the ring of the given limbs: the depth of the transform, or 0 for the ladder below.
// The code tries depths near the balanced split of sqrt(n) pieces, where the inner ring is near sqrt(n) bits. The own
// recursion of the model is thus a few levels deep
static unsigned long long fermat_cost(size_t limbs, unsigned int *depth)
{
    unsigned long long best = fermat_ladder_cost(limbs) + (2ULL * limbs);
    *depth = 0u;
    if (limbs <= (size_t)TRANSFORM_BASE_LIMBS)
    {
        return best;
    }
    const unsigned int balanced = (bits_ceiling_log(limbs * LIMB_BITS) + 1u) / 2u;
    const unsigned int lowest = (balanced > 4u) ? (balanced - 2u) : 2u;
    for (unsigned int trial = lowest; (trial <= (balanced + 1u)) && (((limbs >> trial) << trial) == limbs); trial++)
    {
        const unsigned long long cost = fermat_cost_at(limbs, trial);
        if (cost < best)
        {
            best = cost;
            *depth = trial;
        }
    }
    return best;
}

// the number of pieces that a ring of the given limbs splits into, 2^depth pieces of whole limbs. The cost model
// chooses it. 0 is the ladder below the transform
static unsigned int fermat_depth(size_t limbs)
{
    unsigned int depth = 0u;
    (void)fermat_cost(limbs, &depth);
    return depth;
}

// Schonhage-Strassen modulo 2^n + 1. The code cuts the value into 2^depth pieces. Powers of psi = 2^(n' / 2^depth)
// weight the pieces. The 2^depth-th power of psi is -1 in the inner ring 2^n' + 1. That turns the cyclic transform into
// the negacyclic product that the outer ring needs.
//
// The transforms multiply point by point, by recursion. The inverse transform, the division by 2^depth and the removal
// of the weights are shifts
static int fermat_transform(uint32_t *result, const uint32_t *left, const uint32_t *right, size_t limbs,
                            unsigned int depth)
{
    const size_t count = (size_t)1u << depth;
    const size_t piece = limbs / count;
    const size_t inner_bits = fermat_inner_bits(piece * LIMB_BITS, depth);
    const size_t inner = inner_bits / LIMB_BITS;
    const size_t width = inner + 1u;
    const size_t cycle = 2u * inner_bits;
    const size_t total = limbs + width + 1u;
    uint32_t *const workspace = exact_work_taken((2u * count * width) + (4u * width) + (2u * total) + 2u, 1);
    if (workspace == NULL)
    {
        return 0;
    }
    uint32_t *const first = workspace;
    uint32_t *const second = &workspace[count * width];
    uint32_t *const scratch = &workspace[2u * count * width];
    uint32_t *const turned = &scratch[(2u * inner) + 2u];
    uint32_t *const product = &turned[width];
    uint32_t *const positive = &product[width];
    uint32_t *const negative = &positive[total];
    for (size_t at = 0u; at < count; at++)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = &first[at * width], .src = &left[at * piece], .bytes = piece * sizeof(uint32_t)});
        memor.cpy(
            &(MemoriaCfg){.dst = &second[at * width], .src = &right[at * piece], .bytes = piece * sizeof(uint32_t)});
        fermat_shift(&first[at * width], &first[at * width], at * (inner_bits / count), inner, scratch);
        fermat_shift(&second[at * width], &second[at * width], at * (inner_bits / count), inner, scratch);
    }
    fermat_fourier(first, count, depth, inner, (2u * inner_bits) / count, scratch, turned);
    fermat_fourier(second, count, depth, inner, (2u * inner_bits) / count, scratch, turned);
    for (size_t at = 0u; at < count; at++)
    {
        if (fermat_multiply(product, &first[at * width], &second[at * width], inner) == 0)
        {
            exact_work_released(workspace);
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = &first[at * width], .src = product, .bytes = width * sizeof(uint32_t)});
    }
    fermat_fourier(first, count, depth, inner, cycle - ((2u * inner_bits) / count), scratch, turned);
    for (size_t at = 0u; at < count; at++)
    {
        uint32_t *const coefficient = &first[at * width];
        // divide by 2^depth and unweight by psi^-at in one shift
        const size_t back = ((2u * cycle) - depth - (at * (inner_bits / count))) % cycle;
        fermat_shift(coefficient, coefficient, back, inner, scratch);
        // above half the ring is a negative coefficient
        const int below_zero = (coefficient[inner] != 0u) || ((coefficient[inner - 1u] >> (LIMB_BITS - 1u)) != 0u);
        if (below_zero)
        {
            fermat_negate(coefficient, inner);
        }
        (void)limbs_accumulate(&(below_zero ? negative : positive)[at * piece], total - (at * piece), coefficient,
                               width);
    }
    // each sum folds into the ring, n bits at a time with signs in turn, because 2^n is -1
    uint32_t *const folded = product;
    memor.set(&(MemoriaCfg){.dst = result, .bytes = (limbs + 1u) * sizeof(uint32_t), .val = 0u});
    uint32_t *const chunk = exact_work_taken(limbs + 1u, 1);
    if (chunk == NULL)
    {
        exact_work_released(workspace);
        return 0;
    }
    for (unsigned int sign = 0u; sign < 2u; sign++)
    {
        const uint32_t *const sum = (sign == 0u) ? positive : negative;
        uint32_t *const into = exact_work_taken(limbs + 1u, 1);
        if (into == NULL)
        {
            exact_work_released(chunk);
            exact_work_released(workspace);
            return 0;
        }
        for (size_t start = 0u, index = 0u; start < total; start += limbs, index++)
        {
            const size_t taken = ((total - start) < limbs) ? (total - start) : limbs;
            memor.set(&(MemoriaCfg){.dst = chunk, .bytes = (limbs + 1u) * sizeof(uint32_t), .val = 0u});
            memor.cpy(&(MemoriaCfg){.dst = chunk, .src = &sum[start], .bytes = taken * sizeof(uint32_t)});
            if ((index & 1u) == 0u)
            {
                fermat_add(into, into, chunk, limbs);
            }
            else
            {
                fermat_subtract(into, into, chunk, limbs);
            }
        }
        if (sign == 0u)
        {
            memor.cpy(&(MemoriaCfg){.dst = result, .src = into, .bytes = (limbs + 1u) * sizeof(uint32_t)});
        }
        else
        {
            fermat_subtract(result, result, into, limbs);
        }
        exact_work_released(into);
    }
    (void)folded;
    exact_work_released(chunk);
    exact_work_released(workspace);
    return 1;
}

// result = left . right modulo 2^n + 1; result is distinct from both
static int fermat_multiply(uint32_t *result, const uint32_t *left, const uint32_t *right, size_t limbs)
{
    if ((left[limbs] != 0u) || (right[limbs] != 0u))
    {
        // 2^n is -1
        memor.cpy(&(MemoriaCfg){
            .dst = result, .src = (left[limbs] != 0u) ? right : left, .bytes = (limbs + 1u) * sizeof(uint32_t)});
        if ((left[limbs] != 0u) && (right[limbs] != 0u))
        {
            memor.set(&(MemoriaCfg){.dst = result, .bytes = (limbs + 1u) * sizeof(uint32_t), .val = 0u});
            result[0] = 1u;
            return 1;
        }
        fermat_negate(result, limbs);
        return 1;
    }
    const unsigned int depth = fermat_depth(limbs);
    if (depth < 2u)
    {
        uint32_t *const product = exact_work_taken((2u * limbs) + 1u, 1);
        if ((product == NULL) || (limbs_product(product, left, limbs, right, limbs) == 0))
        {
            exact_work_released(product);
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = result, .src = product, .bytes = limbs * sizeof(uint32_t)});
        result[limbs] = 0u;
        fermat_subtract(result, result, &product[limbs], limbs);
        exact_work_released(product);
        return 1;
    }
    return fermat_transform(result, left, right, limbs, depth);
}

// the product of used limbs by the transform, into wide (left_used + right_used limbs). 0 when the allocation of the
// workspace fails
int transform_product(uint32_t *wide, const uint32_t *left, size_t left_used, const uint32_t *right, size_t right_used)
{
    const size_t bits = (left_used + right_used) * LIMB_BITS;
    const size_t step = (size_t)LIMB_BITS << ((bits_ceiling_log(bits) + 1u) / 2u);
    const size_t limbs = (((bits + step - 1u) / step) * step) / LIMB_BITS;
    uint32_t *const workspace = exact_work_taken(3u * (limbs + 1u), 1);
    if (workspace == NULL)
    {
        return 0;
    }
    uint32_t *const first = workspace;
    uint32_t *const second = &workspace[limbs + 1u];
    uint32_t *const product = &workspace[2u * (limbs + 1u)];
    memor.cpy(&(MemoriaCfg){.dst = first, .src = left, .bytes = left_used * sizeof(uint32_t)});
    memor.cpy(&(MemoriaCfg){.dst = second, .src = right, .bytes = right_used * sizeof(uint32_t)});
    // the product is below 2^bits <= 2^n. The ring leaves it whole
    const int product_ok = fermat_multiply(product, first, second, limbs);
    if (product_ok != 0)
    {
        memor.cpy(&(MemoriaCfg){.dst = wide, .src = product, .bytes = (left_used + right_used) * sizeof(uint32_t)});
    }
    exact_work_released(workspace);
    return product_ok;
}

// the product by the rung of the ladder that the operands reach, or by the transform alone when the caller asks for it.
// A rung that cannot allocate its workspace steps down to the rung below. Long multiplication needs no workspace
static OriorExactStatus exact_multiply_by(const OriorExactInteger *left, const OriorExactInteger *right,
                                          OriorExactInteger *result, int transform_only)
{
    if ((left->sign == 0) || (right->sign == 0))
    {
        orior_exact_zero(result);
        return ORIOR_EXACT_OK;
    }

    // A factor with n used limbs is at least 2^(32 * (n - 1)). The product of factors with n and m used limbs
    // is at least 2^(32 * (n + m - 2)). Where n + m passes the width by more than one limb, that
    // product already overruns, and the call multiplies nothing.
    const size_t left_used = magnitude_used(left->limb);
    const size_t right_used = magnitude_used(right->limb);
    const size_t range = left_used + right_used;
    if (range > (EXACT_LIMBS + 1u))
    {
        return ORIOR_EXACT_WILL_NOT_FIT;
    }
    // The product is below 2^(32 * range) and occupies at max `range` limbs, at max one past the
    // width. An overrun shows in that extra limb, and the accumulator retains it instead of wrapping
    // it away.
    EXACT_SCRATCH(wide, EXACT_LIMBS + 1u);
    if (!EXACT_SCRATCH_VALID(wide))
    {
        return ORIOR_EXACT_WILL_NOT_FIT;
    }
    const size_t shorter = (left_used < right_used) ? left_used : right_used;
    int ok = 0;
    if ((transform_only != 0) || (shorter >= (size_t)ORIOR_EXACT_TRANSFORM_LIMBS))
    {
        ok = transform_product(wide, left->limb, left_used, right->limb, right_used);
    }
    if ((ok == 0) && (transform_only == 0))
    {
        ok = limbs_product(wide, left->limb, left_used, right->limb, right_used);
        if (ok == 0)
        {
            limbs_long_product(wide, left->limb, left_used, right->limb, right_used);
            ok = 1;
        }
    }
    OriorExactStatus status = ORIOR_EXACT_OK;
    if ((ok == 0) || ((range > EXACT_LIMBS) && (wide[EXACT_LIMBS] != 0u)))
    {
        status = ORIOR_EXACT_WILL_NOT_FIT;
    }
    else
    {
        // Written only now, and the sign taken first, since `result` may be either factor.
        const int32_t sign = (left->sign == right->sign) ? 1 : -1;
        const size_t range_used = (range < EXACT_LIMBS) ? range : EXACT_LIMBS;
        memor.cpy(&(MemoriaCfg){.dst = result->limb, .src = wide, .bytes = range_used * sizeof(uint32_t)});
        memor.set(&(MemoriaCfg){
            .dst = &result->limb[range_used], .bytes = (EXACT_LIMBS - range_used) * sizeof(uint32_t), .val = 0u});
        settle_sign(result, sign);
    }
    EXACT_SCRATCH_RELEASE(wide);
    return status;
}

OriorExactStatus orior_exact_multiply_transform(const OriorExactInteger *left, const OriorExactInteger *right,
                                                OriorExactInteger *result)
{
    return exact_multiply_by(left, right, result, 1);
}

OriorExactStatus orior_exact_multiply(const OriorExactInteger *left, const OriorExactInteger *right,
                                      OriorExactInteger *result)
{
    return exact_multiply_by(left, right, result, 0);
}

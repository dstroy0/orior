// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_ntt_cuda.cu
 * @brief The transform on the card, over moduli that carry a primality certificate.
 *
 * Prints one line: length prime root stages kept_digest ms verdict
 *
 * WHAT THIS IS FOR
 *
 * Multiplication is convolution, and convolution is a pointwise product between two transforms. The host's own multiply
 * is Karatsuba, measured at an exponent of 1.635. That puts a billion decimal digits some fifteen hours away for ONE
 * product. The transform is N log N, and the card makes the constant in front of it small enough to matter.
 *
 * WHY THE MODULI CARRY THEIR WITNESSES
 *
 * A transform over a modulus that is not prime returns a value of the ordinary shape, and not the convolution of
 * anything. Take 1610612737: it has the right form, 3 * 2^29 + 1, and it is composite. The root raised to (p-1)/L then
 * has no such order. The transform does not fail: it answers wrongly. A check that tries division only up to one
 * hundred on a ten digit number would report the root as fine.
 *
 * Every modulus below therefore carries the Proth witness that proves it prime, and the root carries the length of its
 * proved order. `examples/proofing/twiddle_proof.py` produces both, and reproduces them. A constant here that does not
 * reproduce there deserves no trust.
 *
 * @note ONE THREAD PER BUTTERFLY, ONE LAUNCH PER STAGE. A stage cannot start before the one under it finishes for every
 *       element, and the launch boundary is the device-wide barrier. The stages are therefore launches, and not a loop
 *       inside one kernel. That costs a launch each, and buys the only ordering that is correct.
 * @note THE REDUCTION IS A REMAINDER, AND NOT A BARRETT FOLD. Integer division is slow here, and Barrett would remove
 *       it, at the cost of a second thing to be wrong about. The remainder is the version whose agreement with the host
 *       means something. The fold is an optimization for later, with this as the reference.
 * @note WHAT THE BENCH CHECKS. Round trip: the inverse of the forward is the input. Then a convolution with an answer
 *       known in closed form. Round trip alone cannot catch a transform with the wrong twiddles, because such a
 *       transform can still be its own inverse.
 */

#include "../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include <cstdio>
#include <cstdlib>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#include <cstdint>
#include <vector>

#define CHECK(call)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        cudaError_t status_ = (call);                                                                                  \
        if (status_ != cudaSuccess)                                                                                    \
        {                                                                                                              \
            std::fprintf(stderr, "%s:%d %s\n", __FILE__, __LINE__, cudaGetErrorString(status_));                       \
            std::exit(1);                                                                                              \
        }                                                                                                              \
    } while (0)

/*
 * The moduli, each with the Proth witness that proves it prime, and its two adic order. A modulus with no witness
 * beside it does not belong in this table.
 */
struct Modulus
{
    uint32_t prime;
    uint32_t generator;
    uint32_t witness; /* Proth witness: a^((p-1)/2) = -1 mod p. */
    uint32_t adic;    /* Two adic order of p-1: the cap on the length. */
};

static const Modulus MODULI[3] = {
    {2013265921u, 31u, 11u, 27u}, /* 15 * 2^27 + 1 */
    {2281701377u, 3u, 3u, 27u},   /* 17 * 2^27 + 1 */
    {3892314113u, 3u, 3u, 27u},   /* 29 * 2^27 + 1 */
};

/** @brief One modular multiply. Both inputs are below 2^32, and the product therefore fits an unsigned 64. */
__host__ __device__ __forceinline__ uint32_t mul_mod(uint32_t left, uint32_t right, uint32_t prime)
{
    return (uint32_t)(((uint64_t)left * (uint64_t)right) % (uint64_t)prime);
}

/** @brief Modular exponentiation by repeated squares, for twiddles and for the inverse length. */
__host__ __device__ uint32_t pow_mod(uint32_t base, uint64_t power, uint32_t prime)
{
    uint32_t out = 1u;
    uint32_t walk = base % prime;
    while (power > 0u)
    {
        if ((power & 1u) != 0u)
        {
            out = mul_mod(out, walk, prime);
        }
        walk = mul_mod(walk, walk, prime);
        power >>= 1;
    }
    return out;
}

/** @brief Reverse the low `bits` bits of `value`: the order that a decimation in time wants. */
__host__ __device__ __forceinline__ uint32_t reversed(uint32_t value, uint32_t bits)
{
    uint32_t out = 0u;
    for (uint32_t at = 0u; at < bits; ++at)
    {
        out = (out << 1) | ((value >> at) & 1u);
    }
    return out;
}

/**
 * @brief Place every element at its bit-reversed index, from one array into another.
 *
 * Out of place. In place would need each swap exactly once, and a thread that cannot see whether its partner already
 * ran undoes the swap.
 */
__global__ void scatter(const uint32_t *from, uint32_t *into, uint32_t length, uint32_t bits)
{
    uint32_t at = blockIdx.x * blockDim.x + threadIdx.x;
    if (at < length)
    {
        into[reversed(at, bits)] = from[at];
    }
}

/**
 * @brief Fill a table with the powers of `root`, which the group law says are all the twiddles.
 *
 * `twiddle_placement.py` proves that the table is a cyclic group of exactly this order. In it, w_j w_k is w_(j+k mod
 * n), w_0 is 1, and the order is the transform length, and not a divisor of it. Every twiddle that any stage will ever
 * want is therefore a power of one root. The whole set is this one table read at a stride, and nothing needs a new
 * computation inside a butterfly.
 *
 * The proof makes this safe, and not merely faster. A table built by repeated multiplication in floating point
 * accumulates error, and the literature warns against it for that reason. In the exact ring there is no error to
 * accumulate, and the cheap construction and the correct one are the same construction.
 */
/*
 * LAID OUT PER STAGE, SO THAT EVERY STAGE READS IT CONTIGUOUSLY. Stage s wants the powers of the root at a stride of
 * 2^(stages-1-s). A single flat table of powers would see a huge stride at the early stages, and a cache line fetch for
 * every load. The strides are powers of two, and the scalar between consecutive stages is therefore exactly two. The
 * set that a stage wants then has a closed form. A run of its own for each stage turns every one of those scattered
 * reads into a sequential one.
 *
 * Stage s occupies [2^s - 1, 2^(s+1) - 1), and the whole table is therefore length - 1.
 */
__global__ void fill_twiddles(uint32_t *table, uint32_t root, uint32_t length, uint32_t stages, uint32_t prime)
{
    uint32_t at = blockIdx.x * blockDim.x + threadIdx.x;
    if (at >= length - 1u)
    {
        return;
    }
    uint32_t stage = 31u - __clz(at + 1u);
    uint32_t inside = (at + 1u) - (1u << stage);
    uint32_t step = 1u << (stages - 1u - stage);
    table[at] = pow_mod(root, (uint64_t)inside * (uint64_t)step, prime);
}

/**
 * @brief The stages small enough to live inside one block, with no trip to global memory.
 *
 * A block that owns a contiguous run of `2 * blockDim.x` elements can run every stage whose pair distance stays inside
 * that run. Those are the first log2(2 * blockDim.x) of them. Each of those stages would otherwise cost a read and a
 * write of the whole array. Here they cost one read and one write between them. The code knows the schedule in advance,
 * and nothing in it depends on a value.
 */
__global__ void fused_low(uint32_t *values, const uint32_t *table, uint32_t here, uint32_t prime)
{
    extern __shared__ uint32_t buf[];
    uint32_t tid = threadIdx.x;
    uint32_t wide = blockDim.x;
    uint32_t base = blockIdx.x * (wide << 1);

    buf[tid] = values[base + tid];
    buf[tid + wide] = values[base + tid + wide];
    __syncthreads();

    for (uint32_t stage = 0u; stage < here; ++stage)
    {
        uint32_t half = 1u << stage;
        uint32_t inside = tid & (half - 1u);
        uint32_t block = (tid / half) * (half << 1);
        uint32_t upper_at = block + inside;
        uint32_t lower_at = upper_at + half;

        uint32_t twiddle = table[(1u << stage) - 1u + inside];
        uint64_t upper = buf[upper_at];
        uint64_t lower = mul_mod(buf[lower_at], twiddle, prime);

        uint64_t sum = upper + lower;
        if (sum >= (uint64_t)prime)
        {
            sum -= (uint64_t)prime;
        }
        uint64_t difference = upper + (uint64_t)prime - lower;
        if (difference >= (uint64_t)prime)
        {
            difference -= (uint64_t)prime;
        }

        __syncthreads();
        buf[upper_at] = (uint32_t)sum;
        buf[lower_at] = (uint32_t)difference;
        __syncthreads();
    }

    values[base + tid] = buf[tid];
    values[base + tid + wide] = buf[tid + wide];
}

__global__ void butterfly(uint32_t *values, const uint32_t *table, uint32_t length, uint32_t half, uint32_t offset,
                          uint32_t prime)
{
    uint32_t at = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t total = length >> 1;
    if (at >= total)
    {
        return;
    }

    uint32_t inside = at & (half - 1u);
    uint32_t block = (at / half) * (half << 1);
    uint32_t upper_at = block + inside;
    uint32_t lower_at = upper_at + half;

    /* One sequential load from this stage's own run of the table. */
    uint32_t twiddle = table[offset + inside];
    uint32_t upper = values[upper_at];
    uint32_t lower = mul_mod(values[lower_at], twiddle, prime);

    /*
     * WIDENED DELIBERATELY. Two residues below a modulus above 2^31 sum past 2^32, and the same is true of the borrow
     * in the difference. In 32 bits both wrap silently, and the transform returns a value of the ordinary shape that is
     * not the convolution of anything. Only the smallest of the three moduli here is under 2^31. A 32-bit run therefore
     * agrees on that modulus, and differs on the other two in every element.
     */
    uint64_t sum = (uint64_t)upper + (uint64_t)lower;
    if (sum >= (uint64_t)prime)
    {
        sum -= (uint64_t)prime;
    }
    uint64_t difference = (uint64_t)upper + (uint64_t)prime - (uint64_t)lower;
    if (difference >= (uint64_t)prime)
    {
        difference -= (uint64_t)prime;
    }

    values[upper_at] = (uint32_t)sum;
    values[lower_at] = (uint32_t)difference;
}

/** @brief Pointwise product of two transforms, which is where the convolution actually happens. */
__global__ void pointwise(uint32_t *into, const uint32_t *other, uint32_t length, uint32_t prime)
{
    uint32_t at = blockIdx.x * blockDim.x + threadIdx.x;
    if (at < length)
    {
        into[at] = mul_mod(into[at], other[at], prime);
    }
}

/** @brief Scale by the inverse length, which the inverse transform owes at the end. */
__global__ void rescale(uint32_t *values, uint32_t length, uint32_t factor, uint32_t prime)
{
    uint32_t at = blockIdx.x * blockDim.x + threadIdx.x;
    if (at < length)
    {
        values[at] = mul_mod(values[at], factor, prime);
    }
}

static uint32_t bits_of(uint32_t length)
{
    uint32_t out = 0u;
    while ((1u << out) < length)
    {
        ++out;
    }
    return out;
}

/** @brief The transform of `values` in place on the device, forward or inverse. */
static void transform(uint32_t *values, uint32_t *scratch, uint32_t *table, uint32_t length, const Modulus &modulus,
                      bool inverse)
{
    uint32_t bits = bits_of(length);
    uint32_t threads = 256u;
    uint32_t blocks = (length + threads - 1u) / threads;

    CHECK(cudaMemcpy(scratch, values, (size_t)length * sizeof(uint32_t), cudaMemcpyDeviceToDevice));
    scatter<<<blocks, threads>>>(scratch, values, length, bits);
    CHECK(cudaGetLastError());

    /* The root of unity of order `length`, from a generator whose order is the whole group. */
    uint32_t root = pow_mod(modulus.generator, (modulus.prime - 1u) / length, modulus.prime);
    if (inverse)
    {
        root = pow_mod(root, (uint64_t)modulus.prime - 2u, modulus.prime);
    }

    /*
     * EVERY TWIDDLE, ONCE, LAID OUT PER STAGE. Built here, and not inside the butterfly. The group law says that they
     * are the powers of one root, and the law has a proof.
     */
    uint32_t fill_blocks = (length + threads - 1u) / threads;
    fill_twiddles<<<fill_blocks, threads>>>(table, root, length, bits, modulus.prime);
    CHECK(cudaGetLastError());

    uint32_t half_count = length >> 1;
    uint32_t pair_blocks = (half_count + threads - 1u) / threads;

    /*
     * The stages that fit inside one block, run together in shared memory. `threads` butterflies cover twice that many
     * elements, and the first log2(2 * threads) stages therefore never leave the block.
     */
    uint32_t here = 0u;
    while ((1u << here) < (threads << 1) && here < bits)
    {
        ++here;
    }
    uint32_t low_blocks = length / (threads << 1);
    if (low_blocks >= 1u && here > 0u)
    {
        size_t shared = (size_t)(threads << 1) * sizeof(uint32_t);
        fused_low<<<low_blocks, threads, shared>>>(values, table, here, modulus.prime);
        CHECK(cudaGetLastError());
    }
    else
    {
        here = 0u;
    }

    for (uint32_t stage = here; stage < bits; ++stage)
    {
        uint32_t half = 1u << stage;
        butterfly<<<pair_blocks, threads>>>(values, table, length, half, (1u << stage) - 1u, modulus.prime);
        CHECK(cudaGetLastError());
    }

    if (inverse)
    {
        uint32_t factor = pow_mod(length, (uint64_t)modulus.prime - 2u, modulus.prime);
        rescale<<<blocks, threads>>>(values, length, factor, modulus.prime);
        CHECK(cudaGetLastError());
    }
    CHECK(cudaDeviceSynchronize());
}

/** @brief Reduce a limb array modulo a prime, since a limb runs to 2^32 and a residue does not. */
__global__ void reduce_into(const uint32_t *from, uint32_t *into, uint32_t length, uint32_t used, uint32_t prime)
{
    uint32_t at = blockIdx.x * blockDim.x + threadIdx.x;
    if (at < length)
    {
        into[at] = (at < used) ? (uint32_t)((uint64_t)from[at] % (uint64_t)prime) : 0u;
    }
}

/**
 * @brief Reduce a SIGNED coefficient array modulo a prime.
 *
 * The multiply path reduces limbs, which carry no sign by construction: an integer is a run of magnitudes, and the
 * caller carries the sign. A POLYNOMIAL is different. Euler's pentagonal series has coefficients in {-1, 0, 1}, and a
 * cusp form's coefficients are negative about half the time. Neither can go through a door that reads a limb.
 *
 * The kernel therefore reads the word as a two's complement int32, and maps it into the field. There is no other
 * difference between this and `reduce_into`. They stay separate, and do not share a flag. A caller who picks the wrong
 * one gets an answer of the ordinary shape either way.
 */
__global__ void reduce_signed(const int32_t *from, uint32_t *into, uint32_t length, uint32_t used, uint32_t prime)
{
    uint32_t at = blockIdx.x * blockDim.x + threadIdx.x;
    if (at < length)
    {
        if (at < used)
        {
            int64_t value = (int64_t)from[at] % (int64_t)prime;
            if (value < 0)
            {
                value += (int64_t)prime;
            }
            into[at] = (uint32_t)value;
        }
        else
        {
            into[at] = 0u;
        }
    }
}

/**
 * @brief Garner's rule across the three moduli, one coefficient per thread.
 *
 * The recombined coefficient is base + p0 * step + p0 * p1 * rest. Every intermediate here stays inside a 64 bit lane,
 * because the two corrections are each below a modulus. The kernel does NOT assemble the full-width value. The caller
 * does that for the whole array at once, as three integer reads, and not a hundred million times over.
 */
__global__ void garner(const uint32_t *first, const uint32_t *second, const uint32_t *third, uint32_t *base,
                       uint32_t *step, uint32_t *rest, uint32_t length, uint32_t prime0, uint32_t prime1,
                       uint32_t prime2, uint32_t into1, uint32_t into2, uint32_t across)
{
    uint32_t at = blockIdx.x * blockDim.x + threadIdx.x;
    if (at >= length)
    {
        return;
    }

    uint64_t low = (uint64_t)first[at];
    uint64_t one = ((uint64_t)second[at] + (uint64_t)prime1 - (low % (uint64_t)prime1)) % (uint64_t)prime1;
    one = (one * (uint64_t)into1) % (uint64_t)prime1;

    uint64_t two = ((uint64_t)third[at] + (uint64_t)prime2 - (low % (uint64_t)prime2)) % (uint64_t)prime2;
    two = (two * (uint64_t)into2) % (uint64_t)prime2;
    two = (two + (uint64_t)prime2 - (one % (uint64_t)prime2)) % (uint64_t)prime2;
    two = (two * (uint64_t)across) % (uint64_t)prime2;

    base[at] = (uint32_t)low;
    step[at] = (uint32_t)one;
    rest[at] = (uint32_t)two;
}

/** @brief Read a whole file into a limb vector, or stop with a message naming the file. */
static std::vector<uint32_t> limbs_from(const char *path)
{
    std::FILE *handle = std::fopen(path, "rb");
    if (handle == nullptr)
    {
        std::fprintf(stderr, "cannot open %s\n", path);
        std::exit(1);
    }
    std::fseek(handle, 0, SEEK_END);
    long size = std::ftell(handle);
    std::fseek(handle, 0, SEEK_SET);
    std::vector<uint32_t> out((size_t)size / sizeof(uint32_t), 0u);
    if (std::fread(out.data(), 1, (size_t)size, handle) != (size_t)size)
    {
        std::fprintf(stderr, "short read on %s\n", path);
        std::exit(1);
    }
    std::fclose(handle);
    return out;
}

static void limbs_into(const char *path, const std::vector<uint32_t> &values)
{
    std::FILE *handle = std::fopen(path, "wb");
    if (handle == nullptr)
    {
        std::fprintf(stderr, "cannot write %s\n", path);
        std::exit(1);
    }
    std::fwrite(values.data(), sizeof(uint32_t), values.size(), handle);
    std::fclose(handle);
}

/**
 * @brief Read `count` limbs from standard input, which costs no disk at all.
 *
 * At a billion digits, the three returned arrays are some 1.5 GB, and the two inputs another 0.5 GB. Through files,
 * that is two gigabytes onto a disk and back for every single multiply: 8.6 seconds against 1.77 seconds of actual
 * transform. A pipe keeps it in memory.
 */
static std::vector<uint32_t> limbs_from_stream(size_t count)
{
    std::vector<uint32_t> out(count, 0u);
    size_t want = count * sizeof(uint32_t);
    size_t got = 0u;
    char *into = (char *)out.data();
    while (got < want)
    {
        size_t step = std::fread(into + got, 1, want - got, stdin);
        if (step == 0u)
        {
            std::fprintf(stderr, "short read on stdin, wanted %zu got %zu\n", want, got);
            std::exit(1);
        }
        got += step;
    }
    return out;
}

static void limbs_to_stream(const std::vector<uint32_t> &values)
{
    size_t want = values.size() * sizeof(uint32_t);
    size_t put = 0u;
    const char *from = (const char *)values.data();
    while (put < want)
    {
        size_t step = std::fwrite(from + put, 1, want - put, stdout);
        if (step == 0u)
        {
            std::fprintf(stderr, "short write on stdout\n");
            std::exit(1);
        }
        put += step;
    }
}

/** @brief The convolution of two limb arrays on the host, exactly, for lengths small enough to. */
static std::vector<uint64_t> host_convolve(const std::vector<uint32_t> &left, const std::vector<uint32_t> &right,
                                           uint32_t prime)
{
    std::vector<uint64_t> out(left.size(), 0u);
    for (size_t one = 0u; one < left.size(); ++one)
    {
        if (left[one] == 0u)
        {
            continue;
        }
        for (size_t two = 0u; two < right.size(); ++two)
        {
            size_t at = (one + two) % out.size();
            out[at] = (out[at] + (uint64_t)left[one] * (uint64_t)right[two]) % (uint64_t)prime;
        }
    }
    return out;
}

/**
 * @brief One resident operand, already forward transformed, kept across commands.
 *
 * WHY A VALUE STAYS ON THE CARD AND STAYS TRANSFORMED
 *
 * `run_multiply` pays nine transforms per call: three moduli, two forwards and one inverse each. In a chain of
 * multiplies, that is the wrong shape. The inverse at the end of one product and the two forwards at the start of the
 * next undo and redo the same work. A product of k factors costs 3k transforms through this door, and 3(k+1) through
 * that one.
 *
 * In the transform domain, a multiply is `pointwise`: one elementwise multiply, and no transform at all. An operand
 * therefore takes one transform on the way in, and one transform back on the way out. In between, it takes as many
 * multiplies as the caller likes, for the cost of a kernel each.
 *
 * ONE LENGTH SERVES THE WHOLE SESSION, AND THAT IS NOT A LIMITATION HERE
 *
 * A pointwise multiply needs two operands transformed at the same length. The caller therefore declares the length
 * once, and every resident buffer uses it. A caller with a long chain of multiplies already knows the final degree, and
 * the final degree decides the length.
 *
 * The padding rule is the ordinary one. A product of two operands that occupy `a` and `b` coefficients needs a + b
 * places. The declared length must therefore cover the output of the whole chain, or the result wraps. The wrap is
 * silent, as a wrong twiddle is silent. The session therefore tells the caller the length that it asked for and the
 * count that it used, and the caller can check the sum.
 */
struct Resident
{
    uint32_t *residues[3]; /**< the three moduli, forward transformed, on the device */
    size_t used;           /**< coefficients the caller supplied, before padding */
    bool live;
};

/**
 * @brief Multiply two limb files, and write the three Garner arrays that the caller reassembles.
 *
 * The caller hands in each factor as a flat run of 32 bit limbs, least significant first. That is the form of an
 * integer in memory. It reads the answer back the same way. Each output array is a little endian integer at a 32 bit
 * stride. A reassembly of the product is then three integer reads and two multiplies by a constant. The carries happen
 * inside that addition, and not in a pass over a hundred million coefficients.
 *
 * @return the milliseconds that the device spent, without the files at either end.
 */
static float run_multiply(const char *left_path, const char *right_path, const char *prefix, bool piped)
{
    std::vector<uint32_t> left;
    std::vector<uint32_t> right;
    if (piped)
    {
        uint64_t counts[2] = {0u, 0u};
        if (std::fread(counts, sizeof(uint64_t), 2u, stdin) != 2u)
        {
            return -1.0f; /* the caller closed the pipe: a run ends that way */
        }
        if (counts[0] == 0u && counts[1] == 0u)
        {
            return -1.0f; /* an explicit goodbye, for a caller that wants to stay tidy */
        }
        left = limbs_from_stream((size_t)counts[0]);
        right = limbs_from_stream((size_t)counts[1]);
    }
    else
    {
        left = limbs_from(left_path);
        right = limbs_from(right_path);
    }
    size_t wanted = left.size() + right.size();

    uint32_t length = 2u;
    while ((size_t)length < wanted)
    {
        length <<= 1;
    }
    for (uint32_t at = 0u; at < 3u; ++at)
    {
        if (length > (1u << MODULI[at].adic))
        {
            std::fprintf(stderr, "length %u exceeds modulus %u, whose order is 2^%u\n", length, MODULI[at].prime,
                         MODULI[at].adic);
            std::exit(1);
        }
    }

    size_t bytes = (size_t)length * sizeof(uint32_t);
    uint32_t *residues[3] = {nullptr, nullptr, nullptr};
    uint32_t *other = nullptr;
    uint32_t *scratch = nullptr;
    uint32_t *raw = nullptr;
    uint32_t *table = nullptr;
    for (uint32_t at = 0u; at < 3u; ++at)
    {
        CHECK(cudaMalloc(&residues[at], bytes));
    }
    CHECK(cudaMalloc(&other, bytes));
    CHECK(cudaMalloc(&scratch, bytes));
    CHECK(cudaMalloc(&raw, bytes));
    // The per stage table holds length - 1 entries, one run for each of the stages.
    CHECK(cudaMalloc(&table, bytes));

    uint32_t threads = 256u;
    uint32_t blocks = (length + threads - 1u) / threads;

    cudaEvent_t started;
    cudaEvent_t stopped;
    CHECK(cudaEventCreate(&started));
    CHECK(cudaEventCreate(&stopped));
    CHECK(cudaEventRecord(started));

    for (uint32_t at = 0u; at < 3u; ++at)
    {
        const Modulus modulus = MODULI[at];

        CHECK(cudaMemcpy(raw, left.data(), left.size() * sizeof(uint32_t), cudaMemcpyHostToDevice));
        reduce_into<<<blocks, threads>>>(raw, residues[at], length, (uint32_t)left.size(), modulus.prime);
        CHECK(cudaGetLastError());

        CHECK(cudaMemcpy(raw, right.data(), right.size() * sizeof(uint32_t), cudaMemcpyHostToDevice));
        reduce_into<<<blocks, threads>>>(raw, other, length, (uint32_t)right.size(), modulus.prime);
        CHECK(cudaGetLastError());

        transform(residues[at], scratch, table, length, modulus, false);
        transform(other, scratch, table, length, modulus, false);
        pointwise<<<blocks, threads>>>(residues[at], other, length, modulus.prime);
        CHECK(cudaGetLastError());
        transform(residues[at], scratch, table, length, modulus, true);
    }

    /*
     * Garner in place. Each thread reads all three residues at its own index, and writes all three outputs at that same
     * index. The arrays may therefore alias, and no thread sees the write of another.
     */
    uint32_t prime0 = MODULI[0].prime;
    uint32_t prime1 = MODULI[1].prime;
    uint32_t prime2 = MODULI[2].prime;
    uint32_t into1 = pow_mod(prime0, (uint64_t)prime1 - 2u, prime1);
    uint32_t into2 = pow_mod(prime0, (uint64_t)prime2 - 2u, prime2);
    uint32_t across = pow_mod(prime1, (uint64_t)prime2 - 2u, prime2);
    garner<<<blocks, threads>>>(residues[0], residues[1], residues[2], residues[0], residues[1], residues[2], length,
                                prime0, prime1, prime2, into1, into2, across);
    CHECK(cudaGetLastError());

    CHECK(cudaEventRecord(stopped));
    CHECK(cudaEventSynchronize(stopped));
    float spent = 0.0f;
    CHECK(cudaEventElapsedTime(&spent, started, stopped));

    /*
     * ONLY WHAT THE PRODUCT HAS. The transform runs at the next power of two, but the product carries only wide + tall
     * limbs. A write of the padded tail would move hundreds of megabytes of zeros across the bus and onto the disk for
     * nothing.
     */
    static const char *names[3] = {"base", "step", "rest"};
    size_t carried = left.size() + right.size();
    if (carried > (size_t)length)
    {
        carried = (size_t)length;
    }
    std::vector<uint32_t> back(carried, 0u);
    for (uint32_t at = 0u; at < 3u; ++at)
    {
        CHECK(cudaMemcpy(back.data(), residues[at], carried * sizeof(uint32_t), cudaMemcpyDeviceToHost));
        if (piped)
        {
            limbs_to_stream(back);
        }
        else
        {
            char path[1024];
            std::snprintf(path, sizeof(path), "%s.%s.bin", prefix, names[at]);
            limbs_into(path, back);
        }
    }

    for (uint32_t at = 0u; at < 3u; ++at)
    {
        CHECK(cudaFree(residues[at]));
    }
    CHECK(cudaFree(other));
    CHECK(cudaFree(scratch));
    CHECK(cudaFree(raw));
    CHECK(cudaFree(table));
    if (piped)
    {
        std::fflush(stdout);
        std::fprintf(stderr, "%u %zu %zu %.3f multiplied\n", length, left.size(), right.size(), spent);
    }
    else
    {
        std::printf("%u %zu %zu %.3f multiplied\n", length, left.size(), right.size(), spent);
    }
    return spent;
}

/**
 * @brief Serve commands that keep operands in the transform domain between multiplies.
 *
 * The protocol is little endian 64 bit words on stdin, with answers on stdout.
 *
 * - `OPEN 1, length, slots`: declare the transform length, and how many buffers to hold.
 *
 * - `LOAD 2, slot, count, limbs`: reduce, forward transform, and keep in `slot`.
 *
 * - `TIMES 3, into, from`: into *= from, pointwise, in the domain.
 *
 * - `READ 4, slot, count`: inverse transform, Garner, and write three arrays of `count`.
 *
 * - `DROP 5, slot`: release a buffer.
 *
 * - `CLOSE 0`: leave.
 *
 * LOAD and READ each pay three transforms. TIMES pays none, and this session exists for that reason.
 *
 * READ IS DESTRUCTIVE, AND THIS SAYS SO. The inverse runs in place. A buffer after a read no longer holds a transformed
 * value, and needs a new LOAD before a multiply. A readable buffer would need a copy of a buffer of hundreds of
 * megabytes, to save a caller one LOAD.
 */
static void run_domain(void)
{
    uint64_t word = 0u;
    uint32_t length = 0u;
    std::vector<Resident> slots;
    uint32_t *scratch = nullptr;
    uint32_t *table = nullptr;
    uint32_t *raw = nullptr;
    uint32_t threads = 256u;
    uint32_t blocks = 0u;

    while (std::fread(&word, sizeof(uint64_t), 1u, stdin) == 1u)
    {
        if (word == 0u)
        {
            break;
        }
        if (word == 1u)
        {
            uint64_t header[2] = {0u, 0u};
            if (std::fread(header, sizeof(uint64_t), 2u, stdin) != 2u)
            {
                break;
            }
            length = (uint32_t)header[0];
            if ((length & (length - 1u)) != 0u || length < 2u)
            {
                std::fprintf(stderr, "a transform length is a power of two\n");
                std::exit(1);
            }
            for (uint32_t at = 0u; at < 3u; ++at)
            {
                if (length > (1u << MODULI[at].adic))
                {
                    std::fprintf(stderr, "length %u exceeds modulus %u, whose order is 2^%u\n", length,
                                 MODULI[at].prime, MODULI[at].adic);
                    std::exit(1);
                }
            }
            size_t bytes = (size_t)length * sizeof(uint32_t);
            blocks = (length + threads - 1u) / threads;
            slots.assign((size_t)header[1], Resident());
            for (size_t slot = 0u; slot < slots.size(); ++slot)
            {
                for (uint32_t at = 0u; at < 3u; ++at)
                {
                    CHECK(cudaMalloc(&slots[slot].residues[at], bytes));
                }
                slots[slot].live = false;
                slots[slot].used = 0u;
            }
            CHECK(cudaMalloc(&scratch, bytes));
            CHECK(cudaMalloc(&table, bytes));
            CHECK(cudaMalloc(&raw, bytes));
            std::fprintf(stderr, "%u %zu opened\n", length, slots.size());
            continue;
        }
        if (word == 2u)
        {
            uint64_t header[2] = {0u, 0u};
            if (std::fread(header, sizeof(uint64_t), 2u, stdin) != 2u)
            {
                break;
            }
            size_t slot = (size_t)header[0];
            std::vector<uint32_t> values = limbs_from_stream((size_t)header[1]);
            CHECK(cudaMemcpy(raw, values.data(), values.size() * sizeof(uint32_t), cudaMemcpyHostToDevice));
            for (uint32_t at = 0u; at < 3u; ++at)
            {
                reduce_into<<<blocks, threads>>>(raw, slots[slot].residues[at], length, (uint32_t)values.size(),
                                                 MODULI[at].prime);
                CHECK(cudaGetLastError());
                transform(slots[slot].residues[at], scratch, table, length, MODULI[at], false);
            }
            slots[slot].live = true;
            slots[slot].used = values.size();
            continue;
        }
        if (word == 6u)
        {
            /*
             * LOAD SIGNED. The same as LOAD, with each word read as a two's complement int32. The coefficients of a
             * polynomial carry signs, and a limb does not. The two doors stay apart, because the wrong one returns a
             * value of the ordinary shape.
             */
            uint64_t header[2] = {0u, 0u};
            if (std::fread(header, sizeof(uint64_t), 2u, stdin) != 2u)
            {
                break;
            }
            size_t slot = (size_t)header[0];
            std::vector<uint32_t> values = limbs_from_stream((size_t)header[1]);
            CHECK(cudaMemcpy(raw, values.data(), values.size() * sizeof(uint32_t), cudaMemcpyHostToDevice));
            for (uint32_t at = 0u; at < 3u; ++at)
            {
                reduce_signed<<<blocks, threads>>>((const int32_t *)raw, slots[slot].residues[at], length,
                                                   (uint32_t)values.size(), MODULI[at].prime);
                CHECK(cudaGetLastError());
                transform(slots[slot].residues[at], scratch, table, length, MODULI[at], false);
            }
            slots[slot].live = true;
            slots[slot].used = values.size();
            continue;
        }
        if (word == 3u)
        {
            uint64_t header[2] = {0u, 0u};
            if (std::fread(header, sizeof(uint64_t), 2u, stdin) != 2u)
            {
                break;
            }
            size_t into = (size_t)header[0];
            size_t from = (size_t)header[1];
            for (uint32_t at = 0u; at < 3u; ++at)
            {
                pointwise<<<blocks, threads>>>(slots[into].residues[at], slots[from].residues[at], length,
                                               MODULI[at].prime);
                CHECK(cudaGetLastError());
            }
            CHECK(cudaDeviceSynchronize());
            slots[into].used += slots[from].used;
            continue;
        }
        if (word == 4u)
        {
            uint64_t header[2] = {0u, 0u};
            if (std::fread(header, sizeof(uint64_t), 2u, stdin) != 2u)
            {
                break;
            }
            size_t slot = (size_t)header[0];
            size_t carried = (size_t)header[1];
            if (carried > (size_t)length)
            {
                carried = (size_t)length;
            }
            for (uint32_t at = 0u; at < 3u; ++at)
            {
                transform(slots[slot].residues[at], scratch, table, length, MODULI[at], true);
            }
            uint32_t prime0 = MODULI[0].prime;
            uint32_t prime1 = MODULI[1].prime;
            uint32_t prime2 = MODULI[2].prime;
            garner<<<blocks, threads>>>(slots[slot].residues[0], slots[slot].residues[1], slots[slot].residues[2],
                                        slots[slot].residues[0], slots[slot].residues[1], slots[slot].residues[2],
                                        length, prime0, prime1, prime2, pow_mod(prime0, (uint64_t)prime1 - 2u, prime1),
                                        pow_mod(prime0, (uint64_t)prime2 - 2u, prime2),
                                        pow_mod(prime1, (uint64_t)prime2 - 2u, prime2));
            CHECK(cudaGetLastError());
            CHECK(cudaDeviceSynchronize());

            std::vector<uint32_t> back(carried, 0u);
            for (uint32_t at = 0u; at < 3u; ++at)
            {
                CHECK(cudaMemcpy(back.data(), slots[slot].residues[at], carried * sizeof(uint32_t),
                                 cudaMemcpyDeviceToHost));
                limbs_to_stream(back);
            }
            std::fflush(stdout);
            slots[slot].live = false;
            continue;
        }
        if (word == 5u)
        {
            uint64_t header[1] = {0u};
            if (std::fread(header, sizeof(uint64_t), 1u, stdin) != 1u)
            {
                break;
            }
            slots[(size_t)header[0]].live = false;
            continue;
        }
        std::fprintf(stderr, "unknown command %llu\n", (unsigned long long)word);
        std::exit(1);
    }

    for (size_t slot = 0u; slot < slots.size(); ++slot)
    {
        for (uint32_t at = 0u; at < 3u; ++at)
        {
            CHECK(cudaFree(slots[slot].residues[at]));
        }
    }
    if (scratch != nullptr)
    {
        CHECK(cudaFree(scratch));
    }
    if (table != nullptr)
    {
        CHECK(cudaFree(table));
    }
    if (raw != nullptr)
    {
        CHECK(cudaFree(raw));
    }
}

int main(int argc, char **argv)
{
    uint32_t length = 1u << 12;
    uint32_t which = 0u;
    bool grading = false;
    const char *left_path = nullptr;
    const char *right_path = nullptr;
    const char *prefix = nullptr;
    bool piped = false;
    bool resident = false;

    for (int at = 1; at < argc; ++at)
    {
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = argv[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = "--length", .ci = EMBED_FALSE) &&
            at + 1 < argc)
        {
            length = (uint32_t)std::strtoul(argv[++at], nullptr, 10);
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = argv[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--modulus", .ci = EMBED_FALSE) &&
                 at + 1 < argc)
        {
            which = (uint32_t)std::strtoul(argv[++at], nullptr, 10);
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = argv[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--check", .ci = EMBED_FALSE))
        {
            grading = true;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = argv[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--left", .ci = EMBED_FALSE) &&
                 at + 1 < argc)
        {
            left_path = argv[++at];
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = argv[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--right", .ci = EMBED_FALSE) &&
                 at + 1 < argc)
        {
            right_path = argv[++at];
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = argv[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--out", .ci = EMBED_FALSE) &&
                 at + 1 < argc)
        {
            prefix = argv[++at];
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = argv[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--pipe", .ci = EMBED_FALSE))
        {
            piped = true;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = argv[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--domain", .ci = EMBED_FALSE))
        {
            resident = true;
        }
    }

    if (resident)
    {
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif
        /* The same one context argument the multiply pipe makes, for the same measured reason. */
        CHECK(cudaFree(nullptr));
        run_domain();
        return 0;
    }

    if (piped)
    {
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif
        /*
         * ONE CONTEXT, MANY MULTIPLIES. A CUDA context costs 192 ms to create, measured. A run of pi to a million
         * places launches this process 56 times. Context setup is 10.76 s of the 19.55 s wall clock, against 0.25 s of
         * actual transform. A loop of service spends that once.
         */
        CHECK(cudaFree(nullptr)); /* force the context up before the first read of a request */
        while (run_multiply(nullptr, nullptr, nullptr, true) >= 0.0f)
        {
        }
        return 0;
    }
    if (left_path != nullptr && right_path != nullptr && prefix != nullptr)
    {
        run_multiply(left_path, right_path, prefix, false);
        return 0;
    }

    if (which > 2u)
    {
        std::fprintf(stderr, "there are three moduli, 0 through 2\n");
        return 1;
    }
    if ((length & (length - 1u)) != 0u || length < 2u)
    {
        std::fprintf(stderr, "a transform length is a power of two\n");
        return 1;
    }

    const Modulus modulus = MODULI[which];
    if (length > (1u << modulus.adic))
    {
        std::fprintf(stderr, "modulus %u admits no root of unity of order %u\n", modulus.prime, length);
        return 1;
    }

    /* Two inputs whose convolution the host can also compute, so that the bench can check agreement. */
    std::vector<uint32_t> left(length, 0u);
    std::vector<uint32_t> right(length, 0u);
    uint32_t filled = grading ? (length >> 1) : (length >> 1);
    uint64_t state = 0x51F7u;
    for (uint32_t at = 0u; at < filled; ++at)
    {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        left[at] = (uint32_t)((state >> 33) % modulus.prime);
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        right[at] = (uint32_t)((state >> 33) % modulus.prime);
    }

    uint32_t *one = nullptr;
    uint32_t *two = nullptr;
    uint32_t *scratch = nullptr;
    uint32_t *table = nullptr;
    size_t bytes = (size_t)length * sizeof(uint32_t);
    CHECK(cudaMalloc(&one, bytes));
    CHECK(cudaMalloc(&two, bytes));
    CHECK(cudaMalloc(&scratch, bytes));
    CHECK(cudaMalloc(&table, bytes));
    CHECK(cudaMemcpy(one, left.data(), bytes, cudaMemcpyHostToDevice));
    CHECK(cudaMemcpy(two, right.data(), bytes, cudaMemcpyHostToDevice));

    cudaEvent_t started;
    cudaEvent_t stopped;
    CHECK(cudaEventCreate(&started));
    CHECK(cudaEventCreate(&stopped));
    CHECK(cudaEventRecord(started));

    transform(one, scratch, table, length, modulus, false);
    transform(two, scratch, table, length, modulus, false);
    uint32_t threads = 256u;
    uint32_t blocks = (length + threads - 1u) / threads;
    pointwise<<<blocks, threads>>>(one, two, length, modulus.prime);
    CHECK(cudaGetLastError());
    transform(one, scratch, table, length, modulus, true);

    CHECK(cudaEventRecord(stopped));
    CHECK(cudaEventSynchronize(stopped));
    float spent = 0.0f;
    CHECK(cudaEventElapsedTime(&spent, started, stopped));

    std::vector<uint32_t> back(length, 0u);
    CHECK(cudaMemcpy(back.data(), one, bytes, cudaMemcpyDeviceToHost));

    const char *verdict = "not-graded";
    if (grading)
    {
        std::vector<uint64_t> wanted = host_convolve(left, right, modulus.prime);
        size_t wrong = 0u;
        for (uint32_t at = 0u; at < length; ++at)
        {
            if ((uint64_t)back[at] != wanted[at])
            {
                ++wrong;
            }
        }
        verdict = (wrong == 0u) ? "agrees" : "DISAGREES";
        if (wrong != 0u)
        {
            std::fprintf(stderr, "%zu of %u slots differ\n", wrong, length);
        }
    }

    /* A digest that changes if any element changes, so that two runs compare without a print of the data. */
    uint64_t digest = 1469598103934665603ull;
    for (uint32_t at = 0u; at < length; ++at)
    {
        digest ^= (uint64_t)back[at];
        digest *= 1099511628211ull;
    }

    std::printf("%u %u %u %u %016llx %.3f %s\n", length, modulus.prime, modulus.witness, bits_of(length),
                (unsigned long long)digest, spent, verdict);

    CHECK(cudaFree(one));
    CHECK(cudaFree(two));
    CHECK(cudaFree(scratch));
    CHECK(cudaFree(table));
    return 0;
}

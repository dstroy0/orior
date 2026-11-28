// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_engines.cpp
 * @brief The old engine against the new one, on the same nonces. It reports what each actually did.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note The legacy arm below is a verbatim copy of the original engine, and not a description of it.
 *       Both arms run the same nonce range from the same header. Thus the comparison is between two
 *       engines, and not between two benchmarks.
 * @note Evaluations per second is the only axis on which these two are comparable. On that axis,
 *       the legacy arm wins by a wide margin. The measurement exists to show that. The legacy arm
 *       is fast because it computes almost nothing. A rate apart from the work per step is not a
 *       hash rate.
 * @warning Do not read the evaluation rate of the legacy arm as a hash rate. It performs no SHA-256
 *          round. Thus its useful hash rate is exactly zero, however fast it iterates.
 */

#include "bench_hex.h"
#include "memoria_operor/memoria_operor.h"
#include "sha256_core.h"

#include "embed_turnstile.h"
#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>

#include "bench_job_ring.h"
#include "bench_load_limit.h"

namespace
{

/** @brief Nonces each arm evaluates. */
const uint32_t BENCH_NONCES = 8000000u;

/** @brief Block 125552, whose header and nonce the chain recorded. */
const char *const BENCH_HEADER_HEX =
    "0100000081cd02ab7e569e8bcd9317e2fe99f2de44d49ab2b8851ba4a308000000000000e320b6c2fffc8d750423"
    "db8b1eb942ae710e951ed797f7affc8892b0f1fc122bc7f5d74df2b9441a42a14695";

/** @brief The nonce of block 125552. */
const uint32_t BENCH_EXPECTED_NONCE = 0x9546a142u;

// ---------------------------------------------------------------------------------------------
// Legacy arm, copied verbatim from commit 6e4747c src/btc_miner.cpp
// ---------------------------------------------------------------------------------------------

#define REPRESENTATION_SIZE_WORDS 128 // 4096 bits / 32 bits

// --- Anchor Sift Topological Engine State ---
uint32_t anchor_sift_measure_departure(const uint32_t *topology_space)
{
    // Computes topological entropy departure across the 4096-bit representation grid
    uint32_t departure_sum = 0;
#pragma unroll
    for (int i = 0; i < REPRESENTATION_SIZE_WORDS; i += 4)
    {
        departure_sum ^= topology_space[i] + topology_space[i + 1];
    }
    return departure_sum & 0x0000FFFF; // Bounded structural metric
}

// --- AVX2 Midstate & Paper-Fold Transform Pipeline ---
inline void sha256_transform_midstate_avx2(__m256i *hash_out, const uint32_t *midstate, const uint32_t *chunk2)
{
    uint32_t base_val = midstate[0] ^ chunk2[3];
    *hash_out = _mm256_set1_epi32(base_val);
}

/** @brief What the legacy arm reports after a run. */
struct LegacyResult
{
    uint64_t evaluations;
    uint32_t winning_nonce;
    int block_found;
    uint32_t checksum;
};

/**
 * @brief Runs the legacy engine's per-nonce pipeline over a range, single threaded.
 *
 * @param[in]  midstate        Chaining value, used exactly as the original used it [BORROWS].
 * @param[in]  remaining_merkle Merkle tail word.
 * @param[in]  timestamp       Header time word.
 * @param[in]  compact_target  The value the original compared against.
 * @param[in]  nonce_start     First nonce.
 * @param[in]  nonce_end       One past the last.
 * @param[out] result          What happened [BORROWS].
 * @note The original ran this under OpenMP across sixteen threads. It is single threaded here. Thus
 *       the comparison is per core. The bench reports the thread scale of both arms separately.
 * @note The checksum exists only so the optimizer cannot delete a pipeline whose output nothing
 *       reads. Without it the legacy arm measures an empty loop.
 */
void run_legacy_engine(const uint32_t *midstate, uint32_t remaining_merkle, uint32_t timestamp, uint32_t compact_target,
                       uint32_t nonce_start, uint32_t nonce_end, LegacyResult *result)
{
    alignas(32) uint32_t topology_space[REPRESENTATION_SIZE_WORDS];
    uint32_t chunk2[4];

    chunk2[0] = remaining_merkle;
    chunk2[1] = timestamp;
    chunk2[2] = compact_target;

    result->evaluations = 0u;
    result->winning_nonce = 0u;
    result->block_found = 0;
    result->checksum = 0u;

    for (uint32_t n = nonce_start; n < nonce_end; n++)
    {
        chunk2[3] = n;

        __m256i hash1_vector;
        sha256_transform_midstate_avx2(&hash1_vector, midstate, chunk2);

        __m256i fold_a = _mm256_shuffle_epi32(hash1_vector, _MM_SHUFFLE(0, 1, 2, 3));
        __m256i crease_1 = _mm256_xor_si256(hash1_vector, fold_a);

        __m256i fold_b = _mm256_permute2x128_si256(crease_1, crease_1, 1);
        __m256i crease_2 = _mm256_xor_si256(crease_1, fold_b);

        __m256i *space_ptr = (__m256i *)topology_space;
#pragma unroll
        for (int i = 0; i < (REPRESENTATION_SIZE_WORDS / 8); i++)
        {
            _mm256_store_si256(&space_ptr[i], crease_2);
        }

        uint32_t distance = anchor_sift_measure_departure(topology_space);
        result->evaluations += 1u;
        result->checksum ^= distance;

        if (distance < compact_target)
        {
            if (!result->block_found)
            {
                result->winning_nonce = n;
                result->block_found = 1;
            }
        }
    }
}

/**
 * @brief Runs one nonce through the legacy pipeline and returns the distance it produces.
 *
 * @param[in] midstate         Chaining value [BORROWS].
 * @param[in] remaining_merkle Merkle tail word.
 * @param[in] timestamp        Header time word.
 * @param[in] nonce            The nonce to evaluate.
 * @return                     The distance the legacy engine measures for that nonce.
 * @note Exists to answer one question the rate cannot: does the legacy output depend on its input.
 */
uint32_t legacy_distance_for_nonce(const uint32_t *midstate, uint32_t remaining_merkle, uint32_t timestamp,
                                   uint32_t nonce)
{
    alignas(32) uint32_t topology_space[REPRESENTATION_SIZE_WORDS];
    uint32_t chunk2[4] = {remaining_merkle, timestamp, 0u, nonce};

    __m256i hash1_vector;
    sha256_transform_midstate_avx2(&hash1_vector, midstate, chunk2);

    __m256i fold_a = _mm256_shuffle_epi32(hash1_vector, _MM_SHUFFLE(0, 1, 2, 3));
    __m256i crease_1 = _mm256_xor_si256(hash1_vector, fold_a);

    __m256i fold_b = _mm256_permute2x128_si256(crease_1, crease_1, 1);
    __m256i crease_2 = _mm256_xor_si256(crease_1, fold_b);

    __m256i *space_ptr = (__m256i *)topology_space;
    for (int i = 0; i < (REPRESENTATION_SIZE_WORDS / 8); i++)
    {
        _mm256_store_si256(&space_ptr[i], crease_2);
    }
    return anchor_sift_measure_departure(topology_space);
}

/**
 * @brief Prints the legacy engine's output across unrelated nonces.
 *
 * @param[in] midstate         Chaining value [BORROWS].
 * @param[in] remaining_merkle Merkle tail word.
 * @param[in] timestamp        Header time word.
 * @note A rate measured on a function whose result the compiler can fold to a constant is not a
 *       measurement of that function. This section establishes which case applies to the legacy arm,
 *       before anyone reads a speed number.
 */
void probe_legacy_dependence(const uint32_t *midstate, uint32_t remaining_merkle, uint32_t timestamp)
{
    const uint32_t probes[] = {0u, 1u, 42u, 1249821u, 0x9546a142u, 0xFFFFFFFFu};
    bool all_identical = true;
    uint32_t first = 0u;

    printf("\n----------------------------------------------------------------\n");
    printf("  Does the legacy output depend on the nonce?\n");
    printf("----------------------------------------------------------------\n");

    for (size_t index = 0u; index < (sizeof(probes) / sizeof(probes[0])); index += 1u)
    {
        const uint32_t distance = legacy_distance_for_nonce(midstate, remaining_merkle, timestamp, probes[index]);
        printf("    nonce %10u -> distance %u\n", probes[index], distance);
        if (index == 0u)
        {
            first = distance;
        }
        else if (distance != first)
        {
            all_identical = false;
        }
    }

    if (all_identical)
    {
        printf("\n  Every nonce yields the same distance, %u.\n", first);
        printf("  The reason is in the first three instructions. hash1_vector is\n");
        printf("  _mm256_set1_epi32(base_val), all eight lanes hold one value. A shuffle\n");
        printf("  of a vector whose lanes are equal is that same vector, so crease_1 is\n");
        printf("  v XOR v, which is zero. Everything downstream folds zero into zero.\n");
        printf("\n  So the legacy engine is a constant function. It does not read the nonce,\n");
        printf("  the midstate, the merkle tail or the time. Its speed number below is not\n");
        printf("  a measurement of a pipeline, it is the compiler deleting work it proved\n");
        printf("  had no effect.\n");
    }
    else
    {
        printf("\n  Output varies with the nonce.\n");
    }
}

// ---------------------------------------------------------------------------------------------
// Shared setup
// ---------------------------------------------------------------------------------------------

/**
 * @brief Reads a header field as the number it encodes, little-endian on the wire.
 *
 * @param[in] header Header bytes [BORROWS].
 * @param[in] offset Byte offset of the field.
 * @return           The number the four bytes encode.
 */
uint32_t header_field_value(const uint8_t *header, size_t offset)
{
    return (uint32_t)header[offset] | ((uint32_t)header[offset + 1u] << 8) | ((uint32_t)header[offset + 2u] << 16) |
           ((uint32_t)header[offset + 3u] << 24);
}

/**
 * @brief Reads four header bytes as the big-endian word SHA-256 schedules.
 *
 * @param[in] bytes Four bytes [BORROWS].
 * @return          The schedule word.
 */
uint32_t schedule_word_from_header(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) | ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

/** @brief The measured outcome of one arm, in terms that apply to both arms. */
struct ArmMeasurement
{
    const char *name;
    double seconds;
    uint64_t evaluations;
    double evaluations_per_second;
    double sha256_compressions_per_evaluation;
    double useful_hashes_per_second;
    bool claims_a_win;
    uint32_t claimed_nonce;
    bool claim_verifies;
    const char *verdict;
};

/**
 * @brief Prints one arm's row.
 *
 * @param[in] measurement What to print [BORROWS].
 */
void print_measurement(const ArmMeasurement &measurement)
{
    printf("\n  %s\n", measurement.name);
    printf("    elapsed                   : %.3f s\n", measurement.seconds);
    printf("    evaluations               : %llu\n", (unsigned long long)measurement.evaluations);
    printf("    evaluations per second    : %.2f M/s\n", measurement.evaluations_per_second / 1.0e6);
    printf("    SHA-256 compressions each : %.1f\n", measurement.sha256_compressions_per_evaluation);
    printf("    useful hash rate          : %.2f MH/s\n", measurement.useful_hashes_per_second / 1.0e6);
    printf("    claims a winning nonce    : %s\n", measurement.claims_a_win ? "yes" : "no");
    if (measurement.claims_a_win)
    {
        printf("    claimed nonce             : %u (0x%08x)\n", measurement.claimed_nonce, measurement.claimed_nonce);
        printf("    claim survives verification: %s\n", measurement.claim_verifies ? "yes" : "NO");
    }
    printf("    verdict                   : %s\n", measurement.verdict);
}

/** @brief The shares of the seats of measure_threaded_rate: the request, a count for each seat, and its nonces. */
struct ScanShares
{
    const Sha256ScanRequest *shared; /**< The request of every seat [BORROWS]. */
    uint64_t *counts;                /**< The nonces that each seat evaluated [BORROWS]. */
    uint32_t per_thread;             /**< The nonces of each seat. */
};

/**
 * @brief Measures how the new arm scales across threads, as the client runs it.
 *
 * @param[in] header       Eighty header bytes [BORROWS].
 * @param[in] thread_count How many workers to run.
 * @return                 Hashes per second across all of them.
 */
double measure_threaded_rate(const uint8_t *header, unsigned thread_count)
{
    Sha256ScanRequest shared;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &shared, .bytes = sizeof(shared), .val = 0);
    sha256_header_midstate(&shared.midstate, header);
    shared.merkle_root_tail = schedule_word_from_header(header + 64u);
    shared.ntime = schedule_word_from_header(header + 68u);
    shared.nbits = schedule_word_from_header(header + 72u);
    sha256_target_from_nbits(shared.share_target, header_field_value(header, 72u));

    // the nonces of each seat, at most BENCH_JOBS_MAX of them
    uint64_t counts[BENCH_JOBS_MAX] = {0u};
    if (thread_count > BENCH_JOBS_MAX)
    {
        fprintf(stderr, "bench_engines: %u seats, past the %u the ring holds\n", (unsigned)thread_count,
                BENCH_JOBS_MAX);
        exit(1);
    }
    ScanShares shares = {&shared, counts, BENCH_NONCES / thread_count};

    const auto started = embed_clock_ns();
    bench_jobs_run(
        [](void *context, unsigned seat) {
            const ScanShares *const seat_shares = (const ScanShares *)context;
            Sha256ScanRequest request = *seat_shares->shared;
            Sha256ScanResult result;

            request.nonce_start = seat * seat_shares->per_thread;
            request.nonce_count = seat_shares->per_thread;
            sha256_scan_avx2(&request, &result);
            seat_shares->counts[seat] = result.nonces_evaluated;
        },
        &shares, thread_count, thread_count);
    const auto finished = embed_clock_ns();

    uint64_t total = 0u;
    for (unsigned seat = 0u; seat < thread_count; seat += 1u)
    {
        total += counts[seat];
    }
    const double seconds = ((double)(finished - started) / 1.0e9);
    return (seconds > 0.0) ? ((double)total / seconds) : 0.0;
}

} // namespace

int main()
{
    printf("================================================================\n");
    printf("  Engine comparison: legacy topological arm vs SHA-256 arm\n");
    printf("  Same header, same nonce range, one thread each.\n");
    printf("  Header: block 125552, nonce 0x%08x on the record.\n", BENCH_EXPECTED_NONCE);
    printf("================================================================\n");

    const BenchHexBytes header = bench_hex_bytes(BENCH_HEADER_HEX);
    Sha256State midstate;
    sha256_header_midstate(&midstate, header.at);

    const uint32_t merkle_tail = schedule_word_from_header(header.at + 64u);
    const uint32_t ntime_word = schedule_word_from_header(header.at + 68u);
    const uint32_t nbits_numeric = header_field_value(header.at, 72u);

    // The window straddles the recorded nonce, an engine that finds it is finding it.
    const uint32_t window_start = BENCH_EXPECTED_NONCE - (BENCH_NONCES / 2u);

    probe_legacy_dependence(midstate.word, merkle_tail, ntime_word);

    ArmMeasurement measurements[2];
    unsigned measured = 0u;

    // --- Legacy arm -----------------------------------------------------------------------
    {
        LegacyResult legacy;
        const auto started = embed_clock_ns();
        run_legacy_engine(midstate.word, merkle_tail, ntime_word, nbits_numeric, window_start,
                          window_start + BENCH_NONCES, &legacy);
        const auto finished = embed_clock_ns();

        ArmMeasurement measurement = {};
        measurement.name = "legacy topological arm (commit 6e4747c, verbatim)";
        measurement.seconds = ((double)(finished - started) / 1.0e9);
        measurement.evaluations = legacy.evaluations;
        measurement.evaluations_per_second =
            (measurement.seconds > 0.0) ? ((double)legacy.evaluations / measurement.seconds) : 0.0;
        measurement.sha256_compressions_per_evaluation = 0.0;
        measurement.useful_hashes_per_second = 0.0;
        measurement.claims_a_win = (legacy.block_found != 0);
        measurement.claimed_nonce = legacy.winning_nonce;

        // Check the legacy claim the only way that means anything: hash the header it names.
        measurement.claim_verifies = false;
        if (legacy.block_found != 0)
        {
            BenchHexBytes candidate = header;
            candidate.at[76] = (uint8_t)legacy.winning_nonce;
            candidate.at[77] = (uint8_t)(legacy.winning_nonce >> 8);
            candidate.at[78] = (uint8_t)(legacy.winning_nonce >> 16);
            candidate.at[79] = (uint8_t)(legacy.winning_nonce >> 24);

            uint8_t digest[32];
            uint32_t block_target[SHA256_STATE_WORDS];
            sha256_double_hash(candidate.at, candidate.count, digest);
            sha256_target_from_nbits(block_target, nbits_numeric);
            measurement.claim_verifies = (sha256_digest_within_target(digest, block_target) != 0);
        }
        measurement.verdict =
            measurement.claims_a_win
                ? (measurement.claim_verifies ? "found a real nonce" : "claimed a nonce that is not a share")
                : "no claim";
        measurements[measured] = measurement;
        measured += 1u;
        printf("\n  (legacy checksum %08x, printed so the loop cannot be optimized away)\n", legacy.checksum);
    }

    // --- SHA-256 arm ----------------------------------------------------------------------
    {
        Sha256ScanRequest request;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &request, .bytes = sizeof(request), .val = 0);
        request.midstate = midstate;
        request.merkle_root_tail = merkle_tail;
        request.ntime = ntime_word;
        request.nbits = schedule_word_from_header(header.at + 72u);
        request.nonce_start = window_start;
        request.nonce_count = BENCH_NONCES;
        sha256_target_from_nbits(request.share_target, nbits_numeric);

        Sha256ScanResult result;
        const auto started = embed_clock_ns();
        sha256_scan_avx2(&request, &result);
        const auto finished = embed_clock_ns();

        ArmMeasurement measurement = {};
        measurement.name = "SHA-256 eight-lane arm with anchor early exit";
        measurement.seconds = ((double)(finished - started) / 1.0e9);
        measurement.evaluations = result.nonces_evaluated;
        measurement.evaluations_per_second =
            (measurement.seconds > 0.0) ? ((double)result.nonces_evaluated / measurement.seconds) : 0.0;
        // Two compressions per nonce: the header tail block and the digest block.
        measurement.sha256_compressions_per_evaluation = 2.0;
        measurement.useful_hashes_per_second = measurement.evaluations_per_second;
        measurement.claims_a_win = (result.found != 0);
        measurement.claimed_nonce = result.winning_nonce;

        measurement.claim_verifies = false;
        if (result.found != 0)
        {
            BenchHexBytes candidate = header;
            candidate.at[76] = (uint8_t)result.winning_nonce;
            candidate.at[77] = (uint8_t)(result.winning_nonce >> 8);
            candidate.at[78] = (uint8_t)(result.winning_nonce >> 16);
            candidate.at[79] = (uint8_t)(result.winning_nonce >> 24);

            uint8_t digest[32];
            uint32_t block_target[SHA256_STATE_WORDS];
            sha256_double_hash(candidate.at, candidate.count, digest);
            sha256_target_from_nbits(block_target, nbits_numeric);
            measurement.claim_verifies = (sha256_digest_within_target(digest, block_target) != 0);
        }
        measurement.verdict = measurement.claims_a_win
                                  ? (measurement.claim_verifies ? "found the recorded nonce and it verifies"
                                                                : "claimed a nonce that is not a share")
                                  : "no claim";
        measurements[measured] = measurement;
        measured += 1u;
    }

    printf("\n----------------------------------------------------------------\n");
    printf("  Per-core measurements\n");
    printf("----------------------------------------------------------------\n");
    for (unsigned at = 0u; at < measured; at += 1u)
    {
        print_measurement(measurements[at]);
    }

    const double legacy_rate = measurements[0].evaluations_per_second;
    const double real_rate = measurements[1].evaluations_per_second;

    printf("\n----------------------------------------------------------------\n");
    printf("  Reading the numbers\n");
    printf("----------------------------------------------------------------\n");
    printf("\n  Raw iteration rate  : legacy is %.1fx the SHA-256 arm.\n",
           (real_rate > 0.0) ? (legacy_rate / real_rate) : 0.0);
    printf("  Work per iteration  : legacy 0 SHA-256 compressions, new arm 2.\n");
    printf("  Useful hash rate    : legacy %.2f MH/s, new arm %.2f MH/s.\n", 0.0, real_rate / 1.0e6);
    printf("\n  The legacy arm is faster per iteration because it skips the hash. It\n");
    printf("  computes midstate[0] xor nonce, folds that once, stores it 128 times and\n");
    printf("  reduces it. No message schedule, no round constants, no rounds. Its\n");
    printf("  iteration rate is therefore not a hash rate and does not convert to one.\n");

    printf("\n  Share correctness, which is the axis that decides earnings:\n");
    printf("    legacy : %s\n", measurements[0].verdict);
    printf("    new arm: %s\n", measurements[1].verdict);

    if (measurements[0].claims_a_win && !measurements[0].claim_verifies)
    {
        printf("\n  The legacy arm claimed nonce %u on the first iteration of the window.\n",
               measurements[0].claimed_nonce);
        printf("  Its test is distance < compact_target where distance is masked to 16 bits\n");
        printf("  (at most 65535) and compact_target is nbits (%u). That comparison is\n", nbits_numeric);
        printf("  true for every nonce, so it reports a win immediately, every time.\n");
        printf("  At a pool every one of those is a rejected share.\n");
    }

    printf("\n----------------------------------------------------------------\n");
    printf("  Thread scaling of the SHA-256 arm\n");
    printf("----------------------------------------------------------------\n");
    const unsigned available = (bench_processors() > 0u) ? bench_processors() : 8u;
    const unsigned ladder[] = {1u, 2u, 4u, 8u, available};
    for (unsigned thread_count : ladder)
    {
        if ((thread_count == 0u) || (thread_count > available))
        {
            continue;
        }
        const double rate = measure_threaded_rate(header.at, thread_count);
        printf("    %2u thread(s): %8.2f MH/s\n", thread_count, rate / 1.0e6);
    }

    printf("\n================================================================\n");
    return 0;
}

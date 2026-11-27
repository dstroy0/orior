// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_cuda_v2.cpp
 * @brief Checks the v2 device arms against the same published blocks as the CPU arm, then times
 *        them. The v2 twin of bench_cuda.cpp, with the arms named as v2 exposes them.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note v2 exposes three sound arms. The raw arm hashes every nonce in full, the pure search. The
 *       anchor arm reaches the identical condition to win through the round-61 early-out. The free arm
 *       runs every probe unconditionally, and branches one time on the combined result.
 * @note The bench checks every arm against the recorded nonces and the CPU arm before it reports a
 *       rate. A device arm that is fast and wrong is worth less than no device arm.
 */

#include "cuda_miner.h"
#include "memoria_operor/memoria_operor.h"
#include "sha256_core.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace
{

int g_tests_run = 0;
int g_tests_failed = 0;

/** @brief Block 125552, whose header and nonce the chain recorded. */
const char *const BENCH_HEADER_HEX =
    "0100000081cd02ab7e569e8bcd9317e2fe99f2de44d49ab2b8851ba4a308000000000000e320b6c2fffc8d750423"
    "db8b1eb942ae710e951ed797f7affc8892b0f1fc122bc7f5d74df2b9441a42a14695";

/** @brief The nonce of block 125552. */
const uint32_t BENCH_EXPECTED_NONCE = 0x9546a142u;

/** @brief The genesis block header. */
const char *const GENESIS_HEADER_HEX =
    "0100000000000000000000000000000000000000000000000000000000000000000000003ba3edfd7a7b12b27ac7"
    "2c3e67768f617fc81bc3888a51323a9fb8aa4b1e5e4a29ab5f49ffff001d1dac2b7c";

/** @brief The nonce of the genesis block. */
const uint32_t GENESIS_EXPECTED_NONCE = 0x7c2bac1du;

/**
 * @brief One way to search a range. The bench checks it against the record, and then times it.
 *
 * @note Each of the first three arms adds one gain to the arm before it. Thus a difference in rate
 *       between neighbors is that gain alone. From raw to shared front, the gain is the rounds and
 *       schedule words that no nonce reaches, which move to the host. From shared front to shared
 *       front with anchor, the gain is the round-61 early-out.
 * @note The naive arm hashes each nonce in full. The inorder arm carries the shared front, and the
 *       anchor too where anchor_is_sound stays on. The free arm carries the shared front and runs every
 *       probe. Its neighbor is the anchored arm, and their ratio is the cost of no early-out.
 */
struct BenchArm
{
    const char *name; /**< What to print in a row. */
    int (*scan)(CudaMinerContext *context, const CudaScanParameters *parameters,
                uint32_t nonce_base, uint32_t nonce_count, uint32_t *found_nonces,
                uint32_t *found_count); /**< The entry that searches. */
    int anchor; /**< Zero forces the anchor off. Nonzero leaves it as the target allows. */
};

/** @brief The arms, in the order in which each adds one gain to the last, then the free arm. */
const BenchArm BENCH_ARMS[] = {
    {"raw", cuda_miner_scan_naive, 0},
    {"shared front", cuda_miner_scan_inorder, 0},
    {"shared front + anchor", cuda_miner_scan_inorder, 1},
    {"free, all probes", cuda_miner_scan_free, 1},
};

/** @brief How many arms BENCH_ARMS holds. */
const size_t BENCH_ARM_COUNT = sizeof(BENCH_ARMS) / sizeof(BENCH_ARMS[0]);

/**
 * @brief Timed rounds per arm, interleaved across arms.
 *
 * @note Interleaved. Thus a drift in the temperature or clock of the card over the run lands on every
 *       arm alike, and not on the last arm. The bench prints every round and compares the medians.
 *       Thus one disturbed round shows as an outlier, and does not move the comparison.
 */
const int BENCH_ROUNDS = 5;

/**
 * @brief Converts a hex string to bytes.
 *
 * @param[in] hex Characters to convert.
 * @return        The bytes that they encode.
 */
std::vector<uint8_t> bytes_from_hex(const std::string &hex)
{
    std::vector<uint8_t> bytes;

    bytes.reserve(hex.size() / 2u);
    for (size_t at = 0u; (at + 1u) < hex.size(); at += 2u)
    {
        bytes.push_back((uint8_t)std::stoul(hex.substr(at, 2u), nullptr, 16));
    }
    return bytes;
}

/**
 * @brief Reads a header field as the number it encodes, little-endian on the wire.
 *
 * @param[in] header Header bytes [BORROWS].
 * @param[in] offset Byte offset of the field.
 * @return           The number the four bytes encode.
 */
uint32_t header_field_value(const uint8_t *header, size_t offset)
{
    return (uint32_t)header[offset] | ((uint32_t)header[offset + 1u] << 8) |
           ((uint32_t)header[offset + 2u] << 16) | ((uint32_t)header[offset + 3u] << 24);
}

/**
 * @brief Reads four header bytes as the big-endian word SHA-256 schedules.
 *
 * @param[in] bytes Four bytes [BORROWS].
 * @return          The schedule word.
 */
uint32_t schedule_word_from_header(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) | ((uint32_t)bytes[2] << 8) |
           (uint32_t)bytes[3];
}

/**
 * @brief Records one test outcome and prints it.
 *
 * @param[in] name   The name of the test.
 * @param[in] passed Nonzero where the test passed.
 * @param[in] detail The observed result, printed on failure.
 */
void report(const std::string &name, bool passed, const std::string &detail = "")
{
    g_tests_run += 1;
    if (passed)
    {
        std::printf("  [PASS] %s\n", name.c_str());
    }
    else
    {
        g_tests_failed += 1;
        std::printf("  [FAIL] %s\n", name.c_str());
        if (!detail.empty())
        {
            std::printf("         %s\n", detail.c_str());
        }
    }
}

/**
 * @brief Builds device parameters from a published header.
 *
 * @param[out] parameters Where they land [BORROWS].
 * @param[in]  header     Eighty header bytes [BORROWS].
 */
void parameters_from_header(CudaScanParameters *parameters, const uint8_t *header)
{
    Sha256State midstate;

    EMBED_CALL(memor.set, MemoriaCfg, .dst = parameters, .bytes = sizeof(*parameters), .val = 0);
    sha256_header_midstate(&midstate, header);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = parameters->midstate, .src = midstate.word,
               .bytes = sizeof(parameters->midstate));

    parameters->merkle_root_tail = schedule_word_from_header(header + 64u);
    parameters->ntime = schedule_word_from_header(header + 68u);
    parameters->nbits = schedule_word_from_header(header + 72u);
    sha256_target_from_nbits(parameters->share_target, header_field_value(header, 72u));
    parameters->anchor_is_sound = (parameters->share_target[0] == 0u) ? 1u : 0u;
}

/**
 * @brief Requires the device to recover a nonce the chain recorded.
 *
 * @param[in] context     Device buffers [BORROWS].
 * @param[in] arm         The arm to hold to the record [BORROWS].
 * @param[in] header_hex  The published header.
 * @param[in] expected    The nonce on the record.
 * @param[in] description What to call it.
 */
void test_device_finds_nonce(CudaMinerContext *context, const BenchArm &arm,
                             const char *header_hex, uint32_t expected, const char *description)
{
    const std::vector<uint8_t> header = bytes_from_hex(header_hex);
    CudaScanParameters parameters;
    parameters_from_header(&parameters, header.data());
    if (arm.anchor == 0)
    {
        parameters.anchor_is_sound = 0u;
    }

    const uint32_t window = 1u << 20;
    const uint32_t base = expected - (window / 2u);

    uint32_t found_nonces[16];
    uint32_t found_count = 0u;

    if (arm.scan(context, &parameters, base, window, found_nonces, &found_count) == 0)
    {
        report(std::string(arm.name) + ": device scan of " + description, false, "launch failed");
        return;
    }

    bool matched = false;
    for (uint32_t at = 0u; at < found_count; at += 1u)
    {
        if (found_nonces[at] == expected)
        {
            matched = true;
        }
    }
    report(std::string(arm.name) + ": finds nonce of " + description, matched,
           found_count ? ("found " + std::to_string(found_count) + " winner(s), none matching")
                       : "FALSE NEGATIVE: no nonce found in a window containing one");
}

/**
 * @brief Requires the device and the CPU to agree nonce for nonce over a range with many winners.
 *
 * @param[in] context Device buffers [BORROWS].
 * @param[in] arm     The arm to hold to the CPU arm [BORROWS].
 */
void test_device_agrees_with_cpu(CudaMinerContext *context, const BenchArm &arm)
{
    const std::vector<uint8_t> header = bytes_from_hex(BENCH_HEADER_HEX);
    CudaScanParameters parameters;
    parameters_from_header(&parameters, header.data());

    // A target loose enough that both arms find many winners, agreement is a real claim. Sixteen
    // leading zero bits puts the top word above zero, which also exercises the unsound-anchor path
    // on both sides.
    EMBED_CALL(memor.set, MemoriaCfg, .dst = parameters.share_target, .bytes = sizeof(parameters.share_target),
               .val = 0xFF);
    parameters.share_target[0] = 0x0000FFFFu;
    parameters.anchor_is_sound = 0u;

    const uint32_t window = 1u << 20;
    uint32_t found_nonces[4096];
    uint32_t found_count = 0u;

    if (arm.scan(context, &parameters, 0u, window, found_nonces, &found_count) == 0)
    {
        report(std::string(arm.name) + ": device and CPU agree at an easy target", false,
               "launch failed");
        return;
    }

    Sha256ScanRequest request;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &request, .bytes = sizeof(request), .val = 0);
    Sha256State midstate;
    sha256_header_midstate(&midstate, header.data());
    request.midstate = midstate;
    request.merkle_root_tail = parameters.merkle_root_tail;
    request.ntime = parameters.ntime;
    request.nbits = parameters.nbits;
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = request.share_target, .src = parameters.share_target,
               .bytes = sizeof(request.share_target));

    // Walk the CPU arm across the same window. Collect every winner, and not only the first.
    std::vector<uint32_t> cpu_winners;
    uint32_t cursor = 0u;
    while (cursor < window)
    {
        request.nonce_start = cursor;
        request.nonce_count = window - cursor;

        Sha256ScanResult result;
        sha256_scan_avx2(&request, &result);
        if (result.found == 0)
        {
            break;
        }
        cpu_winners.push_back(result.winning_nonce);
        cursor = result.winning_nonce + 1u;
    }

    std::vector<uint32_t> device_winners(found_nonces, found_nonces + found_count);
    std::sort(device_winners.begin(), device_winners.end());
    std::sort(cpu_winners.begin(), cpu_winners.end());

    report(std::string(arm.name) + ": device and CPU find the same winner count",
           device_winners.size() == cpu_winners.size(),
           "device " + std::to_string(device_winners.size()) + ", cpu " +
               std::to_string(cpu_winners.size()));
    report(std::string(arm.name) + ": device and CPU find the same winners",
           device_winners == cpu_winners, "the two arms disagree on which nonces win");
    std::printf("         (%zu winners over %u nonces at a 2^-16 target)\n", cpu_winners.size(),
                window);
}

/**
 * @brief Times one arm over one round of launches large enough to hide launch overhead.
 *
 * @param[in]  context Device buffers [BORROWS].
 * @param[in]  arm     The arm to time [BORROWS].
 * @param[out] hashes  Nonces the round covered [BORROWS].
 * @return             Seconds the round took, or a negative value where a launch failed.
 * @note Every arm covers the same nonces of the same header in every round. Thus the work is
 *       identical, and only the arm differs.
 */
double time_arm_round(CudaMinerContext *context, const BenchArm &arm, double *hashes)
{
    const std::vector<uint8_t> header = bytes_from_hex(BENCH_HEADER_HEX);
    CudaScanParameters parameters;
    parameters_from_header(&parameters, header.data());
    if (arm.anchor == 0)
    {
        parameters.anchor_is_sound = 0u;
    }

    const uint32_t batch = 1u << 24;
    const int batches = 64;
    uint32_t found_nonces[16];
    uint32_t found_count = 0u;

    const auto started = std::chrono::steady_clock::now();
    for (int index = 0; index < batches; index += 1)
    {
        if (arm.scan(context, &parameters, (uint32_t)index * batch, batch, found_nonces,
                     &found_count) == 0)
        {
            return -1.0;
        }
    }
    const auto finished = std::chrono::steady_clock::now();

    *hashes = (double)batch * (double)batches;
    return std::chrono::duration<double>(finished - started).count();
}

/**
 * @brief Times every arm, interleaved, and prints every round with its parts and each median.
 *
 * @param[in]  context Device buffers [BORROWS].
 * @param[out] medians One median rate per arm, hashes per second [BORROWS].
 * @return             Nonzero where every launch succeeded.
 */
int measure_arm_rates(CudaMinerContext *context, double *medians)
{
    std::vector<std::vector<double>> rates(BENCH_ARM_COUNT);
    double warm_hashes = 0.0;

    // One untimed round per arm. Thus the first timed round measures the kernel, and not the setup.
    for (size_t which = 0u; which < BENCH_ARM_COUNT; which += 1u)
    {
        if (time_arm_round(context, BENCH_ARMS[which], &warm_hashes) < 0.0)
        {
            return 0;
        }
    }

    for (int round = 0; round < BENCH_ROUNDS; round += 1)
    {
        for (size_t which = 0u; which < BENCH_ARM_COUNT; which += 1u)
        {
            double hashes = 0.0;
            const double seconds = time_arm_round(context, BENCH_ARMS[which], &hashes);
            if (seconds <= 0.0)
            {
                return 0;
            }
            rates[which].push_back(hashes / seconds);
            std::printf("  round %d  %-22s %.0f hashes in %.4f s = %.2f MH/s\n", round + 1,
                        BENCH_ARMS[which].name, hashes, seconds, hashes / seconds / 1.0e6);
        }
    }

    for (size_t which = 0u; which < BENCH_ARM_COUNT; which += 1u)
    {
        std::vector<double> sorted = rates[which];
        std::sort(sorted.begin(), sorted.end());
        medians[which] = sorted[sorted.size() / 2u];
    }
    return 1;
}

} // namespace

int main()
{
    char device_text[256];
    const int multiprocessors = cuda_miner_device_report(device_text, sizeof(device_text));

    std::printf("================================================================\n");
    std::printf("  CUDA v2 arm validation and benchmark\n");
    std::printf("  device: %s\n", device_text);
    std::printf("================================================================\n");

    if (multiprocessors == 0)
    {
        std::printf("\nNo CUDA device. Nothing to test.\n");
        return 1;
    }

    CudaMinerContext *const context = cuda_miner_create(4096u);
    if (context == nullptr)
    {
        std::printf("\nCould not allocate device buffers.\n");
        return 1;
    }

    std::printf("\n[1] Recovering nonces the chain recorded, every arm\n");
    for (size_t which = 0u; which < BENCH_ARM_COUNT; which += 1u)
    {
        test_device_finds_nonce(context, BENCH_ARMS[which], GENESIS_HEADER_HEX,
                                GENESIS_EXPECTED_NONCE, "genesis block, height 0");
        test_device_finds_nonce(context, BENCH_ARMS[which], BENCH_HEADER_HEX,
                                BENCH_EXPECTED_NONCE, "block 125552");
    }

    std::printf("\n[2] Agreement with the CPU arm, every arm\n");
    for (size_t which = 0u; which < BENCH_ARM_COUNT; which += 1u)
    {
        test_device_agrees_with_cpu(context, BENCH_ARMS[which]);
    }

    if (g_tests_failed != 0)
    {
        std::printf("\n%d of %d checks failed. Not reporting a rate for an arm that is wrong.\n",
                    g_tests_failed, g_tests_run);
        cuda_miner_destroy(context);
        return 1;
    }

    std::printf("\n[3] Measured rates, %d interleaved rounds, block 125552's header\n", BENCH_ROUNDS);
    double medians[BENCH_ARM_COUNT];
    if (measure_arm_rates(context, medians) == 0)
    {
        std::printf("  a timed launch failed, so no rate is reported\n");
        cuda_miner_destroy(context);
        return 1;
    }

    std::printf("\n[4] Medians, and what each saving buys\n");
    for (size_t which = 0u; which < BENCH_ARM_COUNT; which += 1u)
    {
        std::printf("  %-22s %.2f MH/s\n", BENCH_ARMS[which].name, medians[which] / 1.0e6);
    }
    for (size_t which = 1u; which < BENCH_ARM_COUNT; which += 1u)
    {
        std::printf("  %-22s over %-22s %.2f / %.2f MH/s = %.4f\n", BENCH_ARMS[which].name,
                    BENCH_ARMS[which - 1u].name, medians[which] / 1.0e6,
                    medians[which - 1u] / 1.0e6, medians[which] / medians[which - 1u]);
    }
    std::printf("  %-22s over %-22s %.2f / %.2f MH/s = %.4f\n",
                BENCH_ARMS[BENCH_ARM_COUNT - 1u].name, BENCH_ARMS[0].name,
                medians[BENCH_ARM_COUNT - 1u] / 1.0e6, medians[0] / 1.0e6,
                medians[BENCH_ARM_COUNT - 1u] / medians[0]);

    std::printf("\n================================================================\n");
    std::printf("  %d run, %d failed\n", g_tests_run, g_tests_failed);
    std::printf("================================================================\n");

    cuda_miner_destroy(context);
    return 0;
}

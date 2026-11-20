// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file harness.cpp
 * @brief A multi-core CPU mining harness on a real block header, with no pool and no device.
 *
 * @note The harness mines a published header from its real midstate, over a nonce range that holds the
 *       nonce that the chain recorded. Every core takes one slice of the range. The lowest nonce that meets
 *       the block target must equal the recorded nonce, or the harness fails.
 * @note The arm is the eight-lane AVX2 arm where the processor has AVX2, and the scalar anchor arm
 *       otherwise. Thus the same source runs on x86 and on AArch64, such as a Raspberry Pi 5.
 * @note Build on x86 with -mavx2 for the eight-lane arm. Build on AArch64 without it.
 */
#include "sha256_core.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace
{

/** @brief One published header and the nonce that the chain recorded for it. */
struct PublishedHeader
{
    const char *header_hex;
    uint32_t recorded_nonce;
    const char *source;
};

/** @brief Two headers that anyone can check against a block explorer. */
const PublishedHeader PUBLISHED_HEADERS[] = {
    {"0100000000000000000000000000000000000000000000000000000000000000000000003ba3edfd7a7b12b27ac7"
     "2c3e67768f617fc81bc3888a51323a9fb8aa4b1e5e4a29ab5f49ffff001d1dac2b7c",
     0x7c2bac1du, "genesis block, height 0"},
    {"0100000081cd02ab7e569e8bcd9317e2fe99f2de44d49ab2b8851ba4a308000000000000e320b6c2fffc8d750423"
     "db8b1eb942ae710e951ed797f7affc8892b0f1fc122bc7f5d74df2b9441a42a14695",
     0x9546a142u, "block 125552"},
};

/**
 * @brief Converts a hex string to bytes.
 *
 * @param[in]  hex   Characters to convert, an even count [BORROWS].
 * @param[out] bytes Receives the bytes [BORROWS].
 */
void bytes_from_hex(const char *hex, std::vector<uint8_t> &bytes)
{
    const size_t length = std::strlen(hex);
    bytes.clear();
    for (size_t at = 0u; (at + 1u) < length; at += 2u)
    {
        const char pair[3] = {hex[at], hex[at + 1u], '\0'};
        // strtoul returns at most 0xFF for two hex digits. The cast narrows that to one byte and loses nothing.
        bytes.push_back((uint8_t)std::strtoul(pair, nullptr, 16));
    }
}

/**
 * @brief Reads four header bytes as one big-endian word, the order in which SHA-256 reads its message.
 *
 * @param[in] bytes Four bytes to read [BORROWS].
 * @return          The schedule word that they encode.
 */
uint32_t schedule_word(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24u) | ((uint32_t)bytes[1] << 16u) | ((uint32_t)bytes[2] << 8u) | (uint32_t)bytes[3];
}

/**
 * @brief Reads four header bytes as the little-endian number that the header stores.
 *
 * @param[in] bytes Four bytes to read [BORROWS].
 * @return          The number that the four bytes encode.
 */
uint32_t header_number(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8u) | ((uint32_t)bytes[2] << 16u) | ((uint32_t)bytes[3] << 24u);
}

/** @brief What one core reports back. */
struct SliceResult
{
    Sha256ScanResult scan;
    uint32_t nonce_start;
};

} // namespace

int main(int argument_count, char **arguments)
{
    unsigned threads = std::thread::hardware_concurrency();
    uint32_t span = 1u << 26u;
    unsigned which = 0u;
    for (int index = 1; index < argument_count; index += 1)
    {
        const std::string argument = arguments[index];
        const bool has_value = (index + 1) < argument_count;
        if ((argument == "--threads") && has_value)
        {
            index += 1;
            threads = (unsigned)std::strtoul(arguments[index], nullptr, 10);
        }
        else if ((argument == "--span") && has_value)
        {
            index += 1;
            span = (uint32_t)std::strtoul(arguments[index], nullptr, 0);
        }
        else if ((argument == "--header") && has_value)
        {
            index += 1;
            which = (unsigned)std::strtoul(arguments[index], nullptr, 10);
        }
        else
        {
            std::printf("usage: harness [--threads N] [--span NONCES] [--header 0|1]\n");
            return 2;
        }
    }
    if (threads == 0u)
    {
        threads = 1u;
    }
    if (which >= (sizeof(PUBLISHED_HEADERS) / sizeof(PUBLISHED_HEADERS[0])))
    {
        std::printf("--header must be 0 or 1\n");
        return 2;
    }
    const PublishedHeader &published = PUBLISHED_HEADERS[which];

    std::vector<uint8_t> header;
    bytes_from_hex(published.header_hex, header);

    Sha256ScanRequest request;
    std::memset(&request, 0, sizeof(request));
    sha256_header_midstate(&request.midstate, header.data());
    request.merkle_root_tail = schedule_word(header.data() + 64u);
    request.ntime = schedule_word(header.data() + 68u);
    request.nbits = schedule_word(header.data() + 72u);
    // The target comes from nbits as the header stores it, little-endian, and not from the schedule word.
    sha256_target_from_nbits(request.share_target, header_number(header.data() + 72u));

    // A build without AVX2 compiles the eight-lane arm as a call to the reference arm. Thus the vector arm
    // needs AVX2 at compile time and on the processor. Build the core and the harness with the same flags.
#if defined(__AVX2__)
    const bool vector_arm = sha256_has_avx2() != 0;
#else
    const bool vector_arm = false;
#endif
    void (*const scan)(const Sha256ScanRequest *, Sha256ScanResult *) =
        vector_arm ? sha256_scan_avx2 : sha256_scan_scalar_early;

    // The range has the recorded nonce at its center. Unsigned arithmetic wraps mod 2^32, and the range wraps
    // with it.
    const uint32_t range_start = published.recorded_nonce - (span / 2u);
    const uint32_t slice = span / threads;

    std::printf("harness: %s\n", published.source);
    std::printf("  arm: %s, threads: %u, nonces: %u from 0x%08x\n", vector_arm ? "eight-lane AVX2" : "scalar anchor",
                threads, slice * threads, range_start);

    std::vector<SliceResult> results(threads);
    std::vector<std::thread> workers;
    const auto started = std::chrono::steady_clock::now();
    for (unsigned core = 0u; core < threads; core += 1u)
    {
        workers.emplace_back([&, core]() {
            Sha256ScanRequest own = request;
            own.nonce_start = range_start + (core * slice);
            own.nonce_count = slice;
            results[core].nonce_start = own.nonce_start;
            scan(&own, &results[core].scan);
        });
    }
    for (std::thread &worker : workers)
    {
        worker.join();
    }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();

    // The lowest winner is the first in range order. The slices ascend from range_start, and each slice
    // reports its own lowest winner. Thus the first slice with a winner holds the lowest.
    uint64_t evaluated = 0u;
    bool found = false;
    uint32_t lowest = 0u;
    for (const SliceResult &result : results)
    {
        evaluated += result.scan.nonces_evaluated;
        if ((!found) && (result.scan.found != 0))
        {
            found = true;
            lowest = result.scan.winning_nonce;
        }
    }

    std::printf("  evaluated: %llu nonces in %.3f s\n", (unsigned long long)evaluated, seconds);
    std::printf("  rate: %.2f MH/s\n", ((double)evaluated / seconds) / 1.0e6);
    if (!found)
    {
        std::printf("  FAIL: no nonce in a range that holds 0x%08x\n", published.recorded_nonce);
        return 1;
    }
    std::printf("  found 0x%08x, recorded 0x%08x: %s\n", lowest, published.recorded_nonce,
                (lowest == published.recorded_nonce) ? "PASS" : "FAIL");
    return (lowest == published.recorded_nonce) ? 0 : 1;
}

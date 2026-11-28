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
 * @note With --telemetry FILE, the harness rewrites FILE twice a second while it scans, and one last time at
 *       the end. The file holds one counter per line: nonces evaluated, anchor survivors, slices with a
 *       winner, and seconds. Put FILE under build/miner.
 */
#include "../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "sha256_core.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
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
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = hex, .cap = PLATFORM_TEXT_TO_TERMINATOR);
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

/** @brief Nonces per scan call. A slice scans in chunks of this size, and the counters advance after each chunk. */
constexpr uint32_t CHUNK_NONCES = 1u << 20u;

/** @brief Milliseconds between two writes of the telemetry file. */
constexpr unsigned TELEMETRY_PERIOD_MS = 500u;

/** @brief What one core reports back: the lowest winner in its slice, where it found one. */
struct SliceResult
{
    bool found = false;
    uint32_t winning_nonce = 0u;
};

/** @brief Counters that every core adds to while it scans, and that the telemetry file reports. */
struct Telemetry
{
    std::atomic<uint64_t> evaluated{0u}; /**< Nonces that the cores hashed. */
    std::atomic<uint64_t> survivors{0u}; /**< Nonces that passed the anchor and reached the exact compare. */
    std::atomic<uint64_t> winners{0u};   /**< Slices that found a nonce at or below the block target. */
};

/**
 * @brief Writes the counters to the telemetry file, one value per line.
 *
 * @param[in] path      The telemetry file [BORROWS].
 * @param[in] telemetry The counters [BORROWS].
 * @param[in] seconds   Seconds since the scan started.
 * @note The function writes a side file and then renames it over the telemetry file. Thus a reader never
 *       sees half a file. The rename on Windows fails where the target exists. Thus the function removes
 *       the target first, and a reader can find no file for that short time.
 */
void write_telemetry(const std::string &path, const Telemetry &telemetry, double seconds)
{
    const std::string partial = path + ".part";
    FILE *const file = std::fopen(partial.c_str(), "w");
    if (file == nullptr)
    {
        return;
    }
    std::fprintf(file, "evaluated %llu\nsurvivors %llu\nwinners %llu\nseconds %.3f\n",
                 (unsigned long long)telemetry.evaluated.load(), (unsigned long long)telemetry.survivors.load(),
                 (unsigned long long)telemetry.winners.load(), seconds);
    std::fclose(file);
    std::remove(path.c_str());
    std::rename(partial.c_str(), path.c_str());
}

} // namespace

int program_main(int argument_count, char **arguments)
{
    unsigned threads = std::thread::hardware_concurrency();
    uint32_t span = 1u << 26u;
    unsigned which = 0u;
    std::string telemetry_path;
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
        else if ((argument == "--telemetry") && has_value)
        {
            index += 1;
            telemetry_path = arguments[index];
        }
        else
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line,
                              "usage: harness [--threads N] [--span NONCES] [--header 0|1] [--telemetry FILE]");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            return 2;
        }
    }
    if (threads == 0u)
    {
        threads = 1u;
    }
    if (which >= (sizeof(PUBLISHED_HEADERS) / sizeof(PUBLISHED_HEADERS[0])))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "--header must be 0 or 1");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 2;
    }
    const PublishedHeader &published = PUBLISHED_HEADERS[which];

    std::vector<uint8_t> header;
    bytes_from_hex(published.header_hex, header);

    Sha256ScanRequest request;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &request, .bytes = sizeof(request), .val = 0);
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

    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "harness: ");
        mmgr_line_put(output_line, published.source);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  arm: ");
        mmgr_line_put(output_line, vector_arm ? "eight-lane AVX2" : "scalar anchor");
        mmgr_line_put(output_line, ", threads: ");
        mmgr_line_decimal(output_line, threads, 1u);
        mmgr_line_put(output_line, ", nonces: ");
        mmgr_line_decimal(output_line, slice * threads, 1u);
        mmgr_line_put(output_line, " from 0x");
        mmgr_line_hex(output_line, range_start, 8u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    std::vector<SliceResult> results(threads);
    std::vector<std::thread> workers;
    Telemetry telemetry;
    std::atomic<bool> finished{false};
    const auto started = std::chrono::steady_clock::now();
    const auto elapsed = [&started]() {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    };

    for (unsigned core = 0u; core < threads; core += 1u)
    {
        workers.emplace_back([&, core]() {
            Sha256ScanRequest own = request;
            const uint32_t slice_start = range_start + (core * slice);
            uint32_t done = 0u;
            // The chunks ascend, and each scan call reports its own lowest winner. Thus the first chunk
            // with a winner holds the lowest winner of the slice, and the slice stops there.
            while (done < slice)
            {
                own.nonce_start = slice_start + done;
                own.nonce_count = ((slice - done) < CHUNK_NONCES) ? (slice - done) : CHUNK_NONCES;
                Sha256ScanResult chunk;
                scan(&own, &chunk);
                telemetry.evaluated += chunk.nonces_evaluated;
                telemetry.survivors += chunk.anchors_survived;
                if (chunk.found != 0)
                {
                    results[core].found = true;
                    results[core].winning_nonce = chunk.winning_nonce;
                    telemetry.winners += 1u;
                    break;
                }
                done += own.nonce_count;
            }
        });
    }

    std::thread writer;
    if (!telemetry_path.empty())
    {
        writer = std::thread([&]() {
            while (!finished.load())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(TELEMETRY_PERIOD_MS));
                write_telemetry(telemetry_path, telemetry, elapsed());
            }
        });
    }
    for (std::thread &worker : workers)
    {
        worker.join();
    }
    const double seconds = elapsed();
    finished.store(true);
    if (writer.joinable())
    {
        writer.join();
        write_telemetry(telemetry_path, telemetry, seconds);
    }

    // The lowest winner is the first in range order. The slices ascend from range_start, and each slice
    // reports its own lowest winner. Thus the first slice with a winner holds the lowest.
    const uint64_t evaluated = telemetry.evaluated.load();
    bool found = false;
    uint32_t lowest = 0u;
    for (const SliceResult &result : results)
    {
        if ((!found) && result.found)
        {
            found = true;
            lowest = result.winning_nonce;
        }
    }

    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  evaluated: ");
        mmgr_line_decimal(output_line, (unsigned long long)evaluated, 1u);
        mmgr_line_put(output_line, " nonces in ");
        mmgr_line_fixed(output_line, (double)(seconds), 3u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " s");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  rate: ");
        mmgr_line_fixed(output_line, (double)(((double)evaluated / seconds) / 1.0e6), 2u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " MH/s");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    if (!found)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  FAIL: no nonce in a range that holds 0x");
            mmgr_line_hex(output_line, published.recorded_nonce, 8u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  found 0x");
        mmgr_line_hex(output_line, lowest, 8u);
        mmgr_line_put(output_line, ", recorded 0x");
        mmgr_line_hex(output_line, published.recorded_nonce, 8u);
        mmgr_line_put(output_line, ": ");
        mmgr_line_put(output_line, (lowest == published.recorded_nonce) ? "PASS" : "FAIL");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return (lowest == published.recorded_nonce) ? 0 : 1;
}

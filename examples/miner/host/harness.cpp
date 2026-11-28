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

#include "embed_atomic.h"

#include <stdio.h>
#include <stdlib.h>

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

/** @brief The bytes of a header, and the cores the harness starts at max. */
#define HARNESS_HEADER_BYTES 80u
#define HARNESS_THREADS_MAX 256u

/** @brief The bytes of the path of the telemetry file and of its side file, each terminated. */
#define HARNESS_PATH_BYTES 1024u

/**
 * @brief Converts a hex string to bytes.
 *
 * @param[in]  hex      Characters to convert, an even count [BORROWS].
 * @param[out] bytes    Receives the bytes [BORROWS].
 * @param[in]  capacity Bytes at bytes.
 * @return              The bytes written.
 */
size_t bytes_from_hex(const char *hex, uint8_t *bytes, size_t capacity)
{
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = hex, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    size_t count = 0u;
    for (size_t at = 0u; ((at + 1u) < length) && (count < capacity); at += 2u)
    {
        const char pair[3] = {hex[at], hex[at + 1u], '\0'};
        // strtoul returns at most 0xFF for two hex digits. The cast narrows that to one byte and loses nothing.
        bytes[count] = (uint8_t)strtoul(pair, nullptr, 16);
        count += 1u;
    }
    return count;
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
    embed_atomic_u64 evaluated; /**< Nonces that the cores hashed. */
    embed_atomic_u64 survivors; /**< Nonces that passed the anchor and reached the exact compare. */
    embed_atomic_u64 winners;   /**< Slices that found a nonce at or below the block target. */
};

/** @brief A counter read, with acquire ordering. */
uint64_t counter_read(const embed_atomic_u64 *word)
{
    return EMBED_CALL(embed_atomic_u64_load, EmbedAtomicU64LoadCfg, .word = word);
}

/** @brief A counter added to. */
void counter_added(embed_atomic_u64 *word, uint64_t value)
{
    (void)EMBED_CALL(embed_atomic_u64_fetch_add, EmbedAtomicU64FetchAddCfg, .word = word, .value = value);
}

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
void write_telemetry(const char *path, const Telemetry &telemetry, double seconds)
{
    char partial[HARNESS_PATH_BYTES + 8u];
    VerbaLinea line = mmgr_line_over(partial, sizeof(partial));
    mmgr_line_put(&line, path);
    mmgr_line_put(&line, ".part");
    (void)mmgr_line_finish(&line);
    FILE *const file = fopen(partial, "w");
    if (file == nullptr)
    {
        return;
    }
    fprintf(file, "evaluated %llu\nsurvivors %llu\nwinners %llu\nseconds %.3f\n",
            (unsigned long long)counter_read(&telemetry.evaluated),
            (unsigned long long)counter_read(&telemetry.survivors),
            (unsigned long long)counter_read(&telemetry.winners), seconds);
    fclose(file);
    remove(path);
    rename(partial, path);
}

/** @brief What every core reads: the request, the range, the slice, the arm, and the counters. */
struct HarnessRun
{
    Sha256ScanRequest request;
    uint32_t range_start;
    uint32_t slice;
    void (*scan)(const Sha256ScanRequest *, Sha256ScanResult *);
    Telemetry telemetry;
    SliceResult results[HARNESS_THREADS_MAX];
    embed_atomic_u64 finished;
    const char *telemetry_path;
    unsigned long long started_ns;
};

HarnessRun g_run;

/** @brief Seconds since the scan started. */
double elapsed()
{
    return (double)(platform_clock_ns() - g_run.started_ns) / 1.0e9;
}

/** @brief One core: its slice of the range, in chunks, until the first chunk with a winner. */
void core_scan(void *context)
{
    const unsigned core = *(const unsigned *)context;
    Sha256ScanRequest own = g_run.request;
    const uint32_t slice_start = g_run.range_start + (core * g_run.slice);
    uint32_t done = 0u;
    // The chunks ascend, and each scan call reports its own lowest winner. Thus the first chunk
    // with a winner holds the lowest winner of the slice, and the slice stops there.
    while (done < g_run.slice)
    {
        own.nonce_start = slice_start + done;
        own.nonce_count = ((g_run.slice - done) < CHUNK_NONCES) ? (g_run.slice - done) : CHUNK_NONCES;
        Sha256ScanResult chunk;
        g_run.scan(&own, &chunk);
        counter_added(&g_run.telemetry.evaluated, chunk.nonces_evaluated);
        counter_added(&g_run.telemetry.survivors, chunk.anchors_survived);
        if (chunk.found != 0)
        {
            g_run.results[core].found = true;
            g_run.results[core].winning_nonce = chunk.winning_nonce;
            counter_added(&g_run.telemetry.winners, 1u);
            break;
        }
        done += own.nonce_count;
    }
}

/** @brief The writer of the telemetry file, twice a second until the cores end. */
void telemetry_writer(void *context)
{
    (void)context;
    while (counter_read(&g_run.finished) == 0u)
    {
        platform_paused(TELEMETRY_PERIOD_MS);
        write_telemetry(g_run.telemetry_path, g_run.telemetry, elapsed());
    }
}

/** @brief True where two terminated texts are equal. */
bool text_same(const char *one, const char *other)
{
    return EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = one, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = other,
                      .ci = EMBED_FALSE);
}

/** @brief The cores and the index each reads, and the writer of the telemetry file. */
PlatformWorker g_cores[HARNESS_THREADS_MAX];
unsigned g_core_index[HARNESS_THREADS_MAX];
PlatformWorker g_writer;

} // namespace

int program_main(int argument_count, char **arguments)
{
    unsigned threads = platform_processors();
    uint32_t span = 1u << 26u;
    unsigned which = 0u;
    const char *telemetry_path = "";
    for (int index = 1; index < argument_count; index += 1)
    {
        const char *const argument = arguments[index];
        const bool has_value = (index + 1) < argument_count;
        if (text_same(argument, "--threads") && has_value)
        {
            index += 1;
            threads = (unsigned)strtoul(arguments[index], nullptr, 10);
        }
        else if (text_same(argument, "--span") && has_value)
        {
            index += 1;
            span = (uint32_t)strtoul(arguments[index], nullptr, 0);
        }
        else if (text_same(argument, "--header") && has_value)
        {
            index += 1;
            which = (unsigned)strtoul(arguments[index], nullptr, 10);
        }
        else if (text_same(argument, "--telemetry") && has_value)
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
    if (threads > HARNESS_THREADS_MAX)
    {
        threads = HARNESS_THREADS_MAX;
    }
    if (EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = telemetry_path, .cap = PLATFORM_TEXT_TO_TERMINATOR) >=
        HARNESS_PATH_BYTES)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "--telemetry passes the bytes of its path");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 2;
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

    uint8_t header[HARNESS_HEADER_BYTES];
    (void)bytes_from_hex(published.header_hex, header, sizeof(header));

    Sha256ScanRequest &request = g_run.request;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &request, .bytes = sizeof(request), .val = 0);
    sha256_header_midstate(&request.midstate, header);
    request.merkle_root_tail = schedule_word(header + 64u);
    request.ntime = schedule_word(header + 68u);
    request.nbits = schedule_word(header + 72u);
    // The target comes from nbits as the header stores it, little-endian, and not from the schedule word.
    sha256_target_from_nbits(request.share_target, header_number(header + 72u));

    // A build without AVX2 compiles the eight-lane arm as a call to the reference arm. Thus the vector arm
    // needs AVX2 at compile time and on the processor. Build the core and the harness with the same flags.
#if defined(__AVX2__)
    const bool vector_arm = sha256_has_avx2() != 0;
#else
    const bool vector_arm = false;
#endif
    g_run.scan = vector_arm ? sha256_scan_avx2 : sha256_scan_scalar_early;

    // The range has the recorded nonce at its center. Unsigned arithmetic wraps mod 2^32, and the range wraps
    // with it.
    g_run.range_start = published.recorded_nonce - (span / 2u);
    g_run.slice = span / threads;
    g_run.telemetry_path = telemetry_path;

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
        mmgr_line_decimal(output_line, g_run.slice * threads, 1u);
        mmgr_line_put(output_line, " from 0x");
        mmgr_line_hex(output_line, g_run.range_start, 8u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    g_run.started_ns = platform_clock_ns();
    unsigned started_cores = 0u;
    for (unsigned core = 0u; core < threads; core += 1u)
    {
        g_core_index[core] = core;
        started_cores += (platform_worker_started(&g_cores[core], core_scan, &g_core_index[core]) != 0) ? 1u : 0u;
    }

    const bool writing = telemetry_path[0] != '\0';
    const bool writer_started = writing && (platform_worker_started(&g_writer, telemetry_writer, nullptr) != 0);
    for (unsigned core = 0u; core < started_cores; core += 1u)
    {
        platform_worker_joined(&g_cores[core]);
    }
    const double seconds = elapsed();
    EMBED_CALL(embed_atomic_u64_store, EmbedAtomicU64StoreCfg, .word = &g_run.finished, .value = 1u);
    if (writer_started)
    {
        platform_worker_joined(&g_writer);
        write_telemetry(telemetry_path, g_run.telemetry, seconds);
    }

    // The lowest winner is the first in range order. The slices ascend from range_start, and each slice
    // reports its own lowest winner. Thus the first slice with a winner holds the lowest.
    const uint64_t evaluated = counter_read(&g_run.telemetry.evaluated);
    bool found = false;
    uint32_t lowest = 0u;
    for (unsigned core = 0u; core < threads; core += 1u)
    {
        if ((!found) && g_run.results[core].found)
        {
            found = true;
            lowest = g_run.results[core].winning_nonce;
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

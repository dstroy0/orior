// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file btc_miner.cpp
 * @brief The stratum client: builds real headers, mines them, and submits what it actually found.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note The byte orders here come from a live job from solo.ckpool.org, and not from memory.
 *       A client must assemble a real header and hash it. It must compute the merkle root, keep the
 *       extranonce from the subscribe reply, and submit the nonce in its own parameter.
 * @note An independent double hash checks every share against the share target before the client
 *       sends it. The client never submits a share that it cannot verify. Thus a defect here costs
 *       shares, and the pool gets no garbage.
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "memoria_operor/memoria_operor.h"
#include <winsock2.h>
#include <ws2tcpip.h>

#include "cuda_miner.h"
#include "../../../src/platform/platform.h"
#include "json_value.h"
#include "sha256_core.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

namespace
{

/** @brief Where to mine. Overridable on the command line. */
const char *const DEFAULT_POOL_HOSTNAME = "solo.ckpool.org";
const char *const DEFAULT_POOL_PORT = "3333";

/**
 * @brief An example address that belongs to nobody here.
 *
 * @note This is the example address from the BIP-173 test vectors, reprinted in a great many
 *       tutorials. Anything it earns goes to whoever holds that key. The client keeps it only to
 *       refuse it by name as a payout address.
 */
const char *const KNOWN_EXAMPLE_ADDRESS = "bc1qxy2kgdygjrsqtzq2n0yrf2493p83kkfjhx0wlh";

/** @brief Who gets paid. Set from the command line or a config file; never defaulted. */
std::string g_payout_address;

/**
 * @brief The name stratum authorizes and submits under, where it is not the payout address.
 *
 * @note Two types of pool exist, and they key on different things. ckpool and Ocean take a
 *       Bitcoin address as the worker name. They pay to it directly. There, the address is the identity.
 *       Braiins and the other account pools take an account worker name. They keep the payout
 *       destination in an account setting. There, the address is not the identity, and the client
 *       must not send it.
 * @note The password field stays "x" either way. The stratum password of a pool is not an account
 *       password. Never put an account password on this wire.
 */
std::string g_stratum_user;

/** @brief What stratum should authorize and submit as. */
const std::string &stratum_identity()
{
    return g_stratum_user.empty() ? g_payout_address : g_stratum_user;
}

/**
 * @brief Connect and parse, but never authorize, mine or submit.
 *
 * @note This proves the job path before a payout address exists. It exercises everything upstream
 *       of the submit: the subscribe reply, the notify parse, the coinbase, the merkle fold and the
 *       header. It sends nothing that identifies anyone.
 */
bool g_dry_run = false;

/** @brief Nonces one worker claims per trip to the shared counter. */
const uint32_t NONCE_CHUNK = 1u << 21;

// ---------------------------------------------------------------------------------------------
// Byte and hex handling
// ---------------------------------------------------------------------------------------------

/**
 * @brief Converts a hex string to bytes.
 *
 * @param[in] hex Characters to convert, an even count.
 * @return        The bytes that they encode, or empty where the input has a bad form.
 */
std::vector<uint8_t> bytes_from_hex(const std::string &hex)
{
    std::vector<uint8_t> bytes;

    if ((hex.size() % 2u) != 0u)
    {
        return bytes;
    }
    bytes.reserve(hex.size() / 2u);
    for (size_t at = 0u; at < hex.size(); at += 2u)
    {
        unsigned value = 0u;
        for (size_t nibble = 0u; nibble < 2u; nibble += 1u)
        {
            const char character = hex[at + nibble];
            unsigned digit = 0u;

            if ((character >= '0') && (character <= '9'))
            {
                digit = (unsigned)(character - '0');
            }
            else if ((character >= 'a') && (character <= 'f'))
            {
                digit = (unsigned)(character - 'a') + 10u;
            }
            else if ((character >= 'A') && (character <= 'F'))
            {
                digit = (unsigned)(character - 'A') + 10u;
            }
            else
            {
                bytes.clear();
                return bytes;
            }
            value = (value << 4) | digit;
        }
        bytes.push_back((uint8_t)value);
    }
    return bytes;
}

/**
 * @brief Renders bytes as hex in the order they sit in memory.
 *
 * @param[in] bytes Bytes to render [BORROWS].
 * @param[in] count How many.
 * @return          The hex string.
 */
std::string hex_from_bytes(const uint8_t *bytes, size_t count)
{
    static const char digits[] = "0123456789abcdef";
    std::string rendered;

    rendered.reserve(count * 2u);
    for (size_t at = 0u; at < count; at += 1u)
    {
        rendered.push_back(digits[bytes[at] >> 4]);
        rendered.push_back(digits[bytes[at] & 0x0Fu]);
    }
    return rendered;
}

/**
 * @brief Renders a digest the way a block explorer prints it, most significant byte first.
 *
 * @param[in] digest Thirty-two bytes as SHA-256 emitted them [BORROWS].
 * @return           The reversed hex string.
 */
std::string block_hash_from_digest(const uint8_t *digest)
{
    uint8_t reversed[32];

    for (size_t at = 0u; at < 32u; at += 1u)
    {
        reversed[at] = digest[31u - at];
    }
    return hex_from_bytes(reversed, sizeof(reversed));
}

/**
 * @brief Renders a value as eight hex characters, most significant first.
 *
 * @param[in] value Number to render.
 * @return          The hex string.
 */
std::string hex_from_word(uint32_t value)
{
    char rendered[9];

    std::snprintf(rendered, sizeof(rendered), "%08x", value);
    return std::string(rendered);
}

/**
 * @brief Writes a word into a header field, little-endian, as the header stores its numbers.
 *
 * @param[out] field Four bytes to write [BORROWS].
 * @param[in]  value The number to store.
 */
void write_header_field(uint8_t *field, uint32_t value)
{
    field[0] = (uint8_t)value;
    field[1] = (uint8_t)(value >> 8);
    field[2] = (uint8_t)(value >> 16);
    field[3] = (uint8_t)(value >> 24);
}

/**
 * @brief Reads four header bytes as one big-endian word, the order SHA-256 reads its message in.
 *
 * @param[in] bytes Four bytes to read [BORROWS].
 * @return          The schedule word that they encode.
 */
uint32_t schedule_word_from_header(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) | ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

// ---------------------------------------------------------------------------------------------
// Job state
// ---------------------------------------------------------------------------------------------

/** @brief One mining.notify, kept as the pool sent it. */
struct MiningJob
{
    std::string job_id;
    std::string prevhash_hex;
    std::string coinbase_head_hex;
    std::string coinbase_tail_hex;
    std::vector<std::string> merkle_branch_hex;
    std::string version_hex;
    std::string nbits_hex;
    std::string ntime_hex;
    bool clean_jobs = false;
    bool valid = false;
};

/** @brief What the whole client shares, guarded by one lock and one generation counter. */
struct MinerState
{
    std::mutex lock;
    MiningJob job;
    std::string extranonce1_hex;
    size_t extranonce2_size = 4u;
    double difficulty = 1.0;

    // Rises whenever the work changes. Workers compare against it and drop their current work.
    std::atomic<uint64_t> generation{0};
    std::atomic<uint64_t> extranonce2_counter{0};
    std::atomic<uint64_t> nonce_cursor{0};
    std::atomic<uint64_t> hashes_done{0};
    std::atomic<uint64_t> anchor_survivors{0};
    std::atomic<bool> running{true};

    // Read through Sha256ScanRequest, which needs a volatile int instead of an atomic.
    volatile int abandon = 0;
};

MinerState g_state;

/** @brief A share that a worker found and verified. It waits for the network thread to send it. */
struct FoundShare
{
    std::string job_id;
    std::string extranonce2_hex;
    std::string ntime_hex;
    uint32_t nonce = 0u;
    std::string block_hash;
    bool meets_block_target = false;
};

std::mutex g_share_lock;
std::vector<FoundShare> g_pending_shares;

// ---------------------------------------------------------------------------------------------
// Header assembly
// ---------------------------------------------------------------------------------------------

/**
 * @brief Folds a coinbase hash through a merkle branch to the root the header carries.
 *
 * @param[in]  coinbase     Serialized coinbase transaction [BORROWS].
 * @param[in]  branch_hex   Branch entries as the pool sent them [BORROWS].
 * @param[out] merkle_root  Thirty-two bytes of root, in header order [BORROWS].
 * @note Every hash here stays in internal order. The branch entries concatenate as sent, and the
 *       root drops into the header as computed. Thus nothing in this function reverses anything.
 */
void merkle_root_from_coinbase(const std::vector<uint8_t> &coinbase, const std::vector<std::string> &branch_hex,
                               uint8_t *merkle_root)
{
    sha256_double_hash(coinbase.data(), coinbase.size(), merkle_root);

    for (const std::string &entry_hex : branch_hex)
    {
        const std::vector<uint8_t> entry = bytes_from_hex(entry_hex);
        uint8_t joined[64];

        if (entry.size() != 32u)
        {
            continue;
        }
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = joined, .src = merkle_root, .bytes = 32u);
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = joined + 32u, .src = entry.data(), .bytes = 32u);
        sha256_double_hash(joined, sizeof(joined), merkle_root);
    }
}

/**
 * @brief Builds the eighty header bytes for one job and one extranonce2.
 *
 * @param[in]  job             The job to build from [BORROWS].
 * @param[in]  extranonce1_hex Extranonce the pool assigned at subscribe [BORROWS].
 * @param[in]  extranonce2_hex Extranonce this client chose [BORROWS].
 * @param[out] header          Eighty bytes to fill [BORROWS].
 * @return                     True where every field decoded.
 * @note The prevhash arrives with each four byte word byte-reversed relative to the header. A
 *       reversal of each word recovers the header field. A reversal of the whole field yields a hash
 *       three zero characters short of the current target. A live job confirms this.
 */
bool build_header(const MiningJob &job, const std::string &extranonce1_hex, const std::string &extranonce2_hex,
                  uint8_t *header)
{
    const std::vector<uint8_t> prevhash = bytes_from_hex(job.prevhash_hex);
    const std::vector<uint8_t> coinbase_head = bytes_from_hex(job.coinbase_head_hex);
    const std::vector<uint8_t> coinbase_tail = bytes_from_hex(job.coinbase_tail_hex);
    const std::vector<uint8_t> extranonce1 = bytes_from_hex(extranonce1_hex);
    const std::vector<uint8_t> extranonce2 = bytes_from_hex(extranonce2_hex);

    if (prevhash.size() != 32u)
    {
        return false;
    }
    if (coinbase_head.empty() || coinbase_tail.empty())
    {
        return false;
    }

    std::vector<uint8_t> coinbase;
    coinbase.reserve(coinbase_head.size() + extranonce1.size() + extranonce2.size() + coinbase_tail.size());
    coinbase.insert(coinbase.end(), coinbase_head.begin(), coinbase_head.end());
    coinbase.insert(coinbase.end(), extranonce1.begin(), extranonce1.end());
    coinbase.insert(coinbase.end(), extranonce2.begin(), extranonce2.end());
    coinbase.insert(coinbase.end(), coinbase_tail.begin(), coinbase_tail.end());

    uint8_t merkle_root[32];
    merkle_root_from_coinbase(coinbase, job.merkle_branch_hex, merkle_root);

    const uint32_t version = (uint32_t)std::stoul(job.version_hex, nullptr, 16);
    const uint32_t nbits = (uint32_t)std::stoul(job.nbits_hex, nullptr, 16);
    const uint32_t ntime = (uint32_t)std::stoul(job.ntime_hex, nullptr, 16);

    write_header_field(header, version);
    for (size_t word = 0u; word < 8u; word += 1u)
    {
        for (size_t byte = 0u; byte < 4u; byte += 1u)
        {
            header[4u + (word * 4u) + byte] = prevhash[(word * 4u) + (3u - byte)];
        }
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = header + 36u, .src = merkle_root, .bytes = 32u);
    write_header_field(header + 68u, ntime);
    write_header_field(header + 72u, nbits);
    write_header_field(header + 76u, 0u);
    return true;
}

// ---------------------------------------------------------------------------------------------
// Mining workers
// ---------------------------------------------------------------------------------------------

/**
 * @brief Verifies a candidate independently of the scan that produced it.
 *
 * @param[in]  header        Eighty header bytes with the nonce already written [BORROWS].
 * @param[in]  share_target  Share threshold, most significant word first [BORROWS].
 * @param[out] digest        Thirty-two bytes of the hash that results [BORROWS].
 * @return                   True where the hash is at or below the share target.
 * @note This runs the plain double hash, and not the scan path. Thus a scan defect cannot smuggle a
 *       bad share past it. A pass of the anchor establishes nothing, and this check exists for that
 *       reason.
 */
bool verify_share(const uint8_t *header, const uint32_t *share_target, uint8_t *digest)
{
    sha256_double_hash(header, BITCOIN_HEADER_BYTES, digest);
    return sha256_digest_within_target(digest, share_target) != 0;
}

/**
 * @brief One mining worker: claims nonce ranges for the current job until the work changes.
 *
 * @param[in] worker_index Which worker this is. Only the report uses it.
 */
/**
 * @brief Counters that outlive one run of the process.
 *
 * @note A figure per run resets on restart. An anchor count that drops from 1020 to 85 then looks
 *       like a fault, but it is only a fresh process. A rate that is a lifetime average reads low
 *       for a long time after another program shares the device. A figure needs its "since when".
 * @note Three views, because they answer different questions. The window rate says whether the
 *       device is healthy now. The run totals say what this run did. The lifetime totals say
 *       whether the work accumulates across restarts.
 */
struct LifetimeTotals
{
    uint64_t hashes = 0u;
    uint64_t anchors = 0u;
    uint64_t submitted = 0u;
    uint64_t accepted = 0u;
    uint64_t rejected = 0u;
    double seconds = 0.0;
    uint64_t runs = 0u;
};

/** @brief Where the lifetime counters live between runs. */
const char *const TOTALS_PATH = "miner_totals.txt";

/**
 * @brief Reads the lifetime counters. An absent file is a first run, and not an error.
 *
 * @return What previous runs accumulated, or zeros.
 * @note One value per line, and no other content. Thus a person can read and check the file by
 *       eye. A miner that reports totals that nobody can verify reports nothing.
 */
LifetimeTotals load_totals()
{
    LifetimeTotals held;
    std::FILE *const handle = std::fopen(TOTALS_PATH, "r");
    if (handle == nullptr)
    {
        return held;
    }
    if (std::fscanf(handle, "%llu %llu %llu %llu %llu %lf %llu", (unsigned long long *)&held.hashes,
                    (unsigned long long *)&held.anchors, (unsigned long long *)&held.submitted,
                    (unsigned long long *)&held.accepted, (unsigned long long *)&held.rejected, &held.seconds,
                    (unsigned long long *)&held.runs) != 7)
    {
        // A truncated file, or a file with a bad form, starts over. It does not carry half a total forward.
        held = LifetimeTotals();
    }
    std::fclose(handle);
    return held;
}

/** @brief Writes the lifetime counters. */
void save_totals(const LifetimeTotals &held)
{
    std::FILE *const handle = std::fopen(TOTALS_PATH, "w");
    if (handle == nullptr)
    {
        return;
    }
    std::fprintf(handle, "%llu %llu %llu %llu %llu %.3f %llu\n", (unsigned long long)held.hashes,
                 (unsigned long long)held.anchors, (unsigned long long)held.submitted,
                 (unsigned long long)held.accepted, (unsigned long long)held.rejected, held.seconds,
                 (unsigned long long)held.runs);
    std::fclose(handle);
}

/** @brief Winners that one device launch may report. The launch drops the rest. */
const uint32_t DEVICE_FOUND_LIMIT = 64u;

/** @brief Nonces that one device launch claims. Sixty-four million is about thirty milliseconds
 *         at 2 GH/s. That keeps the launch overhead small. The abandon flag still gets a check
 *         often enough for a prompt start on a new job. */
const uint64_t DEVICE_NONCE_CHUNK = 1ull << 26;

/** @brief Nonzero where the operator asked for the device instead of the eight-lane arm. */
bool g_use_device = true;

void mining_worker(int worker_index)
{
    (void)worker_index;

    // One device context per worker. Worker zero takes it, and the rest fall back to the eight-lane
    // arm. One feeder at 2 GH/s already saturates the device. A second context would contend for
    // the same SMs, and cost another set of buffers.
    CudaMinerContext *device = nullptr;
    if (g_use_device && (worker_index == 0))
    {
        device = cuda_miner_create(DEVICE_FOUND_LIMIT);
        if (device == nullptr)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "device refused a context; worker 0 falls back to the eight-lane arm");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }

    uint64_t known_generation = UINT64_MAX;
    MiningJob job;
    std::string extranonce1_hex;
    std::string extranonce2_hex;
    uint8_t header[BITCOIN_HEADER_BYTES];
    Sha256ScanRequest request;
    uint32_t share_target[SHA256_STATE_WORDS];
    uint32_t block_target[SHA256_STATE_WORDS];
    bool header_ready = false;

    while (g_state.running.load(std::memory_order_relaxed))
    {
        const uint64_t current_generation = g_state.generation.load(std::memory_order_acquire);

        if (current_generation != known_generation)
        {
            known_generation = current_generation;
            header_ready = false;

            {
                std::lock_guard<std::mutex> held(g_state.lock);
                if (!g_state.job.valid)
                {
                    continue;
                }
                job = g_state.job;
                extranonce1_hex = g_state.extranonce1_hex;
                sha256_share_target_from_difficulty(share_target, g_state.difficulty);
            }

            const uint64_t counter = g_state.extranonce2_counter.load(std::memory_order_relaxed);
            std::string counter_hex = hex_from_word((uint32_t)counter);
            size_t needed = 0u;
            {
                std::lock_guard<std::mutex> held(g_state.lock);
                needed = g_state.extranonce2_size * 2u;
            }
            // Left-pad with zeros to whatever width the pool asked for at subscribe time.
            extranonce2_hex = (counter_hex.size() >= needed)
                                  ? counter_hex.substr(counter_hex.size() - needed)
                                  : std::string(needed - counter_hex.size(), '0') + counter_hex;

            if (!build_header(job, extranonce1_hex, extranonce2_hex, header))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            EMBED_CALL(memor.set, MemoriaCfg, .dst = &request, .bytes = sizeof(request), .val = 0);
            sha256_header_midstate(&request.midstate, header);
            request.merkle_root_tail = schedule_word_from_header(header + 64u);
            request.ntime = schedule_word_from_header(header + 68u);
            request.nbits = schedule_word_from_header(header + 72u);
            request.abandon = &g_state.abandon;
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = request.share_target, .src = share_target,
                       .bytes = sizeof(share_target));
            sha256_target_from_nbits(block_target, (uint32_t)std::stoul(job.nbits_hex, nullptr, 16));
            header_ready = true;
        }

        if (!header_ready)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        // A device chunk must be much larger than a CPU chunk, or the launch overhead dominates.
        // Two million nonces is a millisecond of work at 2 GH/s, and the round trip costs more than
        // that. All workers share the cursor. A device worker simply claims more of it per turn.
        const uint64_t chunk = (device != nullptr) ? DEVICE_NONCE_CHUNK : NONCE_CHUNK;
        const uint64_t claimed = g_state.nonce_cursor.fetch_add(chunk, std::memory_order_relaxed);
        if (claimed >= 0x100000000ULL)
        {
            // This extranonce2 has no nonce space left. Roll it and let every worker rebuild.
            std::lock_guard<std::mutex> held(g_state.lock);
            if (g_state.nonce_cursor.load(std::memory_order_relaxed) >= 0x100000000ULL)
            {
                g_state.extranonce2_counter.fetch_add(1u, std::memory_order_relaxed);
                g_state.nonce_cursor.store(0u, std::memory_order_relaxed);
                g_state.generation.fetch_add(1u, std::memory_order_release);
            }
            continue;
        }

        request.nonce_start = (uint32_t)claimed;
        request.nonce_count = (uint32_t)((claimed + chunk > 0x100000000ULL) ? (0x100000000ULL - claimed) : chunk);

        Sha256ScanResult result;

        // The device where this worker has one, and the eight-lane arm otherwise. bench_cuda
        // measures 2059.79 MH/s against 70.81 on the CPU: 29.1 times. It recovers the nonces of
        // genesis and block 125552. It agrees with the CPU arm on every winner over a million nonces.
        // Because of that agreement, this dispatch changes the engine, and not the answer.
        if (device != nullptr)
        {
            CudaScanParameters parameters;
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = parameters.midstate, .src = request.midstate.word,
                       .bytes = sizeof(parameters.midstate));
            parameters.merkle_root_tail = request.merkle_root_tail;
            parameters.ntime = request.ntime;
            parameters.nbits = request.nbits;
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = parameters.share_target, .src = request.share_target,
                       .bytes = sizeof(parameters.share_target));
            parameters.anchor_is_sound = (request.share_target[0] == 0u) ? 1u : 0u;

            uint32_t found_nonces[DEVICE_FOUND_LIMIT];
            uint32_t found_count = 0u;
            const int launched = cuda_miner_scan(device, &parameters, request.nonce_start, request.nonce_count,
                                                 found_nonces, &found_count);
            if (launched == 0)
            {
                // A failed launch is not a scanned range. A count of it would inflate the rate and
                // silently skip nonces. Thus the loop drops the chunk and tries the next one.
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            result.nonces_evaluated = request.nonce_count;

            // The anchor count of the device, and not its winner count. A winner clears the anchor and
            // then also comes in under the threshold. A winner count here would make the device seem
            // to find no survivors. The figure on screen would then show only the CPU workers: about
            // a thirtieth of the work.
            result.anchors_survived = (uint32_t)cuda_miner_anchor_survivors(device);
            result.found = (found_count > 0u) ? 1 : 0;
            result.winning_nonce = 0u;

            // Winners come back in no order. Submit the lowest.
            for (uint32_t at = 0u; at < found_count; at += 1u)
            {
                if ((at == 0u) || (found_nonces[at] < result.winning_nonce))
                {
                    result.winning_nonce = found_nonces[at];
                }
            }
        }
        else
        {
            sha256_scan_avx2(&request, &result);
        }

        g_state.hashes_done.fetch_add(result.nonces_evaluated, std::memory_order_relaxed);
        g_state.anchor_survivors.fetch_add(result.anchors_survived, std::memory_order_relaxed);

        if (result.found == 0)
        {
            continue;
        }

        write_header_field(header + 76u, result.winning_nonce);
        uint8_t digest[32];
        if (!verify_share(header, share_target, digest))
        {
            // The scan claimed a share, and the plain double hash does not agree. Report it and
            // send nothing. A disagreement between the two is a defect, and not a share.
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "\n[DEFECT] scan reported nonce ");
                mmgr_line_decimal(output_line, result.winning_nonce, 1u);
                mmgr_line_put(output_line, " which independent verification rejects. Not submitting.");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            continue;
        }

        FoundShare share;
        share.job_id = job.job_id;
        share.extranonce2_hex = extranonce2_hex;
        share.ntime_hex = job.ntime_hex;
        share.nonce = result.winning_nonce;
        share.block_hash = block_hash_from_digest(digest);
        share.meets_block_target = sha256_digest_within_target(digest, block_target) != 0;

        {
            std::lock_guard<std::mutex> held(g_share_lock);
            g_pending_shares.push_back(share);
        }
    }

    if (device != nullptr)
    {
        cuda_miner_destroy(device);
    }
}

// ---------------------------------------------------------------------------------------------
// Stratum connection
// ---------------------------------------------------------------------------------------------

/**
 * @brief Sends a whole payload. It loops over partial writes.
 *
 * @param[in] connection Socket to write to.
 * @param[in] payload    Bytes to send.
 * @return               True where all of it went out.
 */
bool send_payload(SOCKET connection, const std::string &payload)
{
    size_t sent_total = 0u;

    while (sent_total < payload.size())
    {
        const int sent = send(connection, payload.data() + sent_total, (int)(payload.size() - sent_total), 0);
        if (sent == SOCKET_ERROR)
        {
            return false;
        }
        sent_total += (size_t)sent;
    }
    return true;
}

/**
 * @brief Reads a mining.notify into a job.
 *
 * @param[in]  params Parameter array as sent [BORROWS].
 * @param[out] job    Where the job lands [BORROWS].
 * @return            True where every field was present and the right type.
 * @note The order is job_id, prevhash, coinb1, coinb2, merkle_branch, version, nbits, ntime,
 *       clean_jobs. A live job confirms this order, and it is not an assumption.
 */
bool job_from_notify(const JsonValue &params, MiningJob &job)
{
    if ((params.type != JsonType::Array) || (params.elements.size() < 9u))
    {
        return false;
    }
    if (!params.at(0).is_string() || !params.at(1).is_string() || !params.at(2).is_string() ||
        !params.at(3).is_string() || !params.at(5).is_string() || !params.at(6).is_string() ||
        !params.at(7).is_string())
    {
        return false;
    }

    job.job_id = params.at(0).text;
    job.prevhash_hex = params.at(1).text;
    job.coinbase_head_hex = params.at(2).text;
    job.coinbase_tail_hex = params.at(3).text;
    job.merkle_branch_hex.clear();

    const JsonValue &branch = params.at(4);
    for (size_t at = 0u; at < branch.elements.size(); at += 1u)
    {
        if (branch.at(at).is_string())
        {
            job.merkle_branch_hex.push_back(branch.at(at).text);
        }
    }

    job.version_hex = params.at(5).text;
    job.nbits_hex = params.at(6).text;
    job.ntime_hex = params.at(7).text;
    job.clean_jobs = params.at(8).boolean;
    job.valid = true;
    return true;
}

/**
 * @brief Prints what a new job actually resolved to, a byte order defect is visible immediately.
 *
 * @param[in] job The job just installed [BORROWS].
 */
void report_job(const MiningJob &job)
{
    const std::vector<uint8_t> prevhash = bytes_from_hex(job.prevhash_hex);
    std::string previous_block;

    if (prevhash.size() == 32u)
    {
        uint8_t header_order[32];
        for (size_t word = 0u; word < 8u; word += 1u)
        {
            for (size_t byte = 0u; byte < 4u; byte += 1u)
            {
                header_order[(word * 4u) + byte] = prevhash[(word * 4u) + (3u - byte)];
            }
        }
        previous_block = block_hash_from_digest(header_order);
    }

    size_t leading_zeros = 0u;
    while ((leading_zeros < previous_block.size()) && (previous_block[leading_zeros] == '0'))
    {
        leading_zeros += 1u;
    }

    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "\n[JOB] ");
        mmgr_line_put(output_line, job.job_id.c_str());
        mmgr_line_put(output_line, "  branch ");
        mmgr_line_decimal(output_line, job.merkle_branch_hex.size(), 1u);
        mmgr_line_put(output_line, "  ntime ");
        mmgr_line_put(output_line, job.ntime_hex.c_str());
        mmgr_line_put(output_line, "  nbits ");
        mmgr_line_put(output_line, job.nbits_hex.c_str());
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "      previous block ");
        mmgr_line_put(output_line, previous_block.c_str());
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "      leading zeros ");
        mmgr_line_decimal(output_line, leading_zeros, 1u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put(output_line, (leading_zeros >= 15u) ? "(consistent with a real chain tip)"
                                                          : "(TOO FEW: prevhash byte order is wrong)");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
}

/**
 * @brief Builds a header from a live job and reports whether every field landed where it should.
 *
 * @param[in] job             The job just received [BORROWS].
 * @param[in] extranonce1_hex Extranonce the pool assigned [BORROWS].
 * @param[in] extranonce2_size How wide the pool wants extranonce2.
 * @return                    True where every check passed.
 * @note These checks catch a byte order defect without a message from a pool. A
 *       correctly assembled header hashes to something, and that hash has to be consistent with the
 *       difficulty the same job declares. A header assembled wrongly still hashes; it just hashes
 *       to noise, and noise does not sit near a target.
 */
bool dry_run_check_job(const MiningJob &job, const std::string &extranonce1_hex, size_t extranonce2_size)
{
    uint8_t header[BITCOIN_HEADER_BYTES];
    const std::string extranonce2_hex(extranonce2_size * 2u, '0');
    bool all_held = true;

    if (!build_header(job, extranonce1_hex, extranonce2_hex, header))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "      [FAIL] header could not be assembled");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return false;
    }

    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "      header      ");
        mmgr_line_put(output_line, hex_from_bytes(header, BITCOIN_HEADER_BYTES).c_str());
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    // The merkle root must not be all zeros or the literal the old client used.
    bool merkle_is_zero = true;
    for (size_t at = 36u; at < 68u; at += 1u)
    {
        if (header[at] != 0u)
        {
            merkle_is_zero = false;
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "      merkle root ");
        mmgr_line_put(output_line, hex_from_bytes(header + 36u, 32u).c_str());
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, merkle_is_zero ? "[FAIL] all zero" : "[ok]");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    all_held = all_held && !merkle_is_zero;

    // The version, ntime and nbits must round-trip back to what the pool sent.
    const uint32_t version_back =
        (uint32_t)header[0] | ((uint32_t)header[1] << 8) | ((uint32_t)header[2] << 16) | ((uint32_t)header[3] << 24);
    const uint32_t nbits_back = (uint32_t)header[72] | ((uint32_t)header[73] << 8) | ((uint32_t)header[74] << 16) |
                                ((uint32_t)header[75] << 24);
    const bool version_ok = (version_back == (uint32_t)std::stoul(job.version_hex, nullptr, 16));
    const bool nbits_ok = (nbits_back == (uint32_t)std::stoul(job.nbits_hex, nullptr, 16));
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "      version     ");
        mmgr_line_hex(output_line, version_back, 8u);
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, version_ok ? "[ok]" : "[FAIL]");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "      nbits       ");
        mmgr_line_hex(output_line, nbits_back, 8u);
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, nbits_ok ? "[ok]" : "[FAIL]");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    all_held = all_held && version_ok && nbits_ok;

    // The declared difficulty has to be a sane network number. A misread nbits gives an absurd one.
    uint32_t block_target[SHA256_STATE_WORDS];
    sha256_target_from_nbits(block_target, nbits_back);
    const bool target_ok = (block_target[0] == 0u) && (block_target[1] != 0u || block_target[2] != 0u);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "      net target  ");
        mmgr_line_hex(output_line, block_target[0], 8u);
        mmgr_line_hex(output_line, block_target[1], 8u);
        mmgr_line_hex(output_line, block_target[2], 8u);
        mmgr_line_put(output_line, "...  ");
        mmgr_line_put(output_line, target_ok ? "[ok]" : "[FAIL] nbits did not expand sanely");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    all_held = all_held && target_ok;

    // Hash the header once. A single nonce should not meet the target. But the digest must have a
    // good form, and the scan path must agree with the plain double hash on the same input.
    uint8_t digest[32];
    sha256_double_hash(header, BITCOIN_HEADER_BYTES, digest);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "      hash@nonce0 ");
        mmgr_line_put(output_line, block_hash_from_digest(digest).c_str());
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    Sha256ScanRequest request;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &request, .bytes = sizeof(request), .val = 0);
    sha256_header_midstate(&request.midstate, header);
    request.merkle_root_tail = schedule_word_from_header(header + 64u);
    request.ntime = schedule_word_from_header(header + 68u);
    request.nbits = schedule_word_from_header(header + 72u);
    request.nonce_start = 0u;
    request.nonce_count = 1u;
    // A threshold of all ones accepts anything. Thus the scan must run its compare path, and report
    // the nonce, and not reject it. That makes the two paths comparable on one input.
    EMBED_CALL(memor.set, MemoriaCfg, .dst = request.share_target, .bytes = sizeof(request.share_target), .val = 0xFF);

    Sha256ScanResult result;
    sha256_scan_scalar(&request, &result);
    const bool scan_agrees = (result.found != 0) && (result.winning_nonce == 0u);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "      scan path agrees with plain double hash: ");
        mmgr_line_put(output_line, scan_agrees ? "[ok]" : "[FAIL]");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    all_held = all_held && scan_agrees;

    return all_held;
}

} // namespace

int program_main(int argument_count, char **arguments)
{
    std::string pool_hostname = DEFAULT_POOL_HOSTNAME;
    std::string pool_port = DEFAULT_POOL_PORT;

    // Vardiff can climb to 10000 after a flood of false submissions. At this hash rate, that is one
    // share every six hours. Ask for a difficulty that produces shares while a person watches the
    // client.
    double suggested_difficulty = 16.0;

    // Zero means the default, which leaves one core for the person at the keyboard.
    unsigned requested_threads = 0u;

    // A miner is background work. At normal priority on a desktop, it starves the window manager,
    // even with a free core. When every core is busy, the priority decides what runs, and the core
    // count does not.
    bool background_priority = true;

    for (int index = 1; index < argument_count; index += 1)
    {
        const std::string argument = arguments[index];
        const bool has_value = (index + 1) < argument_count;

        if ((argument == "--address") && has_value)
        {
            g_payout_address = arguments[index + 1];
            index += 1;
        }
        else if ((argument == "--pool") && has_value)
        {
            pool_hostname = arguments[index + 1];
            index += 1;
        }
        else if ((argument == "--port") && has_value)
        {
            pool_port = arguments[index + 1];
            index += 1;
        }
        else if ((argument == "--difficulty") && has_value)
        {
            suggested_difficulty = std::stod(arguments[index + 1]);
            index += 1;
        }
        else if ((argument == "--threads") && has_value)
        {
            requested_threads = (unsigned)std::stoul(arguments[index + 1]);
            index += 1;
        }
        else if (argument == "--normal-priority")
        {
            background_priority = false;
        }
        else if ((argument == "--user") && has_value)
        {
            g_stratum_user = arguments[index + 1];
            index += 1u;
        }
        else if (argument == "--no-gpu")
        {
            g_use_device = false;
        }
        else if (argument == "--dry-run")
        {
            g_dry_run = true;
        }
    }

    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "BTC");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "AVX2: ");
        mmgr_line_put(output_line, (sha256_has_avx2() != 0) ? "yes" : "no");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    if (sha256_has_avx2() == 0)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "This build needs AVX2. Refusing to run a scalar miner.");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }

    // Fail closed. A miner with no address, or with the example address, mines for someone else.
    // A refusal costs a restart. A guess costs the earnings.
    //
    // An account pool identifies by worker name, and keeps the payout destination in an account
    // setting. Thus --user alone is a complete identity there. A demand for an address too would
    // refuse a correct configuration. An address pool still needs the address.
    if (g_payout_address.empty() && g_stratum_user.empty() && !g_dry_run)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "\nNo identity given. Pass --address <your bitcoin address> for a pool that");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "pays an address, such as ckpool or Ocean, or --user <account.worker> for one");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "that pays an account, such as Braiins.");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "Refusing to mine without one. Pass --dry-run to exercise the job path");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "without authorizing, mining or submitting.");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    if (g_payout_address == KNOWN_EXAMPLE_ADDRESS)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "\nThat is the BIP-173 example address, not a wallet you control.");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "It was hardcoded in the previous version. Refusing to mine to it.");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }

    if (!g_stratum_user.empty())
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "worker: ");
            mmgr_line_put(output_line, g_stratum_user.c_str());
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "payout: set in the pool account, not by this client");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        if (!g_payout_address.empty())
        {
            // Say so, and do not silently prefer one. An operator who passes both probably believes
            // that the address has an effect here. On an account pool, it has none.
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "note:   --address is ignored on an account pool; the account decides");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
        }
    }
    else
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "paying: ");
        mmgr_line_put(output_line, g_payout_address.c_str());
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    if (background_priority)
    {
        // BELOW_NORMAL keeps the miner off the critical path for input and drawing.
        // PROCESS_MODE_BACKGROUND_BEGIN would also throttle its I/O and page priority. That is too
        // much for a process with an open socket.
        if (SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS) != 0)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "priority: below normal, so the desktop stays responsive");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }

    WSADATA winsock;
    if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "WSAStartup failed");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }

    addrinfo hints;
    addrinfo *resolved = nullptr;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &hints, .bytes = sizeof(hints), .val = 0);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(pool_hostname.c_str(), pool_port.c_str(), &hints, &resolved) != 0)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "cannot resolve ");
            mmgr_line_put(output_line, pool_hostname.c_str());
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        WSACleanup();
        return 1;
    }

    const SOCKET connection = socket(resolved->ai_family, resolved->ai_socktype, resolved->ai_protocol);
    if (connection == INVALID_SOCKET)
    {
        freeaddrinfo(resolved);
        WSACleanup();
        return 1;
    }

    BOOL nodelay = TRUE;
    setsockopt(connection, IPPROTO_TCP, TCP_NODELAY, (const char *)&nodelay, sizeof(nodelay));
    DWORD receive_timeout = 200;
    setsockopt(connection, SOL_SOCKET, SO_RCVTIMEO, (const char *)&receive_timeout, sizeof(receive_timeout));

    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "connecting to ");
        mmgr_line_put(output_line, pool_hostname.c_str());
        mmgr_line_put(output_line, ":");
        mmgr_line_put(output_line, pool_port.c_str());
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    if (connect(connection, resolved->ai_addr, (int)resolved->ai_addrlen) == SOCKET_ERROR)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "connect failed");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        closesocket(connection);
        freeaddrinfo(resolved);
        WSACleanup();
        return 1;
    }
    freeaddrinfo(resolved);

    send_payload(connection, "{\"id\":1,\"method\":\"mining.subscribe\",\"params\":[\"btc/0.1\"]}\r\n");
    if (!g_dry_run)
    {
        send_payload(connection, std::string("{\"id\":2,\"method\":\"mining.authorize\",\"params\":[\"") +
                                     stratum_identity() + "\",\"x\"]}\r\n");

        // Ask for a difficulty this hash rate actually produces shares at. Vardiff aims at roughly
        // one share every ten to thirty seconds; the pool is free to ignore this and its own
        // set_difficulty always wins.
        if (suggested_difficulty > 0.0)
        {
            char request[192];
            std::snprintf(request, sizeof(request),
                          "{\"id\":3,\"method\":\"mining.suggest_difficulty\",\"params\":[%.4f]}"
                          "\r\n",
                          suggested_difficulty);
            send_payload(connection, request);
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "suggested difficulty: ");
                mmgr_line_fixed(output_line, (double)(suggested_difficulty), 4u, 0u, EMBED_FALSE);
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
        }
    }

    // Leave one thread for the person at the machine. A miner that takes every core makes the
    // desktop unusable. The last core is worth less than the ability to type. Override with
    // --threads on a dedicated box.
    const unsigned available = (std::thread::hardware_concurrency() > 0u) ? std::thread::hardware_concurrency() : 8u;
    const unsigned worker_count =
        (requested_threads > 0u) ? requested_threads : ((available > 1u) ? (available - 1u) : 1u);
    std::vector<std::thread> workers;
    if (g_dry_run)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "DRY RUN: subscribing and parsing only. Not authorizing, mining or submitting.");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    else
    {
        workers.reserve(worker_count);
        for (unsigned index = 0u; index < worker_count; index += 1u)
        {
            workers.emplace_back(mining_worker, (int)index);
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "workers: ");
            mmgr_line_decimal(output_line, worker_count, 1u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }

    std::string inbound;
    char buffer[16384];
    uint64_t submitted = 0u;
    uint64_t accepted = 0u;
    uint64_t rejected = 0u;
    uint64_t next_submit_id = 100u;
    auto last_report = std::chrono::steady_clock::now();
    const auto started = std::chrono::steady_clock::now();

    // What previous runs accumulated. Thus the report can show both this run and the lifetime total.
    LifetimeTotals lifetime = load_totals();
    uint64_t last_hashes = 0u;
    if (lifetime.runs > 0u)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "lifetime: ");
        mmgr_line_fixed(output_line, (double)(lifetime.seconds / 3600.0), 1u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " hours, ");
        mmgr_line_fixed(output_line, (double)((double)lifetime.hashes / 1.0e12), 2u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " TH, ");
        mmgr_line_decimal(output_line, (unsigned long long)lifetime.anchors, 1u);
        mmgr_line_put(output_line, " anchors, ");
        mmgr_line_decimal(output_line, (unsigned long long)lifetime.accepted, 1u);
        mmgr_line_put(output_line, "/");
        mmgr_line_decimal(output_line, (unsigned long long)lifetime.submitted, 1u);
        mmgr_line_put(output_line, " shares over ");
        mmgr_line_decimal(output_line, (unsigned long long)lifetime.runs, 1u);
        mmgr_line_put(output_line, " runs");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    // The count goes here, and every periodic save carries it forward. An increment of a copy
    // would let the next report write back the old value, and the run count would never rise.
    lifetime.runs += 1u;
    save_totals(lifetime);

    while (true)
    {
        const int received = recv(connection, buffer, (int)sizeof(buffer) - 1, 0);
        if (received > 0)
        {
            buffer[received] = '\0';
            inbound.append(buffer, (size_t)received);

            size_t line_end = inbound.find('\n');
            while (line_end != std::string::npos)
            {
                std::string line = inbound.substr(0u, line_end);
                inbound.erase(0u, line_end + 1u);
                while (!line.empty() && ((line.back() == '\r') || (line.back() == ' ')))
                {
                    line.pop_back();
                }
                line_end = inbound.find('\n');

                if (line.empty())
                {
                    continue;
                }

                JsonValue message;
                if (!json_parse(line, message))
                {
                    continue;
                }

                const JsonValue &method = message.member("method");
                const JsonValue &params = message.member("params");

                if (method.is_string() && (method.text == "mining.notify"))
                {
                    MiningJob job;
                    if (job_from_notify(params, job))
                    {
                        {
                            std::lock_guard<std::mutex> held(g_state.lock);
                            g_state.job = job;
                        }
                        g_state.abandon = 1;
                        g_state.nonce_cursor.store(0u, std::memory_order_relaxed);
                        g_state.generation.fetch_add(1u, std::memory_order_release);
                        g_state.abandon = 0;
                        report_job(job);

                        if (g_dry_run)
                        {
                            std::string extranonce1_hex;
                            size_t extranonce2_size = 4u;
                            {
                                std::lock_guard<std::mutex> held(g_state.lock);
                                extranonce1_hex = g_state.extranonce1_hex;
                                extranonce2_size = g_state.extranonce2_size;
                            }
                            const bool held_up = dry_run_check_job(job, extranonce1_hex, extranonce2_size);
                            {
                                VerbaLinea *const output_line = platform_line_out();
                                mmgr_line_put(output_line, "\n  DRY RUN RESULT: ");
                                mmgr_line_put(output_line, held_up ? "every check held" : "CHECKS FAILED");
                                mmgr_line_ch(output_line, '\n');
                                platform_line_out_ended();
                            }
                            g_state.running.store(false, std::memory_order_relaxed);
                            closesocket(connection);
                            WSACleanup();
                            return held_up ? 0 : 1;
                        }
                    }
                    continue;
                }

                if (method.is_string() && (method.text == "mining.set_difficulty"))
                {
                    if (params.at(0).type == JsonType::Number)
                    {
                        {
                            std::lock_guard<std::mutex> held(g_state.lock);
                            g_state.difficulty = params.at(0).number;
                        }
                        g_state.generation.fetch_add(1u, std::memory_order_release);
                        {
                            VerbaLinea *const output_line = platform_line_out();
                            mmgr_line_put(output_line, "\n[DIFF] share difficulty ");
                            mmgr_line_fixed(output_line, (double)(params.at(0).number), 4u, 0u, EMBED_FALSE);
                            mmgr_line_ch(output_line, '\n');
                            platform_line_out_ended();
                        }
                    }
                    continue;
                }

                const JsonValue &identifier = message.member("id");
                const JsonValue &outcome = message.member("result");

                if ((identifier.type == JsonType::Number) && (identifier.number == 1.0))
                {
                    // Subscribe reply: the extranonce is its purpose, and the client must keep it.
                    // Element one is extranonce1, element two is the extranonce2 width.
                    if (outcome.at(1).is_string())
                    {
                        std::lock_guard<std::mutex> held(g_state.lock);
                        g_state.extranonce1_hex = outcome.at(1).text;
                        if (outcome.at(2).type == JsonType::Number)
                        {
                            g_state.extranonce2_size = (size_t)outcome.at(2).number;
                        }
                        {
                            VerbaLinea *const output_line = platform_line_out();
                            mmgr_line_put(output_line, "[SUBSCRIBE] extranonce1 ");
                            mmgr_line_put(output_line, g_state.extranonce1_hex.c_str());
                            mmgr_line_put(output_line, ", extranonce2 ");
                            mmgr_line_decimal(output_line, g_state.extranonce2_size, 1u);
                            mmgr_line_put(output_line, " bytes");
                            mmgr_line_ch(output_line, '\n');
                            platform_line_out_ended();
                        }
                    }
                    continue;
                }

                if ((identifier.type == JsonType::Number) && (identifier.number >= 100.0))
                {
                    if ((outcome.type == JsonType::Boolean) && outcome.boolean)
                    {
                        accepted += 1u;
                        {
                            VerbaLinea *const output_line = platform_line_out();
                            mmgr_line_put(output_line, "\n[SHARE] accepted (");
                            mmgr_line_decimal(output_line, (unsigned long long)accepted, 1u);
                            mmgr_line_put(output_line, " of ");
                            mmgr_line_decimal(output_line, (unsigned long long)submitted, 1u);
                            mmgr_line_put(output_line, ")");
                            mmgr_line_ch(output_line, '\n');
                            platform_line_out_ended();
                        }
                    }
                    else
                    {
                        rejected += 1u;
                        const JsonValue &failure = message.member("error");
                        std::string reason = "unspecified";
                        if (failure.type == JsonType::Array)
                        {
                            for (const JsonValue &part : failure.elements)
                            {
                                if (part.is_string())
                                {
                                    reason = part.text;
                                }
                            }
                        }
                        {
                            VerbaLinea *const output_line = platform_line_out();
                            mmgr_line_put(output_line, "\n[SHARE] REJECTED: ");
                            mmgr_line_put(output_line, reason.c_str());
                            mmgr_line_ch(output_line, '\n');
                            platform_line_out_ended();
                        }
                    }
                    continue;
                }
            }
        }
        else if (received == 0)
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "\npool closed the connection");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            break;
        }
        else
        {
            const int failure = WSAGetLastError();
            if ((failure != WSAETIMEDOUT) && (failure != WSAEWOULDBLOCK))
            {
                {
                    VerbaLinea *const output_line = platform_line_out();
                    mmgr_line_put(output_line, "\nreceive failed (");
                    mmgr_line_decimal_signed(output_line, failure);
                    mmgr_line_put(output_line, ")");
                    mmgr_line_ch(output_line, '\n');
                    platform_line_out_ended();
                }
                break;
            }
        }

        std::vector<FoundShare> outgoing;
        {
            std::lock_guard<std::mutex> held(g_share_lock);
            outgoing.swap(g_pending_shares);
        }
        for (const FoundShare &share : outgoing)
        {
            const std::string payload = "{\"id\":" + std::to_string(next_submit_id) +
                                        ",\"method\":\"mining.submit\",\"params\":[\"" + stratum_identity() + "\",\"" +
                                        share.job_id + "\",\"" + share.extranonce2_hex + "\",\"" + share.ntime_hex +
                                        "\",\"" + hex_from_word(share.nonce) + "\"]}\r\n";
            next_submit_id += 1u;
            submitted += 1u;

            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "\n[SUBMIT] nonce ");
                mmgr_line_put(output_line, hex_from_word(share.nonce).c_str());
                mmgr_line_put(output_line, "  hash ");
                mmgr_line_put(output_line, share.block_hash.c_str());
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            if (share.meets_block_target)
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "[BLOCK] this hash meets the network target.");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            send_payload(connection, payload);
        }

        const auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration<double>(now - last_report).count() >= 2.0)
        {
            const double elapsed = std::chrono::duration<double>(now - started).count();
            const uint64_t hashes = g_state.hashes_done.load(std::memory_order_relaxed);
            const uint64_t anchors = g_state.anchor_survivors.load(std::memory_order_relaxed);
            const double rate = (elapsed > 0.0) ? ((double)hashes / elapsed) : 0.0;

            // The window rate. It says whether the device is healthy *now*. A lifetime average takes
            // many minutes to recover after another program shares the device for a few minutes.
            // Thus it reports a fault long after the fault ends, and hides a fault that just began.
            const double window_seconds = std::chrono::duration<double>(now - last_report).count();
            const double window_rate = (window_seconds > 0.0) ? ((double)(hashes - last_hashes) / window_seconds) : 0.0;

            // The anchor count is a live correctness check, worth a display against its expected
            // value. A survivor is a one in 2^32 event. Thus the ratio of seen to expected stays
            // near one while the digests are right. It walks off immediately if a kernel change
            // breaks one.
            const double anchors_due = (double)(lifetime.hashes + hashes) / 4294967296.0;
            const double anchor_ratio =
                (anchors_due > 0.0) ? ((double)(lifetime.anchors + anchors) / anchors_due) : 0.0;

            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "\r[");
                mmgr_line_fixed(output_line, (double)(elapsed), 0u, 0u, EMBED_FALSE);
                mmgr_line_put(output_line, "s] now ");
                mmgr_line_fixed(output_line, (double)(window_rate / 1.0e6), 0u, 0u, EMBED_FALSE);
                mmgr_line_put(output_line, "  run ");
                mmgr_line_fixed(output_line, (double)(rate / 1.0e6), 0u, 0u, EMBED_FALSE);
                mmgr_line_put(output_line, " MH/s  h ");
                mmgr_line_decimal(output_line, (unsigned long long)hashes, 1u);
                mmgr_line_put(output_line, "  anc ");
                mmgr_line_decimal(output_line, (unsigned long long)anchors, 1u);
                mmgr_line_put(output_line, " (");
                mmgr_line_fixed(output_line, (double)(anchor_ratio), 2u, 0u, EMBED_FALSE);
                mmgr_line_put(output_line, "x due)  sh ");
                mmgr_line_decimal(output_line, (unsigned long long)accepted, 1u);
                mmgr_line_put(output_line, "/");
                mmgr_line_decimal(output_line, (unsigned long long)submitted, 1u);
                mmgr_line_put(output_line, "  | life ");
                mmgr_line_fixed(output_line, (double)((lifetime.seconds + elapsed) / 3600.0), 1u, 0u, EMBED_FALSE);
                mmgr_line_put(output_line, "h  ");
                mmgr_line_fixed(output_line, (double)((double)(lifetime.hashes + hashes) / 1.0e12), 2u, 0u,
                                EMBED_FALSE);
                mmgr_line_put(output_line, " TH  anc ");
                mmgr_line_decimal(output_line, (unsigned long long)(lifetime.anchors + anchors), 1u);
                mmgr_line_put(output_line, "  sh ");
                mmgr_line_decimal(output_line, (unsigned long long)(lifetime.accepted + accepted), 1u);
                mmgr_line_put(output_line, "/");
                mmgr_line_decimal(output_line, (unsigned long long)(lifetime.submitted + submitted), 1u);
                mmgr_line_put(output_line, " -");
                mmgr_line_decimal(output_line, (unsigned long long)(lifetime.rejected + rejected), 1u);
                mmgr_line_put(output_line, "   ");
                platform_line_out_ended();
            }
            platform_line_flushed();

            // Saved at every report, and not at exit. A kill ends a miner far more often than a
            // request to stop. Totals that survive only a clean shutdown do not survive.
            LifetimeTotals writing = lifetime;
            writing.hashes += hashes;
            writing.anchors += anchors;
            writing.submitted += submitted;
            writing.accepted += accepted;
            writing.rejected += rejected;
            writing.seconds += elapsed;
            save_totals(writing);

            last_hashes = hashes;
            last_report = now;
        }
    }

    g_state.running.store(false, std::memory_order_relaxed);
    g_state.abandon = 1;
    for (std::thread &worker : workers)
    {
        worker.join();
    }

    closesocket(connection);
    WSACleanup();
    return 0;
}

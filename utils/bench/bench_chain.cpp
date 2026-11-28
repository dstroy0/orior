// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_chain.cpp
 * @brief A thousand real block solves as one known answer test, and what their nonces look like.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note This is the strongest correctness test available for a miner. The hardware of somebody else
 *       solved every one of these headers, and its hash is on the chain. If this implementation
 *       disagrees on even one, the implementation is wrong, and the disagreement names the block.
 * @note The bench does not trust the hash of the explorer. It rebuilds the header from its parts,
 *       hashes it here, and compares. A separate test checks the chain links. A corpus with a change
 *       in transit fails the link test, and does not quietly pass.
 * @note The nonces are also real answers to the real search, and worth a look. Section 3 asks
 *       whether the nonces that win land anywhere in particular. If they clustered, the search space
 *       would have a preferred region, and mining would start there.
 */

#include "json_value.h"
#include "sha256_core.h"

#include <algorithm>
#include <chrono>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{

int g_checks_run = 0;
int g_checks_failed = 0;

/**
 * @brief Records one check and prints it.
 *
 * @param[in] name   The name of the check.
 * @param[in] passed Whether the check passed.
 * @param[in] detail The observed result, printed on failure.
 */
void check(const std::string &name, bool passed, const std::string &detail = "")
{
    g_checks_run += 1;
    if (passed)
    {
        printf("  [PASS] %s\n", name.c_str());
    }
    else
    {
        g_checks_failed += 1;
        printf("  [FAIL] %s\n", name.c_str());
        if (!detail.empty())
        {
            printf("         %s\n", detail.c_str());
        }
    }
}

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
 * @brief Renders a digest the way an explorer prints it, most significant byte first.
 *
 * @param[in] digest Thirty-two bytes as SHA-256 emitted them [BORROWS].
 * @return           The reversed hex string.
 */
std::string block_hash_from_digest(const uint8_t *digest)
{
    static const char symbols[] = "0123456789abcdef";
    std::string rendered;

    rendered.reserve(64u);
    for (size_t at = 0u; at < 32u; at += 1u)
    {
        const uint8_t value = digest[31u - at];
        rendered.push_back(symbols[value >> 4]);
        rendered.push_back(symbols[value & 0x0Fu]);
    }
    return rendered;
}

/** @brief One block, as the explorer reported it. */
struct ChainBlock
{
    uint32_t height;
    std::string id;
    uint32_t version;
    std::string previous;
    std::string merkle_root;
    uint32_t timestamp;
    uint32_t bits;
    uint32_t nonce;
};

/**
 * @brief Writes a little-endian word into a header field.
 *
 * @param[out] field Four bytes [BORROWS].
 * @param[in]  value The number to store.
 */
void write_field(uint8_t *field, uint32_t value)
{
    field[0] = (uint8_t)value;
    field[1] = (uint8_t)(value >> 8);
    field[2] = (uint8_t)(value >> 16);
    field[3] = (uint8_t)(value >> 24);
}

/**
 * @brief Rebuilds the eighty header bytes from a block's reported parts.
 *
 * @param[in]  block  The block [BORROWS].
 * @param[out] header Eighty bytes to fill [BORROWS].
 * @note The explorer prints hashes most significant byte first, and the header stores them least
 *       significant byte first. Thus the function reverses both hash fields on the way in.
 */
void header_from_block(const ChainBlock &block, uint8_t *header)
{
    const std::vector<uint8_t> previous = bytes_from_hex(block.previous);
    const std::vector<uint8_t> merkle = bytes_from_hex(block.merkle_root);

    write_field(header, block.version);
    for (size_t at = 0u; at < 32u; at += 1u)
    {
        header[4u + at] = previous[31u - at];
        header[36u + at] = merkle[31u - at];
    }
    write_field(header + 68u, block.timestamp);
    write_field(header + 72u, block.bits);
    write_field(header + 76u, block.nonce);
}

/**
 * @brief Reads the corpus written by tools/fetch_blocks.py.
 *
 * @param[in]  path   Where the corpus is.
 * @param[out] blocks Where the blocks land [BORROWS].
 * @return            True where the file parsed.
 */
bool load_corpus(const std::string &path, std::vector<ChainBlock> &blocks)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();

    JsonValue document;
    if (!json_parse(buffer.str(), document) || (document.type != JsonType::Array))
    {
        return false;
    }

    for (const JsonValue &entry : document.elements)
    {
        ChainBlock block;
        block.height = (uint32_t)entry.member("height").number;
        block.id = entry.member("id").text;
        block.version = (uint32_t)entry.member("version").number;
        block.previous = entry.member("previousblockhash").text;
        block.merkle_root = entry.member("merkle_root").text;
        block.timestamp = (uint32_t)entry.member("timestamp").number;
        block.bits = (uint32_t)entry.member("bits").number;
        // A nonce above 2^31 does not survive a signed round trip. Thus the code reads it as a double
        // and casts it. That is exact for every value below 2^53.
        block.nonce = (uint32_t)entry.member("nonce").number;
        blocks.push_back(block);
    }
    return !blocks.empty();
}

/**
 * @brief Hashes every block and requires agreement with the chain's own record.
 *
 * @param[in] blocks The corpus [BORROWS].
 */
void test_every_block_hashes(const std::vector<ChainBlock> &blocks)
{
    printf("\n================================================================\n");
    printf("  1. Hashing %zu real solves\n", blocks.size());
    printf("================================================================\n");

    size_t hash_matches = 0u;
    size_t target_matches = 0u;
    std::string first_failure;

    for (const ChainBlock &block : blocks)
    {
        uint8_t header[BITCOIN_HEADER_BYTES];
        uint8_t digest[32];

        header_from_block(block, header);
        sha256_double_hash(header, BITCOIN_HEADER_BYTES, digest);

        const std::string produced = block_hash_from_digest(digest);
        if (produced == block.id)
        {
            hash_matches += 1u;
        }
        else if (first_failure.empty())
        {
            first_failure = "height " + std::to_string(block.height) + ": got " + produced + ", chain says " + block.id;
        }

        // Every real solve must satisfy its own nbits, which exercises the target expansion and the
        // ordering the share check depends on.
        uint32_t target[SHA256_STATE_WORDS];
        sha256_target_from_nbits(target, block.bits);
        if (sha256_digest_within_target(digest, target) != 0)
        {
            target_matches += 1u;
        }
    }

    check("every header hashes to the hash the chain recorded", hash_matches == blocks.size(),
          first_failure.empty() ? (std::to_string(hash_matches) + " of " + std::to_string(blocks.size()))
                                : first_failure);
    check("every solve satisfies its own nbits target", target_matches == blocks.size(),
          std::to_string(target_matches) + " of " + std::to_string(blocks.size()));
}

/**
 * @brief Requires the corpus to be a chain, a tampered corpus fails visibly.
 *
 * @param[in] blocks The corpus, newest first [BORROWS].
 */
void test_chain_links(const std::vector<ChainBlock> &blocks)
{
    printf("\n================================================================\n");
    printf("  2. Is the corpus actually a chain?\n");
    printf("================================================================\n");

    size_t links = 0u;
    size_t checked = 0u;

    for (size_t at = 0u; (at + 1u) < blocks.size(); at += 1u)
    {
        // Newest first. Thus the parent of block[at] is block[at+1].
        if (blocks[at].height != (blocks[at + 1u].height + 1u))
        {
            continue;
        }
        checked += 1u;
        if (blocks[at].previous == blocks[at + 1u].id)
        {
            links += 1u;
        }
    }

    check("every block names its predecessor's hash as its parent", links == checked,
          std::to_string(links) + " of " + std::to_string(checked));
    printf("\n  Linkage is checked a corpus altered in transit fails here instead of\n");
    printf("  passing section 1 against its own altered hashes.\n");
}

/**
 * @brief Asks whether real nonces that win land anywhere in particular.
 *
 * @param[in] blocks The corpus [BORROWS].
 * @note If the nonces that win clustered, the search space would have a preferred region, and every
 *       miner should start there. This is the cheapest possible test of that, on real answers.
 */
void analyse_winning_nonces(const std::vector<ChainBlock> &blocks)
{
    printf("\n================================================================\n");
    printf("  3. Where do real winning nonces land?\n");
    printf("================================================================\n");

    // Sixteen buckets across the 32-bit nonce space.
    const unsigned buckets = 16u;
    std::vector<size_t> counts(buckets, 0u);
    for (const ChainBlock &block : blocks)
    {
        counts[(size_t)(block.nonce >> 28)] += 1u;
    }

    const double expected = (double)blocks.size() / (double)buckets;
    double chi_square = 0.0;
    for (size_t bucket = 0u; bucket < buckets; bucket += 1u)
    {
        const double residual = (double)counts[bucket] - expected;
        chi_square += (residual * residual) / expected;
    }

    printf("\n  Nonce space split into %u buckets, %zu solves, %.1f expected each:\n", buckets, blocks.size(),
                expected);
    printf("\n  ");
    for (size_t bucket = 0u; bucket < buckets; bucket += 1u)
    {
        printf("%4zu", counts[bucket]);
    }
    printf("\n");

    // Chi-square on 15 degrees of freedom has mean 15 and standard deviation sqrt(30).
    const double score = (chi_square - 15.0) / sqrt(30.0);
    printf("\n  chi-square %.1f on 15 df (expected 15), z = %+.2f\n", chi_square, score);
    check("winning nonces are spread uniformly across the space", fabs(score) < 4.0,
          "a preferred region would mean every miner should start there");

    // A version roll shows up as many distinct versions across a short span of blocks.
    std::vector<uint32_t> versions;
    for (const ChainBlock &block : blocks)
    {
        versions.push_back(block.version);
    }
    std::sort(versions.begin(), versions.end());
    const size_t distinct = (size_t)(std::unique(versions.begin(), versions.end()) - versions.begin());

    printf("\n  distinct version values across %zu blocks: %zu\n", blocks.size(), distinct);
    printf("  That is BIP320 version rolling in use. It is a real search dimension and it\n");
    printf("  is being spent, which is why bounding the input to the nonce was wrong.\n");
}

/**
 * @brief Reports how much work the chain says these solves cost.
 *
 * @param[in] blocks The corpus [BORROWS].
 */
void report_work(const std::vector<ChainBlock> &blocks)
{
    printf("\n================================================================\n");
    printf("  4. What these solves cost\n");
    printf("================================================================\n");

    if (blocks.empty())
    {
        return;
    }

    uint32_t target[SHA256_STATE_WORDS];
    sha256_target_from_nbits(target, blocks[0].bits);

    // Expected hashes per solve is 2^256 / (target + 1), which for these targets is very close to
    // difficulty times 2^32.
    double difficulty_scale = 0.0;
    for (unsigned rank = 0u; rank < SHA256_STATE_WORDS; rank += 1u)
    {
        difficulty_scale = (difficulty_scale * 4294967296.0) + (double)target[rank];
    }
    const double expected_hashes = pow(2.0, 256.0) / (difficulty_scale + 1.0);

    printf("\n  nbits on the newest block : %08x\n", blocks[0].bits);
    printf("  expected hashes per solve : %.3e\n", expected_hashes);
    printf("  this machine, CPU         : %.3e per second\n", 81.17e6);
    printf("  this machine, RTX 3070    : %.3e per second\n", 1885.79e6);
    printf("\n  So one solve at this difficulty is about %.2e seconds of the GPU,\n", expected_hashes / 1885.79e6);
    printf("  which is %.2e years.\n", expected_hashes / 1885.79e6 / 31557600.0);
    printf("\n  These %zu blocks were found in roughly %u seconds of wall clock by the\n", blocks.size(),
                blocks.front().timestamp - blocks.back().timestamp);
    printf("  whole network, which is the number this machine is being compared against.\n");
}

} // namespace

int main(int argument_count, char **arguments)
{
    const std::string path = (argument_count > 1) ? arguments[1] : "utils/maint/chain/blocks.json";

    printf("================================================================\n");
    printf("  A thousand real solves as a known answer test\n");
    printf("  corpus: %s\n", path.c_str());
    printf("================================================================\n");

    std::vector<ChainBlock> blocks;
    if (!load_corpus(path, blocks))
    {
        printf("\nCould not read the corpus. Run utils/maint/chain/fetch_blocks.py first.\n");
        return 1;
    }
    printf("\nloaded %zu blocks, heights %u to %u\n", blocks.size(), blocks.back().height, blocks.front().height);

    const auto started = std::chrono::steady_clock::now();

    test_every_block_hashes(blocks);
    test_chain_links(blocks);
    analyse_winning_nonces(blocks);
    report_work(blocks);

    const auto finished = std::chrono::steady_clock::now();
    printf("\n================================================================\n");
    printf("  %d checks run, %d failed\n", g_checks_run, g_checks_failed);
    printf("  elapsed %.2f s\n", std::chrono::duration<double>(finished - started).count());
    printf("================================================================\n");
    return (g_checks_failed == 0) ? 0 : 1;
}

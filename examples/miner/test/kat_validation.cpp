// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file kat_validation.cpp
 * @brief Known answer tests whose answers come from outside this tree.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note Every expected value here comes from a source outside this tree: the NIST digests, the
 *       genesis block, and block 125552. A change to the definition of success in the engine
 *       cannot make a test in this file pass.
 * @note The scan tests search a real nonce range for a real header. Each test requires the engine to
 *       find the nonce that the chain recorded. A miss is a false negative, and the anchor rule
 *       forbids it. Thus the test reports a miss as a failure, and not as a slow result.
 */

#include "../../../src/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "sha256_core.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace
{

int g_tests_run = 0;
int g_tests_failed = 0;

/**
 * @brief Converts a hex string to bytes.
 *
 * @param[in] hex Characters to convert, an even count.
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
 * @brief Records one test outcome and prints it.
 *
 * @param[in] name     The name of the test.
 * @param[in] passed   Nonzero where the test passed.
 * @param[in] detail   The observed result, printed on failure.
 */
void report(const std::string &name, bool passed, const std::string &detail = "")
{
    g_tests_run += 1;
    if (passed)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  [PASS] ");
        mmgr_line_put(output_line, name.c_str());
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    else
    {
        g_tests_failed += 1;
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  [FAIL] ");
            mmgr_line_put(output_line, name.c_str());
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        if (!detail.empty())
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "         ");
            mmgr_line_put(output_line, detail.c_str());
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
}

/** @brief A published SHA-256 digest and the message that produces it. */
struct DigestVector
{
    const char *message;
    const char *expected_digest;
    const char *source;
};

/** @brief NIST FIPS 180-4 examples, plus the digest of the empty string. Every implementation agrees on it. */
const DigestVector DIGEST_VECTORS[] = {
    {"", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "empty string"},
    {"abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "FIPS 180-4 B.1"},
    {"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
     "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1", "FIPS 180-4 B.2"},
};

/**
 * @brief Runs the published digest vectors through the reference arm.
 */
void test_published_digests()
{
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "\n[1] SHA-256 against published digests");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    for (const DigestVector &vector : DIGEST_VECTORS)
    {
        uint8_t digest[32];
        sha256_hash((const uint8_t *)vector.message,
                    EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = vector.message, .cap = PLATFORM_TEXT_TO_TERMINATOR),
                    digest);

        const std::string produced = hex_from_bytes(digest, sizeof(digest));
        report(std::string("sha256 ") + vector.source, produced == vector.expected_digest,
               "got " + produced + ", expected " + vector.expected_digest);
    }

    // A message of exactly fifty-six bytes forces the length field into a second padding block. That
    // is the boundary a padding bug hides behind.
    const std::string boundary(56u, 'a');
    uint8_t digest[32];
    sha256_hash((const uint8_t *)boundary.data(), boundary.size(), digest);
    const std::string produced = hex_from_bytes(digest, sizeof(digest));
    report("sha256 fifty-six byte padding boundary",
           produced == "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a", "got " + produced);
}

/** @brief A block whose header, hash and nonce the chain already recorded. */
struct HeaderVector
{
    const char *header_hex;
    const char *expected_hash;
    uint32_t expected_nonce;
    const char *source;
};

/** @brief Two headers anyone can check against a block explorer. */
const HeaderVector HEADER_VECTORS[] = {
    {"0100000000000000000000000000000000000000000000000000000000000000000000003ba3edfd7a7b12b27ac7"
     "2c3e67768f617fc81bc3888a51323a9fb8aa4b1e5e4a29ab5f49ffff001d1dac2b7c",
     "000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f", 0x7c2bac1du, "genesis block, height 0"},
    {"0100000081cd02ab7e569e8bcd9317e2fe99f2de44d49ab2b8851ba4a308000000000000e320b6c2fffc8d750423"
     "db8b1eb942ae710e951ed797f7affc8892b0f1fc122bc7f5d74df2b9441a42a14695",
     "00000000000000001e8d6829a8a21adc5d38d0a473b144b6765798e61f98bd1d", 0x9546a142u,
     "block 125552, the classic test vector"},
};

/**
 * @brief Hashes published headers and checks both the hash and the recovered nonce.
 */
void test_published_headers()
{
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "\n[2] Bitcoin headers against the chain's own record");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    for (const HeaderVector &vector : HEADER_VECTORS)
    {
        const std::vector<uint8_t> header = bytes_from_hex(vector.header_hex);
        if (header.size() != BITCOIN_HEADER_BYTES)
        {
            report(std::string("header length for ") + vector.source, false,
                   "got " + std::to_string(header.size()) + " bytes, expected 80");
            continue;
        }

        uint8_t digest[32];
        sha256_double_hash(header.data(), header.size(), digest);

        const std::string produced = block_hash_from_digest(digest);
        report(std::string("hash of ") + vector.source, produced == vector.expected_hash,
               "got " + produced + ", expected " + vector.expected_hash);

        // The nonce sits at header bytes seventy-six through seventy-nine, little-endian.
        const uint32_t header_nonce = (uint32_t)header[76] | ((uint32_t)header[77] << 8) |
                                      ((uint32_t)header[78] << 16) | ((uint32_t)header[79] << 24);
        report(std::string("nonce field of ") + vector.source, header_nonce == vector.expected_nonce,
               "got " + std::to_string(header_nonce));

        // The block's own hash must satisfy the block's own nbits. If it does not, either the target
        // expansion or the ordering is wrong, and both feed the share check.
        const uint32_t nbits = (uint32_t)header[72] | ((uint32_t)header[73] << 8) | ((uint32_t)header[74] << 16) |
                               ((uint32_t)header[75] << 24);
        uint32_t block_target[SHA256_STATE_WORDS];
        sha256_target_from_nbits(block_target, nbits);
        report(std::string("nbits ordering for ") + vector.source,
               sha256_digest_within_target(digest, block_target) != 0, "block hash did not satisfy its own nbits");
    }
}

/**
 * @brief Reads a header field as the number it encodes, which is little-endian on the wire.
 *
 * @param[in] header Eighty header bytes [BORROWS].
 * @param[in] offset Byte offset of the field.
 * @return           The number the four bytes encode.
 * @note This is not the same word that the SHA-256 schedule sees. The schedule reads those bytes
 *       big-endian. Thus nbits enters the hash as 0xf2b9441a and enters a target as 0x1a44b9f2.
 *       A reader that gets the other form yields an exponent of 242 and a target of zero.
 *       That target refuses every nonce and never reports an error.
 */
uint32_t header_field_value(const uint8_t *header, size_t offset)
{
    return (uint32_t)header[offset] | ((uint32_t)header[offset + 1u] << 8) | ((uint32_t)header[offset + 2u] << 16) |
           ((uint32_t)header[offset + 3u] << 24);
}

/**
 * @brief Builds a scan request from a published header, minus its nonce.
 *
 * @param[out] request Request to fill [BORROWS].
 * @param[in]  header  Eighty header bytes [BORROWS].
 */
void scan_request_from_header(Sha256ScanRequest *request, const uint8_t *header)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = request, .bytes = sizeof(*request), .val = 0);
    sha256_header_midstate(&request->midstate, header);

    request->merkle_root_tail = ((uint32_t)header[64] << 24) | ((uint32_t)header[65] << 16) |
                                ((uint32_t)header[66] << 8) | (uint32_t)header[67];
    request->ntime = ((uint32_t)header[68] << 24) | ((uint32_t)header[69] << 16) | ((uint32_t)header[70] << 8) |
                     (uint32_t)header[71];
    request->nbits = ((uint32_t)header[72] << 24) | ((uint32_t)header[73] << 16) | ((uint32_t)header[74] << 8) |
                     (uint32_t)header[75];
}

/**
 * @brief Requires both scan arms to recover a real block's nonce from a range that contains it.
 */
void test_scan_recovers_real_nonce()
{
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "\n[3] Scan arms recovering a nonce the chain recorded");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    for (const HeaderVector &vector : HEADER_VECTORS)
    {
        const std::vector<uint8_t> header = bytes_from_hex(vector.header_hex);
        Sha256ScanRequest request;
        scan_request_from_header(&request, header.data());

        // Start the window below the real nonce so the arm has to search instead of land on it. The
        // window is the smallest that still proves the arm walks forward into the answer.
        const uint32_t window = 4096u;
        request.nonce_start = vector.expected_nonce - (window / 2u);
        request.nonce_count = window;
        sha256_target_from_nbits(request.share_target, header_field_value(header.data(), 72u));

        Sha256ScanResult scalar_result;
        sha256_scan_scalar(&request, &scalar_result);
        report(std::string("scalar arm finds nonce of ") + vector.source,
               (scalar_result.found != 0) && (scalar_result.winning_nonce == vector.expected_nonce),
               scalar_result.found ? ("got " + std::to_string(scalar_result.winning_nonce))
                                   : "FALSE NEGATIVE: no nonce found in a window containing one");

        Sha256ScanResult vector_result;
        sha256_scan_avx2(&request, &vector_result);
        report(std::string("eight-lane arm finds nonce of ") + vector.source,
               (vector_result.found != 0) && (vector_result.winning_nonce == vector.expected_nonce),
               vector_result.found ? ("got " + std::to_string(vector_result.winning_nonce))
                                   : "FALSE NEGATIVE: no nonce found in a window containing one");
    }
}

/**
 * @brief Requires the two arms to agree on a range with no winner in it.
 *
 * @note This is the disagreement test the header calls a defect. A vectorized arm that quietly drops
 *       lanes passes every test above and fails this one.
 */
void test_arms_agree()
{
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "\n[4] Arms agreeing where no answer exists");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    const std::vector<uint8_t> header = bytes_from_hex(HEADER_VECTORS[1].header_hex);
    Sha256ScanRequest request;
    scan_request_from_header(&request, header.data());

    // A window well away from the real nonce, at the block's own difficulty. Nothing here wins.
    request.nonce_start = 1000000u;
    request.nonce_count = 65536u;
    sha256_target_from_nbits(request.share_target, request.nbits);

    Sha256ScanResult scalar_result;
    Sha256ScanResult vector_result;
    sha256_scan_scalar(&request, &scalar_result);
    sha256_scan_avx2(&request, &vector_result);

    report("neither arm invents a winner", (scalar_result.found == 0) && (vector_result.found == 0),
           "scalar found=" + std::to_string(scalar_result.found) +
               " vector found=" + std::to_string(vector_result.found));
    report("both arms evaluated every nonce",
           (scalar_result.nonces_evaluated == request.nonce_count) &&
               (vector_result.nonces_evaluated == request.nonce_count),
           "scalar " + std::to_string(scalar_result.nonces_evaluated) + " vector " +
               std::to_string(vector_result.nonces_evaluated));

    // The anchor passes at a rate of two to the minus thirty-two. A nonce window of sixty-five
    // thousand should almost never produce a survivor. The test checks that the arms agree, and
    // only reports the count.
    report("arms agree on anchor survivors", scalar_result.anchors_survived == vector_result.anchors_survived,
           "scalar " + std::to_string(scalar_result.anchors_survived) + " vector " +
               std::to_string(vector_result.anchors_survived));
}

/**
 * @brief Requires an easy target to produce the same winner from both arms.
 *
 * @note At the block difficulty, a winner is astronomically rare. Thus only an easy target
 *       actually exercises the compare path on both arms.
 */
void test_arms_agree_on_easy_target()
{
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "\n[5] Arms agreeing on a target loose enough to hit");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    const std::vector<uint8_t> header = bytes_from_hex(HEADER_VECTORS[1].header_hex);
    Sha256ScanRequest request;
    scan_request_from_header(&request, header.data());

    // Sixteen leading zero bits: the top word at or below 0x0000FFFF and every lower word
    // unconstrained. Roughly one nonce in sixty-five thousand clears it. A window of a million nonces
    // holds many, and the arms must agree on the lowest.
    //
    // A nonzero top word also makes the anchor unsound. Thus this test exercises the fallback
    // path, where every nonce pays the full compare. Test three covers the anchor path, where
    // the real block hashes carry sixty-four leading zero bits.
    EMBED_CALL(memor.set, MemoriaCfg, .dst = request.share_target, .bytes = sizeof(request.share_target), .val = 0xFF);
    request.share_target[0] = 0x0000FFFFu;
    request.nonce_start = 0u;
    request.nonce_count = 1000000u;

    Sha256ScanResult scalar_result;
    Sha256ScanResult vector_result;
    sha256_scan_scalar(&request, &scalar_result);
    sha256_scan_avx2(&request, &vector_result);

    report("both arms find a winner at an easy target", (scalar_result.found != 0) && (vector_result.found != 0),
           "scalar found=" + std::to_string(scalar_result.found) +
               " vector found=" + std::to_string(vector_result.found));
    report("both arms report the same lowest nonce", scalar_result.winning_nonce == vector_result.winning_nonce,
           "scalar " + std::to_string(scalar_result.winning_nonce) + " vector " +
               std::to_string(vector_result.winning_nonce));

    // The winner has to survive an independent check that never touches the scan path.
    if (scalar_result.found != 0)
    {
        std::vector<uint8_t> candidate = bytes_from_hex(HEADER_VECTORS[1].header_hex);
        const uint32_t nonce = scalar_result.winning_nonce;
        candidate[76] = (uint8_t)nonce;
        candidate[77] = (uint8_t)(nonce >> 8);
        candidate[78] = (uint8_t)(nonce >> 16);
        candidate[79] = (uint8_t)(nonce >> 24);

        uint8_t digest[32];
        sha256_double_hash(candidate.data(), candidate.size(), digest);
        report("winner verified by an independent double hash",
               sha256_digest_within_target(digest, request.share_target) != 0,
               "hash " + block_hash_from_digest(digest) + " is not within the target");
    }
}

/**
 * @brief Checks the difficulty to target conversion against the values the protocol fixes.
 */
void test_difficulty_conversion()
{
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "\n[6] Difficulty to target conversion");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    uint32_t target[SHA256_STATE_WORDS];

    // Difficulty one is 0xFFFF times two to the two hundred eighth. Those bits land at positions 208
    // through 223: the upper half of word one. Thus word one reads 0xFFFF0000, and not 0x0000FFFF.
    sha256_share_target_from_difficulty(target, 1.0);
    report("difficulty one is 0x00000000FFFF0000...",
           (target[0] == 0x00000000u) && (target[1] == 0xFFFF0000u) && (target[2] == 0u) && (target[7] == 0u),
           "word zero " + std::to_string(target[0]) + " word one " + std::to_string(target[1]));

    sha256_share_target_from_difficulty(target, 2.0);
    report("difficulty two halves the threshold", (target[0] == 0x00000000u) && (target[1] == 0x7FFF8000u),
           "word one is " + std::to_string(target[1]));

    // nbits 0x1d00ffff is difficulty one in the form that a header uses. Thus the two paths must
    // give the same threshold.
    uint32_t from_nbits[SHA256_STATE_WORDS];
    sha256_target_from_nbits(from_nbits, 0x1d00ffffu);
    sha256_share_target_from_difficulty(target, 1.0);
    report("nbits 0x1d00ffff agrees with difficulty one",
           EMBED_CALL(memor.cmp, MemoriaCfg, .src = from_nbits, .other = target, .bytes = sizeof(target)) == 0,
           "nbits path word one is " + std::to_string(from_nbits[1]));

    // Every target the pool can set must leave the anchor sound. Section 2.2 is only satisfied where
    // the most significant word is zero, and the engine falls back where it is not.
    sha256_share_target_from_difficulty(target, 1.0);
    report("difficulty one leaves the anchor sound", target[0] == 0u, "top word is " + std::to_string(target[0]));
}

/**
 * @brief Times the eight-lane arm. The reported rate is then a measurement, and not an assertion.
 */
void measure_scan_rate()
{
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "\n[7] Measured rate of the eight-lane arm, single thread");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    const std::vector<uint8_t> header = bytes_from_hex(HEADER_VECTORS[1].header_hex);
    Sha256ScanRequest request;
    scan_request_from_header(&request, header.data());

    request.nonce_start = 0u;
    request.nonce_count = 2000000u;
    sha256_target_from_nbits(request.share_target, request.nbits);

    const auto started = std::chrono::steady_clock::now();
    Sha256ScanResult result;
    sha256_scan_avx2(&request, &result);
    const auto finished = std::chrono::steady_clock::now();

    const double seconds = std::chrono::duration<double>(finished - started).count();
    const double rate = (seconds > 0.0) ? ((double)result.nonces_evaluated / seconds) : 0.0;

    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  nonces hashed   : ");
        mmgr_line_decimal(output_line, (unsigned long long)result.nonces_evaluated, 1u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  anchor survivors: ");
        mmgr_line_decimal(output_line, (unsigned long long)result.anchors_survived, 1u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  elapsed         : ");
        mmgr_line_fixed(output_line, (double)(seconds), 3u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " s");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  rate            : ");
        mmgr_line_fixed(output_line, (double)(rate / 1.0e6), 2u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " MH/s (one thread)");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
}

} // namespace

int program_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "========================================================");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  BTC known answer tests");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  Every expected value below was published before this");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  code existed. None of them come from the engine.");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "========================================================");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "\nAVX2 available: ");
        mmgr_line_put(output_line, (sha256_has_avx2() != 0) ? "yes" : "no");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    test_published_digests();
    test_published_headers();
    test_scan_recovers_real_nonce();
    test_arms_agree();
    test_arms_agree_on_easy_target();
    test_difficulty_conversion();
    measure_scan_rate();

    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "\n========================================================");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_decimal_signed(output_line, g_tests_run);
        mmgr_line_put(output_line, " run, ");
        mmgr_line_decimal_signed(output_line, g_tests_failed);
        mmgr_line_put(output_line, " failed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, (g_tests_failed == 0) ? "ALL TESTS PASS" : "FAILURES PRESENT");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "========================================================");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return (g_tests_failed == 0) ? 0 : 1;
}

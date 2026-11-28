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

#include "../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "sha256_core.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

namespace
{

int g_tests_run = 0;
int g_tests_failed = 0;

/** @brief The bytes of a header, of a digest as hex, and of a report's text, each text terminated. */
#define KAT_HEADER_BYTES 80u
#define KAT_HEX_BYTES 65u
#define KAT_TEXT_BYTES 512u

/**
 * @brief Converts a hex string to bytes.
 *
 * @param[in]  hex      Characters to convert, an even count [BORROWS].
 * @param[out] bytes    Where the bytes go [BORROWS].
 * @param[in]  capacity Bytes at bytes.
 * @return              The bytes that they encode, at max capacity.
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
 * @brief Renders bytes as hex in the order they sit in memory.
 *
 * @param[in]  bytes    Bytes to render [BORROWS].
 * @param[in]  count    How many.
 * @param[out] rendered 2 count + 1 bytes for the hex, terminated [BORROWS].
 */
void hex_from_bytes(const uint8_t *bytes, size_t count, char *rendered)
{
    static const char digits[] = "0123456789abcdef";
    for (size_t at = 0u; at < count; at += 1u)
    {
        rendered[2u * at] = digits[bytes[at] >> 4];
        rendered[(2u * at) + 1u] = digits[bytes[at] & 0x0Fu];
    }
    rendered[2u * count] = '\0';
}

/**
 * @brief Renders a digest the way a block explorer prints it, most significant byte first.
 *
 * @param[in]  digest   Thirty-two bytes as SHA-256 emitted them [BORROWS].
 * @param[out] rendered KAT_HEX_BYTES bytes for the reversed hex [BORROWS].
 */
void block_hash_from_digest(const uint8_t *digest, char *rendered)
{
    uint8_t reversed[32];

    for (size_t at = 0u; at < 32u; at += 1u)
    {
        reversed[at] = digest[31u - at];
    }
    hex_from_bytes(reversed, sizeof(reversed), rendered);
}

/** @brief True where two terminated texts are equal. */
bool text_same(const char *one, const char *other)
{
    return EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = one, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = other,
                      .ci = EMBED_FALSE);
}

/** @brief A text of a report, built piece by piece in a fixed buffer. */
struct KatText
{
    char bytes[KAT_TEXT_BYTES];
    VerbaLinea line;
};

/** @brief A text begun empty. */
void text_begun(KatText *text)
{
    text->line = mmgr_line_over(text->bytes, sizeof(text->bytes));
    text->bytes[0] = '\0';
}

/** @brief Up to three pieces of text put at the end of a text, NULL for none. */
KatText *text_put(KatText *text, const char *first, const char *second = nullptr, const char *third = nullptr)
{
    const char *const pieces[3] = {first, second, third};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        if (pieces[at] != nullptr)
        {
            mmgr_line_put(&text->line, pieces[at]);
        }
    }
    (void)mmgr_line_finish(&text->line);
    return text;
}

/** @brief A number put at the end of a text in decimal. */
KatText *text_number(KatText *text, unsigned long long value)
{
    mmgr_line_decimal(&text->line, value, 1u);
    (void)mmgr_line_finish(&text->line);
    return text;
}

/**
 * @brief Records one test outcome and prints it.
 *
 * @param[in] name     The name of the test [BORROWS].
 * @param[in] passed   Nonzero where the test passed.
 * @param[in] detail   The observed result, printed on failure [BORROWS].
 */
void report(const char *name, bool passed, const char *detail = "")
{
    g_tests_run += 1;
    if (passed)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  [PASS] ");
        mmgr_line_put(output_line, name);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    else
    {
        g_tests_failed += 1;
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  [FAIL] ");
            mmgr_line_put(output_line, name);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        if (detail[0] != '\0')
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "         ");
            mmgr_line_put(output_line, detail);
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

    KatText name;
    KatText detail;
    char produced[KAT_HEX_BYTES];
    for (const DigestVector &vector : DIGEST_VECTORS)
    {
        uint8_t digest[32];
        sha256_hash((const uint8_t *)vector.message,
                    EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = vector.message, .cap = PLATFORM_TEXT_TO_TERMINATOR),
                    digest);

        hex_from_bytes(digest, sizeof(digest), produced);
        text_begun(&name);
        text_begun(&detail);
        report(text_put(&name, "sha256 ", vector.source)->bytes, text_same(produced, vector.expected_digest),
               text_put(text_put(&detail, "got ", produced, ", expected "), vector.expected_digest)->bytes);
    }

    // A message of exactly fifty-six bytes forces the length field into a second padding block. That
    // is the boundary a padding bug hides behind.
    uint8_t boundary[56];
    EMBED_CALL(memor.set, MemoriaCfg, .dst = boundary, .bytes = sizeof(boundary), .val = 'a');
    uint8_t digest[32];
    sha256_hash(boundary, sizeof(boundary), digest);
    hex_from_bytes(digest, sizeof(digest), produced);
    text_begun(&detail);
    report("sha256 fifty-six byte padding boundary",
           text_same(produced, "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a"),
           text_put(&detail, "got ", produced)->bytes);
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

    KatText name;
    KatText detail;
    for (const HeaderVector &vector : HEADER_VECTORS)
    {
        uint8_t header[KAT_HEADER_BYTES + 1u];
        const size_t header_size = bytes_from_hex(vector.header_hex, header, sizeof(header));
        if (header_size != BITCOIN_HEADER_BYTES)
        {
            text_begun(&name);
            text_begun(&detail);
            report(text_put(&name, "header length for ", vector.source)->bytes, false,
                   text_put(text_number(text_put(&detail, "got "), header_size), " bytes, expected 80")->bytes);
            continue;
        }

        uint8_t digest[32];
        sha256_double_hash(header, header_size, digest);

        char produced[KAT_HEX_BYTES];
        block_hash_from_digest(digest, produced);
        text_begun(&name);
        text_begun(&detail);
        report(text_put(&name, "hash of ", vector.source)->bytes, text_same(produced, vector.expected_hash),
               text_put(text_put(&detail, "got ", produced, ", expected "), vector.expected_hash)->bytes);

        // The nonce sits at header bytes seventy-six through seventy-nine, little-endian.
        const uint32_t header_nonce = (uint32_t)header[76] | ((uint32_t)header[77] << 8) |
                                      ((uint32_t)header[78] << 16) | ((uint32_t)header[79] << 24);
        text_begun(&name);
        text_begun(&detail);
        report(text_put(&name, "nonce field of ", vector.source)->bytes, header_nonce == vector.expected_nonce,
               text_number(text_put(&detail, "got "), header_nonce)->bytes);

        // The block's own hash must satisfy the block's own nbits. If it does not, either the target
        // expansion or the ordering is wrong, and both feed the share check.
        const uint32_t nbits = (uint32_t)header[72] | ((uint32_t)header[73] << 8) | ((uint32_t)header[74] << 16) |
                               ((uint32_t)header[75] << 24);
        uint32_t block_target[SHA256_STATE_WORDS];
        sha256_target_from_nbits(block_target, nbits);
        text_begun(&name);
        report(text_put(&name, "nbits ordering for ", vector.source)->bytes,
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

    KatText name;
    KatText detail;
    for (const HeaderVector &vector : HEADER_VECTORS)
    {
        uint8_t header[KAT_HEADER_BYTES];
        (void)bytes_from_hex(vector.header_hex, header, sizeof(header));
        Sha256ScanRequest request;
        scan_request_from_header(&request, header);

        // Start the window below the real nonce so the arm has to search instead of land on it. The
        // window is the smallest that still proves the arm walks forward into the answer.
        const uint32_t window = 4096u;
        request.nonce_start = vector.expected_nonce - (window / 2u);
        request.nonce_count = window;
        sha256_target_from_nbits(request.share_target, header_field_value(header, 72u));

        Sha256ScanResult scalar_result;
        sha256_scan_scalar(&request, &scalar_result);
        text_begun(&name);
        text_begun(&detail);
        report(text_put(&name, "scalar arm finds nonce of ", vector.source)->bytes,
               (scalar_result.found != 0) && (scalar_result.winning_nonce == vector.expected_nonce),
               scalar_result.found ? text_number(text_put(&detail, "got "), scalar_result.winning_nonce)->bytes
                                   : "FALSE NEGATIVE: no nonce found in a window containing one");

        Sha256ScanResult vector_result;
        sha256_scan_avx2(&request, &vector_result);
        text_begun(&name);
        text_begun(&detail);
        report(text_put(&name, "eight-lane arm finds nonce of ", vector.source)->bytes,
               (vector_result.found != 0) && (vector_result.winning_nonce == vector.expected_nonce),
               vector_result.found ? text_number(text_put(&detail, "got "), vector_result.winning_nonce)->bytes
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

    uint8_t header[KAT_HEADER_BYTES];
    (void)bytes_from_hex(HEADER_VECTORS[1].header_hex, header, sizeof(header));
    Sha256ScanRequest request;
    scan_request_from_header(&request, header);

    // A window well away from the real nonce, at the block's own difficulty. Nothing here wins.
    request.nonce_start = 1000000u;
    request.nonce_count = 65536u;
    sha256_target_from_nbits(request.share_target, request.nbits);

    Sha256ScanResult scalar_result;
    Sha256ScanResult vector_result;
    sha256_scan_scalar(&request, &scalar_result);
    sha256_scan_avx2(&request, &vector_result);

    KatText detail;
    text_begun(&detail);
    report(
        "neither arm invents a winner", (scalar_result.found == 0) && (vector_result.found == 0),
        text_number(text_put(text_number(text_put(&detail, "scalar found="), (unsigned long long)scalar_result.found),
                             " vector found="),
                    (unsigned long long)vector_result.found)
            ->bytes);
    text_begun(&detail);
    report("both arms evaluated every nonce",
           (scalar_result.nonces_evaluated == request.nonce_count) &&
               (vector_result.nonces_evaluated == request.nonce_count),
           text_number(text_put(text_number(text_put(&detail, "scalar "), scalar_result.nonces_evaluated), " vector "),
                       vector_result.nonces_evaluated)
               ->bytes);

    // The anchor passes at a rate of two to the minus thirty-two. A nonce window of sixty-five
    // thousand should almost never produce a survivor. The test checks that the arms agree, and
    // only reports the count.
    text_begun(&detail);
    report("arms agree on anchor survivors", scalar_result.anchors_survived == vector_result.anchors_survived,
           text_number(text_put(text_number(text_put(&detail, "scalar "), scalar_result.anchors_survived), " vector "),
                       vector_result.anchors_survived)
               ->bytes);
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

    uint8_t header[KAT_HEADER_BYTES];
    (void)bytes_from_hex(HEADER_VECTORS[1].header_hex, header, sizeof(header));
    Sha256ScanRequest request;
    scan_request_from_header(&request, header);

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

    KatText detail;
    text_begun(&detail);
    report(
        "both arms find a winner at an easy target", (scalar_result.found != 0) && (vector_result.found != 0),
        text_number(text_put(text_number(text_put(&detail, "scalar found="), (unsigned long long)scalar_result.found),
                             " vector found="),
                    (unsigned long long)vector_result.found)
            ->bytes);
    text_begun(&detail);
    report("both arms report the same lowest nonce", scalar_result.winning_nonce == vector_result.winning_nonce,
           text_number(text_put(text_number(text_put(&detail, "scalar "), scalar_result.winning_nonce), " vector "),
                       vector_result.winning_nonce)
               ->bytes);

    // The winner has to survive an independent check that never touches the scan path.
    if (scalar_result.found != 0)
    {
        uint8_t candidate[KAT_HEADER_BYTES];
        (void)bytes_from_hex(HEADER_VECTORS[1].header_hex, candidate, sizeof(candidate));
        const uint32_t nonce = scalar_result.winning_nonce;
        candidate[76] = (uint8_t)nonce;
        candidate[77] = (uint8_t)(nonce >> 8);
        candidate[78] = (uint8_t)(nonce >> 16);
        candidate[79] = (uint8_t)(nonce >> 24);

        uint8_t digest[32];
        sha256_double_hash(candidate, sizeof(candidate), digest);
        char hash[KAT_HEX_BYTES];
        block_hash_from_digest(digest, hash);
        text_begun(&detail);
        report("winner verified by an independent double hash",
               sha256_digest_within_target(digest, request.share_target) != 0,
               text_put(&detail, "hash ", hash, " is not within the target")->bytes);
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
    KatText detail;

    // Difficulty one is 0xFFFF times two to the two hundred eighth. Those bits land at positions 208
    // through 223: the upper half of word one. Thus word one reads 0xFFFF0000, and not 0x0000FFFF.
    sha256_share_target_from_difficulty(target, 1.0);
    text_begun(&detail);
    report(
        "difficulty one is 0x00000000FFFF0000...",
        (target[0] == 0x00000000u) && (target[1] == 0xFFFF0000u) && (target[2] == 0u) && (target[7] == 0u),
        text_number(text_put(text_number(text_put(&detail, "word zero "), target[0]), " word one "), target[1])->bytes);

    sha256_share_target_from_difficulty(target, 2.0);
    text_begun(&detail);
    report("difficulty two halves the threshold", (target[0] == 0x00000000u) && (target[1] == 0x7FFF8000u),
           text_number(text_put(&detail, "word one is "), target[1])->bytes);

    // nbits 0x1d00ffff is difficulty one in the form that a header uses. Thus the two paths must
    // give the same threshold.
    uint32_t from_nbits[SHA256_STATE_WORDS];
    sha256_target_from_nbits(from_nbits, 0x1d00ffffu);
    sha256_share_target_from_difficulty(target, 1.0);
    text_begun(&detail);
    report("nbits 0x1d00ffff agrees with difficulty one",
           EMBED_CALL(memor.cmp, MemoriaCfg, .src = from_nbits, .other = target, .bytes = sizeof(target)) == 0,
           text_number(text_put(&detail, "nbits path word one is "), from_nbits[1])->bytes);

    // Every target the pool can set must leave the anchor sound. Section 2.2 is only satisfied where
    // the most significant word is zero, and the engine falls back where it is not.
    sha256_share_target_from_difficulty(target, 1.0);
    text_begun(&detail);
    report("difficulty one leaves the anchor sound", target[0] == 0u,
           text_number(text_put(&detail, "top word is "), target[0])->bytes);
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

    uint8_t header[KAT_HEADER_BYTES];
    (void)bytes_from_hex(HEADER_VECTORS[1].header_hex, header, sizeof(header));
    Sha256ScanRequest request;
    scan_request_from_header(&request, header);

    request.nonce_start = 0u;
    request.nonce_count = 2000000u;
    sha256_target_from_nbits(request.share_target, request.nbits);

    const unsigned long long started = platform_clock_ns();
    Sha256ScanResult result;
    sha256_scan_avx2(&request, &result);
    const unsigned long long finished = platform_clock_ns();

    const double seconds = (double)(finished - started) / 1.0e9;
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

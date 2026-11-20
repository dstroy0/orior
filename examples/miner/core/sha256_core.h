// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file sha256_core.h
 * @brief SHA-256 as the standard defines it, plus the eight-lane arm the miner actually runs.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-09-08
 *
 * @note Both arms compute the same function. The scalar arm is the reference and the eight-lane arm
 *       is under measurement, and a disagreement between them is a defect.
 * @note Nothing here reads a clock, opens a socket or prints a row. Those belong to the client.
 */
#ifndef SHA256_CORE_H
#define SHA256_CORE_H
#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <stdint.h>

/** @brief Words in a SHA-256 chaining value. */
#define SHA256_STATE_WORDS 8u

/** @brief Words in one SHA-256 message block. */
#define SHA256_BLOCK_WORDS 16u

/** @brief Bytes in one SHA-256 message block. */
#define SHA256_BLOCK_BYTES 64u

/** @brief Bytes in a Bitcoin block header. */
#define BITCOIN_HEADER_BYTES 80u

/** @brief Nonces the eight-lane arm evaluates per call. */
#define SHA256_LANES 8u

    /** @brief A SHA-256 chaining value, big-endian words in the standard's own order. */
    typedef struct
    {
        uint32_t word[SHA256_STATE_WORDS];
    } Sha256State;

    /**
     * @brief Sets a chaining value to the standard's initial value.
     *
     * @param[out] state Chaining value to initialize [BORROWS].
     */
    void sha256_state_init(Sha256State *state);

    /**
     * @brief Runs the compression function over one message block.
     *
     * @param[in,out] state         Chaining value, advanced in place [BORROWS].
     * @param[in]     message_block Sixteen big-endian words of message [BORROWS].
     * @note This is the reference arm. Every other arm has to agree with it or its measurement is
     *       void.
     */
    void sha256_block_compress(Sha256State *state, const uint32_t *message_block);

    /**
     * @brief Runs the compression function for a chosen number of rounds and no further.
     *
     * @param[in,out] state         Chaining value, advanced in place [BORROWS].
     * @param[in]     message_block Sixteen big-endian words of message [BORROWS].
     * @param[in]     rounds        How many of the sixty-four rounds to run, zero through 64.
     * @note This is the standard reduced-round variant, used to ask how far a structure in the
     *       input survives. At sixty-four it must agree with sha256_block_compress exactly, and a
     *       test asserts that. Thus the instrument cannot drift from the thing that it measures.
     * @note Not a hashing primitive. A reduced-round SHA-256 is not SHA-256 and nothing that mines
     *       may call this.
     */
    void sha256_block_compress_partial(Sha256State *state, const uint32_t *message_block,
                                       unsigned rounds);

    /**
     * @brief Hashes a byte range, handling padding and length encoding.
     *
     * @param[in]  message     Bytes to hash [BORROWS].
     * @param[in]  message_len How many.
     * @param[out] digest      Thirty-two bytes of output [BORROWS].
     */
    void sha256_hash(const uint8_t *message, size_t message_len, uint8_t *digest);

    /**
     * @brief Hashes a byte range twice, the operation Bitcoin means by a hash.
     *
     * @param[in]  message     Bytes to hash [BORROWS].
     * @param[in]  message_len How many.
     * @param[out] digest      Thirty-two bytes of output [BORROWS].
     */
    void sha256_double_hash(const uint8_t *message, size_t message_len, uint8_t *digest);

    /**
     * @brief Compresses the first sixty-four header bytes, which no nonce can change.
     *
     * @param[out] midstate Chaining value after the first block [BORROWS].
     * @param[in]  header   Eighty header bytes. The function reads the first sixty-four [BORROWS].
     * @note Version, previous hash and all but the last four merkle root bytes live in that block.
     *       Thus the miner computes this once per extranonce, and never once per nonce.
     */
    void sha256_header_midstate(Sha256State *midstate, const uint8_t *header);

    /** @brief What a scan call needs, assembled once per nonce range. */
    typedef struct
    {
        Sha256State midstate;       /**< Chaining value after header bytes 0 through 63. */
        uint32_t merkle_root_tail;  /**< Header bytes 64 through 67, as one big-endian word. */
        uint32_t ntime;             /**< Header bytes 68 through 71, as one big-endian word. */
        uint32_t nbits;             /**< Header bytes 72 through 75, as one big-endian word. */
        uint32_t nonce_start;       /**< First nonce to evaluate. */
        uint32_t nonce_count;       /**< How many to evaluate, ascending from nonce_start. */
        uint32_t share_target[SHA256_STATE_WORDS]; /**< Share threshold, most significant word first. */
        const volatile int *abandon; /**< Scan stops early where this turns nonzero [BORROWS]. */
    } Sha256ScanRequest;

    /** @brief What a scan call reports back. */
    typedef struct
    {
        uint64_t nonces_evaluated; /**< How many nonces the arm hashed. */
        uint64_t anchors_survived; /**< How many passed the anchor and reached the exact compare. */
        uint32_t winning_nonce;    /**< The nonce that met the target, where found is nonzero. */
        int found;                 /**< Nonzero where a nonce met the share target. */
    } Sha256ScanResult;

    /**
     * @brief Scans a nonce range for a header hash at or below the share target, one nonce at a time.
     *
     * @param[in]  request Range, header tail and threshold [BORROWS].
     * @param[out] result  Counts and the nonce that wins [BORROWS].
     * @note The reference arm. Correct, and slow enough that only the tests call it.
     */
    void sha256_scan_scalar(const Sha256ScanRequest *request, Sha256ScanResult *result);

    /**
     * @brief The same scan. It skips the work that a nonce cannot change and the anchor cannot read.
     *
     * @param[in]  request What to scan [BORROWS].
     * @param[out] result  Where the outcome goes [BORROWS].
     * @note Two exact savings, both priced in section 2b of theory/workbooks/orior/sha256-topology.md.
     *       The nonce is message word three, and W[at] draws on at-16, at-15, at-7 and at-2.
     *       Thus the first eighteen schedule words and the first three rounds of the first block are
     *       identical for every nonce. The function moves them out of the loop.
     * @note The round moves e to f to g to h. Thus the final word seven is the e of sixty-one
     *       rounds. The last three rounds of the second block cannot change the anchor, and the
     *       function does not run them.
     * @note The arithmetic of sha256_scan_scalar checks again each nonce that passes the anchor.
     *       A defect here can cost throughput, but cannot produce a false share.
     * @warning Where the share target leaves word zero nonzero, the anchor is not a necessary
     *          condition. Then this function defers to sha256_scan_scalar, and does not refuse a
     *          nonce that wins.
     */
    void sha256_scan_scalar_early(const Sha256ScanRequest *request, Sha256ScanResult *result);

    /**
     * @brief The anchor word one nonce produces, by the shortened route.
     *
     * @param[in] request The header to scan [BORROWS].
     * @param[in] nonce   The nonce to try.
     * @return            Word seven of the doubled digest.
     * @note This lets a test check the shortcut against the full arithmetic on every nonce, and not
     *       only where the anchor survives. A survivor is a one in four billion event. A scan that
     *       finds no survivor does not test the shortcut. It only shows that two arms agree on an
     *       empty result.
     * @warning Prepares the header again on every call. Thus this is for a check, and not for a
     *          scan. sha256_scan_scalar_early prepares once and reuses it.
     */
    uint32_t sha256_anchor_word(const Sha256ScanRequest *request, uint32_t nonce);

    /**
     * @brief Scans a nonce range eight nonces at a time with AVX2.
     *
     * @param[in]  request Range, header tail and threshold [BORROWS].
     * @param[out] result  Counts and the nonce that wins [BORROWS].
     * @note Reports the lowest nonce in the range that wins, and a split across threads gives the
     *       same answer as one run whole.
     * @warning Requires AVX2. Call sha256_has_avx2 before a dispatch to this function.
     */
    void sha256_scan_avx2(const Sha256ScanRequest *request, Sha256ScanResult *result);

    /**
     * @brief Reports whether this processor carries AVX2.
     *
     * @return Nonzero where the eight-lane arm is legal to call.
     */
    int sha256_has_avx2(void);

    /**
     * @brief What a survey accumulates over a nonce range. Never early-exits.
     *
     * @note The survey covers the whole digest, and not the anchor window. A structure away from the
     *       leading bits would be invisible to the anchor and still be real. Thus the survey counts
     *       every bit position and every byte position separately.
     */
    typedef struct
    {
        uint64_t nonces_evaluated;       /**< How many nonces the survey hashed. */
        uint64_t leading_zero_count[33]; /**< How many digests carried at least k leading zero bits. */
        uint64_t byte_histogram[256];    /**< How often each byte value appeared, over all positions. */
        uint64_t bit_one_count[256];     /**< How often each of the 256 bit positions came out as one. */
        uint64_t position_histogram[32][256]; /**< Per byte position, how often each value appeared. */
    } Sha256Survey;

    /**
     * @brief Hashes a nonce range and accumulates its statistics. It does not stop early.
     *
     * @param[in]  request Range and header tail. The function ignores share_target and abandon [BORROWS].
     * @param[out] survey  Where the counts accumulate, added to what is already there [BORROWS].
     * @note This tests the cost model of the anchor sift against the digest domain, and does not
     *       assume it. The model predicts a survivor count of the range size times two to the minus k
     *       for a k-bit anchor. It predicts a collision entropy of eight bits for a digest byte.
     *       This function measures both, and does not assert them.
     */
    void sha256_survey_avx2(const Sha256ScanRequest *request, Sha256Survey *survey);

    /**
     * @brief Builds a share target from a pool difficulty.
     *
     * @param[out] share_target Eight words, most significant first [BORROWS].
     * @param[in]  difficulty   Pool difficulty, greater than zero.
     * @note Difficulty one is the threshold 0x00000000FFFF0000... that the protocol names, and every
     *       other difficulty divides it.
     */
    void sha256_share_target_from_difficulty(uint32_t *share_target, double difficulty);

    /**
     * @brief Expands a compact nbits encoding into a full threshold.
     *
     * @param[out] block_target Eight words, most significant first [BORROWS].
     * @param[in]  nbits        Compact encoding as the header carries it.
     */
    void sha256_target_from_nbits(uint32_t *block_target, uint32_t nbits);

    /**
     * @brief Orders a digest against a threshold the way the protocol reads them.
     *
     * @param[in] digest    Thirty-two bytes as SHA-256 emitted them [BORROWS].
     * @param[in] threshold Eight words, most significant first [BORROWS].
     * @return              Nonzero where the digest is at or below the threshold.
     * @note The protocol reads a digest little-endian. Thus the last digest byte is the most
     *       significant. Because of that reversal, the anchor sits on the final state word.
     */
    int sha256_digest_within_target(const uint8_t *digest, const uint32_t *threshold);

#ifdef __cplusplus
}
#endif

#endif

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file cuda_miner.h
 * @brief What the host needs to drive the device arm, with no CUDA types in the interface.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note A translation unit that includes this does not need nvcc. That keeps the client compilable
 *       by the same g++ that builds everything else, with the device arm linked in when present.
 */
#ifndef CUDA_MINER_H
#define CUDA_MINER_H

#include <cstddef>
#include <cstdint>

/** @brief Everything one launch needs, copied to the device by value. */
struct CudaScanParameters
{
    uint32_t midstate[8];      /**< Chaining value after header bytes 0 through 63. */
    uint32_t merkle_root_tail; /**< Header bytes 64 through 67, as a schedule word. */
    uint32_t ntime;            /**< Header bytes 68 through 71, as a schedule word. */
    uint32_t nbits;            /**< Header bytes 72 through 75, as a schedule word. */
    uint32_t share_target[8];  /**< Share threshold, most significant word first. */
    uint32_t anchor_is_sound;  /**< Nonzero where the target leaves the top word at zero. */

    /** @brief The chaining value after the rounds no nonce can reach.
     *  @note cuda_miner_scan fills this, and the caller does not. The nonce is message word three,
     *        and W[at] draws on at-16, at-15, at-7 and at-2. Thus the first three rounds read only
     *        words that no nonce touches, and produce the same state for every nonce. One
     *        computation per launch, and not one per thread, removes work that every thread
     *        would otherwise repeat. */
    uint32_t shared_state[8];

    /** @brief Schedule words sixteen and seventeen, which no nonce reaches either.
     *  @note cuda_miner_scan fills this. W16 draws on W0, W1, W9 and W14; W17 on W1, W2, W10 and
     *        W15. Word eighteen is the first to draw on word three. Thus it is the first word that a
     *        thread must compute for itself. */
    uint32_t shared_schedule[2];
};

struct CudaMinerContext;

/**
 * @brief Allocates the device buffers one miner needs.
 *
 * @param[in] found_limit How many nonces that win one launch may report.
 * @return                A context, or NULL where the device refused. [OWNS]
 */
CudaMinerContext *cuda_miner_create(uint32_t found_limit);

/**
 * @brief Releases a context.
 *
 * @param[in] context What to release [OWNS].
 */
void cuda_miner_destroy(CudaMinerContext *context);

/**
 * @brief How many nonces cleared the anchor in the last scan on this context.
 *
 * @param[in] context Device buffers [BORROWS].
 * @return            The count, or zero where the context is null or no scan ran.
 * @note Separate from the winner count that cuda_miner_scan reports. A winner clears the
 *       anchor and then also comes in under the threshold; a survivor only clears the anchor. The
 *       client shows the survivor count beside the figure of the CPU arm. The winner count under
 *       that name would make the device look as though it found no survivors.
 * @note Valid only until the next scan on the same context, which resets it.
 */
unsigned long long cuda_miner_anchor_survivors(const CudaMinerContext *context);

/**
 * @brief Scans a nonce range on the device.
 *
 * @param[in]  context      Device buffers [BORROWS].
 * @param[in]  parameters   Header tail, midstate and threshold [BORROWS].
 * @param[in]  nonce_base   First nonce to evaluate.
 * @param[in]  nonce_count  How many to evaluate.
 * @param[out] found_nonces Where winners land, at most found_limit of them [BORROWS].
 * @param[out] found_count  How many winners the scan wrote [BORROWS].
 * @return                  Nonzero on success. Zero means the launch failed and nothing is valid.
 * @note The scan reports winners in no order. A caller that wants the lowest must take the minimum.
 */
int cuda_miner_scan(CudaMinerContext *context, const CudaScanParameters *parameters,
                    uint32_t nonce_base, uint32_t nonce_count, uint32_t *found_nonces,
                    uint32_t *found_count);

/**
 * @brief Scans a range of positions on the golden spiral: position k is nonce salt + k * 0x9E3779B9.
 *
 * @param[in]  context        Device buffers [BORROWS].
 * @param[in]  parameters     Header tail, midstate and threshold [BORROWS].
 * @param[in]  salt           Where the spiral starts, any word in the nonce space.
 * @param[in]  position_base  First position to evaluate.
 * @param[in]  position_count How many positions to evaluate.
 * @param[out] found_nonces   Where winners land, as nonces and not as positions [BORROWS].
 * @param[out] found_count    How many winners the scan wrote [BORROWS].
 * @return                    Nonzero on success. Zero means the launch failed and nothing is valid.
 * @note The step is odd. Thus positions 0 through 2^32 - 1 visit every nonce exactly once. A whole
 *       sweep returns exactly the winners that cuda_miner_scan returns over the whole space. Only
 *       the order in which the sweep reaches the nonces differs.
 */
int cuda_miner_scan_salted(CudaMinerContext *context, const CudaScanParameters *parameters,
                           uint32_t salt, uint32_t position_base, uint32_t position_count,
                           uint32_t *found_nonces, uint32_t *found_count);

/**
 * @brief Scans a nonce range with none of the savings: the raw search.
 *
 * @param[in]  context      Device buffers [BORROWS].
 * @param[in]  parameters   Header tail, midstate and threshold. The scan does not read
 *                          anchor_is_sound [BORROWS].
 * @param[in]  nonce_base   First nonce to evaluate.
 * @param[in]  nonce_count  How many to evaluate.
 * @param[out] found_nonces Where winners land, at most found_limit of them [BORROWS].
 * @param[out] found_count  How many winners the scan wrote [BORROWS].
 * @return                  Nonzero on success. Zero means the launch failed and nothing is valid.
 * @note Returns exactly the winners that cuda_miner_scan returns. Its purpose is a timed comparison with
 *       that scan. Both compressions run whole for every nonce, no work moves out of the loop, and no
 *       anchor refuses early.
 * @note Leaves cuda_miner_anchor_survivors at zero, since nothing here is an anchor.
 */
int cuda_miner_scan_raw(CudaMinerContext *context, const CudaScanParameters *parameters,
                        uint32_t nonce_base, uint32_t nonce_count, uint32_t *found_nonces,
                        uint32_t *found_count);

/** @brief What a device survey accumulates. No early exit, every nonce counted. */
struct CudaSurvey
{
    unsigned long long nonces_evaluated;       /**< How many nonces the survey hashed. */
    unsigned long long bit_one_count[256];     /**< How often each digest bit position came out as one. */
    unsigned long long leading_zero_count[33]; /**< How many digests carried at least k zero bits. */
};

/**
 * @brief Surveys a nonce range on the device, and accumulates bit statistics.
 *
 * @param[in]     context     Device buffers [BORROWS].
 * @param[in]     parameters  Header tail and midstate. The survey ignores share_target [BORROWS].
 * @param[in]     nonce_base  First nonce to evaluate.
 * @param[in]     nonce_count How many to evaluate.
 * @param[in,out] survey      Counts, added to whatever is already there [BORROWS].
 * @return                    Nonzero on success.
 * @note The digest field carries no noise. H(digest | input) is zero. Only the sample size limits a
 *       bias bound measured here, and no floor sits beneath it. More samples tighten the bound
 *       indefinitely. Thus the device is worth its time on this survey.
 */
int cuda_miner_survey(CudaMinerContext *context, const CudaScanParameters *parameters,
                      uint32_t nonce_base, uint32_t nonce_count, CudaSurvey *survey);

/**
 * @brief Salts the nonce and counts which output bits respond. The keyhole scan.
 *
 * @param[in]     context              Device buffers [BORROWS].
 * @param[in]     parameters           Header tail and midstate [BORROWS].
 * @param[in]     nonce_base           First nonce.
 * @param[in]     nonce_count          How many.
 * @param[in]     salt                 Input difference applied to the nonce.
 * @param[in,out] difference_one_count 256 counters, added to [BORROWS].
 * @return                             Nonzero on success.
 * @note A keyhole is an output bit whose difference is not a fair coin under a fixed input salt.
 *       Under a flat field every counter sits at half the sample size, and any departure is a place
 *       the construction gives something away.
 */
int cuda_miner_keyhole(CudaMinerContext *context, const CudaScanParameters *parameters,
                       uint32_t nonce_base, uint32_t nonce_count, uint32_t salt,
                       unsigned long long *difference_one_count);

/**
 * @brief Describes the device, for a caller that wants to print it.
 *
 * @param[out] text      Where the description lands [BORROWS].
 * @param[in]  text_size How much room it has.
 * @return               Multiprocessor count, or zero where no device is visible.
 */
int cuda_miner_device_report(char *text, size_t text_size);

#endif

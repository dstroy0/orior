// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file cuda_miner.h
 * @brief The interface from host to device, with a dynamic choice of scan arm and a structural sift.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note Every scan arm has the same signature and returns the same count. Thus a driver can call
 *       them through one pointer. A disagreement between two of them is a defect.
 */
#ifndef CUDA_MINER_H
#define CUDA_MINER_H



#include <cstddef>
#include <cstdint>

/** @brief Everything one launch needs, copied to the device by value. */
struct CudaScanParameters
{
    uint32_t midstate[8];         /**< Chaining value after header bytes 0 through 63. */
    uint32_t merkle_root_tail;    /**< Header bytes 64 through 67, as a schedule word. */
    uint32_t ntime;               /**< Header bytes 68 through 71, as a schedule word. */
    uint32_t nbits;               /**< Header bytes 72 through 75, as a schedule word. */
    uint32_t share_target[8];     /**< Share threshold, most significant word first. */
    uint32_t anchor_is_sound;     /**< Nonzero where the target leaves the top word at zero. */

    uint32_t shared_state[8];     /**< Chaining value through rounds untouched by the nonce. */
    uint32_t shared_schedule[2];  /**< Pre-computed schedule words W16 and W17. */
};

struct CudaMinerContext;

/**
 * @brief One scan arm: evaluate a nonce range on the device with its own sift logic.
 *
 * @param[in]  context      Device buffers [BORROWS].
 * @param[in]  parameters   Header tail, midstate and threshold [BORROWS].
 * @param[in]  nonce_base   First nonce to evaluate.
 * @param[in]  nonce_count  How many to evaluate.
 * @param[out] found_nonces Where winners land, at most found_limit [BORROWS].
 * @param[out] found_count  How many winners the scan wrote [BORROWS].
 * @return                  Nonzero on success. Zero means the launch failed.
 */
typedef int (*CudaScanArm)(CudaMinerContext *context, const CudaScanParameters *parameters,
                           uint32_t nonce_base, uint32_t nonce_count, 
                           uint32_t *found_nonces, uint32_t *found_count);

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Scans a nonce range with naive brute-force evaluation.
 */
int cuda_miner_scan_naive(CudaMinerContext *context, const CudaScanParameters *parameters,
                          uint32_t nonce_base, uint32_t nonce_count, 
                          uint32_t *found_nonces, uint32_t *found_count);

/**
 * @brief Scans with the round-61 anchor early-out, the sound advantage. Finds the same winners as
 *        the raw arm. It refuses all but one nonce in four billion before the last three rounds.
 */
int cuda_miner_scan_inorder(CudaMinerContext *context, const CudaScanParameters *parameters,
                            uint32_t nonce_base, uint32_t nonce_count,
                            uint32_t *found_nonces, uint32_t *found_count);

/**
 * @brief The place for the survivor-restricted arm of the engine. Until that arm exists in the tree,
 *        this resolves to the same sound anchored kernel as the inorder arm.
 */
int cuda_miner_scan_free(CudaMinerContext *context, const CudaScanParameters *parameters,
                         uint32_t nonce_base, uint32_t nonce_count,
                         uint32_t *found_nonces, uint32_t *found_count);

/**
 * @brief Returns the scan arm to run for these parameters: the anchored arm where the anchor is
 *        sound, the raw arm otherwise. The choice affects speed, and never correctness.
 */
CudaScanArm cuda_miner_choose(const CudaScanParameters *parameters);

#ifdef __cplusplus
}
#endif


/**
 * @brief Names the scan arm the dispatcher would choose, for reporting.
 */
const char *cuda_miner_arm_name(CudaScanArm arm);

/** @brief Allocates the device buffers one miner needs. */
CudaMinerContext *cuda_miner_create(uint32_t found_limit);

/** @brief Releases a context. */
void cuda_miner_destroy(CudaMinerContext *context);

/** @brief How many nonces cleared the anchor in the last scan on this context. */
unsigned long long cuda_miner_anchor_survivors(const CudaMinerContext *context);

/** @brief What a device survey accumulates. No early exit, every nonce counted. */
struct CudaSurvey
{
    unsigned long long nonces_evaluated;       /**< How many nonces the survey hashed. */
    unsigned long long bit_one_count[256];     /**< How often each digest bit position came out as one. */
    unsigned long long leading_zero_count[33]; /**< How many digests carried at least k zero bits. */
};

/** @brief Surveys a nonce range on the device, and accumulates bit statistics. */
int cuda_miner_survey(CudaMinerContext *context, const CudaScanParameters *parameters,
                      uint32_t nonce_base, uint32_t nonce_count, CudaSurvey *survey);

/** @brief Salts the nonce and counts which output bits respond (Keyhole scan). */
int cuda_miner_keyhole(CudaMinerContext *context, const CudaScanParameters *parameters,
                       uint32_t nonce_base, uint32_t nonce_count, uint32_t salt,
                       unsigned long long *difference_one_count);

/** @brief Describes the device, for a caller that wants to print it. */
int cuda_miner_device_report(char *text, size_t text_size);


#endif
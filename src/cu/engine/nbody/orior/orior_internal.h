// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// What the orior_*.c pieces share: its includes, types and the functions one piece calls in another
#ifndef ORIOR_INTERNAL_H
#define ORIOR_INTERNAL_H

/*
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file orior_internal.h
 * @brief The engine: search, steering and scan, with no clock and no output in any of them.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note Every engine is sound: a subset of a pattern's points is a necessary condition. None of them
 *       can lose a true occurrence. What differs between them is how much they read and how much of
 *       that reading the machine can overlap.
 */

#include "orior.h"
#include "../../../includes/types/integers/exact_integer.h"

#include <string.h>

/* The dispatch rule in orior_steer_prefers_free compares 100 * total^2 against
 * 85 * distinct * sum(count^2) for a census whose total is a 64 bit count. The right side is below
 * 2^7 * 2^8 * 2^128 = 2^143: 85 is below 2^7, at most 256 symbols are distinct, and a sum of squared
 * counts is at most total^2. The narrowest power of two width holding 143 bits is 256, which is
 * 8 limbs. Narrower, the rule errors on a large enough corpus, and which engine a corpus is given
 * would change with the width. The engine errors on the width here instead. The exact integer on its
 * own builds and grades down to 1 limb. Written in the three forms exact_integer.h uses for its
 * asserts: static_assert for C++, _Static_assert for C11, and a negative array size before C11. */
#if defined(__cplusplus)
static_assert(ORIOR_EXACT_BITS >= 256u, "the steering rule needs 143 bits; build the engine at 8 exact limbs or more");
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(ORIOR_EXACT_BITS >= 256u,
               "the steering rule needs 143 bits; build the engine at 8 exact limbs or more");
#else
typedef char orior_steering_rule_fits_the_width[(ORIOR_EXACT_BITS >= 256u) ? 1 : -1];
#endif

void choose_offsets(size_t *offsets, size_t wanted, size_t needle_len);

void orior_field_census(const uint8_t *corpus, size_t corpus_len, OriorFieldCensus *census);

void orior_steer_probe_order(size_t *offsets, size_t count, const OriorFieldCensus *census, const uint8_t *needle,
                              size_t needle_len);

int orior_steer_prefers_free(const OriorFieldCensus *census);

size_t steer_truthy_after(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle, size_t needle_len,
                          const uint8_t *alive, size_t offset, size_t stride, const OriorField *any);

size_t steer_truthy_total(const uint8_t *alive, size_t alignments, size_t stride);

void steer_make_falsy(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle, size_t needle_len,
                      uint8_t *alive, size_t offset, const OriorField *any);

int orior_steer_probe_fits(const OriorProbe *probe, size_t needle_len);

size_t steer_truthy_after_probe(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle, size_t needle_len,
                                const uint8_t *alive, const OriorProbe *probe, size_t stride);

void steer_make_falsy_probe(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle, size_t needle_len,
                            uint8_t *alive, const OriorProbe *probe);

const OriorSteerEngine *orior_steer_best_engine(void);

#endif

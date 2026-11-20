// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/*
 *
 * Every use falls under AGPL-3.0-or-later unless you have explicit permission. That permission is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file exact_integer.h
 * @brief An exact integer stored as a fixed width array of limbs, and the operations a measure needs.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note A limb array is a transform of an integer, the same way decimal text is one. The value is
 *       identical in every form. What changes is which machine can work on it.
 *       Python carries the arbitrary precision form, and this carries the fixed width form. A GPU
 *       carries the same fixed width form, one warp to a number. No arm gets its own arithmetic
 *       doctrine.
 * @note The compile fixes the width, because a GPU register file cannot grow at run time. That is
 *       the only bound of this representation. A declaration sets it, and no discovery finds it.
 *       Each entry point refuses a value that does not fit, and does not truncate it.
 * @note Base 2^32 with a 64 bit accumulator. Wider limbs would need 128 bit products, which are a
 *       compiler extension on some targets and absent on others. 32 bit limbs also map onto both
 *       SIMD widths this tree builds for and onto a CUDA lane. One representation serves every
 *       arm.
 * @note The reference implementation is portable C11. The tests check each vectorized arm against
 *       it. An arm is wrong where it disagrees, whatever it measures.
 * @note Division is integer division: a quotient and a remainder, an exact quotient that errors on a
 *       remainder, and a greatest common divisor. A constant that is no integer quotient, such as h
 *       over 2 pi, comes from arbitrary precision outside this type. It comes in as finished decimal
 *       text.
 * @note Multiplication climbs a ladder: long multiplication, then Karatsuba from
 *       ORIOR_EXACT_KARATSUBA_LIMBS, then the Schonhage-Strassen transform from
 *       ORIOR_EXACT_TRANSFORM_LIMBS. Division is Knuth's long division, then Newton's reciprocal on
 *       that ladder from ORIOR_EXACT_NEWTON_LIMBS. The script
 *       utils/test/includes/exact_types/integers/exact_transform_test.sh measures each rung. Each
 *       rung gives the product or quotient that the rung below it gives.
 * @note A width-sized work copy sits on the stack up to ORIOR_EXACT_STACK_LIMBS. Past it, the copy
 *       comes from the heap. No stack bounds a width. Where the heap cannot allocate a
 *       copy, the call errors with ORIOR_EXACT_WILL_NOT_FIT. A caller that stores many integers at a
 *       wide width stores them in static or allocated storage. A main thread stack defaults to 1 MiB
 *       under the MSVC linker and 8 MiB under a stock Linux.
 */
#ifndef ORIOR_EXACT_LIMBS_H
#define ORIOR_EXACT_LIMBS_H

#include "exact_integer_api.h"

#endif

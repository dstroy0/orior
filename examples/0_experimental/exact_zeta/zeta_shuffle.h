// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file zeta_shuffle.h
 * @brief The drawn orders of the null: the Mersenne Twister MT19937, seeded and drawn as CPython seeds and draws it.
 *
 * @note The null of Gram's law is the same signs in drawn orders. The orders must be the ones that the published
 *       runs drew. Thus this follows CPython exactly. The seed enters by
 *       init_by_array over its 32-bit words. getrandbits takes the top bits of one word. A draw below n is by
 *       rejection. The shuffle is that of Fisher and Yates, from the top index down.
 * @note The generator is that of Matsumoto and Nishimura (1998).
 */
#ifndef ZETA_SHUFFLE_H
#define ZETA_SHUFFLE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief The words of the state of MT19937. */
#define ZETA_TWISTER_WORDS 624

    /** @brief The state of one generator. */
    typedef struct
    {
        uint32_t state[ZETA_TWISTER_WORDS]; /**< The state words. */
        int index;                          /**< The next word to draw. */
    } ZetaTwister;

    /**
     * @brief Seeds a generator from a seed of at most 32 bits, as CPython seeds it from an int.
     *
     * @param[out] twister The generator [BORROWS].
     * @param[in]  seed    The seed, at least zero.
     */
    void twister_seed(ZetaTwister *twister, uint32_t seed);

    /**
     * @brief A draw below a bound, as CPython draws it: getrandbits of the bit length of the bound, until it is below.
     *
     * @param[in,out] twister The generator [BORROWS].
     * @param[in]     bound   The bound, from 1 to 2^32 - 1.
     * @return                The draw.
     */
    uint32_t twister_below(ZetaTwister *twister, uint32_t bound);

    /**
     * @brief A double in [0, 1), as random.random of CPython draws it from two words.
     *
     * @param[in,out] twister The generator [BORROWS].
     * @return                The draw.
     * @note The draw is (a 2^26 + b) / 2^53, with a the top 27 bits of one word and b the top 26 bits of the next.
     */
    double twister_random(ZetaTwister *twister);

    /**
     * @brief The same symbols in a drawn order, as random.Random(seed).shuffle draws it.
     *
     * @param[in,out] symbols The symbols [BORROWS].
     * @param[in]     count   How many.
     * @param[in]     seed    The seed, at least zero.
     */
    void zeta_permuted(unsigned char *symbols, size_t count, uint32_t seed);

    /**
     * @brief The same symbols in an order that a seeded generator draws, as random.Random.shuffle draws it.
     *
     * @param[in,out] twister The generator, which goes on from its state [BORROWS].
     * @param[in,out] symbols The symbols [BORROWS].
     * @param[in]     count   How many.
     */
    void twister_shuffle(ZetaTwister *twister, unsigned char *symbols, size_t count);

    /**
     * @brief How many positions carry the same symbol as the position one lag above.
     *
     * @param[in] symbols The symbols, position by position [BORROWS].
     * @param[in] count   How many.
     * @param[in] lag     The lag, at least one.
     * @return            The count.
     */
    size_t zeta_agreement(const unsigned char *symbols, size_t count, size_t lag);

#ifdef __cplusplus
}
#endif

#endif

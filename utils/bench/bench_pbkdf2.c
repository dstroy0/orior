// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_pbkdf2.c
 * @brief The rate of one BIP-39 seed derivation on one thread, and the search that the rate reaches.
 *
 * @note A derivation is PBKDF2-HMAC-SHA512 at 2048 iterations: 2048 HMACs, each two SHA-512 compressions after
 *       the key, thus about 4096 compressions. A search over the unknown words of a mnemonic repeats this step once
 *       per candidate that passes the checksum.
 * @note This bench only times. The script bench_pbkdf2.sh runs the vector check of includes/codecs/pbkdf2
 *       first. Where any published vector fails, the script does not run this bench. A rate from a primitive
 *       that fails a vector is a number about nothing.
 * @note THIS MEASURES, AND DOES NOT RECOVER. It derives seeds from a fixed public mnemonic with one letter changed per
 *       trial. It uses no wordlist and no address, and makes no network access.
 */
#include "../../includes/codecs/pbkdf2/pbkdf2.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/** @brief Iterations of PBKDF2 in BIP-39. */
#define BIP39_ITERATIONS 2048u

/** @brief Bytes of a BIP-39 seed. */
#define BIP39_SEED_BYTES 64u

/** @brief Derivations to time. */
#define TRIALS 2000u

int main(void)
{
    // The all-zero-entropy mnemonic of the BIP-39 vectors, with no passphrase.
    static const char base[] = "abandon abandon abandon abandon abandon abandon abandon abandon "
                               "abandon abandon abandon about";
    static const char salt[] = "mnemonic";

    char password[sizeof(base)];
    unsigned char seed[BIP39_SEED_BYTES];
    unsigned char fold = 0u;

    const clock_t began = clock();
    for (unsigned trial = 0u; trial < TRIALS; trial += 1u)
    {
        // A different first letter each trial. Thus nothing can move out of the loop, and the work is identical to a
        // real candidate.
        memcpy(password, base, sizeof(base));
        password[0] = (char)('a' + (trial % 26u));
        if (!pbkdf2_hmac_sha512(password, strlen(password), salt, strlen(salt), BIP39_ITERATIONS, seed, sizeof(seed)))
        {
            printf("  the derivation refused its arguments\n");
            return 1;
        }
        fold ^= seed[trial % sizeof(seed)];
    }
    const clock_t ended = clock();

    const double seconds = (double)(ended - began) / (double)CLOCKS_PER_SEC;
    const double rate = (seconds > 0.0) ? ((double)TRIALS / seconds) : 0.0;
    printf("\n  RATE of the library derivation, one thread. A search repeats this step.\n\n");
    printf("  %u derivations in %.3f s -> %.0f seeds/sec (about %.0f SHA-512 compressions/sec)\n", TRIALS, seconds,
           rate, rate * 4096.0);
    // The fold depends on every seed. Its print keeps the compiler from a removal of the loop.
    printf("  fold %02x\n", fold);

    if (rate <= 0.0)
    {
        return 1;
    }
    // THE WALL, IN THE SAME UNITS. The candidate counts are the checksum-valid candidates per count of unknown words.
    printf("\n  At this one-thread rate, a search covers, by checksum-valid candidate:\n");
    printf("      1 unknown word    ~128        instant\n");
    printf("      2 unknown words   ~2.6e5      %.0f s\n", 2.6e5 / rate);
    printf("      3 unknown words   ~5e8        %.0f s\n", 5.0e8 / rate);
    printf("      4 unknown words   ~1.1e12     %.1f days\n", (1.1e12 / rate) / 86400.0);
    printf("     12 unknown words    2^128      %.1e years (the wall)\n", (3.4e38 / rate) / (86400.0 * 365.0));
    printf("\n  A faster arm moves the first rows. It does not move the last one.\n\n");
    return 0;
}

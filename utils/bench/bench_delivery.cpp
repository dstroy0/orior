// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_delivery.cpp
 * @brief What the human encoded in SHA-256, and why the nonce itself is the whole problem.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note Three questions, all answerable by computation instead of by argument.
 * @note One: a human invented this and humans encode things in what they invent. True, and the
 *       encoding is fully documented and checkable. Every one of the sixty-four round constants and
 *       eight initial values is the fractional part of a root of a small prime. Section 1 derives
 *       all seventy-two from the primes and compares. That is the entire fingerprint of the designer.
 *       The designer chose it that way so that nothing else could hide in it.
 * @note Two: with the nonce, anyone can hash again and derive the solution again. What is the
 *       problem? Nothing at all, and section 2 measures how cheap that is. Verification is one hash.
 * @note The problem is the word "if". The nonce is not a step toward the answer. It is the answer,
 *       and section 2 measures the distance between the two.
 * @note Three: a solution is bound to one header. Section 3 takes the real nonce of a real block.
 *       It changes one byte of the header of that block, as your own payout address in the coinbase
 *       does.
 */

#include "sha256_core.h"

#include <algorithm>
#include "embed_turnstile.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
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

/** @brief The standard's round constants, as published. */
const uint32_t PUBLISHED_ROUND_CONSTANT[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

/** @brief The standard's initial chaining value, as published. */
const uint32_t PUBLISHED_INITIAL[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                       0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};

/**
 * @brief Reports whether a number is prime, by trial division.
 *
 * @param[in] candidate The number to test.
 * @return              True where it is prime.
 */
bool is_prime(unsigned candidate)
{
    if (candidate < 2u)
    {
        return false;
    }
    for (unsigned divisor = 2u; (divisor * divisor) <= candidate; divisor += 1u)
    {
        if ((candidate % divisor) == 0u)
        {
            return false;
        }
    }
    return true;
}

/**
 * @brief Takes the fractional part of a root and scales it to thirty-two bits.
 *
 * @param[in] value The root.
 * @return          The top thirty-two bits of its fractional part.
 * @note Long double carries a sixty-four bit mantissa here, well beyond the thirty-two bits that
 *       the function extracts. Thus the truncation is exact, and not nearly exact.
 */
uint32_t fractional_word(long double value)
{
    const long double fraction = value - floor(value);

    return (uint32_t)(fraction * 4294967296.0L);
}

/**
 * @brief Derives every constant from the primes and compares against the published values.
 */
void derive_the_constants()
{
    printf("\n================================================================\n");
    printf("  1. What the human actually encoded\n");
    printf("================================================================\n");
    printf("\n  A human chose these numbers, so the question is what they chose them to be.\n");
    printf("  The answer is documented and checkable: each round constant is the fractional\n");
    printf("  part of the cube root of a small prime, and each initial value the fractional\n");
    printf("  part of a square root. Derive all seventy-two here and compare.\n");

    unsigned primes[64];
    unsigned found = 0u;
    for (unsigned candidate = 2u; found < 64u; candidate += 1u)
    {
        if (is_prime(candidate))
        {
            primes[found] = candidate;
            found += 1u;
        }
    }

    unsigned constants_matched = 0u;
    for (unsigned index = 0u; index < 64u; index += 1u)
    {
        const uint32_t derived = fractional_word(cbrt((long double)primes[index]));
        if (derived == PUBLISHED_ROUND_CONSTANT[index])
        {
            constants_matched += 1u;
        }
    }

    unsigned initials_matched = 0u;
    for (unsigned index = 0u; index < 8u; index += 1u)
    {
        const uint32_t derived = fractional_word(sqrt((long double)primes[index]));
        if (derived == PUBLISHED_INITIAL[index])
        {
            initials_matched += 1u;
        }
    }

    printf("\n  first eight round constants, derived against published:\n");
    for (unsigned index = 0u; index < 8u; index += 1u)
    {
        printf("    cbrt(%2u) -> 0x%08x   published 0x%08x   %s\n", primes[index],
                    fractional_word(cbrt((long double)primes[index])), PUBLISHED_ROUND_CONSTANT[index],
                    (fractional_word(cbrt((long double)primes[index])) == PUBLISHED_ROUND_CONSTANT[index])
                        ? "match"
                        : "MISMATCH");
    }

    check("all 64 round constants derive from cube roots of the first 64 primes", constants_matched == 64u,
          std::to_string(constants_matched) + " of 64");
    check("all 8 initial values derive from square roots of the first 8 primes", initials_matched == 8u,
          std::to_string(initials_matched) + " of 8");

    printf("\n  So the designer's fingerprint is complete and it is this: the first 64\n");
    printf("  primes, two root functions, and a truncation. Nothing else is in there.\n");
    printf("\n  These are called nothing-up-my-sleeve numbers and the choice is deliberate.\n");
    printf("  A constant chosen freely could hide a trapdoor that only its chooser knows.\n");
    printf("  A constant forced to equal cbrt of the seventeenth prime cannot, because\n");
    printf("  anybody can rederive it and there is no freedom left to hide anything in.\n");
    printf("  The human encoded something, and what they encoded is a proof that they\n");
    printf("  encoded nothing else.\n");
}

/**
 * @brief Measures the gap between checking an answer and finding one.
 */
void measure_verify_against_search()
{
    printf("\n================================================================\n");
    printf("  2. If we know the nonce we can rehash. What is the problem?\n");
    printf("================================================================\n");
    printf("\n  Nothing is the problem, and that half is exactly right. Given the nonce,\n");
    printf("  rederiving the solution is one hash. Here it is, timed.\n");

    // Block 125552, whose nonce the chain recorded.
    const uint8_t header_bytes[80] = {
        0x01, 0x00, 0x00, 0x00, 0x81, 0xcd, 0x02, 0xab, 0x7e, 0x56, 0x9e, 0x8b, 0xcd, 0x93, 0x17, 0xe2,
        0xfe, 0x99, 0xf2, 0xde, 0x44, 0xd4, 0x9a, 0xb2, 0xb8, 0x85, 0x1b, 0xa4, 0xa3, 0x08, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xe3, 0x20, 0xb6, 0xc2, 0xff, 0xfc, 0x8d, 0x75, 0x04, 0x23, 0xdb, 0x8b,
        0x1e, 0xb9, 0x42, 0xae, 0x71, 0x0e, 0x95, 0x1e, 0xd7, 0x97, 0xf7, 0xaf, 0xfc, 0x88, 0x92, 0xb0,
        0xf1, 0xfc, 0x12, 0x2b, 0xc7, 0xf5, 0xd7, 0x4d, 0xf2, 0xb9, 0x44, 0x1a, 0x42, 0xa1, 0x46, 0x95};

    uint8_t digest[32];
    const unsigned repeats = 1000000u;

    const auto started = embed_clock_ns();
    for (unsigned trial = 0u; trial < repeats; trial += 1u)
    {
        sha256_double_hash(header_bytes, sizeof(header_bytes), digest);
    }
    const auto finished = embed_clock_ns();
    const double seconds = ((double)(finished - started) / 1.0e9);
    const double one_hash = seconds / (double)repeats;

    uint32_t target[SHA256_STATE_WORDS];
    sha256_target_from_nbits(target, 0x1a44b9f2u);
    check("the recorded nonce verifies against the recorded target", sha256_digest_within_target(digest, target) != 0);

    printf("\n  verifying one known answer : %.2e seconds\n", one_hash);

    // The search side, at the difficulty the chain currently runs.
    const double expected_hashes = 5.474e23;
    const double device_rate = 1.886e9;

    printf("  finding one, this device   : %.3e hashes at %.3e per second\n", expected_hashes, device_rate);
    printf("                             : %.2e seconds, or %.2e years\n", expected_hashes / device_rate,
                expected_hashes / device_rate / 31557600.0);
    printf("\n  ratio, search over verify  : %.2e\n", (expected_hashes / device_rate) / one_hash);

    printf("\n  That ratio is not an obstacle to the design. It is the design. A proof of\n");
    printf("  work is exactly a problem where checking is trivial and finding is not, and\n");
    printf("  the whole security of the chain is that number being large.\n");
    printf("\n  So the sentence 'if we know the nonce' is not a step toward the answer. It\n");
    printf("  is the answer, already held. Everything the miner does is the work of getting\n");
    printf("  to the point where that sentence is true.\n");
}

/**
 * @brief Shows that a solution belongs to one header, and cannot move to another.
 */
void measure_transferability()
{
    printf("\n================================================================\n");
    printf("  3. Can a known solution be carried to our header?\n");
    printf("================================================================\n");
    printf("\n  Every nonce that ever solved a block is public. If one could be reused with\n");
    printf("  our payout address, mining would be a lookup. Take block 125552's real nonce\n");
    printf("  and change one byte of the header, which is the least a different coinbase\n");
    printf("  does.\n");

    uint8_t header_bytes[80] = {0x01, 0x00, 0x00, 0x00, 0x81, 0xcd, 0x02, 0xab, 0x7e, 0x56, 0x9e, 0x8b, 0xcd, 0x93,
                                0x17, 0xe2, 0xfe, 0x99, 0xf2, 0xde, 0x44, 0xd4, 0x9a, 0xb2, 0xb8, 0x85, 0x1b, 0xa4,
                                0xa3, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe3, 0x20, 0xb6, 0xc2, 0xff, 0xfc,
                                0x8d, 0x75, 0x04, 0x23, 0xdb, 0x8b, 0x1e, 0xb9, 0x42, 0xae, 0x71, 0x0e, 0x95, 0x1e,
                                0xd7, 0x97, 0xf7, 0xaf, 0xfc, 0x88, 0x92, 0xb0, 0xf1, 0xfc, 0x12, 0x2b, 0xc7, 0xf5,
                                0xd7, 0x4d, 0xf2, 0xb9, 0x44, 0x1a, 0x42, 0xa1, 0x46, 0x95};

    uint32_t target[SHA256_STATE_WORDS];
    sha256_target_from_nbits(target, 0x1a44b9f2u);

    uint8_t digest[32];
    sha256_double_hash(header_bytes, sizeof(header_bytes), digest);
    const bool original_wins = (sha256_digest_within_target(digest, target) != 0);

    // Flip the lowest bit of the merkle root. A different coinbase changes the root.
    header_bytes[36] ^= 0x01u;
    sha256_double_hash(header_bytes, sizeof(header_bytes), digest);
    const bool altered_wins = (sha256_digest_within_target(digest, target) != 0);

    unsigned leading = 0u;
    for (int byte = 31; byte >= 0; byte -= 1)
    {
        if (digest[byte] != 0u)
        {
            break;
        }
        leading += 8u;
    }

    check("the original header with its own nonce meets the target", original_wins);
    check("one flipped merkle bit destroys the solution", !altered_wins);

    printf("\n  leading zero bits after the flip: %u, where the target needs about 60\n", leading);
    printf("\n  The merkle root commits to the coinbase transaction, and the coinbase\n");
    printf("  carries the payout address. Putting your address in it changes the merkle\n");
    printf("  root, which changes the header, which invalidates every nonce anyone has ever\n");
    printf("  found. That is not incidental: it is the mechanism that binds the work to the\n");
    printf("  payee, and without it the work would be transferable and worth nothing.\n");
}

} // namespace

int main()
{
    printf("================================================================\n");
    printf("  The human fingerprint, and what delivery actually requires\n");
    printf("================================================================\n");

    derive_the_constants();
    measure_verify_against_search();
    measure_transferability();

    printf("\n================================================================\n");
    printf("  %d checks run, %d failed\n", g_checks_run, g_checks_failed);
    printf("================================================================\n");
    return (g_checks_failed == 0) ? 0 : 1;
}

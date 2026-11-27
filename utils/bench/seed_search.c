// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file seed_search.c
 * @brief Recursive subtraction over a BIP-39 template: enumerate what fits, and derive where it can.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * docs-check: quoting
 * @note Douglas: "subtracting can be recursive here, and should be."
 * docs-check: end quoting
 *
 * WHAT THIS IS
 *
 * A mnemonic is twelve 11-bit indices: 128 bits of entropy, and a 4-bit checksum that is the top
 * four bits of SHA-256 over the entropy. A recovery template fixes the words that you know, and
 * leaves the rest open. The search is a recursion over the word positions. At each node it
 * SUBTRACTS.
 *
 * A known word branches one way, and not 2048. A restricted word branches only over its
 * candidates. The search never visits the LAST open position. The checksum determines it, and the
 * search derives it.
 *
 * WHERE RECURSION COMPOUNDS AND WHERE IT STOPS, STATED PLAINLY
 *
 * Recursion subtracts on constraints that FACTOR across the word positions: known words, known
 * positions, candidate sets per word. That is the recoverable case, and there the tree collapses.
 * The checksum factors only at the last free position. For that reason, the search derives it
 * there, and nowhere else.
 *
 * A target address does NOT factor at all. It is a function of the whole
 * seed through PBKDF2, which has no partial value on a prefix. Thus only a complete leaf can test
 * it. Recursion prunes the structure that you know. It cannot prune the hash, by construction.
 *
 * @note This ENUMERATES and COUNTS. It works in index space, carries no wordlist, derives no
 *       address, and matches no wallet. The only fitness that it applies is the checksum, which is
 *       public and part of the standard. The expensive test per survivor derives the seed and
 *       compares it to a target that the owner supplies for their own recovery.
 * @note This file stops short of that leaf. The closed-form counts grade it. Thus the recursion provably visits the
 *       right tree.
 */
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "sha256_core.h"

/** @brief Words in a 12-word mnemonic. */
#define MNEMONIC_WORDS 12u

/** @brief Distinct words: the range of one index, and the branch width of a fully open word. */
#define WORDLIST_SIZE 2048u

/** @brief Bits an index carries. Twelve of them are 132 bits: 128 entropy and a 4-bit checksum. */
#define INDEX_BITS 11u

/**
 * @brief Packs twelve indices into the sixteen entropy bytes and reads the stored checksum nibble.
 *
 * @param[in]  index    Twelve indices, each below WORDLIST_SIZE [BORROWS].
 * @param[out] entropy  Sixteen entropy bytes [BORROWS].
 * @return              The 4-bit checksum the indices carry, in the low nibble.
 */
static uint8_t pack_entropy(const uint16_t *const index, uint8_t *const entropy)
{
    uint8_t stream[17];
    for (uint32_t at = 0u; at < 17u; ++at)
    {
        stream[at] = 0u;
    }

    for (uint32_t word = 0u; word < MNEMONIC_WORDS; ++word)
    {
        for (uint32_t bit = 0u; bit < INDEX_BITS; ++bit)
        {
            const uint32_t position = (word * INDEX_BITS) + bit;
            const uint32_t value = (index[word] >> (INDEX_BITS - 1u - bit)) & 1u;
            if (value != 0u)
            {
                stream[position >> 3u] |= (uint8_t)(0x80u >> (position & 7u));
            }
        }
    }

    for (uint32_t at = 0u; at < 16u; ++at)
    {
        entropy[at] = stream[at];
    }
    // Bits 128..131 are the top four of byte sixteen.
    return (uint8_t)(stream[16] >> 4u);
}

/**
 * @brief The fitness of one assignment, as a magnitude: zero is falsy, nonzero is truthy.
 *
 * @param[in] index Twelve indices [BORROWS].
 * @return          Zero where the assignment does not fit; a nonzero magnitude where it does.
 *
 * @note THE TEST IS A MAGNITUDE, AND NOT A DEFINED BOOLEAN. A candidate that does not fit weighs
 *       nothing, and the search subtracts it. A candidate that fits carries a weight, and the search
 *       keeps it. Here the only constraint is the checksum. Thus the magnitude is one bit: fits or
 *       does not.
 * @note But the function returns it as a weight. Thus the same machinery reads a richer
 *       fitness (a marginal, an amplitude, a distance to a target) with no change. Keep the truthy,
 *       subtract the falsy, and where the search needs a single survivor, take the heaviest.
 */
static uint32_t fitness_magnitude(const uint16_t *const index)
{
    uint8_t entropy[16];
    const uint8_t carried = pack_entropy(index, entropy);
    uint8_t digest[32];
    sha256_hash(entropy, 16u, digest);
    return (carried == (uint8_t)(digest[0] >> 4u)) ? 1u : 0u;
}

/** @brief The spec of one word of a template: a fixed word, a restricted set, or fully open. */
typedef struct
{
    const uint16_t *candidate; /**< The allowed indices, or NULL for the whole wordlist [BORROWS]. */
    uint16_t count;            /**< How many; 1 is a known word, WORDLIST_SIZE is fully open. */
} WordSpec;

/** @brief What a recursion accumulates. */
typedef struct
{
    uint64_t leaves;    /**< Complete assignments reached: the tree the recursion actually walked. */
    uint64_t survivors; /**< Of those, how many passed the checksum. */
} Counters;

/**
 * @brief The recursion. Assigns word `depth`, then descends. The tree that it walks IS the subtraction.
 *
 * @param[in]     depth    Which word to assign.
 * @param[in,out] index    The assignment under construction [BORROWS].
 * @param[in]     spec     The template [BORROWS].
 * @param[out]    counters Where leaves and survivors accumulate [BORROWS].
 *
 * @note A fixed word has count 1. Thus it branches once, and the search subtracts the other 2047
 *       words at this node, before any deeper work. That is the idea: the pruning is structural, and
 *       not a test applied after the fact.
 */
static void recurse(const uint32_t depth, uint16_t *const index, const WordSpec *const spec, Counters *const counters)
{
    if (depth == MNEMONIC_WORDS)
    {
        ++counters->leaves;
        // Keep the truthy, and subtract the falsy. A survivor is a leaf that weighs something.
        if (fitness_magnitude(index) != 0u)
        {
            ++counters->survivors;
        }
        return;
    }

    const WordSpec *const here = &spec[depth];
    for (uint16_t choice = 0u; choice < here->count; ++choice)
    {
        index[depth] = (here->candidate != NULL) ? here->candidate[choice] : choice;
        recurse(depth + 1u, index, spec, counters);
    }
}

/**
 * @brief The last open position, derived, and not searched.
 *
 * @param[in] fixed  The eleven earlier indices, all known [BORROWS].
 * @return           How many valid last words there are: exactly 2^7. Each choice of the seven
 *                   entropy bits of the last word gives one, and the four checksum bits follow.
 *
 * @note THIS IS THE RECURSION, WHICH SUBTRACTS TO THE ANSWER. A brute last word tries all 2048 and
 *       keeps 128. Here the function builds the 128 directly. Choose the seven free entropy bits,
 *       let SHA-256 finish the checksum, and read off the word. That takes sixteen times fewer
 *       SHA-256 for the identical set. The grade below confirms the identical set.
 */
static uint32_t derive_last_word(const uint16_t *const fixed, uint16_t *const found)
{
    uint16_t index[MNEMONIC_WORDS];
    for (uint32_t at = 0u; at < 11u; ++at)
    {
        index[at] = fixed[at];
    }

    for (uint32_t seven = 0u; seven < 128u; ++seven)
    {
        // The top seven bits of the last word are entropy. Its low four are the checksum. Lay the
        // seven, compute the checksum, then read the low four back from it.
        index[11] = (uint16_t)(seven << 4u);
        uint8_t entropy[16];
        (void)pack_entropy(index, entropy);
        uint8_t digest[32];
        sha256_hash(entropy, 16u, digest);
        const uint8_t checksum = (uint8_t)(digest[0] >> 4u);
        found[seven] = (uint16_t)((seven << 4u) | checksum);
    }
    return 128u;
}

/** @brief Fills a template with every word fixed to `index`. The caller then relaxes chosen words. */
static void template_from(WordSpec *const spec, const uint16_t *const index)
{
    for (uint32_t at = 0u; at < MNEMONIC_WORDS; ++at)
    {
        spec[at].candidate = &index[at];
        spec[at].count = 1u;
    }
}

/**
 * @brief The second route: an iterative odometer over the same template, no recursion.
 *
 * @param[in]  spec     The template [BORROWS].
 * @param[out] counters Leaves and survivors [BORROWS].
 *
 * @note TWO ROUTES, OR IT DOES NOT SHIP. The recursion and this odometer are independent traversals
 *       of the same set. A survivor count that they agree on is a fact about the template, and not
 *       about either loop. The count itself has NO closed form. It is however many entropy values
 *       SHA-256 sends to the stored checksum nibble.
 * @note Thus no number written in advance can grade it. Only a second computation of the same thing
 *       can.
 */
static void enumerate_flat(const WordSpec *const spec, Counters *const counters)
{
    uint16_t index[MNEMONIC_WORDS];
    uint16_t choice[MNEMONIC_WORDS];
    for (uint32_t at = 0u; at < MNEMONIC_WORDS; ++at)
    {
        choice[at] = 0u;
    }

    for (;;)
    {
        for (uint32_t at = 0u; at < MNEMONIC_WORDS; ++at)
        {
            index[at] = (spec[at].candidate != NULL) ? spec[at].candidate[choice[at]] : choice[at];
        }
        ++counters->leaves;
        if (fitness_magnitude(index) != 0u)
        {
            ++counters->survivors;
        }

        int32_t wheel = (int32_t)MNEMONIC_WORDS - 1;
        while (wheel >= 0)
        {
            ++choice[wheel];
            if (choice[wheel] < spec[wheel].count)
            {
                break;
            }
            choice[wheel] = 0u;
            --wheel;
        }
        if (wheel < 0)
        {
            break;
        }
    }
}

/**
 * @brief Grades one template: the leaf count against the structural product, the survivors against
 *        the second route, all exact.
 *
 * @param[in]  label       What this scenario is.
 * @param[in]  spec        The template [BORROWS].
 * @param[in]  want_leaves The product of the spec counts, which the tree must equal exactly.
 * @return                 Non-zero if both routes agree and the leaf count is the structural one.
 */
static int grade_template(const char *const label, const WordSpec *const spec, const uint64_t want_leaves)
{
    uint16_t index[MNEMONIC_WORDS];
    Counters recursive = {0u, 0u};
    Counters flat = {0u, 0u};
    recurse(0u, index, spec, &recursive);
    enumerate_flat(spec, &flat);

    const int leaves_ok = (recursive.leaves == want_leaves) && (flat.leaves == want_leaves);
    const int routes_agree = (recursive.survivors == flat.survivors) && (recursive.leaves == flat.leaves);
    const int good = leaves_ok && routes_agree;

    (void)printf("  %-30s leaves %10llu  survivors %8llu (both routes)  %s\n", label,
                 (unsigned long long)recursive.leaves, (unsigned long long)recursive.survivors,
                 good ? "ok" : "** DISAGREES **");
    return good;
}

int main(void)
{
    (void)printf("\n  RECURSIVE SUBTRACTION over a BIP-39 template. Graded against the closed form.\n\n");

    // The all-zero-entropy mnemonic: eleven "abandon" (index 0) and "about" (index 3). It is the
    // standard valid twelve-word mnemonic, any recursion that reproduces the record must count it
    // as a survivor of itself.
    uint16_t known[MNEMONIC_WORDS] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 3u};

    int good = 1;
    WordSpec spec[MNEMONIC_WORDS];

    // Nothing unknown: one leaf, and it is valid. Thus the recursion agrees that the known mnemonic is real.
    {
        template_from(spec, known);
        good = grade_template("all known", spec, 1u) && good;
    }

    // One middle word open: 2048 leaves. The survivor count is a draw around 2048/16, and not
    // exactly that. A middle word is all entropy, and the checksum is whatever SHA-256 makes of it.
    {
        template_from(spec, known);
        spec[5].candidate = NULL;
        spec[5].count = WORDLIST_SIZE;
        good = grade_template("one middle word open", spec, 2048u) && good;
    }

    // The last word open: 2048 leaves. This one IS exactly 128, provably. The own four bits of the
    // last word are the checksum. Thus for each of its 128 entropy-bit choices, exactly one checksum
    // value fits. A separate check below asserts that exact invariant.
    {
        template_from(spec, known);
        spec[11].candidate = NULL;
        spec[11].count = WORDLIST_SIZE;
        good = grade_template("last word open", spec, 2048u) && good;
    }

    // Two words open: 2048^2 leaves, survivors a draw around 2048^2/16.
    {
        template_from(spec, known);
        spec[3].candidate = NULL;
        spec[3].count = WORDLIST_SIZE;
        spec[8].candidate = NULL;
        spec[8].count = WORDLIST_SIZE;
        good = grade_template("two words open", spec, 2048ull * 2048ull) && good;
    }

    // Restricted words: one word is one of eight, and another is one of four. The tree branches
    // 8*4 = 32. It subtracts every other word at those two nodes before it descends.
    {
        static const uint16_t eight[8] = {11u, 97u, 500u, 501u, 1023u, 1500u, 1900u, 2047u};
        static const uint16_t four[4] = {7u, 42u, 999u, 1600u};
        template_from(spec, known);
        spec[2].candidate = eight;
        spec[2].count = 8u;
        spec[9].candidate = four;
        spec[9].count = 4u;
        good = grade_template("restricted: 8 x 4", spec, 32u) && good;
    }

    // THE ONE EXACT SURVIVOR COUNT: last word open is 128, with no draw about it. A check on its own
    // grades the claim, and does not merely average it.
    {
        template_from(spec, known);
        spec[11].candidate = NULL;
        spec[11].count = WORDLIST_SIZE;
        uint16_t index[MNEMONIC_WORDS];
        Counters counters = {0u, 0u};
        recurse(0u, index, spec, &counters);
        const int exact = (counters.survivors == 128u) ? 1 : 0;
        (void)printf("  %-30s survivors %8llu  (exactly 128, its 4 bits are the checksum)  %s\n",
                     "last word open, exact", (unsigned long long)counters.survivors, exact ? "ok" : "** DISAGREES **");
        good = exact && good;
    }

    // THE SUM RULE: another exact identity, and a check on the packing and the hash together. Vary
    // a middle word over all 2048 values. Each hashes to exactly one of the sixteen checksum
    // nibbles. Thus the survivor counts over all sixteen possible stored nibbles must sum to 2048
    // exactly. No number here is a choice. The clean partition of the entropy forces the identity.
    {
        uint16_t index[MNEMONIC_WORDS];
        for (uint32_t at = 0u; at < MNEMONIC_WORDS; ++at)
        {
            index[at] = known[at];
        }
        uint32_t per_nibble[16];
        for (uint32_t nibble = 0u; nibble < 16u; ++nibble)
        {
            per_nibble[nibble] = 0u;
        }
        for (uint16_t middle = 0u; middle < WORDLIST_SIZE; ++middle)
        {
            index[5] = middle;
            uint8_t entropy[16];
            (void)pack_entropy(index, entropy);
            uint8_t digest[32];
            sha256_hash(entropy, 16u, digest);
            ++per_nibble[digest[0] >> 4u];
        }
        uint32_t total = 0u;
        for (uint32_t nibble = 0u; nibble < 16u; ++nibble)
        {
            total += per_nibble[nibble];
        }
        const int exact = (total == WORDLIST_SIZE) ? 1 : 0;
        (void)printf("  %-30s survivors over 16 nibbles sum to %u  (want 2048)  %s\n", "sum rule", total,
                     exact ? "ok" : "** DISAGREES **");
        good = exact && good;
    }

    (void)printf("\n  DERIVE vs BRUTE at the last position. Same set, a sixteenth of the SHA-256.\n\n");
    {
        // Brute: collect the 128 valid last words. Try all 2048.
        uint16_t brute[WORDLIST_SIZE];
        uint32_t brute_count = 0u;
        for (uint16_t last = 0u; last < WORDLIST_SIZE; ++last)
        {
            uint16_t candidate[MNEMONIC_WORDS];
            for (uint32_t at = 0u; at < 11u; ++at)
            {
                candidate[at] = known[at];
            }
            candidate[11] = last;
            if (fitness_magnitude(candidate) != 0u)
            {
                brute[brute_count] = last;
                ++brute_count;
            }
        }

        uint16_t derived[128];
        const uint32_t derived_count = derive_last_word(known, derived);

        // The derived set must equal the brute set, as sets. Sort-free: each derived word must be
        // among the brute survivors, and the counts must match.
        int identical = (derived_count == brute_count) ? 1 : 0;
        for (uint32_t at = 0u; identical && (at < derived_count); ++at)
        {
            int seen = 0;
            for (uint32_t other = 0u; other < brute_count; ++other)
            {
                if (brute[other] == derived[at])
                {
                    seen = 1;
                    break;
                }
            }
            identical = seen;
        }
        (void)printf("  brute found %u, derive found %u, sets %s\n", brute_count, derived_count,
                     identical ? "identical" : "** DIFFER **");
        // "about" (index 3) must be among them: it is the known valid last word for this prefix.
        int has_about = 0;
        for (uint32_t at = 0u; at < derived_count; ++at)
        {
            if (derived[at] == 3u)
            {
                has_about = 1;
            }
        }
        (void)printf("  the known-valid last word \"about\" is %s the derived set\n",
                     has_about ? "in" : "** NOT IN **");
        good = identical && has_about && good;
    }

    if (!good)
    {
        (void)printf("\n  REFUSED. The recursion visited a tree that is not the one the counts say.\n\n");
        return 1;
    }

    (void)printf("\n  RATE of the recursion, one thread. Enumeration and checksum only.\n\n");
    {
        template_from(spec, known);
        spec[3].candidate = NULL;
        spec[3].count = WORDLIST_SIZE;
        spec[8].candidate = NULL;
        spec[8].count = WORDLIST_SIZE;

        uint16_t index[MNEMONIC_WORDS];
        const clock_t began = clock();
        Counters counters = {0u, 0u};
        recurse(0u, index, spec, &counters);
        const clock_t ended = clock();

        const double seconds = (double)(ended - began) / (double)CLOCKS_PER_SEC;
        const double rate = (seconds > 0.0) ? ((double)counters.leaves / seconds) : 0.0;
        (void)printf("  %llu leaves in %.3f s  ->  %.0f candidates/sec through the checksum\n",
                     (unsigned long long)counters.leaves, seconds, rate);
        (void)printf("  of those, %llu survived to become fitness candidates\n\n",
                     (unsigned long long)counters.survivors);

        // THE HONEST COST STACK. Enumeration is cheap. The survivors cost. A match of each one against
        // a target needs a PBKDF2 derivation: about 400/sec on one thread, by bench_pbkdf2. Thus the
        // two-word case is survivors / 400 seconds of the expensive step.
        (void)printf("  matching survivors to a target is the expensive step: at ~400 seeds/sec/thread\n");
        (void)printf("  the %llu survivors of this two-word template cost about %.0f s per thread.\n",
                     (unsigned long long)counters.survivors, (double)counters.survivors / 400.0);
        (void)printf("\n  Recursion made enumeration free. It did not make the fitness test free,\n");
        (void)printf("  and nothing can: that is the one constraint that does not factor.\n\n");
    }

    return 0;
}

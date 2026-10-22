// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "../../src/cu/includes/types/integers/arm.h"
#if defined(ORIOR_EXACT_HAVE_CUDA) && ORIOR_EXACT_HAVE_CUDA
#include "../../src/cu/includes/types/integers/arm_cuda.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define RUN_PLACES 65536u

#define RUN_DEFAULT 4096u

#define RUN_CYCLE 4u

#define PLACES_HELD ((((unsigned long long)ORIOR_EXACT_BITS - 15ull) * 30102ull) / 100000ull)

#define PLACES ((PLACES_HELD < 24ull) ? (uint32_t)PLACES_HELD : 24u)

#if defined(__cplusplus)
static_assert(PLACES_HELD >= 2ull, "the narrowest width must hold the two places a quarter step takes");
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(PLACES_HELD >= 2ull, "the narrowest width must hold the two places a quarter step takes");
#else
typedef char bench_exact_arms_places_hold_a_quarter[(PLACES_HELD >= 2ull) ? 1 : -1];
#endif

#define LAGS 24u

#define REPEATS 3u

static int plant(OriorExactInteger *positions, uint64_t *values, unsigned int places)
{
    for (unsigned int at = 0u; at < places; at++)
    {
        char text[64];
        (void)snprintf(text, sizeof(text), "%u.%02u", at / 4u, (at % 4u) * 25u);
        if (orior_exact_from_decimal(text, strlen(text), PLACES, &positions[at]) != ORIOR_EXACT_OK)
        {
            return 0;
        }
        values[at] = (uint64_t)(at % RUN_CYCLE);
    }
    return 1;
}

static double sweep(const OriorExactArm *arm, const OriorExactInteger *positions,
                    const uint64_t *values, const OriorExactInteger *lags, size_t *answers,
                    unsigned int places)
{
    const clock_t started = clock();
    for (unsigned int again = 0u; again < REPEATS; again++)
    {
        for (unsigned int at = 0u; at < LAGS; at++)
        {
            answers[at] = arm->agreement(positions, values, (size_t)places, &lags[at]);
        }
    }
    return (double)(clock() - started) / (double)CLOCKS_PER_SEC;
}

static int grade_arms(const OriorExactArm *const *arms, unsigned int count,
                      const OriorExactInteger *positions, const uint64_t *values,
                      const OriorExactInteger *lags, const size_t *reference, size_t *held,
                      unsigned int places, double portable_seconds, const char *portable_name)
{
    int wrong = 0;
    for (unsigned int which = 0u; which < count; which++)
    {
        const OriorExactArm *arm = arms[which];
        const double seconds = sweep(arm, positions, values, lags, held, places);

        unsigned int differs = 0u;
        for (unsigned int at = 0u; at < LAGS; at++)
        {
            if (held[at] != reference[at])
            {
                differs++;
            }
        }
        if (differs != 0u)
        {
            wrong = 1;
        }

        char verdict[96];
        if (differs != 0u)
        {
            (void)snprintf(verdict, sizeof(verdict), "DISAGREES on %u of %u lags", differs, LAGS);
        }
        else if (seconds > 0.0)
        {
            (void)snprintf(verdict, sizeof(verdict), "agrees, %.2fx", portable_seconds / seconds);
        }
        else
        {
            (void)snprintf(verdict, sizeof(verdict), "agrees, too fast to time");
        }
        printf("  %-14s %-10.3f %-12s %s\n", arm->name, seconds, portable_name, verdict);
    }
    return wrong;
}

int main(int argc, char **argv)
{
    static uint64_t values[RUN_PLACES];
    static OriorExactInteger lags[LAGS];
    static size_t reference[LAGS];
    static size_t held[LAGS];

    unsigned int places = RUN_DEFAULT;
    if (argc > 1)
    {
        const long asked = strtol(argv[1], NULL, 10);
        if ((asked < 16) || (asked > (long)RUN_PLACES))
        {
            printf("  positions must be between 16 and %u\n", RUN_PLACES);
            return 1;
        }
        places = (unsigned int)asked;
    }

    OriorExactInteger *const positions =
        (OriorExactInteger *)malloc((size_t)places * sizeof(OriorExactInteger));
    if (positions == NULL)
    {
        printf("  %u positions of %u limbs could not be allocated\n", places,
               (unsigned int)ORIOR_EXACT_LIMBS);
        return 1;
    }

    if (plant(positions, values, places) == 0)
    {
        printf("  the run would not fit the scale\n");
        free(positions);
        return 1;
    }
    for (unsigned int at = 0u; at < LAGS; at++)
    {
        char text[64];
        (void)snprintf(text, sizeof(text), "%u.%02u", (at + 1u) / 4u, ((at + 1u) % 4u) * 25u);
        if (orior_exact_from_decimal(text, strlen(text), PLACES, &lags[at]) != ORIOR_EXACT_OK)
        {
            printf("  lag %u would not fit the scale\n", at);
            free(positions);
            return 1;
        }
    }

    printf("\n  %u positions, %u lags, %u limbs of 32 bits, %u decimal places\n\n", places,
           LAGS, (unsigned int)ORIOR_EXACT_LIMBS, (unsigned int)PLACES);

    const OriorExactArm *portable = orior_exact_portable_arm();
    const double portable_seconds = sweep(portable, positions, values, lags, reference, places);
    printf("  %-14s %-10s %-12s %s\n", "arm", "seconds", "against", "verdict");
    printf("  %-14s %-10.3f %-12s %s\n", portable->name, portable_seconds, "itself",
           "the reference");

    const OriorExactArm *arms[6];
    unsigned int count = 0u;
#if defined(ORIOR_EXACT_HAVE_AVX2) && ORIOR_EXACT_HAVE_AVX2
    arms[count] = orior_exact_avx2_arm();
    if (arms[count] != NULL)
    {
        count++;
    }
#endif
#if defined(ORIOR_EXACT_HAVE_AVX512) && ORIOR_EXACT_HAVE_AVX512
    arms[count] = orior_exact_avx512_arm();
    if (arms[count] != NULL)
    {
        count++;
    }
#endif
#if defined(ORIOR_EXACT_HAVE_SVE) && ORIOR_EXACT_HAVE_SVE
    arms[count] = orior_exact_sve_arm();
    if (arms[count] != NULL)
    {
        count++;
    }
#endif
#if defined(ORIOR_EXACT_HAVE_NEON) && ORIOR_EXACT_HAVE_NEON
    arms[count] = orior_exact_neon_arm();
    if (arms[count] != NULL)
    {
        count++;
    }
#endif
#if defined(ORIOR_EXACT_HAVE_CUDA) && ORIOR_EXACT_HAVE_CUDA
    arms[count] = orior_exact_cuda_arm();
    if (arms[count] != NULL)
    {
        count++;
    }
#endif

    int wrong = grade_arms(arms, count, positions, values, lags, reference, held, places,
                           portable_seconds, portable->name);

    if (count == 0u)
    {
        printf("\n  no arm beyond portable was built into this binary or answered on this machine\n");
    }

    const size_t expected_at_period = (size_t)(places - 4u);
    printf("\n  planted period is lag 4. portable read %llu, arithmetic says %llu: %s\n",
           (unsigned long long)reference[3], (unsigned long long)expected_at_period,
           (reference[3] == expected_at_period) ? "agree" : "DISAGREE");
    if (reference[3] != expected_at_period)
    {
        wrong = 1;
    }

    for (unsigned int at = 7u; at < places; at += 8u)
    {
        positions[at] = positions[at - 1u];
    }
    printf("\n  the same run with the position before every eighth entry repeated\n\n");
    const double repeated_seconds = sweep(portable, positions, values, lags, reference, places);
    printf("  %-14s %-10.3f %-12s %s\n", portable->name, repeated_seconds, "itself",
           "the reference");
    if (grade_arms(arms, count, positions, values, lags, reference, held, places, repeated_seconds,
                   portable->name) != 0)
    {
        wrong = 1;
    }

#if defined(ORIOR_EXACT_HAVE_CUDA) && ORIOR_EXACT_HAVE_CUDA
    if (orior_exact_cuda_available() != 0)
    {
        const size_t device_answer =
            orior_exact_agreement_cuda(positions, values, (size_t)places, &lags[3]);
        if (device_answer == (size_t)-1)
        {
            printf("\n  the device errored on the run at %u limbs. The cuda row is the portable arm\n",
                   (unsigned int)ORIOR_EXACT_LIMBS);
            wrong = 1;
        }
        else
        {
            printf("\n  the device itself answered lag 4: %llu, portable says %llu: %s\n",
                   (unsigned long long)device_answer, (unsigned long long)reference[3],
                   (device_answer == reference[3]) ? "agree" : "DISAGREE");
            if (device_answer != reference[3])
            {
                wrong = 1;
            }
        }
    }
#endif

    printf("\n");
    free(positions);
    return wrong;
}

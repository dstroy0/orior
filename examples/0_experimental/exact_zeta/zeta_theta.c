// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file zeta_theta.c
 * @brief The theta function of Riemann and Siegel, by the series of Stirling. zeta_theta.h states the rules.
 */
#include "zeta_theta.h"
#include "../../../src/platform/platform.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include <stdlib.h>

/**
 * @brief Stops the program where an allocation fails, else returns the block.
 *
 * @param[in] block The block [OWNS].
 * @return          The same block [OWNS].
 */
static void *theta_memory(void *block)
{
    if (block == NULL)
    {
        zeta_stop("out of memory");
    }
    return block;
}

/* ---------------------------------------------------------------- the arctangent of a rational, by two series */

/**
 * @brief A pi route at a scale, kept by scale: the scale is 10^digits, and the digits are its key.
 *
 * @param[in]  digits The digits of the scale.
 * @param[in]  machin 1 for the identity of Machin, 0 for that of Euler.
 * @param[out] result The value [BORROWS].
 */
static void pi_route(int32_t digits, int machin, OriorExactInteger *result)
{
    enum
    {
        KEPT = 4096
    };
    static OriorExactInteger *kept[2][KEPT];
    if ((digits < 0) || (digits >= KEPT))
    {
        zeta_stop("a scale past the kept range");
    }
    if (kept[machin][digits] == NULL)
    {
        kept[machin][digits] = (OriorExactInteger *)theta_memory(
            platform_cell_taken(sizeof(OriorExactInteger), platform_place_g_program_owner, 0));
        OriorExactInteger scale;
        z_ten_to(digits, &scale);
        if (machin)
        {
            zeta_pi_machin(&scale, kept[machin][digits]);
        }
        else
        {
            zeta_pi_euler(&scale, kept[machin][digits]);
        }
    }
    *result = *kept[machin][digits];
}

/**
 * @brief The smaller and the larger of x and y, and the verdict that x > y, for x, y > 0.
 *
 * @param[in]  x       The first [BORROWS].
 * @param[in]  y       The second [BORROWS].
 * @param[out] smaller The smaller [BORROWS].
 * @param[out] larger  The larger [BORROWS].
 * @return             1 where x > y.
 */
static int turned(const OriorExactInteger *x, const OriorExactInteger *y, OriorExactInteger *smaller,
                  OriorExactInteger *larger)
{
    const int verdict = z_cmp(x, y) > 0;
    *smaller = verdict ? *y : *x;
    *larger = verdict ? *x : *y;
    return verdict;
}

/**
 * @brief arctan(x / y) times 10^digits, x, y > 0, by the series of Euler in x^2 / (x^2 + y^2).
 *
 * @param[in]  x      The numerator [BORROWS].
 * @param[in]  y      The denominator [BORROWS].
 * @param[in]  digits The digits of the scale.
 * @param[out] result The value [BORROWS].
 */
static void arctan_euler(const OriorExactInteger *x, const OriorExactInteger *y, int32_t digits,
                         OriorExactInteger *result)
{
    OriorExactInteger a;
    OriorExactInteger c;
    const int verdict = turned(x, y, &a, &c);
    OriorExactInteger scale;
    z_ten_to(digits, &scale);
    OriorExactInteger size;
    OriorExactInteger product;
    z_mul(&a, &a, &size);
    z_mul(&c, &c, &product);
    z_add(&size, &product, &size);
    OriorExactInteger a_square;
    z_mul(&a, &a, &a_square);
    OriorExactInteger term;
    z_mul(&scale, &a, &term);
    z_mul(&term, &c, &term);
    z_floor_div(&term, &size, &term);
    OriorExactInteger total = term;
    for (int64_t step = 1; term.sign != 0; step += 1)
    {
        z_mul_small(&term, 2 * step, &term);
        z_mul(&term, &a_square, &term);
        OriorExactInteger under;
        z_mul_small(&size, (2 * step) + 1, &under);
        z_floor_div(&term, &under, &term);
        z_add(&total, &term, &total);
    }
    OriorExactInteger half_pi;
    pi_route(digits, 0, &half_pi);
    OriorExactInteger two;
    z_set(&two, 2);
    z_floor_div(&half_pi, &two, &half_pi);
    if (verdict)
    {
        z_sub(&half_pi, &total, result);
    }
    else
    {
        *result = total;
    }
}

/**
 * @brief arctan(x / y) times 10^digits, x, y > 0, as arctan(1/2) + arctan((2a - c) / (2c + a)).
 *
 * @param[in]  x      The numerator [BORROWS].
 * @param[in]  y      The denominator [BORROWS].
 * @param[in]  digits The digits of the scale.
 * @param[out] result The value [BORROWS].
 */
static void arctan_taylor(const OriorExactInteger *x, const OriorExactInteger *y, int32_t digits,
                          OriorExactInteger *result)
{
    OriorExactInteger a;
    OriorExactInteger c;
    const int verdict = turned(x, y, &a, &c);
    OriorExactInteger scale;
    z_ten_to(digits, &scale);
    OriorExactInteger top;
    OriorExactInteger bottom;
    z_mul_small(&a, 2, &top);
    z_sub(&top, &c, &top);
    z_mul_small(&c, 2, &bottom);
    z_add(&bottom, &a, &bottom);
    const int sign = top.sign;
    z_abs(&top, &top);
    OriorExactInteger power;
    z_mul(&scale, &top, &power);
    z_floor_div(&power, &bottom, &power);
    OriorExactInteger top_square;
    OriorExactInteger bottom_square;
    z_mul(&top, &top, &top_square);
    z_mul(&bottom, &bottom, &bottom_square);
    OriorExactInteger total;
    orior_exact_zero(&total);
    for (int64_t step = 0; power.sign != 0; step += 1)
    {
        OriorExactInteger odd;
        z_set(&odd, (2 * step) + 1);
        OriorExactInteger term;
        z_floor_div(&power, &odd, &term);
        if ((step % 2) == 0)
        {
            z_add(&total, &term, &total);
        }
        else
        {
            z_sub(&total, &term, &total);
        }
        z_mul(&power, &top_square, &power);
        z_floor_div(&power, &bottom_square, &power);
    }
    OriorExactInteger half;
    zeta_arctan_inverse(2, &scale, &half);
    if (sign < 0)
    {
        z_sub(&half, &total, &half);
    }
    else if (sign > 0)
    {
        z_add(&half, &total, &half);
    }
    OriorExactInteger half_pi;
    pi_route(digits, 1, &half_pi);
    OriorExactInteger two;
    z_set(&two, 2);
    z_floor_div(&half_pi, &two, &half_pi);
    if (verdict)
    {
        z_sub(&half_pi, &half, result);
    }
    else
    {
        *result = half;
    }
}

/** @brief One kept value, by a text key. */
typedef struct
{
    char *key;               /**< The key [OWNS]. */
    OriorExactInteger value; /**< The value. */
} KeptValue;

/** @brief A table of kept values by text key. A full bucket passes a key to the next one. */
typedef struct
{
    KeptValue *entries; /**< The entries [OWNS]. */
    size_t capacity;    /**< How many entries, a power of two. */
    size_t count;       /**< How many hold a value. */
} KeptTable;

/**
 * @brief The hash of a text, by FNV-1a.
 *
 * @param[in] text The text [BORROWS].
 * @return         The hash.
 */
static uint64_t text_hash(const char *text)
{
    uint64_t hash = 1469598103934665603u;
    for (const char *at = text; *at != '\0'; at += 1)
    {
        hash = (hash ^ (uint64_t)(unsigned char)*at) * 1099511628211u;
    }
    return hash;
}

/**
 * @brief The entry of a key, with its key NULL where the key is new.
 *
 * @param[in,out] table The table [BORROWS].
 * @param[in]     key   The key [BORROWS].
 * @return              The entry [BORROWS].
 */
static KeptValue *kept_find(KeptTable *table, const char *key)
{
    if ((table->count * 2u) >= table->capacity)
    {
        const size_t grown = (table->capacity == 0u) ? 256u : (table->capacity * 2u);
        KeptValue *const entries = (KeptValue *)theta_memory(
            platform_cell_cleared(grown, sizeof(KeptValue), platform_place_g_program_owner, 0));
        for (size_t at = 0u; at < table->capacity; at += 1u)
        {
            if (table->entries[at].key != NULL)
            {
                size_t place = (size_t)(text_hash(table->entries[at].key) & (grown - 1u));
                while (entries[place].key != NULL)
                {
                    place = (place + 1u) & (grown - 1u);
                }
                entries[place] = table->entries[at];
            }
        }
        platform_cell_released(table->entries, platform_place_g_program_owner);
        table->entries = entries;
        table->capacity = grown;
    }
    size_t place = (size_t)(text_hash(key) & (table->capacity - 1u));
    while ((table->entries[place].key != NULL) &&
           (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = table->entries[place].key,
                        .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = key, .ci = EMBED_FALSE)))
    {
        place = (place + 1u) & (table->capacity - 1u);
    }
    return &table->entries[place];
}

/**
 * @brief Keeps a value at a new entry.
 *
 * @param[in,out] table The table [BORROWS].
 * @param[in,out] entry The new entry [BORROWS].
 * @param[in]     key   The key [BORROWS].
 * @param[in]     value The value [BORROWS].
 */
static void kept_store(KeptTable *table, KeptValue *entry, const char *key, const OriorExactInteger *value)
{
    entry->key = (char *)theta_memory(platform_cell_taken(
        EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = key, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1u,
        platform_place_g_program_owner, 0));
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = entry->key, .src = key,
               .bytes = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = key, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1u);
    entry->value = *value;
    table->count += 1u;
}

/**
 * @brief arctan(x / y) at the places, by the two series that must agree, kept by its arguments.
 *
 * @param[in]  x      The numerator, positive [BORROWS].
 * @param[in]  y      The denominator, positive [BORROWS].
 * @param[in]  places The places.
 * @param[out] result The value [BORROWS].
 */
static void arctan_agreed(const OriorExactInteger *x, const OriorExactInteger *y, int32_t places,
                          OriorExactInteger *result)
{
    static KeptTable table = {NULL, 0u, 0u};
    static char x_text[8192];
    static char y_text[8192];
    static char key[16448];
    z_text(x, x_text, sizeof(x_text));
    z_text(y, y_text, sizeof(y_text));
    snprintf(key, sizeof(key), "%s/%s/%d", x_text, y_text, (int)places);
    KeptValue *const entry = kept_find(&table, key);
    if (entry->key != NULL)
    {
        *result = entry->value;
        return;
    }
    if (places < 1)
    {
        zeta_stop("places must be at least 1");
    }
    OriorExactInteger one;
    OriorExactInteger two;
    arctan_euler(x, y, places + ZETA_GUARD, &one);
    arctan_taylor(x, y, places + ZETA_GUARD, &two);
    OriorExactInteger drop;
    z_ten_to(ZETA_GUARD, &drop);
    z_floor_div(&one, &drop, &one);
    z_floor_div(&two, &drop, &two);
    if (!orior_exact_equal(&one, &two))
    {
        fprintf(stderr, "  arctan %s/%s: the two routes disagree at %d places\n", x_text, y_text, (int)places);
        exit(2);
    }
    kept_store(&table, entry, key, &one);
    *result = one;
}

/* ---------------------------------------------------------------- theta */

void theta_ln_pi(int32_t digits, OriorExactInteger *result)
{
    OriorExactInteger pi_value;
    zeta_pi(digits, &pi_value);
    OriorExactInteger log;
    zeta_ln_big(&pi_value, digits, &log);
    OriorExactInteger ten;
    zeta_ln(10, digits, &ten);
    z_mul_small(&ten, digits, &ten);
    z_sub(&log, &ten, result);
}

void theta_route(const DecimalPair *t, int32_t digits, int64_t shift, int64_t terms, OriorExactInteger *result)
{
    OriorExactInteger scale;
    z_ten_to(digits, &scale);
    const OriorExactInteger *const big_t = &t->numerator;
    OriorExactInteger unit;
    z_ten_to(t->places, &unit);
    OriorExactInteger a;
    OriorExactInteger b;
    OriorExactInteger d;
    z_mul_small(&unit, 1 + (4 * shift), &a);
    z_mul_small(big_t, 2, &b);
    z_mul_small(&unit, 4, &d);
    OriorExactInteger size;
    OriorExactInteger product;
    z_mul(&a, &a, &size);
    z_mul(&b, &b, &product);
    z_add(&size, &product, &size);

    OriorExactInteger lead;
    arctan_agreed(&b, &a, digits, &lead);
    z_mul_small(&lead, (4 * shift) - 1, &lead);
    OriorExactInteger four;
    z_set(&four, 4);
    z_floor_div(&lead, &four, &lead);
    OriorExactInteger log_size;
    OriorExactInteger log_d;
    zeta_ln_big(&size, digits, &log_size);
    zeta_ln_big(&d, digits, &log_d);
    z_mul_small(&log_d, 2, &log_d);
    z_sub(&log_size, &log_d, &log_size);
    z_mul(big_t, &log_size, &log_size);
    OriorExactInteger under;
    z_mul_small(&unit, 4, &under);
    z_floor_div(&log_size, &under, &log_size);
    z_add(&lead, &log_size, &lead);
    OriorExactInteger linear;
    z_mul(&scale, big_t, &linear);
    z_mul_small(&unit, 2, &under);
    z_floor_div(&linear, &under, &linear);
    z_sub(&lead, &linear, &lead);

    OriorExactInteger args;
    orior_exact_zero(&args);
    for (int64_t k = 0; k < shift; k += 1)
    {
        OriorExactInteger top;
        OriorExactInteger bottom;
        z_mul_small(big_t, 2, &top);
        z_mul_small(&unit, 1 + (4 * k), &bottom);
        OriorExactInteger angle;
        arctan_agreed(&top, &bottom, digits, &angle);
        z_add(&args, &angle, &args);
    }

    OriorExactInteger correction;
    orior_exact_zero(&correction);
    OriorExactInteger re = a;
    OriorExactInteger im;
    z_neg(&b, &im);
    OriorExactInteger square_re;
    OriorExactInteger square_im;
    z_mul(&a, &a, &square_re);
    z_mul(&b, &b, &product);
    z_sub(&square_re, &product, &square_re);
    z_mul(&a, &b, &square_im);
    z_mul_small(&square_im, -2, &square_im);
    OriorExactInteger d_power = d;
    OriorExactInteger size_power = size;
    OriorExactInteger factorial;
    z_set(&factorial, 1);
    OriorExactInteger d_square;
    OriorExactInteger size_square;
    z_mul(&d, &d, &d_square);
    z_mul(&size, &size, &size_square);
    for (int64_t j = 1; j <= terms; j += 1)
    {
        ZetaRational bernoulli;
        zeta_bernoulli(j, &bernoulli);
        OriorExactInteger term;
        z_mul(&scale, &bernoulli.top, &term);
        z_mul(&term, &factorial, &term);
        z_mul(&term, &im, &term);
        z_mul(&term, &d_power, &term);
        OriorExactInteger below;
        z_mul(&bernoulli.under, &size_power, &below);
        z_floor_div(&term, &below, &term);
        z_add(&correction, &term, &correction);
        OriorExactInteger next_re;
        OriorExactInteger next_im;
        z_mul(&re, &square_re, &next_re);
        z_mul(&im, &square_im, &product);
        z_sub(&next_re, &product, &next_re);
        z_mul(&re, &square_im, &next_im);
        z_mul(&im, &square_re, &product);
        z_add(&next_im, &product, &next_im);
        re = next_re;
        im = next_im;
        z_mul(&d_power, &d_square, &d_power);
        z_mul(&size_power, &size_square, &size_power);
        z_mul_small(&factorial, ((2 * j) - 1) * (2 * j), &factorial);
    }

    OriorExactInteger pi_log;
    theta_ln_pi(digits, &pi_log);
    z_mul(big_t, &pi_log, &pi_log);
    z_mul_small(&unit, 2, &under);
    z_floor_div(&pi_log, &under, &pi_log);
    z_add(&lead, &correction, result);
    z_sub(result, &args, result);
    z_sub(result, &pi_log, result);
}

/**
 * @brief A value read toward zero with the guard dropped: its sign times the floor of its size.
 *
 * @param[in,out] value The value [BORROWS].
 */
static void toward_zero(OriorExactInteger *value)
{
    const int32_t sign = value->sign;
    OriorExactInteger size;
    z_abs(value, &size);
    OriorExactInteger drop;
    z_ten_to(ZETA_GUARD, &drop);
    z_floor_div(&size, &drop, value);
    if (sign < 0)
    {
        value->sign = -value->sign;
    }
}

void theta_routes(const DecimalPair *t, int32_t places, int64_t n_sum, int bernoulli, OriorExactInteger *one,
                  OriorExactInteger *two)
{
    const int32_t digits = places + ZETA_GUARD;
    theta_route(t, digits, n_sum, n_sum * bernoulli, one);
    theta_route(t, digits, 2 * n_sum, 2 * n_sum * bernoulli, two);
    toward_zero(one);
    toward_zero(two);
}

void theta_pi_times(int64_t i, const OriorExactInteger *big_pi, OriorExactInteger *result)
{
    z_mul_small(big_pi, (i < 0) ? -i : i, result);
    OriorExactInteger drop;
    z_ten_to(ZETA_GUARD, &drop);
    z_floor_div(result, &drop, result);
    if (i < 0)
    {
        result->sign = -result->sign;
    }
}

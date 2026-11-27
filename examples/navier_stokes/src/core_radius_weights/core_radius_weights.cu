// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// core_radius_weights.cu: the orders, their rule and the carry on the device (core_radius_weights.h)
#include "core_radius_weights.h"

#include "../../../../includes/platform/platform.h"

SimRational core_radius_number(long long numerator, long long denominator)
{
    return sim_rational(numerator, denominator);
}

SimRational core_radius_times(SimRational left, SimRational right)
{
    return sim_rational_product(left, right);
}

SimRational core_radius_plus(SimRational left, SimRational right)
{
    return sim_rational_sum(left, right);
}

SimRational core_radius_over(SimRational left, SimRational right)
{
    return sim_rational_product(left, sim_rational_reciprocal(right));
}

SimRational core_radius_max(SimRational left, SimRational right)
{
    return (sim_rational_sign(sim_rational_difference(left, right)) >= 0) ? left : right;
}

// 1 where the pool could not meet a cell of a sequence, or a write passed a sequence's room
static int s_core_radius_unmet;

int core_radius_unmet(void)
{
    return s_core_radius_unmet;
}

void core_radius_taken(CoreRadiusWeights *weights, unsigned int room, int ephemeral)
{
    weights->count = 0u;
    weights->room = 0u;
    weights->value = NULL;
    if (room == 0u)
    {
        return;
    }
    weights->value = (SimRational *)platform_cell_taken((unsigned long long)room * sizeof(SimRational),
                                                        platform_place_g_program_owner, ephemeral);
    s_core_radius_unmet |= (weights->value == NULL) ? 1 : 0;
    weights->room = (weights->value != NULL) ? room : 0u;
}

void core_radius_released(CoreRadiusWeights *weights)
{
    platform_cell_released(weights->value, platform_place_g_program_owner);
    weights->value = NULL;
    weights->count = 0u;
    weights->room = 0u;
}

void core_radius_kept(const CoreRadiusWeights *from, CoreRadiusWeights *to)
{
    core_radius_taken(to, from->count, 0);
    for (unsigned int m = 0u; m < to->room; m += 1u)
    {
        to->value[m] = from->value[m];
    }
    to->count = to->room;
}

void core_radius_pushed(CoreRadiusWeights *weights, SimRational value)
{
    if (weights->count >= weights->room)
    {
        s_core_radius_unmet = 1;
        return;
    }
    weights->value[weights->count] = value;
    weights->count += 1u;
}

void core_radius_replaced(CoreRadiusWeights *into, CoreRadiusWeights *made)
{
    core_radius_released(into);
    *into = *made;
    made->value = NULL;
    made->count = 0u;
    made->room = 0u;
}

// `add` summed into `into`, and the cell of `add` given back
static void core_radius_sum_into(CoreRadiusWeights *into, CoreRadiusWeights *add)
{
    CoreRadiusWeights made;
    core_radius_sum(into, add, &made);
    core_radius_replaced(into, &made);
    core_radius_released(add);
}

SimRational core_radius_norm(const CoreRadiusWeights *weights, SimRational rho)
{
    SimRational sum = core_radius_number(0ll, 1ll);
    SimRational power = core_radius_number(1ll, 1ll);
    for (unsigned int m = 0u; m < weights->count; m += 1u)
    {
        sum = core_radius_plus(sum, core_radius_times(sim_rational_absolute(weights->value[m]), power));
        power = core_radius_times(power, rho);
    }
    return sum;
}

void core_radius_add(CoreRadiusWeights *into, unsigned int m, SimRational value)
{
    if (m >= into->room)
    {
        s_core_radius_unmet = 1;
        return;
    }
    while (into->count <= m)
    {
        into->value[into->count] = core_radius_number(0ll, 1ll);
        into->count += 1u;
    }
    into->value[m] = core_radius_plus(into->value[m], value);
}

void core_radius_sum(const CoreRadiusWeights *left, const CoreRadiusWeights *right, CoreRadiusWeights *out)
{
    core_radius_taken(out, (left->count > right->count) ? left->count : right->count, 1);
    for (unsigned int m = 0u; (m < left->count) && (m < out->room); m += 1u)
    {
        out->value[m] = left->value[m];
    }
    out->count = (left->count < out->room) ? left->count : out->room;
    for (unsigned int m = 0u; m < right->count; m += 1u)
    {
        core_radius_add(out, m, right->value[m]);
    }
}

void core_radius_scaled(const CoreRadiusWeights *weights, SimRational factor, CoreRadiusWeights *out)
{
    core_radius_taken(out, weights->count, 1);
    for (unsigned int m = 0u; m < out->room; m += 1u)
    {
        out->value[m] = core_radius_times(weights->value[m], factor);
    }
    out->count = out->room;
}

void core_radius_eta(const CoreRadiusWeights *weights, CoreRadiusWeights *out)
{
    const SimRational half = core_radius_number(1ll, 2ll);
    core_radius_taken(out, (weights->count != 0u) ? (weights->count + 1u) : 0u, 1);
    for (unsigned int m = 0u; m < weights->count; m += 1u)
    {
        if (m == 0u)
        {
            core_radius_add(out, 1u, weights->value[0]);
        }
        else
        {
            core_radius_add(out, m + 1u, core_radius_times(half, weights->value[m]));
            core_radius_add(out, m - 1u, core_radius_times(half, weights->value[m]));
        }
    }
}

void core_radius_second(const CoreRadiusWeights *weights, CoreRadiusWeights *out)
{
    const SimRational half = core_radius_number(1ll, 2ll);
    core_radius_taken(out, (weights->count != 0u) ? (weights->count + 2u) : 0u, 1);
    for (unsigned int m = 0u; m < weights->count; m += 1u)
    {
        core_radius_add(out, m + 2u, core_radius_times(half, weights->value[m]));
        core_radius_add(out, (m >= 2u) ? m - 2u : 2u - m, core_radius_times(half, weights->value[m]));
    }
}

SimRational core_radius_half_square(const CoreRadiusScale *scale)
{
    return core_radius_times(core_radius_times(scale->cut, scale->cut), core_radius_number(1ll, 2ll));
}

// `weights` times `constant` and its second times `second_factor`, summed: the shape of L and d
static void core_radius_two_terms(const CoreRadiusWeights *weights, SimRational constant, SimRational second_factor,
                                  CoreRadiusWeights *out)
{
    CoreRadiusWeights plain;
    CoreRadiusWeights second;
    CoreRadiusWeights second_scaled;
    core_radius_scaled(weights, constant, &plain);
    core_radius_second(weights, &second);
    core_radius_scaled(&second, second_factor, &second_scaled);
    core_radius_sum(&plain, &second_scaled, out);
    core_radius_released(&plain);
    core_radius_released(&second);
    core_radius_released(&second_scaled);
}

void core_radius_l(const CoreRadiusWeights *weights, const CoreRadiusScale *scale, SimRational s,
                   CoreRadiusWeights *out)
{
    const SimRational h_square = core_radius_times(core_radius_times(scale->h, scale->cut), scale->cut);
    core_radius_two_terms(weights, sim_rational_difference(core_radius_number(1ll, 1ll), h_square),
                          core_radius_times(s, h_square), out);
}

void core_radius_d(const CoreRadiusWeights *weights, const CoreRadiusScale *scale, SimRational s,
                   CoreRadiusWeights *out)
{
    const SimRational half_square = core_radius_half_square(scale);
    core_radius_two_terms(weights, sim_rational_difference(core_radius_number(1ll, 1ll), half_square),
                          core_radius_times(s, half_square), out);
}

void core_radius_eta_square(const CoreRadiusWeights *weights, const CoreRadiusScale *scale, CoreRadiusWeights *out)
{
    CoreRadiusWeights second;
    CoreRadiusWeights sum;
    core_radius_second(weights, &second);
    core_radius_sum(weights, &second, &sum);
    core_radius_scaled(&sum, core_radius_half_square(scale), out);
    core_radius_released(&second);
    core_radius_released(&sum);
}

void core_radius_slope(const CoreRadiusWeights *weights, CoreRadiusWeights *out)
{
    core_radius_taken(out, (weights->count >= 2u) ? (weights->count - 1u) : 0u, 1);
    if (out->room == 0u)
    {
        return;
    }
    for (unsigned int j = 0u; j < out->room; j += 1u)
    {
        out->value[j] = core_radius_number(0ll, 1ll);
    }
    out->count = out->room;
    SimRational running[2] = {core_radius_number(0ll, 1ll), core_radius_number(0ll, 1ll)};
    for (unsigned int m = weights->count - 1u; m >= 1u; m -= 1u)
    {
        running[m & 1u] = core_radius_plus(
            running[m & 1u], core_radius_times(core_radius_number(2ll * (long long)m, 1ll), weights->value[m]));
        const unsigned int j = m - 1u;
        out->value[j] = (j == 0u) ? core_radius_times(core_radius_number(1ll, 2ll), running[m & 1u]) : running[m & 1u];
    }
}

void core_radius_feet_slope(const CoreRadiusWeights *weights, SimRational s, CoreRadiusWeights *out)
{
    core_radius_taken(out, (weights->count >= 2u) ? (weights->count + 1u) : 0u, 1);
    for (unsigned int m = 1u; m < weights->count; m += 1u)
    {
        const SimRational half = core_radius_times(core_radius_number((long long)m, 2ll), weights->value[m]);
        core_radius_add(out, m - 1u, half);
        core_radius_add(out, m + 1u, core_radius_times(s, half));
    }
}

void core_radius_product(const CoreRadiusWeights *left, const CoreRadiusWeights *right, CoreRadiusWeights *out)
{
    const SimRational half = core_radius_number(1ll, 2ll);
    core_radius_taken(out, ((left->count != 0u) && (right->count != 0u)) ? (left->count + right->count - 1u) : 0u, 1);
    for (unsigned int m = 0u; m < left->count; m += 1u)
    {
        if (sim_rational_sign(left->value[m]) == 0)
        {
            continue;
        }
        for (unsigned int n = 0u; n < right->count; n += 1u)
        {
            const SimRational part = core_radius_times(half, core_radius_times(left->value[m], right->value[n]));
            core_radius_add(out, m + n, part);
            core_radius_add(out, (m >= n) ? m - n : n - m, part);
        }
    }
}

void core_radius_d_slope(const CoreRadiusWeights *weights, const CoreRadiusScale *scale, SimRational s,
                         CoreRadiusWeights *out)
{
    CoreRadiusWeights slope;
    core_radius_feet_slope(weights, s, &slope);
    const SimRational rest = sim_rational_difference(core_radius_number(1ll, 1ll), core_radius_times(scale->cut, scale->cut));
    if (sim_rational_sign(rest) != 0)
    {
        CoreRadiusWeights plain;
        CoreRadiusWeights second;
        CoreRadiusWeights both;
        CoreRadiusWeights rest_part;
        core_radius_slope(weights, &plain);
        core_radius_second(&plain, &second);
        core_radius_sum(&plain, &second, &both);
        core_radius_scaled(&both, core_radius_times(rest, core_radius_number(1ll, 2ll)), &rest_part);
        core_radius_sum_into(&slope, &rest_part);
        core_radius_released(&plain);
        core_radius_released(&second);
        core_radius_released(&both);
    }
    core_radius_scaled(&slope, sim_rational_reciprocal(scale->cut), out);
    core_radius_released(&slope);
}

void core_radius_cut_weights(const CoreRadiusWeights *weights, SimRational cut, CoreRadiusWeights *out)
{
    *out = CoreRadiusWeights{NULL, 0u, 0u};
    CoreRadiusWeights before = {NULL, 0u, 0u};
    CoreRadiusWeights current;
    core_radius_taken(&current, 1u, 1);
    core_radius_pushed(&current, core_radius_number(1ll, 1ll));
    const SimRational twice = core_radius_times(core_radius_number(2ll, 1ll), cut);
    for (unsigned int m = 0u; m < weights->count; m += 1u)
    {
        CoreRadiusWeights term;
        core_radius_scaled(&current, weights->value[m], &term);
        core_radius_sum_into(out, &term);
        CoreRadiusWeights eta;
        CoreRadiusWeights next;
        core_radius_eta(&current, &eta);
        if (m == 0u)
        {
            core_radius_scaled(&eta, cut, &next);
        }
        else
        {
            CoreRadiusWeights eta_twice;
            CoreRadiusWeights before_turned;
            core_radius_scaled(&eta, twice, &eta_twice);
            core_radius_scaled(&before, core_radius_number(-1ll, 1ll), &before_turned);
            core_radius_sum(&eta_twice, &before_turned, &next);
            core_radius_released(&eta_twice);
            core_radius_released(&before_turned);
        }
        core_radius_released(&eta);
        core_radius_replaced(&before, &current);
        current = next;
    }
    core_radius_released(&before);
    core_radius_released(&current);
}

void core_radius_turned_weights(const CoreRadiusWeights *g, SimRational c, unsigned int j, const CoreRadiusScale *scale,
                                SimRational s, CoreRadiusWeights *out)
{
    const SimRational order_j = core_radius_number((long long)j, 1ll);
    const SimRational eta_turn = core_radius_times(s, core_radius_plus(c, core_radius_times(core_radius_number(2ll, 1ll), order_j)));
    CoreRadiusWeights l;
    CoreRadiusWeights l_eta;
    core_radius_l(g, scale, s, &l);
    core_radius_eta(&l, &l_eta);
    core_radius_scaled(&l_eta, core_radius_times(eta_turn, scale->cut), out);
    core_radius_released(&l);
    core_radius_released(&l_eta);
    CoreRadiusWeights d_slope;
    CoreRadiusWeights l_d_slope;
    core_radius_d_slope(g, scale, s, &d_slope);
    core_radius_l(&d_slope, scale, s, &l_d_slope);
    core_radius_sum_into(out, &l_d_slope);
    core_radius_released(&d_slope);
    const SimRational eight_h_j = core_radius_times(core_radius_times(core_radius_number(8ll, 1ll), scale->h), order_j);
    CoreRadiusWeights d;
    CoreRadiusWeights d_eta;
    CoreRadiusWeights d_part;
    core_radius_d(g, scale, s, &d);
    core_radius_eta(&d, &d_eta);
    core_radius_scaled(&d_eta, core_radius_times(eight_h_j, scale->cut), &d_part);
    core_radius_sum_into(out, &d_part);
    core_radius_released(&d);
    core_radius_released(&d_eta);
}

void core_radius_orders_taken(CoreRadiusOrders *orders, unsigned int room)
{
    CoreRadiusWeights **const fields[4] = {&orders->f, &orders->g, &orders->q, &orders->inflow};
    for (unsigned int field = 0u; field < 4u; field += 1u)
    {
        *fields[field] = (CoreRadiusWeights *)platform_cell_cleared(room, sizeof(CoreRadiusWeights),
                                                                    platform_place_g_program_owner, 0);
        s_core_radius_unmet |= (*fields[field] == NULL) ? 1 : 0;
    }
    const int whole = (orders->f != NULL) && (orders->g != NULL) && (orders->q != NULL) && (orders->inflow != NULL);
    orders->count = 0u;
    orders->inflow_count = 0u;
    orders->room = whole ? room : 0u;
}

void core_radius_orders_released(CoreRadiusOrders *orders)
{
    CoreRadiusWeights **const fields[4] = {&orders->f, &orders->g, &orders->q, &orders->inflow};
    for (unsigned int field = 0u; field < 4u; field += 1u)
    {
        for (unsigned int k = 0u; (*fields[field] != NULL) && (k < orders->room); k += 1u)
        {
            core_radius_released(&(*fields[field])[k]);
        }
        platform_cell_released(*fields[field], platform_place_g_program_owner);
        *fields[field] = NULL;
    }
    orders->count = 0u;
    orders->inflow_count = 0u;
    orders->room = 0u;
}

SimRational core_radius_up(SimRational value, const OriorExactInteger *unit)
{
    OriorExactInteger scaled;
    OriorExactInteger quotient;
    OriorExactInteger remainder;
    OriorExactInteger zero;
    OriorExactInteger one;
    sim_exact_unsigned(&zero, 0ull);
    sim_exact_unsigned(&one, 1ull);
    sim_rational_status_check((orior_exact_multiply(&value.numerator, unit, &scaled) == ORIOR_EXACT_OK) &&
                              (orior_exact_divide(&scaled, &value.denominator, &quotient, &remainder) == ORIOR_EXACT_OK));
    if (orior_exact_compare(&remainder, &zero) != 0)
    {
        sim_rational_status_check(orior_exact_add(&quotient, &one, &quotient) == ORIOR_EXACT_OK);
    }
    SimRational up;
    up.numerator = quotient;
    up.denominator = *unit;
    sim_rational_settle(&up);
    return up;
}

SimRational core_radius_down(SimRational value, const OriorExactInteger *unit)
{
    OriorExactInteger scaled;
    OriorExactInteger quotient;
    OriorExactInteger remainder;
    sim_rational_status_check((orior_exact_multiply(&value.numerator, unit, &scaled) == ORIOR_EXACT_OK) &&
                              (orior_exact_divide(&scaled, &value.denominator, &quotient, &remainder) == ORIOR_EXACT_OK));
    SimRational down;
    down.numerator = quotient;
    down.denominator = *unit;
    sim_rational_settle(&down);
    return down;
}

void core_radius_round_up(CoreRadiusWeights *weights, const OriorExactInteger *unit)
{
    for (unsigned int m = 0u; m < weights->count; m += 1u)
    {
        weights->value[m] = core_radius_up(weights->value[m], unit);
    }
}

// [x y]_n modulo p for x of length x_length and y of length y_length
__device__ static unsigned long long core_radius_device_pair(const uint32_t *x, unsigned int x_length, const uint32_t *y, unsigned int y_length, unsigned int n,
                                                              unsigned long long p)
{
    unsigned long long total = 0ull;
    if ((x_length == 0u) || (y_length == 0u))
    {
        return total;
    }
    // m + q = n
    const unsigned int low = (n + 1u > y_length) ? n + 1u - y_length : 0u;
    const unsigned int high = (n < x_length - 1u) ? n : x_length - 1u;
    for (unsigned int m = low; m <= high; m += 1u)
    {
        total = (total + (unsigned long long)x[m] * y[n - m]) % p;
    }
    if (n == 0u)
    {
        const unsigned int both = (x_length < y_length) ? x_length : y_length;
        for (unsigned int m = 0u; m < both; m += 1u)
        {
            total = (total + (unsigned long long)x[m] * y[m]) % p;
        }
        return total;
    }
    // q = m - n
    for (unsigned int m = n; (m < x_length) && (m - n < y_length); m += 1u)
    {
        total = (total + (unsigned long long)x[m] * y[m - n]) % p;
    }
    // q = m + n
    for (unsigned int m = 0u; (m < x_length) && (m + n < y_length); m += 1u)
    {
        total = (total + (unsigned long long)x[m] * y[m + n]) % p;
    }
    return total;
}

// one thread to the mode n, the prime blockIdx.y and the sum blockIdx.z of order k's sums
__global__ static void core_radius_device_kernel(const uint32_t *residues, const uint32_t *lengths, const uint32_t *primes, unsigned int primes_count,
                                                 unsigned int orders, unsigned int modes, unsigned int k, uint32_t *sums)
{
    const unsigned int n = blockIdx.x * blockDim.x + threadIdx.x;
    const unsigned int t = blockIdx.y;
    const unsigned int sum = blockIdx.z;
    if (n >= modes)
    {
        return;
    }
    const unsigned long long p = primes[t];
    unsigned long long total = 0ull;
    for (unsigned int i = 0u; i <= k; i += 1u)
    {
        const unsigned int j = k - i;
        unsigned int left_type[2] = {CORE_RADIUS_SPIN_WEIGHTS, CORE_RADIUS_SPIN_WEIGHTS};
        unsigned int right_type[2] = {CORE_RADIUS_SPIN_WEIGHTS, CORE_RADIUS_SPIN_WEIGHTS};
        unsigned long long weight[2] = {1ull, 0ull};
        if (sum == 0u)
        {
            left_type[0] = CORE_RADIUS_INFLOW_WEIGHTS;
            right_type[0] = CORE_RADIUS_SPIN_WEIGHTS;
            weight[0] = (unsigned long long)j + 1ull;
            left_type[1] = CORE_RADIUS_ALONG_WEIGHTS;
            right_type[1] = CORE_RADIUS_ANGULAR_TURNED;
            weight[1] = 1ull;
        }
        else if (sum == 1u)
        {
            left_type[0] = CORE_RADIUS_INFLOW_WEIGHTS;
            right_type[0] = CORE_RADIUS_ALONG_WEIGHTS;
            weight[0] = (unsigned long long)j;
            left_type[1] = CORE_RADIUS_ALONG_WEIGHTS;
            right_type[1] = CORE_RADIUS_ALONG_TURNED;
            weight[1] = 1ull;
        }
        for (unsigned int part = 0u; part < 2u; part += 1u)
        {
            if (weight[part] == 0ull)
            {
                continue;
            }
            const unsigned int left = left_type[part] * orders + i;
            const unsigned int right = right_type[part] * orders + j;
            const uint32_t *const x = residues + ((size_t)left * primes_count + t) * modes;
            const uint32_t *const y = residues + ((size_t)right * primes_count + t) * modes;
            const unsigned long long pair = core_radius_device_pair(x, lengths[left], y, lengths[right], n, p);
            total = (total + (weight[part] % p) * pair) % p;
        }
    }
    sums[((size_t)sum * primes_count + t) * modes + n] = (uint32_t)total;
}

static unsigned long long core_radius_power_mod(unsigned long long base, unsigned long long power, unsigned long long p)
{
    unsigned long long result = 1ull;
    base %= p;
    while (power > 0ull)
    {
        if ((power & 1ull) != 0ull)
        {
            result = (result * base) % p;
        }
        base = (base * base) % p;
        power >>= 1u;
    }
    return result;
}

// the bits of a magnitude, 0 for 0
static unsigned int core_radius_bits(const OriorExactInteger *value)
{
    const unsigned long long used = sim_exact_limbs_used(value);
    if (used == 0ull)
    {
        return 0u;
    }
    unsigned int top = 0u;
    uint32_t limb = value->limb[used - 1ull];
    while (limb != 0u)
    {
        top += 1u;
        limb >>= 1u;
    }
    return (unsigned int)(32ull * (used - 1ull)) + top;
}

void core_radius_device_open(CoreRadiusDevice *device, unsigned int orders, unsigned int modes, unsigned int primes_count)
{
    device->primes_count = primes_count;
    device->orders = orders;
    device->modes = modes;
    device->widest = 0u;
    device->residues = NULL;
    device->lengths_device = NULL;
    device->primes_device = NULL;
    device->sums = NULL;
    // the host's lengths, primes and inverses, each a cell sized at its count, the lengths and the inverses all 0
    const unsigned long long lengths_count = (unsigned long long)CORE_RADIUS_TYPES * orders;
    device->lengths =
        (uint32_t *)platform_cell_cleared(lengths_count, sizeof(uint32_t), platform_place_g_program_owner, 0);
    device->primes =
        (uint32_t *)platform_cell_cleared(primes_count, sizeof(uint32_t), platform_place_g_program_owner, 0);
    device->inverse = (uint32_t *)platform_cell_cleared((unsigned long long)primes_count * primes_count,
                                                        sizeof(uint32_t), platform_place_g_program_owner, 0);
    const int host = (device->lengths != NULL) && (device->primes != NULL) && (device->inverse != NULL);
    s_core_radius_unmet |= host ? 0 : 1;
    // the largest primes below 2^31, each tested by trial division
    unsigned int found = 0u;
    for (uint32_t candidate = 0x7FFFFFFFu; host && (found < primes_count); candidate -= 2u)
    {
        int prime = 1;
        for (uint32_t divisor = 3u; (unsigned long long)divisor * divisor <= candidate; divisor += 2u)
        {
            if (candidate % divisor == 0u)
            {
                prime = 0;
                break;
            }
        }
        if (prime)
        {
            device->primes[found] = candidate;
            found += 1u;
        }
    }
    for (unsigned int t = 0u; host && (t < primes_count); t += 1u)
    {
        for (unsigned int s = 0u; s < t; s += 1u)
        {
            // Fermat: p_s^(p_t - 2) mod p_t
            device->inverse[(size_t)t * primes_count + s] =
                (uint32_t)core_radius_power_mod(device->primes[s], (unsigned long long)device->primes[t] - 2ull, device->primes[t]);
        }
    }
    int count = 0;
    const size_t residue_bytes = (size_t)CORE_RADIUS_TYPES * orders * primes_count * modes * sizeof(uint32_t);
    const size_t sum_bytes = (size_t)CORE_RADIUS_SUMS * primes_count * modes * sizeof(uint32_t);
    const size_t lengths_bytes = (size_t)lengths_count * sizeof(uint32_t);
    device->available = host && (cudaGetDeviceCount(&count) == cudaSuccess) && (count > 0) &&
                        (cudaMalloc((void **)&device->residues, residue_bytes) == cudaSuccess) &&
                        (cudaMemset(device->residues, 0, residue_bytes) == cudaSuccess) &&
                        (cudaMalloc((void **)&device->lengths_device, lengths_bytes) == cudaSuccess) &&
                        (cudaMemset(device->lengths_device, 0, lengths_bytes) == cudaSuccess) &&
                        (cudaMalloc((void **)&device->primes_device, primes_count * sizeof(uint32_t)) == cudaSuccess) &&
                        (cudaMemcpy(device->primes_device, device->primes, primes_count * sizeof(uint32_t),
                                    cudaMemcpyHostToDevice) == cudaSuccess) &&
                        (cudaMalloc((void **)&device->sums, sum_bytes) == cudaSuccess);
}

void core_radius_device_close(CoreRadiusDevice *device)
{
    cudaFree(device->residues);
    cudaFree(device->lengths_device);
    cudaFree(device->primes_device);
    cudaFree(device->sums);
    platform_cell_released(device->lengths, platform_place_g_program_owner);
    platform_cell_released(device->primes, platform_place_g_program_owner);
    platform_cell_released(device->inverse, platform_place_g_program_owner);
    device->lengths = NULL;
    device->primes = NULL;
    device->inverse = NULL;
}

void core_radius_device_put(CoreRadiusDevice *device, unsigned int type, unsigned int k,
                            const CoreRadiusWeights *weights, const OriorExactInteger *unit)
{
    if (!device->available)
    {
        return;
    }
    if ((k >= device->orders) || (weights->count > device->modes))
    {
        device->available = 0;
        return;
    }
    // the residues of one order, every prime's modes, in a cell of the temporary tier that the call gives back
    const unsigned long long residues_count = (unsigned long long)device->primes_count * device->modes;
    uint32_t *const residues =
        (uint32_t *)platform_cell_cleared(residues_count, sizeof(uint32_t), platform_place_g_program_owner, 1);
    if (residues == NULL)
    {
        s_core_radius_unmet = 1;
        device->available = 0;
        return;
    }
    for (unsigned int m = 0u; m < weights->count; m += 1u)
    {
        OriorExactInteger scaled;
        OriorExactInteger whole;
        OriorExactInteger remainder;
        OriorExactInteger zero;
        sim_exact_unsigned(&zero, 0ull);
        // c 2^bits, whole where c is a multiple of 2^-bits and at least 0
        const int read =
            (orior_exact_multiply(&weights->value[m].numerator, unit, &scaled) == ORIOR_EXACT_OK) &&
            (orior_exact_divide(&scaled, &weights->value[m].denominator, &whole, &remainder) == ORIOR_EXACT_OK) &&
            (orior_exact_compare(&remainder, &zero) == 0) && (sim_rational_sign(weights->value[m]) >= 0);
        if (!read)
        {
            device->available = 0;
            platform_cell_released(residues, platform_place_g_program_owner);
            return;
        }
        const unsigned int bits = core_radius_bits(&whole);
        device->widest = (bits > device->widest) ? bits : device->widest;
        const unsigned long long used = sim_exact_limbs_used(&whole);
        for (unsigned int t = 0u; t < device->primes_count; t += 1u)
        {
            const unsigned long long p = device->primes[t];
            unsigned long long residue = 0ull;
            for (unsigned long long index = used; index > 0ull; index -= 1ull)
            {
                residue = ((residue << 32u) | whole.limb[index - 1ull]) % p;
            }
            // a residue modulo a prime below 2^31 fits a limb
            residues[(size_t)t * device->modes + m] = (uint32_t)residue;
        }
    }
    const size_t at = (size_t)type * device->orders + k;
    // a length is at max the device's modes, which fit 32 bits
    device->lengths[at] = (uint32_t)weights->count;
    device->available =
        (cudaMemcpy(device->residues + at * device->primes_count * device->modes, residues,
                    (size_t)residues_count * sizeof(uint32_t), cudaMemcpyHostToDevice) == cudaSuccess) &&
        (cudaMemcpy(device->lengths_device + at, &device->lengths[at], sizeof(uint32_t), cudaMemcpyHostToDevice) ==
         cudaSuccess);
    platform_cell_released(residues, platform_place_g_program_owner);
}

void core_radius_device_sums(CoreRadiusDevice *device, unsigned int k, const OriorExactInteger *unit, CoreRadiusWeights sums[CORE_RADIUS_SUMS])
{
    for (unsigned int sum = 0u; sum < CORE_RADIUS_SUMS; sum += 1u)
    {
        sums[sum] = CoreRadiusWeights{NULL, 0u, 0u};
    }
    if (!device->available)
    {
        return;
    }
    // every sum is at max (k + 2) times the count of its products times 2^(2 widest); the lengths bound the count
    unsigned long long count = 0ull;
    unsigned int modes = 0u;
    const unsigned int pairs[CORE_RADIUS_SUMS][2][2] = {{{CORE_RADIUS_INFLOW_WEIGHTS, CORE_RADIUS_SPIN_WEIGHTS}, {CORE_RADIUS_ALONG_WEIGHTS, CORE_RADIUS_ANGULAR_TURNED}},
                                                         {{CORE_RADIUS_INFLOW_WEIGHTS, CORE_RADIUS_ALONG_WEIGHTS}, {CORE_RADIUS_ALONG_WEIGHTS, CORE_RADIUS_ALONG_TURNED}},
                                                         {{CORE_RADIUS_SPIN_WEIGHTS, CORE_RADIUS_SPIN_WEIGHTS}, {CORE_RADIUS_SPIN_WEIGHTS, CORE_RADIUS_SPIN_WEIGHTS}}};
    for (unsigned int sum = 0u; sum < CORE_RADIUS_SUMS; sum += 1u)
    {
        for (unsigned int part = 0u; part < ((sum == 2u) ? 1u : 2u); part += 1u)
        {
            for (unsigned int i = 0u; i <= k; i += 1u)
            {
                const unsigned int left = device->lengths[(size_t)pairs[sum][part][0] * device->orders + i];
                const unsigned int right = device->lengths[(size_t)pairs[sum][part][1] * device->orders + (k - i)];
                count += 2ull * left * right;
                if ((left > 0u) && (right > 0u))
                {
                    modes = (left + right - 1u > modes) ? (left + right - 1u) : modes;
                }
            }
        }
    }
    unsigned int count_bits = 0u;
    for (unsigned long long scale = count * ((unsigned long long)k + 2ull); scale != 0ull; scale >>= 1u)
    {
        count_bits += 1u;
    }
    if ((2u * device->widest + count_bits >= 30u * device->primes_count) || (modes > device->modes))
    {
        device->available = 0;
        return;
    }
    const dim3 grid((modes + CORE_RADIUS_THREADS - 1u) / CORE_RADIUS_THREADS, device->primes_count, CORE_RADIUS_SUMS);
    core_radius_device_kernel<<<grid, CORE_RADIUS_THREADS>>>(device->residues, device->lengths_device, device->primes_device, device->primes_count, device->orders,
                                                             device->modes, k, device->sums);
    // the sums' residues and one sum's mixed radix digits, in cells of the temporary tier that the call gives back
    const unsigned long long residues_count =
        (unsigned long long)CORE_RADIUS_SUMS * device->primes_count * device->modes;
    uint32_t *const residues =
        (uint32_t *)platform_cell_taken(residues_count * sizeof(uint32_t), platform_place_g_program_owner, 1);
    unsigned long long *const digits = (unsigned long long *)platform_cell_taken(
        (unsigned long long)device->primes_count * sizeof(unsigned long long), platform_place_g_program_owner, 1);
    s_core_radius_unmet |= ((residues == NULL) || (digits == NULL)) ? 1 : 0;
    device->available = (residues != NULL) && (digits != NULL) && (cudaGetLastError() == cudaSuccess) &&
                        (cudaMemcpy(residues, device->sums, (size_t)residues_count * sizeof(uint32_t),
                                    cudaMemcpyDeviceToHost) == cudaSuccess);
    if (!device->available)
    {
        platform_cell_released(residues, platform_place_g_program_owner);
        platform_cell_released(digits, platform_place_g_program_owner);
        return;
    }
    // 2^(2 bits + 1)
    OriorExactInteger square;
    OriorExactInteger scale;
    sim_rational_status_check((orior_exact_multiply(unit, unit, &square) == ORIOR_EXACT_OK) &&
                              sim_exact_scaled(&square, 2ull, &scale));
    for (unsigned int sum = 0u; sum < CORE_RADIUS_SUMS; sum += 1u)
    {
        core_radius_taken(&sums[sum], modes, 1);
        for (unsigned int n = 0u; n < modes; n += 1u)
        {
            // Garner: x = d_0 + p_0 (d_1 + p_1 (d_2 + ...)), d_t below p_t
            for (unsigned int t = 0u; t < device->primes_count; t += 1u)
            {
                const unsigned long long p = device->primes[t];
                unsigned long long digit = residues[((size_t)sum * device->primes_count + t) * device->modes + n];
                for (unsigned int s = 0u; s < t; s += 1u)
                {
                    digit = ((digit + p - (digits[s] % p)) % p) * device->inverse[(size_t)t * device->primes_count + s] % p;
                }
                digits[t] = digit;
            }
            OriorExactInteger whole;
            sim_exact_unsigned(&whole, digits[device->primes_count - 1u]);
            for (unsigned int t = device->primes_count - 1u; t > 0u; t -= 1u)
            {
                OriorExactInteger digit;
                OriorExactInteger raised;
                sim_exact_unsigned(&digit, digits[t - 1u]);
                sim_rational_status_check(sim_exact_scaled(&whole, device->primes[t - 1u], &raised) && sim_exact_sum(&raised, &digit, &whole));
            }
            SimRational value;
            value.numerator = whole;
            value.denominator = scale;
            sim_rational_settle(&value);
            core_radius_pushed(&sums[sum], value);
        }
        while ((sums[sum].count != 0u) && (sim_rational_sign(sums[sum].value[sums[sum].count - 1u]) == 0))
        {
            sums[sum].count -= 1u;
        }
    }
    platform_cell_released(residues, platform_place_g_program_owner);
    platform_cell_released(digits, platform_place_g_program_owner);
}

// `weights` given a copy on the persistent tier in `kept`, its own cell given back
static void core_radius_kept_from(CoreRadiusWeights *weights, CoreRadiusWeights *kept)
{
    core_radius_kept(weights, kept);
    core_radius_released(weights);
}

// `weights` times L, its slope's eta times L times d, and its eta^2 times 8 h k D: the plain part of an order's spin
// and along, `first` the factor of `weights` times L
static void core_radius_plain_part(const CoreRadiusWeights *weights, SimRational first, SimRational d_eight,
                                   const CoreRadiusScale *scale, SimRational s, CoreRadiusWeights *out)
{
    CoreRadiusWeights l;
    core_radius_l(weights, scale, s, &l);
    core_radius_scaled(&l, first, out);
    core_radius_released(&l);
    CoreRadiusWeights slope;
    CoreRadiusWeights slope_l;
    CoreRadiusWeights slope_eta;
    CoreRadiusWeights slope_part;
    core_radius_slope(weights, &slope);
    core_radius_l(&slope, scale, s, &slope_l);
    core_radius_eta(&slope_l, &slope_eta);
    core_radius_scaled(&slope_eta, scale->d, &slope_part);
    core_radius_sum_into(out, &slope_part);
    core_radius_released(&slope);
    core_radius_released(&slope_l);
    core_radius_released(&slope_eta);
    CoreRadiusWeights eta_square;
    CoreRadiusWeights eta_part;
    core_radius_eta_square(weights, scale, &eta_square);
    core_radius_scaled(&eta_square, d_eight, &eta_part);
    core_radius_sum_into(out, &eta_part);
    core_radius_released(&eta_square);
}

// the product of `left` and `right`, times `factor` where `scaled` is 1, summed into `into`
static void core_radius_product_into(CoreRadiusWeights *into, const CoreRadiusWeights *left,
                                     const CoreRadiusWeights *right, int scaled, SimRational factor)
{
    CoreRadiusWeights product;
    core_radius_product(left, right, &product);
    if (scaled)
    {
        CoreRadiusWeights product_scaled;
        core_radius_scaled(&product, factor, &product_scaled);
        core_radius_released(&product);
        core_radius_sum_into(into, &product_scaled);
    }
    else
    {
        core_radius_sum_into(into, &product);
    }
}

void core_radius_extend(const CoreRadiusScale *scale, SimRational s, unsigned int order, const OriorExactInteger *unit,
                        CoreRadiusDevice *device, CoreRadiusOrders *orders)
{
    const SimRational one = core_radius_number(1ll, 1ll);
    const SimRational two = core_radius_number(2ll, 1ll);
    const SimRational eight_h = core_radius_times(core_radius_number(8ll, 1ll), scale->h);
    const SimRational angular_c = core_radius_plus(two, core_radius_times(two, scale->h));
    const SimRational along_c = core_radius_times(two, scale->a);
    const SimRational pressure_c = core_radius_times(core_radius_number(4ll, 1ll), scale->a);
    CoreRadiusWeights *const f = orders->f;
    CoreRadiusWeights *const g = orders->g;
    CoreRadiusWeights *const q = orders->q;
    CoreRadiusWeights *const inflow = orders->inflow;
    for (unsigned int k = 0u; k < orders->inflow_count; k += 1u)
    {
        core_radius_released(&inflow[k]);
    }
    orders->inflow_count = 0u;
    // the Z parts of each order to `order`, in cells of the temporary tier that the call gives back before it returns
    CoreRadiusWeights *const angular_turned = (CoreRadiusWeights *)platform_cell_cleared(
        (unsigned long long)order + 1ull, sizeof(CoreRadiusWeights), platform_place_g_program_owner, 1);
    CoreRadiusWeights *const along_turned = (CoreRadiusWeights *)platform_cell_cleared(
        (unsigned long long)order + 1ull, sizeof(CoreRadiusWeights), platform_place_g_program_owner, 1);
    // the orders hold order + 2 orders of each field, and the inflow order + 1
    const int room =
        (angular_turned != NULL) && (along_turned != NULL) && (orders->room >= order + 2u) && (orders->count != 0u);
    s_core_radius_unmet |= room ? 0 : 1;
    for (unsigned int k = 0u; room && (k <= order); k += 1u)
    {
        const SimRational order_k = core_radius_number((long long)k, 1ll);
        const SimRational next = core_radius_plus(order_k, one);
        core_radius_turned_weights(&g[k], along_c, k, scale, s, &along_turned[k]);
        CoreRadiusWeights inflow_made;
        core_radius_scaled(&along_turned[k], core_radius_times(s, sim_rational_reciprocal(next)), &inflow_made);
        core_radius_kept_from(&inflow_made, &inflow[k]);
        orders->inflow_count = k + 1u;
        core_radius_turned_weights(&f[k], angular_c, k, scale, s, &angular_turned[k]);
        if (unit != NULL)
        {
            core_radius_round_up(&along_turned[k], unit);
            core_radius_round_up(&inflow[k], unit);
            core_radius_round_up(&angular_turned[k], unit);
        }
        const int on_device = (device != NULL) && (unit != NULL);
        if (on_device)
        {
            core_radius_device_put(device, CORE_RADIUS_SPIN_WEIGHTS, k, &f[k], unit);
            core_radius_device_put(device, CORE_RADIUS_ALONG_WEIGHTS, k, &g[k], unit);
            core_radius_device_put(device, CORE_RADIUS_INFLOW_WEIGHTS, k, &inflow[k], unit);
            core_radius_device_put(device, CORE_RADIUS_ANGULAR_TURNED, k, &angular_turned[k], unit);
            core_radius_device_put(device, CORE_RADIUS_ALONG_TURNED, k, &along_turned[k], unit);
        }
        if (k + 1u < orders->count)
        {
            continue;
        }
        const SimRational d_eight = core_radius_times(core_radius_times(eight_h, order_k), scale->d);
        CoreRadiusWeights spin;
        core_radius_plain_part(&f[k], core_radius_plus(next, scale->h), d_eight, scale, s, &spin);
        CoreRadiusWeights along;
        core_radius_plain_part(&g[k], core_radius_plus(order_k, scale->a), d_eight, scale, s, &along);
        CoreRadiusWeights pressure_turned;
        core_radius_turned_weights(&q[k], pressure_c, k, scale, s, &pressure_turned);
        core_radius_sum_into(&along, &pressure_turned);
        CoreRadiusWeights square = {NULL, 0u, 0u};
        if (on_device)
        {
            CoreRadiusWeights sums[CORE_RADIUS_SUMS];
            core_radius_device_sums(device, k, unit, sums);
            core_radius_sum_into(&spin, &sums[0]);
            core_radius_sum_into(&along, &sums[1]);
            square = sums[2];
        }
        else
        {
            for (unsigned int i = 0u; i <= k; i += 1u)
            {
                const unsigned int j = k - i;
                const SimRational rest = core_radius_number((long long)j, 1ll);
                core_radius_product_into(&spin, &inflow[i], &f[j], 1, core_radius_plus(rest, one));
                core_radius_product_into(&spin, &g[i], &angular_turned[j], 0, one);
                core_radius_product_into(&along, &inflow[i], &g[j], 1, rest);
                core_radius_product_into(&along, &g[i], &along_turned[j], 0, one);
                core_radius_product_into(&square, &f[i], &f[j], 0, one);
            }
        }
        const unsigned int at = orders->count;
        CoreRadiusWeights made;
        core_radius_scaled(
            &spin,
            sim_rational_reciprocal(core_radius_times(core_radius_times(two, next), core_radius_plus(next, one))),
            &made);
        core_radius_kept_from(&made, &f[at]);
        core_radius_scaled(&along, sim_rational_reciprocal(core_radius_times(two, core_radius_times(next, next))),
                           &made);
        core_radius_kept_from(&made, &g[at]);
        CoreRadiusWeights square_l;
        CoreRadiusWeights square_ll;
        core_radius_l(&square, scale, s, &square_l);
        core_radius_l(&square_l, scale, s, &square_ll);
        core_radius_scaled(&square_ll, sim_rational_reciprocal(next), &made);
        core_radius_kept_from(&made, &q[at]);
        core_radius_released(&square_l);
        core_radius_released(&square_ll);
        orders->count = at + 1u;
        if (unit != NULL)
        {
            core_radius_round_up(&f[at], unit);
            core_radius_round_up(&g[at], unit);
            core_radius_round_up(&q[at], unit);
        }
        core_radius_released(&spin);
        core_radius_released(&along);
        core_radius_released(&square);
    }
    for (unsigned int k = 0u; (angular_turned != NULL) && (k <= order); k += 1u)
    {
        core_radius_released(&angular_turned[k]);
    }
    for (unsigned int k = 0u; (along_turned != NULL) && (k <= order); k += 1u)
    {
        core_radius_released(&along_turned[k]);
    }
    platform_cell_released(angular_turned, platform_place_g_program_owner);
    platform_cell_released(along_turned, platform_place_g_program_owner);
}

int core_radius_same(const CoreRadiusWeights *left, const CoreRadiusWeights *right)
{
    const unsigned int length = (left->count > right->count) ? left->count : right->count;
    for (unsigned int m = 0u; m < length; m += 1u)
    {
        const SimRational one = (m < left->count) ? left->value[m] : core_radius_number(0ll, 1ll);
        const SimRational other = (m < right->count) ? right->value[m] : core_radius_number(0ll, 1ll);
        if (sim_rational_sign(sim_rational_difference(one, other)) != 0)
        {
            return 0;
        }
    }
    return 1;
}

SimRational core_radius_value(const CoreRadiusWeights *weights, SimRational xi)
{
    SimRational after = core_radius_number(0ll, 1ll);
    SimRational later = core_radius_number(0ll, 1ll);
    const SimRational twice = core_radius_times(core_radius_number(2ll, 1ll), xi);
    for (unsigned int m = weights->count; m > 1u; m -= 1u)
    {
        const SimRational current =
            sim_rational_difference(core_radius_plus(weights->value[m - 1u], core_radius_times(twice, after)), later);
        later = after;
        after = current;
    }
    const SimRational first = (weights->count == 0u) ? core_radius_number(0ll, 1ll) : weights->value[0];
    return sim_rational_difference(core_radius_plus(first, core_radius_times(xi, after)), later);
}

// the magnitudes of `weights`, or the weights themselves where `exact` is 1, as a copy on the persistent tier in `kept`
static void core_radius_kept_signed(const CoreRadiusWeights *weights, int exact, CoreRadiusWeights *kept)
{
    core_radius_taken(kept, weights->count, 0);
    for (unsigned int m = 0u; m < kept->room; m += 1u)
    {
        core_radius_pushed(kept, exact ? weights->value[m] : sim_rational_absolute(weights->value[m]));
    }
}

void core_radius_weights(const CoreRadiusScale *scale, const CoreRadiusWeights *angular, const CoreRadiusWeights *axial,
                         const CoreRadiusWeights *pressure, unsigned int order, SimRational s, CoreRadiusOrders *orders)
{
    const int exact = sim_rational_sign(s) < 0;
    core_radius_orders_taken(orders, order + 2u);
    const CoreRadiusWeights *const given[3] = {angular, axial, pressure};
    CoreRadiusWeights *const fields[3] = {orders->f, orders->g, orders->q};
    for (unsigned int field = 0u; (orders->room != 0u) && (field < 3u); field += 1u)
    {
        CoreRadiusWeights data;
        core_radius_cut_weights(given[field], scale->cut, &data);
        core_radius_kept_signed(&data, exact, &fields[field][0]);
        core_radius_released(&data);
    }
    orders->count = (orders->room != 0u) ? 1u : 0u;
    core_radius_extend(scale, s, order, NULL, NULL, orders);
}

void core_radius_continued(const CoreRadiusScale *scale, const CoreRadiusOrders *exact, unsigned int order,
                           unsigned int last, const OriorExactInteger *unit, CoreRadiusDevice *device,
                           CoreRadiusOrders *continued)
{
    core_radius_orders_taken(continued, last + 2u);
    const int given = (continued->room != 0u) && (exact->count > order) && (last >= order);
    s_core_radius_unmet |= given ? 0 : 1;
    for (unsigned int k = 0u; given && (k <= order); k += 1u)
    {
        core_radius_kept_signed(&exact->f[k], 0, &continued->f[k]);
        core_radius_kept_signed(&exact->g[k], 0, &continued->g[k]);
        core_radius_kept_signed(&exact->q[k], 0, &continued->q[k]);
        // the exact orders' magnitudes rounded up as they enter the carry, every weight it reads a multiple of 1 / unit
        core_radius_round_up(&continued->f[k], unit);
        core_radius_round_up(&continued->g[k], unit);
        core_radius_round_up(&continued->q[k], unit);
    }
    continued->count = given ? (order + 1u) : 0u;
    core_radius_extend(scale, core_radius_number(1ll, 1ll), last, unit, device, continued);
}

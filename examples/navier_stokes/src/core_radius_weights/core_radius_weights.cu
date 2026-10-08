// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// core_radius_weights.cu: the orders, their rule and the carry on the device (core_radius_weights.h)
#include "core_radius_weights.h"

#include <algorithm>

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

SimRational core_radius_most(SimRational left, SimRational right)
{
    return (sim_rational_sign(sim_rational_difference(left, right)) >= 0) ? left : right;
}

SimRational core_radius_norm(const std::vector<SimRational> &weights, SimRational rho)
{
    SimRational sum = core_radius_number(0ll, 1ll);
    SimRational power = core_radius_number(1ll, 1ll);
    for (const SimRational &weight : weights)
    {
        sum = core_radius_plus(sum, core_radius_times(sim_rational_absolute(weight), power));
        power = core_radius_times(power, rho);
    }
    return sum;
}

void core_radius_add(CoreRadiusWeights *into, size_t m, SimRational value)
{
    if (into->size() <= m)
    {
        into->resize(m + 1u, core_radius_number(0ll, 1ll));
    }
    (*into)[m] = core_radius_plus((*into)[m], value);
}

CoreRadiusWeights core_radius_sum(const CoreRadiusWeights &left, const CoreRadiusWeights &right)
{
    CoreRadiusWeights sum = left;
    for (size_t m = 0u; m < right.size(); m += 1u)
    {
        core_radius_add(&sum, m, right[m]);
    }
    return sum;
}

CoreRadiusWeights core_radius_scaled(const CoreRadiusWeights &weights, SimRational factor)
{
    CoreRadiusWeights scaled;
    for (const SimRational &weight : weights)
    {
        scaled.push_back(core_radius_times(weight, factor));
    }
    return scaled;
}

CoreRadiusWeights core_radius_eta(const CoreRadiusWeights &weights)
{
    const SimRational half = core_radius_number(1ll, 2ll);
    CoreRadiusWeights eta;
    for (size_t m = 0u; m < weights.size(); m += 1u)
    {
        if (m == 0u)
        {
            core_radius_add(&eta, 1u, weights[0]);
        }
        else
        {
            core_radius_add(&eta, m + 1u, core_radius_times(half, weights[m]));
            core_radius_add(&eta, m - 1u, core_radius_times(half, weights[m]));
        }
    }
    return eta;
}

CoreRadiusWeights core_radius_second(const CoreRadiusWeights &weights)
{
    const SimRational half = core_radius_number(1ll, 2ll);
    CoreRadiusWeights second;
    for (size_t m = 0u; m < weights.size(); m += 1u)
    {
        core_radius_add(&second, m + 2u, core_radius_times(half, weights[m]));
        core_radius_add(&second, (m >= 2u) ? m - 2u : 2u - m, core_radius_times(half, weights[m]));
    }
    return second;
}

SimRational core_radius_half_square(const CoreRadiusScale *scale)
{
    return core_radius_times(core_radius_times(scale->cut, scale->cut), core_radius_number(1ll, 2ll));
}

CoreRadiusWeights core_radius_l(const CoreRadiusWeights &weights, const CoreRadiusScale *scale, SimRational s)
{
    const SimRational h_square = core_radius_times(core_radius_times(scale->h, scale->cut), scale->cut);
    return core_radius_sum(core_radius_scaled(weights, sim_rational_difference(core_radius_number(1ll, 1ll), h_square)),
                           core_radius_scaled(core_radius_second(weights), core_radius_times(s, h_square)));
}

CoreRadiusWeights core_radius_d(const CoreRadiusWeights &weights, const CoreRadiusScale *scale, SimRational s)
{
    const SimRational half_square = core_radius_half_square(scale);
    return core_radius_sum(core_radius_scaled(weights, sim_rational_difference(core_radius_number(1ll, 1ll), half_square)),
                           core_radius_scaled(core_radius_second(weights), core_radius_times(s, half_square)));
}

CoreRadiusWeights core_radius_eta_square(const CoreRadiusWeights &weights, const CoreRadiusScale *scale)
{
    return core_radius_scaled(core_radius_sum(weights, core_radius_second(weights)), core_radius_half_square(scale));
}

CoreRadiusWeights core_radius_slope(const CoreRadiusWeights &weights)
{
    CoreRadiusWeights slope;
    if (weights.size() < 2u)
    {
        return slope;
    }
    slope.resize(weights.size() - 1u, core_radius_number(0ll, 1ll));
    SimRational running[2] = {core_radius_number(0ll, 1ll), core_radius_number(0ll, 1ll)};
    for (size_t m = weights.size() - 1u; m >= 1u; m -= 1u)
    {
        running[m & 1u] = core_radius_plus(running[m & 1u], core_radius_times(core_radius_number(2ll * (long long)m, 1ll), weights[m]));
        const size_t j = m - 1u;
        slope[j] = (j == 0u) ? core_radius_times(core_radius_number(1ll, 2ll), running[m & 1u]) : running[m & 1u];
    }
    return slope;
}

CoreRadiusWeights core_radius_feet_slope(const CoreRadiusWeights &weights, SimRational s)
{
    CoreRadiusWeights slope;
    for (size_t m = 1u; m < weights.size(); m += 1u)
    {
        const SimRational half = core_radius_times(core_radius_number((long long)m, 2ll), weights[m]);
        core_radius_add(&slope, m - 1u, half);
        core_radius_add(&slope, m + 1u, core_radius_times(s, half));
    }
    return slope;
}

CoreRadiusWeights core_radius_product(const CoreRadiusWeights &left, const CoreRadiusWeights &right)
{
    const SimRational half = core_radius_number(1ll, 2ll);
    CoreRadiusWeights product;
    for (size_t m = 0u; m < left.size(); m += 1u)
    {
        if (sim_rational_sign(left[m]) == 0)
        {
            continue;
        }
        for (size_t n = 0u; n < right.size(); n += 1u)
        {
            const SimRational part = core_radius_times(half, core_radius_times(left[m], right[n]));
            core_radius_add(&product, m + n, part);
            core_radius_add(&product, (m >= n) ? m - n : n - m, part);
        }
    }
    return product;
}

CoreRadiusWeights core_radius_d_slope(const CoreRadiusWeights &weights, const CoreRadiusScale *scale, SimRational s)
{
    CoreRadiusWeights slope = core_radius_feet_slope(weights, s);
    const SimRational rest = sim_rational_difference(core_radius_number(1ll, 1ll), core_radius_times(scale->cut, scale->cut));
    if (sim_rational_sign(rest) != 0)
    {
        const CoreRadiusWeights plain = core_radius_slope(weights);
        slope = core_radius_sum(slope, core_radius_scaled(core_radius_sum(plain, core_radius_second(plain)), core_radius_times(rest, core_radius_number(1ll, 2ll))));
    }
    return core_radius_scaled(slope, sim_rational_reciprocal(scale->cut));
}

CoreRadiusWeights core_radius_cut_weights(const std::vector<SimRational> &weights, SimRational cut)
{
    CoreRadiusWeights cut_weights;
    CoreRadiusWeights before;
    CoreRadiusWeights current = {core_radius_number(1ll, 1ll)};
    const SimRational twice = core_radius_times(core_radius_number(2ll, 1ll), cut);
    for (size_t m = 0u; m < weights.size(); m += 1u)
    {
        cut_weights = core_radius_sum(cut_weights, core_radius_scaled(current, weights[m]));
        const CoreRadiusWeights next = (m == 0u) ? core_radius_scaled(core_radius_eta(current), cut)
                                                 : core_radius_sum(core_radius_scaled(core_radius_eta(current), twice), core_radius_scaled(before, core_radius_number(-1ll, 1ll)));
        before = current;
        current = next;
    }
    return cut_weights;
}

CoreRadiusWeights core_radius_turned_weights(const CoreRadiusWeights &g, SimRational c, unsigned int j, const CoreRadiusScale *scale, SimRational s)
{
    const SimRational order_j = core_radius_number((long long)j, 1ll);
    const SimRational eta_turn = core_radius_times(s, core_radius_plus(c, core_radius_times(core_radius_number(2ll, 1ll), order_j)));
    CoreRadiusWeights turned = core_radius_scaled(core_radius_eta(core_radius_l(g, scale, s)), core_radius_times(eta_turn, scale->cut));
    turned = core_radius_sum(turned, core_radius_l(core_radius_d_slope(g, scale, s), scale, s));
    const SimRational eight_h_j = core_radius_times(core_radius_times(core_radius_number(8ll, 1ll), scale->h), order_j);
    return core_radius_sum(turned, core_radius_scaled(core_radius_eta(core_radius_d(g, scale, s)), core_radius_times(eight_h_j, scale->cut)));
}

SimRational core_radius_up(SimRational value, const AnchorExactInteger *unit)
{
    AnchorExactInteger scaled;
    AnchorExactInteger quotient;
    AnchorExactInteger remainder;
    AnchorExactInteger zero;
    AnchorExactInteger one;
    sim_exact_unsigned(&zero, 0ull);
    sim_exact_unsigned(&one, 1ull);
    sim_rational_status_check((anchor_exact_multiply(&value.numerator, unit, &scaled) == ANCHOR_EXACT_OK) &&
                              (anchor_exact_divide(&scaled, &value.denominator, &quotient, &remainder) == ANCHOR_EXACT_OK));
    if (anchor_exact_compare(&remainder, &zero) != 0)
    {
        sim_rational_status_check(anchor_exact_add(&quotient, &one, &quotient) == ANCHOR_EXACT_OK);
    }
    SimRational up;
    up.numerator = quotient;
    up.denominator = *unit;
    sim_rational_settle(&up);
    return up;
}

SimRational core_radius_down(SimRational value, const AnchorExactInteger *unit)
{
    AnchorExactInteger scaled;
    AnchorExactInteger quotient;
    AnchorExactInteger remainder;
    sim_rational_status_check((anchor_exact_multiply(&value.numerator, unit, &scaled) == ANCHOR_EXACT_OK) &&
                              (anchor_exact_divide(&scaled, &value.denominator, &quotient, &remainder) == ANCHOR_EXACT_OK));
    SimRational down;
    down.numerator = quotient;
    down.denominator = *unit;
    sim_rational_settle(&down);
    return down;
}

void core_radius_round_up(CoreRadiusWeights *weights, const AnchorExactInteger *unit)
{
    for (SimRational &weight : *weights)
    {
        weight = core_radius_up(weight, unit);
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
        unsigned int left_kind[2] = {CORE_RADIUS_SPIN_WEIGHTS, CORE_RADIUS_SPIN_WEIGHTS};
        unsigned int right_kind[2] = {CORE_RADIUS_SPIN_WEIGHTS, CORE_RADIUS_SPIN_WEIGHTS};
        unsigned long long weight[2] = {1ull, 0ull};
        if (sum == 0u)
        {
            left_kind[0] = CORE_RADIUS_INFLOW_WEIGHTS;
            right_kind[0] = CORE_RADIUS_SPIN_WEIGHTS;
            weight[0] = (unsigned long long)j + 1ull;
            left_kind[1] = CORE_RADIUS_ALONG_WEIGHTS;
            right_kind[1] = CORE_RADIUS_ANGULAR_TURNED;
            weight[1] = 1ull;
        }
        else if (sum == 1u)
        {
            left_kind[0] = CORE_RADIUS_INFLOW_WEIGHTS;
            right_kind[0] = CORE_RADIUS_ALONG_WEIGHTS;
            weight[0] = (unsigned long long)j;
            left_kind[1] = CORE_RADIUS_ALONG_WEIGHTS;
            right_kind[1] = CORE_RADIUS_ALONG_TURNED;
            weight[1] = 1ull;
        }
        for (unsigned int part = 0u; part < 2u; part += 1u)
        {
            if (weight[part] == 0ull)
            {
                continue;
            }
            const unsigned int left = left_kind[part] * orders + i;
            const unsigned int right = right_kind[part] * orders + j;
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
static unsigned int core_radius_bits(const AnchorExactInteger *value)
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
    device->lengths.assign((size_t)CORE_RADIUS_KINDS * orders, 0u);
    // the largest primes below 2^31, each tested by trial division
    device->primes.clear();
    for (uint32_t candidate = 0x7FFFFFFFu; device->primes.size() < primes_count; candidate -= 2u)
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
            device->primes.push_back(candidate);
        }
    }
    device->inverse.assign((size_t)primes_count * primes_count, 0u);
    for (unsigned int t = 0u; t < primes_count; t += 1u)
    {
        for (unsigned int s = 0u; s < t; s += 1u)
        {
            // Fermat: p_s^(p_t - 2) mod p_t
            device->inverse[(size_t)t * primes_count + s] =
                (uint32_t)core_radius_power_mod(device->primes[s], (unsigned long long)device->primes[t] - 2ull, device->primes[t]);
        }
    }
    int count = 0;
    const size_t residue_bytes = (size_t)CORE_RADIUS_KINDS * orders * primes_count * modes * sizeof(uint32_t);
    const size_t sum_bytes = (size_t)CORE_RADIUS_SUMS * primes_count * modes * sizeof(uint32_t);
    device->held = (cudaGetDeviceCount(&count) == cudaSuccess) && (count > 0) && (cudaMalloc((void **)&device->residues, residue_bytes) == cudaSuccess) &&
                   (cudaMemset(device->residues, 0, residue_bytes) == cudaSuccess) &&
                   (cudaMalloc((void **)&device->lengths_device, device->lengths.size() * sizeof(uint32_t)) == cudaSuccess) &&
                   (cudaMemset(device->lengths_device, 0, device->lengths.size() * sizeof(uint32_t)) == cudaSuccess) &&
                   (cudaMalloc((void **)&device->primes_device, primes_count * sizeof(uint32_t)) == cudaSuccess) &&
                   (cudaMemcpy(device->primes_device, device->primes.data(), primes_count * sizeof(uint32_t), cudaMemcpyHostToDevice) == cudaSuccess) &&
                   (cudaMalloc((void **)&device->sums, sum_bytes) == cudaSuccess);
}

void core_radius_device_close(CoreRadiusDevice *device)
{
    cudaFree(device->residues);
    cudaFree(device->lengths_device);
    cudaFree(device->primes_device);
    cudaFree(device->sums);
}

void core_radius_device_put(CoreRadiusDevice *device, unsigned int kind, unsigned int k, const CoreRadiusWeights &weights, const AnchorExactInteger *unit)
{
    if (!device->held)
    {
        return;
    }
    if ((k >= device->orders) || (weights.size() > device->modes))
    {
        device->held = 0;
        return;
    }
    std::vector<uint32_t> residues((size_t)device->primes_count * device->modes, 0u);
    for (size_t m = 0u; m < weights.size(); m += 1u)
    {
        AnchorExactInteger scaled;
        AnchorExactInteger whole;
        AnchorExactInteger remainder;
        AnchorExactInteger zero;
        sim_exact_unsigned(&zero, 0ull);
        // c 2^bits, whole where c is a multiple of 2^-bits and at least 0
        const int read = (anchor_exact_multiply(&weights[m].numerator, unit, &scaled) == ANCHOR_EXACT_OK) &&
                         (anchor_exact_divide(&scaled, &weights[m].denominator, &whole, &remainder) == ANCHOR_EXACT_OK) &&
                         (anchor_exact_compare(&remainder, &zero) == 0) && (sim_rational_sign(weights[m]) >= 0);
        if (!read)
        {
            device->held = 0;
            return;
        }
        device->widest = std::max(device->widest, core_radius_bits(&whole));
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
    const size_t slot = (size_t)kind * device->orders + k;
    // a length is at most the device's modes, which fit 32 bits
    device->lengths[slot] = (uint32_t)weights.size();
    device->held = (cudaMemcpy(device->residues + slot * device->primes_count * device->modes, residues.data(), residues.size() * sizeof(uint32_t),
                               cudaMemcpyHostToDevice) == cudaSuccess) &&
                   (cudaMemcpy(device->lengths_device + slot, &device->lengths[slot], sizeof(uint32_t), cudaMemcpyHostToDevice) == cudaSuccess);
}

void core_radius_device_sums(CoreRadiusDevice *device, unsigned int k, const AnchorExactInteger *unit, CoreRadiusWeights sums[CORE_RADIUS_SUMS])
{
    for (unsigned int sum = 0u; sum < CORE_RADIUS_SUMS; sum += 1u)
    {
        sums[sum].clear();
    }
    if (!device->held)
    {
        return;
    }
    // every sum is at most (k + 2) times the count of its products times 2^(2 widest); the lengths bound the count
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
                    modes = std::max(modes, left + right - 1u);
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
        device->held = 0;
        return;
    }
    const dim3 grid((modes + CORE_RADIUS_THREADS - 1u) / CORE_RADIUS_THREADS, device->primes_count, CORE_RADIUS_SUMS);
    core_radius_device_kernel<<<grid, CORE_RADIUS_THREADS>>>(device->residues, device->lengths_device, device->primes_device, device->primes_count, device->orders,
                                                             device->modes, k, device->sums);
    std::vector<uint32_t> residues((size_t)CORE_RADIUS_SUMS * device->primes_count * device->modes);
    device->held = (cudaGetLastError() == cudaSuccess) &&
                   (cudaMemcpy(residues.data(), device->sums, residues.size() * sizeof(uint32_t), cudaMemcpyDeviceToHost) == cudaSuccess);
    if (!device->held)
    {
        return;
    }
    // 2^(2 bits + 1)
    AnchorExactInteger square;
    AnchorExactInteger scale;
    sim_rational_status_check((anchor_exact_multiply(unit, unit, &square) == ANCHOR_EXACT_OK) && sim_exact_scaled(&square, 2ull, &scale));
    std::vector<unsigned long long> digits(device->primes_count);
    for (unsigned int sum = 0u; sum < CORE_RADIUS_SUMS; sum += 1u)
    {
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
            AnchorExactInteger whole;
            sim_exact_unsigned(&whole, digits[device->primes_count - 1u]);
            for (unsigned int t = device->primes_count - 1u; t > 0u; t -= 1u)
            {
                AnchorExactInteger digit;
                AnchorExactInteger raised;
                sim_exact_unsigned(&digit, digits[t - 1u]);
                sim_rational_status_check(sim_exact_scaled(&whole, device->primes[t - 1u], &raised) && sim_exact_sum(&raised, &digit, &whole));
            }
            SimRational value;
            value.numerator = whole;
            value.denominator = scale;
            sim_rational_settle(&value);
            sums[sum].push_back(value);
        }
        while (!sums[sum].empty() && (sim_rational_sign(sums[sum].back()) == 0))
        {
            sums[sum].pop_back();
        }
    }
}

void core_radius_extend(const CoreRadiusScale *scale, SimRational s, unsigned int order, const AnchorExactInteger *unit,
                               CoreRadiusDevice *device, CoreRadiusOrders *orders)
{
    const SimRational one = core_radius_number(1ll, 1ll);
    const SimRational two = core_radius_number(2ll, 1ll);
    const SimRational eight_h = core_radius_times(core_radius_number(8ll, 1ll), scale->h);
    const SimRational angular_c = core_radius_plus(two, core_radius_times(two, scale->h));
    const SimRational along_c = core_radius_times(two, scale->a);
    const SimRational pressure_c = core_radius_times(core_radius_number(4ll, 1ll), scale->a);
    std::vector<CoreRadiusWeights> &f = orders->f;
    std::vector<CoreRadiusWeights> &g = orders->g;
    std::vector<CoreRadiusWeights> &q = orders->q;
    std::vector<CoreRadiusWeights> &inflow = orders->inflow;
    inflow.clear();
    std::vector<CoreRadiusWeights> angular_turned;
    std::vector<CoreRadiusWeights> along_turned;
    for (unsigned int k = 0u; k <= order; k += 1u)
    {
        const SimRational order_k = core_radius_number((long long)k, 1ll);
        const SimRational next = core_radius_plus(order_k, one);
        along_turned.push_back(core_radius_turned_weights(g[k], along_c, k, scale, s));
        inflow.push_back(core_radius_scaled(along_turned[k], core_radius_times(s, sim_rational_reciprocal(next))));
        angular_turned.push_back(core_radius_turned_weights(f[k], angular_c, k, scale, s));
        if (unit != NULL)
        {
            core_radius_round_up(&along_turned[k], unit);
            core_radius_round_up(&inflow[k], unit);
            core_radius_round_up(&angular_turned[k], unit);
        }
        const int on_device = (device != NULL) && (unit != NULL);
        if (on_device)
        {
            core_radius_device_put(device, CORE_RADIUS_SPIN_WEIGHTS, k, f[k], unit);
            core_radius_device_put(device, CORE_RADIUS_ALONG_WEIGHTS, k, g[k], unit);
            core_radius_device_put(device, CORE_RADIUS_INFLOW_WEIGHTS, k, inflow[k], unit);
            core_radius_device_put(device, CORE_RADIUS_ANGULAR_TURNED, k, angular_turned[k], unit);
            core_radius_device_put(device, CORE_RADIUS_ALONG_TURNED, k, along_turned[k], unit);
        }
        if (k + 1u < f.size())
        {
            continue;
        }
        const SimRational d_eight = core_radius_times(core_radius_times(eight_h, order_k), scale->d);
        CoreRadiusWeights spin = core_radius_scaled(core_radius_l(f[k], scale, s), core_radius_plus(next, scale->h));
        spin = core_radius_sum(spin, core_radius_scaled(core_radius_eta(core_radius_l(core_radius_slope(f[k]), scale, s)), scale->d));
        spin = core_radius_sum(spin, core_radius_scaled(core_radius_eta_square(f[k], scale), d_eight));
        CoreRadiusWeights along = core_radius_scaled(core_radius_l(g[k], scale, s), core_radius_plus(order_k, scale->a));
        along = core_radius_sum(along, core_radius_scaled(core_radius_eta(core_radius_l(core_radius_slope(g[k]), scale, s)), scale->d));
        along = core_radius_sum(along, core_radius_scaled(core_radius_eta_square(g[k], scale), d_eight));
        along = core_radius_sum(along, core_radius_turned_weights(q[k], pressure_c, k, scale, s));
        CoreRadiusWeights square;
        if (on_device)
        {
            CoreRadiusWeights sums[CORE_RADIUS_SUMS];
            core_radius_device_sums(device, k, unit, sums);
            spin = core_radius_sum(spin, sums[0]);
            along = core_radius_sum(along, sums[1]);
            square = sums[2];
        }
        else
        {
            for (unsigned int i = 0u; i <= k; i += 1u)
            {
                const unsigned int j = k - i;
                const SimRational rest = core_radius_number((long long)j, 1ll);
                spin = core_radius_sum(spin, core_radius_scaled(core_radius_product(inflow[i], f[j]), core_radius_plus(rest, one)));
                spin = core_radius_sum(spin, core_radius_product(g[i], angular_turned[j]));
                along = core_radius_sum(along, core_radius_scaled(core_radius_product(inflow[i], g[j]), rest));
                along = core_radius_sum(along, core_radius_product(g[i], along_turned[j]));
                square = core_radius_sum(square, core_radius_product(f[i], f[j]));
            }
        }
        f.push_back(core_radius_scaled(spin, sim_rational_reciprocal(core_radius_times(core_radius_times(two, next), core_radius_plus(next, one)))));
        g.push_back(core_radius_scaled(along, sim_rational_reciprocal(core_radius_times(two, core_radius_times(next, next)))));
        q.push_back(core_radius_scaled(core_radius_l(core_radius_l(square, scale, s), scale, s), sim_rational_reciprocal(next)));
        if (unit != NULL)
        {
            core_radius_round_up(&f.back(), unit);
            core_radius_round_up(&g.back(), unit);
            core_radius_round_up(&q.back(), unit);
        }
    }
}

int core_radius_same(const CoreRadiusWeights &left, const CoreRadiusWeights &right)
{
    const size_t length = std::max(left.size(), right.size());
    for (size_t m = 0u; m < length; m += 1u)
    {
        const SimRational one = (m < left.size()) ? left[m] : core_radius_number(0ll, 1ll);
        const SimRational other = (m < right.size()) ? right[m] : core_radius_number(0ll, 1ll);
        if (sim_rational_sign(sim_rational_difference(one, other)) != 0)
        {
            return 0;
        }
    }
    return 1;
}

void core_radius_weights(const CoreRadiusScale *scale, const CoreRadiusWeights &angular, const CoreRadiusWeights &axial,
                                const CoreRadiusWeights &pressure, unsigned int order, SimRational s, CoreRadiusOrders *orders)
{
    const int exact = sim_rational_sign(s) < 0;
    const CoreRadiusWeights data[3] = {core_radius_cut_weights(angular, scale->cut), core_radius_cut_weights(axial, scale->cut),
                                       core_radius_cut_weights(pressure, scale->cut)};
    std::vector<CoreRadiusWeights> *const fields[3] = {&orders->f, &orders->g, &orders->q};
    for (unsigned int field = 0u; field < 3u; field += 1u)
    {
        *fields[field] = {CoreRadiusWeights()};
        for (const SimRational &weight : data[field])
        {
            (*fields[field])[0].push_back(exact ? weight : sim_rational_absolute(weight));
        }
    }
    core_radius_extend(scale, s, order, NULL, NULL, orders);
}

void core_radius_continued(const CoreRadiusScale *scale, const CoreRadiusOrders *exact, unsigned int order, unsigned int last,
                                  const AnchorExactInteger *unit, CoreRadiusDevice *device, CoreRadiusOrders *continued)
{
    continued->f.clear();
    continued->g.clear();
    continued->q.clear();
    for (unsigned int k = 0u; k <= order; k += 1u)
    {
        continued->f.push_back(CoreRadiusWeights());
        continued->g.push_back(CoreRadiusWeights());
        continued->q.push_back(CoreRadiusWeights());
        for (const SimRational &weight : exact->f[k])
        {
            continued->f[k].push_back(sim_rational_absolute(weight));
        }
        for (const SimRational &weight : exact->g[k])
        {
            continued->g[k].push_back(sim_rational_absolute(weight));
        }
        for (const SimRational &weight : exact->q[k])
        {
            continued->q[k].push_back(sim_rational_absolute(weight));
        }
        // the exact orders' magnitudes rounded up as they enter the carry, every weight it reads a multiple of 1 / unit
        core_radius_round_up(&continued->f[k], unit);
        core_radius_round_up(&continued->g[k], unit);
        core_radius_round_up(&continued->q[k], unit);
    }
    core_radius_extend(scale, core_radius_number(1ll, 1ll), last, unit, device, continued);
}

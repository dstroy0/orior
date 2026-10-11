// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// core_radius_weights.h: the axis core's orders held as exact Chebyshev weights in xi = eta / a, the rule that
// carries them from order to order, and the carry of their magnitudes on the device
#ifndef CORE_RADIUS_WEIGHTS_H
#define CORE_RADIUS_WEIGHTS_H

#include "sim_rational.h"

#include <stdint.h>
#include <vector>

typedef std::vector<SimRational> CoreRadiusSequence;

SimRational core_radius_number(long long numerator, long long denominator);

SimRational core_radius_times(SimRational left, SimRational right);

SimRational core_radius_plus(SimRational left, SimRational right);

SimRational core_radius_over(SimRational left, SimRational right);

SimRational core_radius_most(SimRational left, SimRational right);

// sum |c_m| rho^m
SimRational core_radius_norm(const std::vector<SimRational> &weights, SimRational rho);

// the constants of one ellipse and one bound of d f'; the cut a, the core held on |eta| <= a in xi = eta / a
typedef struct
{
    SimRational h;
    SimRational cut;
    SimRational rho;
    SimRational delta;
    SimRational l;
    SimRational k3;
    SimRational d;
    SimRational a;
    SimRational feet;
} CoreRadiusScale;

// The weights of one order kept apart, entry m bounding |c_m| of T_m in a numerator over a power of L: no ellipse is
// carried through the orders, and one is read only where the order is bounded.
typedef std::vector<SimRational> CoreRadiusWeights;

void core_radius_add(CoreRadiusWeights *into, size_t m, SimRational value);

CoreRadiusWeights core_radius_sum(const CoreRadiusWeights &left, const CoreRadiusWeights &right);

CoreRadiusWeights core_radius_scaled(const CoreRadiusWeights &weights, SimRational factor);

// eta T_0 = T_1, eta T_m = (T_(m+1) + T_(m-1)) / 2
CoreRadiusWeights core_radius_eta(const CoreRadiusWeights &weights);

// T_2 T_m = (T_(m+2) + T_|m-2|) / 2
CoreRadiusWeights core_radius_second(const CoreRadiusWeights &weights);

// The weights are Chebyshev weights in xi = eta / a on the cut |eta| <= a, the whole segment at a = 1: eta = a xi,
// d/deta = (1 / a) d/dxi, xi^2 = (1 + T_2) / 2.

// a^2 / 2 for the scale's cut a
SimRational core_radius_half_square(const CoreRadiusScale *scale);

// L = 1 - 2 h eta^2 = (1 - h a^2) - h a^2 T_2. With s = -1 the weights are a numerator's own and every operation is
// exact; with s = +1 they are magnitudes, each minus held as a plus, and the result bounds the magnitudes of the exact
// one.
CoreRadiusWeights core_radius_l(const CoreRadiusWeights &weights, const CoreRadiusScale *scale, SimRational s);

// d = 1 - eta^2 = (1 - a^2 / 2) - (a^2 / 2) T_2, its minus held as s
CoreRadiusWeights core_radius_d(const CoreRadiusWeights &weights, const CoreRadiusScale *scale, SimRational s);

// eta^2 = (a^2 / 2) (1 + T_2)
CoreRadiusWeights core_radius_eta_square(const CoreRadiusWeights &weights, const CoreRadiusScale *scale);

// T_m' = 2 m (T_(m-1) + T_(m-3) + ...), T_0 taken once where it is reached: entry j is the sum of 2 m c_m over the
// m past j of the other parity, held as a running sum from the top down
CoreRadiusWeights core_radius_slope(const CoreRadiusWeights &weights);

// d T_m' = m (T_(m-1) - T_(m+1)) / 2: at the feet the derivative moves weight one mode each way
CoreRadiusWeights core_radius_feet_slope(const CoreRadiusWeights &weights, SimRational s);

// T_a T_b = (T_(a+b) + T_|a-b|) / 2
CoreRadiusWeights core_radius_product(const CoreRadiusWeights &left, const CoreRadiusWeights &right);

// d g_eta = (1 / a) [(1 - xi^2) g_xi + (1 - a^2) xi^2 g_xi]: the feet's part moves weight one mode each way, and the
// rest is 0 at a = 1
CoreRadiusWeights core_radius_d_slope(const CoreRadiusWeights &weights, const CoreRadiusScale *scale, SimRational s);

// sum c_m T_m(eta) as weights in xi = eta / a: T_0 = 1, T_1 = a xi and T_(m+1) = 2 a xi T_m - T_(m-1), every
// weight exact, and the weights themselves at a = 1
CoreRadiusWeights core_radius_cut_weights(const std::vector<SimRational> &weights, SimRational cut);

// the numerator of (Z_b g)_j over L^(2j+2) for g_j over L^(2j), c = -2 b > 0:
// -(c + 2 j) eta L g + L d g' + 8 h j eta d g, its first sign s, eta = a xi
CoreRadiusWeights core_radius_turned_weights(const CoreRadiusWeights &g, SimRational c, unsigned int j, const CoreRadiusScale *scale, SimRational s);

// The witness by weights: with F_k = f_k / L^(2k), U_k = u_k / L^(2k), P_k = p_k / L^(2k) and v_k = w_k / L^(2k+2),
// (L^-n)' = 4 h n eta L^(-n-1) and the system of core_rule.cu at order k times L^(2k+2) read
//     2 (k+1)(k+2) f_(k+1) = (k + 1 + h) L f_k + D eta L f_k' + 8 h k D eta^2 f_k + sum (k-i+1) w_i f_j
//                            + sum u_i [-(2 + 2h + 2j) eta L f_j + L d f_j' + 8 h j eta d f_j],
//     2 (k+1)^2 u_(k+1) = (k + A) L u_k + D eta L u_k' + 8 h k D eta^2 u_k + sum (k-i) w_i u_j
//                         + sum u_i [-(2A + 2j) eta L u_j + L d u_j' + 8 h j eta d u_j]
//                         - (4A + 2k) eta L p_k + L d p_k' + 8 h k eta d p_k,
//     (k+1) w_k = (2A + 2k) eta L u_k - L d u_k' - 8 h k eta d u_k,   (k+1) p_(k+1) = L^2 sum f_i f_j,
// j = k - i. ||L^-1||_rho <= l bounds each order on E_rho, and |F_k|_k <= ||F_k||_rho0 Delta^k, ||.||_rho rising with
// rho, gives the scalars the proof reads.
typedef struct
{
    std::vector<CoreRadiusWeights> f;
    std::vector<CoreRadiusWeights> g;
    std::vector<CoreRadiusWeights> q;
    std::vector<CoreRadiusWeights> inflow;
} CoreRadiusOrders;

// the least multiple of 1 / unit at or above `value`, for value >= 0: every operation on magnitudes is monotone, and a
// bound rounded up stays a bound, held in a length that does not grow with the order
SimRational core_radius_up(SimRational value, const AnchorExactInteger *unit);

// the largest multiple of 1 / unit at or below `value`, for value >= 0: a lower bound rounded down stays a lower bound
SimRational core_radius_down(SimRational value, const AnchorExactInteger *unit);

void core_radius_round_up(CoreRadiusWeights *weights, const AnchorExactInteger *unit);

// The carry on the device. Past the exact orders every weight the carry reads is a multiple of 2^-bits, and the
// Cauchy sums of an order are integer sums: with every weight held as the integer c 2^bits,
//     spin:   sum (k - i + 1) [w_i f_j] + sum [u_i a_j],   along: sum (k - i) [w_i u_j] + sum [u_i b_j],
//     square: sum [f_i f_j],
// j = k - i, a and b the angular and axial Z parts, and [x y]_n = sum over m + p = n and over |m - p| = n of x_m y_p,
// twice the Chebyshev product, the sum over |m - p| = 0 taken once. Each sum is 2^(2 bits + 1) times the host's. The
// device takes every sum modulo primes below 2^31, one thread to a mode, a prime and a sum, and the host reads each
// back whole by the Chinese remainder theorem, its mixed radix digits by Garner's rule: the sums are at least 0, and
// below the product of the primes where 2 widest + log2 of the terms' count and weight is below 30 times their
// number, widest the most bits of any integer put. The rest of each order, linear in its own weights, stays on the
// host, and the order is the host's to the last bit.

enum
{
    CORE_RADIUS_SPIN_WEIGHTS = 0,
    CORE_RADIUS_ALONG_WEIGHTS = 1,
    CORE_RADIUS_INFLOW_WEIGHTS = 2,
    CORE_RADIUS_ANGULAR_TURNED = 3,
    CORE_RADIUS_ALONG_TURNED = 4,
    CORE_RADIUS_KINDS = 5
};

#define CORE_RADIUS_SUMS 3u

#define CORE_RADIUS_THREADS 128u

typedef struct
{
    unsigned int primes_count;
    unsigned int orders;
    unsigned int modes;
    std::vector<uint32_t> primes;
    // inverse[t * primes_count + s] = p_s^-1 mod p_t for s < t
    std::vector<uint32_t> inverse;
    std::vector<uint32_t> lengths;
    uint32_t *residues;
    uint32_t *lengths_device;
    uint32_t *primes_device;
    uint32_t *sums;
    unsigned int widest;
    int held;
} CoreRadiusDevice;

// the device's buffers for `orders` orders of at most `modes` weights, modulo `primes_count` primes: held 0 where no
// device answers or a buffer does not open
void core_radius_device_open(CoreRadiusDevice *device, unsigned int orders, unsigned int modes, unsigned int primes_count);

void core_radius_device_close(CoreRadiusDevice *device);

// the weights of `kind` at order k, each a multiple of 1 / unit, put on the device as integers modulo every prime
void core_radius_device_put(CoreRadiusDevice *device, unsigned int kind, unsigned int k, const CoreRadiusWeights &weights, const AnchorExactInteger *unit);

// order k's three sums, spin, along and square, each over 2^(2 bits + 1), read back whole
void core_radius_device_sums(CoreRadiusDevice *device, unsigned int k, const AnchorExactInteger *unit, CoreRadiusWeights sums[CORE_RADIUS_SUMS]);

// The orders of `orders` carried to `order`: for every k <= order the inflow and the Z parts are taken from f_k and
// u_k, and every order past those `orders` holds is built from the rule. Exact at s = -1, magnitudes at s = +1: with
// s = +1 and the orders it is given at their magnitudes, each new order bounds the magnitudes of the exact one mode by
// mode. With a device and a unit the Cauchy sums are the device's, every weight they read a multiple of 1 / unit.
void core_radius_extend(const CoreRadiusScale *scale, SimRational s, unsigned int order, const AnchorExactInteger *unit,
                               CoreRadiusDevice *device, CoreRadiusOrders *orders);

// 1 where two orders' weights are the same, a 0 past either's last weight
int core_radius_same(const CoreRadiusWeights &left, const CoreRadiusWeights &right);

// sum c_m T_m(xi), the three-term rule taken from the top weight down, exact
SimRational core_radius_value(const CoreRadiusWeights &weights, SimRational xi);

// the weights of every order to `order` from the data, given in eta and held in xi, exact at s = -1 and magnitudes at
// s = +1, read on no ellipse: they depend on h and the cut alone
void core_radius_weights(const CoreRadiusScale *scale, const CoreRadiusWeights &angular, const CoreRadiusWeights &axial,
                                const CoreRadiusWeights &pressure, unsigned int order, SimRational s, CoreRadiusOrders *orders);

// the exact orders to `order` at their magnitudes, carried by magnitudes to `last`: every order a bound of the exact
// one mode by mode, each its weight kept where it holds it
void core_radius_continued(const CoreRadiusScale *scale, const CoreRadiusOrders *exact, unsigned int order, unsigned int last,
                                  const AnchorExactInteger *unit, CoreRadiusDevice *device, CoreRadiusOrders *continued);

#endif

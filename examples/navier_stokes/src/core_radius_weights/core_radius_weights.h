// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// core_radius_weights.h: the axis core's orders stored as exact Chebyshev weights in xi = eta / a, the rule that
// carries them from order to order, and the carry of their magnitudes on the device
#ifndef CORE_RADIUS_WEIGHTS_H
#define CORE_RADIUS_WEIGHTS_H

#include "sim_rational.h"

#include <stdint.h>

// A sequence of rationals in one cell of the pool: `count` of them, the cell taken with room for `room`. A sequence
// starts as zeros, the empty sequence with no cell. A call that gives a sequence takes its cell on the temporary tier.
// The cell has the room that the call computes before it fills the cell, and the caller gives it back with
// core_radius_released. A sequence
// that lasts past the call that made it is a copy on the persistent tier, which core_radius_kept makes. Where the pool
// cannot meet a cell, the sequence stays empty and core_radius_unmet gives 1.
typedef struct
{
    SimRational *value;
    unsigned int count;
    unsigned int room;
} CoreRadiusWeights;

typedef CoreRadiusWeights CoreRadiusSequence;

SimRational core_radius_number(long long numerator, long long denominator);

SimRational core_radius_times(SimRational left, SimRational right);

SimRational core_radius_plus(SimRational left, SimRational right);

SimRational core_radius_over(SimRational left, SimRational right);

SimRational core_radius_max(SimRational left, SimRational right);

// `weights` empty, with a cell of room for `room` rationals on the temporary tier, or on the persistent tier where
// `ephemeral` is 0
void core_radius_taken(CoreRadiusWeights *weights, unsigned int room, int ephemeral);

// the cell of `weights` given back, and `weights` empty
void core_radius_released(CoreRadiusWeights *weights);

// `from` copied into `to` on the persistent tier, at its own count
void core_radius_kept(const CoreRadiusWeights *from, CoreRadiusWeights *to);

// `value` put at the end of `weights`, which has room for it
void core_radius_pushed(CoreRadiusWeights *weights, SimRational value);

// the cell of `into` given back, and `made` in its place
void core_radius_replaced(CoreRadiusWeights *into, CoreRadiusWeights *made);

// 1 where the pool could not meet a cell of a sequence, or a write passed a sequence's room
int core_radius_unmet(void);

// sum |c_m| rho^m
SimRational core_radius_norm(const CoreRadiusWeights *weights, SimRational rho);

// the constants of one ellipse and one bound of d f'; the cut a, the core bounded on |eta| <= a in xi = eta / a
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

// The weights of one order retained apart, entry m bounding |c_m| of T_m in a numerator over a power of L: no ellipse
// is carried through the orders, and one is read only where the order is bounded. Each call below gives its weights in
// `out`, on the temporary tier, at a room it computes from its operands' counts.

// `value` added at entry m of `into`, the entries from its count to m first 0, m below its room
void core_radius_add(CoreRadiusWeights *into, unsigned int m, SimRational value);

// room: the larger count
void core_radius_sum(const CoreRadiusWeights *left, const CoreRadiusWeights *right, CoreRadiusWeights *out);

// room: the count
void core_radius_scaled(const CoreRadiusWeights *weights, SimRational factor, CoreRadiusWeights *out);

// eta T_0 = T_1, eta T_m = (T_(m+1) + T_(m-1)) / 2; room: the count and 1
void core_radius_eta(const CoreRadiusWeights *weights, CoreRadiusWeights *out);

// T_2 T_m = (T_(m+2) + T_|m-2|) / 2; room: the count and 2
void core_radius_second(const CoreRadiusWeights *weights, CoreRadiusWeights *out);

// The weights are Chebyshev weights in xi = eta / a on the cut |eta| <= a, the whole segment at a = 1: eta = a xi,
// d/deta = (1 / a) d/dxi, xi^2 = (1 + T_2) / 2.

// a^2 / 2 for the scale's cut a
SimRational core_radius_half_square(const CoreRadiusScale *scale);

// L = 1 - 2 h eta^2 = (1 - h a^2) - h a^2 T_2. With s = -1 the weights are a numerator's own and every operation is
// exact; with s = +1 they are magnitudes, each minus represented as a plus, and the result bounds the magnitudes of the
// exact one. Room: the count and 2
void core_radius_l(const CoreRadiusWeights *weights, const CoreRadiusScale *scale, SimRational s,
                   CoreRadiusWeights *out);

// d = 1 - eta^2 = (1 - a^2 / 2) - (a^2 / 2) T_2, its minus represented as s; room: the count and 2
void core_radius_d(const CoreRadiusWeights *weights, const CoreRadiusScale *scale, SimRational s,
                   CoreRadiusWeights *out);

// eta^2 = (a^2 / 2) (1 + T_2); room: the count and 2
void core_radius_eta_square(const CoreRadiusWeights *weights, const CoreRadiusScale *scale, CoreRadiusWeights *out);

// T_m' = 2 m (T_(m-1) + T_(m-3) + ...), T_0 taken once where it is reached: entry j is the sum of 2 m c_m over the
// m past j of the other parity, stored as a running sum from the top down; room: the count less 1
void core_radius_slope(const CoreRadiusWeights *weights, CoreRadiusWeights *out);

// d T_m' = m (T_(m-1) - T_(m+1)) / 2: at the feet the derivative moves weight one mode each way; room: the count and 1
void core_radius_feet_slope(const CoreRadiusWeights *weights, SimRational s, CoreRadiusWeights *out);

// T_a T_b = (T_(a+b) + T_|a-b|) / 2; room: the two counts less 1
void core_radius_product(const CoreRadiusWeights *left, const CoreRadiusWeights *right, CoreRadiusWeights *out);

// d g_eta = (1 / a) [(1 - xi^2) g_xi + (1 - a^2) xi^2 g_xi]: the feet's part moves weight one mode each way, and the
// rest is 0 at a = 1; room: the count and 1
void core_radius_d_slope(const CoreRadiusWeights *weights, const CoreRadiusScale *scale, SimRational s,
                         CoreRadiusWeights *out);

// sum c_m T_m(eta) as weights in xi = eta / a: T_0 = 1, T_1 = a xi and T_(m+1) = 2 a xi T_m - T_(m-1), every
// weight exact, and the weights themselves at a = 1; room: the count
void core_radius_cut_weights(const CoreRadiusWeights *weights, SimRational cut, CoreRadiusWeights *out);

// the numerator of (Z_b g)_j over L^(2j+2) for g_j over L^(2j), c = -2 b > 0:
// -(c + 2 j) eta L g + L d g' + 8 h j eta d g, its first sign s, eta = a xi; room: the count and 3
void core_radius_turned_weights(const CoreRadiusWeights *g, SimRational c, unsigned int j, const CoreRadiusScale *scale,
                                SimRational s, CoreRadiusWeights *out);

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
//
// The orders in cells of the persistent tier, each order's weights a copy there: `count` orders of f, g and q. The
// inflow has one order fewer after the carry. Each cell has room for `room` orders. The orders last the run of the
// program that reads them, and so the persistent tier holds them.
typedef struct
{
    CoreRadiusWeights *f;
    CoreRadiusWeights *g;
    CoreRadiusWeights *q;
    CoreRadiusWeights *inflow;
    unsigned int count;
    unsigned int inflow_count;
    unsigned int room;
} CoreRadiusOrders;

// `orders` empty, with room for `room` orders of each field
void core_radius_orders_taken(CoreRadiusOrders *orders, unsigned int room);

// every order's cell and the orders' cells given back, and `orders` empty
void core_radius_orders_released(CoreRadiusOrders *orders);

// the least multiple of 1 / unit at or above `value`, for value >= 0: every operation on magnitudes is monotone, and a
// bound rounded up stays a bound, stored in a length that does not grow with the order
SimRational core_radius_up(SimRational value, const OriorExactInteger *unit);

// the largest multiple of 1 / unit at or below `value`, for value >= 0: a lower bound rounded down stays a lower bound
SimRational core_radius_down(SimRational value, const OriorExactInteger *unit);

void core_radius_round_up(CoreRadiusWeights *weights, const OriorExactInteger *unit);

// The carry on the device. Past the exact orders every weight the carry reads is a multiple of 2^-bits, and the
// Cauchy sums of an order are integer sums: with every weight stored as the integer c 2^bits,
//     spin:   sum (k - i + 1) [w_i f_j] + sum [u_i a_j],   along: sum (k - i) [w_i u_j] + sum [u_i b_j],
//     square: sum [f_i f_j],
// j = k - i, a and b the angular and axial Z parts, and [x y]_n = sum over m + p = n and over |m - p| = n of x_m y_p,
// twice the Chebyshev product, the sum over |m - p| = 0 taken once. Each sum is 2^(2 bits + 1) times the host's. The
// device takes every sum modulo primes below 2^31, one thread to a mode, a prime and a sum, and the host reads each
// back whole by the Chinese remainder theorem, its mixed radix digits by Garner's rule: the sums are at least 0, and
// below the product of the primes where 2 widest + log2 of the terms' count and weight is below 30 times their
// number, widest the max bits of any integer put. The rest of each order, linear in its own weights, stays on the
// host, and the order is the host's to the last bit.

enum
{
    CORE_RADIUS_SPIN_WEIGHTS = 0,
    CORE_RADIUS_ALONG_WEIGHTS = 1,
    CORE_RADIUS_INFLOW_WEIGHTS = 2,
    CORE_RADIUS_ANGULAR_TURNED = 3,
    CORE_RADIUS_ALONG_TURNED = 4,
    CORE_RADIUS_TYPES = 5
};

#define CORE_RADIUS_SUMS 3u

#define CORE_RADIUS_THREADS 128u

// The device's carry. The primes, the inverses and the host's lengths are cells of the persistent tier. The device
// takes each cell at its count when it opens, and gives it back when it closes. The entry inverse[t * primes_count + s]
// is p_s^-1 mod p_t for s < t. The entry lengths[type * orders + k] is the count of the weights of `type` at order k
typedef struct
{
    unsigned int primes_count;
    unsigned int orders;
    unsigned int modes;
    uint32_t *primes;
    uint32_t *inverse;
    uint32_t *lengths;
    uint32_t *residues;
    uint32_t *lengths_device;
    uint32_t *primes_device;
    uint32_t *sums;
    unsigned int widest;
    int available;
} CoreRadiusDevice;

// the device's buffers for `orders` orders of at max `modes` weights, modulo `primes_count` primes: stored 0 where no
// device answers or a buffer does not open
void core_radius_device_open(CoreRadiusDevice *device, unsigned int orders, unsigned int modes, unsigned int primes_count);

void core_radius_device_close(CoreRadiusDevice *device);

// the weights of `type` at order k, each a multiple of 1 / unit, put on the device as integers modulo every prime
void core_radius_device_put(CoreRadiusDevice *device, unsigned int type, unsigned int k,
                            const CoreRadiusWeights *weights, const OriorExactInteger *unit);

// order k's three sums, spin, along and square, each over 2^(2 bits + 1). The call reads each back whole onto the
// temporary tier, and drops the zeros at its end. Each is empty where the device is not available
void core_radius_device_sums(CoreRadiusDevice *device, unsigned int k, const OriorExactInteger *unit, CoreRadiusWeights sums[CORE_RADIUS_SUMS]);

// The orders of `orders` carried to `order`, exact at s = -1 and magnitudes at s = +1. For every k <= order, the call
// takes the inflow and the Z parts from f_k and u_k. It builds every order past those that `orders` contains from the
// rule. With s = +1 and the orders at their magnitudes, each new order bounds the magnitudes of the
// exact one mode by mode. With a device and a unit the Cauchy sums are the device's, every weight they read a multiple of 1 / unit. The
// orders have room for order + 2 orders
void core_radius_extend(const CoreRadiusScale *scale, SimRational s, unsigned int order, const OriorExactInteger *unit,
                        CoreRadiusDevice *device, CoreRadiusOrders *orders);

// 1 where two orders' weights are the same, a 0 past either's last weight
int core_radius_same(const CoreRadiusWeights *left, const CoreRadiusWeights *right);

// sum c_m T_m(xi), the three-term rule taken from the top weight down, exact
SimRational core_radius_value(const CoreRadiusWeights *weights, SimRational xi);

// the weights of every order to `order` from the data, given in eta and stored in xi, exact at s = -1 and magnitudes at
// s = +1, read on no ellipse: they depend on h and the cut alone. The orders are taken here, with room for order + 2
void core_radius_weights(const CoreRadiusScale *scale, const CoreRadiusWeights *angular, const CoreRadiusWeights *axial,
                         const CoreRadiusWeights *pressure, unsigned int order, SimRational s,
                         CoreRadiusOrders *orders);

// the exact orders to `order` at their magnitudes, carried by magnitudes to `last`: every order a bound of the exact
// one mode by mode, each its weight retained where it contains it. The orders are taken here, with room for last + 2
void core_radius_continued(const CoreRadiusScale *scale, const CoreRadiusOrders *exact, unsigned int order,
                           unsigned int last, const OriorExactInteger *unit, CoreRadiusDevice *device,
                           CoreRadiusOrders *continued);

#endif

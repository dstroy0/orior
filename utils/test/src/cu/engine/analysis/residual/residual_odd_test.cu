// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// The residual's odd orders (ruling (a)): an odd smooth order is held and an odd background order errored. The key's
// window for an order o on an axis starts floor((o + 1) / 2) before the voxel, over the input reflected at its edges.
// With the smooth order s odd, the narrow term's window starts (s + 1) / 2 before and the wide term's (s + b + 1) / 2;
// with b even the two stay centerd on one point, half a voxel before the voxel of the lane's index, and the residual
// sits there with them. An odd b would center them half a voxel apart. Every lane of the key's residual and of the
// unit sweeps' is checked against the residual counted here from that definition, on odd, even and mixed orders over
// extents that include a single voxel and an axis one voxel long, and an odd background order errors at each of the
// three sites as a request error from its own module. The comb of n on an axis is a running sum of n voxels taken on
// both terms before the smooth: its row is the binomial's times n ones, its window starts floor(T / 2) before the voxel
// for a row of T taps, and an even n moves both terms half a voxel as an odd smooth order does. The sets with combs
// hold odd and even n, n of 0 and 1, and n past the extent. A spaced pair at 2^j is the row times 1 + 2 z^(2^j) +
// z^(2^(j + 1)); it widens the window by 2^(j + 1) taps, moves no center, and adds 2 bits to its term and, on the
// background, to the gain. The sets with spaced pairs hold pairs on the smooth and on the background, more than one at
// a spacing, and spacings past the extent. The test is one job on the device's tessera daemon, submitted before its
// first device work.
#include "../../../../../../../src/cu/engine/analysis/cycle/cycle.h"
#include "../../../../../../../src/cu/engine/analysis/key_schedule/key_schedule.h"
#include "../../../../../../../src/cu/engine/analysis/keymath/keymath.h"
#include "../../../../../../../src/cu/engine/analysis/residual/residual.h"
#include "sim.h"
#include "../../../../../../../src/cu/engine/analysis/unit_sweep/unit_sweep.h"

#include <cuda_runtime.h>

#include <stdlib.h>
#include <string.h>

#define ODD_TEST_EXTENTS 5u

#define ODD_TEST_ORDER_SETS 15u

// the spacings the sets' spaced pairs take, 2^j for j below this
#define ODD_TEST_SPACINGS 5u

#define ODD_TEST_TAPS_MAX 64u

#define ODD_TEST_VOXELS_MAX (5u * 9u * 11u)

#define ODD_TEST_KINDS 4ull

// the most the test puts on the device at once: the volume, the key's lanes, the sweep's lanes, the sweep's narrow and
// wide planes and the key's scratch, each at most ENGINE_RESIDUAL_LIMBS a voxel
#define ODD_TEST_DECLARED                                                                                              \
    ((unsigned long long)ODD_TEST_VOXELS_MAX * ((6ull * ENGINE_RESIDUAL_LIMBS) + 1ull) * sizeof(unsigned int))

static const unsigned int ODD_TEST_EXTENT[ODD_TEST_EXTENTS][ENGINE_AXES] = {
    {1u, 1u, 1u}, {3u, 1u, 7u}, {5u, 9u, 11u}, {4u, 6u, 5u}, {2u, 8u, 3u}};

static const unsigned int ODD_TEST_SMOOTH[ODD_TEST_ORDER_SETS][ENGINE_AXES] = {
    {1u, 1u, 1u}, {3u, 0u, 5u}, {5u, 3u, 1u}, {0u, 1u, 0u}, {1u, 2u, 3u}, {2u, 4u, 4u}, {0u, 0u, 0u}, {1u, 0u, 1u},
    {0u, 1u, 0u}, {2u, 2u, 2u}, {0u, 0u, 0u}, {1u, 0u, 2u}, {0u, 0u, 0u}, {1u, 1u, 1u}, {0u, 2u, 1u}};

static const unsigned int ODD_TEST_BACKGROUND[ODD_TEST_ORDER_SETS][ENGINE_AXES] = {
    {2u, 2u, 2u}, {4u, 2u, 0u}, {6u, 6u, 2u}, {0u, 2u, 0u}, {2u, 0u, 4u}, {6u, 6u, 6u}, {2u, 2u, 2u}, {2u, 2u, 2u},
    {0u, 2u, 4u}, {4u, 4u, 4u}, {2u, 0u, 2u}, {2u, 2u, 0u}, {0u, 0u, 0u}, {2u, 2u, 2u}, {0u, 0u, 2u}};

static const unsigned int ODD_TEST_COMB[ODD_TEST_ORDER_SETS][ENGINE_AXES] = {
    {0u, 0u, 0u}, {0u, 0u, 0u}, {0u, 0u, 0u}, {0u, 0u, 0u}, {0u, 0u, 0u}, {0u, 0u, 0u}, {2u, 2u, 2u}, {4u, 4u, 2u},
    {3u, 4u, 5u}, {6u, 7u, 8u}, {1u, 0u, 9u}, {0u, 0u, 0u}, {0u, 0u, 3u}, {2u, 4u, 5u}, {0u, 0u, 0u}};

// the spaced pairs on each axis at spacings 1, 2, 4, 8 and 16; the sets before the last four take none
static const unsigned int ODD_TEST_SMOOTH_SPACED[ODD_TEST_ORDER_SETS][ENGINE_AXES][ODD_TEST_SPACINGS] = {
    {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
    {{0u, 0u, 0u, 0u, 0u}, {0u, 0u, 0u, 0u, 0u}, {0u, 1u, 0u, 0u, 0u}},
    {{0u, 0u, 0u, 0u, 0u}, {0u, 0u, 0u, 0u, 0u}, {0u, 0u, 1u, 0u, 0u}},
    {{0u, 0u, 0u, 0u, 0u}, {0u, 1u, 0u, 0u, 0u}, {0u, 0u, 0u, 0u, 0u}},
    {{0u, 0u, 1u, 0u, 0u}, {1u, 0u, 0u, 0u, 0u}, {0u, 0u, 0u, 0u, 0u}}};

static const unsigned int ODD_TEST_BACKGROUND_SPACED[ODD_TEST_ORDER_SETS][ENGINE_AXES][ODD_TEST_SPACINGS] = {
    {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
    {{0u, 1u, 0u, 0u, 0u}, {0u, 0u, 1u, 0u, 0u}, {0u, 0u, 0u, 0u, 0u}},
    {{0u, 1u, 0u, 0u, 0u}, {0u, 0u, 0u, 0u, 0u}, {0u, 2u, 0u, 0u, 0u}},
    {{0u, 0u, 0u, 0u, 0u}, {0u, 0u, 0u, 0u, 0u}, {0u, 0u, 1u, 0u, 0u}},
    {{0u, 0u, 0u, 0u, 0u}, {0u, 0u, 0u, 1u, 0u}, {0u, 0u, 0u, 0u, 1u}}};

typedef struct
{
    const unsigned int *smooth;
    const unsigned int *background;
    const unsigned int *comb;
    const unsigned int (*smooth_spaced)[ODD_TEST_SPACINGS];
    const unsigned int (*background_spaced)[ODD_TEST_SPACINGS];
} OddTestOrders;

static OddTestOrders odd_test_orders(unsigned int set)
{
    const OddTestOrders orders = {ODD_TEST_SMOOTH[set], ODD_TEST_BACKGROUND[set], ODD_TEST_COMB[set],
                                  ODD_TEST_SMOOTH_SPACED[set], ODD_TEST_BACKGROUND_SPACED[set]};
    return orders;
}

static const unsigned int ODD_TEST_NONE_SPACED[ENGINE_AXES][ODD_TEST_SPACINGS] = {};

typedef struct
{
    unsigned short *volume;
    unsigned int *key_out;
    unsigned int *sweep_out;
} OddTestDevice;

typedef struct
{
    unsigned short volume[ODD_TEST_VOXELS_MAX];
    long long expected[ODD_TEST_VOXELS_MAX];
    unsigned long long along[ODD_TEST_VOXELS_MAX];
    unsigned long long across[ODD_TEST_VOXELS_MAX];
    unsigned int lanes[ODD_TEST_VOXELS_MAX * ENGINE_RESIDUAL_LIMBS];
} OddTestHost;

typedef struct
{
    unsigned long long lanes;
    unsigned long long nonzero;
    unsigned long long key_differ;
    unsigned long long sweep_differ;
} OddTestResults;

// the input position a tap reads, reflected at the edges as the key's fold table reflects it
static unsigned int odd_test_reflect(long long position, long long length)
{
    const long long period = 2ll * length;
    long long folded = position % period;
    folded += (folded < 0ll) ? period : 0ll;
    folded = (folded >= length) ? (period - 1ll - folded) : folded;
    // folded lies in [0, length), and length is an extent of at most 11
    return (unsigned int)folded;
}

static unsigned long long odd_test_binomial(unsigned int order, unsigned int tap)
{
    unsigned long long value = 1ull;
    for (unsigned int step = 0u; step < tap; step += 1u)
    {
        value = (value * (unsigned long long)(order - step)) / (unsigned long long)(step + 1u);
    }
    return value;
}

// one axis's row: the binomial of the order times n ones for a comb of n, times each spaced pair, T taps, T at most
// ODD_TEST_TAPS_MAX
static unsigned int odd_test_row(unsigned int order, unsigned int comb, const unsigned int spaced[ODD_TEST_SPACINGS],
                                 unsigned long long row[ODD_TEST_TAPS_MAX])
{
    const unsigned int ones = (comb < 2u) ? 1u : comb;
    unsigned int taps = order + ones;
    for (unsigned int tap = 0u; tap < taps; tap += 1u)
    {
        row[tap] = 0ull;
        for (unsigned int one = 0u; one < ones; one += 1u)
        {
            row[tap] += ((one <= tap) && ((tap - one) <= order)) ? odd_test_binomial(order, tap - one) : 0ull;
        }
    }
    for (unsigned int spacing = 0u; spacing < ODD_TEST_SPACINGS; spacing += 1u)
    {
        const unsigned int apart = 1u << spacing;
        for (unsigned int pair = 0u; pair < spaced[spacing]; pair += 1u)
        {
            unsigned long long paired[ODD_TEST_TAPS_MAX];
            const unsigned int wider = taps + (2u * apart);
            for (unsigned int tap = 0u; tap < wider; tap += 1u)
            {
                paired[tap] = ((tap < taps) ? row[tap] : 0ull) +
                              (((tap >= apart) && ((tap - apart) < taps)) ? (2ull * row[tap - apart]) : 0ull) +
                              (((tap >= (2u * apart)) && ((tap - (2u * apart)) < taps)) ? row[tap - (2u * apart)]
                                                                                         : 0ull);
            }
            memcpy(row, paired, (size_t)wider * sizeof(unsigned long long));
            taps = wider;
        }
    }
    return taps;
}

// one axis of one term: each voxel's weighted sum over the row's window along the axis, the window of T taps
// starting floor(T / 2) before the voxel
static void odd_test_axis(const unsigned int *extent, unsigned int axis, unsigned int order, unsigned int comb,
                          const unsigned int spaced[ODD_TEST_SPACINGS], const unsigned long long *from,
                          unsigned long long *to)
{
    const unsigned int voxels = extent[0] * extent[1] * extent[2];
    const unsigned int stride = (axis == 0u) ? (extent[1] * extent[2]) : ((axis == 1u) ? extent[2] : 1u);
    unsigned long long row[ODD_TEST_TAPS_MAX];
    const unsigned int taps = odd_test_row(order, comb, spaced, row);
    const long long start = (long long)(taps / 2u);
    for (unsigned int voxel = 0u; voxel < voxels; voxel += 1u)
    {
        const unsigned int coordinate = (voxel / stride) % extent[axis];
        const unsigned int line = voxel - (coordinate * stride);
        unsigned long long sum = 0ull;
        for (unsigned int tap = 0u; tap < taps; tap += 1u)
        {
            const unsigned int read =
                odd_test_reflect((long long)coordinate - start + (long long)tap, (long long)extent[axis]);
            sum += row[tap] * from[line + (read * stride)];
        }
        to[voxel] = sum;
    }
}

// one term, B_orders times the combs and the spaced pairs applied to the volume axis by axis, into `along`
static void odd_test_term(const unsigned int *extent, const unsigned int *orders, const unsigned int *comb,
                          const unsigned int (*spaced)[ODD_TEST_SPACINGS], OddTestHost *host)
{
    const unsigned int voxels = extent[0] * extent[1] * extent[2];
    for (unsigned int voxel = 0u; voxel < voxels; voxel += 1u)
    {
        host->along[voxel] = (unsigned long long)host->volume[voxel];
    }
    for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
    {
        odd_test_axis(extent, axis, orders[axis], comb[axis], spaced[axis], host->along, host->across);
        memcpy(host->along, host->across, (size_t)voxels * sizeof(unsigned long long));
    }
}

// the residual from its definition: the narrow term times 2^gain less the wide term
static void odd_test_expect(const unsigned int *extent, const OddTestOrders *orders, OddTestHost *host)
{
    const unsigned int voxels = extent[0] * extent[1] * extent[2];
    unsigned int wide[ENGINE_AXES];
    unsigned int wide_spaced[ENGINE_AXES][ODD_TEST_SPACINGS];
    unsigned int gain = 0u;
    for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
    {
        wide[axis] = orders->smooth[axis] + orders->background[axis];
        gain += orders->background[axis];
        for (unsigned int spacing = 0u; spacing < ODD_TEST_SPACINGS; spacing += 1u)
        {
            wide_spaced[axis][spacing] =
                orders->smooth_spaced[axis][spacing] + orders->background_spaced[axis][spacing];
            gain += 2u * orders->background_spaced[axis][spacing];
        }
    }
    odd_test_term(extent, orders->smooth, orders->comb, orders->smooth_spaced, host);
    for (unsigned int voxel = 0u; voxel < voxels; voxel += 1u)
    {
        // the narrow term is below 2^(16 + 15 + 10), a comb of n adding bit_length(n - 1) and a spaced pair 2, and the
        // gain at most 18: the shifted term is below 2^59
        host->expected[voxel] = (long long)(host->along[voxel] << gain);
    }
    odd_test_term(extent, wide, orders->comb, wide_spaced, host);
    for (unsigned int voxel = 0u; voxel < voxels; voxel += 1u)
    {
        // the wide term is below 2^(16 + 33 + 10), inside a signed word
        host->expected[voxel] -= (long long)host->along[voxel];
    }
}

// the lanes read back against the expected residual in ENGINE_RESIDUAL_LIMBS limbs of two's complement
static unsigned long long odd_test_differ(const OddTestHost *host, unsigned int voxels)
{
    unsigned long long differ = 0ull;
    for (unsigned int voxel = 0u; voxel < voxels; voxel += 1u)
    {
        // the expected value's 64-bit two's complement
        const unsigned long long bits = (unsigned long long)host->expected[voxel];
        const unsigned int *const limbs = &host->lanes[(size_t)voxel * ENGINE_RESIDUAL_LIMBS];
        const unsigned int extension = (host->expected[voxel] < 0ll) ? 0xFFFFFFFFu : 0u;
        // the low and high halves of the 64-bit pattern
        int same = (limbs[0] == (unsigned int)(bits & 0xFFFFFFFFull)) && (limbs[1] == (unsigned int)(bits >> 32u));
        for (unsigned int limb = 2u; limb < ENGINE_RESIDUAL_LIMBS; limb += 1u)
        {
            same = same && (limbs[limb] == extension);
        }
        differ += (same != 0) ? 0ull : 1ull;
    }
    return differ;
}

static int odd_test_read(const unsigned int *device_lanes, unsigned int voxels, OddTestHost *host)
{
    return (cudaDeviceSynchronize() == cudaSuccess) &&
           (cudaMemcpy(host->lanes, device_lanes, (size_t)voxels * ENGINE_RESIDUAL_LIMBS * sizeof(unsigned int),
                       cudaMemcpyDeviceToHost) == cudaSuccess);
}

// a set's spaced pairs into a request's, which holds ENGINE_SPACINGS spacings to the set's ODD_TEST_SPACINGS
static void odd_test_spaced(const unsigned int (*from)[ODD_TEST_SPACINGS],
                            unsigned int to[ENGINE_AXES][ENGINE_SPACINGS])
{
    memset(to, 0, ENGINE_AXES * ENGINE_SPACINGS * sizeof(unsigned int));
    for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
    {
        memcpy(to[axis], from[axis], ODD_TEST_SPACINGS * sizeof(unsigned int));
    }
}

// the key's residual: the program residual_program writes, encoded, laid out and run on the one atom
static int odd_test_key(const unsigned int *extent, const OddTestOrders *orders, const OddTestDevice *device,
                        OddTestHost *host)
{
    EngineError error;
    memset(&error, 0, sizeof(error));
    EngineResidualRequest residual;
    memset(&residual, 0, sizeof(residual));
    memcpy(residual.smooth_orders, orders->smooth, sizeof(residual.smooth_orders));
    memcpy(residual.background_orders, orders->background, sizeof(residual.background_orders));
    memcpy(residual.comb, orders->comb, sizeof(residual.comb));
    odd_test_spaced(orders->smooth_spaced, residual.smooth_spaced);
    odd_test_spaced(orders->background_spaced, residual.background_spaced);
    residual.error = &error;
    EngineStep program[RESIDUAL_STEPS];
    EngineKey math;
    memset(&math, 0, sizeof(math));
    const KeymathEncodeRequest encode_request = {program, RESIDUAL_STEPS, &math, &error};
    int ok = (residual_program(&residual, program) == (long)RESIDUAL_STEPS) &&
             (keymath_encode(&encode_request) != KEYMATH_ERROR);
    EngineKeyLayout layout;
    memset(&layout, 0, sizeof(layout));
    ok = ok && (key_schedule_layout(&math, &layout, &error) != KEY_SCHEDULE_ERROR);
    keymath_key_release(&math);
    CycleKey *key = NULL;
    ok = ok && (cycle_key_load(&layout, &key, &error) != CYCLE_ERROR);
    key_schedule_release(&layout);
    Atom atom;
    memset(&atom, 0, sizeof(atom));
    atom.lanes = device->volume;
    atom.depth = extent[0];
    atom.height = extent[1];
    atom.width = extent[2];
    const CycleRunRequest run = {key, &atom, 1ull, ENGINE_RESIDUAL_LIMBS, device->key_out, &error};
    ok = ok && (cycle_run(&run) == 1L) && odd_test_read(device->key_out, extent[0] * extent[1] * extent[2], host);
    cycle_key_release(key);
    return ok;
}

static int odd_test_sweep(const unsigned int *extent, const OddTestOrders *orders, const OddTestDevice *device,
                          OddTestHost *host)
{
    EngineError error;
    memset(&error, 0, sizeof(error));
    UnitSweepRequest request;
    memset(&request, 0, sizeof(request));
    request.device_volume = device->volume;
    request.depth = extent[0];
    request.height = extent[1];
    request.width = extent[2];
    memcpy(request.smooth_orders, orders->smooth, sizeof(request.smooth_orders));
    memcpy(request.background_orders, orders->background, sizeof(request.background_orders));
    memcpy(request.comb, orders->comb, sizeof(request.comb));
    odd_test_spaced(orders->smooth_spaced, request.smooth_spaced);
    odd_test_spaced(orders->background_spaced, request.background_spaced);
    request.limbs = ENGINE_RESIDUAL_LIMBS;
    request.device_out = device->sweep_out;
    request.error = &error;
    return (unit_sweep_residual(&request) == 0L) &&
           odd_test_read(device->sweep_out, extent[0] * extent[1] * extent[2], host);
}

static void odd_test_case(SimResults *results, const unsigned int *extent, unsigned int orders, unsigned long long key,
                          const OddTestDevice *device, OddTestHost *host, OddTestResults *counts)
{
    const unsigned int voxels = extent[0] * extent[1] * extent[2];
    const OddTestOrders set = odd_test_orders(orders);
    for (unsigned int voxel = 0u; voxel < voxels; voxel += 1u)
    {
        const unsigned long long draw = sim_draw(key, voxel);
        // a remainder below the kinds' count fits unsigned int
        const unsigned int kind = (unsigned int)(draw % ODD_TEST_KINDS);
        // the draw's bits past the kind, masked to 16, fit unsigned int
        const unsigned int value =
            (kind == 0u) ? 0u : ((kind == 1u) ? 0xFFFFu : (unsigned int)((draw >> 8u) & 0xFFFFull));
        // a value of at most 16 bits narrows to unsigned short exactly
        host->volume[voxel] = (unsigned short)value;
    }
    odd_test_expect(extent, &set, host);
    for (unsigned int voxel = 0u; voxel < voxels; voxel += 1u)
    {
        counts->nonzero += (host->expected[voxel] != 0ll) ? 1ull : 0ull;
    }
    counts->lanes += voxels;
    const int placed = (cudaMemcpy(device->volume, host->volume, (size_t)voxels * sizeof(unsigned short),
                                   cudaMemcpyHostToDevice) == cudaSuccess);
    const int keyed = placed && odd_test_key(extent, &set, device, host);
    sim_check(results, keyed, "the key is encoded from the residual's program and run");
    const unsigned long long key_differ = keyed ? odd_test_differ(host, voxels) : voxels;
    sim_check(results, key_differ == 0ull, "every lane of the key's residual is the residual's definition");
    const int swept = placed && odd_test_sweep(extent, &set, device, host);
    sim_check(results, swept, "the unit sweeps run on the same orders");
    const unsigned long long sweep_differ = swept ? odd_test_differ(host, voxels) : voxels;
    sim_check(results, sweep_differ == 0ull, "every lane of the unit sweeps' residual is the residual's definition");
    counts->key_differ += key_differ;
    counts->sweep_differ += sweep_differ;
}

static void odd_test_report(SimResults *results, unsigned int orders, const OddTestResults *counts)
{
    scriptura_text(&results->line, "  smooth ");
    for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
    {
        scriptura_character(&results->line, (axis == 0u) ? '{' : ',');
        scriptura_decimal(&results->line, ODD_TEST_SMOOTH[orders][axis], 1u);
    }
    scriptura_text(&results->line, "} background ");
    for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
    {
        scriptura_character(&results->line, (axis == 0u) ? '{' : ',');
        scriptura_decimal(&results->line, ODD_TEST_BACKGROUND[orders][axis], 1u);
    }
    scriptura_text(&results->line, "} comb ");
    for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
    {
        scriptura_character(&results->line, (axis == 0u) ? '{' : ',');
        scriptura_decimal(&results->line, ODD_TEST_COMB[orders][axis], 1u);
    }
    scriptura_character(&results->line, '}');
    // a spaced pair is written as axis:spacing, s for the smooth's and b for the background's
    for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
    {
        for (unsigned int spacing = 0u; spacing < ODD_TEST_SPACINGS; spacing += 1u)
        {
            for (unsigned int pair = 0u; pair < ODD_TEST_SMOOTH_SPACED[orders][axis][spacing]; pair += 1u)
            {
                scriptura_text(&results->line, " s");
                scriptura_decimal(&results->line, axis, 1u);
                scriptura_character(&results->line, ':');
                scriptura_decimal(&results->line, 1ull << spacing, 1u);
            }
            for (unsigned int pair = 0u; pair < ODD_TEST_BACKGROUND_SPACED[orders][axis][spacing]; pair += 1u)
            {
                scriptura_text(&results->line, " b");
                scriptura_decimal(&results->line, axis, 1u);
                scriptura_character(&results->line, ':');
                scriptura_decimal(&results->line, 1ull << spacing, 1u);
            }
        }
    }
    scriptura_text(&results->line, ": ");
    scriptura_decimal(&results->line, counts->lanes, 1u);
    scriptura_text(&results->line, " lanes over the extents, ");
    scriptura_decimal(&results->line, counts->nonzero, 1u);
    scriptura_text(&results->line, " of them not zero; the key differs on ");
    scriptura_decimal(&results->line, counts->key_differ, 1u);
    scriptura_text(&results->line, ", the unit sweeps on ");
    scriptura_decimal(&results->line, counts->sweep_differ, 1u);
    scriptura_character(&results->line, '\n');
    sim_flush(results);
}

// the lanes read back that are zero in every limb, over the voxels whose window of `taps` along the line lies inside it
static unsigned long long odd_test_inside_nonzero(const OddTestHost *host, unsigned int length, unsigned int taps)
{
    unsigned long long nonzero = 0ull;
    const unsigned int start = taps / 2u;
    for (unsigned int voxel = start; (voxel + taps) <= (length + start); voxel += 1u)
    {
        unsigned int bits = 0u;
        for (unsigned int limb = 0u; limb < ENGINE_RESIDUAL_LIMBS; limb += 1u)
        {
            bits |= host->lanes[((size_t)voxel * ENGINE_RESIDUAL_LIMBS) + limb];
        }
        nonzero += (bits != 0u) ? 1ull : 0ull;
    }
    return nonzero;
}

// a period of n planted along x: the comb of n sends it to a constant, and every lane whose window lies inside the line
// is zero on the key and on the unit sweeps; without the comb those lanes are not all zero
static void odd_test_period(SimResults *results, const OddTestDevice *device, OddTestHost *host)
{
    static const unsigned int PERIOD_TEST_VALUES[5] = {0u, 9000u, 300u, 65535u, 4242u};
    const unsigned int extent[ENGINE_AXES] = {1u, 1u, 41u};
    const unsigned int smooth[ENGINE_AXES] = {0u, 0u, 0u};
    const unsigned int background[ENGINE_AXES] = {0u, 0u, 4u};
    for (unsigned int period = 4u; period <= 5u; period += 1u)
    {
        for (unsigned int voxel = 0u; voxel < extent[2]; voxel += 1u)
        {
            // a value of at most 16 bits narrows to unsigned short exactly
            host->volume[voxel] = (unsigned short)PERIOD_TEST_VALUES[voxel % period];
        }
        const int placed = (cudaMemcpy(device->volume, host->volume, (size_t)extent[2] * sizeof(unsigned short),
                                       cudaMemcpyHostToDevice) == cudaSuccess);
        const unsigned int combed[ENGINE_AXES] = {0u, 0u, period};
        const unsigned int bare[ENGINE_AXES] = {0u, 0u, 0u};
        const unsigned int taps = period + background[2];
        const OddTestOrders with_comb = {smooth, background, combed, ODD_TEST_NONE_SPACED, ODD_TEST_NONE_SPACED};
        const OddTestOrders without = {smooth, background, bare, ODD_TEST_NONE_SPACED, ODD_TEST_NONE_SPACED};
        const int key_combed = placed && odd_test_key(extent, &with_comb, device, host);
        const unsigned long long key_left = key_combed ? odd_test_inside_nonzero(host, extent[2], taps) : 1ull;
        const int sweep_combed = placed && odd_test_sweep(extent, &with_comb, device, host);
        const unsigned long long sweep_left = sweep_combed ? odd_test_inside_nonzero(host, extent[2], taps) : 1ull;
        const int key_bare = placed && odd_test_key(extent, &without, device, host);
        const unsigned long long bare_left =
            key_bare ? odd_test_inside_nonzero(host, extent[2], background[2] + 1u) : 0ull;
        sim_check(results, (key_left == 0ull) && (sweep_left == 0ull),
                  "a comb of n sends a period of n to zero on every lane inside the line, key and unit sweeps alike");
        sim_check(results, bare_left != 0ull, "without the comb the same lanes are not all zero");
        scriptura_text(&results->line, "  a period of ");
        scriptura_decimal(&results->line, period, 1u);
        scriptura_text(&results->line, " along 41 voxels: with its comb, ");
        scriptura_decimal(&results->line, key_left, 1u);
        scriptura_text(&results->line, " lanes inside are not zero on the key and ");
        scriptura_decimal(&results->line, sweep_left, 1u);
        scriptura_text(&results->line, " on the unit sweeps; without it, ");
        scriptura_decimal(&results->line, bare_left, 1u);
        scriptura_character(&results->line, '\n');
        sim_flush(results);
    }
}

// an odd background order at each of the three sites: residual_program, the key's encoding and the unit sweeps
static void odd_test_errors(SimResults *results, const OddTestDevice *device)
{
    const unsigned int smooth[ENGINE_AXES] = {1u, 2u, 3u};
    const unsigned int background[ENGINE_AXES] = {2u, 3u, 2u};
    const unsigned int comb[ENGINE_AXES] = {0u, 4u, 0u};
    EngineError error;
    memset(&error, 0, sizeof(error));
    EngineResidualRequest residual;
    memset(&residual, 0, sizeof(residual));
    memcpy(residual.smooth_orders, smooth, sizeof(residual.smooth_orders));
    memcpy(residual.background_orders, background, sizeof(residual.background_orders));
    memcpy(residual.comb, comb, sizeof(residual.comb));
    residual.error = &error;
    EngineStep program[RESIDUAL_STEPS];
    sim_check(results,
              (residual_program(&residual, program) == RESIDUAL_ERROR) && (error.kind == ENGINE_ERROR_REQUEST) &&
                  (error.module == ENGINE_MODULE_RESIDUAL),
              "an odd background order errors on the residual's program, a request error from residual");
    memset(program, 0, sizeof(program));
    program[0].operation = ENGINE_COMB;
    memcpy(program[0].orders, comb, sizeof(program[0].orders));
    program[1].operation = ENGINE_SMOOTH;
    memcpy(program[1].orders, smooth, sizeof(program[1].orders));
    program[2].operation = ENGINE_KEEP;
    program[3].operation = ENGINE_SMOOTH;
    memcpy(program[3].orders, background, sizeof(program[3].orders));
    program[4].operation = ENGINE_SCALE_SUBTRACT;
    program[4].shift = background[0] + background[1] + background[2];
    memset(&error, 0, sizeof(error));
    EngineKey math;
    memset(&math, 0, sizeof(math));
    // the five steps written here: the comb, the smooth, the keep, the background's smooth and the subtraction
    const KeymathEncodeRequest encode_request = {program, 5u, &math, &error};
    sim_check(results,
              (keymath_encode(&encode_request) == KEYMATH_ERROR) && (error.kind == ENGINE_ERROR_REQUEST) &&
                  (error.module == ENGINE_MODULE_KEYMATH),
              "a subtraction of terms half a voxel apart errors on the encoding, a request error from keymath");
    memset(&error, 0, sizeof(error));
    UnitSweepRequest request;
    memset(&request, 0, sizeof(request));
    request.device_volume = device->volume;
    request.depth = ODD_TEST_EXTENT[3][0];
    request.height = ODD_TEST_EXTENT[3][1];
    request.width = ODD_TEST_EXTENT[3][2];
    memcpy(request.smooth_orders, smooth, sizeof(request.smooth_orders));
    memcpy(request.background_orders, background, sizeof(request.background_orders));
    memcpy(request.comb, comb, sizeof(request.comb));
    request.limbs = ENGINE_RESIDUAL_LIMBS;
    request.device_out = device->sweep_out;
    request.error = &error;
    sim_check(results,
              (unit_sweep_residual(&request) == UNIT_SWEEP_ERROR) && (error.kind == ENGINE_ERROR_REQUEST) &&
                  (error.module == ENGINE_MODULE_UNIT_SWEEP),
              "an odd background order errors on the unit sweeps, a request error from unit_sweep");
}

int main(int count, char **arguments)
{
    char line_buffer[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, line_buffer);
    const int admitted = sim_job_submit(&results, "residual_odd_test", count, arguments, ODD_TEST_DECLARED);
    OddTestHost *const host = (OddTestHost *)malloc(sizeof(OddTestHost));
    OddTestDevice device;
    memset(&device, 0, sizeof(device));
    const size_t lane_words = (size_t)ODD_TEST_VOXELS_MAX * ENGINE_RESIDUAL_LIMBS;
    const int passed =
        (admitted != 0) && (host != NULL) &&
        (cudaMalloc((void **)&device.volume, (size_t)ODD_TEST_VOXELS_MAX * sizeof(unsigned short)) == cudaSuccess) &&
        (cudaMalloc((void **)&device.key_out, lane_words * sizeof(unsigned int)) == cudaSuccess) &&
        (cudaMalloc((void **)&device.sweep_out, lane_words * sizeof(unsigned int)) == cudaSuccess);
    if (admitted != 0)
    {
        sim_check(&results, passed, "the test's buffers are held on the host and the device");
    }
    for (unsigned int orders = 0u; passed && (orders < ODD_TEST_ORDER_SETS); orders += 1u)
    {
        OddTestResults counts;
        memset(&counts, 0, sizeof(counts));
        for (unsigned int extent = 0u; extent < ODD_TEST_EXTENTS; extent += 1u)
        {
            const unsigned long long key = 0x0DD5ull + ((unsigned long long)orders * ODD_TEST_EXTENTS) + extent;
            odd_test_case(&results, ODD_TEST_EXTENT[extent], orders, key, &device, host, &counts);
        }
        sim_check(&results, counts.nonzero != 0ull, "the residuals compared are not all zero");
        odd_test_report(&results, orders, &counts);
    }
    if (passed)
    {
        odd_test_period(&results, &device, host);
        odd_test_errors(&results, &device);
    }
    if (admitted != 0)
    {
        cudaFree(device.volume);
        cudaFree(device.key_out);
        cudaFree(device.sweep_out);
        unit_sweep_release();
    }
    free(host);
    return sim_close(&results, "residual odd test");
}

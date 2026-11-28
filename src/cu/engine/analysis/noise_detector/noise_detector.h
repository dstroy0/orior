// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef NOISE_DETECTOR_H
#define NOISE_DETECTOR_H

#include "../../engine_config.h"
#include "../../../../../includes/exact_types/integers/exact_integer.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define NOISE_DETECTOR_ERROR (-1L)

// the structure function's lags, frames apart: 1, 2, 4 and on to 64 (noise_vector_integration_table.md, row 7)
#define NOISE_FLICKER_LAGS 7u

// a pair's level bin is the sum of its two values shifted down 4, its mean in bins of 8 lane units. The last bin
// contains every mean from 1024 up
#define NOISE_LEVEL_BIN_SHIFT 4u

#define NOISE_LEVEL_BINS 129u

// the four sums each lag and level bin contains
#define NOISE_FLICKER_PAIRS 0u

#define NOISE_FLICKER_SQUARES 1u

#define NOISE_FLICKER_NEIGHBOR_PAIRS 2u

#define NOISE_FLICKER_NEIGHBOR_SQUARES 3u

#define NOISE_FLICKER_SUMS 4u

    // Loads one sample of a set: its extent (frames, depth, height, width) and its volume. The volume is frame by frame,
    // with z, y and x inside each frame. The caller frees the volume. Returns 0, or a negative value with the error filled. The driver
    // passes the crystal loader of the engine, and the detector reaches no module but its own.
    typedef long (*NoiseLoad)(const char *set, const char *sample, unsigned long long extent[4],
                              unsigned short **volume, EngineError *error);

    typedef struct
    {
        const char *set;
        char *const *samples;
        unsigned int count;
        EngineError *error;
        NoiseLoad load;
    } NoiseSetRequest;

    // every sample's structure function, written to <set>/noise_flicker.tsv and summarized on stdout
    long noise_flicker_set(const NoiseSetRequest *request);

// the lines that the sum of a frame difference runs along (noise_vector_integration_table.md, rows 8 to 10). A row
// runs along x and a column along y. A plane is one whole z plane
#define NOISE_LINE_ROWS 0u

#define NOISE_LINE_COLUMNS 1u

#define NOISE_LINE_PLANES 2u

#define NOISE_LINE_TYPES 3u

    // the line sums of every sample, written to <set>/noise_lines.tsv. Then the z plane of every frame pair with the
    // squares of its row and column sums, written to <set>/noise_planes.tsv. A summary of both goes to stdout
    long noise_lines_set(const NoiseSetRequest *request);

// the five sums of each axis and level bin of the neighbor pass (noise_vector_integration_table.md, rows 17 and 25).
// They are the pairs, the squared frame difference, the same for the next voxel, and their products raised and lowered
// apart
#define NOISE_NEIGHBORS_PAIRS 0u

#define NOISE_NEIGHBORS_SQUARES 1u

#define NOISE_NEIGHBORS_OTHER_SQUARES 2u

#define NOISE_NEIGHBORS_RAISED 3u

#define NOISE_NEIGHBORS_LOWERED 4u

#define NOISE_NEIGHBORS_SUMS 5u

// the radii of the neighbor pass: the next voxel along z, y and x, then the voxel 2, 4, 8, 16 and 32 planes on along
// z. An interpolation between adjacent planes leaves every radius past the first uncorrelated. A blur along z fades over
// its radius. A term that every plane of a pixel shares is valid at every radius.
#define NOISE_NEIGHBOR_RADII 8u

    // every sample's neighbor correlation at each radius, written to <set>/noise_neighbors.tsv and summarized
    long noise_neighbors_set(const NoiseSetRequest *request);

// the clip pass (noise_vector_integration_table.md, rows 11, 12, 23 and 24). A spike is a frame above both frames
// beside it by a threshold, and a dip is one below both. The thresholds are 16, 32, 64 and 128 lane units
#define NOISE_SPIKE_THRESHOLDS 4u

#define NOISE_SPIKE_LEAST 16u

// each level bin contains the triples, then the spikes and the dips at each threshold
#define NOISE_SPIKE_TRIPLES 0u

#define NOISE_SPIKE_SUMS (1u + (2u * NOISE_SPIKE_THRESHOLDS))

// the counts of the clip pass of a sample. They count the voxel-frames at 0 and at the top, and the voxels whose every
// frame is one value. Last, the voxels stuck at 0 or at the top
#define NOISE_CLIPS_AT_ZERO 0u

#define NOISE_CLIPS_AT_TOP 1u

#define NOISE_CLIPS_CONSTANT 2u

#define NOISE_CLIPS_CONSTANT_ZERO 3u

#define NOISE_CLIPS_CONSTANT_TOP 4u

#define NOISE_CLIPS_COUNTS 5u

// the constant voxels' box: least and max z, then y, then x
#define NOISE_CLIPS_BOX (2u * ENGINE_AXES)

    // the value counts of every sample, the voxel-frames at either end of the lane, the voxels that never change, and
    // the spikes and dips. They go to <set>/noise_clips.tsv, <set>/noise_values.tsv and <set>/noise_spikes.tsv, with a
    // summary on stdout
    long noise_clips_set(const NoiseSetRequest *request);

    // the camera pixels of every sample (noise_vector_integration_table.md, rows 18 to 21). At each (y, x), the call
    // counts the static voxels of the z planes below the middle and of the planes from it up. It sums their values, and
    // sums the squares of their frame differences. The result goes to <set>/noise_pixels.tsv
    long noise_pixels_set(const NoiseSetRequest *request);

    // the cumulant sums of every sample (noise_vector_integration_table.md, rows 1, 11 and 12). The frames of each quiet
    // static voxel go in short blocks. The values of a block, less its own rounded mean, go to the powers 1 to 4, and the
    // call sums them (S1 to S4). Then, per level bin by the mean of the block, it sums those sums and their products over
    // the blocks. The unbiased k-statistics of the second, third and fourth cumulants follow from them exactly. The
    // result goes to <set>/noise_moments.tsv
    long noise_moments_set(const NoiseSetRequest *request);

    // One volume's measurements, the numbers each set call prints for a sample. A sim plants a term and reads it back
    // through these. The volume and extent have the layout that NoiseLoad gives them. Each returns 0, or
    // NOISE_DETECTOR_ERROR with the error filled; a measurement the volume cannot give is 0.

    // each lag's mean square against lag 1's, per mille over means 40 to 199: the frame difference, then it less its x
    // neighbor's
    long noise_flicker_volume(const unsigned short *volume, const unsigned long long extent[4],
                              unsigned long long per_mille[NOISE_FLICKER_LAGS],
                              unsigned long long neighbor_per_mille[NOISE_FLICKER_LAGS], EngineError *error);

// the two-way layout over every plane: rows, columns and the plane per mille, each 1000 where no part is common. Then
// the independent variance of the frame difference of a voxel, in lane units squared
#define NOISE_PLANE_ROWS 0u

#define NOISE_PLANE_COLUMNS 1u

#define NOISE_PLANE_PLANE 2u

#define NOISE_PLANE_INDEPENDENT 3u

// then the unbalanced layout of the static voxels: var_row, var_col and var_plane against the independent part s.
// Each is signed per mille, and 0 where no part is common. Then s in thousandths of a lane unit squared
#define NOISE_STATIC_ROWS 4u

#define NOISE_STATIC_COLUMNS 5u

#define NOISE_STATIC_PLANE 6u

#define NOISE_STATIC_INDEPENDENT 7u

#define NOISE_PLANE_READINGS 8u

    long noise_lines_volume(const unsigned short *volume, const unsigned long long extent[4],
                            long long readings[NOISE_PLANE_READINGS], EngineError *error);

    // the neighbor correlation at each radius over means 40 to 199, signed per mille
    long noise_neighbors_volume(const unsigned short *volume, const unsigned long long extent[4],
                                long long per_mille[NOISE_NEIGHBOR_RADII], EngineError *error);

    // the clip pass's counts and box, and its triples, spikes and dips over means 40 to 199
    long noise_clips_volume(const unsigned short *volume, const unsigned long long extent[4],
                            unsigned long long counts[NOISE_CLIPS_COUNTS], unsigned int box[NOISE_CLIPS_BOX],
                            unsigned long long spikes[NOISE_SPIKE_SUMS], EngineError *error);

// the second, third and fourth k-statistics of the moment pass, pooled over every level bin. Each is in signed
// thousandths of a lane unit to its power, rounded toward zero, and 0 where the pass kept no block
#define NOISE_MOMENT_CUMULANTS 3u

    long noise_moments_volume(const unsigned short *volume, const unsigned long long extent[4],
                              long long cumulants[NOISE_MOMENT_CUMULANTS], EngineError *error);

    // The line over the level (build plan item 38): the least-squares line through points, each weighted by a count.
    // The struct stores it as three exact integers over one denominator. `count` points sit at an integer place, and
    // their measurements sum to `total`. A level is a place over level_scale, and a measurement is the share of a total
    // over measurement_scale. The sums are `N = sum of counts`, `A = sum of count place` and `B = sum of count place^2`.
    // Also `T = sum of totals` and `C = sum of place total`.
    //
    // With these sums, the line is
    //   `slope = level_scale (N C - A T) / D`, `intercept = (T B - A C) / D`, `D = measurement_scale (N B - A^2)`
    // in measurements per level and in measurements at level 0.
    typedef struct
    {
        OriorExactInteger samples;
        OriorExactInteger along;
        OriorExactInteger along_square;
        OriorExactInteger measured;
        OriorExactInteger cross;
    } NoiseLineSums;

    typedef struct
    {
        OriorExactInteger slope;
        OriorExactInteger intercept;
        OriorExactInteger denominator;
    } NoiseLine;

    void noise_line_sums_zero(NoiseLineSums *sums);

    // `count` points at `place` whose measurements sum to `total`. An error leaves the sums as they are
    long noise_line_sums_add(NoiseLineSums *sums, unsigned long long count, unsigned long long place,
                             const OriorExactInteger *total, EngineError *error);

    // errors where the places do not span two levels (D would be 0) or where a scale is 0. It also errors where a
    // product passes the width of the exact integer. An error leaves the line as it is
    long noise_line_fit(const NoiseLineSums *sums, unsigned long long level_scale, unsigned long long measurement_scale,
                        NoiseLine *line, EngineError *error);

// C15, the cumulant ladder, and C19, the single-electron tail (build plan item 38). The quiet static blocks of the
// moment pass give k-statistics for each block. The call reads them against the level of the voxel over the frames of
// its other blocks. The own values of a block never set the level that the call reads it at. Then the line of each
// cumulant over that level, in lane units.
#define NOISE_LADDER_ORDERS 3u

    typedef struct
    {
        // the lines of k2, k3 and k4 against the level
        NoiseLine cumulant[NOISE_LADDER_ORDERS];
        unsigned long long blocks;
        unsigned long long left_out;
        // s3 >= s2^2 and s2 s4 >= s3^2, each cross-multiplied over the slopes' positive denominators
        int rising;
        int convex;
        // C19 from the k2 and k3 lines, set where s3 is not 0: `O = -c3 / s3` and `R^2 = (c2 s3 - c3 s2) / s3`. Each
        // is a numerator over a positive denominator. O is the offset only where `rising` and `convex` are both true.
        int tail;
        OriorExactInteger offset;
        OriorExactInteger offset_denominator;
        OriorExactInteger read_square;
        OriorExactInteger read_square_denominator;
    } NoiseLadderMeasurement;

    // errors on a volume with fewer than two whole blocks of frames, or whose blocks' levels do not span two values
    long noise_ladder_volume(const unsigned short *volume, const unsigned long long extent[4],
                             NoiseLadderMeasurement *measurement, EngineError *error);

// C20, crosstalk in the neighbor correlation (build plan item 38). The pass works along y and along x, over the same
// voxels: those whose voxel three steps on is inside the view. For the difference d' of every frame pair, it sums the
// pairs, `sum d'^2`, and the products `sum d'(v) d'(v + s delta)` for the steps `s = 1, 2 and 3`: C1, C2 and C3.
// Suppose that the system shares a draw with each neighbor by alpha after it makes it: then `V = sum d'^2 - 2 C2`,
// `alpha = C1 / (2 V)` and `alpha^2 = C2 / V`. These agree where `C1^2 = 4 C2 V`, and `C3 = 0`. The detector reads
// the sums exactly, and this file sets no bound on how near to agreement and to 0 a measurement must come.
#define NOISE_CROSSTALK_AXES 2u

#define NOISE_CROSSTALK_STEPS 3u

    typedef struct
    {
        unsigned long long pairs[NOISE_CROSSTALK_AXES];
        OriorExactInteger squares[NOISE_CROSSTALK_AXES];
        OriorExactInteger steps[NOISE_CROSSTALK_AXES][NOISE_CROSSTALK_STEPS];
        // V = sum d'^2 - 2 C2
        OriorExactInteger spread[NOISE_CROSSTALK_AXES];
    } NoiseCrosstalkMeasurement;

    // errors on a volume of fewer than two frames or one whose products could pass 64 bits a sum
    long noise_crosstalk_volume(const unsigned short *volume, const unsigned long long extent[4],
                                NoiseCrosstalkMeasurement *measurement, EngineError *error);

    // C14, charge before the gain (build plan item 38). The input is a bias series (no light, no exposure) and a dark
    // series (no light, exposed for a named dt) of one extent. Over each, the call sums I over every voxel-frame and d^2
    // over every frame difference. A series has M voxel-frames and P frame differences. The bias mean `A_b / M` is O,
    // with the mean of the fixed pattern. The bias `E[d^2] / 2 = Q_b / (2 P)` is the level-free sum
    // `R^2 + sigma_J^2 + sigma_kTC^2`.
    //
    // Dark electrons enter before the gain. The dark mean less the bias mean is `g D dt`, and the dark `E[d^2] / 2` less
    // that of the bias is `g^2 D dt`. Thus g is their ratio. Each measurement is a numerator over a positive denominator.
    typedef struct
    {
        OriorExactInteger offset;
        OriorExactInteger offset_denominator;
        OriorExactInteger level_free;
        OriorExactInteger level_free_denominator;
        OriorExactInteger dark_level;
        OriorExactInteger dark_level_denominator;
        OriorExactInteger dark_square;
        OriorExactInteger dark_square_denominator;
        // set where the dark mean differs from the bias mean
        int gain_read;
        OriorExactInteger gain;
        OriorExactInteger gain_denominator;
    } NoiseChargeMeasurement;

    // errors series of fewer than two frames, or whose squares could pass 64 bits a sum
    long noise_charge_series(const unsigned short *bias, const unsigned short *dark, const unsigned long long extent[4],
                             NoiseChargeMeasurement *measurement, EngineError *error);

    // C17, the structure function against the shared scale (build plan item 38). The pass reads each voxel and its x
    // neighbor whose mean level over the frames lies in 40 to 199. At each lag k of the flicker pass, it takes for every
    // frame pair `y = (d_k - d_{k,x})^2` against `q = (sum_t I - sum_t I_x)^2 = (T (L - L_x))^2`. The total of the pair
    // is `M = sum_t I + sum_t I_x`. Per lag and level bin of 8, it sums the pairs N and the sums of
    // `M, M^2, q, q^2, M q, y, M y, q y`.
    //
    // A scale `1 + eps_t` that the whole volume shares moves `d_k - d_{k,x}` by `(eps_{t+k} - eps_t)` times the light
    // apart of the pair. Y rises over `(L - L_x)^2` with slope `D_a(k) = E[(eps_{t+k} - eps_t)^2]`, the structure
    // function of the scale. It is above `2 D(k)`, the own noise of the pair. The own noise grows with the level, and a
    // bin of 8 still holds a spread of it.
    //
    // The fit of Y against M and q together runs in each bin, as the least squares of the line above over two places.
    // Then the fit pools over the bins. The centered sums `N sum u v - sum u sum v` of each bin, summed over the bins, are
    // MM, QQ, MQ, MY and QY. Then `D_a(k) = T^2 (MM QY - MQ MY) / (MM QQ - MQ^2)`. The intercept is the mean y of the
    // pairs, less `D_a(k)` times their mean `(L - L_x)^2`. Each reading is a numerator over a positive denominator.
    typedef struct
    {
        unsigned long long pairs[NOISE_FLICKER_LAGS];
        // set where q is not a line in M within every bin. MM QQ - MQ^2 is positive
        int read[NOISE_FLICKER_LAGS];
        OriorExactInteger slope[NOISE_FLICKER_LAGS];
        OriorExactInteger slope_denominator[NOISE_FLICKER_LAGS];
        OriorExactInteger intercept[NOISE_FLICKER_LAGS];
        OriorExactInteger intercept_denominator[NOISE_FLICKER_LAGS];
    } NoiseStructureMeasurement;

    // errors on a volume of fewer than two frames or two columns, of more than 65536 frames, or whose sums could pass
    // 128 bits
    long noise_structure_volume(const unsigned short *volume, const unsigned long long extent[4],
                                NoiseStructureMeasurement *measurement, EngineError *error);

    // Rows 18 and 19, what a camera pixel records in every plane. Over each pixel (y, x), the call takes its values in
    // the z planes below the middle and in the planes from the middle up. It sums each half over the frames: lo and hi,
    // over n_lo and n_hi values. An offset o or a gain `g (1 + p)` fixed at a pixel puts the same amount into the means
    // of both halves. The draws, independent between planes, share nothing. The covariance of the halves across the P
    // pixels, `C = (P sum lo hi - sum lo sum hi) / (P^2 n_lo n_hi)`, is `var(o) + var(p) (g S)^2` under a flat light S.
    //
    // Read at several lights, the line of C over the level squared has slope `var(p)` and intercept `var(o)`. With it
    // goes the mean level `sum (lo + hi) / (P (n_lo + n_hi))`. Each is a numerator over a positive denominator.
    typedef struct
    {
        OriorExactInteger covariance;
        OriorExactInteger covariance_denominator;
        OriorExactInteger level;
        OriorExactInteger level_denominator;
    } NoiseHalvesMeasurement;

    // errors on a volume of fewer than two planes, or whose sums could pass 128 bits
    long noise_halves_volume(const unsigned short *volume, const unsigned long long extent[4],
                             NoiseHalvesMeasurement *measurement, EngineError *error);

// The root noise of a box: a span of frames and a place that the caller names, such as an object in the cell workbook.
// Each term that the noise vector table names as shared is a pattern in fewer dimensions than the box. It has one value
// for every place along the axes that keep it, and the same value along the axes that share it.
//
// The pattern of a term over a box is the exact mean of the box along its shared axes, rounded down. Its residual is the
// box less the pattern, spread back along those axes. The own coder of the crystal prices the box, each residual and
// each pattern in bits (NoiseCost). A term saves what the box costs, less what its residual and its pattern cost
// together. The root noise is the term that saves the max, the first where two save alike, and none where no term
// saves. A yank of it keeps its residual and its pattern, and the box returns from them exactly (noise_root_return).
//
// Rows (table row 8): shared along x, a value for each frame, z and y
#define NOISE_ROOT_ROWS 0u

// columns (row 9): shared along y, a value for each frame, z and x
#define NOISE_ROOT_COLUMNS 1u

// planes (row 10): shared along y and x, a value for each frame and z
#define NOISE_ROOT_PLANES 2u

// the fixed pattern (row 18): shared along the frames and z, a value for each camera pixel, y and x
#define NOISE_ROOT_PIXELS 3u

// the term every plane of a stack shares (row 25): shared along z, a value for each frame, y and x
#define NOISE_ROOT_STACKS 4u

#define NOISE_ROOT_TERMS 5u

    // Prices a lattice of ints arranged as NoiseLoad arranges a volume, frames, z, y and x: the bits the crystal spends
    // on it. Returns 0, or a negative value with the error filled. The driver passes the engine's, and the detector
    // reaches no module but its own.
    typedef long (*NoiseCost)(const int *values, const unsigned long long extent[4], unsigned long long *bits,
                              EngineError *error);

    // `saved` is the box's bits less the term's residual's and pattern's, signed; `root` is NOISE_ROOT_TERMS where none
    // saves
    typedef struct
    {
        unsigned long long box_bits;
        unsigned long long residual_bits[NOISE_ROOT_TERMS];
        unsigned long long pattern_bits[NOISE_ROOT_TERMS];
        long long saved[NOISE_ROOT_TERMS];
        unsigned int root;
    } NoiseRootMeasurement;

    // The box runs from `low` to below `high` on every axis (frames, z, y and x) inside the extent of the volume, with at
    // max 2^31 voxels. Where `residual` is not NULL, the call leaves the residual of the root in it, the voxels of the box
    // as ints. Where there is no root, it leaves the box itself. Where `pattern` is not NULL, the call leaves the pattern
    // of the root in it, and nothing where there is no root. Its size comes from noise_root_extent, never more than the
    // voxels of the box. A price from the cost past 2^61 bits is an error.
    typedef struct
    {
        const unsigned short *volume;
        unsigned long long extent[4];
        unsigned long long low[4];
        unsigned long long high[4];
        NoiseCost cost;
        NoiseRootMeasurement *measurement;
        int *residual;
        int *pattern;
        EngineError *error;
    } NoiseRootRequest;

    long noise_root_box(const NoiseRootRequest *request);

    // the extent of the pattern of a term over the extent of a box. It is the own extent of the box along the kept axes
    // of the term. It is 1 along its shared axes and for a term past the last
    void noise_root_extent(unsigned int term, const unsigned long long box[4], unsigned long long pattern[4]);

    // The values of the box back from the residual and pattern of a term, each residual plus its pattern value. For the
    // residual and pattern that noise_root_box leaves, the sum is the value of the box exactly. Any other pair must bound
    // every sum within an int, and the call does not check that.
    typedef struct
    {
        const int *residual;
        const int *pattern;
        unsigned int term;
        unsigned long long box[4];
        int *values;
        EngineError *error;
    } NoiseReturnRequest;

    long noise_root_return(const NoiseReturnRequest *request);

#ifdef __cplusplus
}
#endif

#endif

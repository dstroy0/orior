// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file zeta_machine.h
 * @brief The machine over the automata of exact_zeta_turing.cu: the constants, the bounds, and one cell a run.
 *
 * @note exact_zeta_turing.c states the cells, the bounds on Z, the count and the machine. The examples that read the
 *       cells of the device read them through here.
 */
#ifndef ZETA_MACHINE_H
#define ZETA_MACHINE_H

#include "zeta_exact.h"

/** @brief The scale of the device, 2^62. */
#define TURING_SCALE_BITS 62

/** @brief The guard of the constants past the scale. */
#define TURING_GUARD_BITS 64

/** @brief The finest lattice, 2^22 points a cell. The x^2 of every cell lifts to 4^-22. */
#define TURING_FINEST 22

/** @brief The coarsest lattice. */
#define TURING_COARSEST 4

/** @brief The rounds of the machine. */
#define TURING_ROUNDS 4

/** @brief The methods of the main sum, as the device reads them. */
enum
{
    TURING_PAIRS = 0,
    TURING_TRANSFORM = 1,
    TURING_BOTH = 2,
    TURING_EM = 3,
    TURING_POINTS = 4
};

/** @brief The sums of a cell, by name, in the order of TURING_SUM_NAMES. */
enum
{
    SUM_LOW_BEFORE,
    SUM_LOW_AFTER,
    SUM_HIGH_UPTO,
    SUM_HIGH_AFTER,
    SUM_ZEROS_UPTO,
    SUM_ZEROS_AFTER,
    SUM_ZEROS_Q_UPTO,
    SUM_ZEROS_Q_AFTER,
    SUM_ZEROS_P_UPTO,
    SUM_ZEROS_P_AFTER,
    SUM_LOOSE,
    SUM_COUNT
};

/** @brief The names of the sums, as the device writes them. */
extern const char *const TURING_SUM_NAMES[SUM_COUNT];

/** @brief Every constant that the automata read, at 2^62, each within a unit, none of them dependent on the cell. */
typedef struct
{
    OriorExactInteger pi;         /**< Pi at 2^126. */
    int64_t curve_count;          /**< The curves C_0 to C_K, K + 1. */
    OriorExactInteger *gamma;     /**< The Taylor coefficients of each curve, in rows [OWNS]. */
    ZetaRational *sizes;          /**< The size of each curve on [-1, 1] [OWNS]. */
    ZetaRational *slopes;         /**< The slope of each curve on [-1, 1] [OWNS]. */
    OriorExactInteger ln2;        /**< ln 2. */
    OriorExactInteger artanh[21]; /**< 1 / (2k + 1). */
    OriorExactInteger cosine[18]; /**< (-1)^k pi^(2k) / (2k)!. */
    OriorExactInteger c96;        /**< 1 / (96 pi^2). */
    OriorExactInteger c3;         /**< 7 / (46080 pi^4). */
    OriorExactInteger c5;         /**< 31 / (2580480 pi^6). */
    OriorExactInteger pi_scaled;  /**< Pi at 2^62. */
    OriorExactInteger fact[48];   /**< 1 / (n + 1)!. */
    OriorExactInteger sinc[18];   /**< (-1)^n pi^(2n) / (2n + 1)!. */
} TuringConstants;

/**
 * @brief The constants.
 *
 * @param[out] constants The constants [BORROWS].
 */
void turing_constants(TuringConstants *constants);

/** @brief What the device returns for cell nu at 2^p points, with its values of x^2 2^p lifted to 4^-22. */
typedef struct
{
    int64_t nu;                        /**< The cell. */
    int64_t p;                         /**< The bits of the lattice. */
    int64_t lift;                      /**< 2^(44 - p). */
    int64_t points;                    /**< How many points. */
    int64_t first;                     /**< F, the first certified point. */
    int64_t last;                      /**< The last certified point. */
    OriorExactInteger at[3][8];        /**< The records at first, last and origin. */
    size_t at_count[3];                /**< How many values each record holds. */
    OriorExactInteger control[2];      /**< The Z of the device at two points. */
    OriorExactInteger sums[SUM_COUNT]; /**< The sums. */
    int64_t failed;                    /**< How many host checks failed. */
    int64_t steps[16];                 /**< The steps of each program. */
    size_t step_count;                 /**< How many. */
    int has_apart;                     /**< Whether the cell holds the gap of the two methods. */
    OriorExactInteger apart;           /**< That gap. */
    int64_t apart_at;                  /**< Its point. */
    OriorExactInteger margins[3];      /**< The clean, flagged and single counts, where present. */
    int has_margins[3];                /**< Which are present. */
    char path[256];                    /**< The output of the device. */
    int64_t refined[6];                /**< The spans flagged, points listed, zeros found, spans open, single, zeros. */
} TuringCell;

/** @brief The records of the first, the last and the origin point. */
enum
{
    AT_FIRST,
    AT_LAST,
    AT_ORIGIN
};

/**
 * @brief Reads a signed hexadecimal integer, as the device writes it.
 *
 * @param[in]  text   The text [BORROWS].
 * @param[out] result The value [BORROWS].
 */
void turing_parse_hex(const char *text, OriorExactInteger *result);

/**
 * @brief How far theta / pi rises across cell nu, near enough to choose its lattice.
 *
 * @param[in] nu The cell.
 * @return       2 x^2 ln x - x^2 at its ends.
 */
double turing_rises(int64_t nu);

/**
 * @brief The p of cell nu: the least with 2^p at least the rate times the rise of theta / pi across it.
 *
 * @param[in] nu   The cell.
 * @param[in] rate The points a zero.
 * @return         The bits.
 */
int64_t turing_lattice(int64_t nu, int64_t rate);

/**
 * @brief The bound on Z and on theta / pi over cell nu at 2^p points, in whole units of 2^-62.
 *
 * @param[in]  nu          The cell.
 * @param[in]  constants   The constants [BORROWS].
 * @param[in]  p           The bits of the lattice.
 * @param[in]  method      The method.
 * @param[out] bound       The bound on Z [BORROWS].
 * @param[out] theta_bound The bound on theta / pi [BORROWS].
 * @note It is the bound of the arithmetic of the pairs, or that of the multiple evaluation where that gives the
 *       verdict.
 */
void turing_bounds(int64_t nu, const TuringConstants *constants, int64_t p, int method, OriorExactInteger *bound,
                   OriorExactInteger *theta_bound);

/**
 * @brief The bound on Z and on theta / pi over cell nu by Euler-Maclaurin, in whole units of 2^-62.
 *
 * @param[in]  nu          The cell.
 * @param[in]  heads       N.
 * @param[in]  terms       M.
 * @param[out] bound       The bound on Z [BORROWS].
 * @param[out] theta_bound The bound on theta / pi [BORROWS].
 */
void turing_em_bounds(int64_t nu, int64_t heads, int64_t terms, OriorExactInteger *bound,
                      OriorExactInteger *theta_bound);

/**
 * @brief M, the Bernoulli terms that Euler-Maclaurin takes at a level of the search.
 *
 * @param[in] level The level.
 * @return          M.
 */
int64_t turing_em_terms(int64_t level);

/**
 * @brief N for cell nu at M terms: the least with |s + j| / (2 pi N) <= 1/2 for every j < 2M over the cell.
 *
 * @param[in] nu    The cell.
 * @param[in] terms M.
 * @return          N.
 */
int64_t turing_em_heads(int64_t nu, int64_t terms);

/**
 * @brief Cell nu at 2^p points from the device, by the method, or by Euler-Maclaurin at a level of the search.
 *
 * @param[in]  binary    The device program [BORROWS].
 * @param[in]  constants The constants [BORROWS].
 * @param[in]  nu        The cell.
 * @param[in]  p         The bits of the lattice.
 * @param[in]  method    The method.
 * @param[in]  level     The level of Euler-Maclaurin.
 * @param[in]  listing   1 to list every point, 2 to list the twist too.
 * @param[out] cell      The cell [BORROWS].
 * @param[out] bound     The bound on Z [BORROWS].
 */
void turing_run_cell(const char *binary, const TuringConstants *constants, int64_t nu, int64_t p, int method,
                     int64_t level, int listing, TuringCell *cell, OriorExactInteger *bound);

/** @brief The certified sign at a listed point, its S, and the flag or the bracket of theta / pi. */
typedef struct
{
    OriorExactInteger sign;     /**< The sign. */
    OriorExactInteger s;        /**< S. */
    OriorExactInteger extra[2]; /**< The flag, or theta / pi less and more its bound. */
} TuringPoint;

/**
 * @brief The certified signs of Z at the points j of cell nu at 2^p, by pairs at those points alone.
 *
 * @param[in]  binary    The device program [BORROWS].
 * @param[in]  constants The constants [BORROWS].
 * @param[in]  nu        The cell.
 * @param[in]  p         The bits of the lattice.
 * @param[in]  js        The points [BORROWS].
 * @param[in]  count     How many.
 * @param[in]  bracketed 1 for theta / pi less and more its bound, 0 for the flag of the step.
 * @param[out] points    The points [BORROWS]. count of them.
 * @return               How many host checks failed.
 * @note The code pads the list with its last point to a whole number of pieces. A repeated point changes no sign.
 */
int64_t turing_run_points(const char *binary, const TuringConstants *constants, int64_t nu, int64_t p,
                          const int64_t *js, size_t count, int bracketed, TuringPoint *points);

/**
 * @brief x^2 at a record of the cell, at 4^-22.
 *
 * @param[in]  cell   The cell [BORROWS].
 * @param[in]  which  AT_FIRST, AT_LAST or AT_ORIGIN.
 * @param[out] result x^2 [BORROWS].
 */
void turing_x2(const TuringCell *cell, int which, OriorExactInteger *result);

/**
 * @brief N at the F of the cell is at least this, over the window from the F of back.
 *
 * @param[in]  cell   The cell [BORROWS].
 * @param[in]  back   The cell before [BORROWS].
 * @param[out] result The bound [BORROWS].
 */
void turing_below(const TuringCell *cell, const TuringCell *back, ZetaRational *result);

/**
 * @brief N at the F of the cell is at most this. The window runs on to the F of ahead, or to the last point.
 *
 * @param[in]  cell   The cell [BORROWS].
 * @param[in]  ahead  The cell after, or NULL [BORROWS].
 * @param[out] result The bound [BORROWS].
 */
void turing_above(const TuringCell *cell, const TuringCell *ahead, ZetaRational *result);

/**
 * @brief The zeros certified after the F of the cell, up to the F of ahead.
 *
 * @param[in] cell  The cell [BORROWS].
 * @param[in] ahead The cell after [BORROWS].
 * @return          The count.
 */
int64_t turing_between(const TuringCell *cell, const TuringCell *ahead);

/**
 * @brief t = 2 pi x^2 / 4^22, as a double.
 *
 * @param[in] x2 x^2 at 4^-22 [BORROWS].
 * @return       t.
 */
double turing_t_of(const OriorExactInteger *x2);

/**
 * @brief The margin that a step must clear over cell nu at 2^p points, h / 3, the steepness and e_D, at 2^62.
 *
 * @param[in]  nu        The cell.
 * @param[in]  p         The bits of the lattice.
 * @param[in]  constants The constants [BORROWS].
 * @param[in]  bound     The bound on Z [BORROWS].
 * @param[in]  method    TURING_TRANSFORM or TURING_PAIRS.
 * @param[out] values    The margin, h / 3, the steepness and e_D [BORROWS].
 */
void turing_margin(int64_t nu, int64_t p, const TuringConstants *constants, const OriorExactInteger *bound, int method,
                   OriorExactInteger values[4]);

#endif

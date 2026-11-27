// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Catalog: EXP-x-028
/**
 * @file exact_zeta_turing.c
 * @brief The method of Turing, run as a machine over the automata of exact_zeta_turing.cu.
 *
 * @note Each automaton is a fixed program of the record machine, checked against the host word for word. This
 *       machine sits one level up and runs them a cell at a time. It joins what each cell returns to the results of
 *       its neighbors. It halts with a proven count, or with a cell that it does not close.
 * @note Run build_exact_zeta.sh turing <binary> [first] [last] [rate] [pairs|transform|both|refine [check]], or
 *       build_exact_zeta.sh turing <binary> [first] [last] [rate] em. The script exact_zeta_turing.sh beside the
 *       device source builds the binary.
 * @note For each run, the machine writes the input of the device to a file under build/turing, and reads the output
 *       back. Every run goes to one process of the binary, started with serve. That process loads each program onto
 *       the device once, and runs every later stage with the same program on it.
 * @note It sits in 0_experimental, and is an entry in the workbook of analytic number theory, on its rail. It
 *       verifies the zeros up to a height that the caller gives, and says nothing past it.
 * @note The cells: cell nu holds the points t = 2 pi s, s = x^2 = nu^2 + j (2 nu + 1) / P, j below P = 2^p. The
 *       points are even in t, and in theta to within 1 / nu across the cell. P is the least power of 2 that gives the
 *       cell rate points or more for each unit of the rise of theta / pi across it. That unit is the share of a zero.
 * @note The device works out every value of the cell itself. It starts from nu, p, and constants that do not depend
 *       on nu. It takes ln k and k^(-1/2) once for each k <= nu, and theta / pi and the remainder once for each point.
 *       It takes the main sum of each point over k <= nu, and the sign of Z that it certifies.
 * @note It finds the zeros between certified signs. It takes the sums of the method of Turing over the two ranges of
 *       the cell about its first certified point F. A cell returns F, its last certified point, its point 0, and
 *       those sums.
 * @note The main sum: by pairs, the device takes the nu terms of each point one lane apiece, nu P lanes a cell. By
 *       the multiple evaluation of Odlyzko and Schonhage, it takes the sum F_j at every point at once. That sum is
 *       F_j = sum over k of a_k exp(-2 pi i j pos_k / P). Here a_k = k^(-1/2) exp(-2 pi i nu^2 ln k) and pos_k = (2 nu
 * + 1) ln k.
 * @note F is the discrete Fourier transform of u_h / P = sum over k of q_k / (z_h - w_k). That is a sum of nu poles
 *       on the unit circle at the P roots of unity z_h. A tree of multipole and local expansions of order 28, over
 *       leaves of 2^4 frequencies, evaluates it.
 * @note The tree takes the near poles of each leaf whole through the Dirichlet kernel. Half of the main sum is
 *       Re(exp(i theta) F), and its lanes a cell grow as P instead of nu P. With both, the device runs the two, and
 *       writes the largest gap of their Z over the cell.
 * @note The bound on Z: Z = 2 sum + (-1)^(nu - 1) x^(-1/2) sum over n <= K of C_n(z) x^(-n) + R_K. For t >= 200,
 *       |R_K| < d_K t^(-(2K + 3) / 4). These are the bounds of Gabcke, Satz 3.2.2 of his thesis (Neue Herleitung und
 *       explizite Restabschätzung der Riemann-Siegel-Formel, Göttingen 1979).
 * @note The table runs from d_0 = 0.127 up to d_10 = 25966. The bound of K is the least of them, and every C_n to it
 *       runs on the device.
 * @note The value theta is (t/2) log(t / (2 pi e)) - pi/8 + 1/(48 t) + 7/(5760 t^3) + 31/(80640 t^5) + E_theta. For
 *       t >= 10, |E_theta| < 1 / (3322 t^7), from the introduction of the same thesis, (4) and (5).
 * @note Below t = 10, the bound of Brent past 1/(48 t) applies, with the two terms after it:
 *       (7/5760 + pi/960) t^(-3) + exp(-pi t) / 2. The source is On asymptotic approximations to the log-Gamma and
 *       Riemann-Siegel theta functions, Theorems 5 and 6.
 * @note An error in theta moves Z by at most 2 sum of m^(-1/2) <= 4 nu^(1/2) times it. The own error of the device is
 *       a unit of 2^-62 a division and a unit a constant. The module zeta_machine.c bounds it step by step. It also
 *       bounds the error of the multiple evaluation with its truncation. The code certifies a sign of Z where |Z|
 * exceeds the sum of the three.
 * @note By Euler-Maclaurin, Z = sum over n < N of n^(-1/2) cos(theta - t ln n) + Re(exp(i theta) N^(-s) C). The cell
 *       chooses N and the Bernoulli terms M. The bound of Johansson on the remainder holds at every t > 0 (Johansson,
 *       arXiv:1309.2877, Theorem 1).
 * @note The cells below 168 pi: with em, cells first to last run by Euler-Maclaurin, from cell 1 up. Riemann-Siegel
 *       over cells last to last + 2 certifies N at the F of cell last + 1, T_a. Count the certified sign changes in
 *       (the F of cell first, T_a]. Where they number N(T_a), every zero up to T_a is on the line and simple.
 * @note The count: two certified points of opposite sign, with no certified point between, hold a zero of Z between
 *       them. That is a zero of zeta on the line. N(t) = theta(t) / pi + 1 + S(t) off the ordinates.
 * @note For t_2 > t_1 > 168 pi, |integral of S from t_1 to t_2| <= 2.067 + 0.059 log t_2 (Trudgian, Improvements to
 *       the method of Turing, Theorem 2.2). On a step [t_i, t_(i+1)] of the lattice, theta lies between its ends.
 *       The zeros certified past T are at most N(t) - N(T).
 * @note Over a window after T, N(T) <= 1 + B / (2 pi D) + sum of d_i (Theta_(i+1) - c_i) / D. Over one before it,
 *       N(T) >= 1 + sum of d_i (k_(i+1) + Theta_i) / D - B / (2 pi D). Here d_i are the steps of x^2, and D their sum.
 * @note c_i is the count of zeros certified in (T, t_i], and k_(i+1) the count in (t_(i+1), T]. N is a whole number.
 *       The count of zeros of a cell, and its sums of x^2 at their ends, carry all of both sums.
 * @note Every step of x^2 in cell nu is (2 nu + 1) / P, the one from its last point to the point 0 of the next cell
 *       too. That one weighs theta / pi at point 0.
 * @note The machine certifies N at the F of every cell. It uses the window back to the F of the cell before, and the
 *       one on to the F of the cell after. Two sets of cells run again on a lattice four times finer. One set is the
 * cells whose certified zeros fall short of N at the F of the next cell, less N at their own F.
 * @note The other set is the cells about an F where N has no certified single value. Where every cell closes, every
 *       zero from the F of the second cell to the last F is a certified sign change. Each is on the line, and simple.
 * @note With refine, each cell runs by the multiple evaluation with F'. The device takes
 *       Z' = 2 Re(exp(i theta) F (i theta' + F'/F)) at every point. It calls a step clean where the cubic through Z
 *       and Z' at its ends clears the margin. A clean step holds no zero.
 * @note A run of one sign between two certified points, where the run holds a step that is not clean, gets a new
 *       certification. It runs at 2^3 times the points, by pairs at the listed points with their own Z'. It runs
 *       again inside that run where a fine step is not clean, to three times.
 * @note With check, a step whose ends change sign closes only where the slope of the cubic clears the steepness and Z
 *       crosses it once. A run that holds one that does not gets a new certification the same way.
 * @note Positive control: the Z of the device at two points of the first cell against the main sum and remainder of
 *       zeta_riemann_siegel.c, rs_cut_at. By Euler-Maclaurin, it is against rs_em_at. At cell last, it is against the
 *       Z of Riemann-Siegel within the two bounds.
 */
#include "zeta_riemann_siegel.h"
#include "../../../src/platform/platform.h"
#include "zeta_machine.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/** @brief The bits by which a refinement makes the lattice finer. */
#define REFINE_BITS 3

/** @brief How many times a refinement runs. */
#define REFINE_DEPTH 3

/**
 * @brief The text of a truth value, as Python prints it.
 *
 * @param[in] value The value.
 * @return          "True" or "False".
 */
static const char *truth(int value)
{
    return value ? "True" : "False";
}

/**
 * @brief Stops the program where an allocation fails, else returns the block.
 *
 * @param[in] block The block [OWNS].
 * @return          The same block [OWNS].
 */
static void *machine_memory(void *block)
{
    if (block == NULL)
    {
        zeta_stop("out of memory");
    }
    return block;
}

/**
 * @brief The seconds of the wall clock.
 *
 * @return The seconds.
 */
static double seconds_now(void)
{
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    return (double)now.tv_sec + ((double)now.tv_nsec / 1e9);
}

/**
 * @brief An integer over 2^62, as the nearest double.
 *
 * @param[in] value The integer [BORROWS].
 * @return          The double.
 */
static double over_unit(const OriorExactInteger *value)
{
    OriorExactInteger unit;
    z_two_to(TURING_SCALE_BITS, &unit);
    ZetaRational ratio;
    rational_make(value, &unit, &ratio);
    return rational_to_double(&ratio);
}

/**
 * @brief The ceiling or the floor of a rational, as a machine integer.
 *
 * @param[in] value   The rational [BORROWS].
 * @param[in] ceiling 1 for the ceiling, 0 for the floor.
 * @return            The value.
 */
static int64_t whole(const ZetaRational *value, int ceiling)
{
    OriorExactInteger result;
    if (ceiling)
    {
        OriorExactInteger top;
        z_neg(&value->top, &top);
        z_floor_div(&top, &value->under, &result);
        z_neg(&result, &result);
    }
    else
    {
        z_floor_div(&value->top, &value->under, &result);
    }
    return z_small(&result);
}

/* ---------------------------------------------------------------- the positive controls */

/**
 * @brief The point x = (s 10^80 / 2^p)^(1/2) of cell nu as a delta at 40 places.
 *
 * @param[in]  cell The cell [BORROWS].
 * @param[in]  j    The point.
 * @param[out] p    The delta [BORROWS].
 */
static void control_point(const TuringCell *cell, int64_t j, DecimalPair *p)
{
    OriorExactInteger s;
    z_set(&s, cell->nu * cell->nu);
    OriorExactInteger power;
    z_two_to(cell->p, &power);
    z_mul(&s, &power, &s);
    OriorExactInteger step;
    z_set(&step, j * ((2 * cell->nu) + 1));
    z_add(&s, &step, &s);
    zeta_require(orior_exact_scale_by_ten(&s, 80u), "scale");
    z_floor_div(&s, &power, &s);
    OriorExactInteger x;
    z_isqrt(&s, &x);
    OriorExactInteger base;
    z_ten_to(40, &base);
    z_mul_small(&base, cell->nu, &base);
    z_sub(&x, &base, &x);
    pair_make(&x, 40, p);
}

/**
 * @brief The Z of the device against the main sum and remainder of the house at two points of the cell.
 *
 * @param[in] cell   The cell [BORROWS].
 * @param[in] curves The curve count.
 */
static void control(const TuringCell *cell, int64_t curves)
{
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  positive control, cell ");
        mmgr_line_decimal_signed(output_line, (long long)cell->nu);
        mmgr_line_put(output_line, ":");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    const int64_t js[2] = {0, cell->points / 3};
    for (int at = 0; at < 2; at += 1)
    {
        DecimalPair p;
        control_point(cell, js[at], &p);
        OriorExactInteger main_sum;
        OriorExactInteger remainder;
        rs_cut_at(cell->nu, &p, 40, curves - 1, &main_sum, &remainder);
        z_add(&main_sum, &remainder, &main_sum);
        OriorExactInteger places;
        z_ten_to(40, &places);
        ZetaRational house;
        rational_make(&main_sum, &places, &house);
        OriorExactInteger unit;
        z_two_to(TURING_SCALE_BITS, &unit);
        ZetaRational device_value;
        rational_make(&cell->control[at], &unit, &device_value);
        ZetaRational gap;
        rational_sub(&device_value, &house, &gap);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    s = ");
            mmgr_line_decimal_signed(output_line, (long long)cell->nu);
            mmgr_line_put(output_line, "^2 + ");
            mmgr_line_decimal_signed(output_line, (long long)js[at]);
            mmgr_line_put(output_line, " (2 ");
            mmgr_line_decimal_signed(output_line, (long long)cell->nu);
            mmgr_line_put(output_line, " + 1) / 2^");
            mmgr_line_decimal_signed(output_line, (long long)cell->p);
            mmgr_line_put(output_line, "  device Z ");
            mmgr_line_fixed(output_line, (double)(rational_to_double(&device_value)), 15u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, "  house Z ");
            mmgr_line_fixed(output_line, (double)(rational_to_double(&house)), 15u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, "  apart ");
            mmgr_line_exponent(output_line, (double)(fabs(rational_to_double(&gap))), 3u, 0u, EMBED_FALSE);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
}

/**
 * @brief The Z of Euler-Maclaurin at two points of a cell against the routes of the house.
 *
 * @param[in] cell       The cell [BORROWS].
 * @param[in] other      The same cell by Riemann-Siegel, or NULL [BORROWS].
 * @param[in] bounds_sum The sum of the two bounds, beside other [BORROWS].
 * @note With other, the check also holds the Z of the cell against that Z within the two bounds.
 */
static void control_em(const TuringCell *cell, const TuringCell *other, const OriorExactInteger *bounds_sum)
{
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  positive control, cell ");
        mmgr_line_decimal_signed(output_line, (long long)cell->nu);
        mmgr_line_put(output_line, " by Euler-Maclaurin:");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    const int64_t js[2] = {0, cell->points / 3};
    OriorExactInteger unit;
    z_two_to(TURING_SCALE_BITS, &unit);
    for (int at = 0; at < 2; at += 1)
    {
        DecimalPair p;
        control_point(cell, js[at], &p);
        OriorExactInteger value;
        rs_em_at(cell->nu, &p, 30, &value);
        OriorExactInteger places;
        z_ten_to(30, &places);
        ZetaRational house;
        rational_make(&value, &places, &house);
        ZetaRational device_value;
        rational_make(&cell->control[at], &unit, &device_value);
        ZetaRational gap;
        rational_sub(&device_value, &house, &gap);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    s = ");
            mmgr_line_decimal_signed(output_line, (long long)cell->nu);
            mmgr_line_put(output_line, "^2 + ");
            mmgr_line_decimal_signed(output_line, (long long)js[at]);
            mmgr_line_put(output_line, " (2 ");
            mmgr_line_decimal_signed(output_line, (long long)cell->nu);
            mmgr_line_put(output_line, " + 1) / 2^");
            mmgr_line_decimal_signed(output_line, (long long)cell->p);
            mmgr_line_put(output_line, "  device Z ");
            mmgr_line_fixed(output_line, (double)(rational_to_double(&device_value)), 15u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, "  house Z ");
            mmgr_line_fixed(output_line, (double)(rational_to_double(&house)), 15u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, "  apart ");
            mmgr_line_exponent(output_line, (double)(fabs(rational_to_double(&gap))), 3u, 0u, EMBED_FALSE);
            platform_line_out_ended();
        }
        if (other != NULL)
        {
            ZetaRational theirs;
            rational_make(&other->control[at], &unit, &theirs);
            ZetaRational apart;
            rational_sub(&device_value, &theirs, &apart);
            if (apart.top.sign < 0)
            {
                apart.top.sign = 1;
            }
            ZetaRational bar;
            rational_make(bounds_sum, &unit, &bar);
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  from Riemann-Siegel's ");
                mmgr_line_exponent(output_line, (double)(rational_to_double(&apart)), 3u, 0u, EMBED_FALSE);
                mmgr_line_put(output_line, ", within ");
                mmgr_line_exponent(output_line, (double)(rational_to_double(&bar)), 3u, 0u, EMBED_FALSE);
                mmgr_line_put(output_line, ": ");
                mmgr_line_put(output_line, truth(rational_cmp(&apart, &bar) <= 0));
                platform_line_out_ended();
            }
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
}

/**
 * @brief Writes the steps of the programs of a cell as Python prints a list.
 *
 * @param[in] cell The cell [BORROWS].
 */
static void print_steps(const TuringCell *cell)
{
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "[");
        platform_line_out_ended();
    }
    for (size_t at = 0u; at < cell->step_count; at += 1u)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, (at > 0u) ? ", " : "");
        mmgr_line_decimal_signed(output_line, (long long)cell->steps[at]);
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "]");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
}

/* ---------------------------------------------------------------- the refinement of the flagged spans */

/** @brief A run between two certified points, its ends, and the zeros that the count of the device holds in it. */
typedef struct
{
    int64_t a;                /**< The first point. */
    int64_t b;                /**< The last point. */
    OriorExactInteger sign_a; /**< The sign at a. */
    OriorExactInteger sign_b; /**< The sign at b. */
    OriorExactInteger s_a;    /**< S at a. */
    OriorExactInteger s_b;    /**< S at b. */
    int64_t *counted;         /**< The zeros that the count already holds, shared by the spans that come from it. */
} Span;

/** @brief A row of the listing of a cell: the sign, S and the flag of the step. */
typedef struct
{
    OriorExactInteger sign; /**< The sign. */
    OriorExactInteger s;    /**< S. */
    int flag;               /**< The flag. */
} ListedRow;

/**
 * @brief The spans of a cell listed with Z' that the margin stage does not close.
 *
 * @param[in]  cell     The cell [BORROWS].
 * @param[in]  check    1 to count the runs whose ends change sign too.
 * @param[out] spans    The spans [BORROWS]. Allocated.
 * @param[out] counters The counts of the spans [BORROWS]. Allocated.
 * @return              How many spans.
 * @note Each is a run between two consecutive certified points, past F, that holds a step that the device flags.
 *       Such a step is neither clean nor a single crossing. A run whose ends change sign counts only with check,
 *       which proves each crossing single.
 * @note Each span carries the signs and S of its ends, and the zeros that the count of the device already holds in
 *       it. That is one zero where the signs change and the points are at most two apart, the reach of the count
 *       stage, else none.
 */
static size_t flagged_spans(const TuringCell *cell, int check, Span **spans, int64_t **counters)
{
    FILE *const handle = fopen(cell->path, "r");
    if (handle == NULL)
    {
        zeta_stop("the listing of the cell is gone");
    }
    size_t row_count = 0u;
    size_t capacity = 1024u;
    ListedRow *rows = (ListedRow *)machine_memory(
        platform_cell_taken(capacity * sizeof(ListedRow), platform_place_g_program_owner, 0));
    static char line[1 << 16];
    while (fgets(line, sizeof(line), handle) != NULL)
    {
        if (!EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = "point", .ci = EMBED_FALSE))
        {
            continue;
        }
        char *words[64];
        size_t count = 0u;
        char *cursor = line + 5;
        while (*cursor != '\0')
        {
            while ((*cursor == ' ') || (*cursor == '\r') || (*cursor == '\n'))
            {
                *cursor = '\0';
                cursor += 1;
            }
            if (*cursor == '\0')
            {
                break;
            }
            if (count < 64u)
            {
                words[count] = cursor;
                count += 1u;
            }
            while ((*cursor != '\0') && (*cursor != ' ') && (*cursor != '\r') && (*cursor != '\n'))
            {
                cursor += 1;
            }
        }
        if (row_count >= capacity)
        {
            capacity *= 2u;
            rows = (ListedRow *)machine_memory(platform_cell_grown(rows, (capacity / 2u) * sizeof(ListedRow),
                                                                   capacity * sizeof(ListedRow),
                                                                   platform_place_g_program_owner, 0));
        }
        OriorExactInteger flag;
        z_set(&rows[row_count].sign, strtoll(words[0], NULL, 16));
        rows[row_count].s.sign = 0;
        orior_exact_zero(&rows[row_count].s);
        // S at a point is below 2^63 for the cells of this machine.
        z_set(&rows[row_count].s, strtoll(words[1], NULL, 16));
        z_set(&flag, strtoll(words[count - 1u], NULL, 16));
        rows[row_count].flag = flag.sign != 0;
        row_count += 1u;
    }
    fclose(handle);
    *spans =
        (Span *)machine_memory(platform_cell_cleared(row_count + 1u, sizeof(Span), platform_place_g_program_owner, 0));
    *counters = (int64_t *)machine_memory(
        platform_cell_cleared(row_count + 1u, sizeof(int64_t), platform_place_g_program_owner, 0));
    size_t found = 0u;
    int64_t previous = -1;
    for (int64_t j = cell->first; j < (int64_t)row_count; j += 1)
    {
        if (rows[j].sign.sign == 0)
        {
            continue;
        }
        if (previous >= 0)
        {
            const int64_t a = previous;
            const int64_t b = j;
            const int same = orior_exact_equal(&rows[a].sign, &rows[b].sign);
            int flagged = 0;
            for (int64_t q = a + 1; q <= b; q += 1)
            {
                flagged = flagged || rows[q].flag;
            }
            if ((check || same) && flagged)
            {
                Span *const span = &(*spans)[found];
                span->a = a;
                span->b = b;
                span->sign_a = rows[a].sign;
                span->sign_b = rows[b].sign;
                span->s_a = rows[a].s;
                span->s_b = rows[b].s;
                (*counters)[found] = (!same && ((b - a) <= 2)) ? 1 : 0;
                span->counted = &(*counters)[found];
                found += 1u;
            }
        }
        previous = j;
    }
    platform_cell_released(rows, platform_place_g_program_owner);
    return found;
}

/**
 * @brief Counts a zero that a refinement finds, unless the count of the device already holds it.
 *
 * @param[in,out] cell    The cell [BORROWS].
 * @param[in,out] counted The zeros that the count holds in the span [BORROWS].
 * @param[in]     low     S at the earlier point [BORROWS].
 * @param[in]     high    S at the later point [BORROWS].
 * @param[in]     lift    The bits between the lattices.
 * @param[in,out] added   The zeros added [BORROWS].
 * @note The sums take the S of the earlier point rounded down to the lattice of the cell. They take the S of the later
 *       one rounded up.
 */
static void take_zero(TuringCell *cell, int64_t *counted, const OriorExactInteger *low, const OriorExactInteger *high,
                      int64_t lift, int64_t *added)
{
    if (*counted > 0)
    {
        *counted -= 1;
        return;
    }
    OriorExactInteger one;
    z_set(&one, 1);
    z_add(&cell->sums[SUM_ZEROS_AFTER], &one, &cell->sums[SUM_ZEROS_AFTER]);
    OriorExactInteger power;
    z_two_to(lift, &power);
    OriorExactInteger value;
    z_floor_div(low, &power, &value);
    z_add(&cell->sums[SUM_ZEROS_P_AFTER], &value, &cell->sums[SUM_ZEROS_P_AFTER]);
    z_neg(high, &value);
    z_floor_div(&value, &power, &value);
    z_neg(&value, &value);
    z_add(&cell->sums[SUM_ZEROS_Q_AFTER], &value, &cell->sums[SUM_ZEROS_Q_AFTER]);
    *added += 1;
}

/**
 * @brief The zeros that the flagged spans hide, certified at a lattice 2^3 times finer by the listed points.
 *
 * @param[in]     binary    The device program [BORROWS].
 * @param[in]     constants The constants [BORROWS].
 * @param[in,out] cell      The cell [BORROWS].
 * @param[in]     spans     The spans [BORROWS].
 * @param[in]     count     How many.
 * @param[in]     check     1 to close a crossing only where Z crosses once.
 * @param[out]    results   The points listed, the zeros added, the spans still open, the failed host checks
 *                          [BORROWS].
 * @note The ends of each span are among those points, and the zeros go into the sums of the cell past F. The margin
 *       stage lists a fine step that it flags again, finer by as much, to three times.
 * @note A fine step closes where it is clean. It also closes where its ends change sign, and with check where Z also
 *       crosses it once. A span closes where all its steps do.
 * @note A zero that the count of the device already holds in a span counts for the first zero found in it. A span
 *       still open after the last lattice gives the zero that the signs of its ends certify, where they change.
 */
static void refine(const char *binary, const TuringConstants *constants, TuringCell *cell, const Span *spans,
                   size_t count, int check, int64_t results[4])
{
    int64_t listed = 0;
    int64_t added = 0;
    int64_t failed = 0;
    Span *open =
        (Span *)machine_memory(platform_cell_taken((count + 1u) * sizeof(Span), platform_place_g_program_owner, 0));
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = open, .src = spans, .bytes = count * sizeof(Span));
    size_t open_count = count;
    int64_t p = cell->p;
    for (int depth = 0; (depth < REFINE_DEPTH) && (open_count > 0u); depth += 1)
    {
        p += REFINE_BITS;
        const int64_t lift = p - cell->p;
        size_t total = 0u;
        for (size_t at = 0u; at < open_count; at += 1u)
        {
            total += (size_t)(((open[at].b - open[at].a) << REFINE_BITS) + 1);
        }
        int64_t *const js = (int64_t *)machine_memory(
            platform_cell_taken((total + 1u) * sizeof(int64_t), platform_place_g_program_owner, 0));
        size_t *const lows = (size_t *)machine_memory(
            platform_cell_taken((open_count + 1u) * sizeof(size_t), platform_place_g_program_owner, 0));
        size_t *const highs = (size_t *)machine_memory(
            platform_cell_taken((open_count + 1u) * sizeof(size_t), platform_place_g_program_owner, 0));
        size_t filled = 0u;
        for (size_t at = 0u; at < open_count; at += 1u)
        {
            lows[at] = filled;
            for (int64_t j = open[at].a << REFINE_BITS; j <= (open[at].b << REFINE_BITS); j += 1)
            {
                js[filled] = j;
                filled += 1u;
            }
            highs[at] = filled;
        }
        TuringPoint *const rows = (TuringPoint *)machine_memory(
            platform_cell_cleared(total + 1u, sizeof(TuringPoint), platform_place_g_program_owner, 0));
        failed += turing_run_points(binary, constants, cell->nu, p, js, total, 0, rows);
        // The count of listed points fits a machine integer.
        listed += (int64_t)total;
        Span *const opened =
            (Span *)machine_memory(platform_cell_taken((total + 1u) * sizeof(Span), platform_place_g_program_owner, 0));
        size_t opened_count = 0u;
        for (size_t at = 0u; at < open_count; at += 1u)
        {
            const Span *const span = &open[at];
            const size_t length = highs[at] - lows[at];
            // The coarser run already certified the ends of the span.
            ListedRow *const run = (ListedRow *)machine_memory(
                platform_cell_taken((length + 1u) * sizeof(ListedRow), platform_place_g_program_owner, 0));
            OriorExactInteger lifted;
            OriorExactInteger power;
            z_two_to(REFINE_BITS, &power);
            run[0].sign = span->sign_a;
            z_mul(&span->s_a, &power, &lifted);
            run[0].s = lifted;
            run[0].flag = 0;
            for (size_t q = 1u; (q + 1u) < length; q += 1u)
            {
                run[q].sign = rows[lows[at] + q].sign;
                run[q].s = rows[lows[at] + q].s;
                run[q].flag = rows[lows[at] + q].extra[0].sign != 0;
            }
            run[length - 1u].sign = span->sign_b;
            z_mul(&span->s_b, &power, &lifted);
            run[length - 1u].s = lifted;
            run[length - 1u].flag = rows[highs[at] - 1u].extra[0].sign != 0;
            int64_t previous = -1;
            for (size_t q = 0u; q < length; q += 1u)
            {
                if (run[q].sign.sign == 0)
                {
                    continue;
                }
                if (previous >= 0)
                {
                    // The places in a run fit a machine integer.
                    const size_t u = (size_t)previous;
                    const int crossed = !orior_exact_equal(&run[u].sign, &run[q].sign);
                    int flagged = 0;
                    for (size_t k = u + 1u; k <= q; k += 1u)
                    {
                        flagged = flagged || run[k].flag;
                    }
                    if ((check || !crossed) && flagged)
                    {
                        Span *const next = &opened[opened_count];
                        next->a = js[lows[at] + u];
                        next->b = js[lows[at] + q];
                        next->sign_a = run[u].sign;
                        next->sign_b = run[q].sign;
                        next->s_a = run[u].s;
                        next->s_b = run[q].s;
                        next->counted = span->counted;
                        opened_count += 1u;
                    }
                    else if (crossed)
                    {
                        take_zero(cell, span->counted, &run[u].s, &run[q].s, lift, &added);
                    }
                }
                previous = (int64_t)q;
            }
            platform_cell_released(run, platform_place_g_program_owner);
        }
        platform_cell_released(open, platform_place_g_program_owner);
        open = opened;
        open_count = opened_count;
        platform_cell_released(rows, platform_place_g_program_owner);
        platform_cell_released(js, platform_place_g_program_owner);
        platform_cell_released(lows, platform_place_g_program_owner);
        platform_cell_released(highs, platform_place_g_program_owner);
    }
    for (size_t at = 0u; at < open_count; at += 1u)
    {
        if (!orior_exact_equal(&open[at].sign_a, &open[at].sign_b))
        {
            take_zero(cell, open[at].counted, &open[at].s_a, &open[at].s_b, p - cell->p, &added);
        }
    }
    results[0] = listed;
    results[1] = added;
    // The spans still open fit a machine integer.
    results[2] = (int64_t)open_count;
    results[3] = failed;
    platform_cell_released(open, platform_place_g_program_owner);
}

/* ---------------------------------------------------------------- the choice of the lattice */

/**
 * @brief The lattice of each cell from one local cost, its every term a field measured on the run.
 *
 * @note A cell at 2^p points holds r = 2^p / Z points a zero, with Z the rise of theta / pi across it. Two zeros
 *       closer than a step hide between the same two points. With the spacing of the GUE, small gaps g at density
 *       (pi^2 / 3) g^2, the missed zeros number mu = c Z / r^3, with c = pi^2 / 18.
 * @note A cell that misses one runs again. The cost to close it from p is C(p) = T(p) + (1 - exp(-mu(p))) C(q), with
 *       q the next lattice up. T(p) is the time that a run at 2^p points takes. The choice of p is the least C, moved
 *       at most one a cell.
 * @note The code takes c as the ratio of the missed zeros to the sum of Z / r^3 over the measured cells. It starts
 *       from the value of the GUE, with the weight of four cells of misses. T(p) = f + a 2^p, with f and a fit by
 *       least squares to the timed runs.
 * @note The value f is 2 s, and a comes from the first run, until there are times for two lattices. None of it
 *       touches the proof: the count certifies whatever lattice a cell runs on.
 */
typedef struct
{
    double missed;     /**< The missed zeros. */
    double exposure;   /**< The sum of Z^4 / 8^p. */
    double sizes[512]; /**< The points of each timed run. */
    double times[512]; /**< The seconds of each timed run. */
    size_t timed;      /**< How many timed runs. */
    int64_t last_p;    /**< The lattice of the last cell, or -1. */
} Control;

/** @brief The weight of the prior, in cells, and the fixed cost of a run, in seconds. */
#define CONTROL_PRIOR 4.0
#define CONTROL_FIXED 2.0

/**
 * @brief c, the ratio of the missed zeros to their exposure.
 *
 * @param[in] control The control [BORROWS].
 * @return            c.
 */
static double control_c(const Control *control)
{
    const double prior = (3.14159265 * 3.14159265) / 18.0;
    return ((prior * CONTROL_PRIOR) + control->missed) / (CONTROL_PRIOR + control->exposure);
}

/**
 * @brief The time of a run at 2^p points, from the fit.
 *
 * @param[in] control The control [BORROWS].
 * @param[in] p       The bits.
 * @return            The seconds.
 */
static double control_cost(const Control *control, int64_t p)
{
    double f = CONTROL_FIXED;
    double a = 1e-6;
    int distinct = 0;
    for (size_t at = 1u; at < control->timed; at += 1u)
    {
        distinct = distinct || (control->sizes[at] != control->sizes[0]);
    }
    if (distinct)
    {
        double n_bar = 0.0;
        double s_bar = 0.0;
        for (size_t at = 0u; at < control->timed; at += 1u)
        {
            n_bar += control->sizes[at];
            s_bar += control->times[at];
        }
        n_bar /= (double)control->timed;
        s_bar /= (double)control->timed;
        double top = 0.0;
        double bottom = 0.0;
        for (size_t at = 0u; at < control->timed; at += 1u)
        {
            top += (control->sizes[at] - n_bar) * (control->times[at] - s_bar);
            bottom += (control->sizes[at] - n_bar) * (control->sizes[at] - n_bar);
        }
        a = (top / bottom > 0.0) ? (top / bottom) : 0.0;
        f = ((s_bar - (a * n_bar)) > 0.0) ? (s_bar - (a * n_bar)) : 0.0;
    }
    else if (control->timed > 0u)
    {
        const double n = control->sizes[control->timed - 1u];
        const double s = control->times[control->timed - 1u];
        f = (CONTROL_FIXED < s) ? CONTROL_FIXED : s;
        a = (s - f) / n;
    }
    return f + (a * (double)((int64_t)1 << p));
}

/**
 * @brief C(p): the time to close cell nu from 2^p points, with the reruns that it forces on average.
 *
 * @param[in] control The control [BORROWS].
 * @param[in] nu      The cell.
 * @param[in] p       The bits.
 * @return            The seconds.
 */
static double control_close_cost(const Control *control, int64_t nu, int64_t p)
{
    const double z = turing_rises(nu);
    const double c = control_c(control);
    double total = control_cost(control, TURING_FINEST);
    for (int64_t q = TURING_FINEST - 1; q >= p; q -= 1)
    {
        const double mu = (c * pow(z, 4.0)) / pow(8.0, (double)q);
        total = control_cost(control, q) + ((1.0 - exp(-mu)) * total);
    }
    return total;
}

/**
 * @brief The p of least C for cell nu, past above where given, within one of the lattice of the last cell.
 *
 * @param[in,out] control The control [BORROWS].
 * @param[in]     nu      The cell.
 * @param[in]     above   The lattice to pass, or -1.
 * @return                The bits.
 */
static int64_t control_choose(Control *control, int64_t nu, int64_t above)
{
    const int64_t low = (above < 0) ? TURING_COARSEST : (above + 1);
    // The bits of a lattice are small.
    const int64_t least = (int64_t)ceil(log2(turing_rises(nu)));
    int64_t best = -1;
    double best_cost = 0.0;
    for (int64_t p = (low > least) ? low : least; p <= TURING_FINEST; p += 1)
    {
        const double cost = control_close_cost(control, nu, p);
        if ((best < 0) || (cost < best_cost))
        {
            best = p;
            best_cost = cost;
        }
    }
    if ((above < 0) && (control->last_p >= 0))
    {
        best = (best > (control->last_p - 1)) ? best : (control->last_p - 1);
        best = (best < (control->last_p + 1)) ? best : (control->last_p + 1);
    }
    if (above < 0)
    {
        control->last_p = best;
    }
    return best;
}

/* ---------------------------------------------------------------- the machine */

/** @brief The cells of a run of the machine, by nu, with the shared inputs of each fetch. */
typedef struct
{
    const char *binary;               /**< The device program. */
    const TuringConstants *constants; /**< The constants. */
    int method;                       /**< The method, or -1 for refine. */
    int check;                        /**< 1 to check the crossings. */
    Control *control;                 /**< The control, or NULL. */
    TuringCell *cells;                /**< The cells, by nu [OWNS]. */
    int *present;                     /**< Which cells are present [OWNS]. */
    OriorExactInteger bound;          /**< The bound of the last fetch. */
} Machine;

/**
 * @brief Cell nu at 2^p points by the method asked, into the machine.
 *
 * @param[in,out] machine The machine [BORROWS].
 * @param[in]     nu      The cell.
 * @param[in]     p       The bits.
 * @note With refine, it runs by the multiple evaluation with the twist. It then certifies the flagged spans again at
 *       the finer lattice by the listed points. With check, it certifies the crossings too.
 */
static void fetch(Machine *machine, int64_t nu, int64_t p)
{
    const double began = seconds_now();
    TuringCell *const cell = &machine->cells[nu];
    if (machine->method >= 0)
    {
        turing_run_cell(machine->binary, machine->constants, nu, p, machine->method, 0, 0, cell, &machine->bound);
        (void)remove(cell->path);
    }
    else
    {
        turing_run_cell(machine->binary, machine->constants, nu, p, TURING_TRANSFORM, 0, 2, cell, &machine->bound);
        Span *spans = NULL;
        int64_t *counters = NULL;
        const size_t count = flagged_spans(cell, machine->check, &spans, &counters);
        (void)remove(cell->path);
        int64_t results[4];
        refine(machine->binary, machine->constants, cell, spans, count, machine->check, results);
        cell->failed += results[3];
        // The count of spans fits a machine integer.
        cell->refined[0] = (int64_t)count;
        cell->refined[1] = results[0];
        cell->refined[2] = results[1];
        cell->refined[3] = results[2];
        cell->refined[4] = z_small(&cell->margins[2]);
        cell->refined[5] = z_small(&cell->sums[SUM_ZEROS_AFTER]);
        platform_cell_released(spans, platform_place_g_program_owner);
        platform_cell_released(counters, platform_place_g_program_owner);
    }
    machine->present[nu] = 1;
    if ((machine->control != NULL) && (machine->control->timed < 512u))
    {
        machine->control->sizes[machine->control->timed] = (double)((int64_t)1 << p);
        machine->control->times[machine->control->timed] = seconds_now() - began;
        machine->control->timed += 1u;
    }
}

/**
 * @brief The ceiling of the lower bound and the floor of the upper bound of N at the F of cell nu.
 *
 * @param[in]  machine The machine [BORROWS].
 * @param[in]  nu      The cell.
 * @param[in]  ahead   The cell after, or NULL [BORROWS].
 * @param[out] window  The two bounds [BORROWS].
 */
static void window_at(const Machine *machine, int64_t nu, const TuringCell *ahead, int64_t window[2])
{
    ZetaRational low;
    ZetaRational high;
    turing_below(&machine->cells[nu], &machine->cells[nu - 1], &low);
    turing_above(&machine->cells[nu], ahead, &high);
    window[0] = whole(&low, 1);
    window[1] = whole(&high, 0);
}

/**
 * @brief The zeros that cell nu misses, from N certified at its F and at the F of the next cell: 0 where it closes.
 *
 * @param[in] machine The machine [BORROWS].
 * @param[in] nu      The cell.
 * @return            The shortfall.
 */
static int64_t shortfall(const Machine *machine, int64_t nu)
{
    int64_t here[2];
    int64_t next[2];
    window_at(machine, nu, &machine->cells[nu + 1], here);
    window_at(machine, nu + 1, &machine->cells[nu + 2], next);
    const int64_t missing = next[1] - here[0] - turing_between(&machine->cells[nu], &machine->cells[nu + 1]);
    return (missing > 0) ? missing : 0;
}

/**
 * @brief Every zero up to T_a, the F of cell last + 1, by Euler-Maclaurin over cells first to last.
 *
 * @param[in] binary The device program [BORROWS].
 * @param[in] first  The first cell.
 * @param[in] last   The last cell.
 * @param[in] rate   The points a zero.
 * @return           The exit code: 0 where every zero up to T_a is on the line and simple.
 * @note The method of Turing certifies N(T_a) over cells last, last + 1 and last + 2 by Riemann-Siegel, and the
 *       zeros go against it. In this walk by dead reckoning, the Z of each point is a fix, and its bound is the error
 *       about it. A point that clears its bound is a certified sign.
 * @note Where the count falls short, the search widens in some cells. These are each cell with a mark of two points
 *       without a certificate, or every cell where none has one. The wider search uses four times the points and ten
 *       more Bernoulli terms, with N to match. It goes on until the count closes or the rounds run out.
 * @note Count the certified sign changes in (t at the F of cell first, T_a]. If they reach N(T_a), each is a zero on
 *       the line and simple. None lies below, because the count is at most N(T_a) less N at the start.
 */
static int main_em(const char *binary, int64_t first, int64_t last, int64_t rate)
{
    TuringConstants constants;
    turing_constants(&constants);
    TuringCell *const above_cells =
        (TuringCell *)machine_memory(platform_cell_cleared(3u, sizeof(TuringCell), platform_place_g_program_owner, 0));
    OriorExactInteger bound;
    for (int64_t at = 0; at < 3; at += 1)
    {
        const int64_t nu = last + at;
        turing_run_cell(binary, &constants, nu, turing_lattice(nu, rate), TURING_PAIRS, 0, 0, &above_cells[at], &bound);
        (void)remove(above_cells[at].path);
    }
    const TuringCell *const target = &above_cells[1];
    ZetaRational value;
    turing_below(target, &above_cells[0], &value);
    const int64_t low = whole(&value, 1);
    turing_above(target, &above_cells[2], &value);
    const int64_t high = whole(&value, 0);
    OriorExactInteger x2;
    turing_x2(target, AT_FIRST, &x2);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  T_a = ");
        mmgr_line_fixed(output_line, (double)(turing_t_of(&x2)), 6u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, ", cell ");
        mmgr_line_decimal_signed(output_line, (long long)(last + 1));
        mmgr_line_put(output_line, "'s F by Riemann-Siegel: ");
        mmgr_line_decimal_signed(output_line, (long long)low);
        mmgr_line_put(output_line, " <= N(T_a) <= ");
        mmgr_line_decimal_signed(output_line, (long long)high);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    platform_line_flushed();
    TuringCell *const cells = (TuringCell *)machine_memory(
        platform_cell_cleared((size_t)last + 2u, sizeof(TuringCell), platform_place_g_program_owner, 0));
    int64_t *const levels = (int64_t *)machine_memory(
        platform_cell_cleared((size_t)last + 2u, sizeof(int64_t), platform_place_g_program_owner, 0));
    for (int64_t nu = first; nu <= last; nu += 1)
    {
        turing_run_cell(binary, &constants, nu, turing_lattice(nu, rate), TURING_EM, 0, 0, &cells[nu], &bound);
        (void)remove(cells[nu].path);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  cell ");
            mmgr_line_decimal_signed(output_line, (long long)nu);
            mmgr_line_put(output_line, " at 2^");
            mmgr_line_decimal_signed(output_line, (long long)cells[nu].p);
            mmgr_line_put(output_line, " points by Euler-Maclaurin, N = ");
            mmgr_line_decimal_signed(output_line, (long long)turing_em_heads(nu, turing_em_terms(0)));
            mmgr_line_put(output_line, ", M = ");
            mmgr_line_decimal_signed(output_line, (long long)turing_em_terms(0));
            mmgr_line_put(output_line, ": bound on Z ");
            mmgr_line_exponent(output_line, (double)(over_unit(&bound)), 3u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, ", ");
            mmgr_line_decimal_signed(output_line, (long long)z_small(&cells[nu].sums[SUM_ZEROS_AFTER]));
            mmgr_line_put(output_line, " zeros past F, ");
            mmgr_line_decimal_signed(output_line, (long long)z_small(&cells[nu].sums[SUM_LOOSE]));
            mmgr_line_put(output_line, " loose");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        platform_line_flushed();
    }
    control_em(&cells[first], NULL, NULL);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line,
                      "  steps of the pole, point, pair, verdict and count programs, then Euler-Maclaurin's: ");
        platform_line_out_ended();
    }
    print_steps(&cells[first]);
    OriorExactInteger rs_bound;
    OriorExactInteger theta_bound;
    turing_bounds(last, &constants, cells[last].p, TURING_PAIRS, &rs_bound, &theta_bound);
    OriorExactInteger em_bound;
    turing_em_bounds(last, turing_em_heads(last, turing_em_terms(0)), turing_em_terms(0), &em_bound, &theta_bound);
    OriorExactInteger both;
    z_add(&rs_bound, &em_bound, &both);
    control_em(&cells[last], &above_cells[0], &both);
    platform_line_flushed();
    int64_t count = 0;
    for (int64_t round = 0; round <= TURING_ROUNDS; round += 1)
    {
        count = 0;
        for (int64_t nu = first; nu < last; nu += 1)
        {
            count += turing_between(&cells[nu], &cells[nu + 1]);
        }
        count += turing_between(&cells[last], target);
        OriorExactInteger start;
        turing_x2(&cells[first], AT_FIRST, &start);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  round ");
            mmgr_line_decimal_signed(output_line, (long long)round);
            mmgr_line_put(output_line, ": ");
            mmgr_line_decimal_signed(output_line, (long long)count);
            mmgr_line_put(output_line, " sign changes certified in (");
            mmgr_line_fixed(output_line, (double)(turing_t_of(&start)), 6u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, ", ");
            mmgr_line_fixed(output_line, (double)(turing_t_of(&x2)), 6u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, "], N(T_a) ");
            mmgr_line_decimal_signed(output_line, (long long)low);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        platform_line_flushed();
        if ((count >= low) || (round == TURING_ROUNDS))
        {
            break;
        }
        int any_loose = 0;
        for (int64_t nu = first; nu <= last; nu += 1)
        {
            any_loose = any_loose || (cells[nu].sums[SUM_LOOSE].sign > 0);
        }
        for (int64_t nu = first; nu <= last; nu += 1)
        {
            if (any_loose && (cells[nu].sums[SUM_LOOSE].sign <= 0))
            {
                continue;
            }
            if ((cells[nu].p + 2) <= TURING_FINEST)
            {
                levels[nu] += 1;
                turing_run_cell(binary, &constants, nu, cells[nu].p + 2, TURING_EM, levels[nu], 0, &cells[nu], &bound);
                (void)remove(cells[nu].path);
            }
        }
    }
    int64_t failed = 0;
    for (int64_t nu = first; nu <= last; nu += 1)
    {
        failed += cells[nu].failed;
    }
    for (int at = 0; at < 3; at += 1)
    {
        failed += above_cells[at].failed;
    }
    const int proven = (count == high) && (failed == 0);
    if (proven)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  every zero in (0, ");
        mmgr_line_fixed(output_line, (double)(turing_t_of(&x2)), 6u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, "] is on the line and simple: ");
        mmgr_line_decimal_signed(output_line, (long long)count);
        mmgr_line_put(output_line, "; ");
        mmgr_line_decimal_signed(output_line, (long long)failed);
        mmgr_line_put(output_line, " host checks failed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    else
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the count does not close; ");
        mmgr_line_decimal_signed(output_line, (long long)failed);
        mmgr_line_put(output_line, " host checks failed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    platform_cell_released(cells, platform_place_g_program_owner);
    platform_cell_released(levels, platform_place_g_program_owner);
    platform_cell_released(above_cells, platform_place_g_program_owner);
    return proven ? 0 : 1;
}

/**
 * @brief The machine over cells first to last by a method of the main sum, or by refine.
 *
 * @param[in] binary The device program [BORROWS].
 * @param[in] first  The first cell.
 * @param[in] last   The last cell.
 * @param[in] rate   The points a zero, or 0 for the control of the lattice.
 * @param[in] method The method, or -1 for refine.
 * @param[in] check  1 to check the crossings.
 * @return           The exit code: 0 where every zero in (T_a, T_b] is on the line and simple.
 */
static int main_machine(const char *binary, int64_t first, int64_t last, int64_t rate, int method, int check)
{
    TuringConstants constants;
    turing_constants(&constants);
    Control control_state;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &control_state, .bytes = sizeof(control_state), .val = 0);
    control_state.last_p = -1;
    Machine machine;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &machine, .bytes = sizeof(machine), .val = 0);
    machine.binary = binary;
    machine.constants = &constants;
    machine.method = method;
    machine.check = check;
    machine.control = (rate == 0) ? &control_state : NULL;
    machine.cells = (TuringCell *)machine_memory(
        platform_cell_cleared((size_t)last + 3u, sizeof(TuringCell), platform_place_g_program_owner, 0));
    machine.present =
        (int *)machine_memory(platform_cell_cleared((size_t)last + 3u, sizeof(int), platform_place_g_program_owner, 0));
    for (int64_t nu = first; nu <= last; nu += 1)
    {
        const int64_t p =
            (machine.control != NULL) ? control_choose(machine.control, nu, -1) : turing_lattice(nu, rate);
        fetch(&machine, nu, p);
        if ((machine.control != NULL) && ((nu - 3) >= first))
        {
            const int64_t m = nu - 2;
            machine.control->missed += (double)shortfall(&machine, m);
            const double z = turing_rises(m);
            machine.control->exposure += pow(z, 4.0) / pow(8.0, (double)machine.cells[m].p);
        }
        const TuringCell *const cell = &machine.cells[nu];
        if (nu == first)
        {
            control(cell, constants.curve_count);
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(
                    output_line,
                    "  steps of the pole, point, pair, verdict and count programs, then the multiple evaluation's: ");
                platform_line_out_ended();
            }
            print_steps(cell);
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  cell ");
            mmgr_line_decimal_signed(output_line, (long long)nu);
            mmgr_line_put(output_line, " at 2^");
            mmgr_line_decimal_signed(output_line, (long long)cell->p);
            mmgr_line_put(output_line, " points: bound on Z ");
            mmgr_line_exponent(output_line, (double)(over_unit(&machine.bound)), 3u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, ", F ");
            mmgr_line_decimal_signed(output_line, (long long)cell->first);
            mmgr_line_put(output_line, ", ");
            mmgr_line_decimal_signed(output_line, (long long)z_small(&cell->sums[SUM_ZEROS_AFTER]));
            mmgr_line_put(output_line, " zeros past F, ");
            mmgr_line_decimal_signed(output_line, (long long)z_small(&cell->sums[SUM_LOOSE]));
            mmgr_line_put(output_line, " loose");
            platform_line_out_ended();
        }
        if (machine.control != NULL)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "; c ");
            mmgr_line_fixed(output_line, (double)(control_c(machine.control)), 3u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, ", a run ");
            mmgr_line_fixed(output_line, (double)(machine.control->times[machine.control->timed - 1u]), 2u, 0u,
                            EMBED_FALSE);
            mmgr_line_put(output_line, " s at 2^");
            mmgr_line_decimal_signed(output_line, (long long)cell->p);
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        if (method < 0)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    ");
            mmgr_line_decimal_signed(output_line, (long long)cell->refined[0]);
            mmgr_line_put(output_line, " spans flagged, ");
            mmgr_line_decimal_signed(output_line, (long long)cell->refined[1]);
            mmgr_line_put(output_line, " points listed, ");
            mmgr_line_decimal_signed(output_line, (long long)cell->refined[2]);
            mmgr_line_put(output_line, " zeros found in them, ");
            mmgr_line_decimal_signed(output_line, (long long)cell->refined[3]);
            mmgr_line_put(output_line, " spans still open; ");
            mmgr_line_decimal_signed(output_line, (long long)cell->refined[4]);
            mmgr_line_put(output_line, " crossings proven single on the coarse lattice, of ");
            mmgr_line_decimal_signed(output_line, (long long)cell->refined[5]);
            mmgr_line_put(output_line, " zeros past F");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        if (cell->has_apart)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    the transform's Z and the pairs' differ by ");
            mmgr_line_exponent(output_line, (double)(over_unit(&cell->apart)), 3u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, " at max, at point ");
            mmgr_line_decimal_signed(output_line, (long long)cell->apart_at);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        platform_line_flushed();
    }
    int64_t (*const window)[2] = (int64_t (*)[2])machine_memory(
        platform_cell_cleared((size_t)last + 3u, sizeof(int64_t[2]), platform_place_g_program_owner, 0));
    int64_t count = 0;
    for (int64_t round = 0; round <= TURING_ROUNDS; round += 1)
    {
        for (int64_t nu = first + 1; nu <= last; nu += 1)
        {
            window_at(&machine, nu, (nu < last) ? &machine.cells[nu + 1] : NULL, window[nu]);
        }
        int64_t loose_count = 0;
        for (int64_t nu = first + 1; nu <= last; nu += 1)
        {
            loose_count += window[nu][0] != window[nu][1];
        }
        int64_t short_list[4096];
        size_t short_count = 0u;
        count = 0;
        for (int64_t nu = first + 1; nu < last; nu += 1)
        {
            const int64_t between = turing_between(&machine.cells[nu], &machine.cells[nu + 1]);
            if (((window[nu + 1][1] - window[nu][0]) > between) && (short_count < 4096u))
            {
                short_list[short_count] = nu;
                short_count += 1u;
            }
            count += between;
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  round ");
            mmgr_line_decimal_signed(output_line, (long long)round);
            mmgr_line_put(output_line, ": ");
            mmgr_line_decimal_signed(output_line, (long long)count);
            mmgr_line_put(output_line, " zeros certified, ");
            mmgr_line_decimal(output_line, (unsigned long long)short_count, 1u);
            mmgr_line_put(output_line, " cells short [");
            platform_line_out_ended();
        }
        for (size_t at = 0u; (at < short_count) && (at < 12u); at += 1u)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, (at > 0u) ? ", " : "");
            mmgr_line_decimal_signed(output_line, (long long)short_list[at]);
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "], ");
            mmgr_line_decimal_signed(output_line, (long long)loose_count);
            mmgr_line_put(output_line, " points where N is not certified to one value");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        platform_line_flushed();
        if ((short_count == 0u) || (round == TURING_ROUNDS))
        {
            break;
        }
        unsigned char *const coarse = (unsigned char *)machine_memory(
            platform_cell_cleared((size_t)last + 3u, 1u, platform_place_g_program_owner, 0));
        for (size_t at = 0u; at < short_count; at += 1u)
        {
            coarse[short_list[at]] = 1u;
        }
        for (int64_t nu = first + 1; nu <= last; nu += 1)
        {
            if (window[nu][0] == window[nu][1])
            {
                continue;
            }
            for (int64_t m = nu - 1; m <= (nu + 1); m += 1)
            {
                if ((m >= first) && (m <= last))
                {
                    coarse[m] = 1u;
                }
            }
        }
        for (int64_t nu = first; nu <= last; nu += 1)
        {
            if (!coarse[nu])
            {
                continue;
            }
            const int64_t p = machine.cells[nu].p;
            if (((p + 2) <= TURING_FINEST) && (machine.control == NULL))
            {
                fetch(&machine, nu, p + 2);
            }
            else if ((machine.control != NULL) && (p < TURING_FINEST))
            {
                fetch(&machine, nu, control_choose(machine.control, nu, p));
            }
        }
        platform_cell_released(coarse, platform_place_g_program_owner);
    }
    int64_t failed = 0;
    for (int64_t nu = first; nu <= last; nu += 1)
    {
        failed += machine.cells[nu].failed;
    }
    OriorExactInteger x2;
    turing_x2(&machine.cells[first + 1], AT_FIRST, &x2);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  T_a = ");
        mmgr_line_fixed(output_line, (double)(turing_t_of(&x2)), 6u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal_signed(output_line, (long long)window[first + 1][0]);
        mmgr_line_put(output_line, " <= N(T_a) <= ");
        mmgr_line_decimal_signed(output_line, (long long)window[first + 1][1]);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    turing_x2(&machine.cells[last], AT_FIRST, &x2);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  T_b = ");
        mmgr_line_fixed(output_line, (double)(turing_t_of(&x2)), 6u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal_signed(output_line, (long long)window[last][0]);
        mmgr_line_put(output_line, " <= N(T_b) <= ");
        mmgr_line_decimal_signed(output_line, (long long)window[last][1]);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    const int64_t reach = window[last][1] - window[first + 1][0];
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  zeros certified in (T_a, T_b]: ");
        mmgr_line_decimal_signed(output_line, (long long)count);
        mmgr_line_put(output_line, "; N(T_b) - N(T_a) <= ");
        mmgr_line_decimal_signed(output_line, (long long)reach);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    const int proven = (reach <= count) && (failed == 0);
    if (proven)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  every zero in (T_a, T_b] is on the line and simple: ");
        mmgr_line_decimal_signed(output_line, (long long)count);
        mmgr_line_put(output_line, "; ");
        mmgr_line_decimal_signed(output_line, (long long)failed);
        mmgr_line_put(output_line, " host checks failed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    else
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the count does not close; ");
        mmgr_line_decimal_signed(output_line, (long long)failed);
        mmgr_line_put(output_line, " host checks failed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    platform_cell_released(window, platform_place_g_program_owner);
    platform_cell_released(machine.cells, platform_place_g_program_owner);
    platform_cell_released(machine.present, platform_place_g_program_owner);
    return proven ? 0 : 1;
}

int program_main(int argument_count, char **arguments)
{
#ifdef _WIN32
    // The report ends each line with a line feed alone, on every platform.
    (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
    if (argument_count < 2)
    {
        fprintf(stderr, "usage: exact_zeta_turing <binary> [first] [last] [rate] [method] [check]\n");
        return 2;
    }
    const char *const binary = arguments[1];
    const int em =
        (argument_count > 5) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = arguments[5],
                                            .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "em", .ci = EMBED_FALSE));
    if (em)
    {
        const int64_t first = (argument_count > 2) ? atoll(arguments[2]) : 1;
        const int64_t last = (argument_count > 3) ? atoll(arguments[3]) : 10;
        const int64_t rate = (argument_count > 4) ? atoll(arguments[4]) : 4;
        if ((first < 1) || ((2 * last * last) <= 168))
        {
            fprintf(stderr, "  Euler-Maclaurin runs from cell 1 up, and its last cell must start past t = 168 pi\n");
            return 1;
        }
        return main_em(binary, first, last, rate);
    }
    const int64_t first = (argument_count > 2) ? atoll(arguments[2]) : 10;
    const int64_t last = (argument_count > 3) ? atoll(arguments[3]) : 20;
    const int64_t rate = (argument_count > 4) ? atoll(arguments[4]) : 4;
    const char *const name = (argument_count > 5) ? arguments[5] : "pairs";
    const int check =
        (argument_count > 6) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = arguments[6],
                                            .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "check", .ci = EMBED_FALSE));
    static const char *const names[5] = {"pairs", "transform", "both", "em", "points"};
    int method = -2;
    for (int at = 0; at < 5; at += 1)
    {
        method = (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = names[at], .ci = EMBED_FALSE))
                     ? at
                     : method;
    }
    method = (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "refine",
                         .ci = EMBED_FALSE))
                 ? -1
                 : method;
    if (((2 * first * first) <= 168) || (last < (first + 2)) || (method == -2))
    {
        fprintf(stderr, "  the first cell must start past t = 168 pi, at nu = 10 or above, three cells are the least, "
                        "and the method is one of pairs, transform, both, em, points or refine\n");
        return 1;
    }
    return main_machine(binary, first, last, rate, method, check);
}

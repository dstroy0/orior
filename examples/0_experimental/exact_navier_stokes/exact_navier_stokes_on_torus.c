// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Catalog: EXP-x-016
/**
 * @file exact_navier_stokes_on_torus.c
 * @brief The Navier-Stokes equations on the unit torus, in exact integer arithmetic, over the sets of Fefferman.
 *
 * @note This shows an exact solution that two routes reproduce. It shows the horizon that a generic datum runs
 *       past. It shows the two removals (viscosity, force) on an instance, where each changes the solution. It
 *       shows the countable island of fields that exact arithmetic can name, inside the uncountable data class.
 *       It claims nothing about any open problem. It presents the rigor and stops.
 * @note Run build_exact_navier_stokes.sh, beside this file. It builds this program into build/exact_navier_stokes
 *       and runs it.
 * @note This reads no corpus. It sits in 0_experimental: an arithmetic result that runs, in place of a stage
 *       reading.
 * @note The sets are those of Fefferman, read from the Clay statement. They are equations (1) momentum, (2)
 *       divergence and (3) initial datum. They are also (8) periodic datum and force, with the periodic
 *       pressure of the erratum, and (10) periodic solution. Last are (11) smooth pressure and velocity, and (7)
 *       bounded energy. On the torus, (7) is the sum of Parseval over the unit cell. The period is 1, along the
 *       unit vectors e_j.
 * @note torus_fields.h states the ring. Every integer is an exact integer of includes/exact_types. The decimal
 *       viscosity is a numerator at a count of decimal places. Nothing rounds anywhere.
 * @note A derivative multiplies a mode by 2 pi i k_j. A product convolves modes: a linear convolution on Z^3,
 *       with no alias. The Laplacian multiplies by -4 pi^2 |k|^2. The Leray projection removes the gradient part,
 *       mode by mode.
 * @note Every element is smooth and periodic. (8), (10) and (11) hold by construction. (2) is an
 *       exact identity. The pressure is in the same ring, and thus periodic. The energy is an exact Laurent
 *       polynomial in pi over an integer.
 * @note The solution is a list of Taylor coefficients in t at t = 0: u(t) = sum_m u_m t^m / m!. The recurrence
 *       for them is u_(m+1) = nu Laplacian u_m - P sum_(j=0..m) C(m,j) (u_j . grad) u_(m-j). The pressure p_m
 *       comes from the same sum. The recurrence is the derivative of order m of (1) at t = 0, with P to enforce
 *       (2).
 * @note Positive control: the Arnold-Beltrami-Childress field u_0, with three components.
 *
 *           A sin 2 pi z + C cos 2 pi y,   B sin 2 pi x + A cos 2 pi z,   C sin 2 pi y + B cos 2 pi x
 *
 *       It is Beltrami: curl u_0 = 2 pi u_0. Its nonlinear term is a pure gradient. Thus
 *       u = e^(-4 nu pi^2 t) u_0 and p = -e^(-8 nu pi^2 t) |u_0|^2 / 2 solve (1), (2) and (3) exactly. The
 *       recurrence must return u_m = (-4 nu pi^2)^m u_0 and p_m = (-8 nu pi^2)^m p_0 to the integer, and it does.
 * @note There are two routes. The first is the velocity recurrence with the pressure. The second is the
 *       vorticity recurrence, below.
 *
 *           omega_(m+1) = nu Laplacian omega_m
 *                         - sum_j C(m,j) [ (u_j . grad) omega_(m-j) - (omega_j . grad) u_(m-j) ]
 *
 *       The second route recovers u_j from omega_j by Biot-Savart, with no pressure anywhere. The two meet at
 *       curl u_m = omega_m at every order, on the exact solution and on a generic datum.
 * @note The null: without the term of vortex stretch, (omega . grad) u, the equation takes its two-dimensional
 *       form, and the agreement breaks in three dimensions. A wrong pressure sign leaves a nonzero residual on
 *       the exact solution.
 * @note Floor: none in the arithmetic. The boundary is the horizon. The Taylor coefficients of a generic datum
 *       occupy more modes at every order. Thus a cut at any fixed mode radius misses some order. This measures
 *       the count that it misses, and sets no bound on it.
 * @note The forms belong to the field, and this file only runs them exactly. The problem statement, its
 *       numbered conditions and its four alternatives are by Charles Fefferman, for the Clay Mathematics
 *       Institute, read in full with the errata. The equations are those of Navier and Stokes, and the inviscid
 *       case is that of Euler. The field with curl u proportional to u is that of Beltrami. The example with
 *       three terms is that of Arnold (1965) and Childress (1970), from memory of the literature and not read
 *       here. The file checks the solution itself, and does not rest on the citation.
 * @note The projection onto divergence-free fields, and the pressure that it defines, are those of Leray. The
 *       recovery of a velocity from its vorticity is the law of Biot and Savart. The vorticity equation is that of
 *       Helmholtz.
 * @note The energy sum over modes is that of Parseval. The transcendence of pi is that of Lindemann
 *       (1882). It makes the zero test exact. The recurrence for the Taylor coefficients is the classical
 *       power-series method of Cauchy and Kovalevskaya.
 * @note The statement of Fefferman quotes the criterion of Beale, Kato and Majda. The workbook beside this file
 *       names it, and this file does not use it. Nothing in this file is new mathematics.
 */
#include "torus_fields.h"
#include "../../../includes/platform/platform.h"

#include <stdlib.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/** @brief The Taylor coefficients u_0 .. u_ORDER. */
#define ORDER 4u

/** @brief The viscosity, 0.1, as the decimal text reads it. */
static const Viscosity VISCOSITY = {1, 1u};

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
 * @brief Frees a list of vector fields.
 *
 * @param[in,out] vectors The fields [BORROWS].
 * @param[in]     count   How many.
 */
static void vectors_free(TorusVector *vectors, size_t count)
{
    for (size_t at = 0u; at < count; at += 1u)
    {
        vec_free(&vectors[at]);
    }
}

/**
 * @brief Frees a list of scalar fields.
 *
 * @param[in,out] scalars The fields [BORROWS].
 * @param[in]     count   How many.
 */
static void scalars_free(TorusScalar *scalars, size_t count)
{
    for (size_t at = 0u; at < count; at += 1u)
    {
        scalar_free(&scalars[at]);
    }
}

/**
 * @brief Whether the residual of the equation is zero at every order below a limit.
 *
 * @param[in] velocities The velocity coefficients [BORROWS].
 * @param[in] pressures  The pressure coefficients [BORROWS].
 * @param[in] viscosity  The viscosity.
 * @param[in] orders     The limit.
 * @return               1 where every residual is zero.
 */
static int residuals_zero(const TorusVector *velocities, const TorusScalar *pressures, Viscosity viscosity,
                          unsigned orders)
{
    int all = 1;
    for (unsigned step = 0u; step < orders; step += 1u)
    {
        TorusVector left;
        residual(velocities, pressures, viscosity, step, &left);
        all = all && vec_is_zero(&left);
        vec_free(&left);
    }
    return all;
}

/**
 * @brief Whether curl u_m equals omega_m at every order up to a limit.
 *
 * @param[in] velocities  The velocity coefficients [BORROWS].
 * @param[in] vorticities The vorticity coefficients [BORROWS].
 * @param[in] orders      The highest order.
 * @return                1 where the routes meet at every order.
 */
static int routes_meet_through(const TorusVector *velocities, const TorusVector *vorticities, unsigned orders)
{
    int all = 1;
    for (unsigned step = 0u; step <= orders; step += 1u)
    {
        TorusVector rotation;
        curl(&velocities[step], &rotation);
        all = all && vec_equal(&rotation, &vorticities[step]);
        vec_free(&rotation);
    }
    return all;
}

/**
 * @brief The integral of left . right, times numerator / denominator.
 *
 * @param[in]  left        First field [BORROWS].
 * @param[in]  right       Second field [BORROWS].
 * @param[in]  numerator   The numerator of the factor.
 * @param[in]  denominator The denominator of the factor.
 * @param[out] result      The scaled integral [BORROWS].
 */
static void parseval_scaled(const TorusVector *left, const TorusVector *right, int64_t numerator, int64_t denominator,
                            RingRatio *result)
{
    RingRatio plain;
    parseval(left, right, &plain);
    ratio_scale(&plain, numerator, denominator, result);
    ratio_free(&plain);
}

/**
 * @brief Ten raised to the decimal places of the viscosity.
 *
 * @return The power of ten.
 */
static int64_t viscosity_scale(void)
{
    int64_t scale = 1;
    for (uint32_t place = 0u; place < VISCOSITY.places; place += 1u)
    {
        scale *= 10;
    }
    return scale;
}

/**
 * @brief The ABC field: Beltrami, a gradient nonlinear term, the recurrence against the closed form, two routes.
 *
 * @param[in] out The stream [BORROWS].
 * @return        1 where every check lands.
 */
static int report_exact_solution(FILE *out)
{
    fputs("  positive control: the Arnold-Beltrami-Childress solution, exact to the integer\n", out);
    TorusVector datum;
    abc_field(1, 2, 3, &datum);
    RingRatio rate;
    RingRatio pressure_rate;
    decay_rate(VISCOSITY, -4, &rate);
    decay_rate(VISCOSITY, -8, &pressure_rate);

    const int real_valued = is_real_field(&datum);
    TorusScalar spread;
    divergence(&datum, &spread);
    const int divergence_free = scalar_is_zero(&spread);
    scalar_free(&spread);

    TorusVector rotation;
    curl(&datum, &rotation);
    PiPoly two_pi_poly;
    poly_term(&two_pi_poly, 1, 2, 0);
    RingRatio two_pi;
    OriorExactInteger one;
    big_set(&one, 1);
    ratio_make(&two_pi, &two_pi_poly, &one);
    TorusVector doubled;
    vec_times(&datum, &two_pi, &doubled);
    const int beltrami = vec_equal(&rotation, &doubled);
    vec_free(&rotation);
    vec_free(&doubled);

    TorusVector nonlinear;
    advect(&datum, &datum, &nonlinear);
    TorusScalar speed;
    dot(&datum, &datum, &speed);
    TorusScalar half_speed;
    scalar_scale(&speed, 1, 2, &half_speed);
    scalar_free(&speed);
    TorusVector slope;
    gradient(&half_speed, &slope);
    const int gradient_nonlinearity = vec_equal(&nonlinear, &slope);
    vec_free(&slope);
    TorusVector projected;
    leray(&nonlinear, &projected);
    const int projection_vanishes = vec_is_zero(&projected);
    vec_free(&projected);
    vec_free(&nonlinear);

    TorusVector velocities[ORDER + 1u];
    TorusScalar pressures[ORDER];
    taylor_velocity(&datum, VISCOSITY, ORDER, velocities, pressures);
    int closed_form = 1;
    for (unsigned step = 0u; step <= ORDER; step += 1u)
    {
        RingRatio power;
        ratio_power(&rate, step, &power);
        TorusVector expected;
        vec_times(&datum, &power, &expected);
        closed_form = closed_form && vec_equal(&velocities[step], &expected);
        vec_free(&expected);
        ratio_free(&power);
    }
    TorusScalar negative;
    scalar_neg(&half_speed, &negative);
    TorusScalar closed_pressure;
    scalar_without_mean(&negative, &closed_pressure);
    scalar_free(&negative);
    int pressure_form = 1;
    for (unsigned step = 0u; step < ORDER; step += 1u)
    {
        RingRatio power;
        ratio_power(&pressure_rate, step, &power);
        TorusScalar expected;
        scalar_times(&closed_pressure, &power, &expected);
        pressure_form = pressure_form && scalar_equal(&pressures[step], &expected);
        scalar_free(&expected);
        ratio_free(&power);
    }
    scalar_free(&closed_pressure);
    const int residual_zero = residuals_zero(velocities, pressures, VISCOSITY, ORDER);

    TorusVector datum_vorticity;
    curl(&datum, &datum_vorticity);
    TorusVector vorticities[ORDER + 1u];
    taylor_vorticity(&datum_vorticity, VISCOSITY, ORDER, 1, vorticities);
    const int routes_meet = routes_meet_through(velocities, vorticities, ORDER);
    int recovered = 1;
    for (unsigned step = 0u; step <= ORDER; step += 1u)
    {
        TorusVector back;
        biot_savart(&vorticities[step], &back);
        recovered = recovered && vec_equal(&back, &velocities[step]);
        vec_free(&back);
    }

    // The energy identity at t = 0: d/dt int |u|^2 = -2 nu int |grad u|^2, with (7) as its consequence.
    RingRatio energy;
    parseval(&datum, &datum, &energy);
    RingRatio energy_rate;
    parseval_scaled(&datum, &velocities[1], 2, 1, &energy_rate);
    TorusVector laplacian;
    vec_laplacian(&datum, &laplacian);
    RingRatio dissipation;
    parseval_scaled(&datum, &laplacian, 2 * VISCOSITY.numerator, viscosity_scale(), &dissipation);
    vec_free(&laplacian);
    const int energy_identity = ratio_equal(&energy_rate, &dissipation);

    // The nulls: the wrong pressure sign, and the two-dimensional vorticity equation in three dimensions.
    TorusScalar wrong_pressures[ORDER];
    for (unsigned step = 0u; step < ORDER; step += 1u)
    {
        scalar_neg(&pressures[step], &wrong_pressures[step]);
    }
    TorusVector wrong;
    residual(velocities, wrong_pressures, VISCOSITY, 0u, &wrong);
    const int wrong_sign_caught = !vec_is_zero(&wrong);
    vec_free(&wrong);
    scalars_free(wrong_pressures, ORDER);
    TorusVector flat_vorticities[ORDER + 1u];
    taylor_vorticity(&datum_vorticity, VISCOSITY, ORDER, 0, flat_vorticities);
    TorusVector first_rotation;
    curl(&velocities[1], &first_rotation);
    const int stretching_needed = !vec_equal(&first_rotation, &flat_vorticities[1]);
    vec_free(&first_rotation);

    fprintf(out, "    A, B, C = 1, 2, 3 ; nu = %lld at %u place(s) ; orders 0..%u\n", (long long)VISCOSITY.numerator,
            (unsigned)VISCOSITY.places, ORDER);
    fprintf(out, "    real-valued: %s ; divergence-free (2): %s ; Beltrami curl u = 2 pi u: %s\n", truth(real_valued),
            truth(divergence_free), truth(beltrami));
    fprintf(out, "    (u.grad)u = grad(|u|^2/2): %s ; its Leray projection is zero: %s\n", truth(gradient_nonlinearity),
            truth(projection_vanishes));
    fprintf(out, "    recurrence gives u_m = (-4 nu pi^2)^m u_0 at every order: %s\n", truth(closed_form));
    fprintf(out, "    pressure p_m = (-8 nu pi^2)^m (-|u_0|^2/2) at every order: %s\n", truth(pressure_form));
    fprintf(out, "    residual of (1) with (2) is zero at every order: %s\n", truth(residual_zero));
    fprintf(out, "    vorticity route meets the velocity route, curl u_m = omega_m: %s ; Biot-Savart returns u_m: %s\n",
            truth(routes_meet), truth(recovered));
    fputs("    energy int |u_0|^2 = ", out);
    ratio_print(&energy, out);
    fputs(" ; d/dt energy at 0 = ", out);
    ratio_print(&energy_rate, out);
    fputs(" ; -2 nu int |grad u_0|^2 = ", out);
    ratio_print(&dissipation, out);
    fprintf(out, " ; equal: %s\n", truth(energy_identity));
    fprintf(out, "    null, pressure sign flipped: residual nonzero: %s\n", truth(wrong_sign_caught));
    fprintf(out, "    null, stretching term dropped: routes disagree at order 1: %s\n\n", truth(stretching_needed));

    ratio_free(&energy);
    ratio_free(&energy_rate);
    ratio_free(&dissipation);
    ratio_free(&two_pi);
    ratio_free(&rate);
    ratio_free(&pressure_rate);
    scalar_free(&half_speed);
    vec_free(&datum_vorticity);
    vectors_free(flat_vorticities, ORDER + 1u);
    vectors_free(vorticities, ORDER + 1u);
    vectors_free(velocities, ORDER + 1u);
    scalars_free(pressures, ORDER);
    vec_free(&datum);
    return real_valued && divergence_free && beltrami && gradient_nonlinearity && projection_vanishes && closed_form &&
           pressure_form && residual_zero && routes_meet && recovered && energy_identity && wrong_sign_caught &&
           stretching_needed;
}

/**
 * @brief The largest |k|_1 over the modes of a field. A product of modes adds their vectors.
 *
 * @param[in] vector The field [BORROWS].
 * @return           The reach. It climbs by one per order.
 */
static int64_t mode_reach(const TorusVector *vector)
{
    ModeList list;
    modes_of(vector, &list);
    int64_t largest = 0;
    for (size_t at = 0u; at < list.count; at += 1u)
    {
        const int64_t size = llabs((long long)list.modes[at][0]) + llabs((long long)list.modes[at][1]) +
                             llabs((long long)list.modes[at][2]);
        largest = (size > largest) ? size : largest;
    }
    platform_cell_released(list.modes, platform_place_g_program_owner);
    return largest;
}

/**
 * @brief How many modes a field carries.
 *
 * @param[in] vector The field [BORROWS].
 * @return           The count.
 */
static size_t mode_count(const TorusVector *vector)
{
    ModeList list;
    modes_of(vector, &list);
    const size_t count = list.count;
    platform_cell_released(list.modes, platform_place_g_program_owner);
    return count;
}

/**
 * @brief A generic datum: its Taylor coefficients occupy more modes at every order. This measures the horizon.
 *
 * @param[in] out The stream [BORROWS].
 * @return        1 where every check lands.
 */
static int report_horizon(FILE *out)
{
    fputs("  the horizon: a generic datum's coefficients outrun any fixed mode radius\n", out);
    TorusVector datum;
    generic_field(&datum);
    TorusScalar spread;
    divergence(&datum, &spread);
    const int divergence_free = scalar_is_zero(&spread);
    scalar_free(&spread);

    TorusVector rotation;
    curl(&datum, &rotation);
    PiPoly two_pi_poly;
    poly_term(&two_pi_poly, 1, 2, 0);
    RingRatio two_pi;
    OriorExactInteger one;
    big_set(&one, 1);
    ratio_make(&two_pi, &two_pi_poly, &one);
    TorusVector doubled;
    vec_times(&datum, &two_pi, &doubled);
    const int not_beltrami = !vec_equal(&rotation, &doubled);
    vec_free(&rotation);
    vec_free(&doubled);
    ratio_free(&two_pi);

    TorusVector nonlinear;
    advect(&datum, &datum, &nonlinear);
    TorusVector projected;
    leray(&nonlinear, &projected);
    const int projection_survives = !vec_is_zero(&projected);
    vec_free(&projected);

    TorusVector velocities[ORDER + 1u];
    TorusScalar pressures[ORDER];
    taylor_velocity(&datum, VISCOSITY, ORDER, velocities, pressures);
    const int residual_zero = residuals_zero(velocities, pressures, VISCOSITY, ORDER);
    TorusVector datum_vorticity;
    curl(&datum, &datum_vorticity);
    TorusVector vorticities[ORDER + 1u];
    taylor_vorticity(&datum_vorticity, VISCOSITY, ORDER, 1, vorticities);
    const int routes_meet = routes_meet_through(velocities, vorticities, ORDER);
    int still_divergence_free = 1;
    int still_real = 1;
    for (unsigned step = 0u; step <= ORDER; step += 1u)
    {
        TorusScalar step_spread;
        divergence(&velocities[step], &step_spread);
        still_divergence_free = still_divergence_free && scalar_is_zero(&step_spread);
        scalar_free(&step_spread);
        still_real = still_real && is_real_field(&velocities[step]);
    }

    // The energy identity on the generic datum: the nonlinear term and the pressure move no energy.
    RingRatio moved;
    parseval(&datum, &nonlinear, &moved);
    const int nonlinear_moves_none = ratio_is_zero(&moved);
    ratio_free(&moved);
    TorusVector pressure_slope;
    gradient(&pressures[0], &pressure_slope);
    parseval(&datum, &pressure_slope, &moved);
    const int pressure_moves_none = ratio_is_zero(&moved);
    ratio_free(&moved);
    vec_free(&pressure_slope);
    RingRatio energy_rate;
    parseval_scaled(&datum, &velocities[1], 2, 1, &energy_rate);
    TorusVector laplacian;
    vec_laplacian(&datum, &laplacian);
    RingRatio dissipation;
    parseval_scaled(&datum, &laplacian, 2 * VISCOSITY.numerator, viscosity_scale(), &dissipation);
    vec_free(&laplacian);
    const int energy_identity = ratio_equal(&energy_rate, &dissipation);
    ratio_free(&energy_rate);
    ratio_free(&dissipation);

    // The null for the identity: a field with divergence, u_0 + grad(cos 2 pi x), moves energy through a pressure.
    TorusScalar wave;
    cosine(0u, 1, 1, &wave);
    TorusVector wave_slope;
    gradient(&wave, &wave_slope);
    scalar_free(&wave);
    TorusVector leaky;
    vec_add(&datum, &wave_slope, &leaky);
    divergence(&leaky, &spread);
    const int leaky_has_divergence = !scalar_is_zero(&spread);
    scalar_free(&spread);
    parseval(&leaky, &wave_slope, &moved);
    const int leaky_moves_energy = !ratio_is_zero(&moved);
    ratio_free(&moved);
    vec_free(&leaky);
    vec_free(&wave_slope);

    int64_t radii[ORDER + 1u];
    int64_t reaches[ORDER + 1u];
    size_t counts[ORDER + 1u];
    for (unsigned step = 0u; step <= ORDER; step += 1u)
    {
        radii[step] = vec_mode_radius(&velocities[step]);
        reaches[step] = mode_reach(&velocities[step]);
        counts[step] = mode_count(&velocities[step]);
    }
    int reach_grows = 1;
    int radius_grows = radii[ORDER] > radii[0];
    for (unsigned step = 0u; step < ORDER; step += 1u)
    {
        reach_grows = reach_grows && (reaches[step + 1u] == (reaches[step] + 1));
        radius_grows = radius_grows && (radii[step + 1u] >= radii[step]);
    }
    int pressure_present = 0;
    for (unsigned step = 0u; step < ORDER; step += 1u)
    {
        pressure_present = pressure_present || !scalar_is_zero(&pressures[step]);
    }

    fprintf(out, "    divergence-free: %s ; not Beltrami: %s ; Leray part of (u.grad)u survives: %s\n",
            truth(divergence_free), truth(not_beltrami), truth(projection_survives));
    fprintf(out, "    residual zero at every order: %s ; routes meet at every order: %s\n", truth(residual_zero),
            truth(routes_meet));
    fprintf(out,
            "    every coefficient divergence-free: %s ; every coefficient real-valued: %s ; a pressure appears: %s\n",
            truth(still_divergence_free), truth(still_real), truth(pressure_present));
    fprintf(out, "    int u.(u.grad)u = 0: %s ; int u.grad p = 0: %s ; energy identity: %s\n",
            truth(nonlinear_moves_none), truth(pressure_moves_none), truth(energy_identity));
    fprintf(out, "    null, a field with divergence: has divergence %s, moves energy through a pressure %s\n",
            truth(leaky_has_divergence), truth(leaky_moves_energy));
    fputs("    order:                 ", out);
    for (unsigned step = 0u; step <= ORDER; step += 1u)
    {
        fprintf(out, "%s%4u", (step > 0u) ? "  " : "", step);
    }
    fputs("\n    largest |k|_1:         ", out);
    for (unsigned step = 0u; step <= ORDER; step += 1u)
    {
        fprintf(out, "%s%4lld", (step > 0u) ? "  " : "", (long long)reaches[step]);
    }
    fputs("\n    largest |k|_inf:       ", out);
    for (unsigned step = 0u; step <= ORDER; step += 1u)
    {
        fprintf(out, "%s%4lld", (step > 0u) ? "  " : "", (long long)radii[step]);
    }
    fputs("\n    modes retained:            ", out);
    for (unsigned step = 0u; step <= ORDER; step += 1u)
    {
        fprintf(out, "%s%4llu", (step > 0u) ? "  " : "", (unsigned long long)counts[step]);
    }
    fputs("\n", out);
    for (int64_t radius = 1; radius <= 2; radius += 1)
    {
        fprintf(out, "    outside |k|_inf <= %lld:  ", (long long)radius);
        for (unsigned step = 0u; step <= ORDER; step += 1u)
        {
            fprintf(out, "%s%4llu", (step > 0u) ? "  " : "",
                    (unsigned long long)vec_modes_outside(&velocities[step], radius));
        }
        fputs("\n", out);
    }
    fprintf(out,
            "    |k|_1 climbs by one per order: %s ; |k|_inf never falls and ends higher: %s (the horizon; no scale "
            "closes it)\n\n",
            truth(reach_grows), truth(radius_grows));

    vec_free(&nonlinear);
    vec_free(&datum_vorticity);
    vectors_free(vorticities, ORDER + 1u);
    vectors_free(velocities, ORDER + 1u);
    scalars_free(pressures, ORDER);
    vec_free(&datum);
    return divergence_free && not_beltrami && projection_survives && residual_zero && routes_meet &&
           still_divergence_free && still_real && nonlinear_moves_none && pressure_moves_none && energy_identity &&
           leaky_has_divergence && leaky_moves_energy && reach_grows && radius_grows;
}

/**
 * @brief Whether every coefficient of one list equals a scale of the matched coefficient of another.
 *
 * @param[in] got      The list to test [BORROWS].
 * @param[in] base     The list to scale [BORROWS].
 * @param[in] factors  The factor at each order [BORROWS].
 * @param[in] orders   The highest order.
 * @return             1 where every order matches.
 */
static int scaled_match(const TorusVector *got, const TorusVector *base, const int64_t *factors, unsigned orders)
{
    int all = 1;
    for (unsigned step = 0u; step <= orders; step += 1u)
    {
        TorusVector expected;
        vec_scale(&base[step], factors[step], 1, &expected);
        all = all && vec_equal(&got[step], &expected);
        vec_free(&expected);
    }
    return all;
}

/**
 * @brief The two exact symmetries: time scaling moves the viscosity, and space scaling keeps the period.
 *
 * @param[in] out The stream [BORROWS].
 * @return        1 where every check lands.
 */
static int report_scaling(FILE *out)
{
    fputs("  symmetries, exact on the coefficients: the viscosity is a parameter a scaling moves\n", out);
    enum
    {
        DEPTH = 3
    };
    TorusVector datum;
    generic_field(&datum);
    TorusVector velocities[DEPTH + 1];
    taylor_velocity(&datum, VISCOSITY, DEPTH, velocities, NULL);

    // v(x,t) = mu u(x, mu t) solves (1)-(3) with viscosity mu nu: coefficients v_m = mu^(m+1) u_m.
    const int64_t mu = 3;
    const Viscosity scaled_viscosity = {mu * VISCOSITY.numerator, VISCOSITY.places};
    TorusVector scaled_datum;
    vec_scale(&datum, mu, 1, &scaled_datum);
    TorusVector scaled[DEPTH + 1];
    taylor_velocity(&scaled_datum, scaled_viscosity, DEPTH, scaled, NULL);
    int64_t time_factors[DEPTH + 1];
    int64_t wrong_factors[DEPTH + 1];
    int64_t power = 1;
    for (unsigned step = 0u; step <= DEPTH; step += 1u)
    {
        wrong_factors[step] = power;
        power *= mu;
        time_factors[step] = power;
    }
    const int time_scaling = scaled_match(scaled, velocities, time_factors, DEPTH);

    // v(x,t) = lam u(lam x, lam^2 t) solves (1)-(3) with the same nu and period 1/lam: v_m = lam^(1+2m) u_m(lam x).
    const int32_t lam = 2;
    TorusVector stretched_datum;
    vec_stretched(&datum, lam, &stretched_datum);
    TorusVector space_scaled[DEPTH + 1];
    taylor_velocity(&stretched_datum, VISCOSITY, DEPTH, space_scaled, NULL);
    TorusVector stretched[DEPTH + 1];
    int64_t space_factors[DEPTH + 1];
    int64_t lam_power = 1;
    for (unsigned step = 0u; step <= DEPTH; step += 1u)
    {
        vec_stretched(&velocities[step], lam, &stretched[step]);
        space_factors[step] = lam_power;
        lam_power *= (int64_t)lam * lam;
    }
    const int space_scaling = scaled_match(space_scaled, stretched, space_factors, DEPTH);

    // The null: the wrong exponent, mu^m in place of mu^(m+1), fails at order 0 already.
    const int wrong_exponent = scaled_match(scaled, velocities, wrong_factors, DEPTH);

    fprintf(out, "    time scaling mu = %lld: v_m = mu^(m+1) u_m at viscosity mu nu, orders 0..%d: %s\n", (long long)mu,
            DEPTH, truth(time_scaling));
    fprintf(out, "    space scaling lam = %d: v_m = lam^(1+2m) u_m(lam x) at the same nu, period retained: %s\n",
            (int)lam, truth(space_scaling));
    fprintf(out, "    null, exponent mu^m: %s (errored)\n\n", truth(wrong_exponent));

    vectors_free(stretched, DEPTH + 1u);
    vectors_free(space_scaled, DEPTH + 1u);
    vec_free(&stretched_datum);
    vectors_free(scaled, DEPTH + 1u);
    vec_free(&scaled_datum);
    vectors_free(velocities, DEPTH + 1u);
    vec_free(&datum);
    return time_scaling && space_scaling && !wrong_exponent;
}

/**
 * @brief The removal of the viscosity and the removal of the force are different removals, each on an instance.
 *
 * @param[in] out The stream [BORROWS].
 * @return        1 where every check lands.
 */
static int report_removals(FILE *out)
{
    fputs("  the two removals, on instances: neither solution set inherits the other's membership\n", out);
    enum
    {
        DEPTH = 2
    };
    TorusVector datum;
    abc_field(1, 2, 3, &datum);
    const Viscosity no_viscosity = {0, 0u};

    // Viscosity removed: the ABC field is a stationary Euler solution. With viscosity, it decays.
    TorusVector euler[DEPTH + 1];
    TorusScalar euler_pressures[DEPTH];
    taylor_velocity(&datum, no_viscosity, DEPTH, euler, euler_pressures);
    int stationary = 1;
    for (unsigned step = 1u; step <= DEPTH; step += 1u)
    {
        stationary = stationary && vec_is_zero(&euler[step]);
    }
    const int euler_ok = residuals_zero(euler, euler_pressures, no_viscosity, DEPTH);
    // The residual of the stationary field in (1) at viscosity nu is -nu Laplacian u_0 = 4 nu pi^2 u_0. It is
    // not zero.
    TorusVector stationary_in_viscous;
    residual(euler, euler_pressures, VISCOSITY, 0u, &stationary_in_viscous);
    RingRatio rate;
    decay_rate(VISCOSITY, 4, &rate);
    TorusVector expected;
    vec_times(&datum, &rate, &expected);
    ratio_free(&rate);
    const int viscous_residual = vec_equal(&stationary_in_viscous, &expected) && !vec_is_zero(&stationary_in_viscous);
    vec_free(&expected);
    vec_free(&stationary_in_viscous);
    // The residual in Euler at t = 0, of the field that decays, is u_1 = -4 nu pi^2 u_0. It is not zero.
    TorusVector viscous_velocities[DEPTH + 1];
    TorusScalar viscous_pressures[DEPTH];
    taylor_velocity(&datum, VISCOSITY, DEPTH, viscous_velocities, viscous_pressures);
    TorusVector decaying_in_euler;
    residual(viscous_velocities, viscous_pressures, no_viscosity, 0u, &decaying_in_euler);
    decay_rate(VISCOSITY, -4, &rate);
    vec_times(&datum, &rate, &expected);
    ratio_free(&rate);
    const int euler_residual = vec_equal(&decaying_in_euler, &expected) && !vec_is_zero(&decaying_in_euler);
    vec_free(&expected);
    vec_free(&decaying_in_euler);

    // Force removed: w(t) = e^(-t) g_0 solves the forced equation with f as its residual, and not the unforced one.
    TorusVector generic;
    generic_field(&generic);
    TorusVector prescribed[DEPTH + 2];
    for (unsigned step = 0u; step < (DEPTH + 2u); step += 1u)
    {
        vec_scale(&generic, ((step % 2u) == 0u) ? 1 : -1, 1, &prescribed[step]);
    }
    TorusScalar forced_pressures[DEPTH + 1];
    for (unsigned step = 0u; step <= DEPTH; step += 1u)
    {
        TorusVector nonlinear;
        vec_zero(&nonlinear);
        for (unsigned split = 0u; split <= step; split += 1u)
        {
            TorusVector term;
            advect(&prescribed[split], &prescribed[step - split], &term);
            TorusVector scaled;
            vec_scale(&term, binomial(step, split), 1, &scaled);
            TorusVector sum;
            vec_add(&nonlinear, &scaled, &sum);
            vec_free(&term);
            vec_free(&scaled);
            vec_free(&nonlinear);
            nonlinear = sum;
        }
        pressure_of(&nonlinear, &forced_pressures[step]);
        vec_free(&nonlinear);
    }
    TorusVector forces[DEPTH + 1];
    int force_needed = 1;
    int force_periodic_and_smooth = 1;
    for (unsigned step = 0u; step <= DEPTH; step += 1u)
    {
        residual(prescribed, forced_pressures, VISCOSITY, step, &forces[step]);
        force_needed = force_needed && !vec_is_zero(&forces[step]);
        force_periodic_and_smooth = force_periodic_and_smooth && is_real_field(&forces[step]);
    }
    // The same field against the unforced equation: the residual is the force itself. It is not zero.
    TorusVector unforced_residual;
    residual(prescribed, forced_pressures, VISCOSITY, 0u, &unforced_residual);
    const int not_unforced = vec_equal(&unforced_residual, &forces[0]) && !vec_is_zero(&unforced_residual);
    vec_free(&unforced_residual);

    fprintf(out, "    nu = 0 on the ABC datum: stationary (u_m = 0, m >= 1): %s ; Euler residual zero: %s\n",
            truth(stationary), truth(euler_ok));
    fprintf(out, "    the stationary field in (1) at nu = %lld at %u place(s): residual = 4 nu pi^2 u_0, nonzero: %s\n",
            (long long)VISCOSITY.numerator, (unsigned)VISCOSITY.places, truth(viscous_residual));
    fprintf(out, "    the decaying field in Euler at t = 0: residual = -4 nu pi^2 u_0, nonzero: %s\n",
            truth(euler_residual));
    fprintf(out,
            "    e^(-t) g_0 solves the forced equation with f = its residual, f nonzero at orders 0..%d: %s ; f real "
            "and periodic: %s\n",
            DEPTH, truth(force_needed), truth(force_periodic_and_smooth));
    fprintf(out, "    the same field in the unforced equation: residual = f, nonzero: %s\n\n", truth(not_unforced));

    vectors_free(forces, DEPTH + 1u);
    scalars_free(forced_pressures, DEPTH + 1u);
    vectors_free(prescribed, DEPTH + 2u);
    vec_free(&generic);
    vectors_free(viscous_velocities, DEPTH + 1u);
    scalars_free(viscous_pressures, DEPTH);
    vectors_free(euler, DEPTH + 1u);
    scalars_free(euler_pressures, DEPTH);
    vec_free(&datum);
    return stationary && euler_ok && viscous_residual && euler_residual && force_needed && force_periodic_and_smooth &&
           not_unforced;
}

/**
 * @brief The ring is countable. The data class holds a copy of the bit sequences, which are not.
 *
 * @param[in] out The stream [BORROWS].
 * @return        1 where every check lands.
 */
static int report_island(FILE *out)
{
    fputs("  the island: exactly nameable fields are countable, the data class (8) is not\n", out);
    enum
    {
        DEPTH = 12,
        PATTERNS = 2
    };
    static const char *const patterns[PATTERNS] = {"101100111000", "110010100011"};
    TorusVector fields[PATTERNS];
    for (unsigned which = 0u; which < PATTERNS; which += 1u)
    {
        TorusScalar component;
        scalar_zero(&component);
        for (int32_t index = 1; index <= DEPTH; index += 1)
        {
            if (patterns[which][index - 1] != '1')
            {
                continue;
            }
            int64_t power = 1;
            for (int32_t count = 0; count < index; count += 1)
            {
                power *= index;
            }
            const int32_t mode[TORUS_AXES] = {index, 0, 0};
            TorusScalar wave;
            harmonic(mode, 2, power, &wave);
            TorusScalar sum;
            scalar_add(&component, &wave, &sum);
            scalar_free(&component);
            scalar_free(&wave);
            component = sum;
        }
        scalar_zero(&fields[which].axis[0]);
        fields[which].axis[1] = component;
        scalar_zero(&fields[which].axis[2]);
    }
    int divergence_free = 1;
    int real_valued = 1;
    for (unsigned which = 0u; which < PATTERNS; which += 1u)
    {
        TorusScalar spread;
        divergence(&fields[which], &spread);
        divergence_free = divergence_free && scalar_is_zero(&spread);
        scalar_free(&spread);
        real_valued = real_valued && is_real_field(&fields[which]);
    }
    const int distinct = !vec_equal(&fields[0], &fields[1]);
    // The coefficient at mode n is b_n / n^n, and it names the bit. Thus the map from patterns is injective.
    int injective = 1;
    for (unsigned which = 0u; which < PATTERNS; which += 1u)
    {
        for (int32_t index = 1; index <= DEPTH; index += 1)
        {
            const int32_t mode[TORUS_AXES] = {index, 0, 0};
            const char bit = scalar_has_mode(&fields[which].axis[1], mode) ? '1' : '0';
            injective = injective && (bit == patterns[which][index - 1]);
        }
    }

    fprintf(out, "    F_b = sum_n 2 b_n n^-n cos(2 pi n x) e_y, truncated at n = %d, two bit patterns\n", DEPTH);
    fprintf(out, "    divergence-free: %s ; real-valued: %s ; distinct fields: %s ; bits read back: %s\n",
            truth(divergence_free), truth(real_valued), truth(distinct), truth(injective));
    fputs("    the full sums are smooth (n^-n beats every power of n) and periodic. They sit in (8);\n", out);
    fputs("    the bit sequences are uncountable (Cantor, proof_set_theory.py) and the ring is countable.\n\n", out);

    vectors_free(fields, PATTERNS);
    return divergence_free && real_valued && distinct && injective;
}

int program_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
#ifdef _WIN32
    // The report ends each line with a line feed alone, on every platform.
    (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
    FILE *const out = stdout;
    fputs("  Navier-Stokes on the unit torus in exact integer arithmetic: rigor displayed, nothing claimed\n\n", out);
    const int exact_solution = report_exact_solution(out);
    const int horizon = report_horizon(out);
    const int scaling = report_scaling(out);
    const int removals = report_removals(out);
    const int island = report_island(out);
    const int all = exact_solution && horizon && scaling && removals && island;
    if (all)
    {
        fputs("  every check lands: the exact solution is reproduced two ways to the integer, the\n", out);
        fputs("  generic datum's horizon grows at every order, the symmetries hold exactly, the removals\n", out);
        fputs("  change the solution on the instance, and the ring is a countable island in the data class.\n", out);
    }
    else
    {
        fputs("  a check missed: refuted as stated.\n", out);
    }
    fflush(out);
    return all ? 0 : 1;
}

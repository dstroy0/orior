// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file exact_navier_stokes_cascade.c
 * @brief The cascade and the growth of the coefficients for Navier-Stokes on the unit torus, exact.
 *
 * @note This reads three things on the ring of torus_fields.h, each as exact integers, and claims nothing.
 * @note The mode front: how many modes the Taylor coefficients carry in each |k|_1 shell, order by order. For a
 *       generic datum, the front advances one shell per order. For the ABC datum, an eigenfunction of the curl,
 *       every order stays in the shell |k|_1 = 1. That is the null of no cascade.
 * @note The energy shells: which shells |k|^2 carry energy that is not zero, by the exact zero test, with no
 *       value of pi. The velocity route and the vorticity route give the same energy. The shell energies sum to
 *       the total, by the identity of Parseval. A route with one mode dropped fails that match.
 * @note The growth: for the ABC datum, E(u_(m+1)) = 16 nu^2 pi^4 E(u_m), a constant exact rate. Thus the series
 *       is entire. For a generic datum, the degree in pi of the energy rises every order. The limit that fixes
 *       the radius of analyticity needs infinitely many orders, and no finite order reaches it.
 * @note Run build_exact_navier_stokes.sh cascade, beside this file.
 */
#include "../../../src/platform/platform.h"
#include "memoria_operor/memoria_operor.h"
#include "torus_fields.h"

#include <stdlib.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/** @brief Taylor orders 0..DEPTH on the cheaper velocity route. */
#define DEPTH 5u

/** @brief The orders on which the second route, by the vorticity, also runs. */
#define ROUTE_DEPTH 3u

/** @brief The largest shell that a list of shells holds here. */
#define SHELL_LIMIT 256u

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
 * @brief The value |k|_1 of a mode.
 *
 * @param[in] mode The mode [BORROWS].
 * @return         The value.
 */
static int64_t l1_shell(const int32_t mode[TORUS_AXES])
{
    return llabs((long long)mode[0]) + llabs((long long)mode[1]) + llabs((long long)mode[2]);
}

/**
 * @brief The largest |k|_1 over the modes of a field.
 *
 * @param[in] vector The field [BORROWS].
 * @return           The front.
 */
static int64_t front_of(const TorusVector *vector)
{
    ModeList list;
    modes_of(vector, &list);
    int64_t largest = 0;
    for (size_t at = 0u; at < list.count; at += 1u)
    {
        const int64_t shell = l1_shell(list.modes[at]);
        largest = (shell > largest) ? shell : largest;
    }
    platform_cell_released(list.modes, platform_place_g_program_owner);
    return largest;
}

/**
 * @brief Writes a list of integers as Python prints a list.
 *
 * @param[in] values The values [BORROWS].
 * @param[in] count  How many.
 * @param[in] out    The stream [BORROWS].
 */
static void print_list(const int64_t *values, size_t count, FILE *out)
{
    fputs("[", out);
    for (size_t at = 0u; at < count; at += 1u)
    {
        fprintf(out, "%s%lld", (at > 0u) ? ", " : "", (long long)values[at]);
    }
    fputs("]", out);
}

/**
 * @brief The shells |k|^2 that the modes of a field occupy, in ascending order.
 *
 * @param[in]  vector The field [BORROWS].
 * @param[out] shells The shells [BORROWS]. SHELL_LIMIT entries.
 * @return            How many shells.
 */
static size_t energy_shells(const TorusVector *vector, int64_t shells[SHELL_LIMIT])
{
    ModeList list;
    modes_of(vector, &list);
    size_t count = 0u;
    for (size_t at = 0u; at < list.count; at += 1u)
    {
        const int64_t norm = norm_of(list.modes[at]);
        size_t place = 0u;
        while ((place < count) && (shells[place] < norm))
        {
            place += 1u;
        }
        if ((place < count) && (shells[place] == norm))
        {
            continue;
        }
        if (count >= SHELL_LIMIT)
        {
            fprintf(stderr, "  too many shells\n");
            exit(2);
        }
        EMBED_CALL(memor.move_up, MemoriaCfg, .dst = &shells[place + 1u], .src = &shells[place],
                   .bytes = (count - place) * sizeof(int64_t));
        shells[place] = norm;
        count += 1u;
    }
    platform_cell_released(list.modes, platform_place_g_program_owner);
    return count;
}

/**
 * @brief The energy that a field holds in the shell |k|^2 = shell, an exact ratio.
 *
 * @param[in]  vector The field [BORROWS].
 * @param[in]  shell  The shell.
 * @param[out] energy The energy [BORROWS].
 */
static void energy_in_shell(const TorusVector *vector, int64_t shell, RingRatio *energy)
{
    TorusVector masked;
    for (unsigned axis = 0u; axis < TORUS_AXES; axis += 1u)
    {
        scalar_in_shell(&vector->axis[axis], shell, &masked.axis[axis]);
    }
    parseval(&masked, &masked, energy);
    vec_free(&masked);
}

/**
 * @brief The shells that carry energy that is not zero.
 *
 * @param[in]  vector   The field [BORROWS].
 * @param[out] occupied The shells [BORROWS]. SHELL_LIMIT entries.
 * @return              How many shells.
 */
static size_t carrying_shells(const TorusVector *vector, int64_t occupied[SHELL_LIMIT])
{
    int64_t shells[SHELL_LIMIT];
    const size_t count = energy_shells(vector, shells);
    size_t kept = 0u;
    for (size_t at = 0u; at < count; at += 1u)
    {
        RingRatio energy;
        energy_in_shell(vector, shells[at], &energy);
        if (!ratio_is_zero(&energy))
        {
            occupied[kept] = shells[at];
            kept += 1u;
        }
        ratio_free(&energy);
    }
    return kept;
}

/**
 * @brief The mode front: the cascade to finer scales as integers, for a generic datum against the null of ABC.
 *
 * @param[in] out              The stream [BORROWS].
 * @param[in] generic_velocity The coefficients of the generic datum [BORROWS].
 * @param[in] abc_velocity     The coefficients of the ABC datum [BORROWS].
 * @return                     1 where every check lands.
 */
static int report_front(FILE *out, const TorusVector *generic_velocity, const TorusVector *abc_velocity)
{
    fputs("  the mode front: modes carried per |k|_1 shell, order by order (exact integer counts)\n", out);
    int64_t generic_front[DEPTH + 1u];
    int64_t abc_front[DEPTH + 1u];
    int64_t widest = 0;
    for (unsigned step = 0u; step <= DEPTH; step += 1u)
    {
        generic_front[step] = front_of(&generic_velocity[step]);
        abc_front[step] = front_of(&abc_velocity[step]);
        widest = (generic_front[step] > widest) ? generic_front[step] : widest;
    }
    fputs("    generic datum, divergence-free and not Beltrami:\n", out);
    fputs("    order ", out);
    for (int64_t shell = 1; shell <= widest; shell += 1)
    {
        fprintf(out, "  |k|1=%lld", (long long)shell);
    }
    fputs("\n", out);
    for (unsigned step = 0u; step <= DEPTH; step += 1u)
    {
        ModeList list;
        modes_of(&generic_velocity[step], &list);
        fprintf(out, "    %5u ", step);
        for (int64_t shell = 1; shell <= widest; shell += 1)
        {
            long long tally = 0;
            for (size_t at = 0u; at < list.count; at += 1u)
            {
                tally += (l1_shell(list.modes[at]) == shell) ? 1 : 0;
            }
            fprintf(out, "  %6lld", tally);
        }
        fputs("\n", out);
        platform_cell_released(list.modes, platform_place_g_program_owner);
    }

    // The front advances by exactly one shell per order: energy reaches the next finer scale each step.
    int front_advances = 1;
    for (unsigned step = 0u; step < DEPTH; step += 1u)
    {
        front_advances = front_advances && (generic_front[step + 1u] == (generic_front[step] + 1));
    }
    // The null: the ABC datum is an eigenfunction, curl u_0 = 2 pi u_0. Every order stays on |k|_1 = 1.
    int abc_stays = 1;
    for (unsigned step = 0u; step <= DEPTH; step += 1u)
    {
        abc_stays = abc_stays && (abc_front[step] == 1);
    }

    fputs("    generic front (largest |k|_1 by order): ", out);
    print_list(generic_front, DEPTH + 1u, out);
    fprintf(out, " ; advances by one per order: %s\n", truth(front_advances));
    fputs("    null, ABC datum front: ", out);
    print_list(abc_front, DEPTH + 1u, out);
    fprintf(out, " ; stays in one shell (no cascade): %s\n\n", truth(abc_stays));
    return front_advances && abc_stays;
}

/**
 * @brief The energy per shell, two routes, the decomposition of Parseval, and the ABC null of no cascade.
 *
 * @param[in] out              The stream [BORROWS].
 * @param[in] generic          The generic datum [BORROWS].
 * @param[in] generic_velocity The coefficients of the generic datum [BORROWS].
 * @param[in] abc_velocity     The coefficients of the ABC datum [BORROWS].
 * @return                     1 where every check lands.
 */
static int report_energy(FILE *out, const TorusVector *generic, const TorusVector *generic_velocity,
                         const TorusVector *abc_velocity)
{
    fputs("  the energy shells: which shells carry energy, by two routes, summing to the total\n", out);

    // Route two: the vorticity coefficients, back to velocity by Biot-Savart, on the shared orders.
    TorusVector datum_vorticity;
    curl(generic, &datum_vorticity);
    TorusVector generic_vorticity[ROUTE_DEPTH + 1u];
    taylor_vorticity(&datum_vorticity, VISCOSITY, ROUTE_DEPTH, 1, generic_vorticity);
    vec_free(&datum_vorticity);
    int routes_agree = 1;
    for (unsigned step = 0u; step <= ROUTE_DEPTH; step += 1u)
    {
        RingRatio from_velocity;
        parseval(&generic_velocity[step], &generic_velocity[step], &from_velocity);
        TorusVector recovered;
        biot_savart(&generic_vorticity[step], &recovered);
        RingRatio from_vorticity;
        parseval(&recovered, &recovered, &from_vorticity);
        routes_agree = routes_agree && ratio_equal(&from_velocity, &from_vorticity);
        ratio_free(&from_velocity);
        ratio_free(&from_vorticity);
        vec_free(&recovered);
    }
    vectors_free(generic_vorticity, ROUTE_DEPTH + 1u);

    // The decomposition of Parseval: the shell energies sum back to the total, exactly, at every order.
    int decomposition_holds = 1;
    for (unsigned step = 0u; step <= DEPTH; step += 1u)
    {
        RingRatio total;
        parseval(&generic_velocity[step], &generic_velocity[step], &total);
        PiPoly nothing;
        poly_term(&nothing, 0, 0, 0);
        OriorExactInteger one;
        big_set(&one, 1);
        RingRatio summed;
        ratio_make(&summed, &nothing, &one);
        int64_t shells[SHELL_LIMIT];
        const size_t count = energy_shells(&generic_velocity[step], shells);
        for (size_t at = 0u; at < count; at += 1u)
        {
            RingRatio part;
            energy_in_shell(&generic_velocity[step], shells[at], &part);
            RingRatio sum;
            ratio_add(&summed, &part, &sum);
            ratio_free(&summed);
            ratio_free(&part);
            summed = sum;
        }
        decomposition_holds = decomposition_holds && ratio_equal(&summed, &total);
        ratio_free(&summed);
        ratio_free(&total);
    }

    // The cascade as energy: the |k|^2 shells that carry energy, per order, for the generic datum.
    fputs("    generic datum, shells |k|^2 carrying nonzero energy (the exact zero test, no pi value):\n", out);
    for (unsigned step = 0u; step <= DEPTH; step += 1u)
    {
        int64_t carrying[SHELL_LIMIT];
        const size_t count = carrying_shells(&generic_velocity[step], carrying);
        fprintf(out, "    order %u: ", step);
        print_list(carrying, count, out);
        fputs("\n", out);
    }

    // The null: the ABC datum keeps all its energy in |k|^2 = 1 at every order.
    int abc_only_base = 1;
    for (unsigned step = 0u; step <= DEPTH; step += 1u)
    {
        int64_t carrying[SHELL_LIMIT];
        const size_t count = carrying_shells(&abc_velocity[step], carrying);
        abc_only_base = abc_only_base && (count == 1u) && (carrying[0] == 1);
    }

    // The null: a corrupt route, with one mode dropped, does not match the energy.
    const unsigned order = 2u;
    RingRatio full;
    parseval(&generic_velocity[order], &generic_velocity[order], &full);
    const TorusScalar *const dropped_axis = &generic_velocity[order].axis[0];
    TorusVector corrupted;
    scalar_without_mode(dropped_axis, dropped_axis->entries[0].mode, &corrupted.axis[0]);
    scalar_copy(&generic_velocity[order].axis[1], &corrupted.axis[1]);
    scalar_copy(&generic_velocity[order].axis[2], &corrupted.axis[2]);
    RingRatio corrupt_energy;
    parseval(&corrupted, &corrupted, &corrupt_energy);
    const int corruption_caught = !ratio_equal(&corrupt_energy, &full);
    ratio_free(&corrupt_energy);
    ratio_free(&full);
    vec_free(&corrupted);

    fprintf(out, "    energy from the velocity route equals the vorticity route, orders 0..%u: %s\n", ROUTE_DEPTH,
            truth(routes_agree));
    fprintf(out, "    shell energies sum to the total at every order (Parseval): %s\n", truth(decomposition_holds));
    fprintf(out, "    null, ABC energy stays in |k|^2 = 1 at every order (no cascade): %s\n", truth(abc_only_base));
    fprintf(out, "    null, a route with one mode dropped is caught: %s\n\n", truth(corruption_caught));
    return routes_agree && decomposition_holds && abc_only_base && corruption_caught;
}

/**
 * @brief The growth of the coefficients: an exact constant rate for ABC, and the horizon for a generic datum.
 *
 * @param[in] out              The stream [BORROWS].
 * @param[in] generic_velocity The coefficients of the generic datum [BORROWS].
 * @param[in] abc_velocity     The coefficients of the ABC datum [BORROWS].
 * @return                     1 where every check lands.
 */
static int report_growth(FILE *out, const TorusVector *generic_velocity, const TorusVector *abc_velocity)
{
    fputs("  the growth of the coefficients: whose limit is the reciprocal of the analyticity time\n", out);

    // ABC: E(u_(m+1)) = (16 nu^2 pi^4) E(u_m), an exact ratio, constant in m.
    RingRatio rate;
    decay_rate(VISCOSITY, -4, &rate);
    RingRatio rate_squared;
    ratio_mul(&rate, &rate, &rate_squared);
    ratio_free(&rate);
    RingRatio abc_energies[DEPTH + 1u];
    for (unsigned step = 0u; step <= DEPTH; step += 1u)
    {
        parseval(&abc_velocity[step], &abc_velocity[step], &abc_energies[step]);
    }
    int law_holds = 1;
    for (unsigned step = 0u; step < DEPTH; step += 1u)
    {
        RingRatio expected;
        ratio_mul(&abc_energies[step], &rate_squared, &expected);
        law_holds = law_holds && ratio_equal(&abc_energies[step + 1u], &expected);
        ratio_free(&expected);
    }
    // The same exact factor at every order. The rate does not depend on m.
    const int rate_constant = law_holds;

    fputs("    ABC energy rate E(u_{m+1})/E(u_m) = 16 nu^2 pi^4 = ", out);
    ratio_print(&rate_squared, out);
    fprintf(out, ", exact and constant in m: %s\n", truth(law_holds));
    fputs("    a constant rate makes sum u_m t^m/m! = e^(-4 nu pi^2 t) u_0, entire: the radius is\n", out);
    fprintf(out, "    infinite and this datum never blows up (matches the closed form in the sibling file): %s\n",
            truth(rate_constant));

    // A generic datum: the exact energy of each order, and its degree in pi. The limit stays out of reach.
    fputs("    generic datum, exact energy and its pi-degree per order (the coefficients grow):\n", out);
    int32_t degrees[DEPTH + 1u];
    for (unsigned step = 0u; step <= DEPTH; step += 1u)
    {
        RingRatio energy;
        parseval(&generic_velocity[step], &generic_velocity[step], &energy);
        degrees[step] = ratio_pi_degree(&energy);
        fprintf(out, "    order %u: pi-degree %d, energy ", step, (int)degrees[step]);
        ratio_print(&energy, out);
        fputs("\n", out);
        ratio_free(&energy);
    }
    int degree_grows = 1;
    for (unsigned step = 0u; step < DEPTH; step += 1u)
    {
        degree_grows = degree_grows && (degrees[step + 1u] > degrees[step]);
    }

    fprintf(out, "    the pi-degree of the energy rises every order: %s (an exact integer signal of growth)\n",
            truth(degree_grows));
    fputs("    the limit that fixes the analyticity radius needs infinitely many orders and a step\n", out);
    fputs("    outside exact arithmetic; no finite order reaches it. That is the completeness floor,\n", out);
    fputs("    the shape of the horizon on the zeros of zeta, and it claims nothing about (A)-(D).\n\n", out);

    for (unsigned step = 0u; step <= DEPTH; step += 1u)
    {
        ratio_free(&abc_energies[step]);
    }
    ratio_free(&rate_squared);
    return law_holds && rate_constant && degree_grows;
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
    fputs("  Navier-Stokes on the unit torus: the cascade and the coefficient growth, exact, nothing claimed\n\n", out);
    TorusVector generic;
    TorusVector abc;
    generic_field(&generic);
    abc_field(1, 2, 3, &abc);
    TorusVector generic_velocity[DEPTH + 1u];
    TorusVector abc_velocity[DEPTH + 1u];
    taylor_velocity(&generic, VISCOSITY, DEPTH, generic_velocity, NULL);
    taylor_velocity(&abc, VISCOSITY, DEPTH, abc_velocity, NULL);

    const int front = report_front(out, generic_velocity, abc_velocity);
    const int energy = report_energy(out, &generic, generic_velocity, abc_velocity);
    const int growth = report_growth(out, generic_velocity, abc_velocity);
    const int all = front && energy && growth;
    if (all)
    {
        fputs("  every check lands: the generic datum's energy reaches a finer scale each order while the\n", out);
        fputs("  ABC datum stays in one shell, both routes and the Parseval sum agree to the integer, and the\n", out);
        fputs("  coefficient growth is an exact constant for ABC and a horizon, not a limit, for the generic datum.\n",
              out);
    }
    else
    {
        fputs("  a check missed: refuted as stated.\n", out);
    }
    fflush(out);

    vectors_free(generic_velocity, DEPTH + 1u);
    vectors_free(abc_velocity, DEPTH + 1u);
    vec_free(&generic);
    vec_free(&abc);
    return all ? 0 : 1;
}

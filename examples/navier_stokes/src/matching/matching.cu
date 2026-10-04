// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// matching.cu: the six matching functions (matching.h)
#include "matching.h"

#include "ode_series.h"

static SimRational matching_word(long long numerator, long long denominator)
{
    return sim_rational(numerator, denominator);
}

static SimRational matching_add(SimRational left, SimRational right)
{
    return sim_rational_sum(left, right);
}

static SimRational matching_less(SimRational left, SimRational right)
{
    return sim_rational_difference(left, right);
}

static SimRational matching_times(SimRational left, SimRational right)
{
    return sim_rational_product(left, right);
}

static SimRational matching_over(SimRational left, SimRational right)
{
    return sim_rational_product(left, sim_rational_reciprocal(right));
}

// sum values[k] x^k
static SimRational matching_polynomial(const std::vector<SimRational> &values, SimRational x)
{
    SimRational sum = matching_word(0ll, 1ll);
    for (size_t index = values.size(); index > 0u; index -= 1u)
    {
        sum = matching_add(matching_times(sum, x), values[index - 1u]);
    }
    return sum;
}

// int_0^a of the product of the polynomials, each list the coefficients in X, times X^lift
static AtomForm matching_core_integral(const std::vector<const std::vector<SimRational> *> &factors, unsigned int lift,
                                       SimRational inner)
{
    unsigned int total = lift + 1u;
    for (const std::vector<SimRational> *factor : factors)
    {
        total += (unsigned int)factor->size();
    }
    const SimRational zero = matching_word(0ll, 1ll);
    std::vector<SimRational> lifted(lift + 1u, zero);
    lifted[lift] = matching_word(1ll, 1ll);
    TaylorSeries product = taylor_polynomial(lifted, zero, total);
    for (const std::vector<SimRational> *factor : factors)
    {
        product = taylor_product(product, taylor_polynomial(*factor, zero, total));
    }
    return taylor_integral(product, zero, inner);
}

// T_0 .. T_(n-1) at x and their derivatives, by T_(j+1) = 2 x T_j - T_(j-1), T'_(j+1) = 2 T_j + 2 x T'_j - T'_(j-1)
static void matching_chebyshev(size_t count, SimRational x, std::vector<SimRational> *values, std::vector<SimRational> *slopes)
{
    values->assign(count, matching_word(0ll, 1ll));
    slopes->assign(count, matching_word(0ll, 1ll));
    for (size_t index = 0u; index < count; index += 1u)
    {
        if (index == 0u)
        {
            (*values)[0] = matching_word(1ll, 1ll);
        }
        else if (index == 1u)
        {
            (*values)[1] = x;
            (*slopes)[1] = matching_word(1ll, 1ll);
        }
        else
        {
            const SimRational two = matching_word(2ll, 1ll);
            (*values)[index] = matching_less(matching_times(matching_times(two, x), (*values)[index - 1u]), (*values)[index - 2u]);
            (*slopes)[index] = matching_less(
                matching_add(matching_times(two, (*values)[index - 1u]), matching_times(matching_times(two, x), (*slopes)[index - 1u])),
                (*slopes)[index - 2u]);
        }
    }
}

// sum over j of weights[j] T_j(2s - 1), its coefficients in s
static std::vector<SimRational> matching_radial(const std::vector<SimRational> &weights)
{
    const SimRational zero = matching_word(0ll, 1ll);
    std::vector<SimRational> before(1u, matching_word(1ll, 1ll));
    std::vector<SimRational> now = {matching_word(-1ll, 1ll), matching_word(2ll, 1ll)};
    std::vector<SimRational> sum(weights.size() + 1u, zero);
    for (size_t mode = 0u; mode < weights.size(); mode += 1u)
    {
        const std::vector<SimRational> &term = (mode == 0u) ? before : now;
        for (size_t index = 0u; index < term.size(); index += 1u)
        {
            sum[index] = matching_add(sum[index], matching_times(weights[mode], term[index]));
        }
        if (mode >= 1u)
        {
            // T_(j+1) = 2 (2s - 1) T_j - T_(j-1)
            std::vector<SimRational> next(now.size() + 1u, zero);
            for (size_t index = 0u; index < now.size(); index += 1u)
            {
                next[index] = matching_less(next[index], matching_times(matching_word(2ll, 1ll), now[index]));
                next[index + 1u] = matching_add(next[index + 1u], matching_times(matching_word(4ll, 1ll), now[index]));
            }
            for (size_t index = 0u; index < before.size(); index += 1u)
            {
                next[index] = matching_less(next[index], before[index]);
            }
            before = now;
            now = next;
        }
    }
    return sum;
}

// the annulus content at one eta: the radial polynomial in s, and its eta derivative's
static void matching_content(const std::vector<std::vector<SimRational>> &content, SimRational eta,
                             std::vector<SimRational> *value, std::vector<SimRational> *slope)
{
    std::vector<SimRational> value_weights;
    std::vector<SimRational> slope_weights;
    for (const std::vector<SimRational> &row : content)
    {
        std::vector<SimRational> chebyshev;
        std::vector<SimRational> derivative;
        matching_chebyshev(row.size(), eta, &chebyshev, &derivative);
        SimRational sum = matching_word(0ll, 1ll);
        SimRational slope_sum = matching_word(0ll, 1ll);
        for (size_t mode = 0u; mode < row.size(); mode += 1u)
        {
            sum = matching_add(sum, matching_times(row[mode], chebyshev[mode]));
            slope_sum = matching_add(slope_sum, matching_times(row[mode], derivative[mode]));
        }
        value_weights.push_back(sum);
        slope_weights.push_back(slope_sum);
    }
    *value = matching_radial(value_weights);
    *slope = matching_radial(slope_weights);
}

typedef struct
{
    const MatchingRequest *request;
    SimRational eta;
    SimRational twice;
    SimRational width;
    AtomForm exterior_factor;
    std::vector<SimRational> swirl;
    std::vector<SimRational> swirl_eta;
    std::vector<SimRational> axial;
    std::vector<SimRational> axial_eta;
    std::vector<SimRational> swirl_content;
    std::vector<SimRational> swirl_content_eta;
    std::vector<SimRational> axial_content;
    std::vector<SimRational> axial_content_eta;
} MatchingContext;

// a polynomial in X as a field about the place's center
static BlendField matching_core(const MatchingContext *context, const BlendPlace *place,
                                const std::vector<SimRational> &values)
{
    const SimRational at = matching_add(context->request->inner, matching_times(context->width, place->center));
    TaylorSeries series = taylor_stretched(taylor_polynomial(values, at, place->terms), context->width);
    series.center = place->center;
    return blend_field_smooth(place, series);
}

// a polynomial in s as a field
static BlendField matching_in_s(const BlendPlace *place, const std::vector<SimRational> &values)
{
    return blend_field_smooth(place, taylor_polynomial(values, place->center, place->terms));
}

// F_ext and F_ext_eta about the place: k w(z) and (4 eta / (2d)) k ((1 + h) w + z w'), z = X / (2d)
static void matching_exterior(const MatchingContext *context, const BlendPlace *place, AtomBook *book, BlendField *value,
                              BlendField *slope)
{
    const MatchingRequest *const request = context->request;
    const SimRational at = matching_add(request->inner, matching_times(context->width, place->center));
    const SimRational point = matching_over(at, context->twice);
    const std::string name = atom_book_rational(point);
    const unsigned int value_atom = atom_book_id(book, "w(" + name + ")");
    const unsigned int slope_atom = atom_book_id(book, "w'(" + name + ")");
    const unsigned int terms = place->terms + 1u;
    const TaylorSeries kummer = ode_series_kummer(point, request->h, terms, value_atom, slope_atom);
    std::vector<SimRational> line(terms, matching_word(0ll, 1ll));
    line[0] = point;
    if (terms > 1u)
    {
        line[1] = matching_word(1ll, 1ll);
    }
    const TaylorSeries z = taylor_rational(point, line);
    const TaylorSeries inner = taylor_sum(taylor_scaled(kummer, matching_add(matching_word(1ll, 1ll), request->h)),
                                          taylor_product(z, taylor_derivative(kummer)));
    const SimRational stretch = matching_over(context->width, context->twice);
    TaylorSeries value_series = taylor_form_scaled(taylor_stretched(kummer, stretch), context->exterior_factor);
    TaylorSeries slope_series = taylor_form_scaled(
        taylor_scaled(taylor_stretched(inner, stretch), matching_over(matching_times(matching_word(4ll, 1ll), context->eta),
                                                                      context->twice)),
        context->exterior_factor);
    value_series.center = place->center;
    slope_series.center = place->center;
    value_series.coefficient.resize(place->terms);
    slope_series.coefficient.resize(place->terms);
    *value = blend_field_smooth(place, value_series);
    *slope = blend_field_smooth(place, slope_series);
}

void matching_at(const MatchingRequest *request, SimRational eta, AtomBook *book, MatchingFunctions *functions)
{
    const EtaShape *const shape = &request->shape;
    const SimRational one = matching_word(1ll, 1ll);
    const SimRational h = request->h;
    const SimRational big_a = matching_add(matching_word(1ll, 2ll), h);
    const SimRational big_d = matching_less(matching_word(1ll, 2ll), h);
    const SimRational big_ap = matching_add(big_a, matching_word(1ll, 2ll));
    const SimRational d = matching_less(one, matching_times(eta, eta));
    const SimRational l_inverse = sim_rational_reciprocal(matching_less(one, matching_times(matching_times(matching_word(2ll, 1ll), h), matching_times(eta, eta))));
    const SimRational a = request->inner;
    const SimRational b = request->outer;
    const SimRational c = request->amplitude;

    MatchingContext context;
    context.request = request;
    context.eta = eta;
    context.twice = matching_times(matching_word(2ll, 1ll), d);
    context.width = matching_less(b, a);
    const unsigned int root = atom_book_id(book, "2^(-1/2)");
    const unsigned int spread = atom_book_id(book, "(" + atom_book_rational(context.twice) + ")^(-h)");
    const unsigned int reach = atom_book_id(book, "(" + atom_book_rational(b) + ")^(-h)");
    // k = (c / sqrt 2) (2d)^(-1-h)
    context.exterior_factor =
        atom_form_scaled(atom_form_product(atom_form_atom(root), atom_form_atom(spread)), matching_over(c, context.twice));
    context.swirl = core_series_at(shape, request->series->swirl, eta);
    context.swirl_eta = core_series_at(shape, request->series->swirl_slope, eta);
    context.axial = core_series_at(shape, request->series->axial, eta);
    context.axial_eta = core_series_at(shape, request->series->axial_slope, eta);
    matching_content(request->swirl_content, eta, &context.swirl_content, &context.swirl_content_eta);
    matching_content(request->axial_content, eta, &context.axial_content, &context.axial_content_eta);
    const std::vector<SimRational> inflow = core_series_at(shape, request->series->inflow, eta);
    const std::vector<SimRational> pressure = core_series_at(shape, request->series->pressure, eta);
    std::vector<SimRational> pressure_eta;
    for (const EtaFunction &function : request->series->pressure)
    {
        EtaFunction derivative;
        eta_function_derivative(shape, &function, &derivative);
        pressure_eta.push_back(eta_function_value_at(shape, &derivative, eta));
    }

    // the core's flux and pressure at X_a
    const SimRational flux_a = matching_times(a, matching_polynomial(inflow, a));
    const SimRational pressure_a = matching_polynomial(pressure, a);
    const SimRational pressure_eta_a = matching_polynomial(pressure_eta, a);
    const std::string end_name = atom_book_rational(matching_over(b, context.twice));
    // F, F_X, U and U_X at X_a and X_b, taken from the fields at the two ends
    AtomForm f_a;
    AtomForm f_x_a;
    AtomForm u_a;
    AtomForm u_x_a;
    AtomForm f_b;
    AtomForm f_x_b;
    AtomForm u_b;
    AtomForm u_x_b;
    int ends = 1;
    // the weight and the content switched off leave the core alone on the annulus
    const SimRational blend = request->blended ? one : matching_word(0ll, 1ll);

    // the annulus, place by place
    AtomForm torque_sum;
    AtomForm force_sum;
    AtomForm m_sum;
    AtomForm i_sum;
    AtomForm j_sum;
    AtomForm s_sum;
    AtomForm c_sum;
    const std::vector<BlendPlace> places = blend_field_places(&request->pieces, book);
    for (const BlendPlace &place : places)
    {
        const BlendField core_f = matching_core(&context, &place, context.swirl);
        const BlendField core_f_eta = matching_core(&context, &place, context.swirl_eta);
        const BlendField core_u = matching_core(&context, &place, context.axial);
        const BlendField core_u_eta = matching_core(&context, &place, context.axial_eta);
        BlendField exterior_f;
        BlendField exterior_f_eta;
        matching_exterior(&context, &place, book, &exterior_f, &exterior_f_eta);
        const BlendField weight = blend_field_scaled(blend_field_weight(&place), blend);
        const BlendField bump = blend_field_scaled(blend_field_bump(&place), blend);
        const BlendField x = matching_in_s(&place, {a, context.width});
        const BlendField f = blend_field_sum(
            blend_field_sum(core_f, blend_field_product(weight, blend_field_difference(exterior_f, core_f))),
            blend_field_product(bump, matching_in_s(&place, context.swirl_content)));
        const BlendField f_eta = blend_field_sum(
            blend_field_sum(core_f_eta, blend_field_product(weight, blend_field_difference(exterior_f_eta, core_f_eta))),
            blend_field_product(bump, matching_in_s(&place, context.swirl_content_eta)));
        const BlendField u = blend_field_sum(blend_field_difference(core_u, blend_field_product(weight, core_u)),
                                             blend_field_product(bump, matching_in_s(&place, context.axial_content)));
        const BlendField u_eta = blend_field_sum(blend_field_difference(core_u_eta, blend_field_product(weight, core_u_eta)),
                                                 blend_field_product(bump, matching_in_s(&place, context.axial_content_eta)));
        const SimRational over_width = sim_rational_reciprocal(context.width);
        const BlendField f_x = blend_field_scaled(blend_field_derivative(f), over_width);
        const BlendField u_x = blend_field_scaled(blend_field_derivative(u), over_width);
        const BlendField x_f_x = blend_field_product(x, f_x);
        const BlendField x_u_x = blend_field_product(x, u_x);
        if (place.kind == BLEND_FIELD_LOW)
        {
            ends = ends && blend_field_value(f, &f_a) && blend_field_value(f_x, &f_x_a) && blend_field_value(u, &u_a) &&
                   blend_field_value(u_x, &u_x_a);
        }
        if (place.kind == BLEND_FIELD_HIGH)
        {
            ends = ends && blend_field_value(f, &f_b) && blend_field_value(f_x, &f_x_b) && blend_field_value(u, &u_b) &&
                   blend_field_value(u_x, &u_x_b);
        }
        // C = L^-1 (2 A eta U - d U_eta + 2 eta X U_X)
        const BlendField flux_slope = blend_field_scaled(
            blend_field_sum(blend_field_difference(blend_field_scaled(u, matching_times(matching_times(matching_word(2ll, 1ll), big_a), eta)),
                                                   blend_field_scaled(u_eta, d)),
                            blend_field_scaled(x_u_x, matching_times(matching_word(2ll, 1ll), eta))),
            l_inverse);
        // X L^-1 (Ap F + D eta F_eta + X F_X) + X U L^-1 (-2 Ap eta F + d F_eta - 2 eta X F_X) - C X F
        const BlendField t_f = blend_field_scaled(
            blend_field_sum(blend_field_sum(blend_field_scaled(f, big_ap), blend_field_scaled(f_eta, matching_times(big_d, eta))), x_f_x),
            l_inverse);
        const BlendField z_f = blend_field_scaled(
            blend_field_difference(blend_field_sum(blend_field_scaled(f, matching_times(matching_word(-2ll, 1ll), matching_times(big_ap, eta))),
                                                   blend_field_scaled(f_eta, d)),
                                   blend_field_scaled(x_f_x, matching_times(matching_word(2ll, 1ll), eta))),
            l_inverse);
        const BlendField x_f = blend_field_product(x, f);
        const BlendField torque_field = blend_field_difference(
            blend_field_sum(blend_field_product(x, t_f), blend_field_product(blend_field_product(x, u), z_f)),
            blend_field_product(flux_slope, x_f));
        // L^-1 (A U + D eta U_eta + X U_X) + U L^-1 (-2 A eta U + d U_eta - 2 eta X U_X) - C U
        //     + L^-1 (-4 A eta (X_b - X) F^2 + 2 d (X_b - X) F F_eta - 2 eta X F^2)
        const BlendField t_u = blend_field_scaled(
            blend_field_sum(blend_field_sum(blend_field_scaled(u, big_a), blend_field_scaled(u_eta, matching_times(big_d, eta))), x_u_x),
            l_inverse);
        const BlendField z_u = blend_field_scaled(
            blend_field_difference(blend_field_sum(blend_field_scaled(u, matching_times(matching_word(-2ll, 1ll), matching_times(big_a, eta))),
                                                   blend_field_scaled(u_eta, d)),
                                   blend_field_scaled(x_u_x, matching_times(matching_word(2ll, 1ll), eta))),
            l_inverse);
        const BlendField rest = matching_in_s(&place, {matching_less(b, a), matching_times(matching_word(-1ll, 1ll), context.width)});
        const BlendField f_square = blend_field_product(f, f);
        const BlendField pressure_field = blend_field_scaled(
            blend_field_sum(
                blend_field_sum(blend_field_scaled(blend_field_product(rest, f_square),
                                                   matching_times(matching_word(-4ll, 1ll), matching_times(big_a, eta))),
                                blend_field_scaled(blend_field_product(rest, blend_field_product(f, f_eta)),
                                                   matching_times(matching_word(2ll, 1ll), d))),
                blend_field_scaled(blend_field_product(x, f_square), matching_times(matching_word(-2ll, 1ll), eta))),
            l_inverse);
        const BlendField force_field = blend_field_sum(
            blend_field_difference(blend_field_sum(t_u, blend_field_product(u, z_u)), blend_field_product(flux_slope, u)),
            pressure_field);
        torque_sum = atom_form_sum(torque_sum, blend_field_integral(torque_field, book));
        force_sum = atom_form_sum(force_sum, blend_field_integral(force_field, book));
        m_sum = atom_form_sum(m_sum, blend_field_integral(u, book));
        i_sum = atom_form_sum(i_sum, blend_field_integral(blend_field_scaled(x_f, matching_word(2ll, 1ll)), book));
        j_sum = atom_form_sum(j_sum, blend_field_integral(blend_field_scaled(blend_field_product(u, x_f), matching_word(2ll, 1ll)), book));
        s_sum = atom_form_sum(s_sum, blend_field_integral(blend_field_difference(blend_field_product(u, u),
                                                                                blend_field_product(x, f_square)), book));
        c_sum = atom_form_sum(c_sum, blend_field_integral(flux_slope, book));
    }
    const SimRational w = context.width;

    // the core on [0, X_a]
    const AtomForm m_core = matching_core_integral({&context.axial}, 0u, a);
    const AtomForm i_core = atom_form_scaled(matching_core_integral({&context.swirl}, 1u, a), matching_word(2ll, 1ll));
    const AtomForm j_core =
        atom_form_scaled(matching_core_integral({&context.axial, &context.swirl}, 1u, a), matching_word(2ll, 1ll));
    const AtomForm s_core = atom_form_difference(matching_core_integral({&context.axial, &context.axial}, 0u, a),
                                                 matching_core_integral({&context.swirl, &context.swirl}, 1u, a));

    // the torque: w int + V(X_b) X_b F(X_b) - V_a X_a F(X_a) - 2 (X_b^2 F_X(X_b) - X_a^2 F_X(X_a))
    const AtomForm flux_b = atom_form_sum(atom_form_rational(flux_a), atom_form_scaled(c_sum, w));
    AtomForm torque = atom_form_scaled(torque_sum, w);
    torque = atom_form_sum(torque, atom_form_scaled(atom_form_product(flux_b, f_b), b));
    torque = atom_form_difference(torque, atom_form_scaled(f_a, matching_times(flux_a, a)));
    torque = atom_form_difference(torque, atom_form_scaled(f_x_b, matching_times(matching_word(2ll, 1ll), matching_times(b, b))));
    torque = atom_form_sum(torque, atom_form_scaled(f_x_a, matching_times(matching_word(2ll, 1ll), matching_times(a, a))));
    // the force: 2^(-1/2) (w int + L^-1 w (-4 A eta Pi_a + d Pi_eta,a) + [V U] - 2 [X U_X])
    AtomForm force = atom_form_scaled(force_sum, w);
    const SimRational constants =
        matching_times(matching_times(l_inverse, w),
                       matching_add(matching_times(matching_times(matching_word(-4ll, 1ll), matching_times(big_a, eta)), pressure_a),
                                    matching_times(d, pressure_eta_a)));
    force = atom_form_sum(force, atom_form_rational(constants));
    force = atom_form_sum(force, atom_form_product(flux_b, u_b));
    force = atom_form_difference(force, atom_form_scaled(u_a, flux_a));
    force = atom_form_difference(force, atom_form_scaled(u_x_b, matching_times(matching_word(2ll, 1ll), b)));
    force = atom_form_sum(force, atom_form_scaled(u_x_a, matching_times(matching_word(2ll, 1ll), a)));
    force = atom_form_product(force, atom_form_atom(root));
    functions->ends = ends;
    // the tails
    const SimRational square_c = matching_times(c, c);
    const AtomForm reach_square = atom_form_product(atom_form_atom(reach), atom_form_atom(reach));
    functions->s_tail_power = atom_form_scaled(reach_square, matching_over(square_c, matching_times(matching_word(4ll, 1ll), h)));
    const unsigned int s_rest = atom_book_id(book, "int_(" + end_name + ")^inf (z w^2 - z^(-1-2h)) dz");
    const AtomForm s_tail = atom_form_sum(
        functions->s_tail_power,
        atom_form_scaled(atom_form_product(atom_form_product(atom_form_atom(spread), atom_form_atom(spread)), atom_form_atom(s_rest)),
                         matching_times(square_c, matching_word(1ll, 2ll))));
    const unsigned int h_rest = atom_book_id(book, "int_(" + atom_book_rational(b) + ")^inf ((" + atom_book_rational(context.twice) +
                                                       ")^(-1-h) x w(x/(" + atom_book_rational(context.twice) + ")) - x^(-h)) dx");
    // sqrt 2 = 2 2^(-1/2)
    const AtomForm square_root = atom_form_scaled(atom_form_atom(root), matching_word(2ll, 1ll));
    const AtomForm power_part = atom_form_scaled(atom_form_product(square_root, atom_form_atom(reach)),
                                                 matching_over(matching_times(c, b), matching_less(one, h)));
    const AtomForm h_tail = atom_form_scaled(atom_form_product(square_root, atom_form_atom(h_rest)), c);

    functions->torque = atom_form_square_reduced(torque, root, matching_word(1ll, 2ll));
    functions->force = atom_form_square_reduced(force, root, matching_word(1ll, 2ll));
    functions->m_inf = atom_form_square_reduced(atom_form_sum(m_core, atom_form_scaled(m_sum, w)), root, matching_word(1ll, 2ll));
    functions->j_inf = atom_form_square_reduced(atom_form_sum(j_core, atom_form_scaled(j_sum, w)), root, matching_word(1ll, 2ll));
    functions->s_inf = atom_form_square_reduced(
        atom_form_difference(atom_form_sum(s_core, atom_form_scaled(s_sum, w)), s_tail), root, matching_word(1ll, 2ll));
    functions->h_match = atom_form_square_reduced(
        atom_form_sum(atom_form_difference(atom_form_sum(i_core, atom_form_scaled(i_sum, w)), power_part), h_tail), root,
        matching_word(1ll, 2ll));
}

// the content matrix from a flat list, `modes` entries a row
static int matching_rows(const RunCfg *cfg, const char *path, std::vector<std::vector<SimRational>> *rows)
{
    const std::string at = path;
    std::vector<SimRational> weights;
    unsigned long long modes = 0ull;
    if (!run_cfg_count(cfg, (at + ".eta_modes").c_str(), &modes) || (modes == 0ull) ||
        !run_cfg_rationals(cfg, (at + ".weights").c_str(), &weights) || ((weights.size() % modes) != 0u))
    {
        return 0;
    }
    rows->clear();
    for (size_t start = 0u; start < weights.size(); start += (size_t)modes)
    {
        rows->push_back(std::vector<SimRational>(weights.begin() + (long)start, weights.begin() + (long)(start + modes)));
    }
    return 1;
}

int matching_read(const RunCfg *cfg, MatchingRequest *request, CoreSeries *series)
{
    SimRational h;
    std::vector<SimRational> swirl;
    std::vector<SimRational> axial;
    std::vector<SimRational> pressure;
    unsigned long long order = 0ull;
    unsigned long long terms = 0ull;
    unsigned long long orders = 0ull;
    const int read =
        run_cfg_rational(cfg, "core.anisotropy", &h) && (sim_rational_sign(h) > 0) &&
        run_cfg_rationals(cfg, "core.axis.swirl", &swirl) && run_cfg_rationals(cfg, "core.axis.axial", &axial) &&
        run_cfg_rationals(cfg, "core.axis.pressure", &pressure) && run_cfg_count(cfg, "core.order", &order) &&
        (order > 0ull) && (order < 4096ull) && run_cfg_rational(cfg, "join.amplitude", &request->amplitude) &&
        run_cfg_rational(cfg, "join.inner", &request->inner) && run_cfg_rational(cfg, "join.outer", &request->outer) &&
        (sim_rational_sign(request->inner) > 0) &&
        (sim_rational_sign(matching_less(request->outer, request->inner)) > 0) &&
        matching_rows(cfg, "content.swirl", &request->swirl_content) &&
        matching_rows(cfg, "content.axial", &request->axial_content) &&
        run_cfg_rationals(cfg, "blend.cuts", &request->pieces.cuts) && (request->pieces.cuts.size() >= 3u) &&
        run_cfg_count(cfg, "blend.terms", &terms) && (terms > 2ull) && run_cfg_count(cfg, "blend.orders", &orders) &&
        (orders > 0ull);
    if (!read)
    {
        return 0;
    }
    request->pieces.terms = (unsigned int)terms;
    request->pieces.orders = (unsigned int)orders;
    request->h = h;
    request->shape.part = h.numerator;
    request->shape.whole = h.denominator;
    request->blended = 1;
    EtaFunction swirl_data;
    EtaFunction axial_data;
    EtaFunction pressure_data;
    eta_function_chebyshev(swirl, &swirl_data);
    eta_function_chebyshev(axial, &axial_data);
    eta_function_chebyshev(pressure, &pressure_data);
    core_series_recursion(&request->shape, &swirl_data, &axial_data, &pressure_data, (unsigned int)order, series);
    request->series = series;
    return 1;
}

int matching_short(void)
{
    return s_sim_rational_wide != 0;
}

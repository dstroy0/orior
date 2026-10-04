// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// blend_field.cu: fields on the annulus (blend_field.h)
#include "blend_field.h"

#include "decay_integral.h"
#include "ode_series.h"

static int s_blend_field_diverges = 0;

static TaylorSeries blend_field_unit(SimRational center, unsigned int terms)
{
    std::vector<SimRational> values(terms, sim_rational(0ll, 1ll));
    if (terms > 0u)
    {
        values[0] = sim_rational(1ll, 1ll);
    }
    return taylor_rational(center, values);
}

// the series times tau^shift, `length` coefficients kept
static TaylorSeries blend_field_shifted(const TaylorSeries &series, unsigned int shift, size_t length)
{
    TaylorSeries shifted;
    shifted.center = series.center;
    for (size_t index = 0u; index < length; index += 1u)
    {
        shifted.coefficient.push_back((index >= shift) && (index - shift < series.coefficient.size())
                                          ? series.coefficient[index - shift]
                                          : TermForm());
    }
    return shifted;
}

static TaylorSeries blend_field_reduced(TaylorSeries series, const BlendPlace *place)
{
    for (TermForm &form : series.coefficient)
    {
        form = term_form_unit_reduced(form, place->ratio, place->share);
    }
    return series;
}

// a piece added to a part, merged with the piece of the same decay and pole
static void blend_field_put(std::vector<BlendFieldPiece> *part, unsigned int decay, unsigned int pole,
                            const TaylorSeries &series)
{
    for (BlendFieldPiece &piece : *part)
    {
        if ((piece.decay == decay) && (piece.pole == pole))
        {
            piece.series = taylor_sum(piece.series, series);
            return;
        }
    }
    BlendFieldPiece piece;
    piece.decay = decay;
    piece.pole = pole;
    piece.series = series;
    part->push_back(piece);
}

static void blend_field_add(BlendField *field, size_t power, unsigned int decay, unsigned int pole,
                            const TaylorSeries &series)
{
    if (field->part.size() <= power)
    {
        field->part.resize(power + 1u);
    }
    blend_field_put(&field->part[power], decay, pole, series);
}

static BlendField blend_field_empty(const BlendPlace *place)
{
    BlendField field;
    field.place = place;
    return field;
}

BlendField blend_field_smooth(const BlendPlace *place, const TaylorSeries &series)
{
    BlendField field = blend_field_empty(place);
    TaylorSeries local = series;
    if (place->kind == BLEND_FIELD_HIGH)
    {
        local = taylor_stretched(series, sim_rational(-1ll, 1ll));
    }
    local.center = (place->kind == BLEND_FIELD_MIDDLE) ? place->center : sim_rational(0ll, 1ll);
    blend_field_add(&field, 0u, 0u, 0u, local);
    return field;
}

BlendField blend_field_weight(const BlendPlace *place)
{
    BlendField field = blend_field_empty(place);
    const TaylorSeries unit = blend_field_unit(sim_rational(0ll, 1ll), place->terms);
    if (place->kind == BLEND_FIELD_MIDDLE)
    {
        blend_field_add(&field, 0u, 0u, 0u, place->weight);
    }
    else if (place->kind == BLEND_FIELD_LOW)
    {
        blend_field_add(&field, 1u, 0u, 0u, unit);
    }
    else
    {
        // psi(s) = 1 - psi(t)
        blend_field_add(&field, 0u, 0u, 0u, unit);
        blend_field_add(&field, 1u, 0u, 0u, taylor_scaled(unit, sim_rational(-1ll, 1ll)));
    }
    return field;
}

BlendField blend_field_bump(const BlendPlace *place)
{
    BlendField field = blend_field_empty(place);
    if (place->kind == BLEND_FIELD_MIDDLE)
    {
        blend_field_add(&field, 0u, 0u, 0u, ode_series_bump(place->center, place->terms));
        return field;
    }
    // e^(4 - 1/(tau(1-tau))) = e^(-1/tau) e^4 e^(-1/(1-tau)), the same at both ends
    const TaylorSeries rest = taylor_form_scaled(ode_series_decay_reflected(sim_rational(0ll, 1ll), place->terms),
                                                 term_form_e(sim_rational(4ll, 1ll)));
    blend_field_add(&field, 0u, 1u, 0u, rest);
    return field;
}

BlendField blend_field_sum(const BlendField &left, const BlendField &right)
{
    BlendField sum = left;
    for (size_t power = 0u; power < right.part.size(); power += 1u)
    {
        for (const BlendFieldPiece &piece : right.part[power])
        {
            blend_field_add(&sum, power, piece.decay, piece.pole, piece.series);
        }
    }
    return sum;
}

BlendField blend_field_scaled(const BlendField &field, SimRational factor)
{
    BlendField scaled = field;
    for (std::vector<BlendFieldPiece> &part : scaled.part)
    {
        for (BlendFieldPiece &piece : part)
        {
            piece.series = taylor_scaled(piece.series, factor);
        }
    }
    return scaled;
}

BlendField blend_field_form_scaled(const BlendField &field, const TermForm &form)
{
    BlendField scaled = field;
    for (std::vector<BlendFieldPiece> &part : scaled.part)
    {
        for (BlendFieldPiece &piece : part)
        {
            piece.series = taylor_form_scaled(piece.series, form);
            if (field.place->kind == BLEND_FIELD_MIDDLE)
            {
                piece.series = blend_field_reduced(piece.series, field.place);
            }
        }
    }
    return scaled;
}

BlendField blend_field_difference(const BlendField &left, const BlendField &right)
{
    return blend_field_sum(left, blend_field_scaled(right, sim_rational(-1ll, 1ll)));
}

BlendField blend_field_product(const BlendField &left, const BlendField &right)
{
    BlendField product = blend_field_empty(left.place);
    for (size_t first = 0u; first < left.part.size(); first += 1u)
    {
        for (size_t second = 0u; second < right.part.size(); second += 1u)
        {
            for (const BlendFieldPiece &one : left.part[first])
            {
                for (const BlendFieldPiece &other : right.part[second])
                {
                    TaylorSeries series = taylor_product(one.series, other.series);
                    if (left.place->kind == BLEND_FIELD_MIDDLE)
                    {
                        series = blend_field_reduced(series, left.place);
                    }
                    blend_field_add(&product, first + second, one.decay + other.decay, one.pole + other.pole, series);
                }
            }
        }
    }
    return product;
}

BlendField blend_field_derivative(const BlendField &field)
{
    BlendField slope = blend_field_empty(field.place);
    if (field.place->kind == BLEND_FIELD_MIDDLE)
    {
        for (const BlendFieldPiece &piece : field.part.empty() ? std::vector<BlendFieldPiece>() : field.part[0])
        {
            blend_field_add(&slope, 0u, 0u, 0u, blend_field_reduced(taylor_derivative(piece.series), field.place));
        }
        return slope;
    }
    // Q = 1 + tau^2 / (1 - tau)^2 = 1 + sum over k of (k + 1) tau^(k+2)
    std::vector<SimRational> q_values(field.place->terms, sim_rational(0ll, 1ll));
    for (size_t index = 0u; index < q_values.size(); index += 1u)
    {
        q_values[index] = (index == 0u) ? sim_rational(1ll, 1ll)
                                        : ((index == 1u) ? sim_rational(0ll, 1ll) : sim_rational((long long)index - 1ll, 1ll));
    }
    const TaylorSeries q = taylor_rational(sim_rational(0ll, 1ll), q_values);
    const SimRational sign = (field.place->kind == BLEND_FIELD_HIGH) ? sim_rational(-1ll, 1ll) : sim_rational(1ll, 1ll);
    for (size_t power = 0u; power < field.part.size(); power += 1u)
    {
        for (const BlendFieldPiece &piece : field.part[power])
        {
            const size_t length = piece.series.coefficient.size();
            // j psi_e^(j-1) psi_e' = j (psi_e^j - psi_e^(j+1)) tau^(-2) Q
            if (power > 0u)
            {
                const TaylorSeries weighted = taylor_scaled(taylor_product(q, piece.series),
                                                            sim_rational_product(sign, sim_rational((long long)power, 1ll)));
                blend_field_add(&slope, power, piece.decay, piece.pole + 2u, weighted);
                blend_field_add(&slope, power + 1u, piece.decay, piece.pole + 2u,
                                taylor_scaled(weighted, sim_rational(-1ll, 1ll)));
            }
            // d/dtau (e^(-m/tau) tau^(-p) S) = e^(-m/tau) tau^(-p-2) (m S - p tau S + tau^2 S')
            const TaylorSeries derivative = taylor_derivative(piece.series);
            if (piece.decay == 0u)
            {
                if (piece.pole == 0u)
                {
                    blend_field_add(&slope, power, 0u, 0u, taylor_scaled(derivative, sign));
                    continue;
                }
                // tau^(-p-1) (-p S + tau S')
                const TaylorSeries body = taylor_sum(
                    taylor_scaled(piece.series, sim_rational(-(long long)piece.pole, 1ll)),
                    blend_field_shifted(derivative, 1u, length));
                blend_field_add(&slope, power, 0u, piece.pole + 1u, taylor_scaled(body, sign));
                continue;
            }
            const TaylorSeries body = taylor_sum(
                taylor_sum(taylor_scaled(piece.series, sim_rational((long long)piece.decay, 1ll)),
                           taylor_scaled(blend_field_shifted(piece.series, 1u, length),
                                         sim_rational(-(long long)piece.pole, 1ll))),
                blend_field_shifted(derivative, 2u, length));
            blend_field_add(&slope, power, piece.decay, piece.pole + 2u, taylor_scaled(body, sign));
        }
    }
    return slope;
}

int blend_field_value(const BlendField &field, TermForm *value)
{
    *value = TermForm();
    if (field.place->kind == BLEND_FIELD_MIDDLE)
    {
        for (const BlendFieldPiece &piece : field.part.empty() ? std::vector<BlendFieldPiece>() : field.part[0])
        {
            *value = term_form_sum(*value, taylor_value(piece.series, field.place->center));
        }
        return 1;
    }
    // at the end psi_e and every decay vanish, and a pole without a decay has no value there
    for (size_t power = 0u; power < field.part.size(); power += 1u)
    {
        for (const BlendFieldPiece &piece : field.part[power])
        {
            if ((power == 0u) && (piece.decay == 0u))
            {
                if (piece.pole != 0u)
                {
                    return 0;
                }
                *value = term_form_sum(*value, taylor_value(piece.series, sim_rational(0ll, 1ll)));
            }
        }
    }
    return 1;
}

// C(top, bottom) for small arguments
static long long blend_field_choose(long long top, long long bottom)
{
    long long value = 1ll;
    for (long long index = 1ll; index <= bottom; index += 1ll)
    {
        value = value * (top - bottom + index) / index;
    }
    return value;
}

TermForm blend_field_integral(const BlendField &field, TermBook *book)
{
    const BlendPlace *const place = field.place;
    TermForm sum;
    if (place->kind == BLEND_FIELD_MIDDLE)
    {
        for (const BlendFieldPiece &piece : field.part.empty() ? std::vector<BlendFieldPiece>() : field.part[0])
        {
            sum = term_form_sum(sum, taylor_integral(piece.series, place->low, place->high));
        }
        return term_form_unit_reduced(sum, place->ratio, place->share);
    }
    const SimRational end = (place->kind == BLEND_FIELD_LOW) ? place->high
                                                             : sim_rational_difference(sim_rational(1ll, 1ll), place->low);
    const SimRational zero = sim_rational(0ll, 1ll);
    for (size_t power = 0u; power < field.part.size(); power += 1u)
    {
        for (const BlendFieldPiece &piece : field.part[power])
        {
            if (power == 0u)
            {
                if (piece.decay == 0u)
                {
                    if (piece.pole != 0u)
                    {
                        s_blend_field_diverges = 1;
                        continue;
                    }
                    sum = term_form_sum(sum, taylor_integral(piece.series, zero, end));
                    continue;
                }
                for (size_t k = 0u; k < piece.series.coefficient.size(); k += 1u)
                {
                    sum = term_form_sum(sum, term_form_product(piece.series.coefficient[k],
                                                               decay_integral_from_zero((int)k - (int)piece.pole,
                                                                                        sim_rational(piece.decay, 1ll),
                                                                                        end, book)));
                }
                continue;
            }
            for (unsigned int order = (unsigned int)power; order < (unsigned int)power + place->orders; order += 1u)
            {
                const long long sign = (((order - power) & 1u) == 0u) ? 1ll : -1ll;
                const SimRational weight =
                    sim_rational(sign * blend_field_choose((long long)order - 1ll, (long long)power - 1ll), 1ll);
                // g_N = e^(N tau / (1 - tau)): (1 - tau)^2 g' = N g
                const std::vector<SimRational> rise_p = {sim_rational(1ll, 1ll), sim_rational(-2ll, 1ll),
                                                         sim_rational(1ll, 1ll)};
                const std::vector<SimRational> rise_q(1u, sim_rational((long long)order, 1ll));
                const TaylorSeries rise =
                    taylor_rational(zero, ode_series_first(rise_p, rise_q, zero, (unsigned int)piece.series.coefficient.size()));
                const TaylorSeries product = taylor_product(rise, piece.series);
                TermForm part;
                for (size_t k = 0u; k < product.coefficient.size(); k += 1u)
                {
                    part = term_form_sum(part, term_form_product(product.coefficient[k],
                                                                 decay_integral_from_zero(
                                                                     (int)k - (int)piece.pole,
                                                                     sim_rational((long long)(order + piece.decay), 1ll),
                                                                     end, book)));
                }
                sum = term_form_sum(sum, term_form_scaled(term_form_product(part, term_form_e(sim_rational(order, 1ll))),
                                                          weight));
            }
        }
    }
    return sum;
}

std::vector<BlendPlace> blend_field_places(const BlendPieces *pieces, TermBook *book)
{
    const size_t count = pieces->cuts.size();
    std::vector<BlendPlace> places;
    for (size_t index = 0u; index + 1u < count; index += 1u)
    {
        BlendPlace place;
        place.low = pieces->cuts[index];
        place.high = pieces->cuts[index + 1u];
        place.terms = pieces->terms;
        place.orders = pieces->orders;
        place.ratio = sim_rational(0ll, 1ll);
        place.share = 0u;
        if (index == 0u)
        {
            place.kind = BLEND_FIELD_LOW;
            place.center = sim_rational(0ll, 1ll);
        }
        else if (index + 2u == count)
        {
            place.kind = BLEND_FIELD_HIGH;
            place.center = sim_rational(1ll, 1ll);
        }
        else
        {
            place.kind = BLEND_FIELD_MIDDLE;
            place.center = sim_rational_product(sim_rational_sum(place.low, place.high), sim_rational(1ll, 2ll));
            place.weight = blend_weight(place.center, place.terms, book, &place.ratio, &place.share);
        }
        places.push_back(place);
    }
    return places;
}

TermForm blend_field_total(const BlendPieces *pieces, BlendFieldIntegrand integrand, const void *context,
                           TermBook *book)
{
    TermForm sum;
    for (const BlendPlace &place : blend_field_places(pieces, book))
    {
        sum = term_form_sum(sum, blend_field_integral(integrand(context, &place, book), book));
    }
    return sum;
}

int blend_field_short(void)
{
    return (s_sim_rational_wide != 0) || (s_blend_field_diverges != 0);
}

// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pressure_datum.cu: the right side of the pressure datum (pressure_datum.h)
#include "pressure_datum.h"

#include "ode_series.h"

// one evaluation's numbers: the request's, the place in eta and the join, the core's swirl at eta, and the atoms
typedef struct
{
    const PressureDatumRequest *request;
    SimRational eta;
    SimRational inner;
    SimRational outer;
    std::vector<SimRational> core;
    unsigned int root;
    unsigned int spread;
} PressureDatumContext;

// F_core about X_c in s, the core polynomial moved and stretched by X_b - X_a
static TaylorSeries pressure_datum_core(const PressureDatumContext *context, SimRational center, unsigned int terms)
{
    const SimRational width = sim_rational_difference(context->outer, context->inner);
    const SimRational place = sim_rational_sum(context->inner, sim_rational_product(width, center));
    TaylorSeries series = taylor_stretched(taylor_polynomial(context->core, place, terms), width);
    series.center = center;
    return series;
}

// F_ext about X_c in s: (c / sqrt 2) (2d)^(-1-h) w(X / (2d)), w about z_c = X_c / (2d) stretched by (X_b - X_a) / (2d)
static TaylorSeries pressure_datum_exterior(const PressureDatumContext *context, SimRational center, unsigned int terms,
                                            AtomBook *book)
{
    const SimRational width = sim_rational_difference(context->outer, context->inner);
    const SimRational place = sim_rational_sum(context->inner, sim_rational_product(width, center));
    const SimRational twice = sim_rational_product(
        sim_rational(2ll, 1ll), sim_rational_difference(sim_rational(1ll, 1ll),
                                                        sim_rational_product(context->eta, context->eta)));
    const SimRational point = sim_rational_product(place, sim_rational_reciprocal(twice));
    const std::string name = atom_book_rational(point);
    const unsigned int value = atom_book_id(book, "w(" + name + ")");
    const unsigned int slope = atom_book_id(book, "w'(" + name + ")");
    TaylorSeries series = taylor_stretched(ode_series_kummer(point, context->request->h, terms, value, slope),
                                           sim_rational_product(width, sim_rational_reciprocal(twice)));
    series.center = center;
    const AtomForm factor = atom_form_product(
        atom_form_scaled(atom_form_atom(context->root),
                         sim_rational_product(context->request->amplitude, sim_rational_reciprocal(twice))),
        atom_form_atom(context->spread));
    return taylor_form_scaled(series, factor);
}

static TaylorSeries pressure_datum_reduced(TaylorSeries series, const PressureDatumContext *context)
{
    for (AtomForm &form : series.coefficient)
    {
        form = atom_form_square_reduced(form, context->root, sim_rational(1ll, 2ll));
    }
    return series;
}

static TaylorSeries pressure_datum_first(const void *given, SimRational center, unsigned int terms, AtomBook *book)
{
    const PressureDatumContext *const context = (const PressureDatumContext *)given;
    (void)book;
    const TaylorSeries core = pressure_datum_core(context, center, terms);
    return taylor_product(core, core);
}

static TaylorSeries pressure_datum_second(const void *given, SimRational center, unsigned int terms, AtomBook *book)
{
    const PressureDatumContext *const context = (const PressureDatumContext *)given;
    const TaylorSeries core = pressure_datum_core(context, center, terms);
    const TaylorSeries gap = taylor_difference(pressure_datum_exterior(context, center, terms, book), core);
    return pressure_datum_reduced(taylor_scaled(taylor_product(core, gap), sim_rational(2ll, 1ll)), context);
}

static TaylorSeries pressure_datum_third(const void *given, SimRational center, unsigned int terms, AtomBook *book)
{
    const PressureDatumContext *const context = (const PressureDatumContext *)given;
    const TaylorSeries core = pressure_datum_core(context, center, terms);
    const TaylorSeries gap = taylor_difference(pressure_datum_exterior(context, center, terms, book), core);
    return pressure_datum_reduced(taylor_product(gap, gap), context);
}

void pressure_datum_at(const PressureDatumRequest *request, SimRational eta, SimRational inner, SimRational outer,
                       AtomBook *book, PressureDatum *datum)
{
    PressureDatumContext context;
    context.request = request;
    context.eta = eta;
    context.inner = inner;
    context.outer = outer;
    context.core = core_series_at(&request->shape, request->series->swirl, eta);
    context.root = atom_book_id(book, "2^(-1/2)");
    const SimRational twice = sim_rational_product(
        sim_rational(2ll, 1ll), sim_rational_difference(sim_rational(1ll, 1ll), sim_rational_product(eta, eta)));
    context.spread = atom_book_id(book, "(" + atom_book_rational(twice) + ")^(-h)");

    // the core inside X_a: int_0^{X_a} P^2 dX, P held with room for its square
    const unsigned int square_terms = 2u * (unsigned int)context.core.size();
    const TaylorSeries whole = taylor_polynomial(context.core, sim_rational(0ll, 1ll), square_terms);
    datum->inside = taylor_integral(taylor_product(whole, whole), sim_rational(0ll, 1ll), inner);
    // the annulus
    AtomForm annulus = blend_integral(&request->pieces, 0u, pressure_datum_first, &context, book);
    annulus = atom_form_sum(annulus, blend_integral(&request->pieces, 1u, pressure_datum_second, &context, book));
    annulus = atom_form_sum(annulus, blend_integral(&request->pieces, 2u, pressure_datum_third, &context, book));
    datum->annulus = atom_form_scaled(annulus, sim_rational_difference(outer, inner));
    // past the join
    const SimRational end = sim_rational_product(outer, sim_rational_reciprocal(twice));
    const unsigned int tail = atom_book_id(book, "int_(" + atom_book_rational(end) + ")^inf w^2");
    datum->beyond = atom_form_product(
        atom_form_scaled(atom_form_atom(tail),
                         sim_rational_negative(sim_rational_product(
                             sim_rational_product(request->amplitude, request->amplitude),
                             sim_rational_reciprocal(sim_rational_product(sim_rational(2ll, 1ll), twice))))),
        atom_form_product(atom_form_atom(context.spread), atom_form_atom(context.spread)));
    datum->datum = atom_form_sum(atom_form_scaled(atom_form_sum(datum->inside, datum->annulus), sim_rational(-1ll, 1ll)),
                                 datum->beyond);
}

int pressure_datum_short(void)
{
    return s_sim_rational_wide != 0;
}

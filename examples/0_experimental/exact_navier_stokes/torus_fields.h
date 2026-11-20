// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file torus_fields.h
 * @brief The exact ring of fields on the unit torus, for the Navier-Stokes reports beside it.
 *
 * @note A field on R^3/Z^3 is a finite sum of modes e^(2 pi i k.x), with k in Z^3. Each mode carries a Laurent
 *       polynomial in pi with Gaussian integer coefficients. The whole field carries one positive integer
 *       denominator.
 * @note Every integer is an OriorExactInteger of includes/exact_types. No float, no rounding. An operation whose
 *       value does not fit the width stops the program with a message, and prints no wrong value.
 * @note Pi is a symbol. Pi is transcendental (Lindemann 1882). Thus a Laurent polynomial in pi with integer
 *       coefficients is zero exactly when every coefficient is zero.
 * @note Every value keeps a normal form: the denominator is positive, and shares no common divisor with the
 *       integers of the value. Thus two values are equal exactly when their integers are equal.
 * @note Each function that writes a value through an output pointer makes that value new. The caller frees it
 *       with the free function of its type.
 */
#ifndef TORUS_FIELDS_H
#define TORUS_FIELDS_H

#include "../../../includes/exact_types/integers/exact_integer.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/** @brief The three axes of the torus. */
#define TORUS_AXES 3u

/** @brief One term of a Laurent polynomial in pi: a Gaussian integer at a power of pi. */
typedef struct
{
    int32_t power;               /**< The power of pi. It can be negative. */
    OriorExactInteger real;      /**< The real part of the coefficient. */
    OriorExactInteger imaginary; /**< The imaginary part of the coefficient. */
} PiTerm;

/** @brief A Laurent polynomial in pi. The terms are in ascending order of power, and none is zero. */
typedef struct
{
    PiTerm *terms;   /**< The terms [OWNS]. */
    size_t count;    /**< How many terms. */
    size_t capacity; /**< How many terms the array holds. */
} PiPoly;

/** @brief A Laurent polynomial in pi over one positive integer, normalized. */
typedef struct
{
    PiPoly poly;                   /**< The numerator. */
    OriorExactInteger denominator; /**< The denominator, positive. */
} RingRatio;

/** @brief One mode of a field and the polynomial that it carries. */
typedef struct
{
    int32_t mode[TORUS_AXES]; /**< The wave vector k. */
    PiPoly poly;              /**< The coefficient, never zero. */
} ModeEntry;

/** @brief A scalar field: modes in ascending order of k, and one positive denominator, normalized. */
typedef struct
{
    ModeEntry *entries;            /**< The modes [OWNS]. */
    size_t count;                  /**< How many modes. */
    size_t capacity;               /**< How many modes the array holds. */
    OriorExactInteger denominator; /**< The denominator, positive. */
} TorusScalar;

/** @brief A vector field: one scalar field per axis. */
typedef struct
{
    TorusScalar axis[TORUS_AXES]; /**< The components. */
} TorusVector;

/** @brief A viscosity as decimal text reads it: a numerator at a count of decimal places. */
typedef struct
{
    int64_t numerator; /**< The digits of the viscosity. */
    uint32_t places;   /**< The decimal places. The viscosity is numerator / 10^places. */
} Viscosity;

/** @brief A list of modes, in ascending order, with no repeat. */
typedef struct
{
    int32_t (*modes)[TORUS_AXES]; /**< The modes [OWNS]. */
    size_t count;                 /**< How many modes. */
} ModeList;

/* ---------------------------------------------------------------- exact integers */

/**
 * @brief Stops the program where an exact operation fails.
 *
 * @param[in] status The status of the operation.
 * @param[in] what   The name of the operation, for the message [BORROWS].
 */
void ring_require(OriorExactStatus status, const char *what);

/**
 * @brief Sets an exact integer to the value of a machine integer.
 *
 * @param[out] value  The integer [BORROWS].
 * @param[in]  number The value.
 */
void big_set(OriorExactInteger *value, int64_t number);

/**
 * @brief Writes the decimal text of an exact integer into a buffer.
 *
 * @param[in]  value    The integer [BORROWS].
 * @param[out] text     The buffer [BORROWS].
 * @param[in]  capacity Bytes of the buffer.
 * @return              The length of the text.
 */
size_t big_text(const OriorExactInteger *value, char *text, size_t capacity);

/* ---------------------------------------------------------------- polynomials and ratios */

/**
 * @brief Frees a polynomial and leaves it empty.
 *
 * @param[in,out] poly The polynomial [BORROWS].
 */
void poly_free(PiPoly *poly);

/**
 * @brief The polynomial of one term, the Gaussian integer real + i imaginary at a power of pi.
 *
 * @param[out] poly      The polynomial [BORROWS].
 * @param[in]  power     The power of pi.
 * @param[in]  real      The real part.
 * @param[in]  imaginary The imaginary part.
 */
void poly_term(PiPoly *poly, int32_t power, int64_t real, int64_t imaginary);

/**
 * @brief Multiplies two polynomials.
 *
 * @param[in]  left    First factor [BORROWS].
 * @param[in]  right   Second factor [BORROWS].
 * @param[out] product The product [BORROWS].
 */
void poly_mul(const PiPoly *left, const PiPoly *right, PiPoly *product);

/**
 * @brief Whether two polynomials are equal.
 *
 * @param[in] left  First polynomial [BORROWS].
 * @param[in] right Second polynomial [BORROWS].
 * @return          1 where they are equal, else 0.
 */
int poly_equal(const PiPoly *left, const PiPoly *right);

/**
 * @brief Writes a polynomial as the reports print it.
 *
 * @param[in] poly The polynomial [BORROWS].
 * @param[in] out  The stream [BORROWS].
 */
void poly_print(const PiPoly *poly, FILE *out);

/**
 * @brief Frees a ratio and leaves it empty.
 *
 * @param[in,out] ratio The ratio [BORROWS].
 */
void ratio_free(RingRatio *ratio);

/**
 * @brief The ratio of a polynomial over a denominator, normalized. The ratio takes the polynomial.
 *
 * @param[out]    ratio       The ratio [BORROWS].
 * @param[in,out] poly        The numerator. The ratio owns it after the call, and it is empty [OWNS].
 * @param[in]     denominator The denominator, not zero [BORROWS].
 */
void ratio_make(RingRatio *ratio, PiPoly *poly, const OriorExactInteger *denominator);

/**
 * @brief Whether a ratio is zero.
 *
 * @param[in] ratio The ratio [BORROWS].
 * @return          1 where it is zero, else 0.
 */
int ratio_is_zero(const RingRatio *ratio);

/**
 * @brief Whether two ratios are equal.
 *
 * @param[in] left  First ratio [BORROWS].
 * @param[in] right Second ratio [BORROWS].
 * @return          1 where they are equal, else 0.
 */
int ratio_equal(const RingRatio *left, const RingRatio *right);

/**
 * @brief Adds two ratios.
 *
 * @param[in]  left  First addend [BORROWS].
 * @param[in]  right Second addend [BORROWS].
 * @param[out] sum   The sum [BORROWS].
 */
void ratio_add(const RingRatio *left, const RingRatio *right, RingRatio *sum);

/**
 * @brief Multiplies two ratios.
 *
 * @param[in]  left    First factor [BORROWS].
 * @param[in]  right   Second factor [BORROWS].
 * @param[out] product The product [BORROWS].
 */
void ratio_mul(const RingRatio *left, const RingRatio *right, RingRatio *product);

/**
 * @brief A ratio times numerator / denominator.
 *
 * @param[in]  ratio       The ratio [BORROWS].
 * @param[in]  numerator   The numerator of the factor.
 * @param[in]  denominator The denominator of the factor. It is not zero.
 * @param[out] result      The product [BORROWS].
 */
void ratio_scale(const RingRatio *ratio, int64_t numerator, int64_t denominator, RingRatio *result);

/**
 * @brief A ratio raised to a power of at least zero.
 *
 * @param[in]  ratio    The ratio [BORROWS].
 * @param[in]  exponent The power.
 * @param[out] result   The power of the ratio [BORROWS].
 */
void ratio_power(const RingRatio *ratio, uint32_t exponent, RingRatio *result);

/**
 * @brief The highest power of pi in a ratio.
 *
 * @param[in] ratio The ratio, not zero [BORROWS].
 * @return          The highest power.
 */
int32_t ratio_pi_degree(const RingRatio *ratio);

/**
 * @brief Writes a ratio as the reports print it.
 *
 * @param[in] ratio The ratio [BORROWS].
 * @param[in] out   The stream [BORROWS].
 */
void ratio_print(const RingRatio *ratio, FILE *out);

/**
 * @brief The ratio (factor * nu) pi^2: -4 nu pi^2 for the velocity, -8 nu pi^2 for the pressure.
 *
 * @param[in]  viscosity The viscosity.
 * @param[in]  factor    The integer factor.
 * @param[out] rate      The ratio [BORROWS].
 */
void decay_rate(Viscosity viscosity, int64_t factor, RingRatio *rate);

/* ---------------------------------------------------------------- scalar and vector fields */

/**
 * @brief Frees a scalar field and leaves it empty.
 *
 * @param[in,out] scalar The field [BORROWS].
 */
void scalar_free(TorusScalar *scalar);

/**
 * @brief The zero scalar field.
 *
 * @param[out] scalar The field [BORROWS].
 */
void scalar_zero(TorusScalar *scalar);

/**
 * @brief A copy of a scalar field.
 *
 * @param[in]  source The field [BORROWS].
 * @param[out] copy   The copy [BORROWS].
 */
void scalar_copy(const TorusScalar *source, TorusScalar *copy);

/**
 * @brief Whether a scalar field is zero.
 *
 * @param[in] scalar The field [BORROWS].
 * @return           1 where it is zero, else 0.
 */
int scalar_is_zero(const TorusScalar *scalar);

/**
 * @brief Whether two scalar fields are equal.
 *
 * @param[in] left  First field [BORROWS].
 * @param[in] right Second field [BORROWS].
 * @return          1 where they are equal, else 0.
 */
int scalar_equal(const TorusScalar *left, const TorusScalar *right);

/**
 * @brief Adds two scalar fields.
 *
 * @param[in]  left  First addend [BORROWS].
 * @param[in]  right Second addend [BORROWS].
 * @param[out] sum   The sum [BORROWS].
 */
void scalar_add(const TorusScalar *left, const TorusScalar *right, TorusScalar *sum);

/**
 * @brief The negative of a scalar field.
 *
 * @param[in]  scalar The field [BORROWS].
 * @param[out] result The negative [BORROWS].
 */
void scalar_neg(const TorusScalar *scalar, TorusScalar *result);

/**
 * @brief A scalar field times numerator / denominator.
 *
 * @param[in]  scalar      The field [BORROWS].
 * @param[in]  numerator   The numerator of the factor.
 * @param[in]  denominator The denominator of the factor. It is not zero.
 * @param[out] result      The product [BORROWS].
 */
void scalar_scale(const TorusScalar *scalar, int64_t numerator, int64_t denominator, TorusScalar *result);

/**
 * @brief A scalar field times a ratio.
 *
 * @param[in]  scalar The field [BORROWS].
 * @param[in]  ratio  The ratio [BORROWS].
 * @param[out] result The product [BORROWS].
 */
void scalar_times(const TorusScalar *scalar, const RingRatio *ratio, TorusScalar *result);

/**
 * @brief The derivative of a scalar field along one axis: each mode times 2 pi i k_axis.
 *
 * @param[in]  scalar The field [BORROWS].
 * @param[in]  axis   The axis, 0 to 2.
 * @param[out] result The derivative [BORROWS].
 */
void scalar_derivative(const TorusScalar *scalar, unsigned axis, TorusScalar *result);

/**
 * @brief The scalar field without its mean mode.
 *
 * @param[in]  scalar The field [BORROWS].
 * @param[out] result The field without the zero mode [BORROWS].
 */
void scalar_without_mean(const TorusScalar *scalar, TorusScalar *result);

/**
 * @brief The scalar field that keeps only the modes with |k|^2 equal to a shell.
 *
 * @param[in]  scalar The field [BORROWS].
 * @param[in]  shell  The value of |k|^2.
 * @param[out] result The masked field [BORROWS].
 */
void scalar_in_shell(const TorusScalar *scalar, int64_t shell, TorusScalar *result);

/**
 * @brief The scalar field without one mode.
 *
 * @param[in]  scalar The field [BORROWS].
 * @param[in]  mode   The mode to drop [BORROWS].
 * @param[out] result The field without that mode [BORROWS].
 */
void scalar_without_mode(const TorusScalar *scalar, const int32_t mode[TORUS_AXES], TorusScalar *result);

/**
 * @brief Whether a scalar field carries a mode.
 *
 * @param[in] scalar The field [BORROWS].
 * @param[in] mode   The mode [BORROWS].
 * @return           1 where the field carries it, else 0.
 */
int scalar_has_mode(const TorusScalar *scalar, const int32_t mode[TORUS_AXES]);

/**
 * @brief Frees a vector field and leaves it empty.
 *
 * @param[in,out] vector The field [BORROWS].
 */
void vec_free(TorusVector *vector);

/**
 * @brief The zero vector field.
 *
 * @param[out] vector The field [BORROWS].
 */
void vec_zero(TorusVector *vector);

/**
 * @brief A copy of a vector field.
 *
 * @param[in]  source The field [BORROWS].
 * @param[out] copy   The copy [BORROWS].
 */
void vec_copy(const TorusVector *source, TorusVector *copy);

/**
 * @brief Adds two vector fields.
 *
 * @param[in]  left  First addend [BORROWS].
 * @param[in]  right Second addend [BORROWS].
 * @param[out] sum   The sum [BORROWS].
 */
void vec_add(const TorusVector *left, const TorusVector *right, TorusVector *sum);

/**
 * @brief Subtracts the second vector field from the first.
 *
 * @param[in]  left       The minuend [BORROWS].
 * @param[in]  right      The subtrahend [BORROWS].
 * @param[out] difference The difference [BORROWS].
 */
void vec_sub(const TorusVector *left, const TorusVector *right, TorusVector *difference);

/**
 * @brief A vector field times numerator / denominator.
 *
 * @param[in]  vector      The field [BORROWS].
 * @param[in]  numerator   The numerator of the factor.
 * @param[in]  denominator The denominator of the factor. It is not zero.
 * @param[out] result      The product [BORROWS].
 */
void vec_scale(const TorusVector *vector, int64_t numerator, int64_t denominator, TorusVector *result);

/**
 * @brief A vector field times a ratio.
 *
 * @param[in]  vector The field [BORROWS].
 * @param[in]  ratio  The ratio [BORROWS].
 * @param[out] result The product [BORROWS].
 */
void vec_times(const TorusVector *vector, const RingRatio *ratio, TorusVector *result);

/**
 * @brief Whether a vector field is zero.
 *
 * @param[in] vector The field [BORROWS].
 * @return           1 where every component is zero, else 0.
 */
int vec_is_zero(const TorusVector *vector);

/**
 * @brief Whether two vector fields are equal.
 *
 * @param[in] left  First field [BORROWS].
 * @param[in] right Second field [BORROWS].
 * @return          1 where every component is equal, else 0.
 */
int vec_equal(const TorusVector *left, const TorusVector *right);

/**
 * @brief The Laplacian of a vector field: each mode times -4 pi^2 |k|^2.
 *
 * @param[in]  vector The field [BORROWS].
 * @param[out] result The Laplacian [BORROWS].
 */
void vec_laplacian(const TorusVector *vector, TorusVector *result);

/**
 * @brief The divergence of a vector field.
 *
 * @param[in]  vector The field [BORROWS].
 * @param[out] result The divergence [BORROWS].
 */
void divergence(const TorusVector *vector, TorusScalar *result);

/**
 * @brief The gradient of a scalar field.
 *
 * @param[in]  scalar The field [BORROWS].
 * @param[out] result The gradient [BORROWS].
 */
void gradient(const TorusScalar *scalar, TorusVector *result);

/**
 * @brief The curl of a vector field.
 *
 * @param[in]  vector The field [BORROWS].
 * @param[out] result The curl [BORROWS].
 */
void curl(const TorusVector *vector, TorusVector *result);

/**
 * @brief The advection (carrier . grad) carried, component by component.
 *
 * @param[in]  carrier The field that carries [BORROWS].
 * @param[in]  carried The field that the carrier moves [BORROWS].
 * @param[out] result  The advection [BORROWS].
 */
void advect(const TorusVector *carrier, const TorusVector *carried, TorusVector *result);

/**
 * @brief The pointwise dot product of two vector fields.
 *
 * @param[in]  left   First field [BORROWS].
 * @param[in]  right  Second field [BORROWS].
 * @param[out] result The product [BORROWS].
 */
void dot(const TorusVector *left, const TorusVector *right, TorusScalar *result);

/**
 * @brief The divergence-free part of a vector field, by the Leray projection, mode by mode.
 *
 * @param[in]  vector The field [BORROWS].
 * @param[out] result The projection [BORROWS].
 */
void leray(const TorusVector *vector, TorusVector *result);

/**
 * @brief The pressure whose gradient is the gradient part of a field: p_k = i (k . n_k) / (2 pi |k|^2).
 *
 * @param[in]  nonlinear The field [BORROWS].
 * @param[out] result    The pressure [BORROWS].
 */
void pressure_of(const TorusVector *nonlinear, TorusScalar *result);

/**
 * @brief The mean-free divergence-free field with a given curl: u_k = i (k x w_k) / (2 pi |k|^2).
 *
 * @param[in]  vorticity The curl [BORROWS].
 * @param[out] result    The velocity [BORROWS].
 */
void biot_savart(const TorusVector *vorticity, TorusVector *result);

/**
 * @brief The integral over the unit cell of left . right for real fields, by the sum of Parseval.
 *
 * @param[in]  left   First field [BORROWS].
 * @param[in]  right  Second field [BORROWS].
 * @param[out] result The integral [BORROWS].
 */
void parseval(const TorusVector *left, const TorusVector *right, RingRatio *result);

/**
 * @brief The modes that any component of a vector field carries.
 *
 * @param[in]  vector The field [BORROWS].
 * @param[out] list   The modes [BORROWS]. The caller frees list->modes.
 */
void modes_of(const TorusVector *vector, ModeList *list);

/**
 * @brief The value |k|^2 of a mode.
 *
 * @param[in] mode The mode [BORROWS].
 * @return         The value.
 */
int64_t norm_of(const int32_t mode[TORUS_AXES]);

/**
 * @brief Whether a vector field is real: the coefficient at -k is the conjugate of the one at k.
 *
 * @param[in] vector The field [BORROWS].
 * @return           1 where the field is real, else 0.
 */
int is_real_field(const TorusVector *vector);

/* ---------------------------------------------------------------- the recurrences */

/**
 * @brief The Taylor coefficients u_0..u_order of the velocity, and the pressures p_0..p_(order - 1).
 *
 * @param[in]  datum      The initial field u_0 [BORROWS].
 * @param[in]  viscosity  The viscosity.
 * @param[in]  order      The highest order.
 * @param[out] velocities Array of order + 1 fields [BORROWS].
 * @param[out] pressures  Array of order fields, or NULL where the caller needs none [BORROWS].
 */
void taylor_velocity(const TorusVector *datum, Viscosity viscosity, unsigned order, TorusVector *velocities,
                     TorusScalar *pressures);

/**
 * @brief The Taylor coefficients omega_0..omega_order of the vorticity, with velocities by Biot-Savart.
 *
 * @param[in]  datum_vorticity The initial vorticity [BORROWS].
 * @param[in]  viscosity       The viscosity.
 * @param[in]  order           The highest order.
 * @param[in]  stretch_term    Nonzero to keep the term of vortex stretch.
 * @param[out] vorticities     Array of order + 1 fields [BORROWS].
 */
void taylor_vorticity(const TorusVector *datum_vorticity, Viscosity viscosity, unsigned order, int stretch_term,
                      TorusVector *vorticities);

/**
 * @brief The order-step Taylor coefficient of the momentum equation with the divergence condition.
 *
 * @param[in]  velocities The velocity coefficients, at least step + 2 of them [BORROWS].
 * @param[in]  pressures  The pressure coefficients, at least step + 1 of them [BORROWS].
 * @param[in]  viscosity  The viscosity.
 * @param[in]  step       The order.
 * @param[out] result     The residual [BORROWS].
 */
void residual(const TorusVector *velocities, const TorusScalar *pressures, Viscosity viscosity, unsigned step,
              TorusVector *result);

/**
 * @brief C(row, column), by the multiplicative recurrence.
 *
 * @param[in] row    The row.
 * @param[in] column The column.
 * @return           The binomial coefficient.
 */
int64_t binomial(int64_t row, int64_t column);

/* ---------------------------------------------------------------- data */

/**
 * @brief (numerator / denominator) sin(2 pi x_axis).
 *
 * @param[in]  axis        The axis.
 * @param[in]  numerator   The numerator of the amplitude.
 * @param[in]  denominator The denominator of the amplitude.
 * @param[out] scalar      The field [BORROWS].
 */
void sine(unsigned axis, int64_t numerator, int64_t denominator, TorusScalar *scalar);

/**
 * @brief (numerator / denominator) cos(2 pi k.x) for a mode k.
 *
 * @param[in]  mode        The mode [BORROWS].
 * @param[in]  numerator   The numerator of the amplitude.
 * @param[in]  denominator The denominator of the amplitude.
 * @param[out] scalar      The field [BORROWS].
 */
void harmonic(const int32_t mode[TORUS_AXES], int64_t numerator, int64_t denominator, TorusScalar *scalar);

/**
 * @brief (numerator / denominator) cos(2 pi x_axis).
 *
 * @param[in]  axis        The axis.
 * @param[in]  numerator   The numerator of the amplitude.
 * @param[in]  denominator The denominator of the amplitude.
 * @param[out] scalar      The field [BORROWS].
 */
void cosine(unsigned axis, int64_t numerator, int64_t denominator, TorusScalar *scalar);

/**
 * @brief The Arnold-Beltrami-Childress field on the unit torus with amplitudes A, B and C, curl u = 2 pi u.
 *
 * @param[in]  amplitude_a The amplitude A, a whole number.
 * @param[in]  amplitude_b The amplitude B, a whole number.
 * @param[in]  amplitude_c The amplitude C, a whole number.
 * @param[out] vector      The field [BORROWS].
 */
void abc_field(int64_t amplitude_a, int64_t amplitude_b, int64_t amplitude_c, TorusVector *vector);

/**
 * @brief A divergence-free field that is not Beltrami, and whose nonlinear term is not a gradient.
 *
 * @param[out] vector The field [BORROWS].
 */
void generic_field(TorusVector *vector);

/**
 * @brief The field v(factor x) times factor: every mode index times the factor, and the amplitude too.
 *
 * @param[in]  vector The field [BORROWS].
 * @param[in]  factor The integer factor.
 * @param[out] result The stretched field [BORROWS].
 */
void vec_stretched(const TorusVector *vector, int32_t factor, TorusVector *result);

#endif

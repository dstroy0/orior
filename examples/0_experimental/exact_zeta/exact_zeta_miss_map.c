// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Catalog: EXP-x-030
/**
 * @file exact_zeta_miss_map.c
 * @brief Where the machine of Turing misses a zero, seen from the sampler.
 *
 * @note Each cell runs on the device by the multiple evaluation, on a coarse lattice and on a fine one. The run lists
 *       every point: its sign, S, Z and w = exp(i theta) F, with F the main sum. A coarse step hides a pair where it
 *       holds more of the zeros of the fine run than the coarse signs show. Each such pair is a miss.
 * @note Run build_exact_zeta.sh miss_map <binary> [first] [last] [coarse] [fine] [folder], beside this file. The
 *       modes are e <base> <cells> <heights> <rate> [folder] and carrier <base> <cells> <rate> [folder]. The modes
 *       ripple, pulse, source, primes and twist each take <base> <cells> <rate>. A folder without a name goes under
 *       build/miss_map.
 * @note With twist, the run lists each cell with the F'/F of the device. It checks that against the own differences
 *       of w on the fine lattice. It flags each step of the uniform coarse lattice where the step of Newton from either
 *       end places a source. The pulse of that source passes the clock inside the step. The run reads the share of the
 *       flagged steps, and the misses that they hold.
 * @note With ripple, the run locks the misses of the uniform lattice at the rate against the beats ln(n / m) of
 *       |F|^2. It reads the drag and swell of F at their dips against those of every point.
 * @note With pulse, the run reads each place where F passes near a zero of its own against the single pole. Its
 *       twist is a Lorentzian, and (swell, twist) is a circle. The run counts the misses inside each one.
 * @note With source, the run places the source of each pulse, the zero of F that it passes, and locks it against the
 *       beats. It sets the misses and sources against the density that the prime lines of F'/F place.
 * @note With primes, the run locks the sources on ln n for every n to N. The certified zeros of the window read
 *       Lambda(x) at every integer past N by Landau. A sieve grades the calls, and the witness of Proth proves them
 *       where the theorem of Proth reaches. The run sums psi(x) from them.
 * @note With carrier, each cell runs once on its fine lattice, and the run places the coarse lattice several ways.
 *       They are the uniform baseline at the rate, and the lattice even in theta / pi at the rate. They are the trap at
 *       the antinodes where Im(w) crosses zero, and the extrema of Z, where Z' = 0. They are a uniform lattice of the
 *       own point count of each trap, and the {1,1,2} and golden combs at the rate.
 * @note For each lattice, the run reads its points a zero, and its misses a thousand zeros against the floor of the
 *       pigeonhole. It also reads the width of the pickle, the share under us, and the extrema on the wrong side of
 *       zero. The coarse lattice chooses which certified points to compare, and it is not part of the proof.
 * @note With e, the run reads the same fields at heights an e-fold apart in t. The cells start at base, then at
 *       sqrt(e) times it, e times it and on. The coarse lattice of each height is every k-th point of its fine one. The
 *       code chooses k to hold rate points a zero at every height.
 * @note The run reads each field in the units of its height. These are the radius over its median, and the turn over
 *       its median turn a step. Where the fields at every height fall on one another, the shift of a height by e
 * carries them unchanged.
 * @note It sits in 0_experimental, and is an entry in the workbook of analytic number theory, on its rail. It proves
 *       no claim. It reads where the misses fall.
 * @note The ball: Z = 2 Re w + R. w turns with theta, and its size |w| = |F| does not depend on theta at all:
 *       |F|^2 = sum over m, n of (m n)^(-1/2) cos(t ln(m / n)), the beats of the rotations n^(-it).
 * @note The sampler is at its last certified coarse point before the miss, the center of a ball of radius |w| there.
 *       It heads along the step of w from the point before. A miss is the fine point between its two zeros where |Z| is
 *       largest, the bottom of the dip.
 * @note The run places the miss relative to the center, by its direction from the heading, 0 to 360 degrees. It also
 *       gives its distance in radii of the ball and in plain units.
 * @note The ball spins, and a straight heading falls off its curve to one side. Dead reckoning runs a steady arc
 *       through the last two coarse points. It turns at the rate that the heading turned over the last step. The run
 *       places the miss again from that track, with the spin taken out.
 * @note The run draws the curve of the fine points through each miss over and under the plane through the center. Its
 *       height is the change in Z from that of the center, in radii, up away from zero. The misses and the radii go to
 *       files of tab separated values beside the map.
 * @note The radius and the turn over a step at every coarse point are the baselines that the run reads the misses
 *       against. The floating point of the host follows that of CPython. That covers its complex division, its float
 *       repr, its compensated sum, its hypot and its round to even.
 */
#include "zeta_machine.h"
#include "../../../src/platform/platform.h"
#include "zeta_shuffle.h"

#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <fcntl.h>
#include <io.h>
/** @brief The separator that os.path.join of Python puts between a folder and a name. */
#define PATH_JOIN "\\"
#else
#include <sys/stat.h>
#define PATH_JOIN "/"
#endif

/** @brief Pi as a double, as math.pi holds it. */
#define PI_DOUBLE 3.141592653589793

/** @brief 2 pi as a double, the product 2 * math.pi of Python. */
#define TWO_PI (2.0 * PI_DOUBLE)

/** @brief The scale of the device as a double, 2^62. */
#define UNIT 4611686018427387904.0

/** @brief The panel and margin of the figures. */
#define PANEL 420
#define MARGIN 40

/** @brief The longest path of a folder or a file. */
#define PATH_BYTES 600

/* ---------------------------------------------------------------- memory */

/**
 * @brief Stops the program where an allocation fails, else returns the block.
 *
 * @param[in] block The block [OWNS].
 * @return          The same block [OWNS].
 */
static void *map_memory(void *block)
{
    if (block == NULL)
    {
        zeta_stop("out of memory");
    }
    return block;
}

/* ---------------------------------------------------------------- the floating point of CPython */

/** @brief A complex number, as CPython holds one. */
typedef struct
{
    double re; /**< The real part. */
    double im; /**< The imaginary part. */
} Complex;

/**
 * @brief A complex number from its parts.
 *
 * @param[in] re The real part.
 * @param[in] im The imaginary part.
 * @return       The number.
 */
static Complex cx(double re, double im)
{
    const Complex value = {re, im};
    return value;
}

/** @brief The difference of two complex numbers. */
static Complex c_sub(Complex left, Complex right)
{
    return cx(left.re - right.re, left.im - right.im);
}

/** @brief The product of two complex numbers, as _Py_c_prod of CPython takes it. */
static Complex c_mul(Complex left, Complex right)
{
    return cx((left.re * right.re) - (left.im * right.im), (left.re * right.im) + (left.im * right.re));
}

/**
 * @brief The quotient of two complex numbers, as _Py_c_quot of CPython takes it.
 *
 * @param[in] left  The dividend.
 * @param[in] right The divisor.
 * @return          The quotient, or 0 where the divisor is 0.
 */
static Complex c_div(Complex left, Complex right)
{
    const double abs_re = fabs(right.re);
    const double abs_im = fabs(right.im);
    if (abs_re >= abs_im)
    {
        if (abs_re == 0.0)
        {
            return cx(0.0, 0.0);
        }
        const double ratio = right.im / right.re;
        const double denom = right.re + (right.im * ratio);
        return cx((left.re + (left.im * ratio)) / denom, (left.im - (left.re * ratio)) / denom);
    }
    const double ratio = right.re / right.im;
    const double denom = (right.re * ratio) + right.im;
    return cx(((left.re * ratio) + left.im) / denom, ((left.im * ratio) - left.re) / denom);
}

/** @brief A complex times a real, each part alone, as CPython 3.14 multiplies them. */
static Complex c_scale(Complex value, double factor)
{
    return cx(value.re * factor, value.im * factor);
}

/** @brief A complex over a real, each part alone, as CPython 3.14 divides them. */
static Complex c_over(Complex value, double divisor)
{
    return cx(value.re / divisor, value.im / divisor);
}

/** @brief A real plus a complex, as CPython 3.14 adds them: the real part alone. */
static Complex c_plus_real(double real, Complex value)
{
    return cx(real + value.re, value.im);
}

/** @brief A real less a complex, as CPython 3.14 subtracts them. */
static Complex c_real_less(double real, Complex value)
{
    return cx(real - value.re, -value.im);
}

/** @brief The size of a complex, as abs of a complex in CPython takes it, by hypot of the C library. */
static double c_abs(Complex value)
{
    return hypot(value.re, value.im);
}

/** @brief The phase of a complex, by atan2. */
static double c_angle(Complex value)
{
    return atan2(value.im, value.re);
}

/** @brief Whether a complex is 0, as a comparison with 0j of Python tests it. */
static int c_zero(Complex value)
{
    return (value.re == 0.0) && (value.im == 0.0);
}

/** @brief Degrees from radians, as math.degrees of CPython multiplies. */
static double degrees(double value)
{
    return value * (180.0 / PI_DOUBLE);
}

/** @brief Radians from degrees, as math.radians of CPython multiplies. */
static double radians(double value)
{
    return value * (PI_DOUBLE / 180.0);
}

/**
 * @brief The remainder of two floats, as x % y of CPython takes it: the sign of the divisor.
 *
 * @param[in] dividend The dividend.
 * @param[in] divisor  The divisor.
 * @return             The remainder.
 */
static double py_mod(double dividend, double divisor)
{
    double remainder = fmod(dividend, divisor);
    if (remainder != 0.0)
    {
        if ((divisor < 0.0) != (remainder < 0.0))
        {
            remainder += divisor;
        }
    }
    else
    {
        remainder = copysign(0.0, divisor);
    }
    return remainder;
}

/**
 * @brief The floor of the quotient of two floats, as x // y of CPython takes it.
 *
 * @param[in] dividend The dividend.
 * @param[in] divisor  The divisor.
 * @return             The floor of the quotient.
 */
static double py_floordiv(double dividend, double divisor)
{
    const double remainder = fmod(dividend, divisor);
    double quotient = (dividend - remainder) / divisor;
    if ((remainder != 0.0) && ((divisor < 0.0) != (remainder < 0.0)))
    {
        quotient -= 1.0;
    }
    if (quotient == 0.0)
    {
        return copysign(0.0, dividend / divisor);
    }
    double whole = floor(quotient);
    if ((quotient - whole) > 0.5)
    {
        whole += 1.0;
    }
    return whole;
}

/**
 * @brief The nearest whole number, ties to even, as round(x) of CPython takes it.
 *
 * @param[in] value The value.
 * @return          The whole number.
 */
static int64_t py_round(double value)
{
    // The values that the run rounds are counts and places, far below 2^63.
    return (int64_t)nearbyint(value);
}

/**
 * @brief The shortest digits of a double that read back to it, as repr of CPython writes them.
 *
 * @param[in]  value    The double.
 * @param[in]  dot_zero 1 to write ".0" after a whole number, as repr of a float does; 0 as repr of a complex does.
 * @param[out] text     The buffer [BORROWS].
 * @param[in]  capacity Bytes of the buffer.
 */
static void py_repr(double value, int dot_zero, char *text, size_t capacity)
{
    if (isnan(value))
    {
        snprintf(text, capacity, "nan");
        return;
    }
    if (isinf(value))
    {
        snprintf(text, capacity, (value < 0.0) ? "-inf" : "inf");
        return;
    }
    if (value == 0.0)
    {
        snprintf(text, capacity, "%s%s", signbit(value) ? "-0" : "0", dot_zero ? ".0" : "");
        return;
    }
    char digits[64];
    int precision = 1;
    for (; precision <= 17; precision += 1)
    {
        snprintf(digits, sizeof(digits), "%.*e", precision - 1, value);
        if (strtod(digits, NULL) == value)
        {
            break;
        }
    }
    const char *const mark = strchr(digits, 'e');
    const int exponent = (mark != NULL) ? atoi(mark + 1) : 0;
    if ((exponent < -4) || (exponent >= 16))
    {
        snprintf(text, capacity, "%s", digits);
        return;
    }
    const int decimals = ((precision - 1 - exponent) > 0) ? (precision - 1 - exponent) : 0;
    snprintf(digits, sizeof(digits), "%.*f", decimals, value);
    if ((strchr(digits, '.') == NULL) && dot_zero)
    {
        snprintf(text, capacity, "%.60s.0", digits);
    }
    else
    {
        snprintf(text, capacity, "%s", digits);
    }
}

/**
 * @brief The repr of a complex, as CPython writes it: (re+imj), or imj where the real part is +0.
 *
 * @param[in]  value    The complex.
 * @param[out] text     The buffer [BORROWS].
 * @param[in]  capacity Bytes of the buffer.
 */
static void complex_repr(Complex value, char *text, size_t capacity)
{
    char re[64];
    char im[64];
    py_repr(value.im, 0, im, sizeof(im));
    if ((value.re == 0.0) && !signbit(value.re))
    {
        snprintf(text, capacity, "%sj", im);
        return;
    }
    py_repr(value.re, 0, re, sizeof(re));
    const int plus = !signbit(value.im) && !isnan(value.im);
    snprintf(text, capacity, "(%s%s%sj)", re, plus ? "+" : "", im);
}

/** @brief A double and the error that its sum drops, as CPython holds them. */
typedef struct
{
    double high; /**< The sum. */
    double low;  /**< The error. */
} DoubleLength;

/** @brief A product and its exact error, as dl_mul of CPython takes them by a fused multiply and add. */
static DoubleLength length_mul(double left, double right)
{
    const double product = left * right;
    const DoubleLength value = {product, fma(left, right, -product)};
    return value;
}

/** @brief A sum and its exact error where the first term is the larger, as dl_fast_sum of CPython takes them. */
static DoubleLength length_fast_sum(double left, double right)
{
    const double total = left + right;
    const double part = total - left;
    const DoubleLength value = {total, right - part};
    return value;
}

/**
 * @brief The length of a vector of two, as math.hypot of CPython takes it by its vector_norm.
 *
 * @param[in] first  The first part.
 * @param[in] second The second part.
 * @return           The length, correctly rounded in most cases.
 */
static double py_hypot(double first, double second)
{
    double values[2] = {fabs(first), fabs(second)};
    if (isinf(values[0]) || isinf(values[1]))
    {
        return INFINITY;
    }
    if (isnan(values[0]) || isnan(values[1]))
    {
        return NAN;
    }
    const double most = (values[0] > values[1]) ? values[0] : values[1];
    if (most == 0.0)
    {
        return most;
    }
    int max_e = 0;
    (void)frexp(most, &max_e);
    if (max_e < -1023)
    {
        values[0] /= 2.2250738585072014e-308;
        values[1] /= 2.2250738585072014e-308;
        return 2.2250738585072014e-308 * py_hypot(values[0], values[1]);
    }
    const double scale = ldexp(1.0, -max_e);
    double csum = 1.0;
    double frac1 = 0.0;
    double frac2 = 0.0;
    for (int at = 0; at < 2; at += 1)
    {
        const double scaled = values[at] * scale;
        const DoubleLength square = length_mul(scaled, scaled);
        const DoubleLength sum = length_fast_sum(csum, square.high);
        csum = sum.high;
        frac1 += square.low;
        frac2 += sum.low;
    }
    double root = sqrt((csum - 1.0) + (frac1 + frac2));
    const DoubleLength square = length_mul(-root, root);
    const DoubleLength sum = length_fast_sum(csum, square.high);
    csum = sum.high;
    frac1 += square.low;
    frac2 += sum.low;
    const double rest = (csum - 1.0) + (frac1 + frac2);
    root += rest / (2.0 * root);
    return root / scale;
}

/** @brief A compensated sum of doubles, as sum() of CPython 3.14 adds them. */
typedef struct
{
    double high; /**< The sum. */
    double low;  /**< The compensation. */
} Sum;

/**
 * @brief Adds a double to a compensated sum, by the method of Neumaier.
 *
 * @param[in,out] sum   The sum [BORROWS].
 * @param[in]     value The double.
 */
static void sum_add(Sum *sum, double value)
{
    const double total = sum->high + value;
    if (fabs(sum->high) >= fabs(value))
    {
        sum->low += (sum->high - total) + value;
    }
    else
    {
        sum->low += (value - total) + sum->high;
    }
    sum->high = total;
}

/** @brief The double of a compensated sum. */
static double sum_value(const Sum *sum)
{
    if ((sum->low != 0.0) && isfinite(sum->low))
    {
        return sum->high + sum->low;
    }
    return sum->high;
}

/* ---------------------------------------------------------------- lists */

/** @brief A growable list of doubles. */
typedef struct
{
    double *values;  /**< The values [OWNS]. */
    size_t count;    /**< How many. */
    size_t capacity; /**< How many the array holds. */
} Doubles;

/**
 * @brief Adds a double to the end of a list.
 *
 * @param[in,out] list  The list [BORROWS].
 * @param[in]     value The double.
 */
static void doubles_push(Doubles *list, double value)
{
    if (list->count >= list->capacity)
    {
        list->capacity = (list->capacity == 0u) ? 256u : (list->capacity * 2u);
        list->values = (double *)map_memory(realloc(list->values, list->capacity * sizeof(double)));
    }
    list->values[list->count] = value;
    list->count += 1u;
}

/** @brief The order of two doubles, for qsort. */
static int double_order(const void *left, const void *right)
{
    const double first = *(const double *)left;
    const double second = *(const double *)right;
    return (first < second) ? -1 : ((first > second) ? 1 : 0);
}

/** @brief Sorts a list of doubles in place. */
static void doubles_sort(Doubles *list)
{
    if (list->count > 1u)
    {
        qsort(list->values, list->count, sizeof(double), double_order);
    }
}

/**
 * @brief A sorted copy of doubles.
 *
 * @param[in] values The values [BORROWS].
 * @param[in] count  How many.
 * @return           The copy [OWNS].
 */
static double *sorted_copy(const double *values, size_t count)
{
    double *const copy = (double *)map_memory(malloc((count + 1u) * sizeof(double)));
    if (count > 0u)
    {
        memcpy(copy, values, count * sizeof(double));
        qsort(copy, count, sizeof(double), double_order);
    }
    return copy;
}

/**
 * @brief The value at a place of the sorted values.
 *
 * @param[in] values The values [BORROWS].
 * @param[in] count  How many.
 * @param[in] at     The place.
 * @return           The value.
 */
static double sorted_at(const double *values, size_t count, size_t at)
{
    double *const copy = sorted_copy(values, count);
    const double result = copy[at];
    free(copy);
    return result;
}

/** @brief The value at the middle of the sorted values, or 0 where there are none, as median of the Python. */
static double median_of(const double *values, size_t count)
{
    return (count == 0u) ? 0.0 : sorted_at(values, count, count / 2u);
}

/** @brief The ninety-fifth percentile of values, or 1 where there are none. */
static double ninety_fifth(const double *values, size_t count)
{
    if (count == 0u)
    {
        return 1.0;
    }
    const size_t at = (((95u * count) / 100u) < (count - 1u)) ? ((95u * count) / 100u) : (count - 1u);
    return sorted_at(values, count, at);
}

/** @brief The place int(fraction * (count - 1)) of a sorted list, as the quantiles of the Python take it. */
static size_t quantile_at(double fraction, size_t count)
{
    // The product is a place in the list, below its count.
    return (size_t)(fraction * (double)(count - 1u));
}

/** @brief Sorts a list of doubles in place, and returns the value at a place. */
static double sorted_place(Doubles *values, size_t at)
{
    doubles_sort(values);
    return values->values[at];
}

/** @brief A growable list of places. */
typedef struct
{
    size_t *values;  /**< The places [OWNS]. */
    size_t count;    /**< How many. */
    size_t capacity; /**< How many the array holds. */
} Places;

/**
 * @brief Adds a place to the end of a list.
 *
 * @param[in,out] list  The list [BORROWS].
 * @param[in]     value The place.
 */
static void places_push(Places *list, size_t value)
{
    if (list->count >= list->capacity)
    {
        list->capacity = (list->capacity == 0u) ? 256u : (list->capacity * 2u);
        list->values = (size_t *)map_memory(realloc(list->values, list->capacity * sizeof(size_t)));
    }
    list->values[list->count] = value;
    list->count += 1u;
}

/* ---------------------------------------------------------------- the listed points */

/** @brief A listed point: its sign, S, Z, w, and, with the twist, F'/F. */
typedef struct
{
    int64_t sign;      /**< The certified sign. */
    int64_t s;         /**< S. */
    double z;          /**< Z. */
    Complex w;         /**< w. */
    Complex ratio;     /**< F'/F, with the listing word 2. */
    char low_text[48]; /**< theta / pi less its bound at 2^-62, in hexadecimal, with the listing word 3. */
} MapPoint;

/** @brief The points of a cell. */
typedef struct
{
    MapPoint *points; /**< The points [OWNS]. */
    size_t count;     /**< How many. */
    int64_t shift;    /**< The shift of the twist. */
} Listing;

/**
 * @brief A signed hexadecimal integer of up to 128 bits, correctly rounded to a double, as float() of Python rounds.
 *
 * @param[in] text The text [BORROWS].
 * @return         The double.
 */
static double hex_double(const char *text)
{
    const int negative = text[0] == '-';
    uint64_t high = 0u;
    uint64_t low = 0u;
    for (const char *at = text + (negative ? 1 : 0); *at != '\0'; at += 1)
    {
        const char letter = *at;
        uint64_t digit = 16u;
        if ((letter >= '0') && (letter <= '9'))
        {
            digit = (uint64_t)(letter - '0');
        }
        else if ((letter >= 'a') && (letter <= 'f'))
        {
            digit = 10u + (uint64_t)(letter - 'a');
        }
        else if ((letter >= 'A') && (letter <= 'F'))
        {
            digit = 10u + (uint64_t)(letter - 'A');
        }
        if (digit >= 16u)
        {
            break;
        }
        high = (high << 4u) | (low >> 60u);
        low = (low << 4u) | digit;
    }
    double value = 0.0;
    if (high == 0u)
    {
        // The conversion of a 64-bit integer to a double rounds to the nearest, ties to even.
        value = (double)low;
    }
    else
    {
        unsigned bits = 64u;
        while (((high >> (bits - 1u)) & 1u) == 0u)
        {
            bits -= 1u;
        }
        // The top 64 bits of the value, and one sticky bit for the bits below them. The value then rounds once.
        const uint64_t top = (bits == 64u) ? high : ((high << (64u - bits)) | (low >> bits));
        const uint64_t rest = (bits == 64u) ? low : (low & ((1ull << bits) - 1u));
        // The count of bits is at most 64.
        value = ldexp((double)(top | ((rest != 0u) ? 1u : 0u)), (int)bits);
    }
    return negative ? -value : value;
}

/**
 * @brief The words of a line, cut at spaces and line ends.
 *
 * @param[in,out] line  The line [BORROWS]. The function cuts it in place.
 * @param[out]    words The words [BORROWS]. 16 of them at most.
 * @return              How many.
 */
static size_t words_of(char *line, char *words[16])
{
    size_t found = 0u;
    char *cursor = line;
    while ((*cursor != '\0') && (found < 16u))
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
        words[found] = cursor;
        found += 1u;
        while ((*cursor != '\0') && (*cursor != ' ') && (*cursor != '\r') && (*cursor != '\n'))
        {
            cursor += 1;
        }
    }
    return found;
}

/**
 * @brief The points of a cell from the output of the device.
 *
 * @param[in]  path    The output [BORROWS].
 * @param[in]  listing The listing word: 1 for the plain points, 2 with the twist, 3 with the bounds of theta / pi.
 * @param[out] out     The points [BORROWS].
 */
static void read_listing(const char *path, int listing, Listing *out)
{
    FILE *const handle = fopen(path, "r");
    if (handle == NULL)
    {
        zeta_stop("the listing of the cell is gone");
    }
    size_t capacity = 4096u;
    out->points = (MapPoint *)map_memory(malloc(capacity * sizeof(MapPoint)));
    out->count = 0u;
    out->shift = 0;
    static char line[1 << 16];
    while (fgets(line, sizeof(line), handle) != NULL)
    {
        const int twist = strncmp(line, "twist", 5u) == 0;
        if (!twist && (strncmp(line, "point", 5u) != 0))
        {
            continue;
        }
        char *words[16];
        const size_t found = words_of(line, words);
        if (twist)
        {
            out->shift = (found > 1u) ? atoll(words[1]) : 0;
            continue;
        }
        if (found < 6u)
        {
            continue;
        }
        if (out->count >= capacity)
        {
            capacity *= 2u;
            out->points = (MapPoint *)map_memory(realloc(out->points, capacity * sizeof(MapPoint)));
        }
        MapPoint *const point = &out->points[out->count];
        memset(point, 0, sizeof(*point));
        // The sign and S are small whole numbers, exact as doubles.
        point->sign = (int64_t)hex_double(words[1]);
        point->s = (int64_t)hex_double(words[2]);
        point->z = hex_double(words[3]) / UNIT;
        point->w = cx(hex_double(words[4]) / UNIT, hex_double(words[5]) / UNIT);
        if ((listing == 2) && (found > 7u))
        {
            // The shift is a small count of bits.
            const double power = ldexp(1.0, (int)out->shift);
            const Complex turned = c_scale(cx(hex_double(words[6]) / UNIT, hex_double(words[7]) / UNIT), power);
            point->ratio = (c_abs(point->w) != 0.0) ? c_div(turned, point->w) : cx(0.0, 0.0);
            point->s = 0;
        }
        if ((listing == 3) && (found > 6u))
        {
            snprintf(point->low_text, sizeof(point->low_text), "%s", words[6]);
        }
        out->count += 1u;
    }
    fclose(handle);
}

/**
 * @brief Cell nu at 2^p points by the multiple evaluation, listed.
 *
 * @param[in]  binary    The device program [BORROWS].
 * @param[in]  constants The constants [BORROWS].
 * @param[in]  nu        The cell.
 * @param[in]  bits      The bits of the lattice.
 * @param[in]  listing   The listing word.
 * @param[out] out       The points [BORROWS].
 * @return               The failed host checks.
 */
static int64_t run_listed(const char *binary, const TuringConstants *constants, int64_t nu, int64_t bits, int listing,
                          Listing *out)
{
    platform_line_flushed();
    TuringCell *const cell = (TuringCell *)map_memory(malloc(sizeof(TuringCell)));
    OriorExactInteger bound;
    turing_run_cell(binary, constants, nu, bits, TURING_TRANSFORM, 0, listing, cell, &bound);
    read_listing(cell->path, listing, out);
    (void)remove(cell->path);
    const int64_t failed = cell->failed;
    free(cell);
    return failed;
}

/** @brief The bits of the fine lattice for a cell at a rate, ceil(log2(8 rate z)). */
static int64_t fine_bits(double rate, double rise)
{
    // The bits of a lattice are a small count.
    return (int64_t)ceil(log2((8.0 * rate) * rise));
}

/** @brief The step of the coarse lattice for a cell at a rate, max(2, round(2^fine / (rate z))). */
static int64_t coarse_step(int64_t bits, double rate, double rise)
{
    // The bits of a lattice are a small count.
    const int64_t step = py_round(ldexp(1.0, (int)bits) / (rate * rise));
    return (step > 2) ? step : 2;
}

/**
 * @brief x^2 at each fine point of cell nu, nu^2 + j (2 nu + 1) / count, as the Python takes it.
 *
 * @param[in] nu    The cell.
 * @param[in] count The fine points.
 * @return          The values [OWNS].
 */
static double *squares_of(int64_t nu, size_t count)
{
    double *const values = (double *)map_memory(malloc((count + 1u) * sizeof(double)));
    for (size_t lane = 0u; lane < count; lane += 1u)
    {
        // The numerator stays below 2^53, and the quotient rounds once.
        const double part = (double)((int64_t)lane * ((2 * nu) + 1)) / (double)count;
        values[lane] = (double)(nu * nu) + part;
    }
    return values;
}

/* ---------------------------------------------------------------- the zeros and the misses */

/** @brief A sign change between two certified points: the two lanes it lies between. */
typedef struct
{
    size_t before; /**< The lane before the change. */
    size_t after;  /**< The lane after it. */
} Zero;

/** @brief A list of zeros. */
typedef struct
{
    Zero *values; /**< The zeros [OWNS]. */
    size_t count; /**< How many. */
} Zeros;

/**
 * @brief Each sign change between consecutive certified points, and the certified points.
 *
 * @param[in]  points    The points [BORROWS].
 * @param[in]  count     How many.
 * @param[out] zeros     The changes [BORROWS].
 * @param[out] certified The certified points [BORROWS], or NULL.
 */
static void zeros_of(const MapPoint *points, size_t count, Zeros *zeros, Places *certified)
{
    Places kept = {NULL, 0u, 0u};
    for (size_t lane = 0u; lane < count; lane += 1u)
    {
        if (points[lane].sign != 0)
        {
            places_push(&kept, lane);
        }
    }
    zeros->values = (Zero *)map_memory(malloc((kept.count + 1u) * sizeof(Zero)));
    zeros->count = 0u;
    for (size_t at = 0u; (at + 1u) < kept.count; at += 1u)
    {
        if (points[kept.values[at]].sign != points[kept.values[at + 1u]].sign)
        {
            zeros->values[zeros->count].before = kept.values[at];
            zeros->values[zeros->count].after = kept.values[at + 1u];
            zeros->count += 1u;
        }
    }
    if (certified != NULL)
    {
        *certified = kept;
    }
    else
    {
        free(kept.values);
    }
}

/** @brief How many sign changes the points hold. */
static size_t zero_count(const MapPoint *points, size_t count)
{
    Zeros zeros = {NULL, 0u};
    zeros_of(points, count, &zeros, NULL);
    const size_t found = zeros.count;
    free(zeros.values);
    return found;
}

/** @brief A miss: the coarse step it hides in, and the two fine zeros. */
typedef struct
{
    size_t start;  /**< The coarse point before. */
    size_t finish; /**< The coarse point after. */
    Zero first;    /**< The first fine zero. */
    Zero second;   /**< The second fine zero. */
} Miss;

/** @brief A growable list of misses. */
typedef struct
{
    Miss *values;    /**< The misses [OWNS]. */
    size_t count;    /**< How many. */
    size_t capacity; /**< How many the array holds. */
} Misses;

/** @brief A gap between two fine zeros in a step, by its size, for a stable sort. */
typedef struct
{
    int64_t size; /**< The gap. */
    size_t at;    /**< Its place. */
} Gap;

/** @brief The order of two gaps: by size, then by place, as the stable sort of Python keeps them. */
static int gap_order(const void *left, const void *right)
{
    const Gap *const first = (const Gap *)left;
    const Gap *const second = (const Gap *)right;
    if (first->size != second->size)
    {
        return (first->size < second->size) ? -1 : 1;
    }
    return (first->at < second->at) ? -1 : ((first->at > second->at) ? 1 : 0);
}

/**
 * @brief Each pair that a coarse lattice hides: the coarse step it hides in, and the two fine zeros.
 *
 * @param[in]  coarse       The coarse points [BORROWS].
 * @param[in]  coarse_count How many.
 * @param[in]  places       The fine place of each coarse point [BORROWS].
 * @param[in]  fine         The fine points [BORROWS].
 * @param[in]  fine_count   How many.
 * @param[out] misses       The misses [BORROWS].
 * @note In each step, the code pairs the zeros past those the coarse signs show, the nearest gaps first.
 */
static void hidden_pairs(const MapPoint *coarse, size_t coarse_count, const size_t *places, const MapPoint *fine,
                         size_t fine_count, Misses *misses)
{
    Zeros fine_zeros = {NULL, 0u};
    zeros_of(fine, fine_count, &fine_zeros, NULL);
    Zeros coarse_zeros = {NULL, 0u};
    Places certified = {NULL, 0u, 0u};
    zeros_of(coarse, coarse_count, &coarse_zeros, &certified);
    free(coarse_zeros.values);
    misses->values = NULL;
    misses->count = 0u;
    misses->capacity = 0u;
    Zero *const inside = (Zero *)map_memory(malloc((fine_zeros.count + 1u) * sizeof(Zero)));
    Gap *const gaps = (Gap *)map_memory(malloc((fine_zeros.count + 1u) * sizeof(Gap)));
    unsigned char *const taken = (unsigned char *)map_memory(calloc(fine_zeros.count + 2u, 1u));
    size_t at = 0u;
    for (size_t pair = 0u; (pair + 1u) < certified.count; pair += 1u)
    {
        const size_t start = certified.values[pair];
        const size_t finish = certified.values[pair + 1u];
        const size_t low = places[start];
        const size_t high = places[finish];
        size_t inside_count = 0u;
        while ((at < fine_zeros.count) && (fine_zeros.values[at].after <= high))
        {
            if (fine_zeros.values[at].before >= low)
            {
                inside[inside_count] = fine_zeros.values[at];
                inside_count += 1u;
            }
            at += 1u;
        }
        const int64_t seen = coarse[start].sign != coarse[finish].sign;
        // The count of zeros in a step is small.
        const int64_t extra = (int64_t)inside_count - seen;
        if (extra < 2)
        {
            continue;
        }
        for (size_t gap = 0u; (gap + 1u) < inside_count; gap += 1u)
        {
            // The lanes are below 2^63.
            gaps[gap].size = (int64_t)inside[gap + 1u].before - (int64_t)inside[gap].before;
            gaps[gap].at = gap;
        }
        qsort(gaps, inside_count - 1u, sizeof(Gap), gap_order);
        memset(taken, 0, inside_count + 1u);
        int64_t taken_count = 0;
        for (size_t gap = 0u; gap < (inside_count - 1u); gap += 1u)
        {
            const size_t first = gaps[gap].at;
            if ((taken_count / 2) >= (extra / 2))
            {
                break;
            }
            if ((taken[first] != 0u) || (taken[first + 1u] != 0u))
            {
                continue;
            }
            taken[first] = 1u;
            taken[first + 1u] = 1u;
            taken_count += 2;
            if (misses->count >= misses->capacity)
            {
                misses->capacity = (misses->capacity == 0u) ? 64u : (misses->capacity * 2u);
                misses->values = (Miss *)map_memory(realloc(misses->values, misses->capacity * sizeof(Miss)));
            }
            Miss *const miss = &misses->values[misses->count];
            miss->start = start;
            miss->finish = finish;
            miss->first = inside[first];
            miss->second = inside[first + 1u];
            misses->count += 1u;
        }
    }
    free(inside);
    free(gaps);
    free(taken);
    free(certified.values);
    free(fine_zeros.values);
}

/**
 * @brief The misses of a coarse lattice that is every lift-th fine point, as misses_of of the Python reads them.
 *
 * @param[in]  coarse       The coarse points [BORROWS].
 * @param[in]  coarse_count How many.
 * @param[in]  fine         The fine points [BORROWS].
 * @param[in]  fine_count   How many.
 * @param[in]  lift         The fine steps a coarse step.
 * @param[out] misses       The misses [BORROWS].
 */
static void misses_of(const MapPoint *coarse, size_t coarse_count, const MapPoint *fine, size_t fine_count, size_t lift,
                      Misses *misses)
{
    size_t *const places = (size_t *)map_memory(malloc((coarse_count + 1u) * sizeof(size_t)));
    for (size_t at = 0u; at < coarse_count; at += 1u)
    {
        places[at] = at * lift;
    }
    hidden_pairs(coarse, coarse_count, places, fine, fine_count, misses);
    free(places);
}

/**
 * @brief The misses of a coarse lattice of fine places, as misses_on of the Python reads them.
 *
 * @param[in]  places     The fine places [BORROWS].
 * @param[in]  fine       The fine points [BORROWS].
 * @param[in]  fine_count How many.
 * @param[out] misses     The misses [BORROWS].
 */
static void misses_on(const Places *places, const MapPoint *fine, size_t fine_count, Misses *misses)
{
    MapPoint *const coarse = (MapPoint *)map_memory(malloc((places->count + 1u) * sizeof(MapPoint)));
    for (size_t at = 0u; at < places->count; at += 1u)
    {
        coarse[at] = fine[places->values[at]];
    }
    hidden_pairs(coarse, places->count, places->values, fine, fine_count, misses);
    free(coarse);
}

/**
 * @brief The fine point between the two zeros of a miss where |Z| is largest, the first of equals.
 *
 * @param[in] fine The fine points [BORROWS].
 * @param[in] miss The miss [BORROWS].
 * @return         The point.
 */
static size_t dip_of(const MapPoint *fine, const Miss *miss)
{
    size_t dip = miss->first.after;
    for (size_t lane = miss->first.after; lane <= miss->second.before; lane += 1u)
    {
        if (fabs(fine[lane].z) > fabs(fine[dip].z))
        {
            dip = lane;
        }
    }
    return dip;
}

/* ---------------------------------------------------------------- the misses placed */

/** @brief The direction of a complex in degrees, from 0 to 360. */
static double direction_degrees(Complex value)
{
    return py_mod(degrees(c_angle(value)), 360.0);
}

/** @brief A miss placed from the center at its coarse point. */
typedef struct
{
    int64_t nu;            /**< The cell. */
    int64_t dip_s;         /**< S at the dip. */
    double step_place;     /**< The place u of the dip in the step. */
    double angle;          /**< Its direction from the heading. */
    double radii;          /**< Its distance in radii. */
    double units;          /**< Its distance in units. */
    double residual_angle; /**< Its direction from the steady arc. */
    double residual_radii; /**< Its distance from the arc in radii. */
    double residual_steps; /**< Its distance from the arc in steps. */
    double radius;         /**< The radius at the center. */
    double spin;           /**< The turn over the last step. */
    double seen_turn;      /**< The turn across the step, as the coarse chords show it. */
    double true_turn;      /**< The own turn of the tip across the step. */
    double speed;          /**< The size of the heading. */
    double dip_radius;     /**< |w| at the dip. */
    double along;          /**< The part along the track, in steps. */
    double cross;          /**< The part across the track, in steps. */
    double d_steps;        /**< The distance from the center in steps. */
    double (*curve)[3];    /**< The curve of the fine points from the start of the step to its end [OWNS]. */
    size_t curve_count;    /**< How many. */
} Placed;

/** @brief The fields of a placed miss that the figures and medians read. */
typedef enum
{
    FIELD_ANGLE,
    FIELD_RADII,
    FIELD_UNITS,
    FIELD_RESIDUAL_ANGLE,
    FIELD_RESIDUAL_RADII,
    FIELD_RESIDUAL_STEPS,
    FIELD_RADIUS,
    FIELD_SPIN,
    FIELD_SPIN_SIZE,
    FIELD_DIP_RADIUS
} PlacedField;

/** @brief One field of a placed miss. */
static double placed_field(const Placed *placed, PlacedField field)
{
    switch (field)
    {
    case FIELD_ANGLE:
        return placed->angle;
    case FIELD_RADII:
        return placed->radii;
    case FIELD_UNITS:
        return placed->units;
    case FIELD_RESIDUAL_ANGLE:
        return placed->residual_angle;
    case FIELD_RESIDUAL_RADII:
        return placed->residual_radii;
    case FIELD_RESIDUAL_STEPS:
        return placed->residual_steps;
    case FIELD_RADIUS:
        return placed->radius;
    case FIELD_SPIN:
        return placed->spin;
    case FIELD_SPIN_SIZE:
        return fabs(placed->spin);
    case FIELD_DIP_RADIUS:
        return placed->dip_radius;
    default:
        return 0.0;
    }
}

/** @brief A growable list of placed misses. */
typedef struct
{
    Placed *values;  /**< The misses [OWNS]. */
    size_t count;    /**< How many. */
    size_t capacity; /**< How many the array holds. */
} Placements;

/**
 * @brief Adds a placed miss to the end of a list.
 *
 * @param[in,out] list  The list [BORROWS].
 * @param[in]     value The miss [BORROWS].
 */
static void placements_push(Placements *list, const Placed *value)
{
    if (list->count >= list->capacity)
    {
        list->capacity = (list->capacity == 0u) ? 64u : (list->capacity * 2u);
        list->values = (Placed *)map_memory(realloc(list->values, list->capacity * sizeof(Placed)));
    }
    list->values[list->count] = *value;
    list->count += 1u;
}

/** @brief Frees the curves and the array of a list of placed misses. */
static void placements_free(Placements *list)
{
    for (size_t at = 0u; at < list->count; at += 1u)
    {
        free(list->values[at].curve);
    }
    free(list->values);
    list->values = NULL;
    list->count = 0u;
    list->capacity = 0u;
}

/**
 * @brief The values of one field of the misses.
 *
 * @param[in]  misses The misses [BORROWS].
 * @param[in]  field  The field.
 * @param[out] out    The values [BORROWS]. The function allocates them.
 */
static void field_values(const Placements *misses, PlacedField field, Doubles *out)
{
    out->values = (double *)map_memory(malloc((misses->count + 1u) * sizeof(double)));
    out->count = misses->count;
    out->capacity = misses->count + 1u;
    for (size_t at = 0u; at < misses->count; at += 1u)
    {
        out->values[at] = placed_field(&misses->values[at], field);
    }
}

/** @brief The median of one field of the misses, or 0 where there are none. */
static double field_median(const Placements *misses, PlacedField field)
{
    Doubles values = {NULL, 0u, 0u};
    field_values(misses, field, &values);
    const double result = median_of(values.values, values.count);
    free(values.values);
    return result;
}

/** @brief The ninety-fifth percentile of one field of the misses. */
static double field_ninety_fifth(const Placements *misses, PlacedField field)
{
    Doubles values = {NULL, 0u, 0u};
    field_values(misses, field, &values);
    const double result = ninety_fifth(values.values, values.count);
    free(values.values);
    return result;
}

/** @brief The frame of a coarse point: its center, heading, turn, and the rate of the turn over the last step. */
typedef struct
{
    Complex center;      /**< w at the point. */
    Complex heading;     /**< The step of w into the point. */
    double heading_size; /**< Its size. */
    int unit_turn;       /**< 1 where the heading is 0. The turn is then the number 1 of Python. */
    Complex turn;        /**< The heading over its size. */
    double rate;         /**< The turn of the heading over the last step. */
    double radius;       /**< |w| at the point. */
} Frame;

/**
 * @brief The frame of the sampler at a coarse point.
 *
 * @param[in]  coarse The coarse points [BORROWS].
 * @param[in]  start  The point.
 * @param[out] frame  The frame [BORROWS].
 */
static void frame_at(const MapPoint *coarse, size_t start, Frame *frame)
{
    frame->center = coarse[start].w;
    const Complex behind = (start > 0u) ? coarse[start - 1u].w : frame->center;
    const Complex before = (start > 1u) ? coarse[start - 2u].w : behind;
    frame->heading = c_sub(frame->center, behind);
    frame->heading_size = c_abs(frame->heading);
    frame->unit_turn = !(frame->heading_size > 0.0);
    frame->turn = frame->unit_turn ? cx(1.0, 0.0) : c_over(frame->heading, frame->heading_size);
    const Complex last = c_sub(behind, before);
    frame->rate = ((c_abs(last) > 0.0) && (frame->heading_size > 0.0)) ? c_angle(c_div(frame->heading, last)) : 0.0;
    frame->radius = c_abs(frame->center);
}

/** @brief A complex in the frame of the heading: over the turn, or over the number 1. */
static Complex in_frame(const Frame *frame, Complex value)
{
    return frame->unit_turn ? c_over(value, 1.0) : c_div(value, frame->turn);
}

/**
 * @brief The track of a steady arc through the last two fixes, at a place of the step.
 *
 * @param[in] frame      The frame [BORROWS].
 * @param[in] step_place The place u in the step.
 * @return               The track.
 * @note The heading is the chord into the point, and the tangent at the point turns half the rate past it. The arc runs
 *       at the length of the chord times (rate / 2) / sin(rate / 2) a step.
 */
static Complex track_of(const Frame *frame, double step_place)
{
    const double rate = frame->rate;
    if (fabs(rate) > 1e-12)
    {
        const double arc = (rate / 2.0) / sin(rate / 2.0);
        Complex track = c_mul(frame->heading, cx(cos(rate / 2.0), sin(rate / 2.0)));
        track = c_scale(track, arc);
        track = c_mul(track, cx(cos(rate * step_place) - 1.0, sin(rate * step_place)));
        return c_div(track, cx(0.0, rate));
    }
    return c_scale(frame->heading, step_place);
}

/**
 * @brief A miss seen from the center at its coarse point, with every field of the map.
 *
 * @param[in]  nu     The cell.
 * @param[in]  coarse The coarse points [BORROWS].
 * @param[in]  fine   The fine points [BORROWS].
 * @param[in]  miss   The miss [BORROWS].
 * @param[in]  lift   The fine steps a coarse step.
 * @param[out] placed The miss placed [BORROWS].
 * @note It gives the direction of the dip from the heading, and its distance in radii and in units. It gives the same
 *       from the steady arc that dead reckoning predicts, which takes out the curve of the spin of the ball.
 * @note It also gives the radius, the spin and the speed. Last, it gives the curve of fine points across the step in
 *       the frame of the center. The height of that curve is the change in Z toward zero.
 */
static void place(int64_t nu, const MapPoint *coarse, const MapPoint *fine, const Miss *miss, size_t lift,
                  Placed *placed)
{
    memset(placed, 0, sizeof(*placed));
    const size_t start = miss->start;
    const size_t finish = miss->finish;
    Frame frame;
    frame_at(coarse, start, &frame);
    const int64_t toward = (coarse[start].sign > 0) ? -1 : 1;
    const size_t dip = dip_of(fine, miss);
    // The lanes are below 2^53, exact as doubles.
    const double step_place = (double)((int64_t)dip - (int64_t)(start * lift)) / (double)lift;
    const Complex track = track_of(&frame, step_place);
    const Complex offset = c_sub(fine[dip].w, frame.center);
    const Complex from_center = in_frame(&frame, offset);
    const Complex residual = in_frame(&frame, c_sub(offset, track));
    const Complex ahead = c_sub(coarse[finish].w, frame.center);
    const double seen =
        ((frame.heading_size > 0.0) && (c_abs(ahead) > 0.0)) ? c_angle(c_div(ahead, frame.heading)) : 0.0;
    double turned = 0.0;
    int has_prior = 0;
    Complex prior = cx(0.0, 0.0);
    for (size_t lane = start * lift; lane < (finish * lift); lane += 1u)
    {
        const Complex tangent = c_sub(fine[lane + 1u].w, fine[lane].w);
        if (has_prior && (c_abs(prior) > 0.0) && (c_abs(tangent) > 0.0))
        {
            turned += c_angle(c_div(tangent, prior));
        }
        prior = tangent;
        has_prior = 1;
    }
    placed->curve_count = ((finish - start) * lift) + 1u;
    placed->curve = (double (*)[3])map_memory(malloc(placed->curve_count * sizeof(double[3])));
    for (size_t lane = start * lift; lane <= (finish * lift); lane += 1u)
    {
        const Complex shifted = in_frame(&frame, c_sub(fine[lane].w, frame.center));
        const Complex scaled = c_over(shifted, frame.radius);
        double *const row = placed->curve[lane - (start * lift)];
        row[0] = scaled.re;
        row[1] = scaled.im;
        // The direction toward zero is 1 or -1, exact as a double.
        row[2] = ((double)(-toward) * (fine[lane].z - coarse[start].z)) / (2.0 * frame.radius);
    }
    placed->nu = nu;
    placed->dip_s = fine[dip].s;
    placed->step_place = step_place;
    placed->angle = direction_degrees(from_center);
    placed->radii = c_abs(from_center) / frame.radius;
    placed->units = c_abs(from_center);
    placed->residual_angle = direction_degrees(residual);
    placed->residual_radii = c_abs(residual) / frame.radius;
    placed->residual_steps = (frame.heading_size > 0.0) ? (c_abs(residual) / frame.heading_size) : 0.0;
    placed->radius = frame.radius;
    placed->spin = frame.rate;
    placed->seen_turn = seen;
    placed->true_turn = turned;
    placed->speed = frame.heading_size;
    placed->dip_radius = c_abs(fine[dip].w);
}

/**
 * @brief A miss placed from the center at its coarse point of a lattice of fine places, as place_on of the Python.
 *
 * @param[in]  nu     The cell.
 * @param[in]  places The fine places of the coarse lattice [BORROWS].
 * @param[in]  fine   The fine points [BORROWS].
 * @param[in]  miss   The miss [BORROWS].
 * @param[out] placed The miss placed [BORROWS].
 * @note It gives the dip from the steady arc that the last two coarse points turn along. The dip splits into the parts
 *       along the track and across it, in steps of the last coarse chord.
 */
static void place_on(int64_t nu, const Places *places, const MapPoint *fine, const Miss *miss, Placed *placed)
{
    memset(placed, 0, sizeof(*placed));
    const size_t start = miss->start;
    MapPoint window[3];
    const size_t first = (start > 1u) ? (start - 2u) : 0u;
    for (size_t at = first; at <= start; at += 1u)
    {
        window[at - first] = fine[places->values[at]];
    }
    Frame frame;
    frame_at(window, start - first, &frame);
    // The places are below 2^53.
    const int64_t step_size = (start > 0u) ? ((int64_t)places->values[start] - (int64_t)places->values[start - 1u])
                                           : ((int64_t)places->values[start + 1u] - (int64_t)places->values[start]);
    const size_t dip = dip_of(fine, miss);
    const double step_place =
        (step_size != 0) ? ((double)((int64_t)dip - (int64_t)places->values[start]) / (double)step_size) : 0.0;
    const Complex track = track_of(&frame, step_place);
    const Complex offset = c_sub(fine[dip].w, frame.center);
    const Complex from_center = in_frame(&frame, offset);
    const Complex residual = in_frame(&frame, c_sub(offset, track));
    const double steps = (frame.heading_size > 0.0) ? (c_abs(residual) / frame.heading_size) : 0.0;
    const double angle = radians(direction_degrees(residual));
    placed->nu = nu;
    placed->residual_angle = direction_degrees(residual);
    placed->residual_steps = steps;
    placed->radius = frame.radius;
    placed->along = steps * cos(angle);
    placed->cross = steps * sin(angle);
    placed->d_steps = (frame.heading_size > 0.0) ? (c_abs(from_center) / frame.heading_size) : 0.0;
}

/**
 * @brief Joins a folder and a name, as os.path.join of Python joins them.
 *
 * @param[out] path   The path [BORROWS]. PATH_BYTES of them.
 * @param[in]  folder The folder [BORROWS].
 * @param[in]  name   The name [BORROWS].
 */
static void join_path(char path[PATH_BYTES], const char *folder, const char *name)
{
    const size_t length = strlen(folder);
    const int ends = (length > 0u) && ((folder[length - 1u] == '/') || (folder[length - 1u] == '\\'));
    snprintf(path, PATH_BYTES, "%s%s%s", folder, ends ? "" : PATH_JOIN, name);
}

/**
 * @brief One row a miss of the fields of the map, to a file of tab separated values.
 *
 * @param[in] misses The misses [BORROWS].
 * @param[in] path   The path [BORROWS].
 */
static void write_misses(const Placements *misses, const char *path)
{
    FILE *const handle = fopen(path, "w");
    if (handle == NULL)
    {
        zeta_stop("cannot write the table of misses");
    }
    fputs("nu\tdip_s\tu\tangle\tradii\tunits\tresidual_angle\tresidual_radii\tresidual_steps\tradius\tspin\tseen_turn\t"
          "true_turn\tspeed\tdip_radius\n",
          handle);
    for (size_t at = 0u; at < misses->count; at += 1u)
    {
        const Placed *const miss = &misses->values[at];
        const double fields[13] = {miss->step_place,     miss->angle,          miss->radii,          miss->units,
                                   miss->residual_angle, miss->residual_radii, miss->residual_steps, miss->radius,
                                   miss->spin,           miss->seen_turn,      miss->true_turn,      miss->speed,
                                   miss->dip_radius};
        fprintf(handle, "%lld\t%lld", (long long)miss->nu, (long long)miss->dip_s);
        for (int field = 0; field < 13; field += 1)
        {
            char text[64];
            py_repr(fields[field], 1, text, sizeof(text));
            fprintf(handle, "\t%s", text);
        }
        fputs("\n", handle);
    }
    fclose(handle);
}

/**
 * @brief The summary of the map over the misses and the radii of every coarse point.
 *
 * @param[in] misses The misses [BORROWS].
 * @param[in] radii  The radii [BORROWS].
 */
static void summary(const Placements *misses, const Doubles *radii)
{
    const size_t count = radii->count;
    double *const ordered = sorted_copy(radii->values, count);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_decimal(output_line, (unsigned long long)misses->count, 1u);
        mmgr_line_put(output_line, " misses over ");
        mmgr_line_decimal(output_line, (unsigned long long)count, 1u);
        mmgr_line_put(output_line, " coarse points");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    if (misses->count == 0u)
    {
        free(ordered);
        return;
    }
    const double tenth = ordered[count / 10u];
    const double middle = ordered[count / 2u];
    Doubles taken = {NULL, 0u, 0u};
    field_values(misses, FIELD_RADIUS, &taken);
    doubles_sort(&taken);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the ball's radius: median ");
        mmgr_line_fixed(output_line, (double)(middle), 4u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " over every coarse point, ");
        mmgr_line_fixed(output_line, (double)(taken.values[taken.count / 2u]), 4u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " at the misses' centers, ");
        mmgr_line_fixed(output_line, (double)(field_median(misses, FIELD_DIP_RADIUS)), 4u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " at their dips");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    size_t small_count = 0u;
    for (size_t at = 0u; at < taken.count; at += 1u)
    {
        small_count += (taken.values[at] <= tenth) ? 1u : 0u;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  misses whose center lies in the smallest tenth of radii: ");
        mmgr_line_fixed(output_line, (double)((100.0 * (double)small_count) / (double)taken.count), 1u, 0u,
                        EMBED_FALSE);
        mmgr_line_ch(output_line, '%');
        mmgr_line_put(output_line, ", against 10");
        mmgr_line_ch(output_line, '%');
        mmgr_line_put(output_line, " were the radius no field");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    static const char *const NAMES[2] = {"from the heading", "from the dead-reckoned track"};
    const PlacedField fields[2] = {FIELD_ANGLE, FIELD_RESIDUAL_ANGLE};
    for (int which = 0; which < 2; which += 1)
    {
        int64_t sectors[8] = {0};
        for (size_t at = 0u; at < misses->count; at += 1u)
        {
            // The sector of a direction from 0 to 360 is a whole number from 0 to 8.
            const int64_t sector = (int64_t)py_floordiv(placed_field(&misses->values[at], fields[which]), 45.0);
            sectors[((sector % 8) + 8) % 8] += 1;
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  direction ");
            mmgr_line_put(output_line, NAMES[which]);
            mmgr_line_put(output_line, ", by 45 degrees from 0: [");
            platform_line_out_ended();
        }
        for (int at = 0; at < 8; at += 1)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, (at > 0) ? ", " : "");
            mmgr_line_decimal_signed(output_line, (long long)sectors[at]);
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "]");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  distance from the dead-reckoned track in radii: median ");
        mmgr_line_fixed(output_line, (double)(field_median(misses, FIELD_RESIDUAL_RADII)), 3u, 0u, EMBED_FALSE);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  distance in radii: median ");
        mmgr_line_fixed(output_line, (double)(field_median(misses, FIELD_RADII)), 3u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, "; in units: median ");
        mmgr_line_fixed(output_line, (double)(field_median(misses, FIELD_UNITS)), 4u, 0u, EMBED_FALSE);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    free(taken.values);
    free(ordered);
}

/* ---------------------------------------------------------------- the figures */

/** @brief A growable text of the figure, one line after another. */
typedef struct
{
    char *text;      /**< The text [OWNS]. */
    size_t length;   /**< Its length. */
    size_t capacity; /**< The size of the buffer. */
} Figure;

/**
 * @brief Adds a line to the figure.
 *
 * @param[in,out] figure The figure [BORROWS].
 * @param[in]     format The format of the line [BORROWS].
 */
static void line_of(Figure *figure, const char *format, ...) __attribute__((format(printf, 2, 3)));

static void line_of(Figure *figure, const char *format, ...)
{
    va_list list;
    va_start(list, format);
    va_list copy;
    va_copy(copy, list);
    const int written = vsnprintf(NULL, 0u, format, copy);
    va_end(copy);
    // A line holds at most a few megabytes.
    const size_t length = (size_t)((written > 0) ? written : 0);
    if ((figure->length + length + 2u) > figure->capacity)
    {
        figure->capacity = (figure->capacity + length + 2u) * 2u;
        figure->text = (char *)map_memory(realloc(figure->text, figure->capacity));
    }
    if (figure->length > 0u)
    {
        figure->text[figure->length] = '\n';
        figure->length += 1u;
    }
    (void)vsnprintf(figure->text + figure->length, length + 1u, format, list);
    va_end(list);
    figure->length += length;
    figure->text[figure->length] = '\0';
}

/**
 * @brief Writes the figure to a path, its lines joined, with a line feed at the end.
 *
 * @param[in] figure The figure [BORROWS].
 * @param[in] path   The path [BORROWS].
 */
static void figure_write(const Figure *figure, const char *path)
{
    FILE *const handle = fopen(path, "w");
    if (handle == NULL)
    {
        zeta_stop("cannot write the figure");
    }
    fputs(figure->text, handle);
    fputs("\n", handle);
    fclose(handle);
}

/** @brief A growable text of the points of a polyline, joined by spaces. */
typedef struct
{
    char *text;      /**< The text [OWNS]. */
    size_t length;   /**< Its length. */
    size_t capacity; /**< The size of the buffer. */
} Points;

/**
 * @brief Adds one point, or a pair of points, to the text of a polyline.
 *
 * @param[in,out] points The text [BORROWS].
 * @param[in]     format The format of the point [BORROWS].
 */
static void point_of(Points *points, const char *format, ...) __attribute__((format(printf, 2, 3)));

static void point_of(Points *points, const char *format, ...)
{
    char buffer[128];
    va_list list;
    va_start(list, format);
    const int written = vsnprintf(buffer, sizeof(buffer), format, list);
    va_end(list);
    // A point holds a few dozen characters.
    const size_t length = (size_t)((written > 0) ? written : 0);
    if ((points->length + length + 2u) > points->capacity)
    {
        points->capacity = (points->capacity + length + 2u) * 2u;
        points->text = (char *)map_memory(realloc(points->text, points->capacity));
    }
    if (points->length > 0u)
    {
        points->text[points->length] = ' ';
        points->length += 1u;
    }
    memcpy(points->text + points->length, buffer, length);
    points->length += length;
    points->text[points->length] = '\0';
}

/**
 * @brief A color from dark blue at 0 to yellow at the top.
 *
 * @param[out] text     The buffer [BORROWS].
 * @param[in]  capacity Bytes of the buffer.
 * @param[in]  value    The value.
 * @param[in]  top      The top.
 */
static void shade(char *text, size_t capacity, double value, double top)
{
    double share = 0.0;
    if (top > 0.0)
    {
        share = value / top;
        share = (share < 1.0) ? share : 1.0;
        share = (share > 0.0) ? share : 0.0;
    }
    // The parts of a color are from 0 to 255.
    snprintf(text, capacity, "rgb(%d,%d,%d)", (int)(40.0 + (213.0 * share)), (int)(20.0 + (211.0 * share)),
             (int)(110.0 - (75.0 * share)));
}

/**
 * @brief The polar panel: heading at the top, bearings clockwise, distance out to the 95th percentile of the misses.
 *
 * @param[in,out] out        The figure [BORROWS].
 * @param[in]     panel_left The left of the panel.
 * @param[in]     panel_top  The top of the panel.
 * @param[in]     misses     The misses [BORROWS].
 * @param[in]     angle      The field of the direction.
 * @param[in]     distance   The field of the distance.
 * @param[in]     title      The title [BORROWS].
 * @param[in]     top_radius The radius at the top of the color scale.
 */
static void polar(Figure *out, int panel_left, int panel_top, const Placements *misses, PlacedField angle,
                  PlacedField distance, const char *title, double top_radius)
{
    const double middle_x = panel_left + (PANEL / 2.0);
    const double middle_y = (panel_top + (PANEL / 2.0)) + 10.0;
    const double rim = (PANEL / 2.0) - MARGIN;
    double reach = field_ninety_fifth(misses, distance);
    reach = (reach != 0.0) ? reach : 1.0;
    line_of(out, "<text x=\"%d\" y=\"%d\" font-size=\"13\">%s</text>", panel_left + 10, panel_top + 18, title);
    for (int ring = 1; ring <= 4; ring += 1)
    {
        line_of(out, "<circle cx=\"%.1f\" cy=\"%.1f\" r=\"%.1f\" fill=\"none\" stroke=\"#ccc\"/>", middle_x, middle_y,
                (rim * ring) / 4.0);
        line_of(out, "<text x=\"%.1f\" y=\"%.1f\" font-size=\"10\" fill=\"#666\">%.3g</text>", middle_x + 3.0,
                (middle_y - ((rim * ring) / 4.0)) - 2.0, (reach * ring) / 4.0);
    }
    for (int spoke = 0; spoke < 360; spoke += 45)
    {
        const double turn = radians((double)spoke);
        line_of(out, "<line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" stroke=\"#ddd\"/>", middle_x, middle_y,
                middle_x + (rim * sin(turn)), middle_y - (rim * cos(turn)));
        line_of(out, "<text x=\"%.1f\" y=\"%.1f\" font-size=\"10\" text-anchor=\"middle\">%d</text>",
                middle_x + ((rim + 14.0) * sin(turn)), (middle_y - ((rim + 14.0) * cos(turn))) + 4.0, spoke);
    }
    for (size_t at = 0u; at < misses->count; at += 1u)
    {
        const Placed *const miss = &misses->values[at];
        const double turn = radians(placed_field(miss, angle));
        const double ratio = placed_field(miss, distance) / reach;
        const double reach_out = ((ratio < 1.0) ? ratio : 1.0) * rim;
        char color[64];
        shade(color, sizeof(color), miss->radius, top_radius);
        line_of(out, "<circle cx=\"%.1f\" cy=\"%.1f\" r=\"2\" fill=\"%s\"/>", middle_x + (reach_out * sin(turn)),
                middle_y - (reach_out * cos(turn)), color);
    }
}

/** @brief The box of a panel: left, bottom, right and top. */
typedef struct
{
    int left;   /**< The left. */
    int bottom; /**< The bottom. */
    int right;  /**< The right. */
    int top;    /**< The top. */
} Box;

/**
 * @brief The axes of a panel, with the title and the labels.
 *
 * @param[in,out] out        The figure [BORROWS].
 * @param[in]     panel_left The left of the panel.
 * @param[in]     panel_top  The top of the panel.
 * @param[in]     title      The title [BORROWS].
 * @param[in]     xlabel     The label of x [BORROWS].
 * @param[in]     ylabel     The label of y [BORROWS].
 * @return                   The box.
 */
static Box axes(Figure *out, int panel_left, int panel_top, const char *title, const char *xlabel, const char *ylabel)
{
    const Box box = {panel_left + MARGIN + 10, (panel_top + PANEL) - MARGIN, (panel_left + PANEL) - 10,
                     panel_top + MARGIN};
    line_of(out, "<text x=\"%d\" y=\"%d\" font-size=\"13\">%s</text>", panel_left + 10, panel_top + 18, title);
    line_of(out, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"#000\"/>", box.left, box.bottom, box.right,
            box.bottom);
    line_of(out, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"#000\"/>", box.left, box.bottom, box.left,
            box.top);
    // The %d of Python cuts a float toward zero, and these halves are positive.
    line_of(out, "<text x=\"%d\" y=\"%d\" font-size=\"11\" text-anchor=\"middle\">%s</text>",
            (int)((box.left + box.right) / 2.0), box.bottom + 28, xlabel);
    line_of(out,
            "<text x=\"%d\" y=\"%d\" font-size=\"11\" transform=\"rotate(-90 %d %d)\" text-anchor=\"middle\">%s</text>",
            panel_left + 14, (int)((box.top + box.bottom) / 2.0), panel_left + 14, (int)((box.top + box.bottom) / 2.0),
            ylabel);
    return box;
}

/** @brief The open tag of a figure, with its white ground. */
static void figure_begin(Figure *out, int width, int height)
{
    line_of(out, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"%d\" font-family=\"sans-serif\">",
            width, height);
    line_of(out, "%s", "<rect width=\"100%\" height=\"100%\" fill=\"white\"/>");
}

/**
 * @brief The map as an SVG: three polar panels, the turn against the distance from the track, the radii, and the
 *        curves.
 *
 * @param[in]  misses The misses [BORROWS].
 * @param[in]  radii  The radii [BORROWS].
 * @param[in]  folder The folder [BORROWS].
 * @param[out] path   The path of the map [BORROWS]. PATH_BYTES of them.
 */
static void draw(const Placements *misses, const Doubles *radii, const char *folder, char path[PATH_BYTES])
{
    Figure out = {NULL, 0u, 0u};
    figure_begin(&out, 3 * PANEL, 2 * PANEL);
    const double top_radius = field_ninety_fifth(misses, FIELD_RADIUS);
    polar(&out, 0, 0, misses, FIELD_ANGLE, FIELD_RADII, "from the center, heading at 0, in radii", top_radius);
    polar(&out, PANEL, 0, misses, FIELD_ANGLE, FIELD_UNITS, "the same, in units", top_radius);
    polar(&out, 2 * PANEL, 0, misses, FIELD_RESIDUAL_ANGLE, FIELD_RESIDUAL_RADII,
          "from the steady arc, the spin taken out, in radii", top_radius);
    Box box = axes(&out, 0, PANEL, "the turn before against the distance from the arc",
                   "turn over the last step, radians", "distance from the arc, radii");
    double spin_low = (misses->count > 0u) ? misses->values[0].spin : 0.0;
    double spin_high = spin_low;
    for (size_t at = 1u; at < misses->count; at += 1u)
    {
        spin_low = (misses->values[at].spin < spin_low) ? misses->values[at].spin : spin_low;
        spin_high = (misses->values[at].spin > spin_high) ? misses->values[at].spin : spin_high;
    }
    double reach = field_ninety_fifth(misses, FIELD_RESIDUAL_RADII);
    reach = (reach != 0.0) ? reach : 1.0;
    for (size_t at = 0u; at < misses->count; at += 1u)
    {
        const Placed *const miss = &misses->values[at];
        const double span = ((spin_high - spin_low) != 0.0) ? (spin_high - spin_low) : 1.0;
        const double across = box.left + (((double)(box.right - box.left) * (miss->spin - spin_low)) / span);
        const double ratio = miss->residual_radii / reach;
        const double up = box.bottom - ((double)(box.bottom - box.top) * ((ratio < 1.0) ? ratio : 1.0));
        char color[64];
        shade(color, sizeof(color), miss->radius, top_radius);
        line_of(&out, "<circle cx=\"%.1f\" cy=\"%.1f\" r=\"2\" fill=\"%s\"/>", across, up, color);
    }
    box = axes(&out, PANEL, PANEL, "the ball's radius", "|w|, out to the 99th percentile", "share in each of 40 bins");
    const double limit = sorted_at(radii->values, radii->count, (99u * radii->count) / 100u);
    double shares[3][40];
    static const char *const COLORS[3] = {"#4a7fb5", "#e08a2c", "#3a9a3a"};
    double tallest = 0.0;
    int any_share = 0;
    for (int series = 0; series < 3; series += 1)
    {
        Doubles values = {NULL, 0u, 0u};
        if (series == 0)
        {
            values.values = sorted_copy(radii->values, radii->count);
            values.count = radii->count;
        }
        else
        {
            field_values(misses, (series == 1) ? FIELD_RADIUS : FIELD_DIP_RADIUS, &values);
        }
        int64_t bins[40] = {0};
        for (size_t at = 0u; at < values.count; at += 1u)
        {
            if (values.values[at] < limit)
            {
                // The bin of a value below the limit is from 0 to 39.
                bins[(int)((40.0 * values.values[at]) / limit)] += 1;
            }
        }
        for (int bin = 0; bin < 40; bin += 1)
        {
            shares[series][bin] = (double)bins[bin] / (double)values.count;
            tallest = (!any_share || (shares[series][bin] > tallest)) ? shares[series][bin] : tallest;
            any_share = 1;
        }
        free(values.values);
    }
    tallest = (tallest != 0.0) ? tallest : 1.0;
    for (int series = 0; series < 3; series += 1)
    {
        Points points = {NULL, 0u, 0u};
        for (int bin = 0; bin < 40; bin += 1)
        {
            const double from_x = box.left + (((double)(box.right - box.left) * bin) / 40.0);
            const double to_x = box.left + (((double)(box.right - box.left) * (bin + 1)) / 40.0);
            const double up = box.bottom - (((double)(box.bottom - box.top) * shares[series][bin]) / tallest);
            point_of(&points, "%.1f,%.1f %.1f,%.1f", from_x, up, to_x, up);
        }
        line_of(&out, "<polyline points=\"%s\" fill=\"none\" stroke=\"%s\" stroke-width=\"1.6\"/>", points.text,
                COLORS[series]);
        free(points.text);
    }
    static const char *const LEGEND[3] = {"every coarse point", "misses' centers", "misses' dips"};
    for (int at = 0; at < 3; at += 1)
    {
        line_of(&out, "<text x=\"%d\" y=\"%d\" font-size=\"11\" fill=\"%s\">%s</text>", box.right - 130,
                box.top + 14 + (14 * at), COLORS[at], LEGEND[at]);
    }
    const int panel_left = 2 * PANEL;
    const int panel_top = PANEL;
    line_of(&out, "<text x=\"%d\" y=\"%d\" font-size=\"13\">each miss's curve over and under the center's plane</text>",
            panel_left + 10, panel_top + 18);
    const size_t displayed = (misses->count < 200u) ? misses->count : 200u;
    double span = 0.0;
    int any = 0;
    double *const heights = (double *)map_memory(malloc((displayed + 1u) * sizeof(double)));
    for (size_t at = 0u; at < displayed; at += 1u)
    {
        const Placed *const miss = &misses->values[at];
        double height = 0.0;
        for (size_t row = 0u; row < miss->curve_count; row += 1u)
        {
            double size = fabs(miss->curve[row][0]);
            size = (fabs(miss->curve[row][1]) > size) ? fabs(miss->curve[row][1]) : size;
            size = (fabs(miss->curve[row][2]) > size) ? fabs(miss->curve[row][2]) : size;
            span = (!any || (size > span)) ? size : span;
            any = 1;
            height = ((row == 0u) || (fabs(miss->curve[row][2]) > height)) ? fabs(miss->curve[row][2]) : height;
        }
        heights[at] = height;
    }
    if (!any)
    {
        span = 1.0;
    }
    const double tripled = ninety_fifth(heights, displayed) * 3.0;
    const double bound = (tripled != 0.0) ? tripled : span;
    span = (span < bound) ? span : bound;
    free(heights);
    const double middle_x = panel_left + (PANEL / 2.0);
    const double middle_y = (panel_top + (PANEL / 2.0)) + 20.0;
    const double scale = ((PANEL / 2.0) - MARGIN) / ((span != 0.0) ? span : 1.0);
    const double slant[2] = {cos(radians(30.0)) * 0.5, sin(radians(30.0)) * 0.5};
    const double corners[4][2] = {{-span, -span}, {span, -span}, {span, span}, {-span, span}};
    Points plane = {NULL, 0u, 0u};
    for (int corner = 0; corner < 4; corner += 1)
    {
        point_of(&plane, "%.1f,%.1f", middle_x + (scale * (corners[corner][0] + (slant[0] * corners[corner][1]))),
                 middle_y - (scale * (slant[1] * corners[corner][1])));
    }
    line_of(&out, "<polygon points=\"%s\" fill=\"#eef\" stroke=\"#99a\"/>", plane.text);
    free(plane.text);
    for (size_t at = 0u; at < displayed; at += 1u)
    {
        const Placed *const miss = &misses->values[at];
        Points points = {NULL, 0u, 0u};
        for (size_t row = 0u; row < miss->curve_count; row += 1u)
        {
            point_of(&points, "%.1f,%.1f",
                     middle_x + (scale * (miss->curve[row][0] + (slant[0] * miss->curve[row][1]))),
                     middle_y - (scale * (miss->curve[row][2] + (slant[1] * miss->curve[row][1]))));
        }
        // The hue is from 0 to 359.
        line_of(&out, "<polyline points=\"%s\" fill=\"none\" stroke=\"hsl(%d,60%%,45%%)\" stroke-width=\"0.7\"/>",
                points.text, (int)((at * 47u) % 360u));
        free(points.text);
    }
    line_of(&out,
            "<text x=\"%d\" y=\"%d\" font-size=\"11\">along the heading to the right, across it into the page, up away "
            "from zero; radii</text>",
            panel_left + 10, (panel_top + PANEL) - 12);
    line_of(&out, "%s", "</svg>");
    join_path(path, folder, "miss_map.svg");
    figure_write(&out, path);
    free(out.text);
}

/** @brief Makes one folder, where it is absent. */
static void make_one_folder(const char *path)
{
#ifdef _WIN32
    (void)_mkdir(path);
#else
    (void)mkdir(path, 0777);
#endif
}

/**
 * @brief Makes a folder and the folders above it, where they are absent.
 *
 * @param[in] path The folder [BORROWS].
 */
static void make_folder(const char *path)
{
    char partial[PATH_BYTES];
    snprintf(partial, sizeof(partial), "%s", path);
    for (size_t at = 1u; partial[at] != '\0'; at += 1u)
    {
        if ((partial[at] == '/') || (partial[at] == '\\'))
        {
            const char kept = partial[at];
            partial[at] = '\0';
            make_one_folder(partial);
            partial[at] = kept;
        }
    }
    make_one_folder(partial);
}

/**
 * @brief The folder of a run: the one that the arguments name, else one under build/miss_map.
 *
 * @param[out] folder The folder [BORROWS]. PATH_BYTES of them.
 * @param[in]  given  The named folder, or NULL [BORROWS].
 * @param[in]  name   The name of the folder under build/miss_map [BORROWS].
 */
static void folder_of(char folder[PATH_BYTES], const char *given, const char *name)
{
    if (given != NULL)
    {
        snprintf(folder, PATH_BYTES, "%s", given);
    }
    else
    {
        snprintf(folder, PATH_BYTES, "build/miss_map/%s", name);
    }
    make_folder(folder);
}

/**
 * @brief Writes doubles, one a line at six places, to a path.
 *
 * @param[in] values The doubles [BORROWS].
 * @param[in] path   The path [BORROWS].
 */
static void write_column(const Doubles *values, const char *path)
{
    FILE *const handle = fopen(path, "w");
    if (handle == NULL)
    {
        zeta_stop("cannot write a column");
    }
    for (size_t at = 0u; at < values->count; at += 1u)
    {
        fprintf(handle, "%s%.6f", (at > 0u) ? "\n" : "", values->values[at]);
    }
    fputs("\n", handle);
    fclose(handle);
}

/** @brief The turn of the heading over the last step at each coarse point past the second, into a list. */
static void turns_of(const MapPoint *coarse, size_t count, int size, Doubles *turns)
{
    for (size_t at = 2u; at < count; at += 1u)
    {
        const Complex heading = c_sub(coarse[at].w, coarse[at - 1u].w);
        const Complex last = c_sub(coarse[at - 1u].w, coarse[at - 2u].w);
        if ((c_abs(heading) > 0.0) && (c_abs(last) > 0.0))
        {
            const double turn = c_angle(c_div(heading, last));
            doubles_push(turns, size ? fabs(turn) : turn);
        }
    }
}

/**
 * @brief The map of the misses of cells first to last, coarse against fine.
 *
 * @param[in] binary      The device program [BORROWS].
 * @param[in] first       The first cell.
 * @param[in] last        The last cell.
 * @param[in] coarse_bits The bits of the coarse lattice.
 * @param[in] fine_p      The bits of the fine lattice.
 * @param[in] given       The folder, or NULL [BORROWS].
 * @return                0 where every host check passes, else 1.
 */
static int main_map(const char *binary, int64_t first, int64_t last, int64_t coarse_bits, int64_t fine_p,
                    const char *given)
{
    char folder[PATH_BYTES];
    folder_of(folder, given, "map");
    TuringConstants constants;
    turing_constants(&constants);
    // The lift is 2 to the difference of two small counts of bits.
    const size_t lift = (size_t)1u << (unsigned)(fine_p - coarse_bits);
    Placements misses = {NULL, 0u, 0u};
    Doubles radii = {NULL, 0u, 0u};
    Doubles spins = {NULL, 0u, 0u};
    int64_t failed = 0;
    for (int64_t nu = first; nu <= last; nu += 1)
    {
        Listing coarse;
        Listing fine;
        failed += run_listed(binary, &constants, nu, coarse_bits, 1, &coarse);
        failed += run_listed(binary, &constants, nu, fine_p, 1, &fine);
        for (size_t at = 0u; at < coarse.count; at += 1u)
        {
            doubles_push(&radii, c_abs(coarse.points[at].w));
        }
        turns_of(coarse.points, coarse.count, 0, &spins);
        Misses found;
        misses_of(coarse.points, coarse.count, fine.points, fine.count, lift, &found);
        for (size_t at = 0u; at < found.count; at += 1u)
        {
            Placed placed;
            place(nu, coarse.points, fine.points, &found.values[at], lift, &placed);
            placements_push(&misses, &placed);
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  cell ");
            mmgr_line_decimal_signed(output_line, (long long)nu);
            mmgr_line_put(output_line, ": 2^");
            mmgr_line_decimal_signed(output_line, (long long)coarse_bits);
            mmgr_line_put(output_line, " against 2^");
            mmgr_line_decimal_signed(output_line, (long long)fine_p);
            mmgr_line_put(output_line, " points, ");
            mmgr_line_decimal(output_line, (unsigned long long)found.count, 1u);
            mmgr_line_put(output_line, " misses");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        free(found.values);
        free(coarse.points);
        free(fine.points);
    }
    summary(&misses, &radii);
    char path[PATH_BYTES];
    if ((misses.count > 0u) && (spins.count > 0u))
    {
        double *const ordered = sorted_copy(spins.values, spins.count);
        const double top = ordered[(9u * spins.count) / 10u];
        Doubles miss_spins = {NULL, 0u, 0u};
        field_values(&misses, FIELD_SPIN, &miss_spins);
        size_t in_top = 0u;
        for (size_t at = 0u; at < misses.count; at += 1u)
        {
            in_top += (misses.values[at].spin >= top) ? 1u : 0u;
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  the turn over a step: median ");
            mmgr_line_fixed(output_line, (double)(ordered[spins.count / 2u]), 3u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, " radians at every coarse point, ");
            mmgr_line_fixed(output_line, (double)(sorted_at(miss_spins.values, miss_spins.count, misses.count / 2u)),
                            3u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, " at the misses' centers; misses in the largest tenth of turns: ");
            mmgr_line_fixed(output_line, (double)((100.0 * (double)in_top) / (double)misses.count), 1u, 0u,
                            EMBED_FALSE);
            mmgr_line_ch(output_line, '%');
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        free(miss_spins.values);
        const double sixth = PI_DOUBLE / 3.0;
        size_t steps_past = 0u;
        for (size_t at = 0u; at < spins.count; at += 1u)
        {
            steps_past += (fabs(spins.values[at]) > sixth) ? 1u : 0u;
        }
        size_t seen_past = 0u;
        size_t true_past = 0u;
        for (size_t at = 0u; at < misses.count; at += 1u)
        {
            seen_past += (fabs(misses.values[at].seen_turn) > sixth) ? 1u : 0u;
            true_past += (fabs(misses.values[at].true_turn) > sixth) ? 1u : 0u;
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  turns past a sixth of a turn: ");
            mmgr_line_fixed(output_line, (double)((100.0 * (double)steps_past) / (double)spins.count), 1u, 0u,
                            EMBED_FALSE);
            mmgr_line_ch(output_line, '%');
            mmgr_line_put(output_line, " of every coarse step's; at the misses, ");
            mmgr_line_fixed(output_line, (double)((100.0 * (double)seen_past) / (double)misses.count), 1u, 0u,
                            EMBED_FALSE);
            mmgr_line_ch(output_line, '%');
            mmgr_line_put(output_line, " of the steps' as the coarse chords see them and ");
            mmgr_line_fixed(output_line, (double)((100.0 * (double)true_past) / (double)misses.count), 1u, 0u,
                            EMBED_FALSE);
            mmgr_line_ch(output_line, '%');
            mmgr_line_put(output_line, " of the tip's own");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        double *const own = (double *)map_memory(malloc((misses.count + 1u) * sizeof(double)));
        for (size_t at = 0u; at < misses.count; at += 1u)
        {
            own[at] = fabs(misses.values[at].true_turn);
        }
        qsort(own, misses.count, sizeof(double), double_order);
        const size_t tenth = misses.count / 10u;
        const size_t stride = (tenth > 1u) ? tenth : 1u;
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  the tip's own turn across a miss's step: deciles [");
            platform_line_out_ended();
        }
        for (size_t at = tenth; at < misses.count; at += stride)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, (at > tenth) ? ", " : "");
            mmgr_line_put(output_line, "'");
            mmgr_line_fixed(output_line, (double)(degrees(own[at])), 0u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, "'");
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "] degrees");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        free(own);
        free(ordered);
        join_path(path, folder, "spins.tsv");
        write_column(&spins, path);
    }
    join_path(path, folder, "misses.tsv");
    write_misses(&misses, path);
    char radii_path[PATH_BYTES];
    join_path(radii_path, folder, "radii.tsv");
    write_column(&radii, radii_path);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the misses: ");
        mmgr_line_put(output_line, path);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    char map_path[PATH_BYTES];
    draw(&misses, &radii, folder, map_path);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the map: ");
        mmgr_line_put(output_line, map_path);
        mmgr_line_put(output_line, "; ");
        mmgr_line_decimal_signed(output_line, (long long)failed);
        mmgr_line_put(output_line, " host checks failed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    placements_free(&misses);
    free(radii.values);
    free(spins.values);
    return (failed == 0) ? 0 : 1;
}

/* ---------------------------------------------------------------- the heights an e-fold apart */

/** @brief The fields of one height. */
typedef struct
{
    Placements misses; /**< The misses. */
    Doubles radii;     /**< The radius of every coarse point. */
    Doubles turns;     /**< The size of the turn at every coarse point. */
    double rate;       /**< The mean rate kept. */
    int64_t zeros;     /**< The zeros of the fine lattices. */
    int64_t failed;    /**< The failed host checks. */
    int64_t nu;        /**< The first cell. */
    double time;       /**< t at the first cell. */
    double harmonic;   /**< The mean of the sum of 1/n to nu over the cells. */
} Fold;

/**
 * @brief Cells nu to nu + count - 1, each on a fine lattice, its coarse lattice every k-th fine point.
 *
 * @param[in]  binary    The device program [BORROWS].
 * @param[in]  constants The constants [BORROWS].
 * @param[in]  nu        The first cell.
 * @param[in]  count     The cells.
 * @param[in]  rate      The points a zero.
 * @param[out] fold      The fields [BORROWS].
 * @note The code chooses k for rate points a zero.
 */
static void fold_cells(const char *binary, const TuringConstants *constants, int64_t nu, int64_t count, double rate,
                       Fold *fold)
{
    memset(fold, 0, sizeof(*fold));
    Sum rates = {0.0, 0.0};
    for (int64_t at = nu; at < (nu + count); at += 1)
    {
        const double rise = turing_rises(at);
        const int64_t fine_p = fine_bits(rate, rise);
        const int64_t step = coarse_step(fine_p, rate, rise);
        Listing fine;
        fold->failed += run_listed(binary, constants, at, fine_p, 1, &fine);
        // The step is a small positive count.
        const size_t lift = (size_t)step;
        const size_t coarse_count = (fine.count + lift - 1u) / lift;
        MapPoint *const coarse = (MapPoint *)map_memory(malloc((coarse_count + 1u) * sizeof(MapPoint)));
        for (size_t point = 0u; point < coarse_count; point += 1u)
        {
            coarse[point] = fine.points[point * lift];
        }
        // The count of zeros fits a machine integer.
        fold->zeros += (int64_t)zero_count(fine.points, fine.count);
        // The bits of a lattice are a small count.
        sum_add(&rates, ldexp(1.0, (int)fine_p) / ((double)step * rise));
        for (size_t point = 0u; point < coarse_count; point += 1u)
        {
            doubles_push(&fold->radii, c_abs(coarse[point].w));
        }
        turns_of(coarse, coarse_count, 1, &fold->turns);
        Misses found;
        misses_of(coarse, coarse_count, fine.points, fine.count, lift, &found);
        for (size_t miss = 0u; miss < found.count; miss += 1u)
        {
            Placed placed;
            place(at, coarse, fine.points, &found.values[miss], lift, &placed);
            placements_push(&fold->misses, &placed);
        }
        free(found.values);
        free(coarse);
        free(fine.points);
    }
    Sum harmonic = {0.0, 0.0};
    for (int64_t at = nu; at < (nu + count); at += 1)
    {
        Sum inner = {0.0, 0.0};
        for (int64_t term = 1; term <= at; term += 1)
        {
            sum_add(&inner, 1.0 / (double)term);
        }
        sum_add(&harmonic, sum_value(&inner));
    }
    fold->harmonic = sum_value(&harmonic) / (double)count;
    fold->rate = sum_value(&rates) / (double)count;
    fold->nu = nu;
    fold->time = (TWO_PI * (double)nu) * (double)nu;
}

/** @brief The rows of one height, each a label and its text. */
#define FOLD_ROWS 24

/** @brief The labels of the rows of a height. */
static const char *const FOLD_LABELS[FOLD_ROWS] = {"t at the first cell",
                                                   "points a zero, coarse",
                                                   "misses a thousand zeros",
                                                   "median radius, every point",
                                                   "inertia, the mean of |w|^2",
                                                   "its law, the sum of 1/n to nu",
                                                   "misses' radius over the median",
                                                   "misses in the smallest tenth of radii",
                                                   "median turn a step, every point",
                                                   "misses' turn over the median",
                                                   "misses in the largest tenth of turns",
                                                   "bearing from the heading, median",
                                                   "distance from the arc, steps, median",
                                                   "distance from the arc, steps, tenth",
                                                   "distance from the arc, steps, ninetieth",
                                                   "pull from the arc: length, bearing",
                                                   "bearing's spread from the arc, 1 - pull",
                                                   "the pickle's width, cross-track RMS steps",
                                                   "the pickle's aspect, cross over along",
                                                   "under us, within a step of the arc",
                                                   "past a step: radius, turn over the median",
                                                   "past a step: dip radius over the median",
                                                   "past a step over the core's ninetieth",
                                                   "misses"};

/**
 * @brief The fields of one height in its own units.
 *
 * @param[in]  fold The height [BORROWS].
 * @param[out] rows The text of each row [BORROWS].
 * @note The scatter about the steady arc is in the own frame of the walker. A miss within one step of the arc is under
 *       us. The run reads a miss past it as a curl of the field at a scale beyond the reach of the walk.
 * @note The pickle splits the distance of each miss from the arc into the part along the track and the part across it.
 *       The width is the RMS across it.
 */
static void fold_row(const Fold *fold, char rows[FOLD_ROWS][64])
{
    const Placements *const misses = &fold->misses;
    const size_t count = misses->count;
    const double denominator = (double)((count > 0u) ? count : 1u);
    const double r_mid = median_of(fold->radii.values, fold->radii.count);
    const double t_mid = median_of(fold->turns.values, fold->turns.count);
    const double r_tenth = sorted_at(fold->radii.values, fold->radii.count, fold->radii.count / 10u);
    const double t_tenth = sorted_at(fold->turns.values, fold->turns.count, (9u * fold->turns.count) / 10u);
    Sum pull_re = {0.0, 0.0};
    Sum pull_im = {0.0, 0.0};
    Doubles scatter = {NULL, 0u, 0u};
    Doubles near_steps = {NULL, 0u, 0u};
    Doubles far_steps = {NULL, 0u, 0u};
    Doubles far_radius = {NULL, 0u, 0u};
    Doubles far_spin = {NULL, 0u, 0u};
    Doubles far_dip = {NULL, 0u, 0u};
    Sum cross_square = {0.0, 0.0};
    Sum along_square = {0.0, 0.0};
    size_t small_radius = 0u;
    size_t large_turn = 0u;
    for (size_t at = 0u; at < count; at += 1u)
    {
        const Placed *const miss = &misses->values[at];
        const double angle = radians(miss->residual_angle);
        sum_add(&pull_re, cos(angle));
        sum_add(&pull_im, sin(angle));
        doubles_push(&scatter, miss->residual_steps);
        if (miss->residual_steps <= 1.0)
        {
            doubles_push(&near_steps, miss->residual_steps);
        }
        else
        {
            doubles_push(&far_steps, miss->residual_steps);
            doubles_push(&far_radius, miss->radius);
            doubles_push(&far_spin, fabs(miss->spin));
            doubles_push(&far_dip, miss->dip_radius);
        }
        const double along = miss->residual_steps * cos(angle);
        const double cross = miss->residual_steps * sin(angle);
        sum_add(&along_square, along * along);
        sum_add(&cross_square, cross * cross);
        small_radius += (miss->radius <= r_tenth) ? 1u : 0u;
        large_turn += (fabs(miss->spin) >= t_tenth) ? 1u : 0u;
    }
    const Complex pull = c_over(cx(sum_value(&pull_re), sum_value(&pull_im)), denominator);
    doubles_sort(&scatter);
    const double cross_rms = sqrt(sum_value(&cross_square) / denominator);
    const double along_rms = sqrt(sum_value(&along_square) / denominator);
    Sum inertia = {0.0, 0.0};
    for (size_t at = 0u; at < fold->radii.count; at += 1u)
    {
        sum_add(&inertia, fold->radii.values[at] * fold->radii.values[at]);
    }
    const double radii_count = (double)((fold->radii.count > 0u) ? fold->radii.count : 1u);
    // The zeros are a count, exact as a double.
    const double zeros = (double)((fold->zeros > 0) ? fold->zeros : 1);
    snprintf(rows[0], 64u, "%.4g", fold->time);
    snprintf(rows[1], 64u, "%.3f", fold->rate);
    snprintf(rows[2], 64u, "%.3f", ((1000.0 * 2.0) * (double)count) / zeros);
    snprintf(rows[3], 64u, "%.4f", r_mid);
    snprintf(rows[4], 64u, "%.4f", sum_value(&inertia) / radii_count);
    snprintf(rows[5], 64u, "%.4f", fold->harmonic);
    snprintf(rows[6], 64u, "%.3f", field_median(misses, FIELD_RADIUS) / r_mid);
    snprintf(rows[7], 64u, "%.1f%%", (100.0 * (double)small_radius) / denominator);
    snprintf(rows[8], 64u, "%.3f", t_mid);
    snprintf(rows[9], 64u, "%.3f", field_median(misses, FIELD_SPIN_SIZE) / t_mid);
    snprintf(rows[10], 64u, "%.1f%%", (100.0 * (double)large_turn) / denominator);
    snprintf(rows[11], 64u, "%.1f", field_median(misses, FIELD_ANGLE));
    snprintf(rows[12], 64u, "%.3f", field_median(misses, FIELD_RESIDUAL_STEPS));
    if (scatter.count > 0u)
    {
        snprintf(rows[13], 64u, "%.3f", scatter.values[scatter.count / 10u]);
        snprintf(rows[14], 64u, "%.3f", scatter.values[(9u * scatter.count) / 10u]);
    }
    else
    {
        snprintf(rows[13], 64u, "-");
        snprintf(rows[14], 64u, "-");
    }
    snprintf(rows[15], 64u, "%.3f at %.0f", c_abs(pull), py_mod(degrees(c_angle(pull)), 360.0));
    snprintf(rows[16], 64u, "%.3f", 1.0 - c_abs(pull));
    snprintf(rows[17], 64u, "%.3f", cross_rms);
    if (along_rms != 0.0)
    {
        snprintf(rows[18], 64u, "%.3f", cross_rms / along_rms);
    }
    else
    {
        snprintf(rows[18], 64u, "-");
    }
    snprintf(rows[19], 64u, "%.3f%%", (100.0 * (double)near_steps.count) / denominator);
    if (far_steps.count > 0u)
    {
        snprintf(rows[20], 64u, "%.3f, %.3f", median_of(far_radius.values, far_radius.count) / r_mid,
                 median_of(far_spin.values, far_spin.count) / t_mid);
        snprintf(rows[21], 64u, "%.3f", median_of(far_dip.values, far_dip.count) / r_mid);
    }
    else
    {
        snprintf(rows[20], 64u, "-");
        snprintf(rows[21], 64u, "-");
    }
    if ((far_steps.count > 0u) && (near_steps.count > 0u))
    {
        double core = sorted_at(near_steps.values, near_steps.count, (9u * near_steps.count) / 10u);
        core = (core != 0.0) ? core : 1.0;
        snprintf(rows[22], 64u, "%.2f", median_of(far_steps.values, far_steps.count) / core);
    }
    else
    {
        snprintf(rows[22], 64u, "-");
    }
    snprintf(rows[23], 64u, "%llu", (unsigned long long)count);
    free(scatter.values);
    free(near_steps.values);
    free(far_steps.values);
    free(far_radius.values);
    free(far_spin.values);
    free(far_dip.values);
}

/**
 * @brief The heights side by side: the pull from the arc in steps, the radius over its median, and the turn over its
 *        median.
 *
 * @param[in]  folds  The heights [BORROWS].
 * @param[in]  count  How many.
 * @param[in]  folder The folder [BORROWS].
 * @param[out] path   The path of the figure [BORROWS]. PATH_BYTES of them.
 * @note Every point has a dashed line, the misses a solid one, and each height one color.
 */
static void draw_folds(const Fold *folds, size_t count, const char *folder, char path[PATH_BYTES])
{
    static const char *const COLORS[5] = {"#4a7fb5", "#e08a2c", "#3a9a3a", "#a03ab0", "#b03a3a"};
    const size_t shown = (count < 5u) ? count : 5u;
    const int width = 3 * PANEL;
    Figure out = {NULL, 0u, 0u};
    figure_begin(&out, width, PANEL);
    const double middle_x = PANEL / 2.0;
    const double middle_y = (PANEL / 2.0) + 10.0;
    const double rim = (PANEL / 2.0) - MARGIN;
    Doubles every_steps = {NULL, 0u, 0u};
    for (size_t fold = 0u; fold < count; fold += 1u)
    {
        for (size_t at = 0u; at < folds[fold].misses.count; at += 1u)
        {
            doubles_push(&every_steps, folds[fold].misses.values[at].residual_steps);
        }
    }
    double reach = ninety_fifth(every_steps.values, every_steps.count);
    reach = (reach != 0.0) ? reach : 1.0;
    free(every_steps.values);
    line_of(&out, "%s",
            "<text x=\"10\" y=\"18\" font-size=\"13\">from the steady arc, heading at 0, in steps; out to the 95th "
            "percentile</text>");
    for (int ring = 1; ring <= 4; ring += 1)
    {
        line_of(&out, "<circle cx=\"%.1f\" cy=\"%.1f\" r=\"%.1f\" fill=\"none\" stroke=\"#ccc\"/>", middle_x, middle_y,
                (rim * ring) / 4.0);
        line_of(&out, "<text x=\"%.1f\" y=\"%.1f\" font-size=\"10\" fill=\"#666\">%.3g</text>", middle_x + 3.0,
                (middle_y - ((rim * ring) / 4.0)) - 2.0, (reach * ring) / 4.0);
    }
    for (int spoke = 0; spoke < 360; spoke += 45)
    {
        const double turn = radians((double)spoke);
        line_of(&out, "<text x=\"%.1f\" y=\"%.1f\" font-size=\"10\" text-anchor=\"middle\">%d</text>",
                middle_x + ((rim + 14.0) * sin(turn)), (middle_y - ((rim + 14.0) * cos(turn))) + 4.0, spoke);
    }
    for (size_t fold = 0u; fold < shown; fold += 1u)
    {
        for (size_t at = 0u; at < folds[fold].misses.count; at += 1u)
        {
            const Placed *const miss = &folds[fold].misses.values[at];
            const double turn = radians(miss->residual_angle);
            const double ratio = miss->residual_steps / reach;
            const double reach_out = ((ratio < 1.0) ? ratio : 1.0) * rim;
            line_of(&out, "<circle cx=\"%.1f\" cy=\"%.1f\" r=\"1.6\" fill=\"%s\" fill-opacity=\"0.6\"/>",
                    middle_x + (reach_out * sin(turn)), middle_y - (reach_out * cos(turn)), COLORS[fold]);
        }
    }
    static const char *const TITLES[2] = {"the radius over its median", "the turn a step over its median"};
    for (int panel = 0; panel < 2; panel += 1)
    {
        const Box box = axes(&out, (panel + 1) * PANEL, 0, TITLES[panel], "in the height's own units, to 4", "share");
        const size_t curve_count = 2u * shown;
        double (*const curves)[40] = (double (*)[40])map_memory(malloc((curve_count + 1u) * sizeof(double[40])));
        double tallest = 0.0;
        int any = 0;
        for (size_t fold = 0u; fold < shown; fold += 1u)
        {
            const Doubles *const every = (panel == 0) ? &folds[fold].radii : &folds[fold].turns;
            double middle = median_of(every->values, every->count);
            middle = (middle != 0.0) ? middle : 1.0;
            Doubles ours = {NULL, 0u, 0u};
            field_values(&folds[fold].misses, (panel == 0) ? FIELD_RADIUS : FIELD_SPIN_SIZE, &ours);
            for (int series = 0; series < 2; series += 1)
            {
                const Doubles *const values = (series == 0) ? every : &ours;
                int64_t bins[40] = {0};
                for (size_t at = 0u; at < values->count; at += 1u)
                {
                    const double scaled = values->values[at] / middle;
                    if (scaled < 4.0)
                    {
                        // The bin of a value below 4 is from 0 to 39.
                        bins[(int)(10.0 * scaled)] += 1;
                    }
                }
                double *const shares = curves[(2u * fold) + (size_t)series];
                const double total = (double)((values->count > 0u) ? values->count : 1u);
                for (int bin = 0; bin < 40; bin += 1)
                {
                    shares[bin] = (double)bins[bin] / total;
                    tallest = (!any || (shares[bin] > tallest)) ? shares[bin] : tallest;
                    any = 1;
                }
            }
            free(ours.values);
        }
        tallest = (tallest != 0.0) ? tallest : 1.0;
        for (size_t curve = 0u; curve < curve_count; curve += 1u)
        {
            Points points = {NULL, 0u, 0u};
            for (int bin = 0; bin < 40; bin += 1)
            {
                point_of(&points, "%.1f,%.1f", box.left + (((double)(box.right - box.left) * (bin + 0.5)) / 40.0),
                         box.bottom - (((double)(box.bottom - box.top) * curves[curve][bin]) / tallest));
            }
            const int dashed = (curve % 2u) == 0u;
            line_of(&out, "<polyline points=\"%s\" fill=\"none\" stroke=\"%s\" stroke-width=\"1.4\"%s/>", points.text,
                    COLORS[curve / 2u], dashed ? " stroke-dasharray=\"4,3\"" : "");
            free(points.text);
        }
        free(curves);
    }
    for (size_t fold = 0u; fold < shown; fold += 1u)
    {
        // The place of a legend line is a small count.
        line_of(&out, "<text x=\"%d\" y=\"%d\" font-size=\"11\" fill=\"%s\">t from %.3g</text>", width - 120,
                40 + (14 * (int)fold), COLORS[fold], folds[fold].time);
    }
    line_of(&out, "%s", "</svg>");
    join_path(path, folder, "folds.svg");
    figure_write(&out, path);
    free(out.text);
}

/**
 * @brief Cells from base, then sqrt(e) times it, e times it and on: heights an e-fold apart in t.
 *
 * @param[in] binary The device program [BORROWS].
 * @param[in] base   The first cell.
 * @param[in] count  The cells of each height.
 * @param[in] folds  The heights.
 * @param[in] rate   The points a zero of every coarse lattice.
 * @param[in] given  The folder, or NULL [BORROWS].
 * @return           0 where every host check passes, else 1.
 */
static int main_folds(const char *binary, int64_t base, int64_t count, int64_t folds, double rate, const char *given)
{
    char folder[PATH_BYTES];
    folder_of(folder, given, "folds");
    TuringConstants constants;
    turing_constants(&constants);
    // The count of heights is small and positive.
    const size_t height_count = (size_t)folds;
    Fold *const found = (Fold *)map_memory(calloc(height_count + 1u, sizeof(Fold)));
    int64_t failed = 0;
    for (size_t height = 0u; height < height_count; height += 1u)
    {
        const int64_t nu = py_round((double)base * exp((double)height / 2.0));
        fold_cells(binary, &constants, nu, count, rate, &found[height]);
        char name[64];
        snprintf(name, sizeof(name), "misses_%lld.tsv", (long long)nu);
        char path[PATH_BYTES];
        join_path(path, folder, name);
        write_misses(&found[height].misses, path);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  cells ");
            mmgr_line_decimal_signed(output_line, (long long)nu);
            mmgr_line_put(output_line, " to ");
            mmgr_line_decimal_signed(output_line, (long long)(nu + count - 1));
            mmgr_line_put(output_line, ", t from ");
            mmgr_line_general(output_line, (double)(found[height].time), 4u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, ": ");
            mmgr_line_decimal(output_line, (unsigned long long)found[height].misses.count, 1u);
            mmgr_line_put(output_line, " misses, ");
            mmgr_line_decimal_signed(output_line, (long long)found[height].failed);
            mmgr_line_put(output_line, " host checks failed");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        failed += found[height].failed;
    }
    char (*const rows)[FOLD_ROWS][64] =
        (char (*)[FOLD_ROWS][64])map_memory(malloc((height_count + 1u) * sizeof(*rows)));
    for (size_t height = 0u; height < height_count; height += 1u)
    {
        fold_row(&found[height], rows[height]);
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put_columns(output_line, "", 40u);
        mmgr_line_put(output_line, " ");
        platform_line_out_ended();
    }
    for (size_t height = 0u; height < height_count; height += 1u)
    {
        char label[32];
        snprintf(label, sizeof(label), "e^%llu", (unsigned long long)height);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put_right(output_line, label, 18u);
            platform_line_out_ended();
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    for (int row = 0; row < FOLD_ROWS; row += 1)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put_columns(output_line, FOLD_LABELS[row], 40u);
            mmgr_line_put(output_line, " ");
            platform_line_out_ended();
        }
        for (size_t height = 0u; height < height_count; height += 1u)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put_right(output_line, rows[height][row], 18u);
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    char path[PATH_BYTES];
    draw_folds(found, height_count, folder, path);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the heights: ");
        mmgr_line_put(output_line, path);
        mmgr_line_put(output_line, "; ");
        mmgr_line_decimal_signed(output_line, (long long)failed);
        mmgr_line_put(output_line, " host checks failed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    for (size_t height = 0u; height < height_count; height += 1u)
    {
        placements_free(&found[height].misses);
        free(found[height].radii.values);
        free(found[height].turns.values);
    }
    free(rows);
    free(found);
    return (failed == 0) ? 0 : 1;
}

/* ---------------------------------------------------------------- the coarse lattices */

/**
 * @brief Coarse fine places whose gaps cycle through a pattern, the partial quotients of the carrier, scaled to a mean
 *        step.
 *
 * @param[in]  count_fine    The fine points.
 * @param[in]  step          The mean step.
 * @param[in]  pattern       The pattern [BORROWS].
 * @param[in]  pattern_count Its length.
 * @param[out] out           The places [BORROWS].
 * @note A uniform lattice is the pattern (1,), and {1,1,2} is a kick of period 3.
 */
static void comb_indices(size_t count_fine, int64_t step, const int64_t *pattern, size_t pattern_count, Places *out)
{
    int64_t total = 0;
    for (size_t at = 0u; at < pattern_count; at += 1u)
    {
        total += pattern[at];
    }
    // The step and the pattern are small counts.
    const double scale = (double)(step * (int64_t)pattern_count) / (double)total;
    const size_t stride = (size_t)((step > 1) ? step : 1);
    const size_t characters = (((count_fine / stride) + pattern_count) * 2u) + 8u;
    places_push(out, 0u);
    double acc = 0.0;
    for (size_t at = 0u; at < characters; at += 1u)
    {
        acc += (double)pattern[at % pattern_count] * scale;
        const int64_t lane = py_round(acc);
        // The lane is not negative.
        if ((size_t)lane >= count_fine)
        {
            break;
        }
        if ((size_t)lane > out->values[out->count - 1u])
        {
            places_push(out, (size_t)lane);
        }
    }
}

/**
 * @brief Coarse fine places of the metallic quasicrystal {n,n,n,...}: the Sturmian word of slope 1 / beta.
 *
 * @param[in]  count_fine The fine points.
 * @param[in]  step       The mean step.
 * @param[in]  metal      n: 1 for golden, 2 for silver.
 * @param[out] out        The places [BORROWS].
 * @note Beta is the metallic mean (n + sqrt(n^2 + 4)) / 2. The word has two gaps in ratio beta, scaled to the mean
 *       step.
 */
static void metallic_indices(size_t count_fine, int64_t step, int64_t metal, Places *out)
{
    // The metal is a small count.
    const double beta = ((double)metal + sqrt((double)((metal * metal) + 4))) / 2.0;
    const double omega = 1.0 / beta;
    const double short_gap = (double)step / ((1.0 - omega) + (beta * omega));
    const double long_gap = beta * short_gap;
    // The count is a small positive number.
    const size_t count = (size_t)((double)count_fine / short_gap) + 8u;
    places_push(out, 0u);
    double acc = 0.0;
    for (size_t at = 0u; at < count; at += 1u)
    {
        const double bit = floor((double)(at + 1u) * omega) - floor((double)at * omega);
        acc += (bit != 0.0) ? long_gap : short_gap;
        const int64_t lane = py_round(acc);
        // The lane is not negative.
        if ((size_t)lane >= count_fine)
        {
            break;
        }
        if ((size_t)lane > out->values[out->count - 1u])
        {
            places_push(out, (size_t)lane);
        }
    }
}

/**
 * @brief Coarse fine places at the antinodes of Z, where Im(w) changes sign.
 *
 * @param[in]  fine  The fine points [BORROWS].
 * @param[in]  count How many.
 * @param[out] out   The places [BORROWS].
 * @note A zero is where Re(w) crosses zero. The two axes alternate as w winds, and two antinodes trap each zero. A pair
 *       hides only where w crosses the imaginary axis twice with no crossing of the real axis between.
 */
static void antinode_indices(const MapPoint *fine, size_t count, Places *out)
{
    places_push(out, 0u);
    for (size_t lane = 1u; lane < count; lane += 1u)
    {
        if ((fine[lane - 1u].w.im <= 0.0) != (fine[lane].w.im <= 0.0))
        {
            const size_t near_lane = (fabs(fine[lane].w.im) <= fabs(fine[lane - 1u].w.im)) ? lane : (lane - 1u);
            if (near_lane > out->values[out->count - 1u])
            {
                places_push(out, near_lane);
            }
        }
    }
    if (out->values[out->count - 1u] != (count - 1u))
    {
        places_push(out, count - 1u);
    }
}

/**
 * @brief Coarse fine places at the extrema of Z, where its step changes sign.
 *
 * @param[in]  fine  The fine points [BORROWS].
 * @param[in]  count How many.
 * @param[out] out   The places [BORROWS].
 * @note By Rolle, Z is monotone between consecutive extrema, and holds one zero at most there.
 */
static void extremum_indices(const MapPoint *fine, size_t count, Places *out)
{
    places_push(out, 0u);
    for (size_t lane = 1u; (lane + 1u) < count; lane += 1u)
    {
        if (((fine[lane].z - fine[lane - 1u].z) > 0.0) != ((fine[lane + 1u].z - fine[lane].z) > 0.0))
        {
            places_push(out, lane);
        }
    }
    places_push(out, count - 1u);
}

/**
 * @brief The extrema on the wrong side of zero: a positive minimum or a negative maximum of Z.
 *
 * @param[in] places The extrema [BORROWS].
 * @param[in] fine   The fine points [BORROWS].
 * @return           How many.
 */
static int64_t wiggles_of(const Places *places, const MapPoint *fine)
{
    int64_t count = 0;
    for (size_t at = 1u; (at + 1u) < places->count; at += 1u)
    {
        const size_t lane = places->values[at];
        const int low = fine[lane].z < fine[lane - 1u].z;
        if ((low && (fine[lane].z > 0.0)) || (!low && (fine[lane].z < 0.0)))
        {
            count += 1;
        }
    }
    return count;
}

/** @brief A uniform coarse lattice of about a count of places across the fine lattice. */
static void uniform_indices(size_t count_fine, size_t points, Places *out)
{
    const size_t divisor = (points > 1u) ? points : 1u;
    const size_t quotient = count_fine / divisor;
    const size_t step = (quotient > 1u) ? quotient : 1u;
    for (size_t lane = 0u; lane < count_fine; lane += step)
    {
        places_push(out, lane);
    }
}

/** @brief A rate as the exact decimal that its repr gives: numerator and denominator. */
typedef struct
{
    OriorExactInteger num; /**< The numerator. */
    OriorExactInteger den; /**< The denominator, a power of ten. */
} ExactRate;

/**
 * @brief The rate as the exact decimal that its repr gives, as Fraction(str(rate)) of Python reads it.
 *
 * @param[in]  rate  The rate.
 * @param[out] exact The fraction [BORROWS]. Its terms can share a power of ten, and the uses here do not depend on it.
 */
static void exact_rate(double rate, ExactRate *exact)
{
    char text[64];
    py_repr(rate, 1, text, sizeof(text));
    char digits[64];
    size_t length = 0u;
    int32_t places = 0;
    int32_t after = 0;
    int32_t power = 0;
    for (const char *at = text; *at != '\0'; at += 1)
    {
        if ((*at >= '0') && (*at <= '9'))
        {
            digits[length] = *at;
            length += 1u;
            places += after;
        }
        else if (*at == '.')
        {
            after = 1;
        }
        else if ((*at == 'e') || (*at == 'E'))
        {
            power = (int32_t)atoi(at + 1);
            break;
        }
    }
    digits[length] = '\0';
    z_set(&exact->num, 0);
    for (size_t at = 0u; at < length; at += 1u)
    {
        z_mul_small(&exact->num, 10, &exact->num);
        OriorExactInteger digit;
        z_set(&digit, digits[at] - '0');
        z_add(&exact->num, &digit, &exact->num);
    }
    const int32_t shift = places - power;
    if (shift >= 0)
    {
        z_ten_to(shift, &exact->den);
    }
    else
    {
        OriorExactInteger scale;
        z_ten_to(-shift, &scale);
        z_mul(&exact->num, &scale, &exact->num);
        z_set(&exact->den, 1);
    }
}

/**
 * @brief The lattice even in theta / pi at rate points a unit, by integer comparison alone.
 *
 * @param[in]  fine  The fine points, with the listing word 3 [BORROWS].
 * @param[in]  count How many.
 * @param[in]  rate  The rate.
 * @param[out] out   The places [BORROWS].
 * @note For each k in turn, it takes the first fine point whose theta / pi less its bound reaches a mark. The mark is
 *       that of the first point of the cell plus k / rate.
 */
static void theta_indices(const MapPoint *fine, size_t count, double rate, Places *out)
{
    static ExactRate exact;
    exact_rate(rate, &exact);
    static OriorExactInteger origin;
    static OriorExactInteger low;
    static OriorExactInteger lifted;
    static OriorExactInteger unit_den;
    static OriorExactInteger mark;
    static OriorExactInteger mark_index;
    static OriorExactInteger next;
    static OriorExactInteger one;
    z_two_to(TURING_SCALE_BITS, &unit_den);
    z_mul(&unit_den, &exact.den, &unit_den);
    turing_parse_hex(fine[0].low_text, &origin);
    z_set(&mark_index, 0);
    z_set(&one, 1);
    for (size_t lane = 0u; lane < count; lane += 1u)
    {
        turing_parse_hex(fine[lane].low_text, &low);
        z_sub(&low, &origin, &low);
        z_mul(&low, &exact.num, &lifted);
        z_mul(&mark_index, &unit_den, &mark);
        if (z_cmp(&lifted, &mark) >= 0)
        {
            places_push(out, lane);
            z_floor_div(&lifted, &unit_den, &next);
            z_add(&next, &one, &mark_index);
        }
    }
}

/** @brief The names of the coarse lattices of the carrier mode, in the order of the report. */
#define CARRIER_COUNT 8
static const char *const CARRIER_NAMES[CARRIER_COUNT] = {
    "uniform @ rate", "theta / pi even @ rate", "antinode Im w=0",   "uniform @ antinode n",
    "turning Z'=0",   "uniform @ turning n",    "comb 1,1,2 @ rate", "golden @ rate"};

/** @brief The place of the extrema among the lattices of the carrier mode. */
#define CARRIER_TURNING 4

/**
 * @brief The coarse lattices to score, in the order of CARRIER_NAMES.
 *
 * @param[in]  fine    The fine points [BORROWS].
 * @param[in]  count   How many.
 * @param[in]  step    The step at the rate.
 * @param[in]  rate    The rate.
 * @param[out] schemes The lattices [BORROWS].
 * @note Each trap comes with a uniform lattice of its own point count, for a comparison at the same density. The
 *       {1,1,2} and golden combs at the rate come for reference.
 */
static void carrier_lattices(const MapPoint *fine, size_t count, int64_t step, double rate,
                             Places schemes[CARRIER_COUNT])
{
    static const int64_t UNIFORM[1] = {1};
    static const int64_t KICK[3] = {1, 1, 2};
    memset(schemes, 0, CARRIER_COUNT * sizeof(Places));
    comb_indices(count, step, UNIFORM, 1u, &schemes[0]);
    theta_indices(fine, count, rate, &schemes[1]);
    antinode_indices(fine, count, &schemes[2]);
    uniform_indices(count, schemes[2].count, &schemes[3]);
    extremum_indices(fine, count, &schemes[CARRIER_TURNING]);
    uniform_indices(count, schemes[CARRIER_TURNING].count, &schemes[5]);
    comb_indices(count, step, KICK, 3u, &schemes[6]);
    metallic_indices(count, step, 1, &schemes[7]);
}

/** @brief The pickle of a run: the width across the track, the RMS along it, and the share within a step. */
typedef struct
{
    double cross; /**< The RMS across the track. */
    double along; /**< The RMS along the track. */
    double under; /**< The share within a step of the center, in percent. */
    size_t count; /**< The misses. */
} Pickle;

/** @brief The pickle of a list of misses placed on a lattice. */
static Pickle pickle_of(const Placements *misses)
{
    Pickle pickle = {0.0, 0.0, 0.0, 0u};
    if (misses->count == 0u)
    {
        return pickle;
    }
    const double count = (double)misses->count;
    Sum cross = {0.0, 0.0};
    Sum along = {0.0, 0.0};
    size_t under = 0u;
    for (size_t at = 0u; at < misses->count; at += 1u)
    {
        sum_add(&cross, pow(misses->values[at].cross, 2.0));
        sum_add(&along, pow(misses->values[at].along, 2.0));
        under += (misses->values[at].d_steps <= 1.0) ? 1u : 0u;
    }
    pickle.cross = sqrt(sum_value(&cross) / count);
    pickle.along = sqrt(sum_value(&along) / count);
    pickle.under = (100.0 * (double)under) / count;
    pickle.count = misses->count;
    return pickle;
}

/**
 * @brief Each coarse lattice over cells base to base + count - 1, on one fine lattice each.
 *
 * @param[in] binary The device program [BORROWS].
 * @param[in] base   The first cell.
 * @param[in] count  The cells.
 * @param[in] rate   The points a zero.
 * @param[in] given  The folder, or NULL [BORROWS].
 * @return           0 where every host check passes, else 1.
 * @note For each coarse lattice, the run places the misses it hides. It reports the misses a thousand zeros, the
 *       width of the pickle and the share under us. The antinode lattice traps a zero between each pair of peaks. The
 *       uniform lattice of the same count is the fair baseline for it.
 */
static int main_carrier(const char *binary, int64_t base, int64_t count, double rate, const char *given)
{
    char folder[PATH_BYTES];
    folder_of(folder, given, "carrier");
    TuringConstants constants;
    turing_constants(&constants);
    Placements tally[CARRIER_COUNT];
    memset(tally, 0, sizeof(tally));
    int64_t points[CARRIER_COUNT] = {0};
    int64_t floor_count[CARRIER_COUNT] = {0};
    int64_t zeros_total = 0;
    int64_t failed = 0;
    int64_t wiggles = 0;
    for (int64_t at = base; at < (base + count); at += 1)
    {
        const double rise = turing_rises(at);
        const int64_t fine_p = fine_bits(rate, rise);
        const int64_t step = coarse_step(fine_p, rate, rise);
        Listing fine;
        failed += run_listed(binary, &constants, at, fine_p, 3, &fine);
        // The count of zeros fits a machine integer.
        const int64_t zeros = (int64_t)zero_count(fine.points, fine.count);
        zeros_total += zeros;
        Places schemes[CARRIER_COUNT];
        carrier_lattices(fine.points, fine.count, step, rate, schemes);
        for (int scheme = 0; scheme < CARRIER_COUNT; scheme += 1)
        {
            Misses found;
            misses_on(&schemes[scheme], fine.points, fine.count, &found);
            for (size_t miss = 0u; miss < found.count; miss += 1u)
            {
                Placed placed;
                place_on(at, &schemes[scheme], fine.points, &found.values[miss], &placed);
                placements_push(&tally[scheme], &placed);
            }
            free(found.values);
            // The count of places fits a machine integer.
            const int64_t size = (int64_t)schemes[scheme].count;
            points[scheme] += size;
            const int64_t past = zeros - (size - 1);
            floor_count[scheme] += (past > 0) ? past : 0;
            if (scheme == CARRIER_TURNING)
            {
                wiggles += wiggles_of(&schemes[scheme], fine.points);
            }
            free(schemes[scheme].values);
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  cell ");
            mmgr_line_decimal_signed(output_line, (long long)at);
            mmgr_line_put(output_line, " done, ");
            mmgr_line_decimal(output_line, (unsigned long long)fine.count, 1u);
            mmgr_line_put(output_line, " fine points, step ");
            mmgr_line_decimal_signed(output_line, (long long)step);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        free(fine.points);
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put_columns(output_line, "scheme", 22u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "points", 8u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "a zero", 9u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "miss/1k zero", 12u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "floor/1k", 12u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "width", 9u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "under%", 9u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    const double zeros = (double)((zeros_total > 1) ? zeros_total : 1);
    for (int scheme = 0; scheme < CARRIER_COUNT; scheme += 1)
    {
        const Pickle pickle = pickle_of(&tally[scheme]);
        const double permille = ((1000.0 * 2.0) * (double)pickle.count) / zeros;
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put_columns(output_line, CARRIER_NAMES[scheme], 22u);
            mmgr_line_put(output_line, " ");
            {
                char linea_piece[24u];
                VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                const int64_t linea_value = (int64_t)((long long)points[scheme]);
                mmgr_line_decimal_signed(&linea_piece_line, linea_value);
                (void)mmgr_line_finish(&linea_piece_line);
                mmgr_line_put_right(output_line, linea_piece, 8u);
            }
            mmgr_line_put(output_line, " ");
            mmgr_line_fixed(output_line, (double)((double)points[scheme] / zeros), 4u, 9u, EMBED_FALSE);
            mmgr_line_put(output_line, " ");
            mmgr_line_fixed(output_line, (double)(permille), 3u, 12u, EMBED_FALSE);
            mmgr_line_put(output_line, " ");
            mmgr_line_fixed(output_line, (double)((1000.0 * (double)floor_count[scheme]) / zeros), 3u, 12u,
                            EMBED_FALSE);
            mmgr_line_put(output_line, " ");
            mmgr_line_fixed(output_line, (double)(pickle.cross), 4u, 9u, EMBED_FALSE);
            mmgr_line_put(output_line, " ");
            mmgr_line_fixed(output_line, (double)(pickle.under), 2u, 8u, EMBED_FALSE);
            mmgr_line_ch(output_line, '%');
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        placements_free(&tally[scheme]);
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  zeros ");
        mmgr_line_decimal_signed(output_line, (long long)zeros_total);
        mmgr_line_put(output_line, "; turning points on the wrong side of zero ");
        mmgr_line_decimal_signed(output_line, (long long)wiggles);
        mmgr_line_put(output_line, " (");
        mmgr_line_fixed(output_line, (double)((1000.0 * (double)wiggles) / zeros), 3u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " a thousand zeros); ");
        mmgr_line_decimal_signed(output_line, (long long)failed);
        mmgr_line_put(output_line, " host checks failed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return (failed == 0) ? 0 : 1;
}

/* ---------------------------------------------------------------- the ripple */

/** @brief The beats ln(n / m) that the ripple and the source modes lock the times on, and the nulls between them. */
#define RIPPLE_PAIR_COUNT 8
static const int64_t RIPPLE_PAIRS[RIPPLE_PAIR_COUNT][2] = {{1, 2}, {1, 3}, {2, 3}, {1, 4},
                                                           {3, 4}, {1, 5}, {2, 5}, {1, 6}};
#define RIPPLE_NULL_COUNT 5
static const double RIPPLE_NULLS[RIPPLE_NULL_COUNT] = {0.5, 0.9, 1.3, 1.9, 2.3};

/** @brief The drag and the swell of F at a fine point. */
typedef struct
{
    double drag;  /**< phi' / theta'. */
    double swell; /**< (ln |F|)' / theta'. */
} Ripple;

/**
 * @brief The drag and swell of F at a fine point, each over theta' = ln x.
 *
 * @param[in] fine    The fine points [BORROWS].
 * @param[in] count   How many.
 * @param[in] squares x^2 at each point [BORROWS].
 * @param[in] lane    The point.
 * @return            The drag and the swell.
 * @note The drag is 1 where F does not turn, and below 0 where the clock runs back. The swell is the plane on the
 *       rise. The code reads phi, the arg of w, across the point before to the point after.
 */
static Ripple ripple_at(const MapPoint *fine, size_t count, const double *squares, size_t lane)
{
    const size_t low = (lane > 0u) ? (lane - 1u) : 0u;
    const size_t high = ((lane + 1u) < (count - 1u)) ? (lane + 1u) : (count - 1u);
    const Complex before = fine[low].w;
    const Complex after = fine[high].w;
    const double dt = TWO_PI * (squares[high] - squares[low]);
    const double clock = 0.5 * log(squares[lane]);
    if ((c_abs(before) == 0.0) || (c_abs(after) == 0.0) || (dt == 0.0))
    {
        const Ripple still = {1.0, 0.0};
        return still;
    }
    const Complex turn = c_div(after, before);
    const Ripple ripple = {(c_angle(turn) / dt) / clock, ((log(c_abs(after)) - log(c_abs(before))) / dt) / clock};
    return ripple;
}

/** @brief The lock of a set of times on a frequency, by Rayleigh. */
typedef struct
{
    double length;    /**< The mean resultant length. */
    double direction; /**< Its direction in degrees. */
    double z;         /**< n R^2. */
} Lock;

/**
 * @brief The mean resultant length of the phases omega t over the times, its direction, and z = n R^2 of Rayleigh.
 *
 * @param[in] times The times [BORROWS].
 * @param[in] omega The frequency.
 * @return          The lock.
 * @note Among uniform phases, the chance of so tight a lock is near exp(-z).
 */
static Lock rayleigh(const Doubles *times, double omega)
{
    Lock lock = {0.0, 0.0, 0.0};
    if (times->count == 0u)
    {
        return lock;
    }
    Sum cosine = {0.0, 0.0};
    Sum sine = {0.0, 0.0};
    for (size_t at = 0u; at < times->count; at += 1u)
    {
        sum_add(&cosine, cos(omega * times->values[at]));
    }
    for (size_t at = 0u; at < times->count; at += 1u)
    {
        sum_add(&sine, sin(omega * times->values[at]));
    }
    const double count = (double)times->count;
    const double c_mean = sum_value(&cosine) / count;
    const double s_mean = sum_value(&sine) / count;
    lock.length = py_hypot(c_mean, s_mean);
    lock.direction = py_mod(degrees(atan2(s_mean, c_mean)), 360.0);
    lock.z = (count * lock.length) * lock.length;
    return lock;
}

/** @brief The lock of the times on each beat and each null, one row each. */
static void lock_rows(const Doubles *times)
{
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put_columns(output_line, "beat", 14u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "omega", 10u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "R", 9u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "direction", 10u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "z", 9u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    for (int pair = 0; pair < RIPPLE_PAIR_COUNT; pair += 1)
    {
        const double omega = log((double)RIPPLE_PAIRS[pair][1] / (double)RIPPLE_PAIRS[pair][0]);
        const Lock lock = rayleigh(times, omega);
        char label[32];
        snprintf(label, sizeof(label), "ln(%lld/%lld)", (long long)RIPPLE_PAIRS[pair][1],
                 (long long)RIPPLE_PAIRS[pair][0]);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put_columns(output_line, label, 14u);
            mmgr_line_put(output_line, " ");
            mmgr_line_fixed(output_line, (double)(omega), 5u, 10u, EMBED_FALSE);
            mmgr_line_put(output_line, " ");
            mmgr_line_fixed(output_line, (double)(lock.length), 4u, 9u, EMBED_FALSE);
            mmgr_line_put(output_line, " ");
            mmgr_line_fixed(output_line, (double)(lock.direction), 1u, 10u, EMBED_FALSE);
            mmgr_line_put(output_line, " ");
            mmgr_line_fixed(output_line, (double)(lock.z), 2u, 9u, EMBED_FALSE);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    for (int null = 0; null < RIPPLE_NULL_COUNT; null += 1)
    {
        const Lock lock = rayleigh(times, RIPPLE_NULLS[null]);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put_columns(output_line, "null", 14u);
            mmgr_line_put(output_line, " ");
            mmgr_line_fixed(output_line, (double)(RIPPLE_NULLS[null]), 5u, 10u, EMBED_FALSE);
            mmgr_line_put(output_line, " ");
            mmgr_line_fixed(output_line, (double)(lock.length), 4u, 9u, EMBED_FALSE);
            mmgr_line_put(output_line, " ");
            mmgr_line_fixed(output_line, (double)(lock.direction), 1u, 10u, EMBED_FALSE);
            mmgr_line_put(output_line, " ");
            mmgr_line_fixed(output_line, (double)(lock.z), 2u, 9u, EMBED_FALSE);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
}

/**
 * @brief The 10th, 50th and 90th places of the sorted values, each in a format, joined by spaces.
 *
 * @param[in]  values   The values [BORROWS].
 * @param[in]  format   The format of each [BORROWS].
 * @param[out] text     The text [BORROWS].
 * @param[in]  capacity Bytes of the text.
 */
static void deciles_text(const Doubles *values, const char *format, char *text, size_t capacity)
{
    double *const ordered = sorted_copy(values->values, values->count);
    static const double FRACTIONS[3] = {0.1, 0.5, 0.9};
    size_t used = 0u;
    text[0] = '\0';
    for (int at = 0; at < 3; at += 1)
    {
        char part[64];
        snprintf(part, sizeof(part), format, ordered[quantile_at(FRACTIONS[at], values->count)]);
        // The text holds a few dozen characters.
        used += (size_t)snprintf(text + used, capacity - used, "%s%s", (at > 0) ? " " : "", part);
    }
    free(ordered);
}

/**
 * @brief The misses of the uniform lattice at the rate, locked on the beats of |F|^2.
 *
 * @param[in] binary The device program [BORROWS].
 * @param[in] base   The first cell.
 * @param[in] count  The cells.
 * @param[in] rate   The points a zero.
 * @return           0 where every host check passes, else 1.
 * @note The code locks the times of the misses at the frequencies ln(n / m), and at null frequencies between them. It
 *       reads the drag and swell of F at the dips of the misses against those of every fine point.
 */
static int main_ripple(const char *binary, int64_t base, int64_t count, double rate)
{
    TuringConstants constants;
    turing_constants(&constants);
    Doubles times = {NULL, 0u, 0u};
    Doubles dip_drags = {NULL, 0u, 0u};
    Doubles dip_swells = {NULL, 0u, 0u};
    Doubles every_drags = {NULL, 0u, 0u};
    Doubles every_swells = {NULL, 0u, 0u};
    int64_t failed = 0;
    static const int64_t UNIFORM[1] = {1};
    for (int64_t at = base; at < (base + count); at += 1)
    {
        const double rise = turing_rises(at);
        const int64_t fine_p = fine_bits(rate, rise);
        const int64_t step = coarse_step(fine_p, rate, rise);
        Listing fine;
        failed += run_listed(binary, &constants, at, fine_p, 1, &fine);
        double *const squares = squares_of(at, fine.count);
        for (size_t lane = 1u; (lane + 1u) < fine.count; lane += 7u)
        {
            const Ripple ripple = ripple_at(fine.points, fine.count, squares, lane);
            doubles_push(&every_drags, ripple.drag);
            doubles_push(&every_swells, fabs(ripple.swell));
        }
        Places idx = {NULL, 0u, 0u};
        comb_indices(fine.count, step, UNIFORM, 1u, &idx);
        Misses found;
        misses_on(&idx, fine.points, fine.count, &found);
        for (size_t miss = 0u; miss < found.count; miss += 1u)
        {
            const size_t dip = dip_of(fine.points, &found.values[miss]);
            const Ripple ripple = ripple_at(fine.points, fine.count, squares, dip);
            doubles_push(&times, TWO_PI * squares[dip]);
            doubles_push(&dip_drags, ripple.drag);
            doubles_push(&dip_swells, fabs(ripple.swell));
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  cell ");
            mmgr_line_decimal_signed(output_line, (long long)at);
            mmgr_line_put(output_line, " done, ");
            mmgr_line_decimal(output_line, (unsigned long long)times.count, 1u);
            mmgr_line_put(output_line, " misses so far");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        free(found.values);
        free(idx.values);
        free(squares);
        free(fine.points);
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  misses ");
        mmgr_line_decimal(output_line, (unsigned long long)times.count, 1u);
        mmgr_line_put(output_line, "; the lock of their times on each beat of |F|^2");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    lock_rows(&times);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put_columns(output_line, "reading", 34u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "every point 10/50/90", 28u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "the misses' dips 10/50/90", 28u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    char every_text[128];
    char dip_text[128];
    deciles_text(&every_drags, "%9.3f", every_text, sizeof(every_text));
    deciles_text(&dip_drags, "%9.3f", dip_text, sizeof(dip_text));
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put_columns(output_line, "drag phi' / theta'", 34u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, every_text, 28u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, dip_text, 28u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    deciles_text(&every_swells, "%9.3f", every_text, sizeof(every_text));
    deciles_text(&dip_swells, "%9.3f", dip_text, sizeof(dip_text));
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put_columns(output_line, "|swell| (ln |F|)' / theta'", 34u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, every_text, 28u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, dip_text, 28u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    size_t back_every = 0u;
    for (size_t at = 0u; at < every_drags.count; at += 1u)
    {
        back_every += (every_drags.values[at] < 0.0) ? 1u : 0u;
    }
    size_t back_dips = 0u;
    for (size_t at = 0u; at < dip_drags.count; at += 1u)
    {
        back_dips += (dip_drags.values[at] < 0.0) ? 1u : 0u;
    }
    const double every_count = (double)((every_drags.count > 0u) ? every_drags.count : 1u);
    const double dip_count = (double)((dip_drags.count > 0u) ? dip_drags.count : 1u);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  clock running back, phi' < 0: every point ");
        mmgr_line_fixed(output_line, (double)((100.0 * (double)back_every) / every_count), 2u, 0u, EMBED_FALSE);
        mmgr_line_ch(output_line, '%');
        mmgr_line_put(output_line, ", the misses' dips ");
        mmgr_line_fixed(output_line, (double)((100.0 * (double)back_dips) / dip_count), 2u, 0u, EMBED_FALSE);
        mmgr_line_ch(output_line, '%');
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_decimal_signed(output_line, (long long)failed);
        mmgr_line_put(output_line, " host checks failed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    free(times.values);
    free(dip_drags.values);
    free(dip_swells.values);
    free(every_drags.values);
    free(every_swells.values);
    return (failed == 0) ? 0 : 1;
}

/* ---------------------------------------------------------------- the pulses */

/** @brief A twist pulse that F casts off where it passes near a zero of its own. */
typedef struct
{
    double delta;       /**< |F| / |F'|. */
    double clock_delta; /**< delta theta'. */
    double peak;        /**< The peak twist times delta. */
    double sign;        /**< The side of the twist. */
    double loop_low;    /**< The diameter of the loop over 1 / delta, its 10th percentile. */
    double loop_high;   /**< The same, its 90th percentile. */
    double egg;         /**< The lead of the swell over its trail. */
    double turn;        /**< The turn over the window over 2 atan(reach). */
    size_t low;         /**< The first fine place of the window. */
    size_t high;        /**< The last. */
} Pulse;

/** @brief A growable list of pulses. */
typedef struct
{
    Pulse *values;   /**< The pulses [OWNS]. */
    size_t count;    /**< How many. */
    size_t capacity; /**< How many the array holds. */
} Pulses;

/**
 * @brief F' at a fine point, from the step of w across it less the turn of the clock.
 *
 * @param[in] fine  The fine points [BORROWS].
 * @param[in] lane  The point.
 * @param[in] dt    The fine step in t.
 * @param[in] clock theta' there.
 * @return          exp(i theta) F' at the point.
 */
static Complex derivative_at(const MapPoint *fine, size_t lane, double dt, double clock)
{
    const Complex step = c_over(c_sub(fine[lane + 1u].w, fine[lane - 1u].w), 2.0 * dt);
    return c_sub(step, c_mul(cx(0.0, clock), fine[lane].w));
}

/** @brief Whether a fine point is a local minimum of |F|, as the Python tests it. */
static int minimum_at(const MapPoint *fine, size_t lane)
{
    const double here = c_abs(fine[lane].w);
    return (here < c_abs(fine[lane - 1u].w)) && (here <= c_abs(fine[lane + 1u].w));
}

/**
 * @brief The twist pulses that F casts off where it passes near a zero of its own, t* = gamma + i delta.
 *
 * @param[in]  fine    The fine points [BORROWS].
 * @param[in]  count   How many.
 * @param[in]  squares x^2 at each point [BORROWS].
 * @param[out] out     The pulses [BORROWS].
 * @note There d/dt ln F = 1 / (t - t*), one pole, with u = t - gamma. The swell is u / (u^2 + delta^2), and the twist
 *       is delta / (u^2 + delta^2). The twist is a Lorentzian of height 1 / delta and turn pi.
 * @note The code takes each local minimum of |F| where delta = |F| / |F'| is 4 fine steps or more. Delta must also be
 *       under the own 1 / theta' of the clock. For the pulses past the clock, it reads 4 deltas each side.
 */
static void pulses_of(const MapPoint *fine, size_t count, const double *squares, Pulses *out)
{
    const double reach = 4.0;
    const double least = 4.0;
    const double dt = TWO_PI * (squares[1] - squares[0]);
    Doubles swells = {NULL, 0u, 0u};
    Doubles twists = {NULL, 0u, 0u};
    Doubles loop = {NULL, 0u, 0u};
    for (size_t lane = 2u; (lane + 2u) < count; lane += 1u)
    {
        if (!minimum_at(fine, lane))
        {
            continue;
        }
        const double clock = 0.5 * log(squares[lane]);
        const double f_prime = c_abs(derivative_at(fine, lane, dt, clock));
        if (f_prime == 0.0)
        {
            continue;
        }
        const double delta = c_abs(fine[lane].w) / f_prime;
        // The window is a small positive count of fine steps.
        const int64_t span = (int64_t)ceil((reach * delta) / dt);
        // The lanes are below 2^63.
        const int64_t here = (int64_t)lane;
        if ((delta < (least * dt)) || ((delta * clock) >= 1.0) || ((here - span) < 1) ||
            ((here + span) > ((int64_t)count - 2)))
        {
            continue;
        }
        swells.count = 0u;
        twists.count = 0u;
        for (int64_t at = here - span; at <= (here + span); at += 1)
        {
            const Complex turn = c_div(fine[at + 1].w, fine[at - 1].w);
            doubles_push(&twists, (c_angle(turn) / (2.0 * dt)) - (0.5 * log(squares[at])));
            doubles_push(&swells, (log(c_abs(fine[at + 1].w)) - log(c_abs(fine[at - 1].w))) / (2.0 * dt));
        }
        // The window is a small positive count of fine steps.
        const size_t middle = (size_t)span;
        const double sign = (twists.values[middle] > 0.0) ? 1.0 : -1.0;
        double peak = sign * twists.values[0];
        for (size_t at = 1u; at < twists.count; at += 1u)
        {
            peak = ((sign * twists.values[at]) > peak) ? (sign * twists.values[at]) : peak;
        }
        loop.count = 0u;
        for (size_t at = 0u; at < twists.count; at += 1u)
        {
            const double swell_value = swells.values[at];
            const double twist_value = twists.values[at];
            if ((sign * twist_value) > (0.25 * peak))
            {
                doubles_push(&loop,
                             (((swell_value * swell_value) + (twist_value * twist_value)) / (sign * twist_value)) *
                                 delta);
            }
        }
        doubles_sort(&loop);
        double lead = swells.values[0];
        for (size_t at = 1u; at <= middle; at += 1u)
        {
            lead = (swells.values[at] < lead) ? swells.values[at] : lead;
        }
        lead = -lead;
        double trail = swells.values[middle];
        for (size_t at = middle + 1u; at < swells.count; at += 1u)
        {
            trail = (swells.values[at] > trail) ? swells.values[at] : trail;
        }
        Sum total = {0.0, 0.0};
        for (size_t at = 0u; at < twists.count; at += 1u)
        {
            sum_add(&total, twists.values[at]);
        }
        if (out->count >= out->capacity)
        {
            out->capacity = (out->capacity == 0u) ? 64u : (out->capacity * 2u);
            out->values = (Pulse *)map_memory(realloc(out->values, out->capacity * sizeof(Pulse)));
        }
        Pulse *const pulse = &out->values[out->count];
        pulse->delta = delta;
        pulse->clock_delta = delta * clock;
        pulse->peak = peak * delta;
        pulse->sign = sign;
        pulse->loop_low = (loop.count > 0u) ? loop.values[loop.count / 10u] : 0.0;
        pulse->loop_high = (loop.count > 0u) ? loop.values[(9u * loop.count) / 10u] : 0.0;
        pulse->egg = (trail > 0.0) ? (lead / trail) : 0.0;
        pulse->turn = ((sign * sum_value(&total)) * dt) / (2.0 * atan(reach));
        // The window lies inside the lattice, and its ends are not negative.
        pulse->low = (size_t)(here - span);
        pulse->high = (size_t)(here + span);
        out->count += 1u;
    }
    free(swells.values);
    free(twists.values);
    free(loop.values);
}

/**
 * @brief The twist pulses past the clock, read against the single pole, and the misses inside them.
 *
 * @param[in] binary The device program [BORROWS].
 * @param[in] base   The first cell.
 * @param[in] count  The cells.
 * @param[in] rate   The points a zero.
 * @return           0 where every host check passes, else 1.
 * @note Theta is monotone. Two zeros inside one coarse step want phi = theta + arg F to run back across a level, with
 *       phi' < 0. Else they want it to sweep pi inside the step, with phi' at least the rate times theta'. R moves the
 *       level by R / (2 |F|), the way left.
 */
static int main_pulse(const char *binary, int64_t base, int64_t count, double rate)
{
    TuringConstants constants;
    turing_constants(&constants);
    Pulses pulses = {NULL, 0u, 0u};
    int64_t misses = 0;
    int64_t inside = 0;
    int64_t covered = 0;
    int64_t total = 0;
    int64_t failed = 0;
    int64_t back = 0;
    int64_t spin = 0;
    int64_t neither = 0;
    double neither_max = 0.0;
    static const int64_t UNIFORM[1] = {1};
    for (int64_t at = base; at < (base + count); at += 1)
    {
        const double rise = turing_rises(at);
        const int64_t fine_p = fine_bits(rate, rise);
        const int64_t step = coarse_step(fine_p, rate, rise);
        Listing fine;
        failed += run_listed(binary, &constants, at, fine_p, 1, &fine);
        double *const squares = squares_of(at, fine.count);
        const size_t before = pulses.count;
        pulses_of(fine.points, fine.count, squares, &pulses);
        unsigned char *const marks = (unsigned char *)map_memory(calloc(fine.count + 1u, 1u));
        for (size_t pulse = before; pulse < pulses.count; pulse += 1u)
        {
            for (size_t lane = pulses.values[pulse].low; lane <= pulses.values[pulse].high; lane += 1u)
            {
                marks[lane] = 1u;
            }
        }
        for (size_t lane = 0u; lane < fine.count; lane += 1u)
        {
            covered += marks[lane];
        }
        // The count of fine points fits a machine integer.
        total += (int64_t)fine.count;
        Places idx = {NULL, 0u, 0u};
        comb_indices(fine.count, step, UNIFORM, 1u, &idx);
        Misses found;
        misses_on(&idx, fine.points, fine.count, &found);
        for (size_t miss = 0u; miss < found.count; miss += 1u)
        {
            const Miss *const pair = &found.values[miss];
            const size_t dip = dip_of(fine.points, pair);
            misses += 1;
            inside += marks[dip];
            double low_drag = 0.0;
            double high_drag = 0.0;
            for (size_t lane = pair->first.before; lane <= pair->second.after; lane += 1u)
            {
                const double drag = ripple_at(fine.points, fine.count, squares, lane).drag;
                low_drag = ((lane == pair->first.before) || (drag < low_drag)) ? drag : low_drag;
                high_drag = ((lane == pair->first.before) || (drag > high_drag)) ? drag : high_drag;
            }
            if (low_drag < 0.0)
            {
                back += 1;
            }
            else if (high_drag >= rate)
            {
                spin += 1;
            }
            else
            {
                neither += 1;
                neither_max = (high_drag > neither_max) ? high_drag : neither_max;
            }
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  cell ");
            mmgr_line_decimal_signed(output_line, (long long)at);
            mmgr_line_put(output_line, " done, ");
            mmgr_line_decimal(output_line, (unsigned long long)pulses.count, 1u);
            mmgr_line_put(output_line, " pulses so far");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        free(found.values);
        free(idx.values);
        free(marks);
        free(squares);
        free(fine.points);
    }
    int64_t against = 0;
    for (size_t pulse = 0u; pulse < pulses.count; pulse += 1u)
    {
        against += (pulses.values[pulse].sign < 0.0) ? 1 : 0;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  pulses past the clock, delta theta' under 1: ");
        mmgr_line_decimal(output_line, (unsigned long long)pulses.count, 1u);
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal_signed(output_line, (long long)against);
        mmgr_line_put(output_line, " turning against theta and ");
        mmgr_line_decimal_signed(output_line, (long long)pulses.count - (long long)against);
        mmgr_line_put(output_line, " with it");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put_columns(output_line, "against the single pole", 40u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "10/50/90", 26u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    static const char *const LABELS[6] = {
        "delta theta', under 1 past the clock",   "peak twist times delta, pole 1",
        "loop diameter low over 1/delta, pole 1", "loop diameter high over 1/delta, pole 1",
        "swell lead over trail, pole 1",          "turn over 2 atan(4), pole 1"};
    for (int label = 0; label < 6; label += 1)
    {
        Doubles values = {NULL, 0u, 0u};
        for (size_t pulse = 0u; pulse < pulses.count; pulse += 1u)
        {
            const Pulse *const one = &pulses.values[pulse];
            const double fields[6] = {one->clock_delta, one->peak, one->loop_low, one->loop_high, one->egg, one->turn};
            doubles_push(&values, fields[label]);
        }
        char text[128];
        if (values.count > 0u)
        {
            deciles_text(&values, "%8.3f", text, sizeof(text));
        }
        else
        {
            snprintf(text, sizeof(text), "-");
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put_columns(output_line, LABELS[label], 40u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put_right(output_line, text, 26u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        free(values.values);
    }
    const double miss_count = (double)((misses > 1) ? misses : 1);
    const double point_count = (double)((total > 1) ? total : 1);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  misses ");
        mmgr_line_decimal_signed(output_line, (long long)misses);
        mmgr_line_put(output_line, ", inside a pulse ");
        mmgr_line_decimal_signed(output_line, (long long)inside);
        mmgr_line_put(output_line, " (");
        mmgr_line_fixed(output_line, (double)((100.0 * (double)inside) / miss_count), 1u, 0u, EMBED_FALSE);
        mmgr_line_ch(output_line, '%');
        mmgr_line_put(output_line, "); the pulses cover ");
        mmgr_line_fixed(output_line, (double)((100.0 * (double)covered) / point_count), 2u, 0u, EMBED_FALSE);
        mmgr_line_ch(output_line, '%');
        mmgr_line_put(output_line, " of the fine points");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  each miss across its two zeros: the clock runs back ");
        mmgr_line_decimal_signed(output_line, (long long)back);
        mmgr_line_put(output_line, ", spins past ");
        mmgr_line_fixed(output_line, (double)(rate), 2u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " theta' ");
        mmgr_line_decimal_signed(output_line, (long long)spin);
        mmgr_line_put(output_line, ", neither ");
        mmgr_line_decimal_signed(output_line, (long long)neither);
        mmgr_line_put(output_line, " (the max drag among them ");
        mmgr_line_fixed(output_line, (double)(neither_max), 3u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " theta')");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_decimal_signed(output_line, (long long)failed);
        mmgr_line_put(output_line, " host checks failed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    free(pulses.values);
    return (failed == 0) ? 0 : 1;
}

/* ---------------------------------------------------------------- the sources */

/** @brief The bands of delta theta' and the harmonic cuts of the source mode. */
#define SOURCE_BAND_COUNT 5
static const double SOURCE_BANDS[SOURCE_BAND_COUNT] = {0.25, 0.5, 1.0, 2.0, 4.0};
#define SOURCE_HARMONIC_COUNT 7
static const int64_t SOURCE_HARMONICS[SOURCE_HARMONIC_COUNT] = {2, 3, 4, 6, 10, 20, 40};

/**
 * @brief |F_k(t)|, F cut to its first k harmonics, the sum over n to k of n^(-1/2) exp(-i t ln n).
 *
 * @param[in] time      t.
 * @param[in] harmonics k.
 * @return              The size.
 */
static double harmonic_size(double time, int64_t harmonics)
{
    Sum re = {0.0, 0.0};
    Sum im = {0.0, 0.0};
    for (int64_t term = 1; term <= harmonics; term += 1)
    {
        sum_add(&re, cos(time * log((double)term)) / sqrt((double)term));
    }
    for (int64_t term = 1; term <= harmonics; term += 1)
    {
        sum_add(&im, sin(time * log((double)term)) / sqrt((double)term));
    }
    return py_hypot(sum_value(&re), sum_value(&im));
}

/** @brief Lambda(n): ln p where n is a power of the prime p, else 0. */
static double mangoldt(int64_t value)
{
    for (int64_t prime = 2; prime <= value; prime += 1)
    {
        if ((value % prime) == 0)
        {
            int64_t rest = value;
            while ((rest % prime) == 0)
            {
                rest /= prime;
            }
            return (rest == 1) ? log((double)prime) : 0.0;
        }
    }
    return 0.0;
}

/**
 * @brief The density of the sources, read from F'/F cut at k.
 *
 * @param[in] time      t.
 * @param[in] harmonics k.
 * @return              The density.
 * @note The density is -(sum over n to k of Lambda(n) n^(-1/2) cos(t ln n)).
 * @note These are the lines that log F shares with log zeta for every n to N.
 */
static double source_density(double time, int64_t harmonics)
{
    Sum total = {0.0, 0.0};
    for (int64_t term = 2; term <= harmonics; term += 1)
    {
        sum_add(&total, (mangoldt(term) * cos(time * log((double)term))) / sqrt((double)term));
    }
    return -sum_value(&total);
}

/** @brief A source of a pulse: its t, its delta, and delta theta'. */
typedef struct
{
    double time;        /**< Gamma. */
    double delta;       /**< delta, with its side. */
    double clock_delta; /**< |delta| theta'. */
} Source;

/** @brief Whether a time falls in the fifth of a measure at a cut. */
static int in_fifth(double time, int64_t harmonics, double cut, int lowest)
{
    return lowest ? (harmonic_size(time, harmonics) <= cut) : (source_density(time, harmonics) >= cut);
}

/**
 * @brief The share of the dips and of the sources past the clock in a fifth of a measure, one row a cut.
 *
 * @param[in] sample The times of the sample [BORROWS].
 * @param[in] dips   The times of the dips [BORROWS].
 * @param[in] near   The times of the sources past the clock [BORROWS].
 * @param[in] lowest 1 for the lowest fifth of |F_k|, 0 for the highest fifth of the density.
 */
static void fifth_rows(const Doubles *sample, const Doubles *dips, const Doubles *near, int lowest)
{
    for (int harmonic = 0; harmonic < SOURCE_HARMONIC_COUNT; harmonic += 1)
    {
        const int64_t harmonics = SOURCE_HARMONICS[harmonic];
        Doubles measured = {NULL, 0u, 0u};
        for (size_t at = 0u; at < sample->count; at += 1u)
        {
            doubles_push(&measured, lowest ? harmonic_size(sample->values[at], harmonics)
                                           : source_density(sample->values[at], harmonics));
        }
        const size_t place_at = lowest ? (sample->count / 5u) : ((4u * sample->count) / 5u);
        const double cut = sorted_at(measured.values, measured.count, place_at);
        free(measured.values);
        int64_t hit_dips = 0;
        for (size_t at = 0u; at < dips->count; at += 1u)
        {
            hit_dips += in_fifth(dips->values[at], harmonics, cut, lowest) ? 1 : 0;
        }
        int64_t hit_near = 0;
        for (size_t at = 0u; at < near->count; at += 1u)
        {
            hit_near += in_fifth(near->values[at], harmonics, cut, lowest) ? 1 : 0;
        }
        const double dip_count = (double)((dips->count > 0u) ? dips->count : 1u);
        const double near_count = (double)((near->count > 0u) ? near->count : 1u);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            {
                char linea_piece[24u];
                VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                const int64_t linea_value = (int64_t)((long long)harmonics);
                mmgr_line_decimal_signed(&linea_piece_line, linea_value);
                (void)mmgr_line_finish(&linea_piece_line);
                mmgr_line_put_columns(output_line, linea_piece, 10u);
            }
            mmgr_line_put(output_line, " ");
            mmgr_line_fixed(output_line, (double)(cut), 4u, 16u, EMBED_FALSE);
            mmgr_line_put(output_line, " ");
            {
                char linea_piece[24u];
                VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                const int64_t linea_value = (int64_t)((long long)hit_dips);
                mmgr_line_decimal_signed(&linea_piece_line, linea_value);
                (void)mmgr_line_finish(&linea_piece_line);
                mmgr_line_put_right(output_line, linea_piece, 14u);
            }
            mmgr_line_put(output_line, " (");
            mmgr_line_fixed(output_line, (double)((100.0 * (double)hit_dips) / dip_count), 1u, 5u, EMBED_FALSE);
            mmgr_line_ch(output_line, '%');
            mmgr_line_put(output_line, ") ");
            {
                char linea_piece[24u];
                VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                const int64_t linea_value = (int64_t)((long long)hit_near);
                mmgr_line_decimal_signed(&linea_piece_line, linea_value);
                (void)mmgr_line_finish(&linea_piece_line);
                mmgr_line_put_right(output_line, linea_piece, 16u);
            }
            mmgr_line_put(output_line, " (");
            mmgr_line_fixed(output_line, (double)((100.0 * (double)hit_near) / near_count), 1u, 5u, EMBED_FALSE);
            mmgr_line_ch(output_line, '%');
            mmgr_line_put(output_line, ")");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
}

/**
 * @brief Each local minimum of |F| as a source, a zero of F at t* = gamma + i delta.
 *
 * @param[in] binary The device program [BORROWS].
 * @param[in] base   The first cell.
 * @param[in] count  The cells.
 * @param[in] rate   The points a zero.
 * @return           0 where every host check passes, else 1.
 * @note Gamma is where |F| is least, and |delta| = |F| / |F'|, with its side the sign of the twist there. The code
 *       reads the sources a zero of Z by delta theta', and their gammas, locked on the beats ln(n / m).
 * @note It also cuts F to its first k harmonics. It reads the dents of that cut against the dips of the misses and the
 *       sources past the clock. That is the share of each in the lowest fifth of |F_k|, a fifth where the harmonics
 * place no source.
 */
static int main_source(const char *binary, int64_t base, int64_t count, double rate)
{
    TuringConstants constants;
    turing_constants(&constants);
    Source *sources = NULL;
    size_t source_count = 0u;
    size_t source_capacity = 0u;
    Doubles dips = {NULL, 0u, 0u};
    Doubles sample = {NULL, 0u, 0u};
    int64_t zeros_total = 0;
    int64_t failed = 0;
    static const int64_t UNIFORM[1] = {1};
    for (int64_t at = base; at < (base + count); at += 1)
    {
        const double rise = turing_rises(at);
        const int64_t fine_p = fine_bits(rate, rise);
        const int64_t step = coarse_step(fine_p, rate, rise);
        Listing fine;
        failed += run_listed(binary, &constants, at, fine_p, 1, &fine);
        // The count of zeros fits a machine integer.
        zeros_total += (int64_t)zero_count(fine.points, fine.count);
        double *const squares = squares_of(at, fine.count);
        const double dt = TWO_PI * (squares[1] - squares[0]);
        for (size_t lane = 1u; (lane + 1u) < fine.count; lane += 1u)
        {
            const Complex w = fine.points[lane].w;
            if (!minimum_at(fine.points, lane))
            {
                continue;
            }
            const double clock = 0.5 * log(squares[lane]);
            const Complex f_rel =
                (c_abs(w) != 0.0) ? c_div(derivative_at(fine.points, lane, dt, clock), w) : cx(0.0, 0.0);
            if (c_zero(f_rel))
            {
                continue;
            }
            const double delta = copysign(1.0 / c_abs(f_rel), f_rel.im);
            if (source_count >= source_capacity)
            {
                source_capacity = (source_capacity == 0u) ? 256u : (source_capacity * 2u);
                sources = (Source *)map_memory(realloc(sources, source_capacity * sizeof(Source)));
            }
            sources[source_count].time = TWO_PI * squares[lane];
            sources[source_count].delta = delta;
            sources[source_count].clock_delta = fabs(delta) * clock;
            source_count += 1u;
        }
        for (size_t lane = 0u; lane < fine.count; lane += 64u)
        {
            doubles_push(&sample, TWO_PI * squares[lane]);
        }
        Places idx = {NULL, 0u, 0u};
        comb_indices(fine.count, step, UNIFORM, 1u, &idx);
        Misses found;
        misses_on(&idx, fine.points, fine.count, &found);
        for (size_t miss = 0u; miss < found.count; miss += 1u)
        {
            doubles_push(&dips, TWO_PI * squares[dip_of(fine.points, &found.values[miss])]);
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  cell ");
            mmgr_line_decimal_signed(output_line, (long long)at);
            mmgr_line_put(output_line, " done, ");
            mmgr_line_decimal(output_line, (unsigned long long)source_count, 1u);
            mmgr_line_put(output_line, " sources so far");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        free(found.values);
        free(idx.values);
        free(squares);
        free(fine.points);
    }
    const double zeros = (double)((zeros_total > 1) ? zeros_total : 1);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  zeros of Z ");
        mmgr_line_decimal_signed(output_line, (long long)zeros_total);
        mmgr_line_put(output_line, ", sources ");
        mmgr_line_decimal(output_line, (unsigned long long)source_count, 1u);
        mmgr_line_put(output_line, ", ");
        mmgr_line_fixed(output_line, (double)((double)source_count / zeros), 4u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " a zero; Langer's count for a sum to ln N, one source each two zeros");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put_columns(output_line, "delta theta' under", 26u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "sources", 10u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "a zero", 10u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "back share", 10u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    for (int band = 0; band < SOURCE_BAND_COUNT; band += 1)
    {
        size_t inside = 0u;
        size_t back = 0u;
        for (size_t at = 0u; at < source_count; at += 1u)
        {
            if (sources[at].clock_delta < SOURCE_BANDS[band])
            {
                inside += 1u;
                back += (sources[at].delta < 0.0) ? 1u : 0u;
            }
        }
        char label[32];
        py_repr(SOURCE_BANDS[band], 1, label, sizeof(label));
        const double inside_count = (double)((inside > 0u) ? inside : 1u);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put_columns(output_line, label, 26u);
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal_columns(output_line, (unsigned long long)inside, 10u);
            mmgr_line_put(output_line, " ");
            mmgr_line_fixed(output_line, (double)((double)inside / zeros), 4u, 10u, EMBED_FALSE);
            mmgr_line_put(output_line, " ");
            mmgr_line_fixed(output_line, (double)((100.0 * (double)back) / inside_count), 1u, 9u, EMBED_FALSE);
            mmgr_line_ch(output_line, '%');
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    Doubles near = {NULL, 0u, 0u};
    for (size_t at = 0u; at < source_count; at += 1u)
    {
        if (sources[at].clock_delta < 1.0)
        {
            doubles_push(&near, sources[at].time);
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the lock of the sources past the clock, ");
        mmgr_line_decimal(output_line, (unsigned long long)near.count, 1u);
        mmgr_line_put(output_line, ", on each beat of |F|^2");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    lock_rows(&near);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put_columns(output_line, "harmonics", 10u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "lowest fifth at", 16u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "misses in it", 22u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "sources past the clock", 24u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    fifth_rows(&sample, &dips, &near, 1);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put_columns(output_line, "lines to", 10u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "highest fifth at", 16u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "misses in it", 22u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "sources past the clock", 24u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    fifth_rows(&sample, &dips, &near, 0);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_decimal_signed(output_line, (long long)failed);
        mmgr_line_put(output_line, " host checks failed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    free(sources);
    free(dips.values);
    free(sample.values);
    free(near.values);
    return (failed == 0) ? 0 : 1;
}

/* ---------------------------------------------------------------- the primes */

/**
 * @brief Lambda(n) for every n to the top, by a sieve: ln p at each power of each prime p, else 0.
 *
 * @param[in] top The top.
 * @return        The values, top + 1 of them [OWNS].
 */
static double *sieve_mangoldt(int64_t top)
{
    // The top is a small count, and a negative top holds no values.
    const size_t size = (size_t)((top > 0) ? top : 0) + 1u;
    double *const out = (double *)map_memory(calloc(size + 1u, sizeof(double)));
    unsigned char *const composite = (unsigned char *)map_memory(calloc(size + 1u, 1u));
    for (size_t prime = 2u; prime < size; prime += 1u)
    {
        if (composite[prime] != 0u)
        {
            continue;
        }
        for (size_t multiple = prime * prime; multiple < size; multiple += prime)
        {
            composite[multiple] = 1u;
        }
        for (size_t power = prime; power < size; power *= prime)
        {
            out[power] = log((double)prime);
        }
    }
    free(composite);
    return out;
}

/** @brief The product of two residues modulo m below 2^63, with no overflow. */
static uint64_t mul_mod(uint64_t left, uint64_t right, uint64_t modulus)
{
    if (modulus <= UINT32_MAX)
    {
        return (left * right) % modulus;
    }
    uint64_t result = 0u;
    uint64_t base = left % modulus;
    uint64_t rest = right;
    while (rest > 0u)
    {
        if ((rest & 1u) != 0u)
        {
            result = (result + base) % modulus;
        }
        base = (base + base) % modulus;
        rest >>= 1u;
    }
    return result;
}

/** @brief A power of a residue modulo m, as pow(a, e, m) of Python takes it. */
static uint64_t pow_mod(uint64_t base, uint64_t exponent, uint64_t modulus)
{
    uint64_t result = 1u % modulus;
    uint64_t square = base % modulus;
    uint64_t rest = exponent;
    while (rest > 0u)
    {
        if ((rest & 1u) != 0u)
        {
            result = mul_mod(result, square, modulus);
        }
        square = mul_mod(square, square, modulus);
        rest >>= 1u;
    }
    return result;
}

/** @brief The verdicts of Proth. */
typedef enum
{
    PROTH_UNREACHED,
    PROTH_PRIME,
    PROTH_COMPOSITE,
    PROTH_INCONCLUSIVE
} ProthVerdict;

/**
 * @brief The certificate of Proth for n where his theorem reaches it, as proth_form and proth_prime of twiddle_proof.
 *
 * @param[in] value n.
 * @return          The verdict.
 * @note n = k 2^m + 1 with k odd and k below 2^m. A witness a with a^((n-1)/2) = -1 proves n prime, by Proth. An a with
 *       a^((n-1)/2) neither 1 nor -1 proves it composite, by Fermat. The code tries the witnesses 2 to 999.
 */
static ProthVerdict proth_verdict(uint64_t value)
{
    if ((value < 3u) || ((value % 2u) == 0u))
    {
        return PROTH_UNREACHED;
    }
    uint64_t odd = value - 1u;
    unsigned power = 0u;
    while ((odd % 2u) == 0u)
    {
        odd /= 2u;
        power += 1u;
    }
    if ((power < 64u) && (odd >= (1ull << power)))
    {
        return PROTH_UNREACHED;
    }
    const uint64_t half = (value - 1u) / 2u;
    for (uint64_t candidate = 2u; candidate < 1000u; candidate += 1u)
    {
        const uint64_t lifted = pow_mod(candidate, half, value);
        if (lifted == (value - 1u))
        {
            return PROTH_PRIME;
        }
        if (lifted != 1u)
        {
            return PROTH_COMPOSITE;
        }
    }
    return PROTH_INCONCLUSIVE;
}

/** @brief The whole square root of n, as math.isqrt of Python takes it. */
static uint64_t whole_root(uint64_t value)
{
    // The value is far below 2^52. The root of the double is near, and the loops correct it.
    uint64_t root = (uint64_t)sqrt((double)value);
    while ((root * root) > value)
    {
        root -= 1u;
    }
    while (((root + 1u) * (root + 1u)) <= value)
    {
        root += 1u;
    }
    return root;
}

/** @brief Each zero of Z on the fine lattice, its t by the chord between the two certified points it lies between. */
static void zeros_located(const MapPoint *fine, size_t count, const double *squares, Doubles *out)
{
    Zeros zeros = {NULL, 0u};
    zeros_of(fine, count, &zeros, NULL);
    for (size_t at = 0u; at < zeros.count; at += 1u)
    {
        const double za = fine[zeros.values[at].before].z;
        const double zb = fine[zeros.values[at].after].z;
        const double ta = TWO_PI * squares[zeros.values[at].before];
        const double tb = TWO_PI * squares[zeros.values[at].after];
        doubles_push(out, (za != zb) ? (ta + (((tb - ta) * za) / (za - zb))) : ((ta + tb) / 2.0));
    }
    free(zeros.values);
}

/**
 * @brief The sources locked on ln n for every n to N, and the primes past N read from the certified zeros.
 *
 * @param[in] binary The device program [BORROWS].
 * @param[in] base   The first cell.
 * @param[in] count  The cells.
 * @param[in] rate   The points a zero.
 * @return           0 where every host check passes, else 1.
 * @note log F has the Dirichlet coefficients of zeta to N. The lock is Lambda(n) n^(-1/2), which is 0 at every n that
 *       is not a prime power. This reads back what F comes from, and checks that the code places the sources right.
 * @note By Landau, the zeros of Z in [T1, T2] sum to -((T2 - T1) / 2 pi) Lambda(x) / sqrt(x) for every x > 1. With a
 *       Hann taper w, D(x) = -(4 pi / (T2 - T1)) sqrt(x) sum of w cos(gamma ln x) reads Lambda(x). Each integer
 *       stands apart while x is under (T2 - T1) / 4 pi.
 * @note The code calls each x that reads past ln 2 / 2 a prime power. The sieve grades the calls, and the witness of
 *       Proth proves every one that his theorem reaches. The code reads the sum of D to x against psi(x).
 */
static int main_primes(const char *binary, int64_t base, int64_t count, double rate)
{
    TuringConstants constants;
    turing_constants(&constants);
    Doubles sources = {NULL, 0u, 0u};
    Doubles gammas = {NULL, 0u, 0u};
    int64_t failed = 0;
    int64_t top_n = 0;
    for (int64_t at = base; at < (base + count); at += 1)
    {
        const double rise = turing_rises(at);
        const int64_t fine_p = fine_bits(rate, rise);
        Listing fine;
        failed += run_listed(binary, &constants, at, fine_p, 1, &fine);
        double *const squares = squares_of(at, fine.count);
        const double dt = TWO_PI * (squares[1] - squares[0]);
        for (size_t lane = 1u; (lane + 1u) < fine.count; lane += 1u)
        {
            const Complex w = fine.points[lane].w;
            if (!minimum_at(fine.points, lane) || (c_abs(w) == 0.0))
            {
                continue;
            }
            const double clock = 0.5 * log(squares[lane]);
            const Complex f_rel = c_div(derivative_at(fine.points, lane, dt, clock), w);
            if (!c_zero(f_rel) && ((clock / c_abs(f_rel)) < 1.0))
            {
                doubles_push(&sources, TWO_PI * squares[lane]);
            }
        }
        zeros_located(fine.points, fine.count, squares, &gammas);
        top_n = (at > top_n) ? at : top_n;
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  cell ");
            mmgr_line_decimal_signed(output_line, (long long)at);
            mmgr_line_put(output_line, " done, ");
            mmgr_line_decimal(output_line, (unsigned long long)sources.count, 1u);
            mmgr_line_put(output_line, " sources and ");
            mmgr_line_decimal(output_line, (unsigned long long)gammas.count, 1u);
            mmgr_line_put(output_line, " zeros so far");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        free(squares);
        free(fine.points);
    }
    double *const lam_n = sieve_mangoldt(top_n);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the wave origins: ");
        mmgr_line_decimal(output_line, (unsigned long long)sources.count, 1u);
        mmgr_line_put(output_line, " sources, locked on ln n for n from 2 to N = ");
        mmgr_line_decimal_signed(output_line, (long long)top_n);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    Doubles power_z = {NULL, 0u, 0u};
    Doubles other_z = {NULL, 0u, 0u};
    Doubles ratio = {NULL, 0u, 0u};
    Doubles power_directions = {NULL, 0u, 0u};
    for (int64_t term = 2; term <= top_n; term += 1)
    {
        const Lock lock = rayleigh(&sources, log((double)term));
        if (lam_n[term] > 0.0)
        {
            doubles_push(&power_z, lock.z);
            doubles_push(&power_directions, lock.direction);
            doubles_push(&ratio, lock.length / (lam_n[term] / sqrt((double)term)));
        }
        else
        {
            doubles_push(&other_z, lock.z);
        }
    }
    double cut = other_z.values[0];
    for (size_t at = 1u; at < other_z.count; at += 1u)
    {
        cut = (other_z.values[at] > cut) ? other_z.values[at] : cut;
    }
    double least = power_z.values[0];
    int64_t locked = 0;
    int64_t opposed = 0;
    for (size_t at = 0u; at < power_z.count; at += 1u)
    {
        least = (power_z.values[at] < least) ? power_z.values[at] : least;
        locked += (power_z.values[at] > cut) ? 1 : 0;
        opposed += (fabs(power_directions.values[at] - 180.0) <= 30.0) ? 1 : 0;
    }
    const size_t power_count = power_z.count;
    const size_t other_count = other_z.count;
    const double power_middle = sorted_place(&power_z, power_count / 2u);
    const double other_middle = sorted_place(&other_z, other_count / 2u);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  prime powers ");
        mmgr_line_decimal(output_line, (unsigned long long)power_count, 1u);
        mmgr_line_put(output_line, ": z least ");
        mmgr_line_fixed(output_line, (double)(least), 1u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, ", median ");
        mmgr_line_fixed(output_line, (double)(power_middle), 1u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, "; others ");
        mmgr_line_decimal(output_line, (unsigned long long)other_count, 1u);
        mmgr_line_put(output_line, ": z max ");
        mmgr_line_fixed(output_line, (double)(cut), 1u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, ", median ");
        mmgr_line_fixed(output_line, (double)(other_middle), 2u, 0u, EMBED_FALSE);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  prime powers locked past every other n: ");
        mmgr_line_decimal_signed(output_line, (long long)locked);
        mmgr_line_put(output_line, " of ");
        mmgr_line_decimal(output_line, (unsigned long long)power_count, 1u);
        mmgr_line_put(output_line, "; at 180 +- 30 degrees: ");
        mmgr_line_decimal_signed(output_line, (long long)opposed);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    doubles_sort(&ratio);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  R over Lambda(n) n^(-1/2) across the prime powers, 10/50/90: ");
        mmgr_line_fixed(output_line, (double)(ratio.values[ratio.count / 10u]), 4u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " ");
        mmgr_line_fixed(output_line, (double)(ratio.values[ratio.count / 2u]), 4u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " ");
        mmgr_line_fixed(output_line, (double)(ratio.values[(9u * ratio.count) / 10u]), 4u, 0u, EMBED_FALSE);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    double t_low = gammas.values[0];
    double t_high = gammas.values[0];
    for (size_t at = 1u; at < gammas.count; at += 1u)
    {
        t_low = (gammas.values[at] < t_low) ? gammas.values[at] : t_low;
        t_high = (gammas.values[at] > t_high) ? gammas.values[at] : t_high;
    }
    const double span = t_high - t_low;
    // The reach is a small positive count.
    const int64_t reach = (int64_t)(span / (4.0 * PI_DOUBLE));
    double *const lam = sieve_mangoldt(reach);
    double *const weights = (double *)map_memory(malloc((gammas.count + 1u) * sizeof(double)));
    for (size_t at = 0u; at < gammas.count; at += 1u)
    {
        weights[at] = pow(sin((PI_DOUBLE * (gammas.values[at] - t_low)) / span), 2.0);
    }
    const double scale = (4.0 * PI_DOUBLE) / span;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the primes past N: ");
        mmgr_line_decimal(output_line, (unsigned long long)gammas.count, 1u);
        mmgr_line_put(output_line, " zeros in [");
        mmgr_line_fixed(output_line, (double)(t_low), 3u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, ", ");
        mmgr_line_fixed(output_line, (double)(t_high), 3u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, "], each integer apart to ");
        mmgr_line_decimal_signed(output_line, (long long)reach);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    // The reach is a small positive count.
    double *const reading = (double *)map_memory(calloc((size_t)((reach > 0) ? reach : 0) + 2u, sizeof(double)));
    for (int64_t integer = 2; integer <= reach; integer += 1)
    {
        const double lx = log((double)integer);
        double total = 0.0;
        for (size_t at = 0u; at < gammas.count; at += 1u)
        {
            total += weights[at] * cos(gammas.values[at] * lx);
        }
        reading[integer] = ((-scale) * sqrt((double)integer)) * total;
        if ((integer % 500) == 0)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    read to ");
            mmgr_line_decimal_signed(output_line, (long long)integer);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    const double threshold = log(2.0) / 2.0;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put_columns(output_line, "x", 14u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "truth", 8u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "called", 8u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "right", 8u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "false", 8u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "missed", 8u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "D / Lambda", 12u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    const int64_t bands[4][2] = {
        {2, top_n}, {top_n + 1, (reach < 1000) ? reach : 1000}, {1001, (reach < 2000) ? reach : 2000}, {2001, reach}};
    for (int band = 0; band < 4; band += 1)
    {
        const int64_t low = bands[band][0];
        const int64_t high = bands[band][1];
        if (low > high)
        {
            continue;
        }
        int64_t truth = 0;
        int64_t called = 0;
        int64_t right = 0;
        Doubles ratios = {NULL, 0u, 0u};
        for (int64_t integer = low; integer <= high; integer += 1)
        {
            const int is_power = lam[integer] > 0.0;
            const int is_called = reading[integer] > threshold;
            truth += is_power ? 1 : 0;
            called += is_called ? 1 : 0;
            right += (is_called && is_power) ? 1 : 0;
            if (is_power)
            {
                doubles_push(&ratios, reading[integer] / lam[integer]);
            }
        }
        char label[48];
        snprintf(label, sizeof(label), "%lld to %lld", (long long)low, (long long)high);
        const double middle = (ratios.count > 0u) ? sorted_place(&ratios, ratios.count / 2u) : 0.0;
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put_columns(output_line, label, 14u);
            mmgr_line_put(output_line, " ");
            {
                char linea_piece[24u];
                VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                const int64_t linea_value = (int64_t)((long long)truth);
                mmgr_line_decimal_signed(&linea_piece_line, linea_value);
                (void)mmgr_line_finish(&linea_piece_line);
                mmgr_line_put_right(output_line, linea_piece, 8u);
            }
            mmgr_line_put(output_line, " ");
            {
                char linea_piece[24u];
                VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                const int64_t linea_value = (int64_t)((long long)called);
                mmgr_line_decimal_signed(&linea_piece_line, linea_value);
                (void)mmgr_line_finish(&linea_piece_line);
                mmgr_line_put_right(output_line, linea_piece, 8u);
            }
            mmgr_line_put(output_line, " ");
            {
                char linea_piece[24u];
                VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                const int64_t linea_value = (int64_t)((long long)right);
                mmgr_line_decimal_signed(&linea_piece_line, linea_value);
                (void)mmgr_line_finish(&linea_piece_line);
                mmgr_line_put_right(output_line, linea_piece, 8u);
            }
            mmgr_line_put(output_line, " ");
            {
                char linea_piece[24u];
                VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                const int64_t linea_value = (int64_t)((long long)(called - right));
                mmgr_line_decimal_signed(&linea_piece_line, linea_value);
                (void)mmgr_line_finish(&linea_piece_line);
                mmgr_line_put_right(output_line, linea_piece, 8u);
            }
            mmgr_line_put(output_line, " ");
            {
                char linea_piece[24u];
                VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                const int64_t linea_value = (int64_t)((long long)(truth - right));
                mmgr_line_decimal_signed(&linea_piece_line, linea_value);
                (void)mmgr_line_finish(&linea_piece_line);
                mmgr_line_put_right(output_line, linea_piece, 8u);
            }
            mmgr_line_put(output_line, " ");
            mmgr_line_fixed(output_line, (double)(middle), 3u, 12u, EMBED_FALSE);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        free(ratios.values);
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  every wrong call: ");
        platform_line_out_ended();
    }
    int first_wrong = 1;
    for (int64_t integer = 2; integer <= reach; integer += 1)
    {
        if ((reading[integer] > threshold) != (lam[integer] > 0.0))
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, first_wrong ? "" : ", ");
                mmgr_line_decimal_signed(output_line, (long long)integer);
                mmgr_line_put(output_line, " (D ");
                mmgr_line_fixed(output_line, (double)(reading[integer]), 3u, 0u, EMBED_FALSE);
                mmgr_line_put(output_line, ", Lambda ");
                mmgr_line_fixed(output_line, (double)(lam[integer]), 3u, 0u, EMBED_FALSE);
                mmgr_line_put(output_line, ")");
                platform_line_out_ended();
            }
            first_wrong = 0;
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    Doubles all_ratio = {NULL, 0u, 0u};
    Doubles rest = {NULL, 0u, 0u};
    for (int64_t integer = 2; integer <= reach; integer += 1)
    {
        if (lam[integer] > 0.0)
        {
            doubles_push(&all_ratio, reading[integer] / lam[integer]);
        }
        else
        {
            doubles_push(&rest, fabs(reading[integer]));
        }
    }
    doubles_sort(&all_ratio);
    doubles_sort(&rest);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  D / Lambda at the prime powers, 1/10/50/90/99: ");
        platform_line_out_ended();
    }
    static const double RATIO_PLACES[5] = {0.01, 0.1, 0.5, 0.9, 0.99};
    for (int at = 0; at < 5; at += 1)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, (at > 0) ? " " : "");
        mmgr_line_fixed(output_line, (double)(all_ratio.values[quantile_at(RATIO_PLACES[at], all_ratio.count)]), 3u, 0u,
                        EMBED_FALSE);
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "; |D| elsewhere, 50/90/99/max: ");
        platform_line_out_ended();
    }
    static const double REST_PLACES[4] = {0.5, 0.9, 0.99, 1.0};
    for (int at = 0; at < 4; at += 1)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, (at > 0) ? " " : "");
        mmgr_line_fixed(output_line, (double)(rest.values[quantile_at(REST_PLACES[at], rest.count)]), 3u, 0u,
                        EMBED_FALSE);
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    int64_t proved_prime = 0;
    int64_t proved_composite = 0;
    int64_t proved_other = 0;
    int64_t disagree = 0;
    for (int64_t integer = top_n + 1; integer <= reach; integer += 1)
    {
        if (reading[integer] <= threshold)
        {
            continue;
        }
        // The calls are positive and far below 2^63.
        const ProthVerdict verdict = proth_verdict((uint64_t)integer);
        if (verdict == PROTH_UNREACHED)
        {
            continue;
        }
        proved_prime += (verdict == PROTH_PRIME) ? 1 : 0;
        proved_composite += (verdict == PROTH_COMPOSITE) ? 1 : 0;
        proved_other += (verdict == PROTH_INCONCLUSIVE) ? 1 : 0;
        int is_prime = lam[integer] > 0.0;
        // The divisors stay below the root of the call, a small count.
        const int64_t root = (int64_t)whole_root((uint64_t)integer);
        for (int64_t divisor = 2; is_prime && (divisor <= root); divisor += 1)
        {
            is_prime = (integer % divisor) != 0;
        }
        if ((verdict == PROTH_PRIME) != is_prime)
        {
            disagree += 1;
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  calls past N that Proth reaches: ");
        mmgr_line_decimal_signed(output_line, (long long)proved_prime);
        mmgr_line_put(output_line, " proved prime, ");
        mmgr_line_decimal_signed(output_line, (long long)proved_composite);
        mmgr_line_put(output_line, " proved composite, ");
        mmgr_line_decimal_signed(output_line, (long long)proved_other);
        mmgr_line_put(output_line, " otherwise; ");
        mmgr_line_decimal_signed(output_line, (long long)disagree);
        mmgr_line_put(output_line, " against the sieve");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put_columns(output_line, "x", 8u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "psi(x)", 12u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "sum of D", 12u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "x", 12u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    static const int64_t PSI_AT[8] = {100, 300, 500, 1000, 1500, 2000, 2500, 3000};
    for (int at = 0; at < 8; at += 1)
    {
        const int64_t integer = PSI_AT[at];
        if (integer > reach)
        {
            continue;
        }
        Sum psi = {0.0, 0.0};
        Sum read = {0.0, 0.0};
        for (int64_t term = 2; term <= integer; term += 1)
        {
            sum_add(&psi, lam[term]);
        }
        for (int64_t term = 2; term <= integer; term += 1)
        {
            sum_add(&read, reading[term]);
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            {
                char linea_piece[24u];
                VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                const int64_t linea_value = (int64_t)((long long)integer);
                mmgr_line_decimal_signed(&linea_piece_line, linea_value);
                (void)mmgr_line_finish(&linea_piece_line);
                mmgr_line_put_columns(output_line, linea_piece, 8u);
            }
            mmgr_line_put(output_line, " ");
            mmgr_line_fixed(output_line, (double)(sum_value(&psi)), 2u, 12u, EMBED_FALSE);
            mmgr_line_put(output_line, " ");
            mmgr_line_fixed(output_line, (double)(sum_value(&read)), 2u, 12u, EMBED_FALSE);
            mmgr_line_put(output_line, " ");
            {
                char linea_piece[24u];
                VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                const int64_t linea_value = (int64_t)((long long)integer);
                mmgr_line_decimal_signed(&linea_piece_line, linea_value);
                (void)mmgr_line_finish(&linea_piece_line);
                mmgr_line_put_right(output_line, linea_piece, 12u);
            }
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_decimal_signed(output_line, (long long)failed);
        mmgr_line_put(output_line, " host checks failed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    free(lam_n);
    free(lam);
    free(weights);
    free(reading);
    free(sources.values);
    free(gammas.values);
    free(power_z.values);
    free(other_z.values);
    free(power_directions.values);
    free(ratio.values);
    free(all_ratio.values);
    free(rest.values);
    return (failed == 0) ? 0 : 1;
}

/* ---------------------------------------------------------------- the twist */

/** @brief The reaches of the flags on the sources, and the names of the rules. */
#define TWIST_REACH_COUNT 3
static const double TWIST_REACH[TWIST_REACH_COUNT] = {0.5, 1.0, 2.0};
#define TWIST_RULE_COUNT 8
static const char *const TWIST_RULES[TWIST_RULE_COUNT] = {"reach 0.5",       "reach 1",          "reach 2",
                                                          "Hermite",         "the pole's model", "the pole's own",
                                                          "pole or Hermite", "every rule"};

/** @brief The places of the rules of Hermite and of the model among the rules. */
#define TWIST_HERMITE 3
#define TWIST_MODEL 4

/**
 * @brief Whether the relational model from one end crosses zero inside the step while the ends agree in sign.
 *
 * @param[in] w     w at the end.
 * @param[in] ratio F'/F at the end.
 * @param[in] z_end Z at the end.
 * @param[in] clock theta' at the end.
 * @param[in] t_end t at the end.
 * @param[in] t_a   t at the start of the step.
 * @param[in] t_b   t at the end of the step.
 * @return          1 where the model crosses zero.
 * @note The model takes F linear through its source: F(t) = F_e (1 + (F'/F)_e (t - t_e)). The carrier turns at
 *       theta', and R stays fixed. Spin, twist, swell and the level R / (2 |F|) each enter against the others.
 */
static int model_turns(Complex w, Complex ratio, double z_end, double clock, double t_end, double t_a, double t_b)
{
    const int samples = 32;
    const double rest = z_end - (2.0 * w.re);
    const int sign = z_end > 0.0;
    for (int sample = 1; sample < samples; sample += 1)
    {
        const double elapsed = (t_a + (((t_b - t_a) * sample) / samples)) - t_end;
        const Complex grown = c_plus_real(1.0, c_scale(ratio, elapsed));
        const Complex value = c_mul(c_mul(w, grown), cx(cos(clock * elapsed), sin(clock * elapsed)));
        if ((((2.0 * value.re) + rest) > 0.0) != sign)
        {
            return 1;
        }
    }
    return 0;
}

/**
 * @brief Whether the cubic through Z and Z' at the two ends of a step changes sign inside it.
 *
 * @param[in] z_start      Z at the start.
 * @param[in] z_finish     Z at the end.
 * @param[in] slope_start  Z' at the start.
 * @param[in] slope_finish Z' at the end.
 * @param[in] width        The step in t.
 * @return                 1 where the cubic changes sign while its ends hold one sign.
 */
static int hermite_turns(double z_start, double z_finish, double slope_start, double slope_finish, double width)
{
    const int samples = 32;
    if ((z_start > 0.0) != (z_finish > 0.0))
    {
        return 0;
    }
    const double lean_start = slope_start * width;
    const double lean_finish = slope_finish * width;
    for (int sample = 1; sample < samples; sample += 1)
    {
        const double share = (double)sample / samples;
        const double square = pow(share, 2.0);
        const double cube = pow(share, 3.0);
        const double value =
            (((((2.0 * cube) - (3.0 * square)) + 1.0) * z_start) + (((cube - (2.0 * square)) + share) * lean_start)) +
            (((-2.0 * cube) + (3.0 * square)) * z_finish) + ((cube - square) * lean_finish);
        if ((value > 0.0) != (z_start > 0.0))
        {
            return 1;
        }
    }
    return 0;
}

/**
 * @brief The stretch of t where a single pole at gamma + i delta passes the clock.
 *
 * @param[in]  source The pole.
 * @param[in]  clock  theta'.
 * @param[in]  rate   The rate.
 * @param[out] low    The start of the stretch [BORROWS].
 * @param[out] high   Its end [BORROWS].
 * @return            1 where the stretch exists.
 * @note Its twist is delta / (u^2 + delta^2), u = t - gamma. It falls below -theta' where delta < 0 and
 *       u^2 < |delta| / theta' - delta^2, the clock back. It passes (rate - 1) theta' where delta > 0 and
 *       u^2 < delta / ((rate - 1) theta') - delta^2, the clock a step of pi.
 */
static int pole_passes(Complex source, double clock, double rate, double *low, double *high)
{
    const double delta = source.im;
    double room = -1.0;
    if (delta < 0.0)
    {
        room = (-delta / clock) - (delta * delta);
    }
    else if (rate > 1.0)
    {
        room = (delta / ((rate - 1.0) * clock)) - (delta * delta);
    }
    if (room <= 0.0)
    {
        return 0;
    }
    const double half = sqrt(room);
    *low = source.re - half;
    *high = source.re + half;
    return 1;
}

/** @brief The cross product of the second less the first and the third less the first. */
static double side_of(Complex first, Complex second, Complex third)
{
    return ((second.re - first.re) * (third.im - first.im)) - ((second.im - first.im) * (third.re - first.re));
}

/**
 * @brief The times a polyline crosses itself: each pair of its segments, not neighbors, that intersect.
 *
 * @param[in] path  The points [BORROWS].
 * @param[in] count How many.
 * @return          The count.
 */
static int64_t crossings_of(const Complex *path, size_t count)
{
    int64_t crossings = 0;
    for (size_t first = 0u; (first + 1u) < count; first += 1u)
    {
        for (size_t second = first + 2u; (second + 1u) < count; second += 1u)
        {
            const Complex start = path[first];
            const Complex finish = path[first + 1u];
            const Complex other_start = path[second];
            const Complex other_finish = path[second + 1u];
            if (((side_of(start, finish, other_start) > 0.0) != (side_of(start, finish, other_finish) > 0.0)) &&
                ((side_of(other_start, other_finish, start) > 0.0) !=
                 (side_of(other_start, other_finish, finish) > 0.0)))
            {
                crossings += 1;
            }
        }
    }
    return crossings;
}

/** @brief theta(t) = (t / 2) ln(t / 2 pi) - t / 2 - pi / 8 + 1 / (48 t), in floating point. */
static double theta_at(double time)
{
    return ((((0.5 * time) * log(time / TWO_PI)) - (0.5 * time)) - (PI_DOUBLE / 8.0)) + (1.0 / (48.0 * time));
}

/** @brief The figure of a window: the crossings in w, in F and in the portrait, and the runs of the signs of L. */
typedef struct
{
    int64_t in_w;        /**< The crossings of the track of w. */
    int64_t in_f;        /**< The crossings of the track of F. */
    char *signs;         /**< The signs of L, one a run [OWNS]. */
    int64_t in_portrait; /**< The crossings of (Z, Z' / theta'). */
} Shape;

/** @brief A growable list of shapes. */
typedef struct
{
    Shape *values;   /**< The shapes [OWNS]. */
    size_t count;    /**< How many. */
    size_t capacity; /**< How many the array holds. */
} Shapes;

/** @brief Z' = 2 Re(w (i theta' + F'/F)) at a point, the slope of the remainder left out. */
static double slope_at(const MapPoint *point, double clock)
{
    return 2.0 * c_mul(point->w, cx(point->ratio.re, point->ratio.im + clock)).re;
}

/**
 * @brief The figure over a window of fine points.
 *
 * @param[in]  fine    The fine points, with F'/F [BORROWS].
 * @param[in]  squares x^2 at each point [BORROWS].
 * @param[in]  low     The first point.
 * @param[in]  high    The last point.
 * @param[out] shapes  The list that takes the figure [BORROWS].
 * @note It reads the crossings of the track of w, the plane of the walker. It reads those of the track of F, the frame
 *       that turns with the carrier. It reads the signs that the angular momentum L = Im(conj(w) w') runs through, one
 * a run. It reads the crossings of the phase portrait (Z, Z' / theta').
 */
static void figure_of(const MapPoint *fine, const double *squares, size_t low, size_t high, Shapes *shapes)
{
    const size_t count = (high - low) + 1u;
    Complex *const portrait = (Complex *)map_memory(malloc((count + 1u) * sizeof(Complex)));
    Complex *const track = (Complex *)map_memory(malloc((count + 1u) * sizeof(Complex)));
    Complex *const frame = (Complex *)map_memory(malloc((count + 1u) * sizeof(Complex)));
    for (size_t lane = low; lane <= high; lane += 1u)
    {
        const double clock = 0.5 * log(squares[lane]);
        portrait[lane - low] = cx(fine[lane].z, slope_at(&fine[lane], clock) / clock);
        track[lane - low] = fine[lane].w;
        const double phase = -theta_at(TWO_PI * squares[lane]);
        frame[lane - low] = c_mul(fine[lane].w, cx(cos(phase), sin(phase)));
    }
    char *const signs = (char *)map_memory(malloc(count + 2u));
    size_t sign_count = 0u;
    for (size_t at = 0u; (at + 1u) < count; at += 1u)
    {
        const Complex conjugate = cx(track[at].re, -track[at].im);
        const double moment = c_mul(conjugate, c_sub(track[at + 1u], track[at])).im;
        const char sign = (moment > 0.0) ? '+' : '-';
        if ((moment != 0.0) && ((sign_count == 0u) || (signs[sign_count - 1u] != sign)))
        {
            signs[sign_count] = sign;
            sign_count += 1u;
        }
    }
    signs[sign_count] = '\0';
    if (shapes->count >= shapes->capacity)
    {
        shapes->capacity = (shapes->capacity == 0u) ? 64u : (shapes->capacity * 2u);
        shapes->values = (Shape *)map_memory(realloc(shapes->values, shapes->capacity * sizeof(Shape)));
    }
    Shape *const shape = &shapes->values[shapes->count];
    shape->in_w = crossings_of(track, count);
    shape->in_f = crossings_of(frame, count);
    shape->signs = signs;
    shape->in_portrait = crossings_of(portrait, count);
    shapes->count += 1u;
    free(portrait);
    free(track);
    free(frame);
}

/** @brief Frees the signs and the array of a list of shapes. */
static void shapes_free(Shapes *shapes)
{
    for (size_t at = 0u; at < shapes->count; at += 1u)
    {
        free(shapes->values[at].signs);
    }
    free(shapes->values);
}

/** @brief The slip at the dip of a miss, and what goes with it. */
typedef struct
{
    double depth;  /**< |Z| / (2 |F|). */
    double cosine; /**< cos(phi). */
    double drag;   /**< The drag. */
    int64_t apart; /**< The zeros apart in fine steps. */
    double time;   /**< t at the dip. */
    double level;  /**< |R| / (2 |F|). */
} Slip;

/** @brief The order of two doubles, -1, 0 or 1. */
static int compare_doubles(double first, double second)
{
    return (first < second) ? -1 : ((first > second) ? 1 : 0);
}

/** @brief The order of two slips, field by field, as Python orders tuples. */
static int slip_order(const void *left, const void *right)
{
    const Slip *const first = (const Slip *)left;
    const Slip *const second = (const Slip *)right;
    int order = compare_doubles(first->depth, second->depth);
    order = (order != 0) ? order : compare_doubles(first->cosine, second->cosine);
    order = (order != 0) ? order : compare_doubles(first->drag, second->drag);
    if ((order == 0) && (first->apart != second->apart))
    {
        order = (first->apart < second->apart) ? -1 : 1;
    }
    order = (order != 0) ? order : compare_doubles(first->time, second->time);
    return (order != 0) ? order : compare_doubles(first->level, second->level);
}

/** @brief A growable list of complex numbers. */
typedef struct
{
    Complex *values; /**< The numbers [OWNS]. */
    size_t count;    /**< How many. */
    size_t capacity; /**< How many the array holds. */
} Complexes;

/**
 * @brief Adds a complex to the end of a list.
 *
 * @param[in,out] list  The list [BORROWS].
 * @param[in]     value The number.
 */
static void complexes_push(Complexes *list, Complex value)
{
    if (list->count >= list->capacity)
    {
        list->capacity = (list->capacity == 0u) ? 64u : (list->capacity * 2u);
        list->values = (Complex *)map_memory(realloc(list->values, list->capacity * sizeof(Complex)));
    }
    list->values[list->count] = value;
    list->count += 1u;
}

/** @brief The coupling zeta = 1 + (F'/F) / (i theta') at a fine point. */
static Complex coupling_at(const MapPoint *fine, const double *squares, size_t lane)
{
    return c_plus_real(1.0, c_div(fine[lane].ratio, cx(0.0, 0.5 * log(squares[lane]))));
}

/** @brief The distance of a coupling from 1, as abs(zeta - 1) of Python takes it. */
static double off_one(Complex value)
{
    return c_abs(cx(value.re - 1.0, value.im));
}

/** @brief The coupling farthest from 1 over a stretch of fine points, the first of equals. */
static Complex coupling_farthest(const MapPoint *fine, const double *squares, size_t low, size_t high)
{
    Complex best = coupling_at(fine, squares, low);
    double distance = off_one(best);
    for (size_t lane = low + 1u; lane <= high; lane += 1u)
    {
        const Complex value = coupling_at(fine, squares, lane);
        const double here = off_one(value);
        if (here > distance)
        {
            best = value;
            distance = here;
        }
    }
    return best;
}

/** @brief A source placed by the step of Newton from a fine point, t - F/F'. */
static Complex newton_source(const MapPoint *fine, const double *squares, size_t lane)
{
    return c_real_less(TWO_PI * squares[lane], c_div(cx(1.0, 0.0), fine[lane].ratio));
}

/**
 * @brief The rows of the figures of a set of windows: the crossings, and the four usual runs of the signs of L.
 *
 * @param[in] label The label [BORROWS].
 * @param[in] rows  The figures [BORROWS].
 */
static void shape_rows(const char *label, const Shapes *rows)
{
    const double count = (double)((rows->count > 0u) ? rows->count : 1u);
    size_t in_w = 0u;
    size_t in_f = 0u;
    size_t in_portrait = 0u;
    const char **const keys = (const char **)map_memory(malloc((rows->count + 1u) * sizeof(char *)));
    int64_t *const tallies = (int64_t *)map_memory(malloc((rows->count + 1u) * sizeof(int64_t)));
    size_t key_count = 0u;
    for (size_t at = 0u; at < rows->count; at += 1u)
    {
        const Shape *const shape = &rows->values[at];
        in_w += (shape->in_w > 0) ? 1u : 0u;
        in_f += (shape->in_f > 0) ? 1u : 0u;
        in_portrait += (shape->in_portrait > 0) ? 1u : 0u;
        size_t key = 0u;
        while ((key < key_count) && (strcmp(keys[key], shape->signs) != 0))
        {
            key += 1u;
        }
        if (key == key_count)
        {
            keys[key_count] = shape->signs;
            tallies[key_count] = 0;
            key_count += 1u;
        }
        tallies[key] += 1;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the figure, ");
        mmgr_line_put(output_line, label);
        mmgr_line_put(output_line, " (");
        mmgr_line_decimal(output_line, (unsigned long long)rows->count, 1u);
        mmgr_line_put(output_line, "): the track crosses itself, in w ");
        mmgr_line_fixed(output_line, (double)((100.0 * (double)in_w) / count), 1u, 0u, EMBED_FALSE);
        mmgr_line_ch(output_line, '%');
        mmgr_line_put(output_line, ", in F ");
        mmgr_line_fixed(output_line, (double)((100.0 * (double)in_f) / count), 1u, 0u, EMBED_FALSE);
        mmgr_line_ch(output_line, '%');
        mmgr_line_put(output_line, ", in (Z, Z' / theta') ");
        mmgr_line_fixed(output_line, (double)((100.0 * (double)in_portrait) / count), 1u, 0u, EMBED_FALSE);
        mmgr_line_ch(output_line, '%');
        mmgr_line_put(output_line, "; L's signs, the usual: ");
        platform_line_out_ended();
    }
    // The usual: the four largest tallies, ties in the order of first sight, as the stable sort of Python leaves them.
    unsigned char *const shown = (unsigned char *)map_memory(calloc(key_count + 1u, 1u));
    for (size_t rank = 0u; (rank < 4u) && (rank < key_count); rank += 1u)
    {
        size_t best = key_count;
        for (size_t key = 0u; key < key_count; key += 1u)
        {
            if ((shown[key] == 0u) && ((best == key_count) || (tallies[key] > tallies[best])))
            {
                best = key;
            }
        }
        shown[best] = 1u;
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, (rank > 0u) ? ", " : "");
            mmgr_line_put(output_line, keys[best]);
            mmgr_line_put(output_line, " ");
            mmgr_line_fixed(output_line, (double)((100.0 * (double)tallies[best]) / count), 1u, 0u, EMBED_FALSE);
            mmgr_line_ch(output_line, '%');
            platform_line_out_ended();
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    free(shown);
    free(keys);
    free(tallies);
}

/** @brief A pass of the half twist: |delta| theta', and the turn over 2 atan(3) sign(delta). */
typedef struct
{
    double depth; /**< |delta| theta'. */
    double turn;  /**< The turn over 2 atan(3) sign(delta). */
} Half;

/** @brief The least and the most of a run of values, the first of each. */
typedef struct
{
    double least; /**< The least. */
    double most;  /**< The most. */
} Range;

/** @brief Widens a range by a value, or starts it where the value is the first. */
static void range_take(Range *range, double value, int first)
{
    range->least = (first || (value < range->least)) ? value : range->least;
    range->most = (first || (value > range->most)) ? value : range->most;
}

/**
 * @brief A miss that the flag of Hermite leaves, read across its two zeros.
 *
 * @param[in] fine    The fine points, with F'/F [BORROWS].
 * @param[in] count   How many.
 * @param[in] squares x^2 at each point [BORROWS].
 * @param[in] idx     The coarse lattice [BORROWS].
 * @param[in] miss    The miss [BORROWS].
 * @note It gives R = Z - 2 Re w, and the level R / (2 |F|) that the cosine of the phase must reach. It also gives the
 *       drag and the swell over theta', the length of the step in zeros, and the coupling.
 */
static void hermite_left(const MapPoint *fine, size_t count, const double *squares, const Places *idx, const Miss *miss)
{
    const size_t low = miss->first.before;
    const size_t high = miss->second.after;
    Complex least = coupling_at(fine, squares, low);
    for (size_t lane = low + 1u; lane <= high; lane += 1u)
    {
        const Complex value = coupling_at(fine, squares, lane);
        if (value.re < least.re)
        {
            least = value;
        }
    }
    const Complex farthest = coupling_farthest(fine, squares, low, high);
    char least_text[160];
    char farthest_text[160];
    complex_repr(least, least_text, sizeof(least_text));
    complex_repr(farthest, farthest_text, sizeof(farthest_text));
    // The places are below 2^53.
    const int64_t width = (int64_t)idx->values[miss->finish] - (int64_t)idx->values[miss->start];
    const int64_t apart = (int64_t)miss->second.before - (int64_t)miss->first.after;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "    Hermite leaves: steps ");
        mmgr_line_decimal_signed(output_line, (long long)(miss->finish - miss->start));
        mmgr_line_put(output_line, ", zeros apart ");
        mmgr_line_fixed(output_line, (double)((double)apart / (double)((width > 1) ? width : 1)), 3u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " of a step, zeta from ");
        mmgr_line_put(output_line, least_text);
        mmgr_line_put(output_line, " to ");
        mmgr_line_put(output_line, farthest_text);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    Range sizes = {0.0, 0.0};
    Range levels = {0.0, 0.0};
    Range cosines = {0.0, 0.0};
    Range drags = {0.0, 0.0};
    Range swells = {0.0, 0.0};
    for (size_t lane = low; lane <= high; lane += 1u)
    {
        const double size = c_abs(fine[lane].w);
        const Ripple ripple = ripple_at(fine, count, squares, lane);
        const int first = lane == low;
        range_take(&sizes, size, first);
        range_take(&levels, (fine[lane].z - (2.0 * fine[lane].w.re)) / (2.0 * size), first);
        range_take(&cosines, fine[lane].w.re / size, first);
        range_take(&drags, ripple.drag, first);
        range_take(&swells, ripple.swell, first);
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "    left: t ");
        mmgr_line_fixed(output_line, (double)(TWO_PI * squares[low]), 4u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " to ");
        mmgr_line_fixed(output_line, (double)(TWO_PI * squares[high]), 4u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, ", |F| ");
        mmgr_line_fixed(output_line, (double)(sizes.least), 4u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " to ");
        mmgr_line_fixed(output_line, (double)(sizes.most), 4u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, ", R / (2 |F|) ");
        mmgr_line_fixed(output_line, (double)(levels.least), 4u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " to ");
        mmgr_line_fixed(output_line, (double)(levels.most), 4u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, ", cos phi ");
        mmgr_line_fixed(output_line, (double)(cosines.least), 4u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " to ");
        mmgr_line_fixed(output_line, (double)(cosines.most), 4u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, ", drag ");
        mmgr_line_fixed(output_line, (double)(drags.least), 3u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " to ");
        mmgr_line_fixed(output_line, (double)(drags.most), 3u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, ", swell ");
        mmgr_line_fixed(output_line, (double)(swells.least), 3u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " to ");
        mmgr_line_fixed(output_line, (double)(swells.most), 3u, 0u, EMBED_FALSE);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
}

/**
 * @brief The share of the coupling values past each mark, one row.
 *
 * @param[in] label  The label [BORROWS].
 * @param[in] values The couplings [BORROWS].
 * @param[in] rate   The rate.
 */
static void coupling_row(const char *label, const Complexes *values, double rate)
{
    Doubles far = {NULL, 0u, 0u};
    size_t negative = 0u;
    size_t large = 0u;
    size_t turned = 0u;
    for (size_t at = 0u; at < values->count; at += 1u)
    {
        const Complex value = values->values[at];
        doubles_push(&far, off_one(value));
        negative += (value.re < 0.0) ? 1u : 0u;
        large += (c_abs(value) > rate) ? 1u : 0u;
        turned += (fabs(degrees(atan2(value.im, value.re))) > 60.0) ? 1u : 0u;
    }
    char text[128];
    deciles_text(&far, "%.3f", text, sizeof(text));
    const double count = (double)((values->count > 0u) ? values->count : 1u);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the coupling zeta, ");
        mmgr_line_put(output_line, label);
        mmgr_line_put(output_line, ": |zeta - 1| 10/50/90 ");
        mmgr_line_put(output_line, text);
        mmgr_line_put(output_line, "; Re zeta < 0 ");
        mmgr_line_fixed(output_line, (double)((100.0 * (double)negative) / count), 1u, 0u, EMBED_FALSE);
        mmgr_line_ch(output_line, '%');
        mmgr_line_put(output_line, ", |zeta| past ");
        mmgr_line_fixed(output_line, (double)(rate), 2u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " ");
        mmgr_line_fixed(output_line, (double)((100.0 * (double)large) / count), 1u, 0u, EMBED_FALSE);
        mmgr_line_ch(output_line, '%');
        mmgr_line_put(output_line, ", |arg zeta| past 60 degrees ");
        mmgr_line_fixed(output_line, (double)((100.0 * (double)turned) / count), 1u, 0u, EMBED_FALSE);
        mmgr_line_ch(output_line, '%');
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    free(far.values);
}

/** @brief Whether a rule marks any coarse step of a miss. */
static int miss_marked(const Miss *miss, const unsigned char (*marked)[TWIST_RULE_COUNT], int rule)
{
    for (size_t coarse = miss->start; coarse < miss->finish; coarse += 1u)
    {
        if (marked[coarse][rule] != 0u)
        {
            return 1;
        }
    }
    return 0;
}

/** @brief The work lists of the twist mode, over every cell. */
typedef struct
{
    Doubles diffs;                     /**< |F'/F of the device less the differences| / theta'. */
    Doubles courses;                   /**< The course off the tangent at every 97th point. */
    Doubles miss_courses;              /**< The course at the dips. */
    Complexes coupling;                /**< The coupling at every 97th point. */
    Complexes miss_coupling;           /**< The coupling farthest from 1 across each miss. */
    Shapes figures;                    /**< The figures of the misses. */
    Shapes lots;                       /**< The figures of the windows by lot. */
    Half *halves;                      /**< The passes of the half twist [OWNS]. */
    size_t half_count;                 /**< How many. */
    size_t half_capacity;              /**< How many the array holds. */
    Slip *slips;                       /**< The slips [OWNS]. */
    size_t slip_count;                 /**< How many. */
    size_t slip_capacity;              /**< How many the array holds. */
    int64_t caught[TWIST_RULE_COUNT];  /**< The misses each rule catches. */
    int64_t flagged[TWIST_RULE_COUNT]; /**< The steps each rule flags. */
    int64_t misses;                    /**< The misses. */
    int64_t steps;                     /**< The coarse steps. */
} TwistTally;

/**
 * @brief The check of the device against the fine differences, at every 97th point of a cell.
 *
 * @param[in]     fine    The fine points, with F'/F [BORROWS].
 * @param[in]     count   How many.
 * @param[in]     squares x^2 at each point [BORROWS].
 * @param[in]     dt      The fine step in t.
 * @param[in,out] tally   The work lists [BORROWS].
 */
static void twist_check(const MapPoint *fine, size_t count, const double *squares, double dt, TwistTally *tally)
{
    for (size_t lane = 1u; (lane + 1u) < count; lane += 97u)
    {
        const Ripple ripple = ripple_at(fine, count, squares, lane);
        doubles_push(&tally->courses, fabs(degrees(atan2(ripple.swell, ripple.drag))));
        complexes_push(&tally->coupling, coupling_at(fine, squares, lane));
        const Complex before = fine[lane - 1u].w;
        const Complex after = fine[lane + 1u].w;
        if ((c_abs(before) == 0.0) || (c_abs(after) == 0.0))
        {
            continue;
        }
        const Complex ratio = c_div(after, before);
        const double clock = 0.5 * log(squares[lane]);
        const Complex seen =
            c_sub(c_over(cx(log(c_abs(after) / c_abs(before)), c_angle(ratio)), 2.0 * dt), cx(0.0, clock));
        doubles_push(&tally->diffs, c_abs(c_sub(fine[lane].ratio, seen)) / clock);
    }
}

/**
 * @brief The half twist of a cell: at each minimum of |F|, the step of Newton places the source.
 *
 * @param[in]     fine    The fine points, with F'/F [BORROWS].
 * @param[in]     count   How many.
 * @param[in]     squares x^2 at each point [BORROWS].
 * @param[in]     dt      The fine step in t.
 * @param[in,out] tally   The work lists [BORROWS].
 * @note The code reads the twist of the device, summed over u in [-3 |delta|, 3 |delta|], against the
 *       2 atan(3) sign(delta) of the single pass.
 */
static void twist_halves(const MapPoint *fine, size_t count, const double *squares, double dt, TwistTally *tally)
{
    for (size_t lane = 1u; (lane + 1u) < count; lane += 1u)
    {
        if (!minimum_at(fine, lane) || c_zero(fine[lane].ratio))
        {
            continue;
        }
        const Complex source = newton_source(fine, squares, lane);
        const double delta = source.im;
        const double clock = 0.5 * log(squares[lane]);
        // The reach and the center are small counts of fine steps.
        const int64_t reach = (int64_t)ceil((3.0 * fabs(delta)) / dt);
        const int64_t centre = (int64_t)lane + py_round((source.re - (TWO_PI * squares[lane])) / dt);
        if (((fabs(delta) * clock) >= 1.0) || (reach < 4) || ((centre - reach) < 0) ||
            ((centre + reach) >= (int64_t)count))
        {
            continue;
        }
        Sum turn = {0.0, 0.0};
        for (int64_t near_lane = centre - reach; near_lane <= (centre + reach); near_lane += 1)
        {
            sum_add(&turn, fine[near_lane].ratio.im);
        }
        if (tally->half_count >= tally->half_capacity)
        {
            tally->half_capacity = (tally->half_capacity == 0u) ? 64u : (tally->half_capacity * 2u);
            tally->halves = (Half *)map_memory(realloc(tally->halves, tally->half_capacity * sizeof(Half)));
        }
        Half *const half = &tally->halves[tally->half_count];
        half->depth = fabs(delta) * clock;
        half->turn = (sum_value(&turn) * dt) / ((2.0 * atan(3.0)) * copysign(1.0, delta));
        tally->half_count += 1u;
    }
}

/**
 * @brief The slip, the course and the coupling at each miss of a cell.
 *
 * @param[in]     fine    The fine points, with F'/F [BORROWS].
 * @param[in]     count   How many.
 * @param[in]     squares x^2 at each point [BORROWS].
 * @param[in]     found   The misses [BORROWS].
 * @param[in,out] tally   The work lists [BORROWS].
 * @note The slip is how far past the level the ball goes, |Z| / (2 |F|) at the dip. With it go cos(phi) and the drag
 *       there, and the zeros apart in fine steps.
 * @note The coupling zeta = 1 + (F'/F) / (i theta') comes from the device, and least |zeta - 1| is the best grip.
 *       Across the two zeros of the miss, the run takes its value farthest from 1.
 */
static void twist_slips(const MapPoint *fine, size_t count, const double *squares, const Misses *found,
                        TwistTally *tally)
{
    for (size_t miss = 0u; miss < found->count; miss += 1u)
    {
        const Miss *const pair = &found->values[miss];
        const size_t dip = dip_of(fine, pair);
        const Ripple ripple = ripple_at(fine, count, squares, dip);
        doubles_push(&tally->miss_courses, fabs(degrees(atan2(ripple.swell, ripple.drag))));
        if (tally->slip_count >= tally->slip_capacity)
        {
            tally->slip_capacity = (tally->slip_capacity == 0u) ? 64u : (tally->slip_capacity * 2u);
            tally->slips = (Slip *)map_memory(realloc(tally->slips, tally->slip_capacity * sizeof(Slip)));
        }
        const double size = c_abs(fine[dip].w);
        Slip *const slip = &tally->slips[tally->slip_count];
        slip->depth = fabs(fine[dip].z) / (2.0 * size);
        slip->cosine = fine[dip].w.re / size;
        slip->drag = ripple.drag;
        // The lanes are below 2^63.
        slip->apart = (int64_t)pair->second.before - (int64_t)pair->first.after;
        slip->time = TWO_PI * squares[dip];
        slip->level = fabs(fine[dip].z - (2.0 * fine[dip].w.re)) / (2.0 * size);
        tally->slip_count += 1u;
        complexes_push(&tally->miss_coupling, coupling_farthest(fine, squares, pair->first.before, pair->second.after));
    }
}

/**
 * @brief The flags of each rule on each coarse step of a cell.
 *
 * @param[in]     fine    The fine points, with F'/F [BORROWS].
 * @param[in]     squares x^2 at each point [BORROWS].
 * @param[in]     idx     The coarse lattice [BORROWS].
 * @param[in]     rate    The rate.
 * @param[out]    marked  The marks of each step [BORROWS].
 * @param[in,out] tally   The work lists [BORROWS].
 */
static void twist_flags(const MapPoint *fine, const double *squares, const Places *idx, double rate,
                        unsigned char (*marked)[TWIST_RULE_COUNT], TwistTally *tally)
{
    const size_t step_count = (idx->count > 0u) ? (idx->count - 1u) : 0u;
    for (size_t coarse = 0u; coarse < step_count; coarse += 1u)
    {
        tally->steps += 1;
        const size_t ends[2] = {idx->values[coarse], idx->values[coarse + 1u]};
        const double t_a = TWO_PI * squares[ends[0]];
        const double t_b = TWO_PI * squares[ends[1]];
        const double clock = 0.5 * log(squares[ends[0]]);
        Complex places[2];
        size_t place_count = 0u;
        for (int end = 0; end < 2; end += 1)
        {
            if (!c_zero(fine[ends[end]].ratio))
            {
                places[place_count] = newton_source(fine, squares, ends[end]);
                place_count += 1u;
            }
        }
        int hits[TWIST_RULE_COUNT] = {0};
        for (int reach = 0; reach < TWIST_REACH_COUNT; reach += 1)
        {
            for (size_t source = 0u; source < place_count; source += 1u)
            {
                const Complex one = places[source];
                if ((t_a <= one.re) && (one.re <= t_b) && ((fabs(one.im) * clock) < TWIST_REACH[reach]))
                {
                    hits[reach] = 1;
                }
            }
        }
        const double slopes[2] = {slope_at(&fine[ends[0]], 0.5 * log(squares[ends[0]])),
                                  slope_at(&fine[ends[1]], 0.5 * log(squares[ends[1]]))};
        const int hermite = hermite_turns(fine[ends[0]].z, fine[ends[1]].z, slopes[0], slopes[1], t_b - t_a);
        hits[TWIST_HERMITE] = hermite;
        const int same = (fine[ends[0]].z > 0.0) == (fine[ends[1]].z > 0.0);
        int model = 0;
        for (int end = 0; same && !model && (end < 2); end += 1)
        {
            const MapPoint *const point = &fine[ends[end]];
            model = model_turns(point->w, point->ratio, point->z, 0.5 * log(squares[ends[end]]),
                                TWO_PI * squares[ends[end]], t_a, t_b);
        }
        hits[TWIST_MODEL] = same && model;
        int pole = 0;
        for (size_t source = 0u; source < place_count; source += 1u)
        {
            double low = 0.0;
            double high = 0.0;
            if (pole_passes(places[source], clock, rate, &low, &high) && (low <= t_b) && (high >= t_a))
            {
                pole = 1;
            }
        }
        hits[5] = pole;
        hits[6] = pole || hermite;
        hits[7] = pole || hermite || hits[TWIST_MODEL];
        for (int rule = 0; rule < TWIST_RULE_COUNT; rule += 1)
        {
            if (hits[rule])
            {
                tally->flagged[rule] += 1;
                marked[coarse][rule] = 1u;
            }
        }
    }
}

/**
 * @brief One cell of the twist mode, into the work lists.
 *
 * @param[in]     binary    The device program [BORROWS].
 * @param[in]     constants The constants [BORROWS].
 * @param[in]     nu        The cell.
 * @param[in]     rate      The rate.
 * @param[in,out] lot       The draws of the windows [BORROWS].
 * @param[in,out] tally     The work lists [BORROWS].
 * @return                  The failed host checks.
 */
static int64_t twist_cell(const char *binary, const TuringConstants *constants, int64_t nu, double rate,
                          ZetaTwister *lot, TwistTally *tally)
{
    static const int64_t UNIFORM[1] = {1};
    const double rise = turing_rises(nu);
    const int64_t fine_p = fine_bits(rate, rise);
    const int64_t step = coarse_step(fine_p, rate, rise);
    Listing listed;
    const int64_t failed = run_listed(binary, constants, nu, fine_p, 2, &listed);
    const MapPoint *const fine = listed.points;
    const size_t count = listed.count;
    double *const squares = squares_of(nu, count);
    const double dt = TWO_PI * (squares[1] - squares[0]);
    twist_check(fine, count, squares, dt, tally);
    twist_halves(fine, count, squares, dt, tally);
    Places idx = {NULL, 0u, 0u};
    comb_indices(count, step, UNIFORM, 1u, &idx);
    Misses found;
    misses_on(&idx, fine, count, &found);
    twist_slips(fine, count, squares, &found, tally);
    // The count of misses fits a machine integer.
    tally->misses += (int64_t)found.count;
    // The figure: the track of each miss over its steps and one step each side, against windows as long, by lot.
    for (size_t miss = 0u; miss < found.count; miss += 1u)
    {
        const Miss *const pair = &found.values[miss];
        const size_t low = idx.values[(pair->start > 0u) ? (pair->start - 1u) : 0u];
        const size_t last = idx.count - 1u;
        const size_t high = idx.values[((pair->finish + 1u) < last) ? (pair->finish + 1u) : last];
        figure_of(fine, squares, low, high, &tally->figures);
    }
    // The span is a small positive count of fine steps.
    const size_t span = (size_t)(3 * step);
    for (int window = 0; window < 60; window += 1)
    {
        // The width of the lot is far below 2^32.
        const size_t low = (size_t)twister_below(lot, (uint32_t)(count - span - 1u));
        figure_of(fine, squares, low, low + span, &tally->lots);
    }
    unsigned char (*const marked)[TWIST_RULE_COUNT] =
        (unsigned char (*)[TWIST_RULE_COUNT])map_memory(calloc(idx.count + 1u, TWIST_RULE_COUNT));
    twist_flags(fine, squares, &idx, rate, marked, tally);
    for (int rule = 0; rule < TWIST_RULE_COUNT; rule += 1)
    {
        for (size_t miss = 0u; miss < found.count; miss += 1u)
        {
            tally->caught[rule] +=
                miss_marked(&found.values[miss], (const unsigned char (*)[TWIST_RULE_COUNT])marked, rule);
        }
    }
    for (size_t miss = 0u; miss < found.count; miss += 1u)
    {
        if (!miss_marked(&found.values[miss], (const unsigned char (*)[TWIST_RULE_COUNT])marked, TWIST_HERMITE))
        {
            hermite_left(fine, count, squares, &idx, &found.values[miss]);
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  cell ");
        mmgr_line_decimal_signed(output_line, (long long)nu);
        mmgr_line_put(output_line, " done, shift ");
        mmgr_line_decimal_signed(output_line, (long long)listed.shift);
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal_signed(output_line, (long long)tally->misses);
        mmgr_line_put(output_line, " misses so far");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    free(marked);
    free(found.values);
    free(idx.values);
    free(squares);
    free(listed.points);
    return failed;
}

/**
 * @brief The slips by depth: the deciles, the shallowest, and the shallowest and deepest quarters.
 *
 * @param[in,out] tally The work lists [BORROWS]. The function sorts the slips.
 */
static void slip_rows(TwistTally *tally)
{
    Slip *const slips = tally->slips;
    const size_t slip_count = tally->slip_count;
    if (slip_count > 1u)
    {
        qsort(slips, slip_count, sizeof(Slip), slip_order);
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the slip at each miss's dip, |Z| / (2 |F|), 10/50/90: ");
        platform_line_out_ended();
    }
    static const double SLIP_PLACES[3] = {0.1, 0.5, 0.9};
    for (int at = 0; at < 3; at += 1)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, (at > 0) ? " " : "");
        mmgr_line_fixed(output_line, (double)(slips[quantile_at(SLIP_PLACES[at], slip_count)].depth), 4u, 0u,
                        EMBED_FALSE);
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    for (size_t at = 0u; (at < 8u) && (at < slip_count); at += 1u)
    {
        const Slip *const slip = &slips[at];
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    shallowest: t ");
            mmgr_line_fixed(output_line, (double)(slip->time), 4u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, ", slip ");
            mmgr_line_fixed(output_line, (double)(slip->depth), 5u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, ", level ");
            mmgr_line_fixed(output_line, (double)(slip->level), 4u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, ", cos(phi) ");
            mmgr_line_fixed(output_line, (double)(slip->cosine), 4u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, ", drag ");
            mmgr_line_fixed(output_line, (double)(slip->drag), 3u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, ", the zeros ");
            mmgr_line_decimal_signed(output_line, (long long)slip->apart);
            mmgr_line_put(output_line, " fine steps apart");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    // A quarter of no slips takes every slip as the deep quarter, as slips[-0:] of Python does.
    const size_t quarter = slip_count / 4u;
    static const char *const QUARTER_LABELS[2] = {"shallowest quarter", "deepest quarter"};
    for (int which = 0; which < 2; which += 1)
    {
        const Slip *const rows = (which == 0) ? slips : (slips + ((quarter > 0u) ? (slip_count - quarter) : 0u));
        const size_t row_count = (which == 0) ? quarter : ((quarter > 0u) ? quarter : slip_count);
        if (row_count == 0u)
        {
            zeta_stop("the twist has too few slips for its quarters");
        }
        Doubles cosine = {NULL, 0u, 0u};
        Doubles drag = {NULL, 0u, 0u};
        Doubles apart = {NULL, 0u, 0u};
        Doubles level = {NULL, 0u, 0u};
        Doubles over = {NULL, 0u, 0u};
        for (size_t at = 0u; at < row_count; at += 1u)
        {
            doubles_push(&cosine, fabs(rows[at].cosine));
            doubles_push(&drag, fabs(rows[at].drag));
            // The zeros apart are a small count, exact as a double.
            doubles_push(&apart, (double)rows[at].apart);
            doubles_push(&level, rows[at].level);
            doubles_push(&over, (rows[at].level != 0.0) ? (rows[at].depth / rows[at].level) : 0.0);
        }
        const size_t middle = row_count / 2u;
        const double cosine_middle = sorted_place(&cosine, middle);
        const double drag_middle = sorted_place(&drag, middle);
        // The zeros apart are a small whole count.
        const long long apart_middle = (long long)sorted_place(&apart, middle);
        const double level_middle = sorted_place(&level, middle);
        const double over_middle = sorted_place(&over, middle);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, QUARTER_LABELS[which]);
            mmgr_line_put(output_line, ": |cos(phi)| median ");
            mmgr_line_fixed(output_line, (double)(cosine_middle), 4u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, ", |drag| median ");
            mmgr_line_fixed(output_line, (double)(drag_middle), 3u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, ", the zeros apart median ");
            mmgr_line_decimal_signed(output_line, apart_middle);
            mmgr_line_put(output_line, " fine steps, the level |R| / (2 |F|) median ");
            mmgr_line_fixed(output_line, (double)(level_middle), 4u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, ", the slip over the level median ");
            mmgr_line_fixed(output_line, (double)(over_middle), 4u, 0u, EMBED_FALSE);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        free(cosine.values);
        free(drag.values);
        free(apart.values);
        free(level.values);
        free(over.values);
    }
}

/** @brief The passes of the half twist by band of |delta| theta'. */
static void half_rows(const TwistTally *tally)
{
    static const double HALF_BANDS[3][2] = {{0.0, 0.25}, {0.25, 0.5}, {0.5, 1.0}};
    for (int band = 0; band < 3; band += 1)
    {
        Doubles rows = {NULL, 0u, 0u};
        for (size_t at = 0u; at < tally->half_count; at += 1u)
        {
            const double depth = tally->halves[at].depth;
            if ((HALF_BANDS[band][0] <= depth) && (depth < HALF_BANDS[band][1]))
            {
                doubles_push(&rows, tally->halves[at].turn);
            }
        }
        if (rows.count > 0u)
        {
            char text[128];
            deciles_text(&rows, "%.3f", text, sizeof(text));
            size_t positive = 0u;
            for (size_t at = 0u; at < rows.count; at += 1u)
            {
                positive += (rows.values[at] > 0.0) ? 1u : 0u;
            }
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  the half twist, |delta| theta' in [");
                mmgr_line_fixed(output_line, (double)(HALF_BANDS[band][0]), 2u, 0u, EMBED_FALSE);
                mmgr_line_put(output_line, ", ");
                mmgr_line_fixed(output_line, (double)(HALF_BANDS[band][1]), 2u, 0u, EMBED_FALSE);
                mmgr_line_put(output_line, "): ");
                mmgr_line_decimal(output_line, (unsigned long long)rows.count, 1u);
                mmgr_line_put(output_line, " passes, the turn over 2 atan(3) sign(delta), 10/50/90: ");
                mmgr_line_put(output_line, text);
                mmgr_line_put(output_line, "; the sign of delta's ");
                mmgr_line_fixed(output_line, (double)((100.0 * (double)positive) / (double)rows.count), 1u, 0u,
                                EMBED_FALSE);
                mmgr_line_ch(output_line, '%');
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
        }
        free(rows.values);
    }
}

/**
 * @brief Cells base to base + count - 1 on one fine lattice each, listed with the F' of the device.
 *
 * @param[in] binary The device program [BORROWS].
 * @param[in] base   The first cell.
 * @param[in] count  The cells.
 * @param[in] rate   The points a zero.
 * @return           0 where every host check passes, else 1.
 * @note The check: the twist and swell of the device, F'/F at each point, against the own differences of w on the
 *       fine lattice.
 * @note The flag: the step of Newton places the nearest source from each point of the uniform coarse lattice at the
 *       rate. That step is t* = t - F/F', and it reads that point alone. It is exact for a single pole. The code flags
 * a coarse step where a source placed from either end falls inside it, with |Im t*| under reach / theta'. That is a
 * pulse that can pass the clock.
 * @note The run reads the misses in flagged steps, and the share of steps flagged.
 */
static int main_twist(const char *binary, int64_t base, int64_t count, double rate)
{
    TuringConstants constants;
    turing_constants(&constants);
    static TwistTally tally;
    memset(&tally, 0, sizeof(tally));
    int64_t failed = 0;
    ZetaTwister lot;
    twister_seed(&lot, 20261003u);
    for (int64_t nu = base; nu < (base + count); nu += 1)
    {
        failed += twist_cell(binary, &constants, nu, rate, &lot, &tally);
    }
    doubles_sort(&tally.diffs);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(
            output_line,
            "  the device's F'/F against the fine lattice's differences, |difference| / theta', 50/90/99/max: ");
        platform_line_out_ended();
    }
    static const double DIFF_PLACES[4] = {0.5, 0.9, 0.99, 1.0};
    for (int at = 0; at < 4; at += 1)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, (at > 0) ? " " : "");
        mmgr_line_fixed(output_line, (double)(tally.diffs.values[quantile_at(DIFF_PLACES[at], tally.diffs.count)]), 5u,
                        0u, EMBED_FALSE);
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put_columns(output_line, "flag", 16u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "steps flagged", 14u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "misses caught", 16u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    const double step_total = (double)((tally.steps > 1) ? tally.steps : 1);
    const double miss_total = (double)((tally.misses > 1) ? tally.misses : 1);
    for (int rule = 0; rule < TWIST_RULE_COUNT; rule += 1)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put_columns(output_line, TWIST_RULES[rule], 16u);
        mmgr_line_put(output_line, " ");
        mmgr_line_fixed(output_line, (double)((100.0 * (double)tally.flagged[rule]) / step_total), 2u, 13u,
                        EMBED_FALSE);
        mmgr_line_ch(output_line, '%');
        mmgr_line_put(output_line, " ");
        {
            char linea_piece[24u];
            VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
            const int64_t linea_value = (int64_t)((long long)tally.caught[rule]);
            mmgr_line_decimal_signed(&linea_piece_line, linea_value);
            (void)mmgr_line_finish(&linea_piece_line);
            mmgr_line_put_right(output_line, linea_piece, 9u);
        }
        mmgr_line_put(output_line, " (");
        mmgr_line_fixed(output_line, (double)((100.0 * (double)tally.caught[rule]) / miss_total), 1u, 5u, EMBED_FALSE);
        mmgr_line_ch(output_line, '%');
        mmgr_line_put(output_line, ")");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    static const char *const COURSE_LABELS[2] = {"every point", "the misses' dips"};
    Doubles *const course_sets[2] = {&tally.courses, &tally.miss_courses};
    for (int set = 0; set < 2; set += 1)
    {
        Doubles *const values = course_sets[set];
        doubles_sort(values);
        char text[128];
        deciles_text(values, "%.1f", text, sizeof(text));
        size_t past = 0u;
        for (size_t at = 0u; at < values->count; at += 1u)
        {
            past += (values->values[at] > 60.0) ? 1u : 0u;
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  the course off the tangent, |atan(swell / drag)| in degrees, ");
            mmgr_line_put(output_line, COURSE_LABELS[set]);
            mmgr_line_put(output_line, ", 10/50/90: ");
            mmgr_line_put(output_line, text);
            mmgr_line_put(output_line, "; past 60: ");
            mmgr_line_fixed(output_line,
                            (double)((100.0 * (double)past) / (double)((values->count > 0u) ? values->count : 1u)), 1u,
                            0u, EMBED_FALSE);
            mmgr_line_ch(output_line, '%');
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the flagged steps by count: ");
        platform_line_out_ended();
    }
    for (int rule = 0; rule < TWIST_RULE_COUNT; rule += 1)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, (rule > 0) ? ", " : "");
        mmgr_line_put(output_line, TWIST_RULES[rule]);
        mmgr_line_put(output_line, " ");
        mmgr_line_decimal_signed(output_line, (long long)tally.flagged[rule]);
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    coupling_row("every point", &tally.coupling, rate);
    coupling_row("the misses, farthest from 1", &tally.miss_coupling, rate);
    shape_rows("the misses", &tally.figures);
    shape_rows("windows by lot", &tally.lots);
    slip_rows(&tally);
    half_rows(&tally);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  misses ");
        mmgr_line_decimal_signed(output_line, (long long)tally.misses);
        mmgr_line_put(output_line, " over ");
        mmgr_line_decimal_signed(output_line, (long long)tally.steps);
        mmgr_line_put(output_line, " coarse steps; ");
        mmgr_line_decimal_signed(output_line, (long long)failed);
        mmgr_line_put(output_line, " host checks failed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    free(tally.diffs.values);
    free(tally.courses.values);
    free(tally.miss_courses.values);
    free(tally.coupling.values);
    free(tally.miss_coupling.values);
    shapes_free(&tally.figures);
    shapes_free(&tally.lots);
    free(tally.halves);
    free(tally.slips);
    return (failed == 0) ? 0 : 1;
}

/* ---------------------------------------------------------------- the entry */

/** @brief An argument as a whole number, as int() of Python reads it. */
static int64_t whole_of(const char *text)
{
    return (int64_t)strtoll(text, NULL, 10);
}

/** @brief A mode that takes a base, a count of cells and a rate. */
typedef int (*RatedMode)(const char *binary, int64_t base, int64_t count, double rate);

/**
 * @brief Runs the mode that the arguments name.
 *
 * @param[in] argc The count of arguments.
 * @param[in] argv The arguments [BORROWS].
 * @return         0 where every host check passes, else 1, or 2 on a usage error.
 */
int program_main(int argc, char **argv)
{
#ifdef _WIN32
    (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
    if (argc < 2)
    {
        fprintf(stderr, "usage: exact_zeta_miss_map <binary> [first] [last] [coarse] [fine] [folder]\n"
                        "       exact_zeta_miss_map <binary> e <base> <cells> <heights> <rate> [folder]\n"
                        "       exact_zeta_miss_map <binary> carrier <base> <cells> <rate> [folder]\n"
                        "       exact_zeta_miss_map <binary> ripple|pulse|source|primes|twist <base> <cells> <rate>\n");
        return 2;
    }
    const char *const binary = argv[1];
    const char *const mode = (argc > 2) ? argv[2] : "";
    static const char *const RATED[5] = {"twist", "primes", "source", "pulse", "ripple"};
    static const RatedMode RUNS[5] = {main_twist, main_primes, main_source, main_pulse, main_ripple};
    for (int at = 0; at < 5; at += 1)
    {
        if (strcmp(mode, RATED[at]) == 0)
        {
            if (argc < 6)
            {
                fprintf(stderr, "usage: exact_zeta_miss_map <binary> %s <base> <cells> <rate>\n", RATED[at]);
                return 2;
            }
            return RUNS[at](binary, whole_of(argv[3]), whole_of(argv[4]), strtod(argv[5], NULL));
        }
    }
    if (strcmp(mode, "e") == 0)
    {
        if (argc < 7)
        {
            fprintf(stderr, "usage: exact_zeta_miss_map <binary> e <base> <cells> <heights> <rate> [folder]\n");
            return 2;
        }
        return main_folds(binary, whole_of(argv[3]), whole_of(argv[4]), whole_of(argv[5]), strtod(argv[6], NULL),
                          (argc > 7) ? argv[7] : NULL);
    }
    if (strcmp(mode, "carrier") == 0)
    {
        if (argc < 6)
        {
            fprintf(stderr, "usage: exact_zeta_miss_map <binary> carrier <base> <cells> <rate> [folder]\n");
            return 2;
        }
        return main_carrier(binary, whole_of(argv[3]), whole_of(argv[4]), strtod(argv[5], NULL),
                            (argc > 6) ? argv[6] : NULL);
    }
    const int64_t first = (argc > 2) ? whole_of(argv[2]) : 300;
    const int64_t last = (argc > 3) ? whole_of(argv[3]) : 304;
    const int64_t coarse_bits = (argc > 4) ? whole_of(argv[4]) : 15;
    const int64_t fine_p = (argc > 5) ? whole_of(argv[5]) : 18;
    return main_map(binary, first, last, coarse_bits, fine_p, (argc > 6) ? argv[6] : NULL);
}

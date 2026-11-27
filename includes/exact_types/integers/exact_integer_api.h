
// exact_integer_api.h: the exact integer and its operations (exact_integer.h includes the parts in order)
#ifndef EXACT_INTEGER_API_H
#define EXACT_INTEGER_API_H

#include "exact_integer_widths.h"

#ifdef __cplusplus
/* nvcc compiles the GPU arm as C++, and the arm calls these functions, which the C compiler compiles.
 * Without this block, the C++ side looks for mangled names that no C translation unit emits. */
extern "C"
{
#endif

    /**
     * @brief An exact integer: a magnitude in limbs, and a sign that the struct keeps apart from it.
     *
     * @note The least significant limb comes first. A carry then moves up through the indices in order. A
     *       vectorized arm reads the array in the order of memory.
     * @note The struct keeps the sign apart and does not use two's complement across the array. A
     *       comparison is the hot operation. A comparison of magnitudes scans from the top limb down.
     * @note Zero always has sign 0. Two zeros then always compare equal.
     */
    typedef struct
    {
        uint32_t limb[ORIOR_EXACT_LIMBS]; /**< Magnitude, least significant limb at index 0. */
        int32_t sign;                      /**< -1, 0 or 1. Zero is the only value with sign 0. */
    } OriorExactInteger;

    /** @brief Each entry point that returns a status returns one of these values. */
    typedef enum
    {
        ORIOR_EXACT_OK = 0,       /**< The operation is complete and the result is exact. */
        ORIOR_EXACT_WILL_NOT_FIT, /**< The value needs more limbs than the width holds, or the call cannot
                                        get a work copy past the stack. */
        ORIOR_EXACT_NOT_DECIMAL,  /**< The text is not plain decimal. Exponent notation is not plain decimal. */
        ORIOR_EXACT_BY_ZERO,      /**< The divisor is zero. */
        ORIOR_EXACT_NOT_EXACT     /**< The caller asks for an exact quotient, and the division has a remainder. */
    } OriorExactStatus;

    /**
     * @brief Sets an integer to zero.
     *
     * @param[out] value Integer to clear [BORROWS].
     */
    void orior_exact_zero(OriorExactInteger *value);

    /**
     * @brief Whether two integers have the same value.
     *
     * @param[in] left  First integer [BORROWS].
     * @param[in] right Second integer [BORROWS].
     * @return          1 where the values are equal, 0 otherwise.
     * @note The hot operation. A shift measure requires nothing else of a coordinate, and it requires it once
     *       per point per lag. This is the call every vectorized arm exists to widen.
     */
    int orior_exact_equal(const OriorExactInteger *left, const OriorExactInteger *right);

    /**
     * @brief Orders two integers.
     *
     * @param[in] left  First integer [BORROWS].
     * @param[in] right Second integer [BORROWS].
     * @return          -1 where left is smaller, 1 where it is larger, 0 where they are equal.
     */
    int orior_exact_compare(const OriorExactInteger *left, const OriorExactInteger *right);

    /**
     * @brief Adds two integers.
     *
     * @param[in]  left   First addend [BORROWS].
     * @param[in]  right  Second addend [BORROWS].
     * @param[out] result Sum [BORROWS]. It can be the same object as either input.
     * @return            ORIOR_EXACT_OK, or ORIOR_EXACT_WILL_NOT_FIT where the sum needs more limbs.
     * @note On an error the call does not change `result`.
     */
    OriorExactStatus orior_exact_add(const OriorExactInteger *left, const OriorExactInteger *right,
                                       OriorExactInteger *result);

    /**
     * @brief Subtracts the second integer from the first.
     *
     * @param[in]  left   Minuend [BORROWS].
     * @param[in]  right  Subtrahend [BORROWS].
     * @param[out] result Difference [BORROWS]. It can be the same object as either input.
     * @return            ORIOR_EXACT_OK, or ORIOR_EXACT_WILL_NOT_FIT where the result needs more
     *                    limbs.
     * @note This call makes all of the difference set that the period reads.
     * @note On an error the call does not change `result`.
     */
    OriorExactStatus orior_exact_subtract(const OriorExactInteger *left, const OriorExactInteger *right,
                                            OriorExactInteger *result);

    /**
     * @brief Multiplies two integers.
     *
     * @param[in]  left   First factor [BORROWS].
     * @param[in]  right  Second factor [BORROWS].
     * @param[out] result Product [BORROWS]. It can be the same object as either input.
     * @return            ORIOR_EXACT_OK, or ORIOR_EXACT_WILL_NOT_FIT where the product needs more
     *                    limbs than the width holds.
     * @note The call uses the ladder over the limbs that each factor uses. The arithmetic grows with the
     *       used lengths, and the width adds one pass that writes the result.
     * @note That pass is the cost of a product of a few limbs at a wide width: 8e-5 s at 131072 limbs.
     * @note A rung that cannot get its workspace steps down to the rung below it. Long multiplication
     *       needs no workspace.
     * @note Where the used lengths of the factors sum past the width by more than one limb, the call
     *       errors before any arithmetic. Their product is at least 2^(32 * (sum - 2)), which overruns.
     * @note On an error the call does not change `result`.
     * @warning A product overruns the fixed width far sooner than a sum does. For that reason the read
     *          of a coordinate multiplies a fraction by a cell edge exactly once.
     */
    OriorExactStatus orior_exact_multiply(const OriorExactInteger *left, const OriorExactInteger *right,
                                            OriorExactInteger *result);

    /**
     * @brief Multiplies two integers by the Schonhage-Strassen transform at any size.
     *
     * @param[in]  left   First factor [BORROWS].
     * @param[in]  right  Second factor [BORROWS].
     * @param[out] result Product [BORROWS]. It can be the same object as either input.
     * @return            ORIOR_EXACT_OK, or ORIOR_EXACT_WILL_NOT_FIT where the product needs more
     *                    limbs or the transform cannot get its workspace.
     * @note orior_exact_multiply uses this transform from ORIOR_EXACT_TRANSFORM_LIMBS up. This entry point
     *       lets a test grade and time the rung below its threshold.
     * @note On an error the call does not change `result`.
     */
    OriorExactStatus orior_exact_multiply_transform(const OriorExactInteger *left, const OriorExactInteger *right,
                                                      OriorExactInteger *result);

    /**
     * @brief Divides one integer by another, into a quotient and a remainder.
     *
     * @param[in]  numerator Dividend [BORROWS].
     * @param[in]  divisor   Divisor [BORROWS].
     * @param[out] quotient  Quotient, rounded toward zero [BORROWS]. It can be the same object as either
     *                       input.
     * @param[out] remainder Remainder, with the sign of the numerator [BORROWS]. It can be the same object as
     *                       either input, but not `quotient`.
     * @return               ORIOR_EXACT_OK, ORIOR_EXACT_BY_ZERO for a zero divisor, or
     *                       ORIOR_EXACT_WILL_NOT_FIT where the call cannot get a work copy past the stack.
     * @note numerator = quotient * divisor + remainder, with |remainder| below |divisor|. That is the
     *       division of C itself. A caller that moves between the two meets no third sign rule.
     * @note Knuth's Algorithm D, and Newton's reciprocal once the divisor and the quotient both reach
     *       ORIOR_EXACT_NEWTON_LIMBS.
     * @note On an error the call does not change `quotient` or `remainder`.
     */
    OriorExactStatus orior_exact_divide(const OriorExactInteger *numerator, const OriorExactInteger *divisor,
                                          OriorExactInteger *quotient, OriorExactInteger *remainder);

    /**
     * @brief The same division by Newton's reciprocal at any size.
     *
     * @param[in]  numerator Dividend [BORROWS].
     * @param[in]  divisor   Divisor [BORROWS].
     * @param[out] quotient  Quotient, rounded toward zero [BORROWS]. It can be the same object as either
     *                       input.
     * @param[out] remainder Remainder, with the sign of the numerator [BORROWS]. It can be the same object as
     *                       either input, but not `quotient`.
     * @return               As orior_exact_divide.
     * @note orior_exact_divide uses this division from ORIOR_EXACT_NEWTON_LIMBS up. This entry point lets a
     *       test grade and time the rung below its threshold.
     */
    OriorExactStatus orior_exact_divide_newton(const OriorExactInteger *numerator, const OriorExactInteger *divisor,
                                                 OriorExactInteger *quotient, OriorExactInteger *remainder);

    /**
     * @brief The quotient of a division that the caller knows leaves no remainder.
     *
     * @param[in]  numerator Dividend [BORROWS].
     * @param[in]  divisor   Divisor [BORROWS].
     * @param[out] quotient  Quotient [BORROWS]. It can be the same object as either input.
     * @return               ORIOR_EXACT_OK, ORIOR_EXACT_BY_ZERO for a zero divisor, ORIOR_EXACT_NOT_EXACT
     *                       where the division has a remainder, or ORIOR_EXACT_WILL_NOT_FIT.
     * @note The ORIOR_EXACT_WILL_NOT_FIT case is a work copy past the stack that the call cannot get.
     * @note A multiply and a mask, with no division in it. The call first shifts both past the low zero bits
     *       of the divisor.
     * @note Newton's x(2 - dx) then grows the inverse of the odd divisor modulo 2^(32 limbs). It starts from
     *       d itself, which is its own inverse to 3 bits.
     * @note The quotient is the low limbs of the numerator times that inverse. A multiply of the quotient back
     *       proves it. A remainder shows there, and the call returns an error and not a wrong quotient.
     * @note On an error the call does not change `quotient`.
     */
    OriorExactStatus orior_exact_divide_exact(const OriorExactInteger *numerator, const OriorExactInteger *divisor,
                                                OriorExactInteger *quotient);

    /**
     * @brief The greatest common divisor of the magnitudes of two integers.
     *
     * @param[in]  left   First integer [BORROWS].
     * @param[in]  right  Second integer [BORROWS].
     * @param[out] result The gcd, never negative, and 0 only where both are 0 [BORROWS]. It can be the same
     *                    object as either input.
     * @return            ORIOR_EXACT_OK, or ORIOR_EXACT_WILL_NOT_FIT where the call cannot get a work copy
     *                    past the stack.
     * @note Lehmer's gcd, Knuth's Algorithm L. The leading 32 bits of the pair run Euclid's steps in words
     *       while each quotient is certain.
     * @note The cofactors of those steps then advance the whole pair in one pass.
     * @note On an error the call does not change `result`.
     */
    OriorExactStatus orior_exact_gcd(const OriorExactInteger *left, const OriorExactInteger *right,
                                       OriorExactInteger *result);

    /**
     * @brief Multiplies an integer by ten raised to a power, which applies a scale.
     *
     * @param[in,out] value Integer to scale [BORROWS].
     * @param[in]     power How many powers of ten to apply.
     * @return              ORIOR_EXACT_OK, or ORIOR_EXACT_WILL_NOT_FIT.
     * @note On an error the call does not change `value`.
     */
    OriorExactStatus orior_exact_scale_by_ten(OriorExactInteger *value, uint32_t power);

    /**
     * @brief Reads plain decimal text into an exact integer at a given number of decimal places.
     *
     * @param[in]  text   Decimal text, with an optional sign and an optional uncertainty in brackets
     *                    [BORROWS]. The call drops the uncertainty.
     * @param[in]  length How many bytes of text.
     * @param[in]  digits Decimal places of the value.
     * @param[out] value  The result [BORROWS].
     * @return            ORIOR_EXACT_OK, ORIOR_EXACT_NOT_DECIMAL where the text is not plain decimal, or
     *                    ORIOR_EXACT_WILL_NOT_FIT.
     * @note The ORIOR_EXACT_WILL_NOT_FIT case: the value or its uncertainty has more decimal places than
     *       `digits`, or the value needs more limbs than the width holds.
     * @note The text holds these parts in order. First come any spaces, tabs, carriage returns or line
     *       feeds, and an optional + or -.
     * @note Next come one or more ASCII digits, with one decimal point at max among them. Either side of the
     *       point can be empty, but not both.
     * @note Then come an optional uncertainty of ASCII digits between ( and ), and any spaces, tabs,
     *       carriage returns or line feeds. The text ends there.
     * @note Any other text is ORIOR_EXACT_NOT_DECIMAL. The call decides that before any
     *       ORIOR_EXACT_WILL_NOT_FIT. representation.exact.units accepts exactly the same text.
     * @note Zeros at the end, after the point, do not count as places. ".000" reads as zero, and "1.2300"
     *       reads at two places.
     * @note The call drops the uncertainty from the result. It still sizes the uncertainty as
     *       orior_exact_from_measured does.
     * @note An uncertainty that prints past `digits` places is ORIOR_EXACT_WILL_NOT_FIT. An example is
     *       "1.00000000000000000000000000(1)" at 24 places.
     * @note A read of deposited coordinates needs only the value. orior_exact_from_measured returns both.
     * @note The result equals the value of the text. The call pads text with fewer places than `digits` with
     *       zeros, which is exact for the text.
     * @note The text can be a cut expansion of a longer number, such as a constant printed to 1000 places
     *       and read at 1024. The padded places are then zeros and not the digits of that number.
     * @note For such a number, supply text with at least `digits` places.
     * @note An error on a value with too many places is the same error that representation.exact makes. The
     *       reason is the same: a scale that rounds is a quantum that this end adds.
     * @note That quantum must be an error and never a quiet loss.
     * @note On an error the call does not change `value`.
     */
    OriorExactStatus orior_exact_from_decimal(const char *text, size_t length, uint32_t digits,
                                                OriorExactInteger *value);

    /**
     * @brief Reads decimal text into an exact value and an exact uncertainty, both at one scale.
     *
     * @param[in]  text        Decimal text in the grammar that orior_exact_from_decimal accepts [BORROWS].
     * @param[in]  length      How many bytes of text.
     * @param[in]  digits      Decimal places of both results.
     * @param[out] value       The value [BORROWS].
     * @param[out] uncertainty The uncertainty, as an integer that is not negative [BORROWS].
     * @param[out] carried     1 where the text has an uncertainty in brackets, and 0 where it has none
     *                         [BORROWS].
     * @return                 ORIOR_EXACT_OK, ORIOR_EXACT_NOT_DECIMAL, or ORIOR_EXACT_WILL_NOT_FIT.
     * @note The ORIOR_EXACT_WILL_NOT_FIT case: the value or the uncertainty needs more places than `digits`,
     *       or more limbs than the width holds.
     * @note This entry point is for measured constants. CODATA 2022 gives the fine-structure constant as
     *       7.2973525643e-3, with a standard uncertainty of 0.0000000011e-3.
     * @note A read at 1024 places without that uncertainty gives an integer with 1024 places. The integer
     *       then keeps no record that the source measures only 11 of them.
     * @note The digits in brackets count units of the last place PRINTED in the value, zeros at the end
     *       included. "1.2300(5)" is 1.23 with an uncertainty of 0.0005.
     * @note "137(2)" is 137 with an uncertainty of 2.
     * @note That count of places can be more than the count of the value after the call drops its zeros at
     *       the end. The uncertainty can then give an error at a scale that the value fits.
     * @note A text with no bracket gives a zero uncertainty, with `carried` at 0. A text of "(0)" gives a
     *       zero uncertainty with `carried` at 1: its source states the value as exact.
     * @note On an error the call does not change `value`, `uncertainty` or `carried`.
     */
    OriorExactStatus orior_exact_from_measured(const char *text, size_t length, uint32_t digits,
                                                 OriorExactInteger *value, OriorExactInteger *uncertainty,
                                                 int *carried);

    /**
     * @brief A 64 bit hash of an exact value, as the key of a lookup by position.
     *
     * @param[in] value Integer to hash [BORROWS].
     * @return          The hash.
     * @note Two equal values have the same hash, and that is all this promises. A collision is possible.
     * @note A caller that keys a table on the hash compares the values on a hit, and does not trust the hash.
     *       A trust in the hash lets two different coordinates read as equal. The exact path exists to make
     *       that impossible.
     */
    uint64_t orior_exact_hash(const OriorExactInteger *value);

    /**
     * @brief Counts positions whose value equals the value one lag away.
     *
     * @param[in] positions Positions, in any order, each with an index into `values` [BORROWS].
     * @param[in] values    The value at each position [BORROWS].
     * @param[in] count     How many positions.
     * @param[in] lag       The offset to test, as an exact integer [BORROWS].
     * @return              How many different positions have a position exactly `lag` above them, with an
     *                      equal value.
     * @note This is the measure itself, and the reason for each arm below. The portable form keys a table on
     *       the hash.
     * @note A vectorized form compares many limbs at once. A GPU form compares many positions at once.
     * @note All three return the same count, or one of them has a defect.
     * @note A position that the list holds more than once keeps the value of its last entry, and the count
     *       takes it once.
     * @note representation.exact.placed makes its lookup the same way. The two arms return one count on the
     *       same list.
     */
    size_t orior_exact_agreement(const OriorExactInteger *positions, const uint64_t *values, size_t count,
                                  const OriorExactInteger *lag);

    /**
     * @brief The same count, with an equality test that the caller supplies.
     *
     * @param[in] equal     Whether two integers have the same value [BORROWS].
     * @param[in] positions Positions with values, in any order [BORROWS].
     * @param[in] values    The value at each position [BORROWS].
     * @param[in] count     How many positions.
     * @param[in] lag       The offset to test [BORROWS].
     * @return              How many different positions agree with the place one lag above them.
     * @note Each arm runs this one function, and supplies only its own equality test.
     * @note An arm with its own search is a different algorithm. A time of it against the portable arm then
     *       measures the algorithm, and not the instruction set.
     * @note orior_exact_agreement documents how the count takes a repeated position.
     * @note Where the call cannot get the order, a quadratic scan gives the same count and needs no memory.
     */
    size_t orior_exact_agreement_using(int (*equal)(const OriorExactInteger *left, const OriorExactInteger *right),
                                        const OriorExactInteger *positions, const uint64_t *values, size_t count,
                                        const OriorExactInteger *lag);

#ifdef __cplusplus
}
#endif

#endif

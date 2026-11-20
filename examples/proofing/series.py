"""The numerics, written once, with the multiply passed in.

An ENGINE here is one function, `product(left, right)`, and the interface stops there. Everything in this module comes
from multiplies and shifts: reciprocals, roots, division, modular exponentiation, and Chudnovsky's series. The name of
the multiply therefore names the machine, and no second declaration is necessary anywhere.

WHY THE CALLER PASSES THE MULTIPLY IN, AND NO GLOBAL BINDS IT

Two other arrangements each fail the same way, and the code uses neither.

One is a module-level `product` rebound by a `use_device()` call. Any tool that calls it changes the arithmetic of every
other tool in the process, from anywhere, at any time. A description of a run then needs the call order. That is a
monkeypatch, whatever its name.

The other is two engines, each with its own copy of the recurrences. That removes the rebind, and buys a worse problem.
Two copies of a Newton iteration are one edit away from a disagreement. Nothing reports it when they disagree: the
results simply differ, and both look fine.

A multiply passed in has neither failure. There is one copy of each recurrence, and the import line of the caller picks
the machine. The choice is visible at the call, and not hidden in the call order of the process.

    import digit_engine    then digit_engine.chudnovsky(n)    every multiply on the host
    import device_engine   then device_engine.chudnovsky(n)   every multiply on the card

Both bind this module. Neither can change the other's arithmetic, because there is no global here to change.

NOTHING DIVIDES AT SIZE. Every division below is a Newton reciprocal. Its base case runs sixty four bits past the width
of the divisor, where a host does it in one machine instruction. Above that base case there are only multiplies and
shifts. That property makes the engine substitutable at all: a card that can multiply can run every line of this file.
"""

import math


# Chudnovsky's series, as the constants are usually written.
PER_TERM = 14.181647462725477    # decimal digits gained per term, log10(151931373056000^2 / 24^3)
BASE = 13591409
STEP = 545140134
CUBED = 10939058860032000        # 640320^3 / 24
SCALE = 426880                   # the factor outside, against the root of 10005


def reciprocal(product, value, bits):
    """floor(2^bits / value), with multiplies and shifts, and no division at size.

    Newton on f(y) = 1/y - value/2^bits, whose root is the reciprocal wanted. The step is

            y' = 2y - value * y^2 / 2^bits

    which contains no division: two multiplies and a shift. Each step doubles the number of correct bits. The precision
    doubles on the way up, and the last step dominates the work. All of it is therefore a small multiple of ONE full
    size multiply.

    The recursion lifts the precision, and does not iterate from one bit. The base case runs once. The final correction
    is exact by construction. It steps the answer to the largest value whose product with the divisor does not exceed
    the numerator. The return is the true floor, and not a value within a tolerance of it.

    THE SQUARE GOES THROUGH THE ENGINE TOO. A `lifted * lifted` here would be a host multiply in the middle of a routine
    for the device only. At the sizes in use, it is the largest multiply in the call.
    """
    if value <= 0:
        raise ValueError("reciprocal of a non-positive value")
    width = value.bit_length()
    if bits < width:
        return 0
    if bits <= width + 64:
        return (1 << bits) // value

    half = (bits + width) // 2
    lifted = reciprocal(product, value, half)
    out = (lifted << (bits - half + 1)) - (product(value, product(lifted, lifted)) >> (2 * half - bits))

    # Newton lands within a couple of units here. This settles it with steps, and not with a search.
    top = 1 << bits
    while product(out, value) > top:
        out -= 1
    while product(out + 1, value) <= top:
        out += 1
    return out


def divide(product, top, bottom, places):
    """floor(top * 10^places / bottom), by reciprocal. No division happens at size.

    The code applies the scale to the numerator before it folds in the reciprocal. That keeps every intermediate an
    exact integer, and leaves one shift at the end.
    """
    if bottom <= 0:
        raise ValueError("this divide is for a positive divisor")
    if top < 0:
        # Floor division rounds toward negative infinity. The code carries the sign by a negation of the answer to the
        # exact division, and steps down when anything remains.
        out = divide(product, -top, bottom, places)
        return -out if product(out, bottom) == -top * 10 ** places else -out - 1

    lifted = top * 10 ** places
    bits = bottom.bit_length() + lifted.bit_length() + 8
    out = product(lifted, reciprocal(product, bottom, bits)) >> bits
    while product(out, bottom) > lifted:
        out -= 1
    while product(out + 1, bottom) <= lifted:
        out += 1
    return out


def divide_binary(product, top, bottom, bits):
    """floor(top * 2^bits / bottom). The same routine in the base that the fixed point uses.

    It stays beside the decimal one, and not folded into it, because the two serve different places. A single function
    that takes a base would put a branch in front of every call for no gain. Each name carries its base, and neither
    call site needs a second read.
    """
    if bottom <= 0:
        raise ValueError("this divide is for a positive divisor")
    if top < 0:
        out = divide_binary(product, -top, bottom, bits)
        return -out if product(out, bottom) == -top << bits else -out - 1

    lifted = top << bits
    wide = bottom.bit_length() + lifted.bit_length() + 8
    out = product(lifted, reciprocal(product, bottom, wide)) >> wide
    while product(out, bottom) > lifted:
        out -= 1
    while product(out + 1, bottom) <= lifted:
        out += 1
    return out


def inverse_root(product, value, bits):
    """floor(2^bits / sqrt(value)), by Newton, with no division and no square root under it.

    The step is y' = y * (3 * 2^(2b) - value * y^2) / 2^(2b+1), where the half is a shift. This is the iteration that
    converges to one over the root. The code uses it, and not the iteration for the root itself, because that one
    contains a division and this one does not.
    """
    if value <= 0:
        raise ValueError("no inverse root of a non-positive value is wanted here")

    # THE BASE CASE MUST KNOW HOW BIG THE VALUE IS. A direct division of 2^bits by the root returns zero for any value
    # whose root exceeds it. A zero start leaves the settle loop to climb to the answer one unit at a time, which does
    # not finish. A square of the scale first, and one root of the quotient, is the same quantity with nothing to
    # underflow. 2^b / sqrt(v) is sqrt(2^(2b) / v).
    if bits <= max(64, value.bit_length() // 2 + 32):
        return math.isqrt((1 << (2 * bits)) // value)

    # HALVE THE SIGNIFICANT BITS AND LEAVE THE SCALE. The answer carries `bits - width/2` significant bits, the
    # remainder of the scale once the root's own size comes out. To halve `bits` alone would halve the scale, and leave
    # the significant count almost untouched for a large value. The step below it would arrive with too little precision
    # to double.
    half = (bits + value.bit_length() // 2) // 2 + 16
    lifted = inverse_root(product, value, half)
    shifted = lifted << (bits - half)

    squared = product(shifted, shifted)
    inner = (3 << (2 * bits)) - product(value, squared)
    out = product(shifted, inner) >> (2 * bits + 1)

    # Settle it against the definition, which costs two multiplies and removes any doubt.
    while product(product(out, out), value) > (1 << (2 * bits)):
        out -= 1
    while product(product(out + 1, out + 1), value) <= (1 << (2 * bits)):
        out += 1
    return out


def root_scaled(product, value, places):
    """floor(sqrt(value) * 10^places) with no division at size, from the inverse root.

    sqrt(v) = v / sqrt(v). The value times its own inverse root gives the root, with only the operations of the card.
    """
    # THE RECIPROCAL ROOT MUST COVER THE WHOLE VALUE. `back` is 2^bits / sqrt(value), which carries bits -
    # bit_length(value)/2 significant bits. The answer needs bit_length(value)/2 + 4*places of them. At half the bit
    # length, `back` has about sixty four good bits, and the settle loop then never finishes its climb.
    bits = value.bit_length() + 4 * places + 64
    back = inverse_root(product, value, bits)

    # THE CODE FORMS THE SCALE ONCE AND SQUARES IT. A request for ten to twice the places would build a number of twice
    # the width from nothing. The narrower one is already in hand, and one multiply separates them.
    scale = 10 ** places
    out = product(value * scale, back) >> bits
    target = product(value, product(scale, scale))
    while product(out, out) > target:
        out -= 1
    while product(out + 1, out + 1) <= target:
        out += 1
    return out


def reduce_by(product, value, modulus, folded=None):
    """`value` modulo `modulus`, with the reciprocal, and no division at size.

    `folded` lets a caller hand in the reciprocal that it already has. That saves a full Newton lift every time the same
    modulus comes back, which is thousands of times in one exponentiation.
    """
    if value < modulus:
        return value
    # THE RECIPROCAL MUST COVER THE VALUE AS WELL AS THE MODULUS. If `bits` is only wide enough for the modulus, the
    # truncation in `folded` scales with `value`. The quotient then comes out low by an amount that grows with it, and
    # the correction loop must subtract the modulus that many times. A `bits` sized to the value keeps the quotient
    # within one of the truth.
    bits = value.bit_length() + modulus.bit_length() + 8
    if folded is None or folded[1] < bits:
        folded = (reciprocal(product, modulus, bits), bits)
    guess = product(value, folded[0]) >> folded[1]
    rest = value - product(guess, modulus)
    while rest < 0:
        rest += modulus
    while rest >= modulus:
        rest -= modulus
    return rest


def power_mod(product, base, power, modulus):
    """`base` to `power` modulo `modulus`, with the reciprocal formed once for the whole climb.

    Square and multiply, which is log(power) squarings, each one a multiply that the engine carries. The code computes
    the reciprocal of the modulus once, and reuses it at every step. It does not change, and a new one per step would
    cost more than the square that it serves.
    """
    if modulus <= 1:
        return 0
    # Every reduction inside the climb is of a product of two values below the modulus. The
    # widest value passed to `reduce_by` is just under the modulus squared. Sizing the reciprocal to
    # that once means no reduction below has to rebuild it.
    bits = 2 * modulus.bit_length() + 8
    folded = (reciprocal(product, modulus, bits), bits)

    out = 1
    walk = reduce_by(product, base, modulus, folded)
    while power > 0:
        if power & 1:
            out = reduce_by(product, product(out, walk), modulus, folded)
        power >>= 1
        if power:
            walk = reduce_by(product, product(walk, walk), modulus, folded)
    return out


def terms_for(digits):
    """How many terms of the series `digits` places needs, from the series' own rate."""
    return max(int(digits / PER_TERM) + 2, 2)


def split(product, first, last):
    """Chudnovsky's series over the terms in [first, last), as the triple binary split carries.

    P, Q and T are exact integers: the numerator product, the denominator product, and the partial sum already scaled by
    that denominator. The merge is one line, and it is all of the method.

    NOTHING DIVIDES HERE, and that absence is the method. The linear sum divides at every term at full working
    precision, and the square is where it loses. This defers the single division to the very end, and every intermediate
    stays an exact integer that no rounding touched.

    Recursion depth is the log of the term count. A billion digits is a depth of some twenty six, and there is no stack
    concern at any size that a desktop can reach.
    """
    if last - first == 1:
        if first == 0:
            return 1, 1, BASE
        upper = (6 * first - 5) * (2 * first - 1) * (6 * first - 1)
        lower = first ** 3 * CUBED
        total = upper * (BASE + STEP * first)
        return upper, lower, -total if first % 2 else total

    middle = (first + last) // 2
    left_upper, left_lower, left_total = split(product, first, middle)
    right_upper, right_lower, right_total = split(product, middle, last)
    return (product(left_upper, right_upper),
            product(left_lower, right_lower),
            product(left_total, right_lower) + product(left_upper, right_total))


def chudnovsky(product, digits, guard=24):
    """Pi to `digits` places, as an integer that holds them.

    One division and one square root, both at the end, both at the full working precision. The guard digits cover
    exactly those two roundings, and no accumulation, because there is no accumulation: everything before them is exact.
    """
    work = digits + guard
    upper, lower, total = split(product, 0, terms_for(work))
    root = root_scaled(product, 10005, work)
    return divide(product, product(SCALE * root, lower), total, 0) // 10 ** guard

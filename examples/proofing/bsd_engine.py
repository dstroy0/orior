"""The BSD quotient, with the multiply handed in.

docs-check: quoting
    import digit_engine  as engine     every multiply on the host
    import device_engine as engine     every multiply on the card
    bsd_engine.quotient(engine.product, ...)
docs-check: end quoting

WHY THIS FILE TAKES THE MULTIPLY AND DOES NOT PICK ONE

The same reason as in `series`. An engine here is one function, `product(left, right)`, and every operation at size
below goes through it. Nothing rebinds it, nothing dispatches on operand width, and the caller's import line is the
record of which machine ran the work.

THE OBJECT IS A TRANSFORM, AND THE TRANSFORM ENGINE IS THEREFORE THE RIGHT MACHINE

The a_n are the Fourier coefficients of a weight two cusp form, f(tau) = sum a_n q^n. The L-function is its Mellin
transform, and the value computed here is that transform read at s = 1:

    L(E,1) = 2 sum_{n>=1} (a_n / n) exp(-2 pi n / sqrt(N))

This is therefore a wave read at one point, and the engine underneath is a number theoretic transform. A sum in a Python
loop over machine integers computes the right number on the wrong machine.

NO DIVISION HAPPENS AT SIZE

Every quotient below is `series.reciprocal`, which is Newton: two multiplies and a shift per step, with the correct bits
doubled each step. A division then costs a small multiple of one full size multiply. Every root is
`series.inverse_root`, which has neither a division nor a root under it. The only primitive anywhere beneath this file
is `product`.
"""

import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import series


def times(product, left, right, places):
    """left * right at one scale. Signs handled here; the engine sees magnitudes only."""
    if left == 0 or right == 0:
        return 0
    negative = (left < 0) != (right < 0)
    magnitude = product(abs(left), abs(right)) >> places
    return -magnitude if negative else magnitude


def over(product, top, bottom, places):
    """top / bottom at one scale, by Newton reciprocal, with no division at size."""
    if top == 0:
        return 0
    negative = (top < 0) != (bottom < 0)
    bottom = abs(bottom)
    bits = places + bottom.bit_length() + 8
    back = series.reciprocal(product, bottom, bits)
    magnitude = product(abs(top), back) >> (bits - places)
    return -magnitude if negative else magnitude


def root(product, value, places):
    """sqrt of a scaled value at the same scale, from `series.inverse_root`.

    Settled against the definition afterwards, which costs two multiplies, and leaves no doubt about the last unit.
    `series` settles its own roots the same way.
    """
    if value <= 0:
        return 0
    target = value << places
    bits = target.bit_length() // 2 + places + 64
    back = series.inverse_root(product, target, bits)
    out = product(target, back) >> bits
    while product(out, out) > target:
        out -= 1
    while product(out + 1, out + 1) <= target:
        out += 1
    return out


def pi_at(product, places):
    """pi at `places` binary digits, by Machin, through the engine.

    The arctangent terms divide by small odd integers and by k squared, and both are machine words. Those divisions are
    therefore not at size, and stay as they are. The accumulation is exact.
    """
    guard = 64
    scale = 1 << (places + guard)

    def arctangent(k):
        square = k * k
        term = scale // k
        total = 0
        j = 0
        while term:
            piece = term // (2 * j + 1)
            total += piece if j % 2 == 0 else -piece
            term //= square
            j += 1
        return total

    return (16 * arctangent(5) - 4 * arctangent(239)) >> guard


def exponential_negative(product, argument, places):
    """exp(-x) for scaled non-negative x, at the same scale.

    Halved until the argument is below one half, summed, then squared back through the engine. The reduction stops the
    series with alternate signs from a loss of leading digits. Above one, the terms grow before they shrink, and the
    cancellation costs several places.
    """
    one = 1 << places
    halvings = 0
    while argument > (one >> 1):
        argument >>= 1
        halvings += 1

    term = one
    total = one
    k = 1
    while term:
        term = times(product, term, argument, places) // k
        total += -term if k % 2 else term
        k += 1

    for _ in range(halvings):
        total = times(product, total, total, places)
    return total


def divisors(n):
    """d(n), for the Hasse bound on |a_n| in the tail estimate."""
    count = 0
    d = 1
    while d * d <= n:
        if n % d == 0:
            count += 2 if d * d != n else 1
        d += 1
    return count


def l_at_one(product, coefficients, conductor, places, report=None):
    """L(E,1) at `places` binary digits, and an upper bound on everything omitted.

    The tail past the cut has a BOUND, and the code assumes nothing about its size. Hasse gives |a_n| <= d(n) sqrt(n),
    and the n-th term carries q^n. A geometric series therefore dominates the omitted part, and this computes its sum. A
    cut chosen because the terms looked small is a guess. A cut with a bound beside it is a statement.
    """
    pi = pi_at(product, places)
    scaled_conductor = conductor << places
    root_n = root(product, scaled_conductor, places)
    step = over(product, 2 * pi, root_n, places)
    q = exponential_negative(product, step, places)

    total = 0
    power = q
    cut = 0
    for n in range(1, len(coefficients)):
        if power == 0:
            break
        a = coefficients[n]
        if a:
            total += (a * power) // n
        power = times(product, power, q, places)
        cut = n

    tail = 0
    probe = power
    n = cut + 1
    while probe > 0 and n < cut + 4096:
        tail += (divisors(n) * math.isqrt(n) * probe) // n
        probe = times(product, probe, q, places)
        n += 1
    tail = 2 * tail + 1

    if report is not None:
        report.update({"terms": cut, "q": q, "pi": pi, "sqrt_conductor": root_n,
                       "tail_bound": tail})
    return 2 * total


def l_at_one_rectangular(product, coefficients, conductor, places, report=None):
    """L(E,1), summed as a rectangle instead of a line.

    THE WALK IS THE COST, AND IT IS THE WRONG SHAPE

    Summed term by term, the series needs q^n at every n, and forms it as q^(n-1) * q. N terms then cost N FULL WIDTH
    multiplies. That is an absolute walk: each term is its own arm, the work grows as N, and nothing serves twice.

    Written as n = i*m + j, the same N terms become

            q^(i m + j) = (q^m)^i * q^j

    and every term is a PAIR of values already in hand. One is from m baby steps q^j, and one from m giant steps
    (q^m)^i. With m arms on each side, there are m^2 = N terms. The event count therefore grows quadratically in the
    work, and the full width multiplies fall from N to 2*sqrt(N).

    The co-arms bench of this tree states the theorem and its condition. A quadratic gain appears when the win condition
    is RELATIONAL, met by a pair, and not when it is absolute, met by one arm. A power series is relational in exactly
    that sense, because an exponent is a sum, and a sum splits into a pair. No restatement of mining has that form, and
    this has it.

    WHAT STAYS CHEAP AND MUST NOT COUNT AS A MULTIPLY

    The coefficient weight a_n / n is a SMALL integer over a small integer. A full width value times a 16 bit integer,
    divided by another, is linear in the width, and not a multiply at size. The inner sums therefore cost the same as
    the line walk, and only the structure changes.

    Memory is m full width values. At m = sqrt(N), that is the square root of what the line walk would need if it stored
    anything, and it stores nothing.
    """
    pi = pi_at(product, places)
    root_n = root(product, conductor << places, places)
    step = over(product, 2 * pi, root_n, places)
    q = exponential_negative(product, step, places)

    # HOW FAR THE SERIES RUNS, COMPUTED AND NOT WALKED.
    #
    # Consider a loop that steps q^n one full width multiply at a time until it underflows, only to find the cut. That
    # is the line walk that this function exists to remove. Above the rectangle, it would cost exactly what the
    # rectangle saves: the multiplies fall from 23,978 to 309, and the run still takes 32 seconds.
    #
    # q = exp(-2 pi / sqrt(N)), and q^n therefore drops below 2^-places at
    #
    # docs-check: quoting
    #     n > places * ln2 * sqrt(N) / (2 pi)
    # docs-check: end quoting
    #
    # which is one division. The bit length of q gives the same answer without a logarithm. The value q holds
    # `places - width` significant bits, and q^n underflows at n = places / (places - q.bit_length()).
    shortfall = places - q.bit_length()
    if shortfall <= 0:
        shortfall = 1
    reach = min(len(coefficients) - 1, (places // shortfall) + 2)

    side = max(1, math.isqrt(reach) + 1)

    # THE BABY STEPS. q^1 through q^side, one multiply each.
    babies = [0] * (side + 1)
    babies[0] = 1 << places
    babies[1] = q
    for j in range(2, side + 1):
        babies[j] = times(product, babies[j - 1], q, places)
    giant = babies[side]

    # THE ROWS. Each is a sum of small-weighted baby steps, and carries no multiply at size.
    rows = []
    row = 0
    filled = 0
    for i in range((reach // side) + 1):
        row = 0
        used = False
        for j in range(1, side + 1):
            n = i * side + j
            if n > reach or n >= len(coefficients):
                break
            a = coefficients[n]
            if a:
                row += (a * babies[j]) // n
                used = True
            filled = n
        rows.append(row)
        if not used and i * side > reach:
            break

    # HORNER OVER THE GIANT STEP. One multiply per row, and the rows are sqrt(N) of them.
    total = 0
    for row in reversed(rows):
        total = times(product, total, giant, places) + row

    if report is not None:
        report.update({"terms": filled, "side": side, "rows": len(rows),
                       "full_multiplies": (side - 1) + len(rows),
                       "line_multiplies": reach, "q": q, "pi": pi})
    return 2 * total


def agm(product, left, right, places, report=None):
    """Arithmetic-geometric mean, through the engine. Quadratic, and the step count is small."""
    steps = 0
    while left != right:
        mean = (left + right) >> 1
        geometric = root(product, times(product, left, right, places), places)
        if mean == left and geometric == right:
            break
        left, right = mean, geometric
        steps += 1
        if steps > 200:
            break
    if report is not None:
        report["agm_steps"] = steps
    return left


def real_root_of_cubic(product, A, B, places):
    """The real root of x^3 + Ax + B as a scaled integer, by Newton through the engine."""
    magnitude = max(abs(A) ** 3, abs(B) ** 2, 1)
    x = (1 << (magnitude.bit_length() // 6 + 2)) << places
    if B > 0:
        x = -x
    for _ in range(400):
        square = times(product, x, x, places)
        cube = times(product, square, x, places)
        value = cube + A * x + (B << places)
        derivative = 3 * square + (A << places)
        if derivative == 0:
            break
        nxt = x - over(product, value, derivative, places)
        if nxt == x:
            break
        x = nxt
    return x


def real_period(product, A, B, places, report=None):
    """Omega for y^2 = x^3 + Ax + B, through the engine.

    Two branches, decided by the exact sign of the discriminant:

    docs-check: quoting
            three real roots   Omega = 2 pi / AGM(sqrt(e1-e3), sqrt(e1-e2))
            one real root      Omega = 2 pi / AGM(2 sqrt(d), sqrt(2d + 3 e1)), d = sqrt(3 e1^2 + A)
    docs-check: end quoting
    """
    discriminant = -16 * (4 * A ** 3 + 27 * B ** 2)
    pi = pi_at(product, places)
    if discriminant >= 0:
        raise NotImplementedError("three real roots branch is not graded yet")

    e1 = real_root_of_cubic(product, A, B, places)
    inside = 3 * times(product, e1, e1, places) + (A << places)
    d = root(product, inside, places)
    mean = agm(product, 2 * root(product, d, places),
               root(product, 2 * d + 3 * e1, places), places, report)
    if report is not None:
        report.update({"discriminant": discriminant, "e1": e1, "d": d, "agm": mean})
    return over(product, 2 * pi, mean, places) if mean else 0

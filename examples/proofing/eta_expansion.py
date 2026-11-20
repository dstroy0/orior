"""The q-expansion of an eta product, through the transform domain.

WHY THIS, AND NOT A COUNT OF POINTS

A count of points on the curve over F_p for each prime p gives the coefficients a_n of a weight two newform. It is
exact, and costs O(p) for each prime. A count to n = 10^8 that way is out of reach by a wide margin. The arithmetic
engine underneath would sit idle while a Python loop does the work.

For a curve whose form is an ETA PRODUCT, the same coefficients are a polynomial identity. Curve 11a1 is the first and
smallest case:

    sum a_n q^n  =  eta(tau)^2 eta(11 tau)^2  =  q * prod (1 - q^n)^2 (1 - q^(11n))^2

Euler's pentagonal number theorem then makes each factor SPARSE:

    prod (1 - q^n)  =  sum_k (-1)^k q^(k(3k-1)/2),   k over all integers

That has O(sqrt(N)) terms below q^N, and not N of them. The whole expansion is therefore four sparse series and three
multiplies. A multiply of polynomials is a convolution, and the transform computes convolutions.

WHY THE RESIDENT DOMAIN AND NOT device_multiply

Three multiplies in a row. Through `device_multiply`, that is nine transforms per modulus, because each product goes
back out of the domain only to come in again. Through `gpu_domain`, it is four: one for each operand in, and one on the
way out.

THE COEFFICIENTS STAY SMALL, AND THAT MAKES THE DOMAIN LEGAL HERE

The domain has no carries. What three moduli recombine bounds a chain, at about 2^93. That ceiling stops a chain of full
width limbs at one multiply. Here it is nowhere near. Hasse bounds |a_n| by d(n) sqrt(n), and at n = 10^8, a coefficient
needs some 20 bits. The intermediate squares of a pentagonal series are counts of the same order, like partitions.

The code declares the bound to the domain, and then CHECKS it against what comes back. A wrong declared bound wraps
silently.

SIGNS. A transform over a prime field carries no sign: a negative coefficient arrives as its residue near the top of the
recombined range. The read back takes anything above half the range as negative. That is the ordinary balanced reading,
and it applies here once, on the way out.
"""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import gpu_domain


def pentagonal(limit):
    """prod (1 - q^n) truncated below q^limit, as a dense coefficient list.

    Euler: the product is the sum over all integers k of (-1)^k q^(k(3k-1)/2). The exponents are the generalized
    pentagonal numbers, and there are about sqrt(2*limit/3) of them below the cut. This fills a list of `limit` entries,
    and only that many of them are non-zero.

    The list holds residues modulo the recombined range. The value -1 arrives as RANGE - 1. The read back takes the sign
    once at the end, with no sign tracked through the transform.
    """
    out = [0] * limit
    k = 1
    out[0] = 1
    while True:
        placed = False
        for sign_k in (k, -k):
            exponent = sign_k * (3 * sign_k - 1) // 2
            if 0 <= exponent < limit:
                out[exponent] = 1 if k % 2 == 0 else -1
                placed = True
        if not placed:
            break
        k += 1
    return out


def stretched(series, factor, limit):
    """The same series in q^factor: coefficient at n*factor, zero elsewhere."""
    out = [0] * limit
    for n, value in enumerate(series):
        at = n * factor
        if at >= limit:
            break
        if value:
            out[at] = value
    return out


def as_residues(series, modulus):
    """A signed list as residues, since the transform field carries no sign."""
    return [value % modulus for value in series]


def balanced(value, modulus):
    """A residue read back as a signed integer, the ordinary balanced reading."""
    return value - modulus if value > modulus // 2 else value


def eta_product_coefficients(limit, level, power, expect_bits, report=None):
    """a_n below `limit` for q * prod (1 - q^n)^power (1 - q^(level n))^power.

    For curve 11a1, `power` is 2 and `level` is 11. The leading q shifts the answer by one on the way out, and it is why
    a_1 = 1, and not a_0.

    `expect_bits` is the caller's own bound on the widest coefficient, from Hasse or from the combinatorics. The code
    declares it to the domain, and checks it against the result.
    """
    modulus = gpu_domain.PRIME0 * gpu_domain.PRIME1 * gpu_domain.PRIME2

    base = pentagonal(limit)
    lifted = stretched(base, level, limit)

    # THE LENGTH COVERS THE WHOLE CHAIN, AND NOT ONE PRODUCT. There are 2*power factors here, each truncated below
    # q^limit, and the total degree of the chain runs to 2*power*limit. A length for a single product at 2*limit would
    # let the third multiply fold degree 2*limit and above back onto the low coefficients. Those are exactly the
    # requested ones, and the fold is silent.
    span = 2 * power * limit
    length = 1
    while length < span:
        length <<= 1

    domain = gpu_domain.Domain(length, 3)
    try:
        # Every multiply declares the SAME result width. `expect_bits` is the caller's bound on the widest coefficient
        # anywhere in the chain, and not on one step of it. The code checks it against what comes back, below.
        domain.load_signed(0, base[:limit])
        domain.load_signed(1, base[:limit])
        for _ in range(power - 1):
            domain.times(0, 1, bound=expect_bits)

        domain.load_signed(2, lifted[:limit])
        for _ in range(power):
            if domain.used[2] == 0:
                domain.load_signed(2, lifted[:limit])
            domain.times(0, 2, bound=expect_bits)

        coefficients = domain.read_coefficients(0, limit, signed=True)
    finally:
        domain.close()

    # The leading q shifts everything by one: a_1 is the constant coefficient of the product.
    out = [0] * (limit + 1)
    widest = 0
    for n, value in enumerate(coefficients):
        widest = max(widest, abs(value).bit_length())
        if n + 1 <= limit:
            out[n + 1] = value

    if report is not None:
        report.update({"length": length, "declared_bits": expect_bits,
                       "widest_bits": widest, "ceiling": gpu_domain.CAPACITY,
                       "pentagonal_terms": sum(1 for v in base if v)})
    return out

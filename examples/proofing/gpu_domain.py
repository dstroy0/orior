"""Operands that stay in the transform domain between multiplies.

WHAT THIS IS FOR, STATED AS THE COST THAT IT REMOVES

`gpu_multiply.device_multiply` transforms both operands, multiplies pointwise, and transforms back, for every call: nine
transforms across the three moduli. A chain of multiplies through that door pays an inverse at the end of each product,
and two forwards at the start of the next. Those three undo and redo the same work.

In the transform domain, a multiply is one elementwise kernel, and no transform at all. A product of k factors costs:
  - through device_multiply: 3k transforms per modulus.
  - through this: k + 1 transforms per modulus.

A q-expansion is a polynomial built from many factors. For it, the transform then costs once, and does not dominate.

WHAT IT IS NOT FOR

A single multiply. One product through here is the same nine transforms plus the protocol. `device_multiply` stays the
right door for one operation, and nothing here replaces it.

THE SESSION DECLARES THE LENGTH ONCE, AND THE CALLER OWNS THE PADDING

A pointwise multiply needs two values transformed at the same length, and the length is therefore fixed for the session.
A convolution of runs that occupy `a` and `b` coefficients needs a + b places. A length shorter than the total output of
the chain wraps SILENTLY, the same way as a wrong twiddle. `used` tracks each buffer, and `check_room` refuses before a
wrap, and not after it.

THE SECOND CEILING, WHICH DECIDES WHAT THIS IS GOOD FOR

Nothing here carries. `device_multiply` recombines and normalizes to limbs on every call, and its coefficients never
grow. A resident chain holds RAW CONVOLUTION COEFFICIENTS, and those grow with every multiply. The three moduli
recombine values below

    p0 * p1 * p2  ~  2^93

and a coefficient past that wraps silently. The result has the ordinary shape, and is not the convolution of anything.

MEASURED, AND IT IS SHARP. With full 32 bit limbs in a chain, a coefficient after k multiplies runs to about 32(k+1) +
log2(length) bits. At length 4096, that is 32k + 44: k = 1 fits, and k = 2 does not. A chain of five limb multiplies
through this door comes back wrong for exactly that reason, and a single multiply comes back right.

THIS DOOR IS THEREFORE FOR SMALL COEFFICIENTS, AND THAT IS NO CONSOLATION PRIZE. A q-expansion is its design case.
Euler's pentagonal series has coefficients in {-1, 0, 1}, and the coefficients of a weight two form are O(d(n) sqrt(n))
by Hasse. Hundreds of multiplies fit under 2^93 there, and in that chain, one transform in place of three matters.

`check_capacity` refuses before the wrap. A caller who multiplies limbs should use `device_multiply`.
"""

import atexit
import os
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
PACKER = os.path.join(ROOT, "build", "bench", "bench_ntt_cuda.exe")
BYTES = 4

PRIME0 = 2013265921
PRIME1 = 2281701377
PRIME2 = 3892314113

OPEN, LOAD, TIMES, READ, DROP, CLOSE, LOAD_SIGNED = 1, 2, 3, 4, 5, 0, 6

# What the three moduli recombine, as a bit width. Below this a coefficient is exact; above it the
# recombination wraps and returns a value of the ordinary shape that is not the convolution.
CAPACITY = (PRIME0 * PRIME1 * PRIME2).bit_length() - 1


class Domain(object):
    """One device session that holds transformed operands across many multiplies."""

    def __init__(self, length, slots):
        if length & (length - 1) or length < 2:
            raise ValueError("a transform length is a power of two")
        if not os.path.exists(PACKER):
            raise SystemExit("build it first: powershell utils/bench/bench_ntt_cuda.ps1")
        self.length = length
        self.slots = slots
        self.used = [0] * slots
        # Bits that the largest coefficient in each buffer can reach. Tracked and not assumed, because the ceiling that
        # it runs into is silent.
        self.magnitude = [0] * slots
        self.served = subprocess.Popen(
            [PACKER, "--domain"], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=None, bufsize=0)
        self._send(OPEN, length, slots)
        atexit.register(self.close)

    def _send(self, *words):
        self.served.stdin.write(struct.pack("<%dQ" % len(words), *words))
        self.served.stdin.flush()

    def _read_exactly(self, want):
        out = bytearray()
        while len(out) < want:
            chunk = self.served.stdout.read(want - len(out))
            if not chunk:
                raise SystemExit("the device stopped mid answer")
            out.extend(chunk)
        return bytes(out)

    def check_capacity(self, into, other):
        """Refuse a multiply whose coefficients would pass what three moduli can recombine.

        A coefficient of the product is a sum of products of one coefficient from each side. The number of those terms
        is at most the SHORTER of the two runs, and not the transform length. The two widths plus log2(min(used))
        therefore bound the width.

        THE LENGTH IS THE WRONG TERM, AND IT COSTS REAL CHAINS. A bound with log2(length) adds the same fourteen bits at
        every step of a chain at length 2^14. It does so for operands of two coefficients, and for operands of sixteen
        thousand. Over twenty four factors, that is some three hundred bits of slack that no coefficient ever reaches.
        The guard would then refuse a chain of (1 + q^k) factors whose true coefficients are partition counts, and fit
        in ten bits.

        Past CAPACITY, the recombination wraps and says nothing. This checks for that failure in advance, and does not
        report it afterwards.
        """
        terms = max(1, min(self.used[into], self.used[other]))
        reach = self.magnitude[into] + self.magnitude[other] + terms.bit_length()
        if reach > CAPACITY:
            raise ValueError(
                "a coefficient would reach %d bits and three moduli recombine %d, so the "
                "product would wrap. Chain fewer factors, or use device_multiply, which "
                "carries between multiplies." % (reach, CAPACITY))
        return reach

    def check_room(self, slot, adding):
        """Refuse a multiply that would wrap, before it happens, and not after.

        A wrap produces a value of the ordinary shape that is not the convolution of anything, and nothing downstream
        reports that failure.
        """
        if self.used[slot] + adding > self.length:
            raise ValueError(
                "slot %d holds %d coefficients and %d more would pass the length %d, "
                "so the convolution would wrap" % (slot, self.used[slot], adding, self.length))

    def load(self, slot, coefficients):
        """Put a run of 32 bit coefficients in a buffer, forward transformed. Three transforms."""
        count = len(coefficients)
        if count > self.length:
            raise ValueError("%d coefficients do not fit a transform of length %d"
                             % (count, self.length))
        self._send(LOAD, slot, count)
        self.served.stdin.write(struct.pack("<%dI" % count, *coefficients))
        self.served.stdin.flush()
        self.used[slot] = count
        self.magnitude[slot] = max((int(c).bit_length() for c in coefficients), default=0)

    def load_signed(self, slot, coefficients):
        """Put SIGNED coefficients in a buffer, forward transformed.

        The coefficients of a polynomial carry signs, and the limbs of an integer do not. This is therefore a separate
        door from `load`. Each value goes out as a two's complement int32, and the device maps it into each field, where
        a residue has no sign to lose.
        """
        count = len(coefficients)
        if count > self.length:
            raise ValueError("%d coefficients do not fit a transform of length %d"
                             % (count, self.length))
        for value in coefficients:
            if not -(1 << 31) <= value < (1 << 31):
                raise ValueError("%d does not fit a signed 32 bit coefficient" % value)
        self._send(LOAD_SIGNED, slot, count)
        self.served.stdin.write(struct.pack("<%di" % count, *coefficients))
        self.served.stdin.flush()
        self.used[slot] = count
        self.magnitude[slot] = max((abs(int(c)).bit_length() for c in coefficients), default=0)

    def load_integer(self, slot, value):
        """Put a non-negative integer in a buffer as its own limbs, its natural form."""
        if value < 0:
            raise ValueError("this domain holds magnitudes; the caller carries the sign")
        count = max(1, (value.bit_length() + 31) // 32)
        self._send(LOAD, slot, count)
        self.served.stdin.write(value.to_bytes(count * BYTES, "little"))
        self.served.stdin.flush()
        self.used[slot] = count
        self.magnitude[slot] = 32          # a limb is a full 32 bit word

    def expect(self, slot, bits):
        """State the true coefficient width for a buffer, in place of the worst case estimate.

        WHY A CALLER MAY OVERRIDE THE GUARD, AND WHAT IT OWES IN RETURN

        `check_capacity` compounds a worst case: every coefficient at its maximum, and every term in the sum. Over a
        long chain, that estimate outruns the truth badly. On the chain of twenty four (1 + q^k) factors, it reaches 96
        bits against a ceiling of 93. The real coefficients there are counts of partitions into distinct parts, and fit
        in ten.

        A caller with a form usually has a SHARP bound, where this has only a crude one. The coefficients of a weight
        two cusp form obey |a_n| <= d(n) sqrt(n) by Hasse, and a partition count has its own estimate. Those are
        theorems about the object, and they beat an estimate that knows nothing about it.

        THE OBLIGATION. A wrong declared bound wraps silently, and this guard exists for that failure. The caller must
        check the claim against what `read_coefficients` returns, as `eta_expansion` does, so that a wrong declaration
        fails, and does not pass as true.
        """
        self.magnitude[slot] = bits

    def times(self, into, other, bound=None):
        """into *= other, pointwise, in the domain. No transform, and no carry either.

        `bound` is the caller's own width for the RESULT, in bits, and it replaces the worst case, with no combination.
        That distinction matters. A declared width for each OPERAND would still leave `check_capacity` to add them
        together and compound. A caller who knows the width of the answer would then have no way to say so. On a chain
        whose true coefficients are eight bits wide, that would fire at 97 bits against a ceiling of 93.

        A wrong bound wraps silently, and a caller who declares one owes the check on the way out. `read_coefficients`
        returns the values, and their widths are then a fact, and not a claim.
        """
        self.check_room(into, self.used[other])
        if bound is None:
            self.magnitude[into] = self.check_capacity(into, other)
        else:
            if bound > CAPACITY:
                raise ValueError("a declared width of %d bits is past the %d three moduli "
                                 "recombine" % (bound, CAPACITY))
            self.magnitude[into] = bound
        self._send(TIMES, into, other)
        self.used[into] += self.used[other]

    def read(self, slot, count=None):
        """Inverse transform, recombine, and return the coefficients. Three transforms.

        DESTRUCTIVE: the inverse runs in place, and the buffer no longer holds a transformed value. It needs a new load
        before the next multiply. A copy of a buffer of hundreds of megabytes, to avoid that, would cost more than the
        reload.
        """
        if count is None:
            count = min(self.used[slot], self.length)
        self._send(READ, slot, count)
        want = count * BYTES
        arrays = [int.from_bytes(self._read_exactly(want), "little") for _ in range(3)]
        self.used[slot] = 0
        self.magnitude[slot] = 0
        base, step, rest = arrays
        return base + PRIME0 * step + PRIME0 * PRIME1 * rest

    def read_coefficients(self, slot, count=None, signed=True):
        """Inverse transform and return the coefficients ONE AT A TIME, recombined per coefficient.

        WHY THIS IS NOT `read`, AND WHY A MIX OF THEM RETURNS NONSENSE

        `read` treats each of the three returned arrays as one little endian integer, and recombines them as base +
        p0*step + p0*p1*rest. That is right for a PRODUCT OF INTEGERS, where the arrays are the limbs of the product,
        and the carries belong inside that addition.

        A polynomial has no carries. The recombination of its coefficient at index i uses only the three residues AT
        THAT INDEX. The recombination is therefore per coefficient, and a coefficient may be as wide as 2^93 without
        effect on its neighbor. A polynomial through `read` gets its adjacent coefficients added together. It comes back
        as a number of the ordinary shape, and not the convolution of anything.

        `signed` reads anything above half the recombined range as negative. A polynomial over Z needs that balanced
        reading, and the limbs of an integer do not.
        """
        if count is None:
            count = min(self.used[slot], self.length)
        self._send(READ, slot, count)
        want = count * BYTES
        arrays = []
        for _ in range(3):
            raw = self._read_exactly(want)
            arrays.append(struct.unpack("<%dI" % count, raw))
        self.used[slot] = 0
        self.magnitude[slot] = 0

        modulus = PRIME0 * PRIME1 * PRIME2
        half = modulus // 2
        out = []
        for base, step, rest in zip(*arrays):
            value = base + PRIME0 * step + PRIME0 * PRIME1 * rest
            if signed and value > half:
                value -= modulus
            out.append(value)
        return out

    def drop(self, slot):
        self._send(DROP, slot)
        self.used[slot] = 0
        self.magnitude[slot] = 0

    def close(self):
        if self.served is not None and self.served.poll() is None:
            try:
                self._send(CLOSE)
                self.served.stdin.close()
                self.served.wait(timeout=10)
            except Exception:
                self.served.kill()
        self.served = None

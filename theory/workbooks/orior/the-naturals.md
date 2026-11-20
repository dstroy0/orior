# The naturals, to a thousand places, each one agreed by two routes

Seventy-four constants, every one of them verified to all thousand places it is published at, and
the table refuses to be written if that is not true of every row.

The numbers live in `utils/maint/dn_precision/dn_const/dn_constants.csv`. The tool that computes them is
`utils/maint/dn_precision/calc/dn_constants.py`. The guard that decides whether they may ship is
`utils/maint/dn_precision/support/page_guard.py`. This page is the argument; those three are the work.

## Two routes or it does not ship

A constant computed once is a constant nobody has checked. A single series with a transcribed error
returns a plausible number and no amount of rereading the code finds it, because the code says what
it was written to say. So every row carries two computations and the gap between them.

| group                  | first route                        | second route                               | what the pair actually tests                                 |
| ---------------------- | ---------------------------------- | ------------------------------------------ | ------------------------------------------------------------ |
| pi and its multiples   | Chudnovsky, binary splitting       | Machin, `16 arctan(1/5) - 4 arctan(1/239)` | two different series, different denominators, different base |
| square roots           | integer Newton on a scaled integer | squaring the answer back                   | **exact**, and it decides, and does not measure              |
| cube roots             | integer Newton, nth root           | raising the answer back                    | the same, and these are the SHA-256 round constants          |
| logarithms of naturals | one artanh decomposition           | a second sharing no term                   | two identities, each checkable by eye                        |
| zeta of even integers  | `pi^k / c` from Chudnovsky         | the same from Machin                       | inherits pi's independence                                   |
| zeta(3)                | a BBP-type series, binary scaled   | `(5/2) sum (-1)^(k+1)/(k^3 C(2k,k))`       | catches a misremembered leading factor                       |

**A root does not get a second series and pretending otherwise was this tool's first defect.** It
claimed one and then called the same routine twice, which grades nothing at all. There is no
independent series needed: `root^2 <= target < (root+1)^2` is an exact test with no tolerance in it,
and raising an nth root back is the same statement. Those rows are stronger than the ones with two
series, and not weaker.

**The logarithm identities, stated so they can be checked without running anything.** Each rests on
`artanh(1/m) = (1/2) ln((m+1)/(m-1))`:

| constant | route A                          | route B                                         |
| -------- | -------------------------------- | ----------------------------------------------- |
| ln 3     | `2 artanh(1/2)`                  | `4 artanh(1/5) + 2 artanh(1/7)`                 |
| ln 5     | `2 artanh(1/9) + 2 ln 2`         | `2 artanh(1/4) + 2 artanh(1/2)`                 |
| ln 7     | `6 artanh(1/3) - 2 artanh(1/15)` | `2 artanh(1/6) + 2 artanh(1/4) + 2 artanh(1/2)` |
| ln 10    | `3 ln 2 + 2 artanh(1/9)`         | `ln 2 + ln 5`, by route B above                 |

Nothing in that table is compared against digits typed into the source. Digits typed from memory are
how a wrong row ships. The pair therefore decides, and the table consults no prefix.

## No tolerance anywhere, because none is needed

Measured at 60, 250 and 1000 places: **every two-route gap in the file is exactly zero.** Every
threshold is therefore a decision, and not a bound.

An earlier version of these checks allowed a few units of slack. That is a bound chosen by judgment
sitting in front of every row, and a bound chosen by judgment admits precisely the one row that
drifted. One of the checks had it worst: `5 zeta(4) = 2 zeta(2)^2` is an exact identity, and the
first version of that check inflated a three-unit truncation by `10^digits` before comparing it
against a threshold in unscaled units, a correct identity read as a sixty-digit failure. The
repair was then to widen the threshold, which was worse than the arithmetic error. Computing both
sides at the guarded length and cutting gives zero.

## The page guard, which is a narrowing alarm and not an overflow guard

Nothing here can overflow. The arithmetic runs on Python integers, which have no bound, and it
runs on them for that reason. The failure worth guarding is the silent opposite:

|           | what happens                              | how it announces itself     |
| --------- | ----------------------------------------- | --------------------------- |
| overflow  | the value gets too large                  | raises or wraps, **loudly** |
| narrowing | the value keeps its width, loses its tail | **nothing at all**          |

A request for a thousand places that comes back agreeing to nine hundred still prints a thousand
digits, and the first nine hundred are right. There is no signal unless someone counts. Worse, it
compounds: the next constant computed from this one truncates again, and the place the loss finally
surfaces is nowhere near where it began.

**It caught a row that had been shipping as a footnote for as long as the table existed.**
`golden_angle` agreed to 999 of 1000 places, and the tool printed a line saying a few units in the
last place were the guard truncating. That was true and it was the wrong response. The composition
`pi (3 - sqrt 5)` truncates twice, once in the subtraction's operand and once in the product, and
computing it at the guarded width and cutting removes the loss entirely for the cost of one extra
Machin run. The footnote taught a reader to expect slack; the slack was real lost digits.

The guard refuses the write, and does not merely warn. It names every short row. Widening the working
length is the repair. Narrowing the request is not, because the request is what a caller relies on.

**It measures agreement, and not correctness.** Two routes agreeing to N places means N places
are verified. Two transcriptions of one wrong formula agree perfectly. Independence of the routes is
a reading of the mathematics, and no tool performs it. This is therefore the second of two gates, and never
the only one.

## What is in the table

| group                              | count | note                                                        |
| ---------------------------------- | ----- | ----------------------------------------------------------- |
| pi and its multiples and powers    | 6     | `pi`, `2pi`, `pi/2`, `4pi`, `pi^2`, and the placement angle |
| even zetas                         | 3     | `zeta(2)`, `zeta(4)`, `zeta(6)`                             |
| zeta(3)                            | 1     | no closed form in pi: two real series                        |
| square roots of the naturals to 30 | 24    | perfect squares excluded; they are rationals                |
| cube roots of the naturals to 30   | 27    | perfect cubes excluded                                      |
| logarithms                         | 6     | `ln 2`, `ln 3`, `ln 5`, `ln 7`, `ln 10`, `log2 10`          |
| the rest                           | 7     | `e`, `phi`, the harmonic unit, roots of 2, 3, 5             |

The table leaves out perfect powers deliberately. `sqrt 25` is 5, a rational, and a table of these carrying it
would be claiming a thousand places of something with one digit.

**The cube roots are not decoration.** The SHA-256 round constants are the leading fractional bits
of the cube roots of the first primes. That column is the published specification's own arithmetic,
computed here to a thousand places where the standard needs thirty-two bits. A
corrupted constant table anywhere in a tree is therefore caught, and not trusted.

## Euler-Mascheroni has two real routes

The first route is Brent-McMillan, a real implementation, and not a series summed casually. The
second independent route is Sweeney's exponential-integral method, a second real implementation. The
two fail differently: Brent-McMillan has all positive terms, and Sweeney loses about 1779 digits to
cancellation before it delivers one. They agree to the full width, and the row carries that agreement.

A row with one unverified number would be worse than no row, because the claim of the table is that
no row is unverified. One exception makes the other rows unreadable as a group.

## What arbitrary precision costs, since the cost is the evidence

**A constant that arrives instantly was looked up.** The only proof available from outside that a
number was computed is that it was paid for: a lookup is flat in the digit count and a computation
is not. So `--time` reports seconds and grades the growth against what each algorithm predicts from
its own shape, and not against a fit.

The spread between routes is large and it is the reason to care which one is used:

| route                     | digits per term    | terms for 1000 digits                  |
| ------------------------- | ------------------ | -------------------------------------- |
| Chudnovsky                | 14.18              | about 71                               |
| zeta(3), central binomial | 0.78               | about 1282                             |
| Machin, Gregory series    | one term per digit | about 1000, each a full-width division |

A constant with no fast series costs eighteen times the terms for the same output, and each term is
a full-precision division. That is the practical shape of the problem: the naturals with closed
forms in pi are nearly free once pi is paid for, and the ones without a rapidly convergent series
are where the time goes.

## Running the check

```
python utils/maint/dn_precision/support/page_guard.py --check
python utils/maint/dn_precision/calc/dn_constants.py --check
python utils/maint/dn_precision/calc/dn_constants.py --time
python utils/maint/dn_precision/calc/dn_constants.py --digits 1000
python utils/maint/dn_precision/support/dn_load.py --check
python utils/maint/dn_precision/support/dn_load.py --against-stdlib
```

The guard runs its own positive and negative controls: an exact match must not alarm, a one-unit gap
must cost exactly one place, and a clean set of rows must raise nothing. A guard that fires on
correct input gets switched off, and then it protects nothing.

The constants check grades every square and cube root by raising it back, grades each logarithm pair
against the other, and includes a route deliberately off by one to confirm the comparison can see a
difference at all.

`--against-stdlib` reports the narrow place where carrying our own constants buys anything at double
precision: **thirteen of fourteen library forms give bit-identical doubles**, and claiming a gain
there would be false. The one that differs is `math.pi * (3 - math.sqrt(5))`, which rounds three
times and lands 1.0 unit in the last place from the correctly rounded golden angle. That is the
constant every source in the placement is positioned by. Above sixteen digits the rule stops being a
preference and becomes a requirement, because `math.pi` is a double and a computation reaching for
it has silently capped itself.

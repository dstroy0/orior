# The removable uncertainty

Douglas, 2026-09-12: *"like shotgun all of their constant exponential and uncertainty we have
calculated down."*

The shotgun only fires on part of the target, and knowing which part is the result.

## The split

Counted from `utils/maint/dn_precision/dn_const/codata_2022_allascii.txt`, the 2022 CODATA listing as
fetched from NIST on 2026-09-11:

| class | count |
|---|---|
| constants in the listing | 358 |
| marked `(exact)` | 81 |
| of those, **terminating** | 19 |
| of those, **truncated** | **62** |
| carrying a measured uncertainty | **277** |

Three different kinds of number wear the same word.

**The 19 terminating ones need nothing.** `1.602 176 634 e-19` is the elementary charge, and that is
all. The digits printed are the value, exactly, with no tail. Computing them to a
thousand places would append 985 zeros.

**The 62 truncated ones are the target.** NIST prints them as `1.054 571 817... e-34`. The ellipsis
is not an uncertainty: it is a typesetting decision. Every one is a closed form in the seven SI
defining constants, and those are exact **by definition** since the 2019 redefinition. Their
absent digits are unprinted, and not unknown.

**The 277 measured ones are permanent and no arithmetic touches them.** The fine-structure constant,
the gravitational constant, the electron mass. Their uncertainty is what a laboratory measured.
Computing α to a thousand places would be computing a thousand places of a number known to eleven,
and any claim that precision reduces an experimental error is false.

So the honest statement is not that the uncertainty was removed. It is: **which uncertainties are
mathematical and removable, and which are physical and permanent.** That division is not published
anywhere and it falls straight out of the listing.

## What was carried out

44 of the 62, at 1000 places, in `utils/maint/dn_precision/dn_const/codata_exact.csv`.

| verification | count |
|---|---|
| exactly verified by multiplying back | **38** |
| verified against this tree's own π | **6** |
| agreeing with the NIST printed digits | **44 of 44** |

Examples, first 40 places of each:

| constant | value |
|---|---|
| electron volt-hertz relationship | 2.41798924208491816223829142527264067676 |
| electron volt-inverse meter relationship | 8.06554393734921164106900723057095944944 |
| electron volt-kelvin relationship | 1.16045181215500826060787354352916635582 |

The other 18 need extra definitions, and not extra arithmetic: the conventional -90
values, the Loschmidt constants and molar volumes at stated gas conditions, the radiation constants,
Stefan-Boltzmann which needs π⁵, and the two Wien displacement constants, which need the root of
`x = 5(1 - e^-x)` and are therefore the only ones here that are not a closed form in the defining
constants at all.

## Why the verification is stronger than the rest of the precision table

`dn_constants.csv` lives by two routes agreeing to a zero gap. That is evidence, and two routes can
share an error.

A pure rational admits something better. If p/q is reported as v at N places, then

> v·q ≤ p·10^N < (v+1)·q

is an exact integer statement with no arithmetic in it that could go wrong. Either it holds or the
digits are wrong. That is **verification**, and not agreement, and it is available for the 38
constants that do not involve π.

The seven defining constants are integers times powers of ten:

| constant | exact value |
|---|---|
| caesium hyperfine frequency | 9 192 631 770 Hz |
| speed of light | 299 792 458 m/s |
| Planck constant | 662607015 × 10⁻⁴² J s |
| elementary charge | 1602176634 × 10⁻²⁸ C |
| Boltzmann constant | 1380649 × 10⁻²⁹ J/K |
| Avogadro constant | 602214076 × 10¹⁵ mol⁻¹ |
| luminous efficacy | 683 lm/W |

Everything built from those by field operations is a rational. This can be
exact at all for that reason.

## The error the exact check could not catch

`elementary charge over h-bar` came out wrong on the first run, and the way it was caught is the
argument for keeping an external check even when an internal one is exact.

e/ħ = e/(h/2π) = **2π**e/h. Every other π constant in the set has ħ *as* its value and therefore
divides by 2π; this one has ħ in its **denominator** and multiplies. A division here, with a
spurious factor of two besides, would come out low by (2π)² = 19.74.

**The exact check would pass it.** A multiplication back verifies a correct division of a rational,
and says nothing about whether it is the right rational. An exact check cannot see a correctly
computed wrong expression. The NIST comparison catches it.

That is the same shape as the lesson in [what a reading conserves](what-a-reading-conserves.md): an
instrument's blindness is a property of the instrument, and it must be known, and not hoped
about.

## Apparatus

- `utils/maint/dn_precision/calc/codata_exact.py`, the 44, with the exact check and the NIST comparison
- `utils/maint/dn_precision/dn_const/codata_exact.csv`, the values at 1000 places
- `utils/maint/dn_precision/dn_const/codata_2022_provenance.tsv`, where the listing came from and when
- `utils/maint/dn_precision/calc/proved_digits.py`, the derived error bound for the series routes
- `utils/maint/dn_precision/calc/reproduce.py`, the reproduction set

Related: [the naturals](the-naturals.md).

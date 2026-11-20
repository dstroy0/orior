"""What the WIDTH of an eye is worth, and what the position of a source is worth to know.

    `python examples/00_blob_viz_tools/fractal_spike.py`            the ladder, the nulls, and the width sweep
    `python examples/00_blob_viz_tools/fractal_spike.py --check`    grade the instrument before any number counts

THE QUESTION:

docs-check: quoting
    "we can represent this problem however we want in as many dimensions as we want and if a qubit
     knows its position and a null permutation gives identity then the demon arm sweeping infinite
     space"
    "If it's the two of them there s a fundamental integral we don't know"
    "Other path is a macro fractal spike"
    "The shape if were 3d looks like a jellyfish with 4 balloons"
    "It's like four fractal spikes between those at oblique angles from the balloons and a larger
     spike in the center"
docs-check: end quoting

WHAT IS ALREADY SETTLED, AND THIS FILE DOES NOT ARGUE IT AGAIN. The stacked reading closes: arms take the rank to
the SOURCE COUNT. The measurements at 64, 128, 256, 512 and 1024 give arms-to-closure equal to sources - 81 every
time. The 81 is the own rank of the degree-8 harmonic block. No rank is absent, and the reason is not empirical.

At bandlimit L, every function on the sphere lives in a space of (L+1)^2 dimensions. EVERY reading of ANY type is a
linear functional on that space. That holds for a region integral, a line integral, a point evaluation, or an
integral against any measure whatever. The span of all conceivable integrals is therefore exactly (L+1)^2, and no
integral that nobody thought of yet exceeds it. Beams added 175 dimensions at degree 8, and not because a line
integral escapes the space. The region rows did not span it: 81 + 175 = 256.

"A FUNDAMENTAL INTEGRAL WE DON'T KNOW" THEREFORE HAS NO ROOM TO EXIST at fixed degree. A count of dimensions closes
that branch, and this file does not test it. The other branch is wide open, because rank is not the scarce thing.
CONDITIONING is, and the difference shows twice:
  - degree 15 is complete at a least singular value of 3.5e-3, against degree 16 at 1.5e-1.
  - at the orthogonal spiral offset, the concurrent set comes out best conditioned at 8.18e+02 against 3.74e+09.
    That is seven orders of magnitude at IDENTICAL rank.

WHAT THIS FILE MEASURES, in the order that the measurements force it.

1. THE MACRO LADDER LOSES. Widths double from the source spacing to the shell diameter in four rungs.
   That is where the four balloons come from, and the number follows: log2(9.02) = 3.17. The ladder
   conditions at best 2.4e+03, against 6.0e+01 for the single scale already in use. Every variant of
   the ladder is worse than none. The count exponent sweeps over 0, 1 and 2 with no pick, and all
   three lose.

2. THE SELF-SIMILAR LAYOUT IS REAL BUT SMALL. Its own null is the same multiset of widths with the
   assignment shuffled. Against it, the ladder wins by four orders of magnitude at exponent 0, and
   loses at exponent 1. Layout therefore matters, and is worth a factor of a few. Width is worth a
   factor of ten million. Layout is the second-order term.

3. WIDTH IS ALL OF THE EFFECT, AND THE DERIVED FLOOR IS THE WRONG BOUND. A delta limit at the source
   spacing would make the shipped BEAM_WIDTH of 0.10 a defect, because it sits below it. The
   measurement says that it works because it sits below it. A near-delta row is nearly a row of the
   identity, and a stack of them is nearly orthogonal. A wide row is nearly the monopole, and a stack
   of those is nearly rank one. Narrow wins by construction.

4. AND THEN THE NULL PICKS UP THE TURF. The narrow sweep reports condition 4.86 at width 0.003, and
   the rank never breaks. That number measures the AIM, and not the instrument. `beam_set` aims beam
   `at` at source `(at * 7 + 1) % 256`, and 7 is coprime to 256. The aim is a PERMUTATION: one beam
   points at each source. Shrink the width, and the matrix becomes a permuted identity for free. The
   aimed instrument comes from the answer.

THE RESULT THIS FILE EXISTS FOR is the gap between those two aims. It is the price of the first clause of the
question, "if a qubit knows its position":

docs-check: quoting
    width / spacing      aimed cond    blind cond    blind rank
              1.000       2.77e+02      3.44e+02           256
              0.707        5.40e+01      8.32e+01           256     <- shipped BEAM_WIDTH
              0.500       2.45e+01      6.23e+01           256     <- the blind optimum
              0.250       1.01e+01      2.74e+02           256
              0.088       6.35e+00      6.10e+08           256
              0.022       4.86e+00      1.28e+13           130
docs-check: end quoting

The aimed column slides to 4.86 and never breaks. The blind column has a genuine MINIMUM at half the source
spacing. It then collapses eleven orders of magnitude, and takes 126 sources with it into rows that touch nothing at
all. The known position is worth 13x in conditioning, and all of the difference between a complete reading and an
object a third invisible. That is the null permutation with a number on it, and the price comes in advance. The aim
IS the position, and an instrument that uses it holds what it must find.

THE SHIPPED WIDTH IS A QUARTER TOO WIDE, found blind. 0.10 conditions at 8.32e+01 and 0.0687 conditions at 6.23e+01,
both complete, and neither uses a source position.

WHAT THIS FILE DOES NOT TEST, and the obvious next thing, is the interleaved oblique arrangement. That is four narrow
sets in the GAPS of the wide ones, and not along the same golden directions. Every rung in this file draws from the
same golden placement at its own count. The wide and narrow rungs point down nearly the same lines, and the wide rungs
are largely redundant. That is a real defect in the ladder as built, and the likely reason that layout came out
second-order.
"""

import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import numpy

import arm_rank
import beam_rows
import boundary_read

TOTAL = beam_rows.COUNT

# How many independent blind aims the code grades each width over. Three is the smallest number that gives a RANGE and
# not a point, and the report gives the range. A single blind draw is a single shot, and a single shot is not a result.
BLIND_SEEDS = (3, 11, 29)


def limits():
    """(delta limit, monopole limit), the two ends of the available width range.

    THE SOURCES FORCE BOTH. They sit on a shell of radius DEPTH, COUNT of them. The area per source is 4 pi DEPTH^2 /
    COUNT, and the typical spacing is its root. Below that, a beam reads one source and is a point probe. Above the
    diameter of the shell, it reads every source at nearly equal weight, and is a monopole with one number.

    NEITHER END IS A USABILITY BOUND, AND THE LOWER ONE IN PARTICULAR IS NOT. See the module docstring. Conditioning
    improves straight through the delta limit. What actually stops it is a source that falls BETWEEN beams, which is a
    rank floor, measured and not derived.
    """
    spacing = beam_rows.DEPTH * math.sqrt(4.0 * math.pi / beam_rows.COUNT)
    return spacing, 2.0 * beam_rows.DEPTH


def ladder(exponent):
    """(width, count) per rung: widths that double from the delta limit, counts as width^-exponent.

    THE RUNG COUNT FOLLOWS AT BOTH ENDS. Widths double from the delta limit until they pass the monopole limit. The
    count is floor(log2(diameter / spacing)) + 1, and nothing is a choice. At COUNT = 256 on a shell of 0.62, the ratio
    is 9.02, and the ladder has FOUR rungs.

    THE COUNT EXPONENT IS AN ARGUMENT, SWEPT AND NOT PICKED. Three values are defensible, and they disagree. A choice of
    one would be the judgment that this tree does not allow:
      - 0: equal beams per rung. Every scale gets the same look.
      - 1: equal total beam thickness per rung. No scale dominates the reading.
      - 2: equal area coverage. A tiling of a 2-sphere with patches of size w takes area / w^2 of them, the literal
        self-similar tiling.

    The function reports all three. A result that depends on which one IS the result.
    """
    spacing, widest = limits()
    widths = []
    width = spacing
    while width <= widest:
        widths.append(width)
        width *= 2.0

    weights = [(spacing / one) ** exponent for one in widths]
    share = sum(weights)
    counts = [max(1, int(round(TOTAL * one / share))) for one in weights]

    # The rounding remainder goes on the narrowest rung, which holds the max beams. A single
    # beam changes it least.
    counts[0] = max(1, counts[0] + TOTAL - sum(counts))
    return list(zip(widths, counts)), spacing


def spike_plan(rungs, shuffle_seed=None):
    """(origin, direction, width) per beam on the ladder, or with the widths shuffled.

    WITH A SEED, THE CODE PERMUTES THE WIDTHS ACROSS ALL THE DIRECTIONS. That keeps the multiset of widths exactly. It
    destroys the correspondence between the width of a beam and its place in the layout, the only thing that
    "self-similar" adds over "multi-scale". Without this null, self-similarity gets the credit for any gain, even where
    any spread of widths would do the same.

    THE FUNCTION RETURNS THE PLAN APART FROM THE ROWS, and `--check` can then test the invariant that the null actually
    claims. The weights SHOULD differ: a wide beam pointed somewhere dense picks up more. The multiset of widths must
    not. A row COUNT and a difference in total weight are the wrong pair.
    """
    plan = []
    for width, count in rungs:
        origins = boundary_read.golden_place(count)
        targets = boundary_read.golden_place(count)
        for at in range(count):
            start = tuple(beam_rows.ORIGIN_RADIUS * one for one in origins[at])
            aim = tuple(beam_rows.DEPTH * one for one in targets[(at * 7 + 1) % count])
            plan.append((start, tuple(a - b for a, b in zip(aim, start)), width))

    if shuffle_seed is not None:
        generator = numpy.random.default_rng(shuffle_seed)
        widths = [width for _, _, width in plan]
        generator.shuffle(widths)
        plan = [(start, direction, widths[at])
                for at, (start, direction, _) in enumerate(plan)]
    return plan


def spike_set(points, rungs, shuffle_seed=None):
    """The ladder's rows over the sources, from `spike_plan`."""
    return numpy.array([beam_rows.beam_row(start, direction, points, width=width)
                        for start, direction, width in spike_plan(rungs, shuffle_seed)])


def one_scale(points, count, width, blind=False, seed=0):
    """`count` beams at a single width. `blind` aims them WITHOUT a source position.

    THE DEFAULT AIM KNOWS THE ANSWER, AND THAT VOIDS ANY NARROW-BEAM RESULT. `targets` is the same golden placement as
    the sources, and the step 7 is coprime to 256. The aim is a permutation with exactly one beam per source. Narrow the
    width, and the matrix becomes a permuted identity by construction: conditioning goes to 1, and the rank cannot
    break. The number then measures the aim.

    The blind control draws its aim directions apart from the sources. A narrow beam misses, its row goes to zero, that
    source becomes unreadable, and the rank falls. That is the real floor, and the gap between the two columns is what
    the position is worth.
    """
    origins = boundary_read.golden_place(count)
    if blind:
        generator = numpy.random.default_rng(seed)
        aims = generator.normal(size=(count, 3))
        aims = aims / numpy.linalg.norm(aims, axis=1, keepdims=True)
        targets = [tuple(row) for row in aims]
    else:
        targets = boundary_read.golden_place(count)

    rows = []
    for at in range(count):
        start = tuple(beam_rows.ORIGIN_RADIUS * one for one in origins[at])
        pick = at if blind else (at * 7 + 1) % count
        aim = tuple(beam_rows.DEPTH * one for one in targets[pick])
        direction = tuple(a - b for a, b in zip(aim, start))
        rows.append(beam_rows.beam_row(start, direction, points, width=width))
    return numpy.array(rows)


def gap_filling(already, count, pool=8):
    """`count` directions placed greedily in the GAPS of `already`, by farthest-point selection.

    Each pick maximizes the MINIMUM angle to all that is already placed, the deterministic form of "between those".
    Candidates come from a golden set `pool` times larger than the number wanted. The request sets the pool, and no
    constant does.

    THIS COSTS NO KNOWLEDGE OF THE OBJECT. The aimed permutation that reaches condition 4.857e+00 uses the SOURCE
    positions: it holds the answer. This uses only the directions of the beams already placed, which the instrument
    chose itself. A gain here is therefore free in a way that the aimed gain never was, and it stays legal under the
    blind control.
    """
    candidates = numpy.array(boundary_read.golden_place(count * pool), dtype=float)
    taken = numpy.zeros(len(candidates), dtype=bool)

    # THE CODE CARRIES THE NEAREST-PLACED COSINE FORWARD, AND DOES NOT COMPUTE IT AGAIN. A scan of every candidate
    # against every placed direction on every pick is O(count^2 * pool) dot products in Python, and does not finish.
    # Each pick only ever RAISES the nearest cosine of a candidate. One max against the newly placed direction updates
    # the whole column.
    if len(already):
        nearest = candidates.dot(numpy.array(already, dtype=float).T).max(axis=1)
    else:
        nearest = numpy.full(len(candidates), -1.0)

    picked = []
    for _ in range(count):
        masked = numpy.where(taken, numpy.inf, nearest)
        at = int(numpy.argmin(masked))
        taken[at] = True
        chosen = candidates[at]
        picked.append(tuple(chosen))
        nearest = numpy.maximum(nearest, candidates.dot(chosen))
    return picked


def oblique_set(points, rungs, offset, shuffle_seed=None, straight=False):
    """The jellyfish: wide rungs on golden directions, narrow rungs in their gaps, aimed obliquely.

    THE DEFECT THIS FIXES IS REAL. Every rung of `spike_set` draws from the same golden placement at its own count, and
    golden_place(17) shares its first directions with golden_place(137). The wide balloons and the narrow spikes point
    down nearly the same lines. The wide rungs are therefore largely redundant, the likely reason that the layout effect
    comes out second-order.

    `offset` is the impact parameter as a fraction of the shell radius: 0 aims through the center, and 1 grazes the
    shell. The caller SWEEPS it, and nothing picks it here. `straight` aims every beam at the center instead, which
    isolates obliquity from the fill of gaps.

    The center spike is one beam of the narrowest width straight through the origin. It is the longest chord available,
    and the only row in the set that reads every shell crossing.
    """
    widest = max(width for width, _count in rungs)
    plan = []
    placed = []

    for width, count in sorted(rungs, key=lambda one: -one[0]):
        if width == widest:
            aims = [tuple(one) for one in boundary_read.golden_place(count)]
        else:
            aims = gap_filling(placed, count)
        placed.extend(numpy.array(one, dtype=float) for one in aims)

        origins = boundary_read.golden_place(count)
        for at in range(count):
            start = tuple(beam_rows.ORIGIN_RADIUS * one for one in origins[at])
            axis = numpy.array(aims[at], dtype=float)
            if straight:
                target = numpy.zeros(3)
            else:
                # Push the aim point sideways off the axis, which tilts the ray so it crosses the
                # shell obliquely instead of running down a radius.
                side = numpy.cross(axis, numpy.array(start, dtype=float))
                length = float(numpy.linalg.norm(side))
                side = side / length if length > 0.0 else numpy.array([1.0, 0.0, 0.0])
                target = offset * beam_rows.DEPTH * side
            direction = tuple(target - numpy.array(start, dtype=float))
            plan.append((start, direction, width))

    narrowest = min(width for width, _count in rungs)
    plan[-1] = ((beam_rows.ORIGIN_RADIUS, 0.0, 0.0), (-1.0, 0.0, 0.0), narrowest)

    if shuffle_seed is not None:
        generator = numpy.random.default_rng(shuffle_seed)
        widths = [width for _, _, width in plan]
        generator.shuffle(widths)
        plan = [(start, direction, widths[at])
                for at, (start, direction, _) in enumerate(plan)]

    return numpy.array([beam_rows.beam_row(start, direction, points, width=width)
                        for start, direction, width in plan])


def measure(harmonics, beams, truth):
    """Rank, condition number and relative recovery error for one stacked reading."""
    matrix = harmonics if beams is None else numpy.vstack([harmonics, beams])
    rank, values = arm_rank.rank_of(matrix)
    least = float(values[rank - 1]) if rank else 0.0
    condition = float(values[0] / least) if least > 0.0 else float("inf")
    reading = matrix.dot(truth)
    recovered, _residual, _rank, _singular = numpy.linalg.lstsq(matrix, reading, rcond=None)
    error = float(numpy.linalg.norm(recovered - truth)) / float(numpy.linalg.norm(truth))
    return rank, condition, error


def grade(name, harmonics, beams, truth):
    rank, condition, error = measure(harmonics, beams, truth)
    print("  %-30s %6d %13.3e %14.4e" % (name, rank, condition, error))
    return rank, condition, error


def _binary_truth(seed=41):
    """The object is a random BINARY state, because a bit pattern is one."""
    return numpy.random.default_rng(seed).integers(0, 2, size=beam_rows.COUNT).astype(float)


def _ladders(harmonics, points, truth, spacing, baseline):
    print("  %-30s %6s %13s %14s" % ("reading set", "rank", "condition", "recovery error"))
    verdicts = []
    for exponent in (0, 1, 2):
        rungs, _ = ladder(exponent)
        mean_width = (sum(width * count for width, count in rungs)
                      / sum(count for _, count in rungs))
        print("  %s" % ("-" * 66))
        print("  exponent %d, mean width %.5f, rungs %s"
              % (exponent, mean_width,
                 " ".join("%dx%.4f" % (count, width) for width, count in rungs)))
        single = grade("one scale at that mean width", harmonics,
                       one_scale(points, TOTAL, mean_width), truth)
        spike = grade("FRACTAL SPIKE exponent %d" % exponent, harmonics,
                      spike_set(points, rungs), truth)
        band = [grade("  null: widths shuffled %d" % seed, harmonics,
                      spike_set(points, rungs, shuffle_seed=seed), truth)[1]
                for seed in (7, 19, 23)]
        verdicts.append((exponent, single[1], spike[1], band))

    print("")
    print("  THE LADDER VERDICT, off the condition column. The shipped single scale is %.3e."
          % baseline[1])
    for exponent, single, spike, band in verdicts:
        if spike < min(single, baseline[1]) and spike < min(band):
            note = "multi-scale AND the self-similar layout both help"
        elif spike < min(single, baseline[1]):
            note = "the width SPREAD helps, the layout does not"
        elif spike < min(band):
            note = "beats its own shuffle but loses to one scale"
        else:
            note = "NO, one scale is as good or better and the ladder buys nothing"
        print("    exponent %d  spike %.3e  shuffled %.3e to %.3e  %s"
              % (exponent, spike, min(band), max(band), note))
    print("")


def _widths(harmonics, points, truth, spacing, baseline):
    print("  %-30s %6s %6s %11s %11s %11s"
          % ("one scale at width", "aimed", "blind", "aimed cond", "blind cond", "blind err"))

    walk = []
    width = spacing
    while width > spacing / 64.0:
        aimed_rank, aimed_condition, _err = measure(harmonics,
                                                    one_scale(points, TOTAL, width), truth)
        blind = [measure(harmonics, one_scale(points, TOTAL, width, blind=True, seed=seed), truth)
                 for seed in BLIND_SEEDS]
        print("  %-30s %6d %6d %11.3e %11.3e %11.3e"
              % ("%.5f  (%5.3f spacings)" % (width, width / spacing),
                 aimed_rank, min(one[0] for one in blind), aimed_condition,
                 max(one[1] for one in blind), max(one[2] for one in blind)))
        walk.append((width, aimed_rank, aimed_condition,
                     min(one[0] for one in blind), max(one[1] for one in blind)))
        width /= math.sqrt(2.0)

    print("")
    complete = [one for one in walk if one[3] == beam_rows.COUNT]
    broke = [one for one in walk if one[3] < beam_rows.COUNT]
    print("    AIMED best %.3e at width %.5f, and the rank NEVER breaks, because the aiming is a"
          % (min(one[2] for one in walk),
             min(walk, key=lambda one: one[2])[0]))
    print("    permutation onto the sources. That column measures the aiming instead of the instrument.")
    if complete:
        best = min(complete, key=lambda one: one[4])
        print("    BLIND best %.3e at width %.5f  (%.2f spacings), against %.3e for the shipped"
              % (best[4], best[0], best[0] / spacing, baseline[1]))
        print("    BEAM_WIDTH of %.5f  (%.2f spacings). The shipped width is too wide."
              % (beam_rows.BEAM_WIDTH, beam_rows.BEAM_WIDTH / spacing))
    if broke:
        edge = max(broke, key=lambda one: one[0])
        print("    BLIND rank first breaks at %.5f  (%.2f spacings), rank %d of %d: past there a"
              % (edge[0], edge[0] / spacing, edge[3], beam_rows.COUNT))
        print("    source falls between beams and has no row touching it at all.")
    print("")


def _report():
    points = beam_rows.source_points()
    harmonics = beam_rows.harmonic_rows()
    spacing, widest = limits()
    truth = _binary_truth()
    rungs, _ = ladder(1)

    print("")
    print("  THE MACRO FRACTAL SPIKE, against every null that would explain it away.")
    print("")
    print("  delta limit     %.5f   source spacing on the shell" % spacing)
    print("  monopole limit  %.5f   shell diameter" % widest)
    print("  range           %.2f x. Log2 gives %d rungs and the four balloons are derived"
          % (widest / spacing, len(rungs)))
    print("  shipped width   %.5f   which is %.3f spacings, already below the delta limit"
          % (beam_rows.BEAM_WIDTH, beam_rows.BEAM_WIDTH / spacing))
    print("")

    baseline = grade("one scale, shipped BEAM_WIDTH", harmonics,
                     one_scale(points, TOTAL, beam_rows.BEAM_WIDTH), truth)
    print("")
    _ladders(harmonics, points, truth, spacing, baseline)
    print("  %s" % ("=" * 66))
    print("")
    print("  WHERE NARROW STOPS, and whether the answer survives not knowing a source position.")
    print("")
    _widths(harmonics, points, truth, spacing, baseline)
    print("  WHAT NO ARRANGEMENT CAN DO. The rank column never exceeds %d and cannot: every")
    print("  reading is a linear functional on a space of that dimension. No integral nobody")
    print("  has thought of yet adds a direction to it. Conditioning is all that is on the table.")
    print("")
    return 0


def _check():
    lines = []
    failed = 0

    points = beam_rows.source_points()
    harmonics = beam_rows.harmonic_rows()
    spacing, widest = limits()
    truth = _binary_truth()

    # THE LADDER'S SHAPE MUST FOLLOW FROM THE GEOMETRY, AND NOT DRIFT. Four rungs is log2(diameter/spacing). If the
    # source count or shell depth changes, this number must change with it, and not stay at a fixed four.
    rungs, _ = ladder(1)
    want = int(math.log(widest / spacing, 2.0)) + 1
    lines.append("  ladder rungs %d, derived %d, from a range of %.3f x"
                 % (len(rungs), want, widest / spacing))
    if len(rungs) != want:
        lines.append("    FAIL the rung count is not the derived one")
        failed += 1

    # Every exponent must place exactly TOTAL beams. Otherwise the sets under comparison differ in row count, and the
    # comparison is between two different instruments.
    for exponent in (0, 1, 2):
        placed = sum(count for _, count in ladder(exponent)[0])
        lines.append("  exponent %d places %d beams" % (exponent, placed))
        if placed != TOTAL:
            lines.append("    FAIL the beam count is not %d, so the sets are not comparable"
                         % TOTAL)
            failed += 1

    # THE SHUFFLE NULL MUST PRESERVE THE WIDTHS EXACTLY. If it does not, it is a different set and
    # not a null, and any difference it shows is uninterpretable.
    rungs, _ = ladder(1)
    plain = spike_plan(rungs)
    shuffled = spike_plan(rungs, shuffle_seed=7)
    plain_widths = sorted(width for _, _, width in plain)
    shuffled_widths = sorted(width for _, _, width in shuffled)
    moved = sum(1 for one, two in zip(plain, shuffled) if one[2] != two[2])
    lines.append("  shuffle null: %d beams, width multiset identical %s, %d beams changed width"
                 % (len(plain), plain_widths == shuffled_widths, moved))
    if plain_widths != shuffled_widths:
        lines.append("    FAIL the null does not hold the same widths, so it is a different set")
        lines.append("         and not a null, and any difference it shows is uninterpretable")
        failed += 1
    if moved == 0:
        lines.append("    FAIL the shuffle moved nothing, so the null is a copy of the set and")
        lines.append("         cannot distinguish layout from width spread")
        failed += 1

    # THE AIMED AIMING MUST ACTUALLY BE A PERMUTATION, since the whole aimed-versus-blind reading
    # rests on it. 7 must be coprime to COUNT.
    reached = sorted((at * 7 + 1) % TOTAL for at in range(TOTAL))
    lines.append("  aimed targeting hits %d distinct sources of %d" % (len(set(reached)), TOTAL))
    if reached != list(range(TOTAL)):
        lines.append("    FAIL the aimed targeting is not a permutation, so the aimed column is")
        lines.append("         not measuring what the docstring says it measures")
        failed += 1

    # THE POSITIVE CONTROL FOR THE BLIND NULL. At the widest usable width, a blind aim must still be COMPLETE. Otherwise
    # the null is broken and not informative, and every collapse that it reports is an artifact of the aim code.
    rank, condition, _error = measure(harmonics,
                                      one_scale(points, TOTAL, spacing, blind=True, seed=3), truth)
    lines.append("  blind aiming at the delta limit: rank %d of %d, condition %.3e"
                 % (rank, TOTAL, condition))
    if rank != TOTAL:
        lines.append("    FAIL a blind aiming cannot reach full rank even at the widest width, so")
        lines.append("         the blind collapse reported at narrow widths proves nothing")
        failed += 1

    # THE NEGATIVE CONTROL. A blind aim far below the floor must NOT be complete. Without this, the claim "the rank
    # breaks" has no counterpart, and the sweep could report noise.
    rank, _condition, _error = measure(
        harmonics, one_scale(points, TOTAL, spacing / 64.0, blind=True, seed=3), truth)
    lines.append("  blind aiming at 1/64 of the spacing: rank %d of %d" % (rank, TOTAL))
    if rank >= TOTAL:
        lines.append("    FAIL a beam far narrower than the spacing still sees every source, so")
        lines.append("         the beam row is not localized and the width means nothing")
        failed += 1

    # AND THE AIMED CASE MUST NOT BREAK THERE, the contrast the result rests on.
    rank, condition, _error = measure(harmonics, one_scale(points, TOTAL, spacing / 64.0), truth)
    lines.append("  aimed at 1/64 of the spacing: rank %d of %d, condition %.3e"
                 % (rank, TOTAL, condition))
    if rank != TOTAL:
        lines.append("    FAIL the aimed case breaks too, so the gap between aimed and blind is")
        lines.append("         not the value of knowing a position")
        failed += 1

    # TWO CODE PATHS THAT MUST AGREE EXACTLY. `offset = 0` aims at 0 * side, the origin, and `straight = True` aims at
    # the origin directly. The two are separate code, and reach the same beam. They must produce bit-identical rows. If
    # they drift, the obliquity result compares two instruments and not two aims, and 12.69x of it is an artifact.
    rungs, _ = ladder(2)
    by_offset = oblique_set(points, rungs, 0.0)
    by_flag = oblique_set(points, rungs, 0.5, straight=True)
    drift = float(numpy.abs(by_offset - by_flag).max())
    lines.append("  offset 0 against the central-aim flag: worst row difference %.3e" % drift)
    if drift != 0.0:
        lines.append("    FAIL the two ways of aiming at the center disagree, so the obliquity")
        lines.append("         comparison is between two instruments and not two aimings")
        failed += 1

    # GAP FILL MUST ACTUALLY FILL GAPS. Directions chosen in the gaps of a placed set must sit further from it than a
    # fresh golden set of the same size would. Otherwise "between those" is a name for something that the code does not
    # do.
    base = boundary_read.golden_place(64)
    filled = numpy.array(gap_filling(base, 16), dtype=float)
    naive = numpy.array(boundary_read.golden_place(16), dtype=float)
    placed = numpy.array(base, dtype=float)
    filled_angle = math.degrees(math.acos(min(1.0, float(filled.dot(placed.T).max()))))
    naive_angle = math.degrees(math.acos(min(1.0, float(naive.dot(placed.T).max()))))
    lines.append("  gap filling: 16 placed at %.2f deg from the 64, a fresh golden set at %.2f deg"
                 % (filled_angle, naive_angle))
    if filled_angle <= naive_angle:
        lines.append("    FAIL gap-filled directions are no further from the placed set than a")
        lines.append("         fresh golden set, so the selection is not filling gaps")
        failed += 1

    # THE CEILING. No reading set may exceed the source count, the claim of the docstring that counts dimensions. If one
    # does, that claim is wrong, and so is the conclusion.
    everything = numpy.vstack([harmonics, spike_set(points, rungs),
                               one_scale(points, TOTAL, beam_rows.BEAM_WIDTH)])
    rank, _values = arm_rank.rank_of(everything)
    lines.append("  harmonics plus %d beams of every type: rank %d, ceiling %d"
                 % (everything.shape[0] - harmonics.shape[0], rank, TOTAL))
    if rank > TOTAL:
        lines.append("    FAIL the rank exceeded the source count, so the dual space is bigger")
        lines.append("         than the dimension count says and the whole argument is wrong")
        failed += 1

    print("")
    for line in lines:
        print(line)
    print("")
    print("%d check(s) failed" % failed)
    print("")
    return 1 if failed else 0


def _oblique():
    """The interleaved oblique jellyfish, against the co-aligned ladder and three nulls."""
    points = beam_rows.source_points()
    harmonics = beam_rows.harmonic_rows()
    spacing, _widest = limits()
    truth = _binary_truth()

    print("")
    print("  THE INTERLEAVED OBLIQUE JELLYFISH. Narrow rungs in the GAPS of the wide ones by")
    print("  farthest-point selection, aimed off-axis, with a center spike down the longest chord.")
    print("")
    print("  This uses only the directions the instrument already chose, never a source position,")
    print("  so unlike the aimed permutation a gain here is free and not circular.")
    print("")

    baseline = grade("one scale, shipped BEAM_WIDTH", harmonics,
                     one_scale(points, TOTAL, beam_rows.BEAM_WIDTH), truth)
    best_flat = None
    for exponent in (0, 1, 2):
        rungs, _ = ladder(exponent)
        flat = grade("co-aligned ladder, exponent %d" % exponent, harmonics,
                     spike_set(points, rungs), truth)
        if best_flat is None or flat[1] < best_flat[1]:
            best_flat = flat

    print("  %s" % ("-" * 66))
    results = []
    for exponent in (0, 1, 2):
        rungs, _ = ladder(exponent)
        for offset in (0.0, 0.25, 0.5, 0.75):
            spike = grade("interleaved exp %d, offset %.2f" % (exponent, offset), harmonics,
                          oblique_set(points, rungs, offset), truth)
            results.append((exponent, offset, spike))

    print("  %s" % ("-" * 66))
    print("  THE NULLS, on the best offset found above.")
    exponent, offset, spike = min(results, key=lambda one: one[2][1])
    rungs, _ = ladder(exponent)
    straight = grade("null: interleaved, aimed at center", harmonics,
                     oblique_set(points, rungs, offset, straight=True), truth)
    shuffled = [grade("null: widths shuffled %d" % seed, harmonics,
                      oblique_set(points, rungs, offset, shuffle_seed=seed), truth)[1]
                for seed in (7, 19, 23)]

    print("")
    print("  THE VERDICT.")
    print("    shipped one scale              %.3e" % baseline[1])
    print("    best co-aligned ladder         %.3e" % best_flat[1])
    print("    best interleaved oblique       %.3e  (exponent %d, offset %.2f)"
          % (spike[1], exponent, offset))
    print("    null, same set aimed centrally %.3e" % straight[1])
    print("    null, widths shuffled          %.3e to %.3e" % (min(shuffled), max(shuffled)))
    print("")

    # EVERY COMPARISON IS AGAINST THE NULL'S OWN SPREAD AND NOT AGAINST A BARE INEQUALITY. A
    # bare `<` reports a 1.03x difference as "interleaving helps" while the shuffled null's own
    # three seeds span 9%. A difference smaller than the control's seed-to-seed range is not a
    # finding.
    band = max(shuffled) / min(shuffled)
    print("    the shuffled null spans %.2fx across its own seeds. Nothing smaller than that"
          % band)
    print("    counts as an effect here.")
    print("")

    def verdict(label, against, note_yes, note_no):
        ratio = against / spike[1]
        if ratio > band:
            print("    %-14s %6.2fx  %s" % (label, ratio, note_yes))
        elif ratio < 1.0 / band:
            print("    %-14s %6.2fx  WORSE, and outside the null's spread" % (label, ratio))
        else:
            print("    %-14s %6.2fx  %s" % (label, ratio, note_no))

    verdict("obliquity", straight[1],
            "REAL: aiming the same directions at the center costs this much",
            "inside the null's spread, so the off-axis tilt is not displayed to do anything")
    verdict("interleaving", best_flat[1],
            "REAL against the co-aligned ladder",
            "NOT DISPLAYED: below the null's own spread, so the redundancy I blamed for the "
            "second-order layout effect is not demonstrated to be the cause")
    verdict("layout", min(shuffled),
            "REAL: the arrangement beats its own width-shuffle",
            "NOT DISPLAYED: the shuffle does as well, so the gain is the spread of widths")
    verdict("vs shipped", baseline[1],
            "beats the shipped single scale, which no ladder had managed",
            "matches the shipped single scale")
    print("")
    if spike[1] > baseline[1] * band:
        print("    THE SHIPPED SINGLE NARROW SCALE STILL WINS OUTRIGHT, by %.0fx. Every"
              % (spike[1] / baseline[1]))
        print("    arrangement measured here is a way of losing more slowly than the ladder.")
    print("")
    return 0


def main():
    if "--check" in sys.argv:
        return _check()
    if "--oblique" in sys.argv:
        return _oblique()
    return _report()


if __name__ == "__main__":
    sys.exit(main())

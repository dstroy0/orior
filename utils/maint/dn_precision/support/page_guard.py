"""Alarms when a computation returns fewer verified places than the caller asked for.

docs-check: quoting
    python utils/maint/dn_precision/support/page_guard.py --check

    from page_guard import agreed_places, require, survey
    agreed_places(route_one, route_two, 1000)    how many of 1000 places two routes agree to
    require("pi", route_one, route_two, 1000)    raises unless all 1000 agree
    survey(rows, 1000)                           the rows that narrowed, as (name, places)
docs-check: end quoting

THIS IS NOT AN OVERFLOW GUARD, AND THE DISTINCTION IS THE WHOLE REASON IT EXISTS

Nothing in this family can overflow. The arithmetic runs on Python integers, which have no bound, and it runs on them
for that reason. The failure that deserves a guard is the opposite one:
  - overflow: the value gets too large, and something raises or wraps. LOUD.
  - narrowing: the value keeps its width, and its tail becomes noise. SILENT.

A request for a thousand places that comes back with agreement to nine hundred still prints a thousand digits. The
leading nine hundred are right. Nothing about the output looks wrong, nothing raises, and a caller has no way to tell
which nine hundred to trust. There is no signal at all unless someone counts.

AND IT COMPOUNDS, AND THAT MAKES IT WORTH A STOP, AND NOT A NOTE

A narrowed constant then feeds the computation of the next one. Each composition truncates again, and the loss grows
down a chain that nobody watches. The place where it finally shows up is nowhere near the place where it started. This
therefore raises, and does not warn. A printed warning that the run survives teaches a reader to expect slack, and the
slack is real lost digits.

A row that narrows needs a fix, and not a footnote. Golden_angle at 999 of 1000 places, with a line that blames the
guard truncation, is a real narrowing. One extra Machin run at the guarded width removes it outright.

IT MEASURES AGREEMENT, AND NOT CORRECTNESS

Two routes that agree to N places make N places VERIFIED, and not necessarily right. Two transcriptions of one wrong
formula agree perfectly. This is therefore the second of two gates, and never the only one. The routes must be
independent for the count to mean anything. The judgement of independence is a reading of the mathematics that no tool
performs.

NO TOLERANCE ANYWHERE IN THIS FILE

There is no slack parameter, and none may enter. A threshold picked by judgement sits in front of every row, and admits
exactly the one that drifted, and the guard exists for that failure. Either the places agree, or the count of places in
agreement is the finding.
"""

import argparse
import sys


def agreed_places(first, second, digits):
    """How many of `digits` decimal places two scaled integers agree to.

    Both arguments hold `digits` places. The gap is an integer at that same scale. The number of places that agree is
    therefore `digits` less the number of digits in the gap, and an exact match agrees to all of them. A count of the
    digits of the gap avoids a division. It keeps any float out of a routine whose entire purpose is to protect
    precision.
    """
    gap = abs(first - second)
    if gap == 0:
        return digits
    lost = len(str(gap))
    return max(0, digits - lost)


def require(name, first, second, digits):
    """Returns `first` when both routes agree to all `digits` places, and raises otherwise.

    The raise is deliberate, and a shortened value is never the answer. A caller that asked for a thousand places needs
    them. Nine hundred in a container shaped for a thousand is the silent case that this module exists to prevent.
    """
    got = agreed_places(first, second, digits)
    if got < digits:
        raise NarrowingAlarm(
            "%s agrees to %d of the %d places requested, so %d places would be carried unverified. "
            "Widen the working length and recompute. Do not narrow the request: the request is what "
            "the caller will rely on." % (name, got, digits, digits - got))
    return first


class NarrowingAlarm(RuntimeError):
    """The error for fewer verified places than the caller asked for. It has its own name, so that a caller can catch
    this one error alone. A broad catch of RuntimeError would swallow it."""


def survey(rows, digits, places_key="agreed_digits", name_key="name"):
    """Every row with fewer verified places than `digits`, as (name, places) pairs.

    Takes the rows that a table is about to come from. The check then happens at the boundary where numbers become a
    file, and other tools read that file as authoritative.
    """
    return [(row[name_key], row[places_key]) for row in rows
            if row[places_key] < digits]


def refuse_if_narrowed(rows, digits, what="table"):
    """Raises NarrowingAlarm naming every short row, or returns the count checked."""
    short = survey(rows, digits)
    if short:
        raise NarrowingAlarm(
            "NARROWING ALARM: %d of %d rows in this %s agree to fewer than the %d places "
            "requested. Raise the working length and recompute rather than shipping them. "
            "Narrowed: %s"
            % (len(short), len(rows), what, digits,
               ", ".join("%s at %d" % (name, got) for name, got in short)))
    return len(rows)


def _check():
    lines = []
    failed = 0
    digits = 40

    # An exact match agrees to every place. This case must not alarm, and it decides whether anyone can use the guard. A
    # guard that fires on correct input gets switched off, and then it protects nothing.
    same = 12345678901234567890123456789012345678
    got = agreed_places(same, same, digits)
    lines.append("  two identical routes agree to %d of %d places" % (got, digits))
    if got != digits:
        lines.append("    FAIL an exact match must agree to every place")
        failed += 1

    # THE POSITIVE CONTROL. A route short by one unit in the last place loses one place, and the guard must see it.
    # Without this, the guard could return `digits` unconditionally, and every check above would still pass.
    got = agreed_places(same, same + 1, digits)
    lines.append("  a route off by one unit in the last place: %d of %d places" % (got, digits))
    if got != digits - 1:
        lines.append("    FAIL one unit of disagreement should cost exactly one place")
        failed += 1

    # And the loss must scale with the size of the gap, or the count does not measure the gap.
    for lost in (1, 5, 17):
        got = agreed_places(same, same + 10 ** (lost - 1), digits)
        ok = got == digits - lost
        lines.append("  a gap of 10^%d costs %d places, expected %d: %s"
                     % (lost - 1, digits - got, lost, "yes" if ok else "NO"))
        if not ok:
            failed += 1

    # The function `require` must pass the case of agreement through unchanged. A guard that alters the value that it
    # guards is a second source of error, and not a check on the first.
    passed = require("agreeing", same, same, digits)
    lines.append("  require returns the value unchanged when it agrees: %s" % (passed == same))
    if passed != same:
        lines.append("    FAIL the guard modified the value it was guarding")
        failed += 1

    # And it must raise on the narrowed case, by the named type and not a bare RuntimeError.
    try:
        require("narrowed", same, same + 1, digits)
        lines.append("    FAIL a narrowed value was returned instead of raising")
        failed += 1
    except NarrowingAlarm as why:
        lines.append("  require raises NarrowingAlarm on a narrowed value, saying: %s"
                     % str(why).split(",")[0])

    # The row survey, on rows shaped the way the constants table shapes them.
    rows = [{"name": "clean_one", "agreed_digits": digits},
            {"name": "clean_two", "agreed_digits": digits},
            {"name": "short_one", "agreed_digits": digits - 1}]
    short = survey(rows, digits)
    lines.append("  survey over 3 rows, one of them short: found %s" % (short,))
    if short != [("short_one", digits - 1)]:
        lines.append("    FAIL the survey did not name exactly the short row")
        failed += 1

    # The negative control for the survey: all-clean rows must produce no alarm at all.
    try:
        checked = refuse_if_narrowed(rows[:2], digits)
        lines.append("  a clean set of %d rows raises nothing" % checked)
    except NarrowingAlarm:
        lines.append("    FAIL a clean set of rows alarmed, so the guard is unusable")
        failed += 1

    try:
        refuse_if_narrowed(rows, digits)
        lines.append("    FAIL a set containing a short row did not alarm")
        failed += 1
    except NarrowingAlarm:
        lines.append("  a set containing a short row alarms and names it")

    # A request of zero length is a degenerate case, and must not get a false pass. In a log, "agrees to all zero
    # places" reads as agreement.
    lines.append("  at a request of 0 places, two differing routes agree to %d"
                 % agreed_places(1, 2, 0))
    if agreed_places(1, 2, 0) != 0:
        lines.append("    FAIL a zero-width request should report zero agreeing places")
        failed += 1

    lines.append("")
    sys.stdout.write("\n".join(lines) + "\n")
    sys.stdout.write("%d check(s) failed\n" % failed)
    return failed


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="the narrowing alarm, with no tolerance in it")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    if args.check:
        sys.exit(1 if _check() else 0)
    sys.stdout.write(__doc__)
    sys.exit(2)

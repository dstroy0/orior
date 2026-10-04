#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""The gnascor state machine, read off a trace of asks: a state each cycle, and a label each transition.

    python utils/maint/engine/gnascor_read.py <trace>
    python utils/maint/engine/gnascor_read.py --check

A trace holds one line a cycle: the kind and cost of the left side's ask, the kind and cost of the
right side's, and the bound both were put under, the kinds as QueryKind in
src/c/transpiler/bootstrap/query_ask.h names them.

    HELD 412 NOT_HELD 380 900

A side that was not asked is written as - for its kind and - for its cost.

The tables are read out of src/c/transpiler/gnascor.md each run and are never copied here.

A side reads 1 where its ask held inside the bound, and 0 where it did not hold, ended its asker or
came in past the bound. A cycle's state follows from the two sides and from how they read:

    blok  a side ended its asker: a hard refusal
    gray  a side was not asked: every state at once, until an ask is put. Into it is fuzz, out fizz

A transition the tables mark sync passes through the sync superstate: fuzz+in into it, fizz+out of it,
the state it left and the state it reaches locked into the two labels.
    wait  one side held and the other answered past the bound: it lags
    busy  both held, each costing more than any held ask of the baseline cycles
    dual  both held
    lead  the left held and the right did not
    rite  the right held and the left did not
    void  neither held

The baseline is the trace's first cycles, put unbound or well inside their bound: the costs of every
held ask in them. Nothing here writes a scale in. The edge busy reads against is the baseline's largest
held cost, and the fraction of a cycle tilt reads against is that same cost: a side past its bound by
no more than one held ask's cost lagged, and past it by more it timed out.

A transition is its pair, the state left and the state reached. Where the tables name a pair more than
one way, each name is a candidate and the cycle decides between them. tilt holds where the side that
went to 0 came in past the bound by a fraction of a cycle, and the name beside it takes every other
case. fuse holds where both sides went to 0 with neither past the bound, and drop takes every other
case. A name with no condition outranks sync, the catch-all. Every transition gets one label.
"""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from order_check import MATRIX_DOC, read_matrices, table_cells  # noqa: E402

KINDS = ("HELD", "NOT_HELD", "PAST_BOUND", "ENDED")
# the kind a side carries where it was not asked
UNASKED = "-"

# how many leading cycles of a trace are its baseline
BASELINE_CYCLES = 4


def candidates_of(path=MATRIX_DOC):
    """Every pair the tables name, as {(past, now): [names]}, each name once, in the tables' order."""
    held = {}
    for _where, names, rows in read_matrices(path):
        for pair, name in table_cells(names, rows).items():
            held.setdefault(pair, [])
            if name not in held[pair]:
                held[pair].append(name)
    return held


def read_cycle(line):
    """One trace line as ((left kind, left cost), (right kind, right cost), bound)."""
    words = line.split()
    known = KINDS + (UNASKED,)
    if len(words) != 5 or words[0] not in known or words[2] not in known:
        raise ValueError("a trace line is: <kind> <cost> <kind> <cost> <bound>, read: %r" % line)

    def side(kind, cost):
        return (kind, 0) if kind == UNASKED else (kind, int(cost))

    return side(words[0], words[1]), side(words[2], words[3]), int(words[4])


def baseline_of(cycles):
    """The largest cost of any held ask in the baseline cycles, and 0 where none held."""
    costs = [cost for left, right, _bound in cycles[:BASELINE_CYCLES]
             for kind, cost in (left, right) if kind == "HELD"]
    return max(costs) if costs else 0


def state_of(left, right, edge):
    """A cycle's state from its two sides."""
    kinds = (left[0], right[0])
    if "ENDED" in kinds:
        return "blok"
    if UNASKED in kinds:
        return "gray"
    if "HELD" in kinds and "PAST_BOUND" in kinds:
        return "wait"
    if kinds == ("HELD", "HELD"):
        if left[1] > edge and right[1] > edge:
            return "busy"
        return "dual"
    if kinds[0] == "HELD":
        return "lead"
    if kinds[1] == "HELD":
        return "rite"
    return "void"


def lagged(side, bound, fraction):
    """Whether a side came in past its bound by no more than a fraction of a cycle."""
    return side[0] == "PAST_BOUND" and (side[1] - bound) <= fraction


def decide(pair, names, left, right, bound, fraction):
    """The one label a transition takes in this cycle, from the names the tables give its pair."""
    if len(names) == 1:
        return names[0]
    gone = [side for side, held in ((left, left[0] == "HELD"), (right, right[0] == "HELD")) if not held]
    if "tilt" in names:
        if gone and all(lagged(side, bound, fraction) for side in gone):
            return "tilt"
        return [name for name in names if name != "tilt"][0]
    if "fuse" in names:
        if all(side[0] != "PAST_BOUND" for side in gone):
            return "fuse"
        return [name for name in names if name != "fuse"][0]
    named = [name for name in names if name != "sync"]
    return named[0] if named else "sync"


def read_trace(cycles, held=None):
    """The state of every cycle and the label of every transition, as (states, labels)."""
    held = candidates_of() if held is None else held
    edge = baseline_of(cycles)
    states = [state_of(left, right, edge) for left, right, _bound in cycles]
    labels = []
    for at in range(1, len(cycles)):
        pair = (states[at - 1], states[at])
        left, right, bound = cycles[at]
        names = held.get(pair)
        labels.append(decide(pair, names, left, right, bound, edge) if names else None)
    return states, labels


def atomic(labels):
    """Whether every trip through gray is one unit: a fuzz opens it, one fizz closes it, and none nests."""
    open_span = False
    for label in labels:
        if label == "fuzz":
            if open_span:
                return False
            open_span = True
        elif label == "fizz":
            if not open_span:
                return False
            open_span = False
    return True


def steps(states, labels):
    """The trace as (label, state) steps after its first state. A transition the tables mark sync passes through
    the sync superstate: it enters by fuzz carrying the state it left and leaves by fizz carrying the state it
    reaches, and the pair is locked into the two labels."""
    out = []
    for past, state, label in zip(states, states[1:], labels):
        if label == "sync":
            out.append(("fuzz+" + past, "sync"))
            out.append(("fizz+" + state, state))
        else:
            out.append((label, state))
    return out


def pairs_from_labels(walked):
    """Every sync passage's pair, read back from its two labels alone, as [(past, now)]."""
    found = []
    entered = None
    for label, _state in walked:
        if label and label.startswith("fuzz+"):
            entered = label[len("fuzz+"):]
        elif label and label.startswith("fizz+") and entered is not None:
            found.append((entered, label[len("fizz+"):]))
            entered = None
    return found


def stream(states, labels):
    """The trace as the coherence clock prints it."""
    out = ["[%s]" % states[0]]
    for label, state in steps(states, labels):
        out.append("-(%s)-> [%s]" % (label or "----", state))
    return " ".join(out)


def check():
    """Every rule the reader states, held on traces whose answers are known. The count failed."""
    failed = 0
    held = candidates_of()
    base = [("HELD", 100), ("HELD", 120)]

    def run(rows):
        cycles = [(base[0], base[1], 1000)] * BASELINE_CYCLES + rows
        return read_trace(cycles, held)

    def expect(what, rows, want):
        nonlocal failed
        states, labels = run(rows)
        got = labels[-1]
        if got != want:
            failed += 1
            print("  FAILED: %s: %s, wanted %s, read %s" % (what, stream(states, labels), want, got))

    lead = (("HELD", 100), ("NOT_HELD", 0), 1000)
    rite = (("NOT_HELD", 0), ("HELD", 100), 1000)
    dual = (("HELD", 100), ("HELD", 100), 1000)
    expect("lead then rite is the shift right", [lead, rite], "pass")
    expect("rite then lead is the shift left", [rite, lead], "back")
    # A held side beside one a fraction past its bound is wait by the base states: a dual whose right side
    # lags reads dual to wait and never dual to lead: read off a trace, the lag tilt names arrives as wait
    expect("dual with the right side a fraction late reads as dual to wait",
           [dual, (("HELD", 100), ("PAST_BOUND", 1050), 1000)], held_name(held, "dual", "wait"))
    expect("void to dual", [(("NOT_HELD", 0), ("NOT_HELD", 0), 1000), dual], "sprk")
    busy = (("HELD", 900), ("HELD", 950), 1000)
    expect("busy to blok is gridlock", [busy, (("ENDED", 0), ("HELD", 900), 1000)], "jamm")
    wait = (("HELD", 100), ("PAST_BOUND", 1050), 1000)
    expect("wait to void is the timeout", [wait, (("NOT_HELD", 0), ("NOT_HELD", 0), 1000)], "loss")

    # the situation decides between two names for one pair
    tilt_right = (("HELD", 100), ("PAST_BOUND", 1050), 1000)
    states, labels = run([dual, tilt_right])
    if states[-1] != "wait":
        failed += 1
        print("  FAILED: a held side beside one past its bound reads wait, read %s" % states[-1])
    pairs = [
        ("dual to lead, the right side not held", "dual", "lead", (("HELD", 100), ("NOT_HELD", 0)), "drop"),
        ("dual to lead, the right side a fraction past the bound", "dual", "lead",
         (("HELD", 100), ("PAST_BOUND", 1050)), "tilt"),
        ("dual to lead, the right side a timeout past the bound", "dual", "lead",
         (("HELD", 100), ("PAST_BOUND", 5000)), "drop"),
        ("dual to void, both gone at once", "dual", "void", (("NOT_HELD", 0), ("NOT_HELD", 0)), "fuse"),
        ("dual to void, through a timeout", "dual", "void", (("PAST_BOUND", 5000), ("NOT_HELD", 0)), "drop"),
        ("busy to dual", "busy", "dual", (("HELD", 100), ("HELD", 100)), "surg"),
    ]
    for what, past, now, sides, want in pairs:
        got = decide((past, now), held[(past, now)], sides[0], sides[1], 1000, 120)
        if got != want:
            failed += 1
            print("  FAILED: %s: wanted %s, read %s" % (what, want, got))

    # gray: a side not asked leaves the pair every state at once, whatever the other side read, and a refusal is
    # an answer and still reads blok. Into gray is fuzz, out of it fizz, and gray to gray is neither
    unasked = (UNASKED, 0)
    gray_held = all(state_of(unasked, other, 120) == "gray" for other in
                    (("HELD", 100), ("NOT_HELD", 0), ("PAST_BOUND", 1050), unasked))
    gray_held = gray_held and state_of(("HELD", 100), unasked, 120) == "gray"
    gray_held = gray_held and state_of(unasked, ("ENDED", 0), 120) == "blok"
    states, labels = run([(unasked, unasked, 1000), (unasked, unasked, 1000), dual])
    gray_held = gray_held and states[-3:] == ["gray", "gray", "dual"] and labels[-3:] == ["fuzz", None, "fizz"]
    if not gray_held:
        failed += 1
        print("  FAILED: an unasked side reads gray, a refusal blok, into gray is fuzz and out of it fizz")

    # atomicity, over drawn traces: a fuzz never opens while one is open, and every fizz closes the one open fuzz
    drawn = 0x2545f491
    broken = 0
    sync_total = 0
    sync_lost = 0
    sides = [("HELD", 100), ("HELD", 900), ("NOT_HELD", 0), ("PAST_BOUND", 1050), ("ENDED", 0), unasked]
    for _trace in range(500):
        rows = []
        for _cycle in range(40):
            drawn = (drawn * 1103515245 + 12345) & 0x7fffffff
            left = sides[(drawn >> 8) % len(sides)]
            right = sides[(drawn >> 16) % len(sides)]
            rows.append((left, right, 1000))
        states, labels = run(rows)
        broken += 0 if atomic(labels) else 1
        # every sync passage gives back, from its labels alone, the exact pair it stood for
        sync_pairs = [(past, now) for past, now, label in zip(states, states[1:], labels) if label == "sync"]
        sync_total += len(sync_pairs)
        sync_lost += 0 if pairs_from_labels(steps(states, labels)) == sync_pairs else 1
    if broken:
        failed += 1
        print("  FAILED: %d of 500 drawn traces open a fuzz inside one or close a fizz with none open" % broken)
    if sync_lost or not sync_total:
        failed += 1
        print("  FAILED: %d of 500 drawn traces lose a sync passage's pair, over %d passages" % (sync_lost, sync_total))
    print("  %d sync passages over 500 drawn traces, every pair read back from its fuzz+ and fizz+ labels"
          % sync_total)

    # every pair the tables name is decided in every situation a side can be in
    situations = [(kind, cost) for kind in KINDS for cost in (0, 100, 1050, 5000)]
    undecided = 0
    for pair, names in held.items():
        for left in situations:
            for right in situations:
                if decide(pair, names, left, right, 1000, 120) not in names:
                    undecided += 1
    if undecided:
        failed += 1
        print("  FAILED: %d situations left a transition with no label from its own candidates" % undecided)
    print("  %d pairs, each decided in all %d situations two sides can be in"
          % (len(held), len(situations) ** 2))
    print("  gnascor read: %d failed" % failed)
    return failed


def held_name(held, past, now):
    """The one name the tables give a pair."""
    return held[(past, now)][0]


# The states a scenario's pair of side specs can read as. Both held reads dual, or busy where both runs came in heavy,
# which a scenario does not choose
IMPLIED = {
    ("held", "held"): ("dual", "busy"),
    ("held", "not"): ("lead",),
    ("not", "held"): ("rite",),
    ("not", "not"): ("void",),
    ("held", "late"): ("wait",),
    ("late", "held"): ("wait",),
}


def check_scenario(scenario_path, trace_path):
    """Whether every state read off a trace of real asks is one its scenario implies. The count that are not."""
    with open(scenario_path, encoding="utf-8") as handle:
        specs = [tuple(line.split()) for line in handle if line.strip()]
    with open(trace_path, encoding="utf-8") as handle:
        cycles = [read_cycle(line) for line in handle if line.strip() and not line.startswith("#")]
    states, labels = read_trace(cycles)
    traced = states[BASELINE_CYCLES:]
    wrong = 0
    for spec, state in zip(specs, traced):
        wanted = ("gray",) if "unasked" in spec else IMPLIED.get(spec, ())
        if state not in wanted:
            wrong += 1
            print("  FAILED: %s %s read %s, wanted %s" % (spec[0], spec[1], state, " or ".join(wanted)))
    if len(traced) != len(specs):
        wrong += 1
        print("  FAILED: %d cycles traced for %d in the scenario" % (len(traced), len(specs)))
    print("  " + stream(states, labels))
    print("  %d cycles of real asks, %d read as other than the scenario implies" % (len(specs), wrong))
    return wrong


def main():
    out = sys.stdout
    out.reconfigure(encoding="utf-8", errors="replace")
    if len(sys.argv) == 2 and sys.argv[1] == "--check":
        return 1 if check() else 0
    if len(sys.argv) == 4 and sys.argv[1] == "--scenario":
        return 1 if check_scenario(sys.argv[2], sys.argv[3]) else 0
    if len(sys.argv) != 2:
        out.write(__doc__)
        return 2
    with open(sys.argv[1], encoding="utf-8") as handle:
        cycles = [read_cycle(line) for line in handle if line.strip() and not line.startswith("#")]
    states, labels = read_trace(cycles)
    out.write(stream(states, labels) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())

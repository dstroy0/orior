#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# Catalog: VIZ-x-005
#
"""Puts a moving system inside the ball, and derives it back from the boundary alone.

    python examples/00_blob_viz_tools/build_orrery_view.py
    python examples/00_blob_viz_tools/build_orrery_view.py --bodies 6 --frames 512 --mode shadow

  - `--bodies`: how many bodies in orbit. Default 6.
  - `--frames`: how many steps of the clock to precompute. Default 512.
  - `--mode`: glow for a scattering ball, shadow for a clear one. Default glow.
  - `--degrees`: highest harmonic degree carried. Default 48.
  - `--tau`: conduction time that the surface gets to smooth for. Default 0.0006.
  - `--seed`: draw for the sizes and phases. Default 11.
  - `--out`: where to write. Default: orrery_view.html in build/view, through out_path.

WHY A SYSTEM AND NOT A FILE

Everything else in this directory reads data that nobody knows the shape of, and a viewer that looks right is impossible
to grade. Here the build writes the interior down first: bodies at known radii on known periods. The page then shows the
boundary alone, and derives the interior back out of it. It prints the derived numbers next to the ones that the build
used. A viewer that draws a plausible picture and recovers the wrong radius shows its fault in the same glance.

TWO WAYS FOR THE INSIDE TO REACH THE OUTSIDE

    glow    the medium scatters without limit, transport is diffusion, and a body at radius r
            reaches degree l as (r/R)^l. Depth sets how wide a patch the body can print. A body
            near the center is a broad warmth, and one near the shell is a small bright spot. There
            are no shadows in this limit at all: light that lost its direction cannot leave one
            behind.

    shadow  the medium is clear, transport is ballistic, and a beam from outside prints the
            silhouette of whatever it passes. Edges are as sharp as the bodies are, and a body that
            emits nothing is as visible as one that emits everything.

Those are the two limits of one transport equation, and the page carries both, because they fail in opposite directions.
Diffusion sees what is bright, and loses where it is. Ballistics sees where something is, and loses what it was. A very
bright light through a scattering ball is the attempt to read the ballistic part before scattering buries it. The page
prints the attenuation that the attempt must survive.

TIME IS THE INTERIOR IN MOTION WHILE THE BOUNDARY HOLDS STILL

The boundary never moves here. The bodies do, and the surface changes because of them. The clock therefore comes from no
outside source: the page reads it off a fixed surface. To tell a run where the observer also moves from a run where it
does not, hold the observer still. Then watch whether anything still changes.

WHAT THE PAGE DERIVES

The page watches a single patch of the boundary over the whole run, and gets one number per step. Each body that crosses
the line between the source and that patch prints a dip. The dips repeat on the period of that body, and the period
gives the radius by Kepler's third law. The page prints the recovered radius beside the radius that the build gave the
body. Nothing about the recovery reads the body table.
"""

import io
import math
import os
import sys

import dsp
import settings
import sphere_field
import out_path
import generate_template

HERE = os.path.dirname(os.path.abspath(__file__))
TEMPLATE = os.path.join(HERE, "orrery_view_template.html")


def draw(seed):
    """The same small generator the other tools use. A seed gives the same stream in every tool."""
    state = (seed ^ 0x9E3779B97F4A7C15) & 0xFFFFFFFFFFFFFFFF
    while True:
        state = (state * 6364136223846793005 + 1442695040888963407) & 0xFFFFFFFFFFFFFFFF
        yield ((state >> 11) & 0x1FFFFFFFFFFFFF) / float(1 << 53)


def unit(vector):
    length = math.sqrt(sum(one * one for one in vector)) or 1.0
    return [one / length for one in vector]


def cross(left, right):
    return [
        left[1] * right[2] - left[2] * right[1],
        left[2] * right[0] - left[0] * right[2],
        left[0] * right[1] - left[1] * right[0],
    ]


def sideways(axis):
    """Any unit vector at right angles to this one, taken from whichever axis it leans on least."""
    pick = [1.0, 0.0, 0.0] if abs(axis[0]) < 0.9 else [0.0, 1.0, 0.0]
    return unit(cross(axis, pick))


def system(count, seed):
    """Bodies at spread radii on Kepler periods, each on its own orbit plane.

    The period goes as the radius to the three halves. The outer bodies are slow, and the dips that they print are rare.
    The run must be long for that reason. A body at 0.85 of the way out crosses about a third as often as one at 0.4. A
    run that stops early recovers the inner bodies cleanly, and reports the outer ones as absent.

    Each orbit gets a plane of its own, and never a tilt on a shared one. A tilt of one plane leaves every orbit across
    the same two points. A patch at either of them watches every body pass dead center. The recovery then reports the
    whole system from a single patch, and looks far better than it is. This construction exists to avoid that failure.
    """
    stream = draw(seed)
    bodies = []
    for index in range(count):
        share = (index + 1.0) / (count + 1.0)
        radius = 0.10 + 0.80 * share
        size = 0.032 + 0.050 * next(stream)
        # Sunlight falls off with the square of the distance and a body catches it over its own
        # disc. What it sends onward goes as size squared over radius squared.
        brightness = (size * size) / (radius * radius)

        height = 2.0 * next(stream) - 1.0
        around = 2.0 * math.pi * next(stream)
        flat = math.sqrt(max(0.0, 1.0 - height * height))
        normal = unit([flat * math.cos(around), flat * math.sin(around), height])
        first = sideways(normal)
        second = unit(cross(normal, first))

        bodies.append(
            {
                "index": index,
                "radius": round(radius, 4),
                "size": round(size, 4),
                "period": round(math.pow(radius, 1.5), 6),
                "phase": round(next(stream) * 2.0 * math.pi, 5),
                "brightness": round(brightness, 6),
                "normal": [round(one, 5) for one in normal],
                "u": [round(one, 5) for one in first],
                "v": [round(one, 5) for one in second],
            }
        )
    return bodies


def at_time(body, moment):
    """Where a body is at this moment, as a unit direction. The code carries its radius apart."""
    angle = body["phase"] + 2.0 * math.pi * moment / body["period"]
    here = math.cos(angle)
    there = math.sin(angle)
    return (
        body["u"][0] * here + body["v"][0] * there,
        body["u"][1] * here + body["v"][1] * there,
        body["u"][2] * here + body["v"][2] * there,
    )


def watch_shadow(bodies, frames, span, patch):
    """What a fixed patch receives when the ball is clear: the source, less whatever crosses it.

    A body blocks when its angular size covers the line from the center to the patch. This is the transit curve, and it
    carries a body only where the orbit of that body happens to pass across this patch. Most pairs of body and patch
    never line up at all. Most of these curves are flat. The glow curve below and the several patches above exist to
    prevent the mistake of a flat curve read as an empty system.
    """
    curve = []
    for step in range(frames):
        moment = span * step / frames
        blocked = 0.0
        for body in bodies:
            spot = at_time(body, moment)
            along = spot[0] * patch[0] + spot[1] * patch[1] + spot[2] * patch[2]
            if along <= 0.0:
                continue
            apart = math.acos(max(-1.0, min(1.0, along)))
            covers = math.asin(min(1.0, body["size"] / body["radius"]))
            if apart < covers:
                shading = 1.0 - (apart / covers) ** 2
                blocked += shading * (body["size"] / body["radius"]) ** 2
        curve.append(1.0 - min(0.9, blocked))
    return curve


def watch_glow(bodies, frames, span, patch, profiles):
    """What a fixed patch receives when the ball scatters: the sum of every body's spot, in motion.

    Nothing must line up here. The contribution of a body to this patch is its profile, read at the angle between the
    patch and wherever the body is. That angle swings once per orbit for every body and every patch. Each body therefore
    prints a periodic signal on every patch, and the signal is smooth instead of a spike.

    That difference decides whether the recovery works. A transit is a narrow pulse, and a narrow pulse carries more
    power in its high harmonics than in its own fundamental. A read of the strongest peaks off one gives a tidy row of
    periods that are all too short. A read of a smooth swing gives the fundamental first: the period that the body
    actually has.
    """
    curve = []
    for step in range(frames):
        moment = span * step / frames
        total = 0.0
        for body in bodies:
            spot = at_time(body, moment)
            along = spot[0] * patch[0] + spot[1] * patch[1] + spot[2] * patch[2]
            apart = math.acos(max(-1.0, min(1.0, along)))
            table = profiles["%.4f" % body["radius"]]
            at = int(round(apart / math.pi * (len(table) - 1)))
            total += table[at] * body["brightness"]
        curve.append(total)
    return curve


def recover(curve, span, frames, bodies):
    """Periods out of the light curve, then radii out of the periods. Reads no body table.

    The function turns the curve into a spectrum, and reads its peaks as periods. Kepler's third law then gives a radius
    from each period. The build made the period as the radius to the three halves, and no other quantity went into it.
    The page compares against radii that this function never sees. A recovery that agrees agrees with the scene, and not
    with itself.
    """
    middle = sum(curve) / len(curve)
    centered = [one - middle for one in curve]

    # Padded eight times past the transform that the data alone needs. Padding adds no resolution, and this does not ask
    # it to. A period whose peak lands between two bins reads at whichever bin is nearer. A radius from a period off by
    # half a bin is wrong by more than the whole recovery is worth. The interpolation puts the peak where it belongs.
    padded = dsp.next_power(len(centered)) * 8
    magnitudes = dsp.spectrum(centered, dsp.window("hann", len(centered)), padded)

    # A peak must be above the noise before the code calls it a period. A curve with one deep dip has power at every
    # harmonic of that dip. A read of all of them gives a tidy row of bodies that are not there.
    #
    # The floor is the median of the whole spectrum, and never of a window around the peak. Eight times of padding make
    # adjacent bins copies of one another. A window of a dozen bins covers less than two real ones, and its median is
    # the peak itself. A windowed median compares every peak against a slightly smaller copy of itself, and passes
    # nothing. That reads as a system too faint to recover: a window measured in the wrong units.
    ranked = sorted(magnitudes)
    floor = ranked[len(ranked) // 2]
    oversample = max(1, padded // len(centered))

    # A harmonic sum instead of a bare peak list. A body's swing is smooth, and not a sine. It puts power at twice its
    # frequency and at three times it, and those extra peaks read back as bodies that are not there. A filter on pairs
    # of candidates does not fix it. A harmonic of one body sits close enough to a real period of another to take its
    # place.
    #
    # A score for each trial frequency, from what sits at it and at its own multiples, settles it with no comparison at
    # all. A true fundamental collects from every harmonic it has. A harmonic collects from its multiples alone, because
    # the bins below it, where its own sub-harmonics would be, hold nothing.
    reach = 4
    top_bin = len(magnitudes) // reach
    score = [0.0] * top_bin
    for index in range(1, top_bin):
        total = 0.0
        for times in range(1, reach + 1):
            total += magnitudes[index * times]
        score[index] = total

    # The trial band comes from geometry, and not from the scene. A body is inside the ball. Its radius is under one,
    # and its period is under one with it. A body too close to the center has no orbit worth the name. Without the band,
    # the harmonic sum runs away at the bottom, where every one of its multiples lands in the crowded low bins. It
    # collects leakage from all of them, and gives radii above 1.0: a body outside the ball that it orbits inside.
    def bin_for(radius):
        period = math.pow(radius, 1.5)
        return int((1.0 / period) * span / (frames / float(padded)))

    lowest = max(oversample, bin_for(0.99))
    highest = min(top_bin - oversample, bin_for(0.05))

    found = []
    for index in range(lowest, highest):
        here = score[index]
        if floor <= 0.0 or here < floor * reach * 2.0:
            continue
        if here < max(score[index - oversample : index + oversample + 1]):
            continue
        frequency = index * (frames / float(padded)) / span
        if frequency <= 0.0:
            continue
        found.append({"period": 1.0 / frequency, "strength": here / (floor * reach)})

    # Harmonic families, folded to their fundamental. A body's swing is smooth but not a sine. It puts power at twice
    # its frequency and three times it as well. Read as periods, those come back as the radii that the second and third
    # harmonics imply. Each of them lands on a body that is not there, and the count of bodies doubles with ghosts.
    #
    # The code drops a candidate where a stronger candidate already sits at a whole fraction of its frequency. Where the
    # harmonic came in stronger than its own fundamental, the fundamental replaces it instead. The body has the longer
    # period, and the harmonic is an artifact of the shape of the swing.
    found.sort(key=lambda one: -one["strength"])
    retained = []
    for entry in found:
        if any(
            abs(entry["period"] - one["period"]) < 0.04 * one["period"] for one in retained
        ):
            continue
        retained.append(entry)
        # A fixed ceiling, and never the number of bodies. A cap at the count that the scene used would pass the
        # recovery the answer to its own question. A recovery that knows how many to find cannot report too many.
        if len(retained) >= 10:
            break

    out = []
    for entry in sorted(retained, key=lambda one: one["period"]):
        radius = math.pow(entry["period"], 2.0 / 3.0)
        near = min(bodies, key=lambda body: abs(body["radius"] - radius))
        out.append(
            {
                "period": round(entry["period"], 5),
                "radius": round(radius, 4),
                "strength": round(entry["strength"], 2),
                "nearest": near["index"],
                "off_by": round(abs(near["radius"] - radius), 4),
            }
        )
    return out


def main():
    argv = sys.argv[1:]
    if "--help" in argv or "-h" in argv:
        sys.stdout.write(__doc__)
        sys.stdout.write("\n" + settings.usage() + "\n")
        return 2

    opening = settings.collect(argv)

    def option(flag, fallback, cast=str):
        if flag in argv:
            return cast(argv[argv.index(flag) + 1])
        return fallback

    count = option("--bodies", 6, int)
    frames = option("--frames", 512, int)
    mode = option("--mode", "glow")
    top = option("--degrees", 48, int)
    tau = option("--tau", 0.0006, float)
    seed = option("--seed", 11, int)
    if mode not in ("glow", "shadow"):
        sys.stderr.write("--mode takes glow or shadow\n")
        return 2
    if count < 1 or count > 12:
        sys.stderr.write("--bodies sits between 1 and 12\n")
        return 2

    bodies = system(count, seed)

    # Long enough that the slowest body crosses several times. A run cut to the inner periods recovers those, and
    # reports the outer bodies as absent. That reads as a defect in the recovery, and is a defect in the run length.
    span = 8.0 * max(body["period"] for body in bodies)

    # Six patches, at the two poles of each of three great circles at right angles. One patch sees only the bodies whose
    # orbit crosses its own line. With orbit planes drawn independently, that is a different few bodies for each patch.
    # A recovery from one patch, reported as the system, is the mistake that this makes impossible. The page prints what
    # each patch found on its own and what they found together. The gap between those measures what a move of the
    # observer is worth.
    patches = [
        ("+x", (1.0, 0.0, 0.0)),
        ("-x", (-1.0, 0.0, 0.0)),
        ("+y", (0.0, 1.0, 0.0)),
        ("-y", (0.0, -1.0, 0.0)),
        ("+z", (0.0, 0.0, 1.0)),
        ("-z", (0.0, 0.0, -1.0)),
    ]

    profiles = {}
    for body in bodies:
        key = "%.4f" % body["radius"]
        if key not in profiles:
            profiles[key] = sphere_field.zonal_profile(body["radius"], tau, top, 241)

    watched = []
    for name, patch in patches:
        glow = watch_glow(bodies, frames, span, patch, profiles)
        shade = watch_shadow(bodies, frames, span, patch)
        seen = recover(glow, span, frames, bodies)
        watched.append(
            {
                "name": name,
                "curve": [round(one, 6) for one in glow],
                "shadow": [round(one, 6) for one in shade],
                "found": seen,
                "transits": round(1.0 - min(shade), 6),
            }
        )

    # Merged across every patch, and ranked by how many independent axes agree.
    #
    # Peak strength cannot tell a body from an artifact: on one patch a spurious peak can outscore a real body several
    # times over. Strength says how loud a bin is. It does not say whether anything is there.
    #
    # Agreement does work. A body is somewhere, and every patch sees it swing. An artifact is a feature of one curve,
    # which lives on one patch and dies on the next. Antipodal patches are not independent, because a great circle
    # through a point runs through its opposite as well. The code therefore counts axes and never patches. Three of
    # three is a body, and one of three is a number that came out of one arithmetic.
    merged = []
    for entry in watched:
        axis = entry["name"][1]
        for one in entry["found"]:
            match = None
            for already in merged:
                if abs(already["period"] - one["period"]) < 0.08 * already["period"]:
                    match = already
                    break
            if match is None:
                merged.append(
                    {
                        "period": one["period"],
                        "radius": one["radius"],
                        "nearest": one["nearest"],
                        "off_by": one["off_by"],
                        "patches": [entry["name"]],
                        "axes": [axis],
                    }
                )
            else:
                if entry["name"] not in match["patches"]:
                    match["patches"].append(entry["name"])
                if axis not in match["axes"]:
                    match["axes"].append(axis)
    for one in merged:
        one["agree"] = len(one["axes"])
    merged.sort(key=lambda one: (-one["agree"], one["period"]))

    curve = watched[4]["curve"]
    found = merged

    shipped = {}
    for key in profiles:
        shipped[key] = [round(one, 8) for one in profiles[key]]

    # What a ballistic reading must survive. Scattering buries the straight-through part as the exponential of the path
    # in scattering lengths. The brightness that a shadow needs is that exponential. The page prints it, and the shadow
    # mode stays honest as a clear-medium answer.
    depths = [1.0, 4.0, 12.0, 30.0]
    attenuation = [
        {"lengths": one, "survives": "%.2e" % math.exp(-one)} for one in depths
    ]

    payload = {
        "mode": mode,
        "degrees": top,
        "tau": tau,
        "span": round(span, 5),
        "frames": frames,
        "bodies": bodies,
        "profiles": shipped,
        "curve": curve,
        "watched": watched,
        "found": found,
        "attenuation": attenuation,
        "settings": opening,
    }

    try:
        page = generate_template.assemble(TEMPLATE, payload)
    except generate_template.Refused as why:
        sys.stderr.write("%s: %s\n" % (os.path.basename(TEMPLATE), why))
        return 1
    if page.count("</script>") < page.count("<script"):
        sys.stderr.write("the template left a script open. The page would not run\n")
        return 1

    out = out_path.resolve("orrery_view.html", option("--out", None))
    with io.open(out, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(page)

    print("%s" % out)
    print(
        "  %d bodies, %d steps over %.2f turns of the slowest"
        % (count, frames, span / max(b["period"] for b in bodies))
    )
    print(
        "  built radii:     %s" % ", ".join("%.3f" % body["radius"] for body in bodies)
    )
    for entry in watched:
        names = ", ".join("%.3f" % one["radius"] for one in entry["found"]) or "nothing"
        print("    patch %s found %d: %s" % (entry["name"], len(entry["found"]), names))
    if not found:
        print("  together:        none was above the noise")
        return 0

    truth = [body["radius"] for body in bodies]
    for level in (3, 2, 1):
        band = [one for one in found if one["agree"] == level]
        if not band:
            continue
        displayed = []
        for one in band:
            near = min(abs(one["radius"] - was) for was in truth)
            displayed.append("%.3f%s" % (one["radius"], "" if near < 0.04 else "?"))
        print("  seen on %d of 3 axes: %s" % (level, ", ".join(displayed)))

    sure = [one for one in found if one["agree"] == 3]
    caught = 0
    for was in truth:
        if any(abs(one["radius"] - was) < 0.04 for one in sure):
            caught += 1
    wrong = sum(
        1 for one in sure if min(abs(one["radius"] - was) for was in truth) >= 0.04
    )
    print(
        "  of %d bodies built, %d recovered on all three axes, with %d that match nothing"
        % (len(bodies), caught, wrong)
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())

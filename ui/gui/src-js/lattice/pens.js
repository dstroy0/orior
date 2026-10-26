// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The pens a lattice's points draw with, one a level of disorder: a point in place is signal green, and a
// moved one passes through the link blue to violet.

import { rgbOf } from "../colors.js";
import { onScheme } from "../scheme.js";

// The colors from ordered to disordered, the stylesheet's --lattice-1 to --lattice-3.
const STOPS = ["--lattice-1", "--lattice-2", "--lattice-3"];

// The pens a point draws with, one a level of disorder, and the share of its ink each puts down. The
// shader blends the two pens either side of a point's disorder, so its shade is continuous, over as
// many pens as the GL driver names; the 2D canvas draws each point with the nearest of LEVELS_DRAWN
// pens, which keeps a step of shade within a dot or two of the lattice as the order spreads.
export const LEVELS_DRAWN = 96;
export const DIM = 0.85;

// The share of its ink a point puts down at its `order`, 0 for the least ordered point to 1 for the most: from
// FAINTEST, 0.426 dimmed by DIM, to BRIGHTEST.
const FAINTEST = 0.426 * DIM;
const BRIGHTEST = 0.42;
export const inkOf = (order) => FAINTEST * (1 - order) + BRIGHTEST * order;

function shade(loose) {
  const along = loose * (STOPS.length - 1);
  const low = Math.min(Math.floor(along), STOPS.length - 2);
  const part = along - low;
  const to = rgbOf(STOPS[low + 1]);
  return rgbOf(STOPS[low]).map((from, at) => Math.round(from + (to[at] - from) * part));
}

// The pen of each of `count` levels, made again when the scheme or a theme changes.
const pens = new Map();
onScheme(() => pens.clear());
export function pensOf(count) {
  if (!pens.has(count)) {
    pens.set(
      count,
      Array.from({ length: count }, (_, level) => {
        const loose = level / (count - 1);
        const order = 1 - loose;
        const [red, green, blue] = shade(loose);
        return { color: `rgba(${red}, ${green}, ${blue}, ${inkOf(order).toFixed(3)})`, size: 0.59 + 0.16 * order };
      }),
    );
  }
  return pens.get(count);
}

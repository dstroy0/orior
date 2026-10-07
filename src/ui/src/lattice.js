// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The lattice the docs site draws behind its hero: a square grid of points, each moved off its place
// by an amount that is zero at the left and grows toward the right. The field reads from a pattern
// to its shuffled copy. A point in place is signal green, and a moved one passes through the link
// blue to violet. The draw is seeded and gives the same lattice every time, and it holds still.

const STEP = 22;
const SEED = 1729;
const STOPS = [
  [111, 220, 180],
  [138, 184, 255],
  [180, 156, 255],
];

function random(seed) {
  let state = seed >>> 0;
  return () => {
    state = (state + 0x6d2b79f5) >>> 0;
    let mixed = state;
    mixed = Math.imul(mixed ^ (mixed >>> 15), mixed | 1);
    mixed ^= mixed + Math.imul(mixed ^ (mixed >>> 7), mixed | 61);
    return ((mixed ^ (mixed >>> 14)) >>> 0) / 4294967296;
  };
}

function shade(loose) {
  const along = loose * (STOPS.length - 1);
  const low = Math.min(Math.floor(along), STOPS.length - 2);
  const part = along - low;
  return STOPS[low].map((from, at) => Math.round(from + (STOPS[low + 1][at] - from) * part));
}

export function drawLattice(canvas) {
  const box = canvas.getBoundingClientRect();
  if (!box.width || !box.height) {
    return;
  }
  const scale = window.devicePixelRatio || 1;
  canvas.width = Math.round(box.width * scale);
  canvas.height = Math.round(box.height * scale);
  const pen = canvas.getContext("2d");
  pen.setTransform(scale, 0, 0, scale, 0, 0);
  pen.clearRect(0, 0, box.width, box.height);
  const draw = random(SEED);
  for (let y = STEP / 2; y < box.height; y += STEP) {
    for (let x = STEP / 2; x < box.width; x += STEP) {
      const loose = Math.max(0, Math.min(1, (x / box.width - 0.3) / 0.7));
      const shift = loose * loose * STEP * 2.4;
      const turn = draw() * Math.PI * 2;
      const reach = draw();
      const kept = 1 - loose;
      const [red, green, blue] = shade(loose);
      pen.fillStyle = `rgba(${red}, ${green}, ${blue}, ${(0.36 + 0.16 * kept).toFixed(3)})`;
      pen.beginPath();
      pen.arc(x + Math.cos(turn) * shift * reach, y + Math.sin(turn) * shift * reach, 1.3 + 0.4 * kept, 0, Math.PI * 2);
      pen.fill();
    }
  }
}

// Redraws a lattice each time its size changes, which a lattice in a hidden view also does when the
// view is shown. One taken off the page is let go.
const sized = new ResizeObserver((entries) =>
  entries.forEach((entry) => (entry.target.isConnected ? drawLattice(entry.target) : sized.unobserve(entry.target))),
);

export function keepLattice(canvas) {
  sized.observe(canvas);
}

// Keeps every lattice on the page drawn.
export function keepLattices() {
  document.querySelectorAll("canvas.lattice").forEach(keepLattice);
}

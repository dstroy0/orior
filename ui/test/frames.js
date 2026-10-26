// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The frames the editor's view draws, as the page's trace reads them, for the view's tests.


// A line of the window's trace: its indent, the call's name, when it was entered and how long it ran.
export const TRACED = /^( *)([\w.]+) (\d+\.\d{3}) (\d+\.\d{3})$/;

// Where the view stood at its last scroll event, read without laying the page out as a read of the
// scroller would in a frame the page has changed.
export const seen = (view) => (view.lastTop ?? view.scroller.scrollTop) - view.pad;

// The frames the page is given over `ms`, as requestAnimationFrame calls it: the times of each.
export function framesOver(ms) {
  return new Promise((done) => {
    const times = [];
    const tick = (now) => {
      times.push(now);
      if (now - times[0] < ms) {
        requestAnimationFrame(tick);
      } else {
        done(times);
      }
    };
    requestAnimationFrame(tick);
  });
}

// Each frame the trace says the view drew: when it began and how long it took, and its stages'.
export function paintsOf(trace) {
  const paints = [];
  for (const line of trace.split("\n")) {
    const found = TRACED.exec(line);
    if (!found) {
      continue;
    }
    const [, indent, name, at, spent] = found;
    if (indent === "" && name === "paint") {
      paints.push({ at: Number(at), spent: Number(spent), stages: {} });
    } else if (indent === "  " && paints.length) {
      paints.at(-1).stages[name] = (paints.at(-1).stages[name] ?? 0) + Number(spent);
    } else if (indent === "    " && paints.length) {
      paints.at(-1).stages[name] = (paints.at(-1).stages[name] ?? 0) + Number(spent);
    }
  }
  return paints;
}

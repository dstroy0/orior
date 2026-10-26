// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// What the claim's tests share: the editor taking the keys, a rest at a point and the reveal it brings, and where
// the wordmark and the panes stand.

import { claimed, revealed } from "../gui/src-js/claim.js";
import { editing, openFile } from "../gui/src-js/edit.js";
import { showView } from "../gui/src-js/views.js";

const { ui } = window.__harness;

export const GAP = 12;
// The slide in and, a slide after, the work taking the part's room.
export const SLIDE = 900;

// hello.py open in the editor, the editor holding the keys and the claim made.
export async function editingHello() {
  showView("edit");
  await openFile("hello.py");
  await ui.waitFor(() => editing().active === "hello.py" && editing().editor?.s, 10000);
  await ui.click(document.querySelector("#mode-edit .editor"));
  await ui.waitFor(() => claimed(), 3000, "the claim made as the editor takes the keys");
}

// The pointer rested at a point of the window, and the reveal it brings.
export async function restedAt(x, y, zone) {
  await ui.hover({ x, y });
  await ui.waitFor(() => revealed() === zone, 3000, `a rest at ${x},${y} reveals ${zone}`);
  await ui.rest(SLIDE);
}

export const middle = () => ({ x: Math.round(window.innerWidth / 2), y: Math.round(window.innerHeight / 2) });

// The wordmark as it stands, and whether it stands whole, or shrunk to nothing, at the place it lives:
// where the bar lays it out with the bar shown, `inset` across and down from it while the claim holds.
export const mark = () => document.getElementById("bar-mark");
export const markAt = (whole, inset = 0) => {
  const box = mark().getBoundingClientRect();
  const home = Math.abs(box.left - mark().offsetLeft - inset) < 1 && Math.abs(box.top - mark().offsetTop - inset) < 1;
  return home && (whole ? Math.abs(box.width - mark().offsetWidth) < 1 : box.width < 1);
};

// Whether a part's pane stands at `left`, `top`, `right` and `bottom`, each null where it is not asked,
// rounded as the work is.
export const paneAt = (part, { left = null, top = null, right = null, bottom = null }) => {
  const box = part.getBoundingClientRect();
  const near = (have, want) => want === null || Math.abs(have - want) < 1;
  return near(box.left, left) && near(box.top, top) && near(box.right, right) && near(box.bottom, bottom) && getComputedStyle(part).borderTopLeftRadius === "7px";
};
export const paneSaid = (part) => {
  const box = part.getBoundingClientRect();
  return `${box.left},${box.top} to ${box.right},${box.bottom}, rounded ${getComputedStyle(part).borderTopLeftRadius}`;
};
export const markSaid = () => {
  const box = mark().getBoundingClientRect();
  return `the wordmark at ${box.left},${box.top}, ${box.width} wide; it lives at ${mark().offsetLeft},${mark().offsetTop}, ${mark().offsetWidth} wide`;
};

// What `read` gives on every frame from now: a function that stops the reading and gives its rows.
export function sampled(read) {
  const rows = [];
  let going = true;
  const frame = () => {
    if (going) {
      rows.push(read());
      requestAnimationFrame(frame);
    }
  };
  requestAnimationFrame(frame);
  return () => {
    going = false;
    return rows;
  };
}

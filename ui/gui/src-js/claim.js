// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The claim: while the reader edits, the work takes the bar's row, the toolbar's, the strip's column
// and the status bar's row, and keeps the frame's right gap on every side, so the editor moves inside
// a frame of that gap; inside the work the editor's text takes the tabs row's room and the minimap's.
// A pointer resting in the gap at an edge for RESTS brings that edge's part back: the status bar at
// the foot, the bar, the toolbar and the tabs row at the head, the strip at the left, the minimap at
// the right, and in the controls' corner the three controls alone, as a tab, a ghost of them showing
// as the pointer nears.
// A part goes back LINGER after the pointer leaves it, and one edge's part comes only once another's
// has gone, so no two slides run at once. The claim is made as the editor takes the keys and let go as
// they leave it. An edge held, as the foot is while a stuck plugin's Stop stands on the status bar,
// shows its part for as long as it is held, the pointer's leaving not taking it away.

import { showsOnWork, startBend } from "./bend.js";
import { slatFrom, startSlats } from "./slat.js";

const RESTS = 300;
const LINGER = 450;
// The slide's length, as --slide sets it.
const SLIDE = 300;
// The criterion between a pointer that means to stop in an edge's gap and one passing through it, in
// pixels a millisecond, read over the last SPAN milliseconds of its path: about 6 pixels each 100. A
// rest is RESTS with the pointer under it; a pointer that crosses a gap faster than it is a miss, and
// neither the gap's part nor the controls' ghost answers it.
const CRITERION = 0.06;
const SPAN = 50;
// The frame's gap, as --frame-right sets it, and how many gaps out the controls' ghost shows.
const GAP = 12;
const NEAR = 3;

let zoneNow = "";
let resting = 0;
let leaving = 0;
let unclaiming = 0;
let nearing = 0;
// The pointer's path over the last SPAN milliseconds, oldest first.
const trail = [];
// The edges held shown.
const held = new Set();

export const claimed = () => document.body.hasAttribute("data-claim");
export const revealed = () => document.body.dataset.reveal ?? "";

const controlsWidth = () => document.getElementById("window-controls").offsetWidth || 120;

// The edge the point stands in: the controls' corner, the head, the foot, the left, the right, or none.
export function zoneAt(x, y) {
  const width = window.innerWidth;
  const height = window.innerHeight;
  if (y < GAP && x > width - controlsWidth() - 2 * GAP) {
    return "controls";
  }
  if (y < GAP) {
    return "top";
  }
  if (y > height - GAP) {
    return "bottom";
  }
  if (x < GAP) {
    return "left";
  }
  if (x > width - GAP) {
    return "right";
  }
  return "";
}

function nearControls(x, y) {
  return y < GAP * NEAR && x > window.innerWidth - controlsWidth() - GAP * NEAR;
}

// The parts a reveal shows, which hold it while the pointer is over them.
function partsOf(zone) {
  const byId = (id) => document.getElementById(id);
  if (zone === "top") {
    return [document.querySelector("header.bar"), byId("toolbar"), ...document.querySelectorAll('#mode-edit .desk[data-tabs="row"] > .tabs')];
  }
  if (zone === "right") {
    return [...document.querySelectorAll("#mode-edit .editor > .ed-mini")];
  }
  if (zone === "bottom") {
    return [byId("statusbar")];
  }
  if (zone === "left") {
    return [byId("strip")];
  }
  if (zone === "controls") {
    return [byId("window-controls")];
  }
  return [];
}

// Where the pointer stands, as its last move left it: the point an edge's slat is pulled from.
const pointer = { x: 0, y: 0 };

// The reveal waiting for the part shown to go, or 0.
let switching = 0;

function show(zone) {
  if (zone) {
    slatFrom(zone, pointer.x, pointer.y);
    document.body.dataset.reveal = zone;
  } else {
    delete document.body.dataset.reveal;
  }
}

// The controls stand in the head, so the one comes as the other goes without waiting for it.
const together = (one, other) => [one, other].every((zone) => zone === "top" || zone === "controls");

// Shows `zone`'s part, or none, where none is an edge held's part while the claim stands; another
// edge's part shown goes first, over its slide, and `zone`'s comes once it has gone.
function setReveal(given) {
  const zone = given || (claimed() ? [...held][0] ?? "" : "");
  window.clearTimeout(switching);
  const shown = revealed();
  if (zone && shown && zone !== shown && !together(zone, shown)) {
    show("");
    switching = window.setTimeout(() => show(zone), SLIDE);
    return;
  }
  show(zone);
}

function overParts(target) {
  return partsOf(revealed()).some((part) => part && part.contains(target));
}

// The pointer's speed over the last SPAN milliseconds of its path, `event` its latest point.
function paceOf(event) {
  trail.push({ x: event.clientX, y: event.clientY, at: event.timeStamp });
  while (trail.length > 2 && event.timeStamp - trail[0].at > SPAN) {
    trail.shift();
  }
  const lapse = event.timeStamp - trail[0].at;
  return lapse > 0 ? Math.hypot(event.clientX - trail[0].x, event.clientY - trail[0].y) / lapse : 0;
}

// The pointer gone out of the window: a leave across whatever edge it crossed, not a rest in that edge's
// gap, so nothing that was waiting on its rest comes out.
function left() {
  window.clearTimeout(resting);
  window.clearTimeout(nearing);
  zoneNow = "";
  trail.length = 0;
  document.body.removeAttribute("data-near");
}

function moved(event) {
  if (!claimed()) {
    return;
  }
  const zone = zoneAt(event.clientX, event.clientY);
  pointer.x = event.clientX;
  pointer.y = event.clientY;
  const passing = paceOf(event) > CRITERION;
  const near = !revealed() && zone !== "controls" && nearControls(event.clientX, event.clientY);
  document.body.toggleAttribute("data-near", near && !passing);
  // a pointer come to a stop near the corner after a fast move shows the ghost once it has settled
  window.clearTimeout(nearing);
  if (near && passing) {
    nearing = window.setTimeout(() => document.body.toggleAttribute("data-near", !revealed()), SPAN * 2);
  }
  // a pointer passing through starts its rest again with each move, so the rest begins once it slows
  if (zone !== zoneNow || passing) {
    zoneNow = zone;
    window.clearTimeout(resting);
    if (zone && zone !== revealed()) {
      resting = window.setTimeout(() => setReveal(zone), RESTS);
    }
  }
  if (revealed() && !zone && !overParts(event.target)) {
    window.clearTimeout(leaving);
    leaving = window.setTimeout(function stay() {
      const parts = partsOf(revealed());
      if (held.has(revealed()) || parts.some((part) => part && (part.matches(":hover") || part.contains(document.activeElement)))) {
        leaving = window.setTimeout(stay, LINGER);
        return;
      }
      setReveal("");
    }, LINGER);
  } else {
    window.clearTimeout(leaving);
  }
}

export function setClaim(on) {
  window.clearTimeout(unclaiming);
  if (on === claimed()) {
    return;
  }
  document.body.toggleAttribute("data-claim", on);
  if (!on) {
    window.clearTimeout(resting);
    window.clearTimeout(leaving);
    zoneNow = "";
    setReveal("");
    document.body.removeAttribute("data-near");
  } else if (held.size) {
    setReveal("");
  }
}

// Holds `zone`'s part shown while the claim stands, or with `on` false lets it go as any part goes.
export function holdShown(zone, on) {
  if (on === held.has(zone)) {
    return;
  }
  if (on) {
    held.add(zone);
    if (claimed() && revealed() !== zone) {
      setReveal(zone);
    }
    return;
  }
  held.delete(zone);
  if (revealed() === zone && !partsOf(zone).some((part) => part && part.matches(":hover"))) {
    setReveal("");
  }
}

const inEditor = (node) => node instanceof Element && node.closest("#mode-edit .editor") !== null;

export function startClaim() {
  document.addEventListener("focusin", (event) => {
    if (inEditor(event.target)) {
      setClaim(true);
    }
  });
  document.addEventListener("focusout", () => {
    window.clearTimeout(unclaiming);
    // the keys may be passing to a part a reveal shows, which keeps the claim; else the claim goes
    // once they have settled elsewhere
    unclaiming = window.setTimeout(() => {
      const active = document.activeElement;
      if (!inEditor(active) && !overParts(active)) {
        setClaim(false);
      }
    }, 150);
  });
  document.addEventListener("pointermove", moved, { passive: true });
  document.documentElement.addEventListener("pointerleave", left);
  // the controls' tab, its lip with it, comes onto the work from its top
  const controls = document.getElementById("window-controls");
  showsOnWork(() => {
    const box = controls.getBoundingClientRect();
    const lip = Number.parseFloat(getComputedStyle(controls, "::before").left) || 0;
    return { left: box.left + Math.min(0, lip), top: box.top, right: box.right, bottom: box.bottom };
  }, "top");
  startSlats();
  startBend();
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The claim: while the reader edits, the work takes the bar's row, the toolbar's, the strip's column
// and the status bar's row, and keeps the frame's right gap on every side, so the editor moves inside
// a frame of that gap. A pointer resting in the gap at an edge for RESTS brings that edge's part back:
// the status bar at the foot, the bar and the toolbar at the head, the strip at the left, and in the
// controls' corner the three controls alone, as a tab, a ghost of them showing as the pointer nears.
// A part goes back LINGER after the pointer leaves it. With the bar away the eye stands in the
// controls' tab. The claim is made as the editor takes the keys and let go as they leave it.

const RESTS = 350;
const LINGER = 450;
// The frame's gap, as --frame-right sets it, and how many gaps out the controls' ghost shows.
const GAP = 12;
const NEAR = 3;

let zoneNow = "";
let resting = 0;
let leaving = 0;
let unclaiming = 0;

export const claimed = () => document.body.hasAttribute("data-claim");
export const revealed = () => document.body.dataset.reveal ?? "";

const controlsWidth = () => document.getElementById("window-controls").offsetWidth || 120;

// The edge the point stands in: the controls' corner, the head, the foot, the left, or none.
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
  return "";
}

function nearControls(x, y) {
  return y < GAP * NEAR && x > window.innerWidth - controlsWidth() - GAP * NEAR;
}

// The parts a reveal shows, which hold it while the pointer is over them.
function partsOf(zone) {
  const byId = (id) => document.getElementById(id);
  if (zone === "top") {
    return [document.querySelector("header.bar"), byId("toolbar")];
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

function setReveal(zone) {
  if (zone) {
    document.body.dataset.reveal = zone;
  } else {
    delete document.body.dataset.reveal;
  }
}

function overParts(target) {
  return partsOf(revealed()).some((part) => part && part.contains(target));
}

function moved(event) {
  if (!claimed()) {
    return;
  }
  const zone = zoneAt(event.clientX, event.clientY);
  document.body.toggleAttribute("data-near", !revealed() && zone !== "controls" && nearControls(event.clientX, event.clientY));
  if (zone !== zoneNow) {
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
      if (parts.some((part) => part && (part.matches(":hover") || part.contains(document.activeElement)))) {
        leaving = window.setTimeout(stay, LINGER);
        return;
      }
      setReveal("");
    }, LINGER);
  } else {
    window.clearTimeout(leaving);
  }
}

// The eye moves into the controls' tab as the bar goes, and back to the bar's head as it returns.
function eyePlaced(claim) {
  const mark = document.getElementById("bar-mark");
  if (claim) {
    document.getElementById("window-controls").prepend(mark);
  } else {
    document.querySelector("header.bar").prepend(mark);
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
  }
  eyePlaced(on);
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
}

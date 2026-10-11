// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The panes that take the editor's room: the job list, the explorer and the definitions. A moment
// after the app loads, after its view shows, or after the pointer leaves one, it collapses, giving
// its width back and sliding up and toward the edge it stands at, and the pointer reaching that edge
// of the window brings it back; one brought back that the pointer never comes onto goes again a
// moment later. A pane that holds the pointer, that is being typed in, or that a menu is open over,
// or a left pane with the pointer on the tool strip beside it,
// stays until all have left it, as every pane does while the window's frame is held, and is looked
// at again every REST while it stays. A pane is being typed in from a key pressed in it until the
// next press of the pointer anywhere: what a click leaves the keys on does not keep it. A pane that
// collapses holding the keys hands them to the editor beside it. A pane shown for the reader to look
// at, as Find Usages shows the explorer, stays until the pointer has come onto it and left. Whether
// panes collapse on their own is the reader's to set, and kept; Ctrl+B shows or collapses the view's
// own pane either way.

import { menuOpen } from "./menu.js";
import { preempt, still } from "./motion.js";

const AUTO = "orior.panes.auto";

// How long the pointer stays away before a pane collapses, in milliseconds.
const REST = 700;

// How long a pane's inside takes to slide away, as style.css sets it, in milliseconds.
const SLIDE = 220;

const reduced = window.matchMedia("(prefers-reduced-motion: reduce)");

const panes = [];

// What the pointer is over, or null once it has left the window: the window's own record of where
// the pointer is, which a pointer gone out of the window over a pane does not leave standing.
let pointerAt = null;
document.addEventListener("pointerover", (event) => (pointerAt = event.target), true);
document.addEventListener("pointermove", (event) => (pointerAt = event.target), true);
document.addEventListener(
  "pointerout",
  (event) => {
    if (!event.relatedTarget) {
      pointerAt = null;
    }
  },
  true,
);
window.addEventListener("blur", () => (pointerAt = null));

const holdsPointer = (node) => Boolean(node && pointerAt && node.contains(pointerAt));

export function autoCollapse() {
  return localStorage.getItem(AUTO) !== "false";
}

// Shows a pane or collapses it. A pane collapsing holds its room while its inside slides away and
// gives it back in one step after; one shown takes its room at once and its inside slides in.
function setShown(pane, shown) {
  const { node } = pane;
  window.clearTimeout(pane.slide);
  if (shown === node.classList.contains("collapsed") && !reduced.matches) {
    preempt(SLIDE + 60);
  }
  if (!shown && !node.classList.contains("collapsed") && !reduced.matches) {
    const box = node.getBoundingClientRect();
    node.style.setProperty("--held", `${node.classList.contains("toward-bottom") ? box.height : box.width}px`);
    node.classList.add("sliding");
    pane.slide = window.setTimeout(() => {
      node.classList.remove("sliding");
      node.style.removeProperty("--held");
    }, SLIDE);
  } else if (shown) {
    node.classList.remove("sliding");
    node.style.removeProperty("--held");
    // The frame the editor gives its room back in redraws the whole window; the inside starts to
    // slide in once that frame is drawn, and none of the slide's frames go to it.
    if (node.classList.contains("collapsed") && !reduced.matches) {
      node.classList.add("arriving");
      requestAnimationFrame(() => requestAnimationFrame(() => node.classList.remove("arriving")));
    }
  }
  node.classList.toggle("collapsed", !shown);
  node.inert = !shown;
  pane.edge.hidden = shown;
}

function collapseLater(pane) {
  window.clearTimeout(pane.timer);
  pane.timer = window.setTimeout(() => tryCollapse(pane), REST);
}

function tryCollapse(pane) {
  const { node } = pane;
  if (!autoCollapse() || node.hidden || node.classList.contains("collapsed") || node.offsetParent === null) {
    return;
  }
  const typing = pane.keyed && node.contains(document.activeElement);
  if (pane.held) {
    return;
  }
  // The tool strip beside a left pane is a part of it: the pointer on the strip keeps it.
  const onStrip = node.classList.contains("toward-left") && holdsPointer(document.getElementById("strip"));
  if (holdsPointer(node) || onStrip || typing || pane.pinned || menuOpen() || still()) {
    collapseLater(pane);
    return;
  }
  const held = node.contains(document.activeElement);
  setShown(pane, false);
  if (held) {
    node.closest(".mode")?.querySelector(".ed-input")?.focus();
  }
}

// Makes `node` a pane that collapses toward `side`, left, right or bottom, with its edge in the
// view; `own` marks the view's own pane, the one Ctrl+B shows and collapses.
export function keepPane(node, side, { own = false } = {}) {
  const edge = Object.assign(document.createElement("div"), { className: `pane-edge pane-edge-${side}`, hidden: true });
  edge.setAttribute("aria-hidden", "true");
  node.closest(".mode").append(edge);
  node.classList.add("collapsible", `toward-${side}`);
  const pane = { node, edge, timer: 0, keyed: false, pinned: false, own };
  node.addEventListener("pointerenter", () => window.clearTimeout(pane.timer));
  node.addEventListener("pointerleave", () => {
    pane.pinned = false;
    collapseLater(pane);
  });
  node.addEventListener("keydown", () => (pane.keyed = true), true);
  node.addEventListener("focusout", () => collapseLater(pane));
  edge.addEventListener("pointerenter", () => {
    if (!node.hidden) {
      setShown(pane, true);
      collapseLater(pane);
    }
  });
  // A pane hidden while it has nothing to show starts its wait again when it shows.
  new MutationObserver(() => !node.hidden && collapseLater(pane)).observe(node, { attributes: true, attributeFilter: ["hidden"] });
  panes.push(pane);
}

// Starts every pane in sight waiting to collapse, as the app finishes loading and as a view shows:
// a pane the pointer never comes to still goes a moment later.
export function settlePanes() {
  panes.forEach(collapseLater);
}

// A press of the pointer anywhere ends the typing that keeps a pane.
window.addEventListener(
  "pointerdown",
  () => {
    for (const pane of panes) {
      pane.keyed = false;
    }
  },
  true,
);

// Holds the pane at `node` shown, as a window that shows it alone does, or lets it go again.
export function holdPane(node, held) {
  const pane = panes.find((one) => one.node === node);
  if (!pane) {
    return;
  }
  pane.held = held;
  if (held) {
    setShown(pane, true);
  } else {
    collapseLater(pane);
  }
}

// Moves the pane at `node` to collapse toward `side`, its edge with it.
export function movePane(node, side) {
  const pane = panes.find((one) => one.node === node);
  if (!pane) {
    return;
  }
  node.classList.remove("toward-left", "toward-right", "toward-bottom");
  node.classList.add(`toward-${side}`);
  pane.edge.className = `pane-edge pane-edge-${side}`;
}

// The view's own pane, wherever it stands.
function leftPane() {
  return panes.find((pane) => pane.own && pane.node.offsetParent !== null);
}

export function paneShown() {
  const pane = leftPane();
  return Boolean(pane && !pane.node.classList.contains("collapsed"));
}

// Shows the view's own pane or collapses it. A pane shown from the keys takes them, and stays while
// it holds them; one shown with `take` off leaves the keys where they are and stays until the pointer
// has been on it, or, with `pin` off as well, as the strip shows it, only while the pointer is on
// the pane or on the strip.
export function togglePane(shown = !paneShown(), { take = true, pin = true } = {}) {
  const pane = leftPane();
  if (!pane) {
    return;
  }
  setShown(pane, shown);
  if (shown && take) {
    pane.keyed = true;
    pane.node.querySelector("input, button")?.focus();
  } else if (shown) {
    pane.pinned = pin;
  }
  if (shown) {
    collapseLater(pane);
  }
}

// Whether the pane at `node` shows.
export function paneNodeShown(node) {
  return !node.hidden && !node.classList.contains("collapsed");
}

// Shows the pane at `node` or collapses it, a pane shown this way staying until the pointer has been
// on it, or with `pin` off only while the pointer is on it or on the strip.
export function togglePaneNode(node, shown = !paneNodeShown(node), { pin = true } = {}) {
  const pane = panes.find((one) => one.node === node);
  if (!pane) {
    return;
  }
  setShown(pane, shown);
  if (shown) {
    pane.pinned = pin;
    collapseLater(pane);
  }
}

export function setAutoCollapse(on = !autoCollapse()) {
  localStorage.setItem(AUTO, String(on));
  for (const pane of panes) {
    if (on) {
      collapseLater(pane);
    } else {
      setShown(pane, true);
    }
  }
}

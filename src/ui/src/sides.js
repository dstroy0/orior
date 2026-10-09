// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The panes that take the editor's room: the job list, the explorer and the definitions. A moment
// after the app loads, after its view shows, or after the pointer leaves one, it collapses, giving
// its width back and sliding up and toward the edge it stands at, and the pointer reaching that edge
// of the window brings it back; one brought back that the pointer never comes onto goes again a
// moment later. A pane that holds the pointer, that is being typed in, or that a menu is open over,
// stays until all have left it, as every pane does while the window's frame is held, and is looked
// at again every REST while it stays. A pane is being typed in from a key pressed in it until the
// next press of the pointer anywhere: what a click leaves the keys on does not keep it. A pane that
// collapses holding the keys hands them to the editor beside it. A pane shown for the reader to look
// at, as Find Usages shows the explorer, stays until the pointer has come onto it and left. Whether
// panes collapse on their own is the reader's to set, and kept; Ctrl+B shows or collapses the view's
// own pane either way.

import { menuOpen } from "./menu.js";
import { still } from "./motion.js";

const AUTO = "orior.panes.auto";

// How long the pointer stays away before a pane collapses, in milliseconds.
const REST = 700;

const panes = [];

export function autoCollapse() {
  return localStorage.getItem(AUTO) !== "false";
}

function setShown(pane, shown) {
  pane.node.classList.toggle("collapsed", !shown);
  pane.node.inert = !shown;
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
  if (node.matches(":hover") || typing || pane.pinned || menuOpen() || still()) {
    collapseLater(pane);
    return;
  }
  const held = node.contains(document.activeElement);
  setShown(pane, false);
  if (held) {
    node.closest(".mode")?.querySelector(".ed-input")?.focus();
  }
}

// Makes `node` a pane that collapses toward `side`, left or right, with its edge in the view.
export function keepPane(node, side) {
  const edge = Object.assign(document.createElement("div"), { className: `pane-edge pane-edge-${side}`, hidden: true });
  edge.setAttribute("aria-hidden", "true");
  node.closest(".mode").append(edge);
  node.classList.add("collapsible", `toward-${side}`);
  const pane = { node, edge, timer: 0, keyed: false, pinned: false };
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

// The view's own pane, the one at its left.
function leftPane() {
  return panes.find((pane) => pane.node.classList.contains("toward-left") && pane.node.offsetParent !== null);
}

export function paneShown() {
  const pane = leftPane();
  return Boolean(pane && !pane.node.classList.contains("collapsed"));
}

// Shows the view's own pane or collapses it. A pane shown from the keys takes them, and stays while
// it holds them; one shown with `take` off leaves the keys where they are and stays until the pointer
// has been on it.
export function togglePane(shown = !paneShown(), { take = true } = {}) {
  const pane = leftPane();
  if (!pane) {
    return;
  }
  setShown(pane, shown);
  if (shown && take) {
    pane.keyed = true;
    pane.node.querySelector("input, button")?.focus();
  } else if (shown) {
    pane.pinned = true;
  }
  if (shown) {
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

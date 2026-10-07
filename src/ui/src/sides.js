// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The panes that take the editor's room: the job list, the explorer and the definitions. A moment
// after the app loads, after its view shows, or after the pointer leaves one, it collapses, giving
// its width back and sliding up and toward the edge it stands at, and the pointer reaching that edge
// of the window brings it back. A pane that holds the keys, or that a menu is open over, stays until
// both have left it. Whether panes collapse on
// their own is the reader's to set, and kept; Ctrl+B shows or collapses the view's own pane either way.

import { menuOpen } from "./menu.js";

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
  if (node.matches(":hover") || node.contains(document.activeElement)) {
    return;
  }
  if (menuOpen()) {
    collapseLater(pane);
    return;
  }
  setShown(pane, false);
}

// Makes `node` a pane that collapses toward `side`, left or right, with its edge in the view.
export function keepPane(node, side) {
  const edge = Object.assign(document.createElement("div"), { className: `pane-edge pane-edge-${side}`, hidden: true });
  edge.setAttribute("aria-hidden", "true");
  node.closest(".mode").append(edge);
  node.classList.add("collapsible", `toward-${side}`);
  const pane = { node, edge, timer: 0 };
  node.addEventListener("pointerenter", () => window.clearTimeout(pane.timer));
  node.addEventListener("pointerleave", () => collapseLater(pane));
  node.addEventListener("focusout", () => collapseLater(pane));
  edge.addEventListener("pointerenter", () => !node.hidden && setShown(pane, true));
  // A pane hidden while it has nothing to show starts its wait again when it shows.
  new MutationObserver(() => !node.hidden && collapseLater(pane)).observe(node, { attributes: true, attributeFilter: ["hidden"] });
  panes.push(pane);
}

// Starts every pane in sight waiting to collapse, as the app finishes loading and as a view shows:
// a pane the pointer never comes to still goes a moment later.
export function settlePanes() {
  panes.forEach(collapseLater);
}

// The view's own pane, the one at its left.
function leftPane() {
  return panes.find((pane) => pane.node.classList.contains("toward-left") && pane.node.offsetParent !== null);
}

export function paneShown() {
  const pane = leftPane();
  return Boolean(pane && !pane.node.classList.contains("collapsed"));
}

// Shows the view's own pane or collapses it. A pane shown from the keys takes them, and stays while
// it holds them.
export function togglePane(shown = !paneShown()) {
  const pane = leftPane();
  if (!pane) {
    return;
  }
  setShown(pane, shown);
  if (shown) {
    pane.node.querySelector("input, button")?.focus();
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

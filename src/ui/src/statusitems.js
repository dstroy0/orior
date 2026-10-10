// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A status bar of the reader's. Each item of the bar, and each part of the editor's own line on it,
// is shown or hidden from the bar's menu or View, Status Bar, and moved by the menu's Move Left and
// Move Right or by dragging it along the bar; the editor's parts move among themselves. What the
// reader set is kept, and Reset the Status Bar puts it back as it was.
//
// The tool strip hides until the pointer reaches the window's left edge where View, Hide Tool Strip
// Until Its Edge is on: it slides out over the work there, and back a moment after the pointer
// leaves it.

import { menuOn, menuOpen } from "./menu.js";

const KEY = "orior.statusbar";
const EDGE = "orior.strip.edge";

// How long the strip stays after the pointer leaves it, in milliseconds.
const LINGER = 500;

// How far the pointer moves with an item held before the item is being dragged.
const DRAG = 6;

// Each item: its name, its label, and the line it moves along, the bar's or the editor's.
const ITEMS = [
  ["machine", "The Tree's Machine", "bar"],
  ["crumbs", "Breadcrumbs", "bar"],
  ["runs", "Runs and Files Being Read", "bar"],
  ["said", "What Was Last Done", "bar"],
  ["report", "Report", "bar"],
  ["zoom", "Zoom", "bar"],
  ["memory", "Memory", "bar"],
  ["editor", "The Editor's Line", "bar"],
  ["vim", "Vim Mode", "editor"],
  ["cursor", "Line and Column", "editor"],
  ["selection", "Selection", "editor"],
  ["reading", "Share Read", "editor"],
  ["cursors", "Cursors", "editor"],
  ["eol", "Line Ends", "editor"],
  ["encoding", "Encoding", "editor"],
  ["indent", "Indent", "editor"],
  ["lock", "Lock", "editor"],
];

const labelOf = (name) => ITEMS.find(([one]) => one === name)?.[1] ?? name;
const lineOf = (name) => ITEMS.find(([one]) => one === name)?.[2] ?? "bar";

let kept = { hidden: [], order: [] };
let sheet = null;

function load() {
  try {
    const read = JSON.parse(localStorage.getItem(KEY) ?? "null");
    kept = { hidden: Array.isArray(read?.hidden) ? read.hidden : [], order: Array.isArray(read?.order) ? read.order : [] };
  } catch {
    kept = { hidden: [], order: [] };
  }
}

// The items of a line in the order they stand: those the reader moved as moved, the rest after them
// in their own order.
function ordered(line) {
  const names = ITEMS.filter(([, , one]) => one === line).map(([name]) => name);
  return [...kept.order.filter((name) => names.includes(name)), ...names.filter((name) => !kept.order.includes(name))];
}

// Sets each item's place and whether it shows, as a sheet of rules over the bar.
function apply() {
  const rules = [];
  for (const line of ["bar", "editor"]) {
    ordered(line).forEach((name, at) => rules.push(`[data-item="${name}"] { order: ${at}; }`));
  }
  for (const name of kept.hidden) {
    rules.push(`[data-item="${name}"] { display: none !important; }`);
  }
  sheet.textContent = rules.join("\n");
  localStorage.setItem(KEY, JSON.stringify(kept));
}

export const itemShown = (name) => !kept.hidden.includes(name);

export function setItemShown(name, shown) {
  kept.hidden = kept.hidden.filter((one) => one !== name);
  if (!shown) {
    kept.hidden.push(name);
  }
  apply();
}

// Moves an item `by` places along its line.
export function moveItem(name, by) {
  const line = ordered(lineOf(name));
  const at = line.indexOf(name);
  const to = Math.max(0, Math.min(line.length - 1, at + by));
  line.splice(at, 1);
  line.splice(to, 0, name);
  place(line);
}

// Takes `line`, an order of one line's items, as the reader's.
function place(line) {
  kept.order = [...kept.order.filter((name) => !line.includes(name)), ...line];
  apply();
}

export function resetItems() {
  kept = { hidden: [], order: [] };
  apply();
}

// The items, each shown or hidden by a press, as View, Status Bar and the bar's menu list them.
export function statusItems() {
  return [...ITEMS.map(([name, label]) => ({ label, checked: itemShown(name), run: () => setItemShown(name, !itemShown(name)) })), "-", { label: "Reset the Status Bar", run: resetItems }];
}

function barMenu(event) {
  const name = event.target.closest?.("[data-item]")?.dataset.item;
  const label = name ? labelOf(name) : "";
  return [
    ...(name
      ? [
          { label: `Move ${label} Left`, run: () => moveItem(name, -1) },
          { label: `Move ${label} Right`, run: () => moveItem(name, 1) },
          { label: `Hide ${label}`, run: () => setItemShown(name, false) },
          "-",
        ]
      : []),
    ...statusItems(),
  ];
}

// An item dragged along its line is let go before the item the pointer is over, or after it past
// that item's middle.
function startDragging(bar) {
  bar.addEventListener("pointerdown", (event) => {
    const held = event.target.closest?.("[data-item]");
    if (event.button !== 0 || !held || !bar.contains(held)) {
      return;
    }
    const from = event.clientX;
    let dragging = false;
    const move = (moved) => {
      if (!dragging && Math.abs(moved.clientX - from) > DRAG) {
        dragging = true;
        held.classList.add("dragged");
      }
    };
    const up = (released) => {
      window.removeEventListener("pointermove", move, true);
      if (!dragging) {
        return;
      }
      held.classList.remove("dragged");
      // The press that ends a drag is no press on the item.
      window.addEventListener("click", (click) => click.stopPropagation(), { capture: true, once: true });
      const name = held.dataset.item;
      const line = ordered(lineOf(name));
      const over = [...document.elementsFromPoint(released.clientX, released.clientY)].map((node) => node.closest?.("[data-item]")).find((node) => node && node !== held && lineOf(node.dataset.item) === lineOf(name) && line.includes(node.dataset.item));
      if (!over) {
        return;
      }
      const box = over.getBoundingClientRect();
      const without = line.filter((one) => one !== name);
      const at = without.indexOf(over.dataset.item) + (released.clientX > box.left + box.width / 2 ? 1 : 0);
      without.splice(at, 0, name);
      place(without);
    };
    window.addEventListener("pointermove", move, true);
    window.addEventListener("pointerup", up, { capture: true, once: true });
  });
}

// The tool strip hidden until the pointer reaches the window's left edge.

export const stripEdge = () => localStorage.getItem(EDGE) === "true";

export function setStripEdge(on = !stripEdge()) {
  localStorage.setItem(EDGE, String(on));
  document.body.toggleAttribute("data-strip-edge", on);
  document.getElementById("strip").classList.remove("shown");
}

function startEdge() {
  const strip = document.getElementById("strip");
  const edge = Object.assign(document.createElement("div"), { className: "strip-edge" });
  edge.setAttribute("aria-hidden", "true");
  strip.after(edge);
  let leaving = 0;
  const show = () => {
    window.clearTimeout(leaving);
    strip.classList.add("shown");
  };
  const hide = () => {
    window.clearTimeout(leaving);
    leaving = window.setTimeout(function stay() {
      if (menuOpen() || strip.matches(":hover") || strip.contains(document.activeElement)) {
        leaving = window.setTimeout(stay, LINGER);
        return;
      }
      strip.classList.remove("shown");
    }, LINGER);
  };
  edge.addEventListener("pointerenter", show);
  strip.addEventListener("pointerenter", show);
  strip.addEventListener("pointerleave", hide);
  strip.addEventListener("focusin", show);
  strip.addEventListener("focusout", hide);
  document.body.toggleAttribute("data-strip-edge", stripEdge());
}

export function startStatusItems() {
  sheet = Object.assign(document.createElement("style"), { id: "status-items" });
  document.head.append(sheet);
  load();
  apply();
  const bar = document.getElementById("statusbar");
  menuOn(bar, barMenu);
  startDragging(bar);
  startEdge();
}

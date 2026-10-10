// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Tool windows anywhere. Each tool window docks at the left, the right or the foot of the place it
// stands in: Debug and Terminal in the work, beside the views or under them; the explorer and the
// definitions in the edit view, beside the editor or under it; the jobs in the run view, beside the
// stage or under it. Each place is a grid, its content in the middle, the tool windows docked at
// each side in the order they were docked there, and a tool window keeps the width or the height it
// was dragged to at each side it docked at. Its icon on the tool strip dragged over its place shows
// where it can dock, and its menu, the icon's or View, Tool Windows, moves it. An icon the reader
// has no use for is taken off the strip, and its window closed, until Tool Windows brings it back.

import { showMenu } from "./menu.js";
import { holdPane, movePane } from "./sides.js";
import { openWindow, tell, windowId } from "./windows.js";

const KEY = "orior.docks";

// Each tool window: its node, the place it stands in and the content in the middle of it, the side
// it docks at first, the custom property its width is held in where it has one, its sizes at first,
// and its label.
const TOOLS = {
  explorer: { node: "explorer", place: "mode-edit", middle: ".desk", side: "left", width: "--side", sizes: { left: 304, right: 304, bottom: 260 }, label: "Explorer" },
  definitions: { node: "defs-side", place: "mode-edit", middle: ".desk", side: "right", width: "--defs", sizes: { left: 368, right: 368, bottom: 260 }, label: "Definitions" },
  jobs: { node: "job-side", place: "mode-run", middle: "#job-stage", side: "left", width: "--side", sizes: { left: 304, right: 304, bottom: 240 }, label: "Jobs" },
  debug: { node: "debug", place: "work", middle: ".shell", side: "bottom", width: null, sizes: { left: 420, right: 420, bottom: 240 }, label: "Debug", height: "orior.debug.height" },
  terminal: { node: "term", place: "work", middle: ".shell", side: "bottom", width: null, sizes: { left: 480, right: 480, bottom: 256 }, label: "Terminal", height: "orior.terminal.height" },
};

const SIDES = ["left", "right", "bottom"];

// The icons of the strip and the bar, each by its name, and the tool window it opens.
const ICONS = {
  explorer: "explorer",
  structure: "explorer",
  commit: "explorer",
  problems: "explorer",
  tests: "explorer",
  git: "explorer",
  run: "jobs",
  debug: "debug",
  terminal: "terminal",
  definitions: "definitions",
};

const ICON_LABELS = { explorer: "Explorer", structure: "Structure", commit: "Commit", problems: "Problems", tests: "Tests", git: "Git", run: "Run", debug: "Debug", terminal: "Terminal", definitions: "Definitions" };

// The smallest a docked window is made, and the share of its place it can take at most.
const LEAST = 120;
const MOST = 0.75;

// How far the pointer moves with an icon held before the icon is being dragged.
const DRAG = 8;

const state = { kept: { tools: {}, removed: [] }, hooks: null, drag: null };

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

function load() {
  try {
    const kept = JSON.parse(localStorage.getItem(KEY) ?? "null");
    state.kept = { tools: kept?.tools ?? {}, removed: kept?.removed ?? [] };
  } catch {
    state.kept = { tools: {}, removed: [] };
  }
}

function keep() {
  localStorage.setItem(KEY, JSON.stringify(state.kept));
}

const kept = (name) => (state.kept.tools[name] ??= {});

export function sideOf(name) {
  const side = kept(name).side;
  return SIDES.includes(side) ? side : TOOLS[name].side;
}

// The size a tool window was left at a side: the one kept, or for a panel at the foot the height it
// was dragged to before it docked anywhere else, or its first.
function sizeOf(name, side) {
  const size = kept(name).sizes?.[side];
  if (Number.isFinite(size)) {
    return size;
  }
  const tool = TOOLS[name];
  if (side === "bottom" && tool.height) {
    const height = parseFloat(localStorage.getItem(tool.height) ?? "");
    if (Number.isFinite(height)) {
      return height;
    }
  }
  return tool.sizes[side];
}

function placeOf(name) {
  const tool = TOOLS[name];
  return tool.place === "work" ? document.querySelector(".work") : document.getElementById(tool.place);
}

// Gives a tool window its size at the side it stands at.
function size(name) {
  const tool = TOOLS[name];
  const node = document.getElementById(tool.node);
  const side = sideOf(name);
  const px = `${Math.round(sizeOf(name, side))}px`;
  node.style.setProperty("--dock-size", px);
  if (tool.width && side !== "bottom") {
    node.style.setProperty(tool.width, px);
  } else if (tool.width) {
    node.style.removeProperty(tool.width);
  }
  if (tool.place === "work") {
    node.style.width = side === "bottom" ? "" : px;
    node.style.height = side === "bottom" ? px : "";
  }
}

// Lays out a place: its tool windows at the left in their order, its middle, those at the right,
// and those at its foot under them all.
function layout(place) {
  const names = Object.keys(TOOLS).filter((name) => TOOLS[name].place === place);
  const host = placeOf(names[0]);
  const at = (side) => names.filter((name) => sideOf(name) === side).sort((a, b) => (kept(a).order ?? 0) - (kept(b).order ?? 0));
  const [left, right, bottom] = SIDES.map(at);
  host.style.gridTemplateColumns = [...left.map(() => "auto"), "minmax(0, 1fr)", ...right.map(() => "auto")].join(" ");
  host.style.gridTemplateRows = ["minmax(0, 1fr)", ...bottom.map(() => "auto")].join(" ");
  const middle = host.querySelector(`:scope > ${TOOLS[names[0]].middle}`);
  middle.style.gridColumn = String(left.length + 1);
  middle.style.gridRow = "1";
  const put = (name, column, row) => {
    const node = document.getElementById(TOOLS[name].node);
    node.style.gridColumn = column;
    node.style.gridRow = row;
    node.dataset.dock = sideOf(name);
    size(name);
  };
  left.forEach((name, index) => put(name, String(index + 1), "1"));
  right.forEach((name, index) => put(name, String(left.length + 2 + index), "1"));
  bottom.forEach((name, index) => put(name, "1 / -1", String(2 + index)));
}

const places = () => [...new Set(Object.values(TOOLS).map((tool) => tool.place))];

// Docks a tool window at `side`, after those docked there already.
export function dock(name, side) {
  if (!TOOLS[name] || !SIDES.includes(side)) {
    return;
  }
  const entry = kept(name);
  entry.side = side;
  entry.order = Date.now();
  keep();
  const tool = TOOLS[name];
  const node = document.getElementById(tool.node);
  if (node.classList.contains("collapsible")) {
    movePane(node, side);
  }
  layout(tool.place);
  state.hooks?.changed();
}

// The sash on a tool window's inner edge, which drags its width, or its height at the foot.
function sash(name) {
  const node = document.getElementById(TOOLS[name].node);
  const grip = element("div", { className: "dock-sash" });
  grip.setAttribute("aria-hidden", "true");
  node.append(grip);
  grip.addEventListener("pointerdown", (event) => {
    if (event.button !== 0) {
      return;
    }
    event.preventDefault();
    grip.setPointerCapture(event.pointerId);
    const side = sideOf(name);
    const box = node.getBoundingClientRect();
    const room = placeOf(name).getBoundingClientRect();
    const most = (side === "bottom" ? room.height : room.width) * MOST;
    const move = (moved) => {
      const wanted = side === "left" ? moved.clientX - box.left : side === "right" ? box.right - moved.clientX : box.bottom - moved.clientY;
      const entry = kept(name);
      entry.sizes = { ...(entry.sizes ?? {}), [side]: Math.max(LEAST, Math.min(most, wanted)) };
      size(name);
    };
    const end = () => {
      grip.removeEventListener("pointermove", move);
      keep();
    };
    grip.addEventListener("pointermove", move);
    grip.addEventListener("pointerup", end, { once: true });
  });
}

// Taking an icon off the strip, and bringing it back.

export const iconRemoved = (icon) => state.kept.removed.includes(icon);

export function setIconRemoved(icon, removed) {
  state.kept.removed = state.kept.removed.filter((one) => one !== icon);
  if (removed) {
    state.kept.removed.push(icon);
    state.hooks?.close(icon);
  }
  keep();
  state.hooks?.changed();
}

// The menu of a tool window's icon: dock its window at a side, or take the icon off the strip.
export function iconMenu(icon, x, y) {
  const name = ICONS[icon];
  const side = sideOf(name);
  showMenu(x, y, [
    ...SIDES.map((one) => ({ label: `Dock ${TOOLS[name].label} ${one === "bottom" ? "at the Foot" : `at the ${one[0].toUpperCase()}${one.slice(1)}`}`, checked: side === one, run: () => dock(name, one) })),
    "-",
    { label: `Remove ${ICON_LABELS[icon]} from the Strip`, run: () => setIconRemoved(icon, true) },
  ]);
}

// Dragging an icon over its window's place shows the sides it can dock at, the one under the
// pointer lit, and letting go there docks it.

function zoneAt(drop, x, y) {
  return [...drop.querySelectorAll(".dock-zone")].find((zone) => {
    const box = zone.getBoundingClientRect();
    return x >= box.left && x <= box.right && y >= box.top && y <= box.bottom;
  });
}

function startDrag(icon, event) {
  const name = ICONS[icon];
  const room = placeOf(name);
  if (!room || room.offsetParent === null) {
    return false;
  }
  const box = room.getBoundingClientRect();
  const drop = element(
    "div",
    { className: "dock-drop" },
    ...SIDES.map((side) => {
      const zone = element("div", { className: `dock-zone dock-zone-${side}`, textContent: side === "bottom" ? "Foot" : `${side[0].toUpperCase()}${side.slice(1)}` });
      zone.dataset.side = side;
      return zone;
    }),
  );
  Object.assign(drop.style, { left: `${box.left}px`, top: `${box.top}px`, width: `${box.width}px`, height: `${box.height}px` });
  drop.setAttribute("aria-hidden", "true");
  document.body.append(drop);
  state.drag = { name, drop };
  moveDrag(event);
  return true;
}

function moveDrag(event) {
  const { drop } = state.drag;
  const under = zoneAt(drop, event.clientX, event.clientY);
  for (const zone of drop.querySelectorAll(".dock-zone")) {
    zone.classList.toggle("lit", zone === under);
  }
}

function endDrag(event, dropped) {
  const { name, drop } = state.drag;
  state.drag = null;
  const under = dropped ? zoneAt(drop, event.clientX, event.clientY) : null;
  drop.remove();
  if (under) {
    dock(name, under.dataset.side);
  }
}

// Lets an icon of the strip or the bar be dragged to dock its window, and its menu be opened.
export function dockIcon(icon, button) {
  button.addEventListener("contextmenu", (event) => {
    event.preventDefault();
    event.stopPropagation();
    iconMenu(icon, event.clientX, event.clientY);
  });
  button.addEventListener("pointerdown", (event) => {
    if (event.button !== 0) {
      return;
    }
    const from = { x: event.clientX, y: event.clientY };
    let dragging = false;
    const move = (moved) => {
      if (!dragging && Math.hypot(moved.clientX - from.x, moved.clientY - from.y) > DRAG) {
        dragging = startDrag(icon, moved);
        if (dragging) {
          button.dataset.dragged = "true";
        }
      } else if (dragging) {
        moveDrag(moved);
      }
    };
    // The pointer is followed over the whole window, the icon left behind as it goes.
    const up = (released) => {
      window.removeEventListener("pointermove", move, true);
      window.removeEventListener("keydown", escape, true);
      if (dragging) {
        endDrag(released, true);
        // The press that ends a drag is not a click on the icon.
        window.setTimeout(() => delete button.dataset.dragged, 0);
      }
    };
    const escape = (pressed) => {
      if (pressed.key === "Escape" && dragging) {
        pressed.preventDefault();
        dragging = false;
        endDrag(pressed, false);
      }
    };
    window.addEventListener("pointermove", move, true);
    window.addEventListener("pointerup", up, { once: true, capture: true });
    window.addEventListener("keydown", escape, true);
  });
}

// View, Tool Windows: each tool window's side, and each icon on the strip or taken off it.
export function toolWindowsItems() {
  return [
    ...Object.entries(TOOLS).map(([name, tool]) => ({
      label: tool.label,
      items: SIDES.map((side) => ({ label: side === "bottom" ? "At the Foot" : `At the ${side[0].toUpperCase()}${side.slice(1)}`, checked: sideOf(name) === side, run: () => dock(name, side) })),
    })),
    "-",
    ...Object.keys(ICONS).map((icon) => ({ label: `${ICON_LABELS[icon]} on the Strip`, checked: !iconRemoved(icon), run: () => setIconRemoved(icon, !iconRemoved(icon)) })),
  ];
}

// A tool window in a window of its own: the terminal, the explorer or the jobs, each a new window
// that shows it alone.

const FLOATING = { terminal: "Terminal", explorer: "Explorer", jobs: "Jobs" };

// View, Open Tool Window in a Window: the tool window opened alone in a new window, and closed here.
export async function float(name) {
  if (!FLOATING[name]) {
    return false;
  }
  await openWindow(["view", "solo", name, windowId]);
  state.hooks.closeTool(name);
  return true;
}

// The tool windows that open alone in a window, each with what a press on it does.
export const floatItems = (run) => Object.entries(FLOATING).map(([name, label]) => ({ label, checked: run === solo ? document.body.dataset.solo === name : undefined, run: () => run(name) }));

// View, Show Only a Tool Window: this window shows the tool window alone, filling it, and a file
// opened from it opens in the window `from` names, where one is named. With none named, or the one
// shown alone named again, the window shows everything again.
export function solo(name, from = null) {
  const was = document.body.dataset.solo;
  if (was) {
    const node = document.getElementById(TOOLS[was].node);
    delete node.dataset.soloTool;
    if (node.classList.contains("collapsible")) {
      holdPane(node, false);
    }
    delete document.body.dataset.solo;
    state.hooks.openElsewhere(null);
  }
  if (!FLOATING[name] || was === name) {
    places().forEach(layout);
    return;
  }
  const node = document.getElementById(TOOLS[name].node);
  document.body.dataset.solo = name;
  node.dataset.soloTool = "true";
  state.hooks.showTool(name);
  if (node.classList.contains("collapsible")) {
    holdPane(node, true);
  }
  if (from) {
    state.hooks.openElsewhere((path, line, col) => tell(from, "open", { path, line, col }));
  }
}

export const toolNames = () => Object.keys(TOOLS);
export const iconNames = () => Object.keys(ICONS);

// `hooks` closes the window of an icon taken off the strip, draws the strip again as the docks
// change, shows and closes a tool window, and sends the files opened in this window to another, or
// with null keeps them here.
export function startDocks(hooks) {
  state.hooks = hooks;
  load();
  for (const name of Object.keys(TOOLS)) {
    const node = document.getElementById(TOOLS[name].node);
    if (node.classList.contains("collapsible")) {
      movePane(node, sideOf(name));
    }
    sash(name);
  }
  places().forEach(layout);
}

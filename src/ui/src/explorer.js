// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The explorer beside the editor: its panes, one over the next, each opened and closed by its head.
// Search finds text in the tree's files, and shows from Find in Files. Usages lists where a symbol is
// used, and shows from Find Usages. Open Editors lists the tabs, the tree's own pane holds its files, Outline lists what the open file
// declares and Timeline the commits that touched it. The explorer's … menu shows or hides each pane,
// reads the tree again, and closes every folder. Which panes show and which are open is kept between
// visits.
//
// A pane's head is a row of the explorer's list one level above its rows, and the list's keys open
// and close it as they do a folder.

import { invoke } from "./bridge.js";
import { copyText, menuOn, showMenu } from "./menu.js";
import { symbolsOf } from "./outline.js";

const KEPT = "orior.panes";

const PANES = [
  ["search", "Search"],
  ["usages", "Usages"],
  ["open", "Open Editors"],
  ["folder", null],
  ["outline", "Outline"],
  ["timeline", "Timeline"],
];

const state = {
  panes: {},
  hooks: null,
  outline: { session: null, symbols: [], rows: [] },
  timeline: { path: null, commits: [] },
};

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

// A file's icon: a mark for its type in a color of the palette, or a page where the type has no mark.
const ICONS = {
  js: ["JS", 3], mjs: ["JS", 3], json: ["{}", 3], cfg: ["{}", 3], rs: ["rs", 1], py: ["py", 4],
  sh: ["$", 2], bash: ["$", 2], ps1: ["PS", 4], psm1: ["PS", 4], bat: ["$", 7], cmd: ["$", 7],
  c: ["C", 4], h: ["h", 5], cu: ["cu", 2], cuh: ["cu", 5], cpp: ["C", 4], cc: ["C", 4], hpp: ["h", 5],
  md: ["M", 4], html: ["<>", 1], htm: ["<>", 1], svg: ["<>", 3], xml: ["<>", 1], css: ["#", 4],
  tex: ["T", 2], sty: ["T", 2], cls: ["T", 2], bib: ["T", 6], toml: ["≡", 7], ini: ["≡", 7],
  yml: ["≡", 5], yaml: ["≡", 5], tsv: ["⊞", 2], csv: ["⊞", 2], lock: ["≡", 8],
  png: ["▣", 5], jpg: ["▣", 5], jpeg: ["▣", 5], gif: ["▣", 5], pdf: ["▤", 1],
};

const NAMED = { ".gitignore": ["◆", 1], ".gitattributes": ["◆", 1], ".gitmodules": ["◆", 1], ".clang-format": ["≡", 7] };

export function iconOf(name, known = false) {
  const ext = name.includes(".") ? name.split(".").pop().toLowerCase() : "";
  const [mark, color] = known ? ["K", 2] : (NAMED[name] ?? ICONS[ext] ?? [null, 0]);
  const icon = element("span", { className: mark ? "icon" : "icon page", textContent: mark ?? "" });
  icon.setAttribute("aria-hidden", "true");
  if (mark) {
    icon.style.color = `var(--t${color})`;
  }
  return icon;
}

// The panes: which show, which are open.

function keep() {
  localStorage.setItem(KEPT, JSON.stringify(state.panes));
}

function paneOf(name) {
  return document.querySelector(`.pane[data-pane="${name}"]`);
}

function drawPane(name) {
  const pane = paneOf(name);
  const { shown, open } = state.panes[name];
  pane.hidden = !shown;
  pane.classList.toggle("expanded", open);
  pane.querySelector(".pane-head").setAttribute("aria-expanded", String(open));
}

function setPane(name, change) {
  Object.assign(state.panes[name], change);
  drawPane(name);
  keep();
  if (name === "outline" || name === "timeline") {
    state.hooks?.panesChanged?.();
  }
}

// Shows a pane and opens it.
export function showPane(name) {
  setPane(name, { shown: true, open: true });
}

export function paneOpen(name) {
  return state.panes[name]?.shown && state.panes[name]?.open;
}

// The … menu, and each pane head's own menu: the panes to show, then reading the tree again and
// closing its folders.
function paneItems() {
  return [
    ...PANES.map(([name, label]) => ({
      label: label ?? document.querySelector('.pane[data-pane="folder"] .pane-head').textContent,
      checked: state.panes[name].shown,
      disabled: state.panes[name].shown && PANES.filter(([one]) => state.panes[one].shown).length === 1,
      run: () => setPane(name, { shown: !state.panes[name].shown }),
    })),
    "-",
    { label: "Refresh Explorer", run: () => state.hooks.refresh() },
    { label: "Collapse Folders in Explorer", run: () => state.hooks.collapse() },
  ];
}

// Open Editors: a row a tab, its dot where the tab holds changes not saved, its × to close it.

export function drawOpenEditors(tabs) {
  const body = document.getElementById("open-editors");
  const focused = body.contains(document.activeElement) ? document.activeElement.dataset.key : null;
  body.replaceChildren(
    ...tabs.map((tab) => {
      const row = element("button", { className: "open-row", type: "button", title: tab.title });
      row.dataset.key = tab.path;
      row.dataset.depth = "0";
      if (tab.active) {
        row.setAttribute("aria-current", "true");
      }
      const close = element("span", { className: `close${tab.dirty ? " dirty" : ""}`, title: "Close" });
      close.setAttribute("aria-hidden", "true");
      const folder = tab.file.includes("/") ? tab.file.slice(0, tab.file.lastIndexOf("/")) : "";
      row.append(close, iconOf(tab.file.split("/").pop(), tab.known), element("span", { className: "name", textContent: tab.name }));
      row.append(element("span", { className: "where", textContent: folder }));
      row.addEventListener("click", (event) => (event.target === close ? state.hooks.close(tab.path) : state.hooks.show(tab.path)));
      row.addEventListener("auxclick", (event) => event.button === 1 && state.hooks.close(tab.path));
      return row;
    })
  );
  if (focused) {
    [...body.children].find((row) => row.dataset.key === focused)?.focus();
  }
}

// Outline: what the open file declares, a row a symbol, the one the cursor is in marked.

const KIND_MARKS = { function: ["ƒ", 5], method: ["ƒ", 5], class: ["◇", 3], module: ["{}", 4], constant: ["▪", 4], field: ["▪", 6], heading: ["#", 4], label: ["›", 6] };

export function drawOutline(session) {
  const body = document.getElementById("outline");
  state.outline.session = session;
  if (!paneOpen("outline")) {
    return;
  }
  const symbols = session ? symbolsOf(session.language?.id, session.doc) : [];
  state.outline.symbols = symbols;
  state.outline.rows = symbols.map((symbol) => {
    const [mark, color] = KIND_MARKS[symbol.kind] ?? ["▪", 7];
    const row = element("button", { className: "sym", type: "button", title: `${symbol.name}, line ${session.base + symbol.line + 1}` });
    row.dataset.key = `${symbol.line}:${symbol.name}`;
    row.dataset.depth = String(symbol.depth);
    row.append(...guides(symbol.depth));
    const icon = element("span", { className: "icon", textContent: mark });
    icon.style.color = `var(--t${color})`;
    row.append(icon, element("span", { className: "name", textContent: symbol.name }));
    row.addEventListener("click", () => state.hooks.goTo(symbol.line));
    return row;
  });
  body.replaceChildren(...state.outline.rows);
  lightOutline(state.hooks.cursorLine?.() ?? 0);
}

// Marks the symbol the cursor's line falls in: the last one that starts at or above it.
export function lightOutline(line) {
  let lit = -1;
  state.outline.symbols.forEach((symbol, index) => {
    if (symbol.line <= line) {
      lit = index;
    }
  });
  state.outline.rows.forEach((row, index) => (index === lit ? row.setAttribute("aria-current", "true") : row.removeAttribute("aria-current")));
  state.outline.rows[lit]?.scrollIntoView({ block: "nearest" });
}

// Timeline: the commits that touched the open file, the newest first, each with its date.

const day = (when) => {
  const date = new Date(when * 1000);
  const pad = (n) => String(n).padStart(2, "0");
  return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}`;
};

const time = (when) => {
  const date = new Date(when * 1000);
  return `${String(date.getHours()).padStart(2, "0")}:${String(date.getMinutes()).padStart(2, "0")}`;
};

export async function drawTimeline(path, again = false) {
  const body = document.getElementById("timeline");
  if (!paneOpen("timeline")) {
    state.timeline.path = null;
    return;
  }
  if (path === state.timeline.path && !again) {
    return;
  }
  state.timeline.path = path;
  const commits = path ? await invoke("file_commits", { path }).catch(() => []) : [];
  if (state.timeline.path !== path) {
    return;
  }
  state.timeline.commits = commits;
  body.replaceChildren(
    ...commits.map((commit) => {
      const row = element("button", { className: "commit", type: "button", title: `${commit.id.slice(0, 8)}  ${day(commit.when)} ${time(commit.when)}\n${commit.subject}` });
      row.dataset.key = commit.id;
      row.dataset.depth = "0";
      const dot = element("span", { className: "icon commit-dot" });
      dot.setAttribute("aria-hidden", "true");
      row.append(dot, element("span", { className: "name", textContent: commit.subject }), element("span", { className: "where", textContent: day(commit.when) }));
      row.addEventListener("click", () => state.hooks.openCommit(path, commit));
      return row;
    })
  );
}

function timelineItems(event) {
  const row = event.target.closest(".commit");
  const commit = state.timeline.commits.find((one) => one.id === row?.dataset.key);
  if (!commit) {
    return null;
  }
  return [
    { label: "Open", run: () => state.hooks.openCommit(state.timeline.path, commit) },
    "-",
    { label: "Copy Commit ID", run: () => copyText(commit.id) },
    { label: "Copy Commit Message", run: () => copyText(commit.subject) },
  ];
}

// The lines down from each folder above a row to the row, one a level.
export function guides(depth) {
  return Array.from({ length: depth }, () => element("i", { className: "guide" }));
}

export function startExplorer(hooks) {
  state.hooks = hooks;
  let kept = {};
  try {
    kept = JSON.parse(localStorage.getItem(KEPT) ?? "{}") ?? {};
  } catch {
    kept = {};
  }
  for (const [name] of PANES) {
    state.panes[name] = { shown: kept[name]?.shown ?? (name !== "search" && name !== "usages"), open: kept[name]?.open ?? (name !== "timeline" && name !== "outline") };
    drawPane(name);
    paneOf(name)
      .querySelector(".pane-head")
      .addEventListener("click", () => setPane(name, { open: !state.panes[name].open }));
  }
  const more = document.getElementById("explorer-more");
  more.addEventListener("click", () => {
    const box = more.getBoundingClientRect();
    showMenu(box.left, box.bottom + 2, paneItems(), { anchor: more });
  });
  for (const head of document.querySelectorAll(".pane-head")) {
    menuOn(head, paneItems);
  }
  menuOn(document.getElementById("open-editors"), (event) => hooks.tabMenu(event.target.closest(".open-row")?.dataset.key));
  menuOn(document.getElementById("timeline"), timelineItems);
}

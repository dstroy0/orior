// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The explorer beside the editor: its panes, one over the next, each opened and closed by its head,
// in groups the tool strip's icons choose between, one group shown at a time. Explorer holds Search,
// which finds text in the tree's files and shows from Find in Files; Usages, which lists where a
// symbol is used and shows from Find Usages; Open Editors, which lists the tabs; and the tree's own
// pane of its files. Structure holds the Outline of what the open file declares. Commit holds
// Changes, the files that differ from the last commit, the Timeline of commits that touched the open
// file, and its Local History, the file as each save left it. Problems lists the open files' diagnostics, and Git the commits of the branch. The
// explorer's … menu shows or hides each pane of the group, reads the tree again, and closes every
// folder. Which group shows, and which panes show and are open, is kept between visits.
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
  ["todo", "TODO"],
  ["open", "Open Editors"],
  ["folder", null],
  ["outline", "Outline"],
  ["timeline", "Timeline"],
  ["local", "Local History"],
  ["changes", "Changes"],
  ["problems", "Problems"],
  ["git", "Git"],
];

// The panes each icon of the tool strip shows, one group at a time.
const GROUPS = {
  explorer: ["search", "usages", "todo", "open", "folder"],
  structure: ["outline"],
  commit: ["changes", "timeline", "local"],
  problems: ["problems"],
  git: ["git"],
};

const GROUP_KEPT = "orior.panes.group";

const groupOf = (name) => Object.keys(GROUPS).find((group) => GROUPS[group].includes(name));

const state = {
  panes: {},
  hooks: null,
  outline: { session: null, symbols: [], rows: [] },
  timeline: { path: null, commits: [] },
  local: { path: null, snapshots: [] },
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
  pane.hidden = !shown || groupOf(name) !== state.group;
  pane.classList.toggle("expanded", open);
  pane.querySelector(".pane-head").setAttribute("aria-expanded", String(open));
}

function setPane(name, change) {
  Object.assign(state.panes[name], change);
  drawPane(name);
  keep();
  if (["outline", "timeline", "local", "changes", "problems", "git", "todo"].includes(name)) {
    state.hooks?.panesChanged?.();
  }
}

// The group of panes the explorer shows.
export function shownGroup() {
  return state.group;
}

const GROUP_TITLES = { explorer: "Explorer", structure: "Structure", commit: "Commit", problems: "Problems", git: "Git" };

// The explorer's title, and its group on it, which shows the tree's filter only with the files.
function drawGroupTitle() {
  const title = document.querySelector("#explorer .explorer-head h2");
  if (title) {
    title.textContent = GROUP_TITLES[state.group];
  }
  document.getElementById("explorer").dataset.group = state.group;
}

// Shows the panes of `group` in the explorer in place of the ones it showed.
export function showGroup(group) {
  state.group = GROUPS[group] ? group : "explorer";
  localStorage.setItem(GROUP_KEPT, state.group);
  drawGroupTitle();
  // A group of one pane is that pane, shown and open.
  if (GROUPS[state.group].length === 1) {
    Object.assign(state.panes[GROUPS[state.group][0]], { shown: true, open: true });
    keep();
  }
  PANES.forEach(([name]) => drawPane(name));
  state.hooks?.panesChanged?.();
  window.dispatchEvent(new Event("panes-changed"));
}

// Shows a pane and opens it, with the group it is in.
export function showPane(name) {
  if (groupOf(name) !== state.group) {
    showGroup(groupOf(name));
  }
  setPane(name, { shown: true, open: true });
}

// The panes that are not in a group of their own on the tool strip, for its … menu.
export function morePanes() {
  return GROUPS.explorer.concat("timeline").map((name) => ({ name, label: PANES.find(([one]) => one === name)[1] ?? "Files" }));
}

export function paneOpen(name) {
  return state.panes[name]?.shown && state.panes[name]?.open;
}

// The … menu, and each pane head's own menu: the panes to show, then reading the tree again and
// closing its folders.
function paneItems() {
  const group = GROUPS[state.group];
  return [
    ...PANES.filter(([name]) => group.includes(name)).map(([name, label]) => ({
      label: label ?? document.querySelector('.pane[data-pane="folder"] .pane-head').textContent,
      checked: state.panes[name].shown,
      disabled: state.panes[name].shown && group.filter((one) => state.panes[one].shown).length === 1,
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

// Local History: the open file as each save left it, the newest first, kept apart from git. A row
// shows its difference from the text as it stands; its menu takes the file back to it.

const clock = (ms) => {
  const date = new Date(ms);
  return [date.getHours(), date.getMinutes(), date.getSeconds()].map((n) => String(n).padStart(2, "0")).join(":");
};

const bytes = (size) => (size < 1024 ? `${size} B` : `${(size / 1024).toFixed(1)} KB`);

export async function drawLocalHistory(path, again = false) {
  const body = document.getElementById("local-history");
  if (!paneOpen("local")) {
    state.local.path = null;
    return;
  }
  if (path === state.local.path && !again) {
    return;
  }
  state.local.path = path;
  const snapshots = path ? await invoke("history_list", { path }).catch(() => []) : [];
  if (state.local.path !== path) {
    return;
  }
  state.local.snapshots = snapshots;
  body.replaceChildren(
    ...snapshots.map((snapshot) => {
      const row = element("button", { className: "commit", type: "button", title: `${day(snapshot.at / 1000)} ${clock(snapshot.at)}, ${bytes(snapshot.size)}` });
      row.dataset.key = String(snapshot.at);
      row.dataset.depth = "0";
      const dot = element("span", { className: "icon commit-dot" });
      dot.setAttribute("aria-hidden", "true");
      row.append(dot, element("span", { className: "name", textContent: `${day(snapshot.at / 1000)} ${clock(snapshot.at)}` }), element("span", { className: "where", textContent: bytes(snapshot.size) }));
      row.addEventListener("click", () => state.hooks.showSnapshot(path, snapshot.at));
      return row;
    })
  );
}

function localItems(event) {
  const row = event.target.closest(".commit");
  const at = Number(row?.dataset.key);
  if (!row || !state.local.path) {
    return null;
  }
  const path = state.local.path;
  return [
    { label: "Show Difference", run: () => state.hooks.showSnapshot(path, at) },
    { label: "Revert to This", run: () => state.hooks.revertSnapshot(path, at) },
  ];
}

// TODO: every TODO, FIXME, XXX and HACK in the tree's files, as whole words, a row a file and under
// it a row a line, read again each time the pane opens or the explorer is refreshed.

const TODO_WORDS = "TODO|FIXME|XXX|HACK";

export async function drawTodo() {
  const said = document.getElementById("todo-said");
  const body = document.getElementById("todo-hits");
  if (!paneOpen("todo") || state.group !== "explorer") {
    return;
  }
  said.textContent = "Reading the tree…";
  const hits = await invoke("tree_search", { query: TODO_WORDS, how: { case: true, word: true, regex: true } }).catch((error) => {
    said.textContent = String(error);
    return null;
  });
  if (!hits) {
    return;
  }
  const files = new Map();
  for (const hit of hits) {
    if (!files.has(hit.path)) {
      files.set(hit.path, []);
    }
    files.get(hit.path).push(hit);
  }
  said.textContent = hits.length ? `${hits.length} item${hits.length === 1 ? "" : "s"} in ${files.size} file${files.size === 1 ? "" : "s"}` : "No TODO, FIXME, XXX or HACK in the tree.";
  const rows = [];
  for (const [path, found] of files) {
    const cut = path.lastIndexOf("/");
    const head = element("div", { className: "node problem-file" }, iconOf(path.slice(cut + 1)), element("span", { className: "name", textContent: path.slice(cut + 1) }), element("span", { className: "where", textContent: path.slice(0, Math.max(0, cut)) }), element("span", { className: "count", textContent: String(found.length) }));
    rows.push(head);
    for (const hit of found) {
      const text = hit.text.trim();
      const word = text.match(new RegExp(`\\b(${TODO_WORDS})\\b`));
      const row = element("button", { className: "search-hit", type: "button", title: `${path}:${hit.line}` });
      row.dataset.key = `todo:${path}:${hit.line}`;
      row.dataset.depth = "1";
      const line = element("span", { className: "line" });
      if (word) {
        line.append(text.slice(0, word.index), element("mark", { textContent: word[0] }), text.slice(word.index + word[0].length, word.index + 160));
      } else {
        line.textContent = text.slice(0, 160);
      }
      row.append(element("i", { className: "guide" }), line);
      row.addEventListener("click", () => state.hooks.openAt(path, hit.line - 1, hit.col - 1));
      rows.push(row);
    }
  }
  body.replaceChildren(...rows);
}

// Problems: each open file's diagnostics, a row a file and under it a row a diagnostic, the worst first.

export function drawProblems(files) {
  const body = document.getElementById("problems");
  if (!paneOpen("problems") || state.group !== "problems") {
    return;
  }
  const rows = [];
  for (const { path, items } of files) {
    if (!items.length) {
      continue;
    }
    const cut = path.lastIndexOf("/");
    const head = element("div", { className: "node problem-file" }, iconOf(path.slice(cut + 1)), element("span", { className: "name", textContent: path.slice(cut + 1) }), element("span", { className: "where", textContent: path.slice(0, Math.max(0, cut)) }), element("span", { className: "count", textContent: String(items.length) }));
    rows.push(head);
    for (const item of [...items].sort((a, b) => a.severity - b.severity || a.from.line - b.from.line)) {
      const row = element("button", { className: `problem s${item.severity}`, type: "button", title: `${path}:${item.from.line + 1}:${item.from.col + 1}\n${item.message}` });
      row.dataset.key = `problem:${path}:${item.from.line}:${item.from.col}`;
      row.dataset.depth = "1";
      row.append(element("i", { className: "guide" }), element("span", { className: "problem-mark" }), element("span", { className: "name", textContent: item.message.split("\n")[0] }), element("span", { className: "where", textContent: `${item.from.line + 1}:${item.from.col + 1}` }));
      row.addEventListener("click", () => state.hooks.openAt(path, item.from.line, item.from.col));
      rows.push(row);
    }
  }
  body.replaceChildren(...(rows.length ? rows : [element("p", { className: "pane-empty", textContent: "No problems in the open files. A language server or Run, Validate finds them." })]));
}

// Git: the branch the tree is on, and its commits, the newest first.

export async function drawGit(branch) {
  const body = document.getElementById("git-log");
  if (!paneOpen("git") || state.group !== "git") {
    return;
  }
  const commits = await invoke("tree_commits").catch(() => []);
  const head = element("div", { className: "node git-branch" }, element("span", { className: "name", textContent: branch ?? "no branch" }), element("span", { className: "where", textContent: `${commits.length} commit${commits.length === 1 ? "" : "s"} shown` }));
  body.replaceChildren(
    head,
    ...commits.map((commit) => {
      const row = element("button", { className: "commit", type: "button", title: `${commit.id.slice(0, 8)}  ${day(commit.when)} ${time(commit.when)}\n${commit.subject}` });
      row.dataset.key = commit.id;
      row.dataset.depth = "0";
      const dot = element("span", { className: "icon commit-dot" });
      dot.setAttribute("aria-hidden", "true");
      row.append(dot, element("span", { className: "name", textContent: commit.subject }), element("span", { className: "where", textContent: day(commit.when) }));
      row.addEventListener("click", () => copyText(commit.id));
      return row;
    }),
  );
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
  state.group = GROUPS[localStorage.getItem(GROUP_KEPT)] ? localStorage.getItem(GROUP_KEPT) : "explorer";
  drawGroupTitle();
  for (const [name] of PANES) {
    state.panes[name] = { shown: kept[name]?.shown ?? !["search", "usages", "todo"].includes(name), open: kept[name]?.open ?? name !== "timeline" };
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
  menuOn(document.getElementById("local-history"), localItems);
}

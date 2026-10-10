// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The explorer beside the editor: its panes, one over the next, each opened and closed by its head,
// in groups the tool strip's icons choose between, one group shown at a time. Explorer holds Search,
// which finds text in the tree's files and shows from Find in Files; Usages, which lists where a
// symbol is used and shows from Find Usages; Call Hierarchy, which shows what calls a function and
// what it calls; Open Editors, which lists the tabs; and the tree's own
// pane of its files. Structure holds the Outline of what the open file declares and its Undo History.
// Commit holds Changes, the files that differ from the last commit, the Timeline of commits that
// touched the open file, its Local History, the file as each save left it, and Review, the review
// comments left on the tree's changes. Problems lists every file's diagnostics, and Git every
// branch's commits as a graph. The explorer's … menu shows or hides each pane of the group, reads the
// tree again, and closes every folder. Which group shows, and which panes show and are open, is kept
// between visits.
//
// A pane's head is a row of the explorer's list one level above its rows, and the list's keys open
// and close it as they do a folder.

import { invoke } from "./bridge.js";
import { escapeHtml } from "./editor/view.js";
import { copyText, menuOn, showMenu } from "./menu.js";
import { comments, onReview, removeComment, resolveComment } from "./review.js";
import { structureOf } from "./outline.js";

const KEPT = "orior.panes";

const PANES = [
  ["search", "Search"],
  ["usages", "Usages"],
  ["calls", "Call Hierarchy"],
  ["todo", "TODO"],
  ["open", "Open Editors"],
  ["folder", null],
  ["outline", "Outline"],
  ["undo", "Undo History"],
  ["timeline", "Timeline"],
  ["local", "Local History"],
  ["review", "Review"],
  ["changes", "Changes"],
  ["problems", "Problems"],
  ["tests", "Tests"],
  ["git", "Git"],
];

// The panes each icon of the tool strip shows, one group at a time.
const GROUPS = {
  explorer: ["search", "usages", "calls", "todo", "open", "folder"],
  structure: ["outline", "undo"],
  commit: ["changes", "timeline", "local", "review"],
  problems: ["problems"],
  tests: ["tests"],
  git: ["git"],
};

const GROUP_KEPT = "orior.panes.group";

const groupOf = (name) => Object.keys(GROUPS).find((group) => GROUPS[group].includes(name));

const state = {
  panes: {},
  hooks: null,
  outline: { session: null, symbols: [], rows: [], closed: new Set(), pinned: null },
  timeline: { path: null, commits: [] },
  local: { path: null, snapshots: [] },
  undo: { session: null, wait: 0 },
  // The graph: what the field searches for, the commits shown, the ones whose files are open, the
  // files of each commit read so far, and what the graph was last drawn from.
  git: { query: "", commits: [], open: new Set(), files: new Map(), drawn: "", branch: null, chosen: null },
  // The files whose rows in Problems were opened or closed by a press.
  problems: { opened: new Set(), closed: new Set() },
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
  if (!pane) {
    return;
  }
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

const GROUP_TITLES = { explorer: "Explorer", structure: "Structure", commit: "Commit", problems: "Problems", tests: "Tests", git: "Git" };

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

// Outline: what the open file declares, a row a symbol, the one the cursor is in marked; and the
// regions its comments mark, each a row that holds the symbols inside it, opened and closed by its
// arrow. Pinned, the pane shows the structure of the file pinned while another is open, a press on a
// symbol going to that file, until it is unpinned or the file closes.

const KIND_MARKS = { function: ["ƒ", 5], method: ["ƒ", 5], class: ["◇", 3], module: ["{}", 4], constant: ["▪", 4], field: ["▪", 6], heading: ["#", 4], label: ["›", 6], region: ["▤", 2] };

export function drawOutline(session) {
  const body = document.getElementById("outline");
  const pinned = state.outline.pinned;
  if (pinned && !state.hooks?.sessionOf?.(pinned.path)) {
    state.outline.pinned = null;
  }
  const shown = state.outline.pinned ? state.hooks.sessionOf(state.outline.pinned.path) : session;
  state.outline.session = shown;
  const head = document.querySelector('[data-pane="outline"] .pane-head');
  if (head) {
    head.textContent = state.outline.pinned ? `Outline of ${state.outline.pinned.path.split("/").pop()}` : "Outline";
  }
  if (!paneOpen("outline")) {
    return;
  }
  const symbols = shown ? structureOf(shown.language?.id, shown.doc) : [];
  const closed = state.outline.closed;
  const hidden = (symbol) => symbols.some((region) => region.kind === "region" && closed.has(`${region.line}:${region.name}`) && region.line < symbol.line && symbol.line <= region.end);
  state.outline.symbols = symbols.filter((symbol) => !hidden(symbol));
  state.outline.rows = state.outline.symbols.map((symbol) => {
    const [mark, color] = KIND_MARKS[symbol.kind] ?? ["▪", 7];
    const key = `${symbol.line}:${symbol.name}`;
    const region = symbol.kind === "region";
    const row = element("button", { className: region ? "sym dir sym-region" : "sym", type: "button", title: `${symbol.name}, line ${shown.base + symbol.line + 1}` });
    row.dataset.key = key;
    row.dataset.depth = String(symbol.depth);
    row.append(...guides(symbol.depth));
    if (region) {
      row.setAttribute("aria-expanded", String(!closed.has(key)));
      row.append(element("span", { className: "twisty" }));
    }
    const icon = element("span", { className: "icon", textContent: mark });
    icon.style.color = `var(--t${color})`;
    row.append(icon, element("span", { className: "name", textContent: symbol.name }));
    row.addEventListener("click", (event) => {
      if (region && (!event.isTrusted || event.target.closest(".twisty"))) {
        if (closed.has(key)) {
          closed.delete(key);
        } else {
          closed.add(key);
        }
        drawOutline(state.outline.session);
        document.querySelector(`#outline [data-key="${CSS.escape(key)}"]`)?.focus();
        return;
      }
      if (state.outline.pinned) {
        state.hooks.openAt(state.outline.pinned.path, shown.base + symbol.line, 0);
      } else {
        state.hooks.goTo(symbol.line);
      }
    });
    return row;
  });
  body.replaceChildren(...state.outline.rows);
  lightOutline(state.hooks.cursorLine?.() ?? 0);
}

// Pins the Outline to the file `path` shows, or, given none, unpins it to follow the file shown.
export function pinOutline(path) {
  state.outline.pinned = path ? { path } : null;
  drawOutline(state.hooks.sessionOf?.(state.hooks.activePath?.()) ?? null);
}

export function outlinePinned() {
  return state.outline.pinned?.path ?? null;
}

// The Outline's menu: pinning it to the file shown, or unpinning it.
function outlineItems() {
  const active = state.hooks.activePath?.();
  const pinned = state.outline.pinned;
  return [
    pinned ? { label: `Unpin ${pinned.path.split("/").pop()}`, run: () => pinOutline(null) } : { label: active ? `Pin ${active.split("/").pop()}` : "Pin the File Shown", disabled: !active, run: () => pinOutline(active) },
  ];
}

// Marks the symbol the cursor's line falls in: the last one that starts at or above it, where the
// Outline shows the file the cursor is in.
export function lightOutline(line) {
  if (state.outline.pinned && state.outline.pinned.path !== state.hooks.activePath?.()) {
    state.outline.rows.forEach((row) => row.removeAttribute("aria-current"));
    return;
  }
  let lit = -1;
  state.outline.symbols.forEach((symbol, index) => {
    if (symbol.line <= line) {
      lit = index;
    }
  });
  state.outline.rows.forEach((row, index) => (index === lit ? row.setAttribute("aria-current", "true") : row.removeAttribute("aria-current")));
  state.outline.rows[lit]?.scrollIntoView({ block: "nearest" });
}

// Timeline: the commits that touched the open file, the newest first, each with its date. A commit's
// menu opens the file as it left it, or sets it beside the file as it stands or another commit's.

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
  const path = state.timeline.path;
  const others = state.timeline.commits.filter((one) => one !== commit).slice(0, 40);
  return [
    { label: "Open", run: () => state.hooks.openCommit(path, commit) },
    "-",
    { label: "Compare with Current", run: () => state.hooks.compareCommit(path, commit) },
    { label: "Compare with Revision", disabled: !others.length, items: others.map((one) => ({ label: `${one.id.slice(0, 7)}  ${one.subject}`, run: () => state.hooks.compareCommit(path, commit, one) })) },
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

// Undo History: every state the open file's text has been in since it opened, the first at the top,
// each by when it was made and the line its step changed. A branch an edit after an undo left stands
// a level in, under the state it was made from. The state the text is in is marked, and a press on a
// row takes the text to its state. Drawn at most once a frame.
export function drawUndo(session) {
  state.undo.session = session;
  if (state.undo.wait || !paneOpen("undo")) {
    return;
  }
  state.undo.wait = window.requestAnimationFrame(() => {
    state.undo.wait = 0;
    const shown = state.undo.session;
    const rows = shown ? shown.doc.states() : [];
    const body = document.getElementById("undo-history");
    const focused = body.contains(document.activeElement) ? document.activeElement.dataset.key : null;
    body.replaceChildren(
      ...rows.map(({ id, depth, step, here }) => {
        const line = step?.edits[0]?.[0]?.from.line;
        const row = element("button", { className: "commit", type: "button", title: step ? `${clock(step.made)}${step.kind ? `, ${step.kind}` : ""}` : "" });
        row.dataset.key = String(id);
        row.dataset.depth = String(depth);
        if (here) {
          row.setAttribute("aria-current", "true");
        }
        const dot = element("span", { className: "icon commit-dot" });
        dot.setAttribute("aria-hidden", "true");
        row.append(...guides(depth), dot, element("span", { className: "name", textContent: step ? clock(step.made) : "As opened" }));
        if (line !== undefined) {
          row.append(element("span", { className: "where", textContent: `line ${shown.base + line + 1}` }));
        }
        row.addEventListener("click", () => state.hooks.goToState(id));
        return row;
      })
    );
    if (focused !== null) {
      [...body.children].find((row) => row.dataset.key === focused)?.focus();
    }
    body.querySelector('[aria-current="true"]')?.scrollIntoView({ block: "nearest" });
  });
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

// Problems: each file's diagnostics, a row a file and under it a row a diagnostic, the worst first,
// the open files first; and, while the tree's check goes on, how far it has gone. A file's row opens
// and closes its diagnostics, which show for a file open in the editor until its row closes them.

export function drawProblems(files, checking) {
  const body = document.getElementById("problems");
  if (!paneOpen("problems") || state.group !== "problems") {
    return;
  }
  const rows = [];
  if (checking && checking.done < checking.total) {
    rows.push(element("p", { className: "pane-empty problems-checking", textContent: `Checking the tree: ${checking.done} of ${checking.total} files.` }));
  }
  for (const file of files) {
    if (!file.items.length) {
      continue;
    }
    const open = state.problems.opened.has(file.path) || (file.open && !state.problems.closed.has(file.path));
    rows.push(problemFileRow(file, open));
    if (open) {
      rows.push(...problemRows(file));
    }
  }
  body.replaceChildren(...(rows.length ? rows : [element("p", { className: "pane-empty", textContent: "No problems in the tree. A language server or Run, Validate finds them." })]));
}

// A diagnostic's first line as its row shows it: where the server writes Markdown, its code spans
// set as code.
function firstLine(item) {
  const line = item.message.split("\n")[0];
  if (!item.markdown) {
    return [line];
  }
  return line.split(/(`[^`]*`)/).filter(Boolean).map((piece) => (/^`[^`]*`$/.test(piece) ? element("code", { textContent: piece.slice(1, -1) }) : piece));
}

function problemFileRow(file, open) {
  const { path, items } = file;
  const cut = path.lastIndexOf("/");
  const row = element("button", { className: "node dir problem-file", type: "button" }, element("span", { className: "twisty" }), iconOf(path.slice(cut + 1)), element("span", { className: "name", textContent: path.slice(cut + 1) }), element("span", { className: "where", textContent: path.slice(0, Math.max(0, cut)) }), element("span", { className: "count", textContent: String(items.length) }));
  row.dataset.key = `problem-file:${path}`;
  row.dataset.depth = "0";
  row.setAttribute("aria-expanded", String(open));
  row.addEventListener("click", () => {
    const opening = row.getAttribute("aria-expanded") !== "true";
    state.problems.opened[opening ? "add" : "delete"](path);
    state.problems.closed[opening ? "delete" : "add"](path);
    row.setAttribute("aria-expanded", String(opening));
    if (opening) {
      row.after(...problemRows(file));
    } else {
      while (row.nextElementSibling?.classList.contains("problem")) {
        row.nextElementSibling.remove();
      }
    }
  });
  return row;
}

function problemRows({ path, items }) {
  return [...items]
    .sort((a, b) => a.severity - b.severity || a.from.line - b.from.line)
    .map((item) => {
      const row = element("button", { className: `problem s${item.severity}`, type: "button", title: `${path}:${item.from.line + 1}:${item.from.col + 1}\n${item.message}` });
      row.dataset.key = `problem:${path}:${item.from.line}:${item.from.col}`;
      row.dataset.depth = "1";
      row.append(element("i", { className: "guide" }), element("span", { className: "problem-mark" }), element("span", { className: "name" }, ...firstLine(item)), element("span", { className: "where", textContent: `${item.from.line + 1}:${item.from.col + 1}` }));
      row.addEventListener("click", () => state.hooks.openAt(path, item.from.line, item.from.col));
      return row;
    });
}

// Git: the branch the tree is on, and every branch's commits as a graph, the newest first, each with
// the branches and tags at it, its subject, its author and its date. A press on a commit opens the
// files it changed under it, and a press on one of those sets the file as the commit left it beside
// the file as the commit before it did. The field over the graph searches the whole history, by
// subject, author, id or branch, and shows what it finds without the graph. Where the tree holds more
// than one repository, a choice over the field says whose commits the graph shows, the repository
// open's until another is chosen.

// A column of the graph is this wide, and its row this high.
const LANE = 12;
const ROW = 22;

const laneX = (lane) => LANE / 2 + lane * LANE;

const tone = (line) => `var(--t${[4, 2, 5, 3, 6, 1][line % 6]})`;

// A row's strokes and its commit's dot, as an SVG the width of the columns it crosses.
function lanesOf(commit) {
  const across = Math.max(commit.lane, ...commit.lines.flatMap(([from, to]) => [from, to])) + 1;
  const mid = ROW / 2;
  let paths = "";
  for (const [from, to, half, line] of commit.lines) {
    const [top, bottom] = half ? [mid, ROW] : [0, mid];
    const [x1, x2] = [laneX(from), laneX(to)];
    const bend = (bottom - top) / 2;
    const d = x1 === x2 ? `M${x1} ${top}V${bottom}` : `M${x1} ${top}C${x1} ${top + bend} ${x2} ${bottom - bend} ${x2} ${bottom}`;
    paths += `<path d="${d}" style="stroke:${tone(line)}"/>`;
  }
  const merge = commit.parents.length > 1;
  const dot = `<circle cx="${laneX(commit.lane)}" cy="${mid}" r="${merge ? 3 : 3.5}" style="${merge ? `fill:var(--surface);stroke:${tone(commit.color)}` : `fill:${tone(commit.color)};stroke:none`}"/>`;
  return `<svg class="git-lanes" width="${across * LANE}" height="${ROW}" aria-hidden="true">${paths}${dot}</svg>`;
}

function commitRow(commit) {
  const open = state.git.open.has(commit.id);
  const refs = commit.refs
    .map((ref) => {
      const head = ref.startsWith("HEAD -> ");
      const name = head ? ref.slice(8) : ref;
      return `<span class="git-ref${head || ref === "HEAD" ? " git-head" : ""}${name.startsWith("tag: ") ? " git-tag" : ""}">${escapeHtml(name.replace(/^tag: /, ""))}</span>`;
    })
    .join("");
  const title = `${commit.id.slice(0, 8)}  ${commit.author}  ${day(commit.when)} ${time(commit.when)}${commit.refs.length ? `\n${commit.refs.join(", ")}` : ""}\n${commit.subject}`;
  return `<button class="commit git-commit" type="button" data-key="${commit.id}" data-depth="0" aria-expanded="${open}" title="${escapeHtml(title)}">${lanesOf(commit)}${refs}<span class="name">${escapeHtml(commit.subject)}</span><span class="git-author">${escapeHtml(commit.author)}</span><span class="where">${day(commit.when)}</span></button>`;
}

// The rows of the files a commit changed, under its row.
function touchedRows(commit) {
  const files = state.git.files.get(commit.id) ?? [];
  if (!files.length) {
    return [element("p", { className: "pane-empty git-none", textContent: "No file under the tree changed." })];
  }
  return files.map((file) => {
    const cut = file.path.lastIndexOf("/");
    const name = file.path.slice(cut + 1);
    const row = element("button", { className: "node git-file", type: "button", title: file.was ? `${file.was} → ${file.path}` : file.path });
    row.dataset.key = `${commit.id}:${file.path}`;
    row.dataset.depth = "1";
    row.dataset.change = file.state;
    row.append(...guides(1), iconOf(name), element("span", { className: "name", textContent: name }), element("span", { className: "where", textContent: file.path.slice(0, Math.max(0, cut)) }), element("span", { className: "change", textContent: file.state }));
    row.addEventListener("click", () => state.hooks.openTouched(commit, file));
    return row;
  });
}

async function toggleCommit(row) {
  const commit = state.git.commits.find((one) => one.id === row.dataset.key);
  if (!commit) {
    return;
  }
  if (state.git.open.has(commit.id)) {
    state.git.open.delete(commit.id);
    row.setAttribute("aria-expanded", "false");
    while (row.nextElementSibling && !row.nextElementSibling.classList.contains("git-commit")) {
      row.nextElementSibling.remove();
    }
    return;
  }
  state.git.open.add(commit.id);
  row.setAttribute("aria-expanded", "true");
  if (!state.git.files.has(commit.id)) {
    state.git.files.set(commit.id, await invoke("git_touched", { id: commit.id, repo: gitRepo() }).catch(() => []));
  }
  if (state.git.open.has(commit.id) && row.isConnected) {
    row.after(...touchedRows(commit));
  }
}

// Forgets the graph's search, its choice of repository and the commits whose files were read, for a
// tree opened in place of this one.
export function forgetGraph() {
  Object.assign(state.git, { query: "", commits: [], open: new Set(), files: new Map(), drawn: "", chosen: null });
  document.getElementById("git-query").value = "";
}

// The repository the graph shows: the one chosen over it, or the repository open where none is
// chosen or the one chosen is gone.
function gitRepo() {
  const repos = state.hooks?.repos() ?? [];
  return repos.some((repo) => repo.path === state.git.chosen) ? state.git.chosen : (state.hooks?.repo() ?? "");
}

// The choice of repository over the graph, there while the tree holds more than one.
function drawRepoChoice(repo) {
  const choice = document.getElementById("git-repo");
  const repos = state.hooks?.repos() ?? [];
  choice.hidden = repos.length < 2;
  const key = repos.map((one) => `${one.path}:${one.branch}`).join();
  if (choice.dataset.key !== key) {
    choice.dataset.key = key;
    choice.replaceChildren(...repos.map((one) => element("option", { value: one.path, textContent: `${state.hooks.repoName(one.path)}${one.branch ? `: ${one.branch}` : ""}` })));
  }
  choice.value = repo;
}

export async function drawGit(branch) {
  const body = document.getElementById("git-log");
  state.git.branch = branch;
  if (!paneOpen("git") || state.group !== "git") {
    return;
  }
  const query = state.git.query;
  const repo = gitRepo();
  drawRepoChoice(repo);
  const label = (state.hooks?.repos() ?? []).find((one) => one.path === repo)?.branch ?? branch;
  const commits = await invoke("git_graph", { query: query || null, repo }).catch(() => []);
  if (query !== state.git.query || repo !== gitRepo()) {
    return;
  }
  // A graph that is drawn as it stands is left as it is, the focus and the scroll with it.
  const drawn = `${repo}\n${label}\n${query}\n${commits.map((one) => `${one.id}${one.refs.join()}`).join()}`;
  if (drawn === state.git.drawn && body.childElementCount) {
    return;
  }
  state.git.drawn = drawn;
  state.git.commits = commits;
  const count = `${commits.length} commit${commits.length === 1 ? "" : "s"} ${query ? "found" : "shown"}`;
  const focused = body.contains(document.activeElement) ? document.activeElement.dataset.key : null;
  body.innerHTML = `<div class="node git-branch"><span class="name">${escapeHtml(label ?? "no branch")}</span><span class="where">${count}</span></div>${commits.map(commitRow).join("")}`;
  for (const id of state.git.open) {
    const row = body.querySelector(`.git-commit[data-key="${id}"]`);
    const commit = commits.find((one) => one.id === id);
    if (row && state.git.files.has(id)) {
      row.after(...touchedRows(commit));
    }
  }
  if (focused) {
    body.querySelector(`[data-key="${CSS.escape(focused)}"]`)?.focus();
  }
}

function gitItems(event) {
  const row = event.target.closest(".git-commit, .git-file");
  const commit = state.git.commits.find((one) => one.id === (row?.closest(".git-commit") ? row.dataset.key : row?.dataset.key.split(":")[0]));
  if (!commit) {
    return null;
  }
  if (row.classList.contains("git-file")) {
    const file = state.git.files.get(commit.id)?.find((one) => `${commit.id}:${one.path}` === row.dataset.key);
    return [
      { label: "Show Changes", run: () => state.hooks.openTouched(commit, file) },
      { label: "Open as It Was", disabled: file?.state === "D", run: () => state.hooks.openCommit(file.path, commit) },
      "-",
      { label: "Copy Path", run: () => copyText(file.path) },
    ];
  }
  return [
    { label: state.git.open.has(commit.id) ? "Close Files" : "Open Files", run: () => toggleCommit(row) },
    "-",
    { label: "Cherry-Pick", run: () => import("./menubar.js").then((menus) => menus.cherryPick(commit.id, gitRepo())) },
    "-",
    { label: "Copy Commit ID", run: () => copyText(commit.id) },
    { label: "Copy Commit Message", run: () => copyText(commit.subject) },
  ];
}

// Review: every review comment of the tree, a row for each file and under it a row a comment, the
// open ones before the resolved ones. A press opens the file at the comment's line, and a comment's
// menu shows the file's changes, resolves or opens it again, or deletes it.
export function drawReview() {
  const body = document.getElementById("review");
  if (!paneOpen("review")) {
    return;
  }
  const files = new Map();
  for (const one of comments().sort((a, b) => Number(a.done) - Number(b.done) || a.line - b.line)) {
    files.set(one.path, [...(files.get(one.path) ?? []), one]);
  }
  const rows = [];
  for (const [path, items] of [...files].sort(([a], [b]) => a.localeCompare(b))) {
    const cut = path.lastIndexOf("/");
    const open = items.filter((one) => !one.done).length;
    rows.push(element("div", { className: "node problem-file" }, iconOf(path.slice(cut + 1)), element("span", { className: "name", textContent: path.slice(cut + 1) }), element("span", { className: "where", textContent: path.slice(0, Math.max(0, cut)) }), element("span", { className: "count", textContent: String(open) })));
    for (const one of items) {
      const row = element("button", { className: `problem review-row${one.done ? " done" : ""}`, type: "button", title: `${path}:${one.line + 1}\n${one.body}` });
      row.dataset.key = `review:${one.id}`;
      row.dataset.depth = "1";
      row.dataset.comment = one.id;
      row.append(element("i", { className: "guide" }), element("span", { className: "review-mark" }), element("span", { className: "name", textContent: one.body.split("\n")[0] }), element("span", { className: "where", textContent: String(one.line + 1) }));
      row.addEventListener("click", () => state.hooks.openComment(one));
      rows.push(row);
    }
  }
  body.replaceChildren(...(rows.length ? rows : [element("p", { className: "pane-empty", textContent: "No review comments. A press on a line's number in a file's changes leaves one." })]));
}

function reviewItems(event) {
  const id = event.target.closest(".review-row")?.dataset.comment;
  const one = comments().find((comment) => comment.id === id);
  if (!one) {
    return null;
  }
  return [
    { label: "Open", run: () => state.hooks.openComment(one) },
    { label: "Show Changes", run: () => state.hooks.showChanges(one.path) },
    "-",
    { label: one.done ? "Reopen" : "Resolve", run: () => resolveComment(one.id, !one.done) },
    { label: "Delete", run: () => removeComment(one.id) },
    "-",
    { label: "Copy Comment", run: () => copyText(one.body) },
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
  state.group = GROUPS[localStorage.getItem(GROUP_KEPT)] ? localStorage.getItem(GROUP_KEPT) : "explorer";
  drawGroupTitle();
  for (const [name] of PANES) {
    state.panes[name] = { shown: kept[name]?.shown ?? !["search", "usages", "calls", "todo"].includes(name), open: kept[name]?.open ?? name !== "timeline" };
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
  menuOn(document.getElementById("outline"), outlineItems);
  menuOn(document.getElementById("local-history"), localItems);
  menuOn(document.getElementById("review"), reviewItems);
  onReview(drawReview);
  const graph = document.getElementById("git-log");
  menuOn(graph, gitItems);
  graph.addEventListener("click", (event) => {
    const row = event.target.closest(".git-commit");
    if (row) {
      toggleCommit(row);
    }
  });
  document.getElementById("git-repo").addEventListener("change", (event) => {
    state.git.chosen = event.target.value;
    drawGit(state.git.branch);
  });
  const query = document.getElementById("git-query");
  let wait = 0;
  query.addEventListener("input", () => {
    window.clearTimeout(wait);
    wait = window.setTimeout(() => {
      state.git.query = query.value.trim();
      drawGit(state.git.branch);
    }, 250);
  });
  query.addEventListener("keydown", (event) => {
    if (event.key === "ArrowDown") {
      event.preventDefault();
      graph.querySelector("button")?.focus();
    }
  });
}

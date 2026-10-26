// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The explorer beside the editor: its panes, one over the next, each opened and closed by its head,
// in groups the tool strip's icons choose between, one group shown at a time. Search, a group of its
// own, finds text in the tree's files and shows from Find in Files. Explorer holds Usages, which lists where a
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

import { onAuthors } from "./authors.js";
import { invoke } from "./bridge.js";
import { copyText, menuOn, showMenu } from "./menu.js";
import { comments, onReview, removeComment, resolveComment } from "./review.js";
import { say } from "./statusbar.js";
import { iconOf } from "./explorer/icons.js";
import { outlineItems, pinOutline } from "./explorer/outline.js";
import { localItems, timelineItems } from "./explorer/history.js";
import { drawGit, gitItems, gitRepo, toggleCommit } from "./explorer/git.js";
import { drawReview, reviewItems } from "./explorer/review.js";
export { drawGit, forgetGraph } from "./explorer/git.js";
export { drawLocalHistory, drawTimeline, drawTodo, drawUndo } from "./explorer/history.js";
export { folderIcon, iconOf } from "./explorer/icons.js";
export { drawOutline, lightOutline, outlinePinned } from "./explorer/outline.js";
export { drawProblems } from "./explorer/problems.js";
export { drawReview } from "./explorer/review.js";

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
  ["runs", "Runs"],
];

// The panes each icon of the tool strip shows, one group at a time.
const GROUPS = {
  explorer: ["usages", "calls", "todo", "open", "folder"],
  search: ["search"],
  structure: ["outline", "undo"],
  commit: ["changes", "timeline", "local", "review"],
  problems: ["problems"],
  tests: ["tests"],
  git: ["git"],
  runs: ["runs"],
};

const GROUP_KEPT = "orior.panes.group";

const groupOf = (name) => Object.keys(GROUPS).find((group) => GROUPS[group].includes(name));

export const state = {
  panes: {},
  hooks: null,
  outline: { session: null, shownIn: null, symbols: [], rows: new Map(), lit: -1, above: null, below: null, closed: new Set(), pinned: null },
  timeline: { path: null, commits: [] },
  local: { path: null, snapshots: [] },
  undo: { session: null, wait: 0 },
  // The graph: what the field searches for, the commits shown, the ones whose files are open, the
  // files of each commit read so far, and what the graph was last drawn from.
  git: { query: "", commits: [], open: new Set(), files: new Map(), drawn: "", branch: null, chosen: null },
  // The files whose rows in Problems were opened or closed by a press.
  problems: { opened: new Set(), closed: new Set() },
};

export function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
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

const GROUP_TITLES = { explorer: "Explorer", search: "Search", structure: "Structure", commit: "Commit", problems: "Problems", tests: "Tests", git: "Git", runs: "Runs" };

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

// Every pane's state, and each pane put back as `kept` holds it, for the tests that change them.
export const panesNow = () => JSON.parse(JSON.stringify(state.panes));

export function putPanes(kept) {
  for (const [name, pane] of Object.entries(kept)) {
    if (state.panes[name]) {
      setPane(name, pane);
    }
  }
}

export function paneOpen(name) {
  return state.panes[name]?.shown && state.panes[name]?.open;
}

// The … menu, and each pane head's own menu: the panes to show, then reading the tree again and
// closing its folders.
// The explorer's menus as commands, each by the name commands.json gives it, run from the menus or
// from the command line with the words it gives: a pane's name, a file of the tree, a commit by the
// start of its id, a save of Local History by its number, a review comment by its id.

// The commit `id` begins: one the panes hold, or else one of `path`'s, or one of the repository's.
async function commitNamed(id, path = null) {
  const held = [...state.timeline.commits, ...state.git.commits].find((one) => one.id.startsWith(id));
  if (held) {
    return held;
  }
  const found = path ? await invoke("file_commits", { path }).catch(() => []) : await invoke("git_graph", { query: null, repo: gitRepo(), authors: false }).catch(() => []);
  return found.find((one) => one.id.startsWith(id)) ?? null;
}

// Runs `act` with the commit `id` begins, or says there is none.
async function withCommit(id, path, act) {
  const commit = await commitNamed(id, path);
  if (!commit) {
    say(`No commit ${path ? `of ${path} ` : ""}begins ${id}.`, { failed: true });
    return;
  }
  return act(commit);
}

const commentNamed = (id) => comments().find((one) => one.id === id) ?? null;

export const explorerCommands = {
  "pane-show": ([name, on]) => state.panes[name] && setPane(name, { shown: on === "on" ? true : on === "off" ? false : !state.panes[name].shown }),
  "explorer-refresh": () => state.hooks.refresh(),
  "explorer-collapse": () => state.hooks.collapse(),
  // A file not open opens first: the Outline is pinned to an open file's symbols.
  "outline-pin": async ([path = "none"]) => {
    if (path !== "none" && !state.hooks.sessionOf(path)) {
      await state.hooks.open(path);
    }
    pinOutline(path === "none" ? null : path);
  },
  "commit-open": ([path, id]) => withCommit(id, path, (commit) => state.hooks.openCommit(path, commit)),
  "commit-compare": ([path, id, other]) => withCommit(id, path, async (commit) => state.hooks.compareCommit(path, commit, other ? await commitNamed(other, path) : undefined)),
  "commit-copy-id": ([id]) => withCommit(id, null, (commit) => copyText(commit.id)),
  "commit-copy-message": ([id, path]) => withCommit(id, path, (commit) => copyText(commit.subject)),
  "commit-files": async ([id]) => {
    showPane("git");
    await drawGit(state.git.branch);
    const row = [...document.querySelectorAll(".git-commit")].find((one) => one.dataset.key.startsWith(id));
    return row ? toggleCommit(row) : say(`The graph shows no commit beginning ${id}.`, { failed: true });
  },
  "commit-file-changes": ([id, path]) =>
    withCommit(id, null, (commit) => state.hooks.openTouched(commit, state.git.files.get(commit.id)?.find((one) => one.path === path) ?? { path, state: "M" })),
  "history-difference": ([path, at]) => state.hooks.showSnapshot(path, Number(at)),
  "history-revert": ([path, at]) => state.hooks.revertSnapshot(path, Number(at)),
  "comment-open": ([id]) => commentNamed(id) && state.hooks.openComment(commentNamed(id)),
  "comment-changes": ([id]) => commentNamed(id) && state.hooks.showChanges(commentNamed(id).path),
  "comment-resolve": ([id, as]) => commentNamed(id) && resolveComment(id, as === "open" ? false : as === "done" ? true : !commentNamed(id).done),
  "comment-delete": ([id]) => removeComment(id),
  "comment-copy": ([id]) => commentNamed(id) && copyText(commentNamed(id).body),
};

// An item that runs `command` with `args`, as the command line runs it, and says both.
export const act = (command, args, item) => ({ ...item, command, args, run: () => explorerCommands[command](args) });

function paneItems() {
  const group = GROUPS[state.group];
  return [
    ...PANES.filter(([name]) => group.includes(name)).map(([name, label]) =>
      act("pane-show", [name, state.panes[name].shown ? "off" : "on"], {
        label: label ?? document.querySelector('.pane[data-pane="folder"] .pane-head').textContent,
        checked: state.panes[name].shown,
        disabled: state.panes[name].shown && group.filter((one) => state.panes[one].shown).length === 1,
      }),
    ),
    "-",
    act("explorer-refresh", [], { label: "Refresh Explorer" }),
    act("explorer-collapse", [], { label: "Collapse Folders in Explorer" }),
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

// The lines down from each folder above a row to the row, one a level.
export function guides(depth) {
  return Array.from({ length: depth }, () => element("i", { className: "guide" }));
}

export function startExplorer(hooks) {
  state.hooks = hooks;
  onAuthors(() => {
    state.git.drawn = "";
    drawGit(state.git.branch);
  });
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

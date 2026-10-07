// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The menu bar: File, Edit, Selection, View, Go and Run, then the catalog's groups of jobs, then
// Terminal and Help. The run group is the Run menu and the render and view groups share the Render
// menu. A group with no job in the tree has no menu. A job chosen from a menu shows in the run view
// with its form, and starts only from there or from Run, Start.
//
// Edit and Run are the two views as well as menus, and the bar marks the one shown; opening either
// shows its view. A press of the menu bar opens its menu, and while one is open the pointer moving
// to another title opens that one instead. Alt held marks each title's letter, Alt and the letter
// opens the menu, and Alt alone gives the bar the keys: Left and Right step along it, Down, Enter or
// Space opens, and Escape hands the keys back. In an open menu Left and Right step to the menus on
// either side. Past a number of titles set by the window's width, and past what fits, the titles go
// into the last, a menu of menus.

import { invoke } from "./bridge.js";
import { editing } from "./edit.js";
import { clipText, closeMenu, menuOpen, showMenu } from "./menu.js";
import { chosenJob, chosenLive, listedJobs, showJob, startChosen, stopChosen, subject } from "./run.js";
import { scheme, toggleScheme } from "./scheme.js";
import { clearTerminal, killTerminal, newTerminal, toggleTerminal } from "./terminal.js";
import { onView, shownView, showView } from "./views.js";

// The most titles the bar shows at a window width: [narrowest width in pixels, titles]. The rest go
// into the last title's menu, and so do more where even these do not fit.
const TITLES_AT = [
  [1600, Infinity],
  [1440, 13],
  [1280, 11],
  [1120, 9],
  [0, 7],
];

const state = { bar: null, open: -1, titles: [], held: false, before: null, openFolder: () => {} };

// The editor's commands, each acting on the editor with the keys handed to it first.
function editorItem(label, keys, act, needs = true) {
  const { editor } = editing();
  return {
    label,
    keys,
    disabled: needs && !editor,
    run: () => {
      showView("edit");
      editor?.focus();
      act(editor);
    },
  };
}

function jobItem(job) {
  return {
    label: job.title,
    run: () => {
      showView("run");
      showJob(job.id);
    },
  };
}

const jobsIn = (...groups) => listedJobs().filter((job) => groups.includes(job.group));

function jobItems(...groups) {
  return groups.flatMap((group, at) => {
    const items = jobsIn(group).map(jobItem);
    return at > 0 && items.length ? ["-", ...items] : items;
  });
}

// The stage group, a menu of what each stage works on.
function stageItems() {
  const jobs = jobsIn("stage");
  return [...new Set(jobs.map(subject))].map((name) => ({ label: name, items: jobs.filter((job) => subject(job) === name).map(jobItem) }));
}

const MENUS = [
  {
    title: "File",
    items: () => {
      const edits = editing();
      return [
        { label: "Open Folder…", run: () => state.openFolder() },
        "-",
        { label: "Save", keys: "Ctrl+S", disabled: !edits.activeChanged, run: edits.save },
        { label: "Save All", disabled: !edits.changed, run: edits.saveAll },
        "-",
        { label: "Close Editor", keys: "Ctrl+W", disabled: !edits.active, run: edits.close },
        { label: "Close All Editors", disabled: !edits.open, run: edits.closeAll },
        "-",
        { label: "Exit", run: () => invoke("app_exit") },
      ];
    },
  },
  {
    title: "Edit",
    view: "edit",
    items: () => [
      editorItem("Undo", "Ctrl+Z", (e) => e.undo(true)),
      editorItem("Redo", "Ctrl+Y", (e) => e.undo(false)),
      "-",
      editorItem("Cut", "Ctrl+X", () => document.execCommand("cut")),
      editorItem("Copy", "Ctrl+C", () => document.execCommand("copy")),
      editorItem("Paste", "Ctrl+V", async (e) => {
        const text = await clipText();
        e.focus();
        e.paste({ preventDefault() {}, clipboardData: { getData: () => text } });
      }),
      "-",
      editorItem("Find", "Ctrl+F", (e) => e.find.open(false)),
      editorItem("Replace", "Ctrl+H", (e) => e.find.open(true)),
      "-",
      editorItem("Toggle Line Comment", "Ctrl+/", (e) => e.toggleComment()),
    ],
  },
  {
    title: "Selection",
    items: () => [
      editorItem("Select All", "Ctrl+A", (e) => e.selectAll()),
      editorItem("Select Line", "Ctrl+L", (e) => e.selectLine()),
      "-",
      editorItem("Copy Line Up", "Shift+Alt+Up", (e) => e.copyLines(-1)),
      editorItem("Copy Line Down", "Shift+Alt+Down", (e) => e.copyLines(1)),
      editorItem("Move Line Up", "Alt+Up", (e) => e.moveLines(-1)),
      editorItem("Move Line Down", "Alt+Down", (e) => e.moveLines(1)),
      "-",
      editorItem("Add Cursor Above", "Ctrl+Alt+Up", (e) => e.addCursor(-1)),
      editorItem("Add Cursor Below", "Ctrl+Alt+Down", (e) => e.addCursor(1)),
      editorItem("Add Next Occurrence", "Ctrl+D", (e) => e.addMatch(false)),
      editorItem("Select All Occurrences", "Ctrl+Shift+L", (e) => e.addMatch(true)),
    ],
  },
  {
    title: "View",
    items: () => [
      { label: "Edit", keys: "Ctrl+Shift+E", run: () => showView("edit") },
      { label: "Run", keys: "Ctrl+Shift+D", run: () => showView("run") },
      "-",
      { label: "Terminal", keys: "Ctrl+`", run: () => toggleTerminal() },
      "-",
      { label: scheme() === "dark" ? "Light" : "Dark", run: toggleScheme },
      "-",
      editorItem("Fold All", "Ctrl+K Ctrl+0", (e) => e.foldAll(true)),
      editorItem("Unfold All", "Ctrl+K Ctrl+J", (e) => e.foldAll(false)),
    ],
  },
  {
    title: "Go",
    items: () => [
      { label: "Go to File…", keys: "Ctrl+P", run: goToFile },
      editorItem("Go to Line…", "Ctrl+G", (e) => e.goto.open()),
      editorItem("Go to Bracket", "Ctrl+Shift+\\", (e) => e.jumpBracket()),
      "-",
      editorItem("Next Match", "F3", (e) => e.find.step(1)),
      editorItem("Previous Match", "Shift+F3", (e) => e.find.step(-1)),
    ],
  },
  {
    title: "Run",
    view: "run",
    items: () => [
      { label: "Start", keys: "F5", disabled: !chosenJob(), run: startChosen },
      { label: "Stop", keys: "Shift+F5", disabled: !chosenLive(), run: stopChosen },
      ...(jobsIn("run").length ? ["-", ...jobItems("run")] : []),
    ],
  },
  { title: "Build", groups: ["build"], items: () => jobItems("build") },
  { title: "Protocol", groups: ["protocol"], items: () => jobItems("protocol") },
  { title: "Ingest", groups: ["ingest"], items: () => jobItems("ingest") },
  { title: "Render", groups: ["render", "view"], items: () => jobItems("render", "view") },
  { title: "Sim", groups: ["sim"], items: () => jobItems("sim") },
  { title: "Pipeline", groups: ["pipeline"], items: () => jobItems("pipeline") },
  { title: "Stage", groups: ["stage"], items: stageItems },
  { title: "Test", groups: ["test"], items: () => jobItems("test") },
  {
    title: "Terminal",
    items: () => [
      { label: "New Terminal", keys: "Ctrl+Shift+`", run: newTerminal },
      { label: "Toggle Terminal", keys: "Ctrl+`", run: () => toggleTerminal() },
      "-",
      { label: "Clear", run: clearTerminal },
      { label: "Kill Terminal", run: killTerminal },
    ],
  },
  {
    title: "Help",
    items: () => [{ label: "Keyboard Shortcuts", run: showShortcuts }, "-", { label: "About", run: showAbout }],
  },
];

function goToFile() {
  showView("edit");
  editing().find();
}

// The keys the menus list, by menu, in a sheet over the app.
function showShortcuts() {
  const body = document.createElement("div");
  body.className = "sheet-keys";
  for (const menu of MENUS) {
    const rows = menu.items().filter((item) => item !== "-" && item.keys);
    if (!rows.length) {
      continue;
    }
    const block = document.createElement("section");
    block.append(Object.assign(document.createElement("h3"), { textContent: menu.title }));
    for (const item of rows) {
      const row = document.createElement("div");
      row.append(Object.assign(document.createElement("span"), { textContent: item.label }), Object.assign(document.createElement("kbd"), { textContent: item.keys }));
      block.append(row);
    }
    body.append(block);
  }
  sheet(body);
}

async function showAbout() {
  const body = document.createElement("div");
  body.className = "sheet-about";
  const version = await invoke("app_version").catch(() => "");
  body.append(
    Object.assign(document.createElement("span"), { className: "mark", textContent: "Σ", ariaHidden: "true" }),
    Object.assign(document.createElement("h2"), { textContent: "orior" }),
    Object.assign(document.createElement("p"), { textContent: version }),
    Object.assign(document.createElement("p"), { className: "tree", textContent: document.getElementById("tree-path").textContent }),
  );
  sheet(body);
}

// A sheet over the app, which Escape, a press outside it or its × closes.
function sheet(body) {
  const dialog = document.createElement("dialog");
  dialog.className = "sheet";
  const close = Object.assign(document.createElement("button"), { type: "button", className: "sheet-close", textContent: "×", ariaLabel: "Close" });
  close.addEventListener("click", () => dialog.close());
  dialog.append(close, body);
  dialog.addEventListener("click", (event) => event.target === dialog && dialog.close());
  dialog.addEventListener("close", () => dialog.remove());
  document.body.append(dialog);
  dialog.showModal();
}

// The menus the bar has a title for: every one of the app's, and each group's that has a job.
function shownMenus() {
  return MENUS.filter((menu) => !menu.groups || menu.groups.some((group) => jobsIn(group).length));
}

// Gives each title the first of its letters no title before it has.
function lettersFor(menus) {
  const taken = new Set();
  return menus.map((menu) => {
    const at = [...menu.title.toLowerCase()].findIndex((letter) => /[a-z]/.test(letter) && !taken.has(letter));
    if (at >= 0) {
      taken.add(menu.title[at].toLowerCase());
    }
    return at;
  });
}

function titleOf(menu, letter) {
  const button = Object.assign(document.createElement("button"), { type: "button", className: "bar-title" });
  button.setAttribute("role", "menuitem");
  button.setAttribute("aria-haspopup", "menu");
  if (letter >= 0) {
    button.append(menu.title.slice(0, letter), Object.assign(document.createElement("u"), { textContent: menu.title[letter] }), menu.title.slice(letter + 1));
    button.dataset.letter = menu.title[letter].toLowerCase();
  } else {
    button.textContent = menu.title;
  }
  if (menu.view) {
    button.dataset.view = menu.view;
  }
  return button;
}

// Draws the bar's titles, the ones that fit and the rest in the last.
export function drawMenubar() {
  const bar = state.bar;
  closeMenu(false);
  state.open = -1;
  const menus = shownMenus();
  const letters = lettersFor(menus);
  bar.replaceChildren(...menus.map((menu, at) => titleOf(menu, letters[at])));
  state.titles = menus.map((menu, at) => ({ menu, button: bar.children[at] }));
  const more = { title: "…", items: () => [] };
  const moreButton = titleOf(more, -1);
  moreButton.ariaLabel = "More";
  bar.append(moreButton);
  moreButton.hidden = true;
  const most = TITLES_AT.find(([width]) => window.innerWidth >= width)[1];
  if (state.titles.length > most || bar.scrollWidth > bar.clientWidth) {
    moreButton.hidden = false;
    const folded = [];
    while ((state.titles.length > most || bar.scrollWidth > bar.clientWidth) && state.titles.length > 1) {
      const last = state.titles.pop();
      last.button.remove();
      folded.unshift(last.menu);
    }
    more.items = () => folded.map((menu) => ({ label: menu.title, items: menu.items() }));
    state.titles.push({ menu: more, button: moreButton });
  } else {
    moreButton.remove();
  }
  state.titles.forEach(({ button }, at) => {
    button.addEventListener("click", () => (state.open === at && menuOpen() ? closeBar() : openAt(at, true)));
    button.addEventListener("pointerenter", () => menuOpen() && state.open >= 0 && state.open !== at && openAt(at, false));
  });
  markView(shownView());
}

function markView(name) {
  state.bar?.querySelectorAll("[data-view]").forEach((button) => button.setAttribute("aria-current", String(button.dataset.view === name)));
}

function closeBar() {
  closeMenu();
}

// Opens the menu under the at'th title. The keys go into it where it was opened from the keyboard or
// by a press, and stay where they are where the pointer only passed onto its title.
function openAt(at, keyed) {
  const count = state.titles.length;
  const index = ((at % count) + count) % count;
  const { menu, button } = state.titles[index];
  if (menu.view) {
    showView(menu.view);
  }
  const box = button.getBoundingClientRect();
  state.bar.querySelectorAll(".bar-title").forEach((one) => one.removeAttribute("aria-expanded"));
  button.setAttribute("aria-expanded", "true");
  const items = menu.items();
  showMenu(box.left, box.bottom + 2, items.length ? items : [{ label: menu.title, disabled: true, run: () => {} }], {
    side: (by) => openAt(index + by, true),
    keep: state.bar,
    onClose: () => {
      button.removeAttribute("aria-expanded");
      state.open = -1;
    },
    focusFirst: keyed,
  });
  state.open = index;
  leaveBar(false);
}

// The bar holding the keys after Alt alone, its letters marked.
function holdBar() {
  state.before = document.activeElement;
  state.bar.classList.add("held");
  state.titles[0]?.button.focus();
}

function leaveBar(refocus) {
  if (!state.bar.classList.contains("held")) {
    return;
  }
  state.bar.classList.remove("held");
  if (refocus && state.before?.isConnected) {
    state.before.focus();
  }
}

// The keys the menus list that nothing nearer the focus takes first.
const SHORTCUTS = {
  "Ctrl+Shift+KeyE": () => showView("edit"),
  "Ctrl+Shift+KeyD": () => showView("run"),
  "Ctrl+KeyP": goToFile,
  "Ctrl+KeyW": () => editing().close(),
  "Ctrl+Shift+Backquote": newTerminal,
  F5: () => chosenJob() && startChosen(),
  "Shift+F5": stopChosen,
};

function shortcutName(event) {
  return `${event.ctrlKey ? "Ctrl+" : ""}${event.shiftKey ? "Shift+" : ""}${event.altKey ? "Alt+" : ""}${event.code}`;
}

export function startMenubar({ openFolder }) {
  state.bar = document.getElementById("menubar");
  state.openFolder = openFolder;
  onView(markView);
  state.bar.addEventListener("keydown", (event) => {
    const at = state.titles.findIndex(({ button }) => button === document.activeElement);
    if (at < 0) {
      return;
    }
    const step = { ArrowRight: 1, ArrowLeft: -1 }[event.key];
    if (step) {
      state.titles[(at + step + state.titles.length) % state.titles.length].button.focus();
    } else if (event.key === "ArrowDown" || event.key === "Enter" || event.key === " ") {
      openAt(at, true);
    } else if (event.key === "Escape") {
      leaveBar(true);
    } else {
      return;
    }
    event.preventDefault();
  });
  state.bar.addEventListener("focusout", (event) => !state.bar.contains(event.relatedTarget) && state.bar.classList.remove("held"));
  window.addEventListener("keydown", (event) => {
    if (event.key === "Alt") {
      state.held = true;
      document.body.classList.add("alt-held");
      return;
    }
    state.held = false;
    if (event.altKey && !event.ctrlKey && !event.metaKey && /^Key[A-Z]$/.test(event.code)) {
      const letter = event.code.slice(3).toLowerCase();
      const at = state.titles.findIndex(({ button }) => button.dataset.letter === letter);
      if (at >= 0) {
        event.preventDefault();
        document.body.classList.remove("alt-held");
        openAt(at, true);
      }
      return;
    }
    const act = SHORTCUTS[shortcutName(event)];
    if (act && !event.defaultPrevented) {
      event.preventDefault();
      act();
    }
  });
  window.addEventListener("keyup", (event) => {
    if (event.key !== "Alt") {
      return;
    }
    document.body.classList.remove("alt-held");
    if (state.held && !menuOpen()) {
      event.preventDefault();
      if (state.bar.classList.contains("held")) {
        leaveBar(true);
      } else {
        holdBar();
      }
    }
    state.held = false;
  });
  window.addEventListener("blur", () => document.body.classList.remove("alt-held"));
  let wait = 0;
  window.addEventListener("resize", () => {
    window.clearTimeout(wait);
    wait = window.setTimeout(drawMenubar, 120);
  });
  drawMenubar();
}

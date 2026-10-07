// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The menu bar, as commands.json lists it: the list the command line reads too, so that each item
// here is `orior <menu> <command>` there. What each command does in the window is COMMANDS below,
// what it needs before it can act is NEEDS, and the keys an item lists are bound here as well. A
// key the editor or the terminal takes first stays theirs.
//
// A menu of jobs lists the catalog's groups it names, split by what each job works on where it says
// so, and a group with no job in the tree has no menu. A job chosen from a menu shows in the run view
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
import { editing, openAt as openFileAt } from "./edit.js";
import { clipText, closeMenu, menuOpen, showMenu } from "./menu.js";
import { chosenJob, chosenLive, listedJobs, showJob, startChosen, stopChosen, subject } from "./run.js";
import { scheme, setScheme, toggleScheme } from "./scheme.js";
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

const state = { bar: null, menus: [], open: -1, titles: [], held: false, before: null, openFolder: () => {} };

// The editor's command: the run view gives way to the edit view, and the editor takes the keys.
const inEditor = (act) => () => {
  const { editor } = editing();
  if (!editor) {
    return;
  }
  showView("edit");
  editor.focus();
  act(editor);
};

async function pasted(editor) {
  const text = await clipText();
  editor.focus();
  editor.paste({ preventDefault() {}, clipboardData: { getData: () => text } });
}

// The run view's search, holding `word`.
function searchJobs(word = "") {
  showView("run");
  const filter = document.getElementById("job-filter");
  filter.value = word;
  filter.dispatchEvent(new Event("input"));
  filter.focus();
}

// The file a path names, at the line after its colon, or the edit view's search where none is named.
function goToFile(args = []) {
  showView("edit");
  const [path, line] = (args[0] ?? "").split(/:(?=\d+$)/);
  if (path) {
    openFileAt(path, Math.max(0, Number(line || 1) - 1));
  } else {
    editing().find();
  }
}

// The bridge's key in Lstar.klq, or the file itself.
async function goToBridge(args = []) {
  const read = await invoke("bridge_read").catch(() => null);
  const klq = read?.klq;
  if (!klq) {
    return;
  }
  showView("edit");
  openFileAt(klq.path, klq.keys?.[args[0]]?.line ?? 0);
}

// What each command does in the window, given the words the command line passed it, if any.
const COMMANDS = {
  "open-folder": (args) => state.openFolder(args[0]),
  save: () => editing().save(),
  "save-all": () => editing().saveAll(),
  "close-editor": () => editing().close(),
  "close-all": () => editing().closeAll(),
  exit: () => invoke("app_exit"),
  undo: inEditor((e) => e.undo(true)),
  redo: inEditor((e) => e.undo(false)),
  cut: inEditor(() => document.execCommand("cut")),
  copy: inEditor(() => document.execCommand("copy")),
  paste: inEditor(pasted),
  find: inEditor((e) => e.find.open(false)),
  replace: inEditor((e) => e.find.open(true)),
  comment: inEditor((e) => e.toggleComment()),
  "select-all": inEditor((e) => e.selectAll()),
  "select-line": inEditor((e) => e.selectLine()),
  "copy-line-up": inEditor((e) => e.copyLines(-1)),
  "copy-line-down": inEditor((e) => e.copyLines(1)),
  "move-line-up": inEditor((e) => e.moveLines(-1)),
  "move-line-down": inEditor((e) => e.moveLines(1)),
  "cursor-above": inEditor((e) => e.addCursor(-1)),
  "cursor-below": inEditor((e) => e.addCursor(1)),
  "next-occurrence": inEditor((e) => e.addMatch(false)),
  "all-occurrences": inEditor((e) => e.addMatch(true)),
  "edit-view": () => showView("edit"),
  "run-view": () => showView("run"),
  "terminal-view": () => toggleTerminal(),
  scheme: (args) => (args[0] === "light" || args[0] === "dark" ? setScheme(args[0]) : toggleScheme()),
  "fold-all": inEditor((e) => e.foldAll(true)),
  "unfold-all": inEditor((e) => e.foldAll(false)),
  file: goToFile,
  line: (args) => inEditor((e) => (args[0] ? e.goTo(Math.max(0, Number(args[0]) - 1)) : e.goto.open()))(),
  bracket: inEditor((e) => e.jumpBracket()),
  bridge: goToBridge,
  "next-match": inEditor((e) => e.find.step(1)),
  "previous-match": inEditor((e) => e.find.step(-1)),
  start: () => chosenJob() && startChosen(),
  stop: stopChosen,
  list: (args) => searchJobs(args[0]),
  show: (args) => searchJobs(args[0]),
  new: newTerminal,
  toggle: () => toggleTerminal(),
  clear: clearTerminal,
  kill: killTerminal,
  keys: showShortcuts,
  about: showAbout,
};

// What a command needs before it can act, by the name commands.json gives the need.
const NEEDS = {
  editor: () => Boolean(editing().editor),
  "changed-active": () => editing().activeChanged,
  changed: () => editing().changed,
  tab: () => Boolean(editing().active),
  tabs: () => editing().open,
  job: () => Boolean(chosenJob()),
  live: () => chosenLive(),
};

// Runs a command of a menu, as its item does, with the words the command line gave it.
export function runCommand(command, args = []) {
  COMMANDS[command]?.(args);
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

const jobsIn = (groups) => listedJobs().filter((job) => groups.includes(job.group));

// A menu's items: its commands, then the jobs of its groups, split by what each works on where the
// menu says so.
function itemsOf(menu) {
  const items = (menu.items ?? []).map((item) =>
    item === "-"
      ? "-"
      : {
          label: item.labels?.[scheme()] ?? item.label,
          keys: item.keys,
          disabled: Boolean(item.needs && !NEEDS[item.needs]?.()),
          run: () => runCommand(item.command),
        },
  );
  const groups = menu.groups ?? [];
  let jobs;
  if (menu.split) {
    const all = jobsIn(groups);
    jobs = [...new Set(all.map(subject))].map((name) => ({ label: name, items: all.filter((job) => subject(job) === name).map(jobItem) }));
  } else {
    jobs = groups.flatMap((group, at) => {
      const own = jobsIn([group]).map(jobItem);
      return at > 0 && own.length ? ["-", ...own] : own;
    });
  }
  return items.length && jobs.length ? [...items, "-", ...jobs] : [...items, ...jobs];
}

// The keys every menu lists, by menu, in a sheet over the app.
function showShortcuts() {
  const body = document.createElement("div");
  body.className = "sheet-keys";
  for (const menu of state.menus) {
    const rows = (menu.items ?? []).filter((item) => item !== "-" && item.keys);
    if (!rows.length) {
      continue;
    }
    const block = document.createElement("section");
    block.append(Object.assign(document.createElement("h3"), { textContent: menu.title }));
    for (const item of rows) {
      const row = document.createElement("div");
      row.append(Object.assign(document.createElement("span"), { textContent: item.labels?.[scheme()] ?? item.label }), Object.assign(document.createElement("kbd"), { textContent: item.keys }));
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

// The menus the bar has a title for: each of commands, and each of jobs that has a job in the tree.
function shownMenus() {
  return state.menus.filter((menu) => menu.items?.length || jobsIn(menu.groups ?? []).length);
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
  if (!bar) {
    return;
  }
  closeMenu(false);
  state.open = -1;
  const menus = shownMenus();
  const letters = lettersFor(menus);
  bar.replaceChildren(...menus.map((menu, at) => titleOf(menu, letters[at])));
  state.titles = menus.map((menu, at) => ({ items: () => itemsOf(menu), menu, button: bar.children[at] }));
  const more = { title: "…" };
  const moreButton = titleOf(more, -1);
  moreButton.ariaLabel = "More";
  bar.append(moreButton);
  const most = TITLES_AT.find(([width]) => window.innerWidth >= width)[1];
  if (state.titles.length > most || bar.scrollWidth > bar.clientWidth) {
    const folded = [];
    while ((state.titles.length > most || bar.scrollWidth > bar.clientWidth) && state.titles.length > 1) {
      const last = state.titles.pop();
      last.button.remove();
      folded.unshift(last.menu);
    }
    state.titles.push({ items: () => folded.map((menu) => ({ label: menu.title, items: itemsOf(menu) })), menu: more, button: moreButton });
  } else {
    moreButton.remove();
  }
  state.titles.forEach(({ button }, at) => {
    button.addEventListener("click", () => (state.open === at && menuOpen() ? closeMenu() : openAt(at, true)));
    button.addEventListener("pointerenter", () => menuOpen() && state.open >= 0 && state.open !== at && openAt(at, false));
  });
  markView(shownView());
}

function markView(name) {
  state.bar?.querySelectorAll("[data-view]").forEach((button) => button.setAttribute("aria-current", String(button.dataset.view === name)));
}

// Opens the menu under the at'th title. The keys go into it where it was opened from the keyboard or
// by a press, and stay where they are where the pointer only passed onto its title.
function openAt(at, keyed) {
  const count = state.titles.length;
  const index = ((at % count) + count) % count;
  const { menu, button, items: itemsFor } = state.titles[index];
  if (menu.view) {
    showView(menu.view);
  }
  const box = button.getBoundingClientRect();
  state.bar.querySelectorAll(".bar-title").forEach((one) => one.removeAttribute("aria-expanded"));
  button.setAttribute("aria-expanded", "true");
  const items = itemsFor();
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

// The key names commands.json writes, as the page names the key: a letter or digit by its place on
// the keyboard, and the rest by name.
const KEY_CODES = { "`": "Backquote", "\\": "Backslash", "/": "Slash", Up: "ArrowUp", Down: "ArrowDown", Left: "ArrowLeft", Right: "ArrowRight" };

// Whether an event is the keys an item lists. Two presses, as Ctrl+K Ctrl+0, are the editor's.
function pressed(keys, event) {
  if (!keys || keys.includes(" ")) {
    return false;
  }
  const parts = keys.split("+");
  const key = parts.pop() || "+";
  if (parts.includes("Ctrl") !== event.ctrlKey || parts.includes("Shift") !== event.shiftKey || parts.includes("Alt") !== event.altKey) {
    return false;
  }
  if (/^[A-Z]$/.test(key)) {
    return event.code === `Key${key}`;
  }
  if (/^\d$/.test(key)) {
    return event.code === `Digit${key}`;
  }
  return KEY_CODES[key] ? event.code === KEY_CODES[key] : event.key === key;
}

// Runs the command whose keys an event is, where nothing nearer the focus took the keys first. The
// keys of a command for the editor are the editor's own, and a text field or the run's output keeps
// them for itself. A key bound here never reaches the web view, even where its command cannot act.
function onShortcut(event) {
  if (event.defaultPrevented || menuOpen()) {
    return;
  }
  for (const menu of state.menus) {
    for (const item of menu.items ?? []) {
      if (item !== "-" && item.needs !== "editor" && pressed(item.keys, event)) {
        event.preventDefault();
        if (!item.needs || NEEDS[item.needs]?.()) {
          runCommand(item.command);
        }
        return;
      }
    }
  }
}

// Runs the command the command line opened the window for, once the tree is read.
export async function runLaunch() {
  const launch = await invoke("launch_take").catch(() => null);
  if (launch?.command) {
    runCommand(launch.command, launch.args);
  }
}

export async function startMenubar({ openFolder }) {
  state.bar = document.getElementById("menubar");
  state.openFolder = openFolder;
  state.menus = JSON.parse(await invoke("commands_read")).menus;
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
    onShortcut(event);
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

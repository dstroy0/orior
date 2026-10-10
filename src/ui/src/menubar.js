// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The menu bar, as commands.json lists it: the list the command line reads too, so that each item
// here is `orior <menu> <command>` there. What each command does in the window is COMMANDS below,
// what it needs before it can act is NEEDS, whether an item that is on or off is on is CHECKS, and
// the keys an item lists are bound here as well, with its `also`, a second key that does the same. A
// key the editor or the terminal takes first stays theirs, and the keys of a command for the editor
// are given to the editor, which acts on them while it holds the keys.
//
// A menu of jobs lists the catalog's groups it names, split by what each job works on where it says
// so, and a group with no job in the tree has no menu. A menu commands.json gives a `strip` icon is
// on the tool strip in place of the bar. A job chosen from a menu shows in the run view with its
// form, and starts only from there or from Run, Start.
//
// A press of the menu bar opens its menu, and while one is open the pointer moving
// to another title opens that one instead. Alt held marks each title's letter, Alt and the letter
// opens the menu, and Alt alone gives the bar the keys: Left and Right step along it, Down, Enter or
// Space opens, and Escape hands the keys back. In an open menu Left and Right step to the menus on
// either side. Past a number of titles set by the window's width, and past what fits, the titles go
// into the last, a menu of menus.

import { invoke, pick } from "./bridge.js";
import { showClone } from "./clone.js";
import { showCreate, showInit } from "./create.js";
import { say } from "./statusbar.js";
import { showBookmarks, toggleBookmark } from "./bookmarks.js";
import { debugFile, debugging, isPaused, restartDebug, step, stopDebug, toggleBreakpointHere, toggleDebugPanel } from "./debug.js";
import { bindEditorKeys, crumbsShown, editing, openAt as openFileAt, openFile, recentFiles, saving, setCrumbs, setSaving } from "./edit.js";
import { showPane } from "./explorer.js";
import { openPalette, startPalette } from "./palette.js";
import { showPreferences } from "./preferences.js";
import { showGenerator, showPlugins } from "./pluginsheet.js";
import { loadPlugins } from "./plugins.js";
import { runToolchains } from "./toolchains.js";
import { openUserCss } from "./usercss.js";
import { focusSearch } from "./search.js";
import { zoomBy } from "./zoom.js";
import { clipText, closeMenu, menuOpen, showMenu } from "./menu.js";
import { chosenJob, chosenLive, listedJobs, showJob, startChosen, stopChosen, subject } from "./run.js";
import { followsSystem, scheme, setFollowSystem, setScheme, toggleScheme } from "./scheme.js";
import { autoCollapse, paneShown, setAutoCollapse, togglePane } from "./sides.js";
import { clearTerminal, killTerminal, newTerminal, toggleTerminal } from "./terminal.js";
import { showView } from "./views.js";
import { askReports, reportForm } from "./reports.js";
import { wordmark } from "./wordmark.js";

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

// The words on and off as true and false, and anything else as undefined, for the setting to flip.
const onOff = (args) => (args[0] === "on" ? true : args[0] === "off" ? false : undefined);

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

// The file a path names, at the line and column after its colons, or the quick open where none is
// named.
function goToFile(args = []) {
  const found = (args[0] ?? "").match(/^(.*?)(?::(\d+))?(?::(\d+))?$/);
  if (!found[1]) {
    openPalette("");
    return;
  }
  showView("edit");
  openFileAt(found[1], Math.max(0, Number(found[2] || 1) - 1), Math.max(0, Number(found[3] || 1) - 1));
}

// Find in Files: the explorer's Search pane, holding the words given where there are any.
function findInFiles(args = []) {
  showView("edit");
  togglePane(true);
  showPane("search");
  focusSearch(args.length ? args.join(" ") : undefined);
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

// File, Create, File and Folder: the path the command line gave, made at once, or one asked for. A
// file made opens.
async function create(folder, given) {
  const made = async (path) => {
    await editing().treeChanged();
    if (!folder) {
      showView("edit");
      await openFile(path);
    }
  };
  if (!given) {
    showCreate(sheet, { folder, start: editing().folderHere(), made });
    return;
  }
  try {
    await invoke(folder ? "folder_create" : "file_create", { path: given });
    await made(given);
  } catch (error) {
    say(String(error), { failed: true });
  }
}

// File, Create, Repository: the open tree made a repository, or a folder chosen.
function createRepository(given) {
  showInit(sheet, {
    given,
    made: async (folder, here) => {
      if (here) {
        await editing().treeChanged();
      }
      say(`${folder} is a repository now.`);
    },
  });
}

// File, Open, File: a file of the tree, the one the command line named or one the picker gives.
async function openAnyFile(given) {
  const chosen = given ?? (await pick("file"));
  if (typeof chosen !== "string") {
    return;
  }
  const path = (await invoke("tree_relative", { path: chosen }).catch(() => null)) ?? (/^[a-zA-Z]:|^\//.test(chosen) ? null : chosen);
  if (!path) {
    say(`${chosen} is outside the open tree.`, { failed: true });
    return;
  }
  showView("edit");
  await openFile(path);
}

// What each command does in the window, given the words the command line passed it, if any.
const COMMANDS = {
  "create-file": (args) => create(false, args[0]),
  "create-folder": (args) => create(true, args[0]),
  "create-repository": (args) => createRepository(args[0] ?? null),
  "open-file": (args) => openAnyFile(args[0]),
  "open-folder": (args) => state.openFolder(args[0]),
  "open-repository": (args) => showClone(sheet, state.openFolder, args, { open: true }),
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
  search: findInFiles,
  palette: (args) => openPalette(`>${args.join(" ")}`),
  "zoom-in": () => zoomBy(1),
  "zoom-out": () => zoomBy(-1),
  "zoom-reset": () => zoomBy(0),
  back: () => editing().back(),
  forward: () => editing().forward(),
  "last-editor": () => editing().lastEditor(),
  symbol: (args) => {
    showView("edit");
    openPalette(`@${args.join(" ")}`);
  },
  "symbol-tree": (args) => {
    showView("edit");
    openPalette(`#${args.join(" ")}`);
  },
  replace: inEditor((e) => e.find.open(true)),
  comment: inEditor((e) => e.toggleComment()),
  format: () => editing().format(),
  "select-all": inEditor((e) => e.selectAll()),
  "select-line": inEditor((e) => e.selectLine()),
  "join-lines": inEditor((e) => e.joinLines()),
  "sort-ascending": inEditor((e) => e.sortLines(false)),
  "sort-descending": inEditor((e) => e.sortLines(true)),
  "unique-lines": inEditor((e) => e.uniqueLines()),
  "upper-case": inEditor((e) => e.transformCase("upper")),
  "lower-case": inEditor((e) => e.transformCase("lower")),
  "title-case": inEditor((e) => e.transformCase("title")),
  "column-mode": (args) => editing().setColumnMode(onOff(args) ?? !editing().columnMode()),
  "next-change": () => {
    showView("edit");
    editing().nextChange();
  },
  "previous-change": () => {
    showView("edit");
    editing().previousChange();
  },
  "copy-line-up": inEditor((e) => e.copyLines(-1)),
  "copy-line-down": inEditor((e) => e.copyLines(1)),
  "move-line-up": inEditor((e) => e.moveLines(-1)),
  "move-line-down": inEditor((e) => e.moveLines(1)),
  "cursor-above": inEditor((e) => e.addCursor(-1)),
  "cursor-below": inEditor((e) => e.addCursor(1)),
  "next-occurrence": inEditor((e) => e.addMatch(false)),
  "all-occurrences": inEditor((e) => e.addMatch(true)),
  "edit-view": () => showView("edit"),
  "side-bar": (args) => togglePane(args[0] === "show" ? true : args[0] === "hide" ? false : undefined),
  "auto-collapse": (args) => setAutoCollapse(args[0] === "on" ? true : args[0] === "off" ? false : undefined),
  "auto-save": (args) => setSaving("auto-save", onOff(args)),
  "format-on-save": (args) => setSaving("format", onOff(args)),
  breadcrumbs: (args) => setCrumbs(onOff(args)),
  "bracket-pairs": (args) => editing().setBrackets(onOff(args) ?? !editing().brackets()),
  "sticky-scroll": (args) => editing().setSticky(args[0] === "on" ? true : args[0] === "off" ? false : !editing().sticky()),
  preferences: () =>
    showPreferences(sheet, {
      menus: state.menus,
      runCommand,
      checks: CHECKS,
      more: [
        { label: "Trim Trailing Whitespace", on: () => saving("trim"), set: (on) => setSaving("trim", on) },
        { label: "Insert Final Newline", on: () => saving("final-newline"), set: (on) => setSaving("final-newline", on) },
      ],
    }),
  "user-css": () => openUserCss(),
  plugins: () => showPlugins(sheet),
  "new-plugin": (args) => showGenerator(sheet, args),
  "reload-plugins": () => loadPlugins(),
  toolchains: (args) => runToolchains(sheet, args),
  clone: (args) => showClone(sheet, state.openFolder, args),
  "run-view": () => showView("run"),
  "terminal-view": () => toggleTerminal(),
  scheme: (args) => (args[0] === "light" || args[0] === "dark" ? setScheme(args[0]) : toggleScheme()),
  "scheme-system": (args) => setFollowSystem(onOff(args) ?? !followsSystem()),
  "split-right": () => (showView("edit"), editing().split("right")),
  "split-down": () => (showView("edit"), editing().split("down")),
  unsplit: () => editing().unsplit(),
  "fold-all": inEditor((e) => e.foldAll(true)),
  "unfold-all": inEditor((e) => e.foldAll(false)),
  file: goToFile,
  "search-everywhere": () => openPalette("", { everywhere: true }),
  bookmark: inEditor(() => toggleBookmark()),
  bookmarks: () => showBookmarks(),
  "expand-selection": inEditor((e) => e.expandSelection()),
  "shrink-selection": inEditor((e) => e.shrinkSelection()),
  "recent-files": () => openPalette(""),
  line: (args) => inEditor((e) => (args[0] ? e.goTo(Math.max(0, Number(args[0]) - 1)) : e.goto.open()))(),
  bracket: inEditor((e) => e.jumpBracket()),
  bridge: goToBridge,
  definition: inEditor(() => editing().definition()),
  usages: inEditor(() => editing().usages()),
  rename: inEditor(() => editing().rename()),
  "extract-variable": inEditor(() => editing().extractVariable()),
  "extract-constant": inEditor(() => editing().extractConstant()),
  "extract-function": inEditor(() => editing().extractFunction()),
  "inline-variable": inEditor(() => editing().inlineVariable()),
  "quick-fix": inEditor(() => editing().quickFix()),
  "parameter-info": inEditor(() => editing().parameterInfo()),
  "quick-doc": inEditor(() => editing().quickDoc()),
  debug: () => {
    showView("edit");
    debugFile();
  },
  "restart-debug": () => restartDebug(),
  continue: () => step("continue"),
  "step-over": () => step("next"),
  "step-into": () => step("stepIn"),
  "step-out": () => step("stepOut"),
  pause: () => step("pause"),
  "stop-debug": () => stopDebug(),
  breakpoint: inEditor(() => toggleBreakpointHere()),
  "debug-view": () => toggleDebugPanel(),
  "next-problem": inEditor((e) => e.stepProblem(1)),
  "previous-problem": inEditor((e) => e.stepProblem(-1)),
  "next-match": inEditor((e) => e.find.step(1)),
  "previous-match": inEditor((e) => e.find.step(-1)),
  start: () => chosenJob() && startChosen(),
  stop: stopChosen,
  "run-file": () => editing().runFile(),
  validate: () => editing().validate(),
  list: (args) => searchJobs(args[0]),
  show: (args) => searchJobs(args[0]),
  new: newTerminal,
  toggle: () => toggleTerminal(),
  clear: clearTerminal,
  kill: killTerminal,
  keys: showShortcuts,
  report: (args) => reportForm(sheet, args),
  "auto-report": async (args) => {
    const on = args[0] === "on" ? true : args[0] === "off" ? false : !state.autoReport;
    await invoke("report_auto_set", { on });
    state.autoReport = on;
  },
  about: showAbout,
};

// Whether an item that is on or off is on, by the name commands.json gives it.
const CHECKS = {
  "side-bar": paneShown,
  "auto-collapse": autoCollapse,
  "sticky-scroll": () => editing().sticky(),
  "column-mode": () => editing().columnMode(),
  "auto-save": () => saving("auto-save"),
  "format-on-save": () => saving("format"),
  breadcrumbs: crumbsShown,
  "bracket-pairs": () => editing().brackets(),
  "auto-report": () => state.autoReport,
  "scheme-system": followsSystem,
  split: () => editing().splitShown(),
};

// What a command needs before it can act, by the name commands.json gives the need.
const NEEDS = {
  editor: () => Boolean(editing().editor),
  text: () => Boolean(editing().editor),
  changes: () => editing().hasChanges(),
  "changed-active": () => editing().activeChanged,
  changed: () => editing().changed,
  tab: () => Boolean(editing().active),
  tabs: () => editing().open,
  job: () => Boolean(chosenJob()),
  live: () => chosenLive(),
  debugging: () => debugging(),
  debugged: () => debugging() || Boolean(editing().active),
  paused: () => isPaused(),
  running: () => debugging() && !isPaused(),
};

// Runs a command of a menu, as its item does, with the words the command line gave it.
export function runCommand(command, args = []) {
  return COMMANDS[command]?.(args);
}

// Every command the menus list that can act now, for the quick open's >.
function paletteCommands() {
  const found = [];
  for (const menu of state.menus) {
    for (const item of menu.all) {
      if (item.command === "palette" || (item.needs && !NEEDS[item.needs]?.())) {
        continue;
      }
      const label = item.labels?.[scheme()] ?? item.label;
      found.push({ key: `${menu.title}/${item.command}`, menu: menu.title, label: label.replace(/…$/, ""), keys: item.keys, run: () => runCommand(item.command) });
    }
  }
  return found;
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
  const shown = (item) =>
    item === "-"
      ? "-"
      : item.items
        ? { label: item.label, items: item.items.map(shown) }
        : {
            label: item.labels?.[scheme()] ?? item.label,
            keys: item.keys,
            checked: item.checks ? Boolean(CHECKS[item.checks]?.()) : undefined,
            disabled: Boolean(item.needs && !NEEDS[item.needs]?.()),
            run: () => runCommand(item.command),
          };
  const items = (menu.items ?? []).map(shown);
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
    const rows = menu.all.filter((item) => item.keys);
    if (!rows.length) {
      continue;
    }
    const block = document.createElement("section");
    block.append(Object.assign(document.createElement("h3"), { textContent: menu.title }));
    for (const item of rows) {
      const row = document.createElement("div");
      row.append(Object.assign(document.createElement("span"), { textContent: item.labels?.[scheme()] ?? item.label }), Object.assign(document.createElement("kbd"), { textContent: item.also ? `${item.keys}, ${item.also}` : item.keys }));
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
    wordmark("h2"),
    Object.assign(document.createElement("p"), { textContent: version }),
    Object.assign(document.createElement("p"), { className: "tree", textContent: document.getElementById("tree-path").textContent }),
  );
  sheet(body);
}

// A sheet over the app, which Escape, a press outside it or its × closes. Answers the sheet.
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
  return dialog;
}

// The menus the bar has a title for: each of commands, and each of jobs that has a job in the tree.
function shownMenus() {
  return state.menus.filter((menu) => !menu.strip && (menu.items?.length || jobsIn(menu.groups ?? []).length));
}

// The menus of jobs the tool strip holds in place of the bar, each its title, its icon and its
// items, where its groups have a job in the tree.
export function stripMenus() {
  return state.menus.filter((menu) => menu.strip && jobsIn(menu.groups ?? []).length).map((menu) => ({ title: menu.title, icon: menu.strip, items: () => itemsOf(menu) }));
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
  // The tool strip draws the menus it holds again with the bar's.
  window.dispatchEvent(new Event("menus-drawn"));
}

// The keys a command lists, or undefined where it lists none.
export function keysOf(command) {
  for (const menu of state.menus) {
    const item = menu.all.find((one) => one.command === command);
    if (item) {
      return item.keys;
    }
  }
  return undefined;
}

// Opens the menu under the at'th title. The keys go into it where it was opened from the keyboard or
// by a press, and stay where they are where the pointer only passed onto its title.
function openAt(at, keyed) {
  const count = state.titles.length;
  const index = ((at % count) + count) % count;
  const { menu, button, items: itemsFor } = state.titles[index];
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
// them for itself. Of two commands with the same keys, the first that can act now runs, as F5
// continues a stopped program and otherwise starts the chosen job. A key bound here never reaches
// the web view, even where no command of it can act.
function onShortcut(event) {
  if (event.defaultPrevented || menuOpen()) {
    return;
  }
  let bound = false;
  for (const menu of state.menus) {
    for (const item of menu.all) {
      if (item.needs !== "editor" && (pressed(item.keys, event) || pressed(item.also, event))) {
        bound = true;
        if (!item.needs || NEEDS[item.needs]?.()) {
          event.preventDefault();
          runCommand(item.command);
          return;
        }
      }
    }
  }
  if (bound) {
    event.preventDefault();
  }
}

// Runs the command the command line opened the window for, once the tree is read.
export async function runLaunch() {
  const launch = await invoke("launch_take").catch(() => null);
  if (launch?.command) {
    runCommand(launch.command, launch.args);
  }
}

// Starts the menu bar and every key its commands are bound to. `commands` is the reading of the
// commands, where it was asked for already. The page marks the moment the keys are bound as
// "keys-bound" in its timeline.
export async function startMenubar({ openFolder, commands = invoke("commands_read") }) {
  state.bar = document.getElementById("menubar");
  state.openFolder = openFolder;
  state.menus = JSON.parse(await commands).menus;
  // Each menu's commands in one list, those of its groups among them.
  const flat = (items) => items.flatMap((item) => (item === "-" ? [] : item.items ? flat(item.items) : [item]));
  state.menus.forEach((menu) => (menu.all = flat(menu.items ?? [])));
  bindEditorKeys(
    state.menus.flatMap((menu) =>
      menu.all.filter((item) => item.needs === "editor").flatMap((item) => [item.keys, item.also].filter(Boolean).map((keys) => ({ keys, run: () => runCommand(item.command) }))),
    ),
  );
  startPalette({
    commands: paletteCommands,
    findFiles: (query, recent, most) => invoke("files_find", { query, recent, most }),
    recent: recentFiles,
    symbols: () => editing().symbols(),
    treeSymbols: (query) => invoke("symbols_find", { query }),
    lineCount: () => editing().lineCount(),
    goLine: (line, col) => editing().goLine(line, col),
    openFile: (path, line, col) => {
      showView("edit");
      if (line === null) {
        openFile(path);
      } else {
        openFileAt(path, line, col);
      }
    },
  });
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
  performance.mark("keys-bound");
  state.autoReport = await invoke("report_auto").catch(() => true);
  const [asked, question] = await invoke("report_asked").catch(() => [true, ""]);
  if (!asked) {
    askReports(sheet, question, (on) => {
      state.autoReport = on;
    });
  }
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The menu bar, as commands.json lists it: the list the command line reads too, so that each item
// here is `orior_gui <menu> <command>` there. What each command does in the window is COMMANDS below,
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
// to another title opens that one instead. Alt pressed marks each title's letter, Alt and the letter
// opens the menu, and Alt alone gives the bar the keys: Left and Right step along it, Down, Enter or
// Space opens, and Escape hands the keys back. In an open menu Left and Right step to the menus on
// either side. Past a number of titles set by the window's width, and past what fits, the titles go
// into the last, a menu of menus.

import { invoke, listen } from "./bridge.js";
import { step } from "./debug.js";
import { editing, openAt as openFileAt, openFile, recentFiles } from "./edit.js";
import { keptMacros, onMacros } from "./macros.js";
import { startPalette } from "./palette.js";
import { closeMenu, menuOpen, showMenu } from "./menu.js";
import { listedJobs, showJob, startJobWords, subject } from "./run.js";
import { scheme } from "./scheme.js";
import { buttonName, onKeys, wheelName } from "./keys.js";
import { pluginCommandRows } from "./scripts.js";
import { showView } from "./views.js";
import { askReports } from "./reports.js";
import { commandNamed } from "./menubar/commands.js";
import { bindMacros, CHECKS, FILLS, NEEDS, playBack } from "./menubar/checks.js";
import { showBranches } from "./menubar/git.js";
import { showMemory } from "./menubar/settings.js";
import { sheet } from "./menubar/sheets.js";
import { freshenToolbar, loadReaderMenus, shownMenus } from "./menubar/reader.js";
import { applyKeys, onShortcut, runPressed } from "./menubar/keys.js";
export { isChecked } from "./menubar/checks.js";
export { allCommands, firstKeys } from "./menubar/keys.js";
export { restoreEnvironment } from "./menubar/settings.js";
export { askFor, sheet } from "./menubar/sheets.js";

// The most titles the bar shows at a window width: [narrowest width in pixels, titles]. The rest go
// into the last title's menu, and so do more where even these do not fit.
const TITLES_AT = [
  [1600, Infinity],
  [1440, 13],
  [1280, 11],
  [1120, 9],
  [0, 7],
];

export const state = { bar: null, menus: [], open: -1, titles: [], altPressed: false, before: null, openFolder: () => {} };

// The words on and off as true and false, and anything else as undefined, for the setting to flip.
export const onOff = (args) => (args[0] === "on" ? true : args[0] === "off" ? false : undefined);

// Whether Go to File matches the letters typed in order anywhere in a path, or only the text as typed.
const FUZZY_KEY = "orior.fuzzy-files";
export const fuzzyFiles = () => localStorage.getItem(FUZZY_KEY) !== "false";
export const setFuzzyFiles = (on) => localStorage.setItem(FUZZY_KEY, String(on));

// Runs a command of a menu, as its item does, with the words the command line gave it.
// A protocol job's command: the job shown, and started with the words where a command line gave any.
export function protocolJob(id) {
  return (args = []) => {
    showView("run");
    showJob(id);
    return args.length ? startJobWords(id, args) : undefined;
  };
}

export function runCommand(command, args = []) {
  return commandNamed(command)?.(args);
}

// Whether the window answers `command` itself.
export const hasCommand = (command) => Boolean(commandNamed(command));

// Every command the menus list that can act now, for the quick open's >.
function paletteCommands() {
  const found = [];
  for (const menu of state.menus.filter((one) => !one.popup)) {
    for (const item of menu.all) {
      if (item.command === "palette" || (item.needs && !NEEDS[item.needs]?.())) {
        continue;
      }
      const label = item.labels?.[scheme()] ?? item.label;
      found.push({ key: `${menu.title}/${item.command}`, menu: menu.title, label: label.replace(/…$/, ""), keys: item.keys, run: () => runCommand(item.command) });
    }
  }
  if (editing().editor) {
    for (const one of keptMacros()) {
      found.push({ key: `Macros/${one.name}`, menu: "Macros", label: one.name, keys: one.keys || undefined, run: () => playBack(1, one.name) });
    }
  }
  found.push(...pluginCommandRows());
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

export const jobsIn = (groups) => listedJobs().filter((job) => groups.includes(job.group));

// A menu's items: its commands, then the jobs of its groups, split by what each works on where the
// menu says so.
function itemsOf(menu) {
  const shown = (item) =>
    item === "-"
      ? "-"
      : item.items
        ? { label: item.label, items: item.items.flatMap(filled) }
        : {
            label: item.labels?.[scheme()] ?? item.label,
            keys: item.keys,
            checked: item.checks ? Boolean(CHECKS[item.checks]?.()) : undefined,
            disabled: Boolean(item.needs && !NEEDS[item.needs]?.()),
            run: () => runCommand(item.command),
          };
  // An entry to be filled in gives its items as the menu opens.
  const filled = (item) => (item.fill ? (FILLS[item.fill]?.() ?? []) : [shown(item)]);
  const items = (menu.items ?? []).filter((item) => item === "-" || !item.hidden).flatMap(filled);
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

// The menus of jobs the tool strip holds in place of the bar, each its title, its icon and its
// items, where its groups have a job in the tree.
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
    button.setAttribute("aria-keyshortcuts", `Alt+${menu.title[letter].toUpperCase()}`);
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
  state.bar.classList.add("pressed");
  state.titles[0]?.button.focus();
}

function leaveBar(refocus) {
  if (!state.bar.classList.contains("pressed")) {
    return;
  }
  state.bar.classList.remove("pressed");
  if (refocus && state.before?.isConnected) {
    state.before.focus();
  }
}

// Runs the command the command line opened the window for, once the tree is read.
export async function runLaunch() {
  const launch = await invoke("launch_take").catch(() => null);
  if (launch?.command) {
    runCommand(launch.command, launch.args);
  }
}

// How long a command line waits on a command still running, one that asks the reader or works on,
// before it is answered that the command began.
const ANSWER_BY = 2000;

// Runs each command a command line says to this window, as reach.rs sends it, as its menu runs it,
// and answers whether it ran and what it said: a command that gives text gives it to the command line.
export async function hearCommandLine() {
  const heard = new Set();
  await listen("command", async ({ payload }) => {
    if (!payload?.id || heard.has(payload.id)) {
      return;
    }
    heard.add(payload.id);
    const answer = (done, said = "") => invoke("command_answer", { id: payload.id, done, message: said }).catch(() => {});
    if (!hasCommand(payload.command)) {
      answer(false, `the window has no command ${payload.command}`);
      return;
    }
    try {
      const ran = await Promise.race([Promise.resolve(runCommand(payload.command, payload.args ?? [])), new Promise((done) => setTimeout(done, ANSWER_BY))]);
      answer(true, typeof ran === "string" ? ran : "");
    } catch (error) {
      answer(false, String(error?.message ?? error));
    }
  });
}

// Starts the menu bar and every key its commands are bound to. `commands` is the reading of the
// commands, where it was asked for already. The page marks the moment the keys are bound as
// "keys-bound" in its timeline.
export async function startMenubar({ openFolder, commands = invoke("commands_read") }) {
  state.bar = document.getElementById("menubar");
  state.openFolder = openFolder;
  state.menus = JSON.parse(await commands).menus;
  // Each menu's commands in one list, those of its groups among them.
  const flat = (items) => items.flatMap((item) => (item === "-" || item.fill ? [] : item.items ? flat(item.items) : [item]));
  state.menus.forEach((menu) => (menu.all = flat(menu.items ?? [])));
  applyKeys();
  onKeys(() => {
    applyKeys();
    drawMenubar();
  });
  startPalette({
    commands: paletteCommands,
    findFiles: (query, recent, most) => invoke("files_find", { query, recent, most, fuzzy: fuzzyFiles() }),
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
  state.bar.addEventListener("focusout", (event) => !state.bar.contains(event.relatedTarget) && state.bar.classList.remove("pressed"));
  // The reader's menus and toolbar, and again each time the window comes back to the front.
  loadReaderMenus();
  window.addEventListener("focus", () => loadReaderMenus());
  document.getElementById("toolbar").addEventListener("pointerenter", freshenToolbar);
  window.addEventListener("keydown", (event) => {
    if (event.key === "Alt") {
      state.altPressed = true;
      document.body.classList.add("alt-pressed");
      return;
    }
    state.altPressed = false;
    if (event.altKey && !event.ctrlKey && !event.metaKey && /^Key[A-Z]$/.test(event.code)) {
      const letter = event.code.slice(3).toLowerCase();
      const at = state.titles.findIndex(({ button }) => button.dataset.letter === letter);
      if (at >= 0) {
        event.preventDefault();
        document.body.classList.remove("alt-pressed");
        openAt(at, true);
      }
      return;
    }
    onShortcut(event);
  });
  // A button of the mouse past its first three, and the wheel turned with a key pressed, run the command
  // that holds them, where one does.
  for (const type of ["mousedown", "mouseup"]) {
    window.addEventListener(
      type,
      (event) => {
        const name = buttonName(event);
        if (name && (type === "mousedown" ? state.menus?.some((menu) => menu.all.some((item) => item.keys === name || item.also === name)) : runPressed(name))) {
          event.preventDefault();
        }
      },
      true,
    );
  }
  window.addEventListener(
    "wheel",
    (event) => {
      if (runPressed(wheelName(event))) {
        event.preventDefault();
      }
    },
    { capture: true, passive: false },
  );
  window.addEventListener("keyup", (event) => {
    if (event.key !== "Alt") {
      return;
    }
    document.body.classList.remove("alt-pressed");
    if (state.altPressed && !menuOpen()) {
      event.preventDefault();
      if (state.bar.classList.contains("pressed")) {
        leaveBar(true);
      } else {
        holdBar();
      }
    }
    state.altPressed = false;
  });
  window.addEventListener("blur", () => document.body.classList.remove("alt-pressed"));
  let wait = 0;
  window.addEventListener("resize", () => {
    window.clearTimeout(wait);
    wait = window.setTimeout(drawMenubar, 120);
  });
  drawMenubar();
  document.getElementById("status-branch").addEventListener("click", showBranches);
  document.getElementById("status-memory").addEventListener("click", showMemory);
  bindMacros();
  onMacros(bindMacros);
  performance.mark("keys-bound");
  state.autoReport = await invoke("report_auto").catch(() => true);
  const [prompted, question] = await invoke("report_prompted").catch(() => [true, ""]);
  const asked = prompted || (await invoke("app_debug").catch(() => false));
  if (!asked) {
    askReports(sheet, question, (on) => {
      state.autoReport = on;
    });
  }
}

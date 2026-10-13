// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The menu bar's keys: a key two commands for different kinds of file share goes to the one whose
// kind the file shown is. And every item of every menu, pressed from its menu as a person presses it.

import { allCommands, firstKeys, hasCommand, isChecked, runCommand } from "../../src/menubar.js";
import { invoke, listen } from "../../src/bridge.js";
import { closeMenu } from "../../src/menu.js";
import { editing, openFile } from "../../src/edit.js";
import { killTerminal, newTerminal, runInTerminal, toggleTerminal } from "../../src/terminal.js";
import { showView } from "../../src/views.js";
import { paletteOpen } from "../../src/palette.js";
import { closeDiff } from "../../src/diffview.js";
import { itemShown } from "../../src/statusitems.js";
import { changeShown, closeChange } from "../../src/changeview.js";
import { focusOn, memoryBudget, noticesHeld, noticesSaid } from "../../src/statusbar.js";
import { previewMode } from "../../src/preview.js";
import { scheme } from "../../src/scheme.js";
import { zoom } from "../../src/zoom.js";
import { checkersNamed, setCheckers } from "../../src/servers.js";
import { closePeek, peekShown } from "../../src/peek.js";
import { bookmarksOf } from "../../src/bookmarks.js";
import { boardShown, closeBoard, tasks } from "../../src/gather.js";
import { outlinePinned, paneOpen, panesNow, putPanes, showGroup, showPane, shownGroup } from "../../src/explorer.js";
import { addComment, comments } from "../../src/review.js";
import { togglePane } from "../../src/sides.js";
import { forgetMacro, keptMacros, lastMacro, recording } from "../../src/macros.js";
import { breakpointsOf, canStepBack, debugging, isPaused, pausedLineOf, toggleDebugPanel } from "../../src/debug.js";
import { chosenJob, chosenLive, loadRun, showJob } from "../../src/run.js";
import { testsOf } from "../../src/tests.js";

const { test, assert, ui } = window.__harness;

// The page's storage as the file found it, put back by its last test.
const STORED = Object.entries(localStorage);
// The explorer's group and panes as the file found them, put back by its last test.
const FOUND = { group: shownGroup(), panes: panesNow() };

test("a key two commands for different kinds of file share goes to the one the file shown is for", () => {
  const items = [
    { command: "run-statement", keys: "Ctrl+Enter", needs: "sql" },
    { command: "send-request", keys: "Ctrl+Enter", needs: "http" },
    { command: "run-sql-file", keys: "Ctrl+Shift+Enter", needs: "sql" },
  ];
  let shown = "a.sql";
  const needs = { sql: () => shown.endsWith(".sql"), http: () => shown.endsWith(".http") };
  const ran = [];
  const keys = firstKeys(items, needs, (command) => ran.push(command));
  assert.deepEqual(keys.map((one) => one.keys), ["Ctrl+Enter", "Ctrl+Shift+Enter"], "one binding for each key");
  const enter = keys[0].run;
  assert.equal(enter(), true);
  shown = "calls.http";
  assert.equal(enter(), true);
  shown = "notes.md";
  assert.equal(enter(), false, "the editor's own use of the key where neither is for the file");
  assert.deepEqual(ran, ["run-statement", "send-request"]);
});

test("Help, What's New shows what each release changed, the running version's own among them", async () => {
  const help = [...document.getElementById("menubar").children].find((one) => one.textContent.trim() === "Help");
  await ui.click(help);
  const menu = await ui.waitFor(() => document.querySelector("body > .menu"), 3000);
  await ui.click([...menu.querySelectorAll(".menu-item")].find((one) => one.querySelector("span")?.textContent === "What's New"));
  const shown = await ui.waitFor(() => document.querySelector("dialog[open] .sheet-changes"), 3000);
  try {
    const version = await invoke("app_version");
    assert.equal(shown.querySelector("h1")?.textContent, "What's new in orior");
    assert.ok([...shown.querySelectorAll("h2")].some((one) => one.textContent === version), `a section for ${version}`);
  } finally {
    document.querySelectorAll("dialog[open]").forEach((dialog) => dialog.close());
  }
});

// The answers given to what some commands ask; every other question is closed unanswered.
const ANSWERS = {
  "create-file": "walk_new.txt",
  "create-folder": "walk_folder",
  "new-branch": "walk-branch",
  stash: "walk stash",
  "macro-keep": "walk macro",
};

// What puts the window back after a command: its own press again for a setting that turns on and
// off, and the command that closes what it opened. A setting the menus check is pressed again too.
const AFTER = {
  "zoom-in": "zoom-reset",
  "zoom-out": "zoom-reset",
  "split-right": "unsplit",
  "split-down": "unsplit",
  new: "kill",
  split: "kill",
  debug: "stop-debug",
  "debug-mixed": "stop-debug",
  record: "stop-debug",
  profile: "stop-profile",
  "run-tests": "stop-tests",
  "cover-tests": "stop-tests",
  "run-failed-tests": "stop-tests",
  "macro-record": "macro-record",
  scheme: "scheme",
  "side-bar": "side-bar",
  "terminal-view": "terminal-view",
  toggle: "toggle",
  "debug-view": "debug-view",
  "profile-view": "profile-view",
  "containers-view": "containers-view",
  "mark-file": "mark-file",
  bookmark: "bookmark",
  breakpoint: "breakpoint",
};

const wait = (ms) => new Promise((done) => setTimeout(done, ms));
const openMenus = () => [...document.querySelectorAll("body > .menu")];
const itemsIn = (menu) => [...menu.querySelectorAll(":scope > .menu-item")];
const labelOf = (button) => button.querySelector("span")?.textContent ?? "";

// Every menu closed, every sheet and dialog closed, a finding of the slow plugin stopped, and the
// keys let go.
async function putBack() {
  closeMenu(false);
  document.querySelectorAll("dialog[open]").forEach((dialog) => dialog.close());
  [...document.querySelectorAll("#slow-finder:not([hidden]) .slow-finder-button")].find((button) => button.textContent === "Stop")?.click();
  await ui.release("Ctrl+Shift+Alt").catch(() => {});
  await wait(60);
}

// Opens the menu of the bar's title at `at` and, through it, each submenu `path` names, pressing
// each as a person does, and gives the items of the menu at the end.
async function openTo(at, path) {
  await putBack();
  const title = document.getElementById("menubar").children[at];
  const named = (depth) => [title.textContent.trim(), ...path.slice(0, depth)].join(" / ");
  await ui.click(title);
  let menu = await ui.waitFor(() => openMenus()[0], 3000).catch(() => {
    throw new Error(`${named(0)} did not open`);
  });
  for (const [depth, label] of path.entries()) {
    const item = itemsIn(menu).find((button) => labelOf(button) === label);
    assert.ok(item, `${named(depth + 1)} is in its menu`);
    const heard = [];
    const hear = (event) => heard.push(`${event.type}:${event.target?.className || event.target?.nodeName || "window"}${event.type === "click" ? `[${event.target?.textContent?.trim()}]` : ""}:${Math.round(performance.now())}`);
    const kinds = ["pointermove", "pointerdown", "pointerup", "click", "focusout", "blur", "resize", "keydown"];
    kinds.forEach((kind) => window.addEventListener(kind, hear, true));
    const aimed = item.getBoundingClientRect();
    await ui.click(item);
    heard.push(`aimed at ${label} at ${Math.round(aimed.top)}, now at ${Math.round(item.getBoundingClientRect().top)}`);
    menu = await ui.waitFor(() => openMenus()[depth + 1], 3000).catch(() => {
      const open = openMenus().map((one) => `${itemsIn(one).length} items at ${Math.round(one.getBoundingClientRect().top)}`);
      throw new Error(`${named(depth + 1)} did not open: ${open.length} menus open (${open.join(", ")}), the window ${innerWidth}x${innerHeight}; heard ${heard.join(" ")}; closed by ${(window.__removedMenus ?? []).slice(-3).join(" ||| ")}`);
    }).finally(() => kinds.forEach((kind) => window.removeEventListener(kind, hear, true)));
  }
  return itemsIn(menu);
}

// Each item that runs something, by the path of labels that reaches it from the title at `at`:
// every item of every submenu, opened as a person opens it.
async function leavesOf(at, path = []) {
  const found = [];
  for (const button of await openTo(at, path)) {
    const label = labelOf(button);
    if (button.querySelector(".menu-more")) {
      found.push(...(await leavesOf(at, [...path, label])));
    } else if (!button.disabled) {
      found.push([...path, label]);
    }
  }
  return found;
}

test(
  "every item of every menu is pressed from its menu as a person presses it, and none throws",
  async () => {
    const commandOf = new Map();
    // A setting the menus check turns on and off: pressed again, it stands as the walk found it.
    const checking = new Set(allCommands().filter((item) => item.checks).map((item) => item.command));
    for (const item of allCommands()) {
      for (const label of [item.label, ...Object.values(item.labels ?? {})]) {
        commandOf.set(label, item.command);
      }
    }
    const threw = [];
    let pressing = "";
    // A call the harness holds, as Exit's or a window's moved to another display, fails as it would
    // never fail for a person: its error is set aside, the page's and the walk's alike.
    const said = (event) => String(event.error?.message ?? event.reason?.message ?? event.reason ?? event.message);
    const heard = (event) => {
      if (said(event).includes(window.__harness.held)) {
        event.preventDefault();
        return;
      }
      threw.push(`${pressing}: ${said(event)}`);
    };
    addEventListener("error", heard);
    addEventListener("unhandledrejection", heard);
    // Each menu taken off the page, when and by what calls, said where a menu does not open: a press
    // that finds its menus closed names what closed them.
    window.__removedMenus = [];
    const remove = Element.prototype.remove;
    Element.prototype.remove = function () {
      if (this.classList?.contains("menu")) window.__removedMenus.push(`${Math.round(performance.now())} ${new Error("removed").stack.split(String.fromCharCode(10)).slice(1, 7).join(" <- ")}`);
      return remove.call(this);
    };
    let pressed = 0;
    // What kept an item from being reached or pressed, by its path, the walk going on past it.
    const unreached = [];
    try {
      const titles = document.getElementById("menubar").children.length;
      for (let at = 0; at < titles; at++) {
        const title = document.getElementById("menubar").children[at].textContent.trim();
        let paths = [];
        try {
          paths = await leavesOf(at);
        } catch (error) {
          unreached.push(`${title}: ${error.message}`);
          continue;
        }
        for (const path of paths) {
          const label = path.at(-1);
          const command = commandOf.get(label) ?? "";
          pressing = `${title} / ${path.join(" / ")}`;
          try {
            const items = await openTo(at, path.slice(0, -1));
            const item = items.find((button) => labelOf(button) === label);
            if (!item || item.disabled) {
              continue;
            }
            await ui.click(item);
            pressed++;
            await wait(250);
            const field = document.querySelector("dialog[open] .sheet-ask .report-field");
            if (field) {
              if (ANSWERS[command]) {
                field.focus();
                await ui.type(ANSWERS[command]);
                await ui.key("Enter");
              } else {
                await ui.key("Escape");
              }
              await wait(150);
            }
          } catch (error) {
            unreached.push(`${pressing}: ${error.message}`);
          }
          await putBack();
          if (AFTER[command] || checking.has(command)) {
            runCommand(AFTER[command] ?? command);
            await wait(150);
            await putBack();
          }
        }
      }
    } finally {
      Element.prototype.remove = remove;
      delete window.__removedMenus;
      removeEventListener("error", heard);
      removeEventListener("unhandledrejection", heard);
      await putBack();
    }
    assert.deepEqual(unreached, [], "the items not reached or pressed");
    assert.deepEqual(threw, [], "what the items threw");
    assert.ok(pressed > 100, `pressed ${pressed} items`);
  },
  { timeout: 1_800_000 },
);

test("a command given at a command line on the tree runs in this window, as its menu runs it", async () => {
  const kept = localStorage.getItem("orior.word-wrap");
  showView("edit");
  await openFile("hello.py");
  editing().setWordWrap(false);
  newTerminal();
  const screen = () => document.getElementById("term-views").textContent;
  try {
    runInTerminal("orior view word-wrap on");
    await ui.waitFor(() => editing().wordWrap(), 30000, "the window to wrap its words").catch((error) => {
      throw new Error(`${error.message}; the terminal shows: ${screen().trim().slice(-600)}`);
    });
    runInTerminal("orior view word-wrap off");
    await ui.waitFor(() => !editing().wordWrap(), 30000, "the window to stop wrapping");
    assert.ok(document.querySelectorAll("#tabs .tab").length > 0, "the same window, its tabs still open");
  } finally {
    killTerminal();
    toggleTerminal(false);
    editing().setWordWrap(kept === "true");
  }
}, { timeout: 90000 });

// Each command of the menus, run as its menu runs it and as a command line runs it, and what it does
// looked for: COVERED names every command a test below runs, and the first test fails for any command
// of the menus not among them.
const COVERED = new Set();

// The settings turned on and off by a command that takes on or off, with the words that say each.
const SETTINGS = {
  "auto-save": ["on", "off"],
  "format-on-save": ["on", "off"],
  "operator-next-line": ["on", "off"],
  "column-mode": ["on", "off"],
  "side-bar": ["show", "hide"],
  "auto-collapse": ["on", "off"],
  "sticky-scroll": ["on", "off"],
  "word-wrap": ["on", "off"],
  "smooth-scroll": ["on", "off"],
  "reduce-motion": ["on", "off"],
  "flick-scroll": ["on", "off"],
  breadcrumbs: ["on", "off"],
  "bracket-pairs": ["on", "off"],
  "type-hints": ["on", "off"],
  "parameter-hints": ["on", "off"],
  whitespace: ["on", "off"],
  "past-ends": ["on", "off"],
  "hide-comments": ["on", "off"],
  toolbar: ["on", "off"],
  "margin-line": ["on", "off"],
  "focus-follows": ["on", "off"],
  "system-title": ["on", "off"],
  "strip-edge": ["on", "off"],
  "side-tabs": ["on", "off"],
  "scheme-system": ["on", "off"],
  "fuzzy-files": ["on", "off"],
  "parallel-tests": ["on", "off"],
  "line-history": ["on", "off"],
  "git-authors": ["on", "off"],
  "network-off": ["on", "off"],
};
Object.keys(SETTINGS).forEach((command) => COVERED.add(command));
COVERED.add("auto-report");

// The editor's commands on text: the text and selections each is given and those it leaves, each
// selection written between [ and ] and each cursor as |.
const ON_TEXT = {
  "select-all": ["a|b\ncd", "[ab\ncd]"],
  "select-line": ["a|b\ncd", "[ab\n]cd"],
  "copy-line-up": ["a|b\ncd", "a|b\nab\ncd"],
  "copy-line-down": ["a|b\ncd", "ab\na|b\ncd"],
  "move-line-up": ["ab\nc|d", "c|d\nab"],
  "move-line-down": ["a|b\ncd", "cd\na|b"],
  "join-lines": ["ab|\n  cd", "ab cd", "text"],
  "sort-ascending": ["[c\na\nb]", "a\nb\nc", "text"],
  "sort-descending": ["[a\nc\nb]", "c\nb\na", "text"],
  "unique-lines": ["[a\nb\na]", "a\nb", "text"],
  "upper-case": ["[ab] cd", "[AB] cd"],
  "lower-case": ["[AB] cd", "[ab] cd"],
  "title-case": ["[hello world]", "[Hello World]"],
  "cursor-above": ["ab\nc|d", "a|b\nc|d"],
  "cursor-below": ["a|b\ncd", "a|b\nc|d"],
  "next-occurrence": ["x|y xy xy", "[xy] xy xy"],
  "all-occurrences": ["x|y xy z", "[xy] [xy] z"],
  bracket: ["|(ab)", "(ab)|"],
};
Object.keys(ON_TEXT).forEach((command) => COVERED.add(command));
["undo", "redo", "comment", "fold-all", "unfold-all", "line", "expand-selection", "shrink-selection"].forEach((command) => COVERED.add(command));

const GIVEN = "commands_given.py";

// The editor of GIVEN, holding `marked`'s text with its selections.
async function given(marked) {
  if (editing().active !== GIVEN) {
    await invoke("file_write", { path: GIVEN, text: "" });
    showView("edit");
    await openFile(GIVEN);
    await ui.waitFor(() => editing().active === GIVEN && editing().editor?.s, 5000, "the given file's editor");
  }
  const view = editing().editor;
  const marks = [];
  let text = "";
  let line = 0;
  let col = 0;
  let open = null;
  for (const char of marked) {
    if (char === "|") {
      marks.push({ anchor: { line, col }, head: { line, col }, goal: null });
    } else if (char === "[") {
      open = { line, col };
    } else if (char === "]") {
      marks.push({ anchor: open, head: { line, col }, goal: null });
      open = null;
    } else {
      text += char;
      if (char === "\n") {
        line += 1;
        col = 0;
      } else {
        col += 1;
      }
    }
  }
  view.change([{ from: { line: 0, col: 0 }, to: view.doc.end(), text }], null);
  view.select(marks.length ? marks : [{ anchor: { line: 0, col: 0 }, head: { line: 0, col: 0 }, goal: null }]);
  return view;
}

// The editor's text with its selections written as `given` reads them.
function marked() {
  const view = editing().editor;
  const lines = [...view.doc.lines];
  const at = [];
  for (const sel of view.s.selections) {
    const forward = sel.anchor.line < sel.head.line || (sel.anchor.line === sel.head.line && sel.anchor.col <= sel.head.col);
    const [from, to] = forward ? [sel.anchor, sel.head] : [sel.head, sel.anchor];
    if (from.line === to.line && from.col === to.col) {
      at.push([from, "|"]);
    } else {
      at.push([from, "["], [to, "]"]);
    }
  }
  at.sort((a, b) => b[0].line - a[0].line || b[0].col - a[0].col);
  for (const [{ line, col }, mark] of at) {
    lines[line] = lines[line].slice(0, col) + mark + lines[line].slice(col);
  }
  return lines.join("\n");
}

const frame = () => new Promise((done) => requestAnimationFrame(() => done()));

// Runs `command` with `words` through the command line, on this window's tree, as a reader runs it at
// a terminal, and waits for it: the window open there runs it as its menu does, and answers.
async function press(command, ...words) {
  const item = allCommands().find((one) => one.command === command);
  assert.ok(item, `${command} is a command of the menus`);
  const told = window.__harness.servers ?? {};
  assert.ok(told.ORIOR_TEST_PROGRAM, "the harness names the window's program");
  await ui.run(told.ORIOR_TEST_PROGRAM, [item.menu.toLowerCase(), command, ...words], { env: { ORIOR_HOME: told.ORIOR_TEST_HOME, ORIOR_ROOT: told.ORIOR_TEST_TREE, ORIOR_NO_REPORTS: "1" } });
  await frame();
}

test("every command of the menus has a test of what it does, and the window answers every one", () => {
  const missing = allCommands().map((item) => item.command).filter((command) => !COVERED.has(command));
  assert.deepEqual([...new Set(missing)], [], "commands with no test of what they do");
  const unanswered = allCommands().filter((item) => !item.console && !hasCommand(item.command)).map((item) => item.command);
  assert.deepEqual(unanswered, [], "commands the window does not answer, which a command line could not run there");
});

test("each setting of the menus turns on and off as the command line's words say, and its menu shows it", async () => {
  showView("edit");
  await given("a|b");
  const checks = new Map(allCommands().filter((item) => item.checks).map((item) => [item.command, item.checks]));
  const wrong = [];
  for (const [command, [on, off]] of Object.entries(SETTINGS)) {
    const name = checks.get(command);
    assert.ok(name, `${command} has a check in its menu`);
    const was = isChecked(name);
    try {
      await press(command, on);
      if (!isChecked(name)) {
        wrong.push(`${command} ${on} left it off`);
      }
      await press(command, off);
      if (isChecked(name)) {
        wrong.push(`${command} ${off} left it on`);
      }
    } finally {
      await press(command, was ? on : off);
    }
  }
  assert.deepEqual(wrong, []);
}, { timeout: 120000 });

test("each command on text, given at the command line, leaves the text and selections it should", async () => {
  const wrong = [];
  for (const [command, [before, after, only]] of Object.entries(ON_TEXT)) {
    await given(before);
    await press(command);
    const shown = only === "text" ? editing().editor.doc.text() : marked();
    if (shown !== after) {
      wrong.push(`${command}: ${JSON.stringify(before)} gave ${JSON.stringify(shown)}, not ${JSON.stringify(after)}`);
    }
  }
  assert.deepEqual(wrong, []);
});

test("Undo takes back the last change and Redo brings it again", async () => {
  await given("[ab]");
  await press("upper-case");
  assert.equal(editing().editor.doc.text(), "AB");
  await press("undo");
  assert.equal(editing().editor.doc.text(), "ab");
  await press("redo");
  assert.equal(editing().editor.doc.text(), "AB");
});

test("Comment puts the language's comment before each line, and a second press takes it off", async () => {
  await given("x = 1|\ny = 2");
  await press("comment");
  assert.match(editing().editor.doc.line(0), /^# ?x = 1$/);
  assert.equal(editing().editor.doc.line(1), "y = 2", "the line the cursor is not on stays");
  await press("comment");
  assert.equal(editing().editor.doc.text(), "x = 1\ny = 2");
});

test("Fold All closes every region and Unfold All opens them again", async () => {
  await given("def f():\n    return 1\n\ndef g():\n    return 2\n|");
  await press("fold-all");
  assert.equal(editing().editor.s.folded.size, 2, "both functions closed");
  await press("unfold-all");
  assert.equal(editing().editor.s.folded.size, 0);
});

test("Go, Line goes to the line its word names", async () => {
  await given("a|b\ncd\nef");
  await press("line", "3");
  assert.equal(marked(), "ab\ncd\n|ef");
});

test("Expand Selection takes in the word at the cursor and Shrink Selection gives it back", async () => {
  await given("f(a|b)");
  await press("expand-selection");
  await ui.waitFor(() => marked() === "f([ab])", 3000, "the word selected");
  await press("shrink-selection");
  assert.equal(marked(), "f(a|b)");
});

const calls = window.__harness.calls;
const closeSheets = () => document.querySelectorAll("dialog[open]").forEach((dialog) => dialog.close());

// The sheet holding `selector`, waited for and then closed.
async function sheetShown(selector) {
  try {
    return await ui.waitFor(`dialog[open] ${selector}`, 5000, selector);
  } finally {
    closeSheets();
  }
}

// Whether the window has told the reader something `pattern` matches.
const told = (pattern) => [...noticesSaid(), ...noticesHeld()].some((one) => pattern.test(one.text));

// The calls named `name` the window has made since the test began.
const made = (name) => calls.made.filter((one) => one.name === name);

test("Help, Auto Report keeps whether errors file on their own, from the command line and from the menu", async () => {
  const settings = `${window.__harness.servers.ORIOR_TEST_HOME}/settings.json`;
  const kept = async () => JSON.parse(await invoke("file_read_any", { path: settings }).catch(() => "{}")).auto_report;
  await press("auto-report", "on");
  assert.equal(await kept(), true, "the command line keeps it on");
  await press("auto-report", "off");
  assert.equal(await kept(), false, "and off");
  // The menu's press keeps it through a call the harness holds; the test answers it.
  const asked = [];
  calls.answer("report_auto_set", ({ on }) => asked.push(on));
  await runCommand("auto-report", ["on"]);
  assert.ok(isChecked("auto-report"), "the menu shows it on");
  await runCommand("auto-report", ["off"]);
  assert.ok(!isChecked("auto-report"));
  assert.deepEqual(asked, [true, false]);
});

const PANELS = { "terminal-view": "term", "debug-view": "debug", "profile-view": "profile", "containers-view": "containers", "database-view": "database", "comms-view": "comms" };
Object.keys(PANELS).forEach((command) => COVERED.add(command));
["edit-view", "run-view", "palette", "focus", "preview", "preview-alone", "float", "solo", "next-display", "split-right", "split-down", "unsplit", "move-to-split", "open-in-split", "scheme", "zoom-in", "zoom-out", "zoom-reset", "memory-budget", "checkers", "find-slow-plugin"].forEach((command) => COVERED.add(command));
["new", "toggle", "split", "clear", "kill", "keys", "report", "network", "whats-new", "about"].forEach((command) => COVERED.add(command));

test("each panel of View opens and closes from the command line", async () => {
  for (const [command, id] of Object.entries(PANELS)) {
    const panel = document.getElementById(id);
    const was = panel.hidden;
    await press(command);
    await ui.waitFor(() => panel.hidden !== was, 3000, `${command} to ${was ? "open" : "close"} #${id}`);
    await press(command);
    await ui.waitFor(() => panel.hidden === was, 3000, `${command} to put #${id} back`);
  }
});

test("View, Edit and View, Run show their views", async () => {
  await press("run-view");
  assert.equal(document.body.dataset.view, "run");
  await press("edit-view");
  assert.equal(document.body.dataset.view, "edit");
});

test("View, Command Palette opens the quick open on commands, with the words given", async () => {
  await press("palette", "zoom");
  try {
    await ui.waitFor(() => paletteOpen(), 3000, "the quick open");
    assert.equal(document.querySelector(".quick-input").value, ">zoom");
  } finally {
    await ui.key("Escape");
  }
});

test("View, Focus holds the notices and lets them go", async () => {
  const was = focusOn();
  await press("focus");
  assert.equal(focusOn(), !was);
  await press("focus");
  assert.equal(focusOn(), was);
});

test("View, Preview shows the preview beside and alone, and off", async () => {
  const was = previewMode();
  try {
    await press("preview", "beside");
    assert.equal(previewMode(), "beside");
    await press("preview", "off");
    assert.equal(previewMode(), "off");
    await press("preview-alone");
    assert.equal(previewMode(), "alone");
    await press("preview-alone");
    assert.equal(previewMode(), "off");
  } finally {
    await runCommand("preview", [was]);
  }
});

test("View, Open Tool Window in a Window opens one showing the tool alone, and Show Only shows it here alone", async () => {
  const opened = [];
  calls.answer("window_open", (args) => opened.push(args.words));
  await press("float", "terminal");
  assert.deepEqual(opened.at(-1)?.slice(0, 3), ["view", "solo", "terminal"], "a window opened to show the terminal alone");
  await press("solo", "explorer");
  assert.equal(document.body.dataset.solo, "explorer");
  await press("solo");
  assert.equal(document.body.dataset.solo, undefined, "everything shown again");
});

test("View, Move to Next Display asks the window to move", async () => {
  calls.answer("window_act", () => false);
  await press("next-display");
  assert.ok(made("window_act").some((one) => one.args.act === "next-display"));
});

test("View, Split Right and Split Down split the editor, Unsplit closes it, and a file moves and opens there", async () => {
  await given("a|b");
  const desk = document.querySelector("#mode-edit .desk");
  const splitTabs = () => [...document.querySelectorAll(".split-tabs .tab")].map((tab) => tab.textContent);
  await press("split-right");
  assert.ok(editing().splitShown());
  assert.equal(desk.dataset.split, "right");
  await press("unsplit");
  assert.ok(!editing().splitShown());
  await press("split-down");
  assert.equal(desk.dataset.split, "down");
  await press("unsplit");
  await openFile("hello.py");
  await openFile(GIVEN);
  await press("move-to-split");
  assert.ok(editing().splitShown(), "a split made for the file");
  assert.ok(splitTabs().some((name) => name.includes(GIVEN)), `${GIVEN} in the split: ${splitTabs()}`);
  await press("unsplit");
  await press("open-in-split", "hello.py");
  assert.ok(splitTabs().some((name) => name.includes("hello.py")), `hello.py in the split: ${splitTabs()}`);
  await press("unsplit");
  assert.ok(!editing().splitShown());
});

test("View, Scheme turns the window light and dark", async () => {
  const was = scheme();
  try {
    await press("scheme", "light");
    assert.equal(scheme(), "light");
    await press("scheme", "dark");
    assert.equal(scheme(), "dark");
  } finally {
    await runCommand("scheme", [was]);
  }
});

test("View, Zoom In, Zoom Out and Reset Zoom", async () => {
  await press("zoom-reset");
  assert.equal(zoom(), 1);
  await press("zoom-in");
  assert.ok(zoom() > 1, `zoomed in to ${zoom()}`);
  await press("zoom-out");
  assert.equal(zoom(), 1);
  await press("zoom-out");
  assert.ok(zoom() < 1, `zoomed out to ${zoom()}`);
  await press("zoom-reset");
  assert.equal(zoom(), 1);
});

test("View, Memory Budget takes the megabytes given", async () => {
  const was = memoryBudget();
  try {
    await press("memory-budget", "777");
    assert.equal(memoryBudget(), 777);
  } finally {
    await runCommand("memory-budget", [String(was)]);
  }
});

test("View, Checkers runs the checkers named, and says a name none answers to", async () => {
  const was = checkersNamed();
  const known = await invoke("checkers_known");
  assert.ok(known.length, "the manifest knows a checker");
  const name = known[0][1];
  try {
    await press("checkers", name);
    assert.deepEqual(checkersNamed(), [name]);
    await press("checkers", "no-such-checker");
    assert.ok(told(/No checker is named no-such-checker/), "the unknown name said");
  } finally {
    await setCheckers(was);
  }
});

test("View, Find the Slow Plugin halves the plugins on until one is named, or says none is on, and sets each back", async () => {
  const off = localStorage.getItem("orior.plugins.off");
  await press("find-slow-plugin");
  // Begun again before it ends, it starts over from the plugins off before the first began.
  await press("find-slow-plugin");
  const finder = document.getElementById("slow-finder");
  while (!finder.hidden) {
    await ui.click([...finder.querySelectorAll(".slow-finder-button")].find((button) => button.textContent === "No Longer Slow"));
    await frame();
  }
  assert.ok(told(/No plugin is on|is the plugin that slows the window/), "the finding ends with what it found");
  assert.equal(localStorage.getItem("orior.plugins.off"), off ?? "[]", "each plugin set back as it was");
});

test("the Terminal menu opens, splits, clears and kills shells", async () => {
  const panel = document.getElementById("term");
  const tabs = () => panel.querySelectorAll(".term-tab").length;
  await press("new");
  const opened = tabs();
  assert.ok(!panel.hidden && opened >= 1, "a shell in a tab of its own");
  await press("split");
  assert.ok(isChecked("terminal-split"), "a shell beside");
  await press("split");
  assert.ok(!isChecked("terminal-split"), "one shell shown again");
  const screen = () => document.querySelector("#term-views .term-view:not([hidden])")?.textContent ?? "";
  await press("clear");
  await ui.waitFor(() => !screen().trim() || screen().trim().split("\n").length <= 2, 5000, "the screen cleared");
  await press("kill");
  await ui.waitFor(() => tabs() === opened - 1 || panel.hidden, 5000, "the shell ended");
  const shown = !panel.hidden;
  await press("toggle");
  assert.equal(!panel.hidden, !shown);
  await press("toggle");
  assert.equal(!panel.hidden, shown);
  await runCommand("toggle");
  if (!panel.hidden) {
    await runCommand("toggle");
  }
}, { timeout: 60000 });

test("Help, Network and What's New show their sheets", async () => {
  await press("network");
  await sheetShown(".sheet-network");
  await press("whats-new");
  await sheetShown(".sheet-changes");
});

test("Help, Keyboard Shortcuts and About, from the command line and from the menu", async () => {
  await press("keys");
  await press("about");
  await runCommand("keys");
  const keys = await sheetShown(".sheet-keys");
  assert.ok(keys.querySelector("kbd"), "the keys listed");
  await runCommand("about");
  const about = await sheetShown(".sheet-about");
  assert.equal(about.querySelector("p")?.textContent, await invoke("app_version"));
});

test("Help, Report a Problem shows its form", async () => {
  await runCommand("report");
  await sheetShown(".sheet-report");
});

["definition", "peek-definition", "peek-usages", "peek-callers", "usages", "calls"].forEach((command) => COVERED.add(command));
["back", "forward", "last-editor", "search-everywhere", "recent-files", "file", "symbol", "symbol-tree", "jump", "mark-file", "marked-files", "bookmark", "bookmarks", "bridge"].forEach((command) => COVERED.add(command));
[1, 2, 3, 4, 5, 6, 7, 8, 9].forEach((n) => COVERED.add(`mark-${n}`));
["gather-lines", "gathered-lines", "next-problem", "previous-problem", "next-change", "previous-change", "find", "replace", "next-match", "previous-match"].forEach((command) => COVERED.add(command));

const PEEKED = "peek.c";
const PLUGINS_OFF = "orior.plugins.off";

// A C file clangd serves, open with the cursor on the call of helper on its fourth line.
async function served() {
  await invoke("file_write", { path: PEEKED, text: "int helper(void) { return 1; }\n\nint main(void) {\n  return helper() + helper();\n}\n" });
  showView("edit");
  await openFile(PEEKED);
  await ui.waitFor(() => editing().active === PEEKED && editing().editor?.s, 5000, "peek.c open");
  editing().editor.goTo(3, 10);
}

test("Go, Definition, the peeks, Find Usages and Call Hierarchy ask the language server, here clangd", async () => {
  await served();
  await ui.waitFor(async () => {
    editing().editor.goTo(3, 10);
    await press("peek-definition");
    return peekShown()?.places.length > 0;
  }, 30000, "clangd to find the definition").catch((error) => {
    throw new Error(`${error.message}; the window said: ${JSON.stringify([...noticesSaid(), ...noticesHeld()].slice(-4).map((one) => one.text))}; calls: ${JSON.stringify(made("lsp_definition").slice(-2))}; the language: ${editing().editor.s.language?.name}; plugins off: ${localStorage.getItem(PLUGINS_OFF)}`);
  });
  assert.equal(peekShown().title, "Definition of helper");
  closePeek();
  editing().editor.goTo(3, 10);
  await press("peek-usages");
  await ui.waitFor(() => peekShown()?.places.length >= 2, 10000, "the usages");
  closePeek();
  editing().editor.goTo(0, 6);
  await press("peek-callers");
  await ui.waitFor(() => peekShown()?.title === "Callers of helper", 10000, "the callers");
  closePeek();
  editing().editor.goTo(3, 10);
  await press("definition");
  await ui.waitFor(() => editing().editor.head().line === 0, 10000, "the cursor at the definition");
  editing().editor.goTo(3, 10);
  await press("usages");
  await ui.waitFor(() => document.getElementById("usages-hits").children.length >= 2, 10000, "the usages listed");
  editing().editor.goTo(0, 6);
  await press("calls");
  await ui.waitFor(() => /helper/.test(document.getElementById("calls-said").textContent), 10000, "the call hierarchy of helper");
}, { timeout: 90000 });

test("Go, Go to File opens the file at its line and column, and Back and Forward step through where the cursor was", async () => {
  await given("a|b\ncd");
  await press("file", "hello.py:2:5");
  assert.equal(editing().active, "hello.py");
  assert.deepEqual(editing().editor.head(), { line: 1, col: 4 });
  await press("back");
  await ui.waitFor(() => editing().active === GIVEN, 3000, "back to the given file");
  await press("forward");
  await ui.waitFor(() => editing().active === "hello.py", 3000, "forward to hello.py");
  await press("last-editor");
  await ui.waitFor(() => editing().active === GIVEN, 3000, "the editor used before");
});

test("Go, Search Everywhere, Recent Files, Symbol and Symbol in the Tree open the quick open as each asks", async () => {
  await given("a|b");
  const input = () => document.querySelector(".quick-input");
  const quick = () => document.querySelector(".quick");
  for (const [command, words, value, everywhere] of [["search-everywhere", [], "", true], ["recent-files", [], "", false], ["symbol", ["greet"], "@greet", false], ["symbol-tree", ["greet"], "#greet", false]]) {
    await press(command, ...words);
    try {
      await ui.waitFor(() => paletteOpen(), 3000, `${command}'s quick open`);
      assert.equal(input().value, value, command);
      assert.equal(quick().classList.contains("everywhere"), everywhere, `${command} everywhere`);
    } finally {
      await ui.key("Escape");
    }
  }
});

test("Go, Jump marks the places to jump to", async () => {
  await given("ab cd ef|");
  await press("jump");
  assert.equal(editing().editor.host.dataset.jump, "true");
  editing().editor.endJump();
});

test("Go, Mark File marks files, Marked Files lists them, and each Go to Marked File opens its own", async () => {
  const names = [1, 2, 3, 4, 5, 6, 7, 8, 9].map((n) => `marked_${n}.txt`);
  showView("edit");
  for (const name of names) {
    await invoke("file_write", { path: name, text: `${name}\n` });
    await openFile(name);
    await press("mark-file");
  }
  const listed = () => editing().markItems().map((item) => item.label.replace(/^\d+ {2}/, ""));
  try {
    assert.deepEqual(listed().slice(-9), names, "each marked in turn");
    await press("marked-files");
    const menu = await ui.waitFor(() => document.querySelector("body > .menu"), 3000, "the marked files' menu");
    assert.ok(menu.textContent.includes(names[0]));
    closeMenu(false);
    for (const [at, name] of names.entries()) {
      const n = listed().indexOf(name) + 1;
      if (n > 9) {
        continue;
      }
      await press(`mark-${n}`);
      assert.equal(editing().active, name, `mark-${n}`);
      assert.equal(at + 1 <= 9, true);
    }
  } finally {
    for (const name of names) {
      await openFile(name);
      if (listed().includes(name)) {
        await runCommand("mark-file");
      }
    }
  }
  assert.ok(!names.some((name) => listed().includes(name)), "Mark File again lets each go");
});

test("Go, Toggle Bookmark sets and takes off a bookmark on the cursor's line, and Bookmarks lists them", async () => {
  await given("ab\nc|d");
  await press("bookmark");
  assert.ok(bookmarksOf(GIVEN)?.has(1), "the second line bookmarked");
  await press("bookmarks");
  try {
    await ui.waitFor(() => paletteOpen(), 3000, "the bookmarks");
    assert.match(document.querySelector(".quick-input").placeholder, /^Bookmarks/);
  } finally {
    await ui.key("Escape");
  }
  await press("bookmark");
  assert.equal(bookmarksOf(GIVEN), null, "taken off");
});

test("Go, Bridge Key opens Lstar.klq at the key's line", async () => {
  calls.answer("bridge_read", () => ({ klq: { path: "hello.py", keys: { greet: { line: 4 } } } }));
  await runCommand("bridge", ["greet"]);
  await ui.waitFor(() => editing().active === "hello.py" && editing().editor.head().line === 4, 3000, "hello.py at the key's line");
});

test("Go, Gather Lines gathers the cursor's line and Gathered Lines shows and hides them", async () => {
  await given("ab\nc|d");
  await press("gather-lines");
  const kept = tasks();
  assert.ok((kept.tasks[kept.current] ?? []).some((piece) => piece.path === GIVEN && piece.from === 1), JSON.stringify(kept));
  const was = Boolean(boardShown());
  await press("gathered-lines");
  assert.equal(Boolean(boardShown()), !was);
  await press("gathered-lines");
  assert.equal(Boolean(boardShown()), was);
});

test("Go, Next Problem and Previous Problem step through the diagnostics", async () => {
  const view = await given("a|b\ncd\nef");
  // The checks that run as the text rests come first; the diagnostics are set after them.
  await ui.rest(800);
  view.s.diagnostics = [
    { from: { line: 2, col: 0 }, to: { line: 2, col: 1 }, severity: 1, message: "second" },
    { from: { line: 1, col: 1 }, to: { line: 1, col: 2 }, severity: 2, message: "first" },
  ];
  try {
    await press("next-problem");
    assert.equal(marked(), "ab\nc|d\nef");
    await press("next-problem");
    assert.equal(marked(), "ab\ncd\n|ef");
    await press("previous-problem");
    assert.equal(marked(), "ab\nc|d\nef");
  } finally {
    view.s.diagnostics = [];
  }
});

test("Go, Next Change and Previous Change step through the changes since the last commit, each peeked", async () => {
  showView("edit");
  await openFile("hello.py");
  const view = editing().editor;
  view.goTo(0, 0);
  view.change([{ from: { line: 0, col: 0 }, to: { line: 0, col: 0 }, text: "# changed\n" }], null);
  try {
    await ui.waitFor(() => view.s.changes?.hunks?.length, 10000, "the change against the last commit");
    view.goTo(5, 0);
    await press("previous-change");
    assert.equal(view.head().line, 0, "the change before");
    await ui.waitFor(() => document.querySelector(".peek .peek-title")?.textContent.startsWith("Change 1 of"), 3000, "the change peeked");
    view.goTo(5, 0);
    await press("next-change");
    assert.equal(view.head().line, 0, "the first change, after the last");
  } finally {
    closePeek();
    document.querySelector(".peek")?.remove();
    view.change([{ from: { line: 0, col: 0 }, to: { line: 1, col: 0 }, text: "" }], null);
  }
});

test("Edit, Find and Replace open the find bar, and Next Match and Previous Match step through its matches", async () => {
  const view = await given("[xy] ab xy");
  await press("find");
  assert.ok(view.find.shown, "the find bar open");
  assert.equal(view.find.query.value, "xy");
  assert.ok(view.find.replaceRow.hidden, "without the replace row");
  await ui.waitFor(() => view.find.matches.length === 2, 3000, "both matches found");
  await press("next-match");
  assert.equal(marked(), "xy ab [xy]");
  await press("previous-match");
  assert.equal(marked(), "[xy] ab xy");
  await press("replace");
  assert.ok(!view.find.replaceRow.hidden, "the replace row shown");
  view.find.close();
});

["create-file", "create-folder", "create-repository", "create-project", "open-file", "open-folder", "open-repository", "open-workspace", "open-remote", "add-folder", "add-file", "new-window", "open-folder-window"].forEach((command) => COVERED.add(command));
["dev-container", "clone", "keep-template", "trust-tree", "save", "save-all", "print", "close-editor", "close-all", "preferences", "user-css", "reader-menus", "plugins", "new-plugin", "reload-plugins", "environment", "maven-settings", "toolchains", "exit"].forEach((command) => COVERED.add(command));
["undo-history", "cut", "copy", "paste", "compare-clipboard", "macro-record", "macro-play", "macro-keep", "search", "shape-search", "format", "docstring", "docstring-form", "sort-methods", "fill-paragraph", "continuation", "doc-margin"].forEach((command) => COVERED.add(command));
["rename", "extract-variable", "extract-constant", "extract-function", "inline-variable", "change-signature", "move-declaration", "quick-fix", "parameter-info", "quick-doc"].forEach((command) => COVERED.add(command));

const TREE = () => window.__harness.servers.ORIOR_TEST_TREE.replace(/\\/g, "/");
const HOME = () => window.__harness.servers.ORIOR_TEST_HOME.replace(/\\/g, "/");

// The command line's own commands, run in the terminal: whether one ran, or how it failed.
const ran = (command, ...words) => press(command, ...words).then(() => "ran", (error) => String(error));

test("File, Create makes the file or folder the command line names, and asks for one where none is named", async () => {
  await press("create-file", "made_by_command.txt");
  assert.equal(editing().active, "made_by_command.txt", "the file made and opened");
  await press("create-folder", "made_by_command");
  assert.ok(made("folder_create").some((one) => one.args.path === "made_by_command"));
  await press("create-file");
  assert.equal((await sheetShown(".sheet-create")).querySelector("h2").textContent, "Create File");
  await press("create-repository");
  assert.equal((await sheetShown("form")).querySelector("h2").textContent, "Create Repository");
  await press("create-project");
  assert.equal((await sheetShown("h2")).textContent, "Project from Template");
});

test("File, Open opens a file, the tree's own folder, a workspace's folders and a machine's tree, and asks for a repository", async () => {
  await press("open-file", "hello.py");
  assert.equal(editing().active, "hello.py");
  await press("open-folder", TREE());
  await ui.waitFor(() => made("root_set").length, 5000, "the folder opened");
  await invoke("file_write", { path: "made.code-workspace", text: JSON.stringify({ folders: [{ path: "." }] }) });
  await press("open-workspace", `${TREE()}/made.code-workspace`);
  await ui.waitFor(() => made("workspace_read").length, 5000, "the workspace read");
  const opened = [];
  calls.answer("window_open", (args) => opened.push(args));
  await press("open-remote", "wsl:Ubuntu:/tmp");
  assert.equal(opened.at(-1)?.remote, "wsl:Ubuntu:/tmp", "a window opened on the machine's tree");
  await press("open-repository");
  await sheetShown("form");
});

test("File, Add Folder, Add File, New Window and Open Folder in New Window", async () => {
  const opened = [];
  calls.answer("window_open", (args) => opened.push(args));
  await press("add-folder", HOME());
  assert.ok(made("tree_mount").some((one) => one.args.path === HOME()), "the folder mounted beside the tree");
  await invoke("file_write_any", { path: `${HOME()}/added.txt`, text: "added\n" });
  await press("add-file", `${HOME()}/added.txt`);
  await ui.waitFor(() => editing().active?.startsWith("@"), 5000, "the file opened beside the tree");
  await press("new-window");
  assert.deepEqual(opened.at(-1)?.words, [], "a new window");
  await press("open-folder-window", TREE());
  assert.equal(opened.at(-1)?.root, TREE(), "a new window on the folder");
});

test("File, Open in Dev Container, Clone, Keep Tree as Template, Trust This Tree and Exit", async () => {
  calls.answer("devcontainer_line", () => "echo dev-container");
  await runCommand("dev-container");
  await ui.waitFor(() => /dev-container/.test(document.getElementById("term-views").textContent), 10000, "the container's line run in the terminal");
  await runCommand("clone");
  await sheetShown("form");
  const kept = [];
  calls.answer("template_keep", ({ name }) => (kept.push(name), "templates/kept"));
  await press("keep-template", "kept_template");
  assert.deepEqual(kept, ["kept_template"]);
  // A tree that holds code its tools run is asked about; the fixture's holds none, and is told so.
  await press("trust-tree");
  await ui.waitFor(() => !document.getElementById("trust-ask").hidden || told(/nothing to trust/), 3000, "the trust asked or told");
  document.getElementById("trust-ask").hidden = true;
  calls.answer("app_exit", () => null);
  await press("exit");
  assert.equal(made("app_exit").length, 1, "the window asked to end");
  runCommand("toggle");
});

test("File, Save and Save All write the files changed", async () => {
  const view = await given("saved by Save|");
  await press("save");
  assert.equal((await invoke("file_read", { path: GIVEN })).text, "saved by Save");
  await openFile("hello.py");
  const hello = editing().editor;
  hello.change([{ from: { line: 0, col: 0 }, to: { line: 0, col: 0 }, text: "# saved by Save All\n" }], null);
  await given("and this|");
  await press("save-all");
  assert.equal((await invoke("file_read", { path: GIVEN })).text, "and this");
  assert.match((await invoke("file_read", { path: "hello.py" })).text, /^# saved by Save All\n/);
  await openFile("hello.py");
  editing().editor.change([{ from: { line: 0, col: 0 }, to: { line: 1, col: 0 }, text: "" }], null);
  await press("save");
  assert.ok(view);
});

test("File, Print shows the printers to print to", async () => {
  calls.answer("printers_list", () => ({ printers: [{ name: "Test Printer", default: true }], silent: true }));
  await given("printed|");
  await press("print");
  const shown = await sheetShown("select");
  assert.ok([...shown.options].some((one) => one.textContent === "Save as PDF"));
});

test("File, Close Editor and Close All Editors close the tabs", async () => {
  await press("save-all");
  await openFile("hello.py");
  await press("close-editor");
  assert.notEqual(editing().active, "hello.py");
  await press("close-all");
  await ui.waitFor(() => !editing().open, 5000, "every tab closed");
});

test("File, Preferences, Plugins, New Language Plugin, Toolchains and the reader's own files", async () => {
  await press("preferences");
  await sheetShown(".sheet-prefs");
  assert.equal(await ran("user-css"), "ran");
  assert.ok(await invoke("file_read_any", { path: `${HOME()}/user.css` }).then(() => true, () => false), "the command line made user.css in orior's home");
  const revealed = [];
  calls.answer("home_reveal", ({ what }) => revealed.push(what));
  await runCommand("user-css");
  await press("reader-menus");
  assert.deepEqual(revealed, ["user-css", "menus"]);
  assert.equal(await ran("plugins"), "ran");
  await runCommand("plugins");
  await sheetShown(".sheet-plugins");
  assert.equal(await ran("new-plugin", "madelang", "--ext", "madel"), "ran");
  assert.ok(await invoke("file_read_any", { path: `${HOME()}/plugins/madelang/plugin.json` }).then(() => true, () => false), "the plugin made in orior's home");
  await runCommand("new-plugin");
  await sheetShown(".sheet-generator");
  const before = made("plugins_read").length;
  await press("reload-plugins");
  assert.ok(made("plugins_read").length > before, "the plugins read again");
  assert.equal(await ran("toolchains"), "ran");
  await runCommand("toolchains");
  await sheetShown("*");
});

test("File, Tree Environment and Maven Settings", async () => {
  await press("environment", "none");
  assert.ok(made("env_use").some((one) => one.args.kind === null), "the toolchains' own Python used");
  calls.answer("maven_settings", () => [{ group: "pom", label: "Java Release", value: "17", settable: true, key: "release", file: "pom.xml" }]);
  await press("maven-settings");
  const shown = await sheetShown(".sheet-maven");
  assert.ok(shown.textContent.includes("Java Release"));
});

test("Edit, Undo History shows the text's history", async () => {
  await given("a|");
  await press("undo-history");
  assert.ok(paneOpen("undo"));
});

test("Edit, Cut, Copy and Paste take the selection and put the clipboard's text", async () => {
  const clipped = [];
  calls.answer("clip_write", ({ text }) => clipped.push(text));
  await given("[ab]c");
  await press("copy");
  assert.equal(editing().editor.clip?.text, "ab", "copied");
  await given("[ab]c");
  await press("cut");
  assert.equal(editing().editor.doc.text(), "c", "cut");
  assert.deepEqual(clipped, ["ab", "ab"], "each put on the clipboard");
  calls.answer("clip_read", () => "XY");
  await given("c|");
  await press("paste");
  assert.equal(marked(), "cXY|");
  await press("compare-clipboard");
  await ui.waitFor(() => document.querySelector("#mode-edit .desk .diff-button"), 5000, "the file beside the clipboard");
  closeDiff();
});

test("Edit, Macros records keys, plays them back and keeps them by name", async () => {
  const view = await given("|");
  await press("macro-record");
  assert.ok(recording(), "recording");
  view.focus();
  await ui.type("z");
  await press("macro-record");
  assert.ok(!recording() && lastMacro()?.length, "a macro recorded");
  await press("macro-play", "2");
  assert.equal(view.doc.text(), "zzz", "played twice more");
  await press("macro-keep", "made macro");
  try {
    assert.ok(keptMacros().some((one) => one.name === "made macro"));
  } finally {
    forgetMacro("made macro");
  }
});

test("Edit, Find in Files from the command line and from the menu", async () => {
  assert.equal(await ran("search", "greet"), "ran");
  await runCommand("search", ["greet"]);
  assert.ok(paneOpen("search"));
});

test("Edit, Format Document formats the file with its language's formatter", async () => {
  // The command line's own format takes the files to format; the menu's formats the tab shown.
  assert.match(await ran("format"), /exit (code|status): 2/, "the command line asks for the files");
  calls.answer("format_languages", () => ["python"]);
  calls.answer("format_text", () => "x = 1\n");
  await given("x=1\n|");
  await runCommand("format");
  await ui.waitFor(() => editing().editor.doc.text() === "x = 1\n", 5000, "the text formatted");
});

test("Edit, Docstring writes one in the form chosen, and Sort Methods sorts a class's methods", async () => {
  const was = editing().docstringForm();
  await press("docstring-form", "numpy");
  assert.equal(editing().docstringForm(), "numpy");
  await press("docstring-form", "google");
  try {
    await given("def add(a, b):\n    return a| + b\n");
    await press("docstring");
    await ui.waitFor(() => editing().editor.doc.text().includes('"""'), 5000, "the docstring");
    assert.match(editing().editor.doc.text(), /Args:/);
    await given("class C:\n    def b(self):\n        pass|\n\n    def a(self):\n        pass\n");
    await press("sort-methods");
    await ui.waitFor(() => editing().editor.doc.text().indexOf("def a") < editing().editor.doc.text().indexOf("def b"), 5000, "the methods sorted");
  } finally {
    editing().setDocstringForm(was);
  }
});

test("Edit, Fill Paragraph fills a paragraph to the margin, and Continuation Indent and Documentation Margin take their columns", async () => {
  const words = Array.from({ length: 40 }, (_, at) => `word${at}`).join(" ");
  await given(`# ${words}|\n`);
  await press("fill-paragraph");
  await ui.waitFor(() => editing().editor.doc.count > 3, 5000, "the paragraph filled");
  const width = editing().docMargin() ?? editing().editor.s.margin ?? 79;
  assert.ok(editing().editor.doc.lines.every((line) => line.length <= width), `filled to ${width}: ${editing().editor.doc.text()}`);
  assert.equal(editing().editor.doc.text().replace(/\s*#\s*/g, " ").trim(), words, "every word kept, in order");
  const continued = editing().continuation();
  const margin = editing().docMargin();
  try {
    await press("continuation", "4", "8", "2");
    assert.deepEqual(editing().continuation(), { call: 4, declaration: 8, expression: 2 });
    await press("doc-margin", "72");
    assert.equal(editing().docMargin(), 72);
    await press("doc-margin", "none");
    assert.equal(editing().docMargin(), null);
  } finally {
    editing().setContinuation(continued);
    editing().setDocMargin(margin);
  }
});

test("Edit, Extract Variable, Extract Constant, Inline Variable and Extract Function", async () => {
  await given("def f():\n    return [a + b] * 2\n");
  await press("extract-variable");
  assert.equal(editing().editor.doc.text(), "def f():\n    value = a + b\n    return value * 2\n");
  await given("x = [3]\ny = 4");
  await press("extract-constant");
  assert.equal(editing().editor.doc.text(), "VALUE = 3\n\nx = VALUE\ny = 4");
  await given("a = 1\nb = |a + 2");
  await press("inline-variable");
  assert.equal(editing().editor.doc.text(), "b = 1 + 2");
  await given("def f(a):\n    [b = a * 2\n    c = b + 1]\n    return c\n");
  await press("extract-function");
  await ui.waitFor(() => (editing().editor.doc.text().match(/\bdef /g) ?? []).length === 2, 5000, "a function made of the lines");
});

test("Edit, Change Signature, Move and Search Structurally ask in their sheets", async () => {
  await given("def f|(a, b):\n    return a + b\n\nf(1, 2)\n");
  await press("change-signature");
  await sheetShown("*");
  await given("def f|(a, b):\n    return a + b\n");
  await press("move-declaration");
  await sheetShown("*");
  await given("x = 1|\n");
  await press("shape-search");
  await sheetShown("*");
});

test("Edit, Rename, Quick Fix, Parameter Info and Quick Documentation ask the language server, here clangd", async () => {
  await invoke("file_write", { path: "served.c", text: "int add(int a, int b) { return a + b; }\n\nint main(void) {\n  return add(1, 2)\n}\n" });
  showView("edit");
  await openFile("served.c");
  await ui.waitFor(() => editing().active === "served.c" && editing().editor?.s, 5000, "served.c open");
  const view = editing().editor;
  await ui.waitFor(async () => {
    view.goTo(3, 16);
    await press("parameter-info");
    return document.querySelector(".ed-signature:not([hidden])")?.textContent.includes("add");
  }, 30000, "clangd to give the signature");
  view.goTo(0, 5);
  await press("quick-doc");
  await ui.waitFor(() => !view.hover.el.hidden && /add/.test(view.hover.el.textContent), 10000, "the documentation of add");
  view.hover.el.hidden = true;
  view.goTo(0, 5);
  await press("rename");
  const field = await ui.waitFor(() => document.querySelector(".ed-rename"), 10000, "the rename field");
  assert.equal(field.value, "add");
  field.dispatchEvent(new KeyboardEvent("keydown", { key: "Escape", bubbles: true }));
  // a + b selected in add: clangd offers to put it in a variable of its own.
  await ui.waitFor(async () => {
    view.select([{ anchor: { line: 0, col: 31 }, head: { line: 0, col: 36 }, goal: null }]);
    await press("quick-fix");
    return document.querySelector("body > .menu");
  }, 15000, "clangd's refactorings of the selection");
  assert.match(document.querySelector("body > .menu").textContent, /xtract/, "an extraction offered");
  closeMenu(false);
}, { timeout: 90000 });

["start", "stop", "list", "show", "run-file", "validate", "update-snapshots", "run-statement", "run-sql-file", "send-request", "copy-as-curl", "private-environment"].forEach((command) => COVERED.add(command));
["tests", "run-tests", "run-failed-tests", "cover-tests", "stop-tests", "profile", "profile-remote", "stop-profile", "print-here", "breakpoint"].forEach((command) => COVERED.add(command));
["debug", "continue", "step-over", "step-into", "step-out", "stop-debug", "pause", "restart-debug", "record", "step-back", "reverse-continue", "debug-mixed"].forEach((command) => COVERED.add(command));
["attach-process", "attach-address", "stepping", "export-breakpoints", "import-breakpoints", "show-memory", "watch-memory", "machine-code"].forEach((command) => COVERED.add(command));
["commit-view", "review-structure", "push", "pull", "branches", "new-branch", "stash", "stashes", "cherry-pick", "conflicts"].forEach((command) => COVERED.add(command));

const QUICK = "build/src/build_quick.sh";
const SLOW = "build/src/build_slow.sh";

test("Run, Start and Stop run the job chosen, and the command line lists, shows and starts the tree's jobs", async () => {
  await invoke("file_write", { path: "src/build_quick.sh", text: "#!/usr/bin/env bash\necho quick\n" });
  await invoke("file_write", { path: "src/build_slow.sh", text: "#!/usr/bin/env bash\nsleep 30\n" });
  await loadRun();
  assert.equal(await ran("list"), "ran");
  assert.equal(await ran("show", QUICK), "ran");
  assert.equal(await ran("start", QUICK), "ran", "the job run in the terminal, and ended well");
  await runCommand("list", ["slow"]);
  assert.equal(document.body.dataset.view, "run");
  assert.equal(document.getElementById("job-filter").value, "slow");
  showJob(SLOW);
  assert.equal(chosenJob()?.id, SLOW);
  const ends = [];
  await listen("run-end", ({ payload }) => ends.push(payload));
  await runCommand("start");
  await ui.waitFor(() => chosenLive(), 10000, "the job running");
  await press("stop");
  await ui.waitFor(() => !chosenLive(), 10000, "the job stopped").catch((error) => {
    throw new Error(`${error.message}; asked to stop: ${JSON.stringify(made("job_stop").map((one) => one.args))}; started: ${JSON.stringify(made("job_start").map((one) => one.args.job))}; ended: ${JSON.stringify(ends)}; the console: ${document.getElementById("lines")?.textContent.slice(0, 400)}; the window said: ${JSON.stringify([...noticesSaid(), ...noticesHeld()].slice(-3).map((one) => one.text))}`);
  });
}, { timeout: 60000 });

test("Run, Run File runs the file shown, from the window and from the command line", async () => {
  assert.equal(await ran("run-file", `${TREE()}/hello.py`), "ran", "Python ran hello.py");
  showView("edit");
  await openFile("hello.py");
  const before = made("run_file_line").length;
  await runCommand("run-file");
  await ui.waitFor(() => made("run_file_line").length > before, 5000, "the file's run asked for");
  await ui.waitFor(() => /hello, orior/.test(document.getElementById("term-views").textContent), 15000, "its output in the terminal");
}, { timeout: 60000 });

test("Run, Validate says where no tool plugin validates the file, from the window and the command line", async () => {
  assert.match(await ran("validate", `${TREE()}/hello.py`), /ended/, "the command line refuses a file no tool validates");
  showView("edit");
  await openFile("hello.py");
  await runCommand("validate");
  assert.ok(told(/No tool plugin validates Python/));
});

test("Run, Update Snapshots runs its line in the terminal", async () => {
  calls.answer("snapshots_line", () => ["", "echo snapshots-updated"]);
  await press("update-snapshots");
  await ui.waitFor(() => /snapshots-updated/.test(document.getElementById("term-views").textContent), 10000, "the line run");
});

test("Run, Run Statement and Run File of a SQL file read its statements", async () => {
  showView("edit");
  await openFile("queries.sql");
  editing().editor.goTo(0, 1);
  await press("run-statement");
  assert.ok(made("db_statement_at").length, "the statement at the cursor read");
  await press("run-sql-file");
  assert.ok(made("db_statements").length, "every statement read");
});

test("Run, Send Request, Copy as curl and Private Environment File on a file of requests", async () => {
  await invoke("file_write", { path: "asked.http", text: "GET http://127.0.0.1:9/asked\n" });
  showView("edit");
  await openFile("asked.http");
  editing().editor.goTo(0, 0);
  calls.answer("requests_send", () => ({ status: 200, reason: "OK", headers: [], body: "", ms: 1, size: 0 }));
  await press("send-request");
  assert.ok(made("requests_send").some((one) => one.args.path === "asked.http"), "the request sent");
  const copied = [];
  calls.answer("clip_write", ({ text }) => copied.push(text));
  await press("copy-as-curl");
  await ui.waitFor(() => copied.length, 5000, "the curl line copied");
  assert.match(copied[0], /^curl\b[\s\S]*127\.0\.0\.1:9\/asked/);
  await openFile("asked.http");
  await press("private-environment");
  await ui.waitFor(() => made("requests_private").length, 5000, "the private environment asked for");
});

test("Run, Find Tests lists the tree's tests, Run Tests runs them, Run Failed Tests the failed, and Stop Tests stops them", async () => {
  await invoke("file_write", { path: "test_menus.py", text: "def test_passes():\n    assert True\n\n\ndef test_fails():\n    assert False\n" });
  // The tree's index takes in a file as its watch tells of it.
  await ui.waitFor(async () => (await invoke("tests_found")).some((one) => one.file === "test_menus.py"), 10000, "the file's tests found");
  assert.equal(await ran("tests"), "ran");
  await runCommand("tests");
  try {
    await ui.waitFor(() => paletteOpen() && /tests?: type to filter/.test(document.querySelector(".quick-input").placeholder), 5000, "the tests listed");
  } finally {
    await ui.key("Escape");
  }
  await press("run-tests");
  await ui.waitFor(() => testsOf("test_menus.py")?.get(0) === "passed" && testsOf("test_menus.py")?.get(4) === "failed", 60000, "each test's result").catch((error) => {
    throw new Error(`${error.message}; the results: ${JSON.stringify([...(testsOf("test_menus.py") ?? [])])}; run: ${JSON.stringify(made("tests_run").map((one) => one.args))}; the window said: ${JSON.stringify([...noticesSaid(), ...noticesHeld()].slice(-3).map((one) => one.text))}`);
  });
  const asked = made("tests_run").length;
  await press("run-failed-tests");
  await ui.waitFor(() => made("tests_run").length > asked, 5000, "the failed test run again");
  assert.deepEqual(made("tests_run").at(-1).args.given, ["test_menus.py::test_fails"]);
  await press("cover-tests");
  await ui.waitFor(() => made("tests_run").at(-1).args.cover === true, 5000, "the tests run for their coverage");
  await press("stop-tests");
  assert.ok(made("tests_stop").length);
}, { timeout: 120000 });

test("Run, Profile profiles the file shown, Stop Profiling stops it, and Profile on a Machine profiles there", async () => {
  await invoke("file_write", { path: "profiled.py", text: "import time\nfor _ in range(200):\n    time.sleep(0.05)\n" });
  showView("edit");
  await openFile("profiled.py");
  await press("profile");
  assert.ok(!document.getElementById("profile").hidden, "the profile shown");
  assert.ok(made("profile_start").some((one) => one.args.path === "profiled.py" && !one.args.remote));
  await press("stop-profile");
  assert.ok(made("profile_stop").length);
  calls.answer("profile_start", () => null);
  await press("profile-remote", "me@machine:/tree");
  assert.ok(made("profile_start").some((one) => one.args.remote === "me@machine:/tree"));
  runCommand("profile-view");
});

test("Run, Toggle Breakpoint and Print Here set the cursor's line to stop or to print", async () => {
  await given("x = 1\ny = |x + 1\n");
  await press("breakpoint");
  assert.ok(breakpointsOf(GIVEN)?.has(1), "a breakpoint on the second line");
  await press("breakpoint");
  assert.equal(breakpointsOf(GIVEN), null);
  await press("print-here");
  assert.ok(breakpointsOf(GIVEN)?.get(1)?.log, "a breakpoint that prints");
  await press("breakpoint");
});

const DEBUGGED = "debugged.py";

// Ends every debugging session, after a test of the debugger.
async function endSessions() {
  for (let left = 5; debugging() && left > 0; left -= 1) {
    await runCommand("stop-debug");
    await ui.rest(200);
  }
}

test("Run, Debug stops at a breakpoint, and Step Into, Step Out, Step Over and Continue go on from it", async () => {
  await invoke("file_write", { path: DEBUGGED, text: "def inner():\n    x = 1\n    return x\n\n\ny = inner()\nprint(y)\n" });
  showView("edit");
  await openFile(DEBUGGED);
  editing().editor.goTo(5, 0);
  await press("breakpoint");
  const at = () => pausedLineOf(DEBUGGED);
  await press("debug");
  await ui.waitFor(() => isPaused() && at() === 5, 30000, "stopped at the breakpoint");
  await press("step-into");
  await ui.waitFor(() => at() === 1, 10000, "inside inner");
  await press("step-out");
  await ui.waitFor(() => at() === 5 || at() === 6, 10000, "back in the caller");
  if (at() === 5) {
    await press("step-over");
    await ui.waitFor(() => at() === 6, 10000, "the next line");
  }
  await press("continue");
  await ui.waitFor(() => !debugging(), 15000, "the program run to its end").finally(endSessions);
}, { timeout: 90000 });

test("Run, Pause stops a running program, Restart starts it again, and Stop ends it", async () => {
  await invoke("file_write", { path: "looped.py", text: "import time\nwhile True:\n    time.sleep(0.05)\n" });
  showView("edit");
  await openFile("looped.py");
  try {
    await press("debug");
    // Pause takes the thread the program runs on, which the debugger tells once the program runs.
    await ui.waitFor(async () => {
      await press("pause");
      return isPaused();
    }, 30000, "the program paused");
    await press("restart-debug");
    await ui.waitFor(() => debugging() && !isPaused(), 30000, "the program running again");
    await press("stop-debug");
    await ui.waitFor(() => !debugging(), 10000, "the program ended");
  } finally {
    await endSessions();
  }
}, { timeout: 90000 });

test("Run, Show Bytecode reads a Python line's bytecode, and Memory, Watch Memory and Instructions read a stopped C program", async () => {
  await endSessions();
  await given("a = 1 + 2|\n");
  await press("machine-code");
  assert.match((await sheetShown("h2")).textContent, /^Bytecode of /);
  await invoke("file_write", { path: "mem.c", text: "#include <stdio.h>\nint main(void)\n{\n    volatile int count = 0x11223344;\n    int other = 5;\n    other += 1;\n    count = 7;\n    printf(\"%d %d\\n\", count, other);\n    return 0;\n}\n" });
  showView("edit");
  await openFile("mem.c");
  editing().editor.goTo(4, 0);
  await press("breakpoint");
  try {
    await press("debug");
    await ui.waitFor(() => isPaused() && pausedLineOf("mem.c") === 4, 60000, "the C program stopped").catch((error) => {
      throw new Error(`${error.message}; the debugger said: ${document.getElementById("debug-said").textContent}; ${document.getElementById("debug-out").textContent.slice(-600)}`);
    });
    const [session] = (await invoke("debug_sessions")).slice(-1);
    let frames = [];
    for (const thread of await invoke("debug_threads", { session: session.id })) {
      frames = await invoke("debug_stack", { session: session.id, thread: thread.id }).catch(() => []);
      if (frames[0]?.path?.endsWith("mem.c")) {
        break;
      }
    }
    const scopes = await invoke("debug_scopes", { session: session.id, frame: frames[0].id });
    const count = (await invoke("debug_variables", { session: session.id, reference: scopes[0].reference })).find((one) => one.name === "count");
    // Where the debugger gives the variable no memory reference, its address read from `&count` is one.
    let reference = count.memory;
    if (!reference) {
      const found = await invoke("debug_evaluate", { session: session.id, expression: "&count", frame: frames[0].id, context: "watch" });
      reference = found.memory ?? found.value.match(/0x[0-9a-f]+/i)?.[0];
    }
    await press("show-memory", reference);
    const shown = await ui.waitFor(() => [...document.querySelectorAll("dialog[open] pre.debug-memory")].find((pre) => /44 33 22 11/.test(pre.textContent)), 10000, "count's bytes").catch((error) => {
      throw new Error(`${error.message}; the reference ${reference}; the sheet shows ${[...document.querySelectorAll("dialog[open] pre")].map((pre) => pre.textContent.slice(0, 200)).join(" | ")}`);
    });
    assert.ok(shown);
    closeSheets();
    await press("watch-memory", "count");
    await ui.waitFor(() => told(/stops where .*count.* changes|count/), 10000, "the watch set");
    await press("machine-code");
    await ui.waitFor(() => document.querySelector("body > .menu"), 5000, "the frame's menu of its instructions");
    closeMenu(false);
    await press("stop-debug");
    await ui.waitFor(() => !debugging(), 15000, "the session ended");
  } finally {
    await endSessions();
    editing().editor.goTo(4, 0);
    if (breakpointsOf("mem.c")) {
      await runCommand("breakpoint");
    }
  }
}, { timeout: 120000 });

test("Run, Record and Debug records the run, and Step Back and Reverse Continue go back through it", async () => {
  showView("edit");
  await openFile(DEBUGGED);
  await press("record");
  await ui.waitFor(() => canStepBack(), 60000, "the run recorded and stopped").catch(async (error) => {
    await endSessions();
    throw error;
  });
  const from = pausedLineOf(DEBUGGED);
  await press("step-back");
  await ui.waitFor(() => isPaused() && pausedLineOf(DEBUGGED) !== from, 10000, "a step back");
  await press("reverse-continue");
  await ui.waitFor(() => isPaused(), 10000, "stopped going back");
  await press("stop-debug");
  await ui.waitFor(() => !debugging(), 10000, "the session ended").finally(endSessions);
}, { timeout: 120000 });

test("Run, Debug File with Its C starts the Python stopped before its first line", async () => {
  showView("edit");
  await openFile(DEBUGGED);
  await press("debug-mixed");
  assert.ok(made("debug_start").some((one) => one.args.start.path === DEBUGGED && one.args.options.entry === true), "started at its entry");
  await ui.waitFor(() => debugging(), 30000, "the session");
  await press("stop-debug");
  await ui.waitFor(() => !debugging(), 15000, "the sessions ended").finally(endSessions);
}, { timeout: 90000 });

test("Run, Attach to Process lists the processes, and Attach to Address debugs the program listening there", async () => {
  await press("attach-process");
  try {
    await ui.waitFor(() => paletteOpen() && /^Attach to a process/.test(document.querySelector(".quick-input").placeholder), 10000, "the processes listed");
  } finally {
    await ui.key("Escape");
  }
  await press("attach-address", "127.0.0.1:notaport");
  assert.ok(told(/is no address/), "an address with no port refused");
  await invoke("file_write", { path: "listened.py", text: "import time\nwhile True:\n    time.sleep(0.05)\n" });
  await ui.run("python", ["-m", "debugpy", "--listen", "127.0.0.1:5694", "--wait-for-client", `${TREE()}/listened.py`], { wait: false });
  await ui.waitFor(async () => {
    await press("attach-address", "127.0.0.1:5694");
    return debugging();
  }, 30000, "the program at the address debugged");
  await press("stop-debug");
  await ui.waitFor(() => !debugging(), 10000, "the session ended").finally(endSessions);
}, { timeout: 90000 });

test("Run, Stepping asks how steps go, and Export and Import Breakpoints write and take in a file of them", async () => {
  await press("stepping");
  assert.equal((await sheetShown("h2")).textContent, "Stepping");
  const file = `${HOME()}/breakpoints.json`;
  calls.answer("pick", () => file);
  await given("a = 1|\n");
  await press("breakpoint");
  await press("export-breakpoints");
  await ui.waitFor(async () => (await invoke("file_read_any", { path: file }).catch(() => "")).includes(GIVEN), 5000, "the breakpoints written");
  await press("breakpoint");
  assert.equal(breakpointsOf(GIVEN), null);
  await press("import-breakpoints");
  await ui.waitFor(() => breakpointsOf(GIVEN)?.has(0), 5000, "the breakpoints taken in");
  await press("breakpoint");
});

test("Git, Commit, Review Structure, Push and Pull", async () => {
  await press("commit-view");
  assert.ok(paneOpen("changes"));
  await press("review-structure");
  await ui.waitFor(() => made("review_structure").length, 5000, "the change's structure read");
  await ui.waitFor(() => changeShown(), 5000, "the change's structure shown");
  closeChange();
  calls.answer("git_push", () => "pushed by the test");
  calls.answer("git_pull", () => "pulled by the test");
  await press("push");
  await ui.waitFor(() => told(/pushed by the test/), 5000, "the push said");
  await press("pull");
  await ui.waitFor(() => told(/pulled by the test/), 5000, "the pull said");
});

test("Git, Branches lists them and New Branch makes one", async () => {
  const repo = editing().repo();
  await press("branches");
  const menu = await ui.waitFor(() => document.querySelector("body > .menu"), 5000, "the branches");
  assert.ok(menu.textContent.includes("New Branch…"));
  closeMenu(false);
  await press("new-branch", "made-by-command");
  await ui.waitFor(async () => (await invoke("git_branches", { repo })).some((one) => one.name === "made-by-command"), 10000, "the branch made");
  await invoke("git_branch", { act: "delete", name: "made-by-command", repo }).catch(() => {});
});

test("Git, Stash Changes puts the changes aside and Stashes lists them", async () => {
  const repo = editing().repo();
  const before = (await invoke("file_read", { path: "README.md" })).text;
  await invoke("file_write", { path: "README.md", text: `${before}stashed by the test\n` });
  await press("stash", "stashed by the test");
  await ui.waitFor(async () => !(await invoke("file_read", { path: "README.md" })).text.includes("stashed by the test"), 10000, "the change put aside");
  await press("stashes");
  const menu = await ui.waitFor(() => document.querySelector("body > .menu"), 5000, "the stashes");
  assert.ok(menu.textContent.includes("stashed by the test"));
  closeMenu(false);
  // The stash holds the tree's other changes too: each is given back, and README as it was.
  const [stash] = await invoke("git_stashes", { repo });
  await invoke("git_stash", { act: "pop", name: stash.name, repo });
  await invoke("file_write", { path: "README.md", text: before });
});

test("Git, Cherry-Pick applies the commit named, and Resolve Conflicts says where none is in conflict", async () => {
  calls.answer("git_cherry_pick", () => "picked by the test");
  await press("cherry-pick", "abc1234");
  assert.ok(made("git_cherry_pick").some((one) => one.args.id === "abc1234"));
  await press("conflicts");
  assert.ok(told(/No file is in conflict/));
});

// What the tests above made in the tree, each taken out by the last test.
const MADE = [
  GIVEN, "peek.c", "served.c", "src/hello*.py", "src/made_cut.txt", "made_cut.txt", "notebook_cells.py", "history_file.txt", "debugged.py", "looped.py", "listened.py", "mem.*", "profiled.py", "test_menus.py", "asked.http", "http-client.private.env.json", ".gitignore",
  "made_by_command.txt", "made_by_command", "made.code-workspace", "marked_*.txt", "src/build_quick.sh", "src/build_slow.sh", "__pycache__", ".pytest_cache", "build",
];

// The window's explorer as the file found it, and nothing over the window: no board of gathered
// lines, diff, review of a change, peek, sheet, menu, terminal or debugger.
function asFound() {
  closeMenu(false);
  closeSheets();
  closeBoard();
  closeDiff();
  closeChange();
  closePeek();
  toggleTerminal(false);
  toggleDebugPanel(false);
  showGroup(FOUND.group);
  putPanes(FOUND.panes);
}

// Every item of a menu shown, its own items' items among them: each with the command it runs.
const itemsOfMenu = () => [...document.querySelectorAll("body > .menu .menu-item")].map((button) => button.item).filter(Boolean);
const itemsUnder = (items) => items.flatMap((item) => (item === "-" ? [] : item.items ? itemsUnder(typeof item.items === "function" ? [] : item.items) : [item]));

// The items of the menu a right click on `target` opens.
async function menuOver(find) {
  closeMenu(false);
  find().scrollIntoView({ block: "center" });
  await ui.frame();
  // Found again, and its menu asked for at once, as a right click asks: a list draws its rows again
  // as it scrolls or changes.
  const target = find();
  const { left, top, width, height } = target.getBoundingClientRect();
  target.dispatchEvent(new MouseEvent("contextmenu", { bubbles: true, cancelable: true, button: 2, clientX: left + Math.min(20, width / 2), clientY: top + height / 2 }));
  await ui.waitFor(() => document.querySelector("body > .menu"), 3000, "the menu");
  const first = document.querySelector("body > .menu .menu-item")?.item;
  const all = [...document.querySelectorAll("body > .menu > .menu-item")].map((button) => button.item);
  closeMenu(false);
  return first ? all : [];
}

["file-open", "file-remove", "file-names", "file-open-window", "file-cut", "file-copy", "file-paste", "file-copy-path", "file-terminal", "file-select-compare", "file-compare"].forEach((command) => COVERED.add(command));
["tab-close", "tab-close-others", "tab-split-right", "tab-split-down", "tab-new-window", "tab-group", "tab-ungroup", "tab-mark", "tab-open-as"].forEach((command) => COVERED.add(command));

const rowOf = (path) => [...document.querySelectorAll("#files .node")].find((row) => row.dataset.key === path);
const tabOfName = (path) => [...document.querySelectorAll("#tabs .tab")].find((tab) => tab.title === path);

test("every item of the tree's menus, the tabs' and the editor's runs a command the command line runs", async () => {
  asFound();
  showView("edit");
  togglePane(true, { take: false, pin: true });
  showPane("folder");
  await ui.waitFor(() => rowOf("hello.py") && rowOf("src") && rowOf("bundle.zip"), 10000, "the tree's rows");
  await openFile("hello.py");
  const named = new Set(allCommands().map((item) => item.command));
  const wrong = [];
  // Each found as its menu opens: the tree draws its rows again as it changes.
  for (const [where, target] of [["a file", () => rowOf("hello.py")], ["a folder", () => rowOf("src")], ["an archive", () => rowOf("bundle.zip")], ["a tab", () => tabOfName("hello.py")], ["the editor", () => editing().editor.host]]) {
    const items = itemsUnder(await menuOver(target));
    assert.ok(items.length, `${where} has a menu`);
    for (const item of items) {
      if (!item.command || !named.has(item.command) || !hasCommand(item.command)) {
        wrong.push(`${where}: ${item.label} runs ${item.command ?? "no command"}`);
      }
    }
  }
  assert.deepEqual(wrong, []);
});

test("the tree's file menu, from the command line: open, copy, cut and paste, the path, the terminal and comparing", async () => {
  asFound();
  showView("edit");
  togglePane(true, { take: false, pin: true });
  showPane("folder");
  await ui.waitFor(() => rowOf("src"), 10000, "the tree");
  const wasOpen = rowOf("src").getAttribute("aria-expanded") === "true";
  await press("file-open", "src");
  await ui.waitFor(() => (rowOf("src").getAttribute("aria-expanded") === "true") !== wasOpen, 3000, "the folder opened or closed");
  await press("file-open", "src");
  await press("file-open", "README.md");
  assert.equal(editing().active, "README.md");
  const copied = [];
  calls.answer("clip_write", ({ text }) => copied.push(text));
  await press("file-copy-path", "hello.py");
  assert.deepEqual(copied, ["hello.py"]);
  await press("file-copy", "hello.py");
  await press("file-paste", "src");
  await ui.waitFor(async () => (await invoke("tree_list", { dir: "src" })).some((entry) => /^hello.*\.py$/.test(entry.name)), 5000, "hello.py pasted into src");
  await invoke("file_write", { path: "made_cut.txt", text: "cut\n" });
  await press("file-cut", "made_cut.txt");
  await press("file-paste", "src");
  await ui.waitFor(async () => (await invoke("tree_list", { dir: "src" })).some((entry) => entry.name === "made_cut.txt") && !(await invoke("tree_list", { dir: "" })).some((entry) => entry.name === "made_cut.txt"), 5000, "made_cut.txt moved into src");
  await press("file-terminal", "src");
  await ui.waitFor(() => /src/.test(document.getElementById("term-views").textContent), 10000, "the terminal in src");
  toggleTerminal(false);
  await press("file-select-compare", "hello.py");
  await press("file-compare", "README.md");
  await ui.waitFor(() => document.querySelector("#mode-edit .desk .diff-button"), 5000, "hello.py beside README.md");
  closeDiff();
  calls.answer("view_open", () => null);
  await press("file-open-window", "README.md");
  assert.ok(made("view_open").some((one) => one.args.path === "README.md"), "the file asked to open in a window of its own");
}, { timeout: 60000 });

test("the tree's file menu, from the command line: an archive's names and a folder let go", async () => {
  await press("file-names", "bundle.zip", "windows-1252");
  assert.equal(JSON.parse(localStorage.getItem("orior.archive.names"))["bundle.zip"], "windows-1252");
  await press("file-names", "bundle.zip");
  assert.equal(JSON.parse(localStorage.getItem("orior.archive.names"))["bundle.zip"], undefined, "as the archive says again");
  const name = await invoke("tree_mount", { path: HOME() });
  assert.ok((await invoke("tree_mounts")).some((one) => one.name === name));
  await press("file-remove", name);
  assert.ok(!(await invoke("tree_mounts")).some((one) => one.name === name), "the folder let go");
});

test("a tab's menu, from the command line: close, close others, split, a new window, groups, marks and a notebook", async () => {
  // A tab with changes not saved asks before it closes; every one is saved first.
  asFound();
  await press("save-all");
  showView("edit");
  await openFile("hello.py");
  await openFile("README.md");
  await press("tab-close", "hello.py");
  assert.ok(!tabOfName("hello.py"), "hello.py's tab closed");
  await openFile("hello.py");
  await press("tab-close-others", "hello.py");
  assert.deepEqual([...document.querySelectorAll("#tabs .tab")].map((tab) => tab.title), ["hello.py"]);
  const desk = document.querySelector("#mode-edit .desk");
  await press("tab-split-right", "hello.py");
  assert.equal(desk.dataset.split, "right");
  await press("unsplit");
  await press("tab-split-down", "hello.py");
  assert.equal(desk.dataset.split, "down");
  await press("unsplit");
  await press("tab-group", "hello.py", "Made Group");
  await ui.waitFor(() => [...document.querySelectorAll(".tab-group-name")].some((one) => one.textContent === "Made Group"), 3000, "the group");
  await press("tab-ungroup", "hello.py");
  assert.ok(![...document.querySelectorAll(".tab-group-name")].some((one) => one.textContent === "Made Group"), "the group gone");
  await press("tab-mark", "hello.py");
  assert.ok(editing().markItems().some((item) => item.label.endsWith("hello.py")), "marked");
  await press("tab-mark", "hello.py");
  assert.ok(!editing().markItems().some((item) => item.label?.endsWith("hello.py")), "and let go");
  await invoke("file_write", { path: "notebook_cells.py", text: "# %%\nx = 1\n\n# %%\nprint(x)\n" });
  await openFile("notebook_cells.py");
  await press("tab-open-as", "notebook_cells.py", "notebook");
  await ui.waitFor(async () => (await menuOver(() => tabOfName("notebook_cells.py"))).some((item) => item.label === "Open as Text"), 5000, "the file open as a notebook");
  await press("tab-open-as", "notebook_cells.py", "text");
  await ui.waitFor(async () => (await menuOver(() => tabOfName("notebook_cells.py"))).some((item) => item.label === "Open as Notebook"), 5000, "and as its text again");
  const opened = [];
  calls.answer("window_open", (args) => opened.push(args.words));
  await press("tab-new-window", "hello.py");
  assert.deepEqual(opened.at(-1), ["file", "open-file", "hello.py"]);
  await ui.waitFor(() => !tabOfName("hello.py"), 3000, "the tab gone to the new window");
}, { timeout: 60000 });

["pane-show", "explorer-refresh", "explorer-collapse", "outline-pin"].forEach((command) => COVERED.add(command));
["commit-open", "commit-compare", "commit-copy-id", "commit-copy-message", "commit-files", "commit-file-changes", "history-difference", "history-revert"].forEach((command) => COVERED.add(command));
["comment-open", "comment-changes", "comment-resolve", "comment-delete", "comment-copy"].forEach((command) => COVERED.add(command));

test("the explorer's menu, from the command line: a pane shown and hidden, the tree read again, its folders closed, and the Outline pinned", async () => {
  asFound();
  showView("edit");
  togglePane(true, { take: false, pin: true });
  showPane("folder");
  await press("pane-show", "todo", "on");
  assert.ok(panesNow().todo.shown, "TODO shown");
  await press("pane-show", "todo", "off");
  assert.ok(!panesNow().todo.shown, "and hidden");
  const read = made("tree_list").length;
  await press("explorer-refresh");
  await ui.waitFor(() => made("tree_list").length > read, 5000, "the tree read again");
  if (rowOf("src")?.getAttribute("aria-expanded") !== "true") {
    await press("file-open", "src");
  }
  await ui.waitFor(() => rowOf("src")?.getAttribute("aria-expanded") === "true", 3000, "src open");
  await press("explorer-collapse");
  await ui.waitFor(() => rowOf("src")?.getAttribute("aria-expanded") === "false", 3000, "src closed");
  await press("outline-pin", "hello.py");
  assert.equal(outlinePinned(), "hello.py");
  await press("outline-pin", "none");
  assert.equal(outlinePinned(), null);
  asFound();
});

test("the commits' menus, from the command line: a revision opened, compared, its id and message copied, its files and changes shown", async () => {
  asFound();
  const tree = TREE();
  const git = (...words) => ui.run("git", ["-C", tree, "-c", "user.name=orior", "-c", "user.email=orior@localhost", ...words]);
  const before = (await invoke("file_read", { path: "hello.py" })).text;
  await invoke("file_write", { path: "hello.py", text: `${before}# a second revision\n` });
  await git("commit", "-qam", "a second revision of hello");
  try {
    const [newest, older] = await invoke("file_commits", { path: "hello.py" });
    assert.ok(newest && older, "two revisions of hello.py");
    await press("commit-open", "hello.py", older.id.slice(0, 7));
    await ui.waitFor(() => editing().active === `hello.py@${older.id}`, 5000, "hello.py as the older revision left it");
    await press("commit-compare", "hello.py", older.id.slice(0, 7));
    await ui.waitFor(() => document.querySelector("#mode-edit .desk .diff-button"), 5000, "the revision beside the file");
    closeDiff();
    await press("commit-compare", "hello.py", newest.id.slice(0, 7), older.id.slice(0, 7));
    await ui.waitFor(() => document.querySelector("#mode-edit .desk .diff-button"), 5000, "the revisions beside each other");
    closeDiff();
    const copied = [];
    calls.answer("clip_write", ({ text }) => copied.push(text));
    await press("commit-copy-id", newest.id.slice(0, 7));
    await press("commit-copy-message", newest.id.slice(0, 7), "hello.py");
    assert.deepEqual(copied, [newest.id, "a second revision of hello"]);
    await press("commit-files", newest.id.slice(0, 7));
    const row = () => [...document.querySelectorAll(".git-commit")].find((one) => one.dataset.key === newest.id);
    await ui.waitFor(() => row()?.getAttribute("aria-expanded") === "true", 5000, "the commit's files shown");
    await press("commit-file-changes", newest.id.slice(0, 7), "hello.py");
    await ui.waitFor(() => document.querySelector("#mode-edit .desk .diff-button"), 5000, "the commit's change to hello.py");
    closeDiff();
    await press("commit-files", newest.id.slice(0, 7));
    await ui.waitFor(() => row()?.getAttribute("aria-expanded") === "false", 5000, "and hidden again");
  } finally {
    await git("reset", "-q", "--hard", "HEAD~1");
    asFound();
  }
}, { timeout: 60000 });

test("Local History's menu, from the command line: a save's difference shown, and the file taken back to it", async () => {
  asFound();
  showView("edit");
  await invoke("file_write", { path: "history_file.txt", text: "first\n" });
  await openFile("history_file.txt");
  const view = editing().editor;
  view.change([{ from: { line: 0, col: 0 }, to: view.doc.end(), text: "second\n" }], null);
  await press("save");
  view.change([{ from: { line: 0, col: 0 }, to: view.doc.end(), text: "third\n" }], null);
  await press("save");
  const saves = await ui.waitFor(async () => {
    const found = await invoke("history_list", { path: "history_file.txt" });
    return found.length >= 2 && found;
  }, 5000, "the saves kept");
  const second = saves.find((one) => one.size === "second\n".length) ?? saves.at(-1);
  await press("history-difference", "history_file.txt", String(second.at));
  await ui.waitFor(() => document.querySelector("#mode-edit .desk .diff-button"), 5000, "the save beside the file");
  closeDiff();
  await press("history-revert", "history_file.txt", String(second.at));
  await ui.waitFor(async () => (await invoke("file_read", { path: "history_file.txt" })).text === "second\n" || editing().editor.doc.text() === "second\n", 5000, "the file taken back");
  asFound();
});

test("Review's menu, from the command line: a comment opened, its changes shown, resolved, copied and deleted", async () => {
  asFound();
  addComment("hello.py", 0, "def greet(name):", "a note of the test's");
  const one = comments().find((comment) => comment.body === "a note of the test's");
  assert.ok(one, "the comment left");
  await press("comment-open", one.id);
  await ui.waitFor(() => editing().active === "hello.py" && editing().editor.head().line === 0, 5000, "hello.py at the comment's line");
  await press("comment-changes", one.id);
  await ui.waitFor(() => document.querySelector("#mode-edit .desk .diff-button") || told(/No change/), 5000, "the file's changes");
  closeDiff();
  await press("comment-resolve", one.id, "done");
  assert.ok(comments().find((comment) => comment.id === one.id).done, "resolved");
  await press("comment-resolve", one.id, "open");
  assert.ok(!comments().find((comment) => comment.id === one.id).done, "and open again");
  const copied = [];
  calls.answer("clip_write", ({ text }) => copied.push(text));
  await press("comment-copy", one.id);
  assert.deepEqual(copied, ["a note of the test's"]);
  await press("comment-delete", one.id);
  assert.ok(!comments().some((comment) => comment.id === one.id), "deleted");
  asFound();
});

["change-show", "job-start", "job-stop", "job-edit", "job-copy-path", "output-copy", "output-copy-all", "output-select-all"].forEach((command) => COVERED.add(command));
["terminal-copy", "terminal-paste", "terminal-close", "status-show", "status-move", "status-reset"].forEach((command) => COVERED.add(command));

test("the Changes pane's menu, from the command line: a file's changes shown", async () => {
  asFound();
  const before = (await invoke("file_read", { path: "README.md" })).text;
  await invoke("file_write", { path: "README.md", text: `${before}a change of the test's\n` });
  try {
    await press("commit-view");
    await ui.waitFor(() => document.querySelector('.change-row[data-path="README.md"]'), 10000, "README.md among the changes");
    await press("change-show", "README.md");
    await ui.waitFor(() => document.querySelector("#mode-edit .desk .diff-button"), 5000, "README.md's changes");
  } finally {
    closeDiff();
    await invoke("file_write", { path: "README.md", text: before });
    asFound();
  }
});

test("the jobs' menu and their output's, from the command line: started, stopped, edited, their path and output copied", async () => {
  asFound();
  await invoke("file_write", { path: "src/build_quick.sh", text: "#!/usr/bin/env bash\necho quick\n" });
  await invoke("file_write", { path: "src/build_slow.sh", text: "#!/usr/bin/env bash\nsleep 30\n" });
  await loadRun();
  const copied = [];
  calls.answer("clip_write", ({ text }) => copied.push(text));
  await press("job-start", SLOW);
  await ui.waitFor(() => chosenJob()?.id === SLOW && chosenLive(), 10000, "the job running");
  await press("job-stop", SLOW);
  await ui.waitFor(() => !chosenLive(), 10000, "the job stopped");
  await press("job-start", QUICK);
  await ui.waitFor(() => chosenJob()?.id === QUICK && !chosenLive() && /quick/.test(document.getElementById("lines")?.textContent ?? ""), 15000, "the quick job run");
  await press("output-copy-all", QUICK);
  assert.match(copied.at(-1) ?? "", /quick/, "the output copied whole");
  await press("output-select-all");
  assert.match(window.getSelection().toString(), /quick/, "the output chosen whole");
  await press("output-copy");
  assert.match(copied.at(-1) ?? "", /quick/, "what is chosen copied");
  await press("job-copy-path", QUICK);
  assert.equal(copied.at(-1), "src/build_quick.sh");
  await press("job-edit", QUICK);
  await ui.waitFor(() => editing().active === "src/build_quick.sh", 5000, "the job's file open");
  showView("edit");
  asFound();
}, { timeout: 60000 });

test("the terminal's menu, from the command line: what is chosen copied, text pasted, and the panel closed", async () => {
  asFound();
  await press("new");
  const view = () => document.querySelector("#term-views .term-view:not([hidden])");
  await ui.waitFor(() => view()?.textContent.trim(), 15000, "the shell's prompt");
  await press("terminal-paste", "echo pasted-by-the-test");
  await ui.waitFor(() => /pasted-by-the-test/.test(view().textContent), 5000, "the text pasted into the shell");
  const copied = [];
  calls.answer("clip_write", ({ text }) => copied.push(text));
  window.getSelection().selectAllChildren(view());
  await press("terminal-copy");
  assert.match(copied.at(-1) ?? "", /pasted-by-the-test/, "the screen's chosen text copied");
  window.getSelection().removeAllRanges();
  await press("kill");
  await press("terminal-close");
  assert.ok(document.getElementById("term").hidden, "the panel closed");
  asFound();
}, { timeout: 60000 });

test("the status bar's menu, from the command line: an item hidden and shown, moved along its line, and the bar reset", async () => {
  asFound();
  // Memory's line as it stands on the screen, each line ordering its items as its style says.
  const order = () => {
    const line = document.querySelector('#statusbar [data-item="memory"]').parentElement;
    return [...line.querySelectorAll(":scope > [data-item]")].filter((one) => one.getClientRects().length).sort((a, b) => a.getBoundingClientRect().left - b.getBoundingClientRect().left).map((one) => one.dataset.item);
  };
  await press("status-show", "zoom", "off");
  assert.ok(!itemShown("zoom"), "Zoom hidden");
  await press("status-show", "zoom", "on");
  assert.ok(itemShown("zoom"), "and shown");
  const before = order();
  const left = before[before.indexOf("memory") - 1];
  await press("status-move", "memory", "left");
  if (left) {
    assert.ok(order().indexOf("memory") < order().indexOf(left), `Memory moved past ${left}: ${order()}`);
  }
  await press("status-move", "memory", "right");
  await press("status-show", "focus", "off");
  await press("status-reset");
  assert.ok(itemShown("focus"), "every item shown again");
  assert.deepEqual(order(), before, "in the bar's own order");
});

test("the window is put back as the file found it", async () => {
  const tree = TREE();
  const git = (...words) => ui.run("git", ["-C", tree, ...words]).catch(() => {});
  await press("close-all").catch(() => {});
  await git("clean", "-fdqx", "--", ...MADE);
  await git("checkout", "--", "hello.py", "README.md");
  for (const mounted of await invoke("tree_mounts").catch(() => [])) {
    await invoke("tree_unmount", { name: mounted.name }).catch(() => {});
  }
  asFound();
  // Key by key, as orior keeps each change in its own folder through the page's own setItem and
  // removeItem.
  const stored = new Map(STORED);
  for (const key of Object.keys(localStorage).filter((one) => !stored.has(one))) {
    localStorage.removeItem(key);
  }
  for (const [key, value] of STORED) {
    localStorage.setItem(key, value);
  }
  assert.deepEqual(Object.entries(localStorage).sort(), [...STORED].sort());
  // orior keeps the settings in its own folder once they rest, which the next page reads.
  await ui.rest(1500);
});


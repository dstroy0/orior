// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The menu bar's keys: a key two commands for different kinds of file share goes to the one whose
// kind the file shown is. And every item of every menu, pressed from its menu as a person presses it.

import { allCommands, firstKeys, hasCommand, isChecked, runCommand } from "../../src/menubar.js";
import { invoke } from "../../src/bridge.js";
import { closeMenu } from "../../src/menu.js";
import { editing, openFile } from "../../src/edit.js";
import { killTerminal, newTerminal, runInTerminal, toggleTerminal } from "../../src/terminal.js";
import { showView } from "../../src/views.js";
import { paletteOpen } from "../../src/palette.js";
import { focusOn, memoryBudget, noticesHeld, noticesSaid } from "../../src/statusbar.js";
import { previewMode } from "../../src/preview.js";
import { scheme } from "../../src/scheme.js";
import { zoom } from "../../src/zoom.js";
import { checkersNamed, setCheckers } from "../../src/servers.js";

const { test, assert, ui } = window.__harness;

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

// Every menu closed, every sheet and dialog closed, and the keys let go.
async function putBack() {
  closeMenu(false);
  document.querySelectorAll("dialog[open]").forEach((dialog) => dialog.close());
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

test("View, Find the Slow Plugin halves the plugins on until one is named, or says none is on", async () => {
  await press("find-slow-plugin");
  const finder = document.getElementById("slow-finder");
  while (!finder.hidden) {
    await ui.click([...finder.querySelectorAll(".slow-finder-button")].find((button) => button.textContent === "No Longer Slow"));
    await frame();
  }
  assert.ok(told(/No plugin is on|is the plugin that slows the window/), "the finding ends with what it found");
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


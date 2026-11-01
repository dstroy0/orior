// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The menu bar's keys: a key two commands for different kinds of file share goes to the one whose
// kind the file shown is. And every item of every menu, pressed from its menu as a person presses it.

import { allCommands, hasCommand, runCommand } from "../../gui/src-js/menubar.js";
import { editing, openFile } from "../../gui/src-js/edit.js";
import { killTerminal, newTerminal, runInTerminal, toggleTerminal } from "../../gui/src-js/terminal.js";
import { showView } from "../../gui/src-js/views.js";
import { COVERED, given, labelOf, leavesOf, marked, ON_TEXT, openTo, press, putBack, putBackAsFound, viewSaid, wait } from "../menus.js";

const { test, assert, ui } = window.__harness;

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

test(
  "every item of every menu is pressed from its menu as a person presses it, and none throws",
  async () => {
    const commandOf = new Map();
    // A setting the menus check turns on and off: pressed again, it stands as the walk found it.
    const checking = new Set(allCommands().filter((item) => item.checks).map((item) => item.command));
    // A label is read as a command of the bar's own menus: a list's menu can give the same label
    // another command, as the profile's Zoom Out does.
    const onBar = new Set([...document.getElementById("menubar").children].map((title) => title.textContent.trim()));
    for (const item of allCommands().filter((one) => onBar.has(one.menu))) {
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
      if (said(event).includes(window.__harness.blocked)) {
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
            unreached.push(`${pressing}: ${error.message}; ${viewSaid()}`);
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
    runInTerminal("orior_gui view word-wrap on");
    await ui.waitFor(() => editing().wordWrap(), 30000, "the window to wrap its words").catch((error) => {
      throw new Error(`${error.message}; the terminal shows: ${screen().trim().slice(-600)}`);
    });
    runInTerminal("orior_gui view word-wrap off");
    await ui.waitFor(() => !editing().wordWrap(), 30000, "the window to stop wrapping");
    assert.ok(document.querySelectorAll("#tabs .tab").length > 0, "the same window, its tabs still open");
  } finally {
    killTerminal();
    toggleTerminal(false);
    editing().setWordWrap(kept === "true");
  }
}, { timeout: 90000 });

test("every command of the menus has a test of what it does, and the window answers every one", () => {
  const missing = allCommands().map((item) => item.command).filter((command) => !COVERED.has(command));
  assert.deepEqual([...new Set(missing)], [], "commands with no test of what they do");
  const unanswered = allCommands().filter((item) => !item.console && !hasCommand(item.command)).map((item) => item.command);
  assert.deepEqual(unanswered, [], "commands the window does not answer, which a command line could not run there");
});

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

test("Go, Line goes to the line its word names", async () => {
  await given("a|b\ncd\nef");
  await press("line", "3");
  assert.equal(marked(), "ab\ncd\n|ef");
});

test("the window is put back as the file found it", putBackAsFound);

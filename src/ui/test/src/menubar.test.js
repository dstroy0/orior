// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The menu bar's keys: a key two commands for different kinds of file share goes to the one whose
// kind the file shown is. And every item of every menu, pressed from its menu as a person presses it.

import { allCommands, firstKeys, runCommand } from "../../src/menubar.js";
import { closeMenu } from "../../src/menu.js";

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

// The answers given to what some commands ask; every other question is closed unanswered.
const ANSWERS = {
  "create-file": "walk_new.txt",
  "create-folder": "walk_folder",
  "new-branch": "walk-branch",
  stash: "walk stash",
  "macro-keep": "walk macro",
};

// What puts the window back after a command: its own press again for a setting that turns on and
// off, and the command that closes what it opened.
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
  await ui.click(title);
  let menu = await ui.waitFor(() => openMenus()[0], 3000);
  for (const [depth, label] of path.entries()) {
    const item = itemsIn(menu).find((button) => labelOf(button) === label);
    assert.ok(item, `${labelOf(title)} / ${path.slice(0, depth + 1).join(" / ")} is in its menu`);
    await ui.click(item);
    menu = await ui.waitFor(() => openMenus()[depth + 1], 3000);
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
    for (const item of allCommands()) {
      for (const label of [item.label, ...Object.values(item.labels ?? {})]) {
        commandOf.set(label, item.command);
      }
    }
    const threw = [];
    let pressing = "";
    const heard = (event) => threw.push(`${pressing}: ${event.error?.message ?? event.reason?.message ?? event.reason ?? event.message}`);
    addEventListener("error", heard);
    addEventListener("unhandledrejection", heard);
    let pressed = 0;
    try {
      const titles = document.getElementById("menubar").children.length;
      for (let at = 0; at < titles; at++) {
        const title = labelOf(document.getElementById("menubar").children[at]) || document.getElementById("menubar").children[at].textContent;
        for (const path of await leavesOf(at)) {
          const label = path.at(-1);
          const command = commandOf.get(label) ?? "";
          pressing = `${title} / ${path.join(" / ")}`;
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
          await putBack();
          if (AFTER[command]) {
            runCommand(AFTER[command]);
            await wait(150);
            await putBack();
          }
        }
      }
    } finally {
      removeEventListener("error", heard);
      removeEventListener("unhandledrejection", heard);
      await putBack();
    }
    assert.ok(pressed > 100, `pressed ${pressed} items`);
    assert.deepEqual(threw, [], "what the items threw");
  },
  { timeout: 1_800_000 },
);

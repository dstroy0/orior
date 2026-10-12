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
          if (AFTER[command]) {
            runCommand(AFTER[command]);
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

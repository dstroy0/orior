// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The tabs: drawn in their row, marked, grouped, and the tab shown kept in sight.

import { runCommand } from "../../../gui/src-js/menubar.js";
import { invoke } from "../../../gui/src-js/bridge.js";
import { closeMenu } from "../../../gui/src-js/menu.js";
import { editing, openFile } from "../../../gui/src-js/edit.js";
import { showView } from "../../../gui/src-js/views.js";
import { asFound, press, putBackAsFound, tabOfName } from "../../menus.js";
import { togglePane } from "../../../gui/src-js/sides.js";

const { test, assert, ui } = window.__harness;

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

test("a tab group's menu, from the command line: folded, renamed, its tabs closed, and ungrouped", async () => {
  asFound();
  await press("save-all");
  showView("edit");
  for (const name of ["grouped_a.txt", "grouped_b.txt"]) {
    await invoke("file_write", { path: name, text: `${name}\n` });
    await openFile(name);
    await press("tab-group", name, "Made Group");
  }
  const header = () => [...document.querySelectorAll(".tab-group")].find((one) => one.querySelector(".tab-group-name")?.textContent.startsWith("Made Group") || one.querySelector(".tab-group-name")?.textContent === "Named Again");
  await ui.waitFor(() => header(), 3000, "the group");
  await press("group-fold", "Made Group", "on");
  assert.ok(header().classList.contains("folded"), "folded");
  await press("group-fold", "Made Group", "off");
  assert.ok(!header().classList.contains("folded"), "unfolded");
  await press("group-rename", "Made Group", "Named Again");
  assert.equal(header().querySelector(".tab-group-name").textContent, "Named Again");
  await press("group-close", "Named Again");
  await ui.waitFor(() => !tabOfName("grouped_a.txt") && !tabOfName("grouped_b.txt"), 3000, "the group's tabs closed");
  await openFile("grouped_a.txt");
  await press("tab-group", "grouped_a.txt", "Made Again");
  await press("group-ungroup", "Made Again");
  assert.ok(![...document.querySelectorAll(".tab-group-name")].some((one) => one.textContent === "Made Again"), "ungrouped");
  asFound();
});

test("the marked files' menu, from the command line: a file moved up the list and let go", async () => {
  asFound();
  const names = ["grouped_a.txt", "grouped_b.txt"];
  for (const name of names) {
    await invoke("file_write", { path: name, text: `${name}\n` });
    await openFile(name);
    if (!editing().markItems().some((item) => item.label.endsWith(name))) {
      await press("tab-mark", name);
    }
  }
  const listed = () => editing().markItems().map((item) => item.label.replace(/^\d+ {2}/, ""));
  const before = listed();
  assert.ok(before.indexOf("grouped_b.txt") > before.indexOf("grouped_a.txt"), JSON.stringify(before));
  await press("mark-up", "grouped_b.txt");
  assert.ok(listed().indexOf("grouped_b.txt") < listed().indexOf("grouped_a.txt"), JSON.stringify(listed()));
  for (const name of names) {
    await press("mark-let-go", name);
  }
  assert.ok(!names.some((name) => listed().includes(name)), "both let go");
  asFound();
});

test("a tab stands 85% of 2.3rem tall with the row's line under it, however many tabs the row holds", async () => {
  showView("edit");
  await openFile("hello.py");
  await ui.waitFor(() => editing().active === "hello.py", 10000);
  const rem = Number.parseFloat(getComputedStyle(document.documentElement).fontSize);
  const tall = document.querySelector("#tabs .tab").getBoundingClientRect().height + 1;
  assert.ok(Math.abs(tall - 2.3 * 0.85 * rem) < 0.5, `a tab and the line under it ${tall}px tall`);
});

test("the tabs stand inside the work's lines, the first tab clear of its corner", async () => {
  showView("edit");
  togglePane(false);
  await openFile("hello.py");
  await ui.waitFor(() => editing().active === "hello.py", 10000);
  await ui.rest(400);
  await ui.brought("#tabs");
  document.getElementById("tabs").scrollLeft = 0;
  const work = document.querySelector(".work").getBoundingClientRect();
  const first = document.querySelector("#tabs .tab").getBoundingClientRect();
  assert.ok(first.top >= work.top + 1.75, `the first tab's top ${first.top - work.top}px below the work's, under its 2px line`);
  assert.ok(first.left >= work.left + 0.75, `the first tab's left ${first.left - work.left}px in from the work's, past its 1px line`);
});

test("a row of more tabs than it holds keeps each tab its height, its scrollbar hanging below it over the editor, the editor where it stood", async () => {
  showView("edit");
  await openFile("hello.py");
  await ui.waitFor(() => editing().active === "hello.py", 10000);
  const strip = document.getElementById("tabs");
  const rem = Number.parseFloat(getComputedStyle(document.documentElement).fontSize);
  const editorTop = () => document.querySelector("#mode-edit .ed").getBoundingClientRect().top;
  // the editor holds the keys and the tabs row is brought back to it, so each reading is taken with
  // the window's edges standing as they stand while a person edits and looks at the tabs
  await ui.click(document.querySelector("#mode-edit .editor"));
  await ui.brought(strip);
  const before = editorTop();
  const names = Array.from({ length: 40 }, (_, at) => `many_tabs/a_file_with_a_long_name_${String(at).padStart(2, "0")}.txt`);
  await invoke("folder_create", { path: "many_tabs" }).catch(() => {});
  try {
    for (const name of names) {
      await invoke("file_write", { path: name, text: "x\n" });
      await openFile(name);
    }
    await ui.brought(strip);
    await ui.rest(500);
    assert.ok(strip.scrollWidth > strip.clientWidth, "the row holds more than it shows");
    const tab = strip.querySelector(".tab").getBoundingClientRect();
    assert.ok(tab.height >= 2.3 * 0.85 * rem - 1.5, `a tab ${tab.height}px tall`);
    assert.ok(strip.getBoundingClientRect().bottom > tab.bottom + 4, `the scrollbar hangs below the tabs, the row's foot ${strip.getBoundingClientRect().bottom - tab.bottom}px under theirs`);
    assert.ok(Math.abs(editorTop() - before) < 0.5, `the editor's top moved ${editorTop() - before}px`);
  } finally {
    for (const name of names) {
      strip.querySelector(`.tab .close[title="${name}"]`)?.click();
    }
  }
});

test("the tab shown stands in sight in a strip of more tabs than the window holds", async () => {
  showView("edit");
  const names = Array.from({ length: 40 }, (_, at) => `many_tabs/a_file_with_a_long_name_${String(at).padStart(2, "0")}.txt`);
  await invoke("folder_create", { path: "many_tabs" }).catch(() => {});
  for (const name of names) {
    await invoke("file_write", { path: name, text: "x\n" });
    await openFile(name);
  }
  const strip = document.getElementById("tabs");
  const inSight = (path) => {
    const box = strip.querySelector(`.tab[title="${path}"]`)?.getBoundingClientRect();
    const view = strip.getBoundingClientRect();
    return Boolean(box) && box.left >= view.left - 1 && box.right <= Math.min(view.right, innerWidth) + 1;
  };
  try {
    assert.ok(strip.scrollWidth > strip.clientWidth, `the strip holds more than it shows: ${strip.scrollWidth} px in ${strip.clientWidth}`);
    await ui.waitFor(() => inSight(names.at(-1)), 2000, "the last tab opened in sight");
    await openFile(names[0]);
    await ui.waitFor(() => inSight(names[0]), 2000, "the first tab, shown again, in sight");
  } finally {
    for (const name of names) {
      const tab = strip.querySelector(`.tab .close[title="${name}"]`);
      tab?.click();
    }
  }
});

test("the window is put back as the file found it", putBackAsFound);

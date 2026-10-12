// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The explorer's Outline over a file of twenty thousand symbols: only the rows in sight on the page,
// the symbol the cursor is in marked and scrolled to, and a press on a row going to its line.

import { invoke } from "../../src/bridge.js";
import { editing, openFile } from "../../src/edit.js";
import { iconOf, showGroup, showPane } from "../../src/explorer.js";
import { togglePane } from "../../src/sides.js";
import { showView } from "../../src/views.js";

const { test, assert, ui } = window.__harness;

test("the Outline of twenty thousand symbols holds the rows in sight, marks the one the cursor is in and goes to the one pressed", async () => {
  const lines = [];
  for (let line = 0; line < 20000; line += 1) {
    lines.push(`def step_${line}(value):`, "    return value");
  }
  await invoke("file_write", { path: "outlined.py", text: `${lines.join("\n")}\n` });
  showView("edit");
  await openFile("outlined.py");
  await ui.waitFor(() => editing().active === "outlined.py" && editing().editor?.s, 10000);
  const view = editing().editor;
  togglePane(true);
  showPane("outline");
  try {
    const body = document.getElementById("outline");
    const rows = () => [...body.querySelectorAll(".sym")];
    const name = (row) => row?.querySelector(".name")?.textContent;
    await ui.waitFor(() => rows().length > 0, 5000);
    assert.ok(rows().length < 200, `rows on the page: ${rows().length} of 20000`);
    assert.equal(name(rows()[0]), "step_0");
    assert.equal(body.scrollHeight >= 20000 * 22 - 22, true, `the space stands for every row: ${body.scrollHeight}`);
    view.goTo(2 * 15000 + 1, 0);
    await ui.waitFor(() => name(body.querySelector('.sym[aria-current="true"]')) === "step_15000", 3000);
    const lit = body.querySelector('.sym[aria-current="true"]');
    const box = lit.getBoundingClientRect();
    const sight = body.getBoundingClientRect();
    assert.ok(box.top >= sight.top - 1 && box.bottom <= sight.bottom + 1, "the marked row is in sight");
    assert.ok(rows().length < 200, `and the rows on the page stay few: ${rows().length}`);
    const next = rows().find((row) => name(row) === "step_15003");
    await ui.click(next);
    await ui.waitFor(() => view.primary().head.line === 2 * 15003, 3000);
    assert.equal(view.primary().head.line, 30006, "the press goes to the symbol's line");
  } finally {
    showGroup("explorer");
  }
});

test("every file has a mark and every folder the folder's shape, a character's width before each name", async () => {
  assert.equal(iconOf("main.go").textContent, "go");
  assert.equal(iconOf("Dockerfile.dev").textContent, "dk", "a name looked up without a type that has no mark");
  assert.equal(iconOf("license.py").textContent, "py", "and a type with a mark is its type");
  const other = iconOf("model.onnx");
  assert.deepEqual([other.textContent, other.classList.contains("unmarked")], ["on", true], "a type with no mark is drawn with its own first letters");
  assert.equal(iconOf("NOTICE").textContent, "no", "and a name with no type with its own");
  for (const name of ["main.c", "view.tsx", "song.wav", "archive.zip", "Makefile", "LICENSE", ".env"]) {
    assert.ok(iconOf(name).textContent.trim(), `${name} has a mark`);
  }
  await invoke("folder_create", { path: "kinds" }).catch(() => {});
  await invoke("file_write", { path: "kinds/model.onnx", text: "x" });
  showView("edit");
  togglePane(true, { take: false, pin: true });
  showGroup("explorer");
  const rows = () => [...document.querySelectorAll("#files .node")];
  await ui.waitFor(() => rows().some((row) => row.dataset.key === "kinds"), 10000);
  const kinds = rows().find((row) => row.dataset.key === "kinds");
  if (kinds.getAttribute("aria-expanded") !== "true") {
    await ui.click(kinds);
  }
  await ui.waitFor(() => rows().some((row) => row.dataset.key === "kinds/model.onnx"), 10000);
  for (const row of rows()) {
    const icon = row.querySelector(":scope > .icon");
    assert.ok(icon, `${row.dataset.key} has an icon`);
    if (row.classList.contains("dir") && row.dataset.archive !== "true") {
      assert.ok(icon.querySelector(".glyph-folder"), `${row.dataset.key} has the folder's shape`);
    } else {
      assert.ok(icon.textContent.trim(), `${row.dataset.key} has a mark`);
    }
    const name = row.querySelector(".name").getBoundingClientRect();
    const gap = name.left - icon.getBoundingClientRect().right;
    assert.ok(gap >= 0.5 * parseFloat(getComputedStyle(row).fontSize), `${row.dataset.key}: ${gap.toFixed(1)} px before the name`);
  }
});

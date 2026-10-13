// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The explorer's Outline over a file of twenty thousand symbols: only the rows in sight on the page,
// the symbol the cursor is in marked and scrolled to, and a press on a row going to its line; and
// each file's mark standing on the line its name stands on; and a folder of twenty thousand files
// opened, scrolled fast and stepped through by the keys, with only the rows in sight on the page.

import { invoke, onCalls } from "../../src/bridge.js";
import { editing, openFile, treeRowShown } from "../../src/edit.js";
import { drawProblems, iconOf, showGroup, showPane } from "../../src/explorer.js";
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

test("each file's mark in the tree has its ink centered on the middle of its name's capitals", async () => {
  showView("edit");
  togglePane(true, { take: false, pin: true });
  await document.fonts.ready;
  await ui.waitFor(() => document.querySelectorAll("#files .node:not(.dir) .icon").length >= 3, 10000);
  // A box of no height set in a text stands on the text's line, wherever the text is moved to.
  const lineOf = (node) => {
    const probe = Object.assign(document.createElement("span"), { style: "display:inline-block;width:0;height:0;vertical-align:baseline" });
    node.append(probe);
    const at = probe.getBoundingClientRect().bottom;
    probe.remove();
    return at;
  };
  // The ink of `text` set as `node` sets it: how far above its line it reaches, and below, found
  // in its pixels drawn at 100px, the font scaled to set a "0" as wide as the node sets one.
  const canvas = Object.assign(document.createElement("canvas"), { width: 400, height: 240 });
  const pen = canvas.getContext("2d", { willReadFrequently: true });
  const inkOf = (node, text) => {
    const style = getComputedStyle(node);
    const zeros = Object.assign(document.createElement("span"), { textContent: "0000000000", style: "position:absolute;visibility:hidden;white-space:pre" });
    node.append(zeros);
    const wide = zeros.getBoundingClientRect().width / 10;
    zeros.remove();
    pen.font = `${style.fontWeight} 100px ${style.fontFamily}`;
    pen.clearRect(0, 0, canvas.width, canvas.height);
    pen.fillText(text, 10, 160);
    const { data } = pen.getImageData(0, 0, canvas.width, canvas.height);
    const inked = [];
    for (let row = 0; row < canvas.height; row += 1) {
      for (let at = row * canvas.width * 4 + 3; at < (row + 1) * canvas.width * 4; at += 4) {
        if (data[at] > 96) {
          inked.push(row);
          break;
        }
      }
    }
    const scale = wide / pen.measureText("0").width;
    return { above: (160 - inked[0]) * scale, below: (inked.at(-1) + 1 - 160) * scale };
  };
  const off = [...document.querySelectorAll("#files .node:not(.dir)")]
    .filter((row) => row.checkVisibility() && row.querySelector(".icon")?.textContent && row.querySelector(".name"))
    .map((row) => {
      const icon = row.querySelector(".icon");
      const name = row.querySelector(".name");
      const mark = inkOf(icon, icon.textContent);
      const middle = lineOf(icon) - (mark.above - mark.below) / 2;
      const capitals = lineOf(name) - inkOf(name, "H").above / 2;
      return { mark: icon.textContent, name: row.dataset.key, by: Math.round((middle - capitals) * 100) / 100 };
    })
    .filter((one) => Math.abs(one.by) > 0.75);
  assert.deepEqual(off, [], "each mark's ink centered on its name's capitals");
});

test("the tree's check says in Problems how far it has gone, and Stop lets the files waiting go", async () => {
  const calls = window.__harness.calls;
  calls.answer("problems_check_stop", () => null);
  showView("edit");
  togglePane(true, { take: false, pin: true });
  showGroup("problems");
  try {
    drawProblems([], { done: 25, total: 100 });
    const line = await ui.waitFor(() => document.querySelector("#problems .problems-checking"), 2000, "the check's line");
    assert.match(line.textContent, /^Checking the tree: 25 of 100 files\. Stop$/);
    await ui.click(line.querySelector(".pulse-stop"));
    assert.ok(calls.made.some((one) => one.name === "problems_check_stop"), "Stop ends the check");
    drawProblems([], { done: 100, total: 100 });
    assert.equal(document.querySelector("#problems .problems-checking"), null, "and the line goes once it is done");
  } finally {
    showGroup("explorer");
  }
});

// The frames drawn over `ms`, each the time it began.
function framesOver(ms) {
  return new Promise((done) => {
    const times = [];
    const tick = (now) => {
      times.push(now);
      if (now - times[0] < ms) {
        requestAnimationFrame(tick);
      } else {
        done(times);
      }
    };
    requestAnimationFrame(tick);
  });
}

const BIG = 20000;

test("a folder of twenty thousand generated files opens at once, scrolls fast at the display's own rate, and its rows step by the keys", async () => {
  const tree = window.__harness.servers.ORIOR_TEST_TREE.replace(/\\/g, "/");
  // Generated files, as a build's output is, are left out by git: shown in the tree, and not checked.
  const ignoredBefore = await invoke("file_read", { path: ".gitignore" }).then((read) => read.text).catch(() => null);
  await invoke("file_write", { path: ".gitignore", text: `${ignoredBefore ?? ""}generated/\n` });
  // The calls the window makes as the files are written and a moment after.
  const asked = [];
  let hearing = true;
  onCalls((name) => hearing && asked.push(name));
  await ui.run("python", ["-c", `import os\nos.makedirs("${tree}/generated", exist_ok=True)\nfor at in range(${BIG}):\n    open(f"${tree}/generated/part_{at:05}.py", "w").write("x = 1\\n")\n`]);
  await framesOver(3000);
  hearing = false;
  const parsed = asked.filter((name) => name === "parse_colors").length;
  assert.ok(parsed <= 1, `files written elsewhere have the file open parsed again at most once: ${parsed} times`);
  const files = document.getElementById("files");
  const rowOf = (path) => treeRowShown(path);
  try {
    showView("edit");
    togglePane(true, { take: false, pin: true });
    showPane("folder");
    await ui.waitFor(() => rowOf("generated")?.checkVisibility(), 20000, "the folder in the tree");
    const rest = await framesOver(1000);
    const display = (rest.length - 1) / ((rest.at(-1) - rest[0]) / 1000);
    const pressed = performance.now();
    const opening = framesOver(1500);
    await ui.click(rowOf("generated"));
    await ui.waitFor(() => rowOf(`generated/part_${String(BIG - 1).padStart(5, "0")}.py`), 20000, "the folder's last file");
    const opened = performance.now() - pressed;
    const openFrames = await opening;
    const openGaps = openFrames.slice(1).map((time, step) => time - openFrames[step]);
    files.scrollTop = 0;
    const box = files.getBoundingClientRect();
    const at = { x: box.left + box.width / 2, y: box.top + box.height / 2 };
    const counting = framesOver(3000);
    const until = performance.now() + 3000;
    while (performance.now() < until) {
      await ui.wheel(at, 400);
    }
    const times = await counting;
    const gaps = times.slice(1).map((time, step) => time - times[step]);
    const rate = gaps.length / ((times.at(-1) - times[0]) / 1000);
    const said = `opened in ${opened.toFixed(0)} ms, its longest frame ${Math.max(...openGaps).toFixed(1)} ms; scrolled to ${files.scrollTop} at ${rate.toFixed(1)} frames a second of the display's ${display.toFixed(1)}, the longest wait ${Math.max(...gaps).toFixed(1)} ms, ${files.querySelectorAll(".node").length} rows on the page`;
    assert.ok(files.scrollTop > box.height * 20, `the tree scrolled far: ${said}`);
    assert.ok(rate >= display * 0.8, `the display's rate held: ${said}`);
    assert.ok(Math.max(...gaps) < 100, `no frame held as it scrolled: ${said}`);
    assert.ok(opened < 1000, `the folder open within a second: ${said}`);
    assert.ok(Math.max(...openGaps) < 100, `no frame held as it opened: ${said}`);
    assert.ok(files.querySelectorAll(".node").length < 200, `only the rows in sight and ahead on the page: ${said}`);
    // The keys step through every row, on the page or not.
    const focused = () => document.activeElement?.dataset.key;
    files.scrollTop = 0;
    rowOf("generated").focus();
    await ui.key("ArrowDown");
    assert.equal(focused(), "generated/part_00000.py", "Down into the folder's first file");
    for (let step = 0; step < 60; step += 1) {
      await ui.key("ArrowDown");
    }
    assert.equal(focused(), "generated/part_00060.py", "sixty rows on, past those that were on the page");
    await ui.key("End");
    assert.equal(focused(), files.rowModel.key(files.rowModel.count - 1), "End to the tree's last row");
    rowOf("generated/part_00000.py").focus();
    await ui.type("part_12345");
    assert.equal(focused(), "generated/part_12345.py", "letters typed find a row far below");
    await ui.key("Escape");
    await ui.key("ArrowLeft");
    assert.equal(focused(), "generated", "Left out to the folder that holds it");
  } finally {
    files.scrollTop = 0;
    if (rowOf("generated")?.getAttribute("aria-expanded") === "true") {
      await ui.click(rowOf("generated"));
    }
    await ui.run("python", ["-c", `import shutil\nshutil.rmtree("${tree}/generated", ignore_errors=True)\n`]);
    if (ignoredBefore === null) {
      await ui.run("python", ["-c", `import os\nos.remove("${tree}/.gitignore")\n`]);
    } else {
      await invoke("file_write", { path: ".gitignore", text: ignoredBefore });
    }
  }
}, { timeout: 180000 });

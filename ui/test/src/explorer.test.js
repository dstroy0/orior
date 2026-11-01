// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The explorer's Outline over a file of twenty thousand symbols: only the rows in sight on the page,
// the symbol the cursor is in marked and scrolled to, and a press on a row going to its line; and
// each file's mark standing on the line its name stands on; and a folder of twenty thousand files
// opened, scrolled fast and stepped through by the keys, with only the rows in sight on the page.

import { showPane } from "../../gui/src-js/explorer.js";
import { invoke, onCalls } from "../../gui/src-js/bridge.js";
import { treeRowShown } from "../../gui/src-js/edit.js";
import { togglePane } from "../../gui/src-js/sides.js";
import { showView } from "../../gui/src-js/views.js";

const { test, assert, ui } = window.__harness;

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

const rowOf = (path) => treeRowShown(path);

const LAST = `generated/part_${String(BIG - 1).padStart(5, "0")}.py`;

// A folder of BIG generated files written into the run's tree and shown in the explorer, `work` run with the calls
// the window made as they were written and a moment after, and the folder taken away after whatever `work` did.
// Generated files, as a build's output is, are left out by git: shown in the tree, and not checked.
async function withGenerated(work) {
  const tree = window.__harness.servers.ORIOR_TEST_TREE.replace(/\\/g, "/");
  const ignoredBefore = await invoke("file_read", { path: ".gitignore" }).then((read) => read.text).catch(() => null);
  await invoke("file_write", { path: ".gitignore", text: `${ignoredBefore ?? ""}generated/\n` });
  const asked = [];
  let hearing = true;
  onCalls((name) => hearing && asked.push(name));
  const files = document.getElementById("files");
  try {
    await ui.run("python", ["-c", `import os\nos.makedirs("${tree}/generated", exist_ok=True)\nfor at in range(${BIG}):\n    open(f"${tree}/generated/part_{at:05}.py", "w").write("x = 1\\n")\n`]);
    await framesOver(3000);
    hearing = false;
    showView("edit");
    togglePane(true, { take: false, pin: true });
    showPane("folder");
    await ui.waitFor(() => rowOf("generated")?.checkVisibility(), 20000, "the folder in the tree");
    await work({ asked, files });
  } finally {
    hearing = false;
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
}

test("a folder of twenty thousand generated files opens at once with only the rows in sight on the page, and its rows step by the keys", async () => {
  await withGenerated(async ({ asked, files }) => {
    const parsed = asked.filter((name) => name === "parse_colors").length;
    assert.ok(parsed <= 1, `files written elsewhere have the file open parsed again at most once: ${parsed} times`);
    const pressed = performance.now();
    const opening = framesOver(1500);
    await ui.click(rowOf("generated"));
    await ui.waitFor(() => rowOf(LAST), 20000, "the folder's last file");
    const opened = performance.now() - pressed;
    const openFrames = await opening;
    const openGaps = openFrames.slice(1).map((time, step) => time - openFrames[step]);
    const said = `opened in ${opened.toFixed(0)} ms, its longest frame ${Math.max(...openGaps).toFixed(1)} ms, ${files.querySelectorAll(".node").length} rows on the page`;
    assert.ok(opened < 1000, `the folder open within a second: ${said}`);
    assert.ok(Math.max(...openGaps) < 100, `no frame delayed as it opened: ${said}`);
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
  });
}, { timeout: 180000 });

test("a folder of twenty thousand generated files scrolls fast at the display's own rate", async () => {
  // a fast wheel is a wheel step each few milliseconds, which only the window's debugging port takes so often
  if (window.__harness.servers.ORIOR_TEST_DRIVER === "webdriver") {
    throw new Error("not run on a WebDriver window: it sends no wheel step sooner than the step before is answered, which the window with its debugging port sends");
  }
  await withGenerated(async ({ files }) => {
    const rest = await framesOver(1000);
    const display = (rest.length - 1) / ((rest.at(-1) - rest[0]) / 1000);
    await ui.click(rowOf("generated"));
    await ui.waitFor(() => rowOf(LAST), 20000, "the folder's last file");
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
    const said = `scrolled to ${files.scrollTop} at ${rate.toFixed(1)} frames a second of the display's ${display.toFixed(1)}, the longest wait ${Math.max(...gaps).toFixed(1)} ms, ${files.querySelectorAll(".node").length} rows on the page`;
    assert.ok(files.scrollTop > box.height * 20, `the tree scrolled far: ${said}`);
    assert.ok(rate >= display * 0.8, `the display's rate kept: ${said}`);
    assert.ok(Math.max(...gaps) < 100, `no frame delayed as it scrolled: ${said}`);
    assert.ok(files.querySelectorAll(".node").length < 200, `only the rows in sight and ahead on the page: ${said}`);
  });
}, { timeout: 180000 });

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The files people report other editors slow on, scrolled the whole way in one form, and our own container files
// the same way.

import { lineHeight } from "../../../../gui/src-js/editor/view.js";
import { invoke } from "../../../../gui/src-js/bridge.js";
import { editing, openFile } from "../../../../gui/src-js/edit.js";
import { showView } from "../../../../gui/src-js/views.js";
import { status } from "../../../../gui/src-js/status.js";
import { joinRun, traceTime, TRACE_BLOB } from "../../../../gui/src-js/trace.js";
import { framesOver, paintsOf, seen } from "../../../frames.js";

const { test, assert, ui } = window.__harness;

// The files people report other editors slow to open and to scroll, which the harness fetched and put
// in the tree, each scrolled with the window joined to a run of its own, and the window's own trace of
// its frames read back from the run.
const BENCH = window.__harness.servers.ORIOR_TEST_BENCH;

// Scrolls the file of the tree at `path` under a steady fast wheel for `seconds` with the window joined
// to a run, and gives what each frame drew and what the trace says of it.
async function scrolledThrough(path, seconds) {
  const writer = window.__harness.servers.ORIOR_TEST_QRY_WRITER;
  const root = (await invoke("root_get")).replaceAll("\\", "/");
  await invoke("folder_create", { path: "build/trace" }).catch(() => {});
  const relative = `build/trace/query1-${path.replace(/\W/g, "_")}.qry`;
  showView("edit");
  await openFile(path);
  await ui.waitFor(() => editing().active === path && editing().editor?.s, 30000);
  await ui.waitFor(() => !status.reads.size, 60000);
  const view = editing().editor;
  const smooth = view.smoothOn;
  view.smoothOn = true;
  view.goTo(0, 0);
  const rest = await framesOver(1000);
  const restGaps = rest.slice(1).map((time, index) => time - rest[index]);
  const interval = [...restGaps].sort((a, b) => a - b)[Math.floor(restGaps.length / 2)];
  const missedAtRest = restGaps.filter((gap) => gap > 1.5 * interval).length / restGaps.length;
  await ui.run(writer, [`${root}/${relative}`, root], { wait: false });
  const frames = [];
  try {
    assert.ok(await invoke("qry_ready", { path: `${root}/${relative}` }), "the run's writer came ready");
    await joinRun(relative);
    await ui.browserTrace();
    // Each frame as it begins: where the view stands in the file, the window a paged file holds moving under it
    // without moving it, the rows the text layer has written, and, for a few lines on screen, whether each is drawn
    // with a color.
    let sampling = true;
    const sample = () => {
      const first = Math.floor(seen(view) / lineHeight());
      const lines = [first + 2, first + 12, first + 22].filter((line) => line < view.s.doc.count);
      frames.push({ at: traceTime(), y: seen(view) + view.s.base * lineHeight(), writes: view.textLayer.writes, colored: lines.map((line) => [line, /class="t-/.test(view.rowNode(line)?.innerHTML ?? "")]) });
      if (sampling) {
        requestAnimationFrame(sample);
      }
    };
    requestAnimationFrame(sample);
    const box = view.scroller.getBoundingClientRect();
    const at = { x: box.left + box.width / 2, y: box.top + box.height / 2 };
    const began = traceTime();
    const until = performance.now() + seconds * 1000;
    while (performance.now() < until) {
      await ui.wheel(at, 120);
      await new Promise((done) => setTimeout(done, 20));
    }
    const stopped = traceTime();
    await new Promise((done) => setTimeout(done, 1500));
    sampling = false;
    await joinRun(null);
    await ui.browserTraced(writer, `${root}/${relative}`, `${root}/browser.trace`);
    try {
      await ui.waitFor(async () => (await invoke("qry_joined", { path: relative, name: TRACE_BLOB })).length > 0, 5000);
    } catch (error) {
      throw new Error(`${error.message}: the window's trace queue ${JSON.stringify(await invoke("qry_window"))}, the run at ${relative}`);
    }
    await new Promise((done) => setTimeout(done, 300));
    const paints = paintsOf(await invoke("qry_joined", { path: relative, name: TRACE_BLOB }));
    const browser = (await invoke("qry_joined", { path: relative, name: "browser.trace" })).split("\n").filter((line, at) => at < 8 || line.startsWith("forced")).join("; ");
    return { view, interval, missedAtRest, frames, paints, began, stopped, browser };
  } finally {
    view.smoothOn = smooth;
    await joinRun(null);
    await invoke("qry_stop", { path: `${root}/${relative}` }).catch(() => {});
  }
}

// The scroll of the file at `path` judged: every frame on time, every line on screen drawn as it is at
// rest, and the text at rest a moment after the wheel stops with no row written again.
async function judged(name, path) {
    const { view, interval, missedAtRest, frames, paints, began, stopped, browser } = await scrolledThrough(path, 3);
    const during = paints.filter((paint) => paint.at > began + 300 && paint.at < stopped);
    const gaps = during.slice(1).map((paint, index) => paint.at - during[index].at);
    const late = gaps.filter((gap) => gap > 1.5 * interval);
    const slow = [...during].sort((a, b) => b.spent - a.spent).slice(0, 3).map((paint) => `${paint.spent.toFixed(2)} ms ${JSON.stringify(Object.fromEntries(Object.entries(paint.stages).map(([stage, ms]) => [stage, Number(ms.toFixed(2))])))}`);
    const said = `${name}: ${during.length} frames drawn, ${(1000 / interval).toFixed(0)} Hz; the slowest ${slow.join("; ")}`;
    assert.ok(during.length > 100, `frames the trace holds: ${said}`);
    assert.ok(late.length <= gaps.length * (0.03 + missedAtRest), `frames late: ${late.length} of ${gaps.length}, ${(missedAtRest * 100).toFixed(1)}% at rest: ${said}; the browser's busiest work: ${browser}`);
    // Every line sampled on screen as the wheel turned is drawn as it is at rest.
    const plain = [];
    for (const frame of frames.filter((one) => one.at < stopped)) {
      for (const [line, colored] of frame.colored) {
        const atRest = view.s.highlight.runsOf(line).some(([, cls]) => cls);
        if (atRest && !colored) {
          plain.push(line);
        }
      }
    }
    assert.equal(plain.length, 0, `lines drawn without the colors they have at rest: ${plain.length}, the first ${plain.slice(0, 8).join(" ")}: ${said}`);
    // The text stands a moment after the wheel stops, and no row is written again once it stands.
    const moved = frames.filter((frame, index) => index > 0 && frame.y !== frames[index - 1].y).map((frame) => frame.at);
    const stood = Math.max(stopped, ...moved);
    assert.ok(stood - stopped <= 400, `the text stood ${(stood - stopped).toFixed(0)} ms after the last wheel step: ${said}`);
    const written = frames.filter((frame) => frame.at >= stood);
    assert.equal(written.at(-1).writes - written[0].writes, 0, `rows written again after the text stood: ${said}`);
}

for (const [name, keywords] of [["checker.ts", true], ["sqlite3.c", true], ["large-file.json", false]]) {
  test(`a fast wheel through ${name} animates the entire way: every frame on time, every row drawn once in its one form${keywords ? ", its keywords colored the whole way" : ""}, and the text at rest a moment after the wheel stops`, async () => {
    assert.ok(BENCH, "the harness fetched the files people report slow");
    await judged(name, `${BENCH}/${name}`);
  }, { timeout: 120000 });
}

test("our own container files scroll the same way: a file read from a run's .qry, and a scroll's own trace read from the run it was handed to", async () => {
  assert.ok(BENCH, "the harness fetched the files people report slow");
  const writer = window.__harness.servers.ORIOR_TEST_QRY_WRITER;
  const root = (await invoke("root_get")).replaceAll("\\", "/");
  await invoke("folder_create", { path: "build/bench" }).catch(() => {});
  const container = "build/bench/query1-bench.qry";
  await ui.run(writer, [`${root}/${container}`, root], { wait: false });
  try {
    assert.ok(await invoke("qry_ready", { path: `${root}/${container}` }), "the run's writer came ready");
    const before = (await invoke("qry_window")).queued.handed;
    for (const name of ["checker.ts", "sqlite3.c", "large-file.json"]) {
      await invoke("qry_append_file", { path: container, file: `${BENCH}/${name}` });
    }
    await ui.waitFor(async () => (await invoke("qry_window")).queued.handed >= before + 3, 30000);
  } finally {
    await invoke("qry_stop", { path: `${root}/${container}` });
  }
  const held = await invoke("tree_list", { dir: container });
  assert.ok(held.some((child) => child.name === "bench" && child.dir), `the run's files under the folder they were read from: ${JSON.stringify(held.map((child) => child.name))}`);
  await judged("sqlite3.c in a run's .qry", `${container}/${BENCH}/sqlite3.c`);
  await judged("the trace of a scroll", `build/trace/query1-${`${BENCH}/sqlite3.c`.replace(/\W/g, "_")}.qry/window.trace`);
}, { timeout: 240000 });

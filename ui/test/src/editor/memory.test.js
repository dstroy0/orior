// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The memory a document's life takes and gives back: a ten-megabyte file opened, pasted into, undone
// and closed, ten times over, the bytes the window's own process and the whole have committed measured
// after each, with what the Rust side holds; the process holds no more after the tenth than after the
// first beyond what one open takes, or the growth is reported with the calls that kept it.

import { invoke } from "../../../gui/src-js/bridge.js";
import { editing, openFile } from "../../../gui/src-js/edit.js";
import { showView } from "../../../gui/src-js/views.js";

const { test, assert, ui } = window.__harness;

const ROUNDS = 10;
const MEGABYTES = 10;
// The growth over the rounds a process may show and still be giving its documents back, in bytes.
const SLACK = 48 << 20;

const lineOf = (number) => `const value_${number} = ${number} * 2; // a line of the pasted text, number ${number}\n`;

function textOfBytes(bytes) {
  const parts = [];
  let size = 0;
  for (let number = 0; size < bytes; number += 1) {
    const line = lineOf(number);
    parts.push(line);
    size += line.length;
  }
  return parts.join("");
}

async function closed(path) {
  const mark = `#tabs .tab .close[title="${path}"]`;
  const closer = () => document.querySelector(mark);
  for (let press = 0; press < 2 && closer(); press += 1) {
    closer().scrollIntoView({ block: "nearest", inline: "nearest" });
    await ui.waitFor(() => closer()?.checkVisibility(), 3000);
    // pressed by its selector: the tab is drawn anew as the undo leaves the text clean
    await ui.click(mark);
  }
  await ui.waitFor(() => !closer(), 30000, `${path} closed`);
}

async function measured() {
  const read = await invoke("memory_use");
  const calls = await invoke("memory_calls", { on: true, reset: true }).catch(() => ({}));
  const kept = Object.entries(calls).filter(([, one]) => one.kept > 0).sort((a, b) => b[1].kept - a[1].kept).slice(0, 4).map(([call, one]) => `${call}:${(one.kept / 1048576).toFixed(1)} MB`);
  // the bytes the window's own process and the web view's have committed, each by its name, not their working
  // sets: the pool is committed whole as the window starts and its pages come into the working set as cells are
  // first touched, which is no growth
  const part = (name) => read.parts.find((one) => one.name === name)?.commit ?? 0;
  return { own: part("orior_gui"), view: part("web view"), all: read.commit, rust: read.rust_live ?? 0, kept };
}

test("a ten-megabyte file opened, pasted into, undone and closed ten times leaves the window holding no more than once", async () => {
  const text = textOfBytes(MEGABYTES << 20);
  const root = (await invoke("root_get")).replaceAll("\\", "/");
  await invoke("clip_write", { text });
  const rounds = [];
  for (let round = 0; round < ROUNDS; round += 1) {
    const name = `memory_${round}.py`;
    await invoke("file_write_any", { path: `${root}/${name}`, text: "# a line of the target\n".repeat(200) });
    showView("edit");
    await openFile(name);
    await ui.waitFor(() => editing().active === name && editing().editor?.s, 30000);
    const view = editing().editor;
    const doc = view.s.doc;
    const before = doc.count;
    view.goTo(100, 0);
    await ui.key("Ctrl+V");
    await ui.waitFor(() => doc.count > before + 1000, 120000, `round ${round} pasted`);
    view.undo();
    await ui.waitFor(() => doc.count === before, 30000, `round ${round} undone`);
    await closed(name);
    rounds.push(await measured());
  }
  const first = rounds[0];
  const last = rounds[rounds.length - 1];
  const said = rounds.map((one, at) => `${at}: own ${(one.own / 1048576).toFixed(0)} MB, web view ${(one.view / 1048576).toFixed(0)} MB, all ${(one.all / 1048576).toFixed(0)} MB, the Rust side holds ${(one.rust / 1048576).toFixed(0)} MB${one.kept.length ? ` kept ${one.kept.join("; ")}` : ""}`).join("\n");
  const grew = last.own - first.own;
  assert.ok(grew < SLACK, `the window's own process grew ${(grew / 1048576).toFixed(0)} MB over ${ROUNDS} rounds:\n${said}`);
  const viewGrew = last.view - first.view;
  assert.ok(viewGrew < SLACK, `the web view grew ${(viewGrew / 1048576).toFixed(0)} MB over ${ROUNDS} rounds:\n${said}`);
});

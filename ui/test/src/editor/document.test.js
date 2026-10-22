// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The document under a paste: a text of ten lines to ten thousand, then of megabytes, put at a known
// line by the Ctrl+V the harness presses, on each of the two buses, the editor's own clipboard and
// the system's. Each paste is timed from its paste event to the document holding the text and to the
// frame drawn after, and lands whole where the cursor stood. The times go to paste.tsv beside the
// run's report.

import { invoke } from "../../../gui/src-js/bridge.js";
import { editing, openFile } from "../../../gui/src-js/edit.js";
import { showView } from "../../../gui/src-js/views.js";

const { test, assert, ui } = window.__harness;

// The buses in the order each size pastes on them: the two interleaved size by size, so a cold first
// paste lands on one row and the row beside it, the same size on the other bus, tells.
const BUSES = ["internal", "os"];
const LINES = [10, 100, 250, 500, 1000, 2500, 5000, 10000];
// The megabytes pasted and the buses each pastes on: the editor's own clipboard copies from a file the
// editor opens whole, and one the editor opens paged is read, not copied from, so past 1 MB the
// system's clipboard alone.
const MEGABYTES = [
  [1, BUSES],
  [2, ["os"]],
  [5, ["os"]],
  [10, ["os"]],
];
// The line of the target the text is pasted at, and the target's lines around it.
const AT = 100;
const TARGET_LINES = 200;

const lineOf = (number) => `const value_${number} = ${number} * 2; // a line of the pasted text, number ${number}\n`;

function textOfLines(count) {
  const parts = [];
  for (let number = 0; number < count; number += 1) {
    parts.push(lineOf(number));
  }
  return parts.join("");
}

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

// Each paste a row: its bus, lines and bytes; the editor's own milliseconds from the text in hand to
// the document holding it; and from the key the harness pressed to the document and to the frame after.
const rows = ["bus\tlines\tbytes\town_ms\tto_document_ms\tto_frame_ms"];

async function rowKept(bus, lines, bytes, own, held, drawn) {
  rows.push([bus, lines, bytes, own.toFixed(1), held.toFixed(1), drawn.toFixed(1)].join("\t"));
  const home = window.__harness.servers?.ORIOR_TEST_HOME;
  if (home) {
    await invoke("file_write_any", { path: `${String(home).replace(/\\/g, "/")}/../paste.tsv`, text: `${rows.join("\n")}\n` }).catch(() => {});
  }
}

// A file of the tree written with `text` and opened in the editor: its view.
async function opened(name, text) {
  await invoke("file_write_any", { path: `${(await invoke("root_get")).replaceAll("\\", "/")}/${name}`, text });
  showView("edit");
  await openFile(name);
  await ui.waitFor(() => editing().active === name && editing().editor?.s, 30000);
  return editing().editor;
}

// The text put where a Ctrl+V takes it from: on the editor's own clipboard by a Ctrl+C over a file
// holding it, or on the system's clipboard alone.
async function put(bus, text, name) {
  if (bus === "internal") {
    const source = await opened(`${name}.source.txt`, text);
    await ui.waitFor(() => source.s.doc.count === text.split("\n").length, 30000, `${name}: the source's lines read`);
    source.selectAll();
    // The copy's own time, from its event to the text taken, is kept beside the paste's.
    let copying = 0;
    const copy = source.copy;
    source.copy = (event, cut) => {
      const began = performance.now();
      copy.call(source, event, cut);
      copying = performance.now() - began;
    };
    await ui.key("Ctrl+C");
    try {
      await ui.waitFor(() => source.clip?.text === text, 20000, "the editor's clipboard holding the text");
    } catch (error) {
      throw new Error(`${error.message}: the copy ran ${copying.toFixed(1)} ms, the clipboard holds ${source.clip?.text?.length ?? "nothing"} of ${text.length} letters, the source ${source.s.doc.count} lines`);
    } finally {
      source.copy = copy;
    }
  } else {
    await invoke("clip_write", { text });
  }
}

// How many targets were made, so a test run again makes fresh ones in place of those it pasted into.
let made = 0;

// The tab of `path` closed, a changed one by its second press, so the pastes leave no large text
// open behind them.
async function closed(path) {
  const closer = () => document.querySelector(`#tabs .tab .close[title="${path}"]`);
  for (let press = 0; press < 2 && closer(); press += 1) {
    await ui.waitFor(() => closer()?.checkVisibility(), 3000);
    await ui.click(closer());
  }
  await ui.waitFor(() => !closer(), 30000, `${path} closed`);
}

// `text` pasted at line AT of a fresh target by Ctrl+V: the milliseconds from the key's press to the
// document holding it, and to the frame after; the target checked to hold the text whole at that line.
async function pasted(bus, text, named) {
  made += 1;
  const name = `${named}_${made}`;
  await put(bus, text, name);
  const view = await opened(`${name}.py`, "# a line of the target\n".repeat(TARGET_LINES));
  const doc = view.s.doc;
  const expected = doc.count + text.split("\n").length - 1;
  view.goTo(AT, 0);
  let at = 0;
  let began = 0;
  let held = 0;
  let drawn = 0;
  // The editor's paste itself, from the text in hand to the document: the clipboard read before it
  // is the bus's own cost.
  const pasteText = view.pasteText;
  view.pasteText = (text) => {
    began = performance.now();
    pasteText.call(view, text);
  };
  const unwatch = doc.watch(() => {
    if (doc.count === expected && !held) {
      held = performance.now();
      requestAnimationFrame(() => {
        drawn = performance.now();
      });
    }
  });
  try {
    at = performance.now();
    await ui.key("Ctrl+V");
    await ui.waitFor(() => drawn > 0, 120000, `${name} pasted and drawn`);
  } finally {
    view.pasteText = pasteText;
    unwatch();
  }
  assert.ok(began > 0, `${name}: the editor's paste ran`);
  assert.equal(doc.count, expected, `${name}: the lines after the paste`);
  assert.equal(doc.line(AT), lineOf(0).trimEnd(), `${name}: the first pasted line stands at line ${AT}`);
  assert.equal(doc.line(AT + text.split("\n").length - 2), lineOf(text.split("\n").length - 2).trimEnd(), `${name}: and the last after it`);
  assert.equal(doc.line(AT + text.split("\n").length - 1), "# a line of the target", `${name}: the target's line follows`);
  // The paste undone leaves the target as its file is, so its tab closes clean and holds no large text behind.
  view.undo();
  await ui.waitFor(() => doc.count === expected - (text.split("\n").length - 1), 30000, `${name}: the paste undone`);
  await closed(`${name}.py`);
  if (bus === "internal") {
    await closed(`${name}.source.txt`);
  }
  return { own: held - began, held: held - at, drawn: drawn - at };
}

test("a paste of ten lines to ten thousand lands at its line on each bus, size by size, each timed", async () => {
  const said = [];
  for (const count of LINES) {
    const text = textOfLines(count);
    for (const bus of BUSES) {
      const { own, held, drawn } = await pasted(bus, text, `paste_${bus}_${count}_lines`);
      await rowKept(bus, count, text.length, own, held, drawn);
      said.push(`${bus} ${count}: ${own.toFixed(1)} / ${held.toFixed(1)} / ${drawn.toFixed(1)} ms`);
    }
  }
  assert.ok(said.length === LINES.length * BUSES.length, `own / to the document / to the frame: ${said.join("; ")}`);
});

for (const [megabytes, buses] of MEGABYTES) {
  test(`a paste of ${megabytes} MB lands at its line on ${buses.join(" and ")}, timed`, async () => {
    const text = textOfBytes(megabytes << 20);
    const said = [];
    for (const bus of buses) {
      const { own, held, drawn } = await pasted(bus, text, `paste_${bus}_${megabytes}_mb`);
      await rowKept(bus, text.split("\n").length - 1, text.length, own, held, drawn);
      said.push(`${bus}: ${own.toFixed(1)} / ${held.toFixed(1)} / ${drawn.toFixed(1)} ms`);
    }
    assert.ok(said.length === buses.length, `own / to the document / to the frame: ${said.join("; ")}`);
  }, { timeout: 240000, tries: 1 });
}

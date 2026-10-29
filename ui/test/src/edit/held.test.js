// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The memory a document's life takes and gives back: a ten-megabyte file opened, pasted into, undone
// and closed, ten times over, the bytes the window's own process, its web view and the whole have committed
// measured after each; the process holds no more after the tenth than after the first beyond what one open
// takes, or the growth is reported round by round.

import { invoke } from "../../../gui/src-js/bridge.js";
import { editing, openFile } from "../../../gui/src-js/edit.js";
import { showView } from "../../../gui/src-js/views.js";
import { lineChanges } from "../../../gui/src-js/editor/diff.js";

const { test, assert, ui, calls } = window.__harness;

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

// Every call about a document's branch answered: its changes sent, as they go out a moment after the event that
// made them, and the calls before them done.
async function settled(doc) {
  await new Promise((rest) => setTimeout(rest, 50));
  await doc.held?.chain;
}

// Whether the branch under `key` is gone from the window's side, as closing its tab lets it go, within 5 s.
async function released(key) {
  for (let tries = 0; tries < 100; tries += 1) {
    if (await invoke("document_edit", { key, changes: [] }).then(() => false, () => true)) {
      return true;
    }
    await new Promise((rest) => setTimeout(rest, 50));
  }
  return false;
}

async function measured() {
  // what the page keeps, its garbage collected first
  await ui.gc();
  const read = await invoke("memory_usage");
  // the bytes the window's own process and the web view's have committed, each by its name, not their working
  // sets: the pool is committed whole as the window starts and its pages come into the working set as cells are
  // first touched, which is no growth
  const part = (name) => read.parts.find((one) => one.name === name)?.commit ?? 0;
  // the page's own script heap beside them, to tell its growth from the web view's beneath it
  return { own: part("orior_gui"), view: part("web view"), all: read.commit, heap: performance.memory?.usedJSHeapSize ?? 0 };
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
    // a round measured once its work is done: the paste and the undo answered on the window's side, and the branch
    // let go there after the tab closes
    await settled(doc);
    const key = doc.held?.key;
    await closed(name);
    assert.ok(!key || (await released(key)), `round ${round}: the branch let go on the window's side`);
    rounds.push(await measured());
  }
  const first = rounds[0];
  const last = rounds[rounds.length - 1];
  // every round on one line, as a failure is told in its first lines
  const said = rounds.map((one, at) => `${at}: own ${(one.own / 1048576).toFixed(0)}, view ${(one.view / 1048576).toFixed(0)}, heap ${(one.heap / 1048576).toFixed(0)}`).join("; ");
  const grew = last.own - first.own;
  assert.ok(grew < SLACK, `the window's own process grew ${(grew / 1048576).toFixed(0)} MB over ${ROUNDS} rounds:\n${said}`);
  const viewGrew = last.view - first.view;
  assert.ok(viewGrew < SLACK, `the web view grew ${(viewGrew / 1048576).toFixed(0)} MB over ${ROUNDS} rounds:\n${said}`);
  // ten megabytes pasted ten times, timed as the megabyte pastes are; one run alone, as a second over the first's
  // leavings measures them with its own
}, { timeout: 240000, tries: 1 });

test("a large text is held on the window's side, each change follows it there, and its gutter's marks come from there as the page's own diff gives them", async () => {
  const lines = Array.from({ length: 20000 }, (_, line) => `value_${line} = compute(${line}) + ${line % 7}`);
  const root = await invoke("root_get");
  await invoke("folder_create", { path: "held" }).catch(() => {});
  // a try after the first finds the repository made and the file committed as it is written again
  await invoke("repo_init", { folder: `${root}/held` }).catch((error) => assert.match(String(error), /is a repository already/));
  await invoke("file_write", { path: "held/held.py", text: `${lines.join("\n")}\n` });
  await invoke("git_commit", { message: "a large file held", paths: ["held/held.py"] }).catch(async (error) => {
    assert.equal((await invoke("file_head", { path: "held/held.py" })).split(/\r?\n/).length, lines.length + 1, `the commit refused: ${error}`);
  });
  showView("edit");
  await openFile("held/held.py");
  await ui.waitFor(() => editing().active === "held/held.py" && editing().editor?.s?.doc.held?.ready, 10000);
  const view = editing().editor;
  const s = view.s;
  const held = s.doc.held;
  view.goTo(5000, 0);
  view.focus();
  await ui.type("changed_");
  view.goTo(12000, 0);
  await ui.key("Shift+Down");
  await ui.key("Delete");
  view.goTo(15000, 0);
  await ui.type("put in\n");
  // Each change has gone to the window's side, once the calls about the text are answered, and the text
  // there is the page's, line for line and unit for unit.
  await settled(s.doc);
  const shape = await invoke("document_edit", { key: held.key, changes: [] });
  assert.deepEqual(shape, { lines: s.doc.count, units: s.doc.lines.reduce((sum, line) => sum + line.length, 0) });
  const head = (await invoke("file_head", { path: "held/held.py" })).split(/\r?\n/);
  const own = lineChanges(head, s.doc.lines);
  await ui.waitFor(() => s.changes && s.changes.hunks.length === own.hunks.length, 5000);
  assert.deepEqual([...s.changes.changed].sort((a, b) => a - b), [...own.changed].sort((a, b) => a - b), "changed");
  assert.deepEqual([...s.changes.added].sort((a, b) => a - b), [...own.added].sort((a, b) => a - b), "added");
  assert.deepEqual([...s.changes.removed].sort((a, b) => a - b), [...own.removed].sort((a, b) => a - b), "taken out");
  assert.deepEqual(s.changes.hunks, own.hunks);
  for (const hunk of own.hunks) {
    assert.deepEqual(s.changesThen.slice(hunk.then[0], hunk.then[1]), head.slice(hunk.then[0], hunk.then[1]), "the lines a change spans as the commit holds them");
  }
  // Closed, its tab's text is let go on the window's side. The tab strip is drawn again as the marks
  // come, and the editing claims the window's edges, so each press is by the close button's selector:
  // the press brings the strip back from its edge and finds the button as it then stands.
  const mark = `#tabs .tab .close[title="held/held.py"]`;
  const closer = () => document.querySelector(mark);
  // a press brings the strip back, one asks again of a changed tab, one closes it
  for (let press = 0; press < 4 && closer(); press += 1) {
    await ui.click(mark);
    await ui.rest(100);
  }
  await ui.waitFor(() => !closer(), 3000).catch((error) => {
    const box = closer()?.getBoundingClientRect();
    const at = box ? document.elementFromPoint(box.left + box.width / 2, box.top + box.height / 2) : null;
    throw new Error(`${error.message}; the close at ${box ? `${Math.round(box.left)},${Math.round(box.top)} ${Math.round(box.width)}x${Math.round(box.height)}` : "nowhere"}, shown ${closer()?.checkVisibility()}, under it ${at?.outerHTML.slice(0, 160)}; sheet ${document.querySelector("dialog[open]")?.textContent.slice(0, 200)}; status ${document.getElementById("status-said")?.textContent}`);
  });
  assert.ok(await released(held.key), "no branch is stored under its key");
});

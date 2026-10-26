// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { onAuthors } from "../authors.js";
import { invoke } from "../bridge.js";
import { lineChanges } from "../editor/diff.js";
import { state } from "../edit.js";
import { openTouched } from "./compare.js";
import { tabOf, tabOfSession } from "./tabs.js";

// Texts of HOLD_FROM lines or more are held on the window's side as the page holds them, for the work on
// them that costs a frame here to run there and hand back only its answer: the gutter's marks against
// the last commit, which cost the page a frame from about there, and find on a text of more. The Rust
// side reads the file itself; a text the page holds as the file does not goes over in parts of PART
// characters, each holding the page about a millisecond. Each change the page's document writes to a
// held text goes after it, out of the event that made it, and the calls about one text go one after
// another, each once the one before is answered.
const HOLD_FROM = 10000;
const PART = 90000;
let holds = 0;

function shapeOf(doc) {
  let units = 0;
  for (const line of doc.lines) {
    units += line.length;
  }
  return { lines: doc.count, units };
}

// Runs `work`, a call about a held text, once every call before it about that text is answered.
export function along(held, work) {
  const next = held.chain.then(work);
  held.chain = next.catch(() => {});
  return next;
}

// Holds the text of a tab's file on the window's side, read there from the file where `fromFile` says the
// tab's text is the file's own; otherwise, or where what was read differs, the page gives it.
export function hold(tab, fromFile) {
  const s = tab.session;
  const doc = s?.doc;
  if (!doc || doc.held !== undefined || s.window || s.base || tab.commit || doc.count < HOLD_FROM) {
    return;
  }
  holds += 1;
  const held = { key: `${tab.file}#${holds}`, ready: false, changes: [], waiting: false, chain: Promise.resolve(), headsAge: -1 };
  doc.held = held;
  const shape = shapeOf(doc);
  held.unwatch = doc.watch(({ edits }) => {
    held.changes.push(edits.map(({ from, to, text }) => ({ from: { line: from.line, col: from.col }, to: { line: to.line, col: to.col }, text })));
    if (held.ready && !held.waiting) {
      held.waiting = true;
      window.setTimeout(() => sendChanges(doc), 0);
    }
  });
  along(held, async () => {
    const read = fromFile ? await invoke("document_read", { key: held.key, path: tab.file }).catch(() => null) : null;
    if (!read || read.lines !== shape.lines || read.units !== shape.units) {
      // The text as it stands goes whole, and the changes made since it was measured are in it.
      const text = doc.text();
      held.changes = [];
      for (let at = 0; at === 0 || at < text.length; at += PART) {
        await invoke("document_part", { key: held.key, part: text.slice(at, at + PART), first: at === 0, done: at + PART >= text.length });
      }
    }
    held.ready = true;
    sendChanges(doc);
  }).catch(() => letGo(doc));
}

// Sends the changes written to a held text since the last were sent.
export function sendChanges(doc) {
  const held = doc?.held;
  if (!held?.ready) {
    return;
  }
  held.waiting = false;
  if (!held.changes.length) {
    return;
  }
  const changes = held.changes;
  held.changes = [];
  along(held, () => invoke("document_edit", { key: held.key, changes })).catch(() => letGo(doc));
}

// Lets go of a text held on the window's side; it is not held again.
export function letGo(doc) {
  const held = doc?.held;
  if (!held) {
    return;
  }
  held.unwatch?.();
  doc.held = null;
  invoke("document_release", { key: held.key }).catch(() => {});
}

export function forgetHeads() {
  state.heads.clear();
  state.headsAge += 1;
}

// The gutter's marks for a held text, worked out on the window's side against its file as the last commit
// holds it; the lines each change spans there come with them, for the peek and to put back.
async function markHeld(tab) {
  const s = tab.session;
  const held = s.doc.held;
  sendChanges(s.doc);
  const fresh = held.headsAge !== state.headsAge;
  const id = s.doc.id;
  if (!fresh && s.changesFor === id) {
    return;
  }
  held.headsAge = state.headsAge;
  const marks = await along(held, () => invoke("document_changes", { key: held.key, path: tab.file, fresh })).catch(() => null);
  if (s.doc.id !== id || s.doc.held !== held) {
    return;
  }
  if (!marks?.head) {
    state.editor.setChanges(s, null);
    markHistory(tab, null);
    return;
  }
  markHistory(tab, `${held.key} ${state.headsAge}`);
  s.changesFor = id;
  s.changesHead = null;
  const found = marks.changes;
  if (!found) {
    s.changesThen = null;
    state.editor.setChanges(s, null);
    return;
  }
  const then = [];
  for (const hunk of found.hunks) {
    (hunk.thenLines ?? []).forEach((line, at) => (then[hunk.then[0] + at] = line));
  }
  s.changesThen = then;
  const hunks = found.hunks.map(({ now, then: was }) => ({ now, then: was }));
  state.editor.setChanges(s, { added: new Set(found.added), changed: new Set(found.changed), removed: new Set(found.removed), hunks });
}

// Marks the lines of a tab's text that differ from the last commit. A file read only in part, a file
// as a commit left it, and a file the last commit does not hold have no marks.
export async function markChanges(tab) {
  const s = tab?.session;
  if (!s) {
    return;
  }
  if (tab.commit || s.window || s.base) {
    state.editor.setChanges(s, null);
    markHistory(tab, null);
    return;
  }
  if (s.doc.held) {
    await along(s.doc.held, () => null).catch(() => null);
  }
  if (s.doc.held?.ready) {
    await markHeld(tab);
    return;
  }
  if (!state.heads.has(tab.file)) {
    state.heads.set(tab.file, invoke("file_head", { path: tab.file }).catch(() => null));
  }
  const head = await state.heads.get(tab.file);
  if (typeof head !== "string") {
    state.editor.setChanges(s, null);
    markHistory(tab, null);
    return;
  }
  markHistory(tab, head);
  if (s.changesFor === s.doc.id && s.changesHead === head) {
    return;
  }
  s.changesFor = s.doc.id;
  s.changesHead = head;
  s.changesThen = head.split(/\r?\n/);
  state.editor.setChanges(s, lineChanges(s.changesThen, s.doc.lines));
}

// Line History: beside each line of a tab's text, the commit that last changed it, while Git, Line
// History is on, read again as the text or the file's last commit changes. `head` is the file as
// the last commit left it, or null where the last commit does not hold it.
function markHistory(tab, head) {
  const s = tab.session;
  if (!state.lineHistory || head === null) {
    s.historyFor = null;
    if (s.history) {
      state.editor.setLineHistory(s, null);
    }
    return;
  }
  if (s.historyFor === s.doc.id && s.historyHead === head) {
    return;
  }
  const id = s.doc.id;
  s.historyFor = id;
  s.historyHead = head;
  invoke("git_line_history", { path: tab.file, text: s.doc.text() })
    .then((history) => s.historyFor === id && s.historyHead === head && state.editor.setLineHistory(s, history))
    .catch(() => state.editor.setLineHistory(s, null));
}

export function setLineHistory(on) {
  state.lineHistory = on;
  markChanges(tabOf(state.active));
}

// Authors shown or hidden, the Line History shown is drawn again, its column the width it needs.
onAuthors(() => {
  const s = state.editor?.s;
  if (s?.history) {
    state.editor.setLineHistory(s, s.history);
  }
});

// A press on Line History's mark: the file's change in the commit that last changed the line.
export function openLineCommit(s, line) {
  const tab = tabOfSession(s);
  const commit = s.history?.commits[s.history.lines[line]];
  if (!tab || !commit) {
    return;
  }
  openTouched({ id: commit.id, parents: commit.parent ? [commit.parent] : [] }, { path: commit.path ?? tab.file, was: commit.was, state: commit.parent ? "M" : "A" });
}


// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { onAuthors } from "../authors.js";
import { invoke } from "../bridge.js";
import { state } from "../edit.js";
import { openTouched } from "./compare.js";
import { tabOf, tabOfSession } from "./tabs.js";

// Every text the editor opens from a file is a branch on the window's side as it read it, under the key the tab opened
// it with, the session the page's mirror of it, for every piece of work on it to run there and hand back only its
// answer: its colors, find, and the gutter's marks against the last commit. Each change the page's document writes to
// it goes after it, out of the event that made it, and the calls about one branch go one after another, each once the
// one before is answered.

// Runs `work`, a call about a branch, once every call before it about that branch is answered.
export function along(mirrored, work) {
  const next = mirrored.chain.then(work);
  mirrored.chain = next.catch(() => {});
  return next;
}

// The session the page's mirror of the branch under `key`, a tab's, a view's or a notebook cell's: the names and colors
// `opened` gave with it and how far across they color each line, the change `change` sent first where the session's
// text is not the branch's, and each change its document writes after it, every call about it waiting for `first`
// where one is given, the call that makes it.
export function mirror(session, key, opened = null, change = null, first = null) {
  session.highlight.mirrors(key, opened?.names ?? null, change ? [] : (opened?.colors ?? []), opened?.columns ?? 0);
  if (change) {
    session.highlight.changed(change);
  }
  mirrorText(session.doc, key, change, first);
}

// The session's mirror let go, and the branch under its key on the window's side with it.
export function letMirrorGo(session) {
  session?.highlight?.letGo();
  letGo(session?.doc);
}

function mirrorText(doc, key, change, first) {
  const chain = first ? Promise.resolve(first).catch(() => {}) : Promise.resolve();
  const mirrored = { key, ready: true, changes: change ? [change] : [], waiting: false, chain, headsAge: -1 };
  doc.mirrored = mirrored;
  mirrored.unwatch = doc.watch(({ edits }) => {
    mirrored.changes.push(edits.map(({ from, to, text }) => ({ from: { line: from.line, col: from.col }, to: { line: to.line, col: to.col }, text })));
    if (!mirrored.waiting) {
      mirrored.waiting = true;
      window.setTimeout(() => sendChanges(doc), 0);
    }
  });
  sendChanges(doc);
}

// Sends the changes written to a mirror since the last were sent.
export function sendChanges(doc) {
  const mirrored = doc?.mirrored;
  if (!mirrored?.ready) {
    return;
  }
  mirrored.waiting = false;
  if (!mirrored.changes.length) {
    return;
  }
  const changes = mirrored.changes;
  mirrored.changes = [];
  along(mirrored, () => invoke("document_edit", { key: mirrored.key, changes })).catch(() => letGo(doc));
}

// Lets go of a branch on the window's side; it is not mirrored again.
function letGo(doc) {
  const mirrored = doc?.mirrored;
  if (!mirrored) {
    return;
  }
  mirrored.unwatch?.();
  doc.mirrored = null;
  invoke("document_release", { key: mirrored.key }).catch(() => {});
}

// The last commit's files read afresh by each branch's next marks.
export function forgetHeads() {
  state.headsAge += 1;
}

// The gutter's marks for a branch, worked out on the window's side against its file as the last commit
// holds it; the lines each change spans there come with them, for the peek and to put back.
async function markMirrored(tab) {
  const s = tab.session;
  const mirrored = s.doc.mirrored;
  sendChanges(s.doc);
  const fresh = mirrored.headsAge !== state.headsAge;
  const id = s.doc.id;
  if (!fresh && s.changesFor === id) {
    return;
  }
  mirrored.headsAge = state.headsAge;
  const marks = await along(mirrored, () => invoke("document_changes", { key: mirrored.key, path: tab.file, fresh })).catch(() => null);
  if (s.doc.id !== id || s.doc.mirrored !== mirrored) {
    return;
  }
  if (!marks?.head) {
    state.editor.setChanges(s, null);
    markHistory(tab, null);
    return;
  }
  markHistory(tab, `${mirrored.key} ${state.headsAge}`);
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

// Marks the lines of a tab's text that differ from the last commit, as its branch on the window's side marks them. A
// file read only in part, a file as a commit left it, a file the last commit does not hold, and a text with no branch
// have no marks.
export async function markChanges(tab) {
  const s = tab?.session;
  if (!s) {
    return;
  }
  if (tab.commit || s.window || s.base || !s.doc.mirrored) {
    state.editor.setChanges(s, null);
    markHistory(tab, null);
    return;
  }
  await along(s.doc.mirrored, () => null).catch(() => null);
  if (s.doc.mirrored?.ready) {
    await markMirrored(tab);
  }
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

// Authors displayed or hidden, the Line History displayed is drawn again, its column the width it needs.
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


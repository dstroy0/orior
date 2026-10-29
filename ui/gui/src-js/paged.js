// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Files the tree's side opens paged, too large to read whole, held as pages (ui/cli/src/editor/pages.c). The tree's
// side holds the file and the lines written over it, and an index of its lines, and colors it as it colors every
// branch; the page holds a window of the lines about the view, as a file still being read is held: the view scrolls the
// whole file, and the lines outside the window stand as padding. Each read answers the lines with their colors, and the
// page puts the colors before it draws the lines, so no line is drawn uncolored.
//
// The tree's side places the window. The page tells it where the view stands each frame the view moves and once it has
// rested, and takes the lines it answers: as the view moves, the view's rows where it will be, each line cut at how far
// across the view reaches; as it rests, the view's rows whole, then the lines about it and the minimap's a step at a
// time, those outside them let go. A window of cut lines takes no edit and gives no copy until the rest makes it whole. A
// line the history still reaches is let go only past HARD lines, and the history with it. A jump to a line outside the
// window puts the window there. Each edit goes to the tree's side as it is made, the calls about one file one after
// another and every read after the edits before it, and a save writes the file there. A paged file is not backed up as
// it changes, and its tab is never split.

import { invoke } from "./bridge.js";
import { Doc } from "./editor/document.js";
import { Session } from "./editor/session.js";
import { say } from "./statusbar.js";

// The window's sizes in lines as a file opens or jumps: the lines above the line it opens at, and the lines read; and
// the most it keeps while the history reaches lines it would let go.
const NEAR = 100;
const STEP = 400;
const HARD = 20000;

// How long the view stands still, in milliseconds, before it is told as resting; and the reports of where it stands in
// flight at once.
const CALM = 100;
const FLYING = 2;

// The lines a view moves before, once it rests, the web view is asked to give back the memory the drawing on the way
// held, and the milliseconds its memory target is held low for it.
const MOVED_GIVEN = 1000;
const LOW_FOR = 1000;

// How far across each line of the paged session `s` is drawn, the letters a read colors: as far as its view reaches,
// every letter where its lines wrap, and where no view shows it as far as its colors were last told to reach.
const columnsOf = (s) => {
  const reach = s.view?.reach?.() ?? null;
  return reach === null ? s.highlight.columnsTold : Number.isFinite(reach) ? reach : 0;
};

const rest = (ms) => new Promise((done) => setTimeout(done, ms));

// Runs `work`, a call that changes the paged file `paged` or its window, once every such call before it about the file
// is answered: an edit, a jump, a save and the release, one after another.
function along(paged, work) {
  const next = paged.chain.then(work);
  paged.chain = next.catch(() => {});
  return next;
}

// Runs `work`, a read of the paged file `paged`, once every call `along` ran before it is answered: a read waits for
// the edits sent before it and not for another read, so a read slow to come holds back no report of where the view
// stands.
export function after(paged, work) {
  return paged.chain.then(work);
}

// How the paged file `paged` stands as a report is sent: its edits and the window's jumps; and whether it stands so
// still, so the lines answered are the lines the report was for.
const standing = (paged) => ({ edits: paged.edits, jumps: paged.jumps });
const stands = (paged, stood) => !paged.gone && paged.edits === stood.edits && paged.jumps === stood.jumps;

// The file at `path` that orior's side opened paged under `key`, `opened` its answer with the file's shape, the names
// its grammar's tokens take and the colors of the lines in view: its window about the line `line`, in `language`, the
// colors of the lines in view put. Gives its session.
export async function openPaged(path, line, language, key, opened) {
  let shape = opened.shape;
  if (!shape) {
    throw new Error(`${path}: it could not be opened paged`);
  }
  // A line far down the file waits for the index to reach it.
  while (!shape.indexed && shape.lines <= line + NEAR) {
    await rest(25);
    shape = await invoke("pages_shape", { key });
  }
  const from = Math.max(0, Math.min(line - NEAR, shape.lines - 1));
  const to = Math.min(shape.lines, from + STEP);
  const read = await invoke("pages_read", { key, from, to, columns: opened.columns ?? 0 });
  // the calls about the file one after another; the edits made and the window's jumps, counted; and the reports of
  // where the view stands, those in flight, the last sent, the last whose answer the window took, and the view's top
  // last told
  const paged = { key, chain: Promise.resolve(), indexed: shape.indexed, edits: 0, jumps: 0, flying: 0, sent: 0, taken: 0, told: null, resting: false, wanted: false, moved: 0 };
  // the window: its lines' cut, 0 where whole, and where cut each line's whole length
  const session = new Session(null, language, { doc: Doc.ofLines(read.lines), base: from, window: { paged, start: 0, end: 0, size: shape.size, after: shape.lines - to, cut: 0, lengths: null } });
  session.highlight.mirrors(key, opened.names, opened.colors, opened.columns ?? 0);
  session.highlight.given(read.colors);
  paged.session = session;
  // Each edit goes to the tree's side in the file's lines, as the window stands when it is made.
  paged.unwatch = session.doc.watch(({ edits }) => {
    paged.edits += 1;
    const base = session.base;
    const change = edits.map(({ from: a, to: b, text }) => ({ from: { line: a.line + base, col: a.col }, to: { line: b.line + base, col: b.col }, text }));
    along(paged, () => invoke("pages_edit", { key, changes: [change] })).catch((error) => say(`${path}: ${error}`, { failed: true }));
  });
  if (!shape.indexed) {
    followIndex(paged);
  }
  return session;
}

// Counts the lines below the window again as the index reads on, until it is whole.
async function followIndex(paged) {
  while (!paged.indexed && !paged.gone) {
    await rest(200);
    const shape = await after(paged, () => invoke("pages_shape", { key: paged.key })).catch(() => null);
    if (!shape) {
      return;
    }
    const s = paged.session;
    paged.indexed = shape.indexed;
    s.window.after = Math.max(0, shape.lines - s.base - s.doc.count);
    s.view?.size();
    s.view?.schedule();
  }
}

// The file's line at the top of the view of `view` on the paged session `s`, as its last scroll read it: the line its
// top row shows of the window's, its rows wrapped and folded as the view draws them, and outside the window a line a
// row, as the space pads them.
const topOf = (s, view) => {
  const row = Math.floor(view.scrollYKnown() / view.lineHeight);
  const rows = view.rows();
  if (row < 0) {
    return s.base + row;
  }
  return row < rows.size ? s.base + rows.lineOf(row) : s.base + s.doc.count + (row - rows.size);
};

// Tells orior's side where the view of `view` on the paged session `s` stands as it moves, and once it has stood CALM
// milliseconds that it rests, the lines it moved counted.
export function follow(s, view) {
  const paged = s?.window?.paged;
  const top = paged ? topOf(s, view) : null;
  if (!paged || paged.gone || view.s !== s || top === paged.told) {
    return;
  }
  paged.moved += Math.abs(top - (paged.told ?? top));
  paged.resting = false;
  window.clearTimeout(paged.calm);
  paged.calm = window.setTimeout(() => restAgain(s, view), CALM);
  report(s, view);
}

// The view of `view` on the paged session `s` told as resting; and where it moved MOVED_GIVEN lines since the web view
// last gave its memory back, the web view's memory target held low LOW_FOR milliseconds, so it gives back what the
// drawing on the way held.
function restAgain(s, view) {
  const paged = s.window.paged;
  paged.resting = true;
  report(s, view);
  if (paged.moved >= MOVED_GIVEN) {
    paged.moved = 0;
    invoke("memory_target", { low: true })
      .then(() => rest(LOW_FOR))
      .then(() => invoke("memory_target", { low: false }))
      .catch(() => {});
  }
}

// Whether the window of the paged session `s` stands as orior's side placed it for a view at rest, nothing in flight.
export const settled = (s) => {
  const paged = s.window.paged;
  return paged.resting && !paged.flying && !paged.wanted;
};

// Tells orior's side where the view of `view` on the paged session `s` stands: its top, its rows, how far across it
// reaches, the lines the minimap stands beside, the lines the window holds and their cut, and the letters a row holds
// where its lines wrap, 0 where they do not, by which orior's side answers the lines its rows take. FLYING are in
// flight at the most, and one more is sent as one lands where one was wanted meanwhile. The newest answer to land takes
// the window, unless the file was edited or the window jumped since it was sent; and at rest, an answer that brought
// lines in is followed by another until the window holds what the view keeps.
function report(s, view) {
  const paged = s.window.paged;
  if (paged.gone || view.s !== s) {
    return;
  }
  if (paged.flying >= FLYING) {
    paged.wanted = true;
    return;
  }
  paged.wanted = false;
  const top = topOf(s, view);
  const rows = Math.ceil(view.viewSize().height / view.lineHeight) + 1;
  const [mapFrom, mapTo] = view.minimap?.fileSpan?.() ?? [top, top + rows];
  const told = { key: paged.key, top, rows, columns: columnsOf(s), map_from: Math.max(0, mapFrom), map_to: Math.max(0, mapTo), held_from: s.base, held_to: s.base + s.doc.count, held_cut: s.window.cut, width: view.wrapOn ? view.wrapColumns() : 0 };
  paged.told = top;
  const sent = ++paged.sent;
  const stood = standing(paged);
  paged.flying += 1;
  after(paged, () => invoke("pages_view", told))
    .then((answer) => {
      if (sent > paged.taken && stands(paged, stood)) {
        paged.taken = sent;
        took(s, answer);
        paged.wanted = paged.wanted || (paged.resting && Boolean(answer.lines?.length));
      }
    })
    .catch(() => {})
    .finally(() => {
      paged.flying -= 1;
      if (paged.wanted) {
        report(s, view);
      }
    });
}

// The answer to a report put into the window of the paged session `s`: lines that meet or cross the window's ends grow
// it by those past them, the window cut where they are, as an answer to a report sent before the window last grew
// does; lines apart from it, or whole lines for a window of cut ones, take its place, the view left where it stands;
// and the lines outside those orior's side keeps let go.
function took(s, answer) {
  const lines = answer.lines ?? [];
  const cut = answer.cut ?? 0;
  if (lines.length) {
    const from = answer.from;
    const to = from + lines.length;
    const end = s.base + s.doc.count;
    const base = s.base;
    if (from <= end && to >= base && !(cut === 0 && s.window.cut !== 0)) {
      if (to > end) {
        s.window.cut = Math.max(s.window.cut, cut);
        s.window.after = Math.max(0, s.window.after - (to - end));
        s.growLines(lines.slice(end - from), false, answer.colors);
      }
      if (from < base) {
        s.window.cut = Math.max(s.window.cut, cut);
        s.growLines(lines.slice(0, base - from), true, answer.colors);
      }
    } else {
      const total = end + s.window.after;
      s.window.cut = cut;
      s.window.lengths = cut ? answer.lengths ?? null : null;
      s.window.after = Math.max(0, total - answer.from - lines.length);
      s.replaceWindow(lines, answer.from, null, answer.colors);
    }
  }
  letGo(s, answer.keep);
}

// Lets go of the window's lines outside the file's lines `keep`, from and to, those above and those below, unless the
// history reaches them while the window holds fewer than HARD.
function letGo(s, keep) {
  if (!keep) {
    return;
  }
  const above = Math.max(0, Math.min(s.doc.count, keep[0] - s.base));
  if (above && !(s.doc.count < HARD && s.doc.reaches((line) => line < above))) {
    s.shrink(above, true);
  }
  const below = Math.max(0, Math.min(s.doc.count, s.base + s.doc.count - keep[1]));
  if (below && !(s.doc.count < HARD && s.doc.reaches((line) => line >= s.doc.count - below))) {
    s.window.after += below;
    s.shrink(below, false);
  }
}

// Puts the window of the paged session `s` about the file's line `line`, where it is outside it: STEP lines whole, NEAR
// of them above it, `line` put at the top of the view; the window grows from there as the view rests.
export async function jumpPaged(s, line) {
  const paged = s.window.paged;
  const total = s.base + s.doc.count + s.window.after;
  const from = Math.max(0, Math.min(line - NEAR, total - 1));
  const to = Math.min(total, from + STEP);
  // answers to reports sent before the jump land on a window they are no longer for
  paged.jumps += 1;
  const read = await along(paged, () => invoke("pages_read", { key: paged.key, from, to, columns: columnsOf(s) }));
  s.window.cut = 0;
  s.window.lengths = null;
  s.window.after = Math.max(0, total - from - read.lines.length);
  s.replaceWindow(read.lines, from, line - from, read.colors);
  if (s.view) {
    paged.told = null;
    restAgain(s, s.view);
  }
}

// Whether the file's line `line` lies in the window of the paged session `s`.
export const inWindow = (s, line) => line >= s.base && line < s.base + s.doc.count;

// The paged session `s`'s file's lines from `from` to before `to`, as the tree's side holds them once
// every edit before has gone there.
export function readPaged(s, from, to) {
  const paged = s.window.paged;
  return after(paged, () => invoke("pages_read", { key: paged.key, from, to })).then((read) => read.lines);
}

// The matches find gives for the paged session `s`'s whole file on the tree's side, once every edit before has gone
// there: `sought` as the find bar sets it, at most `most`, each [line, from col, to col] in the file's lines.
export function findPaged(s, sought, most) {
  const paged = s.window.paged;
  return after(paged, () => invoke("pages_find", { key: paged.key, sought, most }));
}

// Saves a paged file on the tree's side, once every edit before it has gone there.
export function savePaged(s) {
  const paged = s.window.paged;
  return along(paged, () => invoke("pages_save", { key: paged.key }));
}

// Lets go of a paged file on the tree's side, as its tab closes.
export function releasePaged(s) {
  const paged = s?.window?.paged;
  if (!paged) {
    return;
  }
  paged.gone = true;
  window.clearTimeout(paged.calm);
  paged.unwatch();
  along(paged, () => invoke("pages_release", { key: paged.key })).catch(() => {});
}

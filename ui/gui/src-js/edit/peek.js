// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { element, state } from "../edit.js";
import { markChanges } from "./mirror.js";
import { jumpTo } from "./places.js";
import { tabOf } from "./tabs.js";

// The change peek. A scroll in the first PEEK_SETTLES milliseconds after it shows brought its change
// into sight, and leaves it.

export const PEEK_SETTLES = 400;

// The change a line of the text is in, or for lines taken out, the one above the line.
export function hunkAt(s, line) {
  return s.changes?.hunks.find((hunk) => (hunk.now[0] === hunk.now[1] ? hunk.now[0] === line : line >= hunk.now[0] && line < hunk.now[1])) ?? null;
}

export function closePeek() {
  state.peek?.remove();
  state.peek = null;
}

// Puts a change's lines back as the last commit had them, as one edit undo takes back.
function revertHunk(hunk) {
  const s = state.editor.s;
  const doc = s.doc;
  const [first, end] = hunk.now;
  const then = s.changesThen.slice(hunk.then[0], hunk.then[1]);
  const lineEnd = (line) => ({ line, col: doc.line(line).length });
  let edit;
  if (end > first && then.length) {
    edit = { from: { line: first, col: 0 }, to: lineEnd(end - 1), text: then.join("\n") };
  } else if (end > first) {
    edit = end < doc.count ? { from: { line: first, col: 0 }, to: { line: end, col: 0 }, text: "" } : { from: first > 0 ? lineEnd(first - 1) : { line: 0, col: 0 }, to: lineEnd(end - 1), text: "" };
  } else {
    edit = first < doc.count ? { from: { line: first, col: 0 }, to: { line: first, col: 0 }, text: `${then.join("\n")}\n` } : { from: lineEnd(first - 1), to: lineEnd(first - 1), text: `\n${then.join("\n")}` };
  }
  closePeek();
  state.editor.change([edit], "revert");
  state.editor.focus();
  markChanges(tabOf(state.active));
}

// Goes to the change `by` changes from this one, the first after the last and the last before the
// first, and shows it where `peek` is set.
export function stepChange(by, peek = false) {
  const s = state.editor?.s;
  const hunks = s?.changes?.hunks ?? [];
  if (!hunks.length) {
    return;
  }
  const line = state.editor.head().line;
  let at;
  if (by > 0) {
    at = hunks.findIndex((hunk) => hunk.now[0] > line);
    at = at < 0 ? 0 : at;
  } else {
    at = hunks.findLastIndex((hunk) => Math.max(hunk.now[0], hunk.now[1] - 1) < line && hunk.now[0] < line);
    at = at < 0 ? hunks.length - 1 : at;
  }
  const hunk = hunks[at];
  jumpTo(hunk.now[0]);
  if (peek) {
    showPeek(hunk.now[0]);
  }
}

export function showPeek(line) {
  const s = state.editor?.s;
  const hunk = s && hunkAt(s, line);
  if (!hunk || !s.changesThen) {
    return;
  }
  closePeek();
  const hunks = s.changes.hunks;
  const index = hunks.indexOf(hunk);
  const button = (text, title, run) => {
    const made = element("button", { type: "button", className: "peek-button", textContent: text, title });
    made.addEventListener("click", run);
    return made;
  };
  const go = (by) => () => {
    const next = hunks[(index + by + hunks.length) % hunks.length];
    jumpTo(next.now[0]);
    showPeek(next.now[0]);
  };
  const lines = [
    ...s.changesThen.slice(hunk.then[0], hunk.then[1]).map((text) => element("div", { className: "peek-line removed", textContent: text || " " })),
    ...s.doc.lines.slice(hunk.now[0], hunk.now[1]).map((text) => element("div", { className: "peek-line added", textContent: text || " " })),
  ];
  const panel = element(
    "div",
    { className: "peek", role: "dialog", ariaLabel: `Change ${index + 1} of ${hunks.length}` },
    element(
      "div",
      { className: "peek-head" },
      element("span", { className: "peek-title", textContent: `Change ${index + 1} of ${hunks.length}` }),
      button("Revert", "Revert Change", () => revertHunk(hunk)),
      button("↑", "Previous Change", go(-1)),
      button("↓", "Next Change", go(1)),
      button("×", "Close", () => {
        closePeek();
        state.editor.focus();
      })
    ),
    element("div", { className: "peek-lines" }, ...lines)
  );
  const host = document.getElementById("editor");
  panel.dataset.line = String(line);
  host.append(panel);
  state.peek = panel;
  state.peekAt = performance.now();
  const box = host.getBoundingClientRect();
  const last = hunk.now[1] > hunk.now[0] ? hunk.now[1] - 1 : Math.max(0, hunk.now[0] - 1);
  const below = state.editor.rectOf({ line: last, col: 0 }).bottom - box.top;
  const above = state.editor.rectOf({ line: hunk.now[0], col: 0 }).top - box.top;
  panel.style.left = `${state.editor.gutter.offsetWidth}px`;
  panel.style.top = `${below + panel.offsetHeight <= host.clientHeight ? below : Math.max(0, above - panel.offsetHeight)}px`;
}


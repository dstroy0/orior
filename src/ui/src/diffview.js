// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A file's changes side by side, over the editor: the last commit's text on the left and the text as
// it stands on the right, each with its line numbers, lines the two share level with each other.
// A line changed is marked on both sides, a line added on the right with an empty row across from it
// on the left, and a line taken out the other way. The bar over it names the file, steps to the
// next change and the one before (F7 and Shift+F7), and closes it, as Escape does. Its box sets
// changes to white space aside, lines that differ only in it standing level as the same, and the
// choice is kept under orior.diff-space.
//
// Its second box compares by structure, in the app's Rust side, and the choice is kept under
// orior.diff-structure. Lines then stand level by the tokens they hold, a line moved is marked as
// moved on both sides, a name changed to one other name everywhere as renamed, and a line whose
// tokens are the same and only its layout changed as reshaped; within a line, each token taken out or
// put in is marked, and the bar's second line says what was renamed, moved and reshaped.

import { invoke } from "./bridge.js";
import { escapeHtml } from "./editor/view.js";
import { lineChanges } from "./editor/diff.js";
import { icon } from "./icons.js";

const state = { node: null, hunks: [], at: -1, rows: null, ticket: 0 };

const SPACE_KEY = "orior.diff-space";
const STRUCTURE_KEY = "orior.diff-structure";

// What a line of a structural comparison is, by the number the Rust side gives it.
const KINDS = ["same", "changed", "moved", "reshaped", "renamed"];

// A line's text with each of its marked tokens, `[from, to, kind]`, in a span of its kind.
function markedText(text, marks = []) {
  let html = "";
  let at = 0;
  for (const [from, to, kind] of marks) {
    html += `${escapeHtml(text.slice(at, from))}<span class="diff-token ${KINDS[kind]}">${escapeHtml(text.slice(from, to))}</span>`;
    at = to;
  }
  return html + escapeHtml(text.slice(at));
}

// The marks of a side by line.
function marksByLine(marks) {
  const lines = new Map();
  for (const [line, from, to, kind] of marks) {
    lines.set(line, [...(lines.get(line) ?? []), [from, to, kind]]);
  }
  return lines;
}

// What a structural comparison found, in words: each name renamed, and how many lines moved and
// reshaped.
function summaryOf(found) {
  const parts = found.renames.map(([from, to, count]) => `${from} renamed ${to}${count > 1 ? ` in ${count} places` : ""}`);
  if (found.moved) {
    parts.push(`${found.moved} line${found.moved === 1 ? "" : "s"} moved`);
  }
  if (found.reshaped) {
    parts.push(`${found.reshaped} line${found.reshaped === 1 ? "" : "s"} reshaped`);
  }
  return parts.join("; ");
}

// The rows of a structural comparison: each side's cell tinted by what its line is, its tokens marked.
function structureHtml(found, old, fresh) {
  const left = marksByLine(found.left_marks);
  const right = marksByLine(found.right_marks);
  let html = "";
  found.rows.forEach(([from, to], at) => {
    const side = (lines, line, kinds, marks, which) =>
      line === null ? `<span class="diff-num"></span><span class="diff-text diff-gap ${which}"></span>` : `<span class="diff-num">${line + 1}</span><span class="diff-text ${which} line-${KINDS[kinds[line]]}">${markedText(lines[line], marks.get(line)) || " "}</span>`;
    html += `<div class="diff-row by-structure" data-row="${at}">${side(old, from, found.left, left, "left")}${side(fresh, to, found.right, right, "right")}</div>`;
  });
  return html;
}

// A line as it is compared where white space is set aside: each run of it one space, none at the ends.
const loose = (line) => line.replace(/\s+/g, " ").trim();

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

// The rows of the two texts side by side: each [left line or null, right line or null, kind], kind
// "same", "changed", "added" or "removed", and where each change starts.
function rowsOf(then, now) {
  const aside = localStorage.getItem(SPACE_KEY) === "aside";
  const marks = aside ? lineChanges(then.map(loose), now.map(loose)) : lineChanges(then, now);
  if (!marks) {
    return null;
  }
  const rows = [];
  const starts = [];
  let left = 0;
  let right = 0;
  const same = (to) => {
    while (right < to) {
      rows.push([left, right, "same"]);
      left += 1;
      right += 1;
    }
  };
  for (const hunk of marks.hunks) {
    same(hunk.now[0]);
    starts.push(rows.length);
    const gone = hunk.then[1] - hunk.then[0];
    const come = hunk.now[1] - hunk.now[0];
    for (let at = 0; at < Math.max(gone, come); at += 1) {
      const from = at < gone ? left + at : null;
      const to = at < come ? right + at : null;
      rows.push([from, to, from !== null && to !== null ? "changed" : from === null ? "added" : "removed"]);
    }
    left += gone;
    right += come;
  }
  same(now.length);
  while (left < then.length) {
    rows.push([left, null, "removed"]);
    left += 1;
  }
  return { rows, starts };
}

export function closeDiff() {
  state.ticket += 1;
  state.node?.remove();
  state.node = null;
}

function step(direction) {
  if (!state.starts?.length) {
    return;
  }
  state.at = (state.at + direction + state.starts.length) % state.starts.length;
  const row = state.node.querySelector(`.diff-row[data-row="${state.starts[state.at]}"]`);
  row?.scrollIntoView({ block: "center" });
  state.node.querySelector(".diff-where").textContent = `change ${state.at + 1} of ${state.starts.length}`;
}

// Shows `path`'s changes from `then`, the last commit's text or null where it holds none, to `now`,
// over `host`. `sides` names the two texts where the left one is not the last commit, and `same` says
// they do not differ.
export async function showDiff(host, path, then, now, { sides = null, same = "No change from the last commit." } = {}) {
  state.ticket += 1;
  const ticket = state.ticket;
  const structured = localStorage.getItem(STRUCTURE_KEY) === "on";
  const old = (then ?? "").split(/\r?\n/);
  const fresh = now.split(/\r?\n/);
  const found = structured ? await invoke("structure_compare", { then, now }).catch(() => null) : rowsOf(then === null ? [] : old, fresh);
  // A comparison overtaken by another, or by the view closing, shows nothing.
  if (ticket !== state.ticket) {
    return;
  }
  closeDiff();
  const button = (glyph, label, run) => {
    const made = element("button", { className: "diff-button", type: "button", title: label }, icon(glyph));
    made.setAttribute("aria-label", label);
    made.addEventListener("click", run);
    return made;
  };
  const bar = element(
    "div",
    { className: "diff-bar" },
    element("span", { className: "diff-name", textContent: path }),
    element("span", { className: "diff-sides", textContent: sides ?? (then === null ? "not in the last commit" : "last commit, then as it stands") }),
    element("span", { className: "diff-where" }),
    button("chevron", "Next Change (F7)", () => step(1)),
    button("chevron", "Previous Change (Shift+F7)", () => step(-1)),
    button("close", "Close (Escape)", closeDiff),
  );
  bar.children[4].classList.add("diff-up");
  // Structure sets white space aside on its own, and its box stands unused while structure is chosen.
  const space = element("input", { type: "checkbox", checked: localStorage.getItem(SPACE_KEY) === "aside", disabled: structured });
  space.addEventListener("change", () => {
    localStorage.setItem(SPACE_KEY, space.checked ? "aside" : "shown");
    showDiff(host, path, then, now, { sides, same });
  });
  const structure = element("input", { type: "checkbox", checked: structured });
  structure.addEventListener("change", () => {
    localStorage.setItem(STRUCTURE_KEY, structure.checked ? "on" : "off");
    showDiff(host, path, then, now, { sides, same });
  });
  bar.children[2].after(element("label", { className: "diff-space" }, space, element("span", { textContent: "Set white space aside" })), element("label", { className: "diff-space diff-structure" }, structure, element("span", { textContent: "Compare by structure" })));
  const body = element("div", { className: "diff-body" });
  if (!found) {
    body.append(element("p", { className: "diff-empty", textContent: "The two texts are too far apart to set side by side." }));
  } else if (!found.starts.length) {
    body.append(element("p", { className: "diff-empty", textContent: same }));
  } else if (structured) {
    body.innerHTML = structureHtml(found, old, fresh);
  } else {
    let html = "";
    found.rows.forEach(([left, right, kind], at) => {
      const side = (lines, line, which) =>
        line === null ? `<span class="diff-num"></span><span class="diff-text diff-gap ${which}"></span>` : `<span class="diff-num">${line + 1}</span><span class="diff-text ${which}">${escapeHtml(lines[line]) || " "}</span>`;
      html += `<div class="diff-row ${kind}" data-row="${at}">${side(old, left, "left")}${side(fresh, right, "right")}</div>`;
    });
    body.innerHTML = html;
  }
  const said = structured && found ? summaryOf(found) : "";
  const node = element("section", { className: "diff-view" }, bar, said ? element("p", { className: "diff-summary", textContent: said }) : null, body);
  node.setAttribute("aria-label", `Changes to ${path}`);
  node.tabIndex = -1;
  node.addEventListener("keydown", (event) => {
    if (event.key === "Escape") {
      event.preventDefault();
      closeDiff();
    } else if (event.key === "F7") {
      event.preventDefault();
      step(event.shiftKey ? -1 : 1);
    }
  });
  host.append(node);
  state.node = node;
  state.starts = found?.starts ?? [];
  state.at = -1;
  node.focus();
  if (state.starts.length) {
    step(1);
  }
}

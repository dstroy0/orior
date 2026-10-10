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
//
// Where the right side is a file's, its review comments stand under their lines, a press on a right
// line's number leaves one, and each comment's buttons resolve it, open it again or delete it.

import { invoke } from "./bridge.js";
import { escapeHtml } from "./editor/view.js";
import { lineChanges } from "./editor/diff.js";
import { icon } from "./icons.js";
import { addComment, anchor, comments, onReview, removeComment, resolveComment } from "./review.js";

// `redraw` shows the view again as it was opened, at the place it was scrolled to.
const state = { node: null, hunks: [], at: -1, rows: null, ticket: 0, redraw: null };

onReview(() => state.node && state.redraw?.());

// The review comments of a file by the line of `lines` each stands at.
function commentsAt(path, lines) {
  const at = new Map();
  for (const comment of comments().filter((one) => one.path === path)) {
    const { line, gone } = anchor(comment, lines);
    at.set(line, [...(at.get(line) ?? []), { comment, gone }]);
  }
  return at;
}

const stamp = (ms) => {
  const date = new Date(ms);
  const pad = (n) => String(n).padStart(2, "0");
  return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())} ${pad(date.getHours())}:${pad(date.getMinutes())}`;
};

// The rows of the comments on a line, under the line's row.
function commentsHtml(list = []) {
  return list
    .map(
      ({ comment, gone }) =>
        `<div class="diff-comment${comment.done ? " done" : ""}${gone ? " gone" : ""}" data-comment="${comment.id}"><span class="diff-comment-body">${escapeHtml(comment.body)}</span>${gone ? '<span class="diff-comment-when">its line is gone</span>' : ""}<span class="diff-comment-when">${stamp(comment.made)}</span><button type="button" class="diff-comment-act" data-act="${comment.done ? "reopen" : "resolve"}">${comment.done ? "Reopen" : "Resolve"}</button><button type="button" class="diff-comment-act" data-act="delete">Delete</button></div>`,
    )
    .join("");
}

// The number cell of a line on the right, which a press leaves a comment on where the view is a file's.
const rightNumber = (line, review) => `<span class="diff-num${review ? " can-comment" : ""}" data-line="${line}"${review ? ' title="Comment on this line"' : ""}>${line + 1}</span>`;

// A field under a line's row for a comment on it, saved with Ctrl+Enter or its button.
function openNote(row, line, review, lines) {
  if (row.nextElementSibling?.classList.contains("diff-comment-new")) {
    row.nextElementSibling.querySelector("textarea").focus();
    return;
  }
  const field = Object.assign(document.createElement("textarea"), { className: "diff-comment-field", rows: 2, placeholder: `Comment on line ${line + 1}` });
  field.setAttribute("aria-label", `Comment on line ${line + 1}`);
  const save = Object.assign(document.createElement("button"), { type: "button", className: "diff-comment-act primary-ish", textContent: "Comment" });
  const cancel = Object.assign(document.createElement("button"), { type: "button", className: "diff-comment-act", textContent: "Cancel" });
  const note = element("div", { className: "diff-comment diff-comment-new" }, field, save, cancel);
  const keepIt = () => field.value.trim() && addComment(review, line, lines[line] ?? "", field.value.trim());
  save.addEventListener("click", keepIt);
  cancel.addEventListener("click", () => note.remove());
  field.addEventListener("keydown", (event) => {
    if (event.key === "Enter" && event.ctrlKey) {
      event.preventDefault();
      keepIt();
    } else if (event.key === "Escape") {
      event.preventDefault();
      event.stopPropagation();
      note.remove();
    }
  });
  row.after(note);
  field.focus();
}

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

// The rows of a structural comparison: each side's cell tinted by what its line is, its tokens marked,
// and each right line's review comments under its row.
function structureHtml(found, old, fresh, review, notes) {
  const left = marksByLine(found.left_marks);
  const right = marksByLine(found.right_marks);
  let html = "";
  found.rows.forEach(([from, to], at) => {
    const side = (lines, line, kinds, marks, which) =>
      line === null
        ? `<span class="diff-num"></span><span class="diff-text diff-gap ${which}"></span>`
        : `${which === "right" ? rightNumber(line, review) : `<span class="diff-num">${line + 1}</span>`}<span class="diff-text ${which} line-${KINDS[kinds[line]]}">${markedText(lines[line], marks.get(line)) || " "}</span>`;
    html += `<div class="diff-row by-structure" data-row="${at}">${side(old, from, found.left, left, "left")}${side(fresh, to, found.right, right, "right")}</div>`;
    if (to !== null) {
      html += commentsHtml(notes.get(to));
    }
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
// they do not differ. `review` is the file of the tree whose review comments stand under the right
// side's lines and are left there, where the right side is a file's; `scroll` is where the view opens
// scrolled to, where it is shown again.
export async function showDiff(host, path, then, now, { sides = null, same = "No change from the last commit.", review = null, scroll = null } = {}) {
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
    showDiff(host, path, then, now, { sides, same, review });
  });
  const structure = element("input", { type: "checkbox", checked: structured });
  structure.addEventListener("change", () => {
    localStorage.setItem(STRUCTURE_KEY, structure.checked ? "on" : "off");
    showDiff(host, path, then, now, { sides, same, review });
  });
  bar.children[2].after(element("label", { className: "diff-space" }, space, element("span", { textContent: "Set white space aside" })), element("label", { className: "diff-space diff-structure" }, structure, element("span", { textContent: "Compare by structure" })));
  const body = element("div", { className: "diff-body" });
  const notes = review ? commentsAt(review, fresh) : new Map();
  if (!found) {
    body.append(element("p", { className: "diff-empty", textContent: "The two texts are too far apart to set side by side." }));
  } else if (!found.starts.length) {
    body.append(element("p", { className: "diff-empty", textContent: same }));
  } else if (structured) {
    body.innerHTML = structureHtml(found, old, fresh, review, notes);
  } else {
    let html = "";
    found.rows.forEach(([left, right, kind], at) => {
      const side = (lines, line, which) =>
        line === null
          ? `<span class="diff-num"></span><span class="diff-text diff-gap ${which}"></span>`
          : `${which === "right" ? rightNumber(line, review) : `<span class="diff-num">${line + 1}</span>`}<span class="diff-text ${which}">${escapeHtml(lines[line]) || " "}</span>`;
      html += `<div class="diff-row ${kind}" data-row="${at}">${side(old, left, "left")}${side(fresh, right, "right")}</div>`;
      if (right !== null) {
        html += commentsHtml(notes.get(right));
      }
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
  // A press on a right line's number leaves a comment on it, and a comment's buttons resolve, open
  // again or delete it.
  body.addEventListener("click", (event) => {
    const number = event.target.closest(".diff-num.can-comment");
    const act = event.target.closest(".diff-comment [data-act]");
    if (number) {
      openNote(number.closest(".diff-row"), Number(number.dataset.line), review, fresh);
    } else if (act) {
      const id = act.closest(".diff-comment").dataset.comment;
      if (act.dataset.act === "delete") {
        removeComment(id);
      } else {
        resolveComment(id, act.dataset.act === "resolve");
      }
    }
  });
  host.append(node);
  state.node = node;
  state.redraw = () => showDiff(host, path, then, now, { sides, same, review, scroll: body.scrollTop });
  state.starts = found?.starts ?? [];
  state.at = -1;
  node.focus();
  if (scroll !== null) {
    body.scrollTop = scroll;
  } else if (state.starts.length) {
    step(1);
  }
}

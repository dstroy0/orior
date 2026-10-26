// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { comments } from "../review.js";
import { act, element, paneOpen, state } from "../explorer.js";
import { iconOf } from "./icons.js";

// Review: every review comment of the tree, a row for each file and under it a row a comment, the
// open ones before the resolved ones. A press opens the file at the comment's line, and a comment's
// menu shows the file's changes, resolves or opens it again, or deletes it.
export function drawReview() {
  const body = document.getElementById("review");
  if (!paneOpen("review")) {
    return;
  }
  const files = new Map();
  for (const one of comments().sort((a, b) => Number(a.done) - Number(b.done) || a.line - b.line)) {
    files.set(one.path, [...(files.get(one.path) ?? []), one]);
  }
  const rows = [];
  for (const [path, items] of [...files].sort(([a], [b]) => a.localeCompare(b))) {
    const cut = path.lastIndexOf("/");
    const open = items.filter((one) => !one.done).length;
    rows.push(element("div", { className: "node problem-file" }, iconOf(path.slice(cut + 1)), element("span", { className: "name", textContent: path.slice(cut + 1) }), element("span", { className: "where", textContent: path.slice(0, Math.max(0, cut)) }), element("span", { className: "count", textContent: String(open) })));
    for (const one of items) {
      const row = element("button", { className: `problem review-row${one.done ? " done" : ""}`, type: "button", title: `${path}:${one.line + 1}\n${one.body}` });
      row.dataset.key = `review:${one.id}`;
      row.dataset.depth = "1";
      row.dataset.comment = one.id;
      row.append(element("i", { className: "guide" }), element("span", { className: "review-mark" }), element("span", { className: "name", textContent: one.body.split("\n")[0] }), element("span", { className: "where", textContent: String(one.line + 1) }));
      row.addEventListener("click", () => state.hooks.openComment(one));
      rows.push(row);
    }
  }
  body.replaceChildren(...(rows.length ? rows : [element("p", { className: "pane-empty", textContent: "No review comments. A press on a line's number in a file's changes leaves one." })]));
}

export function reviewItems(event) {
  const id = event.target.closest(".review-row")?.dataset.comment;
  const one = comments().find((comment) => comment.id === id);
  if (!one) {
    return null;
  }
  return [
    act("comment-open", [one.id], { label: "Open" }),
    act("comment-changes", [one.id], { label: "Show Changes" }),
    "-",
    act("comment-resolve", [one.id, one.done ? "open" : "done"], { label: one.done ? "Reopen" : "Resolve" }),
    act("comment-delete", [one.id], { label: "Delete" }),
    "-",
    act("comment-copy", [one.id], { label: "Copy Comment" }),
  ];
}

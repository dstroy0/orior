// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { invoke } from "../bridge.js";
import { timeLeft } from "../progress.js";
import { element, paneOpen, state } from "../explorer.js";
import { iconOf } from "./icons.js";

// Problems: each file's diagnostics, a row a file and under it a row a diagnostic, the worst first,
// the open files first; and, while the tree's check goes on, how far it has gone. A file's row opens
// and closes its diagnostics, which show for a file open in the editor until its row closes them.

export function drawProblems(files, checking) {
  const body = document.getElementById("problems");
  if (!paneOpen("problems") || state.group !== "problems") {
    return;
  }
  const rows = [];
  if (checking && checking.done < checking.total) {
    state.checkBegan ??= performance.now();
    const left = timeLeft(performance.now() - state.checkBegan, checking.done / Math.max(1, checking.total));
    // Stop lets the files waiting go; files that change after are checked as they change.
    const stop = element("button", { type: "button", className: "pulse-stop", textContent: "Stop" });
    stop.addEventListener("click", () => invoke("problems_check_stop").catch(() => {}));
    rows.push(element("p", { className: "pane-empty problems-checking" }, `Checking the tree: ${checking.done} of ${checking.total} files${left ? `, ${left}` : ""}. `, stop));
  } else {
    state.checkBegan = null;
  }
  for (const file of files) {
    if (!file.items.length) {
      continue;
    }
    const open = state.problems.opened.has(file.path) || (file.open && !state.problems.closed.has(file.path));
    rows.push(problemFileRow(file, open));
    if (open) {
      rows.push(...problemRows(file));
    }
  }
  body.replaceChildren(...(rows.length ? rows : [element("p", { className: "pane-empty", textContent: "No problems in the tree. A language server or Run, Validate finds them." })]));
}

// A diagnostic's first line as its row shows it: where the server writes Markdown, its code spans
// set as code.
function firstLine(item) {
  const line = item.message.split("\n")[0];
  if (!item.markdown) {
    return [line];
  }
  return line.split(/(`[^`]*`)/).filter(Boolean).map((piece) => (/^`[^`]*`$/.test(piece) ? element("code", { textContent: piece.slice(1, -1) }) : piece));
}

function problemFileRow(file, open) {
  const { path, items } = file;
  const cut = path.lastIndexOf("/");
  const row = element("button", { className: "node dir problem-file", type: "button" }, element("span", { className: "twisty" }), iconOf(path.slice(cut + 1)), element("span", { className: "name", textContent: path.slice(cut + 1) }), element("span", { className: "where", textContent: path.slice(0, Math.max(0, cut)) }), element("span", { className: "count", textContent: String(items.length) }));
  row.dataset.key = `problem-file:${path}`;
  row.dataset.depth = "0";
  row.setAttribute("aria-expanded", String(open));
  row.addEventListener("click", () => {
    const opening = row.getAttribute("aria-expanded") !== "true";
    state.problems.opened[opening ? "add" : "delete"](path);
    state.problems.closed[opening ? "delete" : "add"](path);
    row.setAttribute("aria-expanded", String(opening));
    if (opening) {
      row.after(...problemRows(file));
    } else {
      while (row.nextElementSibling?.classList.contains("problem")) {
        row.nextElementSibling.remove();
      }
    }
  });
  return row;
}

function problemRows({ path, items }) {
  return [...items]
    .sort((a, b) => a.severity - b.severity || a.from.line - b.from.line)
    .map((item) => {
      const row = element("button", { className: `problem s${item.severity}`, type: "button", title: `${path}:${item.from.line + 1}:${item.from.col + 1}\n${item.message}` });
      row.dataset.key = `problem:${path}:${item.from.line}:${item.from.col}`;
      row.dataset.depth = "1";
      row.append(element("i", { className: "guide" }), element("span", { className: "problem-mark" }), element("span", { className: "name" }, ...firstLine(item)), element("span", { className: "where", textContent: `${item.from.line + 1}:${item.from.col + 1}` }));
      row.addEventListener("click", () => state.hooks.openAt(path, item.from.line, item.from.col));
      return row;
    });
}

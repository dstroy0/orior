// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Conflicts: a file a merge left in conflict, in a window of three panes over the editor: the text as
// the branch open has it on the left, the result in the middle, and the text the merge brought in on
// the right, each stretch the three share level across them. Each conflict takes its lines in the
// middle from either side, from both one after the other, or as written by hand, and once none is
// left open Mark Resolved writes the result and stages the file. The bar steps from conflict to
// conflict (F7 and Shift+F7) and closes the window, as Escape does.
//
// The file's conflicts are read from the marks git writes in it: <<<<<<< before the branch open's
// lines, ||||||| before the lines both came from where git writes them, ======= before the lines the
// merge brought in, and >>>>>>> after them.

import { escapeHtml } from "./editor/view.js";
import { icon } from "./icons.js";

const state = { node: null, merge: null, at: -1 };

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

// The file's parts in order: each stretch all three share, and each conflict with the lines of each
// side, the branch names git wrote, and how it is settled. A conflict git did not close stays text.
export function conflictsOf(text) {
  const lines = text.split(/\r?\n/);
  const parts = [];
  let plain = [];
  let at = 0;
  while (at < lines.length) {
    if (!lines[at].startsWith("<<<<<<<")) {
      plain.push(lines[at]);
      at += 1;
      continue;
    }
    const conflict = { ours: [], base: null, theirs: [], oursName: lines[at].slice(7).trim(), theirsName: "", choice: null, written: null };
    let side = "ours";
    let end = at + 1;
    for (; end < lines.length; end += 1) {
      const line = lines[end];
      if (line.startsWith("|||||||") && side === "ours") {
        side = "base";
        conflict.base = [];
      } else if (line === "=======" && side !== "theirs") {
        side = "theirs";
      } else if (line.startsWith(">>>>>>>") && side === "theirs") {
        conflict.theirsName = line.slice(7).trim();
        break;
      } else {
        conflict[side].push(line);
      }
    }
    if (end >= lines.length) {
      plain.push(...lines.slice(at));
      break;
    }
    if (plain.length) {
      parts.push({ plain });
    }
    plain = [];
    parts.push(conflict);
    at = end + 1;
  }
  if (plain.length || !parts.length) {
    parts.push({ plain });
  }
  return { parts, eol: text.includes("\r\n") ? "\r\n" : "\n" };
}

// A conflict's lines in the result, or null while it is open.
function resultOf(conflict) {
  switch (conflict.choice) {
    case "ours":
      return conflict.ours;
    case "theirs":
      return conflict.theirs;
    case "both":
      return [...conflict.ours, ...conflict.theirs];
    case "written":
      return conflict.written;
    default:
      return null;
  }
}

const conflicts = (merge) => merge.parts.filter((part) => !part.plain);

// The result's text, or null while a conflict is open.
export function resultText(merge) {
  const lines = [];
  for (const part of merge.parts) {
    const taken = part.plain ?? resultOf(part);
    if (!taken) {
      return null;
    }
    lines.push(...taken);
  }
  return lines.join(merge.eol);
}

function draw() {
  const merge = state.merge;
  const body = state.node.querySelector(".diff-body");
  const cell = (number, text, which) =>
    number === null ? `<span class="diff-num"></span><span class="diff-text diff-gap ${which}"></span>` : `<span class="diff-num">${number}</span><span class="diff-text ${which}">${escapeHtml(text) || " "}</span>`;
  let [left, middle, right] = [1, 1, 1];
  let html = "";
  conflicts(merge).forEach((conflict, index) => (conflict.index = index));
  for (const part of merge.parts) {
    if (part.plain) {
      for (const line of part.plain) {
        html += `<div class="merge-row">${cell(left++, line, "left")}${cell(middle++, line, "middle")}${cell(right++, line, "right")}</div>`;
      }
      continue;
    }
    const taken = resultOf(part);
    const chosen = (choice) => (part.choice === choice ? ' aria-pressed="true"' : "");
    html += `<div class="merge-head${taken ? "" : " open"}" data-conflict="${part.index}">
      <span class="merge-name">${escapeHtml(part.oursName)}</span>
      <span class="merge-acts"><button type="button" data-take="ours"${chosen("ours")}>« Yours</button><button type="button" data-take="both"${chosen("both")}>Both</button><button type="button" data-take="written"${chosen("written")}>Edit…</button><button type="button" data-take="theirs"${chosen("theirs")}>Theirs »</button></span>
      <span class="merge-name">${escapeHtml(part.theirsName)}</span></div>`;
    if (part.editing) {
      html += `<div class="merge-write" data-conflict="${part.index}"><textarea class="report-field" spellcheck="false" rows="${Math.max(3, part.editing.split("\n").length + 1)}">${escapeHtml(part.editing)}</textarea><button type="button" class="primary" data-take="done">Done</button></div>`;
    }
    const rows = Math.max(part.ours.length, part.theirs.length, taken?.length ?? 0, 1);
    for (let row = 0; row < rows; row += 1) {
      const mine = part.ours[row];
      const theirs = part.theirs[row];
      const kept = taken?.[row];
      const centre = taken ? (kept === undefined ? cell(null, "", "middle") : cell(middle++, kept, "middle taken")) : '<span class="diff-num"></span><span class="diff-text middle merge-open"></span>';
      html += `<div class="merge-row conflict">${mine === undefined ? cell(null, "", "left") : cell(left++, mine, "left ours")}${centre}${theirs === undefined ? cell(null, "", "right") : cell(right++, theirs, "right theirs")}</div>`;
    }
  }
  body.innerHTML = html;
  const open = conflicts(merge).filter((conflict) => !resultOf(conflict)).length;
  state.node.querySelector(".diff-where").textContent = open ? `${open} of ${conflicts(merge).length} conflicts open` : "every conflict settled";
  state.node.querySelector(".merge-done").disabled = open > 0;
  body.querySelector(".merge-write textarea")?.focus();
}

export function closeMerge() {
  state.node?.remove();
  state.node = null;
  state.merge = null;
}

function step(direction) {
  const heads = [...state.node.querySelectorAll(".merge-head")];
  if (!heads.length) {
    return;
  }
  state.at = (state.at + direction + heads.length) % heads.length;
  heads[state.at].scrollIntoView({ block: "center" });
}

// Shows the conflicts of `path`, whose text as it stands is `text`, over `host`. `resolved` is given
// the result once Mark Resolved is pressed, and the window closes once it is done.
export function showMerge(host, path, text, { resolved }) {
  closeMerge();
  const merge = conflictsOf(text);
  const button = (glyph, label, run) => {
    const made = element("button", { className: "diff-button", type: "button", title: label }, icon(glyph));
    made.setAttribute("aria-label", label);
    made.addEventListener("click", run);
    return made;
  };
  const done = element("button", { className: "primary merge-done", type: "button", textContent: "Mark Resolved" });
  done.addEventListener("click", async () => {
    const result = resultText(state.merge);
    if (result !== null) {
      await resolved(result);
      closeMerge();
    }
  });
  const bar = element(
    "div",
    { className: "diff-bar" },
    element("span", { className: "diff-name", textContent: path }),
    element("span", { className: "diff-sides", textContent: "yours, the result, theirs" }),
    element("span", { className: "diff-where" }),
    done,
    button("chevron", "Next Conflict (F7)", () => step(1)),
    button("chevron", "Previous Conflict (Shift+F7)", () => step(-1)),
    button("close", "Close (Escape)", closeMerge),
  );
  bar.children[5].classList.add("diff-up");
  const body = element("div", { className: "diff-body merge-body" });
  body.addEventListener("click", (event) => {
    const take = event.target.closest("[data-take]")?.dataset.take;
    const index = Number(event.target.closest("[data-conflict]")?.dataset.conflict);
    const conflict = conflicts(state.merge)[index];
    if (!take || !conflict) {
      return;
    }
    if (take === "written") {
      conflict.editing = (resultOf(conflict) ?? [...conflict.ours, ...conflict.theirs]).join("\n");
    } else if (take === "done") {
      conflict.written = event.target.closest(".merge-write").querySelector("textarea").value.split(/\r?\n/);
      conflict.choice = "written";
      conflict.editing = null;
    } else {
      conflict.choice = take;
      conflict.editing = null;
    }
    draw();
  });
  const node = element("section", { className: "diff-view merge-view" }, bar, body);
  node.setAttribute("aria-label", `Conflicts in ${path}`);
  node.tabIndex = -1;
  node.addEventListener("keydown", (event) => {
    if (event.key === "Escape" && !event.target.closest("textarea")) {
      event.preventDefault();
      closeMerge();
    } else if (event.key === "F7") {
      event.preventDefault();
      step(event.shiftKey ? -1 : 1);
    }
  });
  host.append(node);
  state.node = node;
  state.merge = merge;
  state.at = -1;
  draw();
  node.focus();
  step(1);
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { invoke } from "../bridge.js";
import { act, element, guides, paneOpen, state } from "../explorer.js";
import { iconOf } from "./icons.js";

// Timeline: the commits that touched the open file, the newest first, each with its date. A commit's
// menu opens the file as it left it, or sets it beside the file as it is or another commit's.

export const day = (when) => {
  const date = new Date(when * 1000);
  const pad = (n) => String(n).padStart(2, "0");
  return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}`;
};

export const time = (when) => {
  const date = new Date(when * 1000);
  return `${String(date.getHours()).padStart(2, "0")}:${String(date.getMinutes()).padStart(2, "0")}`;
};

export async function drawTimeline(path, again = false) {
  const body = document.getElementById("timeline");
  if (!paneOpen("timeline")) {
    state.timeline.path = null;
    return;
  }
  if (path === state.timeline.path && !again) {
    return;
  }
  state.timeline.path = path;
  const commits = path ? await invoke("file_commits", { path }).catch(() => []) : [];
  if (state.timeline.path !== path) {
    return;
  }
  state.timeline.commits = commits;
  body.replaceChildren(
    ...commits.map((commit) => {
      const row = element("button", { className: "commit", type: "button", title: `${commit.id.slice(0, 8)}  ${day(commit.when)} ${time(commit.when)}\n${commit.subject}` });
      row.dataset.key = commit.id;
      row.dataset.depth = "0";
      const dot = element("span", { className: "icon commit-dot" });
      dot.setAttribute("aria-hidden", "true");
      row.append(dot, element("span", { className: "name", textContent: commit.subject }), element("span", { className: "where", textContent: day(commit.when) }));
      row.addEventListener("click", () => state.hooks.openCommit(path, commit));
      return row;
    })
  );
}

export function timelineItems(event) {
  const row = event.target.closest(".commit");
  const commit = state.timeline.commits.find((one) => one.id === row?.dataset.key);
  if (!commit) {
    return null;
  }
  const path = state.timeline.path;
  const others = state.timeline.commits.filter((one) => one !== commit).slice(0, 40);
  return [
    act("commit-open", [path, commit.id], { label: "Open" }),
    "-",
    act("commit-compare", [path, commit.id], { label: "Compare with Current" }),
    { label: "Compare with Revision", disabled: !others.length, items: others.map((one) => act("commit-compare", [path, commit.id, one.id], { label: `${one.id.slice(0, 7)}  ${one.subject}` })) },
    "-",
    act("commit-copy-id", [commit.id], { label: "Copy Commit ID" }),
    act("commit-copy-message", [commit.id, path], { label: "Copy Commit Message" }),
  ];
}

// Local History: the open file as each save left it, the newest first, retained apart from git. A row
// shows its difference from the text as it is; its menu takes the file back to it.

const clock = (ms) => {
  const date = new Date(ms);
  return [date.getHours(), date.getMinutes(), date.getSeconds()].map((n) => String(n).padStart(2, "0")).join(":");
};

const bytes = (size) => (size < 1024 ? `${size} B` : `${(size / 1024).toFixed(1)} KB`);

export async function drawLocalHistory(path, again = false) {
  const body = document.getElementById("local-history");
  if (!paneOpen("local")) {
    state.local.path = null;
    return;
  }
  if (path === state.local.path && !again) {
    return;
  }
  state.local.path = path;
  const snapshots = path ? await invoke("history_list", { path }).catch(() => []) : [];
  if (state.local.path !== path) {
    return;
  }
  state.local.snapshots = snapshots;
  body.replaceChildren(
    ...snapshots.map((snapshot) => {
      const row = element("button", { className: "commit", type: "button", title: `${day(snapshot.at / 1000)} ${clock(snapshot.at)}, ${bytes(snapshot.size)}` });
      row.dataset.key = String(snapshot.at);
      row.dataset.depth = "0";
      const dot = element("span", { className: "icon commit-dot" });
      dot.setAttribute("aria-hidden", "true");
      row.append(dot, element("span", { className: "name", textContent: `${day(snapshot.at / 1000)} ${clock(snapshot.at)}` }), element("span", { className: "where", textContent: bytes(snapshot.size) }));
      row.addEventListener("click", () => state.hooks.showSnapshot(path, snapshot.at));
      return row;
    })
  );
}

// Undo History: every state the open file's text has been in since it opened, the first at the top,
// each by when it was made and the line its step changed. A branch an edit after an undo left is
// a level in, under the state it was made from. The state the text is in is marked, and a press on a
// row takes the text to its state. Drawn at max once a frame.
export function drawUndo(session) {
  state.undo.session = session;
  if (state.undo.wait || !paneOpen("undo")) {
    return;
  }
  state.undo.wait = window.requestAnimationFrame(() => {
    state.undo.wait = 0;
    const displayed = state.undo.session;
    const rows = displayed ? displayed.doc.states() : [];
    const body = document.getElementById("undo-history");
    const focused = body.contains(document.activeElement) ? document.activeElement.dataset.key : null;
    body.replaceChildren(
      ...rows.map(({ id, depth, step, here }) => {
        const line = step?.edits[0]?.[0]?.from.line;
        const row = element("button", { className: "commit", type: "button", title: step ? `${clock(step.made)}${step.type ? `, ${step.type}` : ""}` : "" });
        row.dataset.key = String(id);
        row.dataset.depth = String(depth);
        if (here) {
          row.setAttribute("aria-current", "true");
        }
        const dot = element("span", { className: "icon commit-dot" });
        dot.setAttribute("aria-hidden", "true");
        row.append(...guides(depth), dot, element("span", { className: "name", textContent: step ? clock(step.made) : "As opened" }));
        if (line !== undefined) {
          row.append(element("span", { className: "where", textContent: `line ${displayed.base + line + 1}` }));
        }
        row.addEventListener("click", () => state.hooks.goToState(id));
        return row;
      })
    );
    if (focused !== null) {
      [...body.children].find((row) => row.dataset.key === focused)?.focus();
    }
    body.querySelector('[aria-current="true"]')?.scrollIntoView({ block: "nearest" });
  });
}

export function localItems(event) {
  const row = event.target.closest(".commit");
  const at = Number(row?.dataset.key);
  if (!row || !state.local.path) {
    return null;
  }
  const path = state.local.path;
  return [
    act("history-difference", [path, String(at)], { label: "Show Difference" }),
    act("history-revert", [path, String(at)], { label: "Revert to This" }),
  ];
}

// TODO: every TODO, FIXME, XXX and HACK in the tree's files, as whole words, a row a file and under
// it a row a line, read again each time the pane opens or the explorer is refreshed.

const TODO_WORDS = "TODO|FIXME|XXX|HACK";

export async function drawTodo() {
  const said = document.getElementById("todo-said");
  const body = document.getElementById("todo-hits");
  if (!paneOpen("todo") || state.group !== "explorer") {
    return;
  }
  said.textContent = "Reading the tree…";
  const hits = await invoke("tree_search", { query: TODO_WORDS, how: { case: true, word: true, regex: true } }).catch((error) => {
    said.textContent = String(error);
    return null;
  });
  if (!hits) {
    return;
  }
  const files = new Map();
  for (const hit of hits) {
    if (!files.has(hit.path)) {
      files.set(hit.path, []);
    }
    files.get(hit.path).push(hit);
  }
  said.textContent = hits.length ? `${hits.length} item${hits.length === 1 ? "" : "s"} in ${files.size} file${files.size === 1 ? "" : "s"}` : "No TODO, FIXME, XXX or HACK in the tree.";
  const rows = [];
  for (const [path, found] of files) {
    const cut = path.lastIndexOf("/");
    const head = element("div", { className: "node problem-file" }, iconOf(path.slice(cut + 1)), element("span", { className: "name", textContent: path.slice(cut + 1) }), element("span", { className: "where", textContent: path.slice(0, Math.max(0, cut)) }), element("span", { className: "count", textContent: String(found.length) }));
    rows.push(head);
    for (const hit of found) {
      const text = hit.text.trim();
      const word = text.match(new RegExp(`\\b(${TODO_WORDS})\\b`));
      const row = element("button", { className: "search-hit", type: "button", title: `${path}:${hit.line}` });
      row.dataset.key = `todo:${path}:${hit.line}`;
      row.dataset.depth = "1";
      const line = element("span", { className: "line" });
      if (word) {
        line.append(text.slice(0, word.index), element("mark", { textContent: word[0] }), text.slice(word.index + word[0].length, word.index + 160));
      } else {
        line.textContent = text.slice(0, 160);
      }
      row.append(element("i", { className: "guide" }), line);
      row.addEventListener("click", () => state.hooks.openAt(path, hit.line - 1, hit.col - 1));
      rows.push(row);
    }
  }
  body.replaceChildren(...rows);
}

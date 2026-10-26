// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Call Hierarchy: the functions that call the one at the cursor, and those it calls, each a tree that
// opens a level at a time, as the language server answers or, where none answers for calls, as orior's
// own tree of functions finds them. A press on a function goes to its call, the first
// where it calls more than once; a press on its arrow, or the list's keys, open and close the level
// under it.

import { asked } from "../bridge.js";
import { showPane } from "../explorer.js";
import { flush } from "../servers.js";
import { togglePane } from "../sides.js";
import { say } from "../statusbar.js";
import { element, here, state } from "./state.js";

const EMPTY_CALLS = "Call Hierarchy (Ctrl+Alt+H) on a function lists what calls it and what it calls.";

export async function callHierarchy() {
  const found = here();
  if (!found) {
    return;
  }
  let root;
  try {
    await flush(found.tab);
    root = await asked("calls_root", found.at);
  } catch (error) {
    say(String(error), { failed: true });
    return;
  }
  togglePane(true, { take: false });
  showPane("calls");
  if (!root) {
    state.calls = null;
    drawCalls("No function at the cursor.");
    return;
  }
  const calls = { root, from: found.tab.file, open: new Set(["in", "out"]), children: new Map() };
  state.calls = calls;
  drawCalls(`Reading the calls of ${root.name}…`);
  await Promise.all([loadCalls(calls, "in", root, true), loadCalls(calls, "out", root, false)]);
  if (state.calls === calls) {
    drawCalls();
  }
}

// Reads the level of calls under `key` once.
async function loadCalls(calls, key, call, incoming) {
  if (calls.children.has(key)) {
    return;
  }
  const found = await asked("calls_of", { path: calls.from, item: call.item, incoming }).catch((error) => {
    say(String(error), { failed: true });
    return [];
  });
  calls.children.set(key, found);
}

async function toggleCalls(key, call, incoming) {
  const calls = state.calls;
  if (calls.open.has(key)) {
    calls.open.delete(key);
  } else {
    calls.open.add(key);
    await loadCalls(calls, key, call, incoming);
  }
  if (state.calls === calls) {
    drawCalls();
    document.querySelector(`#calls-tree [data-key="calls:${CSS.escape(key)}"]`)?.focus();
  }
}

function drawCalls(note) {
  const said = document.getElementById("calls-said");
  const tree = document.getElementById("calls-tree");
  const calls = state.calls;
  if (!calls) {
    said.textContent = note ?? EMPTY_CALLS;
    tree.replaceChildren();
    return;
  }
  said.textContent = note ?? `${calls.root.name}, ${calls.root.path}:${calls.root.from.line + 1}`;
  const rows = [];
  const add = (key, call, depth, incoming, label) => {
    const open = calls.open.has(key);
    const children = calls.children.get(key);
    const where = label ? "" : `${call.path}:${call.from.line + 1}`;
    const row = element("button", { className: "node dir search-file call-row", type: "button", title: label ?? `${call.name}${call.detail ? ` — ${call.detail}` : ""}\n${where}` });
    row.dataset.key = `calls:${key}`;
    row.dataset.depth = String(depth);
    row.setAttribute("aria-expanded", String(open));
    const guides = Array.from({ length: depth }, () => element("i", { className: "guide" }));
    const count = label ? (children?.length ?? "") : call.at.length > 1 ? String(call.at.length) : "";
    row.append(...guides, element("span", { className: "twisty" }), element("span", { className: "name", textContent: label ?? call.name }), element("span", { className: "where", textContent: where }), element("span", { className: "count", textContent: String(count) }));
    row.addEventListener("click", (event) => {
      if (label || !event.isTrusted || event.target.closest(".twisty")) {
        toggleCalls(key, call, incoming);
        return;
      }
      const at = call.at[0] ?? call.from;
      state.hooks.openAt(call.at.length ? call.site : call.path, at.line, at.col);
    });
    rows.push(row);
    if (!open) {
      return;
    }
    if (children && !children.length) {
      const none = element("p", { className: "pane-empty call-none", textContent: incoming ? "Nothing calls it." : "It calls nothing." });
      none.style.paddingLeft = `${1.2 + depth * 0.9}rem`;
      rows.push(none);
    }
    for (const child of children ?? []) {
      add(`${key}/${child.path}:${child.from.line}:${child.name}`, child, depth + 1, incoming);
    }
  };
  add("in", calls.root, 0, true, `Calls to ${calls.root.name}`);
  add("out", calls.root, 0, false, `Calls from ${calls.root.name}`);
  tree.replaceChildren(...rows);
}

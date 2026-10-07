// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Find in Files, the explorer's Search pane: every line of the tree's files that holds what is typed,
// a row a file with its count and under it a row a line, the match marked. Case, whole words and
// regular expressions each turn on and off beside the field. A file's row opens and closes its lines,
// and a line's row opens the file at the match.

import { invoke } from "./bridge.js";
import { iconOf } from "./explorer.js";

// How long typing rests before the tree is searched, in milliseconds, and the most hits the tree
// gives at once.
const REST = 300;
const MOST = 2000;

const state = { query: null, options: null, said: null, hits: null, how: { case: false, word: false, regex: false }, closed: new Set(), asked: 0, open: null, wait: 0 };

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

// A line's text with the match marked, trimmed to start a little before it.
function lineOf(hit, length) {
  const text = hit.text.replace(/\s+$/, "");
  const at = Math.max(0, hit.col - 1);
  const from = at > 40 ? at - 30 : 0;
  const lead = text.slice(from, at).replace(/^\s+/, "");
  const match = text.slice(at, at + Math.max(1, length));
  return [from > 0 ? "…" : "", lead, element("mark", { textContent: match }), text.slice(at + match.length, at + match.length + 160)];
}

// How long a match is: the query's length, or for a regular expression, what it matches on the line.
function lengthOf(hit, query) {
  if (!state.how.regex) {
    return query.length;
  }
  try {
    const found = new RegExp(query, state.how.case ? "" : "i").exec(hit.text.slice(hit.col - 1));
    return found?.index === 0 ? found[0].length : 1;
  } catch {
    return 1;
  }
}

function draw(hits, query) {
  const groups = new Map();
  for (const hit of hits) {
    if (!groups.has(hit.path)) {
      groups.set(hit.path, []);
    }
    groups.get(hit.path).push(hit);
  }
  const files = groups.size;
  const count = hits.length;
  state.said.textContent = count ? `${count >= MOST ? `${MOST}+` : count} result${count === 1 ? "" : "s"} in ${files} file${files === 1 ? "" : "s"}` : "No results found.";
  const rows = [];
  for (const [path, lines] of groups) {
    const cut = path.lastIndexOf("/");
    const open = !state.closed.has(path);
    const head = element("button", { className: "node dir search-file", type: "button", title: path });
    head.dataset.key = `file:${path}`;
    head.dataset.depth = "0";
    head.setAttribute("aria-expanded", String(open));
    head.append(element("span", { className: "twisty" }), iconOf(path.slice(cut + 1)), element("span", { className: "name", textContent: path.slice(cut + 1) }), element("span", { className: "where", textContent: path.slice(0, Math.max(0, cut)) }), element("span", { className: "count", textContent: String(lines.length) }));
    head.addEventListener("click", () => {
      if (state.closed.has(path)) {
        state.closed.delete(path);
      } else {
        state.closed.add(path);
      }
      draw(state.found, state.foundFor);
      state.hits.querySelector(`[data-key="file:${CSS.escape(path)}"]`)?.focus();
    });
    rows.push(head);
    if (!open) {
      continue;
    }
    for (const hit of lines) {
      const row = element("button", { className: "search-hit", type: "button", title: `${path}:${hit.line}:${hit.col}` });
      row.dataset.key = `${path}:${hit.line}:${hit.col}`;
      row.dataset.depth = "1";
      row.append(element("i", { className: "guide" }), element("span", { className: "line" }, ...lineOf(hit, lengthOf(hit, query))));
      row.addEventListener("click", () => state.open(path, hit.line - 1, hit.col - 1));
      rows.push(row);
    }
  }
  state.hits.replaceChildren(...rows);
}

async function run() {
  const query = state.query.value;
  const asked = ++state.asked;
  if (!query.trim()) {
    state.found = [];
    state.said.textContent = "";
    state.hits.replaceChildren();
    return;
  }
  state.said.textContent = "Searching…";
  let hits;
  try {
    hits = await invoke("tree_search", { query, how: state.how });
  } catch (error) {
    if (asked === state.asked) {
      state.said.textContent = String(error);
      state.hits.replaceChildren();
    }
    return;
  }
  if (asked !== state.asked) {
    return;
  }
  state.found = hits;
  state.foundFor = query;
  draw(hits, query);
}

function later() {
  window.clearTimeout(state.wait);
  state.wait = window.setTimeout(run, REST);
}

// Fills the field with `text` where it is given, and gives it the keys.
export function focusSearch(text) {
  if (text !== undefined && text !== null) {
    state.query.value = text;
    run();
  }
  state.query.focus();
  state.query.select();
}

// `open(path, line, col)` opens a file at a match, each counted from zero.
export function startSearch(open) {
  state.open = open;
  state.query = document.getElementById("search-query");
  state.said = document.getElementById("search-said");
  state.hits = document.getElementById("search-hits");
  state.query.addEventListener("input", later);
  state.query.addEventListener("keydown", (event) => {
    if (event.key === "Enter") {
      event.preventDefault();
      window.clearTimeout(state.wait);
      run();
    } else if (event.key === "ArrowDown") {
      event.preventDefault();
      state.hits.querySelector("button")?.focus();
    }
  });
  for (const option of document.querySelectorAll("#search-options [data-option]")) {
    const name = option.dataset.option;
    const flip = () => {
      state.how[name] = !state.how[name];
      option.setAttribute("aria-checked", String(state.how[name]));
      run();
    };
    option.addEventListener("click", flip);
    option.addEventListener("keydown", (event) => {
      if (event.key === " " || event.key === "Enter") {
        event.preventDefault();
        flip();
      }
    });
  }
}

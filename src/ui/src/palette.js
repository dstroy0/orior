// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The quick open along the top of the window, which reads what is typed by its first letter: a file
// of the tree by default, the files opened last first, and path:line:column going to that place; >
// for the menus' commands, the ones run last first; : for a line of the open file, as :line:column;
// and @ for what the open file declares. Up and Down choose, Enter goes, and Escape gives the keys
// back to what had them.

import { fuzzy, marked } from "./fuzzy.js";

const MOST = 200;
const RECENT_COMMANDS = "orior.palette.recent";

// How long the tree's list of files is held before it is read again, in milliseconds.
const FILES_HELD = 20000;

const state = { node: null, input: null, list: null, items: [], chosen: 0, before: null, hooks: null, files: null, read: 0, drawing: 0 };

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

function recentCommands() {
  try {
    return JSON.parse(localStorage.getItem(RECENT_COMMANDS) ?? "[]");
  } catch {
    return [];
  }
}

function ranCommand(key) {
  localStorage.setItem(RECENT_COMMANDS, JSON.stringify([key, ...recentCommands().filter((one) => one !== key)].slice(0, 20)));
}

async function treeFiles() {
  if (!state.files || performance.now() - state.read > FILES_HELD) {
    state.files = await state.hooks.files().catch(() => []);
    state.read = performance.now();
  }
  return state.files;
}

// Forgets the tree's files, for a tree opened in place of this one.
export function forgetFiles() {
  state.files = null;
}

// The rows for what is typed, each { label, hits, detail, keys, run }.

function commandRows(query) {
  const recent = recentCommands();
  const rows = [];
  for (const command of state.hooks.commands()) {
    const label = `${command.menu}: ${command.label}`;
    const found = fuzzy(query, label, command.menu.length + 2);
    if (!found) {
      continue;
    }
    const at = recent.indexOf(command.key);
    rows.push({
      label,
      hits: found.hits,
      keys: command.keys,
      detail: at >= 0 && !query ? "recently used" : "",
      score: query ? found.score : at >= 0 ? 1000 - at : 0,
      run: () => {
        ranCommand(command.key);
        command.run();
      },
    });
  }
  return rows.sort((a, b) => b.score - a.score);
}

// A place after a path or alone: line, or line and column, each counted from one.
function placeOf(text) {
  const found = text.match(/^(.*?)(?::(\d+))?(?::(\d+))?$/);
  return { path: found[1], line: found[2] ? Number(found[2]) : null, col: found[3] ? Number(found[3]) : null };
}

function lineRows(query) {
  const { line, col } = placeOf(`:${query}`);
  const count = state.hooks.lineCount();
  if (count === null) {
    return [{ label: "Open a file to go to a line in it.", hits: [], run: null }];
  }
  if (!line) {
    return [{ label: `Type a line number from 1 to ${count} to go to it.`, hits: [], run: null }];
  }
  const shown = Math.min(line, count);
  return [{ label: col ? `Go to line ${shown}, column ${col}` : `Go to line ${shown}`, hits: [], run: () => state.hooks.goLine(shown - 1, Math.max(0, (col ?? 1) - 1)) }];
}

function symbolRows(query) {
  const symbols = state.hooks.symbols();
  if (!symbols) {
    return [{ label: "Open a file to go to what it declares.", hits: [], run: null }];
  }
  const rows = [];
  for (const symbol of symbols) {
    const found = fuzzy(query, symbol.name);
    if (found) {
      rows.push({ label: symbol.name, hits: found.hits, detail: `${symbol.kind}, line ${symbol.shownLine}`, score: query ? found.score : -symbol.line, run: () => state.hooks.goLine(symbol.line, 0) });
    }
  }
  return rows.sort((a, b) => b.score - a.score);
}

async function fileRows(query) {
  const { path, line, col } = placeOf(query.trim());
  const files = await treeFiles();
  const recent = state.hooks.recent();
  const go = (file) => () => state.hooks.openFile(file, line ? line - 1 : null, col ? col - 1 : 0);
  const rowOf = (file, hits, score, detail = "") => {
    const cut = file.lastIndexOf("/") + 1;
    return {
      label: file.slice(cut),
      hits: hits.filter((at) => at >= cut).map((at) => at - cut),
      detail: [file.slice(0, Math.max(0, cut - 1)), detail].filter(Boolean).join("  "),
      detailHits: hits.filter((at) => at < cut - 1),
      score,
      run: go(file),
    };
  };
  if (!path) {
    return recent.map((file, at) => rowOf(file, [], -at, "recently opened"));
  }
  const rows = [];
  const kept = new Set(recent);
  for (const file of files) {
    const found = fuzzy(path, file, file.lastIndexOf("/") + 1);
    if (found) {
      rows.push(rowOf(file, found.hits, found.score + (kept.has(file) ? 6 : 0)));
    }
  }
  rows.sort((a, b) => b.score - a.score);
  return rows.slice(0, MOST);
}

async function rowsFor(text) {
  if (text.startsWith(">")) {
    return commandRows(text.slice(1).trim());
  }
  if (text.startsWith(":")) {
    return lineRows(text.slice(1).trim());
  }
  if (text.startsWith("@")) {
    return symbolRows(text.slice(1).trim());
  }
  return fileRows(text);
}

function placeholderOf(text) {
  if (text.startsWith(">")) {
    return "Type the name of a command to run.";
  }
  return "Search files by name (append :line to go to a line, or type > for commands, : for a line, @ for a symbol)";
}

function drawRows() {
  const { items, chosen } = state;
  state.list.replaceChildren(
    ...items.map((item, at) => {
      const row = element("div", { className: "quick-row" });
      row.setAttribute("role", "option");
      row.id = `quick-row-${at}`;
      row.setAttribute("aria-selected", String(at === chosen));
      if (!item.run) {
        row.classList.add("said");
      }
      row.append(element("span", { className: "quick-label" }, ...marked(item.label, item.hits)));
      if (item.detail) {
        row.append(element("span", { className: "quick-detail" }, ...marked(item.detail, item.detailHits ?? [])));
      }
      if (item.keys) {
        row.append(element("kbd", { textContent: item.keys }));
      }
      row.addEventListener("pointerdown", (event) => event.preventDefault());
      row.addEventListener("click", () => {
        state.chosen = at;
        go();
      });
      return row;
    })
  );
  state.input.setAttribute("aria-activedescendant", items.length ? `quick-row-${chosen}` : "");
  state.list.children[chosen]?.scrollIntoView({ block: "nearest" });
}

async function draw() {
  const text = state.input.value;
  const drawing = ++state.drawing;
  const items = await rowsFor(text);
  if (drawing !== state.drawing || state.node.hidden) {
    return;
  }
  state.items = items;
  state.chosen = 0;
  state.input.placeholder = placeholderOf(text);
  if (!items.length) {
    state.items = [{ label: text.startsWith(">") ? "No matching commands" : "No matching results", hits: [], run: null }];
  }
  drawRows();
}

function close(refocus = true) {
  if (state.node.hidden) {
    return;
  }
  state.node.hidden = true;
  state.drawing += 1;
  if (refocus && state.before?.isConnected) {
    state.before.focus();
  }
}

function go() {
  const item = state.items[state.chosen];
  if (!item?.run) {
    return;
  }
  close();
  item.run();
}

// Opens the quick open holding `text`: "" for files, ">" for commands, ":" for a line, "@" for what
// the file declares.
export function openPalette(text = "") {
  if (state.node.hidden) {
    state.before = document.activeElement;
  }
  state.node.hidden = false;
  state.input.value = text;
  state.input.focus();
  state.input.setSelectionRange(text.length, text.length);
  draw();
}

export function paletteOpen() {
  return Boolean(state.node && !state.node.hidden);
}

// `hooks` gives the palette what it reads: commands(), files(), recent(), symbols(), lineCount(),
// goLine(line, col) and openFile(path, line, col).
export function startPalette(hooks) {
  state.hooks = hooks;
  state.input = element("input", { className: "quick-input", type: "text", spellcheck: false, autocomplete: "off" });
  state.input.setAttribute("role", "combobox");
  state.input.setAttribute("aria-label", "Quick open");
  state.input.setAttribute("aria-controls", "quick-list");
  state.input.setAttribute("aria-expanded", "true");
  state.list = element("div", { className: "quick-list", id: "quick-list" });
  state.list.setAttribute("role", "listbox");
  state.node = element("div", { className: "quick", hidden: true }, state.input, state.list);
  document.body.append(state.node);
  state.input.addEventListener("input", () => {
    cancelAnimationFrame(state.frame);
    state.frame = requestAnimationFrame(draw);
  });
  state.input.addEventListener("keydown", (event) => {
    const step = { ArrowDown: 1, ArrowUp: -1, PageDown: 10, PageUp: -10 }[event.key];
    if (step) {
      event.preventDefault();
      const count = state.items.length;
      state.chosen = Math.max(0, Math.min(count - 1, state.chosen + step));
      drawRows();
    } else if (event.key === "Enter") {
      event.preventDefault();
      go();
    } else if (event.key === "Escape") {
      event.preventDefault();
      event.stopPropagation();
      close();
    }
  });
  state.input.addEventListener("blur", () => close(false));
}

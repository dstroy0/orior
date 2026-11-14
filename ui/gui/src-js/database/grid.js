// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The grid: the rows a statement gives, a page at a time as it scrolls, the server holding them and the grid only
// those in view; the rows written to a file, and the statements run on a connection.

import { Ahead } from "../ahead.js";
import { invoke } from "../bridge.js";
import { runOn } from "../database.js";
import { showMenu } from "../menu.js";
import { say } from "../statusbar.js";
import { PAGE, ROW, SPARE, chosen, element, parts, state } from "./state.js";

// Shows what a statement gave: its rows in the grid, or what it did, or what the server said.
export function show(name, said) {
  parts.messages.replaceChildren();
  if (said.failure) {
    const failure = said.failure;
    parts.head.textContent = `${name}: failed`;
    parts.messages.append(element("pre", { className: "database-failure", textContent: [failure.message + (failure.code ? ` (${failure.code})` : ""), failure.detail, failure.hint].filter(Boolean).join("\n") }));
    state.result = null;
    drawGrid();
    return;
  }
  for (const notice of said.notices ?? []) {
    parts.messages.append(element("p", { className: "database-notice", textContent: notice }));
  }
  if (!said.columns) {
    parts.head.textContent = `${name}: ${said.done || "done"} · ${said.ms} ms`;
    return;
  }
  state.result = { name, columns: said.columns, rows: new Map(said.rows.map((row, at) => [at, row])), read: said.rows.length, done: said.done, ms: said.ms };
  parts.grid.scrollTop = 0;
  drawGrid();
}

function headLine() {
  const result = state.result;
  if (!result) {
    return;
  }
  const open = state.open.get(result.name);
  parts.head.textContent = `${result.name}: ${result.read}${result.done ? "" : "+"} rows · ${result.ms} ms${open?.zone ? ` · times in ${open.zone}` : ""}${result.done ? ` · ${result.done}` : ""}`;
}

let fetching = false;
// The grid's motion: rows are read as far ahead the way it scrolls as its speed carries it over two
// reads, and those behind the motion are let go first.
const ahead = new Ahead(SPARE);
// Draws the rows in view, asking the server for those not retained in the stretch the scroll heads
// into, and for more as the end comes near.
export async function drawGrid() {
  const result = state.result;
  if (!result) {
    parts.gridHead.replaceChildren();
    parts.rows.replaceChildren();
    parts.spacer.style.height = "0px";
    return;
  }
  headLine();
  const widths = result.columns.map((column, at) => {
    let wide = column.name.length;
    for (let index = 0; index < Math.min(result.read, 50); index += 1) {
      wide = Math.max(wide, (result.rows.get(index)?.[at] ?? "NULL").length);
    }
    return Math.min(40, Math.max(4, wide)) + 2;
  });
  const template = widths.map((wide) => `${wide}ch`).join(" ");
  parts.gridHead.style.gridTemplateColumns = template;
  parts.gridHead.replaceChildren(...result.columns.map((column) => element("span", { className: column.numeric ? "num" : "", textContent: column.name, title: column.type })));
  const total = result.read + (result.done ? 0 : 1);
  parts.spacer.style.height = `${total * ROW}px`;
  const displayed = Math.floor(parts.grid.scrollTop / ROW);
  const first = Math.max(0, displayed - 5);
  const count = Math.ceil(parts.grid.clientHeight / ROW) + 10;
  ahead.moved(displayed);
  const [from, to] = ahead.span(first, first + count);
  const missing = [];
  for (let index = from; index < Math.min(to, result.read); index += 1) {
    if (!result.rows.has(index)) {
      missing.push(index);
    }
  }
  const nearEnd = !result.done && to >= result.read;
  if ((missing.length || nearEnd) && !fetching) {
    fetching = true;
    // The read covers the rows missing from the stretch, or the next page where none is.
    const start = missing.length ? missing[0] : result.read;
    const want = missing.length ? missing.at(-1) - start + 1 : PAGE;
    const began = performance.now();
    invoke("db_rows", { name: result.name, from: start, count: want })
      .then((got) => {
        ahead.read(performance.now() - began);
        if (state.result !== result) {
          return;
        }
        got.rows.forEach((row, at) => result.rows.set(start + at, row));
        result.read = got.read;
        result.done = got.done;
        // The rows far from the view are let go, behind the motion first; the server holds them.
        for (const index of ahead.letGo([...result.rows.keys()], first, first + count, PAGE * 3)) {
          result.rows.delete(index);
        }
      })
      .catch((error) => say(String(error), { failed: true }))
      .finally(() => {
        fetching = false;
        requestAnimationFrame(drawGrid);
      });
  }
  const rows = [];
  for (let index = first; index < Math.min(first + count, total); index += 1) {
    const row = result.rows.get(index);
    const line = element("div", { className: "db-row" });
    line.style.top = `${index * ROW}px`;
    line.style.gridTemplateColumns = template;
    if (!row) {
      line.append(element("span", { className: "db-wait", textContent: index >= result.read ? "reading more…" : "…" }));
    } else {
      row.forEach((value, at) => line.append(element("span", { className: `${result.columns[at]?.numeric ? "num" : ""}${value === null ? " null" : ""}`, textContent: value ?? "NULL", title: value ?? "NULL" })));
    }
    rows.push(line);
  }
  parts.rows.replaceChildren(...rows);
}

// The rows of the last result the grid holds, for the tests.
export const cachedRows = () => [...(state.result?.rows.keys() ?? [])];

export async function exportRows() {
  const result = state.result;
  if (!result) {
    return;
  }
  const path = (await state.hooks.ask("The file to write the rows to, in the tree: .csv, .tsv or .json for a JSON line each", `${result.name}-rows.csv`))?.trim();
  if (!path) {
    return;
  }
  const format = /\.tsv$/i.test(path) ? "tsv" : /\.jsonl?$/i.test(path) ? "json" : "csv";
  parts.head.textContent = `Writing ${path}…`;
  try {
    const written = await invoke("db_export", { name: result.name, path, format });
    say(`${written.written} rows written to ${path}, the rest read from the same run`);
    result.done = result.done ?? "read to its end";
  } catch (error) {
    say(String(error), { failed: true });
  }
  headLine();
}

export async function showHistory() {
  const name = chosen();
  const history = await invoke("db_history", { name }).catch(() => []);
  const items = history.slice(0, 40).map((one) => ({
    label: `${one.sql.replace(/\s+/g, " ").slice(0, 80)}${one.failed ? "  ✗" : ""}`,
    run: () => runOn(name, one.sql),
  }));
  showMenu(window.innerWidth / 3, window.innerHeight / 4, items.length ? items : [{ label: "Nothing has run on this connection", disabled: true }]);
}

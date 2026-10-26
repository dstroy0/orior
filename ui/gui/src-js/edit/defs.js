// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { wordAt } from "../editor/document.js";
import { drawBridge, inBridge, keyAt } from "../bridge_panel.js";
import { opening, rowOf } from "../languages.js";
import { element, extOf, state } from "../edit.js";
import { openFile } from "./open.js";
import { openAt } from "./save.js";
import { fileOf } from "./tabs.js";

// The bridge's key the panel shows: the one the cursor is on, or one picked from the list.
function bridgeKey() {
  const s = state.editor?.s;
  if (!s || !state.active) {
    return null;
  }
  const head = state.editor.head();
  const found = keyAt(state.active, s.doc, head.line, wordAt(s.doc, head)?.text);
  return found ?? state.picked ?? null;
}

// A file that is not text: its k-file head where the bytes open with the tree's magic, then the bytes.

export function drawBinary(tab) {
  const node = document.getElementById("binary");
  const bytes = tab.bytes;
  const head = state.head;
  const parts = [element("h3", { textContent: `${tab.path}, ${tab.size} bytes` })];
  const ascii = (from, to) => String.fromCharCode(...bytes.slice(from, to)).replace(/\0+$/, "");
  const isKrep = head && bytes.length >= head.bytes && ascii(0, 8) === head.magic;
  if (isKrep) {
    const version = new DataView(bytes.buffer, bytes.byteOffset).getUint32(12, true);
    const type = state.known.typeOf(tab.path);
    const named = (index, fallback) => {
      if (!type) {
        return fallback;
      }
      const row = type.table.rows.map((r) => rowOf(type.table, r)).find((r) => r.where === `head ${index}`);
      return row ? row.kind : fallback;
    };
    const grid = element("div", { className: "head" });
    grid.append(element("span", { textContent: named(0, "magic") }), element("span", { textContent: ascii(0, 8) }));
    grid.append(element("span", { textContent: named(1, "kind") }), element("span", { textContent: ascii(8, 12) }));
    grid.append(
      element("span", { textContent: named(2, "version") }),
      element("span", { textContent: String(version), className: version === head.version ? "" : "bad" })
    );
    parts.push(grid);
  }
  const rows = [];
  for (let at = 0; at < bytes.length; at += 16) {
    const line = element("div");
    line.append(at.toString(16).padStart(8, "0"), "  ");
    for (let index = at; index < at + 16; index += 1) {
      const text = index < bytes.length ? bytes[index].toString(16).padStart(2, "0") : "  ";
      line.append(isKrep && index < head.bytes ? element("b", { textContent: text }) : text, index === at + 7 ? "  " : " ");
    }
    const shown = [...bytes.slice(at, at + 16)].map((b) => (b >= 32 && b < 127 ? String.fromCharCode(b) : ".")).join("");
    line.append(" ", shown);
    rows.push(line);
  }
  parts.push(element("pre", {}, ...rows));
  node.replaceChildren(...parts);
}

// The definitions beside the editor.

export function drawDefs() {
  const panel = document.getElementById("defs");
  const path = state.active;
  const type = path ? state.known.typeOf(fileOf(path)) : null;
  const ext = path ? extOf(fileOf(path)) : "";
  const tables = state.known.tablesOf(ext);
  const bridged = inBridge(path);
  const side = document.getElementById("defs-side");
  side.hidden = !type && !tables.length && !bridged;
  if (side.hidden) {
    return;
  }
  const parts = [];
  if (bridged) {
    state.shownKey = bridgeKey();
    const choose = (key) => {
      state.picked = key;
      drawDefs();
    };
    parts.push(element("section", { className: "bridge" }, ...drawBridge(state.shownKey, openAt, choose)));
  }
  if (type) {
    const source = element("a", { className: "source", textContent: type.table.path, tabIndex: 0 });
    source.addEventListener("click", () => openFile(type.table.path));
    parts.push(element("h3", { textContent: `.${type.ext}` }), source);
    if (type.header) {
      parts.push(element("p", { className: "header", textContent: type.header }));
    }
    let where = null;
    for (const row of type.table.rows.map((r) => rowOf(type.table, r))) {
      const place = row.where.replace(/\s+\d+$/, "");
      if (place !== where) {
        where = place;
        parts.push(element("div", { className: "where", textContent: place }));
      }
      const item = element("div", { className: "row", title: row.kind });
      item.dataset.key = opening(row.form);
      item.dataset.kind = row.kind;
      item.append(element("code", { textContent: row.form }), element("span", { textContent: row.gloss }));
      item.append(element("small", { textContent: row.who && row.who !== "-" ? `${row.kind}, ${row.who}` : row.kind }));
      parts.push(item);
    }
  }
  for (const table of tables) {
    parts.push(...languageTable(table));
  }
  panel.replaceChildren(...parts);
  if (bridged) {
    panel.scrollTop = 0;
  }
  light();
}

function languageTable(table) {
  const at = (row, name) => row[table.columns.indexOf(name)] ?? "";
  const asm = table.columns.includes("mnemonic");
  const key = asm ? "mnemonic" : table.columns[0];
  const filter = element("input", { className: "filter", type: "search", placeholder: "Search", spellcheck: false });
  const source = element("a", { className: "source", textContent: table.path, tabIndex: 0 });
  source.addEventListener("click", () => openFile(table.path));
  const rows = element("div");
  const draw = () => {
    const query = filter.value.trim().toLowerCase();
    rows.replaceChildren(
      ...table.rows
        .filter((row) => !query || row.join(" ").toLowerCase().includes(query))
        .slice(0, 400)
        .map((row) => {
          const item = element("div", { className: "row" });
          item.dataset.key = at(row, key);
          const said = asm
            ? `${at(row, "table")}: ${at(row, "past")} ${at(row, "arrow") || "-"} ${at(row, "present")}`
            : [at(row, "kind"), at(row, "names")].filter(Boolean).join(": ");
          item.append(element("code", { textContent: at(row, key) }), element("span", { textContent: said }));
          return item;
        })
    );
    light();
  };
  filter.addEventListener("input", draw);
  draw();
  return [element("h3", { textContent: table.name }), source, filter, rows];
}

// Marks the rows that define what the cursor is on: the line's first word for a line language, the
// word itself for .g and .gsm.
export function light() {
  if (inBridge(state.active) && bridgeKey() !== state.shownKey) {
    drawDefs();
    return;
  }
  const panel = document.getElementById("defs");
  const session = state.editor?.s;
  let keys = [];
  if (session) {
    const head = state.editor.head();
    const lead = session.doc.line(head.line).match(/^\s*([A-Za-z_][\w-]*)/);
    const word = wordAt(session.doc, head)?.text;
    keys = [lead?.[1], word].filter(Boolean);
  }
  let first = null;
  panel.querySelectorAll(".row").forEach((row) => {
    const lit = keys.includes(row.dataset.key);
    row.dataset.lit = String(lit);
    first = first ?? (lit ? row : null);
  });
  // In a file of the bridge the bridge section stays in sight, above the rows.
  if (!inBridge(state.active)) {
    first?.scrollIntoView({ block: "nearest" });
  }
}


// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The hover and the completion of the languages the tree's tables define, each from those tables and from nothing
// written here; their colors are orior's side's, by the plugins kfile, gsm and g, whose words the same tables give.
//
// A k-file that is text is read a line at a time, and the first word of a line names the line: the
// oracle table's forms give those words, and each form's gloss is what hovering one shows. .gsm takes
// its mnemonics, states and arrows from gnascor_asm_lng.tsv and .g its words from gnascor_hol_lng.tsv,
// with C11 around them and .gsm inside __gsm__(...). A k-file that is not text has no language and
// opens in the binary view. Every other language is a plugin, as plugins.js reads them.

import { pos, wordAt, wordBefore } from "./editor/document.js";
import { languageForExtension, languageForName, languageForPath } from "./plugins.js";

// C11's words, as the g plugin's grammar lists them.
const c11 = () => languageForExtension("g")?.grammar?.def?.c11 ?? [];

const BRACKETS = ["()", "[]", "{}"];

const WORD = /^[A-Za-z_][\w-]*/;

const unique = (list) => [...new Set(list.filter(Boolean))];

const column = (table, name) => table.columns.indexOf(name);

const span = (line, from, to) => ({ from: pos(line, from), to: pos(line, to) });

// A row of an oracle table as the hover and the definitions list show it.
export function rowOf(table, row) {
  const at = (name) => row[column(table, name)] ?? "";
  return { where: at("where"), who: at("who"), type: at("type"), form: at("form"), gloss: at("gloss") };
}

// The word a form opens with, where it opens with one: `form <name> ... = <text>` gives form, and
// `<channel> <answer> ...` gives nothing.
export function opening(form) {
  const found = form.trim().match(WORD);
  return found ? found[0] : "";
}

// Whether a file type's table describes bytes instead of lines.
export function isBinary(type) {
  return type.table.rows.some((row) => /^head \d/.test(rowOf(type.table, row).where));
}

function hoverText(rows, table) {
  return rows.map((row) => {
    const said = rowOf(table, row);
    const by = said.who && said.who !== "-" ? `, ${table.columns[1]} ${said.who}` : "";
    return `**${said.type}**\n\n\`${said.form}\`\n\n${said.gloss}\n\n_${table.columns[0]} ${said.where}${by}_`;
  });
}

// A form as a snippet: each <name> a place the cursor stops at, and a trailing ... left out.
function snippet(form) {
  let place = 0;
  return form
    .replace(/\s*\.\.\.(?=\s|$)/g, "")
    .replace(/<([^>]+)>/g, (_, name) => `\${${++place}:${name}}`)
    .replace(/\\t/g, "\t");
}

// A k-file's language, `kfile:<ext>` as orior's side colors it.
function lineLanguage(type) {
  const table = type.table;
  const rows = table.rows.map((row) => rowOf(table, row));
  const commented = rows.some((row) => row.type === "comment" && row.form.startsWith("#"));
  return {
    id: `kfile:${type.ext}`,
    grammar: null,
    comments: commented ? { line: "#" } : {},
    pairs: BRACKETS,
    quotes: ['"'],
    indentAfter: null,
    hover(doc, at) {
      const lead = doc.line(at.line).match(/^(\s*)([A-Za-z_][\w-]*)/);
      if (!lead) {
        return null;
      }
      const start = lead[1].length;
      const end = start + lead[2].length;
      if (at.col < start || at.col > end) {
        return null;
      }
      const matched = table.rows.filter((row) => opening(rowOf(table, row).form) === lead[2]);
      return matched.length ? { ...span(at.line, start, end), parts: hoverText(matched, table) } : null;
    },
    complete(doc, at) {
      const before = doc.line(at.line).slice(0, at.col);
      if (!/^\s*[A-Za-z_]*$/.test(before)) {
        return [];
      }
      const seen = new Set();
      const items = [];
      for (const row of rows) {
        const key = opening(row.form);
        if (!key || seen.has(row.form)) {
          continue;
        }
        seen.add(row.form);
        items.push({ label: key, type: "keyword", detail: row.form, doc: row.gloss, insert: snippet(row.form), snippet: true });
      }
      return items;
    },
  };
}

// The words .gsm is written in, from the assembly table.
function gsmWords(asm) {
  const take = (name) => (column(asm, name) < 0 ? [] : asm.rows.map((row) => row[column(asm, name)]));
  const word = (text) => /^[A-Za-z_]\w*$/.test(text ?? "");
  return {
    mnemonics: unique(take("mnemonic").filter(word)),
    states: unique([...take("past"), ...take("present")].filter(word)),
    arrows: unique(take("arrow").filter((text) => text && !word(text) && text !== "-")),
  };
}

function gsmHover(gsmType, asm, words, doc, at) {
  const word = wordAt(doc, at);
  const table = gsmType?.table;
  if (word && words.mnemonics.includes(word.text)) {
    const cell = (row, name) => row[column(asm, name)] ?? "";
    const retained = asm.rows.filter((row) => cell(row, "mnemonic") === word.text);
    const lines = retained.map((row) => `${cell(row, "table")}: \`${cell(row, "past")} ${cell(row, "arrow") || ""} ${cell(row, "present")}\``);
    const mnemonicRows = table ? table.rows.filter((row) => rowOf(table, row).type === "mnemonic") : [];
    return { from: word.from, to: word.to, parts: [`**${word.text}**`, lines.join("\n\n"), ...(table ? hoverText(mnemonicRows, table) : [])] };
  }
  if (!table) {
    return null;
  }
  const line = doc.line(at.line);
  for (const arrow of [...words.arrows, "|"]) {
    for (let from = line.indexOf(arrow); from >= 0; from = line.indexOf(arrow, from + 1)) {
      if (at.col >= from && at.col < from + arrow.length) {
        const retained = table.rows.filter((row) => rowOf(table, row).form.includes(arrow));
        return { ...span(at.line, from, from + arrow.length), parts: hoverText(retained, table) };
      }
    }
  }
  if (word && words.states.includes(word.text)) {
    const retained = table.rows.filter((row) => rowOf(table, row).type === "pair");
    return { from: word.from, to: word.to, parts: [`**${word.text}**`, ...hoverText(retained, table)] };
  }
  return null;
}

function gsmLanguage(gsmType, asm) {
  const words = gsmWords(asm);
  return {
    words,
    language: {
      id: "gsm",
      grammar: null,
      comments: {},
      pairs: ["[]"],
      quotes: [],
      indentAfter: null,
      hover: (doc, at) => gsmHover(gsmType, asm, words, doc, at),
      complete() {
        return [
          ...words.mnemonics.map((label) => ({ label, type: "keyword", insert: label })),
          ...words.states.map((label) => ({ label, type: "state", insert: label })),
        ];
      },
    },
  };
}

function gLanguage(gType, gsmType, hol, asm, gsm) {
  const cell = (row, name) => row[column(hol, name)] ?? "";
  const single = (row) => /^[A-Za-z_]\w*$/.test(cell(row, "word"));
  const typeWords = unique(hol.rows.filter((r) => single(r) && cell(r, "type") === "type").map((r) => cell(r, "word")));
  const opWords = unique(hol.rows.filter((r) => single(r) && cell(r, "type") === "core op").map((r) => cell(r, "word")));
  const plainWords = unique(hol.rows.filter((r) => single(r) && !["type", "core op"].includes(cell(r, "type"))).map((r) => cell(r, "word")));
  const gTable = gType?.table;
  const byType = (type) => (gTable ? gTable.rows.filter((row) => rowOf(gTable, row).type === type) : []);
  return {
    id: "g",
    grammar: null,
    comments: { line: "//", block: ["/*", "*/"] },
    pairs: BRACKETS,
    quotes: ['"'],
    indentAfter: /[{[(]\s*$/,
    hover(doc, at) {
      const line = doc.line(at.line);
      const shapes = [
        [/@[A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*/g, "address"],
        [/\?[A-Za-z_]\w*/g, "qualifier"],
        [/\(\$[^)]*\)/g, "cost bound"],
        [/__gsm__\(/g, "gsm block"],
        [/^\s*#\s*\w+/g, "preprocessor"],
      ];
      for (const [shape, type] of shapes) {
        for (const found of line.matchAll(shape)) {
          if (gTable && at.col >= found.index && at.col <= found.index + found[0].length) {
            return { ...span(at.line, found.index, found.index + found[0].length), parts: hoverText(byType(type), gTable) };
          }
        }
      }
      const word = wordAt(doc, at);
      if (!word) {
        return null;
      }
      if (asm && gsm.mnemonics.includes(word.text) && /__gsm__\(/.test(line)) {
        return gsmHover(gsmType, asm, gsm, doc, at);
      }
      const rows = hol.rows.filter((row) => cell(row, "word") === word.text);
      if (!rows.length) {
        return word.text === "use" && gTable ? { from: word.from, to: word.to, parts: hoverText(byType("use"), gTable) } : null;
      }
      return {
        from: word.from,
        to: word.to,
        parts: rows.map((row) => {
          const names = cell(row, "names") ? `: ${cell(row, "names")}` : "";
          return `**${word.text}**\n\n${cell(row, "type")}${names}\n\n_${hol.columns[column(hol, "chosen_by")]} ${cell(row, "chosen_by")}_`;
        }),
      };
    },
    complete(doc, at) {
      if (!wordBefore(doc, at).text) {
        return [];
      }
      const make = (label, type, detail) => ({ label, type: type, detail, insert: label });
      return [
        ...typeWords.map((w) => make(w, "type", "type")),
        ...opWords.map((w) => make(w, "operator", "core op")),
        ...plainWords.map((w) => make(w, "keyword", "plain")),
        ...c11().map((w) => make(w, "keyword", "C11")),
      ];
    },
  };
}

// Builds every language the definitions give and returns the ways the editor asks about a file.
export function registerLanguages(defs) {
  const types = new Map(defs.types.map((type) => [type.ext, type]));
  const table = (name) => defs.languages.find((one) => one.name === name);
  const asm = table("gnascor_asm_lng.tsv");
  const hol = table("gnascor_hol_lng.tsv");
  const own = new Map();
  let gsm = { mnemonics: [], states: [], arrows: [] };
  if (asm) {
    const made = gsmLanguage(types.get("gsm"), asm);
    gsm = made.words;
    own.set("gsm", made.language);
  }
  if (hol) {
    own.set("g", gLanguage(types.get("g"), types.get("gsm"), hol, asm, gsm));
  }
  for (const type of defs.types) {
    if (type.ext !== "g" && type.ext !== "gsm" && !isBinary(type)) {
      own.set(type.ext, lineLanguage(type));
    }
  }
  const extOf = (path) => (path.includes(".") ? path.split(".").pop().toLowerCase() : "");
  return {
    types,
    // The language a path opens in.
    languageOf(path) {
      const ext = extOf(path);
      return own.get(ext) ?? languageForName(path.split(/[\\/]/).pop()) ?? languageForPath(path) ?? languageForExtension(ext);
    },
    // The file type a path is, where the tree defines one.
    typeOf(path) {
      return types.get(extOf(path)) ?? null;
    },
    // The language tables beside a type's own: the assembly table for .gsm, both for .g.
    tablesOf(ext) {
      if (ext === "gsm") {
        return [asm].filter(Boolean);
      }
      if (ext === "g") {
        return [hol, asm].filter(Boolean);
      }
      return [];
    },
  };
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The bridge beside the editor: for the key under the cursor, its pairs and their verdicts in
// Lstar.klq, the name each language's .klm gives it, and each ruleset's entry of that name, every
// line of them one click from its file. With no key under the cursor it lists the keys.
//
// A key is a schema form name, gnascor's and no target's. A .klm line `key <name> <key>` maps a
// language's name to it, and the two words are read apart even where they are equal. The index
// is read again after a protocol run ends and after a file of the bridge is saved.

import { invoke } from "./bridge.js";
import { startJob } from "./run.js";
import { status, watch, write } from "./status.js";

const state = { index: null, reading: null, lastRuns: new Set(), query: "" };

const LISTED = 300;

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

const nameOf = (path) => path.split("/").pop();
const extOf = (path) => (path.includes(".") ? path.split(".").pop().toLowerCase() : "");

export function loadBridge() {
  state.reading = invoke("bridge_read")
    .then((index) => {
      state.index = index;
      write("bridge", {
        keys: Object.keys(index.klq?.keys ?? {}).length,
        maps: index.maps.length,
        rulesets: index.rulesets.length,
        at: Date.now(),
      });
      return index;
    })
    .catch(() => null);
  return state.reading;
}

// Reads the index again once a protocol run ends.
export function keepBridge(onRead) {
  watch((held) => {
    const now = new Set([...held.runs].filter(([, run]) => run.job.startsWith("protocol/")).map(([id]) => id));
    const ended = [...state.lastRuns].some((id) => !now.has(id));
    state.lastRuns = now;
    if (ended) {
      loadBridge().then(onRead);
    }
  });
}

// Whether a file belongs to the bridge and its panel shows.
export function inBridge(path) {
  return Boolean(path) && (path.startsWith(`${state.index?.dir ?? "src/cu/transpiler/lstar/protocol/table"}/`) || ["klq", "klm"].includes(extOf(path)));
}

// The key a line of a bridge file names, or of any file the word under the cursor where it is a key.
export function keyAt(path, doc, line, word) {
  const index = state.index;
  const keys = index?.klq?.keys ?? {};
  const words = (at) => doc.line(at).trim().split(/\s+/);
  const ext = extOf(path);
  if (ext === "klq") {
    for (let at = line; at >= Math.max(0, line - 400); at -= 1) {
      const [first, second] = words(at);
      if (first === "key") {
        return second;
      }
      if (first === "pair" && at === line) {
        return second && keys[second] ? second : null;
      }
    }
    return null;
  }
  if (ext === "klm") {
    const [first, second, third] = words(line);
    if (first === "key") {
      return third ?? second;
    }
    if (first === "kind" || first === "breaks") {
      const map = index?.maps.find((one) => one.path === path);
      return map?.names[second]?.key || second;
    }
    return null;
  }
  if (ext === "krs" || ext === "kdm") {
    const [first, second] = words(line);
    if (/^[a-z_]+$/.test(first ?? "") && second && keys[second]) {
      return second;
    }
  }
  return word && keys[word] ? word : null;
}

function jump(openAt, path, line, text, className = "") {
  const link = element("a", { className: `bridge-line ${className}`, tabIndex: 0, title: `${path}:${line + 1}` }, text);
  link.addEventListener("click", () => openAt(path, line));
  return link;
}

function verdictOf(pair) {
  const verdict = pair.verdict;
  if (!verdict) {
    return element("span", { className: "verdict verdict-none", textContent: "no verdict" });
  }
  return element("span", { className: `verdict verdict-${verdict.word}`, textContent: `${verdict.word} ${verdict.rest}`.trim() });
}

function keyDetail(key, openAt) {
  const index = state.index;
  const klq = index.klq;
  const held = klq?.keys[key];
  const parts = [element("div", { className: "bridge-key" }, held ? jump(openAt, klq.path, held.line, key) : key)];
  if (held?.pairs.length) {
    for (const pair of held.pairs) {
      const row = element("div", { className: "bridge-pair" });
      row.append(jump(openAt, klq.path, pair.line, `${pair.first} ~ ${pair.second}`), verdictOf(pair));
      for (const note of pair.notes) {
        row.append(element("div", { className: "bridge-note" }, jump(openAt, klq.path, note.line, `${note.word} ${note.rest}`.trim())));
      }
      parts.push(row);
    }
  } else if (held) {
    parts.push(element("p", { className: "bridge-quiet", textContent: "no pairs" }));
  }
  const mapped = [];
  for (const map of index.maps) {
    for (const [name, entry] of Object.entries(map.names)) {
      if (entry.key !== key && name !== key) {
        continue;
      }
      const row = element("div", { className: "bridge-map" }, element("span", { className: "lang", textContent: map.language || nameOf(map.path) }));
      row.append(jump(openAt, map.path, entry.line, entry.key && entry.key !== name ? `${name} → ${entry.key}` : name));
      if (entry.kind) {
        row.append(jump(openAt, map.path, entry.kind.line, entry.kind.rest, "kind"));
      }
      for (const broken of entry.breaks) {
        row.append(jump(openAt, map.path, broken.line, `breaks ${broken.rest}`, "kind"));
      }
      mapped.push(row);
    }
  }
  if (mapped.length) {
    parts.push(element("div", { className: "where", textContent: "maps" }), ...mapped);
  }
  const ruled = [];
  for (const ruleset of index.rulesets) {
    for (const entry of ruleset.entries[key] ?? []) {
      ruled.push(element("div", { className: "bridge-map" }, element("span", { className: "lang", textContent: nameOf(ruleset.path) }), jump(openAt, ruleset.path, entry.line, entry.kind)));
    }
  }
  if (ruled.length) {
    parts.push(element("div", { className: "where", textContent: "rulesets" }), ...ruled);
  }
  return parts;
}

function tally(key) {
  const pairs = key.pairs;
  const open = pairs.filter((pair) => pair.verdict?.word === "open").length;
  const closed = pairs.filter((pair) => pair.verdict?.word === "closed").length;
  return pairs.length ? `${pairs.length} pairs, ${open} open, ${closed} closed` : "";
}

function keyList(openAt, choose) {
  const klq = state.index.klq;
  const query = state.query.toLowerCase();
  const names = Object.keys(klq?.keys ?? {}).filter((name) => !query || name.toLowerCase().includes(query));
  return names.slice(0, LISTED).map((name) => {
    const row = element("button", { type: "button", className: "bridge-item" }, element("code", { textContent: name }), element("small", { textContent: tally(klq.keys[name]) }));
    row.addEventListener("click", () => choose(name));
    return row;
  });
}

// The panel's parts for a key, or the key list where there is none.
export function drawBridge(key, openAt, choose) {
  const index = state.index;
  if (!index?.klq) {
    return [element("h3", { textContent: "Bridge" }), element("p", { className: "bridge-quiet", textContent: "Lstar.klq not read" })];
  }
  const head = element("h3", { textContent: "Bridge" });
  const source = element("a", { className: "source", textContent: index.klq.path, tabIndex: 0 });
  source.addEventListener("click", () => openAt(index.klq.path, 0));
  const writer = element("button", { type: "button", className: "secondary bridge-write", textContent: "Write Lstar.klq" });
  writer.addEventListener("click", () => {
    writer.disabled = true;
    startJob("protocol/utils/maint/engine/klq_write.sh", {}).catch((error) => {
      writer.disabled = false;
      writer.title = String(error);
    });
  });
  writer.disabled = [...status.runs.values()].some((run) => run.job.startsWith("protocol/"));
  const filter = element("input", { className: "filter", type: "search", placeholder: "Search", spellcheck: false, value: state.query });
  const body = element("div");
  const fill = () => body.replaceChildren(...(key && !state.query ? keyDetail(key, openAt) : keyList(openAt, choose)));
  filter.addEventListener("input", () => {
    state.query = filter.value.trim();
    fill();
  });
  fill();
  return [head, source, element("div", { className: "bridge-tools" }, writer), filter, body];
}

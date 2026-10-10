// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The language plugins, as plugins.rs in the command line's crate describes them: those that come
// with orior and the reader's own, read as the app starts and again on Reload Plugins. Each becomes a
// language as the editor takes one, opened for the extensions it names. One of the reader's stands
// in for one that comes with orior of the same id.
//
// Where two plugins that are on name one extension, one of the reader's opens it ahead of one that
// comes with orior, and of two that come with orior the first by id opens it. Turning that one off
// hands the extension to the next.
//
// A plugin that does not read, or whose grammar does not compile, is set aside with what was wrong,
// which the Plugins sheet shows. One the reader turned off is read and not used. A file whose
// extension no plugin names opens as plain text.
//
// A plugin's word lists and snippets fill the completion list, and its hovers what hovering a word
// shows.

import { invoke } from "./bridge.js";
import { wordAt, wordBefore } from "./editor/document.js";
import { compile } from "./editor/tokens.js";

const OFF = "orior.plugins.off";

const PLAIN = Object.freeze({ id: "plaintext", name: "Plain Text", grammar: null, comments: {}, pairs: [], quotes: [], indentAfter: /$^/ });

const state = { found: [], readings: [], byExtension: new Map(), byName: new Map(), byId: new Map(), listeners: [] };

// Calls `listener` each time the plugins are read or one is turned on or off, until what this answers
// is called.
export function onPlugins(listener) {
  state.listeners.push(listener);
  return () => {
    state.listeners = state.listeners.filter((one) => one !== listener);
  };
}

// The ids of the plugins the reader turned off.
export function pluginsOff() {
  try {
    const kept = JSON.parse(localStorage.getItem(OFF) ?? "[]");
    return new Set(Array.isArray(kept) ? kept : []);
  } catch {
    return new Set();
  }
}

export function setPluginOn(id, on) {
  const off = pluginsOff();
  if (on) {
    off.delete(id);
  } else {
    off.add(id);
  }
  localStorage.setItem(OFF, JSON.stringify([...off]));
  build();
}

const patternOf = (value) => (typeof value === "string" ? new RegExp(value) : new RegExp(value.pattern, value.flags ?? ""));

// A grammar as a plugin writes it, with each rule's pattern made a pattern.
function revive(grammar) {
  const tokenizer = {};
  for (const [name, rules] of Object.entries(grammar.tokenizer ?? {})) {
    tokenizer[name] = rules.map((rule) => (Array.isArray(rule) ? [patternOf(rule[0]), ...rule.slice(1)] : rule));
  }
  return { ...grammar, tokenizer };
}

// A plugin as the editor's language.
function languageOf(plugin) {
  const grammar = plugin.grammar ? compile(revive(plugin.grammar)) : null;
  const lists = Object.entries(plugin.grammar ?? {}).filter(([, value]) => Array.isArray(value));
  const hovers = plugin.hovers && typeof plugin.hovers === "object" ? plugin.hovers : {};
  const snippets = Array.isArray(plugin.snippets) ? plugin.snippets.filter((one) => one && one.label && one.body) : [];
  const kindOf = (list) => (/type/.test(list) ? "type" : /const|predefined/.test(list) ? "constant" : "keyword");
  return {
    id: plugin.id,
    name: plugin.name ?? plugin.id,
    grammar,
    comments: plugin.comments ?? {},
    pairs: plugin.pairs ?? ["()", "[]", "{}"],
    quotes: plugin.quotes ?? ['"', "'"],
    indentAfter: plugin.indentAfter ? patternOf(plugin.indentAfter) : /[{[(]\s*$/,
    hover: Object.keys(hovers).length
      ? (doc, at) => {
          const word = wordAt(doc, at);
          const said = word && Object.hasOwn(hovers, word.text) ? String(hovers[word.text]) : null;
          return said ? { from: word.from, to: word.to, parts: [`**${word.text}**`, said] } : null;
        }
      : undefined,
    complete:
      lists.length || snippets.length
        ? (doc, at) => {
            if (!wordBefore(doc, at).text) {
              return [];
            }
            return [
              ...lists.flatMap(([list, words]) => words.map((word) => ({ label: String(word), kind: kindOf(list), detail: list, insert: String(word) }))),
              ...snippets.map((one) => ({ label: String(one.label), kind: "snippet", detail: "snippet", insert: String(one.body), snippet: true })),
            ];
          }
        : undefined,
  };
}

// Reads each plugin found, then makes the languages of those that read and are on.
function build() {
  const off = pluginsOff();
  const readings = state.found.map(({ id, source, folder, text }) => {
    const reading = { id, source, folder, name: id, version: "", extensions: [], names: [], error: null, replaced: false, language: null };
    try {
      const plugin = JSON.parse(text);
      if (plugin.id !== id) {
        throw new Error(`its id is ${JSON.stringify(plugin.id)}, and its folder is ${id}`);
      }
      Object.assign(reading, { name: plugin.name ?? id, version: plugin.version ?? "", extensions: Array.isArray(plugin.extensions) ? plugin.extensions.map(String) : [], names: Array.isArray(plugin.names) ? plugin.names.map(String) : [] });
      // A tool plugin opens no files: it checks those of the languages it names.
      if (plugin.kind === "tool") {
        Object.assign(reading, { kind: "tool", checks: Array.isArray(plugin.languages) ? plugin.languages.map(String) : [], about: String(plugin.about ?? "") });
      } else {
        reading.language = languageOf(plugin);
      }
    } catch (error) {
      reading.error = String(error.message ?? error);
    }
    return reading;
  });
  const last = new Map(readings.map((reading) => [reading.id, reading]));
  for (const reading of readings) {
    reading.replaced = last.get(reading.id) !== reading;
    reading.on = !off.has(reading.id);
  }
  state.readings = readings;
  state.byId = new Map();
  state.byExtension = new Map();
  state.byName = new Map();
  for (const reading of last.values()) {
    if (!reading.language || !reading.on) {
      continue;
    }
    state.byId.set(reading.id, reading.language);
    for (const ext of reading.extensions.map((one) => one.toLowerCase())) {
      if (reading.source === "user" || !state.byExtension.has(ext)) {
        state.byExtension.set(ext, reading.language);
      }
    }
    for (const name of reading.names.map((one) => one.toLowerCase())) {
      if (reading.source === "user" || !state.byName.has(name)) {
        state.byName.set(name, reading.language);
      }
    }
  }
  [...state.listeners].forEach((listener) => listener());
}

// Reads every plugin again.
export async function loadPlugins() {
  state.found = await invoke("plugins_read").catch(() => []);
  build();
}

// The tool plugin that checks files of `language`, where one is on.
export function toolFor(language) {
  return state.readings?.find((reading) => reading.kind === "tool" && reading.on && !reading.replaced && !reading.error && reading.checks.includes(language)) ?? null;
}

// Each plugin as found, with what was wrong where it did not read, for the Plugins sheet.
export function pluginReadings() {
  return state.readings;
}

// The id of the plugin a file of extension `ext` opens in, or null where none names it.
export function openerOf(ext) {
  return state.byExtension.get(ext.toLowerCase())?.id ?? null;
}

// The language a file named `name` opens in, where a plugin names the file whole, or null.
export function languageForName(name) {
  return state.byName.get(name.toLowerCase()) ?? null;
}

// The language a file of extension `ext` opens in.
export function languageForExtension(ext) {
  return state.byExtension.get(ext) ?? state.byId.get("plaintext") ?? PLAIN;
}

// A plugin's text as its file would hold it, made a language: for the generator's preview.
export function languageFromText(text) {
  return languageOf(JSON.parse(text));
}

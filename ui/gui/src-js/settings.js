// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Settings by file. The reader's own settings are the page's entries, kept in settings.json in
// orior's own folder (early.js). The files settings.json takes stand under them; its part for the
// system orior_gui runs on, a tree's .orior/settings.json and the files that one takes stand over them,
// as settings.rs in the command line's crate reads them. The page's storage gives each setting as
// the files put it in force: each part of the page that reads a setting reads the files' with the
// reader's own. A setting the reader changes where a file stands over it keeps the file's value,
// and the status bar says which file holds it. The tree's settings are read as it opens and again
// as one of the tree's settings files changes.

import { invoke, listen } from "./bridge.js";
import { say } from "./statusbar.js";

export const TREE_FILE = ".orior/settings.json";

const EMPTY = { over: {}, under: {}, own: [], from: {}, kinds: {}, files: [], problems: [], system: "" };
let layers = EMPTY;
let root = "";
const listeners = [];
// The settings the status bar said a file holds, each said once a reading.
const said = new Set();

const getItem = Storage.prototype.getItem;
const setItem = Storage.prototype.setItem;
const settingOf = (storage, key) => (storage === window.localStorage && typeof key === "string" && key.startsWith("orior.") ? key.slice(6) : null);

Storage.prototype.getItem = function (key) {
  const setting = settingOf(this, key);
  if (setting !== null && Object.hasOwn(layers.over, setting)) {
    return layers.over[setting];
  }
  const own = getItem.call(this, key);
  return own === null && setting !== null && Object.hasOwn(layers.under, setting) ? layers.under[setting] : own;
};

Storage.prototype.setItem = function (key, text) {
  setItem.call(this, key, text);
  const setting = settingOf(this, key);
  if (setting === null || !Object.hasOwn(layers.over, setting) || layers.over[setting] === String(text)) {
    return;
  }
  if (layers.own.includes(setting)) {
    delete layers.over[setting];
    layers.own = layers.own.filter((one) => one !== setting);
    return;
  }
  if (!said.has(setting)) {
    said.add(setting);
    const [from] = layers.from[setting] ?? [];
    say(`${setting} stays as ${from?.file ?? "a settings file"}${from?.part ? ` sets it for ${from.part}` : " sets it"}, which stands over your own.`);
  }
  // The part of the page that took the reader's change shows the file's value again.
  queueMicrotask(() => listeners.forEach((listener) => listener(new Set([setting]))));
};

// The reader's own entry of `key`, as the page keeps it, whatever a file stands over it with.
export const ownItem = (key) => getItem.call(window.localStorage, key);

// Tells each listener the settings of `keys`, each without `orior.`, changed in the page's own
// entries, as a taking of another machine's settings changes them.
export function settingsChanged(keys) {
  if (keys.size) {
    listeners.forEach((listener) => listener(keys));
  }
}

// Tells `listener` the settings the files changed, each by its key without `orior.` and a kind's as
// `[kind] key`, each time the files are read again.
export function onSettings(listener) {
  listeners.push(listener);
}

// The settings in force as settings.rs read them: over, under, own, from, kinds, files, problems.
export function settingsInForce() {
  return layers;
}

// The setting `key` of the files of the language `kind`, or null.
export function kindSetting(kind, key) {
  return layers.kinds[kind]?.[key] ?? null;
}

const textsOf = (read) => [...Object.keys(read.over), ...Object.keys(read.under)];
const kindKeys = (read) => Object.entries(read.kinds).flatMap(([kind, set]) => Object.keys(set).map((key) => `[${kind}] ${key}`));
const kindText = (read, name) => {
  const [, kind, key] = name.match(/^\[(.*)\] (.*)$/);
  return read.kinds[kind]?.[key] ?? null;
};

// Reads the settings in force for the tree at `at` again, and tells each listener those that changed.
export async function readSettings(at = root) {
  root = at ?? "";
  const tree = await invoke("settings_tree").catch((error) => ({ files: [], problems: [String(error)] }));
  const read = await invoke("settings_layers", { tree, root }).catch((error) => ({ ...EMPTY, problems: [String(error)] }));
  const keys = new Set([...textsOf(layers), ...textsOf(read)]);
  const before = new Map([...keys].map((key) => [key, localStorage.getItem(`orior.${key}`)]));
  const kinds = new Set([...kindKeys(layers), ...kindKeys(read)]);
  const kindsBefore = new Map([...kinds].map((name) => [name, kindText(layers, name)]));
  layers = { ...EMPTY, ...read };
  said.clear();
  const changed = new Set([...keys].filter((key) => localStorage.getItem(`orior.${key}`) !== before.get(key)));
  for (const name of kinds) {
    if (kindText(layers, name) !== kindsBefore.get(name)) {
      changed.add(name);
    }
  }
  if (changed.size) {
    listeners.forEach((listener) => listener(changed));
  }
  return layers;
}

// Reads the settings again each time a settings file the tree holds changes.
export function startSettings() {
  listen("tree-changed", ({ payload }) => {
    if ((payload?.files ?? []).some((file) => file === TREE_FILE || layers.files.includes(file))) {
      readSettings();
    }
  });
}

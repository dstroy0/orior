// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The window in the reader's language. A language file, `languages/<tag>.json` in orior's own folder,
// gives each of the window's texts in English the reader's own for it, as { "File": "…" }; the
// language is the setting `language`, the system's where none is set, and English, the window's own,
// takes no file. Each text of the window a file gives is shown in its words as it is drawn: a run of
// text an element shows, matched with its spaces trimmed, and each title, placeholder and name a
// reader of the screen hears. The editor's text and the terminal's, the files' names and paths, and every text the
// reader wrote are never changed; a text a file does not give stays in English.

import { invoke } from "./bridge.js";
import { onSettings } from "./settings.js";

const KEY = "orior.language";
const ATTRIBUTES = ["title", "placeholder", "aria-label"];

// Where no text is changed: the editor, the terminal, the files' names and paths as the tabs, the
// breadcrumbs and the explorer show them, code, keys, and what the reader types; and where no title
// or name a reader of the screen hears is changed, those places that show files and text alone.
const KEPT = ".ed, .xterm, #term, .term, .tab, .crumb, .node, .files, .name, .where, .path, code, kbd, pre, textarea, input, [contenteditable], .plugin-panel, .md-preview, iframe";
const KEPT_WHOLE = ".ed, .xterm, #term, .term, .tab, .crumb, .node, .files, .plugin-panel, iframe";

// Each text and each attribute as first drawn in English, and as shown in the window's language.
const state = { tag: "en", words: null, texts: new WeakMap(), attributes: new WeakMap(), units: new WeakMap(), watching: null };

// The language the window is in, as a tag such as "de" or "pt-BR".
export function language() {
  return state.tag;
}

// The language the reader chose, or the system's.
export function chosenLanguage() {
  return localStorage.getItem(KEY) || navigator.language || "en";
}

export async function setLanguage(tag) {
  if (tag) {
    localStorage.setItem(KEY, tag);
  } else {
    localStorage.removeItem(KEY);
  }
  await takeLanguage();
}

// The languages the reader has files for, by tag.
export function languages() {
  return invoke("languages_list").catch(() => []);
}

// `text` in the window's language, or as it is where the language file does not give it.
export function inLanguage(text) {
  return state.words?.[text] ?? text;
}

// Shows the text node `node` in the window's language: from its English, where what it shows is what
// this showed last, and from what it shows where the window wrote it anew.
function turnText(node) {
  if ((node.parentElement ?? null)?.closest(KEPT)) {
    return;
  }
  const held = state.texts.get(node);
  const english = held && node.nodeValue === held.shown ? held.english : node.nodeValue;
  const trimmed = english.trim();
  const given = trimmed ? state.words?.[trimmed] : undefined;
  const shown = given === undefined ? english : english.replace(trimmed, given);
  state.texts.set(node, { english, shown });
  if (node.nodeValue !== shown) {
    node.nodeValue = shown;
  }
}

function turnAttributes(element) {
  if (element.closest(KEPT_WHOLE)) {
    return;
  }
  const held = state.attributes.get(element) ?? {};
  for (const name of ATTRIBUTES) {
    if (!element.hasAttribute(name)) {
      continue;
    }
    const now = element.getAttribute(name);
    const english = held[name] && now === held[name].shown ? held[name].english : now;
    const given = state.words?.[english.trim()];
    const shown = given === undefined ? english : given;
    held[name] = { english, shown };
    if (now !== shown) {
      element.setAttribute(name, shown);
    }
  }
  state.attributes.set(element, held);
}

// Whether `element` shows one text split by an underline of its access key, as a menu's title does.
const underlined = (element) => element.nodeType === Node.ELEMENT_NODE && Boolean(element.querySelector(":scope > u")) && [...element.childNodes].every((node) => node.nodeType === Node.TEXT_NODE || node.tagName === "U");

// Turns such an element as one text, its underline kept on the same letter where the language's
// words hold it, and the text left plain where they do not; its key stays as it was.
function turnUnderlined(element) {
  if (element.closest(KEPT)) {
    return;
  }
  const held = state.units.get(element);
  const now = element.textContent;
  const english = held && now === held.shown ? held.english : now;
  const letter = held && now === held.shown ? held.letter : (element.querySelector(":scope > u")?.textContent ?? "");
  const trimmed = english.trim();
  const given = trimmed ? state.words?.[trimmed] : undefined;
  const shown = given === undefined ? english : english.replace(trimmed, given);
  state.units.set(element, { english, shown, letter });
  if (held && now === shown) {
    return;
  }
  const at = given === undefined ? english.indexOf(letter) : shown.toLowerCase().indexOf(letter.toLowerCase());
  if (letter && at >= 0) {
    element.replaceChildren(shown.slice(0, at), Object.assign(document.createElement("u"), { textContent: shown[at] }), shown.slice(at + 1));
  } else {
    element.replaceChildren(shown);
  }
}

function turnAll(root) {
  if (root.nodeType === Node.TEXT_NODE) {
    const unit = root.parentElement?.closest("*");
    if (unit && (underlined(unit) || (unit.tagName === "U" && underlined(unit.parentElement)))) {
      turnUnderlined(unit.tagName === "U" ? unit.parentElement : unit);
    } else {
      turnText(root);
    }
    return;
  }
  if (root.nodeType !== Node.ELEMENT_NODE) {
    return;
  }
  turnAttributes(root);
  if (underlined(root)) {
    turnUnderlined(root);
    return;
  }
  const walker = document.createTreeWalker(root, NodeFilter.SHOW_ELEMENT | NodeFilter.SHOW_TEXT, {
    acceptNode: (node) => (node.nodeType === Node.ELEMENT_NODE && node.matches(".ed, .xterm, #term, iframe, pre") ? NodeFilter.FILTER_REJECT : NodeFilter.FILTER_ACCEPT),
  });
  for (let node = walker.nextNode(); node; node = walker.nextNode()) {
    if (node.nodeType === Node.TEXT_NODE) {
      if (!underlined(node.parentElement) && !(node.parentElement.tagName === "U" && underlined(node.parentElement.parentElement))) {
        turnText(node);
      }
    } else {
      turnAttributes(node);
      if (underlined(node)) {
        turnUnderlined(node);
      }
    }
  }
}

// Reads the language chosen and shows the window in it, and keeps each text drawn after in it.
export async function takeLanguage() {
  const tag = chosenLanguage();
  const base = tag.toLowerCase().split("-")[0];
  let words = null;
  if (base !== "en") {
    const text = (await invoke("language_read", { tag }).catch(() => "")) || (await invoke("language_read", { tag: base }).catch(() => ""));
    try {
      words = text ? JSON.parse(text) : null;
    } catch {
      words = null;
    }
  }
  state.tag = words ? tag : "en";
  state.words = words;
  document.documentElement.lang = state.tag;
  turnAll(document.body);
  state.watching?.disconnect();
  if (words) {
    state.watching = new MutationObserver((changes) => {
      for (const change of changes) {
        if (change.type === "characterData") {
          turnAll(change.target);
        } else if (change.type === "attributes") {
          turnAttributes(change.target);
        } else {
          change.addedNodes.forEach(turnAll);
        }
      }
    });
    state.watching.observe(document.body, { childList: true, subtree: true, characterData: true, attributes: true, attributeFilter: ATTRIBUTES });
  }
}

export function startLanguage() {
  onSettings((changed) => changed.has("language") && takeLanguage());
  return takeLanguage();
}

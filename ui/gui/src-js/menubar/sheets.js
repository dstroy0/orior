// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { invoke } from "../bridge.js";
import { markdownHtml } from "../markdown.js";
import { scheme } from "../scheme.js";
import { wordmark } from "../wordmark.js";
import { state } from "../menubar.js";
import { KEY_NAMES } from "./keys.js";

// A sheet that asks for a line of text, with `start` in it. Answers the text, or null where it was
// closed with nothing taken.
export function askFor(label, start) {
  return new Promise((done) => {
    const field = Object.assign(document.createElement("input"), { className: "report-field", type: "text", value: start, spellcheck: false, ariaLabel: label });
    const body = document.createElement("form");
    body.className = "sheet-ask";
    const row = Object.assign(document.createElement("label"), { className: "report-row" });
    row.append(Object.assign(document.createElement("span"), { textContent: label }), field);
    body.append(row);
    let answer = null;
    body.addEventListener("submit", (event) => {
      event.preventDefault();
      answer = field.value.trim() || null;
      dialog.close();
    });
    const dialog = sheet(body);
    dialog.addEventListener("close", () => done(answer));
    field.focus();
    field.select();
  });
}

// A sheet that takes the next keys pressed, as the menus write keys: Ctrl, Shift and Alt, then the
// key. Backspace alone answers no keys, and Escape answers null.
export function askKeys(label) {
  return new Promise((done) => {
    const body = document.createElement("div");
    body.className = "sheet-ask";
    const shown = Object.assign(document.createElement("kbd"), { textContent: "…" });
    body.append(Object.assign(document.createElement("p"), { textContent: label }), shown);
    let answer = null;
    const dialog = sheet(body);
    dialog.addEventListener("keydown", (event) => {
      if (["Control", "Shift", "Alt", "Meta"].includes(event.key)) {
        return;
      }
      event.preventDefault();
      event.stopPropagation();
      if (event.key === "Escape") {
        dialog.close();
        return;
      }
      const named = KEY_NAMES[event.code] ?? (/^Key[A-Z]$/.test(event.code) ? event.code.slice(3) : /^Digit\d$/.test(event.code) ? event.code.slice(5) : event.key.length === 1 ? event.key.toUpperCase() : event.key);
      answer = event.key === "Backspace" && !event.ctrlKey && !event.altKey && !event.shiftKey ? "" : [event.ctrlKey && "Ctrl", event.shiftKey && "Shift", event.altKey && "Alt", named].filter(Boolean).join("+");
      shown.textContent = answer || "…";
      window.setTimeout(() => dialog.close(), 300);
    });
    dialog.addEventListener("close", () => done(answer));
    dialog.focus();
  });
}

// A sheet that asks a question with a yes of its own, answering whether that was given.
export function askYes(question, yes) {
  return new Promise((done) => {
    const body = document.createElement("div");
    body.className = "sheet-ask";
    const button = Object.assign(document.createElement("button"), { type: "button", className: "primary", textContent: yes });
    body.append(Object.assign(document.createElement("p"), { textContent: question }), button);
    let answer = false;
    const dialog = sheet(body);
    button.addEventListener("click", () => {
      answer = true;
      dialog.close();
    });
    dialog.addEventListener("close", () => done(answer));
    button.focus();
  });
}

// The keys every menu lists, by menu, in a sheet over the app.
export function showShortcuts() {
  const body = document.createElement("div");
  body.className = "sheet-keys";
  for (const menu of state.menus) {
    const rows = menu.all.filter((item) => item.keys);
    if (!rows.length) {
      continue;
    }
    const block = document.createElement("section");
    block.append(Object.assign(document.createElement("h3"), { textContent: menu.title }));
    for (const item of rows) {
      const row = document.createElement("div");
      row.append(Object.assign(document.createElement("span"), { textContent: item.labels?.[scheme()] ?? item.label }), Object.assign(document.createElement("kbd"), { textContent: item.also ? `${item.keys}, ${item.also}` : item.keys }));
      block.append(row);
    }
    body.append(block);
  }
  sheet(body);
}

// Help, What's New: what each release of the window changed, as CHANGES.md built into the app gives
// it. The window shows it here alone and opens on no page of its own.
export async function showChanges() {
  const body = document.createElement("article");
  body.className = "md-preview sheet-changes";
  body.innerHTML = markdownHtml(await invoke("app_changes").catch(() => "# What's new in orior\n\nThe changes could not be read."));
  sheet(body);
}

export async function showAbout() {
  const body = document.createElement("div");
  body.className = "sheet-about";
  const version = await invoke("app_version").catch(() => "");
  body.append(
    Object.assign(document.createElement("span"), { className: "mark", textContent: "Σ", ariaHidden: "true" }),
    wordmark("h2"),
    Object.assign(document.createElement("p"), { textContent: version }),
    Object.assign(document.createElement("p"), { className: "tree", textContent: document.getElementById("tree-path").textContent }),
  );
  sheet(body);
}

// A sheet over the app, which Escape, a press outside it or its × closes. Answers the sheet.
export function sheet(body) {
  const dialog = document.createElement("dialog");
  dialog.className = "sheet";
  const close = Object.assign(document.createElement("button"), { type: "button", className: "sheet-close", textContent: "×", ariaLabel: "Close" });
  close.addEventListener("click", () => dialog.close());
  dialog.append(close, body);
  dialog.addEventListener("click", (event) => event.target === dialog && dialog.close());
  dialog.addEventListener("close", () => dialog.remove());
  document.body.append(dialog);
  dialog.showModal();
  return dialog;
}


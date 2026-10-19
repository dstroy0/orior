// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Search Structurally: code found by its shape, in the file open or in every file of the tree in its
// language, as shape.rs in the command line's crate finds it, each match listed under the sheet's
// fields to go to; and, with a template, every match written again from it, every file's changes
// one step undo takes back. The last pattern and template are kept, by language.

import { invoke } from "./bridge.js";
import { say } from "./statusbar.js";

const KEPT = "orior.shape-search";

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

function kept() {
  try {
    return JSON.parse(localStorage.getItem(KEPT) ?? "{}");
  } catch {
    return {};
  }
}

// Shows the sheet for the file `path` in `language`, named `languageName`. `sheet` shows a sheet,
// `open` goes to a place of a file of the tree, and `apply` writes every file's edits as one step.
export function shapeSearch({ sheet, language, languageName, path, open, apply }) {
  const last = kept()[language] ?? {};
  const body = element("form", { className: "sheet-ask shape-sheet" });
  const pattern = element("textarea", { className: "report-field shape-field", rows: 3, spellcheck: false, value: last.pattern ?? "", placeholder: "Code with placeholders: $x$ an expression, $x:name$ one name, $x:stmt$ statements" });
  pattern.setAttribute("aria-label", "Pattern");
  const template = element("textarea", { className: "report-field shape-field", rows: 2, spellcheck: false, value: last.template ?? "", placeholder: "Replace with, the same placeholders standing for what each took" });
  template.setAttribute("aria-label", "Replace with");
  const scope = element("select", { className: "report-field" }, element("option", { value: "file", textContent: `In ${path}` }), element("option", { value: "tree", textContent: `In every ${languageName} file of the tree` }));
  scope.setAttribute("aria-label", "Where");
  scope.value = last.scope ?? "file";
  const said = element("p", { className: "shape-said", ariaLive: "polite" });
  const list = element("div", { className: "shape-list" });
  const find = element("button", { type: "submit", className: "primary", textContent: "Find" });
  const replace = element("button", { type: "button", textContent: "Replace All" });
  body.append(element("h3", { textContent: "Search Structurally" }), pattern, template, element("div", { className: "shape-acts" }, scope, find, replace), said, list);
  const dialog = sheet(body);

  const ask = async (withTemplate) => {
    const all = kept();
    all[language] = { pattern: pattern.value, template: template.value, scope: scope.value };
    localStorage.setItem(KEPT, JSON.stringify(all));
    if (!pattern.value.trim()) {
      said.textContent = "Write the code to find, with placeholders where it may differ.";
      return null;
    }
    said.textContent = "Searching…";
    try {
      return await invoke("shape_search", { language, pattern: pattern.value, template: withTemplate ? template.value : null, path: scope.value === "file" ? path : null });
    } catch (error) {
      said.textContent = String(error);
      return null;
    }
  };

  body.addEventListener("submit", async (event) => {
    event.preventDefault();
    const answer = await ask(false);
    if (!answer) {
      return;
    }
    const files = new Set(answer.found.map((one) => one.path)).size;
    said.textContent = answer.found.length ? `${answer.found.length} match${answer.found.length === 1 ? "" : "es"} in ${files} file${files === 1 ? "" : "s"}.` : "Nothing has that shape.";
    list.replaceChildren(
      ...answer.found.map((one) => {
        const row = element("button", { type: "button", className: "shape-row", title: `${one.path}:${one.from.line + 1}:${one.from.col + 1}` }, element("span", { className: "where", textContent: `${one.path}:${one.from.line + 1}` }), element("span", { className: "line", textContent: one.line.trim() }));
        row.addEventListener("click", () => {
          dialog.close();
          open(one.path, one.from.line, one.from.col);
        });
        return row;
      }),
    );
  });

  replace.addEventListener("click", async () => {
    const answer = await ask(true);
    if (!answer) {
      return;
    }
    if (!answer.found.length) {
      said.textContent = "Nothing has that shape.";
      return;
    }
    await apply(answer.files);
    dialog.close();
    const files = answer.files.length;
    say(`${answer.found.length} match${answer.found.length === 1 ? "" : "es"} written again in ${files} file${files === 1 ? "" : "s"}; Undo takes them back.`);
  });
  pattern.focus();
}

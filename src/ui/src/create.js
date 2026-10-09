// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// File, Create: a file or a folder of the tree, named by its path from the tree's top, the folder of
// the file open to start with, and each folder above it that is not there yet made with it; and a
// repository, made here in the open tree or in a folder chosen, as git init makes one.

import { invoke, pick } from "./bridge.js";

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

const label = (text, ...fields) => element("label", { className: "report-row" }, element("span", { textContent: text }), ...fields);

// Asks for the path of a new file, or with `folder` a new folder, under `start`, and hands the path
// made to `made`.
export function showCreate(sheet, { folder = false, start = "", made }) {
  const path = element("input", { className: "report-field", type: "text", spellcheck: false, value: start, ariaLabel: "Path" });
  const said = element("p", { className: "report-said", ariaLive: "polite" });
  const go = element("button", { className: "primary", type: "submit", textContent: "Create" });
  const form = element(
    "form",
    { className: "sheet-report sheet-create" },
    element("h2", { textContent: folder ? "Create Folder" : "Create File" }),
    label("Path", path),
    element("div", { className: "report-foot" }, said, go),
  );
  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    const given = path.value.trim().replace(/\\/g, "/").replace(/^\/+|\/+$/g, "");
    if (!given) {
      said.textContent = "Path is required.";
      path.focus();
      return;
    }
    try {
      await invoke(folder ? "folder_create" : "file_create", { path: given });
      dialog.close();
      await made(given);
    } catch (error) {
      said.textContent = String(error);
    }
  });
  const dialog = sheet(form);
  path.focus();
  path.setSelectionRange(path.value.length, path.value.length);
  return dialog;
}

// Asks whether the repository goes here, in the open tree, or in a folder chosen, makes it, and
// hands the folder to `made`.
export async function showInit(sheet, { given = null, made }) {
  const root = await invoke("root_get").catch(() => null);
  const here = element("input", { type: "radio", name: "init-where", checked: Boolean(root) && !given, disabled: !root });
  const elsewhere = element("input", { type: "radio", name: "init-where", checked: !root || Boolean(given) });
  const folder = element("input", { className: "report-field", type: "text", spellcheck: false, value: given ?? "", ariaLabel: "Folder" });
  const choose = element("button", { className: "prefs-button", type: "button", textContent: "Choose Folder…" });
  const said = element("p", { className: "report-said", ariaLive: "polite" });
  const go = element("button", { className: "primary", type: "submit", textContent: "Create" });
  const form = element(
    "form",
    { className: "sheet-report sheet-clone" },
    element("h2", { textContent: "Create Repository" }),
    element(
      "fieldset",
      { className: "clone-where" },
      element("legend", { textContent: "Where" }),
      element("label", { className: "clone-choice" }, here, element("span", { textContent: root ? `Here, in ${root}` : "Here" })),
      element("label", { className: "clone-choice" }, elsewhere, element("span", { textContent: "In a folder" }), element("span", { className: "clone-folder" }, folder, choose)),
    ),
    element("div", { className: "report-foot" }, said, go),
  );
  folder.addEventListener("input", () => (elsewhere.checked = true));
  choose.addEventListener("click", async () => {
    const chosen = await pick("dir");
    if (typeof chosen === "string") {
      folder.value = chosen;
      elsewhere.checked = true;
    }
  });
  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    if (elsewhere.checked && !folder.value.trim()) {
      said.textContent = "Folder is required.";
      folder.focus();
      return;
    }
    try {
      const at = await invoke("repo_init", { folder: here.checked ? null : folder.value.trim() });
      dialog.close();
      await made(at, here.checked);
    } catch (error) {
      said.textContent = String(error);
    }
  });
  const dialog = sheet(form);
  go.focus();
  return dialog;
}

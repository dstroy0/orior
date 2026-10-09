// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// File, Clone Repository: a repository's address, orior's own to start with, and the folder its
// clone is made in, beside the open tree or somewhere else, the reader's home to start with where no
// tree is open. Clone runs git as git.rs in the command line's
// crate runs it, and a fuse across the sheet burns as git's progress comes, flaring at each line.
// A clone made flashes and opens as the tree; one that fails sputters out and says why. A clone goes
// on when the sheet is closed, and opens all the same.

import { invoke, listen, pick } from "./bridge.js";
import { makeFuse } from "./fuse.js";

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

// How long a clone made flashes before the sheet closes, in milliseconds.
const FLASHED = 900;

// Where the folder the last clone was made in is kept, which the next clone starts from.
const PARENT = "orior.clone.parent";

// Each stage git writes its progress for, and the stretch of the fuse it burns: the server counting
// and packing, the objects coming, the changes worked out between them, and the files written out.
const STAGES = [
  [/^remote: (Enumerating|Counting|Compressing) objects/, 0, 0.05],
  [/^Receiving objects/, 0.05, 0.8],
  [/^Resolving deltas/, 0.8, 0.9],
  [/^(Updating files|Checking out files)/, 0.9, 1],
];

// How far across the fuse a line of git's progress puts the clone, or null for a line that does
// not say.
function partOf(line) {
  const stage = STAGES.find(([pattern]) => pattern.test(line));
  const percent = line.match(/(\d+)%/);
  if (!stage) {
    return null;
  }
  const [, from, to] = stage;
  return from + ((to - from) * (percent ? Number(percent[1]) : 0)) / 100;
}

// The clone going now: what its fuse burns and the line that names its stage.
let going = null;
let listening = false;

// The folder a clone of `url` makes, named as git names it.
function nameOf(url) {
  const address = url.trim().replace(/[\\/]+$/, "");
  return (address.endsWith(".git") ? address.slice(0, -4) : address).split(/[\\/:]/).pop();
}

// With `open`, it is File, Open, Repository: the clone goes in a folder of orior's own, and a
// repository cloned there before opens as it is.
export async function showClone(sheet, openFolder, given = [], { open = false } = {}) {
  if (!listening) {
    listening = true;
    await listen("clone-progress", (event) => {
      if (!going) {
        return;
      }
      const part = partOf(event.payload);
      if (part !== null) {
        going.run.part = Math.max(going.run.part, part);
      }
      going.said.textContent = event.payload.replace(/\s*\(.*$/, "");
      going.fuse.flare();
    });
  }
  const start = await invoke("clone_start").catch(() => ({ url: "", parent: "" }));
  // The folder that holds the open tree, which a clone beside it goes in.
  const root = open ? null : await invoke("root_get").catch(() => null);
  const beside = root ? root.replace(/[\\/][^\\/]*[\\/]?$/, "") : null;
  const kept = open ? await invoke("repos_folder").catch(() => "") : "";
  const url = element("input", { className: "report-field", type: "text", spellcheck: false, value: given[0] ?? (open ? "" : start.url), ariaLabel: "Repository" });
  const parent = element("input", { className: "report-field", type: "text", spellcheck: false, value: given[1] ?? localStorage.getItem(PARENT) ?? start.parent, ariaLabel: "Folder" });
  const choose = element("button", { className: "prefs-button", type: "button", textContent: "Choose Folder…" });
  const nextTo = element("input", { type: "radio", name: "clone-where", checked: Boolean(beside) && !given[1], disabled: !beside });
  const elsewhere = element("input", { type: "radio", name: "clone-where", checked: !beside || Boolean(given[1]) });
  const said = element("p", { className: "report-said", ariaLive: "polite" });
  const go = element("button", { className: "primary", type: "submit", textContent: open ? "Open" : "Clone" });
  let run = null;
  const fuse = makeFuse(() => run?.part ?? 0);
  const label = (text, ...fields) => element("label", { className: "report-row" }, element("span", { textContent: text }), ...fields);
  const chosen = () => (open ? kept : nextTo.checked ? beside : parent.value.trim());
  const where = () => {
    const name = nameOf(url.value);
    const base = (chosen() ?? "").replace(/[\\/]+$/, "");
    said.textContent = name && base ? `${open ? "Opens" : "Makes"} ${base}${base.includes("\\") ? "\\" : "/"}${name}` : "";
  };
  const folders = open
    ? null
    : element(
        "fieldset",
        { className: "clone-where" },
        element("legend", { textContent: "In Folder" }),
        element("label", { className: "clone-choice" }, nextTo, element("span", { textContent: beside ? `Beside the open tree, in ${beside}` : "Beside the open tree" })),
        element("label", { className: "clone-choice" }, elsewhere, element("span", { textContent: "Somewhere else" }), element("span", { className: "clone-folder" }, parent, choose)),
      );
  const form = element(
    "form",
    { className: "sheet-report sheet-clone" },
    element("h2", { textContent: open ? "Open Repository" : "Clone Repository" }),
    label("Repository", url),
    folders,
    fuse.canvas,
    element("div", { className: "report-foot" }, said, go),
  );
  const parts = [url, parent, choose, nextTo, elsewhere, go];
  const takeElsewhere = () => {
    elsewhere.checked = true;
    where();
  };
  url.addEventListener("input", where);
  parent.addEventListener("input", takeElsewhere);
  nextTo.addEventListener("change", where);
  elsewhere.addEventListener("change", where);
  choose.addEventListener("click", async () => {
    const folder = await pick("dir");
    if (typeof folder === "string") {
      parent.value = folder;
      takeElsewhere();
    }
  });
  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    if (!url.value.trim() || !chosen()) {
      said.textContent = !url.value.trim() ? "Repository is required." : "In Folder is required.";
      (url.value.trim() ? parent : url).focus();
      return;
    }
    // A repository Open has cloned before opens as it is.
    const before = open ? await invoke("repo_opened", { url: url.value.trim() }).catch(() => null) : null;
    if (before) {
      dialog.close();
      await openFolder(before);
      return;
    }
    parts.forEach((part) => (part.disabled = true));
    // Each try is a run of its own, which the fuse lights from its start.
    run = { part: 0, done: false, code: 0, stopped: false };
    fuse.follow(run);
    said.textContent = "Cloning…";
    going = { run, said, fuse };
    try {
      const made = await invoke("repo_clone", { url: url.value.trim(), parent: chosen() });
      Object.assign(run, { part: 1, done: true, code: 0 });
      fuse.follow(run);
      if (!open && elsewhere.checked) {
        localStorage.setItem(PARENT, parent.value.trim());
      }
      said.textContent = `Cloned into ${made}.`;
      await new Promise((resolve) => window.setTimeout(resolve, FLASHED));
      dialog.close();
      await openFolder(made);
    } catch (error) {
      Object.assign(run, { done: true, code: 1 });
      fuse.follow(run);
      said.textContent = String(error);
      parts.forEach((part) => (part.disabled = false));
    } finally {
      going = null;
    }
  });
  const dialog = sheet(form);
  fuse.follow(null);
  where();
  go.focus();
  return dialog;
}

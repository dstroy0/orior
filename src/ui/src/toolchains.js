// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// File, Toolchains: every toolchain toolchains.json lists, a group at a time, each with what it is
// for, where orior found its program and the version it says, and what can be done about it. One
// not found opens its install page, for the reader to install it from its own makers, or takes a
// folder the reader picks. One installed and not on the PATH goes on it, or orior runs it from
// there. Above them, whether orior itself is on the PATH, which `orior` needs to work in any terminal.
//
// orior's own runs and terminals read the PATH anew, and a change here reaches them at once; a
// terminal opened outside orior before the change does not have it.

import { invoke, pick } from "./bridge.js";

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

function button(text, act, props = {}) {
  const made = element("button", { type: "button", className: "prefs-button", textContent: text, ...props });
  made.addEventListener("click", act);
  return made;
}

const STATES = {
  env: () => "named by its variable",
  chosen: () => "from your folder",
  path: () => "on PATH",
  found: () => "installed, not on PATH",
  missing: () => "not found",
};

export async function showToolchains(sheet) {
  const body = element("div", { className: "sheet-tools" });
  const said = element("p", { className: "prefs-said", ariaLive: "polite" });
  const own = element("div", { className: "tools-own" });
  const filter = element("input", { className: "filter", type: "search", placeholder: "Search", spellcheck: false, ariaLabel: "Search toolchains" });
  const list = element("div", { className: "tools-list" });
  let read = { tools: [], own: null };
  const versions = new Map();

  const act = async (work, done) => {
    said.textContent = "";
    try {
      const answer = await work();
      said.textContent = done(answer);
      await check();
    } catch (error) {
      said.textContent = String(error);
    }
  };

  const choose = async (tool) => {
    const folder = await pick("dir");
    if (typeof folder === "string") {
      await act(() => invoke("toolchain_use", { id: tool.id, folder }), (program) => `orior runs ${program}.`);
    }
  };

  function actions(tool) {
    const made = [];
    if (tool.state === "found") {
      made.push(button("Add to PATH", () => act(() => invoke("toolchain_add_path", { what: tool.id }), (folder) => `${folder} is on your PATH for orior's runs and every terminal opened from now.`)));
      made.push(button("Use This Folder", () => act(() => invoke("toolchain_use", { id: tool.id, folder: tool.folder }), (program) => `orior runs ${program}.`)));
    }
    if (tool.chosen) {
      made.push(button("Forget Folder", () => act(() => invoke("toolchain_forget", { id: tool.id }), () => `orior looks for ${tool.name} on the PATH again.`), { title: tool.chosen }));
    } else if (tool.state !== "found") {
      made.push(button("Choose Folder…", () => choose(tool)));
    }
    if (tool.install) {
      made.push(button("Install Page", () => act(() => invoke("toolchain_install", { id: tool.id }), (url) => `Opened ${url}.`), { title: tool.install, className: `prefs-button${tool.state === "missing" ? " tools-go" : ""}` }));
    }
    return element("span", { className: "tools-actions" }, ...made);
  }

  function row(tool) {
    const version = versions.get(tool.id);
    const where = tool.program ? element("code", { className: "tools-where", textContent: tool.program, title: tool.program }) : null;
    return element(
      "div",
      { className: `tools-row ${tool.state}` },
      element("span", { className: "tools-dot", ariaHidden: "true" }),
      element("span", { className: "tools-name" }, element("strong", { textContent: tool.name }), element("span", { textContent: tool.for })),
      element(
        "span",
        { className: "tools-state" },
        element("span", { className: "tools-said", textContent: version ? `${STATES[tool.state](tool)}, ${version}` : STATES[tool.state](tool), title: version ?? "" }),
        where,
      ),
      actions(tool),
    );
  }

  function draw() {
    const word = filter.value.trim().toLowerCase();
    const shown = read.tools.filter((tool) => !word || [tool.id, tool.name, tool.for, tool.group, ...tool.programs].some((text) => text.toLowerCase().includes(word)));
    const rows = [];
    let group = null;
    for (const tool of shown) {
      if (tool.group !== group) {
        group = tool.group;
        rows.push(element("h3", { textContent: group }));
      }
      rows.push(row(tool));
    }
    list.replaceChildren(...rows);
    if (!shown.length) {
      list.append(element("p", { className: "prefs-said", textContent: "No toolchain matches." }));
    }
    const mine = read.own;
    const parts = [
      mine
        ? element("span", {
            className: mine.on_path ? "on" : "off",
            title: mine.folder,
            textContent: mine.on_path ? "orior is on your PATH: `orior` works in any terminal." : "orior is not on your PATH: `orior` works only from its own folder.",
          })
        : element("span", { textContent: "orior could not find its own folder." }),
    ];
    if (mine && !mine.on_path) {
      parts.push(button("Add orior to PATH", () => act(() => invoke("toolchain_add_path", { what: "orior" }), (folder) => `${folder} is on your PATH for every terminal opened from now.`), { className: "prefs-button tools-go", title: mine.folder }));
    }
    own.replaceChildren(...parts);
  }

  // Asks each tool found its version, each at once, and shows each as it comes.
  function askVersions() {
    for (const tool of read.tools) {
      if (!tool.program || !tool.versioned || versions.has(tool.id)) {
        continue;
      }
      invoke("toolchain_version", { id: tool.id })
        .then((version) => {
          versions.set(tool.id, version);
          draw();
        })
        .catch(() => {});
    }
  }

  async function check() {
    read = await invoke("toolchains_check");
    versions.clear();
    draw();
    askVersions();
  }

  filter.addEventListener("input", draw);
  body.append(element("h2", { textContent: "Toolchains" }), own, element("div", { className: "prefs-actions" }, button("Check Again", () => check().then(() => (said.textContent = "Checked every toolchain again.")))), said, filter, list);
  sheet(body);
  filter.focus();
  await check();
}

// `toolchains` as the menus and the palette give it words: none shows the sheet, and the rest act
// as `orior file toolchains` does.
export async function runToolchains(sheet, args = []) {
  const [word, first, second] = args;
  if (word === "install" && first) {
    return invoke("toolchain_install", { id: first });
  }
  if (word === "add-path" && first) {
    return invoke("toolchain_add_path", { what: first });
  }
  if (word === "use" && first && second) {
    return invoke("toolchain_use", { id: first, folder: second });
  }
  if (word === "forget" && first) {
    return invoke("toolchain_forget", { id: first });
  }
  return showToolchains(sheet);
}

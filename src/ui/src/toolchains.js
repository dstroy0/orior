// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// File, Toolchains: every toolchain toolchains.json lists, a group at a time, each with what it is
// for, where orior found its program and the version it says, and what can be done about it. One
// not found opens its install page, for the reader to install it from its own makers, or takes a
// folder the reader picks. One installed and not on the PATH goes on it, or orior runs it from
// there. Above them, whether orior itself is on the PATH, which `orior` needs to work in any terminal.
//
// Add Toolchain adds one of the reader's own to a group, a new one or one there already, by its name,
// what it is for, the programs that name it, the words that make it say its version and its install
// page; Add Group adds a group that stands empty until a toolchain goes in it. What the reader added
// is taken out from its row, and an empty group from its heading.
//
// orior's own runs and terminals read the PATH anew, and a change here reaches them at once; a
// terminal opened outside orior before the change does not have it.

import { invoke, pick } from "./bridge.js";
import { runInTerminal } from "./terminal.js";

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
    // Install runs the line its makers give in the terminal, where its progress shows.
    if (tool.setup && tool.state === "missing") {
      made.push(button("Install", () => act(() => install(tool), () => `Installing ${tool.name} in the terminal. Check Again once it is done.`), { className: "prefs-button tools-go" }));
    }
    if (tool.install) {
      made.push(button("Install Page", () => act(() => invoke("toolchain_install", { id: tool.id }), (url) => `Opened ${url}.`), { title: tool.install, className: `prefs-button${tool.state === "missing" && !tool.setup ? " tools-go" : ""}` }));
    }
    if (tool.added) {
      made.push(button("Remove", () => act(() => invoke("toolchain_remove", { id: tool.id }), () => `Took out ${tool.name}.`)));
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
    for (const group of read.groups ?? []) {
      const held = shown.filter((tool) => tool.group === group);
      const empty = !read.tools.some((tool) => tool.group === group);
      if (!held.length && !(empty && (!word || group.toLowerCase().includes(word)))) {
        continue;
      }
      const heading = element("h3", { textContent: group });
      if (empty) {
        heading.append(button("Remove Group", () => act(() => invoke("toolchain_remove_group", { name: group }), () => `Took out ${group}.`), { className: "prefs-button tools-group-remove" }));
      }
      rows.push(heading, ...held.map(row));
      if (empty) {
        rows.push(element("p", { className: "prefs-said", textContent: "No toolchain in it yet." }));
      }
    }
    list.replaceChildren(...rows);
    if (!rows.length) {
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

  // The form that adds a toolchain or a group, shown in place of nothing under the sheet's buttons.
  const adding = element("form", { className: "tools-add", hidden: true });
  const field = (label, input) => element("label", { className: "report-row" }, element("span", { textContent: label }), input);
  const input = (props) => element("input", { className: "report-field", type: "text", spellcheck: false, ...props });
  function addForm(kind) {
    const groups = element("datalist", { id: "tools-groups" }, ...(read.groups ?? []).map((group) => element("option", { value: group })));
    const name = input({ ariaLabel: "Name", required: true });
    const parts = kind === "group" ? [field("Group", name)] : [];
    const group = input({ ariaLabel: "Group", value: read.groups?.at(-1) ?? "", required: true });
    group.setAttribute("list", "tools-groups");
    const uses = input({ ariaLabel: "For" });
    const programs = input({ ariaLabel: "Programs", placeholder: "zig, zig.exe", required: true });
    const version = input({ ariaLabel: "Version", value: "--version" });
    const page = input({ ariaLabel: "Install Page", type: "url", placeholder: "https://" });
    if (kind === "tool") {
      parts.push(field("Name", name), field("Group, one here or a new one", group), field("For", uses), field("Programs, by the names it runs as", programs), field("Says its version when given", version), field("Install Page", page));
    }
    const cancel = button("Cancel", () => (adding.hidden = true));
    adding.replaceChildren(element("h3", { textContent: kind === "group" ? "Add Group" : "Add Toolchain" }), groups, ...parts, element("div", { className: "prefs-actions" }, element("button", { type: "submit", className: "primary", textContent: "Add" }), cancel));
    adding.onsubmit = (event) => {
      event.preventDefault();
      const words = (text) => text.split(/[,\s]+/).map((one) => one.trim()).filter(Boolean);
      const tool = {
        id: "",
        name: name.value,
        group: group.value,
        for: uses.value,
        programs: words(programs.value),
        version: words(version.value).length ? words(version.value) : null,
        install: page.value.trim() ? { any: page.value.trim() } : {},
      };
      const work = kind === "group" ? () => invoke("toolchain_add_group", { name: name.value }) : () => invoke("toolchain_add", { tool });
      act(work, (id) => (kind === "group" ? `Added the group ${name.value.trim()}.` : `Added ${id}.`)).then(() => {
        if (!said.textContent.startsWith("Added")) {
          return;
        }
        adding.hidden = true;
      });
    };
    adding.hidden = false;
    name.focus();
  }

  filter.addEventListener("input", draw);
  body.append(
    element("h2", { textContent: "Toolchains" }),
    own,
    element(
      "div",
      { className: "prefs-actions" },
      button("Check Again", () => check().then(() => (said.textContent = "Checked every toolchain again."))),
      button("Add Toolchain…", () => addForm("tool")),
      button("Add Group…", () => addForm("group")),
    ),
    adding,
    said,
    filter,
    list,
  );
  sheet(body);
  filter.focus();
  await check();
}

// Installs a tool by the line its makers give, in the terminal.
async function install(tool) {
  runInTerminal(await invoke("toolchain_setup", { id: tool.id }));
}

// `toolchains` as the menus and the palette give it words: none shows the sheet, and the rest act
// as `orior file toolchains` does.
export async function runToolchains(sheet, args = []) {
  const [word, first, second] = args;
  if (word === "install" && first) {
    const line = await invoke("toolchain_setup", { id: first }).catch(() => null);
    return line ? runInTerminal(line) : invoke("toolchain_install", { id: first });
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

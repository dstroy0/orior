// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { invoke } from "../bridge.js";
import { checkersNamed, setCheckers } from "../servers.js";
import { lastMemory, memoryBudget, say, setMemoryBudget } from "../statusbar.js";
import { editing, openAt as openFileAt } from "../edit.js";
import { openPalette } from "../palette.js";
import { showMenu } from "../menu.js";
import { runInTerminal, terminalAt } from "../terminal.js";
import { applyEdits } from "../intel.js";
import { askFor, sheet } from "./sheets.js";

// Edit, Formatting, Continuation Indent: the columns a new line inside a bracket is indented by, for
// a call's arguments, a declaration's parameters and any other bracket, each as given or asked for;
// one left empty is one step of the indent.
export function askContinuation(given) {
  const now = editing().continuation();
  const places = [["call", "Calls' arguments"], ["declaration", "Declarations' parameters"], ["expression", "Any other bracket"]];
  const apply = (values) => {
    const set = {};
    places.forEach(([place], index) => {
      const columns = Number(values[index]);
      if (Number.isFinite(columns) && columns > 0) {
        set[place] = Math.round(columns);
      }
    });
    editing().setContinuation(set);
    say(`A new line inside a bracket is indented by ${places.map(([place, label]) => `${set[place] ?? "one step"} for ${label.toLowerCase()}`).join(", ")}.`);
  };
  if (given.length) {
    apply(given);
    return;
  }
  const form = document.createElement("form");
  form.className = "sheet-report sheet-create";
  const fields = places.map(([place, label]) => {
    const field = Object.assign(document.createElement("input"), { className: "report-field", type: "number", min: 1, max: 16, value: now[place] ?? "", placeholder: "One step of the indent" });
    field.setAttribute("aria-label", label);
    const row = Object.assign(document.createElement("label"), { className: "report-row" });
    row.append(Object.assign(document.createElement("span"), { textContent: label }), field);
    return [row, field];
  });
  const go = Object.assign(document.createElement("button"), { className: "primary", type: "submit", textContent: "Set" });
  const foot = Object.assign(document.createElement("div"), { className: "report-foot" });
  foot.append(go);
  form.append(Object.assign(document.createElement("h2"), { textContent: "Continuation Indent" }), ...fields.map(([row]) => row), foot);
  const dialog = sheet(form);
  form.addEventListener("submit", (event) => {
    event.preventDefault();
    apply(fields.map(([, field]) => field.value));
    dialog.close();
  });
  fields[0][1].focus();
}

// File, Maven Settings: the compiler's settings the POM open gives, the tree's top POM where none is
// open, and Maven's own, from settings.xml, each a field; Set writes those changed, the POM's into it
// as edits and Maven's own into the user's settings.xml.
export async function askMavenSettings() {
  const ed = editing();
  const file = ed.fileHere();
  const pom = file && /(^|\/)pom\.xml$/i.test(file) ? file : null;
  let rows;
  try {
    rows = await invoke("maven_settings", { path: pom, text: pom ? ed.textHere() : null });
  } catch (error) {
    say(String(error), { failed: true });
    return;
  }
  const form = document.createElement("form");
  form.className = "sheet-report sheet-maven";
  const fields = [];
  const parts = [Object.assign(document.createElement("h2"), { textContent: "Maven Settings" })];
  for (const [group, title] of [["pom", `The compiler, as ${pom ?? "pom.xml"} sets it`], ["settings", "Maven's own"]]) {
    const shown = rows.filter((row) => row.group === group);
    if (shown.length) {
      parts.push(Object.assign(document.createElement("h3"), { textContent: title }));
    }
    for (const row of shown) {
      const field = Object.assign(document.createElement("input"), { className: "report-field", value: row.value, readOnly: !row.settable, placeholder: row.settable ? "Not set" : "None", title: row.file });
      field.setAttribute("aria-label", row.label);
      const line = Object.assign(document.createElement("label"), { className: "report-row" });
      line.append(Object.assign(document.createElement("span"), { textContent: row.label }), field);
      parts.push(line);
      if (row.settable) {
        fields.push([row, field]);
      }
    }
  }
  const go = Object.assign(document.createElement("button"), { className: "primary", type: "submit", textContent: "Set" });
  const foot = Object.assign(document.createElement("div"), { className: "report-foot" });
  foot.append(go);
  form.append(...parts, foot);
  const dialog = sheet(form);
  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    const changed = fields.filter(([row, field]) => field.value.trim() !== row.value);
    for (const [row, field] of changed) {
      try {
        const now = pom && editing().fileHere() === pom ? editing().textHere() : null;
        await applyEdits(await invoke("maven_set", { path: pom, text: now, key: row.key, value: field.value }));
      } catch (error) {
        say(String(error), { failed: true });
        return;
      }
    }
    say(changed.length ? `${changed.map(([row]) => row.label).join(", ")} set.` : "No setting was changed.");
    dialog.close();
  });
  fields[0]?.[1].focus();
}

// Run, Find Tests: the tree's Python tests, found as pytest and unittest find them with no setup, in
// the quick open, each by its class and its name, then its file and line; Enter opens it.
export async function showTests() {
  let found;
  try {
    found = await invoke("tests_found");
  } catch (error) {
    say(String(error), { failed: true });
    return;
  }
  openPalette("", {
    custom: {
      placeholder: `${found.length} test${found.length === 1 ? "" : "s"}: type to filter, Enter to open one`,
      empty: "No test is found: pytest and unittest take files named test_*.py, *_test.py and test*.py, and in them the functions and methods whose names start with test.",
      items: found.map((test) => {
        const label = test.class ? `${test.class}.${test.name}` : test.name;
        return { text: `${label} ${test.file}`, label, detail: `${test.file}:${test.line + 1}`, run: () => openFileAt(test.file, test.line, 0) };
      }),
    },
  });
}

// Run, Update Snapshots: fetches the newest build of each snapshot dependency of the build the file
// open belongs to, or of the tree's top build, in the terminal.
export async function updateSnapshots() {
  try {
    const [folder, line] = await invoke("snapshots_line", { path: editing().fileHere() });
    terminalAt(folder);
    runInTerminal(line);
  } catch (error) {
    say(String(error), { failed: true });
  }
}

// Edit, Formatting, Documentation Margin: the column documentation is filled to, as given or asked
// for; none has documentation filled to the code's margin.
export async function askDocMargin(given) {
  const answer = given ?? (await askFor("The column documentation is filled to, or none for the code's", String(editing().docMargin() ?? "")));
  if (answer === null || answer === undefined) {
    return;
  }
  const columns = Number(answer);
  if (answer.trim() === "" || answer.trim() === "none") {
    editing().setDocMargin(null);
    say("Documentation is filled to the code's margin.");
  } else if (Number.isFinite(columns) && columns >= 20) {
    editing().setDocMargin(Math.round(columns));
    say(`Documentation is filled to column ${Math.round(columns)}.`);
  } else {
    say(`${answer} is no margin: a column of 20 or more, or none.`, { failed: true });
  }
}

// File, Tree Environment: the environments the tree holds, each by its folder in the tree and its
// type, the one in use checked, and the toolchains' own Python; the one chosen kept for the tree by
// its folder in it, which moves with the tree, and taken up again as the tree opens. A tree with none
// chosen takes its own virtual environment, the one nearest its top, with no setup. An environment
// chosen says which packages the files beside it require that it has not installed.
const ENV_TYPES = { venv: "virtual environment", pipenv: "Pipenv", nix: "Nix" };

// The environment the tree open uses, or null for the toolchains' own Python.
let environmentInUse = null;

function environmentKey() {
  return `orior.environment.${document.getElementById("tree-path").textContent}`;
}

// Uses `env` for the tree, or the toolchains' own Python for null, kept as the reader's choice where
// `kept`.
async function useEnvironment(env, { quiet = false, kept = true } = {}) {
  try {
    const used = await invoke("env_use", { kind: env?.kind ?? null, place: env?.place ?? "", requires: env?.requires ?? [] });
    environmentInUse = env ?? null;
    if (kept) {
      localStorage.setItem(environmentKey(), JSON.stringify({ kind: env?.kind ?? null, place: env?.place ?? "" }));
    }
    if (!quiet) {
      const where = env ? `${env.place || "the tree's top folder"}, a ${ENV_TYPES[env.kind]}` : "the toolchains' own";
      const missing = used.missing.length ? `; it has not installed ${used.missing.join(", ")}` : "";
      say(`The tree's environment is ${where}${used.python ? `, its Python ${used.python}` : ""}${missing}.`, { failed: Boolean(used.missing.length) });
    }
  } catch (error) {
    say(String(error), { failed: true });
  }
}

export async function chooseEnvironment(given) {
  const found = await invoke("envs_found").catch(() => []);
  if (given) {
    const env = given === "none" ? null : found.find((one) => one.place === given || `${one.kind}:${one.place}` === given);
    if (env === undefined) {
      say(`The tree holds no environment at ${given}.`, { failed: true });
      return;
    }
    await useEnvironment(env);
    return;
  }
  const items = [
    { label: "The Toolchains' Own Python", checked: !environmentInUse, command: "environment", args: ["none"], run: () => useEnvironment(null) },
    ...(found.length ? ["-"] : []),
    ...found.map((env) => ({ label: `${env.place || "The tree's top folder"}: ${ENV_TYPES[env.kind]}${env.requires.length ? `, ${env.requires.length} package${env.requires.length === 1 ? "" : "s"} required` : ""}`, checked: environmentInUse?.kind === env.kind && environmentInUse?.place === env.place, command: "environment", args: [`${env.kind}:${env.place}`], run: () => useEnvironment(env) })),
  ];
  showMenu(window.innerWidth / 3, window.innerHeight / 4, items);
}

// Takes up again the environment kept for the tree open, where one is kept and the tree still holds
// it; where none is kept, the tree's own virtual environment nearest its top, not kept, for one the
// tree comes to hold later to be taken in its place.
export async function restoreEnvironment() {
  const kept = JSON.parse(localStorage.getItem(environmentKey()) ?? "null");
  if (kept && kept.kind === null) {
    await useEnvironment(null, { quiet: true, kept: false });
    return;
  }
  const found = await invoke("envs_found").catch(() => []);
  if (!kept) {
    await useEnvironment(found.find((one) => one.kind === "venv") ?? null, { quiet: true, kept: false });
    return;
  }
  const env = found.find((one) => one.kind === kept.kind && one.place === kept.place);
  await useEnvironment(env ?? null, { quiet: true, kept: false });
}

// View, Checkers: the type checkers and linters to run as files change, by name, as given or asked
// for among those the manifest knows; a name none answers to is said.
export async function askCheckers(given) {
  const known = await invoke("checkers_known").catch(() => []);
  const answer = given || (await askFor(`The checkers to run as files change, a comma between each: ${known.map(([, name]) => name).join(", ")}`, checkersNamed().join(", ")));
  if (answer === null || answer === undefined) {
    return;
  }
  const names = answer.split(",").map((name) => name.trim()).filter(Boolean);
  const unknown = await setCheckers(names);
  if (unknown.length) {
    say(`No checker is named ${unknown.join(" or ")}.`, { failed: true });
  } else {
    say(names.length ? `${names.join(", ")} check files as they change.` : "No checker runs as files change.");
  }
}

// View, Memory Budget: the megabytes the app is kept to in RAM, as given or asked for.
export async function askBudget(given) {
  const answer = given ?? (await askFor("The memory budget, in megabytes", String(memoryBudget())));
  const mb = Number(answer);
  if (answer && Number.isFinite(mb) && mb > 0) {
    setMemoryBudget(mb);
    say(`The memory budget is ${Math.round(mb)} MB.`);
  } else if (answer) {
    say(`${answer} is no number of megabytes.`, { failed: true });
  }
}

// The memory reading's menu: what each program the app started holds, the budget, and the servers no
// open tab needs to stop.
export function showMemory() {
  const node = document.getElementById("status-memory");
  const read = lastMemory();
  const mb = (bytes) => `${Math.round(bytes / (1024 * 1024))} MB`;
  const parts = (read?.parts ?? []).map((part) => ({ label: `${part.name}: ${mb(part.working)}`, disabled: true }));
  const box = node.getBoundingClientRect();
  showMenu(
    box.left,
    box.top,
    [...parts, ...(parts.length ? ["-"] : []), { label: `Memory Budget: ${memoryBudget()} MB…`, command: "memory-budget", args: [], run: () => askBudget() }, { label: "Stop Servers No Open Tab Needs", command: "servers-stop-idle", args: [], run: () => editing().holdMemory(Number.MAX_SAFE_INTEGER) }],
    { anchor: node },
  );
}


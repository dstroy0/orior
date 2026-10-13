// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The Database tool window: the connections the tree's databases.json names, PostgreSQL, MySQL or
// MariaDB, and SQLite, each connected to, edited and taken out from its bar; the schema of the one
// chosen, read a level at a time as each is opened, a reading stopped from the bar; and the rows a
// statement gives, a page at a time as the grid scrolls, the server holding them and the grid only
// those in view.
//
// A statement runs from a .sql file of the tree: Run Statement takes the statement the cursor stands
// in, or the text selected, and runs nothing where the cursor stands in no statement; Run File runs
// each statement of the file in turn and stops at the first that fails. Each file runs on the
// connection chosen for it, kept by its path, or on the one the window has chosen. A statement the
// server says is wrong puts the cursor where the server says. A write that would reach every row of
// what it names, and a write on a connection that matters set to ask, asks first, with the count of
// rows. A statement that makes, changes or drops an object reads again the level that holds it.
//
// The status bar names the connection of the file shown, the zone its session reads times in, and
// a transaction open, which it commits or rolls back from its menu.

import { Ahead } from "./ahead.js";
import { invoke } from "./bridge.js";
import { menuOn, showMenu } from "./menu.js";
import { say } from "./statusbar.js";

// The rows asked for at a time past the end, the height of a row of the grid, and the fewest rows
// read beyond those in view each way.
const PAGE = 200;
const ROW = 22;
const SPARE = 40;

const CHOSEN = "orior.database.chosen";
const FILES = "orior.database.files";

const state = {
  hooks: null,
  connections: [],
  open: new Map(),
  // The schema's levels read, by their path, and those opened.
  levels: new Map(),
  opened: new Set(),
  reading: 0,
  // The result shown: the connection it came from, its columns, the rows held here by their index,
  // how many the server has read and whether it has read them all.
  result: null,
  running: null,
};
const parts = {};

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

const chosen = () => localStorage.getItem(CHOSEN) ?? "";
const connectionOf = (name) => state.connections.find((one) => one.connection.name === name)?.connection ?? null;
const keyOf = (path) => path.join("\u0000");

function filesKept() {
  try {
    return JSON.parse(localStorage.getItem(FILES) ?? "{}") ?? {};
  } catch {
    return {};
  }
}

// The connection a file runs on: the one chosen for it, or the window's.
export function connectionForFile(path) {
  return filesKept()[path] || chosen();
}

function chooseForFile(path, name) {
  const files = filesKept();
  files[path] = name;
  localStorage.setItem(FILES, JSON.stringify(files));
  drawStatus();
}

// Reads the tree's connections and which are open, and draws the bar and the status bar again.
export async function readConnections() {
  state.connections = await invoke("db_list").catch((error) => (say(String(error), { failed: true }), []));
  state.open = new Map(state.connections.filter((one) => one.open).map((one) => [one.connection.name, one]));
  drawBar();
  drawStatus();
  return state.connections;
}

export function toggleDatabase(open = parts.panel.hidden) {
  parts.panel.hidden = !open;
  if (open) {
    readConnections().then(drawTree);
  }
}

function button(label, act, title = "") {
  const made = element("button", { className: "profile-action", type: "button", textContent: label, title });
  made.addEventListener("click", act);
  return made;
}

function drawBar() {
  if (!parts.bar) {
    return;
  }
  const name = chosen();
  const open = state.open.get(name);
  parts.pick.replaceChildren(
    element("option", { value: "", textContent: state.connections.length ? "Connection…" : "No connection: New makes one" }),
    ...state.connections.map((one) => element("option", { value: one.connection.name, textContent: `${one.connection.name} (${one.connection.kind})${one.open ? " ●" : ""}`, selected: one.connection.name === name })),
  );
  parts.connect.textContent = open ? "Disconnect" : "Connect";
  parts.connect.disabled = !name;
  parts.edit.disabled = !name;
  const transaction = open?.transaction;
  parts.commit.disabled = transaction !== "open" && transaction !== "failed";
  parts.rollback.disabled = parts.commit.disabled;
  parts.stop.disabled = !state.running;
  parts.stopReading.disabled = state.reading === 0;
  parts.export.disabled = !state.result?.columns;
  parts.history.disabled = !open;
}

// The status bar's item: the connection of the file shown, its zone and an open transaction.
export function drawStatus() {
  const item = document.getElementById("status-database");
  if (!item) {
    return;
  }
  const file = state.hooks?.file?.() ?? null;
  const sql = Boolean(file && /\.sql$/i.test(file));
  const name = sql ? connectionForFile(file) : [...state.open.keys()].find((one) => ["open", "failed"].includes(state.open.get(one)?.transaction)) ?? "";
  const open = state.open.get(name);
  item.hidden = !sql && !open;
  if (item.hidden) {
    return;
  }
  const transaction = open?.transaction === "open" ? " · transaction open" : open?.transaction === "failed" ? " · transaction failed" : "";
  item.textContent = name ? `${name}${open ? "" : " (not connected)"}${transaction}` : "No connection";
  item.dataset.transaction = open?.transaction ?? "";
  item.title = open ? `${name}: ${open.version}${open.zone ? `, times read in ${open.zone}` : ""}` : "The connection this file runs on";
}

function statusItems() {
  const file = state.hooks.file();
  const items = [];
  if (file && /\.sql$/i.test(file)) {
    items.push({ label: "Run This File On", items: state.connections.map((one) => ({ label: one.connection.name, checked: connectionForFile(file) === one.connection.name, run: () => chooseForFile(file, one.connection.name) })) });
  }
  const name = file && /\.sql$/i.test(file) ? connectionForFile(file) : chosen();
  const open = state.open.get(name);
  items.push("-", { label: "Commit", disabled: open?.transaction !== "open", run: () => finish(name, "COMMIT") }, { label: "Roll Back", disabled: !["open", "failed"].includes(open?.transaction), run: () => finish(name, "ROLLBACK") }, "-", { label: "Show the Database Window", run: () => toggleDatabase(true) });
  return items;
}

async function finish(name, statement) {
  await runOn(name, statement, { answered: true });
}

// The connection sheet: what a connection is, made new or changed, and its password, kept in the
// system's keychain and nowhere else.
function connectionSheet(given = null) {
  const field = (label, input) => element("label", { className: "report-row" }, element("span", { textContent: label }), input);
  const text = (value, props = {}) => element("input", { className: "report-field", type: "text", value: value ?? "", spellcheck: false, ...props });
  const kind = element("select", { className: "report-field" }, ...["postgres", "mysql", "sqlite"].map((one) => element("option", { value: one, textContent: { postgres: "PostgreSQL", mysql: "MySQL or MariaDB", sqlite: "SQLite" }[one], selected: (given?.kind ?? "postgres") === one })));
  const name = text(given?.name);
  const host = text(given?.host, { placeholder: "127.0.0.1" });
  const port = text(given?.port || "", { placeholder: "its kind's own" });
  const user = text(given?.user);
  const database = text(given?.database);
  const file = text(given?.file, { placeholder: "a path in the tree, or whole" });
  const ssh = text(given?.ssh, { placeholder: "user@host, as ssh names it" });
  const password = element("input", { className: "report-field", type: "password", autocomplete: "off", placeholder: given ? "kept in the keychain; type to change it" : "" });
  const matters = element("input", { type: "checkbox", checked: Boolean(given?.matters) });
  const guard = element("select", { className: "report-field" }, element("option", { value: "ask", textContent: "asks before each statement that writes", selected: given?.guard !== "read-only" }), element("option", { value: "read-only", textContent: "opens read-only", selected: given?.guard === "read-only" }));
  const said = element("p", { className: "report-said" });
  const served = [field("Host", host), field("Port", port), field("User", user), field("Database", database), field("Through ssh", ssh), field("Password", password)];
  const lite = [field("File", file)];
  const shown = () => {
    served.forEach((one) => (one.hidden = kind.value === "sqlite"));
    lite.forEach((one) => (one.hidden = kind.value !== "sqlite"));
  };
  kind.addEventListener("change", shown);
  const save = element("button", { className: "primary", type: "submit", textContent: given ? "Save" : "Add" });
  // A connection that is there already can be taken out, and its password taken from the keychain.
  const acts = [save];
  if (given) {
    const remove = element("button", { type: "button", textContent: "Remove" });
    remove.addEventListener("click", async () => {
      try {
        await invoke("db_remove", { name: given.name });
        if (chosen() === given.name) {
          localStorage.removeItem(CHOSEN);
        }
        dialog.close();
        say(`Took out the connection ${given.name}`);
        await readConnections();
        drawTree();
      } catch (error) {
        said.textContent = String(error);
      }
    });
    const forget = element("button", { type: "button", textContent: "Forget Password" });
    forget.addEventListener("click", async () => {
      try {
        await invoke("db_forget_password", { name: given.name });
        said.textContent = "The keychain no longer keeps its password.";
      } catch (error) {
        said.textContent = String(error);
      }
    });
    acts.unshift(remove, forget);
  }
  const form = element(
    "form",
    { className: "sheet-ask database-sheet" },
    element("h2", { textContent: given ? `The connection ${given.name}` : "A new connection" }),
    field("Name", name),
    field("Kind", kind),
    ...served,
    ...lite,
    element("label", { className: "report-row" }, matters, element("span", { textContent: "It matters: its writes are guarded" })),
    field("Guarded, it", guard),
    said,
    element("div", { className: "commit-buttons" }, ...acts),
  );
  shown();
  const dialog = state.hooks.sheet(form);
  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    const connection = {
      name: name.value.trim(),
      kind: kind.value,
      host: host.value.trim(),
      port: Number(port.value) || 0,
      user: user.value.trim(),
      database: database.value.trim(),
      file: file.value.trim(),
      ssh: ssh.value.trim(),
      matters: matters.checked,
      guard: matters.checked ? guard.value : "",
    };
    try {
      await invoke("db_save", { connection, was: given?.name ?? null, password: password.value || null });
      localStorage.setItem(CHOSEN, connection.name);
      dialog.close();
      await readConnections();
      drawTree();
    } catch (error) {
      said.textContent = String(error);
    }
  });
  name.focus();
}

// Asks for a connection's password, and whether the keychain keeps it.
function askPassword(name, why) {
  return new Promise((done) => {
    const password = element("input", { className: "report-field", type: "password", autocomplete: "off" });
    const keep = element("input", { type: "checkbox", checked: true });
    // Enter in the password connects, and so does the button, wherever the keys are in the sheet.
    const form = element(
      "form",
      { className: "sheet-ask" },
      element("p", { textContent: why }),
      element("label", { className: "report-row" }, element("span", { textContent: `${name}'s password` }), password),
      element("label", { className: "report-row" }, keep, element("span", { textContent: "Keep it in the system's keychain" })),
      element("div", { className: "commit-buttons" }, element("button", { className: "primary", type: "submit", textContent: "Connect" })),
    );
    let answer = null;
    form.addEventListener("submit", (event) => {
      event.preventDefault();
      answer = { password: password.value, keep: keep.checked };
      dialog.close();
    });
    const dialog = state.hooks.sheet(form);
    dialog.addEventListener("close", () => done(answer));
    password.focus();
  });
}

const signInFailed = (text) => /28P01|1045|password|authentication/i.test(text);

// Connects to `name`, asking for its password where the server refuses the one kept.
export async function connect(name) {
  try {
    const said = await invoke("db_connect", { name });
    say(`Connected to ${name}: ${said.version}`);
  } catch (error) {
    if (!signInFailed(String(error))) {
      say(String(error), { failed: true });
      return false;
    }
    const given = await askPassword(name, String(error).split("\n")[0]);
    if (!given) {
      return false;
    }
    try {
      const said = await invoke("db_connect", { name, password: given.password, keep: given.keep });
      say(`Connected to ${name}: ${said.version}`);
    } catch (again) {
      say(String(again), { failed: true });
      return false;
    }
  }
  state.levels.clear();
  state.opened.clear();
  await readConnections();
  drawTree();
  return true;
}

async function disconnect(name) {
  await invoke("db_close", { name }).catch(() => {});
  state.levels.clear();
  await readConnections();
  drawTree();
}

// The schema's level under `path`, read once and kept until it is read again.
async function level(path) {
  const key = keyOf(path);
  if (!state.levels.has(key)) {
    state.reading += 1;
    drawBar();
    try {
      state.levels.set(key, await invoke("db_children", { name: chosen(), path }));
    } catch (error) {
      state.levels.set(key, [{ label: String(error).split("\n")[0], kind: "failed", detail: "", leaf: true, path: [] }]);
    } finally {
      state.reading -= 1;
      drawBar();
    }
  }
  return state.levels.get(key);
}

// Reads again the level `path` stands in and draws the tree.
async function readAgain(path) {
  state.levels.delete(keyOf(path));
  drawTree();
}

function nodeRow(node, depth) {
  const open = state.opened.has(keyOf(node.path));
  const twisty = element("span", { className: "db-twisty", textContent: node.leaf ? "" : open ? "▾" : "▸" });
  const row = element("button", { className: `db-node db-${node.kind.replace(/\s+/g, "-")}`, type: "button", title: node.detail || node.label }, twisty, element("span", { className: "name", textContent: node.label }), element("span", { className: "detail", textContent: node.kind === "column" ? node.detail : node.leaf ? node.kind : "" }));
  row.style.paddingLeft = `${0.4 + depth * 0.9}rem`;
  row.dataset.path = JSON.stringify(node.path);
  row.dataset.kind = node.kind;
  row.dataset.label = node.label;
  if (!node.leaf) {
    row.addEventListener("click", () => {
      const key = keyOf(node.path);
      if (state.opened.has(key)) {
        state.opened.delete(key);
      } else {
        state.opened.add(key);
      }
      drawTree();
    });
  }
  return row;
}

let drawing = 0;
// Draws the schema of the connection chosen, each level opened read as it is needed.
export async function drawTree() {
  if (!parts.tree || parts.panel.hidden) {
    return;
  }
  const ticket = ++drawing;
  const name = chosen();
  if (!name || !state.open.has(name)) {
    parts.tree.replaceChildren(element("p", { className: "containers-problem", textContent: name ? `${name} is not connected: Connect reads its schema.` : "Choose a connection, or make one with New." }));
    return;
  }
  const rows = [];
  const walk = async (path, depth) => {
    for (const node of await level(path)) {
      rows.push(nodeRow(node, depth));
      if (!node.leaf && state.opened.has(keyOf(node.path))) {
        await walk(node.path, depth + 1);
      }
    }
  };
  await walk([], 0);
  if (ticket === drawing) {
    parts.tree.replaceChildren(...rows);
  }
}

const quoteName = (name, kind) => (kind === "mysql" ? `\`${name.replace(/`/g, "``")}\`` : `"${name.replace(/"/g, '""')}"`);

function treeItems(event) {
  const row = event.target.closest(".db-node");
  if (!row) {
    return null;
  }
  const path = JSON.parse(row.dataset.path || "[]");
  const connection = connectionOf(chosen());
  const items = [{ label: "Copy Name", run: () => navigator.clipboard?.writeText(row.dataset.label) }];
  if (path.length === 3 && ["table", "view", "materialized view", "foreign table"].includes(row.dataset.kind)) {
    const name = `${quoteName(path[0], connection?.kind)}.${quoteName(path[2], connection?.kind)}`;
    items.unshift({ label: "Show Its First Rows", run: () => runOn(chosen(), `SELECT * FROM ${name} LIMIT ${PAGE}`) });
  }
  items.push({ label: "Read Again", run: () => readAgain(path.length ? path : []) });
  return items;
}

// Asks before a guarded statement runs. Says whether to run it.
function askToRun(name, statement, ask) {
  return new Promise((done) => {
    const first = statement.trim().split(/\s+/)[0]?.toUpperCase() ?? "";
    const text = ask.why === "every row" ? `This ${first} reaches every row of what it names on ${name}: ${ask.count ?? "an unknown count of"} rows.` : `${name} is a connection that matters, and this statement writes.`;
    let answer = false;
    const run = element("button", { className: "primary", type: "button", textContent: "Run It" });
    const keep = element("button", { type: "button", textContent: "Do Not Run It" });
    const body = element("div", { className: "sheet-ask database-ask" }, element("p", { textContent: text }), element("pre", { className: "database-statement", textContent: statement.slice(0, 600) }), element("div", { className: "commit-buttons" }, keep, run));
    const dialog = state.hooks.sheet(body);
    run.addEventListener("click", () => ((answer = true), dialog.close()));
    keep.addEventListener("click", () => dialog.close());
    dialog.addEventListener("close", () => done(answer));
    keep.focus();
  });
}

// Runs `statement` on the connection `name`, connecting first where it is not connected, asking
// where it must; shows what it gave and gives the server's answer.
export async function runOn(name, statement, { answered = false } = {}) {
  if (!name) {
    toggleDatabase(true);
    say("Choose the connection the statement runs on, in the Database window", { failed: true });
    return null;
  }
  if (!state.open.has(name) && !(await connect(name))) {
    return null;
  }
  toggleDatabase(true);
  state.running = name;
  drawBar();
  parts.head.textContent = `Running on ${name}…`;
  let said;
  try {
    said = await invoke("db_run", { name, sql: statement, answered, page: PAGE });
    if (said.ask) {
      state.running = null;
      if (!(await askToRun(name, statement, said.ask))) {
        parts.head.textContent = "Not run.";
        drawBar();
        return null;
      }
      state.running = name;
      said = await invoke("db_run", { name, sql: statement, answered: true, page: PAGE });
    }
  } catch (error) {
    said = { failure: { message: String(error), code: "", detail: "", hint: "", position: 0 } };
  } finally {
    state.running = null;
  }
  const open = state.open.get(name);
  if (open && said.transaction) {
    open.transaction = said.transaction;
  }
  if (said.zone && open) {
    open.zone = said.zone;
  }
  show(name, said);
  if (said.shaped) {
    const path = state.levels.has(keyOf([said.shaped.name.split(".")[0].replace(/["`]/g, "")])) ? [said.shaped.name.split(".")[0].replace(/["`]/g, "")] : null;
    if (path) {
      readAgain(path);
    } else {
      [...state.levels.keys()].filter((key) => !key.includes("\u0000")).forEach((key) => state.levels.delete(key));
      drawTree();
    }
  }
  drawBar();
  drawStatus();
  return said;
}

// Shows what a statement gave: its rows in the grid, or what it did, or what the server said.
function show(name, said) {
  parts.messages.replaceChildren();
  if (said.failure) {
    const failure = said.failure;
    parts.head.textContent = `${name}: failed`;
    parts.messages.append(element("pre", { className: "database-failure", textContent: [failure.message + (failure.code ? ` (${failure.code})` : ""), failure.detail, failure.hint].filter(Boolean).join("\n") }));
    state.result = null;
    drawGrid();
    return;
  }
  for (const notice of said.notices ?? []) {
    parts.messages.append(element("p", { className: "database-notice", textContent: notice }));
  }
  if (!said.columns) {
    parts.head.textContent = `${name}: ${said.done || "done"} · ${said.ms} ms`;
    return;
  }
  state.result = { name, columns: said.columns, rows: new Map(said.rows.map((row, at) => [at, row])), read: said.rows.length, done: said.done, ms: said.ms };
  parts.grid.scrollTop = 0;
  drawGrid();
}

function headLine() {
  const result = state.result;
  if (!result) {
    return;
  }
  const open = state.open.get(result.name);
  parts.head.textContent = `${result.name}: ${result.read}${result.done ? "" : "+"} rows · ${result.ms} ms${open?.zone ? ` · times in ${open.zone}` : ""}${result.done ? ` · ${result.done}` : ""}`;
}

let fetching = false;
// The grid's motion: rows are read as far ahead the way it scrolls as its speed carries it over two
// reads, and those behind the motion are let go first.
const ahead = new Ahead(SPARE);
// Draws the rows in view, asking the server for those not held in the stretch the scroll heads
// into, and for more as the end comes near.
async function drawGrid() {
  const result = state.result;
  if (!result) {
    parts.gridHead.replaceChildren();
    parts.rows.replaceChildren();
    parts.spacer.style.height = "0px";
    return;
  }
  headLine();
  const widths = result.columns.map((column, at) => {
    let wide = column.name.length;
    for (let index = 0; index < Math.min(result.read, 50); index += 1) {
      wide = Math.max(wide, (result.rows.get(index)?.[at] ?? "NULL").length);
    }
    return Math.min(40, Math.max(4, wide)) + 2;
  });
  const template = widths.map((wide) => `${wide}ch`).join(" ");
  parts.gridHead.style.gridTemplateColumns = template;
  parts.gridHead.replaceChildren(...result.columns.map((column) => element("span", { className: column.numeric ? "num" : "", textContent: column.name, title: column.kind })));
  const total = result.read + (result.done ? 0 : 1);
  parts.spacer.style.height = `${total * ROW}px`;
  const shown = Math.floor(parts.grid.scrollTop / ROW);
  const first = Math.max(0, shown - 5);
  const count = Math.ceil(parts.grid.clientHeight / ROW) + 10;
  ahead.moved(shown);
  const [from, to] = ahead.span(first, first + count);
  const missing = [];
  for (let index = from; index < Math.min(to, result.read); index += 1) {
    if (!result.rows.has(index)) {
      missing.push(index);
    }
  }
  const nearEnd = !result.done && to >= result.read;
  if ((missing.length || nearEnd) && !fetching) {
    fetching = true;
    // The read covers the rows missing from the stretch, or the next page where none is.
    const start = missing.length ? missing[0] : result.read;
    const want = missing.length ? missing.at(-1) - start + 1 : PAGE;
    const began = performance.now();
    invoke("db_rows", { name: result.name, from: start, count: want })
      .then((got) => {
        ahead.read(performance.now() - began);
        if (state.result !== result) {
          return;
        }
        got.rows.forEach((row, at) => result.rows.set(start + at, row));
        result.read = got.read;
        result.done = got.done;
        // The rows far from the view are let go, behind the motion first; the server holds them.
        for (const index of ahead.letGo([...result.rows.keys()], first, first + count, PAGE * 3)) {
          result.rows.delete(index);
        }
      })
      .catch((error) => say(String(error), { failed: true }))
      .finally(() => {
        fetching = false;
        requestAnimationFrame(drawGrid);
      });
  }
  const rows = [];
  for (let index = first; index < Math.min(first + count, total); index += 1) {
    const row = result.rows.get(index);
    const line = element("div", { className: "db-row" });
    line.style.top = `${index * ROW}px`;
    line.style.gridTemplateColumns = template;
    if (!row) {
      line.append(element("span", { className: "db-wait", textContent: index >= result.read ? "reading more…" : "…" }));
    } else {
      row.forEach((value, at) => line.append(element("span", { className: `${result.columns[at]?.numeric ? "num" : ""}${value === null ? " null" : ""}`, textContent: value ?? "NULL", title: value ?? "NULL" })));
    }
    rows.push(line);
  }
  parts.rows.replaceChildren(...rows);
}

// The rows of the last result the grid holds, for the tests.
export const heldRows = () => [...(state.result?.rows.keys() ?? [])];

async function exportRows() {
  const result = state.result;
  if (!result) {
    return;
  }
  const path = (await state.hooks.ask("The file to write the rows to, in the tree: .csv, .tsv or .json for a JSON line each", `${result.name}-rows.csv`))?.trim();
  if (!path) {
    return;
  }
  const format = /\.tsv$/i.test(path) ? "tsv" : /\.jsonl?$/i.test(path) ? "json" : "csv";
  parts.head.textContent = `Writing ${path}…`;
  try {
    const written = await invoke("db_export", { name: result.name, path, format });
    say(`${written.written} rows written to ${path}, the rest read from the same run`);
    result.done = result.done ?? "read to its end";
  } catch (error) {
    say(String(error), { failed: true });
  }
  headLine();
}

async function showHistory() {
  const name = chosen();
  const history = await invoke("db_history", { name }).catch(() => []);
  const items = history.slice(0, 40).map((one) => ({
    label: `${one.sql.replace(/\s+/g, " ").slice(0, 80)}${one.failed ? "  ✗" : ""}`,
    run: () => runOn(name, one.sql),
  }));
  showMenu(window.innerWidth / 3, window.innerHeight / 4, items.length ? items : [{ label: "Nothing has run on this connection", disabled: true }]);
}

// The statement of the .sql file shown that the cursor stands in, or the text selected, and its
// place in the file by character.
function statementHere() {
  const editor = state.hooks.editor();
  const s = editor?.s;
  if (!s) {
    return null;
  }
  const lines = [];
  for (let line = 0; line < s.doc.count; line += 1) {
    lines.push(s.doc.line(line));
  }
  const text = lines.join("\n");
  const offsetOf = (p) => lines.slice(0, p.line).reduce((sum, one) => sum + one.length + 1, 0) + p.col;
  const selection = editor.primary();
  const from = offsetOf(selection.anchor);
  const to = offsetOf(selection.head);
  return { text, lines, from: Math.min(from, to), to: Math.max(from, to), editor };
}

function placeAt(here, offset) {
  let left = offset;
  for (let line = 0; line < here.lines.length; line += 1) {
    if (left <= here.lines[line].length) {
      here.editor.goTo(line, left);
      return;
    }
    left -= here.lines[line].length + 1;
  }
}

const kindOf = (name) => connectionOf(name)?.kind ?? "postgres";

// The page counts text in UTF-16 units and the Rust side and the servers count it in characters: the
// characters before a place the page gives, and the units before a count of characters.
const charsBefore = (text, units) => [...text.slice(0, units)].length;
const unitsBefore = (text, chars) => [...text].slice(0, chars).join("").length;

// Run, Run Statement: the statement the cursor stands in, or the text selected, on the file's
// connection.
export async function runStatement() {
  const file = state.hooks.file();
  const here = statementHere();
  if (!file || !here) {
    say("Run Statement runs a statement of the file shown", { failed: true });
    return;
  }
  const name = connectionForFile(file);
  if (!state.connections.length) {
    await readConnections();
  }
  // Where the statement starts, in characters.
  let start = charsBefore(here.text, here.from);
  let statement = here.text.slice(here.from, here.to);
  if (!statement.trim()) {
    const span = await invoke("db_statement_at", { text: here.text, offset: start, kind: kindOf(name) });
    if (!span) {
      say("The cursor stands in no statement: Run File runs them all");
      return;
    }
    [start] = span;
    statement = [...here.text].slice(span[0], span[1]).join("");
  }
  const said = await runOn(name, statement);
  if (said?.failure?.position) {
    placeAt(here, unitsBefore(here.text, start + said.failure.position - 1));
  }
}

// Run, Run SQL File: each statement of the file in turn, stopping at the first that fails.
export async function runSqlFile() {
  const file = state.hooks.file();
  const here = statementHere();
  if (!file || !here) {
    return;
  }
  const name = connectionForFile(file);
  if (!state.connections.length) {
    await readConnections();
  }
  const spans = await invoke("db_statements", { text: here.text, kind: kindOf(name) });
  const characters = [...here.text];
  let done = 0;
  for (const [from, to] of spans) {
    const said = await runOn(name, characters.slice(from, to).join(""));
    if (!said || said.failure) {
      if (said?.failure?.position) {
        placeAt(here, unitsBefore(here.text, from + said.failure.position - 1));
      }
      say(`${done} of ${spans.length} statements ran; the next ${said ? "failed" : "did not run"}`, { failed: true });
      return;
    }
    done += 1;
  }
  say(`Each of the ${spans.length} statements of ${file} ran on ${name}`);
}

// `hooks` gives the editor, the file shown, a sheet over the window, and a line asked for.
export function startDatabase(hooks) {
  state.hooks = hooks;
  parts.panel = document.getElementById("database");
  parts.pick = element("select", { className: "nb-pick", title: "The connection the window runs on and shows" });
  parts.pick.addEventListener("change", () => {
    localStorage.setItem(CHOSEN, parts.pick.value);
    state.levels.clear();
    drawBar();
    drawTree();
    drawStatus();
  });
  parts.connect = button("Connect", () => (state.open.has(chosen()) ? disconnect(chosen()) : connect(chosen())));
  parts.edit = button("Edit…", () => connectionSheet(connectionOf(chosen())));
  const fresh = button("New…", () => connectionSheet(null));
  parts.stop = button("Stop", () => invoke("db_stop", { name: state.running ?? chosen() }).catch((error) => say(String(error), { failed: true })), "Stops the statement running");
  parts.stopReading = button("Stop Reading", () => invoke("db_stop_reading", { name: chosen() }).catch(() => {}), "Stops the schema's reading");
  parts.commit = button("Commit", () => finish(chosen(), "COMMIT"));
  parts.rollback = button("Roll Back", () => finish(chosen(), "ROLLBACK"));
  parts.export = button("Export…", exportRows, "Writes the rows to a file, reading the rest from the same run");
  parts.history = button("History", showHistory, "The statements run on this connection");
  const again = button("Read Again", () => {
    state.levels.clear();
    readConnections().then(drawTree);
  });
  parts.bar = element("div", { className: "profile-bar" }, parts.pick, parts.connect, fresh, parts.edit, again, parts.stopReading, parts.stop, parts.commit, parts.rollback, parts.export, parts.history);
  parts.tree = element("div", { className: "containers-list db-tree" });
  parts.head = element("div", { className: "containers-title db-head" });
  parts.gridHead = element("div", { className: "db-grid-head" });
  parts.spacer = element("div", { className: "db-spacer" });
  parts.rows = element("div", { className: "db-rows" });
  parts.grid = element("div", { className: "db-grid" }, parts.spacer, parts.rows);
  parts.grid.addEventListener("scroll", () => {
    parts.gridHead.scrollLeft = parts.grid.scrollLeft;
    requestAnimationFrame(drawGrid);
  });
  parts.messages = element("div", { className: "db-messages" });
  parts.panel.append(parts.bar, element("div", { className: "containers-body db-body" }, parts.tree, element("div", { className: "containers-side db-side" }, parts.head, parts.gridHead, parts.grid, parts.messages)));
  menuOn(parts.tree, treeItems);
  const item = document.getElementById("status-database");
  item?.addEventListener("click", (event) => showMenu(event.clientX, event.clientY - 8, statusItems(), { side: "up" }));
  drawBar();
}

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

import { invoke } from "./bridge.js";
import { drawGrid, exportRows, show, showHistory } from "./database/grid.js";
import { askPassword, connectionSheet } from "./database/sheet.js";
import { CHOSEN, FILES, PAGE, chosen, connectionForFile, connectionOf, element, filesKept, keyOf, parts, state } from "./database/state.js";
import { copyText, menuOn, showMenu } from "./menu.js";
import { say } from "./statusbar.js";

export { cachedRows } from "./database/grid.js";
export { connectionForFile } from "./database/state.js";
export { runSqlFile, runStatement } from "./database/statements.js";

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
      const said = await invoke("db_connect", { name, password: given.password, save: given.keep });
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
  const items = [{ label: "Copy Name", command: "db-copy-name", args: [row.dataset.label], run: () => copyText(row.dataset.label) }];
  if (path.length === 3 && ["table", "view", "materialized view", "foreign table"].includes(row.dataset.kind)) {
    items.unshift({ label: "Show Its First Rows", command: "db-first-rows", args: [path[0], path[2]], run: () => databaseCommands["db-first-rows"]([path[0], path[2]]) });
  }
  items.push({ label: "Read Again", command: "db-read-again", args: path, run: () => readAgain(path.length ? path : []) });
  return items;
}

// The tree's menu as commands, by the names commands.json gives them, each run from the menu or from
// the command line: a table's first rows by its schema and name, on the connection chosen or the one
// named after them, a name copied, and a level of the tree read again by its path.
export const databaseCommands = {
  "db-first-rows": ([schema, table, name = chosen()]) => {
    const kind = connectionOf(name)?.kind;
    return runOn(name, `SELECT * FROM ${quoteName(schema, kind)}.${quoteName(table, kind)} LIMIT ${PAGE}`);
  },
  "db-copy-name": ([name]) => copyText(name),
  "db-read-again": (path) => readAgain(path),
};

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
    if (said.confirm) {
      state.running = null;
      if (!(await askToRun(name, statement, said.confirm))) {
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

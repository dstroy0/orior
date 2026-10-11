// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The Database window, driven as a person drives it, against the harness's own PostgreSQL, MySQL and
// a SQLite file of the run's tree. Each test goes on from the state the one before it left: the
// rest run on the connection the first makes.

import { heldRows, readConnections, runOn } from "../../src/database.js";
import { editing, openFile } from "../../src/edit.js";
import { runCommand } from "../../src/menubar.js";
import { invoke } from "../../src/bridge.js";
import { callsTo } from "../helpers.js";

const { test, assert, ui, Failure } = window.__harness;
const servers = window.__harness.servers ?? {};

const panel = () => document.getElementById("database");
const bar = () => panel().querySelector(".profile-bar");
const barButton = (label) => [...bar().querySelectorAll("button")].find((one) => one.textContent === label);
const sheetOpen = () => document.querySelector("dialog.sheet[open]");
const head = () => panel().querySelector(".db-head").textContent;
const status = () => document.getElementById("status-database");

// The test server `name` names, as host and port and its password.
function server(name) {
  const address = servers[`ORIOR_${name}_ADDRESS`];
  if (!address) {
    throw new Failure(`no ${name} test server: the harness starts one where Docker runs`);
  }
  const [host, port] = address.split(":");
  return { host, port, password: servers[`ORIOR_${name}_PASSWORD`] };
}

// The field of the open sheet named `label`.
function field(label) {
  const row = [...sheetOpen().querySelectorAll("label.report-row")].find((one) => one.querySelector("span")?.textContent === label);
  if (!row) {
    throw new Failure(`the sheet has no field ${label}`);
  }
  return row.querySelector("input, select");
}

async function typeInto(label, text) {
  const input = field(label);
  await ui.click(input);
  await ui.key("Ctrl+A");
  await ui.type(text);
}

// Chooses an option of a select of the sheet by typing its first letters, as the keyboard does; the
// select reads letters typed within a second of each other as one word, and a choice waits that long.
async function choose(label, letters) {
  const select = field(label);
  select.focus();
  await ui.rest(1100);
  await ui.type(letters);
  return select;
}

async function openWindow() {
  if (panel().hidden) {
    await ui.click(".strip-button.strip-database");
    await ui.waitFor(() => !panel().hidden);
  }
}

// Writes `text` to `path` of the tree and shows it in the editor with the cursor at `line`, `col`.
async function showSql(path, text, line, col = 0) {
  await invoke("file_write", { path, text });
  await openFile(path);
  await ui.waitFor(() => editing().fileHere() === path);
  const { editor } = editing();
  editor.goTo(line, col);
  editor.focus();
}

test("the Database window opens and closes from its strip icon, and from View, Database", async () => {
  assert.equal(panel().hidden, true, "it starts closed");
  await ui.click(".strip-button.strip-database");
  await ui.waitFor(() => !panel().hidden);
  await ui.click(".strip-button.strip-database");
  await ui.waitFor(() => panel().hidden);
  runCommand("database-view");
  assert.equal(panel().hidden, false);
  await ui.waitFor(() => /Choose a connection, or make one with New/.test(panel().querySelector(".db-tree")?.textContent ?? ""));
});

test("New makes a connection, its sheet showing the fields of its kind", async () => {
  const pg = server("PG");
  await openWindow();
  await ui.click(barButton("New…"));
  await ui.waitFor(() => sheetOpen());
  assert.equal(field("Host").closest("label").hidden, false, "a server's fields show for PostgreSQL");
  assert.equal(field("File").closest("label").hidden, true);
  await choose("Kind", "S");
  assert.equal(field("Kind").value, "sqlite");
  assert.equal(field("Host").closest("label").hidden, true, "a SQLite file has no host");
  assert.equal(field("File").closest("label").hidden, false);
  await choose("Kind", "P");
  assert.equal(field("Kind").value, "postgres");
  await typeInto("Name", "harness-pg");
  await typeInto("Host", pg.host);
  await typeInto("Port", pg.port);
  await typeInto("User", "postgres");
  await typeInto("Database", "postgres");
  await ui.click([...sheetOpen().querySelectorAll("button")].find((one) => one.textContent === "Add"));
  await ui.waitFor(() => !sheetOpen());
  const listed = await invoke("db_list");
  assert.ok(listed.some((one) => one.connection.name === "harness-pg" && one.connection.port === Number(pg.port)), "databases.json keeps it");
  assert.equal(bar().querySelector("select").value, "harness-pg", "it is the connection chosen");
});

test("Connect asks for the password the keychain lacks, and keeps it only where asked", async () => {
  const pg = server("PG");
  await ui.click(barButton("Connect"));
  await ui.waitFor(() => sheetOpen()?.querySelector("input[type=password]"));
  await ui.type(pg.password);
  const keep = sheetOpen().querySelector("input[type=checkbox]");
  assert.equal(keep.checked, true, "keeping is offered");
  await ui.click(keep);
  assert.equal(keep.checked, false);
  await ui.click([...sheetOpen().querySelectorAll("button")].find((one) => one.textContent === "Connect"));
  await ui.waitFor(() => barButton("Disconnect"), 15000);
  const last = callsTo("db_connect").at(-1).args;
  assert.equal(last.keep, false, "the keychain was not asked to keep it");
  await ui.waitFor(() => panel().querySelector('.db-node[data-label="public"]'), 10000);
});

test("the schema opens a level at a time, as each level is pressed", async () => {
  // The table the tests read, made as a reader would who has said yes to each write.
  const said = await runOn("harness-pg", "CREATE TABLE IF NOT EXISTS harness_t (n int, name text)", { answered: true });
  assert.ok(!said?.failure, JSON.stringify(said?.failure));
  await runOn("harness-pg", "TRUNCATE harness_t", { answered: true });
  await runOn("harness-pg", "INSERT INTO harness_t VALUES (1, 'one'), (2, 'two'), (3, 'three')", { answered: true });
  await ui.click('.db-node[data-label="public"]');
  const table = await ui.waitFor(() => panel().querySelector('.db-node[data-label="harness_t"]'), 10000);
  assert.equal(table.dataset.kind, "table");
  await ui.click(table);
  await ui.waitFor(() => panel().querySelector('.db-node[data-kind="column"][data-label="name"]'), 10000);
});

test("Ctrl+Enter runs the statement the cursor stands in, and only it", async () => {
  await showSql("harness-run.sql", "SELECT 41 + 1 AS answer;\n\nSELECT 'not this one';\n", 0, 3);
  const before = callsTo("db_run").length;
  await ui.key("Ctrl+Enter");
  await ui.waitFor(() => panel().querySelector(".db-grid-head")?.textContent === "answer", 10000);
  assert.equal(panel().querySelector(".db-row").textContent, "42");
  assert.equal(callsTo("db_run").length, before + 1);
  assert.match(callsTo("db_run").at(-1).args.sql.trim(), /^SELECT 41 \+ 1 AS answer;?$/);
});

test("with the cursor in no statement, Ctrl+Enter runs nothing and says so", async () => {
  await showSql("harness-blank.sql", "SELECT 1;\n\n\nSELECT 2;\n", 1, 0);
  const before = callsTo("db_run").length;
  await ui.key("Ctrl+Enter");
  await ui.waitFor(() => /stands in no statement/.test(document.getElementById("status-said").textContent), 5000);
  assert.equal(callsTo("db_run").length, before, "nothing ran");
});

test("Ctrl+Enter in a file that is not SQL is the editor's own: a line under the cursor", async () => {
  await openFile("hello.py");
  await ui.waitFor(() => editing().fileHere() === "hello.py");
  const { editor } = editing();
  const lines = editor.s.doc.count;
  editor.goTo(0, 3);
  editor.focus();
  const before = callsTo("db_run").length;
  await ui.key("Ctrl+Enter");
  await ui.waitFor(() => editor.s.doc.count === lines + 1, 3000);
  assert.equal(callsTo("db_run").length, before, "no statement ran");
  await ui.key("Ctrl+Z");
});

test("the status bar names the connection the SQL file shown runs on, and none for other files", async () => {
  await showSql("harness-status.sql", "SELECT 1;\n", 0);
  await ui.waitFor(() => !status().hidden);
  assert.match(status().textContent, /harness-pg/);
  await openFile("hello.py");
  await ui.waitFor(() => status().hidden, 3000);
});

test("rows come a page at a time as the grid scrolls, the grid holding only those in view", async () => {
  await runOn("harness-pg", "SELECT n, 'row ' || n AS label FROM generate_series(1, 5000) n");
  const grid = panel().querySelector(".db-grid");
  assert.ok(panel().querySelectorAll(".db-row").length < 200, "only the rows in view are drawn");
  assert.match(head(), /200\+ rows/);
  const fetched = callsTo("db_rows").length;
  for (let turn = 0; turn < 40 && !/5000 rows/.test(head()); turn += 1) {
    await ui.wheel(grid, 4000);
    await ui.rest(60);
  }
  await ui.waitFor(() => /5000 rows/.test(head()), 20000);
  assert.ok(callsTo("db_rows").length > fetched, "more rows were asked for as the grid scrolled");
  assert.ok(panel().querySelectorAll(".db-row").length < 200, "still only the rows in view");
});

test("rows are read ahead the way the grid scrolls, as far as its speed carries it, and those behind it go first", async () => {
  const grid = panel().querySelector(".db-grid");
  const asked = callsTo("db_rows").length;
  const first = () => Math.floor(grid.scrollTop / 22);
  // Up from the end, past the most the grid holds, 600 rows: what it read on the way down goes.
  const start = first();
  for (let turn = 0; turn < 60 && start - first() < 1600; turn += 1) {
    await ui.wheel(grid, -1500);
    await ui.rest(40);
  }
  await ui.rest(400);
  const reads = callsTo("db_rows").slice(asked);
  assert.ok(reads.some((one) => one.args.from < first() - 40), `a read reached past the least above the view at ${first()}: ${JSON.stringify(reads.map((one) => one.args))}`);
  const held = heldRows();
  assert.ok(!held.some((row) => row >= 4900), `the rows farthest behind the motion, at the end, went first: ${Math.max(...held)}`);
  for (let row = first(); row < first() + 10; row += 1) {
    assert.ok(held.includes(row), `row ${row} in view is held`);
  }
  assert.ok(held.length <= 600 + 200, `the grid holds no more than its most and a read: ${held.length}`);
});

test("Stop stops a statement as it runs", async () => {
  const running = runOn("harness-pg", "SELECT pg_sleep(20)");
  const stop = barButton("Stop");
  await ui.waitFor(() => !stop.disabled, 5000);
  const started = performance.now();
  await ui.click(stop);
  const said = await running;
  assert.ok(performance.now() - started < 10000, "it stopped well before its 20 s");
  assert.match(said.failure?.code ?? "", /57014/);
  assert.match(panel().querySelector(".database-failure").textContent, /cancel/i);
});

test("a write that reaches every row asks first with the count of rows, and runs only on a yes", async () => {
  const declined = runOn("harness-pg", "UPDATE harness_t SET name = name");
  await ui.waitFor(() => sheetOpen()?.querySelector(".database-ask"), 10000);
  assert.match(sheetOpen().textContent, /every row .* 3 rows/);
  await ui.click([...sheetOpen().querySelectorAll("button")].find((one) => one.textContent === "Do Not Run It"));
  assert.equal(await declined, null);
  assert.equal(head(), "Not run.");
  const ran = runOn("harness-pg", "UPDATE harness_t SET name = name");
  await ui.waitFor(() => sheetOpen()?.querySelector(".database-ask"), 10000);
  await ui.click([...sheetOpen().querySelectorAll("button")].find((one) => one.textContent === "Run It"));
  const said = await ran;
  assert.match(said.done ?? "", /UPDATE 3/);
});

test("an open transaction shows on the status bar until Roll Back ends it", async () => {
  await showSql("harness-transaction.sql", "SELECT 1;\n", 0);
  await runOn("harness-pg", "BEGIN");
  await runOn("harness-pg", "DELETE FROM harness_t WHERE n = 1");
  await ui.waitFor(() => /transaction open/.test(status().textContent), 5000);
  assert.equal(status().dataset.transaction, "open");
  const rollback = barButton("Roll Back");
  assert.equal(rollback.disabled, false);
  await ui.click(rollback);
  await ui.waitFor(() => !/transaction/.test(status().textContent), 5000);
  const counted = await runOn("harness-pg", "SELECT count(*) FROM harness_t");
  assert.equal(counted.rows[0][0], "3", "the delete was rolled back");
});

test("a statement the server finds wrong puts the cursor where the server says", async () => {
  await showSql("harness-wrong.sql", "SELECT nope FROM harness_t;\n", 0, 2);
  await ui.key("Ctrl+Enter");
  await ui.waitFor(() => panel().querySelector(".database-failure"), 10000);
  assert.match(panel().querySelector(".database-failure").textContent, /nope/);
  const { head: cursor } = editing().editor.primary();
  assert.deepEqual([cursor.line, cursor.col], [0, 7], "the cursor stands at nope");
});

test("a character past the basic plane before a statement moves neither the statement found nor the cursor", async () => {
  await showSql("harness-faces.sql", "-- \u{1F642}\u{1F642} two faces\nSELECT nope FROM harness_t;\n", 1, 2);
  const before = callsTo("db_run").length;
  await ui.key("Ctrl+Enter");
  await ui.waitFor(() => callsTo("db_run").length > before, 10000);
  await ui.waitFor(() => panel().querySelector(".database-failure")?.textContent.includes("nope"), 10000);
  assert.match(callsTo("db_run").at(-1).args.sql.trim(), /^SELECT nope FROM harness_t;?$/);
  const { head: cursor } = editing().editor.primary();
  assert.deepEqual([cursor.line, cursor.col], [1, 7], "the cursor stands at nope");
});

test("Export writes the rows to a file of the tree", async () => {
  await runOn("harness-pg", "SELECT n, name FROM harness_t ORDER BY n");
  await ui.click(barButton("Export…"));
  await ui.waitFor(() => sheetOpen()?.querySelector("input"));
  await ui.key("Ctrl+A");
  await ui.type("harness-rows.csv");
  await ui.key("Enter");
  await ui.waitFor(() => /rows written to harness-rows\.csv/.test(document.getElementById("status-said").title), 10000);
  const written = await invoke("file_read", { path: "harness-rows.csv" });
  assert.match(written.text, /n,name\r?\n1,one\r?\n2,two\r?\n3,three/);
});

test("History lists what ran on the connection, and a press runs it again", async () => {
  await ui.click(barButton("History"));
  const item = await ui.waitFor(() => [...document.querySelectorAll(".menu .menu-item")].find((one) => one.textContent.includes("SELECT 41 + 1 AS answer")), 5000);
  await ui.click(item);
  await ui.waitFor(() => panel().querySelector(".db-grid-head")?.textContent === "answer", 10000);
});

test("Run SQL File runs each statement in turn and stops at the first that fails", async () => {
  await showSql("harness-file.sql", "SELECT 1;\nSELECT nope;\nSELECT 3;\n", 0);
  const before = callsTo("db_run").length;
  runCommand("run-sql-file");
  await ui.waitFor(() => /1 of 3 statements ran/.test(document.getElementById("status-said").textContent), 15000);
  assert.equal(callsTo("db_run").length, before + 2, "the third did not run");
});

test("a SQLite file of the tree is a connection with no password", async () => {
  await ui.click(barButton("New…"));
  await ui.waitFor(() => sheetOpen());
  await choose("Kind", "S");
  await typeInto("Name", "harness-lite");
  await typeInto("File", "harness.db");
  await ui.click([...sheetOpen().querySelectorAll("button")].find((one) => one.textContent === "Add"));
  await ui.waitFor(() => !sheetOpen());
  await ui.click(barButton("Connect"));
  await ui.waitFor(() => barButton("Disconnect"), 15000);
  await runOn("harness-lite", "CREATE TABLE IF NOT EXISTS t (n INTEGER)", { answered: true });
  await runOn("harness-lite", "INSERT INTO t VALUES (7)", { answered: true });
  const said = await runOn("harness-lite", "SELECT n FROM t");
  assert.equal(said.rows.at(-1)[0], "7");
});

test("a MySQL connection signs in and reads rows", async () => {
  const my = server("MYSQL");
  await invoke("db_save", { connection: { name: "harness-my", kind: "mysql", host: my.host, port: Number(my.port), user: "root", database: "", file: "", ssh: "", matters: false, guard: "" }, was: null, password: null });
  await invoke("db_connect", { name: "harness-my", password: my.password, keep: false });
  await readConnections();
  const said = await runOn("harness-my", "SELECT 6 * 7 AS answer");
  assert.equal(said.rows[0][0], "42");
});

test("a connection through ssh reaches its server by a tunnel of the system's ssh", async () => {
  const pg = server("PG");
  const through = servers.ORIOR_TEST_SSH_HOST;
  if (!through) {
    throw new Failure("no ssh test server: the harness starts one where Docker runs");
  }
  const connection = { name: "harness-ssh", kind: "postgres", host: servers.ORIOR_TEST_SSH_TO, port: Number(pg.port), user: "postgres", database: "postgres", file: "", ssh: through, matters: false, guard: "" };
  await invoke("db_save", { connection, was: null, password: null });
  await readConnections();
  bar().querySelector("select").value = "harness-ssh";
  bar().querySelector("select").dispatchEvent(new Event("change"));
  await ui.click(barButton("Connect"));
  await ui.waitFor(() => sheetOpen()?.querySelector("input[type=password]"), 20000);
  await ui.type(pg.password);
  await ui.click(sheetOpen().querySelector("input[type=checkbox]"));
  await ui.click([...sheetOpen().querySelectorAll("button")].find((one) => one.textContent === "Connect"));
  await ui.waitFor(() => barButton("Disconnect"), 20000);
  const said = await runOn("harness-ssh", "SELECT 'by the tunnel' AS way");
  assert.equal(said.rows[0][0], "by the tunnel");
});

test("Edit takes a connection out, and Disconnect closes one", async () => {
  bar().querySelector("select").value = "harness-pg";
  bar().querySelector("select").dispatchEvent(new Event("change"));
  await ui.click(barButton("Disconnect"));
  await ui.waitFor(() => barButton("Connect"), 5000);
  for (const name of ["harness-pg", "harness-lite", "harness-my", "harness-ssh"]) {
    bar().querySelector("select").value = name;
    bar().querySelector("select").dispatchEvent(new Event("change"));
    await ui.click(barButton("Edit…"));
    await ui.waitFor(() => sheetOpen());
    await ui.click([...sheetOpen().querySelectorAll("button")].find((one) => one.textContent === "Remove"));
    await ui.waitFor(() => !sheetOpen());
  }
  const listed = await invoke("db_list");
  assert.ok(!listed.some((one) => one.connection.name.startsWith("harness-")), "every connection made here is gone");
});

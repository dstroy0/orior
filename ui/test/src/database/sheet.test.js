// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The connection sheet: a connection made new with the fields its type asks, changed, taken out or its password
// forgotten, and a connection's password asked for with whether the keychain keeps it.

import { askPassword, connectionSheet } from "../../../gui/src-js/database/sheet.js";
import { putBackAsFound } from "../../menus.js";

const { test, assert, ui, calls } = window.__harness;

const sheet = () => document.querySelector("dialog.sheet[open] form");
const field = (label) => [...sheet().querySelectorAll(".report-row")].find((row) => row.querySelector("span")?.textContent === label);

test("a new connection's sheet asks a SQLite file alone, and saves what it was given", async () => {
  const saved = [];
  calls.answer("db_save", (args) => saved.push(args));
  calls.answer("db_list", () => []);
  connectionSheet();
  await ui.waitFor(() => sheet()?.classList.contains("database-sheet"), 3000);
  assert.equal(sheet().querySelector("h2").textContent, "A new connection");
  assert.ok(!field("Host").hidden && field("File").hidden, "a server's fields for PostgreSQL");
  field("Name").querySelector("input").value = "local";
  const type = field("Kind").querySelector("select");
  type.value = "sqlite";
  type.dispatchEvent(new Event("change"));
  assert.ok(field("Host").hidden && field("Password").hidden && !field("File").hidden, "a file's field alone for SQLite");
  field("File").querySelector("input").value = " data/app.db ";
  sheet().querySelector("button.primary").click();
  await ui.waitFor(() => saved.length === 1 && !sheet(), 3000);
  const { connection, was, password } = saved[0];
  assert.deepEqual([connection.name, connection.kind, connection.file, connection.guard, was, password], ["local", "sqlite", "data/app.db", "", null, null]);
  assert.equal(localStorage.getItem("orior.database.chosen"), "local", "the connection made is the one chosen");
});

test("a connection there already is changed under its name, its password forgotten, and taken out", async () => {
  const given = { name: "prod", kind: "postgres", host: "db.example", port: 5433, user: "app", database: "main", matters: true, guard: "read-only" };
  const made = [];
  for (const name of ["db_save", "db_forget_password", "db_remove"]) {
    calls.answer(name, (args) => made.push([name, args]));
  }
  calls.answer("db_list", () => []);
  connectionSheet(given);
  await ui.waitFor(() => sheet()?.classList.contains("database-sheet"), 3000);
  assert.equal(sheet().querySelector("h2").textContent, "The connection prod");
  assert.equal(field("Port").querySelector("input").value, "5433");
  assert.equal(field("Guarded, it").querySelector("select").value, "read-only");
  [...sheet().querySelectorAll("button")].find((one) => one.textContent === "Forget Password").click();
  await ui.waitFor(() => sheet().querySelector(".report-said").textContent === "The keychain no longer keeps its password.", 3000);
  [...sheet().querySelectorAll("button")].find((one) => one.textContent === "Remove").click();
  await ui.waitFor(() => !sheet(), 3000);
  assert.deepEqual(made.map(([name, args]) => [name, args.name]), [["db_forget_password", "prod"], ["db_remove", "prod"]]);
});

test("a password is asked for with whether the keychain keeps it, and none where the sheet is closed", async () => {
  const asked = askPassword("prod", "prod asks for its password.");
  await ui.waitFor(() => sheet(), 3000);
  assert.equal(sheet().querySelector("p").textContent, "prod asks for its password.");
  sheet().querySelector('input[type="password"]').value = "secret";
  sheet().querySelector('input[type="checkbox"]').checked = false;
  sheet().querySelector("button.primary").click();
  assert.deepEqual(await asked, { password: "secret", keep: false });
  const closed = askPassword("prod", "again");
  await ui.waitFor(() => sheet(), 3000);
  sheet().closest("dialog").close();
  assert.equal(await closed, null);
});

test("the window is put back as the file found it", putBackAsFound);

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The connection sheet: what a connection is, made new or changed, and its password, retained in the
// system's keychain and nowhere else.

import { invoke } from "../bridge.js";
import { drawTree, readConnections } from "../database.js";
import { say } from "../statusbar.js";
import { CHOSEN, chosen, element, state } from "./state.js";

export function connectionSheet(given = null) {
  const field = (label, input) => element("label", { className: "report-row" }, element("span", { textContent: label }), input);
  const text = (value, props = {}) => element("input", { className: "report-field", type: "text", value: value ?? "", spellcheck: false, ...props });
  const type = element("select", { className: "report-field" }, ...["postgres", "mysql", "sqlite"].map((one) => element("option", { value: one, textContent: { postgres: "PostgreSQL", mysql: "MySQL or MariaDB", sqlite: "SQLite" }[one], selected: (given?.type ?? "postgres") === one })));
  const name = text(given?.name);
  const host = text(given?.host, { placeholder: "127.0.0.1" });
  const port = text(given?.port || "", { placeholder: "its type's own" });
  const user = text(given?.user);
  const database = text(given?.database);
  const file = text(given?.file, { placeholder: "a path in the tree, or whole" });
  const ssh = text(given?.ssh, { placeholder: "user@host, as ssh names it" });
  const password = element("input", { className: "report-field", type: "password", autocomplete: "off", placeholder: given ? "retained in the keychain; type to change it" : "" });
  const matters = element("input", { type: "checkbox", checked: Boolean(given?.matters) });
  const guard = element("select", { className: "report-field" }, element("option", { value: "confirm", textContent: "asks before each statement that writes", selected: given?.guard !== "read-only" }), element("option", { value: "read-only", textContent: "opens read-only", selected: given?.guard === "read-only" }));
  const said = element("p", { className: "report-said" });
  const served = [field("Host", host), field("Port", port), field("User", user), field("Database", database), field("Through ssh", ssh), field("Password", password)];
  const lite = [field("File", file)];
  const displayed = () => {
    served.forEach((one) => (one.hidden = type.value === "sqlite"));
    lite.forEach((one) => (one.hidden = type.value !== "sqlite"));
  };
  type.addEventListener("change", displayed);
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
        said.textContent = "The keychain no longer retains its password.";
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
    field("Type", type),
    ...served,
    ...lite,
    element("label", { className: "report-row" }, matters, element("span", { textContent: "It matters: its writes are guarded" })),
    field("Guarded, it", guard),
    said,
    element("div", { className: "commit-buttons" }, ...acts),
  );
  displayed();
  const dialog = state.hooks.sheet(form);
  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    const connection = {
      name: name.value.trim(),
      type: type.value,
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

// Asks for a connection's password, and whether the keychain retains it.
export function askPassword(name, why) {
  return new Promise((done) => {
    const password = element("input", { className: "report-field", type: "password", autocomplete: "off" });
    const retain = element("input", { type: "checkbox", checked: true });
    // Enter in the password connects, and so does the button, wherever the keys are in the sheet.
    const form = element(
      "form",
      { className: "sheet-ask" },
      element("p", { textContent: why }),
      element("label", { className: "report-row" }, element("span", { textContent: `${name}'s password` }), password),
      element("label", { className: "report-row" }, retain, element("span", { textContent: "Retain it in the system's keychain" })),
      element("div", { className: "commit-buttons" }, element("button", { className: "primary", type: "submit", textContent: "Connect" })),
    );
    let answer = null;
    form.addEventListener("submit", (event) => {
      event.preventDefault();
      answer = { password: password.value, retain: retain.checked };
      dialog.close();
    });
    const dialog = state.hooks.sheet(form);
    dialog.addEventListener("close", () => done(answer));
    password.focus();
  });
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Code seen without going to it: a peek over the editor of the places found, each in an editor of its
// own of the file's text, live where a tab holds the file, the arrows going from one place to the
// next, and a place opened in a tab that is a preview until it is edited.

import { invoke } from "../../gui/src-js/bridge.js";
import { editing, openFile } from "../../gui/src-js/edit.js";
import { closePeek, openShown, peekHere, peekPlaces, peekShown } from "../../gui/src-js/peek.js";
import { showView } from "../../gui/src-js/views.js";

const { test, assert, ui } = window.__harness;

const tabs = () => [...document.querySelectorAll("#tabs .tab")];
const tabNamed = (name) => tabs().find((one) => one.textContent.includes(name));

const LIB = `${Array.from({ length: 40 }, (_, at) => `# line ${at}`).join("\n")}\ndef helper():\n    return 1\n`;

test("a peek shows a place over the editor under the cursor's line, and no tab opens", async () => {
  await invoke("file_write", { path: "peek_main.py", text: "def main():\n    helper()\n    other()\n" });
  await invoke("file_write", { path: "peek_lib.py", text: LIB });
  showView("edit");
  await openFile("peek_main.py");
  await ui.waitFor(() => editing().active === "peek_main.py", 5000);
  const before = tabs().length;
  editing().editor.goTo(1, 6);
  peekPlaces("Definition of helper", [{ path: "peek_lib.py", line: 40, col: 4, text: "def helper():" }]);
  await ui.waitFor(() => peekShown()?.editor?.s?.doc.line(40) === "def helper():", 3000);
  const shown = peekShown();
  assert.equal(tabs().length, before, "no tab opened");
  assert.equal(shown.editor.head().line, 40);
  assert.equal(shown.editor.s.readOnly, true, "a file no tab holds shows as it was read");
  const line = editing().editor.rectOf({ line: 1, col: 0 });
  assert.ok(Math.abs(shown.panel.getBoundingClientRect().top - line.bottom) < 2, "under the cursor's line");
  assert.match(shown.panel.querySelector(".code-peek-where").textContent, /^peek_lib\.py:41$/);
  await ui.key("Escape");
  assert.equal(peekShown(), null, "Escape closes it");
});

test("several places stand in a list the arrows go through, and a file a tab holds shows live and takes edits", async () => {
  await openFile("peek_lib.py");
  await openFile("peek_main.py");
  await ui.waitFor(() => editing().active === "peek_main.py", 5000);
  editing().editor.goTo(1, 6);
  peekPlaces("Usages of helper", [
    { path: "peek_main.py", line: 1, col: 4, text: "    helper()" },
    { path: "peek_lib.py", line: 40, col: 4, text: "def helper():" },
  ]);
  await ui.waitFor(() => peekShown()?.shown === 0 && peekShown().editor.s?.doc.line(1) === "    helper()", 3000);
  const rows = peekShown().panel.querySelectorAll(".code-peek-place");
  assert.equal(rows.length, 2);
  assert.equal(document.activeElement, rows[0], "the first place has the keys");
  await ui.key("Down");
  await ui.waitFor(() => peekShown().shown === 1 && peekShown().editor.head().line === 40, 3000);
  assert.equal(document.activeElement, rows[1]);
  const view = peekShown().editor;
  assert.equal(view.s.readOnly, false, "the tab's own text");
  view.goTo(41, 0);
  view.focus();
  await ui.type("# peeked ");
  await ui.waitFor(() => tabNamed("peek_lib.py")?.querySelector(".dirty"), 3000).catch(() => {
    throw new Error(`the tab of peek_lib.py shows no change: ${tabNamed("peek_lib.py")?.outerHTML.slice(0, 200)}`);
  });
  closePeek();
});

test("Open takes the place to a preview tab, the next preview takes its place, and an edit keeps it", async () => {
  await invoke("file_write", { path: "peek_a.py", text: "a = 1\n" });
  await invoke("file_write", { path: "peek_b.py", text: "b = 2\n" });
  peekPlaces("Usages of a", [{ path: "peek_a.py", line: 0, col: 0, text: "a = 1" }]);
  await ui.waitFor(() => peekShown()?.editor?.s, 3000);
  await openShown();
  await ui.waitFor(() => editing().active === "peek_a.py", 3000);
  assert.equal(peekShown(), null, "the peek closes");
  assert.ok(tabNamed("peek_a.py").classList.contains("preview"), "a preview, its name in italics");
  peekPlaces("Usages of b", [{ path: "peek_b.py", line: 0, col: 0, text: "b = 2" }]);
  await ui.waitFor(() => peekShown()?.editor?.s, 3000);
  await openShown();
  await ui.waitFor(() => editing().active === "peek_b.py", 3000);
  assert.equal(tabNamed("peek_a.py"), undefined, "the next preview took its place");
  editing().editor.goTo(0, 0);
  editing().editor.focus();
  await ui.type("# ");
  await ui.waitFor(() => !tabNamed("peek_b.py").classList.contains("preview"), 2000);
  await openFile("peek_a.py", { preview: true });
  await ui.waitFor(() => tabNamed("peek_a.py")?.classList.contains("preview"), 2000);
  assert.ok(tabNamed("peek_b.py"), "an edited tab is kept when the next preview opens");
  await ui.double(tabNamed("peek_a.py"));
  await ui.waitFor(() => !tabNamed("peek_a.py").classList.contains("preview"), 2000);
});

test("Peek Definition and Peek Usages ask the language server, here clangd", async () => {
  // names no other file of the tree defines: a C function of another file of the same name is the same function to
  // clangd's index, as main is in every program of the tree
  await invoke("file_write", { path: "peek.c", text: "int peek_helper(void) { return 1; }\n\nint peek_caller(void) {\n  return peek_helper() + peek_helper();\n}\n" });
  showView("edit");
  await openFile("peek.c");
  await ui.waitFor(() => editing().active === "peek.c", 5000);
  editing().editor.goTo(3, 10);
  await ui.waitFor(async () => {
    await peekHere("definition");
    return peekShown()?.places.length > 0;
  }, 30000);
  assert.deepEqual(peekShown().places.map((one) => [one.path, one.line]), [["peek.c", 0]]);
  assert.equal(peekShown().title, "Definition of peek_helper");
  closePeek();
  editing().editor.goTo(3, 10);
  await peekHere("usages");
  await ui.waitFor(() => peekShown()?.places.length >= 2, 10000);
  const lines = peekShown().places.map((one) => one.line);
  assert.equal(lines.filter((line) => line === 3).length, 2, `both calls on line 4: ${JSON.stringify(lines)}`);
  closePeek();
  editing().editor.goTo(0, 6);
  await peekHere("callers");
  await ui.waitFor(() => peekShown()?.places.length >= 1, 10000);
  assert.equal(peekShown().title, "Callers of peek_helper");
  assert.ok(peekShown().places.every((one) => one.path === "peek.c" && one.line === 3 && one.caller === "peek_caller"), JSON.stringify(peekShown().places));
  closePeek();
});

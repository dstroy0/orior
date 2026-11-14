// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The servers a tab is passed to: a change told once the frame that draws it is drawn, with no wait of
// the window's own, and the keys typed while a change is on its way told together after it; a C file's
// server given the tree's slice for the view the page rests on, its answers on the file's lines; a C
// name's usages and renames read from the tree across its files; the tree's check giving a C file no
// tab holds a window of slices at a time; and clangd running only while it is asked.

import { invoke, watchCalls } from "../../gui/src-js/bridge.js";
import { editing, openFile } from "../../gui/src-js/edit.js";
import { closePeek, peekHere, peekShown } from "../../gui/src-js/peek.js";
import { knownProblems } from "../../gui/src-js/servers.js";
import { showView } from "../../gui/src-js/views.js";
import { press, putBackAsFound } from "../menus.js";

const { test, assert, ui, calls } = window.__harness;

const wait = (ms) => new Promise((done) => setTimeout(done, ms));

test("a change is told to the tab's server with no wait of the window's own, and keys typed while one is on its way are told together after it", async () => {
  showView("edit");
  await openFile("hello.py");
  await ui.waitFor(() => editing().active === "hello.py" && editing().editor?.s, 5000);
  const view = editing().editor;
  const before = view.doc.text();
  const told = [];
  let release = null;
  watchCalls((name, args, send) => {
    if (name === "lsp_change" && args.path.endsWith("hello.py")) {
      told.push({ at: performance.now(), text: args.text });
      if (told.length === 1) {
        return new Promise((done) => (release = () => done(calls.watcher(name, args, send))));
      }
    }
    return calls.watcher(name, args, send);
  });
  try {
    view.goTo(0, 0);
    view.focus();
    await ui.type("a");
    const typed = performance.now();
    await ui.waitFor(() => told.length === 1, 2000);
    assert.ok(told[0].at - typed < 150, `told ${Math.round(told[0].at - typed)} ms after the key`);
    await ui.type("bc");
    await wait(300);
    assert.equal(told.length, 1, "nothing more is told while the change before it is on its way");
    release();
    await ui.waitFor(() => told.length === 2, 2000);
    assert.ok(told[1].text.startsWith("abc"), "the keys typed meanwhile are told together, after it");
  } finally {
    release?.();
    watchCalls(calls.watcher);
    for (let undo = 0; undo < 6 && view.doc.text() !== before; undo += 1) {
      await ui.key("Ctrl+Z");
    }
  }
  assert.equal(view.doc.text(), before, "the file is as it was");
});

// A header and a file far longer than a view, the view at its end: the definition of a name the
// header declares, a problem on the line after it and the server's colors, each on the lines they
// are on.
const SHAPE = "#ifndef SHAPE_H\n#define SHAPE_H\nstruct box {\n    int w;\n    int h;\n};\nint area(const struct box *b);\n#endif\n";
const PADS = 120;
const USE = [
  '#include "sliced_shape.h"',
  ...Array.from({ length: PADS }, (_, at) => `static int pad${at}(void) { return ${at}; }`),
  "int main(void)",
  "{",
  "    struct box b = {2, 3};",
  "    int got = area(&b);",
  "    return got + missing;",
  "}",
  "",
].join("\n");
const CALL = PADS + 4;
const PROBLEM = CALL + 1;

test("a C file's server is given the tree's slice for the view the page rests on: its problems and a definition in a header are on the file's own lines, and its colors are the highlighter's", async () => {
  await invoke("file_write", { path: "src/sliced_shape.h", text: SHAPE });
  await invoke("file_write", { path: "src/sliced_use.c", text: USE });
  // the tree reads the header first, so the slice holds what it declares
  await ui.waitFor(async () => {
    const found = await invoke("symbols_find", { query: "area" }).catch(() => null);
    return (found?.symbols ?? []).some((one) => one.name === "area" && one.path.endsWith("src/sliced_shape.h"));
  }, 30000, "the tree to read src/sliced_shape.h");
  const told = [];
  watchCalls((name, args, send) => {
    if (name === "lsp_view" && args.path.endsWith("src/sliced_use.c")) {
      told.push(args);
    }
    return calls.watcher(name, args, send);
  });
  try {
    showView("edit");
    await openFile("src/sliced_use.c");
    await ui.waitFor(() => editing().active === "src/sliced_use.c" && editing().editor?.s, 5000, "src/sliced_use.c open");
    const view = editing().editor;
    view.goTo(CALL, 14);
    await ui.waitFor(() => told.some((one) => one.first <= PROBLEM && one.last >= PROBLEM), 10000, "the view told once it rests");
    const session = () => editing().sessionOf("src/sliced_use.c");
    await ui.waitFor(() => (session().diagnostics ?? []).some((one) => one.from.line === PROBLEM && /missing/.test(one.message)), 30000, "clangd's problem on the line it is on").catch((error) => {
      throw new Error(`${error.message}: ${JSON.stringify(session().diagnostics)}`);
    });
    assert.ok((session().diagnostics ?? []).every((one) => one.from.line === PROBLEM), `no problem on another line: ${JSON.stringify(session().diagnostics)}`);
    await ui.waitFor(async () => {
      view.goTo(CALL, 14);
      await peekHere("definition");
      return peekShown()?.places.length > 0;
    }, 30000, "the definition of area");
    assert.deepEqual(peekShown().places.map((one) => [one.path, one.line]), [["src/sliced_shape.h", 6]]);
    closePeek();
    assert.ok(!session().highlight.parse || session().highlight.parse.whole, "the server is asked for no colors of a file it is given as a slice");
  } finally {
    watchCalls(calls.watcher);
    // a view is told only where it moves, so a run again opens the file anew
    if (editing().active === "src/sliced_use.c") {
      await press("close-editor");
    }
  }
}, { timeout: 120000 });

// A function a header declares, a file that defines it and a file that calls it twice.
const DECLARES = "#ifndef RENAMED_H\n#define RENAMED_H\nint renamed_area(int w);\n#endif\n";
const DEFINES = '#include "renamed_shape.h"\nint renamed_area(int w)\n{\n    return w * 2;\n}\n';
const CALLS = '#include "renamed_shape.h"\nint renamed_total(int n)\n{\n    return renamed_area(n) + renamed_area(1);\n}\n';

test("a C name's usages and its renames come from the tree, across the files whose walks meet the same declaration", async () => {
  await invoke("file_write", { path: "src/renamed_shape.h", text: DECLARES });
  await invoke("file_write", { path: "src/renamed_area.c", text: DEFINES });
  await invoke("file_write", { path: "src/renamed_use.c", text: CALLS });
  await ui.waitFor(async () => {
    const found = await invoke("symbols_find", { query: "renamed_area" }).catch(() => null);
    return ["src/renamed_shape.h", "src/renamed_area.c"].every((path) => (found?.symbols ?? []).some((one) => one.name === "renamed_area" && one.path.endsWith(path)));
  }, 30000, "the tree to read the header and the definition");
  showView("edit");
  await openFile("src/renamed_use.c");
  await ui.waitFor(() => editing().active === "src/renamed_use.c" && editing().editor?.s, 5000, "src/renamed_use.c open");
  const view = editing().editor;
  await ui.waitFor(async () => {
    view.goTo(3, 12);
    await peekHere("usages");
    return peekShown()?.places.length >= 4;
  }, 30000, "the usages of renamed_area");
  const places = peekShown().places.map((one) => `${one.path}:${one.line}:${one.col}`).sort();
  assert.deepEqual(places, ["src/renamed_area.c:1:4", "src/renamed_shape.h:2:4", "src/renamed_use.c:3:11", "src/renamed_use.c:3:29"]);
  closePeek();
  view.goTo(3, 12);
  await press("rename");
  const field = await ui.waitFor(() => document.querySelector(".ed-rename"), 10000, "the rename field");
  assert.equal(field.value, "renamed_area");
  field.value = "renamed_measure";
  field.dispatchEvent(new KeyboardEvent("keydown", { key: "Enter", bubbles: true }));
  await ui.waitFor(async () => ((await invoke("file_read", { path: "src/renamed_area.c" }).catch(() => null))?.text ?? "").includes("int renamed_measure(int w)"), 20000, "the definition renamed in its file");
  assert.ok((await invoke("file_read", { path: "src/renamed_shape.h" })).text.includes("int renamed_measure(int w);"), "the header's declaration renamed");
  assert.equal(view.doc.text().split("renamed_measure").length - 1, 2, "both calls renamed in the tab");
}, { timeout: 120000 });

// A file of 141 lines whose one problem, a call of a function nothing declares, is on its line 100, in
// its second window, and a header it includes.
const CHECKED_LINES = ['#include "checked_shape.h"', "int checked_total(int n)", "{", "    int total = 0;"];
for (let line = 4; line < 100; line += 1) {
  CHECKED_LINES.push(`    total += checked_area(${line});`);
}
CHECKED_LINES.push("    total += checked_missing(n);");
for (let line = 101; line < 139; line += 1) {
  CHECKED_LINES.push(`    total += ${line};`);
}
CHECKED_LINES.push("    return total;", "}");

test("the tree's check gives a C file no tab holds to clangd a window of slices at a time, and reports the problems clangd reports on it whole, each on the file's own line", async () => {
  await invoke("file_write", { path: "src/checked_shape.h", text: "int checked_area(int w);\n" });
  await invoke("file_write", { path: "src/checked_use.c", text: `${CHECKED_LINES.join("\n")}\n` });
  invoke("problems_check").catch(() => {});
  const problems = () => [...knownProblems().entries()].find(([path]) => path.endsWith("src/checked_use.c"))?.[1] ?? [];
  await ui.waitFor(() => problems().some((one) => one.from.line === 100), 120000, "the problem on line 100 among the file's problems");
  assert.deepEqual(problems().map((one) => [one.from.line, one.from.col]), [[100, 13]], `the one problem clangd reports on the whole file: ${JSON.stringify(problems())}`);
  assert.match(problems()[0].message, /checked_missing/);
}, { timeout: 180000 });

// A header, and a file whose one problem is a name nothing declares, on its line 3.
const RETAINED_SHAPE = "int retained_area(int w);\n";
const RETAINED_USE = '#include "retained_shape.h"\nint retained_total(int n)\n{\n    return retained_area(n) + retained_missing;\n}\n';

const retainedRuns = async () => ((await invoke("memory_usage"))?.parts ?? []).some((one) => one.server === "llvm");
const retainedSession = () => editing().sessionOf("src/retained_use.c");
const retainedShown = () => (retainedSession().diagnostics ?? []).map((one) => [one.from.line, one.from.col, one.message]);

// The header and the file written and read by the tree, the file open with clangd's problem on line 3 displayed, and
// clangd let go once nothing asks it: the problems displayed then, as JSON.
async function retainedLetGo() {
  await invoke("file_write", { path: "src/retained_shape.h", text: RETAINED_SHAPE });
  await invoke("file_write", { path: "src/retained_use.c", text: RETAINED_USE });
  await ui.waitFor(async () => {
    const found = await invoke("symbols_find", { query: "retained_area" }).catch(() => null);
    return (found?.symbols ?? []).some((one) => one.name === "retained_area" && one.path.endsWith("src/retained_shape.h"));
  }, 30000, "the tree to read src/retained_shape.h");
  showView("edit");
  await openFile("src/retained_use.c");
  await ui.waitFor(() => editing().active === "src/retained_use.c" && editing().editor?.s, 5000, "src/retained_use.c open");
  await ui.waitFor(() => retainedShown().length === 1 && retainedShown()[0][0] === 3 && /retained_missing/.test(retainedShown()[0][2]), 30000, "clangd's one problem, on line 3").catch((error) => {
    throw new Error(`${error.message}: ${JSON.stringify(retainedShown())}`);
  });
  const before = JSON.stringify(retainedShown());
  await ui.waitFor(async () => !(await retainedRuns()), 75000, "clangd let go once nothing asked it for a while");
  assert.equal(JSON.stringify(retainedShown()), before, "the problems displayed are as clangd is let go");
  return before;
}

test("clangd runs only while it is asked: let go once nothing asks it for a while, started again by an edit it checks, the problems displayed located throughout", async () => {
  const before = await retainedLetGo();
  const view = editing().editor;
  const text = view.doc.text();
  try {
    view.goTo(3, RETAINED_USE.split("\n")[3].length);
    view.focus();
    await ui.type(" retained_other;");
    await ui.waitFor(() => retainedShown().some((one) => one[0] === 3 && /retained_other/.test(one[2])), 30000, "clangd's problem of the edit on line 3").catch((error) => {
      throw new Error(`${error.message}: ${JSON.stringify(retainedShown())}`);
    });
    assert.ok(await retainedRuns(), "clangd runs again to check the edit");
    for (const one of JSON.parse(before)) {
      assert.ok(retainedShown().some((now) => JSON.stringify(now) === JSON.stringify(one)), `the problem displayed before is: ${JSON.stringify(one)}`);
    }
  } finally {
    for (let undo = 0; undo < 16 && view.doc.text() !== text; undo += 1) {
      await ui.key("Ctrl+Z");
    }
    if (editing().active === "src/retained_use.c") {
      await press("close-editor");
    }
  }
}, { timeout: 180000 });

test("everything orior answers about a C file but its problems comes from the tree: hover, definition, usages, rename, signature, completion, hints, calls and selection answered while clangd is let go, and none of them starts it", async () => {
  await retainedLetGo();
  try {
    const at = { path: "src/retained_use.c", line: 3, col: 13 };
    const view = editing().editor;
    view.goTo(at.line, at.col);
    const hover = await retainedSession().language.hover(view.doc, { line: at.line, col: at.col });
    assert.match(JSON.stringify(hover?.parts ?? null), /int retained_area\(int w\);/, `the hover is the tree's declaration: ${JSON.stringify(hover?.parts ?? null)}`);
    assert.equal(await (hover?.more ?? Promise.resolve(null)), null, "and no server's hover follows it");
    assert.deepEqual(await invoke("lsp_definition", at), [{ path: "src/retained_shape.h", line: 0, col: 4 }], "the definition is the header's declaration");
    const usages = (await invoke("lsp_references", at)).map((one) => `${one.path}:${one.from.line}:${one.from.col}`).sort();
    assert.deepEqual(usages, ["src/retained_shape.h:0:4", "src/retained_use.c:3:11"], "the usages are the tree's");
    assert.deepEqual(await invoke("lsp_renamable", at), { from: { line: 3, col: 11 }, to: { line: 3, col: 20 }, name: "retained_area" }, "and so is what a rename takes");
    const signature = await invoke("symbols_signature", { path: at.path, text: RETAINED_USE, state: "root", line: 3, col: 21 }).catch(() => null);
    assert.match(JSON.stringify(signature), /retained_area/, `the signature is the tree's: ${JSON.stringify(signature)}`);
    const completed = (await invoke("lsp_complete", { path: at.path, line: 3, col: 15 })).map((one) => `${one.label} ${one.type}`);
    assert.deepEqual(completed, ["retained_area function", "retained_total function"], "what completes `retained` is what the tree has in effect there");
    const hints = (await invoke("lsp_hints", { path: at.path, from: 3, to: 3 })).map((one) => [one.line, one.col, one.label, one.type]);
    assert.deepEqual(hints, [[3, 21, "w:", 2]], "the hint before the call's argument is the tree's parameter name");
    const root = await invoke("calls_root", at);
    assert.match(JSON.stringify(root), /retained_total/, `the call hierarchy's top is the tree's function: ${JSON.stringify(root)}`);
    const spans = (await invoke("parse_spans", { path: at.path, from: { line: 3, col: 13 }, to: { line: 3, col: 13 } })).map(([from, to]) => `${from.line}:${from.col}-${to.line}:${to.col}`);
    assert.deepEqual(spans, ["3:11-3:20", "3:4-3:39", "2:1-4:0", "2:0-4:1", "1:0-4:1", "0:0-5:0"], "Expand Selection's spans are the tree's: the name, its statement, the body inside and whole, the function and the file");
    assert.equal(await invoke("lsp_hover", at), null, "the server's hover answers none");
    assert.ok(!(await retainedRuns()), "and clangd is not started by any of them");
  } finally {
    if (editing().active === "src/retained_use.c") {
      await press("close-editor");
    }
  }
}, { timeout: 180000 });

test("the window is put back as the file found it", putBackAsFound);

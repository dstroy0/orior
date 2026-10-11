// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The Markdown preview, beside the text and alone: drawn as the text changes, a block at a time,
// and scrolled with the text by their lines.

import { invoke } from "../../src/bridge.js";
import { editing, openFile } from "../../src/edit.js";
import { lastDrawing, previewMode, resolveFrom, setPreview } from "../../src/mdpreview.js";
import { runCommand } from "../../src/menubar.js";
import { showView } from "../../src/views.js";

const { test, assert, ui } = window.__harness;

const pane = () => document.getElementById("md-preview");
const desk = () => document.querySelector("#mode-edit .desk");

// A Markdown file of many sections, each a heading and a paragraph, the sections' first lines known.
const sections = Array.from({ length: 60 }, (_, at) => `## Section ${at}\n\nThe text of section ${at}, a line long.\n`);
const TEXT = `# The preview\n\n${sections.join("\n")}`;

test("an address relative to the file is read from its own folder in the tree", () => {
  const resolve = resolveFrom("docs/guide/read.md");
  const base = navigator.userAgent.includes("Windows") ? "http://view.localhost/" : "view://localhost/";
  assert.equal(resolve("img/a b.png"), `${base}docs/guide/img/a%20b.png`);
  assert.equal(resolve("../top.md#part"), `${base}docs/top.md#part`);
  assert.equal(resolve("https://example.com/x.png"), "https://example.com/x.png");
  assert.equal(resolve("#here"), "#here");
});

test("View, Markdown Preview shows the file in the editor beside its text", async () => {
  await invoke("file_write", { path: "preview.md", text: TEXT });
  showView("edit");
  await openFile("preview.md");
  await ui.waitFor(() => editing().active === "preview.md", 5000);
  assert.equal(previewMode(), "off");
  assert.equal(pane().hidden, true);
  runCommand("markdown-preview");
  await ui.waitFor(() => !pane().hidden && pane().querySelector("h1"), 3000);
  assert.equal(desk().dataset.preview, "beside");
  assert.equal(pane().querySelector("h1").textContent, "The preview");
  assert.equal(pane().querySelectorAll("h2").length, 60);
});

test("a key typed redraws only the block it changed, in time the page has free", async () => {
  const { editor } = editing();
  editor.goTo(4, 0);
  editor.focus();
  await ui.type("Changed: ");
  await ui.waitFor(() => /Changed: The text of section 0/.test(pane().textContent), 3000);
  const drawn = lastDrawing();
  assert.equal(drawn.drawn, 1, `one block drawn again: ${JSON.stringify(drawn)}`);
  assert.ok(drawn.kept > 100, "the others kept");
});

test("the preview scrolls with the text by their lines, and the text with the preview", async () => {
  const { editor } = editing();
  editor.goTo(150, 0);
  editor.place(150 * (editor.scroller.scrollHeight / Math.max(1, editor.allRows())));
  const topLine = () => Number([...pane().children].find((one) => one.offsetTop + one.offsetHeight > pane().scrollTop)?.dataset.line);
  await ui.waitFor(() => Math.abs(topLine() - 150) <= 4, 3000).catch((error) => {
    throw new Error(`${error.message}: the preview's top stands at line ${topLine()}, scrolled ${pane().scrollTop} of ${pane().scrollHeight}; the editor's at ${editor.scrollY()}`);
  });
  pane().scrollTop = 0;
  await ui.waitFor(() => editor.scrollY() < 40, 3000).catch((error) => {
    throw new Error(`${error.message}: the editor stands at ${editor.scrollY()} with the preview scrolled ${pane().scrollTop}`);
  });
});

test("alone, the preview takes the editor's whole place; off, the text has it back", async () => {
  runCommand("markdown-preview-alone");
  await ui.waitFor(() => desk().dataset.preview === "alone", 2000);
  assert.equal(getComputedStyle(document.getElementById("editor")).visibility, "hidden");
  assert.ok(pane().getBoundingClientRect().width > desk().getBoundingClientRect().width * 0.9);
  setPreview("off");
  assert.equal(pane().hidden, true);
  assert.equal(getComputedStyle(document.getElementById("editor")).visibility, "visible");
});

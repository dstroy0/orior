// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The live preview, beside the text and alone: Markdown drawn as the text changes, a block at a time,
// and scrolled with the text by their lines; a page of HTML patched where it changed and its
// stylesheet swapped in place; a CSS file over a page of its own; SVG and a Mermaid file drawn; and
// the place the cursor stands marked in each.

import { invoke } from "../../src/bridge.js";
import { editing, openFile } from "../../src/edit.js";
import { runCommand } from "../../src/menubar.js";
import { kindOf, lastDrawing, previewMode, resolveFrom, setPreview } from "../../src/preview.js";
import { showView } from "../../src/views.js";

const { test, assert, ui } = window.__harness;

const pane = () => document.getElementById("preview");
const desk = () => document.querySelector("#mode-edit .desk");
const frame = () => pane().querySelector(".preview-frame");
const page = () => frame()?.contentDocument;

// A Markdown file of many sections, each a heading and a paragraph, the sections' first lines known.
const sections = Array.from({ length: 60 }, (_, at) => `## Section ${at}\n\nThe text of section ${at}, a line long.\n`);
const TEXT = `# The preview\n\n${sections.join("\n")}`;

async function open(path) {
  showView("edit");
  await openFile(path);
  await ui.waitFor(() => editing().active === path, 5000);
}

// Replaces the whole text of the file in the editor, as a paste over all of it does.
function retype(text) {
  const { editor } = editing();
  editor.selectAll();
  editor.type(text);
}

test("each kind of file that sets how something looks has a preview, and no other", () => {
  const kinds = ["a.md", "a.MARKDOWN", "b.html", "b.htm", "c.css", "d.svg", "e.mmd", "e.mermaid", "f.tex", "a.js", "a.rs", null].map(kindOf);
  assert.deepEqual(kinds, ["markdown", "markdown", "html", "html", "css", "svg", "mermaid", "mermaid", "latex", null, null, null]);
});

test("an address relative to the file is read from its own folder in the tree", () => {
  const resolve = resolveFrom("docs/guide/read.md");
  const base = navigator.userAgent.includes("Windows") ? "http://view.localhost/" : "view://localhost/";
  assert.equal(resolve("img/a b.png"), `${base}docs/guide/img/a%20b.png`);
  assert.equal(resolve("../top.md#part"), `${base}docs/top.md#part`);
  assert.equal(resolve("https://example.com/x.png"), "https://example.com/x.png");
  assert.equal(resolve("#here"), "#here");
});

test("View, Preview shows the Markdown file in the editor beside its text", async () => {
  await invoke("file_write", { path: "preview.md", text: TEXT });
  await open("preview.md");
  assert.equal(previewMode(), "off");
  assert.equal(pane().hidden, true);
  runCommand("preview");
  await ui.waitFor(() => !pane().hidden && pane().querySelector("h1"), 3000);
  assert.equal(desk().dataset.preview, "beside");
  assert.equal(pane().dataset.kind, "markdown");
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

test("the block the cursor stands in is marked", async () => {
  const { editor } = editing();
  editor.goTo(6, 2);
  await ui.waitFor(() => pane().querySelector(".here")?.textContent === "Section 1", 2000).catch((error) => {
    throw new Error(`${error.message}: marked ${pane().querySelector(".here")?.outerHTML.slice(0, 120)}`);
  });
  assert.equal(pane().querySelectorAll(".here").length, 1);
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
  runCommand("preview-alone");
  await ui.waitFor(() => desk().dataset.preview === "alone", 2000);
  assert.equal(getComputedStyle(document.getElementById("editor")).visibility, "hidden");
  assert.ok(pane().getBoundingClientRect().width > desk().getBoundingClientRect().width * 0.9);
  setPreview("off");
  assert.equal(pane().hidden, true);
  assert.equal(getComputedStyle(document.getElementById("editor")).visibility, "visible");
});

const HTML = '<!doctype html>\n<html>\n<head>\n<link rel="stylesheet" href="look.css">\n</head>\n<body>\n<h1>Title</h1>\n<input id="field">\n<div style="height: 3000px">tall</div>\n<p id="end">End</p>\n</body>\n</html>\n';

test("a page of HTML is drawn in a frame that runs none of its scripts, its linked sheet read from the tree", async () => {
  await invoke("folder_create", { path: "site" }).catch(() => {});
  await invoke("file_write", { path: "site/look.css", text: "h1 { color: rgb(10, 20, 30); }\n" });
  await invoke("file_write", { path: "site/index.html", text: HTML.replace("</body>", '<script>document.title = "ran";</script>\n</body>') });
  await open("site/index.html");
  setPreview("beside");
  await ui.waitFor(() => page()?.getElementById("end"), 3000);
  assert.equal(pane().dataset.kind, "html");
  assert.equal(frame().getAttribute("sandbox"), "allow-same-origin");
  assert.notEqual(page().title, "ran", "the page's script did not run");
  await ui.waitFor(() => page().defaultView.getComputedStyle(page().querySelector("h1")).color === "rgb(10, 20, 30)", 3000);
});

test("a change to the page is patched where it changed, its scroll and its fields kept", async () => {
  retype(HTML);
  await ui.waitFor(() => !page().querySelector("script"), 3000);
  const before = page();
  page().getElementById("field").value = "typed";
  const { editor } = editing();
  editor.goTo(9, 15);
  editor.focus();
  await ui.rest(100);
  const bottom = page().scrollingElement.scrollHeight - page().defaultView.innerHeight;
  page().scrollingElement.scrollTop = bottom;
  await ui.type(" again");
  await ui.waitFor(() => page().getElementById("end").textContent === "End again", 3000);
  const drawn = lastDrawing();
  assert.ok(!drawn.whole && drawn.changed >= 1 && drawn.added === 0, `patched: ${JSON.stringify(drawn)}`);
  assert.equal(page(), before, "the same page, not loaded again");
  assert.equal(page().getElementById("field").value, "typed");
  assert.equal(page().scrollingElement.scrollTop, bottom, "at the same scroll");
});

test("the element the cursor stands in is outlined over the frame, and brought into sight", async () => {
  const { editor } = editing();
  editor.goTo(7, 3);
  const here = () => pane().querySelector(".preview-here");
  const over = () => {
    const field = page().getElementById("field").getBoundingClientRect();
    const box = here().getBoundingClientRect();
    const outer = frame().getBoundingClientRect();
    return !here().hidden && Math.abs(box.top - (outer.top + field.top)) < 2 && Math.abs(box.width - field.width) < 2 && field.top >= 0;
  };
  await ui.waitFor(over, 3000).catch((error) => {
    throw new Error(`${error.message}: the outline stands at ${JSON.stringify(here().getBoundingClientRect())}, the field at ${JSON.stringify(page().getElementById("field").getBoundingClientRect())}`);
  });
});

test("while its stylesheet is edited the page stays shown, the sheet swapped into it as it is typed", async () => {
  const before = page();
  page().scrollingElement.scrollTop = 600;
  await open("site/look.css");
  assert.equal(pane().dataset.kind, "html", "still the page");
  const { editor } = editing();
  editor.goTo(0, 0);
  editor.focus();
  await ui.type("p { color: rgb(1, 2, 3) } ");
  await ui.waitFor(() => page().defaultView.getComputedStyle(page().getElementById("end")).color === "rgb(1, 2, 3)", 3000);
  assert.equal(page(), before, "the same page");
  assert.equal(page().scrollingElement.scrollTop, 600, "at the same scroll");
  assert.ok(page().querySelector('style[data-orior-sheet="look.css"]'), "the tab's text in place of the link");
});

test("a CSS file no page shown links is drawn over a page of its own, made from its selectors", async () => {
  await invoke("file_write", { path: "alone.css", text: "h2.big { font-size: 40px; }\nbutton:hover { x: y }\n" });
  await open("alone.css");
  await ui.waitFor(() => page()?.querySelector("h2.big"), 3000);
  assert.equal(pane().dataset.kind, "css");
  assert.equal(page().querySelector("h2.big").textContent, "h2.big");
  assert.equal(page().defaultView.getComputedStyle(page().querySelector("h2.big")).fontSize, "40px");
  assert.equal(page().querySelector("button").textContent, "button:hover");
});

test("SVG is drawn on a page of its own, and patched as it changes", async () => {
  await invoke("file_write", { path: "shape.svg", text: '<svg xmlns="http://www.w3.org/2000/svg" width="40" height="40">\n<rect id="r" width="10" height="10"/>\n</svg>\n' });
  await open("shape.svg");
  await ui.waitFor(() => page()?.getElementById("r"), 3000);
  assert.equal(pane().dataset.kind, "svg");
  const rect = page().getElementById("r");
  retype('<svg xmlns="http://www.w3.org/2000/svg" width="40" height="40">\n<rect id="r" width="30" height="10"/>\n</svg>\n');
  await ui.waitFor(() => page().getElementById("r").getAttribute("width") === "30", 3000);
  assert.equal(page().getElementById("r"), rect, "the same element, patched");
});

test("a Mermaid file is drawn as its diagram, drawn again only when its text changes, the nodes the cursor's line names marked", async () => {
  await invoke("file_write", { path: "flow.mmd", text: "graph TD\n  A[Start] --> B[End]\n  B --> C[Again]\n" });
  await open("flow.mmd");
  await ui.waitFor(() => pane().querySelectorAll(".mm-node-group").length === 3, 3000);
  assert.equal(pane().dataset.kind, "mermaid");
  const { editor } = editing();
  editor.goTo(2, 2);
  await ui.waitFor(() => pane().querySelectorAll(".mm-node-group.here").length === 2, 2000);
  assert.deepEqual([...pane().querySelectorAll(".mm-node-group.here")].map((node) => node.dataset.id), ["B", "C"]);
  const svg = pane().querySelector("svg");
  editor.goTo(1, 0);
  await ui.rest(250);
  assert.equal(pane().querySelector("svg"), svg, "a cursor's move draws nothing again");
  editor.focus();
  await ui.type("  D --> A\n");
  await ui.waitFor(() => pane().querySelectorAll(".mm-node-group").length === 4, 3000);
  assert.deepEqual(lastDrawing(), { kept: 0, moved: 0, drawn: 1 });
  setPreview("off");
});

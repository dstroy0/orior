// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The live preview: the file in the editor shown as it reads, beside its text or alone, as the reader
// chooses once for every file it shows (View, Preview, Ctrl+K V), for each kind of file that sets how
// something looks and runs nothing: Markdown, HTML and the CSS it takes, CSS over a page of its own,
// SVG, Mermaid's diagrams and LaTeX. Each change is drawn as it is typed, and the place the cursor
// stands in the text is marked in the preview.
//
// Markdown is drawn a block at a time, each block tied to the line it came from, and the text and the
// preview scroll by those lines: the one the reader scrolls leads, and the other follows to the same
// place between two blocks, and a scroll made to follow leads nothing in turn. As the text changes,
// the preview is read again in time the page has free, never on the editor's: a block whose HTML is
// as it was stays as it was drawn, one whose lines only moved is told its new lines, and only a block
// that changed is drawn again, a diagram among them. A page of HTML, CSS or SVG is drawn in a frame
// and patched where it changed (pagepreview.js), and while a stylesheet the page links is edited, the
// preview stays on the page, the sheet swapped into it as it is typed; a Mermaid file is drawn as its
// diagram, drawn again only when its text changes; a LaTeX file is made into pages (texpreview.js).
// An image by a path relative to the file is read from the file's own folder; a link to another file
// of the tree opens it, a link to a place in the preview goes there, and a link out opens in the
// browser.

import { invoke } from "./bridge.js";
import { lineHeight } from "./editor/view.js";
import { editing, jumpTo, onCursorMoved, onEdit, onShown, openAt, openFile } from "./edit.js";
import { markdownBlocks } from "./markdown.js";
import { mermaidSvg } from "./mermaid.js";
import { PageFrame, pageOf } from "./pagepreview.js";
import { say } from "./statusbar.js";
import { hideTex, markTex, showTex, texChanged } from "./texpreview.js";

const KEY = "orior.preview";

// How often the text is looked at for a change while the preview shows, in milliseconds, and the
// longest a change waits for free time before it is drawn anyway.
const LOOK = 200;
const LATEST = 600;

const KINDS = [
  ["markdown", /\.(md|markdown|mdown|mkd|mdx)$/i],
  ["html", /\.(html?|xhtml)$/i],
  ["css", /\.css$/i],
  ["svg", /\.svg$/i],
  ["mermaid", /\.(mmd|mermaid)$/i],
  ["latex", /\.(tex|latex|ltx)$/i],
];

const state = { pane: null, kind: null, session: null, path: null, blocks: [], seen: null, timer: 0, idle: 0, paneSet: null, editorSet: null, scroller: null, diagram: null, frame: null, sheets: new Map(), linked: new Set(), last: null };

// The kind of preview the file at `path` has: "markdown", "html", "css", "svg", "mermaid" or
// "latex", or null for a file that has none.
export const kindOf = (path) => KINDS.find(([, pattern]) => pattern.test(path ?? ""))?.[0] ?? null;

export const isMarkdown = (path) => kindOf(path) === "markdown";

// How the reader sees a file the preview shows: "off", "beside" its text, or "alone".
export function previewMode() {
  const kept = localStorage.getItem(KEY);
  return kept === "beside" || kept === "alone" ? kept : "off";
}

export function setPreview(mode) {
  localStorage.setItem(KEY, mode === "beside" || mode === "alone" ? mode : "off");
  apply();
}

const VIEW = navigator.userAgent.includes("Windows") ? "http://view.localhost/" : "view://localhost/";

// The address an image or a link of the file at `path` names, as the page reaches it: one out of the
// tree as it is, one relative to the file from the file's own folder in the tree.
export function resolveFrom(path) {
  const folder = path.includes("/") ? path.slice(0, path.lastIndexOf("/")) : "";
  return (url) => {
    if (!url || /^([a-z][a-z0-9+.-]*:|#|\/\/)/i.test(url)) {
      return url;
    }
    const parts = (url.startsWith("/") ? url.slice(1) : folder ? `${folder}/${url}` : url).split("/");
    const out = [];
    for (const part of parts) {
      if (part === "..") {
        out.pop();
      } else if (part && part !== ".") {
        out.push(part);
      }
    }
    const [file, anchor] = out.join("/").split("#");
    return `${VIEW}${file.split("/").map(encodeURIComponent).join("/")}${anchor ? `#${anchor}` : ""}`;
  };
}

// The address of the folder of the file at `path`, as the page reaches it.
const folderAddress = (path) => `${VIEW}${path.slice(0, path.lastIndexOf("/") + 1).split("/").map(encodeURIComponent).join("/")}`;

// The path in the tree of an address of the file at `from`, or null for one out of the tree.
function treePath(href, from) {
  const to = resolveFrom(from)(href);
  return to?.startsWith(VIEW) ? decodeURIComponent(to.slice(VIEW.length).split("#")[0]) : null;
}

const LINES = / data-line="\d+"/g;

function element(html) {
  const holder = document.createElement("template");
  holder.innerHTML = html;
  return holder.content.firstElementChild ?? document.createElement("div");
}

function drawMarkdown(text) {
  const fresh = markdownBlocks(text, resolveFrom(state.path ?? ""));
  const shown = [...state.pane.children];
  const drawn = { kept: 0, moved: 0, drawn: 0 };
  for (let at = 0; at < Math.max(fresh.length, state.blocks.length); at += 1) {
    const was = state.blocks[at];
    const now = fresh[at];
    if (now === undefined) {
      shown[at]?.remove();
    } else if (was === undefined || !shown[at]) {
      state.pane.append(element(now));
      drawn.drawn += 1;
    } else if (was === now) {
      drawn.kept += 1;
    } else if (was.replace(LINES, "") === now.replace(LINES, "")) {
      // Only its lines moved: each element of the block is told its new line.
      const lines = [...now.matchAll(/data-line="(\d+)"/g)].map((match) => match[1]);
      [shown[at], ...shown[at].querySelectorAll("[data-line]")].filter((one) => one.dataset.line !== undefined).forEach((one, index) => (one.dataset.line = lines[index] ?? one.dataset.line));
      drawn.moved += 1;
    } else {
      shown[at].replaceWith(element(now));
      drawn.drawn += 1;
    }
  }
  state.blocks = fresh;
  state.last = drawn;
}

function drawDiagram(text) {
  if (text === state.diagram) {
    state.last = { kept: 1, moved: 0, drawn: 0 };
    return;
  }
  state.diagram = text;
  const holder = document.createElement("div");
  holder.className = "mermaid";
  holder.innerHTML = mermaidSvg(text);
  state.pane.replaceChildren(holder);
  state.last = { kept: 0, moved: 0, drawn: 1 };
}

function drawPage(text) {
  const sheets = new Map();
  state.linked = new Set();
  const sheet = (href) => {
    const path = treePath(href, state.path);
    if (path) {
      state.linked.add(path);
    }
    const session = path ? editing().sessionOf(path) : null;
    if (!session) {
      return null;
    }
    sheets.set(path, session);
    return { text: session.doc.text(), base: folderAddress(path) };
  };
  const doc = pageOf(state.kind, text, { base: folderAddress(state.path), sheet });
  if (doc) {
    state.sheets = sheets;
    state.last = state.frame.draw(doc);
  }
}

// Draws the text as it stands: Markdown block by block, keeping each block that has not changed, a
// page patched where it changed, a diagram where its text changed, and LaTeX made anew.
export function render() {
  const s = state.session;
  if (!s || !state.pane) {
    return;
  }
  state.seen = s.doc.id;
  if (state.kind === "markdown") {
    drawMarkdown(s.doc.text());
  } else if (state.kind === "mermaid") {
    drawDiagram(s.doc.text());
  } else if (state.kind === "latex") {
    texChanged();
  } else {
    drawPage(s.doc.text());
  }
  mark(false);
}

// What the last drawing did, for the tests: for Markdown and a diagram how many blocks it kept, told
// new lines, and drew; for a page whether it was drawn whole, or how many nodes it kept, changed,
// added and took out.
export const lastDrawing = () => state.last;

function schedule() {
  if (state.idle || !state.session || state.session.doc.id === state.seen) {
    return;
  }
  const run = () => {
    state.idle = 0;
    render();
  };
  state.idle = window.requestIdleCallback ? window.requestIdleCallback(run, { timeout: LATEST }) : window.setTimeout(run, LOOK);
}

function edited(session) {
  if (!state.session || state.pane.hidden) {
    return;
  }
  if (session === state.session) {
    if (state.kind === "latex") {
      texChanged();
    } else {
      schedule();
    }
  } else if (state.kind === "html") {
    // Another file changed, a stylesheet the page takes among those it may be.
    state.seen = null;
    schedule();
  }
}

// The Markdown block the cursor's line `line` stands in: of the blocks tied to the nearest line at or
// before it, the innermost.
function markBlock(line) {
  let best = null;
  let bestLine = -1;
  for (const one of state.pane.querySelectorAll("[data-line]")) {
    const at = Number(one.dataset.line);
    if (at <= line && at >= bestLine) {
      best = one;
      bestLine = at;
    }
  }
  state.pane.querySelectorAll(".here").forEach((one) => one !== best && one.classList.remove("here"));
  best?.classList.add("here");
}

// The nodes of the diagram the cursor's line `line` names.
function markNodes(line) {
  const text = line < state.session.doc.count ? state.session.doc.line(line) : "";
  for (const node of state.pane.querySelectorAll(".mm-node-group")) {
    const id = node.dataset.id ?? "";
    const named = id && new RegExp(`(^|[^\\w-])${id.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")}($|[^\\w-])`).test(text);
    node.classList.toggle("here", Boolean(named));
  }
}

// Marks in the preview the place the cursor stands in the text, and where `reveal` brings it into
// sight in a page or the pages that scroll on their own.
function mark(reveal) {
  const view = editing().editor;
  if (!view || view.s !== state.session || state.pane.hidden) {
    return;
  }
  const line = view.head()?.line ?? null;
  if (line === null) {
    return;
  }
  if (state.kind === "markdown") {
    markBlock(line);
  } else if (state.kind === "mermaid") {
    markNodes(line);
  } else if (state.kind === "latex") {
    markTex(line, reveal);
  } else {
    state.frame?.mark(line, reveal);
  }
}

// The line the top of the preview stands at, between the two blocks it stands between.
function lineAtPreview() {
  const blocks = [...state.pane.children].filter((one) => one.dataset.line !== undefined);
  const top = state.pane.scrollTop;
  let at = 0;
  while (at + 1 < blocks.length && blocks[at + 1].offsetTop <= top) {
    at += 1;
  }
  const here = blocks[at];
  const next = blocks[at + 1];
  if (!here) {
    return 0;
  }
  const line = Number(here.dataset.line);
  if (!next) {
    return line;
  }
  const share = Math.max(0, Math.min(1, (top - here.offsetTop) / Math.max(1, next.offsetTop - here.offsetTop)));
  return line + share * (Number(next.dataset.line) - line);
}

// Scrolls the preview to the line `line`, between the two blocks it falls between.
function previewToLine(line) {
  const blocks = [...state.pane.children].filter((one) => one.dataset.line !== undefined);
  let at = 0;
  while (at + 1 < blocks.length && Number(blocks[at + 1].dataset.line) <= line) {
    at += 1;
  }
  const here = blocks[at];
  const next = blocks[at + 1];
  if (!here) {
    return;
  }
  const from = Number(here.dataset.line);
  const share = next ? Math.max(0, Math.min(1, (line - from) / Math.max(1, Number(next.dataset.line) - from))) : 0;
  state.pane.scrollTop = here.offsetTop + share * ((next?.offsetTop ?? here.offsetTop) - here.offsetTop);
  state.paneSet = state.pane.scrollTop;
}

// The line the top of the editor stands at.
function lineAtEditor(view) {
  const rows = view.rows();
  const row = view.scrollY() / lineHeight();
  const whole = Math.floor(row);
  return view.s.base + rows.lineOf(whole) + (row - whole);
}

// Whether a scroll that lands at `top` was made here, to `set`: such a scroll leads nothing.
const ours = (top, set) => set !== null && Math.abs(top - set) <= 1;

function followEditor() {
  const view = editing().editor;
  if (!view || view.s !== state.session || state.pane.hidden || state.kind !== "markdown") {
    return;
  }
  if (ours(view.scroller.scrollTop, state.editorSet)) {
    state.editorSet = null;
    return;
  }
  previewToLine(lineAtEditor(view));
}

function followPreview() {
  const view = editing().editor;
  if (!view || view.s !== state.session || previewMode() !== "beside" || state.kind !== "markdown") {
    return;
  }
  if (ours(state.pane.scrollTop, state.paneSet)) {
    state.paneSet = null;
    return;
  }
  const line = lineAtPreview() - view.s.base;
  const whole = Math.floor(line);
  view.place((view.rows().rowOf(Math.max(0, whole)) + (line - whole)) * lineHeight());
  state.editorSet = view.scroller.scrollTop;
}

// Takes the editor to line `line` of the file `file`, the one shown or another of the tree.
function goTo(file, line) {
  if (file === state.path) {
    jumpTo(line);
  } else {
    openAt(file, line).catch((error) => say(String(error), { failed: true }));
  }
}

// Follows a link of the preview: to a place in it, to another file of the tree, or out to the browser.
function follow(given) {
  const href = resolveFrom(state.path ?? "")(given);
  if (href.startsWith("#")) {
    const doc = state.kind === "markdown" ? state.pane : state.frame?.page();
    doc?.querySelector(`[id="${CSS.escape(href.slice(1))}"]`)?.scrollIntoView({ block: "start" });
  } else if (href.startsWith(VIEW)) {
    openFile(decodeURIComponent(href.slice(VIEW.length).split("#")[0])).catch((error) => say(String(error), { failed: true }));
  } else {
    invoke("link_open", { url: href }).catch((error) => say(String(error), { failed: true }));
  }
}

// Shows the preview as the reader chose it, for the file in the editor where it has one.
export function apply() {
  const desk = document.querySelector("#mode-edit .desk");
  const { editor, active } = editing();
  const kind = editor ? kindOf(active) : null;
  const mode = kind ? previewMode() : "off";
  desk.dataset.preview = mode;
  state.pane.hidden = mode === "off";
  state.pane.style.top = `${document.getElementById("tabs").offsetHeight}px`;
  window.clearInterval(state.timer);
  if (mode === "off") {
    if (state.kind === "latex") {
      hideTex();
    }
    Object.assign(state, { session: null, path: null, kind: null });
    delete state.pane.dataset.kind;
    return;
  }
  // A stylesheet the page shown links stays under that page, swapped into it as it is typed.
  const under = kind === "css" && state.kind === "html" && state.linked.has(active) && editing().sessionOf(state.path) === state.session;
  if (!under && (state.session !== editor.s || state.path !== active || state.kind !== kind)) {
    if (state.kind === "latex") {
      hideTex();
    }
    Object.assign(state, { session: editor.s, path: active, kind, blocks: [], seen: null, diagram: null, sheets: new Map(), linked: new Set(), last: null });
    state.pane.dataset.kind = kind;
    state.pane.replaceChildren();
    window.cancelIdleCallback?.(state.idle);
    state.idle = 0;
    state.pane.scrollTop = 0;
    state.frame = new PageFrame(state.pane, { follow });
    if (kind === "latex") {
      state.seen = editor.s.doc.id;
      showTex(state.pane, editor.s, active, goTo);
    } else {
      render();
    }
  }
  if (state.scroller !== editor.scroller) {
    state.scroller?.removeEventListener("scroll", followEditor);
    state.scroller = editor.scroller;
    state.scroller.addEventListener("scroll", followEditor, { passive: true });
  }
  state.timer = window.setInterval(schedule, LOOK);
  requestAnimationFrame(followEditor);
}

function onLink(event) {
  const link = event.target.closest("a[href]");
  if (!link || state.kind !== "markdown") {
    return;
  }
  event.preventDefault();
  follow(link.getAttribute("href"));
}

export function startPreview() {
  state.pane = document.getElementById("preview");
  state.pane.addEventListener("scroll", followPreview, { passive: true });
  state.pane.addEventListener("click", onLink);
  onShown(() => apply());
  onEdit(edited);
  onCursorMoved(() => mark(true));
  apply();
}

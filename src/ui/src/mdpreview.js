// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The Markdown preview: the Markdown file in the editor shown as it reads, beside its text or alone,
// as the reader chooses once for every Markdown file (View, Markdown Preview, Ctrl+K V).
//
// Each block of the preview is tied to the line it came from, and the text and the preview scroll by
// those lines: the one the reader scrolls leads, and the other follows to the same place between two
// blocks, and a scroll made to follow leads nothing in turn. As the text changes, the preview is read again in time the page has free, never on the
// editor's: a block whose HTML is as it was stays as it was drawn, one whose lines only moved is
// told its new lines, and only a block that changed is drawn again. An image by a path relative to
// the file is read from the file's own folder; a link to another file of the tree opens it, a link
// to a place in the preview goes there, and a link out opens in the browser.

import { invoke } from "./bridge.js";
import { lineHeight } from "./editor/view.js";
import { editing, onShown, openFile } from "./edit.js";
import { markdownBlocks } from "./markdown.js";
import { say } from "./statusbar.js";

const KEY = "orior.markdown.preview";

// How often the text is looked at for a change while the preview shows, in milliseconds, and the
// longest a change waits for free time before it is drawn anyway.
const LOOK = 200;
const LATEST = 600;

const state = { pane: null, session: null, path: null, blocks: [], seen: null, timer: 0, idle: 0, paneSet: null, editorSet: null, scroller: null };

export const isMarkdown = (path) => /\.(md|markdown|mdown|mkd|mdx)$/i.test(path ?? "");

// How the reader sees a Markdown file: "off", "beside" its text, or "alone".
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

const LINES = / data-line="\d+"/g;

function element(html) {
  const holder = document.createElement("template");
  holder.innerHTML = html;
  return holder.content.firstElementChild ?? document.createElement("div");
}

// Draws the text as it stands, block by block, keeping each block that has not changed.
export function render() {
  const s = state.session;
  if (!s || !state.pane) {
    return;
  }
  state.seen = s.doc.id;
  const fresh = markdownBlocks(s.doc.text(), resolveFrom(state.path ?? ""));
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

// What the last drawing did, for the tests: how many blocks it kept, told new lines, and drew.
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
  if (!view || view.s !== state.session || state.pane.hidden) {
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
  if (!view || view.s !== state.session || previewMode() !== "beside") {
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

// Shows the preview as the reader chose it, for the file in the editor where it is Markdown.
export function apply() {
  const desk = document.querySelector("#mode-edit .desk");
  const { editor, active } = editing();
  const mode = editor && isMarkdown(active) ? previewMode() : "off";
  desk.dataset.preview = mode;
  state.pane.hidden = mode === "off";
  state.pane.style.top = `${document.getElementById("tabs").offsetHeight}px`;
  window.clearInterval(state.timer);
  if (mode === "off") {
    return;
  }
  if (state.session !== editor.s || state.path !== active) {
    state.session = editor.s;
    state.path = active;
    state.blocks = [];
    state.seen = null;
    state.pane.replaceChildren();
    render();
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
  if (!link) {
    return;
  }
  event.preventDefault();
  const href = resolveFrom(state.path ?? "")(link.getAttribute("href"));
  if (href.startsWith("#")) {
    state.pane.querySelector(`[id="${CSS.escape(href.slice(1))}"]`)?.scrollIntoView({ block: "start" });
  } else if (href.startsWith(VIEW)) {
    const path = decodeURIComponent(href.slice(VIEW.length).split("#")[0]);
    openFile(path).catch((error) => say(String(error), { failed: true }));
  } else {
    invoke("link_open", { url: href }).catch((error) => say(String(error), { failed: true }));
  }
}

export function startPreview() {
  state.pane = document.getElementById("md-preview");
  state.pane.addEventListener("scroll", followPreview, { passive: true });
  state.pane.addEventListener("click", onLink);
  onShown(() => apply());
  apply();
}

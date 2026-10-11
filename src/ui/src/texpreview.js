// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A LaTeX file's pages in the live preview, made by the TeX the toolchains give from the text as it is
// typed, saved or not (latex.rs). One make runs at a time: a change made while one runs is made as
// soon as it ends, and the pages last made stay until the next are drawn. A make that fails leaves
// them and lists its errors over them, each taking the editor to its line. The boxes SyncTeX tells
// of tie the text to the pages both ways: the boxes the cursor's line was set in are marked on the
// pages, and a press on a page takes the editor to the line set where it was pressed.

import { invoke } from "./bridge.js";

const SVG = "http://www.w3.org/2000/svg";

const state = { holder: null, session: null, path: null, making: false, again: false, made: null, line: null, goTo: null, last: null };

// What the last make gave, for the tests.
export const lastMade = () => state.made;

// Shows the pages of the LaTeX file `path`, whose text is the session `session`, in `holder`;
// `goTo(path, line)` takes the editor to a line of a file.
export function showTex(holder, session, path, goTo) {
  const fresh = state.path !== path || state.holder !== holder;
  Object.assign(state, { holder, session, path, goTo });
  if (fresh) {
    state.made = null;
    state.last = null;
    holder.replaceChildren();
    holder.addEventListener("click", pressed);
  }
  texChanged();
}

// Makes the text as it now stands, at once where no make runs and as soon as the one running ends
// where one does.
export async function texChanged() {
  if (!state.session || !state.holder) {
    return;
  }
  if (!state.making && state.made?.id === state.session.doc.id) {
    return;
  }
  if (state.making) {
    state.again = true;
    return;
  }
  state.making = true;
  state.again = false;
  const { session, path } = state;
  const id = session.doc.id;
  state.holder.dataset.making = "true";
  try {
    const made = await invoke("latex_make", { path, text: session.doc.text() });
    if (state.path === path) {
      state.made = { ...made, id };
      if (made.pages.length) {
        state.last = made;
        drawPages(made);
      }
      drawErrors(made.errors);
      markTex(state.line, false);
    }
  } catch (error) {
    if (state.path === path) {
      state.made = { pages: [], boxes: [], errors: [{ file: path, line: null, text: String(error) }], id };
      drawErrors(state.made.errors);
    }
  } finally {
    state.making = false;
    if (state.holder) {
      delete state.holder.dataset.making;
    }
    if (state.session === session && (state.again || session.doc.id !== id)) {
      texChanged();
    }
  }
}

function drawPages(made) {
  const pages = document.createElement("div");
  pages.className = "tex-pages";
  pages.innerHTML =
    `<svg class="tex-defs" aria-hidden="true" width="0" height="0"><defs>${made.defs}</defs></svg>` +
    made.pages.map((page, at) => `<svg class="tex-page" data-page="${at + 1}" viewBox="0 0 ${page.width} ${page.height}" role="img" aria-label="Page ${at + 1}">${page.svg}<g class="tex-marks"></g></svg>`).join("");
  const was = state.holder.querySelector(".tex-pages");
  if (was) {
    was.replaceWith(pages);
  } else {
    state.holder.append(pages);
  }
}

function drawErrors(errors) {
  let list = state.holder.querySelector(".tex-errors");
  if (!errors.length) {
    list?.remove();
    return;
  }
  if (!list) {
    list = document.createElement("ol");
    list.className = "tex-errors";
    state.holder.prepend(list);
  }
  list.replaceChildren(
    ...errors.map((error) => {
      const item = document.createElement("li");
      const button = document.createElement("button");
      button.type = "button";
      button.className = "tex-error";
      const where = [error.file === state.path ? "" : error.file, error.line ? `${error.line}` : ""].filter(Boolean).join(":");
      button.textContent = where ? `${where}: ${error.text}` : error.text;
      button.disabled = !error.line;
      button.addEventListener("click", (event) => {
        event.stopPropagation();
        state.goTo(error.file, error.line - 1);
      });
      item.append(button);
      return item;
    })
  );
}

// The boxes the line `line` of the file shown, counted from 0, was set in, or, where none holds it,
// those of the nearest line before it that some box holds.
export function boxesOf(boxes, path, line) {
  const own = boxes.filter((box) => box.file === path);
  const holds = (box, at) => box.lines.some(([, one]) => one === at);
  const exact = own.filter((box) => holds(box, line + 1));
  if (exact.length) {
    return exact;
  }
  const before = Math.max(-1, ...own.flatMap((box) => box.lines.map(([, one]) => one).filter((one) => one <= line + 1)));
  return before < 0 ? [] : own.filter((box) => holds(box, before));
}

// Marks on the pages the boxes the cursor's line `line` was set in, and scrolls the first into sight
// where `reveal` and it is out of it.
export function markTex(line, reveal) {
  state.line = line;
  const pages = state.holder?.querySelector(".tex-pages");
  if (!pages) {
    return;
  }
  pages.querySelectorAll(".tex-here").forEach((one) => one.remove());
  if (line === null || !state.last) {
    return;
  }
  const marks = boxesOf(state.last.boxes, state.path, line).map((box) => {
    const rect = document.createElementNS(SVG, "rect");
    rect.setAttribute("class", "tex-here");
    rect.setAttribute("x", box.x - 2);
    rect.setAttribute("y", box.y - 1);
    rect.setAttribute("width", box.w + 4);
    rect.setAttribute("height", Math.max(box.h, 8) + 2);
    pages.querySelector(`.tex-page[data-page="${box.page}"] .tex-marks`)?.append(rect);
    return rect;
  });
  const first = marks.find((one) => one.isConnected);
  if (first && reveal) {
    const box = first.getBoundingClientRect();
    const view = state.holder.getBoundingClientRect();
    if (box.top < view.top || box.bottom > view.bottom) {
      state.holder.scrollTop += box.top - view.top - view.height / 3;
    }
  }
}

// The file and line, counted from 0, set at the point (x, y) of page `page`, in its points: in the
// smallest box that holds the point, else the box nearest it up or down, the line whose part of the
// box starts nearest before x.
export function lineAt(boxes, page, x, y) {
  const on = boxes.filter((box) => box.page === page && box.lines.length);
  const holding = on.filter((box) => x >= box.x - 1 && x <= box.x + box.w + 1 && y >= box.y - 2 && y <= box.y + box.h + 2).sort((a, b) => a.w * a.h - b.w * b.h);
  const box = holding[0] ?? on.sort((a, b) => Math.abs(y - (a.y + a.h / 2)) - Math.abs(y - (b.y + b.h / 2)))[0];
  if (!box) {
    return null;
  }
  const [, line] = box.lines.filter(([at]) => at <= x).at(-1) ?? box.lines[0];
  return { file: box.file, line: line - 1 };
}

function pressed(event) {
  const page = event.target.closest?.(".tex-page");
  if (!page || !state.last) {
    return;
  }
  const box = page.getBoundingClientRect();
  const view = page.viewBox.baseVal;
  const found = lineAt(state.last.boxes, Number(page.dataset.page), ((event.clientX - box.left) / box.width) * view.width, ((event.clientY - box.top) / box.height) * view.height);
  if (found) {
    state.goTo(found.file, found.line);
  }
}

// Lets go of the file shown, its make left to end unseen.
export function hideTex() {
  state.holder?.removeEventListener("click", pressed);
  Object.assign(state, { holder: null, session: null, path: null, made: null, last: null });
}

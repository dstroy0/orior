// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { structureOf } from "../outline.js";
import { icon } from "../icons.js";
import { act, element, guides, paneOpen, state } from "../explorer.js";

// Outline: what the open file declares, a row a symbol, the one the cursor is in marked; and the
// regions its comments mark, each a row that holds the symbols inside it, opened and closed by its
// arrow. Pinned, the pane shows the structure of the file pinned while another is open, a press on a
// symbol going to that file, until it is unpinned or the file closes.

const KIND_MARKS = { function: ["ƒ", 5], method: ["ƒ", 5], class: ["◇", 3], module: ["{}", 4], constant: ["▪", 4], field: ["▪", 6], heading: ["#", 4], label: ["›", 6], region: ["▤", 2] };

// The Outline holds the rows in sight and OUTLINE_MARGIN more on either side, space above and below
// standing for the rest, each row made as it first comes near: a file of many thousands of symbols
// would otherwise hold a row of each, which the page tracks in every frame. A row already in place
// stays where it is as the rows round it change, and keeps the keys where it has them.
const OUTLINE_ROW = 22;
const OUTLINE_MARGIN = 40;

export function drawOutline(session) {
  const body = document.getElementById("outline");
  const pinned = state.outline.pinned;
  if (pinned && !state.hooks?.sessionOf?.(pinned.path)) {
    state.outline.pinned = null;
  }
  const shown = state.outline.pinned ? state.hooks.sessionOf(state.outline.pinned.path) : session;
  state.outline.session = shown;
  const head = document.querySelector('[data-pane="outline"] .pane-head');
  if (head) {
    head.textContent = state.outline.pinned ? `Outline of ${state.outline.pinned.path.split("/").pop()}` : "Outline";
  }
  if (!paneOpen("outline")) {
    return;
  }
  const symbols = shown ? structureOf(shown.language?.id, shown.doc) : [];
  const closed = state.outline.closed;
  const hidden = (symbol) => symbols.some((region) => region.kind === "region" && closed.has(`${region.line}:${region.name}`) && region.line < symbol.line && symbol.line <= region.end);
  state.outline.symbols = symbols.filter((symbol) => !hidden(symbol));
  state.outline.shownIn = shown;
  state.outline.rows = new Map();
  state.outline.lit = -1;
  if (!state.outline.above) {
    state.outline.above = element("div", { className: "outline-space" });
    state.outline.below = element("div", { className: "outline-space" });
    body.addEventListener("scroll", () => placeOutline(), { passive: true });
  }
  body.replaceChildren(state.outline.above, state.outline.below);
  lightOutline(state.hooks.cursorLine?.() ?? 0);
  placeOutline();
}

// The row of the symbol at `index` of those the Outline shows, made the first time it is asked for.
function outlineRow(index) {
  const made = state.outline.rows.get(index);
  if (made) {
    return made;
  }
  const symbol = state.outline.symbols[index];
  const shown = state.outline.shownIn;
  const closed = state.outline.closed;
  const [mark, color] = KIND_MARKS[symbol.kind] ?? ["▪", 7];
  const key = `${symbol.line}:${symbol.name}`;
  const region = symbol.kind === "region";
  const row = element("button", { className: region ? "sym dir sym-region" : "sym", type: "button", title: `${symbol.name}, line ${shown.base + symbol.line + 1}` });
  row.dataset.key = key;
  row.dataset.index = String(index);
  row.dataset.depth = String(symbol.depth);
  row.append(...guides(symbol.depth));
  if (region) {
    row.setAttribute("aria-expanded", String(!closed.has(key)));
    row.append(element("span", { className: "twisty" }));
  }
  const icon = element("span", { className: "icon", textContent: mark });
  icon.style.color = `var(--t${color})`;
  row.append(icon, element("span", { className: "name", textContent: symbol.name }));
  if (index === state.outline.lit) {
    row.setAttribute("aria-current", "true");
  }
  row.addEventListener("click", (event) => {
    if (region && (!event.isTrusted || event.target.closest(".twisty"))) {
      if (closed.has(key)) {
        closed.delete(key);
      } else {
        closed.add(key);
      }
      drawOutline(state.outline.session);
      document.querySelector(`#outline [data-key="${CSS.escape(key)}"]`)?.focus();
      return;
    }
    if (state.outline.pinned) {
      state.hooks.openAt(state.outline.pinned.path, shown.base + symbol.line, 0);
    } else {
      state.hooks.goTo(symbol.line);
    }
  });
  state.outline.rows.set(index, row);
  return row;
}

// Puts in the Outline the rows in sight and the margin round them, and takes out the rest.
function placeOutline() {
  const body = document.getElementById("outline");
  const { above, below, symbols } = state.outline;
  if (!above || !body.contains(above)) {
    return;
  }
  const count = symbols.length;
  const top = Math.floor(body.scrollTop / OUTLINE_ROW);
  const first = Math.max(0, top - OUTLINE_MARGIN);
  const last = Math.min(count, top + Math.ceil(body.clientHeight / OUTLINE_ROW) + OUTLINE_MARGIN);
  for (const row of [...body.children]) {
    const index = row.dataset.index === undefined ? -1 : Number(row.dataset.index);
    if (index >= 0 && (index < first || index >= last)) {
      row.remove();
    }
  }
  let at = above.nextSibling;
  for (let index = first; index < last; index += 1) {
    const row = outlineRow(index);
    if (row === at) {
      at = at.nextSibling;
    } else {
      body.insertBefore(row, at);
    }
  }
  above.style.height = `${first * OUTLINE_ROW}px`;
  below.style.height = `${(count - last) * OUTLINE_ROW}px`;
}

// Pins the Outline to the file `path` shows, or, given none, unpins it to follow the file shown.
export function pinOutline(path) {
  state.outline.pinned = path ? { path } : null;
  drawOutline(state.hooks.sessionOf?.(state.hooks.activePath?.()) ?? null);
}

export function outlinePinned() {
  return state.outline.pinned?.path ?? null;
}

// The Outline's menu: pinning it to the file shown, or unpinning it.
export function outlineItems() {
  const active = state.hooks.activePath?.();
  const pinned = state.outline.pinned;
  return [
    pinned ? act("outline-pin", ["none"], { label: `Unpin ${pinned.path.split("/").pop()}` }) : act("outline-pin", [active ?? "none"], { label: active ? `Pin ${active.split("/").pop()}` : "Pin the File Shown", disabled: !active }),
  ];
}

// Marks the symbol the cursor's line falls in: the last one that starts at or above it, where the
// Outline shows the file the cursor is in, the Outline scrolled to it where it is out of sight.
export function lightOutline(line) {
  const was = state.outline.rows.get(state.outline.lit);
  was?.removeAttribute("aria-current");
  state.outline.lit = -1;
  if (state.outline.pinned && state.outline.pinned.path !== state.hooks.activePath?.()) {
    return;
  }
  const symbols = state.outline.symbols;
  let low = 0;
  let high = symbols.length;
  while (low < high) {
    const middle = (low + high) >> 1;
    if (symbols[middle].line <= line) {
      low = middle + 1;
    } else {
      high = middle;
    }
  }
  const lit = low - 1;
  state.outline.lit = lit;
  if (lit < 0) {
    return;
  }
  state.outline.rows.get(lit)?.setAttribute("aria-current", "true");
  const body = document.getElementById("outline");
  if (lit * OUTLINE_ROW < body.scrollTop) {
    body.scrollTop = lit * OUTLINE_ROW;
  } else if ((lit + 1) * OUTLINE_ROW > body.scrollTop + body.clientHeight && body.clientHeight) {
    body.scrollTop = (lit + 1) * OUTLINE_ROW - body.clientHeight;
  }
  placeOutline();
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The rows in sight, in the explorer's panes and the lists beside a view. A row out of sight is
// neither laid out nor painted, as style.css sets each row's content-visibility, keeping the height
// it had. As a list scrolls, the rows past its edge the way it heads, as many as its speed carries it
// over two frames' drawing and never fewer than LEAST, are drawn ahead, marked `ahead`; those it has
// left are drawn only as they come into sight again.

import { Ahead } from "./ahead.js";
import { frameMs } from "./motion.js";

// The rows of a list, the height of most of them, and the fewest drawn ahead.
export const ROWS = ".node, .open-row, .sym, .commit, .item, .search-hit, .problem";
const ROW = 22;
const LEAST = 8;

const lists = new Map();

// The row at the edge of `list` its scroll heads into, and the rows past it that way, `count` of them.
function past(list, heading, count) {
  const box = list.getBoundingClientRect();
  const y = heading > 0 ? Math.min(innerHeight - 1, box.bottom - 2) : Math.max(0, box.top + 2);
  const edge = document.elementFromPoint(box.left + Math.min(40, box.width / 2), y)?.closest(ROWS);
  if (!edge || !list.contains(edge)) {
    return [];
  }
  const walker = document.createTreeWalker(list, NodeFilter.SHOW_ELEMENT, { acceptNode: (node) => (node.matches(ROWS) ? NodeFilter.FILTER_ACCEPT : NodeFilter.FILTER_SKIP) });
  walker.currentNode = edge;
  const found = [];
  while (found.length < count && (heading > 0 ? walker.nextNode() : walker.previousNode())) {
    found.push(walker.currentNode);
  }
  return found;
}

function mark(list) {
  const held = lists.get(list);
  held.frame = 0;
  held.ahead.moved(Math.floor(list.scrollTop / ROW));
  held.ahead.read(frameMs());
  const rows = past(list, held.ahead.heading, held.ahead.reach());
  held.marked.forEach((row) => !rows.includes(row) && row.classList.remove("ahead"));
  rows.forEach((row) => row.classList.add("ahead"));
  held.marked = rows;
}

// Draws the rows `list` scrolls toward ahead of it, from now on.
export function drawAhead(list) {
  if (!list || lists.has(list)) {
    return;
  }
  lists.set(list, { ahead: new Ahead(LEAST), marked: [], frame: 0 });
  list.addEventListener(
    "scroll",
    () => {
      const held = lists.get(list);
      held.frame ||= requestAnimationFrame(() => mark(list));
    },
    { passive: true },
  );
}

// The rows of `list` marked to be drawn ahead, for the tests.
export const aheadOf = (list) => lists.get(list)?.marked ?? [];

export function startSight() {
  document.querySelectorAll(".pane-body, #jobs").forEach(drawAhead);
}

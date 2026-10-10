// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The keys of a list beside a view, the jobs or the files: Up and Down step from row to row, Home and
// End go to the first and the last, Right opens a closed group or folder and Left closes an open one
// or steps out to the row that holds it. Down in the list's search field steps into the list, and Up
// from its first row steps back. A row is a button or a group's summary that is in sight; the rows of
// a closed group are not.
//
// A row says how deep it sits in data-depth, and a folder row says whether it is open in
// aria-expanded. A group is a details element, open or closed as it is. A list drawn again keeps the
// focus on the row it was on, by the row's data-key.
//
// Letters typed while a row holds the keys find the rows in sight whose names match them, as Go to
// File matches a name: the first match from that row on takes the keys, each match's letters are
// marked, Up and Down step from match to match, and Backspace takes the last letter back. Escape, a
// click in the list, a key that opens or closes a row, or the keys leaving the list ends the search.
// What was typed shows over the list's top right.

import { fuzzy } from "./fuzzy.js";

const ROWS = "button, summary";

const inSight = (row) => row.offsetParent !== null;

function rowsOf(list) {
  return [...list.querySelectorAll(ROWS)].filter(inSight);
}

const depthOf = (row) => Number(row.dataset.depth ?? row.closest("details")?.dataset.depth ?? 0);

// The row that holds this one: a group's summary, or the nearest folder above it and less deep.
function holderOf(row, rows) {
  const group = row.tagName === "SUMMARY" ? row.parentElement.parentElement.closest("details") : row.closest("details");
  if (group) {
    return group.querySelector(":scope > summary");
  }
  const depth = depthOf(row);
  const at = rows.indexOf(row);
  for (let back = at - 1; back >= 0; back -= 1) {
    if (depthOf(rows[back]) < depth) {
      return rows[back];
    }
  }
  return null;
}

// Whether a row opens: a group's summary, or a folder. Null for a row that does not.
function openOf(row) {
  if (row.tagName === "SUMMARY") {
    return row.parentElement.open;
  }
  const expanded = row.getAttribute("aria-expanded");
  return expanded === null ? null : expanded === "true";
}

function setOpen(row, open) {
  if (row.tagName === "SUMMARY") {
    row.parentElement.open = open;
  } else {
    row.click();
  }
}

// The text of a row's name: its name's own element where it has one, else the row's text.
const nameOf = (row) => row.querySelector(".name") ?? row;

// The search typed in a list: the letters, the label that shows them, and the rows that match.
function typedSearch(list) {
  const shown = document.createElement("div");
  shown.className = "list-typed";
  shown.hidden = true;
  document.body.append(shown);
  const held = { text: "", matches: [] };
  const mark = () => {
    const ranges = [];
    for (const { row, hits } of held.matches) {
      const node = nameOf(row).firstChild;
      if (node?.nodeType !== Node.TEXT_NODE) {
        continue;
      }
      for (const at of hits) {
        const range = new Range();
        range.setStart(node, at);
        range.setEnd(node, at + 1);
        ranges.push(range);
      }
    }
    CSS.highlights?.set("list-typed", new Highlight(...ranges));
  };
  return {
    get text() {
      return held.text;
    },
    // The matches for what is typed now, the row from `from` on that takes the keys.
    find(text, from) {
      held.text = text;
      // In a list whose rows name what they stand for, as the explorer's files and folders do, the
      // headings of its groups are no matches.
      const all = rowsOf(list);
      const named = all.filter((row) => row.querySelector(".name"));
      const rows = named.length ? named : all;
      held.matches = rows.map((row) => ({ row, found: fuzzy(text, nameOf(row).textContent) })).filter((one) => one.found).map(({ row, found }) => ({ row, hits: found.hits }));
      const box = list.getBoundingClientRect();
      shown.textContent = text;
      shown.classList.toggle("none", !held.matches.length);
      shown.style.top = `${Math.round(box.top + 4)}px`;
      shown.style.right = `${Math.round(window.innerWidth - box.right + 8)}px`;
      shown.hidden = false;
      mark();
      const start = Math.max(0, rows.indexOf(from));
      const first = held.matches.find((one) => rows.indexOf(one.row) >= start) ?? held.matches[0];
      return first?.row ?? null;
    },
    // The match `by` matches on from `row`, the last or the first where there is no further one.
    step(row, by) {
      const at = held.matches.findIndex((one) => one.row === row);
      const next = held.matches[Math.max(0, Math.min(held.matches.length - 1, at + by))];
      return next?.row ?? row;
    },
    end() {
      if (held.text) {
        held.text = "";
        held.matches = [];
        shown.hidden = true;
        CSS.highlights?.delete("list-typed");
      }
    },
  };
}

export function keepListKeys(list, search) {
  const typed = typedSearch(list);
  list.addEventListener("focusout", (event) => !list.contains(event.relatedTarget) && typed.end());
  list.addEventListener("pointerdown", () => typed.end());
  list.addEventListener("keydown", (event) => {
    if (event.altKey || event.ctrlKey || event.metaKey) {
      return;
    }
    const rows = rowsOf(list);
    const row = event.target.closest(ROWS);
    const at = rows.indexOf(row);
    if (at < 0) {
      return;
    }
    const letter = event.key.length === 1 && (event.key !== " " || typed.text);
    if (letter || (typed.text && ["Backspace", "Escape", "ArrowDown", "ArrowUp"].includes(event.key))) {
      let next = row;
      if (letter) {
        next = typed.find(typed.text + event.key, row) ?? row;
      } else if (event.key === "Backspace") {
        const left = typed.text.slice(0, -1);
        if (left) {
          next = typed.find(left, row) ?? row;
        } else {
          typed.end();
        }
      } else if (event.key === "Escape") {
        typed.end();
        event.stopPropagation();
      } else {
        next = typed.step(row, event.key === "ArrowDown" ? 1 : -1);
      }
      event.preventDefault();
      next.focus();
      next.scrollIntoView({ block: "nearest" });
      return;
    }
    typed.end();
    let next = null;
    if (event.key === "ArrowDown") {
      next = rows[at + 1] ?? row;
    } else if (event.key === "ArrowUp") {
      next = at === 0 ? search : rows[at - 1];
    } else if (event.key === "Home") {
      next = rows[0];
    } else if (event.key === "End") {
      next = rows.at(-1);
    } else if (event.key === "ArrowRight") {
      const open = openOf(row);
      if (open === false) {
        setOpen(row, true);
      } else if (open) {
        next = rows[at + 1] ?? row;
      }
    } else if (event.key === "ArrowLeft") {
      if (openOf(row)) {
        setOpen(row, false);
      } else {
        next = holderOf(row, rows) ?? row;
      }
    } else {
      return;
    }
    event.preventDefault();
    next?.focus();
    next?.scrollIntoView({ block: "nearest" });
  });
  search.addEventListener("keydown", (event) => {
    if (event.key === "ArrowDown") {
      event.preventDefault();
      rowsOf(list)[0]?.focus();
    }
  });
}

// The key of the row that has the focus, where it is in the list, and null otherwise. A row's key is
// its data-key, the same each time the list is drawn.
export function focusedKey(list) {
  return list.contains(document.activeElement) ? (document.activeElement.dataset.key ?? null) : null;
}

// Focuses the row with this key after the list is drawn again, where the focus was in it before.
export function refocus(list, key) {
  if (key !== null) {
    [...list.querySelectorAll(ROWS)].find((row) => row.dataset.key === key)?.focus();
  }
}

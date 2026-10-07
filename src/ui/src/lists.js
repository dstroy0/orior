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

export function keepListKeys(list, search) {
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

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// What folds, read from indentation: a line opens a region when the next line that is not blank is
// indented deeper, and the region runs to the last line before one indented no deeper than it.
// A closing brace at the opening line's depth stays outside the region and shows when folded.

// The width a line's leading white space takes, or -1 for a blank line.
export function indentOf(text, size) {
  let width = 0;
  for (const char of text) {
    if (char === " ") {
      width += 1;
    } else if (char === "\t") {
      width += size - (width % size);
    } else {
      return width;
    }
  }
  return -1;
}

// How far ahead a line looks for the line that tells whether it opens a region.
const AHEAD = 400;

// Whether a line opens a region, read from the lines after it alone.
export function opens(doc, line, size) {
  const own = indentOf(doc.line(line), size);
  if (own < 0) {
    return false;
  }
  for (let at = line + 1; at < Math.min(doc.count, line + AHEAD); at += 1) {
    const next = indentOf(doc.line(at), size);
    if (next >= 0) {
      return next > own;
    }
  }
  return false;
}

// The last line of the region a line opens, or -1 where it opens none.
export function regionEnd(doc, line, size) {
  if (!opens(doc, line, size)) {
    return -1;
  }
  const own = indentOf(doc.line(line), size);
  let last = line;
  for (let at = line + 1; at < doc.count; at += 1) {
    const next = indentOf(doc.line(at), size);
    if (next < 0) {
      continue;
    }
    if (next <= own) {
      break;
    }
    last = at;
  }
  return last;
}

// Every region as a map from its first line to its last.
export function regions(doc, size) {
  const found = new Map();
  const open = [];
  let lastFilled = -1;
  const close = (until) => {
    const top = open.pop();
    if (lastFilled > top.line && until > top.line) {
      found.set(top.line, lastFilled);
    }
  };
  for (let line = 0; line < doc.count; line += 1) {
    const indent = indentOf(doc.line(line), size);
    if (indent < 0) {
      continue;
    }
    while (open.length && open.at(-1).indent >= indent) {
      close(line);
    }
    open.push({ line, indent });
    lastFilled = line;
  }
  while (open.length) {
    close(doc.count);
  }
  return found;
}

// The rows a text shows once some regions are folded: which line each row shows, and which row
// shows each line. A line inside a folded region has the row of the line that opens it.
export class Rows {
  constructor(count, hidden) {
    this.count = count;
    if (!hidden.length) {
      this.lines = null;
      this.size = count;
      return;
    }
    const lines = new Int32Array(count);
    const rows = new Int32Array(count);
    let size = 0;
    let at = 0;
    for (let line = 0; line < count; line += 1) {
      while (at < hidden.length && hidden[at][1] < line) {
        at += 1;
      }
      const inside = at < hidden.length && hidden[at][0] <= line && line <= hidden[at][1];
      if (inside) {
        rows[line] = size - 1;
      } else {
        lines[size] = line;
        rows[line] = size;
        size += 1;
      }
    }
    this.lines = lines.subarray(0, size);
    this.rows = rows;
    this.size = size;
  }

  lineOf(row) {
    const clamped = Math.max(0, Math.min(this.size - 1, row));
    return this.lines ? this.lines[clamped] : clamped;
  }

  rowOf(line) {
    const clamped = Math.max(0, Math.min(this.count - 1, line));
    return this.lines ? this.rows[clamped] : clamped;
  }
}

// The spans of lines the folded regions hide, merged and in order, from a map of each folded
// region's first line to its last.
export function hiddenSpans(folded) {
  const spans = [...folded]
    .map(([start, end]) => [start + 1, end])
    .filter(([first, last]) => last >= first)
    .sort((a, b) => a[0] - b[0]);
  const merged = [];
  for (const span of spans) {
    if (merged.length && span[0] <= merged.at(-1)[1] + 1) {
      merged.at(-1)[1] = Math.max(merged.at(-1)[1], span[1]);
    } else {
      merged.push([...span]);
    }
  }
  return merged;
}

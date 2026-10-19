// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// What folds, read from indentation: a line opens a region when the next line that is not blank is
// indented deeper, and the region runs to the last line before one indented no deeper than it.
// A closing brace at the opening line's depth stays outside the region; a folded region takes it
// in where its line holds closing brackets alone, and draws them on the region's first line.

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

// How far up from a line the lines that open the regions around it are looked for.
const REACH = 20000;

// The first lines of the regions a line stands inside, the outermost first and the `most` innermost
// at most, read from the lines above it alone: walking up, each line indented less than every line
// after it down to the line opens one. A blank line stands inside what the next line that is not
// blank stands inside.
export function openersOf(doc, line, size, most) {
  let depth = -1;
  for (let at = line; at < Math.min(doc.count, line + AHEAD) && depth < 0; at += 1) {
    depth = indentOf(doc.line(at), size);
  }
  const found = [];
  for (let at = line - 1; at >= Math.max(0, line - REACH) && depth > 0 && found.length < most; at -= 1) {
    const own = indentOf(doc.line(at), size);
    if (own >= 0 && own < depth) {
      found.push(at);
      depth = own;
    }
  }
  return found.reverse();
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

// The rows a text shows once some regions are folded and its long lines are wrapped: which line each
// row shows and the col it starts at, and which row shows each line first. A line inside a folded
// region has the row of the line that opens it, and a line hidden before every line shown has the
// first row. `breaksOf`, where given, gives a line shown the cols its rows after the first start
// at, in order, or null where it takes one row.
export class Rows {
  constructor(count, hidden, breaksOf = null) {
    this.count = count;
    if (!hidden.length && !breaksOf) {
      this.lines = null;
      this.starts = null;
      this.size = count;
      return;
    }
    const rows = new Int32Array(count);
    const shown = [];
    let size = 0;
    let at = 0;
    for (let line = 0; line < count; line += 1) {
      while (at < hidden.length && hidden[at][1] < line) {
        at += 1;
      }
      const inside = at < hidden.length && hidden[at][0] <= line && line <= hidden[at][1];
      if (inside) {
        rows[line] = Math.max(0, size - 1);
        continue;
      }
      const breaks = breaksOf?.(line) ?? null;
      rows[line] = size;
      shown.push(line, breaks);
      size += 1 + (breaks?.length ?? 0);
    }
    const lines = new Int32Array(size);
    const starts = new Int32Array(size);
    let row = 0;
    for (let index = 0; index < shown.length; index += 2) {
      const line = shown[index];
      lines[row] = line;
      row += 1;
      for (const col of shown[index + 1] ?? []) {
        lines[row] = line;
        starts[row] = col;
        row += 1;
      }
    }
    this.lines = lines;
    this.starts = starts;
    this.rows = rows;
    this.size = size;
  }

  lineOf(row) {
    const clamped = Math.max(0, Math.min(this.size - 1, row));
    return this.lines ? this.lines[clamped] : clamped;
  }

  // The first row that shows `line`.
  rowOf(line) {
    const clamped = Math.max(0, Math.min(this.count - 1, line));
    return this.lines ? this.rows[clamped] : clamped;
  }

  // The col of its line `row` starts at.
  startOf(row) {
    return this.starts ? this.starts[Math.max(0, Math.min(this.size - 1, row))] : 0;
  }

  // The col of its line `row` ends before, or -1 where it runs to the line's end.
  endOf(row) {
    if (!this.starts || row + 1 >= this.size || this.lines[row + 1] !== this.lines[row]) {
      return -1;
    }
    return this.starts[row + 1];
  }

  // The last row that shows `line`.
  lastRowOf(line) {
    return this.rowAt(line, Infinity);
  }

  // The row that shows the col `col` of `line`: a col where a row starts is on that row.
  rowAt(line, col) {
    let row = this.rowOf(line);
    if (!this.starts) {
      return row;
    }
    while (row + 1 < this.size && this.lines[row + 1] === this.lines[row] && this.starts[row + 1] <= col) {
      row += 1;
    }
    return row;
  }
}

// The cols a line's rows after the first start at, the line wrapped to `columns` columns: each row
// ends after the last space that fits in it, or where the columns run out where none does. `size`
// is a tab's width. Null where the line fits in one row.
export function wrapBreaks(text, columns, size) {
  if (text.length <= columns && !text.includes("\t")) {
    return null;
  }
  const breaks = [];
  let v = 0;
  let start = 0;
  let space = -1;
  for (let at = 0; at < text.length; at += 1) {
    const width = text[at] === "\t" ? size - (v % size) : 1;
    if (v + width > columns && at > start) {
      const cut = space > start ? space : at;
      breaks.push(cut);
      start = cut;
      space = -1;
      v = 0;
      for (let back = cut; back < at; back += 1) {
        v += text[back] === "\t" ? size - (v % size) : 1;
      }
    }
    v += width;
    if (text[at] === " " || text[at] === "\t") {
      space = at + 1;
    }
  }
  return breaks.length ? breaks : null;
}

// The spans of lines the folded regions hide, merged and in order, from a map of each folded
// region's first line to its last.
export function hiddenSpans(folded) {
  return joinSpans(
    [...folded].map(([start, end]) => [start + 1, end]),
    [],
  );
}

// Two lists of spans of hidden lines as one, merged and in order, each span its first and last line.
export function joinSpans(first, second) {
  const spans = [...first, ...second].filter(([start, last]) => last >= start).sort((a, b) => a[0] - b[0]);
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

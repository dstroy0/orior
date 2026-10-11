// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Sections: the headings a language's plugin names and the blocks it marks, read from a document's
// lines, for the document's folds and its outline. A heading's pattern takes the marks that give its
// level as its first group and its name as its second, as Org writes `** Name` and Markdown writes
// `## Name`. A skip is a pattern that opens a run of lines and one that closes it, inside which no
// line is a heading, as a fenced block of code; a block is the same, a region that folds whole.

// Each heading of a document: its line, its level and its name.
export function headingsOf(doc, { headings, skips = [] }) {
  const found = [];
  if (!headings) {
    return found;
  }
  let skipping = null;
  for (let line = 0; line < doc.count; line += 1) {
    const text = doc.line(line);
    if (skipping) {
      if (skipping.test(text)) {
        skipping = null;
      }
      continue;
    }
    const opened = skips.find(([open]) => open.test(text));
    if (opened) {
      skipping = opened[1];
      continue;
    }
    const match = text.match(headings);
    if (match) {
      found.push({ line, level: match[1].length, name: (match[2] ?? "").trim() });
    }
  }
  return found;
}

// The regions of a document's blocks: each from its opening line to its closing one, a block of the
// same kind inside it closed first.
function blockRegions(doc, blocks, found) {
  for (const [open, close] of blocks) {
    const opened = [];
    for (let line = 0; line < doc.count; line += 1) {
      const text = doc.line(line);
      if (opened.length && close.test(text)) {
        const start = opened.pop();
        if (line > start) {
          found.set(start, line);
        }
      } else if (open.test(text)) {
        opened.push(line);
      }
    }
  }
}

// The folds of a document's headings and blocks, each from its first line to its last: a heading's
// section runs to the last line before the next heading of its level or above, blank lines at its
// end left out.
export function sectionFolds(doc, lang) {
  const found = new Map();
  const headings = headingsOf(doc, lang);
  headings.forEach((heading, index) => {
    const next = headings.slice(index + 1).find((one) => one.level <= heading.level);
    let end = (next ? next.line : doc.count) - 1;
    while (end > heading.line && !doc.line(end).trim()) {
      end -= 1;
    }
    if (end > heading.line) {
      found.set(heading.line, end);
    }
  });
  blockRegions(doc, lang.blocks ?? [], found);
  return found;
}

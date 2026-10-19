// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Move: a declaration moved from the file open to another file of the tree, one step that undo takes
// back in both, where no language server offers it. The declaration is what the cursor's line
// declares, a function, a class or a constant, with the decorators and the comments right above it,
// to the end of its block and the line that closes it. It goes to the end of the file named, which is
// made where it is not there yet.
//
// In JavaScript the declaration is exported in its new file where it was not, and the file it left
// imports it where that file still names it. In Python the file it left imports it from the new
// file's module, named from the tree's top folder. Elsewhere the declaration moves alone.

import { symbolsOf } from "./outline.js";
import { say } from "./statusbar.js";

// A line that belongs above a declaration: a decorator or a comment.
const ABOVE = /^\s*(?:@|#|\/\/|\/\*|\*)/;

// A line of closing brackets alone, which ends a block its region leaves out.
const CLOSING = /^\s*[}\])]+[;,]?\s*$/;

// The lines a declaration at `line` takes, from its first decorator or comment to its last line.
export function declarationLines(s, line) {
  let first = line;
  while (first > 0 && ABOVE.test(s.doc.line(first - 1)) && s.doc.line(first - 1).trim()) {
    first -= 1;
  }
  let last = Math.max(line, s.endOf(line));
  if (last + 1 < s.doc.count && CLOSING.test(s.doc.line(last + 1)) && last > line) {
    last += 1;
  }
  return { first, last };
}

// The path of `to` from the folder of `from`, both paths in the tree, as an import writes it.
export function relativePath(from, to) {
  const there = from.split("/").slice(0, -1);
  const here = to.split("/");
  let shared = 0;
  while (shared < there.length && shared < here.length - 1 && there[shared] === here[shared]) {
    shared += 1;
  }
  const up = there.length - shared;
  const rest = here.slice(shared).join("/");
  return up ? `${"../".repeat(up)}${rest}` : `./${rest}`;
}

// The line an import goes on: after the last import at the file's head, or at its first line.
function importLine(doc, pattern) {
  let at = -1;
  for (let line = 0; line < Math.min(doc.count, 400); line += 1) {
    const text = doc.line(line);
    if (pattern.test(text)) {
      at = line;
    } else if (text.trim() && !ABOVE.test(text) && at >= 0) {
      break;
    }
  }
  return at + 1;
}

// Moves the declaration on the cursor's line of `editor`'s file `path`. `ask` asks for the file it
// goes to, `textOf` reads a file of the tree, `make` makes a file, and `apply` writes every file's
// edits as one step.
export async function moveDeclaration({ editor, path, ask, textOf, make, apply }) {
  const s = editor.s;
  const language = s.language?.id;
  const head = editor.primary().head;
  const symbol = symbolsOf(language, s.doc).find((one) => one.line === head.line && ["function", "class", "constant", "method"].includes(one.kind));
  if (!symbol) {
    say("Move moves what the cursor's line declares: a function, a class or a constant.");
    return;
  }
  const ext = path.includes(".") ? path.slice(path.lastIndexOf(".")) : "";
  const dir = path.includes("/") ? path.slice(0, path.lastIndexOf("/") + 1) : "";
  const to = (await ask(`Move ${symbol.name} to the file`, `${dir}${symbol.name}${ext}`))?.trim().replace(/\\/g, "/");
  if (!to) {
    return;
  }
  if (to === path || to.split("/").includes("..") || to.startsWith("/")) {
    say(`${to} is not another file of the tree.`, { failed: true });
    return;
  }
  const { first, last } = declarationLines(s, symbol.line);
  const lines = [];
  for (let line = first; line <= last; line += 1) {
    lines.push(s.doc.line(line));
  }
  const base = Math.min(...lines.filter((text) => text.trim()).map((text) => text.match(/^\s*/)[0].length));
  let moved = lines.map((text) => text.slice(Math.min(base, text.match(/^\s*/)[0].length)));
  if (language === "javascript") {
    const at = moved.findIndex((text) => !ABOVE.test(text));
    if (at >= 0 && !/^export\b/.test(moved[at])) {
      moved[at] = `export ${moved[at]}`;
    }
  }
  // The file it goes to, made empty where it is not there.
  let there = await textOf(to);
  if (there === null) {
    await make(to);
    there = "";
  }
  const thereLines = there.split(/\r?\n/);
  const end = { line: thereLines.length - 1, col: thereLines.at(-1).length };
  const lead = there.trim() ? (there.endsWith("\n\n") ? "" : there.endsWith("\n") ? "\n" : "\n\n") : "";
  const into = { path: to, edits: [{ from: end, to: end, text: `${lead}${moved.join("\n")}\n` }] };

  // The lines it leaves, with the blank lines after it, which leaves the lines around it spaced as the
  // lines before it were; or, where nothing follows it, with the blank lines before it.
  let after = last;
  while (after + 1 < s.doc.count && !s.doc.line(after + 1).trim()) {
    after += 1;
  }
  let before = first;
  if (after + 1 >= s.doc.count) {
    while (before > 0 && !s.doc.line(before - 1).trim()) {
      before -= 1;
    }
  }
  const gone = after + 1 < s.doc.count ? { from: { line: first, col: 0 }, to: { line: after + 1, col: 0 }, text: "" } : { from: before > 0 ? { line: before - 1, col: s.doc.line(before - 1).length } : { line: 0, col: 0 }, to: { line: after, col: s.doc.line(after).length }, text: "" };
  const from = { path, edits: [gone] };
  const rest = [...Array(s.doc.count).keys()].filter((line) => line < first || line > after).map((line) => s.doc.line(line)).join("\n");
  const named = new RegExp(`(?<![\\w$.])${symbol.name.replace(/[$]/g, "\\$")}(?![\\w$])`).test(rest);
  if (named && language === "javascript") {
    const at = importLine(s.doc, /^\s*import\b/);
    from.edits.unshift({ from: { line: at, col: 0 }, to: { line: at, col: 0 }, text: `import { ${symbol.name} } from "${relativePath(path, to)}";\n` });
  } else if (named && language === "python") {
    const module = to.replace(/\.py$/, "").split("/").join(".");
    const at = importLine(s.doc, /^\s*(?:import|from)\s/);
    from.edits.unshift({ from: { line: at, col: 0 }, to: { line: at, col: 0 }, text: `from ${module} import ${symbol.name}\n` });
  }
  await apply([from, into]);
  say(`${symbol.name} moved to ${to}${named && ["javascript", "python"].includes(language) ? `, and ${path} imports it` : ""}.`);
}

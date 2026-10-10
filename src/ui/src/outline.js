// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The symbols of a file for the outline: what each line declares, found by the line's own shape in
// the file's language. A symbol is { name, kind, line, depth }: its name, what it is, the line it is
// on counted from the document's first, and how deep it sits, by its indent or its heading's level.
// A language with no rules here has no symbols, and a document longer than LIMIT is not read.

const LIMIT = 50000;

// The deepest a symbol is drawn.
const DEEPEST = 5;

// Each rule is a pattern and a kind. The pattern's first group is the line's indent and its second
// the symbol's name.
const RULES = {
  javascript: [
    [/^(\s*)(?:export\s+)?(?:default\s+)?(?:async\s+)?function\s*\*?\s*([A-Za-z_$][\w$]*)/, "function"],
    [/^(\s*)(?:export\s+)?(?:default\s+)?class\s+([A-Za-z_$][\w$]*)/, "class"],
    [/^()(?:export\s+)?(?:const|let|var)\s+([A-Za-z_$][\w$]*)\s*=\s*(?:async\s*)?(?:\([^)]*\)|[A-Za-z_$][\w$]*)\s*=>/, "function"],
    [/^()(?:export\s+)?(?:const|let|var)\s+([A-Za-z_$][\w$]*)\s*=/, "constant"],
    [/^(\s+)(?:static\s+)?(?:async\s+)?(?:get\s+|set\s+)?(?!(?:if|for|while|switch|catch|return|function)\b)([A-Za-z_$][\w$]*)\s*\([^)]*\)\s*\{\s*$/, "method"],
  ],
  rust: [
    [/^(\s*)(?:pub(?:\([^)]*\))?\s+)?(?:(?:const|async|unsafe)\s+|extern\s+"[^"]*"\s+)*fn\s+([A-Za-z_]\w*)/, "function"],
    [/^(\s*)(?:pub(?:\([^)]*\))?\s+)?(?:struct|enum|union|trait|type)\s+([A-Za-z_]\w*)/, "class"],
    [/^(\s*)(?:pub(?:\([^)]*\))?\s+)?mod\s+([A-Za-z_]\w*)/, "module"],
    [/^(\s*)impl(?:<[^>]*>)?\s+([^{]+?)\s*(?:where\b[^{]*)?\{?\s*$/, "class"],
    [/^(\s*)(?:pub(?:\([^)]*\))?\s+)?(?:const|static)\s+([A-Za-z_]\w*)\s*:/, "constant"],
    [/^(\s*)macro_rules!\s*([A-Za-z_]\w*)/, "function"],
  ],
  python: [
    [/^(\s*)(?:async\s+)?def\s+([A-Za-z_]\w*)/, "function"],
    [/^(\s*)class\s+([A-Za-z_]\w*)/, "class"],
    [/^()([A-Z_][A-Z0-9_]*)\s*(?::[^=]*)?=(?!=)/, "constant"],
  ],
  shell: [
    [/^(\s*)function\s+([A-Za-z_][\w-]*)/, "function"],
    [/^(\s*)([A-Za-z_][\w-]*)\s*\(\)/, "function"],
  ],
  powershell: [[/^(\s*)function\s+([\w-]+)/i, "function"]],
  bat: [[/^():(?!:)([\w-]+)/, "label"]],
  c: [
    [/^()#\s*define\s+([A-Za-z_]\w*)/, "constant"],
    [/^()(?:typedef\s+)?(?:struct|enum|union)\s+([A-Za-z_]\w*)\s*\{?\s*$/, "class"],
    [/^()(?!(?:if|for|while|switch|return|else|do|typedef|struct|enum|union)\b)[A-Za-z_][\w\s*&:<>,]*?\b([A-Za-z_]\w*)\s*\([^)]*\)\s*(?:const\s*)?\{/, "function"],
    [/^()(?!(?:if|for|while|switch|return|else|do|typedef|struct|enum|union)\b)[A-Za-z_][\w\s*&:<>,]*?\b([A-Za-z_]\w*)\s*\([^;]*$/, "function"],
  ],
  css: [
    [/^()(@media[^{]*?)\s*\{/, "module"],
    [/^()([^\s{@/*][^{]*?)\s*\{/, "class"],
  ],
  toml: [[/^()\[\[?\s*([^\]]+?)\s*\]\]?/, "module"]],
  yaml: [[/^( {0,2})([\w.-]+):/, "field"]],
};

const TEX_LEVELS = ["part", "chapter", "section", "subsection", "subsubsection", "paragraph"];
const TEX = /\\(part|chapter|section|subsection|subsubsection|paragraph)\*?\s*(?:\[[^\]]*\])?\{([^}]*)\}/;

function markdown(doc) {
  const found = [];
  let fenced = false;
  for (let line = 0; line < doc.count; line += 1) {
    const text = doc.line(line);
    if (/^\s*(```|~~~)/.test(text)) {
      fenced = !fenced;
      continue;
    }
    const heading = !fenced && text.match(/^(#{1,6})\s+(.+?)\s*#*\s*$/);
    if (heading) {
      found.push({ name: heading[2], kind: "heading", line, depth: heading[1].length - 1 });
    }
  }
  const top = Math.min(...found.map((symbol) => symbol.depth));
  return found.map((symbol) => ({ ...symbol, depth: Math.min(DEEPEST, symbol.depth - top) }));
}

function tex(doc) {
  const found = [];
  for (let line = 0; line < doc.count; line += 1) {
    const match = doc.line(line).match(TEX);
    if (match) {
      found.push({ name: match[2], kind: "heading", line, depth: TEX_LEVELS.indexOf(match[1]) });
    }
  }
  const top = Math.min(...found.map((symbol) => symbol.depth));
  return found.map((symbol) => ({ ...symbol, depth: Math.min(DEEPEST, symbol.depth - top) }));
}

// A line's indent in columns, a tab counted as four.
const columns = (indent) => [...indent].reduce((sum, char) => sum + (char === "\t" ? 4 : 1), 0);

function byRules(doc, rules) {
  const found = [];
  for (let line = 0; line < doc.count; line += 1) {
    const text = doc.line(line);
    for (const [pattern, kind] of rules) {
      const match = text.match(pattern);
      if (match) {
        found.push({ name: match[2].trim(), kind, line, indent: columns(match[1]) });
        break;
      }
    }
  }
  const step = Math.min(...found.map((symbol) => symbol.indent).filter((indent) => indent > 0));
  return found.map(({ indent, ...symbol }) => ({ ...symbol, depth: Number.isFinite(step) ? Math.min(DEEPEST, Math.round(indent / step)) : 0 }));
}

// A line that opens a region, and one that closes it: a comment, `#region` or `#pragma region`
// that starts with the word, as Python's `# region Name`, JavaScript's `//#region Name`, C's
// `#pragma region Name` and HTML's `<!-- #region Name -->` write them.
const REGION_OPENS = /^\s*(?:#|\/\/|--|%|;|<!--)?\s*#?\s*(?:pragma\s+)?region\b[\s:]*(.*?)\s*(?:-->)?\s*$/i;
const REGION_CLOSES = /^\s*(?:#|\/\/|--|%|;|<!--)?\s*#?\s*(?:pragma\s+)?end\s*region\b/i;
// What stands before the word on such a line, spaces taken out: a comment's mark, `#` or `#pragma`.
const REGION_MARKS = /^(?:#|\/\/#?|--|%|;|<!--#?|#pragma)$/i;

// The regions a file's comments mark: each its name, its first line, its last, and how deep its
// first line is indented. One not closed reaches the end of the file.
export function regionsOf(doc) {
  if (!doc || doc.count > LIMIT) {
    return [];
  }
  const found = [];
  const open = [];
  for (let line = 0; line < doc.count; line += 1) {
    const text = doc.line(line);
    const at = text.search(/(?:end\s*)?region/i);
    if (at < 0 || !REGION_MARKS.test(text.slice(0, at).replace(/\s+/g, ""))) {
      continue;
    }
    if (REGION_CLOSES.test(text)) {
      const region = open.pop();
      if (region) {
        region.end = line;
      }
      continue;
    }
    const opens = text.match(REGION_OPENS);
    if (opens) {
      const region = { name: opens[1] || text.trim(), line, end: doc.count - 1, indent: text.match(/^\s*/)[0].length };
      found.push(region);
      open.push(region);
    }
  }
  return found;
}

// A file's symbols with the regions its comments mark among them, each region a symbol of kind
// "region" with the line it ends on, and every symbol inside a region a level deeper for each region
// that holds it at its depth or above it.
export function structureOf(language, doc) {
  const symbols = symbolsOf(language, doc);
  const regions = regionsOf(doc);
  if (!regions.length) {
    return symbols;
  }
  const indentOf = (line) => doc.line(line).match(/^\s*/)[0].length;
  const placed = regions.map((region) => {
    const holder = [...symbols].reverse().find((symbol) => symbol.line < region.line && indentOf(symbol.line) < region.indent);
    return { ...region, base: holder ? holder.depth + 1 : 0 };
  });
  const holding = (line, depth) => placed.filter((region) => region.line < line && line <= region.end && region.base <= depth).length;
  const out = symbols.map((symbol) => ({ ...symbol, depth: symbol.depth + holding(symbol.line, symbol.depth) }));
  for (const region of placed) {
    out.push({ name: region.name, kind: "region", line: region.line, end: region.end, depth: region.base + holding(region.line, region.base) });
  }
  return out.sort((a, b) => a.line - b.line || (a.kind === "region" ? -1 : 1));
}

export function symbolsOf(language, doc) {
  if (!doc || doc.count > LIMIT) {
    return [];
  }
  if (language === "markdown") {
    return markdown(doc);
  }
  if (language === "tex") {
    return tex(doc);
  }
  return RULES[language] ? byRules(doc, RULES[language]) : [];
}

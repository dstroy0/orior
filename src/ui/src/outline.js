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

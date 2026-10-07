// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The languages the tree is written in besides its own: what each colors, how each comments a
// line, and what opens a deeper indent. Each is a language as the editor takes one:
//
//   { id, grammar, comments: { line, block: [open, close] }, pairs, quotes, indentAfter }
//
// with hover(doc, at) and complete(doc, at) where a language has them. The tree's own languages
// are built in ../languages.js from the tables the tree holds.

import { compile } from "./tokens.js";

const BRACKETS = ["()", "[]", "{}"];
const QUOTES = ['"', "'"];
const OPENS_BRACKET = /[{[(]\s*$/;

const number = /0[xX][\da-fA-F_']+[uUlLzZ]*|0[bB][01_']+[uUlL]*|(?:\d[\d_']*)?\.?\d[\d_']*(?:[eE][+-]?\d+)?[fFuUlLjJ]*/;

const cComments = {
  whitespace: [
    [/\s+/, ""],
    [/\/\*/, "comment", "@comment"],
    [/\/\/.*$/, "comment"],
  ],
  comment: [
    [/[^/*]+/, "comment"],
    [/\*\//, "comment", "@pop"],
    [/[/*]/, "comment"],
  ],
};

const C_WORDS = [
  "auto", "break", "case", "const", "continue", "default", "do", "else", "enum", "extern", "for", "goto", "if",
  "inline", "register", "restrict", "return", "sizeof", "static", "struct", "switch", "typedef", "union",
  "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Generic", "_Noreturn", "_Static_assert",
  "_Thread_local", "class", "namespace", "template", "typename", "public", "private", "protected", "virtual",
  "override", "final", "constexpr", "consteval", "static_assert", "decltype", "new", "delete", "operator",
  "this", "using", "try", "catch", "throw", "noexcept", "explicit", "friend", "mutable", "nullptr", "true",
  "false", "NULL", "__global__", "__device__", "__host__", "__shared__", "__constant__", "__restrict__",
  "__forceinline__", "__noinline__", "__launch_bounds__", "__syncthreads", "alignas", "alignof",
];

const C_TYPES = [
  "void", "char", "short", "int", "long", "float", "double", "signed", "unsigned", "bool", "_Bool",
  "_Complex", "size_t", "ptrdiff_t", "intptr_t", "uintptr_t", "int8_t", "int16_t", "int32_t", "int64_t",
  "uint8_t", "uint16_t", "uint32_t", "uint64_t", "wchar_t", "char16_t", "char32_t", "half", "dim3", "float2",
  "float3", "float4", "double2", "int2", "int3", "int4", "uint2", "uint3", "uint4", "FILE",
];

const c = {
  words: C_WORDS,
  types: C_TYPES,
  tokenizer: {
    root: [
      [/^\s*#\s*\w+/, "keyword.directive", "@directive"],
      { include: "@whitespace" },
      [/[A-Za-z_]\w*/, { cases: { "@types": "type", "@words": "keyword", "@default": "identifier" } }],
      [number, "number"],
      [/[uUL]?"/, "string", "@string"],
      [/'(?:[^'\\]|\\.)*'/, "string"],
      [/[{}()[\]]/, "@brackets"],
      [/[<>=!~?:&|+\-*/^%]+/, "operator"],
      [/[;,.]/, "delimiter"],
    ],
    directive: [
      [/<[^>]*>/, "string"],
      [/"(?:[^"\\]|\\.)*"/, "string"],
      [/\/\/.*$/, "comment"],
      [/\\$/, "keyword.directive", "@directive"],
      [/$/, "", "@pop"],
      [/[^"<\\/]+|./, "keyword.directive"],
    ],
    string: [
      [/[^\\"]+/, "string"],
      [/\\./, "string.escape"],
      [/"/, "string", "@pop"],
    ],
    ...cComments,
  },
};

const rust = {
  words: [
    "as", "async", "await", "break", "const", "continue", "crate", "dyn", "else", "enum", "extern", "false", "fn",
    "for", "if", "impl", "in", "let", "loop", "match", "mod", "move", "mut", "pub", "ref", "return", "self",
    "Self", "static", "struct", "super", "trait", "true", "type", "unsafe", "use", "where", "while",
  ],
  types: [
    "i8", "i16", "i32", "i64", "i128", "isize", "u8", "u16", "u32", "u64", "u128", "usize", "f32", "f64", "bool",
    "char", "str", "String", "Vec", "Option", "Result", "Box", "Rc", "Arc", "HashMap", "HashSet", "Path",
    "PathBuf", "Some", "None", "Ok", "Err",
  ],
  tokenizer: {
    root: [
      [/#!?\[/, "keyword.directive", "@attribute"],
      { include: "@whitespace" },
      [/[a-z_]\w*!/, "predefined"],
      [/'[A-Za-z_]\w*(?!')/, "type"],
      [/[A-Za-z_]\w*/, { cases: { "@types": "type", "@words": "keyword", "@default": "identifier" } }],
      [number, "number"],
      [/b?r(#*)"/, "string", "@raw"],
      [/b?"/, "string", "@string"],
      [/b?'(?:[^'\\]|\\.[^']*)'/, "string"],
      [/[{}()[\]]/, "@brackets"],
      [/[<>=!~?:&|+\-*/^%@]+/, "operator"],
      [/[;,.]/, "delimiter"],
    ],
    attribute: [
      [/\]/, "keyword.directive", "@pop"],
      [/"(?:[^"\\]|\\.)*"/, "string"],
      [/[^\]"]+/, "keyword.directive"],
    ],
    string: [
      [/[^\\"]+/, "string"],
      [/\\./, "string.escape"],
      [/"/, "string", "@pop"],
    ],
    raw: [
      [/"#*/, "string", "@pop"],
      [/[^"]+/, "string"],
    ],
    whitespace: [
      [/\s+/, ""],
      [/\/\*/, "comment", "@comment"],
      [/\/\/.*$/, "comment"],
    ],
    comment: [
      [/[^/*]+/, "comment"],
      [/\/\*/, "comment", "@comment"],
      [/\*\//, "comment", "@pop"],
      [/[/*]/, "comment"],
    ],
  },
};

const python = {
  words: [
    "False", "None", "True", "and", "as", "assert", "async", "await", "break", "class", "continue", "def", "del",
    "elif", "else", "except", "finally", "for", "from", "global", "if", "import", "in", "is", "lambda",
    "nonlocal", "not", "or", "pass", "raise", "return", "try", "while", "with", "yield", "match", "case", "self",
  ],
  builtins: [
    "abs", "all", "any", "bool", "bytes", "callable", "chr", "dict", "dir", "divmod", "enumerate", "filter",
    "float", "format", "frozenset", "getattr", "hasattr", "hash", "hex", "id", "input", "int", "isinstance",
    "issubclass", "iter", "len", "list", "map", "max", "min", "next", "object", "open", "ord", "pow", "print",
    "range", "repr", "reversed", "round", "set", "setattr", "slice", "sorted", "str", "sum", "super", "tuple",
    "type", "vars", "zip",
  ],
  tokenizer: {
    root: [
      [/\s+/, ""],
      [/#.*$/, "comment"],
      [/@[A-Za-z_][\w.]*/, "tag"],
      [/[rRbBfFuU]{0,2}"""/, "string", "@long2"],
      [/[rRbBfFuU]{0,2}'''/, "string", "@long1"],
      [/[rRbBfFuU]{0,2}"(?:[^"\\]|\\.)*"/, "string"],
      [/[rRbBfFuU]{0,2}'(?:[^'\\]|\\.)*'/, "string"],
      [/[A-Za-z_]\w*/, { cases: { "@words": "keyword", "@builtins": "predefined", "@default": "identifier" } }],
      [number, "number"],
      [/[{}()[\]]/, "@brackets"],
      [/[<>=!~:&|+\-*/^%@]+/, "operator"],
      [/[;,.]/, "delimiter"],
    ],
    long2: [
      [/"""/, "string", "@pop"],
      [/[^"\\]+|\\.|"/, "string"],
    ],
    long1: [
      [/'''/, "string", "@pop"],
      [/[^'\\]+|\\.|'/, "string"],
    ],
  },
};

const shell = {
  words: [
    "if", "then", "else", "elif", "fi", "for", "while", "until", "do", "done", "case", "esac", "in", "function",
    "select", "time", "return", "local", "export", "readonly", "declare", "set", "unset", "shift", "exit",
    "break", "continue", "source", "trap", "eval", "exec",
  ],
  builtins: ["echo", "printf", "cd", "test", "read", "pwd", "true", "false", "command", "type", "wait", "kill"],
  tokenizer: {
    root: [
      [/\s+/, ""],
      [/#.*$/, "comment"],
      [/\$(?:\{[^}]*\}|[A-Za-z_]\w*|[0-9#?@*$!-])/, "variable"],
      [/\$\(\(?/, "operator"],
      [/"/, "string", "@string"],
      [/'[^']*'/, "string"],
      [/'/, "string", "@quoted"],
      [/[A-Za-z_][\w-]*(?==)/, "variable"],
      [/[A-Za-z_][\w.-]*/, { cases: { "@words": "keyword", "@builtins": "predefined", "@default": "" } }],
      [/-{1,2}[A-Za-z][\w-]*/, "attribute.name"],
      [/\d+/, "number"],
      [/[{}()[\]]/, "@brackets"],
      [/[<>=!&|;]+/, "operator"],
    ],
    string: [
      [/\$(?:\{[^}]*\}|[A-Za-z_]\w*|[0-9#?@*$!-])/, "variable"],
      [/\\./, "string.escape"],
      [/"/, "string", "@pop"],
      [/[^"\\$]+|\$/, "string"],
    ],
    quoted: [
      [/'/, "string", "@pop"],
      [/[^']+/, "string"],
    ],
  },
};

const powershell = {
  ignoreCase: true,
  words: [
    "begin", "break", "catch", "class", "continue", "data", "do", "dynamicparam", "else", "elseif", "end", "exit",
    "filter", "finally", "for", "foreach", "function", "if", "in", "param", "process", "return", "switch", "throw",
    "trap", "try", "until", "using", "while",
  ],
  tokenizer: {
    root: [
      [/\s+/, ""],
      [/<#/, "comment", "@comment"],
      [/#.*$/, "comment"],
      [/\$(?:\{[^}]*\}|[\w:]+)/, "variable"],
      [/"/, "string", "@string"],
      [/'(?:[^']|'')*'/, "string"],
      [/-[A-Za-z]+/, "operator"],
      [/[A-Za-z]+-[A-Za-z]+/, "predefined"],
      [/\[[A-Za-z.]+\]/, "type"],
      [/[A-Za-z_]\w*/, { cases: { "@words": "keyword", "@default": "" } }],
      [/\d+/, "number"],
      [/[{}()[\]]/, "@brackets"],
      [/[<>=!&|;+*/%-]+/, "operator"],
    ],
    comment: [
      [/#>/, "comment", "@pop"],
      [/[^#]+|#/, "comment"],
    ],
    string: [
      [/\$(?:\{[^}]*\}|[\w:]+)/, "variable"],
      [/`./, "string.escape"],
      [/"/, "string", "@pop"],
      [/[^"`$]+|\$/, "string"],
    ],
  },
};

const batch = {
  ignoreCase: true,
  perLine: true,
  words: [
    "call", "cd", "copy", "defined", "del", "do", "echo", "else", "endlocal", "equ", "errorlevel", "exist",
    "exit", "for", "geq", "goto", "gtr", "if", "in", "leq", "lss", "md", "mkdir", "neq", "not", "off", "on",
    "pause", "popd", "pushd", "rd", "set", "setlocal", "shift", "start",
  ],
  tokenizer: {
    root: [
      [/^\s*(?:rem\b|::).*$/i, "comment"],
      [/^\s*:\w+/, "tag"],
      [/%%?~?[\w]+%?|![\w]+!/, "variable"],
      [/"[^"]*"/, "string"],
      [/[A-Za-z_]\w*/, { cases: { "@words": "keyword", "@default": "" } }],
      [/\d+/, "number"],
      [/[()]/, "@brackets"],
      [/[<>=&|@]+/, "operator"],
      [/\s+/, ""],
    ],
  },
};

const javascript = {
  words: [
    "async", "await", "break", "case", "catch", "class", "const", "continue", "debugger", "default", "delete",
    "do", "else", "export", "extends", "false", "finally", "for", "from", "function", "if", "import", "in",
    "instanceof", "let", "new", "null", "of", "return", "static", "super", "switch", "this", "throw", "true",
    "try", "typeof", "undefined", "var", "void", "while", "with", "yield",
  ],
  tokenizer: {
    root: [
      { include: "@whitespace" },
      [/[A-Za-z_$][\w$]*/, { cases: { "@words": "keyword", "@default": "identifier" } }],
      [number, "number"],
      [/"(?:[^"\\]|\\.)*"/, "string"],
      [/'(?:[^'\\]|\\.)*'/, "string"],
      [/`/, "string", "@template"],
      [/[{}()[\]]/, "@brackets"],
      [/[<>=!~?:&|+\-*/^%]+/, "operator"],
      [/[;,.]/, "delimiter"],
    ],
    template: [
      [/`/, "string", "@pop"],
      [/\$\{/, "delimiter.bracket", "@inner"],
      [/\\./, "string.escape"],
      [/[^`\\$]+|\$/, "string"],
    ],
    inner: [
      [/\}/, "delimiter.bracket", "@pop"],
      { include: "@root" },
    ],
    ...cComments,
  },
};

const json = {
  tokenizer: {
    root: [
      [/\s+/, ""],
      [/"(?:[^"\\]|\\.)*"(?=\s*:)/, "variable"],
      [/"(?:[^"\\]|\\.)*"/, "string"],
      [/-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?/, "number"],
      [/\b(?:true|false|null)\b/, "keyword"],
      [/[{}[\]]/, "@brackets"],
      [/[:,]/, "delimiter"],
      [/\/\/.*$/, "comment"],
    ],
  },
};

const yaml = {
  perLine: true,
  tokenizer: {
    root: [
      [/^(?:---|\.\.\.)\s*$/, "keyword"],
      [/^(\s*)(-\s+)?([^\s#'"][^:#]*?|"[^"]*"|'[^']*')(:)(?=\s|$)/, ["", "delimiter", "variable", "delimiter"]],
      [/^\s*-(?=\s|$)/, "delimiter"],
      [/(^|\s)#.*$/, "comment"],
      [/[&*][\w-]+/, "tag"],
      [/![\w!-]*/, "type"],
      [/"(?:[^"\\]|\\.)*"/, "string"],
      [/'(?:[^']|'')*'/, "string"],
      [/\b(?:true|false|null|yes|no|on|off)\b/i, "keyword"],
      [/-?\d+(?:\.\d+)?\b/, "number"],
      [/[{}[\]]/, "@brackets"],
      [/[|>]-?(?=\s*$)/, "operator"],
      [/[,:]/, "delimiter"],
      [/[^\s#"'{}[\],:]+|\s+|./, "string"],
    ],
  },
};

const toml = {
  perLine: true,
  tokenizer: {
    root: [
      [/^\s*\[\[?[^\]]*\]\]?/, "keyword"],
      [/^(\s*)([\w.\-"']+)(\s*)([=:])/, ["", "variable", "", "operator"]],
      [/[#;].*$/, "comment"],
      [/"(?:[^"\\]|\\.)*"/, "string"],
      [/'[^']*'/, "string"],
      [/\b(?:true|false)\b/, "keyword"],
      [/[+-]?\d[\d_:.eE+\-TZ]*/, "number"],
      [/[{}[\]]/, "@brackets"],
      [/[,=]/, "delimiter"],
      [/[^\s#;"'{}[\],=]+|\s+/, ""],
    ],
  },
};

const markdown = {
  tokenizer: {
    root: [
      [/^\s*(?:```|~~~).*$/, "string", "@fence"],
      [/^\s{0,3}#{1,6}\s.*$/, "keyword"],
      [/^\s*>/, "comment"],
      [/^\s*(?:[-*+]|\d+[.)])(?=\s)/, "operator"],
      [/^\s*(?:-{3,}|\*{3,}|_{3,})\s*$/, "operator"],
      [/<!--/, "comment", "@comment"],
      [/`[^`]+`/, "string"],
      [/\$\$?[^$]+\$\$?/, "number"],
      [/!?\[[^\]]*\]\([^)]*\)/, "tag"],
      [/\[[^\]]*\]\[[^\]]*\]/, "tag"],
      [/\*\*[^*]+\*\*|__[^_]+__/, "strong"],
      [/\*[^*\s][^*]*\*|\b_[^_\s][^_]*_\b/, "emphasis"],
      [/\|/, "delimiter"],
      [/<\/?[\w-]+[^>]*>/, "tag"],
      [/[^`$!*_[|<\\]+|./, ""],
    ],
    fence: [
      [/^\s*(?:```|~~~)\s*$/, "string", "@pop"],
      [/.*$/, "string"],
    ],
    comment: [
      [/-->/, "comment", "@pop"],
      [/[^-]+|-/, "comment"],
    ],
  },
};

const html = {
  tokenizer: {
    root: [
      [/<!--/, "comment", "@comment"],
      [/<!\w[^>]*>/, "keyword"],
      [/(<\/?)([\w-]+)/, ["delimiter", "tag"], "@tag"],
      [/&\w+;|&#\d+;/, "variable"],
      [/[^<&]+|./, ""],
    ],
    tag: [
      [/\s+/, ""],
      [/([\w:@.-]+)(\s*=\s*)("[^"]*"|'[^']*'|[^\s>]+)/, ["attribute.name", "delimiter", "string"]],
      [/[\w:@.-]+/, "attribute.name"],
      [/\/?>/, "delimiter", "@pop"],
    ],
    comment: [
      [/-->/, "comment", "@pop"],
      [/[^-]+|-/, "comment"],
    ],
  },
};

const css = {
  tokenizer: {
    root: [
      { include: "@whitespace" },
      [/@[\w-]+/, "keyword"],
      [/\{/, "@brackets", "@block"],
      [/::?[\w-]+/, "attribute.name"],
      [/[.#]?[\w-]+/, "tag"],
      [/\[[^\]]*\]/, "attribute.name"],
      [/[>+~,*()]/, "operator"],
    ],
    block: [
      { include: "@whitespace" },
      [/\}/, "@brackets", "@pop"],
      [/\{/, "@brackets", "@block"],
      [/--[\w-]+/, "variable"],
      [/[\w-]+(?=\s*:)/, "variable"],
      [/#[\da-fA-F]{3,8}\b/, "number"],
      [/-?\d*\.?\d+(?:[a-z%]+)?/, "number"],
      [/"[^"]*"|'[^']*'/, "string"],
      [/!important/, "keyword"],
      [/[\w-]+(?=\()/, "predefined"],
      [/[\w-]+/, ""],
      [/[:;,()/]/, "delimiter"],
    ],
    whitespace: [
      [/\s+/, ""],
      [/\/\*/, "comment", "@comment"],
    ],
    comment: cComments.comment,
  },
};

const tex = {
  tokenizer: {
    root: [
      [/%.*$/, "comment"],
      [/(\\(?:begin|end))(\{)([^}]*)(\})/, ["keyword", "delimiter.bracket", "type", "delimiter.bracket"]],
      [/\\(?:[A-Za-z@]+|.)/, "keyword"],
      [/\$\$?/, "number", "@math"],
      [/\\\[|\\\(/, "number", "@math"],
      [/[{}[\]]/, "@brackets"],
      [/[&~^_]/, "operator"],
      [/[^\\$%{}[\]&~^_]+/, ""],
    ],
    math: [
      [/\$\$?|\\\]|\\\)/, "number", "@pop"],
      [/\\(?:[A-Za-z@]+|.)/, "predefined"],
      [/%.*$/, "comment"],
      [/[^$\\%]+/, "number"],
    ],
  },
};

// A table: each column its own color, cycling past the sixth, and # lines its comments.
const COLUMNS = ["identifier", "type", "variable", "string", "keyword", "number"];
const tsvStates = { root: [[/^#.*$/, "comment"], [/[^\t]+/, COLUMNS[0]], [/\t/, "delimiter", "@c1"]] };
for (let index = 1; index < COLUMNS.length; index += 1) {
  const next = index + 1 < COLUMNS.length ? `@c${index + 1}` : "@c1";
  tsvStates[`c${index}`] = [[/[^\t]+/, COLUMNS[index]], [/\t/, "delimiter", next]];
}
const tsv = { perLine: true, tokenizer: tsvStates };

const lang = (id, def, extra = {}) => ({
  id,
  grammar: def ? compile(def) : null,
  comments: {},
  pairs: BRACKETS,
  quotes: QUOTES,
  indentAfter: OPENS_BRACKET,
  ...extra,
});

const cStyle = { comments: { line: "//", block: ["/*", "*/"] } };
const hashed = { comments: { line: "#" } };

export const BROUGHT = [
  lang("c", c, cStyle),
  lang("rust", rust, cStyle),
  lang("python", python, { ...hashed, indentAfter: /(?::\s*(?:#.*)?|[{[(]\s*)$/ }),
  lang("shell", shell, { ...hashed, indentAfter: /(?:\b(?:then|do|else)|[{[(]|\bin)\s*$/ }),
  lang("powershell", powershell, { comments: { line: "#", block: ["<#", "#>"] } }),
  lang("bat", batch, { comments: { line: "REM" }, quotes: ['"'] }),
  lang("javascript", javascript, { ...cStyle, quotes: ['"', "'", "`"] }),
  lang("json", json, { comments: { line: "//" }, quotes: ['"'] }),
  lang("yaml", yaml, { ...hashed, indentAfter: /:\s*$/ }),
  lang("toml", toml, hashed),
  lang("markdown", markdown, { comments: { block: ["<!--", "-->"] }, quotes: [], pairs: ["()", "[]", "``"] }),
  lang("html", html, { comments: { block: ["<!--", "-->"] }, indentAfter: /<(?!\/|br|hr|img|input|meta|link)[\w-]+[^>]*>\s*$/ }),
  lang("css", css, { comments: { block: ["/*", "*/"] } }),
  lang("tex", tex, { comments: { line: "%" }, quotes: [], pairs: ["{}", "[]", "()", "$$"] }),
  lang("tsv", tsv, { ...hashed, quotes: [], pairs: [] }),
  lang("plaintext", null, { quotes: [], pairs: [] }),
];

// The language each extension the tree writes opens in.
export const BY_EXTENSION = {
  c: "c", h: "c", cu: "c", cuh: "c", cpp: "c", hpp: "c", cc: "c", rs: "rust", py: "python", sh: "shell",
  bash: "shell", ps1: "powershell", psm1: "powershell", bat: "bat", cmd: "bat", js: "javascript",
  mjs: "javascript", json: "json", cfg: "json", yml: "yaml", yaml: "yaml", toml: "toml", ini: "toml",
  md: "markdown", html: "html", htm: "html", svg: "html", xml: "html", css: "css", tex: "tex", sty: "tex",
  cls: "tex", bib: "tex", tsv: "tsv", csv: "tsv",
};

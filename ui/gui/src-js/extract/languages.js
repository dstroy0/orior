// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The languages Extract Function has a form for, each with the names it keeps as its own, how its lines are read
// and how a function is written in it.

const PYTHON_KEEPS = new Set(
  (
    "False None True and as assert async await break class continue def del elif else except finally for from global if import in is lambda nonlocal not or pass raise return try while with yield " +
    "abs all any bool bytes callable chr dict dir divmod enumerate filter float format frozenset getattr hasattr hash hex id input int isinstance issubclass iter len list map max min next object oct open ord pow print range repr reversed round set setattr slice sorted str sum super tuple type vars zip " +
    "Exception ValueError TypeError KeyError IndexError RuntimeError StopIteration OSError NotImplementedError self cls"
  ).split(" "),
);
PYTHON_KEEPS.delete("self");
PYTHON_KEEPS.delete("cls");

const SCRIPT_KEEPS = new Set(
  (
    "await break case catch class const continue debugger default delete do else export extends false finally for function if import in instanceof let new null of return static super switch this throw true try typeof undefined var void while with yield async " +
    "Array Boolean Date Error JSON Map Math Number Object Promise RegExp Set String Symbol WeakMap console document globalThis window Infinity NaN isNaN parseFloat parseInt"
  ).split(" "),
);

const RUST_KEEPS = new Set(
  (
    "as async await break const continue crate dyn else enum extern false fn for if impl in let loop match mod move mut pub ref return self Self static struct super trait true type unsafe use where while " +
    "bool char str i8 i16 i32 i64 i128 isize u8 u16 u32 u64 u128 usize f32 f64 String Vec Box Option Result Some None Ok Err"
  ).split(" "),
);

const C_KEEPS = new Set(
  (
    "auto break case char const continue default do double else enum extern float for goto if inline int long register restrict return short signed sizeof static struct switch typedef union unsigned void volatile while " +
    "bool true false nullptr NULL this new delete class public private protected template typename namespace using operator virtual override final constexpr noexcept throw try catch " +
    "size_t ptrdiff_t int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t uint64_t std __auto_type"
  ).split(" "),
);

// The names a parameter list holds: each part's last name in the C family, the name before its `:`
// in Rust, and the name before any default or type in Python and JavaScript, spreads set aside.
// Rust's `self` in any of its forms is no name the selection takes.
export const nameOfPart = {
  default: (part) => part.trim().replace(/^\*{1,2}|^\.\.\./, "").split(/[=:]/)[0].trim(),
  rust: (part) => (/^&?\s*(?:'\w+\s+)?(?:mut\s+)?self\b/.test(part.trim()) ? "" : part.trim().replace(/^mut\s+/, "").split(":")[0].trim()),
  c: (part) => part.replace(/\[[^\]]*\]/g, "").split("=")[0].match(/([A-Za-z_]\w*)\s*\)?\s*$/)?.[1] ?? "",
};

export const LANGUAGES = {
  python: {
    keeps: PYTHON_KEEPS,
    heads: [/^(\s*)(?:async\s+)?def\s+\w+\s*\((.*)\)\s*(?:->[^:]*)?:\s*$/],
    tops: /^(?:async\s+def|def|class)\b/,
    // What a line sets: the names before its `=`, those a for loop takes, and the name after `as`.
    sets: (line) => {
      const names = [];
      const assigned = line.match(/^\s*([A-Za-z_]\w*(?:\s*,\s*[A-Za-z_]\w*)*)\s*(?:[-+*/%&|^@]|\/\/|\*\*|>>|<<)?=(?!=)/);
      if (assigned) {
        names.push(...assigned[1].split(",").map((name) => name.trim()));
      }
      const loop = line.match(/^\s*(?:async\s+)?for\s+([A-Za-z_]\w*(?:\s*,\s*[A-Za-z_]\w*)*)\s+in\b/);
      if (loop) {
        names.push(...loop[1].split(",").map((name) => name.trim()));
      }
      for (const found of line.matchAll(/\bas\s+([A-Za-z_]\w*)/g)) {
        names.push(found[1]);
      }
      return names;
    },
    define: (name, params, body, unit, isAsync) => `${isAsync ? "async " : ""}def ${name}(${params.join(", ")}):\n${body.map((line) => (line ? unit + line : "")).join("\n")}\n`,
    giveBack: (names) => `return ${names.join(", ")}`,
    expression: (value) => `return ${value}`,
    call: (name, params, isAsync) => `${isAsync ? "await " : ""}${name}(${params.join(", ")})`,
    assign: (names, call) => `${names.join(", ")} = ${call}`,
    statement: (call) => call,
    gap: "\n\n",
  },
  javascript: {
    keeps: SCRIPT_KEEPS,
    heads: [
      /^(\s*)(?:export\s+)?(?:default\s+)?(?:async\s+)?function\s*\*?\s*[\w$]*\s*\(([^)]*)\)\s*\{\s*$/,
      /^(\s*)(?:static\s+)?(?:async\s+)?(?!(?:if|for|while|switch|catch|function)\b)[\w$]+\s*\(([^)]*)\)\s*\{\s*$/,
      /^(\s*)(?:export\s+)?(?:const|let|var)\s+[\w$]+\s*=\s*(?:async\s*)?\(([^)]*)\)\s*=>\s*\{\s*$/,
    ],
    tops: /^(?:export\s+)?(?:default\s+)?(?:async\s+)?(?:function|class|const|let|var)\b/,
    sets: (line) => {
      const names = [];
      const declared = line.match(/^\s*(?:const|let|var)\s+([A-Za-z_$][\w$]*)/);
      if (declared) {
        names.push(declared[1]);
      }
      const assigned = line.match(/^\s*([A-Za-z_$][\w$]*)\s*(?:[-+*/%&|^]|\*\*|>>>?|<<|\?\?|&&|\|\|)?=(?!=)/);
      if (assigned) {
        names.push(assigned[1]);
      }
      const loop = line.match(/^\s*for\s*\(\s*(?:const|let|var)\s+([A-Za-z_$][\w$]*)/);
      if (loop) {
        names.push(loop[1]);
      }
      return names;
    },
    define: (name, params, body, unit, isAsync) => `${isAsync ? "async " : ""}function ${name}(${params.join(", ")}) {\n${body.map((line) => (line ? unit + line : "")).join("\n")}\n}\n`,
    giveBack: (names) => `return ${names[0]};`,
    expression: (value) => `return ${value};`,
    call: (name, params, isAsync) => `${isAsync ? "await " : ""}${name}(${params.join(", ")})`,
    assign: (names, call, declared) => `${declared ? `${declared} ` : ""}${names[0]} = ${call};`,
    statement: (call) => `${call};`,
    gap: "\n",
  },
  rust: {
    typed: true,
    keeps: RUST_KEEPS,
    part: nameOfPart.rust,
    heads: [/^(\s*)(?:pub(?:\([^)]*\))?\s+)?(?:const\s+)?(?:async\s+)?(?:unsafe\s+)?(?:extern\s+"[^"]*"\s+)?fn\s+[A-Za-z_]\w*\s*(?:<[^>]*>)?\s*\((.*)\)\s*(?:->[^{]*?)?\s*(?:where\b[^{]*)?\{?\s*$/],
    // What a line sets: the names a `let` binds, one or a tuple of them, the name before an `=`, and
    // those a for loop takes.
    sets: (line) => {
      const names = [];
      const bound = line.match(/^\s*let\s+(?:mut\s+)?([A-Za-z_]\w*)/);
      if (bound) {
        names.push(bound[1]);
      }
      const tuple = line.match(/^\s*let\s+(?:mut\s+)?\(([^)]*)\)/);
      if (tuple) {
        names.push(...tuple[1].split(",").map((name) => name.trim().replace(/^mut\s+/, "")).filter((name) => /^[A-Za-z_]\w*$/.test(name)));
      }
      const assigned = line.match(/^\s*([A-Za-z_]\w*)\s*(?:[-+*/%&|^]|<<|>>)?=(?![=>])/);
      if (assigned) {
        names.push(assigned[1]);
      }
      const loop = line.match(/^\s*for\s+\(?\s*([A-Za-z_]\w*(?:\s*,\s*[A-Za-z_]\w*)*)\s*\)?\s+in\b/);
      if (loop) {
        names.push(...loop[1].split(",").map((name) => name.trim()));
      }
      return names;
    },
    // A name's type as rust-analyzer's hover gives it, `let mut name: Type` or `name: Type`, and
    // whether it is declared `mut`; null for a name that is no local and no parameter.
    typeIn: (said, name) => {
      for (const block of said.matchAll(/```rust\n([\s\S]*?)\n```/g)) {
        for (const line of block[1].split("\n")) {
          const found = line.match(new RegExp(`^(?:let\\s+)?(mut\\s+)?(?:ref\\s+)?${name}\\s*:\\s*(.+?)\\s*$`));
          if (found) {
            return { type: found[2], mut: Boolean(found[1]) };
          }
        }
      }
      return null;
    },
    probe: (name, value) => `let ${name} = ${value};`,
  },
  c: {
    typed: true,
    keeps: C_KEEPS,
    part: nameOfPart.c,
    heads: [/^(\s*)(?!(?:if|for|while|switch|return|else|do|case|sizeof)\b)(?:[A-Za-z_][\w:<>,]*[\s*&]+)+[A-Za-z_~][\w:~]*\s*\(([^;]*)\)\s*(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?\{?\s*$/],
    // What a line sets: the name a declaration declares, the name before an `=`, and one stepped by
    // `++` or `--`.
    sets: (line) => {
      const names = [];
      const declared = line.match(/^\s*(?:for\s*\(\s*)?(?!(?:return|else|case|goto|delete|throw)\b)(?:(?:const|static|unsigned|signed|long|short|struct|enum|volatile|register|auto|__auto_type)\s+)*[A-Za-z_][\w:]*(?:\s*<[^;=]*>)?[\s*&]+([A-Za-z_]\w*)\s*(?:=|;|\[|\{)/);
      if (declared) {
        names.push(declared[1]);
      }
      const assigned = line.match(/^\s*([A-Za-z_]\w*)\s*(?:[-+*/%&|^]|<<|>>)?=(?!=)/);
      if (assigned) {
        names.push(assigned[1]);
      }
      const stepped = line.match(/^\s*(?:\+\+|--)?([A-Za-z_]\w*)\s*(?:\+\+|--)?\s*;/);
      if (stepped && /\+\+|--/.test(line)) {
        names.push(stepped[1]);
      }
      return names;
    },
    // A name's type as clangd's hover gives it, its `Type:`, for a parameter or a variable local to
    // a function; a field marked as one; null for any other name.
    typeIn: (said) => {
      const type = said.match(/^###\s+([\w ]+?)\s+`/m)?.[1] ?? "";
      const hoverType = said.match(/Type:\s*`([^`]+)`/)?.[1] ?? null;
      if (type === "field") {
        return { field: true };
      }
      const local = type === "param" || (type.endsWith("variable") && /\/\/ In (?!namespace\b)/.test(said));
      return local && hoverType ? { type: hoverType } : null;
    },
  },
};

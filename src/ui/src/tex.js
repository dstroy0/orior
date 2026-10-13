// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// TeX's math as MathML, which the page draws as it draws text: the math GitHub's Markdown takes
// between dollars and in math fences. Letters, numbers and operators; fractions, roots, binomials;
// scripts above and below, and limits over and under a big operator in display; Greek letters and
// the common symbols; the functions' names; delimiters sized to what they hold; accents; the type
// faces; spaces; and matrices, cases and aligned lines. A command it does not know is shown as it
// was written, marked as an error.

const ESCAPES = { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" };
const escape = (text) => text.replace(/[&<>"]/g, (letter) => ESCAPES[letter]);

const GREEK = {
  alpha: "α", beta: "β", gamma: "γ", delta: "δ", epsilon: "ϵ", varepsilon: "ε", zeta: "ζ", eta: "η", theta: "θ", vartheta: "ϑ", iota: "ι", kappa: "κ", lambda: "λ", mu: "μ", nu: "ν", xi: "ξ", pi: "π", varpi: "ϖ", rho: "ρ", varrho: "ϱ", sigma: "σ", varsigma: "ς", tau: "τ", upsilon: "υ", phi: "ϕ", varphi: "φ", chi: "χ", psi: "ψ", omega: "ω",
  Gamma: "Γ", Delta: "Δ", Theta: "Θ", Lambda: "Λ", Xi: "Ξ", Pi: "Π", Sigma: "Σ", Upsilon: "Υ", Phi: "Φ", Psi: "Ψ", Omega: "Ω",
};

// Symbols drawn as operators, and those drawn as identifiers.
const OPERATORS = {
  cdot: "⋅", times: "×", div: "÷", pm: "±", mp: "∓", le: "≤", leq: "≤", ge: "≥", geq: "≥", ne: "≠", neq: "≠", approx: "≈", equiv: "≡", sim: "∼", simeq: "≃", cong: "≅", propto: "∝",
  in: "∈", notin: "∉", ni: "∋", subset: "⊂", subseteq: "⊆", supset: "⊃", supseteq: "⊇", cup: "∪", cap: "∩", setminus: "∖", wedge: "∧", land: "∧", vee: "∨", lor: "∨", neg: "¬", lnot: "¬",
  to: "→", rightarrow: "→", leftarrow: "←", gets: "←", Rightarrow: "⇒", Leftarrow: "⇐", implies: "⟹", iff: "⟺", leftrightarrow: "↔", Leftrightarrow: "⇔", mapsto: "↦", uparrow: "↑", downarrow: "↓",
  forall: "∀", exists: "∃", circ: "∘", star: "⋆", ast: "∗", bullet: "∙", oplus: "⊕", otimes: "⊗", perp: "⊥", parallel: "∥", mid: "∣", ll: "≪", gg: "≫", prec: "≺", succ: "≻",
  ldots: "…", cdots: "⋯", vdots: "⋮", ddots: "⋱", colon: ":", vert: "|", Vert: "‖",
  langle: "⟨", rangle: "⟩", lfloor: "⌊", rfloor: "⌋", lceil: "⌈", rceil: "⌉", lbrace: "{", rbrace: "}",
};
const IDENTIFIERS = { infty: "∞", partial: "∂", nabla: "∇", emptyset: "∅", varnothing: "∅", prime: "′", hbar: "ℏ", ell: "ℓ", Re: "ℜ", Im: "ℑ", aleph: "ℵ", angle: "∠", triangle: "△", top: "⊤", bot: "⊥" };
const BIG = { sum: "∑", prod: "∏", coprod: "∐", int: "∫", iint: "∬", iiint: "∭", oint: "∮", bigcup: "⋃", bigcap: "⋂", bigvee: "⋁", bigwedge: "⋀", bigoplus: "⨁", bigotimes: "⨂" };
const FUNCTIONS = ["sin", "cos", "tan", "cot", "sec", "csc", "arcsin", "arccos", "arctan", "sinh", "cosh", "tanh", "log", "ln", "lg", "exp", "det", "dim", "ker", "deg", "gcd", "arg", "hom", "Pr"];
const LIMITS = ["lim", "liminf", "limsup", "max", "min", "sup", "inf", "argmax", "argmin"];
const ACCENTS = { hat: "^", widehat: "^", bar: "¯", overline: "¯", vec: "→", tilde: "~", widetilde: "~", dot: "˙", ddot: "¨", check: "ˇ", breve: "˘", acute: "´", grave: "`" };
const FACES = { mathrm: "normal", textrm: "normal", mathbf: "bold", textbf: "bold", boldsymbol: "bold-italic", mathit: "italic", textit: "italic", mathbb: "double-struck", mathcal: "script", mathscr: "script", mathfrak: "fraktur", mathsf: "sans-serif", mathtt: "monospace", texttt: "monospace" };
const SPACES = { ",": "0.1667em", ":": "0.2222em", ">": "0.2222em", ";": "0.2778em", " ": "0.25em", quad: "1em", qquad: "2em", "!": "-0.1667em", enspace: "0.5em" };
const DELIMITERS = { "(": "(", ")": ")", "[": "[", "]": "]", "\\{": "{", "\\}": "}", "|": "|", "\\|": "‖", ".": "", "\\langle": "⟨", "\\rangle": "⟩", "\\lfloor": "⌊", "\\rfloor": "⌋", "\\lceil": "⌈", "\\rceil": "⌉", "\\vert": "|", "\\Vert": "‖", "/": "/" };
const MATRICES = { matrix: ["", ""], pmatrix: ["(", ")"], bmatrix: ["[", "]"], Bmatrix: ["{", "}"], vmatrix: ["|", "|"], Vmatrix: ["‖", "‖"], smallmatrix: ["", ""], cases: ["{", ""], aligned: ["", ""], align: ["", ""], "align*": ["", ""], gathered: ["", ""], array: ["", ""] };

// The tokens of `tex`: a command with its name, a letter, a digit run, a symbol, or a run of spaces.
function tokens(tex) {
  const found = [];
  const pattern = /\\([A-Za-z]+\*?|.)|(\d+(?:\.\d+)?)|(\s+)|([\s\S])/g;
  for (const match of tex.matchAll(pattern)) {
    if (match[1] !== undefined) {
      found.push({ command: match[1] });
    } else if (match[2] !== undefined) {
      found.push({ number: match[2] });
    } else if (match[3] !== undefined) {
      found.push({ space: true });
    } else {
      found.push({ char: match[4] });
    }
  }
  return found;
}

class Reader {
  constructor(tex, display) {
    this.list = tokens(tex);
    this.at = 0;
    this.display = display;
  }

  // The next token, spaces passed over: math takes no space it is written with, only text does.
  peek() {
    while (this.list[this.at]?.space) {
      this.at += 1;
    }
    return this.list[this.at];
  }

  next() {
    this.peek();
    return this.list[this.at++];
  }

  // A group's tokens as MathML, from just past its { to its }.
  group() {
    const token = this.peek();
    if (token?.char === "{") {
      this.next();
      return row(this.sequence((one) => one.char === "}"), true);
    }
    return this.atom() ?? "<mrow></mrow>";
  }

  // The text of a group as it was written, from just past its { to its }.
  rawGroup() {
    if (this.peek()?.char !== "{") {
      const one = this.next();
      return one?.char ?? one?.number ?? (one?.command ? `\\${one.command}` : "");
    }
    this.next();
    let depth = 1;
    let text = "";
    while (this.at < this.list.length && depth) {
      const one = this.list[this.at++];
      if (one.space) {
        text += " ";
        continue;
      }
      if (one.char === "{") {
        depth += 1;
      } else if (one.char === "}") {
        depth -= 1;
        if (!depth) {
          break;
        }
      }
      text += one.char ?? one.number ?? `\\${one.command}${/[A-Za-z]$/.test(one.command) ? " " : ""}`;
    }
    return text;
  }

  // Each item until `stop` says a token ends the run, which is taken; the items as MathML.
  sequence(stop) {
    const items = [];
    while (this.peek() && !stop(this.peek())) {
      const item = this.scripted();
      if (item !== null) {
        items.push(item);
      }
    }
    this.next();
    return items;
  }

  // An atom with the scripts after it.
  scripted() {
    const start = this.peek();
    const big = start?.command && (BIG[start.command] || LIMITS.includes(start.command));
    const base = this.atom();
    if (base === null) {
      return null;
    }
    let below = null;
    let above = null;
    while (this.peek()?.char === "_" || this.peek()?.char === "^") {
      const mark = this.next().char;
      const script = this.group();
      if (mark === "_") {
        below = script;
      } else {
        above = script;
      }
    }
    if (this.peek()?.char === "'" && !above) {
      let primes = "";
      while (this.peek()?.char === "'") {
        this.next();
        primes += "′";
      }
      above = `<mo>${primes}</mo>`;
    }
    const over = big && this.display && (BIG[start.command] ? !start.command.includes("int") : true);
    if (below && above) {
      return over ? `<munderover>${base}${below}${above}</munderover>` : `<msubsup>${base}${below}${above}</msubsup>`;
    }
    if (below) {
      return over ? `<munder>${base}${below}</munder>` : `<msub>${base}${below}</msub>`;
    }
    if (above) {
      return over ? `<mover>${base}${above}</mover>` : `<msup>${base}${above}</msup>`;
    }
    return base;
  }

  // One atom as MathML, or null for a token that draws nothing.
  atom() {
    const token = this.next();
    if (!token) {
      return null;
    }
    if (token.number !== undefined) {
      return `<mn>${token.number}</mn>`;
    }
    if (token.char !== undefined) {
      const char = token.char;
      if (char === "{") {
        return row(this.sequence((one) => one.char === "}"), true);
      }
      if (/[A-Za-z]/.test(char)) {
        return `<mi>${char}</mi>`;
      }
      if (char === "~") {
        return '<mspace width="0.25em"></mspace>';
      }
      return `<mo>${escape(char)}</mo>`;
    }
    return this.command(token.command);
  }

  command(name) {
    if (GREEK[name]) {
      return /^[A-Z]/.test(name) ? `<mi mathvariant="normal">${GREEK[name]}</mi>` : `<mi>${GREEK[name]}</mi>`;
    }
    if (OPERATORS[name]) {
      return `<mo>${escape(OPERATORS[name])}</mo>`;
    }
    if (IDENTIFIERS[name]) {
      return `<mi>${IDENTIFIERS[name]}</mi>`;
    }
    if (BIG[name]) {
      return `<mo largeop="true" movablelimits="${this.display ? "false" : "true"}">${BIG[name]}</mo>`;
    }
    if (FUNCTIONS.includes(name) || LIMITS.includes(name)) {
      return `<mi>${name}</mi>`;
    }
    if (SPACES[name] !== undefined) {
      return `<mspace width="${SPACES[name]}"></mspace>`;
    }
    if (name === "{" || name === "}" || name === "|" || name === "#" || name === "%" || name === "&" || name === "_" || name === "$") {
      return `<mo>${escape(name === "|" ? "‖" : name)}</mo>`;
    }
    if (name === "\\") {
      return null;
    }
    if (name === "frac" || name === "dfrac" || name === "tfrac") {
      return `<mfrac>${this.group()}${this.group()}</mfrac>`;
    }
    if (name === "binom") {
      return `<mrow><mo>(</mo><mfrac linethickness="0">${this.group()}${this.group()}</mfrac><mo>)</mo></mrow>`;
    }
    if (name === "sqrt") {
      if (this.peek()?.char === "[") {
        this.next();
        const index = row(this.sequence((one) => one.char === "]"), true);
        return `<mroot>${this.group()}${index}</mroot>`;
      }
      return `<msqrt>${this.group()}</msqrt>`;
    }
    if (ACCENTS[name]) {
      return `<mover accent="true">${this.group()}<mo>${escape(ACCENTS[name])}</mo></mover>`;
    }
    if (name === "underline") {
      return `<munder accentunder="true">${this.group()}<mo>_</mo></munder>`;
    }
    if (name === "text" || name === "textnormal" || name === "mbox") {
      return `<mtext>${escape(this.rawGroup())}</mtext>`;
    }
    if (name === "operatorname" || name === "operatorname*") {
      return `<mi>${escape(this.rawGroup())}</mi>`;
    }
    if (FACES[name]) {
      const inner = this.group();
      return `<mstyle mathvariant="${FACES[name]}">${inner.replace(/<mi>/g, `<mi mathvariant="${FACES[name]}">`)}</mstyle>`;
    }
    if (name === "left") {
      const open = this.delimiter();
      const inside = this.sequence((one) => one.command === "right");
      const close = this.delimiter();
      return `<mrow><mo fence="true" stretchy="true">${escape(open)}</mo>${inside.join("")}<mo fence="true" stretchy="true">${escape(close)}</mo></mrow>`;
    }
    if (["big", "Big", "bigg", "Bigg", "bigl", "bigr", "Bigl", "Bigr", "middle"].includes(name)) {
      return `<mo stretchy="true">${escape(this.delimiter())}</mo>`;
    }
    if (name === "begin") {
      return this.environment(this.rawGroup());
    }
    if (name === "displaystyle" || name === "textstyle" || name === "limits" || name === "nolimits" || name === "right" || name === "end") {
      return null;
    }
    return `<merror><mtext>\\${escape(name)}</mtext></merror>`;
  }

  delimiter() {
    const token = this.next();
    const written = token?.char ?? (token?.command ? `\\${token.command}` : ".");
    return DELIMITERS[written] ?? written;
  }

  environment(name) {
    if (name === "array" && this.peek()?.char === "{") {
      this.rawGroup();
    }
    const rows = [[[]]];
    while (this.peek()) {
      const token = this.peek();
      if (token.command === "end") {
        this.next();
        this.rawGroup();
        break;
      }
      if (token.command === "\\") {
        this.next();
        rows.push([[]]);
        continue;
      }
      if (token.char === "&") {
        this.next();
        rows.at(-1).push([]);
        continue;
      }
      const item = this.scripted();
      if (item !== null) {
        rows.at(-1).at(-1).push(item);
      }
    }
    while (rows.length > 1 && rows.at(-1).every((cell) => !cell.length)) {
      rows.pop();
    }
    const align = name.startsWith("align") ? ' columnalign="right left"' : name === "cases" ? ' columnalign="left left"' : "";
    const table = `<mtable${align}>${rows.map((cells) => `<mtr>${cells.map((cell) => `<mtd>${row(cell, true)}</mtd>`).join("")}</mtr>`).join("")}</mtable>`;
    const [open, close] = MATRICES[name] ?? ["", ""];
    if (!open && !close) {
      return table;
    }
    return `<mrow>${open ? `<mo fence="true" stretchy="true">${escape(open)}</mo>` : ""}${table}${close ? `<mo fence="true" stretchy="true">${escape(close)}</mo>` : ""}</mrow>`;
  }
}

function row(items, always = false) {
  return items.length === 1 && !always ? items[0] : `<mrow>${items.join("")}</mrow>`;
}

// `tex` as MathML, drawn as a block of its own where `display`, and in the line elsewhere.
export function texToMathml(tex, display = false) {
  const reader = new Reader(tex, display);
  const items = [];
  while (reader.peek()) {
    const item = reader.scripted();
    if (item !== null) {
      items.push(item);
    }
  }
  return `<math${display ? ' display="block"' : ""}><semantics>${row(items, true)}<annotation encoding="application/x-tex">${escape(tex)}</annotation></semantics></math>`;
}

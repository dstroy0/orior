// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Structural search and replace: code found by its shape, and each match written again from a
//! template.
//!
//! A pattern is code of the file's language with placeholders in it: `$x$` stands for an expression,
//! `$x:name$` for one name, and `$x:stmt$` for one statement or more. The pattern and the code are
//! each read as tokens, names, numbers, strings and operators, past spaces and comments: a match does
//! not hang on how the code is laid out. In Python a line's end outside brackets ends a statement,
//! and elsewhere a semicolon or a block's closing brace does. An expression is a run of tokens whose
//! brackets close inside it and that ends no statement; it takes as few tokens as let the rest of the
//! pattern match, and, last in the pattern, the rest of its statement. Statements are taken as few as
//! let the rest match, and one where they end the pattern. A placeholder named twice matches the same
//! code both times. Matches do not overlap, the first found in the text taken first.
//!
//! A template is text with the same placeholders, each written as the code its placeholder took.

use std::collections::HashMap;

use serde::Serialize;

use crate::servers::{Place, TextEdit};

#[derive(Clone, Copy, Debug, PartialEq)]
enum Kind {
    Name,
    Number,
    Str,
    Punct,
    /// The end of a statement at a line's end, in Python.
    Break,
}

#[derive(Clone, Debug)]
struct Tok {
    kind: Kind,
    start: usize,
    end: usize,
    from: Place,
    to: Place,
}

/// What a placeholder stands for.
#[derive(Clone, Copy, Debug, PartialEq)]
enum Hole {
    Name,
    Expr,
    Stmt,
}

/// A part of a pattern: a token to match as written, or a placeholder.
#[derive(Clone, Debug)]
enum Part {
    Word(Kind, String),
    Hole(String, Hole),
}

/// A match: its span, its first line as the file has it, and what the template writes in its place.
#[derive(Serialize, Clone, Debug)]
pub struct Found {
    pub path: String,
    pub from: Place,
    pub to: Place,
    pub line: String,
    pub with: Option<String>,
}

const OPERATORS: [&str; 40] = [
    ">>>=", "...", "**=", "//=", ">>=", "<<=", "===", "!==", "->", "=>", "::", "==", "!=", "<=", ">=", "&&", "||", "??", "?.", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "**", "//", "<<", ">>",
    "++", "--", ":=", "@=", "&&=", "||=", "??=", "..", "#!",
];

/// How a language comments: Python and the shells with `#`, the rest with `//` and `/* */`.
fn hash_comments(language: &str) -> bool {
    matches!(language, "python" | "shell" | "bash" | "powershell" | "r" | "ruby" | "yaml" | "toml" | "makefile" | "cmake")
}

/// The tokens of `src` in `language`, and, where `holes` is given, each placeholder as a name token
/// whose index is kept there.
fn tokens(language: &str, src: &str, mut holes: Option<&mut Vec<(usize, String, Hole)>>) -> Vec<Tok> {
    let python = language == "python";
    let hashes = hash_comments(language);
    let bytes: Vec<(usize, char)> = src.char_indices().collect();
    let mut out = Vec::new();
    let (mut line, mut col) = (0u32, 0u32);
    let mut at = 0;
    let mut depth = 0i32;
    let place_after = |line: u32, col: u32, c: char| if c == '\n' { (line + 1, 0) } else { (line, col + c.len_utf16() as u32) };
    while at < bytes.len() {
        let (start, c) = bytes[at];
        let from = Place { line, col };
        let step = |at: &mut usize, line: &mut u32, col: &mut u32| {
            let (next_line, next_col) = place_after(*line, *col, bytes[*at].1);
            *line = next_line;
            *col = next_col;
            *at += 1;
        };
        let next = |ahead: usize| bytes.get(at + ahead).map(|(_, c)| *c);
        if c == '\n' {
            if python && depth == 0 && out.last().is_some_and(|tok: &Tok| tok.kind != Kind::Break) {
                out.push(Tok { kind: Kind::Break, start, end: start + 1, from: from.clone(), to: Place { line: line + 1, col: 0 } });
            }
            step(&mut at, &mut line, &mut col);
            continue;
        }
        if c.is_whitespace() {
            step(&mut at, &mut line, &mut col);
            continue;
        }
        if python && c == '\\' && matches!(next(1), Some('\n' | '\r')) {
            step(&mut at, &mut line, &mut col);
            continue;
        }
        if (hashes && c == '#') || (!hashes && c == '/' && next(1) == Some('/')) {
            while at < bytes.len() && bytes[at].1 != '\n' {
                step(&mut at, &mut line, &mut col);
            }
            continue;
        }
        if !hashes && c == '/' && next(1) == Some('*') {
            step(&mut at, &mut line, &mut col);
            step(&mut at, &mut line, &mut col);
            while at < bytes.len() && !(bytes[at].1 == '*' && bytes.get(at + 1).is_some_and(|(_, c)| *c == '/')) {
                step(&mut at, &mut line, &mut col);
            }
            for _ in 0..2 {
                if at < bytes.len() {
                    step(&mut at, &mut line, &mut col);
                }
            }
            continue;
        }
        // A placeholder: `$name$` or `$name:kind$`.
        if c == '$' && holes.is_some() {
            let rest = &src[start + 1..];
            if let Some(close) = rest.find('$') {
                let inside = &rest[..close];
                let (name, kind) = inside.split_once(':').unwrap_or((inside, ""));
                if !name.is_empty() && name.chars().all(|c| c.is_alphanumeric() || c == '_') {
                    let hole = match kind {
                        "name" => Hole::Name,
                        "stmt" | "stmts" => Hole::Stmt,
                        _ => Hole::Expr,
                    };
                    let end = start + 1 + close + 1;
                    while at < bytes.len() && bytes[at].0 < end {
                        step(&mut at, &mut line, &mut col);
                    }
                    if let Some(holes) = holes.as_deref_mut() {
                        holes.push((out.len(), name.to_string(), hole));
                    }
                    out.push(Tok { kind: Kind::Name, start, end, from, to: Place { line, col } });
                    continue;
                }
            }
        }
        let kind;
        if c == '"' || c == '\'' || c == '`' || (python && c.is_alphabetic() && string_prefix(src, start)) {
            while bytes[at].1 != '"' && bytes[at].1 != '\'' {
                step(&mut at, &mut line, &mut col);
            }
            let quote = bytes[at].1;
            let triple = python && bytes.get(at + 1).is_some_and(|(_, c)| *c == quote) && bytes.get(at + 2).is_some_and(|(_, c)| *c == quote);
            let quote = if c == '`' { '`' } else { quote };
            for _ in 0..if triple { 3 } else { 1 } {
                step(&mut at, &mut line, &mut col);
            }
            while at < bytes.len() {
                let here = bytes[at].1;
                if here == '\\' {
                    step(&mut at, &mut line, &mut col);
                    if at < bytes.len() {
                        step(&mut at, &mut line, &mut col);
                    }
                    continue;
                }
                if here == quote && (!triple || (bytes.get(at + 1).is_some_and(|(_, c)| *c == quote) && bytes.get(at + 2).is_some_and(|(_, c)| *c == quote))) {
                    for _ in 0..if triple { 3 } else { 1 } {
                        step(&mut at, &mut line, &mut col);
                    }
                    break;
                }
                if here == '\n' && !triple && quote != '`' {
                    break;
                }
                step(&mut at, &mut line, &mut col);
            }
            kind = Kind::Str;
        } else if c.is_alphabetic() || c == '_' || c == '$' {
            while at < bytes.len() && (bytes[at].1.is_alphanumeric() || bytes[at].1 == '_' || bytes[at].1 == '$') {
                step(&mut at, &mut line, &mut col);
            }
            kind = Kind::Name;
        } else if c.is_ascii_digit() {
            while at < bytes.len() && (bytes[at].1.is_alphanumeric() || bytes[at].1 == '_' || bytes[at].1 == '.') {
                step(&mut at, &mut line, &mut col);
            }
            kind = Kind::Number;
        } else {
            let rest = &src[start..];
            let length = OPERATORS.iter().find(|op| rest.starts_with(**op)).map_or(1, |op| op.chars().count());
            for _ in 0..length {
                step(&mut at, &mut line, &mut col);
            }
            match c {
                '(' | '[' | '{' => depth += 1,
                ')' | ']' | '}' => depth = (depth - 1).max(0),
                _ => {}
            }
            kind = Kind::Punct;
        }
        let end = bytes.get(at).map_or(src.len(), |(byte, _)| *byte);
        out.push(Tok { kind, start, end, from, to: Place { line, col } });
    }
    out
}

/// Whether the name at `start` is a Python string's prefix: up to two of r, b, f, u and t before a
/// quote.
fn string_prefix(src: &str, start: usize) -> bool {
    let rest = &src[start..];
    let prefix: String = rest.chars().take_while(|c| c.is_alphabetic()).collect();
    prefix.len() <= 2 && prefix.chars().all(|c| "rRbBfFuUtT".contains(c)) && rest[prefix.len()..].starts_with(['"', '\''])
}

/// A pattern read as its parts.
fn pattern_of(language: &str, pattern: &str) -> Vec<Part> {
    let mut holes = Vec::new();
    let toks = tokens(language, pattern.trim(), Some(&mut holes));
    let src = pattern.trim();
    let mut parts: Vec<Part> = toks.iter().map(|tok| Part::Word(tok.kind, src[tok.start..tok.end].to_string())).collect();
    for (at, name, hole) in holes {
        parts[at] = Part::Hole(name, hole);
    }
    while matches!(parts.last(), Some(Part::Word(Kind::Break, _))) {
        parts.pop();
    }
    parts
}

/// Whether a token opens or closes a bracket, by how much.
fn depth_of(text: &str) -> i32 {
    match text {
        "(" | "[" | "{" => 1,
        ")" | "]" | "}" => -1,
        _ => 0,
    }
}

/// Whether a token ends a statement where it stands outside brackets.
fn ends_statement(tok: &Tok, text: &str) -> bool {
    tok.kind == Kind::Break || (tok.kind == Kind::Punct && text == ";")
}

struct Matcher<'a> {
    src: &'a str,
    toks: &'a [Tok],
    parts: &'a [Part],
    steps: usize,
}

/// The most steps a match from one place may take before it is given up.
const MOST_STEPS: usize = 20_000;

impl Matcher<'_> {
    fn text(&self, at: usize) -> &str {
        &self.src[self.toks[at].start..self.toks[at].end]
    }

    /// The text a run of tokens holds, as the file has it.
    fn span_text(&self, from: usize, to: usize) -> &str {
        if from >= to {
            return "";
        }
        &self.src[self.toks[from].start..self.toks[to - 1].end]
    }

    /// The ends after `at` a placeholder of `hole` may reach, in the order they are tried.
    fn ends(&self, hole: Hole, at: usize, last: bool) -> Vec<usize> {
        let mut ends = Vec::new();
        let mut depth = 0;
        let mut at_end = at;
        while at_end < self.toks.len() {
            let text = self.text(at_end);
            let tok = &self.toks[at_end];
            let change = depth_of(text);
            if change < 0 && depth == 0 {
                break;
            }
            match hole {
                Hole::Name => {
                    if tok.kind == Kind::Name {
                        ends.push(at_end + 1);
                    }
                    break;
                }
                Hole::Expr => {
                    if depth == 0 && ends_statement(tok, text) {
                        break;
                    }
                    depth += change;
                    if depth == 0 {
                        ends.push(at_end + 1);
                    }
                }
                Hole::Stmt => {
                    depth += change;
                    if depth == 0 && (ends_statement(tok, text) || (text == "}" && change < 0)) {
                        ends.push(at_end + 1);
                        if last {
                            break;
                        }
                    }
                }
            }
            at_end += 1;
        }
        if hole == Hole::Stmt && depth == 0 && at_end > at && ends.last() != Some(&at_end) {
            ends.push(at_end);
        }
        if hole == Hole::Expr && last {
            ends.reverse();
        }
        ends
    }

    /// The end of a match of the parts from `part` on at the token `at`, with the placeholders bound
    /// so far.
    fn from(&mut self, part: usize, at: usize, bound: &mut HashMap<String, (usize, usize)>) -> Option<usize> {
        self.steps += 1;
        if self.steps > MOST_STEPS {
            return None;
        }
        let Some(want) = self.parts.get(part) else {
            return Some(at);
        };
        match want {
            Part::Word(kind, text) => {
                if at < self.toks.len() && self.toks[at].kind == *kind && (*kind == Kind::Break || self.text(at) == text) {
                    self.from(part + 1, at + 1, bound)
                } else {
                    None
                }
            }
            Part::Hole(name, hole) => {
                if let Some(&(from, to)) = bound.get(name) {
                    let length = to - from;
                    let same = at + length <= self.toks.len() && (0..length).all(|offset| self.text(from + offset) == self.text(at + offset));
                    return if same { self.from(part + 1, at + length, bound) } else { None };
                }
                let last = part + 1 == self.parts.len();
                for end in self.ends(*hole, at, last) {
                    bound.insert(name.clone(), (at, end));
                    if let Some(found) = self.from(part + 1, end, bound) {
                        return Some(found);
                    }
                    bound.remove(name);
                }
                None
            }
        }
    }
}

/// Writes a template with each placeholder as the code it took.
fn fill(template: &str, matcher: &Matcher, bound: &HashMap<String, (usize, usize)>) -> String {
    let mut out = String::new();
    let mut rest = template;
    while let Some(open) = rest.find('$') {
        out.push_str(&rest[..open]);
        let after = &rest[open + 1..];
        match after.find('$') {
            Some(close) => {
                let inside = &after[..close];
                let name = inside.split(':').next().unwrap_or_default();
                match bound.get(name) {
                    Some(&(from, to)) => {
                        let mut to = to;
                        while to > from && matcher.toks[to - 1].kind == Kind::Break {
                            to -= 1;
                        }
                        out.push_str(matcher.span_text(from, to));
                        rest = &after[close + 1..];
                    }
                    None => {
                        out.push('$');
                        rest = after;
                    }
                }
            }
            None => {
                out.push('$');
                rest = after;
            }
        }
    }
    out.push_str(rest);
    out
}

/// Every match of `pattern` in `src`, a file of `language` at `path`, each with what `template`
/// writes in its place where one is given; and the edits that write them.
pub fn search(language: &str, path: &str, src: &str, pattern: &str, template: Option<&str>) -> (Vec<Found>, Vec<TextEdit>) {
    let parts = pattern_of(language, pattern);
    if parts.is_empty() {
        return (Vec::new(), Vec::new());
    }
    let toks = tokens(language, src, None);
    let mut matcher = Matcher { src, toks: &toks, parts: &parts, steps: 0 };
    let mut found = Vec::new();
    let mut edits = Vec::new();
    let mut at = 0;
    while at < toks.len() {
        if toks[at].kind == Kind::Break {
            at += 1;
            continue;
        }
        matcher.steps = 0;
        let mut bound = HashMap::new();
        let Some(mut end) = matcher.from(0, at, &mut bound) else {
            at += 1;
            continue;
        };
        while end > at && toks[end - 1].kind == Kind::Break {
            end -= 1;
        }
        if end == at {
            at += 1;
            continue;
        }
        let (from, to) = (toks[at].from.clone(), toks[end - 1].to.clone());
        let with = template.map(|template| fill(template, &matcher, &bound));
        let line = src.split('\n').nth(from.line as usize).unwrap_or_default().trim_end().to_string();
        if let Some(with) = &with {
            edits.push(TextEdit { from: from.clone(), to: to.clone(), text: with.clone() });
        }
        found.push(Found { path: path.to_string(), from, to, line, with });
        at = end;
    }
    (found, edits)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn replaced(language: &str, src: &str, pattern: &str, template: &str) -> String {
        let (_, edits) = search(language, "f", src, pattern, Some(template));
        crate::servers::apply(src, &edits)
    }

    #[test]
    fn an_expression_placeholder_takes_a_call_whole_and_its_text_is_written_back() {
        let src = "x = len( items ) + len(other(1, 2))\nprint(len(z))\n";
        let (found, _) = search("python", "f", src, "len($a$)", None);
        assert_eq!(found.iter().map(|one| (one.from.line, one.from.col, one.to.col)).collect::<Vec<_>>(), vec![(0, 4, 16), (0, 19, 35), (1, 6, 12)]);
        assert_eq!(replaced("python", src, "len($a$)", "count($a$)"), "x = count(items) + count(other(1, 2))\nprint(count(z))\n");
    }

    #[test]
    fn a_name_placeholder_takes_one_name_and_one_named_twice_takes_the_same_both_times() {
        let src = "a = a + 1\nb = c + 1\nd.e = d.e + 1\n";
        assert_eq!(replaced("python", src, "$x:name$ = $x:name$ + 1", "$x$ += 1"), "a += 1\nb = c + 1\nd.e = d.e + 1\n");
        assert_eq!(replaced("python", src, "$x$ = $x$ + 1", "$x$ += 1"), "a += 1\nb = c + 1\nd.e += 1\n");
    }

    #[test]
    fn statements_and_layout_match_in_javascript_past_spaces_and_comments() {
        let src = "if (ready) {\n  start(); // go\n  log(\"started\");\n}\nif(done){finish();}\n";
        let found = search("javascript", "f", src, "if ($c$) { $body:stmt$ }", None).0;
        assert_eq!(found.len(), 2);
        assert_eq!(
            replaced("javascript", src, "if ($c$) { $body:stmt$ }", "when($c$, () => { $body$ })"),
            "when(ready, () => { start(); // go\n  log(\"started\"); })\nwhen(done, () => { finish(); })\n"
        );
    }

    #[test]
    fn an_expression_last_in_a_pattern_takes_the_rest_of_its_statement_and_strings_are_one_token() {
        let src = "total = a + b * c\nname = \"a + b\"\n";
        assert_eq!(replaced("python", src, "total = $v$", "total = ($v$)"), "total = (a + b * c)\nname = \"a + b\"\n");
        assert!(search("python", "f", src, "a + b", None).0.len() == 1);
    }
}

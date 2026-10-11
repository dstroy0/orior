// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! JavaScript as the inspections read it: its tokens, a string, a template's text and a regular
//! expression each one token, and the code in a template's `${}` tokens of its own. A slash starts a
//! regular expression where a value is due, after an operator, an opening bracket or a word such as
//! `return`, and divides elsewhere.
//!
//! Each brace is a function's body, a block, an object or a class's body, by what comes before it:
//! `=>` or a parameter list after a name or `function` opens a function, `class` a class's body, an
//! operator or an opening bracket an object, and anything else a block.
//!
//! What one file says alone comes from here: a `const`, `let` or `var` a function declares and never
//! reads, and code after a return, a throw, a break or a continue in the same block; and, for the
//! tree's checks, the files it imports by a relative path.

use std::collections::HashSet;

use super::{Facts, Finding, Fix, Import, Paint, Parsed, Scan, Severity, COMMENT, DELIMITER, FUNCTION, KEYWORD, NUMBER, OPERATOR, PARAMETER, PREDEFINED, PROPERTY, STRING, TYPE};
use crate::servers::{Place, TextEdit};

#[derive(Clone, Copy, Debug, PartialEq)]
enum Kind {
    Name,
    Number,
    Str,
    Regex,
    Punct,
}

#[derive(Clone, Debug)]
struct Tok {
    kind: Kind,
    start: usize,
    end: usize,
    from: Place,
    to: Place,
}

/// What a brace opens.
#[derive(Clone, Copy, Debug, PartialEq)]
enum Brace {
    Function,
    Block,
    Object,
    Class,
}

const PUNCTS: [&str; 52] = [
    ">>>=", "...", "===", "!==", "**=", "<<=", ">>=", ">>>", "&&=", "||=", "??=", "=>", "==", "!=", "<=", ">=", "&&", "||", "??", "?.", "++", "--", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=",
    "**", "<<", ">>", "+", "-", "*", "/", "%", "&", "|", "^", "!", "~", "<", ">", "=", "?", ":", ";", ",", ".", "@",
];

/// The words after which a slash starts a regular expression.
const BEFORE_VALUE: [&str; 14] = ["return", "typeof", "instanceof", "in", "of", "new", "delete", "void", "throw", "case", "do", "else", "yield", "await"];

/// Words that are never names a function reads.
const KEYWORDS: [&str; 35] = [
    "break", "case", "catch", "class", "const", "continue", "debugger", "default", "delete", "do", "else", "export", "extends", "finally", "for", "function", "if", "import", "in", "instanceof",
    "let", "new", "return", "super", "switch", "this", "throw", "try", "typeof", "var", "void", "while", "with", "yield", "await",
];

/// The words before a parameter list that are no function's name.
const CONTROL: [&str; 7] = ["if", "for", "while", "switch", "catch", "with", "return"];

fn is_name_start(c: char) -> bool {
    c == '_' || c == '$' || c.is_alphabetic()
}

fn is_name(c: char) -> bool {
    c == '_' || c == '$' || c.is_alphanumeric()
}

/// The tokens of `src`, and the spans of its comments.
fn tokens(src: &str) -> (Vec<Tok>, Vec<(Place, Place)>) {
    let mut s = Scan::new(src);
    let mut toks = Vec::new();
    lex(&mut s, &mut toks, false);
    (toks, s.comments)
}

/// Reads tokens into `toks` until the text ends or, `inside` a template's `${`, until its `}`.
fn lex(s: &mut Scan, toks: &mut Vec<Tok>, inside: bool) {
    let src = s.src;
    let mut depth = 0i32;
    while let Some(c) = s.peek() {
        let (start, from) = (s.pos, s.place());
        match c {
            ' ' | '\t' | '\r' | '\n' | '\u{feff}' | '\u{a0}' => {
                s.bump();
                continue;
            }
            '/' if s.peek_at(1) == Some('/') => {
                s.skip_line();
                continue;
            }
            '/' if s.peek_at(1) == Some('*') => {
                s.bump();
                s.bump();
                while let Some(c) = s.peek() {
                    if c == '*' && s.peek_at(1) == Some('/') {
                        s.bump();
                        s.bump();
                        break;
                    }
                    s.bump();
                }
                s.comments.push((from, s.place()));
                continue;
            }
            '\'' | '"' => {
                s.bump();
                while let Some(c) = s.peek() {
                    s.bump();
                    if c == '\\' {
                        s.bump();
                    } else if c == quote_of(src, start) || c == '\n' {
                        break;
                    }
                }
                toks.push(Tok { kind: Kind::Str, start, end: s.pos, from, to: s.place() });
            }
            '`' => {
                s.bump();
                while let Some(c) = s.peek() {
                    if c == '\\' {
                        s.bump();
                        s.bump();
                    } else if c == '`' {
                        s.bump();
                        break;
                    } else if c == '$' && s.peek_at(1) == Some('{') {
                        s.bump();
                        s.bump();
                        lex(s, toks, true);
                    } else {
                        s.bump();
                    }
                }
                toks.push(Tok { kind: Kind::Str, start, end: s.pos, from, to: s.place() });
            }
            '/' if value_due(src, toks) => {
                s.bump();
                let mut class = false;
                while let Some(c) = s.peek() {
                    s.bump();
                    match c {
                        '\\' => {
                            s.bump();
                        }
                        '[' => class = true,
                        ']' => class = false,
                        '/' if !class => break,
                        '\n' => break,
                        _ => {}
                    }
                }
                while s.peek().is_some_and(is_name) {
                    s.bump();
                }
                toks.push(Tok { kind: Kind::Regex, start, end: s.pos, from, to: s.place() });
            }
            c if is_name_start(c) || c == '#' => {
                s.bump();
                while s.peek().is_some_and(is_name) {
                    s.bump();
                }
                toks.push(Tok { kind: Kind::Name, start, end: s.pos, from, to: s.place() });
            }
            c if c.is_ascii_digit() || (c == '.' && s.peek_at(1).is_some_and(|next| next.is_ascii_digit())) => {
                while s.peek().is_some_and(|c| c.is_alphanumeric() || c == '_' || c == '.') {
                    s.bump();
                }
                toks.push(Tok { kind: Kind::Number, start, end: s.pos, from, to: s.place() });
            }
            '(' | '[' | '{' | ')' | ']' | '}' => {
                if inside && c == '}' && depth == 0 {
                    s.bump();
                    return;
                }
                match c {
                    '(' | '[' | '{' => depth += 1,
                    _ => depth -= 1,
                }
                s.bump();
                toks.push(Tok { kind: Kind::Punct, start, end: s.pos, from, to: s.place() });
            }
            _ => {
                let rest = &src[start..];
                let length = PUNCTS.iter().find(|punct| rest.starts_with(**punct)).map_or(1, |punct| punct.chars().count());
                for _ in 0..length {
                    s.bump();
                }
                toks.push(Tok { kind: Kind::Punct, start, end: s.pos, from, to: s.place() });
            }
        }
    }
}

fn quote_of(src: &str, start: usize) -> char {
    src[start..].chars().next().unwrap_or('"')
}

/// Whether a value is due after the tokens so far, and a slash starts a regular expression.
fn value_due(src: &str, toks: &[Tok]) -> bool {
    match toks.last() {
        None => true,
        Some(tok) => {
            let word = &src[tok.start..tok.end];
            match tok.kind {
                Kind::Punct => !matches!(word, ")" | "]" | "}"),
                Kind::Name => BEFORE_VALUE.contains(&word),
                _ => false,
            }
        }
    }
}

/// The tokens, their words, the bracket each bracket matches, and what each brace opens.
struct Read<'a> {
    src: &'a str,
    toks: Vec<Tok>,
    words: Vec<&'a str>,
    pair: Vec<Option<usize>>,
    braces: Vec<Option<Brace>>,
    /// The brace each token stands directly inside, where one does.
    within: Vec<Option<usize>>,
    comments: Vec<(Place, Place)>,
}

impl<'a> Read<'a> {
    fn new(src: &'a str) -> Read<'a> {
        let (toks, comments) = tokens(src);
        let words: Vec<&str> = toks.iter().map(|tok| &src[tok.start..tok.end]).collect();
        let mut pair = vec![None; toks.len()];
        let mut stack: Vec<usize> = Vec::new();
        let mut within = vec![None; toks.len()];
        for at in 0..toks.len() {
            within[at] = stack.iter().rev().find(|open| words[**open] == "{").copied();
            if toks[at].kind != Kind::Punct {
                continue;
            }
            match words[at] {
                "(" | "[" | "{" => stack.push(at),
                ")" | "]" | "}" => {
                    if let Some(open) = stack.pop() {
                        pair[open] = Some(at);
                        pair[at] = Some(open);
                    }
                }
                _ => {}
            }
        }
        let mut read = Read { src, toks, words, pair, braces: Vec::new(), within, comments };
        read.braces = (0..read.toks.len()).map(|at| (read.words[at] == "{" && read.toks[at].kind == Kind::Punct).then(|| read.brace(at))).collect();
        read
    }

    /// What the brace at `at` opens.
    fn brace(&self, at: usize) -> Brace {
        let Some(before) = at.checked_sub(1) else {
            return Brace::Block;
        };
        let word = self.words[before];
        if word == "=>" {
            return Brace::Function;
        }
        if word == ")" {
            if let Some(open) = self.pair[before] {
                let name = open.checked_sub(1).map(|at| self.words[at]);
                return match name {
                    Some("function") => Brace::Function,
                    Some(name) if self.toks[open - 1].kind == Kind::Name && !CONTROL.contains(&name) => Brace::Function,
                    _ => Brace::Block,
                };
            }
        }
        // `class Name {` and `class Name extends Base {`.
        for back in (at.saturating_sub(8)..at).rev() {
            if self.words[back] == "class" && self.toks[back].kind == Kind::Name {
                return Brace::Class;
            }
            if matches!(self.words[back], ";" | "{" | "}") {
                break;
            }
        }
        if self.toks[before].kind == Kind::Punct && !matches!(word, ")" | "]" | "}" | ";") || matches!(word, "return" | "yield" | "await" | "case" | "default" | "in" | "of") {
            return Brace::Object;
        }
        Brace::Block
    }

    /// Whether the name at `at` is read: no keyword, no property after a dot, no key of an object and
    /// no method's name in an object or a class's body.
    fn is_read(&self, at: usize) -> bool {
        if self.toks[at].kind != Kind::Name || KEYWORDS.contains(&self.words[at]) || self.words[at].starts_with('#') {
            return false;
        }
        let before = at.checked_sub(1).map(|at| self.words[at]);
        if matches!(before, Some("." | "?.")) {
            return false;
        }
        let after = self.words.get(at + 1).copied();
        let holder = self.within[at].and_then(|open| self.braces[open]);
        if holder == Some(Brace::Object) && matches!(before, Some("{" | ",")) && matches!(after, Some(":" | "(")) {
            return false;
        }
        !(holder == Some(Brace::Class) && after == Some("(") && matches!(before, Some("{" | "}" | ";" | "static" | "async" | "get" | "set" | "*")))
    }
}

/// What one JavaScript file says alone, and the files it imports.
pub(super) fn facts(src: &str) -> Facts {
    let read = Read::new(src);
    let mut local = Vec::new();
    for at in 0..read.toks.len() {
        match read.braces[at] {
            Some(Brace::Function) => {
                unread(&read, at, &mut local);
                unreachable(&read, at, &mut local);
            }
            Some(Brace::Block) => unreachable(&read, at, &mut local),
            _ => {}
        }
    }
    Facts { imports: imports(&read), classes: Vec::new(), functions: functions(&read), local }
}

/// The span of the lines from the line of `from` to the line of `to` whole, where nothing else stands
/// on them.
fn whole_lines(src: &str, from: &Tok, to: &Tok) -> Option<(Place, Place)> {
    let line_start = src[..from.start].rfind('\n').map_or(0, |at| at + 1);
    let line_end = src[to.end..].find('\n').map_or(src.len(), |at| to.end + at);
    if !src[line_start..from.start].trim().is_empty() || !src[to.end..line_end].trim().is_empty() {
        return None;
    }
    if line_end < src.len() {
        Some((Place { line: from.from.line, col: 0 }, Place { line: to.to.line + 1, col: 0 }))
    } else {
        Some((Place { line: from.from.line, col: 0 }, to.to.clone()))
    }
}

/// The `const`, `let` and `var` the function whose body opens at `open` declares and never reads.
fn unread(read: &Read, open: usize, found: &mut Vec<Finding>) {
    let Some(close) = read.pair[open] else {
        return;
    };
    let mut declared = Vec::new();
    let mut reads: HashSet<&str> = HashSet::new();
    let mut nested_end = 0;
    for at in open + 1..close {
        if at > nested_end && read.braces[at] == Some(Brace::Function) {
            nested_end = read.pair[at].unwrap_or(at);
        }
        let direct = at > nested_end;
        let word = read.words[at];
        if direct && matches!(word, "const" | "let" | "var") && read.toks.get(at + 1).is_some_and(|tok| tok.kind == Kind::Name) && matches!(read.words.get(at + 2).copied(), Some("=" | ";")) && !(at >= 2 && read.words[at - 1] == "(" && read.words[at - 2] == "for") {
            declared.push(at + 1);
            continue;
        }
        if read.is_read(at) && !declared.contains(&at) {
            reads.insert(word);
        }
    }
    if reads.contains("eval") {
        return;
    }
    for name_at in declared {
        let name = read.words[name_at];
        if name.starts_with('_') || reads.contains(name) {
            continue;
        }
        let tok = &read.toks[name_at];
        found.push(Finding {
            from: tok.from.clone(),
            to: tok.to.clone(),
            severity: Severity::Warning,
            message: format!("`{name}` is declared and never read."),
            code: "unread",
            href: None,
            fixes: declaration_fix(read, name_at).into_iter().collect(),
        });
    }
}

/// The fix that takes out an unread declaration of one name, given a value and ended by a semicolon
/// on its own lines: the declaration with its lines, or, where its value calls something, all but
/// its value.
fn declaration_fix(read: &Read, name_at: usize) -> Option<Fix> {
    let keyword = name_at - 1;
    if read.words.get(name_at + 1) != Some(&"=") {
        return None;
    }
    let mut end = name_at + 2;
    while end < read.toks.len() && read.words[end] != ";" {
        if read.words[end] == "," && read.within[end] == read.within[name_at] && read.pair_depth(name_at + 2, end) == 0 {
            return None;
        }
        end = read.pair[end].filter(|close| *close > end && matches!(read.words[end], "(" | "[" | "{")).map_or(end + 1, |close| close + 1);
    }
    if end >= read.toks.len() {
        return None;
    }
    let calls = (name_at + 2..end).any(|at| read.words[at] == "(");
    let name = read.words[name_at];
    if calls {
        return Some(Fix {
            title: format!("Take out `{} {name} =` and keep its value", read.words[keyword]),
            edits: vec![TextEdit { from: read.toks[keyword].from.clone(), to: read.toks[name_at + 2].from.clone(), text: String::new() }],
        });
    }
    let (from, to) = whole_lines(read.src, &read.toks[keyword], &read.toks[end])?;
    Some(Fix { title: format!("Take out the declaration of {name}"), edits: vec![TextEdit { from, to, text: String::new() }] })
}

impl Read<'_> {
    /// How many brackets the tokens from `from` up to `to` leave open.
    fn pair_depth(&self, from: usize, to: usize) -> i32 {
        let mut depth = 0;
        for at in from..to {
            match self.words[at] {
                "(" | "[" | "{" => depth += 1,
                ")" | "]" | "}" => depth -= 1,
                _ => {}
            }
        }
        depth
    }
}

/// Code after a return, a throw, a break or a continue that stands directly in the block opening at
/// `open`, up to the block's end or its switch's next label; a function declared there is left out,
/// as it is made before the block runs.
fn unreachable(read: &Read, open: usize, found: &mut Vec<Finding>) {
    let Some(close) = read.pair[open] else {
        return;
    };
    let mut at = open + 1;
    while at < close {
        if read.within[at] != Some(open) || !matches!(read.words[at], "return" | "throw" | "break" | "continue") || read.toks[at].kind != Kind::Name || !matches!(read.words[at - 1], "{" | ";" | "}" | ":") {
            at = read.pair[at].filter(|end| *end > at).map_or(at + 1, |end| end + 1);
            continue;
        }
        let word = read.words[at];
        let mut end = at + 1;
        while end < close && read.words[end] != ";" {
            end = read.pair[end].filter(|pair| *pair > end && matches!(read.words[end], "(" | "[" | "{")).map_or(end + 1, |pair| pair + 1);
        }
        let first = end + 1;
        if end >= close || first >= close || matches!(read.words[first], "case" | "default" | "function" | "}") {
            at = first;
            continue;
        }
        let mut last = first;
        let mut next = first;
        while next < close && !(read.within[next] == Some(open) && matches!(read.words[next], "case" | "default")) {
            last = read.pair[next].filter(|pair| *pair > next).unwrap_or(next);
            next = last + 1;
        }
        let fixes = whole_lines(read.src, &read.toks[first], &read.toks[last])
            .map(|(from, to)| Fix { title: "Take out the code no path reaches".into(), edits: vec![TextEdit { from, to, text: String::new() }] })
            .into_iter()
            .collect();
        found.push(Finding {
            from: read.toks[first].from.clone(),
            to: read.toks[last].to.clone(),
            severity: Severity::Warning,
            message: format!("No path reaches this code: the `{word}` before it leaves the block."),
            code: "unreachable",
            href: None,
            fixes,
        });
        at = next;
    }
}

/// The name of the function whose body opens at `open`, and the index of its token: `function name(`,
/// a method's `name(`, and `name = function (` or `name = (…) =>` and `name = x =>`.
fn function_name(read: &Read, open: usize) -> Option<usize> {
    let before = open.checked_sub(1)?;
    let named_by_assignment = |at: Option<usize>| at.filter(|at| *at >= 2 && matches!(read.words[*at - 1], "=" | ":") && read.toks[*at - 2].kind == Kind::Name).map(|at| at - 2);
    if read.words[before] == "=>" {
        let params = before.checked_sub(1)?;
        let start = if read.words[params] == ")" { read.pair[params]? } else { params };
        let start = if start >= 1 && read.words[start - 1] == "async" { start - 1 } else { start };
        return named_by_assignment(Some(start));
    }
    let paren = read.pair[before].filter(|_| read.words[before] == ")")?;
    let name = paren.checked_sub(1)?;
    if read.words[name] == "function" {
        let start = if name >= 1 && read.words[name - 1] == "async" { name - 1 } else { name };
        return named_by_assignment(Some(start));
    }
    (read.toks[name].kind == Kind::Name).then_some(name)
}

/// The functions a file declares, with a name, and the calls in each outside the functions it holds:
/// a name before a bracket, after no dot or after `this.`.
fn functions(read: &Read) -> Vec<super::Function> {
    let mut out = Vec::new();
    for open in 0..read.toks.len() {
        if read.braces[open] != Some(Brace::Function) {
            continue;
        }
        let (Some(name_at), Some(close)) = (function_name(read, open), read.pair[open]) else {
            continue;
        };
        let mut calls = Vec::new();
        let mut at = open + 1;
        while at < close {
            if read.braces[at] == Some(Brace::Function) {
                at = read.pair[at].map_or(at + 1, |end| end + 1);
                continue;
            }
            let word = read.words[at];
            let before = at.checked_sub(1).map(|one| read.words[one]);
            let owner = at.checked_sub(2).map(|one| read.words[one]);
            let declares = read.pair[at + 1].and_then(|end| read.words.get(end + 1)).is_some_and(|next| *next == "{");
            if read.toks[at].kind == Kind::Name && read.words.get(at + 1) == Some(&"(") && !KEYWORDS.contains(&word) && !CONTROL.contains(&word) && !declares && (before != Some(".") || owner == Some("this")) && before != Some("function") {
                calls.push((word.to_string(), read.toks[at].from.clone(), read.toks[at].to.clone()));
            }
            at += 1;
        }
        let name = &read.toks[name_at];
        let first = read.toks[name_at.min(open)].from.line;
        out.push(super::Function { name: read.words[name_at].to_string(), from: name.from.clone(), to: name.to.clone(), first, last: read.toks[close].to.line, calls });
    }
    out
}

/// The names a function whose body opens at `open` takes as its parameters, plainly or with a
/// default or a rest, and the first token of its parameters.
fn parameters<'a>(read: &Read<'a>, open: usize) -> (HashSet<&'a str>, usize) {
    let mut names = HashSet::new();
    let Some(before) = open.checked_sub(1) else {
        return (names, open);
    };
    let (first, last) = match read.words[before] {
        ")" => (read.pair[before].unwrap_or(before), before),
        "=>" => match before.checked_sub(1) {
            Some(at) if read.words[at] == ")" => (read.pair[at].unwrap_or(at), at),
            Some(at) => (at, at + 1),
            None => (before, before),
        },
        _ => return (names, open),
    };
    if read.words[before] == "=>" && last == first + 1 && read.toks[first].kind == Kind::Name {
        names.insert(read.words[first]);
        return (names, first);
    }
    for at in first + 1..last {
        if read.toks[at].kind == Kind::Name && read.pair_depth(first + 1, at) == 0 && matches!(read.words[at - 1], "(" | "," | "...") && matches!(read.words.get(at + 1).copied(), Some("," | ")" | "=")) {
            names.insert(read.words[at]);
        }
    }
    (names, first)
}

/// The parse of a JavaScript file: each column's class, the regions that fold, and the spans Expand
/// Selection steps through.
pub(super) fn parse(src: &str) -> Parsed {
    let read = Read::new(src);
    let mut paint = Paint::new(src);
    let mut folds = Vec::new();
    let mut ranges = Vec::new();
    for (from, to) in &read.comments {
        paint.span(from, to, COMMENT);
        folds.push((from.line, to.line));
        ranges.push((from.clone(), to.clone()));
    }
    // Strings first, as a template's own code is read inside it and drawn over it.
    for tok in read.toks.iter().filter(|tok| matches!(tok.kind, Kind::Str | Kind::Regex)) {
        paint.span(&tok.from, &tok.to, STRING);
        folds.push((tok.from.line, tok.to.line));
        ranges.push((tok.from.clone(), tok.to.clone()));
        if tok.end - tok.start >= 2 && tok.kind == Kind::Str {
            ranges.push((Place { line: tok.from.line, col: tok.from.col + 1 }, Place { line: tok.to.line, col: tok.to.col - 1 }));
        }
    }
    let classes: HashSet<&str> = (1..read.toks.len()).filter(|at| read.words[at - 1] == "class" && read.toks[*at].kind == Kind::Name).map(|at| read.words[at]).collect();
    let scopes: Vec<(usize, usize, HashSet<&str>)> = (0..read.toks.len())
        .filter(|at| read.braces[*at] == Some(Brace::Function))
        .map(|open| {
            let (names, first) = parameters(&read, open);
            (first, read.pair[open].unwrap_or(open), names)
        })
        .collect();
    for (at, tok) in read.toks.iter().enumerate() {
        let word = read.words[at];
        let before = at.checked_sub(1).map(|one| read.words[one]);
        let after = read.words.get(at + 1).copied();
        let class = match tok.kind {
            Kind::Str | Kind::Regex => continue,
            Kind::Number => NUMBER,
            Kind::Punct => {
                if let (Some(close), "(" | "[" | "{") = (read.pair[at], word) {
                    if close > at {
                        let closed = &read.toks[close];
                        ranges.push((tok.to.clone(), closed.from.clone()));
                        ranges.push((tok.from.clone(), closed.to.clone()));
                        folds.push((tok.from.line, closed.to.line));
                    }
                }
                if matches!(word, "(" | ")" | "[" | "]" | "{" | "}" | "," | ";" | "." | "?.") { DELIMITER } else { OPERATOR }
            }
            Kind::Name => {
                let holder = read.within[at].and_then(|open| read.braces[open]);
                if matches!(word, "true" | "false" | "null" | "undefined" | "this" | "super") {
                    PREDEFINED
                } else if KEYWORDS.contains(&word) || matches!(word, "async" | "of" | "static" | "get" | "set") && after.is_some_and(|next| !matches!(next, "(" | "=" | "." | "," | ")" | ";" | ":")) {
                    KEYWORD
                } else if before == Some("function") {
                    FUNCTION
                } else if matches!(before, Some("class" | "new" | "extends")) {
                    TYPE
                } else if matches!(before, Some("." | "?.")) {
                    if after == Some("(") { FUNCTION } else { PROPERTY }
                } else if holder == Some(Brace::Object) && matches!(before, Some("{" | ",")) && after == Some(":") {
                    PROPERTY
                } else if after == Some("(") {
                    if classes.contains(word) { TYPE } else { FUNCTION }
                } else if scopes.iter().any(|(first, close, names)| *first <= at && at <= *close && names.contains(word)) {
                    PARAMETER
                } else if classes.contains(word) {
                    TYPE
                } else {
                    0
                }
            }
        };
        paint.span(&tok.from, &tok.to, class);
    }
    // A function declared, or a class, from its first word to its closing brace.
    for open in 0..read.toks.len() {
        if !matches!(read.braces[open], Some(Brace::Function | Brace::Class)) {
            continue;
        }
        let Some(close) = read.pair[open] else {
            continue;
        };
        let mut first = open;
        while first > 0 && !matches!(read.words[first - 1], ";" | "{" | "}" | "," | "(" | "=") && read.within[first - 1] == read.within[open] {
            first -= 1;
        }
        ranges.push((read.toks[first].from.clone(), read.toks[close].to.clone()));
    }
    Parsed { lines: paint.runs(), folds: super::folds_of(folds), ranges }
}

/// A JSDoc comment drawn up for the function at or around `line`: its parameters, what it returns
/// where it returns a value, and what it throws: the edit that writes it over the function, and the
/// place to write its summary.
pub(super) fn jsdoc(src: &str, line: u32) -> Result<(TextEdit, Place), String> {
    let read = Read::new(src);
    let open = (0..read.toks.len())
        .filter(|at| read.braces[*at] == Some(Brace::Function) && function_name(&read, *at).is_some())
        .rfind(|at| read.toks[*at].from.line <= line && read.pair[*at].is_some_and(|close| read.toks[close].to.line >= line) || function_name(&read, *at).is_some_and(|name| read.toks[name].from.line == line))
        .ok_or("Write Docstring writes one for the function the cursor is in.")?;
    let close = read.pair[open].unwrap_or(open);
    let (names, first) = parameters(&read, open);
    let name_at = function_name(&read, open).unwrap_or(first);
    let start = name_at.min(first);
    let mut lead = start;
    while lead > 0 && !matches!(read.words[lead - 1], ";" | "{" | "}") && read.toks[lead - 1].from.line == read.toks[start].from.line {
        lead -= 1;
    }
    let first_line = read.toks[lead].from.line;
    if read.comments.iter().any(|(_, to)| to.line + 1 == first_line && read.src.split('\n').nth(to.line as usize).is_some_and(|text| text.trim_end().ends_with("*/"))) {
        return Err("The function has a comment over it.".into());
    }
    let mut params: Vec<&str> = Vec::new();
    for at in first..open {
        if names.contains(read.words[at]) && !params.contains(&read.words[at]) && read.toks[at].kind == Kind::Name {
            params.push(read.words[at]);
        }
    }
    let mut gives = false;
    let mut throws: Vec<String> = Vec::new();
    let mut at = open + 1;
    while at < close {
        if read.braces[at] == Some(Brace::Function) {
            at = read.pair[at].map_or(at + 1, |end| end + 1);
            continue;
        }
        if read.words[at] == "return" && !matches!(read.words.get(at + 1).copied(), Some(";" | "}")) {
            gives = true;
        }
        if read.words[at] == "throw" {
            let name = if read.words.get(at + 1) == Some(&"new") { read.words.get(at + 2) } else { None };
            let name = name.copied().unwrap_or("Error").to_string();
            if !throws.contains(&name) {
                throws.push(name);
            }
        }
        at += 1;
    }
    let line_text = read.src.split('\n').nth(first_line as usize).unwrap_or_default();
    let indent: String = line_text.chars().take_while(|c| c.is_whitespace()).collect();
    let mut written = format!("{indent}/**\n{indent} * \n");
    if !params.is_empty() || gives || !throws.is_empty() {
        written.push_str(&format!("{indent} *\n"));
    }
    for param in &params {
        written.push_str(&format!("{indent} * @param {param}\n"));
    }
    if gives {
        written.push_str(&format!("{indent} * @returns\n"));
    }
    for thrown in &throws {
        written.push_str(&format!("{indent} * @throws {{{thrown}}}\n"));
    }
    written.push_str(&format!("{indent} */\n"));
    let place = Place { line: first_line, col: 0 };
    let summary = Place { line: first_line + 1, col: indent.encode_utf16().count() as u32 + 3 };
    Ok((TextEdit { from: place.clone(), to: place, text: written }, summary))
}

/// The class around `line` with its methods sorted by name: the edit that writes its body again.
pub(super) fn sort_methods(src: &str, line: u32) -> Result<TextEdit, String> {
    let read = Read::new(src);
    let open = (0..read.toks.len())
        .rfind(|at| read.braces[*at] == Some(Brace::Class) && read.toks[*at].from.line <= line && read.pair[*at].is_some_and(|close| read.toks[close].to.line >= line))
        .ok_or("Sort Methods by Name sorts the class the cursor is in.")?;
    let close = read.pair[open].ok_or("The class does not close.")?;
    let physical: Vec<&str> = src.split('\n').collect();
    let mut members: Vec<(usize, usize, Option<String>)> = Vec::new();
    let mut at = open + 1;
    let mut floor = read.toks[open].to.line as usize + 1;
    while at < close {
        if read.words[at] == ";" {
            at += 1;
            continue;
        }
        let start = at;
        let mut name = None;
        let mut end = at;
        while end < close {
            if read.braces[end] == Some(Brace::Function) {
                name = function_name(&read, end).map(|one| read.words[one].to_string());
                end = read.pair[end].unwrap_or(end);
                break;
            }
            if read.words[end] == ";" {
                break;
            }
            end = read.pair[end].filter(|pair| *pair > end && matches!(read.words[end], "(" | "[")).map_or(end + 1, |pair| pair + 1).min(close);
            if end < close && read.toks[end].from.line > read.toks[end - 1].to.line && read.words[end - 1] != "," && !matches!(read.words[end], "(" | "{" | "=" | ".") && read.braces[end] != Some(Brace::Function) {
                end -= 1;
                break;
            }
        }
        let end = end.min(close - 1);
        let mut first = read.toks[start].from.line as usize;
        while first > floor && read.comments.iter().any(|(from, to)| to.line as usize == first - 1 && from.line as usize >= floor) {
            first = read.comments.iter().filter(|(_, to)| to.line as usize == first - 1).map(|(from, _)| from.line as usize).min().unwrap_or(first - 1);
        }
        let last = read.toks[end].to.line as usize;
        members.push((first, last, name));
        floor = last + 1;
        at = end + 1;
    }
    let first = members.first().map(|member| member.0).ok_or("The class has no members.")?;
    let last = members.last().map_or(first, |member| member.1);
    let body = super::sorted_body(&physical, &members).ok_or("The class's methods stand in the order of their names.")?;
    let end_col = physical[last].encode_utf16().count() as u32;
    Ok(TextEdit { from: Place { line: first as u32, col: 0 }, to: Place { line: last as u32, col: end_col }, text: body })
}

/// The files the file imports by a relative path, at its top level: `import … from`, a bare
/// `import` and `export … from`.
fn imports(read: &Read) -> Vec<Import> {
    let mut out = Vec::new();
    for at in 0..read.toks.len() {
        if read.within[at].is_some() || !matches!(read.words[at], "import" | "export") || matches!(read.words.get(at + 1).copied(), Some("(" | ".")) {
            continue;
        }
        let mut end = at + 1;
        while end < read.toks.len() && read.words[end] != ";" && !(read.toks[end].kind == Kind::Name && matches!(read.words[end], "import" | "export") && read.toks[end].from.line > read.toks[at].from.line && read.within[end].is_none()) {
            end += 1;
        }
        let spec = (at + 1..end.min(read.toks.len())).find(|one| read.toks[*one].kind == Kind::Str && (read.words[*one - 1] == "from" || *one == at + 1)).map(|one| read.words[one]);
        if let Some(spec) = spec {
            let spec = &spec[1..spec.len().saturating_sub(1)];
            if spec.starts_with("./") || spec.starts_with("../") {
                let last = read.toks[(end - 1).max(at)].to.clone();
                out.push(Import { spec: spec.to_string(), names: Vec::new(), alias: None, from: read.toks[at].from.clone(), to: last });
            }
        }
    }
    out
}

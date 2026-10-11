// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Python as the inspections read it: its text as logical lines of tokens, each line's indent the
//! width of its first physical line's, a tab reaching the next multiple of eight. A string is one
//! token, and the names an f-string's fields read are tokens of their own.
//!
//! What one file says alone comes from here: a name a function sets and never reads, code after a
//! return, a raise, a break or a continue in the same block, and a docstring out of PEP 257's form;
//! and, for the tree's checks, the file's imports at the top level and its classes.

use std::collections::HashSet;

use super::{Class, Finding, Fix, Import, Paint, Parsed, Scan, Severity, ATTRIBUTE, COMMENT, DELIMITER, FUNCTION, KEYWORD, MODULE, NUMBER, OPERATOR, PARAMETER, PREDEFINED, PROPERTY, STRING, TYPE};
use crate::servers::{Place, TextEdit};

#[derive(Clone, Copy, Debug, PartialEq)]
pub(super) enum Kind {
    Name,
    Number,
    Str,
    Op,
}

#[derive(Clone, Debug)]
pub(super) struct Tok {
    pub kind: Kind,
    pub start: usize,
    pub end: usize,
    pub from: Place,
    pub to: Place,
    /// A name read inside an f-string's field.
    pub field: bool,
}

/// A logical line: its indent and its tokens, never none.
#[derive(Debug)]
pub(super) struct Line {
    pub indent: usize,
    pub toks: Vec<Tok>,
}

const OPS: [&str; 47] = [
    "**=", "//=", ">>=", "<<=", "...", "->", ":=", "==", "!=", "<=", ">=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "@=", "**", "//", "<<", ">>", "+", "-", "*", "/", "%", "@",
    "&", "|", "^", "~", "<", ">", "(", ")", "[", "]", "{", "}", ",", ":", ".", ";", "=",
];

/// The prefixes a string may have, in lower case.
const PREFIXES: [&str; 13] = ["r", "u", "b", "f", "t", "br", "rb", "fr", "rf", "tr", "rt", "bt", "tb"];

/// Python's own words, which are never names a line reads.
const KEYWORDS: [&str; 35] = [
    "False", "None", "True", "and", "as", "assert", "async", "await", "break", "class", "continue", "def", "del", "elif", "else", "except", "finally", "for", "from", "global", "if", "import",
    "in", "is", "lambda", "nonlocal", "not", "or", "pass", "raise", "return", "try", "while", "with", "yield",
];

fn is_name_start(c: char) -> bool {
    c == '_' || c.is_alphabetic()
}

fn is_name(c: char) -> bool {
    c == '_' || c.is_alphanumeric()
}

/// The logical lines of `src`.
pub(super) fn lines(src: &str) -> Vec<Line> {
    scan(src).0
}

/// The logical lines of `src`, and the spans of its comments.
fn scan(src: &str) -> (Vec<Line>, Vec<(Place, Place)>) {
    let mut s = Scan::new(src);
    let mut lines = Vec::new();
    let mut toks: Vec<Tok> = Vec::new();
    let mut depth = 0i32;
    let mut indent = 0usize;
    let mut at_start = true;
    loop {
        if at_start {
            let mut width = 0;
            while let Some(c) = s.peek() {
                match c {
                    ' ' => width += 1,
                    '\t' => width = (width / 8 + 1) * 8,
                    '\x0c' | '\r' => {}
                    _ => break,
                }
                s.bump();
            }
            match s.peek() {
                None => break,
                Some('\n') => {
                    s.bump();
                    continue;
                }
                Some('#') => {
                    s.skip_line();
                    continue;
                }
                _ => {}
            }
            indent = width;
            at_start = false;
        }
        let Some(c) = s.peek() else {
            break;
        };
        match c {
            '\n' => {
                s.bump();
                if depth > 0 {
                    continue;
                }
                if !toks.is_empty() {
                    lines.push(Line { indent, toks: std::mem::take(&mut toks) });
                }
                at_start = true;
            }
            ' ' | '\t' | '\r' | '\x0c' => {
                s.bump();
            }
            '#' => s.skip_line(),
            '\\' => {
                s.bump();
                if s.peek() == Some('\r') {
                    s.bump();
                }
                if s.peek() == Some('\n') {
                    s.bump();
                }
            }
            '\'' | '"' => {
                let (start, from) = (s.pos, s.place());
                string(&mut s, start, from, "", &mut toks);
            }
            c if is_name_start(c) => {
                let (start, from) = (s.pos, s.place());
                while s.peek().is_some_and(is_name) {
                    s.bump();
                }
                let word = &src[start..s.pos];
                if matches!(s.peek(), Some('\'' | '"')) && PREFIXES.contains(&word.to_ascii_lowercase().as_str()) {
                    string(&mut s, start, from, word, &mut toks);
                } else {
                    toks.push(Tok { kind: Kind::Name, start, end: s.pos, from, to: s.place(), field: false });
                }
            }
            c if c.is_ascii_digit() || (c == '.' && s.peek_at(1).is_some_and(|next| next.is_ascii_digit())) => {
                let (start, from) = (s.pos, s.place());
                while let Some(c) = s.peek() {
                    if c.is_alphanumeric() || c == '_' || c == '.' {
                        let exponent = matches!(c, 'e' | 'E') && !src[start..s.pos].starts_with("0x") && !src[start..s.pos].starts_with("0X");
                        s.bump();
                        if exponent && matches!(s.peek(), Some('+' | '-')) {
                            s.bump();
                        }
                    } else {
                        break;
                    }
                }
                toks.push(Tok { kind: Kind::Number, start, end: s.pos, from, to: s.place(), field: false });
            }
            _ => {
                let (start, from) = (s.pos, s.place());
                let rest = &src[start..];
                let op = OPS.iter().find(|op| rest.starts_with(**op)).copied();
                match op {
                    Some(op) => {
                        for _ in 0..op.chars().count() {
                            s.bump();
                        }
                        match op {
                            "(" | "[" | "{" => depth += 1,
                            ")" | "]" | "}" => depth = (depth - 1).max(0),
                            _ => {}
                        }
                    }
                    None => {
                        s.bump();
                    }
                }
                toks.push(Tok { kind: Kind::Op, start, end: s.pos, from, to: s.place(), field: false });
            }
        }
    }
    if !toks.is_empty() {
        lines.push(Line { indent, toks });
    }
    (lines, s.comments)
}

/// A string from its prefix at `start` to its closing quote, and the names its fields read where it
/// is an f-string or a t-string.
fn string(s: &mut Scan, start: usize, from: Place, prefix: &str, toks: &mut Vec<Tok>) {
    let quote = s.peek().unwrap_or('"');
    let triple = s.peek_at(1) == Some(quote) && s.peek_at(2) == Some(quote);
    let fields = prefix.to_ascii_lowercase().contains(['f', 't']);
    for _ in 0..if triple { 3 } else { 1 } {
        s.bump();
    }
    let mut names = Vec::new();
    let mut braces = 0usize;
    while let Some(c) = s.peek() {
        if c == '\\' {
            s.bump();
            s.bump();
            continue;
        }
        if c == quote && (!triple || (s.peek_at(1) == Some(quote) && s.peek_at(2) == Some(quote))) {
            for _ in 0..if triple { 3 } else { 1 } {
                s.bump();
            }
            break;
        }
        if c == '\n' && !triple {
            break;
        }
        if fields && c == '{' {
            if braces == 0 && s.peek_at(1) == Some('{') {
                s.bump();
                s.bump();
                continue;
            }
            braces += 1;
        } else if fields && c == '}' && braces > 0 {
            braces -= 1;
        } else if braces > 0 && is_name_start(c) {
            let after_dot = s.src[..s.pos].ends_with('.');
            let after_digit = s.src[..s.pos].chars().next_back().is_some_and(|one| one.is_ascii_digit());
            let (name_start, name_from) = (s.pos, s.place());
            while s.peek().is_some_and(is_name) {
                s.bump();
            }
            if !after_dot && !after_digit {
                names.push(Tok { kind: Kind::Name, start: name_start, end: s.pos, from: name_from, to: s.place(), field: true });
            }
            continue;
        }
        s.bump();
    }
    toks.push(Tok { kind: Kind::Str, start, end: s.pos, from, to: s.place(), field: false });
    toks.extend(names);
}

/// The text of a token.
fn text<'a>(src: &'a str, tok: &Tok) -> &'a str {
    &src[tok.start..tok.end]
}

/// Whether a line opens a block: it ends in a colon outside its brackets.
fn opens(src: &str, line: &Line) -> bool {
    line.toks.last().is_some_and(|tok| tok.kind == Kind::Op && text(src, tok) == ":")
}

/// The index past the last line of the block of the line at `at`: every line after it indented
/// deeper than it.
fn block_end(lines: &[Line], at: usize) -> usize {
    let indent = lines[at].indent;
    let mut end = at + 1;
    while end < lines.len() && lines[end].indent > indent {
        end += 1;
    }
    end
}

/// The word a line starts with, past `async`.
fn head<'a>(src: &'a str, line: &Line) -> &'a str {
    let first = text(src, &line.toks[0]);
    if first == "async" && line.toks.len() > 1 { text(src, &line.toks[1]) } else { first }
}

/// What one Python file says alone, and its facts for the tree's checks.
pub(super) fn facts(src: &str) -> super::Facts {
    let lines = lines(src);
    let mut local = Vec::new();
    unreachable(src, &lines, &mut local);
    for (at, line) in lines.iter().enumerate() {
        if head(src, line) == "def" && opens(src, line) {
            unread(src, &lines, at, &mut local);
        }
    }
    docstrings(src, &lines, &mut local);
    super::Facts { imports: imports(src, &lines), classes: classes(src, &lines), functions: functions(src, &lines), local }
}

/// The span from the start of the line `from` to the end of the line `to` whole, its line end with
/// it where another line follows, as a fix takes lines out.
fn whole_lines(src: &str, from: u32, to: u32) -> (Place, Place) {
    let count = src.split('\n').count() as u32;
    if to + 1 < count {
        (Place { line: from, col: 0 }, Place { line: to + 1, col: 0 })
    } else {
        let last = src.split('\n').nth(to as usize).unwrap_or_default();
        (Place { line: from, col: 0 }, Place { line: to, col: last.encode_utf16().count() as u32 })
    }
}

/// Code after a return, a raise, a break or a continue in the same block, which no path reaches.
fn unreachable(src: &str, lines: &[Line], found: &mut Vec<Finding>) {
    let mut at = 0;
    while at < lines.len() {
        let line = &lines[at];
        let word = text(src, &line.toks[0]);
        if !["return", "raise", "break", "continue"].contains(&word) || line.toks.iter().any(|tok| text(src, tok) == ";") {
            at += 1;
            continue;
        }
        let mut end = at + 1;
        while end < lines.len() && lines[end].indent >= line.indent {
            end += 1;
        }
        if end > at + 1 {
            let first = &lines[at + 1];
            let last = lines[end - 1].toks.last().map_or(first.toks[0].to.clone(), |tok| tok.to.clone());
            let (from, to) = whole_lines(src, first.toks[0].from.line, last.line);
            found.push(Finding {
                from: first.toks[0].from.clone(),
                to: last,
                severity: Severity::Warning,
                message: format!("No path reaches this code: the `{word}` before it leaves the block."),
                code: "unreachable",
                href: None,
                fixes: vec![Fix { title: "Take out the code no path reaches".into(), edits: vec![TextEdit { from, to, text: String::new() }] }],
            });
        }
        at = end;
    }
}

/// The statements of a logical line, split at its semicolons outside brackets.
fn statements(src: &str, toks: &[Tok]) -> Vec<std::ops::Range<usize>> {
    let mut out = Vec::new();
    let mut depth = 0i32;
    let mut start = 0;
    for (at, tok) in toks.iter().enumerate() {
        if tok.kind == Kind::Op {
            match text(src, tok) {
                "(" | "[" | "{" => depth += 1,
                ")" | "]" | "}" => depth -= 1,
                ";" if depth == 0 => {
                    out.push(start..at);
                    start = at + 1;
                }
                _ => {}
            }
        }
    }
    if start < toks.len() {
        out.push(start..toks.len());
    }
    out
}

/// A place a name is set that may go unread: the name's token, and the fix that takes it out.
struct Set {
    name: String,
    tok: Tok,
    fix: Option<Fix>,
}

/// The names a function sets and never reads: each name set by an assignment of it alone, by
/// `with … as` or by `except … as`, directly in the function, and read nowhere in it or in what it
/// holds. A name that starts with an underscore, is declared global or nonlocal, or sits in a function
/// that reads its locals whole, is left alone.
fn unread(src: &str, lines: &[Line], at: usize, found: &mut Vec<Finding>) {
    let end = block_end(lines, at);
    let mut sets: Vec<Set> = Vec::new();
    let mut read: HashSet<&str> = HashSet::new();
    let mut declared: HashSet<&str> = HashSet::new();
    let mut nested_end = 0;
    for index in at + 1..end {
        let line = &lines[index];
        let nested = index < nested_end;
        if !nested && matches!(head(src, line), "def" | "class") && opens(src, line) {
            nested_end = block_end(lines, index);
            reads_of(src, &line.toks, &mut read);
            continue;
        }
        if nested {
            reads_of(src, &line.toks, &mut read);
            continue;
        }
        for range in statements(src, &line.toks) {
            let toks = &line.toks[range];
            let first = text(src, &toks[0]);
            if first == "global" || first == "nonlocal" {
                declared.extend(toks[1..].iter().filter(|tok| tok.kind == Kind::Name).map(|tok| text(src, tok)));
                continue;
            }
            let lone = line.toks.len() == toks.len();
            let skip = setting(src, toks, lone, line, &mut sets);
            for (offset, tok) in toks.iter().enumerate() {
                if !skip.contains(&offset) && is_read(src, toks, offset) {
                    read.insert(text(src, tok));
                }
            }
        }
    }
    if ["locals", "vars", "eval", "exec"].iter().any(|name| read.contains(name)) {
        return;
    }
    for set in sets {
        if set.name.starts_with('_') || read.contains(set.name.as_str()) || declared.contains(set.name.as_str()) {
            continue;
        }
        found.push(Finding {
            from: set.tok.from.clone(),
            to: set.tok.to.clone(),
            severity: Severity::Warning,
            message: format!("`{}` is set and never read.", set.name),
            code: "unread",
            href: None,
            fixes: set.fix.into_iter().collect(),
        });
    }
}

/// The names a run of tokens reads.
fn reads_of<'a>(src: &'a str, toks: &[Tok], read: &mut HashSet<&'a str>) {
    for offset in 0..toks.len() {
        if is_read(src, toks, offset) {
            read.insert(text(src, &toks[offset]));
        }
    }
}

/// Whether the token at `offset` is a name read: a name that is no keyword, no attribute after a
/// dot, and no keyword argument's name.
fn is_read(src: &str, toks: &[Tok], offset: usize) -> bool {
    let tok = &toks[offset];
    if tok.kind != Kind::Name {
        return false;
    }
    if tok.field {
        return true;
    }
    let word = text(src, tok);
    if KEYWORDS.contains(&word) {
        return false;
    }
    let before = offset.checked_sub(1).map(|at| text(src, &toks[at]));
    if before == Some(".") {
        return false;
    }
    let after = toks.get(offset + 1).map(|tok| text(src, tok));
    !(after == Some("=") && matches!(before, Some("(" | ",")) && depth_at(src, toks, offset) > 0)
}

/// How many brackets are open before the token at `offset`.
fn depth_at(src: &str, toks: &[Tok], offset: usize) -> i32 {
    let mut depth = 0;
    for tok in &toks[..offset] {
        match text(src, tok) {
            "(" | "[" | "{" => depth += 1,
            ")" | "]" | "}" => depth -= 1,
            _ => {}
        }
    }
    depth
}

/// Notes the names a statement sets that may go unread, and gives the offsets of its tokens that
/// set them, which are no reads.
fn setting(src: &str, toks: &[Tok], lone: bool, line: &Line, sets: &mut Vec<Set>) -> Vec<usize> {
    let words: Vec<&str> = toks.iter().map(|tok| text(src, tok)).collect();
    let mut skip = Vec::new();
    // `with … as name` and `except … as name`.
    if matches!(words[0], "with" | "except") || (words[0] == "async" && words.get(1) == Some(&"with")) {
        let mut depth = 0;
        for offset in 0..toks.len() {
            match words[offset] {
                "(" | "[" | "{" => depth += 1,
                ")" | "]" | "}" => depth -= 1,
                "as" if depth == 0 && toks.get(offset + 1).is_some_and(|tok| tok.kind == Kind::Name) && matches!(words.get(offset + 2), Some(&"," | &":")) => {
                    let name = &toks[offset + 1];
                    skip.push(offset + 1);
                    let before = &toks[offset - 1];
                    let fix = Fix { title: format!("Take out `as {}`", words[offset + 1]), edits: vec![TextEdit { from: before.to.clone(), to: name.to.clone(), text: String::new() }] };
                    sets.push(Set { name: words[offset + 1].to_string(), tok: name.clone(), fix: Some(fix) });
                }
                _ => {}
            }
        }
        return skip;
    }
    // An assignment: the targets before its last `=` outside brackets.
    let mut depth = 0;
    let equals: Vec<usize> = (0..toks.len())
        .filter(|&offset| {
            match words[offset] {
                "(" | "[" | "{" => depth += 1,
                ")" | "]" | "}" => depth -= 1,
                _ => {}
            }
            depth == 0 && words[offset] == "=" && toks[offset].kind == Kind::Op
        })
        .collect();
    let Some(&last) = equals.last() else {
        return skip;
    };
    let mut start = 0;
    for &equal in &equals {
        let target = &toks[start..equal];
        let annotated = target.len() > 2 && start == 0 && words[1] == ":";
        if (target.len() == 1 || annotated) && target[0].kind == Kind::Name && !KEYWORDS.contains(&words[start]) {
            skip.push(start);
            let fix = (equals.len() == 1 && lone && !annotated).then(|| {
                let value = &toks[last + 1..];
                let calls = value.iter().any(|tok| text(src, tok) == "(");
                if calls {
                    Fix { title: format!("Take out `{} =` and keep its value", words[start]), edits: vec![TextEdit { from: toks[0].from.clone(), to: value[0].from.clone(), text: String::new() }] }
                } else {
                    let end = line.toks.last().map_or(toks[0].to.line, |tok| tok.to.line);
                    let (from, to) = whole_lines(src, toks[0].from.line, end);
                    Fix { title: format!("Take out the line that sets {}", words[start]), edits: vec![TextEdit { from, to, text: String::new() }] }
                }
            });
            if last + 1 < toks.len() {
                sets.push(Set { name: words[start].to_string(), tok: toks[start].clone(), fix });
            }
        } else if target.iter().all(|tok| tok.kind == Kind::Name || text(src, tok) == ",") && target.iter().any(|tok| text(src, tok) == ",") {
            // A tuple's names are set, and not reported.
            skip.extend((start..equal).filter(|offset| toks[*offset].kind == Kind::Name));
        }
        start = equal + 1;
    }
    skip
}

/// The page of PEP 257, which a docstring's finding links to.
const PEP_257: &str = "https://peps.python.org/pep-0257/";

/// The docstrings out of PEP 257's form: the module's, and each function's and class's.
fn docstrings(src: &str, lines: &[Line], found: &mut Vec<Finding>) {
    let lone = |line: &Line| line.toks.len() == 1 && line.toks[0].kind == Kind::Str || (line.toks.iter().all(|tok| tok.kind == Kind::Str || tok.field) && line.toks[0].kind == Kind::Str && line.toks.iter().filter(|tok| tok.kind == Kind::Str).count() == 1);
    if let Some(first) = lines.first() {
        if lone(first) {
            docstring(src, &first.toks[0], found);
        }
    }
    for (at, line) in lines.iter().enumerate() {
        if matches!(head(src, line), "def" | "class") && opens(src, line) {
            if let Some(next) = lines.get(at + 1).filter(|next| next.indent > line.indent && lone(next)) {
                docstring(src, &next.toks[0], found);
            }
        }
    }
}

/// What of PEP 257's form one docstring leaves out: three double quotes, a one-line docstring on one
/// line with no space inside its quotes, a summary line that ends in a period, a blank line after the
/// summary line where more follows, and closing quotes on a line of their own after more than one
/// line.
fn docstring(src: &str, tok: &Tok, found: &mut Vec<Finding>) {
    let whole = text(src, tok);
    let prefix_len = whole.find(['"', '\'']).unwrap_or(0);
    let prefix = &whole[..prefix_len];
    if prefix.to_ascii_lowercase().contains(['f', 'b', 't']) {
        return;
    }
    let body = &whole[prefix_len..];
    let quote = &body[..if body.starts_with("\"\"\"") || body.starts_with("'''") { 3 } else { 1 }];
    if body.len() < quote.len() * 2 {
        return;
    }
    let inner = &body[quote.len()..body.len() - quote.len()];
    let note = |message: &str, code: &'static str, fixes: Vec<Fix>| Finding { from: tok.from.clone(), to: tok.to.clone(), severity: Severity::Note, message: format!("PEP 257: {message}"), code, href: Some(PEP_257), fixes };
    let whole_fix = |title: &str, text: String| Fix { title: title.to_string(), edits: vec![TextEdit { from: tok.from.clone(), to: tok.to.clone(), text }] };
    if quote != "\"\"\"" {
        let fixes = if inner.contains("\"\"\"") || inner.ends_with('"') { Vec::new() } else { vec![whole_fix("Use three double quotes", format!("{prefix}\"\"\"{inner}\"\"\""))] };
        found.push(note("a docstring is written in three double quotes.", "docstring-quotes", fixes));
    }
    let trimmed = inner.trim();
    if trimmed.is_empty() {
        return;
    }
    let text_lines: Vec<&str> = trimmed.lines().collect();
    if text_lines.len() == 1 {
        if inner.contains('\n') {
            found.push(note("a one-line docstring fits on one line with its quotes.", "docstring-one-line", vec![whole_fix("Put the docstring on one line", format!("{prefix}\"\"\"{trimmed}\"\"\""))]));
        } else if inner != trimmed {
            found.push(note("a one-line docstring has no space inside its quotes.", "docstring-space", vec![whole_fix("Take out the space inside the quotes", format!("{prefix}{quote}{trimmed}{quote}"))]));
        }
    }
    let summary = text_lines[0].trim_end();
    if !summary.ends_with('.') {
        let first_line = inner.lines().position(|one| !one.trim().is_empty()).unwrap_or(0) as u32;
        let line_text = src.split('\n').nth((tok.from.line + first_line) as usize).unwrap_or_default();
        let fixes = match line_text.find(summary) {
            Some(at) => {
                let col = line_text[..at + summary.len()].encode_utf16().count() as u32;
                let place = Place { line: tok.from.line + first_line, col };
                vec![Fix { title: "End the summary line with a period".into(), edits: vec![TextEdit { from: place.clone(), to: place, text: ".".into() }] }]
            }
            None => Vec::new(),
        };
        found.push(note("a docstring's summary line ends in a period.", "docstring-period", fixes));
    }
    if text_lines.len() > 1 {
        if !text_lines[1].trim().is_empty() {
            found.push(note("a blank line comes between a docstring's summary line and what follows it.", "docstring-blank", Vec::new()));
        }
        let last = inner.rsplit('\n').next().unwrap_or_default();
        if !last.trim().is_empty() {
            let indent: String = src.split('\n').nth(tok.from.line as usize).unwrap_or_default().chars().take_while(|c| c.is_whitespace()).collect();
            let close = Place { line: tok.to.line, col: tok.to.col - quote.encode_utf16().count() as u32 };
            found.push(note(
                "the closing quotes of a docstring of more than one line stand on a line of their own.",
                "docstring-close",
                vec![Fix { title: "Put the closing quotes on a line of their own".into(), edits: vec![TextEdit { from: close.clone(), to: close, text: format!("\n{indent}") }] }],
            ));
        }
    }
}

/// The functions a file declares, methods among them, and the calls in each outside the functions it
/// holds: a name before a bracket, after no dot or after `self.` or `cls.`.
fn functions(src: &str, lines: &[Line]) -> Vec<super::Function> {
    let mut out = Vec::new();
    for (at, line) in lines.iter().enumerate() {
        if head(src, line) != "def" || !opens(src, line) {
            continue;
        }
        let Some(name_at) = line.toks.iter().position(|tok| text(src, tok) == "def").map(|at| at + 1).filter(|at| *at < line.toks.len()) else {
            continue;
        };
        let end = block_end(lines, at);
        let mut calls = Vec::new();
        let mut nested_end = 0;
        for index in at + 1..end {
            let inner = &lines[index];
            if index < nested_end {
                continue;
            }
            if head(src, inner) == "def" && opens(src, inner) {
                nested_end = block_end(lines, index);
                continue;
            }
            for offset in 0..inner.toks.len().saturating_sub(1) {
                let tok = &inner.toks[offset];
                if tok.kind != Kind::Name || text(src, &inner.toks[offset + 1]) != "(" || KEYWORDS.contains(&text(src, tok)) {
                    continue;
                }
                let before = offset.checked_sub(1).map(|at| text(src, &inner.toks[at]));
                let owner = offset.checked_sub(2).map(|at| text(src, &inner.toks[at]));
                if before == Some(".") && !matches!(owner, Some("self" | "cls")) || matches!(before, Some("def" | "class")) {
                    continue;
                }
                calls.push((text(src, tok).to_string(), tok.from.clone(), tok.to.clone()));
            }
        }
        let name = &line.toks[name_at];
        let last = lines[end - 1].toks.last().map_or(name.from.line, |tok| tok.to.line);
        out.push(super::Function { name: text(src, name).to_string(), from: name.from.clone(), to: name.to.clone(), first: line.toks[0].from.line, last, calls });
    }
    out
}

/// The brackets that close their delimiters, the stops and the separators, which a parse colors as
/// delimiters, every other operator as an operator.
const DELIMITERS: [&str; 9] = ["(", ")", "[", "]", "{", "}", ",", ";", "."];

/// The names a function's header declares as its parameters, and the line past its block.
fn parameters<'a>(src: &'a str, lines: &[Line], at: usize) -> (HashSet<&'a str>, usize) {
    let line = &lines[at];
    let mut names = HashSet::new();
    let mut depth = 0;
    for (offset, tok) in line.toks.iter().enumerate() {
        let word = text(src, tok);
        match word {
            "(" | "[" | "{" => depth += 1,
            ")" | "]" | "}" => depth -= 1,
            _ => {}
        }
        let before = offset.checked_sub(1).map(|one| text(src, &line.toks[one]));
        let after = line.toks.get(offset + 1).map(|one| text(src, one));
        if tok.kind == Kind::Name && depth == 1 && matches!(before, Some("(" | "," | "*" | "**")) && matches!(after, Some(":" | "=" | "," | ")")) {
            names.insert(word);
        }
    }
    (names, block_end(lines, at))
}

/// The parse of a Python file: each column's class, the regions that fold, and the spans Expand
/// Selection steps through.
pub(super) fn parse(src: &str) -> Parsed {
    let (lines, comments) = scan(src);
    let mut paint = Paint::new(src);
    for (from, to) in &comments {
        paint.span(from, to, COMMENT);
    }
    let classes: HashSet<&str> = lines.iter().filter(|line| head(src, line) == "class").filter_map(|line| line.toks.iter().position(|tok| text(src, tok) == "class").and_then(|at| line.toks.get(at + 1)).map(|tok| text(src, tok))).collect();
    let scopes: Vec<(usize, HashSet<&str>, usize)> = (0..lines.len()).filter(|at| head(src, &lines[*at]) == "def").map(|at| {
        let (names, end) = parameters(src, &lines, at);
        (at, names, end)
    }).collect();
    let mut folds = Vec::new();
    let mut ranges = Vec::new();
    for (index, line) in lines.iter().enumerate() {
        let params: HashSet<&str> = scopes.iter().filter(|(at, _, end)| *at <= index && index < *end).flat_map(|(_, names, _)| names.iter().copied()).collect();
        let words: Vec<&str> = line.toks.iter().map(|tok| text(src, tok)).collect();
        let importing = words[0] == "import" || words[0] == "from";
        let imported_at = words.iter().position(|word| *word == "import").unwrap_or(words.len());
        let decorated = words[0] == "@";
        let mut depth = 0;
        let mut stack: Vec<usize> = Vec::new();
        for (offset, tok) in line.toks.iter().enumerate() {
            let word = words[offset];
            let before = offset.checked_sub(1).map(|one| words[one]);
            let after = words.get(offset + 1).copied();
            let class = match tok.kind {
                Kind::Str => STRING,
                Kind::Number => NUMBER,
                Kind::Name if tok.field => continue,
                Kind::Op => {
                    match word {
                        "(" | "[" | "{" => {
                            depth += 1;
                            stack.push(offset);
                        }
                        ")" | "]" | "}" => {
                            depth -= 1;
                            if let Some(open) = stack.pop() {
                                let opened = &line.toks[open];
                                ranges.push((opened.to.clone(), tok.from.clone()));
                                ranges.push((opened.from.clone(), tok.to.clone()));
                                folds.push((opened.from.line, tok.to.line));
                            }
                        }
                        _ => {}
                    }
                    if decorated && depth == 0 && (offset == 0 || word == ".") {
                        ATTRIBUTE
                    } else if DELIMITERS.contains(&word) {
                        DELIMITER
                    } else {
                        OPERATOR
                    }
                }
                Kind::Name => {
                    if matches!(word, "True" | "False" | "None" | "self" | "cls") {
                        PREDEFINED
                    } else if KEYWORDS.contains(&word) {
                        KEYWORD
                    } else if decorated && depth == 0 {
                        ATTRIBUTE
                    } else if importing && (words[0] == "import" || offset < imported_at) {
                        MODULE
                    } else if before == Some("def") {
                        FUNCTION
                    } else if before == Some("class") {
                        TYPE
                    } else if before == Some(".") {
                        if after == Some("(") { FUNCTION } else { PROPERTY }
                    } else if after == Some("(") {
                        if classes.contains(word) { TYPE } else { FUNCTION }
                    } else if (after == Some("=") && depth > 0 && matches!(before, Some("(" | ","))) || params.contains(word) {
                        PARAMETER
                    } else if classes.contains(word) {
                        TYPE
                    } else {
                        0
                    }
                }
            };
            paint.span(&tok.from, &tok.to, class);
            if tok.kind == Kind::Str {
                let whole = text(src, tok);
                let open = whole.find(['"', '\'']).unwrap_or(0);
                let quotes = if whole[open..].starts_with("\"\"\"") || whole[open..].starts_with("'''") { 3 } else { 1 };
                ranges.push((tok.from.clone(), tok.to.clone()));
                if whole.len() >= open + quotes * 2 {
                    ranges.push((Place { line: tok.from.line, col: tok.from.col + (open + quotes) as u32 }, Place { line: tok.to.line, col: tok.to.col.saturating_sub(quotes as u32) }));
                }
                folds.push((tok.from.line, tok.to.line));
            }
        }
        let last = line.toks.last().map_or(line.toks[0].to.clone(), |tok| tok.to.clone());
        ranges.push((line.toks[0].from.clone(), last.clone()));
        folds.push((line.toks[0].from.line, last.line));
        if opens(src, line) {
            let end = block_end(&lines, index);
            if end > index + 1 {
                let close = lines[end - 1].toks.last().map_or(last.clone(), |tok| tok.to.clone());
                ranges.push((line.toks[0].from.clone(), close.clone()));
                ranges.push((lines[index + 1].toks[0].from.clone(), close.clone()));
                folds.push((line.toks[0].from.line, close.line));
                // A decorated function or class, with its decorators.
                let mut first = index;
                while first > 0 && lines[first - 1].indent == line.indent && text(src, &lines[first - 1].toks[0]) == "@" {
                    first -= 1;
                }
                if first < index {
                    ranges.push((lines[first].toks[0].from.clone(), close));
                }
            }
        }
    }
    for (from, to) in &comments {
        ranges.push((from.clone(), to.clone()));
    }
    Parsed { lines: paint.runs(), folds: super::folds_of(folds), ranges }
}

/// The function whose header or block holds `line`, the innermost: its line's index.
fn function_around(src: &str, lines: &[Line], line: u32) -> Option<usize> {
    let mut found = None;
    for (at, one) in lines.iter().enumerate() {
        if head(src, one) != "def" || !opens(src, one) || one.toks[0].from.line > line {
            continue;
        }
        let end = block_end(lines, at);
        let last = lines[end - 1].toks.last().map_or(one.toks[0].to.line, |tok| tok.to.line);
        if line <= last {
            found = Some(at);
        }
    }
    found
}

/// What the function whose header is the line at `at` says of itself.
fn signature_of(src: &str, lines: &[Line], at: usize) -> super::Signature {
    let header = &lines[at];
    let words: Vec<&str> = header.toks.iter().map(|tok| text(src, tok)).collect();
    let mut sign = super::Signature::default();
    let open = words.iter().position(|word| *word == "(").unwrap_or(0);
    let mut depth = 0;
    let mut part: Vec<usize> = Vec::new();
    let mut close = open;
    let mut parts: Vec<Vec<usize>> = Vec::new();
    for (offset, word) in words.iter().enumerate().skip(open) {
        match *word {
            "(" | "[" | "{" => {
                depth += 1;
                if depth == 1 {
                    continue;
                }
            }
            ")" | "]" | "}" => {
                depth -= 1;
                if depth == 0 {
                    close = offset;
                    parts.push(std::mem::take(&mut part));
                    break;
                }
            }
            "," if depth == 1 => {
                parts.push(std::mem::take(&mut part));
                continue;
            }
            _ => {}
        }
        part.push(offset);
    }
    for (index, part) in parts.iter().filter(|part| !part.is_empty()).enumerate() {
        let stars: String = part.iter().take_while(|at| matches!(words[**at], "*" | "**")).map(|at| words[*at]).collect();
        let Some(&name_at) = part.iter().find(|at| header.toks[**at].kind == Kind::Name) else {
            continue;
        };
        let name = words[name_at];
        if index == 0 && stars.is_empty() && matches!(name, "self" | "cls") {
            continue;
        }
        let colon = part.iter().position(|at| words[*at] == ":");
        let equal = part.iter().position(|at| words[*at] == "=");
        let annotation = colon.map(|colon| {
            let end = equal.unwrap_or(part.len());
            src[header.toks[part[colon + 1]].start..header.toks[part[end - 1]].end].to_string()
        });
        sign.params.push((format!("{stars}{name}"), annotation));
    }
    if let Some(arrow) = words.iter().skip(close).position(|word| *word == "->").map(|at| at + close) {
        let end = words.len() - 1;
        if arrow + 1 < end {
            sign.returns = Some(src[header.toks[arrow + 1].start..header.toks[end - 1].end].to_string());
        }
    }
    let end = block_end(lines, at);
    let mut nested_end = 0;
    for index in at + 1..end {
        let line = &lines[index];
        if index < nested_end {
            continue;
        }
        if matches!(head(src, line), "def" | "class") && opens(src, line) {
            nested_end = block_end(lines, index);
            continue;
        }
        let words: Vec<&str> = line.toks.iter().map(|tok| text(src, tok)).collect();
        if words[0] == "return" && words.len() > 1 && words[1] != "None" {
            sign.gives = true;
        }
        sign.yields |= words.contains(&"yield");
        if words[0] == "raise" && words.len() > 1 {
            let name: String = words[1..].iter().take_while(|word| **word != "(" && **word != "from").copied().collect();
            if !name.is_empty() && !sign.raises.contains(&name) {
                sign.raises.push(name);
            }
        }
    }
    sign.gives |= sign.returns.as_deref().is_some_and(|returns| returns != "None");
    sign
}

/// A docstring drawn up for the function at or around `line`, in `form`: the edit that writes it
/// under the function's header, and the place to write its summary.
pub(super) fn write_docstring(src: &str, line: u32, form: &str) -> Result<(TextEdit, Place), String> {
    let lines = lines(src);
    let at = function_around(src, &lines, line).ok_or("Write Docstring writes one for the function the cursor is in.")?;
    let end = block_end(&lines, at);
    if end <= at + 1 {
        return Err("The function's body stands on its header's line.".into());
    }
    let first = &lines[at + 1];
    if first.toks[0].kind == Kind::Str && first.toks[1..].iter().all(|tok| tok.field) {
        return Err("The function has a docstring.".into());
    }
    let sign = signature_of(src, &lines, at);
    let header_end = lines[at].toks.last().map_or(0, |tok| tok.to.line);
    let body_line = src.split('\n').nth(first.toks[0].from.line as usize).unwrap_or_default();
    let indent: String = body_line.chars().take_while(|c| c.is_whitespace()).collect();
    let mut out: Vec<String> = Vec::new();
    let section = |out: &mut Vec<String>, title: &str| {
        out.push(String::new());
        match form {
            "numpy" => {
                out.push(title.to_string());
                out.push("-".repeat(title.len()));
            }
            _ => out.push(format!("{title}:")),
        }
    };
    let returned = if sign.yields { "Yields" } else { "Returns" };
    match form {
        "rest" => {
            if !sign.params.is_empty() || sign.gives || sign.yields || !sign.raises.is_empty() {
                out.push(String::new());
            }
            for (name, annotation) in &sign.params {
                out.push(format!(":param {name}:"));
                if let Some(annotation) = annotation {
                    out.push(format!(":type {name}: {annotation}"));
                }
            }
            if sign.gives || sign.yields {
                out.push(if sign.yields { ":yield:".into() } else { ":return:".into() });
                if let Some(returns) = &sign.returns {
                    out.push(format!(":rtype: {returns}"));
                }
            }
            for raised in &sign.raises {
                out.push(format!(":raises {raised}:"));
            }
        }
        "plain" => {}
        _ => {
            if !sign.params.is_empty() {
                section(&mut out, if form == "numpy" { "Parameters" } else { "Args" });
                for (name, annotation) in &sign.params {
                    out.push(match (form, annotation) {
                        ("numpy", Some(annotation)) => format!("{name} : {annotation}"),
                        ("numpy", None) => name.clone(),
                        (_, Some(annotation)) => format!("    {name} ({annotation}):"),
                        (_, None) => format!("    {name}:"),
                    });
                }
            }
            if sign.gives || sign.yields {
                section(&mut out, returned);
                let returns = sign.returns.clone().unwrap_or_default();
                out.push(match form {
                    "numpy" => returns,
                    _ if returns.is_empty() => String::new(),
                    _ => format!("    {returns}:"),
                });
                if out.last().is_some_and(String::is_empty) {
                    out.pop();
                }
            }
            if !sign.raises.is_empty() {
                section(&mut out, "Raises");
                for raised in &sign.raises {
                    out.push(if form == "numpy" { raised.clone() } else { format!("    {raised}:") });
                }
            }
        }
    }
    let mut written = format!("{indent}\"\"\"");
    if out.is_empty() {
        written.push_str("\"\"\"\n");
    } else {
        written.push('\n');
        for one in &out {
            if one.is_empty() {
                written.push('\n');
            } else {
                written.push_str(&format!("{indent}{one}\n"));
            }
        }
        written.push_str(&format!("{indent}\"\"\"\n"));
    }
    let place = Place { line: header_end + 1, col: 0 };
    let summary = Place { line: header_end + 1, col: indent.encode_utf16().count() as u32 + 3 };
    Ok((TextEdit { from: place.clone(), to: place, text: written }, summary))
}

/// The class whose header or block holds `line`, the innermost: its line's index.
fn class_around(src: &str, lines: &[Line], line: u32) -> Option<usize> {
    let mut found = None;
    for (at, one) in lines.iter().enumerate() {
        if head(src, one) != "class" || !opens(src, one) || one.toks[0].from.line > line {
            continue;
        }
        let end = block_end(lines, at);
        if line <= lines[end - 1].toks.last().map_or(one.toks[0].to.line, |tok| tok.to.line) {
            found = Some(at);
        }
    }
    found
}

/// The class around `line` with its methods sorted by name: the edit that writes its body again.
pub(super) fn sort_methods(src: &str, line: u32) -> Result<TextEdit, String> {
    let lines = lines(src);
    let at = class_around(src, &lines, line).ok_or("Sort Methods by Name sorts the class the cursor is in.")?;
    let end = block_end(&lines, at);
    let indent = lines.get(at + 1).filter(|_| end > at + 1).map(|one| one.indent).ok_or("The class's body stands on its header's line.")?;
    let physical: Vec<&str> = src.split('\n').collect();
    let comment = |index: usize| physical[index].trim_start().starts_with('#');
    // Each member: its first and last physical lines, and its name where it is a method.
    let mut members: Vec<(usize, usize, Option<String>)> = Vec::new();
    let mut index = at + 1;
    let mut floor = lines[at].toks.last().map_or(0, |tok| tok.to.line as usize) + 1;
    while index < end {
        let one = &lines[index];
        if one.indent != indent {
            index += 1;
            continue;
        }
        let mut first = one.toks[0].from.line as usize;
        // Decorators and the comments over a member go with it.
        let mut def_at = index;
        while def_at < end && lines[def_at].indent == indent && text(src, &lines[def_at].toks[0]) == "@" {
            def_at += 1;
        }
        let def_at = def_at.min(end - 1);
        while first > floor && comment(first - 1) {
            first -= 1;
        }
        let member_end = block_end(&lines, def_at);
        let last = lines[member_end - 1].toks.last().map_or(first as u32, |tok| tok.to.line) as usize;
        let defined = &lines[def_at];
        let name = (head(src, defined) == "def").then(|| defined.toks.iter().position(|tok| text(src, tok) == "def").and_then(|at| defined.toks.get(at + 1)).map(|tok| text(src, tok).to_string())).flatten();
        members.push((first, last, name));
        floor = last + 1;
        index = member_end;
    }
    let first = members.first().map(|member| member.0).ok_or("The class has no members.")?;
    let last = members.last().map_or(first, |member| member.1);
    let body = super::sorted_body(&physical, &members).ok_or("The class's methods stand in the order of their names.")?;
    let end_col = physical[last].encode_utf16().count() as u32;
    Ok(TextEdit { from: Place { line: first as u32, col: 0 }, to: Place { line: last as u32, col: end_col }, text: body })
}

/// The imports a file makes as it runs: those outside every function and class, and outside a block
/// that runs only for a type checker.
fn imports(src: &str, lines: &[Line]) -> Vec<Import> {
    let mut out = Vec::new();
    let mut skip_to = 0;
    for (at, line) in lines.iter().enumerate() {
        if at < skip_to {
            continue;
        }
        let word = head(src, line);
        if (matches!(word, "def" | "class") || (word == "if" && line.toks.iter().any(|tok| text(src, tok) == "TYPE_CHECKING"))) && opens(src, line) {
            skip_to = block_end(lines, at);
            continue;
        }
        let words: Vec<&str> = line.toks.iter().map(|tok| text(src, tok)).collect();
        let span = (line.toks[0].from.clone(), line.toks.last().map_or(line.toks[0].to.clone(), |tok| tok.to.clone()));
        if words[0] == "import" {
            for part in words[1..].split(|word| *word == ",") {
                let alias = part.iter().position(|word| *word == "as").and_then(|at| part.get(at + 1)).map(|one| one.to_string());
                let dotted: String = part.iter().take_while(|word| **word != "as").copied().collect();
                if !dotted.is_empty() {
                    out.push(Import { spec: dotted, names: Vec::new(), alias, from: span.0.clone(), to: span.1.clone() });
                }
            }
        } else if words[0] == "from" {
            let Some(import_at) = words.iter().position(|word| *word == "import") else {
                continue;
            };
            let spec: String = words[1..import_at].concat();
            let mut names = Vec::new();
            for part in words[import_at + 1..].split(|word| *word == ",") {
                let part: Vec<&str> = part.iter().copied().filter(|word| !matches!(*word, "(" | ")")).collect();
                if let Some(name) = part.first().filter(|name| **name != "*") {
                    let alias = part.iter().position(|word| *word == "as").and_then(|at| part.get(at + 1)).map(|one| one.to_string());
                    names.push((name.to_string(), alias));
                }
            }
            out.push(Import { spec, names, alias: None, from: span.0, to: span.1 });
        }
    }
    out
}

/// The classes a file declares at its top level: each one's name, the bases it names, what its body
/// and its methods set, and the attributes its methods read of `self`.
fn classes(src: &str, lines: &[Line]) -> Vec<Class> {
    let mut out = Vec::new();
    for (at, line) in lines.iter().enumerate() {
        if line.indent != 0 || head(src, line) != "class" || !opens(src, line) {
            continue;
        }
        let words: Vec<&str> = line.toks.iter().map(|tok| text(src, tok)).collect();
        let Some(name_at) = words.iter().position(|word| *word == "class").map(|at| at + 1).filter(|at| *at < words.len()) else {
            continue;
        };
        let mut bases = Vec::new();
        if words.get(name_at + 1) == Some(&"(") {
            let mut depth = 0;
            let mut current = String::new();
            let mut keyword = false;
            for (offset, word) in words.iter().enumerate().skip(name_at + 2) {
                match *word {
                    "(" | "[" | "{" => depth += 1,
                    ")" | "]" | "}" if depth > 0 => depth -= 1,
                    ")" => {
                        if !keyword && !current.is_empty() {
                            bases.push(std::mem::take(&mut current));
                        }
                        break;
                    }
                    "," if depth == 0 => {
                        if !keyword && !current.is_empty() {
                            bases.push(std::mem::take(&mut current));
                        }
                        current.clear();
                        keyword = false;
                        continue;
                    }
                    "=" if depth == 0 => keyword = true,
                    _ => {}
                }
                if depth == 0 && !matches!(*word, "(" | ")") && line.toks[offset].kind != Kind::Str {
                    current.push_str(word);
                } else if depth > 0 && *word == "[" {
                    current.push('[');
                }
            }
        }
        let bases = bases.into_iter().map(|base| base.split('[').next().unwrap_or_default().to_string()).filter(|base| !base.is_empty()).collect();
        let end = block_end(lines, at);
        let body_indent = lines.get(at + 1).map_or(0, |next| next.indent);
        let mut has = HashSet::new();
        let mut reads = Vec::new();
        let mut dynamic = false;
        for inner in &lines[at + 1..end] {
            let words: Vec<&str> = inner.toks.iter().map(|tok| text(src, tok)).collect();
            if inner.indent == body_indent {
                if matches!(head(src, inner), "def" | "class") {
                    let offset = words.iter().position(|word| matches!(*word, "def" | "class")).map_or(1, |at| at + 1);
                    if let Some(name) = words.get(offset) {
                        has.insert(name.to_string());
                        dynamic |= matches!(*name, "__getattr__" | "__getattribute__");
                    }
                } else if inner.toks[0].kind == Kind::Name {
                    for (offset, word) in words.iter().enumerate() {
                        if matches!(*word, "=" | ":") && offset > 0 {
                            for target in &words[..offset] {
                                if !matches!(*target, "," | "(" | ")") {
                                    has.insert(target.to_string());
                                }
                            }
                            break;
                        }
                    }
                }
                continue;
            }
            dynamic |= words.contains(&"setattr") || words.contains(&"__dict__");
            for offset in 0..inner.toks.len().saturating_sub(2) {
                if words[offset] == "self" && words[offset + 1] == "." && inner.toks[offset + 2].kind == Kind::Name && (offset == 0 || words[offset - 1] != ".") {
                    let name = words[offset + 2];
                    let next = words.get(offset + 3).copied().unwrap_or_default();
                    let sets = depth_at(src, &inner.toks, offset) == 0 && (next == "=" || next == "," || next == ":" || AUGMENTED.contains(&next)) && setting_target(&words, offset);
                    if sets {
                        has.insert(name.to_string());
                    } else {
                        let tok = &inner.toks[offset + 2];
                        reads.push((name.to_string(), tok.from.clone(), tok.to.clone()));
                    }
                }
                // `getattr(self, "name", …)` and `hasattr(self, "name")` ask whether it is set, and take it as set.
                if matches!(words[offset], "getattr" | "hasattr") && words.get(offset + 1) == Some(&"(") && words.get(offset + 2) == Some(&"self") && words.get(offset + 3) == Some(&",") {
                    if let Some(tok) = inner.toks.get(offset + 4).filter(|tok| tok.kind == Kind::Str) {
                        has.insert(text(src, tok).trim_matches(['\'', '"']).to_string());
                    }
                }
            }
        }
        let tok = &line.toks[name_at];
        out.push(Class { name: words[name_at].to_string(), from: tok.from.clone(), to: tok.to.clone(), bases, has, reads, dynamic });
    }
    out
}

/// The operators that set a name to a value made from its own.
const AUGMENTED: [&str; 13] = ["+=", "-=", "*=", "/=", "//=", "%=", "**=", "|=", "&=", "^=", "@=", "<<=", ">>="];

/// Whether `self.name` at `offset` stands before the line's assignment, as one of its targets: before
/// its last `=` outside brackets, as each of a chain's targets does, or before an operator that sets it
/// from itself, or before the colon of its annotation.
fn setting_target(words: &[&str], offset: usize) -> bool {
    let mut depth = 0;
    let mut last = None;
    for (at, word) in words.iter().enumerate() {
        match *word {
            "(" | "[" | "{" => depth += 1,
            ")" | "]" | "}" => depth -= 1,
            "=" if depth == 0 => last = Some(at),
            word if depth == 0 && AUGMENTED.contains(&word) && last.is_none() => return offset < at,
            ":" if depth == 0 && at == offset + 3 => return true,
            _ => {}
        }
    }
    last.is_some_and(|at| offset < at)
}

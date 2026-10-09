// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! What the files of a tree declare, for Go to Symbol in the tree and Search Everywhere: each line
//! read by its own shape in its file's language, as the outline reads the open file. The index keeps
//! each file's declarations with the time the file was last written and reads a file again only once
//! that time moves. It is built on a thread of its own, and a search asked for while it is built
//! answers from what is read so far and says so.

use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};
use std::sync::{Arc, Mutex};
use std::time::{Instant, SystemTime};

use serde::Serialize;

/// A file larger than this is not read for its declarations.
const LARGEST: u64 = 1024 * 1024;

/// How long the index stands after it was last brought up to date before a search starts it again.
const FRESH_FOR: std::time::Duration = std::time::Duration::from_secs(4);

/// One declaration: its name, what it is, the file it is in under the tree's root, and its line
/// counted from 0.
#[derive(Clone, Debug, PartialEq, Serialize)]
pub struct Declared {
    pub name: String,
    pub kind: &'static str,
    pub path: String,
    pub line: u32,
}

/// What a search found, the best first, and whether the index was still being read.
#[derive(Serialize)]
pub struct Found {
    pub symbols: Vec<Declared>,
    pub reading: bool,
}

#[derive(Clone, Copy, PartialEq)]
enum Language {
    Python,
    Rust,
    Script,
    C,
    Shell,
    PowerShell,
    Toml,
    Markdown,
    Tex,
}

fn language_of(path: &str) -> Option<Language> {
    let ext = path.rsplit_once('.')?.1.to_ascii_lowercase();
    Some(match ext.as_str() {
        "py" | "pyi" => Language::Python,
        "rs" => Language::Rust,
        "js" | "mjs" | "cjs" | "jsx" | "ts" | "tsx" | "mts" | "cts" => Language::Script,
        "c" | "h" | "cc" | "cpp" | "cxx" | "hpp" | "hh" | "hxx" | "cu" | "cuh" => Language::C,
        "sh" | "bash" | "zsh" => Language::Shell,
        "ps1" | "psm1" => Language::PowerShell,
        "toml" => Language::Toml,
        "md" | "markdown" => Language::Markdown,
        "tex" | "sty" | "cls" => Language::Tex,
        _ => return None,
    })
}

/// The name a line starts with, as a language writes names: a letter or `_` first, then letters,
/// digits and `_`, with `$` too where `dollar`. Gives the name and the rest of the line.
fn name_at(text: &str, dollar: bool) -> Option<(&str, &str)> {
    let first = text.chars().next()?;
    if !(first.is_ascii_alphabetic() || first == '_' || (dollar && first == '$')) {
        return None;
    }
    let end = text.find(|c: char| !(c.is_ascii_alphanumeric() || c == '_' || (dollar && c == '$'))).unwrap_or(text.len());
    Some((&text[..end], &text[end..]))
}

/// `text` with `word` and the spaces after it taken off its front, where it starts with the word.
fn after<'a>(text: &'a str, word: &str) -> Option<&'a str> {
    let rest = text.strip_prefix(word)?;
    let next = rest.chars().next();
    if next.is_some_and(|c| c.is_ascii_alphanumeric() || c == '_') {
        return None;
    }
    Some(rest.trim_start())
}

/// `text` with any of `words` taken off its front, as often as they stand there.
fn without<'a>(mut text: &'a str, words: &[&str]) -> &'a str {
    loop {
        let before = text;
        for word in words {
            if let Some(rest) = after(text, word) {
                text = rest;
            }
        }
        if text == before {
            return text;
        }
    }
}

fn python(line: &str, out: &mut Vec<(String, &'static str)>) {
    let trimmed = line.trim_start();
    let rest = without(trimmed, &["async"]);
    if let Some(rest) = after(rest, "def") {
        if let Some((name, _)) = name_at(rest, false) {
            out.push((name.to_string(), "function"));
        }
    } else if let Some(rest) = after(trimmed, "class") {
        if let Some((name, _)) = name_at(rest, false) {
            out.push((name.to_string(), "class"));
        }
    } else if trimmed.len() == line.len() {
        // A constant: a name in capitals at the left edge, given a value.
        if let Some((name, rest)) = name_at(line, false) {
            let upper = name.chars().all(|c| c.is_ascii_uppercase() || c.is_ascii_digit() || c == '_') && name.chars().any(|c| c.is_ascii_uppercase());
            let rest = rest.trim_start();
            let rest = rest.strip_prefix(':').map(|typed| typed.split_once('=').map_or("", |(_, value)| value)).map(|value| format!("={value}"));
            let assigned = match &rest {
                Some(value) => value.starts_with('=') && !value.starts_with("=="),
                None => {
                    let plain = line[name.len()..].trim_start();
                    plain.starts_with('=') && !plain.starts_with("==")
                }
            };
            if upper && assigned {
                out.push((name.to_string(), "constant"));
            }
        }
    }
}

fn rust(line: &str, out: &mut Vec<(String, &'static str)>) {
    let mut text = line.trim_start();
    if let Some(rest) = after(text, "pub") {
        text = rest;
        if text.starts_with('(') {
            text = text.split_once(')').map_or("", |(_, rest)| rest.trim_start());
        }
    }
    if let Some(rest) = text.strip_prefix("macro_rules!") {
        if let Some((name, _)) = name_at(rest.trim_start(), false) {
            out.push((name.to_string(), "function"));
        }
        return;
    }
    let mut leading = text;
    loop {
        let before = leading;
        leading = without(leading, &["const", "async", "unsafe"]);
        if let Some(rest) = after(leading, "extern") {
            leading = match rest.strip_prefix('"') {
                Some(named) => named.split_once('"').map_or("", |(_, rest)| rest.trim_start()),
                None => rest,
            };
        }
        if leading == before {
            break;
        }
    }
    if let Some(rest) = after(leading, "fn") {
        if let Some((name, _)) = name_at(rest, false) {
            out.push((name.to_string(), "function"));
        }
        return;
    }
    for (word, kind) in [("struct", "class"), ("enum", "class"), ("union", "class"), ("trait", "class"), ("type", "class"), ("mod", "module")] {
        if let Some(rest) = after(text, word) {
            if let Some((name, _)) = name_at(rest, false) {
                out.push((name.to_string(), kind));
            }
            return;
        }
    }
    for word in ["const", "static"] {
        if let Some(rest) = after(text, word) {
            let rest = without(rest, &["mut"]);
            if let Some((name, tail)) = name_at(rest, false) {
                if tail.trim_start().starts_with(':') {
                    out.push((name.to_string(), "constant"));
                }
            }
            return;
        }
    }
}

const SCRIPT_KEEPS: [&str; 7] = ["if", "for", "while", "switch", "catch", "return", "function"];

fn script(line: &str, out: &mut Vec<(String, &'static str)>) {
    let trimmed = line.trim_start();
    let indented = trimmed.len() != line.len();
    let lead = without(trimmed, &["export", "default", "async"]);
    if let Some(rest) = after(lead, "function") {
        let rest = rest.trim_start_matches('*').trim_start();
        if let Some((name, _)) = name_at(rest, true) {
            out.push((name.to_string(), "function"));
        }
        return;
    }
    let lead = without(trimmed, &["export", "default"]);
    if let Some(rest) = after(lead, "class") {
        if let Some((name, _)) = name_at(rest, true) {
            out.push((name.to_string(), "class"));
        }
        return;
    }
    if !indented {
        let declared = without(trimmed, &["export"]);
        for word in ["const", "let", "var"] {
            if let Some(rest) = after(declared, word) {
                if let Some((name, tail)) = name_at(rest, true) {
                    let tail = tail.trim_start();
                    if let Some(value) = tail.strip_prefix('=').filter(|value| !value.starts_with('=')) {
                        let value = without(value.trim_start(), &["async"]);
                        let arrow = (value.starts_with('(') && value.split_once(')').is_some_and(|(_, rest)| rest.trim_start().starts_with("=>")))
                            || name_at(value, true).is_some_and(|(_, rest)| rest.trim_start().starts_with("=>"));
                        out.push((name.to_string(), if arrow { "function" } else { "constant" }));
                    }
                }
                return;
            }
        }
        return;
    }
    // A method: an indented name and its parameters, its body opening at the line's end.
    let lead = without(trimmed, &["static", "async", "get", "set"]);
    if let Some((name, tail)) = name_at(lead, true) {
        if SCRIPT_KEEPS.contains(&name) {
            return;
        }
        let tail = tail.trim_start();
        if tail.starts_with('(') && tail.trim_end().ends_with('{') {
            if let Some((_, rest)) = tail.split_once(')') {
                if rest.trim() == "{" {
                    out.push((name.to_string(), "method"));
                }
            }
        }
    }
}

const C_KEEPS: [&str; 12] = ["if", "for", "while", "switch", "return", "else", "do", "typedef", "struct", "enum", "union", "sizeof"];

fn c(line: &str, out: &mut Vec<(String, &'static str)>) {
    if let Some(rest) = line.strip_prefix('#') {
        if let Some(rest) = after(rest.trim_start(), "define") {
            if let Some((name, _)) = name_at(rest, false) {
                out.push((name.to_string(), "constant"));
            }
        }
        return;
    }
    if line.starts_with(|c: char| c.is_whitespace()) || line.is_empty() {
        return;
    }
    let lead = without(line, &["typedef"]);
    for word in ["struct", "enum", "union", "class"] {
        if let Some(rest) = after(lead, word) {
            if let Some((name, tail)) = name_at(rest, false) {
                let tail = tail.trim();
                if tail.is_empty() || tail == "{" {
                    out.push((name.to_string(), "class"));
                }
            }
            return;
        }
    }
    // A function: words for its type, then its name and a parenthesis, at the left edge, not a
    // statement, and either opening its body or running on to the next line.
    let Some((first, _)) = name_at(line, false) else { return };
    if C_KEEPS.contains(&first) {
        return;
    }
    let Some(open) = line.find('(') else { return };
    let head = &line[..open];
    if head.contains('=') || head.contains(';') || head.contains('"') {
        return;
    }
    let head = head.trim_end();
    let start = head.rfind(|c: char| !(c.is_ascii_alphanumeric() || c == '_')).map_or(0, |at| at + 1);
    let name = &head[start..];
    if name.is_empty() || start == 0 || name.starts_with(|c: char| c.is_ascii_digit()) {
        return;
    }
    let tail = &line[open..];
    let body = tail.trim_end().ends_with('{');
    if body || !tail.contains(';') {
        out.push((name.to_string(), "function"));
    }
}

fn shell(line: &str, out: &mut Vec<(String, &'static str)>) {
    let trimmed = line.trim_start();
    let word = |text: &str| text.split(|c: char| !(c.is_ascii_alphanumeric() || c == '_' || c == '-')).next().map(str::to_string);
    if let Some(rest) = after(trimmed, "function") {
        if let Some(name) = word(rest).filter(|name| !name.is_empty()) {
            out.push((name, "function"));
        }
    } else if let Some(name) = word(trimmed).filter(|name| !name.is_empty() && name.starts_with(|c: char| c.is_ascii_alphabetic() || c == '_')) {
        if trimmed[name.len()..].trim_start().starts_with("()") {
            out.push((name, "function"));
        }
    }
}

fn powershell(line: &str, out: &mut Vec<(String, &'static str)>) {
    let trimmed = line.trim_start();
    if trimmed.len() > 9 && trimmed[..9].eq_ignore_ascii_case("function ") {
        let name: String = trimmed[9..].trim_start().chars().take_while(|c| c.is_ascii_alphanumeric() || *c == '_' || *c == '-').collect();
        if !name.is_empty() {
            out.push((name, "function"));
        }
    }
}

fn toml(line: &str, out: &mut Vec<(String, &'static str)>) {
    if let Some(rest) = line.strip_prefix('[') {
        let rest = rest.trim_start_matches('[');
        if let Some((name, _)) = rest.split_once(']') {
            let name = name.trim();
            if !name.is_empty() {
                out.push((name.to_string(), "module"));
            }
        }
    }
}

fn markdown(line: &str, out: &mut Vec<(String, &'static str)>) {
    let marks = line.chars().take_while(|c| *c == '#').count();
    if (1..=6).contains(&marks) && line[marks..].starts_with(' ') {
        let title = line[marks..].trim().trim_end_matches('#').trim();
        if !title.is_empty() {
            out.push((title.to_string(), "heading"));
        }
    }
}

const TEX_LEVELS: [&str; 6] = ["part", "chapter", "section", "subsection", "subsubsection", "paragraph"];

fn tex(line: &str, out: &mut Vec<(String, &'static str)>) {
    let mut text = line;
    while let Some(at) = text.find('\\') {
        text = &text[at + 1..];
        let Some((word, rest)) = name_at(text, false) else { continue };
        if !TEX_LEVELS.contains(&word) {
            continue;
        }
        let rest = rest.strip_prefix('*').unwrap_or(rest).trim_start();
        let rest = if rest.starts_with('[') { rest.split_once(']').map_or("", |(_, rest)| rest.trim_start()) } else { rest };
        if let Some(inner) = rest.strip_prefix('{') {
            if let Some((title, _)) = inner.split_once('}') {
                out.push((title.trim().to_string(), "heading"));
            }
        }
        return;
    }
}

/// Every declaration in `text`, a file in `language`, each with its line counted from 0.
fn declared(language: Language, text: &str) -> Vec<(String, &'static str, u32)> {
    let mut found = Vec::new();
    let mut fenced = false;
    for (at, line) in text.lines().enumerate() {
        let mut out = Vec::new();
        match language {
            Language::Python => python(line, &mut out),
            Language::Rust => rust(line, &mut out),
            Language::Script => script(line, &mut out),
            Language::C => c(line, &mut out),
            Language::Shell => shell(line, &mut out),
            Language::PowerShell => powershell(line, &mut out),
            Language::Toml => toml(line, &mut out),
            Language::Markdown => {
                if line.trim_start().starts_with("```") {
                    fenced = !fenced;
                } else if !fenced {
                    markdown(line, &mut out);
                }
            }
            Language::Tex => tex(line, &mut out),
        }
        found.extend(out.into_iter().map(|(name, kind)| (name, kind, at as u32)));
    }
    found
}

/// The declarations of the file at `path` under `root`, as written in that file now.
pub fn declared_in(root: &Path, path: &str) -> Vec<Declared> {
    let Some(language) = language_of(path) else { return Vec::new() };
    let Ok(text) = fs::read(root.join(path)) else { return Vec::new() };
    declared(language, &String::from_utf8_lossy(&text))
        .into_iter()
        .map(|(name, kind, line)| Declared { name, kind, path: path.to_string(), line })
        .collect()
}

/// How well `query` names `name`, higher the better, or None where it does not: the name itself
/// first, then a name it begins, then one holding it, then one whose words it starts, as `gtf` starts
/// `go_to_file` and `goToFile`, then one holding its letters in order. Case is set aside, and the
/// shorter of two names that answer alike comes first.
pub fn score(query: &str, name: &str) -> Option<i64> {
    if query.is_empty() {
        return Some(0);
    }
    let asked = query.to_lowercase();
    let lower = name.to_lowercase();
    let shortness = -(name.len() as i64);
    if lower == asked {
        return Some(10_000 + if name == query { 50 } else { 0 });
    }
    if lower.starts_with(&asked) {
        return Some(8_000 + shortness);
    }
    if let Some(at) = lower.find(&asked) {
        return Some(6_000 - at as i64 * 10 + shortness);
    }
    // The first letter of each word: after `_`, `-`, `.`, a space, or a capital after a small letter.
    let chars: Vec<char> = name.chars().collect();
    let initials: String = chars
        .iter()
        .enumerate()
        .filter(|(at, c)| {
            *at == 0 || matches!(chars[at - 1], '_' | '-' | '.' | ' ') || (c.is_uppercase() && chars[at - 1].is_lowercase())
        })
        .map(|(_, c)| c.to_ascii_lowercase())
        .filter(|c| c.is_alphanumeric())
        .collect();
    if initials.starts_with(&asked) {
        return Some(4_000 + shortness);
    }
    let mut letters = lower.chars();
    if asked.chars().all(|wanted| letters.any(|c| c == wanted)) {
        return Some(1_000 + shortness);
    }
    None
}

struct Held {
    written: SystemTime,
    symbols: Vec<(String, &'static str, u32)>,
}

#[derive(Default)]
struct State {
    root: Option<PathBuf>,
    files: HashMap<String, Held>,
    reading: bool,
    read_at: Option<Instant>,
}

/// The declarations of one tree's files, kept up to date as files change.
#[derive(Default)]
pub struct Index {
    state: Arc<Mutex<State>>,
}

impl Index {
    /// Forgets every file, for a tree opened in place of the last.
    pub fn forget(&self) {
        if let Ok(mut state) = self.state.lock() {
            *state = State::default();
        }
    }

    /// Brings the index up to date on a thread of its own, unless it is being brought up to date
    /// already or was within FRESH_FOR. `files` lists the tree's files, as `files::all` gives them.
    pub fn refresh(&self, root: &Path, files: impl FnOnce() -> Vec<String> + Send + 'static) {
        {
            let Ok(mut state) = self.state.lock() else { return };
            if state.root.as_deref() != Some(root) {
                *state = State { root: Some(root.to_path_buf()), ..State::default() };
            }
            if state.reading || state.read_at.is_some_and(|at| at.elapsed() < FRESH_FOR) {
                return;
            }
            state.reading = true;
        }
        let shared = self.state.clone();
        let root = root.to_path_buf();
        std::thread::spawn(move || {
            let listed = files();
            let mut seen = std::collections::HashSet::new();
            for path in listed {
                if language_of(&path).is_none() {
                    continue;
                }
                let Ok(meta) = fs::metadata(root.join(&path)) else { continue };
                if meta.len() > LARGEST {
                    continue;
                }
                let written = meta.modified().unwrap_or(SystemTime::UNIX_EPOCH);
                seen.insert(path.clone());
                let current = shared.lock().ok().and_then(|state| state.files.get(&path).map(|held| held.written == written));
                if current == Some(true) {
                    continue;
                }
                let Some(language) = language_of(&path) else { continue };
                let Ok(bytes) = fs::read(root.join(&path)) else { continue };
                let symbols = declared(language, &String::from_utf8_lossy(&bytes));
                let Ok(mut state) = shared.lock() else { return };
                if state.root.as_deref() != Some(root.as_path()) {
                    return;
                }
                state.files.insert(path, Held { written, symbols });
            }
            if let Ok(mut state) = shared.lock() {
                if state.root.as_deref() == Some(root.as_path()) {
                    state.files.retain(|path, _| seen.contains(path));
                    state.reading = false;
                    state.read_at = Some(Instant::now());
                }
            }
        });
    }

    /// The declarations whose names answer `query`, at most `most` of them, the best first.
    pub fn find(&self, query: &str, most: usize) -> Found {
        let Ok(state) = self.state.lock() else { return Found { symbols: Vec::new(), reading: false } };
        let mut scored: Vec<(i64, &str, &str, &'static str, u32)> = Vec::new();
        for (path, held) in &state.files {
            for (name, kind, line) in &held.symbols {
                if let Some(points) = score(query, name) {
                    scored.push((points, name, path, kind, *line));
                }
            }
        }
        scored.sort_by(|a, b| b.0.cmp(&a.0).then_with(|| a.1.cmp(b.1)).then_with(|| a.2.cmp(b.2)).then_with(|| a.4.cmp(&b.4)));
        scored.truncate(most);
        Found {
            symbols: scored.into_iter().map(|(_, name, path, kind, line)| Declared { name: name.to_string(), kind, path: path.to_string(), line }).collect(),
            reading: state.reading,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn names(language: Language, text: &str) -> Vec<(String, &'static str, u32)> {
        declared(language, text)
    }

    #[test]
    fn python_declarations() {
        let text = "import os\nLIMIT = 3\nlimit = 4\nX: int = 5\nclass Page:\n    def read(self):\n        pass\nasync def go():\n    if a == b:\n        pass\n";
        assert_eq!(
            names(Language::Python, text),
            vec![("LIMIT".into(), "constant", 1), ("X".into(), "constant", 3), ("Page".into(), "class", 4), ("read".into(), "function", 5), ("go".into(), "function", 7)]
        );
    }

    #[test]
    fn rust_declarations() {
        let text = "pub(crate) fn alpha() {}\npub struct Beta;\nconst GAMMA: u32 = 1;\nmacro_rules! delta {\n}\npub async unsafe fn epsilon() {}\nmod zeta;\nimpl Beta {\nextern \"C\" fn eta() {}\nstatic mut THETA: u8 = 0;\n";
        assert_eq!(
            names(Language::Rust, text),
            vec![
                ("alpha".into(), "function", 0),
                ("Beta".into(), "class", 1),
                ("GAMMA".into(), "constant", 2),
                ("delta".into(), "function", 3),
                ("epsilon".into(), "function", 5),
                ("zeta".into(), "module", 6),
                ("eta".into(), "function", 8),
                ("THETA".into(), "constant", 9),
            ]
        );
    }

    #[test]
    fn script_declarations() {
        let text = "export async function load(path) {\nclass Editor {\n  paint() {\n    if (x) {\n  }\nconst STEP = 4;\nexport const go = (a) => a;\nlet run = async () => 1;\n";
        assert_eq!(
            names(Language::Script, text),
            vec![
                ("load".into(), "function", 0),
                ("Editor".into(), "class", 1),
                ("paint".into(), "method", 2),
                ("STEP".into(), "constant", 5),
                ("go".into(), "function", 6),
                ("run".into(), "function", 7),
            ]
        );
    }

    #[test]
    fn c_declarations() {
        let text = "#define LIMIT 4\n#  define TWICE(x) ((x) * 2)\nstruct grid {\ntypedef enum shade\nstatic int count_cells(const struct grid *g) {\nint scale(int a,\n  return helper(1);\nint value = make(2);\nvoid declared(int);\nif (x) {\n";
        assert_eq!(
            names(Language::C, text),
            vec![
                ("LIMIT".into(), "constant", 0),
                ("TWICE".into(), "constant", 1),
                ("grid".into(), "class", 2),
                ("shade".into(), "class", 3),
                ("count_cells".into(), "function", 4),
                ("scale".into(), "function", 5),
            ]
        );
    }

    #[test]
    fn other_declarations() {
        assert_eq!(names(Language::Shell, "build() {\nfunction clean {\necho hi\n"), vec![("build".into(), "function", 0), ("clean".into(), "function", 1)]);
        assert_eq!(names(Language::PowerShell, "Function Get-Thing {\n"), vec![("Get-Thing".into(), "function", 0)]);
        assert_eq!(names(Language::Toml, "[package]\n[[bin]]\nname = 1\n"), vec![("package".into(), "module", 0), ("bin".into(), "module", 1)]);
        assert_eq!(names(Language::Markdown, "# Title\n```\n# not this\n```\n## Part two ##\n"), vec![("Title".into(), "heading", 0), ("Part two".into(), "heading", 4)]);
        assert_eq!(names(Language::Tex, "\\section*{Start}\n\\subsection[short]{Long}\n"), vec![("Start".into(), "heading", 0), ("Long".into(), "heading", 1)]);
    }

    #[test]
    fn scores_rank_the_name_itself_first() {
        let exact = score("paint", "paint").unwrap();
        let begins = score("paint", "paintStrip").unwrap();
        let holds = score("paint", "repaint").unwrap();
        let initials = score("gtf", "go_to_file").unwrap();
        let camel = score("gtf", "goToFile").unwrap();
        let letters = score("pnt", "paint").unwrap();
        assert!(exact > begins && begins > holds && holds > initials && initials > letters);
        assert_eq!(initials, score("gtf", "go_to_file").unwrap());
        assert!(camel > letters);
        assert_eq!(score("xyz", "paint"), None);
    }

    #[test]
    fn index_reads_a_tree_and_reads_a_file_again_once_it_changes() {
        let root = std::env::temp_dir().join(format!("orior_symbols_{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        fs::create_dir_all(&root).unwrap();
        fs::write(root.join("a.py"), "def first():\n    pass\n").unwrap();
        let index = Index::default();
        let wait_read = |index: &Index| {
            for _ in 0..200 {
                if !index.find("", 1).reading {
                    return;
                }
                std::thread::sleep(std::time::Duration::from_millis(10));
            }
        };
        index.refresh(&root, || vec!["a.py".to_string(), "notes.txt".to_string()]);
        wait_read(&index);
        let found = index.find("first", 10);
        assert_eq!(found.symbols, vec![Declared { name: "first".into(), kind: "function", path: "a.py".into(), line: 0 }]);
        std::thread::sleep(std::time::Duration::from_millis(20));
        fs::write(root.join("a.py"), "\ndef second():\n    pass\n").unwrap();
        if let Ok(mut state) = index.state.lock() {
            state.read_at = None;
        }
        index.refresh(&root, || vec!["a.py".to_string()]);
        wait_read(&index);
        assert!(index.find("first", 10).symbols.is_empty());
        assert_eq!(index.find("second", 10).symbols[0].line, 1);
        let _ = fs::remove_dir_all(&root);
    }
}

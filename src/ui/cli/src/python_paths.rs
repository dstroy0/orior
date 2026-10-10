// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The folders a tree's Python scripts put on `sys.path` as they run, read from their text with no
//! setup, for Python's server to search for the modules the scripts import from them. A script that
//! writes `sys.path.insert(0, os.path.join(ROOT, "src", "python"))` names a folder its server knows
//! nothing of until it is told.
//!
//! What is read: `sys.path.insert`, `sys.path.append`, `sys.path.extend` and `sys.path[:0] = [...]`,
//! of an expression made of `__file__`, strings, the names the script set before it, `os.path`'s
//! `dirname`, `abspath`, `realpath`, `normpath` and `join`, `pathlib.Path` with `parent`, `parents`,
//! `resolve`, `/` and `joinpath`, `str`, `os.fspath`, and a string's `rsplit(sep, 1)[0]`. A name set
//! again from itself, as a loop walks up a folder at a time until a folder is there, stands for each
//! folder above its first value; a name a `for` takes from a tuple or a list stands for each of them,
//! and one it takes from `os.scandir`, `os.listdir` or `iterdir` for each folder in that folder. Of
//! what an expression may be, the folders that are there in the tree are kept.

use std::collections::HashMap;
use std::path::{Path, PathBuf};
use std::sync::Mutex;
use std::time::{Duration, Instant};

/// A file larger than this is not read.
const LARGEST: u64 = 1 << 20;

/// How long a tree's reading stands before it is read again.
const FRESH_FOR: Duration = Duration::from_secs(30);

/// The folders a search does not go into: environments, what a build writes, and caches.
const NOT_SEARCHED: [&str; 8] = ["venv", ".venv", "env", "node_modules", "site-packages", "__pycache__", ".tox", ".nox"];

/// An expression as the reading takes one.
#[derive(Clone, Debug)]
enum Expr {
    Name(String),
    Text(String),
    Number(i64),
    Call(Box<Expr>, Vec<Expr>),
    Attr(Box<Expr>, String),
    Index(Box<Expr>, Box<Expr>),
    Join(Box<Expr>, Box<Expr>),
    Add(Box<Expr>, Box<Expr>),
    Many(Vec<Expr>),
}

/// A reading of an expression's text.
struct Parser {
    chars: Vec<char>,
    at: usize,
}

impl Parser {
    fn blank(&mut self) {
        while self.chars.get(self.at).is_some_and(|char| char.is_whitespace() || *char == '\\') {
            self.at += 1;
        }
    }

    fn eat(&mut self, char: char) -> bool {
        self.blank();
        if self.chars.get(self.at) == Some(&char) {
            self.at += 1;
            true
        } else {
            false
        }
    }

    fn expr(&mut self) -> Option<Expr> {
        let mut left = self.postfix()?;
        loop {
            if self.eat('/') {
                left = Expr::Join(Box::new(left), Box::new(self.postfix()?));
            } else if self.eat('+') {
                left = Expr::Add(Box::new(left), Box::new(self.postfix()?));
            } else {
                return Some(left);
            }
        }
    }

    fn postfix(&mut self) -> Option<Expr> {
        let mut value = self.primary()?;
        loop {
            if self.eat('.') {
                self.blank();
                let name = self.word()?;
                value = Expr::Attr(Box::new(value), name);
            } else if self.eat('(') {
                let args = self.list(')')?;
                value = Expr::Call(Box::new(value), args);
            } else if self.eat('[') {
                self.blank();
                let index = if self.chars.get(self.at) == Some(&':') {
                    while self.chars.get(self.at).is_some_and(|char| *char != ']') {
                        self.at += 1;
                    }
                    Expr::Number(0)
                } else {
                    self.expr()?
                };
                if !self.eat(']') {
                    return None;
                }
                value = Expr::Index(Box::new(value), Box::new(index));
            } else {
                return Some(value);
            }
        }
    }

    /// Expressions parted by `,` up to `close`, a keyword argument's name passed over.
    fn list(&mut self, close: char) -> Option<Vec<Expr>> {
        let mut items = Vec::new();
        loop {
            if self.eat(close) {
                return Some(items);
            }
            let start = self.at;
            if let Some(name) = self.word() {
                self.blank();
                if self.chars.get(self.at) == Some(&'=') && self.chars.get(self.at + 1) != Some(&'=') {
                    self.at += 1;
                    let _ = name;
                } else {
                    self.at = start;
                }
            }
            items.push(self.expr()?);
            if !self.eat(',') {
                return self.eat(close).then_some(items);
            }
        }
    }

    fn word(&mut self) -> Option<String> {
        let start = self.at;
        while self.chars.get(self.at).is_some_and(|char| char.is_alphanumeric() || *char == '_') {
            self.at += 1;
        }
        (self.at > start).then(|| self.chars[start..self.at].iter().collect())
    }

    fn primary(&mut self) -> Option<Expr> {
        self.blank();
        let char = *self.chars.get(self.at)?;
        if char == '(' {
            self.at += 1;
            let mut items = self.list(')')?;
            return Some(if items.len() == 1 { items.remove(0) } else { Expr::Many(items) });
        }
        if char == '[' {
            self.at += 1;
            return Some(Expr::Many(self.list(']')?));
        }
        if char.is_ascii_digit() {
            let digits = self.word()?;
            return digits.parse().ok().map(Expr::Number);
        }
        let mark = self.at;
        let prefix: String = self.chars[self.at..].iter().take_while(|one| one.is_ascii_alphabetic()).collect();
        let quote_at = self.at + prefix.len();
        if prefix.len() <= 2 && matches!(self.chars.get(quote_at), Some('"' | '\'')) {
            let raw = prefix.to_lowercase().contains('r');
            if prefix.to_lowercase().contains('f') {
                return None;
            }
            let quote = self.chars[quote_at];
            let mut text = String::new();
            self.at = quote_at + 1;
            while let Some(&next) = self.chars.get(self.at) {
                self.at += 1;
                if next == quote {
                    return Some(Expr::Text(text));
                }
                if next == '\\' && !raw {
                    if let Some(&escaped) = self.chars.get(self.at) {
                        self.at += 1;
                        text.push(match escaped {
                            'n' => '\n',
                            't' => '\t',
                            other => other,
                        });
                    }
                    continue;
                }
                text.push(next);
            }
            return None;
        }
        self.at = mark;
        self.word().map(Expr::Name)
    }
}

fn parse(text: &str) -> Option<Expr> {
    let mut parser = Parser { chars: text.chars().collect(), at: 0 };
    let expr = parser.expr()?;
    parser.blank();
    (parser.at >= parser.chars.len()).then_some(expr)
}

/// The dotted name an expression of names and attributes writes, as `os.path.join`.
fn dotted(expr: &Expr) -> Option<String> {
    match expr {
        Expr::Name(name) => Some(name.clone()),
        Expr::Attr(of, name) => Some(format!("{}.{name}", dotted(of)?)),
        _ => None,
    }
}

/// The folder that holds `path`, as `os.path.dirname` gives it.
fn parent(path: &str) -> String {
    match path.rfind(['/', '\\']) {
        Some(0) => path[..1].to_string(),
        Some(at) if at == 2 && path.as_bytes().get(1) == Some(&b':') => path[..3].to_string(),
        Some(at) => path[..at].to_string(),
        None => String::new(),
    }
}

/// `part` joined onto `base` as `os.path.join` joins them: an absolute part stands alone.
fn join(base: &str, part: &str) -> String {
    if Path::new(part).is_absolute() || base.is_empty() {
        return part.to_string();
    }
    if base.ends_with(['/', '\\']) {
        format!("{base}{part}")
    } else {
        format!("{base}{}{part}", std::path::MAIN_SEPARATOR)
    }
}

/// Every pairing of the values of `a` and `b`, put together by `with`.
fn pairs(a: Vec<String>, b: Vec<String>, with: impl Fn(&str, &str) -> String) -> Vec<String> {
    a.iter().flat_map(|one| b.iter().map(|two| with(one, two)).collect::<Vec<_>>()).take(256).collect()
}

/// What `expr` may be, by the names `names` holds, in the file at `file`.
fn eval(expr: &Expr, names: &HashMap<String, Vec<String>>, file: &str) -> Vec<String> {
    match expr {
        Expr::Name(name) if name == "__file__" => vec![file.to_string()],
        Expr::Name(name) => names.get(name).cloned().unwrap_or_default(),
        Expr::Text(text) => vec![text.clone()],
        Expr::Number(_) => Vec::new(),
        Expr::Many(items) => items.iter().flat_map(|item| eval(item, names, file)).collect(),
        Expr::Join(a, b) => pairs(eval(a, names, file), eval(b, names, file), join),
        Expr::Add(a, b) => pairs(eval(a, names, file), eval(b, names, file), |one, two| format!("{one}{two}")),
        Expr::Attr(of, name) => match name.as_str() {
            "parent" => eval(of, names, file).iter().map(|one| parent(one)).collect(),
            "path" => eval(of, names, file),
            _ => Vec::new(),
        },
        Expr::Index(of, index) => {
            if let (Expr::Attr(base, name), Expr::Number(steps)) = (of.as_ref(), index.as_ref()) {
                if name == "parents" {
                    return eval(base, names, file).iter().map(|one| (0..=*steps).fold(one.clone(), |at, _| parent(&at))).collect();
                }
            }
            if let (Expr::Call(callee, args), Expr::Number(0)) = (of.as_ref(), index.as_ref()) {
                if let (Expr::Attr(base, method), Some(Expr::Text(sep))) = (callee.as_ref(), args.first()) {
                    if method == "rsplit" {
                        return eval(base, names, file).iter().map(|one| one.rsplit_once(sep.as_str()).map_or(one.clone(), |(head, _)| head.to_string())).collect();
                    }
                }
            }
            Vec::new()
        }
        Expr::Call(callee, args) => {
            let values = |at: usize| args.get(at).map(|arg| eval(arg, names, file)).unwrap_or_default();
            match dotted(callee).as_deref() {
                Some("os.path.dirname") => values(0).iter().map(|one| parent(one)).collect(),
                Some("os.path.abspath" | "os.path.realpath" | "os.path.normpath" | "str" | "os.fspath") => values(0),
                Some("os.path.join" | "Path" | "pathlib.Path" | "PurePath" | "pathlib.PurePath") => {
                    let mut all = values(0);
                    for at in 1..args.len() {
                        all = pairs(all, values(at), join);
                    }
                    all
                }
                _ => match callee.as_ref() {
                    Expr::Attr(of, method) if matches!(method.as_str(), "resolve" | "absolute") => eval(of, names, file),
                    Expr::Attr(of, method) if method == "joinpath" => {
                        let mut all = eval(of, names, file);
                        for at in 0..args.len() {
                            all = pairs(all, values(at), join);
                        }
                        all
                    }
                    _ => Vec::new(),
                },
            }
        }
    }
}

/// The folders in `folder`, each a value a loop over its entries takes.
fn folders_in(folder: &str) -> Vec<String> {
    let mut found: Vec<String> = std::fs::read_dir(folder).into_iter().flatten().flatten().filter(|entry| entry.path().is_dir()).map(|entry| entry.path().display().to_string()).collect();
    found.sort();
    found
}

/// The folders above `path`, itself first, up to the top of its drive.
fn upward(path: &str) -> Vec<String> {
    let mut found = vec![path.to_string()];
    let mut at = path.to_string();
    loop {
        let up = parent(&at);
        if up.is_empty() || up == at {
            return found;
        }
        found.push(up.clone());
        at = up;
    }
}

/// The logical lines of a file's text: a line whose brackets stay open goes on with the next.
fn logical_lines(text: &str) -> Vec<String> {
    let mut out = Vec::new();
    let mut open = 0i32;
    let mut line = String::new();
    for raw in text.lines() {
        let code = raw.split(" #").next().unwrap_or(raw);
        if !line.is_empty() {
            line.push(' ');
        }
        line.push_str(code.trim_end_matches('\\'));
        open += code.chars().filter(|char| "([{".contains(*char)).count() as i32 - code.chars().filter(|char| ")]}".contains(*char)).count() as i32;
        if open <= 0 && !code.trim_end().ends_with('\\') {
            out.push(std::mem::take(&mut line));
            open = 0;
        }
    }
    if !line.is_empty() {
        out.push(line);
    }
    out
}

/// The folders the script at `file`, its text `text`, puts on `sys.path`, as it may write them.
fn paths_in(file: &str, text: &str) -> Vec<String> {
    // `__file__` is written with the system's separator, as Python gives it.
    let file = &if cfg!(windows) { file.replace('/', "\\") } else { file.to_string() };
    let mut names: HashMap<String, Vec<String>> = HashMap::new();
    let mut found = Vec::new();
    for line in logical_lines(text) {
        let code = line.trim();
        if let Some(rest) = code.strip_prefix("for ") {
            if let Some((name, iterable)) = rest.split_once(" in ") {
                let name = name.trim();
                let iterable = iterable.trim().trim_end_matches(':').trim();
                if let Some(expr) = parse(iterable) {
                    let values = match &expr {
                        Expr::Call(callee, args) if matches!(dotted(callee).as_deref(), Some("os.scandir" | "os.listdir")) => args.first().map(|arg| eval(arg, &names, file)).unwrap_or_default().iter().flat_map(|one| folders_in(one)).collect(),
                        Expr::Call(callee, _) if matches!(callee.as_ref(), Expr::Attr(_, method) if method == "iterdir") => match callee.as_ref() {
                            Expr::Attr(of, _) => eval(of, &names, file).iter().flat_map(|one| folders_in(one)).collect(),
                            _ => Vec::new(),
                        },
                        other => eval(other, &names, file),
                    };
                    if name.chars().all(|char| char.is_alphanumeric() || char == '_') {
                        names.insert(name.to_string(), values);
                    }
                }
            }
            continue;
        }
        for (lead, close) in [("sys.path.insert(", ")"), ("sys.path.append(", ")"), ("sys.path.extend(", ")")] {
            if let Some(at) = code.find(lead) {
                let inner = &code[at + lead.len()..];
                let Some(end) = inner.rfind(close) else {
                    continue;
                };
                let args = &inner[..end];
                let given = if lead.contains("insert") { args.split_once(',').map(|(_, rest)| rest).unwrap_or("") } else { args };
                if let Some(expr) = parse(given.trim().trim_end_matches(',').trim()) {
                    found.extend(eval(&expr, &names, file));
                }
            }
        }
        if let Some(rest) = code.strip_prefix("sys.path[") {
            if let Some((_, value)) = rest.split_once("] =") {
                if let Some(expr) = parse(value.trim()) {
                    found.extend(eval(&expr, &names, file));
                }
            }
        }
        let Some((target, value)) = code.split_once('=') else {
            continue;
        };
        let target = target.split(':').next().unwrap_or(target).trim();
        if value.starts_with('=') || target.is_empty() || !target.chars().all(|char| char.is_alphanumeric() || char == '_') || target.ends_with(['!', '<', '>']) {
            continue;
        }
        if let Some(expr) = parse(value.trim()) {
            let reads_itself = format!("{value} ").split(|char: char| !(char.is_alphanumeric() || char == '_')).any(|word| word == target);
            let values = if reads_itself {
                names.get(target).map(|now| now.iter().flat_map(|one| upward(one)).collect()).unwrap_or_default()
            } else {
                eval(&expr, &names, file)
            };
            if !values.is_empty() {
                names.insert(target.to_string(), values);
            }
        }
    }
    found
}

static READ: Mutex<Option<(PathBuf, Instant, Vec<PathBuf>)>> = Mutex::new(None);

/// The folders the Python scripts of the tree at `root` put on `sys.path`, each a folder there in the
/// tree other than its top, the most used first.
pub fn search_paths(root: &Path) -> Vec<PathBuf> {
    if let Ok(read) = READ.lock() {
        if let Some((at, when, paths)) = read.as_ref() {
            if at == root && when.elapsed() < FRESH_FOR {
                return paths.clone();
            }
        }
    }
    let top = dunce::canonicalize(root).unwrap_or_else(|_| root.to_path_buf());
    let mut counts: HashMap<PathBuf, usize> = HashMap::new();
    for file in crate::files::all(root).into_iter().filter(|file| file.ends_with(".py") && !file.split('/').any(|part| NOT_SEARCHED.contains(&part))) {
        let path = root.join(&file);
        if !std::fs::metadata(&path).is_ok_and(|meta| meta.len() <= LARGEST) {
            continue;
        }
        let Ok(text) = std::fs::read_to_string(&path) else {
            continue;
        };
        if !text.contains("sys.path") {
            continue;
        }
        let mut seen = Vec::new();
        for one in paths_in(&path.display().to_string(), &text) {
            let Ok(folder) = dunce::canonicalize(&one) else {
                continue;
            };
            if folder.is_dir() && folder.starts_with(&top) && folder != top && !seen.contains(&folder) {
                seen.push(folder);
            }
        }
        for folder in seen {
            *counts.entry(folder).or_insert(0) += 1;
        }
    }
    let mut paths: Vec<(PathBuf, usize)> = counts.into_iter().collect();
    paths.sort_by(|a, b| b.1.cmp(&a.1).then_with(|| a.0.cmp(&b.0)));
    let paths: Vec<PathBuf> = paths.into_iter().map(|(path, _)| path).collect();
    if let Ok(mut read) = READ.lock() {
        *read = Some((root.to_path_buf(), Instant::now(), paths.clone()));
    }
    paths
}

#[cfg(test)]
mod tests {
    use super::*;

    fn tree() -> PathBuf {
        let dir = std::env::temp_dir().join(format!("orior-python-paths-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        for folder in ["src/python/pkg", "examples/art/measure", "examples/proofing", "tools/plugins/one", "tools/plugins/two", "shared"] {
            std::fs::create_dir_all(dir.join(folder)).unwrap();
        }
        dunce::canonicalize(&dir).unwrap()
    }

    #[test]
    fn the_folders_a_script_puts_on_its_path_are_read() {
        let dir = tree();
        let file = dir.join("examples/art/measure/a.py").display().to_string();
        let walked = "import os, sys\nROOT = os.path.dirname(os.path.abspath(__file__))\nwhile not os.path.isdir(os.path.join(ROOT, \"src\", \"python\")):\n    ROOT = os.path.dirname(ROOT)\nsys.path.insert(0, os.path.join(ROOT, \"src\", \"python\"))\n";
        let found: Vec<PathBuf> = paths_in(&file, walked).iter().filter_map(|one| dunce::canonicalize(one).ok()).filter(|one| one.is_dir()).collect();
        assert_eq!(found, [dir.join("src").join("python")]);
        let named = "HERE = os.path.dirname(os.path.abspath(__file__))\nROOT = os.path.dirname(os.path.dirname(HERE))\nPROOFING = os.path.join(ROOT, \"proofing\")\nfor _where in (HERE, PROOFING):\n    if _where not in sys.path:\n        sys.path.insert(0, _where)\n";
        let found: Vec<PathBuf> = paths_in(&file, named).iter().filter_map(|one| dunce::canonicalize(one).ok()).collect();
        assert_eq!(found, [dir.join("examples").join("art").join("measure"), dir.join("examples").join("proofing")]);
        let pathlib = "from pathlib import Path\nsys.path.append(str(Path(__file__).resolve().parents[2] / \"proofing\"))\nsys.path.insert(\n    0,\n    __file__.rsplit(\"\\\\\", 1)[0].rsplit(\"/\", 1)[0],\n)\n";
        let found: Vec<PathBuf> = paths_in(&file, pathlib).iter().filter_map(|one| dunce::canonicalize(one).ok()).collect();
        assert_eq!(found, [dir.join("examples").join("proofing"), dir.join("examples").join("art").join("measure")]);
        let scanned = format!("for _category in os.scandir({:?}):\n    if _category.is_dir():\n        sys.path.insert(0, _category.path)\n", dir.join("tools/plugins").display().to_string());
        let found: Vec<PathBuf> = paths_in(&file, &scanned).iter().filter_map(|one| dunce::canonicalize(one).ok()).collect();
        assert_eq!(found, [dir.join("tools").join("plugins").join("one"), dir.join("tools").join("plugins").join("two")]);
        let _ = std::fs::remove_dir_all(dir);
    }

    #[test]
    fn expressions_read_as_python_writes_them() {
        assert!(matches!(parse("os.path.join(ROOT, 'a', \"b\")"), Some(Expr::Call(_, args)) if args.len() == 3));
        assert!(parse("f\"{ROOT}/x\"").is_none());
        assert!(matches!(parse("(HERE, PROOFING)"), Some(Expr::Many(items)) if items.len() == 2));
        assert_eq!(parent("D:\\a\\b"), "D:\\a");
        assert_eq!(parent("D:\\a"), "D:\\");
        assert_eq!(parent("/a/b"), "/a");
    }
}

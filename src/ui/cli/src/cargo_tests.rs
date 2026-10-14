// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! A tree's Rust tests, found as Cargo and the test harness find them, and run with cargo test.
//!
//! Each Cargo.toml of the tree that makes a package gives its targets: the library, each binary and
//! each test, by its [lib], [[bin]] and [[test]] tables and by the files Cargo finds on its own. Each
//! target's modules are followed from its first file, `mod name;` by `mod name;`, and in each file
//! every function marked `#[test]`, or with an attribute whose path ends in `test` as `#[tokio::test]`,
//! is a test. A test is named as orior names a Python one, its file first, `src/files.rs::listing::name`,
//! and runs by the name the harness gives it, `files::listing::name`.
//!
//! A run gives each target its own cargo test, every target's at once, with the words rust-analyzer
//! gives an editor for a test: the package by its name, the target, `--no-fail-fast`, and the names
//! of the tests asked of it given to the harness with `--exact`, or none where every test of it is
//! asked, with `--show-output`, and `--include-ignored` where each test is asked by its name. The harness's
//! own report is read, and RUSTC_BOOTSTRAP is not set for its report in JSON: Cargo builds every crate
//! again for it, and again for the next build without it. Each result is passed on as the harness
//! prints it, and each failure's message and the line it failed at as its process ends, from the
//! panic it reports; a target whose build fails fails each of its tests with the compiler's first
//! error.

use std::collections::{HashMap, HashSet};
use std::io::{BufRead, BufReader, Read as _};
use std::path::{Path, PathBuf};
use std::process::{Child, Command, Stdio};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use std::thread::JoinHandle;

use crate::test_runs::{Heard, Outcome, Tell};
use crate::testing::Test;

/// Folders a search for packages does not go into: what Cargo builds, and what other tools keep.
const NOT_SEARCHED: [&str; 4] = ["target", "node_modules", ".git", "build"];

/// The longest list of names a run gives the harness, in characters; past it a target runs whole and
/// only the results asked of it are told.
const NAMES_MOST: usize = 16_000;

/// A target of a package as Cargo builds it: the library, a binary or a test, by its name.
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub enum Target {
    Lib,
    Bin(String),
    Test(String),
}

impl Target {
    /// The words that ask Cargo for the target alone.
    fn words(&self) -> Vec<String> {
        match self {
            Target::Lib => vec!["--lib".into()],
            Target::Bin(name) => vec!["--bin".into(), name.clone()],
            Target::Test(name) => vec!["--test".into(), name.clone()],
        }
    }

    /// Whether Cargo's word of an artifact, its target's kinds and name, is of this target.
    fn is(&self, kinds: &[&str], name: &str) -> bool {
        match self {
            Target::Lib => kinds.iter().any(|kind| kind.ends_with("lib") || *kind == "proc-macro"),
            Target::Bin(own) => kinds.contains(&"bin") && own == name,
            Target::Test(own) => kinds.contains(&"test") && own == name,
        }
    }
}

/// A test as Cargo runs it: the test, its package's manifest in the tree, its target, and its name as
/// the harness gives it.
#[derive(Clone, Debug)]
pub struct Found {
    pub test: Test,
    pub manifest: String,
    pub package: String,
    pub target: Target,
    pub full: String,
}

/// The Cargo.toml files of the tree at `root`, in the order of their paths.
fn manifests(root: &Path) -> Vec<String> {
    let mut manifests: Vec<String> = crate::files::all(root)
        .into_iter()
        .filter(|file| file == "Cargo.toml" || file.ends_with("/Cargo.toml"))
        .filter(|file| !file.split('/').any(|part| NOT_SEARCHED.contains(&part)))
        .collect();
    manifests.sort();
    manifests
}

/// The package the file `file`, a path in the tree at `root`, is part of, by its manifest, and the
/// binary it is part of: a binary whose modules reach it, or None where only the library's do, for
/// Cargo to run the package's own binary. None where no package's targets reach the file.
pub fn run_target_of(root: &Path, file: &str) -> Option<(String, Option<String>)> {
    for manifest in manifests(root) {
        let Ok(text) = std::fs::read_to_string(root.join(&manifest)) else { continue };
        let folder = manifest.strip_suffix("Cargo.toml").unwrap_or_default().trim_end_matches('/').to_string();
        let mut in_library = false;
        for (target, first) in targets(&root.join(&folder), &text) {
            let first = if folder.is_empty() { first } else { format!("{folder}/{first}") };
            let mut seen = HashSet::new();
            walk(root, &first, &[], true, &mut seen, &mut |_, _, _, _, _| {});
            if !seen.contains(file) {
                continue;
            }
            match target {
                Target::Bin(name) => return Some((manifest, Some(name))),
                Target::Lib => in_library = true,
                Target::Test(_) => {}
            }
        }
        if in_library {
            return Some((manifest, None));
        }
    }
    None
}

/// The Rust tests of the tree at `root`, package by package in the order of their manifests' paths.
pub fn found(root: &Path) -> Vec<Found> {
    let mut all = Vec::new();
    for manifest in manifests(root) {
        let Ok(text) = std::fs::read_to_string(root.join(&manifest)) else { continue };
        let folder = manifest.strip_suffix("Cargo.toml").unwrap_or_default().trim_end_matches('/').to_string();
        let Some(package) = package_name(&text) else { continue };
        let mut seen = HashSet::new();
        for (target, first) in targets(&root.join(&folder), &text) {
            let file = if folder.is_empty() { first } else { format!("{folder}/{first}") };
            walk(root, &file, &[], true, &mut seen, &mut |own, inline, name, line, file| {
                // The id holds the file and the modules written in it; the harness's name holds the
                // file's own module path too.
                let inside: Vec<&str> = inline.iter().map(String::as_str).chain([name]).collect();
                let full: Vec<&str> = own.iter().map(String::as_str).chain(inside.iter().copied()).collect();
                let class = (!inline.is_empty()).then(|| inline.join("::"));
                let test = Test { id: format!("{file}::{}", inside.join("::")), file: file.to_string(), class, name: name.to_string(), line };
                all.push(Found { test, manifest: manifest.clone(), package: package.clone(), target: target.clone(), full: full.join("::") });
            });
        }
    }
    all
}

/// The Rust tests of the tree at `root`, as the window lists them.
pub fn tests(root: &Path) -> Vec<Test> {
    found(root).into_iter().map(|one| one.test).collect()
}

/// Whether `id`, a test's name or a file's path, is of Rust.
pub fn is_rust(id: &str) -> bool {
    id.split("::").next().is_some_and(|file| file.ends_with(".rs"))
}

/// A TOML value as text: a string's contents in either quotes, or the bare word before a comment.
fn toml_value(value: &str) -> String {
    let value = value.trim();
    for quote in ['"', '\''] {
        if let Some(rest) = value.strip_prefix(quote) {
            return rest.split(quote).next().unwrap_or_default().to_string();
        }
    }
    value.split('#').next().unwrap_or_default().trim().to_string()
}

/// The name of the package a Cargo.toml describes, where it describes one.
fn package_name(text: &str) -> Option<String> {
    let mut table = "";
    for line in text.lines().map(str::trim) {
        if line.starts_with('[') {
            table = line;
            continue;
        }
        if table == "[package]" {
            if let Some((key, value)) = line.split_once('=') {
                if key.trim() == "name" {
                    return Some(toml_value(value));
                }
            }
        }
    }
    None
}

/// The targets of the package a Cargo.toml describes, each with its first file, as a path from the
/// package's folder: the library, the binaries and the tests, as Cargo finds them. A manifest with
/// no [package] makes none.
fn targets(folder: &Path, text: &str) -> Vec<(Target, String)> {
    let mut table = String::new();
    let mut package = None;
    let (mut autobins, mut autotests) = (true, true);
    let mut lib_path = None;
    let mut listed: Vec<(String, Option<String>, Option<String>)> = Vec::new();
    for line in text.lines().map(str::trim) {
        if let Some(inner) = line.strip_prefix("[[").and_then(|rest| rest.split("]]").next()) {
            table = inner.trim().to_string();
            if table == "bin" || table == "test" {
                listed.push((table.clone(), None, None));
            }
            continue;
        }
        if let Some(inner) = line.strip_prefix('[').and_then(|rest| rest.split(']').next()) {
            table = inner.trim().to_string();
            continue;
        }
        let Some((key, value)) = line.split_once('=') else { continue };
        let value = toml_value(value);
        match (table.as_str(), key.trim()) {
            ("package", "name") => package = Some(value),
            ("package", "autobins") => autobins = value != "false",
            ("package", "autotests") => autotests = value != "false",
            ("lib", "path") => lib_path = Some(value),
            ("bin" | "test", "name") => listed.last_mut().into_iter().for_each(|one| one.1 = Some(value.clone())),
            ("bin" | "test", "path") => listed.last_mut().into_iter().for_each(|one| one.2 = Some(value.clone())),
            _ => {}
        }
    }
    let Some(package) = package else { return Vec::new() };
    let clean = |path: String| path.replace('\\', "/").trim_start_matches("./").to_string();
    let mut found = Vec::new();
    let lib = lib_path.map(clean).unwrap_or_else(|| "src/lib.rs".into());
    if folder.join(&lib).is_file() {
        found.push((Target::Lib, lib));
    }
    let mut taken = HashSet::new();
    for (kind, name, path) in listed {
        let Some(name) = name else { continue };
        let path = clean(path.unwrap_or_else(|| match kind.as_str() {
            "bin" if name == package => "src/main.rs".into(),
            "bin" => format!("src/bin/{name}.rs"),
            _ => format!("tests/{name}.rs"),
        }));
        taken.insert(path.clone());
        found.push((if kind == "bin" { Target::Bin(name) } else { Target::Test(name) }, path));
    }
    // The files Cargo finds on its own: a folder of its own with a main.rs, or a file by its stem.
    let auto = |dir: &str| -> Vec<(String, String)> {
        let mut found: Vec<(String, String)> = std::fs::read_dir(folder.join(dir))
            .into_iter()
            .flatten()
            .flatten()
            .filter_map(|entry| {
                let name = entry.file_name().to_string_lossy().to_string();
                if entry.path().is_file() {
                    name.strip_suffix(".rs").map(|stem| (stem.to_string(), format!("{dir}/{name}")))
                } else {
                    entry.path().join("main.rs").is_file().then(|| (name.clone(), format!("{dir}/{name}/main.rs")))
                }
            })
            .collect();
        found.sort();
        found
    };
    if autobins {
        if folder.join("src/main.rs").is_file() && !taken.contains("src/main.rs") {
            found.push((Target::Bin(package.clone()), "src/main.rs".into()));
        }
        for (name, path) in auto("src/bin") {
            if !taken.contains(&path) {
                found.push((Target::Bin(name), path));
            }
        }
    }
    if autotests {
        for (name, path) in auto("tests") {
            if !taken.contains(&path) {
                found.push((Target::Test(name), path));
            }
        }
    }
    found
}

/// Follows the modules of `file`, a path in the tree, whose own module path is `path`: each test it
/// holds is given to `test` with the file's module path, the modules written in the file it stands
/// in, its name, the line its name stands on counted from 0, and its file, and each module it names with `mod name;` is followed in turn. A file
/// that is a target's first, or a mod.rs, holds its modules' files beside it; any other holds them in
/// the folder of its own name. A file already followed for the package is not followed again.
fn walk(root: &Path, file: &str, path: &[String], owns_folder: bool, seen: &mut HashSet<String>, test: &mut dyn FnMut(&[String], &[String], &str, u32, &str)) {
    if !seen.insert(file.to_string()) {
        return;
    }
    let Ok(text) = std::fs::read_to_string(root.join(file)) else { return };
    let read = scan(&text);
    for (inline, name, line) in &read.tests {
        test(path, inline, name, *line, file);
    }
    let (folder, stem) = match file.rsplit_once('/') {
        Some((folder, name)) => (folder.to_string(), name.trim_end_matches(".rs").to_string()),
        None => (String::new(), file.trim_end_matches(".rs").to_string()),
    };
    let base = if owns_folder { folder } else if folder.is_empty() { stem } else { format!("{folder}/{stem}") };
    for (inline, name) in &read.mods {
        let mut dir = base.clone();
        for one in inline {
            dir = if dir.is_empty() { one.clone() } else { format!("{dir}/{one}") };
        }
        let at = |tail: &str| if dir.is_empty() { tail.to_string() } else { format!("{dir}/{tail}") };
        let mut whole = path.to_vec();
        whole.extend(inline.iter().cloned());
        whole.push(name.clone());
        let flat = at(&format!("{name}.rs"));
        let nested = at(&format!("{name}/mod.rs"));
        if root.join(&flat).is_file() {
            walk(root, &flat, &whole, false, seen, test);
        } else if root.join(&nested).is_file() {
            walk(root, &nested, &whole, true, seen, test);
        }
    }
}

/// A token of Rust's text as the search for tests reads it: a word, `::`, or a sign.
#[derive(Debug, PartialEq)]
enum Token {
    Word(String),
    Path,
    Mark(char),
}

/// The tokens of `text`, each with the line it stands on, counted from 0: comments, strings and
/// characters are passed over, and a lifetime is no token.
fn tokens(text: &str) -> Vec<(Token, u32)> {
    let chars: Vec<char> = text.chars().collect();
    let mut found = Vec::new();
    let mut line = 0u32;
    let mut at = 0;
    let word = |c: char| c.is_alphanumeric() || c == '_';
    while at < chars.len() {
        let c = chars[at];
        let next = chars.get(at + 1).copied();
        if c == '\n' {
            line += 1;
            at += 1;
        } else if c.is_whitespace() {
            at += 1;
        } else if c == '/' && next == Some('/') {
            while at < chars.len() && chars[at] != '\n' {
                at += 1;
            }
        } else if c == '/' && next == Some('*') {
            let mut depth = 0;
            while at < chars.len() {
                if chars[at] == '/' && chars.get(at + 1) == Some(&'*') {
                    depth += 1;
                    at += 2;
                } else if chars[at] == '*' && chars.get(at + 1) == Some(&'/') {
                    depth -= 1;
                    at += 2;
                    if depth == 0 {
                        break;
                    }
                } else {
                    if chars[at] == '\n' {
                        line += 1;
                    }
                    at += 1;
                }
            }
        } else if c == '"' {
            at += 1;
            while at < chars.len() && chars[at] != '"' {
                if chars[at] == '\\' {
                    at += 1;
                }
                if chars.get(at) == Some(&'\n') {
                    line += 1;
                }
                at += 1;
            }
            at += 1;
        } else if c == '\'' {
            // A character, 'a' or '\n', or a lifetime, 'a, which is passed over as no token.
            if next == Some('\\') {
                at += 2;
                while at < chars.len() && chars[at] != '\'' {
                    at += 1;
                }
                at += 1;
            } else if chars.get(at + 2) == Some(&'\'') {
                at += 3;
            } else {
                at += 1;
                while at < chars.len() && word(chars[at]) {
                    at += 1;
                }
            }
        } else if word(c) {
            let start = at;
            while at < chars.len() && word(chars[at]) {
                at += 1;
            }
            let text: String = chars[start..at].iter().collect();
            // A raw string or a byte string, r"..", r#".."#, b"..", br#".."#.
            let raw = text == "r" || text == "br";
            if (raw && matches!(chars.get(at), Some('"') | Some('#'))) || (text == "b" && chars.get(at) == Some(&'"')) {
                if text == "b" {
                    continue;
                }
                let mut hashes = 0;
                while chars.get(at) == Some(&'#') {
                    hashes += 1;
                    at += 1;
                }
                if chars.get(at) != Some(&'"') {
                    // r#name, a raw word.
                    let start = at;
                    while at < chars.len() && word(chars[at]) {
                        at += 1;
                    }
                    found.push((Token::Word(chars[start..at].iter().collect()), line));
                    continue;
                }
                at += 1;
                loop {
                    if at >= chars.len() {
                        break;
                    }
                    if chars[at] == '"' && (0..hashes).all(|one| chars.get(at + 1 + one) == Some(&'#')) {
                        at += 1 + hashes;
                        break;
                    }
                    if chars[at] == '\n' {
                        line += 1;
                    }
                    at += 1;
                }
                continue;
            }
            found.push((Token::Word(text), line));
        } else if c == ':' && next == Some(':') {
            found.push((Token::Path, line));
            at += 2;
        } else {
            found.push((Token::Mark(c), line));
            at += 1;
        }
    }
    found
}

/// What one file holds for the search for tests: each test, with the modules written in the file it
/// stands in, its name and its line; and each module the file names with `mod name;`, with the
/// modules written in the file it is named in.
#[derive(Debug, Default, PartialEq)]
struct Scanned {
    tests: Vec<(Vec<String>, String, u32)>,
    mods: Vec<(Vec<String>, String)>,
}

fn scan(text: &str) -> Scanned {
    let all = tokens(text);
    let mut read = Scanned::default();
    let mut depth = 0usize;
    let mut inline: Vec<(String, usize)> = Vec::new();
    let mut marked = false;
    let names = |inline: &[(String, usize)]| inline.iter().map(|(name, _)| name.clone()).collect::<Vec<String>>();
    let mut at = 0;
    while at < all.len() {
        match &all[at].0 {
            Token::Mark('#') => {
                let inner = matches!(all.get(at + 1), Some((Token::Mark('!'), _)));
                let open = if inner { at + 2 } else { at + 1 };
                if !matches!(all.get(open), Some((Token::Mark('['), _))) {
                    at += 1;
                    continue;
                }
                // The attribute's path, up to its first sign that is not `::`.
                let mut last = String::new();
                let mut step = open + 1;
                while let Some((token, _)) = all.get(step) {
                    match token {
                        Token::Word(word) => last = word.clone(),
                        Token::Path => {}
                        _ => break,
                    }
                    step += 1;
                }
                if !inner && last == "test" {
                    marked = true;
                }
                let mut brackets = 0;
                at = open;
                while at < all.len() {
                    match all[at].0 {
                        Token::Mark('[') => brackets += 1,
                        Token::Mark(']') => {
                            brackets -= 1;
                            if brackets == 0 {
                                break;
                            }
                        }
                        _ => {}
                    }
                    at += 1;
                }
                at += 1;
                continue;
            }
            Token::Word(word) if word == "mod" => {
                if let Some((Token::Word(name), _)) = all.get(at + 1) {
                    match all.get(at + 2).map(|(token, _)| token) {
                        Some(Token::Mark(';')) => {
                            read.mods.push((names(&inline), name.clone()));
                            at += 3;
                        }
                        Some(Token::Mark('{')) => {
                            inline.push((name.clone(), depth));
                            depth += 1;
                            at += 3;
                        }
                        _ => at += 2,
                    }
                    marked = false;
                    continue;
                }
            }
            Token::Word(word) if word == "fn" => {
                if let Some((Token::Word(name), line)) = all.get(at + 1) {
                    if marked {
                        read.tests.push((names(&inline), name.clone(), *line));
                    }
                    marked = false;
                    at += 2;
                    continue;
                }
            }
            Token::Mark('{') => {
                depth += 1;
                marked = false;
            }
            Token::Mark('}') => {
                depth = depth.saturating_sub(1);
                if inline.last().is_some_and(|(_, opened)| *opened == depth) {
                    inline.pop();
                }
            }
            Token::Mark(';') => marked = false,
            _ => {}
        }
        at += 1;
    }
    read
}

/// The folder whose paths a package's panics and errors are written from: the workspace's, where the
/// package is one of a workspace in the tree, or the package's own.
fn workspace_of(root: &Path, package: &Path) -> PathBuf {
    let mut best = package.to_path_buf();
    for folder in package.ancestors() {
        if !folder.starts_with(root) {
            break;
        }
        if std::fs::read_to_string(folder.join("Cargo.toml")).is_ok_and(|text| text.lines().any(|line| line.trim() == "[workspace]")) {
            best = folder.to_path_buf();
        }
    }
    best
}

/// A line of the harness's own: the test's name and `passed`, `failed` or `skipped`.
fn result_line(line: &str) -> Option<(String, &'static str)> {
    let rest = line.strip_prefix("test ")?;
    let (name, said) = rest.split_once(" ... ")?;
    let name = name.trim().trim_end_matches(" - should panic").to_string();
    let outcome = match said.trim() {
        "ok" => "passed",
        "FAILED" => "failed",
        other if other.starts_with("ignored") => "skipped",
        _ => return None,
    };
    Some((name, outcome))
}

/// Each failed test's message and the place it panicked, a path as the compiler wrote it and a line
/// counted from 1, from the harness's report of its failures.
fn failures_of(out: &str) -> HashMap<String, (String, Option<(String, u32)>)> {
    let mut found = HashMap::new();
    let mut lines = out.lines().peekable();
    while let Some(line) = lines.next() {
        let Some(name) = line.strip_prefix("---- ").and_then(|rest| rest.strip_suffix(" stdout ----")) else { continue };
        let mut message = String::new();
        let mut place = None;
        while let Some(next) = lines.peek() {
            if next.starts_with("---- ") || *next == "failures:" {
                break;
            }
            let next = lines.next().unwrap_or_default();
            let Some((_, after)) = next.split_once("panicked at ") else { continue };
            if let Some(quoted) = after.strip_prefix('\'') {
                // panicked at 'message', src/lib.rs:4:5
                if let Some((said, at)) = quoted.rsplit_once("', ") {
                    message = said.to_string();
                    place = place_of(at);
                }
            } else {
                // panicked at src/lib.rs:4:5:, the message on the lines after it
                place = place_of(after.trim_end_matches(':'));
                let said: Vec<&str> = std::iter::from_fn(|| lines.next_if(|line| !line.trim().is_empty() && !line.starts_with("note:") && !line.starts_with("---- "))).collect();
                message = said.join("\n");
            }
        }
        found.insert(name.to_string(), (message, place));
    }
    found
}

/// A path and a line from `path:line:column`.
fn place_of(text: &str) -> Option<(String, u32)> {
    let mut parts = text.trim().rsplitn(3, ':');
    let _column = parts.next()?;
    let line = parts.next()?.parse().ok()?;
    Some((parts.next()?.replace('\\', "/"), line))
}

/// The compiler's first error in what a build said, and the place it names, a path as the compiler
/// wrote it and a line counted from 1.
fn build_error(said: &str) -> Option<(String, Option<(String, u32)>)> {
    let mut lines = said.lines();
    let error = lines.find(|line| line.starts_with("error"))?.to_string();
    let place = lines.take(4).find_map(|line| line.trim_start().strip_prefix("--> ")).and_then(place_of);
    Some((error, place))
}

/// What the parts of a run share: its number, where its results go, its counts of passed, failed and
/// skipped, what it says at its end, whether it was stopped, and its processes.
pub struct Shared {
    pub run: u64,
    pub tell: Tell,
    pub counts: Arc<Mutex<(usize, usize, usize)>>,
    pub said: Arc<Mutex<String>>,
    pub stopped: Arc<AtomicBool>,
    pub children: Arc<Mutex<Vec<Child>>>,
}

/// The targets `given`, tests by their names or files by their paths, ask to run: each target with its
/// manifest, the tests of it given, whether all of its tests are, and whether each was asked by its
/// name.
fn grouped(root: &Path, given: &[String]) -> Vec<((String, Target), Vec<Found>, bool, bool)> {
    let all = found(root);
    let asked: HashSet<&str> = given.iter().map(String::as_str).collect();
    let mut groups: Vec<((String, Target), Vec<Found>, usize, bool)> = Vec::new();
    for one in &all {
        let key = (one.manifest.clone(), one.target.clone());
        let at = match groups.iter().position(|(own, ..)| *own == key) {
            Some(at) => at,
            None => {
                groups.push((key, Vec::new(), 0, true));
                groups.len() - 1
            }
        };
        groups[at].2 += 1;
        let by_file = asked.contains(one.test.file.as_str());
        if asked.contains(one.test.id.as_str()) || by_file {
            groups[at].1.push(one.clone());
            groups[at].3 &= !by_file;
        }
    }
    groups.into_iter().filter(|(_, tests, ..)| !tests.is_empty()).map(|(key, tests, total, named)| {
        let whole = tests.len() == total;
        (key, tests, whole, named)
    }).collect()
}

/// How many cargo tests a run of `given` starts.
pub fn targets_given(root: &Path, given: &[String]) -> usize {
    grouped(root, given).len()
}

/// Starts a cargo test for each target `given` reaches, in the tree at `root`, each on a thread of its
/// own that tells what it finds through `shared`. Says the threads, each ending as its process does.
pub fn start(root: &Path, given: &[String], shared: &Shared) -> Result<Vec<JoinHandle<Option<String>>>, String> {
    let cargo = crate::executables::program("rust", "cargo")?;
    let mut workers = Vec::new();
    for ((manifest, target), tests, whole, named) in grouped(root, given) {
        let package = root.join(manifest.strip_suffix("Cargo.toml").unwrap_or_default());
        let workspace = workspace_of(root, &package);
        let by_name: HashMap<String, Test> = tests.iter().map(|one| (one.full.clone(), one.test.clone())).collect();
        let names: Vec<String> = tests.iter().map(|one| one.full.clone()).collect();
        let mut command = Command::new(&cargo);
        command.args(["test", "--package", &tests[0].package]).args(target.words()).args(["--no-fail-fast", "--color", "never", "--manifest-path"]).arg(root.join(&manifest)).arg("--");
        if !whole && names.iter().map(String::len).sum::<usize>() < NAMES_MOST {
            command.args(&names).arg("--exact");
        }
        command.args(["--show-output", "--color", "never"]);
        if named {
            command.arg("--include-ignored");
        }
        command.current_dir(&package).env("PATH", crate::toolchains::run_path()).env("CARGO_TERM_COLOR", "never").stdin(Stdio::null()).stdout(Stdio::piped()).stderr(Stdio::piped());
        crate::runner::quiet(&mut command);
        let mut child = command.spawn().map_err(|error| format!("{}: {error}", cargo.display()))?;
        let (Some(out), Some(err)) = (child.stdout.take(), child.stderr.take()) else { continue };
        if let Ok(mut children) = shared.children.lock() {
            children.push(child);
        }
        let (run, tell, counts, said, stopped) = (shared.run, shared.tell.clone(), shared.counts.clone(), shared.said.clone(), shared.stopped.clone());
        let (root, workspace) = (root.to_path_buf(), workspace.clone());
        workers.push(std::thread::spawn(move || {
            let errors = std::thread::spawn(move || {
                let mut text = String::new();
                let _ = BufReader::new(err).read_to_string(&mut text);
                text
            });
            let tree_path = |path: &str| {
                let full = if Path::new(path).is_absolute() { PathBuf::from(path) } else { workspace.join(path) };
                crate::root::relative(&root, &full)
            };
            let mut heard: HashMap<String, &'static str> = HashMap::new();
            let mut seen = String::new();
            for line in BufReader::new(out).lines().map_while(Result::ok) {
                if stopped.load(Ordering::SeqCst) {
                    break;
                }
                if let Some((name, outcome)) = result_line(&line) {
                    if let Some(test) = by_name.get(&name) {
                        if heard.insert(name.clone(), outcome).is_none() {
                            tally(&counts, outcome);
                        }
                        tell(Heard::Result { run, outcome: Outcome { id: test.id.clone(), outcome: outcome.to_string(), message: String::new(), file: None, line: None, seconds: 0.0 } });
                    }
                }
                seen.push_str(&line);
                seen.push('\n');
            }
            let errors = errors.join().unwrap_or_default();
            for (name, (message, place)) in failures_of(&seen) {
                // A test that passed shows its output too, with --show-output.
                let Some(test) = by_name.get(&name).filter(|_| heard.get(&name) == Some(&"failed")) else { continue };
                let (file, line) = place.map_or((Some(test.file.clone()), Some(test.line)), |(path, line)| (Some(tree_path(&path)), Some(line.saturating_sub(1))));
                tell(Heard::Result { run, outcome: Outcome { id: test.id.clone(), outcome: "failed".into(), message, file, line, seconds: 0.0 } });
            }
            // A target that ran no test says why: its build's first error fails each test given it.
            if heard.is_empty() && !stopped.load(Ordering::SeqCst) {
                let (message, place) = build_error(&errors).unwrap_or_else(|| (errors.lines().rev().find(|line| !line.trim().is_empty()).unwrap_or("cargo test ran no test").trim().to_string(), None));
                let (file, line) = place.map_or((None, None), |(path, line)| (Some(tree_path(&path)), Some(line.saturating_sub(1))));
                for test in by_name.values() {
                    tally(&counts, "failed");
                    tell(Heard::Result { run, outcome: Outcome { id: test.id.clone(), outcome: "failed".into(), message: message.clone(), file: file.clone(), line, seconds: 0.0 } });
                }
                if let Ok(mut said) = said.lock() {
                    if said.is_empty() {
                        *said = message;
                    }
                }
            }
            None
        }));
    }
    Ok(workers)
}

fn tally(counts: &Mutex<(usize, usize, usize)>, outcome: &str) {
    if let Ok(mut counts) = counts.lock() {
        match outcome {
            "passed" => counts.0 += 1,
            "failed" => counts.1 += 1,
            _ => counts.2 += 1,
        }
    }
}

/// The program that runs the test `id` and the name the harness gives it, built as cargo test builds
/// it, with the package's folder for it to run in: for a debugger to start it.
pub fn binary(root: &Path, id: &str) -> Result<(PathBuf, String, PathBuf), String> {
    let one = found(root).into_iter().find(|one| one.test.id == id).ok_or_else(|| format!("{id} is no Rust test of this tree"))?;
    let package = root.join(one.manifest.strip_suffix("Cargo.toml").unwrap_or_default());
    let cargo = crate::executables::program("rust", "cargo")?;
    let mut command = Command::new(&cargo);
    command.args(["test", "--no-run", "--message-format=json", "--package", &one.package]).args(one.target.words()).arg("--manifest-path").arg(root.join(&one.manifest));
    command.current_dir(&package).env("PATH", crate::toolchains::run_path()).stdin(Stdio::null()).stderr(Stdio::piped()).stdout(Stdio::piped());
    crate::runner::quiet(&mut command);
    let out = command.output().map_err(|error| format!("{}: {error}", cargo.display()))?;
    let text = String::from_utf8_lossy(&out.stdout);
    for line in text.lines() {
        let Ok(said) = serde_json::from_str::<serde_json::Value>(line) else { continue };
        if said["reason"] != "compiler-artifact" || said["profile"]["test"] != true {
            continue;
        }
        let kinds: Vec<&str> = said["target"]["kind"].as_array().into_iter().flatten().filter_map(serde_json::Value::as_str).collect();
        if let (true, Some(program)) = (one.target.is(&kinds, said["target"]["name"].as_str().unwrap_or_default()), said["executable"].as_str()) {
            return Ok((PathBuf::from(program), one.full, package));
        }
    }
    let errors = String::from_utf8_lossy(&out.stderr);
    Err(build_error(&errors).map_or_else(|| format!("cargo test built no program for {id}"), |(error, _)| format!("The test's build failed: {error}")))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn tests_are_found_past_strings_comments_and_lifetimes() {
        let text = r##"
//! #[test] fn not_in_a_comment() {}
mod outer;
/* #[test]
   fn not_in_a_block() {} */
fn helper<'a>(text: &'a str) -> char { let _ = "#[test] fn not_in_a_string() {}"; let _ = r#"fn raw() { #[test] }"#; '{' }

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn one() {}

    #[tokio::test]
    async fn two() {}

    #[test]
    #[should_panic(expected = "no {")]
    pub fn three() { let _ = '}'; }

    fn not_a_test() {}

    mod deeper {
        mod inner_file;
        #[test]
        fn four() {}
    }
}

#[test]
fn five() {}
"##;
        let read = scan(text);
        let tests: Vec<(String, u32)> = read.tests.iter().map(|(inline, name, line)| (format!("{}{name}", inline.iter().map(|one| format!("{one}::")).collect::<String>()), *line)).collect();
        assert_eq!(tests, [("tests::one".into(), 12), ("tests::two".into(), 15), ("tests::three".into(), 19), ("tests::deeper::four".into(), 26), ("five".into(), 31)]);
        assert_eq!(read.mods, [(vec![], "outer".to_string()), (vec!["tests".to_string(), "deeper".to_string()], "inner_file".to_string())]);
    }

    #[test]
    fn a_package_s_targets_and_modules_give_each_test_its_name() {
        let dir = std::env::temp_dir().join(format!("orior_cargo_tests_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        let files = [
            ("crate/Cargo.toml", "[package]\nname = \"thing\"\nedition = \"2021\"\n\n[[test]]\nname = \"named\"\npath = \"checks/named.rs\"\n"),
            ("crate/src/lib.rs", "pub mod parts;\nmod flat;\n#[test]\nfn at_top() {}\n"),
            ("crate/src/parts/mod.rs", "mod inner;\n"),
            ("crate/src/parts/inner.rs", "#[cfg(test)]\nmod checks {\n    #[test]\n    fn deep() {}\n}\n"),
            ("crate/src/flat.rs", "mod below;\n"),
            ("crate/src/flat/below.rs", "#[test]\nfn under_flat() {}\n"),
            ("crate/src/main.rs", "#[test]\nfn in_main() {}\n"),
            ("crate/tests/whole.rs", "#[test]\nfn integration() {}\n"),
            ("crate/checks/named.rs", "#[test]\nfn listed() {}\n"),
        ];
        for (name, text) in files {
            let path = dir.join(name);
            std::fs::create_dir_all(path.parent().unwrap()).unwrap();
            std::fs::write(path, text).unwrap();
        }
        let all = found(&dir);
        let seen: Vec<(String, Target, String, Option<String>)> = all.iter().map(|one| (one.test.id.clone(), one.target.clone(), one.full.clone(), one.test.class.clone())).collect();
        assert_eq!(
            seen,
            [
                ("crate/src/lib.rs::at_top".to_string(), Target::Lib, "at_top".to_string(), None),
                ("crate/src/parts/inner.rs::checks::deep".into(), Target::Lib, "parts::inner::checks::deep".into(), Some("checks".into())),
                ("crate/src/flat/below.rs::under_flat".into(), Target::Lib, "flat::below::under_flat".into(), None),
                ("crate/checks/named.rs::listed".into(), Target::Test("named".into()), "listed".into(), None),
                ("crate/src/main.rs::in_main".into(), Target::Bin("thing".into()), "in_main".into(), None),
                ("crate/tests/whole.rs::integration".into(), Target::Test("whole".into()), "integration".into(), None),
            ]
        );
        assert_eq!(all[1].test.line, 3);
        assert!(is_rust("crate/src/lib.rs::at_top") && is_rust("crate/src/lib.rs") && !is_rust("tests/test_a.py::test_one"));
        let _ = std::fs::remove_dir_all(dir);
    }

    #[test]
    fn a_file_runs_as_the_binary_its_package_makes_of_it() {
        let dir = std::env::temp_dir().join(format!("orior_cargo_run_target_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        for (name, text) in [
            ("tool/Cargo.toml", "[package]\nname = \"tool\"\nedition = \"2021\"\n"),
            ("tool/src/lib.rs", "pub mod shared;\n"),
            ("tool/src/shared.rs", "pub fn one() -> i32 { 1 }\n"),
            ("tool/src/main.rs", "mod front;\nfn main() {}\n"),
            ("tool/src/front.rs", "pub fn show() {}\n"),
            ("tool/src/bin/extra.rs", "fn main() {}\n"),
            ("tool/notes.rs", "fn not_reached() {}\n"),
        ] {
            let path = dir.join(name);
            std::fs::create_dir_all(path.parent().unwrap()).unwrap();
            std::fs::write(path, text).unwrap();
        }
        let manifest = "tool/Cargo.toml".to_string();
        assert_eq!(run_target_of(&dir, "tool/src/front.rs"), Some((manifest.clone(), Some("tool".into()))), "a module of the package's own binary");
        assert_eq!(run_target_of(&dir, "tool/src/bin/extra.rs"), Some((manifest.clone(), Some("extra".into()))));
        assert_eq!(run_target_of(&dir, "tool/src/shared.rs"), Some((manifest, None)), "the library's alone: the package's own binary runs");
        assert_eq!(run_target_of(&dir, "tool/notes.rs"), None, "a file no target reaches runs alone");
        let _ = std::fs::remove_dir_all(dir);
    }

    #[test]
    fn the_harness_s_report_gives_each_result_and_each_failure_its_place() {
        assert_eq!(result_line("test files::listing::one ... ok"), Some(("files::listing::one".to_string(), "passed")));
        assert_eq!(result_line("test two - should panic ... FAILED"), Some(("two".to_string(), "failed")));
        assert_eq!(result_line("test three ... ignored, slow"), Some(("three".to_string(), "skipped")));
        assert_eq!(result_line("test result: ok. 3 passed; 0 failed"), None);
        let out = "running 2 tests\ntest a::one ... FAILED\ntest a::two ... FAILED\n\nfailures:\n\n---- a::one stdout ----\n\nthread 'a::one' panicked at src\\a.rs:12:9:\nassertion `left == right` failed\n  left: 1\n right: 2\nnote: run with `RUST_BACKTRACE=1`\n\n---- a::two stdout ----\nthread 'a::two' panicked at 'old words', src/a.rs:20:5\n\nfailures:\n    a::one\n    a::two\n";
        let failed = failures_of(out);
        assert_eq!(failed["a::one"], ("assertion `left == right` failed\n  left: 1\n right: 2".to_string(), Some(("src/a.rs".to_string(), 12))));
        assert_eq!(failed["a::two"], ("old words".to_string(), Some(("src/a.rs".to_string(), 20))));
        let said = "   Compiling thing v0.1.0\nerror[E0308]: mismatched types\n --> src/lib.rs:3:18\n  |\n";
        assert_eq!(build_error(said), Some(("error[E0308]: mismatched types".to_string(), Some(("src/lib.rs".to_string(), 3)))));
    }

    #[test]
    #[ignore = "runs cargo where it is installed"]
    fn a_run_tells_each_result_and_each_failure_s_line() {
        let dir = dunce::canonicalize(std::env::temp_dir()).unwrap().join(format!("orior_cargo_run_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        std::fs::create_dir_all(dir.join("src")).unwrap();
        std::fs::write(dir.join("Cargo.toml"), "[package]\nname = \"ran\"\nversion = \"0.1.0\"\nedition = \"2021\"\n\n[workspace]\n").unwrap();
        std::fs::write(dir.join("src/lib.rs"), "#[cfg(test)]\nmod tests {\n    #[test]\n    fn passes() {}\n\n    #[test]\n    fn fails() {\n        assert_eq!(1, 2);\n    }\n\n    #[test]\n    #[ignore]\n    fn waits() {}\n}\n").unwrap();
        let heard = Arc::new(Mutex::new(Vec::new()));
        let kept = heard.clone();
        let shared = Shared { run: 1, tell: Arc::new(move |one| kept.lock().unwrap().push(one)), counts: Arc::default(), said: Arc::default(), stopped: Arc::default(), children: Arc::default() };
        for worker in start(&dir, &["src/lib.rs".to_string()], &shared).unwrap() {
            worker.join().unwrap();
        }
        let results: Vec<(String, String, Option<u32>)> = heard.lock().unwrap().iter().filter_map(|one| match one {
            Heard::Result { outcome, .. } => Some((outcome.id.clone(), outcome.outcome.clone(), outcome.line)),
            _ => None,
        }).collect();
        assert!(results.contains(&("src/lib.rs::tests::passes".into(), "passed".into(), None)), "{results:?}");
        assert!(results.contains(&("src/lib.rs::tests::fails".into(), "failed".into(), Some(7))), "{results:?}");
        assert!(results.contains(&("src/lib.rs::tests::waits".into(), "skipped".into(), None)), "{results:?}");
        assert_eq!(*shared.counts.lock().unwrap(), (1, 1, 1));
        heard.lock().unwrap().clear();
        for worker in start(&dir, &["src/lib.rs::tests::waits".to_string()], &shared).unwrap() {
            worker.join().unwrap();
        }
        assert!(heard.lock().unwrap().iter().any(|one| matches!(one, Heard::Result { outcome, .. } if outcome.id == "src/lib.rs::tests::waits" && outcome.outcome == "passed")), "an ignored test asked by its name runs");
        let (program, full, _) = binary(&dir, "src/lib.rs::tests::passes").unwrap();
        assert!(program.is_file() && full == "tests::passes");
        let _ = std::fs::remove_dir_all(dir);
    }
}

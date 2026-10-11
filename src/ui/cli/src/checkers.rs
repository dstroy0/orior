// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Checkers: the type checkers and linters the reader names, each a toolchain of the manifest with a
//! `checker` that says how it runs and how its findings read. A checker is found as any toolchain is,
//! and needs nothing set up past its name.
//!
//! A checker that reads a file's text on its input checks the text as the editor holds it, and one
//! that reads files checks the file as the disk holds it. One that reads a tree whole is run over the
//! tree from its top folder. Each finding is a diagnostic of the protocol's form, its source the
//! checker's name, and a fix the checker gives, as ruff does, a fix the editor offers.
//!
//! Findings read in one of four forms: `lines`, a finding a line as `path:line:col: message`, the
//! column, a severity word after it, and a code before the message or in brackets after it each
//! where the checker writes one, as flake8, mypy and pylint write them; and the JSON of ruff, ESLint
//! and ShellCheck.

use std::collections::HashMap;
use std::io::{Read, Write};
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};

use serde::{Deserialize, Serialize};
use serde_json::{json, Value};

use crate::toolchains;

/// How a checker runs: its program, the languages it checks, the words that check one file, `{file}`
/// its path, whether it reads the file's text on its input, the words that check a tree whole where it
/// does, and the form its findings take.
#[derive(Deserialize, Serialize, Clone, Debug)]
pub struct CheckerSpec {
    pub program: String,
    pub languages: Vec<String>,
    pub args: Vec<String>,
    #[serde(default)]
    pub stdin: bool,
    #[serde(default)]
    pub tree: Option<Vec<String>>,
    pub reads: String,
}

/// A checker by its toolchain's id: its name and how it runs.
#[derive(Clone, Debug)]
pub struct Checker {
    pub id: String,
    pub name: String,
    pub spec: CheckerSpec,
}

/// Every checker the manifest knows.
pub fn known() -> Vec<Checker> {
    toolchains::manifest().into_iter().filter_map(|tool| tool.checker.clone().map(|spec| Checker { id: tool.id.clone(), name: tool.name.clone(), spec })).collect()
}

/// The checker a name names, by its id or its name in any case.
pub fn named(name: &str) -> Option<Checker> {
    let name = name.trim().to_lowercase();
    known().into_iter().find(|one| one.id.to_lowercase() == name || one.name.to_lowercase() == name)
}

/// The program a checker runs, where it is found.
fn program_of(checker: &Checker) -> Result<PathBuf, String> {
    let tool = toolchains::manifest().into_iter().find(|tool| tool.id == checker.id).ok_or_else(|| format!("{} is not in the manifest", checker.name))?;
    let found = toolchains::find(&tool, &toolchains::path_folders(), &toolchains::chosen());
    found
        .folder
        .as_deref()
        .and_then(|folder| toolchains::program_in(Path::new(folder), std::slice::from_ref(&checker.spec.program)))
        .ok_or_else(|| format!("{} is not found: File, Toolchains opens its install page or takes its folder", checker.name))
}

/// Runs a checker with `args` from `cwd`, `input` on its input where given, and gives what it wrote.
fn run(checker: &Checker, args: &[String], cwd: &Path, input: Option<&str>) -> Result<String, String> {
    let program = program_of(checker)?;
    let mut command = Command::new(&program);
    command.args(args).current_dir(cwd).env("PATH", toolchains::run_path()).stdin(if input.is_some() { Stdio::piped() } else { Stdio::null() }).stdout(Stdio::piped()).stderr(Stdio::piped());
    crate::runner::quiet(&mut command);
    let mut child = command.spawn().map_err(|error| format!("{}: {error}", program.display()))?;
    let writer = input.map(|text| {
        let mut stdin = child.stdin.take();
        let text = text.to_string();
        std::thread::spawn(move || {
            if let Some(stdin) = stdin.as_mut() {
                let _ = stdin.write_all(text.as_bytes());
            }
        })
    });
    let mut out = String::new();
    if let Some(mut stdout) = child.stdout.take() {
        let _ = stdout.read_to_string(&mut out);
    }
    let mut complaint = String::new();
    if let Some(mut stderr) = child.stderr.take() {
        let _ = stderr.read_to_string(&mut complaint);
    }
    let _ = child.wait();
    if let Some(writer) = writer {
        let _ = writer.join();
    }
    if out.trim().is_empty() && !complaint.trim().is_empty() && checker.spec.reads != "lines" {
        return Err(format!("{}: {}", checker.name, complaint.trim().lines().last().unwrap_or_default()));
    }
    Ok(if checker.spec.reads == "lines" { format!("{out}\n{complaint}") } else { out })
}

/// Checks one file of the tree at `root`, as `text` where the checker reads its input and the text is
/// given, and as the disk holds it otherwise. Gives each file's findings, the file's own first.
pub fn check_file(checker: &Checker, root: &Path, path: &Path, text: Option<&str>) -> Result<Vec<(PathBuf, Vec<Value>)>, String> {
    let file = path.display().to_string();
    let args: Vec<String> = checker.spec.args.iter().map(|word| word.replace("{file}", &file)).collect();
    let input = if checker.spec.stdin { Some(text.map(str::to_string).unwrap_or_else(|| std::fs::read_to_string(path).unwrap_or_default())) } else { None };
    let out = run(checker, &args, root, input.as_deref())?;
    let mut found = read(checker, &out, root, Some(path));
    if !found.iter().any(|(one, _)| same(one, path)) {
        found.insert(0, (path.to_path_buf(), Vec::new()));
    }
    Ok(found)
}

/// Checks the tree at `root` whole, where the checker reads a tree whole.
pub fn check_tree(checker: &Checker, root: &Path) -> Result<Vec<(PathBuf, Vec<Value>)>, String> {
    let Some(args) = &checker.spec.tree else {
        return Ok(Vec::new());
    };
    let out = run(checker, args, root, None)?;
    Ok(read(checker, &out, root, None))
}

fn same(a: &Path, b: &Path) -> bool {
    let key = |path: &Path| path.display().to_string().replace('\\', "/").to_lowercase();
    key(a) == key(b)
}

/// A diagnostic of the protocol's form, its lines and columns counted from one as the checkers count
/// them.
fn diagnostic(checker: &Checker, (line, col): (u64, u64), end: Option<(u64, u64)>, severity: u8, message: &str, code: &str, fixes: Vec<Value>) -> Value {
    let start = json!({"line": line.saturating_sub(1), "character": col.saturating_sub(1)});
    let finish = match end {
        Some((line, col)) => json!({"line": line.saturating_sub(1), "character": col.saturating_sub(1)}),
        None => json!({"line": line.saturating_sub(1), "character": col}),
    };
    json!({"range": {"start": start, "end": finish}, "severity": severity, "message": message, "source": checker.name, "code": code, "data": {"orior": fixes}})
}

/// The findings a checker wrote, by file: `path` stands for a file named `-` or `<stdin>`, and a
/// relative path is read from `root`.
fn read(checker: &Checker, out: &str, root: &Path, path: Option<&Path>) -> Vec<(PathBuf, Vec<Value>)> {
    let mut by_file: Vec<(PathBuf, Vec<Value>)> = Vec::new();
    let file_of = |name: &str| -> PathBuf {
        match (name, path) {
            ("-" | "<stdin>" | "stdin", Some(path)) => path.to_path_buf(),
            _ => {
                let given = PathBuf::from(name);
                if given.is_absolute() { given } else { root.join(given) }
            }
        }
    };
    let mut add = |file: PathBuf, item: Value| match by_file.iter_mut().find(|(one, _)| same(one, &file)) {
        Some((_, items)) => items.push(item),
        None => by_file.push((file, vec![item])),
    };
    match checker.spec.reads.as_str() {
        "ruff" => {
            for one in serde_json::from_str::<Value>(out).ok().and_then(|value| value.as_array().cloned()).unwrap_or_default() {
                let at = |place: &Value| (place["row"].as_u64().unwrap_or(1), place["column"].as_u64().unwrap_or(1));
                let (line, col) = at(&one["location"]);
                let fixes: Vec<Value> = one["fix"]["edits"]
                    .as_array()
                    .map(|edits| {
                        let edits: Vec<Value> = edits
                            .iter()
                            .map(|edit| {
                                let (from, to) = (at(&edit["location"]), at(&edit["end_location"]));
                                json!({"from": {"line": from.0 - 1, "col": from.1 - 1}, "to": {"line": to.0 - 1, "col": to.1 - 1}, "text": edit["content"].as_str().unwrap_or_default()})
                            })
                            .collect();
                        vec![json!({"title": one["fix"]["message"].as_str().unwrap_or("Fix it as ruff does"), "edits": edits})]
                    })
                    .unwrap_or_default();
                let item = diagnostic(checker, (line, col), Some(at(&one["end_location"])), 2, one["message"].as_str().unwrap_or_default(), one["code"].as_str().unwrap_or_default(), fixes);
                add(file_of(one["filename"].as_str().unwrap_or("-")), item);
            }
        }
        "eslint" => {
            for file in serde_json::from_str::<Value>(out).ok().and_then(|value| value.as_array().cloned()).unwrap_or_default() {
                let name = file["filePath"].as_str().unwrap_or("-");
                for one in file["messages"].as_array().into_iter().flatten() {
                    let end = one["endLine"].as_u64().map(|line| (line, one["endColumn"].as_u64().unwrap_or(1)));
                    let severity = if one["severity"].as_u64() == Some(2) { 1 } else { 2 };
                    add(file_of(name), diagnostic(checker, (one["line"].as_u64().unwrap_or(1), one["column"].as_u64().unwrap_or(1)), end, severity, one["message"].as_str().unwrap_or_default(), one["ruleId"].as_str().unwrap_or_default(), Vec::new()));
                }
            }
        }
        "shellcheck" => {
            let value = serde_json::from_str::<Value>(out).unwrap_or(Value::Null);
            for one in value["comments"].as_array().into_iter().flatten() {
                let end = one["endLine"].as_u64().map(|line| (line, one["endColumn"].as_u64().unwrap_or(1)));
                let severity = match one["level"].as_str() {
                    Some("error") => 1,
                    Some("warning") => 2,
                    Some("info") => 3,
                    _ => 4,
                };
                let code = one["code"].as_u64().map(|code| format!("SC{code}")).unwrap_or_default();
                add(file_of(one["file"].as_str().unwrap_or("-")), diagnostic(checker, (one["line"].as_u64().unwrap_or(1), one["column"].as_u64().unwrap_or(1)), end, severity, one["message"].as_str().unwrap_or_default(), &code, Vec::new()));
            }
        }
        _ => {
            for line in out.lines() {
                if let Some((name, at_line, col, rest)) = line_parts(line) {
                    let (severity, rest) = severity_of(rest);
                    let (code, message) = code_of(rest);
                    add(file_of(name), diagnostic(checker, (at_line, col.unwrap_or(1)), None, severity, message, &code, Vec::new()));
                }
            }
        }
    }
    by_file
}

/// A line of findings as `path:line:col: rest`, the column where it is written.
fn line_parts(line: &str) -> Option<(&str, u64, Option<u64>, &str)> {
    let bytes = line.as_bytes();
    for (at, byte) in bytes.iter().enumerate() {
        if *byte != b':' || at == 0 {
            continue;
        }
        let digits = bytes[at + 1..].iter().take_while(|one| one.is_ascii_digit()).count();
        if digits == 0 || bytes.get(at + 1 + digits) != Some(&b':') {
            continue;
        }
        let number: u64 = line[at + 1..at + 1 + digits].parse().ok()?;
        let after = at + 2 + digits;
        let col_digits = bytes[after..].iter().take_while(|one| one.is_ascii_digit()).count();
        let (col, rest) = if col_digits > 0 && bytes.get(after + col_digits) == Some(&b':') { (line[after..after + col_digits].parse().ok(), &line[after + col_digits + 1..]) } else { (None, &line[after..]) };
        return Some((&line[..at], number, col, rest.trim()));
    }
    None
}

/// A severity word at the start of a message, as mypy writes one, and the rest: an error, a
/// warning, or a note; a warning where none is written.
fn severity_of(rest: &str) -> (u8, &str) {
    for (word, severity) in [("error:", 1), ("warning:", 2), ("note:", 3), ("info:", 3)] {
        if let Some(after) = rest.strip_prefix(word) {
            return (severity, after.trim());
        }
    }
    (2, rest)
}

/// A finding's code, as flake8 writes one before its message, `E501 ...`, or mypy in brackets after
/// it, `...  [arg-type]`, or pylint in brackets before it, `[C0114(missing-module-docstring), ]`;
/// and the message without it.
fn code_of(rest: &str) -> (String, &str) {
    if let Some(inner) = rest.strip_prefix('[') {
        if let Some(close) = inner.find(']') {
            let code = inner[..close].split(['(', ',']).next().unwrap_or_default().trim().to_string();
            return (code, inner[close + 1..].trim());
        }
    }
    if let Some(open) = rest.rfind("  [").filter(|_| rest.ends_with(']')) {
        return (rest[open + 3..rest.len() - 1].to_string(), rest[..open].trim());
    }
    let first = rest.split_whitespace().next().unwrap_or_default();
    if first.len() >= 2 && first.chars().next().is_some_and(|c| c.is_ascii_uppercase()) && first.chars().skip(1).all(|c| c.is_ascii_digit()) && first.len() > 1 {
        return (first.to_string(), rest[first.len()..].trim());
    }
    (String::new(), rest)
}

/// Reads each finding a checker of `reads` would write in `out`, as `read` does, for a test.
#[cfg(test)]
fn read_as(reads: &str, out: &str, root: &Path, path: Option<&Path>) -> Vec<(PathBuf, Vec<Value>)> {
    let checker = Checker { id: reads.into(), name: reads.into(), spec: CheckerSpec { program: String::new(), languages: Vec::new(), args: Vec::new(), stdin: false, tree: None, reads: reads.into() } };
    read(&checker, out, root, path)
}

/// The languages of every checker, by the checker's id.
pub fn languages() -> HashMap<String, Vec<String>> {
    known().into_iter().map(|one| (one.id, one.spec.languages)).collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    fn said(found: &[(PathBuf, Vec<Value>)]) -> Vec<String> {
        found.iter().flat_map(|(path, items)| items.iter().map(move |item| format!("{} {}:{} s{} {} {}", path.file_name().unwrap().to_string_lossy(), item["range"]["start"]["line"], item["range"]["start"]["character"], item["severity"], item["code"].as_str().unwrap(), item["message"].as_str().unwrap()))).collect()
    }

    #[test]
    fn findings_written_a_line_each_read_as_flake8_mypy_and_pylint_write_them() {
        let root = Path::new("D:/tree");
        let out = "a.py:3:5: E501 line too long (90 > 79 characters)\nD:\\tree\\b.py:7:1: error: Incompatible return value type  [return-value]\nb.py:8:2: note: See the docs\nc.py:1: [C0114(missing-module-docstring), ] Missing module docstring\nnot a finding\n";
        assert_eq!(
            said(&read_as("lines", out, root, None)),
            vec![
                "a.py 2:4 s2 E501 line too long (90 > 79 characters)",
                "b.py 6:0 s1 return-value Incompatible return value type",
                "b.py 7:1 s3  See the docs",
                "c.py 0:0 s2 C0114 Missing module docstring",
            ]
        );
    }

    #[test]
    fn ruff_s_findings_read_with_their_fixes() {
        let out = r#"[{"filename":"-","location":{"row":1,"column":8},"end_location":{"row":1,"column":10},"code":"F401","message":"`os` imported but unused","fix":{"message":"Remove unused import: `os`","edits":[{"content":"","location":{"row":1,"column":1},"end_location":{"row":2,"column":1}}]}}]"#;
        let found = read_as("ruff", out, Path::new("D:/tree"), Some(Path::new("D:/tree/a.py")));
        assert_eq!(said(&found), vec!["a.py 0:7 s2 F401 `os` imported but unused"]);
        let fix = &found[0].1[0]["data"]["orior"][0];
        assert_eq!(fix["title"], "Remove unused import: `os`");
        assert_eq!(fix["edits"][0], json!({"from": {"line": 0, "col": 0}, "to": {"line": 1, "col": 0}, "text": ""}));
    }

    #[test]
    fn eslint_s_and_shellcheck_s_findings_read() {
        let eslint = r#"[{"filePath":"D:/tree/a.js","messages":[{"ruleId":"no-unused-vars","severity":2,"message":"'x' is assigned a value but never used.","line":2,"column":7,"endLine":2,"endColumn":8}]}]"#;
        assert_eq!(said(&read_as("eslint", eslint, Path::new("D:/tree"), None)), vec!["a.js 1:6 s1 no-unused-vars 'x' is assigned a value but never used."]);
        let shellcheck = r#"{"comments":[{"file":"-","line":3,"endLine":3,"column":6,"endColumn":10,"level":"warning","code":2086,"message":"Double quote to prevent globbing and word splitting."}]}"#;
        assert_eq!(said(&read_as("shellcheck", shellcheck, Path::new("D:/tree"), Some(Path::new("D:/tree/run.sh")))), vec!["run.sh 2:5 s2 SC2086 Double quote to prevent globbing and word splitting."]);
    }

    #[test]
    #[ignore = "runs ruff where it is installed"]
    fn ruff_checks_a_file_as_the_editor_holds_it() {
        let ruff = named("ruff").expect("ruff is in the manifest");
        let dir = std::env::temp_dir().join(format!("orior-checkers-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let path = dir.join("a.py");
        std::fs::write(&path, "x = 1\n").unwrap();
        let found = check_file(&ruff, &dir, &path, Some("import os\nx = 1\n")).unwrap();
        assert!(said(&found).contains(&"a.py 0:7 s2 F401 `os` imported but unused".to_string()), "{:?}", said(&found));
        let _ = std::fs::remove_dir_all(&dir);
    }
}

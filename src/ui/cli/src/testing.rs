// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! A tree's Python tests, found as pytest and unittest find them, with no setup: each file named
//! `test_*.py`, `*_test.py` or `test*.py`, and in it each function whose name starts with `test`,
//! each such method of a class whose name starts with `Test` and that sets no `__init__`, and each
//! such method of a class that inherits a `TestCase`. Where the tree gives pytest its own rules, in
//! `pytest.ini`, `pyproject.toml`'s `[tool.pytest.ini_options]`, `tox.ini` or `setup.cfg`, its
//! `testpaths`, `python_files`, `python_classes` and `python_functions` are read in place of these.
//! Each test is named as pytest names it, `path::Class::name`.

use std::path::Path;

use serde::Serialize;

use crate::inspect;

/// A test: its name as pytest gives it, its file in the tree, its class where it has one, its own
/// name, and the line its name stands on, counted from 0.
#[derive(Serialize, Clone, Debug, PartialEq)]
pub struct Test {
    pub id: String,
    pub file: String,
    pub class: Option<String>,
    pub name: String,
    pub line: u32,
}

/// The rules a tree gives pytest, or pytest's own where it gives none.
#[derive(Debug, PartialEq)]
struct Rules {
    paths: Vec<String>,
    files: Vec<String>,
    classes: Vec<String>,
    functions: Vec<String>,
}

impl Default for Rules {
    fn default() -> Rules {
        Rules { paths: Vec::new(), files: vec!["test_*.py".into(), "*_test.py".into(), "test*.py".into()], classes: vec!["Test".into()], functions: vec!["test".into()] }
    }
}

/// The folders a search for tests does not go into: environments, what a build writes, and caches.
const NOT_SEARCHED: [&str; 10] = ["venv", ".venv", "env", "node_modules", "site-packages", "__pycache__", "build", "dist", ".tox", ".nox"];

/// A file larger than this is not read for its tests.
const LARGEST: u64 = 2 << 20;

/// The values a pytest setting gives, split at spaces and commas, a TOML list's quotes taken off.
fn values_of(raw: &str) -> Vec<String> {
    raw.trim().trim_start_matches('[').trim_end_matches(']').split(|char: char| char.is_whitespace() || char == ',').map(|one| one.trim().trim_matches(['"', '\''])).filter(|one| !one.is_empty()).map(str::to_string).collect()
}

/// The pytest rules of a configuration file's text, read from its `section`; None where it has no
/// such section.
fn rules_in(text: &str, section: &str) -> Option<Rules> {
    let mut inside = false;
    let mut found = false;
    let mut rules = Rules::default();
    let mut key: Option<String> = None;
    let mut value = String::new();
    let set = |rules: &mut Rules, key: &str, value: &str| match key {
        "testpaths" => rules.paths = values_of(value),
        "python_files" => rules.files = values_of(value),
        "python_classes" => rules.classes = values_of(value),
        "python_functions" => rules.functions = values_of(value),
        _ => {}
    };
    for line in text.lines() {
        let trimmed = line.trim();
        if trimmed.starts_with('[') && !line.starts_with([' ', '\t']) && !(key.is_some() && trimmed.starts_with("[\"")) {
            if let Some(done) = key.take() {
                set(&mut rules, &done, &value);
            }
            inside = trimmed == section;
            found |= inside;
            continue;
        }
        if !inside || trimmed.starts_with('#') || trimmed.starts_with(';') {
            continue;
        }
        if line.starts_with([' ', '\t']) && key.is_some() {
            value.push(' ');
            value.push_str(trimmed);
            continue;
        }
        if let Some(done) = key.take() {
            set(&mut rules, &done, &value);
        }
        if let Some((name, rest)) = trimmed.split_once('=') {
            key = Some(name.trim().to_string());
            value = rest.trim().to_string();
        }
    }
    if let Some(done) = key.take() {
        set(&mut rules, &done, &value);
    }
    found.then_some(rules)
}

/// The rules the tree at `root` gives pytest, from the first of its configuration files that holds
/// them, as pytest reads them.
fn rules_of(root: &Path) -> Rules {
    for (file, section) in [("pytest.ini", "[pytest]"), ("pyproject.toml", "[tool.pytest.ini_options]"), ("tox.ini", "[pytest]"), ("setup.cfg", "[tool:pytest]")] {
        if let Some(rules) = std::fs::read_to_string(root.join(file)).ok().and_then(|text| rules_in(&text, section)) {
            return rules;
        }
    }
    Rules::default()
}

/// Whether `name` matches the glob `pattern`, its `*` any run of characters and its `?` any one.
fn glob(pattern: &str, name: &str) -> bool {
    let (pattern, name): (Vec<char>, Vec<char>) = (pattern.chars().collect(), name.chars().collect());
    let (mut p, mut n, mut star, mut mark) = (0, 0, None, 0);
    while n < name.len() {
        if p < pattern.len() && (pattern[p] == '?' || pattern[p] == name[n]) {
            p += 1;
            n += 1;
        } else if p < pattern.len() && pattern[p] == '*' {
            star = Some(p);
            mark = n;
            p += 1;
        } else if let Some(at) = star {
            p = at + 1;
            mark += 1;
            n = mark;
        } else {
            return false;
        }
    }
    pattern[p..].iter().all(|char| *char == '*')
}

/// Whether a name matches one of pytest's prefixes or globs.
fn named(name: &str, rules: &[String]) -> bool {
    rules.iter().any(|rule| if rule.contains(['*', '?']) { glob(rule, name) } else { name.starts_with(rule.as_str()) })
}

/// The tests of one file, its text `text`, at `file` in the tree.
fn tests_in(file: &str, text: &str, rules: &Rules) -> Vec<Test> {
    let Some(facts) = inspect::facts("python", text) else {
        return Vec::new();
    };
    let mut found = Vec::new();
    // A class's body is the lines after its own indented deeper than it, blank lines among them.
    let lines: Vec<&str> = text.lines().collect();
    let indent = |line: &str| line.len() - line.trim_start().len();
    let bodies: Vec<(u32, u32)> = facts
        .classes
        .iter()
        .map(|class| {
            let start = class.from.line as usize;
            let own = lines.get(start).map_or(0, |line| indent(line));
            let end = (start + 1..lines.len()).take_while(|at| lines[*at].trim().is_empty() || indent(lines[*at]) > own).last().unwrap_or(start);
            (start as u32, end as u32)
        })
        .collect();
    let held_by = |line: u32| facts.classes.iter().zip(&bodies).find(|(_, (start, end))| *start < line && line <= *end).map(|(class, _)| class);
    for function in &facts.functions {
        let line = function.from.line;
        let nested = facts.functions.iter().any(|outer| outer.first < function.first && function.last <= outer.last && !std::ptr::eq(outer, function));
        match held_by(line) {
            Some(class) => {
                let case = class.bases.iter().any(|base| base.ends_with("TestCase"));
                let method_of_class = !facts.functions.iter().any(|outer| outer.first > class.from.line && outer.first < function.first && function.last <= outer.last);
                let inits = facts.functions.iter().any(|one| one.name == "__init__" && held_by(one.from.line).is_some_and(|other| other.name == class.name));
                let collected = case || (named(&class.name, &rules.classes) && !inits);
                let test_name = if case { function.name.starts_with("test") } else { named(&function.name, &rules.functions) };
                if collected && method_of_class && test_name {
                    found.push(Test { id: format!("{file}::{}::{}", class.name, function.name), file: file.to_string(), class: Some(class.name.clone()), name: function.name.clone(), line });
                }
            }
            None if !nested && named(&function.name, &rules.functions) => {
                found.push(Test { id: format!("{file}::{}", function.name), file: file.to_string(), class: None, name: function.name.clone(), line });
            }
            None => {}
        }
    }
    found.sort_by_key(|test| test.line);
    found
}

/// The Python tests of the tree at `root`, file by file in the order of their paths.
pub fn found(root: &Path) -> Vec<Test> {
    let rules = rules_of(root);
    let mut files: Vec<String> = crate::files::all(root)
        .into_iter()
        .filter(|file| file.ends_with(".py"))
        .filter(|file| !file.split('/').any(|part| NOT_SEARCHED.contains(&part)))
        .filter(|file| named(file.rsplit('/').next().unwrap_or(file), &rules.files) || rules.files.iter().any(|rule| glob(rule, file.rsplit('/').next().unwrap_or(file))))
        .filter(|file| rules.paths.is_empty() || rules.paths.iter().any(|place| file == place || file.starts_with(&format!("{}/", place.trim_end_matches('/')))))
        .collect();
    files.sort();
    let mut found = Vec::new();
    for file in files {
        let path = root.join(&file);
        if std::fs::metadata(&path).is_ok_and(|meta| meta.len() <= LARGEST) {
            if let Ok(text) = std::fs::read_to_string(&path) {
                found.extend(tests_in(&file, &text, &rules));
            }
        }
    }
    found
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn tests_are_found_as_pytest_and_unittest_find_them() {
        let text = "import unittest\n\n\ndef test_one():\n    def test_inner():\n        pass\n\n\ndef helper():\n    pass\n\n\nclass TestThing:\n    def test_two(self):\n        pass\n\n    def other(self):\n        pass\n\n\nclass TestBuilt:\n    def __init__(self):\n        pass\n\n    def test_skipped(self):\n        pass\n\n\nclass Checks(unittest.TestCase):\n    def test_three(self):\n        pass\n\n    def setUp(self):\n        pass\n";
        let found = tests_in("tests/test_a.py", text, &Rules::default());
        let ids: Vec<&str> = found.iter().map(|test| test.id.as_str()).collect();
        assert_eq!(ids, ["tests/test_a.py::test_one", "tests/test_a.py::TestThing::test_two", "tests/test_a.py::Checks::test_three"]);
        assert_eq!(found[1].line, 13);
    }

    #[test]
    fn a_tree_gives_pytest_its_own_rules() {
        let toml = "[project]\nname = \"x\"\n\n[tool.pytest.ini_options]\ntestpaths = [\"checks\"]\npython_files = [\"check_*.py\"]\npython_functions = \"check\"\n";
        let rules = rules_in(toml, "[tool.pytest.ini_options]").unwrap();
        assert_eq!(rules.paths, ["checks"]);
        assert_eq!(rules.files, ["check_*.py"]);
        assert_eq!(rules.functions, ["check"]);
        let ini = "[pytest]\ntestpaths =\n    tests\n    more\npython_classes = Should*\n";
        let rules = rules_in(ini, "[pytest]").unwrap();
        assert_eq!(rules.paths, ["tests", "more"]);
        assert_eq!(rules.classes, ["Should*"]);
        assert!(rules_in("[other]\nx = 1\n", "[pytest]").is_none());
        assert!(glob("test_*.py", "test_a.py") && glob("*_test.py", "a_test.py") && !glob("test_*.py", "a_test.py"));
    }

    #[test]
    fn a_tree_is_searched_for_its_tests() {
        let dir = std::env::temp_dir().join(format!("orior-testing-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        for (name, text) in [("tests/test_a.py", "def test_a():\n    pass\n"), ("pkg/b_test.py", "def test_b():\n    pass\n"), ("pkg/main.py", "def test_not():\n    pass\n"), (".venv/lib/test_x.py", "def test_x():\n    pass\n")] {
            let path = dir.join(name);
            std::fs::create_dir_all(path.parent().unwrap()).unwrap();
            std::fs::write(path, text).unwrap();
        }
        let ids: Vec<String> = found(&dir).into_iter().map(|test| test.id).collect();
        assert_eq!(ids, ["pkg/b_test.py::test_b", "tests/test_a.py::test_a"]);
        std::fs::write(dir.join("pytest.ini"), "[pytest]\ntestpaths = tests\n").unwrap();
        let ids: Vec<String> = found(&dir).into_iter().map(|test| test.id).collect();
        assert_eq!(ids, ["tests/test_a.py::test_a"]);
        let _ = std::fs::remove_dir_all(dir);
    }
}

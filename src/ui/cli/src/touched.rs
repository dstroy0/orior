// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The tests a change reaches: the change the lines the tree's files hold apart from its last commit,
//! a file git does not yet track changed whole.
//!
//! A Python test reaches a change where its function calls, through any number of calls, a function
//! that holds a changed line, as the tree's index of functions finds calls (inspect.rs). A changed
//! line outside every function of its file changes every function of the file. A Rust test reaches a
//! change where the modules of its target reach a changed file (cargo_tests.rs).

use std::collections::{HashSet, VecDeque};
use std::path::Path;
use std::process::{Command, Stdio};

use crate::inspect;

/// The largest file read into the index, as the inspections read it.
const READ_MOST: u64 = 2 << 20;

/// Each file of the tree at `root` that the change holds, with the lines it changed, each a range
/// counted from 0 and inclusive; a file git does not track is changed whole, as one range of every
/// line. None where git cannot read the tree.
pub fn changed(root: &Path) -> Option<Vec<(String, Vec<(u32, u32)>)>> {
    let git = |words: &[&str]| {
        let mut command = Command::new("git");
        command.args(crate::trust::git_words(root)).args(words).current_dir(root).stdin(Stdio::null()).stderr(Stdio::null());
        crate::runner::quiet(&mut command);
        command.output().ok().filter(|out| out.status.success()).map(|out| String::from_utf8_lossy(&out.stdout).into_owned())
    };
    let diff = git(&["diff", "-U0", "--no-color", "--no-ext-diff", "--relative", "HEAD", "--"])?;
    let mut found: Vec<(String, Vec<(u32, u32)>)> = Vec::new();
    for line in diff.lines() {
        if let Some(path) = line.strip_prefix("+++ ") {
            if let Some(path) = path.strip_prefix("b/") {
                found.push((path.to_string(), Vec::new()));
            }
            continue;
        }
        // @@ -a,b +c,d @@: the lines c to c+d-1 are the change's, and a change of none, a removal,
        // touches the line it was taken from.
        let Some(hunk) = line.strip_prefix("@@ ").and_then(|rest| rest.split(" @@").next()) else { continue };
        let Some(added) = hunk.split(' ').find_map(|part| part.strip_prefix('+')) else { continue };
        let (start, count) = match added.split_once(',') {
            Some((start, count)) => (start.parse::<u32>().unwrap_or(1), count.parse::<u32>().unwrap_or(1)),
            None => (added.parse::<u32>().unwrap_or(1), 1),
        };
        let first = start.saturating_sub(1);
        let last = if count == 0 { first } else { first + count - 1 };
        if let Some((_, ranges)) = found.last_mut() {
            ranges.push((first, last));
        }
    }
    for path in git(&["ls-files", "--others", "--exclude-standard"]).unwrap_or_default().lines().filter(|line| !line.is_empty()) {
        found.push((path.to_string(), vec![(0, u32::MAX)]));
    }
    Some(found)
}

/// The tests of the tree at `root` that the change reaches, by the names Run Tests takes, Python's
/// then Rust's.
pub fn tests(root: &Path) -> Vec<String> {
    let Some(changed) = changed(root) else { return Vec::new() };
    // The tree's files as they stand: a file made a moment ago is in the change and its tests too.
    crate::files::forget_seen();
    let mut index = inspect::Tree::default();
    for one in crate::files::all(root) {
        let path = root.join(&one);
        if let (Some(language), true) = (inspect::language_of(&one), std::fs::metadata(&path).is_ok_and(|meta| meta.len() <= READ_MOST)) {
            if let Ok(text) = std::fs::read_to_string(&path) {
                index.set(&one, language, &text);
            }
        }
    }
    // The functions holding a changed line, each file's every function where a line changed outside
    // them all; then every function that calls one of those, through any number of calls.
    let mut reached: HashSet<(String, usize)> = HashSet::new();
    let mut queue: VecDeque<(String, usize)> = VecDeque::new();
    for (path, ranges) in &changed {
        if !index.holds(path) {
            continue;
        }
        let functions: Vec<(usize, u32, u32)> = (0..).map_while(|at| index.function(path, at).map(|one| (at, one.first, one.last))).collect();
        let mut held = HashSet::new();
        let mut outside = false;
        for (first, last) in ranges {
            let inside: Vec<usize> = functions.iter().filter(|(_, from, to)| from <= last && to >= first).map(|(at, ..)| *at).collect();
            outside |= inside.is_empty();
            held.extend(inside);
        }
        if outside {
            held.extend(functions.iter().map(|(at, ..)| *at));
        }
        for at in held {
            if reached.insert((path.clone(), at)) {
                queue.push_back((path.clone(), at));
            }
        }
    }
    while let Some((path, at)) = queue.pop_front() {
        for (other, index_at, _) in index.callers(&path, at) {
            if reached.insert((other.clone(), index_at)) {
                queue.push_back((other, index_at));
            }
        }
    }
    let mut out: Vec<String> = crate::testing::found(root)
        .into_iter()
        .filter(|test| !crate::cargo_tests::is_rust(&test.id))
        .filter(|test| index.function_on(&test.file, test.line).is_some_and(|at| reached.contains(&(test.file.clone(), at))))
        .map(|test| test.id)
        .collect();
    let rust: HashSet<String> = changed.iter().map(|(path, _)| path.clone()).filter(|path| path.ends_with(".rs")).collect();
    if !rust.is_empty() {
        out.extend(crate::cargo_tests::touched(root, &rust));
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn git(dir: &Path, words: &[&str]) {
        let out = Command::new("git").args(["-c", "user.name=t", "-c", "user.email=t@t", "-c", "commit.gpgsign=false"]).args(words).current_dir(dir).output().unwrap();
        assert!(out.status.success(), "{}", String::from_utf8_lossy(&out.stderr));
    }

    #[test]
    fn the_tests_that_call_a_changed_function_through_any_calls_are_the_change_s() {
        let dir = std::env::temp_dir().join(format!("orior_touched_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        std::fs::create_dir_all(dir.join("tests")).unwrap();
        let shapes = "def area(width, height):\n    return width * height\n\n\ndef square(side):\n    return area(side, side)\n\n\ndef perimeter(width, height):\n    return 2 * (width + height)\n";
        std::fs::write(dir.join("shapes.py"), shapes).unwrap();
        std::fs::write(dir.join("tests/test_shapes.py"), "from shapes import square, perimeter\n\n\ndef test_square():\n    assert square(3) == 9\n\n\ndef test_perimeter():\n    assert perimeter(1, 2) == 6\n").unwrap();
        std::fs::write(dir.join("tests/test_alone.py"), "def test_alone():\n    assert True\n").unwrap();
        git(&dir, &["init", "-q"]);
        git(&dir, &["add", "."]);
        git(&dir, &["commit", "-q", "-m", "first"]);
        assert!(tests(&dir).is_empty(), "no change, no test");
        // area changes: square calls it, and test_square calls square.
        std::fs::write(dir.join("shapes.py"), shapes.replace("return width * height", "return height * width")).unwrap();
        assert_eq!(tests(&dir), vec!["tests/test_shapes.py::test_square".to_string()]);
        // A new test file is the change's whole.
        std::fs::write(dir.join("tests/test_new.py"), "def test_new():\n    assert True\n").unwrap();
        let mut found = tests(&dir);
        found.sort();
        assert_eq!(found, vec!["tests/test_new.py::test_new".to_string(), "tests/test_shapes.py::test_square".to_string()]);
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_removal_touches_the_line_it_was_taken_from() {
        let dir = std::env::temp_dir().join(format!("orior_touched_removed_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        std::fs::create_dir_all(&dir).unwrap();
        std::fs::write(dir.join("a.py"), "def one():\n    x = 1\n    y = 2\n    return x\n").unwrap();
        git(&dir, &["init", "-q"]);
        git(&dir, &["add", "."]);
        git(&dir, &["commit", "-q", "-m", "first"]);
        std::fs::write(dir.join("a.py"), "def one():\n    x = 1\n    return x\n").unwrap();
        assert_eq!(changed(&dir), Some(vec![("a.py".to_string(), vec![(1, 1)])]));
        let _ = std::fs::remove_dir_all(&dir);
    }
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! A change reviewed by its structure: the declarations it touches, across its files, and not file
//! by file. A change is the tree's against its last commit, or a commit's against its first parent,
//! with who made the commit.
//!
//! Each declaration a change adds, takes out or changes is a piece of the review, its text as it was
//! and as it is, a declaration's text running from its line to the line before the next declaration,
//! its blank lines at the end left off. One taken out of a place and put in at another, in the same
//! file or another, by its name and its kind and with the most of its lines kept, is one piece that
//! moved. The pieces that call one another stand together, each caller before what it calls. A file
//! whose language is not read here, or whose lines outside its declarations changed, is named apart.

use std::collections::{HashMap, HashSet, VecDeque};
use std::path::Path;

use serde::Serialize;

use crate::{git, symbols};

/// The least share of a piece's lines a move keeps to count as one.
const MOVE_KEEPS: f64 = 0.6;

/// A piece of the review: a declaration's file and name and kind, how the change touched it, the file
/// it moved from where that is another, its line, its text as it was and as it is, and the pieces it
/// calls.
#[derive(Serialize, Debug, Clone, PartialEq)]
pub struct Piece {
    pub path: String,
    pub name: String,
    pub kind: String,
    pub how: &'static str,
    pub from: Option<String>,
    pub line: u32,
    pub was: String,
    pub now: String,
    pub calls: Vec<usize>,
}

/// A change reviewed: its pieces, the groups they stand in, each in the order it is read, the files
/// named apart, and who made the commit and its subject where the change is a commit's.
#[derive(Serialize, Debug, Default, PartialEq)]
pub struct Reviewed {
    pub pieces: Vec<Piece>,
    pub groups: Vec<Vec<usize>>,
    pub other: Vec<String>,
    pub by: Option<String>,
    pub subject: Option<String>,
}

/// A file a change touched: its path now, its path before, and its text before and now, none where
/// the change made or took out the file.
pub struct File {
    pub path: String,
    pub was_path: String,
    pub was: Option<String>,
    pub now: Option<String>,
}

struct Declaration {
    name: String,
    kind: String,
    line: u32,
    text: String,
}

/// The declarations of `text`, read in the language of `path`, each with its text, and the lines
/// before the first of them.
fn spans(path: &str, text: &str) -> (Vec<Declaration>, String) {
    let lines: Vec<&str> = text.lines().collect();
    let found = symbols::declarations(path, text);
    let mut out = Vec::new();
    for (at, (name, kind, line)) in found.iter().enumerate() {
        let start = *line as usize;
        let mut end = found.get(at + 1).map(|next| next.2 as usize).unwrap_or(lines.len()).min(lines.len());
        while end > start + 1 && lines[end - 1].trim().is_empty() {
            end -= 1;
        }
        out.push(Declaration { name: name.clone(), kind: kind.to_string(), line: *line, text: lines[start.min(lines.len())..end.max(start).min(lines.len())].join("\n") });
    }
    let head = lines[..found.first().map(|first| first.2 as usize).unwrap_or(lines.len()).min(lines.len())].join("\n");
    (out, head)
}

/// The share of the lines of the longer of `a` and `b` the two hold alike, each line's spaces at its
/// ends set aside.
fn kept(a: &str, b: &str) -> f64 {
    let left: Vec<&str> = a.lines().map(str::trim).collect();
    let right: Vec<&str> = b.lines().map(str::trim).collect();
    let longest = left.len().max(right.len());
    if longest == 0 {
        return 1.0;
    }
    let mut counts: HashMap<&str, i32> = HashMap::new();
    for line in &left {
        *counts.entry(line).or_default() += 1;
    }
    let mut same = 0;
    for line in &right {
        if let Some(count) = counts.get_mut(line).filter(|count| **count > 0) {
            *count -= 1;
            same += 1;
        }
    }
    same as f64 / longest as f64
}

/// Whether `text` calls `name`: the name, not part of a longer one, then an opening parenthesis.
fn calls(text: &str, name: &str) -> bool {
    if name.is_empty() {
        return false;
    }
    let bytes = text.as_bytes();
    text.match_indices(name).any(|(at, _)| {
        let before = text[..at].chars().next_back();
        let after = text[at + name.len()..].trim_start();
        !before.is_some_and(|c| c.is_alphanumeric() || c == '_') && after.starts_with('(') && at + name.len() <= bytes.len()
    })
}

/// The pieces of the files `files` and the files named apart.
pub fn compare(files: &[File]) -> (Vec<Piece>, Vec<String>) {
    let mut pieces: Vec<Piece> = Vec::new();
    let mut added: Vec<Piece> = Vec::new();
    let mut removed: Vec<Piece> = Vec::new();
    let mut other = Vec::new();
    for file in files {
        if !symbols::reads(&file.path) {
            other.push(file.path.clone());
            continue;
        }
        let (old, old_head) = spans(&file.was_path, file.was.as_deref().unwrap_or(""));
        let (new, new_head) = spans(&file.path, file.now.as_deref().unwrap_or(""));
        if old_head.trim() != new_head.trim() && file.was.is_some() && file.now.is_some() {
            other.push(file.path.clone());
        }
        let mut seen: HashMap<(String, String), usize> = HashMap::new();
        let mut olds: HashMap<(String, String, usize), &Declaration> = HashMap::new();
        for one in &old {
            let count = seen.entry((one.name.clone(), one.kind.clone())).or_default();
            olds.insert((one.name.clone(), one.kind.clone(), *count), one);
            *count += 1;
        }
        let mut seen: HashMap<(String, String), usize> = HashMap::new();
        let mut matched = HashSet::new();
        for one in &new {
            let count = seen.entry((one.name.clone(), one.kind.clone())).or_default();
            let key = (one.name.clone(), one.kind.clone(), *count);
            *count += 1;
            match olds.get(&key) {
                Some(was) => {
                    matched.insert(key);
                    if was.text != one.text {
                        pieces.push(Piece { path: file.path.clone(), name: one.name.clone(), kind: one.kind.clone(), how: "changed", from: None, line: one.line, was: was.text.clone(), now: one.text.clone(), calls: Vec::new() });
                    }
                }
                None => added.push(Piece { path: file.path.clone(), name: one.name.clone(), kind: one.kind.clone(), how: "added", from: None, line: one.line, was: String::new(), now: one.text.clone(), calls: Vec::new() }),
            }
        }
        let mut gone: Vec<(&(String, String, usize), &&Declaration)> = olds.iter().filter(|(key, _)| !matched.contains(*key)).collect();
        gone.sort_by_key(|(_, one)| one.line);
        for (_, one) in gone {
            removed.push(Piece { path: file.was_path.clone(), name: one.name.clone(), kind: one.kind.clone(), how: "removed", from: None, line: one.line, was: one.text.clone(), now: String::new(), calls: Vec::new() });
        }
    }
    let mut taken = vec![false; added.len()];
    for gone in removed {
        let found = added.iter().enumerate().filter(|(at, one)| !taken[*at] && one.name == gone.name && one.kind == gone.kind).map(|(at, one)| (at, kept(&gone.was, &one.now))).filter(|(_, share)| *share >= MOVE_KEEPS).max_by(|a, b| a.1.total_cmp(&b.1));
        match found {
            Some((at, _)) => {
                taken[at] = true;
                let to = &added[at];
                pieces.push(Piece { how: "moved", from: (gone.path != to.path).then(|| gone.path.clone()), was: gone.was, ..to.clone() });
            }
            None => pieces.push(gone),
        }
    }
    pieces.extend(added.into_iter().enumerate().filter(|(at, _)| !taken[*at]).map(|(_, one)| one));
    pieces.sort_by(|a, b| a.path.cmp(&b.path).then(a.line.cmp(&b.line)));
    let names: Vec<String> = pieces.iter().map(|one| one.name.clone()).collect();
    for at in 0..pieces.len() {
        let text = if pieces[at].now.is_empty() { pieces[at].was.clone() } else { pieces[at].now.clone() };
        let body = text.lines().skip(1).collect::<Vec<_>>().join("\n");
        pieces[at].calls = names.iter().enumerate().filter(|(other, name)| *other != at && **name != pieces[at].name && calls(&body, name)).map(|(other, _)| other).collect();
    }
    other.sort();
    other.dedup();
    (pieces, other)
}

/// The pieces in groups of those that call one another, each group in the order it is read: from
/// each piece no other in it calls, what it calls after it, depth first.
pub fn groups(pieces: &[Piece]) -> Vec<Vec<usize>> {
    let mut called: Vec<Vec<usize>> = vec![Vec::new(); pieces.len()];
    let mut near: Vec<Vec<usize>> = vec![Vec::new(); pieces.len()];
    for (at, piece) in pieces.iter().enumerate() {
        for &to in &piece.calls {
            called[to].push(at);
            near[at].push(to);
            near[to].push(at);
        }
    }
    let mut placed = vec![false; pieces.len()];
    let mut out = Vec::new();
    for start in 0..pieces.len() {
        if placed[start] {
            continue;
        }
        let mut members = Vec::new();
        let mut queue = VecDeque::from([start]);
        let mut reached = HashSet::from([start]);
        while let Some(at) = queue.pop_front() {
            members.push(at);
            for &next in &near[at] {
                if reached.insert(next) {
                    queue.push_back(next);
                }
            }
        }
        members.sort();
        let mut order = Vec::new();
        let mut visited = HashSet::new();
        fn visit(at: usize, pieces: &[Piece], visited: &mut HashSet<usize>, order: &mut Vec<usize>) {
            if !visited.insert(at) {
                return;
            }
            order.push(at);
            for &next in &pieces[at].calls {
                visit(next, pieces, visited, order);
            }
        }
        for &root in members.iter().filter(|&&at| called[at].iter().all(|caller| *caller == at)) {
            visit(root, pieces, &mut visited, &mut order);
        }
        for &at in &members {
            visit(at, pieces, &mut visited, &mut order);
        }
        order.iter().for_each(|&at| placed[at] = true);
        out.push(order);
    }
    out
}

/// The change of the tree `root` against its last commit, or of commit `commit` against its first
/// parent, reviewed by its structure.
pub fn review(root: &Path, commit: Option<&str>) -> Result<Reviewed, String> {
    let mut made = None;
    let files: Vec<File> = match commit {
        Some(id) => {
            let repos: Vec<String> = git::repositories(root).into_iter().map(|repo| repo.path).collect();
            let (repo, touched) = repos
                .iter()
                .find_map(|repo| git::touched(&root.join(repo), id).ok().filter(|found| !found.is_empty()).map(|found| (repo.clone(), found)))
                .ok_or_else(|| format!("no repository of the tree holds the commit {id}"))?;
            made = git::made_by(&root.join(&repo), id);
            let at = if repo.is_empty() { String::new() } else { format!("{repo}/") };
            touched
                .into_iter()
                .map(|touched| {
                    let path = format!("{at}{}", touched.path);
                    let was_path = format!("{at}{}", touched.was.as_deref().unwrap_or(&touched.path));
                    File {
                        was: if touched.state == 'A' { None } else { git::text_before(root, &was_path, id) },
                        now: if touched.state == 'D' { None } else { git::text_at(root, &path, id).ok() },
                        path,
                        was_path,
                    }
                })
                .collect()
        }
        None => git::changed(root, None)
            .into_iter()
            .filter(|changed| !changed.path.ends_with('/'))
            .map(|changed| File { was: git::head_text(root, &changed.path), now: std::fs::read_to_string(root.join(&changed.path)).ok(), was_path: changed.path.clone(), path: changed.path })
            .collect(),
    };
    let (pieces, other) = compare(&files);
    let groups = groups(&pieces);
    Ok(Reviewed { pieces, groups, other, by: made.as_ref().map(|one| one.0.clone()), subject: made.map(|one| one.1) })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn file(path: &str, was: Option<&str>, now: Option<&str>) -> File {
        File { path: path.into(), was_path: path.into(), was: was.map(str::to_string), now: now.map(str::to_string) }
    }

    #[test]
    fn a_change_is_the_declarations_it_touches_a_move_is_one_piece_and_callers_stand_before_what_they_call() {
        let a_was = "def main():\n    helper()\n    return 0\n\n\ndef helper():\n    return 1\n\n\ndef moving():\n    x = 1\n    y = 2\n    return x + y\n";
        let a_now = "def main():\n    helper()\n    other()\n    return 0\n\n\ndef helper():\n    return 2\n";
        let b_was = "def other():\n    return 3\n";
        let b_now = "def other():\n    return moving()\n\n\ndef moving():\n    x = 1\n    y = 2\n    return x + y\n";
        let files = vec![file("a.py", Some(a_was), Some(a_now)), file("b.py", Some(b_was), Some(b_now)), file("notes.md", Some("one"), Some("two"))];
        let (pieces, other) = compare(&files);
        let shown: Vec<(&str, &str, &str, Option<&str>)> = pieces.iter().map(|one| (one.path.as_str(), one.name.as_str(), one.how, one.from.as_deref())).collect();
        assert_eq!(shown, vec![("a.py", "main", "changed", None), ("a.py", "helper", "changed", None), ("b.py", "other", "changed", None), ("b.py", "moving", "moved", Some("a.py"))]);
        assert_eq!(other, vec!["notes.md".to_string()]);
        assert_eq!(pieces[3].was, "def moving():\n    x = 1\n    y = 2\n    return x + y", "the blank lines at a declaration's end left off");
        assert_eq!(pieces[0].calls, vec![1, 2], "main calls helper and other");
        assert_eq!(pieces[2].calls, vec![3], "other calls moving");
        assert_eq!(groups(&pieces), vec![vec![0, 1, 2, 3]], "one group, each caller before what it calls");
    }

    #[test]
    fn a_declaration_added_or_taken_out_is_one_and_pieces_that_call_none_stand_apart() {
        let (pieces, _) = compare(&[file("c.rs", Some("fn gone() {\n}\n\nfn kept() {\n}\n"), Some("fn kept() {\n}\n\nfn fresh() {\n    let a = 1;\n}\n"))]);
        assert_eq!(pieces.iter().map(|one| (one.name.as_str(), one.how)).collect::<Vec<_>>(), vec![("gone", "removed"), ("fresh", "added")]);
        assert_eq!(groups(&pieces), vec![vec![0], vec![1]]);
        let (moved, _) = compare(&[file("d.py", Some("def f():\n    a = 1\n    b = 2\n    c = 3\n"), None), file("e.py", None, Some("def f():\n    a = 1\n    b = 2\n    c = 4\n"))]);
        assert_eq!(moved.iter().map(|one| (one.path.as_str(), one.how, one.from.as_deref())).collect::<Vec<_>>(), vec![("e.py", "moved", Some("d.py"))], "two of three lines kept is a move");
    }

    #[test]
    fn a_commit_is_reviewed_against_its_parent_with_who_made_it() {
        let root = std::env::temp_dir().join(format!("orior_ui_review_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&root);
        std::fs::create_dir_all(&root).unwrap();
        let root = dunce::canonicalize(&root).unwrap();
        let run = |args: &[&str]| assert!(std::process::Command::new("git").args(args).current_dir(&root).output().unwrap().status.success(), "{args:?}");
        run(&["init", "-q"]);
        run(&["config", "user.name", "Tester"]);
        run(&["config", "user.email", "tester@example.com"]);
        std::fs::write(root.join("a.py"), "def one():\n    return 1\n").unwrap();
        run(&["add", "."]);
        run(&["commit", "-q", "-m", "first"]);
        std::fs::write(root.join("a.py"), "def one():\n    return two()\n\n\ndef two():\n    return 2\n").unwrap();
        run(&["commit", "-q", "-am", "second"]);
        let id = String::from_utf8(std::process::Command::new("git").args(["rev-parse", "HEAD"]).current_dir(&root).output().unwrap().stdout).unwrap().trim().to_string();
        let read = review(&root, Some(&id)).unwrap();
        assert_eq!((read.by.as_deref(), read.subject.as_deref()), (Some("Tester"), Some("second")));
        assert_eq!(read.pieces.iter().map(|one| (one.name.as_str(), one.how)).collect::<Vec<_>>(), vec![("one", "changed"), ("two", "added")]);
        assert_eq!(read.groups, vec![vec![0, 1]]);
        std::fs::write(root.join("a.py"), "def one():\n    return 3\n").unwrap();
        let tree = review(&root, None).unwrap();
        assert_eq!(tree.by, None);
        assert_eq!(tree.pieces.iter().map(|one| (one.name.as_str(), one.how)).collect::<Vec<_>>(), vec![("one", "changed"), ("two", "removed")]);
        let _ = std::fs::remove_dir_all(&root);
    }
}

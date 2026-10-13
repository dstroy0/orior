// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! What a run leaves held against what the tree holds, shown and asked before the writer writes
//! it in place: a text file apart as a line diff, a record file as its byte counts; `y`, `n`, `all`
//! or `none` at the terminal, and `--leave` where no one is there. A refused file is handed back as
//! the tree holds it with the writer's `file` word, so that the one writer still writes every
//! current file and the tree's copy comes out unchanged. Nothing here touches the disk.

use std::io::{BufRead, IsTerminal, Write};
use std::path::{Path, PathBuf};

use crate::run::Run;

/// Whether a run's files are left as it leaves them: every one, none, or each asked.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Leave {
    All,
    None,
    Ask,
}

impl Leave {
    pub fn from_word(word: &str) -> Option<Leave> {
        match word {
            "all" => Some(Leave::All),
            "none" => Some(Leave::None),
            "ask" => Some(Leave::Ask),
            _ => None,
        }
    }

    /// The leave given, else `Ask` where the standard input is a terminal and `All` where it is not.
    pub fn chosen(given: Option<Leave>) -> Leave {
        given.unwrap_or(if std::io::stdin().is_terminal() { Leave::Ask } else { Leave::All })
    }
}

/// What is answered of one file: this one left or not, or every one from here on.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Answer {
    Yes,
    No,
    All,
    None,
}

/// Whoever answers for a file.
pub trait Asker {
    /// Whether the file at `path` is to be left as the run leaves it, its difference already shown.
    fn asked(&mut self, path: &Path) -> Answer;
}

/// The person at the terminal.
pub struct Terminal;

impl Asker for Terminal {
    fn asked(&mut self, path: &Path) -> Answer {
        loop {
            eprint!("leave {} as the run leaves it? [y/n/all/none] ", path.display());
            let _ = std::io::stderr().flush();
            let mut line = String::new();
            if std::io::stdin().lock().read_line(&mut line).is_err() || line.is_empty() {
                return Answer::None;
            }
            match line.trim() {
                "y" | "yes" => return Answer::Yes,
                "n" | "no" => return Answer::No,
                "all" => return Answer::All,
                "none" => return Answer::None,
                _ => {}
            }
        }
    }
}

/// One line of a diff: the same in both, added by the run, or removed by it.
#[derive(Debug, PartialEq, Eq)]
pub enum Change<'a> {
    Same(&'a str),
    Added(&'a str),
    Removed(&'a str),
}

/// The lines of `before` against `after`: the shortest edit script, Myers' way, the trace of each
/// step's furthest reach kept and walked back from the end.
pub fn diff<'a>(before: &[&'a str], after: &[&'a str]) -> Vec<Change<'a>> {
    let (n, m) = (before.len() as isize, after.len() as isize);
    let most = (n + m) as usize;
    let offset = most as isize;
    let mut reach = vec![0isize; 2 * most + 2];
    let mut trace: Vec<Vec<isize>> = Vec::new();
    let at = |k: isize| (k + offset) as usize;
    'steps: for d in 0..=(most as isize) {
        trace.push(reach.clone());
        let mut k = -d;
        while k <= d {
            let mut x = if k == -d || (k != d && reach[at(k - 1)] < reach[at(k + 1)]) { reach[at(k + 1)] } else { reach[at(k - 1)] + 1 };
            let mut y = x - k;
            while x < n && y < m && before[x as usize] == after[y as usize] {
                x += 1;
                y += 1;
            }
            reach[at(k)] = x;
            if x >= n && y >= m {
                break 'steps;
            }
            k += 2;
        }
    }
    let mut changes = Vec::new();
    let (mut x, mut y) = (n, m);
    for d in (0..trace.len() as isize).rev() {
        let reach = &trace[d as usize];
        let k = x - y;
        let previous_k = if k == -d || (k != d && reach[at(k - 1)] < reach[at(k + 1)]) { k + 1 } else { k - 1 };
        let previous_x = reach[at(previous_k)];
        let previous_y = previous_x - previous_k;
        while x > previous_x && y > previous_y {
            x -= 1;
            y -= 1;
            changes.push(Change::Same(before[x as usize]));
        }
        if d > 0 {
            if x == previous_x {
                y -= 1;
                changes.push(Change::Added(after[y as usize]));
            } else {
                x -= 1;
                changes.push(Change::Removed(before[x as usize]));
            }
        }
    }
    changes.reverse();
    changes
}

/// The changes as lines, `-`, `+` and ` ` led, a run of more than six unchanged lines folded to its
/// first three, a count, and its last three.
pub fn rendered(changes: &[Change]) -> String {
    let mut out: Vec<String> = Vec::new();
    let mut same: Vec<&str> = Vec::new();
    fn flushed(same: &mut Vec<&str>, out: &mut Vec<String>) {
        if same.len() > 6 {
            out.extend(same[..3].iter().map(|l| format!(" {l}")));
            out.push(format!(" ... {} lines the same ...", same.len() - 6));
            out.extend(same[same.len() - 3..].iter().map(|l| format!(" {l}")));
        } else {
            out.extend(same.iter().map(|l| format!(" {l}")));
        }
        same.clear();
    }
    for change in changes {
        match change {
            Change::Same(line) => same.push(line),
            Change::Added(line) => {
                flushed(&mut same, &mut out);
                out.push(format!("+{line}"));
            }
            Change::Removed(line) => {
                flushed(&mut same, &mut out);
                out.push(format!("-{line}"));
            }
        }
    }
    flushed(&mut same, &mut out);
    out.join("\n")
}

/// The extensions shown as a line diff; every other file is shown by its byte counts.
const TEXT: &[&str] = &["khw", "krs", "klq", "klm", "md", "tsv", "txt", "h", "gsm"];

/// What is shown of `path` apart: its diff where it is a text file, its byte counts where it is a record.
pub fn shown(path: &Path, tree: &[u8], run: &[u8]) -> String {
    let name = path.file_name().map(|n| n.to_string_lossy().into_owned()).unwrap_or_default();
    let is_text = path.extension().and_then(|e| e.to_str()).is_some_and(|e| TEXT.contains(&e));
    match (is_text, std::str::from_utf8(tree), std::str::from_utf8(run)) {
        (true, Ok(before), Ok(after)) => {
            let before: Vec<&str> = before.lines().collect();
            let after: Vec<&str> = after.lines().collect();
            format!("{name}:\n{}", rendered(&diff(&before, &after)))
        }
        _ => format!("{name}: {} bytes in the tree, {} as the run leaves it", tree.len(), run.len()),
    }
}

/// Each file apart decided: shown through `sink`, asked of `asker` under `Ask`, and handed back as
/// the tree holds it through `handed` where refused. `files` is each path with the tree's bytes and
/// the run's.
pub fn decided(files: &[(PathBuf, Vec<u8>, Vec<u8>)], leave: Leave, asker: &mut dyn Asker, sink: &mut dyn FnMut(&str), handed: &mut dyn FnMut(&Path, &[u8]) -> Result<(), String>) -> Result<(), String> {
    let mut standing = leave;
    for (path, tree, run) in files {
        if tree == run {
            continue;
        }
        sink(&shown(path, tree, run));
        let answer = match standing {
            Leave::All => Answer::Yes,
            Leave::None => Answer::No,
            Leave::Ask => asker.asked(path),
        };
        match answer {
            Answer::All => standing = Leave::All,
            Answer::None => standing = Leave::None,
            Answer::Yes | Answer::No => {}
        }
        if matches!(answer, Answer::No | Answer::None) {
            handed(path, tree)?;
            sink(&format!("  {} is left as the tree holds it", path.display()));
        }
    }
    Ok(())
}

/// The run's leaves read as the run holds each and as the tree holds each, and decided.
pub fn settled(run: &Run, leave: Leave, asker: &mut dyn Asker, sink: &mut dyn FnMut(&str)) -> Result<(), String> {
    let mut files = Vec::new();
    for path in run.leaves() {
        let tree = std::fs::read(&path).unwrap_or_default();
        let Some(held) = run.writer().current(&path) else { continue };
        files.push((path, tree, held));
    }
    let writer = run.writer();
    decided(&files, leave, asker, sink, &mut |path, bytes| writer.file(path, bytes))
}

#[cfg(test)]
mod asking {
    use super::*;

    #[test]
    fn a_diff_names_what_changed_and_keeps_the_rest_short() {
        let before: Vec<&str> = (1..=20).map(|_| "same").collect();
        let mut after = before.clone();
        after[10] = "changed";
        let changes = diff(&before, &after);
        assert_eq!(changes.iter().filter(|c| matches!(c, Change::Removed(_))).count(), 1);
        assert_eq!(changes.iter().filter(|c| matches!(c, Change::Added(_))).count(), 1);
        let text = rendered(&changes);
        assert!(text.contains("-same") && text.contains("+changed"), "{text}");
        // each unchanged run folds to its first three, a count and its last three: 7 lines a run at most
        assert!(text.contains("... 4 lines the same ...") && text.lines().count() <= 16, "unchanged runs are folded:\n{text}");
    }

    #[test]
    fn a_diff_of_the_same_lines_is_all_the_same() {
        let lines = ["a", "b", "c"];
        assert!(diff(&lines, &lines).iter().all(|c| matches!(c, Change::Same(_))));
        assert_eq!(diff(&[], &["a"]), [Change::Added("a")]);
        assert_eq!(diff(&["a"], &[]), [Change::Removed("a")]);
    }

    #[test]
    fn a_record_file_is_shown_by_its_counts() {
        let said = shown(Path::new("a/sm_86.kqr"), b"12345", b"1234567");
        assert_eq!(said, "sm_86.kqr: 5 bytes in the tree, 7 as the run leaves it");
    }

    #[test]
    fn no_terminal_means_all() {
        assert_eq!(Leave::chosen(Some(Leave::None)), Leave::None);
        // under cargo test the standard input is no terminal
        assert_eq!(Leave::chosen(None), Leave::All);
        assert_eq!(Leave::from_word("ask"), Some(Leave::Ask));
        assert_eq!(Leave::from_word("maybe"), None);
    }

    #[test]
    fn a_refused_file_is_handed_back_as_the_tree_holds_it() {
        struct Says(Vec<Answer>);
        impl Asker for Says {
            fn asked(&mut self, _: &Path) -> Answer {
                self.0.remove(0)
            }
        }
        let files = [
            (PathBuf::from("a.khw"), b"tree".to_vec(), b"run".to_vec()),
            (PathBuf::from("b.krs"), b"tree".to_vec(), b"run".to_vec()),
            (PathBuf::from("c.krs"), b"same".to_vec(), b"same".to_vec()),
        ];
        let mut handed = Vec::new();
        let mut said = Vec::new();
        decided(&files, Leave::Ask, &mut Says(vec![Answer::No, Answer::All]), &mut |line| said.push(line.to_string()), &mut |path, bytes| {
            handed.push((path.to_path_buf(), bytes.to_vec()));
            Ok(())
        })
        .unwrap();
        assert_eq!(handed, [(PathBuf::from("a.khw"), b"tree".to_vec())]);
        assert!(said.iter().any(|l| l.starts_with("a.khw:")) && said.iter().any(|l| l.starts_with("b.krs:")), "{said:?}");
        assert!(!said.iter().any(|l| l.starts_with("c.krs")), "a file the same is passed over: {said:?}");
    }
}

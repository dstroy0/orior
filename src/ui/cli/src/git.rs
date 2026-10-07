// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! What git says of the tree: the branch it is on, the files that differ from the last commit and
//! how, the commits that touched a file, and a file's text as one of those commits left it. In a tree
//! git cannot read, each of these comes back empty.

use std::path::Path;
use std::process::{Command, Stdio};

use serde::Serialize;

use crate::root::inside;

/// The most commits a file's list holds.
const COMMITS_LIMIT: usize = 200;

/// A file that differs from the last commit. Its state is one letter: M changed, A added, D gone,
/// R renamed, U not yet tracked, C in conflict.
#[derive(Serialize)]
pub struct Changed {
    pub path: String,
    pub state: char,
}

/// One commit that touched a file: its id, when it was made in seconds since 1970, and its subject.
#[derive(Serialize)]
pub struct Commit {
    pub id: String,
    pub when: i64,
    pub subject: String,
}

fn git(root: &Path, args: &[&str]) -> Option<Vec<u8>> {
    let mut command = Command::new("git");
    command.args(args).current_dir(root);
    command.stdin(Stdio::null()).stderr(Stdio::null());
    crate::runner::quiet(&mut command);
    command.output().ok().filter(|out| out.status.success()).map(|out| out.stdout)
}

/// The state letter for a porcelain status's two columns.
fn state_of(index: u8, tree: u8) -> char {
    match (index, tree) {
        (b'?', b'?') => 'U',
        (b'U', _) | (_, b'U') | (b'A', b'A') | (b'D', b'D') => 'C',
        (b'D', _) | (_, b'D') => 'D',
        (b'R', _) => 'R',
        (b'A', _) => 'A',
        _ => 'M',
    }
}

/// The branch the tree is on, or the short id of the commit it stands at where it is on none.
pub fn branch(root: &Path) -> Option<String> {
    let named = git(root, &["rev-parse", "--abbrev-ref", "HEAD"]).map(|out| String::from_utf8_lossy(&out).trim().to_string())?;
    if named != "HEAD" {
        return Some(named);
    }
    git(root, &["rev-parse", "--short", "HEAD"]).map(|out| String::from_utf8_lossy(&out).trim().to_string())
}

/// Every file under the tree that differs from the last commit, by its path in the tree.
pub fn changed(root: &Path) -> Vec<Changed> {
    let prefix = git(root, &["rev-parse", "--show-prefix"]).map(|out| String::from_utf8_lossy(&out).trim().to_string()).unwrap_or_default();
    let Some(out) = git(root, &["status", "--porcelain=v1", "-z", "--untracked-files=all", "--", "."]) else {
        return Vec::new();
    };
    let text = String::from_utf8_lossy(&out);
    let mut records = text.split('\0').filter(|record| !record.is_empty());
    let mut found = Vec::new();
    while let Some(record) = records.next() {
        let bytes = record.as_bytes();
        if bytes.len() < 4 {
            continue;
        }
        let (index, tree) = (bytes[0], bytes[1]);
        // A rename or a copy is followed by the path it came from.
        if index == b'R' || index == b'C' {
            records.next();
        }
        if let Some(path) = record[3..].strip_prefix(prefix.as_str()) {
            found.push(Changed { path: path.to_string(), state: state_of(index, tree) });
        }
    }
    found
}

/// The commits that touched `file`, the newest first, following it through renames.
pub fn commits(root: &Path, file: &str) -> Result<Vec<Commit>, String> {
    inside(root, file)?;
    let limit = format!("-n{COMMITS_LIMIT}");
    let shown = format!("./{file}");
    let Some(out) = git(root, &["log", "--follow", &limit, "--format=%H%x1f%ct%x1f%s", "--", &shown]) else {
        return Ok(Vec::new());
    };
    Ok(String::from_utf8_lossy(&out)
        .lines()
        .filter_map(|line| {
            let mut parts = line.splitn(3, '\u{1f}');
            let id = parts.next()?.to_string();
            let when = parts.next()?.parse().ok()?;
            let subject = parts.next().unwrap_or_default().to_string();
            Some(Commit { id, when, subject })
        })
        .collect())
}

/// The text of `file` as commit `id` left it.
pub fn text_at(root: &Path, file: &str, id: &str) -> Result<String, String> {
    inside(root, file)?;
    if id.is_empty() || !id.bytes().all(|b| b.is_ascii_hexdigit()) {
        return Err(format!("{id} is not a commit"));
    }
    git(root, &["show", &format!("{id}:./{file}")])
        .map(|out| String::from_utf8_lossy(&out).into_owned())
        .ok_or_else(|| format!("{file} is not in {id}"))
}

#[cfg(test)]
mod states {
    use super::state_of;

    #[test]
    fn each_status_has_one_letter() {
        assert_eq!(state_of(b'?', b'?'), 'U');
        assert_eq!(state_of(b' ', b'M'), 'M');
        assert_eq!(state_of(b'M', b' '), 'M');
        assert_eq!(state_of(b'A', b' '), 'A');
        assert_eq!(state_of(b' ', b'D'), 'D');
        assert_eq!(state_of(b'R', b' '), 'R');
        assert_eq!(state_of(b'U', b'U'), 'C');
        assert_eq!(state_of(b'A', b'A'), 'C');
    }
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! What git says of the tree: the branch it is on, the files that differ from the last commit and
//! how, the commits that touched a file, and a file's text as one of those commits left it or as the
//! last did. In a tree git cannot read, each of these comes back empty. Here too is the clone of a
//! repository into a folder of its own, for File, Clone Repository and `orior file clone`.

use std::io::Read;
use std::path::{Path, PathBuf};
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
    Ok(git(root, &["log", "--follow", &limit, "--format=%H%x1f%ct%x1f%s", "--", &shown]).map(|out| commits_of(&out)).unwrap_or_default())
}

/// The commits of the branch the tree is on, the newest first.
pub fn log(root: &Path) -> Vec<Commit> {
    let limit = format!("-n{COMMITS_LIMIT}");
    git(root, &["log", &limit, "--format=%H%x1f%ct%x1f%s"]).map(|out| commits_of(&out)).unwrap_or_default()
}

fn commits_of(out: &[u8]) -> Vec<Commit> {
    String::from_utf8_lossy(out)
        .lines()
        .filter_map(|line| {
            let mut parts = line.splitn(3, '\u{1f}');
            let id = parts.next()?.to_string();
            let when = parts.next()?.parse().ok()?;
            let subject = parts.next().unwrap_or_default().to_string();
            Some(Commit { id, when, subject })
        })
        .collect()
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

/// A file's text as the last commit left it, or nothing where the tree is not git's or the last
/// commit does not hold the file.
pub fn head_text(root: &Path, file: &str) -> Option<String> {
    inside(root, file).ok()?;
    git(root, &["show", &format!("HEAD:./{file}")]).map(|out| String::from_utf8_lossy(&out).into_owned())
}

/// The repository File, Clone Repository offers first.
pub const ORIOR: &str = "https://github.com/dstroy0/orior.git";

/// The folder a clone goes in where none is named: the reader's home.
pub fn clone_parent() -> PathBuf {
    let home = if cfg!(windows) { "USERPROFILE" } else { "HOME" };
    std::env::var_os(home).map(PathBuf::from).unwrap_or_else(|| std::env::current_dir().unwrap_or_default())
}

/// The folder a clone of `url` makes under `parent`: the last part of the address, without `.git`,
/// as git itself names it.
pub fn clone_folder(url: &str, parent: &Path) -> Result<PathBuf, String> {
    let address = url.trim().trim_end_matches(['/', '\\']);
    let name = address.strip_suffix(".git").unwrap_or(address).rsplit(['/', '\\', ':']).next().unwrap_or("");
    if name.is_empty() || name == "." || name == ".." {
        return Err(format!("{url} names no repository"));
    }
    Ok(parent.join(name))
}

/// Clones `url` into `target`, a folder that is not there yet or is empty, with the git orior's runs
/// get. Each line of git's progress goes to `said` as it comes, with whether the line is finished: a
/// line git ends with a carriage return is one it writes over with the next. Answers the folder, or
/// the first line git marked fatal.
pub fn clone(url: &str, target: &Path, mut said: impl FnMut(&str, bool)) -> Result<PathBuf, String> {
    if target.read_dir().is_ok_and(|mut held| held.next().is_some()) {
        return Err(format!("{} is there already and holds files", target.display()));
    }
    let program = crate::toolchains::chosen_program("git").unwrap_or_else(|| PathBuf::from("git"));
    let mut command = Command::new(program);
    // git asks for no password at a terminal no one can type into; a credential helper still may.
    command.args(["clone", "--progress", "--", url]).arg(target).env("PATH", crate::toolchains::run_path()).env("GIT_TERMINAL_PROMPT", "0");
    command.stdin(Stdio::null()).stdout(Stdio::null()).stderr(Stdio::piped());
    crate::runner::quiet(&mut command);
    let mut child = command.spawn().map_err(|error| match error.kind() {
        std::io::ErrorKind::NotFound => "git is not installed, or not on the PATH: File, Toolchains, or orior file toolchains install git, opens its install page".to_string(),
        _ => format!("git did not start: {error}"),
    })?;
    let mut stderr = child.stderr.take().expect("piped");
    let mut line = Vec::new();
    let mut fatal = None;
    let mut chunk = [0u8; 4096];
    let mut tell = |line: &mut Vec<u8>, ended: bool, fatal: &mut Option<String>| {
        let text = String::from_utf8_lossy(line).trim().to_string();
        line.clear();
        if !text.is_empty() {
            said(&text, ended);
            if fatal.is_none() {
                *fatal = text.strip_prefix("fatal: ").map(String::from);
            }
        }
    };
    loop {
        let read = match stderr.read(&mut chunk) {
            Ok(0) | Err(_) => break,
            Ok(read) => read,
        };
        for &byte in &chunk[..read] {
            match byte {
                b'\n' => tell(&mut line, true, &mut fatal),
                b'\r' => tell(&mut line, false, &mut fatal),
                _ => line.push(byte),
            }
        }
    }
    tell(&mut line, true, &mut fatal);
    let status = child.wait().map_err(|error| error.to_string())?;
    if status.success() {
        Ok(target.to_path_buf())
    } else {
        Err(fatal.unwrap_or_else(|| format!("git clone {url} failed")))
    }
}

#[cfg(test)]
mod clones {
    use std::path::Path;

    use super::clone_folder;

    #[test]
    fn a_clone_is_named_as_git_names_it() {
        let parent = Path::new("work");
        assert_eq!(clone_folder("https://github.com/dstroy0/orior.git", parent).unwrap(), parent.join("orior"));
        assert_eq!(clone_folder("https://github.com/dstroy0/orior/", parent).unwrap(), parent.join("orior"));
        assert_eq!(clone_folder("git@github.com:dstroy0/orior.git", parent).unwrap(), parent.join("orior"));
        assert_eq!(clone_folder("git@host:orior", parent).unwrap(), parent.join("orior"));
        assert_eq!(clone_folder(r"D:\repos\orior.git", parent).unwrap(), parent.join("orior"));
        assert!(clone_folder("https://", parent).is_err());
        assert!(clone_folder("  ", parent).is_err());
    }
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

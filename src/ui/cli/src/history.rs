// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Local History: each file of a tree as it was saved, kept in orior's own folder apart from git. A
//! version git never saw can be seen and taken back. A snapshot is kept each time the editor
//! writes a file, the text as it was before the first one among them, and none the same as the one
//! before it. Each file keeps its newest KEPT.
//!
//! The snapshots of a file are in history/<tree>/<file>/<milliseconds>.txt under orior's folder, the
//! tree and the file each named by a hash of its path and the file by its name after it too.

use std::path::{Path, PathBuf};
use std::time::{SystemTime, UNIX_EPOCH};

use serde::Serialize;

/// The most snapshots a file keeps.
const KEPT: usize = 100;

const NO_HOME: &str = "orior has no folder of its own here";

/// One snapshot: when it was kept, in milliseconds since 1970, and its size in bytes.
#[derive(Serialize, Debug)]
pub struct Snapshot {
    pub at: u64,
    pub size: u64,
}

/// FNV-1a over the bytes, which names a folder the same on every run.
fn hash(text: &str) -> String {
    let mut value: u64 = 0xcbf2_9ce4_8422_2325;
    for byte in text.bytes() {
        value ^= u64::from(byte);
        value = value.wrapping_mul(0x0100_0000_01b3);
    }
    format!("{value:016x}")
}

fn folder_of(home: &Path, root: &Path, path: &str) -> PathBuf {
    let name = path.rsplit('/').next().unwrap_or(path).chars().filter(|char| char.is_alphanumeric() || "._-".contains(*char)).collect::<String>();
    home.join("history").join(hash(&root.display().to_string())).join(format!("{}-{name}", hash(path)))
}

fn stamps(folder: &Path) -> Vec<u64> {
    let mut found: Vec<u64> = std::fs::read_dir(folder)
        .map(|entries| entries.flatten().filter_map(|entry| entry.file_name().to_string_lossy().strip_suffix(".txt")?.parse().ok()).collect())
        .unwrap_or_default();
    found.sort_unstable();
    found
}

fn now() -> u64 {
    SystemTime::now().duration_since(UNIX_EPOCH).map(|since| since.as_millis() as u64).unwrap_or(0)
}

/// Keeps `text` as the newest snapshot of the file at `path` under the tree at `root`, and `before`,
/// the text the file held, ahead of it where the file has none yet.
pub fn keep(root: &Path, path: &str, before: Option<&str>, text: &str) -> Result<(), String> {
    keep_in(&crate::home::folder().ok_or(NO_HOME)?, root, path, before, text)
}

/// The snapshots of the file at `path`, the newest first.
pub fn list(root: &Path, path: &str) -> Vec<Snapshot> {
    crate::home::folder().map(|home| list_in(&home, root, path)).unwrap_or_default()
}

/// The text of the snapshot kept at `at`.
pub fn read(root: &Path, path: &str, at: u64) -> Result<String, String> {
    read_in(&crate::home::folder().ok_or(NO_HOME)?, root, path, at)
}

fn keep_in(home: &Path, root: &Path, path: &str, before: Option<&str>, text: &str) -> Result<(), String> {
    let folder = folder_of(home, root, path);
    std::fs::create_dir_all(&folder).map_err(|error| format!("{}: {error}", folder.display()))?;
    let mut held = stamps(&folder);
    let mut at = now();
    if held.is_empty() {
        if let Some(before) = before.filter(|before| *before != text) {
            std::fs::write(folder.join(format!("{at}.txt")), before).map_err(|error| error.to_string())?;
            held.push(at);
            at += 1;
        }
    }
    if let Some(&last) = held.last() {
        if std::fs::read_to_string(folder.join(format!("{last}.txt"))).is_ok_and(|kept| kept == text) {
            return Ok(());
        }
        at = at.max(last + 1);
    }
    std::fs::write(folder.join(format!("{at}.txt")), text).map_err(|error| error.to_string())?;
    held.push(at);
    for old in held.iter().take(held.len().saturating_sub(KEPT)) {
        let _ = std::fs::remove_file(folder.join(format!("{old}.txt")));
    }
    Ok(())
}

fn list_in(home: &Path, root: &Path, path: &str) -> Vec<Snapshot> {
    let folder = folder_of(home, root, path);
    stamps(&folder).into_iter().rev().map(|at| Snapshot { at, size: std::fs::metadata(folder.join(format!("{at}.txt"))).map(|meta| meta.len()).unwrap_or(0) }).collect()
}

fn read_in(home: &Path, root: &Path, path: &str, at: u64) -> Result<String, String> {
    std::fs::read_to_string(folder_of(home, root, path).join(format!("{at}.txt"))).map_err(|error| format!("no snapshot of {path} at {at}: {error}"))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn saves_are_kept_newest_first_without_repeats() {
        let home = std::env::temp_dir().join(format!("orior-history-{}", std::process::id()));
        let root = Path::new("/tree");
        keep_in(&home, root, "src/a.c", Some("zero"), "one").unwrap();
        keep_in(&home, root, "src/a.c", Some("one"), "one").unwrap();
        keep_in(&home, root, "src/a.c", Some("one"), "two").unwrap();
        let texts: Vec<String> = list_in(&home, root, "src/a.c").iter().map(|one| read_in(&home, root, "src/a.c", one.at).unwrap()).collect();
        assert_eq!(texts, ["two", "one", "zero"]);
        assert!(list_in(&home, root, "src/b.c").is_empty());
        let _ = std::fs::remove_dir_all(&home);
    }
}

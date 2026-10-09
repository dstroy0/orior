// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The editor's reads and writes, every one inside the tree.
//!
//! A folder's list holds every entry in it but git's own folder, and marks the ones the tree's ignore
//! files leave out: what a build writes, what a cache keeps and what a tool keeps for itself. A search
//! finds only what git tracks or would track. A tree git cannot read is listed whole, less SKIPPED,
//! and nothing in it is marked.

use std::collections::BTreeSet;
use std::fs;
use std::io::{Read, Seek, SeekFrom, Write};
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

use serde::Serialize;

use crate::root::{inside, relative};

/// The folders the list leaves out where git cannot say: version control's own, and the one cargo
/// writes the app's build to.
const SKIPPED: [&str; 2] = [".git", "target"];

/// How long one reading of git's view stands before it is read again.
const SEEN_FOR: Duration = Duration::from_secs(4);

/// The largest text the editor reads whole. A larger text opens as a window of lines that grows
/// outward a slice at a time.
const WINDOW_FROM: u64 = 1024 * 1024;

/// The bytes read at a time while looking for a line.
const STRIDE: usize = 1024 * 1024;

/// The bytes of a binary file the editor shows.
const SHOWN_BYTES: usize = 64 * 1024;

#[derive(Serialize)]
pub struct Entry {
    pub name: String,
    pub path: String,
    pub dir: bool,
    /// Whether the tree's ignore files leave the entry out.
    pub ignored: bool,
}

#[derive(Serialize)]
pub struct Opened {
    pub path: String,
    pub size: u64,
    /// The text, where the file is text.
    pub text: Option<String>,
    /// The first bytes, where the file is not text.
    pub bytes: Option<Vec<u8>>,
    /// Whether the file is text too large to read whole, to be read a window at a time.
    pub windowed: bool,
}

/// Whole lines of a file: the bytes from `start` to `end`, `line` the number of the first of them.
#[derive(Serialize)]
pub struct Slice {
    pub start: u64,
    pub end: u64,
    pub size: u64,
    pub line: u64,
    pub text: String,
}

/// Git's view of the tree: every file it tracks or would track, and every folder holding one.
struct Seen {
    files: BTreeSet<String>,
    dirs: BTreeSet<String>,
}

type Kept = Option<(PathBuf, Instant, Arc<Seen>)>;

static KEPT: Mutex<Kept> = Mutex::new(None);

fn seen(root: &Path) -> Option<Arc<Seen>> {
    let mut kept = KEPT.lock().ok()?;
    if let Some((at, when, view)) = kept.as_ref() {
        if at == root && when.elapsed() < SEEN_FOR {
            return Some(view.clone());
        }
    }
    let mut git = Command::new("git");
    git.args(["ls-files", "--cached", "--others", "--exclude-standard", "-z"]).current_dir(root);
    git.stdin(Stdio::null()).stderr(Stdio::null());
    crate::runner::quiet(&mut git);
    let out = git.output().ok().filter(|out| out.status.success())?;
    let mut files = BTreeSet::new();
    let mut dirs = BTreeSet::new();
    for path in String::from_utf8_lossy(&out.stdout).split('\0').filter(|p| !p.is_empty()) {
        let mut at = 0;
        while let Some(slash) = path[at..].find('/') {
            at += slash;
            dirs.insert(path[..at].to_string());
            at += 1;
        }
        files.insert(path.to_string());
    }
    let view = Arc::new(Seen { files, dirs });
    *kept = Some((root.to_path_buf(), Instant::now(), view.clone()));
    Some(view)
}

pub fn list(root: &Path, dir: &str) -> Result<Vec<Entry>, String> {
    let path = inside(root, dir)?;
    let view = seen(root);
    let mut entries: Vec<Entry> = fs::read_dir(&path)
        .map_err(|e| format!("{dir}: {e}"))?
        .flatten()
        .filter_map(|entry| {
            let name = entry.file_name().to_string_lossy().into_owned();
            let is_dir = entry.file_type().ok()?.is_dir();
            let path = relative(root, &entry.path());
            if is_dir && name == ".git" {
                return None;
            }
            let ignored = match &view {
                Some(view) if is_dir => !view.dirs.contains(&path),
                Some(view) => !view.files.contains(&path),
                None if is_dir && SKIPPED.contains(&name.as_str()) => return None,
                None => false,
            };
            Some(Entry { path, name, dir: is_dir, ignored })
        })
        .collect();
    entries.sort_by(|a, b| b.dir.cmp(&a.dir).then_with(|| a.name.to_lowercase().cmp(&b.name.to_lowercase())));
    Ok(entries)
}

/// The most paths the whole list holds, for a tree git cannot read.
const ALL_LIMIT: usize = 100_000;

/// Every file git tracks or would track, or for a tree git cannot read every file less SKIPPED, at
/// most ALL_LIMIT of them, sorted.
pub fn all(root: &Path) -> Vec<String> {
    if let Some(view) = seen(root) {
        return view.files.iter().cloned().collect();
    }
    let mut found = Vec::new();
    let mut pending = vec![root.to_path_buf()];
    while let Some(dir) = pending.pop() {
        let Ok(entries) = fs::read_dir(&dir) else { continue };
        for entry in entries.flatten() {
            let Ok(kind) = entry.file_type() else { continue };
            if kind.is_dir() {
                if !SKIPPED.contains(&entry.file_name().to_string_lossy().as_ref()) {
                    pending.push(entry.path());
                }
            } else if found.len() < ALL_LIMIT {
                found.push(relative(root, &entry.path()));
            }
        }
    }
    found.sort();
    found
}

/// One line a search found: the file, the line and the column counted from 1, and the line's text,
/// cut to HIT_TEXT characters.
#[derive(Serialize)]
pub struct Hit {
    pub path: String,
    pub line: u64,
    pub col: u64,
    pub text: String,
}

/// How a search reads its query: case as given or not, whole words only or not, and a regular
/// expression or the text itself.
#[derive(Clone, Copy, Default, serde::Deserialize)]
pub struct Searching {
    #[serde(default)]
    pub case: bool,
    #[serde(default)]
    pub word: bool,
    #[serde(default)]
    pub regex: bool,
}

/// The most lines a search returns.
const HITS_LIMIT: usize = 2000;

/// The most characters of a found line kept.
const HIT_TEXT: usize = 240;

/// The files larger than this a search of a tree git cannot read passes over.
const SEARCHED_BYTES: u64 = 4 * 1024 * 1024;

fn hit(path: String, line: u64, col: u64, text: &str) -> Hit {
    Hit { path, line, col, text: text.trim_end().chars().take(HIT_TEXT).collect() }
}

/// Every line in the tree's files that holds the query, at most HITS_LIMIT of them. Git searches what
/// it tracks or would track; a tree git cannot read is searched a file at a time, the query read as
/// the text itself.
pub fn search(root: &Path, query: &str, how: Searching) -> Result<Vec<Hit>, String> {
    if query.is_empty() {
        return Ok(Vec::new());
    }
    let mut git = Command::new("git");
    git.args(["grep", "-n", "--column", "-I", "--no-color", "--untracked", "--full-name"]).current_dir(root);
    if !how.case {
        git.arg("-i");
    }
    if how.word {
        git.arg("-w");
    }
    git.arg(if how.regex { "-E" } else { "-F" });
    git.args(["-e", query, "--", "."]);
    git.stdin(Stdio::null()).stderr(Stdio::piped());
    crate::runner::quiet(&mut git);
    if let Ok(out) = git.output() {
        // Git says 1 where nothing matched, and more where it could not search.
        match out.status.code() {
            Some(0) => {
                let prefix = Command::new("git").args(["rev-parse", "--show-prefix"]).current_dir(root).output().ok().map(|out| String::from_utf8_lossy(&out.stdout).trim().to_string()).unwrap_or_default();
                return Ok(String::from_utf8_lossy(&out.stdout)
                    .lines()
                    .filter_map(|line| {
                        let mut parts = line.splitn(4, ':');
                        let path = parts.next()?;
                        let number = parts.next()?.parse().ok()?;
                        let col = parts.next()?.parse().ok()?;
                        let path = path.strip_prefix(prefix.as_str()).unwrap_or(path).to_string();
                        Some(hit(path, number, col, parts.next().unwrap_or_default()))
                    })
                    .take(HITS_LIMIT)
                    .collect());
            }
            Some(1) => return Ok(Vec::new()),
            _ if String::from_utf8_lossy(&out.stderr).contains("regular expression") => {
                return Err(String::from_utf8_lossy(&out.stderr).trim().to_string());
            }
            _ => {}
        }
    }
    let wanted = if how.case { query.to_string() } else { query.to_lowercase() };
    let boundary = |text: &str, at: usize, length: usize| {
        let before = text[..at].chars().next_back();
        let after = text[at + length..].chars().next();
        let wordy = |c: Option<char>| c.is_some_and(|c| c.is_alphanumeric() || c == '_');
        !wordy(before) && !wordy(after)
    };
    let mut found = Vec::new();
    for path in all(root) {
        let full = root.join(&path);
        if full.metadata().map_or(true, |meta| meta.len() > SEARCHED_BYTES) {
            continue;
        }
        let Ok(bytes) = fs::read(&full) else { continue };
        if bytes[..bytes.len().min(8192)].contains(&0) {
            continue;
        }
        let text = String::from_utf8_lossy(&bytes);
        for (index, line) in text.lines().enumerate() {
            let seen_as = if how.case { line.to_string() } else { line.to_lowercase() };
            let mut from = 0;
            while let Some(at) = seen_as[from..].find(&wanted) {
                let at = from + at;
                if !how.word || boundary(&seen_as, at, wanted.len()) {
                    let col = line.get(..at).map_or(0, |before| before.chars().count()) as u64 + 1;
                    found.push(hit(path.clone(), index as u64 + 1, col, line));
                    break;
                }
                from = at + wanted.len().max(1);
            }
            if found.len() >= HITS_LIMIT {
                return Ok(found);
            }
        }
    }
    Ok(found)
}

/// The most paths a search returns.
const FOUND_LIMIT: usize = 500;

/// Every listed file whose path holds each word of the query, case aside, at most FOUND_LIMIT of
/// them.
pub fn find(root: &Path, query: &str) -> Vec<String> {
    let words: Vec<String> = query.to_lowercase().split_whitespace().map(str::to_string).collect();
    let holds = |path: &str| {
        let lower = path.to_lowercase();
        words.iter().all(|word| lower.contains(word))
    };
    if let Some(view) = seen(root) {
        return view.files.iter().filter(|p| holds(p)).take(FOUND_LIMIT).cloned().collect();
    }
    let mut found = Vec::new();
    let mut pending = vec![root.to_path_buf()];
    while let Some(dir) = pending.pop() {
        let Ok(entries) = fs::read_dir(&dir) else { continue };
        for entry in entries.flatten() {
            let name = entry.file_name().to_string_lossy().into_owned();
            let Ok(kind) = entry.file_type() else { continue };
            if kind.is_dir() {
                if !SKIPPED.contains(&name.as_str()) {
                    pending.push(entry.path());
                }
                continue;
            }
            let path = relative(root, &entry.path());
            if holds(&path) {
                found.push(path);
                if found.len() >= FOUND_LIMIT {
                    found.sort();
                    return found;
                }
            }
        }
    }
    found.sort();
    found
}

pub fn read(root: &Path, file: &str) -> Result<Opened, String> {
    let path = inside(root, file)?;
    let size = path.metadata().map_err(|e| format!("{file}: {e}"))?.len();
    let path_text = relative(root, &path);
    if size <= WINDOW_FROM {
        let bytes = fs::read(&path).map_err(|e| format!("{file}: {e}"))?;
        let probe = &bytes[..bytes.len().min(8192)];
        if !probe.contains(&0) {
            if let Ok(text) = String::from_utf8(bytes.clone()) {
                return Ok(Opened { path: path_text, size, text: Some(text), bytes: None, windowed: false });
            }
        }
        let shown = bytes.into_iter().take(SHOWN_BYTES).collect();
        return Ok(Opened { path: path_text, size, text: None, bytes: Some(shown), windowed: false });
    }
    let mut handle = fs::File::open(&path).map_err(|e| format!("{file}: {e}"))?;
    let shown = bytes_at(&mut handle, 0, SHOWN_BYTES).map_err(|e| format!("{file}: {e}"))?;
    // Text where the first bytes hold no NUL and read as UTF-8, a character the cut splits aside.
    let probe = &shown[..shown.len().min(8192)];
    let text = !probe.contains(&0) && std::str::from_utf8(probe).map_or_else(|e| e.error_len().is_none(), |_| true);
    Ok(Opened { path: path_text, size, text: None, bytes: (!text).then_some(shown), windowed: text })
}

fn bytes_at(handle: &mut fs::File, at: u64, length: usize) -> std::io::Result<Vec<u8>> {
    handle.seek(SeekFrom::Start(at))?;
    let mut out = Vec::with_capacity(length);
    handle.take(length as u64).read_to_end(&mut out)?;
    Ok(out)
}

/// The first line start at or after `at`: `at` itself where a line ends just before it, or else
/// just past the next newline, or the end of the file.
fn line_start(handle: &mut fs::File, at: u64, size: u64) -> std::io::Result<u64> {
    if at == 0 || at >= size {
        return Ok(at.min(size));
    }
    let mut from = at - 1;
    while from < size {
        let chunk = bytes_at(handle, from, STRIDE)?;
        if let Some(found) = chunk.iter().position(|&b| b == b'\n') {
            return Ok(from + found as u64 + 1);
        }
        from += chunk.len() as u64;
        if chunk.is_empty() {
            break;
        }
    }
    Ok(size)
}

fn newlines(handle: &mut fs::File, from: u64, to: u64) -> std::io::Result<u64> {
    let mut count = 0;
    let mut at = from;
    while at < to {
        let chunk = bytes_at(handle, at, STRIDE.min((to - at) as usize))?;
        if chunk.is_empty() {
            break;
        }
        count += chunk.iter().filter(|&&b| b == b'\n').count() as u64;
        at += chunk.len() as u64;
    }
    Ok(count)
}

fn slice_of(handle: &mut fs::File, start: u64, end: u64, size: u64, line: u64) -> std::io::Result<Slice> {
    let bytes = bytes_at(handle, start, (end - start) as usize)?;
    Ok(Slice { start, end, size, line, text: String::from_utf8_lossy(&bytes).into_owned() })
}

/// The lines around line `line`, about `half` bytes either side of where it starts, cut at line
/// ends. A line past the end of the file is the file's last.
pub fn window(root: &Path, file: &str, line: u64, half: u64) -> Result<Slice, String> {
    let path = inside(root, file)?;
    let said = |e: std::io::Error| format!("{file}: {e}");
    let mut handle = fs::File::open(&path).map_err(said)?;
    let size = handle.metadata().map_err(said)?.len();
    let mut at = 0u64;
    let mut seen = 0u64;
    let mut last_start = 0u64;
    'reading: while seen < line && at < size {
        let chunk = bytes_at(&mut handle, at, STRIDE).map_err(said)?;
        if chunk.is_empty() {
            break;
        }
        for (index, &byte) in chunk.iter().enumerate() {
            if byte == b'\n' {
                seen += 1;
                last_start = at + index as u64 + 1;
                if seen == line {
                    break 'reading;
                }
            }
        }
        at += chunk.len() as u64;
    }
    let target = if seen == line { last_start } else { last_start.min(size) };
    let target_line = seen.min(line);
    let start = line_start(&mut handle, target.saturating_sub(half), size).map_err(said)?.min(target);
    let end = line_start(&mut handle, (target + half).min(size), size).map_err(said)?;
    let first = target_line - newlines(&mut handle, start, target).map_err(said)?;
    slice_of(&mut handle, start, end.max(start), size, first).map_err(said)
}

/// The whole lines from about `start` to about `end`, each moved on to the next line start. Its
/// `line` is not counted and reads 0.
pub fn slice(root: &Path, file: &str, start: u64, end: u64) -> Result<Slice, String> {
    let path = inside(root, file)?;
    let said = |e: std::io::Error| format!("{file}: {e}");
    let mut handle = fs::File::open(&path).map_err(said)?;
    let size = handle.metadata().map_err(said)?.len();
    let from = line_start(&mut handle, start, size).map_err(said)?;
    let to = line_start(&mut handle, end, size).map_err(said)?.max(from);
    slice_of(&mut handle, from, to, size, 0).map_err(said)
}

/// Makes an empty file at `file`, and each folder above it that is not there yet. A file or folder
/// there already is left as it is.
pub fn create_file(root: &Path, file: &str) -> Result<(), String> {
    let path = inside(root, file)?;
    if path.exists() {
        return Err(format!("{file} is there already"));
    }
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent).map_err(|e| format!("{file}: {e}"))?;
    }
    fs::File::create_new(&path).map(drop).map_err(|e| format!("{file}: {e}"))
}

/// Makes a folder at `dir`, and each folder above it that is not there yet.
pub fn create_folder(root: &Path, dir: &str) -> Result<(), String> {
    let path = inside(root, dir)?;
    if path.exists() {
        return Err(format!("{dir} is there already"));
    }
    fs::create_dir_all(&path).map_err(|e| format!("{dir}: {e}"))
}

/// Writes the text beside the file and moves it over the file. A failed write leaves the file as
/// it was.
pub fn write(root: &Path, file: &str, text: &str) -> Result<(), String> {
    let path = inside(root, file)?;
    let parent = path.parent().ok_or_else(|| format!("{file} has no folder"))?;
    let name = path.file_name().ok_or_else(|| format!("{file} has no name"))?.to_string_lossy();
    let beside = parent.join(format!(".{name}.saving"));
    let mut out = fs::File::create(&beside).map_err(|e| format!("{file}: {e}"))?;
    out.write_all(text.as_bytes()).and_then(|_| out.sync_all()).map_err(|e| format!("{file}: {e}"))?;
    drop(out);
    fs::rename(&beside, &path).map_err(|e| {
        let _ = fs::remove_file(&beside);
        format!("{file}: {e}")
    })
}

#[cfg(test)]
mod windows {
    use super::{slice, window};

    #[test]
    fn a_window_and_its_slices_are_whole_numbered_lines() {
        let root = std::env::temp_dir().join(format!("orior_ui_window_{}", std::process::id()));
        std::fs::create_dir_all(&root).unwrap();
        let text: String = (0..5000).map(|n| format!("line {n} {}\n", "x".repeat(n % 37))).collect();
        std::fs::write(root.join("lines.txt"), &text).unwrap();
        let shown = window(&root, "lines.txt", 2500, 3000).unwrap();
        let first = shown.text.lines().next().unwrap();
        assert_eq!(first.split(' ').nth(1).unwrap(), shown.line.to_string());
        assert!(shown.text.contains("\nline 2500 "));
        assert!(shown.text.ends_with('\n'));
        let below = slice(&root, "lines.txt", shown.end, shown.end + 2000).unwrap();
        assert_eq!(below.start, shown.end);
        let next = shown.line + shown.text.lines().count() as u64;
        assert!(below.text.starts_with(&format!("line {next} ")));
        let above = slice(&root, "lines.txt", shown.start.saturating_sub(2000), shown.start).unwrap();
        assert_eq!(above.end, shown.start);
        assert!(above.text.ends_with(&format!("line {} {}\n", shown.line - 1, "x".repeat((shown.line as usize - 1) % 37))));
        let past = window(&root, "lines.txt", 99999, 100).unwrap();
        assert_eq!(past.end, past.size);
        let _ = std::fs::remove_dir_all(&root);
    }
}

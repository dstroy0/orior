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

use crate::patterns::{self, Part, Patterns};
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
    /// Whether the entry is an archive, which opens as a folder.
    pub archive: bool,
    /// A name in an archive whose bytes are not UTF-8 by the archive's word: its bytes, in hex.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub raw: Option<String>,
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
    /// The lines in the whole file, counted for a window and 0 for a slice.
    pub lines: u64,
    pub text: String,
}

/// Git's view of the tree: every file it tracks or would track, and every folder holding one.
struct Seen {
    files: BTreeSet<String>,
    dirs: BTreeSet<String>,
}

/// The tree git's view was last read for, when, and the view, None where git could not read the
/// tree.
type Kept = Option<(PathBuf, Instant, Option<Arc<Seen>>)>;

static KEPT: Mutex<Kept> = Mutex::new(None);

/// Whether git's view is being read again behind the view kept.
static RESEEING: std::sync::atomic::AtomicBool = std::sync::atomic::AtomicBool::new(false);

/// Git's view of the tree: as kept, read again behind it once it is older than SEEN_FOR, or read now
/// where none is kept, the view let go as files move.
fn seen(root: &Path) -> Option<Arc<Seen>> {
    let mut kept = KEPT.lock().ok()?;
    if let Some((at, when, view)) = kept.as_ref() {
        if at == root && when.elapsed() < SEEN_FOR {
            return view.clone();
        }
        if let (true, Some(view)) = (at == root, view) {
            if !RESEEING.swap(true, std::sync::atomic::Ordering::SeqCst) {
                let root = root.to_path_buf();
                std::thread::spawn(move || {
                    let read = read_seen(&root);
                    if let Ok(mut kept) = KEPT.lock() {
                        // A view let go as it was read is not put back: the next one reads it anew.
                        if matches!(kept.as_ref(), Some((at, ..)) if *at == root) {
                            *kept = Some((root, Instant::now(), read));
                        }
                    }
                    RESEEING.store(false, std::sync::atomic::Ordering::SeqCst);
                });
            }
            return Some(view.clone());
        }
    }
    let view = read_seen(root);
    *kept = Some((root.to_path_buf(), Instant::now(), view.clone()));
    view
}

/// Reads git's view of the tree in the background, as a tree opens, for the first list or search.
pub fn warm(root: &Path) {
    let root = root.to_path_buf();
    std::thread::spawn(move || {
        seen(&root);
    });
}

/// Git's view of the tree as git gives it now, or None where git cannot read it or lists nothing.
fn read_seen(root: &Path) -> Option<Arc<Seen>> {
    let mut git = Command::new("git");
    git.args(crate::trust::git_words(root)).args(["ls-files", "--cached", "--others", "--exclude-standard", "-z"]).current_dir(root);
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
    // A tree in a folder its repository ignores is one git lists nothing of: it is read from the disk.
    if files.is_empty() {
        return None;
    }
    Some(Arc::new(Seen { files, dirs }))
}

/// Lets go of git's view of the tree, to be read again by the next list or search, as after files
/// are made, moved or taken out.
pub fn forget_seen() {
    if let Ok(mut kept) = KEPT.lock() {
        *kept = None;
    }
}

/// Whether git tracks the path or would: a file it lists, or a file in a folder holding one it lists
/// or at the tree's top. Every path is, in a tree git cannot read.
pub fn tracked(root: &Path, path: &str) -> bool {
    let Some(view) = seen(root) else { return true };
    let folder = path.rsplit_once('/').map_or("", |(folder, _)| folder);
    view.files.contains(path) || folder.is_empty() || view.dirs.contains(folder)
}

/// A folder's entries, less what the explorer's patterns hide. Where a pattern keeps only what it
/// names, a folder shows while it holds a file kept or a pattern keeps it.
pub fn list(root: &Path, dir: &str) -> Result<Vec<Entry>, String> {
    // An archive, and a folder within one, lists what the archive holds.
    if crate::archive::split(root, dir).is_some() {
        let children = crate::archive::list(root, dir)?;
        return Ok(children.into_iter().map(|child| Entry { name: child.name, path: child.path, dir: child.dir, ignored: false, archive: child.archive, raw: child.raw }).collect());
    }
    let path = inside(root, dir)?;
    let view = seen(root);
    let patterns = patterns::of(Part::Explorer);
    let holding = patterns.keeps_only().then(|| holding(root, &patterns));
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
            if patterns.hides(&path, is_dir) {
                return None;
            }
            if let Some(holding) = &holding
                && is_dir
                && !holding.contains(&path)
                && !patterns.keeps_folder(&path)
            {
                return None;
            }
            let ignored = match &view {
                Some(view) if is_dir => !view.dirs.contains(&path),
                Some(view) => !view.files.contains(&path),
                None if is_dir && SKIPPED.contains(&name.as_str()) => return None,
                None => false,
            };
            let archive = !is_dir && crate::archive::kind_of(&name).is_some();
            Some(Entry { path, name, dir: is_dir || archive, ignored, archive, raw: None })
        })
        .collect();
    entries.sort_by(|a, b| b.dir.cmp(&a.dir).then_with(|| a.name.to_lowercase().cmp(&b.name.to_lowercase())));
    Ok(entries)
}

/// Every folder that holds a file the patterns leave.
fn holding(root: &Path, patterns: &Patterns) -> BTreeSet<String> {
    let mut folders = BTreeSet::new();
    for file in all(root).iter().filter(|file| !patterns.hides(file, false)) {
        let mut at = 0;
        while let Some(slash) = file[at..].find('/') {
            at += slash;
            folders.insert(file[..at].to_string());
            at += 1;
        }
    }
    folders
}

/// The tree's files less what the patterns hide.
fn left(files: impl Iterator<Item = String>, patterns: &Patterns) -> Vec<String> {
    files.filter(|file| !patterns.hides(file, false)).collect()
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

/// A letter of a path as the matcher reads it: a byte of a path that is all ASCII, or a character of
/// one that is not.
trait Letter: Copy + PartialEq {
    fn upper(self) -> bool;
    fn lower(self) -> Self;
    fn splits(self) -> bool;
}

impl Letter for u8 {
    fn upper(self) -> bool {
        self.is_ascii_uppercase()
    }
    fn lower(self) -> Self {
        self.to_ascii_lowercase()
    }
    fn splits(self) -> bool {
        matches!(self, b' ' | b'\t' | b'/' | b'\\' | b'_' | b'-' | b'.' | b':' | b'@')
    }
}

impl Letter for char {
    fn upper(self) -> bool {
        self.is_uppercase()
    }
    fn lower(self) -> Self {
        self.to_lowercase().next().unwrap_or(self)
    }
    fn splits(self) -> bool {
        self.is_whitespace() || matches!(self, '/' | '\\' | '_' | '-' | '.' | ':' | '@')
    }
}

/// Whether a word starts at `at`: the text's start, after a space, a slash, a separator in a name, a
/// dot, a colon or an at, or an upper case letter after one that is not.
fn starts_word<T: Letter>(text: &[T], at: usize) -> bool {
    at == 0 || text[at - 1].splits() || (!text[at - 1].upper() && text[at].upper())
}

/// The match of `wanted`, lower case, in `shown` and its lower case `lower`, letter for letter; see
/// `fuzzy`.
fn fuzzy_in<T: Letter>(wanted: &[T], shown: &[T], lower: &[T], from: usize) -> Option<(f64, Vec<usize>)> {
    if wanted.is_empty() {
        return Some((0.0, Vec::new()));
    }
    // The latest place each letter can stand with the letters after it still fitting.
    let mut latest = vec![0usize; wanted.len()];
    let mut at = lower.len();
    for index in (0..wanted.len()).rev() {
        at = lower[..at].iter().rposition(|letter| *letter == wanted[index])?;
        latest[index] = at;
    }
    let mut hits = Vec::with_capacity(wanted.len());
    let mut score = 0.0;
    let mut last: Option<usize> = None;
    for (index, letter) in wanted.iter().enumerate() {
        let after = last.map_or(0, |last| last + 1);
        // The letter right after the one before it, else the first that starts a word, else the first.
        let mut found = last.filter(|_| lower.get(after) == Some(letter)).map(|_| after);
        let mut first = None;
        let mut place = after;
        while found.is_none() && place <= latest[index] {
            if lower[place] == *letter {
                first = first.or(Some(place));
                if starts_word(shown, place) {
                    found = Some(place);
                }
            }
            place += 1;
        }
        let found = found.or(first)?;
        score += 1.0;
        if Some(found) == last.map(|last| last + 1) {
            score += 5.0;
        }
        if starts_word(shown, found) {
            score += 8.0;
        }
        if found >= from {
            score += 3.0;
        }
        if shown[found] == *letter {
            score += 1.0;
        }
        hits.push(found);
        last = Some(found);
    }
    // A query torn into more pieces than half its letters reads as no match.
    let runs = hits.iter().enumerate().filter(|(index, at)| *index == 0 || **at != hits[index - 1] + 1).count();
    if runs > 3.max(wanted.len().div_ceil(2)) {
        return None;
    }
    score -= (hits[hits.len() - 1] - hits[0]) as f64 * 0.1 + shown.len() as f64 * 0.02;
    Some((score, hits))
}

/// The places in `text` that match `query`, counted in characters, and the match's score, or None
/// where it does not match: the query's letters in order anywhere in the text, case aside, in no more
/// pieces than half its letters or three. A letter scores more at the start of a word or past `from`,
/// where the text's name starts, and right after the letter before it. The palette's own matcher,
/// fuzzy.js, scores the same.
pub fn fuzzy(query: &str, text: &str, from: usize) -> Option<(f64, Vec<usize>)> {
    let wanted: Vec<char> = query.chars().filter(|letter| !letter.is_whitespace()).map(Letter::lower).collect();
    let shown: Vec<char> = text.chars().collect();
    let lower: Vec<char> = shown.iter().map(|letter| letter.lower()).collect();
    fuzzy_in(&wanted, &shown, &lower, from)
}

/// What a file named exactly as typed scores over the match itself, and one whose name starts with
/// it half that.
const EXACT: f64 = 40.0;

/// What a file opened lately scores over the match itself.
const LATELY: f64 = 6.0;

/// A file that answers a quick open: its path, its score and the characters of the path that
/// matched.
#[derive(Serialize)]
pub struct Found {
    pub path: String,
    pub score: f64,
    pub hits: Vec<usize>,
}

/// A file of the tree as a quick open reads it: its path, the path in lower case, and where its
/// name starts, in characters.
struct Listed {
    path: String,
    lower: String,
    from: usize,
}

impl Listed {
    fn of(path: &str) -> Listed {
        let from = path.rfind('/').map_or(0, |at| path[..=at].chars().count());
        Listed { path: path.to_string(), lower: path.chars().map(Letter::lower).collect(), from }
    }
}

/// The tree's files as the last quick open read them, when, whether they are being read again,
/// whether they have been read once, and how many reads of them have begun.
struct Held {
    root: PathBuf,
    at: Instant,
    files: Arc<Vec<Listed>>,
    reading: bool,
    read: bool,
    begun: u64,
}

/// What a quick open found, the best first, and whether the tree's files were still being read for
/// the first time.
#[derive(Serialize)]
pub struct Answer {
    pub found: Vec<Found>,
    pub reading: bool,
}

static HELD: Mutex<Option<Held>> = Mutex::new(None);

/// The tree's files, read as `all` reads them.
fn listed(root: &Path) -> Arc<Vec<Listed>> {
    let patterns = patterns::of(Part::Search);
    let files: Vec<Listed> = match seen(root) {
        Some(view) => view.files.iter().filter(|path| !patterns.hides(path, false)).map(|path| Listed::of(path)).collect(),
        None => left(all(root).into_iter(), &patterns).iter().map(|path| Listed::of(path)).collect(),
    };
    Arc::new(files)
}

/// Reads the files the quick open holds again behind the window, as after the search's patterns
/// change, the files read so far answering meanwhile.
pub fn forget_held() {
    let mut held = HELD.lock().unwrap_or_else(|poisoned| poisoned.into_inner());
    if let Some(kept) = held.as_mut() {
        read_into(kept);
    }
}

/// Reads the tree's files on a thread of their own into what the quick open holds, taken where no
/// later read has begun.
fn read_into(kept: &mut Held) {
    kept.reading = true;
    kept.begun += 1;
    let (root, begun) = (kept.root.clone(), kept.begun);
    std::thread::spawn(move || {
        let files = listed(&root);
        let mut held = HELD.lock().unwrap_or_else(|poisoned| poisoned.into_inner());
        if let Some(kept) = held.as_mut().filter(|kept| kept.root == root && kept.begun == begun) {
            kept.files = files;
            kept.at = Instant::now();
            kept.reading = false;
            kept.read = true;
        }
    });
}

/// Starts reading the tree's files for the quick open behind the window, where they are neither held
/// nor being read.
pub fn read_behind(root: &Path) {
    let mut held = HELD.lock().unwrap_or_else(|poisoned| poisoned.into_inner());
    if held.as_ref().is_some_and(|kept| kept.root == root) {
        return;
    }
    read_into(held.insert(Held { root: root.to_path_buf(), at: Instant::now(), files: Arc::new(Vec::new()), reading: true, read: false, begun: 0 }));
}

/// Whether the tree's files are not read yet for the quick open.
pub fn unread(root: &Path) -> bool {
    !HELD.lock().unwrap_or_else(|poisoned| poisoned.into_inner()).as_ref().is_some_and(|kept| kept.root == root && kept.read)
}

/// The tree's files for a quick open, and whether they have been read once: the files last read,
/// read again behind the search once they are older than SEEN_FOR, and none until the first read,
/// begun behind the window where it has not begun, ends.
fn held(root: &Path) -> (Arc<Vec<Listed>>, bool) {
    let mut held = HELD.lock().unwrap_or_else(|poisoned| poisoned.into_inner());
    match held.as_mut() {
        Some(kept) if kept.root == root => {
            if kept.read && kept.at.elapsed() >= SEEN_FOR && !kept.reading {
                read_into(kept);
            }
            (kept.files.clone(), kept.read)
        }
        _ => {
            drop(held);
            read_behind(root);
            (Arc::new(Vec::new()), false)
        }
    }
}

/// The tree's files that answer `query`, at most `most` of them, the best first, and whether the
/// tree's files are not read yet: until they are, the files of `recent` alone answer. A file in
/// `recent` scores LATELY more, and one named as typed, with or without its extension, EXACT more, as
/// one whose name starts with what was typed scores half of EXACT more.
pub fn ranked(root: &Path, query: &str, recent: &[String], most: usize) -> Answer {
    let (files, read) = held(root);
    let files = if read { files } else { Arc::new(recent.iter().map(|path| Listed::of(path)).collect()) };
    Answer { found: ranked_in(&files, query, recent, most), reading: !read }
}

fn ranked_in(files: &[Listed], query: &str, recent: &[String], most: usize) -> Vec<Found> {
    let lowered: String = query.chars().map(Letter::lower).collect();
    let asked = lowered.rsplit('/').next().unwrap_or_default();
    let wanted_bytes: Vec<u8> = lowered.bytes().filter(|byte| !byte.is_ascii_whitespace()).collect();
    let wanted_chars: Vec<char> = lowered.chars().filter(|letter| !letter.is_whitespace()).collect();
    let mut found: Vec<Found> = Vec::new();
    for file in files.iter() {
        let matched = if file.path.is_ascii() && lowered.is_ascii() {
            fuzzy_in(&wanted_bytes, file.path.as_bytes(), file.lower.as_bytes(), file.from)
        } else {
            let shown: Vec<char> = file.path.chars().collect();
            let lower: Vec<char> = file.lower.chars().collect();
            fuzzy_in(&wanted_chars, &shown, &lower, file.from)
        };
        let Some((score, hits)) = matched else { continue };
        let name = file.lower.rsplit('/').next().unwrap_or_default();
        let stem = name.rsplit_once('.').map_or(name, |(stem, _)| stem);
        let named = if name == asked || stem == asked {
            EXACT
        } else if name.starts_with(asked) {
            EXACT / 2.0
        } else {
            0.0
        };
        let lately = if recent.contains(&file.path) { LATELY } else { 0.0 };
        found.push(Found { path: file.path.clone(), score: score + lately + named, hits });
    }
    found.sort_by(|a, b| b.score.total_cmp(&a.score));
    found.truncate(most);
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

/// The most heads a search's reader reads with.
const HEADS: usize = 8;

/// The files read between two looks at whether another head reads faster.
const HEAD_SPAN: usize = 64;

/// How much faster the reads must come since the head spawned last for the reader to spawn another.
const HEAD_GAIN: f64 = 1.1;

/// The files a search's reader reads ahead of each worker.
const READ_AHEAD: usize = 2;

/// The files larger than this a search of a tree git cannot read passes over.
const SEARCHED_BYTES: u64 = 4 * 1024 * 1024;

fn hit(path: String, line: u64, col: u64, text: &str) -> Hit {
    Hit { path, line, col, text: text.trim_end().chars().take(HIT_TEXT).collect() }
}

/// Every line in the tree's files that holds the query, at most HITS_LIMIT of them, less the files
/// the search's patterns hide, and only in the files `set` names where it names any. The files are
/// what git tracks or would track, or the whole tree where git cannot read it. Text is found by the
/// sift: one reader reads the files, with as many heads as read faster, and a worker on each other
/// core orior's work may take searches the next file read; a regular expression is git's to search.
pub fn search(root: &Path, query: &str, how: Searching, set: &Patterns) -> Result<Vec<Hit>, String> {
    if query.is_empty() {
        return Ok(Vec::new());
    }
    let patterns = patterns::of(Part::Search);
    let searched = |path: &str| !patterns.hides(path, false) && !set.hides(path, false);
    let mut specs = set.kept_specs();
    if specs.is_empty() {
        specs = patterns.kept_specs();
    }
    if specs.is_empty() {
        specs.push(".".into());
    }
    specs.extend(patterns.hidden_specs().into_iter().chain(set.hidden_specs()));
    if how.regex {
        if let Some(found) = search_by_git(root, query, how, &specs, &searched) {
            return found;
        }
    }
    // The files: git's view of the tree, kept between lists and searches and let go as files change,
    // or the tree's own files where git lists none of them.
    let (paths, most): (Vec<String>, u64) = match seen(root) {
        Some(view) => (view.files.iter().filter(|path| searched(path)).cloned().collect(), u64::MAX),
        None => (all(root).into_iter().filter(|path| searched(path)).collect(), SEARCHED_BYTES),
    };
    // One reader takes the files from the disk and hands each, mapped or read, to the workers through a
    // queue that holds READ_AHEAD files of each worker's at most; a mapped file's pages are brought in
    // by the worker that searches it. The reader reads with one head, and spawns another each span of
    // HEAD_SPAN files while the head it spawned last raised the files read a millisecond, at most
    // HEADS of them: the disk is asked for no more at once than reading faster takes. The heads and
    // the workers share the cores the reader's work may take (cores.rs), a quarter of them the heads'
    // at most.
    let budget = crate::cores::for_work();
    let heads_most = HEADS.min((budget / 4).max(1));
    let workers = budget.saturating_sub(heads_most).clamp(1, paths.len().max(1));
    let (give, take) = std::sync::mpsc::sync_channel::<(usize, crate::mapped::Mapped)>(workers * READ_AHEAD);
    let take = Mutex::new(take);
    let next = std::sync::atomic::AtomicUsize::new(0);
    let read = std::sync::atomic::AtomicUsize::new(0);
    let mut found: Vec<(usize, Vec<Hit>)> = std::thread::scope(|scope| {
        let paths = &paths;
        let (next, read) = (&next, &read);
        let head = move |give: std::sync::mpsc::SyncSender<(usize, crate::mapped::Mapped)>| loop {
            let index = next.fetch_add(1, std::sync::atomic::Ordering::Relaxed);
            let Some(path) = paths.get(index) else { break };
            let opened = crate::mapped::Mapped::open(&root.join(path));
            read.fetch_add(1, std::sync::atomic::Ordering::Relaxed);
            let Ok(bytes) = opened else { continue };
            if bytes.len() as u64 > most {
                continue;
            }
            if give.send((index, bytes)).is_err() {
                break;
            }
        };
        scope.spawn(move || {
            scope.spawn({
                let give = give.clone();
                move || head(give)
            });
            let mut heads = 1;
            let mut rate = 0.0;
            let mut since = (Instant::now(), 0);
            while heads < heads_most {
                std::thread::sleep(Duration::from_micros(200));
                let done = read.load(std::sync::atomic::Ordering::Relaxed);
                if done >= paths.len() {
                    break;
                }
                if done - since.1 < HEAD_SPAN {
                    continue;
                }
                let now = (done - since.1) as f64 / since.0.elapsed().as_secs_f64().max(1e-9);
                if heads > 1 && now <= rate * HEAD_GAIN {
                    break;
                }
                rate = now;
                since = (Instant::now(), done);
                heads += 1;
                scope.spawn({
                    let give = give.clone();
                    move || head(give)
                });
            }
        });
        let handles: Vec<_> = (0..workers)
            .map(|_| {
                scope.spawn(|| {
                    let mut mine = Vec::new();
                    loop {
                        let next = take.lock().map(|take| take.recv());
                        let Ok(Ok((index, bytes))) = next else { break };
                        if bytes[..bytes.len().min(8192)].contains(&0) {
                            continue;
                        }
                        let hits = hits_in(&paths[index], &bytes, query, how);
                        if !hits.is_empty() {
                            mine.push((index, hits));
                        }
                    }
                    mine
                })
            })
            .collect();
        handles.into_iter().flat_map(|handle| handle.join().unwrap_or_default()).collect()
    });
    found.sort_by_key(|(index, _)| *index);
    Ok(found.into_iter().flat_map(|(_, hits)| hits).take(HITS_LIMIT).collect())
}

/// Whether a character joins a word, for a search of whole words.
fn wordy(c: Option<char>) -> bool {
    c.is_some_and(|c| c.is_alphanumeric() || c == '_')
}

/// The lines of one file that hold the query, the first place on each line it stands, at most
/// HITS_LIMIT of them. Text in ASCII without case, and any text with case, is found by the sift over
/// the file's bytes; text outside ASCII without case is found line by line in its lower case.
fn hits_in(path: &str, bytes: &[u8], query: &str, how: Searching) -> Vec<Hit> {
    let mut found = Vec::new();
    if how.case || query.is_ascii() {
        let mut line = 1u64;
        let mut counted = 0;
        let mut last_line = 0;
        for at in crate::sift::find_all(bytes, query.as_bytes(), !how.case) {
            line += bytes[counted..at].iter().filter(|&&byte| byte == b'\n').count() as u64;
            counted = at;
            if line == last_line {
                continue;
            }
            let start = bytes[..at].iter().rposition(|&byte| byte == b'\n').map_or(0, |newline| newline + 1);
            let end = bytes[at..].iter().position(|&byte| byte == b'\n').map_or(bytes.len(), |newline| at + newline);
            let before = String::from_utf8_lossy(&bytes[start..at]);
            if how.word {
                let after = String::from_utf8_lossy(&bytes[at + query.len()..end]);
                if wordy(before.chars().next_back()) || wordy(after.chars().next()) {
                    continue;
                }
            }
            last_line = line;
            let text = String::from_utf8_lossy(&bytes[start..end]);
            found.push(hit(path.to_string(), line, before.chars().count() as u64 + 1, text.trim_end_matches('\r')));
            if found.len() >= HITS_LIMIT {
                break;
            }
        }
        return found;
    }
    let wanted = query.to_lowercase();
    let text = String::from_utf8_lossy(bytes);
    for (index, line) in text.lines().enumerate() {
        let seen_as = line.to_lowercase();
        let mut from = 0;
        while let Some(at) = seen_as[from..].find(&wanted) {
            let at = from + at;
            if !how.word || (!wordy(seen_as[..at].chars().next_back()) && !wordy(seen_as[at + wanted.len()..].chars().next())) {
                let col = line.get(..at).map_or(0, |before| before.chars().count()) as u64 + 1;
                found.push(hit(path.to_string(), index as u64 + 1, col, line));
                break;
            }
            from = at + wanted.len().max(1);
        }
        if found.len() >= HITS_LIMIT {
            break;
        }
    }
    found
}

/// A regular expression searched by git, over the files `specs` name and `searched` keeps, or nothing
/// where git cannot search the tree, which is then searched for the query as text.
fn search_by_git(root: &Path, query: &str, how: Searching, specs: &[String], searched: &dyn Fn(&str) -> bool) -> Option<Result<Vec<Hit>, String>> {
    let mut git = Command::new("git");
    git.args(crate::trust::git_words(root)).args(["grep", "-n", "--column", "-I", "--no-color", "--untracked", "--full-name"]).current_dir(root);
    if !how.case {
        git.arg("-i");
    }
    if how.word {
        git.arg("-w");
    }
    git.arg("-E");
    git.args(["-e", query, "--"]).args(specs);
    git.stdin(Stdio::null()).stderr(Stdio::piped());
    crate::runner::quiet(&mut git);
    let out = git.output().ok()?;
    // Git says 1 where nothing matched, and more where it could not search.
    match out.status.code() {
        Some(0) => {
            let prefix = Command::new("git").args(["rev-parse", "--show-prefix"]).current_dir(root).output().ok().map(|out| String::from_utf8_lossy(&out.stdout).trim().to_string()).unwrap_or_default();
            Some(Ok(String::from_utf8_lossy(&out.stdout)
                .lines()
                .filter_map(|line| {
                    let mut parts = line.splitn(4, ':');
                    let path = parts.next()?;
                    let number = parts.next()?.parse().ok()?;
                    let col = parts.next()?.parse().ok()?;
                    let path = path.strip_prefix(prefix.as_str()).unwrap_or(path).to_string();
                    searched(&path).then(|| hit(path, number, col, parts.next().unwrap_or_default()))
                })
                .take(HITS_LIMIT)
                .collect()))
        }
        Some(1) => Some(Ok(Vec::new())),
        _ if String::from_utf8_lossy(&out.stderr).contains("regular expression") => Some(Err(String::from_utf8_lossy(&out.stderr).trim().to_string())),
        _ => None,
    }
}

/// The most paths a search returns.
const FOUND_LIMIT: usize = 500;

/// Every listed file whose path holds each word of the query, case aside, at most FOUND_LIMIT of
/// them, less the files the explorer's patterns hide.
pub fn find(root: &Path, query: &str) -> Vec<String> {
    let words: Vec<String> = query.to_lowercase().split_whitespace().map(str::to_string).collect();
    let patterns = patterns::of(Part::Explorer);
    let holds = |path: &str| {
        let lower = path.to_lowercase();
        words.iter().all(|word| lower.contains(word)) && !patterns.hides(path, false)
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
    // A file within an archive is read out of it whole.
    if let Some((_, inner)) = crate::archive::split(root, file)
        && !inner.is_empty()
    {
        let bytes = crate::archive::read(root, file)?;
        let size = bytes.len() as u64;
        let path = file.trim_matches('/').to_string();
        if !bytes[..bytes.len().min(8192)].contains(&0)
            && let Ok(text) = String::from_utf8(bytes.clone())
        {
            return Ok(Opened { path, size, text: Some(text), bytes: None, windowed: false });
        }
        return Ok(Opened { path, size, text: None, bytes: Some(bytes.into_iter().take(SHOWN_BYTES).collect()), windowed: false });
    }
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
    Ok(Slice { start, end, size, line, lines: 0, text: String::from_utf8_lossy(&bytes).into_owned() })
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
    let mut shown = slice_of(&mut handle, start, end.max(start), size, first).map_err(said)?;
    shown.lines = newlines(&mut handle, 0, size).map_err(said)? + 1;
    Ok(shown)
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
    // A file within an archive is saved back into it.
    if let Some((_, inner)) = crate::archive::split(root, file)
        && !inner.is_empty()
    {
        return crate::archive::write(root, file, text.as_bytes());
    }
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

/// A file or folder pasted: where it was, relative to the tree where it was in it, and where it is.
#[derive(Serialize, Debug, PartialEq)]
pub struct Pasted {
    pub from: Option<String>,
    pub to: String,
}

/// Copies or moves each of `from`, a file or a folder and all it holds, into the folder `into` of the
/// tree. A name the folder holds already is numbered, `notes (2).txt` and on, and nothing there is
/// written over. A file moved into the folder it is in stays where it is. A folder is not pasted
/// into itself. A move between drives is a copy and then a removal.
pub fn paste(root: &Path, into: &str, from: &[PathBuf], moving: bool) -> Result<Vec<Pasted>, String> {
    let folder = inside(root, into)?;
    if !folder.is_dir() {
        return Err(format!("{into} is no folder"));
    }
    let mut pasted = Vec::new();
    for source in from {
        let name = source.file_name().ok_or_else(|| format!("{} has no name", source.display()))?;
        let was = source.starts_with(root).then(|| relative(root, source));
        if moving && source.parent() == Some(folder.as_path()) {
            pasted.push(Pasted { from: was.clone(), to: relative(root, source) });
            continue;
        }
        if source.is_dir() && folder.starts_with(source) {
            return Err(format!("{} is not pasted into itself", source.display()));
        }
        let target = free_name(&folder, Path::new(name));
        let said = |e: std::io::Error| format!("{}: {e}", source.display());
        if moving {
            match fs::rename(source, &target) {
                Ok(()) => {}
                Err(e) if e.kind() == std::io::ErrorKind::CrossesDevices => {
                    copy_all(source, &target).map_err(said)?;
                    if source.is_dir() { fs::remove_dir_all(source) } else { fs::remove_file(source) }.map_err(said)?;
                }
                Err(e) => return Err(said(e)),
            }
        } else {
            copy_all(source, &target).map_err(said)?;
        }
        pasted.push(Pasted { from: was, to: relative(root, &target) });
    }
    forget_seen();
    Ok(pasted)
}

/// `name` in `folder`, numbered where the folder holds that name already: `notes.txt`, then
/// `notes (2).txt`, `notes (3).txt` and on.
fn free_name(folder: &Path, name: &Path) -> PathBuf {
    let first = folder.join(name);
    if !first.exists() {
        return first;
    }
    let stem = name.file_stem().unwrap_or(name.as_os_str()).to_string_lossy();
    let end = name.extension().map(|end| format!(".{}", end.to_string_lossy())).unwrap_or_default();
    (2..).map(|n| folder.join(format!("{stem} ({n}){end}"))).find(|one| !one.exists()).unwrap_or(first)
}

/// Copies a file, or a folder and everything in it.
fn copy_all(source: &Path, target: &Path) -> std::io::Result<()> {
    if !source.is_dir() {
        return fs::copy(source, target).map(drop);
    }
    fs::create_dir(target)?;
    for entry in fs::read_dir(source)? {
        let entry = entry?;
        copy_all(&entry.path(), &target.join(entry.file_name()))?;
    }
    Ok(())
}

#[cfg(test)]
mod pasting {
    use super::{Pasted, paste};
    use std::fs;

    #[test]
    fn a_paste_copies_or_moves_and_writes_over_nothing() {
        let root = std::env::temp_dir().join(format!("orior_ui_paste_{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        fs::create_dir_all(root.join("a/inner")).unwrap();
        fs::create_dir_all(root.join("b")).unwrap();
        fs::write(root.join("a/notes.txt"), "one").unwrap();
        fs::write(root.join("a/inner/deep.rs"), "two").unwrap();
        let root = dunce::canonicalize(&root).unwrap();
        let copied = paste(&root, "a", &[root.join("a/notes.txt")], false).unwrap();
        assert_eq!(copied, [Pasted { from: Some("a/notes.txt".into()), to: "a/notes (2).txt".into() }]);
        assert_eq!(fs::read_to_string(root.join("a/notes (2).txt")).unwrap(), "one");
        paste(&root, "b", &[root.join("a")], false).unwrap();
        assert_eq!(fs::read_to_string(root.join("b/a/inner/deep.rs")).unwrap(), "two");
        assert!(paste(&root, "a/inner", &[root.join("a")], false).is_err());
        let moved = paste(&root, "b", &[root.join("a/notes.txt")], true).unwrap();
        assert_eq!(moved[0].to, "b/notes.txt");
        assert!(!root.join("a/notes.txt").exists());
        let stayed = paste(&root, "b", &[root.join("b/notes.txt")], true).unwrap();
        assert_eq!(stayed[0].to, "b/notes.txt");
        let outside = std::env::temp_dir().join(format!("orior_ui_paste_out_{}.txt", std::process::id()));
        fs::write(&outside, "three").unwrap();
        let brought = paste(&root, "", std::slice::from_ref(&outside), true).unwrap();
        assert_eq!(brought[0].from, None);
        assert!(!outside.exists());
        fs::remove_dir_all(&root).unwrap();
    }
}

#[cfg(test)]
mod listing {
    use super::all;
    use std::fs;
    use std::process::Command;

    #[test]
    fn a_tree_in_a_folder_its_repository_ignores_lists_its_files_from_the_disk() {
        let repo = std::env::temp_dir().join(format!("orior_ui_listing_{}", std::process::id()));
        let _ = fs::remove_dir_all(&repo);
        fs::create_dir_all(repo.join("runs/tree/src")).unwrap();
        fs::write(repo.join(".gitignore"), "runs/\n").unwrap();
        fs::write(repo.join("runs/tree/a.js"), "one").unwrap();
        fs::write(repo.join("runs/tree/src/b.js"), "two").unwrap();
        let repo = dunce::canonicalize(&repo).unwrap();
        assert!(Command::new("git").arg("init").arg("-q").current_dir(&repo).status().unwrap().success());
        let mut listed = all(&repo.join("runs/tree"));
        listed.sort();
        assert_eq!(listed, vec!["a.js".to_string(), "src/b.js".to_string()]);
        fs::remove_dir_all(&repo).unwrap();
    }
}

#[cfg(test)]
mod reading {
    use super::{ranked, unread};
    use std::fs;

    #[test]
    fn a_quick_open_answers_from_the_recent_files_until_the_tree_is_read() {
        let root = std::env::temp_dir().join(format!("orior_ui_reading_{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        fs::create_dir_all(&root).unwrap();
        fs::write(root.join("alpha.txt"), "one").unwrap();
        fs::write(root.join("apple.txt"), "two").unwrap();
        let root = dunce::canonicalize(&root).unwrap();
        let first = ranked(&root, "a", &["apple.txt".to_string()], 10);
        assert!(first.reading, "the tree is not read yet");
        assert_eq!(first.found.iter().map(|one| one.path.as_str()).collect::<Vec<_>>(), ["apple.txt"]);
        for _ in 0..100 {
            if !unread(&root) {
                break;
            }
            std::thread::sleep(std::time::Duration::from_millis(50));
        }
        let read = ranked(&root, "alpha", &[], 10);
        assert!(!read.reading, "the tree is read");
        assert_eq!(read.found[0].path, "alpha.txt");
        fs::remove_dir_all(&root).unwrap();
    }
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

#[cfg(test)]
mod matching {
    use super::fuzzy;

    #[test]
    fn a_name_typed_matches_its_letters_in_the_name() {
        let (_, hits) = fuzzy("edit", "src/ui/src/edit.js", 11).unwrap();
        assert_eq!(hits, vec![11, 12, 13, 14]);
        assert!(fuzzy("xyz", "src/ui/src/edit.js", 11).is_none());
        assert_eq!(fuzzy("", "a.rs", 0).unwrap().1, Vec::<usize>::new());
    }

    #[test]
    fn each_score_is_the_palette_s_own() {
        let close = |found: Option<(f64, Vec<usize>)>, score: f64| (found.unwrap().0 - score).abs() < 1e-9;
        assert!(close(fuzzy("view", "src/ui/src/editor/view.js", 18), 42.2));
        assert!(close(fuzzy("edit", "src/ui/src/edit.js", 11), 42.34));
        assert!(fuzzy("view", "src/vendor/item/eventwatch.js", 21).is_none());
    }

    #[test]
    fn a_query_torn_into_too_many_pieces_does_not_match() {
        assert!(fuzzy("abcd", "a/x/b/x/c/x/d", 12).is_none());
    }
}

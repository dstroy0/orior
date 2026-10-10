// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Changes to the tree's files made outside the window, gathered in batches for the window to take:
//! the folders whose entries changed, the files whose text did, and whether git's view did.
//!
//! On Windows the system tells of each change as it is made, through ReadDirectoryChangesW over the
//! whole tree. Elsewhere, and where the system cannot watch the tree, the files git tracks or would
//! track, and the folders holding them, are read every POLL_EVERY and set against the last reading.
//! A batch goes once QUIET passes with no change, or once BATCH_MOST passes from its first.
//!
//! Of git's own folder only its index, HEAD and refs count, and a change to them says git's view
//! changed. A change the watching's patterns hide is left out.

use std::collections::{BTreeSet, HashMap};
use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::{self, Receiver, RecvTimeoutError, Sender};
use std::time::{Duration, Instant, SystemTime};

use serde::Serialize;

use crate::files;
use crate::patterns::{self, Part};

/// How long a batch waits for another change before it goes.
const QUIET: Duration = Duration::from_millis(150);

/// The longest a batch gathers changes for.
const BATCH_MOST: Duration = Duration::from_secs(1);

/// How often the tree is read where the system does not tell of its changes.
const POLL_EVERY: Duration = Duration::from_secs(2);

/// What happened to a path: made or moved there, taken out or moved away, or written. `Lost` says
/// the system's record of changes ran over, and any file may have changed.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Kind {
    Added,
    Removed,
    Modified,
    Lost,
}

/// A batch of changes, each path relative to the tree's top with forward slashes.
#[derive(Serialize, Clone, Debug, Default, PartialEq)]
pub struct Changed {
    /// The folders whose entries changed, the tree's top being "".
    pub folders: Vec<String>,
    /// The files made or written.
    pub files: Vec<String>,
    /// Whether git's view may have changed: its index, HEAD or a ref, or a file it tracks or would.
    pub git: bool,
    /// Whether changes were lost, and every folder and file is to be read again.
    pub lost: bool,
}

impl Changed {
    fn is_empty(&self) -> bool {
        self.folders.is_empty() && self.files.is_empty() && !self.git && !self.lost
    }
}

/// The batch the changes make, less those the watching's patterns hide.
pub fn gather(root: &Path, changes: &[(String, Kind)]) -> Changed {
    let patterns = patterns::of(Part::Watching);
    let mut folders = BTreeSet::new();
    let mut written = BTreeSet::new();
    let mut changed = Changed::default();
    let mut moved = false;
    for (path, kind) in changes {
        if *kind == Kind::Lost {
            changed.lost = true;
            changed.git = true;
            continue;
        }
        if path == ".git" || path.starts_with(".git/") {
            changed.git |= path == ".git/index" || path == ".git/HEAD" || path.starts_with(".git/refs/");
            continue;
        }
        let dir = root.join(path).is_dir();
        if path.is_empty() || patterns.hides(path, dir) {
            continue;
        }
        let tracked = files::tracked(root, path);
        changed.git |= tracked;
        match kind {
            Kind::Added | Kind::Removed => {
                folders.insert(path.rsplit_once('/').map_or("", |(folder, _)| folder).to_string());
                moved |= tracked;
                if *kind == Kind::Added && !dir {
                    written.insert(path.clone());
                }
            }
            Kind::Modified if dir => {
                folders.insert(path.clone());
            }
            _ => {
                written.insert(path.clone());
            }
        }
    }
    if moved || changed.lost {
        files::forget_seen();
    }
    changed.folders = folders.into_iter().collect();
    changed.files = written.into_iter().collect();
    changed
}

/// A watch over a tree, which stops when it is dropped.
pub struct Watcher {
    stop: Arc<AtomicBool>,
    reader: Option<system::Reader>,
}

impl Drop for Watcher {
    fn drop(&mut self) {
        self.stop.store(true, Ordering::Relaxed);
        if let Some(reader) = self.reader.take() {
            reader.stop();
        }
    }
}

/// Starts watching the tree, handing each batch of changes to `take`.
pub fn start(root: PathBuf, take: impl Fn(Changed) + Send + 'static) -> Watcher {
    let stop = Arc::new(AtomicBool::new(false));
    let (send, receive) = mpsc::channel();
    let reader = system::read(&root, &send);
    if reader.is_none() {
        let (root, stop, send) = (root.clone(), stop.clone(), send.clone());
        std::thread::spawn(move || poll(&root, &stop, &send));
    }
    drop(send);
    let held = stop.clone();
    std::thread::spawn(move || batch(&root, &held, &receive, take));
    Watcher { stop, reader }
}

/// Gathers the changes into batches and hands each over, until no reader is left to send them.
fn batch(root: &Path, stop: &AtomicBool, receive: &Receiver<(String, Kind)>, take: impl Fn(Changed)) {
    while let Ok(first) = receive.recv() {
        let began = Instant::now();
        let mut changes = vec![first];
        while let Some(left) = BATCH_MOST.checked_sub(began.elapsed()) {
            match receive.recv_timeout(QUIET.min(left)) {
                Ok(change) => changes.push(change),
                Err(RecvTimeoutError::Timeout | RecvTimeoutError::Disconnected) => break,
            }
        }
        let changed = gather(root, &changes);
        if stop.load(Ordering::Relaxed) {
            return;
        }
        if !changed.is_empty() {
            take(changed);
        }
    }
}

/// Each file git tracks or would track, each folder holding one, and git's index and HEAD, with when
/// each was last written and how long it is.
type Reading = HashMap<String, (Option<SystemTime>, u64)>;

fn reading(root: &Path) -> Reading {
    let mut read = Reading::new();
    let mut note = |path: &str| {
        if let Ok(meta) = root.join(path).metadata() {
            read.insert(path.to_string(), (meta.modified().ok(), if meta.is_dir() { 0 } else { meta.len() }));
        }
    };
    note("");
    note(".git/index");
    note(".git/HEAD");
    let mut folders = BTreeSet::new();
    for file in files::all(root) {
        note(&file);
        let mut at = 0;
        while let Some(slash) = file[at..].find('/') {
            at += slash;
            folders.insert(file[..at].to_string());
            at += 1;
        }
    }
    for folder in &folders {
        note(folder);
    }
    read
}

/// Reads the tree every POLL_EVERY and sends what changed since the last reading, until stopped.
fn poll(root: &Path, stop: &AtomicBool, send: &Sender<(String, Kind)>) {
    let mut last = reading(root);
    loop {
        std::thread::sleep(POLL_EVERY);
        if stop.load(Ordering::Relaxed) {
            return;
        }
        let now = reading(root);
        let mut changes: Vec<(String, Kind)> = now
            .iter()
            .filter_map(|(path, seen)| match last.get(path) {
                None => Some((path.clone(), Kind::Added)),
                Some(before) if before != seen => Some((path.clone(), Kind::Modified)),
                _ => None,
            })
            .collect();
        changes.extend(last.keys().filter(|path| !now.contains_key(*path)).map(|path| (path.clone(), Kind::Removed)));
        if changes.into_iter().any(|change| send.send(change).is_err()) {
            return;
        }
        last = now;
    }
}

#[cfg(windows)]
mod system {
    use super::Kind;
    use std::os::windows::ffi::OsStrExt;
    use std::path::Path;
    use std::sync::mpsc::Sender;
    use std::thread::JoinHandle;

    use windows_sys::Win32::Foundation::{CloseHandle, HANDLE, INVALID_HANDLE_VALUE, WAIT_OBJECT_0};
    use windows_sys::Win32::Storage::FileSystem::{
        CreateFileW, FILE_ACTION_ADDED, FILE_ACTION_REMOVED, FILE_ACTION_RENAMED_NEW_NAME, FILE_ACTION_RENAMED_OLD_NAME, FILE_FLAG_BACKUP_SEMANTICS,
        FILE_FLAG_OVERLAPPED, FILE_LIST_DIRECTORY, FILE_NOTIFY_CHANGE_DIR_NAME, FILE_NOTIFY_CHANGE_FILE_NAME, FILE_NOTIFY_CHANGE_LAST_WRITE,
        FILE_NOTIFY_CHANGE_SIZE, FILE_SHARE_DELETE, FILE_SHARE_READ, FILE_SHARE_WRITE, OPEN_EXISTING, ReadDirectoryChangesW,
    };
    use windows_sys::Win32::System::IO::{CancelIoEx, GetOverlappedResult, OVERLAPPED};
    use windows_sys::Win32::System::Threading::{CreateEventW, INFINITE, ResetEvent, SetEvent, WaitForMultipleObjects};

    /// The buffer the system writes changes into, in u32s: 64 KB, the most a folder shared over a
    /// network takes.
    const BUFFER: usize = 16 * 1024;

    /// Where a record's name starts, after its offset to the next, its action and its name's length.
    const NAME_AT: usize = 12;

    const FILTER: u32 = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE;

    /// The thread that reads the tree's changes, and the event that stops it.
    pub struct Reader {
        thread: JoinHandle<()>,
        stop: isize,
    }

    impl Reader {
        /// Stops the reader and waits for it to let go of the tree's folder.
        pub fn stop(self) {
            // SAFETY: the event is this reader's own, closed once, after the thread that waits on it ends.
            unsafe { SetEvent(self.stop as HANDLE) };
            let _ = self.thread.join();
            unsafe { CloseHandle(self.stop as HANDLE) };
        }
    }

    fn event() -> HANDLE {
        // SAFETY: an event set by hand, not set, with no attributes and no name.
        unsafe { CreateEventW(std::ptr::null(), 1, 0, std::ptr::null()) }
    }

    /// Starts reading the tree's changes as the system tells of them, or None where it cannot watch
    /// the tree. Each read waits on the system's answer and on the stop together.
    pub fn read(root: &Path, send: &Sender<(String, Kind)>) -> Option<Reader> {
        let wide: Vec<u16> = root.as_os_str().encode_wide().chain([0]).collect();
        let share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
        let flags = FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED;
        // SAFETY: the path ends in a nul, and no security attributes and no template are given.
        let folder = unsafe { CreateFileW(wide.as_ptr(), FILE_LIST_DIRECTORY, share, std::ptr::null(), OPEN_EXISTING, flags, std::ptr::null_mut()) };
        if folder == INVALID_HANDLE_VALUE {
            return None;
        }
        let (answered, stop) = (event(), event());
        if answered.is_null() || stop.is_null() {
            // SAFETY: each handle made here is closed once.
            unsafe {
                CloseHandle(folder);
                CloseHandle(answered);
                CloseHandle(stop);
            }
            return None;
        }
        let (folder, answered, stopped) = (folder as isize, answered as isize, stop as isize);
        let send = send.clone();
        let thread = std::thread::spawn(move || {
            let (folder, answered, stopped) = (folder as HANDLE, answered as HANDLE, stopped as HANDLE);
            let mut buffer = vec![0u32; BUFFER];
            // SAFETY: the buffer and the overlapped live until the read they are given to has ended,
            // waited for after a cancel; the handles are this thread's, each closed once.
            unsafe {
                loop {
                    let mut overlapped = OVERLAPPED { hEvent: answered, ..OVERLAPPED::default() };
                    ResetEvent(answered);
                    if ReadDirectoryChangesW(folder, buffer.as_mut_ptr().cast(), (BUFFER * 4) as u32, 1, FILTER, std::ptr::null_mut(), &mut overlapped, None) == 0 {
                        break;
                    }
                    let waited = WaitForMultipleObjects(2, [answered, stopped].as_ptr(), 0, INFINITE);
                    let mut written = 0u32;
                    if waited != WAIT_OBJECT_0 {
                        CancelIoEx(folder, &overlapped);
                        GetOverlappedResult(folder, &overlapped, &mut written, 1);
                        break;
                    }
                    if GetOverlappedResult(folder, &overlapped, &mut written, 0) == 0 {
                        break;
                    }
                    let changes = if written == 0 { vec![(String::new(), Kind::Lost)] } else { parse(&buffer, written as usize) };
                    if changes.into_iter().any(|change| send.send(change).is_err()) {
                        break;
                    }
                }
                CloseHandle(folder);
                CloseHandle(answered);
            }
        });
        Some(Reader { thread, stop: stopped })
    }

    /// The changes the system wrote: each record's path from the tree's top, and what happened to it.
    fn parse(buffer: &[u32], written: usize) -> Vec<(String, Kind)> {
        let word = |at: usize| buffer[at / 4] as usize;
        let mut changes = Vec::new();
        let mut at = 0;
        while at + NAME_AT <= written.min(buffer.len() * 4) {
            let (next, action, length) = (word(at), word(at + 4) as u32, word(at + 8));
            if at + NAME_AT + length > written {
                break;
            }
            // SAFETY: the name lies within what the system wrote, and starts on a two-byte boundary.
            let name = unsafe { std::slice::from_raw_parts(buffer.as_ptr().cast::<u8>().add(at + NAME_AT).cast::<u16>(), length / 2) };
            let kind = match action {
                FILE_ACTION_ADDED | FILE_ACTION_RENAMED_NEW_NAME => Kind::Added,
                FILE_ACTION_REMOVED | FILE_ACTION_RENAMED_OLD_NAME => Kind::Removed,
                _ => Kind::Modified,
            };
            changes.push((String::from_utf16_lossy(name).replace('\\', "/"), kind));
            if next == 0 {
                break;
            }
            at += next;
        }
        changes
    }
}

#[cfg(not(windows))]
mod system {
    use super::Kind;
    use std::path::Path;
    use std::sync::mpsc::Sender;

    pub struct Reader;

    impl Reader {
        pub fn stop(self) {}
    }

    pub fn read(_root: &Path, _send: &Sender<(String, Kind)>) -> Option<Reader> {
        None
    }
}

#[cfg(test)]
mod watching {
    use super::{Changed, Kind, gather, poll, start};
    use std::sync::atomic::AtomicBool;
    use std::sync::mpsc;
    use std::time::Duration;

    fn scratch(name: &str) -> std::path::PathBuf {
        let root = std::env::temp_dir().join(format!("orior_ui_watch_{name}_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&root);
        std::fs::create_dir_all(root.join("src")).unwrap();
        std::fs::write(root.join("src/a.rs"), "one").unwrap();
        dunce::canonicalize(&root).unwrap()
    }

    #[test]
    fn a_batch_says_which_folders_and_files_changed() {
        let root = scratch("gather");
        let changed = gather(
            &root,
            &[
                ("src/b.rs".into(), Kind::Added),
                ("src/a.rs".into(), Kind::Modified),
                ("src".into(), Kind::Modified),
                ("old.txt".into(), Kind::Removed),
                (".git/objects/ab/cdef".into(), Kind::Added),
            ],
        );
        assert_eq!(changed, Changed { folders: vec!["".into(), "src".into()], files: vec!["src/a.rs".into(), "src/b.rs".into()], git: true, lost: false });
        assert_eq!(gather(&root, &[(".git/objects/ab/cdef".into(), Kind::Added)]), Changed::default());
        assert!(gather(&root, &[(".git/index".into(), Kind::Added)]).git);
        assert!(gather(&root, &[(String::new(), Kind::Lost)]).lost);
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn the_system_tells_of_a_file_written_and_one_made() {
        let root = scratch("system");
        let (send, receive) = mpsc::channel();
        let watcher = start(root.clone(), move |changed| {
            let _ = send.send(changed);
        });
        std::thread::sleep(Duration::from_millis(200));
        std::fs::write(root.join("src/a.rs"), "two").unwrap();
        std::fs::write(root.join("src/b.rs"), "three").unwrap();
        let mut files = Vec::new();
        let mut folders = Vec::new();
        while let Ok(changed) = receive.recv_timeout(Duration::from_secs(5)) {
            files.extend(changed.files);
            folders.extend(changed.folders);
            if files.contains(&"src/a.rs".to_string()) && files.contains(&"src/b.rs".to_string()) {
                break;
            }
        }
        drop(watcher);
        assert!(files.contains(&"src/a.rs".to_string()) && files.contains(&"src/b.rs".to_string()), "{files:?}");
        assert!(folders.contains(&"src".to_string()), "{folders:?}");
        std::fs::remove_dir_all(&root).unwrap();
    }

    #[test]
    fn a_reading_of_the_tree_finds_what_changed_since_the_last() {
        let root = scratch("poll");
        let (send, receive) = mpsc::channel();
        let stop = std::sync::Arc::new(AtomicBool::new(false));
        let (at, held) = (root.clone(), stop.clone());
        let reader = std::thread::spawn(move || poll(&at, &held, &send));
        std::thread::sleep(Duration::from_millis(300));
        std::fs::write(root.join("src/a.rs"), "a longer text").unwrap();
        std::fs::write(root.join("src/b.rs"), "three").unwrap();
        let mut seen = Vec::new();
        while let Ok(change) = receive.recv_timeout(Duration::from_secs(4)) {
            seen.push(change);
            if seen.len() >= 3 {
                break;
            }
        }
        stop.store(true, std::sync::atomic::Ordering::Relaxed);
        drop(receive);
        reader.join().unwrap();
        assert!(seen.contains(&("src/a.rs".into(), Kind::Modified)), "{seen:?}");
        assert!(seen.contains(&("src/b.rs".into(), Kind::Added)), "{seen:?}");
        std::fs::remove_dir_all(&root).unwrap();
    }
}

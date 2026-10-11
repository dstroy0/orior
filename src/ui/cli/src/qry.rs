// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Query runs as the window watches them, each a .qry in a folder of runs (src/cu/types/file_defs/qry:
//! qry.h and qry_buffer.h), read one of three ways.
//!
//! A run whose writer runs is read from its buffer in shared memory, mapped to be read alone by the
//! names qry_buffer.c gives it, from the hash of the string every process of the run names the .qry by.
//! A script that starts a run names it by its full path, with `/` between its folders where Git's bash
//! runs it, and the window tries that path so written, then as the system writes it. Its blobs are
//! read from their tags in the order handed, up to the tail, which the writer moves only once a blob is
//! whole, and each read goes on from where the last stopped.
//!
//! A finished run's .qry opens with its table: its rows are held to the head's CRC-64/XZ and each blob
//! read whole to its row's. A .qry whose writer ended before its run did has no table: its tags are
//! walked, a stretch no tag opens stepped over and said, and its blobs put in the order of their
//! sequences.
//!
//! What a blob says is read for a panic or a critical blob, a conference, and a blob of at most
//! TEXT_SMALL bytes, and at most TEXT_MOST bytes of it; of any other blob, its name and its count.

use std::collections::HashMap;
use std::fs::File;
use std::io::{Read as _, Seek, SeekFrom};
use std::path::{Path, PathBuf};
use std::sync::{Mutex, OnceLock};

use serde::Serialize;

const TABLE_MAGIC: &[u8; 8] = b"qry toc\n";
const BUFFER_MAGIC: &[u8; 8] = b"qrybuf1\0";
const NAME_LONGEST: usize = 255;
const BEGIN_LONGEST: usize = NAME_LONGEST + 112;
const TEXT_SMALL: u64 = 4096;
const TEXT_MOST: u64 = 65536;

/// The prefix of a conference's blobs, its question's identity after it.
pub const CONFERENCE: &str = "conference_";

/// The sizes qry_buffer.h builds a run's buffer with where a build sets none, which this reader reads.
const BUFFER_BYTES: u64 = 64 << 30;
const INDEX_SLOTS: u64 = 1 << 22;
const WRITE_BYTES: u64 = 8 << 20;
const WRITE_MILLISECONDS: u64 = 1000;
const CRITICAL_ROOM: u64 = 256 << 20;
const CRITICAL_SPANS: u64 = 1024;
const TABLE_ROWS: u64 = 1 << 24;
const HEAD_BYTES: u64 = 65536;

/// The bytes of the buffer's head as each system lays it out: its words, the lock POSIX gives it, which
/// differs by system and is none on Windows, and its spans.
const HEAD_SIZES: [u64; 4] = [32952, 32992, 33000, 33016];

/// One blob of a run: its name, its place in the order handed, when it was handed, its priority (0 a
/// panic, 1 critical, 2 ordinary), its count of bytes, what it says where that is read, and whether its
/// bytes held to their row's check where the run's table gives one.
#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct Blob {
    pub name: String,
    pub sequence: u64,
    pub nanoseconds: u64,
    pub priority: u32,
    pub size: u64,
    pub text: Option<String>,
    pub whole: Option<bool>,
}

/// A read of a run: whether its writer runs, its blobs from where the read went on, where the next read
/// goes on from, the bytes stepped over that no tag opened, whether its table was read, and the
/// sequence of its first panic, 0 where none.
#[derive(Debug, Clone, Default, Serialize, PartialEq, Eq)]
pub struct Read {
    pub live: bool,
    pub blobs: Vec<Blob>,
    pub next: u64,
    pub stepped: u64,
    pub table: bool,
    pub panicked: u64,
}

/// The 64-bit FNV-1a of `bytes`, 1 where that is 0, as qry.h hashes a name and qry_buffer.c a path.
pub fn hash(bytes: &[u8]) -> u64 {
    let mut hash: u64 = 0xCBF2_9CE4_8422_2325;
    for &byte in bytes {
        hash = (hash ^ byte as u64).wrapping_mul(0x0100_0000_01B3);
    }
    if hash == 0 { 1 } else { hash }
}

/// The CRC-64/XZ of `bytes`.
pub fn crc(bytes: &[u8]) -> u64 {
    static TABLE: OnceLock<[u64; 256]> = OnceLock::new();
    let table = TABLE.get_or_init(|| {
        let mut table = [0u64; 256];
        for (at, entry) in table.iter_mut().enumerate() {
            let mut value = at as u64;
            for _ in 0..8 {
                value = if value & 1 == 1 { (value >> 1) ^ 0xC96C_5795_D787_0F42 } else { value >> 1 };
            }
            *entry = value;
        }
        table
    });
    !bytes.iter().fold(!0u64, |crc, &byte| table[((crc ^ byte as u64) & 0xFF) as usize] ^ (crc >> 8))
}

fn texted(name: &str, priority: u32, size: u64) -> bool {
    priority < 2 || name.starts_with(CONFERENCE) || size <= TEXT_SMALL
}

/// A begin tag read: the name, the count, the sequence, the time and the priority, and the tag's length.
fn begin_tag(line: &[u8]) -> Option<(String, u64, u64, u64, u32, usize)> {
    let end = line.iter().position(|&byte| byte == b'\n')?;
    let words: Vec<&[u8]> = line[..end].split(|&byte| byte == b' ').collect();
    if words.len() != 7 || words[0] != b"qry" || words[6] != b"begin" || words[1].is_empty() || words[1].len() > NAME_LONGEST {
        return None;
    }
    let count = |word: &[u8]| -> Option<u64> { (!word.is_empty() && word.iter().all(u8::is_ascii_digit)).then(|| std::str::from_utf8(word).ok()?.parse().ok())? };
    let name = std::str::from_utf8(words[1]).ok()?.to_string();
    Some((name, count(words[2])?, count(words[3])?, count(words[4])?, u32::try_from(count(words[5])?).ok()?, end + 1))
}

fn end_tag(name: &str) -> Vec<u8> {
    format!("qry {name} end\n").into_bytes()
}

/// The blobs framed in `data` from `at` on, as far as their tags read whole: each blob, with its text
/// where it is read, and where the next blob starts. Where `step` is set, a stretch no tag opens is
/// stepped over to the next blob that reads whole, and its bytes counted.
fn walked(data: &[u8], mut at: usize, step: bool) -> (Vec<Blob>, usize, u64) {
    let mut blobs = Vec::new();
    let mut stepped = 0;
    while at < data.len() {
        let reach = (data.len() - at).min(BEGIN_LONGEST);
        let framed = begin_tag(&data[at..at + reach]).and_then(|(name, size, sequence, nanoseconds, priority, length)| {
            let bytes_at = at + length;
            let end_at = bytes_at.checked_add(usize::try_from(size).ok()?)?;
            let closing = end_tag(&name);
            (data.len() >= end_at + closing.len() && data[end_at..end_at + closing.len()] == closing[..]).then(|| {
                let text = texted(&name, priority, size).then(|| String::from_utf8_lossy(&data[bytes_at..end_at.min(bytes_at + TEXT_MOST as usize)]).into_owned());
                (Blob { name, sequence, nanoseconds, priority, size, text, whole: None }, end_at + closing.len())
            })
        });
        match framed {
            Some((blob, next)) => {
                blobs.push(blob);
                at = next;
            }
            None if step => {
                // On to the next line that opens with a tag, every byte before it stepped over.
                let found = data[at + 1..].windows(5).position(|five| five == b"\nqry ").map(|found| at + 1 + found + 1);
                let to = found.unwrap_or(data.len());
                stepped += (to - at) as u64;
                at = to;
            }
            None => break,
        }
    }
    (blobs, at, stepped)
}

fn word(bytes: &[u8], at: usize) -> u64 {
    u64::from_le_bytes(bytes[at..at + 8].try_into().expect("eight bytes"))
}

/// A .qry file read: its table where it opens with one, its tags walked where it does not.
pub fn read_file(path: &Path) -> Result<Read, String> {
    let said = |error: std::io::Error| format!("{}: {error}", path.display());
    let mut file = File::open(path).map_err(said)?;
    let mut magic = [0u8; 8];
    let opened = file.read(&mut magic).map_err(said)?;
    if opened == 8 && &magic == TABLE_MAGIC {
        return read_table(&mut file).map_err(|error| format!("{}: {error}", path.display()));
    }
    let mut data = Vec::new();
    file.seek(SeekFrom::Start(0)).map_err(said)?;
    file.read_to_end(&mut data).map_err(said)?;
    let (mut blobs, next, stepped) = walked(&data, 0, true);
    blobs.sort_by_key(|blob| blob.sequence);
    let panicked = blobs.iter().find(|blob| blob.priority == 0).map_or(0, |blob| blob.sequence);
    Ok(Read { live: false, blobs, next: next as u64, stepped, table: false, panicked })
}

fn read_table(file: &mut File) -> Result<Read, String> {
    let io = |error: std::io::Error| error.to_string();
    let mut head = [0u8; 64];
    file.seek(SeekFrom::Start(0)).map_err(io)?;
    file.read_exact(&mut head).map_err(io)?;
    let (rows, rows_at, row_bytes, blobs_at, blobs_bytes, rows_crc) = (word(&head, 8), word(&head, 16), word(&head, 24), word(&head, 32), word(&head, 40), word(&head, 48));
    if row_bytes != 64 || rows.checked_mul(64) != Some(rows * 64) {
        return Err("its table's rows are not 64 bytes".into());
    }
    let mut table = vec![0u8; (rows * 64) as usize];
    file.seek(SeekFrom::Start(rows_at)).map_err(io)?;
    file.read_exact(&mut table).map_err(io)?;
    if crc(&table) != rows_crc {
        return Err("its table does not hold to the CRC its head gives".into());
    }
    let mut blobs = Vec::with_capacity(rows as usize);
    for row in table.chunks_exact(64) {
        let (sequence, nanoseconds, row_crc, name_at, bytes_at, size) = (word(row, 8), word(row, 16), word(row, 24), word(row, 32), word(row, 40), word(row, 48));
        let name_length = u32::from_le_bytes(row[56..60].try_into().expect("four bytes")) as usize;
        let priority = u32::from_le_bytes(row[60..64].try_into().expect("four bytes"));
        if bytes_at < blobs_at || bytes_at.saturating_add(size) > blobs_at.saturating_add(blobs_bytes) || name_length > NAME_LONGEST {
            return Err("a row of its table gives a place past its blobs".into());
        }
        let mut name = vec![0u8; name_length];
        file.seek(SeekFrom::Start(name_at)).map_err(io)?;
        file.read_exact(&mut name).map_err(io)?;
        let name = String::from_utf8_lossy(&name).into_owned();
        let (mut text, mut whole) = (None, None);
        if texted(&name, priority, size) {
            let mut bytes = vec![0u8; size.min(TEXT_MOST) as usize];
            file.seek(SeekFrom::Start(bytes_at)).map_err(io)?;
            file.read_exact(&mut bytes).map_err(io)?;
            if size <= TEXT_MOST {
                whole = Some(crc(&bytes) == row_crc);
            }
            text = Some(String::from_utf8_lossy(&bytes).into_owned());
        }
        blobs.push(Blob { name, sequence, nanoseconds, priority, size, text, whole });
    }
    blobs.sort_by_key(|blob| blob.sequence);
    let panicked = blobs.iter().find(|blob| blob.priority == 0).map_or(0, |blob| blob.sequence);
    let next = file.seek(SeekFrom::End(0)).map_err(io)?;
    Ok(Read { live: false, blobs, next, stepped: 0, table: true, panicked })
}

/// The sizes word of a buffer built with the sizes this reader reads, for a head of `head_bytes`.
fn sizes_word(head_bytes: u64) -> u64 {
    let sizes = [BUFFER_BYTES, INDEX_SLOTS, WRITE_BYTES, WRITE_MILLISECONDS, CRITICAL_ROOM, CRITICAL_SPANS, TABLE_ROWS, head_bytes];
    hash(&sizes.iter().flat_map(|size| size.to_le_bytes()).collect::<Vec<u8>>())
}

/// The strings a run's processes may name its .qry by, for the file at `path`: its full path with `/`
/// between its folders, as a script under Git's bash names it, then as the system writes it.
pub fn named(path: &Path) -> Vec<String> {
    let written = path.to_string_lossy().into_owned();
    let slashed = written.replace('\\', "/");
    if slashed == written { vec![written] } else { vec![slashed, written] }
}

/// A run's buffer mapped to be read: the view's first byte and the writer's process.
struct Mapped {
    view: *const u8,
    writer: u64,
    data: u64,
}

// SAFETY: the view is read alone, and is let go only under the map's lock.
unsafe impl Send for Mapped {}

fn mapped() -> &'static Mutex<HashMap<PathBuf, Mapped>> {
    static MAPPED: OnceLock<Mutex<HashMap<PathBuf, Mapped>>> = OnceLock::new();
    MAPPED.get_or_init(Default::default)
}

impl Mapped {
    fn word(&self, at: u64) -> u64 {
        // SAFETY: every word read lies in the head, which the writer gave memory as it made the buffer;
        // a word the writer changes as it runs is read as an atomic word, as its own processes read it.
        unsafe { (*(self.view.add(at as usize) as *const std::sync::atomic::AtomicU64)).load(std::sync::atomic::Ordering::Acquire) }
    }

    fn writing(&self) -> bool {
        // SAFETY: as `word`, a four-byte word of the head.
        let writing = unsafe { (*(self.view.add(156) as *const std::sync::atomic::AtomicU32)).load(std::sync::atomic::Ordering::Acquire) };
        writing != 0 && system::alive(self.writer)
    }
}

/// A run's buffer opened to be read where its writer runs, by the first of the names its .qry may go
/// by whose buffer is one this reader reads.
fn open(path: &Path) -> Option<Mapped> {
    for name in named(path) {
        let Some(view) = system::map(hash(name.as_bytes())) else { continue };
        let found = Mapped { view, writer: 0, data: 0 };
        // SAFETY: the head's first bytes, the magic and its fixed words, lie in memory the writer gave as it
        // made the buffer.
        let magic = unsafe { std::slice::from_raw_parts(view, 8) };
        let sizes = found.word(24);
        if magic != BUFFER_MAGIC || !HEAD_SIZES.iter().any(|&head| sizes_word(head) == sizes) {
            system::unmap(view);
            continue;
        }
        let slots = found.word(16);
        let found = Mapped { view, writer: found.word(96), data: HEAD_BYTES + slots * 16 + TABLE_ROWS * 64 };
        if found.writing() {
            return Some(found);
        }
        system::unmap(view);
    }
    None
}

/// A read of the run at `path`, going on from `from`: from its buffer where its writer runs, the
/// blobs past `from` in the buffer; from its file where none does, every blob.
pub fn read(path: &Path, from: u64) -> Result<Read, String> {
    let mut all = mapped().lock().unwrap();
    if !all.contains_key(path) {
        if let Some(found) = open(path) {
            all.insert(path.to_path_buf(), found);
        }
    }
    if let Some(found) = all.get(path) {
        if found.writing() {
            let tail = found.word(112);
            let from = from.min(tail);
            // SAFETY: every byte before the tail is a blob handed whole, in memory the writer gave before
            // it moved the tail past it.
            let data = unsafe { std::slice::from_raw_parts(found.view.add((found.data + from) as usize), (tail - from) as usize) };
            let (blobs, next, _) = walked(data, 0, false);
            return Ok(Read { live: true, blobs, next: from + next as u64, stepped: 0, table: false, panicked: found.word(144) });
        }
        if let Some(gone) = all.remove(path) {
            system::unmap(gone.view);
        }
    }
    drop(all);
    read_file(path)
}

/// A run found in the tree: its path from the top of the tree, and its file's count of bytes and when
/// it last changed, in milliseconds since 1970.
#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct Found {
    pub path: String,
    pub size: u64,
    pub changed: u64,
}

/// Each .qry under `root`, the folders of version control and of built crates passed over.
pub fn runs(root: &Path) -> Vec<Found> {
    let mut found = Vec::new();
    let mut folders = vec![root.to_path_buf()];
    while let Some(folder) = folders.pop() {
        let Ok(entries) = std::fs::read_dir(&folder) else { continue };
        for entry in entries.flatten() {
            let path = entry.path();
            let Ok(kind) = entry.file_type() else { continue };
            let name = entry.file_name().to_string_lossy().into_owned();
            if kind.is_dir() {
                if !matches!(name.as_str(), ".git" | "node_modules" | "target") {
                    folders.push(path);
                }
            } else if name.ends_with(".qry") {
                let meta = entry.metadata().ok();
                let changed = meta.as_ref().and_then(|meta| meta.modified().ok()).and_then(|at| at.duration_since(std::time::UNIX_EPOCH).ok()).map_or(0, |at| at.as_millis() as u64);
                let relative = path.strip_prefix(root).unwrap_or(&path).to_string_lossy().replace('\\', "/");
                found.push(Found { path: relative, size: meta.map_or(0, |meta| meta.len()), changed });
            }
        }
    }
    found.sort_by(|a, b| b.changed.cmp(&a.changed).then_with(|| a.path.cmp(&b.path)));
    found
}

/// How often the tree is walked for folders of runs; a folder found is read again on every listing,
/// and a run that appears in it shows at once.
const WALK_EVERY: std::time::Duration = std::time::Duration::from_secs(10);

/// A run as the window lists it: its file, and whether its writer runs and the sequence of the first
/// panic its buffer holds, 0 where none or where its writer has ended.
#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct Listed {
    pub path: String,
    pub folder: String,
    pub size: u64,
    pub changed: u64,
    pub live: bool,
    pub panicked: u64,
}

/// A folder of runs and the lessons of its `overseer.txt`.
#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct Folder {
    pub folder: String,
    pub lessons: Vec<Lesson>,
}

/// Every run of the tree at `root`, the newest first, and its folders of runs with their lessons.
#[derive(Debug, Clone, Default, Serialize, PartialEq, Eq)]
pub struct Runs {
    pub runs: Vec<Listed>,
    pub folders: Vec<Folder>,
}

/// Whether the run at `path` is live, and the sequence of its first panic where it is.
fn standing(path: &Path) -> (bool, u64) {
    let mut all = mapped().lock().unwrap();
    if !all.contains_key(path) {
        if let Some(found) = open(path) {
            all.insert(path.to_path_buf(), found);
        }
    }
    match all.get(path) {
        Some(found) if found.writing() => (true, found.word(144)),
        Some(_) => {
            if let Some(gone) = all.remove(path) {
                system::unmap(gone.view);
            }
            (false, 0)
        }
        None => (false, 0),
    }
}

/// The runs of the tree at `root`, from its folders of runs: those a walk of the tree found, walked
/// again at most every WALK_EVERY, each folder read again now.
pub fn listed(root: &Path) -> Runs {
    static WALKED: OnceLock<Mutex<HashMap<PathBuf, (std::time::Instant, Vec<String>)>>> = OnceLock::new();
    let walked = WALKED.get_or_init(Default::default);
    let folders = {
        let mut all = walked.lock().unwrap();
        let fresh = all.get(root).is_some_and(|(at, _)| at.elapsed() < WALK_EVERY);
        if !fresh {
            let mut found: Vec<String> = runs(root).into_iter().map(|run| run.path.rsplit_once('/').map_or(String::new(), |(folder, _)| folder.to_string())).collect();
            found.sort();
            found.dedup();
            all.insert(root.to_path_buf(), (std::time::Instant::now(), found));
        }
        all[root].1.clone()
    };
    let mut listing = Runs::default();
    for folder in folders {
        let full = if folder.is_empty() { root.to_path_buf() } else { root.join(&folder) };
        let Ok(entries) = std::fs::read_dir(&full) else { continue };
        for entry in entries.flatten() {
            let name = entry.file_name().to_string_lossy().into_owned();
            if !name.ends_with(".qry") {
                continue;
            }
            let meta = entry.metadata().ok();
            let changed = meta.as_ref().and_then(|meta| meta.modified().ok()).and_then(|at| at.duration_since(std::time::UNIX_EPOCH).ok()).map_or(0, |at| at.as_millis() as u64);
            let (live, panicked) = standing(&entry.path());
            let path = if folder.is_empty() { name } else { format!("{folder}/{name}") };
            listing.runs.push(Listed { path, folder: folder.clone(), size: meta.map_or(0, |meta| meta.len()), changed, live, panicked });
        }
        let lessons = std::fs::read_to_string(full.join("overseer.txt")).map(|text| lessons(&text)).unwrap_or_default();
        listing.folders.push(Folder { folder, lessons });
    }
    listing.runs.sort_by(|a, b| b.live.cmp(&a.live).then(b.changed.cmp(&a.changed)).then_with(|| a.path.cmp(&b.path)));
    listing
}

/// One lesson of the overseer's: its cause, the carrier it holds for or `*` for any, and its stand-in.
#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct Lesson {
    pub cause: String,
    pub carrier: String,
    pub stand_in: String,
}

/// The lessons in `text`, one `lesson <cause> <carrier or *> <stand-in>` a line; any other line is
/// passed over.
pub fn lessons(text: &str) -> Vec<Lesson> {
    text.lines()
        .filter_map(|line| {
            let mut words = line.split_whitespace();
            (words.next()? == "lesson").then_some(())?;
            let (cause, carrier) = (words.next()?.to_string(), words.next()?.to_string());
            let stand_in = words.collect::<Vec<_>>().join(" ");
            (!stand_in.is_empty()).then_some(Lesson { cause, carrier, stand_in })
        })
        .collect()
}

#[cfg(windows)]
mod system {
    use windows_sys::Win32::Foundation::CloseHandle;
    use windows_sys::Win32::Storage::FileSystem::SYNCHRONIZE;
    use windows_sys::Win32::System::Memory::{MapViewOfFile, OpenFileMappingA, UnmapViewOfFile, FILE_MAP_READ, MEMORY_MAPPED_VIEW_ADDRESS};
    use windows_sys::Win32::System::Threading::{OpenProcess, WaitForSingleObject};

    /// The buffer of the path hashing to `hashed`, its whole view mapped to be read.
    pub fn map(hashed: u64) -> Option<*const u8> {
        let name = format!("Local\\qry-{hashed:016x}\0");
        // SAFETY: the name ends in a nul, and the handle is let go once the view holds the section.
        unsafe {
            let section = OpenFileMappingA(FILE_MAP_READ, 0, name.as_ptr());
            if section.is_null() {
                return None;
            }
            let view = MapViewOfFile(section, FILE_MAP_READ, 0, 0, 0);
            CloseHandle(section);
            (!view.Value.is_null()).then_some(view.Value as *const u8)
        }
    }

    pub fn unmap(view: *const u8) {
        // SAFETY: the view is one `map` gave, let go once.
        unsafe {
            UnmapViewOfFile(MEMORY_MAPPED_VIEW_ADDRESS { Value: view as *mut _ });
        }
    }

    /// Whether the process `id` runs.
    pub fn alive(id: u64) -> bool {
        // SAFETY: the handle is closed once it is waited on.
        unsafe {
            let process = OpenProcess(SYNCHRONIZE, 0, id as u32);
            if process.is_null() {
                return false;
            }
            let running = WaitForSingleObject(process, 0) == 0x102;
            CloseHandle(process);
            running
        }
    }
}

#[cfg(unix)]
mod system {
    use std::ffi::{c_char, c_int, c_void};

    unsafe extern "C" {
        fn shm_open(name: *const c_char, flags: c_int, ...) -> c_int;
        fn mmap(at: *mut c_void, length: usize, protection: c_int, flags: c_int, file: c_int, offset: i64) -> *mut c_void;
        fn munmap(at: *mut c_void, length: usize) -> c_int;
        fn lseek(file: c_int, offset: i64, whence: c_int) -> i64;
        fn close(file: c_int) -> c_int;
        fn kill(id: c_int, signal: c_int) -> c_int;
    }

    const VIEW_BYTES: usize = (super::HEAD_BYTES + super::INDEX_SLOTS * 16 + super::TABLE_ROWS * 64 + super::BUFFER_BYTES) as usize;

    /// The buffer of the path hashing to `hashed`, its whole view mapped to be read.
    pub fn map(hashed: u64) -> Option<*const u8> {
        let name = format!("/qry-{hashed:016x}\0");
        // SAFETY: the name ends in a nul; the view is the object's whole size, read alone, and the file
        // let go once the view holds it.
        unsafe {
            let file = shm_open(name.as_ptr().cast(), 0);
            if file < 0 {
                return None;
            }
            let size = lseek(file, 0, 2);
            let view = if size as usize == VIEW_BYTES { mmap(std::ptr::null_mut(), VIEW_BYTES, 1, 1, file, 0) } else { usize::MAX as *mut c_void };
            close(file);
            (view as usize != usize::MAX).then_some(view as *const u8)
        }
    }

    pub fn unmap(view: *const u8) {
        // SAFETY: the view is one `map` gave, of VIEW_BYTES, let go once.
        unsafe {
            munmap(view as *mut c_void, VIEW_BYTES);
        }
    }

    /// Whether the process `id` runs.
    pub fn alive(id: u64) -> bool {
        // SAFETY: a signal of 0 asks only whether the process is there.
        unsafe { kill(id as c_int, 0) == 0 }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn framed(name: &str, bytes: &[u8], sequence: u64, priority: u32) -> Vec<u8> {
        let mut out = format!("qry {name} {} {sequence} {} {priority} begin\n", bytes.len(), 1_000 + sequence).into_bytes();
        out.extend_from_slice(bytes);
        out.extend(end_tag(name));
        out
    }

    fn written(name: &str, bytes: &[u8]) -> PathBuf {
        let dir = std::env::temp_dir().join(format!("orior-qry-{}-{name}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let path = dir.join(format!("{name}.qry"));
        std::fs::write(&path, bytes).unwrap();
        path
    }

    #[test]
    fn the_crc_and_the_hash_are_qry_h_s() {
        assert_eq!(crc(b"123456789"), 0x995D_C9BB_DF19_39FA, "CRC-64/XZ's check value");
        assert_eq!(hash(b""), 0xCBF2_9CE4_8422_2325);
        assert_eq!(hash(b"a"), 0xAF63_DC4C_8601_EC8C, "FNV-1a's 64-bit value of a");
    }

    #[test]
    fn a_run_its_writer_left_is_read_by_its_tags_in_sequence_a_stretch_no_tag_opens_stepped_over() {
        let mut data = framed("log", b"first\n", 2, 2);
        data.extend(framed("panic", b"the part faulted", 3, 0));
        data.extend(b"half a blob the run never fin\n");
        data.extend(framed("conference_why_it_failed", b"carrier: it hung\noverseer: a stall\n", 1, 2));
        let read = read_file(&written("left", &data)).unwrap();
        assert!(!read.table && !read.live);
        assert_eq!(read.blobs.iter().map(|blob| blob.sequence).collect::<Vec<_>>(), vec![1, 2, 3], "in the order handed");
        assert_eq!(read.panicked, 3);
        assert_eq!(read.stepped, 30);
        assert_eq!(read.blobs[0].text.as_deref(), Some("carrier: it hung\noverseer: a stall\n"));
    }

    #[test]
    fn a_finished_run_is_read_by_its_table_each_blob_held_to_its_row() {
        let blobs: [(&str, &[u8], u32); 3] = [("questions.txt", b"one\ntwo\n", 2), ("panic", b"hung", 0), ("big", &[7u8; 5000], 2)];
        let mut rows = Vec::new();
        let mut body = Vec::new();
        let blobs_at = 64 + 64 * blobs.len() as u64;
        for (index, (name, bytes, priority)) in blobs.iter().enumerate() {
            let tag_at = blobs_at + body.len() as u64;
            let frame = framed(name, bytes, index as u64 + 1, *priority);
            let bytes_at = tag_at + frame.len() as u64 - bytes.len() as u64 - end_tag(name).len() as u64;
            let mut row = Vec::new();
            for value in [hash(name.as_bytes()), index as u64 + 1, 2_000, crc(bytes), tag_at + 4, bytes_at, bytes.len() as u64] {
                row.extend(value.to_le_bytes());
            }
            row.extend((name.len() as u32).to_le_bytes());
            row.extend(priority.to_le_bytes());
            rows.push(row);
            body.extend(frame);
        }
        rows[0][24] ^= 1;
        let table: Vec<u8> = rows.concat();
        let mut file = TABLE_MAGIC.to_vec();
        for value in [blobs.len() as u64, 64, 64, blobs_at, body.len() as u64, crc(&table), 0] {
            file.extend(value.to_le_bytes());
        }
        file.extend(&table);
        file.extend(&body);
        let read = read_file(&written("finished", &file)).unwrap();
        assert!(read.table);
        assert_eq!(read.blobs.iter().map(|blob| blob.name.as_str()).collect::<Vec<_>>(), vec!["questions.txt", "panic", "big"]);
        assert_eq!(read.blobs[0].whole, Some(false), "a blob that does not hold to its row's check is said");
        assert_eq!(read.blobs[1].whole, Some(true));
        assert_eq!((read.blobs[2].text.is_none(), read.blobs[2].size), (true, 5000), "a large ordinary blob is named and counted");
        assert_eq!(read.panicked, 2);
        let mut broken = file.clone();
        broken[64 + 3] ^= 1;
        assert!(read_file(&written("broken", &broken)).unwrap_err().contains("CRC"));
    }

    #[test]
    fn a_run_s_name_is_its_path_with_slashes_first() {
        assert_eq!(named(Path::new("D:\\tree\\build\\query1-x.qry")), vec!["D:/tree/build/query1-x.qry".to_string(), "D:\\tree\\build\\query1-x.qry".to_string()]);
        assert_eq!(named(Path::new("/tree/build/query1-x.qry")), vec!["/tree/build/query1-x.qry".to_string()]);
    }

    /// The run's writer of src/cu built with the system's C compiler, as qry_run.sh builds it; None where
    /// its sources are not beside the window's, as in a container of the window's alone, or no compiler is.
    fn writer() -> Option<PathBuf> {
        let sources = Path::new(env!("CARGO_MANIFEST_DIR")).join("../../cu/types/file_defs/qry");
        if !sources.join("qry_writer.c").exists() {
            return None;
        }
        let out = std::env::temp_dir().join(format!("orior-qry-writer-{}", std::process::id()));
        std::fs::create_dir_all(&out).ok()?;
        let binary = out.join(if cfg!(windows) { "qry_writer.exe" } else { "qry_writer" });
        let built = std::process::Command::new("cc").args(["-std=c11", "-O2", "-o"]).arg(&binary).arg(sources.join("qry_writer.c")).arg(sources.join("qry_buffer.c")).status().ok()?;
        built.success().then_some(binary)
    }

    #[test]
    fn a_live_run_is_read_from_its_buffer_as_its_blobs_are_handed_and_by_its_table_once_it_ends() {
        use std::io::Write as _;
        use std::process::{Command, Stdio};
        let Some(writer) = writer() else {
            eprintln!("the run's writer is not built here: its sources or a C compiler are missing");
            return;
        };
        let dir = std::env::temp_dir().join(format!("orior-qry-live-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let path = dir.join("query1-live.qry");
        let name = named(&path)[0].clone();
        let mut running = Command::new(&writer).args(["start", &name]).stdout(Stdio::null()).spawn().unwrap();
        assert!(Command::new(&writer).args(["ready", &name]).status().unwrap().success(), "the writer made its buffer");
        let hand = |blob: &str, priority: &str, bytes: &[u8]| {
            let mut child = Command::new(&writer).args(["hand", blob, priority]).env("QRY", &name).stdin(Stdio::piped()).spawn().unwrap();
            child.stdin.take().unwrap().write_all(bytes).unwrap();
            assert!(child.wait().unwrap().success(), "{blob} handed");
        };
        hand("questions.txt", "2", b"one\ntwo\n");
        let first = read(&path, 0).unwrap();
        assert!(first.live, "read from its buffer while its writer runs");
        assert_eq!(first.blobs.iter().map(|blob| blob.name.as_str()).collect::<Vec<_>>(), vec!["questions.txt"]);
        hand("conference_why_it_hung", "2", b"carrier: it hung past its bound\noverseer: a stall, the launch held\n");
        hand("panic", "0", b"the part faulted");
        let next = read(&path, first.next).unwrap();
        assert_eq!(next.blobs.iter().map(|blob| blob.name.as_str()).collect::<Vec<_>>(), vec!["conference_why_it_hung", "panic"], "only what was handed since");
        assert_eq!(next.panicked, 3, "the run's first panic");
        assert_eq!(next.blobs[1].text.as_deref(), Some("the part faulted"));
        assert!(Command::new(&writer).args(["stop", &name]).status().unwrap().success(), "the writer stopped");
        running.wait().unwrap();
        let finished = read(&path, next.next).unwrap();
        assert!(!finished.live && finished.table, "read by its table once its writer has ended");
        let names: Vec<&str> = finished.blobs.iter().map(|blob| blob.name.as_str()).collect();
        assert_eq!(&names[..3], ["questions.txt", "conference_why_it_hung", "panic"]);
        assert!(finished.blobs.iter().filter(|blob| blob.text.is_some()).all(|blob| blob.whole == Some(true)), "each blob holds to its row");
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn the_overseer_s_lessons_are_read_a_line_each() {
        let read = lessons("lesson hang carrier_a stall the launch\n# a note\nlesson fault * retry once\nlesson short\n");
        assert_eq!(read, vec![Lesson { cause: "hang".into(), carrier: "carrier_a".into(), stand_in: "stall the launch".into() }, Lesson { cause: "fault".into(), carrier: "*".into(), stand_in: "retry once".into() }]);
    }

    #[test]
    fn runs_are_found_by_their_files_under_the_tree() {
        let root = std::env::temp_dir().join(format!("orior-qry-runs-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&root);
        std::fs::create_dir_all(root.join("build/engine/qry")).unwrap();
        std::fs::create_dir_all(root.join(".git")).unwrap();
        std::fs::write(root.join("build/engine/qry/query1-a.qry"), b"").unwrap();
        std::fs::write(root.join(".git/query9.qry"), b"").unwrap();
        std::fs::write(root.join("build/engine/qry/notes.txt"), b"").unwrap();
        assert_eq!(runs(&root).iter().map(|found| found.path.as_str()).collect::<Vec<_>>(), vec!["build/engine/qry/query1-a.qry"]);
        let _ = std::fs::remove_dir_all(&root);
    }
}

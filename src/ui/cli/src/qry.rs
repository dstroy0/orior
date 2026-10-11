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

/// The places of the words of the buffer's head (qry_buffer.c's QryHead), the same on every system: the
/// lock POSIX keeps in the head stands after them, and the spans after the lock.
mod head {
    pub const CAPACITY: u64 = 8;
    pub const SLOTS: u64 = 16;
    pub const SIZES: u64 = 24;
    pub const COMMITTED: u64 = 32;
    pub const SEQUENCE: u64 = 40;
    pub const DUMPED_BLOBS: u64 = 48;
    pub const DUMPED_BYTES: u64 = 56;
    pub const DUMPED_FIRST: u64 = 64;
    pub const DUMPED_LAST: u64 = 72;
    pub const DUMPED_ALL: u64 = 80;
    pub const INDEX_FULL: u64 = 88;
    pub const WRITER: u64 = 96;
    pub const SPANS: u64 = 104;
    pub const TAIL: u64 = 112;
    pub const WRITTEN: u64 = 120;
    pub const CRITICAL_HANDED: u64 = 128;
    pub const CRITICAL_WRITTEN: u64 = 136;
    pub const PANICKED: u64 = 144;
    pub const WRITING: u64 = 156;
    pub const ROWS: u64 = 160;
    pub const ROWS_COMMITTED: u64 = 168;
    pub const TABLE_FULL: u64 = 176;
    #[cfg(target_os = "linux")]
    pub const LOCK: u64 = 184;
}

/// The priorities a blob is handed at, as qry_buffer.h numbers them.
pub const PANIC: u32 = 0;
pub const CRITICAL: u32 = 1;
pub const ORDINARY: u32 = 2;

/// The memory a hand gives the blobs and the table a step at a time, and how long a critical blob's hand
/// waits for it to be on disk, as qry_buffer.h sets them.
const COMMIT_BYTES: u64 = 64 << 20;
const CRITICAL_WAIT: std::time::Duration = std::time::Duration::from_millis(30000);

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
        let writing = unsafe { (*(self.view.add(head::WRITING as usize) as *const std::sync::atomic::AtomicU32)).load(std::sync::atomic::Ordering::Acquire) };
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
        let sizes = found.word(head::SIZES);
        if magic != BUFFER_MAGIC || !HEAD_SIZES.iter().any(|&head| sizes_word(head) == sizes) {
            system::unmap(view);
            continue;
        }
        let slots = found.word(head::SLOTS);
        let found = Mapped { view, writer: found.word(head::WRITER), data: HEAD_BYTES + slots * 16 + TABLE_ROWS * 64 };
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
            let tail = found.word(head::TAIL);
            let from = from.min(tail);
            // SAFETY: every byte before the tail is a blob handed whole, in memory the writer gave before
            // it moved the tail past it.
            let data = unsafe { std::slice::from_raw_parts(found.view.add((found.data + from) as usize), (tail - from) as usize) };
            let (blobs, next, _) = walked(data, 0, false);
            return Ok(Read { live: true, blobs, next: from + next as u64, stepped: 0, table: false, panicked: found.word(head::PANICKED) });
        }
        if let Some(gone) = all.remove(path) {
            system::unmap(gone.view);
        }
    }
    drop(all);
    read_file(path)
}

/// Each blob framed in `data` from its start, its name, its sequence and its bytes, a stretch no tag
/// opens stepped over to the next line that opens with one, as `walked` steps over it.
fn each_framed<'a>(data: &'a [u8], mut each: impl FnMut(&str, u64, &'a [u8])) {
    let mut at = 0;
    while at < data.len() {
        let reach = (data.len() - at).min(BEGIN_LONGEST);
        let framed = begin_tag(&data[at..at + reach]).and_then(|(name, size, sequence, _, _, length)| {
            let bytes_at = at + length;
            let end_at = usize::try_from(size).ok().and_then(|size| bytes_at.checked_add(size))?;
            let closing = end_tag(&name);
            (data.len() >= end_at + closing.len() && data[end_at..end_at + closing.len()] == closing[..]).then(|| (name, sequence, bytes_at, end_at, end_at + closing.len()))
        });
        match framed {
            Some((name, sequence, bytes_at, end_at, next)) => {
                each(&name, sequence, &data[bytes_at..end_at]);
                at = next;
            }
            None => at = data[at + 1..].windows(5).position(|five| five == b"\nqry ").map_or(data.len(), |found| at + 1 + found + 1),
        }
    }
}

/// The bytes of every blob of the run at `path` named `name`, joined in the order handed, as
/// qry_joined and qry_file_joined join them: from its buffer where its writer runs, from its file
/// where none does.
pub fn joined_bytes(path: &Path, name: &str) -> Result<Vec<u8>, String> {
    let mut parts: Vec<(u64, Vec<u8>)> = Vec::new();
    {
        let mut all = mapped().lock().unwrap();
        if !all.contains_key(path) {
            if let Some(found) = open(path) {
                all.insert(path.to_path_buf(), found);
            }
        }
        if let Some(found) = all.get(path).filter(|found| found.writing()) {
            let tail = found.word(head::TAIL);
            // SAFETY: every byte before the tail is a blob handed whole.
            let data = unsafe { std::slice::from_raw_parts(found.view.add(found.data as usize), tail as usize) };
            each_framed(data, |found, sequence, bytes| {
                if found == name {
                    parts.push((sequence, bytes.to_vec()));
                }
            });
            return Ok(parts.into_iter().flat_map(|(_, bytes)| bytes).collect());
        }
    }
    let data = std::fs::read(path).map_err(|error| format!("{}: {error}", path.display()))?;
    if data.starts_with(TABLE_MAGIC) && data.len() >= 64 {
        let (rows, rows_at) = (word(&data, 8) as usize, word(&data, 16) as usize);
        let table = data.get(rows_at..rows_at + rows * 64).ok_or("its table lies past its end")?;
        for row in table.chunks_exact(64) {
            let (sequence, name_at, bytes_at, size) = (word(row, 8) as usize, word(row, 32) as usize, word(row, 40) as usize, word(row, 48) as usize);
            let length = u32::from_le_bytes(row[56..60].try_into().expect("four bytes")) as usize;
            if data.get(name_at..name_at + length) == Some(name.as_bytes()) {
                parts.push((sequence as u64, data.get(bytes_at..bytes_at + size).ok_or("a row lies past its end")?.to_vec()));
            }
        }
    } else {
        each_framed(&data, |found, sequence, bytes| {
            if found == name {
                parts.push((sequence, bytes.to_vec()));
            }
        });
    }
    parts.sort_by_key(|(sequence, _)| *sequence);
    Ok(parts.into_iter().flat_map(|(_, bytes)| bytes).collect())
}

/// The blobs of the .qry whose bytes are `data` by name, each name's blobs joined in the order handed
/// and the names in the order of their first blobs: from its table where it opens with one, from its
/// tags where it does not, as a run whose writer still runs has none.
pub fn members(data: &[u8]) -> Result<Vec<(String, Vec<u8>)>, String> {
    let mut found: Vec<(String, u64, &[u8])> = Vec::new();
    if data.starts_with(TABLE_MAGIC) && data.len() >= 64 {
        let (rows, rows_at) = (word(data, 8) as usize, word(data, 16) as usize);
        let table = rows.checked_mul(64).and_then(|bytes| data.get(rows_at..rows_at.checked_add(bytes)?)).ok_or("its table lies past its end")?;
        for row in table.chunks_exact(64) {
            let (sequence, name_at, bytes_at, size) = (word(row, 8), word(row, 32) as usize, word(row, 40) as usize, word(row, 48) as usize);
            let length = u32::from_le_bytes(row[56..60].try_into().expect("four bytes")) as usize;
            let name = data.get(name_at..name_at + length).ok_or("a row's name lies past its end")?;
            let bytes = data.get(bytes_at..bytes_at + size).ok_or("a row's bytes lie past its end")?;
            found.push((String::from_utf8_lossy(name).into_owned(), sequence, bytes));
        }
    } else {
        each_framed(data, |name, sequence, bytes| found.push((name.to_string(), sequence, bytes)));
    }
    found.sort_by_key(|(_, sequence, _)| *sequence);
    let mut joined: Vec<(String, Vec<u8>)> = Vec::new();
    let mut place: HashMap<String, usize> = HashMap::new();
    for (name, _, bytes) in found {
        match place.get(&name) {
            Some(&at) => joined[at].1.extend_from_slice(bytes),
            None => {
                place.insert(name.clone(), joined.len());
                joined.push((name, bytes.to_vec()));
            }
        }
    }
    Ok(joined)
}

/// A run joined to hand it blobs, as qry_open opens it: its buffer mapped to be written, the places of
/// its spans, its index, its table and its blobs, and the lock and the wake its processes share.
struct Joined {
    view: *mut u8,
    spans_at: u64,
    slots: u64,
    rows_at: u64,
    data_at: u64,
    writer: u64,
    shared: system::Shared,
}

// SAFETY: a hand changes the buffer only under the run's own lock, which serves the threads of a process
// as it serves its processes; outside it a hand reads only the words its processes read as atomic words.
unsafe impl Send for Joined {}
unsafe impl Sync for Joined {}

impl Drop for Joined {
    fn drop(&mut self) {
        system::leave(self.view, &self.shared);
    }
}

fn joined_runs() -> &'static Mutex<HashMap<PathBuf, std::sync::Arc<Joined>>> {
    static JOINED: OnceLock<Mutex<HashMap<PathBuf, std::sync::Arc<Joined>>>> = OnceLock::new();
    JOINED.get_or_init(Default::default)
}

/// The wall clock's time, nanoseconds since 1970 in UTC, as a hand stamps a blob.
fn nanoseconds() -> u64 {
    std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map_or(0, |at| at.as_nanos() as u64)
}

impl Joined {
    fn atomic(&self, at: u64) -> &std::sync::atomic::AtomicU64 {
        // SAFETY: every word asked for lies in the head, the index or the table, in memory the writer or a
        // hand gave before any process reads it; each is read and written as the run's processes do.
        unsafe { &*(self.view.add(at as usize) as *const std::sync::atomic::AtomicU64) }
    }

    fn get(&self, at: u64) -> u64 {
        self.atomic(at).load(std::sync::atomic::Ordering::SeqCst)
    }

    fn set(&self, at: u64, value: u64) {
        self.atomic(at).store(value, std::sync::atomic::Ordering::SeqCst);
    }

    fn alive(&self) -> bool {
        // SAFETY: a four-byte word of the head.
        let writing = unsafe { (*(self.view.add(head::WRITING as usize) as *const std::sync::atomic::AtomicU32)).load(std::sync::atomic::Ordering::SeqCst) };
        writing != 0 && system::alive(self.writer)
    }

    /// Memory given to the blobs through `need`, COMMIT_BYTES at a time, under the lock.
    fn committed(&self, need: u64) -> bool {
        let committed = self.get(head::COMMITTED);
        if need <= committed {
            return true;
        }
        let to = need.div_ceil(COMMIT_BYTES).saturating_mul(COMMIT_BYTES).min(self.get(head::CAPACITY));
        // SAFETY: the stretch lies in the view's blobs, below the buffer's capacity.
        if !unsafe { system::commit(self.view.add((self.data_at + committed) as usize), (to - committed) as usize) } {
            return false;
        }
        self.set(head::COMMITTED, to);
        true
    }

    /// Memory given to the table through `rows` rows, COMMIT_BYTES at a time, under the lock.
    fn rows_committed(&self, rows: u64) -> bool {
        let need = rows * 64;
        let whole = TABLE_ROWS * 64;
        let committed = self.get(head::ROWS_COMMITTED);
        if rows > TABLE_ROWS {
            return false;
        }
        if need <= committed {
            return true;
        }
        let to = need.div_ceil(COMMIT_BYTES).saturating_mul(COMMIT_BYTES).min(whole);
        // SAFETY: the stretch lies in the view's table.
        if !unsafe { system::commit(self.view.add((self.rows_at + committed) as usize), (to - committed) as usize) } {
            return false;
        }
        self.set(head::ROWS_COMMITTED, to);
        true
    }

    /// The blob whose begin tag stands at `at` in the blobs, before `tail`, read by its tag.
    fn blob_named(&self, at: u64, tail: u64, name: &str) -> bool {
        if at >= tail {
            return false;
        }
        let reach = (tail - at).min(BEGIN_LONGEST as u64) as usize;
        // SAFETY: every byte before the tail is a blob handed whole.
        let line = unsafe { std::slice::from_raw_parts(self.view.add((self.data_at + at) as usize), reach) };
        begin_tag(line).is_some_and(|(found, ..)| found == name)
    }

    /// The blob of `name` at `at` made the latest the index holds of it, under the lock.
    fn index_set(&self, name: &str, at: u64) -> bool {
        let hashed = hash(name.as_bytes());
        let mask = self.slots - 1;
        let tail = self.get(head::TAIL);
        for probe in 0..self.slots {
            let slot = HEAD_BYTES + ((hashed.wrapping_add(probe)) & mask) * 16;
            let held = self.get(slot + 8);
            let same = held != 0 && self.get(slot) == hashed && self.blob_named(held - 1, tail, name);
            if held == 0 || same {
                self.set(slot, hashed);
                self.set(slot + 8, at + 1);
                return true;
            }
        }
        self.set(head::INDEX_FULL, 1);
        false
    }

    /// One blob framed and put at the tail under the lock, as qry_appended puts it: its sequence, and
    /// whether it fit in `room`.
    fn appended(&self, name: &str, bytes: &[u8], priority: u32, room: u64) -> (u64, bool) {
        let sequence = self.get(head::SEQUENCE) + 1;
        self.set(head::SEQUENCE, sequence);
        let handed_at = nanoseconds();
        let begin = format!("qry {name} {} {sequence} {handed_at} {priority} begin\n", bytes.len());
        let end = end_tag(name);
        let tail = self.get(head::TAIL);
        let framed = (begin.len() + bytes.len() + end.len()) as u64;
        if framed > room || tail > room - framed || !self.committed(tail + framed) {
            return (sequence, false);
        }
        // SAFETY: the frame lies past the tail and inside the memory just given; no reader reads past the
        // tail, and no hand writes while this one holds the lock.
        unsafe {
            let at = self.view.add((self.data_at + tail) as usize);
            std::ptr::copy_nonoverlapping(begin.as_ptr(), at, begin.len());
            std::ptr::copy_nonoverlapping(bytes.as_ptr(), at.add(begin.len()), bytes.len());
            std::ptr::copy_nonoverlapping(end.as_ptr(), at.add(begin.len() + bytes.len()), end.len());
        }
        if priority < ORDINARY {
            let spans = self.get(head::SPANS);
            let span = self.spans_at + (spans % CRITICAL_SPANS) * 32;
            for (place, value) in [tail, framed, sequence, priority as u64].into_iter().enumerate() {
                self.set(span + place as u64 * 8, value);
            }
            self.set(head::SPANS, spans + 1);
            self.set(head::CRITICAL_HANDED, sequence);
            if priority == PANIC {
                let _ = self.atomic(head::PANICKED).compare_exchange(0, sequence, std::sync::atomic::Ordering::SeqCst, std::sync::atomic::Ordering::SeqCst);
            }
        }
        // Its row of the table, where the table holds one more; its CRC is the writer's to take.
        let rows = self.get(head::ROWS);
        let rowed = self.get(head::TABLE_FULL) == 0 && self.rows_committed(rows + 1);
        if rowed {
            let row = self.rows_at + rows * 64;
            for (place, value) in [hash(name.as_bytes()), sequence, handed_at, 0, tail + 4, tail + begin.len() as u64, bytes.len() as u64].into_iter().enumerate() {
                self.set(row + place as u64 * 8, value);
            }
            self.set(row + 56, name.len() as u64 | ((priority as u64) << 32));
        } else {
            self.set(head::TABLE_FULL, 1);
        }
        // Published before it is indexed or rowed: a reader that finds it by either finds it whole.
        self.set(head::TAIL, tail + framed);
        if rowed {
            self.set(head::ROWS, rows + 1);
        }
        self.index_set(name, tail);
        (sequence, true)
    }

    /// The blobs dumped since the last said, said in a critical blob `dumped`, under the lock.
    fn dumped_said(&self) {
        let blobs = self.get(head::DUMPED_BLOBS);
        if blobs == 0 {
            return;
        }
        let said = format!("{blobs} blobs, {} bytes, sequences {} to {}\n", self.get(head::DUMPED_BYTES), self.get(head::DUMPED_FIRST), self.get(head::DUMPED_LAST));
        if self.appended("dumped", said.as_bytes(), CRITICAL, self.get(head::CAPACITY)).1 {
            for word in [head::DUMPED_BLOBS, head::DUMPED_BYTES, head::DUMPED_FIRST, head::DUMPED_LAST] {
                self.set(word, 0);
            }
        }
    }

    fn hand(&self, name: &str, bytes: &[u8], priority: u32) -> u64 {
        self.shared.lock(self.view);
        let capacity = self.get(head::CAPACITY);
        let room = if priority < ORDINARY { capacity } else { capacity - CRITICAL_ROOM };
        // A dump said once, before the first blob that goes in after it.
        let framed_most = bytes.len() as u64 + 2 * name.len() as u64 + 128;
        if self.get(head::TAIL) <= room.saturating_sub(framed_most) {
            self.dumped_said();
        }
        let written = self.get(head::WRITTEN);
        let before = self.get(head::TAIL) - written;
        let (sequence, placed) = self.appended(name, bytes, priority, room);
        if !placed && priority == ORDINARY {
            if self.get(head::DUMPED_BLOBS) == 0 {
                self.set(head::DUMPED_FIRST, sequence);
            }
            self.set(head::DUMPED_LAST, sequence);
            self.set(head::DUMPED_BLOBS, self.get(head::DUMPED_BLOBS) + 1);
            self.set(head::DUMPED_BYTES, self.get(head::DUMPED_BYTES) + bytes.len() as u64);
            self.set(head::DUMPED_ALL, self.get(head::DUMPED_ALL) + 1);
        }
        let after = self.get(head::TAIL) - written;
        self.shared.unlock(self.view);
        // The writer woken at once for a panic or a critical blob, and once as the queue crosses
        // WRITE_BYTES, never for each ordinary blob.
        if placed && (priority < ORDINARY || (before < WRITE_BYTES && after >= WRITE_BYTES)) {
            self.shared.wake();
        }
        if !placed {
            return 0;
        }
        if priority == CRITICAL {
            let began = std::time::Instant::now();
            while self.get(head::CRITICAL_WRITTEN) < sequence {
                if !self.alive() || began.elapsed() > CRITICAL_WAIT {
                    return 0;
                }
                std::thread::sleep(std::time::Duration::from_millis(1));
            }
        }
        sequence
    }
}

/// A run's buffer joined to be handed blobs, by the first of the names its .qry may go by whose buffer
/// is one this system builds and whose writer runs.
fn join(path: &Path) -> Option<Joined> {
    for name in named(path) {
        let Some((view, shared)) = system::join(hash(name.as_bytes())) else { continue };
        let leave = |shared: system::Shared| system::leave(view, &shared);
        // SAFETY: the head's first bytes lie in memory the writer gave as it made the buffer.
        let magic = unsafe { std::slice::from_raw_parts(view, 8) };
        let word = |at: u64| unsafe { (*(view.add(at as usize) as *const std::sync::atomic::AtomicU64)).load(std::sync::atomic::Ordering::SeqCst) };
        let Some(head_bytes) = HEAD_SIZES.iter().copied().find(|&head| sizes_word(head) == word(head::SIZES)) else {
            leave(shared);
            continue;
        };
        if magic != BUFFER_MAGIC {
            leave(shared);
            continue;
        }
        let slots = word(head::SLOTS);
        let rows_at = HEAD_BYTES + slots * 16;
        let joined = Joined { view, spans_at: head_bytes - CRITICAL_SPANS * 32, slots, rows_at, data_at: rows_at + TABLE_ROWS * 64, writer: word(head::WRITER), shared };
        if joined.alive() {
            return Some(joined);
        }
    }
    None
}

/// Hands `bytes` to the run at `path` as the blob `name` at `priority`, as qry_hand does: framed at the
/// buffer's tail under the run's lock, its row put in the run's table and its name in its index, and its
/// writer woken for a panic or a critical blob or as the bytes not yet on disk pass WRITE_BYTES; a
/// critical blob's hand returns once it is on disk. The blob's sequence, or 0 where the buffer had no
/// room for it, said in a blob `dumped` once the run has room again.
pub fn hand(path: &Path, name: &str, bytes: &[u8], priority: u32) -> Result<u64, String> {
    if name.is_empty() || name.len() > NAME_LONGEST || name.bytes().any(|byte| byte.is_ascii_whitespace() || byte == 0x0B) {
        return Err(format!("{name:?} is not a name a blob takes: 1 to {NAME_LONGEST} bytes, no space and no line's end"));
    }
    if priority > ORDINARY {
        return Err(format!("{priority} is not a priority: 0 a panic, 1 critical, 2 ordinary"));
    }
    // The map is held only to find the run: a critical blob's hand waits for the disk, and the hands of
    // the other channels go on meanwhile, each under the run's own lock.
    let joined = {
        let mut all = joined_runs().lock().unwrap();
        if all.get(path).is_some_and(|joined| !joined.alive()) {
            all.remove(path);
        }
        if !all.contains_key(path) {
            let joined = join(path).ok_or_else(|| format!("{}: no writer runs for this run", path.display()))?;
            all.insert(path.to_path_buf(), std::sync::Arc::new(joined));
        }
        all[path].clone()
    };
    Ok(joined.hand(name, bytes, priority))
}

/// What the backend's queue has done: the blobs handed, those the run had no room for, those that found
/// no run, and what the last of those said.
#[derive(Debug, Clone, Default, Serialize, PartialEq, Eq)]
pub struct Queued {
    pub handed: u64,
    pub dumped: u64,
    pub failed: u64,
    pub said: String,
}

fn queued_state() -> &'static Mutex<Queued> {
    static QUEUED: OnceLock<Mutex<Queued>> = OnceLock::new();
    QUEUED.get_or_init(Default::default)
}

/// The backend's channels to a run, each with a thread of its own: the primary channel, for the
/// regular traps a party gathers, its traces and the files it hands; the control channel, for what
/// steers a run; and the signal channel, for its panics and critical blobs. A blob on one never waits
/// behind the blobs queued on another.
#[derive(Debug, Clone, Copy, PartialEq, Eq, serde::Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum Channel {
    Primary,
    Control,
    Signal,
}

/// Queues `bytes` to be handed to the run at `path` as the blob `name` at `priority`, on `channel`, a
/// panic or a critical blob on the signal channel whatever channel is named: each party of the window
/// holds what it gathers in its own buffer and queues it here, and goes on at once; the channel's
/// thread hands each to the run's buffer in the order queued, and the run's writer alone writes it to
/// disk. No party waits on the run's lock, its writer or the disk.
pub fn queue(channel: Channel, path: PathBuf, name: String, bytes: Vec<u8>, priority: u32) {
    type Item = (PathBuf, String, Vec<u8>, u32);
    type Sending = OnceLock<Mutex<std::sync::mpsc::Sender<Item>>>;
    static PRIMARY: Sending = OnceLock::new();
    static CONTROL: Sending = OnceLock::new();
    static SIGNAL: Sending = OnceLock::new();
    let opened = |thread: &str| {
        let (sender, taken) = std::sync::mpsc::channel::<Item>();
        std::thread::Builder::new()
            .name(thread.into())
            .spawn(move || {
                for (path, name, bytes, priority) in taken {
                    let done = hand(&path, &name, &bytes, priority);
                    let mut state = queued_state().lock().unwrap();
                    match done {
                        Ok(0) => state.dumped += 1,
                        Ok(_) => state.handed += 1,
                        Err(error) => {
                            state.failed += 1;
                            state.said = error;
                        }
                    }
                }
            })
            .expect("a thread of the backend's channels");
        Mutex::new(sender)
    };
    let sender = match if priority < ORDINARY { Channel::Signal } else { channel } {
        Channel::Primary => PRIMARY.get_or_init(|| opened("qry-primary")),
        Channel::Control => CONTROL.get_or_init(|| opened("qry-control")),
        Channel::Signal => SIGNAL.get_or_init(|| opened("qry-signal")),
    };
    let _ = sender.lock().unwrap().send((path, name, bytes, priority));
}

/// What the backend's queue has done so far.
pub fn queued() -> Queued {
    queued_state().lock().unwrap().clone()
}

/// The run the window's own work is handed to: the one QRY names where the window was started inside
/// a run, as every process of a script reaches its run, or the one the window was last joined to.
pub fn joined() -> Option<PathBuf> {
    joined_slot().lock().unwrap().clone()
}

/// Joins the window to the run at `path`, or to none.
pub fn join_window(path: Option<PathBuf>) {
    *joined_slot().lock().unwrap() = path;
}

fn joined_slot() -> &'static Mutex<Option<PathBuf>> {
    static SLOT: OnceLock<Mutex<Option<PathBuf>>> = OnceLock::new();
    SLOT.get_or_init(|| Mutex::new(std::env::var_os("QRY").filter(|path| !path.is_empty()).map(PathBuf::from)))
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
        Some(found) if found.writing() => (true, found.word(head::PANICKED)),
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
    use windows_sys::Win32::Foundation::{CloseHandle, HANDLE};
    use windows_sys::Win32::Storage::FileSystem::SYNCHRONIZE;
    use windows_sys::Win32::System::Memory::{MapViewOfFile, OpenFileMappingA, UnmapViewOfFile, VirtualAlloc, FILE_MAP_ALL_ACCESS, FILE_MAP_READ, MEMORY_MAPPED_VIEW_ADDRESS, MEM_COMMIT, PAGE_READWRITE};
    use windows_sys::Win32::System::Threading::{OpenEventA, OpenMutexW, OpenProcess, ReleaseMutex, SetEvent, WaitForSingleObject, EVENT_MODIFY_STATE, INFINITE, MUTEX_MODIFY_STATE};

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

    /// The run's lock and wake, the mutex and the event qry_buffer.c names from the hash of its path.
    pub struct Shared {
        lock: HANDLE,
        wake: HANDLE,
    }

    /// The buffer of the path hashing to `hashed`, its whole view mapped to be written, with its lock and
    /// its wake.
    pub fn join(hashed: u64) -> Option<(*mut u8, Shared)> {
        let memory = format!("Local\\qry-{hashed:016x}\0");
        let lock: Vec<u16> = format!("Local\\qry-{hashed:016x}-lock\0").encode_utf16().collect();
        let wake = format!("Local\\qry-{hashed:016x}-wake\0");
        // SAFETY: each name ends in a nul; every handle opened is closed where the join fails, and the
        // section's once the view holds it.
        unsafe {
            let section = OpenFileMappingA(FILE_MAP_ALL_ACCESS, 0, memory.as_ptr());
            if section.is_null() {
                return None;
            }
            let view = MapViewOfFile(section, FILE_MAP_ALL_ACCESS, 0, 0, 0);
            CloseHandle(section);
            if view.Value.is_null() {
                return None;
            }
            let shared = Shared { lock: OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, 0, lock.as_ptr()), wake: OpenEventA(SYNCHRONIZE | EVENT_MODIFY_STATE, 0, wake.as_ptr()) };
            if shared.lock.is_null() || shared.wake.is_null() {
                leave(view.Value as *mut u8, &shared);
                return None;
            }
            Some((view.Value as *mut u8, shared))
        }
    }

    pub fn leave(view: *mut u8, shared: &Shared) {
        // SAFETY: the view is one `join` gave, and each handle one it opened, each let go once.
        unsafe {
            for handle in [shared.lock, shared.wake] {
                if !handle.is_null() {
                    CloseHandle(handle);
                }
            }
            UnmapViewOfFile(MEMORY_MAPPED_VIEW_ADDRESS { Value: view as *mut _ });
        }
    }

    /// Memory given to `bytes` bytes of the view from `at`, which every process's view then reads.
    ///
    /// # Safety
    /// The stretch lies inside the view's section.
    pub unsafe fn commit(at: *mut u8, bytes: usize) -> bool {
        // SAFETY: as the caller says.
        unsafe { !VirtualAlloc(at as *const _, bytes, MEM_COMMIT, PAGE_READWRITE).is_null() }
    }

    impl Shared {
        pub fn lock(&self, _view: *mut u8) {
            // SAFETY: the mutex is the run's; a hand that ended mid-way leaves it abandoned and held as ever.
            unsafe {
                WaitForSingleObject(self.lock, INFINITE);
            }
        }

        pub fn unlock(&self, _view: *mut u8) {
            // SAFETY: the mutex is held by this thread.
            unsafe {
                ReleaseMutex(self.lock);
            }
        }

        pub fn wake(&self) {
            // SAFETY: the event is the run's.
            unsafe {
                SetEvent(self.wake);
            }
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

    #[cfg(target_os = "linux")]
    unsafe extern "C" {
        fn sem_open(name: *const c_char, flags: c_int, ...) -> *mut c_void;
        fn sem_post(semaphore: *mut c_void) -> c_int;
        fn sem_close(semaphore: *mut c_void) -> c_int;
        fn pthread_mutex_lock(mutex: *mut c_void) -> c_int;
        fn pthread_mutex_unlock(mutex: *mut c_void) -> c_int;
        fn pthread_mutex_consistent(mutex: *mut c_void) -> c_int;
    }

    /// What a lock held by a process that ended says on Linux.
    #[cfg(target_os = "linux")]
    const OWNER_DEAD: c_int = 130;

    /// The run's wake, the named semaphore qry_buffer.c names from the hash of its path; its lock is
    /// the robust lock the head holds, shared by every process of the run.
    #[cfg(target_os = "linux")]
    pub struct Shared {
        wake: *mut c_void,
    }

    /// The buffer of the path hashing to `hashed`, its whole view mapped to be written, with its wake.
    #[cfg(target_os = "linux")]
    pub fn join(hashed: u64) -> Option<(*mut u8, Shared)> {
        let name = format!("/qry-{hashed:016x}\0");
        let wake = format!("/qry-{hashed:016x}-wake\0");
        // SAFETY: each name ends in a nul; the view is the object's whole size, and the file let go once
        // the view holds it.
        unsafe {
            let file = shm_open(name.as_ptr().cast(), 2);
            if file < 0 {
                return None;
            }
            let size = lseek(file, 0, 2);
            let view = if size as usize == VIEW_BYTES { mmap(std::ptr::null_mut(), VIEW_BYTES, 3, 1, file, 0) } else { usize::MAX as *mut c_void };
            close(file);
            if view as usize == usize::MAX {
                return None;
            }
            let shared = Shared { wake: sem_open(wake.as_ptr().cast(), 0) };
            if shared.wake.is_null() {
                munmap(view, VIEW_BYTES);
                return None;
            }
            Some((view as *mut u8, shared))
        }
    }

    #[cfg(target_os = "linux")]
    pub fn leave(view: *mut u8, shared: &Shared) {
        // SAFETY: the view and the semaphore are those `join` gave, each let go once.
        unsafe {
            sem_close(shared.wake);
            munmap(view as *mut c_void, VIEW_BYTES);
        }
    }

    /// Memory the system gives a shared object's pages as they are first written.
    ///
    /// # Safety
    /// The stretch lies inside the view.
    #[cfg(target_os = "linux")]
    pub unsafe fn commit(_at: *mut u8, _bytes: usize) -> bool {
        true
    }

    #[cfg(target_os = "linux")]
    impl Shared {
        pub fn lock(&self, view: *mut u8) {
            // SAFETY: the lock lies in the head, made robust and shared between processes by the writer; a
            // hand that ended holding it leaves it to be made consistent by the next.
            unsafe {
                let lock = view.add(super::head::LOCK as usize) as *mut c_void;
                if pthread_mutex_lock(lock) == OWNER_DEAD {
                    pthread_mutex_consistent(lock);
                }
            }
        }

        pub fn unlock(&self, view: *mut u8) {
            // SAFETY: the lock is held by this thread.
            unsafe {
                pthread_mutex_unlock(view.add(super::head::LOCK as usize) as *mut c_void);
            }
        }

        pub fn wake(&self) {
            // SAFETY: the semaphore is the run's.
            unsafe {
                sem_post(self.wake);
            }
        }
    }

    /// A system the run's writer is not built for joins no run.
    #[cfg(not(target_os = "linux"))]
    pub struct Shared;

    #[cfg(not(target_os = "linux"))]
    pub fn join(_hashed: u64) -> Option<(*mut u8, Shared)> {
        None
    }

    #[cfg(not(target_os = "linux"))]
    pub fn leave(_view: *mut u8, _shared: &Shared) {}

    /// # Safety
    /// None is joined here.
    #[cfg(not(target_os = "linux"))]
    pub unsafe fn commit(_at: *mut u8, _bytes: usize) -> bool {
        false
    }

    #[cfg(not(target_os = "linux"))]
    impl Shared {
        pub fn lock(&self, _view: *mut u8) {}
        pub fn unlock(&self, _view: *mut u8) {}
        pub fn wake(&self) {}
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
        let mut more = data.clone();
        more.extend(framed("log", b"second\n", 4, 2));
        let names: Vec<(String, Vec<u8>)> = members(&more).unwrap();
        assert_eq!(names, vec![("conference_why_it_failed".to_string(), b"carrier: it hung\noverseer: a stall\n".to_vec()), ("log".to_string(), b"first\nsecond\n".to_vec()), ("panic".to_string(), b"the part faulted".to_vec())], "by name, each name's blobs joined in the order handed");
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
        let held = members(&file).unwrap();
        assert_eq!(held.iter().map(|(name, bytes)| (name.as_str(), bytes.len())).collect::<Vec<_>>(), vec![("questions.txt", 8), ("panic", 4), ("big", 5000)], "its members read by its table");
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
    /// Built once a process: the tests that start it run at once, and none builds it over another's.
    fn writer() -> Option<PathBuf> {
        static BUILT: OnceLock<Option<PathBuf>> = OnceLock::new();
        BUILT
            .get_or_init(|| {
                let sources = Path::new(env!("CARGO_MANIFEST_DIR")).join("../../cu/types/file_defs/qry");
                if !sources.join("qry_writer.c").exists() {
                    return None;
                }
                let out = std::env::temp_dir().join(format!("orior-qry-writer-{}", std::process::id()));
                std::fs::create_dir_all(&out).ok()?;
                let binary = out.join(if cfg!(windows) { "qry_writer.exe" } else { "qry_writer" });
                let built = std::process::Command::new("cc").args(["-std=c11", "-O2", "-o"]).arg(&binary).arg(sources.join("qry_writer.c")).arg(sources.join("qry_buffer.c")).status().ok()?;
                built.success().then_some(binary)
            })
            .clone()
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
    fn blobs_handed_from_here_go_into_the_run_as_its_writer_s_own_hands_do_and_each_channel_hands_its_own() {
        use std::process::{Command, Stdio};
        let Some(writer) = writer() else {
            eprintln!("the run's writer is not built here: its sources or a C compiler are missing");
            return;
        };
        if cfg!(not(any(windows, target_os = "linux"))) {
            return;
        }
        let dir = std::env::temp_dir().join(format!("orior-qry-hand-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let path = dir.join("query1-hand.qry");
        let name = named(&path)[0].clone();
        let mut running = Command::new(&writer).args(["start", &name]).stdout(Stdio::null()).spawn().unwrap();
        assert!(Command::new(&writer).args(["ready", &name]).status().unwrap().success(), "the writer made its buffer");
        let run = PathBuf::from(&name);
        assert_eq!(hand(&run, "bad name", b"", ORDINARY).unwrap_err().contains("not a name"), true);
        let first = hand(&run, "frames.trace", b"paint 0.000 0.412\n  rows 0.010 0.230\n", ORDINARY).unwrap();
        assert!(first > 0, "an ordinary blob handed");
        let critical = hand(&run, "held.txt", b"on disk before the hand returns\n", CRITICAL).unwrap();
        assert_eq!(critical, first + 1, "the next in the order handed");
        let panic = hand(&run, "panic", b"the part faulted\n", PANIC).unwrap();
        let live = read(&path, 0).unwrap();
        assert!(live.live);
        let names: Vec<&str> = live.blobs.iter().map(|blob| blob.name.as_str()).collect();
        assert_eq!(names, vec!["frames.trace", "held.txt", "panic"]);
        assert_eq!(live.blobs[0].text.as_deref(), Some("paint 0.000 0.412\n  rows 0.010 0.230\n"));
        assert_eq!(live.panicked, panic, "the run's first panic, as its writer's own hands say it");
        let now = nanoseconds();
        assert!(live.blobs.iter().all(|blob| blob.nanoseconds <= now && now - blob.nanoseconds < 60_000_000_000), "each stamped as it was handed");
        // Each channel hands its own, the signal channel's never behind the primary's.
        let before = queued().handed;
        for at in 0..50 {
            queue(Channel::Primary, run.clone(), "bulk.trace".into(), vec![b'x'; 4096 + at], ORDINARY);
        }
        queue(Channel::Control, run.clone(), "control.txt".into(), b"stop at the next rest\n".to_vec(), ORDINARY);
        queue(Channel::Primary, run.clone(), "signal".into(), b"said on the signal channel\n".to_vec(), CRITICAL);
        let until = std::time::Instant::now() + std::time::Duration::from_secs(20);
        while queued().handed < before + 52 && std::time::Instant::now() < until {
            std::thread::sleep(std::time::Duration::from_millis(10));
        }
        assert_eq!(queued().handed - before, 52, "every blob queued was handed: {:?}", queued());
        let bulk: usize = (0..50).map(|at| 4096 + at).sum();
        assert_eq!(joined_bytes(&path, "bulk.trace").unwrap().len(), bulk, "every blob of a name joined, read from the buffer");
        assert!(Command::new(&writer).args(["stop", &name]).status().unwrap().success(), "the writer stopped");
        running.wait().unwrap();
        let finished = read(&path, 0).unwrap();
        assert!(!finished.live && finished.table, "read by its table once its writer has ended");
        let count = |wanted: &str| finished.blobs.iter().filter(|blob| blob.name == wanted).count();
        assert_eq!((count("frames.trace"), count("held.txt"), count("panic"), count("bulk.trace"), count("control.txt"), count("signal")), (1, 1, 1, 50, 1, 1));
        assert!(finished.blobs.iter().filter(|blob| blob.text.is_some()).all(|blob| blob.whole == Some(true)), "each blob holds to its row");
        assert_eq!(joined_bytes(&path, "bulk.trace").unwrap().len(), bulk, "and from its table");
        assert_eq!(joined_bytes(&path, "frames.trace").unwrap(), b"paint 0.000 0.412\n  rows 0.010 0.230\n");
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

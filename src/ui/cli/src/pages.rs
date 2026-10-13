// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Paged texts: files too large for the page to hold, held here as the file on the disk and the lines
//! written over it, the page holding only the lines near its view.
//!
//! A file opened paged is read for an index behind the page: the byte each STEP-th line starts at,
//! and how many lines there are, which grows as the reading goes and is whole once it ends. A line
//! is read from the disk from the nearest line the index holds before it. The lines are split as the
//! page splits a file it reads: at each "\n", a "\r" before it taken off, and a file that ends in
//! "\n" has an empty line last.
//!
//! The text is a list of pieces, each a run of the file's lines or of lines written here, and an
//! edit the page makes takes the lines it reaches out of their pieces and puts its own in their
//! place: the file on the disk is read and never written until the text is saved. A save writes
//! every piece in turn to a file beside the file, moves it over the file, and reads the file afresh.
//! The lines written since the file was read are the changes the gutter marks.

use std::collections::HashMap;
use std::fs::File;
use std::io::{BufRead, BufReader, Read, Seek, SeekFrom, Write};
use std::path::{Path, PathBuf};
use std::sync::{Arc, Mutex, OnceLock};

use serde::Serialize;

use crate::document::{Edit, Sought};

/// How many lines apart the index holds the byte a line starts at.
const STEP: usize = 1024;

/// How many bytes the index reads at a time.
const CHUNK: usize = 1 << 20;

/// How many lines find and a save read at a time.
const BLOCK: usize = 16 * 1024;

/// How many lines the text holds, whether the index is whole, and the file's size.
#[derive(Debug, Clone, Copy, Serialize, PartialEq, Eq)]
pub struct Shape {
    pub lines: usize,
    pub indexed: bool,
    pub size: u64,
}

/// The index of a file's lines as far as it has been read.
#[derive(Default)]
struct Index {
    starts: Vec<u64>,
    lines: usize,
    done: bool,
    failed: Option<String>,
}

/// A run of lines: of the file, from a line to before another, `None` for to the end of the file
/// however many lines the index finds; or lines written here.
#[derive(Clone)]
enum Piece {
    File { from: usize, to: Option<usize> },
    Written(Vec<String>),
}

struct Paged {
    path: PathBuf,
    size: u64,
    eol: &'static str,
    index: Arc<Mutex<Index>>,
    pieces: Vec<Piece>,
}

fn paged() -> &'static Mutex<HashMap<String, Paged>> {
    static PAGED: OnceLock<Mutex<HashMap<String, Paged>>> = OnceLock::new();
    PAGED.get_or_init(|| Mutex::new(HashMap::new()))
}

/// Reads the index of the file at `path` into `index`, a CHUNK at a time.
fn read_index(path: &Path, index: &Mutex<Index>) {
    let file = match File::open(path) {
        Ok(file) => file,
        Err(error) => {
            if let Ok(mut index) = index.lock() {
                index.failed = Some(error.to_string());
                index.done = true;
            }
            return;
        }
    };
    let mut reader = BufReader::with_capacity(CHUNK, file);
    let mut buffer = vec![0u8; CHUNK];
    let mut offset = 0u64;
    let mut lines = 1usize;
    let mut found = Vec::new();
    loop {
        let read = match reader.read(&mut buffer) {
            Ok(0) => break,
            Ok(read) => read,
            Err(error) => {
                if let Ok(mut index) = index.lock() {
                    index.failed = Some(error.to_string());
                }
                break;
            }
        };
        for (at, byte) in buffer[..read].iter().enumerate() {
            if *byte == b'\n' {
                if lines % STEP == 0 {
                    found.push(offset + at as u64 + 1);
                }
                lines += 1;
            }
        }
        offset += read as u64;
        if let Ok(mut index) = index.lock() {
            index.starts.append(&mut found);
            index.lines = lines;
        }
    }
    if let Ok(mut index) = index.lock() {
        index.lines = lines;
        index.done = true;
    }
}

/// The end of line a file's first line ends with: "\r\n" where it does, and "\n" otherwise.
fn eol_of(path: &Path) -> &'static str {
    let mut head = vec![0u8; 64 * 1024];
    let read = File::open(path).and_then(|mut file| file.read(&mut head)).unwrap_or(0);
    match head[..read].iter().position(|byte| *byte == b'\n') {
        Some(at) if at > 0 && head[at - 1] == b'\r' => "\r\n",
        _ => "\n",
    }
}

/// Opens the file at `path` as the paged text `key`, its index read on a thread of its own. Gives
/// its shape as it stands at once.
pub fn open(key: &str, path: &Path) -> Result<Shape, String> {
    open_reading(key, path, true)
}

/// Opens the file at `path` as the paged text `key`, its index read behind where `behind`, and
/// whole before it answers where not.
fn open_reading(key: &str, path: &Path, behind: bool) -> Result<Shape, String> {
    let size = std::fs::metadata(path).map_err(|error| format!("{}: {error}", path.display()))?.len();
    let index = Arc::new(Mutex::new(Index { starts: vec![0], lines: 1, done: false, failed: None }));
    if behind {
        let reading = index.clone();
        let from = path.to_path_buf();
        std::thread::spawn(move || read_index(&from, &reading));
    } else {
        read_index(path, &index);
    }
    let one = Paged { path: path.to_path_buf(), size, eol: eol_of(path), index, pieces: vec![Piece::File { from: 0, to: None }] };
    let shape = shape_of(&one);
    paged().lock().map_err(|_| "held")?.insert(key.to_string(), one);
    Ok(shape)
}

/// How many lines a piece holds, as far as the index has read.
fn length(piece: &Piece, indexed: usize) -> usize {
    match piece {
        Piece::File { from, to } => to.unwrap_or(indexed).saturating_sub(*from),
        Piece::Written(lines) => lines.len(),
    }
}

fn shape_of(one: &Paged) -> Shape {
    let (indexed, done) = one.index.lock().map(|index| (index.lines, index.done)).unwrap_or((1, true));
    Shape { lines: one.pieces.iter().map(|piece| length(piece, indexed)).sum(), indexed: done, size: one.size }
}

pub fn shape(key: &str) -> Result<Shape, String> {
    with(key, |one| Ok(shape_of(one)))
}

fn with<T>(key: &str, work: impl FnOnce(&mut Paged) -> Result<T, String>) -> Result<T, String> {
    let mut all = paged().lock().map_err(|_| "held")?;
    let one = all.get_mut(key).ok_or_else(|| format!("no paged text is held under {key}"))?;
    work(one)
}

/// The file's lines from `from` to before `to`, read from the disk from the nearest line the index
/// holds before `from`.
fn file_lines(one: &Paged, from: usize, to: usize) -> Result<Vec<String>, String> {
    if from >= to {
        return Ok(Vec::new());
    }
    let start = one.index.lock().map_err(|_| "held")?.starts.get(from / STEP).copied().ok_or("the index has not read that far yet")?;
    let mut reader = BufReader::with_capacity(CHUNK.min(4 * 1024 * 1024), File::open(&one.path).map_err(|error| error.to_string())?);
    reader.seek(SeekFrom::Start(start)).map_err(|error| error.to_string())?;
    let mut lines = Vec::with_capacity(to - from);
    let mut bytes = Vec::new();
    for line in (from / STEP) * STEP..to {
        bytes.clear();
        let read = reader.read_until(b'\n', &mut bytes).map_err(|error| error.to_string())?;
        if line < from {
            continue;
        }
        if bytes.last() == Some(&b'\n') {
            bytes.pop();
            if bytes.last() == Some(&b'\r') {
                bytes.pop();
            }
        }
        lines.push(String::from_utf8_lossy(&bytes).into_owned());
        if read == 0 {
            break;
        }
    }
    while lines.len() < to - from {
        lines.push(String::new());
    }
    Ok(lines)
}

/// The text's lines from `from` to before `to`, as far as the text holds them.
fn lines_of(one: &Paged, from: usize, to: usize) -> Result<Vec<String>, String> {
    let indexed = one.index.lock().map_err(|_| "held")?.lines;
    let mut out = Vec::new();
    let mut at = 0;
    for piece in &one.pieces {
        let count = length(piece, indexed);
        let (start, end) = (from.max(at), to.min(at + count));
        if start < end {
            match piece {
                Piece::File { from: first, .. } => out.extend(file_lines(one, first + start - at, first + end - at)?),
                Piece::Written(lines) => out.extend_from_slice(&lines[start - at..end - at]),
            }
        }
        at += count;
        if at >= to {
            break;
        }
    }
    Ok(out)
}

/// The text's lines from `from` to before `to`, once the index has read as far as `to` or the whole
/// file, as far as the text holds them.
pub fn read(key: &str, from: usize, to: usize) -> Result<Vec<String>, String> {
    let index = with(key, |one| Ok(one.index.clone()))?;
    loop {
        let (lines, done) = index.lock().map(|index| (index.lines, index.done)).map_err(|_| "held")?;
        if done || lines >= to {
            break;
        }
        std::thread::sleep(std::time::Duration::from_millis(10));
    }
    with(key, |one| lines_of(one, from, to))
}

/// Puts `lines` in place of the text's lines from `from` to before `to`.
fn replace(one: &mut Paged, from: usize, to: usize, lines: Vec<String>) -> Result<(), String> {
    let indexed = one.index.lock().map_err(|_| "held")?.lines;
    let mut kept = Vec::with_capacity(one.pieces.len() + 2);
    let mut at = 0;
    let mut placed = false;
    for piece in one.pieces.drain(..) {
        let count = length(&piece, indexed);
        let end = at + count;
        if end <= from || at >= to {
            if at >= to && !placed {
                kept.push(Piece::Written(lines.clone()));
                placed = true;
            }
            kept.push(piece);
        } else {
            // The piece's part before `from` stays, then the lines written, then its part from `to`.
            let cut = |piece: &Piece, a: usize, b: usize| match piece {
                Piece::File { from: first, to: last } => Piece::File { from: first + a, to: if b == count && last.is_none() { None } else { Some(first + b) } },
                Piece::Written(lines) => Piece::Written(lines[a..b].to_vec()),
            };
            if from > at {
                kept.push(cut(&piece, 0, from - at));
            }
            if !placed {
                kept.push(Piece::Written(lines.clone()));
                placed = true;
            }
            if to < end {
                kept.push(cut(&piece, to - at, count));
            }
        }
        at = end;
    }
    if !placed {
        kept.push(Piece::Written(lines));
    }
    kept.retain(|piece| !matches!(piece, Piece::Written(lines) if lines.is_empty()));
    one.pieces = kept;
    Ok(())
}

/// The byte a col counted in UTF-16 units falls at in `line`, past its end for a col past it.
fn byte_at(line: &str, col: usize) -> usize {
    let mut units = 0;
    for (at, letter) in line.char_indices() {
        if units >= col {
            return at;
        }
        units += letter.len_utf16();
    }
    line.len()
}

/// Writes the page's changes, each as its document writes it: its edits placed against the text as
/// it stood before any of them, written from the last to the first. Gives the text's shape after.
pub fn edit(key: &str, changes: &[Vec<Edit>]) -> Result<Shape, String> {
    with(key, |one| {
        for change in changes {
            for edit in change.iter().rev() {
                let lines = lines_of(one, edit.from.line, edit.to.line + 1)?;
                if lines.len() != edit.to.line - edit.from.line + 1 {
                    return Err(format!("an edit reaches past the text, to line {}", edit.to.line));
                }
                let first = &lines[0];
                let last = lines.last().ok_or("an edit reaches no line")?;
                let written = format!("{}{}{}", &first[..byte_at(first, edit.from.col)], edit.text, &last[byte_at(last, edit.to.col)..]);
                replace(one, edit.from.line, edit.to.line + 1, written.split('\n').map(str::to_string).collect())?;
            }
        }
        Ok(shape_of(one))
    })
}

/// The lines written since the file was read, each run as [from, to).
pub fn changed(key: &str) -> Result<Vec<[usize; 2]>, String> {
    with(key, |one| {
        let indexed = one.index.lock().map_err(|_| "held")?.lines;
        let mut runs = Vec::new();
        let mut at = 0;
        for piece in &one.pieces {
            let count = length(piece, indexed);
            if matches!(piece, Piece::Written(_)) {
                runs.push([at, at + count]);
            }
            at += count;
        }
        Ok(runs)
    })
}

/// The text's matches for `sought`, each [line, from col, to col], read a BLOCK of lines at a time,
/// until `most` are found.
pub fn find(key: &str, sought: &Sought, most: usize) -> Result<Vec<[usize; 3]>, String> {
    with(key, |one| {
        let total = shape_of(one).lines;
        let mut found = Vec::new();
        let mut from = 0;
        while from < total && found.len() < most {
            let to = (from + BLOCK).min(total);
            let block: Vec<Vec<u16>> = lines_of(one, from, to)?.iter().map(|line| line.encode_utf16().collect()).collect();
            for [line, start, end] in crate::document::find_in(&block, sought, most - found.len())? {
                found.push([from + line, start, end]);
            }
            from = to;
        }
        Ok(found)
    })
}

/// Saves the text to its file: every piece written in turn to a file beside it, which is then moved
/// over it, and the file read afresh, its index whole before the save answers. Gives its shape.
pub fn save(key: &str) -> Result<Shape, String> {
    let mut all = paged().lock().map_err(|_| "held")?;
    let one = all.get_mut(key).ok_or_else(|| format!("no paged text is held under {key}"))?;
    if !one.index.lock().map_err(|_| "held")?.done {
        return Err("the file is still being read: it saves once it is read whole".into());
    }
    let total = shape_of(one).lines;
    let beside = one.path.with_file_name(format!(".{}.orior-saving", one.path.file_name().map(|name| name.to_string_lossy().to_string()).unwrap_or_default()));
    let written = (|| -> Result<(), String> {
        let mut out = std::io::BufWriter::with_capacity(CHUNK, File::create(&beside).map_err(|error| format!("{}: {error}", beside.display()))?);
        let mut from = 0;
        while from < total {
            let to = (from + BLOCK).min(total);
            for (at, line) in lines_of(one, from, to)?.iter().enumerate() {
                if from + at > 0 {
                    out.write_all(one.eol.as_bytes()).map_err(|error| error.to_string())?;
                }
                out.write_all(line.as_bytes()).map_err(|error| error.to_string())?;
            }
            from = to;
        }
        out.flush().map_err(|error| error.to_string())
    })();
    if let Err(said) = written {
        let _ = std::fs::remove_file(&beside);
        return Err(said);
    }
    std::fs::rename(&beside, &one.path).map_err(|error| format!("{}: {error}", one.path.display()))?;
    let path = one.path.clone();
    drop(all);
    open_reading(key, &path, false)
}

pub fn release(key: &str) {
    if let Ok(mut all) = paged().lock() {
        all.remove(key);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::document::Place;

    fn wait_indexed(key: &str) -> Shape {
        for _ in 0..500 {
            let now = shape(key).unwrap();
            if now.indexed {
                return now;
            }
            std::thread::sleep(std::time::Duration::from_millis(10));
        }
        panic!("the index was never whole");
    }

    fn file(name: &str, text: &str) -> PathBuf {
        let dir = std::env::temp_dir().join(format!("orior_pages_{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let path = dir.join(name);
        std::fs::write(&path, text).unwrap();
        path
    }

    #[test]
    fn lines_are_read_as_the_page_splits_them_from_anywhere_in_the_file() {
        let text: String = (0..5000).map(|n| format!("line {n}\r\n")).collect();
        let path = file("read.txt", &text);
        open("read", &path).unwrap();
        let shape = wait_indexed("read");
        assert_eq!(shape.lines, 5001, "a file ending in a newline has an empty line last");
        assert_eq!(read("read", 0, 2).unwrap(), ["line 0", "line 1"]);
        assert_eq!(read("read", 2047, 2050).unwrap(), ["line 2047", "line 2048", "line 2049"]);
        assert_eq!(read("read", 4999, 5001).unwrap(), ["line 4999", ""]);
        release("read");
    }

    #[test]
    fn edits_are_written_over_the_file_s_lines_and_saved_through_a_file_beside_it() {
        let text: String = (0..3000).map(|n| format!("row {n}\n")).collect();
        let path = file("edit.txt", &text);
        open("edit", &path).unwrap();
        wait_indexed("edit");
        let place = |line, col| Place { line, col };
        let shape = edit(
            "edit",
            &[vec![
                Edit { from: place(10, 0), to: place(10, 3), text: "ROW".into() },
                Edit { from: place(2000, 4), to: place(2001, 4), text: "joined\nand ".into() },
            ]],
        )
        .unwrap();
        assert_eq!(shape.lines, 3001);
        assert_eq!(read("edit", 9, 12).unwrap(), ["row 9", "ROW 10", "row 11"]);
        assert_eq!(read("edit", 2000, 2002).unwrap(), ["row joined", "and 2001"]);
        assert_eq!(changed("edit").unwrap(), [[10, 11], [2000, 2002]]);
        edit("edit", &[vec![Edit { from: place(5, 0), to: place(7, 0), text: "".into() }]]).unwrap();
        assert_eq!(read("edit", 4, 7).unwrap(), ["row 4", "row 7", "row 8"], "lines taken out move the rest up");
        let sought = Sought { query: "ROW".into(), regex: false, case: true, word: false, unicode: false };
        assert_eq!(find("edit", &sought, 10).unwrap(), [[8, 0, 3]]);
        let saved = save("edit").unwrap();
        assert_eq!((saved.lines, saved.indexed), (2999, true), "the saved file read afresh");
        let after = std::fs::read_to_string(&path).unwrap();
        assert!(after.starts_with("row 0\nrow 1\nrow 2\nrow 3\nrow 4\nrow 7\nrow 8\nrow 9\nROW 10\n"));
        assert!(after.contains("row joined\nand 2001\n"));
        assert!(after.ends_with("row 2999\n"));
        assert_eq!(changed("edit").unwrap(), Vec::<[usize; 2]>::new(), "a saved text is the file's again");
        release("edit");
    }
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Archives as folders: a zip, and a jar, a wheel or any other zip by another name, a tar, a gzipped
//! tar and a gzipped file each open in the tree as a folder, their files read without unpacking them.
//! A path of the tree runs on into an archive past its file, as `lib/a.zip/docs/read.me`, and on into
//! an archive inside it the same way.
//!
//! A file changed in an archive is saved back into it: the archive is written anew beside the old one,
//! each other file's bytes as they were, the changed one deflated, and put in the old one's place.
//!
//! A zip's names are UTF-8 where its flag says so, and otherwise the old DOS page, 437; each name's
//! own bytes go with it, for the window to read in an encoding the reader chooses. Deflate, CRC-32,
//! zip, tar and gzip are read and written here, with nothing from outside.
//!
//! A query run's .qry (qry.rs) opens as a folder of its blobs, each name's blobs joined in the order
//! handed into one file, a name with slashes in folders of its own, as a run names a file it read by
//! its path. A run's .qry is the record of what the run did and is never written back.

use std::path::{Path, PathBuf};

use serde::Serialize;

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Kind {
    Zip,
    Tar,
    TarGz,
    Gz,
    Qry,
}

/// The kind of archive a file is by its name, or none.
pub fn kind_of(name: &str) -> Option<Kind> {
    let lower = name.to_ascii_lowercase();
    const ZIPS: &[&str] = &[".zip", ".jar", ".war", ".ear", ".whl", ".apk", ".nupkg", ".vsix", ".xpi", ".epub"];
    if ZIPS.iter().any(|end| lower.ends_with(end)) {
        Some(Kind::Zip)
    } else if lower.ends_with(".qry") {
        Some(Kind::Qry)
    } else if lower.ends_with(".tar.gz") || lower.ends_with(".tgz") {
        Some(Kind::TarGz)
    } else if lower.ends_with(".tar") {
        Some(Kind::Tar)
    } else if lower.ends_with(".gz") {
        Some(Kind::Gz)
    } else {
        None
    }
}

// ---- CRC-32 ----

fn crc_table() -> &'static [u32; 256] {
    static TABLE: std::sync::OnceLock<[u32; 256]> = std::sync::OnceLock::new();
    TABLE.get_or_init(|| {
        let mut table = [0u32; 256];
        for (at, slot) in table.iter_mut().enumerate() {
            let mut value = at as u32;
            for _ in 0..8 {
                value = if value & 1 != 0 { 0xEDB8_8320 ^ (value >> 1) } else { value >> 1 };
            }
            *slot = value;
        }
        table
    })
}

pub fn crc32(bytes: &[u8]) -> u32 {
    let table = crc_table();
    !bytes.iter().fold(!0u32, |crc, &byte| table[((crc ^ byte as u32) & 0xff) as usize] ^ (crc >> 8))
}

// ---- Inflate ----

const LENGTH_BASE: [u16; 29] = [3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258];
const LENGTH_EXTRA: [u8; 29] = [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0];
const DISTANCE_BASE: [u16; 30] = [1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577];
const DISTANCE_EXTRA: [u8; 30] = [0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13];

struct Bits<'a> {
    data: &'a [u8],
    at: usize,
    held: u32,
    count: u32,
}

impl Bits<'_> {
    fn take(&mut self, bits: u32) -> Result<u32, String> {
        while self.count < bits {
            let byte = *self.data.get(self.at).ok_or("the deflated data ends early")?;
            self.held |= (byte as u32) << self.count;
            self.at += 1;
            self.count += 8;
        }
        let value = self.held & ((1u32 << bits) - 1);
        self.held >>= bits;
        self.count -= bits;
        Ok(value)
    }

    fn align(&mut self) {
        self.held = 0;
        self.count = 0;
    }
}

/// A canonical Huffman code: how many codes each length has, and the symbols in code order.
struct Huffman {
    counts: [u16; 16],
    symbols: Vec<u16>,
}

impl Huffman {
    fn of(lengths: &[u8]) -> Huffman {
        let mut counts = [0u16; 16];
        for &length in lengths {
            counts[length as usize] += 1;
        }
        counts[0] = 0;
        let mut offsets = [0u16; 16];
        for length in 1..16 {
            offsets[length] = offsets[length - 1] + counts[length - 1];
        }
        let mut symbols = vec![0u16; lengths.len()];
        for (symbol, &length) in lengths.iter().enumerate() {
            if length != 0 {
                symbols[offsets[length as usize] as usize] = symbol as u16;
                offsets[length as usize] += 1;
            }
        }
        Huffman { counts, symbols }
    }

    fn decode(&self, bits: &mut Bits) -> Result<u16, String> {
        let (mut code, mut first, mut index) = (0i32, 0i32, 0i32);
        for length in 1..16 {
            code |= bits.take(1)? as i32;
            let count = self.counts[length] as i32;
            if code - count < first {
                return self.symbols.get((index + code - first) as usize).copied().ok_or_else(|| "a code past the table".to_string());
            }
            index += count;
            first = (first + count) << 1;
            code <<= 1;
        }
        Err("a code no table holds".into())
    }
}

/// The bytes the deflated `data` holds, and how many bytes of `data` it took.
pub fn inflate(data: &[u8]) -> Result<(Vec<u8>, usize), String> {
    let mut bits = Bits { data, at: 0, held: 0, count: 0 };
    let mut out = Vec::with_capacity(data.len() * 3);
    loop {
        let last = bits.take(1)? == 1;
        match bits.take(2)? {
            0 => {
                bits.align();
                let at = bits.at;
                let header = data.get(at..at + 4).ok_or("a stored block's header is cut")?;
                let length = u16::from_le_bytes([header[0], header[1]]) as usize;
                if length as u16 != !u16::from_le_bytes([header[2], header[3]]) {
                    return Err("a stored block's length does not check".into());
                }
                out.extend_from_slice(data.get(at + 4..at + 4 + length).ok_or("a stored block is cut")?);
                bits.at = at + 4 + length;
            }
            1 => {
                let mut lengths = [0u8; 288];
                lengths[..144].fill(8);
                lengths[144..256].fill(9);
                lengths[256..280].fill(7);
                lengths[280..].fill(8);
                codes(&mut bits, &mut out, &Huffman::of(&lengths), &Huffman::of(&[5u8; 30]))?;
            }
            2 => {
                let literals = bits.take(5)? as usize + 257;
                let distances = bits.take(5)? as usize + 1;
                let code_lengths = bits.take(4)? as usize + 4;
                const ORDER: [usize; 19] = [16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15];
                let mut lengths = [0u8; 19];
                for &at in ORDER.iter().take(code_lengths) {
                    lengths[at] = bits.take(3)? as u8;
                }
                let lengths_code = Huffman::of(&lengths);
                let mut all = Vec::with_capacity(literals + distances);
                while all.len() < literals + distances {
                    let symbol = lengths_code.decode(&mut bits)?;
                    let (value, times) = match symbol {
                        0..=15 => (symbol as u8, 1),
                        16 => (*all.last().ok_or("a repeat with nothing before it")?, 3 + bits.take(2)? as usize),
                        17 => (0, 3 + bits.take(3)? as usize),
                        _ => (0, 11 + bits.take(7)? as usize),
                    };
                    all.extend(std::iter::repeat_n(value, times));
                }
                if all.len() != literals + distances {
                    return Err("the code lengths overrun".into());
                }
                codes(&mut bits, &mut out, &Huffman::of(&all[..literals]), &Huffman::of(&all[literals..]))?;
            }
            _ => return Err("a block of a kind deflate has none of".into()),
        }
        if last {
            return Ok((out, bits.at));
        }
    }
}

fn codes(bits: &mut Bits, out: &mut Vec<u8>, literals: &Huffman, distances: &Huffman) -> Result<(), String> {
    loop {
        let symbol = literals.decode(bits)?;
        if symbol < 256 {
            out.push(symbol as u8);
            continue;
        }
        if symbol == 256 {
            return Ok(());
        }
        let at = symbol as usize - 257;
        let length = *LENGTH_BASE.get(at).ok_or("a length past the table")? as usize + bits.take(LENGTH_EXTRA[at] as u32)? as usize;
        let code = distances.decode(bits)? as usize;
        let distance = *DISTANCE_BASE.get(code).ok_or("a distance past the table")? as usize + bits.take(DISTANCE_EXTRA[code] as u32)? as usize;
        if distance > out.len() {
            return Err("a distance back past the start".into());
        }
        let from = out.len() - distance;
        for step in 0..length {
            out.push(out[from + step]);
        }
    }
}

// ---- Deflate ----

struct Writer {
    out: Vec<u8>,
    held: u64,
    count: u32,
}

impl Writer {
    fn put(&mut self, value: u32, bits: u32) {
        self.held |= (value as u64) << self.count;
        self.count += bits;
        while self.count >= 8 {
            self.out.push(self.held as u8);
            self.held >>= 8;
            self.count -= 8;
        }
    }

    /// Puts a Huffman code, its first bit first.
    fn code(&mut self, code: u32, bits: u32) {
        let reversed = code.reverse_bits() >> (32 - bits);
        self.put(reversed, bits);
    }

    fn literal(&mut self, symbol: u32) {
        match symbol {
            0..=143 => self.code(0x30 + symbol, 8),
            144..=255 => self.code(0x190 + symbol - 144, 9),
            256..=279 => self.code(symbol - 256, 7),
            _ => self.code(0xC0 + symbol - 280, 8),
        }
    }

    fn finish(mut self) -> Vec<u8> {
        if self.count > 0 {
            self.out.push(self.held as u8);
        }
        self.out
    }
}

/// `data` deflated, in one block of the fixed codes, each run seen in the 32 KiB before it sent as a
/// length and a distance back.
pub fn deflate(data: &[u8]) -> Vec<u8> {
    const WINDOW: usize = 32 * 1024;
    const CHAIN: usize = 64;
    let mut writer = Writer { out: Vec::with_capacity(data.len() / 2 + 16), held: 0, count: 0 };
    writer.put(1, 1);
    writer.put(1, 2);
    let mut head = vec![usize::MAX; 1 << 15];
    let mut previous = vec![usize::MAX; WINDOW];
    let hash = |at: usize| ((data[at] as usize) << 10 ^ (data[at + 1] as usize) << 5 ^ data[at + 2] as usize) & ((1 << 15) - 1);
    let mut at = 0;
    while at < data.len() {
        let (mut best, mut back) = (0, 0);
        if at + 3 <= data.len() {
            let key = hash(at);
            let mut candidate = head[key];
            let mut tries = 0;
            while candidate != usize::MAX && at - candidate <= WINDOW && tries < CHAIN {
                let most = (data.len() - at).min(258);
                let mut length = 0;
                while length < most && data[candidate + length] == data[at + length] {
                    length += 1;
                }
                if length > best {
                    best = length;
                    back = at - candidate;
                    if length == most {
                        break;
                    }
                }
                candidate = previous[candidate % WINDOW];
                tries += 1;
            }
        }
        let taken = if best >= 3 {
            let code = LENGTH_BASE.iter().rposition(|&base| base as usize <= best).unwrap_or(0);
            writer.literal(257 + code as u32);
            writer.put((best - LENGTH_BASE[code] as usize) as u32, LENGTH_EXTRA[code] as u32);
            let distance = DISTANCE_BASE.iter().rposition(|&base| base as usize <= back).unwrap_or(0);
            writer.code(distance as u32, 5);
            writer.put((back - DISTANCE_BASE[distance] as usize) as u32, DISTANCE_EXTRA[distance] as u32);
            best
        } else {
            writer.literal(data[at] as u32);
            1
        };
        for step in at..at + taken {
            if step + 3 <= data.len() {
                let key = hash(step);
                previous[step % WINDOW] = head[key];
                head[key] = step;
            }
        }
        at += taken;
    }
    writer.literal(256);
    writer.finish()
}

// ---- Names ----

/// The old DOS page's characters for the bytes from 128 up.
const DOS: &str = "ÇüéâäàåçêëèïîìÄÅÉæÆôöòûùÿÖÜ¢£¥₧ƒáíóúñÑªº¿⌐¬½¼¡«»░▒▓│┤╡╢╖╕╣║╗╝╜╛┐└┴┬├─┼╞╟╚╔╩╦╠═╬╧╨╤╥╙╘╒╓╫╪┘┌█▄▌▐▀αßΓπΣσµτΦΘΩδ∞φε∩≡±≥≤⌠⌡÷≈°∙·√ⁿ²■\u{a0}";

/// `bytes` read in the old DOS page, 437.
pub fn dos(bytes: &[u8]) -> String {
    let high: Vec<char> = DOS.chars().collect();
    bytes.iter().map(|&byte| if byte < 128 { byte as char } else { high[byte as usize - 128] }).collect()
}

fn hex(bytes: &[u8]) -> String {
    bytes.iter().map(|byte| format!("{byte:02x}")).collect()
}

// ---- Members ----

/// A file or folder of an archive.
#[derive(Clone, Debug)]
pub struct Member {
    /// Its path in the archive, with slashes, no leading slash.
    pub name: String,
    /// Its name's own bytes, where they are not UTF-8 by the archive's word.
    pub raw: Option<Vec<u8>>,
    pub dir: bool,
    pub size: u64,
    /// Where its record starts: a zip's central record, or a tar's header.
    at: usize,
}

fn clean(name: &str) -> String {
    name.trim_start_matches("./").trim_start_matches('/').trim_end_matches('/').to_string()
}

fn u16_at(bytes: &[u8], at: usize) -> Result<u16, String> {
    bytes.get(at..at + 2).map(|two| u16::from_le_bytes([two[0], two[1]])).ok_or_else(|| "the archive is cut".to_string())
}

fn u32_at(bytes: &[u8], at: usize) -> Result<u32, String> {
    bytes.get(at..at + 4).map(|four| u32::from_le_bytes([four[0], four[1], four[2], four[3]])).ok_or_else(|| "the archive is cut".to_string())
}

fn u64_at(bytes: &[u8], at: usize) -> Result<u64, String> {
    bytes.get(at..at + 8).map(|eight| u64::from_le_bytes(eight.try_into().unwrap_or([0; 8]))).ok_or_else(|| "the archive is cut".to_string())
}

/// A zip's central records: where they start, and how many.
fn zip_directory(bytes: &[u8]) -> Result<(usize, usize), String> {
    let floor = bytes.len().saturating_sub(22 + 65535);
    let end = (floor..=bytes.len().saturating_sub(22)).rev().find(|&at| u32_at(bytes, at).ok() == Some(0x0605_4b50)).ok_or("no zip's directory is at its end")?;
    let (mut count, mut start) = (u16_at(bytes, end + 10)? as usize, u32_at(bytes, end + 16)? as usize);
    // A zip64's directory is named by a locator just before the end record.
    if (count == 0xffff || start == 0xffff_ffff) && end >= 20 && u32_at(bytes, end - 20)? == 0x0706_4b50 {
        let record = u64_at(bytes, end - 12)? as usize;
        if u32_at(bytes, record)? == 0x0606_4b50 {
            count = u64_at(bytes, record + 32)? as usize;
            start = u64_at(bytes, record + 48)? as usize;
        }
    }
    Ok((start, count))
}

/// A zip's central record at `at`: its fields as the record and its zip64 extra give them.
struct Central {
    flags: u16,
    method: u16,
    crc: u32,
    packed: u64,
    size: u64,
    local: u64,
    name: Vec<u8>,
    length: usize,
}

fn central(bytes: &[u8], at: usize) -> Result<Central, String> {
    if u32_at(bytes, at)? != 0x0201_4b50 {
        return Err("a zip's directory record is broken".into());
    }
    let name_length = u16_at(bytes, at + 28)? as usize;
    let extra_length = u16_at(bytes, at + 30)? as usize;
    let comment_length = u16_at(bytes, at + 32)? as usize;
    let name = bytes.get(at + 46..at + 46 + name_length).ok_or("a zip's name is cut")?.to_vec();
    let mut packed = u32_at(bytes, at + 20)? as u64;
    let mut size = u32_at(bytes, at + 24)? as u64;
    let mut local = u32_at(bytes, at + 42)? as u64;
    let extra = bytes.get(at + 46 + name_length..at + 46 + name_length + extra_length).unwrap_or(&[]);
    let mut field = 0;
    while field + 4 <= extra.len() {
        let id = u16_at(extra, field)?;
        let length = u16_at(extra, field + 2)? as usize;
        if id == 1 {
            let mut value = field + 4;
            for slot in [&mut size, &mut packed, &mut local] {
                if *slot == 0xffff_ffff {
                    *slot = u64_at(extra, value)?;
                    value += 8;
                }
            }
        }
        field += 4 + length;
    }
    Ok(Central { flags: u16_at(bytes, at + 8)?, method: u16_at(bytes, at + 10)?, crc: u32_at(bytes, at + 16)?, packed, size, local, name, length: 46 + name_length + extra_length + comment_length })
}

fn zip_members(bytes: &[u8]) -> Result<Vec<Member>, String> {
    let (start, count) = zip_directory(bytes)?;
    let mut at = start;
    let mut members = Vec::with_capacity(count);
    for _ in 0..count {
        let record = central(bytes, at)?;
        let utf8 = record.flags & (1 << 11) != 0;
        let text = if utf8 { String::from_utf8_lossy(&record.name).into_owned() } else { dos(&record.name) };
        let plain = utf8 || record.name.is_ascii();
        members.push(Member { dir: text.ends_with('/'), name: clean(&text.replace('\\', "/")), raw: (!plain).then(|| record.name.clone()), size: record.size, at });
        at += record.length;
    }
    Ok(members)
}

fn zip_data(bytes: &[u8], member: &Member) -> Result<Vec<u8>, String> {
    let record = central(bytes, member.at)?;
    if record.flags & 1 != 0 {
        return Err(format!("{} is encrypted", member.name));
    }
    let local = record.local as usize;
    if u32_at(bytes, local)? != 0x0403_4b50 {
        return Err(format!("{}'s own header is broken", member.name));
    }
    let start = local + 30 + u16_at(bytes, local + 26)? as usize + u16_at(bytes, local + 28)? as usize;
    let packed = bytes.get(start..start + record.packed as usize).ok_or_else(|| format!("{} is cut", member.name))?;
    let data = match record.method {
        0 => packed.to_vec(),
        8 => inflate(packed)?.0,
        other => return Err(format!("{} is packed by method {other}, which orior does not read", member.name)),
    };
    if crc32(&data) != record.crc {
        return Err(format!("{} does not check: its bytes are not those the zip holds", member.name));
    }
    Ok(data)
}

/// A tar member's size, in octal or in base 256 where its first byte's top bit is set.
fn tar_size(field: &[u8]) -> u64 {
    if field.first().is_some_and(|byte| byte & 0x80 != 0) {
        return field[1..].iter().fold(0u64, |size, &byte| size << 8 | byte as u64);
    }
    let text: String = field.iter().take_while(|&&byte| byte != 0 && byte != b' ').map(|&byte| byte as char).collect();
    u64::from_str_radix(text.trim(), 8).unwrap_or(0)
}

fn tar_text(field: &[u8]) -> String {
    let end = field.iter().position(|&byte| byte == 0).unwrap_or(field.len());
    String::from_utf8_lossy(&field[..end]).into_owned()
}

fn tar_members(bytes: &[u8]) -> Result<Vec<Member>, String> {
    let mut members = Vec::new();
    let mut at = 0;
    let mut long_name = None;
    while at + 512 <= bytes.len() {
        let header = &bytes[at..at + 512];
        if header.iter().all(|&byte| byte == 0) {
            break;
        }
        let size = tar_size(&header[124..136]);
        let kind = header[156];
        let data = at + 512;
        let next = data + (size as usize).div_ceil(512) * 512;
        match kind {
            b'L' => long_name = Some(tar_text(bytes.get(data..data + size as usize).ok_or("a tar's long name is cut")?)),
            b'x' => {
                let records = String::from_utf8_lossy(bytes.get(data..data + size as usize).ok_or("a tar's extended header is cut")?).into_owned();
                for record in records.lines() {
                    if let Some(path) = record.split_once(' ').and_then(|(_, rest)| rest.strip_prefix("path=")) {
                        long_name = Some(path.to_string());
                    }
                }
            }
            b'0' | 0 | b'5' | b'7' => {
                let name = long_name.take().unwrap_or_else(|| {
                    let prefix = tar_text(&header[345..500]);
                    let name = tar_text(&header[0..100]);
                    if &header[257..262] == b"ustar" && !prefix.is_empty() { format!("{prefix}/{name}") } else { name }
                });
                members.push(Member { dir: kind == b'5' || name.ends_with('/'), name: clean(&name), raw: None, size, at });
            }
            _ => long_name = None,
        }
        at = next;
    }
    Ok(members)
}

fn tar_data(bytes: &[u8], member: &Member) -> Result<Vec<u8>, String> {
    let start = member.at + 512;
    bytes.get(start..start + member.size as usize).map(<[u8]>::to_vec).ok_or_else(|| format!("{} is cut", member.name))
}

/// What a gzip holds, unzipped.
pub fn gunzip(bytes: &[u8]) -> Result<Vec<u8>, String> {
    if bytes.get(0..3) != Some(&[0x1f, 0x8b, 8]) {
        return Err("not a gzip".into());
    }
    let flags = bytes[3];
    let mut at = 10;
    if flags & 4 != 0 {
        at += 2 + u16_at(bytes, at)? as usize;
    }
    for flag in [8u8, 16] {
        if flags & flag != 0 {
            at += bytes.get(at..).and_then(|rest| rest.iter().position(|&byte| byte == 0)).ok_or("a gzip's header is cut")? + 1;
        }
    }
    if flags & 2 != 0 {
        at += 2;
    }
    let (data, used) = inflate(bytes.get(at..).ok_or("a gzip is cut")?)?;
    let trailer = at + used;
    if u32_at(bytes, trailer)? != crc32(&data) {
        return Err("the gzip does not check".into());
    }
    Ok(data)
}

/// `data` gzipped.
pub fn gzip(data: &[u8]) -> Vec<u8> {
    let mut out = vec![0x1f, 0x8b, 8, 0, 0, 0, 0, 0, 0, 255];
    out.extend(deflate(data));
    out.extend(crc32(data).to_le_bytes());
    out.extend((data.len() as u32).to_le_bytes());
    out
}

/// The members of an archive of `kind` whose bytes are `bytes`; a gzipped file holds one, `inner`.
fn members_of(bytes: &[u8], kind: Kind, inner: &str) -> Result<(Vec<Member>, Vec<u8>), String> {
    match kind {
        Kind::Zip => Ok((zip_members(bytes)?, Vec::new())),
        Kind::Tar => Ok((tar_members(bytes)?, Vec::new())),
        Kind::TarGz => {
            let tar = gunzip(bytes)?;
            Ok((tar_members(&tar)?, tar))
        }
        Kind::Gz => {
            let data = gunzip(bytes)?;
            Ok((vec![Member { name: inner.to_string(), raw: None, dir: false, size: data.len() as u64, at: 0 }], data))
        }
        // Each name's blobs joined, one after another in what the run holds, each member at its place.
        Kind::Qry => {
            let mut joined = Vec::new();
            let mut members = Vec::new();
            for (name, bytes) in crate::qry::members(bytes)? {
                members.push(Member { name: clean(&name), raw: None, dir: false, size: bytes.len() as u64, at: joined.len() });
                joined.extend(bytes);
            }
            Ok((members, joined))
        }
    }
}

/// The name a gzipped file's one member goes by: the file's own, less its .gz.
fn gz_inner(name: &str) -> String {
    let base = name.rsplit('/').next().unwrap_or(name);
    base.strip_suffix(".gz").or_else(|| base.strip_suffix(".GZ")).unwrap_or(base).to_string()
}

fn data_of(bytes: &[u8], kind: Kind, unpacked: &[u8], member: &Member) -> Result<Vec<u8>, String> {
    match kind {
        Kind::Zip => zip_data(bytes, member),
        Kind::Tar => tar_data(bytes, member),
        Kind::TarGz => tar_data(unpacked, member),
        Kind::Gz => Ok(unpacked.to_vec()),
        Kind::Qry => Ok(unpacked[member.at..member.at + member.size as usize].to_vec()),
    }
}

// ---- Writing ----

fn zip_with(bytes: &[u8], target: &Member, data: &[u8]) -> Result<Vec<u8>, String> {
    let (start, count) = zip_directory(bytes)?;
    let mut out = Vec::with_capacity(bytes.len() + data.len());
    let mut directory = Vec::new();
    let mut at = start;
    for _ in 0..count {
        let record = central(bytes, at)?;
        let raw = &bytes[at..at + record.length];
        if record.packed > u32::MAX as u64 || record.size > u32::MAX as u64 || out.len() > u32::MAX as usize {
            return Err("the zip is too large for orior to write back".into());
        }
        let local = record.local as usize;
        let name_length = u16_at(bytes, local + 26)? as usize;
        let extra_length = u16_at(bytes, local + 28)? as usize;
        let local_head = bytes.get(local..local + 30 + name_length + extra_length).ok_or("a zip's own header is cut")?;
        let (method, crc, packed, size, body) = if at == target.at {
            let deflated = deflate(data);
            let (method, body) = if deflated.len() < data.len() { (8u16, deflated) } else { (0u16, data.to_vec()) };
            (method, crc32(data), body.len() as u32, data.len() as u32, body)
        } else {
            let from = local + local_head.len();
            let body = bytes.get(from..from + record.packed as usize).ok_or("a zip's data is cut")?.to_vec();
            (record.method, record.crc, record.packed as u32, record.size as u32, body)
        };
        let offset = out.len() as u32;
        // The local header as it was, its sizes and check set here, and no record after the data.
        let mut head = local_head.to_vec();
        let flags = u16_at(&head, 6)? & !(1 << 3);
        head[6..8].copy_from_slice(&flags.to_le_bytes());
        head[8..10].copy_from_slice(&method.to_le_bytes());
        head[14..18].copy_from_slice(&crc.to_le_bytes());
        head[18..22].copy_from_slice(&packed.to_le_bytes());
        head[22..26].copy_from_slice(&size.to_le_bytes());
        out.extend(&head);
        out.extend(&body);
        let mut record_out = raw.to_vec();
        let central_flags = u16_at(&record_out, 8)? & !(1 << 3);
        record_out[8..10].copy_from_slice(&central_flags.to_le_bytes());
        record_out[10..12].copy_from_slice(&method.to_le_bytes());
        record_out[16..20].copy_from_slice(&crc.to_le_bytes());
        record_out[20..24].copy_from_slice(&packed.to_le_bytes());
        record_out[24..28].copy_from_slice(&size.to_le_bytes());
        record_out[42..46].copy_from_slice(&offset.to_le_bytes());
        directory.extend(record_out);
        at += record.length;
    }
    let directory_at = out.len() as u32;
    out.extend(&directory);
    let end = (bytes.len().saturating_sub(22 + 65535)..=bytes.len() - 22).rev().find(|&at| u32_at(bytes, at).ok() == Some(0x0605_4b50)).ok_or("no zip's directory is at its end")?;
    let mut tail = bytes[end..].to_vec();
    tail[8..10].copy_from_slice(&(count as u16).to_le_bytes());
    tail[10..12].copy_from_slice(&(count as u16).to_le_bytes());
    tail[12..16].copy_from_slice(&(directory.len() as u32).to_le_bytes());
    tail[16..20].copy_from_slice(&directory_at.to_le_bytes());
    out.extend(tail);
    Ok(out)
}

fn tar_with(bytes: &[u8], target: &Member, data: &[u8]) -> Result<Vec<u8>, String> {
    let mut out = Vec::with_capacity(bytes.len() + data.len());
    out.extend(&bytes[..target.at]);
    let mut header = bytes[target.at..target.at + 512].to_vec();
    let size = format!("{:011o}\0", data.len());
    header[124..136].copy_from_slice(size.as_bytes());
    header[148..156].fill(b' ');
    let sum: u32 = header.iter().map(|&byte| byte as u32).sum();
    header[148..156].copy_from_slice(format!("{sum:06o}\0 ").as_bytes());
    out.extend(&header);
    out.extend(data);
    out.resize(out.len().div_ceil(512) * 512, 0);
    let after = target.at + 512 + (target.size as usize).div_ceil(512) * 512;
    out.extend(bytes.get(after..).unwrap_or(&[]));
    Ok(out)
}

// ---- Paths ----

/// The bytes of an archive of `kind` named `name` with the member at `path` set to `data`, a path
/// that runs on into an archive inside it changing that one.
fn replace(bytes: &[u8], kind: Kind, name: &str, path: &[&str], data: &[u8]) -> Result<Vec<u8>, String> {
    if kind == Kind::Qry {
        return Err(format!("{name} is a run's record of what it did, and is not written back"));
    }
    let (members, unpacked) = members_of(bytes, kind, &gz_inner(name))?;
    for cut in (1..=path.len()).rev() {
        let inner = path[..cut].join("/");
        let Some(member) = members.iter().find(|one| !one.dir && one.name == inner) else { continue };
        let new = if cut == path.len() {
            data.to_vec()
        } else {
            let nested = kind_of(&inner).ok_or_else(|| format!("{inner} is a file, and holds nothing"))?;
            replace(&data_of(bytes, kind, &unpacked, member)?, nested, &inner, &path[cut..], data)?
        };
        return match kind {
            Kind::Zip => zip_with(bytes, member, &new),
            Kind::Tar => tar_with(bytes, member, &new),
            Kind::TarGz => Ok(gzip(&tar_with(&unpacked, member, &new)?)),
            Kind::Gz => Ok(gzip(&new)),
            Kind::Qry => unreachable!("a run's .qry is refused above"),
        };
    }
    Err(format!("{name} holds no {}", path.join("/")))
}

/// What an archive, or a folder within one, holds: each child's name, its path within the archive,
/// whether it is a folder or an archive, and its name's own bytes where they are not UTF-8.
#[derive(Serialize, Debug, PartialEq)]
pub struct Child {
    pub name: String,
    pub path: String,
    pub dir: bool,
    pub archive: bool,
    pub raw: Option<String>,
}

/// Where in the archive of `kind` whose bytes are `bytes` the path `path` leads: the bytes of the
/// innermost archive it runs into, that archive's kind and name, and the path left within it.
fn descend(bytes: Vec<u8>, kind: Kind, name: String, path: Vec<String>) -> Result<(Vec<u8>, Kind, String, Vec<String>), String> {
    let (members, unpacked) = members_of(&bytes, kind, &gz_inner(&name))?;
    for cut in (1..=path.len()).rev() {
        let inner = path[..cut].join("/");
        // A path that names an archive inside this one, or runs on into it, goes on in that one.
        if let Some(member) = members.iter().find(|one| !one.dir && one.name == inner)
            && let Some(nested) = kind_of(&inner)
        {
            let data = data_of(&bytes, kind, &unpacked, member)?;
            return descend(data, nested, inner, path[cut..].to_vec());
        }
    }
    Ok((bytes, kind, name, path))
}

/// The children of the folder `path` within the archive of `kind` whose bytes are `bytes`, each
/// path prefixed with `prefix`.
fn children(bytes: &[u8], kind: Kind, name: &str, path: &[String], prefix: &str) -> Result<Vec<Child>, String> {
    let (members, _) = members_of(bytes, kind, &gz_inner(name))?;
    let mut found: Vec<Child> = Vec::new();
    for member in &members {
        let parts: Vec<&str> = member.name.split('/').filter(|part| !part.is_empty()).collect();
        if parts.len() <= path.len() || parts[..path.len()].iter().zip(path).any(|(one, two)| one != two) {
            continue;
        }
        let child = parts[path.len()];
        let deeper = parts.len() > path.len() + 1;
        if let Some(seen) = found.iter_mut().find(|one| one.name == child) {
            seen.dir |= deeper || member.dir;
            continue;
        }
        let archive = !deeper && !member.dir && kind_of(child).is_some();
        let raw = member.raw.as_ref().filter(|_| !deeper).map(|raw| hex(raw.rsplit(|&byte| byte == b'/').find(|part| !part.is_empty()).unwrap_or(raw)));
        found.push(Child { name: child.to_string(), path: format!("{prefix}/{child}"), dir: deeper || member.dir || archive, archive, raw });
    }
    found.sort_by(|a, b| b.dir.cmp(&a.dir).then_with(|| a.name.to_lowercase().cmp(&b.name.to_lowercase())));
    Ok(found)
}

/// Where a path of the tree runs into an archive: the archive's file, and the path's parts past it.
/// A path that runs into no archive is none.
pub fn split(root: &Path, path: &str) -> Option<(PathBuf, Vec<String>)> {
    let full = crate::root::inside(root, path).ok()?;
    let file = full.ancestors().find(|one| one.exists())?;
    if !file.is_file() {
        return None;
    }
    kind_of(&file.file_name()?.to_string_lossy())?;
    let inner = full.strip_prefix(file).ok()?.components().map(|part| part.as_os_str().to_string_lossy().into_owned()).collect();
    Some((file.to_path_buf(), inner))
}

/// The children of the folder of the tree `path` that is an archive or runs into one.
pub fn list(root: &Path, path: &str) -> Result<Vec<Child>, String> {
    let (file, inner) = split(root, path).ok_or_else(|| format!("{path} is no archive"))?;
    let kind = kind_of(&file.to_string_lossy()).ok_or_else(|| format!("{path} is no archive"))?;
    let bytes = std::fs::read(&file).map_err(|error| format!("{}: {error}", file.display()))?;
    let name = file.file_name().map(|one| one.to_string_lossy().into_owned()).unwrap_or_default();
    let (bytes, kind, name, left) = descend(bytes, kind, name, inner)?;
    children(&bytes, kind, &name, &left, path.trim_end_matches('/'))
}

/// The bytes of the file of the tree `path` within an archive.
pub fn read(root: &Path, path: &str) -> Result<Vec<u8>, String> {
    let (file, inner) = split(root, path).ok_or_else(|| format!("{path} is in no archive"))?;
    let kind = kind_of(&file.to_string_lossy()).ok_or_else(|| format!("{path} is in no archive"))?;
    let bytes = std::fs::read(&file).map_err(|error| format!("{}: {error}", file.display()))?;
    let name = file.file_name().map(|one| one.to_string_lossy().into_owned()).unwrap_or_default();
    let (bytes, kind, name, left) = descend(bytes, kind, name, inner)?;
    let wanted = left.join("/");
    let (members, unpacked) = members_of(&bytes, kind, &gz_inner(&name))?;
    let member = members.iter().find(|one| !one.dir && one.name == wanted).ok_or_else(|| format!("{name} holds no {wanted}"))?;
    data_of(&bytes, kind, &unpacked, member)
}

/// Saves `data` as the file of the tree `path` within an archive: the archive is written anew beside
/// the old one and then put in its place.
pub fn write(root: &Path, path: &str, data: &[u8]) -> Result<(), String> {
    let (file, inner) = split(root, path).ok_or_else(|| format!("{path} is in no archive"))?;
    let kind = kind_of(&file.to_string_lossy()).ok_or_else(|| format!("{path} is in no archive"))?;
    let bytes = std::fs::read(&file).map_err(|error| format!("{}: {error}", file.display()))?;
    let name = file.file_name().map(|one| one.to_string_lossy().into_owned()).unwrap_or_default();
    let parts: Vec<&str> = inner.iter().map(String::as_str).collect();
    let new = replace(&bytes, kind, &name, &parts, data)?;
    let beside = file.with_file_name(format!(".{name}.saving"));
    std::fs::write(&beside, &new).map_err(|error| format!("{}: {error}", beside.display()))?;
    std::fs::rename(&beside, &file).map_err(|error| {
        let _ = std::fs::remove_file(&beside);
        format!("{}: {error}", file.display())
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A zip written as other tools write one: each entry deflated or stored, its name in UTF-8 or
    /// in the DOS page.
    fn zip_of(entries: &[(&[u8], &[u8], bool, bool)]) -> Vec<u8> {
        let mut out = Vec::new();
        let mut directory = Vec::new();
        for &(name, data, deflated, utf8) in entries {
            let body = if deflated { deflate(data) } else { data.to_vec() };
            let flags: u16 = if utf8 { 1 << 11 } else { 0 };
            let method: u16 = if deflated { 8 } else { 0 };
            let offset = out.len() as u32;
            let head = |sig: u32, central: bool| {
                let mut head = sig.to_le_bytes().to_vec();
                if central {
                    head.extend(20u16.to_le_bytes());
                }
                head.extend(20u16.to_le_bytes());
                head.extend(flags.to_le_bytes());
                head.extend(method.to_le_bytes());
                head.extend([0u8; 4]);
                head.extend(crc32(data).to_le_bytes());
                head.extend((body.len() as u32).to_le_bytes());
                head.extend((data.len() as u32).to_le_bytes());
                head.extend((name.len() as u16).to_le_bytes());
                head.extend(0u16.to_le_bytes());
                if central {
                    // The comment's length, the disk, and the inner and outer attributes.
                    head.extend([0u8; 10]);
                    head.extend(offset.to_le_bytes());
                }
                head.extend(name);
                head
            };
            out.extend(head(0x0403_4b50, false));
            out.extend(&body);
            directory.extend(head(0x0201_4b50, true));
        }
        let at = out.len() as u32;
        out.extend(&directory);
        out.extend(0x0605_4b50u32.to_le_bytes());
        out.extend([0u8; 4]);
        out.extend((entries.len() as u16).to_le_bytes());
        out.extend((entries.len() as u16).to_le_bytes());
        out.extend((directory.len() as u32).to_le_bytes());
        out.extend(at.to_le_bytes());
        out.extend(0u16.to_le_bytes());
        out
    }

    fn tar_of(entries: &[(&str, &[u8])]) -> Vec<u8> {
        let mut out = Vec::new();
        for &(name, data) in entries {
            let mut header = vec![0u8; 512];
            header[..name.len()].copy_from_slice(name.as_bytes());
            header[100..108].copy_from_slice(b"0000644\0");
            header[124..136].copy_from_slice(format!("{:011o}\0", data.len()).as_bytes());
            header[156] = if name.ends_with('/') { b'5' } else { b'0' };
            header[257..263].copy_from_slice(b"ustar\0");
            header[148..156].fill(b' ');
            let sum: u32 = header.iter().map(|&byte| byte as u32).sum();
            header[148..156].copy_from_slice(format!("{sum:06o}\0 ").as_bytes());
            out.extend(header);
            out.extend(data);
            out.resize(out.len().div_ceil(512) * 512, 0);
        }
        out.extend([0u8; 1024]);
        out
    }

    fn tree(name: &str) -> PathBuf {
        let dir = std::env::temp_dir().join(format!("orior-archive-{name}-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        std::fs::create_dir_all(&dir).unwrap();
        dir
    }

    #[test]
    fn deflate_and_inflate_give_back_every_byte() {
        let text: Vec<u8> = (0..200_000u32).map(|at| b"orior reads archives "[(at as usize * 7) % 21] ^ (at % 251 == 0) as u8).collect();
        for sample in [&b""[..], b"a", b"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", &text] {
            let packed = deflate(sample);
            assert_eq!(inflate(&packed).unwrap().0, sample);
        }
        assert!(deflate(&text).len() < text.len() / 3, "runs come back as lengths and distances");
        assert_eq!(gunzip(&gzip(&text)).unwrap(), text);
    }

    #[test]
    fn a_crc_checks_as_zip_and_gzip_reckon_it() {
        assert_eq!(crc32(b"123456789"), 0xCBF4_3926);
        assert_eq!(crc32(b""), 0);
    }

    #[test]
    fn the_dos_page_has_a_character_for_each_high_byte() {
        assert_eq!(DOS.chars().count(), 128);
        assert_eq!(dos(&[b'a', 0x81, 0x9c, 0xe1]), "aü£ß");
    }

    #[test]
    fn a_zip_opens_as_a_folder_its_files_read_and_one_saved_back() {
        let dir = tree("zip");
        let zip = zip_of(&[(b"docs/read.me", b"hello from a zip\n", true, true), (b"docs/", b"", false, true), (b"top.txt", b"top", false, true), (b"caf\x82.txt", b"dos", false, false)]);
        std::fs::write(dir.join("a.jar"), &zip).unwrap();
        let top = list(&dir, "a.jar").unwrap();
        let names: Vec<(&str, bool)> = top.iter().map(|one| (one.name.as_str(), one.dir)).collect();
        assert_eq!(names, [("docs", true), ("café.txt", false), ("top.txt", false)]);
        assert_eq!(top[1].raw.as_deref(), Some("636166822e747874"), "a name not in UTF-8 keeps its bytes");
        assert_eq!(list(&dir, "a.jar/docs").unwrap()[0].path, "a.jar/docs/read.me");
        assert_eq!(read(&dir, "a.jar/docs/read.me").unwrap(), b"hello from a zip\n");
        write(&dir, "a.jar/docs/read.me", b"changed in place\n").unwrap();
        assert_eq!(read(&dir, "a.jar/docs/read.me").unwrap(), b"changed in place\n");
        assert_eq!(read(&dir, "a.jar/top.txt").unwrap(), b"top", "the other files are as they were");
        assert_eq!(list(&dir, "a.jar").unwrap().len(), 3);
        assert!(!dir.join(".a.jar.saving").exists());
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_run_s_qry_opens_as_a_folder_of_its_blobs_each_name_s_joined_and_is_never_written_back() {
        let dir = tree("qry");
        let blob = |name: &str, bytes: &[u8], sequence: u64| crate::qry::frame(name, bytes, sequence, 1000, 2).0;
        let run = [blob("bench/sqlite3.c", b"int main(void)\n", 1), blob("window.trace", b"paint 0.000 0.400\n", 2), blob("questions.txt", b"why\n", 3), blob("window.trace", b"paint 6.900 0.380\n", 4)].concat();
        std::fs::write(dir.join("query1-x.qry"), &run).unwrap();
        assert_eq!(kind_of("build/qry/query1-x.QRY"), Some(Kind::Qry));
        let top: Vec<(String, bool)> = list(&dir, "query1-x.qry").unwrap().into_iter().map(|one| (one.name, one.dir)).collect();
        assert_eq!(top, [("bench".to_string(), true), ("questions.txt".to_string(), false), ("window.trace".to_string(), false)], "a name with slashes in folders of its own");
        assert_eq!(read(&dir, "query1-x.qry/bench/sqlite3.c").unwrap(), b"int main(void)\n");
        assert_eq!(read(&dir, "query1-x.qry/window.trace").unwrap(), b"paint 0.000 0.400\npaint 6.900 0.380\n", "each name's blobs joined in the order handed");
        assert!(write(&dir, "query1-x.qry/questions.txt", b"changed").unwrap_err().contains("not written back"));
        assert_eq!(std::fs::read(dir.join("query1-x.qry")).unwrap(), run, "the run's file as it was");
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn an_archive_inside_an_archive_opens_and_saves_as_a_folder_too() {
        let dir = tree("nested");
        let inner = tar_of(&[("lib/", b""), ("lib/x.py", b"print(1)\n")]);
        let outer = zip_of(&[(b"inner.tar", &inner, true, true)]);
        std::fs::write(dir.join("outer.zip"), &outer).unwrap();
        let top = list(&dir, "outer.zip").unwrap();
        assert_eq!((top[0].name.as_str(), top[0].dir, top[0].archive), ("inner.tar", true, true));
        assert_eq!(list(&dir, "outer.zip/inner.tar").unwrap()[0].name, "lib");
        assert_eq!(read(&dir, "outer.zip/inner.tar/lib/x.py").unwrap(), b"print(1)\n");
        write(&dir, "outer.zip/inner.tar/lib/x.py", b"print(2)\n").unwrap();
        assert_eq!(read(&dir, "outer.zip/inner.tar/lib/x.py").unwrap(), b"print(2)\n");
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_gzipped_tar_and_a_gzipped_file_open_and_save_back() {
        let dir = tree("gz");
        std::fs::write(dir.join("src.tgz"), gzip(&tar_of(&[("a/b.txt", b"one"), ("c.txt", b"two")]))).unwrap();
        std::fs::write(dir.join("big.log.gz"), gzip(b"line\n")).unwrap();
        assert_eq!(list(&dir, "src.tgz").unwrap().iter().map(|one| one.name.as_str()).collect::<Vec<_>>(), ["a", "c.txt"]);
        write(&dir, "src.tgz/a/b.txt", b"one, longer than it was").unwrap();
        assert_eq!(read(&dir, "src.tgz/a/b.txt").unwrap(), b"one, longer than it was");
        assert_eq!(read(&dir, "src.tgz/c.txt").unwrap(), b"two");
        assert_eq!(list(&dir, "big.log.gz").unwrap()[0].name, "big.log");
        assert_eq!(read(&dir, "big.log.gz/big.log").unwrap(), b"line\n");
        let _ = std::fs::remove_dir_all(&dir);
    }

    /// Archives Python's own zipfile and tarfile write are read here, and what orior writes back is
    /// read by them in turn.
    #[test]
    fn archives_other_tools_write_are_read_and_those_orior_writes_read_by_them() {
        let dir = tree("python");
        let python = crate::runner::python();
        let make = "import sys, zipfile, tarfile, io\nd = sys.argv[1]\nwith zipfile.ZipFile(d + '/py.zip', 'w', zipfile.ZIP_DEFLATED) as z:\n    z.writestr('src/m.py', 'x = 1\\n' * 500)\n    z.writestr('notes.txt', 'plain')\nwith tarfile.open(d + '/py.tar.gz', 'w:gz') as t:\n    data = b'tar text'\n    info = tarfile.TarInfo('pkg/a.txt'); info.size = len(data); t.addfile(info, io.BytesIO(data))\n";
        let made = std::process::Command::new(&python).args(["-c", make]).arg(&dir).output();
        if !made.as_ref().is_ok_and(|out| out.status.success()) {
            eprintln!("no Python to write the archives with");
            return;
        }
        assert_eq!(read(&dir, "py.zip/src/m.py").unwrap(), "x = 1\n".repeat(500).as_bytes());
        assert_eq!(read(&dir, "py.tar.gz/pkg/a.txt").unwrap(), b"tar text");
        write(&dir, "py.zip/notes.txt", b"written by orior").unwrap();
        write(&dir, "py.tar.gz/pkg/a.txt", b"tar text, written by orior").unwrap();
        let check = "import sys, zipfile, tarfile\nd = sys.argv[1]\nz = zipfile.ZipFile(d + '/py.zip')\nassert z.testzip() is None\nprint(z.read('notes.txt').decode())\nprint(len(z.read('src/m.py')))\nprint(tarfile.open(d + '/py.tar.gz').extractfile('pkg/a.txt').read().decode())\n";
        let read_back = std::process::Command::new(&python).args(["-c", check]).arg(&dir).output().unwrap();
        assert!(read_back.status.success(), "{}", String::from_utf8_lossy(&read_back.stderr));
        assert_eq!(String::from_utf8_lossy(&read_back.stdout).lines().collect::<Vec<_>>(), ["written by orior", "3000", "tar text, written by orior"]);
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_path_of_the_tree_runs_into_an_archive_only_past_an_archive_s_file() {
        let dir = tree("split");
        std::fs::create_dir_all(dir.join("lib")).unwrap();
        std::fs::write(dir.join("lib/a.zip"), zip_of(&[(b"x", b"x", false, true)])).unwrap();
        std::fs::write(dir.join("lib/plain.txt"), b"").unwrap();
        assert_eq!(split(&dir, "lib/a.zip/x"), Some((dir.join("lib/a.zip"), vec!["x".to_string()])));
        assert_eq!(split(&dir, "lib/plain.txt"), None);
        assert_eq!(split(&dir, "lib"), None);
        assert!(read(&dir, "lib/a.zip/missing").is_err());
        let _ = std::fs::remove_dir_all(&dir);
    }
}

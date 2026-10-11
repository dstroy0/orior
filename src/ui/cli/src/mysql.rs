// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! MySQL and MariaDB, spoken by orior itself in the client/server protocol 4.1 over TCP, with no
//! driver: the handshake, a password by mysql_native_password or caching_sha2_password as the
//! account asks, a statement by COM_QUERY, and its rows read off the socket a packet at a time, the
//! rest left to wait in the server until they are asked for. A statement running is stopped by KILL
//! QUERY sent on a connection of its own.
//!
//! caching_sha2_password signs in by its scramble once the server holds the account's password in
//! its cache; until then it takes the password itself, which over a connection that is not encrypted
//! goes encrypted by the server's RSA key with OAEP padding, as MySQL 8 asks. The connection is not
//! encrypted: a server elsewhere is reached through an ssh tunnel.

use std::io::{BufReader, BufWriter, Read, Write};
use std::net::TcpStream;
use std::time::Duration;

use crate::digest::{sha1_bytes, sha256_bytes};
use crate::pg::{Column, Failure, Head, Row};
use crate::serve::unbase64;

const LONG_PASSWORD: u32 = 0x1;
const LONG_FLAG: u32 = 0x4;
const CONNECT_WITH_DB: u32 = 0x8;
const PROTOCOL_41: u32 = 0x200;
const TRANSACTIONS: u32 = 0x2000;
const SECURE_CONNECTION: u32 = 0x8000;
const MULTI_RESULTS: u32 = 0x20000;
const PLUGIN_AUTH: u32 = 0x80000;
const PLUGIN_AUTH_LENENC: u32 = 0x200000;

const IN_TRANSACTION: u16 = 0x1;
const MORE_RESULTS: u16 = 0x8;

/// Where and as whom a connection signs in, kept to reach the server again to stop a statement.
#[derive(Clone, Debug)]
pub struct Cancel {
    host: String,
    port: u16,
    user: String,
    password: String,
    id: u32,
}

impl Cancel {
    /// Stops the statement the connection runs now.
    pub fn send(&self) -> Result<(), String> {
        let mut other = Mysql::connect(&self.host, self.port, &self.user, &self.password, "", Duration::from_secs(5)).map_err(|failure| failure.to_string())?;
        other.all(&format!("KILL QUERY {}", self.id)).map_err(|failure| failure.to_string())?;
        other.close();
        Ok(())
    }
}

/// A connection to a MySQL or MariaDB server.
pub struct Mysql {
    reader: BufReader<TcpStream>,
    writer: BufWriter<TcpStream>,
    sequence: u8,
    cancel: Cancel,
    /// The server's version, as it gave it.
    pub version: String,
    /// The server's status at its last OK or EOF: a transaction open, more results to come.
    pub status: u16,
    /// The count of warnings the last statement left.
    pub warnings: u16,
    /// The columns of the result being read, for their types.
    columns: Vec<(u8, u16)>,
    reading: bool,
    /// Whether the sign-in sent the password itself, sealed by the server's key.
    pub sealed: bool,
}

fn lenenc(bytes: &[u8], at: &mut usize) -> Option<u64> {
    let first = *bytes.get(*at)?;
    *at += 1;
    let width = match first {
        0xfc => 2,
        0xfd => 3,
        0xfe => 8,
        0xfb => return None,
        _ => return Some(u64::from(first)),
    };
    let mut value = 0u64;
    for index in 0..width {
        value |= u64::from(*bytes.get(*at + index)?) << (8 * index);
    }
    *at += width;
    Some(value)
}

fn lenenc_bytes<'a>(bytes: &'a [u8], at: &mut usize) -> Option<&'a [u8]> {
    let length = lenenc(bytes, at)? as usize;
    let out = bytes.get(*at..*at + length)?;
    *at += length;
    Some(out)
}

fn put_lenenc(out: &mut Vec<u8>, value: usize) {
    if value < 0xfb {
        out.push(value as u8);
    } else if value < 0x10000 {
        out.push(0xfc);
        out.extend_from_slice(&(value as u16).to_le_bytes());
    } else {
        out.push(0xfd);
        out.extend_from_slice(&(value as u32).to_le_bytes()[..3]);
    }
}

/// mysql_native_password's answer: SHA1(password) XOR SHA1(scramble, SHA1(SHA1(password))).
fn native(password: &str, scramble: &[u8]) -> Vec<u8> {
    if password.is_empty() {
        return Vec::new();
    }
    let once = sha1_bytes(password.as_bytes());
    let twice = sha1_bytes(&once);
    let mixed = sha1_bytes(&[scramble, &twice[..]].concat());
    once.iter().zip(mixed).map(|(a, b)| a ^ b).collect()
}

/// caching_sha2_password's answer: SHA256(password) XOR SHA256(SHA256(SHA256(password)), scramble).
fn caching(password: &str, scramble: &[u8]) -> Vec<u8> {
    if password.is_empty() {
        return Vec::new();
    }
    let once = sha256_bytes(password.as_bytes());
    let twice = sha256_bytes(&once);
    let mixed = sha256_bytes(&[&twice[..], scramble].concat());
    once.iter().zip(mixed).map(|(a, b)| a ^ b).collect()
}

fn failure_of(payload: &[u8]) -> Failure {
    let code = u16::from_le_bytes([payload.get(1).copied().unwrap_or(0), payload.get(2).copied().unwrap_or(0)]);
    let (state, message) = if payload.get(3) == Some(&b'#') { (String::from_utf8_lossy(payload.get(4..9).unwrap_or_default()).into_owned(), payload.get(9..).unwrap_or_default()) } else { (String::new(), payload.get(3..).unwrap_or_default()) };
    Failure { message: String::from_utf8_lossy(message).into_owned(), code: if state.is_empty() { code.to_string() } else { format!("{code} {state}") }, ..Failure::default() }
}

/// The type names of MySQL's column types, and whether their values are numbers.
fn type_name(code: u8) -> (&'static str, bool) {
    match code {
        0 | 246 => ("decimal", true),
        1 => ("tinyint", true),
        2 => ("smallint", true),
        3 => ("int", true),
        4 => ("float", true),
        5 => ("double", true),
        6 => ("null", false),
        7 => ("timestamp", false),
        8 => ("bigint", true),
        9 => ("mediumint", true),
        10 => ("date", false),
        11 => ("time", false),
        12 => ("datetime", false),
        13 => ("year", true),
        15 | 253 => ("varchar", false),
        16 => ("bit", false),
        245 => ("json", false),
        247 => ("enum", false),
        248 => ("set", false),
        249..=252 => ("blob", false),
        254 => ("char", false),
        255 => ("geometry", false),
        _ => ("", false),
    }
}

impl Mysql {
    /// Connects to `database` at host:port as `user`, signing in with `password` as the account
    /// asks, within `wait`.
    pub fn connect(host: &str, port: u16, user: &str, password: &str, database: &str, wait: Duration) -> Result<Mysql, Failure> {
        let address = std::net::ToSocketAddrs::to_socket_addrs(&(host, port)).map_err(|error| format!("{host}:{port}: {error}"))?.next().ok_or_else(|| format!("{host} has no address"))?;
        let stream = TcpStream::connect_timeout(&address, wait).map_err(|error| format!("{host}:{port}: {error}"))?;
        stream.set_nodelay(true).ok();
        stream.set_read_timeout(Some(wait)).ok();
        let mut my = Mysql {
            reader: BufReader::with_capacity(1 << 16, stream.try_clone().map_err(|error| error.to_string())?),
            writer: BufWriter::new(stream),
            sequence: 0,
            cancel: Cancel { host: host.to_string(), port, user: user.to_string(), password: password.to_string(), id: 0 },
            version: String::new(),
            status: 0,
            warnings: 0,
            columns: Vec::new(),
            reading: false,
            sealed: false,
        };
        let hello = my.packet()?;
        if hello.first() == Some(&0xff) {
            return Err(failure_of(&hello));
        }
        if hello.first() != Some(&10) {
            return Err(format!("{host}:{port} does not speak MySQL's protocol 10").into());
        }
        let mut at = 1;
        let end = hello[at..].iter().position(|byte| *byte == 0).map(|found| at + found).ok_or("the server's greeting is cut short")?;
        my.version = String::from_utf8_lossy(&hello[at..end]).into_owned();
        at = end + 1;
        my.cancel.id = u32::from_le_bytes(hello.get(at..at + 4).ok_or("the server's greeting is cut short")?.try_into().unwrap_or_default());
        at += 4;
        let mut scramble = hello.get(at..at + 8).ok_or("the server's greeting is cut short")?.to_vec();
        at += 9;
        let lower = u16::from_le_bytes([hello[at], hello[at + 1]]) as u32;
        at += 2 + 1 + 2;
        let upper = u16::from_le_bytes([hello[at], hello[at + 1]]) as u32;
        let capabilities = lower | (upper << 16);
        at += 2;
        let data_length = hello[at] as usize;
        at += 1 + 10;
        let rest = data_length.saturating_sub(8).max(13);
        if let Some(more) = hello.get(at..at + rest) {
            scramble.extend_from_slice(more.strip_suffix(&[0]).unwrap_or(more));
            at += rest;
        }
        let mut plugin = if capabilities & PLUGIN_AUTH != 0 {
            let end = hello[at..].iter().position(|byte| *byte == 0).map(|found| at + found).unwrap_or(hello.len());
            String::from_utf8_lossy(&hello[at..end]).into_owned()
        } else {
            "mysql_native_password".to_string()
        };
        let answer = match plugin.as_str() {
            "caching_sha2_password" => caching(password, &scramble),
            _ => {
                plugin = "mysql_native_password".to_string();
                native(password, &scramble)
            }
        };
        let mut flags = LONG_PASSWORD | LONG_FLAG | PROTOCOL_41 | TRANSACTIONS | SECURE_CONNECTION | MULTI_RESULTS | PLUGIN_AUTH | PLUGIN_AUTH_LENENC;
        if !database.is_empty() {
            flags |= CONNECT_WITH_DB;
        }
        let mut reply = Vec::new();
        reply.extend_from_slice(&flags.to_le_bytes());
        reply.extend_from_slice(&(16u32 << 20).to_le_bytes());
        reply.push(45);
        reply.extend_from_slice(&[0u8; 23]);
        reply.extend_from_slice(user.as_bytes());
        reply.push(0);
        put_lenenc(&mut reply, answer.len());
        reply.extend_from_slice(&answer);
        if !database.is_empty() {
            reply.extend_from_slice(database.as_bytes());
            reply.push(0);
        }
        reply.extend_from_slice(plugin.as_bytes());
        reply.push(0);
        my.send(&reply)?;
        loop {
            let said = my.packet()?;
            match said.first() {
                Some(0x00) => break,
                Some(0xff) => return Err(failure_of(&said)),
                Some(0xfe) => {
                    let end = said[1..].iter().position(|byte| *byte == 0).map(|found| 1 + found).unwrap_or(said.len());
                    plugin = String::from_utf8_lossy(&said[1..end]).into_owned();
                    let data = said.get(end + 1..).unwrap_or_default();
                    scramble = data.strip_suffix(&[0]).unwrap_or(data).to_vec();
                    let answer = match plugin.as_str() {
                        "mysql_native_password" => native(password, &scramble),
                        "caching_sha2_password" => caching(password, &scramble),
                        "mysql_clear_password" => [password.as_bytes(), &[0]].concat(),
                        other => return Err(format!("the account signs in by {other}, which orior does not speak: mysql_native_password and caching_sha2_password are spoken").into()),
                    };
                    my.send(&answer)?;
                }
                Some(0x01) => match said.get(1) {
                    Some(0x03) => {}
                    Some(0x04) => my.send(&[0x02])?,
                    _ => {
                        let pem = String::from_utf8_lossy(&said[1..]).into_owned();
                        let key = rsa::public_key(&pem).ok_or("the server's RSA key could not be read")?;
                        let mut secret = password.as_bytes().to_vec();
                        secret.push(0);
                        for (index, byte) in secret.iter_mut().enumerate() {
                            *byte ^= scramble[index % scramble.len()];
                        }
                        let sealed = rsa::encrypt_oaep(&key, &secret).ok_or("the password is too long for the server's RSA key")?;
                        my.send(&sealed)?;
                        my.sealed = true;
                    }
                },
                _ => return Err("the server answered the sign-in with what orior cannot read".into()),
            }
        }
        my.reader.get_ref().set_read_timeout(None).ok();
        Ok(my)
    }

    /// What stops the statement this connection runs, from another thread.
    pub fn canceller(&self) -> Cancel {
        self.cancel.clone()
    }

    /// The connection's id on the server.
    pub fn id(&self) -> u32 {
        self.cancel.id
    }

    fn send(&mut self, payload: &[u8]) -> Result<(), Failure> {
        let broke = |error: std::io::Error| Failure::from(format!("the connection broke: {error}"));
        let mut chunks = payload.chunks(0xff_ffff).peekable();
        if chunks.peek().is_none() {
            self.writer.write_all(&[0, 0, 0, self.sequence]).map_err(broke)?;
            self.sequence = self.sequence.wrapping_add(1);
        }
        let mut last = 0;
        for chunk in chunks {
            last = chunk.len();
            let length = (chunk.len() as u32).to_le_bytes();
            self.writer.write_all(&[length[0], length[1], length[2], self.sequence]).and_then(|_| self.writer.write_all(chunk)).map_err(broke)?;
            self.sequence = self.sequence.wrapping_add(1);
        }
        if last == 0xff_ffff {
            self.writer.write_all(&[0, 0, 0, self.sequence]).map_err(broke)?;
            self.sequence = self.sequence.wrapping_add(1);
        }
        self.writer.flush().map_err(broke)
    }

    fn packet(&mut self) -> Result<Vec<u8>, Failure> {
        let mut out = Vec::new();
        loop {
            let mut head = [0u8; 4];
            self.reader.read_exact(&mut head).map_err(|error| Failure::from(format!("the connection broke: {error}")))?;
            let length = u32::from_le_bytes([head[0], head[1], head[2], 0]) as usize;
            self.sequence = head[3].wrapping_add(1);
            let start = out.len();
            out.resize(start + length, 0);
            self.reader.read_exact(&mut out[start..]).map_err(|error| Failure::from(format!("the connection broke: {error}")))?;
            if length < 0xff_ffff {
                return Ok(out);
            }
        }
    }

    fn ok(&mut self, payload: &[u8]) -> String {
        let mut at = 1;
        let affected = lenenc(payload, &mut at).unwrap_or(0);
        let inserted = lenenc(payload, &mut at).unwrap_or(0);
        self.status = u16::from_le_bytes([payload.get(at).copied().unwrap_or(0), payload.get(at + 1).copied().unwrap_or(0)]);
        self.warnings = u16::from_le_bytes([payload.get(at + 2).copied().unwrap_or(0), payload.get(at + 3).copied().unwrap_or(0)]);
        let info = String::from_utf8_lossy(payload.get(at + 4..).unwrap_or_default()).trim().to_string();
        let mut tag = format!("{affected} row{} affected", if affected == 1 { "" } else { "s" });
        if inserted > 0 {
            tag.push_str(&format!(", last insert id {inserted}"));
        }
        if !info.is_empty() {
            tag.push_str(&format!(": {info}"));
        }
        tag
    }

    fn eof(&mut self, payload: &[u8]) {
        self.warnings = u16::from_le_bytes([payload.get(1).copied().unwrap_or(0), payload.get(2).copied().unwrap_or(0)]);
        self.status = u16::from_le_bytes([payload.get(3).copied().unwrap_or(0), payload.get(4).copied().unwrap_or(0)]);
    }

    /// Whether a transaction is open on the connection.
    pub fn in_transaction(&self) -> bool {
        self.status & IN_TRANSACTION != 0
    }

    /// Sends `sql` as one query and reads until it gives rows or ends.
    pub fn start(&mut self, sql: &str) -> Result<Head, Failure> {
        if self.reading {
            self.finish()?;
        }
        self.sequence = 0;
        let mut command = vec![0x03];
        command.extend_from_slice(sql.as_bytes());
        self.send(&command)?;
        self.head()
    }

    fn head(&mut self) -> Result<Head, Failure> {
        let said = self.packet()?;
        match said.first() {
            Some(0x00) => {
                let tag = self.ok(&said);
                if self.status & MORE_RESULTS != 0 {
                    self.drain_results()?;
                }
                Ok(Head::Done(tag))
            }
            Some(0xff) => Err(failure_of(&said)),
            Some(0xfb) => {
                self.send(&[])?;
                let _ = self.packet()?;
                Err("orior sends no file to LOAD DATA LOCAL INFILE".into())
            }
            _ => {
                let mut at = 0;
                let count = lenenc(&said, &mut at).unwrap_or(0) as usize;
                let mut columns = Vec::with_capacity(count);
                self.columns.clear();
                for _ in 0..count {
                    let definition = self.packet()?;
                    let mut at = 0;
                    for _ in 0..4 {
                        lenenc_bytes(&definition, &mut at);
                    }
                    let name = String::from_utf8_lossy(lenenc_bytes(&definition, &mut at).unwrap_or_default()).into_owned();
                    lenenc_bytes(&definition, &mut at);
                    lenenc(&definition, &mut at);
                    let charset = u16::from_le_bytes([definition.get(at).copied().unwrap_or(0), definition.get(at + 1).copied().unwrap_or(0)]);
                    let code = definition.get(at + 6).copied().unwrap_or(253);
                    let (kind, numeric) = type_name(code);
                    let kind = if charset == 63 && matches!(code, 15 | 249..=254) { if (249..=252).contains(&code) { "blob" } else { "binary" } } else { kind };
                    columns.push(Column { name, kind: if kind.is_empty() { format!("type {code}") } else { kind.to_string() }, numeric });
                    self.columns.push((code, charset));
                }
                let end = self.packet()?;
                if end.first() == Some(&0xfe) {
                    self.eof(&end);
                }
                self.reading = true;
                Ok(Head::Rows(columns))
            }
        }
    }

    /// Reads past the results a statement gave after its first, as a procedure's CALL gives.
    fn drain_results(&mut self) -> Result<(), Failure> {
        while self.status & MORE_RESULTS != 0 {
            if let Head::Rows(_) = self.head()? {
                self.finish()?;
            }
        }
        Ok(())
    }

    /// Up to `count` rows more of the statement running, and a tag once it has none left.
    pub fn rows(&mut self, count: usize) -> Result<(Vec<Row>, Option<String>), Failure> {
        let mut rows = Vec::new();
        if !self.reading {
            return Ok((rows, Some(String::new())));
        }
        while rows.len() < count {
            let said = self.packet()?;
            match said.first() {
                Some(0xfe) if said.len() < 9 => {
                    self.eof(&said);
                    self.reading = false;
                    if self.status & MORE_RESULTS != 0 {
                        self.drain_results()?;
                    }
                    let tag = format!("{} row{} read", rows.len(), if rows.len() == 1 { "" } else { "s" });
                    return Ok((rows, Some(tag)));
                }
                Some(0xff) => {
                    self.reading = false;
                    return Err(failure_of(&said));
                }
                _ => {
                    let mut at = 0;
                    let mut row = Vec::with_capacity(self.columns.len());
                    for (code, charset) in &self.columns {
                        if said.get(at) == Some(&0xfb) {
                            at += 1;
                            row.push(None);
                            continue;
                        }
                        let bytes = lenenc_bytes(&said, &mut at).unwrap_or_default();
                        let binary = *charset == 63 && matches!(code, 15 | 249..=254 | 16 | 255);
                        row.push(Some(if binary && std::str::from_utf8(bytes).is_err() || *code == 16 || *code == 255 { hex(bytes) } else { String::from_utf8_lossy(bytes).into_owned() }));
                    }
                    rows.push(row);
                }
            }
        }
        Ok((rows, None))
    }

    /// Reads past the rows the statement running has left.
    pub fn finish(&mut self) -> Result<(), Failure> {
        while self.reading {
            self.rows(4096)?;
        }
        Ok(())
    }

    /// Every row `sql` gives, for a question that gives few.
    pub fn all(&mut self, sql: &str) -> Result<Vec<Row>, Failure> {
        match self.start(sql)? {
            Head::Done(_) => Ok(Vec::new()),
            Head::Rows(_) => {
                let mut out = Vec::new();
                loop {
                    let (rows, done) = self.rows(4096)?;
                    out.extend(rows);
                    if done.is_some() {
                        return Ok(out);
                    }
                }
            }
        }
    }

    /// Ends the connection as the protocol asks.
    pub fn close(mut self) {
        self.sequence = 0;
        let _ = self.send(&[0x01]);
    }
}

/// Bytes as MySQL writes a binary literal, the first 256 of a long value and its length.
fn hex(bytes: &[u8]) -> String {
    let shown: String = bytes.iter().take(256).map(|byte| format!("{byte:02X}")).collect();
    if bytes.len() > 256 {
        format!("0x{shown}… ({} bytes)", bytes.len())
    } else {
        format!("0x{shown}")
    }
}

/// `text` as a string MySQL reads back as itself.
pub fn literal(text: &str) -> String {
    format!("'{}'", text.replace('\\', "\\\\").replace('\'', "''"))
}

/// RSA encryption with a public key, as RFC 8017 sets it out, with OAEP padding by SHA-1: the
/// password caching_sha2_password takes over a connection that is not encrypted.
mod rsa {
    use super::{sha1_bytes, unbase64};

    /// A number as its 32-bit words, least first.
    #[derive(Clone, Debug, PartialEq, Eq)]
    pub struct Big(Vec<u32>);

    impl Big {
        pub fn from_be(bytes: &[u8]) -> Big {
            let mut words = Vec::with_capacity(bytes.len() / 4 + 1);
            for chunk in bytes.rchunks(4) {
                let mut word = 0u32;
                for byte in chunk {
                    word = (word << 8) | u32::from(*byte);
                }
                words.push(word);
            }
            Big(words).trimmed()
        }

        pub fn to_be(&self, length: usize) -> Vec<u8> {
            let mut out = vec![0u8; length];
            for (index, word) in self.0.iter().enumerate() {
                for (shift, byte) in word.to_le_bytes().iter().enumerate() {
                    let at = index * 4 + shift;
                    if at < length {
                        out[length - 1 - at] = *byte;
                    }
                }
            }
            out
        }

        fn trimmed(mut self) -> Big {
            while self.0.last() == Some(&0) {
                self.0.pop();
            }
            self
        }

        fn bits(&self) -> usize {
            self.0.last().map(|top| self.0.len() * 32 - top.leading_zeros() as usize).unwrap_or(0)
        }

        fn bit(&self, at: usize) -> bool {
            self.0.get(at / 32).is_some_and(|word| word >> (at % 32) & 1 == 1)
        }

        fn mul(&self, other: &Big) -> Big {
            let mut out = vec![0u32; self.0.len() + other.0.len() + 1];
            for (i, a) in self.0.iter().enumerate() {
                let mut carry = 0u64;
                for (j, b) in other.0.iter().enumerate() {
                    let sum = u64::from(out[i + j]) + u64::from(*a) * u64::from(*b) + carry;
                    out[i + j] = sum as u32;
                    carry = sum >> 32;
                }
                let mut at = i + other.0.len();
                while carry > 0 {
                    let sum = u64::from(out[at]) + carry;
                    out[at] = sum as u32;
                    carry = sum >> 32;
                    at += 1;
                }
            }
            Big(out).trimmed()
        }

        fn at_least(&self, other: &Big) -> bool {
            if self.0.len() != other.0.len() {
                return self.0.len() > other.0.len();
            }
            for (a, b) in self.0.iter().rev().zip(other.0.iter().rev()) {
                if a != b {
                    return a > b;
                }
            }
            true
        }

        fn sub(&mut self, other: &Big) {
            let mut borrow = 0i64;
            for index in 0..self.0.len() {
                let mut value = i64::from(self.0[index]) - borrow - i64::from(*other.0.get(index).unwrap_or(&0));
                borrow = 0;
                if value < 0 {
                    value += 1 << 32;
                    borrow = 1;
                }
                self.0[index] = value as u32;
            }
            while self.0.last() == Some(&0) {
                self.0.pop();
            }
        }

        fn shift_in(&mut self, bit: bool) {
            let mut carry = u32::from(bit);
            for word in self.0.iter_mut() {
                let next = *word >> 31;
                *word = (*word << 1) | carry;
                carry = next;
            }
            if carry != 0 {
                self.0.push(carry);
            }
        }

        fn rem(&self, modulus: &Big) -> Big {
            let mut out = Big(Vec::new());
            for at in (0..self.bits()).rev() {
                out.shift_in(self.bit(at));
                if out.at_least(modulus) {
                    out.sub(modulus);
                }
            }
            out
        }

        pub fn pow_mod(&self, exponent: &Big, modulus: &Big) -> Big {
            let mut out = Big(vec![1]);
            let base = self.rem(modulus);
            for at in (0..exponent.bits()).rev() {
                out = out.mul(&out).rem(modulus);
                if exponent.bit(at) {
                    out = out.mul(&base).rem(modulus);
                }
            }
            out
        }
    }

    /// A public key: its modulus, its exponent and its length in bytes.
    pub struct Key {
        pub modulus: Big,
        pub exponent: Big,
        pub length: usize,
    }

    /// The DER element at `at`: its tag and its contents, moving `at` past it.
    fn element<'a>(der: &'a [u8], at: &mut usize) -> Option<(u8, &'a [u8])> {
        let tag = *der.get(*at)?;
        let mut length = *der.get(*at + 1)? as usize;
        *at += 2;
        if length & 0x80 != 0 {
            let width = length & 0x7f;
            length = 0;
            for _ in 0..width {
                length = (length << 8) | *der.get(*at)? as usize;
                *at += 1;
            }
        }
        let contents = der.get(*at..*at + length)?;
        *at += length;
        Some((tag, contents))
    }

    /// The key a PEM holds, as a SubjectPublicKeyInfo or as PKCS #1's RSAPublicKey.
    pub fn public_key(pem: &str) -> Option<Key> {
        let body: String = pem.lines().filter(|line| !line.starts_with("-----")).collect();
        let der = unbase64(&body)?;
        let mut at = 0;
        let (_, outer) = element(&der, &mut at)?;
        let mut inner_at = 0;
        let (tag, first) = element(outer, &mut inner_at)?;
        let pair = if tag == 0x30 {
            let (_, bits) = element(outer, &mut inner_at)?;
            let mut key_at = 0;
            element(bits.get(1..)?, &mut key_at)?.1
        } else {
            let _ = first;
            outer
        };
        let mut pair_at = 0;
        let (_, modulus) = element(pair, &mut pair_at)?;
        let (_, exponent) = element(pair, &mut pair_at)?;
        let modulus = modulus.strip_prefix(&[0]).unwrap_or(modulus);
        Some(Key { length: modulus.len(), modulus: Big::from_be(modulus), exponent: Big::from_be(exponent) })
    }

    fn mgf1(seed: &[u8], length: usize) -> Vec<u8> {
        let mut out = Vec::with_capacity(length + 20);
        let mut counter = 0u32;
        while out.len() < length {
            out.extend_from_slice(&sha1_bytes(&[seed, &counter.to_be_bytes()].concat()));
            counter += 1;
        }
        out.truncate(length);
        out
    }

    /// `message` padded by OAEP with SHA-1 and an empty label, with `seed`, to `length` bytes.
    pub fn oaep(message: &[u8], seed: &[u8; 20], length: usize) -> Option<Vec<u8>> {
        if message.len() + 42 > length {
            return None;
        }
        let mut block = sha1_bytes(b"").to_vec();
        block.resize(length - message.len() - 22, 0);
        block.push(1);
        block.extend_from_slice(message);
        let mask = mgf1(seed, block.len());
        let masked: Vec<u8> = block.iter().zip(mask).map(|(a, b)| a ^ b).collect();
        let seed_mask = mgf1(&masked, 20);
        let mut out = vec![0u8];
        out.extend(seed.iter().zip(seed_mask).map(|(a, b)| a ^ b));
        out.extend(masked);
        Some(out)
    }

    /// `message` encrypted for the holder of `key`'s private half.
    pub fn encrypt_oaep(key: &Key, message: &[u8]) -> Option<Vec<u8>> {
        let mut seed = [0u8; 20];
        seed.copy_from_slice(&crate::pg::random(20));
        let padded = oaep(message, &seed, key.length)?;
        Some(Big::from_be(&padded).pow_mod(&key.exponent, &key.modulus).to_be(key.length))
    }

    #[cfg(test)]
    mod sums {
        use super::*;

        #[test]
        fn a_power_is_taken_by_its_modulus() {
            assert_eq!(Big::from_be(&[4]).pow_mod(&Big::from_be(&[13]), &Big::from_be(&[0x01, 0xf1])), Big::from_be(&[0x01, 0xbd]));
            let big = Big::from_be(&[0xff; 64]);
            let modulus = Big::from_be(&[0xfd; 40]);
            let squared = big.mul(&big).rem(&modulus);
            let again = big.rem(&modulus).mul(&big.rem(&modulus)).rem(&modulus);
            assert_eq!(squared, again);
            assert_eq!(Big::from_be(&[1, 2, 3, 4, 5]).to_be(6), vec![0, 1, 2, 3, 4, 5]);
        }

        #[test]
        fn oaep_unmasks_to_its_message() {
            let seed = [7u8; 20];
            let padded = oaep(b"secret\0", &seed, 128).unwrap();
            assert_eq!((padded.len(), padded[0]), (128, 0));
            let masked = &padded[21..];
            let seed_back: Vec<u8> = padded[1..21].iter().zip(mgf1(masked, 20)).map(|(a, b)| a ^ b).collect();
            assert_eq!(seed_back, seed);
            let block: Vec<u8> = masked.iter().zip(mgf1(&seed, masked.len())).map(|(a, b)| a ^ b).collect();
            assert_eq!(&block[..20], &sha1_bytes(b""));
            assert!(block.ends_with(b"\x01secret\0"));
        }
    }
}

#[cfg(test)]
mod speaking {
    use super::*;

    #[test]
    fn the_scrambles_are_the_ones_mysql_computes() {
        let scramble = b"abcdefghijklmnopqrst";
        let once = sha1_bytes(b"secret");
        let check = sha1_bytes(&[&scramble[..], &sha1_bytes(&once)[..]].concat());
        let back: Vec<u8> = native("secret", scramble).iter().zip(check).map(|(a, b)| a ^ b).collect();
        assert_eq!(back, once.to_vec());
        assert!(native("", scramble).is_empty());
        assert_eq!(caching("secret", scramble).len(), 32);
    }

    /// The server orior's tests sign in to, where one runs: MySQL on 127.0.0.1:3306 with the
    /// password ORIOR_MYSQL_PASSWORD names for root.
    fn server() -> Option<(Mysql, String)> {
        let password = std::env::var("ORIOR_MYSQL_PASSWORD").ok()?;
        Mysql::connect("127.0.0.1", 3306, "root", &password, "", Duration::from_secs(5)).ok().map(|my| (my, password))
    }

    #[test]
    fn each_sign_in_works_and_a_query_s_rows_come_a_page_at_a_time() {
        let Some((mut my, _)) = server() else { return };
        my.all("DROP USER IF EXISTS 'orior_sha2'@'127.0.0.1', 'orior_native'@'127.0.0.1'").unwrap();
        my.all("CREATE USER 'orior_sha2'@'127.0.0.1' IDENTIFIED WITH caching_sha2_password BY 'sha2 secret'").unwrap();
        my.all("CREATE USER 'orior_native'@'127.0.0.1' IDENTIFIED WITH mysql_native_password BY 'native secret'").unwrap();
        my.all("FLUSH PRIVILEGES").unwrap();
        // The first sign-in after the flush takes the password whole, sealed by the server's key;
        // the second is answered from the server's cache.
        for sealed in [true, false] {
            let signed = Mysql::connect("127.0.0.1", 3306, "orior_sha2", "sha2 secret", "", Duration::from_secs(5));
            assert!(signed.as_ref().is_ok_and(|one| one.sealed == sealed), "{:?}", signed.map(|one| one.sealed).err());
        }
        let native = Mysql::connect("127.0.0.1", 3306, "orior_native", "native secret", "", Duration::from_secs(5));
        assert!(native.is_ok(), "{:?}", native.err());
        let wrong = Mysql::connect("127.0.0.1", 3306, "orior_sha2", "not it", "", Duration::from_secs(5)).err().unwrap();
        assert!(wrong.code.starts_with("1045"), "{wrong}");
        my.all("CREATE DATABASE IF NOT EXISTS orior_test").unwrap();
        my.all("CREATE TABLE IF NOT EXISTS orior_test.n (n INT, t VARCHAR(10), b VARBINARY(4), at TIMESTAMP NULL)").unwrap();
        my.all("TRUNCATE orior_test.n").unwrap();
        let Head::Done(tag) = my.start("INSERT INTO orior_test.n SELECT 1 + a.d + 10 * b.d + 100 * c.d, 'x', 0x00FF, NULL FROM (SELECT 0 d UNION SELECT 1 UNION SELECT 2 UNION SELECT 3 UNION SELECT 4 UNION SELECT 5 UNION SELECT 6 UNION SELECT 7 UNION SELECT 8 UNION SELECT 9) a, (SELECT 0 d UNION SELECT 1 UNION SELECT 2 UNION SELECT 3 UNION SELECT 4 UNION SELECT 5 UNION SELECT 6 UNION SELECT 7 UNION SELECT 8 UNION SELECT 9) b, (SELECT 0 d UNION SELECT 1 UNION SELECT 2 UNION SELECT 3 UNION SELECT 4 UNION SELECT 5 UNION SELECT 6 UNION SELECT 7 UNION SELECT 8 UNION SELECT 9) c").unwrap() else { panic!("rows from an insert") };
        assert!(tag.starts_with("1000 rows affected"), "{tag}");
        let Head::Rows(columns) = my.start("SELECT n, t, b, at FROM orior_test.n ORDER BY n").unwrap() else { panic!("no rows") };
        assert_eq!(columns.iter().map(|one| (one.name.as_str(), one.kind.as_str(), one.numeric)).collect::<Vec<_>>(), vec![("n", "int", true), ("t", "varchar", false), ("b", "binary", false), ("at", "timestamp", false)]);
        let (first, done) = my.rows(400).unwrap();
        assert_eq!((first.len(), done.is_none(), first[399][0].as_deref()), (400, true, Some("400")));
        assert_eq!((first[0][2].as_deref(), first[0][3].clone()), (Some("0x00FF"), None));
        let (rest, done) = my.rows(1000).unwrap();
        assert_eq!((rest.len(), done.is_some()), (600, true));
        assert!(matches!(my.start("BEGIN").unwrap(), Head::Done(_)));
        my.all("DELETE FROM orior_test.n WHERE n = 1").unwrap();
        assert!(my.in_transaction());
        my.all("ROLLBACK").unwrap();
        assert!(!my.in_transaction());
        let failed = my.start("SELECT * FROM orior_test.none").unwrap_err();
        assert!(failed.code.starts_with("1146"), "{failed}");
        let cancel = my.canceller();
        let stopper = std::thread::spawn(move || {
            std::thread::sleep(Duration::from_millis(400));
            cancel.send().unwrap();
        });
        let started = std::time::Instant::now();
        let slept = my.all("SELECT SLEEP(30)").unwrap();
        stopper.join().unwrap();
        assert!(started.elapsed() < Duration::from_secs(10));
        assert_eq!(slept, vec![vec![Some("1".to_string())]]);
        assert_eq!(my.all(&format!("SELECT {}", literal("it's \\ ok"))).unwrap(), vec![vec![Some("it's \\ ok".to_string())]]);
        my.all("DROP DATABASE orior_test").unwrap();
        my.close();
    }
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Requests: a file of HTTP requests, `.http` or `.rest`, written as JetBrains' HTTP Client writes
//! them, each sent from this process and its answers kept.
//!
//! A file holds requests one after another, each starting at a line of `###`, the rest of which
//! names it, or at the top of the file. A request is its request line, the method and the address,
//! GET where no method is written, an address that runs on in indented lines; the lines of its
//! headers under it; and after a blank line its body, or `< path` for a body read from a file beside
//! it. A line of `#` or `//` before the body is a comment, `# @name x` names the request, and
//! `# @no-redirect` keeps it from following a redirect. A line `@name = value` outside a request's
//! body sets a variable of the file, and a line of `> ` after a body, a handler of JetBrains' that
//! runs a script, is passed over: a request here runs nothing.
//!
//! `{{name}}` stands for the value of a variable: of the file, or of the environment chosen, read
//! from http-client.env.json and the reader's own http-client.private.env.json, each the nearest
//! beside the file or in a folder above it within the tree, the private values over the shared ones
//! and `$shared` giving values to every environment; or it is one of `{{$uuid}}`, `{{$timestamp}}`,
//! `{{$isoTimestamp}}` and `{{$randomInt}}`. The private file is the reader's alone: orior adds its
//! name to the tree's .gitignore as it makes it.
//!
//! A request goes out over HTTP/1.1 from this process, through the system's TLS for https (tls.rs),
//! and its answer is read whole: chunked, gzip and deflate are undone, and a redirect is followed up
//! to ten times. Each request's last answers are kept in orior's own folder, the last first, each
//! with what was sent, for the reader to look back on and compare.
//!
//! A curl line becomes a request, and a request a curl line.

use std::collections::HashMap;
use std::io::{Read, Write};
use std::net::{TcpStream, ToSocketAddrs};
use std::path::{Path, PathBuf};
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

use serde::{Deserialize, Serialize};

use crate::serve::base64;
use crate::tls::Tls;
use crate::{archive, digest, home, root};

/// How long a connection may take to open, and an answer to come.
const CONNECTING: Duration = Duration::from_secs(15);
const ANSWERING: Duration = Duration::from_secs(60);

/// The most redirects one request follows.
const MOST_REDIRECTS: usize = 10;

/// The most of an answer's body read, and the most of it kept with the answers.
const MOST_BODY: usize = 32 << 20;
const MOST_KEPT: usize = 1 << 20;

/// How many answers each request keeps.
const KEPT: usize = 10;

/// Held while an answer is kept, so that two answers kept at once are both kept.
static KEEPING: std::sync::Mutex<()> = std::sync::Mutex::new(());

pub const PUBLIC: &str = "http-client.env.json";
pub const PRIVATE: &str = "http-client.private.env.json";

/// A request of a file as it is written, its variables not yet filled in.
#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
pub struct Request {
    pub name: String,
    /// The first and the last line of the request's part of the file, and its request line, from 0.
    pub start: usize,
    pub end: usize,
    pub line: usize,
    pub method: String,
    pub url: String,
    pub headers: Vec<(String, String)>,
    pub body: Option<String>,
    pub body_file: Option<String>,
    pub redirect: bool,
}

impl Request {
    /// What the request's answers are kept under: its name, else its method and address as written.
    pub fn key(&self) -> String {
        if self.name.is_empty() {
            format!("{} {}", self.method, self.url)
        } else {
            self.name.clone()
        }
    }
}

/// A file of requests read: its requests in order and the variables it sets.
#[derive(Serialize, Debug, Default, PartialEq)]
pub struct Parsed {
    pub requests: Vec<Request>,
    pub variables: Vec<(String, String)>,
}

fn is_method(word: &str) -> bool {
    !word.is_empty() && word.chars().all(|c| c.is_ascii_uppercase())
}

/// The requests of the text of a file of requests, and the variables it sets.
pub fn parse(text: &str) -> Parsed {
    let lines: Vec<&str> = text.lines().collect();
    let mut parsed = Parsed::default();
    let mut starts = vec![0usize];
    starts.extend(lines.iter().enumerate().filter(|(_, line)| line.trim_start().starts_with("###")).map(|(at, _)| at));
    starts.dedup();
    for (index, &start) in starts.iter().enumerate() {
        let end = starts.get(index + 1).map(|next| next.saturating_sub(1)).unwrap_or(lines.len().saturating_sub(1));
        if start > end || start >= lines.len() {
            continue;
        }
        let mut request = Request { start, end, redirect: true, ..Request::default() };
        let mut at = start;
        if let Some(name) = lines[at].trim_start().strip_prefix("###") {
            request.name = name.trim().to_string();
            at += 1;
        }
        let mut found = false;
        while at <= end {
            let line = lines[at].trim();
            at += 1;
            if line.is_empty() {
                continue;
            }
            if let Some(comment) = line.strip_prefix('#').or_else(|| line.strip_prefix("//")) {
                let comment = comment.trim();
                if let Some(name) = comment.strip_prefix("@name") {
                    request.name = name.trim().trim_start_matches('=').trim().to_string();
                } else if comment == "@no-redirect" {
                    request.redirect = false;
                }
                continue;
            }
            if let Some(variable) = line.strip_prefix('@') {
                if let Some((name, value)) = variable.split_once('=') {
                    parsed.variables.push((name.trim().to_string(), value.trim().to_string()));
                }
                continue;
            }
            let words: Vec<&str> = line.split_whitespace().collect();
            let (method, url) = match words.as_slice() {
                [method, url, ..] if is_method(method) => (method.to_string(), url.to_string()),
                [url, ..] => ("GET".to_string(), url.to_string()),
                [] => continue,
            };
            request.method = method;
            request.url = url;
            request.line = at - 1;
            found = true;
            break;
        }
        if !found {
            continue;
        }
        while at <= end && lines[at].starts_with(char::is_whitespace) && !lines[at].trim().is_empty() {
            request.url.push_str(lines[at].trim());
            at += 1;
        }
        while at <= end && !lines[at].trim().is_empty() {
            let line = lines[at].trim();
            at += 1;
            if line.starts_with('#') || line.starts_with("//") {
                continue;
            }
            if let Some((name, value)) = line.split_once(':') {
                request.headers.push((name.trim().to_string(), value.trim().to_string()));
            }
        }
        let mut body: Vec<&str> = lines[at.min(lines.len())..=end.min(lines.len().saturating_sub(1))].iter().copied().collect();
        if at > end {
            body.clear();
        }
        while body.first().is_some_and(|line| line.trim().is_empty()) {
            body.remove(0);
        }
        if let Some(handler) = body.iter().position(|line| line.starts_with("> ") || line.starts_with(">> ")) {
            body.truncate(handler);
        }
        while body.last().is_some_and(|line| line.trim().is_empty()) {
            body.pop();
        }
        if body.len() == 1 && body[0].starts_with("< ") {
            request.body_file = Some(body[0][2..].trim().to_string());
        } else if !body.is_empty() {
            request.body = Some(body.join("\n"));
        }
        parsed.requests.push(request);
    }
    parsed
}

/// The request whose part of the text holds line `line`, or the last before it.
pub fn request_at(parsed: &Parsed, line: usize) -> Option<&Request> {
    parsed.requests.iter().rev().find(|request| request.start <= line).or_else(|| parsed.requests.first())
}

/// The environment files of the file at `path`: the names of their environments, and the tree's
/// paths of the shared file and the private one, each the nearest beside the file or above it.
#[derive(Serialize, Debug, Default, PartialEq)]
pub struct Environments {
    pub names: Vec<String>,
    pub shared: Option<String>,
    pub private: Option<String>,
}

fn nearest(root: &Path, path: &str, name: &str) -> Option<PathBuf> {
    let file = root::inside(root, path).ok()?;
    file.parent()?.ancestors().take_while(|dir| dir.starts_with(root)).map(|dir| dir.join(name)).find(|candidate| candidate.is_file())
}

fn read_json(file: &Path) -> Result<serde_json::Map<String, serde_json::Value>, String> {
    let text = std::fs::read_to_string(file).map_err(|error| format!("{}: {error}", file.display()))?;
    match serde_json::from_str(&text).map_err(|error| format!("{}: {error}", file.display()))? {
        serde_json::Value::Object(map) => Ok(map),
        _ => Err(format!("{} holds no object of environments", file.display())),
    }
}

pub fn environments(root: &Path, path: &str) -> Result<Environments, String> {
    let mut out = Environments::default();
    for (name, slot) in [(PUBLIC, 0), (PRIVATE, 1)] {
        let Some(file) = nearest(root, path, name) else {
            continue;
        };
        for env in read_json(&file)?.keys() {
            if env != "$shared" && !out.names.contains(env) {
                out.names.push(env.clone());
            }
        }
        let relative = root::relative(root, &file);
        if slot == 0 {
            out.shared = Some(relative);
        } else {
            out.private = Some(relative);
        }
    }
    Ok(out)
}

fn text_of(value: &serde_json::Value) -> String {
    value.as_str().map(str::to_string).unwrap_or_else(|| value.to_string())
}

/// The values of the variables of environment `env`, or of none, for the file at `path`: the shared
/// file's then the private file's, `$shared` under the environment's own.
pub fn values(root: &Path, path: &str, env: Option<&str>) -> Result<HashMap<String, String>, String> {
    let mut out = HashMap::new();
    for name in [PUBLIC, PRIVATE] {
        let Some(file) = nearest(root, path, name) else {
            continue;
        };
        let read = read_json(&file)?;
        for group in std::iter::once("$shared").chain(env) {
            if let Some(serde_json::Value::Object(values)) = read.get(group) {
                for (key, value) in values {
                    out.insert(key.clone(), text_of(value));
                }
            }
        }
    }
    Ok(out)
}

fn random() -> u64 {
    use std::hash::{BuildHasher, Hasher};
    let mut hasher = std::collections::hash_map::RandomState::new().build_hasher();
    hasher.write_u128(SystemTime::now().duration_since(UNIX_EPOCH).map(|time| time.as_nanos()).unwrap_or(0));
    hasher.finish()
}

/// The date and time `seconds` after 1970 began, as ISO 8601 in UTC.
fn iso(seconds: u64) -> String {
    let days = (seconds / 86_400) as i64;
    let rest = seconds % 86_400;
    let shifted = days + 719_468;
    let era = shifted.div_euclid(146_097);
    let day_of_era = shifted - era * 146_097;
    let year_of_era = (day_of_era - day_of_era / 1460 + day_of_era / 36_524 - day_of_era / 146_096) / 365;
    let day_of_year = day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
    let month_index = (5 * day_of_year + 2) / 153;
    let day = day_of_year - (153 * month_index + 2) / 5 + 1;
    let month = if month_index < 10 { month_index + 3 } else { month_index - 9 };
    let year = year_of_era + era * 400 + i64::from(month <= 2);
    format!("{year:04}-{month:02}-{day:02}T{:02}:{:02}:{:02}Z", rest / 3600, rest % 3600 / 60, rest % 60)
}

fn dynamic(name: &str) -> Option<String> {
    let now = SystemTime::now().duration_since(UNIX_EPOCH).unwrap_or_default();
    match name {
        "$uuid" | "$random.uuid" => {
            let (a, b) = (random(), random());
            let mut bytes = [0u8; 16];
            bytes[..8].copy_from_slice(&a.to_be_bytes());
            bytes[8..].copy_from_slice(&b.to_be_bytes());
            bytes[6] = (bytes[6] & 0x0f) | 0x40;
            bytes[8] = (bytes[8] & 0x3f) | 0x80;
            let hex: String = bytes.iter().map(|byte| format!("{byte:02x}")).collect();
            Some(format!("{}-{}-{}-{}-{}", &hex[..8], &hex[8..12], &hex[12..16], &hex[16..20], &hex[20..]))
        }
        "$timestamp" => Some(now.as_secs().to_string()),
        "$isoTimestamp" => Some(iso(now.as_secs())),
        "$randomInt" => Some((random() % 1000).to_string()),
        _ => None,
    }
}

/// `text` with each `{{name}}` in it given its value from `values`, a value that names others given
/// theirs in turn.
pub fn fill(text: &str, values: &HashMap<String, String>) -> Result<String, String> {
    fn go(text: &str, values: &HashMap<String, String>, depth: usize) -> Result<String, String> {
        let mut out = String::new();
        let mut rest = text;
        while let Some(open) = rest.find("{{") {
            let Some(close) = rest[open + 2..].find("}}") else {
                break;
            };
            out.push_str(&rest[..open]);
            let name = rest[open + 2..open + 2 + close].trim();
            let value = match values.get(name) {
                Some(value) if depth < 8 => go(value, values, depth + 1)?,
                Some(value) => value.clone(),
                None => dynamic(name).ok_or_else(|| format!("{{{{{name}}}}} is set by no variable of the file or of the environment chosen"))?,
            };
            out.push_str(&value);
            rest = &rest[open + 2 + close + 2..];
        }
        out.push_str(rest);
        Ok(out)
    }
    go(text, values, 0)
}

/// A request ready to go: its variables filled in and its body read.
#[derive(Serialize, Deserialize, Debug, Clone, PartialEq, Default)]
pub struct Sent {
    pub method: String,
    pub url: String,
    pub headers: Vec<(String, String)>,
    pub body: String,
    pub redirect: bool,
}

/// The request at line `line` of the file at `path`, whose text is `text`, made ready with the
/// variables of the file and of environment `env`.
pub fn ready(root: &Path, path: &str, text: &str, line: usize, env: Option<&str>) -> Result<(Request, Sent), String> {
    let parsed = parse(text);
    let request = request_at(&parsed, line).cloned().ok_or("the file holds no request")?;
    let mut known = values(root, path, env)?;
    for (name, value) in &parsed.variables {
        known.insert(name.clone(), value.clone());
    }
    let mut url = fill(&request.url, &known)?;
    if !url.contains("://") {
        url = format!("http://{url}");
    }
    let headers = request.headers.iter().map(|(name, value)| Ok((fill(name, &known)?, fill(value, &known)?))).collect::<Result<Vec<_>, String>>()?;
    let body = match (&request.body, &request.body_file) {
        (Some(body), _) => fill(body, &known)?,
        (None, Some(file)) => {
            let beside = root::inside(root, path)?.parent().map(|dir| dir.join(file)).ok_or("the file has no folder")?;
            std::fs::read_to_string(&beside).map_err(|error| format!("{file}: {error}"))?
        }
        (None, None) => String::new(),
    };
    Ok((request.clone(), Sent { method: request.method.clone(), url, headers, body, redirect: request.redirect }))
}

/// How long each part of a request took, in milliseconds: finding the host, opening the connection,
/// TLS, waiting for the first byte of the answer, and reading the rest.
#[derive(Serialize, Deserialize, Debug, Clone, Default, PartialEq)]
pub struct Times {
    pub resolve: f64,
    pub connect: f64,
    pub tls: f64,
    pub wait: f64,
    pub read: f64,
    pub total: f64,
}

/// An answer: when it came, what was sent, the status and headers, the body as text or, where it is
/// not text, base64, its size, how long each part took, the redirects followed on the way, and what
/// failed where nothing answered.
#[derive(Serialize, Deserialize, Debug, Clone, Default, PartialEq)]
pub struct Answer {
    pub at: u64,
    pub sent: Sent,
    pub url: String,
    pub status: u16,
    pub reason: String,
    pub version: String,
    pub headers: Vec<(String, String)>,
    pub body: String,
    pub binary: bool,
    pub size: usize,
    pub cut: bool,
    pub times: Times,
    pub redirects: Vec<(u16, String)>,
    pub failed: Option<String>,
}

trait Stream: Read + Write + Send {}
impl Stream for TcpStream {}
impl Stream for Tls {}

/// An address taken apart: whether it is https, its host, its port and the path with its query.
struct Address {
    secure: bool,
    host: String,
    port: u16,
    target: String,
}

fn address(url: &str) -> Result<Address, String> {
    let (scheme, rest) = url.split_once("://").ok_or_else(|| format!("{url} names no scheme"))?;
    let secure = match scheme.to_ascii_lowercase().as_str() {
        "http" => false,
        "https" => true,
        other => return Err(format!("{other} is not http or https")),
    };
    let (authority, target) = match rest.find(['/', '?', '#']) {
        Some(at) => (&rest[..at], &rest[at..]),
        None => (rest, "/"),
    };
    let authority = authority.rsplit('@').next().unwrap_or(authority);
    let target = target.split('#').next().unwrap_or("/");
    let target = if target.starts_with('?') { format!("/{target}") } else { target.to_string() };
    let (host, port) = if let Some(inner) = authority.strip_prefix('[') {
        let (host, after) = inner.split_once(']').ok_or_else(|| format!("{url} has an address that does not close"))?;
        (host.to_string(), after.strip_prefix(':').map(str::to_string))
    } else {
        match authority.rsplit_once(':') {
            Some((host, port)) => (host.to_string(), Some(port.to_string())),
            None => (authority.to_string(), None),
        }
    };
    let port = match port {
        Some(port) => port.parse().map_err(|_| format!("{port} is not a port"))?,
        None if secure => 443,
        None => 80,
    };
    if host.is_empty() {
        return Err(format!("{url} names no host"));
    }
    Ok(Address { secure, host, port, target: if target.is_empty() { "/".into() } else { target } })
}

/// The address `location` names, read from the address `base` it was given at.
fn joined(base: &str, location: &str) -> String {
    if location.contains("://") {
        return location.to_string();
    }
    let (scheme, rest) = base.split_once("://").unwrap_or(("http", base));
    let authority = rest.split(['/', '?', '#']).next().unwrap_or(rest);
    if let Some(path) = location.strip_prefix("//") {
        return format!("{scheme}://{path}");
    }
    if location.starts_with('/') {
        return format!("{scheme}://{authority}{location}");
    }
    let path = &rest[authority.len()..];
    let path = path.split(['?', '#']).next().unwrap_or("");
    let folder = &path[..path.rfind('/').map(|at| at + 1).unwrap_or(0)];
    format!("{scheme}://{authority}{}{location}", if folder.is_empty() { "/" } else { folder })
}

fn header<'a>(headers: &'a [(String, String)], name: &str) -> Option<&'a str> {
    headers.iter().find(|(key, _)| key.eq_ignore_ascii_case(name)).map(|(_, value)| value.as_str())
}

/// A connection's bytes, read as they are wanted.
struct Reading {
    stream: Box<dyn Stream>,
    buffer: Vec<u8>,
}

impl Reading {
    fn fill(&mut self) -> Result<bool, String> {
        let mut chunk = [0u8; 16 * 1024];
        match self.stream.read(&mut chunk) {
            Ok(0) => Ok(false),
            Ok(read) => {
                self.buffer.extend_from_slice(&chunk[..read]);
                Ok(true)
            }
            Err(error) if matches!(error.kind(), std::io::ErrorKind::WouldBlock | std::io::ErrorKind::TimedOut) => Err(format!("no answer came within {} s", ANSWERING.as_secs())),
            Err(error) => Err(error.to_string()),
        }
    }

    fn line(&mut self) -> Result<String, String> {
        loop {
            if let Some(at) = self.buffer.windows(2).position(|pair| pair == b"\r\n") {
                let line = String::from_utf8_lossy(&self.buffer[..at]).into_owned();
                self.buffer.drain(..at + 2);
                return Ok(line);
            }
            if self.buffer.len() > 64 * 1024 || !self.fill()? {
                return Err("the answer ended in its headers".into());
            }
        }
    }

    fn exactly(&mut self, length: usize, out: &mut Vec<u8>, cut: &mut bool) -> Result<(), String> {
        let mut left = length;
        while left > 0 {
            if self.buffer.is_empty() && !self.fill()? {
                return Err("the answer ended before its length".into());
            }
            let take = left.min(self.buffer.len());
            let room = MOST_BODY.saturating_sub(out.len());
            out.extend_from_slice(&self.buffer[..take.min(room)]);
            *cut |= take > room;
            self.buffer.drain(..take);
            left -= take;
        }
        Ok(())
    }
}

/// The body `body` with the encoding `encoding` taken off.
fn decoded(body: Vec<u8>, encoding: Option<&str>) -> Result<Vec<u8>, String> {
    match encoding.map(|one| one.trim().to_ascii_lowercase()).as_deref() {
        Some("gzip") | Some("x-gzip") => archive::gunzip(&body),
        Some("deflate") => {
            let zlib = body.len() > 2 && body[0] & 0x0f == 8 && (u16::from(body[0]) << 8 | u16::from(body[1])) % 31 == 0;
            archive::inflate(if zlib { &body[2..] } else { &body }).map(|(data, _)| data)
        }
        _ => Ok(body),
    }
}

fn is_text(content_type: Option<&str>, body: &[u8]) -> bool {
    let kind = content_type.unwrap_or("").to_ascii_lowercase();
    if kind.starts_with("text/") || ["json", "xml", "javascript", "x-www-form-urlencoded", "yaml", "csv", "graphql"].iter().any(|part| kind.contains(part)) {
        return true;
    }
    if kind.starts_with("image/") || kind.starts_with("audio/") || kind.starts_with("video/") || kind.contains("octet-stream") || kind.contains("pdf") || kind.contains("zip") {
        return false;
    }
    std::str::from_utf8(body).is_ok() && !body.contains(&0)
}

fn millis(since: Instant) -> f64 {
    (since.elapsed().as_secs_f64() * 10_000.0).round() / 10.0
}

/// Sends `sent` once, to its own address, and reads the whole answer.
fn once(method: &str, url: &str, headers: &[(String, String)], body: &[u8], verify: bool) -> Result<Answer, String> {
    let began = Instant::now();
    let to = address(url)?;
    let mut times = Times::default();
    let mut step = Instant::now();
    let places: Vec<_> = (to.host.as_str(), to.port).to_socket_addrs().map_err(|error| format!("{}: {error}", to.host))?.collect();
    times.resolve = millis(step);
    step = Instant::now();
    let mut last = format!("{} has no address", to.host);
    let socket = places
        .iter()
        .find_map(|place| match TcpStream::connect_timeout(place, CONNECTING) {
            Ok(socket) => Some(socket),
            Err(error) => {
                last = format!("{}:{}: {error}", to.host, to.port);
                None
            }
        })
        .ok_or(last)?;
    socket.set_read_timeout(Some(ANSWERING)).map_err(|error| error.to_string())?;
    socket.set_write_timeout(Some(ANSWERING)).map_err(|error| error.to_string())?;
    let _ = socket.set_nodelay(true);
    times.connect = millis(step);
    step = Instant::now();
    let mut stream: Box<dyn Stream> = if to.secure { Box::new(Tls::connect(socket, &to.host, verify)?) } else { Box::new(socket) };
    times.tls = if to.secure { millis(step) } else { 0.0 };
    let default_port = if to.secure { 443 } else { 80 };
    let host = if to.host.contains(':') { format!("[{}]", to.host) } else { to.host.clone() };
    let mut head = format!("{method} {} HTTP/1.1\r\n", to.target);
    let given = |name: &str, value: String, head: &mut String| {
        if header(headers, name).is_none() {
            head.push_str(&format!("{name}: {value}\r\n"));
        }
    };
    given("Host", if to.port == default_port { host } else { format!("{host}:{}", to.port) }, &mut head);
    for (name, value) in headers {
        head.push_str(&format!("{name}: {value}\r\n"));
    }
    given("User-Agent", format!("orior/{}", env!("CARGO_PKG_VERSION")), &mut head);
    given("Accept", "*/*".into(), &mut head);
    given("Accept-Encoding", "gzip, deflate".into(), &mut head);
    if (!body.is_empty() || matches!(method, "POST" | "PUT" | "PATCH")) && header(headers, "Transfer-Encoding").is_none() {
        given("Content-Length", body.len().to_string(), &mut head);
    }
    given("Connection", "close".into(), &mut head);
    head.push_str("\r\n");
    step = Instant::now();
    stream.write_all(head.as_bytes()).and_then(|_| stream.write_all(body)).map_err(|error| error.to_string())?;
    let mut reading = Reading { stream, buffer: Vec::new() };
    let (version, status, reason, answered) = loop {
        let line = reading.line()?;
        times.wait = millis(step);
        let mut parts = line.splitn(3, ' ');
        let version = parts.next().unwrap_or("").to_string();
        let status: u16 = parts.next().and_then(|code| code.parse().ok()).ok_or_else(|| format!("the answer began {line:?}, which is no status line"))?;
        let reason = parts.next().unwrap_or("").to_string();
        let mut answered = Vec::new();
        loop {
            let line = reading.line()?;
            if line.is_empty() {
                break;
            }
            if let Some((name, value)) = line.split_once(':') {
                answered.push((name.trim().to_string(), value.trim().to_string()));
            }
        }
        if (100..200).contains(&status) && status != 101 {
            continue;
        }
        break (version, status, reason, answered);
    };
    step = Instant::now();
    let mut raw = Vec::new();
    let mut cut = false;
    let empty = method == "HEAD" || status == 204 || status == 304 || (100..200).contains(&status);
    if empty {
    } else if header(&answered, "Transfer-Encoding").is_some_and(|value| value.to_ascii_lowercase().contains("chunked")) {
        loop {
            let size_line = reading.line()?;
            let size = usize::from_str_radix(size_line.split(';').next().unwrap_or("").trim(), 16).map_err(|_| format!("a chunk's size reads {size_line:?}"))?;
            if size == 0 {
                while !reading.line()?.is_empty() {}
                break;
            }
            reading.exactly(size, &mut raw, &mut cut)?;
            reading.line()?;
        }
    } else if let Some(length) = header(&answered, "Content-Length").and_then(|value| value.trim().parse::<usize>().ok()) {
        reading.exactly(length, &mut raw, &mut cut)?;
    } else {
        raw.append(&mut reading.buffer);
        while reading.fill()? {
            raw.append(&mut reading.buffer);
            if raw.len() > MOST_BODY {
                raw.truncate(MOST_BODY);
                cut = true;
                break;
            }
        }
    }
    times.read = millis(step);
    let body = if cut { raw } else { decoded(raw, header(&answered, "Content-Encoding"))? };
    let text = is_text(header(&answered, "Content-Type"), &body);
    times.total = millis(began);
    Ok(Answer {
        at: SystemTime::now().duration_since(UNIX_EPOCH).map(|time| time.as_millis() as u64).unwrap_or(0),
        url: url.to_string(),
        status,
        reason,
        version,
        size: body.len(),
        body: if text { String::from_utf8_lossy(&body).into_owned() } else { base64(&body) },
        binary: !text,
        headers: answered,
        cut,
        times,
        ..Answer::default()
    })
}

/// Sends `sent` and reads its answer, following redirects where it does; the peer's certificate is
/// checked where `verify`. A request that gets no answer gives one that says what failed.
pub fn send(sent: &Sent, verify: bool) -> Answer {
    let began = Instant::now();
    let mut method = sent.method.clone();
    let mut url = sent.url.clone();
    let mut headers = sent.headers.clone();
    let mut body = sent.body.clone().into_bytes();
    let mut redirects = Vec::new();
    loop {
        let mut answer = match once(&method, &url, &headers, &body, verify) {
            Ok(answer) => answer,
            Err(failed) => Answer { at: SystemTime::now().duration_since(UNIX_EPOCH).map(|time| time.as_millis() as u64).unwrap_or(0), url: url.clone(), failed: Some(failed), ..Answer::default() },
        };
        let location = header(&answer.headers, "Location").map(str::to_string);
        if sent.redirect && answer.failed.is_none() && matches!(answer.status, 301 | 302 | 303 | 307 | 308) && redirects.len() < MOST_REDIRECTS {
            if let Some(location) = location {
                redirects.push((answer.status, url.clone()));
                url = joined(&url, &location);
                if answer.status == 303 || (matches!(answer.status, 301 | 302) && method == "POST") {
                    method = "GET".into();
                    body.clear();
                    headers.retain(|(name, _)| !name.eq_ignore_ascii_case("Content-Type") && !name.eq_ignore_ascii_case("Content-Length"));
                }
                continue;
            }
        }
        answer.sent = sent.clone();
        answer.redirects = redirects;
        answer.times.total = millis(began);
        return answer;
    }
}

/// The folder in orior's own folder that keeps the answers of the requests of the file at `path`.
pub fn kept_folder(root: &Path, path: &str) -> Option<PathBuf> {
    let file = root::inside(root, path).ok()?;
    Some(home::folder()?.join("requests").join(&digest::sha256(file.to_string_lossy().as_bytes())[..16]))
}

fn kept_file(folder: &Path, key: &str) -> PathBuf {
    folder.join(format!("{}.json", &digest::sha256(key.as_bytes())[..16]))
}

/// The answers kept in `folder` for the request named `key`, the last first.
pub fn answers(folder: &Path, key: &str) -> Vec<Answer> {
    std::fs::read_to_string(kept_file(folder, key)).ok().and_then(|text| serde_json::from_str(&text).ok()).unwrap_or_default()
}

/// Keeps `answer` in `folder` first among the answers of the request named `key`, its body cut to
/// MOST_KEPT, and the last KEPT of them only.
pub fn keep(folder: &Path, key: &str, answer: &Answer) -> Result<(), String> {
    let _held = KEEPING.lock().unwrap_or_else(|held| held.into_inner());
    let file = kept_file(folder, key);
    let mut kept = answers(folder, key);
    let mut answer = answer.clone();
    if answer.body.len() > MOST_KEPT {
        let mut end = MOST_KEPT;
        while !answer.body.is_char_boundary(end) {
            end -= 1;
        }
        answer.body.truncate(end);
        answer.cut = true;
    }
    kept.insert(0, answer);
    kept.truncate(KEPT);
    if let Some(dir) = file.parent() {
        std::fs::create_dir_all(dir).map_err(|error| format!("{}: {error}", dir.display()))?;
    }
    std::fs::write(&file, serde_json::to_string(&kept).map_err(|error| error.to_string())?).map_err(|error| format!("{}: {error}", file.display()))
}

/// Whether the peer's certificate is checked for environment `env`: always, but where the environment
/// says `"SSLConfiguration": {"verifyHostCertificate": false}`, as JetBrains' files do.
pub fn verifies(root: &Path, path: &str, env: Option<&str>) -> bool {
    let Some(env) = env else {
        return true;
    };
    ![PUBLIC, PRIVATE].iter().filter_map(|name| nearest(root, path, name)).filter_map(|file| read_json(&file).ok()).any(|read| read.get(env).and_then(|values| values.get("SSLConfiguration")).and_then(|ssl| ssl.get("verifyHostCertificate")).and_then(serde_json::Value::as_bool) == Some(false))
}

/// The answer the request at line `line` of the file at `path` gets, kept with its last answers.
pub fn send_at(root: &Path, path: &str, text: &str, line: usize, env: Option<&str>) -> Result<Answer, String> {
    let (request, sent) = ready(root, path, text, line, env)?;
    let answer = send(&sent, verifies(root, path, env));
    keep(&kept_folder(root, path).ok_or("orior's own folder is not known")?, &request.key(), &answer)?;
    Ok(answer)
}

/// Makes the reader's private environment file beside the file at `path` where none is there,
/// an empty set of values for each environment the shared file names, and adds its name to the
/// tree's .gitignore. Gives its path in the tree.
pub fn make_private(root: &Path, path: &str) -> Result<String, String> {
    if let Some(found) = nearest(root, path, PRIVATE) {
        return Ok(root::relative(root, &found));
    }
    let folder = root::inside(root, path)?.parent().ok_or("the file has no folder")?.to_path_buf();
    let names = environments(root, path)?.names;
    let names = if names.is_empty() { vec!["dev".to_string()] } else { names };
    let object: serde_json::Map<String, serde_json::Value> = names.into_iter().map(|name| (name, serde_json::json!({}))).collect();
    let file = folder.join(PRIVATE);
    let ignore = root.join(".gitignore");
    let listed = std::fs::read_to_string(&ignore).unwrap_or_default();
    if !listed.lines().any(|line| line.trim() == PRIVATE || line.trim() == format!("/{PRIVATE}") || line.trim() == "*.private.env.json") {
        let joiner = if listed.is_empty() || listed.ends_with('\n') { "" } else { "\n" };
        std::fs::write(&ignore, format!("{listed}{joiner}{PRIVATE}\n")).map_err(|error| format!("{}: {error}", ignore.display()))?;
    }
    std::fs::write(&file, format!("{}\n", serde_json::to_string_pretty(&object).map_err(|error| error.to_string())?)).map_err(|error| format!("{}: {error}", file.display()))?;
    Ok(root::relative(root, &file))
}

/// The words of a shell line as bash reads them: quoted in single quotes, double quotes or `$'...'`,
/// a backslash taking the next character, and a backslash at the end of a line joining it to the
/// next.
fn words(line: &str) -> Result<Vec<String>, String> {
    let chars: Vec<char> = line.chars().collect();
    let mut out = Vec::new();
    let mut word = String::new();
    let mut started = false;
    let mut at = 0;
    while at < chars.len() {
        let c = chars[at];
        match c {
            '\\' if matches!(chars.get(at + 1), Some('\n')) || (chars.get(at + 1) == Some(&'\r') && chars.get(at + 2) == Some(&'\n')) => {
                at += if chars.get(at + 1) == Some(&'\r') { 3 } else { 2 };
                continue;
            }
            '\\' => {
                if let Some(&next) = chars.get(at + 1) {
                    word.push(next);
                    started = true;
                }
                at += 2;
                continue;
            }
            '\'' => {
                let end = chars[at + 1..].iter().position(|&one| one == '\'').ok_or("a quote does not close")?;
                word.extend(&chars[at + 1..at + 1 + end]);
                started = true;
                at += end + 2;
                continue;
            }
            '$' if chars.get(at + 1) == Some(&'\'') => {
                at += 2;
                while at < chars.len() && chars[at] != '\'' {
                    if chars[at] == '\\' && at + 1 < chars.len() {
                        at += 1;
                        word.push(match chars[at] {
                            'n' => '\n',
                            't' => '\t',
                            'r' => '\r',
                            '0' => '\0',
                            other => other,
                        });
                    } else {
                        word.push(chars[at]);
                    }
                    at += 1;
                }
                started = true;
                at += 1;
                continue;
            }
            '"' => {
                at += 1;
                while at < chars.len() && chars[at] != '"' {
                    if chars[at] == '\\' && matches!(chars.get(at + 1), Some('"' | '\\' | '$' | '`')) {
                        at += 1;
                    }
                    word.push(chars[at]);
                    at += 1;
                }
                if at >= chars.len() {
                    return Err("a quote does not close".into());
                }
                started = true;
                at += 1;
                continue;
            }
            c if c.is_whitespace() => {
                if started {
                    out.push(std::mem::take(&mut word));
                    started = false;
                }
            }
            c => {
                word.push(c);
                started = true;
            }
        }
        at += 1;
    }
    if started {
        out.push(word);
    }
    Ok(out)
}

fn url_encoded(text: &str) -> String {
    text.bytes().map(|byte| if byte.is_ascii_alphanumeric() || b"-_.~".contains(&byte) { (byte as char).to_string() } else { format!("%{byte:02X}") }).collect()
}

/// A line copied for cmd as bash reads it: each caret takes the character after it as it is, and a
/// caret at the end of a line joins it to the next.
fn uncaret(line: &str) -> String {
    let mut out = String::new();
    let mut chars = line.chars().peekable();
    while let Some(c) = chars.next() {
        match (c, chars.peek()) {
            ('^', Some('\r')) => {
                chars.next();
                chars.next_if_eq(&'\n');
            }
            ('^', Some('\n')) => {
                chars.next();
            }
            ('^', Some(_)) => out.extend(chars.next()),
            _ => out.push(c),
        }
    }
    out
}

/// The request a curl line sends, written as a request of a file. A line copied for cmd, its lines
/// ending in carets, is read as cmd reads it.
pub fn from_curl(line: &str) -> Result<String, String> {
    let line = line.trim();
    let cmd = line.contains("^\n") || line.contains("^\r\n") || line.contains("^\"");
    let words = words(&if cmd { uncaret(line) } else { line.to_string() })?;
    if words.first().map(String::as_str) != Some("curl") {
        return Err("the text is no curl line".into());
    }
    let mut method: Option<String> = None;
    let mut url: Option<String> = None;
    let mut headers: Vec<(String, String)> = Vec::new();
    let mut data: Vec<String> = Vec::new();
    let mut forms: Vec<(String, String)> = Vec::new();
    let mut get = false;
    let mut at = 1;
    let next = |at: &mut usize, flag: &str| -> Result<String, String> {
        *at += 1;
        words.get(*at).cloned().ok_or_else(|| format!("{flag} wants a value"))
    };
    while at < words.len() {
        let word = words[at].clone();
        let (flag, joined) = match word.split_once('=') {
            Some((flag, value)) if flag.starts_with("--") => (flag.to_string(), Some(value.to_string())),
            _ => (word.clone(), None),
        };
        let value = |at: &mut usize| -> Result<String, String> {
            match &joined {
                Some(value) => Ok(value.clone()),
                None => next(at, &flag),
            }
        };
        match flag.as_str() {
            "-X" | "--request" => method = Some(value(&mut at)?),
            "-H" | "--header" => {
                let given = value(&mut at)?;
                if let Some((name, text)) = given.split_once(':') {
                    headers.push((name.trim().to_string(), text.trim().to_string()));
                }
            }
            "-d" | "--data" | "--data-raw" | "--data-binary" | "--data-ascii" => data.push(value(&mut at)?),
            "--data-urlencode" => {
                let given = value(&mut at)?;
                data.push(match given.split_once('=') {
                    Some((name, text)) => format!("{name}={}", url_encoded(text)),
                    None => url_encoded(&given),
                });
            }
            "--json" => {
                data.push(value(&mut at)?);
                for (name, text) in [("Content-Type", "application/json"), ("Accept", "application/json")] {
                    if header(&headers, name).is_none() {
                        headers.push((name.into(), text.into()));
                    }
                }
            }
            "-F" | "--form" => {
                let given = value(&mut at)?;
                if let Some((name, text)) = given.split_once('=') {
                    forms.push((name.to_string(), text.to_string()));
                }
            }
            "-u" | "--user" => headers.push(("Authorization".into(), format!("Basic {}", base64(value(&mut at)?.as_bytes())))),
            "-A" | "--user-agent" => headers.push(("User-Agent".into(), value(&mut at)?)),
            "-e" | "--referer" => headers.push(("Referer".into(), value(&mut at)?)),
            "-b" | "--cookie" => headers.push(("Cookie".into(), value(&mut at)?)),
            "-G" | "--get" => get = true,
            "-I" | "--head" => method = Some("HEAD".into()),
            "--url" => url = Some(value(&mut at)?),
            "-o" | "--output" | "-w" | "--write-out" | "--connect-timeout" | "-m" | "--max-time" | "--retry" | "-x" | "--proxy" | "--cacert" | "--cert" | "--key" | "-c" | "--cookie-jar" => {
                value(&mut at)?;
            }
            other if other.starts_with('-') => {}
            _ => url = Some(word),
        }
        at += 1;
    }
    let mut url = url.ok_or("the curl line names no address")?;
    let mut body = data.join("&");
    if get && !body.is_empty() {
        url.push(if url.contains('?') { '&' } else { '?' });
        url.push_str(&body);
        body.clear();
    }
    let mut out_headers = headers;
    if !forms.is_empty() {
        let boundary = "WebAppBoundary";
        out_headers.retain(|(name, _)| !name.eq_ignore_ascii_case("Content-Type"));
        out_headers.push(("Content-Type".into(), format!("multipart/form-data; boundary={boundary}")));
        let mut parts = String::new();
        for (name, text) in &forms {
            parts.push_str(&format!("--{boundary}\n"));
            match text.strip_prefix('@') {
                Some(file) => {
                    let file_name = Path::new(file).file_name().map(|one| one.to_string_lossy().into_owned()).unwrap_or_default();
                    parts.push_str(&format!("Content-Disposition: form-data; name=\"{name}\"; filename=\"{file_name}\"\n\n< {file}\n"));
                }
                None => parts.push_str(&format!("Content-Disposition: form-data; name=\"{name}\"\n\n{text}\n")),
            }
        }
        parts.push_str(&format!("--{boundary}--"));
        body = parts;
    } else if !body.is_empty() && header(&out_headers, "Content-Type").is_none() {
        out_headers.push(("Content-Type".into(), "application/x-www-form-urlencoded".into()));
    }
    let method = method.unwrap_or_else(|| if body.is_empty() { "GET".into() } else { "POST".into() });
    let mut out = format!("{method} {url}\n");
    for (name, text) in &out_headers {
        out.push_str(&format!("{name}: {text}\n"));
    }
    if !body.is_empty() {
        out.push('\n');
        if let Some(file) = body.strip_prefix('@').filter(|file| !file.contains('\n')) {
            out.push_str(&format!("< {file}"));
        } else {
            out.push_str(&body);
        }
        out.push('\n');
    }
    Ok(out)
}

fn quoted(text: &str) -> String {
    format!("'{}'", text.replace('\'', "'\\''"))
}

/// A curl line that sends `sent`, for bash and the shells that read quotes as it does.
pub fn to_curl(sent: &Sent) -> String {
    let mut parts = vec!["curl".to_string()];
    let default = if sent.body.is_empty() { "GET" } else { "POST" };
    if sent.method == "HEAD" {
        parts.push("--head".into());
    } else if sent.method != default {
        parts.push(format!("-X {}", sent.method));
    }
    parts.push(quoted(&sent.url));
    for (name, value) in &sent.headers {
        parts.push(format!("-H {}", quoted(&format!("{name}: {value}"))));
    }
    if !sent.body.is_empty() {
        parts.push(format!("--data-raw {}", quoted(&sent.body)));
    }
    if sent.redirect {
        parts.push("-L".into());
    }
    parts.join(" \\\n  ")
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::BufRead;
    use std::net::TcpListener;
    use std::process::Command;

    fn scratch(name: &str) -> PathBuf {
        let root = std::env::temp_dir().join(format!("orior_ui_requests_{name}_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&root);
        std::fs::create_dir_all(root.join("api/v1")).unwrap();
        dunce::canonicalize(&root).unwrap()
    }

    const FILE: &str = "@base = http://{{host}}/v1\n\n### List them\nGET {{base}}/items\n    ?page=2\nAccept: application/json\n# a comment\n\n###\n# @name make\n# @no-redirect\nPOST {{base}}/items\nContent-Type: application/json\n\n{\"name\": \"{{who}}\"}\n\n> {% client.test(); %}\n\n### From a file\nhttp://example.com/upload\n\n< ./body.json\n";

    #[test]
    fn a_file_s_requests_are_read_with_their_names_lines_headers_and_bodies() {
        let parsed = parse(FILE);
        assert_eq!(parsed.variables, vec![("base".to_string(), "http://{{host}}/v1".to_string())]);
        let [list, make, upload] = parsed.requests.as_slice() else { panic!("{:?}", parsed.requests) };
        assert_eq!((list.name.as_str(), list.method.as_str(), list.url.as_str(), list.line), ("List them", "GET", "{{base}}/items?page=2", 3));
        assert_eq!(list.headers, vec![("Accept".to_string(), "application/json".to_string())]);
        assert_eq!(list.body, None);
        assert_eq!((make.name.as_str(), make.redirect, make.body.as_deref()), ("make", false, Some("{\"name\": \"{{who}}\"}")));
        assert_eq!((upload.method.as_str(), upload.body_file.as_deref()), ("GET", Some("./body.json")));
        assert_eq!(request_at(&parsed, 14).map(|one| one.name.as_str()), Some("make"));
        assert_eq!(request_at(&parsed, 0).map(|one| one.name.as_str()), Some("List them"), "above the first, the first");
        assert_eq!(make.key(), "make");
        assert_eq!(upload.key(), "From a file");
        assert_eq!(parse("http://example.com/upload").requests[0].key(), "GET http://example.com/upload", "one with no name by its method and address");
    }

    #[test]
    fn variables_come_from_the_file_and_the_environment_chosen_the_private_values_over_the_shared() {
        let root = scratch("values");
        std::fs::write(root.join("api/http-client.env.json"), "{\"$shared\": {\"who\": \"all\"}, \"dev\": {\"host\": \"localhost:1\", \"token\": \"none\"}, \"prod\": {\"host\": \"example.com\"}}").unwrap();
        std::fs::write(root.join(PRIVATE), "{\"dev\": {\"token\": \"secret\"}}").unwrap();
        let envs = environments(&root, "api/v1/calls.http").unwrap();
        assert_eq!(envs.names, vec!["dev".to_string(), "prod".to_string()]);
        assert_eq!((envs.shared.as_deref(), envs.private.as_deref()), (Some("api/http-client.env.json"), Some(PRIVATE)));
        let known = values(&root, "api/v1/calls.http", Some("dev")).unwrap();
        assert_eq!((known["host"].as_str(), known["token"].as_str(), known["who"].as_str()), ("localhost:1", "secret", "all"));
        assert!(verifies(&root, "api/v1/calls.http", Some("dev")));
        std::fs::write(root.join(PRIVATE), "{\"dev\": {\"token\": \"secret\", \"SSLConfiguration\": {\"verifyHostCertificate\": false}}}").unwrap();
        assert!(!verifies(&root, "api/v1/calls.http", Some("dev")) && verifies(&root, "api/v1/calls.http", Some("prod")));
        let (request, sent) = ready(&root, "api/v1/calls.http", FILE, 14, Some("dev")).unwrap();
        assert_eq!(request.name, "make");
        assert_eq!(sent.url, "http://localhost:1/v1/items");
        assert_eq!(sent.body, "{\"name\": \"all\"}");
        assert!(ready(&root, "api/v1/calls.http", FILE, 14, None).unwrap_err().contains("{{host}}"), "no environment, no host");
        let made = fill("{{$uuid}} {{$timestamp}}", &HashMap::new()).unwrap();
        assert!(made.len() > 40 && made.as_bytes()[14] == b'4', "{made}");
        assert_eq!(iso(0), "1970-01-01T00:00:00Z");
        assert_eq!(iso(1_760_000_000), "2025-10-09T08:53:20Z");
        let _ = std::fs::remove_dir_all(&root);
    }

    #[test]
    fn the_private_file_is_made_beside_the_file_and_named_in_the_gitignore() {
        let root = scratch("private");
        std::fs::write(root.join("api/http-client.env.json"), "{\"local\": {}, \"stage\": {}}").unwrap();
        std::fs::write(root.join(".gitignore"), "target").unwrap();
        let made = make_private(&root, "api/calls.http").unwrap();
        assert_eq!(made, format!("api/{PRIVATE}"));
        let written: serde_json::Value = serde_json::from_str(&std::fs::read_to_string(root.join(&made)).unwrap()).unwrap();
        assert_eq!(written, serde_json::json!({"local": {}, "stage": {}}));
        assert_eq!(std::fs::read_to_string(root.join(".gitignore")).unwrap(), format!("target\n{PRIVATE}\n"));
        assert_eq!(make_private(&root, "api/calls.http").unwrap(), made, "the one there is kept");
        assert_eq!(std::fs::read_to_string(root.join(".gitignore")).unwrap().matches(PRIVATE).count(), 1);
        let _ = std::fs::remove_dir_all(&root);
    }

    #[test]
    fn a_curl_line_becomes_a_request_and_a_request_a_curl_line() {
        let line = "curl 'https://api.example.com/v1/items?x=1' \\\n  -H 'Accept: application/json' \\\n  -H \"Authorization: Bearer abc\" \\\n  --data-raw $'{\"a\":\"it\\'s\"}' \\\n  --compressed";
        let text = from_curl(line).unwrap();
        assert_eq!(text, "POST https://api.example.com/v1/items?x=1\nAccept: application/json\nAuthorization: Bearer abc\nContent-Type: application/x-www-form-urlencoded\n\n{\"a\":\"it's\"}\n");
        assert_eq!(from_curl("curl -G https://h/s -d q=a -d n=2").unwrap(), "GET https://h/s?q=a&n=2\n");
        assert_eq!(from_curl("curl -u me:pw -X DELETE http://h/x").unwrap(), "DELETE http://h/x\nAuthorization: Basic bWU6cHc=\n");
        assert!(from_curl("curl -F name=me -F file=@a.png http://h/up").unwrap().contains("Content-Disposition: form-data; name=\"file\"; filename=\"a.png\"\n\n< a.png\n"));
        assert_eq!(from_curl("curl ^\n  \"http://h/x\" ^\n  -H ^\"A: b^\"").unwrap(), "GET http://h/x\nA: b\n", "a line copied for cmd");
        assert!(from_curl("wget http://h").is_err());
        let sent = Sent { method: "PUT".into(), url: "http://h/x".into(), headers: vec![("A".into(), "it's".into())], body: "{}".into(), redirect: false };
        let back = to_curl(&sent);
        assert_eq!(back, "curl \\\n  -X PUT \\\n  'http://h/x' \\\n  -H 'A: it'\\''s' \\\n  --data-raw '{}'");
        assert_eq!(from_curl(&back).unwrap(), "PUT http://h/x\nA: it's\nContent-Type: application/x-www-form-urlencoded\n\n{}\n");
    }

    /// A server that answers each connection with the next of `answers`, and tells what it was sent.
    fn serve(answers: Vec<Vec<u8>>) -> (u16, std::sync::mpsc::Receiver<String>) {
        let listener = TcpListener::bind("127.0.0.1:0").unwrap();
        let port = listener.local_addr().unwrap().port();
        let (tell, told) = std::sync::mpsc::channel();
        std::thread::spawn(move || {
            for answer in answers {
                let (mut socket, _) = listener.accept().unwrap();
                let mut reader = std::io::BufReader::new(socket.try_clone().unwrap());
                let mut head = String::new();
                loop {
                    let mut line = String::new();
                    reader.read_line(&mut line).unwrap();
                    head.push_str(&line);
                    if line == "\r\n" || line.is_empty() {
                        break;
                    }
                }
                let length = head.lines().find_map(|line| line.to_ascii_lowercase().strip_prefix("content-length:").map(|value| value.trim().parse::<usize>().unwrap())).unwrap_or(0);
                let mut body = vec![0u8; length];
                reader.read_exact(&mut body).unwrap();
                tell.send(format!("{head}{}", String::from_utf8_lossy(&body))).unwrap();
                socket.write_all(&answer).unwrap();
            }
        });
        (port, told)
    }

    #[test]
    fn a_request_is_sent_and_its_answer_read_whole_chunked_and_gzipped() {
        let json = archive::gzip(b"{\"ok\": true}");
        let mut chunked = b"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Encoding: gzip\r\nTransfer-Encoding: chunked\r\n\r\n".to_vec();
        let (first, second) = json.split_at(7);
        for part in [first, second] {
            chunked.extend(format!("{:x};x=y\r\n", part.len()).as_bytes());
            chunked.extend(part);
            chunked.extend(b"\r\n");
        }
        chunked.extend(b"0\r\nTrailer: 1\r\n\r\n");
        let (port, told) = serve(vec![chunked]);
        let sent = Sent { method: "POST".into(), url: format!("http://127.0.0.1:{port}/items?x=1"), headers: vec![("X-One".into(), "1".into())], body: "hello".into(), redirect: true };
        let answer = send(&sent, true);
        assert_eq!(answer.failed, None);
        assert_eq!((answer.status, answer.reason.as_str(), answer.body.as_str(), answer.binary), (200, "OK", "{\"ok\": true}", false));
        let request = told.recv().unwrap();
        assert!(request.starts_with("POST /items?x=1 HTTP/1.1\r\nHost: 127.0.0.1:"), "{request}");
        assert!(request.contains("X-One: 1\r\n") && request.contains("Content-Length: 5\r\n") && request.ends_with("\r\n\r\nhello"), "{request}");
        assert!(answer.times.total >= answer.times.wait);
    }

    #[test]
    fn a_redirect_is_followed_a_303_as_a_get_and_not_where_the_request_says() {
        let (port, told) = serve(vec![b"HTTP/1.1 303 See Other\r\nLocation: /done\r\nContent-Length: 0\r\n\r\n".to_vec(), b"HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\ndone".to_vec()]);
        let sent = Sent { method: "POST".into(), url: format!("http://127.0.0.1:{port}/start"), headers: vec![("Content-Type".into(), "text/plain".into())], body: "x".into(), redirect: true };
        let answer = send(&sent, true);
        assert_eq!((answer.status, answer.body.as_str(), answer.redirects.len()), (200, "done", 1));
        let _ = told.recv().unwrap();
        assert!(told.recv().unwrap().starts_with("GET /done HTTP/1.1"));
        let (port, _told) = serve(vec![b"HTTP/1.1 302 Found\r\nContent-Type: text/plain\r\nLocation: /echo\r\nContent-Length: 0\r\nConnection: close\r\n\r\n".to_vec(), b"HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok".to_vec()]);
        let moved = send(&Sent { method: "GET".into(), url: format!("http://127.0.0.1:{port}/moved"), redirect: true, ..Sent::default() }, true);
        assert_eq!((moved.status, moved.body.as_str(), moved.redirects.clone()), (200, "ok", vec![(302, format!("http://127.0.0.1:{port}/moved"))]));
        let (port, _told) = serve(vec![b"HTTP/1.1 302 Found\r\nLocation: http://127.0.0.1:1/x\r\nContent-Length: 0\r\n\r\n".to_vec()]);
        let kept = send(&Sent { method: "GET".into(), url: format!("http://127.0.0.1:{port}/"), redirect: false, ..Sent::default() }, true);
        assert_eq!((kept.status, kept.redirects.len()), (302, 0));
        assert_eq!(joined("http://h:1/a/b?c", "d"), "http://h:1/a/d");
        assert_eq!(joined("https://h/a/b", "//o/p"), "https://o/p");
    }

    #[test]
    fn an_answer_without_a_length_is_read_to_its_end_and_one_that_is_not_text_given_as_base64() {
        let (port, _told) = serve(vec![b"HTTP/1.1 200 OK\r\nContent-Type: image/png\r\n\r\n\x89PNG\x00\x01".to_vec()]);
        let answer = send(&Sent { method: "GET".into(), url: format!("http://127.0.0.1:{port}/i.png"), redirect: true, ..Sent::default() }, true);
        assert_eq!((answer.binary, answer.size, answer.body.as_str()), (true, 6, base64(b"\x89PNG\x00\x01").as_str()));
        let refused = send(&Sent { method: "GET".into(), url: "http://127.0.0.1:1/".into(), redirect: true, ..Sent::default() }, true);
        assert!(refused.failed.is_some() && refused.status == 0);
    }

    #[test]
    fn each_request_keeps_its_last_answers_the_last_first() {
        let root = scratch("kept");
        let folder = root.join("kept");
        for at in 0..KEPT + 2 {
            keep(&folder, "make", &Answer { at: at as u64, status: 200, body: if at == 0 { "x".repeat(MOST_KEPT + 10) } else { String::new() }, ..Answer::default() }).unwrap();
        }
        keep(&folder, "other", &Answer { at: 99, ..Answer::default() }).unwrap();
        let kept = answers(&folder, "make");
        assert_eq!(kept.iter().map(|answer| answer.at).collect::<Vec<_>>(), (2..KEPT as u64 + 2).rev().collect::<Vec<_>>());
        assert_eq!(answers(&folder, "other").len(), 1);
        assert!(answers(&folder, "none").is_empty());
        keep(&folder, "big", &Answer { body: "y".repeat(MOST_KEPT + 10), ..Answer::default() }).unwrap();
        let big = &answers(&folder, "big")[0];
        assert!(big.cut && big.body.len() == MOST_KEPT);
        let _ = std::fs::remove_dir_all(&root);
    }

    /// The program openssl, where the system or Git's own tools give it.
    fn openssl() -> Option<PathBuf> {
        let names = ["openssl".to_string()];
        crate::toolchains::path_folders()
            .iter()
            .find_map(|dir| crate::toolchains::program_in(dir, &names))
            .or_else(|| ["C:\\Program Files\\Git\\usr\\bin", "C:\\Program Files\\Git\\mingw64\\bin"].iter().find_map(|dir| crate::toolchains::program_in(Path::new(dir), &names)))
    }

    #[test]
    fn https_goes_through_the_system_s_tls_and_a_certificate_no_root_signs_is_refused() {
        let Some(openssl) = openssl() else {
            eprintln!("openssl is not here to serve TLS: the test of TLS needs it");
            return;
        };
        let dir = scratch("tls");
        std::fs::write(dir.join("openssl.cnf"), "[req]\ndistinguished_name = name\n[name]\n").unwrap();
        let made = Command::new(&openssl).args(["req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "2", "-subj", "/CN=localhost", "-keyout", "key.pem", "-out", "cert.pem", "-config", "openssl.cnf"]).env("OPENSSL_CONF", dir.join("openssl.cnf")).current_dir(&dir).output().unwrap();
        assert!(made.status.success(), "{}", String::from_utf8_lossy(&made.stderr));
        let port = TcpListener::bind("127.0.0.1:0").unwrap().local_addr().unwrap().port();
        let mut server = Command::new(&openssl).env("OPENSSL_CONF", dir.join("openssl.cnf")).args(["s_server", "-quiet", "-www", "-cert", "cert.pem", "-key", "key.pem", "-accept", &port.to_string()]).current_dir(&dir).stdin(std::process::Stdio::piped()).stdout(std::process::Stdio::null()).stderr(std::process::Stdio::null()).spawn().unwrap();
        let url = format!("https://localhost:{port}/");
        let up = Instant::now();
        while TcpStream::connect(("127.0.0.1", port)).is_err() && up.elapsed() < Duration::from_secs(10) {
            std::thread::sleep(Duration::from_millis(50));
        }
        let unchecked = send(&Sent { method: "GET".into(), url: url.clone(), redirect: true, ..Sent::default() }, false);
        let checked = send(&Sent { method: "GET".into(), url, redirect: true, ..Sent::default() }, true);
        let _ = server.kill();
        let _ = server.wait();
        let _ = std::fs::remove_dir_all(&dir);
        assert_eq!(unchecked.failed, None);
        assert_eq!(unchecked.status, 200, "{unchecked:?}");
        assert!(unchecked.body.contains("s_server") || unchecked.body.contains("Ciphers"), "{}", unchecked.body);
        assert!(unchecked.times.tls > 0.0);
        assert!(checked.failed.as_deref().is_some_and(|said| said.starts_with("TLS:")), "{checked:?}");
    }
}

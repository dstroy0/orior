// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The Internet Printing Protocol, as RFC 8010 and 8011 set it out, spoken by orior itself over
//! HTTP/1.1: a printer's attributes asked for, a job printed with its document, its state asked for
//! and a job canceled, and the printers a CUPS server shares listed. A printer is named by its
//! ipp:// address; one that takes only ipps://, IPP over TLS, is said to be out of reach.

use std::io::{BufRead, BufReader, Read, Write};
use std::net::TcpStream;
use std::time::Duration;

/// A value of an attribute, by its type.
#[derive(Clone, Debug, PartialEq)]
pub enum Value {
    Integer(i32),
    Boolean(bool),
    Enum(i32),
    Range(i32, i32),
    /// Text, a name, a keyword, a URI, a charset, a language or a media type, by its tag.
    Text(u8, String),
    /// A value orior reads past: its tag and bytes.
    Other(u8, Vec<u8>),
}

impl Value {
    pub fn text(&self) -> Option<&str> {
        match self {
            Value::Text(_, text) => Some(text),
            _ => None,
        }
    }

    pub fn number(&self) -> Option<i32> {
        match self {
            Value::Integer(value) | Value::Enum(value) => Some(*value),
            _ => None,
        }
    }
}

const OPERATION: u8 = 0x01;
const JOB: u8 = 0x02;
const END: u8 = 0x03;
const PRINTER: u8 = 0x04;

const NAME: u8 = 0x42;
const KEYWORD: u8 = 0x44;
const URI: u8 = 0x45;
const CHARSET: u8 = 0x47;
const LANGUAGE: u8 = 0x48;
const MIME: u8 = 0x49;

/// A group of attributes: its tag and each attribute's name and values, in order.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct Group {
    pub tag: u8,
    pub attributes: Vec<(String, Vec<Value>)>,
}

impl Group {
    pub fn get(&self, name: &str) -> Option<&Vec<Value>> {
        self.attributes.iter().find(|(one, _)| one == name).map(|(_, values)| values)
    }

    pub fn first_text(&self, name: &str) -> Option<String> {
        self.get(name).and_then(|values| values.first()).and_then(Value::text).map(str::to_string)
    }

    pub fn first_number(&self, name: &str) -> Option<i32> {
        self.get(name).and_then(|values| values.first()).and_then(Value::number)
    }

    pub fn texts(&self, name: &str) -> Vec<String> {
        self.get(name).into_iter().flatten().filter_map(Value::text).map(str::to_string).collect()
    }
}

/// A request or a response: the operation or status, its groups, and the document after them.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct Message {
    pub code: u16,
    pub request_id: u32,
    pub groups: Vec<Group>,
    pub data: Vec<u8>,
}

impl Message {
    pub fn group(&self, tag: u8) -> Option<&Group> {
        self.groups.iter().find(|group| group.tag == tag)
    }

    pub fn groups_of(&self, tag: u8) -> impl Iterator<Item = &Group> {
        self.groups.iter().filter(move |group| group.tag == tag)
    }
}

/// `message` in IPP's encoding, version 2.0.
pub fn encode(message: &Message) -> Vec<u8> {
    let mut out = vec![2, 0];
    out.extend_from_slice(&message.code.to_be_bytes());
    out.extend_from_slice(&message.request_id.to_be_bytes());
    for group in &message.groups {
        out.push(group.tag);
        for (name, values) in &group.attributes {
            for (index, value) in values.iter().enumerate() {
                let (tag, bytes): (u8, Vec<u8>) = match value {
                    Value::Integer(number) => (0x21, number.to_be_bytes().to_vec()),
                    Value::Boolean(on) => (0x22, vec![*on as u8]),
                    Value::Enum(number) => (0x23, number.to_be_bytes().to_vec()),
                    Value::Range(low, high) => (0x33, [low.to_be_bytes(), high.to_be_bytes()].concat()),
                    Value::Text(tag, text) => (*tag, text.as_bytes().to_vec()),
                    Value::Other(tag, bytes) => (*tag, bytes.clone()),
                };
                out.push(tag);
                let named = if index == 0 { name.as_bytes() } else { &[] };
                out.extend_from_slice(&(named.len() as u16).to_be_bytes());
                out.extend_from_slice(named);
                out.extend_from_slice(&(bytes.len() as u16).to_be_bytes());
                out.extend_from_slice(&bytes);
            }
        }
    }
    out.push(END);
    out.extend_from_slice(&message.data);
    out
}

/// The message `bytes` hold.
pub fn decode(bytes: &[u8]) -> Result<Message, String> {
    let short = || "the printer's answer is cut short".to_string();
    let take = |at: &mut usize, count: usize| -> Result<&[u8], String> {
        let out = bytes.get(*at..*at + count).ok_or_else(short)?;
        *at += count;
        Ok(out)
    };
    let mut at = 2;
    let code = u16::from_be_bytes(take(&mut at, 2)?.try_into().map_err(|_| short())?);
    let request_id = u32::from_be_bytes(take(&mut at, 4)?.try_into().map_err(|_| short())?);
    let mut message = Message { code, request_id, ..Message::default() };
    loop {
        let tag = *take(&mut at, 1)?.first().ok_or_else(short)?;
        if tag == END {
            break;
        }
        if tag < 0x10 {
            message.groups.push(Group { tag, attributes: Vec::new() });
            continue;
        }
        let name_length = u16::from_be_bytes(take(&mut at, 2)?.try_into().map_err(|_| short())?) as usize;
        let name = String::from_utf8_lossy(take(&mut at, name_length)?).into_owned();
        let value_length = u16::from_be_bytes(take(&mut at, 2)?.try_into().map_err(|_| short())?) as usize;
        let raw = take(&mut at, value_length)?;
        let number = |raw: &[u8]| raw.get(0..4).map(|four| i32::from_be_bytes(four.try_into().unwrap_or_default())).unwrap_or(0);
        let value = match tag {
            0x21 => Value::Integer(number(raw)),
            0x22 => Value::Boolean(raw.first() == Some(&1)),
            0x23 => Value::Enum(number(raw)),
            0x33 => Value::Range(number(raw), number(raw.get(4..).unwrap_or_default())),
            0x41..=0x49 => Value::Text(tag, String::from_utf8_lossy(raw).into_owned()),
            _ => Value::Other(tag, raw.to_vec()),
        };
        let group = message.groups.last_mut().ok_or("the printer's answer has an attribute in no group")?;
        if name.is_empty() {
            if let Some((_, values)) = group.attributes.last_mut() {
                values.push(value);
            }
        } else {
            group.attributes.push((name, vec![value]));
        }
    }
    message.data = bytes[at..].to_vec();
    Ok(message)
}

/// Where an ipp:// address reaches: its host, its port and its path.
pub fn address(uri: &str) -> Result<(String, u16, String), String> {
    let rest = if let Some(rest) = uri.strip_prefix("ipp://") {
        rest
    } else if uri.starts_with("ipps://") {
        return Err(format!("{uri} takes only encrypted IPP, which orior does not speak; the printer's ipp:// address, where it has one, reaches it"));
    } else if let Some(rest) = uri.strip_prefix("http://") {
        rest
    } else {
        return Err(format!("{uri} is no ipp:// address"));
    };
    let (authority, path) = rest.split_once('/').map(|(host, path)| (host, format!("/{path}"))).unwrap_or((rest, "/".into()));
    let (host, port) = if let Some(inside) = authority.strip_prefix('[') {
        let (host, rest) = inside.split_once(']').ok_or_else(|| format!("{uri} has an address that does not close"))?;
        (host.to_string(), rest.strip_prefix(':').and_then(|port| port.parse().ok()).unwrap_or(631))
    } else {
        match authority.rsplit_once(':') {
            Some((host, port)) => (host.to_string(), port.parse().map_err(|_| format!("{uri} names no port"))?),
            None => (authority.to_string(), 631),
        }
    };
    Ok((host, port, path))
}

/// Sends `message` to the printer at `uri` and gives its answer, within `wait` to connect and each
/// read.
pub fn exchange(uri: &str, message: &Message, wait: Duration) -> Result<Message, String> {
    let (host, port, path) = address(uri)?;
    let target = std::net::ToSocketAddrs::to_socket_addrs(&(host.as_str(), port)).map_err(|error| format!("{host}: {error}"))?.next().ok_or_else(|| format!("{host} has no address"))?;
    let mut stream = TcpStream::connect_timeout(&target, wait).map_err(|error| format!("{host}:{port} did not take the connection: {error}"))?;
    stream.set_read_timeout(Some(wait.max(Duration::from_secs(30)))).ok();
    let body = encode(message);
    let host_header = if host.contains(':') { format!("[{host}]:{port}") } else { format!("{host}:{port}") };
    let head = format!("POST {path} HTTP/1.1\r\nHost: {host_header}\r\nContent-Type: application/ipp\r\nContent-Length: {}\r\nConnection: close\r\nUser-Agent: orior\r\n\r\n", body.len());
    stream.write_all(head.as_bytes()).and_then(|_| stream.write_all(&body)).and_then(|_| stream.flush()).map_err(|error| format!("{host}: the job did not go: {error}"))?;
    let mut reader = BufReader::new(stream);
    let mut status = String::new();
    reader.read_line(&mut status).map_err(|error| format!("{host}: no answer: {error}"))?;
    let code: u16 = status.split_whitespace().nth(1).and_then(|code| code.parse().ok()).ok_or_else(|| format!("{host} did not answer as a printer: {}", status.trim()))?;
    let mut length: Option<usize> = None;
    let mut chunked = false;
    loop {
        let mut line = String::new();
        reader.read_line(&mut line).map_err(|error| error.to_string())?;
        let line = line.trim_end();
        if line.is_empty() {
            break;
        }
        if let Some((key, value)) = line.split_once(':') {
            match key.trim().to_ascii_lowercase().as_str() {
                "content-length" => length = value.trim().parse().ok(),
                "transfer-encoding" if value.trim().eq_ignore_ascii_case("chunked") => chunked = true,
                _ => {}
            }
        }
    }
    let mut body = Vec::new();
    if chunked {
        loop {
            let mut size = String::new();
            reader.read_line(&mut size).map_err(|error| error.to_string())?;
            let size = usize::from_str_radix(size.trim().split(';').next().unwrap_or("0"), 16).map_err(|_| format!("{host} sent a chunk orior cannot read"))?;
            if size == 0 {
                break;
            }
            let mut chunk = vec![0; size];
            reader.read_exact(&mut chunk).map_err(|error| error.to_string())?;
            body.extend(chunk);
            let mut end = String::new();
            reader.read_line(&mut end).map_err(|error| error.to_string())?;
        }
    } else if let Some(length) = length {
        body.resize(length, 0);
        reader.read_exact(&mut body).map_err(|error| error.to_string())?;
    } else {
        reader.read_to_end(&mut body).map_err(|error| error.to_string())?;
    }
    if code == 401 {
        return Err(format!("{host} asks who prints: it takes jobs only from a user it knows"));
    }
    if code != 200 {
        return Err(format!("{host} answered HTTP {code}"));
    }
    decode(&body)
}

/// What an IPP status says, in words, and whether it is success.
pub fn status(code: u16) -> (bool, String) {
    let said = match code {
        0x0000..=0x00FF => return (true, "done".into()),
        0x0400 => "the printer did not understand the request",
        0x0401 => "the printer refuses jobs from this machine",
        0x0402 | 0x0403 => "the printer asks who prints, and takes jobs only from a user it knows",
        0x0404 => "the printer cannot do what was asked now",
        0x0406 => "the printer has no such job or queue",
        0x0407 => "the job is gone",
        0x040A => "the printer does not take the document's format",
        0x040B => "the printer does not take a setting the job asked for",
        0x0500 => "the printer failed inside",
        0x0501 => "the printer does not do this",
        0x0502 => "the printer does not speak this version of IPP",
        0x0503 => "the printer is not taking jobs now",
        0x0506 => "the printer is not accepting jobs",
        0x0507 => "the printer is busy",
        _ => return (false, format!("the printer answered status 0x{code:04X}")),
    };
    (false, said.to_string())
}

fn operation(uri: &str, extra: Vec<(String, Vec<Value>)>) -> Group {
    let mut attributes = vec![
        ("attributes-charset".to_string(), vec![Value::Text(CHARSET, "utf-8".into())]),
        ("attributes-natural-language".to_string(), vec![Value::Text(LANGUAGE, "en".into())]),
        ("printer-uri".to_string(), vec![Value::Text(URI, uri.into())]),
        ("requesting-user-name".to_string(), vec![Value::Text(NAME, user())]),
    ];
    attributes.extend(extra);
    Group { tag: OPERATION, attributes }
}

fn user() -> String {
    std::env::var("USERNAME").or_else(|_| std::env::var("USER")).unwrap_or_else(|_| "orior".into())
}

static NEXT: std::sync::atomic::AtomicU32 = std::sync::atomic::AtomicU32::new(1);

fn request(code: u16, groups: Vec<Group>, data: Vec<u8>) -> Message {
    Message { code, request_id: NEXT.fetch_add(1, std::sync::atomic::Ordering::SeqCst), groups, data }
}

fn answer(uri: &str, message: &Message) -> Result<Message, String> {
    let said = exchange(uri, message, Duration::from_secs(10))?;
    let (ok, words) = status(said.code);
    if !ok {
        let detail = said.group(OPERATION).and_then(|group| group.first_text("status-message")).map(|text| format!(": {text}")).unwrap_or_default();
        return Err(format!("{words}{detail}"));
    }
    Ok(said)
}

/// What a printer says of itself.
#[derive(Clone, Debug, Default, serde::Serialize)]
pub struct Printer {
    pub name: String,
    pub make: String,
    pub formats: Vec<String>,
    pub color: bool,
    pub state: i32,
    pub reasons: Vec<String>,
    pub accepting: bool,
}

/// The attributes of the printer at `uri` that printing needs.
pub fn printer(uri: &str) -> Result<Printer, String> {
    let wanted = ["printer-name", "printer-make-and-model", "document-format-supported", "color-supported", "printer-state", "printer-state-reasons", "printer-is-accepting-jobs"];
    let asked = request(0x000B, vec![operation(uri, vec![("requested-attributes".into(), wanted.iter().map(|one| Value::Text(KEYWORD, one.to_string())).collect())])], Vec::new());
    let said = answer(uri, &asked)?;
    let group = said.group(PRINTER).cloned().unwrap_or_default();
    Ok(Printer {
        name: group.first_text("printer-name").unwrap_or_default(),
        make: group.first_text("printer-make-and-model").unwrap_or_default(),
        formats: group.texts("document-format-supported"),
        color: matches!(group.get("color-supported").and_then(|values| values.first()), Some(Value::Boolean(true))),
        state: group.first_number("printer-state").unwrap_or(3),
        reasons: group.texts("printer-state-reasons"),
        accepting: !matches!(group.get("printer-is-accepting-jobs").and_then(|values| values.first()), Some(Value::Boolean(false))),
    })
}

/// How a job is printed: its name, its document's format, and the job's own settings.
pub struct Job<'a> {
    pub name: &'a str,
    pub format: &'a str,
    pub copies: u32,
    pub landscape: bool,
    pub gray: bool,
    pub media: &'a str,
}

/// Prints `document` on the printer at `uri`, and gives the job's id.
pub fn print(uri: &str, job: &Job, document: Vec<u8>) -> Result<i32, String> {
    let operation = operation(uri, vec![("job-name".into(), vec![Value::Text(NAME, job.name.into())]), ("document-format".into(), vec![Value::Text(MIME, job.format.into())])]);
    let mut settings: Vec<(String, Vec<Value>)> = vec![("media".into(), vec![Value::Text(KEYWORD, job.media.into())]), ("print-color-mode".into(), vec![Value::Text(KEYWORD, if job.gray { "monochrome" } else { "color" }.into())])];
    if job.copies > 1 {
        settings.push(("copies".into(), vec![Value::Integer(job.copies as i32)]));
    }
    if job.format == "application/pdf" {
        settings.push(("orientation-requested".into(), vec![Value::Enum(if job.landscape { 4 } else { 3 })]));
    }
    let asked = request(0x0002, vec![operation, Group { tag: JOB, attributes: settings }], document);
    let said = answer(uri, &asked)?;
    said.group(JOB).and_then(|group| group.first_number("job-id")).ok_or_else(|| "the printer took the job and gave it no id".to_string())
}

/// A job's state: 3 pending, 4 held, 5 printing, 6 stopped, 7 canceled, 8 aborted, 9 done; and
/// why, as the printer words it.
pub fn job_state(uri: &str, id: i32) -> Result<(i32, Vec<String>), String> {
    let asked = request(0x0009, vec![operation(uri, vec![("job-id".into(), vec![Value::Integer(id)]), ("requested-attributes".into(), vec![Value::Text(KEYWORD, "job-state".into()), Value::Text(KEYWORD, "job-state-reasons".into())])])], Vec::new());
    let said = answer(uri, &asked)?;
    let group = said.group(JOB).cloned().unwrap_or_default();
    Ok((group.first_number("job-state").unwrap_or(3), group.texts("job-state-reasons")))
}

/// Cancels the job `id` on the printer at `uri`.
pub fn cancel(uri: &str, id: i32) -> Result<(), String> {
    let asked = request(0x0008, vec![operation(uri, vec![("job-id".into(), vec![Value::Integer(id)])])], Vec::new());
    answer(uri, &asked).map(|_| ())
}

/// The printers the CUPS server at host:port shares: each its name, its ipp:// address and what it
/// is.
pub fn cups_printers(host: &str, port: u16) -> Result<Vec<(String, String, String)>, String> {
    let uri = format!("ipp://{host}:{port}/");
    let wanted = ["printer-name", "printer-uri-supported", "printer-info", "printer-make-and-model"];
    let mut operation = operation(&uri, vec![("requested-attributes".into(), wanted.iter().map(|one| Value::Text(KEYWORD, one.to_string())).collect())]);
    operation.attributes.retain(|(name, _)| name != "printer-uri");
    let said = answer(&uri, &request(0x4002, vec![operation], Vec::new()))?;
    Ok(said
        .groups_of(PRINTER)
        .filter_map(|group| {
            let name = group.first_text("printer-name")?;
            let uri = group.texts("printer-uri-supported").into_iter().find(|one| one.starts_with("ipp://")).or_else(|| group.first_text("printer-uri-supported"))?;
            let about = group.first_text("printer-info").filter(|info| !info.is_empty()).or_else(|| group.first_text("printer-make-and-model")).unwrap_or_default();
            Some((name, uri, about))
        })
        .collect())
}

#[cfg(test)]
mod speaking {
    use super::*;

    #[test]
    fn a_message_reads_back_as_written() {
        let message = Message {
            code: 0x0002,
            request_id: 7,
            groups: vec![
                operation("ipp://printer.local:631/ipp/print", vec![("job-name".into(), vec![Value::Text(NAME, "main.py".into())])]),
                Group { tag: JOB, attributes: vec![("copies".into(), vec![Value::Integer(2)]), ("page-ranges".into(), vec![Value::Range(1, 3), Value::Range(5, 5)]), ("orientation-requested".into(), vec![Value::Enum(4)])] },
            ],
            data: b"%PDF-1.4".to_vec(),
        };
        let bytes = encode(&message);
        assert_eq!(&bytes[..8], &[2, 0, 0, 2, 0, 0, 0, 7]);
        assert_eq!(decode(&bytes).unwrap(), message);
    }

    #[test]
    fn an_address_gives_its_host_port_and_path() {
        assert_eq!(address("ipp://printer.local/ipp/print").unwrap(), ("printer.local".into(), 631, "/ipp/print".into()));
        assert_eq!(address("ipp://10.0.0.5:8631/printers/office").unwrap(), ("10.0.0.5".into(), 8631, "/printers/office".into()));
        assert_eq!(address("ipp://[fe80::1]:631/x").unwrap(), ("fe80::1".into(), 631, "/x".into()));
        assert!(address("ipps://printer.local/ipp/print").unwrap_err().contains("encrypted"));
    }

    /// The CUPS server orior's tests print to, where one runs: ORIOR_CUPS names it as host:port.
    fn cups() -> Option<(String, u16)> {
        let named = std::env::var("ORIOR_CUPS").ok()?;
        let (host, port) = named.rsplit_once(':')?;
        Some((host.to_string(), port.parse().ok()?))
    }

    #[test]
    fn a_cups_server_lists_its_printers_takes_a_job_and_cancels_one() {
        let Some((host, port)) = cups() else { return };
        let printers = cups_printers(&host, port).unwrap();
        assert!(!printers.is_empty(), "the server shares no printer");
        let (_, uri, _) = &printers[0];
        let uri = uri.replace("localhost", &host).replacen(":631", &format!(":{port}"), 1);
        let about = printer(&uri).unwrap();
        assert!(about.formats.iter().any(|one| one == "application/pdf"), "{:?}", about.formats);
        let listing = super::super::page::Listing { title: "ipp test".into(), lines: vec![super::super::page::Line { number: Some(1), runs: vec![super::super::page::Run { text: "printed by orior".into(), color: "#000000".into(), ..Default::default() }] }], tab: 4 };
        let (pdf, _) = super::super::pdf::write(&listing, &super::super::page::Layout::default());
        let job = Job { name: "orior ipp test", format: "application/pdf", copies: 1, landscape: false, gray: true, media: "iso_a4_210x297mm" };
        let id = print(&uri, &job, pdf).unwrap();
        assert!(id > 0);
        let (state, _) = job_state(&uri, id).unwrap();
        assert!((3..=9).contains(&state), "{state}");
        let held = Job { name: "orior cancel test", ..job };
        let (pdf, _) = super::super::pdf::write(&listing, &super::super::page::Layout::default());
        let second = print(&uri, &held, pdf).unwrap();
        let _ = cancel(&uri, second);
        let (state, _) = job_state(&uri, second).unwrap();
        assert!(state >= 7, "a canceled job ended: {state}");
        let wrong = print(&uri, &Job { format: "application/x-orior-none", ..held }, b"x".to_vec()).unwrap_err();
        assert!(wrong.contains("format"), "{wrong}");
    }
}

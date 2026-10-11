// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! PostgreSQL, spoken by orior itself in the frontend and backend protocol 3.0 over TCP, with no
//! driver: the startup, a password by SCRAM-SHA-256, md5 or in the clear as the server asks, a
//! statement by the simple query protocol, and its rows read off the socket a page at a time, the
//! rest left to wait in the server until they are asked for. A statement running is stopped by a
//! cancel request on a connection of its own, by the key the server gave at the startup.
//!
//! Each value comes as the server writes it in text, a time with a zone in the session's zone with
//! its offset. The connection is not encrypted: a server elsewhere is reached through an ssh tunnel.

use std::collections::BTreeMap;
use std::hash::{BuildHasher, Hasher};
use std::io::{BufReader, BufWriter, Read, Write};
use std::net::TcpStream;
use std::time::Duration;

use crate::digest::{hmac_sha256_bytes, md5, pbkdf2_sha256, sha256_bytes};
use crate::serve::{base64, unbase64};

/// What a server said went wrong, in its own fields.
#[derive(Clone, Debug, Default, serde::Serialize)]
pub struct Failure {
    pub message: String,
    pub code: String,
    pub detail: String,
    pub hint: String,
    /// The place in the statement it names, counted in characters from 1, or 0.
    pub position: usize,
}

impl std::fmt::Display for Failure {
    fn fmt(&self, out: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(out, "{}", self.message)?;
        if !self.code.is_empty() {
            write!(out, " ({})", self.code)?;
        }
        if !self.detail.is_empty() {
            write!(out, "\n{}", self.detail)?;
        }
        if !self.hint.is_empty() {
            write!(out, "\n{}", self.hint)?;
        }
        Ok(())
    }
}

impl From<String> for Failure {
    fn from(message: String) -> Failure {
        Failure { message, ..Failure::default() }
    }
}

impl From<&str> for Failure {
    fn from(message: &str) -> Failure {
        Failure::from(message.to_string())
    }
}

/// A column of a result: its name and its type's name.
#[derive(Clone, Debug, serde::Serialize)]
pub struct Column {
    pub name: String,
    pub kind: String,
    /// Whether its values are numbers, drawn to the right.
    pub numeric: bool,
}

/// A row: each value as text, None for NULL.
pub type Row = Vec<Option<String>>;

/// How a statement began: with rows to read, or done with the tag the server gave.
#[derive(Debug)]
pub enum Head {
    Rows(Vec<Column>),
    Done(String),
}

/// What reaches the server to stop the statement a connection runs.
#[derive(Clone, Debug)]
pub struct Cancel {
    host: String,
    port: u16,
    pid: u32,
    secret: Vec<u8>,
}

impl Cancel {
    /// Asks the server to stop the statement the connection runs now.
    pub fn send(&self) -> Result<(), String> {
        let mut stream = TcpStream::connect((self.host.as_str(), self.port)).map_err(|error| format!("{}:{}: {error}", self.host, self.port))?;
        let mut body = Vec::new();
        body.extend_from_slice(&80877102u32.to_be_bytes());
        body.extend_from_slice(&self.pid.to_be_bytes());
        body.extend_from_slice(&self.secret);
        let mut message = ((body.len() + 4) as u32).to_be_bytes().to_vec();
        message.extend(body);
        stream.write_all(&message).map_err(|error| error.to_string())?;
        let mut rest = [0u8; 1];
        let _ = stream.read(&mut rest);
        Ok(())
    }
}

/// A connection to a PostgreSQL server.
pub struct Pg {
    reader: BufReader<TcpStream>,
    writer: BufWriter<TcpStream>,
    cancel: Option<Cancel>,
    /// The transaction's state at the last ReadyForQuery: I idle, T open, E failed.
    pub status: u8,
    /// What the server said of itself: server_version, TimeZone and the rest.
    pub params: BTreeMap<String, String>,
    /// What the server noted while the last statement ran.
    pub notices: Vec<String>,
    /// Whether the statement running has rows left to read.
    reading: bool,
    /// A statement's failure the server said before its rows were all read.
    failed: Option<Failure>,
}

/// Bytes no one can guess, for SCRAM's nonce.
pub(crate) fn random(count: usize) -> Vec<u8> {
    let mut out = Vec::with_capacity(count + 8);
    let mut index = 0u64;
    while out.len() < count {
        let mut hasher = std::collections::hash_map::RandomState::new().build_hasher();
        hasher.write_u64(index);
        out.extend_from_slice(&hasher.finish().to_le_bytes());
        index += 1;
    }
    out.truncate(count);
    out
}

fn cstr(out: &mut Vec<u8>, text: &str) {
    out.extend_from_slice(text.as_bytes());
    out.push(0);
}

/// The type names of the types a result most often holds, by their oid.
fn type_name(oid: u32) -> (&'static str, bool) {
    match oid {
        16 => ("bool", false),
        17 => ("bytea", false),
        18 => ("char", false),
        19 => ("name", false),
        20 => ("int8", true),
        21 => ("int2", true),
        23 => ("int4", true),
        25 => ("text", false),
        26 => ("oid", true),
        114 => ("json", false),
        142 => ("xml", false),
        700 => ("float4", true),
        701 => ("float8", true),
        790 => ("money", true),
        1042 => ("bpchar", false),
        1043 => ("varchar", false),
        1082 => ("date", false),
        1083 => ("time", false),
        1114 => ("timestamp", false),
        1184 => ("timestamptz", false),
        1186 => ("interval", false),
        1266 => ("timetz", false),
        1700 => ("numeric", true),
        2950 => ("uuid", false),
        3802 => ("jsonb", false),
        _ => ("", false),
    }
}

impl Pg {
    /// Connects to `database` at host:port as `user`, signing in with `password` as the server
    /// asks, within `wait`.
    pub fn connect(host: &str, port: u16, user: &str, password: &str, database: &str, wait: Duration) -> Result<Pg, Failure> {
        let address = std::net::ToSocketAddrs::to_socket_addrs(&(host, port)).map_err(|error| format!("{host}:{port}: {error}"))?.next().ok_or_else(|| format!("{host} has no address"))?;
        let stream = TcpStream::connect_timeout(&address, wait).map_err(|error| format!("{host}:{port}: {error}"))?;
        stream.set_nodelay(true).ok();
        stream.set_read_timeout(Some(wait)).ok();
        let mut pg = Pg {
            reader: BufReader::with_capacity(1 << 16, stream.try_clone().map_err(|error| error.to_string())?),
            writer: BufWriter::new(stream),
            cancel: None,
            status: b'I',
            params: BTreeMap::new(),
            notices: Vec::new(),
            reading: false,
            failed: None,
        };
        let mut body = Vec::new();
        body.extend_from_slice(&196608u32.to_be_bytes());
        for (key, value) in [("user", user), ("database", database), ("application_name", "orior"), ("client_encoding", "UTF8"), ("DateStyle", "ISO")] {
            cstr(&mut body, key);
            cstr(&mut body, value);
        }
        body.push(0);
        let mut startup = ((body.len() + 4) as u32).to_be_bytes().to_vec();
        startup.extend(body);
        pg.writer.write_all(&startup).and_then(|_| pg.writer.flush()).map_err(|error| error.to_string())?;
        let mut scram: Option<(String, String)> = None;
        loop {
            let (kind, body) = pg.message()?;
            match kind {
                b'R' => {
                    let code = u32::from_be_bytes(body[0..4].try_into().unwrap_or_default());
                    match code {
                        0 => {}
                        3 => {
                            let mut reply = Vec::new();
                            cstr(&mut reply, password);
                            pg.send(b'p', &reply)?;
                        }
                        5 => {
                            let inner = md5(format!("{password}{user}").as_bytes());
                            let mut salted = inner.into_bytes();
                            salted.extend_from_slice(&body[4..8]);
                            let mut reply = Vec::new();
                            cstr(&mut reply, &format!("md5{}", md5(&salted)));
                            pg.send(b'p', &reply)?;
                        }
                        10 => {
                            let offered: Vec<String> = body[4..].split(|byte| *byte == 0).filter(|name| !name.is_empty()).map(|name| String::from_utf8_lossy(name).into_owned()).collect();
                            if !offered.iter().any(|name| name == "SCRAM-SHA-256") {
                                return Err(format!("the server signs in only by {}, which orior does not speak", offered.join(", ")).into());
                            }
                            let nonce = base64(&random(18));
                            let first_bare = format!("n=,r={nonce}");
                            let first = format!("n,,{first_bare}");
                            let mut reply = Vec::new();
                            cstr(&mut reply, "SCRAM-SHA-256");
                            reply.extend_from_slice(&(first.len() as u32).to_be_bytes());
                            reply.extend_from_slice(first.as_bytes());
                            pg.send(b'p', &reply)?;
                            scram = Some((first_bare, nonce));
                        }
                        11 => {
                            let (first_bare, nonce) = scram.clone().ok_or("the server went on with a sign-in that was not begun")?;
                            let server_first = String::from_utf8_lossy(&body[4..]).into_owned();
                            let (client_final, signature) = scram_final(password, &first_bare, &nonce, &server_first)?;
                            scram = Some((signature, String::new()));
                            pg.send(b'p', client_final.as_bytes())?;
                        }
                        12 => {
                            let said = String::from_utf8_lossy(&body[4..]).into_owned();
                            let expected = scram.as_ref().map(|(signature, _)| signature.clone()).unwrap_or_default();
                            if said.trim().strip_prefix("v=") != Some(expected.as_str()) {
                                return Err("the server did not prove it holds the password: the sign-in is not trusted".to_string().into());
                            }
                        }
                        other => return Err(format!("the server asks for a sign-in orior does not speak ({other}): SCRAM-SHA-256, md5 and a password in the clear are spoken").into()),
                    }
                }
                b'K' => {
                    let pid = u32::from_be_bytes(body[0..4].try_into().unwrap_or_default());
                    pg.cancel = Some(Cancel { host: host.to_string(), port, pid, secret: body[4..].to_vec() });
                }
                b'S' => pg.parameter(&body),
                b'N' => pg.notice(&body),
                b'E' => {
                    let failure = failure_of(&body);
                    if failure.message.contains("no encryption") || failure.message.contains("SSL") {
                        return Err(Failure { hint: "The server takes only encrypted connections; orior reaches such a server through an ssh tunnel to a machine beside it.".into(), ..failure });
                    }
                    return Err(failure);
                }
                b'Z' => {
                    pg.status = body.first().copied().unwrap_or(b'I');
                    break;
                }
                _ => {}
            }
        }
        pg.reader.get_ref().set_read_timeout(None).ok();
        Ok(pg)
    }

    /// What stops the statement this connection runs, from another thread.
    pub fn canceller(&self) -> Option<Cancel> {
        self.cancel.clone()
    }

    fn send(&mut self, kind: u8, body: &[u8]) -> Result<(), Failure> {
        let mut message = vec![kind];
        message.extend_from_slice(&((body.len() + 4) as u32).to_be_bytes());
        message.extend_from_slice(body);
        self.writer.write_all(&message).and_then(|_| self.writer.flush()).map_err(|error| Failure::from(format!("the connection broke: {error}")))
    }

    fn message(&mut self) -> Result<(u8, Vec<u8>), Failure> {
        let mut head = [0u8; 5];
        self.reader.read_exact(&mut head).map_err(|error| Failure::from(format!("the connection broke: {error}")))?;
        let length = u32::from_be_bytes([head[1], head[2], head[3], head[4]]) as usize;
        let mut body = vec![0u8; length.saturating_sub(4)];
        self.reader.read_exact(&mut body).map_err(|error| Failure::from(format!("the connection broke: {error}")))?;
        Ok((head[0], body))
    }

    fn parameter(&mut self, body: &[u8]) {
        let mut parts = body.split(|byte| *byte == 0).map(|part| String::from_utf8_lossy(part).into_owned());
        if let (Some(key), Some(value)) = (parts.next(), parts.next()) {
            self.params.insert(key, value);
        }
    }

    fn notice(&mut self, body: &[u8]) {
        let said = failure_of(body);
        self.notices.push(said.to_string());
    }

    /// Sends `sql` as one query and reads until it gives rows or ends.
    pub fn start(&mut self, sql: &str) -> Result<Head, Failure> {
        if self.reading {
            self.finish()?;
        }
        self.notices.clear();
        self.failed = None;
        let mut body = Vec::new();
        cstr(&mut body, sql);
        self.send(b'Q', &body)?;
        let mut tag = String::new();
        loop {
            let (kind, body) = self.message()?;
            match kind {
                b'T' => {
                    self.reading = true;
                    return Ok(Head::Rows(columns_of(&body)));
                }
                b'C' => tag = String::from_utf8_lossy(body.strip_suffix(&[0]).unwrap_or(&body)).into_owned(),
                b'I' => {}
                b'E' => self.failed = Some(failure_of(&body)),
                b'N' => self.notice(&body),
                b'S' => self.parameter(&body),
                b'G' => {
                    let mut reason = Vec::new();
                    cstr(&mut reason, "orior sends no data to COPY FROM STDIN");
                    self.send(b'f', &reason)?;
                }
                b'H' => {
                    self.reading = true;
                    return Ok(Head::Rows(vec![Column { name: "copy".into(), kind: "text".into(), numeric: false }]));
                }
                b'Z' => {
                    self.status = body.first().copied().unwrap_or(b'I');
                    return match self.failed.take() {
                        Some(failure) => Err(failure),
                        None => Ok(Head::Done(tag)),
                    };
                }
                _ => {}
            }
        }
    }

    /// Up to `count` rows more of the statement running, and its tag once it has none left.
    pub fn rows(&mut self, count: usize) -> Result<(Vec<Row>, Option<String>), Failure> {
        let mut rows = Vec::new();
        if !self.reading {
            return Ok((rows, Some(String::new())));
        }
        let mut tag = None;
        while rows.len() < count || tag.is_some() {
            let (kind, body) = self.message()?;
            match kind {
                b'D' => rows.push(row_of(&body)),
                b'd' => rows.push(vec![Some(String::from_utf8_lossy(&body).trim_end_matches('\n').to_string())]),
                b'c' => {}
                b'C' => tag = Some(String::from_utf8_lossy(body.strip_suffix(&[0]).unwrap_or(&body)).into_owned()),
                b'E' => self.failed = Some(failure_of(&body)),
                b'N' => self.notice(&body),
                b'S' => self.parameter(&body),
                b'Z' => {
                    self.status = body.first().copied().unwrap_or(b'I');
                    self.reading = false;
                    if let Some(failure) = self.failed.take() {
                        return Err(failure);
                    }
                    return Ok((rows, Some(tag.unwrap_or_default())));
                }
                _ => {}
            }
        }
        Ok((rows, None))
    }

    /// Reads past the rows the statement running has left, to take the next.
    pub fn finish(&mut self) -> Result<String, Failure> {
        let mut tag = String::new();
        while self.reading {
            let (_, done) = self.rows(4096)?;
            if let Some(done) = done {
                tag = done;
            }
        }
        Ok(tag)
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
        let _ = self.send(b'X', &[]);
    }
}

/// The client's final message of SCRAM-SHA-256, as RFC 5802 and 7677 set it out, and the server's
/// signature it must answer with.
fn scram_final(password: &str, first_bare: &str, nonce: &str, server_first: &str) -> Result<(String, String), Failure> {
    let field = |name: &str| server_first.split(',').find_map(|part| part.strip_prefix(name)).map(str::to_string);
    let server_nonce = field("r=").ok_or("the server gave no nonce")?;
    if !server_nonce.starts_with(nonce) {
        return Err("the server's nonce is not the one asked for".to_string().into());
    }
    let salt = field("s=").and_then(|salt| unbase64(&salt)).ok_or("the server gave no salt")?;
    let rounds: u32 = field("i=").and_then(|count| count.parse().ok()).ok_or("the server gave no count of rounds")?;
    let salted = pbkdf2_sha256(password.as_bytes(), &salt, rounds);
    let client_key = hmac_sha256_bytes(&salted, b"Client Key");
    let stored_key = sha256_bytes(&client_key);
    let without_proof = format!("c=biws,r={server_nonce}");
    let auth = format!("{first_bare},{server_first},{without_proof}");
    let client_signature = hmac_sha256_bytes(&stored_key, auth.as_bytes());
    let proof: Vec<u8> = client_key.iter().zip(client_signature).map(|(key, signature)| key ^ signature).collect();
    let server_key = hmac_sha256_bytes(&salted, b"Server Key");
    let server_signature = hmac_sha256_bytes(&server_key, auth.as_bytes());
    Ok((format!("{without_proof},p={}", base64(&proof)), base64(&server_signature)))
}

fn failure_of(body: &[u8]) -> Failure {
    let mut failure = Failure::default();
    for field in body.split(|byte| *byte == 0).filter(|field| !field.is_empty()) {
        let text = String::from_utf8_lossy(&field[1..]).into_owned();
        match field[0] {
            b'M' => failure.message = text,
            b'C' => failure.code = text,
            b'D' => failure.detail = text,
            b'H' => failure.hint = text,
            b'P' => failure.position = text.parse().unwrap_or(0),
            _ => {}
        }
    }
    failure
}

fn columns_of(body: &[u8]) -> Vec<Column> {
    let count = u16::from_be_bytes([body[0], body[1]]) as usize;
    let mut at = 2;
    let mut out = Vec::with_capacity(count);
    for _ in 0..count {
        let end = body[at..].iter().position(|byte| *byte == 0).map(|found| at + found).unwrap_or(body.len());
        let name = String::from_utf8_lossy(&body[at..end]).into_owned();
        at = end + 1;
        let oid = u32::from_be_bytes(body[at + 6..at + 10].try_into().unwrap_or_default());
        at += 18;
        let (kind, numeric) = type_name(oid);
        out.push(Column { name, kind: if kind.is_empty() { format!("oid {oid}") } else { kind.to_string() }, numeric });
    }
    out
}

fn row_of(body: &[u8]) -> Row {
    let count = u16::from_be_bytes([body[0], body[1]]) as usize;
    let mut at = 2;
    let mut out = Vec::with_capacity(count);
    for _ in 0..count {
        let length = i32::from_be_bytes(body[at..at + 4].try_into().unwrap_or_default());
        at += 4;
        if length < 0 {
            out.push(None);
        } else {
            let end = (at + length as usize).min(body.len());
            out.push(Some(String::from_utf8_lossy(&body[at..end]).into_owned()));
            at = end;
        }
    }
    out
}

/// `text` as a string PostgreSQL reads back as itself.
pub fn literal(text: &str) -> String {
    format!("'{}'", text.replace('\'', "''"))
}

#[cfg(test)]
mod speaking {
    use super::*;

    #[test]
    fn scram_gives_rfc_7677_s_proof_and_signature() {
        let (client_final, signature) = scram_final("pencil", "n=user,r=rOprNGfwEbeRWgbNEkqO", "rOprNGfwEbeRWgbNEkqO", "r=rOprNGfwEbeRWgbNEkqO%hvYDpWUa2RaTCAfuxFIlj)hNlF$k0,s=W22ZaJ0SNY7soEsUEjb6gQ==,i=4096").unwrap();
        assert_eq!(client_final, "c=biws,r=rOprNGfwEbeRWgbNEkqO%hvYDpWUa2RaTCAfuxFIlj)hNlF$k0,p=dHzbZapWIk4jUhN+Ute9ytag9zjfMHgsqmmiz7AndVQ=");
        assert_eq!(signature, "6rriTRBi23WpRR/wtup+mMhUZUn/dB5nLTJRsjl95G4=");
    }

    /// The server orior's tests sign in to, where one runs: PostgreSQL at ORIOR_PG_ADDRESS, else on
    /// 127.0.0.1:5432, with the password ORIOR_PG_PASSWORD names for the user postgres.
    fn server() -> Option<Pg> {
        let password = std::env::var("ORIOR_PG_PASSWORD").ok()?;
        let (host, port) = crate::db::test_server("PG", 5432);
        Pg::connect(&host, port, "postgres", &password, "postgres", Duration::from_secs(5)).ok()
    }

    #[test]
    fn a_query_s_rows_come_a_page_at_a_time_and_a_cancel_stops_it() {
        let Some(mut pg) = server() else { return };
        assert!(pg.params.contains_key("server_version"));
        let Head::Rows(columns) = pg.start("select n, n::text as t, null::int as z, now() as at from generate_series(1, 2500) n").unwrap() else { panic!("no rows") };
        assert_eq!(columns.iter().map(|one| (one.name.as_str(), one.kind.as_str(), one.numeric)).collect::<Vec<_>>(), vec![("n", "int4", true), ("t", "text", false), ("z", "int4", true), ("at", "timestamptz", false)]);
        let (first, done) = pg.rows(1000).unwrap();
        assert_eq!((first.len(), done.is_none()), (1000, true));
        assert_eq!(first[999][0].as_deref(), Some("1000"));
        assert_eq!(first[0][2], None);
        assert!(first[0][3].as_deref().is_some_and(|at| at.contains('+') || at.contains('-')));
        let (second, _) = pg.rows(1000).unwrap();
        assert_eq!(second[0][0].as_deref(), Some("1001"));
        assert_eq!(pg.rows(1000).unwrap().1.as_deref(), Some("SELECT 2500"));
        let failed = pg.start("select 1/0").unwrap_err();
        assert_eq!(failed.code, "22012");
        assert!(matches!(pg.start("begin").unwrap(), Head::Done(_)));
        assert_eq!(pg.status, b'T');
        pg.start("rollback").unwrap();
        assert_eq!(pg.status, b'I');
        let cancel = pg.canceller().unwrap();
        let stopper = std::thread::spawn(move || {
            std::thread::sleep(Duration::from_millis(400));
            cancel.send().unwrap();
        });
        let stopped = pg.start("select pg_sleep(30)").unwrap();
        let failed = match stopped {
            Head::Rows(_) => pg.rows(10).unwrap_err(),
            Head::Done(_) => panic!("the sleep was not stopped"),
        };
        stopper.join().unwrap();
        assert_eq!(failed.code, "57014");
        assert_eq!(pg.all("select 'it''s'").unwrap(), vec![vec![Some("it's".to_string())]]);
        pg.close();
    }

    #[test]
    fn md5_and_cleartext_passwords_sign_in_where_the_server_asks_for_them() {
        let Some(mut pg) = server() else { return };
        pg.all("drop role if exists orior_md5").unwrap();
        pg.all("drop role if exists orior_plain").unwrap();
        pg.all("set password_encryption = 'md5'").unwrap();
        pg.all("create role orior_md5 login password 'md5 secret'").unwrap();
        pg.all("reset password_encryption").unwrap();
        pg.all("create role orior_plain login password 'plain secret'").unwrap();
        pg.close();
        let (host, port) = crate::db::test_server("PG", 5432);
        for (user, password) in [("orior_md5", "md5 secret"), ("orior_plain", "plain secret")] {
            let signed = Pg::connect(&host, port, user, password, "postgres", Duration::from_secs(5));
            assert!(signed.is_ok(), "{user}: {:?}", signed.err());
            let wrong = Pg::connect(&host, port, user, "not it", "postgres", Duration::from_secs(5)).err().unwrap();
            assert_eq!(wrong.code, "28P01", "{user}");
        }
    }
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The database window's connections: the ones a tree names in databases.json at its top folder,
//! each a PostgreSQL, a MySQL or MariaDB, or a SQLite database, opened as two links, one that runs
//! the reader's statements and one that reads the schema, which the other never waits on.
//!
//! PostgreSQL and MySQL are spoken by pg.rs and mysql.rs; SQLite is read through the sqlite3 module
//! of the tree's Python, in a process of its own that takes one statement at a time and stops one
//! when asked. A password goes to the system's keychain, and never to a file; a connection reached
//! through ssh goes through the system's ssh with the reader's own config, keys and agent, and a
//! link whose connection broke is made again, tunnel and all, at the next statement.
//!
//! A statement's rows are held here as they are read, a page at a time as the grid asks for them,
//! and the rest wait in the server. An export writes the rows held and reads the rest from the same
//! run. Before a statement runs, an UPDATE or a DELETE with no WHERE, or a TRUNCATE, asks on every
//! connection with the count of the rows it would reach, and on a connection marked as one that
//! matters a statement that writes asks, or the connection is opened read-only, the server itself
//! refusing a write, as the reader sets it. Each statement run is kept in its connection's history
//! in orior's own folder.

use std::collections::BTreeMap;
use std::io::{BufRead, BufReader, BufWriter, Write};
use std::path::{Path, PathBuf};
use std::process::{Child, ChildStdin, ChildStdout, Command, Stdio};
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

use serde::{Deserialize, Serialize};
use serde_json::{json, Value};

pub use crate::pg::{Column, Failure, Head, Row};
use crate::mysql::Mysql;
use crate::pg::Pg;
use crate::sql::{self, Dialect};

/// How long a connection, a tunnel or a sign-in is waited for.
const WAIT: Duration = Duration::from_secs(15);

/// The statements of a connection's history kept, the newest.
const HISTORY: usize = 1000;

/// A connection a tree names.
#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
pub struct Connection {
    pub name: String,
    /// postgres, mysql or sqlite.
    pub kind: String,
    #[serde(default, skip_serializing_if = "String::is_empty")]
    pub host: String,
    #[serde(default, skip_serializing_if = "is_zero")]
    pub port: u16,
    #[serde(default, skip_serializing_if = "String::is_empty")]
    pub user: String,
    #[serde(default, skip_serializing_if = "String::is_empty")]
    pub database: String,
    /// A SQLite database's file, by its path in the tree or whole.
    #[serde(default, skip_serializing_if = "String::is_empty")]
    pub file: String,
    /// The machine, as ssh names it, a tunnel to the server goes through.
    #[serde(default, skip_serializing_if = "String::is_empty")]
    pub ssh: String,
    /// Whether the connection is one that matters, whose writes are guarded.
    #[serde(default, skip_serializing_if = "is_false")]
    pub matters: bool,
    /// How a connection that matters guards its writes: read-only, or ask.
    #[serde(default, skip_serializing_if = "String::is_empty")]
    pub guard: String,
}

fn is_zero(port: &u16) -> bool {
    *port == 0
}

fn is_false(on: &bool) -> bool {
    !*on
}

impl Connection {
    /// The port the server listens at, its kind's own where none is named.
    pub fn port_or_default(&self) -> u16 {
        match (self.port, self.kind.as_str()) {
            (0, "mysql") => 3306,
            (0, _) => 5432,
            (port, _) => port,
        }
    }

    /// The account the keychain keeps its password by.
    pub fn account(&self) -> String {
        let through = if self.ssh.is_empty() { String::new() } else { format!(" via {}", self.ssh) };
        format!("{}://{}@{}:{}/{}{through}", self.kind, self.user, self.host, self.port_or_default(), self.database)
    }

    fn dialect(&self) -> Dialect {
        Dialect::of(&self.kind)
    }

    fn read_only(&self) -> bool {
        self.matters && self.guard == "read-only"
    }
}

#[derive(Default, Serialize, Deserialize)]
struct Named {
    #[serde(default)]
    connections: Vec<Connection>,
}

/// The connections databases.json at the tree's top folder names; none where it is not there.
pub fn read(root: &Path) -> Result<Vec<Connection>, String> {
    let path = root.join("databases.json");
    let Ok(text) = std::fs::read_to_string(&path) else { return Ok(Vec::new()) };
    serde_json::from_str::<Named>(&text).map(|named| named.connections).map_err(|error| format!("databases.json: {error}"))
}

/// Keeps `connection` in databases.json, in place of the one of its name, or as `was` where it is
/// renamed.
pub fn save(root: &Path, connection: Connection, was: Option<&str>) -> Result<(), String> {
    if connection.name.trim().is_empty() {
        return Err("a connection needs a name".into());
    }
    if !matches!(connection.kind.as_str(), "postgres" | "mysql" | "sqlite") {
        return Err(format!("{} is no kind of database orior speaks: postgres, mysql or sqlite", connection.kind));
    }
    let mut all = read(root)?;
    let old = was.unwrap_or(&connection.name).to_string();
    if old != connection.name && all.iter().any(|one| one.name == connection.name) {
        return Err(format!("the tree names a connection {} already", connection.name));
    }
    match all.iter_mut().find(|one| one.name == old) {
        Some(found) => *found = connection,
        None => all.push(connection),
    }
    write(root, &all)
}

/// Takes the connection `name` out of databases.json, and its password out of the keychain.
pub fn remove(root: &Path, name: &str) -> Result<(), String> {
    let mut all = read(root)?;
    if let Some(found) = all.iter().find(|one| one.name == name) {
        let _ = keychain::forget(&found.account());
    }
    all.retain(|one| one.name != name);
    write(root, &all)
}

fn write(root: &Path, all: &[Connection]) -> Result<(), String> {
    let text = serde_json::to_string_pretty(&Named { connections: all.to_vec() }).map_err(|error| error.to_string())?;
    std::fs::write(root.join("databases.json"), format!("{text}\n")).map_err(|error| format!("databases.json: {error}"))
}

/// The system's keychain: Windows' Credential Manager, macOS's keychain through `security`, and the
/// Secret Service through `secret-tool` elsewhere.
pub mod keychain {
    #[cfg(windows)]
    fn wide(text: &str) -> Vec<u16> {
        text.encode_utf16().chain(Some(0)).collect()
    }

    #[cfg(windows)]
    fn target(account: &str) -> Vec<u16> {
        wide(&format!("orior database {account}"))
    }

    /// Keeps `password` for `account`.
    pub fn keep(account: &str, password: &str) -> Result<(), String> {
        #[cfg(windows)]
        {
            use windows_sys::Win32::Security::Credentials::{CredWriteW, CREDENTIALW, CRED_PERSIST_LOCAL_MACHINE, CRED_TYPE_GENERIC};
            let mut name = target(account);
            let mut user = wide(account);
            let mut blob = password.as_bytes().to_vec();
            // SAFETY: every pointer the credential holds lives through the call.
            let written = unsafe {
                let mut credential: CREDENTIALW = std::mem::zeroed();
                credential.Type = CRED_TYPE_GENERIC;
                credential.TargetName = name.as_mut_ptr();
                credential.UserName = user.as_mut_ptr();
                credential.CredentialBlobSize = blob.len() as u32;
                credential.CredentialBlob = blob.as_mut_ptr();
                credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
                CredWriteW(&credential, 0)
            };
            if written == 0 {
                return Err(format!("the keychain did not keep the password: {}", std::io::Error::last_os_error()));
            }
            Ok(())
        }
        #[cfg(not(windows))]
        {
            use std::io::Write;
            use std::process::{Command, Stdio};
            if cfg!(target_os = "macos") {
                let done = Command::new("security").args(["add-generic-password", "-U", "-s", "orior database", "-a", account, "-w", password]).stdout(Stdio::null()).stderr(Stdio::piped()).output().map_err(|error| format!("security: {error}"))?;
                return if done.status.success() { Ok(()) } else { Err(String::from_utf8_lossy(&done.stderr).trim().to_string()) };
            }
            let mut child = Command::new("secret-tool").args(["store", "--label", &format!("orior database {account}"), "service", "orior database", "account", account]).stdin(Stdio::piped()).stdout(Stdio::null()).stderr(Stdio::piped()).spawn().map_err(|error| format!("secret-tool, which keeps passwords in the Secret Service: {error}"))?;
            child.stdin.take().map(|mut input| input.write_all(password.as_bytes()));
            let done = child.wait_with_output().map_err(|error| error.to_string())?;
            if done.status.success() { Ok(()) } else { Err(String::from_utf8_lossy(&done.stderr).trim().to_string()) }
        }
    }

    /// The password kept for `account`, where one is kept.
    pub fn read(account: &str) -> Option<String> {
        #[cfg(windows)]
        {
            use windows_sys::Win32::Security::Credentials::{CredFree, CredReadW, CREDENTIALW, CRED_TYPE_GENERIC};
            let name = target(account);
            let mut found: *mut CREDENTIALW = std::ptr::null_mut();
            // SAFETY: the credential read is freed once its password is copied out.
            unsafe {
                if CredReadW(name.as_ptr(), CRED_TYPE_GENERIC, 0, &mut found) == 0 || found.is_null() {
                    return None;
                }
                let blob = std::slice::from_raw_parts((*found).CredentialBlob, (*found).CredentialBlobSize as usize);
                let password = String::from_utf8_lossy(blob).into_owned();
                CredFree(found as _);
                Some(password)
            }
        }
        #[cfg(not(windows))]
        {
            use std::process::Command;
            let done = if cfg!(target_os = "macos") { Command::new("security").args(["find-generic-password", "-s", "orior database", "-a", account, "-w"]).output().ok()? } else { Command::new("secret-tool").args(["lookup", "service", "orior database", "account", account]).output().ok()? };
            done.status.success().then(|| String::from_utf8_lossy(&done.stdout).trim_end_matches('\n').to_string())
        }
    }

    /// Takes the password kept for `account` out of the keychain.
    pub fn forget(account: &str) -> Result<(), String> {
        #[cfg(windows)]
        {
            use windows_sys::Win32::Security::Credentials::{CredDeleteW, CRED_TYPE_GENERIC};
            let name = target(account);
            // SAFETY: the name lives through the call.
            unsafe { CredDeleteW(name.as_ptr(), CRED_TYPE_GENERIC, 0) };
            Ok(())
        }
        #[cfg(not(windows))]
        {
            use std::process::Command;
            if cfg!(target_os = "macos") {
                Command::new("security").args(["delete-generic-password", "-s", "orior database", "-a", account]).output().map_err(|error| error.to_string())?;
            } else {
                Command::new("secret-tool").args(["clear", "service", "orior database", "account", account]).output().map_err(|error| error.to_string())?;
            }
            Ok(())
        }
    }
}

/// A tunnel to the server through ssh: the system's ssh, with the reader's config, keys and agent,
/// forwarding a port of this machine's loopback.
struct Tunnel {
    child: Child,
    port: u16,
}

impl Tunnel {
    fn open(through: &str, host: &str, port: u16) -> Result<Tunnel, Failure> {
        let local = std::net::TcpListener::bind(("127.0.0.1", 0)).and_then(|listener| listener.local_addr()).map(|address| address.port()).map_err(|error| error.to_string())?;
        let mut command = Command::new("ssh");
        command
            .args(["-N", "-L", &format!("127.0.0.1:{local}:{host}:{port}"), "-o", "ExitOnForwardFailure=yes", "-o", "BatchMode=yes", "-o", "ServerAliveInterval=15", "-o", "ServerAliveCountMax=3", through])
            .env("PATH", crate::toolchains::run_path())
            .stdin(Stdio::null())
            .stdout(Stdio::null())
            .stderr(Stdio::piped());
        crate::runner::quiet(&mut command);
        let mut child = command.spawn().map_err(|error| format!("ssh, which the tunnel goes through: {error}"))?;
        let until = Instant::now() + WAIT;
        loop {
            if let Ok(Some(_)) = child.try_wait() {
                let mut said = String::new();
                child.stderr.take().map(|mut err| std::io::Read::read_to_string(&mut err, &mut said));
                return Err(Failure { message: format!("ssh {through} ended: {}", said.trim()), hint: "The tunnel signs in with the keys ssh's agent holds and the reader's ~/.ssh/config, as the terminal's ssh does; a machine that asks for a password needs a key put on it.".into(), ..Failure::default() });
            }
            if std::net::TcpStream::connect_timeout(&(std::net::Ipv4Addr::LOCALHOST, local).into(), Duration::from_millis(200)).is_ok() {
                return Ok(Tunnel { child, port: local });
            }
            if Instant::now() > until {
                let _ = child.kill();
                return Err(format!("ssh {through} made no tunnel in {} s", WAIT.as_secs()).into());
            }
            std::thread::sleep(Duration::from_millis(100));
        }
    }

    fn alive(&mut self) -> bool {
        matches!(self.child.try_wait(), Ok(None))
    }
}

impl Drop for Tunnel {
    fn drop(&mut self) {
        let _ = self.child.kill();
        let _ = self.child.wait();
    }
}

/// orior's SQLite: a process of the tree's Python that reads one request a line and answers one a
/// line, in JSON. An interrupt reaches the statement running from a thread of its own.
const SQLITE: &str = r#"
import json, queue, sqlite3, sys, threading, urllib.parse

conn = None
cursor = None
jobs = queue.Queue()

def say(value):
    sys.stdout.write(json.dumps(value) + "\n")
    sys.stdout.flush()

def read():
    for line in sys.stdin:
        ask = json.loads(line)
        if "interrupt" in ask:
            if conn is not None:
                conn.interrupt()
            continue
        jobs.put(ask)
    jobs.put(None)

def shown(value):
    if value is None:
        return None
    if isinstance(value, bytes):
        return "0x" + value[:256].hex().upper() + ("… (%d bytes)" % len(value) if len(value) > 256 else "")
    return str(value)

threading.Thread(target=read, daemon=True).start()
while True:
    ask = jobs.get()
    if ask is None:
        break
    try:
        if "open" in ask:
            uri = "file:" + urllib.parse.quote(ask["open"]) + ("?mode=ro" if ask.get("readonly") else "?mode=rwc")
            conn = sqlite3.connect(uri, uri=True, isolation_level=None, check_same_thread=False)
            say({"version": sqlite3.sqlite_version, "tx": False})
        elif "start" in ask:
            cursor = conn.execute(ask["start"])
            if cursor.description is None:
                count = cursor.rowcount
                cursor = None
                say({"done": "%d row%s affected" % (count, "" if count == 1 else "s") if count >= 0 else "", "tx": conn.in_transaction})
            else:
                say({"columns": [one[0] for one in cursor.description], "tx": conn.in_transaction})
        elif "rows" in ask:
            rows = cursor.fetchmany(ask["rows"]) if cursor is not None else []
            kinds = [None] * (len(rows[0]) if rows else 0)
            for row in rows:
                for at, value in enumerate(row):
                    if kinds[at] is None and value is not None:
                        kinds[at] = type(value).__name__
            done = cursor is None or len(rows) < ask["rows"]
            if done:
                cursor = None
            say({"rows": [[shown(value) for value in row] for row in rows], "kinds": kinds, "done": done, "tx": conn.in_transaction})
    except sqlite3.Error as error:
        cursor = None
        say({"error": str(error), "code": type(error).__name__, "tx": conn.in_transaction if conn is not None else False})
"#;

/// A SQLite database open in a process of the tree's Python.
struct Lite {
    child: Child,
    input: Arc<Mutex<ChildStdin>>,
    output: BufReader<ChildStdout>,
    in_transaction: bool,
    reading: bool,
    count: usize,
    version: String,
}

impl Lite {
    fn open(file: &Path, read_only: bool) -> Result<Lite, Failure> {
        let python = crate::test_runs::python().ok_or_else(|| Failure { message: "no Python is found to read SQLite with".into(), hint: "File, Toolchains finds Python, or takes its folder.".into(), ..Failure::default() })?;
        let mut command = Command::new(python);
        command.args(["-I", "-u", "-c", SQLITE]).env("PATH", crate::toolchains::run_path()).env("PYTHONIOENCODING", "utf-8").stdin(Stdio::piped()).stdout(Stdio::piped()).stderr(Stdio::null());
        crate::runner::quiet(&mut command);
        let mut child = command.spawn().map_err(|error| format!("Python: {error}"))?;
        let input = Arc::new(Mutex::new(child.stdin.take().ok_or("Python took no input")?));
        let output = BufReader::new(child.stdout.take().ok_or("Python gave no output")?);
        let mut lite = Lite { child, input, output, in_transaction: false, reading: false, count: 0, version: String::new() };
        let said = lite.ask(json!({"open": file.display().to_string(), "readonly": read_only}))?;
        lite.version = said["version"].as_str().unwrap_or_default().to_string();
        Ok(lite)
    }

    fn ask(&mut self, request: Value) -> Result<Value, Failure> {
        {
            let mut input = self.input.lock().map_err(|_| "SQLite's process is held")?;
            writeln!(input, "{request}").and_then(|_| input.flush()).map_err(|error| Failure::from(format!("the connection broke: {error}")))?;
        }
        let mut line = String::new();
        if self.output.read_line(&mut line).map_err(|error| error.to_string())? == 0 {
            return Err("the connection broke: SQLite's process ended".into());
        }
        let said: Value = serde_json::from_str(&line).map_err(|error| format!("SQLite's process said what orior cannot read: {error}"))?;
        self.in_transaction = said["tx"].as_bool().unwrap_or(false);
        if let Some(error) = said["error"].as_str() {
            self.reading = false;
            return Err(Failure { message: error.to_string(), code: said["code"].as_str().unwrap_or_default().to_string(), ..Failure::default() });
        }
        Ok(said)
    }

    fn start(&mut self, statement: &str) -> Result<Head, Failure> {
        if self.reading {
            while self.reading {
                self.rows(4096)?;
            }
        }
        let said = self.ask(json!({"start": statement}))?;
        if let Some(tag) = said["done"].as_str() {
            return Ok(Head::Done(tag.to_string()));
        }
        self.reading = true;
        self.count = 0;
        let columns = said["columns"].as_array().into_iter().flatten().map(|name| Column { name: name.as_str().unwrap_or_default().to_string(), kind: String::new(), numeric: false }).collect();
        Ok(Head::Rows(columns))
    }

    fn rows(&mut self, count: usize) -> Result<(Vec<Row>, Option<String>), Failure> {
        if !self.reading {
            return Ok((Vec::new(), Some(String::new())));
        }
        let said = self.ask(json!({"rows": count}))?;
        let rows: Vec<Row> = said["rows"].as_array().into_iter().flatten().map(|row| row.as_array().into_iter().flatten().map(|value| value.as_str().map(str::to_string)).collect()).collect();
        self.count += rows.len();
        if said["done"].as_bool().unwrap_or(true) {
            self.reading = false;
            return Ok((rows, Some(format!("{} row{} read", self.count, if self.count == 1 { "" } else { "s" }))));
        }
        Ok((rows, None))
    }
}

impl Drop for Lite {
    fn drop(&mut self) {
        let _ = self.child.kill();
        let _ = self.child.wait();
    }
}

/// One link to a database.
enum Link {
    Pg(Pg),
    My(Mysql),
    Lite(Lite),
}

/// What stops the statement a link runs, from another thread.
#[derive(Clone)]
enum Stop {
    Pg(crate::pg::Cancel),
    My(crate::mysql::Cancel),
    Lite(Arc<Mutex<ChildStdin>>),
}

impl Stop {
    fn send(&self) -> Result<(), String> {
        match self {
            Stop::Pg(cancel) => cancel.send(),
            Stop::My(cancel) => cancel.send(),
            Stop::Lite(input) => {
                let mut input = input.lock().map_err(|_| "SQLite's process is held".to_string())?;
                writeln!(input, "{}", json!({"interrupt": true})).and_then(|_| input.flush()).map_err(|error| error.to_string())
            }
        }
    }
}

impl Link {
    fn start(&mut self, statement: &str) -> Result<Head, Failure> {
        match self {
            Link::Pg(pg) => pg.start(statement),
            Link::My(my) => my.start(statement),
            Link::Lite(lite) => lite.start(statement),
        }
    }

    fn rows(&mut self, count: usize) -> Result<(Vec<Row>, Option<String>), Failure> {
        match self {
            Link::Pg(pg) => pg.rows(count),
            Link::My(my) => my.rows(count),
            Link::Lite(lite) => lite.rows(count),
        }
    }

    fn all(&mut self, statement: &str) -> Result<Vec<Row>, Failure> {
        match self.start(statement)? {
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

    /// The transaction's state: idle, open, or failed and waiting to be rolled back.
    fn transaction(&self) -> &'static str {
        match self {
            Link::Pg(pg) => match pg.status {
                b'T' => "open",
                b'E' => "failed",
                _ => "idle",
            },
            Link::My(my) => {
                if my.in_transaction() {
                    "open"
                } else {
                    "idle"
                }
            }
            Link::Lite(lite) => {
                if lite.in_transaction {
                    "open"
                } else {
                    "idle"
                }
            }
        }
    }

    fn stopper(&self) -> Option<Stop> {
        match self {
            Link::Pg(pg) => pg.canceller().map(Stop::Pg),
            Link::My(my) => Some(Stop::My(my.canceller())),
            Link::Lite(lite) => Some(Stop::Lite(lite.input.clone())),
        }
    }

    fn notices(&mut self) -> Vec<String> {
        match self {
            Link::Pg(pg) => std::mem::take(&mut pg.notices),
            Link::My(my) if my.warnings > 0 => {
                let count = my.warnings;
                let mut said = Vec::new();
                if let Ok(rows) = my.all("SHOW WARNINGS") {
                    said.extend(rows.into_iter().map(|row| row.into_iter().flatten().collect::<Vec<_>>().join(" ")));
                }
                if said.is_empty() {
                    said.push(format!("{count} warning{}", if count == 1 { "" } else { "s" }));
                }
                said
            }
            _ => Vec::new(),
        }
    }

    fn close(self) {
        match self {
            Link::Pg(pg) => pg.close(),
            Link::My(my) => my.close(),
            Link::Lite(_) => {}
        }
    }
}

/// The rows of the last statement run that gave rows, as they have been read.
#[derive(Default)]
struct Held {
    columns: Vec<Column>,
    rows: Vec<Row>,
    done: Option<String>,
    /// Rows read past the ones held, as an export wrote them out.
    beyond: usize,
}

/// A connection open: its links, its tunnel and the rows it holds.
pub struct Open {
    pub config: Connection,
    root: PathBuf,
    password: String,
    query: Mutex<Option<Link>>,
    meta: Mutex<Option<Link>>,
    stop: Mutex<Option<Stop>>,
    meta_stop: Mutex<Option<Stop>>,
    tunnel: Mutex<Option<Tunnel>>,
    held: Mutex<Held>,
    pub version: String,
    pub zone: Mutex<String>,
}

fn now_ms() -> u128 {
    SystemTime::now().duration_since(UNIX_EPOCH).map(|since| since.as_millis()).unwrap_or(0)
}

fn quote_ident(name: &str, dialect: Dialect) -> String {
    match dialect {
        Dialect::Mysql => format!("`{}`", name.replace('`', "``")),
        _ => format!("\"{}\"", name.replace('"', "\"\"")),
    }
}

fn literal(text: &str, dialect: Dialect) -> String {
    match dialect {
        Dialect::Mysql => crate::mysql::literal(text),
        _ => crate::pg::literal(text),
    }
}

impl Open {
    /// Opens `config`'s two links, signing in with `password`, or with the one the keychain keeps.
    pub fn connect(root: &Path, config: Connection, password: Option<String>) -> Result<Open, Failure> {
        let password = password.or_else(|| keychain::read(&config.account())).unwrap_or_default();
        let mut open = Open {
            config,
            root: root.to_path_buf(),
            password,
            query: Mutex::new(None),
            meta: Mutex::new(None),
            stop: Mutex::new(None),
            meta_stop: Mutex::new(None),
            tunnel: Mutex::new(None),
            held: Mutex::new(Held::default()),
            version: String::new(),
            zone: Mutex::new(String::new()),
        };
        let query = open.link(true)?;
        let meta = open.link(false)?;
        open.version = match &query {
            Link::Pg(pg) => pg.params.get("server_version").cloned().unwrap_or_default(),
            Link::My(my) => my.version.clone(),
            Link::Lite(lite) => lite.version.clone(),
        };
        *open.stop.lock().map_err(|_| "the connection is held")? = query.stopper();
        *open.meta_stop.lock().map_err(|_| "the connection is held")? = meta.stopper();
        *open.query.lock().map_err(|_| "the connection is held")? = Some(query);
        *open.meta.lock().map_err(|_| "the connection is held")? = Some(meta);
        open.read_zone();
        Ok(open)
    }

    /// A link to the database, through the tunnel where the connection goes through ssh, read-only
    /// where it is set so; the one that runs the reader's statements where `runs`.
    fn link(&self, runs: bool) -> Result<Link, Failure> {
        let config = &self.config;
        let (host, port) = if config.ssh.is_empty() || config.kind == "sqlite" {
            (if config.host.is_empty() { "127.0.0.1".to_string() } else { config.host.clone() }, config.port_or_default())
        } else {
            let mut tunnel = self.tunnel.lock().map_err(|_| "the tunnel is held")?;
            if !tunnel.as_mut().is_some_and(Tunnel::alive) {
                *tunnel = Some(Tunnel::open(&config.ssh, if config.host.is_empty() { "127.0.0.1" } else { &config.host }, config.port_or_default())?);
            }
            ("127.0.0.1".to_string(), tunnel.as_ref().map(|one| one.port).unwrap_or(0))
        };
        let link = match config.kind.as_str() {
            "postgres" => {
                let database = if config.database.is_empty() { &config.user } else { &config.database };
                let mut pg = Pg::connect(&host, port, &config.user, &self.password, database, WAIT)?;
                if runs && config.read_only() {
                    pg.all("SET SESSION CHARACTERISTICS AS TRANSACTION READ ONLY")?;
                }
                Link::Pg(pg)
            }
            "mysql" => {
                let mut my = Mysql::connect(&host, port, &config.user, &self.password, &config.database, WAIT)?;
                if runs && config.read_only() {
                    my.all("SET SESSION TRANSACTION READ ONLY")?;
                }
                Link::My(my)
            }
            "sqlite" => {
                let file = if Path::new(&config.file).is_absolute() { PathBuf::from(&config.file) } else { crate::root::full(&self.root, &config.file) };
                Link::Lite(Lite::open(&file, runs && config.read_only() || !runs)?)
            }
            other => return Err(format!("{other} is no kind of database orior speaks").into()),
        };
        Ok(link)
    }

    /// The zone the session reads times in, which the status bar names.
    fn read_zone(&self) {
        let zone = match self.query.lock().ok().as_deref_mut().and_then(Option::as_mut) {
            Some(Link::Pg(pg)) => pg.params.get("TimeZone").cloned().unwrap_or_default(),
            Some(Link::My(my)) => my.all("SELECT IF(@@session.time_zone = 'SYSTEM', @@system_time_zone, @@session.time_zone)").ok().and_then(|rows| rows.into_iter().next()).and_then(|row| row.into_iter().next().flatten()).unwrap_or_default(),
            _ => String::new(),
        };
        if let Ok(mut held) = self.zone.lock() {
            *held = zone;
        }
    }

    /// Ends the connection's links and its tunnel.
    pub fn close(&self) {
        for slot in [&self.query, &self.meta] {
            if let Some(link) = slot.lock().ok().and_then(|mut held| held.take()) {
                link.close();
            }
        }
        if let Ok(mut tunnel) = self.tunnel.lock() {
            tunnel.take();
        }
    }

    /// The query link, made again where its connection broke.
    fn with_query<T>(&self, work: impl FnOnce(&mut Link) -> Result<T, Failure>) -> Result<T, Failure> {
        let mut slot = self.query.lock().map_err(|_| "the connection is held")?;
        if slot.is_none() {
            let link = self.link(true)?;
            *self.stop.lock().map_err(|_| "the connection is held")? = link.stopper();
            *slot = Some(link);
        }
        let link = slot.as_mut().ok_or("the connection is closed")?;
        let result = work(link);
        if let Err(failure) = &result {
            if failure.message.starts_with("the connection broke") {
                slot.take();
                return Err(Failure { hint: "The next statement connects again.".into(), ..failure.clone() });
            }
        }
        result
    }

    fn with_meta<T>(&self, work: impl FnOnce(&mut Link) -> Result<T, Failure>) -> Result<T, Failure> {
        let mut slot = self.meta.lock().map_err(|_| "the schema's link is held")?;
        if slot.is_none() {
            let link = self.link(false)?;
            *self.meta_stop.lock().map_err(|_| "the connection is held")? = link.stopper();
            *slot = Some(link);
        }
        let link = slot.as_mut().ok_or("the connection is closed")?;
        let result = work(link);
        if result.as_ref().is_err_and(|failure| failure.message.starts_with("the connection broke")) {
            slot.take();
        }
        result
    }

    /// The transaction's state on the query link.
    pub fn transaction(&self) -> &'static str {
        self.query.try_lock().ok().and_then(|held| held.as_ref().map(Link::transaction)).unwrap_or("busy")
    }

    /// Whether running `statement` asks first, and why: a write that reaches every row of what it
    /// names, with their count, on every connection; and any statement that writes on a connection
    /// that matters and asks.
    fn ask(&self, statement: &str) -> Option<Value> {
        let dialect = self.config.dialect();
        if let Some(counting) = sql::unbounded(statement, dialect) {
            let count = self.with_meta(|link| link.all(&counting)).ok().and_then(|rows| rows.into_iter().next()).and_then(|row| row.into_iter().next().flatten());
            return Some(json!({"why": "every row", "count": count, "matters": self.config.matters}));
        }
        if self.config.matters && self.config.guard != "read-only" && sql::changes(statement, dialect) {
            return Some(json!({"why": "matters", "matters": true}));
        }
        None
    }

    /// Runs `statement`, asking first where it must and the reader has not answered, and gives its
    /// first `page` rows, or what it did.
    pub fn run(&self, statement: &str, answered: bool, page: usize) -> Result<Value, Failure> {
        let statement = statement.trim().trim_end_matches(';').trim_end();
        if statement.is_empty() {
            return Err("there is no statement to run".into());
        }
        if !answered {
            if let Some(ask) = self.ask(statement) {
                return Ok(json!({"ask": ask}));
            }
        }
        let dialect = self.config.dialect();
        let started = Instant::now();
        let outcome = self.with_query(|link| {
            let head = link.start(statement)?;
            match head {
                Head::Done(tag) => Ok((None, Vec::new(), Some(tag), link.notices(), link.transaction())),
                Head::Rows(mut columns) => {
                    let (rows, done) = link.rows(page)?;
                    if let Link::Lite(_) = link {
                        for (at, column) in columns.iter_mut().enumerate() {
                            let kind = rows.iter().filter_map(|row| row.get(at).cloned().flatten()).next().map(|value| if value.parse::<i64>().is_ok() { "integer" } else if value.parse::<f64>().is_ok() { "real" } else { "text" });
                            column.kind = kind.unwrap_or("").to_string();
                            column.numeric = matches!(kind, Some("integer" | "real"));
                        }
                    }
                    Ok((Some(columns), rows, done, link.notices(), link.transaction()))
                }
            }
        });
        let took = started.elapsed().as_millis();
        self.remember(statement, took, outcome.as_ref().map(|(_, rows, done, ..)| done.clone().unwrap_or_else(|| format!("{}+ rows", rows.len()))).map_err(|failure| failure.to_string()));
        let (columns, rows, done, notices, transaction) = outcome?;
        let shaped = if matches!(sql::does(statement, dialect), sql::Does::Shapes) { sql::shaped(statement, dialect) } else { None };
        if shaped.is_some() || matches!(sql::does(statement, dialect), sql::Does::Other) {
            self.read_zone();
        }
        let mut held = self.held.lock().map_err(|_| "the rows are held")?;
        if let Some(columns) = &columns {
            *held = Held { columns: columns.clone(), rows: rows.clone(), done: done.clone(), beyond: 0 };
        }
        Ok(json!({
            "columns": columns,
            "rows": rows,
            "done": done,
            "notices": notices,
            "transaction": transaction,
            "zone": self.zone.lock().map(|zone| zone.clone()).unwrap_or_default(),
            "shaped": shaped,
            "ms": took,
        }))
    }

    /// The rows held from `from`, `count` of them, read from the run where they are not held yet.
    pub fn rows(&self, from: usize, count: usize) -> Result<Value, Failure> {
        let mut held = self.held.lock().map_err(|_| "the rows are held")?;
        while held.rows.len() < from + count && held.done.is_none() {
            let want = (from + count - held.rows.len()).max(200);
            let (rows, done) = self.with_query(|link| link.rows(want))?;
            held.rows.extend(rows);
            held.done = done;
        }
        let end = (from + count).min(held.rows.len());
        let shown = held.rows.get(from.min(end)..end).unwrap_or_default().to_vec();
        Ok(json!({"rows": shown, "read": held.rows.len(), "done": held.done, "beyond": held.beyond}))
    }

    /// Writes the rows held, and then the rest the run gives, to `path` as CSV, TSV or JSON lines,
    /// the run read on to its end and not run again.
    pub fn export(&self, path: &Path, format: &str) -> Result<Value, Failure> {
        let mut held = self.held.lock().map_err(|_| "the rows are held")?;
        if held.columns.is_empty() {
            return Err("no statement has given rows to export".into());
        }
        let file = std::fs::File::create(path).map_err(|error| format!("{}: {error}", path.display()))?;
        let mut out = BufWriter::new(file);
        let names: Vec<String> = held.columns.iter().map(|column| column.name.clone()).collect();
        let line = |values: &[Option<String>]| -> String {
            match format {
                "json" => serde_json::to_string(&names.iter().zip(values).map(|(name, value)| (name.clone(), json!(value))).collect::<serde_json::Map<String, Value>>()).unwrap_or_default(),
                "tsv" => values.iter().map(|value| value.clone().unwrap_or_default().replace(['\t', '\n'], " ")).collect::<Vec<_>>().join("\t"),
                _ => values.iter().map(|value| match value {
                    None => String::new(),
                    Some(text) if text.contains([',', '"', '\n', '\r']) => format!("\"{}\"", text.replace('"', "\"\"")),
                    Some(text) => text.clone(),
                }).collect::<Vec<_>>().join(","),
            }
        };
        let broke = |error: std::io::Error| Failure::from(format!("{}: {error}", path.display()));
        if format != "json" {
            let head: Vec<Option<String>> = names.iter().cloned().map(Some).collect();
            writeln!(out, "{}", line(&head)).map_err(broke)?;
        }
        let mut written = 0usize;
        for row in &held.rows {
            writeln!(out, "{}", line(row)).map_err(broke)?;
            written += 1;
        }
        while held.done.is_none() {
            let (rows, done) = self.with_query(|link| link.rows(4096))?;
            for row in &rows {
                writeln!(out, "{}", line(row)).map_err(broke)?;
            }
            written += rows.len();
            held.beyond += rows.len();
            held.done = done;
        }
        out.flush().map_err(broke)?;
        Ok(json!({"written": written, "path": path.display().to_string()}))
    }

    /// Stops the statement running.
    pub fn stop(&self) -> Result<(), String> {
        let stop = self.stop.lock().map_err(|_| "the connection is held".to_string())?.clone();
        stop.ok_or_else(|| "nothing runs".to_string())?.send()
    }

    /// Stops the schema's reading.
    pub fn stop_reading(&self) -> Result<(), String> {
        let stop = self.meta_stop.lock().map_err(|_| "the connection is held".to_string())?.clone();
        stop.ok_or_else(|| "nothing is read".to_string())?.send()
    }

    /// The level of the schema under `path`: the schemas or databases at the top, a schema's
    /// objects under it, and a table's columns, indexes, keys and triggers under it.
    pub fn children(&self, path: &[String]) -> Result<Vec<Value>, Failure> {
        let dialect = self.config.dialect();
        let lit = |text: &str| literal(text, dialect);
        let node = |label: &str, kind: &str, detail: &str, leaf: bool, at: Vec<String>| json!({"label": label, "kind": kind, "detail": detail, "leaf": leaf, "path": at});
        let text = |row: &Row, at: usize| row.get(at).cloned().flatten().unwrap_or_default();
        let holds_parts = |kind: &str| matches!(kind, "table" | "view" | "materialized view" | "foreign table");
        let kind = self.config.kind.clone();
        self.with_meta(|link| {
            let mut out = Vec::new();
            match (kind.as_str(), path) {
                ("postgres", []) => {
                    for row in link.all("SELECT nspname FROM pg_namespace WHERE nspname !~ '^pg_' AND nspname <> 'information_schema' ORDER BY nspname")? {
                        let name = text(&row, 0);
                        out.push(node(&name, "schema", "", false, vec![name.clone()]));
                    }
                }
                ("postgres", [schema]) => {
                    let s = lit(schema);
                    let asked = format!(
                        "SELECT name, kind FROM (SELECT c.relname AS name, CASE c.relkind WHEN 'r' THEN 'table' WHEN 'p' THEN 'table' WHEN 'v' THEN 'view' WHEN 'm' THEN 'materialized view' WHEN 'f' THEN 'foreign table' ELSE 'sequence' END AS kind FROM pg_class c JOIN pg_namespace n ON n.oid = c.relnamespace WHERE n.nspname = {s} AND c.relkind IN ('r','p','v','m','f','S') AND NOT c.relispartition UNION ALL SELECT p.proname || '(' || pg_get_function_identity_arguments(p.oid) || ')', CASE p.prokind WHEN 'p' THEN 'procedure' ELSE 'function' END FROM pg_proc p JOIN pg_namespace n ON n.oid = p.pronamespace WHERE n.nspname = {s}) o ORDER BY kind, name"
                    );
                    for row in link.all(&asked)? {
                        let (name, kind) = (text(&row, 0), text(&row, 1));
                        out.push(node(&name, &kind, "", !holds_parts(&kind), vec![schema.clone(), kind.clone(), name.clone()]));
                    }
                }
                ("postgres", [schema, _, name]) => {
                    let relation = format!("{}::regclass", lit(&format!("{}.{}", quote_ident(schema, dialect), quote_ident(name, dialect))));
                    for row in link.all(&format!("SELECT a.attname, format_type(a.atttypid, a.atttypmod), a.attnotnull, pg_get_expr(d.adbin, d.adrelid) FROM pg_attribute a LEFT JOIN pg_attrdef d ON d.adrelid = a.attrelid AND d.adnum = a.attnum WHERE a.attrelid = {relation} AND a.attnum > 0 AND NOT a.attisdropped ORDER BY a.attnum"))? {
                        let detail = [text(&row, 1), if text(&row, 2) == "t" { "not null".into() } else { String::new() }, if text(&row, 3).is_empty() { String::new() } else { format!("default {}", text(&row, 3)) }].into_iter().filter(|part| !part.is_empty()).collect::<Vec<_>>().join(" ");
                        out.push(node(&text(&row, 0), "column", &detail, true, Vec::new()));
                    }
                    for (asked, kind) in [
                        (format!("SELECT i.relname, pg_get_indexdef(i.oid) FROM pg_index x JOIN pg_class i ON i.oid = x.indexrelid WHERE x.indrelid = {relation} ORDER BY 1"), "index"),
                        (format!("SELECT conname, pg_get_constraintdef(oid) FROM pg_constraint WHERE conrelid = {relation} ORDER BY 1"), "key"),
                        (format!("SELECT tgname, pg_get_triggerdef(oid) FROM pg_trigger WHERE tgrelid = {relation} AND NOT tgisinternal ORDER BY 1"), "trigger"),
                    ] {
                        for row in link.all(&asked)? {
                            out.push(node(&text(&row, 0), kind, &text(&row, 1), true, Vec::new()));
                        }
                    }
                }
                ("mysql", []) => {
                    for row in link.all("SELECT SCHEMA_NAME FROM information_schema.SCHEMATA ORDER BY SCHEMA_NAME")? {
                        let name = text(&row, 0);
                        out.push(node(&name, "database", "", false, vec![name.clone()]));
                    }
                }
                ("mysql", [schema]) => {
                    let s = lit(schema);
                    for row in link.all(&format!("SELECT name, kind FROM (SELECT TABLE_NAME AS name, IF(TABLE_TYPE = 'VIEW', 'view', 'table') AS kind FROM information_schema.TABLES WHERE TABLE_SCHEMA = {s} UNION ALL SELECT ROUTINE_NAME, LOWER(ROUTINE_TYPE) FROM information_schema.ROUTINES WHERE ROUTINE_SCHEMA = {s}) o ORDER BY kind, name"))? {
                        let (name, kind) = (text(&row, 0), text(&row, 1));
                        out.push(node(&name, &kind, "", !holds_parts(&kind), vec![schema.clone(), kind.clone(), name.clone()]));
                    }
                }
                ("mysql", [schema, _, name]) => {
                    let (s, t) = (lit(schema), lit(name));
                    for row in link.all(&format!("SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COLUMN_DEFAULT, EXTRA FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = {s} AND TABLE_NAME = {t} ORDER BY ORDINAL_POSITION"))? {
                        let detail = [text(&row, 1), if text(&row, 2) == "NO" { "not null".into() } else { String::new() }, if row.get(3).cloned().flatten().is_some() { format!("default {}", text(&row, 3)) } else { String::new() }, text(&row, 4)].into_iter().filter(|part| !part.is_empty()).collect::<Vec<_>>().join(" ");
                        out.push(node(&text(&row, 0), "column", &detail, true, Vec::new()));
                    }
                    for (asked, kind) in [
                        (format!("SELECT INDEX_NAME, CONCAT(IF(MAX(NON_UNIQUE) = 0, 'unique ', ''), '(', GROUP_CONCAT(COLUMN_NAME ORDER BY SEQ_IN_INDEX SEPARATOR ', '), ')') FROM information_schema.STATISTICS WHERE TABLE_SCHEMA = {s} AND TABLE_NAME = {t} GROUP BY INDEX_NAME ORDER BY INDEX_NAME"), "index"),
                        (format!("SELECT CONSTRAINT_NAME, CONCAT('(', GROUP_CONCAT(COLUMN_NAME ORDER BY ORDINAL_POSITION SEPARATOR ', '), ') references ', REFERENCED_TABLE_NAME) FROM information_schema.KEY_COLUMN_USAGE WHERE TABLE_SCHEMA = {s} AND TABLE_NAME = {t} AND REFERENCED_TABLE_NAME IS NOT NULL GROUP BY CONSTRAINT_NAME, REFERENCED_TABLE_NAME ORDER BY CONSTRAINT_NAME"), "key"),
                        (format!("SELECT TRIGGER_NAME, CONCAT(ACTION_TIMING, ' ', EVENT_MANIPULATION) FROM information_schema.TRIGGERS WHERE EVENT_OBJECT_SCHEMA = {s} AND EVENT_OBJECT_TABLE = {t} ORDER BY TRIGGER_NAME"), "trigger"),
                    ] {
                        for row in link.all(&asked)? {
                            out.push(node(&text(&row, 0), kind, &text(&row, 1), true, Vec::new()));
                        }
                    }
                }
                ("sqlite", []) => {
                    for row in link.all("SELECT name FROM pragma_database_list ORDER BY seq")? {
                        let name = text(&row, 1.min(row.len().saturating_sub(1)));
                        out.push(node(&name, "database", "", false, vec![name.clone()]));
                    }
                }
                ("sqlite", [schema]) => {
                    let master = format!("{}.sqlite_master", quote_ident(schema, dialect));
                    for row in link.all(&format!("SELECT name, type FROM {master} WHERE type IN ('table', 'view') AND name NOT LIKE 'sqlite\\_%' ESCAPE '\\' ORDER BY type, name"))? {
                        let (name, kind) = (text(&row, 0), text(&row, 1));
                        out.push(node(&name, &kind, "", false, vec![schema.clone(), kind.clone(), name.clone()]));
                    }
                }
                ("sqlite", [schema, _, name]) => {
                    let (s, t) = (lit(schema), lit(name));
                    for row in link.all(&format!("SELECT name, type, \"notnull\", dflt_value, pk FROM pragma_table_xinfo({t}, {s}) ORDER BY cid"))? {
                        let detail = [text(&row, 1), if text(&row, 2) == "1" { "not null".into() } else { String::new() }, if row.get(3).cloned().flatten().is_some() { format!("default {}", text(&row, 3)) } else { String::new() }, if text(&row, 4) != "0" { "primary key".into() } else { String::new() }].into_iter().filter(|part| !part.is_empty()).collect::<Vec<_>>().join(" ");
                        out.push(node(&text(&row, 0), "column", &detail, true, Vec::new()));
                    }
                    let master = format!("{}.sqlite_master", quote_ident(schema, dialect));
                    for row in link.all(&format!("SELECT name, type, sql FROM {master} WHERE type IN ('index', 'trigger') AND tbl_name = {t} ORDER BY type, name"))? {
                        out.push(node(&text(&row, 0), &text(&row, 1), &text(&row, 2), true, Vec::new()));
                    }
                    for row in link.all(&format!("SELECT \"from\", \"table\", \"to\" FROM pragma_foreign_key_list({t}, {s})"))? {
                        out.push(node(&text(&row, 0), "key", &format!("references {}({})", text(&row, 1), text(&row, 2)), true, Vec::new()));
                    }
                }
                _ => {}
            }
            Ok(out)
        })
    }

    fn history_file(&self) -> Option<PathBuf> {
        let folder = crate::home::folder()?.join("databases");
        std::fs::create_dir_all(&folder).ok()?;
        Some(folder.join(format!("{}.jsonl", &crate::digest::sha256(self.config.account().as_bytes())[..16])))
    }

    /// Keeps a statement run in the connection's history, the newest HISTORY of them.
    fn remember(&self, statement: &str, took: u128, said: Result<String, String>) {
        let Some(file) = self.history_file() else { return };
        let entry = match said {
            Ok(tag) => json!({"at": now_ms() as u64, "sql": statement, "ms": took as u64, "said": tag}),
            Err(error) => json!({"at": now_ms() as u64, "sql": statement, "ms": took as u64, "failed": error}),
        };
        let held = std::fs::read_to_string(&file).unwrap_or_default();
        let mut lines: Vec<&str> = held.lines().collect();
        let entry = entry.to_string();
        lines.push(&entry);
        let start = lines.len().saturating_sub(HISTORY);
        let _ = std::fs::write(&file, lines[start..].join("\n") + "\n");
    }

    /// The statements run on the connection, the newest first.
    pub fn history(&self) -> Vec<Value> {
        let Some(file) = self.history_file() else { return Vec::new() };
        let text = std::fs::read_to_string(file).unwrap_or_default();
        text.lines().rev().filter_map(|line| serde_json::from_str(line).ok()).collect()
    }
}

/// The span of the statement `text` holds at `offset`, counted in characters, as the editor counts
/// them; none where the place stands in no statement.
pub fn statement_at(text: &str, offset: usize, kind: &str) -> Option<(usize, usize)> {
    let byte = text.char_indices().nth(offset).map(|(at, _)| at).unwrap_or(text.len());
    let span = sql::statement_at(text, byte, Dialect::of(kind))?;
    Some((text[..span.start].chars().count(), text[..span.end].chars().count()))
}

/// Each statement of `text`, by its span in characters.
pub fn statements(text: &str, kind: &str) -> Vec<(usize, usize)> {
    sql::statements(text, Dialect::of(kind)).into_iter().map(|span| (text[..span.start].chars().count(), text[..span.end].chars().count())).collect()
}

/// What the window says of each connection: its settings, and whether it is open, with its
/// transaction's state and its session's zone.
pub fn listed(root: &Path, open: &BTreeMap<String, Arc<Open>>) -> Result<Vec<Value>, String> {
    Ok(read(root)?
        .into_iter()
        .map(|connection| {
            let opened = open.get(&connection.name);
            let kept = connection.kind != "sqlite" && keychain::read(&connection.account()).is_some();
            json!({
                "connection": connection,
                "open": opened.is_some(),
                "version": opened.map(|one| one.version.clone()),
                "transaction": opened.map(|one| one.transaction()),
                "zone": opened.and_then(|one| one.zone.lock().ok().map(|zone| zone.clone())),
                "passwordKept": kept,
            })
        })
        .collect())
}

/// Where a test server listens: the host:port ORIOR_<name>_ADDRESS names, as the test harness gives
/// it, else 127.0.0.1 at `port`.
#[cfg(test)]
pub(crate) fn test_server(name: &str, port: u16) -> (String, u16) {
    std::env::var(format!("ORIOR_{name}_ADDRESS"))
        .ok()
        .and_then(|named| {
            let (host, port) = named.rsplit_once(':')?;
            Some((host.to_string(), port.parse().ok()?))
        })
        .unwrap_or_else(|| ("127.0.0.1".to_string(), port))
}

#[cfg(test)]
mod connections {
    use super::*;

    fn tree(name: &str) -> PathBuf {
        let root = std::env::temp_dir().join(format!("orior-db-{name}-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&root);
        std::fs::create_dir_all(&root).unwrap();
        root
    }

    #[test]
    fn databases_json_keeps_each_connection_by_its_name() {
        let root = tree("json");
        let lite = Connection { name: "local".into(), kind: "sqlite".into(), file: "data/app.db".into(), ..Connection::default() };
        save(&root, lite.clone(), None).unwrap();
        let pg = Connection { name: "prod".into(), kind: "postgres".into(), host: "db.example".into(), user: "app".into(), database: "app".into(), ssh: "me@bastion".into(), matters: true, guard: "read-only".into(), ..Connection::default() };
        save(&root, pg.clone(), None).unwrap();
        assert_eq!(read(&root).unwrap(), vec![lite.clone(), pg.clone()]);
        assert!(save(&root, Connection { name: "local".into(), ..pg.clone() }, Some("prod")).is_err());
        save(&root, Connection { name: "live".into(), ..pg.clone() }, Some("prod")).unwrap();
        assert_eq!(read(&root).unwrap().iter().map(|one| one.name.as_str()).collect::<Vec<_>>(), vec!["local", "live"]);
        assert!(!std::fs::read_to_string(root.join("databases.json")).unwrap().contains("password"));
        remove(&root, "local").unwrap();
        assert_eq!(read(&root).unwrap().len(), 1);
        assert_eq!(pg.account(), "postgres://app@db.example:5432/app via me@bastion");
        let _ = std::fs::remove_dir_all(&root);
    }

    #[test]
    fn the_keychain_keeps_a_password_and_forgets_it() {
        let account = format!("test://orior@{}", std::process::id());
        if keychain::keep(&account, "kept secret").is_err() {
            return;
        }
        assert_eq!(keychain::read(&account).as_deref(), Some("kept secret"));
        keychain::forget(&account).unwrap();
        assert_eq!(keychain::read(&account), None);
    }

    #[test]
    fn a_sqlite_database_runs_pages_guards_and_reads_its_schema() {
        if crate::test_runs::python().is_none() {
            return;
        }
        let root = tree("sqlite");
        let connection = Connection { name: "lite".into(), kind: "sqlite".into(), file: "app.db".into(), ..Connection::default() };
        let open = Open::connect(&root, connection, None).unwrap();
        assert!(!open.version.is_empty());
        open.run("CREATE TABLE t (n INTEGER PRIMARY KEY, name TEXT NOT NULL DEFAULT 'x', b BLOB)", true, 100).unwrap();
        open.run("CREATE INDEX t_name ON t (name)", true, 100).unwrap();
        let made = open.run("INSERT INTO t (n, name, b) WITH RECURSIVE k(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM k WHERE i < 1000) SELECT i, 'n' || i, x'00ff' FROM k", true, 100).unwrap();
        assert_eq!(made["done"], "1000 rows affected");
        let first = open.run("SELECT n, name, b, NULL AS z FROM t ORDER BY n", false, 100).unwrap();
        assert_eq!(first["rows"].as_array().unwrap().len(), 100);
        assert_eq!(first["columns"][0]["numeric"], true);
        assert_eq!(first["rows"][0], json!(["1", "n1", "0x00FF", null]));
        assert!(first["done"].is_null());
        let later = open.rows(950, 100).unwrap();
        assert_eq!((later["rows"].as_array().unwrap().len(), later["rows"][0][0].as_str()), (50, Some("951")));
        assert_eq!(later["done"], "1000 rows read");
        let asked = open.run("DELETE FROM t", false, 100).unwrap();
        assert_eq!((asked["ask"]["why"].as_str(), asked["ask"]["count"].as_str()), (Some("every row"), Some("1000")));
        assert_eq!(open.run("SELECT count(*) FROM t", false, 10).unwrap()["rows"][0][0], "1000");
        open.run("BEGIN", true, 10).unwrap();
        open.run("DELETE FROM t WHERE n > 10", false, 10).unwrap();
        assert_eq!(open.transaction(), "open");
        open.run("ROLLBACK", true, 10).unwrap();
        assert_eq!(open.transaction(), "idle");
        let failed = open.run("SELECT * FROM none", false, 10).unwrap_err();
        assert!(failed.message.contains("no such table"), "{failed}");
        let schemas = open.children(&[]).unwrap();
        assert_eq!(schemas[0]["label"], "main");
        let objects = open.children(&["main".to_string()]).unwrap();
        assert_eq!(objects.iter().map(|one| one["label"].as_str().unwrap()).collect::<Vec<_>>(), vec!["t"]);
        let parts = open.children(&["main".into(), "table".into(), "t".into()]).unwrap();
        let shown: Vec<(String, String)> = parts.iter().map(|one| (one["kind"].as_str().unwrap().to_string(), one["label"].as_str().unwrap().to_string())).collect();
        assert_eq!(shown, vec![("column".to_string(), "n".to_string()), ("column".into(), "name".into()), ("column".into(), "b".into()), ("index".into(), "t_name".into())]);
        assert_eq!(parts[1]["detail"], "TEXT not null default 'x'");
        let export = root.join("t.csv");
        open.run("SELECT n, name FROM t ORDER BY n", false, 10).unwrap();
        let written = open.export(&export, "csv").unwrap();
        assert_eq!(written["written"], 1000);
        let text = std::fs::read_to_string(&export).unwrap();
        assert_eq!(text.lines().count(), 1001);
        assert!(text.starts_with("n,name\n1,n1\n"));
        let history = open.history();
        assert!(history.iter().any(|one| one["sql"] == "SELECT * FROM none" && one["failed"].is_string()));
        let shaped = open.run("ALTER TABLE t ADD COLUMN extra TEXT", true, 10).unwrap();
        assert_eq!(shaped["shaped"]["name"], "t");
        open.close();
        let _ = std::fs::remove_dir_all(&root);
    }

    #[test]
    fn a_slow_sqlite_statement_is_stopped() {
        if crate::test_runs::python().is_none() {
            return;
        }
        let root = tree("stop");
        let open = Arc::new(Open::connect(&root, Connection { name: "lite".into(), kind: "sqlite".into(), file: "slow.db".into(), ..Connection::default() }, None).unwrap());
        let stopper = open.clone();
        let thread = std::thread::spawn(move || {
            std::thread::sleep(Duration::from_millis(500));
            stopper.stop().unwrap();
        });
        let started = Instant::now();
        let stopped = open.run("WITH RECURSIVE k(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM k) SELECT count(*) FROM k", false, 10).unwrap_err();
        thread.join().unwrap();
        assert!(stopped.message.contains("interrupt"), "{stopped}");
        assert!(started.elapsed() < Duration::from_secs(10));
        assert_eq!(open.run("SELECT 1", false, 10).unwrap()["rows"][0][0], "1");
        open.close();
        let _ = std::fs::remove_dir_all(&root);
    }

    #[test]
    fn a_postgres_connection_that_matters_guards_its_writes() {
        let Ok(password) = std::env::var("ORIOR_PG_PASSWORD") else { return };
        let root = tree("pg");
        let (host, port) = test_server("PG", 5432);
        let base = Connection { name: "pg".into(), kind: "postgres".into(), host, port, user: "postgres".into(), database: "postgres".into(), ..Connection::default() };
        let open = Open::connect(&root, base.clone(), Some(password.clone())).unwrap();
        open.run("DROP TABLE IF EXISTS orior_guard", true, 10).unwrap();
        open.run("CREATE TABLE orior_guard (n int)", true, 10).unwrap();
        open.run("INSERT INTO orior_guard SELECT generate_series(1, 42)", true, 10).unwrap();
        assert!(!open.zone.lock().unwrap().is_empty());
        let schemas = open.children(&[]).unwrap();
        assert!(schemas.iter().any(|one| one["label"] == "public"));
        let objects = open.children(&["public".into()]).unwrap();
        assert!(objects.iter().any(|one| one["label"] == "orior_guard" && one["kind"] == "table"));
        let parts = open.children(&["public".into(), "table".into(), "orior_guard".into()]).unwrap();
        assert_eq!((parts[0]["label"].as_str(), parts[0]["detail"].as_str()), (Some("n"), Some("integer")));
        assert_eq!(open.run("UPDATE orior_guard SET n = 0", false, 10).unwrap()["ask"]["count"], "42");
        open.close();
        let asks = Open::connect(&root, Connection { matters: true, guard: "ask".into(), ..base.clone() }, Some(password.clone())).unwrap();
        assert_eq!(asks.run("INSERT INTO orior_guard VALUES (1)", false, 10).unwrap()["ask"]["why"], "matters");
        assert!(asks.run("SELECT 1", false, 10).unwrap()["ask"].is_null());
        asks.close();
        let read_only = Open::connect(&root, Connection { matters: true, guard: "read-only".into(), ..base.clone() }, Some(password.clone())).unwrap();
        let refused = read_only.run("INSERT INTO orior_guard VALUES (1)", false, 10).unwrap_err();
        assert_eq!(refused.code, "25006");
        read_only.close();
        let cleanup = Open::connect(&root, base, Some(password)).unwrap();
        cleanup.run("DROP TABLE orior_guard", true, 10).unwrap();
        cleanup.close();
        let _ = std::fs::remove_dir_all(&root);
    }

    #[test]
    fn a_mysql_connection_reads_its_schema_and_its_zone() {
        let Ok(password) = std::env::var("ORIOR_MYSQL_PASSWORD") else { return };
        let root = tree("mysql");
        let (host, port) = test_server("MYSQL", 3306);
        let open = Open::connect(&root, Connection { name: "my".into(), kind: "mysql".into(), host, port, user: "root".into(), ..Connection::default() }, Some(password)).unwrap();
        open.run("CREATE DATABASE IF NOT EXISTS orior_schema", true, 10).unwrap();
        open.run("CREATE TABLE IF NOT EXISTS orior_schema.t (id INT PRIMARY KEY, name VARCHAR(20) NOT NULL, KEY by_name (name))", true, 10).unwrap();
        assert!(!open.zone.lock().unwrap().is_empty());
        assert!(open.children(&[]).unwrap().iter().any(|one| one["label"] == "orior_schema"));
        let parts = open.children(&["orior_schema".into(), "table".into(), "t".into()]).unwrap();
        let shown: Vec<(&str, &str)> = parts.iter().map(|one| (one["kind"].as_str().unwrap(), one["label"].as_str().unwrap())).collect();
        assert_eq!(shown, vec![("column", "id"), ("column", "name"), ("index", "by_name"), ("index", "PRIMARY")]);
        open.run("DROP DATABASE orior_schema", true, 10).unwrap();
        open.close();
        let _ = std::fs::remove_dir_all(&root);
    }
}

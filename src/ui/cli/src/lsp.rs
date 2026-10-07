// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! A client of a language server, such as clangd, over its input and output: each message a
//! Content-Length header and a JSON body, as the Language Server Protocol frames them. A request waits
//! for its answer up to a time it is given; a notification from the server, such as the diagnostics
//! of a file, goes to the function the server was started with. A request from the server is
//! answered with nothing, which the servers orior starts take as no.
//!
//! The editor counts a line's columns in UTF-16 units, as the protocol does where it is not told
//! otherwise: positions pass between them as they are. The whole text goes with each change.

use std::collections::HashMap;
use std::io::{BufRead, BufReader, Write};
use std::path::{Path, PathBuf};
use std::process::{Child, ChildStdin, Command, Stdio};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::mpsc::{self, Sender};
use std::sync::{Arc, Mutex};
use std::time::Duration;

use serde_json::{json, Value};

/// What the server says on its own: the method and its params.
pub type Heard = Box<dyn Fn(&str, &Value) + Send + Sync>;

type Waiting = Arc<Mutex<HashMap<u64, Sender<Result<Value, String>>>>>;

pub struct Server {
    child: Mutex<Child>,
    input: Arc<Mutex<ChildStdin>>,
    next: AtomicU64,
    waiting: Waiting,
    versions: Mutex<HashMap<String, i64>>,
}

/// How long the server has to answer that it started.
const STARTING: Duration = Duration::from_secs(30);

fn send(input: &Mutex<ChildStdin>, message: &Value) -> Result<(), String> {
    let body = message.to_string();
    let mut input = input.lock().map_err(|_| "the server's input is held".to_string())?;
    write!(input, "Content-Length: {}\r\n\r\n{body}", body.len()).and_then(|()| input.flush()).map_err(|error| format!("the server stopped: {error}"))
}

/// One message from `reader`, or None where the server's output ended.
fn read_message(reader: &mut impl BufRead) -> Option<Value> {
    let mut length = None;
    loop {
        let mut line = String::new();
        if reader.read_line(&mut line).ok()? == 0 {
            return None;
        }
        let line = line.trim_end();
        if line.is_empty() {
            break;
        }
        if let Some(value) = line.strip_prefix("Content-Length:") {
            length = value.trim().parse::<usize>().ok();
        }
    }
    let mut body = vec![0u8; length?];
    reader.read_exact(&mut body).ok()?;
    serde_json::from_slice(&body).ok()
}

/// A path as the protocol names a file: file:///D:/a/b%20c on Windows, file:///a/b elsewhere.
pub fn uri_of(path: &Path) -> String {
    let text = path.display().to_string().replace('\\', "/");
    let mut out = String::from(if text.starts_with('/') { "file://" } else { "file:///" });
    for byte in text.bytes() {
        if byte.is_ascii_alphanumeric() || b"-._~/:".contains(&byte) {
            out.push(byte as char);
        } else {
            out.push_str(&format!("%{byte:02X}"));
        }
    }
    out
}

/// The path a file: URI names.
pub fn path_of(uri: &str) -> Option<PathBuf> {
    let rest = uri.strip_prefix("file://")?;
    let bytes = rest.as_bytes();
    let mut out = Vec::new();
    let mut at = 0;
    while at < bytes.len() {
        if bytes[at] == b'%' && at + 3 <= bytes.len() {
            if let Some(byte) = std::str::from_utf8(&bytes[at + 1..at + 3]).ok().and_then(|hex| u8::from_str_radix(hex, 16).ok()) {
                out.push(byte);
                at += 3;
                continue;
            }
        }
        out.push(bytes[at]);
        at += 1;
    }
    let text = String::from_utf8(out).ok()?;
    // file:///D:/x names D:/x, and file:///x names /x.
    let text = if text.len() > 2 && text.as_bytes()[0] == b'/' && text.as_bytes()[2] == b':' { text[1..].to_string() } else { text };
    Some(PathBuf::from(text))
}

impl Server {
    /// Starts `program` with `args` at `root`, and has it read the tree there. `heard` takes what the
    /// server says on its own.
    pub fn start(program: &Path, args: &[String], root: &Path, path_env: std::ffi::OsString, heard: Heard) -> Result<Server, String> {
        let mut command = Command::new(program);
        command.args(args).current_dir(root).env("PATH", path_env).stdin(Stdio::piped()).stdout(Stdio::piped()).stderr(Stdio::null());
        crate::runner::quiet(&mut command);
        let mut child = command.spawn().map_err(|error| format!("{}: {error}", program.display()))?;
        let input = Arc::new(Mutex::new(child.stdin.take().ok_or("the server took no input")?));
        let output = child.stdout.take().ok_or("the server gave no output")?;
        let waiting: Waiting = Arc::new(Mutex::new(HashMap::new()));
        let (reply_to, answers) = (input.clone(), waiting.clone());
        std::thread::spawn(move || {
            let mut reader = BufReader::new(output);
            while let Some(message) = read_message(&mut reader) {
                let id = message.get("id").cloned();
                if let Some(method) = message.get("method").and_then(Value::as_str) {
                    if let Some(id) = id {
                        let _ = send(&reply_to, &json!({"jsonrpc": "2.0", "id": id, "result": null}));
                    } else {
                        heard(method, message.get("params").unwrap_or(&Value::Null));
                    }
                    continue;
                }
                let Some(id) = id.and_then(|id| id.as_u64()) else {
                    continue;
                };
                let answer = match message.get("error") {
                    Some(error) => Err(error.get("message").and_then(Value::as_str).unwrap_or("the server refused").to_string()),
                    None => Ok(message.get("result").cloned().unwrap_or(Value::Null)),
                };
                if let Some(sender) = answers.lock().ok().and_then(|mut all| all.remove(&id)) {
                    let _ = sender.send(answer);
                }
            }
            if let Ok(mut all) = answers.lock() {
                for (_, sender) in all.drain() {
                    let _ = sender.send(Err("the server stopped".into()));
                }
            }
        });
        let server = Server { child: Mutex::new(child), input, next: AtomicU64::new(1), waiting, versions: Mutex::new(HashMap::new()) };
        let root_uri = uri_of(root);
        let name = root.file_name().map(|name| name.to_string_lossy().to_string()).unwrap_or_default();
        let capabilities = json!({
            "general": {"positionEncodings": ["utf-16"]},
            "textDocument": {
                "synchronization": {"didSave": false},
                "hover": {"contentFormat": ["markdown", "plaintext"]},
                "completion": {"completionItem": {"snippetSupport": false, "documentationFormat": ["plaintext"]}},
                "definition": {"linkSupport": false},
                "publishDiagnostics": {"relatedInformation": false}
            },
            "window": {"workDoneProgress": false}
        });
        server.request(
            "initialize",
            json!({"processId": std::process::id(), "rootUri": root_uri, "workspaceFolders": [{"uri": root_uri, "name": name}], "capabilities": capabilities, "clientInfo": {"name": "orior"}}),
            STARTING,
        )?;
        server.notify("initialized", json!({}))?;
        Ok(server)
    }

    pub fn request(&self, method: &str, params: Value, patience: Duration) -> Result<Value, String> {
        let id = self.next.fetch_add(1, Ordering::SeqCst);
        let (sender, answer) = mpsc::channel();
        self.waiting.lock().map_err(|_| "held".to_string())?.insert(id, sender);
        send(&self.input, &json!({"jsonrpc": "2.0", "id": id, "method": method, "params": params}))?;
        match answer.recv_timeout(patience) {
            Ok(answer) => answer,
            Err(_) => {
                if let Ok(mut all) = self.waiting.lock() {
                    all.remove(&id);
                }
                let _ = self.notify("$/cancelRequest", json!({"id": id}));
                Err(format!("{method} had no answer in {} s", patience.as_secs()))
            }
        }
    }

    pub fn notify(&self, method: &str, params: Value) -> Result<(), String> {
        send(&self.input, &json!({"jsonrpc": "2.0", "method": method, "params": params}))
    }

    pub fn open(&self, path: &Path, language_id: &str, text: &str) -> Result<(), String> {
        let uri = uri_of(path);
        self.versions.lock().map_err(|_| "held".to_string())?.insert(uri.clone(), 1);
        self.notify("textDocument/didOpen", json!({"textDocument": {"uri": uri, "languageId": language_id, "version": 1, "text": text}}))
    }

    pub fn change(&self, path: &Path, text: &str) -> Result<(), String> {
        let uri = uri_of(path);
        let version = {
            let mut versions = self.versions.lock().map_err(|_| "held".to_string())?;
            let version = versions.entry(uri.clone()).or_insert(1);
            *version += 1;
            *version
        };
        self.notify("textDocument/didChange", json!({"textDocument": {"uri": uri, "version": version}, "contentChanges": [{"text": text}]}))
    }

    pub fn close(&self, path: &Path) -> Result<(), String> {
        let uri = uri_of(path);
        self.versions.lock().map_err(|_| "held".to_string())?.remove(&uri);
        self.notify("textDocument/didClose", json!({"textDocument": {"uri": uri}}))
    }

    /// Whether the server has `path` open.
    pub fn has(&self, path: &Path) -> bool {
        self.versions.lock().map(|versions| versions.contains_key(&uri_of(path))).unwrap_or(false)
    }

    /// The params of a request about the place `line`, `col` of `path`.
    pub fn at(path: &Path, line: u32, col: u32) -> Value {
        json!({"textDocument": {"uri": uri_of(path)}, "position": {"line": line, "character": col}})
    }

    /// Asks the server to end, and ends it where it does not.
    pub fn stop(&self) {
        let _ = self.request("shutdown", Value::Null, Duration::from_secs(2));
        let _ = self.notify("exit", Value::Null);
        if let Ok(mut child) = self.child.lock() {
            std::thread::sleep(Duration::from_millis(100));
            let _ = child.kill();
            let _ = child.wait();
        }
    }
}

impl Drop for Server {
    fn drop(&mut self) {
        if let Ok(mut child) = self.child.lock() {
            let _ = child.kill();
            let _ = child.wait();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_path_and_its_uri_name_each_other() {
        let path = if cfg!(windows) { PathBuf::from("D:\\a b\\c#.cu") } else { PathBuf::from("/a b/c#.cu") };
        let uri = uri_of(&path);
        assert!(uri.starts_with("file:///"));
        assert!(uri.contains("a%20b/c%23.cu"));
        let back = path_of(&uri).unwrap();
        assert_eq!(back.display().to_string().replace('\\', "/"), path.display().to_string().replace('\\', "/"));
    }

    #[test]
    fn a_message_is_read_by_its_length() {
        let body = r#"{"jsonrpc":"2.0","id":3,"result":{"x":1}}"#;
        let framed = format!("Content-Length: {}\r\nContent-Type: application/vscode-jsonrpc\r\n\r\n{body}Content-Length: 2\r\n\r\n{{}}", body.len());
        let mut reader = BufReader::new(framed.as_bytes());
        assert_eq!(read_message(&mut reader).unwrap()["result"]["x"], 1);
        assert_eq!(read_message(&mut reader).unwrap(), json!({}));
        assert!(read_message(&mut reader).is_none());
    }
}

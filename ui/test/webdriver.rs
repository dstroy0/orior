// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! A WebDriver session of the harness's own, spoken over HTTP to WebKitWebDriver: the session
//! opened on orior's window, scripts run in its page and answered as JSON, and input sent as
//! WebDriver's actions, the page receiving each as it does from the mouse and keyboard.

use std::io::{Read, Write};
use std::net::TcpStream;
use std::time::Duration;

use serde_json::{json, Value};

/// How long a command other than a script is waited for.
const WAIT: Duration = Duration::from_secs(30);

/// Sends `method` to `path` at 127.0.0.1:`port` with `body`, and gives what the answer's JSON
/// holds under `value`, or the error it names.
pub fn request(port: u16, method: &str, path: &str, body: Option<&Value>, wait: Duration) -> Result<Value, String> {
    let mut stream = TcpStream::connect(("127.0.0.1", port)).map_err(|error| format!("WebDriver at {port}: {error}"))?;
    stream.set_read_timeout(Some(wait)).ok();
    let text = body.map(Value::to_string).unwrap_or_default();
    let head = format!("{method} {path} HTTP/1.1\r\nHost: 127.0.0.1:{port}\r\nContent-Type: application/json; charset=utf-8\r\nContent-Length: {}\r\nConnection: close\r\n\r\n", text.len());
    stream.write_all(head.as_bytes()).and_then(|_| stream.write_all(text.as_bytes())).map_err(|error| format!("WebDriver: {error}"))?;
    let mut answer = Vec::new();
    stream.read_to_end(&mut answer).map_err(|error| format!("WebDriver {method} {path}: {error}"))?;
    let answer = String::from_utf8_lossy(&answer);
    let body = answer.split_once("\r\n\r\n").map(|(_, body)| body).unwrap_or("");
    let read: Value = serde_json::from_str(body).map_err(|error| format!("WebDriver {method} {path} answered {}: {error}", body.chars().take(200).collect::<String>()))?;
    let value = read.get("value").cloned().unwrap_or(Value::Null);
    if let Some(error) = value.get("error").and_then(Value::as_str) {
        return Err(format!("WebDriver {method} {path}: {error}: {}", value["message"].as_str().unwrap_or("")));
    }
    Ok(value)
}

/// Whether a WebDriver answers at `port`.
pub fn answers(port: u16) -> bool {
    request(port, "GET", "/status", None, Duration::from_secs(2)).is_ok()
}

/// One session on a window.
pub struct Session {
    port: u16,
    id: String,
}

impl Session {
    /// Opens a session on the program `binary` launches, the WebDriver at `port` starting it.
    pub fn open(port: u16, binary: &str) -> Result<Session, String> {
        let asked = json!({"capabilities": {"alwaysMatch": {"webkitgtk:browserOptions": {"binary": binary, "args": []}}}});
        let opened = request(port, "POST", "/session", Some(&asked), Duration::from_secs(180))?;
        let id = opened["sessionId"].as_str().ok_or("WebDriver gave no session")?.to_string();
        let session = Session { port, id };
        session.command("POST", "/timeouts", json!({"script": 1_800_000, "pageLoad": 120_000}))?;
        Ok(session)
    }

    fn command(&self, method: &str, path: &str, body: Value) -> Result<Value, String> {
        self.command_waiting(method, path, body, WAIT)
    }

    fn command_waiting(&self, method: &str, path: &str, body: Value, wait: Duration) -> Result<Value, String> {
        let sent = (method != "GET" && method != "DELETE").then_some(&body);
        request(self.port, method, &format!("/session/{}{path}", self.id), sent, wait)
    }

    /// Runs `script`, a function's body given `args`, in the page, and gives what it returns.
    pub fn execute(&self, script: &str, args: Value) -> Result<Value, String> {
        self.command("POST", "/execute/sync", json!({"script": script, "args": args}))
    }

    /// Runs `script` in the page with a callback as its last argument, and gives what it is called
    /// with, waiting up to `wait`.
    pub fn execute_async(&self, script: &str, args: Value, wait: Duration) -> Result<Value, String> {
        self.command_waiting("POST", "/execute/async", json!({"script": script, "args": args}), wait)
    }

    /// Performs WebDriver's actions: each source a list of ticks, run side by side.
    pub fn actions(&self, sources: Value) -> Result<(), String> {
        self.command("POST", "/actions", json!({"actions": sources})).map(|_| ())
    }

    /// Lets go of every key and button the actions left held.
    pub fn release(&self) -> Result<(), String> {
        self.command("DELETE", "/actions", Value::Null).map(|_| ())
    }

    /// The page's address.
    pub fn url(&self) -> Result<String, String> {
        self.command("GET", "/url", Value::Null).map(|url| url.as_str().unwrap_or("").to_string())
    }

    /// Ends the session, which closes the window.
    pub fn end(&self) {
        let _ = request(self.port, "DELETE", &format!("/session/{}", self.id), None, WAIT);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::net::TcpListener;

    /// A WebDriver of the test's own that answers each request with `body`, and gives what it was sent.
    fn answering(body: &'static str) -> (u16, std::thread::JoinHandle<String>) {
        let listener = TcpListener::bind(("127.0.0.1", 0)).unwrap();
        let port = listener.local_addr().unwrap().port();
        let heard = std::thread::spawn(move || {
            let (mut stream, _) = listener.accept().unwrap();
            stream.set_read_timeout(Some(Duration::from_secs(5))).unwrap();
            let mut sent = Vec::new();
            let mut chunk = [0u8; 4096];
            loop {
                let read = stream.read(&mut chunk).unwrap_or(0);
                sent.extend_from_slice(&chunk[..read]);
                let text = String::from_utf8_lossy(&sent).to_string();
                if let Some((head, rest)) = text.split_once("\r\n\r\n") {
                    let length: usize = head.lines().find_map(|line| line.strip_prefix("Content-Length: ")).and_then(|n| n.trim().parse().ok()).unwrap_or(0);
                    if rest.len() >= length {
                        break;
                    }
                }
                if read == 0 {
                    break;
                }
            }
            stream.write_all(format!("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: {}\r\n\r\n{body}", body.len()).as_bytes()).unwrap();
            String::from_utf8_lossy(&sent).to_string()
        });
        (port, heard)
    }

    #[test]
    fn a_value_is_what_the_answer_holds() {
        let (port, heard) = answering(r#"{"value": {"ready": true}}"#);
        let value = request(port, "POST", "/session/a/execute/sync", Some(&json!({"script": "return 1", "args": []})), WAIT).unwrap();
        assert_eq!(value["ready"], true);
        let sent = heard.join().unwrap();
        assert!(sent.starts_with("POST /session/a/execute/sync HTTP/1.1\r\n"), "{sent}");
        assert!(sent.ends_with(r#"{"args":[],"script":"return 1"}"#), "{sent}");
    }

    #[test]
    fn an_error_is_said_with_its_message() {
        let (port, _) = answering(r#"{"value": {"error": "javascript error", "message": "x is not defined"}}"#);
        let said = request(port, "POST", "/session/a/execute/sync", Some(&json!({})), WAIT).unwrap_err();
        assert!(said.contains("javascript error: x is not defined"), "{said}");
    }

    #[test]
    fn nothing_listening_does_not_answer() {
        let port = TcpListener::bind(("127.0.0.1", 0)).unwrap().local_addr().unwrap().port();
        assert!(!answers(port));
    }
}

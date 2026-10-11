// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The window's debugging port, spoken to over a WebSocket of the harness's own: the targets the
//! port lists, and a DevTools connection that reads on a thread of its own, each answer going to
//! the call that waits for it and each event to the one channel the harness reads.

use std::collections::HashMap;
use std::io::{Read, Write};
use std::net::TcpStream;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::mpsc::{self, Receiver, Sender};
use std::sync::{Arc, Mutex};
use std::time::Duration;

use serde_json::{json, Value};

/// The targets the port at 127.0.0.1:`port` lists, or none while nothing answers.
pub fn targets(port: u16) -> Vec<Value> {
    let Ok(mut stream) = TcpStream::connect_timeout(&([127, 0, 0, 1], port).into(), Duration::from_millis(500)) else { return Vec::new() };
    stream.set_read_timeout(Some(Duration::from_secs(5))).ok();
    if stream.write_all(format!("GET /json HTTP/1.1\r\nHost: 127.0.0.1:{port}\r\nConnection: close\r\n\r\n").as_bytes()).is_err() {
        return Vec::new();
    }
    let mut answer = Vec::new();
    let _ = stream.read_to_end(&mut answer);
    let text = String::from_utf8_lossy(&answer);
    let body = text.split_once("\r\n\r\n").map(|(_, body)| body).unwrap_or("");
    serde_json::from_str::<Value>(body).ok().and_then(|value| value.as_array().cloned()).unwrap_or_default()
}

/// The DevTools address of orior's window: the page the port lists that is served over http.
pub fn window_page(port: u16) -> Option<String> {
    targets(port).into_iter().find(|one| one["type"] == "page" && one["url"].as_str().is_some_and(|url| url.starts_with("http"))).and_then(|one| one["webSocketDebuggerUrl"].as_str().map(str::to_string))
}

pub fn base64(bytes: &[u8]) -> String {
    const LETTERS: &[u8; 64] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    let mut out = String::new();
    for chunk in bytes.chunks(3) {
        let n = (u32::from(chunk[0]) << 16) | (u32::from(*chunk.get(1).unwrap_or(&0)) << 8) | u32::from(*chunk.get(2).unwrap_or(&0));
        for at in 0..4 {
            out.push(if at <= chunk.len() { LETTERS[(n >> (18 - 6 * at) & 63) as usize] as char } else { '=' });
        }
    }
    out
}

/// A DevTools connection.
pub struct Connection {
    writer: Mutex<TcpStream>,
    waiting: Arc<Mutex<HashMap<u64, Sender<Value>>>>,
    next: AtomicU64,
    pub events: Mutex<Receiver<Value>>,
}

impl Connection {
    pub fn open(url: &str) -> Result<Connection, String> {
        let rest = url.strip_prefix("ws://").ok_or_else(|| format!("{url} is no ws:// address"))?;
        let (authority, path) = rest.split_once('/').map(|(host, path)| (host, format!("/{path}"))).unwrap_or((rest, "/".into()));
        let mut stream = TcpStream::connect(authority).map_err(|error| format!("{authority}: {error}"))?;
        let seed = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map(|since| since.as_nanos()).unwrap_or(0).to_le_bytes();
        let key = base64(&seed);
        stream.write_all(format!("GET {path} HTTP/1.1\r\nHost: {authority}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n").as_bytes()).map_err(|error| error.to_string())?;
        let mut head = Vec::new();
        let mut byte = [0u8; 1];
        while !head.ends_with(b"\r\n\r\n") {
            stream.read_exact(&mut byte).map_err(|error| format!("the page closed during the handshake: {error}"))?;
            head.push(byte[0]);
        }
        if !String::from_utf8_lossy(&head).lines().next().unwrap_or("").contains(" 101 ") {
            return Err(format!("the page refused the connection: {}", String::from_utf8_lossy(&head).lines().next().unwrap_or("")));
        }
        let reader = stream.try_clone().map_err(|error| error.to_string())?;
        let waiting: Arc<Mutex<HashMap<u64, Sender<Value>>>> = Arc::default();
        let (tell, events) = mpsc::channel();
        let held = waiting.clone();
        std::thread::spawn(move || read_frames(reader, held, tell));
        Ok(Connection { writer: Mutex::new(stream), waiting, next: AtomicU64::new(1), events: Mutex::new(events) })
    }

    fn send(&self, text: &str) -> Result<(), String> {
        let data = text.as_bytes();
        let mask: [u8; 4] = (self.next.load(Ordering::Relaxed) as u32 ^ 0x5A3C_96E1).to_le_bytes();
        let mut frame = vec![0x81];
        if data.len() < 126 {
            frame.push(0x80 | data.len() as u8);
        } else if data.len() < 65536 {
            frame.push(0x80 | 126);
            frame.extend_from_slice(&(data.len() as u16).to_be_bytes());
        } else {
            frame.push(0x80 | 127);
            frame.extend_from_slice(&(data.len() as u64).to_be_bytes());
        }
        frame.extend_from_slice(&mask);
        frame.extend(data.iter().enumerate().map(|(at, byte)| byte ^ mask[at % 4]));
        self.writer.lock().map_err(|_| "the connection is held".to_string())?.write_all(&frame).map_err(|error| format!("the page closed: {error}"))
    }

    /// Sends a command and gives its result within `wait`.
    pub fn call(&self, method: &str, params: Value, wait: Duration) -> Result<Value, String> {
        let id = self.next.fetch_add(1, Ordering::SeqCst);
        let (tell, hear) = mpsc::channel();
        self.waiting.lock().map_err(|_| "the connection is held")?.insert(id, tell);
        self.send(&json!({"id": id, "method": method, "params": params}).to_string())?;
        let answer = hear.recv_timeout(wait).map_err(|_| {
            if let Ok(mut waiting) = self.waiting.lock() {
                waiting.remove(&id);
            }
            format!("{method} had no answer in {} s", wait.as_secs())
        })?;
        if let Some(error) = answer.get("error") {
            return Err(format!("{method}: {}", error["message"].as_str().unwrap_or("failed")));
        }
        Ok(answer["result"].clone())
    }

    /// The value of `expression` in the page, awaited where it is a promise.
    pub fn evaluate(&self, expression: &str, wait: Duration) -> Result<Value, String> {
        let result = self.call("Runtime.evaluate", json!({"expression": expression, "awaitPromise": true, "returnByValue": true}), wait)?;
        if let Some(details) = result.get("exceptionDetails") {
            return Err(details["exception"]["description"].as_str().or_else(|| details["text"].as_str()).unwrap_or("the expression threw").to_string());
        }
        Ok(result["result"]["value"].clone())
    }
}

fn read_frames(mut stream: TcpStream, waiting: Arc<Mutex<HashMap<u64, Sender<Value>>>>, events: Sender<Value>) {
    let mut read = |count: usize| -> Option<Vec<u8>> {
        let mut out = vec![0; count];
        stream.read_exact(&mut out).ok()?;
        Some(out)
    };
    let mut parts: Vec<u8> = Vec::new();
    loop {
        let Some(head) = read(2) else { break };
        let mut length = (head[1] & 0x7F) as usize;
        if length == 126 {
            let Some(more) = read(2) else { break };
            length = u16::from_be_bytes([more[0], more[1]]) as usize;
        } else if length == 127 {
            let Some(more) = read(8) else { break };
            length = u64::from_be_bytes(more.try_into().unwrap_or_default()) as usize;
        }
        let Some(payload) = read(length) else { break };
        let opcode = head[0] & 0x0F;
        if opcode == 0x8 {
            break;
        }
        if opcode == 0x9 || opcode == 0xA {
            continue;
        }
        parts.extend(payload);
        if head[0] & 0x80 == 0 {
            continue;
        }
        let message: Value = serde_json::from_slice(&std::mem::take(&mut parts)).unwrap_or(Value::Null);
        if let Some(id) = message["id"].as_u64() {
            if let Some(tell) = waiting.lock().ok().and_then(|mut waiting| waiting.remove(&id)) {
                let _ = tell.send(message);
            }
        } else if events.send(message).is_err() {
            break;
        }
    }
    let _ = events.send(json!({"method": "harness.closed"}));
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::net::TcpListener;

    #[test]
    fn base64_is_rfc_4648_s() {
        assert_eq!(base64(b""), "");
        assert_eq!(base64(b"f"), "Zg==");
        assert_eq!(base64(b"fo"), "Zm8=");
        assert_eq!(base64(b"foo"), "Zm9v");
        assert_eq!(base64(b"foobar"), "Zm9vYmFy");
    }

    /// A frame the page sends: final, unmasked text.
    fn frame(text: &str, last: bool) -> Vec<u8> {
        let mut out = vec![if last { 0x81 } else { 0x01 }];
        if text.len() < 126 {
            out.push(text.len() as u8);
        } else {
            out.push(126);
            out.extend_from_slice(&(text.len() as u16).to_be_bytes());
        }
        out.extend_from_slice(text.as_bytes());
        out
    }

    /// A frame the harness sent, unmasked.
    fn read_sent(stream: &mut TcpStream) -> Value {
        let mut head = [0u8; 2];
        stream.read_exact(&mut head).unwrap();
        let mut length = (head[1] & 0x7F) as usize;
        if length == 126 {
            let mut more = [0u8; 2];
            stream.read_exact(&mut more).unwrap();
            length = u16::from_be_bytes(more) as usize;
        }
        assert!(head[1] & 0x80 != 0, "the harness's frames are masked");
        let mut mask = [0u8; 4];
        stream.read_exact(&mut mask).unwrap();
        let mut data = vec![0u8; length];
        stream.read_exact(&mut data).unwrap();
        let data: Vec<u8> = data.iter().enumerate().map(|(at, byte)| byte ^ mask[at % 4]).collect();
        serde_json::from_slice(&data).unwrap()
    }

    #[test]
    fn a_call_is_answered_and_an_event_reaches_the_channel() {
        let listener = TcpListener::bind(("127.0.0.1", 0)).unwrap();
        let port = listener.local_addr().unwrap().port();
        let page = std::thread::spawn(move || {
            let (mut stream, _) = listener.accept().unwrap();
            let mut head = Vec::new();
            let mut byte = [0u8; 1];
            while !head.ends_with(b"\r\n\r\n") {
                stream.read_exact(&mut byte).unwrap();
                head.push(byte[0]);
            }
            assert!(String::from_utf8_lossy(&head).contains("Upgrade: websocket"));
            stream.write_all(b"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n\r\n").unwrap();
            let asked = read_sent(&mut stream);
            assert_eq!(asked["method"], "Runtime.evaluate");
            // An event first, a ping between, then the answer in two parts.
            stream.write_all(&frame(r#"{"method":"Page.loadEventFired","params":{}}"#, true)).unwrap();
            stream.write_all(&[0x89, 0]).unwrap();
            let answer = json!({"id": asked["id"], "result": {"result": {"value": "x".repeat(200)}}}).to_string();
            let (one, two) = answer.split_at(50);
            stream.write_all(&frame(one, false)).unwrap();
            let mut rest = frame(two, true);
            rest[0] = 0x80;
            stream.write_all(&rest).unwrap();
            let failing = read_sent(&mut stream);
            stream.write_all(&frame(&json!({"id": failing["id"], "error": {"message": "no such thing"}}).to_string(), true)).unwrap();
            stream.write_all(&[0x88, 0]).unwrap();
        });
        let conn = Connection::open(&format!("ws://127.0.0.1:{port}/devtools/page/1")).unwrap();
        assert_eq!(conn.evaluate("1", Duration::from_secs(5)).unwrap(), json!("x".repeat(200)));
        assert!(conn.call("Nothing.here", json!({}), Duration::from_secs(5)).unwrap_err().contains("no such thing"));
        let events = conn.events.lock().unwrap();
        assert_eq!(events.recv_timeout(Duration::from_secs(5)).unwrap()["method"], "Page.loadEventFired");
        assert_eq!(events.recv_timeout(Duration::from_secs(5)).unwrap()["method"], "harness.closed");
        page.join().unwrap();
    }

    #[test]
    fn nothing_listening_lists_no_targets() {
        let port = TcpListener::bind(("127.0.0.1", 0)).unwrap().local_addr().unwrap().port();
        assert!(targets(port).is_empty());
        assert!(window_page(port).is_none());
    }
}

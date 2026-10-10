// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! A client of a debug adapter, such as lldb-dap, gdb's or debugpy's, over its input and output:
//! each message a Content-Length header and a JSON body, as the Debug Adapter Protocol frames them,
//! the same framing a language server uses. A request is answered by a response that names its
//! sequence number; an event goes to the function the adapter was started with. A request from the
//! adapter, such as one to run the program in a terminal, is refused, and the adapter then runs it
//! itself.

use std::collections::HashMap;
use std::io::{BufRead, BufReader};
use std::path::Path;
use std::process::{Child, ChildStdin, Command, Stdio};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::mpsc::{self, Receiver, Sender};
use std::sync::{Arc, Mutex};
use std::time::Duration;

use serde_json::{json, Value};

use crate::lsp;

/// An event from the adapter: its name and its body.
pub type Heard = Box<dyn Fn(&str, &Value) + Send + Sync>;

type Waiting = Arc<Mutex<HashMap<u64, Sender<Result<Value, String>>>>>;

/// How much of what the adapter writes to its errors is kept, in bytes.
const COMPLAINT: usize = 4096;

pub struct Adapter {
    child: Mutex<Child>,
    input: Arc<Mutex<ChildStdin>>,
    next: AtomicU64,
    waiting: Waiting,
    complaint: Arc<Mutex<String>>,
}

impl Adapter {
    /// Starts `program` with `args` in `folder`. `heard` takes the adapter's events.
    pub fn start(program: &Path, args: &[String], folder: &Path, path_env: std::ffi::OsString, heard: Heard) -> Result<Adapter, String> {
        let mut command = Command::new(program);
        command.args(args).current_dir(folder).env("PATH", path_env).stdin(Stdio::piped()).stdout(Stdio::piped()).stderr(Stdio::piped());
        crate::runner::quiet(&mut command);
        let mut child = command.spawn().map_err(|error| format!("{}: {error}", program.display()))?;
        let input = Arc::new(Mutex::new(child.stdin.take().ok_or("the adapter took no input")?));
        let output = child.stdout.take().ok_or("the adapter gave no output")?;
        let complaint = Arc::new(Mutex::new(String::new()));
        if let Some(errors) = child.stderr.take() {
            let kept = complaint.clone();
            std::thread::spawn(move || {
                for line in BufReader::new(errors).lines().map_while(Result::ok) {
                    if let Ok(mut kept) = kept.lock() {
                        kept.push_str(line.trim());
                        kept.push('\n');
                        if kept.len() > COMPLAINT {
                            let cut = kept.len() - COMPLAINT;
                            let cut = (cut..kept.len()).find(|&at| kept.is_char_boundary(at)).unwrap_or(kept.len());
                            kept.drain(..cut);
                        }
                    }
                }
            });
        }
        let waiting: Waiting = Arc::new(Mutex::new(HashMap::new()));
        let (reply_to, answers) = (input.clone(), waiting.clone());
        std::thread::spawn(move || {
            let mut reader = BufReader::new(output);
            let mut seq = 1_000_000u64;
            while let Some(message) = lsp::read_message(&mut reader) {
                match message["type"].as_str() {
                    Some("event") => heard(message["event"].as_str().unwrap_or_default(), &message["body"]),
                    Some("response") => {
                        let Some(id) = message["request_seq"].as_u64() else {
                            continue;
                        };
                        let answer = if message["success"].as_bool() == Some(true) {
                            Ok(message["body"].clone())
                        } else {
                            Err(message["body"]["error"]["format"].as_str().or_else(|| message["message"].as_str()).unwrap_or("the adapter refused").to_string())
                        };
                        if let Some(sender) = answers.lock().ok().and_then(|mut all| all.remove(&id)) {
                            let _ = sender.send(answer);
                        }
                    }
                    Some("request") => {
                        seq += 1;
                        let refused = json!({"seq": seq, "type": "response", "request_seq": message["seq"], "success": false, "command": message["command"], "message": "orior runs the program through the adapter"});
                        let _ = lsp::send(&reply_to, &refused);
                    }
                    _ => {}
                }
            }
            if let Ok(mut all) = answers.lock() {
                for (_, sender) in all.drain() {
                    let _ = sender.send(Err("the adapter stopped".into()));
                }
            }
            heard("adapterStopped", &Value::Null);
        });
        Ok(Adapter { child: Mutex::new(child), input, next: AtomicU64::new(1), waiting, complaint })
    }

    /// Sends a request and gives where its answer will come. A request with no arguments sends an
    /// empty object, which every adapter takes.
    pub fn send(&self, command: &str, arguments: Value) -> Result<Receiver<Result<Value, String>>, String> {
        let arguments = if arguments.is_null() { json!({}) } else { arguments };
        let seq = self.next.fetch_add(1, Ordering::SeqCst);
        let (sender, answer) = mpsc::channel();
        self.waiting.lock().map_err(|_| "held".to_string())?.insert(seq, sender);
        lsp::send(&self.input, &json!({"seq": seq, "type": "request", "command": command, "arguments": arguments}))?;
        Ok(answer)
    }

    /// Sends a request and waits up to `patience` for its answer.
    pub fn request(&self, command: &str, arguments: Value, patience: Duration) -> Result<Value, String> {
        let answer = self.send(command, arguments)?;
        Self::wait(command, &answer, patience)
    }

    pub fn wait(command: &str, answer: &Receiver<Result<Value, String>>, patience: Duration) -> Result<Value, String> {
        answer.recv_timeout(patience).map_err(|_| format!("{command} had no answer in {} s", patience.as_secs()))?
    }

    /// The last line the adapter wrote to its errors, which says why where it stops.
    pub fn complaint(&self) -> String {
        self.complaint.lock().map(|text| text.trim().lines().last().unwrap_or_default().to_string()).unwrap_or_default()
    }

    /// Asks the adapter to end the program and itself, and ends it where it does not.
    pub fn stop(&self) {
        let _ = self.request("disconnect", json!({"terminateDebuggee": true}), Duration::from_secs(3));
        if let Ok(mut child) = self.child.lock() {
            std::thread::sleep(Duration::from_millis(100));
            let _ = child.kill();
            let _ = child.wait();
        }
    }
}

impl Drop for Adapter {
    fn drop(&mut self) {
        if let Ok(mut child) = self.child.lock() {
            let _ = child.kill();
            let _ = child.wait();
        }
    }
}

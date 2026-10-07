// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The window's terminals: a shell each in a pseudo-terminal of its own, its output carried to the
//! window as it comes and the keys typed carried back.
//!
//! The output reaches the window as `term-out` events, text cut only between whole characters, and
//! the end of the shell as one `term-exit` with its exit code. A terminal is gone once its shell
//! ends, and closing one ends its shell.

use std::collections::HashMap;
use std::io::{Read, Write};
use std::path::Path;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;

use portable_pty::{native_pty_system, ChildKiller, CommandBuilder, MasterPty, PtySize};
use serde::Serialize;
use tauri::{AppHandle, Emitter};

struct Term {
    master: Box<dyn MasterPty + Send>,
    writer: Box<dyn Write + Send>,
    killer: Box<dyn ChildKiller + Send + Sync>,
}

/// The terminals open, by number, shared with the threads that read and wait on them.
#[derive(Default)]
pub struct Terms {
    next: AtomicU64,
    open: Arc<Mutex<HashMap<u64, Term>>>,
}

#[derive(Clone, Serialize)]
struct Out {
    id: u64,
    text: String,
}

#[derive(Clone, Serialize)]
struct Exit {
    id: u64,
    code: Option<u32>,
}

/// The shell a terminal runs, at `root`: ORIOR_SHELL where it is set; on Windows Git's bash, the one
/// the jobs run in, as a login shell; elsewhere the reader's own SHELL, else bash. Git's bash is told
/// to stay in the folder it starts in, which a login shell otherwise leaves for the home folder.
fn shell(root: &Path) -> Result<CommandBuilder, String> {
    let mut command = if let Ok(named) = std::env::var("ORIOR_SHELL") {
        CommandBuilder::new(named)
    } else if cfg!(windows) {
        let mut bash = CommandBuilder::new(crate::runner::bash()?);
        bash.args(["--login", "-i"]);
        bash.env("CHERE_INVOKING", "1");
        bash
    } else {
        let mut own = CommandBuilder::new(std::env::var("SHELL").unwrap_or_else(|_| "bash".into()));
        own.arg("-l");
        own
    };
    command.cwd(root);
    command.env("TERM", "xterm-256color");
    command.env("COLORTERM", "truecolor");
    Ok(command)
}

/// How many bytes at the start of `bytes` are whole characters. A character cut by the end of a read
/// waits for the rest of it in the next.
fn whole(bytes: &[u8]) -> usize {
    let end = bytes.len();
    for back in 1..=end.min(3) {
        let byte = bytes[end - back];
        if byte & 0xC0 == 0x80 {
            continue;
        }
        let needs = match byte {
            0xF0.. => 4,
            0xE0.. => 3,
            0xC0.. => 2,
            _ => 1,
        };
        return if needs > back { end - back } else { end };
    }
    end
}

fn size(cols: u16, rows: u16) -> PtySize {
    PtySize { rows: rows.max(2), cols: cols.max(2), pixel_width: 0, pixel_height: 0 }
}

impl Terms {
    /// Opens a terminal `cols` wide and `rows` tall with its shell at `root`, and returns its number.
    pub fn open(&self, app: AppHandle, root: &Path, cols: u16, rows: u16) -> Result<u64, String> {
        let pair = native_pty_system().openpty(size(cols, rows)).map_err(|e| e.to_string())?;
        let mut child = pair.slave.spawn_command(shell(root)?).map_err(|e| e.to_string())?;
        drop(pair.slave);
        let mut reader = pair.master.try_clone_reader().map_err(|e| e.to_string())?;
        let writer = pair.master.take_writer().map_err(|e| e.to_string())?;
        let killer = child.clone_killer();
        let id = self.next.fetch_add(1, Ordering::SeqCst) + 1;
        self.open.lock().map_err(|e| e.to_string())?.insert(id, Term { master: pair.master, writer, killer });

        let out = app.clone();
        thread::spawn(move || {
            let mut buffer = [0u8; 16384];
            let mut held: Vec<u8> = Vec::new();
            loop {
                match reader.read(&mut buffer) {
                    Ok(0) | Err(_) => break,
                    Ok(read) => {
                        held.extend_from_slice(&buffer[..read]);
                        let ready = whole(&held);
                        let text = String::from_utf8_lossy(&held[..ready]).into_owned();
                        held.drain(..ready);
                        if !text.is_empty() {
                            let _ = out.emit("term-out", Out { id, text });
                        }
                    }
                }
            }
        });

        // The shell's end closes its terminal. On Windows the reader only sees the end of its output
        // once the terminal is closed, which dropping it here does.
        let open = self.open.clone();
        thread::spawn(move || {
            let code = child.wait().ok().map(|status| status.exit_code());
            if let Ok(mut open) = open.lock() {
                open.remove(&id);
            }
            let _ = app.emit("term-exit", Exit { id, code });
        });
        Ok(id)
    }

    /// Writes what was typed to a terminal's shell.
    pub fn write(&self, id: u64, text: &str) -> Result<(), String> {
        let mut open = self.open.lock().map_err(|e| e.to_string())?;
        let term = open.get_mut(&id).ok_or_else(|| format!("no terminal {id}"))?;
        term.writer.write_all(text.as_bytes()).and_then(|_| term.writer.flush()).map_err(|e| e.to_string())
    }

    /// Gives a terminal a new size, which its shell and the programs in it are told of.
    pub fn resize(&self, id: u64, cols: u16, rows: u16) -> Result<(), String> {
        let open = self.open.lock().map_err(|e| e.to_string())?;
        let term = open.get(&id).ok_or_else(|| format!("no terminal {id}"))?;
        term.master.resize(size(cols, rows)).map_err(|e| e.to_string())
    }

    /// Ends a terminal's shell, which closes the terminal.
    pub fn close(&self, id: u64) -> Result<(), String> {
        let mut open = self.open.lock().map_err(|e| e.to_string())?;
        match open.get_mut(&id) {
            Some(term) => term.killer.kill().map_err(|e| e.to_string()),
            None => Ok(()),
        }
    }
}

#[cfg(test)]
mod cutting {
    use super::whole;

    #[test]
    fn a_read_is_cut_between_whole_characters() {
        let text = "a\u{e9}\u{20ac}\u{1f525}".as_bytes();
        assert_eq!(whole(text), text.len());
        for cut in 1..text.len() {
            let ready = whole(&text[..cut]);
            assert!(std::str::from_utf8(&text[..ready]).is_ok(), "cut at {cut} leaves {ready}");
            assert!(cut - ready < 4);
        }
    }
}

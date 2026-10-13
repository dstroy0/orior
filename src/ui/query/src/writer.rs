// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The writer's words as functions over its process (`qry_writer.c`): the run's writer started,
//! waited for, stopped and asked, and every blob handed to it or read from it. Nothing here opens
//! the buffer: each word is one run of `qry_writer`, given `QRY`, `QRY_TOP` and the PATH.

use std::io::{BufRead, BufReader, Read, Write};
use std::path::{Path, PathBuf};
use std::process::{Child, Command, Stdio};
use std::sync::{Arc, Mutex};

/// A blob's priority as the writer takes it: 0 a panic, 1 critical, 2 ordinary.
#[derive(Clone, Copy)]
pub enum Priority {
    Panic,
    Critical,
    Ordinary,
}

/// The writer of one run.
pub struct Writer {
    pub binary: PathBuf,
    pub qry: PathBuf,
    /// The top of the tree, `QRY_TOP`, every current file named under it.
    pub top: PathBuf,
    /// The PATH every program of the run runs with.
    pub path: std::ffi::OsString,
}

/// The output's bytes where the word succeeded, else `what` with what the writer said.
fn whole(output: std::process::Output, what: &str) -> Result<Vec<u8>, String> {
    if output.status.success() {
        Ok(output.stdout)
    } else {
        Err(format!("{what}: {}", String::from_utf8_lossy(&output.stderr).trim()))
    }
}

impl Writer {
    /// A command of `program` with the run's environment: `QRY`, `QRY_TOP` and the PATH.
    pub fn command_of(&self, program: &Path) -> Command {
        let mut command = Command::new(program);
        command.env("QRY", &self.qry).env("QRY_TOP", &self.top).env("PATH", &self.path);
        command
    }

    fn word(&self, words: &[&str]) -> Command {
        let mut command = self.command_of(&self.binary);
        command.args(words);
        command
    }

    fn qry_text(&self) -> String {
        self.qry.to_string_lossy().into_owned()
    }

    /// `qry_writer start <qry>` spawned in the background; its process, waited for after `stop`. It
    /// holds no pipe of this process: what it says of itself is its account blob, and `ready` says
    /// whether it started.
    pub fn start(&self) -> Result<Child, String> {
        self.word(&["start", &self.qry_text()]).stdin(Stdio::null()).stdout(Stdio::null()).stderr(Stdio::null()).spawn().map_err(|e| format!("the run's writer did not start: {e}"))
    }

    /// 1 once the writer has made the buffer, waiting up to the writer's own 30 seconds.
    pub fn ready(&self) -> bool {
        self.word(&["ready", &self.qry_text()]).stdin(Stdio::null()).stdout(Stdio::null()).stderr(Stdio::null()).status().is_ok_and(|s| s.success())
    }

    /// 1 where the run's writer runs now.
    pub fn runs(&self) -> bool {
        self.word(&["runs", &self.qry_text()]).stdin(Stdio::null()).stdout(Stdio::null()).stderr(Stdio::null()).status().is_ok_and(|s| s.success())
    }

    /// The writer asked to write the rest and stop, and waited for.
    pub fn stop(&self) -> Result<(), String> {
        let output = self.word(&["stop", &self.qry_text()]).stdin(Stdio::null()).output().map_err(|e| e.to_string())?;
        whole(output, "the run's writer did not stop").map(|_| ())
    }

    /// `bytes` given on the word's standard input, and what came of it.
    fn given(&self, words: &[&str], bytes: &[u8], what: &str) -> Result<Vec<u8>, String> {
        let mut child = self.word(words).stdin(Stdio::piped()).stdout(Stdio::piped()).stderr(Stdio::piped()).spawn().map_err(|e| e.to_string())?;
        let mut stdin = child.stdin.take().ok_or("the writer took no input")?;
        stdin.write_all(bytes).map_err(|e| e.to_string())?;
        drop(stdin);
        let output = child.wait_with_output().map_err(|e| e.to_string())?;
        whole(output, what)
    }

    /// Every byte of `bytes` handed as one blob of `name`.
    pub fn hand(&self, name: &str, bytes: &[u8], priority: Priority) -> Result<(), String> {
        let priority = match priority {
            Priority::Panic => "0",
            Priority::Critical => "1",
            Priority::Ordinary => "2",
        };
        self.given(&["hand", name, priority], bytes, &format!("{name} was not handed to the run")).map(|_| ())
    }

    fn blob(&self, word: &str, name: &str) -> Option<Vec<u8>> {
        let output = self.word(&[word, name]).stdin(Stdio::null()).stderr(Stdio::null()).output().ok()?;
        if output.status.success() { Some(output.stdout) } else { None }
    }

    /// The latest blob of `name`, from the buffer while the writer runs and from the `.qry` after.
    pub fn latest(&self, name: &str) -> Option<Vec<u8>> {
        self.blob("latest", name)
    }

    /// Every blob of `name` joined.
    pub fn read(&self, name: &str) -> Option<Vec<u8>> {
        self.blob("read", name)
    }

    /// The first blob of `name`.
    pub fn first(&self, name: &str) -> Option<Vec<u8>> {
        self.blob("first", name)
    }

    /// The file at `path` as the run holds it: its latest blob, or the file where the run holds none.
    pub fn current(&self, path: &Path) -> Option<Vec<u8>> {
        self.blob("current", &path.to_string_lossy())
    }

    /// `bytes` handed as the current file at `path`, which the writer writes in its place as the run ends.
    pub fn file(&self, path: &Path, bytes: &[u8]) -> Result<(), String> {
        self.given(&["file", &path.to_string_lossy()], bytes, &format!("{} was not written through the run's writer", path.display())).map(|_| ())
    }

    /// The file at `path` handed as the run leaves it.
    pub fn left(&self, path: &Path) -> Result<(), String> {
        let output = self.word(&["left", &path.to_string_lossy()]).stdin(Stdio::null()).output().map_err(|e| e.to_string())?;
        whole(output, &format!("{} was not handed as left", path.display())).map(|_| ())
    }

    /// The latest blob of each of `names` in the finished `.qry` at `from` handed to this run; with
    /// `names` empty, every name `from` holds. What the writer said of it.
    pub fn carry(&self, from: &Path, names: &[String]) -> Result<String, String> {
        let from = from.to_string_lossy().into_owned();
        let mut words = vec!["carry", from.as_str()];
        words.extend(names.iter().map(String::as_str));
        let output = self.word(&words).stdin(Stdio::null()).output().map_err(|e| e.to_string())?;
        whole(output, &format!("the blobs of {from} were not carried")).map(|said| String::from_utf8_lossy(&said).trim().to_string())
    }

    /// The names listed on `qry_writer carry <from> -`'s standard input carried, one a line. What
    /// the writer said of it.
    pub fn carry_listed(&self, from: &Path, listed: &str) -> Result<String, String> {
        let from = from.to_string_lossy().into_owned();
        self.given(&["carry", &from, "-"], listed.as_bytes(), &format!("the blobs of {from} were not carried")).map(|said| String::from_utf8_lossy(&said).trim().to_string())
    }

    /// `child`'s output, its standard output and error alike, passed to `sink` a line at a time and
    /// handed to the run as `<name>.console` through `qry_writer tee`. The child's exit code, or
    /// -1 where it was ended by a signal.
    pub fn tee(&self, name: &str, mut child: Child, sink: &mut dyn FnMut(&str)) -> Result<i32, String> {
        let mut tee = self.word(&["tee", name]).stdin(Stdio::piped()).stdout(Stdio::piped()).stderr(Stdio::inherit()).spawn().map_err(|e| format!("qry_writer tee: {e}"))?;
        let into = Arc::new(Mutex::new(tee.stdin.take().ok_or("qry_writer tee took no input")?));
        let out = tee.stdout.take().ok_or("qry_writer tee gave no output")?;
        let readers: Vec<Box<dyn Read + Send>> = [child.stdout.take().map(|s| Box::new(s) as Box<dyn Read + Send>), child.stderr.take().map(|s| Box::new(s) as Box<dyn Read + Send>)].into_iter().flatten().collect();
        let status = std::thread::scope(|scope| {
            let mut copiers = Vec::new();
            for reader in readers {
                let into = into.clone();
                copiers.push(scope.spawn(move || {
                    let mut lines = BufReader::new(reader);
                    let mut line = Vec::new();
                    while lines.read_until(b'\n', &mut line).is_ok_and(|n| n > 0) {
                        if let Ok(mut w) = into.lock() {
                            let _ = w.write_all(&line);
                            let _ = w.flush();
                        }
                        line.clear();
                    }
                }));
            }
            let waited = scope.spawn(move || {
                for copier in copiers {
                    let _ = copier.join();
                }
                drop(into);
                child.wait()
            });
            for line in BufReader::new(out).lines().map_while(Result::ok) {
                sink(line.trim_end_matches(['\r', '\n']));
            }
            waited.join().map_err(|_| "the output's copiers ended wrong".to_string())
        })?;
        let _ = tee.wait();
        Ok(status.map_err(|e| e.to_string())?.code().unwrap_or(-1))
    }

    /// Each who code named through the who table, a line each.
    pub fn who(&self, codes: &[&str]) -> Result<String, String> {
        let mut words = vec!["who"];
        words.extend(codes);
        let output = self.word(&words).stdin(Stdio::null()).output().map_err(|e| e.to_string())?;
        whole(output, "no who code is named").map(|said| String::from_utf8_lossy(&said).into_owned())
    }
}

#[cfg(test)]
mod words {
    use super::*;
    use crate::programs::{OnPath, Programs, Role};
    use crate::tools;

    fn writer_in(name: &str) -> Option<(PathBuf, Writer)> {
        if OnPath.program(Role::C).is_err() {
            return None;
        }
        let root = crate::tree_root();
        let out = root.join("build").join("query_test").join(name);
        let _ = std::fs::remove_dir_all(&out);
        std::fs::create_dir_all(&out).unwrap();
        let binary = tools::built_into(&root, &OnPath, "qry_writer", &out).unwrap();
        Some((out.clone(), Writer { binary, qry: out.join("query1-test.qry"), top: root, path: OnPath.run_path() }))
    }

    #[test]
    fn a_run_is_started_handed_read_and_stopped() {
        let Some((out, writer)) = writer_in("writer") else { return };
        assert!(!writer.runs());
        let mut process = writer.start().unwrap();
        assert!(writer.ready(), "the writer came ready");
        assert!(writer.runs());
        writer.hand("a.txt", b"one\ntwo\n", Priority::Ordinary).unwrap();
        assert_eq!(writer.latest("a.txt").unwrap(), b"one\ntwo\n");
        assert!(writer.latest("none.txt").is_none());
        writer.stop().unwrap();
        process.wait().unwrap();
        assert!(!writer.runs());
        assert_eq!(writer.latest("a.txt").unwrap(), b"one\ntwo\n", "read from the .qry once the writer stopped");
        let _ = std::fs::remove_dir_all(&out);
    }

    #[test]
    fn a_program_s_output_is_teed_into_the_run_and_the_sink() {
        let Some((out, writer)) = writer_in("tee") else { return };
        let Ok(python) = OnPath.program(Role::Python) else { return };
        let mut process = writer.start().unwrap();
        assert!(writer.ready());
        let child = writer
            .command_of(&python)
            .args(["-c", "import sys; print('out'); print('err', file=sys.stderr); sys.exit(3)"])
            .stdout(Stdio::piped())
            .stderr(Stdio::piped())
            .spawn()
            .unwrap();
        let mut lines = Vec::new();
        let code = writer.tee("said", child, &mut |line| lines.push(line.to_string())).unwrap();
        assert_eq!(code, 3);
        assert!(lines.contains(&"out".to_string()) && lines.contains(&"err".to_string()), "{lines:?}");
        // the tee hands a piece at a time, a blob each: every blob joined is the console
        let console = String::from_utf8(writer.read("said.console").unwrap()).unwrap();
        assert!(console.contains("out") && console.contains("err"), "{console}");
        writer.stop().unwrap();
        process.wait().unwrap();
        let _ = std::fs::remove_dir_all(&out);
    }
}

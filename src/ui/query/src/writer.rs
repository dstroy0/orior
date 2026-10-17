// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The run's writer (`qry_writer.c`) and its buffer, from this process. The writer's process is
//! started, waited for and stopped by its words, each one run of `qry_writer` given `QRY`,
//! `QRY_TOP` and the PATH. Every blob handed or read goes through the buffer this process is a
//! member of (`member.rs`): a hand is one write into this process's ring, a read is the blob where
//! it stands. A program's output goes to the run through `qry_writer tee`, the program's output
//! piped straight into it, the tee's standard output this process's own.

use std::io::Write;
use std::path::{Path, PathBuf};
use std::process::{Child, Command, Stdio};
use std::sync::Arc;

use crate::member::{Bytes, Freed, Member};

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
    /// The buffer, open while the writer runs and this process is a member of the run.
    pub member: Option<Arc<Member>>,
}

/// The variable a step of a job the window's runner starts finds its run's number in (the CLI's runner.rs).
pub const RUN_VARIABLE: &str = "ORIOR_RUN";

/// Where the writer says each blob it places: the job's own output where a runner started this process, which
/// carries the writer's lines to the window as it says them, and nowhere where none did.
fn said_to() -> Stdio {
    if std::env::var_os(RUN_VARIABLE).is_some() { Stdio::inherit() } else { Stdio::null() }
}

/// The output's bytes where the word succeeded, else `what` with what the writer said.
fn whole(output: std::process::Output, what: &str) -> Result<Freed, String> {
    if output.status.success() {
        Freed::of(&output.stdout).ok_or_else(|| format!("{what}: no memory for what the writer said"))
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
        self.word(&["start", &self.qry_text()]).stdin(Stdio::null()).stdout(said_to()).stderr(Stdio::null()).spawn().map_err(|e| format!("the run's writer did not start: {e}"))
    }

    /// 1 once the writer has made the buffer, waiting up to the writer's own 30 seconds.
    pub fn ready(&self) -> bool {
        self.made().is_some()
    }

    /// The writer's line naming the run's database, `  qry_writer: the buffer of <qry> is made`, once
    /// the writer has made the buffer, waiting up to the writer's own 30 seconds; None where it did not
    /// start. The line is said on the console of whatever started the run, for a reader of the run to
    /// find it by.
    pub fn made(&self) -> Option<String> {
        let said = self.word(&["ready", &self.qry_text()]).stdin(Stdio::null()).stdout(Stdio::piped()).stderr(Stdio::null()).output().ok()?;
        if !said.status.success() {
            return None;
        }
        let text = String::from_utf8_lossy(&said.stdout);
        Some(text.lines().find(|line| line.contains("the buffer of")).unwrap_or("").trim_end().to_string())
    }

    /// 1 where the run's writer runs now.
    pub fn runs(&self) -> bool {
        self.word(&["runs", &self.qry_text()]).stdin(Stdio::null()).stdout(Stdio::null()).stderr(Stdio::null()).status().is_ok_and(|s| s.success())
    }

    /// The buffer opened, this process a member of the run: 1, or 0 where the writer does not run.
    pub fn joined(&mut self) -> bool {
        self.member = Member::opened(&self.qry).map(Arc::new);
        self.member.is_some()
    }

    /// The buffer let go: this process is a member of the run no more.
    pub fn parted(&mut self) {
        self.member = None;
    }

    /// The writer asked to write the rest and stop, and waited for.
    pub fn stop(&self) -> Result<(), String> {
        let output = self.word(&["stop", &self.qry_text()]).stdin(Stdio::null()).output().map_err(|e| e.to_string())?;
        whole(output, "the run's writer did not stop").map(|_| ())
    }

    /// `bytes` given on the word's standard input, and what came of it.
    fn given(&self, words: &[&str], bytes: &[u8], what: &str) -> Result<Freed, String> {
        let mut child = self.word(words).stdin(Stdio::piped()).stdout(Stdio::piped()).stderr(Stdio::piped()).spawn().map_err(|e| e.to_string())?;
        let mut stdin = child.stdin.take().ok_or("the writer took no input")?;
        stdin.write_all(bytes).map_err(|e| e.to_string())?;
        drop(stdin);
        let output = child.wait_with_output().map_err(|e| e.to_string())?;
        whole(output, what)
    }

    /// Every byte of `bytes` handed as one blob of `name`, into this process's ring, and moved into
    /// the run's buffer before this returns, so that what is handed is there for the next read and
    /// the next program.
    pub fn hand(&self, name: &str, bytes: &[u8], priority: Priority) -> Result<(), String> {
        let member = self.member.as_ref().ok_or_else(|| format!("{name} was not handed: this process is a member of no run"))?;
        if !member.hand(name, bytes, priority) {
            return Err(format!("{name} was not handed to the run"));
        }
        member.moved();
        Ok(())
    }

    /// The latest blob of `name`: in the buffer while this process is a member, from the `.qry` after.
    pub fn latest(&self, name: &str) -> Option<Bytes<'_>> {
        match &self.member {
            Some(member) => member.latest(name).map(Bytes::Held),
            None => Member::file_latest(&self.qry, name).map(Bytes::Freed),
        }
    }

    /// Every blob of `name` joined.
    pub fn read(&self, name: &str) -> Option<Freed> {
        match &self.member {
            Some(member) => member.joined(name),
            None => Member::file_joined(&self.qry, name),
        }
    }

    /// The file at `path` as the run holds it: its latest blob, or the file where the run holds none.
    pub fn current(&self, path: &Path) -> Option<Freed> {
        self.member.as_ref()?.file_read(path)
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
        let mut command = self.word(&["carry", from.as_str()]);
        command.args(names).stdin(Stdio::null());
        let output = command.output().map_err(|e| e.to_string())?;
        whole(output, &format!("the blobs of {from} were not carried")).map(|said| String::from_utf8_lossy(&said).trim().to_string())
    }

    /// The names listed on `qry_writer carry <from> -`'s standard input carried, one a line. What
    /// the writer said of it.
    pub fn carry_listed(&self, from: &Path, listed: &str) -> Result<String, String> {
        let from = from.to_string_lossy().into_owned();
        self.given(&["carry", &from, "-"], listed.as_bytes(), &format!("the blobs of {from} were not carried")).map(|said| String::from_utf8_lossy(&said).trim().to_string())
    }

    /// `command` run with its standard output and error piped straight into `qry_writer tee <name>`,
    /// which writes each piece to this process's standard output and hands it to the run as
    /// `<name>.console` as it comes. The command's exit code, or -1 where it was ended by a signal.
    pub fn tee(&self, name: &str, mut command: Command) -> Result<i32, String> {
        let mut tee = self.word(&["tee", name]).stdin(Stdio::piped()).stdout(Stdio::inherit()).stderr(Stdio::inherit()).spawn().map_err(|e| format!("qry_writer tee: {e}"))?;
        let into = tee.stdin.take().ok_or("qry_writer tee took no input")?;
        let (out, err) = both_ends(into)?;
        command.stdin(Stdio::null()).stdout(out).stderr(err);
        let mut child = command.spawn().map_err(|e| format!("{:?}: {e}", command.get_program()))?;
        // the command's handles on the tee's input close with the command, so that the tee's input ends with the
        // child's output and not with this process
        drop(command);
        let status = child.wait().map_err(|e| e.to_string())?;
        let _ = tee.wait();
        Ok(status.code().unwrap_or(-1))
    }
}

/// The tee's input as two standard streams, one for a child's output and one for its error.
#[cfg(windows)]
fn both_ends(into: std::process::ChildStdin) -> Result<(Stdio, Stdio), String> {
    let owned: std::os::windows::io::OwnedHandle = into.into();
    let second = owned.try_clone().map_err(|e| format!("the tee's input was not doubled: {e}"))?;
    Ok((Stdio::from(owned), Stdio::from(second)))
}

#[cfg(unix)]
fn both_ends(into: std::process::ChildStdin) -> Result<(Stdio, Stdio), String> {
    let owned: std::os::fd::OwnedFd = into.into();
    let second = owned.try_clone().map_err(|e| format!("the tee's input was not doubled: {e}"))?;
    Ok((Stdio::from(owned), Stdio::from(second)))
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
        Some((out.clone(), Writer { binary, qry: out.join("query1-test.qry"), top: root, path: OnPath.run_path(), member: None }))
    }

    #[test]
    fn a_run_is_started_handed_read_and_stopped() {
        let Some((out, mut writer)) = writer_in("writer") else { return };
        assert!(!writer.runs());
        let mut process = writer.start().unwrap();
        assert!(writer.ready(), "the writer came ready");
        assert!(writer.runs());
        assert!(writer.joined(), "this process is a member of the run");
        writer.hand("a.txt", b"one\ntwo\n", Priority::Ordinary).unwrap();
        writer.member.as_ref().unwrap().moved();
        assert_eq!(&*writer.latest("a.txt").unwrap(), b"one\ntwo\n");
        assert!(writer.latest("none.txt").is_none());
        writer.parted();
        writer.stop().unwrap();
        process.wait().unwrap();
        assert!(!writer.runs());
        assert_eq!(&*writer.latest("a.txt").unwrap(), b"one\ntwo\n", "read from the .qry once the writer stopped");
        assert_eq!(&*writer.read("a.txt").unwrap(), b"one\ntwo\n");
        let _ = std::fs::remove_dir_all(&out);
    }

    #[test]
    fn a_program_s_output_is_teed_into_the_run() {
        let Some((out, mut writer)) = writer_in("tee") else { return };
        let Ok(python) = OnPath.program(Role::Python) else { return };
        let mut process = writer.start().unwrap();
        assert!(writer.ready());
        assert!(writer.joined());
        let mut command = writer.command_of(&python);
        command.args(["-c", "import sys; print('out'); print('err', file=sys.stderr); sys.exit(3)"]);
        let code = writer.tee("said", command).unwrap();
        assert_eq!(code, 3);
        // the tee hands a piece at a time, a blob each: every blob joined is the console
        let console = writer.read("said.console").unwrap();
        let console = String::from_utf8_lossy(&console);
        assert!(console.contains("out") && console.contains("err"), "{console}");
        writer.parted();
        writer.stop().unwrap();
        process.wait().unwrap();
        let _ = std::fs::remove_dir_all(&out);
    }
}

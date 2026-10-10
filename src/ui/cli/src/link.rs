// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! A tree on another machine, and the window's link to the tree's side there.
//!
//! An address names the machine and the tree's folder on it: `user@host:folder` over ssh,
//! `docker:container:folder` in a running container over docker exec, and `wsl:distribution:folder`
//! in a WSL distribution. The link asks the machine what it is, sends it orior's server for it where
//! the machine does not hold the one this window was built with, and starts `orior-cli serve` there,
//! whose calls it answers by id and whose events it hands on. Where the link drops, the calls waiting
//! on it are answered with the drop, and it is made again, sooner at first and then every
//! JOIN_MOST; each change is told as the event "link".
//!
//! ssh runs with BatchMode, its keys and agent its only way in: a password has no terminal to be typed
//! at. ORIOR_SSH names another ssh, with words of its own before ssh's, and ORIOR_SERVER the server
//! to send whatever the machine is.

use std::collections::HashMap;
use std::io::{BufRead, BufReader, Read, Write};
use std::path::PathBuf;
use std::process::{Child, ChildStdin, Command, Stdio};
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::mpsc::{Sender, channel};
use std::sync::{Arc, Condvar, Mutex};
use std::time::{Duration, Instant};

use serde_json::{Value, json};

use crate::serve::Emit;

/// How long a call waits for the link to be made before it says the machine is not reached.
const JOINING: Duration = Duration::from_secs(60);

/// The longest wait between one try at making the link again and the next.
const JOIN_MOST: Duration = Duration::from_secs(30);

/// How much of what ssh, docker or wsl says on its error stream is kept, the end of it, to say why a
/// link failed.
const SAID_MOST: usize = 2000;

/// How the machine is reached.
#[derive(Clone, Debug, PartialEq)]
pub enum Way {
    Ssh(String),
    Docker(String),
    Wsl(String),
}

/// A tree on another machine: how it is reached and its folder there.
#[derive(Clone, Debug, PartialEq)]
pub struct Address {
    pub way: Way,
    pub folder: String,
}

/// `text` as one word of a POSIX shell's line.
pub fn quote(text: &str) -> String {
    format!("'{}'", text.replace('\'', r"'\''"))
}

impl Address {
    pub fn parse(text: &str) -> Result<Address, String> {
        let text = text.trim();
        let bad = || format!("{text} names no machine and folder: user@host:folder, docker:container:folder or wsl:distribution:folder");
        let (first, rest) = text.split_once(':').ok_or_else(bad)?;
        let (way, folder) = match first {
            "docker" | "wsl" => {
                let (name, folder) = rest.split_once(':').ok_or_else(bad)?;
                (if first == "docker" { Way::Docker(name.to_string()) } else { Way::Wsl(name.to_string()) }, folder)
            }
            host => (Way::Ssh(host.to_string()), rest),
        };
        let (Way::Ssh(name) | Way::Docker(name) | Way::Wsl(name)) = &way;
        if name.is_empty() || folder.is_empty() {
            return Err(bad());
        }
        Ok(Address { way, folder: folder.to_string() })
    }

    /// The address as `parse` reads it.
    pub fn text(&self) -> String {
        match &self.way {
            Way::Ssh(host) => format!("{host}:{}", self.folder),
            Way::Docker(name) => format!("docker:{name}:{}", self.folder),
            Way::Wsl(name) => format!("wsl:{name}:{}", self.folder),
        }
    }

    /// The machine, as the window names it.
    pub fn machine(&self) -> String {
        match &self.way {
            Way::Ssh(host) => host.rsplit('@').next().unwrap_or(host).to_string(),
            Way::Docker(name) => name.clone(),
            Way::Wsl(name) => name.clone(),
        }
    }

    /// The program, and its words, that run the shell line `line` on the machine, with a terminal where
    /// `terminal` and over its input and output where not.
    pub fn runs(&self, line: &str, terminal: bool) -> (String, Vec<String>) {
        match &self.way {
            Way::Ssh(host) => {
                let mut words = std::env::var("ORIOR_SSH").ok().and_then(|text| crate::runner::shell_words(&text).ok()).filter(|words| !words.is_empty()).unwrap_or_else(|| vec!["ssh".into()]);
                let program = words.remove(0);
                words.extend(if terminal { vec!["-tt"] } else { vec!["-T", "-o", "BatchMode=yes"] }.into_iter().map(String::from));
                words.extend(["-o", "ServerAliveInterval=15", "-o", "ServerAliveCountMax=3"].map(String::from));
                words.push(host.clone());
                words.push(line.to_string());
                (program, words)
            }
            Way::Docker(name) => {
                let mut words = vec!["exec".to_string(), if terminal { "-it" } else { "-i" }.to_string()];
                words.extend([name.clone(), "sh".into(), "-c".into(), line.to_string()]);
                ("docker".into(), words)
            }
            Way::Wsl(name) => ("wsl".into(), ["-d", name, "--", "sh", "-c", line].map(String::from).to_vec()),
        }
    }

    /// `line` run on the machine over its input and output, its window kept hidden on Windows.
    pub fn command(&self, line: &str) -> Command {
        let (program, words) = self.runs(line, false);
        let mut command = Command::new(program);
        command.args(words);
        crate::runner::quiet(&mut command);
        command
    }

    /// The shell line that opens the reader's shell in `folder`, or the tree's folder, on the machine.
    pub fn shell_line(&self, folder: Option<&str>) -> String {
        format!("cd {} 2>/dev/null || cd; exec \"${{SHELL:-sh}}\" -l", quote(folder.unwrap_or(&self.folder)))
    }

    /// Runs `line` on the machine and gives what it wrote, or what it said where it failed.
    fn ask(&self, line: &str, input: Option<&[u8]>) -> Result<String, String> {
        let mut command = self.command(line);
        command.stdin(if input.is_some() { Stdio::piped() } else { Stdio::null() }).stdout(Stdio::piped()).stderr(Stdio::piped());
        let mut child = command.spawn().map_err(|error| format!("{}: {error}", self.runs(line, false).0))?;
        if let (Some(bytes), Some(mut stdin)) = (input, child.stdin.take()) {
            stdin.write_all(bytes).map_err(|error| error.to_string())?;
        }
        let done = child.wait_with_output().map_err(|error| error.to_string())?;
        if !done.status.success() {
            let said = String::from_utf8_lossy(&done.stderr).trim().to_string();
            return Err(if said.is_empty() { format!("{} could not be reached", self.machine()) } else { said });
        }
        Ok(String::from_utf8_lossy(&done.stdout).into_owned())
    }

    /// Puts orior's server on the machine where it is not there, and gives the shell line that starts
    /// it on the tree. `tell` hears what is being done while it waits.
    pub fn prepare(&self, tell: &dyn Fn(&str)) -> Result<String, String> {
        let probe = self.ask("uname -sm; printf '%s\\n' \"${ORIOR_SERVER_HOME:-$HOME/.orior/server}\"", None)?;
        let mut lines = probe.lines();
        let system = lines.next().unwrap_or("").trim().to_string();
        let home = lines.next().unwrap_or("").trim().to_string();
        let local = server_for(&system, tell)?;
        let windows = is_windows(&system);
        let stamp = stamp_of(&local)?;
        let program = format!("{home}/{stamp}/orior-cli{}", if windows { ".exe" } else { "" });
        if self.ask(&format!("test -x {}", quote(&program)), None).is_err() {
            tell(&format!("Sending orior's server to {}", self.machine()));
            let bytes = std::fs::read(&local).map_err(|error| format!("{}: {error}", local.display()))?;
            let part = format!("{program}.part");
            self.ask(&format!("mkdir -p {} && cat > {} && chmod +x {} && mv -f {} {}", quote(&format!("{home}/{stamp}")), quote(&part), quote(&part), quote(&part), quote(&program)), Some(&bytes))?;
        }
        Ok(format!("exec {} serve --root {}", quote(&program), quote(&self.folder)))
    }
}

fn is_windows(system: &str) -> bool {
    ["MINGW", "MSYS", "CYGWIN", "Windows"].iter().any(|start| system.starts_with(start))
}

/// The machine's Rust target, by what `uname -sm` says of it.
fn target_of(system: &str) -> Result<String, String> {
    let mut words = system.split_whitespace();
    let (kind, arch) = (words.next().unwrap_or(""), words.next().unwrap_or(""));
    let arch = match arch {
        "x86_64" | "amd64" => "x86_64",
        "aarch64" | "arm64" => "aarch64",
        other => return Err(format!("orior builds no server for {other}")),
    };
    match kind {
        "Linux" => Ok(format!("{arch}-unknown-linux-musl")),
        other => Err(format!("orior builds no server for {other} from this machine")),
    }
}

/// orior's server for a machine `uname -sm` says is `system`: ORIOR_SERVER where it is set; on
/// Windows this program, which serves as well; elsewhere one built for the machine's target, beside
/// this program under servers/, or built from orior's own sources with Rust's target for it.
fn server_for(system: &str, tell: &dyn Fn(&str)) -> Result<PathBuf, String> {
    if let Ok(named) = std::env::var("ORIOR_SERVER") {
        return Ok(PathBuf::from(named));
    }
    if is_windows(system) {
        return std::env::current_exe().map_err(|error| error.to_string());
    }
    let target = target_of(system)?;
    let beside = std::env::current_exe().ok().and_then(|exe| exe.parent().map(|dir| dir.join("servers").join(&target).join("orior-cli")));
    if let Some(found) = beside.filter(|path| path.is_file()) {
        return Ok(found);
    }
    let sources = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let built = sources.join("target").join(&target).join("release").join("orior-cli");
    if !sources.join("Cargo.toml").is_file() {
        return Err(format!("no orior server for {target} is beside this program, under servers/{target}/orior-cli"));
    }
    tell(&format!("Building orior's server for {target}"));
    let mut cargo = Command::new("cargo");
    cargo.args(["build", "--release", "--bin", "orior-cli", "--target", &target]).current_dir(&sources);
    cargo.env(format!("CARGO_TARGET_{}_LINKER", target.to_uppercase().replace('-', "_")), "rust-lld");
    crate::runner::quiet(&mut cargo);
    let done = cargo.stdin(Stdio::null()).output().map_err(|error| format!("cargo: {error}"))?;
    if !done.status.success() || !built.is_file() {
        let said = String::from_utf8_lossy(&done.stderr);
        if said.contains("target may not be installed") || said.contains("can't find crate for `std`") {
            return Err(format!("orior's server for {target} is built with Rust's target for it, which this machine does not have: rustup target add {target}"));
        }
        return Err(said.lines().rev().take(6).collect::<Vec<_>>().into_iter().rev().collect::<Vec<_>>().join("\n"));
    }
    Ok(built)
}

/// The name a server is kept under on the machine: its size and the time it was written, so that
/// another build is sent again and the same one is not.
fn stamp_of(path: &PathBuf) -> Result<String, String> {
    let meta = std::fs::metadata(path).map_err(|error| format!("{}: {error}", path.display()))?;
    let written = meta.modified().ok().and_then(|time| time.duration_since(std::time::UNIX_EPOCH).ok()).map_or(0, |since| since.as_secs());
    Ok(format!("{}-{}-{written}", env!("CARGO_PKG_VERSION"), meta.len()))
}

/// The server running on the machine: its process here, ssh's, docker's or wsl's, and its input.
struct Joined {
    child: Child,
    input: ChildStdin,
}

/// The link: the calls waiting on an answer by id, and the server while it is joined.
pub struct Link {
    pub address: Address,
    events: Emit,
    joined: Mutex<Option<Joined>>,
    changed: Condvar,
    pending: Mutex<HashMap<u64, Sender<Result<Value, String>>>>,
    next: AtomicU64,
    ended: AtomicBool,
    /// The line that starts the server on the machine, once it is there.
    start: Mutex<Option<String>>,
    /// How the link stands, as it was last told.
    standing: Mutex<Value>,
}

impl Link {
    /// A link to the tree at `address`, made on a thread of its own and made again as it drops, with
    /// the server's events and the link's own handed to `events`.
    pub fn open(address: Address, events: Emit) -> Arc<Link> {
        let link = Arc::new(Link { address, events, joined: Mutex::new(None), changed: Condvar::new(), pending: Mutex::new(HashMap::new()), next: AtomicU64::new(1), ended: AtomicBool::new(false), start: Mutex::new(None), standing: Mutex::new(Value::Null) });
        let keeping = link.clone();
        std::thread::spawn(move || keeping.keep_joined());
        link
    }

    fn tell(&self, state: &str, said: &str) {
        let told = json!({"state": state, "machine": self.address.machine(), "address": self.address.text(), "said": said});
        if let Ok(mut standing) = self.standing.lock() {
            *standing = told.clone();
        }
        (self.events)("link", told);
    }

    /// The machine, the address and how the link stands, as it was last told.
    pub fn standing(&self) -> Value {
        let standing = self.standing.lock().map(|standing| standing.clone()).unwrap_or(Value::Null);
        if standing.is_null() { json!({"state": "joining", "machine": self.address.machine(), "address": self.address.text(), "said": ""}) } else { standing }
    }

    /// Makes the link, and makes it again each time it drops, until it is ended.
    fn keep_joined(self: Arc<Self>) {
        let mut wait = Duration::from_secs(1);
        let mut once = false;
        while !self.ended.load(Ordering::SeqCst) {
            self.tell(if once { "joining-again" } else { "joining" }, "");
            match self.join() {
                Ok((reader, said)) => {
                    once = true;
                    wait = Duration::from_secs(1);
                    self.tell("joined", "");
                    self.read(reader);
                    let why = said.lock().map(|said| said.trim().to_string()).unwrap_or_default();
                    self.dropped(&why);
                    if self.ended.load(Ordering::SeqCst) {
                        return;
                    }
                    self.tell("dropped", &why);
                }
                Err(why) => {
                    self.tell("failed", &why);
                    std::thread::sleep(wait);
                    wait = (wait * 2).min(JOIN_MOST);
                }
            }
        }
    }

    /// Starts the server on the machine and waits for it to say it serves. Gives its output, and what
    /// the transport says on its error stream as it goes.
    fn join(&self) -> Result<(BufReader<std::process::ChildStdout>, Arc<Mutex<String>>), String> {
        let known = self.start.lock().map_err(|e| e.to_string())?.clone();
        let start = match known {
            Some(line) => line,
            None => {
                let line = self.address.prepare(&|said| self.tell("preparing", said))?;
                *self.start.lock().map_err(|e| e.to_string())? = Some(line.clone());
                line
            }
        };
        let mut command = self.address.command(&start);
        command.stdin(Stdio::piped()).stdout(Stdio::piped()).stderr(Stdio::piped());
        let mut child = command.spawn().map_err(|error| error.to_string())?;
        let input = child.stdin.take().ok_or("no input")?;
        let output = child.stdout.take().ok_or("no output")?;
        let said = Arc::new(Mutex::new(String::new()));
        if let Some(mut errors) = child.stderr.take() {
            let said = said.clone();
            std::thread::spawn(move || {
                let mut buffer = [0u8; 4096];
                while let Ok(read) = errors.read(&mut buffer) {
                    if read == 0 {
                        break;
                    }
                    if let Ok(mut said) = said.lock() {
                        said.push_str(&String::from_utf8_lossy(&buffer[..read]));
                        if said.len() > SAID_MOST {
                            let cut = said.len() - SAID_MOST;
                            let cut = (cut..said.len()).find(|at| said.is_char_boundary(*at)).unwrap_or(said.len());
                            said.drain(..cut);
                        }
                    }
                }
            });
        }
        let mut reader = BufReader::new(output);
        let mut first = String::new();
        let serving = reader.read_line(&mut first).is_ok_and(|read| read > 0) && serde_json::from_str::<Value>(&first).is_ok_and(|line| line["event"] == "serving");
        if !serving {
            let _ = child.kill();
            let _ = child.wait();
            std::thread::sleep(Duration::from_millis(100));
            let why = said.lock().map(|said| said.trim().to_string()).unwrap_or_default();
            // A server that will not start is sent again on the next try.
            *self.start.lock().map_err(|e| e.to_string())? = None;
            return Err(if why.is_empty() { format!("orior's server on {} did not start", self.address.machine()) } else { why });
        }
        *self.joined.lock().map_err(|e| e.to_string())? = Some(Joined { child, input });
        self.changed.notify_all();
        Ok((reader, said))
    }

    /// Hands each answer to its call and each event on, until the server's output ends.
    fn read(&self, reader: BufReader<std::process::ChildStdout>) {
        for line in reader.lines().map_while(Result::ok) {
            let Ok(line) = serde_json::from_str::<Value>(&line) else { continue };
            if let Some(event) = line["event"].as_str() {
                (self.events)(event, line["body"].clone());
                continue;
            }
            let Some(id) = line["id"].as_u64() else { continue };
            let answer = match line.get("error") {
                Some(error) => Err(error.as_str().unwrap_or("the call failed").to_string()),
                None => Ok(line["ok"].clone()),
            };
            if let Some(sender) = self.pending.lock().ok().and_then(|mut pending| pending.remove(&id)) {
                let _ = sender.send(answer);
            }
        }
    }

    /// The link dropped: its server let go of and every call waiting answered with the drop.
    fn dropped(&self, why: &str) {
        if let Some(mut joined) = self.joined.lock().ok().and_then(|mut joined| joined.take()) {
            let _ = joined.child.kill();
            let _ = joined.child.wait();
        }
        let waiting: Vec<_> = self.pending.lock().map(|mut pending| pending.drain().collect()).unwrap_or_default();
        let said = if why.is_empty() { format!("the link to {} dropped", self.address.machine()) } else { format!("the link to {} dropped: {why}", self.address.machine()) };
        for (_, sender) in waiting {
            let _ = sender.send(Err(said.clone()));
        }
    }

    /// Calls `name` on the tree's side with `args`, waiting up to JOINING for the link to be made.
    pub fn call(&self, name: &str, args: Value) -> Result<Value, String> {
        let id = self.next.fetch_add(1, Ordering::SeqCst);
        let (sender, answer) = channel();
        let deadline = Instant::now() + JOINING;
        {
            let mut joined = self.joined.lock().map_err(|e| e.to_string())?;
            while joined.is_none() {
                let now = Instant::now();
                if self.ended.load(Ordering::SeqCst) || now >= deadline {
                    return Err(format!("{} is not reached", self.address.machine()));
                }
                joined = self.changed.wait_timeout(joined, deadline - now).map_err(|e| e.to_string())?.0;
            }
            self.pending.lock().map_err(|e| e.to_string())?.insert(id, sender);
            let Some(joined) = joined.as_mut() else { return Err("the link dropped".into()) };
            let line = json!({"id": id, "name": name, "args": args});
            if writeln!(joined.input, "{line}").and_then(|()| joined.input.flush()).is_err() {
                self.pending.lock().map_err(|e| e.to_string())?.remove(&id);
                return Err(format!("the link to {} dropped", self.address.machine()));
            }
        }
        answer.recv().unwrap_or_else(|_| Err(format!("the link to {} dropped", self.address.machine())))
    }

    /// Ends the link and the server on the machine with it.
    pub fn end(&self) {
        self.ended.store(true, Ordering::SeqCst);
        if let Some(mut joined) = self.joined.lock().ok().and_then(|mut joined| joined.take()) {
            drop(joined.input);
            let _ = joined.child.kill();
            let _ = joined.child.wait();
        }
        self.changed.notify_all();
    }

    /// Ends the server on the machine, as a dropped connection would, for the link to make again.
    /// Its input is closed as well as its process ended: where the transport is a shell that started
    /// the server, ending the shell leaves the server, which ends with its input.
    pub fn drop_now(&self) {
        if let Some(mut joined) = self.joined.lock().ok().and_then(|mut joined| joined.take()) {
            drop(joined.input);
            let _ = joined.child.kill();
            let _ = joined.child.wait();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn addresses_read_each_way_in() {
        let ssh = Address::parse("me@box.lan:/home/me/tree").unwrap();
        assert_eq!(ssh.way, Way::Ssh("me@box.lan".into()));
        assert_eq!(ssh.folder, "/home/me/tree");
        assert_eq!(ssh.machine(), "box.lan");
        assert_eq!(Address::parse(&ssh.text()).unwrap(), ssh);
        let docker = Address::parse("docker:dev-1:/workspaces/tree").unwrap();
        assert_eq!(docker.way, Way::Docker("dev-1".into()));
        assert_eq!(docker.folder, "/workspaces/tree");
        let wsl = Address::parse("wsl:Ubuntu:/home/me/tree").unwrap();
        assert_eq!(wsl.way, Way::Wsl("Ubuntu".into()));
        assert!(Address::parse("box").is_err());
        assert!(Address::parse("docker:dev").is_err());
        assert!(Address::parse("box:").is_err());
    }

    #[test]
    fn a_word_with_a_quote_in_it_stays_one_word() {
        assert_eq!(quote("it's"), r"'it'\''s'");
    }

    /// Waits up to `seconds` for an event `want` answers true for, keeping each event seen.
    fn wait_for(heard: &std::sync::mpsc::Receiver<(String, Value)>, seen: &mut Vec<(String, Value)>, seconds: u64, want: impl Fn(&str, &Value) -> bool) -> bool {
        let deadline = Instant::now() + Duration::from_secs(seconds);
        while Instant::now() < deadline {
            if let Ok((event, body)) = heard.recv_timeout(Duration::from_millis(100)) {
                let found = want(&event, &body);
                seen.push((event, body));
                if found {
                    return true;
                }
            }
        }
        false
    }

    #[test]
    #[ignore = "runs orior-cli serve, built with cargo build --release --bin orior-cli, through a stand-in for ssh in bash"]
    fn a_link_answers_drops_joins_again_and_reads_a_kept_run_from_its_first_line() {
        let bash = crate::runner::bash().unwrap();
        let server = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("target").join("release").join(if cfg!(windows) { "orior-cli.exe" } else { "orior-cli" });
        assert!(server.is_file(), "{} is not built", server.display());
        let dir = dunce::canonicalize(std::env::temp_dir()).unwrap().join(format!("orior-link-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        let tree = dir.join("tree");
        for (name, text) in [
            ("src/cu/engine/engine_config.h", "// marks the tree\n"),
            ("a.txt", "one\n"),
            ("examples/demo/1_slow/slow.py", "import sys, time\nfor n in range(8):\n    print('line', n, flush=True)\n    time.sleep(0.4)\n"),
        ] {
            std::fs::create_dir_all(tree.join(name).parent().unwrap()).unwrap();
            std::fs::write(tree.join(name), text).unwrap();
        }
        let slash = |path: &std::path::Path| path.display().to_string().replace('\\', "/");
        let standin = dir.join("ssh.sh");
        std::fs::write(&standin, format!("while [ $# -gt 1 ]; do case \"$1\" in -o) shift 2 ;; -*) shift ;; *) shift; break ;; esac; done\nexport ORIOR_SERVER_HOME='{}' ORIOR_HOME='{}'\nexec bash -c \"$1\"\n", slash(&dir.join("server")), slash(&dir.join("home")))).unwrap();
        // SAFETY: the test sets these before the link starts any thread that reads them.
        unsafe {
            std::env::set_var("ORIOR_SSH", format!("{} {}", quote(&slash(&bash)), quote(&slash(&standin))));
            std::env::set_var("ORIOR_SERVER", &server);
        }
        let (sender, heard) = std::sync::mpsc::channel();
        let sender = Mutex::new(sender);
        let events: Emit = Arc::new(move |event, body| {
            let _ = sender.lock().unwrap().send((event.to_string(), body));
        });
        let link = Link::open(Address::parse(&format!("box:{}", slash(&tree))).unwrap(), events);
        let mut seen = Vec::new();
        assert!(wait_for(&heard, &mut seen, 60, |event, body| event == "link" && body["state"] == "joined"), "{seen:?}");
        assert_eq!(link.call("file_read", json!({"path": "a.txt"})).unwrap()["text"], "one\n");
        let jobs = link.call("catalog_read", json!({})).unwrap();
        let job = jobs.as_array().unwrap().iter().find(|job| job["file"].as_str().is_some_and(|file| file.ends_with("slow.py"))).unwrap()["id"].as_str().unwrap().to_string();
        let run = link.call("job_start", json!({"job": job, "values": {}})).unwrap().as_u64().unwrap();
        assert!(wait_for(&heard, &mut seen, 30, |event, body| event == "run-line" && body["run"] == run && body["text"].as_str().is_some_and(|text| text.contains("line 1"))), "{seen:?}");
        link.drop_now();
        assert!(wait_for(&heard, &mut seen, 30, |event, body| event == "link" && body["state"] == "dropped"), "{seen:?}");
        assert!(wait_for(&heard, &mut seen, 60, |event, body| event == "link" && body["state"] == "joined"), "{seen:?}");
        let kept = link.call("runs_kept", json!({})).unwrap();
        assert!(kept.as_array().unwrap().iter().any(|one| one["run"] == run), "{kept}");
        let mark = seen.len();
        link.call("run_follow", json!({"run": run})).unwrap();
        assert!(wait_for(&heard, &mut seen, 30, |event, body| event == "run-end" && body["run"] == run), "{seen:?}");
        let again: Vec<String> = seen[mark..].iter().filter(|(event, body)| event == "run-line" && body["run"] == run && body["stream"] == "stdout").filter_map(|(_, body)| body["text"].as_str().map(String::from)).collect();
        assert_eq!(again, (0..8).map(|n| format!("line {n}")).collect::<Vec<_>>());
        assert_eq!(seen.last().unwrap().1["code"], 0);
        link.end();
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    #[ignore = "builds orior's server for Linux and runs it in WSL's Ubuntu, with Python 3 there"]
    fn a_link_into_wsl_serves_a_tree_there_and_keeps_its_run_across_a_drop() {
        let wsl = |line: &str| Command::new("wsl").args(["-d", "Ubuntu", "--", "sh", "-c", line]).output().unwrap();
        let tree = format!("/var/tmp/orior-wsl-{}", std::process::id());
        let made = wsl(&format!(
            "mkdir -p {tree}/src/cu/engine {tree}/examples/demo/1_slow && echo '// marks the tree' > {tree}/src/cu/engine/engine_config.h && printf 'one\\n' > {tree}/a.txt && printf 'import time, platform\\nprint(platform.system(), flush=True)\\nfor n in range(6):\\n    print(\"line\", n, flush=True)\\n    time.sleep(0.4)\\n' > {tree}/examples/demo/1_slow/slow.py"
        ));
        assert!(made.status.success(), "{}", String::from_utf8_lossy(&made.stderr));
        let dir = dunce::canonicalize(std::env::temp_dir()).unwrap().join(format!("orior-wsl-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        // SAFETY: the test sets these before the link starts any thread that reads them.
        unsafe {
            std::env::remove_var("ORIOR_SERVER");
            std::env::set_var("ORIOR_SERVER_HOME", dir.join("server"));
            std::env::set_var("ORIOR_HOME", dir.join("home"));
            std::env::set_var("WSLENV", "ORIOR_SERVER_HOME/p:ORIOR_HOME/p");
        }
        let (sender, heard) = std::sync::mpsc::channel();
        let sender = Mutex::new(sender);
        let events: Emit = Arc::new(move |event, body| {
            let _ = sender.lock().unwrap().send((event.to_string(), body));
        });
        let link = Link::open(Address::parse(&format!("wsl:Ubuntu:{tree}")).unwrap(), events);
        let mut seen = Vec::new();
        assert!(wait_for(&heard, &mut seen, 600, |event, body| event == "link" && (body["state"] == "joined" || body["state"] == "failed")), "{seen:?}");
        assert_eq!(seen.last().unwrap().1["state"], "joined", "{seen:?}");
        assert_eq!(link.call("root_get", json!({})).unwrap(), tree.as_str());
        link.call("file_write", json!({"path": "b.txt", "text": "written there\n"})).unwrap();
        assert_eq!(String::from_utf8_lossy(&wsl(&format!("cat {tree}/b.txt")).stdout), "written there\n");
        let listed = link.call("tree_list", json!({"dir": ""})).unwrap();
        assert!(listed.as_array().unwrap().iter().any(|one| one["name"] == "b.txt"), "{listed}");
        let jobs = link.call("catalog_read", json!({})).unwrap();
        let job = jobs.as_array().unwrap().iter().find(|job| job["file"].as_str().is_some_and(|file| file.ends_with("slow.py"))).unwrap()["id"].as_str().unwrap().to_string();
        let run = link.call("job_start", json!({"job": job, "values": {}})).unwrap().as_u64().unwrap();
        assert!(wait_for(&heard, &mut seen, 30, |event, body| event == "run-line" && body["run"] == run && body["text"].as_str().is_some_and(|text| text.contains("line 1"))), "{seen:?}");
        link.drop_now();
        assert!(wait_for(&heard, &mut seen, 60, |event, body| event == "link" && body["state"] == "joined"), "{seen:?}");
        let mark = seen.len();
        link.call("run_follow", json!({"run": run})).unwrap();
        assert!(wait_for(&heard, &mut seen, 30, |event, body| event == "run-end" && body["run"] == run), "{seen:?}");
        let again: Vec<String> = seen[mark..].iter().filter(|(event, body)| event == "run-line" && body["run"] == run && body["stream"] == "stdout").filter_map(|(_, body)| body["text"].as_str().map(String::from)).collect();
        assert_eq!(again[0], "Linux");
        assert_eq!(again[1..], (0..6).map(|n| format!("line {n}")).collect::<Vec<_>>());
        link.end();
        let _ = wsl(&format!("rm -rf {tree}"));
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_machine_s_target_is_read_from_uname() {
        assert_eq!(target_of("Linux x86_64").unwrap(), "x86_64-unknown-linux-musl");
        assert_eq!(target_of("Linux aarch64").unwrap(), "aarch64-unknown-linux-musl");
        assert!(target_of("Darwin arm64").is_err());
        assert!(is_windows("MINGW64_NT-10.0-26200 x86_64"));
    }
}

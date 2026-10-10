// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The tree's side of the window: every command the page calls that reads or changes the tree, runs
//! its jobs, its tests and its profiles, or speaks to its language servers and its debugger, called by
//! name with its arguments as JSON and answered as JSON, with what each tells as it goes sent as an
//! event by name. The window calls it in its own process for a tree on this machine, and over a link
//! for a tree on another, where `orior-cli serve` answers each line of its input with a line of its
//! output.

use std::collections::HashMap;
use std::io::{BufRead, Read, Seek, SeekFrom, Write};
use std::path::{Path, PathBuf};
use std::process::Stdio;
use std::sync::{Arc, Mutex, OnceLock};
use std::time::Duration;

use serde::Serialize;
use serde::de::DeserializeOwned;
use serde_json::{Map, Value, json};

use crate::{bridge, catalog, debug, defs, files, format, git, history, patterns, plugins, root, run_file, runner, servers, symbols, toolchains, validate, watch};

/// Where what the server tells goes: the event's name and its body.
pub type Emit = Arc<dyn Fn(&str, Value) + Send + Sync>;

/// The tree open and everything that runs on it.
#[derive(Default)]
pub struct Server {
    root: Mutex<Option<PathBuf>>,
    emit: OnceLock<Emit>,
    runs: runner::Runs,
    servers: Arc<servers::Servers>,
    symbols: symbols::Index,
    debugger: Arc<debug::Debugger>,
    tests: crate::test_runs::Runs,
    profiles: crate::profile::Profiles,
    /// The watch over the tree open, which tells of changes made outside the window.
    watcher: Mutex<Option<watch::Watcher>>,
    /// The folder the runs are kept in, where the server answers a window over a link: each run a
    /// process of its own, whose lines a window that joins again reads.
    keeping: Option<PathBuf>,
}

/// A call's arguments, each found by its own name or by the page's, as the page writes names of more
/// than one word: `with_errors` as `withErrors`.
struct Args(Map<String, Value>);

impl Args {
    fn get<T: DeserializeOwned>(&self, key: &str) -> Result<T, String> {
        let value = self.0.get(key).or_else(|| self.0.get(&camel(key))).cloned().unwrap_or(Value::Null);
        serde_json::from_value(value).map_err(|error| format!("{key}: {error}"))
    }
}

fn camel(key: &str) -> String {
    let mut out = String::with_capacity(key.len());
    let mut upper = false;
    for c in key.chars() {
        if c == '_' {
            upper = true;
        } else if upper {
            out.extend(c.to_uppercase());
            upper = false;
        } else {
            out.push(c);
        }
    }
    out
}

fn give<T: Serialize>(value: T) -> Result<Value, String> {
    serde_json::to_value(value).map_err(|error| error.to_string())
}

/// A job as the window lists it: the job, and how many steps it runs, which the run's fuse burns
/// through one at a time.
#[derive(Serialize)]
struct Listed {
    #[serde(flatten)]
    job: catalog::Job,
    steps: usize,
}

#[derive(Serialize)]
struct Toolchains {
    tools: Vec<toolchains::Found>,
    groups: Vec<String>,
    own: Option<toolchains::Own>,
}

/// A folder mounted beside the tree: its name, its path on the disk, and whether it is a folder, a
/// single file being mounted alone.
#[derive(Serialize)]
struct Mounted {
    name: String,
    path: String,
    dir: bool,
}

/// What Search Structurally found: each match, and each file's edits where a template was given.
#[derive(Serialize)]
struct Shapes {
    found: Vec<crate::shape::Found>,
    files: Vec<servers::FileEdit>,
}

/// What choosing an environment did: its Python, and the packages beside it it has not installed.
#[derive(Serialize)]
struct EnvironmentUsed {
    python: Option<String>,
    missing: Vec<String>,
}

/// The most declarations one search of the tree's symbols gives.
const SYMBOLS_MOST: usize = 200;

/// A path a server named, as the page names files: under the tree, from its top folder, and
/// elsewhere whole.
fn tree_path(root: &Path, path: &str) -> String {
    let path = PathBuf::from(path);
    let path = dunce::canonicalize(&path).unwrap_or(path);
    let root = dunce::canonicalize(root).unwrap_or_else(|_| root.to_path_buf());
    // A file of a folder mounted beside the tree goes by its mount's name.
    if !path.starts_with(&root) && root::mounts().iter().any(|(_, base)| path.starts_with(base)) {
        return root::relative(&root, &path);
    }
    path.strip_prefix(&root).map(|inside| inside.to_string_lossy().replace('\\', "/")).unwrap_or_else(|_| path.display().to_string())
}

/// Each file's edits, each path as `tree_path` gives it.
fn tree_edits(root: &Path, files: Vec<servers::FileEdit>) -> Vec<servers::FileEdit> {
    files
        .into_iter()
        .map(|mut file| {
            file.path = tree_path(root, &file.path);
            file
        })
        .collect()
}

/// A function of the call hierarchy, its paths as `tree_path` gives them.
fn tree_call(root: &Path, mut call: servers::Call) -> servers::Call {
    call.path = tree_path(root, &call.path);
    call.site = tree_path(root, &call.site);
    call
}

/// Bytes as base64, for a file's bytes sent as JSON.
pub fn base64(bytes: &[u8]) -> String {
    const LETTERS: &[u8; 64] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    let mut out = String::with_capacity(bytes.len().div_ceil(3) * 4);
    for chunk in bytes.chunks(3) {
        let n = (u32::from(chunk[0]) << 16) | (u32::from(*chunk.get(1).unwrap_or(&0)) << 8) | u32::from(*chunk.get(2).unwrap_or(&0));
        for at in 0..4 {
            if at <= chunk.len() {
                out.push(LETTERS[(n >> (18 - 6 * at) & 63) as usize] as char);
            } else {
                out.push('=');
            }
        }
    }
    out
}

/// The bytes base64 gives, or None where it is not base64.
pub fn unbase64(text: &str) -> Option<Vec<u8>> {
    let value = |c: u8| match c {
        b'A'..=b'Z' => Some(c - b'A'),
        b'a'..=b'z' => Some(c - b'a' + 26),
        b'0'..=b'9' => Some(c - b'0' + 52),
        b'+' => Some(62),
        b'/' => Some(63),
        _ => None,
    };
    let bytes: Vec<u8> = text.bytes().filter(|c| !c.is_ascii_whitespace()).collect();
    let mut out = Vec::with_capacity(bytes.len() / 4 * 3);
    for chunk in bytes.chunks(4) {
        let mut n = 0u32;
        let mut held = 0;
        for &c in chunk {
            if c == b'=' {
                break;
            }
            n = (n << 6) | u32::from(value(c)?);
            held += 1;
        }
        n <<= 6 * (4 - held);
        out.extend_from_slice(&n.to_be_bytes()[1..held.max(1)]);
    }
    Some(out)
}

/// The folder on this machine that keeps the runs started over a link: each run's job, and every line
/// it said, for a window that joins again to read them.
fn kept_folder() -> Option<PathBuf> {
    crate::home::folder().map(|folder| folder.join("runs"))
}

/// How long a kept run is kept after it starts.
const KEPT_FOR: std::time::Duration = std::time::Duration::from_secs(7 * 24 * 3600);

/// A file of the kept run `base`, by its kind: `job`, `said`, `pid` or `stop`.
fn kept_file(base: &Path, kind: &str) -> PathBuf {
    let mut name = base.as_os_str().to_owned();
    name.push(".");
    name.push(kind);
    PathBuf::from(name)
}

/// A kept run's number: the milliseconds since 1970 it started at, each after the last.
fn kept_number() -> u64 {
    static LAST: std::sync::atomic::AtomicU64 = std::sync::atomic::AtomicU64::new(0);
    let now = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map_or(0, |since| since.as_millis() as u64);
    let mut last = LAST.load(std::sync::atomic::Ordering::SeqCst);
    loop {
        let next = now.max(last + 1);
        match LAST.compare_exchange(last, next, std::sync::atomic::Ordering::SeqCst, std::sync::atomic::Ordering::SeqCst) {
            Ok(_) => return next,
            Err(seen) => last = seen,
        }
    }
}

/// Whether the process `pid` is running.
fn running(pid: u32) -> bool {
    #[cfg(windows)]
    {
        use windows_sys::Win32::Foundation::{CloseHandle, STILL_ACTIVE};
        use windows_sys::Win32::System::Threading::{GetExitCodeProcess, OpenProcess, PROCESS_QUERY_LIMITED_INFORMATION};
        // SAFETY: the handle is checked before it is used and closed after.
        unsafe {
            let handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, 0, pid);
            if handle.is_null() {
                return false;
            }
            let mut code = 0u32;
            let read = GetExitCodeProcess(handle, &mut code);
            CloseHandle(handle);
            read != 0 && code == STILL_ACTIVE as u32
        }
    }
    #[cfg(not(windows))]
    {
        Path::new("/proc").exists().then(|| Path::new("/proc").join(pid.to_string()).exists()).unwrap_or(true)
    }
}

/// Runs the kept run `base` as its job file says, every line it says added to its said file as it
/// comes, and stops it where its stop file is made. `orior-cli keep-run` runs this, a process of its
/// own that outlives the server that started it. Gives the code to exit with.
pub fn keep_run(base: &Path) -> i32 {
    let Some(run) = base.file_name().and_then(|name| name.to_str()).and_then(|name| name.parse::<u64>().ok()) else {
        return 2;
    };
    let said_file = match std::fs::OpenOptions::new().create(true).append(true).open(kept_file(base, "said")) {
        Ok(file) => Arc::new(Mutex::new(file)),
        Err(_) => return 2,
    };
    let _ = std::fs::write(kept_file(base, "pid"), std::process::id().to_string());
    let sink: runner::Sink = Arc::new(move |said| {
        let line = match said {
            runner::Said::Line(mut line) => {
                line.run = run;
                json!({"line": line})
            }
            runner::Said::End(mut end) => {
                end.run = run;
                json!({"end": end})
            }
        };
        if let Ok(mut file) = said_file.lock() {
            let _ = writeln!(file, "{line}");
            let _ = file.flush();
        }
    });
    let failed = |said: String| {
        sink(runner::Said::Line(runner::Line { run, stream: "stderr", text: said, ms: 0.0 }));
        sink(runner::Said::End(runner::End { run, code: None, stopped: false, views: Vec::new(), ms: 0.0 }));
        1
    };
    let spec: Value = match std::fs::read_to_string(kept_file(base, "job")).map_err(|error| error.to_string()).and_then(|text| serde_json::from_str(&text).map_err(|error| error.to_string())) {
        Ok(spec) => spec,
        Err(said) => return failed(said),
    };
    let root = PathBuf::from(spec["root"].as_str().unwrap_or(""));
    let id = spec["job"].as_str().unwrap_or("");
    let Some(job) = catalog::read(&root).into_iter().find(|job| job.id == id) else {
        return failed(format!("no job {id}"));
    };
    let values: HashMap<String, Vec<String>> = serde_json::from_value(spec["values"].clone()).unwrap_or_default();
    let runs = Arc::new(runner::Runs::default());
    match runs.start(sink.clone(), root, job, values) {
        Ok((inner, waits)) => {
            let stop = kept_file(base, "stop");
            let stopping = runs.clone();
            std::thread::spawn(move || {
                while !stop.exists() {
                    std::thread::sleep(std::time::Duration::from_millis(200));
                }
                let _ = stopping.stop(inner);
            });
            let _ = waits.join();
            0
        }
        Err(said) => failed(said),
    }
}

impl Server {
    pub fn new(root: Option<PathBuf>) -> Self {
        Server { root: Mutex::new(root), ..Server::default() }
    }

    /// Sends what the server tells to `emit`, from now on; before, it goes nowhere.
    pub fn tell_to(&self, emit: Emit) {
        let _ = self.emit.set(emit);
    }

    fn emitting(&self) -> Emit {
        self.emit.get().cloned().unwrap_or_else(|| Arc::new(|_, _| {}))
    }

    pub fn root(&self) -> Result<PathBuf, String> {
        self.root.lock().map_err(|e| e.to_string())?.clone().ok_or_else(|| "no orior tree is open".to_string())
    }

    /// The tree open, or None.
    pub fn root_now(&self) -> Option<PathBuf> {
        self.root.lock().ok().and_then(|root| root.clone())
    }

    pub fn servers(&self) -> &Arc<servers::Servers> {
        &self.servers
    }

    /// Ends every language server and debug session, as the program ends.
    pub fn stop_all(&self) {
        self.servers.stop_all();
        self.debugger.stop_all();
    }

    /// The folder of a repository of the tree, by its path in the tree, or the tree's own folder where
    /// none is named.
    fn repo_of(&self, repo: Option<&str>) -> Result<PathBuf, String> {
        let root = self.root()?;
        match repo.filter(|repo| !repo.is_empty()) {
            Some(repo) => root::inside(&root, repo),
            None => Ok(root),
        }
    }

    /// Watches the tree open for changes made outside the window, each batch told as tree-changed, in
    /// place of the watch over the tree open before.
    pub fn watch(&self) {
        let Ok(root) = self.root() else { return };
        let emit = self.emitting();
        let servers = self.servers.clone();
        let tree = root.clone();
        let watcher = watch::start(root, move |changed| {
            servers.changed(&tree, &changed.files);
            emit("tree-changed", serde_json::to_value(changed).unwrap_or(Value::Null));
        });
        if let Ok(mut held) = self.watcher.lock() {
            *held = Some(watcher);
        }
    }

    /// What a server tells the page, each path as `tree_path` gives it under `tree`.
    fn emitter(&self, tree: PathBuf) -> servers::Emit {
        let emit = self.emitting();
        let servers = self.servers.clone();
        Arc::new(move |told| match told {
            servers::Told::Diagnostics(mut diagnostics) => {
                diagnostics.path = tree_path(&tree, &diagnostics.path);
                emit("lsp-diagnostics", serde_json::to_value(diagnostics).unwrap_or(Value::Null));
            }
            servers::Told::Edits(files) => emit("lsp-edits", serde_json::to_value(tree_edits(&tree, files)).unwrap_or(Value::Null)),
            servers::Told::Hints => emit("lsp-hints", Value::Null),
            servers::Told::Tokens => {
                servers.forget_tokens();
                emit("lsp-parse", Value::Null);
            }
            servers::Told::Checking { done, total } => emit("tree-check", json!({"done": done, "total": total})),
        })
    }

    fn root_set(&self, path: String) -> Result<String, String> {
        let path = dunce::canonicalize(&path).map_err(|e| format!("{path}: {e}"))?;
        if !root::holds_tree(&path) {
            return Err(format!("{} holds no orior tree", path.display()));
        }
        let mut root = self.root.lock().map_err(|e| e.to_string())?;
        let moved = root.as_ref() != Some(&path);
        *root = Some(path.clone());
        drop(root);
        // The folders mounted beside one tree are its own; another tree mounts its own. A server
        // answers for the tree it started in; another tree starts its own as its files open.
        if moved {
            root::unmount_all();
            self.servers.let_go();
            self.symbols.forget();
            self.watch();
        }
        Ok(path.to_string_lossy().into_owned())
    }

    fn job_start(&self, job: String, values: HashMap<String, Vec<String>>) -> Result<u64, String> {
        let root = self.root()?;
        let found = catalog::read(&root).into_iter().find(|j| j.id == job).ok_or_else(|| format!("no job {job}"))?;
        let emit = self.emitting();
        let sink: runner::Sink = Arc::new(move |said| match said {
            runner::Said::Line(line) => emit("run-line", serde_json::to_value(line).unwrap_or(Value::Null)),
            runner::Said::End(end) => emit("run-end", serde_json::to_value(end).unwrap_or(Value::Null)),
        });
        self.runs.start(sink, root, found, values).map(|(run, _)| run)
    }

    fn shape_search(&self, language: String, pattern: String, template: Option<String>, path: Option<String>) -> Result<Shapes, String> {
        let root = self.root()?;
        let files = match path {
            Some(path) => vec![path],
            None => {
                let languages = plugins::languages();
                files::all(&root).into_iter().filter(|file| Path::new(file).extension().and_then(|ext| languages.get(&ext.to_string_lossy().to_lowercase())).is_some_and(|one| *one == language)).collect()
            }
        };
        let mut shapes = Shapes { found: Vec::new(), files: Vec::new() };
        for file in files {
            let full = root::full(&root, &file);
            let Some(text) = self.servers.text_of(&full).or_else(|| std::fs::read_to_string(&full).ok()) else {
                continue;
            };
            let (found, edits) = crate::shape::search(&language, &file, &text, &pattern, template.as_deref());
            if !edits.is_empty() {
                shapes.files.push(servers::FileEdit { path: file.clone(), edits });
            }
            shapes.found.extend(found);
        }
        Ok(shapes)
    }

    /// The POM at `path` of the tree, `pom.xml` at its top where none is given, and its text, `text`
    /// where the editor holds it.
    fn pom_of(&self, path: Option<String>, text: Option<String>) -> Result<(PathBuf, PathBuf, String), String> {
        let root = self.root()?;
        let pom = path.filter(|path| crate::builds::kind_of(Path::new(path)) == Some(crate::builds::Kind::Maven)).map(|path| root::full(&root, &path)).unwrap_or_else(|| root.join("pom.xml"));
        let text = match text {
            Some(text) => text,
            None => std::fs::read_to_string(&pom).unwrap_or_default(),
        };
        Ok((root, pom, text))
    }

    fn env_use(&self, kind: Option<String>, place: String, requires: Vec<String>) -> Result<EnvironmentUsed, String> {
        let used = match kind {
            Some(kind) => {
                let env = crate::envs::resolve(&self.root()?, &kind, &place)?;
                let missing = crate::envs::missing(&env, &requires);
                let python = env.python.as_ref().map(|python| python.display().to_string());
                toolchains::set_environment(Some(env));
                EnvironmentUsed { python, missing }
            }
            None => {
                toolchains::set_environment(None);
                EnvironmentUsed { python: None, missing: Vec::new() }
            }
        };
        self.servers.settings_changed();
        Ok(used)
    }

    /// Writes edits to files the editor does not have open, each under the tree. Says how many files
    /// it wrote; a file outside the tree is left as it is and named.
    fn edits_write(&self, files: Vec<servers::FileEdit>) -> Result<usize, String> {
        let root = self.root()?;
        let mut written = 0;
        for file in files {
            if Path::new(&file.path).is_absolute() || file.path.split('/').any(|part| part == "..") {
                return Err(format!("{} is outside the tree and was not changed", file.path));
            }
            let path = root::full(&root, &file.path);
            let text = std::fs::read_to_string(&path).map_err(|error| format!("{}: {error}", file.path))?;
            std::fs::write(&path, servers::apply(&text, &file.edits)).map_err(|error| format!("{}: {error}", file.path))?;
            written += 1;
        }
        Ok(written)
    }

    fn debug_start(&self, start: debug::Start, options: debug::Options) -> Result<debug::SessionInfo, String> {
        let root = self.root()?;
        let debugger = self.debugger.clone();
        let emit = self.emitting();
        let told: debug::Emit = Arc::new(move |session, event, body| {
            if event == "terminated" || event == "adapterStopped" {
                debugger.ended(session);
            }
            if event == "process" {
                if let Some(pid) = body["systemProcessId"].as_u64() {
                    debugger.process(session, pid as u32);
                }
            }
            emit("debug-event", json!({"session": session, "event": event, "body": body}));
        });
        self.debugger.start(&root, &start, &options, told)
    }

    fn file_write(&self, path: String, text: String) -> Result<(), String> {
        let root = self.root()?;
        let before = std::fs::read_to_string(root::full(&root, &path)).ok();
        files::write(&root, &path, &text)?;
        if let Err(error) = history::keep(&root, &path, before.as_deref(), &text) {
            eprintln!("orior: local history of {path}: {error}");
        }
        Ok(())
    }

    /// A full path as the tree names it, relative and with forward slashes, or null where it is
    /// outside the tree.
    fn tree_relative(&self, path: String) -> Option<String> {
        let root = self.root().ok()?;
        let full = dunce::canonicalize(&path).ok()?;
        // A file under a folder mounted beside the tree goes by its mount's name as well.
        let mounted = root::mounts().iter().any(|(_, base)| full.starts_with(base));
        (full.starts_with(&root) || mounted).then(|| root::relative(&root, &full))
    }

    /// Starts `job` as a kept run, a process of its own whose lines are kept on this machine, and
    /// follows it. Gives its number.
    fn kept_start(&self, keep: &Path, job: String, values: HashMap<String, Vec<String>>) -> Result<u64, String> {
        let root = self.root()?;
        let found = catalog::read(&root).into_iter().find(|one| one.id == job).ok_or_else(|| format!("no job {job}"))?;
        runner::check(&found, &values)?;
        std::fs::create_dir_all(keep).map_err(|error| format!("{}: {error}", keep.display()))?;
        let run = kept_number();
        let base = keep.join(run.to_string());
        std::fs::write(kept_file(&base, "job"), json!({"job": job, "values": values, "root": root}).to_string()).map_err(|error| error.to_string())?;
        let mut command = std::process::Command::new(std::env::current_exe().map_err(|error| error.to_string())?);
        command.arg("keep-run").arg(&base).stdin(Stdio::null()).stdout(Stdio::null()).stderr(Stdio::null());
        runner::quiet(&mut command);
        command.spawn().map_err(|error| error.to_string())?;
        self.follow(base);
        Ok(run)
    }

    /// Tells each line the kept run `base` said, from its first, as run-line, and its end as run-end,
    /// as they come. A run whose process is gone with no end said ends as stopped.
    fn follow(&self, base: PathBuf) {
        let emit = self.emitting();
        std::thread::spawn(move || {
            let run = base.file_name().and_then(|name| name.to_str()).and_then(|name| name.parse::<u64>().ok()).unwrap_or(0);
            let said = kept_file(&base, "said");
            let mut at = 0u64;
            let mut held = String::new();
            let mut quiet = 0u32;
            loop {
                let mut bytes = Vec::new();
                if let Ok(mut file) = std::fs::File::open(&said) {
                    if file.seek(SeekFrom::Start(at)).is_ok() {
                        let _ = file.read_to_end(&mut bytes);
                    }
                }
                if !bytes.is_empty() {
                    at += bytes.len() as u64;
                    held.push_str(&String::from_utf8_lossy(&bytes));
                    quiet = 0;
                    while let Some(cut) = held.find('\n') {
                        let line: String = held.drain(..=cut).collect();
                        let Ok(line) = serde_json::from_str::<Value>(&line) else { continue };
                        if let Some(end) = line.get("end") {
                            emit("run-end", end.clone());
                            return;
                        }
                        if let Some(one) = line.get("line") {
                            emit("run-line", one.clone());
                        }
                    }
                    continue;
                }
                quiet += 1;
                if quiet % 20 == 0 {
                    let pid = std::fs::read_to_string(kept_file(&base, "pid")).ok().and_then(|text| text.trim().parse::<u32>().ok());
                    if pid.is_some_and(|pid| !running(pid)) && std::fs::metadata(&said).map_or(0, |meta| meta.len()) == at {
                        emit("run-end", json!({"run": run, "code": null, "stopped": true, "views": [], "ms": 0.0}));
                        return;
                    }
                }
                std::thread::sleep(Duration::from_millis(50));
            }
        });
    }

    /// The kept runs of the tree open, the oldest first: each one's number, its job, and whether it
    /// has ended. Runs older than KEPT_FOR are let go of.
    fn kept_runs(&self) -> Vec<Value> {
        let (Some(keep), Some(root)) = (self.keeping.as_ref(), self.root_now()) else { return Vec::new() };
        let root = root.to_string_lossy().into_owned();
        let now = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map_or(0, |since| since.as_millis() as u64);
        let mut found = Vec::new();
        for entry in std::fs::read_dir(keep).into_iter().flatten().flatten() {
            let name = entry.file_name().to_string_lossy().into_owned();
            let Some(run) = name.strip_suffix(".job").and_then(|number| number.parse::<u64>().ok()) else { continue };
            let base = keep.join(run.to_string());
            if now.saturating_sub(run) > KEPT_FOR.as_millis() as u64 {
                for kind in ["job", "said", "pid", "stop"] {
                    let _ = std::fs::remove_file(kept_file(&base, kind));
                }
                continue;
            }
            let Some(spec) = std::fs::read_to_string(kept_file(&base, "job")).ok().and_then(|text| serde_json::from_str::<Value>(&text).ok()) else { continue };
            if spec["root"].as_str() != Some(root.as_str()) {
                continue;
            }
            let ended = std::fs::read_to_string(kept_file(&base, "said")).is_ok_and(|text| text.lines().last().is_some_and(|line| line.starts_with("{\"end\"")));
            found.push(json!({"run": run, "job": spec["job"], "ended": ended}));
        }
        found.sort_by_key(|one| one["run"].as_u64());
        found
    }

    /// Stops a run: a kept run by its stop file, which its own process watches for.
    fn job_stop(&self, run: u64) -> Result<(), String> {
        if let Some(keep) = &self.keeping {
            let base = keep.join(run.to_string());
            if kept_file(&base, "job").exists() {
                return std::fs::write(kept_file(&base, "stop"), "").map_err(|error| error.to_string());
            }
        }
        self.runs.stop(run)
    }

    /// Answers the call `name` with `args`, or says why it cannot.
    pub fn call(&self, name: &str, args: Value) -> Result<Value, String> {
        let a = Args(match args {
            Value::Object(map) => map,
            _ => Map::new(),
        });
        let path = || a.get::<String>("path");
        let at = |path: &str| -> Result<PathBuf, String> { Ok(root::full(&self.root()?, path)) };
        match name {
            "root_get" => give(self.root_now().map(|p| p.to_string_lossy().into_owned())),
            "root_set" => self.root_set(a.get("path")?).and_then(give),
            "catalog_read" => give(catalog::read(&self.root()?).into_iter().map(|job| Listed { steps: job.steps.len(), job }).collect::<Vec<_>>()),
            "definitions_read" => give(defs::read(&self.root()?)),
            "job_start" => match &self.keeping {
                Some(keep) => self.kept_start(keep, a.get("job")?, a.get::<Option<_>>("values")?.unwrap_or_default()).and_then(give),
                None => self.job_start(a.get("job")?, a.get::<Option<_>>("values")?.unwrap_or_default()).and_then(give),
            },
            "job_stop" => self.job_stop(a.get("run")?).and_then(give),
            "runs_kept" => give(self.kept_runs()),
            "run_follow" => {
                let keep = self.keeping.as_ref().ok_or("this tree keeps no runs")?;
                self.follow(keep.join(a.get::<u64>("run")?.to_string()));
                give(())
            }
            "tree_list" => files::list(&self.root()?, &a.get::<String>("dir")?).and_then(give),
            "tree_find" => give(files::find(&self.root()?, &a.get::<String>("query")?)),
            "tree_files" => give(files::all(&self.root()?)),
            "files_find" => give(files::ranked(&self.root()?, &a.get::<String>("query")?, &a.get::<Vec<String>>("recent")?, a.get("most")?)),
            "git_branches" => give(git::branches(&self.repo_of(a.get::<Option<String>>("repo")?.as_deref())?)),
            "git_branch" => git::branch_act(&self.repo_of(a.get::<Option<String>>("repo")?.as_deref())?, &a.get::<String>("act")?, &a.get::<String>("name")?, a.get::<Option<String>>("to")?.as_deref().unwrap_or("")).and_then(give),
            "git_commit_parts" => git::commit_parts(&self.root()?, &a.get::<String>("message")?, &a.get::<Vec<String>>("whole")?, &a.get::<Vec<git::Part>>("parts")?).and_then(give),
            "git_resolve" => git::resolve(&self.root()?, &path()?).and_then(give),
            "indent_for" => give(crate::editorconfig::indent(&root::inside(&self.root()?, &path()?)?)),
            "patterns_set" => {
                let part: patterns::Part = a.get("part")?;
                let changed = patterns::set(part, &a.get::<Vec<String>>("lines")?);
                if changed && matches!(part, patterns::Part::Search) {
                    files::forget_held();
                }
                give(changed)
            }
            "symbols_find" => {
                let root = self.root()?;
                let listed = root.clone();
                self.symbols.refresh(&root, move || files::all(&listed));
                give(self.symbols.find(&a.get::<String>("query")?, SYMBOLS_MOST))
            }
            "tree_search" => {
                let set = patterns::Patterns::read_set(&a.get::<Option<Vec<String>>>("set")?.unwrap_or_default());
                files::search(&self.root()?, &a.get::<String>("query")?, a.get("how")?, &set).and_then(give)
            }
            "tree_changed" => give(git::changed(&self.root()?, a.get::<Option<Vec<String>>>("repos")?.as_deref())),
            "git_repositories" => give(git::repositories(&self.root()?)),
            "tree_branch" => give(git::branch(&self.repo_of(a.get::<Option<String>>("repo")?.as_deref())?)),
            "file_head" => give(git::head_text(&self.root()?, &path()?)),
            "file_commits" => git::commits(&self.root()?, &path()?).and_then(give),
            "git_commit" => git::commit(&self.root()?, &a.get::<String>("message")?, &a.get::<Vec<String>>("paths")?).and_then(give),
            "git_push" => git::push(&self.repo_of(a.get::<Option<String>>("repo")?.as_deref())?).and_then(give),
            "git_pull" => git::pull(&self.repo_of(a.get::<Option<String>>("repo")?.as_deref())?).and_then(give),
            "git_rollback" => git::rollback(&self.root()?, &path()?).and_then(give),
            "git_ahead_behind" => give(git::ahead_behind(&self.repo_of(a.get::<Option<String>>("repo")?.as_deref())?)),
            "git_graph" => give(git::graph(&self.repo_of(a.get::<Option<String>>("repo")?.as_deref())?, a.get::<Option<String>>("query")?.as_deref().unwrap_or(""))),
            "git_touched" => {
                let repo: Option<String> = a.get("repo")?;
                let mut found = git::touched(&self.repo_of(repo.as_deref())?, &a.get::<String>("id")?)?;
                if let Some(repo) = repo.filter(|repo| !repo.is_empty()) {
                    for file in &mut found {
                        file.path = format!("{repo}/{}", file.path);
                        file.was = file.was.take().map(|was| format!("{repo}/{was}"));
                    }
                }
                give(found)
            }
            "git_stashes" => give(git::stashes(&self.repo_of(a.get::<Option<String>>("repo")?.as_deref())?)),
            "git_stash" => git::stash_act(&self.repo_of(a.get::<Option<String>>("repo")?.as_deref())?, &a.get::<String>("act")?, a.get::<Option<String>>("name")?.as_deref().unwrap_or(""), a.get::<Option<String>>("message")?.as_deref().unwrap_or("")).and_then(give),
            "git_elsewhere" => give(git::elsewhere(&self.repo_of(a.get::<Option<String>>("repo")?.as_deref())?)),
            "git_cherry_pick" => git::cherry_pick(&self.repo_of(a.get::<Option<String>>("repo")?.as_deref())?, &a.get::<String>("id")?).and_then(give),
            "git_line_history" => git::line_history(&self.root()?, &path()?, a.get("text")?).and_then(give),
            "file_at" => git::text_at(&self.root()?, &path()?, &a.get::<String>("id")?).and_then(give),
            "format_text" => format::format(&at(&path()?)?, &a.get::<String>("language")?, &a.get::<String>("text")?).and_then(give),
            "format_width" => give(format::width(&at(&path()?)?, &a.get::<String>("language")?)),
            "format_margin" => give(format::margin(&at(&path()?)?, &a.get::<String>("language")?).map(|(width, by)| json!({"width": width, "by": by}))),
            "tree_holds" => give(root::holds_tree(Path::new(&path()?))),
            "tree_mount" => root::mount(Path::new(&path()?)).and_then(give),
            "tree_unmount" => {
                root::unmount(&a.get::<String>("name")?);
                give(())
            }
            "tree_mounts" => give(root::mounts().into_iter().map(|(name, path)| Mounted { name, dir: path.is_dir(), path: path.display().to_string() }).collect::<Vec<_>>()),
            "lsp_open" => {
                let root = self.root()?;
                self.servers.open(&root, &root::full(&root, &path()?), &a.get::<String>("language")?, &a.get::<String>("text")?, &self.emitter(root.clone())).and_then(give)
            }
            "problems_check" => {
                let root = self.root()?;
                give(self.servers.check_tree(&root, files::all(&root), &self.emitter(root.clone())))
            }
            "lsp_references" => {
                let root = self.root()?;
                let found = self.servers.references(&root::full(&root, &path()?), a.get("line")?, a.get("col")?)?;
                give(
                    found
                        .into_iter()
                        .map(|mut one| {
                            one.path = tree_path(&root, &one.path);
                            one
                        })
                        .collect::<Vec<_>>(),
                )
            }
            "lsp_renamable" => self.servers.renamable(&at(&path()?)?, a.get("line")?, a.get("col")?).and_then(give),
            "lsp_rename" => {
                let root = self.root()?;
                give(tree_edits(&root, self.servers.rename(&root::full(&root, &path()?), a.get("line")?, a.get("col")?, &a.get::<String>("name")?)?))
            }
            "lsp_actions" => self.servers.actions(&at(&path()?)?, a.get("from")?, a.get("to")?).and_then(give),
            "lsp_act" => {
                let root = self.root()?;
                give(tree_edits(&root, self.servers.act(&root::full(&root, &path()?), &a.get::<Value>("raw")?)?))
            }
            "parse_colors" => self.servers.colors(&at(&path()?)?, a.get("from")?, a.get("to")?).and_then(give),
            "parse_spans" => self.servers.spans(&at(&path()?)?, a.get("from")?, a.get("to")?).and_then(give),
            "shape_search" => self.shape_search(a.get("language")?, a.get("pattern")?, a.get("template")?, a.get("path")?).and_then(give),
            "maven_settings" => {
                let (root, pom, text) = self.pom_of(a.get("path")?, a.get("text")?)?;
                give(crate::builds::maven::settings(&root, &pom, &text))
            }
            "maven_set" => {
                let (root, pom, text) = self.pom_of(a.get("path")?, a.get("text")?)?;
                give(tree_edits(&root, crate::builds::maven::set(&pom, &text, &a.get::<String>("key")?, &a.get::<String>("value")?)?))
            }
            "snapshots_line" => {
                let root = self.root()?;
                crate::builds::snapshots_line(&root, a.get::<Option<String>>("path")?.map(|path| root::full(&root, &path)).as_deref()).and_then(give)
            }
            "tests_found" => give(crate::testing::found(&self.root()?)),
            "tests_run" => {
                let emit = self.emitting();
                let tell: crate::test_runs::Tell = Arc::new(move |heard| emit("tests-run", serde_json::to_value(heard).unwrap_or(Value::Null)));
                self.tests.start(&self.root()?, a.get("given")?, a.get("parallel")?, a.get("cover")?, tell).and_then(give)
            }
            "tests_stop" => {
                self.tests.stop();
                give(())
            }
            "profile_start" => {
                let emit = self.emitting();
                let tell: crate::profile::Tell = Arc::new(move |heard| emit("profile", serde_json::to_value(heard).unwrap_or(Value::Null)));
                self.profiles.start(&self.root()?, &path()?, a.get::<Option<String>>("remote")?.as_deref(), tell).and_then(give)
            }
            "profile_stop" => {
                self.profiles.stop();
                give(())
            }
            "template_keep" => crate::templates::keep(&self.root()?, &a.get::<String>("name")?).map(|made| made.display().to_string()).and_then(give),
            "checkers_set" => {
                let names: Vec<String> = a.get("names")?;
                give(match self.root() {
                    Ok(root) => self.servers.set_checkers(&names, Some(&root), &self.emitter(root.clone())),
                    Err(_) => self.servers.set_checkers(&names, None, &(Arc::new(|_| {}) as servers::Emit)),
                })
            }
            "envs_found" => give(crate::envs::found(&self.root()?)),
            "env_use" => self.env_use(a.get("kind")?, a.get("place")?, a.get("requires")?).and_then(give),
            "lsp_hints" => self.servers.hints(&at(&path()?)?, a.get("from")?, a.get("to")?).and_then(give),
            "calls_root" => {
                let root = self.root()?;
                give(self.servers.call_root(&root::full(&root, &path()?), a.get("line")?, a.get("col")?)?.map(|call| tree_call(&root, call)))
            }
            "calls_of" => {
                let root = self.root()?;
                give(self.servers.calls(&root::full(&root, &path()?), &a.get::<Value>("item")?, a.get("incoming")?)?.into_iter().map(|call| tree_call(&root, call)).collect::<Vec<_>>())
            }
            "lsp_signature" => self.servers.signature(&at(&path()?)?, a.get("line")?, a.get("col")?).and_then(give),
            "edits_write" => self.edits_write(a.get("files")?).and_then(give),
            "lsp_change" => self.servers.change(&at(&path()?)?, &a.get::<String>("text")?).and_then(give),
            "lsp_close" => self.servers.close(&at(&path()?)?).and_then(give),
            "lsp_hover" => self.servers.hover(&at(&path()?)?, a.get("line")?, a.get("col")?).and_then(give),
            "lsp_definition" => {
                let root = self.root()?;
                let found = self.servers.definition(&root::full(&root, &path()?), a.get("line")?, a.get("col")?)?;
                give(
                    found
                        .into_iter()
                        .map(|mut one| {
                            one.path = tree_path(&root, &one.path);
                            one
                        })
                        .collect::<Vec<_>>(),
                )
            }
            "lsp_complete" => self.servers.complete(&at(&path()?)?, a.get("line")?, a.get("col")?).and_then(give),
            "debug_start" => self.debug_start(a.get("start")?, a.get("options")?).and_then(give),
            "debug_sessions" => give(self.debugger.sessions()),
            "debug_breakpoints" => give(self.debugger.breakpoints(&self.root()?, &path()?, &a.get::<Vec<debug::Breakpoint>>("breakpoints")?)),
            "debug_exceptions" => self.debugger.exceptions(a.get("session")?, &a.get::<Vec<String>>("on")?).and_then(give),
            "debug_raised" => self.debugger.raised(a.get("session")?, a.get("thread")?).and_then(give),
            "debug_threads" => self.debugger.threads(a.get("session")?).and_then(give),
            "debug_stack" => {
                let root = self.root()?;
                give(
                    self.debugger
                        .stack(a.get("session")?, a.get("thread")?)?
                        .into_iter()
                        .map(|mut frame| {
                            frame.path = frame.path.map(|path| tree_path(&root, &path));
                            frame
                        })
                        .collect::<Vec<_>>(),
                )
            }
            "debug_scopes" => self.debugger.scopes(a.get("session")?, a.get("frame")?).and_then(give),
            "debug_variables" => self.debugger.variables(a.get("session")?, a.get("reference")?).and_then(give),
            "debug_evaluate" => self.debugger.evaluate(a.get("session")?, &a.get::<String>("expression")?, a.get("frame")?, &a.get::<String>("context")?).and_then(give),
            "debug_step" => self.debugger.step(a.get("session")?, &a.get::<String>("how")?, a.get("thread")?).and_then(give),
            "debug_memory" => self.debugger.memory(a.get("session")?, &a.get::<String>("reference")?, a.get("offset")?, a.get("count")?).and_then(give),
            "debug_instructions" => self.debugger.instructions(a.get("session")?, &a.get::<String>("reference")?, a.get("offset")?, a.get("count")?).and_then(give),
            "debug_bytecode" => {
                let root = self.root()?;
                debug::bytecode(&root, &root::full(&root, &path()?), a.get("line")?).and_then(give)
            }
            "debug_watch_data" => self.debugger.watch_data(a.get("session")?, &a.get::<String>("name")?, a.get("reference")?, a.get("bytes")?).and_then(give),
            "debug_history" => {
                let root = self.root()?;
                give(
                    self.debugger
                        .history(a.get("session")?, a.get("frame")?, &a.get::<String>("name")?)?
                        .into_iter()
                        .map(|mut change| {
                            change.path = tree_path(&root, &change.path);
                            change
                        })
                        .collect::<Vec<_>>(),
                )
            }
            "debug_step_across" => self.debugger.step_across(&self.root()?, a.get("session")?, a.get("thread")?).and_then(give),
            "debug_goto" => self.debugger.goto(a.get("session")?, a.get("step")?).and_then(give),
            "debug_processes" => give(debug::processes()),
            "debug_stop" => {
                match a.get::<Option<u64>>("session")? {
                    Some(session) => self.debugger.stop(session),
                    None => self.debugger.stop_all(),
                }
                give(())
            }
            "validate_file" => {
                let language: String = a.get("language")?;
                let tool = validate::tool_for(&language).ok_or_else(|| format!("no tool plugin validates {language}"))?;
                validate::validate(&tool, &at(&path()?)?).and_then(give)
            }
            "run_file_line" => {
                let root = self.root()?;
                run_file::line_for(&root, &root::full(&root, &path()?), &a.get::<String>("language")?).and_then(give)
            }
            "toolchains_check" => give(Toolchains { tools: toolchains::check(), groups: toolchains::groups(), own: toolchains::own().ok() }),
            "toolchain_add" => toolchains::add(a.get("tool")?).and_then(give),
            "toolchain_add_group" => toolchains::add_group(&a.get::<String>("name")?).and_then(give),
            "toolchain_remove" => toolchains::remove(&a.get::<String>("id")?).and_then(give),
            "toolchain_remove_group" => toolchains::remove_group(&a.get::<String>("name")?).and_then(give),
            "toolchain_version" => toolchains::version(&a.get::<String>("id")?).and_then(give),
            "toolchain_setup" => toolchains::setup_line(&a.get::<String>("id")?).and_then(give),
            "toolchain_add_path" => toolchains::add_to_path(&a.get::<String>("what")?).and_then(give),
            "toolchain_use" => toolchains::choose(&a.get::<String>("id")?, &a.get::<String>("folder")?).and_then(give),
            "toolchain_forget" => toolchains::forget(&a.get::<String>("id")?).and_then(give),
            "repo_init" => {
                let folder = match a.get::<Option<String>>("folder")? {
                    Some(folder) => PathBuf::from(folder),
                    None => self.root()?,
                };
                git::init(&folder)?;
                give(folder.display().to_string())
            }
            "file_create" => files::create_file(&self.root()?, &path()?).and_then(give),
            "folder_create" => files::create_folder(&self.root()?, &path()?).and_then(give),
            "tree_relative" => give(self.tree_relative(path()?)),
            "file_read" => files::read(&self.root()?, &path()?).and_then(give),
            "bridge_read" => give(bridge::read(&self.root()?)),
            "file_window" => files::window(&self.root()?, &path()?, a.get("line")?, a.get("half")?).and_then(give),
            "file_slice" => {
                let slice = files::slice(&self.root()?, &path()?, a.get("start")?, a.get("end")?)?;
                give(json!({"start": slice.start, "end": slice.end, "size": slice.size, "text": slice.text}))
            }
            "file_bytes" => {
                let file = root::inside(&self.root()?, &path()?)?;
                if !file.is_file() {
                    return Err(format!("{} is not a file", path()?));
                }
                std::fs::read(&file).map(|bytes| base64(&bytes)).map_err(|error| error.to_string()).and_then(give)
            }
            "file_write" => self.file_write(a.get("path")?, a.get("text")?).and_then(give),
            "history_list" => give(history::list(&self.root()?, &path()?)),
            "history_read" => history::read(&self.root()?, &path()?, a.get("at")?).and_then(give),
            other => Err(format!("{other} is not a command of the tree's")),
        }
    }
}

/// Answers the window over `input` and `output`: each line of input a call, { id, name, args }, each
/// answered on a thread of its own by a line { id, ok } or { id, error } once it is ready, and what the
/// server tells sent as lines { event, body } as it comes. Ends, with every server and session it
/// started, once the input ends.
pub fn serve(root: Option<PathBuf>, input: impl BufRead, output: impl Write + Send + 'static) {
    let server = Arc::new(Server { keeping: kept_folder(), ..Server::new(root) });
    let output = Arc::new(Mutex::new(output));
    let said = output.clone();
    let send = move |line: Value| {
        if let Ok(mut output) = said.lock() {
            let _ = writeln!(output, "{line}");
            let _ = output.flush();
        }
    };
    let send = Arc::new(send);
    let telling = send.clone();
    server.tell_to(Arc::new(move |event, body| telling(json!({"event": event, "body": body}))));
    server.watch();
    send(json!({"event": "serving", "body": {"version": env!("CARGO_PKG_VERSION"), "root": server.root_now().map(|root| root.display().to_string())}}));
    for line in input.lines().map_while(Result::ok) {
        let Ok(call) = serde_json::from_str::<Value>(&line) else { continue };
        let server = server.clone();
        let send = send.clone();
        std::thread::spawn(move || {
            let id = call["id"].clone();
            let name = call["name"].as_str().unwrap_or("");
            let answer = match server.call(name, call["args"].clone()) {
                Ok(value) => json!({"id": id, "ok": value}),
                Err(error) => json!({"id": id, "error": error}),
            };
            send(answer);
        });
    }
    server.stop_all();
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn base64_round_trips_every_length() {
        for length in 0..40 {
            let bytes: Vec<u8> = (0..length).map(|n| (n * 37 + 11) as u8).collect();
            assert_eq!(unbase64(&base64(&bytes)).unwrap(), bytes, "{length}");
        }
        assert_eq!(base64(b"orior"), "b3Jpb3I=");
    }

    #[test]
    fn a_name_of_more_than_one_word_is_found_as_the_page_writes_it() {
        let args = Args(serde_json::from_str::<Map<String, Value>>(r#"{"withErrors": true, "path": "a"}"#).unwrap());
        assert!(args.get::<bool>("with_errors").unwrap());
        assert_eq!(args.get::<String>("path").unwrap(), "a");
        assert_eq!(args.get::<Option<String>>("repo").unwrap(), None);
    }

    #[test]
    fn calls_over_lines_are_answered_by_id() {
        let dir = std::env::temp_dir().join(format!("orior-serve-{}", std::process::id()));
        std::fs::create_dir_all(dir.join("src/cu/engine")).unwrap();
        std::fs::write(dir.join("src/cu/engine/engine_config.h"), "// marks the tree\n").unwrap();
        std::fs::write(dir.join("a.txt"), "one\n").unwrap();
        let root = dunce::canonicalize(&dir).unwrap();
        let input = format!(
            "{}\n{}\n{}\n",
            json!({"id": 1, "name": "file_read", "args": {"path": "a.txt"}}),
            json!({"id": 2, "name": "nothing_at_all", "args": {}}),
            json!({"id": 3, "name": "file_write", "args": {"path": "b.txt", "text": "two\n"}}),
        );
        let (sender, heard) = std::sync::mpsc::channel::<Vec<u8>>();
        struct Out(std::sync::mpsc::Sender<Vec<u8>>);
        impl Write for Out {
            fn write(&mut self, bytes: &[u8]) -> std::io::Result<usize> {
                let _ = self.0.send(bytes.to_vec());
                Ok(bytes.len())
            }
            fn flush(&mut self) -> std::io::Result<()> {
                Ok(())
            }
        }
        serve(Some(root.clone()), std::io::Cursor::new(input), Out(sender));
        let mut text = String::new();
        let deadline = std::time::Instant::now() + std::time::Duration::from_secs(10);
        while text.matches("\"id\"").count() < 3 && std::time::Instant::now() < deadline {
            if let Ok(bytes) = heard.recv_timeout(std::time::Duration::from_millis(100)) {
                text.push_str(&String::from_utf8_lossy(&bytes));
            }
        }
        let lines: Vec<Value> = text.lines().filter_map(|line| serde_json::from_str(line).ok()).collect();
        let by_id = |id: u64| lines.iter().find(|line| line["id"] == id).cloned().unwrap_or(Value::Null);
        assert_eq!(lines[0]["event"], "serving");
        assert!(by_id(1)["ok"]["text"].as_str().unwrap_or("").starts_with("one"), "{text}");
        assert!(by_id(2)["error"].as_str().unwrap_or("").contains("nothing_at_all"), "{text}");
        assert_eq!(by_id(3)["ok"], Value::Null);
        assert_eq!(std::fs::read_to_string(root.join("b.txt")).unwrap(), "two\n");
        let _ = std::fs::remove_dir_all(&dir);
    }
}

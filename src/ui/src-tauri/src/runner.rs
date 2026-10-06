// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Starting a job, carrying its output to the window line by line, and stopping it.
//!
//! A job's steps run one after another in the tree's top folder, and the first that exits other than
//! 0 ends the job. Every line either stream writes reaches the window as a `run-line` event, and the
//! end as one `run-end` carrying the exit code and the pages the job wrote.

use std::collections::{HashMap, HashSet};
use std::io::{BufRead, BufReader, Read};
use std::path::{Path, PathBuf};
use std::process::{Child, Command, Stdio};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;

use serde::Serialize;
use tauri::{AppHandle, Emitter};

use crate::catalog::{Arg, Job, Program, Step};
use crate::root::relative;

/// The runs this app has started: the process of each step still running, and the runs a reader has
/// stopped. Each is shared with the threads that wait on the runs.
#[derive(Default)]
pub struct Runs {
    next: AtomicU64,
    live: Arc<Mutex<HashMap<u64, u32>>>,
    stopped: Arc<Mutex<HashSet<u64>>>,
}

fn holds(stopped: &Mutex<HashSet<u64>>, run: u64) -> bool {
    stopped.lock().map(|s| s.contains(&run)).unwrap_or(false)
}

#[derive(Clone, Serialize)]
struct Line {
    run: u64,
    stream: &'static str,
    text: String,
}

#[derive(Clone, Serialize)]
struct End {
    run: u64,
    code: Option<i32>,
    stopped: bool,
    views: Vec<String>,
}

/// The bash that runs the tree's scripts. On Windows that is Git's, found beside git itself, because
/// the bash System32 offers is WSL's and reads none of these paths. ORIOR_BASH overrides it anywhere.
pub fn bash() -> Result<PathBuf, String> {
    if let Ok(named) = std::env::var("ORIOR_BASH") {
        return Ok(PathBuf::from(named));
    }
    if !cfg!(windows) {
        return Ok(PathBuf::from("bash"));
    }
    let mut places = Vec::new();
    if let Some(path) = std::env::var_os("PATH") {
        for dir in std::env::split_paths(&path) {
            if dir.join("git.exe").is_file() {
                if let Some(git) = dir.parent() {
                    places.push(git.join("bin").join("bash.exe"));
                    places.push(git.join("usr").join("bin").join("bash.exe"));
                }
            }
        }
    }
    for base in ["ProgramFiles", "ProgramW6432", "LOCALAPPDATA"] {
        if let Some(dir) = std::env::var_os(base) {
            places.push(PathBuf::from(&dir).join("Git").join("bin").join("bash.exe"));
            places.push(PathBuf::from(&dir).join("Programs").join("Git").join("bin").join("bash.exe"));
        }
    }
    places.into_iter().find(|p| p.is_file()).ok_or_else(|| {
        "no Git bash found: install Git for Windows, or name a bash in ORIOR_BASH".to_string()
    })
}

/// The Python that runs the tree's scripts: ORIOR_PYTHON, else python on Windows and python3 elsewhere.
pub fn python() -> PathBuf {
    if let Ok(named) = std::env::var("ORIOR_PYTHON") {
        return PathBuf::from(named);
    }
    PathBuf::from(if cfg!(windows) { "python" } else { "python3" })
}

/// The newest copy of a built program under the folders the build scripts publish to.
pub fn built(root: &Path, name: &str) -> Result<PathBuf, String> {
    let file = if cfg!(windows) { format!("{name}.exe") } else { name.to_string() };
    ["build", "examples/build", "src/build"]
        .iter()
        .map(|dir| root.join(dir).join(&file))
        .filter(|path| path.is_file())
        .max_by_key(|path| path.metadata().and_then(|m| m.modified()).ok())
        .ok_or_else(|| format!("no {file} under build/, examples/build/ or src/build/: run its build job first"))
}

/// Splits text into words the way a shell does for plain words, single quotes and double quotes.
pub fn shell_words(text: &str) -> Result<Vec<String>, String> {
    let mut words = Vec::new();
    let mut word = String::new();
    let mut started = false;
    let mut quote: Option<char> = None;
    let mut chars = text.chars();
    while let Some(c) = chars.next() {
        match (quote, c) {
            (Some(q), c) if c == q => quote = None,
            (Some('"'), '\\') => match chars.next() {
                Some(next) => word.push(next),
                None => word.push('\\'),
            },
            (Some(_), c) => word.push(c),
            (None, '\'' | '"') => {
                quote = Some(c);
                started = true;
            }
            (None, c) if c.is_whitespace() => {
                if started {
                    words.push(std::mem::take(&mut word));
                    started = false;
                }
            }
            (None, c) => {
                word.push(c);
                started = true;
            }
        }
    }
    if quote.is_some() {
        return Err(format!("a quote is left open in: {text}"));
    }
    if started {
        words.push(word);
    }
    Ok(words)
}

/// A path given to a bash script, written with forward slashes, which Git's bash reads on Windows
/// and every other bash reads already.
fn for_bash(value: &str) -> String {
    if cfg!(windows) {
        value.replace('\\', "/")
    } else {
        value.to_string()
    }
}

fn arguments(step: &Step, values: &HashMap<String, Vec<String>>) -> Result<Vec<String>, String> {
    let first = |key: &str| values.get(key).and_then(|v| v.first()).filter(|v| !v.is_empty()).cloned();
    let mut out = Vec::new();
    for arg in &step.args {
        match arg {
            Arg::Lit(text) => out.push(text.clone()),
            Arg::Value(key) => out.extend(first(key)),
            Arg::Flag(flag, key) => {
                if let Some(value) = first(key) {
                    out.push(flag.to_string());
                    out.push(value);
                }
            }
            Arg::Each(flag, key) => {
                for value in values.get(key).into_iter().flatten().filter(|v| !v.is_empty()) {
                    out.push(flag.to_string());
                    out.push(value.clone());
                }
            }
            Arg::Words(key, separated) => {
                let said = shell_words(&first(key).unwrap_or_default())?;
                if *separated && !said.is_empty() {
                    out.push("--".into());
                }
                out.extend(said);
            }
        }
    }
    Ok(out)
}

/// The command a step runs, with the line the window shows for it.
fn command(root: &Path, step: &Step, values: &HashMap<String, Vec<String>>) -> Result<(Command, String), String> {
    let args = arguments(step, values)?;
    let (program, mut full) = match &step.program {
        Program::Bash(script) => {
            let args: Vec<String> = args.iter().map(|a| for_bash(a)).collect();
            (bash()?, std::iter::once(script.clone()).chain(args).collect::<Vec<_>>())
        }
        Program::Python(script) => (python(), std::iter::once(script.clone()).chain(args).collect()),
        Program::Built(name) => (built(root, name)?, args),
    };
    let mut cmd = Command::new(&program);
    if let Program::Python(_) = step.program {
        cmd.arg("-u");
    }
    cmd.args(&full).current_dir(root);
    let shown_program = match &step.program {
        Program::Bash(_) => "bash".to_string(),
        Program::Python(_) => "python".to_string(),
        Program::Built(_) => relative(root, &program),
    };
    full.insert(0, shown_program);
    let shown = full.iter().map(|w| if w.contains(' ') { format!("\"{w}\"") } else { w.clone() }).collect::<Vec<_>>();
    Ok((cmd, shown.join(" ")))
}

pub(crate) fn quiet(cmd: &mut Command) {
    #[cfg(windows)]
    {
        use std::os::windows::process::CommandExt;
        const CREATE_NO_WINDOW: u32 = 0x0800_0000;
        cmd.creation_flags(CREATE_NO_WINDOW);
    }
    #[cfg(unix)]
    {
        use std::os::unix::process::CommandExt;
        cmd.process_group(0);
    }
}

fn carry<R: Read + Send + 'static>(app: AppHandle, run: u64, stream: &'static str, from: R, seen: Arc<Mutex<Vec<String>>>) -> thread::JoinHandle<()> {
    thread::spawn(move || {
        let mut reader = BufReader::new(from);
        let mut bytes = Vec::new();
        loop {
            bytes.clear();
            match reader.read_until(b'\n', &mut bytes) {
                Ok(0) | Err(_) => break,
                Ok(_) => {
                    let text = String::from_utf8_lossy(&bytes).trim_end_matches(['\r', '\n']).to_string();
                    if let Ok(mut seen) = seen.lock() {
                        seen.push(text.clone());
                    }
                    let _ = app.emit("run-line", Line { run, stream, text });
                }
            }
        }
    })
}

/// The pages a job wrote: every .html file in its view folder, and every .html path its output names
/// that is a file.
fn pages(root: &Path, view_out: &Path, lines: &[String]) -> Vec<String> {
    let mut found: Vec<PathBuf> = Vec::new();
    if let Ok(entries) = std::fs::read_dir(view_out) {
        for entry in entries.flatten() {
            if entry.path().extension().is_some_and(|e| e == "html") {
                found.push(entry.path());
            }
        }
    }
    for line in lines {
        for word in line.split_whitespace() {
            let word = word.trim_matches(|c: char| c == '"' || c == '\'' || c == ',' || c == ':');
            if word.ends_with(".html") {
                let path = PathBuf::from(word);
                let path = if path.is_absolute() { path } else { root.join(path) };
                if path.is_file() {
                    found.push(path);
                }
            }
        }
    }
    let mut named: Vec<String> = found
        .into_iter()
        .filter_map(|p| dunce::canonicalize(p).ok())
        .filter(|p| p.starts_with(root))
        .map(|p| relative(root, &p))
        .collect();
    named.sort();
    named.dedup();
    named
}

impl Runs {
    pub fn start(&self, app: AppHandle, root: PathBuf, job: Job, values: HashMap<String, Vec<String>>) -> Result<u64, String> {
        for param in &job.params {
            let given = values.get(&param.key).is_some_and(|v| v.iter().any(|v| !v.is_empty()));
            if param.required && !given {
                return Err(format!("{} needs a value", param.key));
            }
            if param.kind == "choice" || param.kind == "many" {
                for value in values.get(&param.key).into_iter().flatten().filter(|v| !v.is_empty()) {
                    if !param.choices.contains(value) {
                        return Err(format!("{value} is not one of {}'s values", param.key));
                    }
                }
            }
        }
        let mut commands = Vec::new();
        for step in &job.steps {
            commands.push(command(&root, step, &values)?);
        }
        let run = self.next.fetch_add(1, Ordering::SeqCst) + 1;
        let view_out = root.join("build").join("ui").join("views").join(run.to_string());
        let live = self.live.clone();
        let stopped = self.stopped.clone();
        thread::spawn(move || {
            let seen = Arc::new(Mutex::new(Vec::new()));
            let mut code = Some(0);
            for (mut cmd, shown) in commands {
                if holds(&stopped, run) {
                    break;
                }
                let _ = app.emit("run-line", Line { run, stream: "command", text: shown });
                if job.opens == "views" {
                    let _ = std::fs::create_dir_all(&view_out);
                    cmd.env("VIEW_OUT", &view_out);
                }
                cmd.env("PYTHONUNBUFFERED", "1").env("PYTHONIOENCODING", "utf-8");
                cmd.stdin(Stdio::null()).stdout(Stdio::piped()).stderr(Stdio::piped());
                quiet(&mut cmd);
                let mut child: Child = match cmd.spawn() {
                    Ok(child) => child,
                    Err(error) => {
                        let _ = app.emit("run-line", Line { run, stream: "stderr", text: format!("could not start: {error}") });
                        code = None;
                        break;
                    }
                };
                if let Ok(mut live) = live.lock() {
                    live.insert(run, child.id());
                }
                let out = child.stdout.take().map(|s| carry(app.clone(), run, "stdout", s, seen.clone()));
                let err = child.stderr.take().map(|s| carry(app.clone(), run, "stderr", s, seen.clone()));
                let status = child.wait();
                for reader in [out, err].into_iter().flatten() {
                    let _ = reader.join();
                }
                if let Ok(mut live) = live.lock() {
                    live.remove(&run);
                }
                code = status.ok().and_then(|s| s.code());
                if code != Some(0) {
                    break;
                }
            }
            let lines = seen.lock().map(|s| s.clone()).unwrap_or_default();
            let views = if job.opens == "views" { pages(&root, &view_out, &lines) } else { Vec::new() };
            let _ = app.emit("run-end", End { run, code, stopped: holds(&stopped, run), views });
        });
        Ok(run)
    }

    /// Stops a run and everything it started: the whole process tree on Windows, the process group
    /// elsewhere.
    pub fn stop(&self, run: u64) -> Result<(), String> {
        self.stopped.lock().map_err(|e| e.to_string())?.insert(run);
        let pid = self.live.lock().map_err(|e| e.to_string())?.get(&run).copied();
        let Some(pid) = pid else { return Ok(()) };
        let mut kill = if cfg!(windows) {
            let mut kill = Command::new("taskkill");
            kill.args(["/T", "/F", "/PID", &pid.to_string()]);
            kill
        } else {
            let mut kill = Command::new("kill");
            kill.args(["-TERM", &format!("-{pid}")]);
            kill
        };
        quiet(&mut kill);
        kill.stdout(Stdio::null()).stderr(Stdio::null());
        kill.status().map(|_| ()).map_err(|e| e.to_string())
    }
}

#[cfg(test)]
mod splitting {
    use super::shell_words;

    #[test]
    fn words_quotes_and_escapes() {
        assert_eq!(shell_words("a  'b c' \"d \\\"e\\\"\"").unwrap(), vec!["a", "b c", "d \"e\""]);
        assert_eq!(shell_words("").unwrap(), Vec::<String>::new());
        assert_eq!(shell_words("''").unwrap(), vec![""]);
        assert!(shell_words("'open").is_err());
    }
}

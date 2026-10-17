// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Starting a job, carrying its output line by line, and stopping it.
//!
//! A job's steps run one after another in the tree's top folder, and the first that exits other than
//! 0 ends the job. Every line either stream writes goes to the run's sink as it comes, and then the
//! end, carrying the exit code and the pages the job wrote. Each line and the end carry the
//! milliseconds since the run started, to the microsecond, which the window's time ruler reads. The
//! window's sink sends them to the page; the command line's prints them.

use std::collections::{HashMap, HashSet};
use std::io::{BufRead, BufReader, Read};
use std::path::{Path, PathBuf};
use std::process::{Child, Command, Stdio};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Instant;

use serde::Serialize;

use crate::catalog::{Arg, Job, Program, Step};
use crate::root::relative;

/// The runs started: the process of each step still running, and the runs a reader has stopped.
/// Each is shared with the threads that wait on the runs.
///
/// The window's runs are detached: their steps get no console of their own on Windows and a process
/// group of their own elsewhere, and only Stop ends them. The command line's are attached: they share
/// its console, and the key that stops it stops them.
#[derive(Default)]
pub struct Runs {
    next: AtomicU64,
    live: Arc<Mutex<HashMap<u64, u32>>>,
    /// The group of each detached step still running, which Stop ends whole.
    groups: Arc<Mutex<HashMap<u64, crate::container::Group>>>,
    stopped: Arc<Mutex<HashSet<u64>>>,
    attached: bool,
}

fn holds(stopped: &Mutex<HashSet<u64>>, run: u64) -> bool {
    stopped.lock().map(|s| s.contains(&run)).unwrap_or(false)
}

#[derive(Clone, Serialize)]
pub struct Line {
    pub run: u64,
    pub stream: &'static str,
    pub text: String,
    pub ms: f64,
}

#[derive(Clone, Serialize)]
pub struct End {
    pub run: u64,
    pub code: Option<i32>,
    pub stopped: bool,
    pub views: Vec<String>,
    /// The program a job whose `opens` is "program" built, by its path in the tree where it is in
    /// the tree, where the build made it.
    pub made: Option<String>,
    pub ms: f64,
}

/// A run's start, said before its first line: its number and its job, so that whoever hears the run
/// knows it before anything it says.
#[derive(Clone, Debug, PartialEq, Serialize)]
pub struct Start {
    pub run: u64,
    pub job: String,
}

/// The variable a job'"'"'s step finds the run'"'"'s number in: set on every step a run starts, and read by a
/// command that would otherwise reach a window open on the tree.
pub const RUN_VARIABLE: &str = "ORIOR_RUN";

/// What a run says as it goes: its start, each line, then its end, in that order.
pub enum Said {
    Start(Start),
    Line(Line),
    Blob(Blob),
    End(End),
}

/// A blob a run's writer placed, as it says it on the job's output (qry_blob_said in the run's buffer): its sequence,
/// the time it was handed, its priority, the member whose ring it came from, its size and its name, and when the
/// line came.
#[derive(Clone, Serialize, Debug, PartialEq)]
pub struct Blob {
    pub run: u64,
    pub sequence: u64,
    pub nanoseconds: u64,
    pub priority: u32,
    pub member: u32,
    pub size: u64,
    pub name: String,
    pub ms: f64,
}

impl Blob {
    /// The writer's line read apart, `qry_blob <sequence> <nanoseconds> <priority> <member> <size> <name>`; None
    /// where the line is no such line.
    pub fn read(run: u64, text: &str, ms: f64) -> Option<Blob> {
        let mut words = text.strip_prefix("qry_blob ")?.split(' ');
        let mut number = || words.next()?.parse::<u64>().ok();
        let (sequence, nanoseconds, priority, member, size) = (number()?, number()?, number()?, number()?, number()?);
        let name = words.next()?.to_string();
        if name.is_empty() || words.next().is_some() {
            return None;
        }
        Some(Blob { run, sequence, nanoseconds, priority: u32::try_from(priority).ok()?, member: u32::try_from(member).ok()?, size, name, ms })
    }

    /// 1 where the protocol panel reads this blob by itself (the page's protocol.js): a panic or a critical blob,
    /// a round's list of questions, an answer, a carried file, or a file of the part's table. Every other blob the
    /// panel only counts.
    pub fn panel_reads(&self) -> bool {
        let answer = self.name.strip_prefix("answers").and_then(|rest| rest.strip_suffix(".txt")).is_some_and(|number| number.bytes().all(|b| b.is_ascii_digit()));
        let table = [".ksc", ".khw", ".krs", ".kqr"].iter().any(|kind| self.name.ends_with(kind));
        self.priority < 2 || answer || table || ["questions.txt", "forms_work.txt", "faulted.txt", "overseer.txt"].contains(&self.name.as_str())
    }
}

/// The milliseconds since `began`.
fn since(began: Instant) -> f64 {
    began.elapsed().as_secs_f64() * 1000.0
}

/// Where a run's lines and end go.
pub type Sink = Arc<dyn Fn(Said) + Send + Sync>;

/// The bash that runs the tree's scripts. On Windows that is Git's, found beside git itself, because
/// the bash System32 offers is WSL's and reads none of these paths. ORIOR_BASH overrides it anywhere,
/// and a folder given for bash under File, Toolchains overrides the rest.
pub fn bash() -> Result<PathBuf, String> {
    if let Ok(named) = std::env::var("ORIOR_BASH") {
        return Ok(PathBuf::from(named));
    }
    if let Some(chosen) = crate::toolchains::chosen_program("bash") {
        return Ok(chosen);
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

/// The Python that runs the tree's scripts: ORIOR_PYTHON, else the one in the folder given for Python
/// under File, Toolchains, else python on Windows and python3 elsewhere.
pub fn python() -> PathBuf {
    if let Ok(named) = std::env::var("ORIOR_PYTHON") {
        return PathBuf::from(named);
    }
    if let Some(chosen) = crate::toolchains::chosen_program("python") {
        return chosen;
    }
    PathBuf::from(if cfg!(windows) { "python" } else { "python3" })
}

/// A built program under build/, the folder the build scripts publish to.
pub fn built(root: &Path, name: &str) -> Result<PathBuf, String> {
    let file = if cfg!(windows) { format!("{name}.exe") } else { name.to_string() };
    let path = root.join("build").join(&file);
    if path.is_file() { Ok(path) } else { Err(format!("no {file} under build/: run its build job first")) }
}

/// The program and its words an environment variable names: its text as one word where it is a file,
/// as a Windows path with its backslashes is, and else its words as a shell splits them. None where it
/// is not set or names nothing.
pub fn program_words(variable: &str) -> Option<Vec<String>> {
    let text = std::env::var(variable).ok()?;
    let text = text.trim();
    if Path::new(text).is_file() {
        return Some(vec![text.to_string()]);
    }
    shell_words(text).ok().filter(|words| !words.is_empty())
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
            Arg::Env(_) => {}
            Arg::Around(before, key, after) => out.extend(first(key).map(|value| format!("{before}{value}{after}"))),
            Arg::When(key, value, words) => {
                if first(key).as_deref() == Some(value.as_str()) {
                    out.extend(words.iter().cloned());
                }
            }
            Arg::Format(text) => {
                let mut filled = text.clone();
                for (key, given) in values {
                    filled = filled.replace(&format!("{{{key}}}"), given.first().map(String::as_str).unwrap_or_default());
                }
                out.push(filled);
            }
            Arg::FlagBut(flag, key, but) => {
                if let Some(value) = first(key).filter(|value| value != but) {
                    out.push(flag.to_string());
                    out.push(value);
                }
            }
            Arg::Set(..) | Arg::SetWhen(..) | Arg::Unpath(_) | Arg::Only(..) | Arg::Given(_) | Arg::SetFrom(..) | Arg::EnvBut(..) | Arg::Cross(_) | Arg::SourceDate | Arg::Folder(_) => {}
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
        Program::Tool { tool, program } => (crate::executables::program(tool, program)?, args),
        Program::Orior => (std::env::current_exe().map_err(|error| format!("orior itself: {error}"))?, args),
        Program::Made { .. } => (PathBuf::from("."), args),
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
        Program::Tool { program, .. } => program.clone(),
        Program::Orior => "orior".to_string(),
        Program::Made { system: false } => "<the program the build made>".to_string(),
        Program::Made { system: true } => "PATH=<the system's PATH> <the program the build made>".to_string(),
    };
    full.insert(0, shown_program);
    // each setting given goes into the step's environment, and the line shown leads with it as a
    // shell writes it
    let mut set = Vec::new();
    let mut moved: Option<String> = None;
    for arg in &step.args {
        if let Arg::Env(key) = arg {
            if let Some(value) = values.get(key).and_then(|v| v.first()).filter(|v| !v.is_empty()) {
                let value = if matches!(step.program, Program::Bash(_)) { for_bash(value) } else { value.clone() };
                cmd.env(key, &value);
                set.push(format!("{key}={value}"));
            }
        }
        if let Arg::Set(key, value) = arg {
            cmd.env(key, value);
            set.push(format!("{key}={value}"));
        }
        if let Arg::SetFrom(variable, key) = arg {
            if let Some(value) = values.get(key).and_then(|v| v.first()).filter(|v| !v.is_empty()) {
                cmd.env(variable, value);
                set.push(format!("{variable}={value}"));
            }
        }
        if let Arg::SetWhen(param, given, key, value) = arg {
            if values.get(param).and_then(|v| v.first()) == Some(given) {
                cmd.env(key, value);
                set.push(format!("{key}={value}"));
            }
        }
        if let Arg::EnvBut(key, but) = arg {
            if let Some(value) = values.get(key).and_then(|v| v.first()).filter(|v| !v.is_empty() && *v != but) {
                cmd.env(key, value);
                set.push(format!("{key}={value}"));
            }
        }
        if let Arg::Folder(folder) = arg {
            cmd.current_dir(crate::root::full(root, folder));
            moved = Some(format!("cd {folder} &&"));
        }
        if matches!(arg, Arg::SourceDate) && std::env::var_os("SOURCE_DATE_EPOCH").is_none() {
            if let Some((_, when, _)) = crate::git::head(root) {
                cmd.env("SOURCE_DATE_EPOCH", when.to_string());
                set.push(format!("SOURCE_DATE_EPOCH={when}"));
            }
        }
        if let Arg::Cross(kind) = arg {
            for (key, value) in crate::executables::cross_env(kind, values) {
                set.push(format!("{key}={value}"));
                cmd.env(key, value);
            }
        }
        if let Arg::Unpath(name) = arg {
            let path = crate::toolchains::run_path();
            let kept: Vec<PathBuf> = std::env::split_paths(&path).filter(|dir| crate::toolchains::program_in(dir, std::slice::from_ref(name)).is_none()).collect();
            cmd.env("PATH", std::env::join_paths(kept).unwrap_or(path));
            set.push(format!("PATH=<PATH without {name}>"));
        }
    }
    // the tree's own tools come first on the PATH every step runs with, and the line says so
    let tools: Vec<String> = crate::toolchains::tree_tools().iter().map(|folder| relative(root, folder)).collect();
    if !tools.is_empty() && !step.args.iter().any(|arg| matches!(arg, Arg::Unpath(_)) || matches!(arg, Arg::Set(key, _) if key == "PATH")) {
        set.insert(0, format!("PATH={}:$PATH", tools.join(":")));
    }
    let shown = moved.into_iter().chain(set.into_iter().chain(full).map(|w| if w.contains(' ') { format!("\"{w}\"") } else { w })).collect::<Vec<_>>();
    Ok((cmd, shown.join(" ")))
}

/// A step as a run holds it: its command made; the program the build made, found as it starts; the
/// signing of that program, which becomes the commands that sign it; and what is said of its
/// signature once they have run.
enum Ready {
    Now(Command, String),
    Made { system: bool },
    Sign,
    Signed(PathBuf),
}

/// Starts `cmd` with no console window of its own on Windows, and in a process group of its own
/// elsewhere, where a signal to the window's group does not reach it.
pub fn quiet(cmd: &mut Command) {
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

fn carry<R: Read + Send + 'static>(sink: Sink, run: u64, stream: &'static str, from: R, seen: Arc<Mutex<Vec<String>>>, began: Instant, first: Arc<Mutex<Option<f64>>>) -> thread::JoinHandle<()> {
    thread::spawn(move || {
        let mut reader = BufReader::new(from);
        let mut bytes = Vec::new();
        loop {
            bytes.clear();
            match reader.read_until(b'\n', &mut bytes) {
                Ok(0) | Err(_) => break,
                Ok(_) => {
                    let text = String::from_utf8_lossy(&bytes).trim_end_matches(['\r', '\n']).to_string();
                    // a blob the run's writer placed is the panel's, and no line of the console
                    if let Some(blob) = Blob::read(run, &text, since(began)) {
                        sink(Said::Blob(blob));
                        continue;
                    }
                    if let Ok(mut seen) = seen.lock() {
                        seen.push(text.clone());
                    }
                    let ms = since(began);
                    if let Ok(mut first) = first.lock() {
                        first.get_or_insert(ms);
                    }
                    sink(Said::Line(Line { run, stream, text, ms }));
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

/// Says why `values` do not do for `job`: a value it needs not given, or one given that is not among
/// a choice's values.
pub fn check(job: &Job, values: &HashMap<String, Vec<String>>) -> Result<(), String> {
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
    Ok(())
}

/// The commands a job's steps run with these values, each as the line a run shows for it.
pub fn shown(root: &Path, job: &Job, values: &HashMap<String, Vec<String>>) -> Result<Vec<String>, String> {
    job.steps.iter().filter(|step| runs(step, values)).map(|step| command(root, step, values).map(|(_, shown)| shown)).collect()
}

/// Whether a step runs with these values: every param an Only names holds the value it names, and
/// every param a Given names holds a value.
fn runs(step: &Step, values: &HashMap<String, Vec<String>>) -> bool {
    step.args.iter().all(|arg| match arg {
        Arg::Only(key, value) => values.get(key).and_then(|given| given.first()) == Some(value),
        Arg::Given(key) => values.get(key).and_then(|given| given.first()).is_some_and(|value| !value.is_empty()),
        _ => true,
    })
}

impl Runs {
    /// Runs whose steps share this process's console.
    pub fn attached() -> Self {
        Runs { attached: true, ..Runs::default() }
    }

    /// Starts a job. Its steps run on a thread of their own, which the returned handle waits on.
    ///
    /// A step whose program is the program the build made is found as it starts, from the lines the
    /// steps before it printed; the file is found once, and what it loads is said as it is found.
    pub fn start(&self, sink: Sink, root: PathBuf, job: Job, values: HashMap<String, Vec<String>>) -> Result<(u64, thread::JoinHandle<()>), String> {
        check(&job, &values)?;
        let mut commands = Vec::new();
        for step in job.steps.iter().filter(|step| runs(step, &values)) {
            commands.push(match step.program {
                Program::Made { system } => Ready::Made { system },
                _ => {
                    let (cmd, shown) = command(&root, step, &values)?;
                    Ready::Now(cmd, shown)
                }
            });
        }
        if job.opens == "program" && crate::executables::signs() {
            let at = commands.iter().position(|ready| matches!(ready, Ready::Made { .. })).unwrap_or(commands.len());
            commands.insert(at, Ready::Sign);
        }
        let run = self.next.fetch_add(1, Ordering::SeqCst) + 1;
        // the run's start said before any line of it, so that a hearer knows the run and its job first
        sink(Said::Start(Start { run, job: job.id.clone() }));
        // Run numbers start again in each process, and the window and the command line can run at
        // once. The folder is named by both and emptied before the run writes to it.
        let view_out = root.join("build").join("ui").join("views").join(format!("{}-{run}", std::process::id()));
        let live = self.live.clone();
        let groups = self.groups.clone();
        let stopped = self.stopped.clone();
        let attached = self.attached;
        let began = Instant::now();
        let waits = thread::spawn(move || {
            if job.opens == "views" {
                let _ = std::fs::remove_dir_all(&view_out);
            }
            let seen = Arc::new(Mutex::new(Vec::new()));
            let mut code = Some(0);
            let mut built = true;
            let run_path = crate::toolchains::run_path();
            let say = |text: String| sink(Said::Line(Line { run, stream: "stderr", text, ms: since(began) }));
            let mut made_file: Option<Result<PathBuf, String>> = None;
            let mut find_made = |lines: &[String]| -> Result<PathBuf, String> {
                if made_file.is_none() {
                    let found = crate::executables::made(&root, &job.id, &values, lines);
                    match &found {
                        Ok(file) => {
                            let orior_path: Vec<PathBuf> = std::env::split_paths(&run_path).collect();
                            crate::binary::read(file, &orior_path).as_ref().map(crate::binary::said).unwrap_or_default().into_iter().for_each(&say);
                            crate::executables::built_advice(&root, &job.id, lines).into_iter().for_each(&say);
                            crate::executables::compare_build(&root, &job.id, file).into_iter().for_each(&say);
                        }
                        Err(error) => say(error.clone()),
                    }
                    made_file = Some(found);
                }
                made_file.clone().unwrap_or_else(|| Err("no file was made".to_string()))
            };
            let mut queue: std::collections::VecDeque<Ready> = commands.into();
            while let Some(ready) = queue.pop_front() {
                if holds(&stopped, run) {
                    break;
                }
                let made_step = matches!(ready, Ready::Made { .. });
                let (mut cmd, shown) = match ready {
                    Ready::Now(cmd, shown) => (cmd, shown),
                    Ready::Sign => {
                        let lines = seen.lock().map(|s| s.clone()).unwrap_or_default();
                        let steps = find_made(&lines).and_then(|file| crate::executables::sign_steps(&file).map(|steps| (file, steps)));
                        match steps {
                            Ok((file, steps)) => {
                                queue.push_front(Ready::Signed(file));
                                for (cmd, shown) in steps.into_iter().rev() {
                                    queue.push_front(Ready::Now(cmd, shown));
                                }
                                continue;
                            }
                            Err(error) => {
                                say(error);
                                code = None;
                                built = false;
                                break;
                            }
                        }
                    }
                    Ready::Signed(file) => {
                        crate::executables::signed_said(&file).into_iter().for_each(&say);
                        continue;
                    }
                    Ready::Made { system } => {
                        let lines = seen.lock().map(|s| s.clone()).unwrap_or_default();
                        match find_made(&lines) {
                            Ok(file) => crate::executables::run_command(&root, &file, &values, system),
                            Err(_) => {
                                code = None;
                                break;
                            }
                        }
                    }
                };
                sink(Said::Line(Line { run, stream: "command", text: shown, ms: since(began) }));
                if job.opens == "views" {
                    let _ = std::fs::create_dir_all(&view_out);
                    cmd.env("VIEW_OUT", &view_out);
                }
                cmd.env("PYTHONUNBUFFERED", "1").env("PYTHONIOENCODING", "utf-8");
                if !cmd.get_envs().any(|(key, _)| key == "PATH") {
                    cmd.env("PATH", &run_path);
                }
                cmd.stdin(Stdio::null()).stdout(Stdio::piped()).stderr(Stdio::piped());
                if !attached {
                    quiet(&mut cmd);
                }
                let step_began = since(began);
                // The groups are held from the last look at whether the run was stopped until the step's
                // process has joined its group: Stop, which marks the run and then reads the groups,
                // either finds the process there or keeps it from starting.
                let held = groups.lock();
                if holds(&stopped, run) {
                    break;
                }
                // the run's number in the step's environment: a command that would reach a window open on the
                // tree sees it is a job's own and runs where it is
                cmd.env(RUN_VARIABLE, run.to_string());
                let spawned = if attached { cmd.spawn().map(|child| (child, None)) } else { crate::container::Group::spawn(&mut cmd) };
                let mut child: Child = match spawned {
                    Ok((child, group)) => {
                        if let (Some(group), Ok(mut groups)) = (group, held) {
                            groups.insert(run, group);
                        }
                        child
                    }
                    Err(error) => {
                        say(format!("could not start: {error}"));
                        code = None;
                        built &= made_step;
                        break;
                    }
                };
                if let Ok(mut live) = live.lock() {
                    live.insert(run, child.id());
                }
                let first: Arc<Mutex<Option<f64>>> = Arc::default();
                let out = child.stdout.take().map(|s| carry(sink.clone(), run, "stdout", s, seen.clone(), began, first.clone()));
                let err = child.stderr.take().map(|s| carry(sink.clone(), run, "stderr", s, seen.clone(), began, first.clone()));
                let status = child.wait();
                for reader in [out, err].into_iter().flatten() {
                    let _ = reader.join();
                }
                if let Ok(mut live) = live.lock() {
                    live.remove(&run);
                }
                if let Ok(mut groups) = groups.lock() {
                    groups.remove(&run);
                }
                code = status.ok().and_then(|s| s.code());
                if made_step {
                    if let Some(first) = first.lock().ok().and_then(|first| *first) {
                        say(format!("the program wrote its first line {:.0} ms after it started", first - step_began));
                    }
                    let lines = seen.lock().map(|s| s.clone()).unwrap_or_default();
                    crate::executables::ran_advice(&root, &job.id, code, &lines).into_iter().for_each(&say);
                } else if code != Some(0) {
                    built = false;
                }
                if code != Some(0) {
                    break;
                }
            }
            let lines = seen.lock().map(|s| s.clone()).unwrap_or_default();
            let views = if job.opens == "views" { pages(&root, &view_out, &lines) } else { Vec::new() };
            let stopped_now = holds(&stopped, run);
            if job.opens == "program" && !built && !stopped_now {
                crate::executables::advice(&job.id, &lines).into_iter().for_each(&say);
            }
            let made = if job.opens == "program" && built && !stopped_now {
                find_made(&lines).ok().map(|file| if file.starts_with(&root) { relative(&root, &file) } else { file.display().to_string() })
            } else {
                None
            };
            sink(Said::End(End { run, code, stopped: stopped_now, views, made, ms: since(began) }));
        });
        Ok((run, waits))
    }

    /// Stops a run and everything it started: its job on Windows, its process group elsewhere.
    pub fn stop(&self, run: u64) -> Result<(), String> {
        self.stopped.lock().map_err(|e| e.to_string())?.insert(run);
        if let Some(group) = self.groups.lock().map_err(|e| e.to_string())?.get(&run) {
            group.end();
        }
        let pid = self.live.lock().map_err(|e| e.to_string())?.get(&run).copied();
        let Some(pid) = pid else { return Ok(()) };
        #[cfg(unix)]
        {
            crate::container::end_group(pid);
            Ok(())
        }
        #[cfg(windows)]
        {
            let mut kill = Command::new("taskkill");
            kill.args(["/T", "/F", "/PID", &pid.to_string()]);
            quiet(&mut kill);
            kill.stdout(Stdio::null()).stderr(Stdio::null());
            kill.status().map(|_| ()).map_err(|e| e.to_string())
        }
    }
}

#[cfg(test)]
mod blobs {
    use super::Blob;

    #[test]
    fn a_blob_the_writer_placed_is_read_off_its_line_and_any_other_line_is_none() {
        let read = Blob::read(7, "qry_blob 12 1792201727932693100 0 256 7 panic.txt", 3.5).unwrap();
        assert_eq!(read, Blob { run: 7, sequence: 12, nanoseconds: 1792201727932693100, priority: 0, member: 256, size: 7, name: "panic.txt".into(), ms: 3.5 });
        assert!(Blob::read(7, "  qry_writer: the buffer of a.qry is made", 0.0).is_none(), "a line of the writer's that names no blob");
        assert!(Blob::read(7, "qry_blob 12 1 0 256 7", 0.0).is_none(), "a line with no name");
        assert!(Blob::read(7, "qry_blob 12 1 0 256 7 two names", 0.0).is_none(), "a name holds no space");
        assert!(Blob::read(7, "qry_blob x 1 0 256 7 a.txt", 0.0).is_none(), "a word that is no number");
    }

    #[test]
    fn the_panel_reads_its_own_blobs_and_counts_the_rest() {
        let named = |name: &str, priority: u32| Blob::read(1, &format!("qry_blob 1 1 {priority} 0 1 {name}"), 0.0).unwrap();
        for name in ["answers.txt", "answers12.txt", "questions.txt", "overseer.txt", "sm_86.khw", "sm_86.krs"] {
            assert!(named(name, 2).panel_reads(), "{name} is read");
        }
        assert!(named("dry.txt", 0).panel_reads() && named("dry.txt", 1).panel_reads(), "a panic and a critical blob are read");
        for name in ["dry.txt", "question12.bin", "answers_x.txt", "forms.txt"] {
            assert!(!named(name, 2).panel_reads(), "{name} is only counted");
        }
    }
}

#[cfg(test)]
mod stopping {
    use std::collections::HashMap;
    use std::sync::Arc;
    use std::time::{Duration, Instant};

    #[test]
    fn stop_ends_a_build_and_every_process_it_started_at_once() {
        if super::bash().is_err() {
            return;
        }
        // From inside orior's container, as the window runs.
        crate::container::hold();
        let root = std::env::temp_dir().join(format!("orior_ui_stop_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&root);
        std::fs::create_dir_all(root.join("src")).unwrap();
        std::fs::write(root.join("src/build_slow.sh"), "#!/usr/bin/env bash\nsleep 30\necho late\n").unwrap();
        let job = crate::catalog::read(&root).into_iter().find(|job| job.id == "build/src/build_slow.sh").expect("the build read as a job");
        let runs = super::Runs::default();
        let values = HashMap::from([("arguments".to_string(), vec![String::new()]), ("trace".to_string(), vec!["quiet".to_string()])]);
        let (run, waits) = runs.start(Arc::new(|_| {}), root.clone(), job, values).unwrap_or_else(|said| panic!("{said}"));
        std::thread::sleep(Duration::from_millis(1500));
        let asked = Instant::now();
        runs.stop(run).unwrap();
        waits.join().unwrap();
        assert!(asked.elapsed() < Duration::from_secs(5), "the run ended {:?} after Stop", asked.elapsed());
        let _ = std::fs::remove_dir_all(&root);
    }
}

#[cfg(test)]
mod settings {
    use std::collections::HashMap;
    use std::ffi::OsStr;

    use crate::catalog::{Arg, Program, Step};

    #[test]
    fn a_setting_given_is_set_in_the_step_s_environment_and_leads_its_line() {
        if super::bash().is_err() {
            return;
        }
        let step = Step { program: Program::Bash("run.sh".into()), args: vec![Arg::Value("mode".into()), Arg::Env("KLQ_SEED".into()), Arg::Env("KLQ_TRACE".into())] };
        let values: HashMap<String, Vec<String>> = [("mode".to_string(), vec!["pair".to_string()]), ("KLQ_SEED".to_string(), vec!["2".to_string()])].into_iter().collect();
        let (cmd, shown) = super::command(std::path::Path::new("."), &step, &values).unwrap();
        let envs: Vec<(&OsStr, Option<&OsStr>)> = cmd.get_envs().collect();
        assert_eq!(envs, vec![(OsStr::new("KLQ_SEED"), Some(OsStr::new("2")))]);
        assert_eq!(shown, "KLQ_SEED=2 bash run.sh pair");
    }

    #[test]
    fn a_variable_set_when_a_param_holds_a_value_is_set_then_alone() {
        if super::bash().is_err() {
            return;
        }
        let step = Step { program: Program::Bash("build.sh".into()), args: vec![Arg::SetWhen("trace".into(), "verbose".into(), "SHELLOPTS".into(), "xtrace".into())] };
        let given = |trace: &str| -> HashMap<String, Vec<String>> { [("trace".to_string(), vec![trace.to_string()])].into_iter().collect() };
        let (cmd, shown) = super::command(std::path::Path::new("."), &step, &given("verbose")).unwrap();
        assert_eq!(cmd.get_envs().collect::<Vec<_>>(), vec![(OsStr::new("SHELLOPTS"), Some(OsStr::new("xtrace")))]);
        assert_eq!(shown, "SHELLOPTS=xtrace bash build.sh");
        let (cmd, shown) = super::command(std::path::Path::new("."), &step, &given("quiet")).unwrap();
        assert_eq!(cmd.get_envs().count(), 0);
        assert_eq!(shown, "bash build.sh");
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


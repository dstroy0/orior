// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Runs every test of orior's, and says which part of it has none:
//!
//! 1. the servers the tests reach, from servers.rs, each in a container of the harness's own;
//! 2. the Rust tests of the command line, of the app and of the harness, with cargo test, told
//!    where each server listens; and the command line's on each system of systems.rs, Linux in its
//!    kinds and on aarch64 in Docker, the Macs and Windows on Arm built for alone;
//! 3. the window's tests: test/ mirrors src/ui, each module src/x.js tested by test/src/x.test.js,
//!    each run in orior's own window, started for the run with a home and a tree of its own, its
//!    page loaded fresh for each file, every press, key, drag and wheel sent as real input through
//!    its debugging port;
//! 4. what has no test: each module of the window with no file in test/src, each file of Rust with
//!    code and no test, and each test file whose module is gone.
//!
//! The window is driven through its debugging port where its web view is WebView2, on Windows, and
//! through WebDriver where it is WebKitGTK, on Linux (driven.rs). --docker runs the window's tests in
//! the harness's own Docker image on this machine, driven through WebDriver (docker.rs).
//!
//! cargo run --manifest-path test/Cargo.toml --release -- [--only TEXT] [--monitor N] [--monitors]
//!     [--no-servers] [--no-rust] [--no-window] [--keep-window] [--stop-servers]
//!     [--driver cdp|webdriver] [--docker]
//!
//! The window goes on the monitor --monitor or ORIOR_TEST_MONITOR numbers, as --monitors lists them,
//! else the highest numbered that is not the main one. The report goes to test/runs/<time>/report.md. The run fails where any test fails, the window
//! throws, or a part has no test.

mod bench;
mod cdp;
mod docker;
mod driven;
mod frames;
mod input;
mod servers;
mod systems;
mod webdriver;
mod window;

use std::collections::BTreeMap;
use std::fmt::Write as _;
use std::io::{BufRead, BufReader};
use std::path::{Component, Path, PathBuf};
use std::process::{Command, ExitCode, Stdio};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

use serde_json::{json, Value};

use cdp::Connection;

/// What the run was asked for.
struct Options {
    only: Vec<String>,
    monitor: Option<u32>,
    servers: bool,
    rust: bool,
    window: bool,
    keep_window: bool,
    stop_servers: bool,
    monitors: bool,
    systems: bool,
    /// Whether the window is driven through WebDriver, else through its debugging port.
    webdriver: bool,
    /// Whether the window's tests run in the harness's Docker image, and the words given to the
    /// harness there.
    docker: bool,
    passed: Vec<String>,
}

impl Options {
    fn read() -> Result<Options, String> {
        let mut options = Options { only: Vec::new(), monitor: std::env::var("ORIOR_TEST_MONITOR").ok().and_then(|named| named.parse().ok()), servers: true, rust: true, window: true, keep_window: false, stop_servers: false, monitors: false, systems: true, webdriver: !cfg!(windows), docker: false, passed: Vec::new() };
        let mut args = std::env::args().skip(1);
        while let Some(arg) = args.next() {
            match arg.as_str() {
                "--only" => {
                    let only = args.next().ok_or("--only needs the text of a test file's path")?;
                    options.passed.extend(["--only".to_string(), only.clone()]);
                    options.only.push(only);
                }
                "--driver" => {
                    options.webdriver = match args.next().as_deref() {
                        Some("webdriver") => true,
                        Some("cdp") => false,
                        _ => return Err("--driver needs cdp, the debugging port, or webdriver".into()),
                    }
                }
                "--docker" => options.docker = true,
                "--monitor" => options.monitor = Some(args.next().and_then(|number| number.parse().ok()).ok_or("--monitor needs the monitor's number, as the system's display settings give it")?),
                "--no-servers" => options.servers = false,
                "--no-rust" => options.rust = false,
                "--no-systems" => options.systems = false,
                "--no-window" => options.window = false,
                "--keep-window" => options.keep_window = true,
                "--stop-servers" => options.stop_servers = true,
                "--monitors" => options.monitors = true,
                "--help" | "-h" => return Err(USAGE.into()),
                other => return Err(format!("{other} is no option of the harness's\n{USAGE}")),
            }
        }
        Ok(options)
    }
}

const USAGE: &str = "harness [--only TEXT] [--monitor N] [--monitors] [--no-servers] [--no-rust] [--no-systems] [--no-window] [--keep-window] [--stop-servers] [--driver cdp|webdriver] [--docker]";

/// A test's outcome: its name, whether it passed, and why not.
pub struct Outcome {
    pub name: String,
    pub ok: bool,
    pub ms: u64,
    pub error: String,
}

/// What each part of the run found, for the report.
#[derive(Default)]
struct Report {
    servers: Vec<String>,
    crates: Vec<(String, Vec<Outcome>, String)>,
    files: Vec<(String, Vec<Outcome>, Vec<String>)>,
    untested: Vec<String>,
    orphans: Vec<String>,
    said: Vec<String>,
}

impl Report {
    fn failed(&self) -> usize {
        self.crates.iter().map(|(_, outcomes, _)| outcomes.iter().filter(|one| !one.ok).count()).sum::<usize>() + self.files.iter().map(|(_, outcomes, _)| outcomes.iter().filter(|one| !one.ok).count()).sum::<usize>()
    }

    fn passed(&self) -> usize {
        self.crates.iter().map(|(_, outcomes, _)| outcomes.iter().filter(|one| one.ok).count()).sum::<usize>() + self.files.iter().map(|(_, outcomes, _)| outcomes.iter().filter(|one| one.ok).count()).sum::<usize>()
    }

    fn write(&self) -> String {
        let mut out = String::from("# Test run\n\n");
        let _ = writeln!(out, "{} passed, {} failed, {} parts with no test, {} tests with no part.\n", self.passed(), self.failed(), self.untested.len(), self.orphans.len());
        if !self.said.is_empty() || !self.servers.is_empty() {
            out.push_str("## Servers and the window\n\n");
            for line in self.servers.iter().chain(&self.said) {
                let _ = writeln!(out, "- {line}");
            }
            out.push('\n');
        }
        out.push_str("## Rust\n\n");
        for (name, outcomes, failures) in &self.crates {
            let failed: Vec<&Outcome> = outcomes.iter().filter(|one| !one.ok).collect();
            let _ = writeln!(out, "- {name}: {} passed, {} failed", outcomes.len() - failed.len(), failed.len());
            for one in failed {
                let _ = writeln!(out, "  - FAILED {}", one.name);
            }
            if !failures.is_empty() {
                let _ = writeln!(out, "\n```text\n{}\n```\n", failures.trim_end());
            }
        }
        out.push_str("\n## The window\n\n");
        for (file, outcomes, errors) in &self.files {
            let failed = outcomes.iter().filter(|one| !one.ok).count();
            let _ = writeln!(out, "- {file}: {} passed, {failed} failed", outcomes.len() - failed);
            for one in outcomes.iter().filter(|one| !one.ok) {
                let _ = writeln!(out, "  - FAILED {} ({} ms)\n\n    ```text\n{}\n    ```", one.name, one.ms, one.error.lines().map(|line| format!("    {line}")).collect::<Vec<_>>().join("\n"));
            }
            for error in errors {
                let _ = writeln!(out, "  - the page wrote: {}", error.replace('\n', " "));
            }
        }
        if !self.untested.is_empty() {
            out.push_str("\n## No test\n\n");
            for part in &self.untested {
                let _ = writeln!(out, "- {part}");
            }
        }
        if !self.orphans.is_empty() {
            out.push_str("\n## Tests whose part is gone\n\n");
            for part in &self.orphans {
                let _ = writeln!(out, "- {part}");
            }
        }
        out
    }
}

/// The time now as year-month-day-hourminutesecond, UTC.
/// The query run's writer of src/cu built with the system's C compiler where its sources are newer, as
/// qry_run.sh builds it, for the window's tests of runs to start runs with apart from the window; None
/// where its sources or a compiler are missing.
fn qry_writer(ui: &Path, here: &Path) -> Option<PathBuf> {
    let sources = ui.parent()?.join("cu").join("types").join("file_defs").join("qry");
    let read = ["qry_writer.c", "qry_buffer.c", "qry_buffer.h", "qry.h"].map(|name| sources.join(name));
    if !read[0].exists() {
        return None;
    }
    let out = here.join("target").join("qry");
    std::fs::create_dir_all(&out).ok()?;
    let binary = out.join(if cfg!(windows) { "qry_writer.exe" } else { "qry_writer" });
    let newest = read.iter().filter_map(|file| file.metadata().ok()?.modified().ok()).max();
    let built = binary.metadata().ok().and_then(|meta| meta.modified().ok());
    if built.is_none() || newest > built {
        let made = Command::new("cc").args(["-std=c11", "-O2", "-Wall", "-Wextra", "-o"]).arg(&binary).arg(&read[0]).arg(&read[1]).status().ok()?;
        if !made.success() {
            return None;
        }
    }
    Some(binary)
}

fn stamp() -> String {
    let seconds = SystemTime::now().duration_since(UNIX_EPOCH).map(|since| since.as_secs()).unwrap_or(0);
    let (days, rest) = ((seconds / 86_400) as i64, seconds % 86_400);
    // Days since 1970 to the civil date, as Howard Hinnant's chrono-compatible algorithms give it.
    let z = days + 719_468;
    let era = z.div_euclid(146_097);
    let day_of_era = z - era * 146_097;
    let year_of_era = (day_of_era - day_of_era / 1460 + day_of_era / 36_524 - day_of_era / 146_096) / 365;
    let day_of_year = day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
    let month_part = (5 * day_of_year + 2) / 153;
    let day = day_of_year - (153 * month_part + 2) / 5 + 1;
    let month = if month_part < 10 { month_part + 3 } else { month_part - 9 };
    let year = year_of_era + era * 400 + i64::from(month <= 2);
    format!("{year:04}-{month:02}-{day:02}-{:02}{:02}{:02}", rest / 3600, rest / 60 % 60, rest % 60)
}

fn copy_folder(from: &Path, to: &Path) -> std::io::Result<()> {
    std::fs::create_dir_all(to)?;
    for entry in std::fs::read_dir(from)? {
        let entry = entry?;
        let target = to.join(entry.file_name());
        if entry.file_type()?.is_dir() {
            copy_folder(&entry.path(), &target)?;
        } else {
            std::fs::copy(entry.path(), target)?;
        }
    }
    Ok(())
}

/// The files under `folder` whose names end `ending`, by their paths under `folder`, with `/`.
fn files_under(folder: &Path, ending: &str) -> Vec<String> {
    let mut found = Vec::new();
    let mut folders = vec![folder.to_path_buf()];
    while let Some(dir) = folders.pop() {
        let Ok(entries) = std::fs::read_dir(&dir) else { continue };
        for entry in entries.flatten() {
            let path = entry.path();
            if path.is_dir() {
                folders.push(path);
            } else if path.to_string_lossy().ends_with(ending) {
                if let Ok(part) = path.strip_prefix(folder) {
                    found.push(part.to_string_lossy().replace('\\', "/"));
                }
            }
        }
    }
    found.sort();
    found
}

/// Makes the run's tree a git repository of its own, its files committed: a test's git, as a branch
/// made or a stash, acts on it, and never on the repository the tree's folder sits in. What
/// bench::placed puts under bench/ is left out.
fn own_repository(tree: &Path) -> Result<(), String> {
    let git = |args: &[&str]| -> Result<(), String> {
        let ran = Command::new("git")
            .args(["-c", "user.name=orior harness", "-c", "user.email=harness@localhost", "-c", "commit.gpgsign=false"])
            .args(args)
            .current_dir(tree)
            .stdin(Stdio::null())
            .stdout(Stdio::null())
            .stderr(Stdio::null())
            .status()
            .map_err(|error| format!("git: {error}"))?;
        if ran.success() { Ok(()) } else { Err(format!("git {} in the run's tree ended {ran}", args.join(" "))) }
    };
    git(&["init", "-q", "-b", "main"])?;
    std::fs::write(tree.join(".git").join("info").join("exclude"), "bench/\n").map_err(|error| format!("the run's tree: {error}"))?;
    git(&["add", "-A"])?;
    git(&["commit", "-q", "-m", "The run's tree."])
}

// ---- Rust ----

/// Runs cargo test for the crate at `manifest` and gives each test's outcome and the output of
/// those that failed.
fn cargo_test(manifest: &Path, target: Option<&Path>, env: &[(String, String)]) -> (Vec<Outcome>, String) {
    let mut command = Command::new("cargo");
    command.args(["test", "--no-fail-fast", "--manifest-path"]).arg(manifest);
    if let Some(target) = target {
        command.arg("--target-dir").arg(target);
    }
    command.envs(env.iter().map(|(key, value)| (key, value)));
    tests_of(command)
}

/// Runs `command`, a cargo test here or in a system's container, and gives each test's outcome and
/// the output of those that failed.
pub fn tests_of(mut command: Command) -> (Vec<Outcome>, String) {
    command.stdin(Stdio::null()).stdout(Stdio::piped()).stderr(Stdio::inherit());
    let mut child = match command.spawn() {
        Ok(child) => child,
        Err(error) => return (vec![Outcome { name: "cargo test".into(), ok: false, ms: 0, error: error.to_string() }], String::new()),
    };
    let mut outcomes = Vec::new();
    let mut failures = String::new();
    let mut in_failures = false;
    // A test's verdict comes after its name on the same line, unless what a test's own child writes
    // lands between the two; such a test is judged by cargo's list of failures at the end.
    let mut unsure = Vec::new();
    let mut failed = std::collections::HashSet::new();
    if let Some(stdout) = child.stdout.take() {
        for line in BufReader::new(stdout).lines().map_while(Result::ok) {
            if let Some(rest) = line.strip_prefix("test ") {
                if let Some((name, said)) = rest.rsplit_once(" ... ") {
                    if said.starts_with("ok") || said.starts_with("ignored") {
                        outcomes.push(Outcome { name: name.to_string(), ok: true, ms: 0, error: String::new() });
                    } else if said.starts_with("FAILED") {
                        println!("    FAILED {name}");
                        outcomes.push(Outcome { name: name.to_string(), ok: false, ms: 0, error: String::new() });
                    } else {
                        unsure.push(name.to_string());
                    }
                    continue;
                }
                if rest.starts_with("result:") {
                    println!("    {line}");
                    in_failures = false;
                    continue;
                }
            }
            if line == "failures:" {
                in_failures = true;
            }
            if in_failures {
                if let Some(name) = line.strip_prefix("    ").filter(|name| !name.is_empty() && !name.contains(char::is_whitespace)) {
                    failed.insert(name.to_string());
                }
                failures.push_str(&line);
                failures.push('\n');
            }
        }
    }
    for name in unsure {
        let ok = !failed.contains(&name);
        if !ok {
            println!("    FAILED {name}");
        }
        outcomes.push(Outcome { name, ok, ms: 0, error: String::new() });
    }
    let status = child.wait();
    if !status.as_ref().is_ok_and(|status| status.success()) && outcomes.iter().all(|one| one.ok) {
        outcomes.push(Outcome { name: "the crate's tests".into(), ok: false, ms: 0, error: "cargo test failed before its tests ran: its output is above".into() });
    }
    (outcomes, failures)
}

/// Whether a file of Rust holds code and no test.
fn untested_rust(path: &Path) -> bool {
    let text = std::fs::read_to_string(path).unwrap_or_default();
    text.contains("fn ") && !text.contains("#[test]")
}

// ---- The window ----

/// What the page wrote that the run keeps, and whether it closed.
#[derive(Default)]
struct Page {
    errors: Mutex<Vec<String>>,
    closed: AtomicBool,
}

/// Where `spec`, imported by the file at `file`, is served from: a module of the window from its
/// own place, a file of test/ from under /__harness/.
fn served_as(spec: &str, file: &Path, ui: &Path) -> Option<String> {
    if !(spec.starts_with("./") || spec.starts_with("../")) {
        return None;
    }
    let mut path = PathBuf::new();
    for part in file.parent()?.join(spec).components() {
        match part {
            Component::ParentDir => {
                path.pop();
            }
            Component::CurDir => {}
            other => path.push(other),
        }
    }
    let window = ui.join("src");
    if let Ok(part) = path.strip_prefix(&window) {
        return Some(format!("/{}", part.to_string_lossy().replace('\\', "/")));
    }
    path.strip_prefix(ui).ok().map(|part| format!("/__harness/{}", part.to_string_lossy().replace('\\', "/")))
}

/// The module at `file` with each import of a module by its path made the path the page serves it
/// at: a test imports the very module the window runs, as its own import of it does. Each line is
/// read on its own and a line of a comment not at all: an apostrophe in a comment opens no string.
fn rewrite_imports(source: &str, file: &Path, ui: &Path) -> String {
    source
        .split_inclusive('\n')
        .map(|line| {
            let code = line.trim_start();
            if code.starts_with("//") || code.starts_with("/*") || code.starts_with('*') {
                line.to_string()
            } else {
                rewrite_line(line, file, ui)
            }
        })
        .collect()
}

fn rewrite_line(source: &str, file: &Path, ui: &Path) -> String {
    let mut out = String::with_capacity(source.len());
    let mut rest = source;
    while let Some(at) = rest.find(['"', '\'']) {
        let quote = rest.as_bytes()[at] as char;
        let before = rest[..at].trim_end_matches([' ', '\t', '\n', '\r', '(']);
        let Some(length) = rest[at + 1..].find(quote) else { break };
        let spec = &rest[at + 1..at + 1 + length];
        out.push_str(&rest[..at + 1]);
        if before.ends_with("from") || before.ends_with("import") {
            out.push_str(&served_as(spec, file, ui).unwrap_or_else(|| spec.to_string()));
        } else {
            out.push_str(spec);
        }
        out.push(quote);
        rest = &rest[at + 2 + length..];
    }
    out.push_str(rest);
    out
}

fn content_type(path: &Path) -> &'static str {
    match path.extension().and_then(|ending| ending.to_str()).unwrap_or("") {
        "js" | "mjs" => "text/javascript; charset=utf-8",
        "json" => "application/json; charset=utf-8",
        "css" => "text/css; charset=utf-8",
        "html" => "text/html; charset=utf-8",
        _ => "application/octet-stream",
    }
}

/// Answers a request the page made under /__harness/ with the file of test/ it names.
fn fulfill(conn: &Connection, params: &Value, ui: &Path) {
    let request = params["requestId"].as_str().unwrap_or("").to_string();
    let url = params["request"]["url"].as_str().unwrap_or("");
    let named = url.split_once("/__harness/").map(|(_, part)| part.split(['?', '#']).next().unwrap_or("")).unwrap_or("");
    let path = ui.join(named);
    let inside = path.components().all(|part| part != Component::ParentDir) && path.starts_with(ui.join("test"));
    let (code, body) = match std::fs::read_to_string(&path) {
        Ok(text) if inside => (200, if content_type(&path).starts_with("text/javascript") { rewrite_imports(&text, &path, ui) } else { text }),
        _ => (404, format!("the harness has no {named}")),
    };
    let headers = json!([{"name": "Content-Type", "value": content_type(&path)}, {"name": "Cache-Control", "value": "no-store"}]);
    let _ = conn.call("Fetch.fulfillRequest", json!({"requestId": request, "responseCode": code, "responseHeaders": headers, "body": cdp::base64(body.as_bytes())}), Duration::from_secs(10));
}

/// Reads the page's events on a thread of its own: each step runner.js asks for is performed on a
/// thread of its own and answered, each file asked for under /__harness/ is served, and each error
/// the page throws or writes is kept.
fn serve_page(conn: Arc<Connection>, page: Arc<Page>, ui: PathBuf) {
    std::thread::spawn(move || loop {
        let event = match conn.events.lock() {
            Ok(events) => events.recv(),
            Err(_) => return,
        };
        let Ok(event) = event else { return };
        let params = &event["params"];
        match event["method"].as_str().unwrap_or("") {
            "Runtime.bindingCalled" if params["name"] == "__harnessCall" => {
                let step: Value = serde_json::from_str(params["payload"].as_str().unwrap_or("{}")).unwrap_or(Value::Null);
                let conn = conn.clone();
                std::thread::spawn(move || {
                    let id = step["id"].as_u64().unwrap_or(0);
                    let error = input::perform(&conn, &step).err();
                    let _ = conn.evaluate(&format!("window.__harness?.answer({id}, {})", json!(error)), Duration::from_secs(10));
                });
            }
            "Fetch.requestPaused" => fulfill(&conn, params, &ui),
            "Runtime.exceptionThrown" => {
                let details = &params["exceptionDetails"];
                let text = details["exception"]["description"].as_str().or_else(|| details["text"].as_str()).unwrap_or("an error").to_string();
                if let Ok(mut errors) = page.errors.lock() {
                    errors.push(format!("threw {text}"));
                }
            }
            "Runtime.consoleAPICalled" if params["type"] == "error" || params["type"] == "assert" => {
                let words: Vec<String> = params["args"].as_array().into_iter().flatten().map(|arg| arg["value"].as_str().map(str::to_string).or_else(|| arg["description"].as_str().map(str::to_string)).unwrap_or_else(|| arg["value"].to_string())).collect();
                if let Ok(mut errors) = page.errors.lock() {
                    errors.push(format!("console.error {}", words.join(" ")));
                }
            }
            // What the browser itself says: a policy that refused something, a load that failed.
            "Log.entryAdded" if params["entry"]["level"] == "error" => {
                let entry = &params["entry"];
                if let Ok(mut errors) = page.errors.lock() {
                    errors.push(format!("{} {} {}", entry["source"].as_str().unwrap_or("log"), entry["text"].as_str().unwrap_or(""), entry["url"].as_str().unwrap_or("")).trim().to_string());
                }
            }
            // The browser's own trace, as it comes and as it ends.
            "Tracing.dataCollected" | "Tracing.tracingComplete" => frames::heard(event["method"].as_str().unwrap_or(""), params),
            "harness.closed" => {
                page.closed.store(true, Ordering::SeqCst);
                return;
            }
            _ => {}
        }
    });
}

/// Waits for the window to be whole: its page loaded and its start run to the end.
fn wait_started(conn: &Connection, page: &Page) -> Result<(), String> {
    let until = Instant::now() + Duration::from_secs(90);
    while Instant::now() < until {
        if page.closed.load(Ordering::SeqCst) {
            return Err("the window closed".into());
        }
        if conn.evaluate("!window.__harnessLeft && document.readyState === 'complete' && 'started' in document.documentElement.dataset", Duration::from_secs(10)).is_ok_and(|started| started == json!(true)) {
            return Ok(());
        }
        std::thread::sleep(Duration::from_millis(100));
    }
    Err("the window did not finish starting in 90 s".into())
}

/// Runs the test files `files`, each a path under test/, in the window at `conn`, the page loaded
/// fresh for each and told, as window.__harness.servers, where each test server listens.
fn run_files(conn: &Arc<Connection>, page: &Arc<Page>, ui: &Path, files: &[String], servers: &[(String, String)], report: &mut Report) {
    let runner = std::fs::read_to_string(ui.join("test").join("runner.js")).unwrap_or_default();
    let told: serde_json::Map<String, Value> = servers.iter().map(|(key, value)| (key.clone(), json!(value))).collect();
    let tell = format!("window.__harness.servers = {}", Value::Object(told));
    for (at, file) in files.iter().enumerate() {
        if page.closed.load(Ordering::SeqCst) {
            report.files.push((file.clone(), vec![Outcome { name: "the file".into(), ok: false, ms: 0, error: "the window closed before it ran".into() }], Vec::new()));
            continue;
        }
        // The page left is marked, as Page.reload answers before the page it loads is there.
        if at > 0 {
            let _ = conn.evaluate("window.__harnessLeft = true", Duration::from_secs(10));
            let _ = conn.call("Page.reload", json!({"ignoreCache": true}), Duration::from_secs(30));
        }
        let mut outcomes = Vec::new();
        let started = wait_started(conn, page).and_then(|_| conn.evaluate(&runner, Duration::from_secs(10))).and_then(|_| conn.evaluate(&tell, Duration::from_secs(10)).map(|_| ()));
        if let Ok(mut errors) = page.errors.lock() {
            errors.clear();
        }
        match started {
            Err(error) => outcomes.push(Outcome { name: "the window".into(), ok: false, ms: 0, error }),
            Ok(()) => {
                let url = format!("/__harness/test/{file}");
                let expression = format!("(async () => {{ await import({}); return await window.__harness.run(); }})()", json!(url));
                match conn.evaluate(&expression, Duration::from_secs(1800)) {
                    Ok(Value::Array(results)) => {
                        for one in results {
                            outcomes.push(Outcome { name: one["name"].as_str().unwrap_or("").into(), ok: one["ok"] == json!(true), ms: one["ms"].as_u64().unwrap_or(0), error: one["error"].as_str().unwrap_or("").into() });
                        }
                        if outcomes.is_empty() {
                            outcomes.push(Outcome { name: "the file".into(), ok: false, ms: 0, error: "the file gave no test".into() });
                        }
                    }
                    Ok(other) => outcomes.push(Outcome { name: "the file".into(), ok: false, ms: 0, error: format!("the run gave {other}") }),
                    Err(error) => outcomes.push(Outcome { name: "the file".into(), ok: false, ms: 0, error: format!("the file did not load: {error}") }),
                }
            }
        }
        let errors = page.errors.lock().map(|errors| errors.clone()).unwrap_or_default();
        let threw: Vec<&String> = errors.iter().filter(|one| one.starts_with("threw")).collect();
        if !threw.is_empty() {
            outcomes.push(Outcome { name: "the page threw nothing".into(), ok: false, ms: 0, error: threw.iter().map(|one| one.as_str()).collect::<Vec<_>>().join("\n") });
        }
        let failed = outcomes.iter().filter(|one| !one.ok).count();
        println!("  {file}: {} passed, {failed} failed", outcomes.len() - failed);
        for one in outcomes.iter().filter(|one| !one.ok) {
            println!("    FAILED {}: {}", one.name, one.error.lines().next().unwrap_or(""));
        }
        report.files.push((file.clone(), outcomes, errors));
    }
}

fn main() -> ExitCode {
    let options = match Options::read() {
        Ok(options) => options,
        Err(said) => {
            eprintln!("{said}");
            return ExitCode::from(2);
        }
    };
    let here = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let ui = here.parent().map(Path::to_path_buf).unwrap_or_else(|| here.clone());
    if options.stop_servers {
        servers::stop();
        println!("the test servers are stopped");
        return ExitCode::SUCCESS;
    }
    if options.monitors {
        println!("the monitors, by the number --monitor takes:");
        for (device, primary, [x, y, width, height]) in window::monitors() {
            println!("  {device}  {width}x{height} at {x},{y}{}", if primary { "  main" } else { "" });
        }
        return ExitCode::SUCCESS;
    }
    if options.docker {
        return docker::run(&here, &ui, &options.passed);
    }
    let run = here.join("runs").join(stamp());
    if let Err(error) = std::fs::create_dir_all(&run) {
        eprintln!("{}: {error}", run.display());
        return ExitCode::from(2);
    }
    let mut report = Report::default();

    let mut env: Vec<(String, String)> = Vec::new();
    if let Some(address) = servers::http() {
        env.push(("ORIOR_TEST_HTTP".into(), address));
    }
    if let Some(writer) = qry_writer(&ui, &here) {
        env.push(("ORIOR_TEST_QRY_WRITER".into(), writer.to_string_lossy().replace('\\', "/")));
    }
    // The files the window is measured on, fetched once and checked whole, and named to its tests by
    // their folder in the run's tree.
    let benched = bench::fetched(&here);
    match &benched {
        Ok(_) => env.push(("ORIOR_TEST_BENCH".into(), "bench".into())),
        Err(error) => report.said.push(format!("the benchmark files: {error}")),
    }
    if options.servers {
        println!("servers");
        let up = servers::start(&here);
        for (key, value) in &up.env {
            if !key.ends_with("PASSWORD") {
                println!("  {key}={value}");
                report.servers.push(format!("{key}={value}"));
            }
        }
        for said in &up.said {
            println!("  {said}");
            report.said.push(said.clone());
        }
        env.extend(up.env);
    }

    if options.rust {
        println!("rust");
        let app_target = here.join("target").join("app");
        for (name, manifest, target) in [("cli", ui.join("cli").join("Cargo.toml"), None), ("src-tauri", ui.join("src-tauri").join("Cargo.toml"), Some(app_target.as_path())), ("test", here.join("Cargo.toml"), None)] {
            println!("  {name}");
            let (outcomes, failures) = cargo_test(&manifest, target, &env);
            report.crates.push((name.to_string(), outcomes, failures));
        }
    }

    // The command line's crate on each other system Docker runs, and built for each it cannot.
    if options.systems && options.rust && options.only.is_empty() {
        println!("systems");
        if servers::docker_runs() {
            for (name, folder, platform) in systems::SYSTEMS {
                println!("  {name}");
                let (outcomes, failures) = match systems::tests_in(name, folder, platform, &here, &ui, &env) {
                    Ok(command) => tests_of(command),
                    Err(error) => (vec![Outcome { name: "the system".into(), ok: false, ms: 0, error }], String::new()),
                };
                report.crates.push((format!("cli on {name}"), outcomes, failures));
            }
        } else {
            report.said.push("Docker does not answer: the crates' tests ran on this system alone".into());
        }
        for (name, target) in systems::BUILT_ONLY {
            println!("  {name}, built only");
            report.crates.push((format!("cli for {name}, built only"), vec![systems::built_for(target, &ui, &here)], String::new()));
        }
    }

    let tests: Vec<String> = files_under(&here.join("src"), ".test.js").into_iter().map(|file| format!("src/{file}")).filter(|file| options.only.is_empty() || options.only.iter().any(|only| file.contains(only.as_str()))).collect();
    if options.window && !tests.is_empty() && options.webdriver {
        println!("window, through WebDriver");
        let tree = run.join("tree");
        let started = copy_folder(&here.join("fixtures").join("tree"), &tree)
            .map_err(|error| format!("the run's tree: {error}"))
            .and_then(|_| own_repository(&tree))
            .and_then(|_| benched.as_ref().map_or(Ok(()), |folder| bench::placed(folder, &tree)))
            .and_then(|_| std::fs::create_dir_all(run.join("home")).map_err(|error| error.to_string()))
            .and_then(|_| driven::build(&ui, &here))
            .and_then(|program| {
                let extra: Vec<(String, String)> = env.iter().filter(|(key, _)| key == "ORIOR_TEST_SSH").map(|(_, value)| ("ORIOR_SSH".to_string(), value.clone())).collect();
                driven::start(&program, &run, &tree, &extra)
            });
        match started {
            Err(error) => {
                println!("  {error}");
                report.files.push(("the window".into(), vec![Outcome { name: "it starts".into(), ok: false, ms: 0, error }], Vec::new()));
            }
            Ok(driven) => {
                driven::run_files(&driven, &here, &tests, &env, &mut report);
                input::stop_started();
            }
        }
    } else if options.window && !tests.is_empty() {
        println!("window");
        let tree = run.join("tree");
        let started = copy_folder(&here.join("fixtures").join("tree"), &tree)
            .map_err(|error| format!("the run's tree: {error}"))
            .and_then(|_| own_repository(&tree))
            .and_then(|_| benched.as_ref().map_or(Ok(()), |folder| bench::placed(folder, &tree)))
            .and_then(|_| std::fs::create_dir_all(run.join("home")).map_err(|error| error.to_string()))
            .and_then(|_| window::build(&ui, &here.join("target").join("app")))
            .and_then(|program| {
                // The window's tunnels go through the harness's own ssh, its config and its key.
                let extra: Vec<(String, String)> = env.iter().filter(|(key, _)| key == "ORIOR_TEST_SSH").map(|(_, value)| ("ORIOR_SSH".to_string(), value.clone())).collect();
                window::start(&program, &ui, &run, &tree, options.monitor, &extra)
            });
        match started {
            Err(error) => {
                println!("  {error}");
                report.files.push(("the window".into(), vec![Outcome { name: "it starts".into(), ok: false, ms: 0, error }], Vec::new()));
            }
            Ok(shown) => match Connection::open(&shown.page) {
                Err(error) => report.said.push(format!("the window's debugging port: {error}")),
                Ok(conn) => {
                    let conn = Arc::new(conn);
                    let page = Arc::new(Page::default());
                    serve_page(conn.clone(), page.clone(), ui.clone());
                    let wait = Duration::from_secs(10);
                    let readied = ["Runtime.enable", "Page.enable", "Log.enable"].iter().try_for_each(|method| conn.call(method, json!({}), wait).map(|_| ()))
                        .and_then(|_| conn.call("Runtime.addBinding", json!({"name": "__harnessCall"}), wait).map(|_| ()))
                        .and_then(|_| conn.call("Fetch.enable", json!({"patterns": [{"urlPattern": "*/__harness/*", "requestStage": "Request"}]}), wait).map(|_| ()))
                        .and_then(|_| conn.call("Emulation.setFocusEmulationEnabled", json!({"enabled": true}), wait).map(|_| ()));
                    match readied {
                        Err(error) => report.said.push(format!("the window's debugging port: {error}")),
                        Ok(()) => {
                            window::keep_placed(&shown, options.monitor);
                            run_files(&conn, &page, &ui, &tests, &env, &mut report);
                            input::stop_started();
                        }
                    }
                    if options.keep_window {
                        println!("  the window is kept open on port {}", shown.port);
                        std::mem::forget(shown);
                    }
                }
            },
        }
    }

    // What has no test, and what tests nothing. A run of some files alone says nothing of these.
    if options.only.is_empty() {
        let tested: BTreeMap<String, ()> = files_under(&here.join("src"), ".test.js").into_iter().map(|file| (file.trim_end_matches(".test.js").to_string(), ())).collect();
        let modules: Vec<String> = files_under(&ui.join("src"), ".js").into_iter().map(|file| file.trim_end_matches(".js").to_string()).collect();
        for module in &modules {
            if !tested.contains_key(module) {
                report.untested.push(format!("src/{module}.js"));
            }
        }
        for test in tested.keys() {
            if !modules.contains(test) {
                report.orphans.push(format!("test/src/{test}.test.js"));
            }
        }
        for (folder, shown) in [(ui.join("cli").join("src"), "cli/src"), (ui.join("src-tauri").join("src"), "src-tauri/src")] {
            for file in files_under(&folder, ".rs") {
                if untested_rust(&folder.join(&file)) {
                    report.untested.push(format!("{shown}/{file}"));
                }
            }
        }
        for file in ["bench.rs", "cdp.rs", "docker.rs", "driven.rs", "frames.rs", "harness.rs", "input.rs", "servers.rs", "systems.rs", "webdriver.rs", "window.rs"] {
            if untested_rust(&here.join(file)) {
                report.untested.push(format!("test/{file}"));
            }
        }
    }

    let written = run.join("report.md");
    let _ = std::fs::write(&written, report.write());
    println!("{} passed, {} failed, {} parts with no test, {} tests with no part", report.passed(), report.failed(), report.untested.len(), report.orphans.len());
    println!("report: {}", written.display());
    if report.failed() == 0 && report.untested.is_empty() && report.orphans.is_empty() && report.said.is_empty() {
        ExitCode::SUCCESS
    } else {
        ExitCode::FAILURE
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn an_import_of_the_window_s_module_is_the_page_s_own_path() {
        let ui = Path::new("/ui");
        let file = Path::new("/ui/test/src/editor/view.test.js");
        let source = "// The window's view, and the harness's own helpers.\nimport { a } from \"../../../src/editor/view.js\";\nimport '../../helpers.js';\nconst b = await import(\"../../../src/fuzzy.js\");\nconst c = \"../not/an/import.js\";\nimport x from \"/bridge.js\";\n";
        let rewritten = rewrite_imports(source, file, ui);
        assert!(rewritten.contains("from \"/editor/view.js\""), "{rewritten}");
        assert!(rewritten.contains("import '/__harness/test/helpers.js'"), "{rewritten}");
        assert!(rewritten.contains("import(\"/fuzzy.js\")"), "{rewritten}");
        assert!(rewritten.contains("\"../not/an/import.js\""), "{rewritten}");
        assert!(rewritten.contains("from \"/bridge.js\""), "{rewritten}");
    }

    #[test]
    fn a_verdict_pushed_off_its_line_is_read_from_cargo_s_list_of_failures() {
        let file = std::env::temp_dir().join(format!("orior-harness-cargo-{}.txt", std::process::id()));
        let said = "test a::one ... ok\ntest a::two ... said by a child\nok\ntest a::three ... printed\nFAILED\n\nfailures:\n\n---- a::three stdout ----\nit broke\n\nfailures:\n    a::three\n\ntest result: FAILED. 2 passed; 1 failed\n";
        std::fs::write(&file, said).unwrap();
        let mut command = Command::new(if cfg!(windows) { "cmd" } else { "cat" });
        if cfg!(windows) {
            command.args(["/c", "type"]);
        }
        command.arg(&file);
        let (outcomes, failures) = tests_of(command);
        let _ = std::fs::remove_file(&file);
        let verdicts: Vec<(&str, bool)> = outcomes.iter().map(|one| (one.name.as_str(), one.ok)).collect();
        assert_eq!(verdicts, [("a::one", true), ("a::two", true), ("a::three", false)]);
        assert!(failures.contains("it broke"));
    }

    #[test]
    fn the_run_s_tree_is_a_repository_of_its_own() {
        let tree = std::env::temp_dir().join(format!("orior-harness-tree-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&tree);
        std::fs::create_dir_all(tree.join("bench")).unwrap();
        std::fs::write(tree.join("hello.py"), "print('hello')\n").unwrap();
        std::fs::write(tree.join("bench").join("big.txt"), "big\n").unwrap();
        own_repository(&tree).unwrap();
        let said = |args: &[&str]| String::from_utf8(Command::new("git").args(args).current_dir(&tree).output().unwrap().stdout).unwrap();
        let top = PathBuf::from(said(&["rev-parse", "--show-toplevel"]).trim());
        assert_eq!(top.canonicalize().unwrap(), tree.canonicalize().unwrap(), "git's top is the tree itself");
        assert_eq!(said(&["ls-files"]).trim(), "hello.py", "the files committed, bench/ left out");
        assert_eq!(said(&["status", "--porcelain"]).trim(), "", "nothing left to commit");
        std::fs::remove_dir_all(&tree).unwrap();
    }

    #[test]
    fn a_stamp_is_the_date_and_time() {
        let made = stamp();
        assert_eq!(made.len(), 17, "{made}");
        assert!(made.starts_with("20"), "{made}");
    }

    #[test]
    fn a_file_of_rust_with_code_and_no_test_is_untested() {
        let dir = std::env::temp_dir().join(format!("orior-harness-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        std::fs::write(dir.join("code.rs"), "fn a() {}\n").unwrap();
        std::fs::write(dir.join("tested.rs"), "fn a() {}\n#[test]\nfn b() {}\n").unwrap();
        std::fs::write(dir.join("mods.rs"), "mod a;\n").unwrap();
        assert!(untested_rust(&dir.join("code.rs")));
        assert!(!untested_rust(&dir.join("tested.rs")));
        assert!(!untested_rust(&dir.join("mods.rs")));
        std::fs::remove_dir_all(&dir).unwrap();
    }

    #[test]
    fn a_failure_is_in_the_report() {
        let mut report = Report::default();
        report.files.push(("src/a.test.js".into(), vec![Outcome { name: "one".into(), ok: false, ms: 3, error: "it broke".into() }], vec!["console.error x".into()]));
        report.untested.push("src/b.js".into());
        let written = report.write();
        assert!(written.contains("FAILED one (3 ms)"), "{written}");
        assert!(written.contains("it broke"), "{written}");
        assert!(written.contains("- src/b.js"), "{written}");
        assert_eq!(report.failed(), 1);
    }
}

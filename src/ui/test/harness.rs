// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Runs every test of orior's, and says which part of it has none:
//!
//! 1. the servers the tests reach, from servers.rs, each in a container of the harness's own;
//! 2. the Rust tests of the command line, of the app and of the harness, with cargo test, told
//!    where each server listens;
//! 3. the window's tests: test/ mirrors src/ui, each module src/x.js tested by test/src/x.test.js,
//!    each run in orior's own window, started for the run with a home and a tree of its own, its
//!    page loaded fresh for each file, every press, key, drag and wheel sent as real input through
//!    its debugging port;
//! 4. what has no test: each module of the window with no file in test/src, each file of Rust with
//!    code and no test, and each test file whose module is gone.
//!
//! cargo run --manifest-path test/Cargo.toml --release -- [--only TEXT] [--monitor N] [--monitors]
//!     [--no-servers] [--no-rust] [--no-window] [--keep-window] [--stop-servers]
//!
//! The window goes on the monitor --monitor or ORIOR_TEST_MONITOR numbers, as --monitors lists them,
//! else the highest numbered that is not the main one. The report goes to test/runs/<time>/report.md. The run fails where any test fails, the window
//! throws, or a part has no test.

mod cdp;
mod input;
mod servers;
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
}

impl Options {
    fn read() -> Result<Options, String> {
        let mut options = Options { only: Vec::new(), monitor: std::env::var("ORIOR_TEST_MONITOR").ok().and_then(|named| named.parse().ok()), servers: true, rust: true, window: true, keep_window: false, stop_servers: false, monitors: false };
        let mut args = std::env::args().skip(1);
        while let Some(arg) = args.next() {
            match arg.as_str() {
                "--only" => options.only.push(args.next().ok_or("--only needs the text of a test file's path")?),
                "--monitor" => options.monitor = Some(args.next().and_then(|number| number.parse().ok()).ok_or("--monitor needs the monitor's number, as the system's display settings give it")?),
                "--no-servers" => options.servers = false,
                "--no-rust" => options.rust = false,
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

const USAGE: &str = "harness [--only TEXT] [--monitor N] [--monitors] [--no-servers] [--no-rust] [--no-window] [--keep-window] [--stop-servers]";

/// A test's outcome: its name, whether it passed, and why not.
struct Outcome {
    name: String,
    ok: bool,
    ms: u64,
    error: String,
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

// ---- Rust ----

/// Runs cargo test for the crate at `manifest` and gives each test's outcome and the output of
/// those that failed.
fn cargo_test(manifest: &Path, target: Option<&Path>, env: &[(String, String)]) -> (Vec<Outcome>, String) {
    let mut command = Command::new("cargo");
    command.args(["test", "--no-fail-fast", "--manifest-path"]).arg(manifest);
    if let Some(target) = target {
        command.arg("--target-dir").arg(target);
    }
    command.envs(env.iter().map(|(key, value)| (key, value))).stdin(Stdio::null()).stdout(Stdio::piped()).stderr(Stdio::inherit());
    let mut child = match command.spawn() {
        Ok(child) => child,
        Err(error) => return (vec![Outcome { name: "cargo test".into(), ok: false, ms: 0, error: error.to_string() }], String::new()),
    };
    let mut outcomes = Vec::new();
    let mut failures = String::new();
    let mut in_failures = false;
    if let Some(stdout) = child.stdout.take() {
        for line in BufReader::new(stdout).lines().map_while(Result::ok) {
            if let Some(rest) = line.strip_prefix("test ") {
                if let Some((name, said)) = rest.rsplit_once(" ... ") {
                    let ok = said.starts_with("ok") || said.starts_with("ignored");
                    if !ok {
                        println!("    FAILED {name}");
                    }
                    outcomes.push(Outcome { name: name.to_string(), ok, ms: 0, error: String::new() });
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
                failures.push_str(&line);
                failures.push('\n');
            }
        }
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
/// at: a test imports the very module the window runs, as its own import of it does.
fn rewrite_imports(source: &str, file: &Path, ui: &Path) -> String {
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
fn fulfil(conn: &Connection, params: &Value, ui: &Path) {
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
            "Fetch.requestPaused" => fulfil(&conn, params, &ui),
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
/// fresh for each.
fn run_files(conn: &Arc<Connection>, page: &Arc<Page>, ui: &Path, files: &[String], report: &mut Report) {
    let runner = std::fs::read_to_string(ui.join("test").join("runner.js")).unwrap_or_default();
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
        let started = wait_started(conn, page).and_then(|_| conn.evaluate(&runner, Duration::from_secs(10)).map(|_| ()));
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
    let run = here.join("runs").join(stamp());
    if let Err(error) = std::fs::create_dir_all(&run) {
        eprintln!("{}: {error}", run.display());
        return ExitCode::from(2);
    }
    let mut report = Report::default();

    let mut env: Vec<(String, String)> = Vec::new();
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
        env = up.env;
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

    let tests: Vec<String> = files_under(&here.join("src"), ".test.js").into_iter().map(|file| format!("src/{file}")).filter(|file| options.only.is_empty() || options.only.iter().any(|only| file.contains(only.as_str()))).collect();
    if options.window && !tests.is_empty() {
        println!("window");
        let tree = run.join("tree");
        let started = copy_folder(&here.join("fixtures").join("tree"), &tree)
            .map_err(|error| format!("the run's tree: {error}"))
            .and_then(|_| std::fs::create_dir_all(run.join("home")).map_err(|error| error.to_string()))
            .and_then(|_| window::build(&ui, &here.join("target").join("app")))
            .and_then(|program| window::start(&program, &ui, &run, &tree, options.monitor));
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
                            run_files(&conn, &page, &ui, &tests, &mut report);
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
        for file in ["cdp.rs", "harness.rs", "input.rs", "servers.rs", "window.rs"] {
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
        let source = "import { a } from \"../../../src/editor/view.js\";\nimport '../../helpers.js';\nconst b = await import(\"../../../src/fuzzy.js\");\nconst c = \"../not/an/import.js\";\nimport x from \"/bridge.js\";\n";
        let rewritten = rewrite_imports(source, file, ui);
        assert!(rewritten.contains("from \"/editor/view.js\""), "{rewritten}");
        assert!(rewritten.contains("import '/__harness/test/helpers.js'"), "{rewritten}");
        assert!(rewritten.contains("import(\"/fuzzy.js\")"), "{rewritten}");
        assert!(rewritten.contains("\"../not/an/import.js\""), "{rewritten}");
        assert!(rewritten.contains("from \"/bridge.js\""), "{rewritten}");
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

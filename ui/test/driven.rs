// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! orior's window driven through WebDriver, where its web view is WebKitGTK and has no debugging
//! port: on Linux, and in the harness's own Docker image on any machine.
//!
//! The window is built from a page of the harness's staging: src/ as it is, catch.js put first in
//! its head, and every file of test/ under /__harness/, each import made the path the page serves
//! it at. A test imports the very modules the window runs, from the window's own origin, as the
//! debugging port's window serves them. WebKitWebDriver starts the window through launch.sh, with
//! a home, a tree and a log of the run's own, and every step runner.js asks for is answered as
//! WebDriver's actions, the page receiving each as it does from the mouse and keyboard.

use std::collections::BTreeSet;
use std::fs::File;
use std::path::{Path, PathBuf};
use std::process::{Child, Command, Stdio};
use std::time::{Duration, Instant};

use serde_json::{json, Value};

use crate::webdriver::{self, Session};
use crate::{Outcome, Report};

/// Where the page the window is built from is staged, and where the window is built.
fn staging(here: &Path) -> PathBuf {
    here.join("target").join("driven").join("page")
}

/// Writes `bytes` to `path` where it does not already hold them, and says whether it wrote: a page
/// staged again unchanged leaves the window's build as it was.
fn write_if_changed(path: &Path, bytes: &[u8]) -> Result<bool, String> {
    if std::fs::read(path).is_ok_and(|held| held == bytes) {
        return Ok(false);
    }
    if let Some(folder) = path.parent() {
        std::fs::create_dir_all(folder).map_err(|error| format!("{}: {error}", folder.display()))?;
    }
    std::fs::write(path, bytes).map_err(|error| format!("{}: {error}", path.display()))?;
    Ok(true)
}

/// The page with catch.js put first in its head, ahead of every script it loads.
fn with_catch(index: &str) -> String {
    match index.find("<head>") {
        Some(at) => format!("{}\n    <script src=\"/__harness/catch.js\"></script>{}", &index[..at + "<head>".len()], &index[at + "<head>".len()..]),
        None => index.to_string(),
    }
}

/// The files of test/ the page serves under /__harness/test/: those at its top and those under
/// src/, each a path under test/.
fn harness_files(here: &Path) -> Vec<String> {
    let mut files: Vec<String> = std::fs::read_dir(here)
        .into_iter()
        .flatten()
        .flatten()
        .filter(|entry| entry.path().is_file())
        .map(|entry| entry.file_name().to_string_lossy().to_string())
        .filter(|name| name.ends_with(".js"))
        .collect();
    files.extend(crate::files_under(&here.join("src"), ".js").into_iter().map(|file| format!("src/{file}")));
    files.sort();
    files
}

/// Stages the page into `stage`: src/ of `ui` with catch.js first in its head, and test/'s files
/// under __harness/, each import made the path the page serves it at. What the stage held that the
/// page no longer has is taken away.
pub fn stage(ui: &Path, here: &Path, stage: &Path) -> Result<(), String> {
    let mut kept = BTreeSet::new();
    let window = ui.join("gui").join("src-js");
    for file in crate::files_under(&window, "") {
        let from = window.join(&file);
        let bytes = std::fs::read(&from).map_err(|error| format!("{}: {error}", from.display()))?;
        let bytes = if file == "index.html" { with_catch(&String::from_utf8_lossy(&bytes)).into_bytes() } else { bytes };
        write_if_changed(&stage.join(&file), &bytes)?;
        kept.insert(file);
    }
    let catch = std::fs::read(here.join("catch.js")).map_err(|error| format!("catch.js: {error}"))?;
    write_if_changed(&stage.join("__harness").join("catch.js"), &catch)?;
    kept.insert("__harness/catch.js".into());
    for file in harness_files(here) {
        let from = here.join(&file);
        let text = std::fs::read_to_string(&from).map_err(|error| format!("{}: {error}", from.display()))?;
        let shown = format!("__harness/test/{file}");
        write_if_changed(&stage.join(&shown), crate::rewrite_imports(&text, &from, ui).as_bytes())?;
        kept.insert(shown);
    }
    for file in crate::files_under(stage, "") {
        if !kept.contains(&file) {
            let _ = std::fs::remove_file(stage.join(&file));
        }
    }
    Ok(())
}

/// Stages the page and builds the window with it, the page put in the build in place of src/, and
/// gives the program built.
pub fn build(ui: &Path, here: &Path) -> Result<PathBuf, String> {
    let page = staging(here);
    stage(ui, here, &page)?;
    let target = here.join("target").join("driven").join("app");
    // Merged into tauri.conf.json as the window is built.
    let config = json!({"build": {"frontendDist": page.to_string_lossy()}}).to_string();
    let built = crate::guest("cargo")
        .args(["build", "--manifest-path"])
        .arg(ui.join("gui").join("src-tauri").join("Cargo.toml"))
        .arg("--target-dir")
        .arg(&target)
        .env("TAURI_CONFIG", config)
        .stdin(Stdio::null())
        .status()
        .map_err(|error| format!("cargo: {error}"))?;
    if !built.success() {
        return Err("the app did not build".into());
    }
    Ok(target.join("debug").join(if cfg!(windows) { "orior_gui.exe" } else { "orior_gui" }))
}

/// `text` quoted for the shell, as one word.
fn quoted(text: &str) -> String {
    format!("'{}'", text.replace('\'', "'\\''"))
}

/// The script WebKitWebDriver starts the window through: in the run's tree, with its home, the
/// settings `extra` gives, and the web view's automation allowed, its output in the run's log.
fn launch_script(program: &Path, run: &Path, tree: &Path, extra: &[(String, String)]) -> String {
    let path = |path: &Path| quoted(&path.to_string_lossy());
    let mut settings = vec![("ORIOR_HOME".to_string(), run.join("home").to_string_lossy().to_string()), ("ORIOR_ROOT".to_string(), tree.to_string_lossy().to_string()), ("ORIOR_NO_REPORTS".to_string(), "1".to_string()), ("TAURI_WEBVIEW_AUTOMATION".to_string(), "true".to_string())];
    settings.extend(extra.iter().cloned());
    let exports: Vec<String> = settings.iter().map(|(key, value)| format!("export {key}={}", quoted(value))).collect();
    format!("#!/bin/sh\n# The window, as WebKitWebDriver starts it for the harness; the words it is given are WebDriver's own.\ncd {}\n{}\nexec {} >> {} 2>&1\n", path(tree), exports.join("\n"), path(program), path(&run.join("window.log")))
}

/// The window open under WebDriver, closed with its driver when dropped.
pub struct Driven {
    driver: Child,
    pub session: Session,
}

impl Drop for Driven {
    fn drop(&mut self) {
        self.session.end();
        let _ = self.driver.kill();
        let _ = self.driver.wait();
    }
}

/// Starts WebKitWebDriver and opens a session on the window `program` gives, with `run` as the
/// run's folder, `tree` as its tree and the settings `extra` gives besides.
pub fn start(program: &Path, run: &Path, tree: &Path, extra: &[(String, String)]) -> Result<Driven, String> {
    let launch = run.join("launch.sh");
    std::fs::write(&launch, launch_script(program, run, tree, extra)).map_err(|error| format!("launch.sh: {error}"))?;
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        std::fs::set_permissions(&launch, std::fs::Permissions::from_mode(0o755)).map_err(|error| format!("launch.sh: {error}"))?;
    }
    let port = std::net::TcpListener::bind(("127.0.0.1", 0)).and_then(|listener| listener.local_addr()).map(|address| address.port()).map_err(|error| error.to_string())?;
    let log = File::create(run.join("webdriver.log")).map_err(|error| error.to_string())?;
    let mut driver = Command::new("WebKitWebDriver")
        .arg(format!("--port={port}"))
        .stdin(Stdio::null())
        .stdout(log.try_clone().map_err(|error| error.to_string())?)
        .stderr(log)
        .spawn()
        .map_err(|error| format!("WebKitWebDriver: {error}"))?;
    let until = Instant::now() + Duration::from_secs(30);
    while !webdriver::answers(port) {
        if Instant::now() >= until || driver.try_wait().is_ok_and(|ended| ended.is_some()) {
            let _ = driver.kill();
            return Err(format!("WebKitWebDriver did not answer; its log is {}", run.join("webdriver.log").display()));
        }
        std::thread::sleep(Duration::from_millis(100));
    }
    match Session::open(port, &launch.to_string_lossy()) {
        Ok(session) => Ok(Driven { driver, session }),
        Err(error) => {
            let _ = driver.kill();
            let _ = driver.wait();
            Err(format!("{error}; the window's log is {}", run.join("window.log").display()))
        }
    }
}

/// Waits for the window to be whole: its page loaded and its start run to the end.
fn wait_started(session: &Session) -> Result<(), String> {
    let until = Instant::now() + Duration::from_secs(90);
    let mut said = String::new();
    while Instant::now() < until {
        match session.execute("return !window.__harnessLeft && document.readyState === 'complete' && 'started' in document.documentElement.dataset;", json!([])) {
            Ok(Value::Bool(true)) => return Ok(()),
            Ok(_) => {}
            Err(error) => said = error,
        }
        std::thread::sleep(Duration::from_millis(100));
    }
    Err(format!("the window did not finish starting in 90 s{}", if said.is_empty() { String::new() } else { format!(": {said}") }))
}

/// The page's side of the steps: each step runner.js asks for kept until the harness takes it, and
/// the file's end kept the same way.
const QUEUE: &str = "const h = (window.__harnessDriven = { queue: [], end: null, wake: null }); window.__harnessCall = (payload) => { h.queue.push(payload); h.wake?.(); }; return true;";

/// Runs the file at `arguments[0]` and keeps its results, or what kept it from running, as its end.
const RUN: &str = "const h = window.__harnessDriven; const url = arguments[0]; (async () => { await import(url); return await window.__harness.run(); })().then((results) => { h.end = { results }; h.wake?.(); }, (error) => { h.end = { error: String(error?.stack || error) }; h.wake?.(); }); return true;";

/// Waits for the next step the page asks for, or for the file's end.
const NEXT: &str = "const done = arguments[arguments.length - 1]; const h = window.__harnessDriven; const look = () => { if (h.queue.length) { h.wake = null; done({ step: h.queue.shift() }); } else if (h.end) { h.wake = null; done({ end: h.end }); } else { h.wake = look; } }; look();";

/// runner.js as one expression a script can return: its sourceURL line and the semicolon that
/// ends its statement taken off.
fn expression_of(runner: &str) -> String {
    let kept: Vec<&str> = runner.lines().filter(|line| !line.trim_start().starts_with("//# sourceURL")).collect();
    kept.join("\n").trim_end().trim_end_matches(';').to_string()
}

/// Runs one file in the window, answering each step it asks for, and gives its outcomes.
fn run_file(session: &Session, runner: &str, tell: &Value, file: &str) -> Result<Vec<Outcome>, String> {
    wait_started(session)?;
    session.execute(&format!("return (\n{}\n);", expression_of(runner)), json!([]))?;
    session.execute(QUEUE, json!([]))?;
    session.execute("window.__harness.servers = arguments[0]; if (window.__harnessErrors) window.__harnessErrors.length = 0; return true;", json!([tell]))?;
    session.execute(RUN, json!([format!("/__harness/test/{file}")]))?;
    loop {
        let next = session.execute_async(NEXT, json!([]), Duration::from_secs(1800))?;
        if let Some(payload) = next["step"].as_str() {
            let step: Value = serde_json::from_str(payload).unwrap_or(Value::Null);
            let error = crate::input::perform_driven(session, &step).err();
            session.execute("window.__harness.answer(arguments[0], arguments[1]); return true;", json!([step["id"], error]))?;
            continue;
        }
        let end = &next["end"];
        if let Some(error) = end["error"].as_str() {
            return Err(format!("the file did not load: {error}"));
        }
        let results = end["results"].as_array().cloned().unwrap_or_default();
        let outcomes: Vec<Outcome> = results.iter().map(|one| Outcome { name: one["name"].as_str().unwrap_or("").into(), ok: one["ok"] == json!(true), ms: one["ms"].as_u64().unwrap_or(0), error: one["error"].as_str().unwrap_or("").into() }).collect();
        return Ok(outcomes);
    }
}

/// Runs the test files `files`, each a path under test/, in the window `driven`, the page loaded
/// fresh for each and told, as window.__harness.servers, where each test server listens.
pub fn run_files(driven: &Driven, here: &Path, files: &[String], servers: &[(String, String)], report: &mut Report) {
    let session = &driven.session;
    let runner = std::fs::read_to_string(here.join("runner.js")).unwrap_or_default();
    let told: serde_json::Map<String, Value> = servers.iter().map(|(key, value)| (key.clone(), json!(value))).collect();
    let tell = Value::Object(told);
    let mut closed = false;
    for (at, file) in files.iter().enumerate() {
        if closed {
            report.files.push((file.clone(), vec![Outcome { name: "the file".into(), ok: false, ms: 0, error: "the window closed before it ran".into() }], Vec::new()));
            continue;
        }
        // The page left is marked, as a reload is answered before the page it loads is there.
        if at > 0 {
            let _ = session.execute("window.__harnessLeft = true; location.reload(); return true;", json!([]));
        }
        let mut outcomes = match run_file(session, &runner, &tell, file) {
            Ok(outcomes) if outcomes.is_empty() => vec![Outcome { name: "the file".into(), ok: false, ms: 0, error: "the file gave no test".into() }],
            Ok(outcomes) => outcomes,
            Err(error) => {
                closed = session.url().is_err();
                vec![Outcome { name: "the file".into(), ok: false, ms: 0, error }]
            }
        };
        let errors: Vec<String> = session.execute("return window.__harnessErrors || [];", json!([])).ok().and_then(|kept| kept.as_array().cloned()).unwrap_or_default().iter().filter_map(|one| one.as_str().map(str::to_string)).collect();
        let threw: Vec<&String> = errors.iter().filter(|one| one.starts_with("threw")).collect();
        if !threw.is_empty() {
            outcomes.push(Outcome { name: "the page threw nothing".into(), ok: false, ms: 0, error: threw.iter().map(|one| one.as_str()).collect::<Vec<_>>().join("\n") });
        }
        let failed = outcomes.iter().filter(|one| !one.ok).count();
        let not_run = outcomes.iter().filter(|one| one.not_run()).count();
        println!("  {file}: {} passed, {failed} failed, {not_run} not run on this window", outcomes.len() - failed - not_run);
        for one in outcomes.iter().filter(|one| !one.ok) {
            println!("    FAILED {}: {}", one.name, one.error.lines().next().unwrap_or(""));
        }
        report.files.push((file.clone(), outcomes, errors));
        // A key or a button a file left held is let go before the next.
        let _ = session.release();
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn folder(name: &str) -> PathBuf {
        let dir = std::env::temp_dir().join(format!("orior-driven-{name}-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        std::fs::create_dir_all(&dir).unwrap();
        dir
    }

    #[test]
    fn the_runner_is_one_expression() {
        let runner = "// The runner.\n(() => {\n  return \"ready\";\n})();\n//# sourceURL=harness-runner.js\n";
        assert_eq!(expression_of(runner), "// The runner.\n(() => {\n  return \"ready\";\n})()");
    }

    #[test]
    fn catch_js_comes_first_in_the_head() {
        let page = with_catch("<html>\n  <head>\n    <script src=\"early.js\"></script>\n  </head>\n</html>\n");
        let catch = page.find("/__harness/catch.js").unwrap();
        assert!(catch < page.find("early.js").unwrap(), "{page}");
        assert!(page.contains("<head>\n    <script src=\"/__harness/catch.js\"></script>"), "{page}");
        assert_eq!(with_catch("<p>no head</p>"), "<p>no head</p>");
    }

    #[test]
    fn the_stage_holds_the_page_and_the_tests_served_at_their_paths() {
        let ui = folder("stage");
        let here = ui.join("test");
        std::fs::create_dir_all(ui.join("gui").join("src-js").join("editor")).unwrap();
        std::fs::create_dir_all(here.join("src").join("editor")).unwrap();
        std::fs::write(ui.join("gui").join("src-js").join("index.html"), "<html><head></head></html>").unwrap();
        std::fs::write(ui.join("gui").join("src-js").join("editor").join("view.js"), "export const a = 1;\n").unwrap();
        std::fs::write(here.join("catch.js"), "// kept\n").unwrap();
        std::fs::write(here.join("helpers.js"), "export const b = 2;\n").unwrap();
        std::fs::write(here.join("src").join("editor").join("view.test.js"), "import { a } from \"../../../gui/src-js/editor/view.js\";\nimport \"../../helpers.js\";\n").unwrap();
        let staged = ui.join("stage");
        std::fs::create_dir_all(&staged).unwrap();
        std::fs::write(staged.join("gone.js"), "").unwrap();
        stage(&ui, &here, &staged).unwrap();
        assert!(std::fs::read_to_string(staged.join("index.html")).unwrap().contains("/__harness/catch.js"));
        assert!(staged.join("editor").join("view.js").is_file());
        assert!(staged.join("__harness").join("catch.js").is_file());
        let test = std::fs::read_to_string(staged.join("__harness").join("test").join("src").join("editor").join("view.test.js")).unwrap();
        assert!(test.contains("from \"/editor/view.js\""), "{test}");
        assert!(test.contains("import \"/__harness/test/helpers.js\""), "{test}");
        assert!(!staged.join("gone.js").exists());
        let written = write_if_changed(&staged.join("index.html"), &std::fs::read(staged.join("index.html")).unwrap()).unwrap();
        assert!(!written, "a file the stage already holds is not written again");
        std::fs::remove_dir_all(&ui).unwrap();
    }

    #[test]
    fn the_launch_script_starts_the_window_with_the_run_s_own_settings() {
        let script = launch_script(Path::new("/target/debug/orior_gui"), Path::new("/runs/a"), Path::new("/runs/a/tree"), &[("ORIOR_SSH".into(), "ssh -F 'x'".into())]);
        assert!(script.starts_with("#!/bin/sh\n"), "{script}");
        assert!(script.contains("cd '/runs/a/tree'\n"), "{script}");
        assert!(script.contains("export TAURI_WEBVIEW_AUTOMATION='true'\n"), "{script}");
        assert!(script.contains("export ORIOR_SSH='ssh -F '\\''x'\\'''\n"), "{script}");
        let log = Path::new("/runs/a").join("window.log");
        assert!(script.ends_with(&format!("exec '/target/debug/orior_gui' >> '{}' 2>&1\n", log.display())), "{script}");
    }
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Running a tree's Python tests: with pytest where the tree's Python has it, and with unittest where
//! it has not, unittest running the tests of its `TestCase`s. The files of a run are spread over the
//! machine's cores, a process of pytest to each share, the files of most tests first, each to the
//! share that holds the fewest so far; each test's result is passed on as its process prints it, and
//! each failure's message and the line it failed at as its process ends, from pytest's report.
//!
//! A run that records its coverage runs pytest under orior's own shim, which marks each line of the
//! tree's files that runs, by `sys.monitoring` where Python has it and `sys.settrace` where it has not,
//! and writes them, with the lines each file could run, for the run to put together.

use std::collections::{HashMap, HashSet};
use std::io::{BufRead, BufReader, Read as _};
use std::path::{Path, PathBuf};
use std::process::{Child, Command, Stdio};
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use std::time::Instant;

use serde::Serialize;

use crate::toolchains;

/// A test's result: its name as pytest gives it, `passed`, `failed`, `skipped` or `running`, what it
/// says where it failed or was skipped, the file and line it failed at, and how long it took.
#[derive(Serialize, Clone, Debug, PartialEq)]
pub struct Outcome {
    pub id: String,
    pub outcome: String,
    pub message: String,
    pub file: Option<String>,
    pub line: Option<u32>,
    pub seconds: f64,
}

/// The lines of a file that ran, and those it could run that did not, each counted from 0.
#[derive(Serialize, Clone, Debug, PartialEq)]
pub struct Covered {
    pub file: String,
    pub ran: Vec<u32>,
    pub missed: Vec<u32>,
}

/// What a run tells as it goes: that it started, with how many tests or files, over how many
/// processes and with which runner; each result; the coverage; and its end, with its counts.
#[derive(Serialize, Clone, Debug)]
#[serde(tag = "kind", rename_all = "lowercase")]
pub enum Heard {
    Started { run: u64, given: usize, workers: usize, runner: String },
    Result { run: u64, outcome: Outcome },
    Coverage { run: u64, files: Vec<Covered> },
    Done { run: u64, passed: usize, failed: usize, skipped: usize, seconds: f64, said: String },
}

pub type Tell = Arc<dyn Fn(Heard) + Send + Sync>;

/// The shim a run that records its coverage runs pytest under: its out file, the tree's folder, then
/// pytest's words.
const SHIM: &str = r#"
import json, os, sys, threading
out, root, words = sys.argv[1], os.path.normcase(os.path.abspath(sys.argv[2])), sys.argv[3:]
ran = {}
known = {}
def ours(name):
    if name not in known:
        full = os.path.normcase(os.path.abspath(name))
        known[name] = not name.startswith("<") and os.path.isfile(name) and full.startswith(root + os.sep) and not any(part in full.split(os.sep) for part in ("site-packages", ".venv", "venv", "__pycache__"))
    return known[name]
if hasattr(sys, "monitoring"):
    watch = sys.monitoring
    tool = watch.COVERAGE_ID
    watch.use_tool_id(tool, "orior")
    def seen(code, line):
        if ours(code.co_filename):
            ran.setdefault(code.co_filename, set()).add(line)
        return watch.DISABLE
    watch.register_callback(tool, watch.events.LINE, seen)
    watch.set_events(tool, watch.events.LINE)
else:
    def trace(frame, event, arg):
        if not ours(frame.f_code.co_filename):
            return None
        def lines(frame, event, arg):
            if event == "line":
                ran.setdefault(frame.f_code.co_filename, set()).add(frame.f_lineno)
            return lines
        return lines(frame, event, arg)
    sys.settrace(trace)
    threading.settrace(trace)
import pytest
code = pytest.main(words)
if hasattr(sys, "monitoring"):
    sys.monitoring.set_events(sys.monitoring.COVERAGE_ID, 0)
else:
    sys.settrace(None)
found = {}
for name, lines in ran.items():
    could = set()
    try:
        with open(name, encoding="utf-8") as handle:
            codes = [compile(handle.read(), name, "exec")]
        while codes:
            one = codes.pop()
            could.update(line for _, _, line in one.co_lines() if line)
            codes.extend(inner for inner in one.co_consts if hasattr(inner, "co_lines"))
    except Exception:
        could = set(lines)
    found[os.path.relpath(name, root)] = {"ran": sorted(lines), "could": sorted(could)}
with open(out, "w", encoding="utf-8") as handle:
    json.dump(found, handle)
sys.exit(code)
"#;

/// The Python a tree's tests run with: its environment's, or the toolchains' own.
pub fn python() -> Option<PathBuf> {
    if let Some(python) = toolchains::environment().and_then(|env| env.python) {
        return Some(python);
    }
    let tool = toolchains::manifest().into_iter().find(|tool| tool.id == "python")?;
    toolchains::find(&tool, &toolchains::path_folders(), &toolchains::chosen()).program.map(PathBuf::from)
}

/// Whether `python` has pytest.
fn has_pytest(python: &Path, root: &Path) -> bool {
    let mut command = Command::new(python);
    command.args(["-c", "import pytest"]).current_dir(root).env("PATH", toolchains::run_path()).stdin(Stdio::null()).stdout(Stdio::null()).stderr(Stdio::null());
    crate::runner::quiet(&mut command);
    command.status().is_ok_and(|status| status.success())
}

/// The file a test's name names.
fn file_of(id: &str) -> &str {
    id.split("::").next().unwrap_or(id)
}

/// The shares a run's tests or files are spread over, at most `most`: whole files to a share, the
/// files of most first, each to the share that holds the fewest so far.
fn shares(given: &[String], most: usize) -> Vec<Vec<String>> {
    let mut by_file: Vec<(String, Vec<String>)> = Vec::new();
    for one in given {
        match by_file.iter_mut().find(|(file, _)| file == file_of(one)) {
            Some((_, all)) => all.push(one.clone()),
            None => by_file.push((file_of(one).to_string(), vec![one.clone()])),
        }
    }
    by_file.sort_by(|a, b| b.1.len().cmp(&a.1.len()).then_with(|| a.0.cmp(&b.0)));
    let count = most.clamp(1, by_file.len().max(1));
    let mut out: Vec<Vec<String>> = vec![Vec::new(); count];
    for (_, all) in by_file {
        let least = (0..count).min_by_key(|at| out[*at].len()).unwrap_or(0);
        out[least].extend(all);
    }
    out.retain(|share| !share.is_empty());
    out
}

/// A line pytest prints in its verbose report as a test ends: the test's name and its outcome.
fn pytest_line(line: &str) -> Option<(String, &'static str)> {
    let line = line.trim_end();
    for (word, outcome) in [(" PASSED", "passed"), (" FAILED", "failed"), (" ERROR", "failed"), (" SKIPPED", "skipped"), (" XFAIL", "skipped"), (" XPASS", "passed")] {
        if let Some(at) = line.find(word) {
            let id = line[..at].trim();
            let after = &line[at + word.len()..];
            if id.contains("::") && !id.contains(' ') && (after.is_empty() || after.starts_with(' ') || after.starts_with('[')) {
                return Some((id.to_string(), outcome));
            }
        }
    }
    None
}

/// A line unittest prints in its verbose report as a test ends: the test's name as pytest gives it and
/// its outcome. `test_x (pkg.test_a.Case.test_x) ... ok` names `pkg/test_a.py::Case::test_x`.
fn unittest_line(line: &str) -> Option<(String, &'static str)> {
    let (head, tail) = line.split_once(" ... ")?;
    let dotted = head.split_once(" (")?.1.trim_end_matches(')');
    let outcome = match tail.trim() {
        "ok" => "passed",
        "FAIL" | "ERROR" => "failed",
        other if other.starts_with("skipped") || other.starts_with("expected failure") => "skipped",
        _ => return None,
    };
    Some((unittest_id(dotted)?, outcome))
}

/// A unittest name, `pkg.test_a.Case.test_x`, as pytest names it.
fn unittest_id(dotted: &str) -> Option<String> {
    let parts: Vec<&str> = dotted.split('.').collect();
    let at = parts.iter().rposition(|part| part.chars().next().is_some_and(char::is_uppercase))?;
    if at == 0 || at + 1 >= parts.len() {
        return None;
    }
    Some(format!("{}.py::{}::{}", parts[..at].join("/"), parts[at], parts[at + 1..].join(".")))
}

/// A pytest name as unittest takes it: `pkg/test_a.py::Case::test_x` as `pkg.test_a.Case.test_x`.
fn dotted_of(id: &str) -> String {
    let mut parts = id.split("::");
    let module = parts.next().unwrap_or_default().trim_end_matches(".py").replace(['/', '\\'], ".");
    std::iter::once(module).chain(parts.map(str::to_string)).collect::<Vec<_>>().join(".")
}

/// An XML attribute's value with its entities read.
fn unescaped(text: &str) -> String {
    text.replace("&#10;", "\n").replace("&#13;", "\r").replace("&quot;", "\"").replace("&apos;", "'").replace("&lt;", "<").replace("&gt;", ">").replace("&amp;", "&")
}

/// The attributes of an XML tag's text, `<name a="1" b="2">`.
fn attributes(tag: &str) -> HashMap<String, String> {
    let mut found = HashMap::new();
    let mut rest = tag;
    while let Some(eq) = rest.find("=\"") {
        let name = rest[..eq].rsplit(|char: char| char.is_whitespace()).next().unwrap_or_default().to_string();
        let value_from = eq + 2;
        let Some(len) = rest[value_from..].find('"') else {
            break;
        };
        found.insert(name, unescaped(&rest[value_from..value_from + len]));
        rest = &rest[value_from + len + 1..];
    }
    found
}

/// The outcomes of pytest's report in JUnit's form, each with how long its test took: a failure's and
/// a skip's message, and the line of its file the traceback last names, or the test's own line.
fn report_outcomes(xml: &str) -> Vec<Outcome> {
    let mut found = Vec::new();
    let mut rest = xml;
    while let Some(start) = rest.find("<testcase ") {
        let case = &rest[start..];
        let open_end = case.find('>').unwrap_or(case.len());
        let lone = case[..open_end].ends_with('/');
        let close = if lone { open_end + 1 } else { case.find("</testcase>").map_or(case.len(), |at| at + "</testcase>".len()) };
        let attrs = attributes(&case[..open_end]);
        let body = &case[open_end.min(close)..close];
        rest = &case[close.min(case.len())..];
        let Some(file) = attrs.get("file").cloned() else {
            continue;
        };
        let name = attrs.get("name").cloned().unwrap_or_default();
        let module = file.trim_end_matches(".py").replace(['/', '\\'], ".");
        let classname = attrs.get("classname").cloned().unwrap_or_default();
        let class = classname.strip_prefix(&module).map(|rest| rest.trim_start_matches('.')).filter(|class| !class.is_empty());
        let id = match class {
            Some(class) => format!("{}::{}::{name}", file.replace('\\', "/"), class.replace('.', "::")),
            None => format!("{}::{name}", file.replace('\\', "/")),
        };
        let seconds = attrs.get("time").and_then(|time| time.parse().ok()).unwrap_or(0.0);
        let own_line = attrs.get("line").and_then(|line| line.parse::<u32>().ok());
        let mut told = false;
        for (tag, outcome) in [("<failure", "failed"), ("<error", "failed"), ("<skipped", "skipped")] {
            let Some(at) = body.find(tag) else {
                continue;
            };
            let tag_end = body[at..].find('>').map_or(body.len(), |end| at + end);
            let said = attributes(&body[at..tag_end]);
            let text_end = body[tag_end..].find("</").map_or(body.len(), |end| tag_end + end);
            let text = unescaped(&body[(tag_end + 1).min(text_end)..text_end]);
            let wanted = file.replace('\\', "/");
            let line = text
                .lines()
                .filter_map(|line| {
                    let (place, _) = line.split_once(": ")?;
                    let (path, number) = place.rsplit_once(':')?;
                    (path.replace('\\', "/").ends_with(&wanted)).then(|| number.trim().parse::<u32>().ok()).flatten()
                })
                .next_back()
                .map(|line| line.saturating_sub(1))
                .or(own_line);
            let message = said.get("message").cloned().filter(|message| !message.is_empty()).unwrap_or_else(|| text.lines().rev().find(|line| !line.trim().is_empty()).unwrap_or_default().trim().to_string());
            found.push(Outcome { id: id.clone(), outcome: outcome.to_string(), message, file: Some(wanted), line, seconds });
            told = true;
            break;
        }
        // A test that passed is told for how long it took.
        if !told {
            found.push(Outcome { id, outcome: "passed".to_string(), message: String::new(), file: Some(file.replace('\\', "/")), line: own_line, seconds });
        }
    }
    found
}

/// The failures of unittest's report: each failure's name as pytest gives it, its last line, and the
/// line of its file the traceback last names.
fn unittest_failures(text: &str) -> Vec<Outcome> {
    let mut found = Vec::new();
    for block in text.split("\n======================================================================\n").skip(1) {
        let mut lines = block.lines();
        let head = lines.next().unwrap_or_default();
        let Some(dotted) = head.split_once(" (").map(|(_, rest)| rest.trim_end_matches(')')) else {
            continue;
        };
        let Some(id) = unittest_id(dotted) else {
            continue;
        };
        let wanted = file_of(&id).to_string();
        // The traceback stands between the rule under the head and the rule after it.
        let body: Vec<&str> = lines.skip_while(|line| !line.starts_with("---")).skip(1).take_while(|line| !line.starts_with("---")).collect();
        let line = body.iter().filter_map(|line| {
            let rest = line.trim().strip_prefix("File \"")?;
            let (path, after) = rest.split_once('"')?;
            let number = after.trim_start_matches(", line ").split(',').next()?;
            path.replace('\\', "/").ends_with(&wanted).then(|| number.trim().parse::<u32>().ok()).flatten()
        }).next_back().map(|line| line.saturating_sub(1));
        let message = body.iter().rev().find(|line| !line.trim().is_empty() && !line.starts_with("---")).map(|line| line.trim().to_string()).unwrap_or_default();
        found.push(Outcome { id, outcome: "failed".into(), message, file: Some(wanted), line, seconds: 0.0 });
    }
    found
}

/// The runs of tests the window holds: the processes of the one going, and the number of the last.
#[derive(Default)]
pub struct Runs {
    children: Arc<Mutex<Vec<Child>>>,
    stopped: Arc<AtomicBool>,
    last: Arc<AtomicU64>,
}

impl Runs {
    /// Stops the run going, where one is.
    pub fn stop(&self) {
        self.stopped.store(true, Ordering::SeqCst);
        if let Ok(mut children) = self.children.lock() {
            for child in children.iter_mut() {
                let _ = child.kill();
            }
            children.clear();
        }
    }

    /// Runs `given`, tests by their names or files by their paths, in the tree at `root`: spread over
    /// the machine's cores where `parallel`, with coverage recorded where `cover`. Says the run's
    /// number; what it finds comes by `tell`.
    pub fn start(&self, root: &Path, given: Vec<String>, parallel: bool, cover: bool, tell: Tell) -> Result<u64, String> {
        self.stop();
        if given.is_empty() {
            return Err("No test is given to run".to_string());
        }
        let python = python().ok_or("No Python is found to run the tests: File, Toolchains finds one")?;
        let pytest = has_pytest(&python, root);
        if cover && !pytest {
            return Err("Coverage is recorded by a run of pytest, which the tree's Python has not: python -m pip install pytest".to_string());
        }
        let run = self.last.fetch_add(1, Ordering::SeqCst) + 1;
        self.stopped.store(false, Ordering::SeqCst);
        let cores = std::thread::available_parallelism().map_or(1, usize::from);
        let shares = if parallel { shares(&given, cores) } else { vec![given.clone()] };
        let runner = if pytest { "pytest" } else { "unittest" };
        tell(Heard::Started { run, given: given.len(), workers: shares.len(), runner: runner.to_string() });
        let scratch = std::env::temp_dir().join(format!("orior-tests-{}-{run}", std::process::id()));
        std::fs::create_dir_all(&scratch).map_err(|error| error.to_string())?;
        let started = Instant::now();
        let counts = Arc::new(Mutex::new((0usize, 0usize, 0usize)));
        let said = Arc::new(Mutex::new(String::new()));
        let mut workers = Vec::new();
        for (index, share) in shares.into_iter().enumerate() {
            let report = scratch.join(format!("report-{index}.xml"));
            let covered = scratch.join(format!("coverage-{index}.json"));
            let mut command = Command::new(&python);
            if pytest {
                let mut words: Vec<String> = vec!["-v".into(), "-p".into(), "no:cacheprovider".into(), "--tb=short".into(), "-o".into(), "junit_family=xunit1".into(), format!("--junitxml={}", report.display())];
                words.extend(share.iter().cloned());
                if cover {
                    command.arg("-c").arg(SHIM).arg(&covered).arg(root);
                } else {
                    command.args(["-m", "pytest"]);
                }
                command.args(&words);
            } else {
                command.args(["-m", "unittest", "-v"]);
                command.args(share.iter().map(|id| dotted_of(id)));
            }
            command.current_dir(root).env("PATH", toolchains::run_path()).env("PYTHONIOENCODING", "utf-8").stdin(Stdio::null()).stdout(Stdio::piped()).stderr(Stdio::piped());
            crate::runner::quiet(&mut command);
            let mut child = command.spawn().map_err(|error| format!("{}: {error}", python.display()))?;
            let (Some(out), Some(err)) = (child.stdout.take(), child.stderr.take()) else {
                continue;
            };
            if let Ok(mut children) = self.children.lock() {
                children.push(child);
            }
            let (tell, counts, said, stopped) = (tell.clone(), counts.clone(), said.clone(), self.stopped.clone());
            workers.push(std::thread::spawn(move || {
                let errors = std::thread::spawn(move || {
                    let mut text = String::new();
                    let _ = BufReader::new(err).read_to_string(&mut text);
                    text
                });
                let mut heard: HashSet<String> = HashSet::new();
                let mut seen = String::new();
                let tally = |outcome: &str| {
                    if let Ok(mut counts) = counts.lock() {
                        match outcome {
                            "passed" => counts.0 += 1,
                            "failed" => counts.1 += 1,
                            _ => counts.2 += 1,
                        }
                    }
                };
                for line in BufReader::new(out).lines().map_while(Result::ok) {
                    if stopped.load(Ordering::SeqCst) {
                        break;
                    }
                    if let Some((id, outcome)) = if pytest { pytest_line(&line) } else { None } {
                        if heard.insert(id.clone()) {
                            tally(outcome);
                        }
                        tell(Heard::Result { run, outcome: Outcome { id, outcome: outcome.to_string(), message: String::new(), file: None, line: None, seconds: 0.0 } });
                    }
                    seen.push_str(&line);
                    seen.push('\n');
                }
                let errors = errors.join().unwrap_or_default();
                if !pytest {
                    for line in errors.lines() {
                        if let Some((id, outcome)) = unittest_line(line) {
                            if heard.insert(id.clone()) {
                                tally(outcome);
                            }
                            tell(Heard::Result { run, outcome: Outcome { id, outcome: outcome.to_string(), message: String::new(), file: None, line: None, seconds: 0.0 } });
                        }
                    }
                    for outcome in unittest_failures(&errors) {
                        tell(Heard::Result { run, outcome });
                    }
                } else if let Ok(xml) = std::fs::read_to_string(&report) {
                    for outcome in report_outcomes(&xml) {
                        tell(Heard::Result { run, outcome });
                    }
                }
                // A process that ran no test says why, as the last lines it wrote.
                if heard.is_empty() && !stopped.load(Ordering::SeqCst) {
                    let tail: Vec<&str> = seen.lines().chain(errors.lines()).filter(|line| !line.trim().is_empty()).collect();
                    if let Ok(mut said) = said.lock() {
                        if said.is_empty() {
                            *said = tail.iter().rev().take(3).rev().cloned().collect::<Vec<_>>().join("\n");
                        }
                    }
                }
                std::fs::read_to_string(&covered).ok()
            }));
        }
        let (children, stopped, tell_end) = (self.children.clone(), self.stopped.clone(), tell);
        std::thread::spawn(move || {
            let mut merged: HashMap<String, (HashSet<u32>, HashSet<u32>)> = HashMap::new();
            for worker in workers {
                let Ok(Some(text)) = worker.join() else {
                    continue;
                };
                let Ok(serde_json::Value::Object(files)) = serde_json::from_str::<serde_json::Value>(&text) else {
                    continue;
                };
                for (file, lines) in files {
                    let entry = merged.entry(file.replace('\\', "/")).or_default();
                    let numbers = |key: &str| lines[key].as_array().into_iter().flatten().filter_map(serde_json::Value::as_u64).map(|line| line.saturating_sub(1) as u32).collect::<Vec<u32>>();
                    entry.0.extend(numbers("ran"));
                    entry.1.extend(numbers("could"));
                }
            }
            if let Ok(mut children) = children.lock() {
                for child in children.iter_mut() {
                    let _ = child.wait();
                }
                children.clear();
            }
            if !merged.is_empty() {
                let mut files: Vec<Covered> = merged
                    .into_iter()
                    .map(|(file, (ran, could))| {
                        let mut ran_lines: Vec<u32> = ran.iter().copied().filter(|line| could.is_empty() || could.contains(line)).collect();
                        ran_lines.sort();
                        let mut missed: Vec<u32> = could.difference(&ran).copied().collect();
                        missed.sort();
                        Covered { file, ran: ran_lines, missed }
                    })
                    .collect();
                files.sort_by(|a, b| a.file.cmp(&b.file));
                tell_end(Heard::Coverage { run, files });
            }
            let (passed, failed, skipped) = counts.lock().map(|counts| *counts).unwrap_or_default();
            let mut said = said.lock().map(|said| said.clone()).unwrap_or_default();
            if stopped.load(Ordering::SeqCst) {
                said = "The run was stopped".to_string();
            }
            tell_end(Heard::Done { run, passed, failed, skipped, seconds: started.elapsed().as_secs_f64(), said });
            let _ = std::fs::remove_dir_all(&scratch);
        });
        Ok(run)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_run_is_spread_by_whole_files_the_largest_first() {
        let given: Vec<String> = ["a.py::t1", "a.py::t2", "a.py::t3", "b.py::t1", "c.py::t1", "c.py::t2"].map(String::from).to_vec();
        let out = shares(&given, 2);
        assert_eq!(out, [vec!["a.py::t1", "a.py::t2", "a.py::t3"], vec!["c.py::t1", "c.py::t2", "b.py::t1"]]);
        assert_eq!(shares(&given, 8).len(), 3, "no more shares than files");
        assert_eq!(shares(&given, 1).len(), 1);
    }

    #[test]
    fn verbose_reports_are_read() {
        assert_eq!(pytest_line("tests/test_a.py::TestX::test_two PASSED                [ 50%]"), Some(("tests/test_a.py::TestX::test_two".to_string(), "passed")));
        assert_eq!(pytest_line("tests/test_a.py::test_one[1-2] FAILED"), Some(("tests/test_a.py::test_one[1-2]".to_string(), "failed")));
        assert_eq!(pytest_line("FAILED tests/test_a.py::test_one - assert 1 == 2"), None);
        assert_eq!(unittest_line("test_two (tests.test_a.Checks.test_two) ... ok"), Some(("tests/test_a.py::Checks::test_two".to_string(), "passed")));
        assert_eq!(unittest_line("test_three (pkg.test_b.Checks.test_three) ... skipped 'later'"), Some(("pkg/test_b.py::Checks::test_three".to_string(), "skipped")));
        assert_eq!(dotted_of("pkg/test_b.py::Checks::test_three"), "pkg.test_b.Checks.test_three");
    }

    #[test]
    fn a_report_gives_each_failure_its_message_and_line() {
        let xml = r#"<testsuites><testsuite><testcase classname="tests.test_a" name="test_one" file="tests/test_a.py" line="3" time="0.002"><failure message="assert 1 == 2">tests/test_a.py:5: in test_one
    helper()
tests/test_a.py:9: in helper
    assert 1 == 2
E   assert 1 == 2</failure></testcase><testcase classname="tests.test_a.TestX" name="test_two" file="tests/test_a.py" line="12" time="0.1" /><testcase classname="tests.test_a.TestX" name="test_skip" file="tests/test_a.py" line="14" time="0"><skipped type="pytest.skip" message="not here">skip</skipped></testcase></testsuite></testsuites>"#;
        let found = report_outcomes(xml);
        assert_eq!(found.len(), 3);
        assert_eq!((found[0].id.as_str(), found[0].message.as_str(), found[0].line), ("tests/test_a.py::test_one", "assert 1 == 2", Some(8)));
        assert_eq!((found[1].id.as_str(), found[1].outcome.as_str(), found[1].seconds), ("tests/test_a.py::TestX::test_two", "passed", 0.1));
        assert_eq!((found[2].id.as_str(), found[2].outcome.as_str(), found[2].line), ("tests/test_a.py::TestX::test_skip", "skipped", Some(14)));
    }

    #[test]
    fn a_run_tells_each_result_its_failures_and_its_coverage() {
        let Some(python) = python() else {
            return;
        };
        let dir = std::env::temp_dir().join(format!("orior-test-runs-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        std::fs::create_dir_all(dir.join("tests")).unwrap();
        if !has_pytest(&python, &dir) {
            return;
        }
        std::fs::write(dir.join("shapes.py"), "def area(width, height):\n    return width * height\n\n\ndef unused():\n    return 0\n").unwrap();
        std::fs::write(dir.join("tests/test_a.py"), "import sys, os\nsys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))\nfrom shapes import area\n\n\ndef test_area():\n    assert area(2, 3) == 6\n\n\ndef test_wrong():\n    assert area(2, 3) == 7\n").unwrap();
        std::fs::write(dir.join("tests/test_b.py"), "import pytest\n\n\nclass TestB:\n    def test_skipped(self):\n        pytest.skip(\"not here\")\n").unwrap();
        let (send, heard) = std::sync::mpsc::channel();
        let send = Mutex::new(send);
        let tell: Tell = Arc::new(move |one| {
            let _ = send.lock().unwrap().send(one);
        });
        let runs = Runs::default();
        runs.start(&dir, vec!["tests/test_a.py".into(), "tests/test_b.py".into()], true, true, tell).unwrap();
        let mut results: HashMap<String, Outcome> = HashMap::new();
        let mut coverage = Vec::new();
        let mut done = None;
        let mut workers = 0;
        while let Ok(one) = heard.recv_timeout(std::time::Duration::from_secs(120)) {
            match one {
                Heard::Started { workers: count, .. } => workers = count,
                Heard::Result { outcome, .. } => {
                    let entry = results.entry(outcome.id.clone()).or_insert_with(|| outcome.clone());
                    if !outcome.message.is_empty() {
                        *entry = outcome;
                    }
                }
                Heard::Coverage { files, .. } => coverage = files,
                Heard::Done { passed, failed, skipped, .. } => {
                    done = Some((passed, failed, skipped));
                    break;
                }
            }
        }
        assert_eq!(workers, 2, "two files, two processes");
        assert_eq!(done, Some((1, 1, 1)), "{results:?}");
        let wrong = &results["tests/test_a.py::test_wrong"];
        assert_eq!((wrong.outcome.as_str(), wrong.line), ("failed", Some(10)), "{wrong:?}");
        assert!(wrong.message.contains("assert"), "{wrong:?}");
        assert_eq!(results["tests/test_b.py::TestB::test_skipped"].outcome, "skipped");
        let shapes = coverage.iter().find(|one| one.file == "shapes.py").expect("shapes.py is covered");
        assert!(shapes.ran.contains(&1) && shapes.missed.contains(&5), "{shapes:?}");
        let _ = std::fs::remove_dir_all(dir);
    }

    #[test]
    fn unittest_failures_are_read() {
        let text = "test_two (tests.test_a.Checks.test_two) ... FAIL\n\n======================================================================\nFAIL: test_two (tests.test_a.Checks.test_two)\n----------------------------------------------------------------------\nTraceback (most recent call last):\n  File \"D:\\t\\tests\\test_a.py\", line 7, in test_two\n    self.assertEqual(1, 2)\nAssertionError: 1 != 2\n\n----------------------------------------------------------------------\nRan 1 test in 0.001s\n";
        let found = unittest_failures(text);
        assert_eq!(found.len(), 1);
        assert_eq!((found[0].id.as_str(), found[0].line), ("tests/test_a.py::Checks::test_two", Some(6)));
    }
}

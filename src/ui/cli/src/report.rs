// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Error reports, filed as issues on orior's repository in the form its bug report template gives.
//!
//! A report goes through the reporter's own GitHub CLI where it is signed in, filed with no prompt
//! under the reporter's account. Where it is not, the issue page opens in the browser with every field
//! filled in, for the reporter to submit. A report whose error is already an open issue files nothing
//! and answers with that issue.
//!
//! A report's category is the part of orior it is about: the library the command line and the window
//! stand on, the command line, the window, or unknown. A panic's category is the part of orior its
//! first frame of orior's own is in.
//!
//! Before anything leaves the machine the reporter's home folder, the tree's path and the reporter's
//! name in a path are struck from it. Errors file on their own unless the reporter turns that off, a
//! setting kept in orior's own folder and shared by the window and the command line. The reporter is
//! asked once, yes the answer given by default: by the Windows installer, and otherwise on the first
//! run, in the window or at the terminal. One error files once a run, and a run files at most
//! AUTO_LIMIT on its own.

use std::collections::{BTreeSet, VecDeque};
use std::io::Write;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::sync::Mutex;

use serde::{Deserialize, Serialize};

/// The repository issues are filed on.
pub const REPOSITORY: &str = "dstroy0/orior";

/// The template a report fills, under .github/ISSUE_TEMPLATE.
const TEMPLATE: &str = "bug_report.yml";

/// The categories, as the template's dropdown names them.
pub const CATEGORIES: [&str; 4] = ["library", "cli", "ui", "unknown"];

/// The most reports a run files on its own.
const AUTO_LIMIT: usize = 5;

/// The most recent errors kept for a report that asks for them.
const KEPT_ERRORS: usize = 20;

/// The longest a filled-in issue page's address grows, which browsers and GitHub both read whole.
const ADDRESS_LIMIT: usize = 7000;

/// What a report says: its category, its title, what happened, how to make it happen again, and the
/// error's own text.
#[derive(Clone, Default, Deserialize)]
pub struct Report {
    pub category: String,
    pub title: String,
    pub what: String,
    #[serde(default)]
    pub steps: String,
    #[serde(default)]
    pub logs: String,
}

/// Where a report went: an issue filed for it, an open issue that already holds its error, or the
/// filled-in page opened for the reporter to submit.
#[derive(Clone, Serialize)]
#[serde(tag = "kind", content = "url", rename_all = "lowercase")]
pub enum Filed {
    Filed(String),
    Known(String),
    Page(String),
}

/// What orior keeps between runs: whether errors file on their own, as the reporter answered when they
/// were asked, at installation or on the first run, and nothing where they have not answered yet.
#[derive(Default, Serialize, Deserialize)]
struct Settings {
    #[serde(default)]
    auto_report: Option<bool>,
}

struct Run {
    filed: BTreeSet<String>,
    errors: VecDeque<String>,
}

static RUN: Mutex<Run> = Mutex::new(Run { filed: BTreeSet::new(), errors: VecDeque::new() });

/// orior's own folder for what it keeps between runs.
fn settings_path() -> Option<PathBuf> {
    let base = if cfg!(windows) {
        std::env::var_os("APPDATA").map(PathBuf::from)
    } else {
        std::env::var_os("XDG_CONFIG_HOME").map(PathBuf::from).or_else(|| std::env::var_os("HOME").map(|home| PathBuf::from(home).join(".config")))
    };
    base.map(|base| base.join("orior").join("settings.json"))
}

fn settings() -> Settings {
    settings_path()
        .and_then(|path| std::fs::read_to_string(path).ok())
        .and_then(|text| serde_json::from_str(&text).ok())
        .unwrap_or_default()
}

/// Whether errors file on their own: as the reporter answered, and on where they have not answered.
pub fn auto() -> bool {
    std::env::var_os("ORIOR_NO_REPORTS").is_none() && settings().auto_report.unwrap_or(true)
}

/// Whether the reporter has answered whether errors file on their own.
pub fn asked() -> bool {
    settings().auto_report.is_some()
}

/// The question the reporter is asked once, at installation or on the first run.
pub const QUESTION: &str = "orior files the errors it meets as issues on dstroy0/orior on its own: through your GitHub \
                            CLI where it is signed in, else as a page opened for you to submit. Help, Automatic Error \
                            Reports turns it on or off later.";

/// Asks the reporter once whether errors file on their own, where they have not answered, nothing
/// turned reports off, and the run is at a terminal that can answer. Enter answers yes. A run with no
/// terminal asks nothing and files as it would by default.
pub fn ask_once() {
    use std::io::{BufRead, IsTerminal};
    if asked() || std::env::var_os("ORIOR_NO_REPORTS").is_some() || !std::io::stdin().is_terminal() || !std::io::stderr().is_terminal() {
        return;
    }
    eprint!("{QUESTION}\nFile them? [Y/n] ");
    let _ = std::io::stderr().flush();
    let mut answer = String::new();
    if std::io::stdin().lock().read_line(&mut answer).is_err() {
        return;
    }
    let on = !answer.trim().to_lowercase().starts_with('n');
    if let Err(said) = set_auto(on) {
        eprintln!("{said}");
    }
}

pub fn set_auto(on: bool) -> Result<(), String> {
    let path = settings_path().ok_or("orior has no folder of its own to keep settings in")?;
    if let Some(folder) = path.parent() {
        std::fs::create_dir_all(folder).map_err(|e| e.to_string())?;
    }
    let mut kept = settings();
    kept.auto_report = Some(on);
    std::fs::write(&path, serde_json::to_string_pretty(&kept).map_err(|e| e.to_string())?).map_err(|e| e.to_string())
}

/// `text` with the reporter's home folder, the tree's path and the reporter's name in a path struck,
/// each written either way its slashes lean.
pub fn scrub(text: &str, root: Option<&Path>) -> String {
    let mut out = text.to_string();
    let mut strike = |found: &str, put: &str| {
        if found.len() < 3 {
            return;
        }
        for form in [found.to_string(), found.replace('\\', "/"), found.replace('/', "\\")] {
            out = replace_any_case(&out, &form, put);
        }
    };
    if let Some(root) = root {
        strike(&root.to_string_lossy(), "<tree>");
    }
    for home in ["USERPROFILE", "HOME"] {
        if let Some(home) = std::env::var_os(home) {
            strike(&home.to_string_lossy(), "~");
        }
    }
    for name in ["USERNAME", "USER"] {
        if let Ok(name) = std::env::var(name) {
            for (before, after) in [("\\", "\\"), ("/", "/")] {
                strike(&format!("{before}{name}{after}"), &format!("{before}<user>{after}"));
            }
        }
    }
    out
}

fn replace_any_case(text: &str, found: &str, put: &str) -> String {
    let lower = text.to_lowercase();
    let wanted = found.to_lowercase();
    if wanted.is_empty() || lower.len() != text.len() {
        return text.replace(found, put);
    }
    let mut out = String::with_capacity(text.len());
    let mut at = 0;
    while let Some(index) = lower[at..].find(&wanted) {
        out.push_str(&text[at..at + index]);
        out.push_str(put);
        at += index + wanted.len();
    }
    out.push_str(&text[at..]);
    out
}

/// A short name for an error that stays the same each time it happens: its category and its text with
/// the numbers in it struck, hashed.
pub fn fingerprint(category: &str, text: &str) -> String {
    let mut hash: u64 = 0xcbf2_9ce4_8422_2325;
    for byte in category.bytes().chain(text.bytes().filter(|b| !b.is_ascii_digit())) {
        hash ^= u64::from(byte);
        hash = hash.wrapping_mul(0x0100_0000_01b3);
    }
    format!("{hash:016x}")
}

/// orior's version, and the commit and changes of the tree where it is orior's own.
fn version(root: Option<&Path>) -> String {
    let mut said = format!("orior {}", env!("CARGO_PKG_VERSION"));
    if let Some(root) = root {
        let git = |args: &[&str]| {
            let mut command = Command::new("git");
            command.args(args).current_dir(root).stdin(Stdio::null()).stderr(Stdio::null());
            crate::runner::quiet(&mut command);
            command.output().ok().filter(|out| out.status.success()).map(|out| String::from_utf8_lossy(&out.stdout).trim().to_string())
        };
        if let Some(commit) = git(&["rev-parse", "--short=10", "HEAD"]) {
            let changed = git(&["status", "--porcelain", "--untracked-files=no"]).is_some_and(|out| !out.is_empty());
            said.push_str(&format!(" at {commit}{}", if changed { " with changes" } else { "" }));
        }
    }
    said
}

fn system() -> String {
    format!("{} {}", std::env::consts::OS, std::env::consts::ARCH)
}

fn category_of(report: &Report) -> &str {
    CATEGORIES.iter().find(|one| **one == report.category).copied().unwrap_or("unknown")
}

/// The issue's body, laid out as the template's form lays out what is entered in it.
fn body(report: &Report, root: Option<&Path>, print: &str) -> String {
    let field = |text: &str| if text.trim().is_empty() { "_No response_".to_string() } else { text.trim().to_string() };
    let logs = if report.logs.trim().is_empty() { "_No response_".to_string() } else { format!("```text\n{}\n```", report.logs.trim()) };
    format!(
        "### Category\n\n{}\n\n### What happened\n\n{}\n\n### Steps to reproduce\n\n{}\n\n### Version\n\n{}\n\n### System\n\n{}\n\n### Logs\n\n{}\n\n<!-- orior-report {print} -->\n",
        category_of(report),
        field(&report.what),
        field(&report.steps),
        version(root),
        system(),
        logs
    )
}

fn encode(text: &str) -> String {
    let mut out = String::with_capacity(text.len() * 3);
    for byte in text.bytes() {
        if byte.is_ascii_alphanumeric() || b"-_.~".contains(&byte) {
            out.push(byte as char);
        } else {
            out.push_str(&format!("%{byte:02X}"));
        }
    }
    out
}

/// The new-issue page with the template's fields filled in, its logs cut to keep the address within
/// ADDRESS_LIMIT.
pub fn page(report: &Report, root: Option<&Path>) -> String {
    let category = category_of(report);
    let base = format!("https://github.com/{REPOSITORY}/issues/new?template={TEMPLATE}&labels=bug,{category}");
    let mut fields = vec![
        ("title", report.title.clone()),
        ("category", category.to_string()),
        ("what", report.what.clone()),
        ("steps", report.steps.clone()),
        ("version", version(root)),
        ("system", system()),
    ];
    let without: usize = base.len() + fields.iter().map(|(name, text)| name.len() + 2 + encode(text).len()).sum::<usize>();
    let mut logs = report.logs.clone();
    while !logs.is_empty() && without + 6 + encode(&logs).len() > ADDRESS_LIMIT {
        let keep = logs.chars().count() * 3 / 4;
        logs = logs.chars().take(keep).collect();
    }
    fields.push(("logs", logs));
    let mut address = base;
    for (name, text) in fields {
        if !text.is_empty() {
            address.push_str(&format!("&{name}={}", encode(&text)));
        }
    }
    address
}

fn gh(args: &[&str], input: Option<&str>) -> Option<String> {
    let mut command = Command::new("gh");
    command.args(args).stdout(Stdio::piped()).stderr(Stdio::null());
    command.stdin(if input.is_some() { Stdio::piped() } else { Stdio::null() });
    crate::runner::quiet(&mut command);
    let mut child = command.spawn().ok()?;
    if let (Some(text), Some(mut stdin)) = (input, child.stdin.take()) {
        stdin.write_all(text.as_bytes()).ok()?;
    }
    let out = child.wait_with_output().ok()?;
    out.status.success().then(|| String::from_utf8_lossy(&out.stdout).trim().to_string())
}

/// Opens a page of orior's repository in the browser: the repository itself or a page under it. Any
/// other address is refused.
pub fn open_page(url: &str) -> Result<(), String> {
    let home = format!("https://github.com/{REPOSITORY}");
    if url != home && !url.starts_with(&format!("{home}/")) {
        return Err(format!("{url} is not a page of {REPOSITORY}"));
    }
    let mut command = if cfg!(windows) {
        let mut command = Command::new("rundll32");
        command.args(["url.dll,FileProtocolHandler", url]);
        command
    } else if cfg!(target_os = "macos") {
        let mut command = Command::new("open");
        command.arg(url);
        command
    } else {
        let mut command = Command::new("xdg-open");
        command.arg(url);
        command
    };
    command.stdin(Stdio::null()).stdout(Stdio::null()).stderr(Stdio::null());
    command.spawn().map(|_| ()).map_err(|e| e.to_string())
}

/// Files a report: as an issue through the reporter's GitHub CLI where it is signed in, as nothing
/// where its error is already an open issue, and otherwise as the filled-in page, opened.
pub fn file(given: &Report, root: Option<&Path>) -> Filed {
    let report = Report {
        category: category_of(given).to_string(),
        title: scrub(&given.title, root),
        what: scrub(&given.what, root),
        steps: scrub(&given.steps, root),
        logs: scrub(&given.logs, root),
    };
    let print = fingerprint(&report.category, &format!("{}\n{}", report.title, report.logs));
    if gh(&["auth", "status"], None).is_some() {
        let search = format!("\"orior-report {print}\" in:body");
        if let Some(url) = gh(&["issue", "list", "--repo", REPOSITORY, "--state", "open", "--search", &search, "--json", "url", "--jq", ".[0].url"], None).filter(|url| !url.is_empty()) {
            return Filed::Known(url);
        }
        let text = body(&report, root, &print);
        let title = format!("[bug] {}", report.title);
        let category = report.category.as_str();
        let labelled = ["issue", "create", "--repo", REPOSITORY, "--title", &title, "--body-file", "-", "--label", "bug", "--label", category];
        // A repository without a category's label still takes the issue, unlabelled.
        if let Some(url) = gh(&labelled, Some(&text)).or_else(|| gh(&labelled[..8], Some(&text))) {
            return Filed::Filed(url.lines().last().unwrap_or_default().to_string());
        }
    }
    let url = page(&report, root);
    let _ = open_page(&url);
    Filed::Page(url)
}

/// Keeps an error among the run's recent ones, struck of what identifies the reporter.
pub fn keep(text: &str, root: Option<&Path>) {
    if let Ok(mut run) = RUN.lock() {
        run.errors.push_back(scrub(text, root));
        while run.errors.len() > KEPT_ERRORS {
            run.errors.pop_front();
        }
    }
}

/// The run's recent errors, oldest first.
pub fn recent() -> String {
    RUN.lock().map(|run| run.errors.iter().cloned().collect::<Vec<_>>().join("\n\n")).unwrap_or_default()
}

/// Files an error on its own where the reporter lets errors file, where it has not filed this run,
/// and where the run has filed fewer than AUTO_LIMIT. Answers where it went, or nothing.
pub fn error(category: &str, message: &str, detail: &str, root: Option<&Path>) -> Option<Filed> {
    keep(&format!("{message}\n{detail}"), root);
    if !auto() {
        return None;
    }
    let first = message.lines().next().unwrap_or("error").trim();
    let title: String = format!("{category}: {first}").chars().take(100).collect();
    let print = fingerprint(category, &scrub(&title, root));
    {
        let mut run = RUN.lock().ok()?;
        if run.filed.len() >= AUTO_LIMIT || !run.filed.insert(print) {
            return None;
        }
    }
    let report = Report {
        category: category.to_string(),
        title,
        what: format!("orior reported this error on its own.\n\n{message}"),
        steps: String::new(),
        logs: detail.to_string(),
    };
    Some(file(&report, root))
}

/// The category of a panic, from where its backtrace first enters orior's own code: the window, the
/// command line, or the library under both.
pub fn panic_category(trace: &str) -> &'static str {
    for line in trace.lines() {
        if line.contains("orior_ui_lib::") {
            return "ui";
        }
        if line.contains("orior_cli::cli::") || line.contains("orior_cli::main") {
            return "cli";
        }
        if line.contains("orior_cli::") && !line.contains("orior_cli::report::") {
            return "library";
        }
    }
    "unknown"
}

/// Files every panic of this program as a report, after the panic says what it says as it always
/// does. Where the report went is written to standard error.
pub fn catch_panics() {
    let before = std::panic::take_hook();
    std::panic::set_hook(Box::new(move |info| {
        before(info);
        let trace = std::backtrace::Backtrace::force_capture().to_string();
        let message = info
            .payload()
            .downcast_ref::<&str>()
            .map(|text| text.to_string())
            .or_else(|| info.payload().downcast_ref::<String>().cloned())
            .unwrap_or_else(|| "a panic".to_string());
        let place = info.location().map(|at| format!("{}:{}", at.file(), at.line())).unwrap_or_default();
        let root = crate::root::find();
        let detail = format!("panicked at {place}\n\n{trace}");
        match error(panic_category(&trace), &format!("panic: {message}"), &detail, root.as_deref()) {
            Some(Filed::Filed(url)) | Some(Filed::Known(url)) => eprintln!("reported: {url}"),
            Some(Filed::Page(_)) => eprintln!("the report opened in the browser, to submit there"),
            None => {}
        }
    }));
}

#[cfg(test)]
mod reports {
    use super::{fingerprint, page, panic_category, replace_any_case, Report};

    #[test]
    fn a_path_is_struck_whatever_its_case() {
        assert_eq!(replace_any_case("at D:\\Git\\Tree\\x.rs", "d:\\git\\tree", "<tree>"), "at <tree>\\x.rs");
    }

    #[test]
    fn an_error_keeps_its_name_when_only_numbers_change() {
        assert_eq!(fingerprint("ui", "edit.js:12:3 broke"), fingerprint("ui", "edit.js:40:9 broke"));
        assert_ne!(fingerprint("ui", "edit.js broke"), fingerprint("cli", "edit.js broke"));
    }

    #[test]
    fn a_panic_is_placed_by_orior_s_first_frame() {
        assert_eq!(panic_category("std::panicking\n  orior_cli::files::read\n  orior_ui_lib::run"), "library");
        assert_eq!(panic_category("std::panicking\n  orior_cli::cli::run"), "cli");
        assert_eq!(panic_category("std::panicking\n  orior_ui_lib::start"), "ui");
        assert_eq!(panic_category("std::panicking\n  core::option"), "unknown");
    }

    #[test]
    fn a_page_stays_within_its_limit() {
        let report = Report { category: "ui".into(), title: "t".into(), what: "w".into(), steps: String::new(), logs: "x".repeat(20000) };
        let url = page(&report, None);
        assert!(url.len() <= super::ADDRESS_LIMIT + 200);
        assert!(url.contains("template=bug_report.yml") && url.contains("category=ui"));
    }
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Validate: a tool plugin checks a proof with its prover and tells a failure of the mathematics from
//! a limit of the machine. A tool plugin is a folder holding plugin.json, as a language plugin is, of
//! kind "tool":
//!
//!   languages               the languages whose files it checks
//!   toolchain               the toolchain in the manifest its program is found by
//!   run                     { program, args, project, inProject, patience }: the program and its
//!                           words, `{file}` the file; the names of the files that mark a project's
//!                           folder, and the words that run the program inside the project, from its
//!                           folder; and the seconds one check may take
//!   barriers                each a limit of the machine: `kinds`, the kinds of message the prover
//!                           gives where the limit stopped the check, or `says`, words such a message
//!                           holds, and `not`, words it does not; `lift`, the word
//!                           that sets the limit, `{value}` its value; and `default`, `grow` and
//!                           `ceiling`, where it starts, how many times it grows a step, and how far
//!                           it may go
//!   flags                   each a list of `words`, the `severity` a use of one is, and what it `says`
//!   audit                   { declarations, print, trusted }: the words that open a theorem, the
//!                           line that asks for the axioms one rests on, `{name}` its name, and the
//!                           axioms that are trusted
//!
//! The file is checked as a copy, with the lines that ask for each theorem's axioms after it. Where a
//! check stops at a barrier, it is checked again with that barrier's limit grown a step, until it no
//! longer stops there, the limit reaches its ceiling, or MOST_CHECKS checks have run. A limit the
//! file sets for itself, with `set_option <name> <value>`, is grown in the copy too, since the file's
//! own setting comes after the one the program is given. What is left once no barrier stops the
//! check is the mathematics.

use std::collections::BTreeMap;
use std::io::Read;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::time::{Duration, Instant};

use serde::{Deserialize, Serialize};
use serde_json::Value;

use crate::plugins;
use crate::toolchains;

const MOST_CHECKS: usize = 16;

#[derive(Deserialize, Clone, Debug)]
pub struct Tool {
    pub id: String,
    pub name: String,
    #[serde(default)]
    pub languages: Vec<String>,
    pub toolchain: String,
    pub run: Run,
    #[serde(default)]
    pub barriers: Vec<Barrier>,
    #[serde(default)]
    pub flags: Vec<Flag>,
    pub audit: Option<Audit>,
}

#[derive(Deserialize, Clone, Debug)]
#[serde(rename_all = "camelCase")]
pub struct Run {
    pub program: String,
    pub args: Vec<String>,
    #[serde(default)]
    pub project: Vec<String>,
    #[serde(default)]
    pub in_project: Vec<String>,
    #[serde(default = "patience")]
    pub patience: u64,
}

fn patience() -> u64 {
    600
}

#[derive(Deserialize, Clone, Debug)]
pub struct Barrier {
    pub name: String,
    #[serde(default)]
    pub about: String,
    #[serde(default)]
    pub kinds: Vec<String>,
    #[serde(default)]
    pub says: Vec<String>,
    #[serde(default)]
    pub not: Vec<String>,
    pub lift: String,
    pub default: u64,
    pub grow: u64,
    pub ceiling: u64,
}

#[derive(Deserialize, Clone, Debug)]
pub struct Flag {
    pub words: Vec<String>,
    pub severity: u8,
    pub says: String,
}

#[derive(Deserialize, Clone, Debug)]
pub struct Audit {
    pub declarations: Vec<String>,
    pub print: String,
    pub trusted: Vec<String>,
    /// The axioms that stand for a gap in a proof.
    #[serde(default)]
    pub gaps: Vec<String>,
    /// What an axiom whose name holds the key is.
    #[serde(default)]
    pub explains: BTreeMap<String, String>,
    /// The kinds of the prover's messages that say what the audit says.
    #[serde(default)]
    pub repeats: Vec<String>,
}

/// One thing the check found, at a place in the file: lines and columns from 0.
#[derive(Serialize, Clone, Debug, PartialEq)]
pub struct Finding {
    pub line: u32,
    pub col: u32,
    pub end_line: u32,
    pub end_col: u32,
    pub severity: u8,
    /// "math", "barrier", "flag" or "axiom".
    pub kind: &'static str,
    /// The barrier that stopped the check here, where one did.
    pub barrier: Option<String>,
    pub message: String,
    /// The settings the check went past this barrier at, or why it did not.
    pub lifted: Option<String>,
}

/// One check: the limits it ran with, how long it took, and the barriers that stopped it.
#[derive(Serialize, Clone, Debug)]
pub struct Check {
    pub settings: Vec<String>,
    pub seconds: f64,
    pub errors: usize,
    pub barriers: Vec<String>,
}

#[derive(Serialize, Clone, Debug)]
pub struct Report {
    pub tool: String,
    pub file: String,
    pub project: Option<String>,
    pub holds: bool,
    pub verdict: String,
    pub checks: Vec<Check>,
    pub findings: Vec<Finding>,
}

/// The tool plugins, those that come with orior and the reader's, each read from its plugin.json.
pub fn tools() -> Vec<Tool> {
    let mut found: BTreeMap<String, Tool> = BTreeMap::new();
    for plugin in plugins::all() {
        let Ok(value) = serde_json::from_str::<Value>(&plugin.text) else {
            continue;
        };
        if value.get("kind").and_then(Value::as_str) != Some("tool") {
            continue;
        }
        if let Ok(tool) = serde_json::from_value::<Tool>(value) {
            found.insert(tool.id.clone(), tool);
        }
    }
    found.into_values().collect()
}

/// The tool that checks files of `language`.
pub fn tool_for(language: &str) -> Option<Tool> {
    tools().into_iter().find(|tool| tool.languages.iter().any(|one| one == language))
}

/// The folder above `file`, nearest first, that holds one of `marks`.
fn project_of(file: &Path, marks: &[String]) -> Option<PathBuf> {
    let mut dir = file.parent();
    while let Some(here) = dir {
        if marks.iter().any(|mark| here.join(mark).is_file()) {
            return Some(here.to_path_buf());
        }
        dir = here.parent();
    }
    None
}

/// The text with its comments and strings blanked, every other character kept in its place, so
/// that each word found in it is a word of the code.
pub fn code_of(text: &str) -> String {
    let chars: Vec<char> = text.chars().collect();
    let mut out = String::with_capacity(text.len());
    let mut at = 0;
    let mut depth = 0usize;
    let mut in_string = false;
    let mut in_line = false;
    let blank = |c: char| if c == '\n' { '\n' } else { ' ' };
    while at < chars.len() {
        let c = chars[at];
        let next = chars.get(at + 1).copied();
        if in_line {
            if c == '\n' {
                in_line = false;
            }
            out.push(blank(c));
        } else if depth > 0 {
            if c == '/' && next == Some('-') {
                depth += 1;
                out.push_str("  ");
                at += 2;
                continue;
            }
            if c == '-' && next == Some('/') {
                depth -= 1;
                out.push_str("  ");
                at += 2;
                continue;
            }
            out.push(blank(c));
        } else if in_string {
            if c == '\\' {
                out.push(' ');
                if let Some(escaped) = next {
                    out.push(blank(escaped));
                }
                at += 2;
                continue;
            }
            if c == '"' {
                in_string = false;
            }
            out.push(blank(c));
        } else if c == '-' && next == Some('-') {
            in_line = true;
            out.push(' ');
        } else if c == '/' && next == Some('-') {
            depth = 1;
            out.push_str("  ");
            at += 2;
            continue;
        } else if c == '"' {
            in_string = true;
            out.push(' ');
        } else {
            out.push(c);
        }
        at += 1;
    }
    out
}

fn is_word(c: char) -> bool {
    c.is_alphanumeric() || matches!(c, '_' | '.' | '\'' | '#' | '!' | '?') || (!c.is_ascii() && !c.is_whitespace() && !"()[]{}⟨⟩,:;".contains(c))
}

/// Each word of the code with its line and column, from 0, and its length in characters.
fn words_of(code: &str) -> Vec<(String, u32, u32)> {
    let mut found = Vec::new();
    for (line, text) in code.lines().enumerate() {
        let chars: Vec<char> = text.chars().collect();
        let mut at = 0;
        while at < chars.len() {
            if is_word(chars[at]) {
                let start = at;
                while at < chars.len() && is_word(chars[at]) {
                    at += 1;
                }
                found.push((chars[start..at].iter().collect(), line as u32, start as u32));
            } else {
                at += 1;
            }
        }
    }
    found
}

/// Each use of a flagged word. A word matches as itself, or as the last part of a dotted name, or with
/// `@[`, `@` or a dotted head before it.
pub fn flags_in(code: &str, flags: &[Flag]) -> Vec<Finding> {
    let mut found = Vec::new();
    for (word, line, col) in words_of(code) {
        let bare = word.trim_start_matches('@');
        for flag in flags {
            let hit = flag.words.iter().any(|one| bare == one || bare.ends_with(&format!(".{one}")) || (one.contains('.') && bare == one.as_str()));
            if hit {
                found.push(Finding {
                    line,
                    col,
                    end_line: line,
                    end_col: col + word.chars().count() as u32,
                    severity: flag.severity,
                    kind: "flag",
                    barrier: None,
                    message: format!("{bare}: {}", flag.says),
                    lifted: None,
                });
                break;
            }
        }
    }
    found
}

/// The full name of each theorem the file declares, with the namespaces open where it is declared.
pub fn theorems_in(code: &str, declarations: &[String]) -> Vec<String> {
    let words = words_of(code);
    let mut spaces: Vec<(bool, String)> = Vec::new();
    let mut names = Vec::new();
    for (at, (word, _, _)) in words.iter().enumerate() {
        let next = words.get(at + 1).map(|(next, _, _)| next.as_str());
        match word.as_str() {
            "namespace" => {
                if let Some(name) = next {
                    spaces.push((true, name.to_string()));
                }
            }
            "section" | "noncomputable" if word == "section" || next == Some("section") => {
                if word == "section" {
                    spaces.push((false, String::new()));
                }
            }
            "end" => {
                spaces.pop();
            }
            _ if declarations.iter().any(|one| one == word) => {
                if let Some(name) = next {
                    let mut full: Vec<&str> = spaces.iter().filter(|(named, _)| *named).map(|(_, name)| name.as_str()).collect();
                    // A name from the root is whole as it is written.
                    let name = match name.strip_prefix("_root_.") {
                        Some(rest) => {
                            full.clear();
                            rest
                        }
                        None => name,
                    };
                    full.push(name);
                    names.push(full.join("."));
                }
            }
            _ => {}
        }
    }
    names
}

/// The copy of `text` that is checked: each `set_option <name> <value>` of a grown limit set to its
/// grown value where that is larger, and the lines that ask for each theorem's axioms after it. It
/// says which of the file's own settings it changed.
fn copy_of(text: &str, grown: &BTreeMap<String, u64>, asks: &[String]) -> (String, Vec<String>) {
    let mut changed = Vec::new();
    let mut lines: Vec<String> = Vec::new();
    for (number, line) in text.lines().enumerate() {
        let mut line = line.to_string();
        for (name, value) in grown {
            let set = format!("set_option {name} ");
            if let Some(at) = line.find(&set) {
                let digits: String = line[at + set.len()..].chars().take_while(char::is_ascii_digit).collect();
                if let Ok(own) = digits.parse::<u64>() {
                    if own != 0 && own < *value {
                        line.replace_range(at + set.len()..at + set.len() + digits.len(), &value.to_string());
                        changed.push(format!("line {}: set_option {name} {own}, checked at {value}", number + 1));
                    }
                }
            }
        }
        lines.push(line);
    }
    let mut copy = lines.join("\n");
    copy.push('\n');
    for ask in asks {
        copy.push_str(ask);
        copy.push('\n');
    }
    (copy, changed)
}

/// A message of the prover's, as `lean --json` writes one a line: lines from 1, columns from 0.
#[derive(Clone, Debug, PartialEq)]
pub struct Message {
    pub severity: u8,
    pub line: u32,
    pub col: u32,
    pub end_line: u32,
    pub end_col: u32,
    pub text: String,
    /// The prover's own name for the kind of message, where it gives one.
    pub kind: String,
}

/// The messages in a check's output. A line that is not a message is kept as an error with no
/// place, as a crash writes one.
pub fn messages_of(output: &str) -> Vec<Message> {
    let mut found = Vec::new();
    let mut loose = Vec::new();
    for line in output.lines() {
        let Ok(value) = serde_json::from_str::<Value>(line) else {
            if !line.trim().is_empty() {
                loose.push(line.trim().to_string());
            }
            continue;
        };
        let place = |key: &str, part: &str| value.get(key).and_then(|pos| pos.get(part)).and_then(Value::as_u64);
        let line_at = place("pos", "line").unwrap_or(1).max(1) as u32 - 1;
        let col_at = place("pos", "column").unwrap_or(0) as u32;
        let severity = match value.get("severity").and_then(Value::as_str) {
            Some("error") => 1,
            Some("warning") => 2,
            _ => 3,
        };
        found.push(Message {
            severity,
            line: line_at,
            col: col_at,
            end_line: place("endPos", "line").map(|line| line.max(1) as u32 - 1).unwrap_or(line_at),
            end_col: place("endPos", "column").map(|col| col as u32).unwrap_or(col_at + 1),
            text: value.get("data").and_then(Value::as_str).unwrap_or_default().to_string(),
            kind: value.get("kind").and_then(Value::as_str).unwrap_or_default().to_string(),
        });
    }
    if !loose.is_empty() {
        found.push(Message { severity: 1, line: 0, col: 0, end_line: 0, end_col: 1, text: loose.join("\n"), kind: String::new() });
    }
    found
}

/// The barrier a message says stopped the check, where one did: by the message's kind, or by its words.
pub fn barrier_of<'a>(message: &Message, barriers: &'a [Barrier]) -> Option<&'a Barrier> {
    let text = message.text.as_str();
    barriers
        .iter()
        .find(|barrier| (barrier.kinds.contains(&message.kind) || barrier.says.iter().any(|says| text.contains(says.as_str()))) && !barrier.not.iter().any(|not| text.contains(not.as_str())))
}

/// The axioms a theorem rests on, from the message that answers the line asking for them.
pub fn axioms_of(message: &str) -> Option<(String, Vec<String>)> {
    let name = message.strip_prefix('\'')?.split('\'').next()?.to_string();
    if message.contains("does not depend on any axioms") {
        return Some((name, Vec::new()));
    }
    let list = message.split_once("depends on axioms:")?.1.trim();
    let list = list.trim_start_matches('[').trim_end_matches(']');
    Some((name, list.split(',').map(|one| one.trim().to_string()).filter(|one| !one.is_empty()).collect()))
}

/// The words that run the program on `file`, from the folder it runs in.
fn command_for(tool: &Tool, program: &Path, file: &Path, project: Option<&Path>, settings: &[String]) -> Result<(Command, PathBuf), String> {
    let fill = |words: &[String]| -> Vec<String> { words.iter().map(|word| word.replace("{file}", &file.display().to_string())).collect() };
    let folder = program.parent().map(Path::to_path_buf).unwrap_or_default();
    let (mut words, dir) = match project {
        Some(project) if !tool.run.in_project.is_empty() => (fill(&tool.run.in_project), project.to_path_buf()),
        _ => (std::iter::once(tool.run.program.clone()).chain(fill(&tool.run.args)).collect(), file.parent().map(Path::to_path_buf).unwrap_or_default()),
    };
    let first = words.remove(0);
    let runs = if first == tool.run.program { program.to_path_buf() } else { toolchains::program_in(&folder, std::slice::from_ref(&first)).ok_or_else(|| format!("{} has no {first} beside {}", tool.name, program.display()))? };
    // The limits go just before the file, where the prover reads its own options.
    let at = words.iter().position(|word| *word == file.display().to_string()).unwrap_or(words.len());
    for (offset, setting) in settings.iter().enumerate() {
        words.insert(at + offset, setting.clone());
    }
    let mut command = Command::new(runs);
    command.args(&words).current_dir(&dir).env("PATH", toolchains::run_path()).stdin(Stdio::null()).stdout(Stdio::piped()).stderr(Stdio::piped());
    crate::runner::quiet(&mut command);
    Ok((command, dir))
}

/// Runs one check, taking at most `patience` seconds, and gives its output, the error stream after
/// the messages, and whether it ran out of time.
fn check(mut command: Command, patience: u64) -> Result<(String, f64, bool), String> {
    let began = Instant::now();
    let mut child = command.spawn().map_err(|error| format!("the prover did not start: {error}"))?;
    let mut stdout = child.stdout.take().expect("piped");
    let mut stderr = child.stderr.take().expect("piped");
    let out = std::thread::spawn(move || {
        let mut text = String::new();
        let _ = stdout.read_to_string(&mut text);
        text
    });
    let err = std::thread::spawn(move || {
        let mut text = String::new();
        let _ = stderr.read_to_string(&mut text);
        text
    });
    let mut late = false;
    loop {
        if child.try_wait().map_err(|error| error.to_string())?.is_some() {
            break;
        }
        if began.elapsed() > Duration::from_secs(patience) {
            let _ = child.kill();
            let _ = child.wait();
            late = true;
            break;
        }
        std::thread::sleep(Duration::from_millis(50));
    }
    let mut text = out.join().unwrap_or_default();
    let errors = err.join().unwrap_or_default();
    if !errors.trim().is_empty() {
        text.push('\n');
        text.push_str(&errors);
    }
    Ok((text, began.elapsed().as_secs_f64(), late))
}

/// Validates the file at `path` with `tool`: checks it, lifts each barrier that stops it as far as
/// its ceiling, and reports what is left, the flags and the axioms.
pub fn validate(tool: &Tool, path: &Path) -> Result<Report, String> {
    let text = std::fs::read_to_string(path).map_err(|error| format!("{}: {error}", path.display()))?;
    let toolchain = toolchains::manifest().into_iter().find(|one| one.id == tool.toolchain).ok_or_else(|| format!("the toolchain manifest has no {}", tool.toolchain))?;
    let found = toolchains::find(&toolchain, &toolchains::path_folders(), &toolchains::chosen());
    let program = found.program.map(PathBuf::from).ok_or_else(|| format!("{} is not found: File, Toolchains opens its install page or takes its folder", toolchain.name))?;
    let project = project_of(path, &tool.run.project);
    let code = code_of(&text);
    let theorems = tool.audit.as_ref().map(|audit| theorems_in(&code, &audit.declarations)).unwrap_or_default();
    let asks: Vec<String> = tool.audit.as_ref().map(|audit| theorems.iter().map(|name| audit.print.replace("{name}", name)).collect()).unwrap_or_default();
    let lines = text.lines().count() as u32;

    let stem = path.file_stem().map(|stem| stem.to_string_lossy().to_string()).unwrap_or_default();
    let copy_path = path.with_file_name(format!(".{stem}.validate.{}.lean", std::process::id()));
    let mut grown: BTreeMap<String, u64> = BTreeMap::new();
    let mut checks = Vec::new();
    let mut first: Option<Vec<Message>> = None;
    let mut last: Vec<Message> = Vec::new();
    let mut stuck: BTreeMap<String, String> = BTreeMap::new();
    let mut own_settings = Vec::new();
    let mut late = false;
    let result = (|| -> Result<(), String> {
        for _ in 0..MOST_CHECKS {
            let option_values: BTreeMap<String, u64> = grown
                .iter()
                .filter_map(|(name, value)| tool.barriers.iter().find(|barrier| &barrier.name == name).and_then(|barrier| barrier.lift.strip_prefix("-D").map(|lift| (lift.split('=').next().unwrap_or_default().to_string(), *value))))
                .collect();
            let (copy, changed) = copy_of(&text, &option_values, &asks);
            own_settings = changed;
            std::fs::write(&copy_path, copy).map_err(|error| format!("{}: {error}", copy_path.display()))?;
            let settings: Vec<String> = grown.iter().filter_map(|(name, value)| tool.barriers.iter().find(|barrier| &barrier.name == name).map(|barrier| barrier.lift.replace("{value}", &value.to_string()))).collect();
            let (command, _) = command_for(tool, &program, &copy_path, project.as_deref(), &settings)?;
            let (output, seconds, ran_late) = check(command, tool.run.patience)?;
            let messages = messages_of(&output);
            let hit: Vec<&Barrier> = messages.iter().filter(|message| message.severity == 1).filter_map(|message| barrier_of(message, &tool.barriers)).collect();
            let mut names: Vec<String> = hit.iter().map(|barrier| barrier.name.clone()).collect();
            names.dedup();
            checks.push(Check { settings: settings.clone(), seconds, errors: messages.iter().filter(|message| message.severity == 1).count(), barriers: names.clone() });
            first.get_or_insert_with(|| messages.clone());
            last = messages;
            late = ran_late;
            if late || names.is_empty() {
                break;
            }
            let mut moved = false;
            for barrier in hit {
                let now = grown.get(&barrier.name).copied().unwrap_or(barrier.default);
                if now >= barrier.ceiling {
                    stuck.insert(barrier.name.clone(), format!("still stops the check at its ceiling, {}", barrier.ceiling));
                    continue;
                }
                let next = (now.saturating_mul(barrier.grow.max(2))).min(barrier.ceiling);
                if grown.get(&barrier.name) != Some(&next) {
                    grown.insert(barrier.name.clone(), next);
                    moved = true;
                }
            }
            if !moved {
                break;
            }
        }
        Ok(())
    })();
    let _ = std::fs::remove_file(&copy_path);
    result?;

    let settings_said = |names: &[String]| -> String {
        let said: Vec<String> = names.iter().filter_map(|name| grown.get(name).map(|value| format!("{name} {value}"))).collect();
        said.join(", ")
    };
    let mut findings = Vec::new();
    let mut lifted_names = Vec::new();
    for message in first.clone().unwrap_or_default() {
        if message.severity != 1 || message.line >= lines {
            continue;
        }
        if let Some(barrier) = barrier_of(&message, &tool.barriers) {
            let still = last.iter().any(|now| now.severity == 1 && now.line == message.line && barrier_of(now, &tool.barriers).map(|one| &one.name) == Some(&barrier.name));
            let lifted = if let Some(why) = stuck.get(&barrier.name) {
                why.clone()
            } else if still {
                "still stops the check once the other limits are lifted".to_string()
            } else {
                lifted_names.push(barrier.name.clone());
                format!("goes through at {}", settings_said(std::slice::from_ref(&barrier.name)))
            };
            findings.push(Finding {
                line: message.line,
                col: message.col,
                end_line: message.end_line,
                end_col: message.end_col,
                severity: if still { 1 } else { 3 },
                kind: "barrier",
                barrier: Some(barrier.name.clone()),
                message: format!("{}: {} stopped the check here: {}", barrier.name, barrier.about, message.text.lines().next().unwrap_or_default()),
                lifted: Some(lifted),
            });
        }
    }
    let mut axioms_found = Vec::new();
    for message in &last {
        if message.line >= lines {
            if let Some((name, axioms)) = axioms_of(&message.text) {
                axioms_found.push((name, axioms));
            }
            continue;
        }
        if barrier_of(message, &tool.barriers).is_some() && message.severity == 1 {
            continue;
        }
        findings.push(Finding {
            line: message.line,
            col: message.col,
            end_line: message.end_line,
            end_col: message.end_col,
            severity: message.severity,
            kind: "math",
            barrier: None,
            message: message.text.clone(),
            lifted: None,
        });
    }
    findings.extend(flags_in(&code, &tool.flags));
    let mut untrusted = Vec::new();
    if let Some(audit) = &tool.audit {
        // A message of a kind the audit says better goes; the audit names the gap with what fills it.
        findings.retain(|finding| finding.kind != "math" || !last.iter().any(|message| message.line == finding.line && message.col == finding.col && audit.repeats.contains(&message.kind)));
        let words = words_of(&code);
        // Each theorem's declaration, and the lines it runs to: up to the next theorem's.
        let mut declared: Vec<(String, u32, u32)> = axioms_found
            .iter()
            .filter_map(|(name, _)| {
                let short = name.rsplit('.').next().unwrap_or(name);
                words.windows(2).find(|pair| audit.declarations.contains(&pair[0].0) && (pair[1].0 == short || pair[1].0 == *name)).map(|pair| (name.clone(), pair[1].1, pair[1].2))
            })
            .collect();
        declared.sort_by_key(|(_, line, _)| *line);
        for (name, axioms) in &axioms_found {
            let others: Vec<&String> = axioms.iter().filter(|axiom| !audit.trusted.contains(axiom)).collect();
            if others.is_empty() {
                continue;
            }
            let Some(at) = declared.iter().position(|(one, _, _)| one == name) else {
                continue;
            };
            let (_, line, col) = declared[at].clone();
            let ends = declared.get(at + 1).map(|(_, next, _)| *next).unwrap_or(u32::MAX);
            // A proof that does not check stands on a gap the prover put there: the error says it.
            let failed = findings.iter().any(|finding| finding.severity == 1 && (finding.kind == "math" || finding.kind == "barrier") && finding.line >= line && finding.line < ends);
            if failed && others.iter().all(|axiom| audit.gaps.contains(axiom)) {
                continue;
            }
            let said: Vec<String> = others
                .iter()
                .map(|axiom| match audit.explains.iter().find(|(part, _)| axiom.contains(part.as_str())) {
                    Some((_, why)) => format!("{axiom}, {why}"),
                    None => axiom.to_string(),
                })
                .collect();
            untrusted.push(name.clone());
            let short = name.rsplit('.').next().unwrap_or(name);
            findings.push(Finding {
                line,
                col,
                end_line: line,
                end_col: col + short.chars().count() as u32,
                severity: if others.iter().any(|axiom| audit.gaps.contains(axiom)) { 1 } else { 2 },
                kind: "axiom",
                barrier: None,
                message: format!("{name} rests on {}", said.join("; ")),
                lifted: None,
            });
        }
    }
    findings.sort_by_key(|finding| (finding.line, finding.col));

    let math = findings.iter().filter(|finding| finding.kind == "math" && finding.severity == 1).count();
    let blocked = findings.iter().filter(|finding| finding.kind == "barrier" && finding.severity == 1).count();
    let gaps = findings.iter().filter(|finding| finding.severity == 1 && (finding.kind == "flag" || finding.kind == "axiom")).count();
    let holds = !late && math == 0 && blocked == 0 && gaps == 0;
    let mut said = Vec::new();
    if late {
        said.push(format!("the check ran past {} seconds and was stopped", tool.run.patience));
    } else if holds {
        said.push(if theorems.is_empty() { "it checks".to_string() } else { format!("{} theorems check", theorems.len()) });
    }
    if math > 0 {
        said.push(format!("{math} {} in the mathematics", if math == 1 { "error" } else { "errors" }));
    }
    if blocked > 0 {
        said.push(format!("{blocked} {} a limit stops past its ceiling", if blocked == 1 { "place" } else { "places" }));
    }
    lifted_names.sort();
    lifted_names.dedup();
    if !lifted_names.is_empty() {
        said.push(format!("once lifted to {}", settings_said(&lifted_names)));
    }
    if !own_settings.is_empty() {
        said.push(format!("the file's own {}", own_settings.join("; ")));
    }
    if !untrusted.is_empty() {
        said.push(format!("{} rest on axioms past {}", untrusted.join(", "), tool.audit.as_ref().map(|audit| audit.trusted.join(", ")).unwrap_or_default()));
    }
    Ok(Report {
        tool: tool.name.clone(),
        file: path.display().to_string(),
        project: project.map(|project| project.display().to_string()),
        holds,
        verdict: said.join("; "),
        checks,
        findings,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn lean() -> Tool {
        tool_for("lean").expect("orior comes with a tool for Lean")
    }

    #[test]
    fn the_lean_tool_reads() {
        let tool = lean();
        assert_eq!(tool.toolchain, "lean");
        assert!(tool.barriers.iter().any(|barrier| barrier.name == "maxHeartbeats"));
    }

    #[test]
    fn comments_and_strings_are_not_code() {
        let code = code_of("def x : Float := 1 -- Float\n/- Float /- nested -/ Float -/ \"Float\" theorem y");
        assert_eq!(code.matches("Float").count(), 1);
        assert!(code.contains("theorem y"));
        assert_eq!(code.lines().count(), 2);
    }

    #[test]
    fn flags_find_their_words() {
        let tool = lean();
        let code = code_of("def f (x : Float) : UInt64 := 0\ntheorem t : 1 = 1 := by native_decide\nexample : True := sorry\n#eval f 1.0");
        let found = flags_in(&code, &tool.flags);
        let words: Vec<String> = found.iter().map(|finding| finding.message.split(':').next().unwrap().to_string()).collect();
        assert_eq!(words, ["Float", "UInt64", "native_decide", "#eval"]);
        assert_eq!(found[0].line, 0);
        assert_eq!(found[0].col, 11);
        assert_eq!(found[3].severity, 3);
    }

    #[test]
    fn theorems_take_their_namespaces() {
        let code = code_of("namespace A\ntheorem one : True := trivial\nnamespace B\nlemma two : True := trivial\nend B\nsection\ntheorem three : True := trivial\nend\nend A\ntheorem four : True := trivial");
        assert_eq!(theorems_in(&code, &["theorem".into(), "lemma".into()]), ["A.one", "A.B.two", "A.three", "four"]);
    }

    #[test]
    fn messages_read_lean_json() {
        let output = concat!(
            r#"{"severity":"error","pos":{"line":3,"column":2},"endPos":{"line":3,"column":9},"keepFullRange":false,"fileName":"x.lean","data":"(deterministic) timeout at `whnf`, maximum number of heartbeats (200000) has been reached","caption":""}"#,
            "\n",
            r#"{"severity":"information","pos":{"line":9,"column":0},"endPos":null,"fileName":"x.lean","data":"'A.one' depends on axioms: [propext, sorryAx]","caption":""}"#,
            "\nStack overflow detected. Aborting.\n"
        );
        let messages = messages_of(output);
        assert_eq!(messages.len(), 3);
        assert_eq!((messages[0].line, messages[0].col, messages[0].end_col), (2, 2, 9));
        assert_eq!(messages[1].severity, 3);
        let tool = lean();
        assert_eq!(barrier_of(&messages[0], &tool.barriers).unwrap().name, "maxHeartbeats");
        assert_eq!(barrier_of(&messages[2], &tool.barriers).unwrap().name, "kernel deep recursion");
        assert_eq!(axioms_of(&messages[1].text), Some(("A.one".to_string(), vec!["propext".to_string(), "sorryAx".to_string()])));
        assert_eq!(axioms_of("'t' does not depend on any axioms"), Some(("t".to_string(), vec![])));
    }

    #[test]
    fn instance_timeouts_are_their_own_barrier() {
        let tool = lean();
        let said = "failed to synthesize\n  Foo\n(deterministic) timeout at `isDefEq`, maximum number of heartbeats (20000) has been reached\nUse `set_option synthInstance.maxHeartbeats <num>` to set the limit.";
        let message = |text: &str, kind: &str| Message { severity: 1, line: 0, col: 0, end_line: 0, end_col: 1, text: text.to_string(), kind: kind.to_string() };
        assert_eq!(barrier_of(&message(said, "runtime.maxHeartbeats"), &tool.barriers).unwrap().name, "synthInstance.maxHeartbeats");
        assert_eq!(barrier_of(&message("anything", "runtime.maxRecDepth"), &tool.barriers).unwrap().name, "maxRecDepth");
        assert!(barrier_of(&message("type mismatch\n  h\nhas type", "[anonymous]"), &tool.barriers).is_none());
    }

    #[test]
    fn the_copy_grows_the_files_own_limits() {
        let grown = BTreeMap::from([("maxHeartbeats".to_string(), 800000u64)]);
        let (copy, changed) = copy_of("set_option maxHeartbeats 400000 in\ntheorem t : True := trivial", &grown, &["#print axioms t".to_string()]);
        assert!(copy.starts_with("set_option maxHeartbeats 800000 in\n"));
        assert!(copy.ends_with("#print axioms t\n"));
        assert_eq!(changed, ["line 1: set_option maxHeartbeats 400000, checked at 800000"]);
        let (kept, none) = copy_of("set_option maxHeartbeats 0 in\n", &grown, &[]);
        assert!(kept.contains("maxHeartbeats 0 in") && none.is_empty());
    }
}

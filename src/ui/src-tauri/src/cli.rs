// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The command line: the window's jobs and bridge, read and run from a terminal by the same program.
//!
//! Every job the window lists is a job here, read from the tree the same way, and a run goes through
//! the same runner with the same checks. The steps share the terminal, its output streams as it
//! comes, and the program exits with the job's code.

use std::collections::HashMap;
use std::io::Write;
use std::path::PathBuf;
use std::sync::{Arc, Mutex};

use crate::bridge::{self, Bridge};
use crate::catalog::{self, Job};
use crate::root;
use crate::runner::{self, Said, Sink};

const HELP: &str = "\
orior: the window, and every job it runs, from one program

  orior                          open the window
  orior list [word]              the jobs, or those whose id, title or about holds the word
  orior show <job>               a job's file, values and the commands it runs
  orior run <job> [key=value]    run a job; a key given twice gives two values
  orior run <job> -- <words>     the words after -- are the job's arguments
  orior bridge [key]             the keys of Lstar.klq, or one key's pairs, maps and rulesets
  orior help                     this

  --root <folder>                the orior tree to work on; else ORIOR_ROOT, else the tree the
                                 working folder or the program sits in

A job is named by its id, by the end of its id after a slash, or by its title, where that names
only one. A run exits with the code of its last step.
";

/// The code a run exits with where a step could not start or gave no code.
const NO_CODE: i32 = 1;
/// The code for words the program does not take.
const WRONG: i32 = 2;

fn out(text: &str) {
    let _ = writeln!(std::io::stdout().lock(), "{text}");
}

fn err(text: &str) {
    let _ = writeln!(std::io::stderr().lock(), "{text}");
}

fn width() -> usize {
    std::env::var("COLUMNS").ok().and_then(|c| c.parse().ok()).filter(|&c| c >= 40).unwrap_or(100)
}

/// `text` cut to `most` characters, ending in `...` where it was cut.
fn cut(text: &str, most: usize) -> String {
    if text.chars().count() <= most {
        return text.to_string();
    }
    let kept: String = text.chars().take(most.saturating_sub(3)).collect();
    format!("{}...", kept.trim_end())
}

/// `text` broken into lines of at most `most` characters at spaces, each led by `lead`.
fn wrap(text: &str, lead: &str, most: usize) -> Vec<String> {
    let mut lines = Vec::new();
    let mut line = String::new();
    for word in text.split_whitespace() {
        if !line.is_empty() && lead.len() + line.len() + 1 + word.len() > most {
            lines.push(format!("{lead}{line}"));
            line.clear();
        }
        if !line.is_empty() {
            line.push(' ');
        }
        line.push_str(word);
    }
    if !line.is_empty() {
        lines.push(format!("{lead}{line}"));
    }
    lines
}

/// A word quoted so `runner::shell_words` gives it back whole.
fn quoted(word: &str) -> String {
    format!("\"{}\"", word.replace('\\', "\\\\").replace('"', "\\\""))
}

/// The job a word names: the one whose id it is, else the only one whose id ends in it after a slash
/// or whose title it is.
fn job_named(jobs: &[Job], name: &str) -> Result<Job, String> {
    if let Some(job) = jobs.iter().find(|job| job.id == name) {
        return Ok(job.clone());
    }
    let tail = format!("/{name}");
    let found: Vec<&Job> = jobs.iter().filter(|job| job.id.ends_with(&tail) || job.title == name).collect();
    match found.as_slice() {
        [] => Err(format!("no job {name}: orior list names them")),
        [job] => Ok((*job).clone()),
        many => {
            let ids: Vec<&str> = many.iter().map(|job| job.id.as_str()).collect();
            Err(format!("{name} names {} jobs; give one of:\n  {}", many.len(), ids.join("\n  ")))
        }
    }
}

/// The values a run's words give: `key=value` for each, a key given twice holding both, and the
/// words after `--` as the job's free words. Every param not given takes its default.
fn values_of(job: &Job, words: &[String]) -> Result<HashMap<String, Vec<String>>, String> {
    let keys: Vec<&str> = job.params.iter().map(|p| p.key.as_str()).collect();
    let known = |key: &str| {
        if keys.contains(&key) {
            Ok(())
        } else if keys.is_empty() {
            Err(format!("{} takes no values", job.id))
        } else {
            Err(format!("{} takes no {key}; it takes {}", job.id, keys.join(", ")))
        }
    };
    let mut values: HashMap<String, Vec<String>> = HashMap::new();
    let mut words = words.iter();
    while let Some(word) = words.next() {
        if word == "--" {
            known("arguments")?;
            let free: Vec<String> = words.by_ref().map(|w| quoted(w)).collect();
            values.entry("arguments".into()).or_default().push(free.join(" "));
            break;
        }
        let Some((key, value)) = word.split_once('=') else {
            return Err(format!("{word} is not key=value; words for the job go after --"));
        };
        known(key)?;
        values.entry(key.to_string()).or_default().push(value.to_string());
    }
    for param in &job.params {
        if !param.default.is_empty() {
            values.entry(param.key.clone()).or_insert_with(|| vec![param.default.clone()]);
        }
    }
    Ok(values)
}

fn list(root: &std::path::Path, word: Option<&str>) -> i32 {
    let word = word.map(str::to_lowercase);
    let jobs: Vec<Job> = catalog::read(root)
        .into_iter()
        .filter(|job| {
            let Some(word) = &word else { return true };
            [&job.id, &job.title, &job.about].iter().any(|text| text.to_lowercase().contains(word.as_str()))
        })
        .collect();
    if jobs.is_empty() {
        err("no job holds that word");
        return NO_CODE;
    }
    let pad = jobs.iter().map(|job| job.id.chars().count()).max().unwrap_or(0).min(48);
    let room = width().saturating_sub(pad + 2);
    for job in &jobs {
        let about = if room >= 20 { cut(&job.about, room) } else { String::new() };
        out(format!("{:pad$}  {about}", job.id).trim_end());
    }
    0
}

fn show(root: &std::path::Path, job: &Job) {
    let most = width();
    out(&job.id);
    out(&format!("  title  {}", job.title));
    out(&format!("  file   {}", job.file));
    if !job.about.is_empty() {
        out("");
        for line in wrap(&job.about, "  ", most) {
            out(&line);
        }
    }
    if !job.params.is_empty() {
        out("");
        out("values");
        for param in &job.params {
            let mut line = format!("  {}  {}", param.key, param.kind);
            if param.required {
                line.push_str("  required");
            }
            if !param.default.is_empty() {
                line.push_str(&format!("  default {}", param.default));
            }
            out(&line);
            if !param.choices.is_empty() {
                for line in wrap(&param.choices.join(", "), "      ", most) {
                    out(&line);
                }
            }
        }
    }
    out("");
    out("runs");
    match values_of(job, &[]).and_then(|values| runner::shown(root, job, &values)) {
        Ok(commands) => {
            for command in commands {
                out(&format!("  $ {command}"));
            }
        }
        Err(said) => out(&format!("  {said}")),
    }
}

fn run_job(root: PathBuf, job: Job, words: &[String]) -> i32 {
    let values = match values_of(&job, words) {
        Ok(values) => values,
        Err(said) => {
            err(&said);
            return WRONG;
        }
    };
    let ended: Arc<Mutex<Option<runner::End>>> = Arc::default();
    let keep = ended.clone();
    let sink: Sink = Arc::new(move |said| match said {
        Said::Line(line) => match line.stream {
            "stdout" => out(&line.text),
            "command" => err(&format!("$ {}", line.text)),
            _ => err(&line.text),
        },
        Said::End(end) => {
            for page in &end.views {
                err(&format!("page {page}"));
            }
            if let Ok(mut kept) = keep.lock() {
                *kept = Some(end);
            }
        }
    });
    let runs = runner::Runs::attached();
    let waits = match runs.start(sink, root, job, values) {
        Ok((_, waits)) => waits,
        Err(said) => {
            err(&said);
            return WRONG;
        }
    };
    let _ = waits.join();
    let end = ended.lock().ok().and_then(|mut e| e.take());
    end.and_then(|end| end.code).unwrap_or(NO_CODE)
}

fn tally(key: &bridge::Key) -> String {
    let open = key.pairs.iter().filter(|p| p.verdict.as_ref().is_some_and(|v| v.word == "open")).count();
    let closed = key.pairs.iter().filter(|p| p.verdict.as_ref().is_some_and(|v| v.word == "closed")).count();
    if key.pairs.is_empty() {
        String::new()
    } else {
        format!("{} pairs, {open} open, {closed} closed", key.pairs.len())
    }
}

/// One key of the bridge: its pairs with their verdicts and notes, the name each .klm gives it, and
/// each ruleset's entries of it, every line with the file and line it is read from.
fn show_key(bridge: &Bridge, klq: &bridge::Klq, name: &str) {
    let at = |path: &str, line: usize| format!("{path}:{}", line + 1);
    let key = &klq.keys[name];
    out(&format!("{name}  {}", at(&klq.path, key.line)));
    if key.pairs.is_empty() {
        out("  no pairs");
    }
    for pair in &key.pairs {
        let verdict = pair.verdict.as_ref().map(|v| format!("{} {}", v.word, v.rest)).unwrap_or_default();
        out(format!("  {} ~ {}  {}  {}", pair.first, pair.second, verdict.trim(), at(&klq.path, pair.line)).trim_end());
        for note in &pair.notes {
            out(format!("    {} {}", note.word, note.rest).trim_end());
        }
    }
    let mut maps = Vec::new();
    for map in &bridge.maps {
        for (named, entry) in &map.names {
            if entry.key != name && named != name {
                continue;
            }
            let shown = if !entry.key.is_empty() && entry.key != *named { format!("{named} -> {}", entry.key) } else { named.clone() };
            let mut line = format!("  {}  {shown}", map.language);
            if let Some(kind) = &entry.kind {
                line.push_str(&format!("  {}", kind.rest));
            }
            for broken in &entry.breaks {
                line.push_str(&format!("  breaks {}", broken.rest));
            }
            maps.push(format!("{line}  {}", at(&map.path, entry.line)));
        }
    }
    if !maps.is_empty() {
        out("maps");
        maps.iter().for_each(|line| out(line));
    }
    let mut ruled = Vec::new();
    for ruleset in &bridge.rulesets {
        let file = ruleset.path.rsplit('/').next().unwrap_or(&ruleset.path);
        for entry in ruleset.entries.get(name).into_iter().flatten() {
            ruled.push(format!("  {file}  {}  {}", entry.kind, at(&ruleset.path, entry.line)));
        }
    }
    if !ruled.is_empty() {
        out("rulesets");
        ruled.iter().for_each(|line| out(line));
    }
}

fn bridge(root: &std::path::Path, word: Option<&str>) -> i32 {
    let read = bridge::read(root);
    let Some(klq) = &read.klq else {
        err(&format!("no Lstar.klq in {}", bridge::DIR));
        return NO_CODE;
    };
    if let Some(name) = word.filter(|name| klq.keys.contains_key(*name)) {
        show_key(&read, klq, name);
        return 0;
    }
    let word = word.map(str::to_lowercase);
    let names: Vec<&String> = klq.keys.keys().filter(|name| word.as_ref().map_or(true, |w| name.to_lowercase().contains(w.as_str()))).collect();
    if names.is_empty() {
        err("no key holds that word");
        return NO_CODE;
    }
    let pad = names.iter().map(|name| name.chars().count()).max().unwrap_or(0);
    for name in names {
        out(format!("{name:pad$}  {}", tally(&klq.keys[name])).trim_end());
    }
    0
}

/// The tree a run works on: the folder `--root` names, else the one `root::find` finds.
fn tree(named: Option<String>) -> Result<PathBuf, String> {
    match named {
        Some(dir) => {
            let path = dunce::canonicalize(&dir).map_err(|e| format!("{dir}: {e}"))?;
            if root::holds_tree(&path) {
                Ok(path)
            } else {
                Err(format!("{} holds no orior tree", path.display()))
            }
        }
        None => root::find().ok_or_else(|| "no orior tree here: run from inside one, or name one with --root".to_string()),
    }
}

/// Runs the words given on the command line and returns the code to exit with.
pub fn run(given: Vec<String>) -> i32 {
    let mut named = None;
    let mut words = Vec::new();
    let mut given = given.into_iter();
    while let Some(word) = given.next() {
        if word == "--" {
            words.push(word);
            words.extend(given.by_ref());
        } else if word == "--root" {
            let Some(dir) = given.next() else {
                err("--root needs a folder");
                return WRONG;
            };
            named = Some(dir);
        } else if let Some(dir) = word.strip_prefix("--root=") {
            named = Some(dir.to_string());
        } else {
            words.push(word);
        }
    }
    let first = words.first().map(String::as_str).unwrap_or("help");
    if matches!(first, "help" | "-h" | "--help") {
        out(HELP.trim_end());
        return 0;
    }
    if matches!(first, "--version" | "-V") {
        out(&format!("orior {}", env!("CARGO_PKG_VERSION")));
        return 0;
    }
    if !matches!(first, "list" | "show" | "run" | "bridge") {
        err(&format!("orior takes no {first}: orior help names what it takes"));
        return WRONG;
    }
    let root = match tree(named) {
        Ok(root) => root,
        Err(said) => {
            err(&said);
            return NO_CODE;
        }
    };
    let second = words.get(1).map(String::as_str);
    match first {
        "list" => list(&root, second),
        "bridge" => bridge(&root, second),
        _ => {
            let Some(name) = second else {
                err(&format!("orior {first} needs a job: orior list names them"));
                return WRONG;
            };
            let job = match job_named(&catalog::read(&root), name) {
                Ok(job) => job,
                Err(said) => {
                    err(&said);
                    return NO_CODE;
                }
            };
            if first == "show" {
                show(&root, &job);
                0
            } else {
                run_job(root, job, &words[2..])
            }
        }
    }
}

/// Lets go of the console where this process is the only one on it: the console Windows opens for a
/// console program started from outside a terminal. Started from a terminal, the process shares the
/// terminal's console and keeps it.
#[cfg(windows)]
pub fn console_let_go() {
    #[link(name = "kernel32")]
    extern "system" {
        fn GetConsoleProcessList(list: *mut u32, count: u32) -> u32;
        fn FreeConsole() -> i32;
    }
    let mut list = [0u32; 2];
    // SAFETY: the list is two u32s long and the count given says so; FreeConsole takes nothing.
    unsafe {
        if GetConsoleProcessList(list.as_mut_ptr(), list.len() as u32) == 1 {
            FreeConsole();
        }
    }
}

#[cfg(not(windows))]
pub fn console_let_go() {}

#[cfg(test)]
mod words {
    use super::*;
    use crate::catalog::Param;

    fn job(id: &str, title: &str, params: Vec<Param>) -> Job {
        Job { id: id.into(), group: "test", title: title.into(), file: String::new(), about: String::new(), params, opens: "", steps: Vec::new() }
    }

    fn param(key: &str, kind: &'static str, default: &str) -> Param {
        Param { key: key.into(), kind, choices: Vec::new(), default: default.into(), required: false }
    }

    #[test]
    fn a_job_is_named_by_id_tail_or_title() {
        let jobs = vec![job("build/src/build_engine.sh", "src/build_engine.sh", vec![]), job("sim/wave", "wave", vec![]), job("view/a/build_wave_view.py", "wave", vec![])];
        assert_eq!(job_named(&jobs, "build_engine.sh").unwrap().id, "build/src/build_engine.sh");
        assert_eq!(job_named(&jobs, "sim/wave").unwrap().id, "sim/wave");
        assert!(job_named(&jobs, "wave").err().unwrap().contains("2 jobs"));
        assert!(job_named(&jobs, "none").is_err());
    }

    #[test]
    fn values_take_keys_free_words_and_defaults() {
        let held = job("run/x", "x", vec![param("cfg", "choice", "a.cfg"), param("run", "many", ""), param("arguments", "words", "")]);
        let words: Vec<String> = ["run=one", "run=two", "--", "--steps", "a b", "say \"hi\""].iter().map(|w| w.to_string()).collect();
        let values = values_of(&held, &words).unwrap();
        assert_eq!(values["cfg"], ["a.cfg"]);
        assert_eq!(values["run"], ["one", "two"]);
        assert_eq!(runner::shell_words(&values["arguments"][0]).unwrap(), ["--steps", "a b", "say \"hi\""]);
        assert!(values_of(&held, &["nope=1".to_string()]).is_err());
        assert!(values_of(&held, &["bare".to_string()]).is_err());
    }
}

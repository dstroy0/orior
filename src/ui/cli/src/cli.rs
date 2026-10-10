// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The command line: the window's menus as words. `orior <menu> <command>` is the command of that
//! menu, `orior <menu> <job>` the job of a menu of jobs, and the help is the menus listed.
//!
//! A command the terminal can do, starting a job, listing, showing, the bridge, the keys and the
//! version, runs here. Every other acts in the window, and the words come to the window to open with
//! the command to run in it. Every job the window lists is a job here, read from the tree the same
//! way, and a run goes through the same runner with the same checks. The steps share the terminal,
//! its output streams as it comes, and the program exits with the job's code.

use std::collections::HashMap;
use std::io::{IsTerminal, Write};
use std::path::PathBuf;
use std::sync::{Arc, Mutex};

use crate::bridge::{self, Bridge};
use crate::catalog::{self, Job};
use crate::commands::{self, Commands, Item, Menu};
use crate::files;
use crate::format;
use crate::git;
use crate::home;
use crate::plugins;
use crate::report;
use crate::root;
use crate::run_file;
use crate::runner::{self, Said, Sink};
use crate::toolchains;
use crate::validate;

/// The code a run exits with where a step could not start or gave no code.
const NO_CODE: i32 = 1;
/// The code for words the program does not take.
pub const WRONG: i32 = 2;

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
    // Written to a terminal, each about is cut to the line; written anywhere else, it is whole.
    let terminal = std::io::stdout().is_terminal();
    for job in &jobs {
        let about = match (terminal, room >= 20) {
            (false, _) => job.about.clone(),
            (true, true) => cut(&job.about, room),
            (true, false) => String::new(),
        };
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
    let names: Vec<&String> = klq.keys.keys().filter(|name| word.as_ref().is_none_or(|w| name.to_lowercase().contains(w.as_str()))).collect();
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

/// What the command line's words come to: a code to exit with, or the window to open, on a tree and
/// with a command of its menus to run once it is up.
pub enum Outcome {
    Exit(i32),
    Window(Launch),
}

/// The window a command of its menus opens: the tree to open it on, where one was named, and the
/// command with its words. With no command, the window opens and runs none.
#[derive(Clone, Default, serde::Serialize)]
pub struct Launch {
    pub root: Option<PathBuf>,
    pub menu: String,
    pub command: String,
    pub args: Vec<String>,
}

impl Launch {
    /// The words that name the command, as the command line takes them.
    pub fn words(&self) -> String {
        [self.menu.as_str(), self.command.as_str()].into_iter().chain(self.args.iter().map(String::as_str)).filter(|w| !w.is_empty()).collect::<Vec<_>>().join(" ")
    }
}

/// The widest a usage in the help's left column is; a wider one stands on lines of its own, its
/// label under it in the column.
const HELP_COLUMN: usize = 44;

/// The width the help's lines are wrapped to.
const HELP_WIDTH: usize = 100;

/// `text` broken at spaces into lines of at most `width` characters, each after the first indented
/// `indent` more. A word wider than the width stands alone on its line.
fn wrapped(text: &str, width: usize, indent: usize) -> Vec<String> {
    let mut lines: Vec<String> = Vec::new();
    let mut line = String::new();
    for word in text.split(' ') {
        let lead = if lines.is_empty() { 0 } else { indent };
        if !line.is_empty() && lead + line.chars().count() + 1 + word.chars().count() > width {
            lines.push(format!("{:lead$}{line}", ""));
            line.clear();
        }
        if !line.is_empty() {
            line.push(' ');
        }
        line.push_str(word);
    }
    let lead = if lines.is_empty() { 0 } else { indent };
    lines.push(format!("{:lead$}{line}", ""));
    lines
}

/// The help, made from the menus: a line for each command, the ones that act in the window marked.
fn help(menus: &Commands) -> String {
    let mut rows: Vec<(String, String)> = vec![("orior".into(), "open the window".into())];
    for menu in &menus.menus {
        let word = menu.word();
        for item in menu.commands() {
            let usage = format!("orior {word} {} {}", item.command, item.args);
            let mark = if item.console { "" } else { "* " };
            rows.push((usage.trim_end().to_string(), format!("{mark}{}", item.label)));
        }
        if !menu.groups.is_empty() {
            let usage = match menu.split.as_deref() {
                Some(split) => format!("orior {word} [{split}] [job]"),
                None => format!("orior {word} [job]"),
            };
            rows.push((usage, format!("the {} jobs, or one of them", menu.groups.join(" and "))));
        }
    }
    rows.push(("orior run <job> [key=value] [-- words]".into(), "start a job, as orior run start".into()));
    rows.push(("orior list | show <job> | bridge [key]".into(), "as orior run list, run show and go bridge".into()));
    rows.push(("orior <file>[:line[:column]]".into(), "* open a file of the tree in the window, at a line and column".into()));
    rows.push(("orior --completions <shell>".into(), "the completions for bash, zsh, fish or powershell".into()));
    rows.push(("orior help".into(), "this".into()));
    let pad = rows.iter().map(|(usage, _)| usage.chars().count()).filter(|&wide| wide <= HELP_COLUMN).max().unwrap_or(HELP_COLUMN);
    let mut text = String::from(
        "orior: the window, and every job it runs, from one program. A menu's title and one of its\n\
         commands are the words for it here. A command marked * acts in the window, which it opens.\n\n",
    );
    for (usage, said) in rows {
        let alone = usage.chars().count() > pad;
        if alone {
            for line in wrapped(&usage, HELP_WIDTH - 2, 2) {
                text.push_str(&format!("  {line}\n"));
            }
        }
        for (at, line) in wrapped(&said, HELP_WIDTH - pad - 4, 0).iter().enumerate() {
            let left = if at == 0 && !alone { usage.as_str() } else { "" };
            text.push_str(&format!("  {left:pad$}  {line}\n"));
        }
    }
    text.push_str(
        "\n  --root <folder>  the orior tree to work on; else ORIOR_ROOT, else the tree the working\n\
         \x20                  folder or the program sits in\n\n\
         A job is named by its id, by the end of its id after a slash, or by its title, where that\n\
         names only one. A run exits with the code of its last step. A key given twice gives two values.\n",
    );
    text
}

/// The keys of every command that has some, by menu.
fn keys(menus: &Commands) {
    for menu in &menus.menus {
        let rows: Vec<&Item> = menu.commands().filter(|item| !item.keys.is_empty()).collect();
        if rows.is_empty() {
            continue;
        }
        out(&menu.title);
        let pad = rows.iter().map(|item| item.label.chars().count()).max().unwrap_or(0);
        for item in rows {
            out(&format!("  {:pad$}  {}", item.label, item.keys));
        }
    }
}

/// A menu's commands, and the jobs of a menu of jobs, as `orior <menu>` alone shows them.
fn menu_lines(root: Option<&std::path::Path>, menu: &Menu) -> i32 {
    let word = menu.word();
    for item in menu.commands() {
        let usage = format!("orior {word} {} {}", item.command, item.args);
        let mark = if item.console { "" } else { "  *" };
        out(&format!("{}{mark}", usage.trim_end()));
    }
    if menu.groups.is_empty() {
        return 0;
    }
    let Some(root) = root else {
        err("no orior tree here: run from inside one, or name one with --root");
        return NO_CODE;
    };
    let jobs: Vec<Job> = catalog::read(root).into_iter().filter(|job| menu.groups.iter().any(|g| g == job.group)).collect();
    if menu.split.is_some() {
        let mut subjects: Vec<String> = jobs.iter().map(subject).collect();
        subjects.dedup();
        subjects.iter().for_each(|name| out(name));
    } else {
        jobs.iter().for_each(|job| out(&job.id));
    }
    0
}

/// What a stage job works on, the second part of its file's path.
pub fn subject(job: &Job) -> String {
    job.file.split('/').nth(1).unwrap_or(&job.file).to_string()
}

/// The tree a command works on: the folder `--root` names, else the one `root::find` finds.
fn tree(named: Option<&str>) -> Result<PathBuf, String> {
    match named {
        Some(dir) => {
            let path = dunce::canonicalize(dir).map_err(|e| format!("{dir}: {e}"))?;
            if root::holds_tree(&path) {
                Ok(path)
            } else {
                Err(format!("{} holds no orior tree", path.display()))
            }
        }
        None => root::find().ok_or_else(|| "no orior tree here: run from inside one, or name one with --root".to_string()),
    }
}

/// A command the command line runs itself, in the terminal.
fn console(command: &str, named: Option<&str>, words: &[String], menus: &Commands) -> i32 {
    match command {
        "keys" => {
            keys(menus);
            return 0;
        }
        "about" => {
            out(&format!("orior {}", env!("CARGO_PKG_VERSION")));
            if let Ok(root) = tree(named) {
                out(&root.display().to_string());
            }
            return 0;
        }
        "auto-report" => return auto_report(words.first().map(String::as_str)),
        "search" => return search(named, words),
        "plugins" => return plugins_list(),
        "new-plugin" => return new_plugin(words),
        "user-css" => return user_css(),
        "toolchains" => return toolchains_words(words),
        "clone" => return clone_words(words),
        "format" => return format_files(words),
        "run-file" => return run_file_words(named, words),
        "validate" => return validate_files(words),
        "report" => return report_page(named, words),
        _ => {}
    }
    let root = match tree(named) {
        Ok(root) => root,
        Err(said) => {
            err(&said);
            return NO_CODE;
        }
    };
    let first = words.first().map(String::as_str);
    match command {
        "list" => list(&root, first),
        "bridge" => bridge(&root, first),
        _ => {
            let Some(name) = first else {
                err(&format!("{command} needs a job: orior run list names them"));
                return WRONG;
            };
            let job = match job_named(&catalog::read(&root), name) {
                Ok(job) => job,
                Err(said) => {
                    err(&said);
                    return NO_CODE;
                }
            };
            if command == "show" {
                show(&root, &job);
                0
            } else {
                run_job(root, job, &words[1..])
            }
        }
    }
}

/// `orior file plugins`: every plugin, its id, where it comes from, its name and the extensions it
/// opens. One of the reader's that stands in for one that comes with orior is marked so.
fn plugins_list() -> i32 {
    let found = plugins::all();
    let users: std::collections::HashSet<String> = found.iter().filter(|plugin| plugin.source == "user").map(|plugin| plugin.id.clone()).collect();
    let mut rows = Vec::new();
    for plugin in &found {
        let read: Result<serde_json::Value, _> = serde_json::from_str(&plugin.text);
        let (name, opens) = match &read {
            Ok(value) => (
                value["name"].as_str().unwrap_or(&plugin.id).to_string(),
                value["extensions"].as_array().map(|list| list.iter().filter_map(|ext| ext.as_str()).map(|ext| format!(".{ext}")).collect::<Vec<_>>().join(" ")).unwrap_or_default(),
            ),
            Err(error) => (format!("does not read: {error}"), String::new()),
        };
        let source = if plugin.source == "bundled" && users.contains(&plugin.id) { "bundled, replaced" } else { plugin.source };
        rows.push((plugin.id.clone(), source.to_string(), name, opens));
    }
    let wide = |pick: fn(&(String, String, String, String)) -> &String| rows.iter().map(|row| pick(row).len()).max().unwrap_or(0);
    let (a, b, c) = (wide(|row| &row.0), wide(|row| &row.1), wide(|row| &row.2));
    for (id, source, name, opens) in &rows {
        out(&format!("{id:a$}  {source:b$}  {name:c$}  {opens}"));
    }
    if let Some(dir) = home::plugins() {
        out(&format!("\nthe reader's plugins: {}", dir.display()));
    }
    0
}

/// `orior file new-plugin <name> --ext <ext,...> ...`: writes a language plugin to the reader's
/// plugins folder, as plugins.rs says, and names the folder.
fn new_plugin(words: &[String]) -> i32 {
    let mut spec = plugins::Spec::default();
    let mut name = Vec::new();
    let mut replace = false;
    let list = |text: &str| text.split(',').map(|word| word.trim().to_string()).filter(|word| !word.is_empty()).collect::<Vec<_>>();
    let mut at = 0;
    while at < words.len() {
        let word = words[at].as_str();
        let value = words.get(at + 1).map(String::as_str);
        let taken = match (word, value) {
            ("--replace", _) => {
                replace = true;
                0
            }
            ("--ext", Some(value)) => {
                spec.extensions = list(value);
                1
            }
            ("--id", Some(value)) => {
                spec.id = value.to_string();
                1
            }
            ("--from", Some(value)) => {
                spec.from = value.to_string();
                1
            }
            ("--line-comment", Some(value)) => {
                spec.line_comment = value.to_string();
                1
            }
            ("--block-comment", Some(open)) => match words.get(at + 2) {
                Some(close) => {
                    spec.block_comment = vec![open.to_string(), close.clone()];
                    2
                }
                None => {
                    err("--block-comment takes two words, the opening and the closing");
                    return WRONG;
                }
            },
            ("--keywords", Some(value)) => {
                spec.keywords = list(value);
                1
            }
            ("--types", Some(value)) => {
                spec.types = list(value);
                1
            }
            ("--constants", Some(value)) => {
                spec.constants = list(value);
                1
            }
            ("--quotes", Some(value)) => {
                spec.quotes = list(value);
                1
            }
            (flag, None) if flag.starts_with("--") => {
                err(&format!("{flag} needs a value"));
                return WRONG;
            }
            (flag, _) if flag.starts_with("--") => {
                err(&format!("new-plugin takes no {flag}: orior help names what it takes"));
                return WRONG;
            }
            _ => {
                name.push(word.to_string());
                0
            }
        };
        at += 1 + taken;
    }
    spec.name = name.join(" ");
    match plugins::create(&spec, replace) {
        Ok(folder) => {
            out(&folder.display().to_string());
            0
        }
        Err(said) => {
            err(&said);
            WRONG
        }
    }
}

/// `orior file toolchains [install <tool> | add-path <tool|orior> | use <tool> <folder> | forget <tool>
/// | add <name> --group <group> --program <name,...> [--for <text>] [--id <id>] [--version <word,...>]
/// [--install <url>] | add-group <group> | remove <tool> | remove-group <group>]`: with no words, every
/// toolchain as toolchains.rs finds it, a group at a time, with its version where it says one, and
/// whether orior itself is on the PATH. `install` opens a tool's install page, `add-path` puts the
/// folder its program was found in, or orior's own, on the reader's PATH, `use` has orior run a tool
/// from a folder, and `forget` drops that folder. `add` and `add-group` add a toolchain or a group of
/// the reader's, and `remove` and `remove-group` take one out again.
fn toolchains_words(words: &[String]) -> i32 {
    let word = |at: usize| words.get(at).map(String::as_str);
    let done = |said: Result<String, String>| match said {
        Ok(text) => {
            out(&text);
            0
        }
        Err(text) => {
            err(&text);
            NO_CODE
        }
    };
    if word(0) == Some("add") {
        return done(add_toolchain(&words[1..]).map(|id| format!("added {id}: orior file toolchains lists it")));
    }
    match (word(0), word(1), word(2)) {
        (None, _, _) | (Some("check"), None, _) => {}
        (Some("add-group"), Some(name), None) => return done(toolchains::add_group(name).map(|()| format!("added the group {name}"))),
        (Some("remove"), Some(id), None) => return done(toolchains::remove(id).map(|()| format!("took out {id}"))),
        (Some("remove-group"), Some(name), None) => return done(toolchains::remove_group(name).map(|()| format!("took out the group {name}"))),
        // A tool its makers give a line to install it by is installed here, in this terminal; any other
        // has its install page opened.
        (Some("install"), Some(id), None) => {
            let Ok(line) = toolchains::setup_line(id) else {
                return done(toolchains::open_install(id).map(|url| format!("opened {url}")));
            };
            let bash = match runner::bash() {
                Ok(bash) => bash,
                Err(said) => {
                    err(&said);
                    return NO_CODE;
                }
            };
            err(&format!("$ {line}"));
            return match std::process::Command::new(bash).arg("-c").arg(&line).env("PATH", toolchains::run_path()).status() {
                Ok(status) => status.code().unwrap_or(NO_CODE),
                Err(error) => {
                    err(&error.to_string());
                    NO_CODE
                }
            };
        }
        (Some("add-path"), Some(what), None) => {
            return done(toolchains::add_to_path(what).map(|folder| format!("{folder} is on your PATH for every terminal started from now")));
        }
        (Some("use"), Some(id), Some(folder)) if words.len() == 3 => return done(toolchains::choose(id, folder).map(|program| format!("orior runs {program}"))),
        (Some("forget"), Some(id), None) => return done(toolchains::forget(id).map(|()| format!("orior looks for {id} on the PATH again"))),
        _ => {
            err("toolchains takes check, install <tool>, add-path <tool|orior>, use <tool> <folder>, forget <tool>, add <name> --group <group> --program <name,...>, add-group <group>, remove <tool> or remove-group <group>");
            return WRONG;
        }
    }
    let found = toolchains::check();
    let versions = toolchains::versions(&found);
    let wide = found.iter().map(|one| one.id.len()).max().unwrap_or(0);
    for group in toolchains::groups() {
        out(&format!("\n{group}"));
        let held: Vec<&toolchains::Found> = found.iter().filter(|one| one.group == group).collect();
        if held.is_empty() {
            out("  no toolchain yet");
        }
        for one in held {
            print_toolchain(one, &versions, wide);
        }
    }
    match toolchains::own() {
        Ok(own) if own.on_path => out(&format!("\norior is on PATH: {}", own.folder)),
        Ok(own) => out(&format!("\norior is not on PATH: orior file toolchains add-path orior adds {}", own.folder)),
        Err(said) => err(&said),
    }
    0
}

/// The toolchain `add` describes: its name, then each flag and its value.
fn add_toolchain(words: &[String]) -> Result<String, String> {
    let Some(name) = words.first().filter(|name| !name.starts_with("--")) else {
        return Err("add takes the toolchain's name first".into());
    };
    let list = |value: &str| value.split(',').map(|part| part.trim().to_string()).filter(|part| !part.is_empty()).collect::<Vec<_>>();
    let mut tool: toolchains::Tool = serde_json::from_value(serde_json::json!({ "id": "", "name": name, "group": "", "for": "", "programs": [] })).map_err(|error| error.to_string())?;
    let mut at = 1;
    while at < words.len() {
        let Some(value) = words.get(at + 1) else {
            return Err(format!("{} takes a value", words[at]));
        };
        match words[at].as_str() {
            "--group" => tool.group = value.clone(),
            "--program" => tool.programs = list(value),
            "--for" => tool.uses = value.clone(),
            "--id" => tool.id = value.clone(),
            "--version" => tool.version = Some(list(value)),
            "--install" => {
                tool.install.insert("any".into(), value.clone());
            }
            other => return Err(format!("add knows no {other}")),
        }
        at += 2;
    }
    toolchains::add(tool)
}

fn print_toolchain(one: &toolchains::Found, versions: &std::collections::HashMap<String, String>, wide: usize) {
    {
        let state = match one.state {
            "env" => "named",
            "chosen" => "chosen",
            "path" => "on PATH",
            "found" => "not on PATH",
            _ => "missing",
        };
        let detail = match (&one.program, versions.get(&one.id)) {
            (Some(program), Some(version)) => format!("{version}  {program}"),
            (Some(program), None) => program.clone(),
            (None, _) => one.install.clone().map(|url| format!("install: {url}")).unwrap_or_default(),
        };
        out(&format!("  {:wide$}  {state:11}  {detail}", one.id));
    }
}

/// `orior run run-file <file>`: runs a file with its language's toolchain, as run_file.rs gives the
/// line, in bash at the tree's top folder, attached to this terminal, and exits with its code.
fn run_file_words(named: Option<&str>, words: &[String]) -> i32 {
    let [file] = words else {
        err("run-file takes the one file to run");
        return WRONG;
    };
    let root = match tree(named) {
        Ok(root) => root,
        Err(said) => {
            err(&said);
            return NO_CODE;
        }
    };
    let path = dunce::canonicalize(file).unwrap_or_else(|_| PathBuf::from(file));
    let Some(language) = format::language_of(&path) else {
        err(&format!("{file}: no plugin opens it, and so orior knows no way to run it"));
        return NO_CODE;
    };
    let run = match run_file::line_for(&root, &path, &language) {
        Ok(run) => run,
        Err(said) => {
            err(&said);
            return NO_CODE;
        }
    };
    let bash = match runner::bash() {
        Ok(bash) => bash,
        Err(said) => {
            err(&said);
            return NO_CODE;
        }
    };
    err(&format!("$ {}", run.line));
    match std::process::Command::new(bash).arg("-c").arg(&run.line).env("PATH", toolchains::run_path()).status() {
        Ok(status) => status.code().unwrap_or(NO_CODE),
        Err(error) => {
            err(&error.to_string());
            NO_CODE
        }
    }
}

/// `orior file clone [url] [folder]`: clones a repository, orior's where none is named, into a new
/// folder in `folder` or the working one. git's progress is written over one line as git writes it.
fn clone_words(words: &[String]) -> i32 {
    let url = words.first().map_or(git::ORIOR, String::as_str);
    let parent = words.get(1).map_or_else(|| std::env::current_dir().unwrap_or_default(), PathBuf::from);
    let target = match git::clone_folder(url, &parent) {
        Ok(target) => target,
        Err(said) => {
            err(&said);
            return WRONG;
        }
    };
    let mut shown = 0usize;
    let mut wrote = false;
    let cloned = git::clone(url, &target, |line, ended| {
        wrote = true;
        let width = line.chars().count();
        eprint!("\r{line}{}", " ".repeat(shown.saturating_sub(width)));
        shown = if ended { 0 } else { width };
        if ended {
            eprintln!();
        }
    });
    match cloned {
        Ok(dir) => {
            out(&format!("Cloned into {}.", dir.display()));
            out(&format!("orior --root \"{}\" opens it in the window.", dir.display()));
            0
        }
        // Where git wrote why, it is on the screen already.
        Err(said) => {
            if !wrote {
                err(&said);
            }
            NO_CODE
        }
    }
}

/// `orior run validate [--json] <file>...`: validates each file with the tool plugin for its language,
/// as validate.rs does, and says what it found and its verdict, or with --json the whole report.
/// Exits 1 where a file does not hold.
fn validate_files(words: &[String]) -> i32 {
    let json = words.iter().any(|word| word == "--json");
    let files: Vec<&String> = words.iter().filter(|word| *word != "--json").collect();
    if files.is_empty() || files.iter().any(|word| word.starts_with("--")) {
        err("validate takes [--json] and the files to validate");
        return WRONG;
    }
    let mut code = 0;
    for file in files {
        let path = dunce::canonicalize(file).unwrap_or_else(|_| PathBuf::from(file));
        let Some(tool) = format::language_of(&path).and_then(|language| validate::tool_for(&language)) else {
            err(&format!("{file}: no tool plugin validates it"));
            code = NO_CODE;
            continue;
        };
        let report = match validate::validate(&tool, &path) {
            Ok(report) => report,
            Err(said) => {
                err(&format!("{file}: {said}"));
                code = NO_CODE;
                continue;
            }
        };
        if !report.holds {
            code = NO_CODE;
        }
        if json {
            out(&serde_json::to_string_pretty(&report).unwrap_or_default());
            continue;
        }
        for check in &report.checks {
            let with = if check.settings.is_empty() { "as it is".to_string() } else { check.settings.join(" ") };
            let stopped = if check.barriers.is_empty() { String::new() } else { format!(", stopped by {}", check.barriers.join(", ")) };
            let errors = if check.errors == 1 { "error" } else { "errors" };
            err(&format!("  checked {with}: {:.1} s, {} {errors}{stopped}", check.seconds, check.errors));
        }
        for finding in &report.findings {
            let head = format!("{file}:{}:{}: {}", finding.line + 1, finding.col + 1, finding.kind);
            let mut lines = finding.message.lines().filter(|line| !line.trim().is_empty());
            out(&format!("{head}: {}", lines.next().unwrap_or_default()));
            for line in lines {
                out(&format!("    {line}"));
            }
            if let Some(lifted) = &finding.lifted {
                out(&format!("    {lifted}"));
            }
        }
        out(&format!("{file}: {}: {}", if report.holds { "holds" } else { "does not hold" }, report.verdict));
    }
    code
}

/// `orior edit format [--check] <file>...`: formats each file in place with its language's formatter,
/// as format.rs says, and names each it changed. With --check it changes none, names each it would,
/// and exits 1 where there is one.
fn format_files(words: &[String]) -> i32 {
    let check = words.iter().any(|word| word == "--check");
    let files: Vec<&String> = words.iter().filter(|word| *word != "--check").collect();
    if files.is_empty() || files.iter().any(|word| word.starts_with("--")) {
        err("format takes [--check] and the files to format");
        return WRONG;
    }
    let mut code = 0;
    for file in files {
        let path = PathBuf::from(file);
        let full = dunce::canonicalize(&path).unwrap_or_else(|_| path.clone());
        let Some(language) = format::language_of(&path) else {
            err(&format!("{file}: no plugin opens it, and so no formatter knows it"));
            code = NO_CODE;
            continue;
        };
        let text = match std::fs::read_to_string(&path) {
            Ok(text) => text,
            Err(error) => {
                err(&format!("{file}: {error}"));
                code = NO_CODE;
                continue;
            }
        };
        match format::format(&full, &language, &text) {
            Ok(formatted) if formatted == text => {}
            Ok(_) if check => {
                out(&format!("{file} would change"));
                code = NO_CODE;
            }
            Ok(formatted) => match std::fs::write(&path, formatted) {
                Ok(()) => out(&format!("{file} formatted")),
                Err(error) => {
                    err(&format!("{file}: {error}"));
                    code = NO_CODE;
                }
            },
            Err(said) => {
                err(&said);
                code = NO_CODE;
            }
        }
    }
    code
}

/// `orior file user-css`: names the stylesheet the window lays over its own, and makes it, empty but
/// for a note, where it is not there yet.
fn user_css() -> i32 {
    match home::ensure_user_css() {
        Ok(path) => {
            out(&path.display().to_string());
            0
        }
        Err(said) => {
            err(&said);
            NO_CODE
        }
    }
}

/// `orior edit search [--case] [--word] [--regex] <text>`: every line in the tree's files that holds
/// the text, as path:line:column: line. Exits 1 where none does, as grep does.
fn search(named: Option<&str>, words: &[String]) -> i32 {
    let mut how = files::Searching::default();
    let mut text = Vec::new();
    for word in words {
        match word.as_str() {
            "--case" => how.case = true,
            "--word" => how.word = true,
            "--regex" => how.regex = true,
            _ => text.push(word.as_str()),
        }
    }
    let query = text.join(" ");
    if query.is_empty() {
        err("search needs text: orior edit search <text>");
        return WRONG;
    }
    let root = match tree(named) {
        Ok(root) => root,
        Err(said) => {
            err(&said);
            return NO_CODE;
        }
    };
    match files::search(&root, &query, how, &Default::default()) {
        Ok(hits) if hits.is_empty() => 1,
        Ok(hits) => {
            for hit in hits {
                out(&format!("{}:{}:{}: {}", hit.path, hit.line, hit.col, hit.text.trim()));
            }
            0
        }
        Err(said) => {
            err(&said);
            WRONG
        }
    }
}

/// A word that names a file, as path, path:line or path:line:column: the path in the tree and what
/// follows it, where the file is in the tree.
fn file_word(named: Option<&str>, word: &str) -> Option<Result<String, String>> {
    let mut path = word;
    let mut place = String::new();
    for _ in 0..2 {
        if let Some((before, number)) = path.rsplit_once(':') {
            if !number.is_empty() && number.bytes().all(|b| b.is_ascii_digit()) && !before.is_empty() {
                place = format!(":{number}{place}");
                path = before;
            }
        }
    }
    let full = dunce::canonicalize(path).ok().filter(|full| full.is_file())?;
    Some(tree(named).and_then(|root| {
        full.strip_prefix(&root)
            .map(|inner| format!("{}{place}", inner.to_string_lossy().replace('\\', "/")))
            .map_err(|_| format!("{} is not in the tree {}", full.display(), root.display()))
    }))
}

/// The words that can follow `menu`: its commands, and its jobs or their subjects.
fn next_words(named: Option<&str>, menus: &Commands, menu: &str) -> Vec<String> {
    let Some(menu) = menus.menus.iter().find(|one| one.word() == menu) else {
        return Vec::new();
    };
    let mut found: Vec<String> = menu.commands().map(|item| item.command.clone()).collect();
    if let (false, Ok(root)) = (menu.groups.is_empty(), tree(named)) {
        let jobs = catalog::read(&root).into_iter().filter(|job| menu.groups.iter().any(|g| g == job.group));
        if menu.split.is_some() {
            let mut subjects: Vec<String> = jobs.map(|job| subject(&job)).collect();
            subjects.dedup();
            found.extend(subjects);
        } else {
            found.extend(jobs.map(|job| job.id));
        }
    }
    found
}

/// The completion script for `shell`, which asks orior itself for the words after the first.
fn completions(shell: &str, menus: &Commands) -> Result<String, String> {
    let mut first: Vec<String> = menus.menus.iter().map(|menu| menu.word()).collect();
    first.extend(["list", "show", "bridge", "--root", "--help", "--version", "--completions"].map(String::from));
    let first = first.join(" ");
    let bash = format!(
        "_orior() {{\n  local cur=${{COMP_WORDS[COMP_CWORD]}}\n  if [ \"$COMP_CWORD\" -eq 1 ]; then\n    COMPREPLY=($(compgen -W \"{first}\" -- \"$cur\"))\n  else\n    COMPREPLY=($(compgen -W \"$(orior __words \"${{COMP_WORDS[1]}}\" 2>/dev/null)\" -- \"$cur\"))\n  fi\n}}\ncomplete -o default -F _orior orior\n"
    );
    match shell {
        "bash" => Ok(bash),
        "zsh" => Ok(format!("autoload -U +X bashcompinit && bashcompinit\n{bash}")),
        "fish" => Ok(format!(
            "complete -c orior -n __fish_use_subcommand -a \"{first}\"\ncomplete -c orior -n 'not __fish_use_subcommand' -a '(orior __words (commandline -opc)[2])'\n"
        )),
        "powershell" => {
            let quoted = first.split(' ').map(|word| format!("'{word}'")).collect::<Vec<_>>().join(", ");
            Ok(format!(
                "Register-ArgumentCompleter -Native -CommandName orior -ScriptBlock {{\n  param($word, $ast, $at)\n  $given = @($ast.CommandElements | ForEach-Object {{ $_.ToString() }})\n  if ($given.Count -le 1 -or ($given.Count -eq 2 -and $word)) {{ $all = @({quoted}) }} else {{ $all = @(& orior __words $given[1]) }}\n  $all | Where-Object {{ $_ -like \"$word*\" }} | ForEach-Object {{ [System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', $_) }}\n}}\n"
            ))
        }
        _ => Err(format!("orior has no completions for {shell}: bash, zsh, fish or powershell")),
    }
}

/// `orior help auto-report [on|off]`: turns errors filing on their own on or off, or says which.
fn auto_report(word: Option<&str>) -> i32 {
    let on = match word {
        None => {
            out(if report::auto() { "on" } else { "off" });
            return 0;
        }
        Some("on") => true,
        Some("off") => false,
        Some(other) => {
            err(&format!("{other} is neither on nor off"));
            return WRONG;
        }
    };
    match report::set_auto(on) {
        Ok(()) => 0,
        Err(said) => {
            err(&said);
            NO_CODE
        }
    }
}

/// `orior help report [category] [title]`: opens the bug report page with what was given filled in.
fn report_page(named: Option<&str>, words: &[String]) -> i32 {
    let (category, title) = match words.first() {
        Some(first) if report::CATEGORIES.contains(&first.as_str()) => (first.clone(), words[1..].join(" ")),
        _ => ("unknown".to_string(), words.join(" ")),
    };
    let root = tree(named).ok();
    let given = report::Report { category, title, ..report::Report::default() };
    let url = report::page(&given, root.as_deref());
    out(&url);
    match report::open_page(&url) {
        Ok(()) => 0,
        Err(said) => {
            err(&said);
            NO_CODE
        }
    }
}

/// `orior <menu> [subject] <job>`: a job of a menu of jobs, shown as the window shows it on choosing.
fn menu_job(named: Option<&str>, menu: &Menu, words: &[String]) -> i32 {
    let root = match tree(named) {
        Ok(root) => root,
        Err(said) => {
            err(&said);
            return NO_CODE;
        }
    };
    let mut jobs: Vec<Job> = catalog::read(&root).into_iter().filter(|job| menu.groups.iter().any(|g| g == job.group)).collect();
    let mut words = words.iter();
    if menu.split.is_some() {
        let Some(name) = words.next() else { return menu_lines(Some(&root), menu) };
        jobs.retain(|job| subject(job) == *name);
        if jobs.is_empty() {
            err(&format!("{} has no {name}: orior {} names what it has", menu.title, menu.word()));
            return NO_CODE;
        }
        if words.len() == 0 {
            jobs.iter().for_each(|job| out(&job.id));
            return 0;
        }
    }
    let Some(name) = words.next() else { return menu_lines(Some(&root), menu) };
    match job_named(&jobs, name) {
        Ok(job) => {
            show(&root, &job);
            0
        }
        Err(said) => {
            err(&said);
            NO_CODE
        }
    }
}

/// Runs the words given on the command line: a command run here, with the code to exit with, or the
/// window to open for one that acts there.
pub fn run(given: Vec<String>) -> Outcome {
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
                return Outcome::Exit(WRONG);
            };
            named = Some(dir);
        } else if let Some(dir) = word.strip_prefix("--root=") {
            named = Some(dir.to_string());
        } else {
            words.push(word);
        }
    }
    let menus = commands::read();
    let named = named.as_deref();
    let window = |menu: String, command: String, args: Vec<String>| match named.map(|dir| tree(Some(dir))).transpose() {
        Ok(root) => Outcome::Window(Launch { root, menu, command, args }),
        Err(said) => {
            err(&said);
            Outcome::Exit(NO_CODE)
        }
    };
    let Some(first) = words.first().map(String::as_str) else {
        if named.is_some() {
            return window(String::new(), String::new(), Vec::new());
        }
        out(help(&menus).trim_end());
        return Outcome::Exit(0);
    };
    // help alone is the help; help and a word is that command of the Help menu.
    if matches!(first, "-h" | "--help") || (first == "help" && words.len() == 1) {
        out(help(&menus).trim_end());
        return Outcome::Exit(0);
    }
    if first == "--completions" || first.starts_with("--completions=") {
        let shell = first.strip_prefix("--completions=").map(String::from).or_else(|| words.get(1).cloned()).unwrap_or_default();
        return match completions(&shell, &menus) {
            Ok(script) => {
                print!("{script}");
                Outcome::Exit(0)
            }
            Err(said) => {
                err(&said);
                Outcome::Exit(WRONG)
            }
        };
    }
    if first == "__words" {
        let menu = words.get(1).map(String::as_str).unwrap_or_default();
        next_words(named, &menus, menu).iter().for_each(|word| out(word));
        return Outcome::Exit(0);
    }
    if matches!(first, "--version" | "-V") {
        out(&format!("orior {}", env!("CARGO_PKG_VERSION")));
        return Outcome::Exit(0);
    }
    // The first run at a terminal asks whether errors file on their own, where the installer did not,
    // and a run that answers it itself asks nothing.
    if words.get(1).map(String::as_str) != Some("auto-report") {
        report::ask_once();
    }
    // The words before the menus were the command line's, and each still names what it named.
    match first {
        "list" | "show" => return Outcome::Exit(console(first, named, &words[1..], &menus)),
        "bridge" => return Outcome::Exit(console("bridge", named, &words[1..], &menus)),
        _ => {}
    }
    let Some(menu) = menus.menus.iter().find(|menu| menu.word() == first) else {
        // A word that is no menu and names a file of the tree opens it, as Go, Go to File does.
        if let Some(file) = file_word(named, first) {
            return match file {
                Ok(file) => window("go".into(), "file".into(), vec![file]),
                Err(said) => {
                    err(&said);
                    Outcome::Exit(NO_CODE)
                }
            };
        }
        err(&format!("orior takes no {first}: orior help names what it takes"));
        return Outcome::Exit(WRONG);
    };
    let Some(second) = words.get(1) else {
        let root = tree(named).ok();
        return Outcome::Exit(menu_lines(root.as_deref(), menu));
    };
    if let Some(item) = menu.command(second) {
        let rest = words[2..].to_vec();
        if item.console {
            return Outcome::Exit(console(&item.command, named, &rest, &menus));
        }
        return window(menu.word(), item.command.clone(), rest);
    }
    if menu.word() == "run" {
        return Outcome::Exit(console("start", named, &words[1..], &menus));
    }
    if !menu.groups.is_empty() {
        return Outcome::Exit(menu_job(named, menu, &words[1..]));
    }
    err(&format!("{} has no {second}: orior {first} names what it has", menu.title));
    Outcome::Exit(WRONG)
}

/// Lets go of the console where this process is the only one on it: the console Windows opens for a
/// console program started from outside a terminal. Started from a terminal, the process shares the
/// terminal's console and keeps it.
#[cfg(windows)]
pub fn console_let_go() {
    #[link(name = "kernel32")]
    unsafe extern "system" {
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
mod help_text {
    use super::*;

    #[test]
    fn wrapping_keeps_every_word_and_the_width() {
        let text = "orior file new-plugin <name> --ext <ext,...> [--from <plugin>] [--line-comment <text>] [--keywords <a,b>]";
        let lines = wrapped(text, 40, 2);
        assert!(lines.len() > 1);
        assert!(lines.iter().all(|line| line.chars().count() <= 40));
        assert!(lines[1..].iter().all(|line| line.starts_with("  ") && !line.starts_with("   ")));
        assert_eq!(lines.iter().map(|line| line.trim()).collect::<Vec<_>>().join(" "), text);
        assert_eq!(wrapped("short", 40, 2), vec!["short"]);
    }

    #[test]
    fn the_help_keeps_to_its_width_and_names_every_command() {
        let menus = commands::read();
        let text = help(&menus);
        for line in text.lines() {
            assert!(line.chars().count() <= HELP_WIDTH, "too wide: {line}");
        }
        for menu in &menus.menus {
            for item in menu.commands() {
                assert!(text.contains(&format!("orior {} {}", menu.word(), item.command)), "{} is missing", item.command);
            }
        }
    }
}

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

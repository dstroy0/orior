// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The commands of the query, the one list the command line and the window read, and the dispatch:
//! words in, lines out through a sink, the code back. Each loop runs under a `Context`, the tree,
//! the programs, the sink and the leave, which starts its run, builds its tools and runs its
//! programs.

use std::path::{Path, PathBuf};
use std::process::{Command as Process, Output, Stdio};

use crate::leaves::{Asker, Leave, Terminal};
use crate::programs::{Programs, Role};
use crate::run::Run;
use crate::tools;

/// One input a command takes: a choice among `choices`, a word, or free words.
pub struct Param {
    pub key: &'static str,
    pub kind: &'static str,
    pub choices: Vec<String>,
    pub required: bool,
}

/// How a command's words are made from the values given, as the catalog makes a step's arguments.
pub enum Word {
    /// Written as it is.
    Lit(&'static str),
    /// The value of a param, where one is given.
    Value(&'static str),
    /// A flag and then the value of a param, where one is given.
    Flag(&'static str, &'static str),
    /// The flag before each value chosen of a param.
    Each(&'static str, &'static str),
    /// The words of a param, split as a shell splits them.
    Words(&'static str),
    /// The word, where the param's value is the value named.
    When(&'static str, &'static str, &'static str),
}

/// One command of `orior protocol`.
pub struct Command {
    pub id: &'static str,
    pub title: &'static str,
    pub about: &'static str,
    pub params: Vec<Param>,
    pub words: Vec<Word>,
}

/// The folder the bridge, the rulesets and the part's files stand in.
pub const TABLE: &str = "src/cu/transpiler/lstar/protocol/table";

fn choice(key: &'static str, choices: &[&str], required: bool) -> Param {
    Param { key, kind: "choice", choices: choices.iter().map(|c| c.to_string()).collect(), required }
}

fn choice_of(key: &'static str, choices: Vec<String>, required: bool) -> Param {
    Param { key, kind: "choice", choices, required }
}

fn word(key: &'static str) -> Param {
    Param { key, kind: "word", choices: Vec::new(), required: false }
}

fn words(key: &'static str) -> Param {
    Param { key, kind: "words", choices: Vec::new(), required: false }
}

fn leave_param() -> Param {
    choice("leave", &["all", "none"], false)
}

/// Every name the table holds a file of each of `kinds` of, sorted.
pub fn whole(root: &Path, kinds: &[&str]) -> Vec<String> {
    let table = root.join(TABLE);
    let mut names: Vec<String> = std::fs::read_dir(&table)
        .into_iter()
        .flatten()
        .flatten()
        .filter_map(|entry| entry.file_name().to_str().and_then(|name| name.strip_suffix(&format!(".{}", kinds[0]))).map(str::to_string))
        .filter(|name| kinds.iter().all(|kind| table.join(format!("{name}.{kind}")).is_file()))
        .collect();
    names.sort();
    names
}

/// The commands, each with its params and the words that run it.
pub fn list(root: &Path) -> Vec<Command> {
    let languages = whole(root, &["klm", "krs"]);
    let parts = whole(root, &["khw", "kqr", "ksc"]);
    vec![
        Command {
            id: "query",
            title: "query",
            about: "the bridge written over the two languages' rulesets, then its pairs put to the part where one is chosen",
            params: vec![choice_of("from", languages.clone(), true), choice_of("to", languages.clone(), true), choice_of("part", parts.clone(), false), choice("dry", &["dry"], false), leave_param()],
            words: vec![Word::Lit("query"), Word::Flag("--from", "from"), Word::Flag("--to", "to"), Word::Flag("--part", "part"), Word::When("dry", "dry", "--dry"), Word::Flag("--leave", "leave")],
        },
        Command {
            id: "learn",
            title: "learn",
            about: "a part pass: discover, seed, turns or registers through the airlock, or rounds, dry or enumerate",
            params: vec![choice("step", &["discover", "seed", "turns", "registers", "rounds", "dry", "enumerate"], true), word("word"), word("base"), leave_param()],
            words: vec![Word::Lit("learn"), Word::Value("step"), Word::Value("word"), Word::Flag("--base", "base"), Word::Flag("--leave", "leave")],
        },
        Command {
            id: "cross",
            title: "cross",
            about: "the cross alone over the latest listed questions, reaching no part",
            params: vec![word("base"), leave_param()],
            words: vec![Word::Lit("cross"), Word::Flag("--base", "base"), Word::Flag("--leave", "leave")],
        },
        Command {
            id: "write",
            title: "write",
            about: "the writers: the bridge over rulesets, the part's krs, or its gsm",
            params: vec![choice("writer", &["bridge", "krs", "gsm"], true), words("rulesets"), leave_param()],
            words: vec![Word::Lit("write"), Word::Value("writer"), Word::Words("rulesets"), Word::Flag("--leave", "leave")],
        },
        Command {
            id: "pairs",
            title: "pairs",
            about: "the bridge's pairs put to the part, and the other modes of klq_identity",
            params: vec![
                choice("mode", &["frame", "pair", "cost", "stall", "register", "past", "queue", "curve", "text_identity", "stick"], true),
                words("words"),
                choice_of("part", parts, false),
                choice_of("to", languages, false),
                choice("dry", &["dry"], false),
                word("seed"),
                word("trace"),
                leave_param(),
            ],
            words: vec![
                Word::Lit("pairs"),
                Word::Value("mode"),
                Word::Words("words"),
                Word::Flag("--part", "part"),
                Word::Flag("--to", "to"),
                Word::When("dry", "dry", "--dry"),
                Word::Flag("--seed", "seed"),
                Word::Flag("--trace", "trace"),
                Word::Flag("--leave", "leave"),
            ],
        },
        Command {
            id: "stick",
            title: "stick",
            about: "the vendor's reading of the stick, the engine's stick held against it, or the read with its words",
            params: vec![choice("what", &["vendor", "engine", "read"], false), words("words"), leave_param()],
            words: vec![Word::Lit("stick"), Word::Value("what"), Word::Words("words"), Word::Flag("--leave", "leave")],
        },
        Command { id: "kept", title: "kept", about: "the runs of the tree kept while the host is idle", params: vec![], words: vec![Word::Lit("kept")] },
        Command { id: "sweep", title: "sweep", about: "the host swept for the run's buffer, qry_host.h written from the sweeps' median", params: vec![], words: vec![Word::Lit("sweep")] },
    ]
}

/// `--leave <word>` taken off `words`, where it is there.
pub fn leave_taken(words: &mut Vec<String>) -> Result<Option<Leave>, String> {
    let Some(at) = words.iter().position(|w| w == "--leave") else { return Ok(None) };
    let word = words.get(at + 1).cloned().ok_or("--leave takes all, none or ask")?;
    let leave = Leave::from_word(&word).ok_or_else(|| format!("--leave takes all, none or ask, not {word}"))?;
    words.drain(at..at + 2);
    Ok(Some(leave))
}

/// What every loop runs under: the tree, where its programs are found, where its lines go, how
/// the run's leaves are decided, and the environment its programs are given beyond the run's.
pub struct Context<'a> {
    pub root: &'a Path,
    pub programs: &'a dyn Programs,
    pub sink: &'a mut dyn FnMut(&str),
    pub leave: Leave,
    pub asker: &'a mut dyn Asker,
    pub env: Vec<(String, String)>,
}

impl Context<'_> {
    pub fn say(&mut self, text: &str) {
        (self.sink)(text)
    }

    pub fn program(&self, role: Role) -> Result<PathBuf, String> {
        self.programs.program(role)
    }

    /// The tool `name` built where stale, its path.
    pub fn tool(&self, name: &str) -> Result<PathBuf, String> {
        tools::built(self.root, self.programs, name)
    }

    /// A run started in `folder`, a path from the top of the tree, or joined.
    pub fn started(&self, folder: &str) -> Result<Run, String> {
        Run::started(self.root, &self.root.join(folder), self.programs)
    }

    /// The file `under` the top of the tree.
    pub fn at(&self, under: &str) -> PathBuf {
        self.root.join(under)
    }

    /// A command of `program` in the run's environment and this context's, at the top of the tree.
    pub fn command(&self, run: &Run, program: &Path) -> Process {
        let mut command = run.writer().command_of(program);
        command.envs(self.env.iter().map(|(k, v)| (k.as_str(), v.as_str()))).current_dir(self.root).stdin(Stdio::null());
        command
    }

    /// `program` run with `words`, waited for, its output whole.
    pub fn ran(&self, run: &Run, program: &Path, words: &[&str]) -> Result<Output, String> {
        self.command(run, program).args(words).output().map_err(|e| format!("{}: {e}", program.display()))
    }

    /// `program` run with `words`, its output said through the sink line by line and handed as
    /// `<name>.console`. Its exit code.
    pub fn teed(&mut self, run: &Run, name: &str, program: &Path, words: &[&str]) -> Result<i32, String> {
        let child = self.command(run, program).args(words).stdout(Stdio::piped()).stderr(Stdio::piped()).spawn().map_err(|e| format!("{}: {e}", program.display()))?;
        run.writer().tee(name, child, self.sink)
    }

    /// `program` run with `words`, its output said through the sink line by line and not handed. Its exit code.
    pub fn said(&mut self, run: &Run, program: &Path, words: &[&str]) -> Result<i32, String> {
        let output = self.ran(run, program, words)?;
        for line in String::from_utf8_lossy(&output.stdout).lines().chain(String::from_utf8_lossy(&output.stderr).lines()) {
            (self.sink)(line);
        }
        Ok(output.status.code().unwrap_or(-1))
    }

    /// `program` run with `words`, its output, the standard error after the standard output, handed
    /// as the blob `name` and nothing said. Its exit code.
    pub fn handed(&self, run: &Run, name: &str, program: &Path, words: &[&str]) -> Result<i32, String> {
        let output = self.ran(run, program, words)?;
        let mut bytes = output.stdout;
        bytes.extend(&output.stderr);
        run.writer().hand(name, &bytes, crate::writer::Priority::Ordinary)?;
        Ok(output.status.code().unwrap_or(-1))
    }

    /// The run ended as the leave says.
    pub fn ended(&mut self, run: Run) -> Result<(), String> {
        run.ended(self.leave, self.asker, self.sink)
    }
}

fn usage() -> String {
    "orior protocol takes one of query, learn, cross, write, pairs, stick, kept, sweep".to_string()
}

/// The words run: a code to exit with, or why they could not be.
pub fn run(root: &Path, programs: &dyn Programs, words: &[String], sink: &mut dyn FnMut(&str)) -> Result<i32, String> {
    let mut words: Vec<String> = words.to_vec();
    let leave = Leave::chosen(leave_taken(&mut words)?);
    let mut terminal = Terminal;
    let mut context = Context { root, programs, sink, leave, asker: &mut terminal, env: Vec::new() };
    let (first, rest) = words.split_first().ok_or_else(usage)?;
    match first.as_str() {
        "learn" => crate::learn::run(&mut context, rest),
        "cross" => crate::cross::run(&mut context, rest),
        "write" => crate::write::run(&mut context, rest),
        "query" => crate::write::query(&mut context, rest),
        "pairs" => crate::pairs::run(&mut context, rest),
        "stick" => crate::stick::run(&mut context, rest),
        "kept" => crate::kept::run(&mut context, rest),
        "sweep" => crate::kept::sweep(&mut context, rest),
        _ => Err(usage()),
    }
}

#[cfg(test)]
mod dispatch {
    use super::*;

    #[test]
    fn the_list_names_every_loop_once() {
        let ids: Vec<&str> = list(Path::new(".")).iter().map(|c| c.id).collect();
        assert_eq!(ids, ["query", "learn", "cross", "write", "pairs", "stick", "kept", "sweep"]);
    }

    #[test]
    fn leave_is_taken_off_the_words() {
        let mut words: Vec<String> = ["turns", "--leave", "none", "--base", "a.cu"].iter().map(|w| w.to_string()).collect();
        assert_eq!(leave_taken(&mut words).unwrap(), Some(Leave::None));
        assert_eq!(words, ["turns", "--base", "a.cu"]);
        let mut bad: Vec<String> = vec!["--leave".into(), "maybe".into()];
        assert!(leave_taken(&mut bad).is_err());
        let mut none: Vec<String> = vec!["turns".into()];
        assert_eq!(leave_taken(&mut none).unwrap(), None);
    }

    #[test]
    fn a_loop_missing_a_program_says_so_and_starts_no_run() {
        struct Nothing;
        impl Programs for Nothing {
            fn program(&self, role: Role) -> Result<PathBuf, String> {
                Err(format!("{} is not found", role.name()))
            }
            fn run_path(&self) -> std::ffi::OsString {
                std::ffi::OsString::new()
            }
        }
        let root = crate::tree_root();
        let folder = root.join(crate::learn::FOLDER);
        let before = crate::run::each(&folder).len();
        // a learn needs the C compiler where a tool is stale, and nvdisasm either way: with nothing found it says
        // the first it misses and starts no run
        let said = run(&root, &Nothing, &["learn".into(), "turns".into()], &mut |_| {}).unwrap_err();
        assert!(said.contains("is not found") || said.contains("no nvdisasm"), "{said}");
        assert_eq!(crate::run::each(&folder).len(), before, "no run was started");
    }

    #[test]
    fn an_unknown_command_is_refused_with_the_list() {
        let said = run(Path::new("."), &crate::programs::OnPath, &["dance".into()], &mut |_| {}).unwrap_err();
        assert!(said.contains("learn") && said.contains("sweep"), "{said}");
        let none = run(Path::new("."), &crate::programs::OnPath, &[], &mut |_| {}).unwrap_err();
        assert!(none.contains("learn"));
    }

    #[test]
    fn the_table_s_whole_names_are_those_with_every_kind() {
        let root = crate::tree_root().join("build/query_test/whole");
        let _ = std::fs::remove_dir_all(&root);
        let table = root.join(TABLE);
        std::fs::create_dir_all(&table).unwrap();
        for file in ["cu.klm", "cu.krs", "sass.klm", "sass.krs", "c.krs", "sm_86.khw", "sm_86.kqr", "sm_86.ksc", "sm_89.khw"] {
            std::fs::write(table.join(file), "").unwrap();
        }
        assert_eq!(whole(&root, &["klm", "krs"]), ["cu", "sass"]);
        assert_eq!(whole(&root, &["khw", "kqr", "ksc"]), ["sm_86"]);
        let _ = std::fs::remove_dir_all(&root);
    }
}

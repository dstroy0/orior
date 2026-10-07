// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Every job the app can start, read from the tree each time it is asked for.
//!
//! Nothing here holds a list of sims, parts, programs, viewers or stages. Each is read from the file
//! that defines it: the sims from the case in src/sims/run.sh, the driver's parts from its own usage
//! text, the viewers and the stage scripts from where they sit. A sim added to run.sh is a job the
//! next time the catalog is read, and a job whose file is gone is gone with it.

use std::fs;
use std::path::{Path, PathBuf};

use serde::Serialize;

use crate::root::relative;

/// One input a job takes. `kind` is how the app asks for it: one of `choices`, several of them, a
/// file, a folder, a path to write, a word, or free words added to the end.
#[derive(Clone, Serialize)]
pub struct Param {
    pub key: String,
    pub kind: &'static str,
    pub choices: Vec<String>,
    pub default: String,
    pub required: bool,
}

/// How a step's argument list is made from the values given.
#[derive(Clone)]
pub enum Arg {
    /// Written as it is.
    Lit(String),
    /// The value of a param, where one is given.
    Value(String),
    /// A flag and then the value of a param, where one is given.
    Flag(&'static str, String),
    /// The flag before each value chosen of a param.
    Each(&'static str, String),
    /// The words of a param, split as a shell splits them, after `--` where the given text is not
    /// empty and the step asks for the separator.
    Words(String, bool),
}

#[derive(Clone)]
pub enum Program {
    /// A bash script, by its path in the tree.
    Bash(String),
    /// A Python script, by its path in the tree.
    Python(String),
    /// A program a build job writes under one of the build folders, by its name.
    Built(&'static str),
}

#[derive(Clone)]
pub struct Step {
    pub program: Program,
    pub args: Vec<Arg>,
}

#[derive(Clone, Serialize)]
pub struct Job {
    pub id: String,
    pub group: &'static str,
    pub title: String,
    pub file: String,
    pub about: String,
    pub params: Vec<Param>,
    /// What the job writes for a reader to open: "views" where it writes pages, else empty.
    pub opens: &'static str,
    #[serde(skip)]
    pub steps: Vec<Step>,
}

fn param(key: &str, kind: &'static str) -> Param {
    Param { key: key.to_string(), kind, choices: Vec::new(), default: String::new(), required: false }
}

fn required(key: &str, kind: &'static str) -> Param {
    Param { required: true, ..param(key, kind) }
}

fn choice(key: &str, choices: Vec<String>, required_too: bool) -> Param {
    let default = choices.first().cloned().unwrap_or_default();
    Param { key: key.to_string(), kind: "choice", choices, default, required: required_too }
}

fn many(key: &str, choices: Vec<String>) -> Param {
    Param { key: key.to_string(), kind: "many", choices, default: String::new(), required: false }
}

fn words() -> Param {
    param("arguments", "words")
}

/// The files in `dir` whose names pass `keep`, sorted, as paths.
fn files_in(dir: &Path, keep: impl Fn(&str) -> bool) -> Vec<PathBuf> {
    let mut found: Vec<PathBuf> = fs::read_dir(dir)
        .into_iter()
        .flatten()
        .flatten()
        .map(|entry| entry.path())
        .filter(|path| path.is_file() && path.file_name().and_then(|n| n.to_str()).is_some_and(&keep))
        .collect();
    found.sort();
    found
}

fn dirs_in(dir: &Path) -> Vec<PathBuf> {
    let mut found: Vec<PathBuf> =
        fs::read_dir(dir).into_iter().flatten().flatten().map(|e| e.path()).filter(|p| p.is_dir()).collect();
    found.sort();
    found
}

fn stem(path: &Path) -> String {
    path.file_stem().map(|s| s.to_string_lossy().into_owned()).unwrap_or_default()
}

/// The opening paragraph a file gives of itself: a Python module's docstring, or the comment lines a
/// script or a source file opens with, past its license lines.
pub fn about(path: &Path) -> String {
    let Ok(text) = fs::read_to_string(path) else { return String::new() };
    let lines: Vec<&str> = text.lines().take(120).collect();
    let license = |line: &str| line.contains("SPDX") || line.contains("Copyright") || line.starts_with("#!");
    let mut kept: Vec<String> = Vec::new();
    if path.extension().is_some_and(|e| e == "py") {
        if let Some(open) = lines.iter().position(|l| l.trim_start().starts_with("\"\"\"")) {
            for (at, line) in lines[open..].iter().enumerate() {
                let line = line.trim();
                let line = if at == 0 { line.trim_start_matches("\"\"\"") } else { line };
                let ended = line.contains("\"\"\"");
                let line = line.trim_end_matches("\"\"\"").trim();
                if line.is_empty() && !kept.is_empty() {
                    break;
                }
                if !line.is_empty() {
                    kept.push(line.to_string());
                }
                if ended {
                    break;
                }
            }
            return kept.join(" ");
        }
    }
    let mark = if path.extension().is_some_and(|e| e == "sh" || e == "py" || e == "ps1") { "#" } else { "//" };
    for line in lines.iter().map(|l| l.trim()) {
        if line.is_empty() && kept.is_empty() {
            continue;
        }
        if !line.starts_with(mark) {
            if kept.is_empty() && license(line) {
                continue;
            }
            break;
        }
        if license(line) {
            continue;
        }
        let said = line.trim_start_matches(mark).trim_start_matches('!').trim();
        if said.is_empty() {
            if kept.is_empty() {
                continue;
            }
            break;
        }
        kept.push(said.to_string());
    }
    kept.join(" ")
}

fn script_job(root: &Path, group: &'static str, path: &Path, params: Vec<Param>, steps: Vec<Step>) -> Job {
    let file = relative(root, path);
    Job {
        id: format!("{group}/{file}"),
        group,
        title: file.clone(),
        about: about(path),
        file,
        params,
        opens: "",
        steps,
    }
}

/// A path's folder and name. Two scripts of one name differ in the job list by their folders.
fn short(file: &str) -> String {
    let parts: Vec<&str> = file.rsplitn(3, '/').collect();
    match parts.as_slice() {
        [name, folder, ..] => format!("{folder}/{name}"),
        _ => file.to_string(),
    }
}

fn bash_alone(root: &Path, group: &'static str, path: &Path) -> Job {
    let file = relative(root, path);
    let steps = vec![Step { program: Program::Bash(file.clone()), args: vec![Arg::Words("arguments".into(), false)] }];
    let mut job = script_job(root, group, path, vec![words()], steps);
    job.title = short(&file);
    job
}

/// The build scripts: the engine's, each example's, and the maintenance builds. build_stamp.sh is a
/// library the others source and never a build of its own.
fn builds(root: &Path, jobs: &mut Vec<Job>) {
    let is_build = |name: &str| name.starts_with("build") && name.ends_with(".sh") && name != "build_stamp.sh";
    let mut places = vec![root.join("src"), root.join("utils/maint/engine"), root.join("utils/maint/texbuild")];
    places.extend(dirs_in(&root.join("examples")));
    for place in places {
        for path in files_in(&place, is_build) {
            jobs.push(bash_alone(root, "build", &path));
        }
    }
}

/// The names a `case` in a script accepts on the line after `case "$<variable>" in`.
fn case_names(text: &str, variable: &str) -> Vec<String> {
    let opener = format!("case \"${variable}\" in");
    let Some(at) = text.find(&opener) else { return Vec::new() };
    let rest = &text[at + opener.len()..];
    let line = rest.lines().map(str::trim).find(|line| !line.is_empty()).unwrap_or("");
    let Some(names) = line.split(')').next() else { return Vec::new() };
    names.split('|').map(|name| name.trim().to_string()).filter(|name| !name.is_empty() && name != "*").collect()
}

/// One job per sim in the case src/sims/run.sh accepts, each with the first comment its source opens
/// with.
fn sims(root: &Path, jobs: &mut Vec<Job>) {
    let script = root.join("src/sims/run.sh");
    let Ok(text) = fs::read_to_string(&script) else { return };
    let sims_cu = root.join("src/sims/cu");
    for sim in case_names(&text, "SIM") {
        let source = find_dir(&sims_cu, &sim)
            .and_then(|dir| files_in(&dir, |n| n.ends_with(".cu")).into_iter().next())
            .or_else(|| find_file(&sims_cu, &format!("{sim}.cu")));
        let steps = vec![Step {
            program: Program::Bash("src/sims/run.sh".into()),
            args: vec![Arg::Lit(sim.clone()), Arg::Words("arguments".into(), true)],
        }];
        jobs.push(Job {
            id: format!("sim/{sim}"),
            group: "sim",
            title: sim.clone(),
            file: source.as_deref().map(|p| relative(root, p)).unwrap_or_else(|| "src/sims/run.sh".into()),
            about: source.as_deref().map(about).unwrap_or_default(),
            params: vec![words()],
            opens: "",
            steps,
        });
    }
}

fn find_dir(under: &Path, name: &str) -> Option<PathBuf> {
    for dir in dirs_in(under) {
        if dir.file_name().is_some_and(|n| n == name) {
            return Some(dir);
        }
        if let Some(found) = find_dir(&dir, name) {
            return Some(found);
        }
    }
    None
}

fn find_file(under: &Path, name: &str) -> Option<PathBuf> {
    let here = under.join(name);
    if here.is_file() {
        return Some(here);
    }
    dirs_in(under).into_iter().find_map(|dir| find_file(&dir, name))
}

/// The parts the track driver runs, read from its usage text: the list after "in the order given:"
/// across however many string literals the source splits it into.
fn driver_parts(source: &str) -> Vec<String> {
    let Some(at) = source.find("in the order given:") else { return Vec::new() };
    let mut text = String::new();
    let mut inside = true;
    for c in source[at + "in the order given:".len()..].chars() {
        if c == '"' {
            inside = !inside;
            continue;
        }
        if inside {
            text.push(c);
            if text.ends_with("\\n") {
                text.truncate(text.len() - 2);
                break;
            }
        }
    }
    text.split(',').map(|part| part.trim().to_string()).filter(|part| !part.is_empty()).collect()
}

fn cfgs(dir: &Path, root: &Path) -> Vec<String> {
    files_in(dir, |n| n.ends_with(".cfg")).iter().map(|p| relative(root, p)).collect()
}

/// The cell tracker's driver as ingest, as a run of its parts, and as a render of what it found.
fn driver(root: &Path, jobs: &mut Vec<Job>) {
    let source = root.join("examples/cell_tracking/src/track_driver/track_driver.cu");
    let Ok(text) = fs::read_to_string(&source) else { return };
    let file = relative(root, &source);
    let tracker = root.join("examples/cell_tracking");
    let mut runs = vec![relative(root, &tracker.join("base.cfg"))];
    runs.extend(cfgs(&tracker.join("cfg"), root));
    let driver = |args: Vec<Arg>| vec![Step { program: Program::Built("track_driver"), args }];
    let set = || vec![Arg::Value("set".into()), Arg::Words("samples".into(), false)];

    let mut args = vec![Arg::Lit("--ingest".into()), Arg::Flag("--source", "source".into())];
    args.push(Arg::Flag("--axes", "axes".into()));
    args.push(Arg::Flag("--channel", "channel".into()));
    args.extend(set());
    jobs.push(Job {
        id: "ingest/track_driver".into(),
        group: "ingest",
        title: "track_driver --ingest".into(),
        file: file.clone(),
        about: about(&source),
        params: vec![
            required("source", "dir"),
            param("axes", "word"),
            param("channel", "word"),
            required("set", "save-dir"),
            param("samples", "words"),
        ],
        opens: "",
        steps: driver(args),
    });

    let mut args = vec![Arg::Flag("--cfg", "cfg".into()), Arg::Flag("--cfg-out", "cfg-out".into())];
    args.push(Arg::Each("--run", "run".into()));
    args.push(Arg::Words("arguments".into(), false));
    args.extend(set());
    jobs.push(Job {
        id: "run/track_driver".into(),
        group: "run",
        title: "track_driver --run".into(),
        file: file.clone(),
        about: about(&source),
        params: vec![
            choice("cfg", runs.clone(), false),
            param("cfg-out", "save"),
            many("run", driver_parts(&text)),
            words(),
            param("set", "dir"),
            param("samples", "words"),
        ],
        opens: "",
        steps: driver(args),
    });

    let mut args = vec![Arg::Flag("--cfg", "cfg".into()), Arg::Flag("--vis", "vis".into())];
    args.push(Arg::Flag("--export", "export".into()));
    args.push(Arg::Words("arguments".into(), false));
    args.extend(set());
    jobs.push(Job {
        id: "render/track_driver".into(),
        group: "render",
        title: "track_driver --vis".into(),
        file,
        about: about(&source),
        params: vec![
            choice("cfg", runs, false),
            param("vis", "save"),
            param("export", "save"),
            words(),
            param("set", "dir"),
            param("samples", "words"),
        ],
        opens: "",
        steps: driver(args),
    });
}

/// The other runners: the Navier-Stokes programs on their cfgs and the qasm circuits.
fn runners(root: &Path, jobs: &mut Vec<Job>) {
    let navier = root.join("examples/navier_stokes/run.sh");
    if let Ok(text) = fs::read_to_string(&navier) {
        let programs = case_names(&text, "PROGRAM");
        let configs = cfgs(&root.join("examples/navier_stokes/cfg"), root);
        let steps = vec![Step {
            program: Program::Bash(relative(root, &navier)),
            args: vec![Arg::Value("program".into()), Arg::Value("cfg".into())],
        }];
        let params = vec![choice("program", programs, true), choice("cfg", configs, true)];
        jobs.push(script_job(root, "run", &navier, params, steps));
    }
    let qasm = root.join("examples/qasm/run.sh");
    if qasm.is_file() {
        let circuits = files_in(&root.join("examples/qasm/test"), |n| n.ends_with(".qasm"));
        let circuits = circuits.iter().map(|p| relative(root, p)).collect();
        let steps = vec![Step {
            program: Program::Bash(relative(root, &qasm)),
            args: vec![Arg::Words("arguments".into(), false), Arg::Value("circuit".into())],
        }];
        let mut circuit = choice("circuit", circuits, true);
        circuit.kind = "choice-or-file";
        jobs.push(script_job(root, "run", &qasm, vec![circuit, words()], steps));
    }
}

/// Every page builder: the viewers, and the builders beside the examples that write a page. A
/// builder that takes a file as its first word gets one, and every one takes free words after it.
fn views(root: &Path, jobs: &mut Vec<Job>) {
    let mut places = vec![root.join("examples/00_blob_viz_tools")];
    places.extend(dirs_in(&root.join("examples")).into_iter().flat_map(|d| [d.join("maint"), d]));
    let mut seen = std::collections::BTreeSet::new();
    for place in places {
        let builders = files_in(&place, |n| n.starts_with("build_") && n.ends_with("_view.py"));
        let figures = files_in(&place, |n| n == "build_infographic.py" || n == "make_shadow_figure.py");
        for path in builders.into_iter().chain(figures) {
            if !seen.insert(path.clone()) {
                continue;
            }
            let file = relative(root, &path);
            let steps = vec![Step {
                program: Program::Python(file.clone()),
                args: vec![Arg::Value("input".into()), Arg::Words("arguments".into(), false)],
            }];
            let mut job = script_job(root, "view", &path, vec![param("input", "file"), words()], steps);
            job.title = stem(&path).trim_start_matches("build_").to_string();
            job.opens = "views";
            jobs.push(job);
        }
    }
}

/// Each subject's stage scripts, one job a script, and one pipeline a subject that runs its stages in
/// order and stops at the first that fails.
fn stages(root: &Path, jobs: &mut Vec<Job>) {
    for subject in dirs_in(&root.join("examples")) {
        let stage_dirs: Vec<PathBuf> = dirs_in(&subject)
            .into_iter()
            .filter(|d| {
                let name = d.file_name().map(|n| n.to_string_lossy().into_owned()).unwrap_or_default();
                name.len() > 2 && name.as_bytes()[0].is_ascii_digit() && name.as_bytes()[1] == b'_'
            })
            .collect();
        if stage_dirs.is_empty() {
            continue;
        }
        let name = stem(&subject);
        let mut every = Vec::new();
        for stage in &stage_dirs {
            for script in files_in(stage, |n| n.ends_with(".py")) {
                let file = relative(root, &script);
                let step = Step { program: Program::Python(file), args: vec![Arg::Words("arguments".into(), false)] };
                every.push(step.clone());
                let mut job = script_job(root, "stage", &script, vec![words()], vec![step]);
                job.title = relative(&subject, &script);
                jobs.push(job);
            }
        }
        if every.is_empty() {
            continue;
        }
        let readme = subject.join("README.md");
        jobs.push(Job {
            id: format!("pipeline/{name}"),
            group: "pipeline",
            title: name.clone(),
            file: relative(root, &subject),
            about: if readme.is_file() { markdown_opening(&readme) } else { String::new() },
            params: Vec::new(),
            opens: "",
            steps: every,
        });
    }
}

/// The first paragraph of a README that is prose and not a heading.
fn markdown_opening(path: &Path) -> String {
    let Ok(text) = fs::read_to_string(path) else { return String::new() };
    let mut kept = Vec::new();
    for line in text.lines().map(str::trim) {
        if line.is_empty() || line.starts_with('#') || line.starts_with("**") {
            if !kept.is_empty() {
                break;
            }
            continue;
        }
        kept.push(line);
    }
    kept.join(" ")
}

/// The test suites: every run.sh under utils/test and under an example's test folder.
fn tests(root: &Path, jobs: &mut Vec<Job>) {
    let mut found = Vec::new();
    collect_named(&root.join("utils/test"), "run.sh", &mut found);
    for subject in dirs_in(&root.join("examples")) {
        collect_named(&subject.join("test"), "run.sh", &mut found);
    }
    found.sort();
    for path in found {
        jobs.push(bash_alone(root, "test", &path));
    }
}

fn collect_named(under: &Path, name: &str, found: &mut Vec<PathBuf>) {
    let here = under.join(name);
    if here.is_file() {
        found.push(here);
    }
    for dir in dirs_in(under) {
        collect_named(&dir, name, found);
    }
}

/// The whole catalog, in the order the engine's own steps run.
/// The scripts that write the bridge between languages and judge its pairs: every `klq_*.sh` in
/// utils/maint/engine, each taking the words its usage names.
fn protocol(root: &Path, jobs: &mut Vec<Job>) {
    let is_protocol = |name: &str| name.starts_with("klq_") && name.ends_with(".sh");
    for path in files_in(&root.join("utils/maint/engine"), is_protocol) {
        jobs.push(bash_alone(root, "protocol", &path));
    }
}

pub fn read(root: &Path) -> Vec<Job> {
    let mut jobs = Vec::new();
    builds(root, &mut jobs);
    protocol(root, &mut jobs);
    driver(root, &mut jobs);
    runners(root, &mut jobs);
    sims(root, &mut jobs);
    views(root, &mut jobs);
    stages(root, &mut jobs);
    tests(root, &mut jobs);
    jobs
}

#[cfg(test)]
mod reading {
    use super::*;

    #[test]
    fn case_names_reads_the_line_after_the_case() {
        let script = "case \"$SIM\" in\n    a|b_c|d) ;;\n    *) exit 2 ;;\nesac\n";
        assert_eq!(case_names(script, "SIM"), vec!["a", "b_c", "d"]);
    }

    #[test]
    fn short_keeps_the_folder_and_the_name() {
        assert_eq!(short("examples/cell_tracking/build_driver.sh"), "cell_tracking/build_driver.sh");
        assert_eq!(short("src/build_engine.sh"), "src/build_engine.sh");
        assert_eq!(short("run.sh"), "run.sh");
    }

    #[test]
    fn driver_parts_joins_the_literals() {
        let source = "\"  --run names one part; the parts running in the order given: schedule, iapx-prove,\"\n \
                      \" entropy, floor\\n\"";
        assert_eq!(driver_parts(source), vec!["schedule", "iapx-prove", "entropy", "floor"]);
    }

    #[test]
    fn the_tree_gives_every_group() {
        let Some(root) = crate::root::find() else { return };
        let jobs = read(&root);
        for group in ["build", "protocol", "ingest", "run", "render", "sim", "view", "stage", "pipeline", "test"] {
            assert!(jobs.iter().any(|job| job.group == group), "no {group} job read from the tree");
        }
        let run = jobs.iter().find(|job| job.id == "run/track_driver").expect("the driver's run job");
        let parts = &run.params.iter().find(|p| p.key == "run").expect("its run param").choices;
        assert!(parts.iter().any(|p| p == "track"), "the driver's parts read from its usage: {parts:?}");
    }
}

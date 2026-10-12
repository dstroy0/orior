// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! A query: the language a program is written in, the language it is to be written in, and the part
//! it is to run on. The command line names them as `orior protocol query --<from> --<to> [--<part>]
//! [--dry]`, and the window chooses them in the job's form; both run the one job of the catalog.
//! Dry, the pairs go through the airlock as a live run's do, the vendor's disassembler reading every
//! round, and the carrier stops before the driver: nothing is launched (klq_identity.sh, KLQ_DRY).
//!
//! Nothing here holds a list of languages or parts. Each is the files of the bridge's folder that name
//! it: a language its map to the bridge, .klm, and its ruleset, .krs; a part its machine file, .khw,
//! its record, .kqr, and its costs, .ksc. The job is small programs each doing one step: the bridge
//! written over the two rulesets (klq_write.sh), then, where a part is chosen, the bridge's pairs put
//! to the part in the language chosen to write in (klq_identity.sh pair), every question through the
//! gate and the airlock before the driver sees it. Words that name a language or a part the tree does
//! not hold whole are answered with each file missing, and nothing runs.

use std::path::Path;

use crate::bridge::DIR;
use crate::catalog::{Arg, Job, Param, Program, Step};

/// The job's id in the catalog.
pub const ID: &str = "protocol/query";

/// The step that writes the bridge, and the step that puts its pairs to a part.
const BRIDGE_STEP: &str = "utils/maint/engine/klq_write.sh";
const PAIRS_STEP: &str = "utils/maint/engine/klq_identity.sh";

/// The files a language is held whole by, and those a part is.
const LANGUAGE_FILES: [&str; 2] = ["klm", "krs"];
const PART_FILES: [&str; 3] = ["khw", "kqr", "ksc"];

/// Every name the bridge's folder holds a file of each kind of, in the order of the names.
fn whole(root: &Path, kinds: &[&str]) -> Vec<String> {
    let mut names: Vec<String> = std::fs::read_dir(root.join(DIR))
        .map(|entries| {
            entries
                .filter_map(|entry| entry.ok())
                .filter_map(|entry| entry.file_name().to_str().and_then(|name| name.strip_suffix(&format!(".{}", kinds[0])).map(str::to_string)))
                .filter(|name| kinds.iter().all(|kind| root.join(DIR).join(format!("{name}.{kind}")).is_file()))
                .collect()
        })
        .unwrap_or_default();
    names.sort();
    names
}

/// The query as a job of the catalog: the language from and the language to, each one the tree holds
/// whole, and the part, where one is chosen.
pub fn jobs(root: &Path, jobs: &mut Vec<Job>) {
    let languages = whole(root, &LANGUAGE_FILES);
    let parts = whole(root, &PART_FILES);
    let choice = |key: &str, choices: &[String], required: bool| Param { key: key.into(), kind: "choice", choices: choices.to_vec(), default: String::new(), required };
    let bridge = Step { program: Program::Bash(BRIDGE_STEP.into()), args: vec![Arg::Format("{from}.krs".into()), Arg::Format("{to}.krs".into())] };
    let pairs = Step {
        program: Program::Bash(PAIRS_STEP.into()),
        args: vec![
            Arg::Lit("pair".into()),
            Arg::Given("part".into()),
            Arg::SetFrom("KLQ_PART".into(), "part".into()),
            Arg::SetFrom("KLQ_TO".into(), "to".into()),
            Arg::SetFrom("KLQ_DRY".into(), "dry".into()),
        ],
    };
    jobs.push(Job {
        id: ID.into(),
        group: "protocol",
        title: "query".into(),
        file: DIR.into(),
        about: "the bridge written over the two languages' rulesets, then its pairs put to the part where one is chosen".into(),
        params: vec![choice("from", &languages, true), choice("to", &languages, true), choice("part", &parts, false), choice("dry", &["dry".to_string()], false)],
        opens: "",
        steps: vec![bridge, pairs],
    });
}

/// The values the command line's words name, as the job takes them, each `key=value`: two languages
/// after `--`, then a part where a third is given, each held to the files that make it whole, and
/// `--dry` where the pairs are to stop before the driver, which takes a part.
pub fn values(root: &Path, words: &[String]) -> Result<Vec<String>, String> {
    let usage = "query takes --<from> --<to> [--<part>] [--dry]: two languages, the part to run on, and dry to launch nothing";
    let dry = words.iter().any(|word| word == "--dry");
    let names: Vec<&str> = words.iter().filter(|word| *word != "--dry").map(|word| word.strip_prefix("--").filter(|name| !name.is_empty() && !name.contains(['/', '\\', '.']))).collect::<Option<_>>().ok_or(usage)?;
    if names.len() < 2 || names.len() > 3 || (dry && names.len() != 3) {
        return Err(usage.to_string());
    }
    let mut missing = Vec::new();
    for (at, name) in names.iter().enumerate() {
        let kinds: &[&str] = if at < 2 { &LANGUAGE_FILES } else { &PART_FILES };
        missing.extend(kinds.iter().map(|kind| format!("{DIR}/{name}.{kind}")).filter(|file| !root.join(file).is_file()));
    }
    if !root.join(DIR).join("Lstar.klq").is_file() {
        missing.push(format!("{DIR}/Lstar.klq"));
    }
    if !missing.is_empty() {
        return Err(format!("query {}: not in the tree: {}", words.join(" "), missing.join(", ")));
    }
    let mut given: Vec<String> = ["from", "to", "part"].iter().zip(&names).map(|(key, name)| format!("{key}={name}")).collect();
    if dry {
        given.push("dry=dry".into());
    }
    Ok(given)
}

#[cfg(test)]
mod words {
    use super::*;
    use crate::runner;
    use std::collections::HashMap;
    use std::fs;
    use std::path::PathBuf;

    fn said(words: &[&str]) -> Vec<String> {
        words.iter().map(|word| word.to_string()).collect()
    }

    /// A tree under the top's build/ holding the files named in the bridge's folder, each empty.
    fn tree(name: &str, files: &[&str]) -> PathBuf {
        let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("../../../build/cli_test").join(format!("query_{name}_{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        for file in files {
            let path = root.join(DIR).join(file);
            fs::create_dir_all(path.parent().unwrap()).unwrap();
            fs::write(path, "").unwrap();
        }
        root
    }

    const WHOLE: [&str; 9] = ["Lstar.klq", "cu.klm", "cu.krs", "sass.klm", "sass.krs", "sm_86.khw", "sm_86.kqr", "sm_86.ksc", "c.krs"];

    #[test]
    fn the_choices_are_what_the_tree_holds_whole() {
        let root = tree("choices", &WHOLE);
        let mut found = Vec::new();
        jobs(&root, &mut found);
        let job = &found[0];
        assert_eq!(job.params[0].choices, ["cu", "sass"], "c has a ruleset and no map, so it is no language yet");
        assert_eq!(job.params[2].choices, ["sm_86"]);
        assert!(!job.params[2].required);
        let _ = fs::remove_dir_all(&root);
    }

    #[test]
    fn the_words_are_the_job_s_values() {
        let root = tree("values", &WHOLE);
        assert_eq!(values(&root, &said(&["--cu", "--sass"])).unwrap(), ["from=cu", "to=sass"]);
        assert_eq!(values(&root, &said(&["--cu", "--sass", "--sm_86"])).unwrap(), ["from=cu", "to=sass", "part=sm_86"]);
        assert!(values(&root, &said(&["--cu"])).is_err());
        assert!(values(&root, &said(&["cu", "--sass"])).is_err());
        assert!(values(&root, &said(&["--cu", "--sass", "--sm_86", "--more"])).is_err());
        assert!(values(&root, &said(&["--../cu", "--sass"])).is_err());
        assert_eq!(values(&root, &said(&["--cu", "--sass", "--sm_86", "--dry"])).unwrap(), ["from=cu", "to=sass", "part=sm_86", "dry=dry"]);
        assert!(values(&root, &said(&["--cu", "--sass", "--dry"])).is_err(), "dry runs the pairs, which take a part");
        let _ = fs::remove_dir_all(&root);
    }

    #[test]
    fn a_file_not_there_is_said() {
        let root = tree("short", &WHOLE);
        let part = values(&root, &said(&["--cu", "--sass", "--sm_89"])).err().unwrap();
        assert!(part.contains("sm_89.khw") && part.contains("sm_89.kqr") && part.contains("sm_89.ksc") && !part.contains("cu.klm"));
        let host = values(&root, &said(&["--c", "--rs", "--host"])).err().unwrap();
        assert!(host.contains("c.klm") && !host.contains("c.krs") && host.contains("rs.krs") && host.contains("host.khw"));
        let _ = fs::remove_dir_all(&root);
    }

    #[test]
    fn the_pairs_step_runs_only_where_a_part_is_chosen() {
        let root = tree("steps", &WHOLE);
        let mut found = Vec::new();
        jobs(&root, &mut found);
        let job = &found[0];
        let given = |words: &[&str]| -> HashMap<String, Vec<String>> { words.iter().filter_map(|word| word.split_once('=')).map(|(key, value)| (key.to_string(), vec![value.to_string()])).collect() };
        let bridge_only = runner::shown(&root, job, &given(&["from=cu", "to=sass"])).unwrap();
        assert_eq!(bridge_only.len(), 1);
        assert!(bridge_only[0].contains("cu.krs sass.krs"));
        let on_part = runner::shown(&root, job, &given(&["from=cu", "to=sass", "part=sm_86"])).unwrap();
        assert_eq!(on_part.len(), 2);
        assert!(on_part[1].contains("KLQ_PART=sm_86") && on_part[1].contains("KLQ_TO=sass") && on_part[1].contains("pair"));
        assert!(!on_part[1].contains("KLQ_DRY"));
        let dry = runner::shown(&root, job, &given(&["from=cu", "to=sass", "part=sm_86", "dry=dry"])).unwrap();
        assert!(dry[1].contains("KLQ_DRY=dry"));
        let _ = fs::remove_dir_all(&root);
    }
}

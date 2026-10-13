// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The writers, each over the part's files and the rulesets and reaching no part: `klq_write`
//! writes `Lstar.klq` and each given language's `.klm` beside the rulesets; `krs_write` writes
//! `sass.krs` and `sm_86.krs` beside `sm_86.khw` from the forms the part answered for; `gsm_write`
//! writes `sm_86.gsm`, each relation the part answered as the cases it answered it on. And the
//! query: the bridge written over two languages' rulesets, then its pairs put to the part where one
//! is chosen.

use std::path::{Path, PathBuf};

use crate::commands::{Context, TABLE};
use crate::learn::Files;
use crate::tools::text;

/// Which writer the words name.
#[derive(Debug, PartialEq, Eq)]
pub enum Writer {
    /// The bridge over the rulesets given, or over `cu.krs`, `sass.krs` and `openqasm2_0.krs`.
    Bridge(Vec<String>),
    Krs,
    Gsm,
}

impl Writer {
    pub fn read(words: &[String]) -> Result<Writer, String> {
        let usage = "write takes bridge [<ruleset>...], krs or gsm";
        match words.split_first().map(|(first, rest)| (first.as_str(), rest)) {
            Some(("bridge", rulesets)) => Ok(Writer::Bridge(rulesets.to_vec())),
            Some(("krs", [])) => Ok(Writer::Krs),
            Some(("gsm", [])) => Ok(Writer::Gsm),
            _ => Err(usage.into()),
        }
    }
}

/// `Lstar.klq` and each given language's `.klm` written beside the rulesets (`klq_write`), teed as
/// `klq_write.console`. The writer's exit code.
pub fn bridge(context: &mut Context, rulesets: &[String]) -> Result<i32, String> {
    let klq_write = context.tool("klq_write")?;
    let given: Vec<String> = if rulesets.is_empty() { ["cu.krs", "sass.krs", "openqasm2_0.krs"].map(String::from).to_vec() } else { rulesets.to_vec() };
    let table = context.at(TABLE);
    let mut run = context.started("build/engine/klq/qry")?;
    let mut read: Vec<PathBuf> = given.iter().map(|ruleset| table.join(ruleset)).collect();
    read.push(table.join("Lstar.klq"));
    run.read(&read)?;
    let mut words = vec![text(&table)];
    words.extend(given);
    let code = context.teed(&run, "klq_write", &klq_write, &words.iter().map(String::as_str).collect::<Vec<_>>())?;
    context.ended(run)?;
    Ok(code)
}

/// `sass.krs` and `sm_86.krs` written beside `sm_86.khw` from the forms the part answered for
/// (`krs_write`), teed as `krs_write.console`. The writer's exit code.
fn krs(context: &mut Context) -> Result<i32, String> {
    let krs_write = context.tool("krs_write")?;
    let writer = context.tool("khw_machine_write")?;
    let files = Files::of(context.root, "sm_86");
    let table = context.at(TABLE);
    let mut run = context.started("build/engine/krs/qry")?;
    run.read(&[files.machine.clone(), files.costs.clone(), table.join("sm_86.krs"), table.join("sass.krs"), table.join("cu.krs"), files.layout.clone(), files.mnemonics.clone()])?;
    let words = ["sass".to_string(), "sm_86".to_string(), text(&files.machine), text(&files.layout), text(&files.mnemonics), text(&writer), text(&table.join("cu.krs"))];
    let code = context.teed(&run, "krs_write", &krs_write, &words.iter().map(String::as_str).collect::<Vec<_>>())?;
    context.ended(run)?;
    Ok(code)
}

/// `sm_86.gsm` written beside `sm_86.khw`, each relation the part answered as the cases it answered
/// it on (`gsm_write`). The writer's exit code.
fn gsm(context: &mut Context) -> Result<i32, String> {
    let gsm_write = context.tool("gsm_write")?;
    let writer = context.tool("khw_machine_write")?;
    let files = Files::of(context.root, "sm_86");
    let table = context.at(TABLE);
    let mut run = context.started("build/engine/gsm/qry")?;
    run.read(&[files.machine.clone(), table.join("sm_86.gsm"), files.layout.clone(), files.mnemonics.clone()])?;
    let words = ["sm_86".to_string(), text(&files.machine), text(&files.layout), text(&files.mnemonics), text(&writer)];
    let code = context.said(&run, &gsm_write, &words.iter().map(String::as_str).collect::<Vec<_>>())?;
    context.ended(run)?;
    Ok(code)
}

pub fn run(context: &mut Context, words: &[String]) -> Result<i32, String> {
    match Writer::read(words)? {
        Writer::Bridge(rulesets) => bridge(context, &rulesets),
        Writer::Krs => krs(context),
        Writer::Gsm => gsm(context),
    }
}

/// A query: the language a program is written in, the language it is to be written in, the part it
/// is to run on where one is chosen, and whether the pairs stop before the driver.
#[derive(Debug, PartialEq, Eq)]
pub struct Query {
    pub from: String,
    pub to: String,
    pub part: Option<String>,
    pub dry: bool,
}

/// The files a language is held whole by, and those a part is.
const LANGUAGE_FILES: [&str; 2] = ["klm", "krs"];

const PART_FILES: [&str; 3] = ["khw", "kqr", "ksc"];

/// The query the words name, each held to the files that make it whole: `--<from> --<to>
/// [--<part>] [--dry]`, or `--from <from> --to <to> [--part <part>] [--dry]` as the window's job
/// says it. A name the table does not hold whole is answered with each file missing.
pub fn query_words(root: &Path, words: &[String]) -> Result<Query, String> {
    let usage = "query takes --<from> --<to> [--<part>] [--dry]: two languages, the part to run on, and dry to launch nothing";
    let dry = words.iter().any(|word| word == "--dry");
    let plain: Vec<&String> = words.iter().filter(|word| *word != "--dry").collect();
    let names: Vec<String> = if plain.first().is_some_and(|w| ["--from", "--to", "--part"].contains(&w.as_str())) {
        let mut from = None;
        let mut to = None;
        let mut part = None;
        let mut at = 0;
        while at < plain.len() {
            let value = plain.get(at + 1).cloned().ok_or(usage)?;
            match plain[at].as_str() {
                "--from" => from = Some(value.clone()),
                "--to" => to = Some(value.clone()),
                "--part" => part = Some(value.clone()),
                _ => return Err(usage.into()),
            }
            at += 2;
        }
        [from, to, part].into_iter().flatten().collect()
    } else {
        plain.iter().map(|word| word.strip_prefix("--").map(str::to_string)).collect::<Option<Vec<String>>>().ok_or(usage)?
    };
    if names.iter().any(|name| name.is_empty() || name.contains(['/', '\\', '.'])) {
        return Err(usage.into());
    }
    if names.len() < 2 || names.len() > 3 || (dry && names.len() != 3) {
        return Err(usage.into());
    }
    let mut missing = Vec::new();
    for (at, name) in names.iter().enumerate() {
        let kinds: &[&str] = if at < 2 { &LANGUAGE_FILES } else { &PART_FILES };
        missing.extend(kinds.iter().map(|kind| format!("{TABLE}/{name}.{kind}")).filter(|file| !root.join(file).is_file()));
    }
    if !root.join(TABLE).join("Lstar.klq").is_file() {
        missing.push(format!("{TABLE}/Lstar.klq"));
    }
    if !missing.is_empty() {
        return Err(format!("query {}: not in the tree: {}", words.join(" "), missing.join(", ")));
    }
    Ok(Query { from: names[0].clone(), to: names[1].clone(), part: names.get(2).cloned(), dry })
}

/// The query run: the bridge over the two languages' rulesets, then its pairs put to the part
/// where one is chosen.
pub fn query(context: &mut Context, words: &[String]) -> Result<i32, String> {
    let query = query_words(context.root, words)?;
    let code = bridge(context, &[format!("{}.krs", query.from), format!("{}.krs", query.to)])?;
    if code != 0 {
        return Ok(code);
    }
    match query.part {
        Some(part) => crate::pairs::pair(context, &part, &query.to, query.dry, None, None),
        None => Ok(0),
    }
}

#[cfg(test)]
mod words {
    use super::*;

    fn said(words: &[&str]) -> Vec<String> {
        words.iter().map(|w| w.to_string()).collect()
    }

    /// A tree under build/ holding the files named in the bridge's folder, each empty.
    fn tree(name: &str, files: &[&str]) -> PathBuf {
        let root = crate::tree_root().join("build").join("query_test").join(format!("query_{name}_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&root);
        for file in files {
            let path = root.join(TABLE).join(file);
            std::fs::create_dir_all(path.parent().unwrap()).unwrap();
            std::fs::write(path, "").unwrap();
        }
        root
    }

    const WHOLE: [&str; 9] = ["Lstar.klq", "cu.klm", "cu.krs", "sass.klm", "sass.krs", "sm_86.khw", "sm_86.kqr", "sm_86.ksc", "c.krs"];

    #[test]
    fn the_query_s_words_are_read() {
        let root = tree("values", &WHOLE);
        assert_eq!(query_words(&root, &said(&["--cu", "--sass"])).unwrap(), Query { from: "cu".into(), to: "sass".into(), part: None, dry: false });
        assert_eq!(query_words(&root, &said(&["--cu", "--sass", "--sm_86"])).unwrap(), Query { from: "cu".into(), to: "sass".into(), part: Some("sm_86".into()), dry: false });
        assert_eq!(query_words(&root, &said(&["--from", "cu", "--to", "sass", "--part", "sm_86", "--dry"])).unwrap(), Query { from: "cu".into(), to: "sass".into(), part: Some("sm_86".into()), dry: true });
        assert!(query_words(&root, &said(&["--cu"])).is_err());
        assert!(query_words(&root, &said(&["cu", "--sass"])).is_err());
        assert!(query_words(&root, &said(&["--cu", "--sass", "--sm_86", "--more"])).is_err());
        assert!(query_words(&root, &said(&["--../cu", "--sass"])).is_err());
        assert!(query_words(&root, &said(&["--cu", "--sass", "--dry"])).is_err(), "dry runs the pairs, which take a part");
        let _ = std::fs::remove_dir_all(&root);
    }

    #[test]
    fn a_file_not_there_is_said() {
        let root = tree("short", &WHOLE);
        let part = query_words(&root, &said(&["--cu", "--sass", "--sm_89"])).unwrap_err();
        assert!(part.contains("sm_89.khw") && part.contains("sm_89.kqr") && part.contains("sm_89.ksc") && !part.contains("cu.klm"), "{part}");
        let host = query_words(&root, &said(&["--c", "--rs", "--host"])).unwrap_err();
        assert!(host.contains("c.klm") && !host.contains("c.krs") && host.contains("rs.krs") && host.contains("host.khw"), "{host}");
        let _ = std::fs::remove_dir_all(&root);
    }

    #[test]
    fn the_writers_take_their_words() {
        assert_eq!(Writer::read(&said(&["bridge"])).unwrap(), Writer::Bridge(Vec::new()));
        assert_eq!(Writer::read(&said(&["bridge", "cu.krs", "c.krs"])).unwrap(), Writer::Bridge(said(&["cu.krs", "c.krs"])));
        assert_eq!(Writer::read(&said(&["krs"])).unwrap(), Writer::Krs);
        assert_eq!(Writer::read(&said(&["gsm"])).unwrap(), Writer::Gsm);
        assert!(Writer::read(&said(&["krs", "more"])).is_err());
        assert!(Writer::read(&said(&["kdm"])).is_err());
        assert!(Writer::read(&[]).is_err());
    }
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The cross alone (measuring_stick_query.c): the latest listed questions carried into a run of
//! its own from the latest run that holds them, each question's container popped by the carrier
//! with the part taken out, and the cross read over them against the vendor's disassembler. It
//! reaches no part.

use std::path::Path;

use crate::commands::Context;
use crate::learn::{self, Files};
use crate::programs::Role;
use crate::run;
use crate::writer::Writer;

/// The base source the words name, where they do.
pub fn base_of(words: &[String]) -> Result<Option<String>, String> {
    match words {
        [] => Ok(None),
        [flag, path] if flag == "--base" => Ok(Some(path.clone())),
        _ => Err("cross takes [--base <cu>]".into()),
    }
}

fn text(path: &Path) -> String {
    path.to_string_lossy().into_owned()
}

pub fn run(context: &mut Context, words: &[String]) -> Result<i32, String> {
    let base = base_of(words)?;
    let files = Files::of(context.root, "sm_86");
    let carrier = context.tool("cubin_run")?;
    let cross = context.tool("measuring_stick_query")?;
    let nvdisasm = context.program(Role::Nvdisasm).map_err(|_| "no nvdisasm beside nvcc: the airlock has no reader".to_string())?;
    let mut here = context.started(learn::FOLDER)?;
    let folder = context.at(learn::FOLDER);
    if !here.joined() {
        // the latest listed questions carried in, with every code and cases the list names
        let latest = run::latest(&folder, "questions.txt", Some(&here.writer().qry), here.writer(), None).ok_or_else(|| format!("no run in {} holds a questions.txt to read", folder.display()))?;
        let other = Writer { binary: here.writer().binary.clone(), qry: latest.clone(), top: here.writer().top.clone(), path: here.writer().path.clone() };
        let list = other.latest("questions.txt").unwrap_or_default();
        let mut names = String::from("questions.txt\n");
        for line in String::from_utf8_lossy(&list).lines() {
            let words: Vec<&str> = line.split_whitespace().collect();
            if words.len() >= 3 {
                names.push_str(words[0]);
                names.push('\n');
                names.push_str(words[2]);
                names.push('\n');
            }
        }
        let said = here.writer().carry_listed(&latest, &names)?;
        context.say(&format!("  {said}"));
        let work = context.at(learn::WORK);
        let forms_work = text(&work.join("forms_work.txt"));
        let forms = text(&work.join("forms.txt"));
        here.carried(&folder, &[forms_work.as_str(), forms.as_str(), "held.txt"], None)?;
    }
    if here.writer().latest("questions.txt").is_none() {
        return Err("the run holds no questions.txt".into());
    }
    let mut read = vec![files.machine.clone(), files.layout.clone()];
    if let Some(base) = &base {
        read.push(base.into());
    }
    here.read(&read)?;
    if context.handed(&here, "cubin_run.txt", &carrier, &["--dry", "--full", &text(&files.machine), &text(&files.layout), "--list", "questions.txt"])? != 0 {
        return Err("the carrier did not read the list".into());
    }
    if let Some(base) = &base {
        learn::base_compiled(context, &here, Path::new(base))?;
    }
    let code = context.said(&here, &cross, &["questions.txt", &text(&nvdisasm), "SM86", "measuring_stick_query.md", "held.txt"])?;
    context.say(&format!("  the report is the blob measuring_stick_query.md of {}; the vendor's feedback is its blob held.txt", here.writer().qry.display()));
    context.say("  a live run holds those encodings with: cubin_run --held held.txt ...");
    context.ended(here)?;
    if code == 2 {
        context.say("  cross: the vendor's reader was not reached");
    }
    Ok(code)
}

#[cfg(test)]
mod words {
    use super::*;

    #[test]
    fn the_base_is_the_one_word_the_cross_takes() {
        let said = |words: &[&str]| base_of(&words.iter().map(|w| w.to_string()).collect::<Vec<_>>());
        assert_eq!(said(&[]).unwrap(), None);
        assert_eq!(said(&["--base", "a.cu"]).unwrap(), Some("a.cu".to_string()));
        assert!(said(&["a.cu"]).is_err());
        assert!(said(&["--base"]).is_err());
    }
}

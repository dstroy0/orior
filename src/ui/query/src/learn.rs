// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The part passes (khw_write.c, run_channel.h): a part pass is one live run whose every round is
//! checked before the part by the airlock, the carrier with the part taken out popping each
//! question's container and the cross reading them against the vendor's disassembler, and fixed
//! where a count of registers fixes it. `discover` seeds the kernel's own form into the working set
//! the first time and after that widens the forms the part has classified a round; `seed` is a
//! discover pass seeded as well from a listing of the vendor's compiler; `turns` asks the turns of
//! the forms a discover left; `registers` sets the runs those find to the two registers. `rounds`
//! widens for a count of rounds; `dry` puts every question to the dry carrier and holds each to the
//! gate with the part taken out; `enumerate` emits one slot's questions for the cross to read.

use std::path::{Path, PathBuf};

use crate::commands::{Context, TABLE};
use crate::programs::Role;
use crate::run::Run;
use crate::writer::Priority;

const NVIDIA: &str = "src/cu/transpiler/vendor_bin_layouts/nvidia";

/// The run folder of every part pass.
pub const FOLDER: &str = "build/engine/khw/qry";

/// The working folder beside it: the forms lists and the dry machine file.
pub const WORK: &str = "build/engine/khw";

/// The part's files in the tree and the vendor's tables.
pub struct Files {
    pub machine: PathBuf,
    pub costs: PathBuf,
    pub record: PathBuf,
    pub layout: PathBuf,
    pub mnemonics: PathBuf,
    pub fields: PathBuf,
}

impl Files {
    pub fn of(root: &Path, part: &str) -> Files {
        let table = root.join(TABLE);
        let nvidia = root.join(NVIDIA);
        Files {
            machine: table.join(format!("{part}.khw")),
            costs: table.join(format!("{part}.ksc")),
            record: table.join(format!("{part}.kqr")),
            layout: nvidia.join("elf64_nvidia.tsv"),
            mnemonics: nvidia.join("mnemonic_nvidia.tsv"),
            fields: nvidia.join("sass_nvidia.tsv"),
        }
    }
}

/// What a learn's words ask for.
#[derive(Debug, PartialEq, Eq)]
pub enum Step {
    /// discover, turns or registers, with a base source where one is given.
    Pass(String, Option<String>),
    /// A discover pass seeded from the listing named.
    Seed(String),
    /// The forms widened for a count of rounds, through the airlock.
    Rounds(u32),
    /// Every question put dry for a count of rounds, each held to the gate with the part taken out.
    Dry(u32),
    /// One slot's questions emitted for the cross to read.
    Enumerate(String),
}

impl Step {
    pub fn read(words: &[String]) -> Result<Step, String> {
        let usage = "learn takes discover|turns|registers [--base <cu>], seed <listing.sass>, rounds [<n>], dry [<n>] or enumerate <slot>";
        let (first, rest) = words.split_first().ok_or(usage)?;
        let base = || -> Result<Option<String>, String> {
            match rest {
                [] => Ok(None),
                [flag, path] if flag == "--base" => Ok(Some(path.clone())),
                _ => Err(usage.into()),
            }
        };
        let count = || -> Result<u32, String> { rest.first().map(|n| n.parse::<u32>().map_err(|_| usage.to_string())).transpose().map(|n| n.unwrap_or(1)) };
        match first.as_str() {
            "discover" | "turns" | "registers" => Ok(Step::Pass(first.clone(), base()?)),
            "seed" => rest.first().map(|listing| Step::Seed(listing.clone())).ok_or(usage.into()),
            "rounds" => Ok(Step::Rounds(count()?)),
            "dry" => Ok(Step::Dry(count()?)),
            "enumerate" => rest.first().map(|slot| Step::Enumerate(slot.clone())).ok_or(usage.into()),
            _ => Err(usage.into()),
        }
    }
}

fn text(path: &Path) -> String {
    path.to_string_lossy().into_owned()
}

/// The carrier's words for a live pass: the held list it will not reach the part without, the
/// lessons the conferences taught where `lessons`, and the airlock, the pop and the cross.
pub fn carrier_words(carrier: &Path, cross: &Path, nvdisasm: &Path, machine: &Path, layout: &Path, lessons: bool) -> Vec<String> {
    let mut words = vec![text(carrier), "--held".into(), "held.txt".into()];
    if lessons {
        words.extend(["--lessons".into(), "overseer.txt".into()]);
    }
    words.extend([text(machine), text(layout), "--pop".into(), text(carrier), "--dry".into(), "--full".into()]);
    if lessons {
        words.extend(["--lessons".into(), "overseer.txt".into()]);
    }
    words.extend([text(machine), text(layout), "--cross".into(), text(cross), text(nvdisasm), "SM86".into(), "airlock.md".into(), "held.txt".into()]);
    words
}

/// The base source through the vendor's compiler, what it said handed as `nvcc.txt`, its container
/// as `base_nvcc.cubin` and its listing as `base_nvcc.sass`.
pub fn base_compiled(context: &mut Context, run: &Run, base: &Path) -> Result<(), String> {
    if !base.is_file() {
        return Ok(());
    }
    let nvcc = context.program(Role::Nvcc)?;
    let cuobjdump = context.program(Role::Cuobjdump)?;
    let cubin = context.at(WORK).join("base_nvcc.cubin");
    let compiled = context.handed(run, "nvcc.txt", &nvcc, &["-cubin", "-arch=sm_86", "-O3", "-o", &text(&cubin), &text(base)])? == 0;
    let mut listed = false;
    if compiled {
        if let Ok(bytes) = std::fs::read(&cubin) {
            run.writer().hand("base_nvcc.cubin", &bytes, Priority::Ordinary)?;
        }
        listed = context.handed(run, "base_nvcc.sass", &cuobjdump, &["-sass", &text(&cubin)])? == 0;
    }
    let name = base.file_name().map(|n| n.to_string_lossy().into_owned()).unwrap_or_default();
    context.say(&if listed { format!("  the vendor's compiler read {name}; its listing is the blob base_nvcc.sass") } else { format!("  the vendor's compiler did not read {name}") });
    let _ = std::fs::remove_file(&cubin);
    Ok(())
}

/// nvdisasm beside nvcc, which the airlock reads with; without it nothing goes to the part.
fn nvdisasm(context: &Context) -> Result<PathBuf, String> {
    context.program(Role::Nvdisasm).map_err(|_| "no nvdisasm beside nvcc: the airlock has no reader, and nothing goes to the part".to_string())
}

/// The lines of the latest blob of `name`, counted.
fn lines(run: &Run, name: &str) -> usize {
    run.writer().latest(name).map(|bytes| String::from_utf8_lossy(&bytes).lines().filter(|l| !l.trim().is_empty()).count()).unwrap_or(0)
}

/// The forms lists and the faults carried in, and the lessons where `lessons`.
fn carried(context: &Context, run: &Run, lessons: bool) -> Result<(), String> {
    let work = context.at(WORK);
    let forms_work = text(&work.join("forms_work.txt"));
    let forms = text(&work.join("forms.txt"));
    let mut files = vec![forms_work.as_str(), forms.as_str(), "faulted.txt"];
    if lessons {
        files.push("overseer.txt");
    }
    run.carried(&context.at(FOLDER), &files, crate::run::crystal(context.root).as_deref())
}

/// The teacher's first words: the part, its machine file, the vendor's tables and the vendor's writer.
fn teacher_words(machine: &Path, files: &Files, writer: &Path) -> Vec<String> {
    vec!["sm_86".into(), text(machine), text(&files.layout), text(&files.mnemonics), text(&files.fields), text(writer)]
}

fn as_words(words: &[String]) -> Vec<&str> {
    words.iter().map(String::as_str).collect()
}

/// The gate's verdicts counted off the dry record, as `khw_write.sh dry` prints them.
fn verdicts_said(context: &mut Context, run: &Run) {
    let dry = run.writer().read("dry.txt").unwrap_or_default();
    let mut counts: std::collections::BTreeMap<String, usize> = std::collections::BTreeMap::new();
    for line in String::from_utf8_lossy(&dry).lines() {
        if let Some(at) = line.find("the gate: ") {
            let verdict: String = line[at..].chars().take_while(|c| c.is_ascii_lowercase() || *c == ' ' || *c == ',' || *c == ':').collect();
            *counts.entry(verdict.trim_end().to_string()).or_default() += 1;
        }
    }
    for (verdict, count) in counts {
        context.say(&format!("{count:7} {verdict}"));
    }
}

pub fn run(context: &mut Context, words: &[String]) -> Result<i32, String> {
    let step = Step::read(words)?;
    let files = Files::of(context.root, "sm_86");
    let writer = context.tool("khw_machine_write")?;
    let carrier = context.tool("cubin_run")?;
    let teacher = context.tool("khw_write")?;
    let work = context.at(WORK);
    std::fs::create_dir_all(&work).map_err(|e| format!("{}: {e}", work.display()))?;
    match step {
        Step::Pass(..) | Step::Seed(..) => {
            let cross = context.tool("measuring_stick_query")?;
            let nvdisasm = nvdisasm(context)?;
            let (step_word, base) = match &step {
                Step::Pass(name, base) => (format!("--{name}"), base.clone()),
                _ => ("--discover".to_string(), None),
            };
            let mut run = context.started(FOLDER)?;
            carried(context, &run, true)?;
            let mut read = vec![files.machine.clone(), files.costs.clone(), files.record.clone(), files.layout.clone(), files.mnemonics.clone(), files.fields.clone()];
            if let Some(base) = &base {
                read.push(PathBuf::from(base));
            }
            run.read(&read)?;
            let mut words = teacher_words(&files.machine, &files, &writer);
            words.push(step_word);
            if let Step::Seed(listing) = &step {
                let seeds = context.tool("sass_listing_seeds")?;
                if context.handed(&run, "seeds.txt", &seeds, &[listing])? != 0 {
                    return Err("the seeds were not handed".into());
                }
                words.extend(["--seeds".into(), "seeds.txt".into()]);
            }
            if let Some(base) = &base {
                base_compiled(context, &run, Path::new(base))?;
            }
            words.push("--".into());
            words.extend(carrier_words(&carrier, &cross, &nvdisasm, &files.machine, &files.layout, true));
            let code = context.teed(&run, "khw_write", &teacher, &as_words(&words))?;
            context.ended(run)?;
            Ok(code)
        }
        Step::Rounds(rounds) => {
            let cross = context.tool("measuring_stick_query")?;
            let nvdisasm = nvdisasm(context)?;
            let mut run = context.started(FOLDER)?;
            carried(context, &run, false)?;
            run.read(&[files.machine.clone(), files.costs.clone(), files.record.clone(), files.layout.clone(), files.mnemonics.clone(), files.fields.clone()])?;
            let mut words = teacher_words(&files.machine, &files, &writer);
            words.extend([rounds.to_string(), "--".into()]);
            words.extend(carrier_words(&carrier, &cross, &nvdisasm, &files.machine, &files.layout, false));
            let code = context.said(&run, &teacher, &as_words(&words))?;
            context.ended(run)?;
            Ok(code)
        }
        Step::Dry(rounds) => {
            // the dry machine file and the costs and record beside it, under the working folder, not the tree's
            let machine = work.join("sm_86.khw");
            let mut run = context.started(FOLDER)?;
            carried(context, &run, false)?;
            run.read(&[machine.clone(), work.join("sm_86.ksc"), work.join("sm_86.kqr"), files.layout.clone(), files.mnemonics.clone(), files.fields.clone()])?;
            let mut words = teacher_words(&machine, &files, &writer);
            words.extend([rounds.to_string(), "--".into(), "dry".into()]);
            if context.said(&run, &teacher, &as_words(&words))? != 0 {
                return Err("the protocol did not run dry".into());
            }
            let questions = lines(&run, "questions.txt");
            if questions == 0 {
                return Err("the protocol put no question".into());
            }
            context.say(&format!("  the protocol put {questions} questions; each held to cubin_safe with the part taken out:"));
            if context.said(&run, &carrier, &["--dry", &text(&machine), &text(&files.layout), "--list", "questions.txt"])? != 0 {
                return Err("the carrier did not read the list".into());
            }
            verdicts_said(context, &run);
            context.ended(run)?;
            Ok(0)
        }
        Step::Enumerate(slot) => {
            let machine = work.join("sm_86.khw");
            let mut run = context.started(FOLDER)?;
            carried(context, &run, false)?;
            run.read(&[machine.clone(), work.join("sm_86.ksc"), work.join("sm_86.kqr"), files.layout.clone(), files.mnemonics.clone(), files.fields.clone()])?;
            let forms = if lines(&run, "forms_work.txt") != 0 { "forms_work.txt" } else { "forms.txt" };
            let seeded = lines(&run, forms);
            let mut words = teacher_words(&machine, &files, &writer);
            words.extend(["--slot".into(), slot.clone()]);
            if seeded != 0 {
                words.extend(["--forms".into(), forms.into()]);
                context.say(&format!("  seeding from the {seeded} forms in {forms}"));
            }
            words.extend(["--".into(), "dry".into()]);
            if context.said(&run, &teacher, &as_words(&words))? != 0 {
                return Err("the enumerate did not run".into());
            }
            let questions = lines(&run, "questions.txt");
            if questions == 0 {
                return Err("the enumerate put no question".into());
            }
            context.say(&format!("  the slot {slot}'s questions: questions.txt of {}, {questions} of them", run.writer().qry.display()));
            context.say("  cross-check them: orior protocol cross [--base <cu>]");
            context.ended(run)?;
            Ok(0)
        }
    }
}

#[cfg(test)]
mod passes {
    use super::*;

    #[test]
    fn the_carrier_s_words_are_the_script_s() {
        let words = carrier_words(Path::new("C"), Path::new("X"), Path::new("N"), Path::new("M"), Path::new("L"), true);
        assert_eq!(words, ["C", "--held", "held.txt", "--lessons", "overseer.txt", "M", "L", "--pop", "C", "--dry", "--full", "--lessons", "overseer.txt", "M", "L", "--cross", "X", "N", "SM86", "airlock.md", "held.txt"]);
        let plain = carrier_words(Path::new("C"), Path::new("X"), Path::new("N"), Path::new("M"), Path::new("L"), false);
        assert_eq!(plain, ["C", "--held", "held.txt", "M", "L", "--pop", "C", "--dry", "--full", "M", "L", "--cross", "X", "N", "SM86", "airlock.md", "held.txt"]);
    }

    #[test]
    fn the_steps_words_are_read() {
        let said = |words: &[&str]| Step::read(&words.iter().map(|w| w.to_string()).collect::<Vec<_>>());
        assert_eq!(said(&["turns"]).unwrap(), Step::Pass("turns".into(), None));
        assert_eq!(said(&["turns", "--base", "a.cu"]).unwrap(), Step::Pass("turns".into(), Some("a.cu".into())));
        assert_eq!(said(&["seed", "listing.sass"]).unwrap(), Step::Seed("listing.sass".into()));
        assert_eq!(said(&["rounds", "3"]).unwrap(), Step::Rounds(3));
        assert_eq!(said(&["dry"]).unwrap(), Step::Dry(1));
        assert_eq!(said(&["dry", "2"]).unwrap(), Step::Dry(2));
        assert_eq!(said(&["enumerate", "loop_back_if"]).unwrap(), Step::Enumerate("loop_back_if".into()));
        assert!(said(&["enumerate"]).is_err());
        assert!(said(&["seed"]).is_err());
        assert!(said(&["rounds", "many"]).is_err());
        assert!(said(&["fly"]).is_err());
        assert!(said(&[]).is_err());
    }

    #[test]
    fn the_part_files_are_beside_the_bridge() {
        let files = Files::of(Path::new("T"), "sm_86");
        assert_eq!(files.machine, Path::new("T").join(TABLE).join("sm_86.khw"));
        assert_eq!(files.record, Path::new("T").join(TABLE).join("sm_86.kqr"));
        assert_eq!(files.fields, Path::new("T/src/cu/transpiler/vendor_bin_layouts/nvidia/sass_nvidia.tsv"));
    }
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The bridge's pairs put to the part, and the other modes of `klq_identity` (klq_identity.cu):
//! `stall`, `register`, `past`, `queue`, `cost` and `curve` over the stick's questions and the
//! host's answers to them, `text_identity` over every identity between texts in `Lstar.klq`, each
//! side a kernel of a stick of its own, and `stick`, the no-argument run, which slices the stick's
//! questions into identities, computes the integer questions on the host over every case, and
//! sifts the engine's writing through what nvcc writes. Every mode that puts questions runs them
//! through the airlock, and the decoder reads a pair run's log back into the bridge.

use std::path::{Path, PathBuf};

use crate::commands::{Context, TABLE};
use crate::learn::Files;
use crate::programs::Role;
use crate::run::Run;
use crate::tools::text;

const NVIDIA: &str = "src/cu/transpiler/vendor_bin_layouts/nvidia";

/// The stick's folder: the vendor's reading of it and, under `engine/`, the engine's.
const STICK: &str = "build/measuring_stick";

/// The identity's working folder.
const WORK: &str = "build/engine/identity";

/// The run folder where no trace folder is named.
const FOLDER: &str = "build/engine/qry";

const MODES: &[&str] = &["frame", "fixed", "pair", "cost", "stall", "register", "past", "queue", "curve", "text_identity", "stick"];

/// What a pairs' words ask for: the mode, its own words, the part and the language, dry or not,
/// the seed the links' order is drawn from, and the folder the run goes in.
#[derive(Debug, PartialEq, Eq)]
pub struct Mode {
    pub mode: String,
    pub words: Vec<String>,
    pub part: String,
    pub to: String,
    pub dry: bool,
    pub seed: Option<String>,
    pub trace: Option<PathBuf>,
}

impl Mode {
    pub fn read(words: &[String]) -> Result<Mode, String> {
        let usage = "pairs takes frame|fixed|pair|cost|stall|register|past|queue|curve|text_identity|stick [<words>] [--part <part>] [--to <language>] [--dry] [--seed <n>] [--trace <folder>]";
        let (mode, rest) = words.split_first().ok_or(usage)?;
        if !MODES.contains(&mode.as_str()) {
            return Err(usage.into());
        }
        let mut read = Mode { mode: mode.clone(), words: Vec::new(), part: "sm_86".into(), to: "sass".into(), dry: false, seed: None, trace: None };
        let mut at = 0;
        while at < rest.len() {
            let word = rest[at].as_str();
            let value = || rest.get(at + 1).cloned().ok_or_else(|| format!("{word} takes a value"));
            match word {
                "--part" => read.part = value()?,
                "--to" => read.to = value()?,
                "--seed" => read.seed = Some(value()?),
                "--trace" => read.trace = Some(PathBuf::from(value()?)),
                "--dry" => {
                    read.dry = true;
                    at += 1;
                    continue;
                }
                _ => {
                    read.words.push(word.to_string());
                    at += 1;
                    continue;
                }
            }
            at += 2;
        }
        if read.to != "sass" {
            return Err(format!("klq_identity writes forms in sass alone, and --to names {}", read.to));
        }
        Ok(read)
    }
}

/// The part's name as the vendor's reader takes it: `sm_86` is `SM86`.
pub fn part_read(part: &str) -> String {
    part.chars().filter(|c| *c != '_').collect::<String>().to_ascii_uppercase()
}

/// The words a mode's program is given after its folder: the mode's own targets, or the words given.
pub fn targets(root: &Path, mode: &str, part: &str, words: &[String]) -> Vec<String> {
    let table = root.join(TABLE);
    match mode {
        "queue" => vec![text(&root.join(STICK).join("measuring_stick.tsv"))],
        "pair" => vec![text(&table.join("Lstar.klq")), text(&table.join(format!("{part}.khw")))],
        "stall" => vec![text(&table.join(format!("{part}.khw")))],
        "cost" => vec![text(&table.join(format!("{part}.kdm"))), text(&table.join(format!("{part}.khw")))],
        _ => words.to_vec(),
    }
}

/// The vendor writer's words that hand the kernel the system accepted to the run as `kernel.txt`: the
/// part, its machine file, the container's layout, the vendor's table, the blob's name and the mode.
pub fn kernel_words(part: &str, files: &Files) -> Vec<String> {
    vec![part.to_string(), text(&files.machine), text(&files.layout), text(&files.mnemonics), "kernel.txt".into(), "kernel".into()]
}

/// Whether a mode opens with the kernel the system accepted: `frame` and `fixed`, which read it, and
/// `pair`, whose opening runs both.
pub fn kernel_handed(mode: &str) -> bool {
    matches!(mode, "frame" | "fixed" | "pair")
}

/// The pairs of the bridge put to `part` in the language `to`, dry where `dry`.
pub fn pair(context: &mut Context, part: &str, to: &str, dry: bool, seed: Option<&str>, trace: Option<&Path>) -> Result<i32, String> {
    let mode = Mode { mode: "pair".into(), words: Vec::new(), part: part.into(), to: to.into(), dry, seed: seed.map(str::to_string), trace: trace.map(Path::to_path_buf) };
    if mode.to != "sass" {
        return Err(format!("klq_identity writes forms in sass alone, and --to names {}", mode.to));
    }
    run_mode(context, &mode)
}

pub fn run(context: &mut Context, words: &[String]) -> Result<i32, String> {
    let mode = Mode::read(words)?;
    run_mode(context, &mode)
}

/// The carrier's words for every mode that puts questions: the carrier, its dry flags first where
/// `dry` as it reads them, then its held list and its lessons, the machine file and the layout; the
/// pop, dry and full; and the cross with the vendor's reader.
pub fn carry_words(carrier: &Path, cross: &Path, nvdisasm: &Path, machine: &Path, layout: &Path, part: &str, dry: bool) -> Vec<String> {
    let mut words = vec![text(carrier)];
    if dry {
        words.extend(["--dry".into(), "--full".into()]);
    }
    words.extend(["--held".into(), "held.txt".into(), "--lessons".into(), "overseer.txt".into(), text(machine), text(layout)]);
    words.extend(["--pop".into(), text(carrier), "--dry".into(), "--full".into(), "--lessons".into(), "overseer.txt".into(), text(machine), text(layout)]);
    words.extend(["--cross".into(), text(cross), text(nvdisasm), part_read(part), "airlock.md".into(), "held.txt".into()]);
    words
}

/// The carrier's words with the tools built and the vendor's reader found: the airlock every round
/// passes before the part, the pop and the cross, and the carrier stopped before the driver where `dry`.
fn carry(context: &Context, part: &str, dry: bool) -> Result<Vec<String>, String> {
    let carrier = context.tool("cubin_run")?;
    let cross = context.tool("measuring_stick_query")?;
    let nvdisasm = context.program(Role::Nvdisasm).map_err(|_| "no nvdisasm beside nvcc: the airlock has no reader".to_string())?;
    let machine = context.at(TABLE).join(format!("{part}.khw"));
    let layout = context.at(NVIDIA).join("elf64_nvidia.tsv");
    Ok(carry_words(&carrier, &cross, &nvdisasm, &machine, &layout, part, dry))
}

/// The part's files, the bridge, the rulesets and the stick's manifest read into the run.
fn configs(context: &Context, run: &mut Run, part: &str) -> Result<(), String> {
    let table = context.at(TABLE);
    run.read(&[
        table.join(format!("{part}.khw")),
        table.join(format!("{part}.ksc")),
        table.join(format!("{part}.kqr")),
        table.join("Lstar.klq"),
        table.join("cu.krs"),
        table.join("sass.krs"),
        context.at(STICK).join("measuring_stick.tsv"),
    ])
}

/// The host's questions, the C++ the run holds as `host_questions.cpp`, compiled under `out` with
/// signed arithmetic wrapping as the part's does, run, and its answers handed as `host_answers.txt`.
fn host_answered(context: &mut Context, run: &Run, out: &Path) -> Result<(), String> {
    let questions = run.writer().latest("host_questions.cpp").ok_or("the run holds no host_questions.cpp")?;
    let source = out.join("host_questions.cpp");
    std::fs::write(&source, &questions).map_err(|e| format!("{}: {e}", source.display()))?;
    let program = out.join(crate::tools::binary_name("host_questions"));
    let cxx = context.program(Role::Cxx)?;
    let compiled = context.ran(run, &cxx, &["-std=c++17", "-O0", "-fwrapv", "-w", "-o", &text(&program), &text(&source)])?;
    if !compiled.status.success() {
        return Err(format!("the host questions did not compile\n{}", String::from_utf8_lossy(&compiled.stderr).trim_end()));
    }
    if context.handed(run, "host_answers.txt", &program, &[])? != 0 {
        return Err("the host questions did not run".into());
    }
    Ok(())
}

/// Every mode: the stick's listing and the part's files held to, the environment set, and the
/// mode run.
fn run_mode(context: &mut Context, mode: &Mode) -> Result<i32, String> {
    let stick = context.at(STICK);
    if !stick.join("measuring_stick_nvcc.sass").is_file() {
        return Err("no stick: run orior protocol stick".into());
    }
    let table = context.at(TABLE);
    for kind in ["khw", "ksc", "kqr"] {
        if !table.join(format!("{}.{kind}", mode.part)).is_file() {
            return Err(format!("no {}.{kind} beside the bridge: --part names no part the tree holds", mode.part));
        }
    }
    if let Some(seed) = &mode.seed {
        context.env.push(("KLQ_SEED".into(), seed.clone()));
    }
    if mode.dry {
        context.env.push(("KLQ_DRY".into(), "dry".into()));
    }
    let identity = context.tool("klq_identity")?;
    let folder = mode.trace.clone().map(|t| text(&t)).unwrap_or_else(|| FOLDER.to_string());
    let work = context.at(WORK);
    std::fs::create_dir_all(&work).map_err(|e| format!("{}: {e}", work.display()))?;
    match mode.mode.as_str() {
        "text_identity" => text_identity(context, mode, &identity, &folder),
        "stick" => stick_read(context, mode, &identity, &folder),
        _ => judged(context, mode, &identity, &folder),
    }
}

/// Every identity between texts in `Lstar.klq`: each side a kernel of a stick of its own, listed
/// by nvcc, written by the engine, computed on the host, and put to the part through R.
fn text_identity(context: &mut Context, mode: &Mode, identity: &Path, folder: &str) -> Result<i32, String> {
    let engine = context.tool("measuring_stick_engine")?;
    let python = context.program(Role::Python)?;
    let nvcc = context.program(Role::Nvcc)?;
    let cuobjdump = context.program(Role::Cuobjdump)?;
    let nvdisasm = context.program(Role::Nvdisasm)?;
    let ccbin = if cfg!(windows) { context.program(Role::Cl).ok().and_then(|cl| cl.parent().map(Path::to_path_buf)) } else { None };
    let table = context.at(TABLE);
    let stick = context.at(STICK);
    let mut run = context.started(folder)?;
    configs(context, &mut run, &mode.part)?;
    let sides = context.at(WORK).join("text_identity");
    let _ = std::fs::remove_dir_all(&sides);
    std::fs::create_dir_all(sides.join("engine")).map_err(|e| format!("{}: {e}", sides.display()))?;
    let cu = sides.join("measuring_stick.cu");
    let cubin = sides.join("measuring_stick_nvcc.cubin");
    let sass = sides.join("measuring_stick_nvcc.sass");
    if context.said(&run, &python, &[&text(&context.at(NVIDIA).join("measuring_stick.py")), &text(&cu), &text(&stick.join("measuring_stick.tsv")), &text(&table.join("Lstar.klq"))])? != 0 {
        return Err("the sides were not written".into());
    }
    let mut compile: Vec<String> = Vec::new();
    if let Some(ccbin) = &ccbin {
        compile.extend(["-ccbin".into(), text(ccbin)]);
    }
    compile.extend(["-cubin".into(), format!("-arch={}", mode.part), "-O3".into(), "-diag-suppress".into(), "177".into(), "-o".into(), text(&cubin), text(&cu)]);
    if context.said(&run, &nvcc, &compile.iter().map(String::as_str).collect::<Vec<_>>())? != 0 {
        return Err("nvcc did not compile the sides".into());
    }
    let listed = context.ran(&run, &cuobjdump, &["-sass", &text(&cubin)])?;
    if !listed.status.success() {
        return Err("cuobjdump did not read the sides".into());
    }
    std::fs::write(&sass, &listed.stdout).map_err(|e| format!("{}: {e}", sass.display()))?;
    if context.handed(&run, "assembler.log", &engine, &[&text(&cu), &text(&sass), &text(&sides.join("engine"))])? != 0 {
        return Err("the engine did not write the sides".into());
    }
    for code in std::fs::read_dir(sides.join("engine")).into_iter().flatten().flatten().map(|e| e.path()).filter(|p| p.extension().is_some_and(|e| e == "bin")) {
        let read = context.ran(&run, &nvdisasm, &["-b", &part_read(&mode.part), &text(&code)])?;
        let mut bytes = read.stdout;
        bytes.extend(&read.stderr);
        let _ = std::fs::write(code.with_extension("dis"), bytes);
    }
    if context.said(&run, identity, &["slice", &text(&sass), &text(&stick.join("measuring_stick.tsv")), &text(&cu), &text(&sides), &text(&sides.join("engine"))])? != 0 {
        return Err("the sides were not sliced".into());
    }
    host_answered(context, &run, &sides)?;
    let carry = carry(context, &mode.part, mode.dry)?;
    let mut words = vec!["text_identity".to_string(), text(&sides.join("engine")), "host_answers.txt".into(), text(&table.join(format!("{}.ksc", mode.part))), text(&sides), text(&table.join("Lstar.klq")), text(&sides.join("measuring_stick.tsv")), "--".into()];
    words.extend(carry);
    let code = context.teed(&run, "klq_identity", identity, &words.iter().map(String::as_str).collect::<Vec<_>>())?;
    context.ended(run)?;
    Ok(code)
}

/// The modes over the stick's questions and the host's answers: `stall`, `register`, `past`,
/// `queue`, `pair`, `cost` and `curve`, each put to the part through the airlock, and a pair run's
/// log read back by the decoder.
fn judged(context: &mut Context, mode: &Mode, identity: &Path, folder: &str) -> Result<i32, String> {
    let carry = carry(context, &mode.part, mode.dry)?;
    let table = context.at(TABLE);
    let stick = context.at(STICK);
    let work = context.at(WORK);
    let out = work.join(&mode.mode);
    std::fs::create_dir_all(&out).map_err(|e| format!("{}: {e}", out.display()))?;
    let mut run = context.started(folder)?;
    if kernel_handed(&mode.mode) {
        // the kernel the system accepted, handed to the run by the vendor's writer for the frame's asks
        let writer = context.tool("khw_machine_write")?;
        let files = Files::of(context.root, &mode.part);
        let words = kernel_words(&mode.part, &files);
        if context.said(&run, &writer, &words.iter().map(String::as_str).collect::<Vec<_>>())? != 0 {
            return Err("the vendor's writer handed the run no kernel".into());
        }
    }
    let host_answers = text(&work.join("host_answers.txt"));
    let overseer = text(&work.join("overseer.txt"));
    run.carried(&context.at(folder), &[host_answers.as_str(), overseer.as_str(), "faulted.txt"], crate::run::crystal(context.root).as_deref())?;
    if !matches!(mode.mode.as_str(), "frame" | "fixed") && run.writer().latest("host_answers.txt").is_none() {
        return Err("no host answers: run orior protocol pairs stick first".into());
    }
    configs(context, &mut run, &mode.part)?;
    if mode.mode == "cost" {
        run.read(&[table.join(format!("{}.kdm", mode.part))])?;
    }
    let mut words = vec![mode.mode.clone(), text(&stick.join("engine")), "host_answers.txt".into(), text(&table.join(format!("{}.ksc", mode.part))), text(&out)];
    words.extend(targets(context.root, &mode.mode, &mode.part, &mode.words));
    words.push("--".into());
    words.extend(carry);
    if context.teed(&run, "klq_identity", identity, &words.iter().map(String::as_str).collect::<Vec<_>>())? != 0 {
        context.ended(run)?;
        return Ok(1);
    }
    let mut code = 0;
    if mode.mode == "pair" && run.writer().latest("log").is_some() {
        let decoder = context.tool("klq_decoder")?;
        let qry = text(&run.writer().qry);
        code = context.teed(&run, "klq_decoder", &decoder, &[&qry, &text(&stick.join("engine")), &text(&table.join("Lstar.klq"))])?;
    }
    context.ended(run)?;
    Ok(code)
}

/// The stick's questions sliced into identities, the integer questions computed on the host over
/// every case, each identity's verdict written to `Lstar.klq` beside the forms of the rulesets
/// given, and the engine's writing of the stick sifted through what nvcc writes.
fn stick_read(context: &mut Context, mode: &Mode, identity: &Path, folder: &str) -> Result<i32, String> {
    let table = context.at(TABLE);
    let stick = context.at(STICK);
    let work = context.at(WORK);
    let rulesets: Vec<String> = if mode.words.is_empty() { vec!["sass.krs".into()] } else { mode.words.clone() };
    let mut run = context.started(folder)?;
    configs(context, &mut run, &mode.part)?;
    let sass = text(&stick.join("measuring_stick_nvcc.sass"));
    let tsv = text(&stick.join("measuring_stick.tsv"));
    let engine = text(&stick.join("engine"));
    if context.teed(&run, "klq_identity", identity, &["slice", &sass, &tsv, &text(&stick.join("measuring_stick.cu")), &text(&work), &engine])? != 0 {
        return Ok(1);
    }
    host_answered(context, &run, &work)?;
    let mut read = vec!["read".to_string(), text(&work), text(&table.join("Lstar.klq"))];
    read.extend(rulesets.iter().cloned());
    if context.teed(&run, "klq_identity", identity, &read.iter().map(String::as_str).collect::<Vec<_>>())? != 0 {
        return Ok(1);
    }
    if context.teed(&run, "klq_identity", identity, &["permute", &sass, &tsv, &text(&work)])? != 0 {
        return Ok(1);
    }
    let read_back = std::fs::read_dir(stick.join("engine")).into_iter().flatten().flatten().any(|e| e.path().extension().is_some_and(|x| x == "dis"));
    let mut code = 0;
    if read_back {
        if context.teed(&run, "klq_identity", identity, &["known", &sass, &tsv, &text(&work), &engine])? != 0 {
            return Ok(1);
        }
        let mut broken = vec!["broken".to_string(), sass.clone(), tsv.clone(), text(&work), engine.clone(), "--".into()];
        broken.extend(rulesets);
        code = context.teed(&run, "klq_identity", identity, &broken.iter().map(String::as_str).collect::<Vec<_>>())?;
    }
    context.ended(run)?;
    Ok(code)
}

#[cfg(test)]
mod modes {
    use super::*;

    fn said(words: &[&str]) -> Vec<String> {
        words.iter().map(|w| w.to_string()).collect()
    }

    #[test]
    fn the_part_s_name_for_the_vendor_s_reader() {
        assert_eq!(part_read("sm_86"), "SM86");
        assert_eq!(part_read("sm_120a"), "SM120A");
    }

    #[test]
    fn the_modes_and_their_targets() {
        let root = Path::new("T");
        let table = root.join(TABLE);
        // forward-slashed throughout
        assert_eq!(targets(root, "queue", "sm_86", &[]), [text(&root.join(STICK).join("measuring_stick.tsv"))]);
        assert_eq!(targets(root, "pair", "sm_86", &[]), [text(&table.join("Lstar.klq")), text(&table.join("sm_86.khw"))]);
        assert_eq!(targets(root, "stall", "sm_86", &[]), [text(&table.join("sm_86.khw"))]);
        assert_eq!(targets(root, "cost", "sm_86", &[]), [text(&table.join("sm_86.kdm")), text(&table.join("sm_86.khw"))]);
        assert!(targets(root, "pair", "sm_86", &[]).iter().all(|t| !t.contains('\\')));
        assert_eq!(targets(root, "curve", "sm_86", &said(&["1006", "1007"])), ["1006", "1007"]);
        assert_eq!(targets(root, "register", "sm_86", &[]), Vec::<String>::new());
        assert_eq!(targets(root, "frame", "sm_86", &[]), Vec::<String>::new());
        assert_eq!(targets(root, "fixed", "sm_86", &[]), Vec::<String>::new());
    }

    #[test]
    fn the_dry_carrier_s_flags_come_first_as_the_carrier_reads_them() {
        let (carrier, cross, nvdisasm, machine, layout) = (Path::new("C"), Path::new("X"), Path::new("N"), Path::new("M"), Path::new("L"));
        let dry = carry_words(carrier, cross, nvdisasm, machine, layout, "sm_86", true);
        assert_eq!(&dry[..9], ["C", "--dry", "--full", "--held", "held.txt", "--lessons", "overseer.txt", "M", "L"]);
        assert_eq!(&dry[9..], ["--pop", "C", "--dry", "--full", "--lessons", "overseer.txt", "M", "L", "--cross", "X", "N", "SM86", "airlock.md", "held.txt"]);
        let live = carry_words(carrier, cross, nvdisasm, machine, layout, "sm_86", false);
        assert_eq!(&live[..7], ["C", "--held", "held.txt", "--lessons", "overseer.txt", "M", "L"]);
        assert_eq!(&live[7..], &dry[9..]);
    }

    #[test]
    fn the_kernel_is_handed_before_the_frame_and_the_pairs() {
        let files = crate::learn::Files::of(Path::new("T"), "sm_86");
        let words = kernel_words("sm_86", &files);
        assert_eq!(words[0], "sm_86");
        assert!(words[1].ends_with("/table/sm_86.khw"), "{}", words[1]);
        assert!(words[2].ends_with("/nvidia/elf64_nvidia.tsv"), "{}", words[2]);
        assert!(words[3].ends_with("/nvidia/mnemonic_nvidia.tsv"), "{}", words[3]);
        assert_eq!(&words[4..], ["kernel.txt", "kernel"]);
        assert!(kernel_handed("frame") && kernel_handed("fixed") && kernel_handed("pair") && !kernel_handed("register"));
    }

    #[test]
    fn the_words_are_read() {
        let read = Mode::read(&said(&["pair", "--part", "sm_86", "--dry", "--seed", "2"])).unwrap();
        assert_eq!(read.mode, "pair");
        assert_eq!(read.part, "sm_86");
        assert!(read.dry);
        assert_eq!(read.seed.as_deref(), Some("2"));
        assert_eq!(read.to, "sass");
        assert!(read.words.is_empty());
        let curve = Mode::read(&said(&["curve", "1006", "--trace", "build/x"])).unwrap();
        assert_eq!(curve.words, ["1006"]);
        assert_eq!(curve.trace.as_deref(), Some(Path::new("build/x")));
        let stick = Mode::read(&said(&["stick", "sass.krs", "c.krs"])).unwrap();
        assert_eq!(stick.words, ["sass.krs", "c.krs"]);
        let frame = Mode::read(&said(&["frame", "--part", "sm_86"])).unwrap();
        assert_eq!(frame.mode, "frame");
        assert!(frame.words.is_empty());
        let fixed = Mode::read(&said(&["fixed", "--dry"])).unwrap();
        assert_eq!(fixed.mode, "fixed");
        assert!(fixed.dry && fixed.words.is_empty());
        assert!(Mode::read(&said(&["pair", "--to", "ptx"])).is_err(), "klq_identity writes forms in sass alone");
        assert!(Mode::read(&said(&["fly"])).is_err());
        assert!(Mode::read(&said(&["pair", "--seed"])).is_err());
        assert!(Mode::read(&[]).is_err());
    }
}

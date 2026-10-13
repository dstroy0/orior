// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The measuring stick: every function of the CUDA language in a kernel of its own
//! (measuring_stick.py), compiled by nvcc and disassembled, the answer key what the engine builds
//! for each function is held against; nvcc runs only where the stick's text changed since the
//! container beside it was compiled. The engine's stick: each kernel read form by form through
//! `cu.krs` and written in `sass.krs`, held against nvcc's listing on the host, its encodings read
//! back through nvdisasm. The read: nvcc's listing read kernel by kernel, and against `sass.krs`,
//! into `measuring_stick.md` (measuring_stick_read.c). Nothing goes to a device.

use std::path::{Path, PathBuf};

use crate::commands::{Context, TABLE};
use crate::programs::Role;
use crate::run::Run;

const NVIDIA: &str = "src/cu/transpiler/vendor_bin_layouts/nvidia";

/// The stick's folder, the vendor's reading of it: the generator's text and manifest, nvcc's
/// container and its listing.
pub const OUT: &str = "build/measuring_stick";

/// The vendor's reading of the stick, each file where the stick's folder keeps it.
pub struct Paths {
    pub out: PathBuf,
    pub cu: PathBuf,
    pub cu_new: PathBuf,
    pub tsv: PathBuf,
    pub cubin: PathBuf,
    pub sass: PathBuf,
}

pub fn vendor_paths(root: &Path) -> Paths {
    let out = root.join(OUT);
    Paths {
        cu: out.join("measuring_stick.cu"),
        cu_new: out.join("measuring_stick.cu.new"),
        tsv: out.join("measuring_stick.tsv"),
        cubin: out.join("measuring_stick_nvcc.cubin"),
        sass: out.join("measuring_stick_nvcc.sass"),
        out,
    }
}

/// `path` as the vendor's programs, Python and the engine take it: absolute, without the `\\?\`
/// prefix, its separators forward slashes, which every program of the tree parses and Windows
/// opens.
pub fn native(path: &Path) -> String {
    crate::run::plain(path).to_string_lossy().replace('\\', "/")
}

/// 1 where the stick's text at `new` differs from the one at `cu` nvcc compiled, or no container is there.
pub fn changed(cu: &Path, new: &Path, cubin: &Path) -> bool {
    let same = std::fs::read(cu).ok().zip(std::fs::read(new).ok()).is_some_and(|(before, after)| before == after);
    !(same && cubin.is_file())
}

/// What the stick's words name.
#[derive(Debug, PartialEq, Eq)]
pub enum What {
    /// The vendor's reading: the generator, nvcc where the text changed, cuobjdump, the read.
    Vendor,
    /// The engine's stick held against the vendor's.
    Engine,
    /// The read with the words given.
    Read(Vec<String>),
}

impl What {
    pub fn read(words: &[String]) -> Result<What, String> {
        let usage = "stick takes nothing, vendor, engine, or read <words>";
        match words.split_first().map(|(first, rest)| (first.as_str(), rest)) {
            None => Ok(What::Vendor),
            Some(("vendor", [])) => Ok(What::Vendor),
            Some(("engine", [])) => Ok(What::Engine),
            Some(("read", rest)) => Ok(What::Read(rest.to_vec())),
            _ => Err(usage.into()),
        }
    }
}

/// The text's generator and nvcc's container and listing: the generator run, nvcc where the text
/// changed, and the listing read by cuobjdump.
fn vendor_read(context: &mut Context, run: &Run) -> Result<(), String> {
    let paths = vendor_paths(context.root);
    std::fs::create_dir_all(&paths.out).map_err(|e| format!("{}: {e}", paths.out.display()))?;
    let python = context.program(Role::Python)?;
    let generator = context.at(NVIDIA).join("measuring_stick.py");
    if context.said(run, &python, &[&native(&generator), &native(&paths.cu_new), &native(&paths.tsv)])? != 0 {
        return Err("the stick was not written".into());
    }
    if !changed(&paths.cu, &paths.cu_new, &paths.cubin) {
        let _ = std::fs::remove_file(&paths.cu_new);
        context.say("  the stick is unchanged: nvcc's cubin kept");
        return Ok(());
    }
    std::fs::rename(&paths.cu_new, &paths.cu).map_err(|e| format!("{}: {e}", paths.cu.display()))?;
    let _ = std::fs::remove_file(&paths.cubin);
    let nvcc = context.program(Role::Nvcc)?;
    let cuobjdump = context.program(Role::Cuobjdump)?;
    let mut compile: Vec<String> = Vec::new();
    if cfg!(windows) {
        if let Some(ccbin) = context.program(Role::Cl).ok().and_then(|cl| cl.parent().map(Path::to_path_buf)) {
            compile.extend(["-ccbin".into(), native(&ccbin)]);
        }
    }
    compile.extend(["-cubin".into(), "-arch=sm_86".into(), "-O3".into(), "-diag-suppress".into(), "177".into(), "-o".into(), native(&paths.cubin), native(&paths.cu)]);
    if context.said(run, &nvcc, &compile.iter().map(String::as_str).collect::<Vec<_>>())? != 0 {
        return Err("nvcc did not compile the stick".into());
    }
    let listed = context.ran(run, &cuobjdump, &["-sass", &native(&paths.cubin)])?;
    if !listed.status.success() {
        return Err("cuobjdump did not read the stick".into());
    }
    std::fs::write(&paths.sass, &listed.stdout).map_err(|e| format!("{}: {e}", paths.sass.display()))?;
    Ok(())
}

/// The vendor's reading of the stick, and nvcc's listing read against `sass.krs` into the record.
fn vendor(context: &mut Context) -> Result<i32, String> {
    let reader = context.tool("measuring_stick_read")?;
    let paths = vendor_paths(context.root);
    let mut run = context.started(&format!("{OUT}/qry"))?;
    vendor_read(context, &run)?;
    let record = context.at(NVIDIA).join("measuring_stick.md");
    let krs = context.at(TABLE).join("sass.krs");
    // the record is the read's to write through the run's writer; named here as a leaf, so that it is shown and asked
    run.left(&[record.clone()]);
    let code = context.said(&run, &reader, &[&native(&paths.sass), &native(&paths.tsv), &native(&krs), &native(&record)])?;
    context.ended(run)?;
    Ok(code)
}

/// Every `.bin` under `folder` read back by nvdisasm into its `.dis`, as many at once as the host
/// has processors, each in the run's environment and the context's.
fn read_back(context: &Context, run: &Run, nvdisasm: &Path, folder: &Path) -> Result<(), String> {
    let codes: Vec<PathBuf> = std::fs::read_dir(folder).into_iter().flatten().flatten().map(|e| e.path()).filter(|p| p.extension().is_some_and(|x| x == "bin")).collect();
    let at_once = std::thread::available_parallelism().map(|n| n.get()).unwrap_or(4);
    let next = std::sync::atomic::AtomicUsize::new(0);
    let failed = std::sync::Mutex::new(None);
    let (root, env, writer) = (context.root, &context.env, run.writer());
    std::thread::scope(|scope| {
        for _ in 0..at_once.min(codes.len().max(1)) {
            scope.spawn(|| loop {
                let at = next.fetch_add(1, std::sync::atomic::Ordering::SeqCst);
                let Some(code) = codes.get(at) else { break };
                let mut command = writer.command_of(nvdisasm);
                command.envs(env.iter().map(|(k, v)| (k.as_str(), v.as_str()))).current_dir(root).stdin(std::process::Stdio::null());
                match command.args(["-b", "SM86", &native(code)]).output() {
                    Ok(read) => {
                        let mut bytes = read.stdout;
                        bytes.extend(&read.stderr);
                        let _ = std::fs::write(code.with_extension("dis"), bytes);
                    }
                    Err(said) => {
                        if let Ok(mut f) = failed.lock() {
                            *f = Some(format!("{}: {said}", nvdisasm.display()));
                        }
                    }
                }
            });
        }
    });
    match failed.into_inner().ok().flatten() {
        Some(said) => Err(said),
        None => Ok(()),
    }
}

/// The engine's stick: each kernel written through the rulesets, its encodings read back, and the
/// record written against nvcc's listing.
fn engine(context: &mut Context) -> Result<i32, String> {
    let engine = context.tool("measuring_stick_engine")?;
    let reader = context.tool("measuring_stick_read")?;
    let nvdisasm = context.program(Role::Nvdisasm)?;
    let paths = vendor_paths(context.root);
    let out = paths.out.join("engine");
    std::fs::create_dir_all(&out).map_err(|e| format!("{}: {e}", out.display()))?;
    for left in std::fs::read_dir(&out).into_iter().flatten().flatten().map(|e| e.path()) {
        if left.extension().and_then(|e| e.to_str()).is_some_and(|e| ["sass", "bin", "dis", "registers"].contains(&e)) {
            let _ = std::fs::remove_file(left);
        }
    }
    let table = context.at(TABLE);
    let record = context.at(NVIDIA).join("measuring_stick.md");
    let mut run = context.started(&format!("{OUT}/engine/qry"))?;
    run.read(&[table.join("cu.krs"), table.join("sass.krs"), table.join("sass.ksc"), table.join("sm_86.khw"), paths.cu.clone(), paths.sass.clone(), paths.tsv.clone()])?;
    run.left(&[table.join("sass.krs"), record.clone(), out.join("engine.tsv")]);
    if context.handed(&run, "assembler.log", &engine, &[&native(&paths.cu), &native(&paths.sass), &native(&out)])? != 0 {
        // the engine's refusal ends the loop as its script's did: the leaves handed as the run holds them, exit 1
        context.say("  the engine did not write the stick");
        context.ended(run)?;
        return Ok(1);
    }
    read_back(context, &run, &nvdisasm, &out)?;
    let code = context.said(&run, &reader, &[&native(&paths.sass), &native(&paths.tsv), &native(&table.join("sass.krs")), &native(&record), &native(&out.join("engine.tsv")), &native(&out)])?;
    context.ended(run)?;
    Ok(code)
}

/// The read with the words given, in the run joined or one of the stick's own.
fn read(context: &mut Context, words: &[String]) -> Result<i32, String> {
    let reader = context.tool("measuring_stick_read")?;
    let run = context.started(&format!("{OUT}/qry"))?;
    let code = context.said(&run, &reader, &words.iter().map(String::as_str).collect::<Vec<_>>())?;
    context.ended(run)?;
    Ok(code)
}

pub fn run(context: &mut Context, words: &[String]) -> Result<i32, String> {
    match What::read(words)? {
        What::Vendor => vendor(context),
        What::Engine => engine(context),
        What::Read(words) => read(context, &words),
    }
}

#[cfg(test)]
mod reading {
    use super::*;

    #[test]
    fn paths_given_to_the_vendor_are_native() {
        let paths = vendor_paths(Path::new("T"));
        assert!(paths.cu.ends_with("measuring_stick.cu"));
        let shown = native(&crate::tree_root().join("build").join("x.cu"));
        assert!(!shown.contains("\\\\?\\"), "{shown}");
        assert!(!shown.starts_with("/c/"), "no cygwin path reaches the vendor: {shown}");
        assert!(!shown.contains('\\'), "the separators are forward slashes, which the tree's programs parse: {shown}");
        if cfg!(windows) {
            assert!(shown.chars().nth(1) == Some(':'), "a Windows path with its drive: {shown}");
        }
    }

    #[test]
    fn the_stick_is_recompiled_only_where_its_text_changed() {
        let out = crate::tree_root().join("build/query_test/stick_text");
        let _ = std::fs::remove_dir_all(&out);
        std::fs::create_dir_all(&out).unwrap();
        let cu = out.join("measuring_stick.cu");
        let new = out.join("measuring_stick.cu.new");
        let cubin = out.join("measuring_stick_nvcc.cubin");
        std::fs::write(&cu, "a").unwrap();
        std::fs::write(&new, "a").unwrap();
        std::fs::write(&cubin, "").unwrap();
        assert!(!changed(&cu, &new, &cubin));
        std::fs::write(&new, "b").unwrap();
        assert!(changed(&cu, &new, &cubin));
        std::fs::write(&new, "a").unwrap();
        std::fs::remove_file(&cubin).unwrap();
        assert!(changed(&cu, &new, &cubin), "no container means nvcc runs");
        let _ = std::fs::remove_dir_all(&out);
    }

    #[test]
    fn the_words_name_what_is_read() {
        let said = |words: &[&str]| What::read(&words.iter().map(|w| w.to_string()).collect::<Vec<_>>());
        assert_eq!(said(&[]).unwrap(), What::Vendor);
        assert_eq!(said(&["vendor"]).unwrap(), What::Vendor);
        assert_eq!(said(&["engine"]).unwrap(), What::Engine);
        assert_eq!(said(&["read", "a.sass", "b.tsv"]).unwrap(), What::Read(vec!["a.sass".into(), "b.tsv".into()]));
        assert!(said(&["engine", "more"]).is_err());
        assert!(said(&["ruler"]).is_err());
    }
}

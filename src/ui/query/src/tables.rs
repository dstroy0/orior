// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The vendor's tables: the instruction set and the latencies of every architecture its compiler
//! lists, pulled off its disassembler. For each architecture nvcc compiles a key kernel to a cubin,
//! the disassembler for Linux, fetched once from the vendor's archive at the compiler's version, is
//! run over the cubin in the test harness's Ubuntu image with every copy and heap block it makes
//! written out (tables_intercept.c, tables_capture.sh), and the funnel reads the two tables off that
//! capture (tables_funnel). An architecture whose two tables stand already is kept as it is.

use std::path::{Path, PathBuf};

use crate::commands::Context;
use crate::programs::Role;
use crate::run::Run;
use crate::stick::native;

/// The tables' folder: the key cubins, the vendor's disassembler for Linux, the captures and the tables.
pub const OUT: &str = "build/vendor/nvidia";

const TABLES: &str = "src/cu/transpiler/vendor_bin_layouts/nvidia/tables";

/// The harness's Ubuntu image, which has the C compiler the hook builds with.
const IMAGE: &str = "orior-test-system-ubuntu:latest";

/// The vendor's archive of its disassembler for Linux, by the compiler's version.
pub fn archive_name(version: &str) -> String {
    format!("cuda_nvdisasm-linux-x86_64-{version}-archive")
}

fn archive_url(version: &str) -> String {
    format!("https://developer.download.nvidia.com/compute/cuda/redist/cuda_nvdisasm/linux-x86_64/{}.tar.xz", archive_name(version))
}

/// The key kernel: three lines whose content the disassembler reads the architecture off, and nothing else.
const KEY: &str = "__global__ void kernel(float *out) { out[0] = 0; }\n";

/// The compiler's version, `V13.3.73`, read off `nvcc --version`.
pub fn version_of(said: &str) -> Option<String> {
    said.split_whitespace().find_map(|word| word.strip_prefix('V').filter(|rest| rest.chars().all(|c| c.is_ascii_digit() || c == '.')).map(str::to_string))
}

/// The architectures `nvcc --list-gpu-arch` lists, `compute_86` read as `sm_86`.
pub fn architectures_of(said: &str) -> Vec<String> {
    said.split_whitespace().filter_map(|word| word.strip_prefix("compute_")).map(|number| format!("sm_{number}")).collect()
}

/// The path of `path` under the tree as the container sees it, the tree mounted at /tree.
pub fn in_container(root: &Path, path: &Path) -> String {
    let under = path.strip_prefix(root).unwrap_or(path);
    format!("/tree/{}", under.to_string_lossy().replace('\\', "/"))
}

fn tables_of(out: &Path, arch: &str) -> (PathBuf, PathBuf) {
    (out.join("tables").join(format!("{arch}_instructions.txt")), out.join("tables").join(format!("{arch}_latencies.txt")))
}

/// The architectures the words name, `sm_86 sm_90`, or every one the compiler lists where none is named.
fn architectures(context: &mut Context, run: &Run, words: &[String]) -> Result<Vec<String>, String> {
    if !words.is_empty() {
        return Ok(words.to_vec());
    }
    let nvcc = context.program(Role::Nvcc)?;
    let listed = context.ran(run, &nvcc, &["--list-gpu-arch"])?;
    let found = architectures_of(&String::from_utf8_lossy(&listed.stdout));
    if found.is_empty() {
        return Err("nvcc lists no architecture".into());
    }
    Ok(found)
}

/// The disassembler for Linux under the tables' folder, fetched and unpacked where it is not there.
fn disassembler(context: &mut Context, run: &Run, out: &Path) -> Result<PathBuf, String> {
    let nvcc = context.program(Role::Nvcc)?;
    let said = context.ran(run, &nvcc, &["--version"])?;
    let version = version_of(&String::from_utf8_lossy(&said.stdout)).ok_or("nvcc --version names no version")?;
    let download = out.join("download");
    let program = download.join(archive_name(&version)).join("bin").join("nvdisasm");
    if program.is_file() {
        return Ok(program);
    }
    std::fs::create_dir_all(&download).map_err(|e| format!("{}: {e}", download.display()))?;
    let tar = download.join(format!("{}.tar.xz", archive_name(&version)));
    let curl = context.program(Role::Curl)?;
    context.say(&format!("  the vendor's disassembler for Linux at {version} fetched"));
    if context.said(run, &curl, &["-s", "-f", "-o", &native(&tar), &archive_url(&version)])? != 0 {
        return Err(format!("the vendor's archive could not be fetched: {}", archive_url(&version)));
    }
    let unpack = context.program(Role::Tar)?;
    if context.said(run, &unpack, &["xf", &native(&tar), "-C", &native(&download)])? != 0 {
        return Err(format!("{} could not be unpacked", tar.display()));
    }
    if !program.is_file() {
        return Err(format!("{} holds no nvdisasm", tar.display()));
    }
    Ok(program)
}

/// The key cubin of `arch` compiled where it is not there.
fn cubin(context: &mut Context, run: &Run, out: &Path, arch: &str) -> Result<PathBuf, String> {
    let cubins = out.join("cubins");
    std::fs::create_dir_all(&cubins).map_err(|e| format!("{}: {e}", cubins.display()))?;
    let key = cubins.join("key.cu");
    if !key.is_file() {
        std::fs::write(&key, KEY).map_err(|e| format!("{}: {e}", key.display()))?;
    }
    let cubin = cubins.join(format!("{arch}.cubin"));
    if cubin.is_file() {
        return Ok(cubin);
    }
    let nvcc = context.program(Role::Nvcc)?;
    let arch_flag = format!("-arch={arch}");
    if context.said(run, &nvcc, &[&arch_flag, "-cubin", "-o", &native(&cubin), &native(&key)])? != 0 {
        return Err(format!("nvcc did not compile the key for {arch}"));
    }
    Ok(cubin)
}

/// A line of the step's own said on the sink and kept for the run's console.
fn told(context: &mut Context, lines: &mut Vec<String>, text: &str) {
    context.say(text);
    lines.push(text.to_string());
}

/// The tables of every architecture wanted that lacks them pulled: 0 where each now stands, 1 where
/// the funnel found no tables in some capture. Every line said is kept in `lines`.
fn pulled(context: &mut Context, run: &Run, words: &[String], lines: &mut Vec<String>) -> Result<i32, String> {
    let out = context.at(OUT);
    let wanted = architectures(context, run, words)?;
    let mut missing: Vec<String> = Vec::new();
    for arch in &wanted {
        let (instructions, latencies) = tables_of(&out, arch);
        if instructions.is_file() && latencies.is_file() {
            told(context, lines, &format!("  {arch}: its tables stand"));
        } else {
            missing.push(arch.clone());
        }
    }
    if missing.is_empty() {
        told(context, lines, &format!("tables: every one of {} architectures stands under {}", wanted.len(), out.join("tables").display()));
        return Ok(0);
    }
    let disassembler = disassembler(context, run, &out)?;
    let mut cubins: Vec<PathBuf> = Vec::new();
    for arch in &missing {
        cubins.push(cubin(context, run, &out, arch)?);
    }
    let docker = context.program(Role::Docker)?;
    let root = context.root.to_path_buf();
    let mount = format!("{}:/tree", native(&root));
    let mut docker_words: Vec<String> = vec!["run".into(), "--rm".into(), "-v".into(), mount, IMAGE.into(), "sh".into()];
    docker_words.push(in_container(&root, &context.at(TABLES).join("tables_capture.sh")));
    docker_words.push(in_container(&root, &disassembler));
    docker_words.push(in_container(&root, &context.at(TABLES).join("tables_intercept.c")));
    docker_words.push(in_container(&root, &out.join("captures")));
    docker_words.extend(cubins.iter().map(|cubin| in_container(&root, cubin)));
    told(context, lines, &format!("  the disassembler run over {} key cubins in {IMAGE}", missing.len()));
    if context.said(run, &docker, &docker_words.iter().map(String::as_str).collect::<Vec<_>>())? != 0 {
        return Err(format!("the capture in {IMAGE} did not end clean"));
    }
    let funnel = context.tool("tables_funnel")?;
    let tables = out.join("tables");
    std::fs::create_dir_all(&tables).map_err(|e| format!("{}: {e}", tables.display()))?;
    let mut written = 0usize;
    for arch in &missing {
        let capture = out.join("captures").join(format!("{arch}.capture"));
        let (instructions, latencies) = tables_of(&out, arch);
        let code = context.said(run, &funnel, &[&native(&capture), &native(&instructions), &native(&latencies)])?;
        if code == 0 {
            written += 1;
            told(context, lines, &format!("  {arch}: its tables written"));
        } else {
            told(context, lines, &format!("  {arch}: the funnel found no tables in its capture, exit {code}"));
        }
    }
    told(context, lines, &format!("tables: {written} of {} architectures written under {}", missing.len(), tables.display()));
    Ok(if written == missing.len() { 0 } else { 1 })
}

pub fn run(context: &mut Context, words: &[String]) -> Result<i32, String> {
    let run = context.started(&format!("{OUT}/qry"))?;
    let mut lines: Vec<String> = Vec::new();
    let code = pulled(context, &run, words, &mut lines);
    // the step's own lines are the run's console, as a program's output is where one is teed
    let mut console = lines.join("\n");
    if let Err(said) = &code {
        console.push('\n');
        console.push_str(said);
    }
    run.writer().hand("tables.console", console.as_bytes(), crate::writer::Priority::Ordinary)?;
    context.ended(run)?;
    code
}

#[cfg(test)]
mod reading {
    use super::*;

    #[test]
    fn the_version_and_the_architectures_are_read_off_nvcc() {
        assert_eq!(version_of("Cuda compilation tools, release 13.3, V13.3.73\nBuild cuda_13.3.r13.3/compiler.38244171_0").as_deref(), Some("13.3.73"));
        assert_eq!(version_of("nothing"), None);
        assert_eq!(architectures_of("compute_75 compute_80 compute_120\n"), ["sm_75", "sm_80", "sm_120"]);
        assert_eq!(archive_name("13.3.73"), "cuda_nvdisasm-linux-x86_64-13.3.73-archive");
        assert_eq!(in_container(Path::new("D:\\tree"), Path::new("D:\\tree\\build\\vendor\\nvidia\\cubins\\sm_86.cubin")), "/tree/build/vendor/nvidia/cubins/sm_86.cubin");
    }
}

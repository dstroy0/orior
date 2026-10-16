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
    context.handed(run, "nvcc_architectures.txt", &nvcc, &["--list-gpu-arch"])?;
    let listed = run.writer().latest("nvcc_architectures.txt").ok_or("the run holds no nvcc_architectures.txt")?;
    let found = architectures_of(&String::from_utf8_lossy(&listed));
    if found.is_empty() {
        return Err("nvcc lists no architecture".into());
    }
    Ok(found)
}

/// The disassembler for Linux under the tables' folder, fetched and unpacked where it is not there.
fn disassembler(context: &mut Context, run: &Run, out: &Path) -> Result<PathBuf, String> {
    let nvcc = context.program(Role::Nvcc)?;
    context.handed(run, "nvcc_version.txt", &nvcc, &["--version"])?;
    let said = run.writer().latest("nvcc_version.txt").ok_or("the run holds no nvcc_version.txt")?;
    let version = version_of(&String::from_utf8_lossy(&said)).ok_or("nvcc --version names no version")?;
    let download = out.join("download");
    let program = download.join(archive_name(&version)).join("bin").join("nvdisasm");
    if program.is_file() {
        return Ok(program);
    }
    std::fs::create_dir_all(&download).map_err(|e| format!("{}: {e}", download.display()))?;
    let tar = download.join(format!("{}.tar.xz", archive_name(&version)));
    let curl = context.program(Role::Curl)?;
    context.say(&format!("  the vendor's disassembler for Linux at {version} fetched"));
    if context.teed(run, "curl", &curl, &["-s", "-f", "-o", &native(&tar), &archive_url(&version)])? != 0 {
        return Err(format!("the vendor's archive could not be fetched: {}", archive_url(&version)));
    }
    let unpack = context.program(Role::Tar)?;
    if context.teed(run, "tar", &unpack, &["xf", &native(&tar), "-C", &native(&download)])? != 0 {
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
    if context.teed(run, "nvcc", &nvcc, &[&arch_flag, "-cubin", "-o", &native(&cubin), &native(&key)])? != 0 {
        return Err(format!("nvcc did not compile the key for {arch}"));
    }
    Ok(cubin)
}

/// The tables of every architecture wanted that lacks them pulled: 0 where each now stands, 1 where
/// the funnel found no tables in some capture. Each program runs through the writer's tee, the
/// capture as `tables_capture.console` and the funnel as `tables_funnel.console`.
fn pulled(context: &mut Context, run: &Run, words: &[String]) -> Result<i32, String> {
    let out = context.at(OUT);
    let wanted = architectures(context, run, words)?;
    let mut missing: Vec<String> = Vec::new();
    for arch in &wanted {
        let (instructions, latencies) = tables_of(&out, arch);
        if instructions.is_file() && latencies.is_file() {
            context.say(&format!("  {arch}: its tables stand"));
        } else {
            missing.push(arch.clone());
        }
    }
    if missing.is_empty() {
        context.say(&format!("tables: every one of {} architectures stands under {}", wanted.len(), out.join("tables").display()));
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
    context.say(&format!("  the disassembler run over {} key cubins in {IMAGE}", missing.len()));
    if context.teed(run, "tables_capture", &docker, &docker_words.iter().map(String::as_str).collect::<Vec<_>>())? != 0 {
        return Err(format!("the capture in {IMAGE} did not end clean"));
    }
    let funnel = context.tool("tables_funnel")?;
    let tables = out.join("tables");
    std::fs::create_dir_all(&tables).map_err(|e| format!("{}: {e}", tables.display()))?;
    let mut written = 0usize;
    for arch in &missing {
        let capture = out.join("captures").join(format!("{arch}.capture"));
        let (instructions, latencies) = tables_of(&out, arch);
        let code = context.teed(run, "tables_funnel", &funnel, &[&native(&capture), &native(&instructions), &native(&latencies)])?;
        if code == 0 {
            written += 1;
            context.say(&format!("  {arch}: its tables written"));
        } else {
            context.say(&format!("  {arch}: the funnel found no tables in its capture, exit {code}"));
        }
    }
    context.say(&format!("tables: {written} of {} architectures written under {}", missing.len(), tables.display()));
    Ok(if written == missing.len() { 0 } else { 1 })
}

/// The two readings of a pulled table, `read <arch> [--bank <size>] <low> <high>...` and `fields <arch>`, each a
/// run of the `tables_read` tool over the architecture's instruction set; None where the words ask for a pull.
pub fn reading(words: &[String]) -> Option<Result<(String, Vec<String>), String>> {
    let (first, rest) = words.split_first()?;
    let mode = match first.as_str() {
        "read" => "class",
        "fields" => "spans",
        _ => return None,
    };
    let Some((arch, tail)) = rest.split_first() else {
        return Some(Err(format!("tables {first} takes the architecture, sm_86, first")));
    };
    if (mode == "spans") && !tail.is_empty() {
        return Some(Err("tables fields takes the architecture alone".into()));
    }
    if mode == "class" {
        let banked = tail.first().is_some_and(|word| word == "--bank");
        let encodings = tail.len().saturating_sub(if banked { 2 } else { 0 });
        if (banked && tail.len() < 2) || encodings < 2 || encodings % 2 != 0 {
            return Some(Err("tables read takes the architecture, --bank <size> where the bank's size is known, then each encoding's two words".into()));
        }
    }
    Some(Ok((arch.clone(), std::iter::once(mode.to_string()).chain(tail.iter().cloned()).collect())))
}

/// `read` and `fields` run the reader over the architecture's table through the writer's tee, as
/// `tables_read.console`.
fn read(context: &mut Context, run: &Run, arch: &str, words: &[String]) -> Result<i32, String> {
    let (instructions, _) = tables_of(&context.at(OUT), arch);
    if !instructions.is_file() {
        return Err(format!("no table for {arch}: run orior protocol tables {arch} first"));
    }
    let reader = context.tool("tables_read")?;
    let mut given: Vec<String> = vec![native(&instructions)];
    given.extend(words.iter().cloned());
    context.teed(run, "tables_read", &reader, &given.iter().map(String::as_str).collect::<Vec<_>>())
}

pub fn run(context: &mut Context, words: &[String]) -> Result<i32, String> {
    let asked = reading(words).transpose()?;
    let run = context.started(&format!("{OUT}/qry"))?;
    let code = match &asked {
        Some((arch, given)) => read(context, &run, arch, given),
        None => pulled(context, &run, words),
    };
    if let Err(said) = &code {
        context.say(said);
    }
    context.ended(run)?;
    code
}

#[cfg(test)]
mod reading {
    use super::*;

    #[test]
    fn the_readings_of_a_table_are_told_apart_from_a_pull() {
        let words = |text: &str| -> Vec<String> { text.split_whitespace().map(str::to_string).collect() };
        assert!(reading(&words("sm_86 sm_90")).is_none(), "architectures alone ask for a pull");
        assert!(reading(&[]).is_none());
        assert_eq!(reading(&words("fields sm_86")).unwrap().unwrap(), ("sm_86".to_string(), words("spans")));
        assert!(reading(&words("fields sm_86 extra")).unwrap().is_err());
        assert!(reading(&words("fields")).unwrap().is_err());
        assert_eq!(
            reading(&words("read sm_86 --bank 0x174 0000460000047ab9 000fc80000000a00")).unwrap().unwrap(),
            ("sm_86".to_string(), words("class --bank 0x174 0000460000047ab9 000fc80000000a00"))
        );
        assert!(reading(&words("read sm_86 0000460000047ab9")).unwrap().is_err(), "an encoding is two words");
        assert!(reading(&words("read sm_86 --bank")).unwrap().is_err());
    }

    #[test]
    fn the_version_and_the_architectures_are_read_off_nvcc() {
        assert_eq!(version_of("Cuda compilation tools, release 13.3, V13.3.73\nBuild cuda_13.3.r13.3/compiler.38244171_0").as_deref(), Some("13.3.73"));
        assert_eq!(version_of("nothing"), None);
        assert_eq!(architectures_of("compute_75 compute_80 compute_120\n"), ["sm_75", "sm_80", "sm_120"]);
        assert_eq!(archive_name("13.3.73"), "cuda_nvdisasm-linux-x86_64-13.3.73-archive");
        assert_eq!(in_container(Path::new("D:\\tree"), Path::new("D:\\tree\\build\\vendor\\nvidia\\cubins\\sm_86.cubin")), "/tree/build/vendor/nvidia/cubins/sm_86.cubin");
    }
}

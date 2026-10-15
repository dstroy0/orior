// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Every C program a loop runs, a row each: its sources, its includes, its language and flags and
//! how it links; built where the binary is missing or a source or a header of a folder it reads is
//! newer, objects compiled as many at once as the host has processors. The file definitions every
//! program links are one list here.

use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::time::SystemTime;

use crate::programs::{Programs, Role};

/// A pattern names the files of one folder by their extension, `folder/*.c`, or one file, from the
/// top of the tree.
pub type Pattern = &'static str;

/// The sources of the file definitions, which every program of a run links (`src/cu/types/file_defs`).
pub const FILE_DEFS: &[Pattern] = &[
    "src/cu/types/file_defs/qry/buffer/*.c",
    "src/cu/types/file_defs/krep_records/*.c",
    "src/cu/types/file_defs/ktext.c",
    "src/cu/types/file_defs/kqr/record/*.c",
    "src/cu/types/file_defs/ksc/ksc.c",
    "src/cu/types/file_defs/kdm/kdm.c",
    "src/cu/types/file_defs/krf/krf.c",
];

/// The folders the file definitions' headers stand in beyond their sources' own.
const FILE_DEFS_HEADERS: &[&str] = &["src/cu/types/file_defs", "src/cu/types/file_defs/qry"];

/// The interface every program that starts a probe links.
const INTERFACE: &[Pattern] = &[
    "src/cu/transpiler/lstar/interface/interface.c",
    "src/cu/transpiler/lstar/interface/platform/*.c",
    "src/cu/transpiler/lstar/interface/interface_names.c",
];

/// The code generator, compiled as C++, and the sources every reader of it skips.
const CODEGEN: &[Pattern] = &["src/cu/engine/rmc/*.cu", "src/cu/types/file_defs/readers/*.cu", "src/cu/types/file_defs/readers/ruleset_flat/*.cu"];

const CODEGEN_SKIPPED: &[&str] = &["asm_printer_", "codegen"];

const C11: &[&str] = &["-std=c11", "-O2", "-Wall", "-Wextra"];

const C11_PLAIN: &[&str] = &["-std=c11", "-O2", "-Wall"];

const CXX17: &[&str] = &["-std=c++17", "-O2", "-Wall", "-Wextra"];

const CXX17_PLAIN: &[&str] = &["-std=c++17", "-O2", "-Wall"];

/// One program of the tree.
pub struct Tool {
    pub name: &'static str,
    /// C sources, compiled with the C compiler and `c_flags`, in groups of patterns.
    pub c: &'static [&'static [Pattern]],
    /// Sources compiled as C++ with `-x c++` and `cxx_flags`: the code generator's `.cu` files.
    pub cxx: &'static [&'static [Pattern]],
    /// Sources compiled as C++ as their extension says, `.cpp`.
    pub cxx_tool: &'static [Pattern],
    /// 1 where the file definitions are linked as C sources.
    pub file_defs: bool,
    /// Base-name prefixes of sources skipped among the patterns.
    pub skip: &'static [&'static str],
    /// Include folders, from the top of the tree.
    pub includes: &'static [&'static str],
    pub c_flags: &'static [&'static str],
    pub cxx_flags: &'static [&'static str],
    /// Words given the linker after the objects; the linker is the C++ compiler where any C++ source is there.
    pub link: &'static [&'static str],
    /// Folders whose `.h` files make the binary stale when newer, beyond the sources' own folders.
    pub headers: &'static [&'static str],
}

pub const EVERY: &[Tool] = &[
    Tool {
        name: "bus_clock",
        c: &[&[
            "src/cu/transpiler/vendor_bin_layouts/nvidia/bus_clock.c",
            "src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/mmio_read.c",
            "src/cu/transpiler/lstar/protocol/query/query_ask.c",
        ]],
        cxx: &[],
        cxx_tool: &[],
        file_defs: true,
        skip: &[],
        includes: &[],
        c_flags: C11,
        cxx_flags: &[],
        link: &[],
        headers: &["src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding", "src/cu/transpiler/lstar/protocol/query"],
    },
    Tool {
        name: "qry_writer",
        c: &[&["src/cu/types/file_defs/qry/qry_writer.c", "src/cu/types/file_defs/qry/writer/*.c", "src/cu/types/file_defs/qry/buffer/*.c"]],
        cxx: &[],
        cxx_tool: &[],
        file_defs: false,
        skip: &[],
        includes: &[],
        c_flags: C11,
        cxx_flags: &[],
        link: &[],
        headers: &["src/cu/types/file_defs/qry"],
    },
    Tool {
        name: "cubin_run",
        c: &[&[
            "src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_run.c",
            "src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_run/*.c",
            "src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_safe/*.c",
            "src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_write.c",
            "src/cu/transpiler/vendor_bin_layouts/container_write.c",
            "src/cu/transpiler/vendor_bin_layouts/container_pattern.c",
            "src/cu/transpiler/vendor_bin_layouts/container_layout.c",
            "src/cu/transpiler/vendor_bin_layouts/nvidia/sass_assemble/*.c",
            "src/cu/transpiler/vendor_bin_layouts/nvidia/sass_machine/*.c",
        ]],
        cxx: &[],
        cxx_tool: &[],
        file_defs: true,
        skip: &[],
        includes: &[],
        c_flags: C11_PLAIN,
        cxx_flags: &[],
        link: &[],
        headers: &[],
    },
    Tool {
        name: "measuring_stick_query",
        c: &[&["src/cu/transpiler/vendor_bin_layouts/nvidia/measuring_stick_query.c", "src/cu/transpiler/vendor_bin_layouts/nvidia/measuring_stick_query/*.c"], INTERFACE],
        cxx: &[],
        cxx_tool: &[],
        file_defs: true,
        skip: &[],
        includes: &["src/cu/engine"],
        c_flags: &["-std=c11", "-O2", "-Wall", "-Wextra", "-pthread"],
        cxx_flags: &[],
        link: &["-pthread"],
        headers: &[],
    },
    Tool {
        name: "khw_write",
        c: &[
            &[
                "src/cu/transpiler/lstar/parser/khw_write.c",
                "src/cu/transpiler/lstar/parser/khw_write/*.c",
                "src/cu/transpiler/lstar/protocol/teacher/run_channel/*.c",
                "src/cu/transpiler/lstar/protocol/overseer/*.c",
                "src/cu/transpiler/lstar/protocol/query/answer_read.c",
                "src/cu/transpiler/lstar/protocol/gate/survivors.c",
                "src/cu/transpiler/lstar/protocol/order/scheduler.c",
                "src/cu/transpiler/vendor_bin_layouts/container_layout.c",
            ],
            INTERFACE,
        ],
        cxx: &[],
        cxx_tool: &[],
        file_defs: true,
        skip: &[],
        includes: &["src/cu/engine"],
        c_flags: C11,
        cxx_flags: &[],
        link: &[],
        headers: &["src/cu/transpiler/lstar/protocol/teacher", "src/cu/transpiler/lstar/protocol/query", "src/cu/transpiler/lstar/protocol/gate", "src/cu/transpiler/lstar/protocol/order"],
    },
    Tool {
        name: "khw_machine_write",
        c: &[&[
            "src/cu/transpiler/vendor_bin_layouts/nvidia/khw_machine_write.c",
            "src/cu/transpiler/vendor_bin_layouts/nvidia/khw_machine_write/*.c",
            "src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_write.c",
            "src/cu/transpiler/vendor_bin_layouts/container_write.c",
            "src/cu/transpiler/vendor_bin_layouts/container_pattern.c",
            "src/cu/transpiler/vendor_bin_layouts/container_layout.c",
            "src/cu/transpiler/vendor_bin_layouts/nvidia/sass_assemble/*.c",
            "src/cu/transpiler/vendor_bin_layouts/nvidia/sass_machine/*.c",
        ]],
        cxx: &[],
        cxx_tool: &[],
        file_defs: true,
        skip: &[],
        includes: &[],
        c_flags: C11_PLAIN,
        cxx_flags: &[],
        link: &[],
        headers: &[],
    },
    Tool {
        name: "sass_listing_seeds",
        c: &[&["src/cu/transpiler/vendor_bin_layouts/nvidia/sass_listing_seeds.c"]],
        cxx: &[],
        cxx_tool: &[],
        file_defs: false,
        skip: &[],
        includes: &[],
        c_flags: C11,
        cxx_flags: &[],
        link: &[],
        headers: &[],
    },
    Tool {
        name: "krs_write",
        c: &[&["src/cu/transpiler/lstar/parser/krs_write.c", "src/cu/transpiler/lstar/parser/krs_write/*.c"], INTERFACE],
        cxx: &[],
        cxx_tool: &[],
        file_defs: true,
        skip: &[],
        includes: &["src/cu/engine"],
        c_flags: C11,
        cxx_flags: &[],
        link: &[],
        headers: &[],
    },
    Tool {
        name: "gsm_write",
        c: &[&["src/cu/transpiler/lstar/parser/gsm_write.c"], INTERFACE],
        cxx: &[],
        cxx_tool: &[],
        file_defs: true,
        skip: &[],
        includes: &["src/cu/engine"],
        c_flags: C11,
        cxx_flags: &[],
        link: &[],
        headers: &[],
    },
    Tool {
        name: "klq_write",
        c: &[&["src/cu/types/file_defs/qry/buffer/*.c"]],
        cxx: &[CODEGEN, &["src/cu/transpiler/lstar/protocol/table/klq_write.cu"]],
        cxx_tool: &[],
        file_defs: false,
        skip: CODEGEN_SKIPPED,
        includes: &["src/cu/engine", "src/cu/engine/rmc", "src/cu/types/file_defs/readers", "src/cu/types/file_defs/qry"],
        c_flags: C11,
        cxx_flags: CXX17,
        link: &["-static"],
        headers: &["src/cu/types/file_defs/qry", "src/cu/transpiler/lstar/protocol/table"],
    },
    Tool {
        name: "klq_identity",
        c: &[
            &["src/cu/transpiler/lstar/protocol/teacher/run_channel/*.c", "src/cu/transpiler/lstar/protocol/overseer/*.c"],
            INTERFACE,
            &["src/cu/transpiler/vendor_bin_layouts/nvidia/sass_assemble/*.c", "src/cu/transpiler/vendor_bin_layouts/nvidia/sass_machine/*.c"],
        ],
        cxx: &[CODEGEN, &["src/cu/transpiler/lstar/protocol/table/klq_identity.cu", "src/cu/transpiler/lstar/protocol/table/identity/*.cu"]],
        cxx_tool: &[],
        file_defs: true,
        skip: CODEGEN_SKIPPED,
        includes: &["src/cu/engine", "src/cu/engine/rmc", "src/cu/types/file_defs/readers", "src/cu/types/file_defs/qry", "src/cu/transpiler/lstar/protocol/table"],
        c_flags: C11,
        cxx_flags: CXX17,
        link: &["-static"],
        headers: &["src/cu/transpiler/lstar/protocol", "src/cu/transpiler/lstar/protocol/teacher", "src/cu/transpiler/vendor_bin_layouts", "src/cu/transpiler/vendor_bin_layouts/nvidia"],
    },
    Tool {
        name: "klq_decoder",
        c: &[&["src/cu/types/file_defs/qry/buffer/*.c"]],
        cxx: &[&["src/cu/transpiler/lstar/protocol/table/klq_decoder.cu", "src/cu/transpiler/lstar/protocol/table/decoder/*.cu", "src/cu/types/file_defs/readers/query_trace.cu"]],
        cxx_tool: &[],
        file_defs: false,
        skip: &[],
        includes: &["src/cu/transpiler/lstar/protocol/table", "src/cu/types/file_defs/readers", "src/cu/types/file_defs/qry"],
        c_flags: C11,
        cxx_flags: CXX17,
        link: &["-static"],
        headers: &["src/cu/types/file_defs/qry"],
    },
    Tool {
        name: "measuring_stick_read",
        c: &[&["src/cu/transpiler/vendor_bin_layouts/nvidia/measuring_stick_read.c", "src/cu/transpiler/vendor_bin_layouts/nvidia/measuring_stick_read/*.c"]],
        cxx: &[],
        cxx_tool: &[],
        file_defs: true,
        skip: &[],
        includes: &[],
        c_flags: C11,
        cxx_flags: &[],
        link: &[],
        headers: &[],
    },
    Tool {
        name: "measuring_stick_engine",
        c: &[&[
            "src/cu/transpiler/vendor_bin_layouts/nvidia/sass_machine/*.c",
            "src/cu/transpiler/vendor_bin_layouts/nvidia/sass_assemble/*.c",
            "src/cu/engine/runtime/scriptura/*.c",
            "src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/interface_sass_probe_class.c",
            "src/cu/types/integers/exact_integer_add.c",
            "src/cu/types/integers/exact_integer_limbs.c",
            "src/cu/types/integers/exact_integer_multiply.c",
            "src/cu/types/integers/exact_integer_divide.c",
            "src/cu/types/integers/exact_integer_gcd.c",
            "src/cu/types/integers/exact_integer_decimal.c",
            "src/cu/types/integers/exact_integer_hash.c",
        ]],
        cxx: &[
            &["src/cu/engine/analysis/keymath/keymath.cu", "src/cu/engine/analysis/key_schedule/key_schedule.cu"],
            CODEGEN,
            &["src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/cu_target.cu", "src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/sass_target.cu"],
        ],
        cxx_tool: &["src/cu/transpiler/vendor_bin_layouts/nvidia/measuring_stick_engine.cpp"],
        file_defs: true,
        skip: CODEGEN_SKIPPED,
        includes: &[
            "src/cu/engine",
            "src/cu/engine/includes/codecs/crc",
            "src/cu/engine/analysis/cycle",
            "src/cu/engine/rmc",
            "src/cu/types/file_defs/readers",
            "src/cu/engine/analysis/keymath",
            "src/cu/engine/analysis/key_schedule",
            "src/cu/types/integers",
            "src/cu/engine/runtime/scriptura",
            "src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding",
            "utils/test/src/cu/engine/analysis/cycle",
            "src/cu/transpiler/lstar/interface",
        ],
        c_flags: C11,
        cxx_flags: CXX17_PLAIN,
        link: &["-static", "-lpthread"],
        headers: &["utils/test/src/cu/transpiler/lstar/interface", "src/cu/transpiler/lstar/interface"],
    },
    Tool {
        name: "qry_sweep",
        c: &[&["src/cu/types/file_defs/qry/qry_sweep.c", "src/cu/types/file_defs/qry/buffer/*.c"]],
        cxx: &[],
        cxx_tool: &[],
        file_defs: false,
        skip: &[],
        includes: &[],
        c_flags: C11,
        cxx_flags: &[],
        link: &[],
        headers: &["src/cu/types/file_defs/qry"],
    },
];

/// The files `patterns` name, in the order of the patterns and sorted within one.
pub fn sources(root: &Path, patterns: &[Pattern]) -> Vec<PathBuf> {
    let mut found = Vec::new();
    for pattern in patterns {
        match pattern.rsplit_once('/') {
            Some((folder, glob)) if glob.starts_with("*.") => {
                let extension = &glob[2..];
                let mut files: Vec<PathBuf> = std::fs::read_dir(root.join(folder))
                    .into_iter()
                    .flatten()
                    .flatten()
                    .map(|entry| entry.path())
                    .filter(|path| path.is_file() && path.extension().is_some_and(|e| e == extension))
                    .collect();
                files.sort();
                found.extend(files);
            }
            _ => {
                let file = root.join(pattern);
                if file.is_file() {
                    found.push(file);
                }
            }
        }
    }
    found
}

/// The tool named, or none.
pub fn tool_named(name: &str) -> Option<&'static Tool> {
    EVERY.iter().find(|tool| tool.name == name)
}

/// The binary's name on this host.
pub fn binary_name(name: &str) -> String {
    if cfg!(windows) { format!("{name}.exe") } else { name.to_string() }
}

fn modified(path: &Path) -> Option<SystemTime> {
    std::fs::metadata(path).and_then(|m| m.modified()).ok()
}

/// The newest `.h` in `folder`, one level deep.
fn newest_header(folder: &Path) -> Option<SystemTime> {
    std::fs::read_dir(folder).into_iter().flatten().flatten().map(|e| e.path()).filter(|p| p.extension().is_some_and(|e| e == "h")).filter_map(|p| modified(&p)).max()
}

/// A source's base name, where none of `skip` opens it.
fn kept(skip: &[&str], path: &Path) -> bool {
    !path.file_name().and_then(|n| n.to_str()).is_some_and(|n| skip.iter().any(|s| n.starts_with(s)))
}

/// Every source of `tool`: its C sources with the file definitions where it links them, then its C++ ones, then its tools'.
fn every_source(root: &Path, tool: &Tool) -> (Vec<PathBuf>, Vec<PathBuf>, Vec<PathBuf>) {
    let mut c: Vec<PathBuf> = tool.c.iter().flat_map(|group| sources(root, group)).collect();
    if tool.file_defs {
        c.extend(sources(root, FILE_DEFS));
    }
    let cxx: Vec<PathBuf> = tool.cxx.iter().flat_map(|group| sources(root, group)).filter(|s| kept(tool.skip, s)).collect();
    let tools = sources(root, tool.cxx_tool);
    (c, cxx, tools)
}

/// The folders whose `.h` files a tool's sources read: those the sources stand in, those the tool
/// names, the file definitions' where it links them, and its includes.
fn header_folders(root: &Path, tool: &Tool, sources: &[&PathBuf]) -> Vec<PathBuf> {
    let mut folders: Vec<PathBuf> = sources.iter().filter_map(|source| source.parent().map(Path::to_path_buf)).collect();
    folders.extend(tool.headers.iter().map(|folder| root.join(folder)));
    if tool.file_defs {
        folders.extend(FILE_DEFS_HEADERS.iter().map(|folder| root.join(folder)));
    }
    folders.extend(tool.includes.iter().map(|folder| root.join(folder)));
    folders.sort();
    folders.dedup();
    folders
}

/// The newest `.h` across `folders`.
fn newest_header_of(folders: &[PathBuf]) -> Option<SystemTime> {
    folders.iter().filter_map(|folder| newest_header(folder)).max()
}

/// 1 where an object stands: it is there, and neither its source nor the newest header its tool
/// reads is newer than it.
fn fresh(source: Option<SystemTime>, object: Option<SystemTime>, newest_header: Option<SystemTime>) -> bool {
    match object {
        None => false,
        Some(object) => source.is_some_and(|s| s <= object) && newest_header.is_none_or(|h| h <= object),
    }
}

/// 1 where `binary` is missing or older than a source of the tool, or a header of a folder its
/// sources stand in or of one it names.
pub fn stale(root: &Path, tool: &Tool, binary: &Path) -> bool {
    let Some(built) = modified(binary) else { return true };
    let (c, cxx, tools) = every_source(root, tool);
    let all: Vec<&PathBuf> = c.iter().chain(cxx.iter()).chain(tools.iter()).collect();
    if all.iter().any(|source| modified(source).is_some_and(|m| m > built)) {
        return true;
    }
    newest_header_of(&header_folders(root, tool, &all)).is_some_and(|m| m > built)
}

/// The tool `name` built into `build/engine` where stale, its path either way.
pub fn built(root: &Path, programs: &dyn Programs, name: &str) -> Result<PathBuf, String> {
    built_into(root, programs, name, &root.join("build").join("engine"))
}

/// One compile: the compiler, its words, and the source they compile.
struct Compile {
    compiler: PathBuf,
    words: Vec<String>,
    source: PathBuf,
}

/// The launch queue's two numbers as the interface keeps them (`interface.h`): the compilers run at most this many
/// at once, and this much passes between one start and the next. The crate starts the compilers itself, outside the
/// interface, and paces them by the same numbers.
pub const LAUNCHES_AT_ONCE: usize = 4;
pub const LAUNCH_SPACING: std::time::Duration = std::time::Duration::from_millis(50);

/// Every compile run, `LAUNCHES_AT_ONCE` at a time and `LAUNCH_SPACING` between starts; the first that fails is said.
fn compiled_at_once(compiles: &[Compile], programs: &dyn Programs) -> Result<(), String> {
    let mut started = 0usize;
    for batch in compiles.chunks(LAUNCHES_AT_ONCE) {
        let mut running = Vec::new();
        for compile in batch {
            if started != 0 {
                std::thread::sleep(LAUNCH_SPACING);
            }
            let child = Command::new(&compile.compiler).args(&compile.words).env("PATH", programs.run_path()).stdin(Stdio::null()).stdout(Stdio::piped()).stderr(Stdio::piped()).spawn().map_err(|e| format!("{}: {e}", compile.compiler.display()))?;
            started += 1;
            running.push((child, &compile.source));
        }
        for (child, source) in running {
            let output = child.wait_with_output().map_err(|e| e.to_string())?;
            if !output.status.success() {
                let name = source.file_name().map(|n| n.to_string_lossy().into_owned()).unwrap_or_default();
                return Err(format!("build failed: {name} did not compile\n{}", String::from_utf8_lossy(&output.stderr).trim_end()));
            }
        }
    }
    Ok(())
}

/// A path as the compilers and the tools are given it: forward slashes throughout, which is what a
/// source's `__FILE__` then holds, what the readers walk `..` through, and how every tool's words and
/// console show a path.
pub(crate) fn text(path: &Path) -> String {
    path.to_string_lossy().replace('\\', "/")
}

/// The object a source is compiled to under `objects`, named by the source's path from the top of
/// the tree with its separators made `_`, so that two sources of one base name in two folders
/// stand apart.
fn object_of(root: &Path, objects: &Path, source: &Path) -> PathBuf {
    let under = source.strip_prefix(root).unwrap_or(source).to_string_lossy().replace(['/', '\\'], "_");
    objects.join(format!("{under}.o"))
}

/// The tool `name` built into `out` where stale: every source an object under `out/objects/<name>`,
/// compiled where it is missing or older than its source or than the newest header the tool reads,
/// then every object linked. The compilers are asked for before anything is compiled, so a missing
/// one is said and nothing is half built.
pub fn built_into(root: &Path, programs: &dyn Programs, name: &str, out: &Path) -> Result<PathBuf, String> {
    let tool = tool_named(name).ok_or_else(|| format!("no tool {name}"))?;
    let binary = out.join(binary_name(name));
    if !stale(root, tool, &binary) {
        return Ok(binary);
    }
    let c = programs.program(Role::C)?;
    let (c_sources, cxx_sources, tool_sources) = every_source(root, tool);
    let cxx = if cxx_sources.is_empty() && tool_sources.is_empty() { None } else { Some(programs.program(Role::Cxx)?) };
    let objects_dir = out.join("objects").join(name);
    std::fs::create_dir_all(&objects_dir).map_err(|e| format!("{}: {e}", objects_dir.display()))?;
    let includes: Vec<String> = tool.includes.iter().flat_map(|folder| ["-I".to_string(), text(&root.join(folder))]).collect();
    let all: Vec<&PathBuf> = c_sources.iter().chain(cxx_sources.iter()).chain(tool_sources.iter()).collect();
    let newest = newest_header_of(&header_folders(root, tool, &all));
    let standing = |source: &Path, object: &Path| fresh(modified(source), modified(object), newest);
    let mut compiles = Vec::new();
    let mut objects = Vec::new();
    for source in &c_sources {
        let object = object_of(root, &objects_dir, source);
        if !standing(source, &object) {
            let mut words: Vec<String> = tool.c_flags.iter().map(|w| w.to_string()).collect();
            words.extend(includes.iter().cloned());
            words.extend(["-c".into(), text(source), "-o".into(), text(&object)]);
            compiles.push(Compile { compiler: c.clone(), words, source: source.clone() });
        }
        objects.push(object);
    }
    for (as_cxx, group) in [(true, &cxx_sources), (false, &tool_sources)] {
        for source in group {
            let object = object_of(root, &objects_dir, source);
            if !standing(source, &object) {
                let mut words: Vec<String> = tool.cxx_flags.iter().map(|w| w.to_string()).collect();
                words.extend(includes.iter().cloned());
                if as_cxx {
                    words.extend(["-x".into(), "c++".into()]);
                }
                words.extend(["-c".into(), text(source), "-o".into(), text(&object)]);
                compiles.push(Compile { compiler: cxx.clone().ok_or("the C++ compiler")?, words, source: source.clone() });
            }
            objects.push(object);
        }
    }
    compiled_at_once(&compiles, programs)?;
    let linker = cxx.unwrap_or(c);
    let mut words: Vec<String> = vec!["-o".into(), text(&binary)];
    words.extend(objects.iter().map(|o| text(o)));
    words.extend(tool.link.iter().map(|w| w.to_string()));
    let linked = Command::new(&linker).args(&words).env("PATH", programs.run_path()).stdin(Stdio::null()).output().map_err(|e| format!("{}: {e}", linker.display()))?;
    if !linked.status.success() {
        return Err(format!("build failed: {name} did not link\n{}", String::from_utf8_lossy(&linked.stderr).trim_end()));
    }
    Ok(binary)
}

#[cfg(test)]
mod building {
    use super::*;
    use crate::programs::OnPath;

    fn root() -> PathBuf {
        crate::tree_root()
    }

    /// The number the interface's header defines under `name`, read off its `#define` line.
    fn interface_number(name: &str) -> u64 {
        let header = std::fs::read_to_string(root().join("src/cu/transpiler/lstar/interface/interface.h")).unwrap();
        let line = header.lines().find(|line| line.starts_with("#define ") && line.split_whitespace().nth(1) == Some(name)).unwrap_or_else(|| panic!("{name} is not defined in interface.h"));
        line.split_whitespace().nth(2).unwrap().trim_end_matches('u').parse().unwrap()
    }

    #[test]
    fn the_compiles_pace_as_the_interface_s_queue_does() {
        assert_eq!(LAUNCHES_AT_ONCE as u64, interface_number("INTERFACE_LAUNCHES_AT_ONCE"));
        assert_eq!(LAUNCH_SPACING.as_millis() as u64, interface_number("INTERFACE_LAUNCH_SPACING_MILLISECONDS"));
    }

    #[test]
    fn every_pattern_of_every_tool_reaches_a_file_of_the_tree() {
        let root = root();
        for tool in EVERY {
            let groups = tool.c.iter().chain(tool.cxx.iter()).flat_map(|group| group.iter());
            for pattern in groups.chain(tool.cxx_tool.iter()) {
                assert!(!sources(&root, &[pattern]).is_empty(), "{}: {pattern} reaches nothing", tool.name);
            }
        }
    }

    #[test]
    fn paths_given_to_the_compilers_are_forward_slashed() {
        // a source's path becomes its __FILE__, which the readers walk `..` through: every word a
        // compiler is given has forward slashes
        let root = root();
        let source = root.join("src").join("cu").join("types").join("file_defs").join("ktext.c");
        assert!(!text(&source).contains('\\'), "{}", text(&source));
        assert!(!text(&object_of(&root, &root.join("build").join("objects"), &source)).contains('\\'));
    }

    #[test]
    fn no_two_sources_of_a_tool_share_an_object() {
        let root = root();
        for tool in EVERY {
            let (c, cxx, tools) = every_source(&root, tool);
            let mut objects: Vec<PathBuf> = c.iter().chain(cxx.iter()).chain(tools.iter()).map(|source| object_of(&root, Path::new("objects"), source)).collect();
            let all = objects.len();
            objects.sort();
            objects.dedup();
            assert_eq!(objects.len(), all, "{}: two sources name one object", tool.name);
        }
    }

    #[test]
    fn the_file_definitions_are_what_every_program_links() {
        let root = root();
        let found = sources(&root, FILE_DEFS);
        assert!(found.iter().any(|f| f.ends_with("ktext.c")));
        assert!(found.iter().any(|f| f.file_name().is_some_and(|n| n == "kdm.c")));
        assert!(found.len() > 10);
    }

    #[test]
    fn a_tool_is_stale_where_its_binary_is_missing_or_older_than_a_source() {
        let root = root();
        let out = root.join("build").join("query_test").join("stale");
        let _ = std::fs::remove_dir_all(&out);
        std::fs::create_dir_all(&out).unwrap();
        let binary = out.join(binary_name("qry_writer"));
        let tool = tool_named("qry_writer").unwrap();
        assert!(stale(&root, tool, &binary), "a missing binary is stale");
        std::fs::write(&binary, "").unwrap();
        assert!(!stale(&root, tool, &binary), "a binary newer than every source stands");
        let old = std::time::UNIX_EPOCH + std::time::Duration::from_secs(86_400);
        std::fs::File::options().write(true).open(&binary).unwrap().set_modified(old).unwrap();
        assert!(stale(&root, tool, &binary), "a binary older than its sources is stale");
        let _ = std::fs::remove_dir_all(&out);
    }

    #[test]
    fn an_object_older_than_a_header_its_source_reads_is_compiled_again() {
        let epoch = std::time::UNIX_EPOCH;
        let at = |seconds: u64| epoch + std::time::Duration::from_secs(seconds);
        assert!(fresh(Some(at(10)), Some(at(20)), Some(at(5))), "an object newer than its source and every header stands");
        assert!(!fresh(Some(at(10)), Some(at(20)), Some(at(30))), "a header newer than the object compiles it again");
        assert!(!fresh(Some(at(30)), Some(at(20)), None), "a source newer than the object compiles it again");
        assert!(!fresh(Some(at(10)), None, None), "no object is compiled");
        assert!(fresh(Some(at(10)), Some(at(20)), None), "no header newer stands");
    }

    #[test]
    fn a_missing_compiler_is_said_before_any_build() {
        struct NoC;
        impl Programs for NoC {
            fn program(&self, role: Role) -> Result<PathBuf, String> {
                Err(format!("{} is not found: File, Toolchains opens its install page", role.name()))
            }
            fn run_path(&self) -> std::ffi::OsString {
                std::ffi::OsString::new()
            }
        }
        let root = root();
        let out = root.join("build").join("query_test").join("noc");
        let _ = std::fs::remove_dir_all(&out);
        let said = built_into(&root, &NoC, "sass_listing_seeds", &out).unwrap_err();
        assert!(said.starts_with("the C compiler is not found"), "{said}");
        assert!(!out.join(binary_name("sass_listing_seeds")).exists());
    }

    #[test]
    fn the_writer_builds_and_runs() {
        if OnPath.program(Role::C).is_err() {
            return;
        }
        let root = root();
        let out = root.join("build").join("query_test").join("tools");
        let writer = built_into(&root, &OnPath, "qry_writer", &out).unwrap();
        let status = std::process::Command::new(&writer).status().unwrap();
        assert_eq!(status.code(), Some(2), "qry_writer with no words says its usage and exits 2");
        assert!(!stale(&root, tool_named("qry_writer").unwrap(), &writer), "built, it stands");
    }
}

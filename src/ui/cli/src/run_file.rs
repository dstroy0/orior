// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Running one file with the toolchain its language has: Python with python, R with Rscript, MATLAB
//! with matlab -batch, Octave with octave-cli, Lean with lean --run, TeX with latexmk, a netlist with
//! ngspice or LTspice in batch, VHDL with ghdl, and C, C++, CUDA and Rust compiled to build/run/ and
//! then run. Each toolchain's `runs` in toolchains.json gives the line, by language, or by language
//! and extension as c.cu is, and of the toolchains whose `runs` name it the first found runs it.
//!
//! A file of a program a build makes runs as that program, built first by its own build, which builds
//! again only what changed: a C or C++ file of a CMake or Meson program orior configured with that
//! build's tool (compile_db.rs), and a Rust file of a Cargo package with cargo run, for the binary the
//! file is part of.
//!
//! The line is for a POSIX shell, the one orior's terminal runs, and runs at the tree's top folder.
//! In it {file} is the file's path quoted, {path} the same unquoted, {dir} its folder quoted, {stem}
//! its name without its extension, {out} build/run/<stem> quoted, {program} the toolchain's program
//! found, and any other {name} the program of that name in the same folder.

use std::path::{Path, PathBuf};

use crate::toolchains;

#[derive(serde::Serialize, Debug)]
pub struct RunLine {
    pub line: String,
    pub tool: String,
}

/// A path as a POSIX shell takes it: with forward slashes, in single quotes.
fn quoted(path: &Path) -> String {
    let text = path.display().to_string().replace('\\', "/");
    format!("'{}'", text.replace('\'', "'\\''"))
}

/// The line that builds and runs the program a build makes of the file at `path`, under the tree at
/// `root`, in `language`, where one does.
fn built_line(root: &Path, path: &Path, language: &str) -> Result<Option<RunLine>, String> {
    let run = |line: String, tool: &str| Ok(Some(RunLine { line: format!("( cd {} && {line} )", quoted(root)), tool: tool.to_string() }));
    if language == "c" {
        if let Some(made) = crate::compile_db::program_of(root, path) {
            let tool = crate::executables::program(&made.tool, &made.tool)?;
            let build = if made.tool == "meson" {
                format!("{} compile -C {} {}", quoted(&tool), quoted(&made.build), quoted(Path::new(&made.target)))
            } else {
                format!("{} --build {} --target {} --config {}", quoted(&tool), quoted(&made.build), quoted(Path::new(&made.target)), quoted(Path::new(&made.config)))
            };
            return run(format!("{build} && {}", quoted(&made.program)), if made.tool == "meson" { "Meson" } else { "CMake" });
        }
    }
    if language == "rust" {
        if let Some((manifest, bin)) = crate::cargo_tests::run_target_of(root, &crate::root::relative(root, path)) {
            let cargo = crate::executables::program("rust", "cargo")?;
            let mut line = format!("{} run --manifest-path {}", quoted(&cargo), quoted(&root.join(manifest)));
            if let Some(bin) = bin {
                line.push_str(&format!(" --bin {}", quoted(Path::new(&bin))));
            }
            return run(line, "Cargo");
        }
    }
    Ok(None)
}

/// The line that runs the file at `path`, under the tree at `root`, in `language`.
pub fn line_for(root: &Path, path: &Path, language: &str) -> Result<RunLine, String> {
    if let Some(line) = built_line(root, path, language)? {
        return Ok(line);
    }
    let ext = path.extension().map(|ext| ext.to_string_lossy().to_lowercase()).unwrap_or_default();
    let keys = [format!("{language}.{ext}"), language.to_string()];
    let tools = toolchains::manifest();
    let named: Vec<(&toolchains::Tool, &String)> = keys.iter().flat_map(|key| tools.iter().filter_map(move |tool| tool.runs.get(key).map(|line| (tool, line)))).collect();
    if named.is_empty() {
        return Err(format!("orior has no way to run a {language} file"));
    }
    let path_folders = toolchains::path_folders();
    let kept = toolchains::chosen();
    let found = named.iter().find_map(|(tool, line)| {
        let found = toolchains::find(tool, &path_folders, &kept);
        found.program.clone().map(|program| (*tool, *line, PathBuf::from(program)))
    });
    let Some((tool, template, program)) = found else {
        let names: Vec<&str> = named.iter().map(|(tool, _)| tool.name.as_str()).collect();
        return Err(format!("{} runs it, and is not found: File, Toolchains opens its install page or takes its folder", names.join(" or ")));
    };
    let folder = program.parent().map(Path::to_path_buf).unwrap_or_default();
    let stem = path.file_stem().map(|stem| stem.to_string_lossy().to_string()).unwrap_or_default();
    let mut out_path = root.join("build").join("run").join(&stem);
    if cfg!(windows) {
        out_path.set_extension("exe");
    }
    let mut line = String::new();
    let mut rest = template.as_str();
    while let Some(open) = rest.find('{') {
        line.push_str(&rest[..open]);
        let close = rest[open..].find('}').map(|at| open + at).ok_or_else(|| format!("{}'s line for {language} has an open {{", tool.name))?;
        let name = &rest[open + 1..close];
        let filled = match name {
            "file" => quoted(path),
            "path" => path.display().to_string().replace('\\', "/"),
            "dir" => quoted(path.parent().unwrap_or(root)),
            "stem" => stem.clone(),
            "out" => {
                std::fs::create_dir_all(root.join("build").join("run")).map_err(|error| format!("build/run: {error}"))?;
                quoted(&out_path)
            }
            "program" => quoted(&program),
            other => quoted(&toolchains::program_in(&folder, &[other.to_string()]).ok_or_else(|| format!("{} has no {other} in {}", tool.name, folder.display()))?),
        };
        line.push_str(&filled);
        rest = &rest[close + 1..];
    }
    line.push_str(rest);
    Ok(RunLine { line: format!("( cd {} && {line} )", quoted(root)), tool: tool.name.clone() })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_path_is_quoted_for_the_shell() {
        assert_eq!(quoted(Path::new("a b/it's.py")), "'a b/it'\\''s.py'");
    }

    /// What a run line prints, run by a POSIX shell as orior's terminal runs it.
    fn ran(line: &str) -> String {
        let out = std::process::Command::new("sh").arg("-c").arg(line).output().unwrap();
        format!("{}{}", String::from_utf8_lossy(&out.stdout), String::from_utf8_lossy(&out.stderr))
    }

    #[test]
    #[ignore = "configures and builds with CMake where it is installed"]
    fn a_file_of_a_cmake_program_runs_as_the_program_its_build_makes_built_again_after_an_edit() {
        let dir = std::env::temp_dir().join(format!("orior-run-cmake-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        std::fs::create_dir_all(dir.join("src")).unwrap();
        std::fs::create_dir_all(dir.join("include")).unwrap();
        std::fs::write(dir.join("CMakeLists.txt"), "cmake_minimum_required(VERSION 3.16)\nproject(probe C)\nadd_executable(probe src/main.c src/helper.c)\ntarget_include_directories(probe PRIVATE include)\ntarget_compile_definitions(probe PRIVATE NEEDED=1)\n").unwrap();
        std::fs::write(dir.join("include").join("thing.h"), "static int thing(void) { return NEEDED; }\n").unwrap();
        std::fs::write(dir.join("src").join("helper.c"), "int helper(void) { return 3; }\n").unwrap();
        std::fs::write(dir.join("src").join("main.c"), "#include <stdio.h>\n#include \"thing.h\"\nint helper(void);\nint main(void)\n{\n    printf(\"probe %d\\n\", thing() + helper());\n    return 0;\n}\n").unwrap();
        // As orior's CMake job configures it: its file API asked, into build/cmake/Debug.
        let build = dir.join("build").join("cmake").join("Debug");
        let query = build.join(".cmake").join("api").join("v1").join("query");
        std::fs::create_dir_all(&query).unwrap();
        for kind in ["codemodel-v2", "toolchains-v1"] {
            std::fs::write(query.join(kind), "").unwrap();
        }
        let configured = std::process::Command::new("cmake").arg("-S").arg(&dir).arg("-B").arg(&build).arg("-DCMAKE_BUILD_TYPE=Debug").output().unwrap();
        assert!(configured.status.success(), "{}", String::from_utf8_lossy(&configured.stderr));
        let line = line_for(&dir, &dir.join("src").join("main.c"), "c").unwrap();
        assert_eq!(line.tool, "CMake");
        assert!(ran(&line.line).contains("probe 4"), "the build's include, define and other source: {}", line.line);
        std::fs::write(dir.join("src").join("helper.c"), "int helper(void) { return 30; }\n").unwrap();
        assert!(ran(&line_for(&dir, &dir.join("src").join("main.c"), "c").unwrap().line).contains("probe 31"), "the edit built and run");
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    #[ignore = "builds with Cargo where it is installed"]
    fn a_file_of_a_cargo_package_runs_as_the_binary_it_is_part_of() {
        let dir = std::env::temp_dir().join(format!("orior-run-cargo-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        std::fs::create_dir_all(dir.join("src")).unwrap();
        std::fs::write(dir.join("Cargo.toml"), "[package]\nname = \"probe\"\nversion = \"0.1.0\"\nedition = \"2021\"\n\n[workspace]\n").unwrap();
        std::fs::write(dir.join("src").join("main.rs"), "mod front;\nfn main() {\n    println!(\"probe {}\", front::value());\n}\n").unwrap();
        std::fs::write(dir.join("src").join("front.rs"), "pub fn value() -> i32 {\n    4\n}\n").unwrap();
        let line = line_for(&dir, &dir.join("src").join("front.rs"), "rust").unwrap();
        assert_eq!(line.tool, "Cargo");
        assert!(ran(&line.line).contains("probe 4"), "the package's binary, not the module alone: {}", line.line);
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn every_run_line_closes_its_names_and_a_language_without_one_is_refused() {
        for tool in toolchains::manifest() {
            for (key, line) in &tool.runs {
                assert_eq!(line.matches('{').count(), line.matches('}').count(), "{} {key}", tool.id);
            }
        }
        assert!(line_for(Path::new("."), Path::new("a.json"), "json").unwrap_err().contains("no way to run"));
    }
}

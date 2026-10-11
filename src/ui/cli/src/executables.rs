// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The programs a tree's build scripts make, each built into a file that runs on its own: the
//! binaries of a Cargo package, the executables a CMake or a Meson project adds, the main packages
//! of a Go module, the .NET projects whose output type is Exe, the executables a build.zig adds,
//! the program a PyInstaller spec freezes, and the bin of a package.json, built by Bun or Deno.
//! Each is a job of the executable group. Its run builds it with the build's own tool, the build's
//! lines are its output, and its end names the file the build made.
//!
//! Each file goes where its build puts it by its own defaults: Cargo's target folder, a build.zig's
//! zig-out, .NET's bin, a spec's dist. CMake and Meson build into build/cmake/ and build/meson/,
//! a folder for each configuration, and Go, Bun and Deno into build/exe/.
//!
//! The build scripts are read from the tree's folders to DEEP deep, past the folders SKIPPED names
//! and those whose names start with a dot. A job's id is `executable/<script>:<name>`, the script
//! by its path in the tree, and `made` reads the file back from the id and the build's lines.

use std::collections::{HashMap, HashSet};
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

use serde_json::Value;

use crate::catalog::{Arg, Job, Param, Program, Step};
use crate::root::relative;
use crate::toolchains;

/// How deep in the tree's folders a build script is looked for.
const DEEP: usize = 5;

/// Folders a build writes into or a package manager fills, which hold no build scripts of the tree's.
const SKIPPED: [&str; 8] = ["target", "node_modules", "build", "out", "dist", "vendor", "bin", "obj"];

/// The kinds of build script read here.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
enum Kind {
    Cargo,
    Cmake,
    Meson,
    Go,
    Dotnet,
    Zig,
    Pyinstaller,
    Npm,
}

fn kind_of(name: &str) -> Option<Kind> {
    match name {
        "Cargo.toml" => Some(Kind::Cargo),
        "CMakeLists.txt" => Some(Kind::Cmake),
        "meson.build" => Some(Kind::Meson),
        "go.mod" => Some(Kind::Go),
        "build.zig" => Some(Kind::Zig),
        "package.json" => Some(Kind::Npm),
        _ if name.ends_with(".spec") => Some(Kind::Pyinstaller),
        _ if [".csproj", ".fsproj", ".vbproj"].iter().any(|ext| name.ends_with(ext)) => Some(Kind::Dotnet),
        _ => None,
    }
}

/// What a kind of build needs beyond the program's name.
#[derive(Clone, Debug, PartialEq)]
enum Detail {
    None,
    /// A Go package's folder, as `go build` takes it.
    Package(String),
    /// Whether the build.zig takes the optimize option.
    Optimize(bool),
    /// The file a spec freezes, by its path under the spec's folder.
    Frozen(String),
    /// A bin's script, by its path in the tree; the programs that can build it, the one taken unless
    /// told otherwise first; and whether the script is CommonJS, which Deno has to be told and Node
    /// takes unless told it is a module.
    Bin { entry: String, with: Vec<&'static str>, commonjs: bool },
}

/// One program a build script makes.
#[derive(Clone, Debug, PartialEq)]
struct Executable {
    kind: Kind,
    name: String,
    script: PathBuf,
    detail: Detail,
}

/// The tree's build scripts, sorted.
fn scripts(root: &Path) -> Vec<PathBuf> {
    let mut found = Vec::new();
    let mut folders = vec![(root.to_path_buf(), 0)];
    while let Some((folder, depth)) = folders.pop() {
        let Ok(entries) = fs::read_dir(&folder) else { continue };
        for entry in entries.flatten() {
            let name = entry.file_name().to_string_lossy().to_string();
            let Ok(kind) = entry.file_type() else { continue };
            if kind.is_dir() {
                if depth + 1 < DEEP && !name.starts_with('.') && !SKIPPED.contains(&name.as_str()) {
                    folders.push((entry.path(), depth + 1));
                }
            } else if kind_of(&name).is_some() {
                found.push(entry.path());
            }
        }
    }
    found.sort();
    found
}

/// A TOML value as text: a string's contents in either quotes, or the bare word before a comment.
fn toml_value(value: &str) -> String {
    let value = value.trim();
    for quote in ['"', '\''] {
        if let Some(rest) = value.strip_prefix(quote) {
            return rest.split(quote).next().unwrap_or_default().to_string();
        }
    }
    value.split('#').next().unwrap_or_default().trim().to_string()
}

/// The binaries of the package a Cargo.toml describes: each [[bin]] by its name, and where autobins
/// is not false, src/main.rs under the package's name and each src/bin/<name>.rs or
/// src/bin/<name>/main.rs under its own, as Cargo finds them. A manifest with no [package] makes
/// none.
fn cargo_bins(manifest: &Path, text: &str) -> Vec<String> {
    let mut table = String::new();
    let mut package = None;
    let mut autobins = true;
    let mut bins: Vec<(Option<String>, Option<String>)> = Vec::new();
    for line in text.lines().map(str::trim) {
        if let Some(inner) = line.strip_prefix("[[").and_then(|rest| rest.split("]]").next()) {
            table = inner.trim().to_string();
            if table == "bin" {
                bins.push((None, None));
            }
            continue;
        }
        if let Some(inner) = line.strip_prefix('[').and_then(|rest| rest.split(']').next()) {
            table = inner.trim().to_string();
            continue;
        }
        let Some((key, value)) = line.split_once('=') else { continue };
        let value = toml_value(value);
        match (table.as_str(), key.trim()) {
            ("package", "name") => package = Some(value),
            ("package", "autobins") => autobins = value != "false",
            ("bin", "name") => bins.last_mut().into_iter().for_each(|bin| bin.0 = Some(value.clone())),
            ("bin", "path") => bins.last_mut().into_iter().for_each(|bin| bin.1 = Some(value.clone())),
            _ => {}
        }
    }
    let Some(package) = package else { return Vec::new() };
    let folder = manifest.parent().unwrap_or(Path::new("."));
    let mut names = Vec::new();
    let mut taken = HashSet::new();
    for (name, path) in bins {
        let Some(name) = name else { continue };
        let path = path.unwrap_or_else(|| if name == package { "src/main.rs".into() } else { format!("src/bin/{name}.rs") });
        taken.insert(path.replace('\\', "/").trim_start_matches("./").to_string());
        names.push(name);
    }
    if autobins {
        let mut found = Vec::new();
        if folder.join("src/main.rs").is_file() {
            found.push((package.clone(), "src/main.rs".to_string()));
        }
        let mut more: Vec<(String, String)> = fs::read_dir(folder.join("src/bin"))
            .into_iter()
            .flatten()
            .flatten()
            .filter_map(|entry| {
                let name = entry.file_name().to_string_lossy().to_string();
                let path = entry.path();
                if path.is_file() {
                    name.strip_suffix(".rs").map(|stem| (stem.to_string(), format!("src/bin/{name}")))
                } else if path.join("main.rs").is_file() {
                    Some((name.clone(), format!("src/bin/{name}/main.rs")))
                } else {
                    None
                }
            })
            .collect();
        more.sort();
        found.extend(more);
        for (name, path) in found {
            if !taken.contains(&path) && !names.contains(&name) {
                names.push(name);
            }
        }
    }
    names
}

/// A script's text with its comments taken out: from a # outside a string in `quote` to the line's
/// end.
fn uncommented(text: &str, quote: char) -> String {
    let mut out = String::with_capacity(text.len());
    for line in text.lines() {
        let mut quoted = false;
        for char in line.chars() {
            if char == quote {
                quoted = !quoted;
            }
            if char == '#' && !quoted {
                break;
            }
            out.push(char);
        }
        out.push('\n');
    }
    out
}

/// The text inside the parentheses that open at byte `open` of `text`, to the one that closes them.
fn inside(text: &str, open: usize) -> &str {
    let mut depth = 0;
    for (at, char) in text[open..].char_indices() {
        match char {
            '(' => depth += 1,
            ')' => {
                depth -= 1;
                if depth == 0 {
                    return &text[open + 1..open + at];
                }
            }
            _ => {}
        }
    }
    &text[(open + 1).min(text.len())..]
}

/// The text inside each call of `command` in a script's code, the command's name matched case aside
/// and as a whole word, a method's name after its object's dot among them.
fn calls<'a>(code: &'a str, command: &str) -> Vec<&'a str> {
    let lower = code.to_lowercase();
    let command = command.to_lowercase();
    let mut found = Vec::new();
    let mut from = 0;
    while let Some(at) = lower[from..].find(&command).map(|at| from + at) {
        from = at + command.len();
        let starts_word = lower[..at].chars().next_back().is_none_or(|char| !(char.is_alphanumeric() || char == '_'));
        let rest = &lower[from..];
        let gap = rest.len() - rest.trim_start().len();
        if !starts_word || !rest.trim_start().starts_with('(') {
            continue;
        }
        let call = inside(code, from + gap);
        from += gap + call.len();
        found.push(call);
    }
    found
}

/// The words of a CMake call, their quotes taken off.
fn cmake_words(call: &str) -> Vec<String> {
    call.split_whitespace().map(|word| word.trim_matches('"').to_string()).collect()
}

/// The names the add_executable calls of CMake's code give, past IMPORTED and ALIAS targets, with
/// ${PROJECT_NAME} as `project` names it and no name that holds another variable.
fn cmake_targets(text: &str, project: &str) -> Vec<String> {
    let code = uncommented(text, '"');
    calls(&code, "add_executable")
        .into_iter()
        .map(cmake_words)
        .filter(|words| !words.iter().skip(1).take(1).any(|word| word == "IMPORTED" || word == "ALIAS"))
        .filter_map(|words| words.first().map(|name| name.replace("${PROJECT_NAME}", project)))
        .filter(|name| !name.is_empty() && !name.contains("${"))
        .collect()
}

/// The name a CMakeLists.txt's project() gives, where it calls it.
fn cmake_project(text: &str) -> Option<String> {
    calls(&uncommented(text, '"'), "project").into_iter().find_map(|call| cmake_words(call).first().cloned())
}

/// A Meson call's first argument, where it is a string written out.
fn meson_first(call: &str) -> Option<String> {
    let first = call.split(',').next()?.trim();
    let name = first.strip_prefix('\'')?.strip_suffix('\'')?;
    (!name.is_empty()).then(|| name.to_string())
}

/// The names the executable() calls of a meson.build give, where each is a string written out.
fn meson_targets(text: &str) -> Vec<String> {
    calls(&uncommented(text, '\''), "executable").into_iter().filter_map(meson_first).collect()
}

/// The name a meson.build's project() gives, where it calls it.
fn meson_project(text: &str) -> Option<String> {
    calls(&uncommented(text, '\''), "project").into_iter().find_map(meson_first)
}

/// The name a Go file's package clause gives, past its comments and build lines.
fn go_package(text: &str) -> Option<&str> {
    let mut in_comment = false;
    for line in text.lines().map(str::trim) {
        if in_comment {
            in_comment = !line.contains("*/");
            continue;
        }
        if line.is_empty() || line.starts_with("//") {
            continue;
        }
        if line.starts_with("/*") {
            in_comment = !line.contains("*/");
            continue;
        }
        return line.strip_prefix("package ").map(|rest| rest.split_whitespace().next().unwrap_or_default());
    }
    None
}

/// The main packages of the Go module whose go.mod is `module`: each folder under it, to DEEP deep
/// and in no module of its own, whose .go files other than tests say `package main`. Each is named
/// by its folder, and the module's own folder by the last part of the module's path.
fn go_mains(module: &Path, text: &str) -> Vec<(String, String)> {
    let path = text.lines().find_map(|line| line.trim().strip_prefix("module ")).map(toml_value).unwrap_or_default();
    let top = module.parent().unwrap_or(Path::new("."));
    let mut mains = Vec::new();
    let mut folders = vec![(top.to_path_buf(), 0)];
    while let Some((folder, depth)) = folders.pop() {
        let Ok(entries) = fs::read_dir(&folder) else { continue };
        let mut main = false;
        for entry in entries.flatten() {
            let name = entry.file_name().to_string_lossy().to_string();
            let entry_path = entry.path();
            if entry_path.is_dir() {
                let own_module = entry_path.join("go.mod").is_file();
                if depth + 1 < DEEP && !own_module && !name.starts_with(['.', '_']) && name != "testdata" && !SKIPPED.contains(&name.as_str()) {
                    folders.push((entry_path, depth + 1));
                }
            } else if name.ends_with(".go") && !name.ends_with("_test.go") && !main {
                main = fs::read_to_string(&entry_path).ok().is_some_and(|text| go_package(&text) == Some("main"));
            }
        }
        if main {
            let package = folder.strip_prefix(top).map(|inner| inner.to_string_lossy().replace('\\', "/")).unwrap_or_default();
            let name = if package.is_empty() { path.rsplit('/').next().unwrap_or_default().to_string() } else { package.rsplit('/').next().unwrap_or_default().to_string() };
            let package = if package.is_empty() { ".".to_string() } else { format!("./{package}") };
            if !name.is_empty() {
                mains.push((name, package));
            }
        }
    }
    mains.sort();
    mains
}

/// The text of the first element `tag` holds in an MSBuild project, trimmed.
fn msbuild_value<'a>(text: &'a str, tag: &str) -> Option<&'a str> {
    let open = format!("<{tag}>");
    let start = text.find(&open)? + open.len();
    let end = text[start..].find(&format!("</{tag}>"))? + start;
    Some(text[start..end].trim())
}

/// The name of the program a .NET project builds, where its OutputType is Exe or WinExe: its
/// AssemblyName where it sets one that names no property, else the project file's name.
fn dotnet_program(project: &Path, text: &str) -> Option<String> {
    let output = msbuild_value(text, "OutputType")?;
    if !output.eq_ignore_ascii_case("exe") && !output.eq_ignore_ascii_case("winexe") {
        return None;
    }
    let stem = project.file_stem()?.to_string_lossy().to_string();
    Some(msbuild_value(text, "AssemblyName").filter(|name| !name.contains("$(") && !name.is_empty()).map_or(stem, str::to_string))
}

/// The string a call's argument `key` is set to, as `key = "..."` or `key='...'`.
fn keyword(call: &str, key: &str) -> Option<String> {
    let mut from = 0;
    while let Some(at) = call[from..].find(key).map(|at| from + at) {
        from = at + key.len();
        let before = call[..at].chars().next_back();
        if before.is_some_and(|char| char.is_alphanumeric() || char == '_') {
            continue;
        }
        let rest = call[from..].trim_start();
        let Some(rest) = rest.strip_prefix('=') else { continue };
        let rest = rest.trim_start();
        let quote = rest.chars().next().filter(|char| *char == '"' || *char == '\'')?;
        return rest[1..].split(quote).next().map(str::to_string);
    }
    None
}

/// The names the addExecutable calls of a build.zig give: a name written first, as older builds
/// give it, or the `.name` the call's options set.
fn zig_exes(text: &str) -> Vec<String> {
    let code: String = text.lines().map(|line| line.split("//").next().unwrap_or_default()).collect::<Vec<_>>().join("\n");
    calls(&code, "addExecutable")
        .into_iter()
        .filter_map(|call| {
            let call = call.trim_start();
            if let Some(rest) = call.strip_prefix('"') {
                return rest.split('"').next().map(str::to_string);
            }
            keyword(call, ".name")
        })
        .filter(|name| !name.is_empty())
        .collect()
}

/// The file a PyInstaller spec freezes, by its path under the spec's folder: dist/<name> for one
/// file, and dist/<folder>/<name> where a COLLECT gathers a folder; None where the spec is no
/// PyInstaller's, or names its program other than by a string written out.
fn spec_made(text: &str) -> Option<(String, String)> {
    let code = uncommented(text, '\'');
    if calls(&code, "Analysis").is_empty() {
        return None;
    }
    let exe = calls(&code, "EXE").into_iter().find_map(|call| keyword(call, "name"))?;
    let exe = exe.strip_suffix(".exe").unwrap_or(&exe).to_string();
    let file = program_file(&exe);
    let path = match calls(&code, "COLLECT").into_iter().find_map(|call| keyword(call, "name")) {
        Some(folder) => format!("dist/{folder}/{file}"),
        None => format!("dist/{file}"),
    };
    Some((exe, path))
}

/// The bins of a package.json, each by its name and its script: a bin that is a path is named by
/// the package, its scope taken off.
fn npm_bins(text: &str) -> Vec<(String, String)> {
    let Ok(package) = serde_json::from_str::<Value>(text) else { return Vec::new() };
    let name = package["name"].as_str().unwrap_or_default();
    let name = name.rsplit('/').next().unwrap_or_default();
    match &package["bin"] {
        Value::String(entry) if !name.is_empty() => vec![(name.to_string(), entry.clone())],
        Value::Object(bins) => bins.iter().filter_map(|(name, entry)| entry.as_str().map(|entry| (name.clone(), entry.to_string()))).collect(),
        _ => Vec::new(),
    }
}

/// The programs that can build a bin, the one a job takes unless told otherwise first: Bun where the
/// package holds Bun's lock and Deno where it holds Deno's settings; else Node itself where the
/// script asks for no module outside Node, which a single executable of Node's cannot hold, and
/// else the bundlers, those the machine has before those it lacks.
fn npm_builders(folder: &Path, script: &str) -> Vec<&'static str> {
    if ["bun.lock", "bun.lockb"].iter().any(|file| folder.join(file).is_file()) {
        return vec!["bun", "deno", "node"];
    }
    if ["deno.json", "deno.jsonc"].iter().any(|file| folder.join(file).is_file()) {
        return vec!["deno", "bun", "node"];
    }
    if crate::sea::outside(script).is_empty() {
        return vec!["node", "bun", "deno"];
    }
    let mut bundlers = vec!["bun", "deno"];
    bundlers.sort_by_key(|tool| program(tool, tool).is_err());
    bundlers.push("node");
    bundlers
}

/// The scripts of `found` named `file` that call project() with none above them in the tree, each
/// with the project's name. A project's targets are those of every such file under its folder.
fn projects<'a>(found: &'a [PathBuf], file: &str, project: fn(&str) -> Option<String>) -> Vec<(&'a PathBuf, String, Vec<&'a PathBuf>)> {
    let lists: Vec<&PathBuf> = found.iter().filter(|path| path.file_name().is_some_and(|name| name == file)).collect();
    let mut tops = Vec::new();
    for script in &lists {
        let folder = script.parent().unwrap_or(Path::new("."));
        let above = lists.iter().any(|other| other.parent().is_some_and(|dir| folder.starts_with(dir) && dir != folder));
        let Some(name) = (!above).then(|| fs::read_to_string(script).ok().as_deref().and_then(project)).flatten() else { continue };
        let under: Vec<&PathBuf> = lists.iter().copied().filter(|other| other.starts_with(folder)).collect();
        tops.push((*script, name, under));
    }
    tops
}

/// Every program the tree's build scripts make, by script and then by name.
fn read(root: &Path) -> Vec<Executable> {
    let found = scripts(root);
    let mut all = Vec::new();
    let one = |kind: Kind, script: &Path, name: String, detail: Detail| Executable { kind, name, script: script.to_path_buf(), detail };
    for (top, project, under) in projects(&found, "CMakeLists.txt", cmake_project) {
        let mut names: Vec<String> = Vec::new();
        for list in under {
            let text = fs::read_to_string(list).unwrap_or_default();
            let own = cmake_project(&text).unwrap_or_else(|| project.clone());
            names.extend(cmake_targets(&text, &own).into_iter().filter(|name| !names.contains(name)).collect::<Vec<_>>());
        }
        all.extend(names.into_iter().map(|name| one(Kind::Cmake, top, name, Detail::None)));
    }
    for (top, _, under) in projects(&found, "meson.build", meson_project) {
        let mut names: Vec<String> = Vec::new();
        for list in under {
            names.extend(meson_targets(&fs::read_to_string(list).unwrap_or_default()).into_iter().filter(|name| !names.contains(name)).collect::<Vec<_>>());
        }
        all.extend(names.into_iter().map(|name| one(Kind::Meson, top, name, Detail::None)));
    }
    for script in &found {
        let name = script.file_name().map(|name| name.to_string_lossy().to_string()).unwrap_or_default();
        let Some(kind) = kind_of(&name) else { continue };
        if matches!(kind, Kind::Cmake | Kind::Meson) {
            continue;
        }
        let Ok(text) = fs::read_to_string(script) else { continue };
        let folder = script.parent().unwrap_or(root);
        match kind {
            Kind::Cargo => all.extend(cargo_bins(script, &text).into_iter().map(|name| one(kind, script, name, Detail::None))),
            Kind::Go => all.extend(go_mains(script, &text).into_iter().map(|(name, package)| one(kind, script, name, Detail::Package(package)))),
            Kind::Dotnet => all.extend(dotnet_program(script, &text).map(|name| one(kind, script, name, Detail::None))),
            Kind::Zig => {
                let optimize = text.contains("standardOptimizeOption");
                all.extend(zig_exes(&text).into_iter().map(|name| one(kind, script, name, Detail::Optimize(optimize))));
            }
            Kind::Pyinstaller => all.extend(spec_made(&text).map(|(name, path)| one(kind, script, name, Detail::Frozen(path)))),
            Kind::Npm => {
                let bins = npm_bins(&text);
                if bins.is_empty() {
                    continue;
                }
                let module = serde_json::from_str::<Value>(&text).ok().is_some_and(|package| package["type"] == "module");
                for (name, entry) in bins {
                    let commonjs = entry.ends_with(".cjs") || entry.ends_with(".js") && !module;
                    let path = folder.join(entry.trim_start_matches("./"));
                    let with = npm_builders(folder, &fs::read_to_string(&path).unwrap_or_default());
                    all.push(one(kind, script, name, Detail::Bin { entry: relative(root, &path), with, commonjs }));
                }
            }
            Kind::Cmake | Kind::Meson => {}
        }
    }
    all.sort_by(|one, other| (&one.script, &one.name).cmp(&(&other.script, &other.name)));
    all
}

fn choice(key: &str, choices: &[&str]) -> Param {
    Param { key: key.to_string(), kind: "choice", choices: choices.iter().map(|one| one.to_string()).collect(), default: choices[0].to_string(), required: true }
}

fn tool(tool: &str, program: &str, args: Vec<Arg>) -> Step {
    Step { program: Program::Tool { tool: tool.to_string(), program: program.to_string() }, args }
}

fn lit(text: impl Into<String>) -> Arg {
    Arg::Lit(text.into())
}

fn when(key: &str, value: &str, words: &[&str]) -> Arg {
    Arg::When(key.to_string(), value.to_string(), words.iter().map(|word| word.to_string()).collect())
}

/// The program's file name on this system.
fn program_file(name: &str) -> String {
    format!("{name}{}", std::env::consts::EXE_SUFFIX)
}

/// The script's folder in the tree, "." for the tree's own.
fn folder_of(root: &Path, script: &Path) -> String {
    let folder = relative(root, script.parent().unwrap_or(root));
    if folder.is_empty() { ".".to_string() } else { folder }
}

/// The folder a CMake or Meson project's build of each configuration goes in, before the
/// configuration's name: build/<tool>/ for the tree's own, else under the project's folder's path.
fn build_folder(root: &Path, script: &Path, tool: &str) -> String {
    match folder_of(root, script).as_str() {
        "." => format!("build/{tool}/"),
        folder => format!("build/{tool}/{folder}/"),
    }
}

/// The target choice that builds for the machine the build runs on.
const THIS_MACHINE: &str = "this machine";

/// The cgo choice that leaves it to Go, which turns cgo on where it finds a C compiler.
const AS_GO_CHOOSES: &str = "as Go chooses";

/// The targets a Rust binary can be built for, by Rust's own names: the musl ones link as a whole
/// file that asks for no glibc.
const RUST_TARGETS: [&str; 8] = [
    THIS_MACHINE,
    "x86_64-unknown-linux-musl",
    "aarch64-unknown-linux-musl",
    "x86_64-unknown-linux-gnu",
    "x86_64-pc-windows-msvc",
    "aarch64-pc-windows-msvc",
    "x86_64-apple-darwin",
    "aarch64-apple-darwin",
];

/// This machine's system and processor, by Go's names.
fn go_host() -> (&'static str, &'static str) {
    let system = match std::env::consts::OS {
        "macos" => "darwin",
        other => other,
    };
    let processor = match std::env::consts::ARCH {
        "x86_64" => "amd64",
        "aarch64" => "arm64",
        other => other,
    };
    (system, processor)
}

/// `choices` with `first` first: the default a choice takes is its first.
fn first_of(first: &'static str, choices: &[&'static str]) -> Vec<&'static str> {
    std::iter::once(first).chain(choices.iter().copied().filter(|one| *one != first)).collect()
}

/// Zig's name for a system and processor, as `zig cc -target` takes it, from Go's names or Rust's
/// target: Linux's musl, which links whole, Windows' GNU and macOS.
fn zig_target(system: &str, processor: &str) -> Option<String> {
    let processor = match processor {
        "amd64" | "x86_64" => "x86_64",
        "arm64" | "aarch64" => "aarch64",
        _ => return None,
    };
    let system = match system {
        "linux" => "linux-musl",
        "windows" => "windows-gnu",
        "darwin" | "macos" => "macos",
        _ => return None,
    };
    Some(format!("{processor}-{system}"))
}

/// The C compiler and linker a build for another machine needs, as the job's params choose it: for
/// Go with cgo on, `zig cc` for the system and processor chosen; for Rust, rust-lld to link a musl
/// target, where the environment names no linker of its own, and `zig cc` for the C a crate builds.
/// Nothing where the target is this machine's, or Zig is not found for the C.
pub fn cross_env(kind: &str, values: &HashMap<String, Vec<String>>) -> Vec<(String, String)> {
    let zig = program("zig", "zig").ok().map(|path| {
        let text = path.display().to_string();
        if text.contains(' ') { format!("\"{text}\"") } else { text }
    });
    let mut set = Vec::new();
    match kind {
        "go" => {
            let (host_system, host_processor) = go_host();
            let system = first(values, "GOOS", host_system);
            let processor = first(values, "GOARCH", host_processor);
            if first(values, "CGO_ENABLED", AS_GO_CHOOSES) == "1" && (system, processor) != (host_system, host_processor) {
                if let (Some(zig), Some(target)) = (&zig, zig_target(system, processor)) {
                    set.push(("CC".to_string(), format!("{zig} cc -target {target}")));
                    set.push(("CXX".to_string(), format!("{zig} c++ -target {target}")));
                }
            }
        }
        "rust" => {
            let target = first(values, "target", THIS_MACHINE);
            if target == THIS_MACHINE {
                return set;
            }
            let upper = target.to_uppercase().replace('-', "_");
            let linker = format!("CARGO_TARGET_{upper}_LINKER");
            if target.ends_with("-linux-musl") && std::env::consts::OS != "linux" && std::env::var_os(&linker).is_none() {
                set.push((linker, "rust-lld".to_string()));
            }
            let mut parts = target.split('-');
            let processor = parts.next().unwrap_or_default();
            let system = if target.contains("linux") { "linux" } else if target.contains("windows-gnu") { "windows" } else if target.contains("apple") { "darwin" } else { "" };
            if let (Some(zig), Some(zig_triple)) = (&zig, zig_target(system, processor)) {
                set.push((format!("CC_{}", target.replace('-', "_")), format!("{zig} cc -target {zig_triple}")));
            }
        }
        _ => {}
    }
    set
}

/// Whether Meson is to set up Visual Studio's environment itself: on Windows, where MSVC's cl is on
/// the PATH and the environment it compiles in is not set, Meson takes cl and it cannot compile.
/// Meson sets the environment up only where no cl is on the PATH, and its setup and compile run
/// without cl's folders there.
fn needs_vsenv() -> bool {
    cfg!(windows) && std::env::var_os("INCLUDE").is_none() && toolchains::path_folders().iter().any(|dir| toolchains::program_in(dir, &["cl".to_string()]).is_some())
}

/// .NET's name for this system and processor, as `-r` takes it.
fn dotnet_runtime() -> String {
    let system = match std::env::consts::OS {
        "windows" => "win",
        "macos" => "osx",
        other => other,
    };
    let processor = match std::env::consts::ARCH {
        "x86_64" => "x64",
        "aarch64" => "arm64",
        other => other,
    };
    format!("{system}-{processor}")
}

/// The job that builds `exe`.
fn job_of(root: &Path, exe: &Executable, title: String) -> Job {
    let script = relative(root, &exe.script);
    let folder = folder_of(root, &exe.script);
    let name = exe.name.as_str();
    let (about, params, steps) = match (&exe.kind, &exe.detail) {
        (Kind::Cargo, _) => (
            format!("cargo builds the {name} binary of {script}, by the profile chosen, for this machine or for the target chosen."),
            vec![choice("profile", &["dev", "release"]), choice("target", &RUST_TARGETS)],
            vec![tool(
                "rust",
                "cargo",
                vec![
                    Arg::Folder(folder.clone()),
                    lit("build"),
                    lit("--bin"),
                    lit(name),
                    Arg::Flag("--profile", "profile".into()),
                    Arg::FlagBut("--target", "target".into(), THIS_MACHINE.into()),
                    Arg::When("profile".into(), "release".into(), vec!["--config".into(), format!("build.rustflags=['--remap-path-prefix={}=.']", root.display())]),
                    Arg::Cross("rust"),
                ],
            )],
        ),
        (Kind::Cmake, _) => {
            let build = build_folder(root, &exe.script, "cmake");
            let at = |after: &str| Arg::Around(build.clone(), "config".into(), after.to_string());
            (
                format!("CMake configures {script} into {build}<config> and builds its {name} target."),
                vec![choice("config", &["Debug", "Release", "RelWithDebInfo", "MinSizeRel"])],
                vec![
                    tool("cmake", "cmake", vec![lit("-E"), lit("make_directory"), at("/.cmake/api/v1/query")]),
                    tool("cmake", "cmake", vec![lit("-E"), lit("touch"), at("/.cmake/api/v1/query/codemodel-v2")]),
                    tool("cmake", "cmake", vec![lit("-S"), lit(folder), lit("-B"), at(""), Arg::Around("-DCMAKE_BUILD_TYPE=".into(), "config".into(), String::new())]),
                    tool("cmake", "cmake", vec![lit("--build"), at(""), lit("--target"), lit(name), lit("--config"), Arg::Value("config".into())]),
                ],
            )
        }
        (Kind::Meson, _) => {
            let build = build_folder(root, &exe.script, "meson");
            let at = || Arg::Around(build.clone(), "buildtype".into(), String::new());
            let mut setup = vec![lit("setup"), lit("--reconfigure"), at(), lit(folder), Arg::Around("--buildtype=".into(), "buildtype".into(), String::new())];
            let mut compile = vec![lit("compile"), lit("-C"), at(), lit(name)];
            if needs_vsenv() {
                setup.extend([lit("--vsenv"), Arg::Unpath("cl".into())]);
                compile.push(Arg::Unpath("cl".into()));
            }
            (
                format!("Meson sets up {script} in {build}<buildtype> and compiles its {name} executable."),
                vec![choice("buildtype", &["debug", "debugoptimized", "release", "minsize"])],
                vec![
                    tool("meson", "meson", setup),
                    tool("meson", "meson", compile),
                ],
            )
        }
        (Kind::Go, detail) => {
            let package = match detail {
                Detail::Package(package) => package.clone(),
                _ => ".".to_string(),
            };
            let out = "build/exe/{GOOS}-{GOARCH}/".to_string();
            let mut args = vec![Arg::Env("GOOS".into()), Arg::Env("GOARCH".into()), Arg::EnvBut("CGO_ENABLED".into(), AS_GO_CHOOSES.into()), Arg::Cross("go"), lit("build")];
            let out = if folder == "." {
                out
            } else {
                args.extend([lit("-C"), lit(folder)]);
                format!("{}/{out}", root.display().to_string().replace('\\', "/"))
            };
            args.extend([lit("-trimpath"), lit("-o"), Arg::Format(out), lit(package.clone())]);
            let (system, processor) = go_host();
            let systems = first_of(system, &["windows", "linux", "darwin"]);
            let processors = first_of(processor, &["amd64", "arm64"]);
            (
                format!("go builds the {package} package of {script} into build/exe/<GOOS>-<GOARCH>/, for the system and processor chosen, with cgo as Go chooses or as chosen."),
                vec![choice("GOOS", &systems), choice("GOARCH", &processors), choice("CGO_ENABLED", &[AS_GO_CHOOSES, "0", "1"])],
                vec![tool("go", "go", args)],
            )
        }
        (Kind::Dotnet, _) => (
            format!("dotnet builds {script} by the configuration chosen, as its files or published as one file with its native libraries inside it."),
            vec![choice("configuration", &["Debug", "Release"]), choice("output", &["files", "one file"])],
            vec![tool(
                "dotnet",
                "dotnet",
                vec![
                    Arg::Set("DOTNET_CLI_TELEMETRY_OPTOUT".into(), "1".into()),
                    Arg::Set("DOTNET_NOLOGO".into(), "1".into()),
                    when("output", "files", &["build"]),
                    when("output", "one file", &["publish"]),
                    lit(script.clone()),
                    lit("-c"),
                    Arg::Value("configuration".into()),
                    when("output", "one file", &["-r", &dotnet_runtime(), "--self-contained", "true", "-p:PublishSingleFile=true", "-p:IncludeNativeLibrariesForSelfExtract=true"]),
                    when("configuration", "Release", &["-p:ContinuousIntegrationBuild=true"]),
                ],
            )],
        ),
        (Kind::Zig, detail) => {
            let optimize = matches!(detail, Detail::Optimize(true));
            let mut args = vec![lit("build"), lit("--build-file"), lit(script.clone())];
            let mut params = Vec::new();
            if optimize {
                args.push(Arg::Around("-Doptimize=".into(), "optimize".into(), String::new()));
                params.push(choice("optimize", &["Debug", "ReleaseSafe", "ReleaseFast", "ReleaseSmall"]));
            }
            (format!("zig builds {script} into its zig-out, {name} among the executables it installs."), params, vec![tool("zig", "zig", args)])
        }
        (Kind::Pyinstaller, detail) => {
            let made = match detail {
                Detail::Frozen(path) => path.clone(),
                _ => String::new(),
            };
            let under = |inner: &str| if folder == "." { inner.to_string() } else { format!("{folder}/{inner}") };
            (
                format!("PyInstaller, run by the tree's Python, freezes {script} into {}.", under(&made)),
                Vec::new(),
                vec![tool("python", "python", vec![lit("-m"), lit("PyInstaller"), lit("--noconfirm"), lit("--distpath"), lit(under("dist")), lit("--workpath"), lit(under("build")), lit(script.clone())])],
            )
        }
        (Kind::Npm, Detail::Bin { entry, with, commonjs }) => {
            let out = format!("build/exe/{name}");
            let only = |tool: &str| Arg::Only("with".into(), tool.to_string());
            let mut deno = vec![only("deno"), lit("compile"), lit("-A")];
            if *commonjs {
                deno.push(lit("--unstable-detect-cjs"));
            }
            deno.extend([lit("--output"), lit(out.clone()), lit(entry.clone())]);
            let mut node = vec![only("node"), lit("sea"), lit(entry.clone()), lit(out.clone())];
            if !*commonjs {
                node.push(lit("module"));
            }
            (
                format!("builds the {name} bin of {script} into {out}: with Node's own single executable, which holds Node's modules alone, with bun build --compile, or with deno compile, given every permission a Node program has."),
                vec![choice("with", with)],
                vec![
                    Step { program: Program::Orior, args: node },
                    tool("bun", "bun", vec![only("bun"), lit("build"), lit("--compile"), lit(entry.clone()), lit("--outfile"), lit(out.clone())]),
                    tool("deno", "deno", deno),
                ],
            )
        }
        (Kind::Npm, _) => (String::new(), Vec::new(), Vec::new()),
    };
    let (mut params, mut steps) = (params, steps);
    for step in &mut steps {
        step.args.push(Arg::SourceDate);
    }
    params.push(choice("then", &["build", "run", "run as the system runs it"]));
    params.push(Param { key: "folder".into(), kind: "dir", choices: Vec::new(), default: String::new(), required: false });
    steps.push(Step { program: Program::Made { system: false }, args: vec![Arg::Only("then".into(), "run".into())] });
    steps.push(Step { program: Program::Made { system: true }, args: vec![Arg::Only("then".into(), "run as the system runs it".into())] });
    Job { id: format!("executable/{script}:{name}"), group: "executable", title, file: script, about, params, opens: "program", steps }
}

/// One job for each program the tree's build scripts make. Where two scripts make programs of one
/// name, each is titled by its script's folder as well.
pub fn jobs(root: &Path, jobs: &mut Vec<Job>) {
    let all = read(root);
    let mut counts: HashMap<&str, usize> = HashMap::new();
    for exe in &all {
        *counts.entry(exe.name.as_str()).or_default() += 1;
    }
    for exe in &all {
        let title = if counts[exe.name.as_str()] > 1 {
            let folder = exe.script.parent().and_then(Path::file_name).map(|name| name.to_string_lossy().to_string()).unwrap_or_default();
            format!("{folder}/{}", exe.name)
        } else {
            exe.name.clone()
        };
        jobs.push(job_of(root, exe, title));
    }
}

/// The program `name` of the toolchain `tool`, found where the toolchains window finds the tool.
pub fn program(tool: &str, name: &str) -> Result<PathBuf, String> {
    let tools = toolchains::manifest();
    let one = tools.iter().find(|one| one.id == tool).ok_or_else(|| format!("no toolchain {tool}"))?;
    let found = toolchains::find(one, &toolchains::path_folders(), &toolchains::chosen());
    let folder = found.program.as_deref().and_then(|program| Path::new(program).parent().map(Path::to_path_buf));
    folder
        .and_then(|folder| toolchains::program_in(&folder, &[name.to_string()]))
        .ok_or_else(|| format!("{} builds it, and is not found: File, Toolchains opens its install page or takes its folder", one.name))
}

fn first<'a>(values: &'a HashMap<String, Vec<String>>, key: &str, default: &'a str) -> &'a str {
    values.get(key).and_then(|given| given.first()).map(String::as_str).filter(|value| !value.is_empty()).unwrap_or(default)
}

fn read_json(path: &Path) -> Result<Value, String> {
    let text = fs::read_to_string(path).map_err(|error| format!("{}: {error}", path.display()))?;
    serde_json::from_str(&text).map_err(|error| format!("{}: {error}", path.display()))
}

/// What a tool prints for `args`, where it ends well.
fn output_of<S: AsRef<std::ffi::OsStr>>(tool: &str, name: &str, args: &[S]) -> Result<Vec<u8>, String> {
    let mut cmd = Command::new(program(tool, name)?);
    cmd.args(args);
    crate::runner::quiet(&mut cmd);
    let first = args.first().map(|arg| arg.as_ref().to_string_lossy().to_string()).unwrap_or_default();
    let out = cmd.output().map_err(|error| format!("{name} {first}: {error}"))?;
    if !out.status.success() {
        return Err(format!("{name} {first}: {}", String::from_utf8_lossy(&out.stderr).trim()));
    }
    Ok(out.stdout)
}

/// The folder Cargo builds the package of `manifest` into, as `cargo metadata` says run in the
/// package's folder, where Cargo reads the package's own settings as its build does.
fn cargo_target(manifest: &Path) -> Result<PathBuf, String> {
    let mut cmd = Command::new(program("rust", "cargo")?);
    cmd.args(["metadata", "--no-deps", "--format-version", "1", "--offline", "--manifest-path"]).arg(manifest);
    if let Some(folder) = manifest.parent() {
        cmd.current_dir(folder);
    }
    crate::runner::quiet(&mut cmd);
    let said = cmd.output().map_err(|error| format!("cargo metadata: {error}"))?;
    if !said.status.success() {
        return Err(format!("cargo metadata: {}", String::from_utf8_lossy(&said.stderr).trim()));
    }
    let out = said.stdout;
    let said: Value = serde_json::from_slice(&out).map_err(|error| format!("cargo metadata: {error}"))?;
    said["target_directory"].as_str().map(PathBuf::from).ok_or_else(|| "cargo metadata names no target_directory".to_string())
}

/// The file the CMake build in `build` made for target `name` in configuration `config`, as the
/// reply of CMake's file API names it: the target's first artifact that is no debug database or
/// import library.
fn cmake_artifact(build: &Path, name: &str, config: &str) -> Result<PathBuf, String> {
    let reply = build.join(".cmake").join("api").join("v1").join("reply");
    let index = fs::read_dir(&reply)
        .map_err(|error| format!("{}: {error}", reply.display()))?
        .flatten()
        .map(|entry| entry.path())
        .filter(|path| path.file_name().is_some_and(|file| file.to_string_lossy().starts_with("index-")))
        .max()
        .ok_or_else(|| format!("CMake wrote no reply in {}", reply.display()))?;
    let index = read_json(&index)?;
    let model = index["objects"].as_array().into_iter().flatten().find(|object| object["kind"] == "codemodel").and_then(|object| object["jsonFile"].as_str()).ok_or("CMake's reply holds no code model")?;
    let model = read_json(&reply.join(model))?;
    let configs = model["configurations"].as_array().cloned().unwrap_or_default();
    let chosen = configs.iter().find(|one| one["name"].as_str().is_some_and(|named| named.eq_ignore_ascii_case(config))).or(configs.first()).ok_or("CMake's code model holds no configuration")?;
    let target = chosen["targets"].as_array().into_iter().flatten().find(|target| target["name"] == name).and_then(|target| target["jsonFile"].as_str()).ok_or_else(|| format!("CMake's code model holds no target {name}"))?;
    let target = read_json(&reply.join(target))?;
    let path = target["artifacts"]
        .as_array()
        .into_iter()
        .flatten()
        .filter_map(|artifact| artifact["path"].as_str())
        .find(|path| !["pdb", "lib", "exp", "ilk"].iter().any(|ext| path.to_lowercase().ends_with(&format!(".{ext}"))))
        .ok_or_else(|| format!("CMake names no file for {name}"))?;
    let path = PathBuf::from(path);
    Ok(if path.is_absolute() { path } else { build.join(path) })
}

/// The file Meson's build in `build` made for executable `name`, as `meson introspect` names it.
fn meson_artifact(build: &Path, name: &str) -> Result<PathBuf, String> {
    let out = output_of("meson", "meson", &[Path::new("introspect"), Path::new("--targets"), build])?;
    let targets: Value = serde_json::from_slice(&out).map_err(|error| format!("meson introspect: {error}"))?;
    targets
        .as_array()
        .into_iter()
        .flatten()
        .find(|target| target["name"] == name && target["type"] == "executable")
        .and_then(|target| target["filename"].as_array()?.first()?.as_str().map(PathBuf::from))
        .ok_or_else(|| format!("Meson names no executable {name}"))
}

/// The program .NET's build made, from the last line it printed for the project: a folder it
/// published to holds the program, and an assembly has the program beside it.
fn dotnet_artifact(name: &str, lines: &[String]) -> Result<PathBuf, String> {
    let mark = format!("{name} -> ");
    let said = lines.iter().rev().find_map(|line| line.trim().strip_prefix(&mark)).ok_or_else(|| format!("dotnet named no file for {name}"))?;
    let path = PathBuf::from(said.trim());
    if said.ends_with(['/', '\\']) || path.is_dir() {
        return Ok(path.join(program_file(name)));
    }
    Ok(if path.extension().is_some_and(|ext| ext.eq_ignore_ascii_case("dll")) { path.with_file_name(program_file(name)) } else { path })
}

/// What a build that failed can be told of its cause, where its lines show one orior knows: NuGet
/// with no package source to fetch from, and PyInstaller not in the Python that runs it. Each says
/// the line that sets it right.
pub fn advice(id: &str, lines: &[String]) -> Vec<String> {
    let mut said = Vec::new();
    let has = |mark: &str| lines.iter().any(|line| line.contains(mark));
    let kind = id.strip_prefix("executable/").and_then(|rest| rest.rsplit_once(':')).and_then(|(script, _)| kind_of(script.rsplit('/').next().unwrap_or_default()));
    if kind == Some(Kind::Dotnet) && (has("NU1100") || has("NU1101")) {
        let sources = output_of("dotnet", "dotnet", &["nuget", "list", "source", "--format", "short"]).map(|out| String::from_utf8_lossy(&out).to_string());
        if sources.is_ok_and(|text| text.trim().is_empty() || text.contains("No sources found")) {
            said.push("NuGet has no package source to fetch the build's packages from; `dotnet nuget add source https://api.nuget.org/v3/index.json -n nuget.org` adds nuget.org".to_string());
        }
    }
    if kind == Some(Kind::Cargo) {
        let named = lines.iter().find_map(|line| line.split("the `").nth(1).and_then(|rest| rest.split('`').next()).filter(|_| line.contains("target may not be installed")));
        if let Some(target) = named {
            said.push(format!("Rust's standard library for {target} is not installed: `rustup target add {target}` installs it"));
        } else if has("linker `") && (has("not found") || has("failed")) || has("linking with `") {
            said.push("linking for the target chosen needs that system's linker: the musl targets link here with rust-lld, and a Linux GNU or macOS target links with cargo-zigbuild where it is installed".to_string());
        }
    }
    if kind == Some(Kind::Pyinstaller) && has("No module named PyInstaller") {
        let python = program("python", "python").map(|path| path.display().to_string()).unwrap_or_else(|_| "python".to_string());
        said.push(format!("PyInstaller is not in the Python that runs it, {python}; `\"{python}\" -m pip install pyinstaller` installs it there"));
    }
    said
}

/// A setting of how the programs a build makes are signed, `signing.<key>` in settings.json in
/// orior's own folder: `windows`, a certificate's thumbprint in the reader's store or the path of a
/// .pfx file, whose password comes from ORIOR_SIGN_PASSWORD and from no file; `timestamp`, the
/// address of the timestamp server a Windows signature is stamped by; `macos`, the identity
/// codesign signs with; and `notarize`, the keychain profile notarytool sends the program with.
fn signing(key: &str) -> Option<String> {
    let path = crate::home::folder()?.join("settings.json");
    let map: serde_json::Map<String, Value> = serde_json::from_str(&fs::read_to_string(path).ok()?).ok()?;
    map.get(&format!("signing.{key}")).and_then(Value::as_str).map(str::trim).filter(|value| !value.is_empty()).map(str::to_string)
}

/// Whether the programs a build makes on this system are signed: where the settings name a
/// certificate for it.
pub fn signs() -> bool {
    if cfg!(windows) {
        signing("windows").is_some()
    } else if cfg!(target_os = "macos") {
        signing("macos").is_some()
    } else {
        false
    }
}

/// The commands that sign the program at `file` as the settings say, each with the line shown for
/// it, a password not shown: on Windows SignTool's; on macOS codesign's, then the program zipped and
/// sent to Apple's notary service where a profile is named.
pub fn sign_steps(file: &Path) -> Result<Vec<(Command, String)>, String> {
    let shown_file = file.display().to_string();
    let mut steps = Vec::new();
    if cfg!(windows) {
        let certificate = signing("windows").ok_or("settings.json names no certificate under signing.windows")?;
        let mut cmd = Command::new(program("signtool", "signtool")?);
        let mut shown = vec!["signtool".to_string(), "sign".into(), "/fd".into(), "sha256".into()];
        cmd.args(["sign", "/fd", "sha256"]);
        let as_file = Path::new(&certificate);
        if as_file.is_file() || [".pfx", ".p12"].iter().any(|ext| certificate.to_lowercase().ends_with(ext)) {
            cmd.arg("/f").arg(as_file);
            shown.extend(["/f".into(), certificate.clone()]);
            if let Some(password) = std::env::var_os("ORIOR_SIGN_PASSWORD") {
                cmd.arg("/p").arg(password);
                shown.extend(["/p".into(), "<ORIOR_SIGN_PASSWORD>".into()]);
            }
        } else {
            let thumbprint: String = certificate.chars().filter(|char| char.is_ascii_hexdigit()).collect();
            cmd.args(["/sha1", &thumbprint]);
            shown.extend(["/sha1".into(), thumbprint]);
        }
        if let Some(server) = signing("timestamp") {
            cmd.args(["/tr", &server, "/td", "sha256"]);
            shown.extend(["/tr".into(), server, "/td".into(), "sha256".into()]);
        }
        cmd.arg(file);
        shown.push(shown_file);
        steps.push((cmd, shown.join(" ")));
    } else if cfg!(target_os = "macos") {
        let identity = signing("macos").ok_or("settings.json names no identity under signing.macos")?;
        let mut cmd = Command::new("codesign");
        cmd.args(["--force", "--options", "runtime", "--timestamp", "--sign", &identity]).arg(file);
        steps.push((cmd, format!("codesign --force --options runtime --timestamp --sign \"{identity}\" {shown_file}")));
        if let Some(profile) = signing("notarize") {
            let zip = file.with_extension("zip");
            let mut pack = Command::new("ditto");
            pack.args(["-c", "-k", "--keepParent"]).arg(file).arg(&zip);
            steps.push((pack, format!("ditto -c -k --keepParent {shown_file} {}", zip.display())));
            let mut send = Command::new("xcrun");
            send.args(["notarytool", "submit"]).arg(&zip).args(["--keychain-profile", &profile, "--wait"]);
            steps.push((send, format!("xcrun notarytool submit {} --keychain-profile {profile} --wait", zip.display())));
        }
    }
    Ok(steps)
}

/// What a signed program can be told of its signature: on Windows whether the system trusts it, as
/// SignTool's own check says; on macOS that a program outside an app, a disk image or an installer
/// cannot hold its notarization ticket, and Gatekeeper asks Apple for it as the program opens.
pub fn signed_said(file: &Path) -> Vec<String> {
    let mut said = Vec::new();
    if cfg!(windows) {
        let checked = Command::new(program("signtool", "signtool").unwrap_or_default()).args(["verify", "/pa"]).arg(file).output();
        if let Ok(out) = checked {
            if out.status.success() {
                said.push("Windows trusts the program's signature".to_string());
            } else {
                let reason = String::from_utf8_lossy(&out.stderr).lines().chain(String::from_utf8_lossy(&out.stdout).lines()).map(str::trim).find(|line| line.starts_with("SignTool Error")).map(str::to_string).unwrap_or_default();
                said.push(format!("the program is signed, and Windows does not trust the signature: {reason}; a certificate from an authority Windows trusts, and a reputation built as the program is downloaded, keep SmartScreen from warning"));
            }
        }
    } else if cfg!(target_os = "macos") && signing("notarize").is_some() {
        said.push("a program outside an app, a disk image or an installer cannot hold its notarization ticket: Gatekeeper asks Apple for it as the program opens, online".to_string());
    }
    said
}

/// The places two builds of a program differ, as ranges of bytes, the first `most` of them, and
/// the count of bytes that differ; files of two lengths differ from the shorter one's end as well.
fn differences(old: &[u8], new: &[u8], most: usize) -> (Vec<(usize, usize)>, usize) {
    let mut ranges: Vec<(usize, usize)> = Vec::new();
    let mut count = 0;
    let mut open: Option<usize> = None;
    let shared = old.len().min(new.len());
    for at in 0..shared {
        if old[at] != new[at] {
            count += 1;
            open.get_or_insert(at);
        } else if let Some(start) = open.take() {
            ranges.push((start, at));
        }
    }
    if let Some(start) = open {
        ranges.push((start, shared));
    }
    if old.len() != new.len() {
        count += old.len().max(new.len()) - shared;
        ranges.push((shared, old.len().max(new.len())));
    }
    let shown = ranges.into_iter().take(most).collect();
    (shown, count)
}

/// Compares the program a build made with the last build of the same commit, where the tree's
/// tracked files hold no change since it, and keeps this build for the next: in
/// build/orior/builds/, one commit's copy for each job. Says whether the two are the same byte for
/// byte, or where they differ, the header's time stamp named where a Windows program's differs.
pub fn compare_build(root: &Path, id: &str, file: &Path) -> Vec<String> {
    let Some((commit, _, true)) = crate::git::head(root) else { return Vec::new() };
    let slug: String = id.chars().map(|char| if char.is_alphanumeric() || char == '.' || char == '-' { char } else { '_' }).collect();
    let folder = root.join("build").join("orior").join("builds").join(slug);
    let name = file.file_name().map(|name| name.to_string_lossy().to_string()).unwrap_or_default();
    let kept = folder.join(&commit).join(&name);
    let Ok(new) = fs::read(file) else { return Vec::new() };
    let mut said = Vec::new();
    if let Ok(old) = fs::read(&kept) {
        let short = &commit[..commit.len().min(10)];
        let (ranges, count) = differences(&old, &new, 3);
        if count == 0 {
            said.push(format!("{name} is the same, byte for byte, as the last build of commit {short}"));
        } else {
            let stamp = new.starts_with(b"MZ").then(|| u32_at_le(&new, 0x3C)).flatten().map(|pe| pe as usize + 8..pe as usize + 12);
            let places: Vec<String> = ranges
                .iter()
                .map(|(start, end)| {
                    let field = stamp.as_ref().filter(|stamp| stamp.start < *end && *start < stamp.end).map(|_| " (the header's time stamp)").unwrap_or_default();
                    format!("{start:#x} to {end:#x}{field}")
                })
                .collect();
            let mut line = format!("{name} differs from the last build of commit {short} in {count} bytes, first at {}", places.join(", "));
            // MSVC's linker writes its Rich header between the DOS stub and the PE header.
            let msvc = stamp.as_ref().is_some_and(|stamp| new.get(0x80..stamp.start).is_some_and(|head| head.windows(4).any(|four| four == b"Rich")));
            let stamped = ranges.iter().any(|(start, end)| stamp.as_ref().is_some_and(|stamp| stamp.start < *end && *start < stamp.end));
            if msvc && stamped {
                line.push_str("; MSVC's linker writes the time and a new id into each build, and its /Brepro flag keeps them out: -C link-arg=/Brepro in the tree's rustflags, or the flag among a C or C++ build's linker flags");
            }
            said.push(line);
        }
    }
    let _ = fs::remove_dir_all(&folder);
    if fs::create_dir_all(folder.join(&commit)).is_ok() {
        let _ = fs::write(&kept, &new);
    }
    said
}

fn u32_at_le(bytes: &[u8], at: usize) -> Option<u32> {
    bytes.get(at..at + 4).map(|four| u32::from_le_bytes([four[0], four[1], four[2], four[3]]))
}

/// The variables a tree's .env sets, each `NAME=value` line in order: a leading `export` taken off,
/// a value in quotes taken without them, and comments and blank lines passed over.
pub fn dot_env(text: &str) -> Vec<(String, String)> {
    text.lines()
        .map(str::trim)
        .filter(|line| !line.is_empty() && !line.starts_with('#'))
        .filter_map(|line| {
            let line = line.strip_prefix("export ").unwrap_or(line);
            let (name, value) = line.split_once('=')?;
            let name = name.trim();
            let value = value.trim();
            let value = match value.chars().next() {
                Some(quote @ ('"' | '\'')) => value[1..].split(quote).next().unwrap_or_default().to_string(),
                _ => value.split(" #").next().unwrap_or_default().trim().to_string(),
            };
            (!name.is_empty() && name.chars().all(|char| char.is_alphanumeric() || char == '_')).then(|| (name.to_string(), value))
        })
        .collect()
}

/// The command that runs the program a build made, and the line shown for it: in the folder the
/// job's `folder` names, the tree's top folder where it names none, with the variables of the tree's
/// .env, their values not shown. Where `system`, its PATH is the PATH a program opened on its own
/// gets; else it is orior's, and a .NET program that needs the .NET runtime is told where the .NET
/// that built it is.
pub fn run_command(root: &Path, file: &Path, values: &HashMap<String, Vec<String>>, system: bool) -> (Command, String) {
    let mut cmd = Command::new(file);
    let folder = first(values, "folder", "");
    cmd.current_dir(if folder.is_empty() { root.to_path_buf() } else { crate::root::full(root, folder) });
    let mut shown: Vec<String> = Vec::new();
    for (name, value) in dot_env(&fs::read_to_string(root.join(".env")).unwrap_or_default()) {
        cmd.env(&name, value);
        shown.push(format!("{name}=…"));
    }
    if system {
        cmd.env("PATH", std::env::join_paths(toolchains::system_path()).unwrap_or_default());
        shown.push("PATH=<the system's PATH>".to_string());
    } else if file.with_extension("runtimeconfig.json").is_file() {
        if let Some(dotnet) = program("dotnet", "dotnet").ok().and_then(|path| path.parent().map(Path::to_path_buf)) {
            shown.push(format!("DOTNET_ROOT={}", dotnet.display()));
            cmd.env("DOTNET_ROOT", dotnet);
        }
    }
    if !folder.is_empty() {
        shown.insert(0, format!("cd {folder} &&"));
    }
    let path = if file.starts_with(root) { relative(root, file) } else { file.display().to_string() };
    shown.push(path);
    (cmd, shown.join(" "))
}

/// The folders a program opened on its own looks in for the .NET runtime: DOTNET_ROOT where the
/// system sets it, then where .NET installs for every user.
fn dotnet_places() -> Vec<PathBuf> {
    let mut places: Vec<PathBuf> = std::env::var_os("DOTNET_ROOT").map(PathBuf::from).into_iter().collect();
    if cfg!(windows) {
        places.extend(std::env::var_os("ProgramFiles").map(|folder| PathBuf::from(folder).join("dotnet")));
    } else {
        places.extend(fs::read_to_string("/etc/dotnet/install_location").ok().map(|text| PathBuf::from(text.trim())));
        places.extend(["/usr/share/dotnet", "/usr/lib/dotnet", "/usr/local/share/dotnet"].map(PathBuf::from));
    }
    places
}

/// What a .NET program that is no single file needs of the .NET runtime, and where a program opened
/// on its own finds none: each framework its runtimeconfig.json names, by name and version, that no
/// place of `dotnet_places` holds a version of with the same first number and at least as new.
fn dotnet_missing(file: &Path) -> Vec<String> {
    let Ok(config) = read_json(&file.with_extension("runtimeconfig.json")) else { return Vec::new() };
    let options = &config["runtimeOptions"];
    if options.get("includedFrameworks").is_some() {
        return Vec::new();
    }
    let frameworks: Vec<&Value> = options.get("framework").into_iter().chain(options["frameworks"].as_array().into_iter().flatten()).collect();
    let places = dotnet_places();
    let numbers = |version: &str| version.split(['.', '-']).filter_map(|part| part.parse::<u32>().ok()).collect::<Vec<_>>();
    frameworks
        .into_iter()
        .filter_map(|framework| {
            let (name, version) = (framework["name"].as_str()?, framework["version"].as_str()?);
            let wanted = numbers(version);
            let held = places.iter().any(|place| {
                fs::read_dir(place.join("shared").join(name)).into_iter().flatten().flatten().any(|entry| {
                    let have = numbers(&entry.file_name().to_string_lossy());
                    have.first() == wanted.first() && have >= wanted
                })
            });
            (!held).then(|| format!("{name} {version}"))
        })
        .collect()
}

/// What a build that made its program can be told of it past what it loads: a .NET program that
/// needs a .NET runtime a program opened on its own does not find, and the modules a PyInstaller
/// build warns it did not find that the tree's own code imports, each with the hidden import that
/// takes it in.
pub fn built_advice(root: &Path, id: &str, lines: &[String]) -> Vec<String> {
    let Some((script, _)) = id.strip_prefix("executable/").and_then(|rest| rest.rsplit_once(':')) else { return Vec::new() };
    let path = root.join(script);
    let mut said = Vec::new();
    match kind_of(&path.file_name().map(|name| name.to_string_lossy().to_string()).unwrap_or_default()) {
        Some(Kind::Dotnet) => {
            let name = id.rsplit(':').next().unwrap_or_default();
            if let Ok(file) = dotnet_artifact(name, lines) {
                for framework in dotnet_missing(&file) {
                    let places: Vec<String> = dotnet_places().iter().map(|place| place.display().to_string()).collect();
                    said.push(format!(
                        "{} needs the .NET runtime {framework}, which a program opened on its own looks for in {} and does not find: Publish as One File, the job's output one file, carries it inside the program",
                        file.file_name().map(|one| one.to_string_lossy().to_string()).unwrap_or_default(),
                        places.join(" and ")
                    ));
                }
            }
        }
        Some(Kind::Pyinstaller) => {
            let folder = path.parent().unwrap_or(root);
            let stem = path.file_stem().map(|stem| stem.to_string_lossy().to_string()).unwrap_or_default();
            let warn = fs::read_to_string(folder.join("build").join(&stem).join(format!("warn-{stem}.txt"))).unwrap_or_default();
            let own: HashSet<String> = fs::read_dir(folder)
                .into_iter()
                .flatten()
                .flatten()
                .filter_map(|entry| {
                    let name = entry.file_name().to_string_lossy().to_string();
                    let path = entry.path();
                    if path.is_dir() && path.join("__init__.py").is_file() {
                        Some(name)
                    } else {
                        name.strip_suffix(".py").map(str::to_string)
                    }
                })
                .chain(std::iter::once("__main__".to_string()))
                .collect();
            // An importer is named as a module, or as the path of a script the spec freezes.
            let is_own = |importer: &str| {
                let as_path = Path::new(importer);
                if as_path.extension().is_some_and(|ext| ext == "py") && as_path.is_absolute() {
                    return as_path.starts_with(folder);
                }
                own.contains(importer.split('.').next().unwrap_or_default())
            };
            for (module, importers) in pyinstaller_missing(&warn) {
                let by: Vec<String> = importers
                    .iter()
                    .filter(|(importer, kinds)| is_own(importer) && !kinds.contains("optional"))
                    .map(|(importer, _)| Path::new(importer).file_name().filter(|_| importer.ends_with(".py")).map_or_else(|| importer.clone(), |name| name.to_string_lossy().to_string()))
                    .collect();
                if let Some(first) = by.first() {
                    said.push(format!("{module}, which {first} imports, is not in the frozen program: PyInstaller found it nowhere in the Python that ran it; install it there and build again"));
                }
            }
        }
        _ => {}
    }
    said
}

/// The modules a PyInstaller warnings file says it did not find, each with the modules that import
/// it and how, as `missing module named X - imported by A (top-level), B (delayed, conditional)`.
fn pyinstaller_missing(text: &str) -> Vec<(String, Vec<(String, String)>)> {
    text.lines()
        .filter_map(|line| line.strip_prefix("missing module named "))
        .filter_map(|rest| {
            let (module, importers) = rest.split_once(" - imported by ")?;
            let module = module.trim().trim_matches('\'').to_string();
            let importers = importers
                .split("), ")
                .filter_map(|one| {
                    let (name, kinds) = one.trim_end_matches(')').rsplit_once(" (")?;
                    Some((name.trim().to_string(), kinds.to_string()))
                })
                .collect();
            Some((module, importers))
        })
        .collect()
}

/// The fullest module name the Python files beside a spec write out in quotes that starts with
/// `module`, as `importlib.import_module("email.mime.text")` does for `email.mime`.
fn fullest_name(folder: &Path, module: &str) -> String {
    let mut best = module.to_string();
    for entry in fs::read_dir(folder).into_iter().flatten().flatten().filter(|entry| entry.path().extension().is_some_and(|ext| ext == "py")) {
        let text = fs::read_to_string(entry.path()).unwrap_or_default();
        for quote in ['"', '\''] {
            for piece in text.split(quote).skip(1).step_by(2) {
                let named = piece.chars().all(|char| char.is_alphanumeric() || char == '_' || char == '.');
                if named && (piece == module || piece.starts_with(&format!("{module}."))) && piece.len() > best.len() {
                    best = piece.to_string();
                }
            }
        }
    }
    best
}

/// What a run of the program a build made can be told of how it ended: on Windows, the codes the
/// system ends a program with that cannot load its DLLs; and a frozen Python program's module that
/// is not in it, as one PyInstaller found nowhere or as one the program imports by its name as it
/// runs, which PyInstaller cannot see.
pub fn ran_advice(root: &Path, id: &str, code: Option<i32>, lines: &[String]) -> Vec<String> {
    let mut said = Vec::new();
    if cfg!(windows) {
        let reason = match code.map(|code| code as u32) {
            Some(0xC000_0135) => Some("a DLL it loads was found nowhere (0xC0000135)"),
            Some(0xC000_007B) => Some("a DLL it loads is built for another processor, or is damaged (0xC000007B)"),
            Some(0xC000_0139) => Some("a DLL it loads lacks a function it calls, another version than the one it was built with (0xC0000139)"),
            _ => None,
        };
        if let Some(reason) = reason {
            said.push(format!("the program did not start: {reason}; the build's end says which DLLs it loads from where"));
        }
    }
    let spec = id.strip_prefix("executable/").and_then(|rest| rest.rsplit_once(':')).map(|(script, _)| script).filter(|script| script.ends_with(".spec"));
    if let Some(spec) = spec {
        let path = root.join(spec);
        let folder = path.parent().unwrap_or(root);
        let stem = path.file_stem().map(|stem| stem.to_string_lossy().to_string()).unwrap_or_default();
        let warn = fs::read_to_string(folder.join("build").join(&stem).join(format!("warn-{stem}.txt"))).unwrap_or_default();
        let unfound: HashSet<String> = pyinstaller_missing(&warn).into_iter().map(|(module, _)| module).collect();
        for line in lines {
            let Some(rest) = line.split("No module named ").nth(1) else { continue };
            let module = rest.trim().trim_matches('\'').trim_matches('"');
            if module.is_empty() {
                continue;
            }
            let top = module.split('.').next().unwrap_or(module);
            if unfound.contains(module) || unfound.contains(top) {
                said.push(format!("{module} is not in the frozen program: PyInstaller found it nowhere in the Python that ran it; install it there and build again"));
            } else {
                let name = fullest_name(folder, module);
                said.push(format!("{name} is a module the frozen program imports by its name as it runs, which PyInstaller cannot see: add '{name}' to hiddenimports in {spec}"));
            }
        }
    }
    said
}

/// The file the job `id` built with `values`, read back from its id and the lines its build printed.
pub fn made(root: &Path, id: &str, values: &HashMap<String, Vec<String>>, lines: &[String]) -> Result<PathBuf, String> {
    let (script, name) = id.strip_prefix("executable/").and_then(|rest| rest.rsplit_once(':')).ok_or_else(|| format!("{id} is no executable's job"))?;
    let path = root.join(script);
    let folder = path.parent().unwrap_or(root).to_path_buf();
    let kind = kind_of(&path.file_name().map(|file| file.to_string_lossy().to_string()).unwrap_or_default()).ok_or_else(|| format!("{script} is no build script"))?;
    let file = match kind {
        Kind::Cargo => {
            let profile = first(values, "profile", "dev");
            let profile_folder = if profile == "dev" { "debug" } else { profile };
            match first(values, "target", THIS_MACHINE) {
                THIS_MACHINE => cargo_target(&path)?.join(profile_folder).join(program_file(name)),
                target => {
                    let file = if target.contains("windows") { format!("{name}.exe") } else { name.to_string() };
                    cargo_target(&path)?.join(target).join(profile_folder).join(file)
                }
            }
        }
        Kind::Cmake => {
            let config = first(values, "config", "Debug");
            cmake_artifact(&root.join(format!("{}{config}", build_folder(root, &path, "cmake"))), name, config)?
        }
        Kind::Meson => meson_artifact(&root.join(format!("{}{}", build_folder(root, &path, "meson"), first(values, "buildtype", "debug"))), name)?,
        Kind::Go => {
            let (host_system, host_processor) = go_host();
            let system = first(values, "GOOS", host_system);
            let file = if system == "windows" { format!("{name}.exe") } else { name.to_string() };
            root.join("build").join("exe").join(format!("{system}-{}", first(values, "GOARCH", host_processor))).join(file)
        }
        Kind::Npm => root.join("build").join("exe").join(program_file(name)),
        Kind::Dotnet => dotnet_artifact(name, lines)?,
        Kind::Zig => folder.join("zig-out").join("bin").join(program_file(name)),
        Kind::Pyinstaller => {
            let text = fs::read_to_string(&path).map_err(|error| format!("{script}: {error}"))?;
            let (_, under) = spec_made(&text).ok_or_else(|| format!("{script} is no PyInstaller spec"))?;
            folder.join(under)
        }
    };
    if file.is_file() { Ok(file) } else { Err(format!("the build made no {}", file.display())) }
}

#[cfg(test)]
mod reading {
    use super::*;

    fn scratch(name: &str) -> PathBuf {
        let dir = std::env::temp_dir().join(format!("orior_executables_{name}_{}", std::process::id()));
        let _ = fs::remove_dir_all(&dir);
        fs::create_dir_all(&dir).unwrap();
        dir
    }

    fn put(dir: &Path, path: &str, text: &str) {
        let file = dir.join(path);
        fs::create_dir_all(file.parent().unwrap()).unwrap();
        fs::write(file, text).unwrap();
    }

    #[test]
    fn cargo_gives_its_named_and_found_binaries() {
        let dir = scratch("cargo");
        put(&dir, "Cargo.toml", "[package]\nname = \"tool\" # the package\n\n[[bin]]\nname = 'second'\npath = \"src/second.rs\"\n\n[dependencies]\nname = \"not this\"\n");
        put(&dir, "src/main.rs", "fn main() {}\n");
        put(&dir, "src/bin/third.rs", "fn main() {}\n");
        put(&dir, "src/bin/fourth/main.rs", "fn main() {}\n");
        let text = fs::read_to_string(dir.join("Cargo.toml")).unwrap();
        assert_eq!(cargo_bins(&dir.join("Cargo.toml"), &text), vec!["second", "tool", "fourth", "third"]);
        let off = "[package]\nname = \"tool\"\nautobins = false\n";
        assert!(cargo_bins(&dir.join("Cargo.toml"), off).is_empty());
        assert!(cargo_bins(&dir.join("Cargo.toml"), "[workspace]\nmembers = [\"a\"]\n").is_empty());
        let _ = fs::remove_dir_all(&dir);
    }

    #[test]
    fn cmake_gives_executables_past_imports_aliases_and_comments() {
        let text = "project(demo C)\n# add_executable(commented main.c)\nadd_executable(${PROJECT_NAME} main.c)\nADD_EXECUTABLE ( \"tool\"\n  tool.c)\nadd_executable(other IMPORTED)\nadd_executable(short ALIAS tool)\nadd_executable(${NAME} x.c)\nadd_library(lib STATIC a.c)\n";
        assert_eq!(cmake_project(text).as_deref(), Some("demo"));
        assert_eq!(cmake_targets(text, "demo"), vec!["demo", "tool"]);
    }

    #[test]
    fn meson_gives_the_executables_written_out() {
        let text = "project('me', 'c')\n# executable('commented', 'x.c')\nhello = executable('hello', 'main.c',\n  install : true)\nexecutable(name_var, 'y.c')\nshared_library('lib', 'a.c')\n";
        assert_eq!(meson_project(text).as_deref(), Some("me"));
        assert_eq!(meson_targets(text), vec!["hello"]);
    }

    #[test]
    fn zig_gives_the_names_of_both_forms_of_its_calls() {
        let text = "const exe = b.addExecutable(.{\n    .name = \"zg\",\n    .root_module = b.createModule(.{ .imports = &.{ .{ .name = \"other\", .module = mod } } }),\n});\n// b.addExecutable(.{ .name = \"commented\" });\nconst old = b.addExecutable(\"older\", \"src/main.zig\");\n";
        assert_eq!(zig_exes(text), vec!["zg", "older"]);
    }

    #[test]
    fn a_spec_gives_its_file_by_whether_it_collects_a_folder() {
        let one_file = "a = Analysis(['app.py'])\npyz = PYZ(a.pure)\nexe = EXE(pyz, a.scripts, a.binaries, name='app', console=True)\n";
        assert_eq!(spec_made(one_file), Some(("app".to_string(), format!("dist/{}", program_file("app")))));
        let folder = "a = Analysis(['app.py'])\nexe = EXE(pyz, exclude_binaries=True, name=\"app\")\ncoll = COLLECT(exe, a.binaries, name=\"bundle\")\n";
        assert_eq!(spec_made(folder), Some(("app".to_string(), format!("dist/bundle/{}", program_file("app")))));
        assert_eq!(spec_made("Name: rpm-spec\nVersion: 1\n"), None);
    }

    #[test]
    fn a_package_gives_its_bins_by_name() {
        assert_eq!(npm_bins("{\"name\":\"@x/greet\",\"bin\":\"cli.js\"}"), vec![("greet".to_string(), "cli.js".to_string())]);
        assert_eq!(npm_bins("{\"name\":\"tools\",\"bin\":{\"a\":\"a.js\",\"b\":\"bin/b.mjs\"}}"), vec![("a".to_string(), "a.js".to_string()), ("b".to_string(), "bin/b.mjs".to_string())]);
        assert!(npm_bins("{\"name\":\"lib\"}").is_empty());
    }

    #[test]
    fn go_gives_the_main_packages_by_folder() {
        let dir = scratch("go");
        put(&dir, "go.mod", "module example.com/top/thing\n\ngo 1.22\n");
        put(&dir, "main.go", "// Command thing.\n//go:build linux || windows\n\npackage main\n");
        put(&dir, "cmd/serve/serve.go", "/* the server\n */\npackage main\n");
        put(&dir, "cmd/serve/serve_test.go", "package main_test\n");
        put(&dir, "lib/lib.go", "package lib\n");
        put(&dir, "inner/go.mod", "module example.com/inner\n");
        put(&dir, "inner/main.go", "package main\n");
        let text = fs::read_to_string(dir.join("go.mod")).unwrap();
        assert_eq!(go_mains(&dir.join("go.mod"), &text), vec![("serve".to_string(), "./cmd/serve".to_string()), ("thing".to_string(), ".".to_string())]);
        let _ = fs::remove_dir_all(&dir);
    }

    #[test]
    fn dotnet_gives_the_program_of_an_exe_project_and_finds_it_in_its_lines() {
        let project = Path::new("app/Hello.csproj");
        assert_eq!(dotnet_program(project, "<Project><PropertyGroup><OutputType>Exe</OutputType></PropertyGroup></Project>").as_deref(), Some("Hello"));
        assert_eq!(dotnet_program(project, "<OutputType>WinExe</OutputType><AssemblyName>hi</AssemblyName>").as_deref(), Some("hi"));
        assert_eq!(dotnet_program(project, "<OutputType>Exe</OutputType><AssemblyName>$(Name)x</AssemblyName>").as_deref(), Some("Hello"));
        assert_eq!(dotnet_program(project, "<OutputType>Library</OutputType>"), None);
        let built = vec!["  Hello -> C:/t/bin/Debug/net10.0/Hello.dll".to_string()];
        assert_eq!(dotnet_artifact("Hello", &built).unwrap(), PathBuf::from("C:/t/bin/Debug/net10.0").join(program_file("Hello")));
        let published = vec!["  Hello -> C:/t/bin/Release/net10.0/win-x64/Hello.dll".to_string(), "  Hello -> C:/t/bin/Release/net10.0/win-x64/publish/".to_string()];
        assert_eq!(dotnet_artifact("Hello", &published).unwrap(), PathBuf::from("C:/t/bin/Release/net10.0/win-x64/publish/").join(program_file("Hello")));
    }

    #[test]
    fn a_tree_gives_a_job_for_each_program_and_titles_the_names_it_shares() {
        let dir = scratch("tree");
        put(&dir, "a/Cargo.toml", "[package]\nname = \"same\"\n");
        put(&dir, "a/src/main.rs", "fn main() {}\n");
        put(&dir, "b/Cargo.toml", "[package]\nname = \"same\"\n");
        put(&dir, "b/src/main.rs", "fn main() {}\n");
        put(&dir, "c/CMakeLists.txt", "project(c)\nadd_subdirectory(tools)\n");
        put(&dir, "c/tools/CMakeLists.txt", "project(tools)\nadd_executable(cli cli.c)\n");
        put(&dir, "target/x/Cargo.toml", "[package]\nname = \"built\"\n");
        put(&dir, "target/x/src/main.rs", "fn main() {}\n");
        let mut found = Vec::new();
        jobs(&dir, &mut found);
        let ids: Vec<(&str, &str)> = found.iter().map(|job| (job.id.as_str(), job.title.as_str())).collect();
        assert_eq!(ids, vec![("executable/a/Cargo.toml:same", "a/same"), ("executable/b/Cargo.toml:same", "b/same"), ("executable/c/CMakeLists.txt:cli", "cli")]);
        let cmake = &found[2];
        let values: HashMap<String, Vec<String>> = [("config".to_string(), vec!["Release".to_string()])].into_iter().collect();
        if let Ok(lines) = crate::runner::shown(&dir, cmake, &values) {
            assert_eq!(lines[2], "cmake -S c -B build/cmake/c/Release -DCMAKE_BUILD_TYPE=Release");
            assert_eq!(lines[3], "cmake --build build/cmake/c/Release --target cli --config Release");
        }
        let _ = fs::remove_dir_all(&dir);
    }
}

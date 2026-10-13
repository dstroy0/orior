// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The compilation database clangd reads: the build's own. The code help reads the compiler the
//! build uses, with its include folders and its defines, and no file of settings is written by hand.
//!
//! A tree that holds its own database keeps it: a compile_commands.json at its top or in its build/
//! folder, or the folder a .clangd at its top names. Otherwise each CMake project orior configured
//! gives its targets' sources from CMake's file API, which answers for every generator, Visual
//! Studio's among them, from the configuration configured last; and each Meson build gives the
//! compile_commands.json Meson writes. They are written together to build/orior/compile_commands.json.
//!
//! clangd asks each compiler the database names, but MSVC's cl, for its own headers and target
//! (`--query-driver`); where one is cl, clangd runs in Visual Studio's environment, as cl does. The
//! database is looked for again at most every LOOKED_FOR, and a clangd running when it changes is
//! started again on it.

use std::collections::{BTreeSet, HashMap};
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::sync::{Mutex, OnceLock};
use std::time::{Duration, Instant, SystemTime};

use serde_json::{json, Value};

/// Where the database gathered from the tree's builds is written, from the tree's top.
pub const WRITTEN: &str = "build/orior/compile_commands.json";

/// How long one look for the database stands before the builds are looked at again.
const LOOKED_FOR: Duration = Duration::from_secs(2);

/// How deep below build/cmake and build/meson a build is looked for.
const DEPTH: usize = 6;

/// The database clangd reads: the folder to give it where it is not one clangd finds itself, the
/// compilers it names, and whether one of them is MSVC's cl.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct Database {
    pub folder: Option<PathBuf>,
    pub compilers: Vec<String>,
    pub msvc: bool,
}

impl Database {
    /// The compilers clangd may ask for their headers and target: every one but cl, which answers
    /// no such question.
    pub fn drivers(&self) -> Vec<String> {
        self.compilers.iter().filter(|one| !is_cl(one)).cloned().collect()
    }

    /// The words clangd starts with for this database.
    pub fn words(&self) -> Vec<String> {
        let mut words = Vec::new();
        if let Some(folder) = &self.folder {
            words.push(format!("--compile-commands-dir={}", folder.display()));
        }
        let drivers = self.drivers();
        if !drivers.is_empty() {
            words.push(format!("--query-driver={}", drivers.join(",")));
        }
        words
    }
}

/// The database of the tree at `root`, and whether it changed since the last look.
pub struct Looked {
    pub database: Database,
    pub changed: bool,
}

/// The last look: its tree, when, the files it read with their times, and what it found.
type Kept = Option<(PathBuf, Instant, Vec<(PathBuf, SystemTime)>, Database)>;

static KEPT: Mutex<Kept> = Mutex::new(None);

/// The database of the tree at `root`, gathered and written where the tree's builds changed since
/// the last look, at most every LOOKED_FOR.
pub fn refresh(root: &Path) -> Looked {
    let Ok(mut kept) = KEPT.lock() else {
        return Looked { database: gather(root, &inputs(root)), changed: false };
    };
    if let Some((at, when, _, database)) = kept.as_ref() {
        if at == root && when.elapsed() < LOOKED_FOR {
            return Looked { database: database.clone(), changed: false };
        }
    }
    let read = inputs(root);
    if let Some((at, when, before, database)) = kept.as_mut() {
        if at == root && *before == read.files {
            *when = Instant::now();
            return Looked { database: database.clone(), changed: false };
        }
    }
    let database = gather(root, &read);
    // The builds read changed since the last look at this tree: clangd is to read the database again.
    let changed = matches!(kept.as_ref(), Some((at, ..)) if at == root);
    *kept = Some((root.to_path_buf(), Instant::now(), read.files, database.clone()));
    Looked { database, changed }
}

/// What a look reads: the tree's own database, where it has one, or each build orior configured.
struct Inputs {
    own: Option<PathBuf>,
    files: Vec<(PathBuf, SystemTime)>,
}

fn stamped(path: PathBuf) -> Option<(PathBuf, SystemTime)> {
    let time = std::fs::metadata(&path).and_then(|meta| meta.modified()).ok()?;
    Some((path, time))
}

fn inputs(root: &Path) -> Inputs {
    let mut own = vec![root.join("compile_commands.json"), root.join("build").join("compile_commands.json")];
    if let Ok(text) = std::fs::read_to_string(root.join(".clangd")) {
        if let Some(folder) = text.lines().find_map(|line| line.trim().strip_prefix("CompilationDatabase:")).map(|rest| rest.split('#').next().unwrap_or_default().trim().trim_matches('"').to_string()) {
            if !folder.is_empty() {
                own.insert(0, root.join(folder).join("compile_commands.json"));
            }
        }
    }
    if let Some(found) = own.into_iter().find(|path| path.is_file()) {
        return Inputs { own: Some(found.clone()), files: stamped(found).into_iter().collect() };
    }
    let mut files = Vec::new();
    for reply in folders_named(&root.join("build").join("cmake"), &[".cmake", "api", "v1", "reply"]) {
        let newest = std::fs::read_dir(&reply).into_iter().flatten().flatten().map(|entry| entry.path()).filter(|path| path.file_name().is_some_and(|name| name.to_string_lossy().starts_with("index-"))).max();
        files.extend(newest.and_then(stamped));
    }
    files.extend(files_named(&root.join("build").join("meson"), "compile_commands.json").into_iter().filter_map(stamped));
    files.sort();
    Inputs { own: None, files }
}

/// Each folder below `top`, at most DEPTH down, that ends in the folders `tail`.
fn folders_named(top: &Path, tail: &[&str]) -> Vec<PathBuf> {
    let mut found = Vec::new();
    let mut stack = vec![(top.to_path_buf(), 0)];
    while let Some((folder, depth)) = stack.pop() {
        let below = tail.iter().fold(folder.clone(), |path, part| path.join(part));
        if below.is_dir() {
            found.push(below);
            continue;
        }
        if depth >= DEPTH {
            continue;
        }
        for entry in std::fs::read_dir(&folder).into_iter().flatten().flatten() {
            if entry.file_type().is_ok_and(|kind| kind.is_dir()) && !entry.file_name().to_string_lossy().starts_with('.') {
                stack.push((entry.path(), depth + 1));
            }
        }
    }
    found
}

/// Each file named `name` below `top`, at most DEPTH down.
fn files_named(top: &Path, name: &str) -> Vec<PathBuf> {
    let mut found = Vec::new();
    let mut stack = vec![(top.to_path_buf(), 0)];
    while let Some((folder, depth)) = stack.pop() {
        if folder.join(name).is_file() {
            found.push(folder.join(name));
        }
        if depth >= DEPTH {
            continue;
        }
        for entry in std::fs::read_dir(&folder).into_iter().flatten().flatten() {
            if entry.file_type().is_ok_and(|kind| kind.is_dir()) && !entry.file_name().to_string_lossy().starts_with('.') {
                stack.push((entry.path(), depth + 1));
            }
        }
    }
    found
}

/// The database `read` makes: the tree's own, read for its compilers, or the builds' written
/// together, a project configured more than once taking its configuration configured last.
fn gather(root: &Path, read: &Inputs) -> Database {
    if let Some(own) = &read.own {
        let entries: Vec<Value> = std::fs::read_to_string(own).ok().and_then(|text| serde_json::from_str(&text).ok()).unwrap_or_default();
        return of_entries(None, &entries);
    }
    // Each CMake project's newest configuration, by the project's own folder, and each Meson build's.
    let mut by_project: HashMap<String, (SystemTime, Vec<Value>)> = HashMap::new();
    for (file, time) in &read.files {
        let (project, entries) = if file.file_name().is_some_and(|name| name == "compile_commands.json") {
            let entries: Vec<Value> = std::fs::read_to_string(file).ok().and_then(|text| serde_json::from_str(&text).ok()).unwrap_or_default();
            let project = file.parent().and_then(Path::parent).map(|path| path.display().to_string()).unwrap_or_default();
            (project, entries)
        } else {
            match cmake_entries(file) {
                Some((source, entries)) => (source, entries),
                None => continue,
            }
        };
        if by_project.get(&project).is_none_or(|(kept, _)| kept < time) {
            by_project.insert(project, (*time, entries));
        }
    }
    let mut projects: Vec<(String, (SystemTime, Vec<Value>))> = by_project.into_iter().collect();
    projects.sort_by(|a, b| a.0.cmp(&b.0));
    let entries: Vec<Value> = projects.into_iter().flat_map(|(_, (_, entries))| entries).collect();
    if entries.is_empty() {
        return Database::default();
    }
    let path = root.join(WRITTEN);
    let text = serde_json::to_string_pretty(&entries).unwrap_or_default();
    if std::fs::read_to_string(&path).ok().as_deref() != Some(text.as_str()) {
        if let Some(folder) = path.parent() {
            let _ = std::fs::create_dir_all(folder);
        }
        let _ = std::fs::write(&path, &text);
    }
    of_entries(path.parent().map(Path::to_path_buf), &entries)
}

/// A database of `entries`, given to clangd from `folder`, with the compilers they name.
fn of_entries(folder: Option<PathBuf>, entries: &[Value]) -> Database {
    let compilers: BTreeSet<String> = entries.iter().filter_map(|entry| match entry["arguments"].as_array() {
        Some(words) => words.first().and_then(Value::as_str).map(str::to_string),
        None => entry["command"].as_str().and_then(|command| words(command).into_iter().next()),
    }).map(|compiler| compiler.replace('\\', "/")).collect();
    let msvc = compilers.iter().any(|one| is_cl(one));
    Database { folder, compilers: compilers.into_iter().collect(), msvc }
}

fn is_cl(compiler: &str) -> bool {
    Path::new(compiler).file_stem().is_some_and(|stem| stem.eq_ignore_ascii_case("cl"))
}

/// The words of a command line, a word in double quotes kept whole.
fn words(text: &str) -> Vec<String> {
    let mut found = Vec::new();
    let mut word = String::new();
    let mut quoted = false;
    let mut any = false;
    for c in text.chars() {
        match c {
            '"' => {
                quoted = !quoted;
                any = true;
            }
            c if c.is_whitespace() && !quoted => {
                if any {
                    found.push(std::mem::take(&mut word));
                    any = false;
                }
            }
            c => {
                word.push(c);
                any = true;
            }
        }
    }
    if any {
        found.push(word);
    }
    found
}

fn read_json(path: &Path) -> Option<Value> {
    serde_json::from_str(&std::fs::read_to_string(path).ok()?).ok()
}

/// The entries of the CMake build whose file API reply `index` is: the project's source folder, and
/// one entry for each source of each target in the configuration the build's folder is named for,
/// as the build's own compiler is given it. C and C++ alone: CUDA's compiler takes words clangd
/// does not.
fn cmake_entries(index: &Path) -> Option<(String, Vec<Value>)> {
    let reply = index.parent()?;
    let told = read_json(index)?;
    let model = read_json(&reply.join(told["reply"]["codemodel-v2"]["jsonFile"].as_str()?))?;
    let mut compilers: HashMap<String, (String, String)> = HashMap::new();
    if let Some(file) = told["reply"]["toolchains-v1"]["jsonFile"].as_str() {
        for one in read_json(&reply.join(file)).map(|chains| chains["toolchains"].as_array().cloned().unwrap_or_default()).unwrap_or_default() {
            if let (Some(language), Some(path)) = (one["language"].as_str(), one["compiler"]["path"].as_str()) {
                compilers.insert(language.to_string(), (path.to_string(), one["compiler"]["id"].as_str().unwrap_or_default().to_string()));
            }
        }
    }
    let source = PathBuf::from(model["paths"]["source"].as_str()?);
    let build = model["paths"]["build"].as_str()?.to_string();
    // The configuration the build's folder is named for, as orior names it, or the first.
    let named = Path::new(&build).file_name().map(|name| name.to_string_lossy().to_string()).unwrap_or_default();
    let configurations = model["configurations"].as_array()?;
    let configuration = configurations.iter().find(|one| one["name"].as_str().is_some_and(|name| name.eq_ignore_ascii_case(&named))).or_else(|| configurations.first())?;
    let mut entries = Vec::new();
    for target in configuration["targets"].as_array().into_iter().flatten() {
        let Some(target) = target["jsonFile"].as_str().and_then(|file| read_json(&reply.join(file))) else { continue };
        let sources = target["sources"].as_array().cloned().unwrap_or_default();
        for group in target["compileGroups"].as_array().into_iter().flatten() {
            let language = group["language"].as_str().unwrap_or_default();
            if language != "C" && language != "CXX" {
                continue;
            }
            let Some((compiler, id)) = compilers.get(language) else { continue };
            let msvc = id == "MSVC" || is_cl(compiler);
            let mut flags: Vec<String> = group["compileCommandFragments"].as_array().into_iter().flatten().filter_map(|one| one["fragment"].as_str()).flat_map(words).collect();
            for include in group["includes"].as_array().into_iter().flatten() {
                let Some(path) = include["path"].as_str() else { continue };
                flags.push(match (msvc, include["isSystem"].as_bool().unwrap_or(false)) {
                    (true, _) => format!("/I{path}"),
                    (false, true) => format!("-isystem{path}"),
                    (false, false) => format!("-I{path}"),
                });
            }
            for define in group["defines"].as_array().into_iter().flatten().filter_map(|one| one["define"].as_str()) {
                flags.push(if msvc { format!("/D{define}") } else { format!("-D{define}") });
            }
            if let Some(sysroot) = group["sysroot"]["path"].as_str() {
                flags.push(format!("--sysroot={sysroot}"));
            }
            for at in group["sourceIndexes"].as_array().into_iter().flatten().filter_map(Value::as_u64) {
                let Some(path) = sources.get(at as usize).and_then(|one| one["path"].as_str()) else { continue };
                let file = if Path::new(path).is_absolute() { PathBuf::from(path) } else { source.join(path) };
                let file = file.display().to_string().replace('\\', "/");
                let mut arguments = vec![compiler.replace('\\', "/")];
                arguments.extend(flags.iter().cloned());
                arguments.extend(if msvc { vec!["/c".to_string(), file.clone()] } else { vec!["-c".to_string(), file.clone()] });
                entries.push(json!({"directory": build, "file": file, "arguments": arguments}));
            }
        }
    }
    Some((source.display().to_string(), entries))
}

/// Visual Studio's environment, as its vcvars64.bat sets it, for a program that runs as cl does:
/// read once, from the newest Visual Studio with its C++ tools that vswhere finds.
pub fn vs_environment() -> Option<Vec<(String, String)>> {
    static FOUND: OnceLock<Option<Vec<(String, String)>>> = OnceLock::new();
    FOUND
        .get_or_init(|| {
            if !cfg!(windows) {
                return None;
            }
            let base = std::env::var_os("ProgramFiles(x86)")?;
            let vswhere = Path::new(&base).join("Microsoft Visual Studio").join("Installer").join("vswhere.exe");
            let mut ask = Command::new(vswhere);
            ask.args(["-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"]).stdin(Stdio::null()).stderr(Stdio::null());
            crate::runner::quiet(&mut ask);
            let place = String::from_utf8_lossy(&ask.output().ok()?.stdout).trim().to_string();
            let vcvars = Path::new(&place).join("VC").join("Auxiliary").join("Build").join("vcvars64.bat");
            if !vcvars.is_file() {
                return None;
            }
            let mut shell = Command::new("cmd");
            shell.args(["/d", "/c"]).arg(format!("call \"{}\" >nul && set", vcvars.display())).stdin(Stdio::null()).stderr(Stdio::null());
            crate::runner::quiet(&mut shell);
            let out = shell.output().ok().filter(|out| out.status.success())?;
            let found: Vec<(String, String)> = String::from_utf8_lossy(&out.stdout).lines().filter_map(|line| line.split_once('=')).map(|(key, value)| (key.to_string(), value.to_string())).collect();
            found.iter().any(|(key, _)| key.eq_ignore_ascii_case("INCLUDE")).then_some(found)
        })
        .clone()
}

#[cfg(test)]
mod tests {
    use super::*;

    fn scratch(name: &str) -> PathBuf {
        let dir = std::env::temp_dir().join(format!("orior-compile-db-{name}-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        std::fs::create_dir_all(&dir).unwrap();
        dir
    }

    #[test]
    fn a_cmake_reply_gives_each_source_the_build_s_compiler_includes_and_defines() {
        let dir = scratch("cmake");
        let build = dir.join("build/cmake/Debug");
        let reply = build.join(".cmake/api/v1/reply");
        std::fs::create_dir_all(&reply).unwrap();
        let source = dir.display().to_string().replace('\\', "/");
        let built = build.display().to_string().replace('\\', "/");
        std::fs::write(reply.join("index-2026-01-01T00-00-00-0000.json"), r#"{"reply": {"codemodel-v2": {"jsonFile": "codemodel-v2-a.json"}, "toolchains-v1": {"jsonFile": "toolchains-v1-a.json"}}}"#).unwrap();
        std::fs::write(reply.join("codemodel-v2-a.json"), json!({"paths": {"source": source, "build": built}, "configurations": [{"name": "Release", "targets": []}, {"name": "Debug", "targets": [{"name": "probe", "jsonFile": "target-probe.json"}]}]}).to_string()).unwrap();
        std::fs::write(reply.join("toolchains-v1-a.json"), r#"{"toolchains": [{"language": "C", "compiler": {"path": "/usr/bin/gcc", "id": "GNU"}}, {"language": "CXX", "compiler": {"path": "C:/VS/bin/cl.exe", "id": "MSVC"}}]}"#).unwrap();
        std::fs::write(
            reply.join("target-probe.json"),
            r#"{"sources": [{"path": "src/main.c"}, {"path": "src/more.cpp"}, {"path": "src/kernel.cu"}],
                "compileGroups": [
                  {"language": "C", "compileCommandFragments": [{"fragment": "-g -std=gnu11"}], "includes": [{"path": "/x/include"}, {"path": "/x/sys", "isSystem": true}], "defines": [{"define": "NEEDED=1"}], "sourceIndexes": [0]},
                  {"language": "CXX", "compileCommandFragments": [{"fragment": "/W3"}], "includes": [{"path": "C:/x/include"}], "defines": [{"define": "WIN"}], "sourceIndexes": [1]},
                  {"language": "CUDA", "compileCommandFragments": [], "sourceIndexes": [2]}
                ]}"#,
        )
        .unwrap();
        let looked = refresh(&dir);
        let written: Vec<Value> = serde_json::from_str(&std::fs::read_to_string(dir.join(WRITTEN)).unwrap()).unwrap();
        let file = |name: &str| format!("{source}/src/{name}");
        assert_eq!(
            written,
            vec![
                json!({"directory": built, "file": file("main.c"), "arguments": ["/usr/bin/gcc", "-g", "-std=gnu11", "-I/x/include", "-isystem/x/sys", "-DNEEDED=1", "-c", file("main.c")]}),
                json!({"directory": built, "file": file("more.cpp"), "arguments": ["C:/VS/bin/cl.exe", "/W3", "/IC:/x/include", "/DWIN", "/c", file("more.cpp")]}),
            ],
            "the Debug configuration's, CUDA's left out"
        );
        assert_eq!(looked.database.folder, Some(dir.join("build/orior")));
        assert!(looked.database.msvc);
        assert_eq!(looked.database.drivers(), vec!["/usr/bin/gcc".to_string()], "cl answers no question of its headers");
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_tree_s_own_database_is_kept_and_read_for_its_compilers() {
        let dir = scratch("own");
        std::fs::create_dir_all(dir.join("out")).unwrap();
        std::fs::write(dir.join(".clangd"), "CompileFlags:\n  CompilationDatabase: out  # the tree's own\n").unwrap();
        std::fs::write(dir.join("out/compile_commands.json"), r#"[{"directory": "/t", "file": "/t/a.c", "command": "\"/opt/cross/bin/arm-none-eabi-gcc\" -c a.c"}]"#).unwrap();
        let read = inputs(&dir);
        assert_eq!(read.own, Some(dir.join("out").join("compile_commands.json")));
        let database = gather(&dir, &read);
        assert_eq!(database, Database { folder: None, compilers: vec!["/opt/cross/bin/arm-none-eabi-gcc".into()], msvc: false });
        assert_eq!(database.words(), vec!["--query-driver=/opt/cross/bin/arm-none-eabi-gcc".to_string()]);
        assert!(!dir.join(WRITTEN).exists(), "nothing written over the tree's own");
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_command_line_s_words_keep_quoted_ones_whole() {
        assert_eq!(words("\"C:/Program Files/LLVM/bin/clang.exe\" -I \"a b\" -c x.c"), ["C:/Program Files/LLVM/bin/clang.exe", "-I", "a b", "-c", "x.c"]);
    }
}

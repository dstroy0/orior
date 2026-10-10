// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Debugging through debug adapters: lldb-dap for C, C++ and Rust, gdb where lldb-dap is not found,
//! and debugpy for Python, as each toolchain's `debugger` in toolchains.json names it. A file of a
//! compiled language is first built for debugging, by the `builds` of the toolchain that has its
//! language, into build/debug/, and the program built is what runs.
//!
//! More than one session runs at once, each by its number, each its own adapter with its own threads
//! and stack. A session starts by launching a file, by launching pytest on one test, by attaching to
//! a process by its number or to a debugger listening at an address, or as the child a session's
//! program started, which debugpy asks for by its `debugpyAttach` event. A session goes as the
//! protocol has it: the adapter is started and told who asks, the program is launched or attached
//! to, the breakpoints and the exceptions to stop on are set once the adapter says it is ready for
//! them, and the program runs. From then on the adapter's events go to the function the session was
//! started with, with the session's number, and the stack, the variables, the memory, the
//! instructions and the steps are asked of the session named.
//!
//! A breakpoint stops only where its condition holds, or at the count of hits it gives, and one with
//! a message writes the message in place of stopping. A file's breakpoints go to each session that
//! debugs its language. Stepping keeps to the tree's own code where the tree asks it to, and passes
//! over the code its patterns name: by debugpy's rules for Python, and by lldb's step-avoid pattern
//! for the rest.
//!
//! Lines here count from 0, as the editor counts them, and the adapter is told they count from 1.

use std::collections::HashMap;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::mpsc;
use std::sync::{Arc, Mutex};
use std::time::Duration;

use serde::{Deserialize, Serialize};
use serde_json::{json, Value};

use crate::dap::{self, Adapter};
use crate::toolchains;

/// A toolchain's debug adapter: its program, which is one of the toolchain's or beside them, the
/// words it starts with, the languages it debugs, the arguments of its launch request, in which
/// {program} is the program built, {path} the file's path and {root} the tree's top folder, the
/// arguments of its attach request, in which {pid} is the process's number, and the folders it needs
/// on its PATH, by system as `places` writes them, such as the Python whose library lldb-dap loads. A
/// folder that is not there is left out, and the adapter says what it misses.
#[derive(Deserialize, Serialize, Clone, Debug)]
pub struct DebuggerSpec {
    pub program: String,
    pub args: Vec<String>,
    pub languages: Vec<String>,
    pub launch: Value,
    #[serde(default)]
    pub attach: Option<Value>,
    #[serde(default)]
    pub path: HashMap<String, Vec<String>>,
    /// The systems the adapter is used on, all where none are named.
    #[serde(default)]
    pub systems: Vec<String>,
    /// The line that installs the adapter where its toolchain can be present without it.
    #[serde(default)]
    pub setup: Option<String>,
}

/// What a session says on its own: its number, an adapter's event and its body, or the session's end.
pub type Emit = Arc<dyn Fn(u64, &str, Value) + Send + Sync>;

/// A breakpoint: its line, counted from 0; the expression it stops only where it holds; the count of
/// hits it stops at, as the adapter reads one, `3` or `>= 3`; and the message it writes to the
/// console in place of stopping, `{name}` in it the value of the expression `name`.
#[derive(Deserialize, Serialize, Clone, Debug, Default, PartialEq)]
#[serde(default)]
pub struct Breakpoint {
    pub line: u32,
    pub condition: String,
    pub hits: String,
    pub log: String,
}

/// Each file's breakpoints, by its path in the tree.
pub type Breakpoints = HashMap<String, Vec<Breakpoint>>;

/// How a session starts.
#[derive(Deserialize, Clone, Debug)]
#[serde(tag = "kind", rename_all = "lowercase")]
pub enum Start {
    /// The file at `path` of the tree, in `language`, built where its language is built.
    File { path: String, language: String },
    /// One Python test, named as pytest names it, run by pytest.
    Test { id: String },
    /// The process numbered `pid`, its program in `language`; `parent` is the session that started
    /// it, where this one debugs the same process's native code.
    Process {
        pid: u32,
        language: String,
        #[serde(default)]
        parent: Option<u64>,
    },
    /// A Python program whose debugpy listens at `host` and `port`, as a container's does.
    /// `remote` is the folder the tree's files are at where the program runs, where that is not the
    /// tree itself, as in a container.
    Address {
        host: String,
        port: u16,
        #[serde(default)]
        remote: Option<String>,
    },
    /// The child a session's program started, as debugpy's `debugpyAttach` event gives it.
    Child { parent: u64, config: Value },
    /// The Python file at `path` of the tree, its run recorded and stepped through backward as well
    /// as forward.
    Record { path: String },
}

/// What the tree asks of stepping: whether a step keeps to the tree's own code, and the patterns
/// of the code a step passes over, a path's glob or a module's name for Python and a function's
/// pattern for the rest.
#[derive(Deserialize, Serialize, Clone, Debug, PartialEq)]
#[serde(default)]
pub struct Stepping {
    pub mine_only: bool,
    pub skip: Vec<String>,
}

/// A step keeps to the tree's own code until the tree says otherwise, as debugpy's own default does.
impl Default for Stepping {
    fn default() -> Stepping {
        Stepping { mine_only: true, skip: Vec::new() }
    }
}

/// What a session starts with: the breakpoints, the exceptions to stop on by the adapter's names for
/// them, and the stepping the tree asks for.
#[derive(Deserialize, Clone, Debug, Default)]
#[serde(default)]
pub struct Options {
    pub breakpoints: Breakpoints,
    pub exceptions: Vec<String>,
    pub stepping: Stepping,
    /// Whether a program launched stops before its first line, for a native session to attach to
    /// it first.
    pub entry: bool,
}

/// An exception the adapter can stop on: its name to the adapter, its label, and whether it is on.
#[derive(Serialize, Clone, Debug, PartialEq)]
pub struct Filter {
    pub filter: String,
    pub label: String,
    pub on: bool,
}

/// A session as the window shows it: its number, its name, the toolchain debugging it, its language,
/// the session that started it, the exceptions it can stop on, what its adapter can do, and the
/// number of the process it debugs once the adapter says it.
#[derive(Serialize, Clone, Debug)]
pub struct SessionInfo {
    pub id: u64,
    pub name: String,
    pub tool: String,
    pub language: String,
    pub parent: Option<u64>,
    pub filters: Vec<Filter>,
    pub conditions: bool,
    pub hits: bool,
    pub logs: bool,
    pub memory: bool,
    pub disassembly: bool,
    pub data: bool,
    pub exception_info: bool,
    pub process: Option<u32>,
    /// Whether the session steps backward, and whether it is a recorded run, whose names' values
    /// can be read through the run.
    pub step_back: bool,
    pub recorded: bool,
}

/// A value a name took in a recorded run: the step of the line that gave it, that line, counted
/// from 0, its file, the value, none where the name went away, and whether the step comes before
/// the one stood at.
#[derive(Serialize, Clone, Debug)]
pub struct Change {
    pub step: u64,
    pub line: u32,
    pub path: String,
    pub value: Option<String>,
    pub before: bool,
}

#[derive(Serialize, Clone, Debug)]
pub struct Frame {
    pub id: i64,
    pub name: String,
    pub path: Option<String>,
    pub line: u32,
    pub col: u32,
    /// Where the frame's instruction stands in memory, for its instructions to be read.
    pub ip: Option<String>,
}

#[derive(Serialize, Clone, Debug)]
pub struct Scope {
    pub name: String,
    pub reference: i64,
    pub expensive: bool,
}

#[derive(Serialize, Clone, Debug)]
pub struct Variable {
    pub name: String,
    pub value: String,
    pub kind: String,
    pub reference: i64,
    /// Where the variable stands in memory, where the adapter says.
    pub memory: Option<String>,
}

#[derive(Serialize, Clone, Debug)]
pub struct Thread {
    pub id: i64,
    pub name: String,
}

/// The exception a thread stopped on: its name, what it says, and its stack as the adapter writes it.
#[derive(Serialize, Clone, Debug)]
pub struct Raised {
    pub id: String,
    pub description: String,
    pub stack: String,
}

/// Bytes read at an address: the address of the first, the bytes, and how many after them could
/// not be read.
#[derive(Serialize, Clone, Debug, PartialEq)]
pub struct Memory {
    pub address: String,
    pub bytes: Vec<u8>,
    pub unreadable: u64,
}

/// An instruction: its address, its text, its bytes, the symbol it stands in, and the line of
/// source it is of, counted from 0, where the adapter says.
#[derive(Serialize, Clone, Debug)]
pub struct Instruction {
    pub address: String,
    pub text: String,
    pub bytes: String,
    pub symbol: String,
    pub line: Option<u32>,
}

/// How long the adapter has to start, to be ready for breakpoints, and to launch the program.
const STARTING: Duration = Duration::from_secs(30);
/// How long a request about the stopped program may take.
const ASKING: Duration = Duration::from_secs(10);
/// How long after it is initialized an adapter has to say it is ready before the launch, as gdb does.
const EARLY: Duration = Duration::from_millis(150);
/// How long a build for debugging may take.
const BUILDING: Duration = Duration::from_secs(300);

struct Session {
    info: Mutex<SessionInfo>,
    adapter: Arc<Adapter>,
    /// The adapter's program, as the manifest names it.
    program: String,
    /// The data breakpoints set, as the adapter takes them, set again whole with each one added.
    data: Mutex<Vec<Value>>,
    /// The numbers of the breakpoints the reader placed, and whether a data breakpoint is set
    /// through lldb's own watchpoints, whose stops come at breakpoints of no such number.
    marks: Arc<Marks>,
}

#[derive(Default)]
struct Marks {
    placed: Mutex<std::collections::HashSet<i64>>,
    watching: AtomicBool,
    scripted: AtomicBool,
    /// The native session whose breakpoints of a step across are taken away at the next stop.
    across: Mutex<Option<std::sync::Weak<Adapter>>>,
    /// The number of the process the adapter says it debugs, where it says so before the session
    /// starts.
    pid: AtomicU64,
}

/// The name of the breakpoints a step from Python into C sets on the tree's native code.
const ACROSS: &str = "orior_across";

/// Runs `text` as a command of lldb's own in lldb session `session`, and gives what it says; lldb
/// says a command failed in the answer's text.
fn lldb_command(session: &Session, text: &str) -> Result<String, String> {
    let said = session.adapter.request("evaluate", json!({"expression": format!("`{text}"), "context": "repl"}), ASKING)?;
    let said = said["result"].as_str().unwrap_or_default().to_string();
    if said.trim_start().starts_with("error:") {
        return Err(said.trim().to_string());
    }
    Ok(said)
}

/// Loads orior's script into lldb session `session`, once.
fn lldb_script(session: &Session) -> Result<(), String> {
    if session.marks.scripted.load(Ordering::SeqCst) {
        return Ok(());
    }
    let script = std::env::temp_dir().join("orior-lldb").join("orior_lldb.py");
    std::fs::create_dir_all(script.parent().unwrap_or(Path::new("."))).map_err(|error| error.to_string())?;
    std::fs::write(&script, LLDB_SCRIPT).map_err(|error| error.to_string())?;
    lldb_command(session, &format!("command script import \"{}\"", script.display().to_string().replace('\\', "/")))?;
    session.marks.scripted.store(true, Ordering::SeqCst);
    Ok(())
}

#[derive(Default)]
pub struct Debugger {
    sessions: Mutex<Vec<Arc<Session>>>,
    next: AtomicU64,
}

/// The adapter for `language` that is found, its toolchain's name, and its program's path: the
/// adapter of the first toolchain that has one for it on this system and is found, and that
/// toolchain's id, whose compiler then builds the program so that its debugger reads what it wrote.
fn adapter_for(language: &str) -> Result<(DebuggerSpec, String, String, PathBuf), String> {
    let tools = toolchains::manifest();
    let system = toolchains::system();
    let named: Vec<&toolchains::Tool> = tools
        .iter()
        .filter(|tool| tool.debugger.as_ref().is_some_and(|spec| spec.languages.iter().any(|one| one == language) && (spec.systems.is_empty() || spec.systems.iter().any(|one| one == system))))
        .collect();
    if named.is_empty() {
        return Err(format!("orior has no debugger for {language}"));
    }
    let path_folders = toolchains::path_folders();
    let kept = toolchains::chosen();
    for tool in &named {
        let spec = tool.debugger.clone().unwrap_or_else(|| unreachable!("filtered on its debugger"));
        let found = toolchains::find(tool, &path_folders, &kept);
        if let Some(program) = found.folder.as_deref().and_then(|folder| toolchains::program_in(Path::new(folder), std::slice::from_ref(&spec.program))) {
            return Ok((spec, tool.name.clone(), tool.id.clone(), program));
        }
    }
    let names: Vec<&str> = named.iter().map(|tool| tool.name.as_str()).collect();
    Err(format!("{} debugs it, and is not found: File, Toolchains opens its install page or takes its folder", names.join(" or ")))
}

/// Builds the file at `path` for debugging where its language is built, with the toolchain `ahead`
/// names where it builds it and the first found where not, and gives the program built, or None for
/// a language that runs from its source.
fn build(root: &Path, path: &Path, language: &str, ahead: &str) -> Result<Option<PathBuf>, String> {
    let ext = path.extension().map(|ext| ext.to_string_lossy().to_lowercase()).unwrap_or_default();
    let keys = [format!("{language}.{ext}"), language.to_string()];
    let tools = toolchains::manifest();
    let mut named: Vec<(&toolchains::Tool, &Vec<String>)> = keys.iter().flat_map(|key| tools.iter().filter_map(move |tool| tool.builds.get(key).map(|words| (tool, words)))).collect();
    named.sort_by_key(|(tool, _)| tool.id != ahead);
    if named.is_empty() {
        return Ok(None);
    }
    let path_folders = toolchains::path_folders();
    let kept = toolchains::chosen();
    let Some((tool, words, program)) = named.iter().find_map(|(tool, words)| toolchains::find(tool, &path_folders, &kept).program.map(|program| (*tool, *words, PathBuf::from(program)))) else {
        let names: Vec<&str> = named.iter().map(|(tool, _)| tool.name.as_str()).collect();
        return Err(format!("{} builds it, and is not found: File, Toolchains opens its install page or takes its folder", names.join(" or ")));
    };
    let folder = program.parent().map(Path::to_path_buf).unwrap_or_default();
    let stem = path.file_stem().map(|stem| stem.to_string_lossy().to_string()).unwrap_or_default();
    let out_folder = root.join("build").join("debug");
    std::fs::create_dir_all(&out_folder).map_err(|error| format!("build/debug: {error}"))?;
    let mut out = out_folder.join(&stem);
    if cfg!(windows) {
        out.set_extension("exe");
    }
    let mut filled = Vec::new();
    for word in words {
        filled.push(match word.as_str() {
            "{path}" => path.display().to_string(),
            "{out}" => out.display().to_string(),
            "{program}" => program.display().to_string(),
            other if other.starts_with('{') && other.ends_with('}') => {
                let name = &other[1..other.len() - 1];
                toolchains::program_in(&folder, &[name.to_string()]).ok_or_else(|| format!("{} has no {name} in {}", tool.name, folder.display()))?.display().to_string()
            }
            other => other.to_string(),
        });
    }
    let mut command = Command::new(&filled[0]);
    command.args(&filled[1..]).current_dir(root).env("PATH", toolchains::run_path());
    crate::runner::quiet(&mut command);
    let child = command.stdout(std::process::Stdio::piped()).stderr(std::process::Stdio::piped()).spawn().map_err(|error| format!("{}: {error}", filled[0]))?;
    let (sender, done) = mpsc::channel();
    std::thread::spawn(move || {
        let _ = sender.send(child.wait_with_output());
    });
    let output = done.recv_timeout(BUILDING).map_err(|_| format!("the build took longer than {} s", BUILDING.as_secs()))?.map_err(|error| error.to_string())?;
    if !output.status.success() {
        let said = format!("{}{}", String::from_utf8_lossy(&output.stderr), String::from_utf8_lossy(&output.stdout));
        return Err(format!("the build for debugging failed:\n{}", said.trim()));
    }
    Ok(Some(out))
}

/// The launch arguments with their names filled in.
fn filled(value: &Value, names: &[(&str, String)]) -> Value {
    match value {
        Value::String(text) => {
            let mut out = text.clone();
            for (name, with) in names {
                out = out.replace(&format!("{{{name}}}"), with);
            }
            Value::String(out)
        }
        Value::Array(all) => Value::Array(all.iter().map(|one| filled(one, names)).collect()),
        Value::Object(all) => Value::Object(all.iter().map(|(key, one)| (key.clone(), filled(one, names))).collect()),
        other => other.clone(),
    }
}

/// `value` with each string of digits alone made a number, as a process's number is taken.
fn numbers(value: Value) -> Value {
    match value {
        Value::String(text) if !text.is_empty() && text.chars().all(|char| char.is_ascii_digit()) => text.parse::<u64>().map_or(Value::String(text), Value::from),
        Value::Array(all) => Value::Array(all.into_iter().map(numbers).collect()),
        Value::Object(all) => Value::Object(all.into_iter().map(|(key, one)| (key, numbers(one))).collect()),
        other => other,
    }
}

fn frame_of(one: &Value) -> Frame {
    Frame {
        id: one["id"].as_i64().unwrap_or(0),
        name: one["name"].as_str().unwrap_or_default().to_string(),
        path: one["source"]["path"].as_str().map(str::to_string),
        line: one["line"].as_u64().unwrap_or(1).saturating_sub(1) as u32,
        col: one["column"].as_u64().unwrap_or(1).saturating_sub(1) as u32,
        ip: one["instructionPointerReference"].as_str().map(str::to_string),
    }
}

fn variable_of(one: &Value) -> Variable {
    Variable {
        name: one["name"].as_str().unwrap_or_default().to_string(),
        value: one["value"].as_str().or_else(|| one["result"].as_str()).unwrap_or_default().to_string(),
        kind: one["type"].as_str().unwrap_or_default().to_string(),
        reference: one["variablesReference"].as_i64().unwrap_or(0),
        memory: one["memoryReference"].as_str().map(str::to_string),
    }
}

/// Whether a session in `language` debugs the file at `file`: a Python session its `.py` files, any
/// other session the rest.
fn takes(language: &str, file: &str) -> bool {
    let python = file.to_lowercase().ends_with(".py") || file.to_lowercase().ends_with(".pyw");
    (language == "python") == python
}

/// The breakpoints of a request to set them, as the protocol writes them.
fn breakpoints_of(breakpoints: &[Breakpoint]) -> Vec<Value> {
    breakpoints
        .iter()
        .map(|one| {
            let mut value = json!({"line": one.line + 1});
            for (key, text) in [("condition", &one.condition), ("hitCondition", &one.hits), ("logMessage", &one.log)] {
                if !text.trim().is_empty() {
                    value[key] = json!(text.trim());
                }
            }
            value
        })
        .collect()
}

/// Bytes from base64, as the protocol sends memory.
fn base64(text: &str) -> Vec<u8> {
    let value = |char: u8| match char {
        b'A'..=b'Z' => Some(char - b'A'),
        b'a'..=b'z' => Some(char - b'a' + 26),
        b'0'..=b'9' => Some(char - b'0' + 52),
        b'+' | b'-' => Some(62),
        b'/' | b'_' => Some(63),
        _ => None,
    };
    let mut out = Vec::new();
    let mut held = 0u32;
    let mut bits = 0;
    for char in text.bytes().filter_map(value) {
        held = (held << 6) | u32::from(char);
        bits += 6;
        if bits >= 8 {
            bits -= 8;
            out.push((held >> bits) as u8);
            held &= (1 << bits) - 1;
        }
    }
    out
}

/// The arguments that set stepping for a session in `language`: debugpy's rules and its own-code
/// switch for Python, and lldb's step-avoid pattern for the rest.
fn step_arguments(language: &str, stepping: &Stepping, arguments: &mut Value) {
    let Some(map) = arguments.as_object_mut() else {
        return;
    };
    if language == "python" {
        map.insert("justMyCode".into(), json!(stepping.mine_only));
        let rules: Vec<Value> = stepping
            .skip
            .iter()
            .filter(|one| !one.trim().is_empty())
            .map(|one| if one.contains(['/', '\\', '*', '.']) && !one.chars().all(|char| char.is_alphanumeric() || char == '_' || char == '.') { json!({"path": one.trim(), "include": false}) } else { json!({"module": one.trim(), "include": false}) })
            .collect();
        if !rules.is_empty() {
            map.insert("rules".into(), json!(rules));
        }
    } else {
        let patterns: Vec<&str> = stepping.skip.iter().map(|one| one.trim()).filter(|one| !one.is_empty()).collect();
        if !patterns.is_empty() {
            let mut commands = map.get("initCommands").and_then(Value::as_array).cloned().unwrap_or_default();
            commands.push(json!(format!("settings set target.process.thread.step-avoid-regexp {}", patterns.join("|"))));
            map.insert("initCommands".into(), json!(commands));
        }
    }
}

impl Debugger {
    fn session(&self, id: u64) -> Result<Arc<Session>, String> {
        self.sessions.lock().map_err(|_| "held".to_string())?.iter().find(|one| one.info.lock().is_ok_and(|info| info.id == id)).cloned().ok_or_else(|| "that session has ended".to_string())
    }

    fn adapter(&self, id: u64) -> Result<Arc<Adapter>, String> {
        self.session(id).map(|session| session.adapter.clone())
    }

    /// The sessions running, the first started first.
    pub fn sessions(&self) -> Vec<SessionInfo> {
        self.sessions.lock().map(|all| all.iter().filter_map(|one| one.info.lock().ok().map(|info| info.clone())).collect()).unwrap_or_default()
    }

    pub fn running(&self) -> bool {
        self.sessions.lock().map(|all| !all.is_empty()).unwrap_or(false)
    }

    /// Starts a session as `start` says, in the tree at `root`, with `options`. Says what the session
    /// is, its number first.
    pub fn start(&self, root: &Path, start: &Start, options: &Options, emit: Emit) -> Result<SessionInfo, String> {
        let parent_language = match start {
            Start::Child { parent, .. } => Some(self.session(*parent)?.info.lock().map_err(|_| "held".to_string())?.language.clone()),
            _ => None,
        };
        let language = match start {
            Start::File { language, .. } | Start::Process { language, .. } => language.clone(),
            Start::Test { .. } | Start::Address { .. } | Start::Record { .. } => "python".to_string(),
            Start::Child { .. } => parent_language.clone().unwrap_or_else(|| "python".to_string()),
        };
        let recording = matches!(start, Start::Record { .. });
        let (spec, tool, tool_id, adapter_program) = if recording {
            let spec = DebuggerSpec { program: "orior".into(), args: Vec::new(), languages: vec![language.clone()], launch: json!({}), attach: None, path: HashMap::new(), systems: Vec::new(), setup: None };
            (spec, "orior's recorder".to_string(), String::new(), PathBuf::new())
        } else {
            adapter_for(&language)?
        };
        let (request, mut arguments, name, parent) = match start {
            Start::File { path, .. } => {
                let file = crate::root::full(root, path);
                let program = build(root, &file, &language, &tool_id)?;
                let names = [
                    ("program", program.as_ref().map_or_else(|| file.display().to_string(), |out| out.display().to_string())),
                    ("path", file.display().to_string()),
                    ("root", root.display().to_string()),
                ];
                ("launch", filled(&spec.launch, &names), path.clone(), None)
            }
            Start::Test { id } => {
                let names = [("program", String::new()), ("path", String::new()), ("root", root.display().to_string())];
                let mut launch = filled(&spec.launch, &names);
                // A test is launched as pytest running it, in place of a program.
                if let Some(map) = launch.as_object_mut() {
                    map.remove("program");
                    map.insert("module".into(), json!("pytest"));
                    map.insert("args".into(), json!([id, "-p", "no:cacheprovider", "-q"]));
                    if let Some(python) = crate::test_runs::python() {
                        map.insert("python".into(), json!(python.display().to_string()));
                    }
                }
                ("launch", launch, id.clone(), None)
            }
            Start::Process { pid, parent, .. } => {
                let template = spec.attach.clone().unwrap_or_else(|| json!({"pid": "{pid}"}));
                let named = if parent.is_some() { format!("the native code of process {pid}") } else { format!("process {pid}") };
                ("attach", numbers(filled(&template, &[("pid", pid.to_string()), ("root", root.display().to_string())])), named, *parent)
            }
            Start::Address { host, port, remote } => {
                let mut asked = json!({"connect": {"host": host, "port": port}});
                if let Some(remote) = remote.as_deref().filter(|remote| !remote.is_empty()) {
                    asked["pathMappings"] = json!([{"localRoot": root.display().to_string(), "remoteRoot": remote}]);
                }
                ("attach", asked, format!("{host}:{port}"), None)
            }
            Start::Child { parent, config } => {
                let named = config["name"].as_str().map(str::to_string).or_else(|| config["subProcessId"].as_u64().map(|pid| format!("process {pid}"))).unwrap_or_else(|| "child".to_string());
                ("attach", config.clone(), named, Some(*parent))
            }
            Start::Record { path } => {
                let python = crate::test_runs::python().map(|python| python.display().to_string()).unwrap_or_default();
                ("launch", json!({"program": crate::root::full(root, path).display().to_string(), "python": python}), path.clone(), None)
            }
        };
        if request == "launch" && language == "python" && !recording {
            if let Some(map) = arguments.as_object_mut() {
                // The processes a Python program starts are debugged with it.
                map.insert("subProcess".into(), json!(true));
            }
        }
        if !matches!(start, Start::Child { .. }) {
            step_arguments(&language, &options.stepping, &mut arguments);
        }
        if options.entry && request == "launch" {
            arguments["stopOnEntry"] = json!(true);
        }
        let id = self.next.fetch_add(1, Ordering::SeqCst) + 1;
        let (ready, readied) = mpsc::channel();
        let ready = Mutex::new(Some(ready));
        let told = emit.clone();
        let marks = Arc::new(Marks::default());
        let marked = marks.clone();
        let heard: dap::Heard = Box::new(move |event, body| {
            if event == "initialized" {
                if let Some(ready) = ready.lock().ok().and_then(|mut ready| ready.take()) {
                    let _ = ready.send(());
                }
            }
            if event == "process" {
                if let Some(pid) = body["systemProcessId"].as_u64() {
                    marked.pid.store(pid, Ordering::SeqCst);
                }
            }
            if event == "stopped" {
                // A stop ends a step across, and its breakpoints go.
                if let Some(native) = marked.across.lock().ok().and_then(|mut across| across.take()).and_then(|weak| weak.upgrade()) {
                    std::thread::spawn(move || {
                        let _ = native.request("evaluate", json!({"expression": format!("`breakpoint delete --force {ACROSS}"), "context": "repl"}), ASKING);
                    });
                }
            }
            told(id, event, data_stop(&marked, event, body));
        });
        // A program listening at an address, and a process a debugged program started, are reached
        // through the adapter already listening for them, at the address it gives.
        let listening = match start {
            Start::Address { host, port, .. } => Some((host.clone(), *port, false)),
            Start::Child { config, .. } => config["connect"]["port"].as_u64().map(|port| (config["connect"]["host"].as_str().unwrap_or("127.0.0.1").to_string(), port as u16, true)),
            _ => None,
        };
        let reached = listening.is_some();
        let adapter = Arc::new(match listening {
            _ if recording => Adapter::within(crate::recording::serve(root.to_path_buf()), heard),
            Some((host, port, ends_program)) => Adapter::connect(&host, port, ends_program, heard)?,
            None => Adapter::start(&adapter_program, &spec.args, root, toolchains::run_path_with(&toolchains::folders_for(&spec.path)), request == "launch", heard)?,
        });
        let failed = |said: String| {
            let complaint = adapter.complaint();
            adapter.stop();
            let said = if complaint.is_empty() { said } else { format!("{said}: {complaint}") };
            match &spec.setup {
                Some(setup) => format!("{said}\n{setup} installs {}", spec.program),
                None => said,
            }
        };
        let initialize = json!({
            "clientID": "orior", "clientName": "orior", "adapterID": spec.program, "locale": "en",
            "linesStartAt1": true, "columnsStartAt1": true, "pathFormat": "path",
            "supportsVariableType": true, "supportsRunInTerminalRequest": false, "supportsMemoryReferences": true,
            "supportsStartDebuggingRequest": false
        });
        // An adapter already listening answers at once; one that took the connection and is silent
        // is serving another client.
        let capabilities = adapter
            .request("initialize", initialize, if reached { ASKING } else { STARTING })
            .map_err(|said| if reached { format!("{name} took the connection and did not answer; it may be serving another debugger") } else { said })
            .map_err(&failed)?;
        let filters: Vec<Filter> = capabilities["exceptionBreakpointFilters"]
            .as_array()
            .into_iter()
            .flatten()
            .filter_map(|one| {
                let filter = one["filter"].as_str()?.to_string();
                Some(Filter { on: options.exceptions.contains(&filter), label: one["label"].as_str().unwrap_or(&filter).to_string(), filter })
            })
            .collect();
        let can = |key: &str| capabilities[key].as_bool().unwrap_or(false);
        let info = SessionInfo {
            id,
            name,
            tool: tool.clone(),
            language: language.clone(),
            parent,
            filters: filters.clone(),
            conditions: can("supportsConditionalBreakpoints"),
            hits: can("supportsHitConditionalBreakpoints"),
            logs: can("supportsLogPoints"),
            memory: can("supportsReadMemoryRequest"),
            disassembly: can("supportsDisassembleRequest"),
            data: can("supportsDataBreakpoints"),
            exception_info: can("supportsExceptionInfoRequest"),
            process: match start {
                Start::Process { pid, .. } => Some(*pid),
                _ => None,
            },
            step_back: can("supportsStepBack"),
            recorded: recording,
        };
        let configure = || {
            for (file, breakpoints) in &options.breakpoints {
                if takes(&language, file) {
                    let _ = Self::set(&adapter, &marks, &crate::root::full(root, file), breakpoints);
                }
            }
            let on: Vec<&str> = filters.iter().filter(|one| one.on).map(|one| one.filter.as_str()).collect();
            let _ = adapter.request("setExceptionBreakpoints", json!({"filters": on}), ASKING);
        };
        // An adapter that says it is ready before the launch, as gdb does, runs the program as the
        // launch comes: its breakpoints are set first, each bound as the program loads. The others
        // say it once the launch is under way, and are set then.
        let early = readied.recv_timeout(EARLY).is_ok();
        if early {
            configure();
        }
        let requested = adapter.send(request, arguments).map_err(&failed)?;
        if !early {
            if readied.recv_timeout(STARTING).is_err() {
                if let Ok(Err(said)) = requested.try_recv() {
                    return Err(failed(said));
                }
                return Err(failed(format!("{} did not say it was ready", spec.program)));
            }
            configure();
        }
        adapter.request("configurationDone", Value::Null, ASKING).map_err(&failed)?;
        Adapter::wait(request, &requested, STARTING).map_err(&failed)?;
        let mut info = info;
        if info.process.is_none() {
            info.process = Some(marks.pid.load(Ordering::SeqCst) as u32).filter(|&pid| pid != 0);
        }
        let session = Arc::new(Session { info: Mutex::new(info.clone()), adapter, program: spec.program.clone(), data: Mutex::new(Vec::new()), marks });
        self.sessions.lock().map_err(|_| "held".to_string())?.push(session);
        Ok(info)
    }

    fn set(adapter: &Adapter, marks: &Marks, file: &Path, breakpoints: &[Breakpoint]) -> Result<Vec<(u32, bool)>, String> {
        let said = adapter.request("setBreakpoints", json!({"source": {"path": file.display().to_string()}, "breakpoints": breakpoints_of(breakpoints)}), ASKING)?;
        if let Ok(mut placed) = marks.placed.lock() {
            placed.extend(said["breakpoints"].as_array().into_iter().flatten().filter_map(|one| one["id"].as_i64()));
        }
        Ok(said["breakpoints"]
            .as_array()
            .map(|all| all.iter().zip(breakpoints).map(|(one, given)| (one["line"].as_u64().map_or(given.line, |at| at.saturating_sub(1) as u32), one["verified"].as_bool().unwrap_or(false))).collect())
            .unwrap_or_default())
    }

    /// Sets the breakpoints of `file`, at `path` in the tree, in each session that debugs its
    /// language, and gives each line the first placed one on and whether it is bound to code; None
    /// where no session debugs it.
    pub fn breakpoints(&self, root: &Path, path: &str, breakpoints: &[Breakpoint]) -> Option<Vec<(u32, bool)>> {
        let sessions: Vec<Arc<Session>> = self.sessions.lock().ok()?.clone();
        let mut placed = None;
        for session in sessions {
            let language = session.info.lock().ok()?.language.clone();
            if takes(&language, path) {
                if let Ok(lines) = Self::set(&session.adapter, &session.marks, &crate::root::full(root, path), breakpoints) {
                    placed.get_or_insert(lines);
                }
            }
        }
        placed
    }

    /// Sets the exceptions session `id` stops on, by the adapter's names for them.
    pub fn exceptions(&self, id: u64, on: &[String]) -> Result<(), String> {
        let session = self.session(id)?;
        session.adapter.request("setExceptionBreakpoints", json!({"filters": on}), ASKING)?;
        if let Ok(mut info) = session.info.lock() {
            for filter in &mut info.filters {
                filter.on = on.contains(&filter.filter);
            }
        }
        Ok(())
    }

    /// The exception thread `thread` of session `id` stopped on.
    pub fn raised(&self, id: u64, thread: i64) -> Result<Raised, String> {
        let said = self.adapter(id)?.request("exceptionInfo", json!({"threadId": thread}), ASKING)?;
        Ok(Raised {
            id: said["exceptionId"].as_str().unwrap_or_default().to_string(),
            description: said["description"].as_str().unwrap_or_default().to_string(),
            stack: said["details"]["stackTrace"].as_str().unwrap_or_default().to_string(),
        })
    }

    /// Keeps the number of the process session `id` debugs, as the adapter's `process` event says it.
    pub fn process(&self, id: u64, pid: u32) {
        if let Ok(session) = self.session(id) {
            if let Ok(mut info) = session.info.lock() {
                info.process = Some(pid);
            }
        }
    }

    pub fn threads(&self, id: u64) -> Result<Vec<Thread>, String> {
        let said = self.adapter(id)?.request("threads", Value::Null, ASKING)?;
        Ok(said["threads"].as_array().map(|all| all.iter().map(|one| Thread { id: one["id"].as_i64().unwrap_or(0), name: one["name"].as_str().unwrap_or_default().to_string() }).collect()).unwrap_or_default())
    }

    pub fn stack(&self, id: u64, thread: i64) -> Result<Vec<Frame>, String> {
        let said = self.adapter(id)?.request("stackTrace", json!({"threadId": thread, "startFrame": 0, "levels": 200}), ASKING)?;
        Ok(said["stackFrames"].as_array().map(|all| all.iter().map(frame_of).collect()).unwrap_or_default())
    }

    pub fn scopes(&self, id: u64, frame: i64) -> Result<Vec<Scope>, String> {
        let said = self.adapter(id)?.request("scopes", json!({"frameId": frame}), ASKING)?;
        Ok(said["scopes"]
            .as_array()
            .map(|all| {
                all.iter()
                    .map(|one| Scope { name: one["name"].as_str().unwrap_or_default().to_string(), reference: one["variablesReference"].as_i64().unwrap_or(0), expensive: one["expensive"].as_bool().unwrap_or(false) })
                    .collect()
            })
            .unwrap_or_default())
    }

    pub fn variables(&self, id: u64, reference: i64) -> Result<Vec<Variable>, String> {
        let said = self.adapter(id)?.request("variables", json!({"variablesReference": reference}), ASKING)?;
        Ok(said["variables"].as_array().map(|all| all.iter().map(variable_of).collect()).unwrap_or_default())
    }

    /// What `expression` comes to in `frame` of session `id`, as a watch asks it, as the console does
    /// with `context` "repl", or as the pointer resting on a name does with "hover".
    pub fn evaluate(&self, id: u64, expression: &str, frame: Option<i64>, context: &str) -> Result<Variable, String> {
        // With no frame chosen the frame is left out, which an adapter takes as its global scope.
        let mut arguments = json!({"expression": expression, "context": context});
        if let Some(frame) = frame {
            arguments["frameId"] = json!(frame);
        }
        let said = self.adapter(id)?.request("evaluate", arguments, ASKING)?;
        let mut found = variable_of(&said);
        found.name = expression.to_string();
        Ok(found)
    }

    /// Continue, next, stepIn, stepOut or pause, for `thread` of session `id`.
    pub fn step(&self, id: u64, how: &str, thread: i64) -> Result<(), String> {
        if !["continue", "next", "stepIn", "stepOut", "pause", "stepBack", "reverseContinue"].contains(&how) {
            return Err(format!("{how} is not a step"));
        }
        // A step out of the tree's C into the Python that called it lets the native code run on and
        // stops the Python at the next line it runs.
        if how == "stepOut" {
            let parent = self.session(id)?.info.lock().map_err(|_| "held".to_string())?.parent;
            if let Some(python) = parent.filter(|&parent| self.native_of(parent).is_some_and(|native| native.info.lock().is_ok_and(|info| info.id == id))) {
                let frames = self.stack(id, thread)?;
                // lldb names a frame with no source by its module and symbol, which is no file.
                if frames.get(1).is_none_or(|caller| caller.path.as_deref().is_none_or(|path| !Path::new(path).is_file())) {
                    self.adapter(id)?.request("continue", json!({"threadId": thread}), ASKING)?;
                    // debugpy pauses every thread, whichever is named.
                    return self.session(python)?.adapter.send("pause", json!({"threadId": thread})).map(|_| ());
                }
            }
        }
        // The C a Python step runs can stop the whole process at a native breakpoint before the
        // Python's debugger, which answers from inside the process, says the step began; in a
        // session whose process a native session also debugs, the step is sent and not waited on.
        if how != "pause" && self.native_of(id).is_some() {
            return self.adapter(id)?.send(how, json!({"threadId": thread})).map(|_| ());
        }
        self.adapter(id)?.request(how, json!({"threadId": thread}), ASKING).map(|_| ())
    }

    /// The native session debugging the same process as Python session `id`, where there is one.
    fn native_of(&self, id: u64) -> Option<Arc<Session>> {
        let all = self.sessions.lock().ok()?;
        all.iter().find(|one| one.info.lock().is_ok_and(|info| info.parent == Some(id) && info.language != "python" && info.process.is_some())).cloned()
    }

    /// Steps Python session `id`'s thread `thread` into the next line it runs, or into the tree's own
    /// C where the line calls it: every line of the tree's own files in the native modules of the
    /// tree at `root` the process loaded gets a breakpoint of one stop, and the first stop, in either
    /// session, takes them away.
    pub fn step_across(&self, root: &Path, id: u64, thread: i64) -> Result<(), String> {
        let python = self.session(id)?;
        let Some(native) = self.native_of(id) else {
            return self.step(id, "stepIn", thread);
        };
        lldb_script(&native)?;
        let root = root.display().to_string().replace('\\', "/");
        let said = lldb_command(&native, &format!("script print(orior_lldb.across(lldb.debugger.GetSelectedTarget(), {root:?}, {ACROSS:?}))"))?;
        if said.trim().parse::<u64>().unwrap_or(0) > 0 {
            let weak = Arc::downgrade(&native.adapter);
            for marks in [&python.marks, &native.marks] {
                if let Ok(mut across) = marks.across.lock() {
                    *across = Some(weak.clone());
                }
            }
        }
        self.step(id, "stepIn", thread)
    }

    /// The values `name` of frame `frame` took through recorded session `id`'s run, each where the
    /// line that gave it ran.
    pub fn history(&self, id: u64, frame: i64, name: &str) -> Result<Vec<Change>, String> {
        let said = self.adapter(id)?.request("orior/history", json!({"frameId": frame, "name": name}), ASKING)?;
        Ok(said["changes"]
            .as_array()
            .into_iter()
            .flatten()
            .map(|one| Change {
                step: one["step"].as_u64().unwrap_or(0),
                line: one["line"].as_u64().unwrap_or(1).saturating_sub(1) as u32,
                path: one["path"].as_str().unwrap_or_default().to_string(),
                value: one["value"].as_str().map(str::to_string),
                before: one["before"].as_bool().unwrap_or(false),
            })
            .collect())
    }

    /// Goes to step `step` of recorded session `id`'s run.
    pub fn goto(&self, id: u64, step: u64) -> Result<(), String> {
        self.adapter(id)?.request("orior/goto", json!({"step": step}), ASKING).map(|_| ())
    }

    /// `count` bytes from `offset` past the address `reference` names, in session `id`.
    pub fn memory(&self, id: u64, reference: &str, offset: i64, count: u64) -> Result<Memory, String> {
        let said = self.adapter(id)?.request("readMemory", json!({"memoryReference": reference, "offset": offset, "count": count}), ASKING)?;
        Ok(Memory {
            address: said["address"].as_str().unwrap_or(reference).to_string(),
            bytes: said["data"].as_str().map(base64).unwrap_or_default(),
            unreadable: said["unreadableBytes"].as_u64().unwrap_or(0),
        })
    }

    /// `count` instructions from `offset` instructions past the address `reference` names, in
    /// session `id`, each with the line of source it is of.
    pub fn instructions(&self, id: u64, reference: &str, offset: i64, count: i64) -> Result<Vec<Instruction>, String> {
        let said = self.adapter(id)?.request("disassemble", json!({"memoryReference": reference, "instructionOffset": offset, "instructionCount": count, "resolveSymbols": true}), ASKING)?;
        Ok(said["instructions"]
            .as_array()
            .into_iter()
            .flatten()
            .map(|one| Instruction {
                address: one["address"].as_str().unwrap_or_default().to_string(),
                text: one["instruction"].as_str().unwrap_or_default().to_string(),
                bytes: one["instructionBytes"].as_str().unwrap_or_default().to_string(),
                symbol: one["symbol"].as_str().unwrap_or_default().to_string(),
                line: one["line"].as_u64().map(|line| line.saturating_sub(1) as u32),
            })
            .collect())
    }

    /// Stops session `id` where the bytes of `name` change: the variable of that name under the
    /// container `reference`, or, with `bytes`, the address `name` writes. Says what the adapter
    /// watches.
    pub fn watch_data(&self, id: u64, name: &str, reference: Option<i64>, bytes: Option<u64>) -> Result<String, String> {
        let session = self.session(id)?;
        let mut asked = json!({"name": name});
        if let Some(reference) = reference {
            asked["variablesReference"] = json!(reference);
        }
        if let Some(bytes) = bytes {
            asked["asAddress"] = json!(true);
            asked["bytes"] = json!(bytes);
        }
        let info = session.adapter.request("dataBreakpointInfo", asked, ASKING)?;
        let Some(data) = info["dataId"].as_str().map(str::to_string) else {
            return Err(info["description"].as_str().unwrap_or("the adapter cannot watch that").to_string());
        };
        // lldb-dap on Windows ends where it reports a stop at a watchpoint. The watchpoint is
        // lldb's own, and its script stops the program at the next instruction it runs.
        if cfg!(windows) && session.program == "lldb-dap" {
            let (address, size) = data.split_once('/').ok_or_else(|| format!("lldb named the bytes {data}"))?;
            let address = address.trim_start_matches("0x").trim_start_matches("0X");
            lldb_script(&session)?;
            let said = lldb_command(&session, &format!("watchpoint set expression -w write -s {size} -- 0x{address}"))?;
            // lldb answers "Watchpoint created: Watchpoint 1: addr = ...".
            let number = said.split("Watchpoint ").find_map(|rest| rest.split(':').next()?.trim().parse::<u32>().ok()).ok_or_else(|| said.trim().to_string())?;
            lldb_command(&session, &format!("watchpoint command add -F orior_lldb.changed {number}"))?;
            session.marks.watching.store(true, Ordering::SeqCst);
            return Ok(info["description"].as_str().unwrap_or(name).to_string());
        }
        let mut all = session.data.lock().map_err(|_| "held".to_string())?;
        all.push(json!({"dataId": data, "accessType": "write"}));
        session.adapter.request("setDataBreakpoints", json!({"breakpoints": *all}), ASKING)?;
        Ok(info["description"].as_str().unwrap_or(name).to_string())
    }

    /// Ends session `id`'s program and its adapter, and those of the sessions it started.
    pub fn stop(&self, id: u64) {
        let ended: Vec<Arc<Session>> = self.sessions.lock().map(|mut all| {
            let mut out = Vec::new();
            let mut gone = vec![id];
            while let Some(one) = gone.pop() {
                let (taken, kept): (Vec<_>, Vec<_>) = all.drain(..).partition(|session| session.info.lock().is_ok_and(|info| info.id == one || info.parent == Some(one)));
                *all = kept;
                for session in taken {
                    if let Ok(info) = session.info.lock() {
                        if info.id != one {
                            gone.push(info.id);
                        }
                    }
                    out.push(session);
                }
            }
            out
        }).unwrap_or_default();
        for session in ended {
            session.adapter.stop();
        }
    }

    /// Ends every session.
    pub fn stop_all(&self) {
        let all: Vec<Arc<Session>> = self.sessions.lock().map(|mut all| all.drain(..).collect()).unwrap_or_default();
        for session in all {
            session.adapter.stop();
        }
    }

    /// Forgets session `id` after its adapter says its program ended, without asking it again.
    pub fn ended(&self, id: u64) {
        if let Ok(mut all) = self.sessions.lock() {
            all.retain(|session| session.info.lock().map_or(true, |info| info.id != id));
        }
    }
}

/// orior's script in lldb. Where a watchpoint of orior's sees its bytes change, the program carries
/// on, with a breakpoint of one stop at each place the instruction it stands at can go next, and the
/// first reached stops it and takes the others away. A step from Python into C sets a breakpoint of
/// one stop on every line of the tree's own files, and of the files they name by `#line`, as
/// Cython's do, in the native modules of the tree the process loaded.
const LLDB_SCRIPT: &str = r#"
import os
import re

import lldb

NAME = "orior_watch"
BRANCHES = ("b", "bl", "br", "blr", "cbz", "cbnz", "tbz", "tbnz")


def changed(frame, watchpoint, internal_dict):
    thread = frame.GetThread()
    process = thread.GetProcess()
    target = process.GetTarget()
    pc = frame.GetPC()
    found = target.ReadInstructions(lldb.SBAddress(pc, target), 1)
    places = [pc]
    if found.GetSize():
        one = found.GetInstructionAtIndex(0)
        name = one.GetMnemonic(target).lower()
        after = pc + one.GetByteSize()
        operands = one.GetOperands(target).split(";")[0].strip()
        direct = re.search(r"(?:^|[\s,])(0x[0-9a-fA-F]+)$", operands)
        if name.startswith("ret"):
            if target.GetTriple().startswith(("arm", "aarch64")):
                back = frame.FindRegister("lr").GetValueAsUnsigned()
                places = [back] if back else [after]
            else:
                error = lldb.SBError()
                back = process.ReadPointerFromMemory(frame.GetSP(), error)
                places = [back] if error.Success() else [after]
        elif name.startswith(("j", "call", "loop", "b.")) or name in BRANCHES:
            places = [after]
            if direct:
                to = int(direct.group(1), 16)
                places = [to] if name in ("jmp", "b") else [after, to]
        else:
            places = [after]
    for at in places:
        breakpoint = target.BreakpointCreateByAddress(at)
        breakpoint.SetOneShot(True)
        breakpoint.AddName(NAME)
        breakpoint.SetScriptCallbackFunction("orior_lldb.reached")
    return False


def reached(frame, location, internal_dict):
    target = frame.GetThread().GetProcess().GetTarget()
    here = location.GetBreakpoint().GetID()
    others = [target.GetBreakpointAtIndex(at) for at in range(target.GetNumBreakpoints())]
    for one in others:
        if one.MatchesName(NAME) and one.GetID() != here:
            target.BreakpointDelete(one.GetID())
    return True


def across(target, root, name):
    root = os.path.normcase(os.path.abspath(root))
    inside = lambda path: bool(path) and os.path.normcase(os.path.abspath(path)).startswith(root + os.sep)
    modules = lldb.SBFileSpecList()
    files = lldb.SBFileSpecList()
    seen = set()
    for module in target.module_iter():
        if not inside(module.GetFileSpec().fullpath):
            continue
        modules.Append(module.GetFileSpec())
        for at in range(module.GetNumCompileUnits()):
            unit = module.GetCompileUnitAtIndex(at)
            specs = [unit.GetFileSpec()] + [unit.GetSupportFileAtIndex(one) for one in range(unit.GetNumSupportFiles())]
            for spec in specs:
                path = spec.fullpath
                if inside(path) and os.path.isfile(path) and os.path.normcase(path) not in seen:
                    seen.add(os.path.normcase(path))
                    files.Append(spec)
    if not files.GetSize():
        return 0
    breakpoint = target.BreakpointCreateBySourceRegex(".", modules, files)
    breakpoint.SetOneShot(True)
    breakpoint.AddName(name)
    return breakpoint.GetNumLocations()
"#;

/// A stop at a breakpoint the reader did not place, in a session whose data breakpoints are
/// lldb's own watchpoints, told as the stop of a data breakpoint, which it is.
fn data_stop(marks: &Marks, event: &str, body: &Value) -> Value {
    if event != "stopped" || body["reason"] != "breakpoint" || !marks.watching.load(Ordering::SeqCst) {
        return body.clone();
    }
    let placed = marks.placed.lock().map(|placed| placed.clone()).unwrap_or_default();
    let hit: Vec<i64> = body["hitBreakpointIds"].as_array().into_iter().flatten().filter_map(Value::as_i64).collect();
    if hit.is_empty() || hit.iter().any(|one| placed.contains(one)) {
        return body.clone();
    }
    let mut told = body.clone();
    told["reason"] = json!("data breakpoint");
    told["description"] = json!("data changed");
    told
}

/// A process of the machine: its number, its program's name, and the line it was started with where
/// the system says.
#[derive(Serialize, Clone, Debug, PartialEq)]
pub struct Process {
    pub pid: u32,
    pub name: String,
    pub command: String,
}

/// The processes of the machine, by their names.
pub fn processes() -> Vec<Process> {
    let mut found = listed();
    let own = std::process::id();
    found.retain(|one| one.pid != own && one.pid != 0);
    found.sort_by(|a, b| a.name.to_lowercase().cmp(&b.name.to_lowercase()).then(a.pid.cmp(&b.pid)));
    found
}

/// The system's list of processes, read in one snapshot, and the line each was started with, read
/// from the process where it lets that be read.
#[cfg(windows)]
fn listed() -> Vec<Process> {
    #[repr(C)]
    struct Entry {
        size: u32,
        usage: u32,
        pid: u32,
        heap: usize,
        module: u32,
        threads: u32,
        parent: u32,
        priority: i32,
        flags: u32,
        exe: [u16; 260],
    }
    #[link(name = "kernel32")]
    unsafe extern "system" {
        fn CreateToolhelp32Snapshot(flags: u32, pid: u32) -> isize;
        fn Process32FirstW(snapshot: isize, entry: *mut Entry) -> i32;
        fn Process32NextW(snapshot: isize, entry: *mut Entry) -> i32;
        fn OpenProcess(access: u32, inherit: i32, pid: u32) -> isize;
        fn CloseHandle(handle: isize) -> i32;
    }
    #[link(name = "ntdll")]
    unsafe extern "system" {
        fn NtQueryInformationProcess(handle: isize, class: u32, info: *mut u8, length: u32, returned: *mut u32) -> i32;
    }
    const SNAPSHOT_PROCESSES: u32 = 0x2;
    const QUERY_LIMITED: u32 = 0x1000;
    const COMMAND_LINE: u32 = 60;
    // The line a process was started with: a counted string, its length in bytes first and the
    // address of its text after, the text following in the same buffer.
    let command_of = |pid: u32| -> String {
        // SAFETY: the handle is checked before use and closed once; the buffer is as long as the
        // length given, aligned for the string's header, and the text read lies within it, as the
        // system wrote it there.
        unsafe {
            let process = OpenProcess(QUERY_LIMITED, 0, pid);
            if process == 0 {
                return String::new();
            }
            let mut needed = 0u32;
            NtQueryInformationProcess(process, COMMAND_LINE, std::ptr::null_mut(), 0, &mut needed);
            let mut text = String::new();
            if needed as usize > 2 * std::mem::size_of::<usize>() && needed < 1 << 20 {
                let mut buffer = vec![0u64; (needed as usize).div_ceil(8)];
                if NtQueryInformationProcess(process, COMMAND_LINE, buffer.as_mut_ptr().cast(), needed, &mut needed) >= 0 {
                    let base = buffer.as_ptr().cast::<u8>();
                    let length = usize::from(*base.cast::<u16>()) / 2;
                    let start = *base.add(std::mem::size_of::<usize>()).cast::<*const u16>();
                    let end = base.add(buffer.len() * 8).cast::<u16>();
                    if !start.is_null() && start >= base.cast() && start.add(length) <= end {
                        text = String::from_utf16_lossy(std::slice::from_raw_parts(start, length));
                    }
                }
            }
            CloseHandle(process);
            text
        }
    };
    let mut found = Vec::new();
    // SAFETY: the snapshot is checked before use and closed once; each entry is given its own size,
    // as the system asks, and its name is read up to its end within the array.
    unsafe {
        let snapshot = CreateToolhelp32Snapshot(SNAPSHOT_PROCESSES, 0);
        if snapshot == -1 {
            return found;
        }
        let mut entry: Entry = std::mem::zeroed();
        entry.size = std::mem::size_of::<Entry>() as u32;
        let mut more = Process32FirstW(snapshot, &mut entry) != 0;
        while more {
            let end = entry.exe.iter().position(|&unit| unit == 0).unwrap_or(entry.exe.len());
            found.push(Process { pid: entry.pid, name: String::from_utf16_lossy(&entry.exe[..end]), command: command_of(entry.pid) });
            more = Process32NextW(snapshot, &mut entry) != 0;
        }
        CloseHandle(snapshot);
    }
    found
}

#[cfg(not(windows))]
fn listed() -> Vec<Process> {
    let mut command = Command::new("ps");
    command.args(["-eo", "pid=,comm=,args="]).stdin(std::process::Stdio::null());
    let text = command.output().map(|out| String::from_utf8_lossy(&out.stdout).into_owned()).unwrap_or_default();
    text.lines()
        .filter_map(|line| {
            let mut words = line.split_whitespace();
            let pid = words.next()?.parse().ok()?;
            let name = words.next()?.to_string();
            Some(Process { pid, name, command: words.collect::<Vec<_>>().join(" ") })
        })
        .collect()
}

/// The shim that writes the bytecode of the function that holds a line of a Python file: its file,
/// then the line, counted from 1.
const BYTECODE: &str = r#"
import dis, json, sys
path, line = sys.argv[1], int(sys.argv[2])
with open(path, encoding="utf-8") as handle:
    top = compile(handle.read(), path, "exec")
best, codes = top, [top]
while codes:
    code = codes.pop()
    lines = [number for _, _, number in code.co_lines() if number]
    if lines and min(lines) <= line <= max(lines) and code is not top:
        best = code
    codes.extend(inner for inner in code.co_consts if hasattr(inner, "co_lines"))
out = []
for one in dis.get_instructions(best):
    out.append({"offset": one.offset, "name": one.opname, "argument": one.argrepr, "line": one.positions.lineno if one.positions else None})
print(json.dumps({"name": best.co_qualname if hasattr(best, "co_qualname") else best.co_name, "instructions": out}))
"#;

/// The bytecode of the function of the Python file at `path` that holds `line`, counted from 0, as
/// the tree's Python compiles it: its name, then each instruction with the line it is of.
pub fn bytecode(root: &Path, path: &Path, line: u32) -> Result<(String, Vec<Instruction>), String> {
    let python = crate::test_runs::python().ok_or("No Python is found to read the bytecode: File, Toolchains finds one")?;
    let mut command = Command::new(&python);
    command.args(["-c", BYTECODE]).arg(path).arg((line + 1).to_string()).current_dir(root).stdin(std::process::Stdio::null());
    crate::runner::quiet(&mut command);
    let out = command.output().map_err(|error| format!("{}: {error}", python.display()))?;
    if !out.status.success() {
        return Err(String::from_utf8_lossy(&out.stderr).trim().lines().last().unwrap_or("the bytecode could not be read").to_string());
    }
    let value: Value = serde_json::from_slice(&out.stdout).map_err(|error| error.to_string())?;
    let instructions = value["instructions"]
        .as_array()
        .into_iter()
        .flatten()
        .map(|one| Instruction {
            address: one["offset"].to_string(),
            text: format!("{} {}", one["name"].as_str().unwrap_or_default(), one["argument"].as_str().unwrap_or_default()).trim().to_string(),
            bytes: String::new(),
            symbol: String::new(),
            line: one["line"].as_u64().map(|line| line.saturating_sub(1) as u32),
        })
        .collect();
    Ok((value["name"].as_str().unwrap_or_default().to_string(), instructions))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_function_s_bytecode_is_read_with_its_lines() {
        let dir = std::env::temp_dir().join(format!("orior-bytecode-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let file = dir.join("a.py");
        std::fs::write(&file, "x = 1\n\n\ndef area(width, height):\n    return width * height\n").unwrap();
        if crate::test_runs::python().is_none() {
            return;
        }
        let (name, instructions) = bytecode(&dir, &file, 4).unwrap();
        assert_eq!(name, "area");
        assert!(instructions.iter().any(|one| one.text.starts_with("BINARY_OP") && one.line == Some(4)), "{instructions:?}");
        let _ = std::fs::remove_dir_all(dir);
    }

    #[test]
    fn the_machine_s_processes_are_listed_with_the_lines_they_started_with() {
        let mut command = Command::new(if cfg!(windows) { "cmd" } else { "sleep" });
        if cfg!(windows) {
            command.args(["/c", "ping", "-n", "4", "127.0.0.1", ">", "NUL"]);
        } else {
            command.arg("3");
        }
        let mut child = command.stdin(std::process::Stdio::null()).stdout(std::process::Stdio::null()).spawn().unwrap();
        let started = std::time::Instant::now();
        let found = processes();
        let took = started.elapsed();
        let _ = child.kill();
        let _ = child.wait();
        let one = found.iter().find(|one| one.pid == child.id()).unwrap_or_else(|| panic!("{} not among {}", child.id(), found.len()));
        assert!(one.command.contains(if cfg!(windows) { "127.0.0.1" } else { "3" }), "{one:?}");
        assert!(!found.iter().any(|one| one.pid == std::process::id()));
        assert!(took < Duration::from_millis(500), "{took:?}");
    }

    #[test]
    fn launch_arguments_take_the_program_and_the_folder() {
        let launch = json!({"program": "{program}", "cwd": "{root}", "args": [], "deep": {"file": "{path}"}});
        let names = [("program", "a.exe".to_string()), ("path", "a.c".to_string()), ("root", "/t".to_string())];
        assert_eq!(filled(&launch, &names), json!({"program": "a.exe", "cwd": "/t", "args": [], "deep": {"file": "a.c"}}));
        assert_eq!(numbers(filled(&json!({"processId": "{pid}"}), &[("pid", "412".to_string())])), json!({"processId": 412}));
    }

    #[test]
    fn a_frame_counts_its_line_from_zero() {
        let frame = frame_of(&json!({"id": 7, "name": "main", "source": {"path": "/t/a.c"}, "line": 6, "column": 13, "instructionPointerReference": "0x1000"}));
        assert_eq!((frame.id, frame.line, frame.col, frame.path.as_deref(), frame.ip.as_deref()), (7, 5, 12, Some("/t/a.c"), Some("0x1000")));
    }

    #[test]
    fn breakpoints_carry_their_conditions_counts_and_messages() {
        let given = [Breakpoint { line: 4, ..Breakpoint::default() }, Breakpoint { line: 9, condition: "x > 2".into(), hits: ">= 3".into(), log: String::new() }, Breakpoint { line: 11, log: "x is {x}".into(), ..Breakpoint::default() }];
        assert_eq!(breakpoints_of(&given), vec![json!({"line": 5}), json!({"line": 10, "condition": "x > 2", "hitCondition": ">= 3"}), json!({"line": 12, "logMessage": "x is {x}"})]);
        assert!(takes("python", "a/b.py") && !takes("python", "a/b.c") && takes("c", "a/b.c") && !takes("rust", "x.py"));
    }

    #[test]
    fn memory_comes_as_base64() {
        assert_eq!(base64("SGVsbG8="), b"Hello");
        assert_eq!(base64("AAEC/w=="), [0, 1, 2, 255]);
    }

    #[test]
    fn stepping_sets_debugpy_rules_and_lldb_patterns() {
        let stepping = Stepping { mine_only: true, skip: vec!["requests".into(), "**/vendor/**".into()] };
        let mut python = json!({"program": "a.py"});
        step_arguments("python", &stepping, &mut python);
        assert_eq!(python["justMyCode"], true);
        assert_eq!(python["rules"], json!([{"module": "requests", "include": false}, {"path": "**/vendor/**", "include": false}]));
        let mut native = json!({"program": "a.exe", "initCommands": ["x"]});
        step_arguments("c", &Stepping { mine_only: false, skip: vec!["^std::".into(), "^boost::".into()] }, &mut native);
        assert_eq!(native["initCommands"], json!(["x", "settings set target.process.thread.step-avoid-regexp ^std::|^boost::"]));
    }

    #[test]
    fn c_rust_and_python_each_have_a_debugger_and_c_and_rust_a_build() {
        let tools = toolchains::manifest();
        for language in ["c", "rust", "python"] {
            assert!(tools.iter().any(|tool| tool.debugger.as_ref().is_some_and(|spec| spec.languages.iter().any(|one| one == language))), "{language}");
        }
        assert!(tools.iter().any(|tool| tool.builds.contains_key("c.c")));
        assert!(tools.iter().any(|tool| tool.builds.contains_key("rust")));
        assert!(!tools.iter().any(|tool| tool.builds.contains_key("python")));
    }

    /// Waits for `wanted` from session `id`, past the events of any other.
    fn wait_for(heard: &mpsc::Receiver<(u64, String, Value)>, id: u64, wanted: &[&str]) -> (String, Value) {
        loop {
            let (from, event, body) = heard.recv_timeout(Duration::from_secs(60)).unwrap_or_else(|_| panic!("no {wanted:?}"));
            if from == id && wanted.contains(&event.as_str()) {
                return (event, body);
            }
        }
    }

    /// Debugs a small program in `language` through its adapter: stops at a breakpoint, reads the
    /// stack and a variable, steps, and runs to the end.
    fn round_trip(name: &str, language: &str, text: &str, stop_line: u32, variable: &str, value: &str) {
        let dir = std::env::temp_dir().join(format!("orior-debug-{language}-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        std::fs::write(dir.join(name), text).unwrap();
        let (sender, heard) = mpsc::channel();
        let sender = Mutex::new(sender);
        let emit: Emit = Arc::new(move |id, event, body| {
            let _ = sender.lock().unwrap().send((id, event.to_string(), body));
        });
        let debugger = Debugger::default();
        let options = Options { breakpoints: HashMap::from([(name.to_string(), vec![Breakpoint { line: stop_line, ..Breakpoint::default() }])]), ..Options::default() };
        let info = debugger.start(&dir, &Start::File { path: name.to_string(), language: language.to_string() }, &options, emit).unwrap_or_else(|said| panic!("{said}"));
        let id = info.id;
        let (_, stopped) = wait_for(&heard, id, &["stopped", "terminated"]);
        let thread = stopped["threadId"].as_i64().unwrap_or_else(|| debugger.threads(id).unwrap()[0].id);
        let frames = debugger.stack(id, thread).unwrap();
        assert_eq!(frames[0].line, stop_line, "{frames:?}");
        let scopes = debugger.scopes(id, frames[0].id).unwrap();
        let locals = debugger.variables(id, scopes[0].reference).unwrap();
        let found = locals.iter().find(|one| one.name == variable).unwrap_or_else(|| panic!("{locals:?}"));
        assert!(found.value.contains(value), "{found:?}");
        assert!(debugger.evaluate(id, variable, Some(frames[0].id), "watch").unwrap().value.contains(value));
        debugger.step(id, "next", thread).unwrap();
        let (event, body) = wait_for(&heard, id, &["stopped", "terminated"]);
        assert_eq!(event, "stopped", "{body}");
        assert_eq!(debugger.stack(id, thread).unwrap()[0].line, stop_line + 1, "{body}");
        debugger.step(id, "continue", thread).unwrap();
        wait_for(&heard, id, &["terminated", "exited"]);
        debugger.stop(id);
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    #[ignore = "starts lldb-dap and clang where they are installed"]
    fn lldb_debugs_a_c_program() {
        round_trip("a.c", "c", "#include <stdio.h>\nint main(void)\n{\n    int y = 21;\n    y = y * 2;\n    printf(\"%d\\n\", y);\n    return 0;\n}\n", 4, "y", "21");
    }

    /// Debugs a C program and reads the bytes of a variable, the instructions about the line
    /// stopped at, and stops where the variable's bytes change.
    #[test]
    #[ignore = "starts lldb-dap and clang where they are installed"]
    fn lldb_reads_memory_and_instructions_and_stops_where_data_changes() {
        let dir = std::env::temp_dir().join(format!("orior-debug-memory-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let text = "#include <stdio.h>\nint main(void)\n{\n    volatile int count = 0x11223344;\n    int other = 5;\n    other += 1;\n    count = 7;\n    printf(\"%d %d\\n\", count, other);\n    return 0;\n}\n";
        std::fs::write(dir.join("m.c"), text).unwrap();
        let (sender, heard) = mpsc::channel();
        let sender = Mutex::new(sender);
        let emit: Emit = Arc::new(move |id, event, body| {
            let _ = sender.lock().unwrap().send((id, event.to_string(), body));
        });
        let debugger = Debugger::default();
        let options = Options { breakpoints: HashMap::from([("m.c".to_string(), vec![Breakpoint { line: 4, ..Breakpoint::default() }])]), ..Options::default() };
        let info = debugger.start(&dir, &Start::File { path: "m.c".into(), language: "c".into() }, &options, emit).unwrap_or_else(|said| panic!("{said}"));
        assert!(info.memory && info.disassembly && info.data, "{info:?}");
        let id = info.id;
        let (_, stopped) = wait_for(&heard, id, &["stopped"]);
        let thread = stopped["threadId"].as_i64().unwrap();
        let frames = debugger.stack(id, thread).unwrap();
        assert_eq!(frames[0].line, 4, "{frames:?}");
        let scopes = debugger.scopes(id, frames[0].id).unwrap();
        let locals = debugger.variables(id, scopes[0].reference).unwrap();
        let count = locals.iter().find(|one| one.name == "count").unwrap_or_else(|| panic!("{locals:?}"));
        let memory = debugger.memory(id, count.memory.as_deref().unwrap_or_else(|| panic!("{count:?}")), 0, 4).unwrap();
        assert_eq!(memory.bytes, vec![0x44, 0x33, 0x22, 0x11], "{memory:?}");
        let ip = frames[0].ip.clone().unwrap_or_else(|| panic!("{frames:?}"));
        let instructions = debugger.instructions(id, &ip, -4, 12).unwrap();
        assert!(instructions.len() >= 8 && instructions.iter().any(|one| one.line == Some(4)), "{instructions:?}");
        let watched = debugger.watch_data(id, "count", Some(scopes[0].reference), None).unwrap();
        assert!(watched.contains("count"), "{watched}");
        debugger.step(id, "continue", thread).unwrap();
        let (event, body) = wait_for(&heard, id, &["stopped", "terminated", "adapterStopped"]);
        assert_eq!(event, "stopped", "{body} {}", debugger.adapter(id).map(|adapter| adapter.complaint()).unwrap_or_default());
        assert_eq!(body["reason"], "data breakpoint", "{body}");
        let line = debugger.stack(id, thread).unwrap()[0].line;
        assert!((6..=7).contains(&line), "{body} at {line}");
        debugger.stop(id);
        let _ = std::fs::remove_dir_all(&dir);
    }

    /// Watches a loop's counter, whose write is followed by a jump back to the loop's test, and stops
    /// at the test each time the counter changes.
    #[test]
    #[ignore = "starts lldb-dap and clang where they are installed"]
    fn lldb_stops_where_a_loop_s_counter_changes() {
        let dir = std::env::temp_dir().join(format!("orior-debug-loop-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        std::fs::write(dir.join("l.c"), "#include <stdio.h>\nint main(void)\n{\n    int total = 0;\n    for (int i = 0; i < 3; i++) {\n        total += i;\n    }\n    printf(\"%d\\n\", total);\n    return 0;\n}\n").unwrap();
        let (sender, heard) = mpsc::channel();
        let sender = Mutex::new(sender);
        let emit: Emit = Arc::new(move |id, event, body| {
            let _ = sender.lock().unwrap().send((id, event.to_string(), body));
        });
        let debugger = Debugger::default();
        let options = Options { breakpoints: HashMap::from([("l.c".to_string(), vec![Breakpoint { line: 5, ..Breakpoint::default() }])]), ..Options::default() };
        let id = debugger.start(&dir, &Start::File { path: "l.c".into(), language: "c".into() }, &options, emit).unwrap_or_else(|said| panic!("{said}")).id;
        let (_, stopped) = wait_for(&heard, id, &["stopped"]);
        let thread = stopped["threadId"].as_i64().unwrap();
        let frames = debugger.stack(id, thread).unwrap();
        let scopes = debugger.scopes(id, frames[0].id).unwrap();
        debugger.watch_data(id, "i", Some(scopes[0].reference), None).unwrap();
        debugger.breakpoints(&dir, "l.c", &[]);
        let mut lines = Vec::new();
        for _ in 0..2 {
            debugger.step(id, "continue", thread).unwrap();
            let (event, body) = wait_for(&heard, id, &["stopped", "terminated", "adapterStopped"]);
            assert_eq!((event.as_str(), &body["reason"]), ("stopped", &json!("data breakpoint")), "{body}");
            lines.push(debugger.stack(id, thread).unwrap()[0].line);
        }
        assert_eq!(lines, vec![4, 4]);
        debugger.stop(id);
        let _ = std::fs::remove_dir_all(&dir);
    }

    /// Records a Python run, stops at a breakpoint in it, steps backward and forward, reads a name's
    /// values through the run and goes to the line that gave one.
    #[test]
    #[ignore = "runs Python where it is installed"]
    fn a_recorded_run_steps_backward_and_reads_a_name_s_history() {
        let dir = std::env::temp_dir().join(format!("orior-debug-record-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        std::fs::write(dir.join("r.py"), "def grow(n):\n    n += 1\n    return n\n\n\ntotal = 0\nfor x in range(3):\n    total = grow(total)\nprint(\"total\", total)\n").unwrap();
        let (sender, heard) = mpsc::channel();
        let sender = Mutex::new(sender);
        let emit: Emit = Arc::new(move |id, event, body| {
            let _ = sender.lock().unwrap().send((id, event.to_string(), body));
        });
        let debugger = Debugger::default();
        let options = Options { breakpoints: HashMap::from([("r.py".to_string(), vec![Breakpoint { line: 8, ..Breakpoint::default() }])]), ..Options::default() };
        let info = debugger.start(&dir, &Start::Record { path: "r.py".into() }, &options, emit).unwrap_or_else(|said| panic!("{said}"));
        assert!(info.step_back && info.recorded, "{info:?}");
        let id = info.id;
        let (_, stopped) = wait_for(&heard, id, &["stopped"]);
        let thread = stopped["threadId"].as_i64().unwrap();
        let line = |debugger: &Debugger| debugger.stack(id, thread).unwrap()[0].line;
        assert_eq!(line(&debugger), 8);
        let frames = debugger.stack(id, thread).unwrap();
        let scopes = debugger.scopes(id, frames[0].id).unwrap();
        let values = debugger.variables(id, scopes[0].reference).unwrap();
        assert!(values.iter().any(|one| one.name == "total" && one.value == "3"), "{values:?}");
        debugger.step(id, "stepBack", thread).unwrap();
        wait_for(&heard, id, &["stopped"]);
        assert_eq!(line(&debugger), 6);
        debugger.step(id, "stepBack", thread).unwrap();
        wait_for(&heard, id, &["stopped"]);
        assert_eq!(line(&debugger), 7);
        debugger.step(id, "stepIn", thread).unwrap();
        wait_for(&heard, id, &["stopped"]);
        debugger.step(id, "stepIn", thread).unwrap();
        wait_for(&heard, id, &["stopped"]);
        assert_eq!(debugger.stack(id, thread).unwrap().len(), 2);
        let module = debugger.stack(id, thread).unwrap()[1].id;
        let history = debugger.history(id, module, "total").unwrap();
        assert_eq!(history.iter().map(|one| (one.line, one.value.clone().unwrap_or_default())).collect::<Vec<_>>(), vec![(5, "0".into()), (7, "1".into()), (7, "2".into()), (7, "3".into())]);
        debugger.goto(id, history[2].step).unwrap();
        wait_for(&heard, id, &["stopped"]);
        assert_eq!(line(&debugger), 7);
        debugger.step(id, "reverseContinue", thread).unwrap();
        wait_for(&heard, id, &["stopped"]);
        assert_eq!(line(&debugger), 0, "back to the start, with no breakpoint before");
        debugger.stop(id);
        let _ = std::fs::remove_dir_all(&dir);
    }

    /// Builds a C module for the machine's Python, debugs a program that calls it with debugpy and
    /// lldb-dap together, steps from a Python line into the C, stops at a breakpoint in the C, and
    /// steps out of the C back to the Python.
    #[cfg(windows)]
    #[test]
    #[ignore = "builds a Python module with clang and starts debugpy and lldb-dap where they are installed"]
    fn python_and_the_c_it_calls_are_debugged_together() {
        let dir = std::env::temp_dir().join(format!("orior-debug-mixed-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let module = "#define PY_SSIZE_T_CLEAN\n#include <Python.h>\n\nstatic long twice(long value)\n{\n    long doubled = value * 2;\n    return doubled;\n}\n\nstatic PyObject *add(PyObject *self, PyObject *args)\n{\n    long a, b;\n    if (!PyArg_ParseTuple(args, \"ll\", &a, &b)) {\n        return NULL;\n    }\n    long sum = a + b;\n    return PyLong_FromLong(twice(sum));\n}\n\nstatic PyMethodDef methods[] = {\n    {\"add\", add, METH_VARARGS, \"Adds two numbers and doubles the sum.\"},\n    {NULL, NULL, 0, NULL},\n};\n\nstatic struct PyModuleDef module = {PyModuleDef_HEAD_INIT, \"fast\", NULL, -1, methods};\n\nPyMODINIT_FUNC PyInit_fast(void)\n{\n    return PyModule_Create(&module);\n}\n";
        std::fs::write(dir.join("fast.c"), module).unwrap();
        std::fs::write(dir.join("main.py"), "import fast\n\ntotal = fast.add(2, 3)\nprint(\"total\", total)\n").unwrap();
        let python = crate::test_runs::python().expect("a Python");
        let said = Command::new(&python).args(["-c", "import sys, sysconfig; print(sysconfig.get_paths()['include']); print(sys.base_prefix); print(f'python{sys.version_info[0]}{sys.version_info[1]}')"]).output().unwrap();
        let said = String::from_utf8_lossy(&said.stdout).into_owned();
        let found: Vec<&str> = said.lines().map(str::trim).collect();
        let built = Command::new("clang").args(["-shared", "-g", "-O0", &format!("-I{}", found[0]), "fast.c", &format!("-L{}\\libs", found[1]), &format!("-l{}", found[2]), "-o", "fast.pyd"]).current_dir(&dir).status().unwrap();
        assert!(built.success());
        let (sender, heard) = mpsc::channel();
        let sender = Mutex::new(sender);
        let emit: Emit = Arc::new(move |id, event, body| {
let _ = sender.lock().unwrap().send((id, event.to_string(), body));
        });
        let debugger = Debugger::default();
        let breakpoints = HashMap::from([("main.py".to_string(), vec![Breakpoint { line: 2, ..Breakpoint::default() }]), ("fast.c".to_string(), vec![Breakpoint { line: 5, ..Breakpoint::default() }])]);
        let options = Options { breakpoints, entry: true, ..Options::default() };
        let py = debugger.start(&dir, &Start::File { path: "main.py".into(), language: "python".into() }, &options, emit.clone()).unwrap_or_else(|said| panic!("{said}")).id;
        let (_, process) = wait_for(&heard, py, &["process"]);
        let (_, entry) = wait_for(&heard, py, &["stopped"]);
        let pid = process["systemProcessId"].as_u64().unwrap() as u32;
        debugger.process(py, pid);
        let native = debugger.start(&dir, &Start::Process { pid, language: "c".into(), parent: Some(py) }, &options, emit).unwrap_or_else(|said| panic!("{said}")).id;
        let thread = entry["threadId"].as_i64().unwrap();
        debugger.step(py, "continue", thread).unwrap();
        let (_, stopped) = wait_for(&heard, py, &["stopped"]);
        assert_eq!(debugger.stack(py, thread).unwrap()[0].line, 2, "{stopped}");
        debugger.step_across(&dir, py, thread).unwrap();
        let (_, inside) = wait_for(&heard, native, &["stopped"]);
        let native_thread = inside["threadId"].as_i64().unwrap();
        let frames = debugger.stack(native, native_thread).unwrap();
        assert!(frames[0].name.contains("add") && frames[0].path.as_deref().is_some_and(|path| path.ends_with("fast.c")), "{frames:?}");
        debugger.step(native, "continue", native_thread).unwrap();
        wait_for(&heard, native, &["stopped"]);
        let frames = debugger.stack(native, native_thread).unwrap();
        assert!(frames[0].name.contains("twice") && frames[0].line == 5, "{frames:?}");
debugger.step(native, "stepOut", native_thread).unwrap();
        wait_for(&heard, native, &["stopped"]);
        assert!(debugger.stack(native, native_thread).unwrap()[0].name.contains("add"));
        debugger.step(native, "stepOut", native_thread).unwrap();
        let (_, back) = wait_for(&heard, py, &["stopped"]);
        let frames = debugger.stack(py, back["threadId"].as_i64().unwrap_or(thread)).unwrap();
        assert!(frames[0].path.as_deref().is_some_and(|path| path.ends_with("main.py")) && frames[0].line >= 2, "{frames:?}");
        debugger.stop_all();
        let _ = std::fs::remove_dir_all(&dir);
    }

    /// Builds a module whose C names the lines of a `.pyx` file by `#line`, as Cython writes it with
    /// its line directives, and stops at a breakpoint in the `.pyx` file and steps into it from Python.
    #[cfg(windows)]
    #[test]
    #[ignore = "builds a Python module with clang and starts debugpy and lldb-dap where they are installed"]
    fn a_cython_file_s_lines_are_stopped_at_and_stepped_into() {
        let dir = std::env::temp_dir().join(format!("orior-debug-cython-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        std::fs::write(dir.join("cy.pyx"), "def square(long n):\n    cdef long result = n * n\n    return result\n").unwrap();
        std::fs::write(dir.join("cy.c"), "#define PY_SSIZE_T_CLEAN\n#include <Python.h>\n\nstatic PyObject *square(PyObject *self, PyObject *arg);\n\nstatic PyMethodDef methods[] = {\n    {\"square\", square, METH_O, \"Squares a number.\"},\n    {NULL, NULL, 0, NULL},\n};\n\nstatic struct PyModuleDef module = {PyModuleDef_HEAD_INIT, \"cy\", NULL, -1, methods};\n\nPyMODINIT_FUNC PyInit_cy(void)\n{\n    return PyModule_Create(&module);\n}\n\nstatic PyObject *square(PyObject *self, PyObject *arg)\n{\n#line 1 \"cy.pyx\"\n    long n = PyLong_AsLong(arg);\n#line 2 \"cy.pyx\"\n    long result = n * n;\n#line 3 \"cy.pyx\"\n    return PyLong_FromLong(result);\n}\n").unwrap();
        std::fs::write(dir.join("main.py"), "import cy\n\nvalue = cy.square(7)\nvalue = cy.square(value)\nprint(value)\n").unwrap();
        let python = crate::test_runs::python().expect("a Python");
        let said = Command::new(&python).args(["-c", "import sys, sysconfig; print(sysconfig.get_paths()['include']); print(sys.base_prefix); print(f'python{sys.version_info[0]}{sys.version_info[1]}')"]).output().unwrap();
        let said = String::from_utf8_lossy(&said.stdout).into_owned();
        let found: Vec<&str> = said.lines().map(str::trim).collect();
        let built = Command::new("clang").args(["-shared", "-g", "-O0", &format!("-I{}", found[0]), "cy.c", &format!("-L{}\\libs", found[1]), &format!("-l{}", found[2]), "-o", "cy.pyd"]).current_dir(&dir).status().unwrap();
        assert!(built.success());
        let (sender, heard) = mpsc::channel();
        let sender = Mutex::new(sender);
        let emit: Emit = Arc::new(move |id, event, body| {
            let _ = sender.lock().unwrap().send((id, event.to_string(), body));
        });
        let debugger = Debugger::default();
        let breakpoints = HashMap::from([("main.py".to_string(), vec![Breakpoint { line: 3, ..Breakpoint::default() }]), ("cy.pyx".to_string(), vec![Breakpoint { line: 1, ..Breakpoint::default() }])]);
        let options = Options { breakpoints, entry: true, ..Options::default() };
        let py = debugger.start(&dir, &Start::File { path: "main.py".into(), language: "python".into() }, &options, emit.clone()).unwrap_or_else(|said| panic!("{said}")).id;
        let (_, process) = wait_for(&heard, py, &["process"]);
        let (_, entry) = wait_for(&heard, py, &["stopped"]);
        let pid = process["systemProcessId"].as_u64().unwrap() as u32;
        debugger.process(py, pid);
        let native = debugger.start(&dir, &Start::Process { pid, language: "c".into(), parent: Some(py) }, &options, emit).unwrap_or_else(|said| panic!("{said}")).id;
        let thread = entry["threadId"].as_i64().unwrap();
        debugger.step(py, "continue", thread).unwrap();
        let (_, inside) = wait_for(&heard, native, &["stopped"]);
        let native_thread = inside["threadId"].as_i64().unwrap();
        let frames = debugger.stack(native, native_thread).unwrap();
        assert!(frames[0].path.as_deref().is_some_and(|path| path.ends_with("cy.pyx")) && frames[0].line == 1, "{frames:?}");
        debugger.step(native, "continue", native_thread).unwrap();
        let (_, stopped) = wait_for(&heard, py, &["stopped"]);
        assert_eq!(debugger.stack(py, stopped["threadId"].as_i64().unwrap_or(thread)).unwrap()[0].line, 3);
        // The breakpoint in the .pyx file goes, and a step from the Python line lands on its first.
        debugger.breakpoints(&dir, "cy.pyx", &[]);
        debugger.step_across(&dir, py, thread).unwrap();
        let (_, inside) = wait_for(&heard, native, &["stopped"]);
        let frames = debugger.stack(native, inside["threadId"].as_i64().unwrap()).unwrap();
        assert!(frames[0].path.as_deref().is_some_and(|path| path.ends_with("cy.pyx")) && frames[0].line == 0, "{frames:?}");
        debugger.stop_all();
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    #[ignore = "starts debugpy where it is installed"]
    fn debugpy_debugs_a_python_script() {
        round_trip("a.py", "python", "y = 21\ny = y * 2\nprint(y)\n", 1, "y", "21");
    }

    #[test]
    #[ignore = "starts lldb-dap and rustc where they are installed"]
    fn lldb_debugs_a_rust_program() {
        round_trip("main.rs", "rust", "fn main() {\n    let y = 21;\n    let z = y * 2;\n    println!(\"{z}\");\n}\n", 2, "y", "21");
    }

    /// Debugs a Python program that starts another, and a program listening at an address, printing
    /// what each adapter tells.
    #[test]
    #[ignore = "starts debugpy where it is installed"]
    fn debugpy_children_and_addresses() {
        let dir = std::env::temp_dir().join(format!("orior-debug-children-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        std::fs::write(dir.join("parent.py"), "import subprocess\nimport sys\n\nsubprocess.run([sys.executable, \"child.py\"], check=True)\nprint(\"parent done\")\n").unwrap();
        std::fs::write(dir.join("child.py"), "value = 41\nvalue += 1\nprint(value)\n").unwrap();
        let (sender, heard) = mpsc::channel();
        let sender = Mutex::new(sender);
        let emit: Emit = Arc::new(move |id, event, body| {
            let _ = sender.lock().unwrap().send((id, event.to_string(), body));
        });
        let debugger = Arc::new(Debugger::default());
        let options = Options { breakpoints: HashMap::from([("child.py".to_string(), vec![Breakpoint { line: 1, ..Breakpoint::default() }])]), ..Options::default() };
        let info = debugger.start(&dir, &Start::File { path: "parent.py".into(), language: "python".into() }, &options, emit.clone()).unwrap_or_else(|said| panic!("{said}"));
        let (_, config) = wait_for(&heard, info.id, &["debugpyAttach"]);
        let child = debugger.start(&dir, &Start::Child { parent: info.id, config }, &options, emit).unwrap_or_else(|said| panic!("{said}"));
        let (_, stopped) = wait_for(&heard, child.id, &["stopped"]);
        let frames = debugger.stack(child.id, stopped["threadId"].as_i64().unwrap()).unwrap();
        assert_eq!(frames[0].line, 1, "{frames:?}");
        debugger.stop_all();

        // The program runs from a folder of its own, as one in a container does, its files the tree's.
        let text = "import time\n\nfor n in range(400):\n    time.sleep(0.05)\nprint(\"listened\")\n";
        let elsewhere = dir.join("elsewhere");
        std::fs::create_dir_all(&elsewhere).unwrap();
        std::fs::write(dir.join("listen.py"), text).unwrap();
        std::fs::write(elsewhere.join("listen.py"), text).unwrap();
        let port = std::net::TcpListener::bind("127.0.0.1:0").unwrap().local_addr().unwrap().port();
        let python = crate::test_runs::python().expect("a Python");
        let mut listener = Command::new(python).args(["-m", "debugpy", "--listen", &format!("127.0.0.1:{port}"), "--wait-for-client", "listen.py"]).current_dir(&elsewhere).spawn().unwrap();
        let (sender, heard) = mpsc::channel();
        let sender = Mutex::new(sender);
        let emit: Emit = Arc::new(move |id, event, body| {
            let _ = sender.lock().unwrap().send((id, event.to_string(), body));
        });
        let options = Options { breakpoints: HashMap::from([("listen.py".to_string(), vec![Breakpoint { line: 3, ..Breakpoint::default() }])]), ..Options::default() };
        let start = Start::Address { host: "127.0.0.1".into(), port, remote: Some(elsewhere.display().to_string()) };
        let mut attached = Err(String::new());
        for _ in 0..50 {
            attached = debugger.start(&dir, &start, &options, emit.clone());
            if attached.is_ok() {
                break;
            }
            std::thread::sleep(Duration::from_millis(200));
        }
        let info = attached.unwrap_or_else(|said| panic!("{said}"));
        let (_, stopped) = wait_for(&heard, info.id, &["stopped"]);
        let frames = debugger.stack(info.id, stopped["threadId"].as_i64().unwrap()).unwrap();
        assert_eq!(frames[0].line, 3, "{frames:?}");
        assert_eq!(frames[0].path.as_deref().map(Path::new), Some(dir.join("listen.py").as_path()), "{frames:?}");
        debugger.stop(info.id);
        // Letting go of the program leaves it running to its end.
        assert!(listener.wait().unwrap().success());
        let _ = std::fs::remove_dir_all(&dir);
    }

    /// Debugs a Python program with a breakpoint that stops only where its condition holds, one that
    /// writes a message in place of stopping, and a stop on an exception raised.
    #[test]
    #[ignore = "starts debugpy where it is installed"]
    fn debugpy_takes_conditions_messages_and_exceptions() {
        let dir = std::env::temp_dir().join(format!("orior-debug-conditions-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        std::fs::write(dir.join("a.py"), "total = 0\nfor x in range(10):\n    total += x\nprint(total)\ntry:\n    raise ValueError(\"kept\")\nexcept ValueError:\n    pass\n").unwrap();
        let (sender, heard) = mpsc::channel();
        let sender = Mutex::new(sender);
        let emit: Emit = Arc::new(move |id, event, body| {
            let _ = sender.lock().unwrap().send((id, event.to_string(), body));
        });
        let debugger = Debugger::default();
        let breakpoints = vec![Breakpoint { line: 2, condition: "x == 7".into(), ..Breakpoint::default() }, Breakpoint { line: 3, log: "the total is {total}".into(), ..Breakpoint::default() }];
        let options = Options { breakpoints: HashMap::from([("a.py".to_string(), breakpoints)]), exceptions: vec!["raised".into()], ..Options::default() };
        let info = debugger.start(&dir, &Start::File { path: "a.py".into(), language: "python".into() }, &options, emit).unwrap_or_else(|said| panic!("{said}"));
        assert!(info.conditions && info.logs && info.filters.iter().any(|one| one.filter == "raised" && one.on), "{info:?}");
        let (_, stopped) = wait_for(&heard, info.id, &["stopped"]);
        let thread = stopped["threadId"].as_i64().unwrap();
        let frame = debugger.stack(info.id, thread).unwrap()[0].id;
        assert_eq!(debugger.evaluate(info.id, "x", Some(frame), "watch").unwrap().value, "7");
        debugger.step(info.id, "continue", thread).unwrap();
        let (_, logged) = loop {
            let (event, body) = wait_for(&heard, info.id, &["output", "stopped"]);
            if event == "stopped" || body["output"].as_str().is_some_and(|text| text.contains("the total is")) {
                break (event, body);
            }
        };
        assert!(logged["output"].as_str().unwrap_or_default().contains("the total is 45"), "{logged}");
        let (_, raised) = wait_for(&heard, info.id, &["stopped"]);
        assert_eq!(raised["reason"], "exception", "{raised}");
        let thrown = debugger.raised(info.id, raised["threadId"].as_i64().unwrap()).unwrap();
        assert!(thrown.id.contains("ValueError") && thrown.description.contains("kept"), "{thrown:?}");
        debugger.stop_all();
        let _ = std::fs::remove_dir_all(&dir);
    }
}

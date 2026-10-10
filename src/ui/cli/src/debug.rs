// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Debugging one file through a debug adapter: lldb-dap for C, C++ and Rust, gdb where lldb-dap is
//! not found, and debugpy for Python, as each toolchain's `debugger` in toolchains.json names it. A
//! file of a compiled language is first built for debugging, by the `builds` of the toolchain that
//! has its language, into build/debug/, and the program built is what runs.
//!
//! A session goes as the protocol has it: the adapter is started and told who asks, the program is
//! launched, the breakpoints are set once the adapter says it is ready for them, and the program
//! runs. From then on the adapter's events, a stop, the program's output, its end, go to the
//! function the session was started with, and the stack, the variables and the steps are asked for.
//!
//! Lines here count from 0, as the editor counts them, and the adapter is told they count from 1.

use std::collections::HashMap;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::sync::mpsc;
use std::sync::{Arc, Mutex};
use std::time::Duration;

use serde::{Deserialize, Serialize};
use serde_json::{json, Value};

use crate::dap::Adapter;
use crate::toolchains;

/// A toolchain's debug adapter: its program, which is one of the toolchain's or beside them, the
/// words it starts with, the languages it debugs, the arguments of its launch request, in which
/// {program} is the program built, {path} the file's path and {root} the tree's top folder, and the
/// folders it needs on its PATH, by system as `places` writes them, such as the Python whose library
/// lldb-dap loads. A folder that is not there is left out, and the adapter says what it misses.
#[derive(Deserialize, Serialize, Clone, Debug)]
pub struct DebuggerSpec {
    pub program: String,
    pub args: Vec<String>,
    pub languages: Vec<String>,
    pub launch: Value,
    #[serde(default)]
    pub path: HashMap<String, Vec<String>>,
    /// The systems the adapter is used on, all where none are named.
    #[serde(default)]
    pub systems: Vec<String>,
    /// The line that installs the adapter where its toolchain can be present without it.
    #[serde(default)]
    pub setup: Option<String>,
}

/// What the session says on its own: an adapter's event and its body, or the session's end.
pub type Emit = Arc<dyn Fn(&str, Value) + Send + Sync>;

#[derive(Serialize, Clone, Debug)]
pub struct Frame {
    pub id: i64,
    pub name: String,
    pub path: Option<String>,
    pub line: u32,
    pub col: u32,
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
}

#[derive(Serialize, Clone, Debug)]
pub struct Thread {
    pub id: i64,
    pub name: String,
}

/// How long the adapter has to start, to be ready for breakpoints, and to launch the program.
const STARTING: Duration = Duration::from_secs(30);
/// How long a request about the stopped program may take.
const ASKING: Duration = Duration::from_secs(10);
/// How long a build for debugging may take.
const BUILDING: Duration = Duration::from_secs(300);

#[derive(Default)]
pub struct Debugger {
    session: Mutex<Option<Arc<Adapter>>>,
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

fn frame_of(one: &Value) -> Frame {
    Frame {
        id: one["id"].as_i64().unwrap_or(0),
        name: one["name"].as_str().unwrap_or_default().to_string(),
        path: one["source"]["path"].as_str().map(str::to_string),
        line: one["line"].as_u64().unwrap_or(1).saturating_sub(1) as u32,
        col: one["column"].as_u64().unwrap_or(1).saturating_sub(1) as u32,
    }
}

fn variable_of(one: &Value) -> Variable {
    Variable {
        name: one["name"].as_str().unwrap_or_default().to_string(),
        value: one["value"].as_str().or_else(|| one["result"].as_str()).unwrap_or_default().to_string(),
        kind: one["type"].as_str().unwrap_or_default().to_string(),
        reference: one["variablesReference"].as_i64().unwrap_or(0),
    }
}

impl Debugger {
    fn adapter(&self) -> Result<Arc<Adapter>, String> {
        self.session.lock().map_err(|_| "held".to_string())?.clone().ok_or_else(|| "nothing is being debugged".to_string())
    }

    pub fn running(&self) -> bool {
        self.session.lock().map(|session| session.is_some()).unwrap_or(false)
    }

    /// Builds the file at `path` where it is built, starts its language's adapter, sets
    /// `breakpoints`, each file's lines, and runs it. Says which toolchain debugs it.
    pub fn start(&self, root: &Path, path: &Path, language: &str, breakpoints: &HashMap<String, Vec<u32>>, emit: Emit) -> Result<String, String> {
        self.begin(root, path, language, breakpoints, emit, None)
    }

    /// Debugs the Python test uid=197609(Douglas) gid=197609 groups=197609, named as pytest names it, run by pytest with the tree's Python,
    /// with `breakpoints`, each file's lines. Says which toolchain debugs it.
    pub fn start_test(&self, root: &Path, id: &str, breakpoints: &HashMap<String, Vec<u32>>, emit: Emit) -> Result<String, String> {
        let file = id.split("::").next().unwrap_or(id);
        self.begin(root, &root.join(file), "python", breakpoints, emit, Some(id))
    }

    fn begin(&self, root: &Path, path: &Path, language: &str, breakpoints: &HashMap<String, Vec<u32>>, emit: Emit, test: Option<&str>) -> Result<String, String> {
        self.stop();
        let (spec, tool, tool_id, adapter_program) = adapter_for(language)?;
        let program = if test.is_some() { None } else { build(root, path, language, &tool_id)? };
        let names = [
            ("program", program.as_ref().map_or_else(|| path.display().to_string(), |out| out.display().to_string())),
            ("path", path.display().to_string()),
            ("root", root.display().to_string()),
        ];
        let (ready, readied) = mpsc::channel();
        let ready = Mutex::new(Some(ready));
        let told = emit.clone();
        let adapter = Arc::new(Adapter::start(
            &adapter_program,
            &spec.args,
            root,
            toolchains::run_path_with(&toolchains::folders_for(&spec.path)),
            Box::new(move |event, body| {
                if event == "initialized" {
                    if let Some(ready) = ready.lock().ok().and_then(|mut ready| ready.take()) {
                        let _ = ready.send(());
                    }
                }
                told(event, body.clone());
            }),
        )?);
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
            "supportsVariableType": true, "supportsRunInTerminalRequest": false
        });
        adapter.request("initialize", initialize, STARTING).map_err(&failed)?;
        let mut launch = filled(&spec.launch, &names);
        // A test is launched as pytest running it, in place of a program.
        if let (Some(id), Some(map)) = (test, launch.as_object_mut()) {
            map.remove("program");
            map.insert("module".into(), json!("pytest"));
            map.insert("args".into(), json!([id, "-p", "no:cacheprovider", "-q"]));
            if let Some(python) = crate::test_runs::python() {
                map.insert("python".into(), json!(python.display().to_string()));
            }
        }
        let launched = adapter.send("launch", launch).map_err(&failed)?;
        if readied.recv_timeout(STARTING).is_err() {
            if let Ok(Err(said)) = launched.try_recv() {
                return Err(failed(said));
            }
            return Err(failed(format!("{} did not say it was ready", spec.program)));
        }
        for (file, lines) in breakpoints {
            let _ = Self::set(&adapter, &root.join(file), lines);
        }
        let _ = adapter.request("setExceptionBreakpoints", json!({"filters": []}), ASKING);
        adapter.request("configurationDone", Value::Null, ASKING).map_err(&failed)?;
        Adapter::wait("launch", &launched, STARTING).map_err(&failed)?;
        *self.session.lock().map_err(|_| "held".to_string())? = Some(adapter);
        Ok(tool)
    }

    fn set(adapter: &Adapter, file: &Path, lines: &[u32]) -> Result<Vec<(u32, bool)>, String> {
        let breakpoints: Vec<Value> = lines.iter().map(|line| json!({"line": line + 1})).collect();
        let said = adapter.request("setBreakpoints", json!({"source": {"path": file.display().to_string()}, "breakpoints": breakpoints}), ASKING)?;
        Ok(said["breakpoints"]
            .as_array()
            .map(|all| all.iter().zip(lines).map(|(one, &line)| (one["line"].as_u64().map_or(line, |at| at.saturating_sub(1) as u32), one["verified"].as_bool().unwrap_or(false))).collect())
            .unwrap_or_default())
    }

    /// Sets the breakpoints of `file` to `lines` in the running session, and gives each line the
    /// adapter placed one on and whether it is bound to code.
    pub fn breakpoints(&self, file: &Path, lines: &[u32]) -> Result<Vec<(u32, bool)>, String> {
        let adapter = self.adapter()?;
        Self::set(&adapter, file, lines)
    }

    pub fn threads(&self) -> Result<Vec<Thread>, String> {
        let said = self.adapter()?.request("threads", Value::Null, ASKING)?;
        Ok(said["threads"].as_array().map(|all| all.iter().map(|one| Thread { id: one["id"].as_i64().unwrap_or(0), name: one["name"].as_str().unwrap_or_default().to_string() }).collect()).unwrap_or_default())
    }

    pub fn stack(&self, thread: i64) -> Result<Vec<Frame>, String> {
        let said = self.adapter()?.request("stackTrace", json!({"threadId": thread, "startFrame": 0, "levels": 200}), ASKING)?;
        Ok(said["stackFrames"].as_array().map(|all| all.iter().map(frame_of).collect()).unwrap_or_default())
    }

    pub fn scopes(&self, frame: i64) -> Result<Vec<Scope>, String> {
        let said = self.adapter()?.request("scopes", json!({"frameId": frame}), ASKING)?;
        Ok(said["scopes"]
            .as_array()
            .map(|all| {
                all.iter()
                    .map(|one| Scope { name: one["name"].as_str().unwrap_or_default().to_string(), reference: one["variablesReference"].as_i64().unwrap_or(0), expensive: one["expensive"].as_bool().unwrap_or(false) })
                    .collect()
            })
            .unwrap_or_default())
    }

    pub fn variables(&self, reference: i64) -> Result<Vec<Variable>, String> {
        let said = self.adapter()?.request("variables", json!({"variablesReference": reference}), ASKING)?;
        Ok(said["variables"].as_array().map(|all| all.iter().map(variable_of).collect()).unwrap_or_default())
    }

    /// What `expression` comes to in `frame`, as a watch asks it or, with `context` "repl", as the
    /// console does.
    pub fn evaluate(&self, expression: &str, frame: Option<i64>, context: &str) -> Result<Variable, String> {
        // With no frame chosen the frame is left out, which an adapter takes as its global scope.
        let mut arguments = json!({"expression": expression, "context": context});
        if let Some(frame) = frame {
            arguments["frameId"] = json!(frame);
        }
        let said = self.adapter()?.request("evaluate", arguments, ASKING)?;
        let mut found = variable_of(&said);
        found.name = expression.to_string();
        Ok(found)
    }

    /// Continue, next, stepIn, stepOut or pause, for `thread`.
    pub fn step(&self, how: &str, thread: i64) -> Result<(), String> {
        if !["continue", "next", "stepIn", "stepOut", "pause"].contains(&how) {
            return Err(format!("{how} is not a step"));
        }
        self.adapter()?.request(how, json!({"threadId": thread}), ASKING).map(|_| ())
    }

    /// Ends the program and the adapter, where a session is running.
    pub fn stop(&self) {
        let adapter = self.session.lock().ok().and_then(|mut session| session.take());
        if let Some(adapter) = adapter {
            adapter.stop();
        }
    }

    /// Forgets the session after the adapter says the program ended, without asking it again.
    pub fn ended(&self) {
        if let Ok(mut session) = self.session.lock() {
            session.take();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn launch_arguments_take_the_program_and_the_folder() {
        let launch = json!({"program": "{program}", "cwd": "{root}", "args": [], "deep": {"file": "{path}"}});
        let names = [("program", "a.exe".to_string()), ("path", "a.c".to_string()), ("root", "/t".to_string())];
        assert_eq!(filled(&launch, &names), json!({"program": "a.exe", "cwd": "/t", "args": [], "deep": {"file": "a.c"}}));
    }

    #[test]
    fn a_frame_counts_its_line_from_zero() {
        let frame = frame_of(&json!({"id": 7, "name": "main", "source": {"path": "/t/a.c"}, "line": 6, "column": 13}));
        assert_eq!((frame.id, frame.line, frame.col, frame.path.as_deref()), (7, 5, 12, Some("/t/a.c")));
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

    /// Debugs a small program in `language` through its adapter: stops at a breakpoint, reads the
    /// stack and a variable, steps, and runs to the end.
    fn round_trip(name: &str, language: &str, text: &str, stop_line: u32, variable: &str, value: &str) {
        let dir = std::env::temp_dir().join(format!("orior-debug-{language}-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let file = dir.join(name);
        std::fs::write(&file, text).unwrap();
        let (sender, heard) = mpsc::channel();
        let sender = Mutex::new(sender);
        let emit: Emit = Arc::new(move |event, body| {
            let _ = sender.lock().unwrap().send((event.to_string(), body));
        });
        let debugger = Debugger::default();
        let breakpoints = HashMap::from([(name.to_string(), vec![stop_line])]);
        debugger.start(&dir, &file, language, &breakpoints, emit).unwrap_or_else(|said| panic!("{said}"));
        let stopped = loop {
            let (event, body) = heard.recv_timeout(Duration::from_secs(60)).expect("no stop");
            if event == "stopped" {
                break body;
            }
            assert_ne!(event, "terminated", "ended before the breakpoint");
        };
        let thread = stopped["threadId"].as_i64().unwrap_or_else(|| debugger.threads().unwrap()[0].id);
        let frames = debugger.stack(thread).unwrap();
        assert_eq!(frames[0].line, stop_line, "{frames:?}");
        let scopes = debugger.scopes(frames[0].id).unwrap();
        let locals = debugger.variables(scopes[0].reference).unwrap();
        let found = locals.iter().find(|one| one.name == variable).unwrap_or_else(|| panic!("{locals:?}"));
        assert!(found.value.contains(value), "{found:?}");
        assert!(debugger.evaluate(variable, Some(frames[0].id), "watch").unwrap().value.contains(value));
        debugger.step("next", thread).unwrap();
        loop {
            let (event, body) = heard.recv_timeout(Duration::from_secs(60)).expect("no stop after the step");
            if event == "stopped" {
                assert_eq!(debugger.stack(thread).unwrap()[0].line, stop_line + 1, "{body}");
                break;
            }
        }
        debugger.step("continue", thread).unwrap();
        loop {
            let (event, _) = heard.recv_timeout(Duration::from_secs(60)).expect("no end");
            if event == "terminated" || event == "exited" {
                break;
            }
        }
        debugger.stop();
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    #[ignore = "starts lldb-dap and clang where they are installed"]
    fn lldb_debugs_a_c_program() {
        round_trip("a.c", "c", "#include <stdio.h>\nint main(void)\n{\n    int y = 21;\n    y = y * 2;\n    printf(\"%d\\n\", y);\n    return 0;\n}\n", 4, "y", "21");
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
}

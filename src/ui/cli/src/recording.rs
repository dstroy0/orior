// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! A Python program's run recorded as it goes, and a debug adapter, run in orior itself, that steps
//! through the recording backward as well as forward.
//!
//! The recorder is a shim the program runs under. It notes each line of the tree's own code the
//! program runs, before the line runs, with the names of the line's frame whose values changed since
//! the frame's last line, each value as `reprlib` writes it; each call and return of the tree's
//! functions; and each exception raised. A step of the recording is one such line, and the values a
//! stop shows are the frame's as that line was about to run.
//!
//! The adapter answers the requests a debugger's client sends: Continue and Run Backward go to the
//! next or the last step at a breakpoint, or at an exception raised where those stop it; Step Over,
//! Into and Out go forward as a live debugger's do; Step Back goes to the frame's line before, past
//! the calls it made. Two requests of orior's own read a name's values through the run, each where
//! it changed and the line that changed it, and go to any step.

use std::collections::{HashMap, HashSet};
use std::io::{BufRead, BufReader};
use std::path::{Path, PathBuf};
use std::process::{Child, Command, Stdio};
use std::sync::mpsc::Sender;
use std::sync::{Arc, Mutex};

use serde_json::{json, Value};

use crate::dap::Serve;
use crate::toolchains;

/// The shim a recorded run runs under: its out file, the tree's folder, the program, then the
/// program's words.
const RECORDER: &str = r#"
import json, os, reprlib, runpy, sys, threading, traceback
out, root, program, words = sys.argv[1], os.path.normcase(os.path.abspath(sys.argv[2])), sys.argv[3], sys.argv[4:]
MOST = 2000000
shown = reprlib.Repr()
shown.maxstring = shown.maxother = 160
shown.maxlist = shown.maxtuple = shown.maxset = shown.maxfrozenset = shown.maxdict = shown.maxdeque = shown.maxarray = 16
shown.maxlevel = 4
known = {}
def ours(name):
    if name not in known:
        full = os.path.normcase(os.path.abspath(name))
        known[name] = not name.startswith("<") and os.path.isfile(name) and full.startswith(root + os.sep) and not any(part in full.split(os.sep) for part in ("site-packages", ".venv", "venv", "__pycache__"))
    return known[name]
handle = open(out, "w", encoding="utf-8")
lock = threading.Lock()
files, serials, held, threads = {}, {}, {}, {}
state = {"serial": 0, "count": 0, "on": True}
quiet = (type(sys), type(ours), type, type(len))
def write(item):
    handle.write(json.dumps(item, separators=(",", ":")) + "\n")
def file_of(name):
    if name not in files:
        files[name] = len(files)
        write(["file", files[name], os.path.relpath(name, root)])
    return files[name]
def thread_of():
    ident = threading.get_ident()
    if ident not in threads:
        threads[ident] = len(threads) + 1
        write(["thread", threads[ident], threading.current_thread().name])
    return threads[ident]
def values(frame):
    found = {}
    for name, value in list(frame.f_locals.items()):
        if name.startswith("__") or isinstance(value, quiet):
            continue
        try:
            found[name] = shown.repr(value)
        except Exception as error:
            found[name] = "<" + type(error).__name__ + ">"
    return found
def started(frame):
    caller = frame.f_back
    while caller is not None and caller not in serials:
        caller = caller.f_back
    if frame not in serials:
        state["serial"] += 1
        serials[frame] = state["serial"]
    write(["call", serials[frame], serials.get(caller, 0) if caller is not None else 0, file_of(frame.f_code.co_filename), frame.f_code.co_qualname if hasattr(frame.f_code, "co_qualname") else frame.f_code.co_name, thread_of()])
def stepped(frame, line):
    if frame not in serials:
        started(frame)
    serial = serials[frame]
    now = values(frame)
    before = held.get(serial, {})
    changed = {name: text for name, text in now.items() if before.get(name) != text}
    gone = [name for name in before if name not in now]
    held[serial] = now
    write(["line", serial, line, changed, gone])
    state["count"] += 1
    if state["count"] >= MOST:
        state["on"] = False
        write(["cut", state["count"]])
        stop()
def left(frame, value, ended):
    serial = serials.get(frame)
    if serial is None:
        return
    try:
        text = shown.repr(value)
    except Exception as error:
        text = "<" + type(error).__name__ + ">"
    write(["return", serial, text])
    if ended:
        serials.pop(frame, None)
        held.pop(serial, None)
def raised(frame, error):
    serial = serials.get(frame)
    if serial is not None:
        write(["raise", serial, frame.f_lineno, type(error).__name__, str(error)[:400]])
if hasattr(sys, "monitoring"):
    watch = sys.monitoring
    tool = watch.DEBUGGER_ID
    watch.use_tool_id(tool, "orior")
    events = watch.events
    def frame_of(code):
        frame = sys._getframe(2)
        while frame is not None and frame.f_code is not code:
            frame = frame.f_back
        return frame
    def on_start(code, offset):
        if not ours(code.co_filename):
            return watch.DISABLE
        with lock:
            if state["on"]:
                frame = frame_of(code)
                if frame is not None:
                    started(frame)
    def on_line(code, line):
        if not ours(code.co_filename):
            return watch.DISABLE
        with lock:
            if state["on"]:
                frame = frame_of(code)
                if frame is not None:
                    stepped(frame, line)
    def on_return(code, offset, value):
        if not ours(code.co_filename):
            return watch.DISABLE
        with lock:
            if state["on"]:
                frame = frame_of(code)
                if frame is not None:
                    left(frame, value, True)
    def on_yield(code, offset, value):
        if not ours(code.co_filename):
            return watch.DISABLE
        with lock:
            if state["on"]:
                frame = frame_of(code)
                if frame is not None:
                    left(frame, value, False)
    def on_raise(code, offset, error):
        if ours(code.co_filename):
            with lock:
                if state["on"]:
                    frame = frame_of(code)
                    if frame is not None:
                        raised(frame, error)
    def on_unwind(code, offset, error):
        if ours(code.co_filename):
            with lock:
                if state["on"]:
                    frame = frame_of(code)
                    if frame is not None:
                        left(frame, error, True)
    watch.register_callback(tool, events.PY_START, on_start)
    watch.register_callback(tool, events.PY_RESUME, on_start)
    watch.register_callback(tool, events.LINE, on_line)
    watch.register_callback(tool, events.PY_RETURN, on_return)
    watch.register_callback(tool, events.PY_YIELD, on_yield)
    watch.register_callback(tool, events.RAISE, on_raise)
    watch.register_callback(tool, events.PY_UNWIND, on_unwind)
    watch.set_events(tool, events.PY_START | events.PY_RESUME | events.LINE | events.PY_RETURN | events.PY_YIELD | events.RAISE | events.PY_UNWIND)
    def stop():
        watch.set_events(tool, 0)
else:
    def trace(frame, event, arg):
        if not ours(frame.f_code.co_filename):
            return None
        with lock:
            if state["on"]:
                started(frame)
        def lines(frame, event, arg):
            with lock:
                if not state["on"]:
                    return None
                if event == "line":
                    stepped(frame, frame.f_lineno)
                elif event == "return":
                    left(frame, arg, True)
                elif event == "exception":
                    raised(frame, arg[1])
            return lines
        return lines
    sys.settrace(trace)
    threading.settrace(trace)
    def stop():
        sys.settrace(None)
        threading.settrace(None)
sys.argv = [program] + words
sys.path.insert(0, os.path.dirname(os.path.abspath(program)))
code = 0
try:
    runpy.run_path(program, run_name="__main__")
except SystemExit as leaving:
    code = leaving.code if isinstance(leaving.code, int) else (0 if leaving.code is None else 1)
except BaseException:
    traceback.print_exc()
    code = 1
with lock:
    state["on"] = False
    stop()
    write(["end", code])
    handle.close()
sys.stdout.flush()
os._exit(code)
"#;

/// A frame the recording saw: its function's name, its file, the frame that called it, and its
/// thread.
#[derive(Clone, Debug, Default)]
struct Frame {
    name: String,
    file: usize,
    parent: u64,
    thread: u64,
}

/// A step of the recording: a line of a frame about to run, how deep the frame stood, and the
/// names whose values changed since the frame's last line, a name gone with no value.
#[derive(Clone, Debug)]
struct Step {
    frame: u64,
    line: u32,
    depth: u32,
    changes: Vec<(String, Option<String>)>,
}

/// An exception raised: the step it was raised at, its kind and its message.
#[derive(Clone, Debug)]
struct Raised {
    step: usize,
    kind: String,
    message: String,
}

/// A recorded run, read from the recorder's lines.
#[derive(Default, Debug)]
pub struct Recording {
    files: Vec<String>,
    threads: Vec<(u64, String)>,
    frames: HashMap<u64, Frame>,
    steps: Vec<Step>,
    by_frame: HashMap<u64, Vec<usize>>,
    raised: Vec<Raised>,
    /// The program's exit code, and whether the recording stopped short of the run's end.
    pub code: Option<i64>,
    pub cut: bool,
}

impl Recording {
    /// The recording the recorder wrote as `text`.
    pub fn read(text: &str) -> Recording {
        let mut found = Recording::default();
        let mut stacks: HashMap<u64, Vec<u64>> = HashMap::new();
        for line in text.lines() {
            let Ok(Value::Array(item)) = serde_json::from_str::<Value>(line) else {
                continue;
            };
            let number = |at: usize| item.get(at).and_then(Value::as_u64).unwrap_or(0);
            match item.first().and_then(Value::as_str) {
                Some("file") => {
                    let at = number(1) as usize;
                    if found.files.len() <= at {
                        found.files.resize(at + 1, String::new());
                    }
                    found.files[at] = item.get(2).and_then(Value::as_str).unwrap_or_default().replace('\\', "/");
                }
                Some("thread") => found.threads.push((number(1), item.get(2).and_then(Value::as_str).unwrap_or_default().to_string())),
                Some("call") => {
                    let frame = Frame { name: item.get(4).and_then(Value::as_str).unwrap_or_default().to_string(), file: number(3) as usize, parent: number(2), thread: number(5) };
                    let stack = stacks.entry(frame.thread).or_default();
                    if stack.last() != Some(&number(1)) {
                        stack.push(number(1));
                    }
                    found.frames.insert(number(1), frame);
                }
                Some("line") => {
                    let serial = number(1);
                    let thread = found.frames.get(&serial).map_or(0, |frame| frame.thread);
                    let depth = stacks.get(&thread).map_or(1, |stack| stack.iter().rposition(|&one| one == serial).map_or(stack.len(), |at| at + 1)) as u32;
                    let mut changes: Vec<(String, Option<String>)> = item.get(3).and_then(Value::as_object).into_iter().flatten().map(|(name, value)| (name.clone(), value.as_str().map(str::to_string))).collect();
                    changes.extend(item.get(4).and_then(Value::as_array).into_iter().flatten().filter_map(Value::as_str).map(|name| (name.to_string(), None)));
                    found.by_frame.entry(serial).or_default().push(found.steps.len());
                    found.steps.push(Step { frame: serial, line: number(2) as u32, depth, changes });
                }
                Some("return") => {
                    let serial = number(1);
                    let thread = found.frames.get(&serial).map_or(0, |frame| frame.thread);
                    if let Some(stack) = stacks.get_mut(&thread) {
                        if let Some(at) = stack.iter().rposition(|&one| one == serial) {
                            stack.truncate(at);
                        }
                    }
                }
                Some("raise") => {
                    let serial = number(1);
                    let step = found.by_frame.get(&serial).and_then(|steps| steps.last().copied()).unwrap_or(found.steps.len().saturating_sub(1));
                    let text = |at: usize| item.get(at).and_then(Value::as_str).unwrap_or_default().to_string();
                    // An exception passing up through frames is raised again in each; the first is
                    // where it began.
                    if !found.raised.last().is_some_and(|last| last.kind == text(3) && last.message == text(4) && found.steps.get(last.step).is_some_and(|one| one.depth > found.steps.get(step).map_or(0, |here| here.depth))) {
                        found.raised.push(Raised { step, kind: text(3), message: text(4) });
                    }
                }
                Some("cut") => found.cut = true,
                Some("end") => found.code = item.get(1).and_then(Value::as_i64),
                _ => {}
            }
        }
        found
    }

    pub fn len(&self) -> usize {
        self.steps.len()
    }

    pub fn is_empty(&self) -> bool {
        self.steps.is_empty()
    }

    fn thread_of(&self, step: usize) -> u64 {
        self.frames.get(&self.steps[step].frame).map_or(0, |frame| frame.thread)
    }

    /// The tree path of the file a step's line is in.
    fn path_of(&self, step: usize) -> &str {
        self.frames.get(&self.steps[step].frame).and_then(|frame| self.files.get(frame.file)).map_or("", String::as_str)
    }

    /// The last step of frame `serial` at or before step `at`.
    fn last_of(&self, serial: u64, at: usize) -> Option<usize> {
        let steps = self.by_frame.get(&serial)?;
        let found = steps.partition_point(|&one| one <= at);
        found.checked_sub(1).map(|index| steps[index])
    }

    /// The values of frame `serial`'s names as step `at` was about to run, by name.
    fn values(&self, serial: u64, at: usize) -> Vec<(String, String)> {
        let mut held: Vec<(String, String)> = Vec::new();
        for &step in self.by_frame.get(&serial).into_iter().flatten().take_while(|&&step| step <= at) {
            for (name, value) in &self.steps[step].changes {
                held.retain(|(one, _)| one != name);
                if let Some(value) = value {
                    held.push((name.clone(), value.clone()));
                }
            }
        }
        held.sort_by(|a, b| a.0.cmp(&b.0));
        held
    }

    /// The frames standing at step `at`, the innermost first: each frame's number and the step of
    /// its line then.
    fn stack(&self, at: usize) -> Vec<(u64, usize)> {
        let mut found = Vec::new();
        let mut serial = self.steps[at].frame;
        while serial != 0 && found.len() < 512 {
            let Some(step) = self.last_of(serial, at) else {
                break;
            };
            found.push((serial, step));
            serial = self.frames.get(&serial).map_or(0, |frame| frame.parent);
        }
        found
    }

    /// Each step where `name` of frame `serial` took a new value: the step of the line that gave
    /// it, the line, and the value.
    fn history(&self, serial: u64, name: &str) -> Vec<(usize, u32, Option<String>)> {
        let steps = self.by_frame.get(&serial).map(Vec::as_slice).unwrap_or_default();
        let mut found = Vec::new();
        for (index, &step) in steps.iter().enumerate() {
            if let Some((_, value)) = self.steps[step].changes.iter().find(|(one, _)| one == name) {
                // A value seen first at a line was given by the frame's line before it, or by the
                // call, for an argument, where the line is the frame's first.
                let by = if index > 0 { steps[index - 1] } else { step };
                found.push((by, self.steps[by].line, value.clone()));
            }
        }
        found
    }
}

/// Where the replay stands, and what it stops at.
struct Replay {
    root: PathBuf,
    program: PathBuf,
    python: Option<PathBuf>,
    recording: Recording,
    at: usize,
    breakpoints: HashMap<String, HashSet<u32>>,
    on_raised: bool,
    running: Arc<Mutex<Option<Child>>>,
}

/// How a path is compared: whole, with one kind of slash, and without case on Windows.
fn key(path: &Path) -> String {
    let text = path.display().to_string().replace('\\', "/");
    if cfg!(windows) {
        text.to_lowercase()
    } else {
        text
    }
}

impl Replay {
    /// The full path of the file at tree path `relative`, joined a part at a time.
    fn full(&self, relative: &str) -> PathBuf {
        relative.split('/').filter(|part| !part.is_empty()).fold(self.root.clone(), |path, part| path.join(part))
    }

    fn thread(&self) -> u64 {
        if self.recording.is_empty() {
            1
        } else {
            self.recording.thread_of(self.at)
        }
    }

    fn at_breakpoint(&self, step: usize) -> bool {
        let path = self.full(self.recording.path_of(step));
        self.breakpoints.get(&key(&path)).is_some_and(|lines| lines.contains(&self.recording.steps[step].line))
    }

    fn at_raise(&self, step: usize) -> bool {
        self.on_raised && self.recording.raised.iter().any(|raised| raised.step == step)
    }

    /// Where `how` goes from where the replay stands, and the reason it stops there.
    fn go(&self, how: &str) -> (usize, &'static str) {
        let steps = &self.recording.steps;
        let last = steps.len() - 1;
        let here = self.at;
        let thread = self.recording.thread_of(here);
        let depth = steps[here].depth;
        let same = |step: usize| self.recording.thread_of(step) == thread;
        let stop = |step: usize| self.at_breakpoint(step) || self.at_raise(step);
        let reason = |step: usize| if self.at_raise(step) && !self.at_breakpoint(step) { "exception" } else { "breakpoint" };
        match how {
            "continue" => (here + 1..=last).find(|&step| stop(step)).map_or((last, "end"), |step| (step, reason(step))),
            "reverseContinue" => (0..here).rev().find(|&step| stop(step)).map_or((0, "start"), |step| (step, reason(step))),
            "next" => (here + 1..=last).find(|&step| same(step) && steps[step].depth <= depth).map_or((last, "end"), |step| (step, "step")),
            "stepIn" => (here + 1..=last).find(|&step| same(step)).map_or((last, "end"), |step| (step, "step")),
            "stepOut" => (here + 1..=last).find(|&step| same(step) && steps[step].depth < depth).map_or((last, "end"), |step| (step, "step")),
            "stepBack" => (0..here).rev().find(|&step| same(step) && steps[step].depth <= depth).map_or((0, "start"), |step| (step, "step")),
            _ => (here, "pause"),
        }
    }

    fn stopped(&self, reason: &str) -> Value {
        let description = match reason {
            "end" => "the end of the recording".to_string(),
            "start" => "the start of the recording".to_string(),
            _ => format!("step {} of {}", self.at + 1, self.recording.len()),
        };
        let reason = match reason {
            "end" | "start" => "step",
            other => other,
        };
        json!({"type": "event", "event": "stopped", "body": {"reason": reason, "description": description, "threadId": self.thread(), "allThreadsStopped": true}})
    }

    fn frame_json(&self, serial: u64, step: usize) -> Value {
        let frame = self.recording.frames.get(&serial).cloned().unwrap_or_default();
        let path = self.full(self.recording.files.get(frame.file).map_or("", String::as_str));
        json!({"id": serial, "name": frame.name, "line": self.recording.steps[step].line, "column": 1, "source": {"name": path.file_name().map(|name| name.to_string_lossy().into_owned()), "path": path.display().to_string()}})
    }

    /// The frame `id` names, or the innermost where it names none or one not standing.
    fn frame_of(&self, id: Option<i64>) -> u64 {
        let stack = self.recording.stack(self.at);
        id.map(|id| id as u64).filter(|id| stack.iter().any(|(serial, _)| serial == id)).or_else(|| stack.first().map(|(serial, _)| *serial)).unwrap_or(0)
    }

    /// The answer to `command` with `arguments`, or what it fails with.
    fn answer(&mut self, command: &str, arguments: &Value, tell: &Sender<Value>) -> Result<Value, String> {
        let ready = !self.recording.is_empty();
        match command {
            "threads" => Ok(json!({"threads": if self.recording.threads.is_empty() { json!([{"id": 1, "name": "MainThread"}]) } else { Value::Array(self.recording.threads.iter().map(|(id, name)| json!({"id": id, "name": name})).collect()) }})),
            "stackTrace" if ready => {
                let thread = arguments["threadId"].as_u64().unwrap_or(self.thread());
                // A thread other than the step's stands where its own last step left it.
                let at = (0..=self.at).rev().find(|&step| self.recording.thread_of(step) == thread).unwrap_or(self.at);
                let frames: Vec<Value> = self.recording.stack(at).into_iter().map(|(serial, step)| self.frame_json(serial, step)).collect();
                Ok(json!({"stackFrames": frames, "totalFrames": frames.len()}))
            }
            "scopes" if ready => {
                let serial = self.frame_of(arguments["frameId"].as_i64());
                Ok(json!({"scopes": [{"name": "Locals", "presentationHint": "locals", "variablesReference": serial, "expensive": false}]}))
            }
            "variables" if ready => {
                let serial = arguments["variablesReference"].as_u64().unwrap_or(0);
                let values = self.recording.values(serial, self.at);
                Ok(json!({"variables": values.into_iter().map(|(name, value)| json!({"name": name, "value": value, "type": "", "variablesReference": 0})).collect::<Vec<_>>()}))
            }
            "evaluate" if ready => {
                let name = arguments["expression"].as_str().unwrap_or_default().trim();
                let serial = self.frame_of(arguments["frameId"].as_i64());
                // A name of the frame, or of the module that holds it.
                let mut frames = vec![serial];
                frames.extend(self.recording.stack(self.at).into_iter().map(|(one, _)| one).filter(|&one| self.recording.frames.get(&one).is_some_and(|frame| frame.parent == 0)));
                frames.into_iter().find_map(|one| self.recording.values(one, self.at).into_iter().find(|(held, _)| held == name)).map(|(_, value)| json!({"result": value, "type": "", "variablesReference": 0})).ok_or_else(|| format!("a recorded run reads the names its frames held, and {name} is none of them"))
            }
            "exceptionInfo" if ready => {
                let raised = self.recording.raised.iter().find(|raised| raised.step == self.at).ok_or("no exception was raised at this step")?;
                Ok(json!({"exceptionId": raised.kind, "description": raised.message, "breakMode": "always"}))
            }
            "continue" | "next" | "stepIn" | "stepOut" | "stepBack" | "reverseContinue" if ready => {
                let (step, reason) = self.go(command);
                self.at = step;
                let _ = tell.send(json!({"type": "event", "event": "continued", "body": {"threadId": self.thread(), "allThreadsContinued": true}}));
                let _ = tell.send(self.stopped(reason));
                Ok(json!({"allThreadsContinued": true}))
            }
            "pause" => Ok(Value::Null),
            "orior/history" if ready => {
                let serial = self.frame_of(arguments["frameId"].as_i64());
                let name = arguments["name"].as_str().unwrap_or_default();
                let path = self.full(self.recording.frames.get(&serial).and_then(|frame| self.recording.files.get(frame.file)).map_or("", String::as_str));
                let changes: Vec<Value> = self.recording.history(serial, name).into_iter().map(|(step, line, value)| json!({"step": step, "line": line, "path": path.display().to_string(), "value": value, "before": step < self.at})).collect();
                Ok(json!({"changes": changes, "at": self.at}))
            }
            "orior/goto" if ready => {
                let step = arguments["step"].as_u64().map(|step| step as usize).filter(|&step| step < self.recording.len()).ok_or("no such step")?;
                self.at = step;
                let _ = tell.send(self.stopped("goto"));
                Ok(Value::Null)
            }
            _ if !ready => Err("the run is still being recorded".to_string()),
            other => Err(format!("a recorded run does not take {other}")),
        }
    }

    /// Runs the program under the recorder, its output told as it comes, and hands the adapter the
    /// recording when the run ends.
    fn record(&self, tell: &Sender<Value>, itself: &Sender<Value>) -> Result<(), String> {
        let python = self.python.clone().or_else(crate::test_runs::python).ok_or("no Python is found: File, Toolchains takes its folder")?;
        let out = std::env::temp_dir().join(format!("orior-recording-{}-{}.jsonl", std::process::id(), RECORDINGS.fetch_add(1, std::sync::atomic::Ordering::SeqCst)));
        let mut command = Command::new(&python);
        command.arg("-c").arg(RECORDER).arg(&out).arg(&self.root).arg(&self.program).current_dir(&self.root).env("PATH", toolchains::run_path()).env("PYTHONUNBUFFERED", "1").stdin(Stdio::null()).stdout(Stdio::piped()).stderr(Stdio::piped());
        crate::runner::quiet(&mut command);
        let mut child = command.spawn().map_err(|error| format!("{}: {error}", python.display()))?;
        let mut readers = Vec::new();
        for (output, category) in [(child.stdout.take().map(|out| Box::new(out) as Box<dyn std::io::Read + Send>), "stdout"), (child.stderr.take().map(|out| Box::new(out) as Box<dyn std::io::Read + Send>), "stderr")] {
            let Some(output) = output else {
                continue;
            };
            let tell = tell.clone();
            readers.push(std::thread::spawn(move || {
                let mut reader = BufReader::new(output);
                let mut line = String::new();
                while reader.read_line(&mut line).is_ok_and(|read| read > 0) {
                    let _ = tell.send(json!({"type": "event", "event": "output", "body": {"category": category, "output": line}}));
                    line.clear();
                }
            }));
        }
        if let Ok(mut running) = self.running.lock() {
            *running = Some(child);
        }
        let running = self.running.clone();
        let itself = itself.clone();
        std::thread::spawn(move || {
            for reader in readers {
                let _ = reader.join();
            }
            let code = running.lock().ok().and_then(|mut running| running.take()).and_then(|mut child| child.wait().ok()).and_then(|status| status.code());
            let _ = itself.send(json!({"type": "orior", "event": "recorded", "out": out.display().to_string(), "code": code}));
        });
        Ok(())
    }

    /// Takes the recording the recorder wrote, says what it holds, and stops at the first breakpoint
    /// it reaches, or at its first step.
    fn recorded(&mut self, message: &Value, tell: &Sender<Value>) {
        let out = PathBuf::from(message["out"].as_str().unwrap_or_default());
        let text = std::fs::read_to_string(&out).unwrap_or_default();
        let _ = std::fs::remove_file(&out);
        self.recording = Recording::read(&text);
        let code = self.recording.code.or_else(|| message["code"].as_i64());
        let said = match (self.recording.len(), self.recording.cut) {
            (0, _) => "The run ran no line of the tree's own code; there is nothing to step through.\n".to_string(),
            (count, true) => format!("The recording holds the first {count} lines the run ran, and stops there.\n"),
            (count, false) => format!("The recording holds the {count} lines the run ran; it ended with exit code {}.\n", code.map_or("unknown".to_string(), |code| code.to_string())),
        };
        let _ = tell.send(json!({"type": "event", "event": "output", "body": {"category": "console", "output": said}}));
        if self.recording.is_empty() {
            let _ = tell.send(json!({"type": "event", "event": "terminated", "body": {}}));
            return;
        }
        let first = (0..self.recording.len()).find(|&step| self.at_breakpoint(step) || self.at_raise(step));
        self.at = first.unwrap_or(0);
        let reason = match first {
            Some(step) if self.at_raise(step) && !self.at_breakpoint(step) => "exception",
            Some(_) => "breakpoint",
            None => "start",
        };
        let _ = tell.send(self.stopped(reason));
    }
}

static RECORDINGS: std::sync::atomic::AtomicU64 = std::sync::atomic::AtomicU64::new(0);

/// The adapter that records a Python program's run in the tree at `root` and steps through it.
pub fn serve(root: PathBuf) -> Serve {
    let mut replay = Replay { root, program: PathBuf::new(), python: None, recording: Recording::default(), at: 0, breakpoints: HashMap::new(), on_raised: false, running: Arc::new(Mutex::new(None)) };
    let mut seq = 0u64;
    Box::new(move |message: Value, tell: &Sender<Value>, itself: &Sender<Value>| {
        if message["type"] == "orior" {
            replay.recorded(&message, tell);
            return true;
        }
        let command = message["command"].as_str().unwrap_or_default().to_string();
        let arguments = &message["arguments"];
        let mut answered = |result: Result<Value, String>| {
            seq += 1;
            let mut reply = json!({"seq": seq, "type": "response", "request_seq": message["seq"], "command": command, "success": result.is_ok()});
            match result {
                Ok(body) => reply["body"] = body,
                Err(said) => reply["message"] = json!(said),
            }
            let _ = tell.send(reply);
        };
        match command.as_str() {
            "initialize" => {
                answered(Ok(json!({
                    "supportsConfigurationDoneRequest": true,
                    "supportsStepBack": true,
                    "supportsExceptionInfoRequest": true,
                    "supportsEvaluateForHovers": true,
                    "exceptionBreakpointFilters": [{"filter": "raised", "label": "Raised Exceptions", "default": false}]
                })));
            }
            "launch" => {
                replay.program = PathBuf::from(arguments["program"].as_str().unwrap_or_default());
                replay.python = arguments["python"].as_str().filter(|python| !python.is_empty()).map(PathBuf::from);
                answered(Ok(Value::Null));
                let _ = tell.send(json!({"type": "event", "event": "initialized"}));
            }
            "setBreakpoints" => {
                let path = PathBuf::from(arguments["source"]["path"].as_str().unwrap_or_default());
                let lines: HashSet<u32> = arguments["breakpoints"].as_array().into_iter().flatten().filter_map(|one| one["line"].as_u64()).map(|line| line as u32).collect();
                let placed: Vec<Value> = arguments["breakpoints"].as_array().into_iter().flatten().enumerate().map(|(at, one)| json!({"id": at + 1, "verified": true, "line": one["line"]})).collect();
                replay.breakpoints.insert(key(&path), lines);
                answered(Ok(json!({"breakpoints": placed})));
            }
            "setExceptionBreakpoints" => {
                replay.on_raised = arguments["filters"].as_array().is_some_and(|all| all.iter().any(|one| one == "raised"));
                answered(Ok(Value::Null));
            }
            "configurationDone" => {
                let started = replay.record(tell, itself);
                let failed = started.is_err();
                answered(started.map(|()| Value::Null));
                if failed {
                    let _ = tell.send(json!({"type": "event", "event": "terminated", "body": {}}));
                }
            }
            "disconnect" | "terminate" => {
                if let Ok(mut running) = replay.running.lock() {
                    if let Some(mut child) = running.take() {
                        let _ = child.kill();
                        let _ = child.wait();
                    }
                }
                answered(Ok(Value::Null));
                return command != "disconnect";
            }
            _ => {
                let result = replay.answer(&command, arguments, tell);
                answered(result);
            }
        }
        true
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A recording of the recorder's lines for a function `grow` called twice from the module.
    fn sample() -> Recording {
        Recording::read(concat!(
            "[\"thread\",1,\"MainThread\"]\n",
            "[\"file\",0,\"a.py\"]\n",
            "[\"call\",1,0,0,\"<module>\",1]\n",
            "[\"line\",1,1,{},[]]\n",
            "[\"line\",1,5,{},[]]\n",
            "[\"call\",2,1,0,\"grow\",1]\n",
            "[\"line\",2,2,{\"n\":\"1\"},[]]\n",
            "[\"line\",2,3,{\"n\":\"2\"},[]]\n",
            "[\"return\",2,\"2\"]\n",
            "[\"line\",1,6,{\"a\":\"2\"},[]]\n",
            "[\"call\",3,1,0,\"grow\",1]\n",
            "[\"line\",3,2,{\"n\":\"2\"},[]]\n",
            "[\"line\",3,3,{\"n\":\"3\"},[]]\n",
            "[\"raise\",3,3,\"ValueError\",\"too big\"]\n",
            "[\"return\",3,\"None\"]\n",
            "[\"line\",1,7,{\"b\":\"3\"},[]]\n",
            "[\"end\",0]\n",
        ))
    }

    #[test]
    fn a_recording_keeps_each_line_with_its_frame_s_depth_and_values() {
        let found = sample();
        assert_eq!(found.len(), 8);
        assert_eq!(found.steps.iter().map(|step| step.depth).collect::<Vec<_>>(), vec![1, 1, 2, 2, 1, 2, 2, 1]);
        assert_eq!(found.values(1, 7), vec![("a".to_string(), "2".to_string()), ("b".to_string(), "3".to_string())]);
        assert_eq!(found.values(2, 3), vec![("n".to_string(), "2".to_string())]);
        assert_eq!(found.code, Some(0));
        assert_eq!(found.raised.len(), 1);
        assert_eq!(found.raised[0].step, 6);
    }

    #[test]
    fn a_stack_stands_each_caller_at_its_line_then() {
        let found = sample();
        assert_eq!(found.stack(3), vec![(2, 3), (1, 1)]);
        assert_eq!(found.stack(4), vec![(1, 4)]);
    }

    #[test]
    fn a_name_s_history_gives_each_line_that_changed_it() {
        let found = sample();
        // n is given by the call at the first line, then by line 2 for the second value.
        assert_eq!(found.history(2, "n"), vec![(2, 2, Some("1".to_string())), (2, 2, Some("2".to_string()))]);
        assert_eq!(found.history(1, "a"), vec![(1, 5, Some("2".to_string()))]);
    }

    fn replay(at: usize) -> Replay {
        Replay { root: PathBuf::from("/tree"), program: PathBuf::new(), python: None, recording: sample(), at, breakpoints: HashMap::new(), on_raised: false, running: Arc::new(Mutex::new(None)) }
    }

    #[test]
    fn steps_go_over_into_out_and_back() {
        let mut replay = replay(1);
        assert_eq!(replay.go("next"), (4, "step"));
        assert_eq!(replay.go("stepIn"), (2, "step"));
        replay.at = 3;
        assert_eq!(replay.go("stepOut"), (4, "step"));
        replay.at = 4;
        assert_eq!(replay.go("stepBack"), (1, "step"));
        replay.at = 7;
        assert_eq!(replay.go("next"), (7, "end"));
    }

    #[test]
    fn continue_and_run_backward_stop_at_breakpoints_and_raises() {
        let mut replay = replay(0);
        replay.breakpoints.insert(key(Path::new("/tree/a.py")), HashSet::from([2]));
        assert_eq!(replay.go("continue"), (2, "breakpoint"));
        replay.at = 2;
        assert_eq!(replay.go("continue"), (5, "breakpoint"));
        replay.at = 7;
        assert_eq!(replay.go("reverseContinue"), (5, "breakpoint"));
        replay.on_raised = true;
        replay.at = 5;
        assert_eq!(replay.go("continue"), (6, "exception"));
    }

    #[test]
    #[ignore = "runs Python where it is installed"]
    fn a_python_run_is_recorded_and_stepped_back_through() {
        let dir = std::env::temp_dir().join(format!("orior-recording-test-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        std::fs::write(dir.join("r.py"), "def grow(n):\n    n += 1\n    return n\n\n\ntotal = 0\nfor x in range(3):\n    total = grow(total)\nprint(\"total\", total)\n").unwrap();
        let python = crate::test_runs::python().expect("a Python");
        let out = dir.join("r.jsonl");
        let status = Command::new(python).arg("-c").arg(RECORDER).arg(&out).arg(&dir).arg(dir.join("r.py")).current_dir(&dir).status().unwrap();
        assert!(status.success());
        let found = Recording::read(&std::fs::read_to_string(&out).unwrap());
        assert_eq!(found.code, Some(0));
        let module = found.steps[0].frame;
        let changes = found.history(module, "total");
        assert_eq!(changes.iter().map(|(_, line, value)| (*line, value.clone().unwrap_or_default())).collect::<Vec<_>>(), vec![(6, "0".to_string()), (8, "1".to_string()), (8, "2".to_string()), (8, "3".to_string())]);
        assert!(found.steps.iter().any(|step| step.depth == 2 && step.line == 2));
        let _ = std::fs::remove_dir_all(&dir);
    }
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! A profiler for a Python program, on this machine or on another one reached by `ssh`.
//!
//! The program runs under a shim that looks at every thread's stack every EVERY seconds, counting
//! each stack it sees. At each look it reads the process's own memory, its private bytes on Windows
//! and its resident pages elsewhere, and puts its growth since the look before to the frames the
//! program's main thread, or the first of its threads, held at both looks, outermost first as far as
//! the two agree. The program runs at its own speed under it, where tracing each allocation would slow
//! it many times over; memory taken and kept within one look goes to the frames around it. Every
//! SEND seconds, and once the program ends, it writes the counts so far on a line of its own that
//! starts with MARK, among the program's own lines. On another
//! machine the shim goes to its Python on its input, and the lines come back on `ssh`'s output.
//!
//! A frame of a stack is the function's name, its file, as a path under the folder the program ran
//! in where it is under it, and the line it starts at; a stack is its frames, the outermost first,
//! each on a line of its own.

use std::io::{BufRead, BufReader, Write};
use std::path::Path;
use std::process::{Child, Command, Stdio};
use std::sync::{Arc, Mutex};

use serde::Serialize;
use serde_json::Value;

/// The start of the lines the shim writes its counts on.
const MARK: &str = "\u{1e}orior-profile ";

/// The shim: the folder the program runs in, the program, then the program's words, or with "-" in
/// place of the folder, the folder it runs in.
const SHIM: &str = r#"
import json, os, runpy, sys, threading, time, traceback
MARK = "\x1eorior-profile "
EVERY = 0.005
SEND = 0.5
root = os.path.normcase(os.path.abspath(os.getcwd() if sys.argv[1] == "-" else sys.argv[1]))
program, words = os.path.abspath(sys.argv[2]), sys.argv[3:]
time_stacks, memory_stacks, lock, out = {}, {}, threading.Lock(), sys.stdout
state = {"samples": 0, "on": True}
def place(name):
    full = os.path.normcase(os.path.abspath(name))
    return os.path.relpath(full, root).replace(os.sep, "/") if full.startswith(root + os.sep) else name.replace(os.sep, "/")
labels = {}
def label(code):
    if code not in labels:
        name = getattr(code, "co_qualname", code.co_name)
        labels[code] = name + "\t" + place(code.co_filename) + "\t" + str(code.co_firstlineno)
    return labels[code]
target = os.path.normcase(program)
def stack_of(frame):
    parts = []
    while frame is not None:
        parts.append(frame)
        frame = frame.f_back
    parts.reverse()
    for at, one in enumerate(parts):
        if os.path.normcase(os.path.abspath(one.f_code.co_filename)) == target:
            return "\n".join(label(two.f_code) for two in parts[at:])
    return None
def reader():
    if sys.platform == "win32":
        import ctypes
        from ctypes import wintypes
        class Counters(ctypes.Structure):
            _fields_ = [("cb", wintypes.DWORD), ("faults", wintypes.DWORD)] + [(name, ctypes.c_size_t) for name in ("peak_set", "set", "peak_paged", "paged", "peak_unpaged", "unpaged", "private", "peak_private")]
        counters = Counters()
        counters.cb = ctypes.sizeof(Counters)
        current = ctypes.windll.kernel32.GetCurrentProcess
        current.restype = wintypes.HANDLE
        query = ctypes.windll.psapi.GetProcessMemoryInfo
        query.argtypes = [wintypes.HANDLE, ctypes.POINTER(Counters), wintypes.DWORD]
        process = current()
        def read():
            query(process, ctypes.byref(counters), counters.cb)
            return counters.private
        return read
    if os.path.exists("/proc/self/statm"):
        size = os.sysconf("SC_PAGE_SIZE")
        def read():
            with open("/proc/self/statm") as handle:
                return int(handle.read().split()[1]) * size
        return read
    import resource
    scale = 1 if sys.platform == "darwin" else 1024
    return lambda: resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * scale
read_memory = reader()
main = threading.main_thread().ident
def sample():
    me = threading.get_ident()
    last, before = read_memory(), None
    while state["on"]:
        grown_on = None
        for ident, frame in sys._current_frames().items():
            if ident == me:
                continue
            key = stack_of(frame)
            if key:
                if grown_on is None or ident == main:
                    grown_on = key
                with lock:
                    time_stacks[key] = time_stacks.get(key, 0) + 1
                    state["samples"] += 1
        now = read_memory()
        if now > last and grown_on:
            put = grown_on
            if before and before != grown_on:
                shared = []
                for one, two in zip(before.split("\n"), grown_on.split("\n")):
                    if one != two:
                        break
                    shared.append(one)
                put = "\n".join(shared) or grown_on
            with lock:
                memory_stacks[put] = memory_stacks.get(put, 0) + now - last
        last, before = now, grown_on
        time.sleep(EVERY)
started = time.perf_counter()
def send(done=False, code=None):
    with lock:
        said = {"time": dict(time_stacks), "memory": dict(memory_stacks), "samples": state["samples"], "seconds": time.perf_counter() - started}
    if done:
        said["done"] = True
        said["code"] = code
    out.write(MARK + json.dumps(said) + "\n")
    out.flush()
def sender():
    while state["on"]:
        time.sleep(SEND)
        if state["on"]:
            send()
threading.Thread(target=sample, daemon=True).start()
threading.Thread(target=sender, daemon=True).start()
sys.argv = [program] + words
sys.path.insert(0, os.path.dirname(program))
code = 0
try:
    runpy.run_path(program, run_name="__main__")
except SystemExit as leaving:
    code = leaving.code if isinstance(leaving.code, int) else (0 if leaving.code is None else 1)
except BaseException:
    traceback.print_exc()
    code = 1
state["on"] = False
sys.stdout.flush()
send(True, code)
os._exit(code)
"#;

/// What a profile tells as it goes: a line the program wrote, the counts so far, and its end.
#[derive(Serialize, Clone, Debug)]
#[serde(tag = "kind", rename_all = "lowercase")]
pub enum Heard {
    Output { text: String, error: bool },
    Counts { counts: Value },
    Done { code: Option<i64>, said: String },
}

pub type Tell = Arc<dyn Fn(Heard) + Send + Sync>;

/// The profile running, where one is.
#[derive(Default)]
pub struct Profiles {
    running: Arc<Mutex<Option<Child>>>,
}

impl Profiles {
    /// Profiles the Python file at `file`, a path under `root`, on this machine, or where `remote`
    /// names a machine, as `user@host:folder`, on it, in that folder, by the same path.
    pub fn start(&self, root: &Path, file: &str, remote: Option<&str>, tell: Tell) -> Result<(), String> {
        self.stop();
        let mut command = match remote.filter(|remote| !remote.trim().is_empty()) {
            Some(remote) => {
                let (host, folder) = remote.split_once(':').ok_or_else(|| format!("{remote} names no folder: user@host:folder"))?;
                let mut command = Command::new("ssh");
                let quoted = |text: &str| format!("'{}'", text.replace('\'', "'\\''"));
                command.args(["-o", "BatchMode=yes", host, &format!("cd {} && exec python3 - - {}", quoted(folder), quoted(file))]);
                command
            }
            None => {
                let python = crate::test_runs::python().ok_or("no Python is found: File, Toolchains takes its folder")?;
                let mut command = Command::new(python);
                command.arg("-").arg(root).arg(crate::root::full(root, file)).current_dir(root).env("PATH", crate::toolchains::run_path());
                command
            }
        };
        command.env("PYTHONUNBUFFERED", "1").stdin(Stdio::piped()).stdout(Stdio::piped()).stderr(Stdio::piped());
        crate::runner::quiet(&mut command);
        let mut child = command.spawn().map_err(|error| error.to_string())?;
        if let Some(mut input) = child.stdin.take() {
            input.write_all(SHIM.as_bytes()).map_err(|error| error.to_string())?;
        }
        let output = child.stdout.take();
        let errors = child.stderr.take();
        *self.running.lock().map_err(|_| "held".to_string())? = Some(child);
        let told = tell.clone();
        let errors = errors.map(|errors| {
            std::thread::spawn(move || {
                for line in BufReader::new(errors).lines().map_while(Result::ok) {
                    told(Heard::Output { text: line, error: true });
                }
            })
        });
        let running = self.running.clone();
        std::thread::spawn(move || {
            let mut code = None;
            if let Some(output) = output {
                for line in BufReader::new(output).lines().map_while(Result::ok) {
                    match line.strip_prefix(MARK) {
                        Some(json) => {
                            let counts: Value = serde_json::from_str(json).unwrap_or(Value::Null);
                            if counts["done"] == true {
                                code = counts["code"].as_i64();
                            }
                            tell(Heard::Counts { counts });
                        }
                        None => tell(Heard::Output { text: line, error: false }),
                    }
                }
            }
            if let Some(errors) = errors {
                let _ = errors.join();
            }
            let status = running.lock().ok().and_then(|mut running| running.take()).and_then(|mut child| child.wait().ok());
            let code = code.or_else(|| status.and_then(|status| status.code()).map(i64::from));
            let said = match code {
                Some(0) => "The run ended.".to_string(),
                Some(code) => format!("The run ended with exit code {code}."),
                None => "The run was stopped.".to_string(),
            };
            tell(Heard::Done { code, said });
        });
        Ok(())
    }

    pub fn stop(&self) {
        if let Some(mut child) = self.running.lock().ok().and_then(|mut running| running.take()) {
            let _ = child.kill();
            let _ = child.wait();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::mpsc;
    use std::time::Duration;

    #[test]
    #[ignore = "runs Python where it is installed"]
    fn a_python_run_is_profiled_by_function_in_time_and_memory() {
        let dir = std::env::temp_dir().join(format!("orior-profile-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        std::fs::write(dir.join("p.py"), "def busy():\n    total = 0\n    for n in range(3_000_000):\n        total += n * n\n    return total\n\n\ndef hold():\n    return [\" \".join(str(n + k) for k in range(8)) for n in range(200_000)]\n\n\nkept = hold()\nprint(\"busy\", busy())\n").unwrap();
        let (sender, heard) = mpsc::channel();
        let sender = Mutex::new(sender);
        let tell: Tell = Arc::new(move |one| {
            let _ = sender.lock().unwrap().send(one);
        });
        let profiles = Profiles::default();
        profiles.start(&dir, "p.py", None, tell).unwrap();
        let mut last = Value::Null;
        let mut printed = String::new();
        loop {
            match heard.recv_timeout(Duration::from_secs(60)).unwrap() {
                Heard::Counts { counts } => last = counts,
                Heard::Output { text, .. } => printed.push_str(&text),
                Heard::Done { code, .. } => {
                    assert_eq!(code, Some(0));
                    break;
                }
            }
        }
        assert!(printed.contains("busy"), "{printed}");
        assert_eq!(last["done"], true);
        let time = last["time"].as_object().unwrap();
        let busy: u64 = time.iter().filter(|(stack, _)| stack.lines().last().is_some_and(|frame| frame.starts_with("busy\tp.py\t1"))).map(|(_, count)| count.as_u64().unwrap()).sum();
        assert!(busy > 10, "{time:?}");
        let memory = last["memory"].as_object().unwrap();
        let held: u64 = memory.iter().filter(|(stack, _)| stack.lines().any(|frame| frame.starts_with("hold\tp.py\t8"))).map(|(_, size)| size.as_u64().unwrap()).sum();
        assert!(held > 10_000_000, "{memory:?}");
        let _ = std::fs::remove_dir_all(&dir);
    }
}

// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Jupyter's kernels, in any language the machine has one for: found where Jupyter keeps their
//! specs and in the Pythons orior finds, started with a connection file of orior's, and spoken to
//! by Jupyter's messaging protocol over ZMTP, each message signed with the kernel's key.
//!
//! A kernel's specs are read in Jupyter's order, the first of a name standing: the folders
//! JUPYTER_PATH names, the reader's own Jupyter folder, each Python's own, and the system's. A
//! Python orior finds with ipykernel in it and no spec of its own is a kernel too; one without
//! ipykernel is listed with the line that installs it.
//!
//! An interrupt reaches a kernel as its spec asks: by a message on its control socket, or else by
//! SIGINT, or on Windows by the event ipykernel waits on, which orior makes and names in
//! JPY_INTERRUPT_EVENT as it starts the kernel. A kernel that does not stop when asked is ended.

use std::collections::{BTreeMap, HashMap};
use std::fs;
use std::hash::{BuildHasher, Hasher};
use std::io::{BufRead, BufReader};
use std::path::{Path, PathBuf};
use std::process::{Child, Command, Stdio};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::mpsc::{self, Receiver, Sender};
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

use serde::{Deserialize, Serialize};
use serde_json::{json, Value};

use crate::digest::hmac_sha256;
use crate::zmtp::Socket;

/// A kernel a notebook can run on.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct Spec {
    pub name: String,
    pub display_name: String,
    pub language: String,
    pub argv: Vec<String>,
    #[serde(default)]
    pub interrupt_mode: String,
    #[serde(default)]
    pub env: BTreeMap<String, String>,
    /// The folder the spec is read from, which `{resource_dir}` in its words names.
    #[serde(default)]
    pub folder: String,
    /// The line that installs what the kernel lacks, where it lacks something.
    #[serde(default)]
    pub install: Option<String>,
}

/// The folders Jupyter keeps its data in, in the order it reads them, and the Pythons whose own
/// folders follow the reader's.
fn data_folders(pythons: &[PathBuf]) -> Vec<PathBuf> {
    let mut folders: Vec<PathBuf> = std::env::var_os("JUPYTER_PATH").map(|path| std::env::split_paths(&path).collect()).unwrap_or_default();
    let home = |name: &str| std::env::var_os(name).map(PathBuf::from);
    if cfg!(windows) {
        folders.extend(home("APPDATA").map(|folder| folder.join("jupyter")));
    } else if cfg!(target_os = "macos") {
        folders.extend(home("HOME").map(|folder| folder.join("Library").join("Jupyter")));
    } else {
        folders.extend(home("XDG_DATA_HOME").or_else(|| home("HOME").map(|folder| folder.join(".local").join("share"))).map(|folder| folder.join("jupyter")));
    }
    folders.extend(pythons.iter().filter_map(|python| python_prefix(python)).map(|prefix| prefix.join("share").join("jupyter")));
    if cfg!(windows) {
        folders.extend(home("PROGRAMDATA").map(|folder| folder.join("jupyter")));
    } else {
        folders.extend(["/usr/local/share/jupyter", "/usr/share/jupyter"].map(PathBuf::from));
    }
    folders
}

/// The prefix a Python's program stands in: its own folder on Windows, or the folder above its bin
/// or Scripts folder.
fn python_prefix(python: &Path) -> Option<PathBuf> {
    let folder = python.parent()?;
    let name = folder.file_name()?.to_string_lossy().to_lowercase();
    Some(if name == "bin" || name == "scripts" { folder.parent()?.to_path_buf() } else { folder.to_path_buf() })
}

/// Whether a Python holds ipykernel, by the package's folder in its site-packages.
fn holds_ipykernel(python: &Path) -> bool {
    let Some(prefix) = python_prefix(python) else { return false };
    let windows = prefix.join("Lib").join("site-packages").join("ipykernel");
    let unix = fs::read_dir(prefix.join("lib")).into_iter().flatten().flatten().any(|entry| entry.path().join("site-packages").join("ipykernel").is_dir());
    windows.is_dir() || unix
}

/// The Pythons a notebook can run on: the tree's environment's, then the one the toolchains find.
fn pythons() -> Vec<PathBuf> {
    let mut found: Vec<PathBuf> = crate::toolchains::environment().and_then(|env| env.python).into_iter().collect();
    if let Ok(python) = crate::executables::program("python", "python") {
        if !found.contains(&python) {
            found.push(python);
        }
    }
    found
}

/// Every kernel the machine has, each name once, in Jupyter's order.
pub fn specs() -> Vec<Spec> {
    let pythons = pythons();
    let mut found: Vec<Spec> = Vec::new();
    for folder in data_folders(&pythons) {
        let mut names: Vec<PathBuf> = fs::read_dir(folder.join("kernels")).into_iter().flatten().flatten().map(|entry| entry.path()).collect();
        names.sort();
        for spec_folder in names {
            let name = spec_folder.file_name().map(|name| name.to_string_lossy().to_string()).unwrap_or_default();
            if found.iter().any(|one| one.name == name) {
                continue;
            }
            let Ok(text) = fs::read_to_string(spec_folder.join("kernel.json")) else { continue };
            let Ok(mut spec) = serde_json::from_str::<Spec>(&{
                let mut value: Value = serde_json::from_str(&text).unwrap_or(Value::Null);
                value["name"] = Value::String(name.clone());
                value.to_string()
            }) else {
                continue;
            };
            spec.folder = spec_folder.display().to_string();
            found.push(spec);
        }
    }
    for (index, python) in pythons.iter().enumerate() {
        let name = if index == 0 && crate::toolchains::environment().is_some() { "python3-tree".to_string() } else { format!("python3-{}", index + 1) };
        let quoted = python.display().to_string();
        let mut spec = Spec {
            name,
            display_name: format!("Python ({quoted})"),
            language: "python".into(),
            argv: vec![quoted.clone(), "-m".into(), "ipykernel_launcher".into(), "-f".into(), "{connection_file}".into()],
            interrupt_mode: "signal".into(),
            env: BTreeMap::new(),
            folder: String::new(),
            install: None,
        };
        if !holds_ipykernel(python) {
            spec.install = Some(format!("\"{quoted}\" -m pip install ipykernel"));
        }
        if !found.iter().any(|one| one.argv.first().is_some_and(|program| Path::new(program) == python.as_path())) {
            found.push(spec);
        }
    }
    found
}

/// A message to or from a kernel, as Jupyter's protocol has one.
#[derive(Clone, Debug, Serialize)]
pub struct Message {
    pub channel: &'static str,
    pub msg_type: String,
    pub msg_id: String,
    pub parent: String,
    pub content: Value,
}

/// A key no other process can guess, from the system's own randomness that seeds Rust's hashes.
fn new_key() -> String {
    (0..4)
        .map(|index| {
            let mut hasher = std::collections::hash_map::RandomState::new().build_hasher();
            hasher.write_u64(index);
            format!("{:016x}", hasher.finish())
        })
        .collect()
}

/// A port on the loopback no one holds now.
fn free_port() -> Result<u16, String> {
    std::net::TcpListener::bind(("127.0.0.1", 0)).and_then(|listener| listener.local_addr()).map(|address| address.port()).map_err(|error| error.to_string())
}

/// Ends a process and every process it started: the whole tree on Windows, as a kernel's own
/// children would outlive it, and the process elsewhere.
fn end_tree(pid: u32) {
    let mut kill = if cfg!(windows) {
        let mut kill = Command::new("taskkill");
        kill.args(["/T", "/F", "/PID", &pid.to_string()]);
        kill
    } else {
        let mut kill = Command::new("kill");
        kill.args(["-KILL", &pid.to_string()]);
        kill
    };
    crate::runner::quiet(&mut kill);
    let _ = kill.stdout(Stdio::null()).stderr(Stdio::null()).status();
}

static NEXT_ID: AtomicU64 = AtomicU64::new(1);

fn new_id(session: &str) -> String {
    format!("{session}_{}", NEXT_ID.fetch_add(1, Ordering::SeqCst))
}

fn now_iso() -> String {
    let since = SystemTime::now().duration_since(UNIX_EPOCH).unwrap_or_default();
    let seconds = since.as_secs();
    let days = (seconds / 86_400) as i64;
    let shifted = days + 719_468;
    let era = shifted.div_euclid(146_097);
    let of_era = shifted - era * 146_097;
    let year_of_era = (of_era - of_era / 1460 + of_era / 36_524 - of_era / 146_096) / 365;
    let day_of_year = of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
    let month_index = (5 * day_of_year + 2) / 153;
    let day = day_of_year - (153 * month_index + 2) / 5 + 1;
    let month = if month_index < 10 { month_index + 3 } else { month_index - 9 };
    let year = year_of_era + era * 400 + i64::from(month <= 2);
    let rest = seconds % 86_400;
    format!("{year:04}-{month:02}-{day:02}T{:02}:{:02}:{:02}.{:06}Z", rest / 3600, rest % 3600 / 60, rest % 60, since.subsec_micros())
}

/// A running kernel, and the sockets orior speaks to it on.
pub struct Kernel {
    pub spec: Spec,
    child: Mutex<Child>,
    shell: Mutex<Socket>,
    control: Mutex<Socket>,
    key: Vec<u8>,
    session: String,
    connection: PathBuf,
    replies: Mutex<HashMap<String, Sender<Message>>>,
    /// The port debugpy listens at in the kernel's process, once it has been asked to.
    debug_port: Mutex<Option<u16>>,
    #[cfg(windows)]
    interrupt_event: isize,
}

/// The kernel's next run of the code `{file}` holds named by that file, in place of the name the
/// kernel gives a cell: a breakpoint set in the file stops it. A run of other code keeps the
/// kernel's own name, and the first run of that code puts the kernel's own naming back.
const DEBUG_NEXT: &str = r#"def _orior_named(raw_code, transformed_code, number, _shell=get_ipython(), _file={file}, _named=type(get_ipython().compile).get_code_name):
    with open(_file, encoding="utf-8") as held:
        meant = held.read()
    if raw_code.replace("\r\n", "\n") != meant.replace("\r\n", "\n"):
        return _named(_shell.compile, raw_code, transformed_code, number)
    del _shell.compile.get_code_name
    return _file
get_ipython().compile.get_code_name = _orior_named
del _orior_named
"#;

/// The parts of a message as they go: its header, parent's header, metadata and content, signed.
fn frames_of(key: &[u8], header: &Value, parent: &Value, content: &Value) -> Vec<Vec<u8>> {
    let parts: Vec<Vec<u8>> = [header, parent, &json!({}), content].iter().map(|part| part.to_string().into_bytes()).collect();
    let signed: Vec<u8> = parts.iter().flatten().copied().collect();
    let mut frames = vec![b"<IDS|MSG>".to_vec(), hmac_sha256(key, &signed).into_bytes()];
    frames.extend(parts);
    frames
}

/// A message's frames read back, its signature checked, past the routing frames before the mark.
fn message_of(key: &[u8], channel: &'static str, frames: &[Vec<u8>]) -> Option<Message> {
    let at = frames.iter().position(|frame| frame == b"<IDS|MSG>")?;
    let parts = frames.get(at + 1..at + 6)?;
    let signed: Vec<u8> = parts[1..5].iter().flatten().copied().collect();
    if !key.is_empty() && hmac_sha256(key, &signed).as_bytes() != parts[0].as_slice() {
        return None;
    }
    let header: Value = serde_json::from_slice(&parts[1]).ok()?;
    let parent: Value = serde_json::from_slice(&parts[2]).ok()?;
    let content: Value = serde_json::from_slice(&parts[4]).ok()?;
    Some(Message {
        channel,
        msg_type: header["msg_type"].as_str().unwrap_or_default().to_string(),
        msg_id: header["msg_id"].as_str().unwrap_or_default().to_string(),
        parent: parent["msg_id"].as_str().unwrap_or_default().to_string(),
        content,
    })
}

/// Connects to a kernel's socket, trying again while the kernel starts and its process stands.
fn connect_while(child: &Mutex<Child>, port: u16, kind: &str, until: Instant) -> Result<Socket, String> {
    loop {
        match Socket::connect("127.0.0.1", port, kind) {
            Ok(socket) => return Ok(socket),
            Err(error) => {
                if let Ok(Some(status)) = child.lock().map_err(|_| ()).and_then(|mut child| child.try_wait().map_err(|_| ())) {
                    return Err(format!("the kernel ended as it started, {status}"));
                }
                if Instant::now() > until {
                    return Err(format!("the kernel's {kind} socket did not open: {error}"));
                }
                std::thread::sleep(Duration::from_millis(100));
            }
        }
    }
}

impl Kernel {
    /// Starts the kernel `spec` in `folder`, and hands each message it publishes, and each reply to a
    /// request, to `heard`. Gives the kernel once it has answered that it is ready.
    pub fn start(spec: &Spec, folder: &Path, heard: Arc<dyn Fn(Message) + Send + Sync>) -> Result<Arc<Kernel>, String> {
        if let Some(install) = &spec.install {
            return Err(format!("{} has no ipykernel: {install} installs it", spec.display_name));
        }
        let key = new_key();
        let ports = [free_port()?, free_port()?, free_port()?, free_port()?, free_port()?];
        let connection = std::env::temp_dir().join(format!("orior-kernel-{}-{}.json", std::process::id(), NEXT_ID.fetch_add(1, Ordering::SeqCst)));
        let info = json!({
            "shell_port": ports[0], "iopub_port": ports[1], "stdin_port": ports[2], "control_port": ports[3], "hb_port": ports[4],
            "ip": "127.0.0.1", "key": key, "transport": "tcp", "signature_scheme": "hmac-sha256", "kernel_name": spec.name,
        });
        fs::write(&connection, info.to_string()).map_err(|error| format!("{}: {error}", connection.display()))?;
        let words: Vec<String> = spec.argv.iter().map(|word| word.replace("{connection_file}", &connection.display().to_string()).replace("{resource_dir}", &spec.folder)).collect();
        let (program, args) = words.split_first().ok_or("the kernel's spec names no program")?;
        let mut cmd = Command::new(program);
        cmd.args(args).current_dir(folder).envs(&spec.env).env("PATH", crate::toolchains::run_path());
        cmd.stdin(Stdio::null()).stdout(Stdio::piped()).stderr(Stdio::piped());
        #[cfg(windows)]
        let interrupt_event = {
            use windows_sys::Win32::Security::SECURITY_ATTRIBUTES;
            use windows_sys::Win32::System::Threading::CreateEventW;
            let attributes = SECURITY_ATTRIBUTES { nLength: std::mem::size_of::<SECURITY_ATTRIBUTES>() as u32, lpSecurityDescriptor: std::ptr::null_mut(), bInheritHandle: 1 };
            // SAFETY: the attributes live through the call, and no name is given.
            let event = unsafe { CreateEventW(&attributes, 0, 0, std::ptr::null()) };
            cmd.env("JPY_INTERRUPT_EVENT", (event as isize).to_string());
            event as isize
        };
        crate::runner::quiet(&mut cmd);
        let mut child = cmd.spawn().map_err(|error| format!("{program}: {error}"))?;
        // What the kernel writes of itself goes on as messages of the log channel.
        for stream in [child.stdout.take().map(|one| Box::new(one) as Box<dyn std::io::Read + Send>), child.stderr.take().map(|one| Box::new(one) as Box<dyn std::io::Read + Send>)].into_iter().flatten() {
            let heard = heard.clone();
            std::thread::spawn(move || {
                for line in BufReader::new(stream).lines().map_while(Result::ok) {
                    heard(Message { channel: "log", msg_type: "log".into(), msg_id: String::new(), parent: String::new(), content: Value::String(line) });
                }
            });
        }
        let child = Mutex::new(child);
        let until = Instant::now() + Duration::from_secs(60);
        let shell = connect_while(&child, ports[0], "DEALER", until)?;
        let control = connect_while(&child, ports[3], "DEALER", until)?;
        let mut iopub = connect_while(&child, ports[1], "SUB", until)?;
        let session = new_key()[..16].to_string();
        let kernel = Arc::new(Kernel {
            spec: spec.clone(),
            child,
            shell: Mutex::new(shell.split()?),
            control: Mutex::new(control.split()?),
            key: key.into_bytes(),
            session,
            connection,
            replies: Mutex::new(HashMap::new()),
            debug_port: Mutex::new(None),
            #[cfg(windows)]
            interrupt_event,
        });
        let key = kernel.key.clone();
        let iopub_heard = heard.clone();
        std::thread::spawn(move || {
            while let Ok(frames) = iopub.receive() {
                if let Some(message) = message_of(&key, "iopub", &frames) {
                    iopub_heard(message);
                }
            }
        });
        for (mut socket, channel) in [(shell, "shell"), (control, "control")] {
            let weak = Arc::downgrade(&kernel);
            let heard = heard.clone();
            let key = kernel.key.clone();
            std::thread::spawn(move || {
                while let Ok(frames) = socket.receive() {
                    let Some(message) = message_of(&key, channel, &frames) else { continue };
                    let waiter = weak.upgrade().and_then(|kernel| kernel.replies.lock().ok().and_then(|mut replies| replies.remove(&message.parent)));
                    match waiter {
                        Some(waiter) => {
                            let _ = waiter.send(message.clone());
                        }
                        None => heard(message),
                    }
                }
            });
        }
        // The kernel is ready once it says what it is; the subscription to what it publishes has
        // had the time to reach it by then.
        let mut ready = None;
        while Instant::now() < until && ready.is_none() {
            ready = kernel.request("shell", "kernel_info_request", json!({}), Duration::from_secs(2)).ok();
        }
        ready.ok_or_else(|| "the kernel did not answer that it is ready".to_string())?;
        Ok(kernel)
    }

    /// Sends a request on the shell or the control socket, and gives its id.
    pub fn send(&self, channel: &str, msg_type: &str, content: Value) -> Result<String, String> {
        let id = new_id(&self.session);
        let header = json!({"msg_id": id, "session": self.session, "username": "orior", "date": now_iso(), "msg_type": msg_type, "version": "5.3"});
        let frames = frames_of(&self.key, &header, &json!({}), &content);
        let socket = if channel == "control" { &self.control } else { &self.shell };
        socket.lock().map_err(|_| "the kernel's socket is held".to_string())?.send(&frames)?;
        Ok(id)
    }

    /// Sends a request and waits up to `wait` for its reply.
    pub fn request(&self, channel: &str, msg_type: &str, content: Value, wait: Duration) -> Result<Message, String> {
        let (tell, hear): (Sender<Message>, Receiver<Message>) = mpsc::channel();
        let id = new_id(&self.session);
        self.replies.lock().map_err(|_| "the kernel's replies are held".to_string())?.insert(id.clone(), tell);
        let header = json!({"msg_id": id, "session": self.session, "username": "orior", "date": now_iso(), "msg_type": msg_type, "version": "5.3"});
        let frames = frames_of(&self.key, &header, &json!({}), &content);
        let socket = if channel == "control" { &self.control } else { &self.shell };
        socket.lock().map_err(|_| "the kernel's socket is held".to_string())?.send(&frames)?;
        let answer = hear.recv_timeout(wait).map_err(|_| format!("the kernel did not answer {msg_type}"));
        if let Ok(mut replies) = self.replies.lock() {
            replies.remove(&id);
        }
        answer
    }

    /// Runs `code`, and gives the id its output and its reply carry as their parent.
    pub fn execute(&self, code: &str) -> Result<String, String> {
        self.send("shell", "execute_request", json!({"code": code, "silent": false, "store_history": true, "user_expressions": {}, "allow_stdin": false, "stop_on_error": true}))
    }

    /// Interrupts what the kernel runs, as its spec asks.
    pub fn interrupt(&self) -> Result<(), String> {
        if self.spec.interrupt_mode == "message" {
            return self.request("control", "interrupt_request", json!({}), Duration::from_secs(5)).map(|_| ());
        }
        #[cfg(windows)]
        {
            // SAFETY: the event is this kernel's own, open while the kernel is.
            let set = unsafe { windows_sys::Win32::System::Threading::SetEvent(self.interrupt_event as _) };
            if set == 0 {
                return Err(std::io::Error::last_os_error().to_string());
            }
            Ok(())
        }
        #[cfg(not(windows))]
        {
            let pid = self.child.lock().map_err(|_| "the kernel is held".to_string())?.id();
            let mut kill = Command::new("kill");
            kill.args(["-INT", &pid.to_string()]);
            kill.status().map(|_| ()).map_err(|error| error.to_string())
        }
    }

    /// Readies the kernel's next run of the code `file` holds to be debugged, under that file's name,
    /// with debugpy listening in the kernel's process from the first time it is asked. Gives the port
    /// debugpy listens at.
    pub fn debug_next(&self, file: &Path) -> Result<u16, String> {
        let mut port = self.debug_port.lock().map_err(|_| "the kernel's debugger is held".to_string())?;
        let named = serde_json::to_string(&file.display().to_string()).map_err(|error| error.to_string())?;
        let listen = if port.is_none() { json!({"port": "__import__('debugpy').listen(('127.0.0.1', 0))[1]"}) } else { json!({}) };
        let asked = json!({"code": DEBUG_NEXT.replace("{file}", &named), "silent": true, "store_history": false, "user_expressions": listen, "allow_stdin": false, "stop_on_error": false});
        let reply = self.request("shell", "execute_request", asked, Duration::from_secs(30))?;
        let failed = |said: &Value| format!("{}: {}", said["ename"].as_str().unwrap_or("error"), said["evalue"].as_str().unwrap_or_default());
        if reply.content["status"] != "ok" {
            return Err(format!("{} cannot name a cell's code as its own file, as IPython 8 and later can: {}", self.spec.display_name, failed(&reply.content)));
        }
        if let Some(port) = *port {
            return Ok(port);
        }
        let said = &reply.content["user_expressions"]["port"];
        if said["status"] != "ok" {
            let python = self.spec.argv.first().cloned().unwrap_or_else(|| "python".to_string());
            return Err(match said["ename"].as_str() {
                Some("ModuleNotFoundError") => format!("{} has no debugpy: \"{python}\" -m pip install debugpy installs it", self.spec.display_name),
                _ => format!("debugpy did not listen in {}: {}", self.spec.display_name, failed(said)),
            });
        }
        let listening = said["data"]["text/plain"].as_str().and_then(|text| text.trim().parse::<u16>().ok()).ok_or("debugpy gave no port it listens at")?;
        *port = Some(listening);
        Ok(listening)
    }

    /// Whether the kernel's process still runs.
    pub fn alive(&self) -> bool {
        self.child.lock().map(|mut child| child.try_wait().map(|status| status.is_none()).unwrap_or(false)).unwrap_or(false)
    }

    /// Asks the kernel to stop, and ends its process where it has not stopped within `wait`. Says
    /// whether it had to be ended.
    pub fn stop(&self, wait: Duration) -> bool {
        let _ = self.send("control", "shutdown_request", json!({"restart": false}));
        let until = Instant::now() + wait;
        while Instant::now() < until {
            if !self.alive() {
                self.forget();
                return false;
            }
            std::thread::sleep(Duration::from_millis(50));
        }
        if let Ok(mut child) = self.child.lock() {
            end_tree(child.id());
            let _ = child.kill();
            let _ = child.wait();
        }
        self.forget();
        true
    }

    fn forget(&self) {
        let _ = fs::remove_file(&self.connection);
        #[cfg(windows)]
        // SAFETY: the event is this kernel's own, and is not used once it has stopped.
        unsafe {
            windows_sys::Win32::Foundation::CloseHandle(self.interrupt_event as _);
        }
    }
}

#[cfg(test)]
mod kernels {
    use super::*;

    #[test]
    fn a_message_is_signed_and_read_back() {
        let key = b"secret";
        let frames = frames_of(key, &json!({"msg_id": "m1", "msg_type": "execute_request"}), &json!({}), &json!({"code": "1"}));
        assert_eq!(frames[0], b"<IDS|MSG>");
        let mut routed = vec![b"route".to_vec()];
        routed.extend(frames.clone());
        let message = message_of(key, "shell", &routed).unwrap();
        assert_eq!((message.msg_type.as_str(), message.content["code"].as_str()), ("execute_request", Some("1")));
        let mut forged = frames;
        forged[5] = json!({"code": "2"}).to_string().into_bytes();
        assert!(message_of(key, "shell", &forged).is_none());
    }

    /// Runs a cell, an error, and a loop it interrupts, on the Python kernel the machine has, where
    /// it has one.
    #[test]
    fn a_python_kernel_runs_cells_and_stops_when_interrupted() {
        let Some(spec) = specs().into_iter().find(|spec| spec.language == "python" && spec.install.is_none()) else { return };
        let heard: Arc<Mutex<Vec<Message>>> = Arc::default();
        let held = heard.clone();
        let kernel = Kernel::start(&spec, &std::env::temp_dir(), Arc::new(move |message| held.lock().unwrap().push(message))).unwrap();
        let outputs = |id: &str| heard.lock().unwrap().iter().filter(|message| message.parent == id && message.channel == "iopub").map(|message| (message.msg_type.clone(), message.content.clone())).collect::<Vec<_>>();
        let wait_idle = |id: &str| {
            let until = Instant::now() + Duration::from_secs(30);
            while Instant::now() < until && !outputs(id).iter().any(|(kind, content)| kind == "status" && content["execution_state"] == "idle") {
                std::thread::sleep(Duration::from_millis(20));
            }
        };
        let first = kernel.execute("print('hi')\n1 + 1").unwrap();
        wait_idle(&first);
        let got = outputs(&first);
        assert!(got.iter().any(|(kind, content)| kind == "stream" && content["text"] == "hi\n"), "{got:?}");
        assert!(got.iter().any(|(kind, content)| kind == "execute_result" && content["data"]["text/plain"] == "2"), "{got:?}");
        let failing = kernel.execute("1 / 0").unwrap();
        wait_idle(&failing);
        assert!(outputs(&failing).iter().any(|(kind, content)| kind == "error" && content["ename"] == "ZeroDivisionError"));
        let looping = kernel.execute("import time\nwhile True:\n    time.sleep(0.05)").unwrap();
        std::thread::sleep(Duration::from_millis(800));
        kernel.interrupt().unwrap();
        wait_idle(&looping);
        assert!(outputs(&looping).iter().any(|(kind, content)| kind == "error" && content["ename"] == "KeyboardInterrupt"), "{:?}", outputs(&looping));
        assert!(!kernel.stop(Duration::from_secs(10)));
    }

    /// The next run of a file's code is named by the file, with debugpy listening; the runs after it,
    /// and a run of other code, are named as the kernel names them.
    #[test]
    fn a_run_readied_to_debug_is_named_by_its_file() {
        let Some(spec) = specs().into_iter().find(|spec| spec.language == "python" && spec.install.is_none()) else { return };
        let heard: Arc<Mutex<Vec<Message>>> = Arc::default();
        let held = heard.clone();
        let folder = std::env::temp_dir().join(format!("orior-debug-cell-{}", std::process::id()));
        fs::create_dir_all(&folder).unwrap();
        let file = folder.join("cell.py");
        let code = "import sys\nsys._getframe().f_code.co_filename";
        fs::write(&file, code).unwrap();
        let kernel = Kernel::start(&spec, &folder, Arc::new(move |message| held.lock().unwrap().push(message))).unwrap();
        let result = |id: &str| {
            let until = Instant::now() + Duration::from_secs(30);
            loop {
                let found = heard.lock().unwrap().iter().find(|message| message.parent == id && matches!(message.msg_type.as_str(), "execute_result" | "error")).map(|message| message.content.clone());
                if let Some(found) = found {
                    return found["data"]["text/plain"].as_str().map(str::to_string).unwrap_or_else(|| found.to_string());
                }
                assert!(Instant::now() < until, "no result for {id}");
                std::thread::sleep(Duration::from_millis(20));
            }
        };
        let port = kernel.debug_next(&file);
        if port.as_ref().is_err_and(|said| said.contains("has no debugpy")) {
            kernel.stop(Duration::from_secs(10));
            return;
        }
        let port = port.unwrap();
        assert!(port > 0);
        let other = kernel.execute("import sys\nsys._getframe().f_code.co_filename + ''").unwrap();
        assert!(!result(&other).contains("cell.py"));
        let debugged = kernel.execute(code).unwrap();
        let named = result(&debugged);
        assert!(named.contains("orior-debug-cell-") && named.contains("cell.py"), "{named}");
        let again = kernel.execute(code).unwrap();
        assert!(!result(&again).contains("cell.py"));
        assert_eq!(kernel.debug_next(&file).unwrap(), port);
        kernel.stop(Duration::from_secs(10));
        let _ = fs::remove_dir_all(&folder);
    }
}

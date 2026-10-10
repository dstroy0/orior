// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The language servers the editor talks to, one for each toolchain whose `server` in
//! toolchains.json names a program, such as clangd for C, C++ and CUDA. A server starts with the
//! first file of its languages the editor opens, at the tree's top folder, and stops when another
//! tree is opened or orior ends. One that cannot be found or does not start is not tried again until
//! the tree changes, and the file opens without it.
//!
//! What the servers say comes back as orior's own shapes: a hover as Markdown, a definition as a
//! path and a place, a usage as a span with its line's text, a completion as the editor's items, a
//! file's diagnostics as spans with a severity, 1 an error, 2 a warning, 3 a note and 4 a hint, and
//! an edit, from a rename, a quick fix or the server's own asking, as each file's spans and their
//! new text. A quick fix goes to the editor whole, as the server gave it, and comes back to be
//! carried out.
//!
//! The tree's check hands every file of the tree the editor does not have open to its language's
//! server, CHECKERS at a time, as the editor would open it, and lets it go once the server has said
//! what is wrong in it, the server's clearing of a file it lets go not passed on. A server that
//! checks its projects as a whole, as rust-analyzer does with cargo check, is told to check them all
//! in place of that. A file is checked again when it changes on the disk, with the files whose
//! includes or imports name it, and when the editor closes it, as the disk holds it.
//!
//! The inspections of inspect.rs read every Python and JavaScript file of the tree once a file of the
//! tree is handed over, on a thread of their own, and each file as the editor changes it, as it
//! changes on the disk, and as the editor closes it. A file's diagnostics go to the editor whole: its
//! server's, then the inspections'. A finding's fix is offered with the server's quick fixes.

use std::collections::{HashMap, HashSet, VecDeque};
use std::path::{Path, PathBuf};
use std::sync::{mpsc, Arc, Condvar, Mutex};
use std::time::Duration;

use serde::{Deserialize, Serialize};
use serde_json::{json, Value};

use crate::inspect;
use crate::lsp::{self, Server};
use crate::toolchains;

/// A toolchain's server: its program, which is one of the toolchain's or beside them, the words it
/// starts with, the languages it serves, the protocol's name for each extension's language, the
/// line that installs it where the toolchain can be present without it, and the name of the file
/// that marks a project, each of which in the tree the server is given as one of its linked
/// projects where it does not look below the top folder for them itself; and, for a server that
/// checks its projects as a whole and not a file at a time, the notification that has it check them.
#[derive(Deserialize, Serialize, Clone, Debug)]
pub struct ServerSpec {
    pub program: String,
    pub args: Vec<String>,
    pub languages: Vec<String>,
    #[serde(default)]
    pub ids: HashMap<String, String>,
    #[serde(default)]
    pub setup: Option<String>,
    #[serde(default)]
    pub projects: Option<String>,
    #[serde(default)]
    pub checks: Option<String>,
}

/// Folders a search for projects does not go into: what a build writes, and what git keeps.
const NOT_PROJECTS: [&str; 4] = ["target", "build", "node_modules", "archive"];

/// How many folders below the top a search for projects goes.
const PROJECT_DEPTH: usize = 6;

/// Every file named `name` in the tree at `root`, outside the folders a build writes.
fn projects_in(root: &Path, name: &str) -> Vec<String> {
    let mut found = Vec::new();
    let mut folders = vec![(root.to_path_buf(), 0usize)];
    while let Some((folder, depth)) = folders.pop() {
        let Ok(entries) = std::fs::read_dir(&folder) else {
            continue;
        };
        for entry in entries.flatten() {
            let path = entry.path();
            let named = entry.file_name().to_string_lossy().to_string();
            if path.is_dir() {
                if depth < PROJECT_DEPTH && !named.starts_with('.') && !NOT_PROJECTS.contains(&named.as_str()) {
                    folders.push((path, depth + 1));
                }
            } else if named == name {
                found.push(path.display().to_string());
            }
        }
    }
    found.sort();
    found
}

#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
pub struct Place {
    pub line: u32,
    pub col: u32,
}

#[derive(Serialize, Clone, Debug)]
pub struct Diagnostic {
    pub from: Place,
    pub to: Place,
    pub severity: u8,
    pub message: String,
    pub source: String,
}

/// A file's diagnostics, the whole of them, as the server last gave them.
#[derive(Serialize, Clone, Debug)]
pub struct Diagnostics {
    pub path: String,
    pub items: Vec<Diagnostic>,
}

#[derive(Serialize, Clone, Debug)]
pub struct Found {
    pub path: String,
    pub line: u32,
    pub col: u32,
}

#[derive(Serialize, Clone, Debug)]
pub struct Item {
    pub label: String,
    pub kind: &'static str,
    pub detail: String,
    pub insert: String,
}

/// A place a symbol is used: its span, and the text of its line.
#[derive(Serialize, Clone, Debug)]
pub struct Usage {
    pub path: String,
    pub from: Place,
    pub to: Place,
    pub text: String,
}

/// One span of a file and what is written over it.
#[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
pub struct TextEdit {
    pub from: Place,
    pub to: Place,
    pub text: String,
}

/// The edits to one file, first in the file first.
#[derive(Serialize, Deserialize, Clone, Debug)]
pub struct FileEdit {
    pub path: String,
    pub edits: Vec<TextEdit>,
}

/// What a rename would change: the span of the name, and the name to start from where the server
/// offers one in place of the span's text.
#[derive(Serialize, Clone, Debug)]
pub struct Renamable {
    pub from: Place,
    pub to: Place,
    pub name: Option<String>,
}

/// A quick fix or a refactoring the server offers, and the server's own form of it, which is given
/// back to carry it out.
#[derive(Serialize, Clone, Debug)]
pub struct Action {
    pub title: String,
    pub kind: String,
    pub preferred: bool,
    pub disabled: Option<String>,
    pub raw: Value,
}

/// A function of the call hierarchy: its name and what the server says of it, its file and the span
/// of its name, the file its calls stand in and where each starts, and the server's or the index's
/// own name for it, which is given back to read the level below it.
#[derive(Serialize, Clone, Debug)]
pub struct Call {
    pub name: String,
    pub detail: String,
    pub path: String,
    pub from: Place,
    pub to: Place,
    pub site: String,
    pub at: Vec<Place>,
    pub item: Value,
}

/// A hint a server writes in the text and not into it: its place, its text, its kind, 1 a type, 2 a
/// parameter's name and 0 any other, and whether a space stands before it and after it.
#[derive(Serialize, Clone, Debug)]
pub struct Hint {
    pub line: u32,
    pub col: u32,
    pub label: String,
    pub kind: u8,
    pub left: bool,
    pub right: bool,
}

/// A call's signature: its text, each parameter's span in it in UTF-16 units, the parameter the
/// cursor is in, and what the server says of it.
#[derive(Serialize, Clone, Debug)]
pub struct Signature {
    pub label: String,
    pub params: Vec<(u32, u32)>,
    pub active: Option<usize>,
    pub doc: String,
    pub count: usize,
}

/// What a server says on its own that the editor is to hear, and how far the tree's check has gone:
/// the files it has checked, of those it was given; and that a server's hints are to be asked for
/// again.
pub enum Told {
    Diagnostics(Diagnostics),
    Edits(Vec<FileEdit>),
    Checking { done: usize, total: usize },
    /// The server's hints are to be asked for again, as it has read more of the tree.
    Hints,
}

/// Where what the servers say on their own goes.
pub type Emit = Arc<dyn Fn(Told) + Send + Sync>;

/// A server running: its toolchain's id, its program's name, its process, and the files it has open.
#[derive(Serialize, Debug, Clone)]
pub struct Running {
    pub tool: String,
    pub program: String,
    pub pid: u32,
    pub files: Vec<String>,
}

const ASKING: Duration = Duration::from_secs(5);

/// How long a search of the whole tree, for usages or a rename, may take.
const SEARCHING: Duration = Duration::from_secs(60);

/// How many files the tree's check has its servers hold at once.
const CHECKERS: usize = 6;

/// How long the tree's check waits for a file's diagnostics, and, once they come, for any the server
/// gives after them.
const CHECK_PATIENCE: Duration = Duration::from_secs(30);
const CHECK_QUIET: Duration = Duration::from_millis(250);

/// The most text a file the tree's check hands to a server may hold.
const CHECK_MOST: u64 = 2 << 20;

/// A file waiting for the tree's check, the server it goes to and the protocol's name for its language.
struct Waiting {
    path: PathBuf,
    server: Arc<Server>,
    id: String,
}

/// The tree's check: whether it is on, the tree's files, the files waiting, and how many of the files
/// it was given are done.
#[derive(Default)]
struct Queue {
    on: bool,
    root: Option<PathBuf>,
    files: Vec<String>,
    waiting: VecDeque<Waiting>,
    total: usize,
    done: usize,
    emit: Option<Emit>,
    started: bool,
}

/// What the tree's check hears, each file by its key: the file whose first diagnostics wake its
/// checker, and the files it let go, whose clearing as each closes is not passed on. Its lock is never
/// held while a server is written to, as the thread that reads a server takes it.
#[derive(Default)]
struct Heard {
    woken: HashMap<String, mpsc::Sender<()>>,
    letting_go: HashSet<String>,
}

#[derive(Default)]
struct Checks {
    queue: Mutex<Queue>,
    ready: Condvar,
    /// The files a server holds for the check and not for the editor. A server is written to about
    /// a file only with this held, by the check or by the editor.
    held: Mutex<HashSet<String>>,
    heard: Mutex<Heard>,
    /// Each file's findings as diagnostics, by its key, with its URI, which go to the editor after
    /// its server's.
    inspected: Published,
    /// What each Python class's family sets, by its file's key and its name.
    supplied: Mutex<HashMap<String, HashMap<String, HashSet<String>>>>,
    /// The servers, by their process, that refuse to be asked for a file's diagnostics, whose
    /// diagnostics the check waits for them to give.
    refused: Mutex<HashSet<u32>>,
}

#[derive(Default)]
pub struct Servers {
    root: Mutex<Option<PathBuf>>,
    running: Mutex<HashMap<String, Arc<Server>>>,
    failed: Mutex<HashMap<String, String>>,
    published: Arc<Published>,
    checks: Arc<Checks>,
    /// The inspections' index of the tree, the tree it was read from, and where their findings go.
    index: Arc<Mutex<inspect::Tree>>,
    indexed: Arc<Mutex<Option<PathBuf>>>,
    told: Mutex<Option<Emit>>,
}

/// Each file's diagnostics as its server last gave them, which a quick fix is asked about, by the
/// file's key, each with the URI the server gave it.
type Published = Mutex<HashMap<String, (String, Vec<Value>)>>;

fn place(value: &Value) -> Place {
    Place { line: value["line"].as_u64().unwrap_or(0) as u32, col: value["character"].as_u64().unwrap_or(0) as u32 }
}

fn text_edit_of(one: &Value) -> TextEdit {
    TextEdit { from: place(&one["range"]["start"]), to: place(&one["range"]["end"]), text: one["newText"].as_str().unwrap_or_default().to_string() }
}

/// A workspace edit as each file's edits, in either of the shapes the protocol allows. A file made,
/// moved or removed is not among them.
pub fn workspace_edit_of(edit: &Value) -> Vec<FileEdit> {
    let mut files: Vec<FileEdit> = Vec::new();
    let mut add = |uri: &str, edits: &Value| {
        let Some(path) = lsp::path_of(uri) else {
            return;
        };
        let path = path.display().to_string();
        let edits = edits.as_array().map(|all| all.iter().map(text_edit_of).collect::<Vec<_>>()).unwrap_or_default();
        match files.iter_mut().find(|file| file.path == path) {
            Some(file) => file.edits.extend(edits),
            None => files.push(FileEdit { path, edits }),
        }
    };
    if let Some(changes) = edit["documentChanges"].as_array() {
        for change in changes {
            if let Some(uri) = change["textDocument"]["uri"].as_str() {
                add(uri, &change["edits"]);
            }
        }
    } else if let Some(changes) = edit["changes"].as_object() {
        for (uri, edits) in changes {
            add(uri, edits);
        }
    }
    for file in &mut files {
        file.edits.sort_by_key(|edit| (edit.from.line, edit.from.col, edit.to.line, edit.to.col));
    }
    files
}

/// The byte where the UTF-16 column `col` of a line falls, the line's end where it is past it.
fn byte_of(line: &str, col: u32) -> usize {
    let mut units = 0u32;
    for (at, char) in line.char_indices() {
        if units >= col {
            return at;
        }
        units += char.len_utf16() as u32;
    }
    line.len()
}

/// `text` with `edits` written over it, each placed against the text as it stood before any. A line
/// ends at its "\n", and a "\r" before that stays the line's end.
pub fn apply(text: &str, edits: &[TextEdit]) -> String {
    let mut starts = vec![0usize];
    starts.extend(text.match_indices('\n').map(|(at, _)| at + 1));
    let offset = |at: &Place| -> usize {
        let Some(&start) = starts.get(at.line as usize) else {
            return text.len();
        };
        let end = starts.get(at.line as usize + 1).map_or(text.len(), |next| next - 1);
        let line = text[start..end].strip_suffix('\r').unwrap_or(&text[start..end]);
        start + byte_of(line, at.col)
    };
    let mut spans: Vec<(usize, usize, &str)> = edits.iter().map(|edit| (offset(&edit.from), offset(&edit.to), edit.text.as_str())).collect();
    spans.sort_by_key(|&(from, to, _)| (from, to));
    let mut out = String::with_capacity(text.len());
    let mut at = 0;
    for (from, to, written) in spans {
        let from = from.max(at);
        out.push_str(&text[at..from]);
        out.push_str(written);
        at = to.max(from);
    }
    out.push_str(&text[at..]);
    out
}

/// The UTF-16 span of `part` in `label`, where it is in it.
fn span_in(label: &str, part: &str) -> Option<(u32, u32)> {
    let at = label.find(part)?;
    let from = label[..at].encode_utf16().count() as u32;
    Some((from, from + part.encode_utf16().count() as u32))
}

fn documentation_of(value: &Value) -> String {
    match value {
        Value::String(text) => text.clone(),
        Value::Object(part) => part.get("value").and_then(Value::as_str).unwrap_or_default().to_string(),
        _ => String::new(),
    }
}

fn signature_of(help: &Value) -> Option<Signature> {
    let all = help["signatures"].as_array()?;
    let chosen = help["activeSignature"].as_u64().unwrap_or(0) as usize;
    let one = all.get(chosen).or_else(|| all.first())?;
    let label = one["label"].as_str()?.to_string();
    let params: Vec<(u32, u32)> = one["parameters"]
        .as_array()
        .map(|all| {
            all.iter()
                .filter_map(|param| match &param["label"] {
                    Value::String(part) => span_in(&label, part),
                    Value::Array(ends) => Some((ends.first()?.as_u64()? as u32, ends.get(1)?.as_u64()? as u32)),
                    _ => None,
                })
                .collect()
        })
        .unwrap_or_default();
    let active = one["activeParameter"].as_u64().or_else(|| help["activeParameter"].as_u64()).map(|at| at as usize).filter(|&at| at < params.len());
    Some(Signature { label, params, active, doc: documentation_of(&one["documentation"]), count: all.len() })
}

fn action_of(one: &Value) -> Option<Action> {
    let title = one["title"].as_str()?.to_string();
    Some(Action {
        title,
        kind: one["kind"].as_str().unwrap_or_default().to_string(),
        preferred: one["isPreferred"].as_bool().unwrap_or(false),
        disabled: one["disabled"]["reason"].as_str().map(str::to_string),
        raw: one.clone(),
    })
}

/// Whether two spans share a place, a span with no width touching the other counting.
fn overlaps(from: &Place, to: &Place, range: &Value) -> bool {
    let start = place(&range["start"]);
    let end = place(&range["end"]);
    let before = |a: &Place, b: &Place| (a.line, a.col) < (b.line, b.col);
    !before(&end, from) && !before(to, &start)
}

/// The text of each line a usage is on, read from its file once.
fn lines_of(path: &str, cache: &mut HashMap<String, Vec<String>>) -> Vec<String> {
    cache.entry(path.to_string()).or_insert_with(|| std::fs::read_to_string(path).map(|text| text.lines().map(str::to_string).collect()).unwrap_or_default()).clone()
}

fn diagnostics_of(params: &Value) -> Option<Diagnostics> {
    let path = lsp::path_of(params["uri"].as_str()?)?;
    let items = params["diagnostics"]
        .as_array()?
        .iter()
        .map(|one| Diagnostic {
            from: place(&one["range"]["start"]),
            to: place(&one["range"]["end"]),
            severity: one["severity"].as_u64().unwrap_or(1) as u8,
            message: one["message"].as_str().unwrap_or_default().to_string(),
            source: one["source"].as_str().unwrap_or_default().to_string(),
        })
        .collect();
    Some(Diagnostics { path: path.display().to_string(), items })
}

/// A hover's contents as Markdown, from any of the shapes the protocol allows.
fn markdown_of(contents: &Value) -> String {
    match contents {
        Value::String(text) => text.clone(),
        Value::Array(parts) => parts.iter().map(markdown_of).filter(|text| !text.is_empty()).collect::<Vec<_>>().join("\n\n"),
        Value::Object(part) => match (part.get("language").and_then(Value::as_str), part.get("value").and_then(Value::as_str)) {
            (Some(language), Some(value)) => format!("```{language}\n{value}\n```"),
            (None, Some(value)) => value.to_string(),
            _ => String::new(),
        },
        _ => String::new(),
    }
}

fn kind_of(kind: u64) -> &'static str {
    match kind {
        2..=4 => "function",
        5 | 10 => "field",
        6 => "variable",
        7 | 8 | 13 | 22 | 25 => "type",
        9 => "module",
        12 | 20 | 21 => "constant",
        14 => "keyword",
        15 => "snippet",
        24 => "operator",
        _ => "word",
    }
}

fn item_of(one: &Value) -> Item {
    let label = one["label"].as_str().unwrap_or_default().trim().trim_start_matches('•').trim().to_string();
    let name = one["filterText"].as_str().map(str::to_string).unwrap_or_else(|| label.split(['(', ' ']).next().unwrap_or_default().to_string());
    let insert = one["textEdit"]["newText"].as_str().or_else(|| one["insertText"].as_str()).map(str::to_string).unwrap_or_else(|| name.clone());
    let detail = one["detail"].as_str().map(|detail| format!("{detail} {label}")).unwrap_or(label);
    Item { label: name, kind: kind_of(one["kind"].as_u64().unwrap_or(1)), detail: detail.trim().to_string(), insert }
}

/// What a server says on its own, and the answer where it asks: its diagnostics kept and passed
/// on, an edit it asks for passed on and said to be made, and each setting it asks for left to its
/// own default.
fn heard(method: &str, params: &Value, emit: &Emit, published: &Published, checks: &Checks) -> Value {
    match method {
        "textDocument/publishDiagnostics" => {
            if let (Some(uri), Some(all)) = (params["uri"].as_str(), params["diagnostics"].as_array()) {
                let key = lsp::key_of(uri);
                if let Ok(mut heard) = checks.heard.lock() {
                    if all.is_empty() && heard.letting_go.remove(&key) {
                        return Value::Null;
                    }
                    if let Some(woken) = heard.woken.get(&key) {
                        let _ = woken.send(());
                    }
                }
                if let Ok(mut kept) = published.lock() {
                    kept.insert(key.clone(), (uri.to_string(), all.clone()));
                }
                emit_whole(&key, uri, published, checks, emit);
            }
            Value::Null
        }
        "workspace/applyEdit" => {
            emit(Told::Edits(workspace_edit_of(&params["edit"])));
            json!({"applied": true})
        }
        "workspace/configuration" => Value::Array(vec![Value::Null; params["items"].as_array().map_or(0, Vec::len)]),
        "workspace/inlayHint/refresh" => {
            emit(Told::Hints);
            Value::Null
        }
        _ => Value::Null,
    }
}

fn found_of(location: &Value) -> Option<Found> {
    let uri = location["uri"].as_str().or_else(|| location["targetUri"].as_str())?;
    let range = if location["range"].is_object() { &location["range"] } else { &location["targetSelectionRange"] };
    let at = place(&range["start"]);
    Some(Found { path: lsp::path_of(uri)?.display().to_string(), line: at.line, col: at.col })
}

impl Servers {
    /// The toolchain and spec whose server serves `language`, where one does.
    fn spec_for(language: &str) -> Option<(toolchains::Tool, ServerSpec)> {
        toolchains::manifest().into_iter().find_map(|tool| tool.server.clone().filter(|spec| spec.languages.iter().any(|one| one == language)).map(|spec| (tool, spec)))
    }

    /// The server for `language`, started at `root` where it is not running yet.
    fn server(&self, root: &Path, language: &str, emit: &Emit) -> Result<Option<(Arc<Server>, ServerSpec)>, String> {
        let Some((tool, spec)) = Self::spec_for(language) else {
            return Ok(None);
        };
        {
            let mut kept = self.root.lock().map_err(|_| "held".to_string())?;
            if kept.as_deref() != Some(root) {
                drop(kept);
                self.stop_all();
                kept = self.root.lock().map_err(|_| "held".to_string())?;
                *kept = Some(root.to_path_buf());
            }
        }
        if let Some(server) = self.running.lock().map_err(|_| "held".to_string())?.get(&tool.id) {
            return Ok(Some((server.clone(), spec)));
        }
        if let Some(said) = self.failed.lock().map_err(|_| "held".to_string())?.get(&tool.id) {
            return Err(said.clone());
        }
        let found = toolchains::find(&tool, &toolchains::path_folders(), &toolchains::chosen());
        let program = found.folder.as_deref().and_then(|folder| toolchains::program_in(Path::new(folder), std::slice::from_ref(&spec.program)));
        let started = match program {
            Some(program) => {
                let emit = emit.clone();
                let published = self.published.clone();
                let checks = self.checks.clone();
                let options = spec.projects.as_deref().map_or(Value::Null, |name| json!({"linkedProjects": projects_in(root, name)}));
                Server::start(&program, &spec.args, root, options, toolchains::run_path(), Box::new(move |method, params| heard(method, params, &emit, &published, &checks))).map_err(|said| match &spec.setup {
                    Some(setup) => format!("{} did not start ({said}): {setup} installs it", spec.program),
                    None => format!("{} did not start: {said}", spec.program),
                })
            }
            None => Err(format!("{} is not found: File, Toolchains opens {}'s install page or takes its folder", spec.program, tool.name)),
        };
        match started {
            Ok(server) => {
                let server = Arc::new(server);
                self.running.lock().map_err(|_| "held".to_string())?.insert(tool.id.clone(), server.clone());
                Ok(Some((server, spec)))
            }
            Err(said) => {
                self.failed.lock().map_err(|_| "held".to_string())?.insert(tool.id.clone(), said.clone());
                Err(said)
            }
        }
    }

    /// Hands the file at `path` to its language's server, and its text to the inspections. Says
    /// whether a server took it.
    pub fn open(&self, root: &Path, path: &Path, language: &str, text: &str, emit: &Emit) -> Result<bool, String> {
        self.inspect(root, path, Some(text), emit);
        let Some((server, spec)) = self.server(root, language, emit)? else {
            return Ok(false);
        };
        let id = Self::id_of(&spec, path, language);
        // A file the check holds is the editor's from here, and its clearing as it closes is heard.
        let uri = lsp::uri_of(path);
        let mut held = self.checks.held.lock().map_err(|_| "held".to_string())?;
        held.remove(&uri);
        if let Ok(mut heard) = self.checks.heard.lock() {
            heard.letting_go.remove(&lsp::key_of(&uri));
        }
        if server.has(path) {
            server.change(path, text)?;
            // A tab opened again on a file the server holds hears the diagnostics it last gave.
            emit_whole(&lsp::key_of(&uri), &uri, &self.published, &self.checks, emit);
        } else {
            server.open(path, &id, text)?;
        }
        drop(held);
        Ok(true)
    }

    /// The protocol's name for the language of `path`, by its extension.
    fn id_of(spec: &ServerSpec, path: &Path, language: &str) -> String {
        let ext = path.extension().map(|ext| ext.to_string_lossy().to_lowercase()).unwrap_or_default();
        spec.ids.get(&ext).cloned().unwrap_or_else(|| language.to_string())
    }

    /// The server that has `path` open, where one does.
    fn holding(&self, path: &Path) -> Option<Arc<Server>> {
        self.running.lock().ok()?.values().find(|server| server.has(path)).cloned()
    }

    pub fn change(&self, path: &Path, text: &str) -> Result<(), String> {
        self.inspect_again(path, Some(text));
        let _held = self.checks.held.lock().map_err(|_| "held".to_string())?;
        self.holding(path).map_or(Ok(()), |server| server.change(path, text))
    }

    /// Lets go of a file the editor closed, whose text the inspections read again as the disk holds
    /// it. Where the tree's check is on, the file is checked as the disk holds it before its server
    /// lets it go.
    pub fn close(&self, path: &Path) -> Result<(), String> {
        let text = std::fs::read_to_string(path).ok();
        self.inspect_again(path, text.as_deref());
        let Some(server) = self.holding(path) else {
            return Ok(());
        };
        let mut held = self.checks.held.lock().map_err(|_| "held".to_string())?;
        let mut queue = self.checks.queue.lock().map_err(|_| "held".to_string())?;
        if !queue.on {
            return server.close(path);
        }
        held.insert(lsp::uri_of(path));
        queue.total += 1;
        queue.waiting.push_front(Waiting { path: path.to_path_buf(), server, id: String::new() });
        self.checks.ready.notify_one();
        Ok(())
    }

    pub fn hover(&self, path: &Path, line: u32, col: u32) -> Result<Option<String>, String> {
        let Some(server) = self.holding(path) else {
            return Ok(None);
        };
        let said = server.request("textDocument/hover", Server::at(path, line, col), ASKING)?;
        Ok(Some(markdown_of(&said["contents"])).filter(|text| !text.trim().is_empty()))
    }

    pub fn definition(&self, path: &Path, line: u32, col: u32) -> Result<Vec<Found>, String> {
        let Some(server) = self.holding(path) else {
            return Ok(Vec::new());
        };
        let said = server.request("textDocument/definition", Server::at(path, line, col), ASKING)?;
        Ok(match said {
            Value::Array(all) => all.iter().filter_map(found_of).collect(),
            Value::Null => Vec::new(),
            one => found_of(&one).into_iter().collect(),
        })
    }

    pub fn complete(&self, path: &Path, line: u32, col: u32) -> Result<Vec<Item>, String> {
        let Some(server) = self.holding(path) else {
            return Ok(Vec::new());
        };
        let mut params = Server::at(path, line, col);
        params["context"] = json!({"triggerKind": 1});
        let said = server.request("textDocument/completion", params, ASKING)?;
        let items = if said.is_array() { said } else { said["items"].clone() };
        Ok(items.as_array().map(|all| all.iter().map(item_of).collect()).unwrap_or_default())
    }

    /// The server that has `path` open, or the reason there is none to ask.
    fn asked(&self, path: &Path) -> Result<Arc<Server>, String> {
        self.holding(path).ok_or_else(|| "no language server has this file open".to_string())
    }

    /// Every place the symbol at `line`, `col` of `path` is used, its declaration among them.
    pub fn references(&self, path: &Path, line: u32, col: u32) -> Result<Vec<Usage>, String> {
        let server = self.asked(path)?;
        let mut params = Server::at(path, line, col);
        params["context"] = json!({"includeDeclaration": true});
        let said = server.request("textDocument/references", params, SEARCHING)?;
        let mut cache = HashMap::new();
        let mut usages: Vec<Usage> = said
            .as_array()
            .map(|all| {
                all.iter()
                    .filter_map(|one| {
                        let path = lsp::path_of(one["uri"].as_str()?)?.display().to_string();
                        let from = place(&one["range"]["start"]);
                        let text = lines_of(&path, &mut cache).get(from.line as usize).cloned().unwrap_or_default();
                        Some(Usage { path, from, to: place(&one["range"]["end"]), text })
                    })
                    .collect()
            })
            .unwrap_or_default();
        usages.sort_by(|a, b| (&a.path, a.from.line, a.from.col).cmp(&(&b.path, b.from.line, b.from.col)));
        usages.dedup_by(|a, b| a.path == b.path && a.from == b.from);
        Ok(usages)
    }

    /// Whether the symbol at `line`, `col` can be renamed, its span and the name to start from. None
    /// where the server does not say before a rename, and an error where it says it cannot.
    pub fn renamable(&self, path: &Path, line: u32, col: u32) -> Result<Option<Renamable>, String> {
        let server = self.asked(path)?;
        if server.capabilities["renameProvider"]["prepareProvider"].as_bool() != Some(true) {
            return Ok(None);
        }
        let said = server.request("textDocument/prepareRename", Server::at(path, line, col), ASKING)?;
        let range = if said["range"].is_object() { &said["range"] } else { &said };
        if !range["start"].is_object() {
            return if said.is_null() { Err("nothing here can be renamed".into()) } else { Ok(None) };
        }
        Ok(Some(Renamable { from: place(&range["start"]), to: place(&range["end"]), name: said["placeholder"].as_str().map(str::to_string) }))
    }

    /// The edits that rename the symbol at `line`, `col` of `path` to `name` wherever it is used.
    pub fn rename(&self, path: &Path, line: u32, col: u32, name: &str) -> Result<Vec<FileEdit>, String> {
        let server = self.asked(path)?;
        let mut params = Server::at(path, line, col);
        params["newName"] = json!(name);
        let said = server.request("textDocument/rename", params, SEARCHING)?;
        if said.is_null() {
            return Err("the server found nothing to rename".into());
        }
        Ok(workspace_edit_of(&said))
    }

    /// The quick fixes and refactorings the server offers for the span `from`, `to` of `path`, given
    /// the diagnostics it gave that touch it.
    pub fn actions(&self, path: &Path, from: Place, to: Place) -> Result<Vec<Action>, String> {
        let uri = lsp::uri_of(path);
        let mut fixes: Vec<Action> = Vec::new();
        if let Some((_, all)) = self.checks.inspected.lock().map_err(|_| "held".to_string())?.get(&lsp::key_of(&uri)) {
            for one in all.iter().filter(|one| overlaps(&from, &to, &one["range"])) {
                for fix in one["data"]["orior"].as_array().into_iter().flatten() {
                    fixes.push(Action { title: fix["title"].as_str().unwrap_or_default().to_string(), kind: "quickfix".into(), preferred: true, disabled: None, raw: json!({"orior": fix["edits"]}) });
                }
            }
        }
        let server = match self.asked(path) {
            Ok(server) => server,
            Err(_) if !fixes.is_empty() => return Ok(fixes),
            Err(said) => return Err(said),
        };
        let diagnostics: Vec<Value> = self.published.lock().map_err(|_| "held".to_string())?.get(&lsp::key_of(&uri)).map(|(_, all)| all.iter().filter(|one| overlaps(&from, &to, &one["range"])).cloned().collect()).unwrap_or_default();
        let params = json!({
            "textDocument": {"uri": uri},
            "range": {"start": {"line": from.line, "character": from.col}, "end": {"line": to.line, "character": to.col}},
            "context": {"diagnostics": diagnostics, "triggerKind": 1}
        });
        let said = server.request("textDocument/codeAction", params, ASKING)?;
        let mut actions: Vec<Action> = said.as_array().map(|all| all.iter().filter_map(action_of).collect()).unwrap_or_default();
        // A fix the server offers for a diagnostic and again for its note is listed once.
        let mut seen = std::collections::HashSet::new();
        actions.retain(|action| seen.insert((action.title.clone(), action.raw["edit"].to_string())));
        actions.sort_by_key(|action| (action.disabled.is_some(), !action.preferred, !action.kind.starts_with("quickfix")));
        fixes.extend(actions);
        Ok(fixes)
    }

    /// Carries out an action `actions` gave: its edits, filled in by the server first where it left
    /// them out, are returned to be written, and its command is run, whose edits the server asks
    /// for on its own.
    pub fn act(&self, path: &Path, raw: &Value) -> Result<Vec<FileEdit>, String> {
        if let Some(edits) = raw.get("orior") {
            let edits: Vec<TextEdit> = serde_json::from_value(edits.clone()).map_err(|error| error.to_string())?;
            return Ok(vec![FileEdit { path: path.display().to_string(), edits }]);
        }
        let server = self.asked(path)?;
        let mut action = raw.clone();
        if action["command"].is_string() {
            server.request("workspace/executeCommand", json!({"command": action["command"], "arguments": action["arguments"]}), SEARCHING)?;
            return Ok(Vec::new());
        }
        if action["edit"].is_null() && action["command"].is_null() && server.capabilities["codeActionProvider"]["resolveProvider"].as_bool() == Some(true) {
            action = server.request("codeAction/resolve", action, ASKING)?;
        }
        let edits = workspace_edit_of(&action["edit"]);
        if let Some(command) = action["command"].as_object() {
            server.request("workspace/executeCommand", json!({"command": command.get("command"), "arguments": command.get("arguments")}), SEARCHING)?;
        }
        Ok(edits)
    }

    /// The hints the server of `path` writes on its lines `from` to `to`, where it writes any.
    pub fn hints(&self, path: &Path, from: u32, to: u32) -> Result<Vec<Hint>, String> {
        let Some(server) = self.holding(path).filter(|server| !server.capabilities["inlayHintProvider"].is_null() && server.capabilities["inlayHintProvider"] != false) else {
            return Ok(Vec::new());
        };
        // The range ends at the start of the line after `to`, or at the end of the text where `to` is
        // its last line.
        let lines: Vec<u32> = server.text(path).unwrap_or_default().split('\n').map(|line| line.encode_utf16().count() as u32).collect();
        let end = if (to as usize) + 1 < lines.len() { json!({"line": to + 1, "character": 0}) } else { json!({"line": lines.len().saturating_sub(1), "character": lines.last().copied().unwrap_or(0)}) };
        let range = json!({"start": {"line": from, "character": 0}, "end": end});
        let said = server.request("textDocument/inlayHint", json!({"textDocument": {"uri": lsp::uri_of(path)}, "range": range}), ASKING)?;
        Ok(said
            .as_array()
            .into_iter()
            .flatten()
            .map(|one| {
                let at = place(&one["position"]);
                let label = match &one["label"] {
                    Value::String(text) => text.clone(),
                    Value::Array(parts) => parts.iter().filter_map(|part| part["value"].as_str()).collect(),
                    _ => String::new(),
                };
                Hint { line: at.line, col: at.col, label: label.trim().to_string(), kind: one["kind"].as_u64().unwrap_or(0) as u8, left: one["paddingLeft"].as_bool().unwrap_or(false), right: one["paddingRight"].as_bool().unwrap_or(false) }
            })
            .filter(|hint| !hint.label.is_empty())
            .collect())
    }

    /// The function at `line`, `col` of `path`, as the top of its call hierarchy: as its server finds
    /// it where the server answers for calls, and as the inspections' index finds it elsewhere.
    pub fn call_root(&self, path: &Path, line: u32, col: u32) -> Result<Option<Call>, String> {
        if let Some(server) = self.holding(path).filter(|server| !server.capabilities["callHierarchyProvider"].is_null() && server.capabilities["callHierarchyProvider"] != false) {
            let said = server.request("textDocument/prepareCallHierarchy", Server::at(path, line, col), ASKING)?;
            return Ok(said.as_array().and_then(|all| all.first()).and_then(|item| call_of(item, None, &[])));
        }
        let (root, file) = self.indexed_file(path)?;
        let index = self.index.lock().map_err(|_| "held".to_string())?;
        let found = index.function_on(&file, line).or_else(|| index.function_at(&file, line));
        Ok(found.and_then(|at| index.function(&file, at).map(|function| indexed_call(&root, &file, function, &file, &[]))))
    }

    /// The functions that call the function `item` names, where `incoming`, or that it calls, each with
    /// the places of its calls: from the server of `path`, the file the hierarchy started in, or from
    /// the inspections' index where the item is its.
    pub fn calls(&self, path: &Path, item: &Value, incoming: bool) -> Result<Vec<Call>, String> {
        if let Some(found) = item.get("orior") {
            let (root, _) = self.indexed_file(path)?;
            let file = found["path"].as_str().unwrap_or_default().to_string();
            let line = found["line"].as_u64().unwrap_or(0) as u32;
            let index = self.index.lock().map_err(|_| "held".to_string())?;
            let Some(at) = index.function_on(&file, line) else {
                return Ok(Vec::new());
            };
            let reached = if incoming { index.callers(&file, at) } else { index.callees(&file, at) };
            return Ok(reached
                .iter()
                .filter_map(|(other, index_at, sites)| {
                    let site = if incoming { other.clone() } else { file.clone() };
                    index.function(other, *index_at).map(|function| indexed_call(&root, other, function, &site, sites))
                })
                .collect());
        }
        let server = self.asked(path)?;
        let method = if incoming { "callHierarchy/incomingCalls" } else { "callHierarchy/outgoingCalls" };
        let said = server.request(method, json!({"item": item}), SEARCHING)?;
        let parent = item["uri"].as_str().and_then(lsp::path_of).map(|path| path.display().to_string());
        Ok(said
            .as_array()
            .into_iter()
            .flatten()
            .filter_map(|one| {
                let ranges: Vec<Place> = one["fromRanges"].as_array().into_iter().flatten().map(|range| place(&range["start"])).collect();
                if incoming { call_of(&one["from"], None, &ranges) } else { call_of(&one["to"], parent.clone(), &ranges) }
            })
            .collect())
    }

    /// The tree the inspections' index holds and the path of `path` in it.
    fn indexed_file(&self, path: &Path) -> Result<(PathBuf, String), String> {
        let root = self.indexed.lock().map_err(|_| "held".to_string())?.clone().ok_or_else(|| "no language server answers for calls here, and orior's own index is still being read".to_string())?;
        let file = path.strip_prefix(&root).map_err(|_| format!("{} is outside the tree", path.display()))?.to_string_lossy().replace('\\', "/");
        Ok((root, file))
    }

    /// The signature of the call the cursor at `line`, `col` is in, where it is in one.
    pub fn signature(&self, path: &Path, line: u32, col: u32) -> Result<Option<Signature>, String> {
        let Some(server) = self.holding(path) else {
            return Ok(None);
        };
        let said = server.request("textDocument/signatureHelp", Server::at(path, line, col), ASKING)?;
        Ok(signature_of(&said))
    }

    /// Takes every server out of the running ones, and forgets those that failed: a server asked for
    /// after this starts afresh.
    fn take_all(&self) -> Vec<Arc<Server>> {
        if let Ok(mut failed) = self.failed.lock() {
            failed.clear();
        }
        self.running.lock().map(|mut all| all.drain().map(|(_, server)| server).collect()).unwrap_or_default()
    }

    /// Every server running: its toolchain's id, its program's name, its process, and the files it
    /// has open.
    pub fn running(&self) -> Vec<Running> {
        let manifest = toolchains::manifest();
        let Ok(all) = self.running.lock() else {
            return Vec::new();
        };
        all.iter()
            .map(|(tool, server)| Running {
                tool: tool.clone(),
                program: manifest.iter().find(|one| &one.id == tool).and_then(|one| one.server.as_ref()).map_or_else(|| tool.clone(), |spec| spec.program.clone()),
                pid: server.pid(),
                files: server.files().iter().map(|path| path.display().to_string()).collect(),
            })
            .collect()
    }

    /// The toolchains whose servers serve any of `languages`.
    pub fn tools_for(languages: &[String]) -> Vec<String> {
        languages.iter().filter_map(|language| Self::spec_for(language).map(|(tool, _)| tool.id)).collect()
    }

    /// Stops the servers of `tools` without waiting, each told to end on a thread of its own, and
    /// gives the files they had open. A server stopped starts again when a file of its language is
    /// handed to it.
    pub fn stop(&self, tools: &[String]) -> Vec<String> {
        let taken: Vec<Arc<Server>> = self.running.lock().map(|mut all| tools.iter().filter_map(|tool| all.remove(tool)).collect()).unwrap_or_default();
        let files = taken.iter().flat_map(|server| server.files()).map(|path| path.display().to_string()).collect();
        for server in taken {
            std::thread::spawn(move || server.stop());
        }
        files
    }

    /// Stops every server and waits for each to end, as the app closes.
    pub fn stop_all(&self) {
        for server in self.take_all() {
            server.stop();
        }
    }

    /// Stops every server without waiting, as another tree opens: the servers are let go at once,
    /// and each is told to end on a thread of its own.
    pub fn let_go(&self) {
        if let Ok(mut queue) = self.checks.queue.lock() {
            let started = queue.started;
            *queue = Queue { started, ..Queue::default() };
        }
        if let Ok(mut held) = self.checks.held.lock() {
            held.clear();
        }
        if let Ok(mut published) = self.published.lock() {
            published.clear();
        }
        if let Ok(mut inspected) = self.checks.inspected.lock() {
            inspected.clear();
        }
        if let (Ok(mut index), Ok(mut indexed)) = (self.index.lock(), self.indexed.lock()) {
            *index = inspect::Tree::default();
            *indexed = None;
        }
        for server in self.take_all() {
            std::thread::spawn(move || server.stop());
        }
    }

    /// Starts the tree's check over `files`, each relative to `root`: every file of a language with a
    /// server is handed to it, or its projects checked as a whole where it checks them so. Gives how
    /// many files wait.
    pub fn check_tree(&self, root: &Path, files: Vec<String>, emit: &Emit) -> usize {
        {
            let Ok(mut queue) = self.checks.queue.lock() else {
                return 0;
            };
            queue.on = true;
            queue.root = Some(root.to_path_buf());
            queue.emit = Some(emit.clone());
            queue.files = files.clone();
        }
        self.start_checkers();
        self.check_files(root, &files, emit)
    }

    /// Checks again the files of a batch of changes made on the disk, and the files whose includes or
    /// imports name one of them, where the tree's check is on; a file gone loses its diagnostics.
    pub fn changed(&self, root: &Path, files: &[String]) {
        let emit = {
            let Ok(mut queue) = self.checks.queue.lock() else {
                return;
            };
            if !queue.on || queue.root.as_deref() != Some(root) {
                return;
            }
            for file in files {
                if !queue.files.contains(file) {
                    queue.files.push(file.clone());
                }
            }
            queue.files.retain(|file| root.join(file).is_file());
            queue.emit.clone()
        };
        let Some(emit) = emit else {
            return;
        };
        let gone: Vec<String> = self.published.lock().map(|mut all| {
            let gone: Vec<String> = all.keys().filter(|key| !Path::new(key).exists()).cloned().collect();
            gone.iter().filter_map(|key| all.remove(key)).map(|(uri, _)| uri).collect()
        }).unwrap_or_default();
        // Each server hears of the changes, and a server that checks a file at a time is given each
        // file the editor holds again, which it then reads against a header or a module that changed
        // under it.
        let events: Vec<Value> = files.iter().map(|file| json!({"uri": lsp::uri_of(&root.join(file)), "type": 2})).chain(gone.iter().map(|uri| json!({"uri": uri, "type": 3}))).collect();
        if !events.is_empty() {
            let manifest = toolchains::manifest();
            let running: Vec<(String, Arc<Server>)> = self.running.lock().map(|all| all.iter().map(|(tool, server)| (tool.clone(), server.clone())).collect()).unwrap_or_default();
            let held = self.checks.held.lock();
            for (tool, server) in running {
                let _ = server.notify("workspace/didChangeWatchedFiles", json!({"changes": events}));
                let whole = manifest.iter().find(|one| one.id == tool).and_then(|one| one.server.as_ref()).is_some_and(|spec| spec.checks.is_some());
                if let (false, Ok(held)) = (whole, held.as_ref()) {
                    for path in server.files() {
                        if !held.contains(&lsp::uri_of(&path)) {
                            let _ = server.read_again(&path);
                        }
                    }
                }
            }
        }
        for uri in gone {
            if let Some(diagnostics) = diagnostics_of(&json!({"uri": uri, "diagnostics": []})) {
                emit(Told::Diagnostics(diagnostics));
            }
        }
        for file in files {
            let path = root.join(file);
            let text = std::fs::read_to_string(&path).ok();
            self.inspect(root, &path, text.as_deref(), &emit);
        }
        let tree = self.checks.queue.lock().map(|queue| queue.files.clone()).unwrap_or_default();
        let mut again: Vec<String> = files.iter().filter(|file| root.join(file).is_file()).cloned().collect();
        let languages = crate::plugins::languages();
        let mut tools: HashMap<String, Option<String>> = HashMap::new();
        let mut tool_of = |file: &str| language_of(&languages, file).and_then(|language| tools.entry(language.clone()).or_insert_with(|| Self::spec_for(&language).map(|(tool, _)| tool.id)).clone());
        let mut stems: HashMap<String, Vec<String>> = HashMap::new();
        for file in files {
            if let (Some(tool), Some(stem)) = (tool_of(file), Path::new(file).file_stem()) {
                stems.entry(tool).or_default().push(stem.to_string_lossy().to_string());
            }
        }
        if !stems.is_empty() {
            for other in &tree {
                if again.contains(other) {
                    continue;
                }
                if let Some(wanted) = tool_of(other).and_then(|tool| stems.get(&tool)) {
                    if names(&root.join(other), wanted) {
                        again.push(other.clone());
                    }
                }
            }
        }
        self.check_files(root, &again, &emit);
    }

    /// Reads a file's text into the inspections' index, or takes the file out of it where `text` is
    /// none, and passes on each file's findings that changed. The tree's Python and JavaScript are
    /// read first, on a thread of their own, where the index holds another tree or none.
    fn inspect(&self, root: &Path, path: &Path, text: Option<&str>, emit: &Emit) {
        let Some(file) = path.strip_prefix(root).ok().map(|inside| inside.to_string_lossy().replace('\\', "/")) else {
            return;
        };
        let Some(language) = inspect::language_of(&file) else {
            return;
        };
        if let Ok(mut told) = self.told.lock() {
            *told = Some(emit.clone());
        }
        let ready = self.indexed.lock().is_ok_and(|read| read.as_deref() == Some(root));
        let text = text.map(str::to_string);
        let (index, indexed, published, checks, emit, root) = (self.index.clone(), self.indexed.clone(), self.published.clone(), self.checks.clone(), emit.clone(), root.to_path_buf());
        let work = move || {
            let Ok(mut tree) = index.lock() else {
                return;
            };
            let Ok(mut read) = indexed.lock() else {
                return;
            };
            if read.as_deref() != Some(root.as_path()) {
                *tree = inspect::Tree::default();
                for one in crate::files::all(&root) {
                    if let (Some(language), Ok(text)) = (inspect::language_of(&one), std::fs::read_to_string(root.join(&one))) {
                        tree.set(&one, language, &text);
                    }
                }
                *read = Some(root.clone());
            }
            drop(read);
            match &text {
                Some(text) => tree.set(&file, language, text),
                None => tree.remove(&file),
            }
            let found = tree.findings();
            let supplied = tree.supplied();
            drop(tree);
            let supplied: HashMap<String, HashMap<String, HashSet<String>>> = supplied.into_iter().map(|(file, classes)| (lsp::key_of(&lsp::uri_of(&root.join(file))), classes)).collect();
            let moved = checks.supplied.lock().map(|mut kept| {
                let moved = *kept != supplied;
                *kept = supplied;
                moved
            });
            pass_on(&root, found, &published, &checks, &emit);
            // What a family sets decides which of a server's reports of an attribute pass on.
            if moved.unwrap_or(false) {
                let reporting: Vec<(String, String)> = published.lock().map(|all| all.iter().filter(|(_, (_, items))| items.iter().any(|item| attribute_of(item).is_some())).map(|(key, (uri, _))| (key.clone(), uri.clone())).collect()).unwrap_or_default();
                for (key, uri) in reporting {
                    emit_whole(&key, &uri, &published, &checks, &emit);
                }
            }
            // A file the editor opens hears its diagnostics, changed or the same.
            if text.is_some() {
                let uri = lsp::uri_of(&root.join(&file));
                emit_whole(&lsp::key_of(&uri), &uri, &published, &checks, &emit);
            }
        };
        if ready {
            work();
        } else {
            std::thread::spawn(work);
        }
    }

    /// Reads a file's text into the inspections' index as the editor changes or closes it, in the tree
    /// the index holds.
    fn inspect_again(&self, path: &Path, text: Option<&str>) {
        let root = self.indexed.lock().ok().and_then(|read| read.clone());
        let emit = self.told.lock().ok().and_then(|told| told.clone());
        if let (Some(root), Some(emit)) = (root, emit) {
            self.inspect(&root, path, text, &emit);
        }
    }

    /// Whether the tree's check has files waiting or being checked.
    pub fn checking(&self) -> bool {
        self.checks.queue.lock().is_ok_and(|queue| queue.on && queue.done < queue.total)
    }

    /// Queues `files` for the tree's check, or has their servers check their projects as a whole,
    /// each server started where it is not running.
    fn check_files(&self, root: &Path, files: &[String], emit: &Emit) -> usize {
        let languages = crate::plugins::languages();
        let mut by_language: HashMap<String, Option<(String, Arc<Server>, ServerSpec)>> = HashMap::new();
        let mut waiting = Vec::new();
        let mut whole = HashSet::new();
        for file in files {
            let Some(language) = language_of(&languages, file) else {
                continue;
            };
            let found = by_language.entry(language.clone()).or_insert_with(|| {
                let (tool, spec) = Self::spec_for(&language)?;
                self.server(root, &language, emit).ok().flatten().map(|(server, _)| (tool.id, server, spec))
            });
            let Some((tool, server, spec)) = found.clone() else {
                continue;
            };
            let path = root.join(file);
            match &spec.checks {
                Some(method) => {
                    if whole.insert(tool) {
                        let _ = server.notify(method, json!({"textDocument": null}));
                    }
                }
                None => {
                    if std::fs::metadata(&path).is_ok_and(|meta| meta.len() <= CHECK_MOST) {
                        let id = Self::id_of(&spec, &path, &language);
                        waiting.push(Waiting { path, server, id });
                    }
                }
            }
        }
        let Ok(mut queue) = self.checks.queue.lock() else {
            return 0;
        };
        let count = waiting.len();
        for one in waiting {
            if !queue.waiting.iter().any(|queued| queued.path == one.path) {
                queue.total += 1;
                queue.waiting.push_back(one);
            }
        }
        if queue.waiting.is_empty() && queue.done >= queue.total {
            let (done, total) = (queue.done, queue.total);
            drop(queue);
            emit(Told::Checking { done, total });
        } else {
            self.checks.ready.notify_all();
        }
        count
    }

    /// Starts the threads that check the tree's files, once.
    fn start_checkers(&self) {
        let Ok(mut queue) = self.checks.queue.lock() else {
            return;
        };
        if queue.started {
            return;
        }
        queue.started = true;
        for _ in 0..CHECKERS {
            let checks = Arc::downgrade(&self.checks);
            let published = Arc::downgrade(&self.published);
            std::thread::spawn(move || {
                while let (Some(checks), Some(published)) = (checks.upgrade(), published.upgrade()) {
                    let Some((one, emit)) = next_waiting(&checks) else {
                        continue;
                    };
                    check_one(&checks, &published, &one, &emit);
                    let Ok(mut queue) = checks.queue.lock() else {
                        return;
                    };
                    queue.done += 1;
                    let told = (queue.done, queue.total, queue.emit.clone());
                    drop(queue);
                    if let (done, total, Some(emit)) = told {
                        emit(Told::Checking { done, total });
                    }
                }
            });
        }
    }
}

/// A function of the call hierarchy from a server's item: its calls in `site`, the item's own file
/// where none is given, at `ranges`.
fn call_of(item: &Value, site: Option<String>, ranges: &[Place]) -> Option<Call> {
    let path = lsp::path_of(item["uri"].as_str()?)?.display().to_string();
    let span = if item["selectionRange"].is_object() { &item["selectionRange"] } else { &item["range"] };
    Some(Call {
        name: item["name"].as_str().unwrap_or_default().to_string(),
        detail: item["detail"].as_str().unwrap_or_default().to_string(),
        site: site.unwrap_or_else(|| path.clone()),
        path,
        from: place(&span["start"]),
        to: place(&span["end"]),
        at: ranges.to_vec(),
        item: item.clone(),
    })
}

/// A function of the call hierarchy from the inspections' index: `function` of `file`, its calls in
/// `site` at `sites`.
fn indexed_call(root: &Path, file: &str, function: &inspect::Function, site: &str, sites: &[(Place, Place)]) -> Call {
    Call {
        name: function.name.clone(),
        detail: String::new(),
        path: root.join(file).display().to_string(),
        from: function.from.clone(),
        to: function.to.clone(),
        site: root.join(site).display().to_string(),
        at: sites.iter().map(|(from, _)| from.clone()).collect(),
        item: json!({"orior": {"path": file, "line": function.from.line}}),
    }
}

/// Passes on a file's diagnostics whole: its server's, less its reports of an attribute a class's
/// family sets, then the inspections'.
fn emit_whole(key: &str, uri: &str, published: &Published, checks: &Checks, emit: &Emit) {
    let mut items = published.lock().ok().and_then(|all| all.get(key).map(|(_, items)| items.clone())).unwrap_or_default();
    if let Some(classes) = checks.supplied.lock().ok().and_then(|all| all.get(key).cloned()) {
        items.retain(|item| !attribute_of(item).is_some_and(|(name, class)| classes.get(&class).is_some_and(|names| names.contains(&name))));
    }
    items.extend(checks.inspected.lock().ok().and_then(|all| all.get(key).map(|(_, items)| items.clone())).unwrap_or_default());
    if let Some(diagnostics) = diagnostics_of(&json!({"uri": uri, "diagnostics": items})) {
        emit(Told::Diagnostics(diagnostics));
    }
}

/// The attribute and the class a server's report that a class has no such attribute names, as
/// pyright writes it: `Cannot access attribute "name" for class "Class*"`.
fn attribute_of(item: &Value) -> Option<(String, String)> {
    let message = item["message"].as_str()?;
    let rest = message.strip_prefix("Cannot access attribute \"")?;
    let (name, rest) = rest.split_once('"')?;
    let rest = rest.strip_prefix(" for class \"")?;
    let (class, _) = rest.split_once('"')?;
    Some((name.to_string(), class.trim_end_matches('*').to_string()))
}

/// Keeps every file's findings, by its path in the tree at `root`, and passes on the diagnostics of
/// each file whose findings changed.
fn pass_on(root: &Path, found: HashMap<String, Vec<inspect::Finding>>, published: &Published, checks: &Checks, emit: &Emit) {
    let mut changed = Vec::new();
    {
        let Ok(mut inspected) = checks.inspected.lock() else {
            return;
        };
        let mut seen = HashSet::new();
        for (file, findings) in found {
            let uri = lsp::uri_of(&root.join(&file));
            let key = lsp::key_of(&uri);
            let values: Vec<Value> = findings.iter().map(inspect::value_of).collect();
            seen.insert(key.clone());
            if (inspected.contains_key(&key) || !values.is_empty()) && inspected.get(&key).map(|(_, kept)| kept) != Some(&values) {
                inspected.insert(key.clone(), (uri.clone(), values));
                changed.push((key, uri));
            }
        }
        let gone: Vec<String> = inspected.keys().filter(|key| !seen.contains(*key)).cloned().collect();
        for key in gone {
            if let Some((uri, _)) = inspected.remove(&key) {
                changed.push((key, uri));
            }
        }
    }
    for (key, uri) in changed {
        emit_whole(&key, &uri, published, checks, emit);
    }
}

/// The next file waiting for the tree's check, once one waits, and where what is said of it goes;
/// none after a moment with none, for the checker to see whether the checks are still kept.
fn next_waiting(checks: &Checks) -> Option<(Waiting, Emit)> {
    let queue = checks.queue.lock().ok()?;
    let (mut queue, _) = checks.ready.wait_timeout_while(queue, Duration::from_secs(5), |queue| queue.waiting.is_empty() || queue.emit.is_none()).ok()?;
    let emit = queue.emit.clone()?;
    queue.waiting.pop_front().map(|one| (one, emit))
}

/// Hands one file to its server as the disk holds it, asks the server for its diagnostics or, where
/// the server refuses to be asked, waits for the server to give them, and lets it go where the check
/// still holds it. A file the editor holds is left to the editor.
fn check_one(checks: &Checks, published: &Published, one: &Waiting, emit: &Emit) {
    let uri = lsp::uri_of(&one.path);
    let key = lsp::key_of(&uri);
    let Ok(text) = std::fs::read_to_string(&one.path) else {
        if let Ok(mut held) = checks.held.lock() {
            if held.remove(&uri) {
                let _ = one.server.close(&one.path);
            }
        }
        return;
    };
    let (send, woken) = mpsc::channel();
    {
        let Ok(mut held) = checks.held.lock() else {
            return;
        };
        let open = one.server.has(&one.path);
        if open && !held.contains(&uri) {
            return;
        }
        if let Ok(mut heard) = checks.heard.lock() {
            heard.woken.insert(key.clone(), send);
            heard.letting_go.remove(&key);
        }
        held.insert(uri.clone());
        let sent = if open { one.server.change(&one.path, &text) } else { one.server.open(&one.path, &one.id, &text) };
        if sent.is_err() {
            held.remove(&uri);
            if let Ok(mut heard) = checks.heard.lock() {
                heard.woken.remove(&key);
            }
            return;
        }
    }
    let pid = one.server.pid();
    let asked = !checks.refused.lock().is_ok_and(|refused| refused.contains(&pid));
    let answer = if asked { Some(one.server.request("textDocument/diagnostic", json!({"textDocument": {"uri": uri}}), CHECK_PATIENCE)) } else { None };
    match answer {
        Some(Ok(answer)) if answer["kind"] == "full" => {
            let items = answer["items"].as_array().cloned().unwrap_or_default();
            if let Ok(mut kept) = published.lock() {
                kept.insert(key.clone(), (uri.clone(), items));
            }
            emit_whole(&key, &uri, published, checks, emit);
        }
        Some(Ok(_)) => {}
        Some(Err(said)) if !said.contains("had no answer") => {
            if let Ok(mut refused) = checks.refused.lock() {
                refused.insert(pid);
            }
            wait_for(&woken);
        }
        Some(Err(_)) => {}
        None => wait_for(&woken),
    }
    let Ok(mut held) = checks.held.lock() else {
        return;
    };
    let ours = held.remove(&uri);
    if let Ok(mut heard) = checks.heard.lock() {
        heard.woken.remove(&key);
        if ours {
            heard.letting_go.insert(key.clone());
        }
    }
    if ours {
        let _ = one.server.close(&one.path);
    }
}

/// Waits for a server to give a file's diagnostics, and for any it gives after them.
fn wait_for(woken: &mpsc::Receiver<()>) {
    if woken.recv_timeout(CHECK_PATIENCE).is_ok() {
        while woken.recv_timeout(CHECK_QUIET).is_ok() {}
    }
}

/// The language a file of the tree opens as, by its extension.
fn language_of(languages: &HashMap<String, String>, file: &str) -> Option<String> {
    let ext = Path::new(file).extension()?.to_string_lossy().to_lowercase();
    languages.get(&ext).cloned()
}

/// Whether a line of the file at `path` that includes, imports or uses another names one of `stems`
/// as a word.
fn names(path: &Path, stems: &[String]) -> bool {
    let Ok(text) = std::fs::read_to_string(path) else {
        return false;
    };
    let word = |line: &str, stem: &str| {
        !stem.is_empty()
            && line.match_indices(stem).any(|(at, _)| {
                let before = line[..at].chars().next_back();
                let after = line[at + stem.len()..].chars().next();
                !before.is_some_and(|one| one.is_alphanumeric() || one == '_') && !after.is_some_and(|one| one.is_alphanumeric() || one == '_')
            })
    };
    text.lines().map(str::trim_start).any(|line| ["#include", "import ", "from ", "use ", "mod "].iter().any(|start| line.starts_with(start)) && stems.iter().any(|stem| word(line, stem)))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn clangd_serves_c_and_names_cuda_files_as_cuda() {
        let (tool, spec) = Servers::spec_for("c").unwrap();
        assert_eq!(tool.id, "llvm");
        assert_eq!(spec.program, "clangd");
        assert_eq!(spec.ids["cu"], "cuda-cpp");
        assert!(Servers::spec_for("plaintext").is_none());
    }

    #[test]
    fn a_hover_reads_as_markdown_in_each_shape() {
        assert_eq!(markdown_of(&json!({"kind": "markdown", "value": "**x**"})), "**x**");
        assert_eq!(markdown_of(&json!([{"language": "cpp", "value": "int x"}, "a note"])), "```cpp\nint x\n```\n\na note");
    }

    #[test]
    #[ignore = "starts clangd where it is installed"]
    fn clangd_reads_a_file_and_answers_about_it() {
        let dir = std::env::temp_dir().join(format!("orior-clangd-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let file = dir.join("a.c");
        let text = "static int twice(int x) { return 2 * x; }\nint main(void) { int y = twice(3); return y + missing; }\n";
        std::fs::write(&file, text).unwrap();
        let (sender, heard) = std::sync::mpsc::channel();
        let sender = Mutex::new(sender);
        let emit: Emit = Arc::new(move |told| {
            if let Told::Diagnostics(diagnostics) = told {
                let _ = sender.lock().unwrap().send(diagnostics);
            }
        });
        let servers = Servers::default();
        assert!(servers.open(&dir, &file, "c", text, &emit).unwrap());
        let diagnostics = heard.recv_timeout(Duration::from_secs(30)).unwrap();
        assert!(diagnostics.items.iter().any(|one| one.severity == 1 && one.message.contains("missing")), "{diagnostics:?}");
        assert!(servers.hover(&file, 1, 27).unwrap().unwrap().contains("twice"));
        let found = servers.definition(&file, 1, 27).unwrap();
        assert_eq!((found[0].line, found[0].col), (0, 11));
        let items = servers.complete(&file, 1, 27).unwrap();
        assert!(items.iter().any(|item| item.label == "twice"));
        let usages = servers.references(&file, 1, 27).unwrap();
        assert_eq!(usages.iter().map(|one| (one.from.line, one.from.col)).collect::<Vec<_>>(), vec![(0, 11), (1, 25)]);
        assert!(usages[0].text.starts_with("static int twice"));
        let renamed = servers.rename(&file, 1, 27, "double_it").unwrap();
        assert_eq!(apply(text, &renamed[0].edits), text.replace("twice", "double_it"));
        let signature = servers.signature(&file, 1, 31).unwrap().unwrap();
        assert_eq!(&signature.label[signature.params[0].0 as usize..signature.params[0].1 as usize], "int x");
        let actions = servers.actions(&file, Place { line: 1, col: 47 }, Place { line: 1, col: 54 }).unwrap();
        assert!(actions.iter().all(|action| !action.title.is_empty()));
        servers.stop_all();
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_workspace_edit_reads_in_either_shape_first_edit_first() {
        let uri = lsp::uri_of(Path::new(if cfg!(windows) { "D:\\t\\a.c" } else { "/t/a.c" }));
        let edit = |line: u32, text: &str| json!({"range": {"start": {"line": line, "character": 0}, "end": {"line": line, "character": 1}}, "newText": text});
        let changes = workspace_edit_of(&json!({"changes": {uri.clone(): [edit(3, "b"), edit(1, "a")]}}));
        assert_eq!(changes.len(), 1);
        assert_eq!(changes[0].edits.iter().map(|one| one.text.as_str()).collect::<Vec<_>>(), ["a", "b"]);
        let documents = workspace_edit_of(&json!({"documentChanges": [{"textDocument": {"uri": uri, "version": 2}, "edits": [edit(0, "c")]}, {"kind": "create", "uri": "file:///t/b.c"}]}));
        assert_eq!(documents.len(), 1);
        assert_eq!(documents[0].edits[0].text, "c");
    }

    #[test]
    fn edits_are_written_by_utf16_columns_and_keep_line_ends() {
        let at = |line, col| Place { line, col };
        let text = "a\u{1F600}b x\r\nsecond x\r\n";
        let edits = [TextEdit { from: at(0, 3), to: at(0, 4), text: "B".into() }, TextEdit { from: at(1, 7), to: at(1, 8), text: "yy".into() }, TextEdit { from: at(0, 5), to: at(0, 9), text: "!".into() }];
        assert_eq!(apply(text, &edits), "a\u{1F600}B !\r\nsecond yy\r\n");
    }

    #[test]
    fn a_signature_names_each_parameter_by_its_span() {
        let help = json!({"signatures": [{"label": "twice(int x, int y) -> int", "parameters": [{"label": "int x"}, {"label": [13, 18]}], "documentation": {"kind": "markdown", "value": "Doubles."}}], "activeSignature": 0, "activeParameter": 1});
        let signature = signature_of(&help).unwrap();
        assert_eq!(signature.params, vec![(6, 11), (13, 18)]);
        assert_eq!(signature.active, Some(1));
        assert_eq!(signature.doc, "Doubles.");
    }

    #[test]
    fn a_server_asking_for_settings_is_given_its_defaults_and_an_edit_is_passed_on() {
        let told = Arc::new(Mutex::new(Vec::new()));
        let kept = told.clone();
        let emit: Emit = Arc::new(move |one| {
            if let Told::Edits(files) = one {
                kept.lock().unwrap().push(files.len());
            }
        });
        let published = Mutex::new(HashMap::new());
        let checks = Checks::default();
        assert_eq!(heard("workspace/configuration", &json!({"items": [{}, {}]}), &emit, &published, &checks), json!([null, null]));
        assert_eq!(heard("workspace/applyEdit", &json!({"edit": {"changes": {"file:///t/a.c": []}}}), &emit, &published, &checks), json!({"applied": true}));
        assert_eq!(*told.lock().unwrap(), vec![1]);
    }

    #[test]
    fn a_file_the_check_lets_go_keeps_its_diagnostics_and_wakes_its_checker() {
        let told = Arc::new(Mutex::new(Vec::new()));
        let kept = told.clone();
        let emit: Emit = Arc::new(move |one| {
            if let Told::Diagnostics(diagnostics) = one {
                kept.lock().unwrap().push(diagnostics.items.len());
            }
        });
        let published = Mutex::new(HashMap::new());
        let checks = Checks::default();
        let uri = if cfg!(windows) { "file:///C:/t/a.c" } else { "file:///t/a.c" };
        let key = lsp::key_of(uri);
        let (send, woken) = mpsc::channel();
        checks.heard.lock().unwrap().woken.insert(key.clone(), send);
        let one = json!({"range": {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 1}}, "severity": 1, "message": "no"});
        heard("textDocument/publishDiagnostics", &json!({"uri": uri, "diagnostics": [one]}), &emit, &published, &checks);
        assert!(woken.try_recv().is_ok());
        checks.heard.lock().unwrap().letting_go.insert(key.clone());
        heard("textDocument/publishDiagnostics", &json!({"uri": uri, "diagnostics": [one]}), &emit, &published, &checks);
        let written = if cfg!(windows) { "file:///c%3A/t/a.c" } else { "file:///t/a.c" };
        heard("textDocument/publishDiagnostics", &json!({"uri": written, "diagnostics": []}), &emit, &published, &checks);
        assert_eq!(*told.lock().unwrap(), vec![1, 1], "a file's diagnostics given after it is let go pass on, and its clearing, however its URI is written, does not");
        assert_eq!(published.lock().unwrap()[&key].1.len(), 1);
        heard("textDocument/publishDiagnostics", &json!({"uri": uri, "diagnostics": []}), &emit, &published, &checks);
        assert_eq!(*told.lock().unwrap(), vec![1, 1, 0]);
    }

    #[test]
    fn a_file_names_another_in_its_includes_and_imports_only() {
        let dir = std::env::temp_dir().join(format!("orior-names-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let write = |name: &str, text: &str| {
            std::fs::write(dir.join(name), text).unwrap();
            dir.join(name)
        };
        assert!(names(&write("a.c", "#include \"shape.h\"\nint x;\n"), &["shape".into()]));
        assert!(!names(&write("b.c", "#include \"shapes.h\"\n/* shape */\n"), &["shape".into()]));
        assert!(names(&write("c.py", "import os\nfrom geometry.shape import area\n"), &["shape".into()]));
        assert!(!names(&write("d.py", "shape = 1\n"), &["shape".into()]));
        let _ = std::fs::remove_dir_all(&dir);
    }

    /// The errors in each file, by its name, and how far a check has gone.
    type ByName = Arc<Mutex<HashMap<String, usize>>>;
    type Far = Arc<Mutex<(usize, usize)>>;

    /// The errors a tree's check passes on, by file name, and how far it has gone.
    fn heard_by_name() -> (Emit, ByName, Far) {
        let found = Arc::new(Mutex::new(HashMap::new()));
        let gone = Arc::new(Mutex::new((0, 0)));
        let (kept, far) = (found.clone(), gone.clone());
        let emit: Emit = Arc::new(move |told| match told {
            Told::Diagnostics(diagnostics) => {
                let name = Path::new(&diagnostics.path).file_name().unwrap().to_string_lossy().to_string();
                kept.lock().unwrap().insert(name, diagnostics.items.iter().filter(|one| one.severity == 1).count());
            }
            Told::Checking { done, total } => *far.lock().unwrap() = (done, total),
            Told::Edits(_) | Told::Hints => {}
        });
        (emit, found, gone)
    }

    /// Waits until `ready` holds, for at most `seconds`.
    fn until(seconds: u64, ready: impl Fn() -> bool) -> bool {
        for _ in 0..seconds * 10 {
            if ready() {
                return true;
            }
            std::thread::sleep(Duration::from_millis(100));
        }
        ready()
    }

    #[test]
    #[ignore = "starts clangd where it is installed"]
    fn the_tree_check_hands_each_c_file_to_clangd_and_checks_again_what_includes_a_change() {
        let dir = std::env::temp_dir().join(format!("orior-check-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        std::fs::write(dir.join("shape.h"), "static int area(int side) { return side * side; }\n").unwrap();
        std::fs::write(dir.join("use.c"), "#include \"shape.h\"\nint main(void) { return area(2); }\n").unwrap();
        std::fs::write(dir.join("bad.c"), "int main(void) { return missing; }\n").unwrap();
        let (emit, found, gone) = heard_by_name();
        let servers = Servers::default();
        let files: Vec<String> = ["bad.c", "shape.h", "use.c"].iter().map(|one| one.to_string()).collect();
        assert_eq!(servers.check_tree(&dir, files, &emit), 3);
        assert!(until(60, || *gone.lock().unwrap() == (3, 3)), "{:?}", gone.lock().unwrap());
        assert_eq!(found.lock().unwrap().get("bad.c"), Some(&1), "{:?}", found.lock().unwrap());
        assert_eq!(found.lock().unwrap().get("use.c").copied().unwrap_or(0), 0);
        assert!(servers.running().iter().all(|one| one.files.is_empty()), "the check lets every file go");
        std::thread::sleep(Duration::from_secs(1));
        assert_eq!(found.lock().unwrap().get("bad.c"), Some(&1), "a file let go keeps its diagnostics");
        std::fs::write(dir.join("shape.h"), "static int perimeter(int side) { return 4 * side; }\n").unwrap();
        servers.changed(&dir, &["shape.h".to_string()]);
        assert!(until(60, || found.lock().unwrap().get("use.c").copied().unwrap_or(0) > 0), "use.c is checked again: {:?}", found.lock().unwrap());
        std::fs::remove_file(dir.join("bad.c")).unwrap();
        servers.changed(&dir, &[]);
        assert_eq!(found.lock().unwrap().get("bad.c"), Some(&0), "a file gone loses its diagnostics");
        servers.stop_all();
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    #[ignore = "starts pyright-langserver where it is on the PATH"]
    fn the_tree_check_asks_pyright_for_each_file() {
        let dir = std::env::temp_dir().join(format!("orior-check-py-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        std::fs::write(dir.join("shape.py"), "def area(side: int) -> int:\n    return side * side\n").unwrap();
        std::fs::write(dir.join("use.py"), "from shape import area\n\nprint(area(\"two\"))\n").unwrap();
        std::fs::write(dir.join("fine.py"), "print(1)\n").unwrap();
        let (emit, found, gone) = heard_by_name();
        let servers = Servers::default();
        let files: Vec<String> = ["fine.py", "shape.py", "use.py"].iter().map(|one| one.to_string()).collect();
        assert_eq!(servers.check_tree(&dir, files, &emit), 3);
        assert!(until(60, || *gone.lock().unwrap() == (3, 3)), "{:?}", gone.lock().unwrap());
        std::thread::sleep(Duration::from_secs(2));
        assert_eq!(found.lock().unwrap().get("use.py"), Some(&1), "{:?}", found.lock().unwrap());
        assert_eq!(found.lock().unwrap().get("fine.py").copied().unwrap_or(0), 0);
        assert!(servers.running().iter().all(|one| one.files.is_empty()), "the check lets every file go");
        servers.stop_all();
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    #[ignore = "starts rust-analyzer where it is installed"]
    fn the_tree_check_has_rust_analyzer_check_a_crate_whole() {
        let dir = std::env::temp_dir().join(format!("orior-check-crate-{}", std::process::id()));
        let krate = dir.join("tool");
        std::fs::create_dir_all(krate.join("src")).unwrap();
        std::fs::write(krate.join("Cargo.toml"), "[package]\nname = \"tool\"\nversion = \"0.1.0\"\nedition = \"2021\"\n").unwrap();
        std::fs::write(krate.join("src").join("main.rs"), "mod part;\n\nfn main() {\n    part::run();\n}\n").unwrap();
        std::fs::write(krate.join("src").join("part.rs"), "pub fn run() {\n    let x: u32 = \"text\";\n    println!(\"{x}\");\n}\n").unwrap();
        let (emit, found, _) = heard_by_name();
        let servers = Servers::default();
        servers.check_tree(&dir, vec!["tool/src/main.rs".into(), "tool/src/part.rs".into()], &emit);
        let checked = until(240, || {
            if found.lock().unwrap().get("part.rs").copied().unwrap_or(0) > 0 {
                return true;
            }
            // A server still reading its projects passes over a check asked for; it is asked again.
            if let Some(server) = servers.running.lock().unwrap().get("rust") {
                let _ = server.notify("rust-analyzer/runFlycheck", json!({"textDocument": null}));
            }
            std::thread::sleep(Duration::from_secs(2));
            false
        });
        assert!(checked, "{:?}", found.lock().unwrap());
        servers.stop_all();
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_file_with_no_server_hears_its_findings_and_their_fixes_and_so_does_the_tree() {
        let dir = std::env::temp_dir().join(format!("orior-inspect-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let text = "export function f() {\n  const unused = 1;\n  return 2;\n}\n";
        std::fs::write(dir.join("a.js"), text).unwrap();
        std::fs::write(dir.join("b.py"), "def g():\n    '''Return two.'''\n    return 2\n").unwrap();
        let told = Arc::new(Mutex::new(HashMap::new()));
        let kept = told.clone();
        let emit: Emit = Arc::new(move |one| {
            if let Told::Diagnostics(diagnostics) = one {
                let name = Path::new(&diagnostics.path).file_name().unwrap().to_string_lossy().to_string();
                kept.lock().unwrap().insert(name, diagnostics.items.iter().map(|item| item.message.clone()).collect::<Vec<_>>());
            }
        });
        let servers = Servers::default();
        let file = dir.join("a.js");
        assert!(!servers.open(&dir, &file, "javascript", text, &emit).unwrap());
        assert!(until(10, || told.lock().unwrap().contains_key("a.js") && told.lock().unwrap().contains_key("b.py")), "{:?}", told.lock().unwrap());
        assert_eq!(told.lock().unwrap()["a.js"], vec!["unused is declared and never read."]);
        assert_eq!(told.lock().unwrap()["b.py"].len(), 1, "a file of the tree no tab holds is inspected too");
        let actions = servers.actions(&file, Place { line: 1, col: 9 }, Place { line: 1, col: 9 }).unwrap();
        assert_eq!(actions[0].title, "Take out the declaration of unused");
        let edits = servers.act(&file, &actions[0].raw).unwrap();
        let fixed = apply(text, &edits[0].edits);
        assert_eq!(fixed, "export function f() {\n  return 2;\n}\n");
        servers.change(&file, &fixed).unwrap();
        assert!(until(10, || told.lock().unwrap()["a.js"].is_empty()), "{:?}", told.lock().unwrap());
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_report_of_an_attribute_names_it_and_its_class() {
        let item = json!({"message": "Cannot access attribute \"render\" for class \"Saving*\"
  Attribute \"render\" is unknown"});
        assert_eq!(attribute_of(&item), Some(("render".to_string(), "Saving".to_string())));
        assert_eq!(attribute_of(&json!({"message": "Import \"x\" could not be resolved"})), None);
    }

    #[test]
    fn rust_and_python_each_have_a_server() {
        assert_eq!(Servers::spec_for("rust").unwrap().1.program, "rust-analyzer");
        assert_eq!(Servers::spec_for("python").unwrap().1.program, "pyright-langserver");
    }

    #[test]
    #[ignore = "starts rust-analyzer where it is installed"]
    fn rust_analyzer_reads_a_crate_below_the_top_folder() {
        let dir = std::env::temp_dir().join(format!("orior-analyzer-{}", std::process::id()));
        let krate = dir.join("src").join("tool");
        std::fs::create_dir_all(krate.join("src")).unwrap();
        std::fs::write(krate.join("Cargo.toml"), "[package]\nname = \"tool\"\nversion = \"0.1.0\"\nedition = \"2021\"\n").unwrap();
        let file = krate.join("src").join("main.rs");
        let text = "fn twice(x: i32) -> i32 {\n    2 * x\n}\n\nfn main() {\n    let y = twice(3);\n    println!(\"{y}\");\n}\n";
        std::fs::write(&file, text).unwrap();
        let emit: Emit = Arc::new(|_| {});
        let servers = Servers::default();
        let opened = servers.open(&dir, &file, "rust", text, &emit);
        assert!(opened.clone().unwrap_or_else(|said| panic!("{said}")), "{opened:?}");
        let mut usages = Vec::new();
        for _ in 0..120 {
            usages = servers.references(&file, 5, 13).unwrap_or_default();
            if usages.len() == 2 {
                break;
            }
            std::thread::sleep(Duration::from_millis(500));
        }
        assert_eq!(usages.iter().map(|one| (one.from.line, one.from.col)).collect::<Vec<_>>(), vec![(0, 3), (5, 12)]);
        let renamed = servers.rename(&file, 5, 13, "double_it").unwrap();
        assert_eq!(apply(text, &renamed[0].edits), text.replace("twice", "double_it"));
        servers.stop_all();
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    #[ignore = "starts pyright-langserver where it is on the PATH"]
    fn pyright_reads_a_script_and_answers_about_it() {
        let dir = std::env::temp_dir().join(format!("orior-pyright-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let file = dir.join("a.py");
        let text = "def twice(x: int) -> int:\n    return 2 * x\n\n\ny = twice(3)\nprint(y + missing)\n";
        std::fs::write(&file, text).unwrap();
        let (sender, heard) = std::sync::mpsc::channel();
        let sender = Mutex::new(sender);
        let emit: Emit = Arc::new(move |told| {
            if let Told::Diagnostics(diagnostics) = told {
                let _ = sender.lock().unwrap().send(diagnostics);
            }
        });
        let servers = Servers::default();
        let opened = servers.open(&dir, &file, "python", text, &emit);
        assert!(opened.clone().unwrap_or_else(|said| panic!("{said}")), "{opened:?}");
        let diagnostics = heard.recv_timeout(Duration::from_secs(60)).unwrap();
        assert!(diagnostics.items.iter().any(|one| one.message.contains("missing")), "{diagnostics:?}");
        let usages = servers.references(&file, 4, 5).unwrap();
        assert_eq!(usages.iter().map(|one| (one.from.line, one.from.col)).collect::<Vec<_>>(), vec![(0, 4), (4, 4)]);
        let renamed = servers.rename(&file, 4, 5, "double_it").unwrap();
        assert_eq!(apply(text, &renamed[0].edits), text.replace("twice", "double_it"));
        let signature = servers.signature(&file, 4, 10).unwrap().unwrap();
        assert_eq!(&signature.label[signature.params[0].0 as usize..signature.params[0].1 as usize], "x: int");
        servers.stop_all();
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_completion_is_named_by_what_it_inserts() {
        let item = item_of(&json!({"label": " printf(const char *, ...)", "kind": 3, "detail": "int", "filterText": "printf", "insertText": "printf"}));
        assert_eq!(item.label, "printf");
        assert_eq!(item.kind, "function");
        assert_eq!(item.detail, "int printf(const char *, ...)");
    }
}

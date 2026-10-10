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

use std::collections::HashMap;
use std::path::{Path, PathBuf};
use std::sync::{Arc, Mutex};
use std::time::Duration;

use serde::{Deserialize, Serialize};
use serde_json::{json, Value};

use crate::lsp::{self, Server};
use crate::toolchains;

/// A toolchain's server: its program, which is one of the toolchain's or beside them, the words it
/// starts with, the languages it serves, the protocol's name for each extension's language, the
/// line that installs it where the toolchain can be present without it, and the name of the file
/// that marks a project, each of which in the tree the server is given as one of its linked
/// projects where it does not look below the top folder for them itself.
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

/// What a server says on its own that the editor is to hear.
pub enum Told {
    Diagnostics(Diagnostics),
    Edits(Vec<FileEdit>),
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

#[derive(Default)]
pub struct Servers {
    root: Mutex<Option<PathBuf>>,
    running: Mutex<HashMap<String, Arc<Server>>>,
    failed: Mutex<HashMap<String, String>>,
    /// Each file's diagnostics as its server last gave them, which a quick fix is asked about.
    published: Arc<Mutex<HashMap<String, Vec<Value>>>>,
}

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
fn heard(method: &str, params: &Value, emit: &Emit, published: &Mutex<HashMap<String, Vec<Value>>>) -> Value {
    match method {
        "textDocument/publishDiagnostics" => {
            if let (Some(uri), Some(all)) = (params["uri"].as_str(), params["diagnostics"].as_array()) {
                if let Ok(mut kept) = published.lock() {
                    kept.insert(uri.to_string(), all.clone());
                }
            }
            if let Some(diagnostics) = diagnostics_of(params) {
                emit(Told::Diagnostics(diagnostics));
            }
            Value::Null
        }
        "workspace/applyEdit" => {
            emit(Told::Edits(workspace_edit_of(&params["edit"])));
            json!({"applied": true})
        }
        "workspace/configuration" => Value::Array(vec![Value::Null; params["items"].as_array().map_or(0, Vec::len)]),
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
                let options = spec.projects.as_deref().map_or(Value::Null, |name| json!({"linkedProjects": projects_in(root, name)}));
                Server::start(&program, &spec.args, root, options, toolchains::run_path(), Box::new(move |method, params| heard(method, params, &emit, &published))).map_err(|said| match &spec.setup {
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

    /// Hands the file at `path` to its language's server. Says whether one took it.
    pub fn open(&self, root: &Path, path: &Path, language: &str, text: &str, emit: &Emit) -> Result<bool, String> {
        let Some((server, spec)) = self.server(root, language, emit)? else {
            return Ok(false);
        };
        let ext = path.extension().map(|ext| ext.to_string_lossy().to_lowercase()).unwrap_or_default();
        let id = spec.ids.get(&ext).cloned().unwrap_or_else(|| language.to_string());
        if server.has(path) {
            server.change(path, text)?;
            // A tab opened again on a file the server holds hears the diagnostics it last gave.
            let uri = lsp::uri_of(path);
            let kept = self.published.lock().ok().and_then(|all| all.get(&uri).cloned());
            if let Some(diagnostics) = kept.and_then(|all| diagnostics_of(&json!({"uri": uri, "diagnostics": all}))) {
                emit(Told::Diagnostics(diagnostics));
            }
        } else {
            server.open(path, &id, text)?;
        }
        Ok(true)
    }

    /// The server that has `path` open, where one does.
    fn holding(&self, path: &Path) -> Option<Arc<Server>> {
        self.running.lock().ok()?.values().find(|server| server.has(path)).cloned()
    }

    pub fn change(&self, path: &Path, text: &str) -> Result<(), String> {
        self.holding(path).map_or(Ok(()), |server| server.change(path, text))
    }

    pub fn close(&self, path: &Path) -> Result<(), String> {
        self.holding(path).map_or(Ok(()), |server| server.close(path))
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
        let server = self.asked(path)?;
        let uri = lsp::uri_of(path);
        let diagnostics: Vec<Value> = self.published.lock().map_err(|_| "held".to_string())?.get(&uri).map(|all| all.iter().filter(|one| overlaps(&from, &to, &one["range"])).cloned().collect()).unwrap_or_default();
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
        Ok(actions)
    }

    /// Carries out an action `actions` gave: its edits, filled in by the server first where it left
    /// them out, are returned to be written, and its command is run, whose edits the server asks
    /// for on its own.
    pub fn act(&self, path: &Path, raw: &Value) -> Result<Vec<FileEdit>, String> {
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
        for server in self.take_all() {
            std::thread::spawn(move || server.stop());
        }
    }
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
        assert_eq!(heard("workspace/configuration", &json!({"items": [{}, {}]}), &emit, &published), json!([null, null]));
        assert_eq!(heard("workspace/applyEdit", &json!({"edit": {"changes": {"file:///t/a.c": []}}}), &emit, &published), json!({"applied": true}));
        assert_eq!(*told.lock().unwrap(), vec![1]);
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

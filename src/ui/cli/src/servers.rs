// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The language servers the editor talks to, one for each toolchain whose `server` in
//! toolchains.json names a program, such as clangd for C, C++ and CUDA. A server starts with the
//! first file of its languages the editor opens, at the tree's top folder, and stops when another
//! tree is opened or orior ends. One that cannot be found or does not start is not tried again until
//! the tree changes, and the file opens without it.
//!
//! What the servers say comes back as orior's own shapes: a hover as Markdown, a definition as a
//! path and a place, a completion as the editor's items, and a file's diagnostics as spans with a
//! severity, 1 an error, 2 a warning, 3 a note and 4 a hint.

use std::collections::HashMap;
use std::path::{Path, PathBuf};
use std::sync::{Arc, Mutex};
use std::time::Duration;

use serde::{Deserialize, Serialize};
use serde_json::{json, Value};

use crate::lsp::{self, Server};
use crate::toolchains;

/// A toolchain's server: its program, which is one of the toolchain's or beside them, the words it
/// starts with, the languages it serves, and the protocol's name for each extension's language.
#[derive(Deserialize, Serialize, Clone, Debug)]
pub struct ServerSpec {
    pub program: String,
    pub args: Vec<String>,
    pub languages: Vec<String>,
    #[serde(default)]
    pub ids: HashMap<String, String>,
}

#[derive(Serialize, Clone, Debug, PartialEq)]
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

/// Where the diagnostics go.
pub type Emit = Arc<dyn Fn(Diagnostics) + Send + Sync>;

const ASKING: Duration = Duration::from_secs(5);

#[derive(Default)]
pub struct Servers {
    root: Mutex<Option<PathBuf>>,
    running: Mutex<HashMap<String, Arc<Server>>>,
    failed: Mutex<HashMap<String, String>>,
}

fn place(value: &Value) -> Place {
    Place { line: value["line"].as_u64().unwrap_or(0) as u32, col: value["character"].as_u64().unwrap_or(0) as u32 }
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
                Server::start(
                    &program,
                    &spec.args,
                    root,
                    toolchains::run_path(),
                    Box::new(move |method, params| {
                        if method == "textDocument/publishDiagnostics" {
                            if let Some(diagnostics) = diagnostics_of(params) {
                                emit(diagnostics);
                            }
                        }
                    }),
                )
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

    /// Stops every server, and forgets those that failed, as another tree opens.
    pub fn stop_all(&self) {
        let running: Vec<Arc<Server>> = self.running.lock().map(|mut all| all.drain().map(|(_, server)| server).collect()).unwrap_or_default();
        for server in running {
            server.stop();
        }
        if let Ok(mut failed) = self.failed.lock() {
            failed.clear();
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
        assert!(Servers::spec_for("python").is_none());
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
        let emit: Emit = Arc::new(move |diagnostics| {
            let _ = sender.lock().unwrap().send(diagnostics);
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
